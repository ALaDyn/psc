
#include <mrc_domain.h>
#include <mrc_bits.h>

#include <stdlib.h>
#include <math.h>

// ======================================================================
// PDE/mesh parameters that we keep around statically

static int s_n_ghosts; // number of ghost points
static int s_n_comps;  // number of components in state vector
static int s_n_dims;   // number of (not invariant) dimensions

// mesh info
static int s_size_1d;  // largest local dimension (including ghosts)
static int s_ldims[3]; // local dimensions (interior only) 
static int s_sw[3];    // number of ghost points per dim
static int s_dijk[3];  // set to 1 if actual direction, 0 if invariant

// need to have the static parameters above before we can include pde_fld1d.c

#include "pde_fld1d.c"

// ----------------------------------------------------------------------
// pde_setup

static void
pde_setup(struct mrc_fld *fld)
{
  s_n_ghosts = fld->_nr_ghosts;
  s_n_comps = mrc_fld_nr_comps(fld);

  int gdims[3];
  mrc_domain_get_global_dims(fld->_domain, gdims);
  int n_dims = 3;
  if (gdims[2] == 1) {
    n_dims--;
    if (gdims[1] == 1) {
      n_dims--;
    }
  }
  s_n_dims = n_dims;

  s_size_1d = 0;
  for (int d = 0; d < 3; d++) {
    s_ldims[d] = mrc_fld_spatial_dims(fld)[d];
    s_sw[d] = mrc_fld_spatial_sw(fld)[d];
    s_size_1d = MAX(s_size_1d, s_ldims[d] + 2 * s_sw[d]);
    s_dijk[d] = (gdims[d] > 1);
  }
}

