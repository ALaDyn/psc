
#ifndef PSC_PARTICLES_PRIVATE_H
#define PSC_PARTICLES_PRIVATE_H

#include "psc_particles.h"

struct psc_particles {
  struct mrc_obj obj;
  int n_part;
  int p; //< patch number
  unsigned int flags;
};

struct psc_particles_ops {
  MRC_SUBCLASS_OPS(struct psc_particles);
  void (*reorder)(struct psc_particles *prts,
		  unsigned int *b_idx, unsigned int *b_sums);
};

#define psc_particles_ops(prts) ((struct psc_particles_ops *) ((prts)->obj.ops))

// ======================================================================

extern struct psc_particles_ops psc_particles_c_ops;
extern struct psc_particles_ops psc_particles_single_ops;
extern struct psc_particles_ops psc_particles_double_ops;
extern struct psc_particles_ops psc_particles_fortran_ops;
extern struct psc_particles_ops psc_particles_cuda_ops;

#endif
