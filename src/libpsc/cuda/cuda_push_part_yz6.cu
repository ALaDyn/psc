
#include "psc_cuda.h"

#define DIM DIM_YZ
#define PFX(x) yz6_ ## x
#define CACHE_SHAPE_ARRAYS 4
#define CALC_CURRENT
#undef USE_SCRATCH
#define SW (2)

#include "constants.c"
#include "common.c"
#include "common_push.c"
#include "common_fld_cache.c"
#include "common_curr.c"

#include "push_part_p1.c"
#include "push_part_p2_noshift_4.c"
#include "push_part_kernelcall_3.c"
