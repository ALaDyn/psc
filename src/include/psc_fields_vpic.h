
#ifndef PSC_FIELDS_VPIC_H
#define PSC_FIELDS_VPIC_H

#include "psc_fields_private.h"

#define FTYPE FTYPE_VPIC
#include "psc_fields_common.h"
#undef FTYPE

// ----------------------------------------------------------------------

struct psc_mfields_vpic {
  struct vpic_mfields *vmflds;
};

#define psc_mfields_vpic(mflds) mrc_to_subobj(mflds, struct psc_mfields_vpic)

#endif
