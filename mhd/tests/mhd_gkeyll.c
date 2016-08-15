
#include <ggcm_mhd_private.h>
#include <ggcm_mhd_step.h>
#include <ggcm_mhd_ic_private.h>
#include <ggcm_mhd_crds_private.h>
#include <ggcm_mhd_bnd.h>
#include <ggcm_mhd_bndsw.h>
#include <ggcm_mhd_diag.h>

#include <mrc_fld_as_double_aos.h>
#include <mrc_domain.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>

#include <ggcm_mhd_diag_item_private.h>

// ======================================================================
// ggcm_mhd_ic subclass "bowshock3d"

struct ggcm_mhd_ic_bowshock3d {
  float rho_obstacle; // initial density in obstacle
  float r_obstacle;
  float x_obstacle;
  float rho0; // initial density outside
  float p0; // initial pressure
  float v0; // initial velocity
  float Bx0;
  float By0;
  float Bz0;
};

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bowshock3d_run
//
// assuming domain [-1.5,-1.5]x[-1.5,1.5]x[-0.75,0.75]

static mrc_fld_data_t linear_change(mrc_fld_data_t x,
    mrc_fld_data_t x1, mrc_fld_data_t val1,
    mrc_fld_data_t x2, mrc_fld_data_t val2)
{
  mrc_fld_data_t val = val1;
  if (x > x2) {
    val = val2;
  } else if (x > x1) {
    val = val1 + (val2 - val1) * (x - x1) / (x2 - x1);
  }
  return val;
}

static void
ggcm_mhd_ic_bowshock3d_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_bowshock3d *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_bowshock3d);
  struct ggcm_mhd *mhd = ic->mhd;
  struct mrc_fld *fld = mrc_fld_get_as(mhd->fld, FLD_TYPE);

  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  struct mrc_patch_info pinfo;
  double dx0[3], dx[3];
  mrc_crds_get_dx_base(crds, dx0);

  int gdims[3], p1x, p1y, p1z;
  mrc_domain_get_global_dims(mhd->domain, gdims);
  p1x = (gdims[0] > 1);
  p1y = (gdims[1] > 1);
  p1z = (gdims[2] > 1);

  struct mrc_fld *A = mrc_domain_fld_create(mhd->domain, SW_2, NULL);
  mrc_fld_set_type(A, FLD_TYPE);
  mrc_fld_set_param_int(A, "nr_comps", 3);
  mrc_fld_setup(A);
  mrc_fld_view(A);

  double l[3];
  mrc_crds_get_param_double3(mrc_domain_get_crds(mhd->domain), "l", l);

  for (int p = 0; p < mrc_fld_nr_patches(fld); p++) {
    /* get dx for this patch */
    mrc_domain_get_local_patch_info(mhd->domain, p, &pinfo);
    for (int d = 0; d < 3; d++){
      float refine = 1.0;
      if (pinfo.ldims[d] > 1) {
        refine = 1.0 / (1 << pinfo.level);
      }
      dx[d] = dx0[d] * refine;
    }

    /* Initialize vector potential */
    mrc_fld_foreach(fld, ix,iy,iz, 1, 2) {
      mrc_fld_data_t xx = MRC_MCRDX(crds, ix, p) - .5 * dx[0];
      mrc_fld_data_t yy = MRC_MCRDY(crds, iy, p) - .5 * dx[1];
      M3(A, 0, ix,iy,iz, p) = 
        linear_change(xx, l[0], - sub->Bz0 * yy,
            sub->x_obstacle - sub->r_obstacle, 0.);
      M3(A, 2, ix,iy,iz, p) = 
        linear_change(xx, l[0], + sub->Bx0 * yy - sub->By0 * xx,
            sub->x_obstacle - sub->r_obstacle, 0.);
    } mrc_fld_foreach_end;

    /* Initialize face-centered fields B = curl(A) */
    mrc_fld_foreach(fld, ix,iy,iz, 1, 1) {
      BX_(fld, ix,iy,iz, p) =
        + (M3(A, 2, ix    , iy+p1y, iz, p) - M3(A, 2, ix,iy,iz, p)) / dx[1]
        - (M3(A, 1, ix    , iy, iz+p1z, p) - M3(A, 1, ix,iy,iz, p)) / dx[2];
      BY_(fld, ix,iy,iz, p) =
        + (M3(A, 0, ix, iy    , iz+p1z, p) - M3(A, 0, ix,iy,iz, p)) / dx[2]
        - (M3(A, 2, ix+p1x, iy    , iz, p) - M3(A, 2, ix,iy,iz, p)) / dx[0];
      BZ_(fld, ix,iy,iz, p) =
        + (M3(A, 1, ix+p1x, iy    , iz, p) - M3(A, 1, ix,iy,iz, p)) / dx[0]
        - (M3(A, 0, ix, iy    , iz+p1y, p) - M3(A, 0, ix,iy,iz, p)) / dx[1];
    } mrc_fld_foreach_end;

    /* Initialize density, momentum, total energy */
    mrc_fld_foreach(fld, ix, iy, iz, 1, 1) {
      mrc_fld_data_t xx = MRC_CRD(crds, 0, ix) - sub->x_obstacle;
      mrc_fld_data_t yy = MRC_CRD(crds, 1, iy);
      mrc_fld_data_t zz = MRC_CRD(crds, 2, iz);

      PP_(fld, ix, iy, iz, p) = sub->p0;
      if( sqrt((xx*xx) + (yy*yy) + (zz*zz)) <= sub->r_obstacle ){
        RR_(fld, ix, iy, iz, p) = sub->rho_obstacle;
        VX_(fld, ix, iy, iz, p) = 0.;
      } else{
        RR_(fld, ix, iy, iz, p) = sub->rho0;
        VX_(fld, ix, iy, iz, p) = sub->v0;
        if( xx > 0. && sqrt((yy*yy) + (zz*zz)) <= sub->r_obstacle ){
          VX_(fld, ix, iy, iz, p) = 0.;
        }
      }
    } mrc_fld_foreach_end;
  }

  mrc_fld_destroy(A);
  mrc_fld_put_as(fld, mhd->fld);

  ggcm_mhd_convert_from_primitive(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bowshock3d_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_bowshock3d, x)
static struct param ggcm_mhd_ic_bowshock3d_descr[] = {
  { "rho_obstacle", VAR(rho_obstacle), PARAM_FLOAT(100.f)   },
  { "r_obstacle"  , VAR(r_obstacle)  , PARAM_FLOAT(.0625f)  },
  { "x_obstacle"  , VAR(x_obstacle)  , PARAM_FLOAT(-.75)    },
  { "rho0"        , VAR(rho0)        , PARAM_FLOAT(.01f)    },
  { "p0"          , VAR(p0)          , PARAM_FLOAT(.0015f)  },
  { "v0"          , VAR(v0)          , PARAM_FLOAT(1.f)     },
  { "Bx0"         , VAR(Bx0)         , PARAM_FLOAT(0.f)     },
  { "By0"         , VAR(By0)         , PARAM_FLOAT(0.001f)  },
  { "Bz0"         , VAR(Bz0)         , PARAM_FLOAT(0.f)     },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic_bowshock3d_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_bowshock3d_ops = {
  .name        = "bowshock3d",
  .size        = sizeof(struct ggcm_mhd_ic_bowshock3d),
  .param_descr = ggcm_mhd_ic_bowshock3d_descr,
  .run         = ggcm_mhd_ic_bowshock3d_run,
};

// ======================================================================
// ggcm_mhd_ic subclass "ot"

struct ggcm_mhd_ic_ot {
  // params
  double rr0;
  double v0;
  double pp0;
  double B0;

  // state
  double kx;
  double ky;
};

#define ggcm_mhd_ic_ot(ic) mrc_to_subobj(ic, struct ggcm_mhd_ic_ot)

// ----------------------------------------------------------------------
// ggcm_mhd_ic_ot_setup

static void
ggcm_mhd_ic_ot_setup(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_ot *sub = ggcm_mhd_ic_ot(ic);
  struct mrc_crds *crds = mrc_domain_get_crds(ic->mhd->domain);
  
  sub->kx = 2. * M_PI / (crds->xh[0] - crds->xl[0]);
  sub->ky = 2. * M_PI / (crds->xh[1] - crds->xl[1]);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_ot_primitive

static double
ggcm_mhd_ic_ot_primitive(struct ggcm_mhd_ic *ic, int m, double crd[3])
{
  struct ggcm_mhd_ic_ot *sub = ggcm_mhd_ic_ot(ic);

  double rr0 = sub->rr0, pp0 = sub->pp0, v0 = sub->v0;
  double B0 = sub->B0, kx = sub->kx, ky = sub->ky;
  double xx = crd[0], yy = crd[1];

  switch (m) {
  case RR: return rr0;
  case PP: return pp0;
  case VX: return -v0 * sin(ky * yy);
  case VY: return  v0 * sin(kx * xx);
  // B here won't actually be used because the vector potential takes preference
  case BX: return -B0 * sin(ky * yy);
  case BY: return  B0 * sin(2. * kx * xx);
  default: return 0.;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_ot_vector_potential

static double
ggcm_mhd_ic_ot_vector_potential(struct ggcm_mhd_ic *ic, int m, double crd[3])
{
  struct ggcm_mhd_ic_ot *sub = ggcm_mhd_ic_ot(ic);

  double B0 = sub->B0, kx = sub->kx, ky = sub->ky;
  double xx = crd[0], yy = crd[1];

  switch (m) {
  case 2: return B0 / (2. * kx) * cos(2. * kx * xx) + B0 / ky * cos(ky * yy);
  default: return 0.;
  }
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_ot_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_ot, x)
static struct param ggcm_mhd_ic_ot_descr[] = {
  { "rr0"          , VAR(rr0)          , PARAM_DOUBLE(25. / (36. * M_PI))   },
  { "v0"           , VAR(v0)           , PARAM_DOUBLE(1.)                   },
  { "pp0"          , VAR(pp0)          , PARAM_DOUBLE(5. / (12. * M_PI))    },
  { "B0"           , VAR(B0)           , PARAM_DOUBLE(0.28209479177387814)  }, // 1. / sqrt(4. * M_PI)
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic_ot_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_ot_ops = {
  .name                = "ot",
  .size                = sizeof(struct ggcm_mhd_ic_ot),
  .param_descr         = ggcm_mhd_ic_ot_descr,
  .setup               = ggcm_mhd_ic_ot_setup,
  .primitive           = ggcm_mhd_ic_ot_primitive,
  .vector_potential    = ggcm_mhd_ic_ot_vector_potential,
};

// ----------------------------------------------------------------------
// ggcm_mhd_gkeyll_create

static void
ggcm_mhd_gkeyll_create(struct ggcm_mhd *mhd)
{
  ggcm_mhd_default_box(mhd);

  /* set defaults for coord arrays */
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  mrc_crds_set_type(crds, "uniform");
  mrc_crds_set_param_int(crds, "sw", SW_2);   // 'stencil width' 
  mrc_crds_set_param_double3(crds, "l", (double[3]) {  0.0, 0.0, 0.0 });
  mrc_crds_set_param_double3(crds, "h", (double[3]) {  1.0, 1.0, 0.1 });
}

// ======================================================================
// ggcm_mhd subclass "gkeyll"

// ----------------------------------------------------------------------
// ggcm_mhd_gkeyll_ops

struct ggcm_mhd_gkeyll {
};

#define VAR(x) (void *)offsetof(struct ggcm_mhd_gkeyll, x)
static struct param ggcm_mhd_gkeyll_descr[] = {
  {},
};
#undef VAR

static struct ggcm_mhd_ops ggcm_mhd_gkeyll_ops = {
  .name             = "gkeyll",
  .size             = sizeof(struct ggcm_mhd_gkeyll),
  .param_descr      = ggcm_mhd_gkeyll_descr,
  .create           = ggcm_mhd_gkeyll_create,
};

// ======================================================================
// main

extern struct ggcm_mhd_diag_ops ggcm_mhd_diag_c_ops;

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_bowshock3d_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_ot_ops);  

  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_gkeyll_ops);  
 
  return ggcm_mhd_main(&argc, &argv);
}

