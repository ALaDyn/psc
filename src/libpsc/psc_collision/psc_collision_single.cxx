
#include <psc_fields_as_single.h>
#include <psc_particles_as_single.h>

#define psc_output_fields_item_coll_stats_ops psc_output_fields_item_coll_stats_single_ops
#define psc_output_fields_item_coll_rei_ops psc_output_fields_item_coll_rei_single_ops

#include "psc_collision_common.cxx"

// ======================================================================
// psc_collision: subclass "single"

psc_collision_ops_<Collision_<mparticles_t>> psc_collision_single_ops;

// ======================================================================
// psc_output_fields_item: subclass "coll_stats"

struct psc_output_fields_item_ops_coll : psc_output_fields_item_ops {
  using Collision = Collision_<mparticles_t>;
  psc_output_fields_item_ops_coll() {
    name      = "coll_stats_" PARTICLE_TYPE;
    nr_comp   = Collision::NR_STATS;
    fld_names[0] = "coll_nudt_min";
    fld_names[1] = "coll_nudt_med";
    fld_names[2] = "coll_nudt_max";
    fld_names[3] = "coll_nudt_large";
    fld_names[4] = "coll_ncoll";
    run_all   = CollisionWrapper<Collision>::copy_stats;
  }
} psc_output_fields_item_coll_stats_ops;

// ======================================================================
// psc_output_fields_item: subclass "coll_rei"

struct psc_output_fields_item_ops_coll_rei : psc_output_fields_item_ops {
  using Collision = Collision_<mparticles_t>;
  psc_output_fields_item_ops_coll_rei() {
    name      = "coll_rei_" PARTICLE_TYPE;
    nr_comp   = 3;
    fld_names[0] = "coll_rei_x";
    fld_names[1] = "coll_rei_y";
    fld_names[2] = "coll_rei_z";
    run_all   = CollisionWrapper<Collision>::copy_rei;
  }
} psc_output_fields_item_coll_rei_ops;


