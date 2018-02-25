
#include "psc_generic_c.h"

#include <mrc_profile.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct Config2ndYZ
{
  using mparticles_t = PscMparticlesDouble;
  using mfields_t = PscMfieldsC;
};

#define CONFIG Config2ndYZ

#define DIM DIM_YZ
#define ORDER ORDER_2ND
#define do_push_part do_push_part_genc_yz
#define PROF_NAME "genc_push_mprts_yz"
#define PRTS PRTS_STAGGERED
#include "push_part_common.c"

