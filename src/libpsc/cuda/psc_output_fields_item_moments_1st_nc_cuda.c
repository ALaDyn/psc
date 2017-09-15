
#include "psc.h"

#include "psc_output_fields_item_private.h"
#include "psc_cuda.h"

#include <math.h>

// ======================================================================
// rho_1st_nc

static void
rho_run_all(struct psc_output_fields_item *item, struct psc_mfields *mflds_base,
	    struct psc_mparticles *mprts_base, struct psc_mfields *mres_base)
{
  struct psc_mparticles *mprts = psc_mparticles_get_as(mprts_base, "cuda", 0);
  struct psc_mfields *mres = psc_mfields_get_as(mres_base, "cuda", 0, 0);
  psc_mfields_zero_range(mres, 0, mres->nr_fields);

  yz_moments_rho_1st_nc_cuda_run_patches(mprts, mres);

  psc_mparticles_put_as(mprts, mprts_base, MP_DONT_COPY);
  psc_mfields_put_as(mres, mres_base, 0, 1);
}

// ----------------------------------------------------------------------
// psc_output_fields_item: subclass "rho_1st_nc_cuda"

struct psc_output_fields_item_ops psc_output_fields_item_rho_1st_nc_cuda_ops = {
  .name               = "rho_1st_nc_cuda",
  .nr_comp            = 1,
  .fld_names          = { "rho_nc_cuda" }, // FIXME
  .run_all            = rho_run_all,
  .flags              = POFI_ADD_GHOSTS,
};

// ======================================================================
// n_1st

// ----------------------------------------------------------------------
// n_1st_run_all

static void
n_1st_run_all(struct psc_output_fields_item *item, struct psc_mfields *mflds_base,
	      struct psc_mparticles *mprts_base, struct psc_mfields *mres_base)
{
  struct psc_mparticles *mprts = psc_mparticles_get_as(mprts_base, "cuda", 0);
  struct psc_mfields *mres = psc_mfields_get_as(mres_base, "cuda", 0, 0);
  psc_mfields_zero_range(mres, 0, mres->nr_fields);

  yz_moments_n_1st_cuda_run_patches(mprts, mres);

  psc_mparticles_put_as(mprts, mprts_base, MP_DONT_COPY);
  psc_mfields_put_as(mres, mres_base, 0, mres->nr_fields);
}

// ----------------------------------------------------------------------
// psc_output_fields_item: subclass "n_1st_cuda"

struct psc_output_fields_item_ops psc_output_fields_item_n_1st_cuda_ops = {
  .name               = "n_1st_cuda",
  .nr_comp            = 1,
  .fld_names          = { "n" },
  .run_all            = n_1st_run_all,
  .flags              = POFI_ADD_GHOSTS | POFI_BY_KIND,
};

