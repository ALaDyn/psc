
#ifndef GGCM_MHD_DIAG_H
#define GGCM_MHD_DIAG_H

#include <mrc_obj.h>

// ======================================================================
// ggcm_mhd_diag
//
// This object is responsible for handling output from the MHD nodes

MRC_CLASS_DECLARE(ggcm_mhd_diag, struct ggcm_mhd_diag);

// Output whatever is needed at this time.
// (called at every timestep, most of the time it doesn't
// actually write any output, but just returns.)
void ggcm_mhd_diag_run(struct ggcm_mhd_diag *diag);

// Shutdown the output server (if any).
void ggcm_mhd_diag_shutdown(struct ggcm_mhd_diag *diag);

#endif
