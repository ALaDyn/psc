
#include "psc_push_particles_private.h"

#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"

#define DIM DIM_YZ

#include "../inc_defs.h"

#define CALC_J CALC_J_1VB_VAR1
#define IP_VARIANT IP_VARIANT_EC

#define psc_push_particles_push_mprts_yz psc_push_particles_1vbec_double_push_mprts_yz

#include "1vb.c"

