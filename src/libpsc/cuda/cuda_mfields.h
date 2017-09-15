
#ifndef CUDA_MFIELDS_H
#define CUDA_MFIELDS_H

#include "cuda_iface.h"

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

// FIXME, better call it cuda_mfields_real_t
typedef float fields_cuda_real_t;

// ----------------------------------------------------------------------
// cuda_mfields

struct cuda_mfields {
  fields_cuda_real_t *d_flds;
};

#endif
