
#ifndef PSC_CUDA_H
#define PSC_CUDA_H

#include "psc_fields_cuda.h"
#include "psc_particles_cuda.h"

#include <assert.h>
#include <math.h>
#include <psc.h>

// ======================================================================

#define check(a) do { int ierr = a; if (ierr != cudaSuccess) fprintf(stderr, "IERR = %d (%d)\n", ierr, cudaSuccess); assert(ierr == cudaSuccess); } while(0)

// ======================================================================

EXTERN_C void __particles_cuda_get(particles_cuda_t *pp);
EXTERN_C void __particles_cuda_put(particles_cuda_t *pp);
EXTERN_C void __fields_cuda_get(fields_cuda_t *pf);
EXTERN_C void __fields_cuda_put(fields_cuda_t *pf);

EXTERN_C void cuda_push_part_yz_a();
EXTERN_C void cuda_push_part_yz_b();
EXTERN_C void cuda_push_part_yz_b2();

struct d_particle {
  real xi[3];
  real qni_div_mni;
  real pxi[3];
  real qni_wni;
};

#define THREADS_PER_BLOCK 128
#define BLOCKSIZE_X 1
#define BLOCKSIZE_Y 1
#define BLOCKSIZE_Z 1

#define LOAD_PARTICLE(pp, d_p, n) do {					\
    (pp).xi[0]       = d_p.xi4[n].x;					\
    (pp).xi[1]       = d_p.xi4[n].y;					\
    (pp).xi[2]       = d_p.xi4[n].z;					\
    (pp).qni_div_mni = d_p.xi4[n].w;					\
    (pp).pxi[0]      = d_p.pxi4[n].x;					\
    (pp).pxi[1]      = d_p.pxi4[n].y;					\
    (pp).pxi[2]      = d_p.pxi4[n].z;					\
    (pp).qni_wni     = d_p.pxi4[n].w;					\
} while (0)

#define STORE_PARTICLE_POS(pp, d_p, n) do {				\
    d_p.xi4[n].x = (pp).xi[0];						\
    d_p.xi4[n].y = (pp).xi[1];						\
    d_p.xi4[n].z = (pp).xi[2];						\
    d_p.xi4[n].w = (pp).qni_div_mni;					\
} while (0)

#define STORE_PARTICLE_MOM(pp, d_p, n) do {				\
    d_p.pxi4[n].x = (pp).pxi[0];					\
    d_p.pxi4[n].y = (pp).pxi[1];					\
    d_p.pxi4[n].z = (pp).pxi[2];					\
    d_p.pxi4[n].w = (pp).qni_wni;					\
} while (0)

#endif
