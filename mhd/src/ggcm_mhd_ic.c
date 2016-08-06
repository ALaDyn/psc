
#include "ggcm_mhd_ic_private.h"

#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_step.h"

#include <mrc_io.h>
#include <mrc_ddc.h>
#include <mrc_fld_as_double.h>

#include <assert.h>

// ======================================================================
// ggcm_mhd_ic class

typedef double (*primitive_f)(struct ggcm_mhd_ic *ic, int m, double crd[3]);

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_vector_potential_fc
//
// initialize face-centered B from edge-centered vector potential

static mrc_fld_data_t
get_vector_potential_ec(struct ggcm_mhd_ic *ic, int m, int ix, int iy, int iz, int p)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct ggcm_mhd_ic_ops *ops = ggcm_mhd_ic_ops(ic);

  // FIXME, want double precision crds natively here
  float crd_ec[3];
  ggcm_mhd_get_crds_ec(mhd, ix,iy,iz, p, m, crd_ec);
  double dcrd_ec[3] = { crd_ec[0], crd_ec[1], crd_ec[2] };
  
  return ops->vector_potential(ic, m, dcrd_ec);
}

static void
ggcm_mhd_ic_B_from_vector_potential_fc(struct ggcm_mhd_ic *ic, struct mrc_fld *b)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  int gdims[3], p1x, p1y;
  mrc_domain_get_global_dims(mhd->domain, gdims);
  p1x = (gdims[0] > 1);
  p1y = (gdims[1] > 1);
  
  for (int p = 0; p < mrc_fld_nr_patches(b); p++) {
    double dx[3];
    mrc_crds_get_dx(crds, p, dx);
    
    /* initialize face-centered fields */
    mrc_fld_foreach(b, ix,iy,iz, 1, 2) {
      mrc_fld_data_t Az    = get_vector_potential_ec(ic, 2, ix    ,iy    ,iz, p);
      mrc_fld_data_t Az_xp = get_vector_potential_ec(ic, 2, ix+p1x,iy    ,iz, p);
      mrc_fld_data_t Az_yp = get_vector_potential_ec(ic, 2, ix    ,iy+p1y,iz, p);
      M3(b, 0, ix,iy,iz, p) +=  (Az_yp - Az) / dx[1];
      M3(b, 1, ix,iy,iz, p) += -(Az_xp - Az) / dx[0];
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_vector_potential_cc
//
// initialize face-centered B from edge-centered vector potential

static mrc_fld_data_t
get_vector_potential_cc(struct ggcm_mhd_ic *ic, int m, int ix, int iy, int iz, int p)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct ggcm_mhd_ic_ops *ops = ggcm_mhd_ic_ops(ic);

  // FIXME, want double precision crds natively here
  float crd_cc[3];
  ggcm_mhd_get_crds_cc(mhd, ix,iy,iz, p, crd_cc);
  double dcrd_cc[3] = { crd_cc[0], crd_cc[1], crd_cc[2] };
  
  return ops->vector_potential(ic, m, dcrd_cc);
}

static void
ggcm_mhd_ic_B_from_vector_potential_cc(struct ggcm_mhd_ic *ic, struct mrc_fld *b)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  int gdims[3], p1x, p1y;
  mrc_domain_get_global_dims(mhd->domain, gdims);
  p1x = (gdims[0] > 1);
  p1y = (gdims[1] > 1);
  
  for (int p = 0; p < mrc_fld_nr_patches(b); p++) {
    double dx[3];
    mrc_crds_get_dx(crds, p, dx);
    
    /* initialize face-centered fields */
    mrc_fld_foreach(b, ix,iy,iz, 1, 1) {
      mrc_fld_data_t Az_xp = get_vector_potential_cc(ic, 2, ix+p1x,iy    ,iz, p);
      mrc_fld_data_t Az_xm = get_vector_potential_cc(ic, 2, ix-p1x,iy    ,iz, p);
      mrc_fld_data_t Az_yp = get_vector_potential_cc(ic, 2, ix    ,iy+p1y,iz, p);
      mrc_fld_data_t Az_ym = get_vector_potential_cc(ic, 2, ix    ,iy-p1y,iz, p);
      M3(b, 0, ix,iy,iz, p) +=  (Az_yp - Az_ym) / (2. * dx[1]);
      M3(b, 1, ix,iy,iz, p) += -(Az_xp - Az_xm) / (2. * dx[0]);
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_vector_potential

static void
ggcm_mhd_ic_B_from_vector_potential(struct ggcm_mhd_ic *ic, struct mrc_fld *b)
{
  struct ggcm_mhd *mhd = ic->mhd;
  int mhd_type;
  mrc_fld_get_param_int(mhd->fld, "mhd_type", &mhd_type);
  
  if (mhd_type == MT_FULLY_CONSERVATIVE ||
      mhd_type == MT_SEMI_CONSERVATIVE) {
    ggcm_mhd_ic_B_from_vector_potential_fc(ic, b);
  } else if (mhd_type == MT_FULLY_CONSERVATIVE_CC) {
    ggcm_mhd_ic_B_from_vector_potential_cc(ic, b);
  } else {
    mprintf("mhd_type %d unhandled\n", mhd_type);
    assert(0);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_primitive_fc
//
// initialize face-centered B directly

static void
ggcm_mhd_ic_B_from_primitive_fc(struct ggcm_mhd_ic *ic, struct mrc_fld *b, primitive_f primitive)
{
  struct ggcm_mhd *mhd = ic->mhd;

  for (int p = 0; p < mrc_fld_nr_patches(b); p++) {
    mrc_fld_foreach(b, ix,iy,iz, 0, 0) {
      for (int m = 0; m < 3; m++) {
	float crd_fc[3];
	ggcm_mhd_get_crds_fc(mhd, ix,iy,iz, p, m, crd_fc);
	double dcrd_fc[3] = { crd_fc[0], crd_fc[1], crd_fc[2] };
	
	M3(b, m, ix,iy,iz, p) += primitive(ic, BX + m, dcrd_fc);
      }
    } mrc_fld_foreach_end;    
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_primitive_cc
//
// initialize cell-centered B directly

static void
ggcm_mhd_ic_B_from_primitive_cc(struct ggcm_mhd_ic *ic, struct mrc_fld *b, primitive_f primitive)
{
  struct ggcm_mhd *mhd = ic->mhd;

  for (int p = 0; p < mrc_fld_nr_patches(b); p++) {
    mrc_fld_foreach(b, ix,iy,iz, 0, 0) {
      float crd_cc[3];
      ggcm_mhd_get_crds_cc(mhd, ix,iy,iz, p, crd_cc);
      double dcrd_cc[3] = { crd_cc[0], crd_cc[1], crd_cc[2] };
	
      for (int m = 0; m < 3; m++) {
	M3(b, m, ix,iy,iz, p) += primitive(ic, BX + m, dcrd_cc);
      }
    } mrc_fld_foreach_end;    
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_B_from_primitive

static void
ggcm_mhd_ic_B_from_primitive(struct ggcm_mhd_ic *ic, struct mrc_fld *b, primitive_f primitive)
{
  struct ggcm_mhd *mhd = ic->mhd;
  int mhd_type;
  mrc_fld_get_param_int(mhd->fld, "mhd_type", &mhd_type);
  
  if (mhd_type == MT_FULLY_CONSERVATIVE ||
      mhd_type == MT_SEMI_CONSERVATIVE) {
    ggcm_mhd_ic_B_from_primitive_fc(ic, b, primitive);
  } else if (mhd_type == MT_FULLY_CONSERVATIVE_CC) {
    ggcm_mhd_ic_B_from_primitive_cc(ic, b, primitive);
  } else {
    mprintf("mhd_type %d unhandled\n", mhd_type);
    assert(0);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_hydro_from_primitive_semi
// 
// init semi conservative hydro state

static void
ggcm_mhd_ic_hydro_from_primitive_semi(struct ggcm_mhd_ic *ic, struct mrc_fld *fld)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct ggcm_mhd_ic_ops *ops = ggcm_mhd_ic_ops(ic);

  mrc_fld_data_t gamma_m1 = mhd->par.gamm - 1.;

  for (int p = 0; p < mrc_fld_nr_patches(fld); p++) {
    mrc_fld_foreach(fld, ix,iy,iz, 0, 0) {
      float crd_cc[3];
      ggcm_mhd_get_crds_cc(mhd, ix,iy,iz, p, crd_cc);
      double dcrd_cc[3] = { crd_cc[0], crd_cc[1], crd_cc[2] };
      
      mrc_fld_data_t prim[5];
      for (int m = 0; m < 5; m++) {
	prim[m] = ops->primitive(ic, m, dcrd_cc);
      }
      
      RR_ (fld, ix,iy,iz, p) = prim[RR];
      RVX_(fld, ix,iy,iz, p) = prim[RR] * prim[VX];
      RVY_(fld, ix,iy,iz, p) = prim[RR] * prim[VY];
      RVZ_(fld, ix,iy,iz, p) = prim[RR] * prim[VZ];
      UU_ (fld, ix,iy,iz, p) = prim[PP] / gamma_m1
	+ .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ]));
    } mrc_fld_foreach_end;    
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_hydro_from_primitive_fully_cc
//
// init fully conservative hydro state assuming that B is cell centered

static void
ggcm_mhd_ic_hydro_from_primitive_fully_cc(struct ggcm_mhd_ic *ic, struct mrc_fld *fld)
{
  struct ggcm_mhd *mhd = ic->mhd;
  struct ggcm_mhd_ic_ops *ops = ggcm_mhd_ic_ops(ic);

  mrc_fld_data_t gamma_m1 = mhd->par.gamm - 1.;

  for (int p = 0; p < mrc_fld_nr_patches(fld); p++) {
    mrc_fld_foreach(fld, ix,iy,iz, 0, 0) {
      float crd_cc[3];
      ggcm_mhd_get_crds_cc(mhd, ix,iy,iz, p, crd_cc);
      double dcrd_cc[3] = { crd_cc[0], crd_cc[1], crd_cc[2] };
      
      mrc_fld_data_t prim[5];
      for (int m = 0; m < 5; m++) {
	prim[m] = ops->primitive(ic, m, dcrd_cc);
      }
      
      RR_ (fld, ix,iy,iz, p) = prim[RR];
      RVX_(fld, ix,iy,iz, p) = prim[RR] * prim[VX];
      RVY_(fld, ix,iy,iz, p) = prim[RR] * prim[VY];
      RVZ_(fld, ix,iy,iz, p) = prim[RR] * prim[VZ];
      EE_ (fld, ix,iy,iz, p) = prim[PP] / gamma_m1
	+ .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ]))
	+ .5f * (sqr(BX_(fld, ix,iy,iz, p)) +
		 sqr(BY_(fld, ix,iy,iz, p)) +
		 sqr(BZ_(fld, ix,iy,iz, p)));
    } mrc_fld_foreach_end;    
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_hydro_from_primitive

static void
ggcm_mhd_ic_hydro_from_primitive(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd *mhd = ic->mhd;
  int mhd_type;
  mrc_fld_get_param_int(mhd->fld, "mhd_type", &mhd_type);

  struct mrc_fld *fld = mrc_fld_get_as(mhd->fld, FLD_TYPE);

  if (mhd_type == MT_SEMI_CONSERVATIVE) {
    ggcm_mhd_ic_hydro_from_primitive_semi(ic, fld);
  } else if (mhd_type == MT_FULLY_CONSERVATIVE_CC) {
    ggcm_mhd_ic_hydro_from_primitive_fully_cc(ic, fld);
  } else {
    assert(0);
  }
  
  mrc_fld_put_as(fld, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_run

void
ggcm_mhd_ic_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd *mhd = ic->mhd;
  assert(mhd);
  struct ggcm_mhd_ic_ops *ops = ggcm_mhd_ic_ops(ic);

  if (ops->init_b0) {
    mhd->b0 = ggcm_mhd_get_3d_fld(mhd, 3);
    ops->init_b0(ic, mhd->b0);
    // FIXME, this doesn't set B values in exterior ghost points
    mrc_ddc_fill_ghosts_fld(mrc_domain_get_ddc(mhd->domain), 0, 3, mhd->b0);
  }

  struct mrc_fld *fld = mrc_fld_get_as(mhd->fld, FLD_TYPE);
  struct mrc_fld *b = mrc_fld_make_view(fld, BX, BX + 3);

  /* initialize background magnetic field */
  if (ggcm_mhd_step_supports_b0(mhd->step)) {
    assert(0);
  } else {
    if (ops->primitive_bg) {
      ggcm_mhd_ic_B_from_primitive(ic, b, ops->primitive_bg);
    }
  }

  /* initialize magnetic field */
  if (ops->vector_potential) {
    ggcm_mhd_ic_B_from_vector_potential(ic, b);
  } else if (ops->primitive) {
    ggcm_mhd_ic_B_from_primitive(ic, b, ops->primitive);
  }

  mrc_fld_destroy(b);
  mrc_fld_put_as(fld, mhd->fld);

  /* initialize density, velocity, pressure, or corresponding
     conservative quantities */
  if (ops->primitive) {
    ggcm_mhd_ic_hydro_from_primitive(ic);
  }

  if (ops->run) {
    ops->run(ic);
  }

  if (ops->init_b0) {
    if (!ggcm_mhd_step_supports_b0(mhd->step)) {
      // if the stepper doesn't support a separate b0, 
      // add b0 into b, destroy b0 again.
      struct mrc_fld *b0 = mrc_fld_get_as(mhd->b0, FLD_TYPE);
      struct mrc_fld *fld = mrc_fld_get_as(mhd->fld, FLD_TYPE);
      
      // FIXME, could use some axpy kinda thing
      for (int p = 0; p < mrc_fld_nr_patches(fld); p++) {
	mrc_fld_foreach(fld, ix,iy,iz, 2, 2) {
	  for (int d = 0; d < 3; d++) {
	    M3(fld, BX+d, ix,iy,iz, p) += M3(b0, d, ix,iy,iz, p);
	  }
	} mrc_fld_foreach_end;
      }
      
      mrc_fld_put_as(b0, mhd->b0);
      mrc_fld_put_as(fld, mhd->fld);
      
      ggcm_mhd_put_3d_fld(mhd, mhd->b0);
      mhd->b0 = NULL;
    }
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_init

static void
ggcm_mhd_ic_init()
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_mirdip_float_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_mirdip_double_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_obstacle_double_ops);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic, x)
static struct param ggcm_mhd_ic_descr[] = {
  { "mhd"             , VAR(mhd)             , PARAM_OBJ(ggcm_mhd)      },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic class description

struct mrc_class_ggcm_mhd_ic mrc_class_ggcm_mhd_ic = {
  .name             = "ggcm_mhd_ic",
  .size             = sizeof(struct ggcm_mhd_ic),
  .param_descr      = ggcm_mhd_ic_descr,
  .init             = ggcm_mhd_ic_init,
};

