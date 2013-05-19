
#include <mrc_domain_private.h>
#include <mrc_fld.h>
#include <mrc_params.h>
#include <mrc_io.h>
#include <mrc_ddc.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

static inline struct mrc_domain_ops *
mrc_domain_ops(struct mrc_domain *domain)
{
  return (struct mrc_domain_ops *) domain->obj.ops;
}

// ======================================================================
// mrc_domain

static void
_mrc_domain_create(struct mrc_domain *domain)
{
  domain->crds = mrc_crds_create(mrc_domain_comm(domain));
  mrc_crds_set_domain(domain->crds, domain); // FIXME, should be added as child

  domain->ddc = mrc_ddc_create(mrc_domain_comm(domain));
  // FIXME, this isn't really general, though ok if we always use
  // multi for whatever domain. Otherwise, we need a call back for set type to
  // updated the sub type accordingly... Similar problem exists with crds, btw.
  mrc_ddc_set_type(domain->ddc, "multi");
  mrc_ddc_set_domain(domain->ddc, domain);
  mrc_ddc_set_param_int(domain->ddc, "size_of_type", sizeof(float));
  mrc_ddc_set_funcs(domain->ddc, &mrc_ddc_funcs_f3);
  mrc_domain_add_child(domain, (struct mrc_obj *) domain->ddc);
}

static void
_mrc_domain_destroy(struct mrc_domain *domain)
{
  mrc_crds_destroy(domain->crds);
}

static void
_mrc_domain_set_from_options(struct mrc_domain *domain)
{
  mrc_crds_set_from_options(domain->crds);
}

static void
_mrc_domain_view(struct mrc_domain *domain)
{
  mrc_crds_view(domain->crds);
}

static void
_mrc_domain_setup(struct mrc_domain *domain)
{
  if (domain->crds) {
    mrc_crds_setup(domain->crds);
  }
  mrc_domain_setup_children(domain);
}

static void
_mrc_domain_read(struct mrc_domain *domain, struct mrc_io *io)
{
  mrc_crds_destroy(domain->crds);
  domain->crds = NULL;

  mrc_domain_setup(domain);
  domain->crds = mrc_io_read_ref(io, domain, "crds", mrc_crds);
}

static void
_mrc_domain_write(struct mrc_domain *domain, struct mrc_io *io)
{
  mrc_io_write_int(io, domain, "mpi_size", domain->size);
  mrc_io_write_ref(io, domain, "crds", domain->crds);
}

struct mrc_patch *
mrc_domain_get_patches(struct mrc_domain *domain, int *nr_patches)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_patches);
  return mrc_domain_ops(domain)->get_patches(domain, nr_patches);
}

struct mrc_crds *
mrc_domain_get_crds(struct mrc_domain *domain)
{
  return domain->crds;
}

struct mrc_ddc *
mrc_domain_get_ddc(struct mrc_domain *domain)
{
  return domain->ddc;
}

int
mrc_domain_get_neighbor_rank(struct mrc_domain *domain, int shift[3])
{
  assert(mrc_domain_is_setup(domain));
  return mrc_domain_ops(domain)->get_neighbor_rank(domain, shift);
}

void
mrc_domain_get_global_dims(struct mrc_domain *domain, int *dims)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_global_dims);
  mrc_domain_ops(domain)->get_global_dims(domain, dims);
}

void
mrc_domain_get_nr_procs(struct mrc_domain *domain, int *nr_procs)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_nr_procs);
  mrc_domain_ops(domain)->get_nr_procs(domain, nr_procs);
}

void
mrc_domain_get_bc(struct mrc_domain *domain, int *bc)
{
  assert(mrc_domain_is_setup(domain));
  mrc_domain_ops(domain)->get_bc(domain, bc);
}

void
mrc_domain_get_nr_global_patches(struct mrc_domain *domain, int *nr_global_patches)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_nr_global_patches);
  mrc_domain_ops(domain)->get_nr_global_patches(domain, nr_global_patches);
}

void
mrc_domain_get_global_patch_info(struct mrc_domain *domain, int gp,
				 struct mrc_patch_info *info)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_global_patch_info);
  mrc_domain_ops(domain)->get_global_patch_info(domain, gp, info);
}

void
mrc_domain_get_local_patch_info(struct mrc_domain *domain, int p,
				struct mrc_patch_info *info)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_local_patch_info);
  mrc_domain_ops(domain)->get_local_patch_info(domain, p, info);
}

void
mrc_domain_get_level_idx3_patch_info(struct mrc_domain *domain, int level,
				     int idx[3], struct mrc_patch_info *info)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->get_level_idx3_patch_info);
  mrc_domain_ops(domain)->get_level_idx3_patch_info(domain, level, idx, info);
}

void
mrc_domain_get_nr_levels(struct mrc_domain *domain, int *p_nr_levels)
{
  assert(mrc_domain_is_setup(domain));
  if (!mrc_domain_ops(domain)->get_nr_levels) {
    *p_nr_levels = 1;
    return;
  }
  mrc_domain_ops(domain)->get_nr_levels(domain, p_nr_levels);
}

void
mrc_domain_get_neighbor_rank_patch(struct mrc_domain *domain, int p, int dir[3],
				   int *nei_rank, int *nei_patch)
{
  struct mrc_domain_ops *ops = mrc_domain_ops(domain);
  assert(ops->get_neighbor_rank_patch);
  ops->get_neighbor_rank_patch(domain, p, dir, nei_rank, nei_patch);
}

void
mrc_domain_plot(struct mrc_domain *domain)
{
  assert(mrc_domain_is_setup(domain));
  assert(mrc_domain_ops(domain)->plot);
  mrc_domain_ops(domain)->plot(domain);
}

void
mrc_domain_add_patch(struct mrc_domain *domain, int l, int idx3[3])
{
  assert(mrc_domain_ops(domain)->add_patch);
  mrc_domain_ops(domain)->add_patch(domain, l, idx3);
}

// ======================================================================

struct mrc_ddc *
mrc_domain_create_ddc(struct mrc_domain *domain)
{
  assert(mrc_domain_ops(domain)->create_ddc);
  return mrc_domain_ops(domain)->create_ddc(domain);
}

// ======================================================================

struct mrc_f1 *
mrc_domain_f1_create(struct mrc_domain *domain)
{
  int nr_patches;
  mrc_domain_get_patches(domain, &nr_patches);
  assert(nr_patches == 1);
  struct mrc_f1 *f1 = mrc_f1_create(mrc_domain_comm(domain));
  f1->domain = domain;
  return f1;
}

// ======================================================================
// mrc_domain_fld_create

struct mrc_fld *
mrc_domain_fld_create(struct mrc_domain *domain, int sw, int nr_comp)
{
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(domain, &nr_patches);
  assert(nr_patches == 1);
  struct mrc_fld *fld = mrc_fld_create(mrc_domain_comm(domain));
  int dims[4] = { patches[0].ldims[0], patches[0].ldims[1], patches[0].ldims[2],
		  nr_comp };
  int sw_arr[4] = { sw, sw, sw, 0 };
  mrc_fld_set_param_int_array(fld, "dims", 4, dims);
  mrc_fld_set_param_int_array(fld, "sw", 4, sw_arr);
  fld->_domain = domain;
  return fld;
}

// ======================================================================
// mrc_domain_f3_create

struct mrc_f3 *
mrc_domain_f3_create(struct mrc_domain *domain, int sw, const char *comps)
{
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(domain, &nr_patches);
  assert(nr_patches == 1);
  struct mrc_f3 *f3 = mrc_f3_create(mrc_domain_comm(domain));
  if (!comps) {
    comps = "";
  }

  char *s1, *s = strdup(comps), *s_save = s;
  // count first
  int nr_comps = 0;
  while (strsep(&s, ",:")) {
    nr_comps++;
  }
  mrc_f3_set_nr_comps(f3, nr_comps);

  int *ldims = patches[0].ldims;
  mrc_f3_set_param_int_array(f3, "dims", 4,
			     (int [4]) { ldims[0], ldims[1], ldims[2], nr_comps });
  mrc_f3_set_param_int_array(f3, "sw", 4,
			     (int[4]) { sw, sw, sw, 0 });
  
  // parse component names
  s = s_save;
  strcpy(s, comps);
  for (int m = 0; (s1 = strsep(&s, ",:")); m++) {
    mrc_f3_set_comp_name(f3, m, s1);
  }
  free(s_save);

  f3->_domain = domain;


  return f3;
}

// ======================================================================
// mrc_domain_m3_create

struct mrc_m3 *
mrc_domain_m3_create(struct mrc_domain *domain)
{
  struct mrc_m3 *m3 = mrc_m3_create(domain->obj.comm);
  m3->domain = domain;
  return m3;
}

// ======================================================================
// mrc_domain_m1_create

struct mrc_m1 *
mrc_domain_m1_create(struct mrc_domain *domain)
{
  struct mrc_m1 *m1 = mrc_m1_create(domain->obj.comm);
  m1->domain = domain;
  return m1;
}

// ======================================================================
// mrc_domain_init

static void
mrc_domain_init()
{
  mrc_class_register_subclass(&mrc_class_mrc_domain, &mrc_domain_simple_ops);
  mrc_class_register_subclass(&mrc_class_mrc_domain, &mrc_domain_multi_ops);
  mrc_class_register_subclass(&mrc_class_mrc_domain, &mrc_domain_amr_ops);
}

// ======================================================================
// mrc_domain class

struct mrc_class_mrc_domain mrc_class_mrc_domain = {
  .name             = "mrc_domain",
  .size             = sizeof(struct mrc_domain),
  .init             = mrc_domain_init,
  .create           = _mrc_domain_create,
  .destroy          = _mrc_domain_destroy,
  .set_from_options = _mrc_domain_set_from_options,
  .view             = _mrc_domain_view,
  .setup            = _mrc_domain_setup,
  .read             = _mrc_domain_read,
  .write            = _mrc_domain_write,
};

