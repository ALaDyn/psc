
#include <mrc_fld.h>
#include <mrc_io.h>
#include <mrc_params.h>
#include <mrc_vec.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// ======================================================================
// mrc_f1

static void
_mrc_f1_destroy(struct mrc_f1 *f1)
{
  if (f1->_arr) {
    mrc_vec_put_array(f1->_vec, f1->_arr);
    f1->_arr = NULL;
  }

  for (int m = 0; m < f1->_nr_allocated_comp_name; m++) {
    free(f1->_comp_name[m]);
  }
  free(f1->_comp_name);
}

static void
_mrc_f1_setup(struct mrc_f1 *f1)
{
  assert(mrc_f1_nr_comps(f1) > 0);
  if (f1->_domain) {
    int nr_patches;
    struct mrc_patch *patches = mrc_domain_get_patches(f1->_domain, &nr_patches);
    assert(nr_patches == 1);
    assert(f1->_offs.nr_vals == 2);
    f1->_offs.vals[0] = 0;
    assert(f1->_dims.nr_vals == 2);
    f1->_dims.vals[0] = patches[0].ldims[f1->_dim];
  }
  f1->_ghost_offs[0] = f1->_offs.vals[0] - f1->_sw.vals[0];
  f1->_ghost_dims[0] = f1->_dims.vals[0] + 2 * f1->_sw.vals[0];
  f1->_len = f1->_ghost_dims[0] * mrc_f1_nr_comps(f1);

  mrc_vec_set_type(f1->_vec, "float");
  mrc_vec_set_param_int(f1->_vec, "len", f1->_len);
  mrc_f1_setup_member_objs(f1); // sets up our .vec member
  
  f1->_arr = mrc_vec_get_array(f1->_vec);
}

void
mrc_f1_set_array(struct mrc_f1 *f1, float *arr)
{
  mrc_vec_set_array(f1->_vec, arr);
}

static void
_mrc_f1_read(struct mrc_f1 *f1, struct mrc_io *io)
{
  // instead of reading back fld->_vec (which doesn't contain anything useful,
  // anyway, since mrc_fld saves/restores the data rather than mrc_vec),
  // we make a new one, so at least we're sure that with_array won't be honored
  f1->_vec = mrc_vec_create(mrc_f1_comm(f1));
  mrc_f1_setup(f1);
  mrc_io_read_f1(io, mrc_io_obj_path(io, f1), f1);
}

static void
_mrc_f1_write(struct mrc_f1 *f1, struct mrc_io *io)
{
  mrc_io_write_f1(io, mrc_io_obj_path(io, f1), f1);
}

struct mrc_f1 *
mrc_f1_duplicate(struct mrc_f1 *f1_in)
{
  struct mrc_f1 *f1 = mrc_f1_create(mrc_f1_comm(f1_in));

  mrc_f1_set_param_int_array(f1, "offs", f1_in->_offs.nr_vals, f1_in->_offs.vals);
  mrc_f1_set_param_int_array(f1, "dims", f1_in->_dims.nr_vals, f1_in->_dims.vals);
  mrc_f1_set_param_int_array(f1, "sw"  , f1_in->_sw.nr_vals  , f1_in->_sw.vals);
  mrc_f1_set_param_obj(f1, "domain", f1_in->_domain);
  mrc_f1_setup(f1);

  return f1;
}

void
mrc_f1_set_sw(struct mrc_f1 *f1, int sw)
{
  assert(f1->_sw.nr_vals == 2);
  f1->_sw.vals[0] = sw;
}

void
mrc_f1_set_nr_comps(struct mrc_f1 *f1, int nr_comps)
{
  assert(f1->_dims.nr_vals == 2);
  f1->_dims.vals[1] = nr_comps;
}

int
mrc_f1_nr_comps(struct mrc_f1 *f1)
{
  assert(f1->_dims.nr_vals == 2);
  return f1->_dims.vals[1];
}

void
mrc_f1_set_comp_name(struct mrc_f1 *f1, int m, const char *name)
{
  mrc_fld_set_comp_name(f1, m, name);
}

const char *
mrc_f1_comp_name(struct mrc_f1 *f1, int m)
{
  return mrc_fld_comp_name(f1, m);
}

const int *
mrc_f1_dims(struct mrc_f1 *f1)
{
  return f1->_dims.vals;
}

const int *
mrc_f1_off(struct mrc_f1 *f1)
{
  return f1->_offs.vals;
}

const int *
mrc_f1_ghost_dims(struct mrc_f1 *f1)
{
  return f1->_ghost_dims;
}

void
mrc_f1_dump(struct mrc_f1 *x, const char *basename, int n)
{
  struct mrc_io *io = mrc_io_create(MPI_COMM_WORLD);
  mrc_io_set_name(io, "mrc_f1_dump");
  mrc_io_set_param_string(io, "basename", basename);
  mrc_io_set_from_options(io);
  mrc_io_setup(io);
  mrc_io_open(io, "w", n, n);
  mrc_f1_write(x, io);
  mrc_io_close(io);
  mrc_io_destroy(io);
}

void
mrc_f1_zero(struct mrc_f1 *x)
{
  mrc_f1_foreach(x, ix, 0, 0) {
    for (int m = 0; m < mrc_f1_nr_comps(x); m++) {
      MRC_F1(x,m, ix) = 0.;
    }
  } mrc_f1_foreach_end;
}

void
mrc_f1_copy(struct mrc_f1 *x, struct mrc_f1 *y)
{
  assert(mrc_f1_nr_comps(x) == mrc_f1_nr_comps(y));
  assert(x->_ghost_dims[0] == y->_ghost_dims[0]);

  mrc_f1_foreach(x, ix, 0, 0) {
    for (int m = 0; m < mrc_f1_nr_comps(y); m++) {
      MRC_F1(x,m, ix) = MRC_F1(y,m, ix);
    }
  } mrc_f1_foreach_end;
}

void
mrc_f1_waxpy(struct mrc_f1 *w, float alpha, struct mrc_f1 *x, struct mrc_f1 *y)
{
  assert(mrc_f1_nr_comps(w) == mrc_f1_nr_comps(x));
  assert(mrc_f1_nr_comps(w) == mrc_f1_nr_comps(y));
  assert(w->_ghost_dims[0] == x->_ghost_dims[0]);
  assert(w->_ghost_dims[0] == y->_ghost_dims[0]);

  mrc_f1_foreach(w, ix, 0, 0) {
    for (int m = 0; m < mrc_f1_nr_comps(w); m++) {
      MRC_F1(w,m, ix) = alpha * MRC_F1(x,m, ix) + MRC_F1(y,m, ix);
    }
  } mrc_f1_foreach_end;
}

void
mrc_f1_axpy(struct mrc_f1 *y, float alpha, struct mrc_f1 *x)
{
  assert(mrc_f1_nr_comps(x) == mrc_f1_nr_comps(y));
  assert(x->_ghost_dims[0] == y->_ghost_dims[0]);

  mrc_f1_foreach(x, ix, 0, 0) {
    for (int m = 0; m < mrc_f1_nr_comps(y); m++) {
      MRC_F1(y,m, ix) += alpha * MRC_F1(x,m, ix);
    }
  } mrc_f1_foreach_end;
}

float
mrc_f1_norm(struct mrc_f1 *x)
{
  float res = 0.;
  mrc_f1_foreach(x, ix, 0, 0) {
    for (int m = 0; m < mrc_f1_nr_comps(x); m++) {
      res = fmaxf(res, fabsf(MRC_F1(x,m, ix)));
    }
  } mrc_f1_foreach_end;

  MPI_Allreduce(MPI_IN_PLACE, &res, 1, MPI_FLOAT, MPI_MAX, mrc_f1_comm(x));
  return res;
}

float
mrc_f1_norm_comp(struct mrc_f1 *x, int m)
{
  float res = 0.;
  mrc_f1_foreach(x, ix, 0, 0) {
    res = fmaxf(res, fabsf(MRC_F1(x,m, ix)));
  } mrc_f1_foreach_end;

  MPI_Allreduce(MPI_IN_PLACE, &res, 1, MPI_FLOAT, MPI_MAX, mrc_f1_comm(x));
  return res;
}

// ----------------------------------------------------------------------
// mrc_class_mrc_f1

#define VAR(x) (void *)offsetof(struct mrc_f1, x)
static struct param mrc_f1_params_descr[] = {
  { "offs"            , VAR(_offs)        , PARAM_INT_ARRAY(2, 0)    },
  { "dims"            , VAR(_dims)        , PARAM_INT_ARRAY(2, 0)    },
  { "sw"              , VAR(_sw)          , PARAM_INT_ARRAY(2, 0)    },
  { "dim"             , VAR(_dim)         , PARAM_INT(-1)            },

  { "domain"          , VAR(_domain)      , PARAM_OBJ(mrc_domain)    },

  { "vec"             , VAR(_vec)         , MRC_VAR_OBJ(mrc_vec)     },
  {},
};
#undef VAR

static struct mrc_obj_method mrc_f1_methods[] = {
  MRC_OBJ_METHOD("duplicate", mrc_f1_duplicate),
  MRC_OBJ_METHOD("copy"     , mrc_f1_copy),
  MRC_OBJ_METHOD("axpy"     , mrc_f1_axpy),
  MRC_OBJ_METHOD("waxpy"    , mrc_f1_waxpy),
  MRC_OBJ_METHOD("norm"     , mrc_f1_norm),
  {}
};

struct mrc_class_mrc_f1 mrc_class_mrc_f1 = {
  .name         = "mrc_f1",
  .size         = sizeof(struct mrc_f1),
  .param_descr  = mrc_f1_params_descr,
  .methods      = mrc_f1_methods,
  .destroy      = _mrc_f1_destroy,
  .setup        = _mrc_f1_setup,
  .read         = _mrc_f1_read,
  .write        = _mrc_f1_write,
};

// ======================================================================
// mrc_m1

#define to_mrc_m1(o) container_of(o, struct mrc_m1, obj)

static void
_mrc_m1_create(struct mrc_m1 *m1)
{
  m1->_comp_name = calloc(m1->nr_comp, sizeof(*m1->_comp_name));
}

static void
_mrc_m1_destroy(struct mrc_m1 *m1)
{
  for (int p = 0; p < m1->nr_patches; p++) {
    struct mrc_m1_patch *m1p = &m1->patches[p];
    free(m1p->arr);
  }
  free(m1->patches);

  for (int m = 0; m < m1->nr_comp; m++) {
    free(m1->_comp_name[m]);
  }
  free(m1->_comp_name);
}

static void
_mrc_m1_setup(struct mrc_m1 *m1)
{
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(m1->domain, &nr_patches);

  m1->nr_patches = nr_patches;
  m1->patches = calloc(nr_patches, sizeof(*patches));
  for (int p = 0; p < nr_patches; p++) {
    struct mrc_m1_patch *m1p = &m1->patches[p];
    m1p->ib[0] = -m1->sw;
    m1p->im[0] = patches[p].ldims[m1->dim] + 2 * m1->sw;
    int len = m1p->im[0] * m1->nr_comp;
    m1p->arr = calloc(len, sizeof(*m1p->arr));
  }
}

static void
_mrc_m1_view(struct mrc_m1 *m1)
{
#if 0
  int rank, size;
  MPI_Comm_rank(obj->comm, &rank);
  MPI_Comm_size(obj->comm, &size);

  for (int r = 0; r < size; r++) {
    if (r == rank) {
      mrc_m1_foreach_patch(m1, p) {
	struct mrc_m1_patch *m1p = mrc_m1_patch_get(m1, p);
	mprintf("patch %d: ib = %d im = %d\n", p,
		m1p->ib[0], m1p->im[0]);
      }
    }
    MPI_Barrier(obj->comm);
  }
#endif
}

static void
_mrc_m1_write(struct mrc_m1 *m1, struct mrc_io *io)
{
  mrc_io_write_ref(io, m1, "domain", m1->domain);
  mrc_io_write_m1(io, mrc_io_obj_path(io, m1), m1);
}

static void
_mrc_m1_read(struct mrc_m1 *m1, struct mrc_io *io)
{
  m1->domain = mrc_io_read_ref(io, m1, "domain", mrc_domain);
  
  m1->_comp_name = calloc(m1->nr_comp, sizeof(*m1->_comp_name));
  mrc_m1_setup(m1);
  mrc_io_read_m1(io, mrc_io_obj_path(io, m1), m1);
}

void
mrc_m1_set_comp_name(struct mrc_m1 *m1, int m, const char *name)
{
  assert(m < m1->nr_comp);
  free(m1->_comp_name[m]);
  m1->_comp_name[m] = name ? strdup(name) : NULL;
}

const char *
mrc_m1_comp_name(struct mrc_m1 *m1, int m)
{
  assert(m < m1->nr_comp);
  return m1->_comp_name[m];
}

bool
mrc_m1_same_shape(struct mrc_m1 *m1_1, struct mrc_m1 *m1_2)
{
  if (m1_1->nr_comp != m1_2->nr_comp) return false;
  if (m1_1->nr_patches != m1_2->nr_patches) return false;
  mrc_m1_foreach_patch(m1_1, p) {
    struct mrc_m1_patch *m1p_1 = mrc_m1_patch_get(m1_1, p);
    struct mrc_m1_patch *m1p_2 = mrc_m1_patch_get(m1_2, p);

    if (m1p_1->im[0] != m1p_2->im[0])
      return false;

    mrc_m1_patch_put(m1_1);
    mrc_m1_patch_put(m1_2);
  }
  return true;
}


// ----------------------------------------------------------------------
// mrc_class_mrc_m1

#define VAR(x) (void *)offsetof(struct mrc_m1, x)
static struct param mrc_m1_params_descr[] = {
  { "nr_comps"        , VAR(nr_comp)      , PARAM_INT(1)           },
  { "sw"              , VAR(sw)           , PARAM_INT(0)           },
  { "dim"             , VAR(dim)          , PARAM_INT(0)           },
  {},
};
#undef VAR

struct mrc_class_mrc_m1 mrc_class_mrc_m1 = {
  .name         = "mrc_m1",
  .size         = sizeof(struct mrc_m1),
  .param_descr  = mrc_m1_params_descr,
  .create       = _mrc_m1_create,
  .destroy      = _mrc_m1_destroy,
  .setup        = _mrc_m1_setup,
  .view         = _mrc_m1_view,
  .read         = _mrc_m1_read,
  .write        = _mrc_m1_write,
};


