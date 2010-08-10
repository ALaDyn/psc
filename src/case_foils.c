
#include "psc.h"
#include "util/params.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// ======================================================================
// Foils : different density distribution examples:
// 
// 1. Line
// 2. Circle
// 3. Part of circle
//-----------------------------------------------------
//  
// 1. Line
// needs the coordinates of its beginning and end, thickness 
//

struct foils {
  
  double Line0_x0, Line0_z0;   // coordinates of the beginning of the line0
  double Line0_x1, Line0_z1;   // coordinates of the end of the line0
  double Line0_Thickness;       // thickness of the line
  double Line0_Preplasma;    
 
  double Line1_x0, Line1_z0;
  double Line1_x1, Line1_z1;
  double Line1_Thickness;
  double Line1_Preplasma;

  double Te, Ti;
  double x0, y0, z0; // location of density center in m of the first(0) foil
  double L0; // gradient of density profile in m of the first foil
  double width0; // width of transverse / longitudinal 
                                 // density profile in m of the first foil 
  double mass_ratio; // M_i / M_e
  double charge_state;   // Charge state of the ion
  double R_curv0;  // curvature of the first foil  in meters
};

#define VAR(x) (void *)offsetof(struct foils, x)

static struct param foils_descr[] = {
  { "Line0_x0"      , VAR(Line0_x0)        , PARAM_DOUBLE(1. * 1e-6)           },
  { "Line0_x1"      , VAR(Line0_x1)        , PARAM_DOUBLE(2. * 1e-6)             },
  { "Line0_z0"      , VAR(Line0_z0)        , PARAM_DOUBLE(2.0 * 1e-6)            },
  { "Line0_z1"      , VAR(Line0_z1)        , PARAM_DOUBLE(4.0 * 1e-6)           },
  { "Line0_Thickness", VAR(Line0_Thickness)        , PARAM_DOUBLE(0.2 * 1e-6)          },
  { "Line0_Preplasma", VAR(Line0_Preplasma) , PARAM_DOUBLE(1. * 1e-9)     },
  { "Line1_x0"      , VAR(Line1_x0)        , PARAM_DOUBLE(1. * 1e-6)           },
  { "Line1_x1"      , VAR(Line1_x1)        , PARAM_DOUBLE(4. * 1e-6)             },
  { "Line1_z0"      , VAR(Line1_z0)        , PARAM_DOUBLE(2.0 * 1e-6)            },
  { "Line1_z1"      , VAR(Line1_z1)        , PARAM_DOUBLE(2.0 * 1e-6)           },
  { "Line1_Thickness", VAR(Line1_Thickness)        , PARAM_DOUBLE(0.2 * 1e-6)          },
  { "Line1_Preplasma", VAR(Line1_Preplasma) , PARAM_DOUBLE(1. * 1e-9)     },
  { "Te"            , VAR(Te)              , PARAM_DOUBLE(0.)             },
  { "Ti"            , VAR(Ti)              , PARAM_DOUBLE(0.)             },
  { "x0"            , VAR(x0)              , PARAM_DOUBLE(2.5 * 1e-6)     },
  { "y0"            , VAR(y0)              , PARAM_DOUBLE(.01 * 1e-6)     },
  { "z0"            , VAR(z0)              , PARAM_DOUBLE(2.5  * 1e-6)     },
  { "L0"            , VAR(L0)              , PARAM_DOUBLE(10.  * 1e-9)     },
  { "width0"        , VAR(width0)          , PARAM_DOUBLE(200. * 1e-9)     },
  { "mass_ratio"    , VAR(mass_ratio)      , PARAM_DOUBLE(12.*1836.)      },
  { "charge_state"  , VAR(charge_state)    , PARAM_DOUBLE(6.)             },
  { "R_curv0"     , VAR(R_curv0)      ,       PARAM_DOUBLE(2.5 * 1e-6)             },
  {},
};

#undef VAR

static real Line_dens(double x0, double z0, double x1, double z1, double xc, double zc, double Thickness, double Preplasma)
{
    // returns the density in the current cell for the line density distribution
    // x0,z0 - coordinates of the beginning of the line
    // x1,z1 - coordinates of the end of the line
    // xc, zc - current coordinates of the grid
    // Thickness - thickness of the line
    // Preplasma - preplasma of the line

    real Length = sqrt(sqr(x0-x1)+sqr(z0-z1));
    real cosrot = 1.0;
    real sinrot = 0.0;

    if(Length!=0.0)
    { 
      cosrot = -(z0 - z1) / Length;
      sinrot = -(x0 - x1) / Length;  
    }

  real xmiddle = (x0+x1)*0.5;
  real zmiddle = (z0+z1)*0.5;
  
  //real yr = x[1];
  //real xr = Line0_sinrot * (x[0]-Line0_xmiddle) + Line0_cosrot * (x[2]-Line0_zmiddle) + Line0_xmiddle;
  //real zr = Line0_sinrot * (x[2]-Line0_zmiddle) - Line0_cosrot * (x[0]-Line0_xmiddle) + Line0_zmiddle;

   real xr = sinrot * (xc - xmiddle) + cosrot * (zc - zmiddle) + xmiddle;
   real zr = sinrot * (zc - zmiddle) - cosrot * (xc - xmiddle) + zmiddle;

   real argx = (fabs(xr-xmiddle)-0.5*Length)/Preplasma;
//  real argy = (fabs(yr-Line0_y0))/1e-9;
   real argz = (fabs(zr-zmiddle)-0.5*Thickness)/Preplasma;
  if (argx > 200.0) argx = 200.0;
//  if (argy > 200.0) argy = 200.0;
  if (argz > 200.0) argz = 200.0;

  return 1. / ((1. + exp(argx)) * (1. + exp(argz)));
}

static void
foils_create(struct psc_case *Case)
{
  struct psc_pulse_gauss prm_p = {
    .xm = 2.5   * 1e-6,
    .ym = 2.5   * 1e-6,
    .zm = -0. * 1e-6,
    .dxm = 0.5   * 1e-6,
    .dym = 2.   * 1e-6,
    .dzm = 4.   * 1e-6,
//    .zb  = 10. * 1e-6,
    .phase = 0.0,
  };
  
  struct psc_pulse_gauss prm_s = {
    .xm = 2.5   * 1e-6,
    .ym = 2.5   * 1e-6,
    .zm = -0. * 1e-6,
    .dxm = 0.5   * 1e-6,
    .dym = 2.   * 1e-6,
    .dzm = 4.   * 1e-6,
//    .zb  = 10. * 1e-6,
    .phase = M_PI / 2.,
  };

//  psc.pulse_p_z1 = psc_pulse_flattop_create(&prm_p);
//  psc.pulse_s_z1 = psc_pulse_flattop_create(&prm_s);

  psc.pulse_p_z1 = psc_pulse_gauss_create(&prm_p);
  psc.pulse_s_z1 = psc_pulse_gauss_create(&prm_s);
}



static void
foils_init_param(struct psc_case *Case)
{
  psc.prm.nmax = 500;
  psc.prm.cpum = 25000;
  psc.prm.lw = 1. * 1e-6;
  psc.prm.i0 = 1.0e10;
  psc.prm.n0 = 2.0e29;

  psc.prm.nicell = 50;

  psc.domain.length[0] = 5.0 * 1e-6;			// length of the domain in x-direction (transverse)
  psc.domain.length[1] = 0.02 * 1e-6;
  psc.domain.length[2] = 5.0  * 1e-6;			// length of the domain in z-direction (longitudinal)

  psc.domain.itot[0] = 500;				// total number of steps in x-direction. dx=length/itot;
  psc.domain.itot[1] = 10;				
  psc.domain.itot[2] = 500;				// total number of steps in z-direction. dz=length/itot;
  psc.domain.ilo[0] = 0;
  psc.domain.ilo[1] = 9;
  psc.domain.ilo[2] = 0;
  psc.domain.ihi[0] = 500;
  psc.domain.ihi[1] = 10;
  psc.domain.ihi[2] = 500;

  psc.domain.bnd_fld_lo[0] = 1;
  psc.domain.bnd_fld_hi[0] = 1;
  psc.domain.bnd_fld_lo[1] = 1;
  psc.domain.bnd_fld_hi[1] = 1;
  psc.domain.bnd_fld_lo[2] = 3; // time
  psc.domain.bnd_fld_hi[2] = 2; // upml
  psc.domain.bnd_part[0] = 0;
  psc.domain.bnd_part[1] = 0;
  psc.domain.bnd_part[2] = 0;
}

static void
foils_init_field(struct psc_case *Case)
{
  // FIXME, do we need the ghost points?
  for (int jz = psc.ilg[2]; jz < psc.ihg[2]; jz++) {
    for (int jy = psc.ilg[1]; jy < psc.ihg[1]; jy++) {
      for (int jx = psc.ilg[0]; jx < psc.ihg[0]; jx++) {
	double dx = psc.dx[0], dy = psc.dx[1], dz = psc.dx[2], dt = psc.dt;
	double xx = jx * dx, yy = jy * dy, zz = jz * dz;

	// FIXME, why this time?
	FF3(EY, jx,jy,jz) = psc_p_pulse_z1(xx, yy + .5*dy, zz, 0.*dt);
	FF3(BX, jx,jy,jz) = -psc_p_pulse_z1(xx, yy + .5*dy, zz + .5*dz, 0.*dt);
	FF3(EX, jx,jy,jz) = psc_s_pulse_z1(xx + .5*dx, yy, zz, 0.*dt);
	FF3(BY, jx,jy,jz) = psc_s_pulse_z1(xx + .5*dx, yy, zz + .5*dz, 0.*dt);
      }
    }
  }
}

static void
foils_init_npt(struct psc_case *Case, int kind, double x[3], 
		  struct psc_particle_npt *npt)
{
  struct foils *foils = Case->ctx;

  real Te = foils->Te, Ti = foils->Ti;

  real ld = psc.coeff.ld;   
 
  real Line0_x0 = foils->Line0_x0 / ld;  
  real Line0_x1 = foils->Line0_x1 / ld;   
  real Line0_z0 = foils->Line0_z0 / ld;
  real Line0_z1 = foils->Line0_z1 / ld;
  real Line0_Thickness = foils->Line0_Thickness / ld;
  real Line0_Preplasma = foils->Line0_Preplasma / ld;

  real Line1_x0 = foils->Line1_x0 / ld;  
  real Line1_x1 = foils->Line1_x1 / ld;   
  real Line1_z0 = foils->Line1_z0 / ld;
  real Line1_z1 = foils->Line1_z1 / ld;
  real Line1_Thickness = foils->Line1_Thickness / ld;
  real Line1_Preplasma = foils->Line1_Preplasma / ld;


  real dens = Line_dens(Line0_x0, Line0_z0, Line0_x1, Line0_z1, x[0], x[2], Line0_Thickness, Line0_Preplasma);
  dens += Line_dens(Line1_x0, Line1_z0, Line1_x1, Line1_z1, x[0], x[2], Line1_Thickness, Line1_Preplasma);

  if (dens>1.0) dens=1.0;

  //real xr = x[0];
  //real yr = x[1];
  //real zr = x[2];


/*  real x0 = foils->x0 / ld;
  real y0 = foils->y0 / ld;
  real z0 = foils->z0 / ld;
  real L0 = foils->L0 / ld;
  real width0 = foils->width0 / ld;
  real R_curv0 = foils->R_curv0 / ld;


  real xsphere0 = x0;                    // coordinates 
  real ysphere0 = y0;                    // of the 
  real zsphere0 = z0;                    // sphere0 center

  real Radius0AtCurrentCell = sqrt((xr-xsphere0)*(xr-xsphere0)+(yr-ysphere0)*(yr-ysphere0)+(zr-zsphere0)*(zr-zsphere0));
  real argsphere0 = (fabs(Radius0AtCurrentCell-R_curv0)-width0)/L0;

  if (argsphere0 > 200.0) argsphere0 = 200.0;

  real dens = 1./(1.+exp(argsphere0));*/

  switch (kind) {
  case 0: // electrons
    npt->q = -1.;
    npt->m = 1.;
    npt->n = dens;
    npt->T[0] = Te;
    npt->T[1] = Te;
    npt->T[2] = Te;
    break;
  case 1: // ions
    npt->q = foils->charge_state;
    npt->m = foils->mass_ratio;
    npt->n = dens/foils->charge_state;
    npt->T[0] = Ti;
    npt->T[1] = Ti;
    npt->T[2] = Ti;
    break;
  default:
    assert(0);
  }
}

struct psc_case_ops psc_case_ops_foils = {
  .name       = "foils",
  .ctx_size   = sizeof(struct foils),
  .ctx_descr  = foils_descr,
  .create     = foils_create,
  .init_param = foils_init_param,
  .init_field = foils_init_field,
  .init_npt   = foils_init_npt,
};
