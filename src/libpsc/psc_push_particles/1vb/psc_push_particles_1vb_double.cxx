
#include "psc_push_particles_private.h"

#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"

#define DIM DIM_YZ

#include "../inc_defs.h"
#include "../push_config.hxx"

#define CALC_J CALC_J_1VB_2D

using push_p_conf = push_p_config<MparticlesDouble, MfieldsC, dim_yz, opt_ip_1st, opt_order_1st,
				  opt_calcj_1vb_split>;

#include "../1vb.c"

template<typename dim_t>
using push_p_ops_1vb_double = push_p_ops<push_p_conf>;

using PushParticles_t = PushParticles_<push_p_ops_1vb_double>;
using PushParticlesWrapper_t = PushParticlesWrapper<PushParticles_t>;
  
// ======================================================================
// psc_push_particles: subclass "1vb_double"

struct psc_push_particles_ops_1vb_double : psc_push_particles_ops {
  psc_push_particles_ops_1vb_double() {
    name                  = "1vb_double";
    size                  = PushParticlesWrapper_t::size;
    setup                 = PushParticlesWrapper_t::setup;
    destroy               = PushParticlesWrapper_t::destroy;
    particles_type        = PARTICLE_TYPE;
  }
} psc_push_particles_1vb_double_ops;

