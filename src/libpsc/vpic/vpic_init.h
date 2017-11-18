
#ifndef VPIC_INIT_H
#define VPIC_INIT_H

#include "vpic_iface.h"
#include "vpic.h"

#include "mrc_common.h"

extern vpic_simulation *simulation;

struct globals_diag;

void vpic_simulation_diagnostics(vpic_simulation *simulation, globals_diag *diag);
void vpic_simulation_setup_diagnostics(vpic_simulation *simulation, globals_diag *diag,
				       species_t *electron, species_t *ion);

// ----------------------------------------------------------------------
// globals_diag

struct globals_diag {
  int interval;
  int energies_interval;
  int fields_interval;
  int ehydro_interval;
  int Hhydro_interval;
  int eparticle_interval;
  int Hparticle_interval;
  int restart_interval;

  // state
  int rtoggle;               // enables save of last 2 restart dumps for safety
  // Output variables
  DumpParameters fdParams;
  DumpParameters hedParams;
  DumpParameters hHdParams;
  std::vector<DumpParameters *> outputParams;

  globals_diag(int interval_);
  void setup();
  void run();
};

inline globals_diag::globals_diag(int interval_)
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

inline void globals_diag::setup()
{
  species_t *electron = simulation->find_species("electron");
  species_t *ion = simulation->find_species("ion");
  vpic_simulation_setup_diagnostics(simulation, this, electron, ion);
}

inline void globals_diag::run()
{
  TIC vpic_simulation_diagnostics(simulation, this); TOC(user_diagnostics, 1);
}

// ----------------------------------------------------------------------



#endif
