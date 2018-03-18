
#include <psc.h>
#include <psc_push_fields.h>
#include <psc_bnd_fields.h>

#ifdef USE_VPIC
#include "../libpsc/vpic/vpic_iface.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <psc_balance.h>
#include <psc_sort.h>
#include <psc_collision.h>
#include <psc_checks.h>
#include <psc_bnd_particles.h>
#include <psc_marder.h>
#include <psc_method.h>

#include <balance.hxx>
#include <particles.hxx>
#include <fields3d.hxx>
#include <push_particles.hxx>
#include <push_fields.hxx>
#include <sort.hxx>
#include <collision.hxx>
#include <bnd_particles.hxx>
#include <bnd.hxx>
#include <bnd_fields.hxx>
#include <inject.hxx>
#include <heating.hxx>
#include <setup_particles.hxx>
#include <setup_fields.hxx>

#include "psc_particles_double.h"
#include "psc_fields_c.h"
#include "../libpsc/psc_sort/psc_sort_impl.hxx"
#include "../libpsc/psc_collision/psc_collision_impl.hxx"
#include "../libpsc/psc_push_particles/push_config.hxx"
#include "../libpsc/psc_push_particles/push_dispatch.hxx"
#include "../libpsc/psc_push_particles/1vb/push_particles_1vbec_single.hxx"
#include "psc_push_fields_impl.hxx"
#include "bnd_particles_impl.hxx"
#include "../libpsc/psc_bnd/psc_bnd_impl.hxx"
#include "../libpsc/psc_bnd_fields/psc_bnd_fields_impl.hxx"
#include "../libpsc/psc_inject/psc_inject_impl.hxx"
#include "../libpsc/psc_heating/psc_heating_impl.hxx"
#include "../libpsc/psc_balance/psc_balance_impl.hxx"

enum {
  MY_ION,
  MY_ELECTRON,
  N_MY_KINDS,
};

// ======================================================================
// InjectFoil

struct InjectFoilParams
{
  double yl, yh;
  double zl, zh;
  double n;
  double Te, Ti;
};

struct InjectFoil : InjectFoilParams
{
  InjectFoil() = default;
  
  InjectFoil(const InjectFoilParams& params)
    : InjectFoilParams{params}
  {}

  bool is_inside(double crd[3])
  {
    return (crd[1] >= yl && crd[1] <= yh &&
	    crd[2] >= zl && crd[2] <= zh);
  }

  void init_npt(int pop, double crd[3], struct psc_particle_npt *npt)
  {
    if (!is_inside(crd)) {
      npt->n = 0;
      return;
    }
    
    switch (pop) {
    case MY_ION:
      npt->n    = n;
      npt->T[0] = Ti;
      npt->T[1] = Ti;
      npt->T[2] = Ti;
      break;
    case MY_ELECTRON:
      npt->n    = n;
      npt->T[0] = Te;
      npt->T[1] = Te;
      npt->T[2] = Te;
      break;
    default:
      assert(0);
    }
  }
};

// ======================================================================
// HeatingSpotFoil

struct HeatingSpotFoilParams
{
  double zl; // in internal units (d_e)
  double zh;
  double xc;
  double yc;
  double rH;
  double T;
  double Mi;
};

struct HeatingSpotFoil : HeatingSpotFoilParams
{
  HeatingSpotFoil() = default;
  
  HeatingSpotFoil(const HeatingSpotFoilParams& params)
    : HeatingSpotFoilParams{params}
  {
    double width = zh - zl;
    fac = (8.f * pow(T, 1.5)) / (sqrt(Mi) * width);
    // FIXME, I don't understand the sqrt(Mi) in here
  }
  
  double operator()(const double *crd)
  {
    double x = crd[0], y = crd[1], z = crd[2];

    if (z <= zl || z >= zh) {
      return 0;
    }
    
    return fac * exp(-(sqr(x-xc) + sqr(y-yc)) / sqr(rH));
  }

private:
  double fac;
};

// ======================================================================
// PscFlatfoilParams

struct PscFlatfoilParams
{
  double BB;
  double Zi;
  double LLf;
  double LLz;
  double LLy;

  double background_n;
  double background_Te;
  double background_Ti;

  int sort_interval;

  int collision_interval;
  double collision_nu;

  int balance_interval;
  double balance_factor_fields;
  bool balance_print_loads;
  bool balance_write_loads;

  bool inject_enable;
  int inject_kind_n;
  int inject_interval;
  int inject_tau;
  InjectFoil inject_target;
};

// ======================================================================
// psc subclass "flatfoil"

struct PscFlatfoil;

struct psc_flatfoil
{
  using Mparticles_t = MparticlesDouble;
  using Mfields_t = MfieldsC;
  using Heating_t = Heating__<Mparticles_t>;

  psc_flatfoil();
  
  void setup_initial_particles(PscMparticlesBase mprts, std::vector<uint>& n_prts_by_patch);
  void setup_initial_fields(PscMfieldsBase mflds);

  PscFlatfoil* makePscFlatfoil();

  PscFlatfoilParams params;
  
  // state
  double d_i;
  double LLs;
  double LLn;

  psc* psc_;
};

// ----------------------------------------------------------------------
// psc_flatfoil ctor

psc_flatfoil::psc_flatfoil()
  : psc_(psc_create(MPI_COMM_WORLD))
{}

// ----------------------------------------------------------------------
// psc_flatfoil::setup_initial_particles

void psc_flatfoil::setup_initial_particles(PscMparticlesBase mprts, std::vector<uint>& n_prts_by_patch)
{
  auto init_npt = [&](int kind, double crd[3], psc_particle_npt& npt) {
    switch (kind) {
    case MY_ION:
    npt.n    = params.background_n;
    npt.T[0] = params.background_Ti;
    npt.T[1] = params.background_Ti;
    npt.T[2] = params.background_Ti;
    break;
    case MY_ELECTRON:
    npt.n    = params.background_n;
    npt.T[0] = params.background_Te;
    npt.T[1] = params.background_Te;
    npt.T[2] = params.background_Te;
    break;
    default:
    assert(0);
    }

    if (params.inject_target.is_inside(crd)) {
      // replace values above by target values
      params.inject_target.init_npt(kind, crd, &npt);
    }
  };

  SetupParticles<MparticlesDouble>::setup_particles(mprts, ppsc, n_prts_by_patch, init_npt);
}

// ----------------------------------------------------------------------
// psc_flatfoil::setup_initial_fields

void psc_flatfoil::setup_initial_fields(PscMfieldsBase mflds)
{
  SetupFields<MfieldsC>::set(mflds, [&](int m, double crd[3]) {
      switch (m) {
      case HY: return params.BB;
      default: return 0.;
      }
    });
}

#define psc_flatfoil_(psc) mrc_to_subobj(psc, struct psc_flatfoil)

// ======================================================================
// PscFlatfoil
//
// eventually, a Psc replacement / derived class, but for now just
// pretending to be something like that

struct PscFlatfoil : PscFlatfoilParams
{
  using Mparticles_t = MparticlesDouble;
  using Mfields_t = MfieldsC;
#if 1 // generic_c
  using PushParticlesPusher_t = PushParticles__<Config2nd<dim_yz>>;
#else // 1vbec
  using PushParticlesPusher_t = PushParticles1vb<Config1vbec<Mparticles_t, Mfields_t, dim_yz>>;
#endif
  
  using Sort_t = SortCountsort2<Mparticles_t>;
  using Collision_t = Collision_<Mparticles_t, Mfields_t>;
  using PushFields_t = PushFields<Mfields_t>;
  using BndParticles_t = psc_bnd_particles_sub<Mparticles_t>;
  using Bnd_t = Bnd_<Mfields_t>;
  using BndFields_t = BndFieldsNone<Mfields_t>; // FIXME, why MfieldsC hardcoded???
  using Inject_t = Inject_<Mparticles_t, PscMfieldsC::sub_t, InjectFoil>; // FIXME, shouldn't always use MfieldsC
  using Heating_t = Heating__<Mparticles_t>;
  using Balance_t = Balance_<PscMparticles<Mparticles_t>, PscMfields<Mfields_t>>;
  
  PscFlatfoil(const PscFlatfoilParams& params, Heating_t heating, psc *psc)
    : PscFlatfoilParams{params},
      psc_{psc},
      mprts_{dynamic_cast<Mparticles_t&>(*PscMparticlesBase{psc->particles}.sub())},
      mflds_{dynamic_cast<Mfields_t&>(*PscMfieldsBase{psc->flds}.sub())},
      collision_{psc_comm(psc), collision_interval, collision_nu},
      bndp_{psc_->mrc_domain, psc_->grid()},
      bnd_{psc_->grid(), psc_->mrc_domain, psc_->ibn},
      balance_{balance_interval, balance_factor_fields, balance_print_loads, balance_write_loads},
      heating_{heating},
      inject_{psc_comm(psc), inject_enable, inject_interval, inject_tau, inject_kind_n, inject_target}
  {}
  
  // ----------------------------------------------------------------------
  // step
  //
  // things are missing from the generic step():
  // - timing
  // - psc_checks
  // - pushp prep
  // - marder

  void step()
  {
    // state is at: x^{n+1/2}, p^{n}, E^{n+1/2}, B^{n+1/2}
    MPI_Comm comm = psc_comm(psc_);
    int timestep = psc_->timestep;

    balance_(psc_, mprts_);
    
    if (sort_interval > 0 && timestep % sort_interval == 0) {
      mpi_printf(comm, "***** Sorting...\n");
      sort_(mprts_);
    }
    
    if (collision_interval > 0 && ppsc->timestep % collision_interval == 0) {
      mpi_printf(comm, "***** Performing collisions...\n");
      collision_(mprts_);
    }
    
    // === particle propagation p^{n} -> p^{n+1}, x^{n+1/2} -> x^{n+3/2}
    pushp_.push_mprts(mprts_, mflds_);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1/2}, j^{n+1}
    
    // === field propagation B^{n+1/2} -> B^{n+1}
    pushf_.push_H<dim_yz>(mflds_, .5);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1}, j^{n+1}

    bndp_(mprts_);
    
    inject_(mprts_);
    heating_(mprts_);
    
    // === field propagation E^{n+1/2} -> E^{n+3/2}
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    
    bndf_.add_ghosts_J(mflds_);
    bnd_.add_ghosts(mflds_, JXI, JXI + 3);
    bnd_.fill_ghosts(mflds_, JXI, JXI + 3);
    
    pushf_.push_E<dim_yz>(mflds_, 1.);
    
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+1}
    
    // === field propagation B^{n+1} -> B^{n+3/2}
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    
    pushf_.push_H<dim_yz>(mflds_, .5);
    
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+3/2}


    //psc_checks_continuity_after_particle_push(psc->checks, psc);

    // E at t^{n+3/2}, particles at t^{n+3/2}
    // B at t^{n+3/2} (Note: that is not it's natural time,
    // but div B should be == 0 at any time...)
    //psc_marder_run(psc->marder, psc->flds, psc->particles);
    
    //psc_checks_gauss(psc->checks, psc);

    //psc_push_particles_prep(psc->push_particles, psc->particles, psc->flds);
  }

  void setup()
  {
    setup_stats();
  }

  void setup_stats()
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
  // integrate

  void integrate()
  {
    //psc_method_initialize(psc_->method, psc_);
    psc_output(psc_);
    psc_stats_log(psc_);
    psc_print_profiling(psc_);

    mpi_printf(psc_comm(psc_), "Initialization complete.\n");

    static int pr;
    if (!pr) {
      pr = prof_register("psc_step", 1., 0, 0);
    }

    mpi_printf(psc_comm(psc_), "*** Advancing\n");
    double elapsed = MPI_Wtime();

    bool first_iteration = true;
    while (psc_->timestep < psc_->prm.nmax) {
      prof_start(pr);
      psc_stats_start(st_time_step);

      if (!first_iteration &&
	  psc_->prm.write_checkpoint_every_step > 0 &&
	  psc_->timestep % psc_->prm.write_checkpoint_every_step == 0) {
	psc_write_checkpoint(psc_);
      }
      first_iteration = false;

      mpi_printf(psc_comm(psc_), "**** Step %d / %d, Time %g\n", psc_->timestep + 1,
		 psc_->prm.nmax, psc_->timestep * psc_->dt);

      PscMparticlesBase mprts(psc_->particles);

      prof_start(pr_time_step_no_comm);
      prof_stop(pr_time_step_no_comm); // actual measurements are done w/ restart

      step();
    
      psc_->timestep++; // FIXME, too hacky
      psc_output(psc_);

      psc_stats_stop(st_time_step);
      prof_stop(pr);

      psc_stats_val[st_nr_particles] = mprts->get_n_prts();

      if (psc_->timestep % psc_->prm.stats_every == 0) {
	psc_stats_log(psc_);
	psc_print_profiling(psc_);
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

private:
  psc* psc_;
  Mparticles_t& mprts_;
  Mfields_t& mflds_;

  Sort_t sort_;
  Collision_t collision_;
  PushParticlesPusher_t pushp_;
  PushFields_t pushf_;
  BndParticles_t bndp_;
  Bnd_t bnd_;
  BndFields_t bndf_;
  Balance_t balance_;

  Heating_t heating_;
  Inject_t inject_;
  
  int st_nr_particles;
  int st_time_step;
};

// ----------------------------------------------------------------------
// psc_flatfoil::makePscFlatfoil

PscFlatfoil* psc_flatfoil::makePscFlatfoil()
{
  MPI_Comm comm = psc_comm(psc_);
  
  mpi_printf(comm, "*** Setting up...\n");

  psc_default_dimensionless(psc_);

  psc_->prm.nmax = 210001;
  psc_->prm.nicell = 100;
  psc_->prm.nr_populations = N_MY_KINDS;
  psc_->prm.fractional_n_particles_per_cell = true;
  psc_->prm.cfl = 0.75;

  // --- setup domain
  psc_->domain.gdims[0] = 1;
  psc_->domain.gdims[1] = 1600;
  psc_->domain.gdims[2] = 1600*4;

  params.LLf = 25.;
  params.LLy = 400.;
  params.LLz = 400. * 4.;

  LLs = 4. * params.LLf; // FIXME, unused?
  LLn = .5 * params.LLf; // FIXME, unused?

  psc_->domain.length[0] = 1.;
  psc_->domain.length[1] = params.LLy;
  psc_->domain.length[2] = params.LLz;

  // center around origin
  for (int d = 0; d < 3; d++) {
    psc_->domain.corner[d] = -.5 * psc_->domain.length[d];
  }

  psc_->domain.bnd_fld_lo[0] = BND_FLD_PERIODIC;
  psc_->domain.bnd_fld_hi[0] = BND_FLD_PERIODIC;
  psc_->domain.bnd_fld_lo[1] = BND_FLD_PERIODIC;
  psc_->domain.bnd_fld_hi[1] = BND_FLD_PERIODIC;
  psc_->domain.bnd_fld_lo[2] = BND_FLD_PERIODIC;
  psc_->domain.bnd_fld_hi[2] = BND_FLD_PERIODIC;
  psc_->domain.bnd_part_lo[0] = BND_PART_PERIODIC;
  psc_->domain.bnd_part_hi[0] = BND_PART_PERIODIC;
  psc_->domain.bnd_part_lo[1] = BND_PART_PERIODIC;
  psc_->domain.bnd_part_hi[1] = BND_PART_PERIODIC;
  psc_->domain.bnd_part_lo[2] = BND_PART_PERIODIC;
  psc_->domain.bnd_part_hi[2] = BND_PART_PERIODIC;

  psc_set_from_options(psc_);

  params.BB = 0.;
  params.Zi = 1.;

  // --- for background plasma
  params.background_n  = .002;
  params.background_Te = .001;
  params.background_Ti = .001;
  
  // -- setup particles
  // last population is neutralizing
  psc_->kinds[MY_ELECTRON].q = -1.;
  psc_->kinds[MY_ELECTRON].m = 1.;
  psc_->kinds[MY_ELECTRON].name = strdup("e");

  psc_->kinds[MY_ION     ].q = params.Zi;
  psc_->kinds[MY_ION     ].m = 100. * params.Zi;  // FIXME, hardcoded mass ratio 100
  psc_->kinds[MY_ION     ].name = strdup("i");

  d_i = sqrt(psc_->kinds[MY_ION].m / psc_->kinds[MY_ION].q);

  mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., d_i);
  mpi_printf(comm, "lambda_De (background) = %g\n", sqrt(params.background_Te));
  // sort
  params.sort_interval = 10;

  // collisions
  params.collision_interval = 10;
  params.collision_nu = .1;

  // --- setup heating
  double heating_zl = -1.;
  double heating_zh =  1.;
  double heating_xc = 0.;
  double heating_yc = 0.;
  double heating_rH = 3.;
  auto heating_foil_params = HeatingSpotFoilParams{};
  heating_foil_params.zl = heating_zl * d_i;
  heating_foil_params.zh = heating_zh * d_i;
  heating_foil_params.xc = heating_xc * d_i;
  heating_foil_params.yc = heating_yc * d_i;
  heating_foil_params.rH = heating_rH * d_i;
  heating_foil_params.T  = .04;
  heating_foil_params.Mi = heating_rH * psc_->kinds[MY_ION].m;
  auto heating_spot = HeatingSpotFoil{heating_foil_params};
  int heating_interval = 20;
  int heating_begin = 0;
  int heating_end = 10000000;
  int heating_kind = MY_ELECTRON;
  auto heating = Heating_t{heating_interval, heating_begin, heating_end,
			   heating_kind, heating_spot};

  // -- setup injection
  double target_yl     = -100000.;
  double target_yh     =  100000.;
  double target_zwidth =  1.;
  auto inject_foil_params = InjectFoilParams{};
  inject_foil_params.yl =   target_yl * d_i;
  inject_foil_params.yh =   target_yh * d_i;
  inject_foil_params.zl = - target_zwidth * d_i;
  inject_foil_params.zh =   target_zwidth * d_i;
  inject_foil_params.n  = 1.;
  inject_foil_params.Te = .001;
  inject_foil_params.Ti = .001;
  params.inject_target = InjectFoil{inject_foil_params};
  params.inject_enable = true;
  params.inject_kind_n = MY_ELECTRON;
  params.inject_interval = 20;
  params.inject_tau = 40;

  // --- balancing
  params.balance_interval = 100;
  params.balance_factor_fields = 1.;
  params.balance_print_loads = true;
  params.balance_write_loads = false;

  // --- generic setup
  psc_setup_coeff(psc_);
  psc_setup_domain(psc_);

  // --- partition particles and initial balancing
  mpi_printf(comm, "**** Partitioning...\n");
  auto n_prts_by_patch_old = SetupParticles<MparticlesDouble>::setup_partition(psc_);
  psc_balance_setup(psc_->balance);
  auto balance = PscBalanceBase{psc_->balance};
  auto n_prts_by_patch_new = balance.initial(psc_, n_prts_by_patch_old);

  // --- create and initialize base particle data structure x^{n+1/2}, p^{n+1/2}
  mpi_printf(comm, "**** Setting up particles...\n");
  psc_->particles = PscMparticlesCreate(comm, psc_->grid(), psc_->prm.particles_base).mprts();
  setup_initial_particles(PscMparticlesBase{psc_->particles}, n_prts_by_patch_new);

  // --- create and set up base mflds
  mpi_printf(comm, "**** Setting up fields...\n");
  psc_->flds = PscMfieldsCreate(comm, psc_->grid(), psc_->n_state_fields, psc_->ibn,
				psc_->prm.fields_base).mflds();
  setup_initial_fields(psc_->flds);

  psc_setup_member_objs(psc_);

  return new PscFlatfoil(params, heating, psc_);
}

// ----------------------------------------------------------------------
// psc_ops "flatfoil"

struct psc_ops_flatfoil : psc_ops {
  psc_ops_flatfoil() {
    name             = "flatfoil";
  }
} psc_flatfoil_ops;

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

  mrc_class_register_subclass(&mrc_class_psc, &psc_flatfoil_ops);

  psc_flatfoil *sim = new psc_flatfoil{};

  auto flatfoil = sim->makePscFlatfoil();
  
  psc_view(sim->psc_);
  psc_mparticles_view(sim->psc_->particles);
  psc_mfields_view(sim->psc_->flds);
  
  flatfoil->setup();
  flatfoil->integrate();
  
  delete flatfoil;
  
  psc_destroy(sim->psc_);
  
  libmrc_params_finalize();
  MPI_Finalize();

  return 0;
}

