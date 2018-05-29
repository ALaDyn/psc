
#pragma once

#include "psc.h"
#include "fields.hxx"
#include "bnd.hxx"
#include "psc_balance.h"

#include <mrc_profile.h>
#include <mrc_ddc.h>

struct CudaBnd
{
  using Mfields = MfieldsCuda;
  using fields_t = typename Mfields::fields_t;
  using real_t = typename Mfields::real_t;
  using Fields = Fields3d<fields_t>;

  // ----------------------------------------------------------------------
  // ctor
  
  CudaBnd(const Grid_t& grid, mrc_domain* domain, int ibn[3])
  {
    static struct mrc_ddc_funcs ddc_funcs = {
      .copy_to_buf   = copy_to_buf,
      .copy_from_buf = copy_from_buf,
      .add_from_buf  = add_from_buf,
    };

    ddc_ = mrc_domain_create_ddc(domain);
    mrc_ddc_set_funcs(ddc_, &ddc_funcs);
    mrc_ddc_set_param_int3(ddc_, "ibn", ibn);
    mrc_ddc_set_param_int(ddc_, "max_n_fields", 24);
    mrc_ddc_set_param_int(ddc_, "size_of_type", sizeof(real_t));
    mrc_ddc_setup(ddc_);
  }

  // ----------------------------------------------------------------------
  // dtor
  
  ~CudaBnd()
  {
    mrc_ddc_destroy(ddc_);
  }

  // ----------------------------------------------------------------------
  // add_ghosts
  
  void add_ghosts(Mfields& mflds, int mb, int me)
  {
    auto& mflds_single = mflds.template get_as<MfieldsSingle>(mb, me);
    mrc_ddc_add_ghosts(ddc_, mb, me, &mflds_single);
    mflds.put_as(mflds_single, mb, me);
  }
  
  // ----------------------------------------------------------------------
  // fill_ghosts

  void fill_ghosts(Mfields& mflds, int mb, int me)
  {
    // FIXME
    // I don't think we need as many points, and only stencil star
    // rather then box
    auto& mflds_single = mflds.template get_as<MfieldsSingle>(mb, me);
    mrc_ddc_fill_ghosts(ddc_, mb, me, &mflds_single);
    mflds.put_as(mflds_single, mb, me);
  }

  // ----------------------------------------------------------------------
  // copy_to_buf

  static void copy_to_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			  void *_buf, void *ctx)
  {
    auto& mf = *static_cast<MfieldsSingle*>(ctx);
    auto F = mf[p];
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    MRC_DDC_BUF3(buf, m - mb, ix,iy,iz) = F(m, ix,iy,iz);
	  }
	}
      }
    }
  }

  // ----------------------------------------------------------------------
  // add_from_buf

  static void add_from_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			   void *_buf, void *ctx)
  {
    auto& mf = *static_cast<MfieldsSingle*>(ctx);
    auto F = mf[p];
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    real_t val = F(m, ix,iy,iz) + MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	    F(m, ix,iy,iz) = val;
	  }
	}
      }
    }
  }
  
  // ----------------------------------------------------------------------
  // copy_from_buf

  static void copy_from_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			    void *_buf, void *ctx)
  {
    auto& mf = *static_cast<MfieldsSingle*>(ctx);
    auto F = mf[p];
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    F(m, ix,iy,iz) = MRC_DDC_BUF3(buf, m - mb, ix,iy,iz);
	  }
	}
      }
    }
  }

private:
  mrc_ddc* ddc_;
};

template<typename MF>
struct BndCuda3 : BndBase
{
  using Mfields = MF;

  BndCuda3(const Grid_t& grid, mrc_domain* domain, int ibn[3]);
  ~BndCuda3();
  
  void reset();
  void add_ghosts(Mfields& mflds, int mb, int me);
  void fill_ghosts(Mfields& mflds, int mb, int me);

  void add_ghosts(PscMfieldsBase mflds_base, int mb, int me) override
  {
    assert(0);
  }

  void fill_ghosts(PscMfieldsBase mflds_base, int mb, int me) override
  {
    assert(0);
  }

private:
  CudaBnd* cbnd_;
  int balance_generation_cnt_;
};

// ----------------------------------------------------------------------
// ctor

template<typename MF>
BndCuda3<MF>::BndCuda3(const Grid_t& grid, mrc_domain* domain, int ibn[3])
  : cbnd_{new CudaBnd{grid, domain, ibn}},
    balance_generation_cnt_{psc_balance_generation_cnt}
{}

// ----------------------------------------------------------------------
// dtor

template<typename MF>
BndCuda3<MF>::~BndCuda3()
{
  delete cbnd_;
}
  
// ----------------------------------------------------------------------
// reset

template<typename MF>
void BndCuda3<MF>::reset()
{
  // FIXME, not really a pretty way of doing this
  delete cbnd_;
  cbnd_ = new CudaBnd{ppsc->grid(), ppsc->mrc_domain_, ppsc->ibn};
}
  
// ----------------------------------------------------------------------
// add_ghosts

template<typename MF>
void BndCuda3<MF>::add_ghosts(Mfields& mflds, int mb, int me)
{
  if (psc_balance_generation_cnt != balance_generation_cnt_) {
    reset();
  }
  cbnd_->add_ghosts(mflds, mb, me);
}

// ----------------------------------------------------------------------
// fill_ghosts

template<typename MF>
void BndCuda3<MF>::fill_ghosts(Mfields& mflds, int mb, int me)
{
  if (psc_balance_generation_cnt != balance_generation_cnt_) {
    reset();
  }
  cbnd_->fill_ghosts(mflds, mb, me);
}

