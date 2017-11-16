
#include "psc_method_private.h"

#include <psc_fields_vpic.h>
#include <psc_particles_vpic.h>
#include <psc_push_particles_vpic.h>

#include <psc_balance.h>
#include <psc_marder.h>

#include <vpic_iface.h>

// ======================================================================
// psc_method "vpic"

struct psc_method_vpic {
  bool use_deck_field_ic;
  bool use_deck_particle_ic;
  bool split;

  struct vpic_params vpic_prm;
};

#define psc_method_vpic(method) mrc_to_subobj(method, struct psc_method_vpic)

#define VAR(x) (void *)offsetof(struct psc_method_vpic, x)
static struct param psc_method_vpic_descr[] = {
  { "use_deck_field_ic"     , VAR(use_deck_field_ic)              , PARAM_BOOL(false) },
  { "use_deck_particle_ic"  , VAR(use_deck_particle_ic)           , PARAM_BOOL(false) },
  { "split"                 , VAR(split)                          , PARAM_BOOL(false) },
  
  { "quota"                 , VAR(vpic_prm.quota)                 , PARAM_DOUBLE(1.)  },
  { "quota_check_interval"  , VAR(vpic_prm.quota_check_interval)  , PARAM_INT(100)    },
  { "restart_interval"      , VAR(vpic_prm.restart_interval)      , PARAM_INT(8000)   },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_method_vpic_do_setup

static void
psc_method_vpic_do_setup(struct psc_method *method, struct psc *psc)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);
  struct vpic_params *prm = &sub->vpic_prm;
  MPI_Comm comm = psc_comm(psc);

  prm->cfl_req              = psc->prm.cfl;
  prm->status_interval      = psc->prm.stats_every;
  for (int d = 0; d < 3; d++) {
    prm->gdims[d]           = psc->domain.gdims[d];
    prm->np[d]              = psc->domain.np[d];
  }

  mpi_printf(comm, "*** Initializing\n");
  struct vpic_simulation_info info;
  if (sub->split) {
    struct psc_harris *harris = psc_harris(psc);
    vpic_simulation_init_split(prm, harris, &info);
  } else {
    vpic_simulation_init(&info);
  }

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

  psc->dt = info.dt;
  mpi_printf(comm, "method_vpic_do_setup: Setting dt = %g\n", psc->dt);

  psc->n_state_fields = VPIC_MFIELDS_N_COMP;
  // having two ghost points wouldn't really hurt, however having no ghost points
  // in the invariant direction does cause trouble.
  // By setting this here, it will override what otherwise happens automatically
  psc->ibn[0] = psc->ibn[1] = psc->ibn[2] = 1;
  mpi_printf(comm, "method_vpic_do_setup: Setting n_state_fields = %d, ibn = [%d,%d,%d]\n",
	     psc->n_state_fields, psc->ibn[0], psc->ibn[1], psc->ibn[2]);

  psc_setup_coeff(psc);
  psc_setup_domain(psc);
}

// ----------------------------------------------------------------------
// psc_method_vpic_setup_partition

static void
psc_method_vpic_setup_partition(struct psc_method *method, struct psc *psc,
				int *n_prts_by_patch, int *particle_label_offset)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);

  if (sub->use_deck_particle_ic) {
    assert(psc->nr_patches == 1);
    n_prts_by_patch[0] = 1; // fake, but not possible to balance, anyway
  } else {
    psc_setup_partition(psc, n_prts_by_patch, particle_label_offset);
  }
}

// ----------------------------------------------------------------------
// psc_method_vpic_setup_particles

static void
psc_method_vpic_setup_particles(struct psc_method *method, struct psc *psc,
				int *n_prts_by_patch, int particle_label_offset)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);

  // set up particles
    if (sub->use_deck_particle_ic) {
    // If we want to use the deck particle i.c., we need to copy the
    // already set up "vpic" particles over to the base particles.
    struct psc_mparticles *mprts_vpic = psc_mparticles_get_as(psc->particles, "vpic", MP_DONT_COPY | MP_DONT_RESIZE);
    psc_mparticles_put_as(mprts_vpic, psc->particles, 0);
  } else {
    psc_mparticles_reserve_all(psc->particles, n_prts_by_patch);
    psc_setup_particles(psc, n_prts_by_patch, particle_label_offset);
  }
}

// ----------------------------------------------------------------------
// psc_method_vpic_setup_fields

static void
psc_method_vpic_setup_fields(struct psc_method *method, struct psc *psc)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);
  
  if (sub->use_deck_field_ic) {
    // The vpic-internal fields have already been initialized by the deck,
    // but by the end of this function, PSC expects the base fields to be contains
    // the initial condition.
    // So let's copy the vpic-internal fields into the base fields in this somewhat
    // odd fashion.
    struct psc_mfields *mflds_vpic = psc_mfields_get_as(psc->flds, "vpic", 0, 0);
    psc_mfields_put_as(mflds_vpic, psc->flds, 0, VPIC_MFIELDS_N_COMP);
  } else {
    // While the fields may already have been initialized by the deck,
    // we'll initialize them the PSC way now.  And in case PSC doesn't
    // specificy a field i.c., clear out whatever the deck did first.
    psc_mfields_zero_range(psc->flds, 0, psc->flds->nr_fields);
    // This does the usual PSC initialization.
    psc_setup_fields(psc);
  }
}

// ----------------------------------------------------------------------
// psc_method_vpic_initialize

static void
psc_method_vpic_initialize(struct psc_method *method, struct psc *psc)
{
  struct psc_method_vpic *sub = psc_method_vpic(method);

  struct psc_mfields *mflds_base = psc->flds;
  struct psc_mparticles *mprts_base = psc->particles;
  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, "vpic", 0, VPIC_MFIELDS_N_COMP);
  struct psc_mparticles *mprts = psc_mparticles_get_as(mprts_base, "vpic", 0);
  
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

  psc_mparticles_put_as(mprts, mprts_base, 0);
  psc_mfields_put_as(mflds, mflds_base, 0, VPIC_MFIELDS_N_COMP);

  // First output / stats
  
  mpi_printf(psc_comm(psc), "Performing initial diagnostics.\n");
  if (sub->split) {
    vpic_diagnostics_split(psc_harris(psc));
  } else {
    vpic_diagnostics();
  }
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
  struct psc_method_vpic *sub = psc_method_vpic(method);

  // FIXME, a hacky place to do this
  vpic_inc_step(psc->timestep);

  if (sub->split) {
    vpic_diagnostics_split(psc_harris(psc));
  } else {
    vpic_diagnostics();
  }
  
  if (psc->prm.stats_every > 0 && psc->timestep % psc->prm.stats_every == 0) {
    vpic_print_status();
  }
  
  psc_method_default_output(NULL, psc);
}

// ----------------------------------------------------------------------
// psc_method "vpic"

struct psc_method_ops psc_method_ops_vpic = {
  .name                          = "vpic",
  .size                          = sizeof(struct psc_method_vpic),
  .param_descr                   = psc_method_vpic_descr,
  .do_setup                      = psc_method_vpic_do_setup,
  .setup_fields                  = psc_method_vpic_setup_fields,
  .setup_partition               = psc_method_vpic_setup_partition,
  .setup_particles               = psc_method_vpic_setup_particles,
  .initialize                    = psc_method_vpic_initialize,
  .output                        = psc_method_vpic_output,
};
