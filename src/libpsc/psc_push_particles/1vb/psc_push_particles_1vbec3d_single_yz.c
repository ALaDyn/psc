
#include "psc_push_particles_private.h"

#include "psc_particles_as_single.h"
#include "psc_fields_as_single.h"

#include "../inc_defs.h"

#define DIM DIM_YZ
#define CALC_J CALC_J_1VB_VAR1
#define F3_CURR F3_S
#define F3_CACHE F3_S
#define F3_CACHE_TYPE "single"
#define INTERPOLATE_1ST INTERPOLATE_1ST_EC

#define NOT_STATIC

#define psc_push_particles_push_a_yz psc_push_particles_1vbec3d_single_push_a_yz

#include "1vb_yz.c"

