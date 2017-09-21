
#include "psc.h"
#include "psc_fields_private.h"

#include <mrc_profile.h>
#include <mrc_params.h>
#include <string.h>

// ======================================================================
// psc_fields_size

unsigned int
psc_fields_size(struct psc_fields *pf)
{
  return pf->im[0] * pf->im[1] * pf->im[2];
}

// ======================================================================
// psc_fields_zero_comp

void
psc_fields_zero_comp(struct psc_fields *pf, int m)
{
  struct psc_fields_ops *ops = psc_fields_ops(pf);

  assert(ops && ops->zero_comp);
  ops->zero_comp(pf, m);
}

// ======================================================================
// psc_fields_zero_range

void
psc_fields_zero_range(struct psc_fields *pf, int mb, int me)
{
  for (int m = mb; m < me; m++) {
    psc_fields_zero_comp(pf, m);
  }
}

// ======================================================================
// psc_fields_descr

#define VAR(x) (void *)offsetof(struct psc_fields, x)

static struct param psc_fields_descr[] = {
  { "ib"            , VAR(ib)                     , PARAM_INT3(0, 0, 0)  },
  { "im"            , VAR(im)                     , PARAM_INT3(0, 0, 0)  },
  { "nr_comp"       , VAR(nr_comp)                , PARAM_INT(1)         },
  { "first_comp"    , VAR(first_comp)             , PARAM_INT(0)         },
  { "p"             , VAR(p)                      , PARAM_INT(0)         },
  {}
};

// ======================================================================
// psc_fields_init

static void
psc_fields_init()
{
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_fortran_ops);
#ifdef USE_CUDA
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_cuda_ops);
#endif
#ifdef USE_CUDA2
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_cuda2_ops);
#endif
#ifdef USE_ACC
  mrc_class_register_subclass(&mrc_class_psc_fields, &psc_fields_acc_ops);
#endif
}

// ======================================================================
// psc_fields class

struct mrc_class_psc_fields mrc_class_psc_fields = {
  .name             = "psc_fields",
  .size             = sizeof(struct psc_fields),
  .param_descr      = psc_fields_descr,
  .init             = psc_fields_init,
};

