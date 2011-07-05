
#include "psc_output_fields_private.h"

#include <mrc_io.h>

void
psc_output_fields_set_psc(struct psc_output_fields *output_fields, struct psc *psc)
{
  output_fields->psc = psc;
}

// ----------------------------------------------------------------------
// psc_output_fields_write

static void
_psc_output_fields_write(struct psc_output_fields *out, struct mrc_io *io)
{
  const char *path = psc_output_fields_name(out);
  mrc_io_write_obj_ref(io, path, "psc", (struct mrc_obj *) out->psc);
}

// ----------------------------------------------------------------------
// psc_output_fields_read

static void
_psc_output_fields_read(struct psc_output_fields *out, struct mrc_io *io)
{
  const char *path = psc_output_fields_name(out);
  out->psc = (struct psc *)
    mrc_io_read_obj_ref(io, path, "psc", &mrc_class_psc);

  struct psc_output_fields_ops *ops = psc_output_fields_ops(out);
  if (ops->read) {
    ops->read(out, io);
  }
}

// ======================================================================
// forward to subclass

void
psc_output_fields_run(struct psc_output_fields *output_fields,
		      mfields_base_t *flds, mparticles_base_t *particles)
{
  struct psc_output_fields_ops *ops = psc_output_fields_ops(output_fields);
  assert(ops->run);
  ops->run(output_fields, flds, particles);
}

// ======================================================================
// psc_output_fields_init

static void
psc_output_fields_init()
{
  mrc_class_register_subclass(&mrc_class_psc_output_fields, &psc_output_fields_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_output_fields, &psc_output_fields_fortran_ops);
}

// ======================================================================
// psc_output_fields class

struct mrc_class_psc_output_fields mrc_class_psc_output_fields = {
  .name             = "psc_output_fields",
  .size             = sizeof(struct psc_output_fields),
  .init             = psc_output_fields_init,
  .write            = _psc_output_fields_write,
  .read             = _psc_output_fields_read,
};

