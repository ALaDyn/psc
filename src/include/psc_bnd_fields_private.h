
#ifndef PSC_BND_FIELDS_PRIVATE_H
#define PSC_BND_FIELDS_PRIVATE_H

#include <psc_bnd_fields.h>

struct psc_bnd_fields {
  struct mrc_obj obj;
  struct psc_pulse *pulse_x1;
  struct psc_pulse *pulse_x2;
  struct psc_pulse *pulse_y1;
  struct psc_pulse *pulse_y2;
  struct psc_pulse *pulse_z1;
  struct psc_pulse *pulse_z2;
};

struct psc_bnd_fields_ops {
  MRC_SUBCLASS_OPS(struct psc_bnd_fields);
  void (*fill_ghosts_a_E)(struct psc_bnd_fields *bnd, mfields_base_t *flds);
  void (*fill_ghosts_a_H)(struct psc_bnd_fields *bnd, mfields_base_t *flds);
  void (*fill_ghosts_b_H)(struct psc_bnd_fields *bnd, mfields_base_t *flds);
  void (*fill_ghosts_b_E)(struct psc_bnd_fields *bnd, mfields_base_t *flds);
  void (*add_ghosts_J)(struct psc_bnd_fields *bnd, mfields_base_t *flds);
};

// ======================================================================

extern struct psc_bnd_fields_ops psc_bnd_fields_fortran_ops;
extern struct psc_bnd_fields_ops psc_bnd_fields_none_ops;

#define psc_bnd_fields_ops(bnd_fields) ((struct psc_bnd_fields_ops *)((bnd_fields)->obj.ops))

#endif
