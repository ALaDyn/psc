
#include "psc_event_generator_private.h"

#include <mrc_profile.h>

// ----------------------------------------------------------------------
// psc_event_generator_none_run

static void
psc_event_generator_none_run(struct psc_event_generator *gen,
			     struct psc_mparticles *mparticles_base,
			     struct psc_mfields *mflds_base)
{
}

// ======================================================================
// psc_event_generator: subclass "none"

struct psc_event_generator_ops psc_event_generator_none_ops = {
  .name                  = "none",
  .run                   = psc_event_generator_none_run,
};
