
#ifndef GRID_HXX
#define GRID_HXX

#include "vec3.hxx"
#include <vector>
#include <cstring>

// ======================================================================
// Grid_

template<class T>
struct Grid_
{
  using real_t = T;
  using Real3 = Vec3<real_t>;
  
  struct Patch
  {
    Real3 xb;
    Real3 xe;
  };

  struct Kind
  {
    Kind() // FIXME, do we want to keep this ctor?
    {
    }
    
    Kind(real_t q_, real_t m_, const char *name_)
      : q(q_), m(m_), name(strdup(name_))
    {
    };

    Kind(const Kind& k)
      : q(k.q), m(k.m), name(strdup(k.name))
    {
    }

    Kind(Kind&& k)
      : q(k.q), m(k.m), name(k.name)
    {
      k.name = nullptr;
    }

    ~Kind()
    {
      free((void*) name);
    }
    
    real_t q;
    real_t m;
    const char *name;
  };
  
  Grid_() = default;
  Grid_(const Grid_& grid) = delete;

  Int3 gdims;
  Int3 ldims;
  Real3 dx;
  // FIXME? these defaults, in particular for dt, might be a bit
  // dangerous, as they're useful for testing but might hide if one
  // forgets to init them correctly for an real simulation
  real_t fnqs = { 1. };
  real_t eta = { 1. };
  real_t dt = { 1. };
  std::vector<Patch> patches;
  std::vector<Kind> kinds;
};

using Grid_t = Grid_<double>;

#endif

