
#include "psc_push_particles_private.h"

#include "psc_cuda2.h"

#include <string.h>

//#define GOLD

// ======================================================================
// psc_push_particles: subclass "1vb_cuda2"

static void
psc_push_particles_1vbec_push_mprts_yz(struct psc_push_particles *push,
				     struct psc_mparticles *mprts_base,	
				     struct psc_mfields *mflds_base)					
{									
  struct psc_mparticles *mprts =
    psc_mparticles_get_as(mprts_base, "cuda2", 0);
  struct psc_mfields *mflds =
    psc_mfields_get_as(mflds_base, "cuda2", EX, EX + 6);

#ifdef GOLD
  cuda2_1vbec_push_mprts_yz_gold(mprts, mflds);
#else
  struct psc_mparticles *mprts_cuda =
    psc_mparticles_get_as(mprts, "cuda", 0);
  struct psc_mfields *mflds_cuda =
    psc_mfields_get_as(mflds, "cuda", EX, EX + 6);

  cuda2_1vbec_push_mprts_yz(mprts, mflds, mprts_cuda, mflds_cuda);

  psc_mparticles_put_as(mprts_cuda, mprts, 0);
  psc_mfields_put_as(mflds_cuda, mflds, JXI, JXI + 3);
#endif

  psc_mparticles_put_as(mprts, mprts_base, 0);
  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);	
}

// ----------------------------------------------------------------------
// psc_push_particles: subclass "1vbec_cuda2"
									
struct psc_push_particles_ops						
psc_push_particles_1vbec_cuda2_ops = {	
  .name                  = "1vbec_cuda2",
  .push_mprts_yz         = psc_push_particles_1vbec_push_mprts_yz,
};
