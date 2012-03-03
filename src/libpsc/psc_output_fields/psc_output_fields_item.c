
#include "psc_output_fields_item_private.h"

// ----------------------------------------------------------------------
// psc_output_fields_item_set_psc_bnd

void
psc_output_fields_item_set_psc_bnd(struct psc_output_fields_item *item,
				   struct psc_bnd *bnd)
{
  item->bnd = bnd; // FIXME, ref counting?
}

// ----------------------------------------------------------------------
// psc_output_fields_item_create_mfields

mfields_c_t *
psc_output_fields_item_create_mfields(struct psc_output_fields_item *item)
{
  struct psc_output_fields_item_ops *ops = psc_output_fields_item_ops(item);
  mfields_c_t *flds = psc_mfields_create(psc_output_fields_item_comm(item));
  psc_mfields_set_type(flds, "c");
  psc_mfields_set_domain(flds, ppsc->mrc_domain);
  psc_mfields_set_param_int(flds, "nr_fields", ops->nr_comp);
  psc_mfields_set_param_int3(flds, "ibn", ppsc->ibn);
  psc_mfields_setup(flds);
  for (int m = 0; m < ops->nr_comp; m++) {
    flds->name[m] = strdup(ops->fld_names[m]);
  }

  return flds;
}

// ----------------------------------------------------------------------
// psc_output_fields_item_run

void
psc_output_fields_item_run(struct psc_output_fields_item *item,
			   mfields_base_t *flds, mparticles_base_t *particles,
			   mfields_c_t *res)
{
  psc_output_fields_item_ops(item)->run(item, flds, particles, res);
}

// ======================================================================
// psc_output_fields_item_init

static void
psc_output_fields_item_init()
{
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_j_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_j_nc_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_j_ec_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_e_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_e_nc_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_e_ec_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_h_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_h_nc_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_h_fc_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_jdote_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_poyn_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_e2_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_h2_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_densities_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_v_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_vv_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields_item, &psc_output_fields_item_photon_n_ops);
}

// ======================================================================
// psc_output_fields_item class

struct mrc_class_psc_output_fields_item mrc_class_psc_output_fields_item = {
  .name             = "psc_output_fields_item",
  .size             = sizeof(struct psc_output_fields_item),
  .init             = psc_output_fields_item_init,
};

