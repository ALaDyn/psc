
#include <psc_cuda.h>
#include "cuda_sort2.h"
#include "particles_cuda.h"

#include <thrust/scan.h>
#include <thrust/device_vector.h>

#define PFX(x) xchg_##x
#include "constants.c"

// FIXME const mem for dims?
// FIXME probably should do our own loop rather than use blockIdx

__global__ static void
exchange_particles(int n_part, particles_cuda_dev_t d_part,
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
      d_part.xi4[i].x * d_consts.dxi[0],
      d_part.xi4[i].y * d_consts.dxi[1],
      d_part.xi4[i].z * d_consts.dxi[2] };
    int pos[3];
    for (int d = 0; d < 3; d++) {
      pos[d] = cuda_fint(xi[d]);
    }
    if (pos[1] < 0) {
      d_part.xi4[i].y += xm[1];
      if (d_part.xi4[i].y >= xm[1])
	d_part.xi4[i].y = 0.f;
    }
    if (pos[2] < 0) {
      d_part.xi4[i].z += xm[2];
      if (d_part.xi4[i].z >= xm[2])
	d_part.xi4[i].z = 0.f;
    }
    if (pos[1] >= ldims[1]) {
      d_part.xi4[i].y -= xm[1];
    }
    if (pos[2] >= ldims[2]) {
      d_part.xi4[i].z -= xm[2];
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
	     exchange_particles, (prts->n_part, cuda->d_part,
				  patch->ldims[0], patch->ldims[1], patch->ldims[2]));
}

EXTERN_C void
cuda_alloc_block_indices(struct psc_particles *prts, unsigned int **d_bidx)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  check(cudaMalloc((void **) d_bidx, cuda->n_alloced * sizeof(**d_bidx)));
}

EXTERN_C void
cuda_free_block_indices(unsigned int *d_bidx)
{
  check(cudaFree(d_bidx));
}

EXTERN_C void
cuda_copy_bidx_from_dev(struct psc_particles *prts, unsigned int *h_bidx, unsigned int *d_bidx)
{
  check(cudaMemcpy(h_bidx, d_bidx, prts->n_part * sizeof(*h_bidx),
		   cudaMemcpyDeviceToHost));
}

EXTERN_C void
cuda_copy_bidx_to_dev(struct psc_particles *prts, unsigned int *d_bidx, unsigned int *h_bidx)
{
  check(cudaMemcpy(d_bidx, h_bidx, prts->n_part * sizeof(*d_bidx),
		   cudaMemcpyHostToDevice));
}

// ======================================================================
// cuda_find_block_indices

__global__ static void
find_block_indices(int n_part, particles_cuda_dev_t d_part, unsigned int *d_bidx,
		   int dimy, float b_dyi, float b_dzi, int b_my, int b_mz)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    float4 xi4 = d_part.xi4[i];
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
	     find_block_indices, (prts->n_part, cuda->d_part, d_bidx,
				  cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				  cuda->b_mx[1], cuda->b_mx[2]));
}

// ======================================================================
// cuda_find_block_indices_ids

__global__ static void
find_block_indices_ids(int n_part, particles_cuda_dev_t d_part, unsigned int *d_bidx,
		       unsigned int *d_ids, int dimy, float b_dyi, float b_dzi, int b_my, int b_mz)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    float4 xi4 = d_part.xi4[i];
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
	     find_block_indices_ids, (prts->n_part, cuda->d_part, d_bidx, d_ids,
				      cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				      cuda->b_mx[1], cuda->b_mx[2]));
}

// ======================================================================
// cuda_find_block_indices_2
//
// like cuda_find_block_indices, but handles out-of-bound
// particles

__global__ static void
find_block_indices_2(int n_part, particles_cuda_dev_t d_part, unsigned int *d_bidx,
		     int dimy, float b_dyi, float b_dzi,
		     int b_my, int b_mz, int start)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x + start;
  if (i < n_part) {
    float4 xi4 = d_part.xi4[i];
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
	     find_block_indices_2, (prts->n_part, cuda->d_part, d_bidx,
				    cuda->map.dims[1], cuda->b_dxi[1], cuda->b_dxi[2],
				    cuda->b_mx[1], cuda->b_mx[2], start));
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_2

__global__ static void
mprts_find_block_indices_2(struct cuda_params prm, struct cuda_patch_prts *d_cp_prts)
{
  int tid = threadIdx.x;

  int block_pos[3];
  block_pos[1] = blockIdx.x;
  block_pos[2] = blockIdx.y % prm.b_mx[2];
  int bid = block_pos_to_block_idx(block_pos, prm.b_mx);
  int p = blockIdx.y / prm.b_mx[2];

  int block_begin = d_cp_prts[p].d_part.offsets[bid];
  int block_end   = d_cp_prts[p].d_part.offsets[bid+1];

  for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
    float4 xi4 = d_cp_prts[p].d_part.xi4[n];
    unsigned int block_pos_y = cuda_fint(xi4.y * prm.b_dxi[1]);
    unsigned int block_pos_z = cuda_fint(xi4.z * prm.b_dxi[2]);

    int block_idx;
    if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
      block_idx = prm.b_mx[1] * prm.b_mx[2];
    } else {
      block_idx = block_pos_z * prm.b_mx[1] + block_pos_y;
    }
    d_cp_prts[p].d_part.bidx[n] = block_idx;
  }
}

EXTERN_C void
cuda_mprts_find_block_indices_2(struct cuda_mprts *cuda_mprts)
{
  if (cuda_mprts->nr_patches > 0) {
    struct cuda_params prm;
    set_params(&prm, ppsc, cuda_mprts->mprts_cuda[0], NULL);
    
    int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
    int dimGrid[2]  = { prm.b_mx[1], prm.b_mx[2] * cuda_mprts->nr_patches };

    RUN_KERNEL(dimGrid, dimBlock,
	       mprts_find_block_indices_2, (prm, cuda_mprts->d_cp_prts));
    free_params(&prm);
  }
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
// reorder_send_buf

__global__ static void
reorder_send_buf(int n_part, particles_cuda_dev_t d_part, unsigned int *d_bidx,
		 unsigned int *d_sums, unsigned int nr_blocks)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  if (i < n_part) {
    if (d_bidx[i] == nr_blocks) {
      int j = d_sums[i] + n_part;
      d_part.xi4[j] = d_part.xi4[i];
      d_part.pxi4[j] = d_part.pxi4[i];
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
	     reorder_send_buf, (prts->n_part, cuda->d_part, d_bidx, d_sums, cuda->nr_blocks));
}

__global__ static void
mprts_reorder_send_buf(struct cuda_params prm, struct cuda_patch_prts *d_cp_prts, int nr_patches)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];

  for (int p = 0; p < nr_patches; p++) {
#if 0
    __shared__ struct cuda_patch_prts cp_prts;

    __syncthreads();
    if (threadIdx.x < sizeof(cp_prts) / sizeof(int)) {
      ((int *) &cp_prts)[threadIdx.x] = ((int *) &d_cp_prts[p])[threadIdx.x];
    }
    __syncthreads();
#else
    struct cuda_patch_prts cp_prts = d_cp_prts[p];
#endif
    
    if (i < cp_prts.n_part) {
      if (cp_prts.d_part.bidx[i] == nr_blocks) {
	int j = cp_prts.d_part.sums[i] + cp_prts.n_part;
	cp_prts.d_part.xi4[j]  = cp_prts.d_part.xi4[i];
	cp_prts.d_part.pxi4[j] = cp_prts.d_part.pxi4[i];
      }
    }
  }
}

EXTERN_C void
cuda_mprts_reorder_send_buf(struct cuda_mprts *cuda_mprts)
{
  if (cuda_mprts->nr_patches > 0) {
    struct cuda_params prm;
    set_params(&prm, ppsc, cuda_mprts->mprts_cuda[0], NULL);

    int max_n_part = 0;
    for (int p = 0; p < cuda_mprts->nr_patches; p++) {
      struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
      struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
      assert(prts->n_part + cuda->bnd_n_send <= cuda->n_alloced);
      if (prts->n_part > max_n_part) {
	max_n_part = prts->n_part;
      }
    }

    int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
    int dimGrid[2]  = { (max_n_part + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };

    RUN_KERNEL(dimGrid, dimBlock,
		 mprts_reorder_send_buf, (prm, cuda_mprts->d_cp_prts, cuda_mprts->nr_patches));

    free_params(&prm);
  }
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
  __particles_cuda_to_device(prts, xi4, pxi4, NULL, NULL);
  prts->n_part = n_part;
  delete[] xi4;
  delete[] pxi4;
  delete[] bidx;
  delete[] sums;
}

// ======================================================================
// reorder_and_offsets

__global__ static void
reorder_and_offsets(int n_part, particles_cuda_dev_t d_part, float4 *xi4, float4 *pxi4,
		    unsigned int *d_bidx, unsigned int *d_ids, int nr_blocks)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i > n_part)
    return;

  int block, prev_block;
  if (i < n_part) {
    xi4[i] = d_part.xi4[d_ids[i]];
    pxi4[i] = d_part.pxi4[d_ids[i]];
    
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
    d_part.offsets[b] = i;
  }
}

EXTERN_C void
cuda_reorder_and_offsets(struct psc_particles *prts, unsigned int *d_bidx,
			 unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *alt_xi4 = cuda->d_part.alt_xi4;
  float4 *alt_pxi4 = cuda->d_part.alt_pxi4;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     reorder_and_offsets, (prts->n_part, cuda->d_part, alt_xi4, alt_pxi4,
				   d_bidx, d_ids, cuda->nr_blocks));

  cuda->d_part.alt_xi4 = cuda->d_part.xi4;
  cuda->d_part.alt_pxi4 = cuda->d_part.pxi4;
  cuda->d_part.xi4 = alt_xi4;
  cuda->d_part.pxi4 = alt_pxi4;
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

  float4 *d_alt_xi4 = cuda->d_part.alt_xi4;
  float4 *d_alt_pxi4 = cuda->d_part.alt_pxi4;
  cuda->d_part.alt_xi4 = cuda->d_part.xi4;
  cuda->d_part.alt_pxi4 = cuda->d_part.pxi4;
  cuda->d_part.xi4 = d_alt_xi4;
  cuda->d_part.pxi4 = d_alt_pxi4;

  __particles_cuda_to_device(prts, alt_xi4, alt_pxi4, offsets, NULL);
  delete[] xi4;
  delete[] pxi4;
  delete[] alt_xi4;
  delete[] alt_pxi4;
  delete[] bidx;
  delete[] ids;
  delete[] offsets;
}

// ======================================================================
// cuda_reorder

__global__ static void
reorder(int n_part, particles_cuda_dev_t d_part, float4 *xi4, float4 *pxi4,
	unsigned int *d_ids)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i < n_part) {
    xi4[i] = d_part.xi4[d_ids[i]];
    pxi4[i] = d_part.pxi4[d_ids[i]];
  }
}

EXTERN_C void
cuda_reorder(struct psc_particles *prts, unsigned int *d_ids)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  float4 *alt_xi4 = cuda->d_part.alt_xi4;
  float4 *alt_pxi4 = cuda->d_part.alt_pxi4;

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { (prts->n_part + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     reorder, (prts->n_part, cuda->d_part, alt_xi4, alt_pxi4, d_ids));

  cuda->d_part.alt_xi4 = cuda->d_part.xi4;
  cuda->d_part.alt_pxi4 = cuda->d_part.pxi4;
  cuda->d_part.xi4 = alt_xi4;
  cuda->d_part.pxi4 = alt_pxi4;
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
cuda_mprts_copy_from_dev(struct cuda_mprts *cuda_mprts)
{
  cudaStream_t stream[cuda_mprts->nr_patches];
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    cuda->bnd_n_part = 0;
    cuda->bnd_prts = NULL;
    
    int n_send = cuda->bnd_n_send;
    cuda->bnd_xi4  = new float4[n_send];
    cuda->bnd_pxi4 = new float4[n_send];
    cuda->bnd_idx  = new unsigned int[n_send];
    cuda->bnd_off  = new unsigned int[n_send];

    cudaStreamCreate(&stream[p]);
  }    
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

    int n_send = cuda->bnd_n_send;
    check(cudaMemcpyAsync(cuda->bnd_xi4, cuda->d_part.xi4 + cuda->bnd_n_part_save,
			  n_send * sizeof(*cuda->bnd_xi4), cudaMemcpyDeviceToHost, stream[p]));
    check(cudaMemcpyAsync(cuda->bnd_pxi4, cuda->d_part.pxi4 + cuda->bnd_n_part_save,
			  n_send * sizeof(*cuda->bnd_pxi4), cudaMemcpyDeviceToHost, stream[p]));
  }
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    cudaStreamSynchronize(stream[p]);
    cudaStreamDestroy(stream[p]);
  }
}

// ======================================================================
// cuda_mprts_copy_to_dev

void
cuda_mprts_copy_to_dev(struct cuda_mprts *cuda_mprts)
{
  cudaStream_t stream[cuda_mprts->nr_patches];
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    cudaStreamCreate(&stream[p]);
  }
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    check(cudaMemcpy(cuda->d_part.xi4 + cuda->bnd_n_part_save, cuda->bnd_xi4,
		     (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_xi4),
		     cudaMemcpyHostToDevice));
    check(cudaMemcpy(cuda->d_part.pxi4 + cuda->bnd_n_part_save, cuda->bnd_pxi4,
		     (prts->n_part - cuda->bnd_n_part_save) * sizeof(*cuda->bnd_pxi4),
		     cudaMemcpyHostToDevice));
  }

  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cudaStreamSynchronize(stream[p]);
    cudaStreamDestroy(stream[p]);
    free(cuda->bnd_prts);
    free(cuda->bnd_xi4);
    free(cuda->bnd_pxi4);
  }
}

