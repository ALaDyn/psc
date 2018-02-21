
#include "psc_fields_vpic.h"

#include "psc_fields_c.h"
#include "psc_fields_single.h"
#include "psc.h"
#include "psc_method.h"
#include "psc_particles_vpic.h"
#include "fields.hxx"

#include "mrc_domain.h"
#include "mrc_bits.h"

#include "vpic_iface.h"

using Fields = Fields3d<fields_vpic_t>;
using FieldsS = Fields3d<fields_single_t>;

static const int map_psc2vpic[VPIC_MFIELDS_N_COMP] = {
  [JXI] = VPIC_MFIELDS_JFX, [JYI] = VPIC_MFIELDS_JFY, [JZI] = VPIC_MFIELDS_JFZ,
  [EX]  = VPIC_MFIELDS_EX , [EY]  = VPIC_MFIELDS_EY , [EZ]  = VPIC_MFIELDS_EZ,
  [HX]  = VPIC_MFIELDS_BX , [HY]  = VPIC_MFIELDS_BY , [HZ]  = VPIC_MFIELDS_BZ,

  [9]   = VPIC_MFIELDS_TCAX, [10]  = VPIC_MFIELDS_TCAY, [11]  = VPIC_MFIELDS_TCAZ,
  [12]  = VPIC_MFIELDS_DIV_E_ERR, [13] = VPIC_MFIELDS_DIV_B_ERR,
  [14]  = VPIC_MFIELDS_RHOB     , [15] = VPIC_MFIELDS_RHOF,
  [16]  = 16, [17] = 17, [18] = 18, [19] = 19,
};

void psc_mfields_vpic::accumulate_rho_p(Particles *vmprts)
{
  Simulation_accumulate_rho_p(sim, vmprts, vmflds_fields);
}

// ======================================================================
// convert from/to "single"

static void
psc_mfields_vpic_copy_from_single(struct psc_mfields *mflds, struct psc_mfields *mflds_single,
				  int mb, int me)
{
  mfields_single_t mf_single(mflds_single);
  mfields_vpic_t mf(mflds);
  for (int p = 0; p < mf->n_patches(); p++) {
    fields_vpic_t flds = mf[p];
    Fields F(flds);
    FieldsS F_s(mf_single[p]);

    
    // FIXME, hacky way to distinguish whether we want
    // to copy the field into the standard PSC component numbering
    if (mflds->nr_fields == VPIC_MFIELDS_N_COMP) {
      for (int m = mb; m < me; m++) {
	int m_vpic = map_psc2vpic[m];
	for (int jz = flds.ib[2]; jz < flds.ib[2] + flds.im[2]; jz++) {
	  for (int jy = flds.ib[1]; jy < flds.ib[1] + flds.im[1]; jy++) {
	    for (int jx = flds.ib[0]; jx < flds.ib[0] + flds.im[0]; jx++) {
	      F(m_vpic, jx,jy,jz) = F_s(m, jx,jy,jz);
	    }
	  }
	}
      }
    } else {
      assert(0);
    }
  }
}

static void
psc_mfields_vpic_copy_to_single(struct psc_mfields *mflds, struct psc_mfields *mflds_single,
			   int mb, int me)
{
  mfields_single_t mf_single(mflds_single);
  mfields_vpic_t mf(mflds);
  for (int p = 0; p < mf->n_patches(); p++) {
    fields_vpic_t flds = mf[p];
    fields_single_t flds_single = mf_single[p];
    Fields F(flds);
    FieldsS F_s(flds_single);

    int ib[3], ie[3];
    for (int d = 0; d < 3; d++) {
      ib[d] = MAX(flds.ib[d], flds_single.ib[d]);
      ie[d] = MIN(flds.ib[d] + flds.im[d], flds_single.ib[d] + flds_single.im[d]);
    }

    // FIXME, hacky way to distinguish whether we want
    // to copy the field into the standard PSC component numbering or,
    // as in this case, just copy one-to-one
    if (mflds->nr_fields == VPIC_HYDRO_N_COMP) {
      for (int m = mb; m < me; m++) {
	for (int jz = ib[2]; jz < ie[2]; jz++) {
	  for (int jy = ib[1]; jy < ie[1]; jy++) {
	    for (int jx = ib[0]; jx < ie[0]; jx++) {
	      F_s(m, jx,jy,jz) = F(m, jx,jy,jz);
	    }
	  }
	}
      }
    } else if (mflds->nr_fields == VPIC_MFIELDS_N_COMP) {
      for (int m = mb; m < me; m++) {
	int m_vpic = map_psc2vpic[m];
	for (int jz = ib[2]; jz < ie[2]; jz++) {
	  for (int jy = ib[1]; jy < ie[1]; jy++) {
	    for (int jx = ib[0]; jx < ie[0]; jx++) {
	      F_s(m, jx,jy,jz) = F(m_vpic, jx,jy,jz);
	    }
	  }
	}
      }
    } else {
      assert(0);
    }
  }
}

// ======================================================================

static int ref_count_fields, ref_count_hydro;

// ----------------------------------------------------------------------
// psc_mfields_vpic_setup

static void psc_mfields_vpic_setup(struct psc_mfields *mflds)
{
  struct psc_mfields_vpic *sub = psc_mfields_vpic(mflds);

  psc_mfields_setup_super(mflds);

  int n_patches;
  mrc_domain_get_patches(mflds->domain, &n_patches);
  assert(n_patches == 1);
  assert(mflds->ibn[0] == 1);
  assert(mflds->ibn[1] == 1);
  assert(mflds->ibn[2] == 1);
  assert(mflds->first_comp == 0);

  psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sub->sim);

  if (mflds->nr_fields == VPIC_MFIELDS_N_COMP) {
    // make sure we notice if we create a second psc_mfields
    // which would share its memory with the first
    assert(ref_count_fields == 0);
    ref_count_fields++;

    sub->vmflds_fields = Simulation_get_FieldArray(sub->sim);
  } else if (mflds->nr_fields == VPIC_HYDRO_N_COMP) {
    // make sure we notice if we create a second psc_mfields
    // which would share its memory with the first
    assert(ref_count_hydro == 0);
    ref_count_hydro++;

    sub->vmflds_hydro = Simulation_get_HydroArray(sub->sim);
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_mfields_vpic_destroy

static void psc_mfields_vpic_destroy(struct psc_mfields *mflds)
{
  if (mflds->nr_fields == VPIC_MFIELDS_N_COMP) {
    ref_count_fields--;
  } else if (mflds->nr_fields == VPIC_HYDRO_N_COMP) {
    ref_count_hydro--;
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_mfields_vpic_get_field_t

fields_vpic_t psc_mfields_vpic_get_field_t(struct psc_mfields *mflds, int p)
{
  struct psc_mfields_vpic *sub = mrc_to_subobj(mflds, struct psc_mfields_vpic);

  assert(mflds->first_comp == 0);

  // FIXME hacky...
  if (mflds->nr_fields == VPIC_MFIELDS_N_COMP) {
    int ib[3], im[3];
    float* data = Simulation_mflds_getData(sub->sim, sub->vmflds_fields, ib, im);
    return fields_vpic_t(ib, im, VPIC_MFIELDS_N_COMP, 0, data);
  } else if (mflds->nr_fields == VPIC_HYDRO_N_COMP) {
    int ib[3], im[3];
    float* data = Simulation_hydro_getData(sub->sim, sub->vmflds_hydro, ib, im);
    return fields_vpic_t(ib, im, VPIC_HYDRO_N_COMP, 0, data);
  } else {
    assert(0);
  }
  abort();
}

// ----------------------------------------------------------------------
// forwards

// ======================================================================
// psc_mfields: subclass "vpic"

static struct mrc_obj_method psc_mfields_vpic_methods[] = {
  MRC_OBJ_METHOD("copy_to_single"  , psc_mfields_vpic_copy_to_single),
  MRC_OBJ_METHOD("copy_from_single", psc_mfields_vpic_copy_from_single),
  {}
};

struct psc_mfields_ops_vpic : psc_mfields_ops {
  psc_mfields_ops_vpic() {
    name                  = "vpic";
    size                  = sizeof(struct psc_mfields_vpic);
    methods               = psc_mfields_vpic_methods;
    setup                 = psc_mfields_vpic_setup;
    destroy               = psc_mfields_vpic_destroy;
#ifdef HAVE_LIBHDF5_HL
    write                 = psc_mfields_vpic_write;
    read                  = psc_mfields_vpic_read;
#endif
  }
} psc_mfields_vpic_ops;


