
#include "ggcm_mhd_step_private.h"
#include "ggcm_mhd_private.h"
#include "ggcm_mhd_defs.h"
#include "ggcm_mhd_crds.h"

#include <mrc_domain.h>
#include <mrc_profile.h>

#include <math.h>

// FIXME, mv to right place
static void
mrc_fld_copy_range(struct mrc_fld *to, struct mrc_fld *from, int mb, int me)
{
  assert(to->_nr_ghosts == from->_nr_ghosts);
  int bnd = to->_nr_ghosts;

   mrc_fld_foreach(to, ix,iy,iz, bnd, bnd) {
    for (int m = mb; m < me; m++) {
      F3(to, m, ix,iy,iz) = F3(from, m, ix,iy,iz);
    }
  } mrc_fld_foreach_end;
}

// ======================================================================
// ggcm_mhd_step subclass "c3"

struct ggcm_mhd_step_c3 {
  struct mrc_fld *x_half;
  struct mrc_fld *prim;

  struct mrc_fld *masks;
  struct mrc_fld *bc;
  struct mrc_fld *flux;
  struct mrc_fld *tmp;
};

#define ggcm_mhd_step_c3(step) mrc_to_subobj(step, struct ggcm_mhd_step_c3)

// TODO:
// - handle various resistivity models
// - handle limit2, limit3
// - handle lowmask

#define REPS (1.e-10f)

enum {
  LIMIT_NONE,
  LIMIT_1,
};

static void
setup_mrc_fld_3d(struct mrc_fld *x, struct mrc_fld *tmpl, int nr_comps)
{
  mrc_fld_set_type(x, FLD_TYPE);
  mrc_fld_set_param_obj(x, "domain", tmpl->_domain);
  mrc_fld_set_param_int(x, "nr_spatial_dims", 3);
  mrc_fld_set_param_int(x, "nr_comps", nr_comps);
  mrc_fld_set_param_int(x, "nr_ghosts", tmpl->_nr_ghosts);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_setup

static void
ggcm_mhd_step_c_setup(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;

  assert(mhd);

  setup_mrc_fld_3d(sub->x_half, mhd->fld, 8);
  mrc_fld_dict_add_int(sub->x_half, "mhd_type", ggcm_mhd_step_mhd_type(step));
  setup_mrc_fld_3d(sub->prim, mhd->fld, _VZ + 1);

  sub->masks = mhd->fld;
  sub->bc    = mhd->fld;
  sub->flux  = mhd->fld;
  sub->tmp   = mhd->fld;

  ggcm_mhd_step_setup_member_objs_sub(step);
  ggcm_mhd_step_setup_super(step);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_primvar

static void
ggcm_mhd_step_c_primvar(struct ggcm_mhd_step *step, struct mrc_fld *prim,
			struct mrc_fld *x)
{
  mrc_fld_data_t gamm = step->mhd->par.gamm;
  mrc_fld_data_t s = gamm - 1.f;

  mrc_fld_foreach(x, ix,iy,iz, 2, 2) {
    F3(prim,_RR, ix,iy,iz) = RR1(x, ix,iy,iz);
    mrc_fld_data_t rri = 1.f / RR1(x, ix,iy,iz);
    F3(prim,_VX, ix,iy,iz) = rri * RV1X(x, ix,iy,iz);
    F3(prim,_VY, ix,iy,iz) = rri * RV1Y(x, ix,iy,iz);
    F3(prim,_VZ, ix,iy,iz) = rri * RV1Z(x, ix,iy,iz);
    mrc_fld_data_t rvv =
      F3(prim,_VX, ix,iy,iz) * RV1X(x, ix,iy,iz) +
      F3(prim,_VY, ix,iy,iz) * RV1Y(x, ix,iy,iz) +
      F3(prim,_VZ, ix,iy,iz) * RV1Z(x, ix,iy,iz);
    F3(prim,_PP, ix,iy,iz) = s * (UU1(x, ix,iy,iz) - .5f * rvv);
    mrc_fld_data_t cs2 = mrc_fld_max(gamm * F3(prim,_PP, ix,iy,iz) * rri, 0.f);
    F3(prim,_CMSV, ix,iy,iz) = sqrtf(rvv * rri) + sqrtf(cs2);
  } mrc_fld_foreach_end;
}

// ======================================================================

static void
rmaskn_c(struct ggcm_mhd_step *step)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *masks = sub->masks;

  mrc_fld_data_t diffco = mhd->par.diffco;
  mrc_fld_data_t diff_swbnd = mhd->par.diff_swbnd;
  int diff_obnd = mhd->par.diff_obnd;
  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(mhd->domain, 0, &info);

  float *fx1x = ggcm_mhd_crds_get_crd(mhd->crds, 0, FX1);

  mrc_fld_foreach(masks, ix,iy,iz, 2, 2) {
    F3(masks, _RMASK, ix,iy,iz) = 0.f;
    mrc_fld_data_t xxx = fx1x[ix];
    if (xxx < diff_swbnd)
      continue;
    if (iy + info.off[1] < diff_obnd)
      continue;
    if (iz + info.off[2] < diff_obnd)
      continue;
    if (ix + info.off[0] >= gdims[0] - diff_obnd)
      continue;
    if (iy + info.off[1] >= gdims[1] - diff_obnd)
      continue;
    if (iz + info.off[2] >= gdims[2] - diff_obnd)
      continue;
    F3(masks, _RMASK, ix,iy,iz) = diffco * F3(masks, _ZMASK, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
vgflrr_c(struct ggcm_mhd *mhd, struct mrc_fld *tmp, int m_tmp,
	 struct mrc_fld *prim)
{
  mrc_fld_foreach(tmp, ix,iy,iz, 2, 2) {
    mrc_fld_data_t a = F3(prim, _RR, ix,iy,iz);
    F3(tmp, m_tmp + 0, ix,iy,iz) = a * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 1, ix,iy,iz) = a * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 2, ix,iy,iz) = a * F3(prim, _VZ, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
vgflrvx_c(struct ggcm_mhd *mhd, struct mrc_fld *tmp, int m_tmp,
	 struct mrc_fld *prim)
{
  mrc_fld_foreach(tmp, ix,iy,iz, 2, 2) {
    mrc_fld_data_t a = F3(prim, _RR, ix,iy,iz) * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 0, ix,iy,iz) = a * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 1, ix,iy,iz) = a * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 2, ix,iy,iz) = a * F3(prim, _VZ, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
vgflrvy_c(struct ggcm_mhd *mhd, struct mrc_fld *tmp, int m_tmp,
	 struct mrc_fld *prim)
{
  mrc_fld_foreach(tmp, ix,iy,iz, 2, 2) {
    mrc_fld_data_t a = F3(prim, _RR, ix,iy,iz) * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 0, ix,iy,iz) = a * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 1, ix,iy,iz) = a * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 2, ix,iy,iz) = a * F3(prim, _VZ, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
vgflrvz_c(struct ggcm_mhd *mhd, struct mrc_fld *tmp, int m_tmp,
	 struct mrc_fld *prim)
{
  mrc_fld_foreach(tmp, ix,iy,iz, 2, 2) {
    mrc_fld_data_t a = F3(prim, _RR, ix,iy,iz) * F3(prim, _VZ, ix,iy,iz);
    F3(tmp, m_tmp + 0, ix,iy,iz) = a * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 1, ix,iy,iz) = a * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 2, ix,iy,iz) = a * F3(prim, _VZ, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
vgfluu_c(struct ggcm_mhd *mhd, struct mrc_fld *tmp, int m_tmp,
	 struct mrc_fld *prim)
{
  mrc_fld_data_t gamma = mhd->par.gamm;
  mrc_fld_data_t s = gamma / (gamma - 1.f);
  mrc_fld_foreach(tmp, ix,iy,iz, 2, 2) {
    mrc_fld_data_t ep = s * F3(prim, _PP, ix,iy,iz) +
      .5f * F3(prim, _RR, ix,iy,iz) * (sqr(F3(prim, _VX, ix,iy,iz)) + 
				       sqr(F3(prim, _VY, ix,iy,iz)) + 
				       sqr(F3(prim, _VZ, ix,iy,iz)));
    F3(tmp, m_tmp + 0, ix,iy,iz) = ep * F3(prim, _VX, ix,iy,iz);
    F3(tmp, m_tmp + 1, ix,iy,iz) = ep * F3(prim, _VY, ix,iy,iz);
    F3(tmp, m_tmp + 2, ix,iy,iz) = ep * F3(prim, _VZ, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static void
fluxl_c(struct ggcm_mhd *mhd, struct mrc_fld *flux, int m_flux,
	struct mrc_fld *tmp, int m_tmp, struct mrc_fld *x, int m,
	struct mrc_fld *prim)
{
  mrc_fld_foreach(flux, ix,iy,iz, 1, 0) {
    mrc_fld_data_t aa = F3(x, m, ix,iy,iz);
    mrc_fld_data_t cmsv = F3(prim, _CMSV, ix,iy,iz);
    F3(flux, m_flux + 0, ix,iy,iz) =
      .5f * ((F3(tmp, m_tmp + 0, ix  ,iy,iz) + F3(tmp, m_tmp + 0, ix+1,iy,iz)) -
	     .5f * (F3(prim, _CMSV, ix+1,iy,iz) + cmsv) * (F3(x, m, ix+1,iy,iz) - aa));
    F3(flux, m_flux + 1, ix,iy,iz) =
      .5f * ((F3(tmp, m_tmp + 1, ix,iy  ,iz) + F3(tmp, m_tmp + 1, ix,iy+1,iz)) -
	     .5f * (F3(prim, _CMSV, ix,iy+1,iz) + cmsv) * (F3(x, m, ix,iy+1,iz) - aa));
    F3(flux, m_flux + 2, ix,iy,iz) =
      .5f * ((F3(tmp, m_tmp + 2, ix,iy,iz  ) + F3(tmp, m_tmp + 2, ix,iy,iz+1)) -
	     .5f * (F3(prim, _CMSV, ix,iy,iz+1) + cmsv) * (F3(x, m, ix,iy,iz+1) - aa));
  } mrc_fld_foreach_end;
}

static void
fluxb_c(struct ggcm_mhd *mhd, struct mrc_fld *flux, int m_flux,
	struct mrc_fld *tmp, int m_tmp, struct mrc_fld *x, int m,
	struct mrc_fld *prim, struct mrc_fld *c)
{
  mrc_fld_data_t s1 = 1.f/12.f;
  mrc_fld_data_t s7 = 7.f * s1;

  mrc_fld_foreach(flux, ix,iy,iz, 1, 0) {
    mrc_fld_data_t fhx = (s7 * (F3(tmp, m_tmp + 0, ix  ,iy,iz) + F3(tmp, m_tmp + 0, ix+1,iy,iz)) -
			  s1 * (F3(tmp, m_tmp + 0, ix-1,iy,iz) + F3(tmp, m_tmp + 0, ix+2,iy,iz)));
    mrc_fld_data_t fhy = (s7 * (F3(tmp, m_tmp + 1, ix,iy  ,iz) + F3(tmp, m_tmp + 1, ix,iy+1,iz)) -
			  s1 * (F3(tmp, m_tmp + 1, ix,iy-1,iz) + F3(tmp, m_tmp + 1, ix,iy+2,iz)));
    mrc_fld_data_t fhz = (s7 * (F3(tmp, m_tmp + 2, ix,iy,iz  ) + F3(tmp, m_tmp + 2, ix,iy,iz+1)) -
			  s1 * (F3(tmp, m_tmp + 2, ix,iy,iz-1) + F3(tmp, m_tmp + 2, ix,iy,iz+2)));

    mrc_fld_data_t aa = F3(x, m, ix,iy,iz);
    mrc_fld_data_t cmsv = F3(prim, _CMSV, ix,iy,iz);
    mrc_fld_data_t flx =
      .5f * ((F3(tmp, m_tmp + 0, ix  ,iy,iz) + F3(tmp, m_tmp + 0, ix+1,iy,iz)) -
	     .5f * (F3(prim, _CMSV, ix+1,iy,iz) + cmsv) * (F3(x, m, ix+1,iy,iz) - aa));
    mrc_fld_data_t fly =
      .5f * ((F3(tmp, m_tmp + 1, ix,iy  ,iz) + F3(tmp, m_tmp + 1, ix,iy+1,iz)) -
	     .5f * (F3(prim, _CMSV, ix,iy+1,iz) + cmsv) * (F3(x, m, ix,iy+1,iz) - aa));
    mrc_fld_data_t flz = 
      .5f * ((F3(tmp, m_tmp + 2, ix,iy,iz  ) + F3(tmp, m_tmp + 2, ix,iy,iz+1)) -
	     .5f * (F3(prim, _CMSV, ix,iy,iz+1) + cmsv) * (F3(x, m, ix,iy,iz+1) - aa));

    mrc_fld_data_t cx = F3(c, _CX, ix,iy,iz);
    F3(flux, m_flux + 0, ix,iy,iz) = cx * flx + (1.f - cx) * fhx;
    mrc_fld_data_t cy = F3(c, _CY, ix,iy,iz);
    F3(flux, m_flux + 1, ix,iy,iz) = cy * fly + (1.f - cy) * fhy;
    mrc_fld_data_t cz = F3(c, _CZ, ix,iy,iz);
    F3(flux, m_flux + 2, ix,iy,iz) = cz * flz + (1.f - cz) * fhz;
  } mrc_fld_foreach_end;
}

static void
pushn_c(struct ggcm_mhd_step *step, struct mrc_fld *x, int m,
	struct mrc_fld *flux, int m_flux, mrc_fld_data_t dt)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *masks = sub->masks;
  float *fd1x = ggcm_mhd_crds_get_crd(mhd->crds, 0, FD1);
  float *fd1y = ggcm_mhd_crds_get_crd(mhd->crds, 1, FD1);
  float *fd1z = ggcm_mhd_crds_get_crd(mhd->crds, 2, FD1);

  mrc_fld_foreach(x, ix,iy,iz, 0, 0) {
    mrc_fld_data_t s = dt * F3(masks, _YMASK, ix,iy,iz);
    F3(x, m, ix,iy,iz) +=
      - s * (fd1x[ix] * (F3(flux, m_flux + 0, ix,iy,iz) - F3(flux, m_flux + 0, ix-1,iy,iz)) +
	     fd1y[iy] * (F3(flux, m_flux + 1, ix,iy,iz) - F3(flux, m_flux + 1, ix,iy-1,iz)) +
	     fd1z[iz] * (F3(flux, m_flux + 2, ix,iy,iz) - F3(flux, m_flux + 2, ix,iy,iz-1)));
  } mrc_fld_foreach_end;
}

static void
pushpp_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, struct mrc_fld *x, int m,
	 struct mrc_fld *prim)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *masks = sub->masks;
  float *fd1x = ggcm_mhd_crds_get_crd(mhd->crds, 0, FD1);
  float *fd1y = ggcm_mhd_crds_get_crd(mhd->crds, 1, FD1);
  float *fd1z = ggcm_mhd_crds_get_crd(mhd->crds, 2, FD1);

  mrc_fld_data_t dth = -.5f * dt;
  mrc_fld_foreach(x, ix,iy,iz, 0, 0) {
    mrc_fld_data_t fpx = fd1x[ix] * (F3(prim, _PP, ix+1,iy,iz) - F3(prim, _PP, ix-1,iy,iz));
    mrc_fld_data_t fpy = fd1y[iy] * (F3(prim, _PP, ix,iy+1,iz) - F3(prim, _PP, ix,iy-1,iz));
    mrc_fld_data_t fpz = fd1z[iz] * (F3(prim, _PP, ix,iy,iz+1) - F3(prim, _PP, ix,iy,iz-1));
    mrc_fld_data_t z = dth * F3(masks, _ZMASK, ix,iy,iz);
    F3(x, m + _RV1X, ix,iy,iz) += z * fpx;
    F3(x, m + _RV1Y, ix,iy,iz) += z * fpy;
    F3(x, m + _RV1Z, ix,iy,iz) += z * fpz;
  } mrc_fld_foreach_end;
}

static void
vgrs(struct mrc_fld *f, int m, mrc_fld_data_t s)
{
  mrc_fld_foreach(f, ix,iy,iz, 2, 2) {
    F3(f, m, ix,iy,iz) = s;
  } mrc_fld_foreach_end;
}

static void
vgrv(struct mrc_fld *f, int m_to, int m_from)
{
  mrc_fld_foreach(f, ix,iy,iz, 2, 2) {
    F3(f, m_to, ix,iy,iz) = F3(f, m_from, ix,iy,iz);
  } mrc_fld_foreach_end;
}

static inline void
limit1a(struct mrc_fld *x, int m, int ix, int iy, int iz, int IX, int IY, int IZ,
	struct mrc_fld *c, int m_c)
{
  const mrc_fld_data_t reps = 0.003;
  const mrc_fld_data_t seps = -0.001;
  const mrc_fld_data_t teps = 1.e-25;

  // Harten/Zwas type switch
  mrc_fld_data_t aa = F3(x, m, ix,iy,iz);
  mrc_fld_data_t a1 = F3(x, m, ix+IX,iy+IY,iz+IZ);
  mrc_fld_data_t a2 = F3(x, m, ix-IX,iy-IY,iz-IZ);
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
  F3(c, m_c, ix   ,iy   ,iz   ) = fmaxf(F3(c, m_c, ix   ,iy   ,iz   ), r3);
  F3(c, m_c, ix-IX,iy-IY,iz-IZ) = fmaxf(F3(c, m_c, ix-IX,iy-IY,iz-IZ), r3);
}

static void
limit1_c(struct mrc_fld *x, int m, mrc_fld_data_t time, mrc_fld_data_t timelo,
	 struct mrc_fld *bc, int m_c)
{
  if (time < timelo) {
    vgrs(bc, m_c + 0, 1.f);
    vgrs(bc, m_c + 1, 1.f);
    vgrs(bc, m_c + 2, 1.f);
    return;
  }

  mrc_fld_foreach(bc, ix,iy,iz, 1, 1) {
/* .if (limit_aspect_low) then */
/* .call lowmask(0,0,0,tl1) */
    limit1a(x, m, ix,iy,iz, 1,0,0, bc, m_c + 0);
    limit1a(x, m, ix,iy,iz, 0,1,0, bc, m_c + 1);
    limit1a(x, m, ix,iy,iz, 0,0,1, bc, m_c + 2);
  } mrc_fld_foreach_end;
}

static void
vgfl_c(struct ggcm_mhd *mhd, int m, struct mrc_fld *tmp, int m_tmp,
       struct mrc_fld *prim)
{
  switch (m) {
  case _RR1:  return vgflrr_c(mhd, tmp, m_tmp, prim);
  case _RV1X: return vgflrvx_c(mhd, tmp, m_tmp, prim);
  case _RV1Y: return vgflrvy_c(mhd, tmp, m_tmp, prim);
  case _RV1Z: return vgflrvz_c(mhd, tmp, m_tmp, prim);
  case _UU1:  return vgfluu_c(mhd, tmp, m_tmp, prim);
  default: assert(0);
  }
}

static void
pushfv_c(struct ggcm_mhd_step *step, int m, mrc_fld_data_t dt, struct mrc_fld *x_curr, int m_curr,
	 struct mrc_fld *x_next, int m_next,
	 int limit)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  int m_flux = _FLX, m_tmp = _TMP1;
  struct mrc_fld *prim = sub->prim, *bc = sub->bc, *flux = sub->flux, *tmp = sub->tmp;

  vgfl_c(mhd, m, tmp, m_tmp, prim);
  if (limit == LIMIT_NONE) {
    fluxl_c(mhd, flux, m_flux, tmp, m_tmp, x_curr, m_curr + m, prim);
  } else {
    vgrv(bc, _CX, _BX); vgrv(bc, _CY, _BY); vgrv(bc, _CY, _BY);
    limit1_c(x_curr, m_curr + m, mhd->time, mhd->par.timelo, bc, _CX);
    fluxb_c(mhd, flux, m_flux, tmp, m_tmp, x_curr, m_curr + m, prim, bc);
  }

  pushn_c(step, x_next, m_next + m, flux, m_flux, dt);
}

// ----------------------------------------------------------------------
// curr_c
//
// edge centered current density

static void
curr_c(struct ggcm_mhd *mhd, int m_j, int m_curr)
{
  struct mrc_fld *f = mhd->fld;
  float *bd4x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD4) - 1;
  float *bd4y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD4) - 1;
  float *bd4z = ggcm_mhd_crds_get_crd(mhd->crds, 2, BD4) - 1;

  mrc_fld_foreach(f, ix,iy,iz, 1, 2) {
    F3(f, m_j + 0, ix,iy,iz) =
      (F3(f, m_curr + _B1Z, ix,iy,iz) - F3(f, m_curr + _B1Z, ix,iy-1,iz)) * bd4y[iy] -
      (F3(f, m_curr + _B1Y, ix,iy,iz) - F3(f, m_curr + _B1Y, ix,iy,iz-1)) * bd4z[iz];
    F3(f, m_j + 1, ix,iy,iz) =
      (F3(f, m_curr + _B1X, ix,iy,iz) - F3(f, m_curr + _B1X, ix,iy,iz-1)) * bd4z[iz] -
      (F3(f, m_curr + _B1Z, ix,iy,iz) - F3(f, m_curr + _B1Z, ix-1,iy,iz)) * bd4x[ix];
    F3(f, m_j + 2, ix,iy,iz) =
      (F3(f, m_curr + _B1Y, ix,iy,iz) - F3(f, m_curr + _B1Y, ix-1,iy,iz)) * bd4x[ix] -
      (F3(f, m_curr + _B1X, ix,iy,iz) - F3(f, m_curr + _B1X, ix,iy-1,iz)) * bd4y[iy];
  } mrc_fld_foreach_end;
}

// ----------------------------------------------------------------------
// currbb_c
//
// cell-averaged B

static void
currbb_c(struct ggcm_mhd *mhd, int m, int m_curr)
{
  struct mrc_fld *f = mhd->fld;

  mrc_fld_foreach(f, ix,iy,iz, 1, 1) {
    F3(f, m + 0, ix,iy,iz) = .5f * (F3(f, m_curr + _B1X, ix  ,iy,iz) +
				    F3(f, m_curr + _B1X, ix+1,iy,iz));
    F3(f, m + 1, ix,iy,iz) = .5f * (F3(f, m_curr + _B1Y, ix,iy  ,iz) +
				    F3(f, m_curr + _B1Y, ix,iy+1,iz));
    F3(f, m + 2, ix,iy,iz) = .5f * (F3(f, m_curr + _B1Z, ix,iy,iz  ) +
				    F3(f, m_curr + _B1Z, ix,iy,iz+1));
  } mrc_fld_foreach_end;
}

// ----------------------------------------------------------------------
// curbc_c
//
// cell-centered j

static void
curbc_c(struct ggcm_mhd *mhd, int m_curr)
{ 
  enum { _TX = _TMP1, _TY = _TMP2, _TZ = _TMP3 };

  curr_c(mhd, _TX, m_curr);

  struct mrc_fld *f = mhd->fld;

  // j averaged to cell-centered
  mrc_fld_foreach(f, ix,iy,iz, 1, 1) {
    mrc_fld_data_t s = .25f * F3(f, _ZMASK, ix, iy, iz);
    F3(f, _CURRX, ix,iy,iz) = s * (F3(f, _TX, ix,iy+1,iz+1) + F3(f, _TX, ix,iy,iz+1) +
				   F3(f, _TX, ix,iy+1,iz  ) + F3(f, _TX, ix,iy,iz  ));
    F3(f, _CURRY, ix,iy,iz) = s * (F3(f, _TY, ix+1,iy,iz+1) + F3(f, _TY, ix,iy,iz+1) +
				   F3(f, _TY, ix+1,iy,iz  ) + F3(f, _TY, ix,iy,iz  ));
    F3(f, _CURRZ, ix,iy,iz) = s * (F3(f, _TZ, ix+1,iy+1,iz) + F3(f, _TZ, ix,iy+1,iz) +
				   F3(f, _TZ, ix+1,iy  ,iz) + F3(f, _TZ, ix,iy  ,iz));
  } mrc_fld_foreach_end;
}

static void
push_ej_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, int m_curr, int m_next)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *prim = sub->prim;

  enum { XJX = _BX, XJY = _BY, XJZ = _BZ };
  enum { BX = _TMP1, BY = _TMP2, BZ = _TMP3 };

  curr_c(mhd, XJX, m_curr);
  currbb_c(mhd, BX, m_curr);
	
  struct mrc_fld *f = mhd->fld;

  mrc_fld_data_t s1 = .25f * dt;
  mrc_fld_foreach(f, ix,iy,iz, 0, 0) {
    mrc_fld_data_t z = F3(f,_ZMASK, ix,iy,iz);
    mrc_fld_data_t s2 = s1 * z;
    mrc_fld_data_t cx = (F3(f, XJX, ix  ,iy+1,iz+1) +
		F3(f, XJX, ix  ,iy  ,iz+1) +
		F3(f, XJX, ix  ,iy+1,iz  ) +
		F3(f, XJX, ix  ,iy  ,iz  ));
    mrc_fld_data_t cy = (F3(f, XJY, ix+1,iy  ,iz+1) +
		F3(f, XJY, ix  ,iy  ,iz+1) +
		F3(f, XJY, ix+1,iy  ,iz  ) +
		F3(f, XJY, ix  ,iy  ,iz  ));
    mrc_fld_data_t cz = (F3(f, XJZ, ix+1,iy+1,iz  ) +
		F3(f, XJZ, ix  ,iy+1,iz  ) +
		F3(f, XJZ, ix+1,iy  ,iz  ) +
		F3(f, XJZ, ix  ,iy  ,iz  ));
    mrc_fld_data_t ffx = s2 * (cy * F3(f, BZ, ix,iy,iz) -
			       cz * F3(f, BY, ix,iy,iz));
    mrc_fld_data_t ffy = s2 * (cz * F3(f, BX, ix,iy,iz) -
			       cx * F3(f, BZ, ix,iy,iz));
    mrc_fld_data_t ffz = s2 * (cx * F3(f, BY, ix,iy,iz) -
			       cy * F3(f, BX, ix,iy,iz));
    mrc_fld_data_t duu = (ffx * F3(prim, _VX, ix,iy,iz) +
			  ffy * F3(prim, _VY, ix,iy,iz) +
			  ffz * F3(prim, _VZ, ix,iy,iz));

    F3(f, m_next + _RV1X, ix,iy,iz) += ffx;
    F3(f, m_next + _RV1Y, ix,iy,iz) += ffy;
    F3(f, m_next + _RV1Z, ix,iy,iz) += ffz;
    F3(f, m_next + _UU1 , ix,iy,iz) += duu;
  } mrc_fld_foreach_end;
}

static void
res1_const_c(struct ggcm_mhd *mhd)
{
  // resistivity comes in ohm*m
  int diff_obnd = mhd->par.diff_obnd;
  mrc_fld_data_t eta0i = 1.0/53.5848e6;
  mrc_fld_data_t diffsphere2 = sqr(mhd->par.diffsphere);
  mrc_fld_data_t diff = mhd->par.diffco * eta0i;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  struct mrc_patch_info info;
  mrc_domain_get_local_patch_info(mhd->domain, 0, &info);

  struct mrc_fld *f = mhd->fld;
  float *fx2x = ggcm_mhd_crds_get_crd(mhd->crds, 0, FX2);
  float *fx2y = ggcm_mhd_crds_get_crd(mhd->crds, 1, FX2);
  float *fx2z = ggcm_mhd_crds_get_crd(mhd->crds, 2, FX2);

  mrc_fld_foreach(f, ix,iy,iz, 1, 1) {
    F3(f, _RESIS, ix,iy,iz) = 0.f;
    mrc_fld_data_t r2 = fx2x[ix] + fx2y[iy] + fx2z[iz];
    if (r2 < diffsphere2)
      continue;
    if (iy + info.off[1] < diff_obnd)
      continue;
    if (iz + info.off[2] < diff_obnd)
      continue;
    if (ix + info.off[0] >= gdims[0] - diff_obnd)
      continue;
    if (iy + info.off[1] >= gdims[1] - diff_obnd)
      continue;
    if (iz + info.off[2] >= gdims[2] - diff_obnd)
      continue;

    F3(f, _RESIS, ix,iy,iz) = diff;
  } mrc_fld_foreach_end;
}

static void
calc_resis_const_c(struct ggcm_mhd *mhd, int m_curr)
{
  curbc_c(mhd, m_curr);
  res1_const_c(mhd);
}

static void
calc_resis_nl1_c(struct ggcm_mhd *mhd, int m_curr)
{
  // used to zero _RESIS field, but that's not needed.
}

static inline float
bcthy3f(mrc_fld_data_t s1, mrc_fld_data_t s2)
{
  if (s1 > 0.f && fabsf(s2) > REPS) {
/* .if(calce_aspect_low) then */
/* .call lowmask(IX, 0, 0,tl1) */
/* .call lowmask( 0,IY, 0,tl2) */
/* .call lowmask( 0, 0,IZ,tl3) */
/* .call lowmask(IX,IY,IZ,tl4) */
/*       tt=tt*(1.0-max(tl1,tl2,tl3,tl4)) */
    return s1 / s2;
  }
  return 0.f;
}

static inline void
calc_avg_dz_By(struct ggcm_mhd *mhd, struct mrc_fld *f, int m_curr, int XX, int YY, int ZZ,
	       int JX1, int JY1, int JZ1, int JX2, int JY2, int JZ2)
{
  float *bd1x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD1);
  float *bd1y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD1);
  float *bd1z = ggcm_mhd_crds_get_crd(mhd->crds, 2, BD1);

  // d_z B_y, d_y B_z on x edges
  mrc_fld_foreach(f, ix,iy,iz, 1, 2) {
    mrc_fld_data_t bd1[3] = { bd1x[ix-1], bd1y[iy-1], bd1z[iz-1] };

    F3(f, _TMP1, ix,iy,iz) = bd1[ZZ] * 
      (F3(f, m_curr + _B1X + YY, ix,iy,iz) - F3(f, m_curr + _B1X + YY, ix-JX2,iy-JY2,iz-JZ2));
    F3(f, _TMP2, ix,iy,iz) = bd1[YY] * 
      (F3(f, m_curr + _B1X + ZZ, ix,iy,iz) - F3(f, m_curr + _B1X + ZZ, ix-JX1,iy-JY1,iz-JZ1));
  } mrc_fld_foreach_end;

  // .5 * harmonic average if same sign
  mrc_fld_foreach(f, ix,iy,iz, 1, 1) {
    mrc_fld_data_t s1, s2;
    // dz_By on y face
    s1 = F3(f, _TMP1, ix+JX2,iy+JY2,iz+JZ2) * F3(f, _TMP1, ix,iy,iz);
    s2 = F3(f, _TMP1, ix+JX2,iy+JY2,iz+JZ2) + F3(f, _TMP1, ix,iy,iz);
    F3(f, _TMP3, ix,iy,iz) = bcthy3f(s1, s2);
    // dy_Bz on z face
    s1 = F3(f, _TMP2, ix+JX1,iy+JY1,iz+JZ1) * F3(f, _TMP2, ix,iy,iz);
    s2 = F3(f, _TMP2, ix+JX1,iy+JY1,iz+JZ1) + F3(f, _TMP2, ix,iy,iz);
    F3(f, _TMP4, ix,iy,iz) = bcthy3f(s1, s2);
  } mrc_fld_foreach_end;
}

#define CC_TO_EC(f, m, ix,iy,iz, IX,IY,IZ) \
  (.25f * (F3(f, m, ix-IX,iy-IY,iz-IZ) +  \
	   F3(f, m, ix-IX,iy   ,iz   ) +  \
	   F3(f, m, ix   ,iy-IY,iz   ) +  \
	   F3(f, m, ix   ,iy   ,iz-IZ)))

static inline void
calc_v_x_B(mrc_fld_data_t ttmp[2], struct mrc_fld *f, int m_curr, struct mrc_fld *prim,
	   int ix, int iy, int iz,
	   int XX, int YY, int ZZ, int IX, int IY, int IZ,
	   int JX1, int JY1, int JZ1, int JX2, int JY2, int JZ2,
	   float *bd2x, float *bd2y, float *bd2z, mrc_fld_data_t dt)
{
    mrc_fld_data_t bd2m[3] = { bd2x[ix-1], bd2y[iy-1], bd2z[iz-1] };
    mrc_fld_data_t bd2[3] = { bd2x[ix], bd2y[iy], bd2z[iz] };
    mrc_fld_data_t vbZZ;
    // edge centered velocity
    mrc_fld_data_t vvYY = CC_TO_EC(prim, _VX + YY, ix,iy,iz, IX,IY,IZ) /* - d_i * vcurrYY */;
    if (vvYY > 0.f) {
      vbZZ = F3(f, m_curr + _B1X + ZZ, ix-JX1,iy-JY1,iz-JZ1) +
	F3(f, _TMP4, ix-JX1,iy-JY1,iz-JZ1) * (bd2m[YY] - dt*vvYY);
    } else {
      vbZZ = F3(f, m_curr + _B1X + ZZ, ix,iy,iz) -
	F3(f, _TMP4, ix,iy,iz) * (bd2[YY] + dt*vvYY);
    }
    ttmp[0] = vbZZ * vvYY;

    mrc_fld_data_t vbYY;
    // edge centered velocity
    mrc_fld_data_t vvZZ = CC_TO_EC(prim, _VX + ZZ, ix,iy,iz, IX,IY,IZ) /* - d_i * vcurrZZ */;
    if (vvZZ > 0.f) {
      vbYY = F3(f, m_curr + _B1X + YY, ix-JX2,iy-JY2,iz-JZ2) +
	F3(f, _TMP3, ix-JX2,iy-JY2,iz-JZ2) * (bd2m[ZZ] - dt*vvZZ);
    } else {
      vbYY = F3(f, m_curr + _B1X + YY, ix,iy,iz) -
	F3(f, _TMP3, ix,iy,iz) * (bd2[ZZ] + dt*vvZZ);
    }
    ttmp[1] = vbYY * vvZZ;

}

static void
bcthy3z_NL1(struct ggcm_mhd_step *step, int XX, int YY, int ZZ, int IX, int IY, int IZ,
	    int JX1, int JY1, int JZ1, int JX2, int JY2, int JZ2,
	    mrc_fld_data_t dt, int m_curr)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *prim = sub->prim;
  struct mrc_fld *f = mhd->fld;

  float *bd2x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD2);
  float *bd2y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD2);
  float *bd2z = ggcm_mhd_crds_get_crd(mhd->crds, 2, BD2);

  calc_avg_dz_By(mhd, f, m_curr, XX, YY, ZZ, JX1, JY1, JZ1, JX2, JY2, JZ2);

  mrc_fld_data_t diffmul=1.0;
  if (mhd->time < mhd->par.diff_timelo) { // no anomalous res at startup
    diffmul = 0.f;
  }

  // edge centered E = - v x B (+ dissipation)
  mrc_fld_foreach(f, ix,iy,iz, 1, 0) {
    mrc_fld_data_t ttmp[2];
    calc_v_x_B(ttmp, f, m_curr, prim, ix, iy, iz, XX, YY, ZZ, IX, IY, IZ,
	       JX1, JY1, JZ1, JX2, JY2, JZ2, bd2x, bd2y, bd2z, dt);
    
    mrc_fld_data_t t1m = F3(f, m_curr + _B1X + ZZ, ix+JX1,iy+JY1,iz+JZ1) - F3(f, m_curr + _B1X + ZZ, ix,iy,iz);
    mrc_fld_data_t t1p = fabsf(F3(f, m_curr + _B1X + ZZ, ix+JX1,iy+JY1,iz+JZ1)) + fabsf(F3(f, m_curr + _B1X + ZZ, ix,iy,iz));
    mrc_fld_data_t t2m = F3(f, m_curr + _B1X + YY, ix+JX2,iy+JY2,iz+JZ2) - F3(f, m_curr + _B1X + YY, ix,iy,iz);
    mrc_fld_data_t t2p = fabsf(F3(f, m_curr + _B1X + YY, ix+JX2,iy+JY2,iz+JZ2)) + fabsf(F3(f, m_curr + _B1X + YY, ix,iy,iz));
    mrc_fld_data_t tp = t1p + t2p + REPS;
    mrc_fld_data_t tpi = diffmul / tp;
    mrc_fld_data_t d1 = sqr(t1m * tpi);
    mrc_fld_data_t d2 = sqr(t2m * tpi);
    if (d1 < mhd->par.diffth) d1 = 0.;
    if (d2 < mhd->par.diffth) d2 = 0.;
    ttmp[0] -= d1 * t1m * F3(f, _RMASK, ix,iy,iz);
    ttmp[1] -= d2 * t2m * F3(f, _RMASK, ix,iy,iz);
    F3(f, _RESIS, ix,iy,iz) += fabsf(d1+d2) * F3(f, _ZMASK, ix,iy,iz);
    F3(f, _FLX + XX, ix,iy,iz) = ttmp[0] - ttmp[1];
  } mrc_fld_foreach_end;
}

static void
bcthy3z_const(struct ggcm_mhd_step *step, int XX, int YY, int ZZ, int IX, int IY, int IZ,
	      int JX1, int JY1, int JZ1, int JX2, int JY2, int JZ2, mrc_fld_data_t dt, int m_curr)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *prim = sub->prim;
  struct mrc_fld *f = mhd->fld;

  float *bd2x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD2);
  float *bd2y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD2);
  float *bd2z = ggcm_mhd_crds_get_crd(mhd->crds, 2, BD2);

  calc_avg_dz_By(mhd, f, m_curr, XX, YY, ZZ, JX1, JY1, JZ1, JX2, JY2, JZ2);

  // edge centered E = - v x B (+ dissipation)
  mrc_fld_foreach(f, ix,iy,iz, 0, 1) {
    mrc_fld_data_t ttmp[2];
    calc_v_x_B(ttmp, f, m_curr, prim, ix, iy, iz, XX, YY, ZZ, IX, IY, IZ,
	       JX1, JY1, JZ1, JX2, JY2, JZ2, bd2x, bd2y, bd2z, dt);

    mrc_fld_data_t vcurrXX = CC_TO_EC(f, _CURRX + XX, ix,iy,iz, IX,IY,IZ);
    mrc_fld_data_t vresis = CC_TO_EC(f, _RESIS, ix,iy,iz, IX,IY,IZ);
    F3(f, _FLX + XX, ix,iy,iz) = ttmp[0] - ttmp[1] - vresis * vcurrXX;
  } mrc_fld_foreach_end;
}

static void
calce_nl1_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, int m_curr)
{
  bcthy3z_NL1(step, 0,1,2, 0,1,1, 0,1,0, 0,0,1, dt, m_curr);
  bcthy3z_NL1(step, 1,2,0, 1,0,1, 0,0,1, 1,0,0, dt, m_curr);
  bcthy3z_NL1(step, 2,0,1, 1,1,0, 1,0,0, 0,1,0, dt, m_curr);
}

static void
calce_const_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, int m_curr)
{
  bcthy3z_const(step, 0,1,2, 0,1,1, 0,1,0, 0,0,1, dt, m_curr);
  bcthy3z_const(step, 1,2,0, 1,0,1, 0,0,1, 1,0,0, dt, m_curr);
  bcthy3z_const(step, 2,0,1, 1,1,0, 1,0,0, 0,1,0, dt, m_curr);
}

static void
calce_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt, int m_curr)
{
  struct ggcm_mhd *mhd = step->mhd;

  switch (mhd->par.magdiffu) {
  case MAGDIFFU_NL1:
    return calce_nl1_c(step, dt, m_curr);
  case MAGDIFFU_CONST:
    return calce_const_c(step, dt, m_curr);
  default:
    assert(0);
  }
}

static void
bpush_c(struct ggcm_mhd *mhd, mrc_fld_data_t dt, int m_next)
{
  struct mrc_fld *f = mhd->fld;
  float *bd3x = ggcm_mhd_crds_get_crd(mhd->crds, 0, BD3);
  float *bd3y = ggcm_mhd_crds_get_crd(mhd->crds, 1, BD3);
  float *bd3z = ggcm_mhd_crds_get_crd(mhd->crds, 2, BD3);

  mrc_fld_foreach(f, ix,iy,iz, 0, 0) {
    F3(f, m_next + _B1X, ix,iy,iz) +=
      dt * (bd3y[iy] * (F3(f,_FLZ, ix,iy+1,iz) - F3(f,_FLZ, ix,iy,iz)) -
	    bd3z[iz] * (F3(f,_FLY, ix,iy,iz+1) - F3(f,_FLY, ix,iy,iz)));
    F3(f, m_next + _B1Y, ix,iy,iz) +=
      dt * (bd3z[iz] * (F3(f,_FLX, ix,iy,iz+1) - F3(f,_FLX, ix,iy,iz)) -
	    bd3x[ix] * (F3(f,_FLZ, ix+1,iy,iz) - F3(f,_FLZ, ix,iy,iz)));
    F3(f, m_next + _B1Z, ix,iy,iz) +=
      dt * (bd3x[ix] * (F3(f,_FLY, ix+1,iy,iz) - F3(f,_FLY, ix,iy,iz)) -
	    bd3y[iy] * (F3(f,_FLX, ix,iy+1,iz) - F3(f,_FLX, ix,iy,iz)));
  } mrc_fld_foreach_end;
}

static void
pushstage_c(struct ggcm_mhd_step *step, mrc_fld_data_t dt,
	    struct mrc_fld *x_curr, int m_curr,
	    struct mrc_fld *x_next, int m_next,
	    struct mrc_fld *prim,
	    int limit)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct ggcm_mhd *mhd = step->mhd;
  rmaskn_c(step);

  if (limit != LIMIT_NONE) {
    struct mrc_fld *prim = sub->prim, *bc = sub->bc;

    vgrs(bc, _BX, 0.f); vgrs(bc, _BY, 0.f); vgrs(bc, _BZ, 0.f);
    limit1_c(prim, _PP, mhd->time, mhd->par.timelo, bc, _BX);
    // limit2, 3
  }

  pushfv_c(step, _RR1 , dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1X, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1Y, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1Z, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _UU1 , dt, x_curr, m_curr, x_next, m_next, limit);

  pushpp_c(step, dt, x_next, m_next, prim);

  switch (mhd->par.magdiffu) {
  case MAGDIFFU_NL1:
    calc_resis_nl1_c(mhd, m_curr);
    break;
  case MAGDIFFU_CONST:
    calc_resis_const_c(mhd, m_curr);
    break;
  default:
    assert(0);
  }

  push_ej_c(step, dt, m_curr, m_next);
  calce_c(step, dt, m_curr);
  bpush_c(mhd, dt, m_next);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_pred

static void
ggcm_mhd_step_c_pred(struct ggcm_mhd_step *step,
		     struct mrc_fld *x_half, struct mrc_fld *x)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct mrc_fld *prim = sub->prim;

  ggcm_mhd_step_c_primvar(step, prim, x);
  primbb_c2_c(step->mhd, _RR1);
  zmaskn_c(step->mhd);

  mrc_fld_data_t dt = .5f * step->mhd->dt;

  // set x_half = x^n, then advance to n+1/2
  mrc_fld_copy_range(x_half, x, 0, 8);

  mrc_fld_foreach(x, ix,iy,iz, 2, 2) {
    for (int m = 0; m < 8; m++) {
      F3(x, _RR2 + m, ix,iy,iz) = F3(x_half, m, ix,iy,iz);
    }
  } mrc_fld_foreach_end;

#if 0
  pushstage_c(step, dt, x, _RR1, x, _RR2, LIMIT_NONE);
#else
  int limit = LIMIT_NONE;
  struct ggcm_mhd *mhd = step->mhd;
  struct mrc_fld *x_curr = x, *x_next = x;
  int m_curr = _RR1, m_next = _RR2;
  rmaskn_c(step);

  if (limit != LIMIT_NONE) {
    struct mrc_fld *prim = sub->prim, *bc = sub->bc;

    vgrs(bc, _BX, 0.f); vgrs(bc, _BY, 0.f); vgrs(bc, _BZ, 0.f);
    limit1_c(prim, _PP, mhd->time, mhd->par.timelo, bc, _BX);
    // limit2, 3
  }

  pushfv_c(step, _RR1 , dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1X, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1Y, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _RV1Z, dt, x_curr, m_curr, x_next, m_next, limit);
  pushfv_c(step, _UU1 , dt, x_curr, m_curr, x_next, m_next, limit);

  pushpp_c(step, dt, x_next, m_next, prim);

  switch (mhd->par.magdiffu) {
  case MAGDIFFU_NL1:
    calc_resis_nl1_c(mhd, m_curr);
    break;
  case MAGDIFFU_CONST:
    calc_resis_const_c(mhd, m_curr);
    break;
  default:
    assert(0);
  }

  push_ej_c(step, dt, m_curr, m_next);
  calce_c(step, dt, m_curr);
  bpush_c(mhd, dt, m_next);
#endif

  mrc_fld_foreach(x_half, ix,iy,iz, 2, 2) {
    for (int m = 0; m < 8; m++) {
      F3(x_half, m, ix,iy,iz) = F3(x, m + 8, ix,iy,iz);
    }
  } mrc_fld_foreach_end;
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_corr

static void
ggcm_mhd_step_c_corr(struct ggcm_mhd_step *step,
		     struct mrc_fld *x, struct mrc_fld *x_half)
{
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct mrc_fld *prim = sub->prim;

  ggcm_mhd_step_c_primvar(step, prim, x_half);

  mrc_fld_foreach(x_half, ix,iy,iz, 2, 2) {
    for (int m = 0; m < 8; m++) {
      F3(x, m + 8, ix,iy,iz) = F3(x_half, m, ix,iy,iz);
    }
  } mrc_fld_foreach_end;

  //  primbb_c2_c(step->mhd, _RR2);
  //  zmaskn_c(step->mhd);

  pushstage_c(step, step->mhd->dt, x, _RR2, x, _RR1, prim, LIMIT_1);
}

// ----------------------------------------------------------------------
// ggcm_mhd_step_c_run

static void
ggcm_mhd_step_c_run(struct ggcm_mhd_step *step, struct mrc_fld *x)
{
  struct ggcm_mhd *mhd = step->mhd;
  struct ggcm_mhd_step_c3 *sub = ggcm_mhd_step_c3(step);
  struct mrc_fld *x_half = sub->x_half;

  float dtn;
  if (step->do_nwst) {
    newstep(mhd, &dtn);
  }

  ggcm_mhd_fill_ghosts(mhd, x, _RR1, mhd->time);
  ggcm_mhd_step_c_pred(step, x_half, x);

  ggcm_mhd_fill_ghosts(mhd, x_half, 0, mhd->time + mhd->bndt);
  ggcm_mhd_step_c_corr(step, x, x_half);

  if (step->do_nwst) {
    dtn = fminf(1., dtn); // FIXME, only kept for compatibility

    if (dtn > 1.02 * mhd->dt || dtn < mhd->dt / 1.01) {
      mpi_printf(ggcm_mhd_comm(mhd), "switched dt %g <- %g\n",
		 dtn, mhd->dt);
      mhd->dt = dtn;
    }
  }
}

// ----------------------------------------------------------------------
// subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_step_c3, x)
static struct param ggcm_mhd_step_c_descr[] = {
  { "x_half"          , VAR(x_half)          , MRC_VAR_OBJ(mrc_fld)           },
  { "prim"            , VAR(prim)            , MRC_VAR_OBJ(mrc_fld)           },

  {},
};
#undef VAR

