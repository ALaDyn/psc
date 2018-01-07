
#ifndef PSC_FIELDS_CUDA_H
#define PSC_FIELDS_CUDA_H

#include <mpi.h>
#include "fields3d.hxx"
#include "fields_traits.hxx"

struct fields_cuda_t
{
  using mfields_t = mfields3d<fields_cuda_t>;
};

using mfields_cuda_t = mfields3d<fields_cuda_t>;

template<>
struct fields_traits<fields_cuda_t>
{
  static constexpr const char* name = "cudax`x`";
};

// ----------------------------------------------------------------------

struct psc_mfields_cuda {
  struct cuda_mfields *cmflds;
};

#define psc_mfields_cuda(pf) mrc_to_subobj(pf, struct psc_mfields_cuda)

#endif
