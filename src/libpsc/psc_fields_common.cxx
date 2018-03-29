
#include <string>

// ======================================================================
// psc_mfields

template<typename F>
void Mfields<F>::write_as_mrc_fld(mrc_io *io, const std::vector<std::string>& comp_names)
{
  struct mrc_fld *fld = mrc_domain_m3_create(ppsc->mrc_domain_);
  mrc_fld_set_param_int(fld, "nr_ghosts", 0);
  mrc_fld_set_param_int(fld, "nr_comps", n_comps());
  mrc_fld_setup(fld);
  assert(comp_names.size() == n_comps());
  for (int m = 0; m < n_comps(); m++) {
    mrc_fld_set_comp_name(fld, m, comp_names[m].c_str());
  }

  for (int p = 0; p < n_patches(); p++) {
    mrc_fld_patch *m3p = mrc_fld_patch_get(fld, p);
    mrc_fld_foreach(fld, i,j,k, 0,0) {
      for (int m = 0; m < n_comps(); m++) {
	MRC_M3(m3p ,m , i,j,k) = (*this)[p](m, i,j,k);
      }
    } mrc_fld_foreach_end;
    mrc_fld_patch_put(fld);
  }
  
  mrc_fld_write(fld, io);
  mrc_fld_destroy(fld);
}

template
void Mfields<MFIELDS::fields_t>::write_as_mrc_fld(mrc_io *io, const std::vector<std::string>& comp_names);

#if defined(HAVE_LIBHDF5_HL) && (PSC_FIELDS_AS_SINGLE || PSC_FIELDS_AS_C)

#include <mrc_io.h>

// FIXME. This is a rather bad break of proper layering, HDF5 should be all
// mrc_io business. OTOH, it could be called flexibility...

#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

// ----------------------------------------------------------------------
// psc_mfields_write

static void
MPFX(write)(struct psc_mfields *mflds, struct mrc_io *io)
{
  mfields_t mf(mflds);
  herr_t ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group0 = H5Gopen(h5_file, mrc_io_obj_path(io, mflds), H5P_DEFAULT); H5_CHK(group0);

  for (int p = 0; p < mflds->nr_patches; p++) {
    fields_t flds = mf[p];
    char name[20]; sprintf(name, "flds%d", p);
    hid_t group = H5Gcreate(group0, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); H5_CHK(group);
    ierr = H5LTset_attribute_int(group, ".", "ib", flds.ib, 3); CE;
    ierr = H5LTset_attribute_int(group, ".", "im", flds.im, 3); CE;
    ierr = H5LTset_attribute_int(group, ".", "nr_comp", &flds.nr_comp, 1); CE;
    // write components separately instead?
    hsize_t hdims[4];
    hdims[0] = flds.nr_comp;
    hdims[1] = flds.im[2];
    hdims[2] = flds.im[1];
    hdims[3] = flds.im[0];
#if PSC_FIELDS_AS_SINGLE
    ierr = H5LTmake_dataset_float(group, "fields_single", 4, hdims, flds.data); CE;
#elif PSC_FIELDS_AS_C
    ierr = H5LTmake_dataset_double(group, "fields_c", 4, hdims, flds.data); CE;
#endif
    ierr = H5Gclose(group); CE;
  }

  ierr = H5Gclose(group0); CE;
}

// ----------------------------------------------------------------------
// psc_mfields_read

static void
MPFX(read)(struct psc_mfields *mflds, struct mrc_io *io)
{
  psc_mfields_read_super(mflds, io);

  psc_mfields_setup(mflds);

  mfields_t mf(mflds);

  herr_t ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group0 = H5Gopen(h5_file, mrc_io_obj_path(io, mflds), H5P_DEFAULT); H5_CHK(group0);

  for (int p = 0; p < mflds->nr_patches; p++) {
    fields_t flds = mf[p];
    char name[20]; sprintf(name, "flds%d", p);
    hid_t group = H5Gopen(group0, name, H5P_DEFAULT); H5_CHK(group);
    int ib[3], im[3], nr_comp;
    ierr = H5LTget_attribute_int(group, ".", "ib", ib); CE;
    ierr = H5LTget_attribute_int(group, ".", "im", im); CE;
    ierr = H5LTget_attribute_int(group, ".", "nr_comp", &nr_comp); CE;
    for (int d = 0; d < 3; d++) {
      assert(ib[d] == flds.ib[d]);
      assert(im[d] == flds.im[d]);
    }
    assert(nr_comp == flds.nr_comp);
#if PSC_FIELDS_AS_SINGLE
    ierr = H5LTread_dataset_float(group, "fields_single", flds.data); CE;
#elif PSC_FIELDS_AS_C
    ierr = H5LTread_dataset_double(group, "fields_c", flds.data); CE;
#endif
    ierr = H5Gclose(group); CE;
  }

  ierr = H5Gclose(group0); CE;
}

#endif

// ----------------------------------------------------------------------
// psc_mfields: subclass ops
  
struct MPFX(psc_mfields_ops) : psc_mfields_ops {
  using Wrapper_t = MfieldsWrapper<MFIELDS>;
  MPFX(psc_mfields_ops)() {
    name                  = Wrapper_t::name;
    size                  = Wrapper_t::size;
    setup                 = Wrapper_t::setup;
    destroy               = Wrapper_t::destroy;
  }
} MPFX(ops);

#if 0 //defined(HAVE_LIBHDF5_HL) && (PSC_FIELDS_AS_SINGLE || PSC_FIELDS_AS_C)
    write                 = MPFX(write);
    read                  = MPFX(read);
#endif

