
#include "psc_push_particles_private.h"

#include "psc_particles_as_single.h"
#include "psc_fields_as_single.h"

#include "../inc_defs.h"

#define DIM DIM_YZ
#define CALC_J CALC_J_1VB_VAR1
#define INTERPOLATE_1ST INTERPOLATE_1ST_EC

#define psc_push_particles_push_a_yz psc_push_particles_1vbec_single_push_a_yz

#include "1vb.c"

