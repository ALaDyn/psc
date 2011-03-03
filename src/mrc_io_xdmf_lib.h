
#ifndef MRC_IO_XDMF_LIB_H
#define MRC_IO_XDMF_LIB_H

#include <mrc_list.h>
#include <mrc_io.h>

#include <stdbool.h>

#define MAX_XDMF_FLD_INFO (30)

struct xdmf_fld_info {
  char *name;
  char *path;
  bool is_vec;
};

struct xdmf_spatial {
  char *name; //< from domain::name

  bool crds_done;

  int nr_global_patches;
  struct mrc_patch_info *patch_infos;

  int nr_fld_info;
  struct xdmf_fld_info fld_info[MAX_XDMF_FLD_INFO];

  list_t entry; //< on xdmf_file::xdmf_spatial_list
};

struct xdmf_temporal_step {
  list_t entry;
  char filename[0];
};

struct xdmf_temporal {
  char *filename;
  list_t timesteps;
};

struct xdmf_temporal *xdmf_temporal_create(const char *filename);
void xdmf_temporal_destroy(struct xdmf_temporal *xt);
void xdmf_temporal_append(struct xdmf_temporal *xt,
			  const char *fname_spatial);
void xdmf_temporal_write(struct xdmf_temporal *xt);

void xdmf_spatial_open(list_t *xdmf_spatial_list);
void xdmf_spatial_close(list_t *xdmf_spatial_list, struct mrc_io *io,
			struct xdmf_temporal *xt);
struct xdmf_spatial *xdmf_spatial_find(list_t *xdmf_spatial_list,
				       const char *name);
struct xdmf_spatial *xdmf_spatial_create_m3(list_t *xdmf_spatial_list,
					    const char *name, 
					    struct mrc_domain *domain);
void xdmf_spatial_save_fld_info(struct xdmf_spatial *xs, char *fld_name,
				char *path, bool is_vec);

#endif
