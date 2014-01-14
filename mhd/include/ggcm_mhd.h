
#ifndef GGCM_MHD_H
#define GGCM_MHD_H

#include <mrc_obj.h>

#include <mrc_fld.h>
#include <mrc_ts.h>

// ======================================================================
// ggcm_mhd
//
// This object runs an MHD simulation

MRC_CLASS_DECLARE(ggcm_mhd, struct ggcm_mhd);

void ggcm_mhd_fill_ghosts(struct ggcm_mhd *mhd, struct mrc_fld *fld,
			  int m, float bntim);
void ggcm_mhd_newstep(struct ggcm_mhd *mhd, float *dtn);
void ggcm_mhd_calc_divb(struct ggcm_mhd *mhd, struct mrc_fld *fld,
			struct mrc_fld *divb);
void ggcm_mhd_calc_currcc(struct ggcm_mhd *mhd, struct mrc_fld *fld, int m,
			struct mrc_fld *currcc);
void ggcm_mhd_get_state(struct ggcm_mhd *mhd);
void ggcm_mhd_set_state(struct ggcm_mhd *mhd);

int ggcm_mhd_ntot(struct ggcm_mhd *mhd);

void ggcm_mhd_default_box(struct ggcm_mhd *mhd);

void ggcm_mhd_convert_from_primitive(struct ggcm_mhd *mhd,
				     struct mrc_fld *fld_base);

enum {
  MT_PRIMITIVE,
  MT_SEMI_CONSERVATIVE,
  MT_FULLY_CONSERVATIVE,
  N_MT,
};

// ----------------------------------------------------------------------
// wrappers / helpers

void ts_ggcm_mhd_step_calc_rhs(void *ctx, struct mrc_obj *_rhs, float time,
			       struct mrc_obj *_x);
void ts_ggcm_mhd_step_run(void *ctx, struct mrc_ts *ts, struct mrc_obj *_x);

int ggcm_mhd_main(int *argc, char ***argv);

// ----------------------------------------------------------------------

void ggcm_mhd_register();

#endif
