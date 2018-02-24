
#include "psc_push_particles_private.h"

#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"
#include "psc_push_particles_1vb.h"
#include "push_particles.hxx"

// ======================================================================
// psc_push_particles: subclass "1vbec_double"

template<typename dim_t>
using push_p_ops_1vbec_double = push_p_ops<push_p_config<mfields_c_t, dim_t>>;

// ======================================================================
// PushParticles1vbecDouble

class PushParticles1vbecDouble : public PushParticlesBase
{
public:
  void push_mprts_xyz(struct psc_mparticles *mprts, struct psc_mfields *mflds_base) override
  { push_p_ops_1vbec_double<dim_xyz>::push_mprts(nullptr, mprts, mflds_base); }

  void push_mprts_yz(struct psc_mparticles *mprts, struct psc_mfields *mflds_base) override
  { push_p_ops_1vbec_double<dim_yz>::push_mprts(nullptr, mprts, mflds_base); }

  void push_mprts_1(struct psc_mparticles *mprts, struct psc_mfields *mflds_base) override
  { push_p_ops_1vbec_double<dim_1>::push_mprts(nullptr, mprts, mflds_base); }

  static void setup(struct psc_push_particles *push)
  {
    PscPushParticles<PushParticles1vbecDouble> pushp(push);
    new(pushp.sub()) PushParticles1vbecDouble;
  }

  static void destroy(struct psc_push_particles *push)
  {
    PscPushParticles<PushParticles1vbecDouble> pushp(push);
    pushp.sub()->~PushParticles1vbecDouble();
  }

  static void push_mprts_xyz(struct psc_push_particles *push,
			     struct psc_mparticles *mprts, struct psc_mfields *mflds_base)
  {
    PscPushParticles<PushParticles1vbecDouble> pushp(push);
    pushp->push_mprts_xyz(mprts, mflds_base);
  }
  
  static void push_mprts_yz(struct psc_push_particles *push,
			    struct psc_mparticles *mprts, struct psc_mfields *mflds_base)
  {
    PscPushParticles<PushParticles1vbecDouble> pushp(push);
    pushp->push_mprts_yz(mprts, mflds_base);
  }
  
  static void push_mprts_1(struct psc_push_particles *push,
			   struct psc_mparticles *mprts, struct psc_mfields *mflds_base)
  {
    PscPushParticles<PushParticles1vbecDouble> pushp(push);
    pushp->push_mprts_1(mprts, mflds_base);
  }

};

using PushParticles_t = PushParticles1vbecDouble;
  
struct psc_push_particles_ops_1vbec_double : psc_push_particles_ops {
  psc_push_particles_ops_1vbec_double() {
    name                  = "1vbec_double";
    size                  = sizeof(PushParticles1vbecDouble);
    setup                 = PushParticles_t::setup;
    destroy               = PushParticles_t::destroy;
    push_mprts_xyz        = PushParticles_t::push_mprts_xyz;
    push_mprts_yz         = PushParticles_t::push_mprts_yz;
    push_mprts_1          = PushParticles_t::push_mprts_1;
    //stagger_mprts_1      = push_p_ops_1vbec_double<dim_1>::stagger_mprts;
    particles_type        = PARTICLE_TYPE;
  }
} psc_push_particles_1vbec_double_ops;

