
#include "gtest/gtest.h"

#include "testing.hxx"
#include "../libpsc/psc_collision/psc_collision_impl.hxx"
#include "setup_particles.hxx"
#include "psc_particles_single.h"
#include "psc_fields_single.h"

#ifdef USE_CUDA
#include "../libpsc/cuda/collision_cuda_impl.hxx"
#include "../libpsc/cuda/setup_particles_cuda.hxx"
#endif

struct TestParticle
{
  using real_t = double;

  real_t q() const { return q_; }
  real_t m() const { return m_; }
  real_t  u(int d) const { return u_[d]; }
  real_t& u(int d)       { return u_[d]; }

  Vec3<real_t> u_;
  real_t q_;
  real_t m_;
};

TEST(BinaryCollision, Test1)
{
  double eps = 1e-14;
  
  TestParticle prt1{{ 1., 0., 0.}, 1., 1. };
  TestParticle prt2{{ 0., 0., 0.}, 1., 1. };

  RngFake rng;
  BinaryCollision<TestParticle> bc;

  double nudt = bc(prt1, prt2, .1, rng);

  EXPECT_NEAR(prt1.u(0) + prt2.u(0), 1., eps);
  EXPECT_NEAR(prt1.u(1) + prt2.u(1), 0., eps);
  EXPECT_NEAR(prt1.u(2) + prt2.u(2), 0., eps);

#if 0
  printf("prt1: %g %g %g\n", prt1.u(0), prt1.u(1), prt1.u(2));
  printf("prt2: %g %g %g\n", prt2.u(0), prt2.u(1), prt2.u(2));
#endif
}

// ======================================================================
// make_psc
//
// FIXME, duplicated in various testing environments

template<typename dim>
static void make_psc(const Grid_t::Kinds& kinds)
{
  Int3 gdims = {16, 16, 16};
  Int3 ibn = {2, 2, 2};
  Vec3<double> length = { 160., 160., 160. };
  if (dim::InvarX::value) { gdims[0] = 1; ibn[0] = 0; }
  if (dim::InvarY::value) { gdims[1] = 1; ibn[1] = 0; }
  if (dim::InvarZ::value) { gdims[2] = 1; ibn[2] = 0; }
  
  auto grid_domain = Grid_t::Domain{gdims, length};
  auto grid_bc = GridBc{{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC },
			{ BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC }};
  
  auto psc = psc_create(MPI_COMM_WORLD); // to create ppsc, mostly
  psc_default_dimensionless(psc);
  psc_setup_coeff(psc);
  
  psc_setup_domain(psc, grid_domain, grid_bc, kinds);
  
  psc->dt = psc->grid_->dt = 1.;
}

// ======================================================================
// CollisionTest

template<typename DIM, typename COLLISION>
struct CollisionTestConfig
{
  using dim = DIM;
  using Collision = COLLISION;
  using Mparticles = typename Collision::Mparticles;
};

template <typename T>
class CollisionTest : public ::testing::Test
{};

using CollisionTestConfigSingle = CollisionTestConfig<dim_yz, CollisionHost<MparticlesSingle, MfieldsSingle, RngFake>>;
using CollisionTestConfigDouble = CollisionTestConfig<dim_yz, CollisionHost<MparticlesDouble, MfieldsC, RngFake>>;
using CollisionTestConfigCuda = CollisionTestConfig<dim_yz, CollisionCuda<BS144, RngStateFake>>;

using CollisionTestTypes = ::testing::Types<CollisionTestConfigSingle,
					    CollisionTestConfigDouble,
					    CollisionTestConfigCuda>;

TYPED_TEST_CASE(CollisionTest, CollisionTestTypes);
  
TYPED_TEST(CollisionTest, Test1)
{
  using Config = TypeParam;
  using dim = typename Config::dim;
  using Mparticles = typename Config::Mparticles;
  using particle_t = typename Mparticles::particle_t;
  using Collision = typename Config::Collision;
  const typename Mparticles::real_t eps = 1e-5;

  auto kinds = Grid_t::Kinds{Grid_t::Kind(1., 1., "test_species")};
  make_psc<dim>(kinds);
  const auto& grid = ppsc->grid();
  
  // init particles
  particle_t prt0;
  prt0.xi = 5.; prt0.yi = 5.; prt0.zi = 5.;
  prt0.qni_wni_ = 1.;
  prt0.pxi = 1.; prt0.pyi = 0.; prt0.pzi = 0.;
  prt0.kind_ = 0;

  particle_t prt1;
  prt1.xi = 5.; prt1.yi = 5.; prt1.zi = 5.;
  prt1.qni_wni_ = 1.;
  prt1.pxi = 0.; prt1.pyi = 0.; prt1.pzi = 0.;
  prt1.kind_ = 0;

  std::vector<particle_t> prts = { prt0, prt1 };

  auto n_prts_by_patch = std::vector<uint>{2};
  Mparticles mprts{grid};
  SetupParticles<Mparticles>::setup_particles(mprts, n_prts_by_patch, [&](int p, int n) -> typename Mparticles::particle_t {
      return prts[n];
    });

  auto collision = Collision(psc_comm(ppsc), 1, 1.);

  collision(mprts);

  auto it = make_getter(mprts)[0].begin();
  auto prtf0 = *it++;
  auto prtf1 = *it++;
  EXPECT_NEAR(prtf0.pxi + prtf1.pxi, 1., eps);
  EXPECT_NEAR(prtf0.pyi + prtf1.pyi, 0., eps);
  EXPECT_NEAR(prtf0.pzi + prtf1.pzi, 0., eps);

  // depends on random numbers, but for RngFake, we know
  EXPECT_NEAR(prtf0.pxi,  0.96226911, eps);
  EXPECT_NEAR(prtf0.pyi,  0.        , eps);
  EXPECT_NEAR(prtf0.pzi, -0.17342988, eps);
  EXPECT_NEAR(prtf1.pxi,  0.03773088, eps);
  EXPECT_NEAR(prtf1.pyi, -0.        , eps);
  EXPECT_NEAR(prtf1.pzi,  0.17342988, eps);
  
  ppsc = NULL; // FIXME
}

// ======================================================================
// main

int main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();
  MPI_Finalize();
  return rc;
}
