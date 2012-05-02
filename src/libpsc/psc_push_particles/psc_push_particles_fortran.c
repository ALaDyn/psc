
#include "psc_push_particles_private.h"
#include "psc_glue.h"

#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_push_particles_push_xy

static void
psc_push_particles_fortran_push_xy(struct psc_push_particles *push,
				   mparticles_base_t *particles_base,
				   mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xy", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_xy(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		     psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_xz

static void
psc_push_particles_fortran_push_xz(struct psc_push_particles *push,
				   mparticles_base_t *particles_base,
				   mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xz", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_xz(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		     psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_yz

static void
psc_push_particles_fortran_push_yz(struct psc_push_particles *push,
				   mparticles_base_t *particles_base,
				   mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_yz(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		     psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_xyz

static void
psc_push_particles_fortran_push_xyz(struct psc_push_particles *push,
				   mparticles_base_t *particles_base,
				   mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xyz", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_xyz(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		      psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_z

static void
psc_push_particles_fortran_push_z(struct psc_push_particles *push,
				   mparticles_base_t *particles_base,
				   mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_z", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_z(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		    psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_yz_a

static void
psc_push_particles_fortran_push_yz_a(struct psc_push_particles *push,
				     mparticles_base_t *particles_base,
				     mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz_a", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_yz_a(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		       psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ----------------------------------------------------------------------
// psc_push_particles_push_yz_b

static void
psc_push_particles_fortran_push_yz_b(struct psc_push_particles *push,
				     mparticles_base_t *particles_base,
				     mfields_base_t *flds_base)
{
  assert(ppsc->nr_patches == 1);
  mparticles_fortran_t *particles = psc_mparticles_get_fortran(particles_base, 0);
  mfields_fortran_t *flds = psc_mfields_get_fortran(flds_base, EX, EX + 6);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz_b", 1., 0, 0);
  }
  prof_start(pr);
  psc_foreach_patch(ppsc, p) {
    PIC_push_part_yz_b(ppsc, p, psc_mparticles_get_patch_fortran(particles, p),
		       psc_mfields_get_patch_fortran(flds, p));
  }
  prof_stop(pr);

  psc_mparticles_put_fortran(particles, particles_base);
  psc_mfields_put_fortran(flds, flds_base, JXI, JXI + 3);
}

// ======================================================================
// psc_push_particles: subclass "fortran"

struct psc_push_particles_ops psc_push_particles_fortran_ops = {
  .name                  = "fortran",
  .push_z                = psc_push_particles_fortran_push_z,
  .push_xy               = psc_push_particles_fortran_push_xy,
  .push_xz               = psc_push_particles_fortran_push_xz,
  .push_yz               = psc_push_particles_fortran_push_yz,
  .push_xyz              = psc_push_particles_fortran_push_xyz,

  .push_yz_a             = psc_push_particles_fortran_push_yz_a,
  .push_yz_b             = psc_push_particles_fortran_push_yz_b,
};
