
#include "psc_push_particles_private.h"
#include "psc_push_particles_1st.h"

#include "push_particles.hxx"

#include "push.hxx"

// ======================================================================
// psc_push_particles: subclass "1st"

struct PushParticles1st : PushParticlesBase
{
  void push_mprts_xz(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return PscPushParticles_<PushParticles_<Config1stXZ>>::push_mprts(nullptr, mprts, mflds); }

  void push_mprts_yz(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return PscPushParticles_<PushParticles_<Config1stYZ>>::push_mprts(nullptr, mprts, mflds); }
};

using PushParticlesWrapper_t = PushParticlesWrapper<PushParticles1st>;

struct psc_push_particles_ops_1st : psc_push_particles_ops {
  psc_push_particles_ops_1st() {
    name                  = "1st";
    size                  = PushParticlesWrapper_t::size;
    setup                 = PushParticlesWrapper_t::setup;
    destroy               = PushParticlesWrapper_t::destroy;
    particles_type        = "double";
  }
} psc_push_particles_1st_ops;

