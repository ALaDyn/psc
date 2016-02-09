
#include "psc_push_particles_private.h"

#include "psc_particles_as_single.h"
#include "psc_fields_as_single.h"

#include "../inc_defs.h"

#define DIM DIM_YZ
#define CALC_J CALC_J_1VB_2D
#define INTERPOLATE_1ST INTERPOLATE_1ST_STD

#define psc_push_particles_push_a_yz psc_push_particles_1vb_single_push_a_yz

#include "1vb.c"

// ======================================================================
// psc_push_particles: subclass "1vb_single"

struct psc_push_particles_ops psc_push_particles_1vb_single_ops = {
  .name                  = "1vb_single",
  .push_a_yz             = psc_push_particles_1vb_single_push_a_yz,
  .particles_type        = PARTICLE_TYPE,
  .fields_type           = FIELDS_TYPE,
};

