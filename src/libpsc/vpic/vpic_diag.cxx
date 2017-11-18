
#include "vpic_iface.h"
#include "vpic_diag.h"

#include "simulation.h"

#include "mrc_common.h"

// ================================================================================
// globals_diag implementation

void vpic_simulation_diagnostics(vpic_simulation *simulation, globals_diag *diag);
void vpic_simulation_setup_diagnostics(vpic_simulation *simulation, globals_diag *diag,
				       species_t *electron, species_t *ion);



globals_diag::globals_diag(int interval_)
{
  rtoggle = 0;
  
  interval = interval_;
  fields_interval = interval_;
  ehydro_interval = interval_;
  Hhydro_interval = interval_;
  eparticle_interval = 8 * interval_;
  Hparticle_interval = 8 * interval_;
  restart_interval = 8000;

  energies_interval = 50;

  MPI_Comm comm = MPI_COMM_WORLD;
  mpi_printf(comm, "interval = %d\n", interval);
  mpi_printf(comm, "energies_interval: %d\n", energies_interval);
}

void globals_diag::setup(Simulation *sim)
{
  species_t *electron = sim->simulation_->find_species("electron");
  species_t *ion = sim->simulation_->find_species("ion");
  vpic_simulation_setup_diagnostics(sim->simulation_, this, electron, ion);
}

void globals_diag::run(Simulation *sim)
{
  TIC vpic_simulation_diagnostics(sim->simulation_, this); TOC(user_diagnostics, 1);
}

