
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_crds.h"
#include "ggcm_mhd_gkeyll.h"

#include <mrc_domain.h>
#include <mrc_fld_as_double.h>

#include <math.h>
#include <string.h>

// ----------------------------------------------------------------------
// ggcm_mhd_calc_curcc_gkeyll

static void
ggcm_mhd_calc_currcc_gkeyll(struct ggcm_mhd *mhd, struct mrc_fld *f, int m,
			    struct mrc_fld *c)
{
  int nr_fluids = mhd->par.gk_nr_fluids;
  int nr_moments = mhd->par.gk_nr_moments;
  float *mass = mhd->par.gk_mass.vals;
  float *charge = mhd->par.gk_charge.vals;

  float q_m[nr_fluids];
  for (int s = 0; s < nr_fluids; s++)
    q_m[s] = charge[s] / mass[s];

  assert(nr_moments == 5);
  int idx[nr_fluids];
  ggcm_mhd_gkeyll_fluid_species_index_all(mhd, idx);

  for (int p = 0; p < mrc_fld_nr_patches(f); p++) {
    mrc_fld_foreach(f, ix,iy,iz, 0, 0) {
      for (int d = 0; d < 3; d++)
        M3(c, d, ix,iy,iz, p) = 0.;
      for (int s = 0; s < nr_fluids; s++) {
        M3(c, 0, ix,iy,iz, p) += M3(f, idx[s]+G5M_RVXS, ix,iy,iz, p) * q_m[s];
        M3(c, 1, ix,iy,iz, p) += M3(f, idx[s]+G5M_RVYS, ix,iy,iz, p) * q_m[s];
        M3(c, 2, ix,iy,iz, p) += M3(f, idx[s]+G5M_RVZS, ix,iy,iz, p) * q_m[s];
      }
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_calc_currcc

void
ggcm_mhd_calc_currcc(struct ggcm_mhd *mhd, struct mrc_fld *_fld, int m,
		     struct mrc_fld *_currcc)
{
  int mhd_type;
  mrc_fld_get_param_int(_fld, "mhd_type", &mhd_type);

  struct mrc_fld *fld = mrc_fld_get_as(_fld, FLD_TYPE);
  struct mrc_fld *currcc = mrc_fld_get_as(_currcc, FLD_TYPE);
  
  if (MT_FORMULATION(mhd_type) == MT_FORMULATION_GKEYLL) {
    ggcm_mhd_calc_currcc_gkeyll(mhd, fld, m, currcc);
  } else { // MHD
    switch (MT_BGRID(mhd_type)) {
    case MT_BGRID_CC:
      ggcm_mhd_calc_currcc_bgrid_cc(mhd, fld, m, currcc);
      break;
    case MT_BGRID_FC:
      ggcm_mhd_calc_currcc_bgrid_fc(mhd, fld, m, currcc);
      break;
    case MT_BGRID_FC_GGCM:
      ggcm_mhd_calc_currcc_bgrid_fc_ggcm(mhd, fld, m, currcc);
      break;
    default:
      assert(0);
    }
  }

  mrc_fld_put_as(fld, _fld);
  mrc_fld_put_as(currcc, _currcc);
}
