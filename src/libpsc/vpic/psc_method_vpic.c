
#include "psc_method_private.h"

#include <psc_fields_vpic.h>
#include <psc_particles_vpic.h>
#include <psc_push_particles_vpic.h>

#include <psc_marder.h>

#include <vpic_iface.h>

// ======================================================================
// psc_method "vpic"

// ----------------------------------------------------------------------
// psc_method_vpic_do_setup

static void
psc_method_vpic_do_setup(struct psc_method *method, struct psc *psc)
{
  struct vpic_simulation_info info;
  vpic_simulation_init(&info);

  MPI_Comm comm = psc_comm(psc);
  MPI_Barrier(comm);

  psc->prm.nmax = info.num_step;
  psc->prm.stats_every = info.status_interval;

  struct psc_kind *kinds = calloc(info.n_kinds, sizeof(*kinds));
  for (int m = 0; m < info.n_kinds; m++) {
    kinds[m].q = info.kinds[m].q;
    kinds[m].m = info.kinds[m].m;
    // map "electron" -> "e", "ion"-> "i" to avoid even more confusion with
    // how moments etc are named.
    if (strcmp(info.kinds[m].name, "electron") == 0) {
      kinds[m].name = "e";
    } else if (strcmp(info.kinds[m].name, "ion") == 0) {
      kinds[m].name = "i";
    } else {
      kinds[m].name = info.kinds[m].name;
    }
  }
  psc_set_kinds(psc, info.n_kinds, kinds);
  free(kinds);
  
  psc_marder_set_param_int(psc->marder, "clean_div_e_interval", info.clean_div_e_interval);
  psc_marder_set_param_int(psc->marder, "clean_div_b_interval", info.clean_div_b_interval);
  psc_marder_set_param_int(psc->marder, "sync_shared_interval", info.sync_shared_interval);
  psc_marder_set_param_int(psc->marder, "num_div_e_round", info.num_div_e_round);
  psc_marder_set_param_int(psc->marder, "num_div_b_round", info.num_div_b_round);

  int *np = psc->domain.np;
  mpi_printf(comm, "domain: np = %d x %d x %d\n", np[0], np[1], np[2]);
  //int np[3] = { 4, 1, 1 }; // FIXME, hardcoded, but really hard to get from vpic

  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);
  assert(size == np[0] * np[1] * np[2]);

  int p[3];
  p[2] = rank / (np[1] * np[0]); rank -= p[2] * (np[1] * np[0]);
  p[1] = rank / np[0]; rank -= p[1] * np[0];
  p[0] = rank;

  double x0[3], x1[3];
  for (int d = 0; d < 3; d++) {
    x0[d] = info.x0[d] - p[d] * (info.x1[d] - info.x0[d]);
    x1[d] = info.x0[d] + (np[d] - p[d]) * (info.x1[d] - info.x0[d]);
  }

  // FIXME, it's also not so obvious that this is going to be the same on all procs
  mprintf("domain: local x0 %g:%g:%g x1 %g:%g:%g\n",
	  info.x0[0], info.x0[1], info.x0[2],
	  info.x1[0], info.x1[1], info.x1[2]);
  mprintf("domain: p %d %d %d\n",
	  p[0], p[1], p[2]);
  mprintf("domain: global x0 %g:%g:%g x1 %g:%g:%g\n",
	  x0[0], x0[1], x0[2],
	  x1[0], x1[1], x1[2]);

  // set size of simulation box to match vpic
  for (int d = 0; d < 3; d++) {
    psc->domain.length[d] = x1[d] - x0[d];
    psc->domain.corner[d] = x0[d];
    psc->domain.gdims[d] = np[d] * info.nx[d];
    psc->domain.np[d] = np[d];
  }

  mpi_printf(comm, "method_vpic_do_setup: Setting dt = %g\n", info.dt);
  psc->dt = info.dt;
}

// ----------------------------------------------------------------------
// psc_method_vpic_initialize

static void
psc_method_vpic_initialize(struct psc_method *method, struct psc *psc)
{
  struct psc_mparticles *mprts = psc->particles;
  psc_mfields_view(psc->flds);
  struct psc_mfields *mflds = psc_mfields_get_as(psc->flds, "vpic", 0, 0);
  
  // Do some consistency checks on user initialized fields

  mpi_printf(psc_comm(psc), "Checking interdomain synchronization\n");
  double err = psc_mfields_synchronize_tang_e_norm_b(mflds);
  mpi_printf(psc_comm(psc), "Error = %g (arb units)\n", err);
  
  mpi_printf(psc_comm(psc), "Checking magnetic field divergence\n");
  psc_mfields_compute_div_b_err(mflds);
  err = psc_mfields_compute_rms_div_b_err(mflds);
  mpi_printf(psc_comm(psc), "RMS error = %e (charge/volume)\n", err);
  psc_mfields_clean_div_b(mflds);
  
  // Load fields not initialized by the user

  mpi_printf(psc_comm(psc), "Initializing radiation damping fields\n");
  psc_mfields_compute_curl_b(mflds);

  mpi_printf(psc_comm(psc), "Initializing bound charge density\n");
  psc_mfields_clear_rhof(mflds);
  psc_mfields_accumulate_rho_p(mflds, mprts);
  psc_mfields_synchronize_rho(mflds);
  psc_mfields_compute_rhob(mflds);

  // Internal sanity checks

  mpi_printf(psc_comm(psc), "Checking electric field divergence\n");
  psc_mfields_compute_div_e_err(mflds);
  err = psc_mfields_compute_rms_div_e_err(mflds);
  mpi_printf(psc_comm(psc), "RMS error = %e (charge/volume)\n", err);
  psc_mfields_clean_div_e(mflds);

  mpi_printf(psc_comm(psc), "Rechecking interdomain synchronization\n");
  err = psc_mfields_synchronize_tang_e_norm_b(mflds);
  mpi_printf(psc_comm(psc), "Error = %e (arb units)\n", err);

  mpi_printf(psc_comm(psc), "Uncentering particles\n");
  psc_push_particles_stagger(psc->push_particles, mprts, mflds);

  psc_mfields_put_as(mflds, psc->flds, 0, 9);

  // First output / stats
  
  mpi_printf(psc_comm(psc), "Performing initial diagnostics.\n");
  vpic_diagnostics();
  psc_method_default_output(method, psc);

  vpic_print_status();
  psc_stats_log(psc);
  psc_print_profiling(psc);
}

// ----------------------------------------------------------------------
// psc_method_vpic_output

static void
psc_method_vpic_output(struct psc_method *method, struct psc *psc)
{
  // FIXME, a hacky place to do this
  vpic_inc_step(psc->timestep);

  vpic_diagnostics();
  
  if (psc->prm.stats_every > 0 && psc->timestep % psc->prm.stats_every == 0) {
    vpic_print_status();
  }
  
  psc_method_default_output(NULL, psc);
}

// ----------------------------------------------------------------------
// psc_method "vpic"

struct psc_method_ops psc_method_ops_vpic = {
  .name                = "vpic",
  .do_setup            = psc_method_vpic_do_setup,
  .initialize          = psc_method_vpic_initialize,
  .output              = psc_method_vpic_output,
};
