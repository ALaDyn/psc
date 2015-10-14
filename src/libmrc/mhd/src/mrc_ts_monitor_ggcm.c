
#include <mrc_ts_monitor_private.h>

#include "ggcm_mhd_private.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_defs.h"

#include <mrc_ts_private.h> // FIXME?
#include <mrc_io.h>

// ======================================================================
// mrc_ts_monitor subclass "ggcm"

struct mrc_ts_monitor_ggcm {
  int nr;
};

#define mrc_ts_monitor_ggcm(mon) mrc_to_subobj(mon, struct mrc_ts_monitor_ggcm)

static void
mrc_ts_monitor_ggcm_run(struct mrc_ts_monitor *mon, struct mrc_ts *ts)
{
  struct mrc_ts_monitor_ggcm *out = mrc_ts_monitor_ggcm(mon);

  mpi_printf(mrc_ts_monitor_comm(mon), "Writing output %d (time = %g)\n",
	     out->nr, ts->time);
  struct ggcm_mhd *mhd = (struct ggcm_mhd *) ts->ctx_obj;

  mhd->time = ts->time;
  ggcm_mhd_fill_ghosts(mhd, mhd->fld, 0, mhd->time);
  ggcm_mhd_diag_run_now(mhd->diag, mhd->fld, DIAG_TYPE_3D, out->nr++);
}

struct mrc_ts_monitor_ops mrc_ts_monitor_ggcm_ops = {
  .name             = "ggcm",
  .size             = sizeof(struct mrc_ts_monitor_ggcm),
  .run              = mrc_ts_monitor_ggcm_run,
};

