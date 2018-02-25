
#ifndef PSC_PARTICLE_FORTRAN_H
#define PSC_PARTICLE_FORTRAN_H

#include "psc_particles_private.h"

#include "particles.hxx"
#include "particles_traits.hxx"

using particle_fortran_real_t = double;

struct particle_fortran_t
{
  using real_t = particle_fortran_real_t;

  real_t xi, yi, zi;
  real_t pxi, pyi, pzi;
  real_t qni;
  real_t mni;
  real_t cni;
  real_t lni;
  real_t wni;
};

using MparticlesFortran = Mparticles<particle_fortran_t>;
using PscMparticlesFortran = PscMparticles<MparticlesFortran>;

template<>
struct mparticles_traits<PscMparticlesFortran>
{
  static constexpr const char* name = "fortran";
  static MPI_Datatype mpi_dtype() { return MPI_DOUBLE; }
};

#endif
