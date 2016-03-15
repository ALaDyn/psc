
#include "ggcm_mhd_diag_item_private.h"

#include "ggcm_mhd_private.h"
#include "ggcm_mhd_diag_private.h"


// ======================================================================
// ggcm_mhd_diag_item subclass "ymask"

// ----------------------------------------------------------------------
// ggcm_mhd_diag_item_ymask_run

static void
ggcm_mhd_diag_item_ymask_run(struct ggcm_mhd_diag_item *item,
			   struct mrc_io *io, struct mrc_fld *f,
			   int diag_type, float plane)
{
  struct ggcm_mhd *mhd = item->diag->mhd;
  assert(mhd->ymask);
  ggcm_mhd_diag_c_write_one_field(io, mhd->ymask, 0, "ymask", 1., diag_type, plane);
}

// ----------------------------------------------------------------------
// ggcm_mhd_diag_item subclass "ymask"

struct ggcm_mhd_diag_item_ops ggcm_mhd_diag_item_ops_ymask = {
  .name             = "ymask",
  .run              = ggcm_mhd_diag_item_ymask_run,
};


// ======================================================================
// ggcm_mhd_diag_item subclass "b0"

// ----------------------------------------------------------------------
// ggcm_mhd_diag_item_b0_run

static void
ggcm_mhd_diag_item_b0_run(struct ggcm_mhd_diag_item *item,
			   struct mrc_io *io, struct mrc_fld *f,
			   int diag_type, float plane)
{
  struct ggcm_mhd *mhd = item->diag->mhd;
  assert(mhd->b0);
  ggcm_mhd_diag_c_write_one_field(io, mhd->b0, 0, "b0x", 1., diag_type, plane);
  ggcm_mhd_diag_c_write_one_field(io, mhd->b0, 1, "b0y", 1., diag_type, plane);
  ggcm_mhd_diag_c_write_one_field(io, mhd->b0, 2, "b0z", 1., diag_type, plane);
}

// ----------------------------------------------------------------------
// ggcm_mhd_diag_item subclass "b0"

struct ggcm_mhd_diag_item_ops ggcm_mhd_diag_item_ops_b0 = {
  .name             = "b0",
  .run              = ggcm_mhd_diag_item_b0_run,
};

