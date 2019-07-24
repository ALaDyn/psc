
#include <psc.h>
#include <psc.hxx>

#include <balance.hxx>
#include <bnd.hxx>
#include <bnd_fields.hxx>
#include <bnd_particles.hxx>
#include <collision.hxx>
#include <fields3d.hxx>
#include <heating.hxx>
#include <inject.hxx>
#include <marder.hxx>
#include <particles.hxx>
#include <push_fields.hxx>
#include <push_particles.hxx>
#include <setup_fields.hxx>
#include <setup_particles.hxx>
#include <sort.hxx>

#include "../libpsc/psc_checks/checks_impl.hxx"
#include "../libpsc/psc_heating/psc_heating_impl.hxx"
#include "inject_impl.hxx"

#ifdef USE_CUDA
#include "../libpsc/cuda/setup_fields_cuda.hxx"
#endif

#include "psc_config.hxx"

#include "heating_spot_foil.hxx"

enum
{
  MY_ION,
  MY_ELECTRON,
  N_MY_KINDS,
};

// ======================================================================
// global parameters (FIXME?)

double BB;
double Zi;

double background_n;
double background_Te;
double background_Ti;

int inject_interval;

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

  InjectFoil(const InjectFoilParams& params) : InjectFoilParams(params) {}

  bool is_inside(double crd[3])
  {
    return (crd[1] >= yl && crd[1] <= yh && crd[2] >= zl && crd[2] <= zh);
  }

  void init_npt(int pop, double crd[3], psc_particle_npt& npt)
  {
    if (!is_inside(crd)) {
      npt.n = 0;
      return;
    }

    switch (pop) {
      case MY_ION:
        npt.n = n;
        npt.T[0] = Ti;
        npt.T[1] = Ti;
        npt.T[2] = Ti;
        break;
      case MY_ELECTRON:
        npt.n = n;
        npt.T[0] = Te;
        npt.T[1] = Te;
        npt.T[2] = Te;
        break;
      default:
        assert(0);
    }
  }
};

// EDIT to change order / floating point type / cuda / 2d/3d
using dim_t = dim_yz;
#ifdef USE_CUDA
using PscConfig = PscConfig1vbecCuda<dim_t>;
#else
using PscConfig = PscConfig1vbecSingle<dim_t>;
#endif

using MfieldsState = PscConfig::MfieldsState;
using Mparticles = PscConfig::Mparticles;
using Balance = PscConfig::Balance_t;
using Inject = typename InjectSelector<Mparticles, InjectFoil, dim_t>::Inject;

// ======================================================================
// PscFlatfoil

struct PscFlatfoil : Psc<PscConfig>
{
  using DIM = PscConfig::dim_t;
  using Heating_t = typename HeatingSelector<Mparticles>::Heating;

  // ----------------------------------------------------------------------
  // ctor

  PscFlatfoil(const PscParams& params, Grid_t& grid, MfieldsState& mflds,
              Mparticles& mprts, Balance& balance, Inject& inject,
              InjectFoil& inject_target)
    : inject_{inject}, inject_target_{inject_target}
  {
    auto comm = grid.comm();

    p_ = params;

    define_grid(grid);
    define_field_array(mflds);
    define_particles(mprts);

    balance_.reset(&balance);

    // -- Collision
    int collision_interval = 10;
    double collision_nu = .1;
    collision_.reset(new Collision_t{grid, collision_interval, collision_nu});

    // -- Checks
    ChecksParams checks_params{};
    checks_params.continuity_every_step = 50;
    checks_params.continuity_threshold = 1e-5;
    checks_params.continuity_verbose = false;
    checks_.reset(new Checks_t{grid, comm, checks_params});

    // -- Marder correction
    double marder_diffusion = 0.9;
    int marder_loop = 3;
    bool marder_dump = false;
    p_.marder_interval = 0 * 5;
    marder_.reset(
      new Marder_t(grid, marder_diffusion, marder_loop, marder_dump));

    double d_i = sqrt(grid.kinds[MY_ION].m / grid.kinds[MY_ION].q);

    mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., d_i);
    mpi_printf(comm, "lambda_De (background) = %g\n", sqrt(background_Te));

    // -- Heating
    auto heating_foil_params = HeatingSpotFoilParams{};
    heating_foil_params.zl = -1. * d_i;
    heating_foil_params.zh = 1. * d_i;
    heating_foil_params.xc = 0. * d_i;
    heating_foil_params.yc = 0. * d_i;
    heating_foil_params.rH = 3. * d_i;
    heating_foil_params.T = .04;
    heating_foil_params.Mi = grid.kinds[MY_ION].m;
    auto heating_spot = HeatingSpotFoil{heating_foil_params};

    heating_interval_ = 20;
    heating_begin_ = 0;
    heating_end_ = 10000000;
    heating_.reset(
      new Heating_t{grid, heating_interval_, MY_ELECTRON, heating_spot});

    // -- output fields
    OutputFieldsCParams outf_params{};
    outf_params.pfield_step = 200;
    std::vector<std::unique_ptr<FieldsItemBase>> outf_items;
    outf_items.emplace_back(
      new FieldsItemFields<ItemLoopPatches<Item_e_cc>>(grid));
    outf_items.emplace_back(
      new FieldsItemFields<ItemLoopPatches<Item_h_cc>>(grid));
    outf_items.emplace_back(
      new FieldsItemFields<ItemLoopPatches<Item_j_cc>>(grid));
    outf_items.emplace_back(
      new FieldsItemMoment<
        ItemMomentAddBnd<Moment_n_1st<Mparticles, MfieldsC>>>(grid));
    outf_items.emplace_back(
      new FieldsItemMoment<
        ItemMomentAddBnd<Moment_v_1st<Mparticles, MfieldsC>>>(grid));
    outf_items.emplace_back(
      new FieldsItemMoment<
        ItemMomentAddBnd<Moment_T_1st<Mparticles, MfieldsC>>>(grid));
    outf_.reset(new OutputFieldsC{grid, outf_params, std::move(outf_items)});

    // -- output particles
    OutputParticlesParams outp_params{};
    outp_params.every_step = 0;
    outp_params.data_dir = ".";
    outp_params.basename = "prt";
    outp_.reset(new OutputParticles{grid, outp_params});

    mpi_printf(comm, "**** Setting up fields...\n");
    setup_initial_fields(*mflds_);

    init();
  }

  void init_npt(int kind, double crd[3], psc_particle_npt& npt)
  {
    switch (kind) {
      case MY_ION:
        npt.n = background_n;
        npt.T[0] = background_Ti;
        npt.T[1] = background_Ti;
        npt.T[2] = background_Ti;
        break;
      case MY_ELECTRON:
        npt.n = background_n;
        npt.T[0] = background_Te;
        npt.T[1] = background_Te;
        npt.T[2] = background_Te;
        break;
      default:
        assert(0);
    }

    if (inject_target_.is_inside(crd)) {
      // replace values above by target values
      inject_target_.init_npt(kind, crd, npt);
    }
  }

  // ----------------------------------------------------------------------
  // setup_initial_partition

  template <typename F>
  static std::vector<uint> setup_initial_partition(const Grid_t& grid,
                                                   F&& init_npt)
  {
    SetupParticles<Mparticles> setup_particles;
    setup_particles.fractional_n_particles_per_cell =
      true; // FIXME, should use same setup_particles for partition/setup
    setup_particles.neutralizing_population = MY_ELECTRON;
    return setup_particles.setup_partition(grid, std::forward<F>(init_npt));
  }

  // ----------------------------------------------------------------------
  // setup_initial_particles

  template <typename F>
  static void setup_initial_particles(Mparticles& mprts,
                                      std::vector<uint>& n_prts_by_patch,
                                      F&& init_npt)
  {
    SetupParticles<Mparticles>
      setup_particles; // FIXME, injection uses another setup_particles, which
                       // won't have those settings
    setup_particles.fractional_n_particles_per_cell = true;
    setup_particles.neutralizing_population = MY_ELECTRON;
    setup_particles.setup_particles(mprts, n_prts_by_patch,
                                    std::forward<F>(init_npt));
  }

  // ----------------------------------------------------------------------
  // setup_initial_fields

  void setup_initial_fields(MfieldsState& mflds)
  {
    setupFields(grid(), mflds, [&](int m, double crd[3]) {
      switch (m) {
        case HY:
          return BB;
        default:
          return 0.;
      }
    });
  }

  // ----------------------------------------------------------------------
  // inject_particles

  void inject_particles() override
  {
    static int pr_inject, pr_heating;
    if (!pr_inject) {
      pr_inject = prof_register("inject", 1., 0, 0);
      pr_heating = prof_register("heating", 1., 0, 0);
    }

    auto comm = grid().comm();
    auto timestep = grid().timestep();

    if (inject_interval > 0 && timestep % inject_interval == 0) {
      mpi_printf(comm, "***** Performing injection...\n");
      prof_start(pr_inject);
      inject_(*mprts_);
      prof_stop(pr_inject);
    }

    // only heating between heating_tb and heating_te
    if (timestep >= heating_begin_ && timestep < heating_end_ &&
        heating_interval_ > 0 && timestep % heating_interval_ == 0) {
      mpi_printf(comm, "***** Performing heating...\n");
      prof_start(pr_heating);
      (*heating_)(*mprts_);
      prof_stop(pr_heating);
    }
  }

protected:
  std::unique_ptr<Heating_t> heating_;
  Inject& inject_;

private:
  InjectFoil& inject_target_;

  int heating_begin_;
  int heating_end_;
  int heating_interval_;
};

// ======================================================================
// main

int main(int argc, char** argv)
{
  psc_init(argc, argv);

  mpi_printf(MPI_COMM_WORLD, "*** Setting up...\n");

  PscParams psc_params;
  psc_params.nmax = 2001; // 5001;
  psc_params.cfl = 0.75;

  BB = 0.;
  Zi = 1.;
  double mass_ratio = 100.; // 25.

  // --- for background plasma
  background_n = .002;
  background_Te = .001;
  background_Ti = .001;

  // -- setup particle kinds
  // last population ("e") is neutralizing
  Grid_t::Kinds kinds = {{Zi, mass_ratio * Zi, "i"}, {-1., 1., "e"}};

  double d_i = sqrt(kinds[MY_ION].m / kinds[MY_ION].q);

  mpi_printf(MPI_COMM_WORLD, "d_e = %g, d_i = %g\n", 1., d_i);
  mpi_printf(MPI_COMM_WORLD, "lambda_De (background) = %g\n",
             sqrt(background_Te));

  // --- setup domain
#if 0
  Grid_t::Real3 LL = {384., 384.*2., 384.*6}; // domain size (in d_e)
  Int3 gdims = {384, 384*2, 384*6}; // global number of grid points
  Int3 np = {12, 24, 72}; // division into patches
#endif
#if 0
  Grid_t::Real3 LL = {192., 192.*2, 192.*6}; // domain size (in d_e)
  Int3 gdims = {192, 192*2, 192*6}; // global number of grid points
  Int3 np = {6, 12, 36}; // division into patches
  // -> patch size 32 x 32 x 32
#endif
#if 0
  Grid_t::Real3 LL = {32., 32.*2., 32.*6 }; // domain size (in d_e)
  Int3 gdims = {32, 32*2, 32*6}; // global number of grid points
  Int3 np = { 1, 2, 6 }; // division into patches
#endif
#if 0
  Grid_t::Real3 LL = {1., 1600., 400.}; // domain size (in d_e)
  // Int3 gdims = {40, 10, 20}; // global number of grid points
  // Int3 np = {4, 1, 2; // division into patches
  Int3 gdims = {1, 2048, 512}; // global number of grid points
  Int3 np = {1, 64, 16}; // division into patches
#endif
#if 1
  Grid_t::Real3 LL = {1., 800., 200.}; // domain size (in d_e)
  Int3 gdims = {1, 1024, 256};         // global number of grid points
  Int3 np = {1, 8, 2};                 // division into patches
#endif

  Grid_t::Domain grid_domain{gdims, LL, -.5 * LL, np};

  psc::grid::BC grid_bc{{BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC},
                        {BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC},
                        {BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC},
                        {BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC}};

  // --- generic setup
  auto norm_params = Grid_t::NormalizationParams::dimensionless();
  norm_params.nicell = 50;

  double dt = psc_params.cfl * courant_length(grid_domain);
  Grid_t::Normalization norm{norm_params};

  Int3 ibn = {2, 2, 2};
  if (dim_t::InvarX::value) {
    ibn[0] = 0;
  }
  if (dim_t::InvarY::value) {
    ibn[1] = 0;
  }
  if (dim_t::InvarZ::value) {
    ibn[2] = 0;
  }

  {
    auto grid_ptr = new Grid_t{grid_domain, grid_bc, kinds, norm, dt, -1, ibn};
    auto& grid = *grid_ptr;
    auto& mflds = *new MfieldsState{grid};
    auto& mprts = *new Mparticles{grid};

    // -- Balance
    psc_params.balance_interval = 300;
    auto& balance = *new Balance{psc_params.balance_interval, 3, true};

    // -- Sort
    psc_params.sort_interval = 10;

    // -- Particle injection
    InjectFoilParams inject_foil_params;
    inject_foil_params.yl = -100000. * d_i;
    inject_foil_params.yh = 100000. * d_i;
    double target_zwidth = 1.;
    inject_foil_params.zl = -target_zwidth * d_i;
    inject_foil_params.zh = target_zwidth * d_i;
    inject_foil_params.n = 1.;
    inject_foil_params.Te = .001;
    inject_foil_params.Ti = .001;
    InjectFoil inject_target{inject_foil_params};

    inject_interval = 20;
    int inject_tau = 40;
    Inject inject{grid, inject_interval, inject_tau, MY_ELECTRON,
                  inject_target};

    auto lf_init_npt = [&](int kind, double crd[3], psc_particle_npt& npt) {
      switch (kind) {
        case MY_ION:
          npt.n = background_n;
          npt.T[0] = background_Ti;
          npt.T[1] = background_Ti;
          npt.T[2] = background_Ti;
          break;
        case MY_ELECTRON:
          npt.n = background_n;
          npt.T[0] = background_Te;
          npt.T[1] = background_Te;
          npt.T[2] = background_Te;
          break;
        default:
          assert(0);
      }

      if (inject_target.is_inside(crd)) {
        // replace values above by target values
        inject_target.init_npt(kind, crd, npt);
      }
    };

    // --- partition particles and initial balancing
    mpi_printf(MPI_COMM_WORLD, "**** Partitioning...\n");
    auto n_prts_by_patch =
      PscFlatfoil::setup_initial_partition(grid, lf_init_npt);
    balance.initial(grid_ptr, n_prts_by_patch);
    // !!! FIXME! grid is now invalid
    // balance::initial does not rebalance particles, because the old way of
    // doing this does't even have the particle data structure created yet --
    // FIXME?
    mprts.reset(*grid_ptr);

    mpi_printf(MPI_COMM_WORLD, "**** Setting up particles...\n");
    PscFlatfoil::setup_initial_particles(mprts, n_prts_by_patch, lf_init_npt);
    

    PscFlatfoil psc{psc_params, *grid_ptr,   mflds,         mprts,
                    balance,    inject, inject_target};
    psc.initialize();
    psc.integrate();
  }

  libmrc_params_finalize();
  MPI_Finalize();

  return 0;
}
