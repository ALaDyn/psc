
#include <mrc_domain.h>
#include <mrc_profile.h>
#include <string.h>

#define DDCP_TYPE DDCP_TYPE_COMMON2

static void psc_bnd_particles_sub_exchange_mprts_prep_common2(struct psc_bnd_particles *bnd,
							      struct psc_mparticles *mprts);
static void psc_bnd_particles_sub_exchange_mprts_post_common2(struct psc_bnd_particles *bnd,
							      struct psc_mparticles *mprts);

#include "ddc_particles_inc.c"


// ----------------------------------------------------------------------
// find_block_indices_count

static inline void
find_block_indices_count(unsigned int *b_idx, unsigned int *b_cnts,
			 struct psc_mparticles *mprts, int p, int off)
{
  struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
  struct psc_mparticles_single_patch *patch = &sub->patch[p];

  particle_range_t prts = mparticles_t(mprts)[p].range();
  unsigned int n_prts = prts.size();
  int *b_mx = patch->b_mx;
  for (int i = off; i < n_prts; i++) {
    particle_t *part = &prts.begin[i];
    int b_pos[3];
    particle_xi_get_block_pos(&part->xi, patch->b_dxi, b_pos);
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
  struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
  struct psc_mparticles_single_patch *patch = &sub->patch[p];
  particle_range_t prts = mparticles_t(mprts)[p].range();
  unsigned int n_prts = prts.size();
  unsigned int cnt = n_prts;
  int *b_mx = patch->b_mx;
  memset(patch->b_cnt, 0, (patch->nr_blocks + 1) * sizeof(*patch->b_cnt));

  for (int i = 0; i < n_prts; i++) {
    particle_t *part = &prts.begin[i];
    int b_pos[3];
    particle_xi_get_block_pos(&part->xi, patch->b_dxi, b_pos);
    if (b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	b_pos[2] >= 0 && b_pos[2] < b_mx[2]) {
      patch->b_idx[i] = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
    } else { // out of bounds
      patch->b_idx[i] = patch->nr_blocks;
      prts.begin[cnt] = *part;
      cnt++;
    }
    patch->b_cnt[patch->b_idx[i]]++;
  }
}

static void _mrc_unused
count_and_reorder_to_back(struct psc_mparticles *mprts, int p)
{
  struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
  struct psc_mparticles_single_patch *patch = &sub->patch[p];
  particle_range_t prts = mparticles_t(mprts)[p].range();

  memset(patch->b_cnt, 0, (patch->nr_blocks + 1) * sizeof(*patch->b_cnt));
  unsigned int n_prts = prts.size();
  unsigned int cnt = n_prts;
  for (int i = 0; i < n_prts; i++) {
    if (patch->b_idx[i] == patch->nr_blocks) {
      prts.begin[cnt] = prts.begin[i];
      cnt++;
    }
    patch->b_cnt[patch->b_idx[i]]++;
  }
}

static void _mrc_unused
reorder_to_back(struct psc_mparticles *mprts, int p)
{
  struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
  struct psc_mparticles_single_patch *patch = &sub->patch[p];
  particle_range_t prts = mparticles_t(mprts)[p].range();

  unsigned int n_prts = prts.size();
  unsigned int cnt = n_prts;
  for (int i = 0; i < n_prts; i++) {
    if (patch->b_idx[i] == patch->nr_blocks) {
      prts.begin[cnt] = prts.begin[i];
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

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_exchange_mprts_prep_common2

static void
psc_bnd_particles_sub_exchange_mprts_prep_common2(struct psc_bnd_particles *bnd,
						  struct psc_mparticles *mprts)
{
  struct ddc_particles *ddcp = bnd->ddcp;

  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
    struct psc_mparticles_single_patch *patch = &sub->patch[p];
    struct ddcp_patch *dpatch = &ddcp->patches[p];

    if (1) {
      //      find_block_indices_count_reorderx(prts);
      count_and_reorder_to_back(mprts, p);
    }
    dpatch->m_buf = mparticles_patch_get_buf(mprts, p);
    dpatch->m_begin = particle_buf_size(dpatch->m_buf);

    unsigned int n_send = patch->b_cnt[patch->nr_blocks];
    particle_buf_resize(dpatch->m_buf, dpatch->m_begin + n_send);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_particles_sub_exchange_mprts_post_common2

static void
psc_bnd_particles_sub_exchange_mprts_post_common2(struct psc_bnd_particles *bnd,
						  struct psc_mparticles *mprts)
{
  struct ddc_particles *ddcp = bnd->ddcp;

  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_mparticles_single *sub = psc_mparticles_single(mprts);
    struct psc_mparticles_single_patch *patch = &sub->patch[p];
    struct ddcp_patch *dpatch = &ddcp->patches[p];

    int n_prts = particle_buf_size(dpatch->m_buf);
    
    find_block_indices_count(patch->b_idx, patch->b_cnt, mprts, p, dpatch->m_begin);
    exclusive_scan(patch->b_cnt, patch->nr_blocks + 1);
    sort_indices(patch->b_idx, patch->b_cnt, patch->b_ids, n_prts);

    // FIXME, why?
    mparticles_patch_resize(mprts, p, patch->b_cnt[patch->nr_blocks - 1]);
    patch->need_reorder = true; // FIXME, need to honor before get()/put()
  }
}

