
#include "ggcm_mhd_crds.h"
#include "mhd_util.h"

#include <mrc_ddc.h>

// ----------------------------------------------------------------------
// compute_B_cc
//
// cell-averaged B

static void __unused
compute_B_cc(struct mrc_fld *B_cc, struct mrc_fld *x, int l, int r)
{
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    mrc_fld_foreach(x, i,j,k, l, r) {
      M3(B_cc, 0, i,j,k, p) = .5f * (BX_(x, i,j,k, p) + BX_(x, i+dx,j,k, p));
      M3(B_cc, 1, i,j,k, p) = .5f * (BY_(x, i,j,k, p) + BY_(x, i,j+dy,k, p));
      M3(B_cc, 2, i,j,k, p) = .5f * (BZ_(x, i,j,k, p) + BZ_(x, i,j,k+dz, p));
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// correct_E

// FIXME
void mrc_domain_get_neighbor_patch_same(struct mrc_domain *domain, int p,
					int dx[3], int *p_nei);
void mrc_domain_get_neighbor_patch_fine(struct mrc_domain *domain, int gp,
					int dir[3], int off[3], int *gp_nei);

void mrc_domain_find_valid_point_fine(struct mrc_domain *domain, int ext[3], int gp, int i[3],
				      int *gp_nei, int j[3]);

static void
correct_E(struct ggcm_mhd *mhd, struct mrc_fld *E, int l, int r)
{
  mrc_ddc_amr_apply(mhd->ddc_amr_E, E);

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  int ext[3] = { 1, 1, 0 };

  for (int p = 0; p < mrc_fld_nr_patches(E); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    int gp = info.global_patch, *ldims = info.ldims;

    // 2D EZ only for now...
    for (int iy = 0; iy <= ldims[1]; iy++) {
      for (int ix = 0; ix <= ldims[0]; ix++) {
	if (ix > 0 && ix < ldims[0] &&
	    iy > 0 && iy < ldims[1]) {
	  // skip interior
	  continue;
	}

	// just the boundary is left

	// let's see whether there's a fine point at the same location that we'll use to replace
	// the current value
	int i[3] = { ix, iy, 0 };
	int gp_nei, j[3];
	mrc_domain_find_valid_point_fine(mhd->domain, ext, gp, (int[]) { 2*i[0], 2*i[1], 2*i[2] }, &gp_nei, j);
	if (gp_nei >= 0) {
	  /* mprintf("EEE gp %d i %d:%d gp_nei %d j %d %d\n", gp, i[0],i[1], */
	  /* 	  gp_nei, j[0], j[1]); */
	  M3(E, 2, i[0],i[1],i[2], gp) = M3(E, 2, j[0],j[1],j[2], gp_nei);
	}
      }
    }
  }

#if 1
  for (int p = 0; p < mrc_fld_nr_patches(E); p++) {
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	continue;
      }

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(mhd->domain, p, &info);
      int gp = info.global_patch, *ldims = info.ldims;

      // low side
      int gp_nei;
      int dir[3] = {};
      dir[d] = -1;
      // if there's a neighbor patch at the same refinement level,
      // the E fields better be equal already, this is for debugging / making sure
      mrc_domain_get_neighbor_patch_same(mhd->domain, gp, dir, &gp_nei);

      if (gp_nei >= 0) {
	//	mprintf("gp %d d %d gp_nei %d\n", gp, d, gp_nei);
	int p_nei = gp_nei; // FIXME, serial only
	if (d == 0) {
	  for (int iz = 0; iz < 1; iz++) {
	    for (int iy = 0; iy <= ldims[1]; iy++) {
	      if (M3(E, 2, 0,iy,iz, p) != M3(E, 2, ldims[0],iy,iz, p_nei)) {
		mprintf("!!! gp %d EZ[0,%d,%d] = %g // gp_nei %d [%d:%d:%d] %g\n",
			gp, iy, iz, M3(E, 2, 0,iy,iz, p),
			p_nei, ldims[0],iy,iz, M3(E, 2, ldims[0],iy,iz, p_nei));
	      }
	    }
	  }
	}
      }
    }
  }
#endif

  for (int p = 0; p < mrc_fld_nr_patches(E); p++) {
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	continue;
      }

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(mhd->domain, p, &info);
      int gp = info.global_patch, *ldims = info.ldims;

      // low side
      int gp_nei;
      int dir[3] = {}, off[3] = {};

#if 0
      dir[d] = -1;
      off[d] = 1; 
      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);

      if (gp_nei >= 0) {
	if (d == 0) {
	  int offy = (gdims[1] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[1] = 0; off[1] <= offy; off[1]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int iy = 0; iy <= ldims[1] / 2; iy++) {
		  int ix = 0, ix_nei = ldims[0];
		  int p_nei = gp_nei; // FIXME, serial
		  mrc_fld_data_t val = M3(E, 2, ix_nei,iy*2,iz*2, p_nei);
		  M3(E, 2, ix,iy + ldims[1]/2 * off[1],iz + ldims[2]/2 * off[2], p) = val;
		}
	      }
	    }
	  }
	} else if (d == 1) {
	  //mprintf("low gp %d d %d gp_nei %d\n", gp, d, gp_nei);
	  int offx = (gdims[0] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[0] = 0; off[0] <= offx; off[0]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int ix = 0; ix <= ldims[0] / 2; ix++) {
		  int iy = 0, iy_nei = ldims[1];
		  int p_nei = gp_nei; // FIXME, serial
		  mrc_fld_data_t val = M3(E, 2, ix*2,iy_nei,iz*2, p_nei);
		  /* mprintf("EZ[%d,%d,%d] = %g // gp_nei %d, EZ[%d,%d,%d] = %g\n", ix + ldims[0]/2 * off[0], iy, iz, */
		  /* 	  M3(E, 2, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p), */
		  /* 	  p_nei, ix*2, iy_nei, iz*2, val); */
		  M3(E, 2, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p) = val;
		}
	      }
	    }
	  }
	}
      }
#endif

#if 0
      // high side
      dir[d] = 1;
      off[d] = 0;
      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);

      if (gp_nei >= 0) {
	if (d == 0) {
	  int offy = (gdims[1] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[1] = 0; off[1] <= offy; off[1]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int iy = 0; iy <= ldims[1] / 2; iy++) {
		  int ix = ldims[0], ix_nei = 0;
		  int p_nei = gp_nei; // FIXME, serial
		  mrc_fld_data_t val = M3(E, 2, ix_nei,iy*2,iz*2, p_nei);
		  M3(E, 2, ix,iy + ldims[1]/2 * off[1],iz + ldims[2]/2 * off[2], p) = val;
		}
	      }
	    }
	  }
	} else if (d == 1) {
	  //	  mprintf("high gp %d d %d gp_nei %d\n", gp, d, gp_nei);
	  int offx = (gdims[0] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[0] = 0; off[0] <= offx; off[0]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int ix = 0; ix <= ldims[0] / 2; ix++) {
		  int iy = ldims[1], iy_nei = 0;
		  int p_nei = gp_nei; // FIXME, serial
		  mrc_fld_data_t val = M3(E, 2, ix*2,iy_nei,iz*2, p_nei);
		  M3(E, 2, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p) = val;
		}
	      }
	    }
	  }
	}
      }
#endif
    }
  }  
}

// ----------------------------------------------------------------------
// update_ct_uniform

static void __unused
update_ct_uniform(struct ggcm_mhd *mhd,
		  struct mrc_fld *x, struct mrc_fld *E, mrc_fld_data_t dt, int _l, int _r,
		  bool do_correct)
{
  if (mhd->amr > 0 && do_correct) {
    correct_E(mhd, E, _l, _r);
  }

  int gdims[3]; mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);
  int l[3], r[3];
  for (int d = 0; d < 3; d++) {
    l[d] = (gdims[d] > 1) ? _l : 0;
    r[d] = (gdims[d] > 1) ? _r : 0;
  }

  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  const int *ldims = mrc_fld_spatial_dims(x);

  // FIXME, works with gdims[2] == 1, but not with other invar directions

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    double ddx[3]; mrc_crds_get_dx(crds, p, ddx);
    mrc_fld_data_t dt_on_dx[3] = { dt / ddx[0], dt / ddx[1], dt / ddx[2] };

    for (int k = -l[2]; k < ldims[2] + r[2]; k++) {
      for (int j = -l[1]; j < ldims[1] + r[1]; j++) {
	for (int i = -l[0]; i < ldims[0] + r[0]; i++) {
	  M3(x, BX, i,j,k, p) += (dt_on_dx[2] * (M3(E, 1, i   ,j   ,k+dz, p) - M3(E, 1, i,j,k, p)) -
				  dt_on_dx[1] * (M3(E, 2, i   ,j+dy,k   , p) - M3(E, 2, i,j,k, p)));
	  M3(x, BY, i,j,k, p) += (dt_on_dx[0] * (M3(E, 2, i+dx,j   ,k   , p) - M3(E, 2, i,j,k, p)) -
				  dt_on_dx[2] * (M3(E, 0, i   ,j   ,k+dz, p) - M3(E, 0, i,j,k, p)));
	  M3(x, BZ, i,j,k, p) += (dt_on_dx[1] * (M3(E, 0, i   ,j+dy,k   , p) - M3(E, 0, i,j,k, p)) -
				  dt_on_dx[0] * (M3(E, 1, i+dx,j   ,k   , p) - M3(E, 1, i,j,k, p)));
	}
	int i = ldims[0] + r[0];
	M3(x, BX, i,j,k, p) += (dt_on_dx[2] * (M3(E, 1, i   ,j   ,k+dz, p) - M3(E, 1, i,j,k, p)) -
				dt_on_dx[1] * (M3(E, 2, i   ,j+dy,k   , p) - M3(E, 2, i,j,k, p)));
      }
      for (int i = -l[0]; i < ldims[0] + r[0]; i++) {
	int j = ldims[1] + r[1];
	M3(x, BY, i,j,k, p) += (dt_on_dx[0] * (M3(E, 2, i+dx,j   ,k   , p) - M3(E, 2, i,j,k, p)) -
				dt_on_dx[2] * (M3(E, 0, i   ,j   ,k+dz, p) - M3(E, 0, i,j,k, p)));
      }
    }
    if (gdims[2] > 1) {
      for (int j = -l[1]; j < ldims[1] + r[1]; j++) {
	for (int i = -l[0]; i < ldims[0] + r[0]; i++) {
	  int k = ldims[2] + r[2];
	  M3(x, BZ, i,j,k, p) += (dt_on_dx[1] * (M3(E, 0, i   ,j+dy,k   , p) - M3(E, 0, i,j,k, p)) -
				  dt_on_dx[0] * (M3(E, 1, i+dx,j   ,k   , p) - M3(E, 1, i,j,k, p)));
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// update_ct

static void __unused
update_ct(struct ggcm_mhd *mhd,
	  struct mrc_fld *x, struct mrc_fld *E, mrc_fld_data_t dt)
{
  int gdims[3]; mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    float *bd3x = ggcm_mhd_crds_get_crd_p(mhd->crds, 0, BD3, p);
    float *bd3y = ggcm_mhd_crds_get_crd_p(mhd->crds, 1, BD3, p);
    float *bd3z = ggcm_mhd_crds_get_crd_p(mhd->crds, 2, BD3, p);

    mrc_fld_foreach(x, i,j,k, 0, 0) {
      M3(x, BX, i,j,k, p) -= dt * (bd3y[j] * (M3(E, 2, i,j+dy,k, p) - M3(E, 2, i,j,k, p)) -
				   bd3z[k] * (M3(E, 1, i,j,k+dz, p) - M3(E, 1, i,j,k, p)));
      M3(x, BY, i,j,k, p) -= dt * (bd3z[k] * (M3(E, 0, i,j,k+dz, p) - M3(E, 0, i,j,k, p)) -
				   bd3x[i] * (M3(E, 2, i+dx,j,k, p) - M3(E, 2, i,j,k, p)));
      M3(x, BZ, i,j,k, p) -= dt * (bd3x[i] * (M3(E, 1, i+dx,j,k, p) - M3(E, 1, i,j,k, p)) -
				   bd3y[j] * (M3(E, 0, i,j+dy,k, p) - M3(E, 0, i,j,k, p)));
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// mhd_fluxes

static void __unused
mhd_fluxes(struct ggcm_mhd_step *step, struct mrc_fld *fluxes[3], struct mrc_fld *x,
	   struct mrc_fld *B_cc, int bn, int nghost,
	   void (*flux_func)(struct ggcm_mhd_step *step, struct mrc_fld *fluxes[3],
			     struct mrc_fld *x, struct mrc_fld *B_cc,
			     int ldim, int bnd, int j, int k, int dir, int p))
{
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);

  int bnd[3];
  for (int d = 0; d < 3; d++) {
    if (gdims[d] == 1) {
      bnd[d] = 0;
    } else {
      bnd[d] = bn;
    }
  }

  const int *ldims = mrc_fld_spatial_dims(x);

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    if (gdims[0] > 1) {
      for (int k = -bnd[2]; k < ldims[2] + bnd[2]; k++) {
	for (int j = -bnd[1]; j < ldims[1] + bnd[1]; j++) {
	  flux_func(step, fluxes, x, B_cc, ldims[0], nghost, j, k, 0, p);
	}
      }
    }

    if (gdims[1] > 1) {
      for (int k = -bnd[2]; k < ldims[2] + bnd[2]; k++) {
	for (int i = -bnd[0]; i < ldims[0] + bnd[0]; i++) {
	  flux_func(step, fluxes, x, B_cc, ldims[1], nghost, k, i, 1, p);
	}
      }
    }

    if (gdims[2] > 1) {
      for (int j = -bnd[1]; j < ldims[1] + bnd[1]; j++) {
	for (int i = -bnd[0]; i < ldims[0] + bnd[0]; i++) {
	  flux_func(step, fluxes, x, B_cc, ldims[2], nghost, i, j, 2, p);
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// correct_fluxes

// FIXME
void mrc_domain_get_neighbor_patch_same(struct mrc_domain *domain, int p,
					int dx[3], int *p_nei);
void mrc_domain_get_neighbor_patch_fine(struct mrc_domain *domain, int gp,
					int dir[3], int off[3], int *gp_nei);

static void
correct_fluxes(struct ggcm_mhd *mhd, struct mrc_fld *fluxes[3])
{
  mrc_ddc_amr_apply(mhd->ddc_amr_flux_x, fluxes[0]);
  mrc_ddc_amr_apply(mhd->ddc_amr_flux_y, fluxes[1]);
  return;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);

  int nr_patches = mrc_fld_nr_patches(fluxes[0]);
  for (int p = 0; p < nr_patches; p++) {
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	continue;
      }

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(mhd->domain, p, &info);
      int gp = info.global_patch, *ldims = info.ldims;

      // low side
      int gp_nei;
      int dir[3] = {}, off[3] = {};
      dir[d] = -1;
#if 0
      // if there's a neighbor patch at the same refinement level,
      // the fluxes better be equal already, this for debugging / making sure
      mrc_domain_get_neighbor_patch_same(mhd->domain, gp, dir, &gp_nei);

      if (gp_nei >= 0) {
	mprintf("gp %d d %d gp_nei %d\n", gp, d, gp_nei);
      int p_nei = gp_nei; // FIXME, serial only
	if (d == 1) {
	  for (int iz = 0; iz < ldims[2]; iz++) {
	    for (int ix = 0; ix < ldims[0]; ix++) {
	      mprintf("flux[1][%d,0,%d] = %g // %g\n", ix, iz,
		      M3(fluxes[d], 0, ix,0,iz, p),
		      M3(fluxes[d], 0, ix,ldims[1],iz, p_nei));
	    }
	  }
	}
      }
#endif

      off[d] = 1; // for low side
      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);

      if (gp_nei >= 0) {
	if (d == 1) {
	  //	  mprintf("low gp %d d %d gp_nei %d\n", gp, d, gp_nei);
	  int offx = (gdims[0] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[0] = 0; off[0] <= offx; off[0]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int ix = 0; ix < ldims[0] / 2; ix++) {
		  int iy = 0, iy_nei = ldims[1];
		  int p_nei = gp_nei; // FIXME, serial
		  for (int m = 0; m < 5; m++) {
		    mrc_fld_data_t val =
		      .5f * (M3(fluxes[d], m, ix*2  ,iy_nei,iz*2, p_nei) +
			     M3(fluxes[d], m, ix*2+1,iy_nei,iz*2, p_nei));
		    /* mprintf("flux[1][%d,%d,%d] = %g // %g\n", ix, iy, iz, */
		    /* 	M3(fluxes[d], m, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p), */
		    /* 	val); */
		    M3(fluxes[d], m, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p) = val;
		  }
		}
	      }
	    }
	  }
	}
      }

      // high side
      dir[d] = 1;
      off[d] = 0; // for high side
      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);

      if (gp_nei >= 0) {
	if (d == 1) {
	  //	  mprintf("high gp %d d %d gp_nei %d\n", gp, d, gp_nei);
	  int offx = (gdims[0] > 1), offz = (gdims[2] > 1);
	  for (off[2] = 0; off[2] <= offz; off[2]++) {
	    for (off[0] = 0; off[0] <= offx; off[0]++) {
	      mrc_domain_get_neighbor_patch_fine(mhd->domain, gp, dir, off, &gp_nei);
	      for (int iz = 0; iz < 1; iz++) {
		for (int ix = 0; ix < ldims[0] / 2; ix++) {
		  int iy = ldims[1], iy_nei = 0;
		  int p_nei = gp_nei; // FIXME, serial
		  for (int m = 0; m < 5; m++) {
		    mrc_fld_data_t val =
		      .5f * (M3(fluxes[d], m, ix*2  ,iy_nei,iz*2, p_nei) +
			     M3(fluxes[d], m, ix*2+1,iy_nei,iz*2, p_nei));
		    if (m == 0) {
		      mprintf("flux[1] gp %d [%d,%d,%d] = %g // %g\n", gp, ix, iy, iz,
			      M3(fluxes[d], m, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p),
			      val);
		    }
		    M3(fluxes[d], m, ix + ldims[0]/2 * off[0],iy,iz + ldims[2]/2 * off[2], p) = val;
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }  
}

// ----------------------------------------------------------------------
// update_finite_volume_uniform

static void __unused
update_finite_volume_uniform(struct ggcm_mhd *mhd,
			     struct mrc_fld *x, struct mrc_fld *fluxes[3],
			     mrc_fld_data_t dt, int l, int r, int do_correct)
{
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  if (mhd->amr > 0 && do_correct) {
    correct_fluxes(mhd, fluxes);
  }

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    double ddx[3]; mrc_crds_get_dx(crds, p, ddx);
    mrc_fld_data_t dt_on_dx[3] = { dt / ddx[0], dt / ddx[1], dt / ddx[2] };
    // FIXME, potential for accelerating the 2-d/1-d versions
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	dt_on_dx[d] = 0.f;
      }
    }

    mrc_fld_foreach(x, i,j,k, l, r) {
      for (int m = 0; m < 5; m++) {
	M3(x, m, i,j,k, p) -= 
	  (dt_on_dx[0] * (M3(fluxes[0], m, i+dx,j,k, p) - M3(fluxes[0], m, i,j,k, p)) +
	   dt_on_dx[1] * (M3(fluxes[1], m, i,j+dy,k, p) - M3(fluxes[1], m, i,j,k, p)) + 
	   dt_on_dx[2] * (M3(fluxes[2], m, i,j,k+dz, p) - M3(fluxes[2], m, i,j,k, p)));
      }
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// update_finite_volume

static void __unused
update_finite_volume(struct ggcm_mhd *mhd,
		     struct mrc_fld *x, struct mrc_fld *fluxes[3],
		     struct mrc_fld *ymask, mrc_fld_data_t dt)
{
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);


  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    float *fd1x = ggcm_mhd_crds_get_crd_p(mhd->crds, 0, FD1, p);
    float *fd1y = ggcm_mhd_crds_get_crd_p(mhd->crds, 1, FD1, p);
    float *fd1z = ggcm_mhd_crds_get_crd_p(mhd->crds, 2, FD1, p);

    mrc_fld_foreach(x, i,j,k, 0, 0) {
      mrc_fld_data_t s = dt * M3(ymask, 0, i,j,k, p);
      for (int m = 0; m < 5; m++) {
	M3(x, m, i,j,k, p) -=
	  s * (fd1x[i] * (M3(fluxes[0], m, i+dx,j,k, p) - M3(fluxes[0], m, i,j,k, p)) +
	       fd1y[j] * (M3(fluxes[1], m, i,j+dy,k, p) - M3(fluxes[1], m, i,j,k, p)) +
	       fd1z[k] * (M3(fluxes[2], m, i,j,k+dz, p) - M3(fluxes[2], m, i,j,k, p)));
      }
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// mrc_fld_copy_range

// FIXME, mv to right place
static void __unused
mrc_fld_copy_range(struct mrc_fld *to, struct mrc_fld *from, int mb, int me)
{
  assert(to->_nr_ghosts == from->_nr_ghosts);
  int bnd = to->_nr_ghosts;

  for (int p = 0; p < mrc_fld_nr_patches(to); p++) {
    mrc_fld_foreach(to, ix,iy,iz, bnd, bnd) {
      for (int m = mb; m < me; m++) {
	M3(to, m, ix,iy,iz, p) = M3(from, m, ix,iy,iz, p);
      }
    } mrc_fld_foreach_end;
  }
}

