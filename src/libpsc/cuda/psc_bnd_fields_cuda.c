
#include "psc_bnd_fields_private.h"
#include "psc_fields_as_c.h"

#include "psc.h"
#include <mrc_profile.h>


EXTERN_C void cuda_conducting_wall_H_lo_hi_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_E_lo_hi_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_J_lo_hi_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_H_lo_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_H_hi_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_E_lo_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_E_hi_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_J_lo_y(int p, struct psc_fields *pf);
EXTERN_C void cuda_conducting_wall_J_hi_y(int p, struct psc_fields *pf);

// ----------------------------------------------------------------------
// psc_bnd_fields_cuda_fill_ghosts_E

static void
psc_bnd_fields_cuda_fill_ghosts_E(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    return;
  }

  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, "cuda", EX, EX + 3);
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_hi[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    int d = 1;
    for (int p = 0; p < mflds->nr_patches; p++) {
      if (ppsc->patch[p].off[d] == 0) {
	cuda_conducting_wall_E_lo_y(p, psc_mfields_get_patch(mflds, p));
      }
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	cuda_conducting_wall_E_hi_y(p, psc_mfields_get_patch(mflds, p));
      }
    }
  } else {
    assert(0);
  }
  psc_mfields_put_as(mflds, mflds_base, EX, EX + 3);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_cuda_fill_ghosts_H

static void
psc_bnd_fields_cuda_fill_ghosts_H(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    return;
  }

  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, "cuda", HX, HX + 3);
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_hi[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    int d = 1;
    for (int p = 0; p < mflds->nr_patches; p++) {
      if (ppsc->patch[p].off[d] == 0) {
	cuda_conducting_wall_H_lo_y(p, psc_mfields_get_patch(mflds, p));
      }
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	cuda_conducting_wall_H_hi_y(p, psc_mfields_get_patch(mflds, p));

      }
    }
  } else {
    assert(0);
  }
  psc_mfields_put_as(mflds, mflds_base, HX, HX + 3);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_cuda_add_ghosts_J

static void
psc_bnd_fields_cuda_add_ghosts_J(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    return;
  }

  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, "cuda", JXI, JXI + 3);
  if (ppsc->domain.bnd_fld_lo[0] == BND_FLD_PERIODIC &&
      ppsc->domain.bnd_fld_lo[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_hi[1] == BND_FLD_CONDUCTING_WALL &&
      ppsc->domain.bnd_fld_lo[2] == BND_FLD_PERIODIC) {
    int d = 1;
    for (int p = 0; p < mflds->nr_patches; p++) {
      if (ppsc->patch[p].off[d] == 0) {
	cuda_conducting_wall_J_lo_y(p, psc_mfields_get_patch(mflds, p));
      }
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	cuda_conducting_wall_J_hi_y(p, psc_mfields_get_patch(mflds, p));
      }
    }
  } else {
    assert(0);
  }
  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);
}

// ======================================================================
// psc_bnd_fields: subclass "cuda"

struct psc_bnd_fields_ops psc_bnd_fields_cuda_ops = {
  .name                  = "cuda",
  .fill_ghosts_a_E       = psc_bnd_fields_cuda_fill_ghosts_E,
  .fill_ghosts_a_H       = psc_bnd_fields_cuda_fill_ghosts_H,
  // OPT fill_ghosts_b_E is probably not needed except for proper output
  // fill_ghosts_b_H: ?
  .fill_ghosts_b_E       = psc_bnd_fields_cuda_fill_ghosts_E,
  .fill_ghosts_b_H       = psc_bnd_fields_cuda_fill_ghosts_H,
  .add_ghosts_J          = psc_bnd_fields_cuda_add_ghosts_J,
};
