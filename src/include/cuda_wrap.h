
#ifndef CUDA_WRAP_H
#define CUDA_WRAP_H

#define check(a) do { int ierr = a; if (ierr != cudaSuccess) fprintf(stderr, "IERR = %d (%d)\n", ierr, cudaSuccess); assert(ierr == cudaSuccess); } while(0)

#ifndef __CUDACC__

// ======================================================================
// CUDA emulation

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

static struct {
  int x, y;
} threadIdx __unused;

static struct {
  int x, y;
} blockIdx __unused;

static struct {
  int x, y;
} blockDim __unused;

#define RUN_KERNEL(dimGrid, dimBlock, func, params) do {		\
    blockDim.x = dimBlock[0];						\
    blockDim.y = dimBlock[1];						\
    for (blockIdx.y = 0; blockIdx.y < dimGrid[1]; blockIdx.y++) {	\
      for (blockIdx.x = 0; blockIdx.x < dimGrid[0]; blockIdx.x++) {	\
	for (threadIdx.y = 0; threadIdx.y < dimBlock[1]; threadIdx.y++) { \
	  for (threadIdx.x = 0; threadIdx.x < dimBlock[0]; threadIdx.x++) { \
	    func params;						\
	  }								\
	}								\
      }									\
    }									\
  } while (0)

//#define __syncthreads() do {} while (0)

#define __device__
#define __global__
#define __constant__
#define __shared__

enum {
  cudaSuccess,
};

enum {
  cudaMemcpyHostToDevice,
  cudaMemcpyDeviceToHost,
};

static inline int
cudaMalloc(void **p, size_t len)
{
  *p = malloc(len);
  return cudaSuccess;
}

static inline int
cudaMemcpy(void *to, void *from, size_t len, int dir)
{
  memcpy(to, from, len);
  return cudaSuccess;
}

static inline int
cudaFree(void *p)
{
  free(p);
  return cudaSuccess;
}

#define cudaMemcpyToSymbol(to, from, len)		\
  cudaMemcpy(&to, from, len, cudaMemcpyHostToDevice) 
 
typedef struct {
  float x, y, z, w;
} float4;

#define EXTERN_C

static inline float
rsqrtf(float x)
{
  return 1.f / sqrtf(x);
}

static inline int
cuda_nint(real x)
{
  // FIXME?
  return (int)(x + real(10.5)) - 10;
}

static inline int
cuda_fint(real x)
{
  // FIXME?
  return (int)(x + real(10.)) - 10;
}

#else

static bool CUDA_SYNC = true;

static inline void
cuda_sync_if_enabled()
{
  if (CUDA_SYNC) {
    check(cudaThreadSynchronize());
  }
}

#define RUN_KERNEL(dimGrid, dimBlock, func, params) do {	\
    dim3 dG(dimGrid[0], dimGrid[1]);				\
    dim3 dB(dimBlock[0], dimBlock[1]);				\
    func<<<dG, dB>>>params;					\
    cuda_sync_if_enabled();					\
  } while (0)

#define EXTERN_C extern "C"

__device__ static inline int
cuda_nint(real x)
{
  return __float2int_rn(x);
}

__device__ __host__ static inline int
cuda_fint(real x)
{
  return (int)(x + real(10.)) - 10;
}

#endif

#define fabsr fabsf

#define rsqrtr rsqrtf

#endif
