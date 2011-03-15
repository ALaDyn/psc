
#include "psc.h"
#include "psc_fields_fortran.h"
#include "psc_glue.h"

void
__fields_fortran_alloc(fields_fortran_t *pf, int ib[3], int ie[3], int nr_comp,
		       fields_fortran_real_t *arr, bool with_array)
{
  pf->flds = calloc(nr_comp, sizeof(*pf->flds));
  pf->name = calloc(nr_comp, sizeof(*pf->name));
  for (int m = 0; m < nr_comp; m++) {
    pf->name[m] = NULL;
  }

  unsigned int size = 1;
  for (int d = 0; d < 3; d++) {
    pf->ib[d] = ib[d];
    pf->im[d] = ie[d] - ib[d];
    size *= pf->im[d];
  }
  pf->nr_comp = nr_comp;
  pf->with_array = with_array;

  if (with_array) {
    assert(nr_comp == 1);
    for (int i = 0; i < nr_comp; i++) {
      pf->flds[i] = arr;
    }
  } else {
    for (int i = 0; i < nr_comp; i++) {
      pf->flds[i] = calloc(sizeof(*pf->flds[i]), size);
    }
  }
}

void
fields_fortran_alloc(fields_fortran_t *pf, int ib[3], int ie[3], int nr_comp)
{
  return __fields_fortran_alloc(pf, ib, ie, nr_comp, NULL, false);
}

void
fields_fortran_alloc_with_array(fields_fortran_t *pf, int ib[3], int ie[3],
				int nr_comp, fields_fortran_real_t *arr)
{
  return __fields_fortran_alloc(pf, ib, ie, nr_comp, arr, true);
}


void
fields_fortran_free(fields_fortran_t *pf)
{
  if (!pf->with_array) {
    for (int i = 0; i < pf->nr_comp; i++) {
      free(pf->flds[i]);
    }
  }
  for (int i = 0; i < pf->nr_comp; i++) {
    pf->flds[i] = NULL;
  }
  for (int m = 0; m < pf->nr_comp; m++) {
    free(pf->name[m]);
  }
  free(pf->name);
  free(pf->flds);
}

#if FIELDS_BASE == FIELDS_FORTRAN

void
fields_fortran_get(mfields_fortran_t *flds, int mb, int me, void *_flds_base)
{
  assert(psc.nr_patches == 1);
  mfields_base_t *flds_base = _flds_base;
  *flds = *flds_base;
}

void
fields_fortran_get_from(mfields_fortran_t *flds, int mb, int me, void *_flds_base,
			int mb_base)
{
  fields_fortran_get(flds, mb, me, &psc.flds);

  mfields_base_t *flds_base = _flds_base;
  foreach_patch(p) {
    fields_base_t *pf = &flds->f[p];
    fields_base_t *pf_base = &flds_base->f[p];
    for (int m = mb; m < me; m++) {
      foreach_3d_g(p, jx, jy, jz) {
	F3_FORTRAN(pf, m, jx,jy,jz) = F3_BASE(pf_base, m - mb + mb_base, jx,jy,jz);
      } foreach_3d_g_end;
    }
  }
}

void
fields_fortran_put(mfields_fortran_t *flds, int mb, int me, void *_flds_base)
{
}

void
fields_fortran_put_to(mfields_fortran_t *flds, int mb, int me, void *_flds_base,
		      int mb_base)
{
  mfields_base_t *flds_base = _flds_base;
  foreach_patch(p) {
    fields_base_t *pf = &flds->f[p];
    fields_base_t *pf_base = &flds_base->f[p];
    for (int m = mb; m < me; m++) {
      foreach_3d_g(p, jx, jy, jz) {
	F3_BASE(pf_base, m - mb + mb_base, jx,jy,jz) = F3_FORTRAN(pf, m, jx,jy,jz);
      } foreach_3d_g_end;
    }
  }
  
  fields_fortran_put(flds, mb, me, &psc.flds);
}

#else

static mfields_fortran_t __flds;
static int __gotten; // to check we're pairing get/put correctly

void
fields_fortran_get_from(mfields_fortran_t *flds, int mb, int me, void *_flds_base,
			int mb_base)
{
  mfields_base_t *flds_base = _flds_base;
  assert(!__gotten);
  __gotten = 1;

  if (!__flds.f) {
    __flds.f = calloc(psc.nr_patches, sizeof(*__flds.f));
    foreach_patch(p) {
      int ilg[3] = { -psc.ibn[0], -psc.ibn[1], -psc.ibn[2] };
      int ihg[3] = { psc.patch[p].ldims[0] + psc.ibn[0],
		     psc.patch[p].ldims[1] + psc.ibn[1],
		     psc.patch[p].ldims[2] + psc.ibn[2] };
      fields_fortran_alloc(&__flds.f[p], ilg, ihg, NR_FIELDS);
    }
  }
  *flds = __flds;

  foreach_patch(p) {
    for (int m = mb; m < me; m++) {
      fields_fortran_t *pf = &flds->f[p];
      fields_base_t *pf_base = &flds_base->f[p];
      foreach_3d_g(p, jx, jy, jz) {
	F3_FORTRAN(pf, m, jx,jy,jz) = F3_BASE(pf_base, m - mb + mb_base, jx,jy,jz);
      } foreach_3d_g_end;
    }
  }
}

void
fields_fortran_get(mfields_fortran_t *flds, int mb, int me, void *flds_base)
{
  fields_fortran_get_from(flds, mb, me, flds_base, mb);
}

void
fields_fortran_put_to(mfields_fortran_t *flds, int mb, int me, void *_flds_base,
		      int mb_base)
{
  mfields_base_t *flds_base = _flds_base;
  assert(__gotten);
  __gotten = 0;

  foreach_patch(p) {
    fields_fortran_t *pf = &flds->f[p];
    fields_base_t *pf_base = &flds_base->f[p];
    for (int m = mb; m < me; m++) {
      foreach_3d_g(p, jx, jy, jz) {
	F3_BASE(pf_base, m - mb + mb_base, jx,jy,jz) = F3_FORTRAN(pf, m, jx,jy,jz);
      } foreach_3d_g_end;
    }
  }
}

void
fields_fortran_put(mfields_fortran_t *flds, int mb, int me, void *flds_base)
{
  return fields_fortran_put_to(flds, mb, me, flds_base, mb);
}

#endif

void
fields_fortran_zero(fields_fortran_t *pf, int m)
{
  memset(pf->flds[m], 0, 
	 pf->im[0] * pf->im[1] * pf->im[2] * sizeof(fields_fortran_real_t));
}

void
fields_fortran_zero_all(fields_fortran_t *pf)
{
  for (int m = 0; m < pf->nr_comp; m++) {
    fields_fortran_zero(pf, m);
  }
}

void
fields_fortran_set(fields_fortran_t *pf, int m, fields_fortran_real_t val)
{
  foreach_patch(patch) {
    foreach_3d_g(patch, jx, jy, jz) {
      F3_FORTRAN(pf, m, jx,jy,jz) = val;
    } foreach_3d_g_end;
  }
}

void
fields_fortran_copy(fields_fortran_t *pf, int m_to, int m_from)
{
  foreach_patch(patch) {
    foreach_3d_g(patch, jx, jy, jz) {
      F3_FORTRAN(pf, m_to, jx,jy,jz) = F3_FORTRAN(pf, m_from, jx,jy,jz);
    } foreach_3d_g_end;
  }
}

void
fields_fortran_axpy_all(fields_fortran_t *y, fields_fortran_real_t a,
			fields_fortran_t *x)
{
  assert(y->nr_comp == x->nr_comp);
  for (int m = 0; m < y->nr_comp; m++) {
    for (int jz = y->ib[2]; jz < y->ib[2] + y->im[2]; jz++) {
      for (int jy = y->ib[1]; jy < y->ib[1] + y->im[1]; jy++) {
	for (int jx = y->ib[0]; jx < y->ib[0] + y->im[0]; jx++) {
	  F3_FORTRAN(y, m, jx, jy, jz) += a * F3_FORTRAN(x, m, jx, jy, jz);
	}
      }
    }
  }
}

void
fields_fortran_scale_all(fields_fortran_t *pf, fields_fortran_real_t val)
{
  for (int m = 0; m < pf->nr_comp; m++) {
    for (int jz = pf->ib[2]; jz < pf->ib[2] + pf->im[2]; jz++) {
      for (int jy = pf->ib[1]; jy < pf->ib[1] + pf->im[1]; jy++) {
	for (int jx = pf->ib[0]; jx < pf->ib[0] + pf->im[0]; jx++) {
	  F3_FORTRAN(pf, m, jx, jy, jz) *= val;
	}
      }
    }
  }
}

