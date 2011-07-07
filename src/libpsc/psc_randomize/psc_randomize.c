
#include "psc_randomize_private.h"

// ======================================================================
// forward to subclass

void
psc_randomize_run(struct psc_randomize *randomize, mparticles_base_t *particles)
{
  static int st_time_randomize;
  if (!st_time_randomize) {
    st_time_randomize = psc_stats_register("time randomize");
  }

  psc_stats_start(st_time_randomize);
  struct psc_randomize_ops *ops = psc_randomize_ops(randomize);
  assert(ops->run);
  ops->run(randomize, particles);
  psc_stats_stop(st_time_randomize);
}

// ======================================================================
// psc_randomize_init

static void
psc_randomize_init()
{
  mrc_class_register_subclass(&mrc_class_psc_randomize, &psc_randomize_none_ops);
  mrc_class_register_subclass(&mrc_class_psc_randomize, &psc_randomize_fortran_ops);
}

// ======================================================================
// psc_randomize class

struct mrc_class_psc_randomize mrc_class_psc_randomize = {
  .name             = "psc_randomize",
  .size             = sizeof(struct psc_randomize),
  .init             = psc_randomize_init,
};

