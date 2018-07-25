
#include <psc_config.h>

#include <psc.h>
#include <psc.hxx>
#include <psc_method.h>

// small 3d box (heating)
#define TEST_1_HEATING_3D 1
#define TEST_2_FLATFOIL_3D 2
#define TEST_3_NILSON_3D 3
#define TEST_4_SHOCK_3D 4

// EDIT to change test we're running (if TEST is not defined, default is regular 2d flatfoil)
//#define TEST TEST_1_HEATING_3D
//#define TEST TEST_2_FLATFOIL_3D
//#define TEST TEST_3_NILSON_3D
#define TEST TEST_4_SHOCK_3D

#ifdef USE_VPIC
#include "../libpsc/vpic/vpic_iface.h"
#endif

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
#include <marder.hxx>
#include <inject.hxx>
#include <heating.hxx>
#include <setup_particles.hxx>
#include <setup_fields.hxx>

#include "../libpsc/psc_inject/psc_inject_impl.hxx"
#include "../libpsc/psc_heating/psc_heating_impl.hxx"
#include "../libpsc/psc_checks/checks_impl.hxx"

#ifdef USE_CUDA
#include "../libpsc/cuda/inject_cuda_impl.hxx"
#include "../libpsc/cuda/heating_cuda_impl.hxx"
#include "../libpsc/cuda/setup_fields_cuda.hxx"
#include "../libpsc/cuda/setup_particles_cuda.hxx"
#endif

#include "psc_config.hxx"

#include "heating_spot_foil.hxx"

// ======================================================================
// HeatingSelector

template<typename Mparticles>
struct HeatingSelector
{
  using Heating = Heating__<Mparticles>;
};

#ifdef USE_CUDA

// FIXME, enable_if for any BS
template<>
struct HeatingSelector<MparticlesCuda<BS444>>
{
  using Mparticles = MparticlesCuda<BS444>;
  using Heating = HeatingCuda<typename Mparticles::BS>;
};

template<>
struct HeatingSelector<MparticlesCuda<BS144>>
{
  using Mparticles = MparticlesCuda<BS144>;
  using Heating = HeatingCuda<typename Mparticles::BS>;
};

#endif

// ======================================================================
// InjectSelector

template<typename Mparticles, typename Mfields, typename InjectShape, typename Dim>
struct InjectSelector
{
  using Inject = Inject_<Mparticles, MfieldsC, InjectShape>; // FIXME, shouldn't always use MfieldsC
};

#ifdef USE_CUDA

// FIXME, this should really be condition to Mparticles == MparticlesCuda<BS>, not
// Mfields == MfieldsCuda
template<typename Mparticles, typename InjectShape, typename Dim>
struct InjectSelector<Mparticles, MfieldsCuda, InjectShape, Dim>
{
  using Mfields = MfieldsCuda;
  using Inject = InjectCuda<typename Mparticles::BS, Dim, InjectShape>;
};

#endif


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
    : InjectFoilParams(params)
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
// PscFlatfoilParams

struct PscFlatfoilParams
{
  double BB;
  double Zi;

  double background_n;
  double background_Te;
  double background_Ti;

  int marder_interval;
  double marder_diffusion;
  int marder_loop;
  bool marder_dump;

  bool inject_enable;
  int inject_kind_n;
  int inject_interval;
  int inject_tau;
  InjectFoil inject_target;

  int heating_begin;
  int heating_end;
  int heating_interval;
  int heating_kind;
  HeatingSpotFoil heating_spot;

  ChecksParams checks_params;
};

// EDIT to change order / floating point type / cuda / 2d/3d
#if TEST == TEST_1_HEATING_3D || TEST == TEST_2_FLATFOIL_3D || TEST == TEST_3_NILSON_3D || TEST == TEST_4_SHOCK_3D
using dim_t = dim_xyz;
#else
using dim_t = dim_yz;
#endif
using PscConfig = PscConfig1vbecSingle<dim_t>;

// ======================================================================
// PscFlatfoil
//
// eventually, a Psc replacement / derived class, but for now just
// pretending to be something like that

struct PscFlatfoil : Psc<PscConfig>, PscFlatfoilParams
{
  using DIM = PscConfig::dim_t;
  using Heating_t = typename HeatingSelector<Mparticles_t>::Heating;
  using Inject_t = typename InjectSelector<Mparticles_t, Mfields_t, InjectFoil, DIM>::Inject;
  using Checks_t = PscConfig::Checks_t;
  using Marder_t = PscConfig::Marder_t;
  
  PscFlatfoil(const PscParams& p, const PscFlatfoilParams& params, psc *psc)
    : Psc{p, psc},
      PscFlatfoilParams(params),
      bndp_{psc_->mrc_domain_, psc_->grid()},
      heating_{heating_interval, heating_kind, heating_spot},
      inject_{psc_comm(psc), inject_interval, inject_tau, inject_kind_n, inject_target},
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
    switch (kind) {
    case MY_ION:
      npt.n    = background_n;
      npt.T[0] = background_Ti;
      npt.T[1] = background_Ti;
      npt.T[2] = background_Ti;
      break;
    case MY_ELECTRON:
      npt.n    = background_n;
      npt.T[0] = background_Te;
      npt.T[1] = background_Te;
      npt.T[2] = background_Te;
      break;
    default:
      assert(0);
    }
      
    if (inject_target.is_inside(crd)) {
      // replace values above by target values
      inject_target.init_npt(kind, crd, &npt);
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
	switch (m) {
	case HY: return BB;
	default: return 0.;
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

    if (p_.balance_interval > 0 && timestep % p_.balance_interval == 0) {
      balance_(psc_, mprts_);
    }

    if (p_.sort_interval > 0 && timestep % p_.sort_interval == 0) {
      mpi_printf(comm, "***** Sorting...\n");
      prof_start(pr_sort);
      sort_(mprts_);
      prof_stop(pr_sort);
    }
    
    if (p_.collision_interval > 0 && timestep % p_.collision_interval == 0) {
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
    
    if (inject_interval > 0 && timestep % inject_interval == 0) {
      mpi_printf(comm, "***** Performing injection...\n");
      prof_start(pr_inject);
      inject_(mprts_);
      prof_stop(pr_inject);
    }
      
    // only heating between heating_tb and heating_te
    if (timestep >= heating_begin && timestep < heating_end &&
	heating_interval > 0 && timestep % heating_interval == 0) {
      mpi_printf(comm, "***** Performing heating...\n");
      prof_start(pr_heating);
      heating_(mprts_);
      prof_stop(pr_heating);
    }

#if 1
    prof_start(pr_sync4);
    MPI_Barrier(comm);
    prof_stop(pr_sync4);
#endif
    
    prof_start(pr_bndp);
    bndp_(mprts_);
    prof_stop(pr_bndp);

    // === field propagation E^{n+1/2} -> E^{n+3/2}
#if 0
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
    
    prof_start(pr_sync4b);
    MPI_Barrier(comm);
    prof_stop(pr_sync4b);

#if 0
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

#if 0
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
  BndParticles_t bndp_;

  Heating_t heating_;
  Inject_t inject_;

  Checks_t checks_;
  Marder_t marder_;
};

// ======================================================================
// PscFlatfoilBuilder

struct PscFlatfoilBuilder
{
  using Heating_t = PscFlatfoil::Heating_t;

  PscFlatfoil* makePsc();
};

// ----------------------------------------------------------------------
// PscFlatfoilBuilder::makePsc

PscFlatfoil* PscFlatfoilBuilder::makePsc()
{
  auto psc_ = psc_create(MPI_COMM_WORLD);
  MPI_Comm comm = psc_comm(psc_);
  
  mpi_printf(comm, "*** Setting up...\n");

  PscParams p;
  PscFlatfoilParams params;

  psc_default_dimensionless(psc_);

  p.nmax = 5001;
  psc_->prm.nicell = 100;
  psc_->prm.fractional_n_particles_per_cell = true;
  p.cfl = 0.75;

  // --- setup domain
  Grid_t::Real3 LL = { 1., 400.*4, 400. }; // domain size (in d_e)
  Int3 gdims = { 1, 4096, 1024 }; // global number of grid points
  Int3 np = { 1, 64, 16 }; // division into patches

#if TEST == TEST_4_SHOCK_3D
  LL = { 256., 256., 512. }; // domain size (in d_e)
  gdims = { 256, 256, 512 }; // global number of grid points
  np = { 16, 16, 32 }; // division into patches
#endif
  
#if TEST == TEST_3_NILSON_3D
  LL = { 400., 200., 800. }; // domain size (in d_e)
  gdims = { 64, 32, 128 }; // global number of grid points
  np = { 2, 1, 4 }; // division into patches
#endif
  
#if TEST == TEST_2_FLATFOIL_3D
  LL = { 400., 400.*4, 400. }; // domain size (in d_e)
  gdims = { 16, 64, 16 }; // global number of grid points
  np = { 1, 4, 1 }; // division into patches
#endif
  
#if TEST == TEST_1_HEATING_3D
  LL = { 2., 2., 2. }; // domain size (in d_e)
  gdims = { 8, 8, 8 }; // global number of grid points
  np = { 1, 1, 1 }; // division into patches
#endif
  
  auto grid_domain = Grid_t::Domain{gdims, LL, -.5 * LL, np};

  auto grid_bc = GridBc{{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC }};

  psc_set_from_options(psc_);

  params.BB = 0.;
  params.Zi = 1.;

  // --- for background plasma
  params.background_n  = .002;
  params.background_Te = .001;
  params.background_Ti = .001;
  
  // -- setup particle kinds
  // last population ("e") is neutralizing
 // FIXME, hardcoded mass ratio 100
#if TEST == TEST_4_SHOCK_3D
  Grid_t::Kinds kinds = {{params.Zi, 100.*params.Zi, "i"}, { -1., 1., "e"}};
#else
  Grid_t::Kinds kinds = {{params.Zi, 100.*params.Zi, "i"}, { -1., 1., "e"}};
#endif
  psc_->prm.neutralizing_population = MY_ELECTRON;
  
  double d_i = sqrt(kinds[MY_ION].m / kinds[MY_ION].q);

  mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., d_i);
  mpi_printf(comm, "lambda_De (background) = %g\n", sqrt(params.background_Te));

  // sort
  p.sort_interval = 10;

  // collisions
  p.collision_interval = 10;
  p.collision_nu = .1;

  // --- setup heating
  double heating_zl = -1.;
  double heating_zh =  1.;
  double heating_xc = 0.;
  double heating_yc = 0.;
#if TEST == TEST_4_SHOCK_3D
  double heating_rH = 100000.;
#else
  double heating_rH = 3.;
#endif
  auto heating_foil_params = HeatingSpotFoilParams{};
  heating_foil_params.zl = heating_zl * d_i;
  heating_foil_params.zh = heating_zh * d_i;
  heating_foil_params.xc = heating_xc * d_i;
  heating_foil_params.yc = heating_yc * d_i;
  heating_foil_params.rH = heating_rH * d_i;
#if TEST == TEST_4_SHOCK_3D
  heating_foil_params.T  = .06;
#else
  heating_foil_params.T  = .04;
#endif
  heating_foil_params.Mi = kinds[MY_ION].m;
  params.heating_spot = HeatingSpotFoil{heating_foil_params};
  params.heating_interval = 20;
  params.heating_begin = 0;
  params.heating_end = 10000000;
  params.heating_kind = MY_ELECTRON;

  // -- setup injection
  double target_yl     = -100000.;
  double target_yh     =  100000.;
  double target_zwidth =  1.;
  auto inject_foil_params = InjectFoilParams{};
  inject_foil_params.yl =   target_yl * d_i;
  inject_foil_params.yh =   target_yh * d_i;
  inject_foil_params.zl = - target_zwidth * d_i;
  inject_foil_params.zh =   target_zwidth * d_i;
#if TEST == TEST_4_SHOCK_3D
  inject_foil_params.n  = 2.5;
  inject_foil_params.Te = .002;
  inject_foil_params.Ti = .002;
#else
  inject_foil_params.n  = 1.;
  inject_foil_params.Te = .001;
  inject_foil_params.Ti = .001;
#endif
  params.inject_target = InjectFoil{inject_foil_params};
  params.inject_kind_n = MY_ELECTRON;
  params.inject_interval = 20;
  params.inject_tau = 40;

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
  p.balance_interval = 0;
  p.balance_factor_fields = 0.1;
  p.balance_print_loads = true;
  p.balance_write_loads = false;

#if TEST == TEST_4_SHOCK_3D
  p.nmax = 100002;
  psc_->prm.nicell = 100;
  params.BB = 0.02;
  params.background_n = .01;
  params.background_Te = .002;
  params.background_Ti = .002;
  p.collision_interval = 0;
  params.inject_interval = 0;
#endif
  
#if TEST == TEST_3_NILSON_3D
  psc_->prm.nmax = 101;
  psc_->prm.nicell = 50;
  params.background_n = .02;
  p.collision_interval = 0;
  params.inject_interval = 0;
#endif
  
#if TEST == TEST_2_FLATFOIL_3D
  p.collision_interval = 0;
  params.heating_interval = 0;
  params.inject_interval = 5;
#endif
  
#if TEST == TEST_1_HEATING_3D
  params.background_n  = 1.0;

  p.collision_interval = 0;
  params.heating_interval = 0;
  params.inject_interval = 0;
  
  params.checks_params.continuity_every_step = 1;
  params.checks_params.continuity_threshold = 1e-12;
  params.checks_params.continuity_verbose = true;

  params.checks_params.gauss_every_step = 1;
  // eventually, errors accumulate above 1e-10, but it should take a long time
  params.checks_params.gauss_threshold = 1e-10;
  params.checks_params.gauss_verbose = true;
#endif

  // --- generic setup
  psc_setup_coeff(psc_);
  double dt = PscFlatfoil::set_dt(p, grid_domain);
  psc_setup_domain(psc_, grid_domain, grid_bc, kinds, dt);

  return new PscFlatfoil{p, params, psc_};
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

  auto builder = PscFlatfoilBuilder{};
  auto psc = builder.makePsc();

  psc->initialize();
  psc->integrate();

  delete psc;
  
  libmrc_params_finalize();
  MPI_Finalize();

  return 0;
}
