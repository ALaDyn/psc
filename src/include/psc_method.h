
#ifndef PSC_METHOD_H
#define PSC_METHOD_H

#include "psc.h"

MRC_CLASS_DECLARE(psc_method, struct psc_method);

void psc_method_do_setup(struct psc_method *method, struct psc *psc);
void psc_method_setup_fields(struct psc_method *method, struct psc *psc);
void psc_method_setup_partition(struct psc_method *method, struct psc *psc, int *n_prts_by_patch);
void psc_method_setup_particles(struct psc_method *method, struct psc *psc, int *n_prts_by_patch);
void psc_method_initialize(struct psc_method *method, struct psc *psc);
void psc_method_output(struct psc_method *method, struct psc *psc);

#endif

