
#include "ggcm_mhd_diag_private.h"

#include "ggcm_mhd.h"

#include <mrc_io.h>
#include <assert.h>

// ======================================================================
// ggcm_mhd_diag class

#define ggcm_mhd_diag_ops(diag) ((struct ggcm_mhd_diag_ops *)(diag->obj.ops))

// ----------------------------------------------------------------------
// ggcm_mhd_diag_run

void
ggcm_mhd_diag_run(struct ggcm_mhd_diag *diag)
{
  struct ggcm_mhd_diag_ops *ops = ggcm_mhd_diag_ops(diag);
  assert(ops && ops->run);
  ops->run(diag);
}

// ----------------------------------------------------------------------
// ggcm_mhd_diag_shutdown

void
ggcm_mhd_diag_shutdown(struct ggcm_mhd_diag *diag)
{
  struct ggcm_mhd_diag_ops *ops = ggcm_mhd_diag_ops(diag);
  assert(ops && ops->shutdown);
  ops->shutdown(diag);
}

// ----------------------------------------------------------------------
// ggcm_mhd_diag_init

static void
ggcm_mhd_diag_init()
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
// FIXME, that's not a pretty way of figuring out whether we're being built
// as part of openggcm or standalone
#ifndef GNX
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_s2_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_f2_ops);
#endif
}

// ----------------------------------------------------------------------
// ggcm_mhd_diag description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_diag, x)
static struct param ggcm_mhd_diag_descr[] = {
  { "mhd"             , VAR(mhd)             , PARAM_OBJ(ggcm_mhd)      },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_diag class

struct mrc_class_ggcm_mhd_diag mrc_class_ggcm_mhd_diag = {
  .name             = "ggcm_mhd_diag",
  .size             = sizeof(struct ggcm_mhd_diag),
  .param_descr      = ggcm_mhd_diag_descr,
  .init             = ggcm_mhd_diag_init,
};

