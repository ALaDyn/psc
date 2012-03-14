
#ifndef PSC_DIAG_ITEM_PRIVATE_H
#define PSC_DIAG_ITEM_PRIVATE_H

#include "psc_diag_item.h"

struct psc_diag_item {
  struct mrc_obj obj;
};

struct psc_diag_item_ops {
  MRC_SUBCLASS_OPS(struct psc_diag_item);
  
  void (*run)(struct psc_diag_item *item, struct psc *psc, double *result);
  int nr_values;
  const char *title[];
};

#define psc_diag_item_ops(item) ((struct psc_diag_item_ops *)((item)->obj.ops))

extern struct psc_diag_item_ops psc_diag_item_field_energy_ops;
extern struct psc_diag_item_ops psc_diag_item_particle_energy_ops;

#endif
