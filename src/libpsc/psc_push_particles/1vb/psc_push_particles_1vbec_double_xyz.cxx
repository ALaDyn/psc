
#include "psc_push_particles_private.h"

#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"

#define DIM DIM_XYZ
#define CALC_J CALC_J_1VB_VAR1

#include "../inc_defs.h"
#include "../push_config.hxx"

using push_p_conf = Config1vbecDouble<dim_xyz>;

#include "../1vb.c"

