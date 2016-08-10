
#include <mrc_domain.h>
#include <mrc_bits.h>

#include <stdlib.h>
#include <math.h>

// ======================================================================
// PDE/mesh parameters that we keep around statically

static int s_n_ghosts; // number of ghost points
static int s_n_comps;  // number of components in state vector
static int s_n_dims;   // number of (not invariant) dimensions

// mesh-related info
static int s_size_1d;  // largest local dimension (including ghosts)
static int s_ldims[3]; // local dimensions (interior only) 
static int s_sw[3];    // number of ghost points per dim
static int s_dijk[3];  // set to 1 if actual direction, 0 if invariant

// this is violating any kind of name space pretty badly, but otherwise
// it's just way too ugly to actually use these
#define di (s_dijk[0])
#define dj (s_dijk[1])
#define dk (s_dijk[2])

static double s_g_dxmin[3];
static double s_g_dxyzmin; // global minimum grid spacing

// need to have the static parameters above before we can include pde_fld1d.c

#include "pde_fld1d.c"

#include "pde_fld3d.c"

// ======================================================================
// static info for current patch

// ----------------------------------------------------------------------
// s_patches
//
// mesh spacing, statically saved for each patch

struct pde_patch {
  fld1d_t dx[3]; // cell sizes in each direction (ie. spacing between faces)
  fld1d_t inv_dx[3]; // 1 / s_dx[]
  fld1d_t inv_dxf[3]; // 1 / dxf[], where dxf = spacing between cell centers (located at face)
};

static struct pde_patch *s_patches;

// ----------------------------------------------------------------------
// s_patch
//
// current patch mesh info, taken from s_patches by calling
// pde_patch_set()

static struct pde_patch s_patch;

// macros to access these quantities in less ugly way
#define PDE_INV_DX(i) F1(s_patch.inv_dx[0], i)
#define PDE_INV_DY(j) F1(s_patch.inv_dx[1], j)
#define PDE_INV_DZ(k) F1(s_patch.inv_dx[2], k)

#define PDE_INV_DXF(i) F1(s_patch.inv_dxf[0], i)
#define PDE_INV_DYF(j) F1(s_patch.inv_dxf[1], j)
#define PDE_INV_DZF(k) F1(s_patch.inv_dxf[2], k)

// ----------------------------------------------------------------------
// pde_patch_set

static void _mrc_unused
pde_patch_set(int p)
{
  s_patch = s_patches[p];
}

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

  struct mrc_crds *crds = mrc_domain_get_crds(fld->_domain);

  int n_patches = mrc_fld_nr_patches(fld);
  s_patches = calloc(n_patches, sizeof(*s_patches));

  double dxmin[3] = { 1e10, 1e10, 1e10 };
  for (int p = 0; p < n_patches; p++) {
    struct pde_patch *patch = &s_patches[p];
    for (int d = 0; d < 3; d++) {
      fld1d_setup(&patch->dx[d]);
      fld1d_setup(&patch->inv_dx[d]);
      fld1d_setup(&patch->inv_dxf[d]);

      for (int i = -s_sw[d]; i < s_ldims[d] + s_sw[d]; i++) {
	F1(patch->dx[d], i) = MRC_DMCRD_NC(crds, d, i+1, p) - MRC_DMCRD_NC(crds, d, i, p);
	F1(patch->inv_dx[d], i) = 1.f / F1(patch->dx[d], i);
      }
      
      for (int i = -s_sw[d] + 1; i < s_ldims[d] + s_sw[d]; i++) {
	F1(patch->inv_dxf[d], i) = 1. / (MRC_DMCRD(crds, d, i, p) - MRC_DMCRD(crds, d, i-1, p));
      }

      for (int i = 0; i < s_ldims[d]; i++) {
	dxmin[d] = fmin(dxmin[d], F1(patch->dx[d], i));
      }
    }
  }

  MPI_Allreduce(dxmin, s_g_dxmin, 3, MPI_DOUBLE, MPI_MIN, mrc_fld_comm(fld));
  s_g_dxyzmin = 1e10;
  for (int d = 0; d < 3; d++) {
    s_g_dxyzmin = mrc_fld_min(s_g_dxyzmin, s_g_dxmin[d]);
  }
}

// ======================================================================
// loop over lines // loop along line

static fld1d_t s_line_inv_dx;

// FIXME, this is too easily confused with PDE_INV_DX
#define PDE_INV_DS(i) F1(s_line_inv_dx, i)

// ----------------------------------------------------------------------
// pde_for_each_dir

#define pde_for_each_dir(dir)			\
      for (int dir = 0; dir < 3; dir++)		\
	if (s_sw[dir] > 0)

// ----------------------------------------------------------------------
// pde_line_set_dir

static void _mrc_unused
pde_line_set_dir(int dir)
{
  s_line_inv_dx = s_patch.inv_dx[dir];
}

// ----------------------------------------------------------------------
// pde_for_each_line

#define pde_for_each_line(dir, j, k, sw)				\
  int j, k, *_i1, *_i2, _i1b, _i1e, _i2b, _i2e;				\
  if (dir == 0) {							\
    _i1 = &j; _i2 = &k;							\
    _i1b = s_sw[1] ? -sw : 0; _i1e = s_ldims[1] + (s_sw[1] ? sw : 0);	\
    _i2b = s_sw[2] ? -sw : 0; _i2e = s_ldims[2] + (s_sw[2] ? sw : 0);	\
  } else if (dir == 1) {						\
    _i1 = &k; _i2 = &j;							\
    _i1b = s_sw[0] ? -sw : 0; _i1e = s_ldims[0] + (s_sw[0] ? sw : 0);	\
    _i2b = s_sw[2] ? -sw : 0; _i2e = s_ldims[2] + (s_sw[2] ? sw : 0);	\
  } else if (dir == 2) {						\
    _i1 = &j; _i2 = &k;							\
    _i1b = s_sw[0] ? -sw : 0; _i1e = s_ldims[0] + (s_sw[0] ? sw : 0);	\
    _i2b = s_sw[1] ? -sw : 0; _i2e = s_ldims[1] + (s_sw[1] ? sw : 0);	\
  } else {								\
    assert(0);								\
  }									\
  for (*_i2 = _i2b; *_i2 < _i2e; (*_i2)++)				\
    for (*_i1 = _i1b; *_i1 < _i1e; (*_i1)++)


