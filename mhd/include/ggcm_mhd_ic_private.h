
#ifndef GGCM_MHD_IC_PRIVATE_H
#define GGCM_MHD_IC_PRIVATE_H

#include "ggcm_mhd_ic.h"

struct ggcm_mhd_ic {
  struct mrc_obj obj;
  struct ggcm_mhd *mhd;
};

struct ggcm_mhd_ic_ops {
  MRC_SUBCLASS_OPS(struct ggcm_mhd_ic);
  void (*run)(struct ggcm_mhd_ic *ic);
  void (*init_masks)(struct ggcm_mhd_ic *ic);
};

#define ggcm_mhd_ic_ops(ic) ((struct ggcm_mhd_ic_ops *)((ic)->obj.ops))

#endif
