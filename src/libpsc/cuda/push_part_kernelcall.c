
// ----------------------------------------------------------------------
// cuda_push_part_p1
//
// alloc scratch

EXTERN_C void
PFX(cuda_push_part_p1)(particles_cuda_t *pp, fields_cuda_t *pf,
		       real **d_scratch)
{
  int size = pp->nr_blocks * 3 * BLOCKSTRIDE;
  check(cudaMalloc((void **)d_scratch, size * sizeof(real)));
  check(cudaMemset(*d_scratch, 0, size * sizeof(real)));
  cudaThreadSynchronize(); // FIXME
}

// ----------------------------------------------------------------------
// cuda_push_part_p2
//
// particle push

EXTERN_C void
PFX(cuda_push_part_p2)(particles_cuda_t *pp, fields_cuda_t *pf)
{
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { pp->nr_blocks, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     push_part_p1, (pp->n_part, pp->d_part, pf->d_flds));
}

// ----------------------------------------------------------------------
// cuda_push_part_p3
//
// calculate currents locally

EXTERN_C void
PFX(cuda_push_part_p3)(particles_cuda_t *pp, fields_cuda_t *pf, real *d_scratch)
{
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { pp->nr_blocks, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     push_part_p2, (pp->n_part, pp->d_part, pf->d_flds, d_scratch));
}

// ----------------------------------------------------------------------
// cuda_push_part_p4
//
// collect calculation

EXTERN_C void
PFX(cuda_push_part_p4)(particles_cuda_t *pp, fields_cuda_t *pf, real *d_scratch)
{
  unsigned int size = pf->im[0] * pf->im[1] * pf->im[2];
  check(cudaMemset(pf->d_flds + JXI * size, 0,
		   3 * size * sizeof(*pf->d_flds)));

#if DIM == DIM_Z
  int dimBlock[2] = { BLOCKSIZE_Z + 2*SW, 1 };
#elif DIM == DIM_YZ
  int dimBlock[2]  = { BLOCKSIZE_Y + 2*SW, BLOCKSIZE_Z + 2*SW };
#endif
  int dimGrid[2]  = { 1, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     collect_currents, (pf->d_flds, d_scratch, pp->nr_blocks));

  int sz = pp->nr_blocks * 3 * BLOCKSTRIDE;
  real *h_scratch = (real *) malloc(sz * sizeof(real));
  check(cudaMemcpy(h_scratch, d_scratch, sz * sizeof(real),
		   cudaMemcpyDeviceToHost));

#define h_scratch(m,jy,jz) (p[(m)*BLOCKSTRIDE +		\
			      ((jz)+3) * (BLOCKSIZE_Y+6) +	\
			      (jy)+3])
  for (int m = 0; m < 3; m++) {
    printf("m %d\n", m);
    for (int b = 0; b < pp->nr_blocks; b++) {
      real *p = h_scratch + b * 3 * BLOCKSTRIDE;
      for (int iz = -3; iz <= 3; iz++) {
	if (h_scratch(m, 0, iz) != 0.) {
	  printf("b %d iz %d : %g\n", b, iz, h_scratch(m, 0, iz));
	}
      }
    }
  }
  free(h_scratch);

  for (int b = 0; b < pp->nr_blocks; b++) {
    real *p = h_scratch + b * 3 * BLOCKSTRIDE;
    for (int m = 0; m < 3; m++) {
      for (int iz = -3; iz <= 3; iz++) {
	F3_CUDA(pf, m, 0,0,b+iz) += h_scratch(m, 0,iz);
      }
    }
  }
  for (int iz = -3; iz < 10+3; iz++) {
    printf("iz %d: %g\n", iz, F3_CUDA(pf, 2, 0,0,iz));
  }
}

// ----------------------------------------------------------------------
// cuda_push_part_p5
//
// free

EXTERN_C void
PFX(cuda_push_part_p5)(particles_cuda_t *pp, fields_cuda_t *pf, real *d_scratch)
{
  check(cudaFree(d_scratch));
}

