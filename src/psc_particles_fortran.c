
#include "psc.h"
#include "psc_particles_fortran.h"

#include <stdlib.h>

static bool __alloced;

void
psc_particles_fortran_alloc(psc_particles_fortran_t *pp, int n_part)
{
  if (!__alloced) {
    __alloced = true;
    pp->particles = ALLOC_particles(n_part);
  } else {
    // FIXME, realloc also copies all particles over,
    // which is not needed when fortran is not the particles base type,
    // but we just use this function to alloc temp storage
    pp->particles = REALLOC_particles(n_part);
  }
}

void
psc_particles_fortran_realloc(psc_particles_fortran_t *pp, int new_n_part)
{
  assert(__alloced);
  pp->particles = REALLOC_particles(new_n_part);
}

void
psc_particles_fortran_free(psc_particles_fortran_t *pp)
{
  assert(__alloced);
  FREE_particles();
  pp->particles = NULL;
  __alloced = false;
}

#if PARTICLES_BASE == PARTICLES_FORTRAN

void
psc_particles_fortran_get(psc_particles_fortran_t *pp)
{
  pp->particles = psc.pp.particles;
  pp->n_part = psc.pp.n_part;
}

void
psc_particles_fortran_put(psc_particles_fortran_t *pp)
{
  psc.pp.n_part = pp->n_part;
  psc.pp.particles = pp->particles;
}

#else

static int __gotten;

void
psc_particles_fortran_get(psc_particles_fortran_t *pp)
{
  assert(!__gotten);
  __gotten = 1;

  psc_particles_base_t *pp_base = &psc.pp;

  psc_particles_fortran_alloc(pp, pp_base->n_part);
  pp->n_part = pp_base->n_part;
  SET_niloc(pp->n_part);

  for (int n = 0; n < pp_base->n_part; n++) {
    particle_fortran_t *f_part = psc_particles_fortran_get_one(pp, n);
    particle_base_t *part = psc_particles_base_get_one(pp_base, n);

    f_part->xi  = part->xi;
    f_part->yi  = part->yi;
    f_part->zi  = part->zi;
    f_part->pxi = part->pxi;
    f_part->pyi = part->pyi;
    f_part->pzi = part->pzi;
    f_part->qni = part->qni;
    f_part->mni = part->mni;
    f_part->wni = part->wni;
  }
}

void
psc_particles_fortran_put(psc_particles_fortran_t *pp)
{
  assert(__gotten);
  __gotten = 0;

  GET_niloc(&pp->n_part);
  psc_particles_base_t *pp_base = &psc.pp;
  pp_base->n_part = pp->n_part;

  for (int n = 0; n < pp_base->n_part; n++) {
    particle_fortran_t *f_part = &pp->particles[n];
    particle_base_t *part = psc_particles_base_get_one(pp_base, n);

    part->xi  = f_part->xi;
    part->yi  = f_part->yi;
    part->zi  = f_part->zi;
    part->pxi = f_part->pxi;
    part->pyi = f_part->pyi;
    part->pzi = f_part->pzi;
    part->qni = f_part->qni;
    part->mni = f_part->mni;
    part->wni = f_part->wni;
  }
}

#endif
