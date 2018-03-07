
#include "psc_bnd_private.h"

#include "vpic_iface.h"

// ----------------------------------------------------------------------
// psc_bnd_vpic_fill_ghosts

static void
psc_bnd_vpic_fill_ghosts(struct psc_bnd *bnd, struct psc_mfields *mflds,
			 int mb, int me)
{  
}

// ----------------------------------------------------------------------
// psc_bnd_vpic_add_ghosts

static void
psc_bnd_vpic_add_ghosts(struct psc_bnd *bnd, struct psc_mfields *mflds,
			int mb, int me)
{  
}

// ----------------------------------------------------------------------
// psc_bnd: subclass "vpic"

struct psc_bnd_ops_vpic : psc_bnd_ops {
  psc_bnd_ops_vpic() {
    name                  = "vpic";
    fill_ghosts           = psc_bnd_vpic_fill_ghosts;
    add_ghosts            = psc_bnd_vpic_add_ghosts;
  }
} psc_bnd_vpic_ops;

