
// ======================================================================
// field caching

// OPT: precalc offsets into fld_cache (including ci[])
// OPT: use more shmem?

#if EM_CACHE == EM_CACHE_NONE

#define F3_CACHE(flds_em, m, jx, jy, jz)	\
  (F3_DEV(flds_em, m, jx,jy,jz))

#define DECLARE_EM_CACHE(flds_em, d_flds, size, ci0)	\
  real *flds_em = d_flds

#elif EM_CACHE == EM_CACHE_CUDA

#if DIM == DIM_YZ
#define BLOCKBND_X 0
#define BLOCKBND_Y 2
#define BLOCKBND_Z 2
#elif DIM == DIM_XYZ
#define BLOCKBND_X 2
#define BLOCKBND_Y 2
#define BLOCKBND_Z 2
#endif

#define BLOCKGSIZE_X (BLOCKSIZE_X + 2 * BLOCKBND_X)
#define BLOCKGSIZE_Y (BLOCKSIZE_Y + 2 * BLOCKBND_Y)
#define BLOCKGSIZE_Z (BLOCKSIZE_Z + 2 * BLOCKBND_Z)

#if DIM == DIM_YZ
#define F3_CACHE(flds_em, m, jx, jy, jz)				\
  ((flds_em)[(((m-EX)							\
	       *BLOCKGSIZE_Z + ((jz-ci0[2])-(-BLOCKBND_Z)))		\
	      *BLOCKGSIZE_Y + ((jy-ci0[1])-(-BLOCKBND_Y)))])
#elif DIM == DIM_XYZ
#define F3_CACHE(flds_em, m, jx, jy, jz)				\
  ((flds_em)[((((m-EX)							\
		*BLOCKGSIZE_Z + ((jz-ci0[2])-(-BLOCKBND_Z)))		\
	       *BLOCKGSIZE_Y + ((jy-ci0[1])-(-BLOCKBND_Y)))		\
	      *BLOCKGSIZE_X + ((jx-ci0[0])-(-BLOCKBND_X)))])
#endif

__device__ static void
cache_fields(float *flds_em, float *d_flds, int size, int *ci0)
{
  int ti = threadIdx.x;
  while (ti < n) {
    int tmp = ti;
    int jx = tmp % BLOCKGSIZE_X - BLOCKBND_X;
    tmp /= dims[0];
    int jy = tmp % BLOCKGSIZE_Y - BLOCKBND_Y;
    tmp /= dims[1];
    int jz = tmp % BLOCKGSIZE_Z - BLOCKBND_Z;
    // OPT? currently it seems faster to do the loop rather than do m by threadidx
    for (int m = EX; m <= HZ; m++) {
      F3_CACHE(flds_em, m, jx+ci0[0],jy+ci0[1] jz+ci0[2]) = 
	F3_DEV(d_flds, m, jx+ci0[0],jy+ci0[1],jz+ci0[2]);
    }
    ti += THREADS_PER_BLOCK;
  }
}

#define DECLARE_EM_CACHE(flds_em, d_flds, size, ci0)	\
  __shared__ real flds_em[6 * BLOCKGSIZE_X * BLOCKGSIZE_Y * BLOCKGSIZE_Z];\
  cache_fields(flds_em, d_flds, size, ci0)

#endif

