
#include <mrc_fld_as_double.h>

#include "ggcm_mhd_step_c_common.c"

// ----------------------------------------------------------------------
// ggcm_mhd_step subclass "c_double"

struct ggcm_mhd_step_ops ggcm_mhd_step_c_double_ops = {
  .name        = "c_double",
  .mhd_type    = MT_SEMI_CONSERVATIVE_GGCM,
  .fld_type    = FLD_TYPE,
  .nr_ghosts   = 2,
  .newstep     = ggcm_mhd_step_c_newstep,
  .pred        = ggcm_mhd_step_c_pred,
  .corr        = ggcm_mhd_step_c_corr,
  .run         = ggcm_mhd_step_run_predcorr,
};
