
#include "psc_push_particles_double.h"

#include "psc_fields_c.h"

#define F3_CURR F3_C
#define F3_CACHE F3_C
#define F3_CACHE_TYPE "c"
#define psc_push_particles_push_yz psc_push_particles_1vbec_double_push_a_yz

#include "1vbec_yz.c"
