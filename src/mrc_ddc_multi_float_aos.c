
#include <mrc_fld_as_float_aos.h>

#define mrc_ddc_funcs_fld_TYPE mrc_ddc_funcs_fld_float_aos
#define mrc_fld_TYPE_ddc_copy_to_buf mrc_fld_float_aos_ddc_copy_to_buf
#define mrc_fld_TYPE_ddc_copy_from_buf mrc_fld_float_aos_ddc_copy_from_buf

#include "mrc_ddc_multi_common.c"

