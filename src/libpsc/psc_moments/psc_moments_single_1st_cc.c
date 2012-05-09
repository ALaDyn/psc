
#include "psc_moments_private.h"
#include "psc_particles_as_single.h"
#include "psc_fields_as_c.h"

// ======================================================================
// psc_moments: subclass "single_1st_cc"
//
// !!! These moments are shifted to (n+.5) * dt, rather than n * dt,
// since the "single" particles are shifted that way.

#include "psc_moments_1st_cc.c"

struct psc_moments_ops psc_moments_single_1st_cc_ops = {
  .name                  = "single_1st_cc",
  .calc_densities        = psc_moments_1st_cc_calc_densities,
  .calc_v                = psc_moments_1st_cc_calc_v,
  .calc_vv               = psc_moments_1st_cc_calc_vv,
};

// ======================================================================
// !!! These moments are shifted to (n+.5) * dt, rather than n * dt,
// since the "single" particles are shifted that way.

#include "psc_output_fields_item_n_1st.c"

// ======================================================================
// psc_output_fields_item: subclass "n_1st_single"

struct psc_output_fields_item_ops psc_output_fields_item_n_1st_single_ops = {
  .name               = "n_1st_single",
  .get_component_name = n_1st_get_component_name,
  .get_nr_components  = n_1st_get_nr_components,
  .run                = n_1st_run,
};

// ======================================================================
// psc_output_fields_item: subclass "v_1st_single"

struct psc_output_fields_item_ops psc_output_fields_item_v_1st_single_ops = {
  .name               = "v_1st_single",
  .get_component_name = v_1st_get_component_name,
  .get_nr_components  = v_1st_get_nr_components,
  .run                = v_1st_run,
};

// ======================================================================
// psc_output_fields_item: subclass "vv_1st_single"

struct psc_output_fields_item_ops psc_output_fields_item_vv_1st_single_ops = {
  .name               = "vv_1st_single",
  .get_component_name = vv_1st_get_component_name,
  .get_nr_components  = vv_1st_get_nr_components,
  .run                = vv_1st_run,
};

