
#ifndef MRC_FLD_H
#define MRC_FLD_H

#include <mrc_common.h>
#include <mrc_obj.h>

#include <stdbool.h>

#define MRC_F1(f1,m, ix)					\
  ((f1)->arr[(m) * (f1)->im[0] + (ix) - (f1)->ib[0]])

#define MRC_F2(f2,m, ix,iy)					\
  ((f2)->arr[((m) * (f2)->im[1] + (iy) - (f2)->ib[1]) *		\
	      (f2)->im[0] + (ix) - (f2)->ib[0]])

#define MRC_F3(f3,m, ix,iy,iz)					\
  ((f3)->arr[(((m) * (f3)->im[2] + (iz) - (f3)->ib[2]) *	\
	      (f3)->im[1] + (iy) - (f3)->ib[1]) *		\
	     (f3)->im[0] + (ix) - (f3)->ib[0]])

struct mrc_io;

// ======================================================================
// mrc_f1

struct mrc_f1 {
  struct mrc_obj obj;
  float *arr;
  int im[1];
  int ib[1];
  int nr_comp;
  int len;
  bool with_array;
  char **name;
};

MRC_CLASS_DECLARE(mrc_f1, struct mrc_f1);

// ======================================================================

struct mrc_f2 {
  float *arr;
  int im[2];
  int ib[2];
  int nr_comp;
  int len;
  bool with_array;
  struct mrc_domain *domain; //< optional, if allocated through mrc_domain
  int sw; //< # of ghost points
  char **name;
};

void mrc_f2_alloc(struct mrc_f2 *f2, int ib[2], int im[2], int nr_comp);
void mrc_f2_alloc_with_array(struct mrc_f2 *f2, int ib[2], int im[2], int nr_comp, float *arr);
void mrc_f2_free(struct mrc_f2 *f2);

// ======================================================================

struct mrc_f3 {
  struct mrc_obj obj;
  float *arr;
  int im[3];
  int ib[3];
  int nr_comp;
  int len;
  bool with_array;
  struct mrc_domain *domain; //< optional, if allocated through mrc_domain
  int sw; //< # of ghost points
  char **name;
};

MRC_CLASS_DECLARE(mrc_f3, struct mrc_f3);

struct mrc_f3 *mrc_f3_alloc(MPI_Comm comm, int ib[3], int im[3]);
struct mrc_f3 *mrc_f3_duplicate(struct mrc_f3 *f3);
void mrc_f3_set_nr_comps(struct mrc_f3 *f3, int nr_comps);
void mrc_f3_set_array(struct mrc_f3 *f3, float *arr);
void mrc_f3_copy(struct mrc_f3 *f3_to, struct mrc_f3 *f3_from);
void mrc_f3_set(struct mrc_f3 *f3, float val);
void mrc_f3_write(struct mrc_f3 *f3, struct mrc_io *io);
void mrc_f3_write_scaled(struct mrc_f3 *f3, struct mrc_io *io, float scale);
void mrc_f3_write_comps(struct mrc_f3 *f3, struct mrc_io *io, int mm[]);

static inline bool
mrc_f3_same_shape(struct mrc_f3 *f3_1, struct mrc_f3 *f3_2)
{
  return (f3_1->nr_comp == f3_2->nr_comp &&
	  f3_1->im[0] == f3_2->im[0] &&
	  f3_1->im[1] == f3_2->im[1] &&
	  f3_1->im[2] == f3_2->im[2]);
}

#define mrc_f3_foreach(f3, ix,iy,iz, l,r)				        \
  for (int iz = (f3)->ib[2] + l; iz < (f3)->ib[2] + (f3)->im[2] - r; iz++) {    \
    for (int iy = (f3)->ib[1] + l; iy < (f3)->ib[1] + (f3)->im[1] - r; iy++) {	\
      for (int ix = (f3)->ib[0] + l; ix < (f3)->ib[0] + (f3)->im[0] - r; ix++)	\

#define mrc_f3_foreach_end			\
  }						\
    } do {} while (0)				\

// ======================================================================
// mrc_m1

struct mrc_m1_patch {
  float *arr;
  int im[1];
  int ib[1];
};

struct mrc_m1 {
  struct mrc_obj obj;
  int nr_comp;
  int nr_patches;
  int dim; //< this dimension of the domain
  struct mrc_m1_patch *patches;
  struct mrc_domain *domain;
  int sw;
  char **name;
};

MRC_CLASS_DECLARE(mrc_m1, struct mrc_m1);

static inline struct mrc_m1_patch *
mrc_m1_patch_get(struct mrc_m1 *m1, int p)
{
  return &m1->patches[p];
}

static inline void
mrc_m1_patch_put(struct mrc_m1 *m1)
{
}

#define MRC_M1(m1p,m, ix)					\
  ((m1p)->arr[((m) * (m1p)->im[0] + (ix) - (m1p)->ib[0])])

#define mrc_m1_foreach_patch(m1, p) \
  for (int p = 0; p < m1->nr_patches; p++)

#define mrc_m1_foreach(m1p, ix, l,r) {			\
  int _l[1] = { -l };					\
  int _r[1] = { m1p->im[0] + 2 * m1p->ib[0]  + r};	\
  for (int ix = _l[0]; ix < _r[0]; ix++)		\

#define mrc_m1_foreach_bnd(m1p, ix) {			\
  int _l[1] = { m1p->ib[0] };				\
  int _r[1] = { m1p->ib[0] + m1p->im[0]};		\
  for (int ix = _l[0]; ix < _r[0]; ix++)		\

#define mrc_m1_foreach_end  }
  

// ======================================================================
// mrc_m3

struct mrc_m3_patch {
  float *arr;
  int im[3];
  int ib[3];
};

struct mrc_m3 {
  struct mrc_obj obj;
  int nr_comp;
  int nr_patches;
  struct mrc_m3_patch *patches;
  struct mrc_domain *domain; //< based on this mrc_domain
  int sw; //< # of ghost points
  char **name;
};

MRC_CLASS_DECLARE(mrc_m3, struct mrc_m3);

void mrc_m3_set_nr_comps(struct mrc_m3 *m3, int nr_comps); // FIXME

static inline struct mrc_m3_patch *
mrc_m3_patch_get(struct mrc_m3 *m3, int p)
{
  return &m3->patches[p];
}

static inline void
mrc_m3_patch_put(struct mrc_m3 *m3)
{
}

struct mrc_m3 *mrc_m3_duplicate(struct mrc_m3 *m3);
void mrc_m3_copy(struct mrc_m3 *m3_to, struct mrc_m3 *m3_from);
void mrc_m3_set(struct mrc_m3 *m3, float val);
void mrc_m3_write(struct mrc_m3 *m3, struct mrc_io *io);
void mrc_m3_write_scaled(struct mrc_m3 *m3, struct mrc_io *io, float scale);
void mrc_m3_write_comps(struct mrc_m3 *m3, struct mrc_io *io, int mm[]);

#ifdef BOUNDS_CHECK

#define MRC_M3(m3p,m, ix,iy,iz) (*({					\
  assert((ix) >= (m3p)->ib[0] && ix < (m3p)->ib[0] + (m3p)->im[0]);     \
  assert((iy) >= (m3p)->ib[1] && iy < (m3p)->ib[1] + (m3p)->im[1]);     \
  assert((iz) >= (m3p)->ib[2] && iz < (m3p)->ib[2] + (m3p)->im[2]);     \
  float *__p = &((m3p)->arr[(((m) * (m3p)->im[2] + (iz) - (m3p)->ib[2]) * \
			    (m3p)->im[1] + (iy) - (m3p)->ib[1]) *	\
			   (m3p)->im[0] + (ix) - (m3p)->ib[0]]);	\
  __p;									\
      }))

#else

#define MRC_M3(m3p,m, ix,iy,iz)					\
  ((m3p)->arr[(((m) * (m3p)->im[2] + (iz) - (m3p)->ib[2]) *	\
	       (m3p)->im[1] + (iy) - (m3p)->ib[1]) *		\
	      (m3p)->im[0] + (ix) - (m3p)->ib[0]])

#endif

#define mrc_m3_foreach_patch(m3, p) \
  for (int p = 0; p < m3->nr_patches; p++)

#define mrc_m3_foreach(m3p, ix,iy,iz, l,r) {		\
  int _l[3] = { -l, -l, -l };				\
  int _r[3] = { m3p->im[0] + 2 * m3p->ib[0] + r,	\
		m3p->im[1] + 2 * m3p->ib[1] + r,	\
		m3p->im[2] + 2 * m3p->ib[2] + r};	\
  for (int iz = _l[2]; iz < _r[2]; iz++) {		\
    for (int iy = _l[1]; iy < _r[1]; iy++) {		\
      for (int ix = _l[0]; ix < _r[0]; ix++)		\

#define mrc_m3_foreach_bnd(m3p, ix,iy,iz) {		\
  int _l[3] = { m3p->ib[0], m3p->ib[1], m3p->ib[2] };	\
  int _r[3] = { m3p->ib[0] + m3p->im[0],		\
		m3p->ib[1] + m3p->im[1],		\
		m3p->ib[2] + m3p->im[2] };		\
  for (int iz = _l[2]; iz < _r[2]; iz++) {		\
    for (int iy = _l[1]; iy < _r[1]; iy++) {		\
      for (int ix = _l[0]; ix < _r[0]; ix++)		\

#define mrc_m3_foreach_end  }}}
  

#endif
