
#include "psc_push_particles_single.h"

#include "psc_fields_single.h"

#define F3_CURR F3_S
#define F3_CACHE F3_S
#define F3_CACHE_TYPE "single"
#define psc_push_particles_push_yz psc_push_particles_1vb_single_push_a_yz

#define PUSHER_TYPE "1vb"
#define INTERPOLATE_1ST INTERPOLATE_1ST_STD
#define VB_2D

#include "1vb_yz.c"
