
#include <psc.h>
#include <psc.hxx>
#include <psc_push_fields.h>
#include <psc_bnd_fields.h>
#include <psc_sort.h>
#include <psc_balance.h>
#include <psc_particles_single.h>
#include <psc_fields_single.h>
#include <psc_method.h>
#ifdef USE_VPIC
#include "../libpsc/vpic/vpic_iface.h" // FIXME
#endif

#include "push_particles.hxx"
#include "push_fields.hxx"
#include "sort.hxx"
#include "collision.hxx"
#include "bnd_particles.hxx"
#include "balance.hxx"
#include "checks.hxx"
#include "marder.hxx"

#include "setup_particles.hxx"
#include "setup_fields.hxx"

#include "psc_config.hxx"

#include <mrc_params.h>

#include <math.h>

// ======================================================================
// PscBubbleParams

struct PscBubbleParams
{
  double BB;
  double nnb;
  double nn0;
  double MMach;
  double LLn;
  double LLB;
  double LLz;
  double LLy;
  double TTe;
  double TTi;
  double MMi;
  
  int sort_interval;

  int collision_interval;
  double collision_nu;

  int marder_interval;
  double marder_diffusion;
  int marder_loop;
  bool marder_dump;

  int balance_interval;
  double balance_factor_fields;
  bool balance_print_loads;
  bool balance_write_loads;

  ChecksParams checks_params;
};

using PscConfig = PscConfig1vbecSingle<dim_yz>;

// ======================================================================
// PscBubble

struct PscBubble : Psc<PscConfig>, PscBubbleParams
{
  using DIM = PscConfig::dim_t;
  using Mfields_t = PscConfig::Mfields_t;
  using Sort_t = PscConfig::Sort_t;
  using Collision_t = PscConfig::Collision_t;
  using PushParticles_t = PscConfig::PushParticles_t;
  using PushFields_t = PscConfig::PushFields_t;
  using BndParticles_t = PscConfig::BndParticles_t;
  using Bnd_t = PscConfig::Bnd_t;
  using BndFields_t = PscConfig::BndFields_t;
  using Balance_t = PscConfig::Balance_t;
  using Checks_t = PscConfig::Checks_t;
  using Marder_t = PscConfig::Marder_t;

  PscBubble(const PscBubbleParams& params, psc *psc, PscMparticlesBase mprts)
    : Psc{psc, mprts},
      PscBubbleParams(params),
      mprts_{dynamic_cast<Mparticles_t&>(*mprts.sub())},
      mflds_{dynamic_cast<Mfields_t&>(*PscMfieldsBase{psc->flds}.sub())},
      collision_{psc_comm(psc), collision_interval, collision_nu},
      bndp_{psc_->mrc_domain_, psc_->grid()},
      bnd_{psc_->grid(), psc_->mrc_domain_, psc_->ibn},
      balance_{balance_interval, balance_factor_fields, balance_print_loads, balance_write_loads},
      checks_{psc_->grid(), psc_comm(psc), checks_params},
      marder_(psc_comm(psc), marder_diffusion, marder_loop, marder_dump)
  {
    MPI_Comm comm = psc_comm(psc_);

    // --- partition particles and initial balancing
    mpi_printf(comm, "**** Partitioning...\n");
    auto n_prts_by_patch_old = setup_initial_partition();
    auto n_prts_by_patch_new = balance_.initial(psc_, n_prts_by_patch_old);
    // balance::initial does not rebalance particles, because the old way of doing this
    // does't even have the particle data structure created yet -- FIXME?
    mprts_.reset(psc_->grid());
    
    mpi_printf(comm, "**** Setting up particles...\n");
    setup_initial_particles(mprts_, n_prts_by_patch_new);
    
    mpi_printf(comm, "**** Setting up fields...\n");
    setup_initial_fields(mflds_);

    checks_.gauss(mprts_, mflds_);
    psc_setup_member_objs(psc_);
  
    initialize_stats();
  }

  void init_npt(int kind, double crd[3], psc_particle_npt& npt)
  {
    double V0 = MMach * sqrt(TTe / MMi);
    
    double r1 = sqrt(sqr(crd[2]) + sqr(crd[1] + .5 * LLy));
    double r2 = sqrt(sqr(crd[2]) + sqr(crd[1] - .5 * LLy));
    
    npt.n = nnb;
    if (r1 < LLn) {
      npt.n += (nn0 - nnb) * sqr(cos(M_PI / 2. * r1 / LLn));
      if (r1 > 0.0) {
	npt.p[2] += V0 * sin(M_PI * r1 / LLn) * crd[2] / r1;
	npt.p[1] += V0 * sin(M_PI * r1 / LLn) * (crd[1] + .5 * LLy) / r1;
      }
    }
    if (r2 < LLn) {
      npt.n += (nn0 - nnb) * sqr(cos(M_PI / 2. * r2 / LLn));
      if (r2 > 0.0) {
	npt.p[2] += V0 * sin(M_PI * r2 / LLn) * crd[2] / r2;
	npt.p[1] += V0 * sin(M_PI * r2 / LLn) * (crd[1] - .5 * LLy) / r2;
      }
    }
    
    switch (kind) {
    case 0: // electrons
      // electron drift consistent with initial current
      if ((r1 <= LLn) && (r1 >= LLn - 2.*LLB)) {
	npt.p[0] = - BB * M_PI/(2.*LLB) * cos(M_PI * (LLn-r1)/(2.*LLB)) / npt.n;
      }
      if ((r2 <= LLn) && (r2 >= LLn - 2.*LLB)) {
	npt.p[0] = - BB * M_PI/(2.*LLB) * cos(M_PI * (LLn-r2)/(2.*LLB)) / npt.n;
      }
      
      npt.T[0] = TTe;
      npt.T[1] = TTe;
      npt.T[2] = TTe;
      break;
    case 1: // ions
      npt.T[0] = TTi;
      npt.T[1] = TTi;
      npt.T[2] = TTi;
      break;
    default:
      assert(0);
    }
  }
  
  // ----------------------------------------------------------------------
  // setup_initial_partition
  
  std::vector<uint> setup_initial_partition()
  {
    return SetupParticles<Mparticles_t>::setup_partition(psc_, [&](int kind, double crd[3], psc_particle_npt& npt) {
	this->init_npt(kind, crd, npt);
      });
  }
  
  // ----------------------------------------------------------------------
  // setup_initial_particles
  
  void setup_initial_particles(Mparticles_t& mprts, std::vector<uint>& n_prts_by_patch)
  {
#if 0
    n_prts_by_patch[0] = 2;
    mprts.reserve_all(n_prts_by_patch.data());
    mprts.resize_all(n_prts_by_patch.data());

    for (int p = 0; p < mprts.n_patches(); p++) {
      mprintf("npp %d %d\n", p, n_prts_by_patch[p]);
      for (int n = 0; n < n_prts_by_patch[p]; n++) {
	auto &prt = mprts[p][n];
	prt.pxi = n;
	prt.kind_ = n % 2;
	prt.qni_wni_ = mprts.grid().kinds[prt.kind_].q;
      }
    };
#else
    SetupParticles<Mparticles_t>::setup_particles(mprts, psc_, n_prts_by_patch, [&](int kind, double crd[3], psc_particle_npt& npt) {
	this->init_npt(kind, crd, npt);
      });
#endif
  }

  // ----------------------------------------------------------------------
  // setup_initial_fields
  
  void setup_initial_fields(Mfields_t& mflds)
  {
    SetupFields<Mfields_t>::set(mflds, [&](int m, double crd[3]) {
	double z1 = crd[2];
	double y1 = crd[1] + .5 * LLy;
	double r1 = sqrt(sqr(z1) + sqr(y1));
	double z2 = crd[2];
	double y2 = crd[1] - .5 * LLy;
	double r2 = sqrt(sqr(z2) + sqr(y2));

	double rv = 0.;
	switch (m) {
	case HZ:
	  if ( (r1 < LLn) && (r1 > LLn - 2*LLB) ) {
	    rv += - BB * sin(M_PI * (LLn - r1)/(2.*LLB)) * y1 / r1;
	  }
	  if ( (r2 < LLn) && (r2 > LLn - 2*LLB) ) {
	    rv += - BB * sin(M_PI * (LLn - r2)/(2.*LLB)) * y2 / r2;
	  }
	  return rv;
	  
	case HY:
	  if ( (r1 < LLn) && (r1 > LLn - 2*LLB) ) {
	    rv += BB * sin(M_PI * (LLn - r1)/(2.*LLB)) * z1 / r1;
	  }
	  if ( (r2 < LLn) && (r2 > LLn - 2*LLB) ) {
	    rv += BB * sin(M_PI * (LLn - r2)/(2.*LLB)) * z2 / r2;
	  }
	  return rv;
	  
	case EX:
	  if ( (r1 < LLn) && (r1 > LLn - 2*LLB) ) {
	    rv += MMach * sqrt(TTe/MMi) * BB *
	      sin(M_PI * (LLn - r1)/(2.*LLB)) * sin(M_PI * r1 / LLn);
	  }
	  if ( (r2 < LLn) && (r2 > LLn - 2*LLB) ) {
	    rv += MMach * sqrt(TTe/MMi) * BB *
	      sin(M_PI * (LLn - r2)/(2.*LLB)) * sin(M_PI * r2 / LLn);
	  }
	  return rv;

	  // FIXME, JXI isn't really needed anymore (?)
	case JXI:
	  if ( (r1 < LLn) && (r1 > LLn - 2*LLB) ) {
	    rv += BB * M_PI/(2.*LLB) * cos(M_PI * (LLn - r1)/(2.*LLB));
	  }
	  if ( (r2 < LLn) && (r2 > LLn - 2*LLB) ) {
	    rv += BB * M_PI/(2.*LLB) * cos(M_PI * (LLn - r2)/(2.*LLB));
	  }
	  return rv;
	  
	default:
	  return 0.;
	}
      });
  }

  // ----------------------------------------------------------------------
  // step
  //
  // things are missing from the generic step():
  // - pushp prep

  void step()
  {
    static int pr_sort, pr_collision, pr_checks, pr_push_prts, pr_push_flds,
      pr_bndp, pr_bndf, pr_marder, pr_inject, pr_heating,
      pr_sync1, pr_sync2, pr_sync3, pr_sync4, pr_sync5, pr_sync4a, pr_sync4b;
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
      pr_sync1 = prof_register("step_sync1", 1., 0, 0);
      pr_sync2 = prof_register("step_sync2", 1., 0, 0);
      pr_sync3 = prof_register("step_sync3", 1., 0, 0);
      pr_sync4 = prof_register("step_sync4", 1., 0, 0);
      pr_sync5 = prof_register("step_sync5", 1., 0, 0);
      pr_sync4a = prof_register("step_sync4a", 1., 0, 0);
      pr_sync4b = prof_register("step_sync4b", 1., 0, 0);
    }

    // state is at: x^{n+1/2}, p^{n}, E^{n+1/2}, B^{n+1/2}
    MPI_Comm comm = psc_comm(psc_);
    int timestep = psc_->timestep;

    if (balance_interval > 0 && timestep % balance_interval == 0) {
      balance_(psc_, mprts_);
    }

    if (sort_interval > 0 && timestep % sort_interval == 0) {
      mpi_printf(comm, "***** Sorting...\n");
      prof_start(pr_sort);
      sort_(mprts_);
      prof_stop(pr_sort);
    }
    
    if (collision_interval > 0 && timestep % collision_interval == 0) {
      mpi_printf(comm, "***** Performing collisions...\n");
      prof_start(pr_collision);
      collision_(mprts_);
      prof_stop(pr_collision);
    }
    
    if (checks_params.continuity_every_step > 0 && timestep % checks_params.continuity_every_step == 0) {
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

#if 0
    prof_start(pr_sync1);
    MPI_Barrier(comm);
    prof_stop(pr_sync1);
#endif
    
    // === field propagation B^{n+1/2} -> B^{n+1}
    prof_start(pr_push_flds);
    pushf_.push_H(mflds_, .5, DIM{});
    prof_stop(pr_push_flds);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1}, j^{n+1}

    prof_start(pr_sync3);
    MPI_Barrier(comm);
    prof_stop(pr_sync3);

    prof_start(pr_bndp);
    bndp_(mprts_);
    prof_stop(pr_bndp);

    // === field propagation E^{n+1/2} -> E^{n+3/2}
#if 1
    prof_start(pr_bndf);
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
#endif

    bndf_.add_ghosts_J(mflds_);
    bnd_.add_ghosts(mflds_, JXI, JXI + 3);
    bnd_.fill_ghosts(mflds_, JXI, JXI + 3);
    prof_stop(pr_bndf);
    
#if 1
    prof_start(pr_sync4a);
    MPI_Barrier(comm);
    prof_stop(pr_sync4a);
#endif
    
    prof_restart(pr_push_flds);
    pushf_.push_E(mflds_, 1., DIM{});
    prof_stop(pr_push_flds);
    
#if 0
    prof_start(pr_sync4b);
    MPI_Barrier(comm);
    prof_stop(pr_sync4b);
#endif

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

#if 0
    prof_start(pr_sync5);
    MPI_Barrier(comm);
    prof_stop(pr_sync5);
#endif
    
    if (checks_params.continuity_every_step > 0 && timestep % checks_params.continuity_every_step == 0) {
      prof_restart(pr_checks);
      checks_.continuity_after_particle_push(mprts_, mflds_);
      prof_stop(pr_checks);
    }
    
    // E at t^{n+3/2}, particles at t^{n+3/2}
    // B at t^{n+3/2} (Note: that is not its natural time,
    // but div B should be == 0 at any time...)
    if (marder_interval > 0 && timestep % marder_interval == 0) {
      mpi_printf(comm, "***** Performing Marder correction...\n");
      prof_start(pr_marder);
      marder_(mflds_, mprts_);
      prof_stop(pr_marder);
    }
    
    if (checks_params.gauss_every_step > 0 && timestep % checks_params.gauss_every_step == 0) {
      prof_restart(pr_checks);
      checks_.gauss(mprts_, mflds_);
      prof_stop(pr_checks);
    }
    
    //psc_push_particles_prep(psc->push_particles, psc->particles, psc->flds);
  }

protected:
  Mparticles_t& mprts_;
  Mfields_t& mflds_;

  Sort_t sort_;
  Collision_t collision_;
  PushParticles_t pushp_;
  PushFields_t pushf_;
  BndParticles_t bndp_;
  Bnd_t bnd_;
  BndFields_t bndf_;
  Balance_t balance_;

  Checks_t checks_;
  Marder_t marder_;
};

// ======================================================================
// PscBubbleBuilder

struct PscBubbleBuilder
{
  PscBubble* makePsc();
};

// ----------------------------------------------------------------------
// PscBubbleBuilder::makePsc

PscBubble* PscBubbleBuilder::makePsc()
{
  auto psc_ = psc_create(MPI_COMM_WORLD);
  MPI_Comm comm = psc_comm(psc_);
  
  mpi_printf(comm, "*** Setting up...\n");

  PscBubbleParams params;

  psc_default_dimensionless(psc_);

  params.BB = .07;
  params.nnb = .1;
  params.nn0 = 1.;
  params.MMach = 3.;
  params.LLn = 200.;
  params.LLB = 200./6.;
  params.TTe = .02;
  params.TTi = .02;
  params.MMi = 100.;
    
  psc_->prm.nmax = 1000; //32000;
  psc_->prm.nicell = 100;

  params.LLy = 2. * params.LLn;
  params.LLz = 3. * params.LLn;

  auto grid_domain = Grid_t::Domain{{1, 128, 512},
				    {params.LLn, params.LLy, params.LLz},
				    {0., -.5 * params.LLy, -.5 * params.LLz},
				    {1, 1, 4}};
  
  auto grid_bc = GridBc{{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC }};

  psc_set_from_options(psc_);

  // sort
  params.sort_interval = 10;

  // collisions
  params.collision_interval = 10;
  params.collision_nu = .1;

  // --- checks
  params.checks_params.continuity_every_step = -1;
  params.checks_params.continuity_threshold = 1e-6;
  params.checks_params.continuity_verbose = true;
  params.checks_params.continuity_dump_always = false;

  params.checks_params.gauss_every_step = -1;
  params.checks_params.gauss_threshold = 1e-6;
  params.checks_params.gauss_verbose = true;
  params.checks_params.gauss_dump_always = false;

  // --- marder
  params.marder_interval = 0*5;
  params.marder_diffusion = 0.9;
  params.marder_loop = 3;
  params.marder_dump = false;

  // --- balancing
  params.balance_interval = 0;
  params.balance_factor_fields = 0.1;
  params.balance_print_loads = true;
  params.balance_write_loads = false;
  
  // --- generic setup
  psc_setup_coeff(psc_);
  psc_setup_domain(psc_, grid_domain, grid_bc, psc_->kinds_);

  // --- create and initialize base particle data structure x^{n+1/2}, p^{n+1/2}
  mpi_printf(comm, "**** Creating particle data structure...\n");
  auto mprts = PscMparticlesCreate(comm, psc_->grid(),
				   Mparticles_traits<PscBubble::Mparticles_t>::name);

  // --- create and set up base mflds
  psc_->flds = PscMfieldsCreate(comm, psc_->grid(), psc_->n_state_fields, psc_->ibn,
				Mfields_traits<PscBubble::Mfields_t>::name).mflds();

  mpi_printf(comm, "lambda_D = %g\n", sqrt(params.TTe));
  
  return new PscBubble{params, psc_, mprts};
}

// ======================================================================
// main

int
main(int argc, char **argv)
{
#ifdef USE_VPIC
  vpic_base_init(&argc, &argv);
#else
  MPI_Init(&argc, &argv);
#endif
  libmrc_params_init(argc, argv);
  mrc_set_flags(MRC_FLAG_SUPPRESS_UNPREFIXED_OPTION_WARNING);

  auto builder = PscBubbleBuilder{};
  auto psc = builder.makePsc();

  psc->initialize(psc->mprts);
  psc->integrate(psc->mprts);

  delete psc;
  
  libmrc_params_finalize();
  MPI_Finalize();

  return 0;
}
