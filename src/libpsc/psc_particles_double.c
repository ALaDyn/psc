
#include "psc.h"
#include "psc_particles_double.h"
#include "psc_particles_c.h"

#include <mrc_io.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// ======================================================================
// psc_particles "double"

static void
psc_particles_double_setup(struct psc_particles *prts)
{
  struct psc_particles_double *dbl = psc_particles_double(prts);

  dbl->n_alloced = prts->n_part * 1.2;
  dbl->particles = calloc(dbl->n_alloced, sizeof(*dbl->particles));
}

static void
psc_particles_double_destroy(struct psc_particles *prts)
{
  struct psc_particles_double *dbl = psc_particles_double(prts);

  free(dbl->particles);
}

#if 0
void
particles_double_realloc(particles_double_t *pp, int new_n_part)
{
  if (new_n_part <= pp->n_alloced)
    return;

  pp->n_alloced = new_n_part * 1.2;
  pp->particles = realloc(pp->particles, pp->n_alloced * sizeof(*pp->particles));
}
#endif

static inline void
calc_vxi(particle_double_real_t vxi[3], particle_double_t *part)
{
  particle_double_real_t root =
    1.f / sqrtf(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

static void
psc_particles_double_copy_to_c(struct psc_particles *prts_base,
			       struct psc_particles *prts_c, unsigned int flags)
{
  particle_double_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  prts_c->n_part = prts_base->n_part;
  assert(prts_c->n_part <= psc_particles_c(prts_c)->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_double_t *part_base = particles_double_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_c_real_t qni = ppsc->kinds[part_base->kind].q;
    particle_c_real_t mni = ppsc->kinds[part_base->kind].m;
    particle_c_real_t wni = part_base->qni_wni / qni;
    
    particle_double_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part->xi  = part_base->xi - dth[0] * vxi[0];
    part->yi  = part_base->yi - dth[1] * vxi[1];
    part->zi  = part_base->zi - dth[2] * vxi[2];
    part->pxi = part_base->pxi;
    part->pyi = part_base->pyi;
    part->pzi = part_base->pzi;
    part->qni = qni;
    part->mni = mni;
    part->wni = wni;
    part->kind = part->kind;
  }
}

static void
psc_particles_double_copy_from_c(struct psc_particles *prts_base,
				 struct psc_particles *prts_c, unsigned int flags)
{
  particle_double_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  struct psc_particles_double *dbl = psc_particles_double(prts_base);
  prts_base->n_part = prts_c->n_part;
  assert(prts_base->n_part <= dbl->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_double_t *part_base = particles_double_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_double_real_t qni_wni;
    if (part->qni != 0.) {
      qni_wni = part->qni * part->wni;
    } else {
	qni_wni = part->wni;
    }
    
    part_base->xi          = part->xi;
    part_base->yi          = part->yi;
    part_base->zi          = part->zi;
    part_base->pxi         = part->pxi;
    part_base->pyi         = part->pyi;
    part_base->pzi         = part->pzi;
    part_base->qni_wni     = qni_wni;
    part_base->kind        = part->kind;

    particle_double_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part_base->xi += dth[0] * vxi[0];
    part_base->yi += dth[1] * vxi[1];
    part_base->zi += dth[2] * vxi[2];
  }
}

// ======================================================================
// psc_mparticles: subclass "double"
  
struct psc_mparticles_ops psc_mparticles_double_ops = {
  .name                    = "double",
};

// ======================================================================
// psc_particles: subclass "double"

static struct mrc_obj_method psc_particles_double_methods[] = {
  MRC_OBJ_METHOD("copy_to_c",   psc_particles_double_copy_to_c),
  MRC_OBJ_METHOD("copy_from_c", psc_particles_double_copy_from_c),
  {}
};

struct psc_particles_ops psc_particles_double_ops = {
  .name                    = "double",
  .size                    = sizeof(struct psc_particles_double),
  .methods                 = psc_particles_double_methods,
  .setup                   = psc_particles_double_setup,
  .destroy                 = psc_particles_double_destroy,
};
