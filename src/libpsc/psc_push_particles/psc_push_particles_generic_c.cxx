
#include "psc_push_particles_private.h"
#include "psc_generic_c.h"

#include "push_particles.hxx"

struct PushParticlesGenericC : PushParticlesBase
{
  void push_mprts_xyz(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return psc_push_particles_generic_c_push_mprts_xyz(nullptr, mprts, mflds); }

  void push_mprts_xy(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return psc_push_particles_generic_c_push_mprts_xy(nullptr, mprts, mflds); }

  void push_mprts_xz(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return psc_push_particles_generic_c_push_mprts_xz(nullptr, mprts, mflds); }

  void push_mprts_yz(struct psc_mparticles *mprts, struct psc_mfields *mflds) override
  { return psc_push_particles_generic_c_push_mprts_yz(nullptr, mprts, mflds); }
};

using PushParticlesWrapper_t = PushParticlesWrapper<PushParticlesGenericC>;

// ======================================================================
// psc_push_particles: subclass "generic_c"

struct psc_push_particles_ops_c : psc_push_particles_ops {
  psc_push_particles_ops_c() {
    name                  = "generic_c";
    size                  = PushParticlesWrapper_t::size;
    setup                 = PushParticlesWrapper_t::setup;
    destroy               = PushParticlesWrapper_t::destroy;
    push_mprts_y          = PushParticlesWrapper_t::push_mprts_y;
    push_mprts_z          = PushParticlesWrapper_t::push_mprts_z;
    push_mprts_xy         = PushParticlesWrapper_t::push_mprts_xy;
    push_mprts_xz         = PushParticlesWrapper_t::push_mprts_xz;
    push_mprts_yz         = PushParticlesWrapper_t::push_mprts_yz;
    push_mprts_xyz        = PushParticlesWrapper_t::push_mprts_xyz;
    particles_type        = "double";
  }
} psc_push_particles_generic_c_ops;
