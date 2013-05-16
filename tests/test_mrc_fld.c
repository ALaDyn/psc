
#include <mrc_fld.h>
#include <mrc_domain.h>
#include <mrc_io.h>
#include <mrc_params.h>
#include <mrctest.h>

#include <assert.h>
#include <string.h>

// ----------------------------------------------------------------------
// test_12

static void
test_12(int sw)
{
  struct mrc_fld *fld = mrc_fld_create(MPI_COMM_WORLD);
  mrc_fld_set_name(fld, "test_fld");
  mrc_fld_set_param_int3(fld, "offs", (int [3]) { 1, 2, 3 });
  mrc_fld_set_param_int3(fld, "dims", (int [3]) { 2, 3, 4 });
  mrc_fld_set_param_int(fld, "sw", sw);
  mrc_fld_set_from_options(fld);
  mrc_fld_setup(fld);
  mrc_fld_view(fld);

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    MRC_S3(fld, ix,iy,iz) = ix * 10000 + iy * 100 + iz;
  } mrc_fld_foreach_end;

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    assert(MRC_S3(fld, ix,iy,iz) == ix * 10000 + iy * 100 + iz);
  } mrc_fld_foreach_end;

  mrc_fld_destroy(fld);
}

// ----------------------------------------------------------------------
// test_34
//
// same as test_12 as "double"

static void
test_34(int sw)
{
  struct mrc_fld *fld = mrc_fld_create(MPI_COMM_WORLD);
  mrc_fld_set_name(fld, "test_fld");
  mrc_fld_set_param_select(fld, "data_type", MRC_NT_DOUBLE);
  mrc_fld_set_param_int3(fld, "offs", (int [3]) { 1, 2, 3 });
  mrc_fld_set_param_int3(fld, "dims", (int [3]) { 2, 3, 4 });
  mrc_fld_set_param_int(fld, "sw", sw);
  mrc_fld_set_from_options(fld);
  mrc_fld_setup(fld);
  mrc_fld_view(fld);

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    MRC_D3(fld, ix,iy,iz) = ix * 10000 + iy * 100 + iz;
  } mrc_fld_foreach_end;

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    assert(MRC_D3(fld, ix,iy,iz) == ix * 10000 + iy * 100 + iz);
  } mrc_fld_foreach_end;

  mrc_fld_destroy(fld);
}

// ----------------------------------------------------------------------
// test_56
//
// 4-d field

static void
test_56(int sw)
{
  struct mrc_fld *fld = mrc_fld_create(MPI_COMM_WORLD);
  mrc_fld_set_param_int(fld, "nr_dims", 4);
  mrc_fld_set_param_int_array(fld, "offs", 4, (int []) { 1, 2, 3, 0 });
  mrc_fld_set_param_int_array(fld, "dims", 4, (int []) { 2, 3, 4, 2 });
  mrc_fld_set_param_int(fld, "sw", sw);
  mrc_fld_set_from_options(fld);
  mrc_fld_setup(fld);
  mrc_fld_view(fld);

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    MRC_S4(fld, ix,iy,iz,1) = ix * 10000 + iy * 100 + iz;
  } mrc_fld_foreach_end;

  mrc_fld_foreach(fld, ix,iy,iz, sw, sw) {
    assert(MRC_S4(fld, ix,iy,iz,1) == ix * 10000 + iy * 100 + iz);
  } mrc_fld_foreach_end;

  mrc_fld_destroy(fld);
}


// ----------------------------------------------------------------------
// main

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  int testcase = 1;
  mrc_params_get_option_int("case", &testcase);

  switch (testcase) {
  case 1: test_12(0); break;
  case 2: test_12(1); break;
  case 3: test_34(0); break;
  case 4: test_34(1); break;
  case 5: test_56(0); break;
  default: assert(0);
  }

  MPI_Finalize();
}
