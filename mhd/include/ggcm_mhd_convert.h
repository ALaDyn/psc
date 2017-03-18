
#ifndef GGCM_MHD_CONVERT_H
#define GGCM_MHD_CONVERT_H

#include "ggcm_mhd_defs.h"

static mrc_fld_data_t cvt_gamma;
static mrc_fld_data_t cvt_gamma_m1;
static mrc_fld_data_t cvt_gamma_m1_inv;

static inline void ggcm_mhd_convert_setup(struct ggcm_mhd *mhd)
{
  cvt_gamma = mhd->par.gamm;
  cvt_gamma_m1 = cvt_gamma - 1.f;
  cvt_gamma_m1_inv = 1.f / cvt_gamma_m1;
}

static inline void
convert_state_from_prim_scons(mrc_fld_data_t state[5], mrc_fld_data_t prim[5])
{
  assert(cvt_gamma);
  
  state[RR ] = prim[RR];
  state[RVX] = prim[RR] * prim[VX];
  state[RVY] = prim[RR] * prim[VY];
  state[RVZ] = prim[RR] * prim[VZ];
  state[UU ] = prim[PP] * cvt_gamma_m1_inv +
    + .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ]));
}
      
static inline void
convert_prim_from_state_scons(mrc_fld_data_t prim[5], mrc_fld_data_t state[5])
{
  assert(cvt_gamma);
  
  prim[RR] = state[RR];
  mrc_fld_data_t rri = 1.f / state[RR];
  prim[VX] = rri * state[RVX];
  prim[VY] = rri * state[RVY];
  prim[VZ] = rri * state[RVZ];
  prim[PP] = cvt_gamma_m1 * (state[UU] 
			     - .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ])));
}
      
static inline void
convert_state_from_prim_fcons(mrc_fld_data_t state[5], mrc_fld_data_t prim[8])
{
  assert(cvt_gamma);

  state[RR ] = prim[RR];
  state[RVX] = prim[RR] * prim[VX];
  state[RVY] = prim[RR] * prim[VY];
  state[RVZ] = prim[RR] * prim[VZ];
  state[EE ] = prim[PP] * cvt_gamma_m1_inv
    + .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ]))
    + .5f * (sqr(prim[BX]) + sqr(prim[BY]) + sqr(prim[BZ]));
}

static inline void
convert_prim_from_state_fcons(mrc_fld_data_t prim[5], mrc_fld_data_t state[8])
{
  assert(cvt_gamma);
  
  prim[RR] = state[RR];
  mrc_fld_data_t rri = 1.f / state[RR];
  prim[VX] = rri * state[RVX];
  prim[VY] = rri * state[RVY];
  prim[VZ] = rri * state[RVZ];
  prim[PP] = cvt_gamma_m1 * (state[UU] 
			     - .5f * prim[RR] * (sqr(prim[VX]) + sqr(prim[VY]) + sqr(prim[VZ]))
			     - .5f * (sqr(state[BX]) + sqr(state[BY]) + sqr(state[BZ])));
}
      
#endif
