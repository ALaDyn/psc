
#include "grid.hxx"
#include "fields.hxx"
#include "cuda_mfields.h"
#include "cuda_mparticles.h"

#include "../vpic/PscRng.h"

#include "gtest/gtest.h"

// Rng hackiness

using Rng = PscRng;
using RngPool = PscRngPool<Rng>;

// enum hackiness

enum IP { // FIXME, dup
  IP_STD, // standard interpolation
  IP_EC,  // energy-conserving interpolation
};

enum DEPOSIT { // FIXME, dup
  DEPOSIT_VB_2D,
  DEPOSIT_VB_3D,
};

enum CURRMEM { // FIXME, dup
  CURRMEM_SHARED,
  CURRMEM_GLOBAL,
};

// profile hackiness

#include "mrc_profile.h"

struct prof_globals prof_globals; // FIXME

int
prof_register(const char *name, float simd, int flops, int bytes)
{
  return 0;
}

// ======================================================================
// class PushMprtsTest

struct PushMprtsTestParam
{
  double q;
  double m;
};

// ======================================================================
// class TestAccel

class TestAccel
{
  enum { // FIXME, duplicated
    JXI, JYI, JZI,
    EX , EY , EZ ,
    HX , HY , HZ ,
    N_FIELDS,
  };

public:
  TestAccel()
    : grid_({ 1, 1, 1 }, { L, L, L })
  {
  }

  ~TestAccel()
  {
    delete cmflds_;
  }

  void init_cmflds()
  {
    cmflds_ = new cuda_mfields(grid_, N_FIELDS, { 0, 2, 2 });
    fields_single_t flds = cmflds_->get_host_fields();
    Fields3d<fields_single_t> F(flds);

    F(EX, 0,0,0) = 1;
    F(EX, 0,1,0) = 1;
    F(EX, 0,0,1) = 1;
    F(EX, 0,1,1) = 1;
    
    F(EY, 0,0,0) = 2;
    F(EY, 0,0,1) = 2;
    //    F(EY, 1,0,0) = 2;
    //    F(EY, 1,0,1) = 2;
    
    F(EZ, 0,0,0) = 3;
    //    F(EZ, 1,0,0) = 3;
    F(EZ, 0,1,0) = 3;
    //    F(EZ, 1,1,0) = 3;

    cmflds_->copy_to_device(0, flds, 0, N_FIELDS);
    cmflds_->dump("accel.fld.json");
    flds.dtor();
  };

  double L = 1e10;
  unsigned int n_prts = 131;
  int n_steps = 10;
  cuda_mparticles::real_t eps = 1e-5;
  Int3 bs_ = { 1, 1, 1 };
  
  Grid_t grid_;
  cuda_mfields* cmflds_;
};

// ======================================================================
// class TestCyclo

class TestCyclo
{
  enum { // FIXME, duplicated
    JXI, JYI, JZI,
    EX , EY , EZ ,
    HX , HY , HZ ,
    N_FIELDS,
  };

public:
  TestCyclo()
    : grid_({ 1, 1, 1 }, { L, L, L })
  {
  }

  ~TestCyclo()
  {
    delete cmflds_;
  }

  void init_cmflds()
  {
    cmflds_ = new cuda_mfields(grid_, N_FIELDS, { 0, 2, 2 });
    fields_single_t flds = cmflds_->get_host_fields();
    Fields3d<fields_single_t> F(flds);

    F(HZ, 0,0,0) = 2. * M_PI / n_steps;
    F(HZ, 0,0,1) = 2. * M_PI / n_steps;

    cmflds_->copy_to_device(0, flds, 0, N_FIELDS);
    cmflds_->dump("cyclo.fld.json");
    flds.dtor();
  };

  double L = 1e10;
  unsigned int n_prts = 131;
  int n_steps = 64;
  cuda_mparticles::real_t eps = 1e-2;
  Int3 bs_ = { 1, 1, 1 };
  
  Grid_t grid_;
  cuda_mfields* cmflds_;
};

struct PushMprtsAccelTest : ::testing::TestWithParam<PushMprtsTestParam>
{
  TestAccel test;
  cuda_mparticles* cmprts_;

  void SetUp()
  {
    PushMprtsTestParam prm = GetParam();
  
    test.grid_.kinds.push_back(Grid_t::Kind(prm.q, prm.m, "test_species"));
    test.init_cmflds();

    cmprts_ = new cuda_mparticles(test.grid_, test.bs_);
    test.init_cmprts(cmprts_);
  }
};

struct PushMprtsCycloTest : ::testing::TestWithParam<PushMprtsTestParam>
{
  TestCyclo test;
  cuda_mparticles *cmprts_;
  
  void SetUp()
  {
    PushMprtsTestParam prm = GetParam();
  
    test.grid_.kinds.push_back(Grid_t::Kind(prm.q, prm.m, "test_species"));
    test.init_cmflds();

    cmprts_ = new cuda_mparticles(test.grid_, test.bs_);
    test.init_cmprts(cmprts_);
  }
};

TEST_P(PushMprtsAccelTest, Accel)
{
  // init particles
  RngPool rngpool;
  Rng *rng = rngpool[0];
  
  uint n_prts_by_patch[1] = { test.n_prts };
  
  cmprts_->reserve_all(n_prts_by_patch);
  
  std::vector<cuda_mparticles_prt> prts;
  prts.reserve(test.n_prts);
  
  for (int i = 0; i < test.n_prts; i++) {
    cuda_mparticles_prt prt = {};
    prt.xi[0] = rng->uniform(0, test.L);
    prt.xi[1] = rng->uniform(0, test.L);
    prt.xi[2] = rng->uniform(0, test.L);
    prt.qni_wni = 1.;
    
    prts.push_back(prt);
  }
  cmprts_->inject(prts.data(), n_prts_by_patch);
  //cmprts_->dump();
  
  int n_failed = 0;
  int n_steps = test.n_steps;
  
  for (int n = 0; n < n_steps; n++) {
    cuda_push_mprts_yz(cmprts_, test.cmflds_, test.bs_, IP_EC, DEPOSIT_VB_3D, CURRMEM_GLOBAL);
    cmprts_->get_particles(0, [&] (int i, const cuda_mparticles_prt &prt) {
	if (std::abs(prt.pxi[0] - 1*(n+1)) > test.eps ||
	    std::abs(prt.pxi[1] - 2*(n+1)) > test.eps ||
	    std::abs(prt.pxi[2] - 3*(n+1)) > test.eps) {
	  printf("FAIL: n %d i %d px %g %g %g // exp %g %g %g\n", n, i,
		 prt.pxi[0], prt.pxi[1], prt.pxi[2],
		 1.*(n+1), 2.*(n+1), 3.*(n+1));
	  n_failed++;
	}
      });
    
    //cmprts_->dump();
  }
  EXPECT_EQ(n_failed, 0);
}

TEST_P(PushMprtsCycloTest, Cyclo)
{
  // init particles
  RngPool rngpool;
  Rng *rng = rngpool[0];
  
  uint n_prts_by_patch[1] = { test.n_prts };
  
  cmprts_->reserve_all(n_prts_by_patch);
  
  std::vector<cuda_mparticles_prt> prts;
  prts.reserve(test.n_prts);
  
  for (int i = 0; i < test.n_prts; i++) {
    cuda_mparticles_prt prt = {};
    prt.xi[0] = rng->uniform(0, test.L);
    prt.xi[1] = rng->uniform(0, test.L);
    prt.xi[2] = rng->uniform(0, test.L);
    prt.pxi[0] = 1.; // gamma = 2
    prt.pxi[1] = 1.;
    prt.pxi[2] = 1.;
    prt.qni_wni = rng->uniform(0, 1.);;
    
    prts.push_back(prt);
  }
  cmprts_->inject(prts.data(), n_prts_by_patch);
  //cmprts_->dump();
  
  int n_failed = 0;
  int n_steps = test.n_steps;
  
  for (int n = 0; n < n_steps; n++) {
    cuda_push_mprts_yz(cmprts_, test.cmflds_, test.bs_, IP_EC, DEPOSIT_VB_3D, CURRMEM_GLOBAL);
    double ux = (cos(2*M_PI*(0.125*n_steps-(n+1))/(double)n_steps) /
		 cos(2*M_PI*(0.125*n_steps)      /(double)n_steps));
    double uy = (sin(2*M_PI*(0.125*n_steps-(n+1))/(double)n_steps) /
		 sin(2*M_PI*(0.125*n_steps)      /(double)n_steps));
    double uz = 1.;
    cmprts_->get_particles(0, [&] (int i, const cuda_mparticles_prt &prt) {
	if (std::abs(prt.pxi[0] - ux) > test.eps ||
	    std::abs(prt.pxi[1] - uy) > test.eps ||
	    std::abs(prt.pxi[2] - uz) > test.eps) {
	  printf("FAIL: n %d i %d px %g %g %g // exp %g %g %g\n", n, i,
		 prt.pxi[0], prt.pxi[1], prt.pxi[2], ux, uy, uz);
	  n_failed++;
	}
      });
    
    //cmprts_->dump();
  }
  EXPECT_EQ(n_failed, 0);
}

INSTANTIATE_TEST_CASE_P(T1,
                        PushMprtsAccelTest,
                        ::testing::Values(PushMprtsTestParam{1., 1.}));
INSTANTIATE_TEST_CASE_P(T1,
                        PushMprtsCycloTest,
                        ::testing::Values(PushMprtsTestParam{2., 1.}));
