
#include "psc_output_fields_item_private.h"

static void
do_n_1st_run(int p, fields_t *pf, particles_t *pp)
{
  particle_real_t fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  particle_real_t dxi = 1.f / ppsc->dx[0], dyi = 1.f / ppsc->dx[1], dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < pp->n_part; n++) {
    particle_t *part = particles_get_one(pp, n);
    int m = particle_single_kind(part);
    DEPOSIT_TO_GRID_1ST_CC(part, pf, m, 1.f);
  }
}

static void
n_1st_run(struct psc_output_fields_item *item, mfields_base_t *flds,
	  mparticles_base_t *particles_base, mfields_c_t *res)
{
  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  psc_mfields_zero_range(res, 0, res->nr_fields);
  
  psc_foreach_patch(ppsc, p) {
    do_n_1st_run(p, psc_mfields_get_patch_c(res, p),
		 psc_mparticles_get_patch(particles, p));
  }

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(item->bnd, res, 0, res->nr_fields);
  add_ghosts_boundary(res, 0, res->nr_fields);
}

static int
n_1st_get_nr_components(struct psc_output_fields_item *item)
{
  return ppsc->prm.nr_kinds;
}

static const char *
n_1st_get_component_name(struct psc_output_fields_item *item, int m)
{
  static char s[100];
  sprintf(s, "n_%s", ppsc->kinds[m].name);
  return s;
}

