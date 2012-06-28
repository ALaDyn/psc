
#include <mrc_domain.h>
#include <mrc_profile.h>
#include <string.h>

struct psc_bnd_sub {
};

#define to_psc_bnd_sub(bnd) ((struct psc_bnd_sub *)((bnd)->obj.subctx))

static void
ddcp_particles_realloc(void *_particles, int p, int new_n_particles)
{
  mparticles_t *particles = _particles;
  struct psc_particles *prts = psc_mparticles_get_patch(particles, p);
  particles_realloc(prts, new_n_particles);
}

static void *
ddcp_particles_get_addr(void *_particles, int p, int n)
{
  mparticles_t *particles = _particles;
  struct psc_particles *prts = psc_mparticles_get_patch(particles, p);
  return particles_get_one(prts, n);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_setup

static void
psc_bnd_sub_setup(struct psc_bnd *bnd)
{
  psc_bnd_setup_super(bnd);
  bnd->ddcp = ddc_particles_create(bnd->ddc, sizeof(particle_t),
				   sizeof(particle_real_t),
				   MPI_PARTICLES_REAL,
				   ddcp_particles_realloc,
				   ddcp_particles_get_addr);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_unsetup

static void
psc_bnd_sub_unsetup(struct psc_bnd *bnd)
{
  ddc_particles_destroy(bnd->ddcp);
}

// ======================================================================
//
// ----------------------------------------------------------------------
// find_block_index

static inline void
find_block_index(int b_pos[3], particle_real_t xi[3], particle_real_t b_dxi[3])
{
  for (int d = 0; d < 3; d++) {
    b_pos[d] = particle_real_fint(xi[d] * b_dxi[d]);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_prep

static void
psc_bnd_sub_exchange_particles_prep(struct psc_bnd *bnd, struct psc_particles *prts)
{
  struct ddc_particles *ddcp = bnd->ddcp;
  struct psc *psc = bnd->psc;

  // New-style boundary requirements.
  // These will need revisiting when it comes to non-periodic domains.

  particle_real_t b_dxi[3] = { 1.f / psc->dx[0], 1.f / psc->dx[1], 1.f / psc->dx[2] };
  struct psc_patch *ppatch = &psc->patch[prts->p];
  particle_real_t xm[3];
  for (int d = 0; d < 3; d++) {
    xm[d] = ppatch->ldims[d] * psc->dx[d];
  }
  int *b_mx = ppatch->ldims;
  
  struct ddcp_patch *patch = &ddcp->patches[prts->p];
  patch->head = 0;
  for (int dir1 = 0; dir1 < N_DIR; dir1++) {
    patch->nei[dir1].n_send = 0;
  }
  for (int i = 0; i < prts->n_part; i++) {
    particle_t *part = particles_get_one(prts, i);
    particle_real_t *xi = &part->xi; // slightly hacky relies on xi, yi, zi to be contiguous in the struct. FIXME
    
    int b_pos[3];
    find_block_index(b_pos, xi, b_dxi);
    particle_real_t *pxi = &part->pxi;
    if (b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	b_pos[2] >= 0 && b_pos[2] < b_mx[2]) {
      // fast path
      // inside domain: move into right position
      *particles_get_one(prts, patch->head++) = *part;
    } else {
      // slow path
      bool drop = false;
      int dir[3];
      for (int d = 0; d < 3; d++) {
	if (b_pos[d] < 0) {
	  if (ppatch->off[d] > 0 ||
	      psc->domain.bnd_part_lo[d] == BND_PART_PERIODIC) {
	    xi[d] += xm[d];
	    dir[d] = -1;
	    int bi = particle_real_fint(xi[d] * b_dxi[d]);
	    if (bi >= b_mx[d]) {
	      xi[d] = 0.;
	      dir[d] = 0;
	    }
	  } else {
	    switch (psc->domain.bnd_part_lo[d]) {
	    case BND_PART_REFLECTING:
	      xi[d] =  -xi[d];
	      pxi[d] = -pxi[d];
	      dir[d] = 0;
	      break;
	    case BND_PART_ABSORBING:
	      drop = true;
	      break;
	    default:
	      assert(0);
	    }
	  }
	} else if (b_pos[d] >= b_mx[d]) {
	  if (ppatch->off[d] + ppatch->ldims[d] < psc->domain.gdims[d] ||
	      psc->domain.bnd_part_hi[d] == BND_PART_PERIODIC) {
	    xi[d] -= xm[d];
	    dir[d] = +1;
	    int bi = particle_real_fint(xi[d] * b_dxi[d]);
	    if (bi < 0) {
	      xi[d] = 0.;
	    }
	  } else {
	    switch (psc->domain.bnd_part_hi[d]) {
	    case BND_PART_REFLECTING:
	      xi[d] = 2.f * xm[d] - xi[d];
	      pxi[d] = -pxi[d];
	      dir[d] = 0;
	      int bi = particle_real_fint(xi[d] * b_dxi[d]);
	      if (bi >= b_mx[d]) {
		xi[d] *= (1. - 1e-6);
	      }
	      break;
	    case BND_PART_ABSORBING:
	      drop = true;
	      break;
	    default:
	      assert(0);
	    }
	  }
	} else {
	  // computational bnd
	  dir[d] = 0;
	}
	assert(xi[d] >= 0.f);
	assert(xi[d] <= xm[d]);
      }
      if (!drop) {
	if (dir[0] == 0 && dir[1] == 0 && dir[2] == 0) {
	  *particles_get_one(prts, patch->head++) = *part;
	} else {
	  ddc_particles_queue(ddcp, patch, dir, part);
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_post

static void
psc_bnd_sub_exchange_particles_post(struct psc_bnd *bnd, struct psc_particles *prts)
{
  struct ddc_particles *ddcp = bnd->ddcp;
  struct ddcp_patch *patch = &ddcp->patches[prts->p];
  prts->n_part = patch->head;
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles

static void
psc_bnd_sub_exchange_particles(struct psc_bnd *bnd, mparticles_base_t *particles_base)
{
  struct psc *psc = bnd->psc;

  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  static int pr_A, pr_B, pr_C;
  if (!pr_A) {
    pr_A = prof_register("xchg_prep", 1., 0, 0);
    pr_B = prof_register("xchg_comm", 1., 0, 0);
    pr_C = prof_register("xchg_post", 1., 0, 0);
  }
  
  prof_start(pr_A);

  struct ddc_particles *ddcp = bnd->ddcp;

  // FIXME we should make sure (assert) we don't quietly drop particle which left
  // in the invariant direction

  psc_foreach_patch(psc, p) {
    psc_bnd_sub_exchange_particles_prep(bnd, psc_mparticles_get_patch(particles, p));
  }
  prof_stop(pr_A);

  prof_start(pr_B);
  ddc_particles_comm(ddcp, particles);
  prof_stop(pr_B);

  prof_start(pr_C);
  for (int p = 0; p < particles->nr_patches; p++) {
    psc_bnd_sub_exchange_particles_post(bnd, psc_mparticles_get_patch(particles, p));
  }
  prof_stop(pr_C);

  psc_mparticles_put_cf(particles, particles_base, 0);
}

