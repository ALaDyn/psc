
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_bnd.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_ic_private.h"

#include <mrc_fld_as_float.h>
#include <mrc_domain.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>

// ======================================================================
// ggcm_mhd_ic subclass "bw"

struct ggcm_mhd_ic_bw {
  const char* pdim;
};

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bw_run

static void
ggcm_mhd_ic_bw_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_bw *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_bw);
  struct ggcm_mhd *mhd = ic->mhd;
  struct mrc_fld *fld = mrc_fld_get_as(mhd->fld, FLD_TYPE);
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);  

  float xl[3], xh[3], L[3], r[3];
  mrc_crds_get_param_float3(crds, "l", xl);
  mrc_crds_get_param_float3(crds, "h", xh);
  for(int i = 0; i < 3; i++){
    L[i] = xh[i] - xl[i];
  }

  mrc_fld_foreach(fld, ix, iy, iz, 2, 2) {
    r[0] = MRC_CRD(crds, 0, ix);
    r[1] = MRC_CRD(crds, 1, iy);
    r[2] = MRC_CRD(crds, 2, iz);
    
    if(strcmp(sub->pdim, "x") == 0){

      MHERE;
      if(fabs(r[0]) < 0.5*L[0]){
	// Left                         
	RR1(fld, ix,iy,iz) = 1.0;
	PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
	B1X(fld, ix,iy,iz) = 0.75; 
	B1Y(fld, ix,iy,iz) = 1.0;
      } else {
	// Right
	RR1(fld, ix,iy,iz) = 0.125;	 
	PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
	B1X(fld, ix,iy,iz) = 0.75;
	B1Y(fld, ix,iy,iz) = -1.0;
	B1Z(fld, ix,iy,iz) = 0.0;
      }

      /*
  } else if(strcmp(sub->pdim, "y") == 1){

    if(fabs(r[1]) < 0.5*L[1]){
      // Left 
      F3(fld, _RR1, ix, iy, iz) = 1.0;
      PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
      F3(fld, _B1Y , ix, iy, iz) = 0.75;
      F3(fld, _B1X , ix, iy, iz) = 1.0;
      F3(fld, _B1Z , ix, iy, iz) = 0.0;
    }else{
      // Right
      F3(fld, _RR1, ix, iy, iz) = 0.125;
      PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
      F3(fld, _B1Y , ix, iy, iz) = 0.75;
      F3(fld, _B1X , ix, iy, iz) = -1.0;
      F3(fld, _B1Z , ix, iy, iz) = 0.0;
    }
  } else if(strcmp(sub->pdim, "z") == 1){
    if(fabs(r[2]) < 0.5*L[2]){
      // Left 
      F3(fld, _RR1, ix, iy, iz) = 1.0;
      PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
      F3(fld, _B1Z , ix, iy, iz) = 0.75;
      F3(fld, _B1X , ix, iy, iz) = 1.0;
    }else{
      // Right
      F3(fld, _RR1, ix, iy, iz) = 0.125;
      F3(fld, _B1X , ix, iy, iz) = -1.0;
      F3(fld, _B1Z , ix, iy, iz) = 0.75;
      PP1(fld, ix,iy,iz) = RR1(fld, ix,iy,iz);
      */

  } else {           
    assert(0); /* unknown initial condition */
  }
  } mrc_fld_foreach_end;
  
  mrc_fld_put_as(fld, mhd->fld);

  ggcm_mhd_convert_fc_from_primitive(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bw_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_bw, x)
static struct param ggcm_mhd_ic_bw_descr[] = {
  {"pdim", VAR(pdim), PARAM_STRING("x")},  
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bw_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_bw_ops = {
  .name        = "bw",
  .size        = sizeof(struct ggcm_mhd_ic_bw),
  .param_descr = ggcm_mhd_ic_bw_descr,
  .run         = ggcm_mhd_ic_bw_run,
};


// ======================================================================
// ggcm_mhd class "bw"

// ----------------------------------------------------------------------
// ggcm_mhd_bw_create

static void
ggcm_mhd_bw_create(struct ggcm_mhd *mhd)
{
  ggcm_mhd_default_box(mhd);

  ggcm_mhd_bnd_set_type(mhd->bnd, "open_x");
  mrc_domain_set_param_int(mhd->domain, "bcx", BC_NONE);

  /* set defaults for coord arrays */
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  mrc_crds_set_type(crds, "uniform");
  mrc_crds_set_param_int(crds, "sw", SW_2);   // 'stencil width' 
  mrc_crds_set_param_float3(crds, "l", (float[3]) {  0.0, 0.0, 0.0 });
  mrc_crds_set_param_float3(crds, "h", (float[3]) {  1.0, 0.05, 0.05 });
}

static struct ggcm_mhd_ops ggcm_mhd_bw_ops = {
  .name             = "bw",
  .create           = ggcm_mhd_bw_create,
};

// ======================================================================

extern struct ggcm_mhd_diag_ops ggcm_mhd_diag_c_ops;

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_bw_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_bw_ops);  
 
  return ggcm_mhd_main(&argc, &argv);
}

