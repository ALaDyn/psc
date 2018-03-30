
#pragma once

#include "fields.hxx"
#include "fields_item.hxx"

#define define_dxdydz(dx, dy, dz)			       \
  int dx _mrc_unused = (ppsc->grid().isInvar(0)) ? 0 : 1;      \
  int dy _mrc_unused = (ppsc->grid().isInvar(1)) ? 0 : 1;      \
  int dz _mrc_unused = (ppsc->grid().isInvar(2)) ? 0 : 1

// ======================================================================
// Item_dive

template<class MF>
struct Item_dive
{
  using Mfields = MF;
  using fields_t = typename Mfields::fields_t;
  using Fields = Fields3d<fields_t>;
  
  constexpr static char const* name = "dive";
  constexpr static int n_comps = 1;
  static fld_names_t fld_names() { return { "dive" }; }
  
  static void set(Fields& R, Fields&F, int i, int j, int k)
  {
    auto& grid = ppsc->grid();
    define_dxdydz(dx, dy, dz);
    R(0, i,j,k) = ((F(EX, i,j,k) - F(EX, i-dx,j,k)) / grid.domain.dx[0] +
		   (F(EY, i,j,k) - F(EY, i,j-dy,k)) / grid.domain.dx[1] +
		   (F(EZ, i,j,k) - F(EZ, i,j,k-dz)) / grid.domain.dx[2]);
  }
};

// ======================================================================
// Item_divj

// FIXME, almost same as dive

template<class MF>
struct Item_divj
{
  using Mfields = MF;
  using fields_t = typename Mfields::fields_t;
  using Fields = Fields3d<fields_t>;
  
  constexpr static char const* name = "divj";
  constexpr static int n_comps = 1;
  static fld_names_t fld_names() { return { "divj" }; }
  
  static void set(Fields& R, Fields&F, int i, int j, int k)
  {
    auto& grid = ppsc->grid();
    define_dxdydz(dx, dy, dz);
    R(0, i,j,k) = ((F(JXI, i,j,k) - F(JXI, i-dx,j,k)) / grid.domain.dx[0] +
		   (F(JYI, i,j,k) - F(JYI, i,j-dy,k)) / grid.domain.dx[1] +
		   (F(JZI, i,j,k) - F(JZI, i,j,k-dz)) / grid.domain.dx[2]);
  }
};

