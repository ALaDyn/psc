
#include "psc_bnd_particles_private.h"
#include "../psc_bnd/ddc_particles.h"
#include "psc_particles_as_single.h"

#include "psc_bnd_particles_common.c"

// ======================================================================
// psc_bnd_particles: subclass "single"

struct psc_bnd_particles_ops psc_bnd_particles_single_ops = {
  .name                    = "single",
  .setup                   = psc_bnd_particles_sub_setup,
  .unsetup                 = psc_bnd_particles_sub_unsetup,
  .exchange_particles      = psc_bnd_particles_sub_exchange_particles,
  .open_calc_moments       = psc_bnd_particles_sub_open_calc_moments,
};
