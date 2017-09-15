
#include <mrc_obj.h>

#include "psc.h"

MRC_CLASS_DECLARE(psc_push_particles, struct psc_push_particles);

void psc_push_particles_run(struct psc_push_particles *push,
			    mparticles_base_t *particles, mfields_base_t *flds);
unsigned int psc_push_particles_get_mp_flags(struct psc_push_particles *push);
