
#pragma once

#include <math.h>

#include "common_moments.cxx"

// ======================================================================
// n

template<typename MP, typename MF>
struct Moment_n_1st_nc
{
  using Mparticles = MP;
  using Mfields = MF;
  using real_t = typename Mparticles::real_t;
  using particles_t = typename Mparticles::Patch;
  using fields_t = typename Mfields::fields_t;  
  
  constexpr static char const* name = "n_1st_nc";
  constexpr static int n_comps = 1;
  constexpr static fld_names_t fld_names() { return { "n" }; }
  constexpr static int flags = POFI_BY_KIND;
  
  static void run(Mfields& mflds, Mparticles& mprts)
  {
    const auto& grid = mprts.grid();
    real_t fnqs = grid.norm.fnqs;
    real_t dxi = 1.f / grid.domain.dx[0], dyi = 1.f / grid.domain.dx[1], dzi = 1.f / grid.domain.dx[2];

    for (int p = 0; p < mprts.n_patches(); p++) {
      auto flds = mflds[p];
      auto prts = mprts[p];
      for (auto prt: prts) {
	int m = prt.kind();
	DEPOSIT_TO_GRID_1ST_NC(prt, flds, m, 1.f);
      }
    }
  }
};

// ======================================================================
// rho

template<typename MP, typename MF>
struct Moment_rho_1st_nc
{
  using Mparticles = MP;
  using Mfields = MF;
  using real_t = typename Mparticles::real_t;
  using particles_t = typename Mparticles::Patch;
  using fields_t = typename Mfields::fields_t;  
  
  constexpr static char const* name = "rho_1st_nc";
  constexpr static int n_comps = 1;
  constexpr static fld_names_t fld_names() { return { "rho" }; }
  constexpr static int flags = 0;
  
  static void run(Mfields& mflds, Mparticles& mprts)
  {
    const auto& grid = mprts.grid();
    real_t fnqs = grid.norm.fnqs;
    real_t dxi = 1.f / grid.domain.dx[0], dyi = 1.f / grid.domain.dx[1], dzi = 1.f / grid.domain.dx[2];
    
    for (int p = 0; p < mprts.n_patches(); p++) {
      auto flds = mflds[p];
      auto prts = mprts[p];
      for (auto prt: prts) {
	DEPOSIT_TO_GRID_1ST_NC(prt, flds, 0, prts.prt_qni(prt));
      }
    }
  }
};

// ======================================================================
// v

template<typename MP, typename MF>
struct Moment_v_1st_nc
{
  using Mparticles = MP;
  using Mfields = MF;
  using real_t = typename Mparticles::real_t;
  using particles_t = typename Mparticles::Patch;
  using fields_t = typename Mfields::fields_t;  
  
  constexpr static char const* name = "v_1st_nc";
  constexpr static int n_comps = 3;
  constexpr static fld_names_t fld_names() { return { "vx", "vy", "vz" }; }
  constexpr static int flags = POFI_BY_KIND;
  
  static void run(Mfields& mflds, Mparticles& mprts)
  {
    const Grid_t& grid = mprts.grid();
    real_t fnqs = grid.norm.fnqs;
    real_t dxi = 1.f / grid.domain.dx[0], dyi = 1.f / grid.domain.dx[1], dzi = 1.f / grid.domain.dx[2];

    for (int p = 0; p < mprts.n_patches(); p++) {
      auto flds = mflds[p];
      auto prts = mprts[p];
      for (auto prt: prts) {
	int mm = prt.kind() * 3;
      
	real_t vxi[3];
	particle_calc_vxi(&prt, vxi);
	
	for (int m = 0; m < 3; m++) {
	  DEPOSIT_TO_GRID_1ST_NC(prt, flds, mm + m, vxi[m]);
	}
      }
    }
  }
};

template<typename Moment_t>
using Ops = FieldsItemMoment2Ops<Moment_t>;

#define MAKE_POFI_OPS(MP, MF, TYPE)					\
  Ops<Moment_n_1st_nc<MP, MF>> psc_output_fields_item_n_1st_nc_##TYPE##_ops; \
  Ops<Moment_rho_1st_nc<MP, MF>> psc_output_fields_item_rho_1st_nc_##TYPE##_ops; \
  Ops<Moment_v_1st_nc<MP, MF>> psc_output_fields_item_v_1st_nc_##TYPE##_ops; \

