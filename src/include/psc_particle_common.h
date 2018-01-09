
#include <cmath>

#define PTYPE_SINGLE          1
#define PTYPE_DOUBLE          2
#define PTYPE_SINGLE_BY_BLOCK 3
#define PTYPE_FORTRAN         5
#define PTYPE_CUDA            6


#if PTYPE == PTYPE_SINGLE

#define particle_PTYPE_real_t particle_single_real_t
#define particle_PTYPE_t particle_single_t
#define psc_particle_PTYPE psc_particle_single

#elif PTYPE == PTYPE_DOUBLE

#define particle_PTYPE_real_t particle_double_real_t
#define particle_PTYPE_t particle_double_t
#define psc_particle_PTYPE psc_particle_double

#elif PTYPE == PTYPE_SINGLE_BY_BLOCK

#define particle_PTYPE_real_t particle_single_by_block_real_t
#define particle_PTYPE_t particle_single_by_block_t
#define psc_particle_PTYPE psc_particle_single_by_block

#elif PTYPE == PTYPE_FORTRAN

#define particle_PTYPE_real_t particle_fortran_real_t
#define particle_PTYPE_t particle_fortran_t
#define psc_particle_PTYPE psc_particle_fortran

#elif PTYPE == PTYPE_CUDA

#define particle_PTYPE_real_t particle_cuda_real_t
#define particle_PTYPE_t particle_cuda_t
#define psc_particle_PTYPE psc_particle_cuda

#endif

// ----------------------------------------------------------------------
// particle_PYTPE_real_t

#if PTYPE == PTYPE_SINGLE || PTYPE == PTYPE_SINGLE_BY_BLOCK || PTYPE == PTYPE_CUDA

typedef float particle_PTYPE_real_t;

#elif PTYPE == PTYPE_DOUBLE || PTYPE == PTYPE_FORTRAN

typedef double particle_PTYPE_real_t;

#endif

// ----------------------------------------------------------------------
// MPI_PARTICLES_PTYPE_REAL
// annoying, but need to use a macro, which means we can't consolidate float/double

#if PTYPE == PTYPE_SINGLE

#define MPI_PARTICLES_SINGLE_REAL MPI_FLOAT
#define psc_mparticles_single(mprts) mrc_to_subobj(mprts, struct psc_mparticles_single)

#elif PTYPE == PTYPE_DOUBLE

#define MPI_PARTICLES_DOUBLE_REAL MPI_DOUBLE
#define psc_mparticles_double(prts) mrc_to_subobj(prts, struct psc_mparticles_double)

#elif PTYPE == PTYPE_SINGLE_BY_BLOCK

#define MPI_PARTICLES_SINGLE_REAL MPI_FLOAT
#define psc_mparticles_single_by_block(prts) mrc_to_subobj(prts, struct psc_mparticles_single_by_block)

#elif PTYPE == PTYPE_FORTRAN

#define MPI_PARTICLES_FORTRAN_REAL MPI_DOUBLE
#define psc_mparticles_fortran(prts) mrc_to_subobj(prts, struct psc_mparticles_fortran)

#elif PTYPE == PTYPE_CUDA

#define MPI_PARTICLES_CUDA_REAL MPI_FLOAT
#define psc_mparticles_cuda(prts) mrc_to_subobj(prts, struct psc_mparticles_cuda)

#endif

// ----------------------------------------------------------------------
// particle_PTYPE_t

#if PTYPE == PTYPE_SINGLE || PTYPE == PTYPE_SINGLE_BY_BLOCK || PTYPE == PTYPE_DOUBLE || PTYPE == PTYPE_CUDA

struct particle_PTYPE_t
{
  using real_t = particle_PTYPE_real_t;

  real_t xi, yi, zi;
  real_t qni_wni;
  real_t pxi, pyi, pzi;
  int kind;
};

#elif PTYPE == PTYPE_FORTRAN

struct particle_PTYPE_t
{
  using real_t = particle_PTYPE_real_t;

  real_t xi, yi, zi;
  real_t pxi, pyi, pzi;
  real_t qni;
  real_t mni;
  real_t cni;
  real_t lni;
  real_t wni;
};

#endif

#undef particle_PTYPE_real_t
#undef particle_PTYPE_t
#undef psc_particle_PTYPE

