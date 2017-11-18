
#include "vpic_iface.h"

#include "vpic_init.h"
#include "vpic_mfields.h"
#include "vpic_mparticles.h"
#include "vpic_push_particles.h"

#include <vpic.h>

#include <cassert>

extern vpic_simulation *simulation;

// ======================================================================

// ----------------------------------------------------------------------
// vpic_base_init

void vpic_base_init(int *pargc, char ***pargv)
{
  static bool vpic_base_inited = false;

  if (vpic_base_inited) {
    return;
  }
  vpic_base_inited = true;
  
  //  boot_services( &argc, &argv );
  {
    // Start up the checkpointing service.  This should be first.
    
    boot_checkpt( pargc, pargv );
    
    serial.boot( pargc, pargv );
    thread.boot( pargc, pargv );
    
    // Boot up the communications layer
    // See note above about thread-core-affinity
    
    boot_mp( pargc, pargv );
    
    mp_barrier();
    _boot_timestamp = 0;
    _boot_timestamp = uptime();
  }
}

static void
vpic_simulation_get_info(struct vpic_simulation_info *info)
{
  info->num_step = simulation->num_step;

  // grid
  info->dt = simulation->grid->dt;
  info->nx[0] = simulation->grid->nx;
  info->nx[1] = simulation->grid->ny;
  info->nx[2] = simulation->grid->nz;
  info->dx[0] = simulation->grid->dx;
  info->dx[1] = simulation->grid->dy;
  info->dx[2] = simulation->grid->dz;
  info->x0[0] = simulation->grid->x0;
  info->x0[1] = simulation->grid->y0;
  info->x0[2] = simulation->grid->z0;
  info->x1[0] = simulation->grid->x1;
  info->x1[1] = simulation->grid->y1;
  info->x1[2] = simulation->grid->z1;

  // species
  info->n_kinds = num_species(simulation->species_list);
  info->kinds = new vpic_kind_info[info->n_kinds];
  species_t *sp;
  LIST_FOR_EACH( sp, simulation->species_list ) {
    info->kinds[sp->id].q = sp->q;
    info->kinds[sp->id].m = sp->m;
    info->kinds[sp->id].name = sp->name;
  }
  
  // Marder cleaning etc
  info->clean_div_e_interval = simulation->clean_div_e_interval;
  info->clean_div_b_interval = simulation->clean_div_b_interval;
  info->sync_shared_interval = simulation->sync_shared_interval;
  info->num_div_e_round = simulation->num_div_e_round;
  info->num_div_b_round = simulation->num_div_b_round;

  info->status_interval = simulation->status_interval;
}

void vpic_simulation_new()
{
  assert(!simulation);
  if( world_rank==0 ) log_printf( "*** Initializing\n" );
  simulation = new vpic_simulation;
}

void vpic_simulation_init(vpic_simulation_info *info)
{
  // Call the user initialize the simulation
  TIC simulation->user_initialization(0, 0); TOC( user_initialization, 1 );

  vpic_simulation_get_info(info);
}

// ======================================================================
// vpic_diagnostics

void vpic_diagnostics()
{
  // Let the user compute diagnostics
  TIC simulation->user_diagnostics(); TOC( user_diagnostics, 1 );
}

// ======================================================================

void vpic_inc_step(int step)
{
  simulation->grid->step++;
  assert(simulation->grid->step == step);
}


// ======================================================================
// ======================================================================

// ----------------------------------------------------------------------

struct user_global_t {
  struct globals_diag diag;
};

// ----------------------------------------------------------------------
// vpic_simulation_init_split

void vpic_simulation_init_split(vpic_params *vpic_prm, psc_harris *sub,
				vpic_simulation_info *info)
{
  user_global_t *user_global = (struct user_global_t *) simulation->user_global;
  
  user_init(simulation, vpic_prm, sub, &user_global->diag);

  vpic_simulation_get_info(info);
}

// ----------------------------------------------------------------------
// vpic_diagnostics_split

void vpic_diagnostics_split(vpic_params *vpic_prm, psc_harris *harris)
{
  // Let the user compute diagnostics
  user_global_t *user_global = (struct user_global_t *) simulation->user_global;
  TIC vpic_simulation_diagnostics(simulation, vpic_prm,
				  &user_global->diag); TOC( user_diagnostics, 1 );
}

// ----------------------------------------------------------------------
// vpic_simulation_set_params

void vpic_simulation_set_params(int num_step,
				int status_interval,
				int sync_shared_interval,
				int clean_div_e_interval,
				int clean_div_b_interval)
{
  simulation->num_step             = num_step;
  simulation->status_interval      = status_interval;
  simulation->sync_shared_interval = sync_shared_interval;
  simulation->clean_div_e_interval = clean_div_e_interval;
  simulation->clean_div_b_interval = clean_div_b_interval;
}

// ----------------------------------------------------------------------
// vpic_simulation_setup_grid

void vpic_simulation_setup_grid(double dx[3], double dt, double cvac, double eps0)
{
  grid_t *grid = simulation->grid;

  grid->dx = dx[0];
  grid->dy = dx[1];
  grid->dz = dx[2];
  grid->dt = dt;
  grid->cvac = cvac;
  grid->eps0 = eps0;
}

// ----------------------------------------------------------------------
// vpic_simulation_define_periodic_grid

void vpic_simulation_define_periodic_grid(double xl[3], double xh[3],
					  int gdims[3], int np[3])
{
  simulation->define_periodic_grid(xl[0], xl[1], xl[2], xh[0], xh[1], xh[2],
				   gdims[0], gdims[1], gdims[2], np[0], np[1], np[2]);
}

// ----------------------------------------------------------------------
// vpic_simulation_set_domain_field_bc

// FIXME, replicated
///Possible boundary conditions for fields
enum {
  BND_FLD_OPEN,
  BND_FLD_PERIODIC,
  BND_FLD_UPML,
  BND_FLD_TIME,
  BND_FLD_CONDUCTING_WALL,
  BND_FLD_ABSORBING,
};

///Possible boundary conditions for particles
enum {
  BND_PART_REFLECTING,
  BND_PART_PERIODIC,
  BND_PART_ABSORBING,
  BND_PART_OPEN,
};

void
vpic_simulation_set_domain_field_bc(int boundary, int bc)
{
  int fbc;
  switch (bc) {
  case BND_FLD_CONDUCTING_WALL: fbc = pec_fields   ; break;
  case BND_FLD_ABSORBING:       fbc = absorb_fields; break;
  default: assert(0);
  }
  simulation->set_domain_field_bc(boundary, fbc);
}

void
vpic_simulation_set_domain_particle_bc(int boundary, int bc)
{
  int pbc;
  switch (bc) {
  case BND_PART_REFLECTING: pbc = reflect_particles; break;
  case BND_PART_ABSORBING:  pbc = absorb_particles ; break;
  default: assert(0);
  }
  simulation->set_domain_particle_bc(boundary, pbc);
}

