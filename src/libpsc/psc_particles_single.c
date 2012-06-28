
#include "psc.h"
#include "psc_particles_single.h"
#include "psc_particles_c.h"

#include <mrc_io.h>
#include <mrc_profile.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// ======================================================================
// psc_particles "single"

static void
psc_particles_single_setup(struct psc_particles *prts)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  sngl->n_alloced = prts->n_part * 1.2;
  sngl->particles = calloc(sngl->n_alloced, sizeof(*sngl->particles));
  sngl->b_idx = calloc(sngl->n_alloced, sizeof(*sngl->b_idx));

  for (int d = 0; d < 3; d++) {
    sngl->b_mx[d] = ppsc->patch[prts->p].ldims[d];
    sngl->b_dxi[d] = 1.f / ppsc->dx[d];
  }
  sngl->nr_blocks = sngl->b_mx[0] * sngl->b_mx[1] * sngl->b_mx[2];
  sngl->b_cnt = calloc(sngl->nr_blocks + 1, sizeof(*sngl->b_cnt));
}

static void
psc_particles_single_destroy(struct psc_particles *prts)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  free(sngl->particles);
  free(sngl->b_idx);
  free(sngl->b_cnt);
}

static void
psc_particles_single_reorder(struct psc_particles *prts,
			     unsigned int *b_idx, unsigned int *b_sums)
{
  static int pr, pr_A;
  if (!pr) {
    pr = prof_register("single_reorder", 1., 0, 0);
    pr_A = prof_register("single_reorder2", 1., 0, 0);
  }

  struct psc_particles_single *sngl = psc_particles_single(prts);

  sngl->n_alloced = prts->n_part * 1.2;
  struct psc_particle_single *particles_new = malloc(sngl->n_alloced * sizeof(*particles_new));

#if 1
  prof_start(pr);
  for (int n = 0; n < prts->n_part; n++) {
    int n_new = b_sums[b_idx[n]]++;
    particles_new[n_new] = sngl->particles[n];
  }
  prof_stop(pr);
#else
  int *b_off = malloc(sngl->n_alloced * sizeof(*b_off));
  prof_start(pr);
  for (int n = 0; n < prts->n_part; n++) {
    int n_new = b_sums[b_idx[n]]++;
    //    b_off[n_new] = n;
    b_off[n] = n_new;
  }
  prof_stop(pr);

  prof_start(pr_A);
  for (int n = 0; n < prts->n_part; n++) {
    //    particles_new[n] = sngl->particles[b_off[n]];
    particles_new[b_off[n]] = sngl->particles[n];
  }
  prof_stop(pr_A);
  free(b_off);
#endif

  free(sngl->particles);
  sngl->particles = particles_new;
}

void
particles_single_realloc(struct psc_particles *prts, int new_n_part)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  if (new_n_part <= sngl->n_alloced)
    return;

  sngl->n_alloced = new_n_part * 1.2;
  sngl->particles = realloc(sngl->particles, sngl->n_alloced * sizeof(*sngl->particles));
  sngl->b_idx = realloc(sngl->b_idx, sngl->n_alloced * sizeof(*sngl->b_idx));
}

static inline void
calc_vxi(particle_single_real_t vxi[3], particle_single_t *part)
{
  particle_single_real_t root =
    1.f / sqrtf(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

static void
psc_particles_single_copy_to_c(struct psc_particles *prts_base,
			       struct psc_particles *prts_c, unsigned int flags)
{
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  prts_c->n_part = prts_base->n_part;
  assert(prts_c->n_part <= psc_particles_c(prts_c)->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_single_t *part_base = particles_single_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_c_real_t qni = ppsc->kinds[part_base->kind].q;
    particle_c_real_t mni = ppsc->kinds[part_base->kind].m;
    particle_c_real_t wni = part_base->qni_wni / qni;
    
    particle_single_real_t vxi[3];
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
    part->kind = part_base->kind;
  }
}

static void
psc_particles_single_copy_from_c(struct psc_particles *prts_base,
				 struct psc_particles *prts_c, unsigned int flags)
{
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  struct psc_particles_single *sngl = psc_particles_single(prts_base);
  prts_base->n_part = prts_c->n_part;
  assert(prts_base->n_part <= sngl->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_single_t *part_base = particles_single_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_single_real_t qni_wni;
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

    particle_single_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part_base->xi += dth[0] * vxi[0];
    part_base->yi += dth[1] * vxi[1];
    part_base->zi += dth[2] * vxi[2];
  }
}

// ======================================================================
// psc_mparticles: subclass "single"
  
struct psc_mparticles_ops psc_mparticles_single_ops = {
  .name                    = "single",
};

// ======================================================================
// psc_particles: subclass "single"

static struct mrc_obj_method psc_particles_single_methods[] = {
  MRC_OBJ_METHOD("copy_to_c",   psc_particles_single_copy_to_c),
  MRC_OBJ_METHOD("copy_from_c", psc_particles_single_copy_from_c),
  {}
};

struct psc_particles_ops psc_particles_single_ops = {
  .name                    = "single",
  .size                    = sizeof(struct psc_particles_single),
  .methods                 = psc_particles_single_methods,
  .setup                   = psc_particles_single_setup,
  .destroy                 = psc_particles_single_destroy,
  .reorder                 = psc_particles_single_reorder,
};
