
#include "psc_generic_c.h"

#include <mrc_profile.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DIM DIM_YZ
#define ORDER ORDER_2ND
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_yz
#define PROF_NAME "genc_push_mprts_yz"
#include "push_part_common.c"

