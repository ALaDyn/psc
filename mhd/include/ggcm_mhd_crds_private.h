
#ifndef GGCM_MHD_CRDS_PRIVATE_H
#define GGCM_MHD_CRDS_PRIVATE_H

#include "ggcm_mhd_crds.h"

struct ggcm_mhd_crds {
  struct mrc_obj obj;
  struct ggcm_mhd_crds_gen *crds_gen;
  struct mrc_domain *domain;
  struct mrc_f1 *f1[3];
};

struct ggcm_mhd_crds_ops {
  MRC_SUBCLASS_OPS(struct ggcm_mhd_crds);
};

extern struct ggcm_mhd_crds_ops ggcm_mhd_crds_c_ops;

#endif
