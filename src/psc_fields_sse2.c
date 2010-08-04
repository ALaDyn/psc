
#include "psc.h"
#include "psc_fields_sse2.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if FIELDS_BASE == FIELDS_SSE2

void
psc_fields_sse2_alloc(psc_fields_sse2_t *pf)
{
  pf->flds = calloc(NR_FIELDS * psc.fld_size, sizeof(*pf->flds));
}

void
psc_fields_sse2_free(psc_fields_sse2_t *pf)
{
  free(pf->flds);
}

void
psc_fields_sse2_get(psc_fields_sse2_t *pf, int mb, int me)
{
  *pf = psc.pf;
}

void
psc_fields_sse2_put(psc_fields_sse2_t *pf, int mb, int me)
{
  pf->flds = NULL;
}

#else

static bool __gotten; // to check we're pairing get/put correctly

/// Copy fields from base data structure to an SSE2 friendly format.
void
psc_fields_sse2_get(psc_fields_sse2_t *pf, int mb, int me)
{
  assert(!__gotten);
  __gotten = true;

  pf->flds = _mm_malloc(NR_FIELDS*psc.fld_size*sizeof(sse2_real), 16);
  
  int *ilg = psc.ilg;
  for(int m = mb; m < me; m++){
    for(int n = 0; n < psc.fld_size; n++){
      //preserve Fortran ordering for now
      pf->flds[m * psc.fld_size + n] =
	(sse2_real) ((&F3_BASE(m, ilg[0],ilg[1],ilg[2]))[n]);
    }
  }
}

/// Copy fields from SSE2 data structures into base structures.
void
psc_fields_sse2_put(psc_fields_sse2_t *pf, int mb, int me)
{
  assert(__gotten);
  __gotten = false;

  int *ilg = psc.ilg;
  for(int m = mb; m < me; m++){
    for(int n = 0; n < psc.fld_size; n++){
      ((&F3_BASE(m, ilg[0],ilg[1],ilg[2]))[n]) = 
	pf->flds[m * psc.fld_size + n];
    }
  }
  _mm_free(pf->flds);
}

#endif

void
psc_fields_sse2_zero(psc_fields_sse2_t *pf, int m)
{
  memset(&F3_SSE2(pf, m, psc.ilg[0], psc.ilg[1], psc.ilg[2]), 0,
	 psc.fld_size * sizeof(fields_sse2_real_t));
}

