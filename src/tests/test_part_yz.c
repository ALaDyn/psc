
#include "psc_testing.h"
#include "psc_push_particles.h"
#include <mrc_profile.h>

#include <stdio.h>
#include <mpi.h>

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);

  printf("=== testing push_part_yz_a()\n");

  struct psc_case *_case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "fortran");
  psc_case_setup(_case);
  mparticles_base_t *particles = ppsc->particles;
  //  psc_dump_particles("part-0");
  psc_push_particles_push_yz_a(ppsc->push_particles, particles, ppsc->flds);
  //  psc_dump_particles("part-1");
  psc_save_particles_ref(ppsc, particles);
  psc_case_destroy(_case);

  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "generic_c");
  psc_case_setup(_case);
  particles = ppsc->particles;
  psc_push_particles_push_yz_a(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-6, "push_part_yz_a -- generic_c");
  psc_case_destroy(_case);

#ifdef USE_CUDA
  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "cuda");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_push_yz_a(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(1e-6, "push_part_yz_a -- cuda");
  psc_case_destroy(_case);
#endif 

#ifdef USE_SSE2
  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "sse2");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_push_yz_a(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-6, "push_part_yz_a -- sse2");
  psc_case_destroy(_case);
#endif

  printf("=== testing push_part_yz_b()\n");

  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "fortran");
  psc_case_setup(_case);
  particles = ppsc->particles;
  //  psc_dump_particles("part-0");
  psc_push_particles_push_yz_b(ppsc->push_particles, particles, ppsc->flds);
  //  psc_dump_particles("part-1");
  psc_save_particles_ref(ppsc, particles);
  psc_case_destroy(_case);

  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "generic_c");
  psc_case_setup(_case);
  particles = ppsc->particles;
  psc_push_particles_push_yz_b(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-6, "push_part_yz_b -- generic_c");
  psc_case_destroy(_case);

#ifdef USE_CUDA
  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "cuda");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_push_yz_b(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(1e-3, "push_part_yz_b -- cuda");
  psc_case_destroy(_case);
#endif

#ifdef USE_SSE2
  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "sse2");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_push_yz_b(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-6, "push_part_yz_b -- sse2");
  psc_case_destroy(_case);
#endif

  printf("=== testing push_part_yz()\n");

  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "fortran");
  psc_case_setup(_case);
  particles = ppsc->particles;
  //  psc_dump_particles("part-0");
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  //  psc_dump_particles("part-1");
  psc_save_particles_ref(ppsc, particles);
  psc_save_fields_ref(ppsc, ppsc->flds);
  psc_case_destroy(_case);

  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "generic_c");
  psc_case_setup(_case);
  particles = ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-7, "push_part_yz -- generic_c");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-7);
  psc_case_destroy(_case);

#ifdef USE_SSE2
  _case = psc_create_test_yz();
  psc_push_particles_set_type(ppsc->push_particles, "sse2");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(&psc, particles, 1e-8, "push_part_yz -- sse2");
  psc_check_currents_ref(&psc, ppsc->flds, 2e-6); 
  psc_case_destroy(_case);
#endif

  prof_print();

  MPI_Finalize();
}
