
#include "psc_randomize_private.h"

#include "psc_glue.h"
#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_randomize_fortran_run

static void
psc_randomize_fortran_run(struct psc_randomize *randomize,
			  struct psc_mparticles *mprts_base)
{
  static int pr;
  if (!pr) {
    pr = prof_register("fort_randomize", 1., 0, 0);
  }

  mparticles_fortran_t mprts = mprts_base->get_as<mparticles_fortran_t>();

  prof_start(pr);
  assert(mprts->nr_patches == 1);
  struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
  PIC_randomize(prts);
  prof_stop(pr);

  mprts.put_as(mprts_base);
}

// ======================================================================
// psc_randomize: subclass "fortran"

struct psc_randomize_ops psc_randomize_fortran_ops = {
  .name                  = "fortran",
  .run                   = psc_randomize_fortran_run,
};
