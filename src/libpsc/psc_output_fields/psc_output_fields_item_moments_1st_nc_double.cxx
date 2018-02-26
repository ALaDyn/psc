
#include "psc.h"
#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"
#include "fields.hxx"

using fields_t = mfields_t::fields_t;
using Fields = Fields3d<fields_t>;

// ======================================================================
// !!! These moments are shifted to (n+.5) * dt, rather than n * dt,
// since the "double" particles are shifted that way.
//
// node-centered moments -- probably mostly useful for testing
// charge continuity

#include "psc_output_fields_item_moments_1st_nc.cxx"

MAKE_POFI_OPS(double)
