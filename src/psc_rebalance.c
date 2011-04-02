
#include "psc.h"
#include "psc_output_fields.h"
#include "psc_bnd.h"

#include <stdlib.h>

static void
psc_get_loads(struct psc *psc, double *loads, int *nr_particles_by_patch)
{
  psc_foreach_patch(psc, p) {
    loads[p] = nr_particles_by_patch[p];
  }
}

static int
find_best_mapping(struct mrc_domain *domain, int nr_global_patches, double *loads_all)
{
  MPI_Comm comm = mrc_domain_comm(domain);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  int *nr_patches_all_new = NULL;

  if (rank == 0) { // do the mapping on proc 0
    nr_patches_all_new = calloc(size, sizeof(*nr_patches_all_new));
    double loads_sum = 0.;
    for (int i = 0; i < nr_global_patches; i++) {
      loads_sum += loads_all[i];
    }
    double load_target = loads_sum / size;
    
    int p = 0, nr_new_patches = 0;
    double load = 0.;
    for (int i = 0; i < nr_global_patches; i++) {
      load += loads_all[i];
      nr_new_patches++;
      double next_target = (p + 1) * load_target;
      if (p < size - 1) {
	if (load > next_target) {
	  double above_target = load - next_target;
	  double below_target = next_target - (load - loads_all[i-1]);
	  if (above_target > below_target) {
	    nr_patches_all_new[p] = nr_new_patches - 1;
	    nr_new_patches = 1;
	  } else {
	    nr_patches_all_new[p] = nr_new_patches;
	    nr_new_patches = 0;
	  }
	  p++;
	}
      }
      // last proc takes what's left
      if (i == nr_global_patches - 1) {
	nr_patches_all_new[size - 1] = nr_new_patches;
      }
    }
    
    int pp = 0;
    for (int p = 0; p < size; p++) {
      double load = 0.;
      for (int i = 0; i < nr_patches_all_new[p]; i++) {
	load += loads_all[pp++];
      }
      mprintf("p %d # = %d load %g / %g : %g\n", p, nr_patches_all_new[p],
	      load, load_target, load - load_target);
    }
  }
  // then scatter
  int nr_patches_new;
  MPI_Scatter(nr_patches_all_new, 1, MPI_INT, &nr_patches_new, 1, MPI_INT,
	      0, comm);
  free(nr_patches_all_new);
  return nr_patches_new;
}

static double *
gather_loads(struct mrc_domain *domain, double *loads, int nr_patches,
	     int *p_nr_global_patches)
{
  MPI_Comm comm = mrc_domain_comm(domain);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  // gather nr_patches for all procs on proc 0
  int *nr_patches_all = NULL;
  if (rank == 0) {
    nr_patches_all = calloc(size, sizeof(*nr_patches_all));
  }
  MPI_Gather(&nr_patches, 1, MPI_INT, nr_patches_all, 1, MPI_INT, 0, comm);

  // gather loads for all patches on proc 0
  int *displs = NULL;
  double *loads_all = NULL;
  if (rank == 0) {
    displs = calloc(size, sizeof(*displs));
    int off = 0;
    for (int i = 0; i < size; i++) {
      displs[i] = off;
      off += nr_patches_all[i];
    }
    mrc_domain_get_nr_global_patches(domain, p_nr_global_patches);
    loads_all = calloc(*p_nr_global_patches, sizeof(*loads_all));
  }
  MPI_Gatherv(loads, nr_patches, MPI_DOUBLE, loads_all, nr_patches_all, displs,
	      MPI_DOUBLE, 0, comm);

  if (rank == 0) {
    free(nr_patches_all);
    free(displs);
  }

  return loads_all;
}

static void
communicate_new_nr_particles(struct mrc_domain *domain_old,
			     struct mrc_domain *domain_new, int **p_nr_particles_by_patch)
{
  MPI_Comm comm = mrc_domain_comm(domain_new);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  int nr_patches_old, nr_patches_new;
  mrc_domain_get_patches(domain_old, &nr_patches_old);
  mrc_domain_get_patches(domain_new, &nr_patches_new);

  int *nr_particles_by_patch_old = *p_nr_particles_by_patch;
  int *nr_particles_by_patch_new = calloc(nr_patches_new,
					  sizeof(nr_particles_by_patch_new));

  MPI_Request *send_reqs = calloc(nr_patches_old, sizeof(*send_reqs));
  // send info from old local patches
  for (int p = 0; p < nr_patches_old; p++) {
    struct mrc_patch_info info, info_new;
    mrc_domain_get_local_patch_info(domain_old, p, &info);
    mrc_domain_get_global_patch_info(domain_new, info.global_patch, &info_new);
    if (info_new.rank == rank) {
      nr_particles_by_patch_new[info_new.patch] = nr_particles_by_patch_old[p];
      send_reqs[p] = MPI_REQUEST_NULL;
    } else {
      MPI_Isend(&nr_particles_by_patch_old[p], 1, MPI_INT, info_new.rank,
		info.global_patch, comm, &send_reqs[p]);
    }
  }
  // recv info for new local patches
  MPI_Request *recv_reqs = calloc(nr_patches_new, sizeof(*recv_reqs));
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank == rank) {
      recv_reqs[p] = MPI_REQUEST_NULL;
    } else {
      MPI_Irecv(&nr_particles_by_patch_new[p], 1, MPI_INT, info_old.rank, info.global_patch,
		comm, &recv_reqs[p]);
    }
  }
  
  MPI_Waitall(nr_patches_old, send_reqs, MPI_STATUSES_IGNORE);
  MPI_Waitall(nr_patches_new, recv_reqs, MPI_STATUSES_IGNORE);
  free(send_reqs);
  free(recv_reqs);

  free(*p_nr_particles_by_patch);
  *p_nr_particles_by_patch = nr_particles_by_patch_new;
}

void
psc_rebalance_initial(struct psc *psc, int **p_nr_particles_by_patch)
{
  struct mrc_domain *domain_old = psc->mrc_domain;

  int nr_patches;
  mrc_domain_get_patches(domain_old, &nr_patches);
  double *loads = calloc(nr_patches, sizeof(*loads));
  psc_get_loads(psc, loads, *p_nr_particles_by_patch);

  int nr_global_patches;
  double *loads_all = gather_loads(domain_old, loads, nr_patches,
				   &nr_global_patches);
  free(loads);

  int nr_patches_new = find_best_mapping(domain_old, nr_global_patches,
					 loads_all);
  free(loads_all);

  free(psc->patch);
  struct mrc_domain *domain_new = psc_setup_mrc_domain(psc, nr_patches_new);
  //  mrc_domain_view(domain_new);

  communicate_new_nr_particles(domain_old, domain_new, p_nr_particles_by_patch);

  mrc_domain_destroy(domain_old);
  psc->mrc_domain = domain_new;

  // FIXME, this shouldn't be necessary here, the other modules oughta learn
  // to adapt automatically...
  psc_output_fields_destroy(psc->output_fields);
  psc->output_fields = psc_output_fields_create(MPI_COMM_WORLD);
  psc_output_fields_set_from_options(psc->output_fields);
  psc_output_fields_setup(psc->output_fields);

  psc_bnd_destroy(psc->bnd);
  psc->bnd = psc_bnd_create(MPI_COMM_WORLD);
  psc_bnd_set_from_options(psc->bnd);
  psc_bnd_setup(psc->bnd);
}

