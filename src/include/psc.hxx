
#pragma once

#include <psc_method.h>
#include <mrc_profile.h>

#include <particles.hxx>

#include <push_particles.hxx>
#include <checks.hxx>

void psc_method_vpic_initialize(struct psc_method *method, struct psc *psc,
				MfieldsBase& mflds_base, MparticlesBase& mprts_base,
				bool detailed_profiling); // FIXME

// ======================================================================
// PscParams

struct PscParams
{
  double cfl = { .75 }; // CFL number used to determine time step
  int nmax;             // Number of timesteps to run

  bool detailed_profiling; // output profiling info for each process separately

  int balance_interval;
  double balance_factor_fields;
  bool balance_print_loads;
  bool balance_write_loads;

  int sort_interval;

  int collision_interval;
  double collision_nu;

  ChecksParams checks_params;

  int marder_interval;
  double marder_diffusion;
  int marder_loop;
  bool marder_dump;
};
  
// ======================================================================
// Psc

template<typename PscConfig>
struct Psc
{
  using Mparticles_t = typename PscConfig::Mparticles_t;
  using Mfields_t = typename PscConfig::Mfields_t;
  using Balance_t = typename PscConfig::Balance_t;
  using Sort_t = typename PscConfig::Sort_t;
  using Collision_t = typename PscConfig::Collision_t;
  using PushParticles_t = typename PscConfig::PushParticles_t;
  using PushFields_t = typename PscConfig::PushFields_t;
  using Bnd_t = typename PscConfig::Bnd_t;
  using BndFields_t = typename PscConfig::BndFields_t;
  using BndParticles_t = typename PscConfig::BndParticles_t;
  using Checks_t = typename PscConfig::Checks_t;
  using Marder_t = typename PscConfig::Marder_t;

  // ----------------------------------------------------------------------
  // ctor

  Psc(const PscParams& params, psc* psc)
    : p_{params},
      psc_(psc),
      mprts_{psc->grid()},
      mflds_{psc->grid(), psc->n_state_fields, psc->ibn},
      balance_{p_.balance_interval, p_.balance_factor_fields, p_.balance_print_loads, p_.balance_write_loads},
      collision_{psc_comm(psc), p_.collision_interval, p_.collision_nu},
      bnd_{psc_->grid(), psc_->mrc_domain_, psc_->ibn},
      bndp_{psc_->mrc_domain_, psc_->grid()},
      checks_{psc_->grid(), psc_comm(psc), p_.checks_params},
      marder_(psc_comm(psc), p_.marder_diffusion, p_.marder_loop, p_.marder_dump)
  {}

  // ----------------------------------------------------------------------
  // dtor

  ~Psc()
  {
    psc_destroy(psc_);
  }
  
  // ----------------------------------------------------------------------
  // initialize_stats
  
  void initialize_stats()
  {
    st_nr_particles = psc_stats_register("nr particles");
    st_time_step = psc_stats_register("time entire step");

    // generic stats categories
    st_time_particle = psc_stats_register("time particle update");
    st_time_field = psc_stats_register("time field update");
    st_time_comm = psc_stats_register("time communication");
    st_time_output = psc_stats_register("time output");
  }
  
  // ----------------------------------------------------------------------
  // initialize

  void initialize()
  {
    psc_view(psc_);
    mprts_.view();

    if (strcmp(psc_method_type(psc_->method), "vpic") == 0) {
      psc_method_vpic_initialize(psc_->method, psc_, mflds_, mprts_, p_.detailed_profiling);
    } else {
      initialize_default(psc_->method, psc_, mflds_, mprts_, p_.detailed_profiling);
    }

    psc_print_profiling(psc_, p_.detailed_profiling);

    mpi_printf(psc_comm(psc_), "Initialization complete.\n");
  }

  // ----------------------------------------------------------------------
  // integrate

  void integrate()
  {
    static int pr;
    if (!pr) {
      pr = prof_register("psc_step", 1., 0, 0);
    }

    mpi_printf(psc_comm(psc_), "*** Advancing\n");
    double elapsed = MPI_Wtime();

    bool first_iteration = true;
    while (psc_->timestep < p_.nmax) {
      prof_start(pr);
      psc_stats_start(st_time_step);

      if (!first_iteration &&
	  psc_->prm.write_checkpoint_every_step > 0 &&
	  psc_->timestep % psc_->prm.write_checkpoint_every_step == 0) {
	psc_write_checkpoint(psc_);
      }
      first_iteration = false;

      mpi_printf(psc_comm(psc_), "**** Step %d / %d, Code Time %g, Wall Time %g\n", psc_->timestep + 1,
		 p_.nmax, psc_->timestep * dt(), MPI_Wtime() - psc_->time_start);

      prof_start(pr_time_step_no_comm);
      prof_stop(pr_time_step_no_comm); // actual measurements are done w/ restart

      step();
    
      psc_->timestep++; // FIXME, too hacky
      psc_method_output(psc_->method, psc_, mflds_, mprts_);

      psc_stats_stop(st_time_step);
      prof_stop(pr);

      psc_stats_val[st_nr_particles] = mprts_.get_n_prts();

      if (psc_->timestep % psc_->prm.stats_every == 0) {
	psc_stats_log(psc_);
	psc_print_profiling(psc_, p_.detailed_profiling);
      }

      if (psc_->prm.wallclock_limit > 0.) {
	double wallclock_elapsed = MPI_Wtime() - psc_->time_start;
	double wallclock_elapsed_max;
	MPI_Allreduce(&wallclock_elapsed, &wallclock_elapsed_max, 1, MPI_DOUBLE, MPI_MAX,
		      MPI_COMM_WORLD);
      
	if (wallclock_elapsed_max > psc_->prm.wallclock_limit) {
	  mpi_printf(MPI_COMM_WORLD, "WARNING: Max wallclock time elapsed!\n");
	  break;
	}
      }
    }

    if (psc_->prm.write_checkpoint) {
      psc_write_checkpoint(psc_);
    }

    // FIXME, merge with existing handling of wallclock time
    elapsed = MPI_Wtime() - elapsed;

    int  s = (int)elapsed, m  = s/60, h  = m/60, d  = h/24, w = d/ 7;
    /**/ s -= m*60,        m -= h*60, h -= d*24, d -= w*7;
    mpi_printf(psc_comm(psc_), "*** Finished (%gs / %iw:%id:%ih:%im:%is elapsed)\n",
	       elapsed, w, d, h, m, s );
  }

  virtual void step() = 0;

  // ----------------------------------------------------------------------
  // set_dt
  
  static double set_dt(const PscParams& p, const Grid_t::Domain& domain)
  {
    double inv_sum = 0.;
    for (int d = 0; d < 3; d++) {
      if (!domain.isInvar(d)) {
	inv_sum += 1. / sqr(domain.dx[d]);
      }
    }
    if (!inv_sum) { // simulation has 0 dimensions
      inv_sum = 1.;
    }
    return p.cfl * sqrt(1./inv_sum);
  }
  
protected:
  double dt() const { return psc_->grid().dt; }

private:

  // ----------------------------------------------------------------------
  // initialize_default
  
  static void initialize_default(struct psc_method *method, struct psc *psc,
			  MfieldsBase& mflds, MparticlesBase& mprts, bool detailed_profiling)
  {
    //pushp_.stagger(mprts, mflds); FIXME, vpic does it
    
    // initial output / stats
    psc_method_output(psc->method, psc, mflds, mprts);
    psc_stats_log(psc);
  }

protected:
  PscParams p_;
  psc* psc_;

  Mparticles_t mprts_;
  Mfields_t mflds_;

  Balance_t balance_;
  Sort_t sort_;
  Collision_t collision_;
  PushParticles_t pushp_;
  PushFields_t pushf_;
  Bnd_t bnd_;
  BndFields_t bndf_;
  BndParticles_t bndp_;
  Checks_t checks_;
  Marder_t marder_;

  int st_nr_particles;
  int st_time_step;
};
