
#include "psc_bnd_c.h"
#include "psc_bnd_private.h"
#include "ddc_particles.h"
#include "psc_particles_as_c.h"

#include "psc_bnd_common.c"

// ======================================================================
// psc_bnd: subclass "c"

struct psc_bnd_ops psc_bnd_c_ops = {
  .name                  = "c",
  .size                  = sizeof(struct psc_bnd_sub),
  .setup                 = psc_bnd_sub_setup,
  .unsetup               = psc_bnd_sub_unsetup,
  .exchange_particles    = psc_bnd_sub_exchange_particles,

  .create_ddc            = psc_bnd_fields_c_create,
  .add_ghosts            = psc_bnd_fields_c_add_ghosts,
  .fill_ghosts           = psc_bnd_fields_c_fill_ghosts,
};
