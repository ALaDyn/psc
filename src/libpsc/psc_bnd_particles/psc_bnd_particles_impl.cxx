
#include "psc_bnd_particles_private.h"
#include "psc_particles_single.h"
#include "psc_particles_double.h"
#include "psc_particles_fortran.h"

//#include "psc_bnd_particles_open.cxx"

#include "ddc_particles_inc.c"

// ======================================================================
// psc_bnd_particles: subclass "single"

struct psc_bnd_particles_ops_single : psc_bnd_particles_ops {
  using sub = psc_bnd_particles_sub<mparticles_single_t>;
  psc_bnd_particles_ops_single() {
    name                    = "single";
    size                    = sizeof(sub);
    setup                   = sub::setup;
    unsetup                 = sub::unsetup;
    exchange_particles      = sub::exchange_particles;
    //open_calc_moments       = psc_bnd_particles_sub_open_calc_moments;
  }
} psc_bnd_particles_single_ops;

// ======================================================================
// psc_bnd_particles: subclass "double"

struct psc_bnd_particles_ops_double : psc_bnd_particles_ops {
  using sub = psc_bnd_particles_sub<mparticles_double_t>;
  psc_bnd_particles_ops_double() {
    name                    = "double";
    size                    = sizeof(sub);
    setup                   = sub::setup;
    unsetup                 = sub::unsetup;
    exchange_particles      = sub::exchange_particles;
    //open_calc_moments       = psc_bnd_particles_sub_open_calc_moments;
  }
} psc_bnd_particles_double_ops;

// ======================================================================
// psc_bnd_particles: subclass "fortran"

struct psc_bnd_particles_ops_fortran : psc_bnd_particles_ops {
  using sub = psc_bnd_particles_sub<mparticles_fortran_t>;
  psc_bnd_particles_ops_fortran() {
    name                    = "fortran";
    size                    = sizeof(sub);
    setup                   = sub::setup;
    unsetup                 = sub::unsetup;
    exchange_particles      = sub::exchange_particles;
    //open_calc_moments       = psc_bnd_particles_sub_open_calc_moments;
  }
} psc_bnd_particles_fortran_ops;
