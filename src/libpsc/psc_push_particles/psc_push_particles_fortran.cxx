
#include "psc_push_particles_private.h"
#include "psc_glue.h"

#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_push_particles_push_a_xy

static void
psc_push_particles_fortran_push_a_xy(struct psc_push_particles *push,
				     struct psc_particles *prts,
				     struct psc_fields *flds)
{
  assert(ppsc->nr_patches == 1);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xy", 1., 0, 0);
  }

  prof_start(pr);
  psc_fields_zero_range(flds, JXI, JXI + 3);
  PIC_push_part_xy(ppsc, prts->p, prts, flds);
  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_a_xz

static void
psc_push_particles_fortran_push_a_xz(struct psc_push_particles *push,
				     struct psc_particles *prts,
				     struct psc_fields *flds)
{
  assert(ppsc->nr_patches == 1);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xz", 1., 0, 0);
  }

  prof_start(pr);
  psc_fields_zero_range(flds, JXI, JXI + 3);
  PIC_push_part_xz(ppsc, prts->p, prts, flds);
  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_a_yz

static void
psc_push_particles_fortran_push_a_yz(struct psc_push_particles *push,
				     struct psc_particles *prts,
				     struct psc_fields *flds)
{
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz", 1., 0, 0);
  }

  prof_start(pr);
  psc_fields_zero_range(flds, JXI, JXI + 3);
  PIC_push_part_yz(ppsc, prts->p, prts, flds);
  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_xyz

static void
psc_push_particles_fortran_push_a_xyz(struct psc_push_particles *push,
				      struct psc_particles *prts,
				      struct psc_fields *flds)
{
  assert(ppsc->nr_patches == 1);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xyz", 1., 0, 0);
  }

  prof_start(pr);
  psc_fields_zero_range(flds, JXI, JXI + 3);
  PIC_push_part_xyz(ppsc, prts->p, prts, flds);
  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_a_z

static void
psc_push_particles_fortran_push_a_z(struct psc_push_particles *push,
				    struct psc_particles *prts,
				    struct psc_fields *flds)
{
  assert(ppsc->nr_patches == 1);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_z", 1., 0, 0);
  }

  prof_start(pr);
  psc_fields_zero_range(flds, JXI, JXI + 3);
  PIC_push_part_z(ppsc, prts->p, prts, flds);
  prof_stop(pr);
}

// ======================================================================
// psc_push_particles: subclass "fortran"

struct psc_push_particles_ops psc_push_particles_fortran_ops = {
  .name                  = "fortran",
  .push_a_z              = psc_push_particles_fortran_push_a_z,
  .push_a_xy             = psc_push_particles_fortran_push_a_xy,
  .push_a_xz             = psc_push_particles_fortran_push_a_xz,
  .push_a_yz             = psc_push_particles_fortran_push_a_yz,
  .push_a_xyz            = psc_push_particles_fortran_push_a_xyz,
  .particles_type        = "fortran",
};
