
#include <string.h>
#include <stdlib.h>

#define FTYPE_SINGLE          1
#define FTYPE_C               2
#define FTYPE_FORTRAN         3
#define FTYPE_CUDA            4

#if FTYPE == FTYPE_SINGLE

#define fields_FTYPE_real_t fields_single_real_t
#define fields_FTYPE_t fields_single_t
#define fields_FTYPE_t_ctor fields_single_t_ctor
#define fields_FTYPE_t_dtor fields_single_t_dtor
#define fields_FTYPE_t_mflds fields_single_t_mflds
#define fields_FTYPE_t_size fields_single_t_size
#define fields_FTYPE_t_zero_range fields_single_t_zero_range

#elif FTYPE == FTYPE_C

#define fields_FTYPE_real_t fields_c_real_t
#define fields_FTYPE_t fields_c_t
#define fields_FTYPE_t_ctor fields_c_t_ctor
#define fields_FTYPE_t_dtor fields_c_t_dtor
#define fields_FTYPE_t_mflds fields_c_t_mflds
#define fields_FTYPE_t_size fields_c_t_size
#define fields_FTYPE_t_zero_range fields_c_t_zero_range

#elif FTYPE == FTYPE_FORTRAN

#define fields_FTYPE_real_t fields_fortran_real_t
#define fields_FTYPE_t fields_fortran_t
#define fields_FTYPE_t_ctor fields_fortran_t_ctor
#define fields_FTYPE_t_dtor fields_fortran_t_dtor
#define fields_FTYPE_t_mflds fields_fortran_t_mflds
#define fields_FTYPE_t_size fields_fortran_t_size
#define fields_FTYPE_t_zero_range fields_fortran_t_zero_range

#elif FTYPE == FTYPE_CUDA

#define fields_FTYPE_real_t fields_cuda_real_t
#define fields_FTYPE_t fields_cuda_t
#define fields_FTYPE_t_ctor fields_cuda_t_ctor
#define fields_FTYPE_t_dtor fields_cuda_t_dtor
#define fields_FTYPE_t_mflds fields_cuda_t_mflds
#define fields_FTYPE_t_size fields_cuda_t_size
#define fields_FTYPE_t_zero_range fields_cuda_t_zero_range

#endif

// ----------------------------------------------------------------------
// fields_FYTPE_real_t

#if FTYPE == FTYPE_SINGLE || FTYPE == FTYPE_CUDA

typedef float fields_FTYPE_real_t;

#elif FTYPE == FTYPE_C || FTYPE == FTYPE_FORTRAN

typedef double fields_FTYPE_real_t;

#endif

// ----------------------------------------------------------------------
// MPI_FIELDS_FTYPE_REAL

#if FTYPE == FTYPE_SINGLE

#define MPI_FIELDS_SINGLE_REAL MPI_FLOAT

#elif FTYPE == FTYPE_C

#define MPI_FIELDS_C_REAL MPI_DOUBLE

#elif FTYPE == FTYPE_FORTRAN

#define MPI_FIELDS_FORTRAN_REAL MPI_DOUBLE

#elif FTYPE == FTYPE_CUDA

#define MPI_FIELDS_CUDA_REAL MPI_FLOAT

#endif

// ----------------------------------------------------------------------
// F3

// Lower bounds and dims are intentionally not called ilg, ihg, img,
// to lessen confusion with psc.ilg, psc.ihg, etc.. These bounds may
// be psc.ilo, psc.ihi or psc.ilg, psc.ihg, or something yet different.

#if FTYPE == FTYPE_SINGLE

#define F3_OFF_S(pf, fldnr, jx,jy,jz)					\
  ((((((fldnr - (pf)->first_comp)					\
       * (pf)->im[2] + ((jz)-(pf)->ib[2]))				\
      * (pf)->im[1] + ((jy)-(pf)->ib[1]))				\
     * (pf)->im[0] + ((jx)-(pf)->ib[0]))))

#define _F3_OFF_S(flds, m, i,j,k)					\
  (((((((m) - (flds).first_comp)					\
       * (flds).im[2] + ((k)-(flds).ib[2]))				\
      * (flds).im[1] + ((j)-(flds).ib[1]))				\
     * (flds).im[0] + ((i)-(flds).ib[0]))))

#elif FTYPE == FTYPE_C

#define F3_OFF_C(pf, fldnr, jx,jy,jz)					\
  ((((((fldnr - (pf)->first_comp)					\
       * (pf)->im[2] + ((jz)-(pf)->ib[2]))				\
      * (pf)->im[1] + ((jy)-(pf)->ib[1]))				\
     * (pf)->im[0] + ((jx)-(pf)->ib[0]))))

#define _F3_OFF_C(flds, m, i,j,k)					\
  (((((((m) - (flds).first_comp)					\
       * (flds).im[2] + ((k)-(flds).ib[2]))				\
      * (flds).im[1] + ((j)-(flds).ib[1]))				\
     * (flds).im[0] + ((i)-(flds).ib[0]))))

#elif FTYPE == FTYPE_FORTRAN

#define F3_OFF_FORTRAN(pf, jx,jy,jz)			\
  (((((((jz)-(pf)->ib[2]))				\
      * (pf)->im[1] + ((jy)-(pf)->ib[1]))		\
     * (pf)->im[0] + ((jx)-(pf)->ib[0]))))

#define _F3_OFF_FORTRAN(flds, m, i,j,k)					\
  (((((((m) - (flds).first_comp)					\
       * (flds).im[2] + ((k)-(flds).ib[2]))				\
      * (flds).im[1] + ((j)-(flds).ib[1]))				\
     * (flds).im[0] + ((i)-(flds).ib[0]))))

#endif

#ifndef BOUNDS_CHECK // ------------------------------

#if FTYPE == FTYPE_SINGLE

#define F3_S(pf, fldnr, jx,jy,jz)		\
  (((fields_single_real_t *) (pf)->data)[F3_OFF_S(pf, fldnr, jx,jy,jz)])

#define _F3_S(flds, m, i,j,k)			\
  ((flds).data[_F3_OFF_S(flds, m, i,j,k)])

#elif FTYPE == FTYPE_C

#define F3_C(pf, fldnr, jx,jy,jz)		\
  (((fields_c_real_t *) (pf)->data)[F3_OFF_C(pf, fldnr, jx,jy,jz)])

#define _F3_C(flds, m, i,j,k)			\
  ((flds).data[_F3_OFF_C(flds, m, i,j,k)])

#elif FTYPE == FTYPE_FORTRAN

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)					\
  (((fields_fortran_real_t **) (pf)->data)[fldnr][F3_OFF_FORTRAN(pf, jx,jy,jz)])

#define _F3_FORTRAN(flds, m, i, j, k)					\
  ((flds).data[_F3_OFF_FORTRAN(flds, m, i,j,k)])

#endif

#else // BOUNDS_CHECK ------------------------------

#if FTYPE == FTYPE_SINGLE

#define F3_S(pf, fldnr, jx,jy,jz)					\
  (*({int off = F3_OFF_S(pf, fldnr, jx,jy,jz);				\
      assert(fldnr >= (pf)->first_comp && fldnr < (pf)->first_comp + (pf)->nr_comp); \
      assert(jx >= (pf)->ib[0] && jx < (pf)->ib[0] + (pf)->im[0]);	\
      assert(jy >= (pf)->ib[1] && jy < (pf)->ib[1] + (pf)->im[1]);	\
      assert(jz >= (pf)->ib[2] && jz < (pf)->ib[2] + (pf)->im[2]);	\
      &(((fields_single_real_t *) (pf)->data)[off]);			\
    }))

#elif FTYPE == FTYPE_C

#define F3_C(pf, fldnr, jx,jy,jz)					\
  (*({int off = F3_OFF_C(pf, fldnr, jx,jy,jz);				\
      assert(fldnr >= (pf)->first_comp && fldnr < (pf)->first_comp + (pf)->nr_comp); \
      assert(jx >= (pf)->ib[0] && jx < (pf)->ib[0] + (pf)->im[0]);	\
      assert(jy >= (pf)->ib[1] && jy < (pf)->ib[1] + (pf)->im[1]);	\
      assert(jz >= (pf)->ib[2] && jz < (pf)->ib[2] + (pf)->im[2]);	\
      &(((fields_c_real_t *) (pf)->data)[off]);				\
    }))

#elif FTYPE == FTYPE_FORTRAN

#define F3_FORTRAN(pf, fldnr, jx,jy,jz)					\
  (*({int off = F3_OFF_FORTRAN(pf, jx,jy,jz);				\
      assert(fldnr >= 0 && fldnr < (pf)->nr_comp);			\
      assert(jx >= (pf)->ib[0] && jx < (pf)->ib[0] + (pf)->im[0]);	\
      assert(jy >= (pf)->ib[1] && jy < (pf)->ib[1] + (pf)->im[1]);	\
      assert(jz >= (pf)->ib[2] && jz < (pf)->ib[2] + (pf)->im[2]);	\
      &(((fields_fortran_real_t **) (pf)->data)[fldnr][off]);		\
    }))

#endif

#endif // BOUNDS_CHECK ------------------------------

// ======================================================================
// fields_FTYPE_t

typedef struct {
  fields_FTYPE_real_t *data;
  int ib[3], im[3]; //> lower bounds and length per direction
  int nr_comp; //> nr of components
  int first_comp; // first component
} fields_FTYPE_t;

// ----------------------------------------------------------------------
// fields_t_ctor

static inline fields_FTYPE_t
fields_FTYPE_t_ctor(int ib[3], int im[3], int n_comps)
{
  fields_FTYPE_t flds;

  unsigned int size = 1;
  for (int d = 0; d < 3; d++) {
    flds.ib[d] = ib[d];
    flds.im[d] = im[d];
    size *= im[d];
  }
  flds.nr_comp = n_comps;
  flds.first_comp = 0;
  flds.data = (fields_FTYPE_real_t *) calloc(size * flds.nr_comp, sizeof(*flds.data));

  return flds;
}

// ----------------------------------------------------------------------
// fields_t_dtor

static inline void
fields_FTYPE_t_dtor(fields_FTYPE_t *flds)
{
  free(flds->data);
  flds->data = NULL;
}

// ----------------------------------------------------------------------
// fields_t_mflds

#if FTYPE == FTYPE_SINGLE

struct psc_mfields;
fields_single_t psc_mfields_single_get_field_t(struct psc_mfields *mflds, int p);

static inline fields_single_t
fields_single_t_mflds(struct psc_mfields *mflds, int p)
{
  return psc_mfields_single_get_field_t(mflds, p);
}

#elif FTYPE == FTYPE_C

struct psc_mfields;
fields_c_t psc_mfields_c_get_field_t(struct psc_mfields *mflds, int p);

static inline fields_c_t
fields_c_t_mflds(struct psc_mfields *mflds, int p)
{
  return psc_mfields_c_get_field_t(mflds, p);
}

#elif FTYPE == FTYPE_CUDA

struct psc_mfields;
EXTERN_C fields_cuda_t psc_mfields_cuda_get_field_t(struct psc_mfields *mflds, int p);

static inline fields_cuda_t
fields_cuda_t_mflds(struct psc_mfields *mflds, int p)
{
  return psc_mfields_cuda_get_field_t(mflds, p);
}

#elif FTYPE == FTYPE_FORTRAN

struct psc_mfields;
fields_fortran_t psc_mfields_fortran_get_field_t(struct psc_mfields *mflds, int p);

static inline fields_fortran_t
fields_fortran_t_mflds(struct psc_mfields *mflds, int p)
{
  return psc_mfields_fortran_get_field_t(mflds, p);
}

#endif

// ----------------------------------------------------------------------
// fields_t_size

static inline unsigned int
fields_FTYPE_t_size(fields_FTYPE_t flds)
{
  return flds.im[0] * flds.im[1] * flds.im[2];
}

// ----------------------------------------------------------------------
// fields_t_zero_range

static inline void
fields_FTYPE_t_zero_range(fields_FTYPE_t flds, int mb, int me)
{
  for (int m = mb; m < me; m++) {
#if FTYPE == FTYPE_SINGLE
    memset(&_F3_S(flds, m, flds.ib[0], flds.ib[1], flds.ib[2]), 0,
	   flds.im[0] * flds.im[1] * flds.im[2] * sizeof(fields_FTYPE_real_t));
#elif FTYPE == FTYPE_C
    memset(&_F3_C(flds, m, flds.ib[0], flds.ib[1], flds.ib[2]), 0,
	   flds.im[0] * flds.im[1] * flds.im[2] * sizeof(fields_FTYPE_real_t));
#else
    assert(0);
#endif
  }
}

// ----------------------------------------------------------------------

#undef fields_FTYPE_real_t
#undef fields_FTYPE_t
#undef fields_FTYPE_t_ctor
#undef fields_FTYPE_t_dtor
#undef fields_FTYPE_t_mflds
#undef fields_FTYPE_t_size
#undef fields_FTYPE_t_zero_range



