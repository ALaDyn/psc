
#include "psc_testing.h"
#include <mrc_params.h>

#include <string.h>

static void
psc_test_setup_particles(struct psc *psc, int *nr_particles_by_patch, bool count_only)
{
  assert(psc->nr_patches == 1);

  if (count_only) {
    nr_particles_by_patch[0] = 1;
    return;
  }

  struct psc_particles *pp = psc_mparticles_get_patch(psc->particles, 0);
  pp->n_part = nr_particles_by_patch[0];
  for (int i = 0; i < pp->n_part; i++) {
    particle_c_t *prt = particles_c_get_one(pp, 0);
    prt->qni = 1.; prt->mni = 1.; prt->wni = 1.;
    prt->yi = prt->zi = .5;
  }
}

// ======================================================================
// psc_test_ops_1a

static struct psc_ops psc_test_ops_1a = {
  .name             = "test",
  .size             = sizeof(struct psc_test),
  .create           = psc_test_create,
  .init_field       = psc_test_init_field_linear,
  .setup_particles  = psc_test_setup_particles,
  .step             = psc_test_step,
};

// ----------------------------------------------------------------------
// check push_particles_push_yz against "fortran" ref

// psc_push_particles_type used as reference
static const char *s_ref_type = "fortran";
// psc_push_particles_type to be tested
static const char *s_type = "fortran";
// threshold for particles
static double eps_particles = 1e-7;
// threshold for fields
static double eps_fields = 1e-7;
// which test case to set up particles / fields
static const char *s_case = "1";

int
main(int argc, char **argv)
{
  psc_testing_init(&argc, &argv);

  mrc_params_get_option_string("ref_type", &s_ref_type);
  mrc_params_get_option_string("type", &s_type);
  mrc_params_get_option_double("eps_particles", &eps_particles);
  mrc_params_get_option_double("eps_fields", &eps_fields);
  mrc_params_get_option_string("case", &s_case);

  if (strcmp(s_case, "1") == 0) {
    mrc_class_register_subclass(&mrc_class_psc, &psc_test_ops_1);
  } else if (strcmp(s_case, "1a") == 0) {
    mrc_class_register_subclass(&mrc_class_psc, &psc_test_ops_1a);
  } else {
    assert(0);
  }

  struct psc *psc = psc_testing_create_test_yz(s_ref_type, 0);
  psc_setup(psc);
  psc_testing_push_particles(psc, s_ref_type);
  psc_testing_save_ref(psc);
  psc_destroy(psc);

  psc = psc_testing_create_test_yz(s_type, 0);
  psc_setup(psc);
  psc_testing_push_particles(psc, s_type);
  psc_testing_push_particles_check(psc, eps_particles, eps_fields);
  psc_destroy(psc);

  psc_testing_finalize();
}
