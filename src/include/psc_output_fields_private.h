
#ifndef PSC_OUTPUT_FIELDS_PRIVATE_H
#define PSC_OUTPUT_FIELDS_PRIVATE_H

#include <psc_output_fields.h>

struct psc_output_fields {
  struct mrc_obj obj;
};

struct psc_output_fields_ops {
  MRC_SUBCLASS_OPS(struct psc_output_fields);
  void (*run)(struct psc_output_fields *output_fields,
	      MfieldsBase& mflds, MparticlesBase& mprts);
};

// ======================================================================

#define psc_output_fields_ops(output_fields) ((struct psc_output_fields_ops *)((output_fields)->obj.ops))

#endif
