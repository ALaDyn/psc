
#include "psc_method_private.h"

#include <psc_fields_vpic.h>
#include <psc_particles_vpic.h>

#include <setup_particles.hxx>
#include <psc_particles_double.h>

#include <vpic_iface.h>

// ======================================================================
// psc_method "vpic"

struct psc_method_vpic {
  // state
  Simulation *sim;
};

#define psc_method_vpic(method) mrc_to_subobj(method, struct psc_method_vpic)

#define VAR(x) (void *)offsetof(struct psc_method_vpic, x)
static struct param psc_method_vpic_descr[] = {
  { "sim"                   , VAR(sim)                            , PARAM_PTR(NULL)   },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_method_vpic_print_status

void
psc_method_vpic_print_status(struct psc_method *method)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);

#ifdef HAVE_VPIC
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  update_profile(rank == 0);
#endif
}

// ----------------------------------------------------------------------
// psc_method_vpic_inc_step

void
psc_method_vpic_inc_step(struct psc_method *method, int timestep)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);
  Simulation* sim = sub->sim;

  sim->grid_->step++;
  assert(sim->grid_->step == timestep);
}

// ----------------------------------------------------------------------
// psc_method "vpic"

struct psc_method_ops_vpic : psc_method_ops {
  psc_method_ops_vpic() {
    name                          = "vpic";
    size                          = sizeof(struct psc_method_vpic);
    param_descr                   = psc_method_vpic_descr;
  }
} psc_method_ops_vpic;
