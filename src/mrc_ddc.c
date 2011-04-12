
#include "mrc_ddc.h"
#include "mrc_ddc_private.h"

#include <mrc_params.h>

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define mrc_ddc_ops(domain) ((struct mrc_ddc_ops *) ddc->obj.ops)

// ----------------------------------------------------------------------
// mrc_ddc_create

static void
_mrc_ddc_create(struct mrc_ddc *ddc)
{
  MPI_Comm_rank(mrc_ddc_comm(ddc), &ddc->rank);
  MPI_Comm_size(mrc_ddc_comm(ddc), &ddc->size);
}

// ----------------------------------------------------------------------
// mrc_ddc_set_funcs

void
mrc_ddc_set_funcs(struct mrc_ddc *ddc, struct mrc_ddc_funcs *funcs)
{
  ddc->funcs = funcs;
}

// ----------------------------------------------------------------------
// mrc_ddc_set_domain

void
mrc_ddc_set_domain(struct mrc_ddc *ddc, struct mrc_domain *domain)
{
  struct mrc_ddc_ops *ops = mrc_ddc_ops(ddc);
  assert(ops->set_domain);
  ops->set_domain(ddc, domain);
}

// ----------------------------------------------------------------------
// mrc_ddc_get_domain

struct mrc_domain *
mrc_ddc_get_domain(struct mrc_ddc *ddc)
{
  struct mrc_ddc_ops *ops = mrc_ddc_ops(ddc);
  assert(ops->get_domain);
  return ops->get_domain(ddc);
}

// ----------------------------------------------------------------------
// mrc_ddc_fill_ghosts

void
mrc_ddc_fill_ghosts(struct mrc_ddc *ddc, int mb, int me, void *ctx)
{
  struct mrc_ddc_ops *ops = mrc_ddc_ops(ddc);
  assert(ops->fill_ghosts);
  ops->fill_ghosts(ddc, mb, me, ctx);
}

// ----------------------------------------------------------------------
// mrc_ddc_add_ghosts

void
mrc_ddc_add_ghosts(struct mrc_ddc *ddc, int mb, int me, void *ctx)
{
  struct mrc_ddc_ops *ops = mrc_ddc_ops(ddc);
  assert(ops->add_ghosts);
  ops->add_ghosts(ddc, mb, me, ctx);
}

// ----------------------------------------------------------------------
// mrc_ddc_get_nei_rank_patch

void
mrc_ddc_get_nei_rank_patch(struct mrc_ddc *ddc, int p, int dir[3],
			   int *nei_rank, int *nei_patch)
{
  struct mrc_ddc_ops *ops = mrc_ddc_ops(ddc);
  assert(ops->get_nei_rank_patch);
  ops->get_nei_rank_patch(ddc, p, dir, nei_rank, nei_patch);
}

// ======================================================================
// mrc_ddc_init

static void
mrc_ddc_init()
{
  mrc_class_register_subclass(&mrc_class_mrc_ddc, &mrc_ddc_simple_ops);
  mrc_class_register_subclass(&mrc_class_mrc_ddc, &mrc_ddc_multi_ops);
}

// ======================================================================
// mrc_ddc class

#define VAR(x) (void *)offsetof(struct mrc_ddc, x)
static struct param mrc_ddc_params_descr[] = {
  { "size_of_type"    , VAR(size_of_type) , PARAM_INT(0)           },
  { "max_n_fields"    , VAR(max_n_fields) , PARAM_INT(1)           },
  { "ibn"             , VAR(ibn)          , PARAM_INT3(0, 0, 0)    },
  {},
};
#undef VAR

struct mrc_class_mrc_ddc mrc_class_mrc_ddc = {
  .name             = "mrc_ddc",
  .size             = sizeof(struct mrc_ddc),
  .param_descr      = mrc_ddc_params_descr,
  .init             = mrc_ddc_init,
  .create           = _mrc_ddc_create,
};

