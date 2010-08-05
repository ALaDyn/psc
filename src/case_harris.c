
#include "psc.h"
#include "util/params.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// WFox: Plasma simulation parameters
//       needed because they determine length scales for initial conditions
// BB:  peak Harris magnetic field  (magnitude gives ratio w_ce/w_pe)
// nnb: number of background particles (density max == 1)
// TTe,TTi:  bulk temperature of electrons and ions (units m_e c^2)
// MMi: ion mass / electron mass
// LLL = reversal length scale (units of c/wpe)
// LLz, LLx = simulation box size (units of c/wpe)
// AA = perturbation (units of B * de)

// FIXME (description), below parameters don't include scaling factors

struct harris {
  double BB;
  double nnb;
  double Te, Ti;
  double MMi;
  double lambda;
  double lx, lz;
  double pert;
};

#define VAR(x) (void *)offsetof(struct harris, x)

static struct param harris_descr[] = {
  { "BB"            , VAR(BB)              , PARAM_DOUBLE(1.)     },
  { "MMi"           , VAR(MMi)             , PARAM_DOUBLE(25.)    },
  { "nnb"           , VAR(nnb)             , PARAM_DOUBLE(.2)     },
  { "Te"            , VAR(Te)              , PARAM_DOUBLE(1./12.) },
  { "Ti"            , VAR(Ti)              , PARAM_DOUBLE(5./12.) },
  { "lambda"        , VAR(lambda)          , PARAM_DOUBLE(.5)     },
  { "lx"            , VAR(lx)              , PARAM_DOUBLE(25.6)   },
  { "lz"            , VAR(lz)              , PARAM_DOUBLE(12.8)   },
  { "pert"          , VAR(pert)            , PARAM_DOUBLE(.1)     },
  {},
};

#undef VAR

static void
harris_init_param(struct psc_case *Case)
{
  struct harris *harris = Case->ctx;

  psc.prm.qq = 1.;
  psc.prm.mm = 1.;
  psc.prm.tt = 1.;
  psc.prm.cc = 1.;
  psc.prm.eps0 = 1.;

  psc.prm.nmax = 16000;
  psc.prm.cpum = 5*24.0*60*60;
  psc.prm.lw = 2.*M_PI;
  psc.prm.i0 = 0.;
  psc.prm.n0 = 1.;
  psc.prm.e0 = 1.;

  psc.prm.nicell = 50;

  psc.domain.length[0] = harris->lx * sqrt(harris->MMi);
  psc.domain.length[1] = 10000000.; // no y dependence 
  psc.domain.length[2] = 2. * harris->lz * sqrt(harris->MMi); // double tearing

  psc.domain.itot[0] = 640;
  psc.domain.itot[1] = 1;
  psc.domain.itot[2] = 640;
  psc.domain.ilo[0] = 0;
  psc.domain.ilo[1] = 0;
  psc.domain.ilo[2] = 0;
  psc.domain.ihi[0] = 640;
  psc.domain.ihi[1] = 1;
  psc.domain.ihi[2] = 640;

  psc.domain.bnd_fld_lo[0] = BND_FLD_PERIODIC;
  psc.domain.bnd_fld_hi[0] = BND_FLD_PERIODIC;
  psc.domain.bnd_fld_lo[1] = BND_FLD_PERIODIC;
  psc.domain.bnd_fld_hi[1] = BND_FLD_PERIODIC;
  psc.domain.bnd_fld_lo[2] = BND_FLD_PERIODIC;
  psc.domain.bnd_fld_hi[2] = BND_FLD_PERIODIC;
  psc.domain.bnd_part[0] = BND_PART_PERIODIC;
  psc.domain.bnd_part[1] = BND_PART_PERIODIC;
  psc.domain.bnd_part[2] = BND_PART_PERIODIC;
}

static void
harris_init_field(struct psc_case *Case)
{
  struct harris *harris = Case->ctx;

  double BB = harris->BB, MMi = harris->MMi;
  double LLx = harris->lx * sqrt(MMi), LLz = harris->lz * sqrt(MMi);
  double LLL = harris->lambda * sqrt(MMi);
  double AA = harris->pert * BB * sqrt(MMi);

  // FIXME, do we need the ghost points?
  for (int jz = psc.ilg[2]; jz < psc.ihg[2]; jz++) {
    for (int jy = psc.ilg[1]; jy < psc.ihg[1]; jy++) {
      for (int jx = psc.ilg[0]; jx < psc.ihg[0]; jx++) {
	double dx = psc.dx[0], dz = psc.dx[2];
	double xx = jx * dx, zz = jz * dz;

	F3_BASE(HX, jx,jy,jz) = 
	  BB * (-1. 
		+ tanh((zz + .5*dz - 0.5*LLz)/LLL)
		- tanh((zz + .5*dz - 1.5*LLz)/LLL))
	  + AA*M_PI/LLz * sin(2.*M_PI*xx/LLx) * cos(M_PI*(zz+.5*dz)/LLz);

	F3_BASE(HZ, jx,jy,jz) =
	  - AA*2.*M_PI/LLx * cos(2.*M_PI*(xx+.5*dx)/LLx) * sin(M_PI*zz/LLz);

	F3_BASE(JYI, jx,jy,jz) = BB/LLL *
	  (1./sqr(cosh((zz - 0.5*LLz)/LLL)) -1./sqr(cosh((zz - 1.5*LLz)/LLL)))
	  - (AA*sqr(M_PI) * (1./sqr(LLz) + 4./sqr(LLx)) 
	     * sin(2.*M_PI*xx/LLx) * sin(M_PI*zz/LLz));
      }
    }
  }
}

static void
harris_init_npt(struct psc_case *Case, int kind, double x[3],
		struct psc_particle_npt *npt)
{
  struct harris *harris = Case->ctx;

  double BB = harris->BB, MMi = harris->MMi;
  double LLz = harris->lz * sqrt(MMi);
  double LLL = harris->lambda * sqrt(MMi);
  double nnb = harris->nnb;
  double TTi = harris->Ti * sqr(BB);
  double TTe = harris->Te * sqr(BB);

  double jy0 = 1./sqr(cosh((x[2]-0.5*LLz)/LLL)) - 1./sqr(cosh((x[2]-1.5*LLz)/LLL));

  switch (kind) {
  case 0: // electrons
    npt->q = -1.;
    npt->m = 1.;
    npt->n = nnb + 1./sqr(cosh((x[2]-0.5*LLz)/LLL)) + 1./sqr(cosh((x[2]-1.5*LLz)/LLL));
    npt->p[1] = - 2. * TTe / BB / LLL * jy0 / npt->n;
    npt->T[0] = TTe;
    npt->T[1] = TTe;
    npt->T[2] = TTe;
    break;
  case 1: // ions
    npt->q = 1.;
    npt->m = harris->MMi;
    npt->n = nnb + 1./sqr(cosh((x[2]-0.5*LLz)/LLL)) + 1./sqr(cosh((x[2]-1.5*LLz)/LLL));
    npt->p[1] = 2. * TTi / BB / LLL * jy0 / npt->n;
    npt->T[0] = TTi;
    npt->T[1] = TTi;
    npt->T[2] = TTi;
    break;
  default:
    assert(0);
  }
}

struct psc_case_ops psc_case_ops_harris = {
  .name       = "harris",
  .ctx_size   = sizeof(struct harris),
  .ctx_descr  = harris_descr,
  .init_param = harris_init_param,
  .init_field = harris_init_field,
  .init_npt   = harris_init_npt,
};

// ----------------------------------------------------------------------
// case test_xz:
//
// basically the same as harris

static void
test_xz_init_param(struct psc_case *Case)
{
  harris_init_param(Case);
  
  psc.prm.nicell = 100;

  psc.domain.itot[0] = 64;
  psc.domain.itot[1] = 1;
  psc.domain.itot[2] = 64;
  psc.domain.ilo[0] = 0;
  psc.domain.ilo[1] = 0;
  psc.domain.ilo[2] = 0;
  psc.domain.ihi[0] = 64;
  psc.domain.ihi[1] = 1;
  psc.domain.ihi[2] = 64;
  
}

struct psc_case_ops psc_case_ops_test_xz = {
  .name       = "test_xz",
  .ctx_size   = sizeof(struct harris),
  .ctx_descr  = harris_descr,
  .init_param = test_xz_init_param,
  .init_field = harris_init_field,
  .init_npt   = harris_init_npt,
};

// ----------------------------------------------------------------------
// case test_yz:
//
// basically the same as harris, but with different coordinates

static void
test_yz_init_param(struct psc_case *Case)
{
  harris_init_param(Case);
  
  struct harris *harris = Case->ctx;

  psc.prm.nicell = 100;

  psc.domain.length[0] = 10000000;
  psc.domain.length[1] = harris->lx * sqrt(harris->MMi);
  psc.domain.length[2] = 2. * harris->lz * sqrt(harris->MMi); // double tearing

  psc.domain.itot[0] = 1;
  psc.domain.itot[1] = 64;
  psc.domain.itot[2] = 64;
  psc.domain.ilo[0] = 0;
  psc.domain.ilo[1] = 0;
  psc.domain.ilo[2] = 0;
  psc.domain.ihi[0] = 1;
  psc.domain.ihi[1] = 64;
  psc.domain.ihi[2] = 64;
  
}

static void
test_yz_init_field(struct psc_case *Case)
{
  struct harris *harris = Case->ctx;

  double BB = harris->BB, MMi = harris->MMi;
  double LLy = harris->lx * sqrt(MMi), LLz = harris->lz * sqrt(MMi);
  double LLL = harris->lambda * sqrt(MMi);
  double AA = harris->pert * BB * sqrt(MMi);

  // FIXME, do we need the ghost points?
  for (int jz = psc.ilg[2]; jz < psc.ihg[2]; jz++) {
    for (int jy = psc.ilg[1]; jy < psc.ihg[1]; jy++) {
      for (int jx = psc.ilg[0]; jx < psc.ihg[0]; jx++) {
	double dy = psc.dx[1], dz = psc.dx[2];
	double yy = jy * dy, zz = jz * dz;

	F3_BASE(HY, jx,jy,jz) = 
	  BB * (-1. 
		+ tanh((zz + .5*dz - 0.5*LLz)/LLL)
		- tanh((zz + .5*dz - 1.5*LLz)/LLL))
	  + AA*M_PI/LLz * sin(2.*M_PI*yy/LLy) * cos(M_PI*(zz+.5*dz)/LLz);

	F3_BASE(HZ, jx,jy,jz) =
	  - AA*2.*M_PI/LLy * cos(2.*M_PI*(yy+.5*dy)/LLy) * sin(M_PI*zz/LLz);

	F3_BASE(JXI, jx,jy,jz) = - BB/LLL *
	  (1./sqr(cosh((zz - 0.5*LLz)/LLL)) -1./sqr(cosh((zz - 1.5*LLz)/LLL)))
	  - (AA*sqr(M_PI) * (1./sqr(LLz) + 4./sqr(LLy)) 
	     * sin(2.*M_PI*yy/LLy) * sin(M_PI*zz/LLz));
      }
    }
  }
}

static void
test_yz_init_npt(struct psc_case *Case, int kind, double x[3],
		 struct psc_particle_npt *npt)
{
  struct harris *harris = Case->ctx;

  double BB = harris->BB, MMi = harris->MMi;
  double LLz = harris->lz * sqrt(MMi);
  double LLL = harris->lambda * sqrt(MMi);
  double nnb = harris->nnb;
  double TTi = harris->Ti * sqr(BB);
  double TTe = harris->Te * sqr(BB);

  double jx0 = -1./sqr(cosh((x[2]-0.5*LLz)/LLL)) - 1./sqr(cosh((x[2]-1.5*LLz)/LLL));

  switch (kind) {
  case 0: // electrons
    npt->q = -1.;
    npt->m = 1.;
    npt->n = nnb + 1./sqr(cosh((x[2]-0.5*LLz)/LLL)) + 1./sqr(cosh((x[2]-1.5*LLz)/LLL));
    npt->p[0] = - 2. * TTe / BB / LLL * jx0 / npt->n;
    npt->T[0] = TTe;
    npt->T[1] = TTe;
    npt->T[2] = TTe;
    break;
  case 1: // ions
    npt->q = 1.;
    npt->m = harris->MMi;
    npt->n = nnb + 1./sqr(cosh((x[2]-0.5*LLz)/LLL)) + 1./sqr(cosh((x[2]-1.5*LLz)/LLL));
    npt->p[0] = 2. * TTi / BB / LLL * jx0 / npt->n;
    npt->T[0] = TTi;
    npt->T[1] = TTi;
    npt->T[2] = TTi;
    break;
  default:
    assert(0);
  }
}

struct psc_case_ops psc_case_ops_test_yz = {
  .name       = "test_yz",
  .ctx_size   = sizeof(struct harris),
  .ctx_descr  = harris_descr,
  .init_param = test_yz_init_param,
  .init_field = test_yz_init_field,
  .init_npt   = test_yz_init_npt,
};
