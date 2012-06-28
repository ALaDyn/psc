
#include "psc_output_fields_item_private.h"
#include "psc_particles_as_c.h"
#include "psc_fields_as_c.h"

#include "psc_bnd.h"
#include <math.h>

// ======================================================================

typedef fields_c_real_t creal;

static void
do_n_2nd_nc_run(int p, fields_t *pf, struct psc_particles *prts, int m_NE, int m_NI, int m_NN)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0];
  creal dyi = 1.f / ppsc->dx[1];
  creal dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < prts->n_part; n++) {
    particle_t *part = particles_get_one(prts, n);
      
    creal u = (part->xi - patch->xb[0]) * dxi;
    creal v = (part->yi - patch->xb[1]) * dyi;
    creal w = (part->zi - patch->xb[2]) * dzi;
    int j1 = particle_real_nint(u);
    int j2 = particle_real_nint(v);
    int j3 = particle_real_nint(w);
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
      
    if (ppsc->domain.gdims[0] == 1) {
      j1 = 0; gmx = 0.; g0x = 1.; g1x = 0.;
    }
    if (ppsc->domain.gdims[1] == 1) {
      j2 = 0; gmy = 0.; g0y = 1.; g1y = 0.;
    }
    if (ppsc->domain.gdims[2] == 1) {
      j3 = 0; gmz = 0.; g0z = 1.; g1z = 0.;
    }

    creal fnq;
    int m;
    if (part->qni < 0.) {
      fnq = part->qni * part->wni * fnqs;
      m = m_NE;
    } else if (part->qni > 0.) {
      fnq = part->qni * part->wni * fnqs;
      m = m_NI;
    } else {
      fnq = part->wni * fnqs;
      m = m_NN;
    }
    F3(pf, m, j1-1,j2-1,j3-1) += fnq*gmx*gmy*gmz;
    F3(pf, m, j1  ,j2-1,j3-1) += fnq*g0x*gmy*gmz;
    F3(pf, m, j1+1,j2-1,j3-1) += fnq*g1x*gmy*gmz;
    F3(pf, m, j1-1,j2  ,j3-1) += fnq*gmx*g0y*gmz;
    F3(pf, m, j1  ,j2  ,j3-1) += fnq*g0x*g0y*gmz;
    F3(pf, m, j1+1,j2  ,j3-1) += fnq*g1x*g0y*gmz;
    F3(pf, m, j1-1,j2+1,j3-1) += fnq*gmx*g1y*gmz;
    F3(pf, m, j1  ,j2+1,j3-1) += fnq*g0x*g1y*gmz;
    F3(pf, m, j1+1,j2+1,j3-1) += fnq*g1x*g1y*gmz;
    F3(pf, m, j1-1,j2-1,j3  ) += fnq*gmx*gmy*g0z;
    F3(pf, m, j1  ,j2-1,j3  ) += fnq*g0x*gmy*g0z;
    F3(pf, m, j1+1,j2-1,j3  ) += fnq*g1x*gmy*g0z;
    F3(pf, m, j1-1,j2  ,j3  ) += fnq*gmx*g0y*g0z;
    F3(pf, m, j1  ,j2  ,j3  ) += fnq*g0x*g0y*g0z;
    F3(pf, m, j1+1,j2  ,j3  ) += fnq*g1x*g0y*g0z;
    F3(pf, m, j1-1,j2+1,j3  ) += fnq*gmx*g1y*g0z;
    F3(pf, m, j1  ,j2+1,j3  ) += fnq*g0x*g1y*g0z;
    F3(pf, m, j1+1,j2+1,j3  ) += fnq*g1x*g1y*g0z;
    F3(pf, m, j1-1,j2-1,j3+1) += fnq*gmx*gmy*g1z;
    F3(pf, m, j1  ,j2-1,j3+1) += fnq*g0x*gmy*g1z;
    F3(pf, m, j1+1,j2-1,j3+1) += fnq*g1x*gmy*g1z;
    F3(pf, m, j1-1,j2  ,j3+1) += fnq*gmx*g0y*g1z;
    F3(pf, m, j1  ,j2  ,j3+1) += fnq*g0x*g0y*g1z;
    F3(pf, m, j1+1,j2  ,j3+1) += fnq*g1x*g0y*g1z;
    F3(pf, m, j1-1,j2+1,j3+1) += fnq*gmx*g1y*g1z;
    F3(pf, m, j1  ,j2+1,j3+1) += fnq*g0x*g1y*g1z;
    F3(pf, m, j1+1,j2+1,j3+1) += fnq*g1x*g1y*g1z;
  }
}

static void
n_2nd_nc_run(struct psc_output_fields_item *item, mfields_base_t *flds,
	     mparticles_base_t *particles_base, mfields_c_t *res)
{
  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  psc_mfields_zero(res, 0);
  psc_mfields_zero(res, 1);
  psc_mfields_zero(res, 2);
  
  psc_foreach_patch(ppsc, p) {
    do_n_2nd_nc_run(p, psc_mfields_get_patch(res, p),
		    psc_mparticles_get_patch(particles, p), 0, 1, 2);
  }

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(item->bnd, res, 0, res->nr_fields);
}

// FIXME too much duplication, specialize 2d/1d

static void
do_v_2nd_nc_run(int p, fields_t *pf, struct psc_particles *prts)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0];
  creal dyi = 1.f / ppsc->dx[1];
  creal dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < prts->n_part; n++) {
    particle_t *part = particles_get_one(prts, n);

    creal u = (part->xi - patch->xb[0]) * dxi;
    creal v = (part->yi - patch->xb[1]) * dyi;
    creal w = (part->zi - patch->xb[2]) * dzi;
    int j1 = particle_real_nint(u);
    int j2 = particle_real_nint(v);
    int j3 = particle_real_nint(w);
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

    if (ppsc->domain.gdims[0] == 1) {
      j1 = 0; gmx = 0.; g0x = 1.; g1x = 0.;
    }
    if (ppsc->domain.gdims[1] == 1) {
      j2 = 0; gmy = 0.; g0y = 1.; g1y = 0.;
    }
    if (ppsc->domain.gdims[2] == 1) {
      j3 = 0; gmz = 0.; g0z = 1.; g1z = 0.;
    }
    
    creal pxi = part->pxi;
    creal pyi = part->pyi;
    creal pzi = part->pzi;
    creal root = 1.0/sqrt(1.0+pxi*pxi+pyi*pyi+pzi*pzi);
    creal vv[3] = { pxi*root, pyi*root, pzi*root };
    creal fnq = part->wni * fnqs;
    int mm;
    if (part->qni < 0.) {
      mm = 0; // electrons
    } else if (part->qni > 0.) {
      mm = 3; // ions
    } else {
      assert(0);
    }
    for (int m = 0; m < 3; m++) {
      F3(pf, mm+m, j1-1,j2-1,j3-1) += fnq*gmx*gmy*gmz * vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3-1) += fnq*g0x*gmy*gmz * vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3-1) += fnq*g1x*gmy*gmz * vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3-1) += fnq*gmx*g0y*gmz * vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3-1) += fnq*g0x*g0y*gmz * vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3-1) += fnq*g1x*g0y*gmz * vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3-1) += fnq*gmx*g1y*gmz * vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3-1) += fnq*g0x*g1y*gmz * vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3-1) += fnq*g1x*g1y*gmz * vv[m];
      F3(pf, mm+m, j1-1,j2-1,j3  ) += fnq*gmx*gmy*g0z * vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3  ) += fnq*g0x*gmy*g0z * vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3  ) += fnq*g1x*gmy*g0z * vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3  ) += fnq*gmx*g0y*g0z * vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3  ) += fnq*g0x*g0y*g0z * vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3  ) += fnq*g1x*g0y*g0z * vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3  ) += fnq*gmx*g1y*g0z * vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3  ) += fnq*g0x*g1y*g0z * vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3  ) += fnq*g1x*g1y*g0z * vv[m];
      F3(pf, mm+m, j1-1,j2-1,j3+1) += fnq*gmx*gmy*g1z * vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3+1) += fnq*g0x*gmy*g1z * vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3+1) += fnq*g1x*gmy*g1z * vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3+1) += fnq*gmx*g0y*g1z * vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3+1) += fnq*g0x*g0y*g1z * vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3+1) += fnq*g1x*g0y*g1z * vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3+1) += fnq*gmx*g1y*g1z * vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3+1) += fnq*g0x*g1y*g1z * vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3+1) += fnq*g1x*g1y*g1z * vv[m];
    }  
  }
}

static void
v_2nd_nc_run(struct psc_output_fields_item *item, mfields_base_t *flds,
	     mparticles_base_t *particles_base, mfields_c_t *res)
{
  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  for (int m = 0; m < 6; m++) {
    psc_mfields_zero(res, m);
  }
  
  psc_foreach_patch(ppsc, p) {
    do_v_2nd_nc_run(p, psc_mfields_get_patch(res, p),
		    psc_mparticles_get_patch(particles, p));
  }

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(item->bnd, res, 0, res->nr_fields);
}

static void
do_vv_2nd_nc_run(int p, fields_t *pf, struct psc_particles *prts)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0];
  creal dyi = 1.f / ppsc->dx[1];
  creal dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < prts->n_part; n++) {
    particle_t *part = particles_get_one(prts, n);

    creal u = (part->xi - patch->xb[0]) * dxi;
    creal v = (part->yi - patch->xb[1]) * dyi;
    creal w = (part->zi - patch->xb[2]) * dzi;
    int j1 = particle_real_nint(u);
    int j2 = particle_real_nint(v);
    int j3 = particle_real_nint(w);
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

    if (ppsc->domain.gdims[0] == 1) {
      j1 = 0; gmx = 0.; g0x = 1.; g1x = 0.;
    }
    if (ppsc->domain.gdims[1] == 1) {
      j2 = 0; gmy = 0.; g0y = 1.; g1y = 0.;
    }
    if (ppsc->domain.gdims[2] == 1) {
      j3 = 0; gmz = 0.; g0z = 1.; g1z = 0.;
    }
    
    creal pxi = part->pxi;
    creal pyi = part->pyi;
    creal pzi = part->pzi;
    creal root = 1.0/sqrt(1.0+pxi*pxi+pyi*pyi+pzi*pzi);
    creal vv[3] = { pxi*root, pyi*root, pzi*root };
    creal fnq = part->wni * fnqs;
    int mm;
    if (part->qni < 0.) {
      mm = 0; // electrons
    } else if (part->qni > 0.) {
      mm = 3; // ions
    } else {
      assert(0);
    }
    for (int m = 0; m < 3; m++) {
      F3(pf, mm+m, j1-1,j2-1,j3-1) += fnq*gmx*gmy*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3-1) += fnq*g0x*gmy*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3-1) += fnq*g1x*gmy*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3-1) += fnq*gmx*g0y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3-1) += fnq*g0x*g0y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3-1) += fnq*g1x*g0y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3-1) += fnq*gmx*g1y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3-1) += fnq*g0x*g1y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3-1) += fnq*g1x*g1y*gmz * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2-1,j3  ) += fnq*gmx*gmy*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3  ) += fnq*g0x*gmy*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3  ) += fnq*g1x*gmy*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3  ) += fnq*gmx*g0y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3  ) += fnq*g0x*g0y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3  ) += fnq*g1x*g0y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3  ) += fnq*gmx*g1y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3  ) += fnq*g0x*g1y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3  ) += fnq*g1x*g1y*g0z * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2-1,j3+1) += fnq*gmx*gmy*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2-1,j3+1) += fnq*g0x*gmy*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2-1,j3+1) += fnq*g1x*gmy*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2  ,j3+1) += fnq*gmx*g0y*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2  ,j3+1) += fnq*g0x*g0y*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2  ,j3+1) += fnq*g1x*g0y*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1-1,j2+1,j3+1) += fnq*gmx*g1y*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1  ,j2+1,j3+1) += fnq*g0x*g1y*g1z * vv[m]*vv[m];
      F3(pf, mm+m, j1+1,j2+1,j3+1) += fnq*g1x*g1y*g1z * vv[m]*vv[m];
    }  
  }
}

static void
vv_2nd_nc_run(struct psc_output_fields_item *item, mfields_base_t *flds,
	      mparticles_base_t *particles_base, mfields_c_t *res)
{
  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  for (int m = 0; m < 6; m++) {
    psc_mfields_zero(res, m);
  }
  
  psc_foreach_patch(ppsc, p) {
    do_vv_2nd_nc_run(p, psc_mfields_get_patch(res, p),
		     psc_mparticles_get_patch(particles, p));
  }

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(item->bnd, res, 0, res->nr_fields);
}

// ======================================================================
// psc_output_fields_item: subclass "n_2nd_nc"

struct psc_output_fields_item_ops psc_output_fields_item_n_2nd_nc_ops = {
  .name               = "n",
  .nr_comp            = 3,
  .fld_names          = { "ne", "ni", "nn" },
  .run                = n_2nd_nc_run,
};

// ======================================================================
// psc_output_fields_item: subclass "v_2nd_nc"

struct psc_output_fields_item_ops psc_output_fields_item_v_2nd_nc_ops = {
  .name               = "v",
  .nr_comp            = 6,
  .fld_names          = { "vex", "vey", "vez",
			  "vix", "viy", "viz" },
  .run                = v_2nd_nc_run,
};

// ======================================================================
// psc_output_fields_item: subclass "vv_2nd_nc"

struct psc_output_fields_item_ops psc_output_fields_item_vv_2nd_nc_ops = {
  .name               = "vv",
  .nr_comp            = 6,
  .fld_names          = { "vexvex", "veyvey", "vezvez",
			  "vixvix", "viyviy", "vizviz" },
  .run                = vv_2nd_nc_run,
};

