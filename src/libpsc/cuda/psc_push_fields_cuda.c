
#include "psc_push_fields_private.h"
#include "psc.h"

EXTERN_C void cuda_push_fields_a_E_yz(int p, fields_cuda_t *pf);
EXTERN_C void cuda_push_fields_a_H_yz(int p, fields_cuda_t *pf);
EXTERN_C void cuda_push_fields_b_H_yz(int p, fields_cuda_t *pf);
EXTERN_C void cuda_push_fields_b_E_yz(int p, fields_cuda_t *pf);

// ----------------------------------------------------------------------
// E-field propagation E^(n)    , H^(n), j^(n) 
//                  -> E^(n+0.5), H^(n), j^(n)

static void
psc_push_fields_cuda_push_a_E(struct psc_push_fields *push, mfields_base_t *flds_base)
{
  mfields_cuda_t flds;
  psc_mfields_cuda_get_from(&flds, JXI, HX + 3, flds_base);

  if (ppsc->domain.gdims[0] == 1) {
    psc_foreach_patch(ppsc, p) {
      cuda_push_fields_a_E_yz(p, &flds.f[p]);
    }
  } else {
    assert(0);
  }

  psc_mfields_cuda_put_to(&flds, EX, EX + 3, flds_base);
}

// ----------------------------------------------------------------------
// B-field propagation E^(n+0.5), H^(n    ), j^(n), m^(n+0.5)
//                  -> E^(n+0.5), H^(n+0.5), j^(n), m^(n+0.5)

static void
psc_push_fields_cuda_push_a_H(struct psc_push_fields *push, mfields_base_t *flds_base)
{
  mfields_cuda_t flds;
  psc_mfields_cuda_get_from(&flds, EX, HX + 3, flds_base);

  if (ppsc->domain.gdims[0] == 1) {
    psc_foreach_patch(ppsc, p) {
      cuda_push_fields_a_H_yz(p, &flds.f[p]);
    }
  } else {
    assert(0);
  }

  psc_mfields_cuda_put_to(&flds, HX, HX + 3, flds_base);
}

// ----------------------------------------------------------------------
// B-field propagation E^(n+0.5), B^(n+0.5), j^(n+1.0), m^(n+0.5)
//                  -> E^(n+0.5), B^(n+1.0), j^(n+1.0), m^(n+0.5)

static void
psc_push_fields_cuda_push_b_H(struct psc_push_fields *push, mfields_base_t *flds_base)
{
  mfields_cuda_t flds;
  psc_mfields_cuda_get_from(&flds, EX, HX + 3, flds_base);

  if (ppsc->domain.gdims[0] == 1) {
    psc_foreach_patch(ppsc, p) {
      cuda_push_fields_b_H_yz(p, &flds.f[p]);
    }
  } else {
    assert(0);
  }

  psc_mfields_cuda_put_to(&flds, HX, HX + 3, flds_base);
}

// ----------------------------------------------------------------------
// E-field propagation E^(n+0.5), B^(n+1.0), j^(n+1.0) 
//                  -> E^(n+1.0), B^(n+1.0), j^(n+1.0)

static void
psc_push_fields_cuda_push_b_E(struct psc_push_fields *push, mfields_base_t *flds_base)
{
  mfields_cuda_t flds;
  psc_mfields_cuda_get_from(&flds, JXI, HX + 3, flds_base);

  if (ppsc->domain.gdims[0] == 1) {
    psc_foreach_patch(ppsc, p) {
      cuda_push_fields_b_E_yz(p, &flds.f[p]);
    }
  } else {
    assert(0);
  }

  psc_mfields_cuda_put_to(&flds, EX, EX + 3, flds_base);
}

// ======================================================================
// psc_push_fields: subclass "cuda"

struct psc_push_fields_ops psc_push_fields_cuda_ops = {
  .name                  = "cuda",
  .push_a_E              = psc_push_fields_cuda_push_a_E,
  .push_a_H              = psc_push_fields_cuda_push_a_H,
  .push_b_H              = psc_push_fields_cuda_push_b_H,
  .push_b_E              = psc_push_fields_cuda_push_b_E,
};
