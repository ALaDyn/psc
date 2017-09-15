
#undef _GLIBCXX_USE_INT128

#include "cuda_mparticles.h"

#include "cuda_sort2.h"
#include "particles_cuda.h"
#include "psc_bnd_cuda.h"
#include "psc_particles_as_cuda.h"

#define PFX(x) xchg_##x
#include "constants.c"

#if 0
// FIXME const mem for dims?
// FIXME probably should do our own loop rather than use blockIdx

__global__ static void
exchange_particles(int n_part, particles_cuda_dev_t h_dev,
		   int ldimsx, int ldimsy, int ldimsz)
{
  int ldims[3] = { ldimsx, ldimsy, ldimsz };
  int xm[3];

  for (int d = 0; d < 3; d++) {
    xm[d] = ldims[d] / d_consts.dxi[d];
  }

  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    particle_cuda_real_t xi[3] = {
      h_dev.xi4[i].x * d_consts.dxi[0],
      h_dev.xi4[i].y * d_consts.dxi[1],
      h_dev.xi4[i].z * d_consts.dxi[2] };
    int pos[3];
    for (int d = 0; d < 3; d++) {
      pos[d] = __float2int_rd(xi[d]);
    }
    if (pos[1] < 0) {
      h_dev.xi4[i].y += xm[1];
      if (h_dev.xi4[i].y >= xm[1])
	h_dev.xi4[i].y = 0.f;
    }
    if (pos[2] < 0) {
      h_dev.xi4[i].z += xm[2];
      if (h_dev.xi4[i].z >= xm[2])
	h_dev.xi4[i].z = 0.f;
    }
    if (pos[1] >= ldims[1]) {
      h_dev.xi4[i].y -= xm[1];
    }
    if (pos[2] >= ldims[2]) {
      h_dev.xi4[i].z -= xm[2];
    }
  }
}

EXTERN_C void
cuda_exchange_particles(int p, struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  struct psc_patch *patch = &ppsc->patch[p];

  xchg_set_constants(prts, NULL);

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     exchange_particles, (prts->n_part, *cuda->h_dev,
				  patch->ldims[0], patch->ldims[1], patch->ldims[2]));
}
#endif

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_2_total
//
// like cuda_find_block_indices, but handles out-of-bound
// particles

__global__ static void
mprts_find_block_indices_2_total(struct cuda_params prm, float4 *d_xi4,
				 unsigned int *d_off,
				 unsigned int *d_bidx, int nr_patches)
{
  int tid = threadIdx.x;

  int block_pos[3];
  block_pos[1] = blockIdx.x;
  block_pos[2] = blockIdx.y % prm.b_mx[2];
  int bid = block_pos_to_block_idx(block_pos, prm.b_mx);
  int p = blockIdx.y / prm.b_mx[2];

  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];

  // FIXME/OPT, could be done better like reorder_send_buf
  int block_begin = d_off[bid + p * nr_blocks];
  int block_end   = d_off[bid + p * nr_blocks + 1];

  for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
    float4 xi4 = d_xi4[n];
    unsigned int block_pos_y = __float2int_rd(xi4.y * prm.b_dxi[1]);
    unsigned int block_pos_z = __float2int_rd(xi4.z * prm.b_dxi[2]);

    int block_idx;
    if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
      block_idx = nr_blocks * nr_patches;
    } else {
      block_idx = block_pos_z * prm.b_mx[1] + block_pos_y + p * nr_blocks;
    }
    d_bidx[n] = block_idx;
  }
}

EXTERN_C void
cuda_mprts_find_block_indices_2_total(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  if (mprts->nr_patches == 0) {
    return;
  }

  struct cuda_params prm;
  set_params(&prm, ppsc, cmprts, NULL);
    
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { prm.b_mx[1], prm.b_mx[2] * mprts->nr_patches };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_find_block_indices_2_total, (prm, cmprts->d_xi4, cmprts->d_off,
						cmprts->d_bidx, mprts->nr_patches));
  free_params(&prm);
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_keys

__global__ static void
mprts_find_block_keys(struct cuda_params prm, float4 *d_xi4,
		      unsigned int *d_off,
		      unsigned int *d_bidx, int nr_total_blocks)
{
  int tid = threadIdx.x;
  int bid = blockIdx.x;

  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];
  int p = bid / nr_blocks;

  int block_begin = d_off[bid];
  int block_end   = d_off[bid + 1];

  for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
    float4 xi4 = d_xi4[n];
    unsigned int block_pos_y = __float2int_rd(xi4.y * prm.b_dxi[1]);
    unsigned int block_pos_z = __float2int_rd(xi4.z * prm.b_dxi[2]);

    int block_idx;
    if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
      block_idx = CUDA_BND_S_OOB;
    } else {
      int bidx = block_pos_z * prm.b_mx[1] + block_pos_y + p * nr_blocks;
      int b_diff = bid - bidx + prm.b_mx[1] + 1;
      int d1 = b_diff % prm.b_mx[1];
      int d2 = b_diff / prm.b_mx[1];
      block_idx = d2 * 3 + d1;
    }
    d_bidx[n] = block_idx;
  }
}

EXTERN_C void
cuda_mprts_find_block_keys(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  if (mprts->nr_patches == 0) {
    return;
  }

  struct cuda_params prm;
  set_params(&prm, ppsc, cmprts, NULL);
    
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { cmprts->n_blocks, 1 };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_find_block_keys, (prm, cmprts->d_xi4, cmprts->d_off,
				     cmprts->d_bidx, cmprts->n_blocks));
  free_params(&prm);
}

// ======================================================================
// cuda_mprts_find_block_indices_3

EXTERN_C void
cuda_mprts_find_block_indices_3(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  unsigned int nr_recv = cmprts->bnd.n_prts_recv;
  unsigned int nr_prts_prev = cmprts->n_prts - nr_recv;

  // for consistency, use same block indices that we counted earlier
  // OPT unneeded?
  check(cudaMemcpy(cmprts->d_bidx + nr_prts_prev, cmprts->bnd.h_bnd_idx,
		   nr_recv * sizeof(*cmprts->d_bidx),
		   cudaMemcpyHostToDevice));
  // slight abuse of the now unused last part of spine_cnts
  check(cudaMemcpy(cmprts->bnd.d_bnd_spine_cnts + 10 * cmprts->n_blocks,
		   cmprts->bnd.h_bnd_cnt,
		   cmprts->n_blocks * sizeof(*cmprts->bnd.d_bnd_spine_cnts),
		   cudaMemcpyHostToDevice));
  check(cudaMemcpy(cmprts->bnd.d_alt_bidx + nr_prts_prev, cmprts->bnd.h_bnd_off,
		   nr_recv * sizeof(*cmprts->bnd.d_alt_bidx),
		   cudaMemcpyHostToDevice));

  free(cmprts->bnd.h_bnd_idx);
  free(cmprts->bnd.h_bnd_off);
}

// ======================================================================
// mprts_reorder_send_buf_total

__global__ static void
mprts_reorder_send_buf_total(int nr_prts, int nr_total_blocks,
			     unsigned int *d_bidx, unsigned int *d_sums,
			     float4 *d_xi4, float4 *d_pxi4,
			     float4 *d_xchg_xi4, float4 *d_xchg_pxi4)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i >= nr_prts)
    return;

  if (d_bidx[i] == CUDA_BND_S_OOB) {
    int j = d_sums[i];
    d_xchg_xi4[j]  = d_xi4[i];
    d_xchg_pxi4[j] = d_pxi4[i];
  }
}

EXTERN_C void
cuda_mprts_reorder_send_buf_total(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  if (mprts->nr_patches == 0)
    return;

  float4 *xchg_xi4 = cmprts->d_xi4 + cmprts->n_prts;
  float4 *xchg_pxi4 = cmprts->d_pxi4 + cmprts->n_prts;
  assert(cmprts->n_prts + cmprts->bnd.n_prts_send < cmprts->n_alloced);
  
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (cmprts->n_prts + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_reorder_send_buf_total, (cmprts->n_prts, cmprts->n_blocks,
					    cmprts->d_bidx, cmprts->bnd.d_sums,
					    cmprts->d_xi4, cmprts->d_pxi4,
					    xchg_xi4, xchg_pxi4));
}

// ======================================================================
// cuda_mprts_copy_from_dev

void
cuda_mprts_copy_from_dev(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  if (mprts->nr_patches == 0) {
    return;
  }

  cmprts->bnd.h_bnd_xi4 = new float4[cmprts->bnd.n_prts_send];
  cmprts->bnd.h_bnd_pxi4 = new float4[cmprts->bnd.n_prts_send];

  assert(cmprts->n_prts + cmprts->bnd.n_prts_send < cmprts->n_alloced);

  check(cudaMemcpy(cmprts->bnd.h_bnd_xi4, cmprts->d_xi4 + cmprts->n_prts,
		   cmprts->bnd.n_prts_send * sizeof(float4), cudaMemcpyDeviceToHost));
  check(cudaMemcpy(cmprts->bnd.h_bnd_pxi4, cmprts->d_pxi4 + cmprts->n_prts,
		   cmprts->bnd.n_prts_send * sizeof(float4), cudaMemcpyDeviceToHost));
}

//======================================================================
// cuda_mprts_convert_from_cuda

void
cuda_mprts_convert_from_cuda(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  if (mprts->nr_patches == 0) {
    return;
  }

  float4 *bnd_xi4 = cmprts->bnd.h_bnd_xi4;
  float4 *bnd_pxi4 = cmprts->bnd.h_bnd_pxi4;
  for (int p = 0; p < mprts->nr_patches; p++) {
    cmprts->bnd.bpatch[p].prts = new particle_t[cmprts->bnd.bpatch[p].n_send];
    for (int n = 0; n < cmprts->bnd.bpatch[p].n_send; n++) {
      particle_t *prt = &cmprts->bnd.bpatch[p].prts[n];
      prt->xi      = bnd_xi4[n].x;
      prt->yi      = bnd_xi4[n].y;
      prt->zi      = bnd_xi4[n].z;
      prt->kind    = cuda_float_as_int(bnd_xi4[n].w);
      prt->pxi     = bnd_pxi4[n].x;
      prt->pyi     = bnd_pxi4[n].y;
      prt->pzi     = bnd_pxi4[n].z;
      prt->qni_wni = bnd_pxi4[n].w;
    }
    bnd_xi4 += cmprts->bnd.bpatch[p].n_send;
    bnd_pxi4 += cmprts->bnd.bpatch[p].n_send;
  }
  delete[] cmprts->bnd.h_bnd_xi4;
  delete[] cmprts->bnd.h_bnd_pxi4;
}

// ======================================================================
// cuda_mprts_copy_to_dev

void
cuda_mprts_copy_to_dev(struct psc_mparticles *mprts)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  float4 *d_xi4 = cmprts->d_xi4;
  float4 *d_pxi4 = cmprts->d_pxi4;

  unsigned int nr_recv = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    nr_recv += cmprts->bnd.bpatch[p].n_recv;
  }
  assert(cmprts->n_prts + nr_recv <= cmprts->n_alloced);

  check(cudaMemcpy(d_xi4 + cmprts->n_prts, cmprts->bnd.h_bnd_xi4,
		   nr_recv * sizeof(*d_xi4),
		   cudaMemcpyHostToDevice));
  check(cudaMemcpy(d_pxi4 + cmprts->n_prts, cmprts->bnd.h_bnd_pxi4,
		   nr_recv * sizeof(*d_pxi4),
		   cudaMemcpyHostToDevice));

  free(cmprts->bnd.h_bnd_xi4);
  free(cmprts->bnd.h_bnd_pxi4);

  cmprts->bnd.n_prts_recv = nr_recv;
  cmprts->n_prts += nr_recv;
}

// ======================================================================
// cuda_mprts_sort

void
cuda_mprts_sort(struct psc_mparticles *mprts, int *n_prts_by_patch)
{
  struct cuda_mparticles *cmprts = psc_mparticles_cuda(mprts)->cmprts;

  cuda_mprts_sort_pairs_device(mprts);

  for (int p = 0; p < mprts->nr_patches; p++) {
    n_prts_by_patch[p] += cmprts->bnd.bpatch[p].n_recv - cmprts->bnd.bpatch[p].n_send;
  }
  cmprts->n_prts -= cmprts->bnd.n_prts_send;
}

