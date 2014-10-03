// MHD wave tests for waves propogating in an arbitrary direction with
// respect to the grid. The k vectors are specified by mode number in
// each direction.
//
// Whistler Initial condition (Alfven wave if d_i=0):
// The initial condition is a circularly polarized wave with k specified
// in 3D by "mode number" (number of full cycles in the box). Background
// B field is given by b_par, parallel to k, and background parallel flow
// is v_par. The perturbation strength is specified with b_perp. The
// polarization parameter is for right (+1.0) and left (-1.0) polarized
// waves.
//
// Sound Initial condition:
// Similar to whistler setup, except the background fields are rho0,
// p0, and v_par. The velocity perturbation strength is specified with
// pert.

// #define BOUNDS_CHECK
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_diag.h"
#include "ggcm_mhd_ic_private.h"

#include <mrc_fld.h>
#include <mrc_fld_as_double.h>
#include <mrc_domain.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>

// ----------------------------------------------------------------------
// return  whistler_omega
// Whistler wave dispersion relation (both branches (polarizations))
// ksq: k dot k
// k_par: k dot B / |B|
// B: background B
// rho: background density
// d_i: hall parameter (length)
// polarization == 1: right handed, polarization == -1: left handed
static double
whistler_omega(double ksq, double k_par, double B, double rho,
               double d_i, double polarization)
{
  double vA = sqrt(sqr(B) / rho);
  double p = sqr(vA) * (-2 * sqr(k_par) - sqr(d_i) * sqr(k_par) * ksq);
  double q = sqr(sqr(vA * k_par));
  return sqrt(- p / 2 + polarization * sqrt(sqr(p) / 4 - q + 1e-15));
}

// ----------------------------------------------------------------------
// whistler_omega_3d
// Whistler wave dispersion relation (both branches (polarizations))
// k: wave vector
// B: background B
// rho: background density
// d_i: hall parameter (length)
// polarization == 1: right handed, polarization == -1: left handed
static double __unused
whistler_omega_3d(double k[3], double B[3], double rho, double d_i,
                  double polarization)
{
  double k_par = 0.0;  // = k \cdot B / |B| = |k| \cos \theta
  double Bsq, Bmag, ksq, kmag;

  Bsq = sqr(B[0]) + sqr(B[1]) + sqr(B[2]);
  ksq = sqr(k[0]) + sqr(k[1]) + sqr(k[2]);
  Bmag = sqrt(Bsq);
  kmag = sqrt(ksq);
  for (int i = 0; i < 3; i++) {
    k_par += B[i] * k[i] / Bmag;
  }

  return whistler_omega(ksq, k_par, sqrt(Bsq), rho, d_i, polarization);
}

// ======================================================================
// ======================================================================
// ======================================================================
// ======================================================================
// ggcm_mhd_ic "whistler"

struct ggcm_mhd_ic_whistler {
  double rho0;  // background density / pressure
  double b_par; // background field
  double v_par; // background parallel flow
  double b_perp; // strength of B perturbation
  int m[3];  // mode number in all 3 dimensions, number of full cycles
  double polarization;  // +1.0 for right hand, -1.0 for left hand
};

// ----------------------------------------------------------------------
// calc_Arot
// Claculate vector potential for a circularly polarized wave rotated
// by the euler angles alpha1 and alpha2 where s1 = sin(alpha1) and
// c1 = cos(alpha1), and similarly for alpha2, s2, c2. Here, k is a
// scalar wave number in the rotated frame. Polarization is +1 for
// right handed, -1 for left handed.

static void
calc_Arot(double k, double r[3], double s1, double c1, double s2, double c2,
          double *Ayrot, double *Azrot, double polarization)
{
  double xrot = c1 * c2 * r[0] + c2 * s1 * r[1] - s2 * r[2];
  *Ayrot = polarization * sin(k * xrot) / k;
  *Azrot = cos(k * xrot) / k;
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_whistler_run

static void
ggcm_mhd_ic_whistler_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_whistler *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_whistler);
  struct ggcm_mhd *mhd = ic->mhd;  

  struct mrc_fld *f = mrc_fld_get_as(mhd->fld, FLD_TYPE);
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);  
  double xl[3], xh[3], L[3], r[3];
  double Aroty, Arotz;
  double pert_cc[3];
  double k[3], kunit[3];
  double ksq = 0;
  double kmag, om, v_perp;
  double alpha1, c1, s1;
  double alpha2, c2, s2;
  double dx, dy, dz;

  struct mrc_fld *A = mrc_domain_fld_create(f->_domain, 2, "Ax:Ay:Az");
  mrc_fld_set_type(A, FLD_TYPE);
  mrc_fld_setup(A); 
  struct mrc_fld *pert_fc = mrc_domain_fld_create(f->_domain, 2, "Bx:By:Bz");
  mrc_fld_set_type(pert_fc, FLD_TYPE);
  mrc_fld_setup(pert_fc);   

  mrc_crds_get_param_double3(crds, "l", xl);
  mrc_crds_get_param_double3(crds, "h", xh);
  for (int i = 0; i < 3; i++) {
    L[i] = xh[i] - xl[i];
    k[i] = 2.0 * M_PI * sub->m[i] / L[i];
    ksq += sqr(k[i]);
  }
  kmag = sqrt(ksq);
  for (int i = 0; i < 3; i++) {
    kunit[i] = k[i] / kmag;
  }
  om = whistler_omega(ksq, kmag, sub->b_par, sub->rho0, mhd->par.d_i,
                      sub->polarization);
  // from Biscamp 2000, eq 6.32
  v_perp = -kmag * sub->b_par * sub->b_perp / om;

  // find euler angles from k vector... the euler angles are for the
  // intrinsic rotation z-y'-x'' such than in the rotated frame, the
  // wave has k purely in x, and components in y and z... the rotation
  // matrices come from the euler angle wikipedia page where R=Z1*Y2*X3
  // the third euler rotation is missing since that would just rotate
  // the perpendicular wave directions, which doesn't really matter
  alpha1 = atan2(k[1], k[0]);
  s1 = sin(alpha1);
  c1 = cos(alpha1);  
  alpha2 = -atan2(k[2], sqrt(sqr(k[0]) + sqr(k[1])));
  s2 = sin(alpha2);
  c2 = cos(alpha2);
  
  // Construct a circularly polarized vector potential
  mrc_fld_foreach(f, ix,iy,iz, 1, 2) {
    // calc Ax
    r[0] = MRC_DCRDX(crds, ix);
    r[1] = 0.5 * (MRC_DCRDY(crds, iy - 1) + MRC_DCRDY(crds, iy));
    r[2] = 0.5 * (MRC_DCRDZ(crds, iz - 1) + MRC_DCRDZ(crds, iz));
    calc_Arot(kmag, r, s1, c1, s2, c2, &Aroty, &Arotz, sub->polarization);
    F3(A, 0, ix, iy, iz) = -s1 * Aroty + c1 * s2 * Arotz;

    // calc Ay
    r[0] = 0.5 * (MRC_DCRDX(crds, ix - 1) + MRC_DCRDX(crds, ix));
    r[1] = MRC_DCRDY(crds, iy);
    r[2] = 0.5 * (MRC_DCRDZ(crds, iz - 1) + MRC_DCRDZ(crds, iz));
    calc_Arot(kmag, r, s1, c1, s2, c2, &Aroty, &Arotz, sub->polarization);
    F3(A, 1, ix, iy, iz) = c1 * Aroty + s1 * s2 * Arotz;

    // calc Az
    r[0] = 0.5 * (MRC_DCRDX(crds, ix - 1) + MRC_DCRDX(crds, ix));
    r[1] = 0.5 * (MRC_DCRDY(crds, iy - 1) + MRC_DCRDY(crds, iy));
    r[2] = MRC_DCRDZ(crds, iz);
    calc_Arot(kmag, r, s1, c1, s2, c2, &Aroty, &Arotz, sub->polarization);
    F3(A, 2, ix, iy, iz) = c2 * Arotz;
  } mrc_fld_foreach_end;

  // set face centered variables
  mrc_fld_foreach(f, ix,iy,iz, 1, 1) {
    // pert_fc = curl A
    dx = 0.5 * (MRC_DCRDX(crds, ix + 1) - MRC_DCRDX(crds, ix - 1));
    dy = 0.5 * (MRC_DCRDY(crds, iy + 1) - MRC_DCRDY(crds, iy - 1));
    dz = 0.5 * (MRC_DCRDZ(crds, iz + 1) - MRC_DCRDZ(crds, iz - 1));
    F3(pert_fc, 0, ix, iy, iz) =
       (F3(A, 2, ix  , iy+1, iz  ) - F3(A, 2, ix, iy, iz)) / dy -
       (F3(A, 1, ix  , iy  , iz+1) - F3(A, 1, ix, iy, iz)) / dz;
    F3(pert_fc, 1, ix, iy, iz) = 
      -(F3(A, 2, ix+1, iy  , iz  ) - F3(A, 2, ix, iy, iz)) / dx +
       (F3(A, 0, ix  , iy  , iz+1) - F3(A, 0, ix, iy, iz)) / dz;
    F3(pert_fc, 2, ix, iy, iz) = 
       (F3(A, 1, ix+1, iy  , iz  ) - F3(A, 1, ix, iy, iz)) / dx -
       (F3(A, 0, ix  , iy+1, iz  ) - F3(A, 0, ix, iy, iz)) / dy;
    BX(f, ix,iy,iz) = sub->b_par * kunit[0] + sub->b_perp * F3(pert_fc, 0, ix, iy, iz);
    BY(f, ix,iy,iz) = sub->b_par * kunit[1] + sub->b_perp * F3(pert_fc, 1, ix, iy, iz);
    BZ(f, ix,iy,iz) = sub->b_par * kunit[2] + sub->b_perp * F3(pert_fc, 2, ix, iy, iz);
  } mrc_fld_foreach_end;

  // set cell centered variables
  mrc_fld_foreach(f, ix,iy,iz, 1, 0) {
    pert_cc[0] = 0.5 * (F3(pert_fc, 0, ix, iy, iz) + F3(pert_fc, 0, ix+1, iy, iz));
    pert_cc[1] = 0.5 * (F3(pert_fc, 1, ix, iy, iz) + F3(pert_fc, 1, ix, iy+1, iz));
    pert_cc[2] = 0.5 * (F3(pert_fc, 2, ix, iy, iz) + F3(pert_fc, 2, ix, iy, iz+1));

    // pert_cc[0] = 0.5 * (F3(pert_fc, 0, ix, iy, iz) + F3(pert_fc, 0, ix-1, iy, iz));
    // pert_cc[1] = 0.5 * (F3(pert_fc, 1, ix, iy, iz) + F3(pert_fc, 1, ix, iy-1, iz));
    // pert_cc[2] = 0.5 * (F3(pert_fc, 2, ix, iy, iz) + F3(pert_fc, 2, ix, iy, iz-1));

    RR(f, ix,iy,iz) = sub->rho0;
    PP(f, ix,iy,iz) = sub->rho0;

    VX(f, ix,iy,iz) = sub->v_par * kunit[0] + v_perp * pert_cc[0];
    VY(f, ix,iy,iz) = sub->v_par * kunit[1] + v_perp * pert_cc[1];
    VZ(f, ix,iy,iz) = sub->v_par * kunit[2] + v_perp * pert_cc[2];    
  } mrc_fld_foreach_end;

  mrc_fld_destroy(A);
  mrc_fld_destroy(pert_fc);
  mrc_fld_put_as(f, mhd->fld);

  ggcm_mhd_convert_from_primitive(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_whistler_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_whistler, x)
static struct param ggcm_mhd_ic_whistler_descr[] = {
  { "rho0"        , VAR(rho0)        , PARAM_DOUBLE(1.0)            },
  { "b_par"       , VAR(b_par)       , PARAM_DOUBLE(1.0)            },
  { "v_par"       , VAR(v_par)       , PARAM_DOUBLE(1.0)            },
  { "b_perp"      , VAR(b_perp)      , PARAM_DOUBLE(1e-3)           },
  { "m"           , VAR(m)           , PARAM_INT3(1, 0, 0)          },
  { "polarization", VAR(polarization), PARAM_DOUBLE(1.0)            },
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
// ======================================================================
// ======================================================================
// ======================================================================
// ggcm_mhd_ic "sound"

struct ggcm_mhd_ic_sound {
  double rho0;  // background density
  double p0; // background pressure
  double v_par; // background parallel flow
  double pert; // velocity perturbation strength
  int m[3];  // mode number in all 3 dimensions, number of full cycles
};

// ----------------------------------------------------------------------
// ggcm_mhd_ic_sound_run

static void
ggcm_mhd_ic_sound_run(struct ggcm_mhd_ic *ic)
{
  struct ggcm_mhd_ic_sound *sub = mrc_to_subobj(ic, struct ggcm_mhd_ic_sound);
  struct ggcm_mhd *mhd = ic->mhd;  

  struct mrc_fld *f = mrc_fld_get_as(mhd->fld, FLD_TYPE);
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);  
  double xl[3], xh[3], L[3], r[3];
  double k[3], kunit[3];
  double ksq = 0;
  double kmag;
  double cs, rho_pert, p_pert, v_pert;
  double alpha1, c1, s1;
  double alpha2, c2, s2;
  double xrot, pert;

  mrc_crds_get_param_double3(crds, "l", xl);
  mrc_crds_get_param_double3(crds, "h", xh);
  for (int i = 0; i < 3; i++) {
    L[i] = xh[i] - xl[i];
    k[i] = 2.0 * M_PI * sub->m[i] / L[i];
    ksq += sqr(k[i]);
  }
  kmag = sqrt(ksq);
  for (int i = 0; i < 3; i++) {
    kunit[i] = k[i] / kmag;
  }

  // find euler angles from k vector... the euler angles are for the
  // intrinsic rotation z-y'-x'' such than in the rotated frame, the
  // wave has k purely in x, and components in y and z... the rotation
  // matrices come from the euler angle wikipedia page where R=Z1*Y2*X3
  // the third euler rotation is missing since that would just rotate
  // the perpendicular wave directions, which doesn't really matter
  alpha1 = atan2(k[1], k[0]);
  s1 = sin(alpha1);
  c1 = cos(alpha1);  
  alpha2 = -atan2(k[2], sqrt(sqr(k[0]) + sqr(k[1])));
  s2 = sin(alpha2);
  c2 = cos(alpha2);
  
  // get strength of perturbation for p and v based on the strength
  // of the perturbation in density
  cs = sqrt(mhd->par.gamm * sub->p0 / sub->rho0);
  v_pert = sub->pert;
  rho_pert = sub->pert * sub->rho0 / cs;
  p_pert = sqr(cs) * rho_pert;

  // printf("@@@@@@@@ rho_pert %g  gamm %g cs %g  p1 %g  v1 %g\n", rho_pert, mhd->par.gamm, cs, p_pert, v_pert);

  // set cell centered variables
  mrc_fld_foreach(f, ix,iy,iz, 0, 0) {
    r[0] = MRC_DCRDX(crds, ix);
    r[1] = MRC_DCRDY(crds, iy);
    r[2] = MRC_DCRDZ(crds, iz);

    xrot = c1 * c2 * r[0] + c2 * s1 * r[1] - s2 * r[2];
    pert = sin(kmag * xrot);

    RR(f, ix,iy,iz) = sub->rho0 + rho_pert * pert;
    PP(f, ix,iy,iz) = sub->p0 + p_pert * pert;

    VX(f, ix,iy,iz) = (sub->v_par + v_pert * pert) * kunit[0];
    VY(f, ix,iy,iz) = (sub->v_par + v_pert * pert) * kunit[1];
    VZ(f, ix,iy,iz) = (sub->v_par + v_pert * pert) * kunit[2];
  } mrc_fld_foreach_end;

  mrc_fld_put_as(f, mhd->fld);
  ggcm_mhd_convert_from_primitive(mhd, mhd->fld);
}

// ----------------------------------------------------------------------
// ggcm_mhd_ic_sound_descr

#define VAR(x) (void *)offsetof(struct ggcm_mhd_ic_sound, x)
static struct param ggcm_mhd_ic_sound_descr[] = {
  { "rho0"        , VAR(rho0)        , PARAM_DOUBLE(1.0)            },
  { "p0"          , VAR(p0)          , PARAM_DOUBLE(1.0)            },
  { "v_par"       , VAR(v_par)       , PARAM_DOUBLE(1.0)            },
  { "pert"        , VAR(pert)        , PARAM_DOUBLE(1e-3)           },
  { "m"           , VAR(m)           , PARAM_INT3(1, 0, 0)          },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_ic_sound_ops

struct ggcm_mhd_ic_ops ggcm_mhd_ic_sound_ops = {
  .name        = "sound",
  .size        = sizeof(struct ggcm_mhd_ic_sound),
  .param_descr = ggcm_mhd_ic_sound_descr,
  .run         = ggcm_mhd_ic_sound_run,
};


// ======================================================================
// ggcm_mhd class "wave"

// ----------------------------------------------------------------------
// ggcm_mhd_whistler_create

static void
ggcm_mhd_wave_create(struct ggcm_mhd *mhd)
{
  ggcm_mhd_default_box(mhd);

  /* set defaults for coord arrays */
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  mrc_crds_set_type(crds, "uniform");
  mrc_crds_set_param_int(crds, "sw", SW_2);   // 'stencil width' 
  mrc_crds_set_param_double3(crds, "l", (double[3]) {  0.0, 0.0, 0.0 });
  mrc_crds_set_param_double3(crds, "h", (double[3]) {  1.0, 0.1, 0.1 });

  ggcm_mhd_set_param_int(mhd, "magdiffu", MAGDIFFU_CONST);
  ggcm_mhd_set_param_float(mhd, "diffconstant", 0.0);
}

static struct ggcm_mhd_ops ggcm_mhd_wave_ops = {
  .name             = "wave",
  .create           = ggcm_mhd_wave_create,
};

// ======================================================================

extern struct ggcm_mhd_diag_ops ggcm_mhd_diag_c_ops;

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd, &ggcm_mhd_wave_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_diag, &ggcm_mhd_diag_c_ops);
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_whistler_ops);  
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_ic, &ggcm_mhd_ic_sound_ops);  
 
  return ggcm_mhd_main(&argc, &argv);
}
