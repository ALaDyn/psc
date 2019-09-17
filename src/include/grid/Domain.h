
#ifndef GRID_DOMAIN_H
#define GRID_DOMAIN_H

#include <mrc_common.h>
#include <iostream>

namespace psc
{
namespace grid
{

// ======================================================================
// Domain

template <class R>
struct Domain
{
  using Real = R;
  using Real3 = Vec3<R>;

  Domain() {}

  Domain(Int3 gdims, Real3 length, Real3 corner = {0., 0., 0.},
         Int3 np = {1, 1, 1})
    : gdims(gdims), length(length), corner(corner), np(np)
  {
    for (int d = 0; d < 3; d++) {
      assert(gdims[d] % np[d] == 0);
      ldims[d] = gdims[d] / np[d];
    }
    dx = length / Real3(gdims);
  }

  void view() const
  {
    mprintf("Grid_::Domain: gdims %d x %d x %d\n", gdims[0], gdims[1],
            gdims[2]);
  }

  bool isInvar(int d) const { return gdims[d] == 1; }

  Int3 gdims;   ///< Number of grid-points in each dimension
  Real3 length; ///< The physical size of the simulation-box
  Real3 corner;
  Int3 np; ///< Number of patches in each dimension
  Int3 ldims;
  Real3 dx;
};

template <typename R>
inline std::ostream& operator<<(std::ostream& os, const Domain<R>& domain)
{
  os << "Domain{gdims=" << domain.gdims;
  os << ", length=" << domain.length;
  os << ", corner=" << domain.corner;
  os << ", np=" << domain.np;
  os << ", ldims=" << domain.ldims;
  os << ", dx=" << domain.dx;
  os << "}";
  return os;
}

} // namespace grid
} // namespace psc

#endif
