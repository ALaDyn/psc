
#ifndef GGCM_MHD_PRIVATE_H
#define GGCM_MHD_PRIVATE_H

#include "ggcm_mhd.h"

#include "mrc_crds.h"

enum {
  MAGDIFFU_NL1,
  MAGDIFFU_RES1,
  MAGDIFFU_CONST,
};

#define GK_NR_FLUIDS_MAX (5)

struct ggcm_mhd_params {
  float gamm;
  float rrmin;

  float bbnorm0, vvnorm0, rrnorm0, ppnorm0;
  float ccnorm0, eenorm0, resnorm0, tnorm0;
  float qqnorm0;
  float norm_length; // normalizing length (in m)
  float norm_B; // normalizing magnetic field (in T)
  float norm_density; // normalizing density (in 1/m^3)
  float mu0;
  float amu;

  float diffco, diffth;
  float diffsphere;
  float speedlimit, thx;
  float r_db_dt, isphere, timelo;
  float diff_timelo;
  float diff_swbnd;
  int diff_obnd;
  float d_i;
  float dtmin;
  double dbasetime;
  int modnewstep;
  int magdiffu;
  bool do_badval_checks; // check for NaN or negative density / pressure

  bool do_limit2;
  bool do_limit3;
  bool limit_aspect_low;
  bool calce_aspect_low;

  // params for multi-fluid moment runs
  // to be obtained from gkeyll instead
  // of from command line options
  int nr_fluids;
  int nr_moments;
  double mass_ratios[GK_NR_FLUIDS_MAX];
  double momentum_ratios[GK_NR_FLUIDS_MAX];
  double pressure_ratios[GK_NR_FLUIDS_MAX];

  bool monitor_conservation;
};

struct ggcm_mhd {
  struct mrc_obj obj;
  struct ggcm_mhd_params par;
  int amr; //< turn on if > 0, value selects initial domain refinement
  char *amr_grid_file;  // used if mhd->amr == 999  
  struct mrc_ddc *ddc_amr_cc;
  struct mrc_ddc *ddc_amr_flux[3];
  struct mrc_ddc *ddc_amr_E;

  struct mrc_domain *domain;
  struct mrc_fld *fld;
  struct mrc_fld *ymask;
  struct mrc_fld *bnd_mask;
  // background B field
  // b0 = NULL means there is none
  struct mrc_fld *b0;
  struct ggcm_mhd_crds *crds;
  struct ggcm_mhd_step *step;
  struct ggcm_mhd_bnd *bnd;
  struct ggcm_mhd_bnd *bnd1;
  struct ggcm_mhd_diag *diag;
  struct ggcm_mhd_ic *ic;

  // mhd state
  // normalization parameters
  // multiplying the internal normalized quantities by these will produce
  // physical values in SI units, but with a prefix given by the corresponding
  // XXnorm0 parameter
  float bbnorm, vvnorm, rrnorm, ppnorm;
  float ccnorm, eenorm, resnorm, tnorm;
  float qqnorm;

  float time; // current time
  float dt;   // current timestep (parameter to pred/corr, so can be .5 dt)
  int istep;
  float timla;
  double dacttime;
  float max_time;  // set from mrc_ts->max_time at the start of the run

  float bndt; // .5 * current timestep in sec, not alfven times

  // for easy access, cached from ::domain
  int im[3];  // local domain excl ghost points
  int img[3]; // local domain incl ghost points
};

struct ggcm_mhd_ops {
  MRC_SUBCLASS_OPS(struct ggcm_mhd);
  void (*get_state)(struct ggcm_mhd *mhd);
  void (*set_state)(struct ggcm_mhd *mhd);
};

extern struct ggcm_mhd_ops ggcm_mhd_ops_box;

// ----------------------------------------------------------------------

// helpers for subclasses to use

struct mrc_fld *ggcm_mhd_get_3d_fld(struct ggcm_mhd *mhd, int nr_comps);
void ggcm_mhd_put_3d_fld(struct ggcm_mhd *mhd, struct mrc_fld *f);

struct mrc_ddc *ggcm_mhd_create_amr_ddc(struct ggcm_mhd *mhd);
struct mrc_ddc *ggcm_mhd_create_amr_ddc_flux(struct ggcm_mhd *mhd, int d);
struct mrc_ddc *ggcm_mhd_create_amr_ddc_E(struct ggcm_mhd *mhd);
void ggcm_mhd_setup_amr_domain(struct ggcm_mhd *mhd);

// reference implementation only
void ggcm_mhd_amr_fill_ghosts_b(struct ggcm_mhd *mhd, struct mrc_fld *fld);

// direct access to coords for a given cell
// (ideally avoided for performance critical parts, because it's slower)
//
// FIXME, this should maybe become a feature of mrc_crds, or go away entirely
// because it duplicates already existing functionality to access coordinates via
// MRC_MCRD macros, though the latter only support cell-centered coords at this
// time

void primvar_c(struct ggcm_mhd *mhd, int m_curr);
void primvar_float(struct ggcm_mhd *mhd, int m_curr);
void primvar_double(struct ggcm_mhd *mhd, int m_curr);
void primvar1_c(struct ggcm_mhd *mhd);
void primbb_c(struct ggcm_mhd *mhd, int m_curr);
void primbb_float(struct ggcm_mhd *mhd, int m_curr);
void primbb_double(struct ggcm_mhd *mhd, int m_curr);
void zmaskn_c(struct ggcm_mhd *mhd);
void zmaskn_float(struct ggcm_mhd *mhd);
void zmaskn_double(struct ggcm_mhd *mhd);
void newstep(struct ggcm_mhd *mhd, float *dtn);

#endif
