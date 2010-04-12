
#include <stdio.h>

#include "psc.h"

int
main()
{
  int ilo[3] = { 0, 0, 0 };
  int ihi[3] = { 8, 8, 8 };
  int ibn[3] = { 2, 2, 2 }; // FIXME?

  int n_part = 1;

  psc_alloc(ilo, ihi, ibn, n_part);
  psc_setup_parameters();
  psc_setup_fields_zero();
  psc_setup_particles_1();

  psc_dump_particles("part-0.asc");
  PIC_push_part_yz();
  psc_dump_particles("part-1.asc");

  psc_free();
}
