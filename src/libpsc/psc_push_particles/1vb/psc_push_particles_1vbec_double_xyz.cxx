
#include "psc_push_particles_private.h"

#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"

#define DIM DIM_XYZ

#include "../inc_defs.h"

#define ORDER ORDER_1ST
#define IP_VARIANT IP_VARIANT_EC
#define CALC_J CALC_J_1VB_SPLIT

#define psc_push_particles_push_mprts_xyz psc_push_particles_1vbec_double_push_mprts_xyz
#define psc_push_particles_stagger_mprts_xyz psc_push_particles_1vbec_double_stagger_mprts_xyz

#include "1vb.c"

