
#ifndef PSC_PARTICLE_DOUBLE_H
#define PSC_PARTICLE_DOUBLE_H

#include "psc_particles_private.h"
#include "psc.h"

#include "particles.hxx"
#include "particles_traits.hxx"

using particle_double_real_t = double;

struct particle_double_t : psc_particle<particle_double_real_t> {};

using MparticlesDouble = Mparticles<particle_double_t>;
using PscMparticlesDouble = PscMparticles<MparticlesDouble>;

template<>
struct mparticles_traits<PscMparticlesDouble>
{
  static constexpr const char* name = "double";
  static MPI_Datatype mpi_dtype() { return MPI_DOUBLE; }
};

static inline particle_double_real_t
particle_double_qni_wni(particle_double_t *p)
{
  return p->qni_wni_;
}

#endif
