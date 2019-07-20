
#pragma once

template <typename Mfields>
class Deposit1stCc
{
public:
  using FE = typename Mfields::fields_view_t;
  using R = typename Mfields::real_t;
  
  Deposit1stCc(const Grid_t& grid, FE flds)
    : flds_{flds},
      fnqs_{R(grid.norm.fnqs)},
      dxi_{R(1.f / grid.domain.dx[0]), R(1.f / grid.domain.dx[1]), R(1.f / grid.domain.dx[2])},
      is_invar_{grid.isInvar(0), grid.isInvar(1), grid.isInvar(2)},
      ldims_{grid.ldims}
  {}
  
  template<typename PRT>
  void operator()(const PRT& prt, int m, R val)
  {
    auto xi = prt.x(); /* don't shift back in time */
    R u = xi[0] * dxi_[0] - .5f;
    R v = xi[1] * dxi_[1] - .5f;
    R w = xi[2] * dxi_[2] - .5f;
    
    int jx = fint(u);
    int jy = fint(v);
    int jz = fint(w);
    R h1 = u - jx;
    R h2 = v - jy;
    R h3 = w - jz;
    
    R g0x = 1.f - h1;
    R g0y = 1.f - h2;
    R g0z = 1.f - h3;
    R g1x = h1;
    R g1y = h2;
    R g1z = h3;
    
    int jxd = 1, jyd = 1, jzd = 1;
    if (is_invar_[0]) {
      jx = 0;
      g0x = 1.;
      g1x = 0.;
      jxd = 0;
    }
    if (is_invar_[1]) {
      jy = 0;
      g0y = 1.;
      g1y = 0.;
      jyd = 0;
    }
    if (is_invar_[2]) {
      jz = 0;
      g0z = 1.;
      g1z = 0.;
      jzd = 0;
    }
    
    assert(jx >= -1 && jx < ldims_[0]);
    assert(jy >= -1 && jy < ldims_[1]);
    assert(jz >= -1 && jz < ldims_[2]);
    
    R fnq = prt.w() * fnqs_;
    
    flds_(m, jx, jy, jz) += fnq * g0x * g0y * g0z * (val);
    flds_(m, jx + jxd, jy, jz) += fnq * g1x * g0y * g0z * (val);
    flds_(m, jx, jy + jyd, jz) += fnq * g0x * g1y * g0z * (val);
    flds_(m, jx + jxd, jy + jyd, jz) += fnq * g1x * g1y * g0z * (val);
    flds_(m, jx, jy, jz + jzd) += fnq * g0x * g0y * g1z * (val);
    flds_(m, jx + jxd, jy, jz + jzd) += fnq * g1x * g0y * g1z * (val);
    flds_(m, jx, jy + jyd, jz + jzd) += fnq * g0x * g1y * g1z * (val);
    flds_(m, jx + jxd, jy + jyd, jz + jzd) += fnq * g1x * g1y * g1z * (val);
  }

private:
  Fields3d<FE> flds_;
  R fnqs_;
  Vec3<R> dxi_;
  Vec3<bool> is_invar_;
  Int3 ldims_;
};
  
