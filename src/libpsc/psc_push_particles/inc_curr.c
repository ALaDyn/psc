
// ----------------------------------------------------------------------
// curr_add

#if PSC_FIELDS_AS_CUDA2

typedef fields_real_t * flds_curr_t;

CUDA_DEVICE static inline void
curr_add(flds_curr_t flds_curr, int m, int jx, int jy, int jz, real val)
{
  real *addr = &F3_DEV(flds_curr, JXI+m, jx,jy,jz);
#ifdef __CUDACC__
  atomicAdd(addr, val);
#else
  *addr += val;
#endif
}

#else

typedef struct psc_fields * flds_curr_t;

CUDA_DEVICE static inline void
curr_add(flds_curr_t flds_curr, int m, int jx, int jy, int jz, real val)
{
  F3_CURR(flds_curr, JXI+m, jx,jy,jz) += val;
}

#endif

// ----------------------------------------------------------------------

#if CALC_J == CALC_J_1VB_SPLIT
#include "inc_curr_1vb_split.c"
#elif CALC_J == CALC_J_1VB_VAR1
#include "inc_curr_1vb_var1.c"
#elif CALC_J == CALC_J_1VB_2D
#include "inc_curr_1vb_2d.c"
#endif

