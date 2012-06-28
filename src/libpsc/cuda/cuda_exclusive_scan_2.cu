
#include "psc_cuda.h"
#include "particles_cuda.h"

#include <thrust/functional.h>
#include <thrust/transform_scan.h>

struct count_if_equal : public thrust::unary_function<unsigned int, unsigned int> {
  const unsigned int value;

  __device__ __host__ count_if_equal(unsigned int _value) : value(_value) { }

  __device__ __host__ unsigned int operator()(unsigned int value_in) {
    return value_in == value;
  }
};

EXTERN_C int
cuda_exclusive_scan_2(struct psc_particles *prts, unsigned int *_d_vals,
		      unsigned int *_d_sums)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  thrust::device_ptr<unsigned int> d_vals(_d_vals);
  thrust::device_ptr<unsigned int> d_sums(_d_sums);

  count_if_equal unary_op(cuda->nr_blocks);
  thrust::transform_exclusive_scan(d_vals, d_vals + prts->n_part, d_sums, unary_op,
				   0, thrust::plus<unsigned int>());

  // OPT, don't mv to host
  int sum = d_sums[prts->n_part - 1] + (d_vals[prts->n_part - 1] == cuda->nr_blocks);
  return sum;
}

EXTERN_C int
_cuda_exclusive_scan_2(struct psc_particles *prts, unsigned int *d_bidx,
		       unsigned int *d_sums)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
  unsigned int *bidx = new unsigned int[prts->n_part];
  unsigned int *sums = new unsigned int[prts->n_part];
  check(cudaMemcpy(bidx, d_bidx, prts->n_part * sizeof(*bidx),
		   cudaMemcpyDeviceToHost));

  unsigned int sum = 0;
  for (int i = 0; i < prts->n_part; i++) {
    sums[i] = sum;
    sum += (bidx[i] == cuda->nr_blocks ? 1 : 0);
  }

  check(cudaMemcpy(d_sums, sums, prts->n_part * sizeof(*d_sums),
		   cudaMemcpyHostToDevice));
  delete[] sums;
  delete[] bidx;
  return sum;
}

void
cuda_mprts_scan_send_buf(struct cuda_mprts *cuda_mprts)
{
  for (int p = 0; p < cuda_mprts->nr_patches; p++) {
    struct psc_particles *prts = cuda_mprts->mprts_cuda[p];
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    cuda->bnd_n_send = cuda_exclusive_scan_2(prts, cuda->d_part.bidx, cuda->d_part.sums);
    cuda->bnd_n_part_save = prts->n_part;
  }
}
