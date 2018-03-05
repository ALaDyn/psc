
#ifndef PSC_BND_PARTICLES_PRIVATE_H
#define PSC_BND_PARTICLES_PRIVATE_H

#include <psc_bnd_particles.h>

struct psc_bnd_particles {
  struct mrc_obj obj;
  struct psc *psc;

  // for open b.c.
  double time_relax;
  bool first_time;
  struct psc_bnd *flds_bnd;
  struct psc_output_fields_item *item_nvt;
  struct psc_mfields *mflds_nvt_av;
  struct psc_mfields *mflds_nvt_last;
  struct psc_mfields *mflds_n_in;
};

struct psc_bnd_particles_ops {
  MRC_SUBCLASS_OPS(struct psc_bnd_particles);
};

#define psc_bnd_particles_ops(bnd) ((struct psc_bnd_particles_ops *)((bnd)->obj.ops))

// ======================================================================

#endif
