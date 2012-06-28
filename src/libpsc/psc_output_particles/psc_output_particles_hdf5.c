
#include "psc_output_particles_private.h"
#include "psc_particles_c.h"

#include <mrc_params.h>
#include <mrc_profile.h>
#include <string.h>

#include <hdf5.h>
#include <hdf5_hl.h>
#include <string.h>

// when changing the following struct, the HDF5 compound data type prt_type
// needs to be changed accordingly

struct hdf5_prt {
  float x, y, z;
  float px, py, pz;
  float q, m, w;
};

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define to_psc_output_particles_hdf5(out) \
  mrc_to_subobj(out, struct psc_output_particles_hdf5)

struct psc_output_particles_hdf5 {
  // parameters
  const char *data_dir;
  const char *basename;
  int every_step;
  int lo[3];
  int hi[3];
  bool use_independent_io;
  char *romio_cb_write;
  char *romio_ds_write;

  // internal
  hid_t prt_type;
  int wdims[3]; // dimensions of the subdomain we're actually writing
};

#define VAR(x) (void *)offsetof(struct psc_output_particles_hdf5, x)
static struct param psc_output_particles_hdf5_descr[] = {
  { "data_dir"           , VAR(data_dir)             , PARAM_STRING(".")       },
  { "basename"           , VAR(basename)             , PARAM_STRING("prt")     },
  { "every_step"         , VAR(every_step)           , PARAM_INT(-1)           },
  { "lo"                 , VAR(lo)                   , PARAM_INT3(0, 0, 0)     },
  { "hi"                 , VAR(hi)                   , PARAM_INT3(0, 0, 0)     },
  { "use_independent_io" , VAR(use_independent_io)   , PARAM_BOOL(false)       },
  { "romio_cb_write"     , VAR(romio_cb_write)       , PARAM_STRING(NULL)      },
  { "romio_ds_write"     , VAR(romio_ds_write)       , PARAM_STRING(NULL)      },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_output_particles_hdf5_create

static void
psc_output_particles_hdf5_create(struct psc_output_particles *out)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);

  hid_t id = H5Tcreate(H5T_COMPOUND, sizeof(struct hdf5_prt));
  H5Tinsert(id, "x" , HOFFSET(struct hdf5_prt, x) , H5T_NATIVE_FLOAT);
  H5Tinsert(id, "y" , HOFFSET(struct hdf5_prt, y) , H5T_NATIVE_FLOAT);
  H5Tinsert(id, "z" , HOFFSET(struct hdf5_prt, z) , H5T_NATIVE_FLOAT);
  H5Tinsert(id, "px", HOFFSET(struct hdf5_prt, px), H5T_NATIVE_FLOAT);
  H5Tinsert(id, "py", HOFFSET(struct hdf5_prt, py), H5T_NATIVE_FLOAT);
  H5Tinsert(id, "pz", HOFFSET(struct hdf5_prt, pz), H5T_NATIVE_FLOAT);
  H5Tinsert(id, "q" , HOFFSET(struct hdf5_prt, q) , H5T_NATIVE_FLOAT);
  H5Tinsert(id, "m" , HOFFSET(struct hdf5_prt, m) , H5T_NATIVE_FLOAT);
  H5Tinsert(id, "w" , HOFFSET(struct hdf5_prt, w) , H5T_NATIVE_FLOAT);
  
  hdf5->prt_type = id;
}

// ----------------------------------------------------------------------
// psc_output_particles_hdf5_setup

static void
psc_output_particles_hdf5_setup(struct psc_output_particles *out)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);

  // set hi to gdims by default (if not set differently before)
  // and calculate wdims (global dims of region we're writing)
  for (int d = 0; d < 3; d++) {
    assert(hdf5->lo[d] >= 0);
    if (hdf5->hi[d] == 0) {
      hdf5->hi[d] = ppsc->domain.gdims[d];
    }
    assert(hdf5->hi[d] <= ppsc->domain.gdims[d]);
    hdf5->wdims[d] = hdf5->hi[d] - hdf5->lo[d];
  }
}

// ----------------------------------------------------------------------
// psc_output_particles_hdf5_destroy

static void
psc_output_particles_hdf5_destroy(struct psc_output_particles *out)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);

  H5Tclose(hdf5->prt_type);
}

// ----------------------------------------------------------------------
// get_cell_index
// FIXME, lots of stuff here is pretty much duplicated from countsort2

static inline int
cell_index_3_to_1(int *ldims, int j0, int j1, int j2)
{
  return ((j2) * ldims[1] + j1) * ldims[0] + j0;
}

static inline int
get_sort_index(int p, const particle_c_t *part)
{
  struct psc_patch *patch = &ppsc->patch[p];
  particle_c_real_t dxi = 1.f / ppsc->dx[0];
  particle_c_real_t dyi = 1.f / ppsc->dx[1];
  particle_c_real_t dzi = 1.f / ppsc->dx[2];
  int *ldims = patch->ldims;
  
  particle_c_real_t u = part->xi * dxi;
  particle_c_real_t v = part->yi * dyi;
  particle_c_real_t w = part->zi * dzi;
  int j0 = particle_c_real_fint(u);
  int j1 = particle_c_real_fint(v);
  int j2 = particle_c_real_fint(w);
  assert(j0 >= 0 && j0 < ldims[0]);
  assert(j1 >= 0 && j1 < ldims[1]);
  assert(j2 >= 0 && j2 < ldims[2]);

  int kind;
  if (part->qni < 0.) {
    kind = 0; // electron
  } else if (part->qni > 0.) {
    kind = 1; // ion
  } else {
    kind = 2; // neutral
  }
  assert(kind < ppsc->nr_kinds);
 
  return cell_index_3_to_1(ldims, j0, j1, j2) * ppsc->nr_kinds + kind;
}

// ----------------------------------------------------------------------
// count_sort

static void
count_sort(struct psc_mparticles *particles_base, int **off, int **map)
{
  int nr_kinds = ppsc->nr_kinds;

  for (int p = 0; p < particles_base->nr_patches; p++) {
    int *ldims = ppsc->patch[p].ldims;
    int nr_indices = ldims[0] * ldims[1] * ldims[2] * nr_kinds;
    off[p] = calloc(nr_indices + 1, sizeof(*off[p]));
    struct psc_particles *prts_base = psc_mparticles_get_patch(particles_base, p);
    struct psc_particles *prts = psc_particles_get_as(prts_base, "c", 0);

    // counting sort to get map 
    for (int n = 0; n < prts->n_part; n++) {
      particle_c_t *part = particles_c_get_one(prts, n);
      int si = get_sort_index(p, part);
      off[p][si]++;
    }
    // prefix sum to get offsets
    int o = 0;
    int *off2 = malloc((nr_indices + 1) * sizeof(*off2));
    for (int si = 0; si <= nr_indices; si++) {
      int cnt = off[p][si];
      off[p][si] = o; // this will be saved for later
      off2[si] = o; // this one will overwritten when making the map
      o += cnt;
    }

    // sort a map only, not the actual particles
    map[p] = malloc(prts->n_part * sizeof(*map[p]));
    for (int n = 0; n < prts->n_part; n++) {
      particle_c_t *part = particles_c_get_one(prts, n);
      int si = get_sort_index(p, part);
      map[p][off2[si]++] = n;
    }
    free(off2);
    psc_particles_put_as(prts, prts_base, MP_DONT_COPY);
  }  
}

// ----------------------------------------------------------------------
// 

static void
find_patch_bounds(struct psc_output_particles_hdf5 *hdf5,
		  struct mrc_patch_info *info,
		  int ilo[3], int ihi[3], int ld[3], int *p_sz)
{
  int *ldims = info->ldims, *off = info->off;

  for (int d = 0; d < 3; d++) {
    ilo[d] = MAX(0, hdf5->lo[d] - off[d]);
    ihi[d] = MIN(ldims[d], hdf5->hi[d] - off[d]);
    ld[d] = ihi[d] - ilo[d];
  }
  *p_sz = ppsc->nr_kinds * ld[0] * ld[1] * ld[2];
}

// ----------------------------------------------------------------------
// make_local_particle_array

static struct hdf5_prt *
make_local_particle_array(struct psc_output_particles *out,
			  struct psc_mparticles *particles_base, int **off, int **map,
			  int **idx, int *p_n_write, int *p_n_off, int *p_n_total)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);
  MPI_Comm comm = psc_output_particles_comm(out);
  int nr_kinds = ppsc->nr_kinds;
  struct mrc_patch_info info;

  // count all particles to be written locally
  int n_write = 0;
  for (int p = 0; p < particles_base->nr_patches; p++) {
    mrc_domain_get_local_patch_info(ppsc->mrc_domain, p, &info);
    int ilo[3], ihi[3], ld[3], sz;
    find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
    for (int jz = ilo[2]; jz < ihi[2]; jz++) {
      for (int jy = ilo[1]; jy < ihi[1]; jy++) {
	for (int jx = ilo[0]; jx < ihi[0]; jx++) {
	  for (int kind = 0; kind < nr_kinds; kind++) {
	    int si = cell_index_3_to_1(info.ldims, jx, jy, jz) * nr_kinds + kind;
	    n_write += off[p][si+1] - off[p][si];
	  }
	}
      }
    }
  }

  int n_total, n_off = 0;
  MPI_Allreduce(&n_write, &n_total, 1, MPI_INT, MPI_SUM, comm);
  MPI_Exscan(&n_write, &n_off, 1, MPI_INT, MPI_SUM, comm);

  struct hdf5_prt *arr = malloc(n_write * sizeof(*arr));

  // copy particles to be written into temp array
  int nn = 0;
  for (int p = 0; p < particles_base->nr_patches; p++) {
    struct psc_particles *prts_base = psc_mparticles_get_patch(particles_base, p);
    struct psc_particles *prts = psc_particles_get_as(prts_base, "c", 0);
    mrc_domain_get_local_patch_info(ppsc->mrc_domain, p, &info);
    int ilo[3], ihi[3], ld[3], sz;
    find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
    idx[p] = malloc(2 * sz * sizeof(*idx));

    for (int jz = ilo[2]; jz < ihi[2]; jz++) {
      for (int jy = ilo[1]; jy < ihi[1]; jy++) {
	for (int jx = ilo[0]; jx < ihi[0]; jx++) {
	  for (int kind = 0; kind < nr_kinds; kind++) {
	    int si = cell_index_3_to_1(info.ldims, jx, jy, jz) * nr_kinds + kind;
	    int jj = ((kind * ld[2] + jz - ilo[2])
		      * ld[1] + jy - ilo[1]) * ld[0] + jx - ilo[0];
	    idx[p][jj     ] = nn + n_off;
	    idx[p][jj + sz] = nn + n_off + off[p][si+1] - off[p][si];
	    for (int n = off[p][si]; n < off[p][si+1]; n++, nn++) {
	      particle_c_t *part = particles_c_get_one(prts, map[p][n]);
	      arr[nn].x  = part->xi;
	      arr[nn].y  = part->yi;
	      arr[nn].z  = part->zi;
	      arr[nn].px = part->pxi;
	      arr[nn].py = part->pyi;
	      arr[nn].pz = part->pzi;
	      arr[nn].q  = part->qni;
	      arr[nn].m  = part->mni;
	      arr[nn].w  = part->wni;
	    }
	  }
	}
      }
    }
    psc_particles_put_as(prts, prts_base, MP_DONT_COPY);
  }

  *p_n_write = n_write;
  *p_n_off = n_off;
  *p_n_total = n_total;
  return arr;
}

static void
write_particles(struct psc_output_particles *out, int n_write, int n_off,
		int n_total, struct hdf5_prt *arr, hid_t group, hid_t dxpl)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);
  int ierr;

  hsize_t mdims[1] = { n_write };
  hsize_t fdims[1] = { n_total };
  hsize_t foff[1] = { n_off };
  hid_t memspace = H5Screate_simple(1, mdims, NULL); H5_CHK(memspace);
  hid_t filespace = H5Screate_simple(1, fdims, NULL); H5_CHK(filespace);
  ierr = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, foff, NULL,
			       mdims, NULL); CE;
  
  hid_t dset = H5Dcreate(group, "1d", hdf5->prt_type, filespace, H5P_DEFAULT,
			 H5P_DEFAULT, H5P_DEFAULT); H5_CHK(dset);
  ierr = H5Dwrite(dset, hdf5->prt_type, memspace, filespace, dxpl, arr); CE;
  
  ierr = H5Dclose(dset); CE;
  ierr = H5Sclose(filespace); CE;
  ierr = H5Sclose(memspace); CE;
}

static void
write_idx(struct psc_output_particles *out, int *gidx_begin, int *gidx_end,
	  hid_t group, hid_t dxpl)
{
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);
  int ierr;

  hsize_t fdims[4] = { ppsc->nr_kinds,
		       hdf5->wdims[2], hdf5->wdims[1], hdf5->wdims[0] };
  hid_t filespace = H5Screate_simple(4, fdims, NULL); H5_CHK(filespace);
  hid_t memspace;

  int rank;
  MPI_Comm_rank(psc_output_particles_comm(out), &rank);
  if (rank == 0) {
    memspace = H5Screate_simple(4, fdims, NULL); H5_CHK(memspace);
  } else {
    memspace = H5Screate(H5S_NULL);
    H5Sselect_none(memspace);
    H5Sselect_none(filespace);
  }

  hid_t dset = H5Dcreate(group, "idx_begin", H5T_NATIVE_INT, filespace,
			 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); H5_CHK(dset);
  ierr = H5Dwrite(dset, H5T_NATIVE_INT, memspace, filespace, dxpl, gidx_begin); CE;
  ierr = H5Dclose(dset); CE;

  dset = H5Dcreate(group, "idx_end", H5T_NATIVE_INT, filespace,
			 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); H5_CHK(dset);
  ierr = H5Dwrite(dset, H5T_NATIVE_INT, memspace, filespace, dxpl, gidx_end); CE;
  ierr = H5Dclose(dset); CE;

  ierr = H5Sclose(filespace); CE;
  ierr = H5Sclose(memspace); CE;
}

// ----------------------------------------------------------------------
// psc_output_particles_hdf5_run

static void
psc_output_particles_hdf5_run(struct psc_output_particles *out,
			      mparticles_base_t *particles_base)
{
  // OPT: this is not optimal in that it will convert particles to "c" twice,
  // though that only matters if particle type isn't "c" to start with.
  struct psc_output_particles_hdf5 *hdf5 = to_psc_output_particles_hdf5(out);
  MPI_Comm comm = psc_output_particles_comm(out);
  int ierr;

  static int pr_A, pr_B, pr_C;
  if (!pr_A) {
    pr_A = prof_register("outp: local", 1., 0, 0);
    pr_B = prof_register("outp: comm", 1., 0, 0);
    pr_C = prof_register("outp: write", 1., 0, 0);
  }

  if (hdf5->every_step < 0 ||
      ppsc->timestep % hdf5->every_step != 0) {
    return;
  }

  prof_start(pr_A);
  int rank;
  MPI_Comm_rank(comm, &rank);

  struct mrc_patch_info info;
  int nr_kinds = ppsc->nr_kinds;
  int *wdims = hdf5->wdims;

  int **off = malloc(particles_base->nr_patches * sizeof(*off));
  int **map = malloc(particles_base->nr_patches * sizeof(*off));

  count_sort(particles_base, map, off);

  int **idx = malloc(particles_base->nr_patches * sizeof(*idx));

  int *gidx_begin = NULL, *gidx_end = NULL;
  if (rank == 0) {
    // alloc global idx array
    gidx_begin = malloc(nr_kinds * wdims[0]*wdims[1]*wdims[2] * sizeof(*gidx_begin));
    gidx_end = malloc(nr_kinds * wdims[0]*wdims[1]*wdims[2] * sizeof(*gidx_end));
    for (int jz = 0; jz < wdims[2]; jz++) {
      for (int jy = 0; jy < wdims[1]; jy++) {
	for (int jx = 0; jx < wdims[0]; jx++) {
	  for (int kind = 0; kind < nr_kinds; kind++) {
	    int ii = ((kind * wdims[2] + jz) * wdims[1] + jy) * wdims[0] + jx;
	    gidx_begin[ii] = -1;
	    gidx_end[ii] = -1;
	  }
	}
      }
    }
  }

  // find local particle and idx arrays
  int n_write, n_off, n_total;
  struct hdf5_prt *arr =
    make_local_particle_array(out, particles_base, map, off, idx,
			      &n_write, &n_off, &n_total);
  prof_stop(pr_A);

  prof_start(pr_B);
  if (rank == 0) {
    int nr_global_patches;
    mrc_domain_get_nr_global_patches(ppsc->mrc_domain, &nr_global_patches);

    MPI_Request *recv_reqs = malloc(nr_global_patches * sizeof(*recv_reqs));
    int **recv_bufs = calloc(nr_global_patches, sizeof(*recv_bufs));
    for (int p = 0; p < nr_global_patches; p++) {
      mrc_domain_get_global_patch_info(ppsc->mrc_domain, p, &info);
      if (info.rank == rank) { // skip local patches
	recv_reqs[p] = MPI_REQUEST_NULL;
	continue;
      }
      int ilo[3], ihi[3], ld[3], sz;
      find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
      recv_bufs[p] = malloc(2 * sz * sizeof(recv_bufs[p]));
      MPI_Irecv(recv_bufs[p], 2 * sz, MPI_INT, info.rank, p, comm,
		&recv_reqs[p]);
    }

    // build global idx array, local part
    for (int p = 0; p < particles_base->nr_patches; p++) {
      mrc_domain_get_local_patch_info(ppsc->mrc_domain, p, &info);
      int ilo[3], ihi[3], ld[3], sz;
      find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
      
      for (int jz = ilo[2]; jz < ihi[2]; jz++) {
	for (int jy = ilo[1]; jy < ihi[1]; jy++) {
	  for (int jx = ilo[0]; jx < ihi[0]; jx++) {
	    for (int kind = 0; kind < nr_kinds; kind++) {
	      int ix = jx + info.off[0];
	      int iy = jy + info.off[1];
	      int iz = jz + info.off[2];
	      int jj = ((kind * ld[2] + jz - ilo[2])
			* ld[1] + jy - ilo[1]) * ld[0] + jx - ilo[0];
	      int ii = ((kind * wdims[2] + iz) * wdims[1] + iy) * wdims[0] + ix;
	      gidx_begin[ii] = idx[p][jj];
	      gidx_end[ii]   = idx[p][jj + sz];
	    }
	  }
	}
      }
    }

    MPI_Waitall(nr_global_patches, recv_reqs, MPI_STATUSES_IGNORE);

    // build global idx array, remote part
    for (int p = 0; p < nr_global_patches; p++) {
      mrc_domain_get_global_patch_info(ppsc->mrc_domain, p, &info);
      if (info.rank == rank) { // skip local patches
	continue;
      }
      int ilo[3], ihi[3], ld[3], sz;
      find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
      for (int jz = ilo[2]; jz < ihi[2]; jz++) {
	for (int jy = ilo[1]; jy < ihi[1]; jy++) {
	  for (int jx = ilo[0]; jx < ihi[0]; jx++) {
	    for (int kind = 0; kind < nr_kinds; kind++) {
	      int ix = jx + info.off[0];
	      int iy = jy + info.off[1];
	      int iz = jz + info.off[2];
	      int jj = ((kind * ld[2] + jz - ilo[2])
			* ld[1] + jy - ilo[1]) * ld[0] + jx - ilo[0];
	      int ii = ((kind * wdims[2] + iz) * wdims[1] + iy) * wdims[0] + ix;
	      gidx_begin[ii] = recv_bufs[p][jj];
	      gidx_end[ii]   = recv_bufs[p][jj + sz];
	    }
	  }
	}
      }
      free(recv_bufs[p]);
    }
    free(recv_bufs);
    free(recv_reqs);
  } else { // rank != 0: send to rank 0
    MPI_Request *send_reqs = malloc(particles_base->nr_patches * sizeof(*send_reqs));
    for (int p = 0; p < particles_base->nr_patches; p++) {
      mrc_domain_get_local_patch_info(ppsc->mrc_domain, p, &info);
      int ilo[3], ihi[3], ld[3], sz;
      find_patch_bounds(hdf5, &info, ilo, ihi, ld, &sz);
      MPI_Isend(idx[p], 2 * sz, MPI_INT, 0, info.global_patch, comm, &send_reqs[p]);
    }
    
    MPI_Waitall(particles_base->nr_patches, send_reqs, MPI_STATUSES_IGNORE);
    free(send_reqs);
  }
  prof_stop(pr_B);

  prof_start(pr_C);
  char filename[strlen(hdf5->data_dir) + strlen(hdf5->basename) + 20];
  sprintf(filename, "%s/%s.%06d_p%06d.h5", hdf5->data_dir,
	  hdf5->basename, ppsc->timestep, 0);

  hid_t plist = H5Pcreate(H5P_FILE_ACCESS);

  MPI_Info mpi_info;
  MPI_Info_create(&mpi_info);
#ifdef H5_HAVE_PARALLEL
  if (hdf5->romio_cb_write) {
    MPI_Info_set(mpi_info, "romio_cb_write", hdf5->romio_cb_write);
  }
  if (hdf5->romio_ds_write) {
    MPI_Info_set(mpi_info, "romio_ds_write", hdf5->romio_ds_write);
  }
  H5Pset_fapl_mpio(plist, comm, mpi_info);
#endif

  hid_t file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, plist); H5_CHK(file);
  H5Pclose(plist);
  MPI_Info_free(&mpi_info);

  hid_t group = H5Gcreate(file, "particles", H5P_DEFAULT,
			  H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
  hid_t groupp = H5Gcreate(group, "p0", H5P_DEFAULT,
			   H5P_DEFAULT, H5P_DEFAULT); H5_CHK(groupp);

  ierr = H5LTset_attribute_int(group, ".", "lo", hdf5->lo, 3); CE;
  ierr = H5LTset_attribute_int(group, ".", "hi", hdf5->hi, 3); CE;

  hid_t dxpl = H5Pcreate(H5P_DATASET_XFER); H5_CHK(dxpl);
#ifdef H5_HAVE_PARALLEL
  if (hdf5->use_independent_io) {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT); CE;
  } else {
    ierr = H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE); CE;
  }
#endif

  write_idx(out, gidx_begin, gidx_end, groupp, dxpl);
  write_particles(out, n_write, n_off, n_total, arr, groupp, dxpl);

  ierr = H5Pclose(dxpl); CE;

  H5Gclose(groupp);
  H5Gclose(group);
  H5Fclose(file);
  prof_stop(pr_C);

  free(arr);
  free(gidx_begin);
  free(gidx_end);

  for (int p = 0; p < particles_base->nr_patches; p++) {
    free(off[p]);
    free(map[p]);
    free(idx[p]);
  }
  free(off);
  free(map);
  free(idx);
}

// ======================================================================
// psc_output_particles: subclass "hdf5"

struct psc_output_particles_ops psc_output_particles_hdf5_ops = {
  .name                  = "hdf5",
  .size                  = sizeof(struct psc_output_particles_hdf5),
  .param_descr           = psc_output_particles_hdf5_descr,
  .create                = psc_output_particles_hdf5_create,
  .setup                 = psc_output_particles_hdf5_setup,
  .destroy               = psc_output_particles_hdf5_destroy,
  .run                   = psc_output_particles_hdf5_run,
};
