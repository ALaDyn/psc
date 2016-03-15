
#include <mrc_fld_as_double.h>
#define F1(f, m, i) MRC_D2(f, m, i)

#define ggcm_mhd_step_mhdcc_ops ggcm_mhd_step_mhdcc_double_ops
#define ggcm_mhd_step_mhdcc_name "mhdcc_double"

#include "ggcm_mhd_step_private.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_crds.h"
#include "ggcm_mhd_diag_private.h"
#include "mhd_reconstruct.h"
#include "mhd_riemann.h"
#include "mhd_util.h"

#include <mrc_domain.h>
#include <mrc_profile.h>
#include <mrc_io.h>

#include <math.h>
#include <string.h>

#include "mhd_1d.c"
#include "mhd_3d.c"
#include "mhd_sc.c"

// ======================================================================
// ggcm_mhd_step subclass "mhdcc"

struct ggcm_mhd_step_mhdcc {
  struct mhd_reconstruct *reconstruct;
  struct mhd_riemann *riemann;

  struct mrc_fld *U_1d;
  struct mrc_fld *U_l;
  struct mrc_fld *U_r;
  struct mrc_fld *W_1d;
  struct mrc_fld *W_l;
  struct mrc_fld *W_r;
  struct mrc_fld *F_1d;
  struct mrc_fld *F_cc;
  struct mrc_fld *Fl;

  bool debug_dump;
};

#define ggcm_mhd_step_mhdcc(step) mrc_to_subobj(step, struct ggcm_mhd_step_mhdcc)

// TODO:
// - handle various resistivity models
// - handle limit2, limit3
// - handle lowmask

#define REPS (1.e-10f)

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_create

static void
ggcm_mhd_step_mhdcc_create(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);

  mhd_reconstruct_set_type(sub->reconstruct, "pcm_" FLD_TYPE);
  mhd_riemann_set_type(sub->riemann, "hydro_rusanov");
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_setup

static void
ggcm_mhd_step_mhdcc_setup(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;

  assert(mhd);

  mhd_reconstruct_set_param_obj(sub->reconstruct, "mhd", mhd);
  mhd_riemann_set_param_obj(sub->riemann, "mhd", mhd);

  setup_mrc_fld_1d(sub->U_1d, mhd->fld, 5);
  setup_mrc_fld_1d(sub->U_l , mhd->fld, 5);
  setup_mrc_fld_1d(sub->U_r , mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_1d, mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_l , mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_r , mhd->fld, 5);
  setup_mrc_fld_1d(sub->F_1d, mhd->fld, 5);
  setup_mrc_fld_1d(sub->F_cc, mhd->fld, 5);
  setup_mrc_fld_1d(sub->Fl  , mhd->fld, 5);

  mhd->ymask = ggcm_mhd_get_3d_fld(mhd, 1);
  mrc_fld_set(mhd->ymask, 1.);

  ggcm_mhd_step_setup_member_objs_sub(step);
  ggcm_mhd_step_setup_super(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_destroy

static void
ggcm_mhd_step_mhdcc_destroy(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;

  ggcm_mhd_put_3d_fld(mhd, mhd->ymask);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_setup_flds

static void
ggcm_mhd_step_mhdcc_setup_flds(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;

  mrc_fld_set_type(mhd->fld, FLD_TYPE);
  mrc_fld_set_param_int(mhd->fld, "nr_ghosts", 2);
  mrc_fld_dict_add_int(mhd->fld, "mhd_type", MT_SEMI_CONSERVATIVE);
  mrc_fld_set_param_int(mhd->fld, "nr_comps", 8);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_primvar

static void
ggcm_mhd_step_mhdcc_primvar(struct ggcm_mhd_step *step, struct mrc_fld *prim,
			struct mrc_fld *x)
{
  mrc_fld_data_t gamm = step->mhd->par.gamm;
  mrc_fld_data_t s = gamm - 1.f;

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    mrc_fld_foreach(x, i,j,k, 2, 2) {
      M3(prim, RR, i,j,k, p) = RR_(x, i,j,k, p);
      mrc_fld_data_t rri = 1.f / RR_(x, i,j,k, p);
      M3(prim, VX, i,j,k, p) = rri * RVX_(x, i,j,k, p);
      M3(prim, VY, i,j,k, p) = rri * RVY_(x, i,j,k, p);
      M3(prim, VZ, i,j,k, p) = rri * RVZ_(x, i,j,k, p);
      mrc_fld_data_t rvv =
	M3(prim, VX, i,j,k, p) * RVX_(x, i,j,k, p) +
	M3(prim, VY, i,j,k, p) * RVY_(x, i,j,k, p) +
	M3(prim, VZ, i,j,k, p) * RVZ_(x, i,j,k, p);
      M3(prim, PP, i,j,k, p) = s * (UU_(x, i,j,k, p) - .5f * rvv);
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// fluxes_cc

static void
flux_pred_reconstruct(struct ggcm_mhd_step *step, 
		      struct mrc_fld *U3d_l[3], struct mrc_fld *U3d_r[3],
		      struct mrc_fld *x, struct mrc_fld *B_cc,
		      int ldim, int bnd, int j, int k, int dir, int p)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);

  struct mrc_fld *U_1d = sub->U_1d, *U_l = sub->U_l, *U_r = sub->U_r;
  struct mrc_fld *W_1d = sub->W_1d, *W_l = sub->W_l, *W_r = sub->W_r;

  pick_line_sc(U_1d, x, ldim, 1, 1, j, k, dir, p);
  mhd_prim_from_sc(step->mhd, W_1d, U_1d, ldim, 1, 1);
  mhd_reconstruct_run(sub->reconstruct, U_l, U_r, W_l, W_r, W_1d, NULL,
		      ldim, 1, 1, dir);
  put_line_sc(U3d_l[dir], U_l, ldim, 0, 1, j, k, dir, p);
  put_line_sc(U3d_r[dir], U_r, ldim, 0, 1, j, k, dir, p);
}

static void
flux_pred_riemann(struct ggcm_mhd_step *step, struct mrc_fld *fluxes[3],
		  struct mrc_fld *U3d_l[3], struct mrc_fld *U3d_r[3],
		  struct mrc_fld *B_cc,
		  int ldim, int bnd, int j, int k, int dir, int p)
{
  struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);

  struct mrc_fld *U_l = sub->U_l, *U_r = sub->U_r;
  struct mrc_fld *W_l = sub->W_l, *W_r = sub->W_r;
  struct mrc_fld *F_1d = sub->F_1d;

  pick_line_sc(U_l, U3d_l[dir], ldim, 0, 1, j, k, dir, p);
  pick_line_sc(U_r, U3d_r[dir], ldim, 0, 1, j, k, dir, p);
  mhd_prim_from_sc(step->mhd, W_l, U_l, ldim, 0, 1);
  mhd_prim_from_sc(step->mhd, W_r, U_r, ldim, 0, 1);
  mhd_riemann_run(sub->riemann, F_1d, U_l, U_r, W_l, W_r, ldim, 0, 1, dir);
  put_line_sc(fluxes[dir], F_1d, ldim, 0, 1, j, k, dir, p);
}

static inline mrc_fld_data_t
limit_hz(mrc_fld_data_t a2, mrc_fld_data_t aa, mrc_fld_data_t a1)
{
  const mrc_fld_data_t reps = 0.003;
  const mrc_fld_data_t seps = -0.001;
  const mrc_fld_data_t teps = 1.e-25;

  // Harten/Zwas type switch
  mrc_fld_data_t d1 = aa - a2;
  mrc_fld_data_t d2 = a1 - aa;
  mrc_fld_data_t s1 = fabsf(d1);
  mrc_fld_data_t s2 = fabsf(d2);
  mrc_fld_data_t f1 = fabsf(a1) + fabsf(a2) + fabsf(aa);
  mrc_fld_data_t s5 = s1 + s2 + reps*f1 + teps;
  mrc_fld_data_t r3 = fabsf(s1 - s2) / s5; // edge condition
  mrc_fld_data_t f2 = seps * f1 * f1;
  if (d1 * d2 < f2) {
    r3 = 1.f;
  }
  r3 = r3 * r3;
  r3 = r3 * r3;
  r3 = fminf(2.f * r3, 1.);

  return r3;
}

// ----------------------------------------------------------------------
// curr_c
//
// edge centered current density

static void
curr_c(struct ggcm_mhd *mhd, struct mrc_fld *j_ec, struct mrc_fld *x)
{
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  for (int p = 0; p < mrc_fld_nr_patches(x); p++) {
    float *bd4x = ggcm_mhd_crds_get_crd_p(mhd->crds, 0, BD4, p) - 1;
    float *bd4y = ggcm_mhd_crds_get_crd_p(mhd->crds, 1, BD4, p) - 1;
    float *bd4z = ggcm_mhd_crds_get_crd_p(mhd->crds, 2, BD4, p) - 1;

    mrc_fld_foreach(j_ec, i,j,k, 1, 2) {
      M3(j_ec, 0, i,j,k, p) =
	(B1(x, 2, i,j,k, p) - B1(x, 2, i,j-dy,k, p)) * bd4y[j] -
	(B1(x, 1, i,j,k, p) - B1(x, 1, i,j,k-dz, p)) * bd4z[k];
      M3(j_ec, 1, i,j,k, p) =
	(B1(x, 0, i,j,k, p) - B1(x, 0, i,j,k-dz, p)) * bd4z[k] -
	(B1(x, 2, i,j,k, p) - B1(x, 2, i-dx,j,k, p)) * bd4x[i];
      M3(j_ec, 2, i,j,k, p) =
	(B1(x, 1, i,j,k, p) - B1(x, 1, i-dx,j,k, p)) * bd4x[i] -
	(B1(x, 0, i,j,k, p) - B1(x, 0, i,j-dy,k, p)) * bd4y[j];
    } mrc_fld_foreach_end;
  }
}

// ----------------------------------------------------------------------
// curbc_c
//
// cell-centered j

static void
curbc_c(struct ggcm_mhd_step *step, struct mrc_fld *j_cc,
	struct mrc_fld *x)
{ 
  struct ggcm_mhd *mhd = step->mhd;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  // get j on edges
  struct mrc_fld *j_ec = ggcm_mhd_get_3d_fld(mhd, 3);
  curr_c(mhd, j_ec, x);

  // then average to cell centers
  for (int p = 0; p < mrc_fld_nr_patches(j_cc); p++) {
    mrc_fld_foreach(j_cc, i,j,k, 2, 1) {
      mrc_fld_data_t s = .25f;
      M3(j_cc, 0, i,j,k, p) = s * (M3(j_ec, 0, i   ,j+dy,k+dz, p) + M3(j_ec, 0, i,j   ,k+dz, p) +
				   M3(j_ec, 0, i   ,j+dy,k   , p) + M3(j_ec, 0, i,j   ,k   , p));
      M3(j_cc, 1, i,j,k, p) = s * (M3(j_ec, 1, i+dx,j   ,k+dz, p) + M3(j_ec, 1, i,j   ,k+dz, p) +
				   M3(j_ec, 1, i+dx,j   ,k   , p) + M3(j_ec, 1, i,j   ,k   , p));
      M3(j_cc, 2, i,j,k, p) = s * (M3(j_ec, 2, i+dx,j+dy,k   , p) + M3(j_ec, 2, i,j+dy,k   , p) +
				   M3(j_ec, 2, i+dx,j   ,k   , p) + M3(j_ec, 2, i,j   ,k   , p));
    } mrc_fld_foreach_end;
  }

  ggcm_mhd_put_3d_fld(mhd, j_ec);
}

static void
push_ej_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, struct mrc_fld *x_curr,
	  struct mrc_fld *prim, struct mrc_fld *x_next)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *j_ec = ggcm_mhd_get_3d_fld(mhd, 3);
  struct mrc_fld *b_cc = ggcm_mhd_get_3d_fld(mhd, 3);

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  int dx = (gdims[0] > 1), dy = (gdims[1] > 1), dz = (gdims[2] > 1);

  curr_c(mhd, j_ec, x_curr);
  compute_Bt_cc(mhd, b_cc, x_curr, 1, 1);
	
  mrc_fld_data_t s1 = .25f * dt;
  for (int p = 0; p < mrc_fld_nr_patches(x_next); p++) {
    mrc_fld_foreach(x_next, i,j,k, 0, 0) {
      mrc_fld_data_t s2 = s1;
      mrc_fld_data_t cx = (M3(j_ec, 0, i   ,j+dy,k+dz, p) + M3(j_ec, 0, i  ,j   ,k+dz, p) +
			   M3(j_ec, 0, i   ,j+dy,k   , p) + M3(j_ec, 0, i  ,j   ,k   , p));
      mrc_fld_data_t cy = (M3(j_ec, 1, i+dx,j   ,k+dz, p) + M3(j_ec, 1, i  ,j   ,k+dz, p) +
			   M3(j_ec, 1, i+dx,j   ,k   , p) + M3(j_ec, 1, i  ,j   ,k   , p));
      mrc_fld_data_t cz = (M3(j_ec, 2, i+dx,j+dy,k   , p) + M3(j_ec, 2, i  ,j+dy,k   , p) +
			   M3(j_ec, 2, i+dx,j   ,k   , p) + M3(j_ec, 2, i  ,j   ,k   , p));
      mrc_fld_data_t ffx = s2 * (cy * M3(b_cc, 2, i,j,k, p) - cz * M3(b_cc, 1, i,j,k, p));
      mrc_fld_data_t ffy = s2 * (cz * M3(b_cc, 0, i,j,k, p) - cx * M3(b_cc, 2, i,j,k, p));
      mrc_fld_data_t ffz = s2 * (cx * M3(b_cc, 1, i,j,k, p) - cy * M3(b_cc, 0, i,j,k, p));
      mrc_fld_data_t duu = (ffx * M3(prim, VX, i,j,k, p) +
			    ffy * M3(prim, VY, i,j,k, p) +
			    ffz * M3(prim, VZ, i,j,k, p));
      
      M3(x_next, RVX, i,j,k, p) += ffx;
      M3(x_next, RVY, i,j,k, p) += ffy;
      M3(x_next, RVZ, i,j,k, p) += ffz;
      M3(x_next, UU , i,j,k, p) += duu;
    } mrc_fld_foreach_end;
  }

  ggcm_mhd_put_3d_fld(mhd, j_ec);
  ggcm_mhd_put_3d_fld(mhd, b_cc);
}

static void
res1_const_c(struct ggcm_mhd *mhd, struct mrc_fld *resis)
{
  // resistivity comes in ohm*m
  int diff_obnd = mhd->par.diff_obnd;
  mrc_fld_data_t eta0i = 1. / mhd->resnorm;
  mrc_fld_data_t diffsphere2 = sqr(mhd->par.diffsphere);
  mrc_fld_data_t diff = mhd->par.diffco * eta0i;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  for (int p = 0; p < mrc_fld_nr_patches(resis); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);

    mrc_fld_foreach(resis, i,j,k, 1, 1) {
      M3(resis, 0, i,j,k, p) = 0.f;
      mrc_fld_data_t r2 = (sqr(MRC_MCRDX(crds, i, p)) +
			   sqr(MRC_MCRDY(crds, j, p)) +
			   sqr(MRC_MCRDZ(crds, k, p)));
      if (r2 < diffsphere2)
	continue;
      if (j + info.off[1] < diff_obnd)
	continue;
      if (k + info.off[2] < diff_obnd)
	continue;
      if (i + info.off[0] >= gdims[0] - diff_obnd)
	continue;
      if (j + info.off[1] >= gdims[1] - diff_obnd)
	continue;
      if (k + info.off[2] >= gdims[2] - diff_obnd)
	continue;
      
      M3(resis, 0, i,j,k, p) = diff;
    } mrc_fld_foreach_end;
  }
}

static void
calc_resis_const_c(struct ggcm_mhd_step *step, struct mrc_fld *curr,
		   struct mrc_fld *resis, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;

  curbc_c(step, curr, x);
  res1_const_c(mhd, resis);
}

static inline mrc_fld_data_t
bcthy3f(mrc_fld_data_t s1, mrc_fld_data_t s2)
{
  if (s1 > 0.f && fabsf(s2) > REPS) {
    return s1 / s2;
  }
  return 0.f;
}

static inline void
calc_avg_dz_By(struct ggcm_mhd_step *step, struct mrc_fld *tmp,
	       struct mrc_fld *x, struct mrc_fld *b0, int XX, int YY, int ZZ,
	       int JX1_, int JY1_, int JZ1_, int JX2_, int JY2_, int JZ2_)
{
  struct ggcm_mhd *mhd = step->mhd;

  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);

  int JX1 = (gdims[0] > 1) ? JX1_ : 0;
  int JY1 = (gdims[1] > 1) ? JY1_ : 0;
  int JZ1 = (gdims[2] > 1) ? JZ1_ : 0;
  int JX2 = (gdims[0] > 1) ? JX2_ : 0;
  int JY2 = (gdims[1] > 1) ? JY2_ : 0;
  int JZ2 = (gdims[2] > 1) ? JZ2_ : 0;

  // d_z B_y, d_y B_z on x edges
  for (int p = 0; p < mrc_fld_nr_patches(tmp); p++) {
    float *bd1x = ggcm_mhd_crds_get_crd_p(mhd->crds, 0, BD1, p);
    float *bd1y = ggcm_mhd_crds_get_crd_p(mhd->crds, 1, BD1, p);
    float *bd1z = ggcm_mhd_crds_get_crd_p(mhd->crds, 2, BD1, p);

    mrc_fld_foreach(tmp, i,j,k, 1, 2) {
      mrc_fld_data_t bd1[3] = { bd1x[i-1], bd1y[j-1], bd1z[k-1] };
      
      M3(tmp, 0, i,j,k, p) = bd1[ZZ] * 
	(BT(x, YY, i,j,k, p) - BT(x, YY, i-JX2,j-JY2,k-JZ2, p));
      M3(tmp, 1, i,j,k, p) = bd1[YY] * 
	(BT(x, ZZ, i,j,k, p) - BT(x, ZZ, i-JX1,j-JY1,k-JZ1, p));
    } mrc_fld_foreach_end;
  }

  // .5 * harmonic average if same sign
  for (int p = 0; p < mrc_fld_nr_patches(tmp); p++) {
    mrc_fld_foreach(tmp, i,j,k, 1, 1) {
      mrc_fld_data_t s1, s2;
      // dz_By on y face
      s1 = M3(tmp, 0, i+JX2,j+JY2,k+JZ2, p) * M3(tmp, 0, i,j,k, p);
      s2 = M3(tmp, 0, i+JX2,j+JY2,k+JZ2, p) + M3(tmp, 0, i,j,k, p);
      M3(tmp, 2, i,j,k, p) = bcthy3f(s1, s2);
      // dy_Bz on z face
      s1 = M3(tmp, 1, i+JX1,j+JY1,k+JZ1, p) * M3(tmp, 1, i,j,k, p);
      s2 = M3(tmp, 1, i+JX1,j+JY1,k+JZ1, p) + M3(tmp, 1, i,j,k, p);
      M3(tmp, 3, i,j,k, p) = bcthy3f(s1, s2);
    } mrc_fld_foreach_end;
  }
}

#define CC_TO_EC(f, m, i,j,k, I,J,K, p)		\
  ({						\
    int I_ = (gdims[0] > 1 ) ? I : 0;		\
    int J_ = (gdims[1] > 1 ) ? J : 0;		\
    int K_ = (gdims[2] > 1 ) ? K : 0;		\
    (.25f * (M3(f, m, i-I_,j-J_,k-K_, p) +	\
	     M3(f, m, i-I_,j   ,k   , p) +	\
	     M3(f, m, i   ,j-J_,k   , p) +	\
	     M3(f, m, i   ,j   ,k-K_, p)));})

// ve = v - d_i J
static inline void
calc_ve_x_B(struct ggcm_mhd_step *step,
	    mrc_fld_data_t ttmp[2], struct mrc_fld *x,
	    struct mrc_fld *prim, struct mrc_fld *curr, struct mrc_fld *tmp,
	    int i, int j, int k,
	    int XX, int YY, int ZZ, int I, int J, int K, int p,
	    int JX1, int JY1, int JZ1, int JX2, int JY2, int JZ2,
	    float *bd2x, float *bd2y, float *bd2z, mrc_fld_data_t dt)
{
  // struct ggcm_mhd_step_mhdcc *sub = ggcm_mhd_step_mhdcc(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *b0 = mhd->b0;
  mrc_fld_data_t d_i = mhd->par.d_i;
  
  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);
  
  mrc_fld_data_t vcurrYY = CC_TO_EC(curr, YY, i, j, k, I, J, K, p);
  mrc_fld_data_t vcurrZZ = CC_TO_EC(curr, ZZ, i, j, k, I, J, K, p);
  
  mrc_fld_data_t bd2m[3] = { bd2x[i-1], bd2y[j-1], bd2z[k-1] };
  mrc_fld_data_t bd2[3] = { bd2x[i], bd2y[j], bd2z[k] };
  mrc_fld_data_t vbZZ;
  // edge centered velocity
  mrc_fld_data_t vvYY = CC_TO_EC(prim, VX + YY, i,j,k, I,J,K, p) - d_i * vcurrYY;
  if (vvYY > 0.f) {
    vbZZ = BT(x, ZZ, i-JX1,j-JY1,k-JZ1, p) +
      M3(tmp, 3, i-JX1,j-JY1,k-JZ1, p) * (bd2m[YY] - dt*vvYY);
  } else {
    vbZZ = BT(x, ZZ, i,j,k, p) -
      M3(tmp, 3, i,j,k, p) * (bd2[YY] + dt*vvYY);
  }
  ttmp[0] = vbZZ * vvYY;
  
  mrc_fld_data_t vbYY;
  // edge centered velocity
  mrc_fld_data_t vvZZ = CC_TO_EC(prim, VX + ZZ, i,j,k, I,J,K, p) - d_i * vcurrZZ;
  if (vvZZ > 0.f) {
    vbYY = BT(x, YY, i-JX2,j-JY2,k-JZ2, p) +
      M3(tmp, 2, i-JX2,j-JY2,k-JZ2, p) * (bd2m[ZZ] - dt*vvZZ);
  } else {
    vbYY = BT(x, YY, i,j,k, p) -
      M3(tmp, 2, i,j,k, p) * (bd2[ZZ] + dt*vvZZ);
  }
  ttmp[1] = vbYY * vvZZ;
}

static inline void
bcthy3z_const(struct ggcm_mhd_step *step, int XX, int YY, int ZZ, int I, int J, int K,
	      int _JX1, int _JY1, int _JZ1, int _JX2, int _JY2, int _JZ2,
	      struct mrc_fld *E, mrc_fld_data_t dt, struct mrc_fld *x,
	      struct mrc_fld *prim, struct mrc_fld *curr, struct mrc_fld *resis,
	      struct mrc_fld *b0)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *tmp = ggcm_mhd_get_3d_fld(mhd, 4);

  int gdims[3];
  mrc_domain_get_global_dims(x->_domain, gdims);

  int JX1 = (gdims[0] > 1 ) ? _JX1 : 0;
  int JY1 = (gdims[1] > 1 ) ? _JY1 : 0;
  int JZ1 = (gdims[2] > 1 ) ? _JZ1 : 0;
  int JX2 = (gdims[0] > 1 ) ? _JX2 : 0;
  int JY2 = (gdims[1] > 1 ) ? _JY2 : 0;
  int JZ2 = (gdims[2] > 1 ) ? _JZ2 : 0;

  calc_avg_dz_By(step, tmp, x, b0, XX, YY, ZZ, JX1, JY1, JZ1, JX2, JY2, JZ2);

  // edge centered E = - ve x B (+ dissipation)
  for (int p = 0; p < mrc_fld_nr_patches(E); p++) {
    float *bd2x = ggcm_mhd_crds_get_crd_p(mhd->crds, 0, BD2, p);
    float *bd2y = ggcm_mhd_crds_get_crd_p(mhd->crds, 1, BD2, p);
    float *bd2z = ggcm_mhd_crds_get_crd_p(mhd->crds, 2, BD2, p);

    mrc_fld_foreach(E, i,j,k, 0, 1) {
      mrc_fld_data_t ttmp[2];
      calc_ve_x_B(step, ttmp, x, prim, curr, tmp, i, j, k, XX, YY, ZZ, I, J, K, p,
		  JX1, JY1, JZ1, JX2, JY2, JZ2, bd2x, bd2y, bd2z, dt);
      
      mrc_fld_data_t vcurrXX = CC_TO_EC(curr, XX, i,j,k, I,J,K, p);
      mrc_fld_data_t vresis = CC_TO_EC(resis, 0, i,j,k, I,J,K, p);
      M3(E, XX, i,j,k, p) = - (ttmp[0] - ttmp[1]) + vresis * vcurrXX;
    } mrc_fld_foreach_end;
  }

  ggcm_mhd_put_3d_fld(mhd, tmp);
}

static void
calce_const_c(struct ggcm_mhd_step *step, struct mrc_fld *E,
	      mrc_fld_data_t dt, struct mrc_fld *x, struct mrc_fld *prim,
	      struct mrc_fld *curr, struct mrc_fld *resis)
{
  struct mrc_fld *b0 = step->mhd->b0;

  if (b0) {
    bcthy3z_const(step, 0,1,2, 0,1,1, 0,1,0, 0,0,1, E, dt, x, prim, curr, resis, b0);
    bcthy3z_const(step, 1,2,0, 1,0,1, 0,0,1, 1,0,0, E, dt, x, prim, curr, resis, b0);
    bcthy3z_const(step, 2,0,1, 1,1,0, 1,0,0, 0,1,0, E, dt, x, prim, curr, resis, b0);
  } else {
    bcthy3z_const(step, 0,1,2, 0,1,1, 0,1,0, 0,0,1, E, dt, x, prim, curr, resis, NULL);
    bcthy3z_const(step, 1,2,0, 1,0,1, 0,0,1, 1,0,0, E, dt, x, prim, curr, resis, NULL);
    bcthy3z_const(step, 2,0,1, 1,1,0, 1,0,0, 0,1,0, E, dt, x, prim, curr, resis, NULL);
  }
}

static void
calce_c(struct ggcm_mhd_step *step, struct mrc_fld *E,
	struct mrc_fld *x, struct mrc_fld *prim, mrc_fld_data_t dt)
{
  struct ggcm_mhd *mhd = step->mhd;

  switch (mhd->par.magdiffu) {
  case MAGDIFFU_CONST: {
    struct mrc_fld *curr = ggcm_mhd_get_3d_fld(mhd, 3);
    struct mrc_fld *resis = ggcm_mhd_get_3d_fld(mhd, 1);

    calc_resis_const_c(step, curr, resis, x);
    calce_const_c(step, E, dt, x, prim, curr, resis);

    ggcm_mhd_put_3d_fld(mhd, curr);
    ggcm_mhd_put_3d_fld(mhd, resis);
    break;
  }
  default:
    assert(0);
  }
}

static void
pushstage_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt,
	    struct mrc_fld *x_curr, struct mrc_fld *x_next,
	    struct mrc_fld *prim)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *fluxes[3] = { ggcm_mhd_get_3d_fld(mhd, 5),
				ggcm_mhd_get_3d_fld(mhd, 5),
				ggcm_mhd_get_3d_fld(mhd, 5), };
  struct mrc_fld *E = ggcm_mhd_get_3d_fld(mhd, 3);

  struct mrc_fld *U_l[3] = { ggcm_mhd_get_3d_fld(mhd, 5),
			     ggcm_mhd_get_3d_fld(mhd, 5),
			     ggcm_mhd_get_3d_fld(mhd, 5), };
  struct mrc_fld *U_r[3] = { ggcm_mhd_get_3d_fld(mhd, 5),
			     ggcm_mhd_get_3d_fld(mhd, 5),
			     ggcm_mhd_get_3d_fld(mhd, 5), };
  
  mhd_fluxes_reconstruct(step, U_l, U_r, x_curr, NULL, 0, 0,
			 flux_pred_reconstruct);
  ggcm_mhd_fill_ghosts_reconstr(mhd, U_l, U_r);
  mhd_fluxes_riemann(step, fluxes, U_l, U_r, NULL, 0, 0,
		     flux_pred_riemann);
  
  ggcm_mhd_put_3d_fld(mhd, U_l[0]);
  ggcm_mhd_put_3d_fld(mhd, U_l[1]);
  ggcm_mhd_put_3d_fld(mhd, U_l[2]);
  ggcm_mhd_put_3d_fld(mhd, U_r[0]);
  ggcm_mhd_put_3d_fld(mhd, U_r[1]);
  ggcm_mhd_put_3d_fld(mhd, U_r[2]);

  update_finite_volume(mhd, x_next, fluxes, mhd->ymask, dt, true);

  push_ej_c(step, dt, x_curr, prim, x_next);

  calce_c(step, E, x_curr, prim, dt);
  //  ggcm_mhd_fill_ghosts_E(mhd, E);
  update_ct(mhd, x_next, E, dt, true);

  ggcm_mhd_put_3d_fld(mhd, E);
  ggcm_mhd_put_3d_fld(mhd, fluxes[0]);
  ggcm_mhd_put_3d_fld(mhd, fluxes[1]);
  ggcm_mhd_put_3d_fld(mhd, fluxes[2]);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_mhdcc_run

static void
ggcm_mhd_step_mhdcc_run(struct ggcm_mhd_step *step, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *x_half = ggcm_mhd_get_3d_fld(mhd, 8);
  mrc_fld_dict_add_int(x_half, "mhd_type", MT_SEMI_CONSERVATIVE);
  struct mrc_fld *prim = ggcm_mhd_get_3d_fld(mhd, 5);

  static int pr_A, pr_B;
  if (!pr_A) {
    pr_A = prof_register("mhdcc_pred", 0, 0, 0);
    pr_B = prof_register("mhdcc_corr", 0, 0, 0);
  }

#if 0
  if (sub->debug_dump) {
    static struct ggcm_mhd_diag *diag;
    static int cnt;
    if (!diag) {
      diag = ggcm_mhd_diag_create(ggcm_mhd_comm(mhd));
      ggcm_mhd_diag_set_type(diag, "c");
      ggcm_mhd_diag_set_name(diag, "ggcm_mhd_debug");
      ggcm_mhd_diag_set_param_obj(diag, "mhd", mhd);
      ggcm_mhd_diag_set_param_string(diag, "fields", "rr1:rv1:uu1:b1:rr:v:pp:b:divb:ymask:zmask");
      ggcm_mhd_diag_set_from_options(diag);
      ggcm_mhd_diag_set_param_string(diag, "run", "dbg0");
      ggcm_mhd_diag_setup(diag);
      ggcm_mhd_diag_view(diag);
    }
    ggcm_mhd_fill_ghosts(mhd, mhd->fld, 0, mhd->time);
    ggcm_mhd_diag_run_now(diag, mhd->fld, DIAG_TYPE_3D, cnt++);
  }
#endif

  mrc_fld_data_t dtn = 0.0;
  if (step->do_nwst) {
    ggcm_mhd_fill_ghosts(mhd, x, 0, mhd->time);
    dtn = newstep_sc(mhd, x, NULL, 0);
    // yes, dtn isn't set to mhd->dt until the end of the step... this
    // is what the fortran code did    
  }

  // --- PREDICTOR
  prof_start(pr_A);
  ggcm_mhd_fill_ghosts(mhd, x, 0, mhd->time);
  ggcm_mhd_step_mhdcc_primvar(step, prim, x);
  // --- check for NaNs and negative pressures
  // (still controlled by do_badval_checks)
  badval_checks_sc(mhd, x, prim);

  // set x_half = x^n, then advance to n+1/2
  mrc_fld_copy_range(x_half, x, 0, 8);
  pushstage_c(step, .5f * mhd->dt, x, x_half, prim);
  prof_stop(pr_A);

#if 0
  static struct ggcm_mhd_diag *diag;
  static int cnt;
  if (!diag) {
    diag = ggcm_mhd_diag_create(ggcm_mhd_comm(mhd));
    ggcm_mhd_diag_set_type(diag, "c");
    ggcm_mhd_diag_set_param_obj(diag, "mhd", mhd);
    ggcm_mhd_diag_set_param_string(diag, "run", "dbg1");
    ggcm_mhd_diag_set_param_string(diag, "fields", "rr1:rv1:uu1:b1:rr:v:pp:b:divb");
    ggcm_mhd_diag_setup(diag);
    ggcm_mhd_diag_view(diag);
  }
  ggcm_mhd_fill_ghosts(mhd, x_half, 0, mhd->time);
  ggcm_mhd_diag_run_now(diag, x_half, DIAG_TYPE_3D, cnt++);
#endif

  // --- CORRECTOR
  prof_start(pr_B);
  ggcm_mhd_fill_ghosts(mhd, x_half, 0, mhd->time + mhd->bndt);
  ggcm_mhd_step_mhdcc_primvar(step, prim, x_half);
  // --- check for NaNs and negative pressures
  // (still controlled by do_badval_checks)
  badval_checks_sc(mhd, x_half, prim);
  pushstage_c(step, mhd->dt, x_half, x, prim);
  prof_stop(pr_B);

  // --- update timestep
  if (step->do_nwst) {
    dtn = mrc_fld_min(1., dtn); // FIXME, only kept for compatibility

    if (dtn > 1.02 * mhd->dt || dtn < mhd->dt / 1.01) {
      mpi_printf(ggcm_mhd_comm(mhd), "switched dt %g <- %g\n", dtn, mhd->dt);

      // FIXME: determining when to die on a bad dt should be generalized, since
      //        there's another hiccup if refining dt for actual AMR
      bool first_step = mhd->istep <= 1;
      bool last_step = mhd->time + dtn > (1.0 - 1e-5) * mhd->max_time;

      if (!first_step && !last_step &&
          (dtn < 0.5 * mhd->dt || dtn > 2.0 * mhd->dt)) {            
        mpi_printf(ggcm_mhd_comm(mhd), "!!! dt changed by > a factor of 2. "
                   "Dying now!\n");
	//        ggcm_mhd_wrongful_death(mhd, mhd->fld, 2);
      }

      mhd->dt = dtn;
    }
  }

  ggcm_mhd_put_3d_fld(mhd, x_half);
  ggcm_mhd_put_3d_fld(mhd, prim);
}

// ----------------------------------------------------------------------
// subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step_mhdcc, x)
static struct param ggcm_mhd_step_mhdcc_descr[] = {
  { "debug_dump"      , VAR(debug_dump)      , PARAM_BOOL(false)            },
  
  { "reconstruct"     , VAR(reconstruct)     , MRC_VAR_OBJ(mhd_reconstruct) },
  { "riemann"         , VAR(riemann)         , MRC_VAR_OBJ(mhd_riemann)     },

  { "U_1d"            , VAR(U_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  { "U_l"             , VAR(U_l)             , MRC_VAR_OBJ(mrc_fld)         },
  { "U_r"             , VAR(U_r)             , MRC_VAR_OBJ(mrc_fld)         },
  { "W_1d"            , VAR(W_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  { "W_l"             , VAR(W_l)             , MRC_VAR_OBJ(mrc_fld)         },
  { "W_r"             , VAR(W_r)             , MRC_VAR_OBJ(mrc_fld)         },
  { "F_1d"            , VAR(F_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  { "F_cc"            , VAR(F_cc)            , MRC_VAR_OBJ(mrc_fld)         },
  { "Fl"              , VAR(Fl)              , MRC_VAR_OBJ(mrc_fld)         },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_step subclass "mhdcc_*"

struct ggcm_mhd_step_ops ggcm_mhd_step_mhdcc_ops = {
  .name                = ggcm_mhd_step_mhdcc_name,
  .size                = sizeof(struct ggcm_mhd_step_mhdcc),
  .param_descr         = ggcm_mhd_step_mhdcc_descr,
  .create              = ggcm_mhd_step_mhdcc_create,
  .setup               = ggcm_mhd_step_mhdcc_setup,
  .run                 = ggcm_mhd_step_mhdcc_run,
  .destroy             = ggcm_mhd_step_mhdcc_destroy,
  .setup_flds          = ggcm_mhd_step_mhdcc_setup_flds,
};
