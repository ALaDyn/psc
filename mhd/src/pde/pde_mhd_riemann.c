
// ----------------------------------------------------------------------
// constants

static mrc_fld_data_t Gamma _mrc_unused;

// ----------------------------------------------------------------------
// fluxes_fc

static inline void
fluxes_fc(mrc_fld_data_t F[8], mrc_fld_data_t U[8], mrc_fld_data_t W[8])
{
  mrc_fld_data_t b2 = sqr(W[BX]) + sqr(W[BY]) + sqr(W[BZ]);

  F[RR]  = W[RR] * W[VX];
  F[RVX] = W[RR] * W[VX] * W[VX] + W[PP] + .5 * b2 - W[BX] * W[BX];
  F[RVY] = W[RR] * W[VY] * W[VX]                   - W[BY] * W[BX];
  F[RVZ] = W[RR] * W[VZ] * W[VX]                   - W[BZ] * W[BX];
  F[EE] = (U[EE] + W[PP] + .5 * b2) * W[VX]
    - W[BX] * (W[BX] * W[VX] + W[BY] * W[VY] + W[BZ] * W[VZ]);
  F[BX] = 0;
  F[BY] = W[BY] * W[VX] - W[BX] * W[VY];
  F[BZ] = W[BZ] * W[VX] - W[BX] * W[VZ]; 
}

// ----------------------------------------------------------------------
// fluxes_sc

static void // FIXME, duplicated
fluxes_sc(mrc_fld_data_t F[5], mrc_fld_data_t U[5], mrc_fld_data_t W[5])
{
  F[RR]  = W[RR] * W[VX];
  F[RVX] = W[RR] * W[VX] * W[VX];
  F[RVY] = W[RR] * W[VY] * W[VX];
  F[RVZ] = W[RR] * W[VZ] * W[VX];
  F[UU]  = (U[UU] + W[PP]) * W[VX];
}

// ----------------------------------------------------------------------
// fluxes_hydro

// FIXME, sc vs hydro is a kinda arbitrary distinction, but the former does not
// contain pressure, because that's what's needed for Jimmy-MHD ("c3")

static void
fluxes_hydro(mrc_fld_data_t F[5], mrc_fld_data_t U[5], mrc_fld_data_t W[5])
{
  F[RR]  = W[RR] * W[VX];
  F[RVX] = W[RR] * W[VX] * W[VX] + W[PP];
  F[RVY] = W[RR] * W[VY] * W[VX];
  F[RVZ] = W[RR] * W[VZ] * W[VX];
  F[UU]  = (U[UU] + W[PP]) * W[VX];
}

// ----------------------------------------------------------------------
// wavespeed_fc
//
// calculate speed of fastest (fast magnetosonic) wave

static inline mrc_fld_data_t
wavespeed_fc(mrc_fld_data_t U[8], mrc_fld_data_t W[8])
{
  mrc_fld_data_t cs2 = Gamma * W[PP] / W[RR];
  mrc_fld_data_t b2 = sqr(W[BX]) + sqr(W[BY]) + sqr(W[BZ]);
  mrc_fld_data_t as2 = b2 / W[RR]; 
  mrc_fld_data_t cf2 = .5f * (cs2 + as2 + 
			      mrc_fld_sqrt(sqr(as2 + cs2) - (4.f * sqr(sqrt(cs2) * W[BX]) / W[RR])));
  return mrc_fld_sqrt(cf2);
}

// ----------------------------------------------------------------------
// fluxes_rusanov_fc

static void
fluxes_rusanov_fc(mrc_fld_data_t F[8], mrc_fld_data_t Ul[8], mrc_fld_data_t Ur[8],
		  mrc_fld_data_t Wl[8], mrc_fld_data_t Wr[8])
{
  mrc_fld_data_t Fl[8], Fr[8];
  mrc_fld_data_t cf;

  cf = wavespeed_fc(Ul, Wl);
  mrc_fld_data_t cp_l = Wl[VX] + cf;
  mrc_fld_data_t cm_l = Wl[VX] - cf; 
  fluxes_fc(Fl, Ul, Wl);

  cf = wavespeed_fc(Ur, Wr);
  mrc_fld_data_t cp_r = Wr[VX] + cf;
  mrc_fld_data_t cm_r = Wr[VX] - cf; 
  fluxes_fc(Fr, Ur, Wr);

  mrc_fld_data_t c_l = mrc_fld_max(mrc_fld_abs(cm_l), mrc_fld_abs(cp_l)); 
  mrc_fld_data_t c_r = mrc_fld_max(mrc_fld_abs(cm_r), mrc_fld_abs(cp_r)); 
  mrc_fld_data_t c_max = mrc_fld_max(c_l, c_r);

  for (int m = 0; m < 8; m++) {
    F[m] = .5f * (Fl[m] + Fr[m] - c_max * (Ur[m] - Ul[m]));
  }
}

// ----------------------------------------------------------------------
// fluxes_rusanov_sc

static void
fluxes_rusanov_sc(mrc_fld_data_t F[5], mrc_fld_data_t Ul[5], mrc_fld_data_t Ur[5],
		  mrc_fld_data_t Wl[5], mrc_fld_data_t Wr[5])
{
  mrc_fld_data_t Fl[5], Fr[5];
  
  fluxes_sc(Fl, Ul, Wl);
  fluxes_sc(Fr, Ur, Wr);

  mrc_fld_data_t vv, cs2;
  vv = sqr(Wl[VX]) + sqr(Wl[VY]) + sqr(Wl[VZ]);
  cs2 = Gamma * Wl[PP] / Wl[RR];
  mrc_fld_data_t cmsv_l = sqrtf(vv) + sqrtf(cs2);

  vv = sqr(Wr[VX]) + sqr(Wr[VY]) + sqr(Wr[VZ]);
  cs2 = Gamma * Wr[PP] / Wr[RR];
  mrc_fld_data_t cmsv_r = sqrtf(vv) + sqrtf(cs2);

  mrc_fld_data_t lambda = .5 * (cmsv_l + cmsv_r);
  
  for (int m = 0; m < 5; m++) {
    F[m] = .5f * ((Fr[m] + Fl[m]) - lambda * (Ur[m] - Ul[m]));
  }
}

// ----------------------------------------------------------------------
// fluxes_rusanov_hydro

static void
fluxes_rusanov_hydro(mrc_fld_data_t F[5], mrc_fld_data_t Ul[5], mrc_fld_data_t Ur[5],
		     mrc_fld_data_t Wl[5], mrc_fld_data_t Wr[5])
{
  mrc_fld_data_t Fl[5], Fr[5];
  
  fluxes_hydro(Fl, Ul, Wl);
  fluxes_hydro(Fr, Ur, Wr);

  mrc_fld_data_t vv, cs2;
  vv = sqr(Wl[VX]) + sqr(Wl[VY]) + sqr(Wl[VZ]);
  cs2 = Gamma * Wl[PP] / Wl[RR];
  mrc_fld_data_t cmsv_l = sqrtf(vv) + sqrtf(cs2);

  vv = sqr(Wr[VX]) + sqr(Wr[VY]) + sqr(Wr[VZ]);
  cs2 = Gamma * Wr[PP] / Wr[RR];
  mrc_fld_data_t cmsv_r = sqrtf(vv) + sqrtf(cs2);

  mrc_fld_data_t lambda = .5 * (cmsv_l + cmsv_r);
  
  for (int m = 0; m < 5; m++) {
    F[m] = .5f * ((Fr[m] + Fl[m]) - lambda * (Ur[m] - Ul[m]));
  }
}

// ----------------------------------------------------------------------
// fluxes_hll_fc

static void
fluxes_hll_fc(mrc_fld_data_t F[8], mrc_fld_data_t Ul[8], mrc_fld_data_t Ur[8],
	      mrc_fld_data_t Wl[8], mrc_fld_data_t Wr[8])
{
  mrc_fld_data_t Fl[8], Fr[8];
  mrc_fld_data_t cf;

  cf = wavespeed_fc(Ul, Wl);
  mrc_fld_data_t cp_l = Wl[VX] + cf;
  mrc_fld_data_t cm_l = Wl[VX] - cf; 
  fluxes_fc(Fl, Ul, Wl);

  cf = wavespeed_fc(Ur, Wr);
  mrc_fld_data_t cp_r = Wr[VX] + cf;
  mrc_fld_data_t cm_r = Wr[VX] - cf; 
  fluxes_fc(Fr, Ur, Wr);

  mrc_fld_data_t c_l =  mrc_fld_min(mrc_fld_min(cm_l, cm_r), 0.); 
  mrc_fld_data_t c_r =  mrc_fld_max(mrc_fld_max(cp_l, cp_r), 0.); 

  for (int m = 0; m < 8; m++) {
    F[m] = ((c_r * Fl[m] - c_l * Fr[m]) + (c_r * c_l * (Ur[m] - Ul[m]))) / (c_r - c_l);
  }
}

// ----------------------------------------------------------------------
// fluxes_hll_hydro

static void
fluxes_hll_hydro(mrc_fld_data_t F[5], mrc_fld_data_t Ul[5], mrc_fld_data_t Ur[5],
		 mrc_fld_data_t Wl[5], mrc_fld_data_t Wr[5])
{
  mrc_fld_data_t Fl[5], Fr[5];

  fluxes_hydro(Fl, Ul, Wl);
  fluxes_hydro(Fr, Ur, Wr);

  mrc_fld_data_t cs2;

  cs2 = Gamma * Wl[PP] / Wl[RR];
  mrc_fld_data_t cpv_l = Wl[VX] + sqrtf(cs2);
  mrc_fld_data_t cmv_l = Wl[VX] - sqrtf(cs2); 

  cs2 = Gamma * Wr[PP] / Wr[RR];
  mrc_fld_data_t cpv_r = Wr[VX] + sqrtf(cs2);
  mrc_fld_data_t cmv_r = Wr[VX] - sqrtf(cs2); 

  mrc_fld_data_t SR =  fmaxf(fmaxf(cpv_l, cpv_r), 0.); 
  mrc_fld_data_t SL =  fminf(fminf(cmv_l, cmv_r), 0.); 

  //  mrc_fld_data_t lambda = .5 * (cmsv_l + cmsv_r);  
  for (int m = 0; m < 5; m++) {
    F[m] = ((SR * Fl[m] - SL * Fr[m]) + (SR * SL * (Ur[m] - Ul[m]))) / (SR - SL);
  }
}

// ----------------------------------------------------------------------
// fluxes_hllc_hydro

static void
fluxes_hllc_hydro(mrc_fld_data_t F[5], mrc_fld_data_t Ul[5], mrc_fld_data_t Ur[5],
		  mrc_fld_data_t Wl[5], mrc_fld_data_t Wr[5])
{
  mrc_fld_data_t Fl[5], Fr[5];

  fluxes_hydro(Fl, Ul, Wl);
  fluxes_hydro(Fr, Ur, Wr);

  mrc_fld_data_t cs2;

  cs2 = Gamma * Wl[PP] / Wl[RR];
  mrc_fld_data_t cpv_l = Wl[VX] + sqrtf(cs2);
  mrc_fld_data_t cmv_l = Wl[VX] - sqrtf(cs2); 

  cs2 = Gamma * Wr[PP] / Wr[RR];
  mrc_fld_data_t cpv_r = Wr[VX] + sqrtf(cs2);
  mrc_fld_data_t cmv_r = Wr[VX] - sqrtf(cs2); 

  mrc_fld_data_t SR =  fmaxf(fmaxf(cpv_l, cpv_r), 0.); 
  mrc_fld_data_t SL =  fminf(fminf(cmv_l, cmv_r), 0.); 

  mrc_fld_data_t SRmUR = SR - Wr[VX];
  mrc_fld_data_t SLmUL = SL - Wl[VX];
  mrc_fld_data_t SM =
    (SRmUR * Wr[RR] * Wr[VX] - SLmUL * Wl[RR] * Wl[VX] - Wr[PP] + Wl[PP]) / 
    (SRmUR * Wr[RR] - SLmUL * Wl[RR]);

  mrc_fld_data_t spT= Wr[PP] + (Wr[RR] * SRmUR * (SM - Wr[VX]));

  mrc_fld_data_t sUR[5];
  mrc_fld_data_t sUL[5]; 
  
  sUR[0] = Wr[RR] * SRmUR / ( SR - SM );
  sUL[0] = Wl[RR] * SLmUL / ( SL - SM ); 

  sUR[1] = sUR[0] * SM;
  sUL[1] = sUL[0] * SM;
  sUR[2] = sUR[0] * Wr[VY]; 
  sUL[2] = sUL[0] * Wl[VY]; 
  sUR[3] = sUR[0] * Wr[VZ]; 
  sUL[3] = sUL[0] * Wl[VZ];

  sUR[4] = ((SR - Wr[VX]) * Ur[UU] - Wr[PP] * Wr[VX] + spT * SM) / (SR - SM); 
  sUL[4] = ((SL - Wl[VX]) * Ul[UU] - Wl[PP] * Wl[VX] + spT * SM) / (SL - SM); 

 for (int m = 0; m < 5; m++) {
   if ( SL > 0 ) {
     F[m] = Fl[m];
   } else if (( SL <= 0 ) && ( SM >= 0 )) {  
     F[m] = (SL * (sUL[m]-Ul[m])) + Fl[m];
   } else if (( SR >= 0 ) && ( SM <= 0 )) {
     F[m] = (SR * (sUR[m]-Ur[m])) + Fr[m];
   } else if ( SR < 0 ) { 	  
     F[m] = Fr[m];
   }
 }
}

// ----------------------------------------------------------------------
// mhd_riemann_rusanov_run_fc

static void _mrc_unused
mhd_riemann_rusanov_run_fc(struct ggcm_mhd *mhd, fld1d_state_t F,
			   fld1d_state_t U_l, fld1d_state_t U_r,
			   fld1d_state_t W_l, fld1d_state_t W_r,
			   int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_rusanov_fc(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
		      &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

// ----------------------------------------------------------------------
// mhd_riemann_rusanov_run_sc

static void _mrc_unused
mhd_riemann_rusanov_run_sc(struct ggcm_mhd *mhd, fld1d_state_t F,
			   fld1d_state_t U_l, fld1d_state_t U_r,
			   fld1d_state_t W_l, fld1d_state_t W_r,
			   int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_rusanov_sc(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
		      &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

// ----------------------------------------------------------------------
// mhd_riemann_rusanov_run_hydro

static void _mrc_unused
mhd_riemann_rusanov_run_hydro(struct ggcm_mhd *mhd, fld1d_state_t F,
			      fld1d_state_t U_l, fld1d_state_t U_r,
			      fld1d_state_t W_l, fld1d_state_t W_r,
			      int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_rusanov_hydro(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
			 &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

// ----------------------------------------------------------------------
// mhd_riemann_hll_run_fc

static void _mrc_unused
mhd_riemann_hll_run_fc(struct ggcm_mhd *mhd, fld1d_state_t F,
		       fld1d_state_t U_l, fld1d_state_t U_r,
		       fld1d_state_t W_l, fld1d_state_t W_r,
		       int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_hll_fc(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
		  &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

// ----------------------------------------------------------------------
// mhd_riemann_hll_run_hydro

static void _mrc_unused
mhd_riemann_hll_run_hydro(struct ggcm_mhd *mhd, fld1d_state_t F,
			  fld1d_state_t U_l, fld1d_state_t U_r,
			  fld1d_state_t W_l, fld1d_state_t W_r,
			  int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_hll_hydro(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
		     &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

// ----------------------------------------------------------------------
// mhd_riemann_hllc_run_hydro

static void _mrc_unused
mhd_riemann_hllc_run_hydro(struct ggcm_mhd *mhd, fld1d_state_t F,
			   fld1d_state_t U_l, fld1d_state_t U_r,
			   fld1d_state_t W_l, fld1d_state_t W_r,
			   int ldim, int l, int r, int dim)
{
  Gamma = mhd->par.gamm;

  for (int i = -l; i < ldim + r; i++) {
    fluxes_hllc_hydro(&F1S(F, 0, i), &F1S(U_l, 0, i), &F1S(U_r, 0, i),
		      &F1S(W_l, 0, i), &F1S(W_r, 0, i));
  }
}

