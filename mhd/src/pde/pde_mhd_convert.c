
#include "ggcm_mhd_private.h"

#define TINY_NUMBER 1.0e-20 // FIXME

// ----------------------------------------------------------------------
// mhd_prim_from_fcons

static void
mhd_prim_from_fcons(fld1d_state_t W, fld1d_state_t U, int ib, int ie)
{
  mrc_fld_data_t gamma_minus_1 = s_gamma - 1.f;

  for (int i = ib; i < ie; i++) {
    mrc_fld_data_t *w = &F1S(W, 0, i), *u = &F1S(U, 0, i);

    mrc_fld_data_t rri = 1.f / u[RR];
    w[RR] = u[RR];
    w[VX] = u[RVX] * rri;
    w[VY] = u[RVY] * rri;
    w[VZ] = u[RVZ] * rri;
    w[PP] = gamma_minus_1 * (u[EE] 
			     - .5 * (sqr(u[RVX]) + sqr(u[RVY]) + sqr(u[RVZ])) * rri
			     - .5 * (sqr(u[BX]) + sqr(u[BY]) + sqr(u[BZ])));
    w[PP] = fmax(w[PP], TINY_NUMBER);
    w[BX] = u[BX];
    w[BY] = u[BY];
    w[BZ] = u[BZ];
    if (s_opt_divb == OPT_DIVB_GLM) {
      w[PSI] = u[PSI];
    }
  }
}

// ----------------------------------------------------------------------
// mhd_fcons_from_prim

static void _mrc_unused
mhd_fcons_from_prim(fld1d_state_t U, fld1d_state_t W, int ib, int ie)
{
  mrc_fld_data_t gamma_minus_1 = s_gamma - 1.f;

  for (int i = ib; i < ie; i++) {
    mrc_fld_data_t *u = &F1S(U, 0, i), *w = &F1S(W, 0, i);

    mrc_fld_data_t rr = w[RR];
    u[RR ] = rr;
    u[RVX] = rr * w[VX];
    u[RVY] = rr * w[VY];
    u[RVZ] = rr * w[VZ];
    u[EE ] = 
      w[PP] / gamma_minus_1 +
      + .5 * (sqr(w[VX]) + sqr(w[VY]) + sqr(w[VZ])) * rr
      + .5 * (sqr(w[BX]) + sqr(w[BY]) + sqr(w[BZ]));
    u[BX ] = w[BX];
    u[BY ] = w[BY];
    u[BZ ] = w[BZ];
    if (s_opt_divb == OPT_DIVB_GLM) {
      u[PSI] = w[PSI];
    }
  }
}

// ----------------------------------------------------------------------
// mhd_prim_from_scons

static void
mhd_prim_from_scons(fld1d_state_t W, fld1d_state_t U, int ib, int ie)
{
  mrc_fld_data_t gamma_minus_1 = s_gamma - 1.f;

  for (int i = ib; i < ie; i++) {
    mrc_fld_data_t *w = &F1S(W, 0, i), *u = &F1S(U, 0, i);

    mrc_fld_data_t rri = 1.f / u[RR];
    w[RR] = u[RR];
    w[VX] = rri * u[RVX];
    w[VY] = rri * u[RVY];
    w[VZ] = rri * u[RVZ];
    mrc_fld_data_t rvv = (sqr(u[RVX]) + sqr(u[RVY]) + sqr(u[RVZ])) * rri;
    w[PP] = gamma_minus_1 * (u[UU] - .5 * rvv);
    w[PP] = mrc_fld_max(w[PP], TINY_NUMBER);
  }
}

// ----------------------------------------------------------------------
// mhd_scons_from_prim

static void _mrc_unused
mhd_scons_from_prim(fld1d_state_t U, fld1d_state_t W, int ib, int ie)
{
  mrc_fld_data_t gamma_minus_1 = s_gamma - 1.f;

  for (int i = ib; i < ie; i++) {
    mrc_fld_data_t *u = &F1S(U, 0, i), *w = &F1S(W, 0, i);

    mrc_fld_data_t rr = w[RR];
    u[RR ] = rr;
    u[RVX] = rr * w[VX];
    u[RVY] = rr * w[VY];
    u[RVZ] = rr * w[VZ];
    u[UU ] = 
      w[PP] / gamma_minus_1 +
      + .5 * (sqr(w[VX]) + sqr(w[VY]) + sqr(w[VZ])) * rr;
  }
}

// ----------------------------------------------------------------------
// mhd_prim_from_cons

static void _mrc_unused
mhd_prim_from_cons(fld1d_state_t W, fld1d_state_t U, int ib, int ie)
{
  if (s_opt_eqn == OPT_EQN_MHD_FCONS) {
    mhd_prim_from_fcons(W, U, ib, ie);
  } else if (s_opt_eqn == OPT_EQN_MHD_SCONS ||
	     s_opt_eqn == OPT_EQN_HD) {
    mhd_prim_from_scons(W, U, ib, ie);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// mhd_cons_from_prim

static void _mrc_unused
mhd_cons_from_prim(fld1d_state_t U, fld1d_state_t W, int ib, int ie)
{
  if (s_opt_eqn == OPT_EQN_MHD_FCONS) {
    mhd_fcons_from_prim(U, W, ib, ie);
  } else if (s_opt_eqn == OPT_EQN_MHD_SCONS ||
	     s_opt_eqn == OPT_EQN_HD) {
    mhd_scons_from_prim(U, W, ib, ie);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// patch_prim_from_cons

static void _mrc_unused
patch_prim_from_cons(fld3d_t p_W, fld3d_t p_U, int sw)
{
  static fld1d_state_t l_U, l_W;
  if (!fld1d_state_is_setup(l_U)) {
    fld1d_state_setup(&l_U);
    fld1d_state_setup(&l_W);
  }

  int dir = 0;
  pde_for_each_line(dir, j, k, sw) {
    int ib = -sw, ie = s_ldims[0] + sw;
    mhd_line_get_state(l_U, p_U, j, k, dir, ib, ie);
    mhd_prim_from_cons(l_W, l_U, ib, ie);
    mhd_line_put_state(l_W, p_W, j, k, dir, ib, ie);
  }
}

