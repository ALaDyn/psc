
#include "psc_bnd_private.h"
#include "ddc_particles.h"

#include <mrc_domain.h>
#include <mrc_ddc.h>
#include <mrc_profile.h>

struct psc_bnd_c {
  struct mrc_ddc *ddc;
  struct ddc_particles *ddcp;
};

#define to_psc_bnd_c(bnd) ((struct psc_bnd_c *)((bnd)->obj.subctx))

// ======================================================================
// ddc funcs

static void
copy_to_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_base_t *flds = ctx;
  fields_base_t *pf = &flds->f[p];
  fields_base_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  MRC_DDC_BUF3(buf, m - mb, ix,iy,iz) = F3_BASE(pf, m, ix,iy,iz);
	}
      }
    }
  }
}

static void
add_from_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_base_t *flds = ctx;
  fields_base_t *pf = &flds->f[p];
  fields_base_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  F3_BASE(pf, m, ix,iy,iz) += MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	}
      }
    }
  }
}

static void
copy_from_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_base_t *flds = ctx;
  fields_base_t *pf = &flds->f[p];
  fields_base_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  F3_BASE(pf, m, ix,iy,iz) = MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	}
      }
    }
  }
}

struct mrc_ddc_funcs ddc_funcs = {
  .copy_to_buf   = copy_to_buf,
  .copy_from_buf = copy_from_buf,
  .add_from_buf  = add_from_buf,
};

// ----------------------------------------------------------------------
// psc_bnd_c_setup

static void
psc_bnd_c_setup(struct psc_bnd *bnd)
{
  struct psc_bnd_c *bnd_c = to_psc_bnd_c(bnd);

  bnd_c->ddc = mrc_domain_create_ddc(psc.mrc_domain);
  mrc_ddc_set_funcs(bnd_c->ddc, &ddc_funcs);
  mrc_ddc_set_param_int3(bnd_c->ddc, "ibn", psc.ibn);
  mrc_ddc_set_param_int(bnd_c->ddc, "max_n_fields", 6);
  mrc_ddc_set_param_int(bnd_c->ddc, "size_of_type", sizeof(fields_base_real_t));
  mrc_ddc_setup(bnd_c->ddc);

  bnd_c->ddcp = ddc_particles_create(bnd_c->ddc);
}

// ----------------------------------------------------------------------
// psc_bnd_c_add_ghosts

static void
psc_bnd_c_add_ghosts(struct psc_bnd *bnd, mfields_base_t *flds, int mb, int me)
{
  struct psc_bnd_c *c_bnd = to_psc_bnd_c(bnd);

  static int pr;
  if (!pr) {
    pr = prof_register("c_add_ghosts", 1., 0, 0);
  }
  prof_start(pr);

  mrc_ddc_add_ghosts(c_bnd->ddc, mb, me, flds);

  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_bnd_c_fill_ghosts

static void
psc_bnd_c_fill_ghosts(struct psc_bnd *bnd, mfields_base_t *flds, int mb, int me)
{
  struct psc_bnd_c *c_bnd = to_psc_bnd_c(bnd);

  static int pr;
  if (!pr) {
    pr = prof_register("c_fill_ghosts", 1., 0, 0);
  }
  prof_start(pr);

  // FIXME
  // I don't think we need as many points, and only stencil star
  // rather then box
  mrc_ddc_fill_ghosts(c_bnd->ddc, mb, me, flds);

  prof_stop(pr);
}

// ----------------------------------------------------------------------
// psc_bnd_c_exchange_particles

static void
psc_bnd_c_exchange_particles(struct psc_bnd *bnd, mparticles_base_t *particles)
{
  struct psc_bnd_c *c_bnd = to_psc_bnd_c(bnd);

  static int pr, pr_A, pr_B;
  if (!pr) {
    pr = prof_register("c_xchg_part", 1., 0, 0);
    pr_A = prof_register("c_xchg_part_A", 1., 0, 0);
    pr_B = prof_register("c_xchg_part_B", 1., 0, 0);
  }
  prof_start(pr);
  prof_start(pr_A);

  struct ddc_particles *ddcp = c_bnd->ddcp;

  f_real xb[3], xe[3], xgb[3], xge[3], xgl[3];

  // New-style boundary requirements.
  // These will need revisiting when it comes to non-periodic domains.
  // FIXME, calculate once

  foreach_patch(p) {
    struct psc_patch *psc_patch = &psc.patch[p];
    particles_base_t *pp = &particles->p[p];

    for (int d = 0; d < 3; d++) {
      xb[d] = (psc_patch->off[d]-.5) * psc.dx[d];
      if (psc.domain.bnd_fld_lo[d] == BND_FLD_PERIODIC) {
	xgb[d] = -.5 * psc.dx[d];
      } else {
	xgb[d] = 0.;
	if (psc_patch->off[d] == 0) {
	  xb[d] = xgb[d];
	}
      }
      
      xe[d] = (psc_patch->off[d] + psc_patch->ldims[d] - .5) * psc.dx[d];
      if (psc.domain.bnd_fld_lo[d] == BND_FLD_PERIODIC) {
	xge[d] = (psc.domain.gdims[d]-.5) * psc.dx[d];
      } else {
	xge[d] = (psc.domain.gdims[d]-1) * psc.dx[d];
	if (psc_patch->off[d] + psc_patch->ldims[d] == psc.domain.gdims[d]) {
	  xe[d] = xge[d];
	}
      }
      
      xgl[d] = xge[d] - xgb[d];
    }

    struct ddcp_patch *patch = &ddcp->patches[p];
    patch->head = 0;
    for (int dir1 = 0; dir1 < N_DIR; dir1++) {
      patch->nei[dir1].n_send = 0;
    }
    for (int i = 0; i < pp->n_part; i++) {
      particle_base_t *part = particles_base_get_one(pp, i);
      particle_base_real_t *xi = &part->xi; // slightly hacky relies on xi, yi, zi to be contiguous in the struct. FIXME
      particle_base_real_t *pxi = &part->pxi;
      if (xi[0] >= xb[0] && xi[0] <= xe[0] &&
	  xi[1] >= xb[1] && xi[1] <= xe[1] &&
	  xi[2] >= xb[2] && xi[2] <= xe[2]) {
	// fast path
	// inside domain: move into right position
	pp->particles[patch->head++] = *part;
      } else {
	// slow path
	int dir[3];
	for (int d = 0; d < 3; d++) {
	  if (xi[d] < xb[d]) {
	    if (xi[d] < xgb[d]) {
	      switch (psc.domain.bnd_part[d]) {
	      case BND_PART_REFLECTING:
		xi[d] = 2.f * xgb[d] - xi[d];
		pxi[d] = -pxi[d];
		dir[d] = 0;
		break;
	      case BND_PART_PERIODIC:
		xi[d] += xgl[d];
		dir[d] = -1;
		break;
	      default:
		assert(0);
	      }
	    } else {
	      // computational bnd
	      dir[d] = -1;
	    }
	  } else if (xi[d] > xe[d]) {
	    if (xi[d] > xge[d]) {
	      switch (psc.domain.bnd_part[d]) {
	      case BND_PART_REFLECTING:
		xi[d] = 2.f * xge[d] - xi[d];
		pxi[d] = -pxi[d];
		dir[d] = 0;
		break;
	      case BND_PART_PERIODIC:
		xi[d] -= xgl[d];
		dir[d] = +1;
		break;
	      default:
		assert(0);
	      }
	    } else {
	      dir[d] = +1;
	    }
	  } else {
	    // computational bnd
	    dir[d] = 0;
	  }
	}
	if (dir[0] == 0 && dir[1] == 0 && dir[2] == 0) {
	  pp->particles[patch->head++] = *part;
	} else {
	  ddc_particles_queue(patch, dir, part);
	}
      }
    }
  }
  prof_stop(pr_A);

  prof_start(pr_B);
  ddc_particles_comm(ddcp, particles);
  foreach_patch(p) {
    particles_base_t *pp = &particles->p[p];
    struct ddcp_patch *patch = &ddcp->patches[p];
    pp->n_part = patch->head;
  }
  prof_stop(pr_B);

  prof_stop(pr);
}


// ======================================================================
// psc_bnd: subclass "c"

struct psc_bnd_ops psc_bnd_c_ops = {
  .name                  = "c",
  .size                  = sizeof(struct psc_bnd_c),
  .setup                 = psc_bnd_c_setup,
  .add_ghosts            = psc_bnd_c_add_ghosts,
  .fill_ghosts           = psc_bnd_c_fill_ghosts,
  .exchange_particles    = psc_bnd_c_exchange_particles,
};
