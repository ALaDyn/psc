
#ifndef MRC_CRDS_H
#define MRC_CRDS_H

#include <mrc_obj.h>

struct mrc_crds_params {
  float xl[3];
  float xh[3];
  int sw;
};

struct mrc_crds {
  struct mrc_obj obj;
  struct mrc_crds_params par;
  struct mrc_domain *domain;
  struct mrc_f1 *crd[3];
};

#define MRC_CRD(crds, d, ix) MRC_F1((crds)->crd[d],0, ix)
#define MRC_CRDX(crds, ix) MRC_CRD(crds, 0, ix)
#define MRC_CRDY(crds, iy) MRC_CRD(crds, 1, iy)
#define MRC_CRDZ(crds, iz) MRC_CRD(crds, 2, iz)

struct mrc_crds_ops {
  MRC_OBJ_OPS;
  void (*set_values)(struct mrc_crds *crds, float *crdx, int mx,
		     float *crdy, int my, float *crdz, int mz);
};

extern struct mrc_class mrc_class_mrc_crds;

MRC_OBJ_DEFINE_STANDARD_METHODS(mrc_crds, struct mrc_crds)
void mrc_crds_set_domain(struct mrc_crds *crds, struct mrc_domain *domain);
void mrc_crds_set_values(struct mrc_crds *crds, float *crdx, int mx,
			 float *crdy, int my, float *crdz, int mz);
void mrc_crds_setup(struct mrc_crds *crds);
void mrc_crds_get_xl_xh(struct mrc_crds *crds, float xl[3], float xh[3]);
void mrc_crds_get_dx(struct mrc_crds *crds, float dx[3]);

#endif

