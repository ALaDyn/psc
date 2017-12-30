
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

using DIM_XYZ = Invar<false, false, false>;
using DIM_XY  = Invar<false, false, true >;
using DIM_XZ  = Invar<false, true , false>;
using DIM_YZ  = Invar<true , false, false>;

template<typename F>
struct fields_traits
{
};

template<typename F, typename D = DIM_XYZ>
class Fields3d
{
public:
  using fields_t = F;
  using real_t = typename fields_traits<F>::real_t;
  using DIM = D;

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
    int i = DIM::InvarX::value ? 0 : i_;
    int j = DIM::InvarY::value ? 0 : j_;
    int k = DIM::InvarZ::value ? 0 : k_;

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


#endif
