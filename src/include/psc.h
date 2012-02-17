/*! @file */

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

#include "psc_patchmanager.h"

///User specified parameters
///
///These parameters are set in psc_case->init_param() to define the normalization coefficients of the system
struct psc_param {
  double qq;	///<elemental charge 
  double mm;	///<mass
  double tt;	///<some measurement for energy ? (default is 1keV in fortran) 
  double cc;	///<speed of light
  double eps0;	///<vacuum permittivity
  int nmax;	///<number of timesteps
  double cpum;	///<time until CPU checks out data and restarts (deprecated?) 
  double lw;	///<normalization coefficient for laser wavelength (omega)
  double i0;	///<laser intensity
  double n0;	///<electron density
  double e0;	///<field intesity
  double b0;
  double j0;
  double rho0;
  double phi0;
  double a0;
  double cfl;   ///<CFL number to be used for determining timestep
  int nicell;	///<number of particles per gridpoint to represent a normalised density of 1 
  int nr_kinds;
  bool seed_by_time;
  bool const_num_particles_per_cell;
  bool fortran_particle_weight_hack;
  bool adjust_dt_to_cycles;
  bool gdims_in_terms_of_cells; 
  double wallclock_limit;
  bool from_checkpoint;
  bool write_checkpoint;
  double initial_particle_shift;
  char *fields_base; ///< base type for psc_mfields ("c", "fortran", "cuda")
  char *particles_base; ///< base type for psc_mparticles ("c", "fortran", "cuda")
  unsigned int particles_base_flags; ///< additional flags for the particles base type
};

/// coefficients needed for computations
/// -- derived, not provided by user
struct psc_coeff {
  double cori;	///< 1 / psc_params.nicell 
  double alpha;
  double beta;
  double eta;

  // FIXME are these needed in general?
  double wl;	///<omega
  double ld;	///Normalization factor for lengths
  double vos;	///< 1/k
  double vt;
  double wp;
  int np; ///< # steps for time-averaging fields
  int nnp; ///< # steps per laser cycle
};

///Paramters for PML
struct psc_pml {
  int thick; ///< # grid points for PML
  int cushion; ///< # grid points for buffer zone
  int size; ///< # grid points PML + buffer
  int order; ///< PML order
};

// need to match fortran values

///Possible boundary conditions for fields
enum {
  BND_FLD_OPEN,
  BND_FLD_PERIODIC,
  BND_FLD_UPML,
  BND_FLD_TIME,
  BND_FLD_CONDUCTING_WALL,
};

///Possible boundary conditions for particles
enum {
  BND_PART_REFLECTING,
  BND_PART_PERIODIC,
};

///Describes the spatial domain to operate on.
///
///This struct describes the spatial dimension of the simulation-box
///@note Here, you can also set the dimensionality by eliminating a dimension. Example: To simulate in xy only, set
///\verbatim psc_domain.gdims[2]=1 \endverbatim
///Also, set the boundary conditions for the eliminated dimensions to BND_FLD_PERIODIC or you'll get invalid \a dt and \a dx

struct psc_domain {
  double length[3];	///<The physical size of the simulation-box 
  double corner[3];
  int gdims[3];		///<Number of grid-points in each dimension
  int np[3];		///<Number of patches in each dimension
  int bnd_fld_lo[3];	///<Boundary conditions of the fields. Can be any value of BND_FLD.
  int bnd_fld_hi[3];	///<Boundary conditions of the fields. Can be any value of BND_FLD.
  int bnd_part[3];	///<Boundary conditions of the fields. Can be any value of BND_PART.
  bool use_pml;		///<Enables or disables PML
};

// FIXME, turn into mrc_obj
void psc_push_photons_run(mphotons_t *mphotons);
// FIXME, turn into mrc_obj
void psc_photon_generator_run(mphotons_t *mphotons);

// ----------------------------------------------------------------------
// general info / parameters for the code

// FIXME, the randomize / sort interaction needs more work
// In particular, it's better to randomize just per-cell after the sorting

struct psc_patch {
  int ldims[3];       ///< size of local domain (w/o ghost points)
  int off[3];         ///< local to global offset
  
  //! @brief lower left corner of this patch in the domain
  //! 
  //! This value is given in the internal coordinates and is consistent with those given to pulses, particles etc.
  //! \code double xx = psc.patch[i]->xb[0] + x * psc.dx[0]; \endcode
  //! Use the macros CRDX, CRDY, CRDZ to get the internal positions of any grid-point on this patch
  double xb[3];
};

#define CRDX(p, jx) (psc->dx[0] * (jx) + psc->patch[p].xb[0])
#define CRDY(p, jy) (psc->dx[1] * (jy) + psc->patch[p].xb[1])
#define CRDZ(p, jz) (psc->dx[2] * (jz) + psc->patch[p].xb[2])

///This structure holds all the interfaces for the given configuration.
///
///
struct psc {
  ///@defgroup interfaces Interfaces @{
  struct mrc_obj obj;
  struct psc_push_particles *push_particles;	///< particle pusher
  struct psc_push_fields *push_fields;		///< field pusher
  struct psc_bnd *bnd;				///< boundaries
  struct psc_collision *collision;		///< collision operator
  struct psc_randomize *randomize;		///< randomizer
  struct psc_sort *sort;			///< sort operator
  struct psc_output_fields *output_fields;	///< field output
  struct psc_output_particles *output_particles;///< particle output
  struct psc_output_photons *output_photons;    ///< particle output
  struct psc_moments *moments;			///< Moment generator
  struct psc_event_generator *event_generator;	///< event generator
  struct psc_balance *balance;                  ///< rebalancer
  ///@}

  ///@defgroup config-params user-configurable parameters @{
  // user-configurable parameters
  struct psc_param prm;		///< normalization parameters set by the user
  struct psc_coeff coeff;	///< automatically derived constants
  struct psc_domain domain;	///< the computational domain
  struct psc_pml pml;		///< PML settings
  ///@}

  
  // other parameters / constants
  double p2A, p2B;
  int timestep;	///< the current timestep
  double dt;	///< timestep in physical units
  double dx[3];	///< cell size in physical units
  ///@}

  mparticles_base_t *particles;	///< All the particles, indexed by their containing patch
  mfields_base_t *flds;	///< The fields.
  mphotons_t *mphotons;

  ///The domain partitioner.
  ///
  ///Use this to access the global list of patches \sa \ref patches

  struct mrc_domain *mrc_domain;
  
  bool use_dynamic_patches;	///< Setting this to true will enable dynamic allocation of patches. Make sure to provide at least one domainwindow in this case.

  struct psc_patchmanager patchmanager;	///< Use this to allocate and deallocate patches dynamically on the domain

  int nr_patches;	///< Number of patches (on this processor)
  struct psc_patch *patch;	///< List of patches (on this processor)
  int ibn[3];         ///< number of ghost points
  // did we allocate the fields / particles (otherwise, Fortran did)
  bool allocated;

  double time_start;
};

MRC_CLASS_DECLARE(psc, struct psc);

struct psc_particle_npt {
  double q; ///< charge
  double m; ///< mass
  double n; ///< density
  double p[3]; ///< momentum
  double T[3]; ///< temperature
  int particles_per_cell; ///< desired number of particles per cell per unit density. If not specified, the global nicell is used.
};

struct psc_photon_np {
  double n; ///< density
  double k[3]; ///< wave number
  double sigma_k[3]; ///< width of Gaussian in momentum space
  int n_in_cell; ///< nr of quasi-particles in this cell
};

struct psc_ops {
  MRC_SUBCLASS_OPS(struct psc);
  void (*setup_particles)(struct psc *psc, int *nr_particles_by_patch, bool count_only);
  void (*init_npt)(struct psc *psc, int kind, double x[3],
		   struct psc_particle_npt *npt);
  void (*setup_fields)(struct psc *psc, mfields_base_t *flds);
  double (*init_field)(struct psc *psc, double x[3], int m);
  void (*init_photon_np)(struct psc *psc, double x[3], struct psc_photon_np *np);
  void (*integrate)(struct psc *psc);
  void (*step)(struct psc *psc);
  void (*output)(struct psc *psc);
};

/*!
Enumerates all grid points of patch p

Use this macro like a regular for() expression to process all grid points on any patch p.
Variables ix, iy, iz will be automatically declared.

\param p the patch to process
\param l Number of ghost-points to include in negative direction
\param r Number of ghost-points to include in positive direction
\param[out] ix x-coordinate of current grid point
\param[out] iy y-coordinate of current grid point
\param[out] iz z-coordinate of current grid point

Always close this expression with foreach_3d_end
*/
#define foreach_3d(p, ix, iy, iz, l, r) {				\
  int __ilo[3] = { -l, -l, -l };					\
  int __ihi[3] = { psc.patch[p].ldims[0] + r,				\
		   psc.patch[p].ldims[1] + r,				\
		   psc.patch[p].ldims[2] + r };				\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

/*! Closes a loop started by foreach_3d
 * 
Usage:
\code
psc_foreach_patch(&psc, p){
psc_foreach_3d(p, jx, jy, jz, 0, 0) {
  //do stuff
} psc_foreach_3d_end;
}
\endcode
*/
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
void psc_setup_partition(struct psc *psc, int *nr_particles_by_patch,
			int *particle_label_offset);
void psc_setup_particles(struct psc *psc, int *nr_particles_by_patch,
			int particle_label_offset);
void psc_setup_partition_and_particles(struct psc *psc);
void psc_setup_fields(struct psc *psc);
void psc_output(struct psc *psc);
void psc_integrate(struct psc *psc);

void psc_setup_default(struct psc *psc);
void psc_setup_coeff(struct psc *psc);
void psc_setup_domain(struct psc *psc);
struct mrc_domain *psc_setup_mrc_domain(struct psc *psc, int nr_patches);
void psc_setup_patches(struct psc *psc, struct mrc_domain *domain);

void psc_dump_particles(mparticles_base_t *particles, const char *fname);
void psc_dump_field(mfields_base_t *flds, int m, const char *fname);

extern bool opt_checks_verbose;

void psc_check_continuity(struct psc *psc, mparticles_base_t *particles,
			  mfields_base_t *flds, double eps);

struct psc *psc_read_checkpoint(MPI_Comm comm, int n);
void psc_write_checkpoint(struct psc *psc);

void psc_setup_fortran(struct psc *psc);


int psc_main(int *argc, char ***argv, struct psc_ops *type);


// ----------------------------------------------------------------------
// cell_map

struct cell_map {
  int dims[3];
  int b_bits[3];
  int b_bits_max;
  int N; // indices will be 0..N-1
};

int cell_map_init(struct cell_map *map, const int dims[3],
		  const int blocksize[3]);
int cell_map_3to1(struct cell_map *map, int i[3]);
void cell_map_1to3(struct cell_map *map, int idx, int i[3]);
void cell_map_free(struct cell_map *map);

// ----------------------------------------------------------------------
// psc_stats: simple statistics

#define MAX_PSC_STATS 20

extern double psc_stats_val[MAX_PSC_STATS+1]; // [0] is left empty
extern int nr_psc_stats;

int psc_stats_register(const char *name);
void psc_stats_log(struct psc *psc);

#define psc_stats_start(n) do {				\
    psc_stats_val[n] -= MPI_Wtime();			\
  } while (0)

#define psc_stats_stop(n) do {				\
    psc_stats_val[n] += MPI_Wtime();			\
  } while (0)

// These are general statistics categories to be used in different parts
// of the code as appropriate.

extern int st_time_output;   //< time spent in output
extern int st_time_comm;     //< time spent in communications
extern int st_time_particle; //< time spent in particle computation
extern int st_time_field;    //< time spent in field computation

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
