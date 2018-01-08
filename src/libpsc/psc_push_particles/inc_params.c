
#if DIM == DIM_1
using dim_t = dim_1;
#elif DIM == DIM_X
using dim_t = dim_x;
#elif DIM == DIM_Y
using dim_t = dim_y;
#elif DIM == DIM_Z
using dim_t = dim_z;
#elif DIM == DIM_XY
using dim_t = dim_xy;
#elif DIM == DIM_XZ
using dim_t = dim_xz;
#elif DIM == DIM_YZ
using dim_t = dim_yz;
#elif DIM == DIM_XYZ
using dim_t = dim_xyz;
#endif


#define CUDA_CONSTANT
#define CUDA_DEVICE
#define __forceinline__
#define atomicAdd(addr, val) \
  do { *(addr) += (val); } while (0)


// ----------------------------------------------------------------------
// c_prm: constant parameters

#define MAX_NR_KINDS (10)

struct const_params {
  particle_real_t dt; // FIXME, do we need both dt and dqs? or maybe get rid of xl/yl/zl
  particle_real_t dqs;
  particle_real_t fnqs;
  particle_real_t fnqxs, fnqys, fnqzs;
  particle_real_t dxi[3];
};

struct params_1vb {
  // particle-related
  particle_real_t dq_kind[MAX_NR_KINDS];
  int b_mx[3];

  // field-related
  int mx[3];
  int ilg[3];
};

CUDA_CONSTANT static struct const_params c_prm;
CUDA_CONSTANT static struct params_1vb prm;

// ----------------------------------------------------------------------
// c_prm_set

static void
c_prm_set(struct psc *psc)
{
  struct const_params prm;

  prm.dt = psc->dt;
  prm.dqs = .5f * ppsc->coeff.eta * prm.dt;
  prm.fnqs = sqr(psc->coeff.alpha) * psc->coeff.cori / psc->coeff.eta;

  assert(psc->nr_patches > 0);

  for (int d = 0; d < 3; d++) {
    prm.dxi[d] = 1.f / psc->patch[0].dx[d];
  }

  prm.fnqxs = ppsc->patch[0].dx[0] * prm.fnqs / prm.dt;
  prm.fnqys = ppsc->patch[0].dx[1] * prm.fnqs / prm.dt;
  prm.fnqzs = ppsc->patch[0].dx[2] * prm.fnqs / prm.dt;

#ifndef __CUDACC__
  c_prm = prm;
#else
  check(cudaMemcpyToSymbol(c_prm, &prm, sizeof(prm)));
#endif
}

// ----------------------------------------------------------------------
// params_1vb

static void _mrc_unused
params_1vb_set(struct psc *psc,
	       struct psc_mparticles *mprts, struct psc_mfields *mflds)
{
  struct params_1vb params;

  particle_real_t dt = psc->dt;

#if CALC_J == CALC_J_1VB_2D && DIM != DIM_YZ
#error inc_params.c: CALC_J_1VB_2D only works for DIM_YZ
#endif

  assert(psc->nr_kinds <= MAX_NR_KINDS);
  for (int k = 0; k < ppsc->nr_kinds; k++) {
    params.dq_kind[k] = .5f * ppsc->coeff.eta * dt * ppsc->kinds[k].q / ppsc->kinds[k].m;
  }

  if (mprts && mprts->nr_patches > 0) {
#if PSC_PARTICLES_AS_CUDA2
    struct psc_mparticles_cuda2 *mprts_sub = psc_mparticles_cuda2(mprts);
    for (int d = 0; d < 3; d++) {
      params.b_mx[d] = mprts_sub->b_mx[d];
    }
#else
    assert(0);
#endif
  }

  if (mflds) {
#if PSC_FIELDS_AS_CUDA2
    struct psc_mfields_cuda2 * mflds_sub = psc_mfields_cuda2(mflds);
    for (int d = 0; d < 3; d++) {
      params.mx[d] = mflds_sub->im[d];
      params.ilg[d] = mflds_sub->ib[d];
      assert(mflds_sub->ib[d] == -2 || mflds_sub->im[d] == 1); // assumes BND == 2
    }
#else
    assert(0);
#endif
  }

#ifndef __CUDACC__
  prm = params;
#else
  check(cudaMemcpyToSymbol(prm, &params, sizeof(prm)));
#endif
}

