
#ifndef PSC_FIELD_ARRAY_BASE_H
#define PSC_FIELD_ARRAY_BASE_H

#include "psc_vpic_bits.h"
#include "PscFieldBase.h"
#include "Field3D.h"

#include <mrc_common.h>
#include <cassert>
#include <cmath>

struct PscFieldT
{
  float ex,   ey,   ez,   div_e_err;     // Electric field and div E error
  float cbx,  cby,  cbz,  div_b_err;     // Magnetic field and div B error
  float tcax, tcay, tcaz, rhob;          // TCA fields and bound charge density
  float jfx,  jfy,  jfz,  rhof;          // Free current and charge density
  MaterialId ematx, ematy, ematz, nmat; // Material at edge centers and nodes
  MaterialId fmatx, fmaty, fmatz, cmat; // Material at face and cell centers
};

// ======================================================================
// PscFieldArrayBase

template<class G, class ML>
struct PscFieldArrayBase : PscFieldBase<PscFieldT, G>
{
  typedef PscFieldBase<PscFieldT, G> Base;
  typedef ML MaterialList;
  using typename Base::Grid;
  using typename Base::Element;
  
  enum {
    EX  = 0,
    EY  = 1,
    EZ  = 2,
    CBX = 4,
    CBY = 5,
    CBZ = 6,
    N_COMP = sizeof(typename Base::Element) / sizeof(float),
  };
  
  struct SfaParams
  {
    struct MaterialCoefficient
    {
      float decayx, drivex;         // Decay of ex and drive of (curl H)x and Jx
      float decayy, drivey;         // Decay of ey and drive of (curl H)y and Jy
      float decayz, drivez;         // Decay of ez and drive of (curl H)z and Jz
      float rmux, rmuy, rmuz;       // Reciprocle of relative permeability
      float nonconductive;          // Divergence cleaning related coefficients
      float epsx, epsy, epsz; 
      float pad[3];                 // For 64-byte alignment and future expansion
    };

    MaterialCoefficient* mc;
    int n_mc;
    float damp;
  };

  typedef typename SfaParams::MaterialCoefficient MaterialCoefficient;

  static PscFieldArrayBase* create(Grid *grid, const MaterialList& material_list, float damp)
  {
    return new PscFieldArrayBase(grid, material_list, damp);
  }

 private:
  PscFieldArrayBase(Grid* grid, const MaterialList& material_list, float damp)
    : Base(grid)
  {
    assert(!material_list.empty());
    assert(damp >= 0.);
    params = create_sfa_params(grid, material_list, damp);
  }
  
  ~PscFieldArrayBase()
  {
    destroy_sfa_params(params);
  }

public:
  static float minf(float a, float b)
  {
    return a < b ? a : b;
  }

  static SfaParams *create_sfa_params(const Grid* g,
				      const MaterialList& m_list,
				      float damp)
  {
    SfaParams* p;
    float ax, ay, az, cg2;
    MaterialCoefficient* mc;
    int n_mc;

    // Run sanity checks on the material list

    ax = g->nx > 1 ? g->cvac*g->dt*g->rdx : 0; ax *= ax;
    ay = g->ny > 1 ? g->cvac*g->dt*g->rdy : 0; ay *= ay;
    az = g->nz > 1 ? g->cvac*g->dt*g->rdz : 0; az *= az;
    n_mc = 0;
    for (auto m = m_list.cbegin(); m != m_list.cend(); ++m) {
      if( m->sigmax/m->epsx<0 )
	LOG_WARN("\"%s\" is an active medium along x", m->name);
      if( m->epsy*m->muz<0 )
	LOG_WARN("\"%s\" has an imaginary x speed of light (ey)", m->name);
      if( m->epsz*m->muy<0 )
	LOG_WARN("\"%s\" has an imaginary x speed of light (ez)", m->name);
      if( m->sigmay/m->epsy<0 )
	LOG_WARN("\"%s\" is an active medium along y", m->name);
      if( m->epsz*m->mux<0 )
	LOG_WARN("\"%s\" has an imaginary y speed of light (ez)", m->name);
      if( m->epsx*m->muz<0 )
	LOG_WARN("\"%s\" has an imaginary y speed of light (ex)", m->name);
      if( m->sigmaz/m->epsz<0 )
	LOG_WARN("\"%s\" is an an active medium along z", m->name);
      if( m->epsx*m->muy<0 )
	LOG_WARN("\"%s\" has an imaginary z speed of light (ex)", m->name);
      if( m->epsy*m->mux<0 )
	LOG_WARN("\"%s\" has an imaginary z speed of light (ey)", m->name);
      cg2 = ax/minf(m->epsy*m->muz,m->epsz*m->muy) +
	ay/minf(m->epsz*m->mux,m->epsx*m->muz) +
	az/minf(m->epsx*m->muy,m->epsy*m->mux);
      if( cg2>=1 )
	LOG_WARN("\"%s\" Courant condition estimate = %e", m->name, sqrt(cg2));
      if( m->zetax!=0 || m->zetay!=0 || m->zetaz!=0 )
	LOG_WARN("\"%s\" magnetic conductivity is not supported", m->name);
      n_mc++;
    }

    // Allocate the sfa parameters

    p = new SfaParams;
    p->mc = new MaterialCoefficient[n_mc+2];
    p->n_mc = n_mc;
    p->damp = damp;

    // Fill up the material coefficient array
    // FIXME: THIS IMPLICITLY ASSUMES MATERIALS ARE NUMBERED CONSECUTIVELY FROM
    // O.

    for (auto m = m_list.cbegin(); m != m_list.cend(); ++m) {
      mc = p->mc + m->id;

      // Advance E coefficients
      // Note: m ->sigma{x,y,z} = 0 -> Non conductive
      //       mc->decay{x,y,z} = 0 -> Perfect conductor to numerical precision
      //       otherwise            -> Conductive
      ax = ( m->sigmax*g->dt ) / ( m->epsx*g->eps0 );
      ay = ( m->sigmay*g->dt ) / ( m->epsy*g->eps0 );
      az = ( m->sigmaz*g->dt ) / ( m->epsz*g->eps0 );
      mc->decayx = exp(-ax);
      mc->decayy = exp(-ay);
      mc->decayz = exp(-az);
      if( ax==0 )              mc->drivex = 1./m->epsx;
      else if( mc->decayx==0 ) mc->drivex = 0;
      else mc->drivex = 2.*exp(-0.5*ax)*sinh(0.5*ax) / (ax*m->epsx);
      if( ay==0 )              mc->drivey = 1./m->epsy;
      else if( mc->decayy==0 ) mc->drivey = 0;
      else mc->drivey = 2.*exp(-0.5*ay)*sinh(0.5*ay) / (ay*m->epsy);
      if( az==0 )              mc->drivez = 1./m->epsz;
      else if( mc->decayz==0 ) mc->drivez = 0;
      else mc->drivez = 2.*exp(-0.5*az)*sinh(0.5*az) / (az*m->epsz);
      mc->rmux = 1./m->mux;
      mc->rmuy = 1./m->muy;
      mc->rmuz = 1./m->muz;

      // Clean div E coefficients.  Note: The charge density due to J =
      // sigma E currents is not computed.  Consequently, the divergence
      // error inside conductors cannot computed.  The divergence error
      // multiplier is thus set to zero to ignore divergence errors
      // inside conducting materials.

      mc->nonconductive = ( ax==0 && ay==0 && az==0 ) ? 1. : 0.;
      mc->epsx = m->epsx;
      mc->epsy = m->epsy;
      mc->epsz = m->epsz;
    }

    return p;
  }
  
  void destroy_sfa_params(SfaParams* p)
  {
    delete[] p->mc;
    delete p;
  }
  
  float* getData(int* ib, int* im)
  {
    const int B = 1; // VPIC always uses one ghost cell (on c.c. grid)
    im[0] = g_->nx + 2*B;
    im[1] = g_->ny + 2*B;
    im[2] = g_->nz + 2*B;
    ib[0] = -B;
    ib[1] = -B;
    ib[2] = -B;
    return &arr_[0].ex;
  }

  // These operators can be used to access the field directly,
  // though the performance isn't great, so one you use Field3D
  // when performance is important
  float operator()(int m, int i, int j, int k) const
  {
    float *ff = &arr_[0].ex;
    return ff[VOXEL(i,j,k, g_->nx,g_->ny,g_->nz) * N_COMP + m];
  }
  
  float& operator()(int m, int i, int j, int k)
  {
    float *ff = &arr_[0].ex;
    return ff[VOXEL(i,j,k, g_->nx,g_->ny,g_->nz) * N_COMP + m];
  }

  using Base::grid;

private:
  using Base::arr_;

protected:
  using Base::g_;
  SfaParams* params;
};



#endif

