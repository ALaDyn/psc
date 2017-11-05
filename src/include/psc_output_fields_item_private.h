
#ifndef PSC_OUTPUT_FIELDS_ITEM_PRIVATE_H
#define PSC_OUTPUT_FIELDS_ITEM_PRIVATE_H

#include <psc_output_fields_item.h>

struct psc_output_fields_item {
  struct mrc_obj obj;
  struct psc_bnd *bnd;
};

enum {
  POFI_ADD_GHOSTS = 1, // this item needs to have ghost points added to interior points
  POFI_BY_KIND    = 2, // this item needs to be replicated by kind
};

#define POFI_MAX_COMPS (10)

struct psc_output_fields_item_ops {
  MRC_SUBCLASS_OPS(struct psc_output_fields_item);
  void (*run_all)(struct psc_output_fields_item *item,
		  struct psc_mfields *mflds, struct psc_mparticles *mprts,
		  struct psc_mfields *mres);
  int nr_comp;
  char *fld_names[POFI_MAX_COMPS];
  unsigned int flags;
};

#define psc_output_fields_item_ops(item)			\
  ((struct psc_output_fields_item_ops *)((item)->obj.ops))

// ======================================================================

extern struct psc_output_fields_item_ops psc_output_fields_item_j_nc_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_j_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_j_ec_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_e_nc_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_e_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_e_ec_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_h_nc_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_h_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_h_fc_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_jdote_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_poyn_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_e2_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_h2_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_divb_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_divj_ops;

extern struct psc_output_fields_item_ops psc_output_fields_item_n_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_p_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_T_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_v_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_vv_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_Tvv_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_nvt_1st_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_nvp_1st_single_ops;

extern struct psc_output_fields_item_ops psc_output_fields_item_n_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_p_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_T_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_v_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_vv_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_Tvv_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_nvt_1st_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_nvp_1st_double_ops;


extern struct psc_output_fields_item_ops psc_output_fields_item_n_1st_nc_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_n_1st_nc_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_n_2nd_nc_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_rho_1st_nc_single_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_rho_1st_nc_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_rho_2nd_nc_double_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_n_photon_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_coll_stats_single_ops;

extern struct psc_output_fields_item_ops psc_output_fields_item_dive_c_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_dive_single_ops;

extern struct psc_output_fields_item_ops psc_output_fields_item_dive_cuda_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_rho_1st_nc_cuda_ops;
extern struct psc_output_fields_item_ops psc_output_fields_item_n_1st_cuda_ops;

#endif
