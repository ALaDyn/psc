
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
  struct mrc_patch *patches = mrc_domain_get_patches(m1->_domain, &nr_patches);

  m1->nr_patches = nr_patches;
  m1->patches = calloc(nr_patches, sizeof(*patches));
  assert(nr_patches > 0);
  assert(m1->_dims.nr_vals >= 1);
  m1->_dims.vals[0] = patches[0].ldims[m1->_dim];

  if (m1->_offs.nr_vals == 0) {
    mrc_m1_set_param_int_array(m1, "offs", m1->_dims.nr_vals, NULL);
  }
  if (m1->_sw.nr_vals == 0) {
    mrc_m1_set_param_int_array(m1, "sw", m1->_dims.nr_vals, NULL);
  }


  m1->_ghost_offs[0] = -m1->_sw.vals[0];
  m1->_ghost_dims[0] = m1->_dims.vals[0] + 2 * m1->_sw.vals[0];
  for (int p = 0; p < nr_patches; p++) {
    assert(patches[p].ldims[m1->_dim] == patches[0].ldims[m1->_dim]);
    struct mrc_m1_patch *m1p = &m1->patches[p];
    int len = m1->_ghost_dims[0] * m1->nr_comp;
    m1p->arr = calloc(len, sizeof(*m1p->arr));
    m1p->_m1 = m1;
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
  mrc_io_write_ref(io, m1, "domain", m1->_domain);
  mrc_io_write_m1(io, mrc_io_obj_path(io, m1), m1);
}

static void
_mrc_m1_read(struct mrc_m1 *m1, struct mrc_io *io)
{
  m1->_domain = mrc_io_read_ref(io, m1, "domain", mrc_domain);
  
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

void
mrc_m1_set_sw(struct mrc_m1 *fld, int sw)
{
  assert(fld->_domain);
  if (fld->_dims.nr_vals == 3) {
    mrc_m1_set_param_int_array(fld, "sw", fld->_dims.nr_vals,
			       (int[3]) { sw, 0, 0 });
  } else {
    assert(0);
  }
}

bool
mrc_m1_same_shape(struct mrc_m1 *m1_1, struct mrc_m1 *m1_2)
{
  if (m1_1->nr_comp != m1_2->nr_comp) return false;
  if (m1_1->nr_patches != m1_2->nr_patches) return false;
  if (m1_1->_ghost_dims[0] != m1_2->_ghost_dims[0]) return false;

  return true;
}

const int *
mrc_m1_dims(struct mrc_m1 *x)
{
  return x->_dims.vals;
}

const int *
mrc_m1_ghost_offs(struct mrc_m1 *x)
{
  return x->_ghost_offs;
}

const int *
mrc_m1_ghost_dims(struct mrc_m1 *x)
{
  return x->_ghost_dims;
}


// ----------------------------------------------------------------------
// mrc_class_mrc_m1

#define VAR(x) (void *)offsetof(struct mrc_m1, x)
static struct param mrc_m1_params_descr[] = {
  { "offs"            , VAR(_offs)        , PARAM_INT_ARRAY(0, 0) },
  { "dims"            , VAR(_dims)        , PARAM_INT_ARRAY(0, 0) },
  { "sw"              , VAR(_sw)          , PARAM_INT_ARRAY(0, 0) },

  { "nr_comps"        , VAR(nr_comp)      , PARAM_INT(1)           },
  { "dim"             , VAR(_dim)         , PARAM_INT(0)           },
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


