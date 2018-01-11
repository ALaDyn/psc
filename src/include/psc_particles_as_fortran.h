
#ifndef PSC_PARTICLES_AS_FORTRAN_H
#define PSC_PARTICLES_AS_FORTRAN_H

#include "psc_particles_fortran.h"

typedef particle_fortran_real_t particle_real_t;
typedef particle_fortran_t particle_t;
using mparticles_t = mparticles_fortran_t;

#define particle_iter_t               psc_particle_fortran_iter_t
#define particle_range_t              psc_particle_fortran_range_t

#define PARTICLE_TYPE                 "fortran"

#define PSC_PARTICLES_AS_FORTRAN 1

#include "particle_iter.h"

#endif

