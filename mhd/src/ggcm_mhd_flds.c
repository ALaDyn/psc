
#include "ggcm_mhd_flds_private.h"
#include "ggcm_mhd.h"

#include <mrc_io.h>
#include <mrc_profile.h>
#include <assert.h>
#include <string.h>

// ======================================================================
// ggcm_mhd_flds class

// ----------------------------------------------------------------------
// ggcm_mhd_flds_duplicate

struct ggcm_mhd_flds *
ggcm_mhd_flds_duplicate(struct ggcm_mhd_flds *flds)
{
  struct ggcm_mhd_flds *rv = ggcm_mhd_flds_create(ggcm_mhd_flds_comm(flds));
  if (strcmp(ggcm_mhd_flds_type(flds), "fortran") == 0) {
    MHERE;
    ggcm_mhd_flds_set_type(rv, "c");
  } else {
    ggcm_mhd_flds_set_type(rv, ggcm_mhd_flds_type(flds));
  }
  ggcm_mhd_flds_set_param_obj(rv, "domain", flds->domain);
  ggcm_mhd_flds_setup(rv);
  return rv;
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds_copy

void
ggcm_mhd_flds_copy(struct ggcm_mhd_flds *to, struct ggcm_mhd_flds *from)
{
  struct ggcm_mhd_flds_ops *ops = ggcm_mhd_flds_ops(to);
  assert(ops && ops->copy);
  ops->copy(to, from);
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds_get_mrc_fld
//
// returns the underlying mrc_fld that contains the MHD field data
// only actually works (and should be used) if we know that these flds
// are of type "fortran" or "c"

struct mrc_fld *
ggcm_mhd_flds_get_mrc_fld(struct ggcm_mhd_flds *flds)
{
  assert(flds->fld);
  return flds->fld;
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds_get_as
//
// convert flds_base to ggcm_mhd_flds of type "type"

struct ggcm_mhd_flds *
ggcm_mhd_flds_get_as(struct ggcm_mhd_flds *flds_base, const char *type)
{
  const char *type_base = ggcm_mhd_flds_type(flds_base);
  // If we're already the subtype, nothing to be done
  if (strcmp(type_base, type) == 0)
    return flds_base;

  // special case: when getting a "c" field from a "fortran" base,
  // we just copy the array pointer
  if (strcmp(type_base, "fortran") == 0 && strcmp(type, "c") == 0) {
    struct ggcm_mhd_flds *flds = ggcm_mhd_flds_create(ggcm_mhd_flds_comm(flds_base));
    ggcm_mhd_flds_set_type(flds, type);
    ggcm_mhd_flds_set_param_obj(flds, "domain", flds_base->domain);
    mrc_fld_set_array(flds->fld, flds_base->fld->_arr);
    ggcm_mhd_flds_setup(flds);
    return flds;
  }

  static int pr;
  if (!pr) {
    pr = prof_register("ggcm_mhd_flds_get_as", 1., 0, 0);
  }
  prof_start(pr);

  struct ggcm_mhd_flds *flds = ggcm_mhd_flds_create(ggcm_mhd_flds_comm(flds_base));
  ggcm_mhd_flds_set_type(flds, type);
  ggcm_mhd_flds_set_param_obj(flds, "domain", flds_base->domain);
  ggcm_mhd_flds_setup(flds);

  char s[strlen(type) + 12]; sprintf(s, "copy_to_%s", type);
  ggcm_mhd_flds_copy_to_func_t copy_to = (ggcm_mhd_flds_copy_to_func_t)
    ggcm_mhd_flds_get_method(flds_base, s);
  if (copy_to) {
    copy_to(flds_base, flds);
  } else {
    sprintf(s, "copy_from_%s", type_base);
    ggcm_mhd_flds_copy_from_func_t copy_from = (ggcm_mhd_flds_copy_from_func_t)
      ggcm_mhd_flds_get_method(flds, s);
    if (copy_from) {
      copy_from(flds, flds_base);
    } else {
      fprintf(stderr, "ERROR: no 'copy_to_%s' in ggcm_mhd_flds '%s' and "
	      "no 'copy_from_%s' in '%s'!\n",
	      type, ggcm_mhd_flds_type(flds_base), type_base, ggcm_mhd_flds_type(flds));
      assert(0);
    }
  }

  prof_stop(pr);
  return flds;
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds_put_as
//
// after being done with the fields gotten from get_as(), need to put them
// back using this routine, which will copy the contents from flds back
// to flds_base

void
ggcm_mhd_flds_put_as(struct ggcm_mhd_flds *flds, struct ggcm_mhd_flds *flds_base)
{
  const char *type_base = ggcm_mhd_flds_type(flds_base);
  const char *type = ggcm_mhd_flds_type(flds);
  // If we're already the subtype, nothing to be done
  if (strcmp(type_base, type) == 0)
    return;

  // special case: when we originall got a "c" field from a "fortran" base,
  // we just copied the array pointer, so there's nothing to copy back
  if (strcmp(type_base, "fortran") == 0 && strcmp(type, "c") == 0) {
    ggcm_mhd_flds_destroy(flds);
    return;
  }

  static int pr;
  if (!pr) {
    pr = prof_register("ggcm_mhd_flds_put_as", 1., 0, 0);
  }
  prof_start(pr);

  char s[strlen(type) + 12]; sprintf(s, "copy_from_%s", type);
  ggcm_mhd_flds_copy_from_func_t copy_from = (ggcm_mhd_flds_copy_from_func_t)
    ggcm_mhd_flds_get_method(flds_base, s);
  if (copy_from) {
    copy_from(flds_base, flds);
  } else {
    sprintf(s, "copy_to_%s", type_base);
    ggcm_mhd_flds_copy_to_func_t copy_to = (ggcm_mhd_flds_copy_to_func_t)
      ggcm_mhd_flds_get_method(flds, s);
    if (copy_to) {
      copy_to(flds, flds_base);
    } else {
      fprintf(stderr, "ERROR: no 'copy_from_%s' in ggcm_mhd_flds '%s' and "
	      "no 'copy_to_%s' in '%s'!\n",
	      type, ggcm_mhd_flds_type(flds_base), type_base, ggcm_mhd_flds_type(flds));
      assert(0);
    }
  }

  ggcm_mhd_flds_destroy(flds);

  prof_stop(pr);
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds_init

static void
ggcm_mhd_flds_init()
{
  mrc_class_register_subclass(&mrc_class_ggcm_mhd_flds, &ggcm_mhd_flds_ops_c);
}

// ----------------------------------------------------------------------
// ggcm_mhd_flds description

#define VAR(x) (void *)offsetof(struct ggcm_mhd_flds, x)
static struct param ggcm_mhd_flds_descr[] = {
  { "domain"          , VAR(domain)          , PARAM_OBJ(mrc_domain)    },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// ggcm_mhd_flds class description

struct mrc_class_ggcm_mhd_flds mrc_class_ggcm_mhd_flds = {
  .name             = "ggcm_mhd_flds",
  .size             = sizeof(struct ggcm_mhd_flds),
  .param_descr      = ggcm_mhd_flds_descr,
  .init             = ggcm_mhd_flds_init,
};

