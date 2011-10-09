
#include "psc_sort_private.h"

#include "psc_glue.h"
#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_sort_fortran_run

static void
psc_sort_fortran_run(struct psc_sort *sort,
			  mparticles_base_t *particles_base)
{
  assert(ppsc->nr_patches == 1);
  
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_sort", 1., 0, 0);
  }
  prof_start(pr);
  particles_fortran_t *pp = psc_mparticles_get_patch_fortran(particles, 0);
  PIC_find_cell_indices(pp);
  PIC_sort(pp);
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
}

// ======================================================================
// psc_sort: subclass "fortran"

struct psc_sort_ops psc_sort_fortran_ops = {
  .name                  = "fortran",
  .run                   = psc_sort_fortran_run,
};
