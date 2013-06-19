
#include "psc_push_fields_private.h"
#include "psc_fields_cuda.h"
#include "psc.h"
#include "psc_bnd.h"
#include "psc_bnd_fields.h"

EXTERN_C void cuda_push_fields_E_yz(int p, struct psc_fields *flds);
EXTERN_C void cuda_push_fields_H_yz(int p, struct psc_fields *flds);

// ----------------------------------------------------------------------
// psc_push_fields_cuda_push_mflds_E

static void
psc_push_fields_cuda_push_mflds_E(struct psc_push_fields *push, struct psc_mfields *mflds_base)
{
  struct psc_mfields *mflds = psc_mfields_get_cuda(mflds_base, JXI, HX + 3);
  if (ppsc->domain.gdims[0] == 1) {
    for (int p = 0; p < mflds->nr_patches; p++) {
	cuda_push_fields_E_yz(p, psc_mfields_get_patch(mflds, p));
    }
  } else {
    assert(0);
  }
  psc_mfields_put_cuda(mflds, mflds_base, EX, EX + 3);
}

// ----------------------------------------------------------------------
// psc_push_fields_cuda_push_mflds_H

static void
psc_push_fields_cuda_push_mflds_H(struct psc_push_fields *push, struct psc_mfields *mflds_base)
{
  struct psc_mfields *mflds = psc_mfields_get_cuda(mflds_base, EX, HX + 3);
  if (ppsc->domain.gdims[0] == 1) {
    for (int p = 0; p < mflds->nr_patches; p++) {
      cuda_push_fields_H_yz(p, psc_mfields_get_patch(mflds, p));
    }
  } else {
    assert(0);
  }
  psc_mfields_put_cuda(mflds, mflds_base, HX, HX + 3);
}

// ======================================================================
// psc_push_fields: subclass "cuda"

struct psc_push_fields_ops psc_push_fields_cuda_ops = {
  .name                  = "cuda",
  .push_mflds_E          = psc_push_fields_cuda_push_mflds_E,
  .push_mflds_H          = psc_push_fields_cuda_push_mflds_H,
};
