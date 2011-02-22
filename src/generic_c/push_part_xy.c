
#include "psc_generic_c.h"
#include <mrc_profile.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void
do_genc_push_part_xy(fields_t *pf, particles_t *pp)
{
#define S0X(off) s0x[off+2]
#define S0Y(off) s0y[off+2]
#define S1X(off) s1x[off+2]
#define S1Y(off) s1y[off+2]

  creal s0x[5] = {}, s0y[5] = {}, s1x[5], s1y[5];

  creal dt = psc.dt;
  creal xl = .5f * dt;
  creal yl = .5f * dt;
  creal dqs = .5f * psc.coeff.eta * dt;
  creal fnqs = sqr(psc.coeff.alpha) * psc.coeff.cori / psc.coeff.eta;
  creal fnqxs = psc.dx[0] * fnqs / dt;
  creal fnqys = psc.dx[1] * fnqs / dt;
  creal dxi = 1.f / psc.dx[0];
  creal dyi = 1.f / psc.dx[1];

  fields_zero(pf, JXI);
  fields_zero(pf, JYI);
  fields_zero(pf, JZI);
  
  struct psc_patch *patch = &psc.patch[0];
  int n_part = pp->n_part;
  for (int n = 0; n < n_part; n++) {
    particle_t *part = particles_get_one(pp, n);

    // x^n, p^n -> x^(n+.5), p^n

    creal root = 1.f / creal_sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
    creal vxi = part->pxi * root;
    creal vyi = part->pyi * root;
    creal vzi = part->pzi * root;

    part->xi += vxi * xl;
    part->yi += vyi * yl;
    creal u = (part->xi - patch->xb[0]) * dxi;
    creal v = (part->yi - patch->xb[1]) * dyi;
    int j1 = nint(u);
    int j2 = nint(v);
    int j3 = 0;
    creal h1 = j1-u;
    creal h2 = j2-v;

    creal gmx=.5f*(.5f+h1)*(.5f+h1);
    creal gmy=.5f*(.5f+h2)*(.5f+h2);
    creal g0x=.75f-h1*h1;
    creal g0y=.75f-h2*h2;
    creal g1x=.5f*(.5f-h1)*(.5f-h1);
    creal g1y=.5f*(.5f-h2)*(.5f-h2);

    // CHARGE DENSITY FORM FACTOR AT (n+.5)*dt 

    S0X(-1) = .5f*(1.5f-creal_abs(h1-1.f))*(1.5f-creal_abs(h1-1.f));
    S0X(+0) = .75f-creal_abs(h1)*creal_abs(h1);
    S0X(+1) = .5f*(1.5f-creal_abs(h1+1.f))*(1.5f-creal_abs(h1+1.f));
    S0Y(-1) = .5f*(1.5f-creal_abs(h2-1.f))*(1.5f-creal_abs(h2-1.f));
    S0Y(+0) = .75f-creal_abs(h2)*creal_abs(h2);
    S0Y(+1) = .5f*(1.5f-creal_abs(h2+1.f))*(1.5f-creal_abs(h2+1.f));

    u = (part->xi - patch->xb[0]) * dxi - .5f;
    v = (part->yi - patch->xb[1]) * dyi - .5f;
    int l1 = nint(u);
    int l2 = nint(v);
    int l3 = 0;
    h1=l1-u;
    h2=l2-v;

    creal hmx=.5f*(.5f+h1)*(.5f+h1);
    creal hmy=.5f*(.5f+h2)*(.5f+h2);
    creal h0x=.75f-h1*h1;
    creal h0y=.75f-h2*h2;
    creal h1x=.5f*(.5f-h1)*(.5f-h1);
    creal h1y=.5f*(.5f-h2)*(.5f-h2);

    // FIELD INTERPOLATION

    creal exq = (gmy*(hmx*F3(EX, l1-1,j2-1,j3) +
		      h0x*F3(EX, l1  ,j2-1,j3) +
		      h1x*F3(EX, l1+1,j2-1,j3)) +
		 g0y*(hmx*F3(EX, l1-1,j2  ,j3) +
		      h0x*F3(EX, l1  ,j2  ,j3) +
		      h1x*F3(EX, l1+1,j2  ,j3)) +
		 g1y*(hmx*F3(EX, l1-1,j2+1,j3) +
		      h0x*F3(EX, l1  ,j2+1,j3) +
		      h1x*F3(EX, l1+1,j2+1,j3)));

    creal eyq = (hmy*(gmx*F3(EY, j1-1,l2-1,j3) +
		      g0x*F3(EY, j1  ,l2-1,j3) +
		      g1x*F3(EY, j1+1,l2-1,j3)) +
		 h0y*(gmx*F3(EY, j1-1,l2  ,j3) +
		      g0x*F3(EY, j1  ,l2  ,j3) +
		      g1x*F3(EY, j1+1,l2  ,j3)) +
		 h1y*(gmx*F3(EY, j1-1,l2+1,j3) +
		      g0x*F3(EY, j1  ,l2+1,j3) +
		      g1x*F3(EY, j1+1,l2+1,j3)));

    creal ezq = (gmy*(gmx*F3(EZ, j1-1,j2-1,l3) +
		      g0x*F3(EZ, j1  ,j2-1,l3) +
		      g1x*F3(EZ, j1+1,j2-1,l3)) +
		 g0y*(gmx*F3(EZ, j1-1,j2  ,l3) +
		      g0x*F3(EZ, j1  ,j2  ,l3) +
		      g1x*F3(EZ, j1+1,j2  ,l3)) +
		 g1y*(gmx*F3(EZ, j1-1,j2+1,l3) +
		      g0x*F3(EZ, j1  ,j2+1,l3) +
		      g1x*F3(EZ, j1+1,j2+1,l3)));

    creal hxq = (hmy*(gmx*F3(HX, j1-1,l2-1,l3) +
		      g0x*F3(HX, j1  ,l2-1,l3) +
		      g1x*F3(HX, j1+1,l2-1,l3)) +
		 h0y*(gmx*F3(HX, j1-1,l2  ,l3) +
		      g0x*F3(HX, j1  ,l2  ,l3) +
		      g1x*F3(HX, j1+1,l2  ,l3)) +
		 h1y*(gmx*F3(HX, j1-1,l2+1,l3) +
		      g0x*F3(HX, j1  ,l2+1,l3) +
		      g1x*F3(HX, j1+1,l2+1,l3)));

    creal hyq = (gmy*(hmx*F3(HY, l1-1,j2-1,l3) +
		      h0x*F3(HY, l1  ,j2-1,l3) +
		      h1x*F3(HY, l1+1,j2-1,l3)) +
		 g0y*(hmx*F3(HY, l1-1,j2  ,l3) +
		      h0x*F3(HY, l1  ,j2  ,l3) +
		      h1x*F3(HY, l1+1,j2  ,l3)) +
		 g1y*(hmx*F3(HY, l1-1,j2+1,l3) +
		      h0x*F3(HY, l1  ,j2+1,l3) +
		      h1x*F3(HY, l1+1,j2+1,l3)));

    creal hzq = (hmy*(hmx*F3(HZ, l1-1,l2-1,j3) +
		      h0x*F3(HZ, l1  ,l2-1,j3) +
		      h1x*F3(HZ, l1+1,l2-1,j3)) +
		 h0y*(hmx*F3(HZ, l1-1,l2  ,j3) +
		      h0x*F3(HZ, l1  ,l2  ,j3) +
		      h1x*F3(HZ, l1+1,l2  ,j3)) +
		 h1y*(hmx*F3(HZ, l1-1,l2+1,j3) +
		      h0x*F3(HZ, l1  ,l2+1,j3) +
		      h1x*F3(HZ, l1+1,l2+1,j3)));
		 
     // c x^(n+.5), p^n -> x^(n+1.0), p^(n+1.0) 

    creal dq = dqs * part->qni / part->mni;
    creal pxm = part->pxi + dq*exq;
    creal pym = part->pyi + dq*eyq;
    creal pzm = part->pzi + dq*ezq;

    root = dq / creal_sqrt(1.f + pxm*pxm + pym*pym + pzm*pzm);
    creal taux = hxq*root;
    creal tauy = hyq*root;
    creal tauz = hzq*root;

    creal tau = 1.f / (1.f + taux*taux + tauy*tauy + tauz*tauz);
    creal pxp = ((1.f+taux*taux-tauy*tauy-tauz*tauz)*pxm + 
		(2.f*taux*tauy+2.f*tauz)*pym + 
		(2.f*taux*tauz-2.f*tauy)*pzm)*tau;
    creal pyp = ((2.f*taux*tauy-2.f*tauz)*pxm +
		(1.f-taux*taux+tauy*tauy-tauz*tauz)*pym +
		(2.f*tauy*tauz+2.f*taux)*pzm)*tau;
    creal pzp = ((2.f*taux*tauz+2.f*tauy)*pxm +
		(2.f*tauy*tauz-2.f*taux)*pym +
		(1.f-taux*taux-tauy*tauy+tauz*tauz)*pzm)*tau;
    
    part->pxi = pxp + dq * exq;
    part->pyi = pyp + dq * eyq;
    part->pzi = pzp + dq * ezq;

    root = 1.f / creal_sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
    vxi = part->pxi * root;
    vyi = part->pyi * root;
    vzi = part->pzi * root;

    part->xi += vxi * xl;
    part->yi += vyi * yl;

    // CHARGE DENSITY FORM FACTOR AT (n+1.5)*dt 
    // x^(n+1), p^(n+1) -> x^(n+1.5f), p^(n+1)

    creal xi = part->xi + vxi * xl;
    creal yi = part->yi + vyi * yl;

    u = (xi - patch->xb[0]) * dxi;
    v = (yi - patch->xb[1]) * dyi;
    int k1 = nint(u);
    int k2 = nint(v);
    h1 = k1 - u;
    h2 = k2 - v;

    for (int i = -2; i <= 2; i++) {
      S1X(i) = 0.f;
      S1Y(i) = 0.f;
    }

    S1X(k1-j1-1) = .5f*(1.5f-creal_abs(h1-1.f))*(1.5f-creal_abs(h1-1.f));
    S1X(k1-j1+0) = .75f-creal_abs(h1)*creal_abs(h1);
    S1X(k1-j1+1) = .5f*(1.5f-creal_abs(h1+1.f))*(1.5f-creal_abs(h1+1.f));
    S1Y(k2-j2-1) = .5f*(1.5f-creal_abs(h2-1.f))*(1.5f-creal_abs(h2-1.f));
    S1Y(k2-j2+0) = .75f-creal_abs(h2)*creal_abs(h2);
    S1Y(k2-j2+1) = .5f*(1.5f-creal_abs(h2+1.f))*(1.5f-creal_abs(h2+1.f));

    // CURRENT DENSITY AT (n+1.0)*dt

    for (int i = -1; i <= 1; i++) {
      S1X(i) -= S0X(i);
      S1Y(i) -= S0Y(i);
    }

    int l1min, l2min, l1max, l2max;
    
    if (k1 == j1) {
      l1min = -1; l1max = +1;
    } else if (k1 == j1 - 1) {
      l1min = -2; l1max = +1;
    } else { // (k1 == j1 + 1)
      l1min = -1; l1max = +2;
    }

    if (k2 == j2) {
      l2min = -1; l2max = +1;
    } else if (k2 == j2 - 1) {
      l2min = -2; l2max = +1;
    } else { // (k2 == j2 + 1)
      l2min = -1; l2max = +2;
    }

    creal fnqx = part->qni * part->wni * fnqxs;
    creal fnqy = part->qni * part->wni * fnqys;
    creal fnqz = vzi * part->qni * part->wni * fnqs;
    for (int l2 = l2min; l2 <= l2max; l2++) {
      creal jxh = 0.f;
      for (int l1 = l1min; l1 <= l1max; l1++) {
	creal wx = S1X(l1) * (S0Y(l2) + .5f*S1Y(l2));
	creal wz = S0X(l1) * S0Y(l2)
	  + .5f * S1X(l1) * S0Y(l2)
	  + .5f * S0X(l1) * S1Y(l2)
	  + (1.f/3.f) * S1X(l1) * S1Y(l2);

	jxh -= fnqx*wx;
	F3(JXI, j1+l1,j2+l2,j3) += jxh;
	F3(JZI, j1+l1,j2+l3,j3) += fnqz * wz;
      }
    }
    for (int l1 = l1min; l1 <= l1max; l1++) {
      creal jyh = 0.f;
      for (int l2 = l2min; l2 <= l2max; l2++) {
	creal wy = S1Y(l2) * (S0X(l1) + .5f*S1X(l1));

	jyh -= fnqy*wy;
	F3(JYI, j1+l1,j2+l2,j3) += jyh;
      }
    }
  }
}

void
genc_push_part_xy(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  fields_t pf;
  particles_t pp;
  fields_get(&pf, EX, EX + 6, flds_base);
  particles_get(&pp, particles_base);

  static int pr;
  if (!pr) {
    pr = prof_register("genc_part_xy", 1., 0, 0);
  }
  prof_start(pr);
  do_genc_push_part_xy(&pf, &pp);
  prof_stop(pr);

  fields_put(&pf, JXI, JXI + 3, flds_base);
  particles_put(&pp, particles_base);
}

