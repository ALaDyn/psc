
#include <psc_inject_private.h>
#include <psc_balance.h>

#include <psc_fields_as_single.h>
#include <fields.hxx>

#include <cuda_iface.h>

#include <stdlib.h>

using Fields = Fields3d<fields_t>;

// ======================================================================
// psc_inject subclass "cuda"

// ----------------------------------------------------------------------
// psc_inject_cuda_create

static void
psc_inject_cuda_create(struct psc_inject *inject)
{
  psc_bnd_set_name(inject->item_n_bnd, "inject_item_n_bnd");
  psc_bnd_set_type(inject->item_n_bnd, "cuda");
  psc_bnd_set_psc(inject->item_n_bnd, ppsc);

  psc_output_fields_item_set_type(inject->item_n, "n_1st_cuda");
  psc_output_fields_item_set_psc_bnd(inject->item_n, inject->item_n_bnd);
}

// ----------------------------------------------------------------------
// get_n_in_cell
//
// helper function for partition / particle setup FIXME duplicated

static inline int
get_n_in_cell(struct psc *psc, struct psc_particle_npt *npt)
{
  if (psc->prm.const_num_particles_per_cell) {
    return psc->prm.nicell;
  }
  if (npt->particles_per_cell) {
    return npt->n * npt->particles_per_cell + .5;
  }
  if (psc->prm.fractional_n_particles_per_cell) {
    int n_prts = npt->n / psc->coeff.cori;
    float rmndr = npt->n / psc->coeff.cori - n_prts;
    float ran = random() / ((float) RAND_MAX + 1);
    if (ran < rmndr) {
      n_prts++;
    }
    return n_prts;
  }
  return npt->n / psc->coeff.cori + .5;
}

// FIXME duplicated

static void
_psc_setup_particle(struct psc *psc, struct cuda_mparticles_prt *cprt,
		    struct psc_particle_npt *npt, int p, double xx[3])
{
  double beta = psc->coeff.beta;

  float ran1, ran2, ran3, ran4, ran5, ran6;
  do {
    ran1 = random() / ((float) RAND_MAX + 1);
    ran2 = random() / ((float) RAND_MAX + 1);
    ran3 = random() / ((float) RAND_MAX + 1);
    ran4 = random() / ((float) RAND_MAX + 1);
    ran5 = random() / ((float) RAND_MAX + 1);
    ran6 = random() / ((float) RAND_MAX + 1);
  } while (ran1 >= 1.f || ran2 >= 1.f || ran3 >= 1.f ||
	   ran4 >= 1.f || ran5 >= 1.f || ran6 >= 1.f);
	      
  double pxi = npt->p[0] +
    sqrtf(-2.f*npt->T[0]/npt->m*sqr(beta)*logf(1.0-ran1)) * cosf(2.f*M_PI*ran2);
  double pyi = npt->p[1] +
    sqrtf(-2.f*npt->T[1]/npt->m*sqr(beta)*logf(1.0-ran3)) * cosf(2.f*M_PI*ran4);
  double pzi = npt->p[2] +
    sqrtf(-2.f*npt->T[2]/npt->m*sqr(beta)*logf(1.0-ran5)) * cosf(2.f*M_PI*ran6);

  if (psc->prm.initial_momentum_gamma_correction) {
    double gam;
    if (sqr(pxi) + sqr(pyi) + sqr(pzi) < 1.) {
      gam = 1. / sqrt(1. - sqr(pxi) - sqr(pyi) - sqr(pzi));
      pxi *= gam;
      pyi *= gam;
      pzi *= gam;
    }
  }
  
  assert(npt->kind >= 0 && npt->kind < psc->nr_kinds);
  assert(npt->q == psc->kinds[npt->kind].q);
  assert(npt->m == psc->kinds[npt->kind].m);

  cprt->xi[0] = xx[0] - psc->patch[p].xb[0];
  cprt->xi[1] = xx[1] - psc->patch[p].xb[1];
  cprt->xi[2] = xx[2] - psc->patch[p].xb[2];
  cprt->pxi[0] = pxi;
  cprt->pxi[1] = pyi;
  cprt->pxi[2] = pzi;
  cprt->kind = npt->kind;
  cprt->qni_wni = psc->kinds[npt->kind].q;
}	      

// ----------------------------------------------------------------------
// psc_inject_cuda_run

void psc_mparticles_cuda_inject(struct psc_mparticles *mprts_base, struct cuda_mparticles_prt *buf,
				uint *buf_n_by_patch); // FIXME

static void
psc_inject_cuda_run(struct psc_inject *inject, struct psc_mparticles *mprts_base,
		    struct psc_mfields *mflds_base)
{
  struct psc *psc = ppsc;

  float fac = 1. / psc->coeff.cori * 
    (inject->every_step * psc->dt / inject->tau) /
    (1. + inject->every_step * psc->dt / inject->tau);

  if (psc_balance_generation_cnt != inject->balance_generation_cnt) {
    inject->balance_generation_cnt = psc_balance_generation_cnt;
    psc_bnd_check_domain(inject->item_n_bnd);
  }
  psc_output_fields_item_run(inject->item_n, mflds_base, mprts_base, inject->mflds_n);

  int kind_n = inject->kind_n;
  
  mfields_t mf_n = inject->mflds_n->get_as<mfields_t>(kind_n, kind_n+1);

  static struct cuda_mparticles_prt *buf;
  static uint buf_n_alloced;
  if (!buf) {
    buf_n_alloced = 1000;
    buf = (struct cuda_mparticles_prt *) calloc(buf_n_alloced, sizeof(*buf));
  }
  uint buf_n_by_patch[psc->nr_patches];

  uint buf_n = 0;
  psc_foreach_patch(psc, p) {
    buf_n_by_patch[p] = 0;
    Fields N(mf_n[p]);
    int *ldims = psc->patch[p].ldims;
    
    int nr_pop = psc->prm.nr_populations;
    for (int jz = 0; jz < ldims[2]; jz++) {
      for (int jy = 0; jy < ldims[1]; jy++) {
	for (int jx = 0; jx < ldims[0]; jx++) {
	  double xx[3] = { .5 * (CRDX(p, jx) + CRDX(p, jx+1)),
			   .5 * (CRDY(p, jy) + CRDY(p, jy+1)),
			   .5 * (CRDZ(p, jz) + CRDZ(p, jz+1)) };
	  // FIXME, the issue really is that (2nd order) particle pushers
	  // don't handle the invariant dim right
	  if (psc->domain.gdims[0] == 1) xx[0] = CRDX(p, jx);
	  if (psc->domain.gdims[1] == 1) xx[1] = CRDY(p, jy);
	  if (psc->domain.gdims[2] == 1) xx[2] = CRDZ(p, jz);

	  if (!psc_target_is_inside(inject->target, xx)) {
	    continue;
	  }

	  int n_q_in_cell = 0;
	  for (int kind = 0; kind < nr_pop; kind++) {
	    struct psc_particle_npt npt = {};
	    if (kind < psc->nr_kinds) {
	      npt.kind = kind;
	      npt.q    = psc->kinds[kind].q;
	      npt.m    = psc->kinds[kind].m;
	      npt.n    = psc->kinds[kind].n;
	      npt.T[0] = psc->kinds[kind].T;
	      npt.T[1] = psc->kinds[kind].T;
	      npt.T[2] = psc->kinds[kind].T;
	    };
	    psc_target_init_npt(inject->target, kind, xx, &npt);
	    
	    int n_in_cell;
	    if (kind != psc->prm.neutralizing_population) {
	      if (psc->timestep >= 0) {
		npt.n -= N(kind_n, jx,jy,jz);
		if (npt.n < 0) {
		  n_in_cell = 0;
		} else {
		  // this rounds down rather than trying to get fractional particles
		  // statistically right...
		  n_in_cell = npt.n *fac;		}
	      } else {
		n_in_cell = get_n_in_cell(psc, &npt);
	      }
	      n_q_in_cell += npt.q * n_in_cell;
	    } else {
	      // FIXME, should handle the case where not the last population is neutralizing
	      assert(psc->prm.neutralizing_population == nr_pop - 1);
	      n_in_cell = -n_q_in_cell / npt.q;
	    }

	    if (buf_n + n_in_cell > buf_n_alloced) {
	      buf_n_alloced = 2 * (buf_n + n_in_cell);
	      buf = (struct cuda_mparticles_prt *) realloc(buf, buf_n_alloced * sizeof(*buf));
	    }
	    for (int cnt = 0; cnt < n_in_cell; cnt++) {
	      _psc_setup_particle(psc, &buf[buf_n + cnt], &npt, p, xx);
	      assert(psc->prm.fractional_n_particles_per_cell);
	    }
	    buf_n += n_in_cell;
	    buf_n_by_patch[p] += n_in_cell;
	  }
	}
      }
    }
  }

  mf_n.put_as(inject->mflds_n, 0, 0);

  psc_mparticles_cuda_inject(mprts_base, buf, buf_n_by_patch);
}

// ----------------------------------------------------------------------
// psc_inject "cuda"

struct psc_inject_ops_cuda : psc_inject_ops {
  psc_inject_ops_cuda() {
    name                = "cuda";
    create              = psc_inject_cuda_create;
    run                 = psc_inject_cuda_run;
  }
} psc_inject_ops_cuda;

