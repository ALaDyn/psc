
#include "psc_output_fields_item_private.h"
#include "psc_particles_cuda.h"
#include "psc_fields_cuda.h"
#include "cuda_iface.h"

#include "fields_item.hxx"

// ======================================================================
// Moment_rho_1st_nc_cuda

struct Moment_rho_1st_nc_cuda : ItemMomentCRTP<Moment_rho_1st_nc_cuda, MfieldsCuda>
{
  using Base = ItemMomentCRTP<Moment_rho_1st_nc_cuda, MfieldsCuda>;
  using Mfields = MfieldsCuda;
  using Mparticles = MparticlesCuda;
  
  constexpr static const char* name = "rho_1st_nc";
  constexpr static int n_comps = 1;
  constexpr static fld_names_t fld_names() { return { "rho_nc_cuda" }; } // FIXME
  constexpr static int flags = 0;

  Moment_rho_1st_nc_cuda(MPI_Comm comm)
    : Base(comm)
  {}

  void run(MparticlesCuda& mprts)
  {
    PscMfields<Mfields> mres{this->mres_};
    cuda_mparticles *cmprts = mprts.cmprts();
    cuda_mfields *cmres = mres->cmflds;
    
    mres->zero();
    cuda_moments_yz_rho_1st_nc(cmprts, cmres);
    assert(0); // FIXME
    //bnd_.add_ghosts(mres.mflds(), 0, mres->n_comps());
  }
};

FieldsItemOps<FieldsItemMoment<Moment_rho_1st_nc_cuda>> psc_output_fields_item_rho_1st_nc_cuda_ops;

// ======================================================================
// n_1st_cuda

struct Moment_n_1st_cuda : ItemMomentCRTP<Moment_n_1st_cuda, MfieldsCuda>
{
  using Base = ItemMomentCRTP<Moment_n_1st_cuda, MfieldsCuda>;
  using Mfields = MfieldsCuda;
  using Mparticles = MparticlesCuda;
  
  constexpr static const char* name = "n_1st";
  constexpr static int n_comps = 1;
  constexpr static fld_names_t fld_names() { return { "n_1st_cuda" }; }
  constexpr static int flags = 0;

  Moment_n_1st_cuda(MPI_Comm comm)
    : Base(comm)
  {}

  void run(MparticlesCuda& mprts)
  {
    PscMfields<Mfields> mres{this->mres_};
    cuda_mparticles *cmprts = mprts.cmprts();
    cuda_mfields *cmres = mres->cmflds;
    
    mres->zero();
    cuda_moments_yz_n_1st(cmprts, cmres);
    assert(0);
    //bnd_.add_ghosts(mres.mflds(), 0, mres->n_comps());
  }
};

FieldsItemOps<FieldsItemMoment<Moment_n_1st_cuda>> psc_output_fields_item_n_1st_cuda_ops;

