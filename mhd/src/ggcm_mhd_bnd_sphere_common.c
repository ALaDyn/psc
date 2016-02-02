
#include "ggcm_mhd_bnd_private.h"

#include <ggcm_mhd_private.h>
#include <ggcm_mhd_defs.h>

#include <mrc_domain.h>
#include <mrc_bits.h>
#include <math.h>

enum {
  FIXED_RR,
  FIXED_PP,
  FIXED_VX,
  FIXED_VY,
  FIXED_VZ,
  FIXED_BX,
  FIXED_BY,
  FIXED_BZ,
  FIXED_NR,
};


// FIXME, consolidate with ggcm_mhd_iono

// ======================================================================
// ggcm_mhd_bnd subclass "sphere"

struct ggcm_mhd_bnd_sphere {
  // params
  double radius;

  // state
  struct ggcm_mhd_bnd_sphere_map map;

  // constant values to set
  double bnvals[FIXED_NR];
};

#define ggcm_mhd_bnd_sphere(bnd) mrc_to_subobj(bnd, struct ggcm_mhd_bnd_sphere)

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_map_cc

static void
ggcm_mhd_bnd_map_cc(struct ggcm_mhd_bnd *bnd)
{
  struct ggcm_mhd_bnd_sphere *sub = ggcm_mhd_bnd_sphere(bnd);
  struct ggcm_mhd_bnd_sphere_map *map = &sub->map;

  struct ggcm_mhd *mhd = bnd->mhd;
  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);

  double r1 = map->r1, r2 = map->r2;

  // compute e-field mapping coefficients

  int cc_n_map = 0;
  for (int p = 0; p < mrc_fld_nr_patches(mhd->fld); p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(mhd->domain, p, &info);
    int gdims[3];
    mrc_domain_get_global_dims(mhd->domain, gdims);
    // cell-centered
    int sw[3] = { 2, 2, 2 };
    for (int d = 0; d < 3; d++) {
      if (gdims[d] == 1) {
	sw[d] = 0;
      }
    }
    for (int jz = -sw[2]; jz < info.ldims[2] + sw[2]; jz++) {
      for (int jy = -sw[1]; jy < info.ldims[1] + sw[1]; jy++) {
	for (int jx = -sw[0]; jx < info.ldims[0] + sw[0]; jx++) {
	  double xx = MRC_MCRDX(crds, jx, p);
	  double yy = MRC_MCRDY(crds, jy, p);
	  double zz = MRC_MCRDZ(crds, jz, p);
	  double rr = sqrtf(sqr(xx) + sqr(yy) + sqr(zz));
	  if (rr < r1 || rr > r2) continue;
	  
	  MRC_I2(map->cc_imap, 0, cc_n_map) = jx;
	  MRC_I2(map->cc_imap, 1, cc_n_map) = jy;
	  MRC_I2(map->cc_imap, 2, cc_n_map) = jz;
	  MRC_I2(map->cc_imap, 3, cc_n_map) = p;

	  cc_n_map++;
	}
      }
    }
  }

  assert(cc_n_map == map->cc_n_map);
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_setup_flds

static void
ggcm_mhd_bnd_sphere_setup_flds(struct ggcm_mhd_bnd *bnd)
{
  struct ggcm_mhd_bnd_sphere *sub = ggcm_mhd_bnd_sphere(bnd);
  struct ggcm_mhd_bnd_sphere_map *map = &sub->map;

  ggcm_mhd_bnd_sphere_map_find_cc_n_map(map);
  mprintf("cc_n_map %d\n", map->cc_n_map);

  // cell-centered

  mrc_fld_set_type(map->cc_imap, "int");
  mrc_fld_set_param_int_array(map->cc_imap, "dims", 2, (int[2]) { 4, map->cc_n_map });
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_setup

static void
ggcm_mhd_bnd_sphere_setup(struct ggcm_mhd_bnd *bnd)
{
  struct ggcm_mhd_bnd_sphere *sub = ggcm_mhd_bnd_sphere(bnd);
  struct ggcm_mhd_bnd_sphere_map *map = &sub->map;

  ggcm_mhd_bnd_sphere_map_setup(map, bnd->mhd, sub->radius);
  ggcm_mhd_bnd_sphere_setup_flds(bnd);
  ggcm_mhd_bnd_setup_member_objs_sub(bnd);
  ggcm_mhd_bnd_map_cc(bnd);
}

// ----------------------------------------------------------------------
// sphere_fill_ghosts_mhd_do

static void
sphere_fill_ghosts_mhd_do(struct mrc_fld *fld,
    int cc_n_map, struct mrc_fld*cc_mhd_imap,
    double bnvals[FIXED_NR], int m, float bntim, float gamm)
{
  double rvx = bnvals[FIXED_RR] * bnvals[FIXED_VX];
  double rvy = bnvals[FIXED_RR] * bnvals[FIXED_VY];
  double rvz = bnvals[FIXED_RR] * bnvals[FIXED_VZ];

  double vvbn  = sqr(bnvals[FIXED_VX]) + sqr(bnvals[FIXED_VY]) + sqr(bnvals[FIXED_VZ]);
  double uubn  = .5f * (bnvals[FIXED_RR]*vvbn) + bnvals[FIXED_PP] / (gamm - 1.f);
  double b2bn  = sqr(bnvals[FIXED_BX]) + sqr(bnvals[FIXED_BY]) + sqr(bnvals[FIXED_BZ]);
  double eebn = uubn + .5 * b2bn;

  for (int i = 0; i < cc_n_map; i++) {
    int ix = MRC_I2(cc_mhd_imap, 0, i);
    int iy = MRC_I2(cc_mhd_imap, 1, i);
    int iz = MRC_I2(cc_mhd_imap, 2, i);
    int p  = MRC_I2(cc_mhd_imap, 3, i);

    M3 (fld, m + RR,  ix,iy,iz, p) = bnvals[FIXED_RR];
    M3 (fld, m + RVX, ix,iy,iz, p) = rvx;
    M3 (fld, m + RVY, ix,iy,iz, p) = rvy;
    M3 (fld, m + RVZ, ix,iy,iz, p) = rvz;
    if (MT == MT_SEMI_CONSERVATIVE ||
        MT == MT_SEMI_CONSERVATIVE_GGCM) {
      M3(fld, m + UU , ix,iy,iz, p) = uubn;
    } else if (MT == MT_FULLY_CONSERVATIVE) {
      M3(fld, m + EE , ix,iy,iz, p) = eebn;
    } else {
      assert(0);
    }
    M3(fld, m + BX , ix,iy,iz, p) = bnvals[FIXED_BX];
    M3(fld, m + BY , ix,iy,iz, p) = bnvals[FIXED_BY];
    M3(fld, m + BZ , ix,iy,iz, p) = bnvals[FIXED_BZ];
  }
}


// ----------------------------------------------------------------------
// ggcm_mhd_bnd_sphere_fill_ghosts

static void
ggcm_mhd_bnd_sphere_fill_ghosts(struct ggcm_mhd_bnd *bnd, struct mrc_fld *fld_base,
			      int m, float bntim)
{
  struct ggcm_mhd_bnd_sphere *sub = ggcm_mhd_bnd_sphere(bnd);
  struct ggcm_mhd_bnd_sphere_map *map = &sub->map;
  struct ggcm_mhd *mhd = bnd->mhd;

  if (map->cc_n_map == 0) {
    return;
  }

  int mhd_type;
  mrc_fld_get_param_int(fld_base, "mhd_type", &mhd_type);
  assert(mhd_type == MT);
  assert(m == 0 || m == 8);

  struct mrc_fld *fld = mrc_fld_get_as(fld_base, FLD_TYPE);

  sphere_fill_ghosts_mhd_do(fld, map->cc_n_map, map->cc_imap,
      sub->bnvals, m, bntim, mhd->par.gamm);

  mrc_fld_put_as(fld, fld_base);
}

// ----------------------------------------------------------------------
// ggcm_mhd_bnd "sphere" subclass description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_bnd_sphere, x)
static struct param ggcm_mhd_bnd_sphere_descr[] = {
  { "radius"          , VAR(radius)          , PARAM_DOUBLE(1.)          },

  { "min_dr"          , VAR(map.min_dr)      , MRC_VAR_DOUBLE            },
  { "r1"              , VAR(map.r1)          , MRC_VAR_DOUBLE            },
  { "r2"              , VAR(map.r2)          , MRC_VAR_DOUBLE            },
  { "cc_n_map"        , VAR(map.cc_n_map)    , MRC_VAR_INT               },
  { "cc_mhd_imap"     , VAR(map.cc_imap)     , MRC_VAR_OBJ(mrc_fld)      },

  { "rr"              , VAR(bnvals[FIXED_RR]), PARAM_DOUBLE(1.) },
  { "pp"              , VAR(bnvals[FIXED_PP]), PARAM_DOUBLE(1.) },
  { "vx"              , VAR(bnvals[FIXED_VX]), PARAM_DOUBLE(0.) },
  { "vy"              , VAR(bnvals[FIXED_VY]), PARAM_DOUBLE(0.) },
  { "vz"              , VAR(bnvals[FIXED_VZ]), PARAM_DOUBLE(0.) },
  { "bx"              , VAR(bnvals[FIXED_BX]), PARAM_DOUBLE(0.) },
  { "by"              , VAR(bnvals[FIXED_BY]), PARAM_DOUBLE(0.) },
  { "bz"              , VAR(bnvals[FIXED_BZ]), PARAM_DOUBLE(0.) },

  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_bnd subclass "sphere"

struct ggcm_mhd_bnd_ops ggcm_mhd_bnd_ops_sphere = {
  .name             = ggcm_mhd_bnd_sub_name,
  .size             = sizeof(struct ggcm_mhd_bnd_sphere),
  .param_descr      = ggcm_mhd_bnd_sphere_descr,
  .setup            = ggcm_mhd_bnd_sphere_setup,
  .fill_ghosts      = ggcm_mhd_bnd_sphere_fill_ghosts,
};

