
#include "psc_generic_c.h"

#include <mrc_profile.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void
do_genc_push_part_yz(fields_t *pf, particles_t *pp)
{
#define S0Y(off) s0y[off+2]
#define S0Z(off) s0z[off+2]
#define S1Y(off) s1y[off+2]
#define S1Z(off) s1z[off+2]

  creal s0y[5] = {}, s0z[5] = {}, s1y[5], s1z[5];

  creal dt = psc.dt;
  creal yl = .5f * dt;
  creal zl = .5f * dt;
  creal dqs = .5f * psc.coeff.eta * dt;
  creal fnqs = sqr(psc.coeff.alpha) * psc.coeff.cori / psc.coeff.eta;
  creal fnqys = psc.dx[1] * fnqs / dt;
  creal fnqzs = psc.dx[2] * fnqs / dt;
  creal dxi = 1.f / psc.dx[0];
  creal dyi = 1.f / psc.dx[1];
  creal dzi = 1.f / psc.dx[2];

  fields_zero(pf, JXI);
  fields_zero(pf, JYI);
  fields_zero(pf, JZI);
  
  for (int n = 0; n < psc.pp.n_part; n++) {
    particle_t *part = particles_get_one(pp, n);

    // x^n, p^n -> x^(n+.5), p^n

    creal root = 1.f / creal_sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
    creal vxi = part->pxi * root;
    creal vyi = part->pyi * root;
    creal vzi = part->pzi * root;

    part->yi += vyi * yl;
    part->zi += vzi * zl;

    creal u = part->xi * dxi;
    creal v = part->yi * dyi;
    creal w = part->zi * dzi;
    int j1 = nint(u);
    int j2 = nint(v);
    int j3 = nint(w);
    creal h1 = j1-u;
    creal h2 = j2-v;
    creal h3 = j3-w;

    creal gmy=.5f*(.5f+h2)*(.5f+h2);
    creal gmz=.5f*(.5f+h3)*(.5f+h3);
    creal g0y=.75f-h2*h2;
    creal g0z=.75f-h3*h3;
    creal g1y=.5f*(.5f-h2)*(.5f-h2);
    creal g1z=.5f*(.5f-h3)*(.5f-h3);

    // CHARGE DENSITY FORM FACTOR AT (n+.5)*dt 

    S0Y(-1) = .5f*(1.5f-creal_abs(h2-1.f))*(1.5f-creal_abs(h2-1.f));
    S0Y(+0) = .75f-creal_abs(h2)*creal_abs(h2);
    S0Y(+1) = .5f*(1.5f-creal_abs(h2+1.f))*(1.5f-creal_abs(h2+1.f));
    S0Z(-1) = .5f*(1.5f-creal_abs(h3-1.f))*(1.5f-creal_abs(h3-1.f));
    S0Z(+0) = .75f-creal_abs(h3)*creal_abs(h3);
    S0Z(+1) = .5f*(1.5f-creal_abs(h3+1.f))*(1.5f-creal_abs(h3+1.f));

    u = part->xi*dxi;
    v = part->yi*dyi-.5f;
    w = part->zi*dzi-.5f;
    int l1=nint(u);
    int l2=nint(v);
    int l3=nint(w);
    h1=l1-u;
    h2=l2-v;
    h3=l3-w;
    creal hmy=.5f*(.5f+h2)*(.5f+h2);
    creal hmz=.5f*(.5f+h3)*(.5f+h3);
    creal h0y=.75f-h2*h2;
    creal h0z=.75f-h3*h3;
    creal h1y=.5f*(.5f-h2)*(.5f-h2);
    creal h1z=.5f*(.5f-h3)*(.5f-h3);

    // FIELD INTERPOLATION

    creal exq=gmz*(gmy*F3(EX, l1,j2-1,j3-1)
		  +g0y*F3(EX, l1,j2  ,j3-1)
		  +g1y*F3(EX, l1,j2+1,j3-1))
             +g0z*(gmy*F3(EX, l1,j2-1,j3  )
    	          +g0y*F3(EX, l1,j2  ,j3  )
	          +g1y*F3(EX, l1,j2+1,j3  ))
             +g1z*(gmy*F3(EX, l1,j2-1,j3+1)
	          +g0y*F3(EX, l1,j2  ,j3+1)
	          +g1y*F3(EX, l1,j2+1,j3+1));

    creal eyq=gmz*(hmy*F3(EY, j1,l2-1,j3-1)
                  +h0y*F3(EY, j1,l2  ,j3-1)
                  +h1y*F3(EY, j1,l2+1,j3-1))
             +g0z*(hmy*F3(EY, j1,l2-1,j3  )
                  +h0y*F3(EY, j1,l2  ,j3  )
                  +h1y*F3(EY, j1,l2+1,j3  ))
             +g1z*(hmy*F3(EY, j1,l2-1,j3+1)
                  +h0y*F3(EY, j1,l2  ,j3+1)
	          +h1y*F3(EY, j1,l2+1,j3+1));

    creal ezq=hmz*(gmy*F3(EZ, j1,j2-1,l3-1)
		  +g0y*F3(EZ, j1,j2  ,l3-1)
                  +g1y*F3(EZ, j1,j2+1,l3-1))
             +h0z*(gmy*F3(EZ, j1,j2-1,l3  )
                  +g0y*F3(EZ, j1,j2  ,l3  )
                  +g1y*F3(EZ, j1,j2+1,l3  ))
             +h1z*(gmy*F3(EZ, j1,j2-1,l3+1)
                  +g0y*F3(EZ, j1,j2  ,l3+1)
		  +g1y*F3(EZ, j1,j2+1,l3+1));

    creal hxq=hmz*(hmy*F3(HX, j1,l2-1,l3-1)
                  +h0y*F3(HX, j1,l2  ,l3-1)
                  +h1y*F3(HX, j1,l2+1,l3-1))
             +h0z*(hmy*F3(HX, j1,l2-1,l3  )
                  +h0y*F3(HX, j1,l2  ,l3  )
                  +h1y*F3(HX, j1,l2+1,l3  ))
             +h1z*(hmy*F3(HX, j1,l2-1,l3+1)
                  +h0y*F3(HX, j1,l2  ,l3+1)
		  +h1y*F3(HX, j1,l2+1,l3+1));

    creal hyq=hmz*(gmy*F3(HY, l1,j2-1,l3-1)
                  +g0y*F3(HY, l1,j2  ,l3-1)
                  +g1y*F3(HY, l1,j2+1,l3-1))
             +h0z*(gmy*F3(HY, l1,j2-1,l3  )
                  +g0y*F3(HY, l1,j2  ,l3  )
                  +g1y*F3(HY, l1,j2+1,l3  ))
             +h1z*(gmy*F3(HY, l1,j2-1,l3+1)
                  +g0y*F3(HY, l1,j2  ,l3+1)
	          +g1y*F3(HY, l1,j2+1,l3+1));

    creal hzq=gmz*(hmy*F3(HZ, l1,l2-1,j3-1)
		  +h0y*F3(HZ, l1,l2  ,j3-1)
                  +h1y*F3(HZ, l1,l2+1,j3-1))
             +g0z*(hmy*F3(HZ, l1,l2-1,j3  )
                  +h0y*F3(HZ, l1,l2  ,j3  )
                  +h1y*F3(HZ, l1,l2+1,j3  ))
             +g1z*(hmy*F3(HZ, l1,l2-1,j3+1)
                  +h0y*F3(HZ, l1,l2  ,j3+1)
		  +h1y*F3(HZ, l1,l2+1,j3+1));

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

    part->yi += vyi * yl;
    part->zi += vzi * zl;

    // CHARGE DENSITY FORM FACTOR AT (n+1.5)*dt 
    // x^(n+1), p^(n+1) -> x^(n+1.5f), p^(n+1)

    creal yi = part->yi + vyi * yl;
    creal zi = part->zi + vzi * zl;

    v = yi * dyi;
    w = zi * dzi;
    int k2 = nint(v);
    int k3 = nint(w);
    h2 = k2 - v;
    h3 = k3 - w;

    for (int i = -2; i <= 2; i++) {
      S1Y(i) = 0.f;
      S1Z(i) = 0.f;
    }

    S1Y(k2-j2-1) = .5f*(1.5f-creal_abs(h2-1.f))*(1.5f-creal_abs(h2-1.f));
    S1Y(k2-j2+0) = .75f-creal_abs(h2)*creal_abs(h2);
    S1Y(k2-j2+1) = .5f*(1.5f-creal_abs(h2+1.f))*(1.5f-creal_abs(h2+1.f));
    S1Z(k3-j3-1) = .5f*(1.5f-creal_abs(h3-1.f))*(1.5f-creal_abs(h3-1.f));
    S1Z(k3-j3+0) = .75f-creal_abs(h3)*creal_abs(h3);
    S1Z(k3-j3+1) = .5f*(1.5f-creal_abs(h3+1.f))*(1.5f-creal_abs(h3+1.f));

    // CURRENT DENSITY AT (n+1.0)*dt

    for (int i = -1; i <= 1; i++) {
      S1Y(i) -= S0Y(i);
      S1Z(i) -= S0Z(i);
    }

    int l2min, l3min, l2max, l3max;
    
    if (k2 == j2) {
      l2min = -1; l2max = +1;
    } else if (k2 == j2 - 1) {
      l2min = -2; l2max = +1;
    } else { // (k2 == j2 + 1)
      l2min = -1; l2max = +2;
    }

    if (k3 == j3) {
      l3min = -1; l3max = +1;
    } else if (k3 == j3 - 1) {
      l3min = -2; l3max = +1;
    } else { // (k3 == j3 + 1)
      l3min = -1; l3max = +2;
    }

    creal jxh;
    creal jyh;
    creal jzh[5];

#define JZH(i) jzh[i+2]

    creal fnqx = vxi * part->qni * part->wni * fnqs;
    creal fnqy = part->qni * part->wni * fnqys;
    creal fnqz = part->qni * part->wni * fnqzs;
    for (int l2 = l2min; l2 <= l2max; l2++) {
      JZH(l2) = 0.f;
    }
    for (int l3 = l3min; l3 <= l3max; l3++) {
      jyh = 0.f;
      for (int l2 = l2min; l2 <= l2max; l2++) {
	creal wx = S0Y(l2) * S0Z(l3)
	  + .5f * S1Y(l2) * S0Z(l3)
	  + .5f * S0Y(l2) * S1Z(l3)
	  + (1.f/3.f) * S1Y(l2) * S1Z(l3);
	creal wy = S1Y(l2) * (S0Z(l3) + .5f*S1Z(l3));
	creal wz = S1Z(l3) * (S0Y(l2) + .5f*S1Y(l2));

	jxh = fnqx*wx;
	jyh -= fnqy*wy;
	JZH(l2) -= fnqz*wz;

	F3(JXI, j1,j2+l2,j3+l3) += jxh;
	F3(JYI, j1,j2+l2,j3+l3) += jyh;
	F3(JZI, j1,j2+l2,j3+l3) += JZH(l2);
      }
    }
  }
}

void
genc_push_part_yz()
{
  fields_t pf;
  particles_t pp;
  fields_get(&pf, EX, EX + 6);
  particles_get(&pp);

  static int pr;
  if (!pr) {
    pr = prof_register("genc_part_yz", 1., 0, psc.pp.n_part * 12 * sizeof(creal));
  }
  prof_start(pr);
  do_genc_push_part_yz(&pf, &pp);
  prof_stop(pr);

  fields_put(&pf, JXI, JXI + 3);
  particles_put(&pp);
}

