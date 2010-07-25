
#include "psc.h"
#include "util/profile.h"
#include "util/params.h"
#include "output_fields.h"

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
hdf5_open(struct psc_fields_list *list, const char *prefix, struct hdf5_ctx **ctx)
{
  struct hdf5_ctx *hdf5 = malloc(sizeof(*hdf5));

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  char filename[200]; // FIXME
  sprintf(filename, "data/%s_%06d_%07d.h5", prefix, rank, psc.timestep);

  hdf5->file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  hdf5->group = H5Gcreate(hdf5->file, "psc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  H5LTset_attribute_int(hdf5->group, ".", "timestep", &psc.timestep, 1);
  H5LTset_attribute_double(hdf5->group, ".", "dt", &psc.dt, 1);
  H5LTset_attribute_double(hdf5->group, ".", "dx", psc.dx, 3);
  H5LTset_attribute_int(hdf5->group, ".", "lo", psc.glo, 3);
  H5LTset_attribute_int(hdf5->group, ".", "hi", psc.ghi, 3);

  hdf5->group_fld = H5Gcreate(hdf5->group, "fields",
			      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

  *ctx = hdf5;
}

static void
hdf5_close(struct hdf5_ctx *hdf5)
{
  H5Gclose(hdf5->group_fld);
  H5Gclose(hdf5->group);
  H5Fclose(hdf5->file);
}

static void
hdf5_write_field(struct hdf5_ctx *hdf5, struct psc_field *fld)
{
  hsize_t dims[3];
  for (int d = 0; d < 3; d++) {
    // reverse dimensions because of Fortran order
    dims[d] = fld->ihi[2-d] - fld->ilo[2-d];
  }
  
  H5LTmake_dataset_float(hdf5->group_fld, fld->name, 3, dims, fld->data);
  H5LTset_attribute_int(hdf5->group_fld, fld->name, "lo", fld->ilo, 3);
  H5LTset_attribute_int(hdf5->group_fld, fld->name, "hi", fld->ihi, 3);
}

// ======================================================================

struct psc_hdf5 {
  int field_next_out;
  int field_step;
};

#define VAR(x) (void *)offsetof(struct psc_hdf5, x)

static struct param psc_hdf5_descr[] = {
  { "field_first_out"    , VAR(field_next_out)       , PARAM_INT(0)        },
  { "field_step_out"     , VAR(field_step)           , PARAM_INT(10)        },
  {},
};

#undef VAR

static struct psc_hdf5 psc_hdf5;

static void hdf5_out_create(void)
{ 
  params_parse_cmdline(&psc_hdf5, psc_hdf5_descr, "PSC HDF5", MPI_COMM_WORLD);
  params_print(&psc_hdf5, psc_hdf5_descr, "PSC HDF5", MPI_COMM_WORLD);
};

static void
copy_to_global(float *fld, f_real *buf, int *ilo, int *ihi, int *ilg, int *img)
{
  int my = psc.ghi[1] - psc.glo[1];
  int mx = psc.ghi[0] - psc.glo[0];

  for (int iz = ilo[2]; iz < ihi[2]; iz++) {
    for (int iy = ilo[1]; iy < ihi[1]; iy++) {
      for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	fld[((iz - psc.glo[2]) * my + iy - psc.glo[1]) * mx + ix - psc.glo[0]] =
	  buf[((iz - ilg[2]) * img[1] + iy - ilg[1]) * img[0] + ix - ilg[0]];
      }
    }
  }
}

static void
hdf5_out_field()
{
  if (psc.timestep < psc_hdf5.field_next_out) {
    return;
  }
  psc_hdf5.field_next_out += psc_hdf5.field_step;

  static int pr;
  if (!pr) {
    pr = prof_register("hdf5_out_field", 1., 0, 0);
  }
  prof_start(pr);
  MPI_Comm comm = MPI_COMM_WORLD;
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);
  
  hid_t file = -1, group = -1, group_fld = -1;
  float *fld = NULL;
  hsize_t dims[3] = { psc.ghi[2] - psc.glo[2],
		      psc.ghi[1] - psc.glo[1],
		      psc.ghi[0] - psc.glo[0] };
  if (rank == 0) {
    char datadir[] = ".";
    char filename[strlen(datadir) + 20];
    sprintf(filename, "%s/field_%09d.h5", datadir, psc.timestep);
    printf("[%d] hdf5_out_field: %s\n", rank, filename);
    file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    group = H5Gcreate(file, "psc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5LTset_attribute_int(group, ".", "timestep", &psc.timestep, 1);
    H5LTset_attribute_double(group, ".", "dt", &psc.dt, 1);
    H5LTset_attribute_double(group, ".", "dx", psc.dx, 3);
    H5LTset_attribute_int(group, ".", "lo", psc.glo, 3);
    H5LTset_attribute_int(group, ".", "hi", psc.ghi, 3);

    group_fld = H5Gcreate(group, "fields", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    fld = calloc(dims[0] * dims[1] * dims[2], sizeof(float));
  }

  /* printf("glo %d %d %d ghi %d %d %d\n", glo[0], glo[1], glo[2], */
  /* 	     ghi[0], ghi[1], ghi[2]); */

  for (int m = 0; m < NR_FIELDS; m++) {
    if (rank != 0) {
      MPI_Send(psc.ilo, 3, MPI_INT, 0, 100, MPI_COMM_WORLD);
      MPI_Send(psc.ihi, 3, MPI_INT, 0, 101, MPI_COMM_WORLD);
      MPI_Send(psc.ilg, 3, MPI_INT, 0, 102, MPI_COMM_WORLD);
      MPI_Send(psc.img, 3, MPI_INT, 0, 103, MPI_COMM_WORLD);
      int ntot = psc.img[0] * psc.img[1] * psc.img[2];
      MPI_Send(psc.f_fields[m], ntot, MPI_DOUBLE, 0, 104, MPI_COMM_WORLD);
    } else { // rank == 0
      for (int n = 0; n < size; n++) {
	int ilo[3], ihi[3], ilg[3], img[3];
	f_real *buf;
	
	if (n == 0) {
	  for (int d = 0; d < 3; d++) {
	    ilo[d] = psc.ilo[d];
	    ihi[d] = psc.ihi[d];
	    ilg[d] = psc.ilg[d];
	    img[d] = psc.img[d];
	  }
	  buf = psc.f_fields[m];
	} else {
	  MPI_Recv(ilo, 3, MPI_INT, n, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  MPI_Recv(ihi, 3, MPI_INT, n, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  MPI_Recv(ilg, 3, MPI_INT, n, 102, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  MPI_Recv(img, 3, MPI_INT, n, 103, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  int ntot = img[0] * img[1] * img[2];
	  buf = calloc(ntot, sizeof(*buf));
	  MPI_Recv(buf, ntot, MPI_DOUBLE, n, 104, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	/* printf("[%d] ilo %d %d %d ihi %d %d %d\n", rank, ilo[0], ilo[1], ilo[2], */
	/*        ihi[0], ihi[1], ihi[2]); */
	copy_to_global(fld, buf, ilo, ihi, ilg, img);
	if (n != 0) {
	  free(buf);
	}
      }
      H5LTmake_dataset_float(group_fld, fldname[m], 3, dims, fld);
    }
  }

  if (rank == 0) {
    free(fld);
    H5Gclose(group_fld);
    H5Gclose(group);
    H5Fclose(file);
  }

  prof_stop(pr);
}

struct psc_output_ops psc_output_ops_hdf5 = {
  .name      = "hdf5",
  .create    = hdf5_out_create,
  .out_field = hdf5_out_field,
};

// ======================================================================

static void
hdf5_write_fields(struct psc_fields_list *list, const char *prefix)
{
  struct hdf5_ctx *hdf5;

  hdf5_open(list, prefix, &hdf5);

  for (int m = 0; m < list->nr_flds; m++) {
    hdf5_write_field(hdf5, &list->flds[m]);
  }
  
  hdf5_close(hdf5);
}

struct psc_output_format_ops psc_output_format_ops_hdf5 = {
  .name         = "hdf5",
  .write_fields = hdf5_write_fields,
};

