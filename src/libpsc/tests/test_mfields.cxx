
#include <gtest/gtest.h>

#include "fields3d.hxx"
#include "psc_fields_single.h"
#include "psc_fields_c.h"
#ifdef USE_CUDA
#include "psc_fields_cuda.h"
#endif

#include "psc.h" // FIXME, just for EX etc

static Grid_t make_grid()
{
  auto domain = Grid_t::Domain{{8, 4, 2},
			       {80.,  40., 20.}, {-40., -20., 0.},
			       {2, 2, 1}};
  auto bc = GridBc{};
  auto kinds = Grid_t::Kinds{};
  auto norm = Grid_t::Normalization{};
  double dt = .1;
  return Grid_t{domain, bc, kinds, norm, dt};
}

template <typename T>
class MfieldsTest : public ::testing::Test
{};

#ifdef USE_CUDA
using MfieldsTestTypes = ::testing::Types<MfieldsSingle, MfieldsC, MfieldsCuda>;
#else
using MfieldsTestTypes = ::testing::Types<MfieldsSingle, MfieldsC>;
#endif

TYPED_TEST_CASE(MfieldsTest, MfieldsTestTypes);

TYPED_TEST(MfieldsTest, Constructor)
{
  using Mfields = TypeParam;

  auto grid = make_grid();
  auto mflds = Mfields{grid, NR_FIELDS, Int3{ 1, 1, 1 }};

  EXPECT_EQ(mflds.n_patches(), grid.n_patches());
}

TYPED_TEST(MfieldsTest, Access)
{
  using Mfields = TypeParam;

  auto grid = make_grid();
  auto mflds = Mfields{grid, NR_FIELDS, Int3{ 1, 1, 1 }};

  EXPECT_EQ(mflds[0](0, 1, 1, 1), 0.);

  mflds[0](0, 1, 1, 1) = 99.;

  EXPECT_EQ(mflds[0](0, 1, 1, 1), 99.);
}

TYPED_TEST(MfieldsTest, ZeroComp)
{
  using Mfields = TypeParam;

  auto grid = make_grid();
  auto mflds = Mfields{grid, NR_FIELDS, Int3{ 1, 1, 1 }};

  mflds[0](EX,  4, 2, 2) = 1.;
  mflds[0](EY, -1,-1,-1) = 2.;
  mflds[0](EY,  0, 0, 0) = 3.;
  mflds[0](EY,  4, 2, 2) = 4.;
  mflds[0](EZ, -1,-1,-1) = 5.;

  mflds.zero_comp(EY);

  EXPECT_EQ(mflds[0](EX,  4, 2, 2), 1.);
  EXPECT_EQ(mflds[0](EY, -1,-1,-1), 0.);
  EXPECT_EQ(mflds[0](EY,  0, 0, 0), 0.);
  EXPECT_EQ(mflds[0](EY,  4, 2, 2), 0.);
  EXPECT_EQ(mflds[0](EZ, -1,-1,-1), 5.);
}

// ======================================================================
// main

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();
  MPI_Finalize();
  return rc;
}
