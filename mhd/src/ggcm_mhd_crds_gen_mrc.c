
#include "ggcm_mhd_crds_gen_private.h"

#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_crds_private.h"

#include <mrc_domain.h>
#include <assert.h>

// ----------------------------------------------------------------------
// ggcm_mhd_crds_gen_mrc_run
//
// initializes the Fortran common block coord arrays FX1, FD1 from
// using C grid generation

static void
ggcm_mhd_crds_gen_mrc_run(struct ggcm_mhd_crds_gen *gen, struct ggcm_mhd_crds *crds)
{
  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(crds->domain, 0, &info);
  struct mrc_crds *mrc_crds = mrc_domain_get_crds(crds->domain);
  int *ldims = info.ldims;

  for (int d = 0; d < 3; d++) {
    float *fxx1 = ggcm_mhd_crds_get_crd(crds, d, FX1);
    float *fdx1 = ggcm_mhd_crds_get_crd(crds, d, FD1);
    int sw = mrc_crds->sw;
    struct mrc_fld *global_x = crds->global_f1[d];

    // FIXME: this is not bounds checked at all since sw is set from mrc_crds,
    //        NOT ggcm_mhd_crds, which is on the left hand side here
    for (int i = -sw; i < ldims[d] + sw; i++) {
      fxx1[i] = MRC_CRD(mrc_crds, d, i);
    }

    // have to move one in on both sides
    for (int i = -sw + 1; i < ldims[d] + sw - 1; i++) {
        fdx1[i] = 1.0 / (0.5 * (MRC_CRD(mrc_crds, d, i + 1) - MRC_CRD(mrc_crds, d, i - 1)));
    }

    mrc_f1_foreach(global_x, i, 0, 0) {
      MRC_F1(global_x, 0, i) = MRC_D2(mrc_crds->global_crd[d], i, 0);
    } mrc_f1_foreach_end;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_crds_gen subclass "mrc"

struct ggcm_mhd_crds_gen_ops ggcm_mhd_crds_gen_mrc_ops = {
  .name       = "mrc",
  .run        = ggcm_mhd_crds_gen_mrc_run,
};
