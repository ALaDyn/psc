
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
// calculate currents

EXTERN_C void
PFX(cuda_push_part_p3)(particles_cuda_t *pp, fields_cuda_t *pf, real *dummy,
		       int block_stride)
{
  unsigned int size = pf->im[0] * pf->im[1] * pf->im[2];
  check(cudaMemset(pf->d_flds + JXI * size, 0,
		   3 * size * sizeof(*pf->d_flds)));

  assert(pp->nr_blocks % block_stride == 0);
  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { pp->nr_blocks / block_stride, 1 };
  for (int block_start = 0; block_start < block_stride; block_start++) {
    RUN_KERNEL(dimGrid, dimBlock,
	       push_part_p2, (pp->n_part, pp->d_part, pf->d_flds,
			      block_stride, block_start));
  }
}

