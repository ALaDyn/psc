
#include <mrc_fld_as_double.h>

#define ggcm_mhd_step_c_ops ggcm_mhd_step_c_double_ops
#define ggcm_mhd_step_c_name "c_double"

#define OPT_STAGGER OPT_STAGGER_GGCM

#include "ggcm_mhd_step_c_common.c"
