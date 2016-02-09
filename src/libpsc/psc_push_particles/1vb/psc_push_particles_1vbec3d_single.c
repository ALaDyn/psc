
#include "psc_push_particles_private.h"

#include "psc_particles_as_single.h"
#include "psc_fields_as_single.h"

// FIXME -> some header
void psc_push_particles_1vbec3d_single_push_a_yz(struct psc_push_particles *push,
						 struct psc_particles *prts,
						 struct psc_fields *flds);
void psc_push_particles_1vbec3d_single_push_a_xyz(struct psc_push_particles *push,
						  struct psc_particles *prts,
						  struct psc_fields *flds);

// ======================================================================
// psc_push_particles: subclass "1vbec3d_single"

struct psc_push_particles_ops psc_push_particles_1vbec3d_single_ops = {
  .name                  = "1vbec3d_single",
  .push_a_yz             = psc_push_particles_1vbec3d_single_push_a_yz,
  .push_a_xyz            = psc_push_particles_1vbec3d_single_push_a_xyz,
  .particles_type        = PARTICLE_TYPE,
  .fields_type           = FIELDS_TYPE,
};

