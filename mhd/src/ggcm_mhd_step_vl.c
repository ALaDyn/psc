
#include <ggcm_mhd_step_private.h>

#include <ggcm_mhd_private.h>
#include <ggcm_mhd_defs.h>
#include <mhd_riemann.h>
#include <mhd_reconstruct.h>

#include <mrc_domain.h>
#include <mrc_ddc.h>
#include <mrc_ts.h>
#include <mrc_fld_as_double_aos.h>
#define F1(f, m, i) MRC_D2(f, m, i)

#include "mhd_1d.c"
#include "mhd_3d.c"

static int ldims[3];

// ======================================================================
// ggcm_mhd_step subclass "vl"

struct ggcm_mhd_step_vl {
  struct mhd_reconstruct *reconstruct_pred;
  struct mhd_reconstruct *reconstruct_corr;
  struct mhd_riemann *riemann;

  struct mrc_fld *U_1d;
  struct mrc_fld *U_l;
  struct mrc_fld *U_r;
  struct mrc_fld *W_1d;
  struct mrc_fld *W_l;
  struct mrc_fld *W_r;
  struct mrc_fld *F_1d;
};

#define ggcm_mhd_step_vl(step) mrc_to_subobj(step, struct ggcm_mhd_step_vl)

// ======================================================================

static void
flux_pred(struct ggcm_mhd_step *step, struct mrc_fld *flux[3], struct mrc_fld *x, struct mrc_fld *B_cc,
	  int ldim, int nghost, int j, int k, int dir)
{
  struct ggcm_mhd_step_vl *sub = ggcm_mhd_step_vl(step);
  struct mrc_fld *U_1d = sub->U_1d, *U_l = sub->U_l, *U_r = sub->U_r;
  struct mrc_fld *W_1d = sub->W_1d, *W_l = sub->W_l, *W_r = sub->W_r;
  struct mrc_fld *F_1d = sub->F_1d;

  pick_line_sc(U_1d, x, ldim, 2, 2, j, k, dir);
  mhd_prim_from_sc(step->mhd, W_1d, U_1d, ldim, 2, 2); // for up to plm reconstruction
  mhd_reconstruct_run(sub->reconstruct_pred, U_l, U_r, W_l, W_r, W_1d, NULL,
		      ldim, 1, 1, dir);
  mhd_riemann_run(sub->riemann, F_1d, U_l, U_r, W_l, W_r, ldim, 0, 1, dir);
  put_line_sc(flux[dir], F_1d, j, k, ldim, 0, 1, dir);
}

static void
flux_corr(struct ggcm_mhd_step *step, struct mrc_fld *flux[3], struct mrc_fld *x, struct mrc_fld *B_cc,
	  int ldim, int nghost, int j, int k, int dir)
{
  struct ggcm_mhd_step_vl *sub = ggcm_mhd_step_vl(step);
  struct mrc_fld *U_1d = sub->U_1d, *U_l = sub->U_l, *U_r = sub->U_r;
  struct mrc_fld *W_1d = sub->W_1d, *W_l = sub->W_l, *W_r = sub->W_r;
  struct mrc_fld *F_1d = sub->F_1d;

  pick_line_sc(U_1d, x, ldim, 2, 2, j, k, dir);
  mhd_prim_from_sc(step->mhd, W_1d, U_1d, ldim, 2, 2); // for up to plm reconstruction
  mhd_reconstruct_run(sub->reconstruct_corr, U_l, U_r, W_l, W_r, W_1d, NULL,
		      ldim, 1, 1, dir);
  mhd_riemann_run(sub->riemann, F_1d, U_l, U_r, W_l, W_r, ldim, 0, 1, dir);
  put_line_sc(flux[dir], F_1d, j, k, ldim, 0, 1, dir);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl_create

static void
ggcm_mhd_step_vl_create(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_vl *sub = ggcm_mhd_step_vl(step);

  mhd_reconstruct_set_type(sub->reconstruct_pred, "pcm_double");
  mhd_reconstruct_set_type(sub->reconstruct_corr, "plm_double");
  mhd_riemann_set_type(sub->riemann, "hydro_rusanov");
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl_setup

static void
ggcm_mhd_step_vl_setup(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_vl *sub = ggcm_mhd_step_vl(step);
  struct ggcm_mhd *mhd = step->mhd;

  mhd_reconstruct_set_param_obj(sub->reconstruct_pred, "mhd", mhd);
  mhd_reconstruct_set_param_obj(sub->reconstruct_corr, "mhd", mhd);
  mhd_riemann_set_param_obj(sub->riemann, "mhd", mhd);

  setup_mrc_fld_1d(sub->U_1d, mhd->fld, 5);
  setup_mrc_fld_1d(sub->U_l , mhd->fld, 5);
  setup_mrc_fld_1d(sub->U_r , mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_1d, mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_l , mhd->fld, 5);
  setup_mrc_fld_1d(sub->W_r , mhd->fld, 5);
  setup_mrc_fld_1d(sub->F_1d, mhd->fld, 5);

  ggcm_mhd_step_setup_member_objs_sub(step);
}

// ----------------------------------------------------------------------
// newstep_hydro

static mrc_fld_data_t
newstep_hydro(struct ggcm_mhd *mhd, struct mrc_fld *x)
{
  mrc_fld_data_t gamma = mhd->par.gamm;
  mrc_fld_data_t gamma_minus_1 = gamma - 1.;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  double dx[3]; mrc_crds_get_dx(crds, dx);

  mrc_fld_data_t max_vx = 0., max_vy = 0., max_vz = 0.;
  mrc_fld_foreach(x, i,j,k, 0, 0) {
    mrc_fld_data_t rri = 1. / RR(x, i,j,k);
    mrc_fld_data_t vx = RVX(x, i,j,k) * rri;
    mrc_fld_data_t vy = RVY(x, i,j,k) * rri;
    mrc_fld_data_t vz = RVZ(x, i,j,k) * rri;
    
    /* compute sound speed */
    mrc_fld_data_t pp = mrc_fld_max(gamma_minus_1*(EE(x, i,j,k) - .5*RR(x, i,j,k) * (sqr(vx) + sqr(vy) + sqr(vz))), 1e-15);
    mrc_fld_data_t cs = mrc_fld_sqrt(gamma * pp * rri);
    
    /* compute maximum cfl velocity (corresponding to minimum dt) */
    max_vx = mrc_fld_max(max_vx, fabs(vx) + cs);
    max_vy = mrc_fld_max(max_vy, fabs(vy) + cs);
    max_vz = mrc_fld_max(max_vz, fabs(vz) + cs);
  } mrc_fld_foreach_end;
  
  mrc_fld_data_t cfl = mhd->par.thx;
  mrc_fld_data_t local_dt = cfl * mrc_fld_min(dx[0] / max_vx, mrc_fld_min(dx[1] / max_vy, dx[2] / max_vz));
  mrc_fld_data_t global_dt;
  MPI_Allreduce(&local_dt, &global_dt, 1, MPI_MRC_FLD_DATA_T, MPI_MIN, ggcm_mhd_comm(mhd));
  return global_dt;
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl_run

static void
ggcm_mhd_step_vl_run(struct ggcm_mhd_step *step, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;

  ldims[0] = mrc_fld_spatial_dims(x)[0];
  ldims[1] = mrc_fld_spatial_dims(x)[1];
  ldims[2] = mrc_fld_spatial_dims(x)[2];

  struct mrc_fld *x_half = ggcm_mhd_step_get_3d_fld(step, 8);
  struct mrc_fld *flux[3] = { ggcm_mhd_step_get_3d_fld(step, 5),
			      ggcm_mhd_step_get_3d_fld(step, 5),
			      ggcm_mhd_step_get_3d_fld(step, 5), };

  // CFL CONDITION

  ggcm_mhd_fill_ghosts(mhd, x, 0, mhd->time);

  mhd->dt = newstep_hydro(mhd, x);
  mrc_fld_data_t dt = mhd->dt;

  // PREDICTOR

  // ghosts have already been set
  mrc_fld_copy_range(x_half, x, 0, 5);
  mhd_fluxes(step, flux, x, NULL, 0, 99, flux_pred);
  update_finite_volume_uniform(mhd, x_half, flux, .5 * dt, 0, 0);

  // CORRECTOR

  ggcm_mhd_fill_ghosts(mhd, x_half, 0, mhd->time);
  mhd_fluxes(step, flux, x_half, NULL, 0, 99, flux_corr);
  update_finite_volume_uniform(mhd, x, flux, dt, 0, 0);

  // clean up

  ggcm_mhd_step_put_3d_fld(step, x_half);
  ggcm_mhd_step_put_3d_fld(step, flux[0]);
  ggcm_mhd_step_put_3d_fld(step, flux[1]);
  ggcm_mhd_step_put_3d_fld(step, flux[2]);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl_setup_flds

static void
ggcm_mhd_step_vl_setup_flds(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd *mhd = step->mhd;

  mrc_fld_set_type(mhd->fld, FLD_TYPE);
  mrc_fld_set_param_int(mhd->fld, "nr_ghosts", 2);
  mrc_fld_dict_add_int(mhd->fld, "mhd_type", MT_FULLY_CONSERVATIVE);
  mrc_fld_set_param_int(mhd->fld, "nr_comps", 8); // FIXME, should be 5, but needs testing
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step_vl, x)
static struct param ggcm_mhd_step_vl_descr[] = {
  { "reconstruct_pred", VAR(reconstruct_pred), MRC_VAR_OBJ(mhd_reconstruct) },
  { "reconstruct_corr", VAR(reconstruct_corr), MRC_VAR_OBJ(mhd_reconstruct) },
  { "riemann"         , VAR(riemann)         , MRC_VAR_OBJ(mhd_riemann)     },

  { "U_1d"            , VAR(U_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  { "U_l"             , VAR(U_l)             , MRC_VAR_OBJ(mrc_fld)         },
  { "U_r"             , VAR(U_r)             , MRC_VAR_OBJ(mrc_fld)         },
  { "W_1d"            , VAR(W_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  { "W_l"             , VAR(W_l)             , MRC_VAR_OBJ(mrc_fld)         },
  { "W_r"             , VAR(W_r)             , MRC_VAR_OBJ(mrc_fld)         },
  { "F_1d"            , VAR(F_1d)            , MRC_VAR_OBJ(mrc_fld)         },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_step_vl_ops
//
// This scheme is a hydro-solver only -- still working on 8-component fields,
// but ignoring the magnetic field completely.

struct ggcm_mhd_step_ops ggcm_mhd_step_vl_ops = {
  .name             = "vl",
  .size             = sizeof(struct ggcm_mhd_step_vl),
  .param_descr      = ggcm_mhd_step_vl_descr,
  .create           = ggcm_mhd_step_vl_create,
  .setup            = ggcm_mhd_step_vl_setup,
  .run              = ggcm_mhd_step_vl_run,
  .setup_flds       = ggcm_mhd_step_vl_setup_flds,
};

