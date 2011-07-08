
#include "psc_balance.h"
#include "psc_case.h"
#include "psc_bnd_fields.h"
#include "psc_push_fields.h"

#include <mrc_params.h>
#include <stdlib.h>
#include <string.h>

struct psc_balance {
  struct mrc_obj obj;
  int every;
  int force_update;
};

static void
psc_get_loads_initial(struct psc *psc, double *loads, int *nr_particles_by_patch)
{
  psc_foreach_patch(psc, p) {
    loads[p] = nr_particles_by_patch[p] + 1;
  }
}

static void
psc_get_loads(struct psc *psc, double *loads)
{
  mparticles_base_t *mparticles = psc->particles;

  psc_foreach_patch(psc, p) {
    loads[p] = mparticles->p[p].n_part + 1;
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
    mprintf("target %g size %d sum %g\n", load_target, size, loads_sum);
    
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
	mprintf("  pp %d load %g : %g\n", pp-1, loads_all[pp-1], load);
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

#define MAX(x,y) (x > y ? x : y)

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
	  
	  //HACK If we have a dynamic domain, assume all newly created patches have a fixed load
	  if(ppsc->use_dynamic_patches)
	  {
		  int n_old_patches = *p_nr_global_patches;
		  *p_nr_global_patches = bitfield3d_count_bits_set(ppsc->patchmanager.activepatches);
		  
		  loads_all = calloc(MAX(n_old_patches, *p_nr_global_patches), sizeof(*loads_all));
		  
		  for(int i=n_old_patches; i<*p_nr_global_patches; ++i)
		  {
			  loads_all[i] = 1.;	//TODO Better assumption? Like take the median or sth alike...
		  }
	  }
	  else
	  {
		  loads_all = calloc(*p_nr_global_patches, sizeof(*loads_all));
	  }
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
    } else if ( info_new.rank < 0 ) {	//This patch has been deleted
      //Issue a warning if there will be particles lost
      if(nr_particles_by_patch_old[p] > 0) printf("Warning: losing %d particles in patch deallocation\n", nr_particles_by_patch_old[p]);
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
    } else if ( info_old.rank < 0 ) {
      //TODO Get number of particles
      nr_particles_by_patch_new[p] = psc_case_calc_nr_particles_in_patch(ppsc->patchmanager.currentcase, p);
      recv_reqs[p] = MPI_REQUEST_NULL;
    } else {
      printf("a: rank: %d gp: %d\n", info_old.rank, info.global_patch);
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

static void
communicate_particles(struct mrc_domain *domain_old, struct mrc_domain *domain_new,
		      mparticles_base_t *particles_old, mparticles_base_t *particles_new,
		      int *nr_particles_by_patch_new)
{
  MPI_Comm comm = mrc_domain_comm(domain_new);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  int nr_patches_old, nr_patches_new;
  mrc_domain_get_patches(domain_old, &nr_patches_old);
  mrc_domain_get_patches(domain_new, &nr_patches_new);
  
  for (int p = 0; p < nr_patches_new; p++) {
    particles_new->p[p].n_part = nr_particles_by_patch_new[p];
  }

  MPI_Request *send_reqs = calloc(nr_patches_old, sizeof(*send_reqs));
  // send from old local patches
  for (int p = 0; p < nr_patches_old; p++) {
    struct mrc_patch_info info, info_new;
    mrc_domain_get_local_patch_info(domain_old, p, &info);
    mrc_domain_get_global_patch_info(domain_new, info.global_patch, &info_new);
    if (info_new.rank == rank || info_new.rank < 0) {
      send_reqs[p] = MPI_REQUEST_NULL;
    } else {
      particles_base_t *pp_old = &particles_old->p[p];
      int nn = pp_old->n_part * (sizeof(particle_base_t)  / sizeof(particle_base_real_t));
      MPI_Isend(pp_old->particles, nn, MPI_PARTICLES_BASE_REAL, info_new.rank,
		info.global_patch, comm, &send_reqs[p]);
    }
  }

  // recv for new local patches
  MPI_Request *recv_reqs = calloc(nr_patches_new, sizeof(*recv_reqs));
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank == rank) {
      recv_reqs[p] = MPI_REQUEST_NULL;
    } else if (info_old.rank < 0) {
      recv_reqs[p] = MPI_REQUEST_NULL;
      //TODO Seed particles
    } else {
      particles_base_t *pp_new = &particles_new->p[p];
      int nn = pp_new->n_part * (sizeof(particle_base_t)  / sizeof(particle_base_real_t));
      MPI_Irecv(pp_new->particles, nn, MPI_PARTICLES_BASE_REAL, info_old.rank,
		info.global_patch, comm, &recv_reqs[p]);
    }
  }

  // local particles
  // OPT: could keep the alloced arrays, just move pointers...
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank != rank) {
      continue;
    }

    particles_base_t *pp_old = &particles_old->p[info_old.patch];
    particles_base_t *pp_new = &particles_new->p[p];
    assert(pp_old->n_part == pp_new->n_part);
    for (int n = 0; n < pp_new->n_part; n++) {
      pp_new->particles[n] = pp_old->particles[n];
    }
  }
  
  MPI_Waitall(nr_patches_old, send_reqs, MPI_STATUSES_IGNORE);
  MPI_Waitall(nr_patches_new, recv_reqs, MPI_STATUSES_IGNORE);
  free(send_reqs);
  free(recv_reqs);
}

static void
communicate_fields(struct mrc_domain *domain_old, struct mrc_domain *domain_new,
		   mfields_base_t *flds)
{
  MPI_Comm comm = mrc_domain_comm(domain_new);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  int nr_patches_old, nr_patches_new;
  mrc_domain_get_patches(domain_old, &nr_patches_old);
  struct mrc_patch *patches_new =
    mrc_domain_get_patches(domain_new, &nr_patches_new);

	
	//HACK: Don't communicate output fields if they don't correspond to the domain
	//This is needed e.g. for the boosted output which handles its MPI communication internally
	//printf("Field: %s\n", flds->f[0].name);
	
	if(nr_patches_old != flds->nr_patches /* || strncmp(flds->f[0].name, "lab", 3) == 0 */) return;
	
	
  assert(nr_patches_old == flds->nr_patches);
  assert(nr_patches_old > 0);
  int ibn[3] = { -flds->f[0].ib[0], -flds->f[0].ib[1], -flds->f[0].ib[2] };
  int nr_comp = flds->f[0].nr_comp;
  char **fld_name = flds->f[0].name;

  fields_base_t *f_old = flds->f;
  fields_base_t *f_new = calloc(nr_patches_new, sizeof(*f_new));
  for (int p = 0; p < nr_patches_new; p++) {
    int ilg[3] = { -ibn[0], -ibn[1], -ibn[2] };
    int ihg[3] = { patches_new[p].ldims[0] + ibn[0],
		   patches_new[p].ldims[1] + ibn[1],
		   patches_new[p].ldims[2] + ibn[2] };
    fields_base_alloc(&f_new[p], ilg, ihg, nr_comp);
    for (int m = 0; m < nr_comp; m++) {
      if (fld_name[m]) {
	f_new[p].name[m] = strdup(fld_name[m]);
      }
    }
  }

  MPI_Request *send_reqs = calloc(nr_patches_old, sizeof(*send_reqs));
  // send from old local patches
  for (int p = 0; p < nr_patches_old; p++) {
    struct mrc_patch_info info, info_new;
    mrc_domain_get_local_patch_info(domain_old, p, &info);
    mrc_domain_get_global_patch_info(domain_new, info.global_patch, &info_new);
    if (info_new.rank == rank || info_new.rank < 0) {
      send_reqs[p] = MPI_REQUEST_NULL;
    } else {
      fields_base_t *pf_old = &f_old[p];
      int nn = fields_base_size(pf_old) * pf_old->nr_comp;
      int *ib = pf_old->ib;
      void *addr_old = &F3_BASE(pf_old, 0, ib[0], ib[1], ib[2]);
      MPI_Isend(addr_old, nn, MPI_FIELDS_BASE_REAL, info_new.rank,
		info.global_patch, comm, &send_reqs[p]);
    }
  }

  // recv for new local patches
  MPI_Request *recv_reqs = calloc(nr_patches_new, sizeof(*recv_reqs));
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank == rank) {
      recv_reqs[p] = MPI_REQUEST_NULL;
    } else if (info_old.rank < 0) {	//this patch did not exist before
      recv_reqs[p] = MPI_REQUEST_NULL;
      //Seed new data
    }
    else {
      fields_base_t *pf_new = &f_new[p];
      int nn = fields_base_size(pf_new) * pf_new->nr_comp;
      int *ib = pf_new->ib;
      void *addr_new = &F3_BASE(pf_new, 0, ib[0], ib[1], ib[2]);
      MPI_Irecv(addr_new, nn, MPI_FIELDS_BASE_REAL, info_old.rank,
		info.global_patch, comm, &recv_reqs[p]);
    }
  }

  // local fields
  // OPT: could keep the alloced arrays, just move pointers...
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank != rank) {
      continue;
    }

    fields_base_t *pf_old = &f_old[info_old.patch];
    fields_base_t *pf_new = &f_new[p];

    assert(pf_old->nr_comp == pf_new->nr_comp);
    assert(fields_base_size(pf_old) == fields_base_size(pf_new));
    int size = fields_base_size(pf_old) * pf_old->nr_comp;
    int *ib = pf_new->ib;
    void *addr_new = &F3_BASE(pf_new, 0, ib[0], ib[1], ib[2]);
    void *addr_old = &F3_BASE(pf_old, 0, ib[0], ib[1], ib[2]);
    memcpy(addr_new, addr_old, size * sizeof(fields_base_real_t));
  }

  MPI_Waitall(nr_patches_old, send_reqs, MPI_STATUSES_IGNORE);
  MPI_Waitall(nr_patches_new, recv_reqs, MPI_STATUSES_IGNORE);
  free(send_reqs);
  free(recv_reqs);

  for (int p = 0; p < nr_patches_old; p++) {
    fields_base_free(&f_old[p]);
  }
  free(f_old);

  flds->f = f_new;
  flds->nr_patches = nr_patches_new;
}

static void psc_balance_seed_patches(struct mrc_domain *domain_old, struct mrc_domain *domain_new)
{
  int nr_patches_new;
  mrc_domain_get_patches(domain_new, &nr_patches_new);
    
  for (int p = 0; p < nr_patches_new; p++) {
    struct mrc_patch_info info, info_old;
    mrc_domain_get_local_patch_info(domain_new, p, &info);
    mrc_domain_get_global_patch_info(domain_old, info.global_patch, &info_old);
    if (info_old.rank < 0)	//Patch has to be seeded
    {
      //Seed field
      double t = ppsc->timestep * ppsc->dt;
      psc_bnd_fields_setup_patch(psc_push_fields_get_bnd_fields(ppsc->push_fields), p, ppsc->flds, t);

      //Seed particles
      psc_case_init_particles_patch(ppsc->patchmanager.currentcase, p, 0);
    }
  }
}

void
psc_balance_initial(struct psc_balance *bal, struct psc *psc,
		    int **p_nr_particles_by_patch)
{
  /*if (bal->every <= 0)
    return;*/

  struct mrc_domain *domain_old = psc->mrc_domain;

  int nr_patches;
  mrc_domain_get_patches(domain_old, &nr_patches);
  double *loads = calloc(nr_patches, sizeof(*loads));
  psc_get_loads_initial(psc, loads, *p_nr_particles_by_patch);

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
  psc_setup_patches(psc, domain_new);

  communicate_new_nr_particles(domain_old, domain_new, p_nr_particles_by_patch);

  // ----------------------------------------------------------------------
  // fields

  mfields_base_t *mf;
  list_for_each_entry(mf, &mfields_base_list, entry) {
    communicate_fields(domain_old, domain_new, mf);
  }
  psc_balance_seed_patches(domain_old, domain_new);	//TODO required here?


  mrc_domain_destroy(domain_old);
  psc->mrc_domain = domain_new;
}

// FIXME, way too much duplication from the above

void
psc_balance_run(struct psc_balance *bal, struct psc *psc)
{
  if (bal->force_update == true)
  {
    bal->force_update = false;
  }
  else
  {
    if (bal->every <= 0)
      return;

    if (psc->timestep == 0 || psc->timestep % bal->every != 0)
      return;
  }

  static int st_time_balance;
  if (!st_time_balance) {
    st_time_balance = psc_stats_register("time balancing");
  }

  psc_stats_start(st_time_balance);
  struct mrc_domain *domain_old = psc->mrc_domain;

  int nr_patches;
  mrc_domain_get_patches(domain_old, &nr_patches);
  double *loads = calloc(nr_patches, sizeof(*loads));
  psc_get_loads(psc, loads);

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
  psc_setup_patches(psc, domain_new);
  
  //If there are no active patches, exit here
  int n_global_patches;
  mrc_domain_get_nr_global_patches(domain_new, &n_global_patches);
  if(n_global_patches < 1) exit(0);

  int *nr_particles_by_patch = calloc(nr_patches, sizeof(*nr_particles_by_patch));
  for (int p = 0; p < nr_patches; p++) {
    nr_particles_by_patch[p] = psc->particles->p[p].n_part;
  }
  communicate_new_nr_particles(domain_old, domain_new, &nr_particles_by_patch);

  // OPT: if local patches didn't change at all, no need to do anything...

  // ----------------------------------------------------------------------
  // particles

  // alloc new particles
  mparticles_base_t *mparticles_new = 
    psc_mparticles_base_create(mrc_domain_comm(domain_new));
  psc_mparticles_base_set_domain_nr_particles(mparticles_new, domain_new,
					  nr_particles_by_patch);
  psc_mparticles_base_setup(mparticles_new);

  // communicate particles
  communicate_particles(domain_old, domain_new, 
			psc->particles, mparticles_new, nr_particles_by_patch);
  free(nr_particles_by_patch);

  // replace particles by redistributed ones
  psc_mparticles_base_destroy(psc->particles);
  psc->particles = mparticles_new;

  // ----------------------------------------------------------------------
  // fields

  mfields_base_t *mf;
  list_for_each_entry(mf, &mfields_base_list, entry) {
    communicate_fields(domain_old, domain_new, mf);
  }
  
  psc_balance_seed_patches(domain_old, domain_new);

  // ----------------------------------------------------------------------
  // photons
  // alloc new photons
  // FIXME, will break if there are actual photons
  mphotons_t *mphotons_new = psc_mphotons_create(mrc_domain_comm(domain_new));
  psc_mphotons_set_domain(mphotons_new, domain_new);
  psc_mphotons_setup(mphotons_new);

  // replace photons by redistributed ones
  psc_mphotons_destroy(psc->mphotons);
  psc->mphotons = mphotons_new;


  mrc_domain_destroy(domain_old);
  psc->mrc_domain = domain_new;

  psc_stats_stop(st_time_balance);
}

// ======================================================================
// psc_balance class

#define VAR(x) (void *)offsetof(struct psc_balance, x)
static struct param psc_balance_descr[] = {
  { "every"            , VAR(every)               , PARAM_INT(0)        },
  { "force_update"     , VAR(force_update)	  , PARAM_INT(0)	},
  {},
};
#undef VAR

struct mrc_class_psc_balance mrc_class_psc_balance = {
  .name             = "psc_balance",
  .size             = sizeof(struct psc_balance),
  .param_descr      = psc_balance_descr,
};

