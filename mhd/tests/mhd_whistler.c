
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_ic_private.h"

#include <mrc_fld.h>
#include <mrc_fld_as_float.h>
#include <mrc_domain.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>

struct ggcm_mhd_ic_whistler {
  float Boz;
  float pert; 
  float eps; 
  float n0; 
  float lambda;   
};
// ----------------------------------------------------------------------
// ggcm_mhd_ic_whistler_run

static void
ggcm_mhd_ic_whistler_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_whistler *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_whistler);
  struct ggcm_mhd *mhd = ic->mhd;  
  struct mrc_fld *f3 = mrc_fld_get_as(mhd->fld, FLD_TYPE);
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);  
  float xl[3], xh[3], L[3], r[3];
  mrc_crds_get_param_float3(crds, "l", xl);
  mrc_crds_get_param_float3(crds, "h", xh);
  for(int i=0; i<3; i++){
    L[i] = xh[i] - xl[i];
  }
  mrc_fld_foreach(f3, ix, iy, iz, 1, 1) {
    r[0] = MRC_CRD(crds, 0, ix);
    r[1] = MRC_CRD(crds, 1, iy);
    r[2] = MRC_CRD(crds, 2, iz);

    // Xin etal (2005)      
    float kk= (sub->lambda * 2.*M_PI) / L[2] ;
    const int *dims = mrc_fld_dims(f3);
    int nz = dims[2];
    float vp= 2.*M_PI*sub->Boz*nz/((sub->n0)*L[2]); 
    RR1(f3, ix,iy,iz) = sub->n0 ;       
    V1X(f3, ix,iy,iz) = (sub->pert) * sin( kk*r[2] ) ;      
    V1Y(f3, ix,iy,iz) = -(sub->pert) * cos( kk*r[2] ) ;
    PP1(f3, ix,iy,iz) = RR1(f3, ix,iy, iz);
    B1X(f3, ix+1,iy,iz) = (sub->pert) * vp * sin( kk*r[2] ) ;       
    B1Y(f3, ix,iy+1,iz) = -(sub->pert) * vp * cos( kk*r[2] ) ;   
    B1Z(f3, ix,iy,iz+1) = sub->Boz ; 
  } mrc_fld_foreach_end;

  mrc_fld_put_as(f3, mhd->fld);

  ggcm_mhd_convert_from_primitive(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_whistler_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_whistler, x)
static struct param ggcm_mhd_ic_whistler_descr[] = {
  {"pert", VAR(pert), PARAM_FLOAT(1e-5)},
  {"Boz", VAR(Boz), PARAM_FLOAT(1.0)},
  {"n0", VAR(n0), PARAM_FLOAT(25.)},
  {"lambda", VAR(lambda), PARAM_FLOAT(4)},  
  {},
};
#undef VAR



// ----------------------------------------------------------------------
// ggcm_mhd_ic_whistler_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_whistler_ops = {
  .name        = "whistler",
  .size        = sizeof(struct ggcm_mhd_ic_whistler),
  .param_descr = ggcm_mhd_ic_whistler_descr,
  .run         = ggcm_mhd_ic_whistler_run,
};



// ======================================================================
// ggcm_mhd class "whistler"

// ----------------------------------------------------------------------
// ggcm_mhd_whistler_create

static void
ggcm_mhd_whistler_create(struct ggcm_mhd *mhd)
{
  ggcm_mhd_default_box(mhd);

  /* set defaults for coord arrays */
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  mrc_crds_set_type(crds, "uniform");
  mrc_crds_set_param_int(crds, "sw", SW_2);   // 'stencil width' 
  mrc_crds_set_param_float3(crds, "l", (float[3]) {  0.0, 0.0, 0.0 });
  mrc_crds_set_param_float3(crds, "h", (float[3]) {  2.0, 0.1, 0.1 });
}

static struct ggcm_mhd_ops ggcm_mhd_whistler_ops = {
  .name             = "whistler",
  .create           = ggcm_mhd_whistler_create,
};

// ======================================================================

extern struct ggcm_mhd_diag_ops ggcm_mhd_diag_c_ops;

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_whistler_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_whistler_ops);  
 
  return ggcm_mhd_main(&argc, &argv);
}

