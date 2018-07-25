/*! @file */

#ifndef PSC_H
#define PSC_H

#include "psc_config.h"

#include "psc_bits.h"
#include "psc_particles.h"
#include "grid.hxx"
#include "particles.hxx"

#include "psc_stats.h"

#include <mrc_domain.h>

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <cmath>
#include <vector>

// ----------------------------------------------------------------------

enum {
  JXI, JYI, JZI,
  EX , EY , EZ ,
  HX , HY , HZ ,
  NR_FIELDS,
};

// Fortran types

typedef double f_real;
typedef int f_int;

///User specified parameters
///
struct psc_param {
  double qq;	///<elemental charge 
  double mm;	///<mass
  double tt;	///<some measurement for energy ? (default is 1keV in fortran) 
  double cc;	///<speed of light
  double eps0;	///<vacuum permittivity
  double lw;	///<normalization coefficient for laser wavelength (omega)
  double i0;	///<laser intensity
  double n0;	///<electron density
  double e0;	///<field intesity
  double b0;
  double j0;
  double rho0;
  double phi0;
  double a0;
  int nicell;	///<number of particles per gridpoint to represent a normalised density of 1 
  int neutralizing_population;  ///< the initial number of particles in a cell for this population will be st so that it achieves neutrality
  bool fractional_n_particles_per_cell;
  bool const_num_particles_per_cell;
  bool initial_momentum_gamma_correction;
  double wallclock_limit;
  bool write_checkpoint;
  int write_checkpoint_every_step;
  char *fields_base; ///< base type for psc_mfields ("c", "fortran", "cuda")
  char *particles_base; ///< base type for psc_mparticles ("c", "fortran", "cuda")
  int stats_every; ///< output timing and other info every so many steps
  bool detailed_profiling; ///< output profiling info for each process separately
  double theta_xz; ///< rotate anisotropic maxwellian in x-z plane
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
};

// ----------------------------------------------------------------------
// general info / parameters for the code

///Default kinds (electrons + ions)
enum {
  KIND_ELECTRON,
  KIND_ION,
  NR_KINDS,
};

#define CRDX(p, jx) (psc->grid().domain.dx[0] * (jx) + psc->grid().patches[p].xb[0])
#define CRDY(p, jy) (psc->grid().domain.dx[1] * (jy) + psc->grid().patches[p].xb[1])
#define CRDZ(p, jz) (psc->grid().domain.dx[2] * (jz) + psc->grid().patches[p].xb[2])

///This structure holds all the interfaces for the given configuration.
///
///
struct psc {
  ///@defgroup interfaces Interfaces @{
  struct mrc_obj obj;
  struct psc_method *method;                    ///< particular variant of PIC method
  struct psc_marder *marder;                    ///< marder correction
  struct psc_diag *diag;                	///< timeseries diagnostics
  struct psc_output_fields_collection *output_fields_collection; ///< collection of psc_output_fields
  struct psc_output_particles *output_particles;///< particle output
  struct psc_event_generator *event_generator;	///< event generator
  struct psc_checks *checks;                    ///< run-time checks
  ///@}

  ///@defgroup config-params user-configurable parameters @{
  // user-configurable parameters
  struct psc_param prm;		///< normalization parameters set by the user
  struct psc_coeff coeff_;	///< automatically derived constants
  ///@}

  // other parameters / constants
  double p2A, p2B;
  int timestep;	///< the current timestep
  ///@}

  int n_state_fields;           ///< How many field components do we need in ::flds

  ///The domain partitioner.
  ///
  ///Use this to access the global list of patches \sa \ref patches

  struct mrc_domain *mrc_domain_;

  Grid_t* make_grid(struct mrc_domain* mrc_domain, const Grid_t::Domain& domain, const GridBc& bc,
		    const Grid_t::Kinds& kinds, double dt);

  int ibn[3];         ///< number of ghost points

  double time_start;

  const Grid_t& grid() const { assert(grid_); return *grid_; }

  Grid_t* grid_ = {};
};

MRC_CLASS_DECLARE(psc, struct psc);

struct psc_particle_npt {
  int kind; ///< particle kind
  double q; ///< charge
  double m; ///< mass
  double n; ///< density
  double p[3]; ///< momentum
  double T[3]; ///< temperature
  int particles_per_cell; ///< desired number of particles per cell per unit density. If not specified, the global nicell is used.
};

struct psc_ops {
  MRC_SUBCLASS_OPS(struct psc);
};

#define psc_ops(psc) ((struct psc_ops *)((psc)->obj.ops))

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
#define foreach_3d(psc, p, ix, iy, iz, l, r) {                         \
  int __ilo[3] = { (psc)->grid().isInvar(0) ? 0 : -l ,			\
		   (psc)->grid().isInvar(1) ? 0 : -l,	\
		   (psc)->grid().isInvar(2) ? 0 : -l };	\
  int __ihi[3] = { (psc)->grid().ldims[0] + ((psc)->grid().isInvar(0) ? 0 : r), \
		   (psc)->grid().ldims[1] + ((psc)->grid().isInvar(1) ? 0 : r), \
		   (psc)->grid().ldims[2] + ((psc)->grid().isInvar(2) ? 0 : r) }; \
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

/*! Closes a loop started by foreach_3d
 * 
Usage:
\code
psc_foreach_3d(p, jx, jy, jz, 0, 0) {
  //do stuff
} psc_foreach_3d_end;
\endcode
*/
#define foreach_3d_end				\
  } } }

#define psc_foreach_3d(psc, p, ix, iy, iz, l, r) {			\
  int __ilo[3] = { -l, -l, -l };					\
  int __ihi[3] = { psc->grid().ldims[0] + r,				\
		   psc->grid().ldims[1] + r,				\
		   psc->grid().ldims[2] + r };				\
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
  int __ihi[3] = { psc->grid().ldims[0] + psc->ibn[0],			\
		   psc->grid().ldims[1] + psc->ibn[1],			\
		   psc->grid().ldims[2] + psc->ibn[2] };		\
  for (int iz = __ilo[2]; iz < __ihi[2]; iz++) {			\
    for (int iy = __ilo[1]; iy < __ihi[1]; iy++) {			\
      for (int ix = __ilo[0]; ix < __ihi[0]; ix++)

#define psc_foreach_3d_g_end				\
  } } }

// ----------------------------------------------------------------------
// we keep this info global for now.

extern struct psc *ppsc;

extern int pr_time_step_no_comm;

struct psc *psc_create(MPI_Comm comm);
void psc_set_from_options(struct psc *psc);
void psc_view(struct psc *psc);
void psc_destroy(struct psc *psc);

void psc_setup_coeff(struct psc *psc);
void psc_setup_domain(struct psc *psc, const Grid_t::Domain& domain, GridBc& bc, const Grid_t::Kinds& kinds,
		      double dt);
struct mrc_domain *psc_setup_mrc_domain(const Grid_t::Domain&, const GridBc& grid_bc, int nr_patches);

struct psc *psc_read_checkpoint(MPI_Comm comm, int n);
void psc_write_checkpoint(struct psc *psc);

void psc_setup_fortran(struct psc *psc);
void psc_print_profiling(struct psc *psc);

void psc_default_dimensionless(struct psc *psc);

static inline bool psc_at_boundary_lo(struct psc *psc, int p, int d)
{
  return psc->grid().patches[p].off[d] == 0;
}

static inline bool psc_at_boundary_hi(struct psc *psc, int p, int d)
{
  return psc->grid().patches[p].off[d] + psc->grid().ldims[d] == psc->grid().domain.gdims[d];
}

#endif
