
#include "psc_marder_private.h"
#include "psc_bnd.h"
#include "psc_output_fields_item.h"
#include "psc_fields_cuda.h"
#include "cuda_iface.h"

#include "marder.hxx"
#include "fields_item.hxx"

#include <mrc_io.h>

#include <stdlib.h>

// FIXME: checkpointing won't properly restore state
// FIXME: if the subclass creates objects, it'd be cleaner to have them
// be part of the subclass

struct MarderCuda : MarderBase
{
  using real_t = MfieldsCuda::fields_t::real_t;
  
  MarderCuda(MPI_Comm comm, real_t diffusion, int loop, bool dump)
    : diffusion_{diffusion},
      loop_{loop},
      dump_{dump}
  {
    bnd_ = psc_bnd_create(comm);
    psc_bnd_set_name(bnd_, "marder_bnd");
    psc_bnd_set_type(bnd_, "cuda");
    psc_bnd_set_psc(bnd_, ppsc);
    psc_bnd_setup(bnd_);

    // FIXME, output_fields should be taking care of their own psc_bnd?
    item_div_e = psc_output_fields_item_create(psc_comm(ppsc));
    psc_output_fields_item_set_type(item_div_e, "dive_cuda");
    psc_output_fields_item_set_psc_bnd(item_div_e, bnd_);
    psc_output_fields_item_setup(item_div_e);

    item_rho = psc_output_fields_item_create(psc_comm(ppsc));
    psc_output_fields_item_set_type(item_rho, "rho_1st_nc_cuda");
    psc_output_fields_item_set_psc_bnd(item_rho, bnd_);
    psc_output_fields_item_setup(item_rho);

    if (dump_) {
      io_ = mrc_io_create(psc_comm(ppsc));
      mrc_io_set_type(io_, "xdmf_collective");
      mrc_io_set_name(io_, "mrc_io_marder");
      mrc_io_set_param_string(io_, "basename", "marder");
      mrc_io_set_from_options(io_);
      mrc_io_setup(io_);
    }
  }

  ~MarderCuda()
  {
    psc_bnd_destroy(bnd_);
    psc_output_fields_item_destroy(item_div_e);
    psc_output_fields_item_destroy(item_rho);
    if (dump_) {
      mrc_io_destroy(io_);
    }
  }
  
  void calc_aid_fields(PscMfieldsBase mflds_base, PscMparticlesBase mprts_base)
  {
    PscFieldsItemBase item_div_e(this->item_div_e);
    PscFieldsItemBase item_rho(this->item_rho);
    item_div_e(mflds_base, mprts_base); // FIXME, should accept NULL for particles
  
    if (dump_) {
      static int cnt;
      mrc_io_open(io_, "w", cnt, cnt);//ppsc->timestep, ppsc->timestep * ppsc->dt);
      cnt++;
      psc_mfields_write_as_mrc_fld(item_rho->mres().mflds(), io_);
      psc_mfields_write_as_mrc_fld(item_div_e->mres().mflds(), io_);
      mrc_io_close(io_);
    }

    item_div_e->mres()->axpy_comp(0, -1., *item_rho->mres().sub(), 0);
    // FIXME, why is this necessary?
    auto bnd = PscBndBase(bnd_);
    bnd.fill_ghosts(item_div_e->mres(), 0, 1);
  }

  // ----------------------------------------------------------------------
  // psc_marder_cuda_correct
  //
  // Do the modified marder correction (See eq.(5, 7, 9, 10) in Mardahl and Verboncoeur, CPC, 1997)

  void correct(struct psc_mfields *_mflds_base, struct psc_mfields *_mf_base)
  {
    auto mflds_base = PscMfieldsBase{_mflds_base};
    auto mf_base = PscMfieldsBase{_mf_base};
    assert(mflds_base->grid().isInvar(0));

    const Grid_t& grid = ppsc->grid();
    // FIXME: how to choose diffusion parameter properly?
    float dx[3];
    for (int d = 0; d < 3; d++) {
      dx[d] = grid.domain.dx[d];
    }
    float inv_sum = 0.;
    for (int d = 0; d < 3; d++) {
      if (!grid.isInvar(d)) {
	inv_sum += 1. / sqr(grid.domain.dx[d]);
      }
    }
    float diffusion_max = 1. / 2. / (.5 * ppsc->dt) / inv_sum;
    float diffusion     = diffusion_max * diffusion_;
    
    float fac[3];
    fac[0] = 0.f;
    fac[1] = .5 * ppsc->dt * diffusion / dx[1];
    fac[2] = .5 * ppsc->dt * diffusion / dx[2];

    auto& mflds = mflds_base->get_as<MfieldsCuda>(EX, EX + 3);
    auto& mf = mf_base->get_as<MfieldsCuda>(0, 1);
    cuda_mfields *cmflds = mflds.cmflds;
    cuda_mfields *cmf = mf.cmflds;

    // OPT, do all patches in one kernel
    for (int p = 0; p < mf.n_patches(); p++) {
      int l_cc[3] = {0, 0, 0}, r_cc[3] = {0, 0, 0};
      int l_nc[3] = {0, 0, 0}, r_nc[3] = {0, 0, 0};
      for (int d = 0; d < 3; d++) {
	if (grid.bc.fld_lo[d] == BND_FLD_CONDUCTING_WALL &&
	    psc_at_boundary_lo(ppsc, p, d)) {
	  l_cc[d] = -1;
	  l_nc[d] = -1;
	}
	if (grid.bc.fld_hi[d] == BND_FLD_CONDUCTING_WALL &&
	    psc_at_boundary_hi(ppsc, p, d)) {
	  r_cc[d] = -1;
	  r_nc[d] = 0;
	}
      }
    
      const int *ldims = ppsc->grid().ldims;
    
      int ly[3] = { l_nc[0], l_cc[1], l_nc[2] };
      int ry[3] = { r_nc[0] + ldims[0], r_cc[1] + ldims[1], r_nc[2] + ldims[2] };
    
      int lz[3] = { l_nc[0], l_nc[1], l_cc[2] };
      int rz[3] = { r_nc[0] + ldims[0], r_nc[1] + ldims[1], r_cc[2] + ldims[2] };
    
      cuda_marder_correct_yz(cmflds, cmf, p, fac, ly, ry, lz, rz);
    }

    mflds_base->put_as(mflds, EX, EX + 3);
    mf_base->put_as(mf, 0, 0);
  }

  void run(PscMfieldsBase mflds_base, PscMparticlesBase mprts_base) override
  {
    PscFieldsItemBase item_rho(this->item_rho);
    PscFieldsItemBase item_div_e(this->item_div_e);
    item_rho(mflds_base, mprts_base);
  
    // need to fill ghost cells first (should be unnecessary with only variant 1) FIXME
    auto bnd = PscBndBase(ppsc->bnd);
    bnd.fill_ghosts(mflds_base, EX, EX+3);
  
    for (int i = 0; i < loop_; i++) {
      calc_aid_fields(mflds_base, mprts_base);
      correct(mflds_base.mflds(), item_div_e->mres().mflds());
      auto bnd = PscBndBase(ppsc->bnd);
      bnd.fill_ghosts(mflds_base, EX, EX+3);
    }
  }

private:
  real_t diffusion_; //< diffusion coefficient for Marder correction
  int loop_; //< execute this many relaxation steps in a loop
  bool dump_; //< dump div_E, rho

  psc_bnd* bnd_; //< for filling ghosts on rho, div_e
  psc_output_fields_item* item_div_e;
  psc_output_fields_item* item_rho;
  mrc_io *io_; //< for debug dumping
};

// ======================================================================
// psc_marder: subclass "cuda"

struct psc_marder_ops_cuda : psc_marder_ops {
  using Wrapper = MarderWrapper<MarderCuda>;
  psc_marder_ops_cuda() {
    name                  = "cuda";
    size                  = Wrapper::size;
    setup                 = Wrapper::setup;
    destroy               = Wrapper::destroy;
  }
} psc_marder_cuda_ops;

