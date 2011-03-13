
#include "psc.h"
#include "psc_particles_fortran.h"

#include <stdlib.h>

#if PARTICLES_BASE == PARTICLES_FORTRAN

void
particles_fortran_alloc(particles_fortran_t *pp, int n_part)
{
  pp->n_alloced = n_part * 1.2;
  pp->particles = calloc(pp->n_alloced, sizeof(*pp->particles));
}

void
particles_fortran_realloc(particles_fortran_t *pp, int new_n_part)
{
  if (new_n_part <= pp->n_alloced)
    return;

  pp->n_alloced = new_n_part * 1.2;
  pp->particles = realloc(pp->particles, pp->n_alloced * sizeof(*pp->particles));
}

void
particles_fortran_free(particles_fortran_t *pp)
{
  free(pp->particles);
  pp->n_alloced = 0;
  pp->particles = NULL;
}

void
particles_fortran_get(mparticles_fortran_t *particles, void *_particles_base)
{
  mparticles_base_t *particles_base = _particles_base;
  *particles = *particles_base;
}

void
particles_fortran_put(mparticles_fortran_t *particles, void *_particles_base)
{
  mparticles_base_t *particles_base = _particles_base;
  *particles_base = *particles;
}

#else

static bool __gotten;

void
particles_fortran_get(mparticles_fortran_t *particles, void *_particles_base)
{
  assert(!__gotten);
  __gotten = true;

  mparticles_base_t *particles_base = _particles_base;

  particles->p = calloc(psc.nr_patches, sizeof(*particles->p));
  foreach_patch(p) {
    particles_base_t *pp_base = &particles_base->p[p];
    particles_c_t *pp = &particles->p[p];
    pp->n_part = pp_base->n_part;
    pp->particles = calloc(pp->n_part, sizeof(*pp->particles));

    for (int n = 0; n < pp_base->n_part; n++) {
      particle_fortran_t *f_part = particles_fortran_get_one(pp, n);
      particle_base_t *part = particles_base_get_one(pp_base, n);

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
}

void
particles_fortran_put(mparticles_fortran_t *particles, void *_particles_base)
{
  assert(__gotten);
  __gotten = false;

  mparticles_base_t *particles_base = _particles_base;
  foreach_patch(p) {
    particles_base_t *pp_base = &particles_base->p[p];
    particles_c_t *pp = &particles->p[p];
    assert(pp->n_part == pp_base->n_part);
    for (int n = 0; n < pp_base->n_part; n++) {
      particle_fortran_t *f_part = &pp->particles[n];
      particle_base_t *part = particles_base_get_one(pp_base, n);
      
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

    free(pp->particles);
  }
  free(particles->p);
  particles->p = NULL;
}

#endif
