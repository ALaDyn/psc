
#include "psc_push_particles_double.h"

#include "psc_fields_c.h"

#define F3_CURR F3_C
#define F3_CACHE F3_C
#define F3_CACHE_TYPE "c"
#define psc_push_particles_push_yz psc_push_particles_1vbec3d_double_push_a_yz

#define PUSHER_TYPE "1vbec3d"
#define INTERPOLATE_1ST INTERPOLATE_1ST_EC

#include "1vb_yz.c"
