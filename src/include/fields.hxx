
#ifndef FIELDS_HXX
#define FIELDS_HXX

#include <type_traits>

template<bool IX = false, bool IY = false, bool IZ = false>
struct Invar
{
  using InvarX = std::integral_constant<bool, IX>;
  using InvarY = std::integral_constant<bool, IY>;
  using InvarZ = std::integral_constant<bool, IZ>;
};

using dim_xyz = Invar<false, false, false>;
using dim_xy  = Invar<false, false, true >;
using dim_xz  = Invar<false, true , false>;
using dim_yz  = Invar<true , false, false>;
using dim_1   = Invar<true , true , true >;

template<typename F>
struct fields_traits
{
};

template<typename F, typename D = dim_xyz>
class Fields3d
{
public:
  using fields_t = F;
  using real_t = typename fields_traits<F>::real_t;
  using dim = D;

  Fields3d(const fields_t& f)
    : data_(f.data),
      n_comp_(f.nr_comp),
      first_comp_(f.first_comp)
  {
    for (int d = 0; d < 3; d++) {
      ib[d] = f.ib[d];
      im[d] = f.im[d];
    }
  }

  const real_t operator()(int m, int i, int j, int k) const
  {
    return data_[index(m, i, j, k)];
  }

  real_t& operator()(int m, int i, int j, int k)
  {
    return data_[index(m, i, j, k)];
  }

private:
  int index(int m, int i_, int j_, int k_)
  {
    int i = dim::InvarX::value ? 0 : i_;
    int j = dim::InvarY::value ? 0 : j_;
    int k = dim::InvarZ::value ? 0 : k_;

#ifdef BOUNDS_CHECK
    assert(m >= first_comp_ && m < n_comp_);
    assert(i >= ib[0] && i < ib[0] + im[0]);
    assert(j >= ib[1] && j < ib[1] + im[1]);
    assert(k >= ib[2] && k < ib[2] + im[2]);
#endif
  
      return ((((((m) - first_comp_)
	       * im[2] + (k - ib[2]))
	      * im[1] + (j - ib[1]))
	     * im[0] + (i - ib[0])));
      }

private:
  real_t *data_;
  int ib[3], im[3];
  int n_comp_;
  int first_comp_;
};

#include "psc_fields_single.h"
#include "psc_fields_c.h"
#include "psc_fields_fortran.h"

template<>
struct fields_traits<fields_single_t>
{
  using real_t = fields_single_real_t;
  static constexpr const char* name = "single";
  static fields_single_t get_patch(struct psc_mfields *mflds, int p) { return fields_single_t_mflds(mflds, p); }
};

template<>
struct fields_traits<fields_c_t>
{
  using real_t = fields_c_real_t;
  static constexpr const char* name = "c";
  static fields_c_t get_patch(struct psc_mfields *mflds, int p) { return fields_c_t_mflds(mflds, p); }
};

template<>
struct fields_traits<fields_fortran_t>
{
  using real_t = fields_fortran_real_t;
  static constexpr const char* name = "fortran";
  static fields_fortran_t get_patch(struct psc_mfields *mflds, int p) { return fields_fortran_t_mflds(mflds, p); }
};


#endif
