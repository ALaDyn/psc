
#ifndef PSC_COLLISION_PRIVATE_H
#define PSC_COLLISION_PRIVATE_H

#include <psc_collision.h>

struct psc_collision {
  struct mrc_obj obj;
};

struct psc_collision_ops {
  MRC_OBJ_OPS;
  void (*run)(struct psc_collision *collision, mparticles_base_t *particles);
};

// ======================================================================

extern struct psc_collision_ops psc_collision_none_ops;
extern struct psc_collision_ops psc_collision_fortran_ops;

#define to_psc_collision(o) (container_of(o, struct psc_collision, obj))
#define psc_collision_ops(collision) ((struct psc_collision_ops *)((collision)->obj.ops))

#endif
