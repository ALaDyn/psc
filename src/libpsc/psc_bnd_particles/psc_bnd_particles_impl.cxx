
#include "psc_bnd_particles_private.h"
#include "psc_particles_single.h"
#include "psc_particles_double.h"

//#include "psc_bnd_particles_open.cxx"

#include "bnd_particles_impl.hxx"
#include "bnd_particles_ordered_impl.hxx"

// ======================================================================
// psc_bnd_particles: subclass "single"

struct psc_bnd_particles_ops_single : psc_bnd_particles_ops {
  using sub_t = psc_bnd_particles_sub<MparticlesSingle>;
  using PscBndParticles_t = PscBndParticlesWrapper<sub_t>;
  psc_bnd_particles_ops_single() {
    name                    = "single";
    size                    = sizeof(sub_t);
    destroy                 = PscBndParticles_t::destroy;
    setup                   = PscBndParticles_t::setup;
  }
} psc_bnd_particles_single_ops;

// ======================================================================
// psc_bnd_particles: subclass "double"

struct psc_bnd_particles_ops_double : psc_bnd_particles_ops {
  using sub_t = psc_bnd_particles_sub<MparticlesDouble>;
  using PscBndParticles_t = PscBndParticlesWrapper<sub_t>;
  psc_bnd_particles_ops_double() {
    name                    = "double";
    size                    = sizeof(sub_t);
    destroy                 = PscBndParticles_t::destroy;
    setup                   = PscBndParticles_t::setup;
  }
} psc_bnd_particles_double_ops;

// ======================================================================
// psc_bnd_particles: subclass "single2"

struct psc_bnd_particles_ops_single2 : psc_bnd_particles_ops {
  using sub_t = psc_bnd_particles_ordered<MparticlesSingle>;
  using PscBndParticles_t = PscBndParticlesWrapper<sub_t>;
  psc_bnd_particles_ops_single2() {
    name                    = "single2";
    size                    = sizeof(sub_t);
    destroy                 = PscBndParticles_t::destroy;
    setup                   = PscBndParticles_t::setup;
  }
} psc_bnd_particles_single2_ops;
