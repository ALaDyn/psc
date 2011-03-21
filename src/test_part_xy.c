
#include "psc_testing.h"
#include "psc_push_particles.h"
#include <mrc_profile.h>
#include <mrc_params.h>

#include <stdio.h>
#include <mpi.h>

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  struct psc_mod_config conf_fortran = {
    .mod_particle = "fortran",
  };
  struct psc_mod_config conf_generic_c = {
    .mod_particle = "generic_c",
  };

  psc_create_test_xy(&conf_fortran);
  mfields_base_t *flds = &psc.flds;
  mparticles_base_t *particles = &psc.particles;
  //  psc_dump_particles("part-0");
  psc_push_particles_run(psc.push_particles, particles, flds);
  psc_save_particles_ref(&psc,particles);
  psc_save_fields_ref(&psc,flds);
  //  psc_dump_particles("part-1");
  psc_destroy(&psc);


  psc_create_test_xy(&conf_generic_c);
  psc_push_particles_run(psc.push_particles, particles, flds);
  //  psc_dump_particles("part-2");
  psc_check_particles_ref(&psc,particles, 1e-7, "push_part_xy -- generic_c");
  psc_check_currents_ref(&psc,flds, 1e-7);
  psc_destroy(&psc);



#ifdef USE_SSE2
  struct psc_mod_config conf_sse2 = {
    .mod_particle = "sse2",
  };
  psc_create_test_xy(&conf_sse2);
  psc_push_particles_run(psc.push_particles, particles, flds);
  //  psc_dump_particles("part-2");
  psc_check_particles_ref(&psc,1e-7, "push_part_xy -- sse2");
  psc_check_currents_ref(&psc,1e-6);
  psc_destroy(&psc);
#endif

#ifdef USE_CBE
  struct psc_mod_config conf_cbe = {
    .mod_particle = "cbe",
  };
  psc_create_test_xy(&conf_cbe);
  psc_push_particles_run(psc.push_particles, particles, flds);
  //  psc_dump_particles("part-2");
  psc_check_particles_ref(&psc,particles,1e-7, "push_part_xy -- cbe");
  psc_check_currents_ref(&psc,flds,1e-6);
  psc_destroy(&psc);
#endif

  prof_print();

  MPI_Finalize();
}
