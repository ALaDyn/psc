
#ifndef PSC_PARTICLES_AS_DOUBLE_H
#define PSC_PARTICLES_AS_DOUBLE_H

#include "psc_particles_double.h"

using mparticles_t = mparticles_double_t;
using particle_t = mparticles_t::particle_t;

#define particle_qni_wni            particle_double_qni_wni

#define PARTICLE_TYPE               "double"

#define PSC_PARTICLES_AS_DOUBLE 1

#include "particle_iter.h"

#endif

