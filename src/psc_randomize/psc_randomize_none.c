
#include "psc_randomize_private.h"

#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_randomize_none_run

static void
psc_randomize_none_run(struct psc_randomize *randomize,
			  mparticles_base_t *particles_base)
{
}

// ======================================================================
// psc_randomize: subclass "none"

struct psc_randomize_ops psc_randomize_none_ops = {
  .name                  = "none",
  .run                   = psc_randomize_none_run,
};
