
#pragma once

#include "push_fields.hxx"
#include "vpic_iface.h"
#include "psc_method.h"
#include "psc_fields_vpic.h"

// ======================================================================
// PushFieldsVpic

struct PushFieldsVpic : PushFieldsBase
{
  PushFieldsVpic()
  {
    psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sim_);
  }

  void push_E(MfieldsBase& mflds_base, double dt_fac) override
  {
    // needs J, E, B, TCA, material
    auto& mflds = mflds_base.get_as<MfieldsVpic>(JXI, VPIC_MFIELDS_N_COMP);
    FieldArray *vmflds = mflds.vmflds_fields;
    Simulation_push_mflds_E(sim_, vmflds, dt_fac);
    Simulation_field_injection(sim_); // FIXME, this isn't the place, should have its own psc_field_injection
    
    // updates E, TCA, and B ghost points FIXME 9 == TCAX
    mflds_base.put_as(mflds, EX, 9 + 3);
  }

  void push_H(MfieldsBase& mflds_base, double dt_fac) override
  {
    // needs E, B
    auto& mflds = mflds_base.get_as<MfieldsVpic>(EX, HX + 6);
    Simulation_push_mflds_H(sim_, mflds.vmflds_fields, dt_fac);
    // updates B
    mflds_base.put_as(mflds, HX, HX + 3);
  }

private:
  Simulation *sim_;
};

