
#include "psc_bnd_cuda.h"
#include "psc_cuda.h"
#include "psc_bnd_cuda_fields.h"
#include "particles_cuda.h"
#include "psc_particles_as_single.h"
#include "../psc_bnd/ddc_particles.h"

#include <mrc_profile.h>

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

#include "../psc_bnd/psc_bnd_exchange_particles_pre.c"

// ----------------------------------------------------------------------
// mprts_exchange_particles_pre

static void
mprts_exchange_particles_pre(struct psc_bnd *bnd, struct cuda_mprts *cuda_mprts)
{
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    exchange_particles_pre(bnd, prts);
  }
}

// ----------------------------------------------------------------------
// mprts_append_received

static void
mprts_append_recvd(struct psc_bnd *bnd, struct cuda_mprts *cuda_mprts)
{
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    struct ddc_particles *ddcp = bnd->ddcp;
    struct ddcp_patch *patch = &ddcp->patches[prts->p];
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
}

// ----------------------------------------------------------------------
// xchg_prep

static void
xchg_prep(struct psc_bnd *bnd, struct cuda_mprts *cuda_mprts)
{
  static int pr_A, pr_B, pr_C, pr_D, pr_E;
  if (!pr_A) {
    pr_A = prof_register("xchg_bidx", 1., 0, 0);
    pr_B = prof_register("xchg_scan", 1., 0, 0);
    pr_C = prof_register("xchg_reorder_send", 1., 0, 0);
    pr_D = prof_register("xchg_from_dev", 1., 0, 0);
    pr_E = prof_register("xchg_pre", 1., 0, 0);
  }

  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda->bnd_cnt = calloc(cuda->nr_blocks, sizeof(*cuda->bnd_cnt));
  }

  prof_start(pr_A);
  //  cuda_mprts_find_block_indices_2(cuda_mprts);
  prof_stop(pr_A);
  
  prof_start(pr_B);
  cuda_mprts_scan_send_buf(cuda_mprts);
  prof_stop(pr_B);
  
  prof_start(pr_C);
  cuda_mprts_reorder_send_buf(cuda_mprts);
  prof_stop(pr_C);
  
  prof_start(pr_D);
  cuda_mprts_copy_from_dev(cuda_mprts);
  prof_stop(pr_D);
  
  prof_start(pr_E);
  mprts_exchange_particles_pre(bnd, cuda_mprts);
  prof_stop(pr_E);
}

// ----------------------------------------------------------------------
// xchg_post

static void
xchg_post(struct psc_bnd *bnd, struct cuda_mprts *cuda_mprts)
{
  static int pr_A, pr_B, pr_C, pr_D, pr_E;
  if (!pr_A) {
    pr_A = prof_register("xchg_append", 1., 0, 0);
    pr_B = prof_register("xchg_to_dev", 1., 0, 0);
    pr_C = prof_register("xchg_bidx", 1., 0, 0);
    pr_D = prof_register("xchg_sort", 1., 0, 0);
    pr_E = prof_register("xchg_reorder", 1., 0, 0);
  }

  prof_start(pr_A);
  mprts_append_recvd(bnd, cuda_mprts);
  prof_stop(pr_A);

  prof_start(pr_B);
  cuda_mprts_copy_to_dev(cuda_mprts);
  prof_stop(pr_B);

  prof_start(pr_C);
  cuda_mprts_find_block_indices_3(cuda_mprts);
  prof_stop(pr_C);
  
  prof_start(pr_D);
  cuda_mprts_sort(cuda_mprts);
  prof_stop(pr_D);
  
  prof_start(pr_E);
  cuda_mprts_reorder(cuda_mprts);
  prof_stop(pr_E);
  
  cuda_mprts_free(cuda_mprts);
}

static struct cuda_mprts cuda_mprts; // FIXME!

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_prep

static void
psc_bnd_sub_exchange_particles_prep(struct psc_bnd *bnd, struct psc_particles *prts)
{
  cuda_mprts_create_single(&cuda_mprts, prts);
  xchg_prep(bnd, &cuda_mprts);
  cuda_mprts_destroy(&cuda_mprts);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_post

static void
psc_bnd_sub_exchange_particles_post(struct psc_bnd *bnd, struct psc_particles *prts)
{
  cuda_mprts_create_single(&cuda_mprts, prts);
  xchg_post(bnd, &cuda_mprts);
  cuda_mprts_destroy(&cuda_mprts);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_mprts_prep

static void
psc_bnd_sub_exchange_mprts_prep(struct psc_bnd *bnd,
				struct psc_mparticles *mprts)
{
  cuda_mprts_create(&cuda_mprts, mprts);
  xchg_prep(bnd, &cuda_mprts);
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_mprts_post

static void
psc_bnd_sub_exchange_mprts_post(struct psc_bnd *bnd,
				struct psc_mparticles *mprts)
{
  xchg_post(bnd, &cuda_mprts);
  cuda_mprts_destroy(&cuda_mprts);
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
    cuda_find_block_indices(prts, cuda->h_dev->bidx);
    prof_stop(pr_F);

    prof_start(pr_G);
    sort_pairs_device_2(cuda->sort_ctx, cuda->h_dev->bidx,
			cuda->h_dev->alt_ids,
			prts->n_part,
			cuda->h_dev->offsets);
    prof_stop(pr_G);

    prof_start(pr_H);
    cuda_reorder(prts, cuda->h_dev->alt_ids);
    prof_stop(pr_H);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_sub_exchange_particles_v1

static void __unused
psc_bnd_sub_exchange_particles_v1(struct psc_bnd *bnd,
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
  psc_bnd_sub_exchange_mprts_prep(bnd, particles);
  prof_stop(pr_A);

  prof_start(pr_B);
  ddc_particles_comm(ddcp, particles);
  prof_stop(pr_B);

  prof_start(pr_C);
  psc_bnd_sub_exchange_mprts_post(bnd, particles);
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

