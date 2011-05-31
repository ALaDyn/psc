
#include <mrc_ts_monitor_private.h>

#include <mrc_ts_private.h> // FIXME?
#include <mrc_params.h>
#include <assert.h>

// ======================================================================

#define mrc_ts_monitor_ops(mon) ((struct mrc_ts_monitor_ops *) mon->obj.ops)

void
mrc_ts_monitor_run(struct mrc_ts_monitor *mon, struct mrc_ts *ts)
{
  assert(mrc_ts_monitor_ops(mon)->run);

  while (ts->n > mon->next_step) {
    mon->next_step += mon->every;
  }
  if (mon->next_step == ts->n) {
    mrc_ts_monitor_ops(mon)->run(mon, ts);
    mon->next_step += mon->every;
  }
}

// ======================================================================
// mrc_ts_monitor_init

static void
mrc_ts_monitor_init()
{
  mrc_class_register_subclass(&mrc_class_mrc_ts_monitor, &mrc_ts_monitor_output_ops);
  mrc_class_register_subclass(&mrc_class_mrc_ts_monitor, &mrc_ts_monitor_diag_ops);
}

// ======================================================================
// mrc_ts_monitor class

#define VAR(x) (void *)offsetof(struct mrc_ts_monitor, x)
static struct param mrc_ts_monitor_descr[] = {
  { "every"         , VAR(every)         , PARAM_INT(1)             },
  {},
};
#undef VAR

struct mrc_class_mrc_ts_monitor mrc_class_mrc_ts_monitor = {
  .name         = "mrc_ts_monitor",
  .size         = sizeof(struct mrc_ts_monitor),
  .param_descr  = mrc_ts_monitor_descr,
  .init         = mrc_ts_monitor_init,
};

