
#include "ggcm_mhd_commu_private.h"

#include "ggcm_mhd_private.h"

#include <mrc_domain.h>
#include <mrc_ddc.h>

#include <assert.h>

// ======================================================================

struct ggcm_mhd_commu_c {
  struct mrc_ddc *ddc;
};

#define ggcm_mhd_commu_c(commu) mrc_to_subobj(commu, struct ggcm_mhd_commu_c)

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_view

static void
ggcm_mhd_commu_c_view(struct ggcm_mhd_commu *commu)
{
  struct ggcm_mhd_commu_c *commu_c = ggcm_mhd_commu_c(commu);

  if (commu_c->ddc) {
    mrc_ddc_view(commu_c->ddc);
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_setup

static void
ggcm_mhd_commu_c_setup(struct ggcm_mhd_commu *commu)
{
  struct ggcm_mhd_commu_c *commu_c = ggcm_mhd_commu_c(commu);

  assert(commu->mhd);
  commu_c->ddc = mrc_domain_create_ddc(commu->mhd->domain);
  mrc_ddc_set_param_int3(commu_c->ddc, "ibn", (int[3]) { 2, 2, 2});
  mrc_ddc_setup(commu_c->ddc);
}

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_read

static void
ggcm_mhd_commu_c_read(struct ggcm_mhd_commu *commu, struct mrc_io *io)
{
  ggcm_mhd_commu_read_super(commu, io);
  ggcm_mhd_commu_c_setup(commu);
}

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_destroy

static void
ggcm_mhd_commu_c_destroy(struct ggcm_mhd_commu *commu)
{
  struct ggcm_mhd_commu_c *commu_c = ggcm_mhd_commu_c(commu);

  mrc_ddc_destroy(commu_c->ddc);
}

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_run

void
ggcm_mhd_commu_c_run(struct ggcm_mhd_commu *commu, struct mrc_fld *fld,
		     int mb, int me)
{
  struct ggcm_mhd_commu_c *commu_c = ggcm_mhd_commu_c(commu);

  assert(fld->_data_type == MRC_NT_FLOAT);
  struct mrc_fld *f = mrc_fld_get_as(fld, mrc_fld_type(fld));
  mrc_ddc_fill_ghosts_fld(commu_c->ddc, mb, me, f);
  mrc_fld_put_as(f, fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_commu_c_ops

struct ggcm_mhd_commu_ops ggcm_mhd_commu_c_ops = {
  .name             = "c",
  .size             = sizeof(struct ggcm_mhd_commu_c),
  .view             = ggcm_mhd_commu_c_view,
  .setup            = ggcm_mhd_commu_c_setup,
  .read             = ggcm_mhd_commu_c_read,
  .destroy          = ggcm_mhd_commu_c_destroy,
  .run              = ggcm_mhd_commu_c_run,
};
