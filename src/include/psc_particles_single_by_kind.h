
#ifndef PSC_PARTICLES_SINGLE_BY_KIND_H
#define PSC_PARTICLES_SINGLE_BY_KIND_H

#include "psc_particles_private.h"

#include "particles.hxx"

using particle_single_by_kind_real_t = float;

struct particle_single_by_kind_t
{
  using real_t = particle_single_by_kind_real_t;
};

struct psc_mparticles_single_by_kind : psc_mparticles_base
{
  using Base = psc_mparticles_base;
  using particle_t = particle_single_by_kind_t;

  using Base::Base;

  bk_mparticles *bkmprts;

  void get_size_all(uint *n_prts_by_patch)
  {
    bk_mparticles_size_all(bkmprts, n_prts_by_patch);
  }

  void reserve_all(uint *n_prts_by_patch)
  {
    bk_mparticles_reserve_all(bkmprts, n_prts_by_patch);
  }

  void resize_all(uint *n_prts_by_patch)
  {
    bk_mparticles_resize_all(bkmprts, n_prts_by_patch);
  }
};

#define psc_mparticles_single_by_kind(mprts)({				\
      mrc_to_subobj(mprts, struct psc_mparticles_single_by_kind);	\
})

// ======================================================================
// mparticles_single_by_kind_t

using mparticles_single_by_kind_t = mparticles_base<psc_mparticles_single_by_kind>;

#endif
