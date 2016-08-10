
#include "ggcm_mhd_step_private.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_defs_extra.h"
#include "ggcm_mhd_diag_private.h"

#include <string.h>

// FIXME: major ugliness
// The fortran fields do primitive vars in the order _RR,_PP,_VX,_VY,_VZ
// but in C, we stick with the corresponding conservative var order, ie.,
// RR,VX,VY,VZ,PP
// The below hackily switches the order around in C, so that it matches fortran

#define PP 1
#define VX 2
#define VY 3
#define VZ 4

#include "pde/pde_defs.h"

// mhd options

#define OPT_EQN OPT_EQN_MHD_SCONS

#include "pde/pde_setup.c"
#include "pde/pde_mhd_setup.c"

// TODO:
// - handle remaining resistivity models
// - handle limit2, limit3
// - handle lowmask

enum {
  LIMIT_NONE,
  LIMIT_1,
};

static mrc_fld_data_t s_mhd_time;

static float *s_fd1x, *s_fd1y, *s_fd1z;

#define FX1X(ix) PDE_CRDX_CC(ix)
#define FX1Y(iy) PDE_CRDY_CC(iy)
#define FX1Z(iz) PDE_CRDZ_CC(iz)

#define FX2X(ix) (sqr(FX1X(ix)))
#define FX2Y(iy) (sqr(FX1Y(iy)))
#define FX2Z(iz) (sqr(FX1Z(iz)))

// FD1 is really different if 'legacy_fd1' is used
#if 1
#define FD1X(ix) (s_fd1x[ix])
#define FD1Y(iy) (s_fd1y[iy])
#define FD1Z(iz) (s_fd1z[iz])
#else
#define FD1X(ix) PDE_INV_DX(ix)
#define FD1Y(iy) PDE_INV_DY(iy)
#define FD1Z(iz) PDE_INV_DZ(iz)
#endif

#define BD1X(ix) PDE_INV_DXF(ix+1)
#define BD1Y(iy) PDE_INV_DYF(iy+1)
#define BD1Z(iz) PDE_INV_DZF(iz+1)

#define BD2X(ix) PDE_DX(ix)
#define BD2Y(iy) PDE_DY(iy)
#define BD2Z(iz) PDE_DZ(iz)

#define BD3X(ix) PDE_INV_DX(ix)
#define BD3Y(iy) PDE_INV_DY(iy)
#define BD3Z(iz) PDE_INV_DZ(iz)

#define BD4X(ix) BD1X(ix)
#define BD4Y(iy) BD1Y(iy)
#define BD4Z(iz) BD1Z(iz)

// FIXME, this is here, because it uses FD1X etc,
// so it either shouldn't, or we put the macros above into some pde_* compat file

#include "pde/pde_mhd_get_dt.c"
#include "pde/pde_mhd_push.c"
#include "pde/pde_mhd_badval_checks.c"

// ======================================================================
// ggcm_mhd_step subclass "c"
//
// this class will do full predictor / corrector steps,
// ie., including primvar() etc.

struct ggcm_mhd_step_c {
  struct mhd_options opt;
};

#define ggcm_mhd_step_c(step) mrc_to_subobj(step, struct ggcm_mhd_step_c)

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_setup_flds

static void
ggcm_mhd_step_c_setup_flds(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_c *sub = ggcm_mhd_step_c(step);
  struct ggcm_mhd *mhd = step->mhd;

  pde_mhd_set_options(mhd, &sub->opt);
  mrc_fld_set_type(mhd->fld, FLD_TYPE);
  mrc_fld_set_param_int(mhd->fld, "nr_ghosts", 2);
  mrc_fld_dict_add_int(mhd->fld, "mhd_type", MT_SEMI_CONSERVATIVE_GGCM);
  mrc_fld_set_param_int(mhd->fld, "nr_comps", _NR_FLDS);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_setup

static void
ggcm_mhd_step_c_setup(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;
  pde_setup(mhd->fld);
  pde_mhd_setup(mhd);

  mhd->ymask = mrc_fld_make_view(mhd->fld, _YMASK, _YMASK + 1);
  mrc_fld_set(mhd->ymask, 1.);

  ggcm_mhd_step_setup_member_objs_sub(step);
  ggcm_mhd_step_setup_super(step);

  s_fd1x = ggcm_mhd_crds_get_crd(mhd->crds, 0, FD1);
  s_fd1y = ggcm_mhd_crds_get_crd(mhd->crds, 1, FD1);
  s_fd1z = ggcm_mhd_crds_get_crd(mhd->crds, 2, FD1);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_destroy

static void
ggcm_mhd_step_c_destroy(struct ggcm_mhd_step *step)
{
  mrc_fld_destroy(step->mhd->ymask);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_newstep

static void
ggcm_mhd_step_c_newstep(struct ggcm_mhd_step *step, float *dtn)
{
  struct ggcm_mhd *mhd = step->mhd;

  ggcm_mhd_fill_ghosts(mhd, mhd->fld, _RR1, mhd->time);
  *dtn = pde_mhd_get_dt_scons_ggcm(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_pred

static void
ggcm_mhd_step_c_pred(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;
  fld3d_t p_f;
  fld3d_setup(&p_f, mhd->fld);
  s_mhd_time = mhd->time;

  pde_for_each_patch(p) {
    fld3d_get(&p_f, p);
    patch_pushstage(p_f, mhd->dt, 0);
    fld3d_put(&p_f, 0);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_corr

static void
ggcm_mhd_step_c_corr(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;
  fld3d_t p_f;
  fld3d_setup(&p_f, mhd->fld);
  s_mhd_time = mhd->time;

  pde_for_each_patch(p) {
    fld3d_get(&p_f, 0);
    patch_pushstage(p_f, mhd->dt, 1);
    patch_badval_checks_sc(step->mhd, p_f, p_f);
    fld3d_put(&p_f, 0);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_get_e_ec

static void
ggcm_mhd_step_c_get_e_ec(struct ggcm_mhd_step *step, struct mrc_fld *Eout,
                         struct mrc_fld *state_vec)
{
  // the state vector should already be FLD_TYPE, but Eout is the data type
  // of the output
  struct mrc_fld *E = mrc_fld_get_as(Eout, FLD_TYPE);
  struct mrc_fld *x = mrc_fld_get_as(state_vec, FLD_TYPE);

  mrc_fld_foreach(x, ix, iy, iz, 1, 0) {
    F3(E, 0, ix, iy, iz) = F3(x, _FLX, ix, iy, iz);
    F3(E, 1, ix, iy, iz) = F3(x, _FLY, ix, iy, iz);
    F3(E, 2, ix, iy, iz) = F3(x, _FLZ, ix, iy, iz);
  } mrc_fld_foreach_end;

  mrc_fld_put_as(E, Eout);
  // FIXME, should use _put_as, but don't want copy-back
  if (strcmp(mrc_fld_type(state_vec), FLD_TYPE) != 0) {
    mrc_fld_destroy(x);
  }
} 

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_diag_item_zmask_run

static void
ggcm_mhd_step_c_diag_item_zmask_run(struct ggcm_mhd_step *step,
				    struct ggcm_mhd_diag_item *item,
				    struct mrc_io *io, struct mrc_fld *f,
				    int diag_type, float plane)
{
  ggcm_mhd_diag_c_write_one_field(io, f, _ZMASK, "zmask", 1., diag_type, plane);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_diag_item_rmask_run

static void
ggcm_mhd_step_c_diag_item_rmask_run(struct ggcm_mhd_step *step,
				    struct ggcm_mhd_diag_item *item,
				    struct mrc_io *io, struct mrc_fld *f,
				    int diag_type, float plane)
{
  ggcm_mhd_diag_c_write_one_field(io, f, _RMASK, "rmask", 1., diag_type, plane);
}

// ----------------------------------------------------------------------
// subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step_c, x)
static struct param ggcm_mhd_step_c_descr[] = {
  { "eqn"                , VAR(opt.eqn)            , PARAM_SELECT(OPT_EQN,
								  opt_eqn_descr)                },
  { "mhd_primvar"        , VAR(opt.mhd_primvar)    , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_primbb"         , VAR(opt.mhd_primbb)     , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_zmaskn"         , VAR(opt.mhd_zmaskn)     , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_rmaskn"         , VAR(opt.mhd_rmaskn)     , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_newstep"        , VAR(opt.mhd_newstep)    , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushpred"       , VAR(opt.mhd_pushpred)   , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushcorr"       , VAR(opt.mhd_pushcorr)   , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushfluid1"     , VAR(opt.mhd_pushfluid1) , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushfluid2"     , VAR(opt.mhd_pushfluid2) , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushfield1"     , VAR(opt.mhd_pushfield1) , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pushfield2"     , VAR(opt.mhd_pushfield2) , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_push_ej"        , VAR(opt.mhd_push_ej)    , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_pfie3"          , VAR(opt.mhd_pfie3)      , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_bpush1"         , VAR(opt.mhd_bpush1)     , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_calce"          , VAR(opt.mhd_calce)      , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  { "mhd_calc_resis"     , VAR(opt.mhd_calc_resis) , PARAM_SELECT(OPT_MHD_C,
								  opt_mhd_descr)                },
  
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_step subclass "c_*"

struct ggcm_mhd_step_ops ggcm_mhd_step_c_ops = {
  .name                = ggcm_mhd_step_c_name,
  .size                = sizeof(struct ggcm_mhd_step_c),
  .param_descr         = ggcm_mhd_step_c_descr,
  .setup               = ggcm_mhd_step_c_setup,
  .destroy             = ggcm_mhd_step_c_destroy,
  .setup_flds          = ggcm_mhd_step_c_setup_flds,
  .newstep             = ggcm_mhd_step_c_newstep,
  .pred                = ggcm_mhd_step_c_pred,
  .corr                = ggcm_mhd_step_c_corr,
  .run                 = ggcm_mhd_step_run_predcorr,
  .get_e_ec            = ggcm_mhd_step_c_get_e_ec,
  .diag_item_zmask_run = ggcm_mhd_step_c_diag_item_zmask_run,
  .diag_item_rmask_run = ggcm_mhd_step_c_diag_item_rmask_run,
};
