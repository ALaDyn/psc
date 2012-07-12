
#ifndef PSC_BND_H
#define PSC_BND_H

#include <mrc_obj.h>

#include "psc.h"

MRC_CLASS_DECLARE(psc_bnd, struct psc_bnd);

void psc_bnd_set_psc(struct psc_bnd *bnd, struct psc *psc);
void psc_bnd_add_ghosts(struct psc_bnd *bnd, mfields_base_t *flds, int mb, int me);
void psc_bnd_fill_ghosts(struct psc_bnd *bnd, mfields_base_t *flds, int mb, int me);
void psc_bnd_exchange_particles(struct psc_bnd *bnd, mparticles_base_t *particles);

#endif
