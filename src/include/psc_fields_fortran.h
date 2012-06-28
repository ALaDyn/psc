
#ifndef PSC_FIELD_FORTRAN_H
#define PSC_FIELD_FORTRAN_H

#include "psc_fields_private.h"

typedef double fields_fortran_real_t;
#define MPI_FIELDS_FORTRAN_REAL MPI_DOUBLE

#define F3_OFF_FORTRAN(pf, jx,jy,jz)			\
  (((((((jz)-(pf)->ib[2]))				\
      * (pf)->im[1] + ((jy)-(pf)->ib[1]))		\
     * (pf)->im[0] + ((jx)-(pf)->ib[0]))))

#ifndef BOUNDS_CHECK

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)					\
  (((fields_fortran_real_t **) (pf)->data)[fldnr][F3_OFF_FORTRAN(pf, jx,jy,jz)])

#else

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)					\
  (*({int off = F3_OFF_FORTRAN(pf, jx,jy,jz);				\
      assert(fldnr >= 0 && fldnr < (pf)->nr_comp);			\
      assert(jx >= (pf)->ib[0] && jx < (pf)->ib[0] + (pf)->im[0]);	\
      assert(jy >= (pf)->ib[1] && jy < (pf)->ib[1] + (pf)->im[1]);	\
      assert(jz >= (pf)->ib[2] && jz < (pf)->ib[2] + (pf)->im[2]);	\
      &(((fields_fortran_real_t **) (pf)->data)[fldnr][off]);		\
    }))

#endif

static inline unsigned int
fields_fortran_size(struct psc_fields *pf)
{
  return pf->im[0] * pf->im[1] * pf->im[2];
}

#endif
