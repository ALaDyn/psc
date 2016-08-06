
#include <mrc_fld_as_double.h>

#define ggcm_mhd_step_mhdcc_ops ggcm_mhd_step_mhdcc_double_ops
#define ggcm_mhd_step_mhdcc_name "mhdcc_double"

#include "ggcm_mhd_step_private.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_crds.h"
#include "ggcm_mhd_diag_private.h"
#include "mhd_util.h"

#include <mrc_domain.h>
#include <mrc_profile.h>
#include <mrc_io.h>

#include <math.h>
#include <string.h>

#include "pde/pde_defs.h"

#define OPT_EQN OPT_EQN_MHD_FCONS

#include "pde/pde_setup.c"
#include "pde/pde_mhd_setup.c"
#include "pde/pde_mhd_line.c"
#include "pde/pde_mhd_convert.c"
#include "pde/pde_mhd_reconstruct.c"
#include "pde/pde_mhd_riemann.c"

#include "mhd_3d.c"
#include "mhd_sc.c"

// ======================================================================
// ggcm_mhd_step subclass "mhdcc"

struct ggcm_mhd_step_mhdcc {
  struct mhd_options opt;

  fld1d_state_t U;
  fld1d_state_t U_l;
  fld1d_state_t U_r;
  fld1d_state_t W;
  fld1d_state_t W_l;
  fld1d_state_t W_r;
  fld1d_state_t F;

  struct mrc_fld *x_star;
};

#define ggcm_mhd_step_mhdcc(step) mrc_to_subobj(step, struct ggcm_mhd_step_mhdcc)

// TODO:
// - handle various resistivity models

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_setup

static void
ggcm_mhd_step_mhdcc_setup(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;

  assert(mhd);

  pde_setup(mhd->fld);
  pde_mhd_setup(mhd);

  fld1d_state_setup(&sub->U);
  fld1d_state_setup(&sub->U_l);
  fld1d_state_setup(&sub->U_r);
  fld1d_state_setup(&sub->W);
  fld1d_state_setup(&sub->W_l);
  fld1d_state_setup(&sub->W_r);
  fld1d_state_setup(&sub->F);

  mhd->ymask = ggcm_mhd_get_3d_fld(mhd, 1);
  mrc_fld_set(mhd->ymask, 1.);

  sub->x_star = ggcm_mhd_get_3d_fld(mhd, s_n_comps);
  mrc_fld_dict_add_int(sub->x_star, "mhd_type", MT_FULLY_CONSERVATIVE_CC);

  ggcm_mhd_step_setup_member_objs_sub(step);
  ggcm_mhd_step_setup_super(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_destroy

static void
ggcm_mhd_step_mhdcc_destroy(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;

  ggcm_mhd_put_3d_fld(mhd, mhd->ymask);
  ggcm_mhd_put_3d_fld(mhd, sub->x_star);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_setup_flds

static void
ggcm_mhd_step_mhdcc_setup_flds(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;

  pde_mhd_set_options(mhd, &sub->opt);

  mrc_fld_set_type(mhd->fld, FLD_TYPE);
  mrc_fld_set_param_int(mhd->fld, "nr_ghosts", 2);
  mrc_fld_dict_add_int(mhd->fld, "mhd_type", MT_FULLY_CONSERVATIVE_CC);
  mrc_fld_set_param_int(mhd->fld, "nr_comps", 8);
}

// ----------------------------------------------------------------------
// flux_reconstruct

static void
flux_reconstruct(struct ggcm_mhd_step *step, 
		 struct mrc_fld *U3d_l[3], struct mrc_fld *U3d_r[3],
		 struct mrc_fld *x, struct mrc_fld *B_cc,
		 int ldim, int bnd, int j, int k, int dir, int p)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);

  fld1d_state_t U = sub->U, U_l = sub->U_l, U_r = sub->U_r;
  fld1d_state_t W = sub->W, W_l = sub->W_l, W_r = sub->W_r;

  // FIXME: +2,+2 is specifically for PLM reconstr (and enough for PCM)
  pick_line_fc_cc(U, x, ldim, bnd + 2, bnd + 2, j, k, dir, p);
  mhd_prim_from_cons(W, U, ldim, bnd + 2, bnd + 2);
  int l = bnd, r = bnd + 1;
  mhd_reconstruct(U_l, U_r, W_l, W_r, W, (fld1d_t) {}, ldim, l, r, dir);
  put_line_fc_cc(U3d_l[dir], U_l, ldim, l, r, j, k, dir, p);
  put_line_fc_cc(U3d_r[dir], U_r, ldim, l, r, j, k, dir, p);
}

// ----------------------------------------------------------------------
// flux_riemann

static void
flux_riemann(struct ggcm_mhd_step *step, struct mrc_fld *fluxes[3],
	     struct mrc_fld *U3d_l[3], struct mrc_fld *U3d_r[3],
	     struct mrc_fld *B_cc,
	     int ldim, int bnd, int j, int k, int dir, int p)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);

  fld1d_state_t U_l = sub->U_l, U_r = sub->U_r;
  fld1d_state_t W_l = sub->W_l, W_r = sub->W_r, F = sub->F;

  int l = bnd, r = bnd + 1;
  pick_line_fc_cc(U_l, U3d_l[dir], ldim, l, r, j, k, dir, p);
  pick_line_fc_cc(U_r, U3d_r[dir], ldim, l, r, j, k, dir, p);
  mhd_prim_from_cons(W_l, U_l, ldim, l, r);
  mhd_prim_from_cons(W_r, U_r, ldim, l, r);
  mhd_riemann(F, U_l, U_r, W_l, W_r, ldim, l, r, dir);
  put_line_fc_cc(fluxes[dir], F, ldim, l, r, j, k, dir, p);
}

static void
pushstage_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt,
	    struct mrc_fld *x_curr, struct mrc_fld *x_next)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *fluxes[3] = { ggcm_mhd_get_3d_fld(mhd, 8),
				ggcm_mhd_get_3d_fld(mhd, 8),
				ggcm_mhd_get_3d_fld(mhd, 8), };
  struct mrc_fld *U_l[3] = { ggcm_mhd_get_3d_fld(mhd, 8),
			     ggcm_mhd_get_3d_fld(mhd, 8),
			     ggcm_mhd_get_3d_fld(mhd, 8), };
  struct mrc_fld *U_r[3] = { ggcm_mhd_get_3d_fld(mhd, 8),
			     ggcm_mhd_get_3d_fld(mhd, 8),
			     ggcm_mhd_get_3d_fld(mhd, 8), };
  
  int gdims[3];
  mrc_domain_get_global_dims(x_curr->_domain, gdims);

  // reconstruct
  for (int p = 0; p < mrc_fld_nr_patches(x_curr); p++) {
    pde_for_each_dir(dir) {
      pde_for_each_line(dir, j, k, 0) {
	flux_reconstruct(step, U_l, U_r, x_curr, NULL, s_ldims[dir], 0, j, k, dir, p);
       }
    }
  }

  ggcm_mhd_fill_ghosts_reconstr(mhd, U_l, U_r);

  // riemann solve
  for (int p = 0; p < mrc_fld_nr_patches(x_curr); p++) {
    pde_for_each_dir(dir) {
      pde_for_each_line(dir, j, k, 0) {
	flux_riemann(step, fluxes, U_l, U_r, x_curr, s_ldims[dir], 0, j, k, dir, p);
       }
    }
  }

  update_finite_volume(mhd, x_next, fluxes, mhd->ymask, dt, true);
  
  ggcm_mhd_put_3d_fld(mhd, U_l[0]);
  ggcm_mhd_put_3d_fld(mhd, U_l[1]);
  ggcm_mhd_put_3d_fld(mhd, U_l[2]);
  ggcm_mhd_put_3d_fld(mhd, U_r[0]);
  ggcm_mhd_put_3d_fld(mhd, U_r[1]);
  ggcm_mhd_put_3d_fld(mhd, U_r[2]);
  ggcm_mhd_put_3d_fld(mhd, fluxes[0]);
  ggcm_mhd_put_3d_fld(mhd, fluxes[1]);
  ggcm_mhd_put_3d_fld(mhd, fluxes[2]);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_get_dt

static double
ggcm_mhd_step_mhdcc_get_dt(struct ggcm_mhd_step *step, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;

  ggcm_mhd_fill_ghosts(mhd, x, 0, mhd->time);
  return newstep_sc(mhd, x, NULL, 0);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_euler

static void
ggcm_mhd_step_euler(struct ggcm_mhd_step *step, struct mrc_fld *x, double dt)
{
  static int pr_A;
  if (!pr_A) {
    pr_A = prof_register("mhd_step_euler", 0, 0, 0);
  }

  prof_start(pr_A);
  pushstage_c(step, dt, x, x);
  prof_stop(pr_A);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_predcorr

static void
ggcm_mhd_step_predcorr(struct ggcm_mhd_step *step, struct mrc_fld *x, double dt)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *x_star = sub->x_star;

  static int pr_A, pr_B;
  if (!pr_A) {
    pr_A = prof_register("mhd_step_pred", 0, 0, 0);
    pr_B = prof_register("mhd_step_corr", 0, 0, 0);
  }

  // --- PREDICTOR
  prof_start(pr_A);
  ggcm_mhd_fill_ghosts(mhd, x, 0, mhd->time);

  // set x_star = x^n, then advance to n+1/2
  mrc_fld_copy(x_star, x);
  pushstage_c(step, .5f * dt, x, x_star);
  prof_stop(pr_A);

  // --- CORRECTOR
  prof_start(pr_B);
  ggcm_mhd_fill_ghosts(mhd, x_star, 0, mhd->time + .5f * mhd->bndt);

  pushstage_c(step, dt, x_star, x);
  prof_stop(pr_B);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_run

static void
ggcm_mhd_step_mhdcc_run(struct ggcm_mhd_step *step, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;

  if (s_opt_time_integrator == OPT_TIME_INTEGRATOR_EULER) {
    ggcm_mhd_step_euler(step, x, mhd->dt);
  } else if (s_opt_time_integrator == OPT_TIME_INTEGRATOR_PREDCORR) {
    ggcm_mhd_step_predcorr(step, x, mhd->dt);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step_mhdcc, x)
static struct param ggcm_mhd_step_mhdcc_descr[] = {
  { "eqn"                , VAR(opt.eqn)            , PARAM_SELECT(OPT_EQN,
								  opt_eqn_descr)                },
  { "limiter"            , VAR(opt.limiter)        , PARAM_SELECT(OPT_LIMITER_FLAT,
								  opt_limiter_descr)            },
  { "riemann"            , VAR(opt.riemann)        , PARAM_SELECT(OPT_RIEMANN_RUSANOV,
								  opt_riemann_descr)            },
  { "time_integrator"    , VAR(opt.time_integrator), PARAM_SELECT(OPT_TIME_INTEGRATOR_PREDCORR,
								  opt_time_integrator_descr)    },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_step subclass "mhdcc_*"

struct ggcm_mhd_step_ops ggcm_mhd_step_mhdcc_ops = {
  .name                = ggcm_mhd_step_mhdcc_name,
  .size                = sizeof(struct ggcm_mhd_step_mhdcc),
  .param_descr         = ggcm_mhd_step_mhdcc_descr,
  .setup               = ggcm_mhd_step_mhdcc_setup,
  .get_dt              = ggcm_mhd_step_mhdcc_get_dt,
  .run                 = ggcm_mhd_step_mhdcc_run,
  .destroy             = ggcm_mhd_step_mhdcc_destroy,
  .setup_flds          = ggcm_mhd_step_mhdcc_setup_flds,
};
