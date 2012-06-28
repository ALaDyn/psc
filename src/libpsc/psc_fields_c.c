
#include "psc.h"
#include "psc_fields_c.h"

#include <mrc_params.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void
psc_fields_c_setup(struct psc_fields *pf)
{
  unsigned int size = 1;
  for (int d = 0; d < 3; d++) {
    size *= pf->im[d];
  }
#ifdef USE_CBE
  // The Cell processor translation can use the C fields with one modification:
  // the data needs to be 128 byte aligned (to speed off-loading to spes). This
  // change is roughly put in below.
  void *m;
  int ierr = posix_memalign(&m, 128, nr_comp * size * sizeof(*pf->flds));
  pf->flds =  m; 
  assert(ierr == 0);
#else
  pf->data = calloc(pf->nr_comp * size, sizeof(fields_c_real_t));
#endif
}

static void
psc_fields_c_destroy(struct psc_fields *pf)
{
  free(pf->data);
}

static void
psc_fields_c_zero_comp(struct psc_fields *pf, int m)
{
  memset(&F3_C(pf, m, pf->ib[0], pf->ib[1], pf->ib[2]), 0,
	 pf->im[0] * pf->im[1] * pf->im[2] * sizeof(fields_c_real_t));
}

static void
psc_fields_c_set_comp(struct psc_fields *pf, int m, double _val)
{
  fields_c_real_t val = _val;

  for (int jz = pf->ib[2]; jz < pf->ib[2] + pf->im[2]; jz++) {
    for (int jy = pf->ib[1]; jy < pf->ib[1] + pf->im[1]; jy++) {
      for (int jx = pf->ib[0]; jx < pf->ib[0] + pf->im[0]; jx++) {
	F3_C(pf, m, jx, jy, jz) = val;
      }
    }
  }
}

static void
psc_fields_c_scale_comp(struct psc_fields *pf, int m, double _val)
{
  fields_c_real_t val = _val;

  for (int jz = pf->ib[2]; jz < pf->ib[2] + pf->im[2]; jz++) {
    for (int jy = pf->ib[1]; jy < pf->ib[1] + pf->im[1]; jy++) {
      for (int jx = pf->ib[0]; jx < pf->ib[0] + pf->im[0]; jx++) {
	F3_C(pf, m, jx, jy, jz) *= val;
      }
    }
  }
}

static void
psc_fields_c_copy_comp(struct psc_fields *pto, int m_to, struct psc_fields *pfrom, int m_from)
{
  for (int jz = pto->ib[2]; jz < pto->ib[2] + pto->im[2]; jz++) {
    for (int jy = pto->ib[1]; jy < pto->ib[1] + pto->im[1]; jy++) {
      for (int jx = pto->ib[0]; jx < pto->ib[0] + pto->im[0]; jx++) {
	F3_C(pto, m_to, jx, jy, jz) = F3_C(pfrom, m_from, jx, jy, jz);
      }
    }
  }
}

static void
psc_fields_c_axpy_comp(struct psc_fields *y, int ym, double _a, struct psc_fields *x, int xm)
{
  fields_c_real_t a = _a;

  for (int jz = y->ib[2]; jz < y->ib[2] + y->im[2]; jz++) {
    for (int jy = y->ib[1]; jy < y->ib[1] + y->im[1]; jy++) {
      for (int jx = y->ib[0]; jx < y->ib[0] + y->im[0]; jx++) {
	F3_C(y, ym, jx, jy, jz) += a * F3_C(x, xm, jx, jy, jz);
      }
    }
  }
}

// ======================================================================

#ifdef HAVE_LIBHDF5_HL

#include <mrc_io.h>

// FIXME. This is a rather bad break of proper layering, HDF5 should be all
// mrc_io business. OTOH, it could be called flexibility...

#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

// ----------------------------------------------------------------------
// psc_fields_c_write

static void
psc_fields_c_write(struct psc_fields *flds, struct mrc_io *io)
{
  int ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group = H5Gopen(h5_file, psc_fields_name(flds), H5P_DEFAULT); H5_CHK(group);
  ierr = H5LTset_attribute_int(group, ".", "p", &flds->p, 1); CE;
  ierr = H5LTset_attribute_int(group, ".", "ib", flds->ib, 3); CE;
  ierr = H5LTset_attribute_int(group, ".", "im", flds->im, 3); CE;
  ierr = H5LTset_attribute_int(group, ".", "nr_comp", &flds->nr_comp, 1); CE;
  // write components separately instead?
  hsize_t hdims[4] = { flds->nr_comp, flds->im[2], flds->im[1], flds->im[0] };
  ierr = H5LTmake_dataset_float(group, "fields_c", 4, hdims, flds->data); CE;
  ierr = H5Gclose(group); CE;
}

// ----------------------------------------------------------------------
// psc_fields_c_read

static void
psc_fields_c_read(struct psc_fields *flds, struct mrc_io *io)
{
  int ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group = H5Gopen(h5_file, psc_fields_name(flds), H5P_DEFAULT); H5_CHK(group);
  int ib[3], im[3], nr_comp;
  ierr = H5LTget_attribute_int(group, ".", "p", &flds->p); CE;
  ierr = H5LTget_attribute_int(group, ".", "ib", ib); CE;
  ierr = H5LTget_attribute_int(group, ".", "im", im); CE;
  ierr = H5LTget_attribute_int(group, ".", "nr_comp", &nr_comp); CE;
  for (int d = 0; d < 3; d++) {
    assert(ib[d] == flds->ib[d]);
    assert(im[d] == flds->im[d]);
  }
  assert(nr_comp == flds->nr_comp);
  psc_fields_setup(flds);
  ierr = H5LTread_dataset_float(group, "fields_c", flds->data); CE;
  ierr = H5Gclose(group); CE;
}

#endif

// ======================================================================
// psc_mfields: subclass "c"
  
struct psc_mfields_ops psc_mfields_c_ops = {
  .name                  = "c",
};

// ======================================================================
// psc_fields: subclass "c"
  
struct psc_fields_ops psc_fields_c_ops = {
  .name                  = "c",
  .setup                 = psc_fields_c_setup,
  .destroy               = psc_fields_c_destroy,
#ifdef HAVE_LIBHDF5_HL
  .read                  = psc_fields_c_read,
  .write                 = psc_fields_c_write,
#endif
  .zero_comp             = psc_fields_c_zero_comp,
  .set_comp              = psc_fields_c_set_comp,
  .scale_comp            = psc_fields_c_scale_comp,
  .copy_comp             = psc_fields_c_copy_comp,
  .axpy_comp             = psc_fields_c_axpy_comp,
};

