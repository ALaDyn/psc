
#ifndef PSC_FIELDS_CUDA2_H
#define PSC_FIELDS_CUDA2_H

#include "psc_fields_private.h"

typedef float fields_cuda2_real_t;

#define MPI_FIELDS_CUDA2_REAL MPI_FLOAT

struct psc_fields_cuda2 {
};

#define psc_fields_cuda2(flds) mrc_to_subobj(flds, struct psc_fields_cuda2)

// ----------------------------------------------------------------------
// macros to access C (host) versions of the fields

#define F3_OFF_CUDA2(flds, fldnr, jx,jy,jz)				\
  ((((((fldnr)								\
       * (flds)->im[2] + ((jz)-(flds)->ib[2]))				\
      * (flds)->im[1] + ((jy)-(flds)->ib[1]))				\
     * (flds)->im[0] + ((jx)-(flds)->ib[0]))))

#ifndef BOUNDS_CHECK

#define F3_CUDA2(flds, fldnr, jx,jy,jz)		\
  (((fields_cuda2_real_t *) (flds)->data)[F3_OFF_CUDA2(flds, fldnr, jx,jy,jz)])

#else

#define F3_CUDA2(flds, fldnr, jx,jy,jz)					\
  (*({int off = F3_OFF_CUDA2(flds, fldnr, jx,jy,jz);			\
      assert(fldnr >= 0 && fldnr < (flds)->nr_comp);			\
      assert(jx >= (flds)->ib[0] && jx < (flds)->ib[0] + (flds)->im[0]); \
      assert(jy >= (flds)->ib[1] && jy < (flds)->ib[1] + (flds)->im[1]); \
      assert(jz >= (flds)->ib[2] && jz < (flds)->ib[2] + (flds)->im[2]); \
      &(((fields_single_real_t *) (flds)->data)[off]);			\
    }))

#endif

// ----------------------------------------------------------------------

struct psc_mfields_cuda2 {
  fields_cuda2_real_t *h_flds;
  int ib[3], im[3];
};

#define psc_mfields_cuda2(mflds) mrc_to_subobj(mflds, struct psc_mfields_cuda2)

#endif
