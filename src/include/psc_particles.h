
#ifndef PSC_PARTICLES_H
#define PSC_PARTICLES_H

// ----------------------------------------------------------------------
// mparticles type

// This type is replicated for each actual particle type, however,
// the interface and implementation is always identical, hence 
// created automatically for the variants using macros

#define DECLARE_MPARTICLES_METHODS(type)				\
  									\
typedef struct psc_mparticles_##type {				        \
  struct mrc_obj obj;							\
  particles_##type##_t *p;						\
  int nr_patches;							\
  struct mrc_domain *domain;						\
} mparticles_##type##_t;						\
									\
MRC_CLASS_DECLARE(psc_mparticles_##type, struct psc_mparticles_##type);	\
									\
void psc_mparticles_##type##_set_domain_nr_particles(mparticles_##type##_t *mparticles, \
						 struct mrc_domain *domain, \
						 int *nr_particles_by_patch); \
 									\
void psc_mparticles_##type##_get_from(mparticles_##type##_t *particles, \
					  void *particles_base);	\
void psc_mparticles_##type##_put_to(mparticles_##type##_t *particles,	\
					void *particles_base);		\


#include "psc_particles_fortran.h"
DECLARE_MPARTICLES_METHODS(fortran)

#include "psc_particles_c.h"
DECLARE_MPARTICLES_METHODS(c)

#ifdef USE_SSE2
#include "psc_particles_sse2.h"
DECLARE_MPARTICLES_METHODS(sse2)
#endif

#include "psc_particles_cbe.h"
DECLARE_MPARTICLES_METHODS(cbe)

#ifdef USE_CUDA
#include "psc_particles_cuda.h"
DECLARE_MPARTICLES_METHODS(cuda)
#endif

// ----------------------------------------------------------------------
// base particles type

#if PARTICLES_BASE == PARTICLES_FORTRAN

typedef mparticles_fortran_t mparticles_base_t;

#define psc_mparticles_base_create  psc_mparticles_fortran_create
#define psc_mparticles_base_set_name  psc_mparticles_fortran_set_name
#define psc_mparticles_base_set_domain_nr_particles psc_mparticles_fortran_set_domain_nr_particles
#define psc_mparticles_base_setup   psc_mparticles_fortran_setup
#define psc_mparticles_base_write   psc_mparticles_fortran_write
#define psc_mparticles_base_read    psc_mparticles_fortran_read
#define psc_mparticles_base_destroy psc_mparticles_fortran_destroy

#elif PARTICLES_BASE == PARTICLES_C

typedef mparticles_c_t mparticles_base_t;

#define psc_mparticles_base_create  psc_mparticles_c_create
#define psc_mparticles_base_set_name  psc_mparticles_c_set_name
#define psc_mparticles_base_set_domain_nr_particles psc_mparticles_c_set_domain_nr_particles
#define psc_mparticles_base_setup   psc_mparticles_c_setup
#define psc_mparticles_base_write   psc_mparticles_c_write
#define psc_mparticles_base_read    psc_mparticles_c_read
#define psc_mparticles_base_destroy psc_mparticles_c_destroy

#elif PARTICLES_BASE == PARTICLES_SSE2

typedef mparticles_sse2_t mparticles_base_t;

#define psc_mparticles_base_create  psc_mparticles_sse2_create
#define psc_mparticles_base_set_domain_nr_particles psc_mparticles_sse2_set_domain_nr_particles
#define psc_mparticles_base_setup   psc_mparticles_sse2_setup
#define psc_mparticles_base_write   psc_mparticles_sse2_write
#define psc_mparticles_base_read    psc_mparticles_sse2_read
#define psc_mparticles_base_destroy psc_mparticles_sse2_destroy

#elif PARTICLES_BASE == PARTICLES_CUDA

typedef mparticles_cuda_t mparticles_base_t;

#define psc_mparticles_base_create  psc_mparticles_cuda_create
#define psc_mparticles_base_set_name  psc_mparticles_cuda_set_name
#define psc_mparticles_base_set_domain_nr_particles psc_mparticles_cuda_set_domain_nr_particles
#define psc_mparticles_base_setup   psc_mparticles_cuda_setup
#define psc_mparticles_base_write   psc_mparticles_cuda_write
#define psc_mparticles_base_read    psc_mparticles_cuda_read
#define psc_mparticles_base_destroy psc_mparticles_cuda_destroy

#elif PARTICLES_BASE == PARTICLES_CBE

typedef mparticles_cbe_t mparticles_base_t;

#define psc_mparticles_base_create  psc_mparticles_cbe_create
#define psc_mparticles_base_set_name  psc_mparticles_cbe_set_name
#define psc_mparticles_base_set_domain_nr_particles psc_mparticles_cbe_set_domain_nr_particles
#define psc_mparticles_base_setup   psc_mparticles_cbe_setup
#define psc_mparticles_base_write   psc_mparticles_cbe_write
#define psc_mparticles_base_read    psc_mparticles_cbe_read
#define psc_mparticles_base_destroy psc_mparticles_cbe_destroy

#else
#error unknown PARTICLES_BASE
#endif

#endif
