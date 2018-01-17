
#include "psc_particles_vpic.h"

#include <psc_particles_single_by_kind.h>
#include <psc_method.h>

#include "vpic_iface.h"

// ======================================================================
// conversion

struct copy_ctx {
  struct psc_mparticles *mprts;
  int p;
  int im[3];
  float dx[3];
  float dVi;
};

template<typename F>
static void copy_to(struct psc_mparticles *mprts, struct psc_mparticles *mprts_to,
		    F convert_from)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);
  Particles *vmprts = sub->vmprts;
  
  int n_prts_by_patch[mprts->nr_patches];
  Simulation_mprts_get_size_all(sub->sim, vmprts, mprts->nr_patches, n_prts_by_patch);
  
  unsigned int off = 0;
  for (int p = 0; p < mprts->nr_patches; p++) {
    int n_prts = n_prts_by_patch[p];
    struct copy_ctx ctx = { .mprts = mprts_to, .p = p };
    Simulation_mprts_get_grid_nx_dx(sub->sim, vmprts, ctx.im, ctx.dx);
    for (int d = 0; d < 3; d++) {
      ctx.im[d] += 2; // add ghost points
    }
    ctx.dVi = 1.f / (ctx.dx[0] * ctx.dx[1] * ctx.dx[2]);
    vpic_mparticles_get_particles(vmprts, n_prts, off, convert_from, &ctx);

    off += n_prts;
  }
}

template<typename F>
static void copy_from(struct psc_mparticles *mprts, struct psc_mparticles *mprts_from,
		      F convert_to)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);
  Particles *vmprts = sub->vmprts;

  int n_prts_by_patch[mprts->nr_patches];
  for (int p = 0; p < mprts->nr_patches; p++) {
    n_prts_by_patch[p] = 0;
  }
  // reset particle counts to zero, then use push_back to add back new particles
  Simulation_mprts_resize_all(sub->sim, vmprts, mprts->nr_patches, n_prts_by_patch);
  psc_mparticles_get_size_all(mprts_from, n_prts_by_patch);
  
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct copy_ctx ctx = { .mprts = mprts_from, .p = p };
    Simulation_mprts_get_grid_nx_dx(sub->sim, vmprts, ctx.im, ctx.dx);
    for (int d = 0; d < 3; d++) {
      ctx.im[d] += 2; // add ghost points
    }
    ctx.dVi = 1.f / (ctx.dx[0] * ctx.dx[1] * ctx.dx[2]);
    struct vpic_mparticles_prt prt;

    int n_prts = n_prts_by_patch[p];
    for (int n = 0; n < n_prts; n++) {
      convert_to(&prt, n, &ctx);
      Simulation_mprts_push_back(sub->sim, vmprts, &prt);
    }
  }
}

// ======================================================================
// conversion to "single"

struct ConvertFromSingle
{
  void operator()(struct vpic_mparticles_prt *prt, int n, void *_ctx)
  {
    struct copy_ctx *ctx = (struct copy_ctx *) _ctx;
    particle_single_t *part = &mparticles_single_t(ctx->mprts)[ctx->p][n];
    
    assert(part->kind() < ppsc->nr_kinds);
    int *im = ctx->im;
    float *dx = ctx->dx;
    int i3[3];
    for (int d = 0; d < 3; d++) {
      float val = (&part->xi)[d] / dx[d];
      i3[d] = (int) val;
      //mprintf("d %d val %g xi %g\n", d, val, part->xi);
      assert(i3[d] >= -1 && i3[d] < im[d] + 1);
      prt->dx[d] = (val - i3[d]) * 2.f - 1.f;
      i3[d] += 1;
    }
    prt->i     = (i3[2] * im[1] + i3[1]) * im[0] + i3[0];
    prt->ux[0] = part->pxi;
    prt->ux[1] = part->pyi;
    prt->ux[2] = part->pzi;
    prt->w     = part->qni_wni / ppsc->kinds[part->kind_].q / ctx->dVi;
    prt->kind  = part->kind();
  }
};

struct ConvertToSingle
{
  void operator()(struct vpic_mparticles_prt *prt, int n, void *_ctx)
  {
    struct copy_ctx *ctx = (struct copy_ctx *) _ctx;
    particle_single_t *part = &mparticles_single_t(ctx->mprts)[ctx->p][n];
    
    assert(prt->kind < ppsc->nr_kinds);
    int *im = ctx->im;
    float *dx = ctx->dx;
    int i = prt->i;
    int i3[3];
    i3[2] = i / (im[0] * im[1]); i -= i3[2] * (im[0] * im[1]);
    i3[1] = i / im[0]; i-= i3[1] * im[0];
    i3[0] = i;
    part->xi      = (i3[0] - 1 + .5f * (1.f + prt->dx[0])) * dx[0];
    part->yi      = (i3[1] - 1 + .5f * (1.f + prt->dx[1])) * dx[1];
    part->zi      = (i3[2] - 1 + .5f * (1.f + prt->dx[2])) * dx[2];
    float w = part->zi / dx[2];
    if (!(w >= 0 && w <= im[2] - 2)) {
      printf("w %g im %d i3 %d dx %g\n", w, im[2], i3[2], prt->dx[2]);
    }
    assert(w >= 0 && w <= im[2] - 2);
    part->kind_   = prt->kind;
    part->pxi     = prt->ux[0];
    part->pyi     = prt->ux[1];
    part->pzi     = prt->ux[2];
    part->qni_wni = ppsc->kinds[prt->kind].q * prt->w * ctx->dVi;
  }
};

static void
psc_mparticles_vpic_copy_from_single(struct psc_mparticles *mprts,
				    struct psc_mparticles *mprts_single, unsigned int flags)
{
  ConvertFromSingle convert_from_single;
  copy_from(mprts, mprts_single, convert_from_single);
}

static void
psc_mparticles_vpic_copy_to_single(struct psc_mparticles *mprts,
				  struct psc_mparticles *mprts_single, unsigned int flags)
{
  ConvertToSingle convert_to_single;
  copy_to(mprts, mprts_single, convert_to_single);
}

// ======================================================================
// conversion to "single_by_kind"

static void
psc_mparticles_vpic_copy_from_single_by_kind(struct psc_mparticles *mprts,
				    struct psc_mparticles *mprts_single_by_kind, unsigned int flags)
{
  Particles *vmprts = psc_mparticles_vpic(mprts)->vmprts;
  bk_mparticles *bkmprts = psc_mparticles_single_by_kind(mprts_single_by_kind)->bkmprts;

  vpic_mparticles_copy_from_single_by_kind(vmprts, bkmprts);
}

static void
psc_mparticles_vpic_copy_to_single_by_kind(struct psc_mparticles *mprts,
				  struct psc_mparticles *mprts_single_by_kind, unsigned int flags)
{
  Particles *vmprts = psc_mparticles_vpic(mprts)->vmprts;
  bk_mparticles *bkmprts = psc_mparticles_single_by_kind(mprts_single_by_kind)->bkmprts;

  vpic_mparticles_copy_to_single_by_kind(vmprts, bkmprts);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_methods

static struct mrc_obj_method psc_mparticles_vpic_methods[] = {
  MRC_OBJ_METHOD("copy_to_single"  , psc_mparticles_vpic_copy_to_single),
  MRC_OBJ_METHOD("copy_from_single", psc_mparticles_vpic_copy_from_single),
  MRC_OBJ_METHOD("copy_to_single_by_kind"  , psc_mparticles_vpic_copy_to_single_by_kind),
  MRC_OBJ_METHOD("copy_from_single_by_kind", psc_mparticles_vpic_copy_from_single_by_kind),
  {}
};

// ----------------------------------------------------------------------
// psc_mparticles_vpic_setup

static void
psc_mparticles_vpic_setup(struct psc_mparticles *mprts)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);

  psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sub->sim);
  sub->vmprts = Simulation_get_particles(sub->sim);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_get_size_all

static void
psc_mparticles_vpic_get_size_all(struct psc_mparticles *mprts, int *n_prts_by_patch)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);
  
  Simulation_mprts_get_size_all(sub->sim, sub->vmprts, mprts->nr_patches, n_prts_by_patch);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_reserve_all

static void
psc_mparticles_vpic_reserve_all(struct psc_mparticles *mprts, int *n_prts_by_patch)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);

  Simulation_mprts_reserve_all(sub->sim, sub->vmprts, mprts->nr_patches, n_prts_by_patch);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_resize_all

static void
psc_mparticles_vpic_resize_all(struct psc_mparticles *mprts, int *n_prts_by_patch)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);

  Simulation_mprts_resize_all(sub->sim, sub->vmprts, mprts->nr_patches, n_prts_by_patch);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_get_nr_particles

static unsigned int
psc_mparticles_vpic_get_nr_particles(struct psc_mparticles *mprts)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);
  
  return Simulation_mprts_get_nr_particles(sub->sim, sub->vmprts);
}

// ----------------------------------------------------------------------
// psc_mparticles_vpic_inject

static void
psc_mparticles_vpic_inject(struct psc_mparticles *mprts, int p,
			   const struct psc_particle_inject *prt)
{
  struct psc_mparticles_vpic *sub = psc_mparticles_vpic(mprts);

  Simulation_inject_particle(sub->sim, sub->vmprts, p, prt);
}

// ----------------------------------------------------------------------
// psc_mparticles: subclass "vpic"
  
struct psc_mparticles_ops_vpic : psc_mparticles_ops {
  psc_mparticles_ops_vpic() {
    name                    = "vpic";
    size                    = sizeof(struct psc_mparticles_vpic);
    methods                 = psc_mparticles_vpic_methods;
    setup                   = psc_mparticles_vpic_setup;
    reserve_all             = psc_mparticles_vpic_reserve_all;
    get_size_all            = psc_mparticles_vpic_get_size_all;
    resize_all              = psc_mparticles_vpic_resize_all;
    get_nr_particles        = psc_mparticles_vpic_get_nr_particles;
    inject                  = psc_mparticles_vpic_inject;
  }
} psc_mparticles_vpic_ops;

