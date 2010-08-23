
#include "psc.h"

#include "util/profile.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef fields_base_real_t creal;

static inline int
nint(creal x)
{
  return (int)(x + 10.5f) - 10;
}

static void
do_c_calc_densities(void)
{
  fields_base_zero(&psc.pf, NE);
  fields_base_zero(&psc.pf, NI);
  fields_base_zero(&psc.pf, NN);
  
  creal fnqs = sqr(psc.coeff.alpha) * psc.coeff.cori / psc.coeff.eta;
  creal dxi = 1.f / psc.dx[0];
  creal dyi = 1.f / psc.dx[1];
  creal dzi = 1.f / psc.dx[2];

  for (int n = 0; n < psc.pp.n_part; n++) {
    particle_base_t *part = particles_base_get_one(&psc.pp, n);

    creal u = part->xi * dxi;
    creal v = part->yi * dyi;
    creal w = part->zi * dzi;
    int j1 = nint(u);
    int j2 = nint(v);
    int j3 = nint(w);
    creal h1 = j1-u;
    creal h2 = j2-v;
    creal h3 = j3-w;

    creal gmx=.5f*(.5f+h1)*(.5f+h1);
    creal gmy=.5f*(.5f+h2)*(.5f+h2);
    creal gmz=.5f*(.5f+h3)*(.5f+h3);
    creal g0x=.75f-h1*h1;
    creal g0y=.75f-h2*h2;
    creal g0z=.75f-h3*h3;
    creal g1x=.5f*(.5f-h1)*(.5f-h1);
    creal g1y=.5f*(.5f-h2)*(.5f-h2);
    creal g1z=.5f*(.5f-h3)*(.5f-h3);

    if (psc.ihi[0] - psc.ilo[0] == 1) {
      j1 = psc.ilo[0]; gmx = 0.; g0x = 1.; g1x = 0.;
    }
    if (psc.ihi[1] - psc.ilo[1] == 1) {
      j2 = psc.ilo[1]; gmy = 0.; g0y = 1.; g1y = 0.;
    }
    if (psc.ihi[2] - psc.ilo[2] == 1) {
      j3 = psc.ilo[2]; gmz = 0.; g0z = 1.; g1z = 0.;
    }

    creal fnq;
    int m;
    if (part->qni < 0.) {
      fnq = part->qni * part->wni * fnqs;
      m = NE;
    } else if (part->qni > 0.) {
      fnq = part->qni * part->wni * fnqs;
      m = NI;
    } else {
      fnq = part->wni * fnqs;
      m = NN;
    }
    F3_BASE(m, j1-1,j2-1,j3-1) += fnq*gmx*gmy*gmz;
    F3_BASE(m, j1  ,j2-1,j3-1) += fnq*g0x*gmy*gmz;
    F3_BASE(m, j1+1,j2-1,j3-1) += fnq*g1x*gmy*gmz;
    F3_BASE(m, j1-1,j2  ,j3-1) += fnq*gmx*g0y*gmz;
    F3_BASE(m, j1  ,j2  ,j3-1) += fnq*g0x*g0y*gmz;
    F3_BASE(m, j1+1,j2  ,j3-1) += fnq*g1x*g0y*gmz;
    F3_BASE(m, j1-1,j2+1,j3-1) += fnq*gmx*g1y*gmz;
    F3_BASE(m, j1  ,j2+1,j3-1) += fnq*g0x*g1y*gmz;
    F3_BASE(m, j1+1,j2+1,j3-1) += fnq*g1x*g1y*gmz;
    F3_BASE(m, j1-1,j2-1,j3  ) += fnq*gmx*gmy*g0z;
    F3_BASE(m, j1  ,j2-1,j3  ) += fnq*g0x*gmy*g0z;
    F3_BASE(m, j1+1,j2-1,j3  ) += fnq*g1x*gmy*g0z;
    F3_BASE(m, j1-1,j2  ,j3  ) += fnq*gmx*g0y*g0z;
    F3_BASE(m, j1  ,j2  ,j3  ) += fnq*g0x*g0y*g0z;
    F3_BASE(m, j1+1,j2  ,j3  ) += fnq*g1x*g0y*g0z;
    F3_BASE(m, j1-1,j2+1,j3  ) += fnq*gmx*g1y*g0z;
    F3_BASE(m, j1  ,j2+1,j3  ) += fnq*g0x*g1y*g0z;
    F3_BASE(m, j1+1,j2+1,j3  ) += fnq*g1x*g1y*g0z;
    F3_BASE(m, j1-1,j2-1,j3+1) += fnq*gmx*gmy*g1z;
    F3_BASE(m, j1  ,j2-1,j3+1) += fnq*g0x*gmy*g1z;
    F3_BASE(m, j1+1,j2-1,j3+1) += fnq*g1x*gmy*g1z;
    F3_BASE(m, j1-1,j2  ,j3+1) += fnq*gmx*g0y*g1z;
    F3_BASE(m, j1  ,j2  ,j3+1) += fnq*g0x*g0y*g1z;
    F3_BASE(m, j1+1,j2  ,j3+1) += fnq*g1x*g0y*g1z;
    F3_BASE(m, j1-1,j2+1,j3+1) += fnq*gmx*g1y*g1z;
    F3_BASE(m, j1  ,j2+1,j3+1) += fnq*g0x*g1y*g1z;
    F3_BASE(m, j1+1,j2+1,j3+1) += fnq*g1x*g1y*g1z;
  }  
}

void
c_calc_densities()
{
  static int pr;
  if (!pr) {
    pr = prof_register("c_densities", 1., 0, psc.pp.n_part * 12 * sizeof(creal));
  }
  prof_start(pr);
  do_c_calc_densities();
  prof_stop(pr);

  psc_add_ghosts(NE, NE + 3);
}

struct psc_moment_ops psc_moment_ops_c = {
  .name                   = "c",
  .calc_densities         = c_calc_densities,
};
