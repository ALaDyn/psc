
#include <mrc_params.h>
#include <mrc_domain.h>
#include <mrc_fld.h>
#include <mrc_ddc.h>
#include <mrc_io.h>
#include <mrctest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define AMR

// ----------------------------------------------------------------------
// x-y plane, Xx: interior point, Oo: ghost point

// Bx
// +-------+---+---+
// |   o   o   x   x
// X       X---+---O
// |   o   o   x   x
// +-------+---+---+

// By
// +---X-o-+-x-O-x-+
// |       |   |   |
// |     o +-x-+-x-+
// |       |   |   |
// +---X-o-+-x-O-x-+

// Bz
// +-------+---+---+
// | o   o | x | x |
// |   X   +---O---+
// | o   o | x | x |
// +-------+---+---+

// Ex
// +-o-X-o-+-x-O-x-+
// |       |   |   |
// |       +-x-+-x-+
// |       |   |   |
// +-o-X-o-+-x-O-x-+

// Ey
// +-------+---+---+
// o   o   x   x   x
// X       X---+---O
// o   o   x   x   x
// +-------+---+---+

// Ez
// X---o---X---x---O
// |       |   |   |
// o   o   x---x---x
// |       |   |   |
// X---o---X---x---O

struct mrc_ddc_amr_stencil_entry {
  int dx[3];
  float val;
};

struct mrc_ddc_amr_stencil {
  struct mrc_ddc_amr_stencil_entry *s;
  int nr_entries;
};
  
// ----------------------------------------------------------------------
// mrc_domain_get_neighbor_patch_same

static void
mrc_domain_get_neighbor_patch_same(struct mrc_domain *domain, int p,
				   int dx[3], int *p_nei)
{
  struct mrc_patch_info pi, pi_nei;
  mrc_domain_get_local_patch_info(domain, p, &pi);
  // FIXME: how about if we only refine in selected directions?
  int mx[3] = { 1 << pi.level, 1 << pi.level, 1 << pi.level };
  int idx3[3];
  for (int d = 0; d < 3; d++) {
    idx3[d] = pi.idx3[d] + dx[d];
    if (idx3[d] < 0) {
      idx3[d] += mx[d];
    }
    if (idx3[d] >= mx[d]) {
      idx3[d] -= mx[d];
    }
  }
  mrc_domain_get_level_idx3_patch_info(domain, pi.level, idx3, &pi_nei);
  *p_nei = pi_nei.patch;
}

// ----------------------------------------------------------------------
// mrc_domain_get_neighbor_patch_coarse

static void
mrc_domain_get_neighbor_patch_coarse(struct mrc_domain *domain, int p,
				     int dx[3], int *p_nei)
{
  struct mrc_patch_info pi, pi_nei;
  mrc_domain_get_local_patch_info(domain, p, &pi);
  // FIXME: how about if we only refine in selected directions?
  int mx[3] = { 1 << (pi.level - 1), 1 << (pi.level - 1), 1 << (pi.level - 1) };
  int idx3[3];
  for (int d = 0; d < 3; d++) {
    idx3[d] = (pi.idx3[d] + dx[d] + 2) / 2 - 1;
    if (idx3[d] < 0) {
      idx3[d] += mx[d];
    }
    if (idx3[d] >= mx[d]) {
      idx3[d] -= mx[d];
    }
  }
  mrc_domain_get_level_idx3_patch_info(domain, pi.level - 1, idx3, &pi_nei);
  *p_nei = pi_nei.patch;
}

// ----------------------------------------------------------------------
// mrc_domain_get_neighbor_patch_fine

static void
mrc_domain_get_neighbor_patch_fine(struct mrc_domain *domain, int p,
				   int dir[3], int off[3], int *p_nei)
{
  struct mrc_patch_info pi, pi_nei;
  mrc_domain_get_local_patch_info(domain, p, &pi);
  // FIXME: how about if we only refine in selected directions?
  int mx[3] = { 1 << pi.level, 1 << pi.level, 1 << pi.level };
  int idx3[3];
  for (int d = 0; d < 3; d++) {
    idx3[d] = pi.idx3[d] + dir[d];
    if (idx3[d] < 0) {
      idx3[d] += mx[d];
    }
    if (idx3[d] >= mx[d]) {
      idx3[d] -= mx[d];
    }
    idx3[d] = 2 * idx3[d] + off[d];
  }
  mrc_domain_get_level_idx3_patch_info(domain, pi.level + 1, idx3, &pi_nei);
  *p_nei = pi_nei.patch;
}

// ======================================================================

// this function incorporates Fujimoto (2011)-specific FDTD policy to
// some extent, ie., points on the boundary between coarse and fine levels
// are considered interior points on the coarse level, ghost points on the
// fine level.

static bool
mrc_domain_is_ghost(struct mrc_domain *domain, int ext[3], int p, int i[3])
{
  int ldims[3];
  mrc_domain_get_param_int3(domain, "m", ldims);

  int dir[3], dirx[3] = {};
  for (int d = 0; d < 3; d++) {
    if (i[d] < 0) {
      return true;
    } else if (ext[d] == 1 && i[d] == 0) {
      dir[d] = 0;
      dirx[d] = 1;
    } else if (i[d] < ldims[d]) {
      dir[d] = 0;
    } else if (ext[d] == 1 && i[d] == ldims[d]) {
      dir[d] = 1;
      dirx[d] = 1;
    } else {
      return true;
    }
  }
  // if outside, we've already returned true

  // inside, not on the boundary
  if (dir[0] == 0 && dirx[0] == 0 &&
      dir[1] == 0 && dirx[1] == 0) {
    return false;
  }

  // on the boundary...
  int dd[3];
  // do we border a coarse domain? (then it's a ghost point)
  for (dd[2] = 0; dd[2] >= 0; dd[2]--) {
    for (dd[1] = dir[1]; dd[1] >= dir[1] - dirx[1]; dd[1]--) {
      for (dd[0] = dir[0]; dd[0] >= dir[0] - dirx[0]; dd[0]--) {
	if (dd[0] == 0 && dd[1] == 0 && dd[2] == 0) {
	  continue;
	}
	int p_nei;
	mrc_domain_get_neighbor_patch_coarse(domain, p, dd, &p_nei);
	if (p_nei >= 0) {
	  return true;
	}
      }
    }
  }

  // is another same level patch in line before us, then it's his, and we have
  // a ghost point
  for (dd[2] = 0; dd[2] >= 0; dd[2]--) {
    for (dd[1] = dir[1]; dd[1] >= dir[1] - dirx[1]; dd[1]--) {
      for (dd[0] = dir[0]; dd[0] >= dir[0] - dirx[0]; dd[0]--) {
	int p_nei;
	mrc_domain_get_neighbor_patch_same(domain, p, dd, &p_nei);
	if (p_nei >= 0) {
	  return p != p_nei;
	}
      }
    }
  }
  return true;
}

static void
mrc_domain_find_valid_point_same(struct mrc_domain *domain, int ext[3], int p, int i[3],
				 int *p_nei, int j[3])
{
  int ldims[3];
  mrc_domain_get_param_int3(domain, "m", ldims);

  int dir[3], dirx[3] = {};
  for (int d = 0; d < 3; d++) {
    if (i[d] < 0) {
      dir[d] = -1;
    } else if (ext[d] == 1 && i[d] == 0) {
      dir[d] = 0;
      dirx[d] = 1;
    } else if (i[d] < ldims[d]) {
      dir[d] = 0;
    } else if (ext[d] == 1 && i[d] == ldims[d]) {
      dir[d] = 1;
      dirx[d] = 1;
    } else {
      dir[d] = 1;
    }
  }

  int dd[3];
  for (dd[2] = 0; dd[2] >= 0; dd[2]--) {
    for (dd[1] = dir[1]; dd[1] >= dir[1] - dirx[1]; dd[1]--) {
      for (dd[0] = dir[0]; dd[0] >= dir[0] - dirx[0]; dd[0]--) {
	if (dd[0] == 0 && dd[1] == 0 && dd[2] == 0) {
	  continue;
	}
	mrc_domain_get_neighbor_patch_same(domain, p, dd, p_nei);
	if (*p_nei >= 0) {
	  for (int d = 0; d < 3; d++) {
	    j[d] = i[d] - dd[d] * ldims[d];
	  }
	  // need to double check whether we actually picked an interior point
	  if (!mrc_domain_is_ghost(domain, ext, *p_nei, j)) {
	    return;
	  }
	}
      }
    }
  }
  *p_nei = -1;
}

static void
mrc_domain_to_valid_point_same(struct mrc_domain *domain, int ext[3], int p, int i[3],
			       int *p_nei, int j[3])
{
  if (!mrc_domain_is_ghost(domain, ext, p, i)) {
    for (int d = 0; d < 3; d++) {
      j[d] = i[d];
    }
    *p_nei = p;
    return;
  }

  mrc_domain_find_valid_point_same(domain, ext, p, i, p_nei, j);
}

static void
mrc_domain_find_valid_point_coarse(struct mrc_domain *domain, int ext[3],
				   int p, int i[3], int *p_nei, int j[3])
{
  int ldims[3];
  mrc_domain_get_param_int3(domain, "m", ldims);
  struct mrc_patch_info pi;
  mrc_domain_get_local_patch_info(domain, p, &pi);
    
  int ii[3], dir[3], dirx[3] = {};
  for (int d = 0; d < 3; d++) {
    ii[d] = i[d] + ((pi.idx3[d] & 1) ? ldims[d] / 2 : 0);

    if (ii[d] < 0) {
      dir[d] = -1;
    } else if (ext[d] == 1 && ii[d] == 0) {
      dir[d] = 0;
      dirx[d] = 1;
    } else if (ii[d] < ldims[d]) {
      dir[d] = 0;
    } else if (ext[d] == 1 && ii[d] == ldims[d]) {
      dir[d] = 1;
      dirx[d] = 1;
    } else {
      dir[d] = 1;
    }
  }

  int dd[3];
  for (dd[2] = dir[2]; dd[2] >= dir[2] - dirx[2]; dd[2]--) {
    for (dd[1] = dir[1]; dd[1] >= dir[1] - dirx[1]; dd[1]--) {
      for (dd[0] = dir[0]; dd[0] >= dir[0] - dirx[0]; dd[0]--) {
	if (dd[0] == 0 && dd[1] == 0 && dd[2] == 0) {
	  continue;
	}
	mrc_domain_get_neighbor_patch_coarse(domain, p, dd, p_nei);
	if (*p_nei >= 0) {
	  for (int d = 0; d < 3; d++) {
	    j[d] = ii[d] - dd[d] * ldims[d];
	  }
	  return;
	}
      }
    }
  }
  *p_nei = -1;
}

static void
mrc_domain_find_valid_point_fine(struct mrc_domain *domain, int p, int i[3],
				 int *p_nei, int j[3])
{
  int ldims[3];
  mrc_domain_get_param_int3(domain, "m", ldims);
  struct mrc_patch_info pi;
  mrc_domain_get_local_patch_info(domain, p, &pi);
    
  int ii[3], off[3], dir[3];
  for (int d = 0; d < 3; d++) {
    off[d] = (i[d] + ldims[d]) / ldims[d] - 1;
    ii[d] = (i[d] + ldims[d]) % ldims[d];
    if (ii[d] < 0) {
      dir[d] = -1;
    } else if (ii[d] < ldims[d]) {
      dir[d] = 0;
    } else {
      dir[d] = 1;
    }
  }

  mrc_domain_get_neighbor_patch_fine(domain, p, dir, off, p_nei);
  if (*p_nei < 0) {
    return;
  }

  for (int d = 0; d < 3; d++) {
    j[d] = ii[d] - dir[d] * ldims[d];
  }
}

static inline int
div_2(int i)
{
  // divide by 2, but always round down
  return (i + 10) / 2 - 5;
}

static bool
mrc_ddc_amr_stencil_coarse(struct mrc_ddc *ddc, int ext[3],
			   struct mrc_ddc_amr_stencil *stencil,
			   int m, int p, int i[3])
{
  struct mrc_domain *domain = mrc_ddc_get_domain(ddc);

  int p_nei, j[3];
  mrc_domain_find_valid_point_coarse(domain, ext, p,
				     (int[]) { div_2(i[0]), div_2(i[1]), div_2(i[2]) },
				     &p_nei, j);
  if (p_nei < 0) {
    return false;
  }

  for (struct mrc_ddc_amr_stencil_entry *s = stencil->s; s < stencil->s + stencil->nr_entries; s++) {
    int jd[3], p_dnei, j_dnei[3];
    for (int d = 0; d < 3; d++) {
      jd[d] = j[d] + s->dx[d] * (i[d] & 1 && d < 2); // FIXME 3D
    }
    mrc_domain_to_valid_point_same(domain, ext, p_nei, jd, &p_dnei, j_dnei);
    assert(!mrc_domain_is_ghost(domain, ext, p_dnei, j_dnei));
    mrc_ddc_amr_add_value(ddc, p, m, i, p_dnei, m, j_dnei, s->val);
  }
  return true;
}

static bool
mrc_ddc_amr_stencil_fine(struct mrc_ddc *ddc, int ext[3],
			 struct mrc_ddc_amr_stencil *stencil,
			 int m, int p, int i[3])
{
  struct mrc_domain *domain = mrc_ddc_get_domain(ddc);

  int p_nei, j[3];
  mrc_domain_find_valid_point_fine(domain, p, (int[]) { 2*i[0], 2*i[1], 2*i[2] }, &p_nei, j);
  if (p_nei < 0) {
    return false;
  }

  for (struct mrc_ddc_amr_stencil_entry *s = stencil->s; s < stencil->s + stencil->nr_entries; s++) {
    int id[3];
    for (int d = 0; d < 3; d++) {
      id[d] = 2*i[d] + s->dx[d];
    }
    mrc_domain_find_valid_point_fine(domain, p, id, &p_nei, j);
    assert(!mrc_domain_is_ghost(domain, ext, p_nei, j));
    mrc_ddc_amr_add_value(ddc, p, m, i, p_nei, m, j, s->val);
  }
  return true;
}

// ================================================================================

static void
mrc_ddc_amr_set_by_stencil(struct mrc_ddc *ddc, int m, int bnd, int ext[3],
			   struct mrc_ddc_amr_stencil *stencil_coarse,
			   struct mrc_ddc_amr_stencil *stencil_fine)
{
  struct mrc_domain *domain = mrc_ddc_get_domain(ddc);

  int ldims[3];
  mrc_domain_get_param_int3(domain, "m", ldims);
  int nr_patches;
  mrc_domain_get_patches(domain, &nr_patches);

  for (int p = 0; p < nr_patches; p++) {
    int i[3];
    for (i[2] = 0; i[2] < ldims[2] + 0; i[2]++) { // FIXME 3D
      for (i[1] = -bnd; i[1] < ldims[1] + ext[1] + bnd; i[1]++) {
	for (i[0] = -bnd; i[0] < ldims[0] + ext[0] + bnd; i[0]++) {
	  if (i[0] >= ext[0] && i[0] < ldims[0] &&
	      i[1] >= ext[1] && i[1] < ldims[1] &&
	      i[2] >= ext[2] && i[2] < ldims[2]) {
	    assert(!mrc_domain_is_ghost(domain, ext, p, i));
	    continue;
	  }
	  if (!mrc_domain_is_ghost(domain, ext, p, i)) {
	    continue;
	  }

	  // at this point, we skipped all interior points, so only ghostpoints are left

	  // try to find an interior point corresponding to the current ghostpoint
	  int j[3], p_nei;
	  mrc_domain_find_valid_point_same(domain, ext, p, i, &p_nei, j);
	  if (p_nei >= 0) {
	    assert(!mrc_domain_is_ghost(domain, ext, p_nei, j));
	    mrc_ddc_amr_add_value(ddc, p, m, i, p_nei, m, j, 1.f);
	    continue;
	  }

	  // try to interpolate from coarse
	  if (mrc_ddc_amr_stencil_coarse(ddc, ext, stencil_coarse, m, p, i)) {
	    continue;
	  }
	      
	  // try to restrict from fine
	  if (mrc_ddc_amr_stencil_fine(ddc, ext, stencil_fine, m, p, i)) {
	    continue;
	  }

	  // oops, no way to fill this point?
	  // (This may be okay if the domain has holes or other physical boundaries)
	  MHERE;
	}
      }
    }
  }
}

#define F3 MRC_M3

// ======================================================================

enum {
  EX,
  EY,
  EZ,
  HX,
  HY,
  HZ,
  NR_COMPS,
};

static struct mrc_ddc_amr_stencil_entry stencil_coarse_EY[2] = {
  // FIXME, 3D
  { .dx = { 0, 0, 0 }, .val = .5f },
  { .dx = { 1, 0, 0 }, .val = .5f },
};

static struct mrc_ddc_amr_stencil_entry stencil_coarse_EZ[4] = {
  { .dx = { 0, 0, 0 }, .val = .25f },
  { .dx = { 1, 0, 0 }, .val = .25f },
  { .dx = { 0, 1, 0 }, .val = .25f },
  { .dx = { 1, 1, 0 }, .val = .25f },
};

static struct mrc_ddc_amr_stencil_entry stencil_coarse_HY[2] = {
  // FIXME, 3D
  { .dx = { 0, 0, 0 }, .val = .5f },
  { .dx = { 0, 1, 0 }, .val = .5f },
};

static struct mrc_ddc_amr_stencil_entry stencil_coarse_HZ[2] = {
  // FIXME, 3D
  { .dx = { 0, 0, 0 }, .val = .5f },
  { .dx = { 0, 0, 1 }, .val = .5f },
};

static struct mrc_ddc_amr_stencil stencils_coarse[NR_COMPS] = {
  [EY] = { stencil_coarse_EY, ARRAY_SIZE(stencil_coarse_EY) },
  [EZ] = { stencil_coarse_EZ, ARRAY_SIZE(stencil_coarse_EZ) },
  [HY] = { stencil_coarse_HY, ARRAY_SIZE(stencil_coarse_HY) },
  [HZ] = { stencil_coarse_HZ, ARRAY_SIZE(stencil_coarse_HZ) },
};

static struct mrc_ddc_amr_stencil_entry stencil_fine_EY[6] = {
  // FIXME, 3D
  { .dx = { -1,  0,  0 }, .val = (1.f/8.f) * 1.f },
  { .dx = { -1, +1,  0 }, .val = (1.f/8.f) * 1.f },
  { .dx = {  0,  0,  0 }, .val = (1.f/8.f) * 2.f },
  { .dx = {  0, +1,  0 }, .val = (1.f/8.f) * 2.f },
  { .dx = { +1,  0,  0 }, .val = (1.f/8.f) * 1.f },
  { .dx = { +1, +1,  0 }, .val = (1.f/8.f) * 1.f },
};

static struct mrc_ddc_amr_stencil_entry stencil_fine_EZ[9] = {
  // FIXME, 3D
  { .dx = { -1, -1,  0 }, .val = (2.f/8.f) * .25f },
  { .dx = {  0, -1,  0 }, .val = (2.f/8.f) * .5f  },
  { .dx = { +1, -1,  0 }, .val = (2.f/8.f) * .25f },
  { .dx = { -1,  0,  0 }, .val = (2.f/8.f) * .5f  },
  { .dx = {  0,  0,  0 }, .val = (2.f/8.f) * 1.f  },
  { .dx = { +1,  0,  0 }, .val = (2.f/8.f) * .5f  },
  { .dx = { -1, +1,  0 }, .val = (2.f/8.f) * .25f },
  { .dx = {  0, +1,  0 }, .val = (2.f/8.f) * .5f  },
  { .dx = { +1, +1,  0 }, .val = (2.f/8.f) * .25f },
};

static struct mrc_ddc_amr_stencil_entry stencil_fine_HY[6] = {
  // FIXME, 3D
  { .dx = {  0, -1,  0 }, .val = (2.f/8.f) * .5f },
  { .dx = { +1, -1,  0 }, .val = (2.f/8.f) * .5f },
  { .dx = {  0,  0,  0 }, .val = (2.f/8.f) * 1.f },
  { .dx = { +1,  0,  0 }, .val = (2.f/8.f) * 1.f },
  { .dx = {  0, +1,  0 }, .val = (2.f/8.f) * .5f },
  { .dx = { +1, +1,  0 }, .val = (2.f/8.f) * .5f },
};
	  
static struct mrc_ddc_amr_stencil_entry stencil_fine_HZ[4] = {
  // FIXME, 3D
  { .dx = {  0,  0,  0 }, .val = (2.f/8.f) * 1.f },
  { .dx = { +1,  0,  0 }, .val = (2.f/8.f) * 1.f },
  { .dx = {  0, +1,  0 }, .val = (2.f/8.f) * 1.f },
  { .dx = { +1, +1,  0 }, .val = (2.f/8.f) * 1.f },
};

static struct mrc_ddc_amr_stencil stencils_fine[NR_COMPS] = {
  [EY] = { stencil_fine_EY, ARRAY_SIZE(stencil_fine_EY) },
  [EZ] = { stencil_fine_EZ, ARRAY_SIZE(stencil_fine_EZ) },
  [HY] = { stencil_fine_HY, ARRAY_SIZE(stencil_fine_HY) },
  [HZ] = { stencil_fine_HZ, ARRAY_SIZE(stencil_fine_HZ) },
};

static void __unused
find_ghosts(struct mrc_domain *domain, struct mrc_m3 *fld, int m,
	    int ext[3], int bnd)
{
  int ldims[3];
  mrc_domain_get_param_int3(fld->domain, "m", ldims);
  int nr_patches;
  mrc_domain_get_patches(domain, &nr_patches);

  for (int p = 0; p < nr_patches; p++) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    for (int iz = 0; iz < ldims[2] + 0; iz++) {
      for (int iy = -bnd; iy < ldims[1] + ext[1] + bnd; iy++) {
	for (int ix = -bnd; ix < ldims[0] + ext[0] + bnd; ix++) {
	  bool is_ghost = mrc_domain_is_ghost(domain, ext, p, (int[]) { ix, iy, iz });
	  if (!is_ghost) {
	    MRC_M3(fldp, m, ix,iy,iz) = 1.;
	  } else {
	    MRC_M3(fldp, m, ix,iy,iz) = 1./0.;
	  }
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// step_fdtd

static void
step_fdtd(struct mrc_m3 *fld, struct mrc_ddc *ddc_E, struct mrc_ddc *ddc_H)
{
  struct mrc_crds *crds = mrc_domain_get_crds(fld->domain);
#ifdef AMR
  float dt = 1. / 64;
#else
  float dt = 1. / 16;
#endif

  mrc_ddc_amr_apply(ddc_H, fld);

  mrc_m3_foreach_patch(fld, p) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    mrc_crds_patch_get(crds, p);
    float dx = MRC_MCRDX(crds, 1) - MRC_MCRDX(crds, 0); // FIXME
    //    float dy = MRC_MCRDY(crds, 1) - MRC_MCRDY(crds, 0);
    float cnx = .5 * dt / dx;
    float cny = 0.;//.5 * dt / dy;
    float cnz = 0.;
    mrc_m3_foreach(fldp, ix,iy,iz, 0, 1) {
      F3(fldp, EX, ix,iy,iz) +=
      	cny * (F3(fldp, HZ, ix,iy,iz) - F3(fldp, HZ, ix,iy-1,iz)) -
      	cnz * (F3(fldp, HY, ix,iy,iz) - F3(fldp, HY, ix,iy,iz-1));

      F3(fldp, EY, ix,iy,iz) +=
	cnz * (F3(fldp, HX, ix,iy,iz) - F3(fldp, HX, ix,iy,iz-1)) -
	cnx * (F3(fldp, HZ, ix,iy,iz) - F3(fldp, HZ, ix-1,iy,iz));

      F3(fldp, EZ, ix,iy,iz) +=
      	cnx * (F3(fldp, HY, ix,iy,iz) - F3(fldp, HY, ix-1,iy,iz)) -
      	cny * (F3(fldp, HX, ix,iy,iz) - F3(fldp, HX, ix,iy-1,iz));
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(fld);
    mrc_crds_patch_put(crds);
  }

  mrc_ddc_amr_apply(ddc_E, fld);

  mrc_m3_foreach_patch(fld, p) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    mrc_crds_patch_get(crds, p);
    float dx = MRC_MCRDX(crds, 1) - MRC_MCRDX(crds, 0); // FIXME
    //    float dy = MRC_MCRDY(crds, 1) - MRC_MCRDY(crds, 0);
    float cnx = .5 * dt / dx;
    float cny = 0.;//.5 * dt / dy;
    float cnz = 0.;
    mrc_m3_foreach(fldp, ix,iy,iz, 0, 1) {
      F3(fldp, HX, ix,iy,iz) -=
	cny * (F3(fldp, EZ, ix,iy+1,iz) - F3(fldp, EZ, ix,iy,iz)) -
	cnz * (F3(fldp, EY, ix,iy,iz+1) - F3(fldp, EY, ix,iy,iz));
      
      F3(fldp, HY, ix,iy,iz) -=
	cnz * (F3(fldp, EX, ix,iy,iz+1) - F3(fldp, EX, ix,iy,iz)) -
	cnx * (F3(fldp, EZ, ix+1,iy,iz) - F3(fldp, EZ, ix,iy,iz));
      
      F3(fldp, HZ, ix,iy,iz) -=
	cnx * (F3(fldp, EY, ix+1,iy,iz) - F3(fldp, EY, ix,iy,iz)) -
	cny * (F3(fldp, EX, ix,iy+1,iz) - F3(fldp, EX, ix,iy,iz));
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(fld);
    mrc_crds_patch_put(crds);
  }

  mrc_m3_foreach_patch(fld, p) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    mrc_crds_patch_get(crds, p);
    float dx = MRC_MCRDX(crds, 1) - MRC_MCRDX(crds, 0); // FIXME
    //    float dy = MRC_MCRDY(crds, 1) - MRC_MCRDY(crds, 0);
    float cnx = .5 * dt / dx;
    float cny = 0.;//.5 * dt / dy;
    float cnz = 0.;
    mrc_m3_foreach(fldp, ix,iy,iz, 0, 1) {
      F3(fldp, HX, ix,iy,iz) -=
	cny * (F3(fldp, EZ, ix,iy+1,iz) - F3(fldp, EZ, ix,iy,iz)) -
	cnz * (F3(fldp, EY, ix,iy,iz+1) - F3(fldp, EY, ix,iy,iz));
      
      F3(fldp, HY, ix,iy,iz) -=
	cnz * (F3(fldp, EX, ix,iy,iz+1) - F3(fldp, EX, ix,iy,iz)) -
	cnx * (F3(fldp, EZ, ix+1,iy,iz) - F3(fldp, EZ, ix,iy,iz));
      
      F3(fldp, HZ, ix,iy,iz) -=
	cnx * (F3(fldp, EY, ix+1,iy,iz) - F3(fldp, EY, ix,iy,iz)) -
	cny * (F3(fldp, EX, ix,iy+1,iz) - F3(fldp, EX, ix,iy,iz));
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(fld);
    mrc_crds_patch_put(crds);
  }

  mrc_ddc_amr_apply(ddc_H, fld);

  mrc_m3_foreach_patch(fld, p) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    mrc_crds_patch_get(crds, p);
    float dx = MRC_MCRDX(crds, 1) - MRC_MCRDX(crds, 0); // FIXME
    //    float dy = MRC_MCRDY(crds, 1) - MRC_MCRDY(crds, 0);
    float cnx = .5 * dt / dx;
    float cny = 0.;//.5 * dt / dy;
    float cnz = 0.;
    mrc_m3_foreach(fldp, ix,iy,iz, 0, 1) {
      F3(fldp, EX, ix,iy,iz) +=
	cny * (F3(fldp, HZ, ix,iy,iz) - F3(fldp, HZ, ix,iy-1,iz)) -
	cnz * (F3(fldp, HY, ix,iy,iz) - F3(fldp, HY, ix,iy,iz-1));
      
      F3(fldp, EY, ix,iy,iz) +=
	cnz * (F3(fldp, HX, ix,iy,iz) - F3(fldp, HX, ix,iy,iz-1)) -
	cnx * (F3(fldp, HZ, ix,iy,iz) - F3(fldp, HZ, ix-1,iy,iz));
      
      F3(fldp, EZ, ix,iy,iz) +=
	cnx * (F3(fldp, HY, ix,iy,iz) - F3(fldp, HY, ix-1,iy,iz)) -
	cny * (F3(fldp, HX, ix,iy,iz) - F3(fldp, HX, ix,iy-1,iz));
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(fld);
    mrc_crds_patch_put(crds);
  }

}

float
func1(float x, float y)
{
  float kx = 2. * M_PI;
  return sin(.5 + kx * x);
}

float
func2(float x, float y)
{
  float kx = 2. * M_PI, ky = 2. * M_PI;
  return sin(.5 + kx * x) * cos(.5 + ky * y);
}

float (*func)(float, float) = func2;

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  struct mrc_domain *domain = mrc_domain_create(MPI_COMM_WORLD);
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  mrc_domain_set_type(domain, "amr");
  mrc_domain_set_param_int3(domain, "m", (int [3]) { 8, 8, 1});
  mrc_crds_set_type(crds, "amr_uniform");
  mrc_crds_set_param_int(crds, "sw", 3);
  
  mrc_domain_set_from_options(domain);
  mrctest_set_amr_domain_3(domain);

  mrc_domain_setup(domain);
  mrc_domain_plot(domain);

  // create and fill a field

  struct mrc_m3 *fld = mrc_domain_m3_create(domain);
  mrc_m3_set_name(fld, "fld");
  mrc_m3_set_param_int(fld, "nr_comps", NR_COMPS);
  mrc_m3_set_param_int(fld, "sw", 3);
  mrc_m3_set_from_options(fld);
  mrc_m3_setup(fld);
  mrc_m3_set_comp_name(fld, EX, "EX");
  mrc_m3_set_comp_name(fld, EY, "EY");
  mrc_m3_set_comp_name(fld, EZ, "EZ");
  mrc_m3_set_comp_name(fld, HX, "HX");
  mrc_m3_set_comp_name(fld, HY, "HY");
  mrc_m3_set_comp_name(fld, HZ, "HZ");

  int ldims[3];
  mrc_domain_get_param_int3(fld->domain, "m", ldims);

  mrc_m3_foreach_patch(fld, p) {
    struct mrc_m3_patch *fldp = mrc_m3_patch_get(fld, p);
    mrc_crds_patch_get(crds, p);

#if 1
    mrc_m3_foreach(fldp, ix,iy,iz, 3, 3) {
      MRC_M3(fldp, EY, ix,iy,iz) = 1.f / 0.f;
      MRC_M3(fldp, EZ, ix,iy,iz) = 1.f / 0.f;
      MRC_M3(fldp, HY, ix,iy,iz) = 1.f / 0.f;
      MRC_M3(fldp, HZ, ix,iy,iz) = 1.f / 0.f;
    } mrc_m3_foreach_end;
#endif
    mrc_m3_foreach(fldp, ix,iy,iz, 0, 1) {
      float x_cc = MRC_MCRDX(crds, ix);
      float y_cc = MRC_MCRDY(crds, iy);
      float x_nc = .5f * (MRC_MCRDX(crds, ix-1) + MRC_MCRDX(crds, ix));
      float y_nc = .5f * (MRC_MCRDY(crds, iy-1) + MRC_MCRDY(crds, iy));
      if (!mrc_domain_is_ghost(domain, (int[]) { 1, 0, 1 }, p, (int[]) { ix, iy, iz })) {
	MRC_M3(fldp, EY, ix,iy,iz) = func(x_nc, y_cc);
      }
      if (!mrc_domain_is_ghost(domain, (int[]) { 1, 1, 0 }, p, (int[]) { ix, iy, iz })) {
	MRC_M3(fldp, EZ, ix,iy,iz) = func(x_nc, y_nc);
      }
      if (!mrc_domain_is_ghost(domain, (int[]) { 0, 1, 0 }, p, (int[]) { ix, iy, iz })) {
	MRC_M3(fldp, HY, ix,iy,iz) = func(x_cc, y_nc);
      }
      if (!mrc_domain_is_ghost(domain, (int[]) { 0, 0, 1 }, p, (int[]) { ix, iy, iz })) {
	MRC_M3(fldp, HZ, ix,iy,iz) = func(x_cc, y_cc);
      }
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(fld);
    mrc_crds_patch_put(crds);
  }

  struct mrc_ddc *ddc_E = mrc_ddc_create(mrc_domain_comm(domain));
  mrc_ddc_set_type(ddc_E, "amr");
  mrc_ddc_set_domain(ddc_E, domain);
  mrc_ddc_set_param_int(ddc_E, "sw", fld->sw);
  mrc_ddc_setup(ddc_E);
  mrc_ddc_amr_set_by_stencil(ddc_E, EY, 2, (int[]) { 1, 0, 1 }, &stencils_coarse[EY], &stencils_fine[EY]);
  mrc_ddc_amr_set_by_stencil(ddc_E, EZ, 2, (int[]) { 1, 1, 0 }, &stencils_coarse[EZ], &stencils_fine[EZ]);
  mrc_ddc_amr_assemble(ddc_E);

  struct mrc_ddc *ddc_H = mrc_ddc_create(mrc_domain_comm(domain));
  mrc_ddc_set_type(ddc_H, "amr");
  mrc_ddc_set_domain(ddc_H, domain);
  mrc_ddc_set_param_int(ddc_H, "sw", fld->sw);
  mrc_ddc_setup(ddc_H);
  mrc_ddc_amr_set_by_stencil(ddc_H, HY, 2, (int[]) { 0, 1, 0 }, &stencils_coarse[HY], &stencils_fine[HY]);
  mrc_ddc_amr_set_by_stencil(ddc_H, HZ, 2, (int[]) { 0, 0, 1 }, &stencils_coarse[HZ], &stencils_fine[HZ]);
  mrc_ddc_amr_assemble(ddc_H);

  // write field to disk

  struct mrc_io *io = mrc_io_create(mrc_domain_comm(domain));
  mrc_io_set_type(io, "xdmf2");
  mrc_io_set_param_int(io, "sw", 3);
  mrc_io_set_from_options(io);
  mrc_io_setup(io);

  mrc_io_open(io, "w", 0, 0);
  mrc_m3_write(fld, io);
  mrc_io_close(io);

  mrc_ddc_amr_apply(ddc_E, fld);
  mrc_ddc_amr_apply(ddc_H, fld);
#if 0
  find_ghosts(domain, fld, EY, (int[]) { 1, 0, 1 }, 2);
  find_ghosts(domain, fld, EZ, (int[]) { 1, 1, 0 }, 2);
#endif

  mrc_io_open(io, "w", 1, 1);
  mrc_m3_write(fld, io);
  mrc_io_close(io);

  for (int n = 0; n <= 32; n++) {
    mrc_io_open(io, "w", n+2, n+2);
    mrc_m3_write(fld, io);
    mrc_io_close(io);

    step_fdtd(fld, ddc_E, ddc_H);
  }

  mrc_io_destroy(io);

  mrc_ddc_destroy(ddc_E);
  mrc_ddc_destroy(ddc_H);

  mrc_m3_destroy(fld);

  mrc_domain_destroy(domain);

  MPI_Finalize();
}
