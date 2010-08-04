
#include "psc.h"
#include "psc_fields_fortran.h"

void
psc_fields_fortran_alloc(psc_fields_fortran_t *pf)
{
  f_real **fields = ALLOC_field();
  for (int i = 0; i < NR_FIELDS; i++) {
    pf->flds[i] = fields[i];
  }
}

void
psc_fields_fortran_free(psc_fields_fortran_t *pf)
{
  FREE_field();
  for (int i = 0; i < NR_FIELDS; i++) {
    pf->flds[i] = NULL;
  }
}

void
psc_fields_fortran_zero(psc_fields_fortran_t *pf, int m)
{
  memset(pf->flds[m], 0, psc.fld_size * sizeof(f_real));
}

#if FIELDS_BASE == FIELDS_FORTRAN

void
psc_fields_fortran_get(psc_fields_fortran_t *pf, int mb, int me)
{
  psc_fields_fortran_t *pf_base = &psc.pf;
  for (int i = 0; i < NR_FIELDS; i++) {
    pf->flds[i] = pf_base->flds[i];
  }
}

void
psc_fields_fortran_put(psc_fields_fortran_t *pf, int mb, int me)
{
}

#else

static psc_fields_fortran_t __flds;
static int __gotten; // to check we're pairing get/put correctly

void
psc_fields_fortran_get(psc_fields_fortran_t *pf, int mb, int me)
{
  assert(!__gotten);
  __gotten = 1;

  if (!__flds.flds[0]) {
    psc_fields_fortran_alloc(&__flds);
  }
  for (int m = 0; m < NR_FIELDS; m++) {
    pf->flds[m] = __flds.flds[m];
  }

  for (int m = mb; m < me; m++) {
    for (int jz = psc.ilg[2]; jz < psc.ihg[2]; jz++) {
      for (int jy = psc.ilg[1]; jy < psc.ihg[1]; jy++) {
	for (int jx = psc.ilg[0]; jx < psc.ihg[0]; jx++) {
	  F3_FORTRAN(pf, m, jx,jy,jz) = F3_BASE(m, jx,jy,jz);
	}
      }
    }
  }
}

void
psc_fields_fortran_put(psc_fields_fortran_t *pf, int mb, int me)
{
  assert(__gotten);
  __gotten = 0;

  for (int m = mb; m < me; m++) {
    for (int jz = psc.ilg[2]; jz < psc.ihg[2]; jz++) {
      for (int jy = psc.ilg[1]; jy < psc.ihg[1]; jy++) {
	for (int jx = psc.ilg[0]; jx < psc.ihg[0]; jx++) {
	  F3_BASE(m, jx,jy,jz) = F3_FORTRAN(pf, m, jx,jy,jz);
	}
      }
    }
  }

  for (int m = 0; m < NR_FIELDS; m++) {
    pf->flds[0] = NULL;
  }
}

#endif
