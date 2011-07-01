
#ifndef PSC_H
#define PSC_H

#include <config.h>

#include <mrc_domain.h>

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "psc_photons.h"

// ----------------------------------------------------------------------

#define FIELDS_FORTRAN 1
#define FIELDS_C       2
#define FIELDS_SSE2    3

#define PARTICLES_FORTRAN 1
#define PARTICLES_C       2
#define PARTICLES_SSE2    3
#define PARTICLES_CBE     4

// FIELDS_BASE and PARTICLES_BASE macros are defined by configure
// #define FIELDS_BASE FIELDS_FORTRAN
// #define PARTICLES_BASE PARTICLES_FORTRAN

enum {
  NE , NI , NN ,
  JXI, JYI, JZI,
  EX , EY , EZ ,
  HX , HY , HZ ,
  DX , DY , DZ ,
  BX , BY , BZ ,
  EPS, MU ,
  NR_FIELDS,
};

extern const char *fldname[NR_FIELDS];

// C floating point type
// used to switch between single and double precision

typedef float real;

#define real(x) x ## f

// Fortran types

typedef double f_real;
typedef int f_int;

// always need the fortran types for fortran interface
#include "psc_particles_fortran.h"
#include "psc_fields_fortran.h"

#include "psc_particles.h"
#include "psc_fields.h"

// user settable parameters
struct psc_param {
  double qq;
  double mm;
  double tt;
  double cc;
  double eps0;
  int nmax;
  double cpum;
  double lw;
  double i0;
  double n0;
  double e0;
  double b0;
  double j0;
  double rho0;
  double phi0;
  double a0;
  int nicell;
  bool const_num_particles_per_cell;
  bool fortran_particle_weight_hack;
  bool adjust_dt_to_cycles;
  double wallclock_limit;
  bool from_checkpoint;
};

// coefficients needed for computations
// -- derived, not provided by user
struct psc_coeff {
  double cori;
  double alpha;
  double beta;
  double eta;

  // FIXME are these needed in general?
  double wl;
  double ld;
  double vos;
  double vt;
  double wp;
  int np; // # steps for time-averaging fields
  int nnp; // # steps per laser cycle
};

struct psc_pml {
  int thick; // # grid points for PML
  int cushion; // # grid points for buffer zone
  int size; // # grid points PML + buffer
  int order; // PML order
};

// need to match fortran values

enum {
  BND_FLD_OPEN,
  BND_FLD_PERIODIC,
  BND_FLD_UPML,
  BND_FLD_TIME,
};

enum {
  BND_PART_REFLECTING,
  BND_PART_PERIODIC,
};

struct psc_domain {
  double length[3];
  double corner[3];
  int gdims[3];
  int bnd_fld_lo[3], bnd_fld_hi[3], bnd_part[3];
  bool use_pml;
};

mfields_base_t *mfields_base_alloc(struct mrc_domain *domain, int nr_fields, int ibn[3]);
void mfields_base_destroy(mfields_base_t *flds);

// FIXME, turn into mrc_obj
void psc_push_photons_run(mphotons_t *mphotons);
// FIXME, turn into mrc_obj
void psc_photon_generator_run(mphotons_t *mphotons);

// ----------------------------------------------------------------------
// general info / parameters for the code

// FIXME, the randomize / sort interaction needs more work
// In particular, it's better to randomize just per-cell after the sorting

struct psc_patch {
  int ldims[3];       // size of local domain (w/o ghost points)
  int off[3];         // local to global offset
  double xb[3];       // lower left corner of the domain in this patch
};

#define CRDX(p, jx) (psc->dx[0] * ((jx) + psc->patch[p].off[0]) + psc->domain.corner[0])
#define CRDY(p, jy) (psc->dx[1] * ((jy) + psc->patch[p].off[1]) + psc->domain.corner[1])
#define CRDZ(p, jz) (psc->dx[2] * ((jz) + psc->patch[p].off[2]) + psc->domain.corner[2])

MRC_CLASS_DECLARE(psc, struct psc);

struct psc {
  struct mrc_obj obj;
  struct psc_push_particles *push_particles;
  struct psc_push_fields *push_fields;
  struct psc_bnd *bnd;
  struct psc_collision *collision;
  struct psc_randomize *randomize;
  struct psc_sort *sort;
  struct psc_output_fields *output_fields;
  struct psc_output_particles *output_particles;
  struct psc_moments *moments;
  struct psc_event_generator *event_generator;
  struct psc_balance *balance;

  // user-configurable parameters
  struct psc_param prm;
  struct psc_coeff coeff;
  struct psc_domain domain;
  struct psc_pml pml;

  // other parameters / constants
  double p2A, p2B;
  int timestep;
  double dt;
  double dx[3];

  mparticles_base_t *particles;
  mfields_base_t *flds;
  mphotons_t mphotons;
  struct mrc_domain *mrc_domain;

  int nr_patches;
  struct psc_patch *patch;
  int ibn[3];         // number of ghost points

  // did we allocate the fields / particles (otherwise, Fortran did)
  bool allocated;

  double time_start;
};

#define foreach_3d(p, ix, iy, iz, l, r) {				\
  int __ilo[3] = { -l, -l, -l };					\
  int __ihi[3] = { psc.patch[p].ldims[0] + r,				\
		   psc.patch[p].ldims[1] + r,				\
		   psc.patch[p].ldims[2] + r };				\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

#define foreach_3d_end				\
  } } }

#define psc_foreach_3d(psc, p, ix, iy, iz, l, r) {			\
  int __ilo[3] = { -l, -l, -l };					\
  int __ihi[3] = { psc->patch[p].ldims[0] + r,				\
		   psc->patch[p].ldims[1] + r,				\
		   psc->patch[p].ldims[2] + r };				\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

#define psc_foreach_3d_end				\
  } } }

#define foreach_3d_g(p, ix, iy, iz) {					\
  int __ilo[3] = { -psc.ibn[0], -psc.ibn[1], -psc.ibn[2] };		\
  int __ihi[3] = { psc.patch[p].ldims[0] + psc.ibn[0],			\
		   psc.patch[p].ldims[1] + psc.ibn[1],			\
		   psc.patch[p].ldims[2] + psc.ibn[2] };		\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

#define foreach_3d_g_end				\
  } } }

#define psc_foreach_3d_g(psc, p, ix, iy, iz) {				\
  int __ilo[3] = { -psc->ibn[0], -psc->ibn[1], -psc->ibn[2] };		\
  int __ihi[3] = { psc->patch[p].ldims[0] + psc->ibn[0],			\
		   psc->patch[p].ldims[1] + psc->ibn[1],			\
		   psc->patch[p].ldims[2] + psc->ibn[2] };		\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

#define psc_foreach_3d_g_end				\
  } } }

#define psc_foreach_patch(psc, p)		\
  for (int p = 0; p < (psc)->nr_patches; p++)


// ----------------------------------------------------------------------
// we keep this info global for now.

extern struct psc *ppsc;

static inline void
psc_local_to_global_indices(struct psc *psc, int p, int jx, int jy, int jz,
			    int *ix, int *iy, int *iz)
{
  *ix = jx + psc->patch[p].off[0];
  *iy = jy + psc->patch[p].off[1];
  *iz = jz + psc->patch[p].off[2];
}

struct psc *psc_create(MPI_Comm comm);
void psc_set_from_options(struct psc *psc);
void psc_setup(struct psc *psc);
void psc_view(struct psc *psc);
void psc_destroy(struct psc *psc);
void psc_integrate(struct psc *psc);

struct mrc_domain *psc_setup_mrc_domain(struct psc *psc, int nr_patches);

void psc_dump_particles(mparticles_base_t *particles, const char *fname);
void psc_dump_field(mfields_base_t *flds, int m, const char *fname);
void psc_check_particles(mparticles_base_t *particles);

struct psc *psc_read_checkpoint(MPI_Comm comm);
void psc_write_checkpoint(struct psc *psc);

void psc_setup_fortran(struct psc *psc);

// FIXME, should be per mrc_domain or sth, really
extern list_t mfields_list;

// FIXME, only used for one thing, could be consolidated?

static inline int
particle_base_real_nint(particle_base_real_t x)
{
  return (int)(x + 10.5f) - 10;
}

// ----------------------------------------------------------------------
// other bits and hacks...

#define sqr(a) ((a) * (a))

#define HERE do { int __rank; MPI_Comm_rank(MPI_COMM_WORLD, &__rank); printf("[%d] HERE: in %s() at %s:%d\n", __rank, __FUNCTION__, __FILE__, __LINE__); } while(0)

// ----------------------------------------------------------------------
// compiler bits

#ifndef __unused
#ifdef __GNUC__
#define	__unused	__attribute__((__unused__))
#else
#define	__unused	/* no attribute */
#endif
#endif

#endif
