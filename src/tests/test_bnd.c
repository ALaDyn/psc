
#include "psc_testing.h"
#include "psc_bnd.h"
#include "psc_fields_as_c.h"
#include <mrc_profile.h>
#include <mrc_params.h>

#include <stdio.h>
#include <math.h>
#include <mpi.h>

#if 0
static void
setup_jx(mfields_base_t *flds_base)
{
  psc_foreach_patch(ppsc, p) {
    fields_t *pf_base = psc_mfields_get_patch(flds_base, p);
    fields_t *pf = psc_fields_get_as(pf_base, "c", 0, 0);
    psc_foreach_3d_g(ppsc, p, jx, jy, jz) {
      int ix, iy, iz;
      psc_local_to_global_indices(ppsc, p, jx, jy, jz, &ix, &iy, &iz);
      f_real xx = 2.*M_PI * ix / ppsc->domain.gdims[0];
      f_real zz = 2.*M_PI * iz / ppsc->domain.gdims[2];
      F3(pf, JXI, jx,jy,jz) = cos(xx) * sin(zz);
    } foreach_3d_g_end;
    psc_fields_put_as(pf, pf_base, JXI, JXI + 1);
  }
}

static void
setup_jx_noghost(mfields_base_t *flds_base)
{
  psc_foreach_patch(ppsc, p) {
    fields_t *pf_base = psc_mfields_get_patch(flds_base, p);
    fields_t *pf = psc_fields_get_as(pf_base, "c", 0, 0);
    psc_foreach_3d_g(ppsc, p, jx, jy, jz) {
      int ix, iy, iz;
      psc_local_to_global_indices(ppsc, p, jx, jy, jz, &ix, &iy, &iz);
      f_real xx = 2.*M_PI * ix / ppsc->domain.gdims[0];
      f_real zz = 2.*M_PI * iz / ppsc->domain.gdims[2];
      F3(pf, JXI, jx,jy,jz) = cos(xx) * sin(zz);
    } foreach_3d_end;
    psc_fields_put_as(pf, pf_base, JXI, JXI + 1);
  }
}
#endif

int
main(int argc, char **argv)
{
#if 0
  psc_testing_init(&argc, &argv);

  // test psc_add_ghosts()

  struct psc_case *_case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "fortran");
  psc_case_setup(_case);
  setup_jx(ppsc->flds);
  //  psc_dump_field(JXI, "jx0");
  psc_bnd_add_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(JXI, "jx1");
  psc_save_fields_ref(ppsc, ppsc->flds);
  psc_case_destroy(_case);

  _case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "c");
  psc_case_setup(_case);
  setup_jx(ppsc->flds);
  psc_bnd_add_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(JXI, "jx2");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-10, 0);
  psc_case_destroy(_case);

#ifdef USE_CUDA
  _case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "cuda");
  psc_case_setup(_case);
  setup_jx(ppsc->flds);
  psc_bnd_add_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(JXI, "jx2");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-7, 0);
  psc_case_destroy(_case);
#endif

  // test psc_fill_ghosts()

  _case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "fortran");
  psc_case_setup(_case);
  setup_jx_noghost(ppsc->flds);
  //  psc_dump_field(ppsc->flds, JXI, "jx0");
  psc_bnd_fill_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(ppsc->flds, JXI, "jx1");
  psc_save_fields_ref(ppsc, ppsc->flds);
  psc_case_destroy(_case);

  _case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "c");
  psc_case_setup(_case);
  setup_jx_noghost(ppsc->flds);
  psc_bnd_fill_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(ppsc->flds, JXI, "jx2");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-10, 3);
  psc_case_destroy(_case);

#ifdef USE_CUDA
  _case = psc_create_test_yz();
  psc_bnd_set_type(ppsc->bnd, "cuda");
  psc_case_setup(_case);
  setup_jx_noghost(ppsc->flds);
  psc_bnd_fill_ghosts(ppsc->bnd, ppsc->flds, JXI, JXI + 1);
  //  psc_dump_field(ppsc->flds, JXI, "jx2");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-7, 3);
  psc_case_destroy(_case);
#endif

  psc_testing_finalize();
#endif
}
