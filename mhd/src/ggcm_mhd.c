
#include "ggcm_mhd_private.h"

#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_crds.h"
#include "ggcm_mhd_crds_gen.h"
#include "ggcm_mhd_step.h"
#include "ggcm_mhd_commu.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_bnd.h"
#include "ggcm_mhd_ic.h"

#include <mrc_domain.h>
#include <mrc_profile.h>

#include <assert.h>
#include <string.h>

#define ggcm_mhd_ops(mhd) ((struct ggcm_mhd_ops *) mhd->obj.ops)

static const char *fldname[_NR_FLDS] = {
  [_RR1 ] = "rr1",
  [_RV1X] = "rv1x",
  [_RV1Y] = "rv1y",
  [_RV1Z] = "rv1z",
  [_UU1 ] = "uu1",
  [_B1X ] = "b1x",
  [_B1Y ] = "b1y",
  [_B1Z ] = "b1z",

  [_RR2 ] = "rr2",
  [_RV2X] = "rv2x",
  [_RV2Y] = "rv2y",
  [_RV2Z] = "rv2z",
  [_UU2 ] = "uu2",
  [_B2X ] = "b2x",
  [_B2Y ] = "b2y",
  [_B2Z ] = "b2z",

  [_YMASK] = "ymask",
  [_ZMASK] = "zmask",
  [_CMSV ] = "cmsv",

  [_RR  ] = "rr",
  [_PP  ] = "pp",
  [_VX  ] = "vx",
  [_VY  ] = "vy",
  [_VZ  ] = "vz",
  [_BX  ] = "bx",
  [_BY  ] = "by",
  [_BZ  ] = "bz",

  [_TMP1] = "tmp1",
  [_TMP2] = "tmp2",
  [_TMP3] = "tmp3",
  [_TMP4] = "tmp4",

  [_FLX ] = "ex",
  [_FLY ] = "ey",
  [_FLZ ] = "ez",

  [_CX  ] = "cx",
  [_CY  ] = "cy",
  [_CZ  ] = "cz",

  [_XTRA1] = "xtra1",
  [_XTRA2] = "xtra2",

  [_RESIS] = "resis",

  [_CURRX] = "currx",
  [_CURRY] = "curry",
  [_CURRZ] = "currz",

  [_RMASK] = "rmask",

  [_BDIPX] = "bdipx",
  [_BDIPY] = "bdipy",
  [_BDIPZ] = "bdipz",
};

// ----------------------------------------------------------------------
// ggcm_mhd methods

static void
_ggcm_mhd_create(struct ggcm_mhd *mhd)
{
  mrc_domain_set_type(mhd->domain, "simple");

  ggcm_mhd_crds_set_param_obj(mhd->crds, "domain", mhd->domain);
  ggcm_mhd_step_set_param_obj(mhd->step, "mhd", mhd);
  ggcm_mhd_commu_set_param_obj(mhd->commu, "mhd", mhd);
  ggcm_mhd_diag_set_param_obj(mhd->diag, "mhd", mhd);
  ggcm_mhd_bnd_set_param_obj(mhd->bnd, "mhd", mhd);
  ggcm_mhd_ic_set_param_obj(mhd->ic, "mhd", mhd);
  mrc_fld_set_param_obj(mhd->fld, "domain", mhd->domain);
  mrc_fld_set_sw(mhd->fld, BND);
  mrc_fld_set_nr_comps(mhd->fld, _NR_FLDS);
  for (int m = 0; m < _NR_FLDS; m++) {
    mrc_fld_set_comp_name(mhd->fld, m, fldname[m]);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_get_state
//
// update C ggcm_mhd state from Fortran common blocks

void
ggcm_mhd_get_state(struct ggcm_mhd *mhd)
{
  struct ggcm_mhd_ops *ops = ggcm_mhd_ops(mhd);
  if (ops->get_state) {
    ops->get_state(mhd);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_set_state
//
// updated Fortran common blocks from C ggcm_mhd state

void
ggcm_mhd_set_state(struct ggcm_mhd *mhd)
{
  struct ggcm_mhd_ops *ops = ggcm_mhd_ops(mhd);
  if (ops->set_state) {
    ops->set_state(mhd);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_read

static void
_ggcm_mhd_read(struct ggcm_mhd *mhd, struct mrc_io *io)
{
  ggcm_mhd_read_member_objs(mhd, io);

  // domain params
  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(mhd->domain, 0, &info);
  for (int d = 0; d < 3; d++) {
    // local domain size
    mhd->im[d] = info.ldims[d];
    // local domain size incl ghost points
    mhd->img[d] = info.ldims[d] + 2 * BND;
  }
}

static void
_ggcm_mhd_setup(struct ggcm_mhd *mhd)
{
  ggcm_mhd_setup_member_objs(mhd);

  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(mhd->domain, 0, &info);
  for (int d = 0; d < 3; d++) {
    // local domain size
    mhd->im[d] = info.ldims[d];
    // local domain size incl ghost points
    mhd->img[d] = info.ldims[d] + 2 * BND;
  }
}

void
ggcm_mhd_fill_ghosts(struct ggcm_mhd *mhd, struct mrc_fld *fld, int m, float bntim)
{
  ggcm_mhd_commu_run(mhd->commu, fld, m, m + 8);
  ggcm_mhd_bnd_fill_ghosts(mhd->bnd, fld, m, bntim);
}

void
ggcm_mhd_newstep(struct ggcm_mhd *mhd, float *dtn)
{
  struct ggcm_mhd_ops *ops = ggcm_mhd_ops(mhd);
  ops->newstep(mhd, dtn);
}

int
ggcm_mhd_ntot(struct ggcm_mhd *mhd)
{
  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(mhd->domain, 0, &info);
  return (info.ldims[0] + 2 * BND) * (info.ldims[1] + 2 * BND) * (info.ldims[2] + 2 * BND);
}

// ======================================================================
// ggcm_mhd class

static void
ggcm_mhd_init()
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_ops_box);
}

static struct mrc_param_select magdiffu_descr[] = {
  { .val = MAGDIFFU_NL1  , .str = "nl1"     },
  { .val = MAGDIFFU_RES1 , .str = "res1"    },
  { .val = MAGDIFFU_CONST, .str = "const"   },
  {},
};

#define VAR(x) (void *)offsetof(struct ggcm_mhd, x)
static struct param ggcm_mhd_descr[] = {
  { "gamma"           , VAR(par.gamm)        , PARAM_FLOAT(1.66667f) },
  { "rrmin"           , VAR(par.rrmin)       , PARAM_FLOAT(.1f)      },
  { "bbnorm"          , VAR(par.bbnorm)      , PARAM_FLOAT(30574.f)  },
  { "vvnorm"          , VAR(par.vvnorm)      , PARAM_FLOAT(6692.98f) },
  { "rrnorm"          , VAR(par.rrnorm)      , PARAM_FLOAT(10000.f)  },
  { "ppnorm"          , VAR(par.ppnorm)      , PARAM_FLOAT(7.43866e8)},
  { "ccnorm"          , VAR(par.ccnorm)      , PARAM_FLOAT(3.81885)  },
  { "eenorm"          , VAR(par.eenorm)      , PARAM_FLOAT(204631.f) },
  { "resnorm"         , VAR(par.resnorm)     , PARAM_FLOAT(5.35845e7)},
  { "diffconstant"    , VAR(par.diffco)      , PARAM_FLOAT(.03f)     },
  { "diffthreshold"   , VAR(par.diffth)      , PARAM_FLOAT(.75f)     },
  { "diffsphere"      , VAR(par.diffsphere)  , PARAM_FLOAT(6.f)      },
  { "speedlimit"      , VAR(par.speedlimit)  , PARAM_FLOAT(1500.f)   },
  { "thx"             , VAR(par.thx)         , PARAM_FLOAT(.40f)     },
  { "isphere"         , VAR(par.isphere)     , PARAM_FLOAT(3.0f)     },
  { "timelo"          , VAR(par.timelo)      , PARAM_FLOAT(0.f)      },
  { "d_i"             , VAR(par.d_i)         , PARAM_FLOAT(0.f)      },
  { "dtmin"           , VAR(par.dtmin)       , PARAM_FLOAT(.0002f)   },
  { "modnewstep"      , VAR(par.modnewstep)  , PARAM_INT(1)          },
  { "magdiffu"        , VAR(par.magdiffu)    , PARAM_SELECT(MAGDIFFU_NL1,
							    magdiffu_descr) },

  { "time"            , VAR(time)            , MRC_VAR_FLOAT         },
  { "dt"              , VAR(dt)              , MRC_VAR_FLOAT         },
  { "istep"           , VAR(istep)           , MRC_VAR_INT           },
  { "timla"           , VAR(timla)           , MRC_VAR_FLOAT         },
  { "dacttime"        , VAR(dacttime)        , MRC_VAR_DOUBLE        },

  { "domain"          , VAR(domain)          , MRC_VAR_OBJ(mrc_domain)        },
  { "fld"             , VAR(fld)             , MRC_VAR_OBJ(mrc_fld)           },
  { "crds"            , VAR(crds)            , MRC_VAR_OBJ(ggcm_mhd_crds)     },
  { "step"            , VAR(step)            , MRC_VAR_OBJ(ggcm_mhd_step)     },
  { "commu"           , VAR(commu)           , MRC_VAR_OBJ(ggcm_mhd_commu)    },
  { "diag"            , VAR(diag)            , MRC_VAR_OBJ(ggcm_mhd_diag)     },
  { "bnd"             , VAR(bnd)             , MRC_VAR_OBJ(ggcm_mhd_bnd)      },
  { "ic"              , VAR(ic)              , MRC_VAR_OBJ(ggcm_mhd_ic)       },

  {},
};
#undef VAR

struct mrc_class_ggcm_mhd mrc_class_ggcm_mhd = {
  .name             = "ggcm_mhd",
  .size             = sizeof(struct ggcm_mhd),
  .param_descr      = ggcm_mhd_descr,
  .init             = ggcm_mhd_init,
  .create           = _ggcm_mhd_create,
  .setup            = _ggcm_mhd_setup,
  .read             = _ggcm_mhd_read,
};

// ----------------------------------------------------------------------
// ts_ggcm_mhd_step_calc_rhs
//
// wrapper to be used in a mrc_ts object

void
ts_ggcm_mhd_step_calc_rhs(void *ctx, struct mrc_obj *_rhs, float time, struct mrc_obj *_fld)
{
  struct ggcm_mhd *mhd = ctx;
  struct mrc_fld *rhs = (struct mrc_fld *) _rhs;
  struct mrc_fld *fld = (struct mrc_fld *) _fld;
  
  mhd->time = time;
  ggcm_mhd_step_calc_rhs(mhd->step, rhs, fld);
}

