
#include <mpi.h>

#include "psc.h"
#include "util/params.h"

#define INIT_basic_F77 F77_FUNC_(init_basic, INIT_BASIC)
#define INIT_param_fortran_F77 F77_FUNC_(init_param_fortran, INIT_PARAM_FORTRAN)
#define ALLOC_field_fortran_F77 F77_FUNC_(alloc_field_fortran, ALLOC_FIELD_FORTRAN)
#define SETUP_field_F77 F77_FUNC_(setup_field, SETUP_FIELD)
#define PSC_driver_F77 F77_FUNC_(psc_driver, PSC_DRIVER)

void INIT_basic_F77(void);
void INIT_param_fortran_F77(void);
void ALLOC_field_fortran_F77(void);
void SETUP_field_F77(void);
void PSC_driver_F77(void);

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  params_init(argc, argv);

  psc_create("fortran", "fortran", "fortran");

  psc_init_param();

  SET_param_domain();
  SET_param_psc();
  SET_param_coeff();
  INIT_basic_F77();
  INIT_param_fortran_F77();

  int n_part;
  psc_init_partition(&n_part);
  SET_subdomain();
  psc.f_part = ALLOC_particles(n_part);
  psc_init_particles();

  f_real **fields = ALLOC_field();
  for (int n = 0; n < NR_FIELDS; n++) {
    psc.f_fields[n] = fields[n];
  }
  psc_init_field();
  ALLOC_field_fortran_F77();
  SETUP_field_F77();

  PSC_driver_F77();

  MPI_Finalize();
}
