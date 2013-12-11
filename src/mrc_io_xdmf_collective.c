
#include <mrc_io_private.h>
#include <mrc_params.h>
#include "mrc_io_xdmf_lib.h"

#include <hdf5.h>
#include <hdf5_hl.h>
#include <stdlib.h>
#include <string.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

struct xdmf_file {
  hid_t h5_file;
  list_t xdmf_spatial_list;
};

struct xdmf {
  int slab_dims[3];
  int slab_off[3];
  struct xdmf_file file;
  struct xdmf_temporal *xdmf_temporal;
  bool use_independent_io;
  char *romio_cb_write;
  char *romio_ds_write;
  int nr_writers;
  MPI_Comm comm_writers; //< communicator for only the writers
  int *writers;          //< rank (in mrc_io comm) for each writer
  int is_writer;         //< this rank is a writer
  char *mode;            //< open mode, "r" or "w"
};

#define VAR(x) (void *)offsetof(struct xdmf, x)
static struct param xdmf_collective_descr[] = {
  { "use_independent_io"     , VAR(use_independent_io)      , PARAM_BOOL(false)      },
  { "nr_writers"             , VAR(nr_writers)              , PARAM_INT(1)           },
  { "romio_cb_write"         , VAR(romio_cb_write)          , PARAM_STRING(NULL)     },
  { "romio_ds_write"         , VAR(romio_ds_write)          , PARAM_STRING(NULL)     },
  { "slab_dims"              , VAR(slab_dims)               , PARAM_INT3(0, 0, 0)    },
  { "slab_off"               , VAR(slab_off)                , PARAM_INT3(0, 0, 0)    },
  {},
};
#undef VAR

#define to_xdmf(io) mrc_to_subobj(io, struct xdmf)

// ----------------------------------------------------------------------
// xdmf_collective_setup

static void
xdmf_collective_setup(struct mrc_io *io)
{
  mrc_io_setup_super(io);

  struct xdmf *xdmf = to_xdmf(io);

  char filename[strlen(io->par.outdir) + strlen(io->par.basename) + 7];
  sprintf(filename, "%s/%s.xdmf", io->par.outdir, io->par.basename);
  xdmf->xdmf_temporal = xdmf_temporal_create(filename);

#ifndef H5_HAVE_PARALLEL
  assert(xdmf->nr_writers == 1);
#endif
  
  if (xdmf->nr_writers > io->size) {
    xdmf->nr_writers = io->size;
  }
  xdmf->writers = calloc(xdmf->nr_writers, sizeof(*xdmf->writers));
  // setup writers, just use first nr_writers ranks,
  // could do something fancier in the future
  for (int i = 0; i < xdmf->nr_writers; i++) {
    xdmf->writers[i] = i;
    if (i == io->rank)
      xdmf->is_writer = 1;
  }
  MPI_Comm_split(mrc_io_comm(io), xdmf->is_writer, io->rank, &xdmf->comm_writers);
}

// ----------------------------------------------------------------------
// xdmf_collective_destroy

static void
xdmf_collective_destroy(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);
  
  free(xdmf->writers);
  if (xdmf->comm_writers) {
    MPI_Comm_free(&xdmf->comm_writers);
  }

  if (xdmf->xdmf_temporal) {
    xdmf_temporal_destroy(xdmf->xdmf_temporal);
  }
}

// ----------------------------------------------------------------------
// xdmf_collective_open

static void
xdmf_collective_open(struct mrc_io *io, const char *mode)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  xdmf->mode = strdup(mode);
  //  assert(strcmp(mode, "w") == 0);

  char filename[strlen(io->par.outdir) + strlen(io->par.basename) + 20];
  sprintf(filename, "%s/%s.%06d_p%06d.h5", io->par.outdir, io->par.basename,
	  io->step, 0);

  if (xdmf->is_writer) {
    hid_t plist = H5Pcreate(H5P_FILE_ACCESS);
    MPI_Info info;
    MPI_Info_create(&info);
    if (xdmf->romio_cb_write) {
      MPI_Info_set(info, "romio_cb_write", xdmf->romio_cb_write);
    }
    if (xdmf->romio_ds_write) {
      MPI_Info_set(info, "romio_ds_write", xdmf->romio_ds_write);
    }
#ifdef H5_HAVE_PARALLEL
    H5Pset_fapl_mpio(plist, xdmf->comm_writers, info);
#endif
    if (strcmp(mode, "w") == 0) {
      file->h5_file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, plist);
    } else if (strcmp(mode, "r") == 0) {
      file->h5_file = H5Fopen(filename, H5F_ACC_RDONLY, plist);
    } else {
      assert(0);
    }
    H5Pclose(plist);
    MPI_Info_free(&info);
  }
  xdmf_spatial_open(&file->xdmf_spatial_list);
}

// ----------------------------------------------------------------------
// xdmf_collective_close

static void
xdmf_collective_close(struct mrc_io *io)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;

  xdmf_spatial_close(&file->xdmf_spatial_list, io, xdmf->xdmf_temporal);
  if (xdmf->is_writer) {
    H5Fclose(file->h5_file);
    memset(file, 0, sizeof(*file));
  }
  free(xdmf->mode);
  xdmf->mode = NULL;
}

static void
xdmf_collective_write_attr(struct mrc_io *io, const char *path, int type,
		const char *name, union param_u *pv)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;
  
  if (!xdmf->is_writer) {
    // FIXME? should check whether the attribute is the same on every proc?
    return;
  }
  hid_t group;
  if (H5Lexists(file->h5_file, path, H5P_DEFAULT) > 0) {
    group = H5Gopen(file->h5_file, path, H5P_DEFAULT); H5_CHK(group);
  } else {
    group = H5Gcreate(file->h5_file, path, H5P_DEFAULT, H5P_DEFAULT,
		      H5P_DEFAULT); H5_CHK(group);
  }

  switch (type) {
  case PT_SELECT:
  case PT_INT:
    ierr = H5LTset_attribute_int(group, ".", name, &pv->u_int, 1); CE;
    break;
  case PT_BOOL: {
    int val = pv->u_bool;
    ierr = H5LTset_attribute_int(group, ".", name, &val, 1); CE;
    break;
  }
  case PT_FLOAT:
    ierr = H5LTset_attribute_float(group, ".", name, &pv->u_float, 1); CE;
    break;
  case PT_DOUBLE:
    ierr = H5LTset_attribute_double(group, ".", name, &pv->u_double, 1); CE;
    break;
  case PT_STRING:
    ierr = H5LTset_attribute_string(group, ".", name, pv->u_string); CE;
    break;
  case PT_INT3:
    ierr = H5LTset_attribute_int(group, ".", name, pv->u_int3, 3); CE;
    break;
  case PT_FLOAT3:
    ierr = H5LTset_attribute_float(group, ".", name, pv->u_float3, 3); CE;
    break;
  case PT_INT_ARRAY:
    ierr = H5LTset_attribute_int(group, ".", name, pv->u_int_array.vals,
				 pv->u_int_array.nr_vals); CE;
    break;
  }
  ierr = H5Gclose(group); CE;
}

static void
xdmf_collective_read_attr(struct mrc_io *io, const char *path, int type,
			  const char *name, union param_u *pv)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;

  // read on I/O procs
  if (xdmf->is_writer) {
    hid_t group = H5Gopen(file->h5_file, path, H5P_DEFAULT); H5_CHK(group);
    switch (type) {
    case PT_SELECT:
    case PT_INT:
      ierr = H5LTget_attribute_int(group, ".", name, &pv->u_int); CE;
      break;
    case PT_BOOL: ;
      int val;
      ierr = H5LTget_attribute_int(group, ".", name, &val); CE;
      pv->u_bool = val;
      break;
    case PT_FLOAT:
      ierr = H5LTget_attribute_float(group, ".", name, &pv->u_float); CE;
      break;
    case PT_DOUBLE:
      ierr = H5LTget_attribute_double(group, ".", name, &pv->u_double); CE;
      break;
    case PT_STRING: ;
      hsize_t dims;
      H5T_class_t class;
      size_t sz;
      ierr = H5LTget_attribute_info(group, ".", name, &dims, &class, &sz); CE;
      pv->u_string = malloc(sz);
      ierr = H5LTget_attribute_string(group, ".", name, (char *)pv->u_string); CE;
      break;
    case PT_INT3:
      ierr = H5LTget_attribute_int(group, ".", name, pv->u_int3); CE;
      break;
    case PT_FLOAT3:
      ierr = H5LTget_attribute_float(group, ".", name, pv->u_float3); CE;
      break;
    case PT_INT_ARRAY: {
      int attr = H5Aopen(group, name, H5P_DEFAULT); H5_CHK(attr);
      H5A_info_t ainfo;
      ierr = H5Aget_info(attr, &ainfo); CE;
      ierr = H5Aclose(attr); CE;
      pv->u_int_array.nr_vals = ainfo.data_size / sizeof(int);
      pv->u_int_array.vals = calloc(pv->u_int_array.nr_vals, sizeof(int));
      ierr = H5LTget_attribute_int(group, ".", name, pv->u_int_array.vals); CE;
      break;
    }
    }
    ierr = H5Gclose(group); CE;
  }

  int root = xdmf->writers[0];
  MPI_Comm comm = mrc_io_comm(io);
  switch (type) {
  case PT_SELECT:
  case PT_INT:
    MPI_Bcast(&pv->u_int, 1, MPI_INT, root, comm);
    break;
  case PT_BOOL: ;
    int val = pv->u_int;
    MPI_Bcast(&val, 1, MPI_INT, root, comm);
    pv->u_int = val;
    break;
  case PT_FLOAT:
    MPI_Bcast(&pv->u_float, 1, MPI_FLOAT, root, comm);
    break;
  case PT_DOUBLE:
    MPI_Bcast(&pv->u_double, 1, MPI_DOUBLE, root, comm);
    break;
  case PT_STRING: ;
    int len;
    if (io->rank == root) {
      len = strlen(pv->u_string);
    }
    MPI_Bcast(&len, 1, MPI_INT, root, comm);
    if (io->rank != root) {
      pv->u_string = malloc(len + 1);
    }
    // FIXME, u_string type should not be const
    MPI_Bcast((char *) pv->u_string, len + 1, MPI_CHAR, root, comm);
    break;
  case PT_INT3:
    MPI_Bcast(pv->u_int3, 3, MPI_INT, root, comm);
    break;
  case PT_FLOAT3:
    MPI_Bcast(pv->u_float3, 3, MPI_FLOAT, root, comm);
    break;
  }
}

// ======================================================================

// ----------------------------------------------------------------------
// collective_m1_write_f1
// does the actual write of the f1 to the file
// only called on writer procs

static void
collective_m1_write_f1(struct mrc_io *io, const char *path, struct mrc_f1 *f1,
		       int m, hid_t group0)
{
  struct xdmf *xdmf = to_xdmf(io);
  int ierr;

  hid_t group_fld = H5Gcreate(group0, mrc_f1_comp_name(f1, m), H5P_DEFAULT,
			      H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_fld);
  ierr = H5LTset_attribute_int(group_fld, ".", "m", &m, 1); CE;
  
  hid_t group = H5Gcreate(group_fld, "p0", H5P_DEFAULT,
			  H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
  int i0 = 0;
  ierr = H5LTset_attribute_int(group, ".", "global_patch", &i0, 1); CE;

  hsize_t fdims[1] = { mrc_f1_ghost_dims(f1)[0] };
  hid_t filespace = H5Screate_simple(1, fdims, NULL); H5_CHK(filespace);
  hid_t dset = H5Dcreate(group, "1d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(dset);
  hid_t dxpl = H5Pcreate(H5P_DATASET_XFER); H5_CHK(dxpl); // FIXME, consolidate
#ifdef H5_HAVE_PARALLEL
  if (xdmf->use_independent_io) {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT); CE;
  } else {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE); CE;
  }
#endif
  hid_t memspace;
  if (io->rank == xdmf->writers[0]) {
    memspace = H5Screate_simple(1, fdims, NULL);
  } else {
    memspace = H5Screate(H5S_NULL);
    H5Sselect_none(memspace);
    H5Sselect_none(filespace);
  }
  ierr = H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, dxpl, f1->arr); CE;
  
  ierr = H5Dclose(dset); CE;
  ierr = H5Sclose(memspace); CE;
  ierr = H5Sclose(filespace); CE;
  ierr = H5Pclose(dxpl); CE;

  ierr = H5Gclose(group); CE;
  ierr = H5Gclose(group_fld); CE;
}

struct collective_m1_ctx {
  int nr_patches;
  int nr_global_patches;
  int dim;
  int sw;
  int gdims[3];
  int np[3];
  MPI_Request *send_reqs;
  int nr_send_reqs;
  MPI_Request *recv_reqs;
  int nr_recv_reqs;
  char comp_name[100];
};

static void
collective_m1_send_begin(struct mrc_io *io, struct collective_m1_ctx *ctx,
			 struct mrc_m1 *m1, int m)
{
  struct xdmf *xdmf = to_xdmf(io);
  int dim = ctx->dim;

  ctx->send_reqs = calloc(ctx->nr_patches, sizeof(*ctx->send_reqs));
  ctx->nr_send_reqs = 0;

  for (int p = 0; p < ctx->nr_patches; p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(m1->domain, p, &info);
    bool skip = false;
    for (int d = 0; d < 3; d++) {
      if (d != dim && info.off[d] != 0) {
	skip = true;
      }
    }
    if (skip) {
      continue;
    }
    
    struct mrc_m1_patch *m1p = mrc_m1_patch_get(m1, p);
    // FIXME, should use intersection, probably won't work if slab_dims are actually smaller
    int ib = 0;
    if (info.off[dim] == 0) { // FIXME, -> generic code
      ib = xdmf->slab_off[0];
    }
    int ie = info.ldims[dim];
    if (info.off[dim] + info.ldims[dim] == ctx->gdims[dim]) {
      ie = xdmf->slab_off[0] + xdmf->slab_dims[0] - info.off[dim];
    }
    //mprintf("send to %d tag %d len %d\n", xdmf->writers[0], info.global_patch, ie - ib);
    assert(ib < ie);
    MPI_Isend(&MRC_M1(m1p, m, ib), ie - ib, MPI_FLOAT,
	      xdmf->writers[0], info.global_patch, mrc_io_comm(io),
	      &ctx->send_reqs[ctx->nr_send_reqs++]);
    mrc_m1_patch_put(m1);
  }
}

static void
collective_m1_send_end(struct mrc_io *io, struct collective_m1_ctx *ctx)
{
  MPI_Waitall(ctx->nr_send_reqs, ctx->send_reqs, MPI_STATUSES_IGNORE);
  free(ctx->send_reqs);
}

static void
collective_m1_recv_begin(struct mrc_io *io, struct collective_m1_ctx *ctx,
			 struct mrc_domain *domain, struct mrc_f1 *f1, int m)
{
  struct xdmf *xdmf = to_xdmf(io);

  if (io->rank != xdmf->writers[0])
    return;

  int dim = ctx->dim;

  ctx->recv_reqs = calloc(ctx->np[dim], sizeof(*ctx->recv_reqs));
  ctx->nr_recv_reqs = 0;

  for (int gp = 0; gp < ctx->nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(domain, gp, &info);
    bool skip = false;
    for (int d = 0; d < 3; d++) {
      if (d != dim && info.off[d] != 0) {
	skip = true;
      }
    }
    if (skip) {
      continue;
    }
    
    int ib = info.off[dim];
    if (ib == 0) {
      ib = xdmf->slab_off[0];
    }
    int ie = info.off[dim] + info.ldims[dim];
    if (ie == ctx->gdims[dim]) {
      ie = xdmf->slab_off[0] + xdmf->slab_dims[0];
    }
    //mprintf("recv from %d tag %d len %d\n", info.rank, gp, ie - ib);
    MPI_Irecv(&MRC_F1(f1, 0, ib), ie - ib, MPI_FLOAT, info.rank,
	      gp, mrc_io_comm(io), &ctx->recv_reqs[ctx->nr_recv_reqs++]);
  }
  assert(ctx->nr_recv_reqs == ctx->np[dim]);
}

static void
collective_m1_recv_end(struct mrc_io *io, struct collective_m1_ctx *ctx)
{
  struct xdmf *xdmf = to_xdmf(io);

  if (io->rank != xdmf->writers[0])
    return;

  MPI_Waitall(ctx->nr_recv_reqs, ctx->recv_reqs, MPI_STATUSES_IGNORE);
  free(ctx->recv_reqs);
}

// ----------------------------------------------------------------------
// xdmf_collective_write_m1

static void
xdmf_collective_write_m1(struct mrc_io *io, const char *path, struct mrc_m1 *m1)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;

  struct collective_m1_ctx ctx;
  int nr_comps;
  mrc_m1_get_param_int(m1, "nr_comps", &nr_comps);
  mrc_m1_get_param_int(m1, "dim", &ctx.dim);
  mrc_m1_get_param_int(m1, "sw", &ctx.sw);
  mrc_domain_get_global_dims(m1->domain, ctx.gdims);
  mrc_domain_get_nr_global_patches(m1->domain, &ctx.nr_global_patches);
  mrc_domain_get_nr_procs(m1->domain, ctx.np);
  mrc_domain_get_patches(m1->domain, &ctx.nr_patches);
  int dim = ctx.dim;
  int slab_off_save, slab_dims_save;
  slab_off_save = xdmf->slab_off[0];
  slab_dims_save = xdmf->slab_dims[0];
  // FIXME
  if (!xdmf->slab_dims[0]) {
    xdmf->slab_dims[0] = ctx.gdims[dim] + 2 * ctx.sw;
    xdmf->slab_off[0] = -ctx.sw;
  }

  if (xdmf->is_writer) {
    // we're creating the f1 on all writers, but only fill and actually write
    // it on writers[0]
    struct mrc_f1 *f1 = mrc_f1_create(MPI_COMM_SELF);
    mrc_f1_set_param_int(f1, "dimsx", xdmf->slab_dims[0]);
    mrc_f1_set_param_int(f1, "offx", xdmf->slab_off[0]);
    mrc_f1_setup(f1);

    hid_t group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT); H5_CHK(group0);
    for (int m = 0; m < nr_comps; m++) {
      mrc_f1_set_comp_name(f1, 0, mrc_m1_comp_name(m1, m));
      collective_m1_recv_begin(io, &ctx, m1->domain, f1, m);
      collective_m1_send_begin(io, &ctx, m1, m);
      collective_m1_recv_end(io, &ctx);
      collective_m1_write_f1(io, path, f1, m, group0);
      collective_m1_send_end(io, &ctx);
    }
    ierr = H5Gclose(group0); CE;

    mrc_f1_destroy(f1);
  } else { // not writer
    for (int m = 0; m < nr_comps; m++) {
      collective_m1_send_begin(io, &ctx, m1, m);
      collective_m1_send_end(io, &ctx);
    }
  }
  xdmf->slab_dims[0] = slab_dims_save;
  xdmf->slab_off[0] = slab_off_save;
}

// ----------------------------------------------------------------------

static void
collective_m1_read_recv_begin(struct mrc_io *io, struct collective_m1_ctx *ctx,
			      struct mrc_m1 *m1, int m)
{
  struct xdmf *xdmf = to_xdmf(io);

  ctx->recv_reqs = calloc(1 + ctx->nr_patches, sizeof(*ctx->recv_reqs));
  ctx->nr_recv_reqs = 0;

  MPI_Irecv(ctx->comp_name, 100, MPI_CHAR, xdmf->writers[0], 0, mrc_io_comm(io),
	    &ctx->recv_reqs[ctx->nr_recv_reqs++]);

  for (int p = 0; p < ctx->nr_patches; p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(m1->domain, p, &info);
    int ib = -ctx->sw;
    int ie = info.ldims[ctx->dim] + ctx->sw;
    struct mrc_m1_patch *m1p = mrc_m1_patch_get(m1, p);
    //	mprintf("recv to %d tag %d\n", xdmf->writers[0], info.global_patch);
    MPI_Irecv(&MRC_M1(m1p, m, ib), ie - ib, MPI_FLOAT,
	      xdmf->writers[0], info.global_patch, mrc_io_comm(io),
	      &ctx->recv_reqs[ctx->nr_recv_reqs++]);
    mrc_m1_patch_put(m1);
  }
}

static void
collective_m1_read_recv_end(struct mrc_io *io, struct collective_m1_ctx *ctx,
			    struct mrc_m1 *m1, int m)
{
  MPI_Waitall(ctx->nr_recv_reqs, ctx->recv_reqs, MPI_STATUSES_IGNORE);
  free(ctx->recv_reqs);
  mrc_m1_set_comp_name(m1, m, ctx->comp_name);
}

static void
collective_m1_read_send_begin(struct mrc_io *io, struct collective_m1_ctx *ctx,
			      struct mrc_domain *domain, struct mrc_f1 *f1, int m)
{
  struct xdmf *xdmf = to_xdmf(io);

  if (io->rank != xdmf->writers[0])
    return;

  int dim = ctx->dim;
  ctx->send_reqs = calloc(io->size + ctx->nr_global_patches, sizeof(*ctx->send_reqs));
  ctx->nr_send_reqs = 0;
  const char *comp_name = mrc_f1_comp_name(f1, m);
  assert(comp_name && strlen(comp_name) < 99);
  for (int r = 0; r < io->size; r++) {
    MPI_Isend((char *) comp_name, strlen(comp_name) + 1, MPI_CHAR,
	      r, 0, mrc_io_comm(io), &ctx->send_reqs[ctx->nr_send_reqs++]);
  }
  for (int gp = 0; gp < ctx->nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(domain, gp, &info);
    int ib = info.off[dim] - ctx->sw;
    int ie = info.off[dim] + info.ldims[dim] + ctx->sw;
    //  mprintf("send from %d tag %d\n", info.rank, gp);
    MPI_Isend(&MRC_F1(f1, m, ib), ie - ib, MPI_FLOAT,
	      info.rank, gp, mrc_io_comm(io), &ctx->send_reqs[ctx->nr_send_reqs++]);
  }
}

static void
collective_m1_read_send_end(struct mrc_io *io, struct collective_m1_ctx *ctx)
{
  struct xdmf *xdmf = to_xdmf(io);

  if (io->rank != xdmf->writers[0])
    return;

  MPI_Waitall(ctx->nr_send_reqs, ctx->send_reqs, MPI_STATUSES_IGNORE);
  free(ctx->send_reqs);
}

struct read_m1_cb_data {
  struct mrc_io *io;
  struct mrc_f1 *gfld;
  hid_t filespace;
  hid_t memspace;
  hid_t dxpl;
};

static herr_t
read_m1_cb(hid_t g_id, const char *name, const H5L_info_t *info, void *op_data)
{
  struct read_m1_cb_data *data = op_data;
  struct mrc_f1 *gfld = data->gfld;
  int *ib = gfld->_ghost_off;
  int ierr;

  hid_t group_fld = H5Gopen(g_id, name, H5P_DEFAULT); H5_CHK(group_fld);
  int m;
  ierr = H5LTget_attribute_int(group_fld, ".", "m", &m); CE;
  mrc_f1_set_comp_name(data->gfld, m, name);
  hid_t group = H5Gopen(group_fld, "p0", H5P_DEFAULT); H5_CHK(group);

  hid_t dset = H5Dopen(group, "1d", H5P_DEFAULT); H5_CHK(dset);
  ierr = H5Dread(dset, H5T_NATIVE_FLOAT, data->memspace, data->filespace,
		 data->dxpl, &MRC_F1(gfld, m, ib[0])); CE;
  ierr = H5Dclose(dset); CE;
  
  ierr = H5Gclose(group); CE;
  ierr = H5Gclose(group_fld); CE;

  return 0;
}

// ----------------------------------------------------------------------
// xdmf_collective_read_m1

static void
xdmf_collective_read_m1(struct mrc_io *io, const char *path, struct mrc_m1 *m1)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;

  struct collective_m1_ctx ctx;
  int nr_comps, gdims[3];
  mrc_m1_get_param_int(m1, "nr_comps", &nr_comps);
  mrc_m1_get_param_int(m1, "dim", &ctx.dim);
  mrc_m1_get_param_int(m1, "sw", &ctx.sw);
  mrc_domain_get_global_dims(m1->domain, gdims);
  mrc_domain_get_nr_global_patches(m1->domain, &ctx.nr_global_patches);
  mrc_domain_get_patches(m1->domain, &ctx.nr_patches);

  if (xdmf->is_writer) {
    struct mrc_f1 *f1 = mrc_f1_create(MPI_COMM_SELF);
    mrc_f1_set_param_int(f1, "nr_comps", nr_comps);
    mrc_f1_set_param_int(f1, "dimsx", gdims[ctx.dim]);
    mrc_f1_set_param_int(f1, "sw", ctx.sw);
    mrc_f1_setup(f1);

    hid_t group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT); H5_CHK(group0);

    hsize_t hgdims[1] = { mrc_f1_ghost_dims(f1)[0] };

    hid_t filespace = H5Screate_simple(1, hgdims, NULL); H5_CHK(filespace);
    hid_t memspace;
    if (io->rank == xdmf->writers[0]) {
      memspace = H5Screate_simple(1, hgdims, NULL); H5_CHK(memspace);
    } else {
      memspace = H5Screate(H5S_NULL);
      H5Sselect_none(memspace);
      H5Sselect_none(filespace);
    }
    hid_t dxpl = H5Pcreate(H5P_DATASET_XFER); H5_CHK(dxpl);
#ifdef H5_HAVE_PARALLEL
    if (xdmf->use_independent_io) {
      H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT);
    } else {
      H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE);
    }
#endif

    struct read_m1_cb_data cb_data = {
      .io        = io,
      .gfld      = f1,
      .memspace  = memspace,
      .filespace = filespace,
      .dxpl      = dxpl,
    };
    
    hsize_t idx = 0;
    H5Literate_by_name(group0, ".", H5_INDEX_NAME, H5_ITER_INC, &idx,
		       read_m1_cb, &cb_data, H5P_DEFAULT);

    ierr = H5Pclose(dxpl); CE;
    ierr = H5Sclose(memspace); CE;
    ierr = H5Sclose(filespace); CE;

    ierr = H5Gclose(group0); CE;

    for (int m = 0; m < nr_comps; m++) {
      collective_m1_read_recv_begin(io, &ctx, m1, m);
      collective_m1_read_send_begin(io, &ctx, m1->domain, f1, m);
      collective_m1_read_recv_end(io, &ctx, m1, m);
      collective_m1_read_send_end(io, &ctx);
    }
  } else { // not writer
    for (int m = 0; m < nr_comps; m++) {
      collective_m1_read_recv_begin(io, &ctx, m1, m);
      collective_m1_read_recv_end(io, &ctx, m1, m);
    }
  }
}

// ======================================================================

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static bool
find_intersection(int *ilo, int *ihi, const int *ib1, const int *im1,
		  const int *ib2, const int *im2)
{
  for (int d = 0; d < 3; d++) {
    ilo[d] = MAX(ib1[d], ib2[d]);
    ihi[d] = MIN(ib1[d] + im1[d], ib2[d] + im2[d]);
    if (ihi[d] - ilo[d] <= 0) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------
// collective helper context

struct collective_m3_entry {
  struct mrc_fld *fld;
  int ilo[3];
  int ihi[3];
  int patch;
  int global_patch; //< also used as tag
  int rank; //< of peer
};

struct collective_m3_ctx {
  int gdims[3];
  int slab_dims[3], slab_off[3];
  int writer_dims[3], writer_off[3];
  int nr_patches, nr_global_patches;
  int slow_dim;
  int slow_indices_per_writer;
  int slow_indices_rmndr;

  struct collective_m3_entry *sends;
  float **send_bufs; // one for each writer
  MPI_Request *send_reqs;
  int nr_sends;

  struct collective_m3_entry *recvs;
  float **recv_bufs; // one for each rank
  MPI_Request *recv_reqs;
  int nr_recvs;
};

static void
collective_m3_init(struct mrc_io *io, struct collective_m3_ctx *ctx,
		   struct mrc_domain *domain)
{
  struct xdmf *xdmf = to_xdmf(io);

  mrc_domain_get_global_dims(domain, ctx->gdims);
  mrc_domain_get_patches(domain, &ctx->nr_patches);
  mrc_domain_get_nr_global_patches(domain, &ctx->nr_global_patches);
  for (int d = 0; d < 3; d++) {
    if (xdmf->slab_dims[d]) {
      ctx->slab_dims[d] = xdmf->slab_dims[d];
    } else {
      ctx->slab_dims[d] = ctx->gdims[d];
    }
    ctx->slab_off[d] = xdmf->slab_off[d];
  }
  ctx->slow_dim = 2;
  while (ctx->gdims[ctx->slow_dim] == 1) {
    ctx->slow_dim--;
  }
  assert(ctx->slow_dim >= 0);
  int total_slow_indices = ctx->slab_dims[ctx->slow_dim];
  ctx->slow_indices_per_writer = total_slow_indices / xdmf->nr_writers;
  ctx->slow_indices_rmndr = total_slow_indices % xdmf->nr_writers;
}

static void
get_writer_off_dims(struct collective_m3_ctx *ctx, int writer,
		    int *writer_off, int *writer_dims)
{
  for (int d = 0; d < 3; d++) {
    writer_dims[d] = ctx->slab_dims[d];
    writer_off[d] = ctx->slab_off[d];
  }
  writer_dims[ctx->slow_dim] = ctx->slow_indices_per_writer + (writer < ctx->slow_indices_rmndr);
  if (writer < ctx->slow_indices_rmndr) {
    writer_off[ctx->slow_dim] += (ctx->slow_indices_per_writer + 1) * writer;
  } else {
    writer_off[ctx->slow_dim] += ctx->slow_indices_rmndr +
      ctx->slow_indices_per_writer * writer;
  }
}

// ----------------------------------------------------------------------
// collective_send_fld_begin

static void
collective_send_fld_begin(struct collective_m3_ctx *ctx, struct mrc_io *io,
			  struct mrc_fld *m3, int m)
{
  struct xdmf *xdmf = to_xdmf(io);

  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(m3->_domain, &nr_patches);

  ctx->nr_sends = 0;
  int buf_size[xdmf->nr_writers];
  ctx->send_bufs = calloc(xdmf->nr_writers, sizeof(*ctx->send_bufs));

  for (int writer = 0; writer < xdmf->nr_writers; writer++) {
    // don't send to self
    if (xdmf->writers[writer] == io->rank) {
      continue;
    }
    int writer_off[3], writer_dims[3];
    get_writer_off_dims(ctx, writer, writer_off, writer_dims);

    // find buf_size per writer
    buf_size[writer] = 0;

    for (int p = 0; p < nr_patches; p++) {
      int ilo[3], ihi[3];
      bool has_intersection =
	find_intersection(ilo, ihi, patches[p].off, patches[p].ldims,
			  writer_off, writer_dims);
      if (!has_intersection)
	continue;

      ctx->nr_sends++;
      int len = m3->_dims.vals[0] * m3->_dims.vals[1] * m3->_dims.vals[2];
      buf_size[writer] += len;
    }

    // allocate buf per writer
    mprintf("to writer %d buf_size %d\n", writer, buf_size[writer]);
    ctx->send_bufs[writer] = malloc(buf_size[writer] * sizeof(*ctx->send_bufs[writer]));
    buf_size[writer] = 0;

    // fill buf per writer
    for (int p = 0; p < nr_patches; p++) {
      int ilo[3], ihi[3];
      int *off = patches[p].off;
      bool has_intersection =
	find_intersection(ilo, ihi, off, patches[p].ldims,
			  writer_off, writer_dims);
      if (!has_intersection)
	continue;

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(m3->_domain, p, &info);
      float *buf_ptr = &ctx->send_bufs[writer][buf_size[writer]];
      mprintf("ilo %d %d %d ihi %d %d %d\n", ilo[0], ilo[1], ilo[2],
	      ihi[0], ihi[1], ihi[2]);
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    *buf_ptr++ = MRC_S5(m3, ix-off[0],iy-off[1],iz-off[2], m, p);
	  }
	}
      }

      int len = (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
      buf_size[writer] += len;
    }
  }

  // send buf per writer
  for (int writer = 0; writer < xdmf->nr_writers; writer++) {
    buf_size[writer] = 0;
  }

  mprintf("nr_sends = %d\n", ctx->nr_sends);
  int sr = 0;
  ctx->send_reqs = calloc(ctx->nr_sends, sizeof(*ctx->send_reqs));

  for (int writer = 0; writer < xdmf->nr_writers; writer++) {
    // don't send to self
    if (xdmf->writers[writer] == io->rank) {
      continue;
    }
    int writer_off[3], writer_dims[3];
    get_writer_off_dims(ctx, writer, writer_off, writer_dims);
    for (int p = 0; p < nr_patches; p++) {
      int ilo[3], ihi[3];
      bool has_intersection =
	find_intersection(ilo, ihi, patches[p].off, patches[p].ldims,
			  writer_off, writer_dims);
      if (!has_intersection)
	continue;

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(m3->_domain, p, &info);

      int len = (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
      mprintf("MPI_Isend -> %d gp %d len %d\n", xdmf->writers[writer],
	      info.global_patch, len);
      MPI_Isend(&ctx->send_bufs[writer][buf_size[writer]], len, MPI_FLOAT,
		xdmf->writers[writer], info.global_patch,
		mrc_io_comm(io), &ctx->send_reqs[sr++]);

      buf_size[writer] += len;
    }
  }
}

// ----------------------------------------------------------------------
// collective_send_fld_end

static void
collective_send_fld_end(struct collective_m3_ctx *ctx, struct mrc_io *io,
			struct mrc_fld *m3, int m)
{
  struct xdmf *xdmf = to_xdmf(io);

  MPI_Waitall(ctx->nr_sends, ctx->send_reqs, MPI_STATUSES_IGNORE);

  for (int writer = 0; writer < xdmf->nr_writers; writer++) {
    // don't send to self
    if (xdmf->writers[writer] == io->rank) {
      continue;
    }
    free(ctx->send_bufs[writer]);
  }
  free(ctx->send_bufs);
  free(ctx->send_reqs);
}
    
// ----------------------------------------------------------------------
// collective_recv_fld_begin

static void
collective_recv_fld_begin(struct collective_m3_ctx *ctx,
			  struct mrc_io *io, struct mrc_fld *fld,
			  struct mrc_fld *m3)
{
  // find out who's sending, OPT: this way is not very scalable
  // could also be optimized by just looking at slow_dim
  // FIXME, figure out pattern and cache, at least across components

  int nr_global_patches;
  mrc_domain_get_nr_global_patches(m3->_domain, &nr_global_patches);
  ctx->nr_recvs = 0;
  ctx->recv_bufs = calloc(io->size, sizeof(*ctx->recv_bufs));
  int *buf_sizes = calloc(io->size, sizeof(*buf_sizes));
  for (int gp = 0; gp < nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(m3->_domain, gp, &info);
    // skip local patches for now
    if (info.rank == io->rank) {
      continue;
    }
    int ilo[3], ihi[3];
    int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					     mrc_fld_ghost_offs(fld), mrc_fld_ghost_dims(fld));
    if (!has_intersection) {
      continue;
    }
    ctx->nr_recvs++;
    int len = (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
    buf_sizes[info.rank] += len;
  }
  mprintf("nr_recvs = %d\n", ctx->nr_recvs);
  for (int rank = 0; rank < io->size; rank++) {
    mprintf("recv buf_sizes[%d] = %d\n", rank, buf_sizes[rank]);
    if (buf_sizes[rank] > 0) {
      ctx->recv_bufs[rank] = malloc(buf_sizes[rank] * sizeof(ctx->recv_bufs[rank]));
    }
  }
  int rr = 0;
  ctx->recv_reqs = calloc(ctx->nr_recvs, sizeof(*ctx->recv_reqs));
  ctx->recvs = calloc(ctx->nr_recvs, sizeof(*ctx->recvs));

  for (int rank = 0; rank < io->size; rank++) {
    float *recv_buf = ctx->recv_bufs[rank];
    if (!recv_buf) {
      continue;
    }

    for (int gp = 0; gp < nr_global_patches; gp++) {
      struct mrc_patch_info info;
      mrc_domain_get_global_patch_info(m3->_domain, gp, &info);
      // only consider recvs from "rank" for now
      if (info.rank != rank) {
	continue;
      }
      int ilo[3], ihi[3];
      int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					       mrc_fld_ghost_offs(fld), mrc_fld_ghost_dims(fld));
      if (!has_intersection) {
	continue;
      }
      int len = (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
      
      mprintf("MPI_Irecv <- %d gp %d len %d\n", info.rank, info.global_patch, len);
      MPI_Irecv(recv_buf, len, MPI_FLOAT, info.rank,
		info.global_patch, mrc_io_comm(io), &ctx->recv_reqs[rr++]);
      recv_buf += len;
    }
  }

  free(buf_sizes);
}

// ----------------------------------------------------------------------
// collective_recv_fld_end

static void
collective_recv_fld_end(struct collective_m3_ctx *ctx,
			struct mrc_io *io, struct mrc_fld *fld,
			struct mrc_fld *m3, int m)
{
  MPI_Waitall(ctx->nr_recvs, ctx->recv_reqs, MPI_STATUSES_IGNORE);

  int nr_global_patches;
  mrc_domain_get_nr_global_patches(m3->_domain, &nr_global_patches);
  int rr = 0;

  for (int rank = 0; rank < io->size; rank++) {
    float *recv_buf = ctx->recv_bufs[rank];
    if (!recv_buf) {
      continue;
    }
    
    for (int gp = 0; gp < nr_global_patches; gp++) {
      struct mrc_patch_info info;
      mrc_domain_get_global_patch_info(m3->_domain, gp, &info);
      // only consider recvs from "rank" for now
      if (info.rank != rank) {
	continue;
      }
      
      int *off = info.off;
      // OPT, could be cached 2nd(?) and 3rd time
      int ilo[3], ihi[3];
      int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					       mrc_fld_ghost_offs(fld), mrc_fld_ghost_dims(fld));
      if (!has_intersection) {
	continue;
      }
      
      int len = (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
      float *buf_ptr = recv_buf;
      recv_buf += len;
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    MRC_F3(fld,0, ix,iy,iz) = *buf_ptr++;
	  }
	}
      }
      rr++;
    }
  }

  free(ctx->recv_reqs);
  free(ctx->recvs);

  for (int rank = 0; rank < io->size; rank++) {
    free(ctx->recv_bufs[rank]);
  }
  free(ctx->recv_bufs);
}

// ----------------------------------------------------------------------
// collective_recv_fld_local

static void
collective_recv_fld_local(struct collective_m3_ctx *ctx,
			  struct mrc_io *io, struct mrc_fld *fld,
			  struct mrc_fld *m3, int m)
{
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(m3->_domain, &nr_patches);

  for (int p = 0; p < nr_patches; p++) {
    struct mrc_patch *patch = &patches[p];
    int *off = patch->off, *ldims = patch->ldims;

    int ilo[3], ihi[3];
    bool has_intersection =
      find_intersection(ilo, ihi, off, ldims, mrc_fld_ghost_offs(fld), mrc_fld_ghost_dims(fld));
    if (!has_intersection) {
      continue;
    }
    struct mrc_fld_patch *m3p = mrc_fld_patch_get(m3, p);
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  MRC_F3(fld,0, ix,iy,iz) =
	    MRC_M3(m3p, m, ix - off[0], iy - off[1], iz - off[2]);
	}
      }
    }
    mrc_fld_patch_put(m3);
  }
}

// ----------------------------------------------------------------------
// collective_write_fld
// does the actual write of the partial fld to the file
// only called on writer procs

static void
collective_write_fld(struct collective_m3_ctx *ctx, struct mrc_io *io,
		    const char *path, struct mrc_fld *fld, int m,
		    struct mrc_fld *m3, struct xdmf_spatial *xs, hid_t group0)
{
  int ierr;

  xdmf_spatial_save_fld_info(xs, strdup(mrc_fld_comp_name(m3, m)), strdup(path), false);

  hid_t group_fld = H5Gcreate(group0, mrc_fld_comp_name(m3, m), H5P_DEFAULT,
			      H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group_fld);
  ierr = H5LTset_attribute_int(group_fld, ".", "m", &m, 1); CE;
  
  hid_t group = H5Gcreate(group_fld, "p0", H5P_DEFAULT,
			  H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
  int i0 = 0;
  ierr = H5LTset_attribute_int(group, ".", "global_patch", &i0, 1); CE;

  hsize_t fdims[3] = { ctx->slab_dims[2], ctx->slab_dims[1], ctx->slab_dims[0] };
  hid_t filespace = H5Screate_simple(3, fdims, NULL); H5_CHK(filespace);
  hid_t dset = H5Dcreate(group, "3d", H5T_NATIVE_FLOAT, filespace, H5P_DEFAULT,
			 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(dset);
  hid_t dxpl = H5Pcreate(H5P_DATASET_XFER); H5_CHK(dxpl);
#ifdef H5_HAVE_PARALLEL
  struct xdmf *xdmf = to_xdmf(io);
  if (xdmf->use_independent_io) {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT); CE;
  } else {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE); CE;
  }
#endif
  const int *im = mrc_fld_ghost_dims(fld), *ib = mrc_fld_ghost_offs(fld);
  hsize_t mdims[3] = { im[2], im[1], im[0] };
  hsize_t foff[3] = { ib[2] - ctx->slab_off[2],
		      ib[1] - ctx->slab_off[1],
		      ib[0] - ctx->slab_off[0] };
  hid_t memspace = H5Screate_simple(3, mdims, NULL);
  ierr = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, foff, NULL,
			     mdims, NULL); CE;

  ierr = H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, dxpl, fld->_arr); CE;
  
  ierr = H5Dclose(dset); CE;
  ierr = H5Sclose(memspace); CE;
  ierr = H5Sclose(filespace); CE;
  ierr = H5Pclose(dxpl); CE;

  ierr = H5Gclose(group); CE;
  ierr = H5Gclose(group_fld); CE;
}

// ----------------------------------------------------------------------
// xdmf_collective_write_m3

static void
xdmf_collective_write_m3(struct mrc_io *io, const char *path, struct mrc_fld *m3)
{
  struct xdmf *xdmf = to_xdmf(io);

  struct collective_m3_ctx ctx;
  collective_m3_init(io, &ctx, m3->_domain);

  struct xdmf_file *file = &xdmf->file;
  struct xdmf_spatial *xs = xdmf_spatial_find(&file->xdmf_spatial_list,
					      mrc_domain_name(m3->_domain));
  if (!xs) {
    xs = xdmf_spatial_create_m3_parallel(&file->xdmf_spatial_list,
					 mrc_domain_name(m3->_domain),
					 m3->_domain,
					 ctx.slab_off, ctx.slab_dims, io);
  }

  if (xdmf->is_writer) {
    int writer;
    MPI_Comm_rank(xdmf->comm_writers, &writer);
    int writer_dims[3], writer_off[3];
    get_writer_off_dims(&ctx, writer, writer_off, writer_dims);
    /* mprintf("writer_off %d %d %d dims %d %d %d\n", */
    /* 	    writer_off[0], writer_off[1], writer_off[2], */
    /* 	    writer_dims[0], writer_dims[1], writer_dims[2]); */

    struct mrc_fld *fld = mrc_fld_create(MPI_COMM_NULL);
    mrc_fld_set_param_int_array(fld, "dims", 4,
				(int[4]) { writer_dims[0], writer_dims[1], writer_dims[2], 1 });
    mrc_fld_set_param_int_array(fld, "offs", 4,
				(int[4]) { writer_off[0], writer_off[1], writer_off[2], 0 });
    mrc_fld_setup(fld);

    hid_t group0;
    if (H5Lexists(file->h5_file, path, H5P_DEFAULT) > 0) {
      group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT);
    } else {
      assert(0); // FIXME, can this happen?
      group0 = H5Gcreate(file->h5_file, path, H5P_DEFAULT,
			 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group0);
    }
    int nr_1 = 1;
    H5LTset_attribute_int(group0, ".", "nr_patches", &nr_1, 1);
    
    for (int m = 0; m < mrc_fld_nr_comps(m3); m++) {
      collective_recv_fld_begin(&ctx, io, fld, m3);
      collective_send_fld_begin(&ctx, io, m3, m);
      collective_recv_fld_local(&ctx, io, fld, m3, m);
      collective_recv_fld_end(&ctx, io, fld, m3, m);
      collective_write_fld(&ctx, io, path, fld, m, m3, xs, group0);
      collective_send_fld_end(&ctx, io, m3, m);
    }

    H5Gclose(group0);
    mrc_fld_destroy(fld);
  } else {
    for (int m = 0; m < mrc_fld_nr_comps(m3); m++) {
      collective_send_fld_begin(&ctx, io, m3, m);
      collective_send_fld_end(&ctx, io, m3, m);
    }
  }
}

// ======================================================================

static void
collective_m3_send_setup(struct mrc_io *io, struct collective_m3_ctx *ctx,
			 struct mrc_domain *domain, struct mrc_fld *gfld)
{
  ctx->nr_sends = 0;
  for (int gp = 0; gp < ctx->nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(domain, gp, &info);
    
    int ilo[3], ihi[3];
    int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					     mrc_fld_ghost_offs(gfld), mrc_fld_ghost_dims(gfld));
    if (has_intersection)
      ctx->nr_sends++;
  }

  ctx->send_reqs = calloc(ctx->nr_sends, sizeof(*ctx->send_reqs));
  ctx->sends = calloc(ctx->nr_sends, sizeof(*ctx->sends));

  for (int i = 0, gp = 0; i < ctx->nr_sends; i++) {
    struct collective_m3_entry *send = &ctx->sends[i];
    struct mrc_patch_info info;
    bool has_intersection;
    do {
      mrc_domain_get_global_patch_info(domain, gp++, &info);
      has_intersection = find_intersection(send->ilo, send->ihi, info.off, info.ldims,
					   mrc_fld_ghost_offs(gfld), mrc_fld_ghost_dims(gfld));
    } while (!has_intersection);
    send->patch = info.global_patch;
    send->rank = info.rank;
  }
}

static void
collective_m3_send_begin(struct mrc_io *io, struct collective_m3_ctx *ctx,
			 struct mrc_domain *domain, struct mrc_fld *gfld)
{
  collective_m3_send_setup(io, ctx, domain, gfld);

  for (int i = 0; i < ctx->nr_sends; i++) {
    struct collective_m3_entry *send = &ctx->sends[i];
    int *ilo = send->ilo, *ihi = send->ihi;
    struct mrc_fld *fld = mrc_fld_create(MPI_COMM_NULL);
    mrc_fld_set_param_int_array(fld, "offs", 4,
			       (int[4]) { ilo[0], ilo[1], ilo[2], 0 });
    mrc_fld_set_param_int_array(fld, "dims", 4,
			       (int [4]) { ihi[0] - ilo[0], ihi[1] - ilo[1], ihi[2] - ilo[2],
				    mrc_fld_nr_comps(gfld) });
    mrc_fld_setup(fld);
    
    for (int m = 0; m < mrc_fld_nr_comps(gfld); m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    MRC_F3(fld, m, ix,iy,iz) = MRC_F3(gfld, m, ix,iy,iz);
	  }
	}
      }
    }
    
    MPI_Isend(fld->_arr, fld->_len, MPI_FLOAT, send->rank, send->patch,
	      mrc_io_comm(io), &ctx->send_reqs[i]);
    send->fld = fld;
  }
}

static void
collective_m3_send_end(struct mrc_io *io, struct collective_m3_ctx *ctx)
{
  MPI_Waitall(ctx->nr_sends, ctx->send_reqs, MPI_STATUSES_IGNORE);
  for (int i = 0; i < ctx->nr_sends; i++) {
    mrc_fld_destroy(ctx->sends[i].fld);
  }
  free(ctx->sends);
  free(ctx->send_reqs);
}

// ----------------------------------------------------------------------

static void
collective_m3_recv_setup(struct mrc_io *io, struct collective_m3_ctx *ctx,
			 struct mrc_domain *domain)
{
  struct xdmf *xdmf = to_xdmf(io);

  ctx->nr_recvs = 0;
  for (int p = 0; p < ctx->nr_patches; p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(domain, p, &info);

    for (int writer = 0; writer < xdmf->nr_writers; writer++) {
      int writer_off[3], writer_dims[3];
      get_writer_off_dims(ctx, writer, writer_off, writer_dims);
      int ilo[3], ihi[3];
      if (find_intersection(ilo, ihi, info.off, info.ldims,
			    writer_off, writer_dims)) {
	ctx->nr_recvs++;
      }
    }
  }

  ctx->recv_reqs = calloc(ctx->nr_recvs, sizeof(*ctx->recv_reqs));
  ctx->recvs = calloc(ctx->nr_recvs, sizeof(*ctx->recvs));

  int i = 0;
  for (int p = 0; p < ctx->nr_patches; p++) {
    struct mrc_patch_info info;
    mrc_domain_get_local_patch_info(domain, p, &info);

    for (int writer = 0; writer < xdmf->nr_writers; writer++) {
      if (i == ctx->nr_recvs) {
	break;
      }
      struct collective_m3_entry *recv = &ctx->recvs[i];

      int writer_off[3], writer_dims[3];
      get_writer_off_dims(ctx, writer, writer_off, writer_dims);
      if (!find_intersection(recv->ilo, recv->ihi, info.off, info.ldims,
			     writer_off, writer_dims)) {
	continue;
      }
      recv->rank = xdmf->writers[writer];
      recv->patch = p;
      recv->global_patch = info.global_patch;
      // patch-local indices from here on
      for (int d = 0; d < 3; d++) {
	recv->ilo[d] -= info.off[d];
	recv->ihi[d] -= info.off[d];
      }
      i++;
    }
  }
}

static void
collective_m3_recv_begin(struct mrc_io *io, struct collective_m3_ctx *ctx,
			 struct mrc_domain *domain, struct mrc_fld *m3)
{
  collective_m3_recv_setup(io, ctx, domain);

  for (int i = 0; i < ctx->nr_recvs; i++) {
    struct collective_m3_entry *recv = &ctx->recvs[i];

    struct mrc_fld *fld = mrc_fld_create(MPI_COMM_NULL);
    int *ilo = recv->ilo, *ihi = recv->ihi; // FIXME, -> off, dims
    mrc_fld_set_param_int_array(fld, "offs", 4,
			       (int[4]) { ilo[0], ilo[1], ilo[2], 0 });
    mrc_fld_set_param_int_array(fld, "dims", 4,
			       (int[4]) { ihi[0] - ilo[0], ihi[1] - ilo[1], ihi[2] - ilo[2],
				   mrc_fld_nr_comps(m3) });
    mrc_fld_setup(fld);
    
    MPI_Irecv(fld->_arr, fld->_len, MPI_FLOAT, recv->rank,
	      recv->global_patch, mrc_io_comm(io), &ctx->recv_reqs[i]);
    recv->fld = fld;
  }
}

static void
collective_m3_recv_end(struct mrc_io *io, struct collective_m3_ctx *ctx,
		       struct mrc_domain *domain, struct mrc_fld *m3)
{
  MPI_Waitall(ctx->nr_recvs, ctx->recv_reqs, MPI_STATUSES_IGNORE);
  
  for (int i = 0; i < ctx->nr_recvs; i++) {
    struct collective_m3_entry *recv = &ctx->recvs[i];
    struct mrc_fld_patch *m3p = mrc_fld_patch_get(m3, recv->patch);

    int *ilo = recv->ilo, *ihi = recv->ihi;
    for (int m = 0; m < mrc_fld_nr_comps(m3); m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    MRC_M3(m3p, m, ix,iy,iz) = MRC_F3(recv->fld, m, ix,iy,iz);
	  }
	}
      }
    }
    mrc_fld_patch_put(m3);
    mrc_fld_destroy(recv->fld);
  }
  free(ctx->recvs);
  free(ctx->recv_reqs);
}

struct read_m3_cb_data {
  struct mrc_io *io;
  struct mrc_fld *gfld;
  hid_t filespace;
  hid_t memspace;
  hid_t dxpl;
};

static herr_t
read_m3_cb(hid_t g_id, const char *name, const H5L_info_t *info, void *op_data)
{
  struct read_m3_cb_data *data = op_data;
  int ierr;

  hid_t group_fld = H5Gopen(g_id, name, H5P_DEFAULT); H5_CHK(group_fld);
  int m;
  ierr = H5LTget_attribute_int(group_fld, ".", "m", &m); CE;
  hid_t group = H5Gopen(group_fld, "p0", H5P_DEFAULT); H5_CHK(group);

  hid_t dset = H5Dopen(group, "3d", H5P_DEFAULT); H5_CHK(dset);
  struct mrc_fld *gfld = data->gfld;
  int *ib = gfld->_ghost_offs;
  ierr = H5Dread(dset, H5T_NATIVE_FLOAT, data->memspace, data->filespace,
		 data->dxpl, &MRC_F3(gfld, m, ib[0], ib[1], ib[2])); CE;
  ierr = H5Dclose(dset); CE;
  
  ierr = H5Gclose(group); CE;
  ierr = H5Gclose(group_fld); CE;

  return 0;
}

// ----------------------------------------------------------------------
// xdmf_collective_read_m3

static void
collective_m3_read_fld(struct mrc_io *io, struct collective_m3_ctx *ctx,
		       hid_t group0, struct mrc_fld *fld)
{
  struct xdmf *xdmf = to_xdmf(io);
  int ierr;

  int writer_rank;
  MPI_Comm_rank(xdmf->comm_writers, &writer_rank);
  int writer_dims[3], writer_off[3];
  get_writer_off_dims(ctx, writer_rank, writer_off, writer_dims);
  /* mprintf("writer_off %d %d %d dims %d %d %d\n", */
  /* 	    writer_off[0], writer_off[1], writer_off[2], */
  /* 	    writer_dims[0], writer_dims[1], writer_dims[2]); */

  int nr_comps = mrc_fld_nr_comps(fld);
  mrc_fld_set_param_int_array(fld, "dims", 4,
			      (int[4]) { writer_dims[0], writer_dims[1], writer_dims[2], nr_comps });
  mrc_fld_set_param_int_array(fld, "offs", 4,
			      (int[4]) { writer_off[0], writer_off[1], writer_off[2], 0 });
  mrc_fld_setup(fld);

  hsize_t fdims[3] = { ctx->gdims[2], ctx->gdims[1], ctx->gdims[0] };
  hsize_t foff[3] = { writer_off[2], writer_off[1], writer_off[0] };
  hsize_t mdims[3] = { writer_dims[2], writer_dims[1], writer_dims[0] };
  hid_t filespace = H5Screate_simple(3, fdims, NULL); H5_CHK(filespace);
  ierr = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, foff, NULL,
			     mdims, NULL); CE;
  hid_t memspace = H5Screate_simple(3, mdims, NULL); H5_CHK(memspace);
  hid_t dxpl = H5Pcreate(H5P_DATASET_XFER); H5_CHK(dxpl);
#ifdef H5_HAVE_PARALLEL
  if (xdmf->use_independent_io) {
    H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT);
  } else {
    H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE);
  }
#endif

  struct read_m3_cb_data cb_data = {
    .io        = io,
    .gfld      = fld,
    .memspace  = memspace,
    .filespace = filespace,
    .dxpl      = dxpl,
  };
  
  hsize_t idx = 0;
  H5Literate_by_name(group0, ".", H5_INDEX_NAME, H5_ITER_INC, &idx,
		     read_m3_cb, &cb_data, H5P_DEFAULT);
  
  ierr = H5Pclose(dxpl); CE;
  ierr = H5Sclose(memspace); CE;
  ierr = H5Sclose(filespace); CE;
}

static void
xdmf_collective_read_m3(struct mrc_io *io, const char *path, struct mrc_fld *m3)
{
  struct xdmf *xdmf = to_xdmf(io);
  struct xdmf_file *file = &xdmf->file;
  int ierr;

  struct collective_m3_ctx ctx;
  collective_m3_init(io, &ctx, m3->_domain);

  if (xdmf->is_writer) {
    struct mrc_fld *gfld = mrc_fld_create(MPI_COMM_SELF);
    mrc_fld_set_nr_comps(gfld, mrc_fld_nr_comps(m3));

    hid_t group0 = H5Gopen(file->h5_file, path, H5P_DEFAULT); H5_CHK(group0);
    collective_m3_read_fld(io, &ctx, group0, gfld);
    ierr = H5Gclose(group0); CE;

    collective_m3_recv_begin(io, &ctx, m3->_domain, m3);
    collective_m3_send_begin(io, &ctx, m3->_domain, gfld);
    collective_m3_recv_end(io, &ctx, m3->_domain, m3);
    collective_m3_send_end(io, &ctx);
    mrc_fld_destroy(gfld);
  } else {
    collective_m3_recv_begin(io, &ctx, m3->_domain, m3);
    collective_m3_recv_end(io, &ctx, m3->_domain, m3);
  }
}

// ======================================================================
// mrc_io_ops_xdmf_collective

struct mrc_io_ops mrc_io_xdmf_collective_ops = {
  .name          = "xdmf_collective",
  .size          = sizeof(struct xdmf),
  .param_descr   = xdmf_collective_descr,
  .parallel      = true,
  .setup         = xdmf_collective_setup,
  .destroy       = xdmf_collective_destroy,
  .open          = xdmf_collective_open,
  .close         = xdmf_collective_close,
  .write_attr    = xdmf_collective_write_attr,
  .read_attr     = xdmf_collective_read_attr,
  .write_m1      = xdmf_collective_write_m1,
  .read_m1       = xdmf_collective_read_m1,
  .write_m3      = xdmf_collective_write_m3,
  .read_m3       = xdmf_collective_read_m3,
};


