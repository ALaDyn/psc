
#pragma once

#include "push_particles.hxx"
#include "vpic_iface.h"
#include "psc_method.h"

#include "psc_fields_vpic.h"
#include "psc_particles_vpic.h"

// ======================================================================
// PushParticlesVpic

struct PushParticlesVpic : PushParticlesBase
{
  using Mparticles = MparticlesVpic;
  using Mfields = MfieldsVpic;
  
  PushParticlesVpic()
  {
    psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sim_);
  }

  void prep(MparticlesBase& mprts_base, MfieldsBase& mflds_base) override
  {
    // needs E, B
    auto mprts = mprts_base.get_as<Mparticles>();
    auto& mflds = mflds_base.get_as<Mfields>(EX, HX + 6);
    sim_->push_mprts_prep(mprts.vmprts_, *mflds.vmflds_fields);
    mflds_base.put_as(mflds, 0, 0);
    mprts_base.put_as(mprts, MP_DONT_COPY);
  }
  
  void push_mprts(MparticlesBase& mprts_base, MfieldsBase& mflds_base) override
  {
    // needs E, B (not really, because they're already in interpolator), rhob?
    auto& mflds = mflds_base.get_as<Mfields>(EX, HX + 6);
    auto& mprts = mprts_base.get_as<Mparticles>();
    
    sim_->push_mprts(mprts.vmprts_, *mflds.vmflds_fields);
    
    // update jf FIXME: rhob too, probably, depending on b.c.
    mflds_base.put_as(mflds, JXI, JXI + 3);
    mprts_base.put_as(mprts);
  }

private:
  Simulation* sim_;
};

