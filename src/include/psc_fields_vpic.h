
#ifndef PSC_FIELDS_VPIC_H
#define PSC_FIELDS_VPIC_H

#include "psc_fields_private.h"

#include <mrc_common.h>

#define FTYPE FTYPE_VPIC
#include "psc_fields_common.h"
#undef FTYPE

#include "vpic_iface.h"

#ifdef __cplusplus
#include "fields.hxx"

template<>
struct fields_traits<fields_vpic_t>
{
  using real_t = fields_vpic_real_t;
};
#endif


BEGIN_C_DECLS

// ----------------------------------------------------------------------

struct psc_mfields_vpic {
  Simulation *sim;
  FieldArray *vmflds_fields;
  HydroArray *vmflds_hydro;
};

#define psc_mfields_vpic(mflds) ({					\
      assert((struct psc_mfields_ops *) mflds->obj.ops == &psc_mfields_vpic_ops); \
      mrc_to_subobj(mflds, struct psc_mfields_vpic);			\
    })


END_C_DECLS

#endif
