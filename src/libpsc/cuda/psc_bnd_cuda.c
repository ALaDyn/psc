
#include "psc_bnd_cuda.h"
#include "psc_cuda.h"
#include "psc_bnd_cuda_fields.h"
#include "psc_particles_as_single.h"
#include "../psc_bnd/ddc_particles.h"

#include <mrc_profile.h>

EXTERN_C void cuda_mprts_find_block_indices_2(struct psc_mparticles *mprts);
EXTERN_C void cuda_mprts_reorder_send_buf(struct psc_mparticles *mprts);

struct psc_bnd_sub {
};

#define to_psc_bnd_sub(bnd) ((struct psc_bnd_sub *)((bnd)->obj.subctx))

// ----------------------------------------------------------------------
// ddcp_particles helpers

static void
ddcp_particles_realloc(void *_ctx, int p, int new_n_particles)
{
  struct psc_mparticles *mprts = _ctx;
  struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  assert(!cuda->bnd_prts);
  cuda->bnd_prts = malloc(new_n_particles * sizeof(*cuda->bnd_prts));
}

static void *
ddcp_particles_get_addr(void *_ctx, int p, int n)
{
  struct psc_mparticles *mprts = _ctx;
  struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  return &cuda->bnd_prts[n];
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

// ----------------------------------------------------------------------
// xchg_append helper

static void
xchg_append(struct psc_particles *prts, struct ddcp_patch *ddcp_patch, particle_t *prt)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int nn = cuda->bnd_n_part++;
  cuda->bnd_xi4[nn].x  = prt->xi;
  cuda->bnd_xi4[nn].y  = prt->yi;
  cuda->bnd_xi4[nn].z  = prt->zi;
  cuda->bnd_xi4[nn].w  = cuda_int_as_float(prt->kind);
  cuda->bnd_pxi4[nn].x = prt->pxi;
  cuda->bnd_pxi4[nn].y = prt->pyi;
  cuda->bnd_pxi4[nn].z = prt->pzi;
  cuda->bnd_pxi4[nn].w = prt->qni_wni;

  int b_pos[3];
  for (int d = 0; d < 3; d++) {
    float *xi = &cuda->bnd_xi4[nn].x;
    b_pos[d] = particle_real_fint(xi[d] * cuda->b_dxi[d]);
    if (b_pos[d] < 0 || b_pos[d] >= cuda->b_mx[d]) {
      printf("!!! xi %g %g %g\n", xi[0], xi[1], xi[2]);
      printf("!!! d %d xi4[n] %g biy %d // %d\n",
	     d, xi[d], b_pos[d], cuda->b_mx[d]);
      if (b_pos[d] < 0) {
	xi[d] = 0.f;
      } else {
	xi[d] *= (1. - 1e-6);
      }
    }
    b_pos[d] = particle_real_fint(xi[d] * cuda->b_dxi[d]);
    assert(b_pos[d] >= 0 && b_pos[d] < cuda->b_mx[d]);
  }
  unsigned int b =
    (b_pos[2] * cuda->b_mx[1] + b_pos[1]) * cuda->b_mx[0] + b_pos[0];
  assert(b < cuda->nr_blocks);
  cuda->bnd_idx[nn] = b;
  cuda->bnd_off[nn] = cuda->bnd_cnt[b]++;
}

static inline particle_t *
xchg_get_one(struct psc_particles *prts, int n)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  static particle_t prt_static;

  particle_t *prt = &prt_static;
  prt->xi      = cuda->bnd_xi4[n].x;
  prt->yi      = cuda->bnd_xi4[n].y;
  prt->zi      = cuda->bnd_xi4[n].z;
  prt->kind    = cuda_float_as_int(cuda->bnd_xi4[n].w);
  prt->pxi     = cuda->bnd_pxi4[n].x;
  prt->pyi     = cuda->bnd_pxi4[n].y;
  prt->pzi     = cuda->bnd_pxi4[n].z;
  prt->qni_wni = cuda->bnd_pxi4[n].w;

  return prt;
}

static inline int *
get_b_mx(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  return cuda->b_mx;
}

static inline particle_real_t *
get_b_dxi(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  return cuda->b_dxi;
}

static inline int
get_n_send(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  return cuda->bnd_n_send;
}

static inline int
get_head(struct psc_particles *prts)
{
  return 0;
}

// ----------------------------------------------------------------------
// xchg_copy_from_dev

static void
xchg_copy_from_dev(struct psc_bnd *bnd, struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  
  cuda->bnd_n_part = 0;
  cuda->bnd_prts = NULL;
  
  int n_send = cuda->bnd_n_send;
  cuda->bnd_xi4  = malloc(n_send * sizeof(*cuda->bnd_xi4));
  cuda->bnd_pxi4 = malloc(n_send * sizeof(*cuda->bnd_pxi4));
  cuda->bnd_idx  = malloc(n_send * sizeof(*cuda->bnd_idx));
  cuda->bnd_off  = malloc(n_send * sizeof(*cuda->bnd_off));
  
  // OPT, could use streaming
  __particles_cuda_from_device_range(prts, cuda->bnd_xi4, cuda->bnd_pxi4,
				     cuda->bnd_n_part_save, cuda->bnd_n_part_save + n_send);
}

#include "../psc_bnd/psc_bnd_exchange_particles_pre.c"

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_prep

static void
psc_bnd_sub_exchange_particles_prep(struct psc_bnd *bnd, struct psc_particles *prts)
{
  static int pr_A, pr_A2, pr_B, pr_C, pr_D;
  if (!pr_A) {
    pr_A = prof_register("xchg_bidx", 1., 0, 0);
    pr_A2= prof_register("xchg_scan", 1., 0, 0);
    pr_B = prof_register("xchg_reorder_send", 1., 0, 0);
    pr_C = prof_register("xchg_from_dev", 1., 0, 0);
    pr_D = prof_register("xchg_pre", 1., 0, 0);
  }

  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  prof_start(pr_A);
  cuda->bnd_cnt = calloc(cuda->nr_blocks, sizeof(*cuda->bnd_cnt));
  cuda_find_block_indices_2(prts, cuda->d_part.bidx, 0);
  prof_stop(pr_A);
  
  prof_start(pr_A2);
  cuda->bnd_n_send = cuda_exclusive_scan_2(prts, cuda->d_part.bidx, cuda->d_part.sums);
  cuda->bnd_n_part_save = prts->n_part;
  prof_stop(pr_A2);
  
  prof_start(pr_B);
  cuda_reorder_send_buf(prts->p, prts, cuda->d_part.bidx, cuda->d_part.sums, cuda->bnd_n_send);
  prof_stop(pr_B);
  
  prof_start(pr_C);
  xchg_copy_from_dev(bnd, prts);
  prof_stop(pr_C);
  
  prof_start(pr_D);
  exchange_particles_pre(bnd, prts);
  prof_stop(pr_D);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_mprts_prep

static void
psc_bnd_sub_exchange_mprts_prep(struct psc_bnd *bnd,
				struct psc_mparticles *mprts)
{
  static int pr_A, pr_A2, pr_B, pr_C, pr_D;
  if (!pr_A) {
    pr_A = prof_register("xchg_bidx", 1., 0, 0);
    pr_A2= prof_register("xchg_scan", 1., 0, 0);
    pr_B = prof_register("xchg_reorder_send", 1., 0, 0);
    pr_C = prof_register("xchg_from_dev", 1., 0, 0);
    pr_D = prof_register("xchg_pre", 1., 0, 0);
  }

  prof_start(pr_A);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda->bnd_cnt = (unsigned int *) calloc(cuda->nr_blocks, sizeof(*cuda->bnd_cnt));
  }
  //cuda_mprts_find_block_indices_2(mprts);
  prof_stop(pr_A);

  prof_start(pr_A2);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda->bnd_n_send = cuda_exclusive_scan_2(prts, cuda->d_part.bidx, cuda->d_part.sums);
    cuda->bnd_n_part_save = prts->n_part;
  }
  prof_stop(pr_A2);
    
  prof_start(pr_B);
  cuda_mprts_reorder_send_buf(mprts);
  prof_stop(pr_B);
    
  prof_start(pr_C);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    xchg_copy_from_dev(bnd, prts);
  }
  prof_stop(pr_C);
    
  prof_start(pr_D);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    exchange_particles_pre(bnd, prts);
  }
  prof_stop(pr_D);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_post

static void
psc_bnd_sub_exchange_particles_post(struct psc_bnd *bnd, struct psc_particles *prts)
{
  static int pr_A, pr_B, pr_C, pr_C2, pr_D;
  if (!pr_A) {
    pr_A = prof_register("xchg_append", 1., 0, 0);
    pr_B = prof_register("xchg_to_dev", 1., 0, 0);
    pr_C = prof_register("xchg_bidx", 1., 0, 0);
    pr_C2= prof_register("xchg_sort", 1., 0, 0);
    pr_D = prof_register("xchg_reorder", 1., 0, 0);
  }

  prof_start(pr_A);
  struct ddc_particles *ddcp = bnd->ddcp;
  struct ddcp_patch *patch = &ddcp->patches[prts->p];
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int n_recv = cuda->bnd_n_part + patch->head;
  prts->n_part = cuda->bnd_n_part_save + n_recv;
  assert(prts->n_part <= cuda->n_alloced);
  
  cuda->bnd_xi4  = realloc(cuda->bnd_xi4, n_recv * sizeof(float4));
  cuda->bnd_pxi4 = realloc(cuda->bnd_pxi4, n_recv * sizeof(float4));
  cuda->bnd_idx  = realloc(cuda->bnd_idx, n_recv * sizeof(*cuda->bnd_idx));
  cuda->bnd_off  = realloc(cuda->bnd_off, n_recv * sizeof(*cuda->bnd_off));
  for (int n = 0; n < patch->head; n++) {
    xchg_append(prts, NULL, &cuda->bnd_prts[n]);
  }
  prof_stop(pr_A);

  prof_start(pr_B);
  __particles_cuda_to_device_range(prts, cuda->bnd_xi4, cuda->bnd_pxi4,
				   cuda->bnd_n_part_save, cuda->bnd_n_part_save + n_recv);
  
  free(cuda->bnd_prts);
  free(cuda->bnd_xi4);
  free(cuda->bnd_pxi4);
  prof_stop(pr_B);

  prof_start(pr_C);
  cuda_find_block_indices_3(prts, cuda->d_part.bidx, cuda->d_part.alt_bidx,
			    cuda->bnd_n_part_save, cuda->bnd_idx, cuda->bnd_off);
  free(cuda->bnd_idx);
  free(cuda->bnd_off);
  prof_stop(pr_C);
  
  prof_start(pr_C2);
  // OPT: when calculating bidx, do preprocess then
  void *sp = sort_pairs_3_create(cuda->b_mx);
  sort_pairs_3_device(sp, cuda->d_part.bidx, cuda->d_part.alt_bidx, cuda->d_part.alt_ids,
		      prts->n_part, cuda->d_part.offsets,
		      cuda->bnd_n_part_save, cuda->bnd_cnt);
  sort_pairs_3_destroy(sp);
  prof_stop(pr_C2);
  
  prof_start(pr_D);
  cuda_reorder(prts, cuda->d_part.alt_ids);
  prof_stop(pr_D);
  
  prts->n_part -= cuda->bnd_n_send;
  
  free(cuda->bnd_cnt);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_mprts_post

static void
psc_bnd_sub_exchange_mprts_post(struct psc_bnd *bnd,
				struct psc_mparticles *mprts)
{
  static int pr_A, pr_B, pr_C, pr_C2, pr_D;
  if (!pr_A) {
    pr_A = prof_register("xchg_append", 1., 0, 0);
    pr_B = prof_register("xchg_to_dev", 1., 0, 0);
    pr_C = prof_register("xchg_bidx", 1., 0, 0);
    pr_C2= prof_register("xchg_sort", 1., 0, 0);
    pr_D = prof_register("xchg_reorder", 1., 0, 0);
  }

  prof_start(pr_A);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct ddc_particles *ddcp = bnd->ddcp;
    struct ddcp_patch *patch = &ddcp->patches[prts->p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    int n_recv = cuda->bnd_n_part + patch->head;
    prts->n_part = cuda->bnd_n_part_save + n_recv;
    assert(prts->n_part <= cuda->n_alloced);
    
    cuda->bnd_xi4  = realloc(cuda->bnd_xi4, n_recv * sizeof(float4));
    cuda->bnd_pxi4 = realloc(cuda->bnd_pxi4, n_recv * sizeof(float4));
    cuda->bnd_idx  = realloc(cuda->bnd_idx, n_recv * sizeof(*cuda->bnd_idx));
    cuda->bnd_off  = realloc(cuda->bnd_off, n_recv * sizeof(*cuda->bnd_off));
    for (int n = 0; n < patch->head; n++) {
      xchg_append(prts, NULL, &cuda->bnd_prts[n]);
    }
  }
  prof_stop(pr_A);

  prof_start(pr_B);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    __particles_cuda_to_device_range(prts, cuda->bnd_xi4, cuda->bnd_pxi4,
				     cuda->bnd_n_part_save, prts->n_part);
    
    free(cuda->bnd_prts);
    free(cuda->bnd_xi4);
    free(cuda->bnd_pxi4);
  }
  prof_stop(pr_B);

  prof_start(pr_C);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda_find_block_indices_3(prts, cuda->d_part.bidx, cuda->d_part.alt_bidx,
			      cuda->bnd_n_part_save, cuda->bnd_idx, cuda->bnd_off);
    free(cuda->bnd_idx);
    free(cuda->bnd_off);
  }
  prof_stop(pr_C);
    
  prof_start(pr_C2);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    // OPT: when calculating bidx, do preprocess then
    void *sp = sort_pairs_3_create(cuda->b_mx);
    sort_pairs_3_device(sp, cuda->d_part.bidx, cuda->d_part.alt_bidx, cuda->d_part.alt_ids,
			prts->n_part, cuda->d_part.offsets,
			cuda->bnd_n_part_save, cuda->bnd_cnt);
    sort_pairs_3_destroy(sp);
  }
  prof_stop(pr_C2);
    
  prof_start(pr_D);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    if (psc_particles_ops(prts) != &psc_particles_cuda_ops) {
      continue;
    }
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda_reorder(prts, cuda->d_part.alt_ids);
    prts->n_part -= cuda->bnd_n_send;
    free(cuda->bnd_cnt);
  }
  prof_stop(pr_D);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_serial_periodic
//
// specialized version if there's only one single patch,
// with all periodic b.c.
//
// TODO: a lot of consolidation with the generic case should
// be possible. In particular, optimizations carry over, like
// calculating new offsets as part of the spine, not calculating
// new block indices, not calculates old ids.
// The most significant stumbling block to make them pretty much the same
// is that this case here needs to handle the odd periodic spine.

static void
psc_bnd_sub_exchange_particles_serial_periodic(struct psc_bnd *psc_bnd,
						mparticles_cuda_t *particles)
{
  static int pr_F, pr_G, pr_H;
  if (!pr_F) {
    pr_F = prof_register("xchg_bidx_ids", 1., 0, 0);
    pr_G = prof_register("xchg_sort_pairs", 1., 0, 0);
    pr_H = prof_register("xchg_reorder_off", 1., 0, 0);
  }

  cuda_exchange_particles(0, psc_mparticles_get_patch(particles, 0));

  // sort
  for (int p = 0; p < particles->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(particles, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    prof_start(pr_F);
    cuda_find_block_indices(prts, cuda->d_part.bidx);
    prof_stop(pr_F);

    prof_start(pr_G);
    sort_pairs_device_2(cuda->d_part.sort_ctx, cuda->d_part.bidx,
			cuda->d_part.alt_ids,
			prts->n_part,
			cuda->d_part.offsets);
    prof_stop(pr_G);

    prof_start(pr_H);
    cuda_reorder(prts, cuda->d_part.alt_ids);
    prof_stop(pr_H);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_general

static void
psc_bnd_sub_exchange_particles_general(struct psc_bnd *bnd,
					mparticles_cuda_t *particles)
{
  struct ddc_particles *ddcp = bnd->ddcp;

  static int pr_A, pr_B, pr_C;
  if (!pr_A) {
    pr_A = prof_register("xchg_prep", 1., 0, 0);
    pr_B = prof_register("xchg_comm", 1., 0, 0);
    pr_C = prof_register("xchg_post", 1., 0, 0);
  }
  
  prof_start(pr_A);
  for (int p = 0; p < particles->nr_patches; p++) {
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
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles

static void
psc_bnd_sub_exchange_particles(struct psc_bnd *bnd,
				mparticles_base_t *particles_base)
{
  int size;
  MPI_Comm_size(psc_bnd_comm(bnd), &size);

  // This function only makes sense if it's called for particles already being of cuda
  // type. We could call _get_cuda(), but that wouldn't be happy if some particles were
  // not in the right patch in the first place.

  assert(strcmp(psc_mparticles_type(particles_base), "cuda") == 0);
  mparticles_cuda_t *particles = particles_base;

  if (size == 1 && ppsc->nr_patches == 1 && // FIXME !!!
      ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    psc_bnd_sub_exchange_particles_serial_periodic(bnd, particles);
  } else {
    psc_bnd_sub_exchange_particles_general(bnd, particles);
  }
}

// ======================================================================
// psc_bnd: subclass "cuda"

struct psc_bnd_ops psc_bnd_cuda_ops = {
  .name                    = "cuda",
  .size                    = sizeof(struct psc_bnd_sub),
  .setup                   = psc_bnd_sub_setup,
  .unsetup                 = psc_bnd_sub_unsetup,
  .exchange_particles      = psc_bnd_sub_exchange_particles,
  .exchange_particles_prep = psc_bnd_sub_exchange_particles_prep,
  .exchange_particles_post = psc_bnd_sub_exchange_particles_post,
  .exchange_mprts_prep     = psc_bnd_sub_exchange_mprts_prep,
  .exchange_mprts_post     = psc_bnd_sub_exchange_mprts_post,

  .create_ddc              = psc_bnd_fld_cuda_create,
  .add_ghosts              = psc_bnd_fld_cuda_add_ghosts,
  .add_ghosts_prep         = psc_bnd_fld_cuda_add_ghosts_prep,
  .add_ghosts_post         = psc_bnd_fld_cuda_add_ghosts_post,
  .fill_ghosts             = psc_bnd_fld_cuda_fill_ghosts,
  .fill_ghosts_prep        = psc_bnd_fld_cuda_fill_ghosts_prep,
  .fill_ghosts_post        = psc_bnd_fld_cuda_fill_ghosts_post,
};

