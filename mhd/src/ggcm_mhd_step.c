
#include "ggcm_mhd_step_private.h"

#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_commu.h"

#include <mrc_io.h>
#include <assert.h>

// ======================================================================
// ggcm_mhd_step class

// ----------------------------------------------------------------------
// ggcm_mhd_step_pred

void
ggcm_mhd_step_pred(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_ops *ops = ggcm_mhd_step_ops(step);
  assert(ops && ops->pred);
  ops->pred(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_corr

void
ggcm_mhd_step_corr(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_ops *ops = ggcm_mhd_step_ops(step);
  assert(ops && ops->corr);
  ops->corr(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_push

void
ggcm_mhd_step_push(struct ggcm_mhd_step *step)
{
  ggcm_mhd_commu_run(step->mhd->commu, _RR1, _RR1 + 8);
  ggcm_mhd_step_pred(step);
  ggcm_mhd_commu_run(step->mhd->commu, _RR2, _RR2 + 8);
  ggcm_mhd_step_corr(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_calc_rhs

void
ggcm_mhd_step_calc_rhs(struct ggcm_mhd_step *step, struct mrc_f3 *rhs,
		       struct mrc_f3 *x)
{
  struct ggcm_mhd_step_ops *ops = ggcm_mhd_step_ops(step);
  assert(ops && ops->calc_rhs);
  ops->calc_rhs(step, rhs, x);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_init

static void
ggcm_mhd_step_init()
{
}

// ----------------------------------------------------------------------
// ggcm_mhd_step description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step, x)
static struct param ggcm_mhd_step_descr[] = {
  { "mhd"             , VAR(mhd)             , PARAM_OBJ(ggcm_mhd)       },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_step class description

struct mrc_class_ggcm_mhd_step mrc_class_ggcm_mhd_step = {
  .name             = "ggcm_mhd_step",
  .size             = sizeof(struct ggcm_mhd_step),
  .param_descr      = ggcm_mhd_step_descr,
  .init             = ggcm_mhd_step_init,
};

