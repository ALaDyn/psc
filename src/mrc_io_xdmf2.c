
#include <mrc_io_private.h>
#include <mrc_params.h>
#include <mrc_list.h>
#include "mrc_io_xdmf_lib.h"

#include <stdlib.h>
#include <string.h>
#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

// ----------------------------------------------------------------------

struct xdmf_file {
  hid_t h5_file;
  list_t xdmf_spatial_list;
};

struct xdmf {
  struct xdmf_file file;
  struct xdmf_temporal *xdmf_temporal;
  int sw;
  // parallel only
  bool use_independent_io;
};

#define VAR(x) (void *)offsetof(struct xdmf, x)
static struct param xdmf2_descr[] = {
  { "sw"                     , VAR(sw)                      , PARAM_INT(0)           },
  {},
};
#undef VAR

#define VAR(x) (void *)offsetof(struct xdmf, x)
static struct param xdmf_parallel_descr[] = {
  { "use_independent_io"     , VAR(use_independent_io)      , PARAM_BOOL(false)      },
  {},
};
#undef VAR

#define to_xdmf(io) ((struct xdmf *)((io)->obj.subctx))

// ======================================================================
// xdmf

static void
xdmf_setup(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);

  char filename[strlen(io->par.outdir) + strlen(io->par.basename) + 7];
  sprintf(filename, "%s/%s.xdmf", io->par.outdir, io->par.basename);
  xdmf->xdmf_temporal = xdmf_temporal_create(filename);

  mrc_io_setup_super(io);
}

static void
xdmf_destroy(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);
  if (xdmf->xdmf_temporal) {
    xdmf_temporal_destroy(xdmf->xdmf_temporal);
  }
}

// ----------------------------------------------------------------------
// xdmf_open

static void
xdmf_open(struct mrc_io *io, const char *mode)
{
  struct xdmf *xdmf = to_xdmf(io);
  assert(strcmp(mode, "w") == 0);

  struct xdmf_file *file = &xdmf->file;
  char filename[strlen(io->par.outdir) + strlen(io->par.basename) + 20];
  sprintf(filename, "%s/%s.%06d_p%06d.h5", io->par.outdir, io->par.basename,
	  io->step, io->rank);
  file->h5_file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  xdmf_spatial_open(&file->xdmf_spatial_list);
}

static void
xdmf_close(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;

  H5Fclose(file->h5_file);
  xdmf_spatial_close(&file->xdmf_spatial_list, io, xdmf->xdmf_temporal);

  memset(file, 0, sizeof(*file));
}

static void
xdmf_write_attr(struct mrc_io *io, const char *path, int type,
		const char *name, union param_u *pv)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  
  hid_t group;
  if (H5Lexists(file->h5_file, path, H5P_DEFAULT) > 0) {
    group = H5Gopen(file->h5_file, path, H5P_DEFAULT);
  } else {
    group = H5Gcreate(file->h5_file, path, H5P_DEFAULT,
		      H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
  }

  switch (type) {
  case PT_SELECT:
  case PT_INT:
    H5LTset_attribute_int(group, ".", name, &pv->u_int, 1);
    break;
  case PT_BOOL: {
    int val = pv->u_bool;
    H5LTset_attribute_int(group, ".", name, &val, 1);
    break;
  }
  case PT_FLOAT:
    H5LTset_attribute_float(group, ".", name, &pv->u_float, 1);
    break;
  case PT_DOUBLE:
    H5LTset_attribute_double(group, ".", name, &pv->u_double, 1);
    break;
  case PT_STRING:
    H5LTset_attribute_string(group, ".", name, pv->u_string);
    break;
  case PT_INT3:
    H5LTset_attribute_int(group, ".", name, pv->u_int3, 3);
    break;
  case PT_FLOAT3:
    H5LTset_attribute_float(group, ".", name, pv->u_float3, 3);
    break;
  }
  H5Gclose(group);
}

static void
xdmf_spatial_write_mcrds_multi(struct mrc_io *io, struct xdmf_file *file,
			       struct mrc_domain *domain, int sw)
{
  struct mrc_crds *crds = mrc_domain_get_crds(domain);

  for (int d = 0; d < 3; d++) {
    struct mrc_m1 *mcrd = crds->mcrd[d];

    hid_t group_crd1 = H5Gopen(file->h5_file, mrc_io_obj_path(io, mcrd),
			       H5P_DEFAULT); H5_CHK(group_crd1);

    mrc_m1_foreach_patch(mcrd, p) {
      struct mrc_m1_patch *mcrdp = mrc_m1_patch_get(mcrd, p);
      int im = mcrdp->im[0] + 2 * mcrdp->ib[0];
      // get node-centered coordinates
      float *crd_nc = calloc(im + 2*sw + 1, sizeof(*crd_nc));
      if (mcrdp->ib[0] < -sw) {
	for (int i = -sw; i <= im + sw; i++) {
	  crd_nc[i + sw] = .5 * (MRC_M1(mcrdp,0, i-1) + MRC_M1(mcrdp,0, i));
	}
      } else {
	for (int i = 1-sw; i < im+sw; i++) {
	  crd_nc[i + sw] = .5 * (MRC_M1(mcrdp,0, i-1) + MRC_M1(mcrdp,0, i));
	}
	// extrapolate
	crd_nc[0      ] = MRC_M1(mcrdp,0, -sw)     - .5 * (MRC_M1(mcrdp,0, -sw+1)    - MRC_M1(mcrdp,0, -sw));
	crd_nc[im+2*sw] = MRC_M1(mcrdp,0, im+sw-1) + .5 * (MRC_M1(mcrdp,0, im+sw-1) - MRC_M1(mcrdp,0, im+sw-2));
      }
      hsize_t im1 = im + 2*sw + 1;
      char s_patch[10];
      sprintf(s_patch, "p%d", p);
      hid_t group_crdp = H5Gcreate(group_crd1, s_patch, H5P_DEFAULT,
				   H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crdp);
      H5LTmake_dataset_float(group_crdp, "1d", 1, &im1, crd_nc);
      H5Gclose(group_crdp);

      free(crd_nc);
      mrc_m1_patch_put(mcrd);
    }

    H5Gclose(group_crd1);
  }
}

static void
xdmf_spatial_write_mcrds(struct mrc_io *io, struct xdmf_spatial *xs, struct xdmf_file *file,
			 struct mrc_domain *domain, int sw)
{
  if (xs->crds_done)
    return;

  xs->crds_done = true;

  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  if (strcmp(mrc_crds_type(crds), "multi_uniform") == 0) {
    xdmf_spatial_write_mcrds_multi(io, file, domain, sw); // FIXME
  } else {
    xdmf_spatial_write_mcrds_multi(io, file, domain, sw);
  }
}

static void
xdmf_spatial_write_crds_nonuni(struct xdmf_file *file,
			       struct mrc_domain *domain)
{
  struct mrc_crds *crds = mrc_domain_get_crds(domain);

  for (int d = 0; d < 1; d++) {
    struct mrc_f1 *crd = crds->crd[d];

    hid_t group_crd1 = H5Gcreate(file->h5_file, mrc_f1_name(crd), H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crd1);

    const int *dims = mrc_f1_dims(crd);
    hsize_t hdims[1] = { dims[0] };
    char s_patch[10];
    sprintf(s_patch, "p%d", 0);
    hid_t group_crdp = H5Gcreate(group_crd1, s_patch, H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crdp);
    H5LTmake_dataset_float(group_crdp, "1d", 1, hdims, &MRC_F1(crd,0, 0));
    H5Gclose(group_crdp);

    H5Gclose(group_crd1);
  }
}

static void
xdmf_spatial_write_crds(struct xdmf_spatial *xs, struct xdmf_file *file,
			struct mrc_domain *domain)
{
  if (xs->crds_done)
    return;

  xs->crds_done = true;

  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  if (strcmp(mrc_crds_type(crds), "uniform") == 0) {
    xdmf_spatial_write_crds_nonuni(file, domain); // FIXME
  } else {
    xdmf_spatial_write_crds_nonuni(file, domain);
  }
}

static void
xdmf_write_m3(struct mrc_io *io, const char *path, struct mrc_fld *m3)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;

  int sw = xdmf->sw;
  hid_t group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT);
  int nr_patches = mrc_fld_nr_patches(m3);
  H5LTset_attribute_int(group0, ".", "nr_patches", &nr_patches, 1);

  struct xdmf_spatial *xs = xdmf_spatial_find(&file->xdmf_spatial_list,
					      mrc_domain_name(m3->_domain));
  if (!xs) {
    xs = xdmf_spatial_create_m3(&file->xdmf_spatial_list,
				mrc_domain_name(m3->_domain), m3->_domain, io);
    xdmf_spatial_write_mcrds(io, xs, file, m3->_domain, xdmf->sw);
  }

  for (int m = 0; m < mrc_fld_nr_comps(m3); m++) {
    xdmf_spatial_save_fld_info(xs, strdup(mrc_fld_comp_name(m3, m)), strdup(path), false);

    hid_t group_fld = H5Gcreate(group0, mrc_fld_comp_name(m3, m), H5P_DEFAULT,
				H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_fld);
    mrc_fld_foreach_patch(m3, p) {
      struct mrc_fld_patch *m3p = mrc_fld_patch_get(m3, p);

      char s_patch[10];
      sprintf(s_patch, "p%d", p);

      hsize_t mdims[3] = { m3->_ghost_dims[2], m3->_ghost_dims[1], m3->_ghost_dims[0] };
      hsize_t fdims[3] = { m3->_ghost_dims[2] + 2 * m3->_ghost_offs[2] + 2*sw,
			   m3->_ghost_dims[1] + 2 * m3->_ghost_offs[1] + 2*sw,
			   m3->_ghost_dims[0] + 2 * m3->_ghost_offs[0] + 2*sw};
      hsize_t off[3] = { -m3->_ghost_offs[2]-sw, -m3->_ghost_offs[1]-sw, -m3->_ghost_offs[0]-sw };

      hid_t group = H5Gcreate(group_fld, s_patch, H5P_DEFAULT,
			      H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(m3->_domain, p, &info);
      H5LTset_attribute_int(group, ".", "global_patch", &info.global_patch, 1);
      hid_t filespace = H5Screate_simple(3, fdims, NULL);
      hid_t memspace = H5Screate_simple(3, mdims, NULL);
      H5Sselect_hyperslab(memspace, H5S_SELECT_SET, off, NULL, fdims, NULL);

      hid_t dset = H5Dcreate(group, "3d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			     H5P_DEFAULT, H5P_DEFAULT);
      H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT,
	       &MRC_M3(m3p, m, m3->_ghost_offs[0], m3->_ghost_offs[1], m3->_ghost_offs[2]));
      H5Dclose(dset);
      ierr = H5Gclose(group); CE;
      mrc_fld_patch_put(m3);
    }
    H5Gclose(group_fld);
  }

  H5Gclose(group0);
}

static void
xdmf_write_f1(struct mrc_io *io, const char *path, struct mrc_f1 *f1)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;

  hid_t group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT);
  const int i1 = 1, i0 = 0;
  H5LTset_attribute_int(group0, ".", "nr_patches", &i1, 1);

  struct xdmf_spatial *xs = xdmf_spatial_find(&file->xdmf_spatial_list,
					      mrc_domain_name(f1->domain));
  if (!xs) {
    xs = xdmf_spatial_create_f1(&file->xdmf_spatial_list,
				mrc_domain_name(f1->domain), f1->domain);
    xdmf_spatial_write_crds(xs, file, f1->domain);
  }

  for (int m = 0; m < f1->nr_comp; m++) {
    const char *fld_name = mrc_f1_comp_name(f1, m);
    if (!fld_name) {
      char tmp_fld_name[10];
      fld_name = tmp_fld_name;
      // FIXME: warn
      MHERE;
      sprintf(tmp_fld_name, "m%d", m);
    }
    xdmf_spatial_save_fld_info(xs, strdup(fld_name), strdup(path), false);

    hid_t group_fld = H5Gcreate(group0, fld_name, H5P_DEFAULT,
				H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_fld);
    char s_patch[10];
    sprintf(s_patch, "p%d", 0);

    const int *dims = mrc_f1_dims(f1);
    hsize_t mdims[1] = { dims[0] };
    hsize_t fdims[1] = { dims[0] };
    hsize_t off[1] = { 0 };
    
    hid_t group = H5Gcreate(group_fld, s_patch, H5P_DEFAULT,
			    H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
    H5LTset_attribute_int(group, ".", "global_patch", &i0, 1);
    hid_t filespace = H5Screate_simple(1, fdims, NULL);
    hid_t memspace = H5Screate_simple(1, mdims, NULL);
    H5Sselect_hyperslab(memspace, H5S_SELECT_SET, off, NULL, fdims, NULL);
    
    hid_t dset = H5Dcreate(group, "1d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			   H5P_DEFAULT, H5P_DEFAULT);
    H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT,
	     &MRC_F1(f1, m, 0));
    H5Dclose(dset);
    H5Gclose(group);

    H5Gclose(group_fld);
  }
  H5Gclose(group0);
}


// ----------------------------------------------------------------------
// mrc_io_ops_xdmf

struct mrc_io_ops mrc_io_xdmf2_ops = {
  .name          = "xdmf2",
  .size          = sizeof(struct xdmf),
  .param_descr   = xdmf2_descr,
  .parallel      = true,
  .setup         = xdmf_setup,
  .destroy       = xdmf_destroy,
  .open          = xdmf_open,
  .close         = xdmf_close,
  .write_attr    = xdmf_write_attr,
  .write_m3      = xdmf_write_m3,
  .write_f1      = xdmf_write_f1,
};


// ======================================================================

#ifdef H5_HAVE_PARALLEL

// ----------------------------------------------------------------------
// xdmf_parallel_open

static void
xdmf_parallel_open(struct mrc_io *io, const char *mode)
{
  struct xdmf *xdmf = to_xdmf(io);
  assert(strcmp(mode, "w") == 0);

  struct xdmf_file *file = &xdmf->file;
  char filename[strlen(io->par.outdir) + strlen(io->par.basename) + 20];
  sprintf(filename, "%s/%s.%06d_p%06d.h5", io->par.outdir, io->par.basename,
	  io->step, 0);

  hid_t plist = H5Pcreate(H5P_FILE_ACCESS);
#ifdef H5_HAVE_PARALLEL
  H5Pset_fapl_mpio(plist, io->obj.comm, MPI_INFO_NULL);
#endif
  file->h5_file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, plist);
  H5Pclose(plist);

  xdmf_spatial_open(&file->xdmf_spatial_list);
}

// ----------------------------------------------------------------------
// xdmf_parallel_close

static void
xdmf_parallel_close(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;

  H5Fclose(file->h5_file);
  xdmf_spatial_close(&file->xdmf_spatial_list, io, xdmf->xdmf_temporal);

  memset(file, 0, sizeof(*file));
}

// ----------------------------------------------------------------------
// xdmf_spatial_write_mcrds_multi_parallel

static void
xdmf_spatial_write_mcrds_multi_parallel(struct xdmf_file *file,
					struct mrc_domain *domain)
{
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  int gdims[3];
  mrc_domain_get_global_dims(domain, gdims);

  for (int d = 0; d < 3; d++) {
    struct mrc_m1 *mcrd = crds->mcrd[d];

    hid_t group_crd1 = H5Gcreate(file->h5_file, mrc_m1_name(mcrd), H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crd1);

    hid_t group_crdp = H5Gcreate(group_crd1, "p0", H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crdp);
    
    hsize_t fdims[1] = { gdims[d] + 1 };
    hid_t filespace = H5Screate_simple(1, fdims, NULL);
    hid_t dset = H5Dcreate(group_crdp, "1d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			   H5P_DEFAULT, H5P_DEFAULT);

    mrc_m1_foreach_patch(mcrd, p) {
      struct mrc_m1_patch *mcrdp = mrc_m1_patch_get(mcrd, p);
      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(domain, p, &info);
      bool skip_write = false;
      for (int dd = 0; dd < 3; dd++) {
	if (d != dd && info.off[dd] != 0) {
	  skip_write = true;
	  break;
	}
      }
      // indep I/O only!
      // FIXME, do collective ?
      if (skip_write) {
	continue;
      }

      // get node-centered coordinates
      int im = info.ldims[d];
      float *crd_nc = calloc(im + 1, sizeof(*crd_nc));
      if (mcrdp->ib[0] < 0) {
	for (int i = 0; i <= im; i++) {
	  crd_nc[i] = .5 * (MRC_M1(mcrdp,0, i-1) + MRC_M1(mcrdp,0, i));
	}
      } else {
	for (int i = 1; i < im; i++) {
	  crd_nc[i] = .5 * (MRC_M1(mcrdp,0, i-1) + MRC_M1(mcrdp,0, i));
	}
	// extrapolate
	crd_nc[0]  = MRC_M1(mcrdp,0, 0) - .5 * (MRC_M1(mcrdp,0, 1) - MRC_M1(mcrdp,0, 0));
	crd_nc[im] = MRC_M1(mcrdp,0, im-1) + .5 * (MRC_M1(mcrdp,0, im-1) - MRC_M1(mcrdp,0, im-2));
      }

      hsize_t mdims[1] = { info.ldims[d] + (info.off[d] == 0 ? 1 : 0) };
      hsize_t off[1] = { info.off[d] + (info.off[d] == 0 ? 0 : 1) };
      hid_t memspace = H5Screate_simple(1, mdims, NULL);
      H5Sselect_hyperslab(filespace, H5S_SELECT_SET, off, NULL, mdims, NULL);

      H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT,
	       &crd_nc[(info.off[d] == 0) ? 0 : 1]);

      H5Sclose(memspace);
    }
    
    H5Dclose(dset);
    H5Sclose(filespace);

    H5Gclose(group_crdp);
    H5Gclose(group_crd1);
  }
}

// ----------------------------------------------------------------------
// xdmf_spatial_write_crds_multi_parallel

static void
xdmf_spatial_write_crds_multi_parallel(struct mrc_io *io, struct mrc_domain *domain)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  int gdims[3], np[3], nr_global_patches, nr_patches;
  mrc_domain_get_global_dims(domain, gdims);
  mrc_domain_get_nr_procs(domain, np);
  mrc_domain_get_nr_global_patches(domain, &nr_global_patches);
  mrc_domain_get_patches(domain, &nr_patches);

  for (int d = 0; d < 3; d++) {
    struct mrc_f1 *crd = crds->crd[d];

    MPI_Request *send_reqs = calloc(nr_patches, sizeof(*send_reqs));
    int nr_send_reqs = 0;
    assert(nr_patches == 1); // otherwise need to redo tmp_nc
    float *tmp_nc = NULL;
    for (int p = 0; p < nr_patches; p++) {
      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(domain, p, &info);
      bool skip_write = false;
      for (int dd = 0; dd < 3; dd++) {
	if (d != dd && info.off[dd] != 0) {
	  skip_write = true;
	  break;
	}
      }
      if (!skip_write) {
	tmp_nc = calloc(info.ldims[d] + 1, sizeof(*tmp_nc));

	// get node-centered coordinates
	if (crd->_sw > 0) {
	  for (int i = 0; i <= info.ldims[d]; i++) {
	    tmp_nc[i] = .5 * (MRC_F1(crd,0, i-1) + MRC_F1(crd,0, i));
	  }
	} else {
	  int ld = info.ldims[d];
	  for (int i = 1; i < ld; i++) {
	    tmp_nc[i] = .5 * (MRC_F1(crd,0, i-1) + MRC_F1(crd,0, i));
	  }
	  // extrapolate
	  tmp_nc[0]  = MRC_F1(crd,0, 0) - .5 * (MRC_F1(crd,0, 1) - MRC_F1(crd,0, 0));
	  tmp_nc[ld] = MRC_F1(crd,0, ld-1) + .5 * (MRC_F1(crd,0, ld-1) - MRC_F1(crd,0, ld-2));
	}

 	/* mprintf("Isend off %d %d %d gp %d\n", info.off[0], info.off[1], info.off[2], */
	/* 	info.global_patch); */
	MPI_Isend(tmp_nc + (info.off[d] == 0 ? 0 : 1),
		  info.ldims[d] + (info.off[d] == 0 ? 1 : 0), MPI_FLOAT,
		  0, info.global_patch,
		  mrc_io_comm(io), &send_reqs[nr_send_reqs++]);
      }
    }

    int im = gdims[d];
    float *crd_nc = NULL;

    if (io->rank == 0) { // only on first writer
      crd_nc = calloc(im + 1, sizeof(*crd_nc));
      MPI_Request *recv_reqs = calloc(np[d], sizeof(*recv_reqs));
      int nr_recv_reqs = 0;
      for (int gp = 0; gp < nr_global_patches; gp++) {
	struct mrc_patch_info info;
	mrc_domain_get_global_patch_info(domain, gp, &info);
	bool skip_write = false;
	for (int dd = 0; dd < 3; dd++) {
	  if (d != dd && info.off[dd] != 0) {
	    skip_write = true;
	    break;
	  }
	}
	if (skip_write) {
	  continue;
	}
	/* mprintf("Irecv off %d %d %d gp %d\n", info.off[0], info.off[1], info.off[2], gp); */
	MPI_Irecv(&crd_nc[info.off[d] + (info.off[d] == 0 ? 0 : 1)],
		  info.ldims[d] + (info.off[d] == 0 ? 1 : 0), MPI_FLOAT, info.rank,
		  gp, mrc_io_comm(io), &recv_reqs[nr_recv_reqs++]);
      }
      assert(nr_recv_reqs == np[d]);

      MPI_Waitall(nr_recv_reqs, recv_reqs, MPI_STATUSES_IGNORE);
      free(recv_reqs);
    }

    MPI_Waitall(nr_send_reqs, send_reqs, MPI_STATUSES_IGNORE);
    free(tmp_nc);
    free(send_reqs);

    // this group has been generated by generic mrc_obj code
    hid_t group_crd1 = H5Gopen(file->h5_file, mrc_f1_name(crd),
			       H5P_DEFAULT); H5_CHK(group_crd1);

    hid_t group_crdp = H5Gcreate(group_crd1, "p0", H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crd1);
    
    hsize_t fdims[1] = { gdims[d] + 1 };
    hid_t filespace = H5Screate_simple(1, fdims, NULL);
    hid_t dset = H5Dcreate(group_crdp, "1d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			   H5P_DEFAULT, H5P_DEFAULT);

    hid_t memspace;
    if (io->rank == 0) {
      memspace = H5Screate_simple(1, fdims, NULL);
    } else {
      memspace = H5Screate(H5S_NULL);
      H5Sselect_none(memspace);
      H5Sselect_none(filespace);
    }
    
    H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, crd_nc);
    
    H5Sclose(memspace);
    
    H5Dclose(dset);
    H5Sclose(filespace);

    H5Gclose(group_crdp);
    H5Gclose(group_crd1);

    free(crd_nc);
  }
}

// ----------------------------------------------------------------------
// xdmf_spatial_write_mcrds_multi_uniform_parallel

static void
xdmf_spatial_write_mcrds_multi_uniform_parallel(struct xdmf_file *file,
						struct mrc_domain *domain)
{
  struct mrc_crds *crds = mrc_domain_get_crds(domain);

  float xl[3], xh[3];
  mrc_crds_get_xl_xh(crds, xl, xh);

  for (int d = 0; d < 3; d++) {
    struct mrc_m1 *mcrd = crds->mcrd[d];

    hid_t group_crd1 = H5Gcreate(file->h5_file, mrc_m1_name(mcrd), H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crd1);

    hid_t group_crdp = H5Gcreate(group_crd1, "p0", H5P_DEFAULT,
				 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_crd1);
    H5LTset_attribute_float(group_crdp, ".", "xl", &xl[d], 1);
    H5LTset_attribute_float(group_crdp, ".", "xh", &xh[d], 1);
    H5Gclose(group_crdp);
    H5Gclose(group_crd1);
  }
}

// ----------------------------------------------------------------------
// xdmf_spatial_write_mcrds_parallel

static void
xdmf_spatial_write_mcrds_parallel(struct xdmf_spatial *xs,
				  struct mrc_io *io,
				  struct mrc_domain *domain)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  if (xs->crds_done)
    return;

  xs->crds_done = true;

  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  if (strcmp(mrc_crds_type(crds), "multi_uniform") == 0) {
    xdmf_spatial_write_mcrds_multi_uniform_parallel(file, domain);
  } else if (strcmp(mrc_crds_type(crds), "multi") == 0) {
    // FIXME, broken since not all are writers
    assert(0);
    xdmf_spatial_write_mcrds_multi_parallel(file, domain);
  } else if (strcmp(mrc_crds_type(crds), "uniform") == 0) {
    // FIXME, should do XDMF uniform, or rather just use m1, not f1
    xdmf_spatial_write_crds_multi_parallel(io, domain);
  } else if (strcmp(mrc_crds_type(crds), "rectilinear") == 0) {
    // FIXME, should do XDMF uniform, or rather just use m1, not f1
    xdmf_spatial_write_crds_multi_parallel(io, domain);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// xdmf_parallel_write_m3

static void
xdmf_parallel_write_m3(struct mrc_io *io, const char *path, struct mrc_fld *m3)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;

  hid_t group0;
  if (H5Lexists(file->h5_file, path, H5P_DEFAULT) > 0) {
    group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT);
    MHERE;
  } else {
    group0 = H5Gcreate(file->h5_file, path, H5P_DEFAULT,
		       H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group0);
  }
  int nr_1 = 1;
  H5LTset_attribute_int(group0, ".", "nr_patches", &nr_1, 1);

  struct xdmf_spatial *xs = xdmf_spatial_find(&file->xdmf_spatial_list,
					      mrc_domain_name(m3->_domain));
  int gdims[3];
  mrc_domain_get_global_dims(m3->_domain, gdims);

  if (!xs) {
    int off[3] = {};
    xs = xdmf_spatial_create_m3_parallel(&file->xdmf_spatial_list,
					 mrc_domain_name(m3->_domain),
					 m3->_domain, off, gdims, io);
    xdmf_spatial_write_mcrds_parallel(xs, io, m3->_domain);
  }

  int nr_patches;
  mrc_domain_get_patches(m3->_domain, &nr_patches);
  int nr_patches_max;
  // FIXME, mrc_domain may know / cache
  MPI_Allreduce(&nr_patches, &nr_patches_max, 1, MPI_INT, MPI_MAX,
		mrc_domain_comm(m3->_domain));

  for (int m = 0; m < mrc_fld_nr_comps(m3); m++) {
    xdmf_spatial_save_fld_info(xs, strdup(mrc_fld_comp_name(m3, m)), strdup(path), false);

    hid_t group_fld = H5Gcreate(group0, mrc_fld_comp_name(m3, m), H5P_DEFAULT,
				H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_fld);
    hid_t group = H5Gcreate(group_fld, "p0", H5P_DEFAULT,
			    H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
    int i0 = 0;
    H5LTset_attribute_int(group, ".", "global_patch", &i0, 1);

    hsize_t fdims[3] = { gdims[2], gdims[1], gdims[0] };
    hid_t filespace = H5Screate_simple(3, fdims, NULL);
    hid_t dset = H5Dcreate(group, "3d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			   H5P_DEFAULT, H5P_DEFAULT);
    hid_t dxpl = H5Pcreate(H5P_DATASET_XFER);
#ifdef H5_HAVE_PARALLEL
    if (xdmf->use_independent_io) {
      H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT);
    } else {
      H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE);
    }
#endif
    for (int p = 0; p < nr_patches_max; p++) {
      if (p >= nr_patches) {
	if (xdmf->use_independent_io)
	  continue;

	// for collective I/O write nothing if no patch left,
	// but still call H5Dwrite()
	H5Sselect_none(filespace);
	hid_t memspace = H5Screate(H5S_NULL);
	H5Sselect_none(memspace);
	H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, dxpl, NULL);
	H5Sclose(memspace);
	continue;
      }
      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(m3->_domain, p, &info);
      struct mrc_fld_patch *m3p = mrc_fld_patch_get(m3, p);

      hsize_t mdims[3] = { m3->_ghost_dims[2], m3->_ghost_dims[1], m3->_ghost_dims[0] };
      hsize_t mcount[3] = { info.ldims[2], info.ldims[1], info.ldims[0] };
      hsize_t moff[3] = { -m3->_ghost_offs[2], -m3->_ghost_offs[1], -m3->_ghost_offs[0] };
      hsize_t foff[3] = { info.off[2], info.off[1], info.off[0] };

      H5Sselect_hyperslab(filespace, H5S_SELECT_SET, foff, NULL, mcount, NULL);
      hid_t memspace = H5Screate_simple(3, mdims, NULL);
      H5Sselect_hyperslab(memspace, H5S_SELECT_SET, moff, NULL, mcount, NULL);

      H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, dxpl,
	       &MRC_M3(m3p, m, m3->_ghost_offs[0], m3->_ghost_offs[1], m3->_ghost_offs[2]));

      H5Sclose(memspace);

      mrc_fld_patch_put(m3);
    }
    H5Dclose(dset);
    H5Sclose(filespace);
    H5Pclose(dxpl);

    H5Gclose(group);
    H5Gclose(group_fld);
  }
  H5Gclose(group0);
}


// ----------------------------------------------------------------------
// mrc_io_ops_xdmf_parallel

struct mrc_io_ops mrc_io_xdmf2_parallel_ops = {
  .name          = "xdmf2_parallel",
  .size          = sizeof(struct xdmf),
  .param_descr   = xdmf_parallel_descr,
  .parallel      = true,
  .setup         = xdmf_setup,
  .destroy       = xdmf_destroy,
  .open          = xdmf_parallel_open,
  .close         = xdmf_parallel_close,
#if 0
  .write_attr    = xdmf_write_attr,
#endif
  .write_m3      = xdmf_parallel_write_m3,
};

#endif
