
#include "psc_push_particles_private.h"

#include "psc_particles_as_single.h"
#include "psc_fields_as_single.h"

#define DIM DIM_YZ

#include "../inc_defs.h"

#define ORDER ORDER_1ST
#define IP_VARIANT IP_VARIANT_EC
#define CALC_J CALC_J_1VB_VAR1

#define psc_push_particles_push_mprts_yz psc_push_particles_1vbec_single_push_mprts_yz

#include "1vb.c"

