
#include <psc.h>
#include <psc_push_particles.h>
#include <psc_push_fields.h>
#include <psc_sort.h>
#include <psc_balance.h>

#include <mrc_params.h>

#include <math.h>

// originally by WFox

// Plasma simulation parameters
//   needed because they determine length scales for initial conditions
// B0:  peak Harris magnetic field  (magnitude gives ratio w_ce/w_pe)
// nb:  background particle density
// Te, Ti:  bulk temperature of electrons and ions (units m_e c^2)
// mi_over_me: ion mass / electron mass
// lambda = shear length scale (units of d_i)
// pert = perturbation (units of B * d_i)

// FIXME (description), below parameters don't include scaling factors

struct psc_harris {
  double B0;
  double LLy, LLz; // in d_i
  double nb;
  double Te, Ti;
  double mi_over_me;
  double lambda; // in d_i
  double pert;

  // normalized quantities
  double LLL; // lambda in d_e
  double AA; // perturbation amplitude (from pert)
};

#define to_psc_harris(psc) mrc_to_subobj(psc, struct psc_harris)

#define VAR(x) (void *)offsetof(struct psc_harris, x)
static struct param psc_harris_descr[] = {
  { "B0"            , VAR(B0)              , PARAM_DOUBLE(.5)     },
  { "mi_over_me"    , VAR(mi_over_me)      , PARAM_DOUBLE(40.)    },
  { "nb"            , VAR(nb)              , PARAM_DOUBLE(.3)     },
  { "LLy"           , VAR(LLy)             , PARAM_DOUBLE(50.)    },
  { "LLz"           , VAR(LLz)             , PARAM_DOUBLE(200.)   },
  { "Te"            , VAR(Te)              , PARAM_DOUBLE(0.0625) },
  { "Ti"            , VAR(Ti)              , PARAM_DOUBLE(0.0625) },
  { "lambda"        , VAR(lambda)          , PARAM_DOUBLE(2.)     },
  { "pert"          , VAR(pert)            , PARAM_DOUBLE(.025)   },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_harris_create

static void
psc_harris_create(struct psc *psc)
{
  psc_default_dimensionless(psc);

  psc->prm.nmax = 16000;
  psc->prm.nicell = 50;
  psc->prm.nr_populations = 4;
  psc->prm.cfl = 0.98;

  // will be set to actual values in psc_harris_setup()
  psc->domain.length[0] = 1.; // no x dependence 
  psc->domain.length[1] = 1.;
  psc->domain.length[2] = 1.;

  psc->domain.gdims[0] = 1;
  psc->domain.gdims[1] = 800;
  psc->domain.gdims[2] = 3200;

  psc->domain.bnd_fld_lo[0] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_hi[0] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_lo[1] = BND_FLD_CONDUCTING_WALL;
  psc->domain.bnd_fld_hi[1] = BND_FLD_CONDUCTING_WALL;
  psc->domain.bnd_fld_lo[2] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_hi[2] = BND_FLD_PERIODIC;
 
  psc->domain.bnd_part_lo[0] = BND_PART_PERIODIC;
  psc->domain.bnd_part_hi[0] = BND_PART_PERIODIC;
  psc->domain.bnd_part_lo[1] = BND_PART_REFLECTING;
  psc->domain.bnd_part_hi[1] = BND_PART_REFLECTING;
  psc->domain.bnd_part_lo[2] = BND_PART_PERIODIC;
  psc->domain.bnd_part_hi[2] = BND_PART_PERIODIC;

  // FIXME: can only use 1st order pushers with current conducting wall b.c.
  psc_push_particles_set_type(psc->push_particles, "1vb");
}

// ----------------------------------------------------------------------
// psc_harris_setup

static void
psc_harris_setup(struct psc *psc)
{
  struct psc_harris *harris = to_psc_harris(psc);

  double d_i = sqrt(harris->mi_over_me);
  psc->domain.corner[1] = -.5 * harris->LLy * d_i;
  psc->domain.length[1] = harris->LLy * d_i;
  psc->domain.length[2] = harris->LLz * d_i;
  harris->LLL = harris->lambda * d_i;
  harris->AA = harris->pert * harris->B0 * psc->domain.length[2] / (2. * M_PI);

  psc->kinds[KIND_ELECTRON].m = 1.;
  psc->kinds[KIND_ION     ].m = harris->mi_over_me;
  psc->kinds[KIND_ELECTRON].T = harris->Te;
  psc->kinds[KIND_ELECTRON].T = harris->Ti;

  // initializes fields, particles, etc.
  psc_setup_super(psc);

  MPI_Comm comm = psc_comm(psc);
  mpi_printf(comm, "dt = %g, dy = %g dz = %g\n", psc->dt, psc->dx[1], psc->dx[2]);
  mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., d_i);
  mpi_printf(comm, "v_A = %g\n", harris->B0 / sqrt(harris->mi_over_me));
  mpi_printf(comm, "om_ci = %g om_ce = %g\n", harris->B0 / harris->mi_over_me,
	     harris->B0);
  mpi_printf(comm, "om_pi = %g om_pe = %g\n", 1. / sqrt(harris->mi_over_me), 1.);
  mpi_printf(comm, "lambda_De = %g\n", sqrt(harris->Te));
}

// ----------------------------------------------------------------------
// psc_harris_init_field

static double
psc_harris_init_field(struct psc *psc, double x[3], int m)
{
  struct psc_harris *harris = to_psc_harris(psc);

  double BB = harris->B0;
  double LLz = psc->domain.length[2], LLy = psc->domain.length[1];
  double LLL = harris->LLL;
  double AA = harris->AA;

  switch (m) {
  case HZ:
    return BB * tanh((x[1]) / LLL)
      - AA * M_PI/LLy * cos(2.*M_PI * (x[2] - .5 * LLz) / LLz) * sin(M_PI * x[1] / LLy);

  case HY:
    return AA * 2.*M_PI / LLz * sin(2.*M_PI * (x[2] - .5 * LLz) / LLz) * cos(M_PI * x[1] / LLy);

  case JXI:
    return
      BB / LLL * (1./sqr(cosh(x[1] / LLL)))
      - AA * sqr(M_PI) * (1./sqr(LLy) + 4./sqr(LLz)) 
      * cos(2.*M_PI * (x[2] - .5 *LLz) / LLz) * cos(M_PI * x[1] / LLy);

  default: return 0.;
  }
}

// ----------------------------------------------------------------------
// psc_harris_init_npt
//
// jx = n e (vi - ve) = 1/cosh^2 (2 (TTi + TTe) / BB / LLL

static void
psc_harris_init_npt(struct psc *psc, int pop, double x[3],
		struct psc_particle_npt *npt)
{
  struct psc_harris *harris = to_psc_harris(psc);

  double BB = harris->B0;
  double LLL = harris->LLL;
  double nnb = harris->nb;
  double TTi = harris->Ti;
  double TTe = harris->Te;

  switch (pop) {
  case 0: // ion drifting
    npt->n = 1. / sqr(cosh(x[1] / LLL));
    npt->q = 1.;
    npt->m = harris->mi_over_me;
    npt->p[0] = 2. * TTi / BB / LLL;
    npt->T[0] = TTi;
    npt->T[1] = TTi;
    npt->T[2] = TTi;
    npt->kind = KIND_ION;
    break;
  case 1: // ion bg
    npt->n = nnb;
    npt->q = 1.;
    npt->m = harris->mi_over_me;
    npt->T[0] = TTi;
    npt->T[1] = TTi;
    npt->T[2] = TTi;
    npt->kind = KIND_ION;
    break;
  case 2: // electron drifting
    npt->n = 1. / sqr(cosh(x[1] / LLL));
    npt->q = -1.;
    npt->m = 1.;
    npt->p[0] = -2. * TTe / BB / LLL;
    npt->T[0] = TTe;
    npt->T[1] = TTe;
    npt->T[2] = TTe;
    npt->kind = KIND_ELECTRON;
    break;
  case 3: // electron bg
    npt->n = nnb;
    npt->q = -1.;
    npt->m = 1.;
    npt->T[0] = TTe;
    npt->T[1] = TTe;
    npt->T[2] = TTe;
    npt->kind = KIND_ELECTRON;
    break;
  default:
    assert(0);
  }
}

// ======================================================================
// psc_harris_ops

struct psc_ops psc_harris_ops = {
  .name             = "harris",
  .size             = sizeof(struct psc_harris),
  .param_descr      = psc_harris_descr,
  .create           = psc_harris_create,
  .setup            = psc_harris_setup,
  .init_field       = psc_harris_init_field,
  .init_npt         = psc_harris_init_npt,
};

// ======================================================================
// main

int
main(int argc, char **argv)
{
  return psc_main(&argc, &argv, &psc_harris_ops);
}
