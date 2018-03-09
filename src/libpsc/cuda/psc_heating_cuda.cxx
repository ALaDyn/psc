
#include <psc_heating_private.h>

#include "cuda_iface.h"
#include "psc_particles_cuda.h"
#include "heating.hxx"

#include <stdlib.h>
#include <string.h>

// ======================================================================
// psc_heating subclass "cuda"

struct HeatingCuda : HeatingBase
{
  // ----------------------------------------------------------------------
  // ctor

  HeatingCuda(int every_step, int tb, int te, int kind,
	   psc_heating_spot& spot)
    : every_step_(every_step),
      tb_(tb), te_(te),
      kind_(kind),
      spot_(spot)
  {
    struct cuda_heating_foil foil;
    double val;

    psc_heating_spot_get_param_double(&spot_, "zl", &val);
    foil.zl = val;
    psc_heating_spot_get_param_double(&spot_, "zh", &val);
    foil.zh = val;
    psc_heating_spot_get_param_double(&spot_, "xc", &val);
    foil.xc = val;
    psc_heating_spot_get_param_double(&spot_, "yc", &val);
    foil.yc = val;
    psc_heating_spot_get_param_double(&spot_, "rH", &val);
    foil.rH = val;
    psc_heating_spot_get_param_double(&spot_, "T", &val);
    foil.T = val;
    psc_heating_spot_get_param_double(&spot_, "Mi", &val);
    foil.Mi = val;
    foil.kind = kind_;
    foil.heating_dt = every_step_ * ppsc->dt;

    cuda_heating_setup_foil(&foil);
  }

  // ----------------------------------------------------------------------
  // run

  void run(psc_mparticles* mprts_base) override
  {
    struct psc *psc = ppsc;

    // only heating between heating_tb and heating_te
    if (psc->timestep < tb_ || psc->timestep >= te_) {
      return;
    }

    if (psc->timestep % every_step_ != 0) {
      return;
    }

    assert(strcmp(psc_mparticles_type(mprts_base), "cuda") == 0);
    struct psc_mparticles *mprts = mprts_base;

    struct cuda_mparticles *cmprts = PscMparticlesCuda(mprts)->cmprts();
    assert(strcmp(psc_heating_spot_type(&spot_), "foil") == 0);
    cuda_heating_run_foil(cmprts);
  }

private:
  int every_step_;
  int tb_, te_;
  int kind_;
  psc_heating_spot& spot_;
};

// ----------------------------------------------------------------------
// psc_heating "cuda"

struct psc_heating_ops_cuda : psc_heating_ops {
  using PscHeating_t = PscHeatingWrapper<HeatingCuda>;
  psc_heating_ops_cuda() {
    name                = "cuda";
    size                = PscHeating_t::size;
    setup               = PscHeating_t::setup;
    destroy             = PscHeating_t::destroy;
  }
} psc_heating_ops_cuda;
