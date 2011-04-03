
#include "psc_testing.h"
#include "psc_bnd.h"
#include <mrc_profile.h>
#include <mrc_params.h>

#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <string.h>

void
setup_particles(mparticles_base_t *particles)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // FIXME, realloc
  // only particles on proc 0 / patch 0, but some are out of bounds.
  // particles are right on nodes of the grid, but as far as
  // particle -> cell mapping goes, that's right in the center of the
  // particle ordering cell, for reasons that should be laid out more clearly.
  // for the old ordering, nodes aren't good because they're indeterminate
  // (could go either way), so let's shift them a bit so we get a unique answer
  // we can check.

  foreach_patch(p) {
    particles_base_t *pp = &particles->p[p];
    if (rank != 0 || p != 0) {
      pp->n_part = 0;
      continue;
    }

    struct psc_patch *patch = &psc.patch[p];
    int *ilo = patch->off;
    int ihi[3] = { patch->off[0] + patch->ldims[0],
		   patch->off[1] + patch->ldims[1],
		   patch->off[2] + patch->ldims[2] };
    int i = 0;
    
    for (int iz = ilo[2]-1; iz < ihi[2]+1; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) { // xz only !!!
	for (int ix = ilo[0]-1; ix < ihi[0]+1; ix++) {
	  particle_base_t *p;
	  p = particles_base_get_one(pp, i++);
	  memset(p, 0, sizeof(*p));
	  p->xi = (ix + .01) * psc.dx[0];
	  p->yi = (iy + .01) * psc.dx[1];
	  p->zi = (iz + .01) * psc.dx[2];
	  
	  p = particles_base_get_one(pp, i++);
	  memset(p, 0, sizeof(*p));
	  p->xi = (ix - .01) * psc.dx[0];
	  p->yi = (iy - .01) * psc.dx[1];
	  p->zi = (iz - .01) * psc.dx[2];
	}
      }
    }
    pp->n_part = i;
  }
}

// FIXME, make generic
static int
get_total_num_particles(mparticles_base_t *particles)
{
  int nr_part = 0;
  foreach_patch(p) {
    particles_base_t *pp = &particles->p[p];
    nr_part += pp->n_part;
  }

  int total_nr_part;
  MPI_Allreduce(&nr_part, &total_nr_part, 1, MPI_INT, MPI_SUM,
		MPI_COMM_WORLD);

  return total_nr_part;
}

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  struct psc_mod_config conf_c = {
    .mod_bnd = "c",
  };

  // test psc_exchange_particles()

  struct psc_case *_case = psc_create_test_xz(&conf_c);
  psc_case_setup(_case);
  mparticles_base_t *particles = &psc.particles;
  setup_particles(particles);
  //  psc_dump_particles("part-0");
  int total_num_particles_before = get_total_num_particles(particles);
  psc_bnd_exchange_particles(psc.bnd, particles);
  //  psc_dump_particles("part-1");
  int total_num_particles_after = get_total_num_particles(particles);
  psc_check_particles(particles);
  assert(total_num_particles_before == total_num_particles_after);
  psc_case_destroy(_case);

  prof_print();

  MPI_Finalize();
}
