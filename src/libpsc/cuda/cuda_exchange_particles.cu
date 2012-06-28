
#include <psc_cuda.h>
#include "cuda_sort2.h"
#include "particles_cuda.h"
#include "psc_bnd_cuda.h"

#include <thrust/scan.h>
#include <thrust/device_vector.h>

#define PFX(x) xchg_##x
#include "constants.c"

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
      pos[d] = cuda_fint(xi[d]);
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

// ======================================================================
// cuda_find_block_indices

__global__ static void
find_block_indices(int n_part, particles_cuda_dev_t h_dev, unsigned int *d_bidx,
		   int dimy, float b_dyi, float b_dzi, int b_my, int b_mz)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    float4 xi4 = h_dev.xi4[i];
    unsigned int block_pos_y = cuda_fint(xi4.y * b_dyi);
    unsigned int block_pos_z = cuda_fint(xi4.z * b_dzi);

    int block_idx = block_pos_z * b_my + block_pos_y;
    d_bidx[i] = block_idx;
  }
}

EXTERN_C void
cuda_find_block_indices(struct psc_particles *prts, unsigned int *d_bidx)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     find_block_indices, (prts->n_part, *cuda->h_dev, d_bidx,
				  cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				  cuda->b_mx[1], cuda->b_mx[2]));
}

// ======================================================================
// cuda_find_block_indices_ids

__global__ static void
find_block_indices_ids(int n_part, particles_cuda_dev_t h_dev, unsigned int *d_bidx,
		       unsigned int *d_ids, int dimy, float b_dyi, float b_dzi, int b_my, int b_mz)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    float4 xi4 = h_dev.xi4[i];
    unsigned int block_pos_y = cuda_fint(xi4.y * b_dyi);
    unsigned int block_pos_z = cuda_fint(xi4.z * b_dzi);

    int block_idx = block_pos_z * b_my + block_pos_y;
    d_bidx[i] = block_idx;
    d_ids[i] = i;
  }
}

EXTERN_C void
cuda_find_block_indices_ids(struct psc_particles *prts, unsigned int *d_bidx,
			    unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     find_block_indices_ids, (prts->n_part, *cuda->h_dev, d_bidx, d_ids,
				      cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				      cuda->b_mx[1], cuda->b_mx[2]));
}

// ======================================================================
// cuda_find_block_indices_2
//
// like cuda_find_block_indices, but handles out-of-bound
// particles

__global__ static void
find_block_indices_2(int n_part, particles_cuda_dev_t h_dev, unsigned int *d_bidx,
		     int dimy, float b_dyi, float b_dzi,
		     int b_my, int b_mz, int start)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x + start;
  if (i < n_part) {
    float4 xi4 = h_dev.xi4[i];
    unsigned int block_pos_y = cuda_fint(xi4.y * b_dyi);
    unsigned int block_pos_z = cuda_fint(xi4.z * b_dzi);

    int block_idx;
    if (block_pos_y >= b_my || block_pos_z >= b_mz) {
      block_idx = b_my * b_mz;
    } else {
      block_idx = block_pos_z * b_my + block_pos_y;
    }
    d_bidx[i] = block_idx;
  }
}

EXTERN_C void
cuda_find_block_indices_2(struct psc_particles *prts, unsigned int *d_bidx,
			  int start)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { ((prts->n_part - start) + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     find_block_indices_2, (prts->n_part, *cuda->h_dev, d_bidx,
				    cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				    cuda->b_mx[1], cuda->b_mx[2], start));
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_2

__global__ static void
mprts_find_block_indices_2(struct cuda_params prm, particles_cuda_dev_t *d_cp_prts,
			   unsigned int *d_bidx, int nr_patches)
{
  int tid = threadIdx.x;

  int block_pos[3];
  block_pos[1] = blockIdx.x;
  block_pos[2] = blockIdx.y;
  int bid = block_pos_to_block_idx(block_pos, prm.b_mx);

  unsigned int off = 0;
  for (int p = 0; p < nr_patches; p++) {
    int block_begin = d_cp_prts[p].offsets[bid];
    int block_end   = d_cp_prts[p].offsets[bid+1];

    for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
      float4 xi4 = d_cp_prts[p].xi4[n];
      unsigned int block_pos_y = cuda_fint(xi4.y * prm.b_dxi[1]);
      unsigned int block_pos_z = cuda_fint(xi4.z * prm.b_dxi[2]);
      
      int block_idx;
      if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
	block_idx = prm.b_mx[1] * prm.b_mx[2];
      } else {
	block_idx = block_pos_z * prm.b_mx[1] + block_pos_y;
      }
      d_bidx[off + n] = block_idx;
    }
    off += d_cp_prts[p].n_part;
  }
}

EXTERN_C void
cuda_mprts_find_block_indices_2(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }
  struct cuda_params prm;
  set_params(&prm, ppsc, psc_mparticles_get_patch(mprts, 0), NULL);
  
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { prm.b_mx[1], prm.b_mx[2] };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_find_block_indices_2, (prm, mprts_cuda->d_dev,
					  mprts_cuda->d_bidx, mprts->nr_patches));
  free_params(&prm);
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_2_total

__global__ static void
mprts_find_block_indices_2_total(struct cuda_params prm, particles_cuda_dev_t *d_cp_prts,
				 unsigned int *d_bidx, int nr_patches)
{
  int tid = threadIdx.x;

  int block_pos[3];
  block_pos[1] = blockIdx.x;
  block_pos[2] = blockIdx.y % prm.b_mx[2];
  int bid = block_pos_to_block_idx(block_pos, prm.b_mx);
  int p = blockIdx.y / prm.b_mx[2];

  // FIXME/OPT, could be done better like reorder_send_buf
  int block_begin = d_cp_prts[p].d_off[bid];
  int block_end   = d_cp_prts[p].d_off[bid+1];

  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];

  for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
    float4 xi4 = d_cp_prts[0].xi4[n];
    unsigned int block_pos_y = cuda_fint(xi4.y * prm.b_dxi[1]);
    unsigned int block_pos_z = cuda_fint(xi4.z * prm.b_dxi[2]);

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
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }

  struct cuda_params prm;
  set_params(&prm, ppsc, psc_mparticles_get_patch(mprts, 0), NULL);
    
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { prm.b_mx[1], prm.b_mx[2] * mprts->nr_patches };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_find_block_indices_2_total, (prm, psc_mparticles_cuda(mprts)->d_dev,
						mprts_cuda->d_bidx, mprts->nr_patches));
  free_params(&prm);
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_ids_total

__global__ static void
mprts_find_block_indices_ids_total(struct cuda_params prm, particles_cuda_dev_t *d_cp_prts,
				   unsigned int *d_bidx, unsigned int *d_ids, int nr_patches)
{
  int n = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];

  unsigned int off = 0;
  for (int p = 0; p < nr_patches; p++) {
    if (n < d_cp_prts[p].n_part) {
      float4 xi4 = d_cp_prts[0].xi4[n + off];
      unsigned int block_pos_y = cuda_fint(xi4.y * prm.b_dxi[1]);
      unsigned int block_pos_z = cuda_fint(xi4.z * prm.b_dxi[2]);
      
      int block_idx;
      if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
	block_idx = -1; // not supposed to happen here!
      } else {
	block_idx = block_pos_z * prm.b_mx[1] + block_pos_y + p * nr_blocks;
      }
      d_bidx[n + off] = block_idx;
      d_ids[n + off] = n + off;
    }
    off += d_cp_prts[p].n_part;
  }
}

EXTERN_C void
cuda_mprts_find_block_indices_ids_total(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }

  int max_n_part = 0;
  mprts_cuda->nr_prts_send = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    mprts_cuda->nr_prts_send += cuda->bnd_n_send;
    if (prts->n_part > max_n_part) {
      max_n_part = prts->n_part;
    }
  }

  struct cuda_params prm;
  set_params(&prm, ppsc, psc_mparticles_get_patch(mprts, 0), NULL);
    
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (max_n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };

  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_find_block_indices_ids_total, (prm, psc_mparticles_cuda(mprts)->d_dev,
						  mprts_cuda->d_bidx, mprts_cuda->d_ids,
						  mprts->nr_patches));
  free_params(&prm);
}

// ----------------------------------------------------------------------

EXTERN_C void
_cuda_find_block_indices_2(struct psc_particles *prts, unsigned int *d_bidx,
			   int start)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *xi4 = new float4[prts->n_part];
  float4 *pxi4 = new float4[prts->n_part];
  unsigned int *bidx = new unsigned int[prts->n_part];
  __particles_cuda_from_device(prts, xi4, pxi4);
  cuda_copy_bidx_from_dev(prts, bidx, d_bidx);

  float b_dyi = cuda->b_dxi[1], b_dzi = cuda->b_dxi[2];
  int b_my = cuda->b_mx[1], b_mz = cuda->b_mx[2];
  for (int i = start; i < prts->n_part; i++) {
    unsigned int block_pos_y = cuda_fint(xi4[i].y * b_dyi);
    unsigned int block_pos_z = cuda_fint(xi4[i].z * b_dzi);

    int block_idx;
    if (block_pos_y >= b_my || block_pos_z >= b_mz) {
      block_idx = cuda->nr_blocks;
    } else {
      block_idx = block_pos_z * b_my + block_pos_y;
    }
    bidx[i] = block_idx;
  }
  
  cuda_copy_bidx_to_dev(prts, d_bidx, bidx);
  delete[] xi4;
  delete[] pxi4;
  delete[] bidx;
}

// ======================================================================
// cuda_find_block_indices_3

EXTERN_C void
cuda_find_block_indices_3(struct psc_particles *prts, unsigned int *d_bidx,
			  unsigned int *d_alt_bidx,
			  int start, unsigned int *bn_idx, unsigned int *bn_off)
{
  // for consistency, use same block indices that we counted earlier
  check(cudaMemcpy(d_bidx + start, bn_idx, (prts->n_part - start) * sizeof(*d_bidx),
		   cudaMemcpyHostToDevice));
  // abuse of alt_bidx!!! FIXME
  check(cudaMemcpy(d_alt_bidx + start, bn_off, (prts->n_part - start) * sizeof(*d_bidx),
		   cudaMemcpyHostToDevice));
}

// ======================================================================
// cuda_find_block_indices_3

EXTERN_C void
cuda_mprts_find_block_indices_3(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    unsigned int start = cuda->bnd_n_part_save;
    // for consistency, use same block indices that we counted earlier
    check(cudaMemcpy(mprts_cuda->d_bidx + off + start, cuda->bnd_idx,
		     (prts->n_part - start) * sizeof(*mprts_cuda->d_bidx),
		     cudaMemcpyHostToDevice));
    // abuse of alt_bidx!!! FIXME
    check(cudaMemcpy(mprts_cuda->d_alt_bidx + off + start, cuda->bnd_off,
		     (prts->n_part - start) * sizeof(*mprts_cuda->d_alt_bidx),
		     cudaMemcpyHostToDevice));
    off += cuda->n_alloced;
  }
}

// ======================================================================
// reorder_send_buf

__global__ static void
reorder_send_buf(int n_part, particles_cuda_dev_t h_dev, unsigned int *d_bidx,
		 unsigned int *d_sums, unsigned int nr_blocks)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    if (d_bidx[i] == nr_blocks) {
      int j = d_sums[i] + n_part;
      h_dev.xi4[j] = h_dev.xi4[i];
      h_dev.pxi4[j] = h_dev.pxi4[i];
    }
  }
}

EXTERN_C void
cuda_reorder_send_buf(int p, struct psc_particles *prts, 
		      unsigned int *d_bidx, unsigned int *d_sums, int n_send)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  assert(prts->n_part + n_send <= cuda->n_alloced);

  // OPT: don't pass offset, get it in device code
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     reorder_send_buf, (prts->n_part, *cuda->h_dev, d_bidx, d_sums, cuda->nr_blocks));
}

__global__ static void
mprts_reorder_send_buf_total(int nr_prts, int nr_oob, unsigned int *d_bidx, unsigned int *d_sums,
			     float4 *d_xi4, float4 *d_pxi4,
			     float4 *d_xchg_xi4, float4 *d_xchg_pxi4)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i >= nr_prts)
    return;

  if (d_bidx[i] == nr_oob) {
    int j = d_sums[i];
    d_xchg_xi4[j]  = d_xi4[i];
    d_xchg_pxi4[j] = d_pxi4[i];
  }
}

EXTERN_C void
cuda_mprts_reorder_send_buf_total(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);
  if (mprts->nr_patches == 0)
    return;

  struct psc_particles_cuda *cuda = psc_particles_cuda(psc_mparticles_get_patch(mprts, 0));
  
  mprts_cuda->nr_prts_send = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    mprts_cuda->nr_prts_send += cuda->bnd_n_send;
  }

  float4 *xchg_xi4 = mprts_cuda->d_xi4 + mprts_cuda->nr_prts;
  float4 *xchg_pxi4 = mprts_cuda->d_pxi4 + mprts_cuda->nr_prts;
  assert(mprts_cuda->nr_prts + mprts_cuda->nr_prts_send < mprts_cuda->nr_alloced);
  int nr_oob = cuda->nr_blocks * mprts->nr_patches;
  
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (mprts_cuda->nr_prts + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_reorder_send_buf_total, (mprts_cuda->nr_prts, nr_oob,
					    mprts_cuda->d_bidx, mprts_cuda->d_sums,
					    mprts_cuda->d_xi4, mprts_cuda->d_pxi4,
					    xchg_xi4, xchg_pxi4));
}

EXTERN_C void
_cuda_reorder_send_buf(int p, struct psc_particles *prts, 
		       unsigned int *d_bidx, unsigned int *d_sums, int n_send)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  int n_part = prts->n_part;
  int n_total = n_part + n_send;
  assert(n_total <= cuda->n_alloced);
  float4 *xi4 = new float4[n_total];
  float4 *pxi4 = new float4[n_total];
  unsigned int *bidx = new unsigned int[n_total];
  unsigned int *sums = new unsigned int[n_total];
  __particles_cuda_from_device(prts, xi4, pxi4);
  cuda_copy_bidx_from_dev(prts, bidx, d_bidx);
  cuda_copy_bidx_from_dev(prts, sums, d_sums);

  for (int i = 0; i < prts->n_part; i++) {
    if (bidx[i] == cuda->nr_blocks) {
      int j = sums[i] + prts->n_part;
      xi4[j] = xi4[i];
      pxi4[j] = pxi4[i];
    }
  }

  prts->n_part = n_total;
  __particles_cuda_to_device(prts, xi4, pxi4, NULL);
  prts->n_part = n_part;
  delete[] xi4;
  delete[] pxi4;
  delete[] bidx;
  delete[] sums;
}

// ======================================================================

static void
psc_particles_cuda_swap_alt(struct psc_particles *prts)
{
  // FIXME (eventually)
  // this function should not exist, since mprts needs to be swapped, too,
  // but isn't available here.
  // but due to sorting in copy_from/to, it's inevitable, so we fix up 
  // the mprts pointers elsewhere for nwo
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  float4 *alt_xi4 = cuda->h_dev->alt_xi4;
  float4 *alt_pxi4 = cuda->h_dev->alt_pxi4;
  cuda->h_dev->alt_xi4 = cuda->h_dev->xi4;
  cuda->h_dev->alt_pxi4 = cuda->h_dev->pxi4;
  cuda->h_dev->xi4 = alt_xi4;
  cuda->h_dev->pxi4 = alt_pxi4;
}

static void
psc_mparticles_cuda_swap_alt(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    psc_particles_cuda_swap_alt(prts);
  }
  float4 *tmp_xi4 = mprts_cuda->d_alt_xi4;
  float4 *tmp_pxi4 = mprts_cuda->d_alt_pxi4;
  mprts_cuda->d_alt_xi4 = mprts_cuda->d_xi4;
  mprts_cuda->d_alt_pxi4 = mprts_cuda->d_pxi4;
  mprts_cuda->d_xi4 = tmp_xi4;
  mprts_cuda->d_pxi4 = tmp_pxi4;
}

// ======================================================================
// reorder_and_offsets

__global__ static void
reorder_and_offsets(int n_part, particles_cuda_dev_t h_dev, float4 *xi4, float4 *pxi4,
		    unsigned int *d_bidx, unsigned int *d_ids, int nr_blocks)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i > n_part)
    return;

  int block, prev_block;
  if (i < n_part) {
    xi4[i] = h_dev.xi4[d_ids[i]];
    pxi4[i] = h_dev.pxi4[d_ids[i]];
    
    block = d_bidx[i];
  } else if (i == n_part) { // needed if there is no particle in the last block
    block = nr_blocks;
  }

  // create offsets per block into particle array
  prev_block = -1;
  if (i > 0) {
    prev_block = d_bidx[i-1];
  }
  for (int b = prev_block + 1; b <= block; b++) {
    h_dev.offsets[b] = i;
  }
}

EXTERN_C void
cuda_reorder_and_offsets(struct psc_particles *prts, unsigned int *d_bidx,
			 unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *alt_xi4 = cuda->h_dev->alt_xi4;
  float4 *alt_pxi4 = cuda->h_dev->alt_pxi4;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     reorder_and_offsets, (prts->n_part, *cuda->h_dev, alt_xi4, alt_pxi4,
				   d_bidx, d_ids, cuda->nr_blocks));

  psc_particles_cuda_swap_alt(prts);
}

void
_cuda_reorder_and_offsets(struct psc_particles *prts, unsigned int *d_bidx,
			  unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *xi4 = new float4[prts->n_part];
  float4 *pxi4 = new float4[prts->n_part];
  float4 *alt_xi4 = new float4[prts->n_part];
  float4 *alt_pxi4 = new float4[prts->n_part];
  unsigned int *bidx = new unsigned int[prts->n_part];
  unsigned int *ids = new unsigned int[prts->n_part];
  int *offsets = new int[cuda->nr_blocks + 2];

  __particles_cuda_from_device(prts, xi4, pxi4);
  cuda_copy_bidx_from_dev(prts, bidx, d_bidx);
  cuda_copy_bidx_from_dev(prts, ids, d_ids);

  for (int i = 0; i < prts->n_part; i++) {
    alt_xi4[i] = xi4[ids[i]];
    alt_pxi4[i] = pxi4[ids[i]];

    int block = bidx[i];
    int prev_block = (i > 0) ? (int) bidx[i-1] : -1;
    for (int b = prev_block + 1; b <= block; b++) {
      offsets[b] = i;
    }
  }
  int block = cuda->nr_blocks + 1;
  int prev_block = bidx[prts->n_part - 1];
  for (int b = prev_block + 1; b <= block; b++) {
    offsets[b] = prts->n_part;
  }

  psc_particles_cuda_swap_alt(prts);

  __particles_cuda_to_device(prts, alt_xi4, alt_pxi4, offsets);
  delete[] xi4;
  delete[] pxi4;
  delete[] alt_xi4;
  delete[] alt_pxi4;
  delete[] bidx;
  delete[] ids;
  delete[] offsets;
}

__global__ static void
mprts_reorder_and_offsets(int nr_prts, float4 *xi4, float4 *pxi4, float4 *alt_xi4, float4 *alt_pxi4,
			  unsigned int *d_bidx, unsigned int *d_ids, unsigned int *d_off, int last_block)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i > nr_prts)
    return;

  int block, prev_block;
  if (i < nr_prts) {
    alt_xi4[i] = xi4[d_ids[i]];
    alt_pxi4[i] = pxi4[d_ids[i]];
    
    block = d_bidx[i];
  } else { // needed if there is no particle in the last block
    block = last_block;
  }

  // OPT: d_bidx[i-1] could use shmem
  // create offsets per block into particle array
  prev_block = -1;
  if (i > 0) {
    prev_block = d_bidx[i-1];
  }
  for (int b = prev_block + 1; b <= block; b++) {
    d_off[b] = i;
  }
}

void
cuda_mprts_reorder_and_offsets(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }
  int nr_blocks = psc_particles_cuda(psc_mparticles_get_patch(mprts, 0))->nr_blocks;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (mprts_cuda->nr_prts + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_reorder_and_offsets, (mprts_cuda->nr_prts, mprts_cuda->d_xi4, mprts_cuda->d_pxi4,
					 mprts_cuda->d_alt_xi4, mprts_cuda->d_alt_pxi4,
					 mprts_cuda->d_bidx, mprts_cuda->d_ids,
					 mprts_cuda->d_off, mprts->nr_patches * nr_blocks));

  psc_mparticles_cuda_swap_alt(mprts);
  psc_mparticles_cuda_copy_to_dev(mprts);
}

// ======================================================================
// cuda_reorder

__global__ static void
reorder(int n_part, particles_cuda_dev_t h_dev, float4 *xi4, float4 *pxi4,
	unsigned int *d_ids)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i < n_part) {
    xi4[i] = h_dev.xi4[d_ids[i]];
    pxi4[i] = h_dev.pxi4[d_ids[i]];
  }
}

EXTERN_C void
cuda_reorder(struct psc_particles *prts, unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *alt_xi4 = cuda->h_dev->alt_xi4;
  float4 *alt_pxi4 = cuda->h_dev->alt_pxi4;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     reorder, (prts->n_part, *cuda->h_dev, alt_xi4, alt_pxi4, d_ids));

  cuda->h_dev->alt_xi4 = cuda->h_dev->xi4;
  cuda->h_dev->alt_pxi4 = cuda->h_dev->pxi4;
  cuda->h_dev->xi4 = alt_xi4;
  cuda->h_dev->pxi4 = alt_pxi4;
}

// ======================================================================
// cuda_exclusive_scan

EXTERN_C int
_cuda_exclusive_scan(int p, struct psc_particles *prts,
		    unsigned int *d_vals, unsigned int *d_sums)
{
  unsigned int *vals = new unsigned int[prts->n_part];
  unsigned int *sums = new unsigned int[prts->n_part];
  cuda_copy_bidx_from_dev(prts, vals, d_vals);

  unsigned int sum = 0;
  for (int i = 0; i < prts->n_part; i++) {
    sums[i] = sum;
    sum += vals[i];
  }

  cuda_copy_bidx_to_dev(prts, d_sums, sums);
  delete[] sums;
  delete[] vals;
  return sum;
}

EXTERN_C int
cuda_exclusive_scan(int p, struct psc_particles *prts, unsigned int *_d_vals, unsigned int *_d_sums)
{
  thrust::device_ptr<unsigned int> d_vals(_d_vals);
  thrust::device_ptr<unsigned int> d_sums(_d_sums);
  thrust::exclusive_scan(d_vals, d_vals + prts->n_part, d_sums);
  int sum = d_sums[prts->n_part - 1] + d_vals[prts->n_part - 1];
  return sum;
}

// ======================================================================
// cuda_mprts_copy_from_dev

void
cuda_mprts_copy_from_dev(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }

  mprts_cuda->h_bnd_xi4 = new float4[mprts_cuda->nr_prts_send];
  mprts_cuda->h_bnd_pxi4 = new float4[mprts_cuda->nr_prts_send];

  check(cudaMemcpy(mprts_cuda->h_bnd_xi4, mprts_cuda->d_xi4 + mprts_cuda->nr_prts,
		   mprts_cuda->nr_prts_send * sizeof(float4), cudaMemcpyDeviceToHost));
  check(cudaMemcpy(mprts_cuda->h_bnd_pxi4, mprts_cuda->d_pxi4 + mprts_cuda->nr_prts,
		   mprts_cuda->nr_prts_send * sizeof(float4), cudaMemcpyDeviceToHost));
}

// ----------------------------------------------------------------------
// cuda_mprts_convert_from_cuda

void
cuda_mprts_convert_from_cuda(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }

  float4 *bnd_xi4 = mprts_cuda->h_bnd_xi4;
  float4 *bnd_pxi4 = mprts_cuda->h_bnd_pxi4;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    cuda->bnd_prts = new particle_single_t[cuda->bnd_n_send];
    for (int n = 0; n < cuda->bnd_n_send; n++) {
      particle_single_t *prt = &cuda->bnd_prts[n];
      prt->xi      = bnd_xi4[n].x;
      prt->yi      = bnd_xi4[n].y;
      prt->zi      = bnd_xi4[n].z;
      prt->kind    = cuda_float_as_int(bnd_xi4[n].w);
      prt->pxi     = bnd_pxi4[n].x;
      prt->pyi     = bnd_pxi4[n].y;
      prt->pzi     = bnd_pxi4[n].z;
      prt->qni_wni = bnd_pxi4[n].w;
    }
    bnd_xi4 += cuda->bnd_n_send;
    bnd_pxi4 += cuda->bnd_n_send;
  }
  delete[] mprts_cuda->h_bnd_xi4;
  delete[] mprts_cuda->h_bnd_pxi4;
}

// ======================================================================
// cuda_mprts_copy_to_dev

void
cuda_mprts_copy_to_dev(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  float4 *d_alt_xi4 = mprts_cuda->d_alt_xi4;
  float4 *d_alt_pxi4 = mprts_cuda->d_alt_pxi4;
  float4 *d_xi4 = mprts_cuda->d_xi4;
  float4 *d_pxi4 = mprts_cuda->d_pxi4;
  unsigned int *d_bidx = mprts_cuda->d_bidx;
  unsigned int *d_alt_bidx = mprts_cuda->d_alt_bidx;
  unsigned int *d_ids = mprts_cuda->d_ids;
  unsigned int *d_alt_ids = mprts_cuda->d_alt_ids;
  unsigned int *d_sums = mprts_cuda->d_sums;

 for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    assert(d_alt_xi4 + prts->n_part <= mprts_cuda->d_alt_xi4 + mprts_cuda->nr_alloced);
    check(cudaMemcpy(d_alt_xi4, cuda->h_dev->xi4,
		     cuda->bnd_n_part_save * sizeof(*cuda->h_dev->alt_xi4),
		     cudaMemcpyDeviceToDevice));
    check(cudaMemcpy(d_alt_xi4 + cuda->bnd_n_part_save, cuda->bnd_xi4,
		     (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_xi4),
		     cudaMemcpyHostToDevice));
    check(cudaMemcpy(d_alt_pxi4, cuda->h_dev->pxi4,
		     cuda->bnd_n_part_save * sizeof(*cuda->h_dev->alt_xi4),
		     cudaMemcpyDeviceToDevice));
    check(cudaMemcpy(d_alt_pxi4 + cuda->bnd_n_part_save, cuda->bnd_pxi4,
		     (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_pxi4),
		     cudaMemcpyHostToDevice));
    cuda->n_alloced = prts->n_part;
    cuda->h_dev->alt_xi4 = d_alt_xi4;
    cuda->h_dev->alt_pxi4 = d_alt_pxi4;
    cuda->h_dev->xi4 = d_xi4;
    cuda->h_dev->pxi4 = d_pxi4;
    d_alt_xi4 += cuda->n_alloced;
    d_alt_pxi4 += cuda->n_alloced;
    d_xi4 += cuda->n_alloced;
    d_pxi4 += cuda->n_alloced;
    d_bidx += cuda->n_alloced;
    d_alt_bidx += cuda->n_alloced;
    d_ids += cuda->n_alloced;
    d_alt_ids += cuda->n_alloced;
    d_sums += cuda->n_alloced;
  }
  psc_mparticles_cuda_swap_alt(mprts);
  psc_mparticles_cuda_copy_to_dev(mprts);
}

void
cuda_mprts_copy_to_dev_v1(struct psc_mparticles *mprts)
{
  cudaStream_t stream[mprts->nr_patches];
  for (int p = 0; p < mprts->nr_patches; p++) {
    cudaStreamCreate(&stream[p]);
  }
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    check(cudaMemcpyAsync(cuda->h_dev->xi4 + cuda->bnd_n_part_save, cuda->bnd_xi4,
			  (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_xi4),
			  cudaMemcpyHostToDevice, stream[p]));
    check(cudaMemcpyAsync(cuda->h_dev->pxi4 + cuda->bnd_n_part_save, cuda->bnd_pxi4,
			  (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_pxi4),
			  cudaMemcpyHostToDevice, stream[p]));
  }

  for (int p = 0; p < mprts->nr_patches; p++) {
    cudaStreamSynchronize(stream[p]);
    cudaStreamDestroy(stream[p]);
  }
}

// ======================================================================
// cuda_mprts_sort

void
cuda_mprts_sort(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);
  unsigned int *d_bidx = mprts_cuda->d_bidx;
  unsigned int *h_bidx = new unsigned int[mprts_cuda->nr_alloced];
  unsigned int *h_bidx_save = h_bidx;
  check(cudaMemcpy(h_bidx, d_bidx, mprts_cuda->nr_alloced * sizeof(float),
		   cudaMemcpyDeviceToHost));
  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    check(cudaMemcpy(d_bidx, h_bidx, cuda->n_alloced * sizeof(float),
		     cudaMemcpyHostToDevice));
    // OPT: when calculating bidx, do preprocess then
    void *sp = sort_pairs_3_create(cuda->b_mx);
    sort_pairs_3_device(sp, d_bidx, mprts_cuda->d_alt_bidx + off, mprts_cuda->d_alt_ids + off,
			prts->n_part, cuda->h_dev->offsets,
			cuda->bnd_n_part_save, cuda->bnd_cnt);
    sort_pairs_3_destroy(sp);
    //    cuda->h_dev->bidx = d_bidx;
    d_bidx += 122880;
    h_bidx += cuda->n_alloced;
    off += cuda->n_alloced;
  }
  delete[] h_bidx_save;

  unsigned int *h_alt_ids = new unsigned int[mprts_cuda->nr_alloced];
  check(cudaMemcpy(h_alt_ids, mprts_cuda->d_alt_ids, mprts_cuda->nr_alloced * sizeof(unsigned int),
		   cudaMemcpyDeviceToHost));

  off = 0;
  unsigned int off2 = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    assert(off == cuda->h_dev->xi4 - mprts_cuda->d_xi4);
    for (int n = 0; n < prts->n_part - cuda->bnd_n_send; n++) {
      h_alt_ids[off2 + n] = h_alt_ids[off + n] + off;
    }
    off += cuda->n_alloced;
    off2 += prts->n_part - cuda->bnd_n_send;
  }
  check(cudaMemcpy(mprts_cuda->d_alt_ids, h_alt_ids,
		   mprts_cuda->nr_alloced * sizeof(unsigned int),
		   cudaMemcpyHostToDevice));

  delete[] h_alt_ids;
  psc_mparticles_cuda_copy_to_dev(mprts);
}

// ======================================================================
// cuda_mprts_reorder

__global__ static void
mprts_reorder(int nr_prts, unsigned int *alt_ids,
	      float4 *xi4, float4 *pxi4,
	      float4 *alt_xi4, float4 *alt_pxi4)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i < nr_prts) {
    int j = alt_ids[i];
    alt_xi4[i] = xi4[j];
    alt_pxi4[i] = pxi4[j];
  }
}

void
cuda_mprts_reorder(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    prts->n_part -= cuda->bnd_n_send;
    cuda->h_dev->xi4 = mprts_cuda->d_xi4 + off;
    cuda->h_dev->pxi4 = mprts_cuda->d_pxi4 + off;
    cuda->h_dev->alt_xi4 = mprts_cuda->d_alt_xi4 + off;
    cuda->h_dev->alt_pxi4 = mprts_cuda->d_alt_pxi4 + off;
    cuda->n_alloced = prts->n_part;
    off += prts->n_part;
  }
  mprts_cuda->nr_prts = off;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (mprts_cuda->nr_prts + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     mprts_reorder, (mprts_cuda->nr_prts, mprts_cuda->d_alt_ids,
			     mprts_cuda->d_xi4, mprts_cuda->d_pxi4,
			     mprts_cuda->d_alt_xi4, mprts_cuda->d_alt_pxi4));
  
  psc_mparticles_cuda_swap_alt(mprts);
  cuda_mprts_find_off(mprts);
}

// ======================================================================
// cuda_mprts_check_ordered

void
cuda_mprts_check_ordered(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  psc_mparticles_cuda_copy_to_dev(mprts); // update n_part, particle pointers
  cuda_mprts_find_block_indices_2(mprts);

  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    unsigned int *bidx = new unsigned int[prts->n_part];
    cuda_copy_bidx_from_dev(prts, bidx, mprts_cuda->d_bidx + off);
    
    float4 *xi4 = new float4[prts->n_part];
    float4 *pxi4 = new float4[prts->n_part];
    __particles_cuda_from_device(prts, xi4, pxi4);

    unsigned int last = 0;
    for (int n = 0; n < prts->n_part; n++) {
      unsigned int block_pos_y = cuda_fint(xi4[n].y * cuda->b_dxi[1]);
      unsigned int block_pos_z = cuda_fint(xi4[n].z * cuda->b_dxi[2]);
      
      int block_idx;
      if (block_pos_y >= cuda->b_mx[1] || block_pos_z >= cuda->b_mx[2]) {
	block_idx = cuda->nr_blocks;
      } else {
	block_idx = block_pos_z * cuda->b_mx[1] + block_pos_y;
      }
      if (block_idx != bidx[n]) {
	mprintf("n = %d bidx = %d block_idx = %d bp [%d:%d] real [%g:%g]\n",
		n, bidx[n], block_idx, block_pos_y, block_pos_z,
		xi4[n].y * cuda->b_dxi[1], xi4[n].z * cuda->b_dxi[2]);
	static int error_cnt;
	assert(error_cnt++ < 10);
      }
      if (!(bidx[n] >= last && bidx[n] < cuda->nr_blocks)) {
	mprintf("n = %d bidx = %d last = %d\n", n, bidx[n], last);
	static int error_cnt;
	assert(error_cnt++ < 10);
      }
      last = block_idx;
    }

    delete[] bidx;
    delete[] xi4;
    delete[] pxi4;

    off += prts->n_part;
  }
}

// ======================================================================
// cuda_mprts_check_ordered_offsets

void
cuda_mprts_check_ordered_offsets(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  psc_mparticles_cuda_copy_to_dev(mprts); // update n_part, particle pointers
  cuda_mprts_find_block_indices_2(mprts);

  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    unsigned int *bidx = new unsigned int[prts->n_part];
    cuda_copy_bidx_from_dev(prts, bidx, mprts_cuda->d_bidx + off);
    
    float4 *xi4 = new float4[prts->n_part];
    float4 *pxi4 = new float4[prts->n_part];
    __particles_cuda_from_device(prts, xi4, pxi4);

    unsigned int *offsets = new unsigned int[cuda->nr_blocks+1];
    cuda_copy_offsets_from_dev(prts, offsets);
    assert(offsets[0] == 0);
    assert(offsets[cuda->nr_blocks] == prts->n_part);

    unsigned int last = 0;
    for (int b = 0; b < cuda->nr_blocks; b++) {
      for (int n = offsets[b]; n < offsets[b+1]; n++) {
	unsigned int block_pos_y = cuda_fint(xi4[n].y * cuda->b_dxi[1]);
	unsigned int block_pos_z = cuda_fint(xi4[n].z * cuda->b_dxi[2]);
	
	int block_idx;
	if (block_pos_y >= cuda->b_mx[1] || block_pos_z >= cuda->b_mx[2]) {
	  block_idx = cuda->nr_blocks;
	} else {
	  block_idx = block_pos_z * cuda->b_mx[1] + block_pos_y;
	}
	if (block_idx != bidx[n]) {
	  mprintf("n = %d bidx = %d block_idx = %d bp [%d:%d] real [%g:%g]\n",
		  n, bidx[n], block_idx, block_pos_y, block_pos_z,
		  xi4[n].y * cuda->b_dxi[1], xi4[n].z * cuda->b_dxi[2]);
	  static int error_cnt;
	  assert(error_cnt++ < 10);
	}
	if (!(bidx[n] >= last && bidx[n] < cuda->nr_blocks)) {
	  mprintf("n = %d bidx = %d last = %d\n", n, bidx[n], last);
	  static int error_cnt;
	  assert(error_cnt++ < 10);
	}
	if (bidx[n] != b) {
	  mprintf("n = %d bidx = %d block_idx = %d b = %d\n",
		  n, bidx[n], block_idx, b);
	  static int error_cnt;
	  assert(error_cnt++ < 10);
	}
	last = block_idx;
      }
    }
    delete[] offsets;
    delete[] bidx;
    delete[] xi4;
    delete[] pxi4;

    off += prts->n_part;
  }
}

// ======================================================================
// cuda_mprts_free

void
cuda_mprts_free(struct psc_mparticles *mprts)
{
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    free(cuda->bnd_idx);
    free(cuda->bnd_off);
    free(cuda->bnd_cnt);
    free(cuda->bnd_prts);
    free(cuda->bnd_xi4);
    free(cuda->bnd_pxi4);
  }
}

// ======================================================================
// cuda_mprts_check_ordered_total

void
cuda_mprts_check_ordered_total(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  psc_mparticles_cuda_copy_to_dev(mprts); // update n_part, particle pointers
  cuda_mprts_find_block_indices_2_total(mprts);

  unsigned int last = 0;
  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    unsigned int *bidx = new unsigned int[prts->n_part];
    cuda_copy_bidx_from_dev(prts, bidx, mprts_cuda->d_bidx + off);
    
    for (int n = 0; n < prts->n_part; n++) {
      if (!(bidx[n] >= last && bidx[n] < mprts->nr_patches * cuda->nr_blocks)) {
	mprintf("p = %d, n = %d bidx = %d last = %d\n", p, n, bidx[n], last);
	assert(0);
      }
      last = bidx[n];
    }

    delete[] bidx;

    off += prts->n_part;
  }
}

// ======================================================================
// cuda_mprts_compact

void
cuda_mprts_compact(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda *mprts_cuda = psc_mparticles_cuda(mprts);

  float4 *d_alt_xi4 = mprts_cuda->d_alt_xi4;
  float4 *d_alt_pxi4 = mprts_cuda->d_alt_pxi4;
  float4 *d_xi4 = mprts_cuda->d_xi4;
  float4 *d_pxi4 = mprts_cuda->d_pxi4;
  unsigned int *d_bidx = mprts_cuda->d_bidx;
  unsigned int *d_alt_bidx = mprts_cuda->d_alt_bidx;
  unsigned int *d_sums = mprts_cuda->d_sums;
  unsigned int *d_ids = mprts_cuda->d_ids;
  unsigned int *d_alt_ids = mprts_cuda->d_alt_ids;

  int nr_prts = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    assert(d_alt_xi4 + prts->n_part <= mprts_cuda->d_alt_xi4 + mprts_cuda->nr_alloced);
    check(cudaMemcpy(d_alt_xi4, cuda->h_dev->xi4,
		     prts->n_part * sizeof(*cuda->h_dev->alt_xi4),
		     cudaMemcpyDeviceToDevice));
    check(cudaMemcpy(d_alt_pxi4, cuda->h_dev->pxi4,
		     prts->n_part * sizeof(*cuda->h_dev->alt_pxi4),
		     cudaMemcpyDeviceToDevice));
    nr_prts += prts->n_part;
    cuda->n_alloced = prts->n_part;
    cuda->h_dev->alt_xi4 = d_alt_xi4;
    cuda->h_dev->alt_pxi4 = d_alt_pxi4;
    cuda->h_dev->xi4 = d_xi4;
    cuda->h_dev->pxi4 = d_pxi4;
    d_alt_xi4 += cuda->n_alloced;
    d_alt_pxi4 += cuda->n_alloced;
    d_xi4 += cuda->n_alloced;
    d_pxi4 += cuda->n_alloced;
    d_bidx += cuda->n_alloced;
    d_alt_bidx += cuda->n_alloced;
    d_ids += cuda->n_alloced;
    d_alt_ids += cuda->n_alloced;
    d_sums += cuda->n_alloced;
  }
  mprts_cuda->nr_prts = nr_prts;
  psc_mparticles_cuda_swap_alt(mprts);
  psc_mparticles_cuda_copy_to_dev(mprts);
}

void
cuda_mprts_find_off(struct psc_mparticles *mprts)
{
  unsigned int off = 0;
  unsigned int last = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    int *offsets = new int[cuda->nr_blocks + 1];
    check(cudaMemcpy(offsets, cuda->h_dev->offsets,
		     (cuda->nr_blocks + 1) * sizeof(*offsets),
		     cudaMemcpyDeviceToHost));
    assert(offsets[cuda->nr_blocks] == prts->n_part);
    for (int n = 0; n <= cuda->nr_blocks; n++) {
      offsets[n] += off;
    }
    assert(offsets[0] == last);
    last = offsets[cuda->nr_blocks];
    check(cudaMemcpy(cuda->h_dev->d_off, offsets,
		     (cuda->nr_blocks + 1) * sizeof(*offsets),
		     cudaMemcpyHostToDevice));
    delete[] offsets;

    off += prts->n_part;
  }
}

void
cuda_mprts_find_offsets(struct psc_mparticles *mprts)
{
  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    int *d_off = new int[cuda->nr_blocks + 1];
    check(cudaMemcpy(d_off, cuda->h_dev->d_off,
		     (cuda->nr_blocks + 1) * sizeof(*d_off),
		     cudaMemcpyDeviceToHost));
    assert(d_off[cuda->nr_blocks] == prts->n_part + off);
    for (int n = 0; n <= cuda->nr_blocks; n++) {
      d_off[n] -= off;
    }
    assert(d_off[0] == 0);
    check(cudaMemcpy(cuda->h_dev->offsets, d_off,
		     (cuda->nr_blocks + 1) * sizeof(*d_off),
		     cudaMemcpyHostToDevice));
    delete[] d_off;

    off += prts->n_part;
  }
}
