
#pragma once

// ----------------------------------------------------------------------
// Item_vpic_fields

struct Item_vpic_fields
{
  using Mfields = MfieldsSingle;
  using MfieldsState = MfieldsStateVpic;
  using Fields = Fields3d<typename Mfields::fields_t>;
  using FieldsState = Fields3d<typename MfieldsState::fields_t>;

  constexpr static const char* name = "fields_vpic";
  constexpr static int n_comps = 16;
  constexpr static fld_names_t fld_names()
  {
    return { "jx_ec", "jy_ec", "jz_ec",
	     "ex_ec", "ey_ec", "ez_ec",
	     "hx_fc", "hy_fc", "hz_fc",
	     "tcax_ec", "tcay_ec", "tcaz_ec",
	     "div_e_err_nc", "div_b_err_cc",
	     "rhob_nc", "rhof_nc", };
  }

  static void run(Mfields& mflds, Mfields& mres)
  {
    auto& grid = mflds.grid();
    
    for (int p = 0; p < mres.n_patches(); p++) {
      Fields F(mflds[p]), R(mres[p]);
      grid.Foreach_3d(0, 0, [&](int ix, int iy, int iz) {
	  for (int m = 0; m < 16; m++) {
	    R(m, ix,iy,iz) = F(m, ix,iy,iz);
	  }
	});
    }
  }
};

// ----------------------------------------------------------------------
// Moment_vpic_hydro

struct Moment_vpic_hydro : ItemMomentCRTP<Moment_vpic_hydro, MfieldsSingle>
{
  using Base = ItemMomentCRTP<Moment_vpic_hydro, MfieldsSingle>;
  using Mfields = MfieldsSingle;
  using MfieldsState = MfieldsStateVpic;
  using Mparticles = MparticlesVpic;
  using Fields = Fields3d<typename Mfields::fields_t>;
  
  constexpr static char const* name = "hydro";
  constexpr static int n_comps = MfieldsHydroVpic::N_COMP;
  constexpr static fld_names_t fld_names()
  {
    return { "jx_nc", "jy_nc", "jz_nc", "rho_nc",
	     "px_nc", "py_nc", "pz_nc", "ke_nc",
	     "txx_nc", "tyy_nc", "tzz_nc", "tyz_nc",
	     "tzx_nc", "txy_nc", "_pad0", "_pad1", };
  }
  constexpr static int flags = POFI_BY_KIND;

  Moment_vpic_hydro(const Grid_t& grid, MPI_Comm comm)
    : Base(grid, comm)
  {}
  
  void run(MparticlesVpic& mprts)
  {
    const auto& kinds = mprts.grid().kinds;
    auto& mres = this->mres_;
    auto& grid = mprts.grid();
    auto mf_hydro = MfieldsHydroVpic{ppsc->grid(), 16, { 1, 1, 1 }};
    Simulation *sim;
    psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sim);
    
    for (int kind = 0; kind < kinds.size(); kind++) {
      HydroArray *vmflds_hydro = mf_hydro.vmflds_hydro;
      sim->moments_run(vmflds_hydro, &mprts.vmprts_, kind);
      
      for (int p = 0; p < mres.n_patches(); p++) {
	Fields R(mres[p]);
	auto F = mf_hydro[p];
	grid.Foreach_3d(0, 0, [&](int ix, int iy, int iz) {
	    for (int m = 0; m < MfieldsHydroVpic::N_COMP; m++) {
	      R(m + kind * MfieldsHydroVpic::N_COMP, ix,iy,iz) = F(m, ix,iy,iz);
	    }
	  });
      }
    }
  }
};

