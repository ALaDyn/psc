
#include "psc_bnd_c.h"
#include "psc.h"
#include "psc_fields_as_c.h"

#include <mrc_profile.h>

// ======================================================================
// ddc funcs

static void
copy_to_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_t *flds = ctx;
  fields_t *pf = psc_mfields_get_patch(flds, p);
  fields_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  MRC_DDC_BUF3(buf, m - mb, ix,iy,iz) = F3(pf, m, ix,iy,iz);
	}
      }
    }
  }
}

static void
add_from_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_t *flds = ctx;
  fields_t *pf = psc_mfields_get_patch(flds, p);
  fields_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  F3(pf, m, ix,iy,iz) += MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	}
      }
    }
  }
}

static void
copy_from_buf(int mb, int me, int p, int ilo[3], int ihi[3], void *_buf, void *ctx)
{
  mfields_t *flds = ctx;
  fields_t *pf = psc_mfields_get_patch(flds, p);
  fields_real_t *buf = _buf;

  for (int m = mb; m < me; m++) {
    for (int iz = ilo[2]; iz < ihi[2]; iz++) {
      for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	  F3(pf, m, ix,iy,iz) = MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	}
      }
    }
  }
}

static struct mrc_ddc_funcs ddc_funcs = {
  .copy_to_buf   = copy_to_buf,
  .copy_from_buf = copy_from_buf,
  .add_from_buf  = add_from_buf,
};

// ----------------------------------------------------------------------
// psc_bnd_lib_create_ddc

struct mrc_ddc *
psc_bnd_lib_create_ddc(struct psc *psc)
{
  struct mrc_ddc *ddc = mrc_domain_create_ddc(psc->mrc_domain);
  mrc_ddc_set_funcs(ddc, &ddc_funcs);
  mrc_ddc_set_param_int3(ddc, "ibn", psc->ibn);
  mrc_ddc_set_param_int(ddc, "max_n_fields", 6);
  mrc_ddc_set_param_int(ddc, "size_of_type", sizeof(fields_real_t));
  mrc_ddc_setup(ddc);
  return ddc;
}

// ----------------------------------------------------------------------
// psc_bnd_lib_add_ghosts

void
__psc_bnd_lib_add_ghosts(struct mrc_ddc *ddc, mfields_t *flds, int mb, int me)
{
  static int pr;
  if (!pr) {
    pr = prof_register("c_add_ghosts", 1., 0, 0);
  }
  prof_start(pr);
  mrc_ddc_add_ghosts(ddc, mb, me, flds);
  prof_stop(pr);
}

void
psc_bnd_lib_add_ghosts(struct mrc_ddc *ddc, mfields_base_t *flds_base, int mb, int me)
{
  mfields_t *flds = psc_mfields_get_cf(flds_base, mb, me);
  __psc_bnd_lib_add_ghosts(ddc, flds, mb, me);
  psc_mfields_put_cf(flds, flds_base, mb, me);
}

// ----------------------------------------------------------------------
// psc_bnd_lib_fill_ghosts

void
psc_bnd_lib_fill_ghosts(struct mrc_ddc *ddc, mfields_base_t *flds_base, int mb, int me)
{
  mfields_t *flds = psc_mfields_get_cf(flds_base, mb, me);

  static int pr;
  if (!pr) {
    pr = prof_register("c_fill_ghosts", 1., 0, 0);
  }
  prof_start(pr);
  // FIXME
  // I don't think we need as many points, and only stencil star
  // rather then box
  mrc_ddc_fill_ghosts(ddc, mb, me, flds);
  prof_stop(pr);

  psc_mfields_put_cf(flds, flds_base, mb, me);
}



