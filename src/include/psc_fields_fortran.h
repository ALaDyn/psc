
#ifndef PSC_FIELD_FORTRAN_H
#define PSC_FIELD_FORTRAN_H

#include "fields3d.hxx"

struct fields_fortran_t : fields3d<double>
{
  using Base = fields3d<double>;
  using mfields_t = mfields_base<fields_fortran_t>;

  using Base::Base;
};

struct psc_mfields_fortran_sub
{
  using fields_t = fields_fortran_t;
  
  fields_t::real_t ***data;
  int ib[3]; //> lower left corner for each patch (incl. ghostpoints)
  int im[3]; //> extent for each patch (incl. ghostpoints)
};

using mfields_fortran_t = mfields_base<psc_mfields_fortran_sub>;

template<>
inline fields_fortran_t mfields_fortran_t::operator[](int p)
{
  fields_fortran_t psc_mfields_fortran_get_field_t(struct psc_mfields *mflds, int p);
  return psc_mfields_fortran_get_field_t(mflds_, p);
}

#endif
