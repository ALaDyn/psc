
#ifndef GGCM_MHD_FLDS_PRIVATE_H
#define GGCM_MHD_FLDS_PRIVATE_H

#include "ggcm_mhd_flds.h"

#include "ggcm_mhd_defs.h"

struct ggcm_mhd_flds {
  struct mrc_obj obj;
  struct mrc_fld *fld;
};

struct ggcm_mhd_flds_ops {
  MRC_SUBCLASS_OPS(struct ggcm_mhd_flds);
};

#define ggcm_mhd_flds_ops(flds) ((struct ggcm_mhd_flds_ops *)(flds)->obj.ops)

extern struct ggcm_mhd_flds_ops ggcm_mhd_flds_ops_c;

#endif


