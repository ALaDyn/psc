
#ifndef PSC_FIELD_FORTRAN_H
#define PSC_FIELD_FORTRAN_H

#include "psc.h"

typedef double fields_fortran_real_t;
#define MPI_FIELDS_FORTRAN_REAL MPI_DOUBLE

typedef struct {
  fields_fortran_real_t *flds[NR_FIELDS];
} fields_fortran_t;

#if 1

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)                \
  ((pf)->flds[fldnr][FF3_OFF(jx,jy,jz)])

#else

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)					\
  (*({int off = FF3_OFF(jx,jy,jz);					\
      assert(off >= 0);							\
      assert(off < NR_FIELDS*psc.fld_size);				\
      &((pf)->flds[fldnr][off]);					\
    }))

#endif

void fields_fortran_alloc(fields_fortran_t *pf);
void fields_fortran_free(fields_fortran_t *pf);
void fields_fortran_get(fields_fortran_t *pf, int mb, int me);
void fields_fortran_get_from(fields_fortran_t *pf, int mb, int me,
			     void *pf_base, int mb_base);
void fields_fortran_put(fields_fortran_t *pf, int mb, int me);
void fields_fortran_put_to(fields_fortran_t *pf, int mb, int me,
			   void *pf_base, int mb_base);
void fields_fortran_zero(fields_fortran_t *pf, int m);
void fields_fortran_set(fields_fortran_t *pf, int m, fields_fortran_real_t val);
void fields_fortran_copy(fields_fortran_t *pf, int m_to, int m_from);

#endif
