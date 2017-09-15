
#include "cuda_mparticles.h"
#include "cuda_bits.h"

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>

#include <cstdio>
#include <cassert>

// ----------------------------------------------------------------------
// cuda_mparticles_create

struct cuda_mparticles *
cuda_mparticles_create()
{
  struct cuda_mparticles *cmprts = new cuda_mparticles;
  std::memset(cmprts, 0, sizeof(*cmprts));

  return cmprts;
}

// ----------------------------------------------------------------------
// cuda_mparticles_set_domain_info

void
cuda_mparticles_set_domain_info(struct cuda_mparticles *cmprts,
				const struct cuda_domain_info *info)
{
  cmprts->n_patches = info->n_patches;
  cmprts->xb_by_patch = new float_3[cmprts->n_patches];
  for (int d = 0; d < 3; d++) {
    cmprts->ldims[d] = info->ldims[d];
    cmprts->bs[d] = info->bs[d];
    assert(info->ldims[d] % info->bs[d] == 0);
    cmprts->b_mx[d] = info->ldims[d] / info->bs[d];
    cmprts->dx[d] = info->dx[d];
    cmprts->b_dxi[d] = 1.f / (info->bs[d] * info->dx[d]);

    assert(info->xb_by_patch);
    for (int p = 0; p < cmprts->n_patches; p++) {
      cmprts->xb_by_patch[p][d] = info->xb_by_patch[p][d];
    }
  }
  cmprts->n_blocks_per_patch = cmprts->b_mx[0] * cmprts->b_mx[1] * cmprts->b_mx[2];
  cmprts->n_blocks = cmprts->n_patches * cmprts->n_blocks_per_patch;
}

// ----------------------------------------------------------------------
// cuda_mparticles_setup

void
cuda_mparticles_setup(struct cuda_mparticles *cmprts)
{
  cudaError_t ierr;

  ierr = cudaMalloc(&cmprts->d_n_prts_by_patch, cmprts->n_patches * sizeof(unsigned int)); cudaCheck(ierr);
  ierr = cudaMalloc(&cmprts->d_off, (cmprts->n_blocks + 1) * sizeof(*cmprts->d_off)); cudaCheck(ierr);
  ierr = cudaMemset(cmprts->d_off, 0, (cmprts->n_blocks + 1) * sizeof(*cmprts->d_off)); cudaCheck(ierr);
}

// ----------------------------------------------------------------------
// cuda_mparticles_free_particle_mem

static void
cuda_mparticles_free_particle_mem(struct cuda_mparticles *cmprts)
{
  cudaError_t ierr;

  ierr = cudaFree(cmprts->d_xi4); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_pxi4); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_alt_xi4); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_alt_pxi4); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_bidx); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_id); cudaCheck(ierr);
}

// ----------------------------------------------------------------------
// cuda_mparticles_destroy

void
cuda_mparticles_destroy(struct cuda_mparticles *cmprts)
{
  cudaError_t ierr;

  ierr = cudaFree(cmprts->d_n_prts_by_patch); cudaCheck(ierr);
  ierr = cudaFree(cmprts->d_off); cudaCheck(ierr);

  cuda_mparticles_free_particle_mem(cmprts);

  delete[] cmprts->xb_by_patch;
  delete cmprts;
}

// ----------------------------------------------------------------------
// cuda_mparticles_reserve

void
cuda_mparticles_reserve(struct cuda_mparticles *cmprts, unsigned int *n_prts_by_patch)
{
  cudaError_t ierr;

  unsigned int size = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    size += n_prts_by_patch[p];
  }

  if (size <= cmprts->n_alloced) {
    return;
  }

  unsigned int n_alloced = max(size, 2 * cmprts->n_alloced);

  if (cmprts->n_alloced > 0) {
    cuda_mparticles_free_particle_mem(cmprts);
  }
  cmprts->n_alloced = n_alloced;

  ierr = cudaMalloc((void **) &cmprts->d_xi4, n_alloced * sizeof(float4)); cudaCheck(ierr);
  ierr = cudaMalloc((void **) &cmprts->d_pxi4, n_alloced * sizeof(float4)); cudaCheck(ierr);
  ierr = cudaMalloc((void **) &cmprts->d_alt_xi4, n_alloced * sizeof(float4)); cudaCheck(ierr);
  ierr = cudaMalloc((void **) &cmprts->d_alt_pxi4, n_alloced * sizeof(float4)); cudaCheck(ierr);
  ierr = cudaMalloc((void **) &cmprts->d_bidx, n_alloced * sizeof(unsigned int)); cudaCheck(ierr);
  ierr = cudaMalloc((void **) &cmprts->d_id, n_alloced * sizeof(unsigned int)); cudaCheck(ierr);
}

// ----------------------------------------------------------------------
// cuda_mparticles_to_device

void
cuda_mparticles_to_device(struct cuda_mparticles *cmprts, float4 *xi4, float4 *pxi4,
			  unsigned int n_prts, unsigned int off)
{
  cudaError_t ierr;

  assert(off + n_prts <= cmprts->n_alloced);
  ierr = cudaMemcpy(cmprts->d_xi4 + off, xi4, n_prts * sizeof(*xi4),
		    cudaMemcpyHostToDevice); cudaCheck(ierr);
  ierr = cudaMemcpy(cmprts->d_pxi4 + off, pxi4, n_prts * sizeof(*pxi4),
		    cudaMemcpyHostToDevice); cudaCheck(ierr);
}

// ----------------------------------------------------------------------
// cuda_mparticles_from_device

void
cuda_mparticles_from_device(struct cuda_mparticles *cmprts, float4 *xi4, float4 *pxi4,
			    unsigned int n_prts, unsigned int off)
{
  cudaError_t ierr;

  assert(off + n_prts <= cmprts->n_alloced);
  ierr = cudaMemcpy(xi4, cmprts->d_xi4 + off, n_prts * sizeof(*xi4),
		    cudaMemcpyDeviceToHost); cudaCheck(ierr);
  ierr = cudaMemcpy(pxi4, cmprts->d_pxi4 + off, n_prts * sizeof(*pxi4),
		    cudaMemcpyDeviceToHost); cudaCheck(ierr);
}

// ----------------------------------------------------------------------
// cuda_mparticles_dump_by_patch

void
cuda_mparticles_dump_by_patch(struct cuda_mparticles *cmprts, unsigned int *n_prts_by_patch)
{
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<float4> d_pxi4(cmprts->d_pxi4);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);

  printf("cuda_mparticles_dump_by_patch: n_prts = %d\n", cmprts->n_prts);
  unsigned int off = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    float *xb = &cmprts->xb_by_patch[p][0];
    for (int n = 0; n < n_prts_by_patch[p]; n++) {
      float4 xi4 = d_xi4[n + off], pxi4 = d_pxi4[n + off];
      unsigned int bidx = d_bidx[n + off], id = d_id[n + off];
      printf("cuda_mparticles_dump_by_patch: [%d/%d] %g %g %g // %d // %g %g %g // %g b_idx %d id %d\n",
	     p, n, xi4.x + xb[0], xi4.y + xb[1], xi4.z + xb[2],
	     cuda_float_as_int(xi4.w),
	     pxi4.x, pxi4.y, pxi4.z, pxi4.w,
	     bidx, id);
    }
    off += n_prts_by_patch[p];
  }
}

// ----------------------------------------------------------------------
// cuda_mparticles_dump

void
cuda_mparticles_dump(struct cuda_mparticles *cmprts)
{
  int n_prts = cmprts->n_prts;
  
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<float4> d_pxi4(cmprts->d_pxi4);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);

  printf("cuda_mparticles_dump: n_prts = %d\n", n_prts);
  unsigned int off = 0;
  for (int b = 0; b < cmprts->n_blocks; b++) {
    unsigned int off_b = d_off[b], off_e = d_off[b+1];
    int p = b / cmprts->n_blocks_per_patch;
    printf("cuda_mparticles_dump: block %d: %d -> %d (patch %d)\n", b, off_b, off_e, p);
    assert(d_off[b] == off);
    for (int n = d_off[b]; n < d_off[b+1]; n++) {
      float4 xi4 = d_xi4[n], pxi4 = d_pxi4[n];
      unsigned int bidx = d_bidx[n], id = d_id[n];
      printf("cuda_mparticles_dump: [%d] %g %g %g // %d // %g %g %g // %g || bidx %d id %d\n",
	     n, xi4.x, xi4.y, xi4.z, cuda_float_as_int(xi4.w), pxi4.x, pxi4.y, pxi4.z, pxi4.w,
	     bidx, id);
      assert(b == bidx);
    }
    off += off_e - off_b;
  }
}

// ----------------------------------------------------------------------
// cuda_mparticles_swap_alt

void
cuda_mparticles_swap_alt(struct cuda_mparticles *cmprts)
{
  float4 *tmp_xi4 = cmprts->d_alt_xi4;
  float4 *tmp_pxi4 = cmprts->d_alt_pxi4;
  cmprts->d_alt_xi4 = cmprts->d_xi4;
  cmprts->d_alt_pxi4 = cmprts->d_pxi4;
  cmprts->d_xi4 = tmp_xi4;
  cmprts->d_pxi4 = tmp_pxi4;
}

// ----------------------------------------------------------------------
// cuda_params2

struct cuda_params2 {
  unsigned int b_mx[3];
  float b_dxi[3];
};

static void
cuda_params2_set(struct cuda_params2 *prm, const struct cuda_mparticles *cuda_mprts)
{
  for (int d = 0; d < 3; d++) {
    prm->b_mx[d]  = cuda_mprts->b_mx[d];
    prm->b_dxi[d] = cuda_mprts->b_dxi[d];
  }
}

static void
cuda_params2_free(struct cuda_params2 *prm)
{
}

#define THREADS_PER_BLOCK 256

// ----------------------------------------------------------------------
// get_block_idx

static int
get_block_idx(struct cuda_mparticles *cmprts, float4 xi4, int p)
{
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  float *b_dxi = cmprts->b_dxi;
  int *b_mx = cmprts->b_mx;
  
  unsigned int block_pos_y = (int) floorf(xi4.y * b_dxi[1]);
  unsigned int block_pos_z = (int) floorf(xi4.z * b_dxi[2]);

  int bidx;
  if (block_pos_y >= b_mx[1] || block_pos_z >= b_mx[2]) {
    bidx = -1;
  } else {
    bidx = (p * b_mx[2] + block_pos_z) * b_mx[1] + block_pos_y;
  }

  return bidx;
}

// ----------------------------------------------------------------------
// cuda_mprts_find_block_indices_ids

__global__ static void
mprts_find_block_indices_ids(struct cuda_params2 prm, float4 *d_xi4,
			     int *d_n_prts_by_patch, unsigned int *d_bidx,
			     unsigned int *d_ids, int nr_patches)
{
  int n = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;
  int nr_blocks = prm.b_mx[1] * prm.b_mx[2];

  unsigned int off = 0;
  for (int p = 0; p < nr_patches; p++) {
    if (n < d_n_prts_by_patch[p]) {
      float4 xi4 = d_xi4[n + off];
      unsigned int block_pos_y = __float2int_rd(xi4.y * prm.b_dxi[1]);
      unsigned int block_pos_z = __float2int_rd(xi4.z * prm.b_dxi[2]);
      
      int block_idx;
      if (block_pos_y >= prm.b_mx[1] || block_pos_z >= prm.b_mx[2]) {
	block_idx = -1; // not supposed to happen here!
      } else {
	block_idx = block_pos_z * prm.b_mx[1] + block_pos_y + p * nr_blocks;
      }
      d_bidx[n + off] = block_idx;
      d_ids[n + off] = n + off;
    }
    off += d_n_prts_by_patch[p];
  }
}

void
cuda_mparticles_find_block_indices_ids(struct cuda_mparticles *cmprts,
				       unsigned int *n_prts_by_patch)
{
  if (cmprts->n_patches == 0) {
    return;
  }

  int max_n_prts = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    if (n_prts_by_patch[p] > max_n_prts) {
      max_n_prts = n_prts_by_patch[p];
    }
  }

  cudaError_t ierr;
  ierr = cudaMemcpy(cmprts->d_n_prts_by_patch, n_prts_by_patch,
		    cmprts->n_patches * sizeof(unsigned int),
		    cudaMemcpyHostToDevice); cudaCheck(ierr);

  struct cuda_params2 prm;
  cuda_params2_set(&prm, cmprts);
    
  dim3 dimGrid((max_n_prts + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK);
  dim3 dimBlock(THREADS_PER_BLOCK);

  mprts_find_block_indices_ids<<<dimGrid, dimBlock>>>(prm,
						      cmprts->d_xi4, 
						      cmprts->d_n_prts_by_patch,
						      cmprts->d_bidx,
						      cmprts->d_id,
						      cmprts->n_patches);
  cuda_sync_if_enabled();
  cuda_params2_free(&prm);
}

// ----------------------------------------------------------------------
// cuda_mparticles_reorder_and_offsets

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
cuda_mparticles_reorder_and_offsets(struct cuda_mparticles *cmprts)
{
  if (cmprts->n_patches == 0) {
    return;
  }

  dim3 dimGrid((cmprts->n_prts + 1 + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK);
  dim3 dimBlock(THREADS_PER_BLOCK);

  mprts_reorder_and_offsets<<<dimGrid, dimBlock>>>(cmprts->n_prts, cmprts->d_xi4, cmprts->d_pxi4,
						   cmprts->d_alt_xi4, cmprts->d_alt_pxi4,
						   cmprts->d_bidx, cmprts->d_id,
						   cmprts->d_off, cmprts->n_blocks);
  cuda_sync_if_enabled();

  cuda_mparticles_swap_alt(cmprts);
  cmprts->need_reorder = false;
}

void
cuda_mparticles_reorder_and_offsets_slow(struct cuda_mparticles *cmprts)
{
  if (cmprts->n_patches == 0) {
    return;
  }

  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<float4> d_pxi4(cmprts->d_pxi4);
  thrust::device_ptr<float4> d_alt_xi4(cmprts->d_alt_xi4);
  thrust::device_ptr<float4> d_alt_pxi4(cmprts->d_alt_pxi4);
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);

  thrust::host_vector<float4> h_xi4(d_xi4, d_xi4 + cmprts->n_prts);
  thrust::host_vector<float4> h_pxi4(d_pxi4, d_pxi4 + cmprts->n_prts);
  thrust::host_vector<float4> h_alt_xi4(d_alt_xi4, d_alt_xi4 + cmprts->n_prts);
  thrust::host_vector<float4> h_alt_pxi4(d_alt_pxi4, d_alt_pxi4 + cmprts->n_prts);
  thrust::host_vector<unsigned int> h_off(d_off, d_off + cmprts->n_blocks + 1);
  thrust::host_vector<unsigned int> h_bidx(d_bidx, d_bidx + cmprts->n_prts);
  thrust::host_vector<unsigned int> h_id(d_id, d_id + cmprts->n_prts);

  for (int i = 0; i <= cmprts->n_prts; i++) {
    //    unsigned int bidx;
    unsigned int block;
    if (i < cmprts->n_prts) {
      h_alt_xi4[i] = h_xi4[h_id[i]];
      h_alt_pxi4[i] = h_pxi4[h_id[i]];
      //bidx = get_block_idx(cmprts, h_alt_xi4[i], 0);
      block = h_bidx[i];
    } else {
      //bidx = cmprts->n_blocks;
      block = cmprts->n_blocks;
    }
    // if (i < 10) {
    //   printf("i %d bidx %d block %d xi4 %g %g\n", bidx, block, h_alt_xi4[i].y, h_alt_xi4[i].z);
    // }
    int prev_block = (i > 0) ? (int) h_bidx[i-1] : -1;
    for (int b = prev_block + 1; b <= block; b++) {
      h_off[b] = i;
    }
  }

  thrust::copy(h_alt_xi4.begin(), h_alt_xi4.end(), d_alt_xi4);
  thrust::copy(h_alt_pxi4.begin(), h_alt_pxi4.end(), d_alt_pxi4);
  thrust::copy(h_off.begin(), h_off.end(), d_off);
  
  cuda_mparticles_swap_alt(cmprts);
  cmprts->need_reorder = false;
}

// ----------------------------------------------------------------------
// cuda_mparticles_check_in_patch_unordered_slow

void
cuda_mparticles_check_in_patch_unordered_slow(struct cuda_mparticles *cmprts,
					      unsigned int *nr_prts_by_patch)
{
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);

  unsigned int off = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    for (int n = 0; n < nr_prts_by_patch[p]; n++) {
      int bidx = get_block_idx(cmprts, d_xi4[off + n], p);
      assert(bidx >= 0 && bidx <= cmprts->n_blocks);
    }
    off += nr_prts_by_patch[p];
  }

  assert(off == cmprts->n_prts);
  printf("PASS: cuda_mparticles_check_in_patch_unordered_slow()\n");
}

// ----------------------------------------------------------------------
// cuda_mparticles_check_bix_id_unordered_slow

void
cuda_mparticles_check_bidx_id_unordered_slow(struct cuda_mparticles *cmprts,
					     unsigned int *n_prts_by_patch)
{
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);

  unsigned int off = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    for (int n = 0; n < n_prts_by_patch[p]; n++) {
      int bidx = get_block_idx(cmprts, d_xi4[off + n], p);
      assert(bidx == d_bidx[off+n]);
      assert(off+n == d_id[off+n]);
    }
    off += n_prts_by_patch[p];
  }

  assert(off == cmprts->n_prts);
  printf("PASS: cuda_mparticles_check_bidx_id_unordered_slow()\n");
}

// ----------------------------------------------------------------------
// cuda_mparticles_check_ordered_slow

void
cuda_mparticles_check_ordered_slow(struct cuda_mparticles *cmprts)
{
  bool need_reorder = cmprts->need_reorder;
  
  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);

  unsigned int off = 0;
  for (int b = 0; b < cmprts->n_blocks; b++) {
    int p = b / cmprts->n_blocks_per_patch;
    unsigned int off_b = d_off[b], off_e = d_off[b+1];
    assert(off_e >= off_b);
    // printf("cuda_mparticles_check_ordered: block %d: %d -> %d (patch %d)\n", b, off_b, off_e, p);
    assert(d_off[b] == off);
    for (int n = d_off[b]; n < d_off[b+1]; n++) {
      float4 xi4;
      if (need_reorder) {
	xi4 = d_xi4[d_id[n]];
      } else {
	xi4 = d_xi4[n];
      }
      unsigned int bidx = get_block_idx(cmprts, xi4, p);
      //printf("cuda_mparticles_check_ordered: bidx %d\n", bidx);
      if (b != bidx) {
	printf("b %d bidx %d n %d p %d xi4 %g %g %g\n",
	       b, bidx, n, p, xi4.x, xi4.y, xi4.z);
	unsigned int block_pos_y = (int) floorf(xi4.y * cmprts->b_dxi[1]);
	unsigned int block_pos_z = (int) floorf(xi4.z * cmprts->b_dxi[2]);
	printf("block_pos %d %d %g %g\n", block_pos_y, block_pos_z, xi4.y * cmprts->b_dxi[1],
	       xi4.z * cmprts->b_dxi[2]);
      }
      assert(b == bidx);
    }
    off += off_e - off_b;
  }
  assert(off == cmprts->n_prts);
  printf("cuda_mparticles_check_ordered: PASS\n");
}

// ----------------------------------------------------------------------
// cuda_mparticles_check_ordered

void
cuda_mparticles_check_ordered(struct cuda_mparticles *cmprts)
{
  bool need_reorder = cmprts->need_reorder;

  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::host_vector<float4> h_xi4(d_xi4, d_xi4 + cmprts->n_prts);
  thrust::host_vector<unsigned int> h_off(d_off, d_off + cmprts->n_blocks + 1);
  thrust::host_vector<unsigned int> h_id(d_id, d_id + cmprts->n_prts);

  //printf("cuda_mparticles_check_ordered: need_reorder %s\n", need_reorder ? "true" : "false");

  // for (int n = 0; n < 10; n++) {
  //   unsigned int bidx = d_bidx[n];
  //   printf("n %d bidx %d xi4 %g %g\n", n, bidx, h_xi4[n].y, h_xi4[n].z);
  // }
  unsigned int off = 0;
  for (int b = 0; b < cmprts->n_blocks; b++) {
    int p = b / cmprts->n_blocks_per_patch;
    unsigned int off_b = h_off[b], off_e = h_off[b+1];
    assert(off_e >= off_b);
    //printf("cuda_mparticles_check_ordered: block %d: %d -> %d (patch %d)\n", b, off_b, off_e, p);
    assert(off_b == off);
    for (int n = h_off[b]; n < h_off[b+1]; n++) {
      float4 xi4;
      if (need_reorder) {
	xi4 = h_xi4[h_id[n]];
      } else {
	xi4 = h_xi4[n];
      }
      unsigned int bidx = get_block_idx(cmprts, xi4, p);
      //printf("cuda_mparticles_check_ordered: bidx %d\n", bidx);
      if (b != bidx) {
	printf("b %d bidx %d n %d p %d xi4 %g %g %g\n",
	       b, bidx, n, p, xi4.x, xi4.y, xi4.z);
	unsigned int block_pos_y = (int) floorf(xi4.y * cmprts->b_dxi[1]);
	unsigned int block_pos_z = (int) floorf(xi4.z * cmprts->b_dxi[2]);
	printf("block_pos %d %d %g %g\n", block_pos_y, block_pos_z, xi4.y * cmprts->b_dxi[1],
	       xi4.z * cmprts->b_dxi[2]);
      }
      assert(b == bidx);
    }
    off += off_e - off_b;
  }
  assert(off == cmprts->n_prts);
  printf("cuda_mparticles_check_ordered: PASS\n");
}

// ----------------------------------------------------------------------
// cuda_mparticles_sort_initial

void
cuda_mparticles_sort_initial(struct cuda_mparticles *cmprts,
			     unsigned int *n_prts_by_patch)
{
  static int first_time = false;
  if (first_time) {
    cuda_mparticles_check_in_patch_unordered_slow(cmprts, n_prts_by_patch);
  }

  cuda_mparticles_find_block_indices_ids(cmprts, n_prts_by_patch);
  if (first_time) {
    cuda_mparticles_check_bidx_id_unordered_slow(cmprts, n_prts_by_patch);
  }

  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);
  thrust::stable_sort_by_key(d_bidx, d_bidx + cmprts->n_prts, d_id);
  cuda_mparticles_reorder_and_offsets(cmprts);

  if (first_time) {
    cuda_mparticles_check_ordered(cmprts);
    first_time = false;
  }
}

// ----------------------------------------------------------------------
// cuda_mparticles_set_n_prts_by_patch

void
cuda_mparticles_set_n_prts_by_patch(struct cuda_mparticles *cmprts,
				    const unsigned int *n_prts_by_patch)
{
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::host_vector<unsigned int> h_off(cmprts->n_blocks + 1);

  unsigned int off = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    h_off[p * cmprts->n_blocks_per_patch] = off;
    off += n_prts_by_patch[p];
    // printf("set_n_prts p%d: %d\n", p, n_prts_by_patch[p]);
  }
  h_off[cmprts->n_blocks] = off;
  cmprts->n_prts = off;

  thrust::copy(h_off.begin(), h_off.end(), d_off);
}

// ----------------------------------------------------------------------
// cuda_mparticles_get_n_prts_by_patch

void
cuda_mparticles_get_n_prts_by_patch(struct cuda_mparticles *cmprts,
				    unsigned int *n_prts_by_patch)
{
  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::host_vector<unsigned int> h_off(d_off, d_off + cmprts->n_blocks + 1);

  for (int p = 0; p < cmprts->n_patches; p++) {
    n_prts_by_patch[p] = h_off[(p+1) * cmprts->n_blocks_per_patch] - h_off[p * cmprts->n_blocks_per_patch];
    //printf("p %d n_prts_by_patch %d\n", p, n_prts_by_patch[p]);
  }
}

// ----------------------------------------------------------------------
// cuda_mparticles_reorder

__global__ static void
k_cuda_mparticles_reorder(int nr_prts, unsigned int *d_ids,
		 float4 *xi4, float4 *pxi4,
		 float4 *alt_xi4, float4 *alt_pxi4)
{
  int i = threadIdx.x + THREADS_PER_BLOCK * blockIdx.x;

  if (i < nr_prts) {
    int j = d_ids[i];
    alt_xi4[i] = xi4[j];
    alt_pxi4[i] = pxi4[j];
  }
}

void
cuda_mparticles_reorder(struct cuda_mparticles *cmprts)
{
  if (!cmprts->need_reorder) {
    return;
  }
  
  dim3 dimGrid((cmprts->n_prts + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK);
  
  k_cuda_mparticles_reorder<<<dimGrid, THREADS_PER_BLOCK>>>
    (cmprts->n_prts, cmprts->d_id,
     cmprts->d_xi4, cmprts->d_pxi4,
     cmprts->d_alt_xi4, cmprts->d_alt_pxi4);
  
  cuda_mparticles_swap_alt(cmprts);

  cmprts->need_reorder = false;
}

// ----------------------------------------------------------------------
// cuda_mparticles_inject

void
cuda_mparticles_inject(struct cuda_mparticles *cmprts, struct cuda_mparticles_prt *buf,
		       unsigned int *buf_n_by_patch)
{
  if (cmprts->need_reorder) {
    cuda_mparticles_reorder(cmprts);
    cmprts->need_reorder = false;
  }
  
  unsigned int buf_n = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    buf_n += buf_n_by_patch[p];
    //    printf("p %d buf_n_by_patch %d\n", p, buf_n_by_patch[p]);
  }
  //  printf("buf_n %d\n", buf_n);

  thrust::host_vector<float4> h_xi4(buf_n);
  thrust::host_vector<float4> h_pxi4(buf_n);
  thrust::host_vector<unsigned int> h_bidx(buf_n);
  thrust::host_vector<unsigned int> h_id(buf_n);

  unsigned int off = 0;
  for (int p = 0; p < cmprts->n_patches; p++) {
    for (int n = 0; n < buf_n_by_patch[p]; n++) {
      float4 *xi4 = &h_xi4[off + n];
      float4 *pxi4 = &h_pxi4[off + n];
      cuda_mparticles_prt *prt = &buf[off + n];
      
      xi4->x  = prt->xi[0];
      xi4->y  = prt->xi[1];
      xi4->z  = prt->xi[2];
      xi4->w  = cuda_int_as_float(prt->kind);
      pxi4->x = prt->pxi[0];
      pxi4->y = prt->pxi[1];
      pxi4->z = prt->pxi[2];
      pxi4->w = prt->qni_wni;

      h_bidx[off + n] = get_block_idx(cmprts, *xi4, p);
      h_id[off + n] = cmprts->n_prts + off + n;
    }
    off += buf_n_by_patch[p];
  }
  assert(off == buf_n);

  thrust::device_ptr<unsigned int> d_off(cmprts->d_off);
  thrust::host_vector<unsigned int> h_off(d_off, d_off + cmprts->n_blocks + 1);
  unsigned int n_prts_by_patch[cmprts->n_patches];
  for (int p = 0; p < cmprts->n_patches; p++) {
    n_prts_by_patch[p] = h_off[(p+1) * cmprts->n_blocks_per_patch] - h_off[p * cmprts->n_blocks_per_patch];
    //printf("p %d n_prts_by_patch %d\n", p, n_prts_by_patch[p]);
  }

  //cuda_mparticles_check_in_patch_unordered_slow(cmprts, n_prts_by_patch);

  cuda_mparticles_find_block_indices_ids(cmprts, n_prts_by_patch);
  //cuda_mparticles_check_bidx_id_unordered_slow(cmprts, n_prts_by_patch);

  thrust::device_ptr<float4> d_xi4(cmprts->d_xi4);
  thrust::device_ptr<float4> d_pxi4(cmprts->d_pxi4);
  thrust::device_ptr<unsigned int> d_bidx(cmprts->d_bidx);
  thrust::device_ptr<unsigned int> d_id(cmprts->d_id);

  assert(cmprts->n_prts + buf_n <= cmprts->n_alloced);
  thrust::copy(h_xi4.begin(), h_xi4.end(), d_xi4 + cmprts->n_prts);
  thrust::copy(h_pxi4.begin(), h_pxi4.end(), d_pxi4 + cmprts->n_prts);
  thrust::copy(h_bidx.begin(), h_bidx.end(), d_bidx + cmprts->n_prts);
  //thrust::copy(h_id.begin(), h_id.end(), d_id + cmprts->n_prts);
  thrust::sequence(d_id, d_id + cmprts->n_prts + buf_n);

  // for (int i = -5; i <= 5; i++) {
  //   //    float4 xi4 = d_xi4[cmprts->n_prts + i];
  //   unsigned int bidx = d_bidx[cmprts->n_prts + i];
  //   unsigned int id = d_id[cmprts->n_prts + i];
  //   printf("i %d bidx %d %d\n", i, bidx, id);
  // }

  // cuda_mparticles_check_ordered(cmprts);

  cmprts->n_prts += buf_n;

  thrust::stable_sort_by_key(d_bidx, d_bidx + cmprts->n_prts, d_id);
  cuda_mparticles_reorder_and_offsets(cmprts);

  // cuda_mparticles_check_ordered(cmprts);
}

// ----------------------------------------------------------------------
// cuda_mparticles_set_particles

void
cuda_mparticles_set_particles(struct cuda_mparticles *cmprts, unsigned int n_prts, unsigned int off,
			      void (*get_particle)(struct cuda_mparticles_prt *prt, int n, void *ctx),
			      void *ctx)
{
  float4 *xi4  = new float4[n_prts];
  float4 *pxi4 = new float4[n_prts];
  
  for (int n = 0; n < n_prts; n++) {
    struct cuda_mparticles_prt prt;
    get_particle(&prt, n, ctx);

    for (int d = 0; d < 3; d++) {
      int bi = floorf(prt.xi[d] * cmprts->b_dxi[d]); // FIXME, consolidate the fint stuff
      if (bi < 0 || bi >= cmprts->b_mx[d]) {
	printf("XXX xi %g %g %g\n", prt.xi[0], prt.xi[1], prt.xi[2]);
	printf("XXX n %d d %d xi4[n] %g biy %d // %d\n",
	       n, d, prt.xi[d], bi, cmprts->b_mx[d]);
	if (bi < 0) {
	  prt.xi[d] = 0.f;
	} else {
	  prt.xi[d] *= (1. - 1e-6);
	}
      }
      bi = floorf(prt.xi[d] * cmprts->b_dxi[d]);
      assert(bi >= 0 && bi < cmprts->b_mx[d]);
    }

    xi4[n].x  = prt.xi[0];
    xi4[n].y  = prt.xi[1];
    xi4[n].z  = prt.xi[2];
    xi4[n].w  = cuda_int_as_float(prt.kind);
    pxi4[n].x = prt.pxi[0];
    pxi4[n].y = prt.pxi[1];
    pxi4[n].z = prt.pxi[2];
    pxi4[n].w = prt.qni_wni;
  }

  cuda_mparticles_to_device(cmprts, xi4, pxi4, n_prts, off);
  
  delete[] xi4;
  delete[] pxi4;
}

// ----------------------------------------------------------------------
// cuda_mparticles_get_particles

void
cuda_mparticles_get_particles(struct cuda_mparticles *cmprts, unsigned int n_prts, unsigned int off,
			      void (*put_particle)(struct cuda_mparticles_prt *, int, void *),
			      void *ctx)
{
  float4 *xi4  = new float4[n_prts];
  float4 *pxi4 = new float4[n_prts];

  cuda_mparticles_reorder(cmprts);
  cuda_mparticles_from_device(cmprts, xi4, pxi4, n_prts, off);
  
  for (int n = 0; n < n_prts; n++) {
    struct cuda_mparticles_prt prt;
    prt.xi[0]   = xi4[n].x;
    prt.xi[1]   = xi4[n].y;
    prt.xi[2]   = xi4[n].z;
    prt.kind    = cuda_float_as_int(xi4[n].w);
    prt.pxi[0]  = pxi4[n].x;
    prt.pxi[1]  = pxi4[n].y;
    prt.pxi[2]  = pxi4[n].z;
    prt.qni_wni = pxi4[n].w;

    put_particle(&prt, n, ctx);

#if 0
    for (int d = 0; d < 3; d++) {
      int bi = particle_single_real_fint(prt.xi[d] * cmprts->b_dxi[d]);
      if (bi < 0 || bi >= cmprts->b_mx[d]) {
	MHERE;
	mprintf("XXX xi %.10g %.10g %.10g\n", prt.xi[0], prt.xi[1], prt.xi[2]);
	mprintf("XXX n %d d %d xi %.10g b_dxi %.10g bi %d // %d\n",
		n, d, prt.xi[d] * cmprts->b_dxi[d], cmprts->b_dxi[d], bi, cmprts->b_mx[d]);
      }
    }
#endif
  }

  delete[] (xi4);
  delete[] (pxi4);
}

