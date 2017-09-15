
#include <mrc_domain.h>
#include <mrc_profile.h>
#include <string.h>

// ----------------------------------------------------------------------
// ddcp_particles helpers

static void
ddcp_particles_realloc(void *_ctx, int p, int new_n_particles)
{
  struct psc_mparticles *mprts = _ctx;
  psc_mparticles_realloc(mprts, p, new_n_particles);
}

static void *
ddcp_particles_get_addr(void *_ctx, int p, int n)
{
  struct psc_mparticles *mprts = _ctx;
  particle_range_t prts = particle_range_mprts(mprts, p);
  return particle_iter_at(prts.begin, n);
}

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_setup

static void
psc_bnd_particles_sub_setup(struct psc_bnd_particles *bnd)
{
  bnd->ddcp = ddc_particles_create(bnd->psc->mrc_domain, sizeof(particle_t),
				   sizeof(particle_real_t),
				   MPI_PARTICLES_REAL,
				   ddcp_particles_realloc,
				   ddcp_particles_get_addr);
}

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_unsetup

static void
psc_bnd_particles_sub_unsetup(struct psc_bnd_particles *bnd)
{
  ddc_particles_destroy(bnd->ddcp);
}

// ======================================================================
//
// ----------------------------------------------------------------------
// find_block_position

static inline void
find_block_position(int b_pos[3], particle_real_t xi[3], particle_real_t b_dxi[3])
{
  for (int d = 0; d < 3; d++) {
    b_pos[d] = particle_real_fint(xi[d] * b_dxi[d]);
  }
}

// ----------------------------------------------------------------------
// find_block_indices_count

static inline void
find_block_indices_count(unsigned int *b_idx, unsigned int *b_cnts,
			 struct psc_mparticles *mprts, int p, int off)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);
  unsigned int n_prts = particle_range_size(prts);
  int *b_mx = sngl->b_mx;
  for (int i = off; i < n_prts; i++) {
    particle_t *part = particle_iter_at(prts.begin, i);
    int b_pos[3];
    find_block_position(b_pos, &part->xi, sngl->b_dxi);
    assert(b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	   b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	   b_pos[2] >= 0 && b_pos[2] < b_mx[2]);
    b_idx[i] = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
    b_cnts[b_idx[i]]++;
  }
}

// ----------------------------------------------------------------------
// find_block_indices_count_reorder

static void _mrc_unused
find_block_indices_count_reorder(struct psc_mparticles *mprts, int p)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);
  unsigned int n_prts = particle_range_size(prts);
  unsigned int cnt = n_prts;
  int *b_mx = sngl->b_mx;
  memset(sngl->b_cnt, 0, (sngl->nr_blocks + 1) * sizeof(*sngl->b_cnt));
  for (int i = 0; i < n_prts; i++) {
    particle_t *part = particle_iter_at(prts.begin, i);
    int b_pos[3];
    find_block_position(b_pos, &part->xi, sngl->b_dxi);
    if (b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	b_pos[2] >= 0 && b_pos[2] < b_mx[2]) {
      sngl->b_idx[i] = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
    } else { // out of bounds
      sngl->b_idx[i] = sngl->nr_blocks;
      *particle_iter_at(prts.begin, cnt) = *part;
      cnt++;
    }
    sngl->b_cnt[sngl->b_idx[i]]++;
  }
}

static void _mrc_unused
count_and_reorder_to_back(struct psc_mparticles *mprts, int p)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);

  memset(sngl->b_cnt, 0, (sngl->nr_blocks + 1) * sizeof(*sngl->b_cnt));
  unsigned int n_prts = particle_range_size(prts);
  unsigned int cnt = n_prts;
  for (int i = 0; i < n_prts; i++) {
    if (sngl->b_idx[i] == sngl->nr_blocks) {
      *particle_iter_at(prts.begin, cnt) = *particle_iter_at(prts.begin, i);
      cnt++;
    }
    sngl->b_cnt[sngl->b_idx[i]]++;
  }
}

static void _mrc_unused
reorder_to_back(struct psc_mparticles *mprts, int p)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);

  unsigned int n_prts = particle_range_size(prts);
  unsigned int cnt = n_prts;
  for (int i = 0; i < n_prts; i++) {
    if (sngl->b_idx[i] == sngl->nr_blocks) {
      *particle_iter_at(prts.begin, cnt) = *particle_iter_at(prts.begin, i);
      cnt++;
    }
  }
}

// ----------------------------------------------------------------------
// count_block_indices

static inline void
count_block_indices(unsigned int *b_cnts, unsigned int *b_idx, int n_prts, int off)
{
  for (int i = off; i < n_prts; i++) {
    b_cnts[b_idx[i]]++;
  }
}

// ----------------------------------------------------------------------
// exclusive_scan

static inline void
exclusive_scan(unsigned int *b_cnts, int n)
{
  unsigned int sum = 0;
  for (int i = 0; i < n; i++) {
    unsigned int cnt = b_cnts[i];
    b_cnts[i] = sum;
    sum += cnt;
  }
}

// ----------------------------------------------------------------------
// sort_indices

static void
sort_indices(unsigned int *b_idx, unsigned int *b_sum, unsigned int *b_ids, int n_prts)
{
  for (int n = 0; n < n_prts; n++) {
    unsigned int n_new = b_sum[b_idx[n]]++;
    assert(n_new < n_prts);
    b_ids[n_new] = n;
  }
}

// ======================================================================

static inline particle_t *
xchg_get_one(struct psc_mparticles *mprts, int p, int n)
{
  particle_range_t prts = particle_range_mprts(mprts, p);
  return particle_iter_at(prts.begin, n);
}

static inline void
xchg_append(struct psc_mparticles *mprts, int p, void *patch_ctx, particle_t *prt)
{
  struct ddcp_patch *ddcp_patch = patch_ctx;
  particle_range_t prts = particle_range_mprts(mprts, p);
  *particle_iter_at(prts.begin, ddcp_patch->head++) = *prt;
}

static inline int *
get_b_mx(struct psc_particles *prts)
{
  return ppsc->patch[prts->p].ldims;
}

static inline particle_real_t *
get_b_dxi(struct psc_particles *prts)
{
  static particle_real_t b_dxi[3];
  if (!b_dxi[0]) {
    for (int d = 0; d < 3; d++) {
      b_dxi[d] = 1.f / ppsc->patch[prts->p].dx[d];
    }
  }
  return b_dxi;
}

static inline int
get_n_send(struct psc_particles *prts)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);
  return sngl->n_send;
}

static inline int
get_head(struct psc_mparticles *mprts, int p)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);
  return particle_range_size(prts) - sngl->n_send;
}

#include "psc_bnd_particles_exchange_particles_pre.c"

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_exchange_particles_prep

static void
psc_bnd_particles_sub_exchange_particles_prep(struct psc_bnd_particles *bnd,
					      struct psc_mparticles *mprts, int p)
{
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);
  if (1) {
    //      find_block_indices_count_reorderx(prts);
    count_and_reorder_to_back(mprts, p);
  }
  sngl->n_part_save = particle_range_size(prts);
  sngl->n_send = sngl->b_cnt[sngl->nr_blocks];
  particle_range_resize(&prts, particle_range_size(prts) + sngl->n_send);

  exchange_particles_pre(bnd, mprts, p);
}

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_exchange_particles_post

static void
psc_bnd_particles_sub_exchange_particles_post(struct psc_bnd_particles *bnd,
					      struct psc_mparticles *mprts, int p)
{
  struct ddc_particles *ddcp = bnd->ddcp;
  struct psc_particles *_prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_single *sngl = psc_particles_single(_prts);
  particle_range_t prts = particle_range_mprts(mprts, p);
  struct ddcp_patch *patch = &ddcp->patches[p];

  particle_range_resize(&prts, patch->head);
  
  find_block_indices_count(sngl->b_idx, sngl->b_cnt, mprts, p, sngl->n_part_save);
  exclusive_scan(sngl->b_cnt, sngl->nr_blocks + 1);
  sort_indices(sngl->b_idx, sngl->b_cnt, sngl->b_ids, particle_range_size(prts));
  
  particle_range_resize(&prts, sngl->b_cnt[sngl->nr_blocks - 1]);
  sngl->need_reorder = true; // FIXME, need to honor before get()/put()
}

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_exchange_particles

extern double *psc_balance_comp_time_by_patch;
extern int pr_time_step_no_comm;

static void
psc_bnd_particles_sub_exchange_particles(struct psc_bnd_particles *bnd, struct psc_mparticles *particles_base)
{
  static int pr_A, pr_B, pr_C;
  if (!pr_A) {
    pr_A = prof_register("xchg_prep", 1., 0, 0);
    pr_B = prof_register("xchg_comm", 1., 0, 0);
    pr_C = prof_register("xchg_post", 1., 0, 0);
  }
  
  struct ddc_particles *ddcp = bnd->ddcp;
  struct psc_mparticles *particles = psc_mparticles_get_as(particles_base, PARTICLE_TYPE, 0);

  prof_start(pr_A);
  prof_restart(pr_time_step_no_comm);
  for (int p = 0; p < particles->nr_patches; p++) {
    psc_balance_comp_time_by_patch[p] -= MPI_Wtime();
    psc_bnd_particles_sub_exchange_particles_prep(bnd, particles, p);
    psc_balance_comp_time_by_patch[p] += MPI_Wtime();
  }
  prof_stop(pr_time_step_no_comm);
  prof_stop(pr_A);

  prof_start(pr_B);
  ddc_particles_comm(ddcp, particles);
  prof_stop(pr_B);

  prof_start(pr_C);
  for (int p = 0; p < particles->nr_patches; p++) {
    psc_bnd_particles_sub_exchange_particles_post(bnd, particles, p);
  }
  prof_stop(pr_C);

  psc_mparticles_put_as(particles, particles_base, 0);
}

