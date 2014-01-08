
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_crds.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_ic_private.h"

#include <mrc_fld.h>
#include <mrc_domain.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>

// ======================================================================
// ggcm_mhd_ic subclass "cosine"

struct ggcm_mhd_ic_cosine {
  //float l_x;
  float by0; 
  float bz0;
  float rho0; 
  float t0; 
  float tau;   
  //  float xs;
  float pert;
};

// ----------------------------------------------------------------------
// ggcm_mhd_ic_cosine_run

#define F3 MRC_F3 // FIXME

static void
ggcm_mhd_ic_cosine_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_cosine *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_cosine);
  struct ggcm_mhd *mhd = ic->mhd;
  struct mrc_fld *f3 = mrc_fld_get_as(mhd->fld, "float");
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);  

  struct mrc_fld *fld_psi = mrc_domain_fld_create(mhd->domain, SW_2, NULL);
  mrc_fld_setup(fld_psi);

  float xl[3], xh[3], L[3], r[3];
  mrc_crds_get_param_float3(crds, "l", xl);
  mrc_crds_get_param_float3(crds, "h", xh);
  for(int i = 0; i < 3; i++){
    L[i] = xh[i] - xl[i];
  }
  float kx = 2.*M_PI / L[0] ; 
  float ky = 2.*M_PI / L[1] ; 

  //float l_x = sub->l_x; // Scale length of the magenetic shear at each surface
  float by0 = sub->by0; // Asymptotic reconnection field
  float bz0 = sub->bz0; // Uniform guide field
  float rho0 = sub->rho0; // Asymptotic plasma density
  //float t0 = sub->t0; // Uniform electron temperature (Te)
  //float tau = sub->tau; // Ratio of ion temperature to electron temperature (Ti/Te)
  //float xs = sub->xs; // Distance of each current sheet from zero
  //float pert = sub->pert; // 

  mrc_fld_foreach(f3, ix, iy, iz, 2, 2) {

    r[0] = .5*(MRC_CRDX(crds, ix) + MRC_CRDX(crds, ix-1));
    r[1] = .5*(MRC_CRDY(crds, iy) + MRC_CRDY(crds, iy-1));

    MRC_F3(fld_psi, 0, ix,iy,iz) = ( L[0] / (4. * M_PI) ) * (1. - cos(2*kx*r[0])) * sin(ky*r[1]);   
    //MRC_F3(fld_psi, 0, ix,iy,iz) = 0.01*cos(kx*r[0])*(sin(ky*r[1]));
    //(L[0] / (4.*M_PI)) * ((1 - cos(kx*r[0])) * sin(ky*r[1]));
  } mrc_fld_foreach_end;

  float *bd2x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD2);
  //float *bd2y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD2);

  mrc_fld_foreach(f3, ix, iy, iz, 1, 1) {
    // FIXME! the staggering for B is okay, but fld_psi and other stuff below needs to be
    // fixed / checked for cell-centered
    r[0] = MRC_CRD(crds, 0, ix);
    r[1] = MRC_CRD(crds, 1, iy);
    r[2] = MRC_CRD(crds, 2, iz); 
    
    B1Y(f3, ix,iy,iz) = by0 * cos(kx*r[0]) - 
      (MRC_F3(fld_psi, 0, ix+1,iy,iz) - MRC_F3(fld_psi, 0, ix,iy,iz)) / bd2x[ix];    
    B1X(f3, ix,iy,iz) =  0.0;
    //(MRC_F3(fld_psi, 0, ix,iy+1,iz) - MRC_F3(fld_psi, 0, ix,iy,iz)) / bd2y[iy];
    B1Z(f3, ix,iy,iz) = sqrt( sqr(bz0) - sqr(B1Y(f3,ix,iy,iz)) ); 
    MRC_F3(f3, _RR1, ix, iy, iz) = rho0;

    MRC_F3(f3, _UU1 , ix, iy, iz) =  MRC_F3(f3, _RR1, ix, iy, iz) / (mhd->par.gamm -1.f) + 	
      .5f * (sqr(MRC_F3(f3, _RV1X, ix, iy, iz)) +
	     sqr(MRC_F3(f3, _RV1Y, ix, iy, iz)) +
	     sqr(MRC_F3(f3, _RV1Z, ix, iy, iz))) / MRC_F3(f3, _RR1, ix, iy, iz) +
      .5f * (sqr(.5*(B1X(f3, ix,iy,iz) + B1X(f3, ix+1,iy,iz))) +
	     sqr(.5*(B1Y(f3, ix,iy,iz) + B1Y(f3, ix,iy+1,iz))) +
	     sqr(.5*(B1Z(f3, ix,iy,iz) + B1Z(f3, ix,iy,iz+1))));
  } mrc_fld_foreach_end;
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_cosine_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_cosine, x)
static struct param ggcm_mhd_ic_cosine_descr[] = {
  //{"l_b", VAR(l_b), PARAM_FLOAT(0.1)},
  {"by0", VAR(by0), PARAM_FLOAT(1.0)},
  {"bz0", VAR(bz0), PARAM_FLOAT(1.0)},
  {"rho0", VAR(rho0), PARAM_FLOAT(1.0)},  
  {"t0", VAR(t0), PARAM_FLOAT(0.5)},  
  {"tau", VAR(tau), PARAM_FLOAT(1.0)},
  {"pert",VAR(pert), PARAM_FLOAT(1.0)},
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic_cosine_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_cosine_ops = {
  .name        = "cosine",
  .size        = sizeof(struct ggcm_mhd_ic_cosine),
  .param_descr = ggcm_mhd_ic_cosine_descr,
  .run         = ggcm_mhd_ic_cosine_run,
};


// ======================================================================
// ggcm_mhd class "cosine"

// ----------------------------------------------------------------------
// ggcm_mhd_cosine_create

static void
ggcm_mhd_cosine_create(struct ggcm_mhd *mhd)
{
  ggcm_mhd_default_box(mhd);

  /* set defaults for coord arrays */
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  mrc_crds_set_type(crds, "two_gaussian");
  mrc_crds_set_param_int(crds, "sw", SW_2);   // 'stencil width' 
  mrc_crds_set_param_float3(crds, "l", (float[3]) {  0.0, 0.0, -1.0 });
  mrc_crds_set_param_float3(crds, "h", (float[3]) {  2.*M_PI, 2.*M_PI,  1.0 });
}

static struct ggcm_mhd_ops ggcm_mhd_cosine_ops = {
  .name             = "cosine",
  .create           = ggcm_mhd_cosine_create,
};

// ======================================================================

extern struct ggcm_mhd_diag_ops ggcm_mhd_diag_c_ops;

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_cosine_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_cosine_ops);  
 
  return ggcm_mhd_main(&argc, &argv);
}

