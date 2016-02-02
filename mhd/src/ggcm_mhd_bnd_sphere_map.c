
#include "ggcm_mhd_bnd_private.h"

#include "ggcm_mhd_private.h"

#include <mrc_domain.h>
#include <mrc_bits.h>
#include <math.h>

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_find_dr
//
// find minimum cell size (over all of the domain -- FIXME?)

static void
ggcm_mhd_bnd_sphere_map_find_dr(struct ggcm_mhd_bnd_sphere_map *map, double *dr)
{
  // FIXME, it'd make sense to base this on the ionosphere boundary region
  // (e.g., box of +/- 7 RE in all directions, as later used in determining
  // r1, r2). It shouldn't really hurt if the dr determined here is too small,
  // though it'll slow down finding the proper r1, r2.

  struct ggcm_mhd *mhd = map->mhd;

  double min_dr = 1.e30;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  for (int p = 0; p < mrc_fld_nr_patches(mhd->fld); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    for (int d = 0; d < 3; d++) {
      for (int i = 0; i < info.ldims[d]; i++) {
	min_dr = fmin(min_dr, MRC_MCRD(crds, d, i, p) - MRC_MCRD(crds, d, i-1, p));
      }
    }
  }
  MPI_Allreduce(&min_dr, dr, 1, MPI_DOUBLE, MPI_MIN, ggcm_mhd_comm(mhd));
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_find_r1_r2
//
// determines r1 to be small enough so that
// +/- 2 grid points around each cell with center outside of r2
// are outside (ie., their centers) the smaller r1 sphere

static void
ggcm_mhd_bnd_sphere_map_find_r1_r2(struct ggcm_mhd_bnd_sphere_map *map,
				   double radius, double *p_r1, double *p_r2)
{
  struct ggcm_mhd *mhd = map->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  double dr = map->min_dr;
  double r2 = radius;
  double r1 = r2 - dr;

 loop2:
  // check if r1, r2 are ok
  for (int p = 0; p < mrc_domain_nr_patches(mhd->domain); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    for (int iz = 0; iz < info.ldims[2]; iz++) {
      for (int iy = 0; iy < info.ldims[1]; iy++) {
	for (int ix = 0; ix < info.ldims[0]; ix++) {
	  double xx = MRC_MCRDX(crds, ix, p);
	  double yy = MRC_MCRDY(crds, iy, p);
	  double zz = MRC_MCRDZ(crds, iz, p);
	  
	  double rr = sqrt(sqr(xx) + sqr(yy) + sqr(zz));
	  if (rr <= r2) continue;
	  
	  for (int jz = -2; jz <= 2; jz++) {
	    for (int jy = -2; jy <= 2; jy++) {
	      for (int jx = -2; jx <= 2; jx++) {
		double xxx = MRC_MCRDX(crds, ix+jx, p);
		double yyy = MRC_MCRDY(crds, iy+jy, p);
		double zzz = MRC_MCRDZ(crds, iz+jz, p);
		double rrr = sqrt(sqr(xxx) + sqr(yyy) + sqr(zzz));
		if (rrr < r1) {
		  r1 -= .01; // FIXME, hardcoded number
		  goto loop2;
		}
	      }
	    }
	  }
	}
      }
    }
  }

  MPI_Allreduce(&r1, p_r1, 1, MPI_DOUBLE, MPI_MIN, ggcm_mhd_comm(mhd));

  *p_r2 = r2;
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_setup

void
ggcm_mhd_bnd_sphere_map_setup(struct ggcm_mhd_bnd_sphere_map *map, struct ggcm_mhd *mhd,
			      double radius)
{
  map->mhd = mhd;
  ggcm_mhd_bnd_sphere_map_find_dr(map, &map->min_dr);
  ggcm_mhd_bnd_sphere_map_find_r1_r2(map, radius, &map->r1, &map->r2);
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_find_cc_n_map

static void
ggcm_mhd_bnd_sphere_map_find_cc_n_map(struct ggcm_mhd_bnd_sphere_map *map)
{
  struct ggcm_mhd *mhd = map->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  double r1 = map->r1, r2 = map->r2;
  assert(r1 > 0.);

  int cc_n_map = 0;
  for (int p = 0; p < mrc_fld_nr_patches(mhd->fld); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    int gdims[3];
    mrc_domain_get_global_dims(mhd->domain, gdims);
    // cell-centered
    int sw[3] = { 2, 2, 2 };
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	sw[d] = 0;
      }
    }
    for (int jz = -sw[2]; jz < info.ldims[2] + sw[2]; jz++) {
      for (int jy = -sw[1]; jy < info.ldims[1] + sw[1]; jy++) {
	for (int jx = -sw[0]; jx < info.ldims[0] + sw[0]; jx++) {
	  float xx = MRC_MCRDX(crds, jx, p);
	  float yy = MRC_MCRDY(crds, jy, p);
	  float zz = MRC_MCRDZ(crds, jz, p);
	  float rr = sqrtf(sqr(xx) + sqr(yy) + sqr(zz));
	  if (rr < r1 || rr > r2) continue;
	  cc_n_map++;
	}
      }
    }
  }
  map->cc_n_map = cc_n_map;
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_find_ec_n_map

static void
ggcm_mhd_bnd_sphere_map_find_ec_n_map(struct ggcm_mhd_bnd_sphere_map *map)
{
  struct ggcm_mhd *mhd = map->mhd;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);

  float r1 = map->r1, r2 = map->r2;
  int ec_n_map[3] = {};
  for (int p = 0; p < mrc_fld_nr_patches(mhd->fld); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);

    // edge-centered
    int l[3] = { 2, 2, 2 }, r[3] = { 1, 1, 1 };
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	l[d] = 0;
	r[d] = 0;
      }
    }
    for (int jz = -l[2]; jz < info.ldims[2] + r[2]; jz++) {
      for (int jy = -l[1]; jy < info.ldims[1] + r[1]; jy++) {
	for (int jx = -l[0]; jx < info.ldims[0] + r[0]; jx++) {
	  for (int d = 0; d < 3; d++) {
	    // find the correct edge centered coords for the locations of E
	    float crd_ec[3];
	    ggcm_mhd_get_crds_ec(mhd, jx,jy,jz, p, d, crd_ec);
	    float xx = crd_ec[0], yy = crd_ec[1], zz = crd_ec[2];
	    float rr = sqrtf(sqr(xx) + sqr(yy) + sqr(zz));
	    if (rr < r1 || rr > r2) continue;

	    ec_n_map[d]++;
	  }
	}
      }
    }
  }

  for (int d = 0; d < 3; d++) {
    map->ec_n_map[d] = ec_n_map[d];
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_setup_flds

void
ggcm_mhd_bnd_sphere_map_setup_flds(struct ggcm_mhd_bnd_sphere_map *map)
{
  // cell-centered
  ggcm_mhd_bnd_sphere_map_find_cc_n_map(map);
  mrc_fld_set_type(map->cc_imap, "int");
  mrc_fld_set_param_int_array(map->cc_imap, "dims", 2, (int[2]) { 4, map->cc_n_map });

  // edge-centered
  ggcm_mhd_bnd_sphere_map_find_ec_n_map(map);
  for (int d = 0; d < 3; d++) {
    mrc_fld_set_type(map->ec_imap[d], "int");
    mrc_fld_set_param_int_array(map->ec_imap[d], "dims", 2, (int[2]) { 4, map->ec_n_map[d] });
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_map_setup_cc

void
ggcm_mhd_bnd_sphere_map_setup_cc(struct ggcm_mhd_bnd_sphere_map *map)
{
  struct ggcm_mhd *mhd = map->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  double r1 = map->r1, r2 = map->r2;

  // compute e-field mapping coefficients

  int cc_n_map = 0;
  for (int p = 0; p < mrc_fld_nr_patches(mhd->fld); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    int gdims[3];
    mrc_domain_get_global_dims(mhd->domain, gdims);
    // cell-centered
    int sw[3] = { 2, 2, 2 };
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	sw[d] = 0;
      }
    }
    for (int jz = -sw[2]; jz < info.ldims[2] + sw[2]; jz++) {
      for (int jy = -sw[1]; jy < info.ldims[1] + sw[1]; jy++) {
	for (int jx = -sw[0]; jx < info.ldims[0] + sw[0]; jx++) {
	  double xx = MRC_MCRDX(crds, jx, p);
	  double yy = MRC_MCRDY(crds, jy, p);
	  double zz = MRC_MCRDZ(crds, jz, p);
	  double rr = sqrtf(sqr(xx) + sqr(yy) + sqr(zz));
	  if (rr < r1 || rr > r2) continue;
	  
	  MRC_I2(map->cc_imap, 0, cc_n_map) = jx;
	  MRC_I2(map->cc_imap, 1, cc_n_map) = jy;
	  MRC_I2(map->cc_imap, 2, cc_n_map) = jz;
	  MRC_I2(map->cc_imap, 3, cc_n_map) = p;

	  cc_n_map++;
	}
      }
    }
  }

  assert(cc_n_map == map->cc_n_map);
}


