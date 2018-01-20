
#ifndef PSC_FIELDS_CUDA_H
#define PSC_FIELDS_CUDA_H

#include <mpi.h>
#include "fields3d.hxx"
#include "fields_traits.hxx"

#include "psc_fields_single.h"

#include "mrc_json.h"

struct fields_cuda_t
{
  using real_t = float;
  using mfields_t = mfields_base<fields_cuda_t>;
};

struct psc_mfields_cuda
{
  using fields_t = fields_cuda_t;
  
  psc_mfields_cuda(Grid_t& grid, int n_fields, const Int3& ibn);
  psc_mfields_cuda(const psc_mfields_cuda&) = delete;
  ~psc_mfields_cuda();

  fields_single_t get_host_fields();
  void copy_to_device(int p, fields_single_t h_flds, int mb, int me);
  void copy_from_device(int p, fields_single_t h_flds, int mb, int me);

  struct cuda_mfields *cmflds;
};

using mfields_cuda_t = mfields_base<psc_mfields_cuda>;

template<>
struct fields_traits<fields_cuda_t>
{
  static constexpr const char* name = "cuda";
};

// ----------------------------------------------------------------------

#endif
