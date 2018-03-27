
#include "psc_push_particles_private.h"

#include "psc_fields_vpic.h"
#include "psc_particles_vpic.h"
#include "psc_method.h"
#include "push_particles.hxx"
#include "vpic_iface.h"

// ======================================================================
// PushParticlesVpic

struct PushParticlesVpic : PushParticlesBase
{
  PushParticlesVpic()
  {
    psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sim_);
  }

  void prep(PscMparticlesBase _mprts_base, PscMfieldsBase _mflds_base) override
  {
    auto mflds_base = PscMfieldsBase{_mflds_base};
    // needs E, B
    PscMfieldsVpic mf = mflds_base.get_as<PscMfieldsVpic>(EX, HX + 6);
    FieldArray *vmflds = mf->vmflds_fields;
    
    Simulation_push_mprts_prep(sim_, vmflds);
    
    mf.put_as(mflds_base, 0, 0);
  }
  
  void push_mprts(PscMparticlesBase _mprts_base, PscMfieldsBase _mflds_base) override
  {
    auto mflds_base = PscMfieldsBase{_mflds_base};
    // needs E, B (not really, because they're already in interpolator), rhob?
    auto mprts_base = PscMparticlesBase{_mprts_base};
    PscMfieldsVpic mf = mflds_base.get_as<PscMfieldsVpic>(EX, HX + 6);
    FieldArray *vmflds = mf->vmflds_fields;
    auto& mprts = mprts_base->get_as<MparticlesVpic>();
    
    Simulation_push_mprts(sim_, mprts.vmprts, vmflds);
    
    // update jf FIXME: rhob too, probably, depending on b.c.
    mf.put_as(mflds_base, JXI, JXI + 3);
    mprts_base->put_as(mprts);
  }

private:
  Simulation* sim_;
};

// ----------------------------------------------------------------------
// psc_push_particles_vpic_prep

static void
psc_push_particles_vpic_prep(struct psc_push_particles *push,
			     struct psc_mparticles *mprts_base,
			     struct psc_mfields *mflds_base)
{
  PscPushParticles<PushParticlesVpic> pushp(push);
  pushp->prep(PscMparticlesBase{mprts_base}, mflds_base);
}

// ----------------------------------------------------------------------
// psc_push_particles: subclass "vpic"

using PushParticlesWrapper_t = PushParticlesWrapper<PushParticlesVpic>;

struct psc_push_particles_ops_vpic : psc_push_particles_ops {
  psc_push_particles_ops_vpic() {
    name                  = "vpic";
    size                  = PushParticlesWrapper_t::size;
    setup                 = PushParticlesWrapper_t::setup;
    destroy               = PushParticlesWrapper_t::destroy;
    prep                  = psc_push_particles_vpic_prep;
  }
} psc_push_particles_vpic_ops;

