
#pragma once

#include <mrc_profile.h>
#include <psc_diag.h>

#include <particles.hxx>

#include "../libpsc/vpic/fields_item_vpic.hxx"
#include <checks.hxx>
#include <output_fields_c.hxx>
#include <output_particles.hxx>
#include <push_particles.hxx>

#include "fields3d.inl"
#include "grid.inl"
#include "particles_simple.inl"
#include <kg/io.h>
#ifdef USE_CUDA
#include "../libpsc/cuda/mparticles_cuda.hxx"
#include "../libpsc/cuda/mparticles_cuda.inl"
#include "psc_fields_cuda.h"
#include "psc_fields_cuda.inl"
#endif

#ifdef VPIC
#include "../libpsc/vpic/vpic_iface.h"
#endif

#ifndef VPIC
struct MaterialList;
#endif

#ifdef VPIC

// FIXME, global variables are bad...

using VpicConfig = VpicConfigPsc;
using Mparticles = typename VpicConfig::Mparticles;
using MfieldsState = typename VpicConfig::MfieldsState;
using MfieldsHydro = typename VpicConfig::MfieldsHydro;
using MfieldsInterpolator = typename VpicConfig::MfieldsInterpolator;
using MfieldsAccumulator = typename VpicConfig::MfieldsAccumulator;
using Grid = typename MfieldsState::Grid;
using ParticleBcList = typename Mparticles::ParticleBcList;
using MaterialList = typename MfieldsState::MaterialList;
using Material = typename MaterialList::Material;
using OutputHydro =
  OutputHydroVpic<Mparticles, MfieldsHydro, MfieldsInterpolator>;
using DiagMixin =
  NoneDiagMixin<Mparticles, MfieldsState, MfieldsInterpolator, MfieldsHydro>;

Grid* vgrid;
std::unique_ptr<MfieldsHydro> hydro;
std::unique_ptr<MfieldsInterpolator> interpolator;
std::unique_ptr<MfieldsAccumulator> accumulator;
ParticleBcList particle_bc_list;
DiagMixin diag_mixin;

#endif

// ======================================================================
// PscParams

struct PscParams
{
  double cfl = .75;            // CFL number used to determine time step
  int nmax;                    // Number of timesteps to run
  double wallclock_limit = 0.; // Maximum wallclock time to run
  int write_checkpoint_every_step = 0;

  bool detailed_profiling =
    false;              // output profiling info for each process separately
  int stats_every = 10; // output timing and other info every so many steps

  int balance_interval = 0;
  int sort_interval = 0;
  int marder_interval = 0;
};

// ----------------------------------------------------------------------
// courant_length

inline double courant_length(const Grid_t::Domain& domain)
{
  double inv_sum = 0.;
  for (int d = 0; d < 3; d++) {
    if (!domain.isInvar(d)) {
      inv_sum += 1. / sqr(domain.dx[d]);
    }
  }
  if (!inv_sum) { // simulation has 0 dimensions (happens in some test?)
    inv_sum = 1.;
  }
  return sqrt(1. / inv_sum);
}

// ======================================================================
// Psc

template <typename PscConfig>
struct Psc
{
  using Mparticles = typename PscConfig::Mparticles;
  using MfieldsState = typename PscConfig::MfieldsState;
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
  using OutputParticles = typename PscConfig::OutputParticles;
  using Dim = typename PscConfig::dim_t;

#ifdef VPIC
  using AccumulateOps = typename PushParticles_t::AccumulateOps;
#endif

  // ----------------------------------------------------------------------
  // ctor

  Psc(const PscParams& params, Grid_t& grid, MfieldsState& mflds,
      Mparticles& mprts, Balance_t& balance, Collision_t& collision,
      Checks_t& checks, Marder_t& marder, OutputFieldsC& outf,
      OutputParticles& outp)
    : p_{params},
      grid_{&grid},
      mflds_{mflds},
      mprts_{mprts},
      balance_{balance},
      collision_{collision},
      checks_{checks},
      marder_{marder},
      outf_{outf},
      outp_{outp},
      bnd_{grid, grid.ibn},
      bndp_{grid}
  {
    time_start_ = MPI_Wtime();

    assert(grid.isInvar(0) == Dim::InvarX::value);
    assert(grid.isInvar(1) == Dim::InvarY::value);
    assert(grid.isInvar(2) == Dim::InvarZ::value);

    diag_ = psc_diag_create(MPI_COMM_WORLD);
    psc_diag_set_from_options(diag_);


    psc_diag_setup(diag_);

    initialize_stats();
    initialize();
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

    // FIXME not quite the right place
    pr_time_step_no_comm = prof_register("time step w/o comm", 1., 0, 0);
  }

  // ----------------------------------------------------------------------
  // initialize

  void initialize()
  {
#ifdef VPIC
    initialize_vpic();
#else
    initialize_default();
#endif

    // initial output / stats
    mpi_printf(grid().comm(), "Performing initial diagnostics.\n");
    diagnostics();
    print_status();

    mpi_printf(grid().comm(), "Initialization complete.\n");
  }

  // ----------------------------------------------------------------------
  // integrate

  void integrate()
  {
    static int pr;
    if (!pr) {
      pr = prof_register("psc_step", 1., 0, 0);
    }

    mpi_printf(grid().comm(), "*** Advancing\n");
    double elapsed = MPI_Wtime();

    bool first_iteration = true;
    while (grid().timestep() < p_.nmax) {
      prof_start(pr);
      psc_stats_start(st_time_step);

      if (!first_iteration && p_.write_checkpoint_every_step > 0 &&
          grid().timestep() % p_.write_checkpoint_every_step == 0) {
        write_checkpoint();
      }
      first_iteration = false;

      mpi_printf(grid().comm(),
                 "**** Step %d / %d, Code Time %g, Wall Time %g\n",
                 grid().timestep() + 1, p_.nmax, grid().timestep() * grid().dt,
                 MPI_Wtime() - time_start_);

      prof_start(pr_time_step_no_comm);
      prof_stop(
        pr_time_step_no_comm); // actual measurements are done w/ restart

      step();
      grid_->timestep_++; // FIXME, too hacky
#ifdef VPIC
      vgrid->step++;
      assert(vgrid->step == grid().timestep());
#endif

      diagnostics();

      psc_stats_stop(st_time_step);
      prof_stop(pr);

      psc_stats_val[st_nr_particles] = mprts_.size();

      if (grid().timestep() % p_.stats_every == 0) {
        print_status();
      }

      if (p_.wallclock_limit > 0.) {
        double wallclock_elapsed = MPI_Wtime() - time_start_;
        double wallclock_elapsed_max;
        MPI_Allreduce(&wallclock_elapsed, &wallclock_elapsed_max, 1, MPI_DOUBLE,
                      MPI_MAX, MPI_COMM_WORLD);

        if (wallclock_elapsed_max > p_.wallclock_limit) {
          mpi_printf(MPI_COMM_WORLD, "WARNING: Max wallclock time elapsed!\n");
          break;
        }
      }
    }

    if (p_.write_checkpoint_every_step > 0) {
      write_checkpoint();
    }

    // FIXME, merge with existing handling of wallclock time
    elapsed = MPI_Wtime() - elapsed;

    int s = (int)elapsed, m = s / 60, h = m / 60, d = h / 24, w = d / 7;
    /**/ s -= m * 60, m -= h * 60, h -= d * 24, d -= w * 7;
    mpi_printf(grid().comm(),
               "*** Finished (%gs / %iw:%id:%ih:%im:%is elapsed)\n", elapsed, w,
               d, h, m, s);
  }

#ifdef VPIC
  // ----------------------------------------------------------------------
  // step_vpic

  void step_vpic()
  {
    static int pr_sort, pr_collision, pr_checks, pr_push_prts, pr_push_flds,
      pr_bndp, pr_bndf, pr_marder, pr_inject, pr_heating;
    if (!pr_sort) {
      pr_sort = prof_register("step_sort", 1., 0, 0);
      pr_collision = prof_register("step_collision", 1., 0, 0);
      pr_push_prts = prof_register("step_push_prts", 1., 0, 0);
      pr_push_flds = prof_register("step_push_flds", 1., 0, 0);
      pr_bndp = prof_register("step_bnd_prts", 1., 0, 0);
      pr_bndf = prof_register("step_bnd_flds", 1., 0, 0);
      pr_checks = prof_register("step_checks", 1., 0, 0);
      pr_marder = prof_register("step_marder", 1., 0, 0);
      pr_inject = prof_register("step_inject", 1., 0, 0);
      pr_heating = prof_register("step_heating", 1., 0, 0);
    }

    MPI_Comm comm = grid().comm();

    // x^{n+1/2}, p^{n}, E^{n+1/2}, B^{n+1/2}

    int timestep = grid().timestep();

    if (p_.balance_interval > 0 && timestep % p_.balance_interval == 0) {
      balance_(grid_, mprts_);
    }

    prof_start(pr_time_step_no_comm);
    prof_stop(pr_time_step_no_comm); // actual measurements are done w/ restart

    if (p_.sort_interval > 0 && timestep % p_.sort_interval == 0) {
      // mpi_printf(comm, "***** Sorting...\n");
      prof_start(pr_sort);
      sort_(mprts_);
      prof_stop(pr_sort);
    }

    if (collision_.interval() > 0 && timestep % collision_.interval() == 0) {
      mpi_printf(comm, "***** Performing collisions...\n");
      prof_start(pr_collision);
      collision_(mprts_);
      prof_stop(pr_collision);
    }

    // psc_bnd_particles_open_calc_moments(psc_->bnd_particles,
    // psc_->particles);

    checks_.continuity_before_particle_push(mprts_);

    // === particle propagation p^{n} -> p^{n+1}, x^{n+1/2} -> x^{n+3/2}
    prof_start(pr_push_prts);
    pushp_.push_mprts(mprts_, mflds_, *interpolator, *accumulator,
                      particle_bc_list, num_comm_round);
    prof_stop(pr_push_prts);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1/2}, j^{n+1}

    // field propagation B^{n+1/2} -> B^{n+1}
    pushf_.push_H(mflds_, .5);
    // x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1}, j^{n+1}

    prof_start(pr_bndp);
    bndp_(mprts_);
    prof_stop(pr_bndp);

    // field propagation E^{n+1/2} -> E^{n+3/2}

    // fill ghosts for H
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);

    // add and fill ghost for J
    bndf_.add_ghosts_J(mflds_);
    bnd_.add_ghosts(mflds_, JXI, JXI + 3);
    bnd_.fill_ghosts(mflds_, JXI, JXI + 3);

    // push E
    pushf_.push_E(mflds_, 1.);

    bndf_.fill_ghosts_E(mflds_);
    // if (pushf_->variant == 0) {
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    //}
    // x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+1}

    // field propagation B^{n+1} -> B^{n+3/2}
    // if (pushf_->variant == 0) {
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    //    }

    // push H
    pushf_.push_H(mflds_, .5);

    bndf_.fill_ghosts_H(mflds_);
    // if (pushf_->variant == 0) {
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    //}
    // x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+3/2}

    checks_.continuity_after_particle_push(mprts_, mflds_);

    // E at t^{n+3/2}, particles at t^{n+3/2}
    // B at t^{n+3/2} (Note: that is not it's natural time,
    // but div B should be == 0 at any time...)
    if (p_.marder_interval > 0 && timestep % p_.marder_interval == 0) {
      // mpi_printf(comm, "***** Performing Marder correction...\n");
      prof_start(pr_marder);
      marder_(mflds_, mprts_);
      prof_stop(pr_marder);
    }

    checks_.gauss(mprts_, mflds_);

#ifdef VPIC
    pushp_.load_interpolator(mprts_, mflds_, *interpolator);
#endif
  }
#endif

  // ----------------------------------------------------------------------
  // step_psc

  void step_psc()
  {
    using DIM = typename PscConfig::dim_t;

    static int pr_sort, pr_collision, pr_checks, pr_push_prts, pr_push_flds,
      pr_bndp, pr_bndf, pr_marder, pr_inject_prts;
    if (!pr_sort) {
      pr_sort = prof_register("step_sort", 1., 0, 0);
      pr_collision = prof_register("step_collision", 1., 0, 0);
      pr_push_prts = prof_register("step_push_prts", 1., 0, 0);
      pr_inject_prts = prof_register("step_inject_prts", 1., 0, 0);
      pr_push_flds = prof_register("step_push_flds", 1., 0, 0);
      pr_bndp = prof_register("step_bnd_prts", 1., 0, 0);
      pr_bndf = prof_register("step_bnd_flds", 1., 0, 0);
      pr_checks = prof_register("step_checks", 1., 0, 0);
      pr_marder = prof_register("step_marder", 1., 0, 0);
    }

    // state is at: x^{n+1/2}, p^{n}, E^{n+1/2}, B^{n+1/2}
    MPI_Comm comm = grid().comm();
    int timestep = grid().timestep();

    if (p_.balance_interval > 0 && timestep % p_.balance_interval == 0) {
      balance_(grid_, mprts_);
    }

    if (p_.sort_interval > 0 && timestep % p_.sort_interval == 0) {
      mpi_printf(comm, "***** Sorting...\n");
      prof_start(pr_sort);
      sort_(mprts_);
      prof_stop(pr_sort);
    }

    if (collision_.interval() > 0 && timestep % collision_.interval() == 0) {
      mpi_printf(comm, "***** Performing collisions...\n");
      prof_start(pr_collision);
      collision_(mprts_);
      prof_stop(pr_collision);
    }

    // === particle injection
    prof_start(pr_inject_prts);
    inject_particles();
    prof_stop(pr_inject_prts);

    if (checks_.continuity_every_step > 0 &&
        timestep % checks_.continuity_every_step == 0) {
      mpi_printf(comm, "***** Checking continuity...\n");
      prof_start(pr_checks);
      checks_.continuity_before_particle_push(mprts_);
      prof_stop(pr_checks);
    }

    // === particle propagation p^{n} -> p^{n+1}, x^{n+1/2} -> x^{n+3/2}
    prof_start(pr_push_prts);
    pushp_.push_mprts(mprts_, mflds_);
    prof_stop(pr_push_prts);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1/2}, j^{n+1}

    // === field propagation B^{n+1/2} -> B^{n+1}
    prof_start(pr_push_flds);
    pushf_.push_H(mflds_, .5, DIM{});
    prof_stop(pr_push_flds);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1}, j^{n+1}

    prof_start(pr_bndp);
    bndp_(mprts_);
    prof_stop(pr_bndp);

    // === field propagation E^{n+1/2} -> E^{n+3/2}
    prof_start(pr_bndf);
#if 1
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
#endif

    bndf_.add_ghosts_J(mflds_);
    bnd_.add_ghosts(mflds_, JXI, JXI + 3);
    bnd_.fill_ghosts(mflds_, JXI, JXI + 3);
    prof_stop(pr_bndf);

    prof_restart(pr_push_flds);
    pushf_.push_E(mflds_, 1., DIM{});
    prof_stop(pr_push_flds);

#if 1
    prof_restart(pr_bndf);
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    prof_stop(pr_bndf);
#endif
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+1}

    // === field propagation B^{n+1} -> B^{n+3/2}
    prof_restart(pr_push_flds);
    pushf_.push_H(mflds_, .5, DIM{});
    prof_stop(pr_push_flds);

#if 1
    prof_start(pr_bndf);
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    prof_stop(pr_bndf);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+3/2}
#endif

    if (checks_.continuity_every_step > 0 &&
        timestep % checks_.continuity_every_step == 0) {
      prof_restart(pr_checks);
      checks_.continuity_after_particle_push(mprts_, mflds_);
      prof_stop(pr_checks);
    }

    // E at t^{n+3/2}, particles at t^{n+3/2}
    // B at t^{n+3/2} (Note: that is not its natural time,
    // but div B should be == 0 at any time...)
    if (p_.marder_interval > 0 && timestep % p_.marder_interval == 0) {
      mpi_printf(comm, "***** Performing Marder correction...\n");
      prof_start(pr_marder);
      marder_(mflds_, mprts_);
      prof_stop(pr_marder);
    }

    if (checks_.gauss_every_step > 0 &&
        timestep % checks_.gauss_every_step == 0) {
      prof_restart(pr_checks);
      checks_.gauss(mprts_, mflds_);
      prof_stop(pr_checks);
    }

    // psc_push_particles_prep(psc->push_particles, psc->particles, psc->flds);
  }

  virtual void step()
  {
#ifdef VPIC
    step_vpic();
#else
    step_psc();
#endif
  }

  // ----------------------------------------------------------------------
  // inject_particles

  virtual void inject_particles() {}

private:
  // ----------------------------------------------------------------------
  // print_profiling

  void print_profiling()
  {
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (!p_.detailed_profiling) {
      prof_print_mpi(MPI_COMM_WORLD);
    } else {
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      for (int i = 0; i < size; i++) {
        if (i == rank) {
          mprintf("profile\n");
          prof_print();
        }
        MPI_Barrier(MPI_COMM_WORLD);
      }
    }
  }

#ifndef VPIC
  // ----------------------------------------------------------------------
  // initialize_default

  void initialize_default()
  {
    // pushp_.stagger(mprts, mflds); FIXME, vpic does it

    checks_.gauss(mprts_, mflds_);
  }
#endif

#ifdef VPIC

  // ----------------------------------------------------------------------
  // initialize_vpic

  void initialize_vpic()
  {
    MPI_Comm comm = grid().comm();

    // Do some consistency checks on user initialized fields

    mpi_printf(comm, "Checking interdomain synchronization\n");
    double err = marder_.synchronize_tang_e_norm_b(mflds_);
    mpi_printf(comm, "Error = %g (arb units)\n", err);

    mpi_printf(comm, "Checking magnetic field divergence\n");
    marder_.compute_div_b_err(mflds_);
    err = marder_.compute_rms_div_b_err(mflds_);
    mpi_printf(comm, "RMS error = %e (charge/volume)\n", err);
    marder_.clean_div_b(mflds_);

    // Load fields not initialized by the user

    mpi_printf(comm, "Initializing radiation damping fields\n");
    TIC AccumulateOps::compute_curl_b(mflds_);
    TOC(compute_curl_b, 1);

    mpi_printf(comm, "Initializing bound charge density\n");
    marder_.clear_rhof(mflds_);
    marder_.accumulate_rho_p(mprts_, mflds_);
    marder_.synchronize_rho(mflds_);
    TIC AccumulateOps::compute_rhob(mflds_);
    TOC(compute_rhob, 1);

    // Internal sanity checks

    mpi_printf(comm, "Checking electric field divergence\n");
    marder_.compute_div_e_err(mflds_);
    err = marder_.compute_rms_div_e_err(mflds_);
    mpi_printf(comm, "RMS error = %e (charge/volume)\n", err);
    marder_.clean_div_e(mflds_);

    mpi_printf(comm, "Rechecking interdomain synchronization\n");
    err = marder_.synchronize_tang_e_norm_b(mflds_);
    mpi_printf(comm, "Error = %e (arb units)\n", err);

    mpi_printf(comm, "Uncentering particles\n");
    if (!mprts_.empty()) {
      pushp_.load_interpolator(mprts_, mflds_, *interpolator);
      pushp_.uncenter(mprts_, *interpolator);
    }
  }

#endif

  // ----------------------------------------------------------------------
  // diagnostics

  virtual void diagnostics()
  {
#ifdef VPIC
#if 0
    TIC user_diagnostics(); TOC(user_diagnostics, 1);
#endif
#else
    // FIXME
    psc_diag_run(diag_, mprts_, mflds_);
    // FIXME
    outf_(mflds_, mprts_);
#endif
    psc_stats_start(st_time_output);
    outp_.run(mprts_);
    psc_stats_stop(st_time_output);
  }

  // ----------------------------------------------------------------------
  // print_status

  void print_status()
  {
#ifdef VPIC
#ifdef USE_VPIC
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    update_profile(rank == 0);
#endif
#endif
    psc_stats_log(grid().timestep());
    print_profiling();
  }

  // ----------------------------------------------------------------------
  // write_checkpoint
  //

  void write_checkpoint()
  {
#if defined(PSC_HAVE_ADIOS2) && !defined(VPIC)
    MPI_Barrier(grid().comm());

    std::string filename =
      "checkpoint_" + std::to_string(grid().timestep()) + ".bp";

    auto io = kg::io::IOAdios2{};
    auto writer = io.open(filename, kg::io::Mode::Write);
    writer.put("grid", *grid_);
    writer.put("mflds", mflds_);
    writer.put("mprts", mprts_);
    writer.close();
#else
    std::cerr << "write_checkpoint not available without adios2" << std::endl;
    std::abort();
#endif
  }

  // ----------------------------------------------------------------------
  // read_checkpoint
  //

public:
  void read_checkpoint(const std::string& filename)
  {
#ifdef PSC_HAVE_ADIOS2
    MPI_Barrier(grid().comm());

    auto io = kg::io::IOAdios2{};
    auto reader = io.open(filename, kg::io::Mode::Read);
    reader.get("grid", *grid_);
    reader.get("mflds", mflds_);
    reader.get("mprts", mprts_);
    reader.close();
#else
    std::cerr << "write_checkpoint not available without adios2" << std::endl;
    std::abort();
#endif
  }

  const Grid_t& grid() { return *grid_; }

private:
  double time_start_;
  PscParams p_;

protected:
  Grid_t* grid_;

  MfieldsState& mflds_;
  Mparticles& mprts_;

  Balance_t& balance_;
  Collision_t& collision_;
  Checks_t& checks_;
  Marder_t& marder_;
  OutputFieldsC& outf_;
  OutputParticles& outp_;

  Sort_t sort_;
  PushParticles_t pushp_;
  PushFields_t pushf_;
  Bnd_t bnd_;
  BndFields_t bndf_;
  BndParticles_t bndp_;

  psc_diag* diag_; ///< timeseries diagnostics

  // FIXME, maybe should be private
  // need to make sure derived class sets these (? -- or just leave them off by
  // default)
  int num_comm_round = {3};

  int st_nr_particles;
  int st_time_step;
};

// ======================================================================
// VPIC-like stuff

// ----------------------------------------------------------------------
// define_periodic_grid

void define_periodic_grid(const double xl[3], const double xh[3],
                          const int gdims[3], const int np[3])
{
#ifdef VPIC
  // SimulationMixin::setTopology(np[0], np[1], np[2]); FIXME, needed for
  // vpic_simulation, I believe only because this info is written out in
  // diagnostics_run
  vgrid->partition_periodic_box(xl, xh, gdims, Int3::fromPointer(np));
#endif
}

// ----------------------------------------------------------------------
// set_domain_field_bc

void set_domain_field_bc(Int3 bnd, int bc)
{
#ifdef VPIC
  int boundary = BOUNDARY(bnd[0], bnd[1], bnd[2]);
  int fbc;
  switch (bc) {
    case BND_FLD_CONDUCTING_WALL: fbc = Grid::pec_fields; break;
    case BND_FLD_ABSORBING: fbc = Grid::absorb_fields; break;
    default: assert(0);
  }
  vgrid->set_fbc(boundary, fbc);
#endif
}

// ----------------------------------------------------------------------
// set_domain_particle_bc

void set_domain_particle_bc(Int3 bnd, int bc)
{
#ifdef VPIC
  int boundary = BOUNDARY(bnd[0], bnd[1], bnd[2]);
  int pbc;
  switch (bc) {
    case BND_PRT_REFLECTING: pbc = Grid::reflect_particles; break;
    case BND_PRT_ABSORBING: pbc = Grid::absorb_particles; break;
    default: assert(0);
  }
  vgrid->set_pbc(boundary, pbc);
#endif
}

void grid_setup_communication()
{
#ifdef VPIC
  assert(vgrid->nx && vgrid->ny && vgrid->ny);

  // Pre-size communications buffers. This is done to get most memory
  // allocation over with before the simulation starts running
  // FIXME, this isn't a great place. First, we shouldn't call mp
  // functions (semi-)directly. 2nd, whether we need these buffers depends
  // on b.c., which aren't yet known.

  // FIXME, this really isn't a good place to do this, as it requires layer
  // breaking knowledge of which communication will need the largest
  // buffers...
  int nx1 = vgrid->nx + 1, ny1 = vgrid->ny + 1, nz1 = vgrid->nz + 1;
  vgrid->mp_size_recv_buffer(
    BOUNDARY(-1, 0, 0), ny1 * nz1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_recv_buffer(
    BOUNDARY(1, 0, 0), ny1 * nz1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_recv_buffer(
    BOUNDARY(0, -1, 0), nz1 * nx1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_recv_buffer(
    BOUNDARY(0, 1, 0), nz1 * nx1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_recv_buffer(
    BOUNDARY(0, 0, -1), nx1 * ny1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_recv_buffer(
    BOUNDARY(0, 0, 1), nx1 * ny1 * sizeof(typename MfieldsHydro::Element));

  vgrid->mp_size_send_buffer(
    BOUNDARY(-1, 0, 0), ny1 * nz1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_send_buffer(
    BOUNDARY(1, 0, 0), ny1 * nz1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_send_buffer(
    BOUNDARY(0, -1, 0), nz1 * nx1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_send_buffer(
    BOUNDARY(0, 1, 0), nz1 * nx1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_send_buffer(
    BOUNDARY(0, 0, -1), nx1 * ny1 * sizeof(typename MfieldsHydro::Element));
  vgrid->mp_size_send_buffer(
    BOUNDARY(0, 0, 1), nx1 * ny1 * sizeof(typename MfieldsHydro::Element));
#endif
}

// ----------------------------------------------------------------------
// vpic_define_grid

void vpic_define_grid(const Grid_t& grid)
{
#ifdef VPIC
  auto domain = grid.domain;
  auto bc = grid.bc;
  auto dt = grid.dt;

  vgrid = Grid::create();
  vgrid->setup(domain.dx, dt, grid.norm.cc, grid.norm.eps0);

  // define the grid
  define_periodic_grid(domain.corner, domain.corner + domain.length,
                       domain.gdims, domain.np);

  // set field boundary conditions
  for (int p = 0; p < grid.n_patches(); p++) {
    assert(p == 0);
    for (int d = 0; d < 3; d++) {
      bool lo = grid.atBoundaryLo(p, d);
      bool hi = grid.atBoundaryHi(p, d);

      if (lo && bc.fld_lo[d] != BND_FLD_PERIODIC) {
        Int3 bnd = {0, 0, 0};
        bnd[d] = -1;
        set_domain_field_bc(bnd, bc.fld_lo[d]);
      }

      if (hi && bc.fld_hi[d] != BND_FLD_PERIODIC) {
        Int3 bnd = {0, 0, 0};
        bnd[d] = 1;
        set_domain_field_bc(bnd, bc.fld_hi[d]);
      }
    }
  }

  // set particle boundary conditions
  for (int p = 0; p < grid.n_patches(); p++) {
    assert(p == 0);
    for (int d = 0; d < 3; d++) {
      bool lo = grid.atBoundaryLo(p, d);
      bool hi = grid.atBoundaryHi(p, d);

      if (lo && bc.prt_lo[d] != BND_PRT_PERIODIC) {
        Int3 bnd = {0, 0, 0};
        bnd[d] = -1;
        set_domain_particle_bc(bnd, bc.prt_lo[d]);
      }

      if (hi && bc.prt_hi[d] != BND_PRT_PERIODIC) {
        Int3 bnd = {0, 0, 0};
        bnd[d] = 1;
        set_domain_particle_bc(bnd, bc.prt_hi[d]);
      }
    }
  }

  grid_setup_communication();
#endif
}

// ----------------------------------------------------------------------
// vpic_define_material

#ifdef VPIC
static Material* vpic_define_material(MaterialList& material_list,
                                      const char* name, double eps,
                                      double mu = 1., double sigma = 0.,
                                      double zeta = 0.)
{
  auto m = MaterialList::create(name, eps, eps, eps, mu, mu, mu, sigma, sigma,
                                sigma, zeta, zeta, zeta);
  return material_list.append(m);
}
#else
static void vpic_define_material(MaterialList& material_list, const char* name,
                                 double eps, double mu = 1., double sigma = 0.,
                                 double zeta = 0.)
{}
#endif

// ----------------------------------------------------------------------
// vpic_define_fields

void vpic_define_fields(const Grid_t& grid)
{
#ifdef VPIC
  hydro.reset(new MfieldsHydro{grid, vgrid});
  interpolator.reset(new MfieldsInterpolator{vgrid});
  accumulator.reset(new MfieldsAccumulator{vgrid});
#endif
}

// ----------------------------------------------------------------------
// vpic_create_diagnotics

void vpic_create_diagnostics(int interval)
{
#ifdef VPIC
  diag_mixin.diagnostics_init(interval);
#endif
}

// ----------------------------------------------------------------------
// vpic_setup_diagnostics

void vpic_setup_diagnostics()
{
#ifdef VPIC
  diag_mixin.diagnostics_setup();
#endif
}

#ifdef VPIC
// ----------------------------------------------------------------------
// vpic_run_diagnostics

void vpic_run_diagnostics(Mparticles& mprts, MfieldsState& mflds)
{
  diag_mixin.diagnostics_run(mprts, mflds, *interpolator, *hydro,
                             mprts.grid().domain.np);
}
#endif
