
#include "psc.h"
#include "psc_output_fields_c.h"
#include "psc_output_format_private.h"

#include <mrc_profile.h>
#include <mrc_common.h>
#include <mrc_params.h>

#include <mpi.h>
#include <string.h>
#include <hdf5.h>
#include <hdf5_hl.h>

// ======================================================================
// hdf5_ctx

struct hdf5_ctx {
  hid_t file;
  hid_t group;
  hid_t group_fld;
};

static void
hdf5_open(struct psc_output_fields_c *out, struct psc_fields_list *list,
	  const char *filename, struct hdf5_ctx *hdf5)
{
  hdf5->file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  hdf5->group = H5Gcreate(hdf5->file, "psc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  H5LTset_attribute_int(hdf5->group, ".", "timestep", &psc.timestep, 1);
  H5LTset_attribute_double(hdf5->group, ".", "dt", &psc.dt, 1);
  H5LTset_attribute_double(hdf5->group, ".", "dx", psc.dx, 3);
  H5LTset_attribute_int(hdf5->group, ".", "gdims", psc.domain.gdims, 3);

  hdf5->group_fld = H5Gcreate(hdf5->group, "fields",
			      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
}

static void
hdf5_close(struct hdf5_ctx *hdf5)
{
  H5Gclose(hdf5->group_fld);
  H5Gclose(hdf5->group);
  H5Fclose(hdf5->file);
}

static void
hdf5_write_field(void *ctx, fields_base_t *fld)
{
  struct psc_patch *patch = &psc.patch[0];
  struct hdf5_ctx *hdf5 = ctx;

  hid_t mem_type;
  if (sizeof(*fld->flds) == 4) { // FIXME
    mem_type = H5T_NATIVE_FLOAT;
  } else {
    mem_type = H5T_NATIVE_DOUBLE;
  }

  hsize_t file_dims[3], mem_dims[3];
  int ie[3];
  for (int d = 0; d < 3; d++) {
    // reverse dimensions because of Fortran order
    mem_dims[d] = fld->im[2-d];
    ie[d] = fld->ib[d] + fld->im[d];
  }
  hid_t mem_space = H5Screate_simple(3, mem_dims, NULL);
  
  if (fld->im[0] == patch->ldims[0] + 2 * psc.ibn[0] &&
      fld->im[1] == patch->ldims[1] + 2 * psc.ibn[1] &&
      fld->im[2] == patch->ldims[2] + 2 * psc.ibn[2]) {
    // we're writing the local field, let's drop the ghost points
    hsize_t start[3], count[3];
    for (int d = 0; d < 3; d++) {
      // reverse dimensions because of Fortran order
      file_dims[d] = patch->ldims[2-d];
      start[d] = psc.ibn[2-d];
      count[d] = patch->ldims[2-d];
    }
    H5Sselect_hyperslab(mem_space, H5S_SELECT_SET, start, NULL, count, NULL);
  } else {
    for (int d = 0; d < 3; d++) {
      file_dims[d] = mem_dims[d];
    }
  }

  hid_t file_space = H5Screate_simple(3, file_dims, NULL); 
  hid_t dataset = H5Dcreate(hdf5->group_fld, fld->name[0], H5T_NATIVE_FLOAT,
			    file_space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  H5Sclose(file_space);
  
  H5Dwrite(dataset, mem_type, mem_space, H5S_ALL, H5P_DEFAULT,
	   &F3_BASE(fld, 0, fld->ib[0], fld->ib[1], fld->ib[2]));
  H5Sclose(mem_space);
  
  H5Dclose(dataset);

  H5LTset_attribute_int(hdf5->group_fld, fld->name[0], "lo", fld->ib, 3);
  H5LTset_attribute_int(hdf5->group_fld, fld->name[0], "hi", ie, 3);
}

// ======================================================================

static void
psc_output_format_hdf5_write_fields(struct psc_output_format *format,
				    struct psc_output_fields_c *out,
				    struct psc_fields_list *list,
				    const char *pfx)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  struct hdf5_ctx hdf5;
  if (rank == 0) {
    char filename[strlen(out->data_dir) + 30];
    sprintf(filename, "%s/%s_%07d.h5", out->data_dir, pfx, psc.timestep);
    hdf5_open(out, list, filename, &hdf5);
  }
  write_fields_combine(list, hdf5_write_field, &hdf5);

  if (rank == 0) {
    hdf5_close(&hdf5);
  }
}

// ======================================================================
// psc_output_format: subclass "hdf5"

struct psc_output_format_ops psc_output_format_hdf5_ops = {
  .name                  = "hdf5",
  .write_fields          = psc_output_format_hdf5_write_fields,
};

// ======================================================================

static void
xdmf_write_spatial_collection(struct psc_output_fields_c *out, struct psc_fields_list *list,
			      const char *pfx)
{
  char fname[strlen(out->data_dir) + strlen(pfx) + 20];
  sprintf(fname, "%s/%s_%07d.xdmf", out->data_dir, pfx, psc.timestep);
  FILE *f = fopen(fname, "w");
  int np[3];
  mrc_domain_get_param_int(psc.mrc_domain, "npx", &np[0]);
  mrc_domain_get_param_int(psc.mrc_domain, "npy", &np[1]);
  mrc_domain_get_param_int(psc.mrc_domain, "npz", &np[2]);

  fields_base_t *fld = &list->flds[0]->f[0];
  fprintf(f, "<?xml version=\"1.0\" ?>\n");
  fprintf(f, "<Xdmf xmlns:xi=\"http://www.w3.org/2001/XInclude\" Version=\"2.0\">\n");
  fprintf(f, "<Domain>\n");
  fprintf(f, "<Grid GridType='Collection' CollectionType='Spatial'>\n");
  fprintf(f, "   <Time Type=\"Single\" Value=\"%g\" />\n", psc.timestep * psc.dt);
  int im[3];
  for (int d = 0; d < 3; d++) {
    im[d] = psc.domain.gdims[d] / np[d];
  }
  for (int kz = 0; kz < np[2]; kz++) {
    for (int ky = 0; ky < np[1]; ky++) {
      for (int kx = 0; kx < np[0]; kx++) {
	fprintf(f, "   <Grid Name=\"mesh-%d-%d-%d-%d\" GridType=\"Uniform\">\n",
		kx, ky, kz, psc.timestep);
	fprintf(f, "     <Topology TopologyType=\"3DCoRectMesh\" Dimensions=\"%d %d %d\"/>\n",
		im[2] + 1, im[1] + 1, im[0] + 1);
	fprintf(f, "     <Geometry GeometryType=\"Origin_DxDyDz\">\n");
	fprintf(f, "     <DataStructure Name=\"Origin\" DataType=\"Float\" Dimensions=\"3\" Format=\"XML\">\n");
	fprintf(f, "        %g %g %g\n",
		(im[2] * kz) * psc.dx[2],
		(im[1] * ky) * psc.dx[1],
		(im[0] * kx) * psc.dx[0]);
	fprintf(f, "     </DataStructure>\n");
	fprintf(f, "     <DataStructure Name=\"Spacing\" DataType=\"Float\" Dimensions=\"3\" Format=\"XML\">\n");
	fprintf(f, "        %g %g %g\n", psc.dx[2], psc.dx[1], psc.dx[0]);
	fprintf(f, "     </DataStructure>\n");
	fprintf(f, "     </Geometry>\n");
	fprintf(f, "\n");
	for (int m = 0; m < list->nr_flds; m++) {
	  fld = &list->flds[m]->f[0];
	  fprintf(f, "     <Attribute Name=\"%s\" AttributeType=\"Scalar\" Center=\"Cell\">\n",
		  fld->name[0]);
	  fprintf(f, "       <DataItem Dimensions=\"%d %d %d\" NumberType=\"Float\" Precision=\"4\" Format=\"HDF\">\n",
		  im[2], im[1], im[0]);
	  int proc = (kz * np[1] + ky) * np[0] + kx;
	  fprintf(f, "        %s_%06d_%07d.h5:/psc/fields/%s\n",
		  pfx, proc, psc.timestep, fld->name[0]);
	  fprintf(f, "       </DataItem>\n");
	  fprintf(f, "     </Attribute>\n");
	}
	fprintf(f, "   </Grid>\n");
      }
    }
  }
  fprintf(f, "</Grid>\n");
  fprintf(f, "</Domain>\n");
  fprintf(f, "</Xdmf>\n");
  fclose(f);
}

static void
xdmf_write_temporal_collection(struct psc_output_fields_c *out, const char *pfx)
{
  // It'd be easier to create those files line by line as time goes by.
  // However, then we won't be able to get the timeseries into Paraview
  // until the solution is all finished.
  // So this version rewrites the xdmf file completely every timestep,
  // which needs however some way to figure out what times we've written
  // before.
  char fname[strlen(out->data_dir) + strlen(pfx) + 20];
  sprintf(fname, "%s/%s.xdmf", out->data_dir, pfx);
  FILE *f = fopen(fname, "w");

  fprintf(f, "<?xml version='1.0' ?>\n");
  fprintf(f, "<Xdmf xmlns:xi='http://www.w3.org/2001/XInclude' Version='2.0'>\n");
  fprintf(f, "<Domain>\n");
  fprintf(f, "  <Grid GridType='Collection' CollectionType='Temporal'>\n");
  int first, step;
  if (strcmp(pfx, "pfd") == 0) {
    first = out->pfield_first;
    step = out->pfield_step;
  } else {
    first = out->tfield_first;
    step = out->tfield_step;
  }
  for (int i = first; i <= psc.timestep; i += step) {
    fprintf(f, "  <xi:include href='%s_%07d.xdmf' xpointer='xpointer(//Xdmf/Domain/Grid)'/>\n", pfx, i);
  }
  fprintf(f, "  </Grid>\n");
  fprintf(f, "  </Domain>\n");
  fprintf(f, "</Xdmf>\n");
  fclose(f);
}

static void
psc_output_format_xdmf_write_fields(struct psc_output_format *format,
				    struct psc_output_fields_c *out,
				    struct psc_fields_list *list,
				    const char *pfx)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  char filename[strlen(out->data_dir) + 30];
  sprintf(filename, "%s/%s_%06d_%07d.h5", out->data_dir, pfx, rank, psc.timestep);
  struct hdf5_ctx hdf5;
  hdf5_open(out, list, filename, &hdf5);

  for (int m = 0; m < list->nr_flds; m++) {
    hdf5_write_field(&hdf5, &list->flds[m]->f[0]);
  }
  hdf5_close(&hdf5);

  if (rank == 0) {
    xdmf_write_spatial_collection(out, list, pfx);
    xdmf_write_temporal_collection(out, pfx);
  }
}

// ======================================================================
// psc_output_format: subclass "xdmf"

struct psc_output_format_ops psc_output_format_xdmf_ops = {
  .name                  = "xdmf",
  .write_fields          = psc_output_format_xdmf_write_fields,
};

// ======================================================================
// hdf5_dump_field
//
// dumps the local fields to a file

// FIXME still assuming fortran layout for buf

#if 0
static void
hdf5_dump_field(int m, const char *fname)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  char *filename = malloc(strlen(fname) + 10);
  sprintf(filename, "%s-p%d.h5", fname, rank);
  mpi_printf(MPI_COMM_WORLD, "hdf5_dump_field: '%s'\n", filename);
  
  hsize_t dims[3] = { psc.ihg[2] - psc.ilg[2],
		      psc.ihg[1] - psc.ilg[1],
		      psc.ihg[0] - psc.ilg[0] };

  hid_t file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  hid_t group = H5Gcreate(file, "psc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  hid_t group_fld = H5Gcreate(group, "fields", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  assert(sizeof(fields_base_real_t) == sizeof(double));
  H5LTmake_dataset_double(group_fld, fldname[m], 3, dims,
			  &F3_BASE(&psc.pf, m, -psc.ibn[0], -psc.ibn[1], -psc.ibn[2]));
  H5Gclose(group_fld);
  H5Gclose(group);
  H5Fclose(file);
}
#endif
