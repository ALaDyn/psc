
#include "psc.h"
#include "psc_particles_c.h"

#include <mrc_io.h>
#include <stdlib.h>
#include <assert.h>

// ======================================================================
// psc_particles "c"

static void
psc_particles_c_setup(struct psc_particles *prts)
{
  struct psc_particles_c *c = psc_particles_c(prts);

  int n_alloced = psc_particles_size(prts) * 1.2;
  psc_mparticles_set_n_alloced(prts->mprts, prts->p, n_alloced);
  c->particles = calloc(n_alloced, sizeof(*c->particles));
}

static void
psc_particles_c_destroy(struct psc_particles *prts)
{
  struct psc_particles_c *c = psc_particles_c(prts);

  free(c->particles);
}

void
particles_c_realloc(struct psc_particles *prts, int new_n_part)
{
  struct psc_particles_c *c = psc_particles_c(prts);
  if (new_n_part <= psc_mparticles_n_alloced(prts->mprts, prts->p))
    return;

  int n_alloced = new_n_part * 1.2;
  psc_mparticles_set_n_alloced(prts->mprts, prts->p, n_alloced);
  c->particles = realloc(c->particles, n_alloced * sizeof(*c->particles));
}

// ======================================================================
// psc_mparticles: subclass "c"
  
struct psc_mparticles_ops psc_mparticles_c_ops = {
  .name                    = "c",
};

// ======================================================================
// psc_particles: subclass "c"

struct psc_particles_ops psc_particles_c_ops = {
  .name                    = "c",
  .size                    = sizeof(struct psc_particles_c),
  .setup                   = psc_particles_c_setup,
  .destroy                 = psc_particles_c_destroy,
};
