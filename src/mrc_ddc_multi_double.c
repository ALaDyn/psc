
#include <mrc_fld_as_double.h>

#define mrc_ddc_funcs_fld_TYPE mrc_ddc_funcs_fld_double
#define mrc_fld_TYPE_ddc_copy_to_buf mrc_fld_double_ddc_copy_to_buf
#define mrc_fld_TYPE_ddc_copy_from_buf mrc_fld_double_ddc_copy_from_buf

#include "mrc_ddc_multi_common.c"

