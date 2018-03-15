
#ifndef PSC_PUSH_PARTICLES_H
#define PSC_PUSH_PARTICLES_H

#include <mrc_obj.h>

#include "psc.h"

MRC_CLASS_DECLARE(psc_push_particles, struct psc_push_particles);

void psc_push_particles_prep(struct psc_push_particles *push,
			     struct psc_mparticles *mprts, struct psc_mfields *mflds);
unsigned int psc_push_particles_get_mp_flags(struct psc_push_particles *push);

#endif
