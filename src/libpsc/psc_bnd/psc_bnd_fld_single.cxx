
#include "psc_bnd_fld.h"
#include "psc_bnd_private.h"
#include "psc_fields_as_single.h"

#define psc_bnd_fld_sub_create        psc_bnd_fld_single_create
#define psc_bnd_fld_sub_add_ghosts    psc_bnd_fld_single_add_ghosts
#define psc_bnd_fld_sub_fill_ghosts   psc_bnd_fld_single_fill_ghosts
#define psc_bnd_fld_sub_copy_to_buf   psc_bnd_fld_single_copy_to_buf
#define psc_bnd_fld_sub_copy_from_buf psc_bnd_fld_single_copy_from_buf
#define psc_bnd_fld_sub_add_from_buf  psc_bnd_fld_single_add_from_buf

#include "psc_bnd_fld.cxx"

