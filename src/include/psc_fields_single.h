
#ifndef PSC_FIELD_SINGLE_H
#define PSC_FIELD_SINGLE_H

#include <mpi.h>
#include "fields3d.hxx"
#include "fields_traits.hxx"

using fields_single_real_t = float;

struct fields_single_t : fields3d<fields_single_real_t>
{
  using Base = fields3d<fields_single_real_t>;
  using mfields_t = PscMfields<fields_single_t>;

  using Base::Base;
};

using MfieldsSingle = psc_mfields_<fields_single_t>;
using PscMfieldsSingle = PscMfields<MfieldsSingle>;

template<>
struct fields_traits<fields_single_t>
{
  static constexpr const char* name = "single";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};

#endif
