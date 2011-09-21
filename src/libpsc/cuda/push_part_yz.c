
#include "psc_cuda.h"

#include <mrc_profile.h>
#include <math.h>

#if 0

__constant__ static real d_dt, d_dxi[3], d_dqs;
__constant__ int d_mx[3], d_iglo[3];
__constant__ int d_b_mx[3];

static void
set_constants(particles_cuda_t *pp)
{
  real __dt = ppsc->dt, __dqs = .5f * ppsc->coeff.eta * ppsc->dt;
  check(cudaMemcpyToSymbol(d_dt, &__dt, sizeof(d_dt)));
  check(cudaMemcpyToSymbol(d_dqs, &__dqs, sizeof(d_dqs)));
  real __dxi[3] = { 1.f / ppsc->dx[0], 1.f / ppsc->dx[1], 1.f / ppsc->dx[2] };
  check(cudaMemcpyToSymbol(d_dxi, __dxi, sizeof(d_dxi)));
  check(cudaMemcpyToSymbol(d_mx, ppsc->img, sizeof(d_mx)));
  check(cudaMemcpyToSymbol(d_iglo, ppsc->ilg, sizeof(d_iglo)));
  check(cudaMemcpyToSymbol(d_b_mx, pp->b_mx, sizeof(d_mx)));
}

__device__ static inline void
blockIdx_to_blockCrd(int bidx, int bi[3])
{
  bi[2] = bidx / (d_b_mx[1] * d_b_mx[0]);
  bidx -= bi[2] * (d_b_mx[1] * d_b_mx[0]);
  bi[1] = bidx / d_b_mx[0];
  bidx -= bi[1] * d_b_mx[0];
  bi[0] = bidx;
}

// ======================================================================
// field caching

__shared__ real fld_cache[6 * (BLOCKSIZE_Z + 4) * (BLOCKSIZE_Y + 4) * 1]; //yz

#if 1

#define F3C(fldnr, jx,jy,jz)						\
  (*({									\
      int off = ((((fldnr-EX)						\
		   *(BLOCKSIZE_Z + 4) + ((jz)-(-2)))			\
		  *(BLOCKSIZE_Y + 4) + ((jy)-(-2)))			\
		 *1 + ((jx)));						\
      &(fld_cache[off]);						\
    }))

#else

#define F3C(fldnr, jx,jy,jz)						\
  (*({									\
      int off = ((((fldnr-EX)						\
		   *(BLOCKSIZE_Z+4) + ((jz)-(-2)))			\
		  *(BLOCKSIZE_Y+4) + ((jy)-(-2)))			\
		 *1 + ((jx)));						\
      if (jx != 0) printf("!!! jx\n");					\
      if (jy < -2 || jy >= BLOCKSIZE_Y + 2) printf("!!! jy %d\n", jy);	\
      if (jz < -2 || jz >= BLOCKSIZE_Z + 2) printf("!!! jz %d\n", jz);	\
      &(fld_cache[off]);						\
    }))

#endif

__device__ static void
cache_fields(real *d_flds, const int l[3])
{
#ifdef __CUDACC__
  int ti = threadIdx.x;
  int n = BLOCKSIZE_X * (BLOCKSIZE_Y + 4) * (BLOCKSIZE_Z + 4);
  while (ti < n) {
    int tmp = ti;
    int jx = tmp % BLOCKSIZE_X;
    tmp /= BLOCKSIZE_X;
    int jy = tmp % (BLOCKSIZE_Y + 4) - 2;
    tmp /= BLOCKSIZE_Y + 4;
    int jz = tmp % (BLOCKSIZE_Z + 4) - 2;
    //    tmp /= BLOCKSIZE_Z + 4;
    //    int m = tmp + EX;
    //    printf("n %d ti %d m %d, jx %d,%d,%d\n", n, ti, m, jx, jy, jz);
    // currently it seems faster to do the loop rather than do m by threadidx
    for (int m = EX; m <= HZ; m++) {
      F3C(m, jx,jy,jz) = F3(m, jx+l[0],jy+l[1],jz+l[2]);
    }
    ti += blockDim.x;
  }
  __syncthreads();
#else
  int ti = threadIdx.x;
  int n = BLOCKSIZE_X * (BLOCKSIZE_Y + 4) * (BLOCKSIZE_Z + 4);
  if (ti == 0) {
    for (ti = 0; ti < n; ti++) {
      int tmp = ti;
      int jx = tmp % BLOCKSIZE_X;
      tmp /= BLOCKSIZE_X;
      int jy = tmp % (BLOCKSIZE_Y + 4) - 2;
      tmp /= BLOCKSIZE_Y + 4;
      int jz = tmp % (BLOCKSIZE_Z + 4) - 2;
      //    tmp /= BLOCKSIZE_Z + 4;
      //    int m = tmp + EX;
      //    printf("n %d ti %d m %d, jx %d,%d,%d\n", n, ti, m, jx, jy, jz);
      // currently it seems faster to do the loop rather than do m by threadidx
      for (int m = EX; m <= HZ; m++) {
	F3C(m, jx,jy,jz) = F3(m, jx+l[0],jy+l[1],jz+l[2]);
      }
    }
  }
#endif
}

// ======================================================================

__device__ static void
calc_vxi(real vxi[3], struct d_particle p)
{
  real root = rsqrtr(real(1.) + sqr(p.pxi[0]) + sqr(p.pxi[1]) + sqr(p.pxi[2]));

  for (int d = 1; d < 3; d++) {
    vxi[d] = p.pxi[d] * root;
  }
}

__device__ static void
push_xi_halfdt(struct d_particle *p, const real vxi[3])
{
  for (int d = 1; d < 3; d++) {
    p->xi[d] += real(.5) * d_dt * vxi[d];
  }
}

__device__ static void
find_idx_off(const real xi[3], int j[3], real h[3], real shift)
{
  for (int d = 0; d < 3; d++) {
    real pos = xi[d] * d_dxi[d] + shift;
    j[d] = nint(pos);
    h[d] = j[d] - pos;
  }
}

__device__ static real
ip_to_grid_m(real h)
{
  return real(.5) * sqr(real(.5) + h);
}

__device__ static real
ip_to_grid_0(real h)
{
  return real(.75) - sqr(h);
}

__device__ static real
ip_to_grid_p(real h)
{
  return real(.5) * sqr(real(.5) - h);
}

#define ARR_3_off(arr, d, o) (arr[(d-1)*3 + ((o)+1)])

__device__ static void
find_interp_to_grid_coeff(real *arr, const real h[3])
{
  for (int d = 1; d < 3; d++) {
    ARR_3_off(arr, d, -1) = ip_to_grid_m(h[d]);
    ARR_3_off(arr, d,  0) = ip_to_grid_0(h[d]);
    ARR_3_off(arr, d, +1) = ip_to_grid_p(h[d]);
  }
}

__device__ static void
push_pxi_dt(struct d_particle *p,
	    real exq, real eyq, real ezq, real hxq, real hyq, real hzq)
{
  real dq = p->qni_div_mni * d_dqs;
  real pxm = p->pxi[0] + dq*exq;
  real pym = p->pxi[1] + dq*eyq;
  real pzm = p->pxi[2] + dq*ezq;
  
  real root = dq * rsqrtr(real(1.) + sqr(pxm) + sqr(pym) + sqr(pzm));
  real taux = hxq * root, tauy = hyq * root, tauz = hzq * root;
  
  real tau = real(1.) / (real(1.) + sqr(taux) + sqr(tauy) + sqr(tauz));
  real pxp = ( (real(1.) + sqr(taux) - sqr(tauy) - sqr(tauz)) * pxm
	       +(real(2.)*taux*tauy + real(2.)*tauz)*pym
	       +(real(2.)*taux*tauz - real(2.)*tauy)*pzm)*tau;
  real pyp = ( (real(2.)*taux*tauy - real(2.)*tauz)*pxm
	       +(real(1.) - sqr(taux) + sqr(tauy) - sqr(tauz)) * pym
	       +(real(2.)*tauy*tauz + real(2.)*taux)*pzm)*tau;
  real pzp = ( (real(2.)*taux*tauz + real(2.)*tauy)*pxm
	       +(real(2.)*tauy*tauz - real(2.)*taux)*pym
	       +(real(1.) - sqr(taux) - sqr(tauy) + sqr(tauz))*pzm)*tau;
  
  p->pxi[0] = pxp + dq * exq;
  p->pxi[1] = pyp + dq * eyq;
  p->pxi[2] = pzp + dq * ezq;
}

__device__ static void
push_part_yz_a_one(int n, particles_cuda_dev_t d_particles, real *d_flds)
{
  struct d_particle p;
  LOAD_PARTICLE(p, d_particles, n);
  real vxi[3];

  // x^n, p^n -> x^(n+0.5), p^n
  
  calc_vxi(vxi, p);
  push_xi_halfdt(&p, vxi);

  STORE_PARTICLE_POS(p, d_particles, n);
}

__device__ static void
push_part_yz_b_one(int n, particles_cuda_dev_t d_particles, real *d_flds)
{
  struct d_particle p;
  LOAD_PARTICLE(p, d_particles, n);
  real h[3], vxi[3];

  // x^n, p^n -> x^(n+0.5), p^n
  
  calc_vxi(vxi, p);
  push_xi_halfdt(&p, vxi);

  real __hh[3*3];
  int l[3];
  find_idx_off(p.xi, l, h, real(-.5));
  find_interp_to_grid_coeff(__hh, h);

  int j[3];
  real __gg[3*3];
  find_idx_off(p.xi, j, h, real(0.));
  find_interp_to_grid_coeff(__gg, h);

  // field interpolation
  
#define INTERPOLATE_FIELD(exq, EX, gg1, gg2, j0, j1, j2)		\
  real exq = 0;								\
  for (int dz = -1; dz <= 1; dz++) {					\
    for (int dy = -1; dy <= 1; dy++) {					\
      exq += ARR_3_off(__##gg1, 1, dy) * ARR_3_off(__##gg2, 2, dz) *	\
	F3(EX, j0[0], j1[1]+dy, j2[2]+dz);				\
    }									\
  } do {} while(0)

  INTERPOLATE_FIELD(exq, EX, gg, gg, j, j, j);
  INTERPOLATE_FIELD(eyq, EY, hh, gg, j, l, j);
  INTERPOLATE_FIELD(ezq, EZ, gg, hh, j, j, l);
  INTERPOLATE_FIELD(hxq, HX, hh, hh, j, l, l);
  INTERPOLATE_FIELD(hyq, HY, gg, hh, j, j, l);
  INTERPOLATE_FIELD(hzq, HZ, hh, gg, j, l, j);

#undef INTERPOLATE_FIELD

  // x^(n+0.5), p^n -> x^(n+0.5), p^(n+1.0) 
  
  push_pxi_dt(&p, exq, eyq, ezq, hxq, hyq, hzq);

  // x^(n+0.5), p^(n+1.0) -> x^(n+1.0), p^(n+1.0) 

  calc_vxi(vxi, p);
  push_xi_halfdt(&p, vxi);

  STORE_PARTICLE_POS(p, d_particles, n);
  STORE_PARTICLE_MOM(p, d_particles, n);
}

__device__ static void
push_part_yz_b2_one(int n, particles_cuda_dev_t d_particles, real *d_flds, int l0[3])
{
  struct d_particle p;
  LOAD_PARTICLE(p, d_particles, n);
  real vxi[3];

  // x^n, p^n -> x^(n+0.5), p^n
  
  calc_vxi(vxi, p);
  push_xi_halfdt(&p, vxi);

  // field interpolation

  int lh[3], lg[3];
  real oh[3], og[3];
  find_idx_off(p.xi, lh, oh, real(-.5));
  find_idx_off(p.xi, lg, og, real(0.));

#define OFF(g, d) o##g[d]
  
#define INTERPOLATE_FIELD(exq, fldnr, g1, g2)				\
  do {									\
    int ddy = l##g1[1]-l0[1], ddz = l##g2[2]-l0[2];			\
    /* printf("C %g [%d,%d,%d]\n", F3C(fldnr, 0, ddy, ddz), 0, ddy, ddz); */ \
    exq =								\
      ip_to_grid_m(OFF(g1, 1)) * ip_to_grid_m(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy-1, ddz-1) +					\
      ip_to_grid_0(OFF(g1, 1)) * ip_to_grid_m(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+0, ddz-1) +					\
      ip_to_grid_p(OFF(g1, 1)) * ip_to_grid_m(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+1, ddz-1) +					\
      ip_to_grid_m(OFF(g1, 1)) * ip_to_grid_0(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy-1, ddz+0) +					\
      ip_to_grid_0(OFF(g1, 1)) * ip_to_grid_0(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+0, ddz+0) +					\
      ip_to_grid_p(OFF(g1, 1)) * ip_to_grid_0(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+1, ddz+0) +					\
      ip_to_grid_m(OFF(g1, 1)) * ip_to_grid_p(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy-1, ddz+1) +					\
      ip_to_grid_0(OFF(g1, 1)) * ip_to_grid_p(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+0, ddz+1) +					\
      ip_to_grid_p(OFF(g1, 1)) * ip_to_grid_p(OFF(g2, 2)) *		\
      F3C(fldnr, 0, ddy+1, ddz+1);					\
  } while(0)

  real exq, eyq, ezq, hxq, hyq, hzq;
  INTERPOLATE_FIELD(exq, EX, g, g);
  INTERPOLATE_FIELD(eyq, EY, h, g);
  INTERPOLATE_FIELD(ezq, EZ, g, h);
  INTERPOLATE_FIELD(hxq, HX, h, h);
  INTERPOLATE_FIELD(hyq, HY, g, h);
  INTERPOLATE_FIELD(hzq, HZ, h, g);

#undef INTERPOLATE_FIELD

  // x^(n+0.5), p^n -> x^(n+0.5), p^(n+1.0) 
  
  push_pxi_dt(&p, exq, eyq, ezq, hxq, hyq, hzq);

  // x^(n+0.5), p^(n+1.0) -> x^(n+1.0), p^(n+1.0) 

  calc_vxi(vxi, p);
  push_xi_halfdt(&p, vxi);

  STORE_PARTICLE_POS(p, d_particles, n);
  STORE_PARTICLE_MOM(p, d_particles, n);
}

__global__ static void
push_part_yz_a(int n_part, particles_cuda_dev_t d_part, float *d_flds, int stride)
{
  int n = threadIdx.x + blockDim.x * blockIdx.x;

  while (n < n_part) {
    push_part_yz_a_one(n, d_part, d_flds);
    n += stride;
  }
}

__global__ static void
push_part_yz_b(int n_part, particles_cuda_dev_t d_part, float *d_flds, int stride)
{
  int n = threadIdx.x + blockDim.x * blockIdx.x;

  while (n < n_part) {
    push_part_yz_b_one(n, d_part, d_flds);
    n += stride;
  }
}

__global__ static void
push_part_yz_b2(int n_particles, particles_cuda_dev_t d_part, real *d_flds)
{
  int tid = threadIdx.x, bid = blockIdx.x;
  int block_begin = d_part.offsets[bid];
  int block_end   = d_part.offsets[bid+1]; // FIXME for blocksize != 1
  int ci[3];

  // cache fields

  blockIdx_to_blockCrd(blockIdx.x, ci);
  ci[0] *= BLOCKSIZE_X;
  ci[1] *= BLOCKSIZE_Y;
  ci[2] *= BLOCKSIZE_Z;

  cache_fields(d_flds, ci);
  
  for (int n = block_begin + tid; n < block_end; n += THREADS_PER_BLOCK) {
    /* printf("bid %d tid %d %d:%d ci %d,%d,%d\n", */
    /* 	   bid, tid, block_begin, block_end, ci[0], ci[1], ci[2]); */
    push_part_yz_b2_one(n, d_part, d_flds, ci); // FIXME d_flds?
  }
}

EXTERN_C void
cuda_push_part_yz_a()
{
  particles_cuda_t pp;
  particles_cuda_get(&pp);
  fields_cuda_t pf;
  fields_cuda_get(&pf, EX, HZ + 1);

  static int pr;
  if (!pr) {
    pr = prof_register("cuda_part_yz_a", 1., 0, ppsc->pp.n_part * 12 * sizeof(float));
  }
  prof_start(pr);

  const int threadsPerBlock = 128;
  const int gridSize = 256;
  int dimBlock[2]  = { threadsPerBlock, 1 };
  int dimGrid[2] = { gridSize, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     push_part_yz_a, (ppsc->pp.n_part, pp.d_part, pf.d_flds,
			      gridSize * threadsPerBlock));

  prof_stop(pr);

  particles_cuda_put(&pp);
  fields_cuda_put(&pf, 0, 0);
}

EXTERN_C void
cuda_push_part_yz_b()
{
  particles_cuda_t pp;
  particles_cuda_get(&pp);
  fields_cuda_t pf;
  fields_cuda_get(&pf, EX, HZ + 1);

  static int pr;
  if (!pr) {
    pr = prof_register("cuda_part_yz_b", 1., 0, ppsc->pp.n_part * 16 * sizeof(float));
  }
  prof_start(pr);

  const int threadsPerBlock = 128;
  const int gridSize = 256;
  int dimBlock[2]  = { threadsPerBlock, 1 };
  int dimGrid[2] = { gridSize, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     push_part_yz_b, (ppsc->pp.n_part, pp.d_part, pf.d_flds,
			      gridSize * threadsPerBlock));

  prof_stop(pr);

  particles_cuda_put(&pp);
  fields_cuda_put(&pf, 0, 0);
}

EXTERN_C void
cuda_push_part_yz_b2()
{
  particles_cuda_t pp;
  particles_cuda_get(&pp);
  fields_cuda_t pf;
  fields_cuda_get(&pf, EX, HZ + 1);

  static int pr;
  if (!pr) {
    pr = prof_register("cuda_part_yz_b", 1., 0, ppsc->pp.n_part * 16 * sizeof(float));
  }
  prof_start(pr);

  int dimBlock[2] = { THREADS_PER_BLOCK, 1 };
  int dimGrid[2]  = { pp.nr_blocks, 1 };
  RUN_KERNEL(dimGrid, dimBlock,
	     push_part_yz_b2, (ppsc->pp.n_part, pp.d_part, pf.d_flds));

  prof_stop(pr);

  particles_cuda_put(&pp);
  fields_cuda_put(&pf, 0, 0);
}

EXTERN_C void
__particles_cuda_get(particles_cuda_t *pp)
{
  int n_part = ppsc->pp.n_part;
  particles_cuda_dev_t *h_part = &pp->h_part;
  particles_cuda_dev_t *d_part = &pp->d_part;

  check(cudaMalloc((void **) &d_part->xi4, n_part * sizeof(float4)));
  check(cudaMalloc((void **) &d_part->pxi4, n_part * sizeof(float4)));
  check(cudaMalloc((void **) &d_part->offsets, 
		   (pp->nr_blocks + 1) * sizeof(int)));

  check(cudaMemcpy(d_part->xi4, h_part->xi4, n_part * sizeof(float4),
		   cudaMemcpyHostToDevice));
  check(cudaMemcpy(d_part->pxi4, h_part->pxi4, n_part * sizeof(float4),
		   cudaMemcpyHostToDevice));
  check(cudaMemcpy(d_part->offsets, h_part->offsets,
		   (pp->nr_blocks + 1) * sizeof(int), cudaMemcpyHostToDevice));

  set_constants(pp);
}

EXTERN_C void
__particles_cuda_put(particles_cuda_t *pp)
{
  int n_part = ppsc->pp.n_part;
  particles_cuda_dev_t *h_part = &pp->h_part;
  particles_cuda_dev_t *d_part = &pp->d_part;

  check(cudaMemcpy(h_part->xi4, d_part->xi4, n_part * sizeof(float4),
		   cudaMemcpyDeviceToHost));
  check(cudaMemcpy(h_part->pxi4, d_part->pxi4, n_part * sizeof(float4),
		   cudaMemcpyDeviceToHost));
  check(cudaFree(d_part->xi4));
  check(cudaFree(d_part->pxi4));
  check(cudaFree(d_part->offsets));
}

EXTERN_C void
__fields_cuda_get(fields_cuda_t *pf)
{
  assert(!ppsc->domain.use_pml);
  int nr_fields = HZ + 1; // FIXME, repeated

  check(cudaMalloc((void **) &pf->d_flds, nr_fields * ppsc->fld_size * sizeof(float)));
  check(cudaMemcpy(pf->d_flds + EX * ppsc->fld_size,
		   pf->flds + EX * ppsc->fld_size,
		   6 * ppsc->fld_size * sizeof(float),
		   cudaMemcpyHostToDevice));
}

EXTERN_C void
__fields_cuda_put(fields_cuda_t *pf)
{
  check(cudaFree(pf->d_flds));
}

#endif
