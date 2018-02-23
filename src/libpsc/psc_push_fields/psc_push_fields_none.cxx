
#include "psc_push_fields_private.h"

#include "push_fields.hxx"

class PushFieldsNone : PushFieldsBase
{
public:
  void push_E(struct psc_push_fields *push, struct psc_mfields *mflds_base,
	      double dt_fac) override
  {}

  void push_H(struct psc_push_fields *push, struct psc_mfields *mflds_base,
	      double dt_fac) override
  {}
};

static void
psc_push_fields_none_setup(struct psc_push_fields *push)
{
  PscPushFields<PushFieldsNone> pushf(push);
  new(pushf.sub()) PushFieldsNone;
}

static void
psc_push_fields_none_destroy(struct psc_push_fields *push)
{
  PscPushFields<PushFieldsNone> pushf(push);
  pushf.sub()->~PushFieldsNone();
}

static void
psc_push_fields_none_push_mflds_E(struct psc_push_fields *push, struct psc_mfields *mflds_base,
				  double dt_fac)
{
}

static void
psc_push_fields_none_push_mflds_H(struct psc_push_fields *push, struct psc_mfields *mflds_base,
				  double dt_fac)
{
}

// ======================================================================
// psc_push_fields: subclass "none"

struct psc_push_fields_ops_none : psc_push_fields_ops {
  psc_push_fields_ops_none() {
    name                  = "none";
    size                  = sizeof(PushFieldsNone);
    setup                 = psc_push_fields_none_setup;
    destroy               = psc_push_fields_none_destroy;
    push_mflds_E          = psc_push_fields_none_push_mflds_E;
    push_mflds_H          = psc_push_fields_none_push_mflds_H;
  }
} psc_push_fields_none_ops;
