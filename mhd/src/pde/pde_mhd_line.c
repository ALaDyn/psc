
// ----------------------------------------------------------------------
// mhd_get_line_state_fcons

static void _mrc_unused
mhd_get_line_state_fcons(fld1d_state_t u, struct mrc_fld *U,
			 int j, int k, int dir, int p, int ib, int ie)
{
#define GET_LINE(X,Y,Z,I,J,K) do {			\
    for (int i = ib; i < ie; i++) {			\
      F1S(u, RR , i) = M3(U, RR   , I,J,K, p);		\
      F1S(u, RVX, i) = M3(U, RVX+X, I,J,K, p);		\
      F1S(u, RVY, i) = M3(U, RVX+Y, I,J,K, p);		\
      F1S(u, RVZ, i) = M3(U, RVX+Z, I,J,K, p);		\
      F1S(u, EE , i) = M3(U, EE   , I,J,K, p);		\
      F1S(u, BX , i) = M3(U, BX+X , I,J,K, p);		\
      F1S(u, BY , i) = M3(U, BX+Y , I,J,K, p);		\
      F1S(u, BZ , i) = M3(U, BX+Z , I,J,K, p);		\
      if (s_opt_divb == OPT_DIVB_GLM) {			\
	F1S(u, PSI, i) = M3(U, PSI, I,J,K, p);		\
      }							\
    }							\
  } while (0)

  if (dir == 0) {
    GET_LINE(0,1,2, i,j,k);
  } else if (dir == 1) {
    GET_LINE(1,2,0, k,i,j);
  } else if (dir == 2) {
    GET_LINE(2,0,1, j,k,i);
  }
#undef GET_LINE
}

// ----------------------------------------------------------------------
// mhd_put_line_state_fcons

static void _mrc_unused
mhd_put_line_state_fcons(struct mrc_fld *flux, fld1d_state_t F,
			 int j, int k, int dir, int p, int ib, int ie)
{
#define PUT_LINE(X, Y, Z, I, J, K) do {				\
    for (int i = ib; i < ie; i++) {				\
      M3(flux, RR     , I,J,K, p) = F1S(F, RR , i);		\
      M3(flux, RVX + X, I,J,K, p) = F1S(F, RVX, i);		\
      M3(flux, RVX + Y, I,J,K, p) = F1S(F, RVY, i);		\
      M3(flux, RVX + Z, I,J,K, p) = F1S(F, RVZ, i);		\
      M3(flux, EE     , I,J,K, p) = F1S(F, EE , i);		\
      M3(flux, BX + X , I,J,K, p) = F1S(F, BX , i);		\
      M3(flux, BX + Y , I,J,K, p) = F1S(F, BY , i);		\
      M3(flux, BX + Z , I,J,K, p) = F1S(F, BZ , i);		\
      if (s_opt_divb == OPT_DIVB_GLM) {				\
	M3(flux, PSI, I,J,K, p)   = F1S(F, PSI, i);		\
      }								\
    }								\
} while (0)

  if (dir == 0) {
    PUT_LINE(0, 1, 2, i, j, k);
  } else if (dir == 1) {
    PUT_LINE(1, 2, 0, k, i, j);
  } else if (dir == 2) {
    PUT_LINE(2, 0, 1, j, k, i);
  } else {
    assert(0);
  }
#undef PUT_LINE
}


// ----------------------------------------------------------------------
// mhd_get_line_state_fcons_ct

static void _mrc_unused
mhd_get_line_state_fcons_ct(fld1d_state_t u, fld1d_t bx,
			    struct mrc_fld *U, struct mrc_fld *Bcc,
			    int j, int k, int dir, int p, int ib, int ie)
{
#define GET_LINE(X, Y, Z, I, J, K)			       \
  do {							       \
    for (int i = ib; i < ie; i++) {			       \
      F1S(u, RR , i) = M3(U, RR     , I,J,K, p);	       \
      F1S(u, RVX, i) = M3(U, RVX + X, I,J,K, p);	       \
      F1S(u, RVY, i) = M3(U, RVX + Y, I,J,K, p);	       \
      F1S(u, RVZ, i) = M3(U, RVX + Z, I,J,K, p);	       \
      F1S(u, EE , i) = M3(U, EE     , I,J,K, p);	       \
      F1S(u, BX , i) = M3(Bcc, X    , I,J,K, p);	       \
      F1S(u, BY , i) = M3(Bcc, Y    , I,J,K, p);	       \
      F1S(u, BZ , i) = M3(Bcc, Z    , I,J,K, p);	       \
      F1(bx, i)      = M3(U, BX+X   , I,J,K, p);	       \
    }							       \
  } while (0)
  if (dir == 0) {
    GET_LINE(0, 1, 2, i, j, k);
  } else if (dir == 1) {
    GET_LINE(1, 2, 0, k, i, j);
  } else if (dir == 2) {
    GET_LINE(2, 0, 1, j, k, i);
  } else {
    assert(0);
  }
#undef GET_LINE
}

// ----------------------------------------------------------------------
// mhd_put_line_state_fcons_ct

static void _mrc_unused
mhd_put_line_state_fcons_ct(struct mrc_fld *flux, fld1d_state_t F,
			    int j, int k, int dir, int p, int ib, int ie)
{
#define PUT_LINE(X, Y, Z, I, J, K) do {				\
    for (int i = ib; i < ie; i++) {				\
      M3(flux, RR     , I,J,K, p) = F1S(F, RR , i);		\
      M3(flux, RVX + X, I,J,K, p) = F1S(F, RVX, i);		\
      M3(flux, RVX + Y, I,J,K, p) = F1S(F, RVY, i);		\
      M3(flux, RVX + Z, I,J,K, p) = F1S(F, RVZ, i);		\
      M3(flux, EE     , I,J,K, p) = F1S(F, EE , i);		\
      M3(flux, BX + Y , I,J,K, p) = F1S(F, BY , i);		\
      M3(flux, BX + Z , I,J,K, p) = F1S(F, BZ , i);		\
    }								\
} while (0)

  if (dir == 0) {
    PUT_LINE(0, 1, 2, i, j, k);
  } else if (dir == 1) {
    PUT_LINE(1, 2, 0, k, i, j);
  } else if (dir == 2) {
    PUT_LINE(2, 0, 1, j, k, i);
  } else {
    assert(0);
  }
#undef PUT_LINE
}


// ----------------------------------------------------------------------
// mhd_get_line_state_scons

static void _mrc_unused
mhd_get_line_state_scons(fld1d_state_t u, struct mrc_fld *U,
			 int j, int k, int dim, int p, int ib, int ie)
{
#define GET_LINE(X,Y,Z,I,J,K) do {			\
    for (int i = ib; i < ie; i++) {			\
      F1S(u, RR , i) = M3(U, RR   , I,J,K, p);		\
      F1S(u, RVX, i) = M3(U, RVX+X, I,J,K, p);		\
      F1S(u, RVY, i) = M3(U, RVX+Y, I,J,K, p);		\
      F1S(u, RVZ, i) = M3(U, RVX+Z, I,J,K, p);		\
      F1S(u, UU , i) = M3(U, UU   , I,J,K, p);		\
    }							\
  } while (0)

  if (dim == 0) {
    GET_LINE(0,1,2, i,j,k);
  } else if (dim == 1) {
    GET_LINE(1,2,0, k,i,j);
  } else if (dim == 2) {
    GET_LINE(2,0,1, j,k,i);
  }
#undef GET_LINE
}

// ----------------------------------------------------------------------
// mhd_put_line_state_scons

static void _mrc_unused
mhd_put_line_state_scons(struct mrc_fld *flux, fld1d_state_t F,
			 int j, int k, int dir, int p, int ib, int ie)
{
#define PUT_LINE(X,Y,Z, I,J,K) do {					\
    for (int i = ib; i < ie; i++) {					\
      M3(flux, RR   , I,J,K, p) = F1S(F, RR , i);			\
      M3(flux, RVX+X, I,J,K, p) = F1S(F, RVX, i);			\
      M3(flux, RVX+Y, I,J,K, p) = F1S(F, RVY, i);			\
      M3(flux, RVX+Z, I,J,K, p) = F1S(F, RVZ, i);			\
      M3(flux, UU   , I,J,K, p) = F1S(F, UU , i);			\
    }									\
  } while (0)

  if (dir == 0) {
    PUT_LINE(0,1,2, i,j,k);
  } else if (dir == 1) {
    PUT_LINE(1,2,0, k,i,j);
  } else if (dir == 2) {
    PUT_LINE(2,0,1, j,k,i);
  }
#undef PUT_LINE
}

// ----------------------------------------------------------------------
// mhd_get_line_state

static void _mrc_unused
mhd_get_line_state(fld1d_state_t u, struct mrc_fld *U, int j, int k, int dir,
                  int p, int ib, int ie)
{
  if (s_opt_eqn == OPT_EQN_MHD_FCONS) {
    mhd_get_line_state_fcons(u, U, j, k, dir, p, ib, ie);
  } else if (s_opt_eqn == OPT_EQN_MHD_SCONS ||
	     s_opt_eqn == OPT_EQN_HD) {
    mhd_get_line_state_scons(u, U, j, k, dir, p, ib, ie);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// mhd_put_line_state

static void _mrc_unused
mhd_put_line_state(struct mrc_fld *U, fld1d_state_t u,
		   int j, int k, int dir, int p, int ib, int ie)
{
  if (s_opt_eqn == OPT_EQN_MHD_FCONS) {
    mhd_put_line_state_fcons(U, u, j, k, dir, p, ib, ie);
  } else if (s_opt_eqn == OPT_EQN_MHD_SCONS ||
	     s_opt_eqn == OPT_EQN_HD) {
    mhd_put_line_state_scons(U, u, j, k, dir, p, ib, ie);
  } else {
    assert(0);
  }
}
