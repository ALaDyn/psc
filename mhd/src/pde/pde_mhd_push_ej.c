
#ifndef PDE_MHD_PUSHEJ_C
#define PDE_MHD_PUSHEJ_C

#include "pde/pde_mhd_calc_current.c"

// ----------------------------------------------------------------------
// patch_push_ej_c

static void
patch_push_ej_c(fld3d_t p_Unext, mrc_fld_data_t dt, fld3d_t p_Ucurr,
		fld3d_t p_W, fld3d_t p_zmask, fld3d_t p_f)
{
  fld3d_t p_Jec, p_Bcc;
  fld3d_setup_view(&p_Jec  , p_f, _BX);
  fld3d_setup_view(&p_Bcc  , p_f, _TMP1);

  patch_calc_current_ec(p_Jec, p_Ucurr);
  patch_primbb(p_Bcc, p_Ucurr);
	
  mrc_fld_data_t s1 = .25f * dt;
  fld3d_foreach(i,j,k, 0, 0) {
    mrc_fld_data_t z = F3S(p_zmask, 0, i,j,k);
    mrc_fld_data_t s2 = s1 * z;
    mrc_fld_data_t cx = (F3S(p_Jec, 0, i,j  ,k  ) + F3S(p_Jec, 0, i,j-1,k  ) +
			 F3S(p_Jec, 0, i,j  ,k-1) + F3S(p_Jec, 0, i,j-1,k-1));
    mrc_fld_data_t cy = (F3S(p_Jec, 1, i  ,j,k  ) + F3S(p_Jec, 1, i-1,j,k  ) +
			 F3S(p_Jec, 1, i  ,j,k-1) + F3S(p_Jec, 1, i-1,j,k-1));
    mrc_fld_data_t cz = (F3S(p_Jec, 2, i  ,j  ,k) + F3S(p_Jec, 2, i-1,j  ,k) +
			 F3S(p_Jec, 2, i  ,j-1,k) + F3S(p_Jec, 2, i-1,j-1,k));
    mrc_fld_data_t ffx = s2 * (cy * F3S(p_Bcc, 2, i,j,k) - cz * F3S(p_Bcc, 1, i,j,k));
    mrc_fld_data_t ffy = s2 * (cz * F3S(p_Bcc, 0, i,j,k) - cx * F3S(p_Bcc, 2, i,j,k));
    mrc_fld_data_t ffz = s2 * (cx * F3S(p_Bcc, 1, i,j,k) - cy * F3S(p_Bcc, 0, i,j,k));
    mrc_fld_data_t duu = (ffx * F3S(p_W, VX, i,j,k) +
			  ffy * F3S(p_W, VY, i,j,k) +
			  ffz * F3S(p_W, VZ, i,j,k));

    F3S(p_Unext, RVX, i,j,k) += ffx;
    F3S(p_Unext, RVY, i,j,k) += ffy;
    F3S(p_Unext, RVZ, i,j,k) += ffz;
    F3S(p_Unext, UU , i,j,k) += duu;
  } fld3d_foreach_end;
}

// ----------------------------------------------------------------------
// patch_push_ej_fortran

#if defined(HAVE_OPENGGCM_FORTRAN) && defined(MRC_FLD_AS_FLOAT_H)

#define push_ej_F77 F77_FUNC(push_ej,PUSH_EJ)

void push_ej_F77(real *b1x, real *b1y, real *b1z,
		 real *rv1x, real *rv1y, real *rv1z, real *uu1,
		 real *zmask, real *vx, real *vy, real *vz,
		 real *dt);

static void
patch_push_ej_fortran(fld3d_t p_Unext, mrc_fld_data_t dt, fld3d_t p_Ucurr,
		      fld3d_t p_W, fld3d_t p_zmask, fld3d_t p_f)
{
  push_ej_F77(F(p_Ucurr, BX), F(p_Ucurr, BY), F(p_Ucurr, BZ),
	      F(p_Unext, RVX), F(p_Unext, RVY), F(p_Unext, RVZ), F(p_Unext, UU), 
	      F(p_zmask, 0), F(p_W, VX), F(p_W, VY), F(p_W, VZ), &dt);
}

#endif

// ----------------------------------------------------------------------
// patch_push_ej

static void
patch_push_ej(fld3d_t p_Unext, mrc_fld_data_t dt, fld3d_t p_Ucurr,
	      fld3d_t p_W, fld3d_t p_zmask, fld3d_t p_f)
{
  if (s_opt_mhd_push_ej == OPT_MHD_C) {
    patch_push_ej_c(p_Unext, dt, p_Ucurr, p_W, p_zmask, p_f);
#if defined(HAVE_OPENGGCM_FORTRAN) && defined(MRC_FLD_AS_FLOAT_H)
  } else if (s_opt_mhd_push_ej == OPT_MHD_FORTRAN) {
    patch_push_ej_fortran(p_Unext, dt, p_Ucurr, p_W, p_zmask, p_f);
#endif
  } else {
    assert(0);
  }
}

#endif
