
// ----------------------------------------------------------------------
// calc_j

__shared__ int block_begin, block_end; // first, last+1 particle in block
__shared__ int ci0[3]; // cell index of lower-left cell in block

__device__ static void
calc_j(int i, particles_cuda_dev_t d_particles,
       real *vxi, real *qni_wni, int *ci, SHAPE_INFO_ARGS)
{
#if DIM == DIM_Z  
  short int shift0z;
  short int shift1z;
#elif DIM == DIM_YZ
  short int shift0y;
  short int shift0z;
  short int shift1y;
  short int shift1z;
#endif
  real h0[3], h1[3];
  if (i < block_end) {
    struct d_particle p;
    LOAD_PARTICLE(p, d_particles, i);
    *qni_wni = p.qni_wni;
    int j[3], k[3];
    find_idx(p.xi, ci, real(0.));

    calc_vxi(vxi, p);

    // x^(n+1.0), p^(n+1.0) -> x^(n+0.5), p^(n+1.0) 
    push_xi(&p, vxi, -.5f * d_dt);
    find_idx_off(p.xi, j, h0, real(0.));
    
    // x^(n+0.5), p^(n+1.0) -> x^(n+1.5), p^(n+1.0) 
    push_xi(&p, vxi, d_dt);
    find_idx_off(p.xi, k, h1, real(0.));

#if DIM == DIM_Z  
    shift0z = j[2] - ci[2];
    shift1z = k[2] - ci[2];
#elif DIM == DIM_YZ
    shift0y = j[1] - ci[1];
    shift0z = j[2] - ci[2];
    shift1y = k[1] - ci[1];
    shift1z = k[2] - ci[2];
#endif
    ci[1] -= ci0[1];
    ci[2] -= ci0[2];
  } else {
    *qni_wni = real(0.);
    vxi[0] = real(0.);
    vxi[1] = real(0.);
    vxi[2] = real(0.);
#if DIM == DIM_Z  
    shift0z = 0;
    shift1z = 0;
    h0[2] = real(0.);
    h1[2] = real(0.);
#elif DIM == DIM_YZ
    shift0y = 0;
    shift0z = 0;
    shift1y = 0;
    shift1z = 0;
    h0[1] = real(0.);
    h1[1] = real(0.);
    h0[2] = real(0.);
    h1[2] = real(0.);
#endif
  }
#if DIM == DIM_Z
  cache_shape_arrays(SHAPE_INFO_PARAMS, h0, h1, shift0z, shift1z);
#elif DIM == DIM_YZ
  cache_shape_arrays(SHAPE_INFO_PARAMS, h0, h1, shift0y, shift0z,
		     shift1y, shift1z);
#endif
}

// ----------------------------------------------------------------------
// add_current_to_scratch

__device__ static void
add_current_to_scratch_z(real *scratch, int jy, int len)
{
  int tid = threadIdx.x;

  //__syncthreads(); // not necessary if len < WARPSIZE (?)
  if (tid < len) {
    int jz = tid - SW;
    scratch(0,jy,jz) += SDATA(0,jz);
  }
  //__syncthreads(); // CHECKME, necessary?
}

__device__ static void
add_current_to_scratch_y(real *scratch, int jz, int len)
{
  int tid = threadIdx.x;

  //  __syncthreads(); // not necessary if BLOCKSIZE < WARPSIZE (?)
  if (tid < len) {
    int jy = tid - SW;
    scratch(0,jy,jz) += SDATA(0,jy);
  }
  //__syncthreads(); // CHECKME, necessary?
}

// ======================================================================

#if DIM == DIM_Z

// ----------------------------------------------------------------------

__device__ static void
z_calc_jxh(real qni_wni, real vxi, SHAPE_INFO_ARGS, int jz)
{
  int tid = threadIdx.x;
  real fnqx = vxi * qni_wni * d_fnqs;

  real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
  real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z);
  real wx = .5f * (s0z + s1z);
    
  SDATA(tid,jz) = fnqx * wx;
}

__device__ static void
z_calc_jyh(real qni_wni, real vyi, SHAPE_INFO_ARGS, int jz)
{
  int tid = threadIdx.x;
  real fnqy = vyi * qni_wni * d_fnqs;

  real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
  real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z);
  real wy = .5f * (s0z + s1z);

  SDATA(tid,jz) = fnqy * wy;
}

__device__ static void
z_calc_jzh(real qni_wni, SHAPE_INFO_ARGS)
{
  int tid = threadIdx.x;
  real fnqz = qni_wni * d_fnqzs;

  real last;
  { int jz = -2;
    if (SI_SHIFT0Z >= 0 && SI_SHIFT1Z >= 0) {
      last = 0;
    } else {
      real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z);
      real wz = s1z - s0z;
      last = -fnqz*wz;
    }
    SDATA(tid,jz) = last;
  }
  for (int jz = -1; jz <= 0; jz++) {
    real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
    real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z);
    real wz = s1z - s0z;
    last -= fnqz*wz;
    SDATA(tid,jz) = last;
  }
  { int jz = 1;
    if (SI_SHIFT0Z <= 0 && SI_SHIFT1Z <= 0) {
      last = 0;
    } else {
      real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z);
      real wz = s1z - s0z;
      last -= fnqz*wz;
    }
    SDATA(tid,jz) = last;
  }
}

// ----------------------------------------------------------------------

EXTERN_C __global__ static void
push_part_p2(int n_particles, particles_cuda_dev_t d_particles, real *d_flds,
	     real *d_scratch, int block_stride, int block_start)
{
  int tid = threadIdx.x, bid = blockIdx.x * block_stride + block_start;
  int cell_begin = d_particles.offsets[bid];
  int cell_end   = d_particles.offsets[bid+1];
  int ci[3];

  blockIdx_to_blockCrd(blockIdx.x, ci);
  ci[0] *= BLOCKSIZE_X;
  ci[1] *= BLOCKSIZE_Y;
  ci[2] *= BLOCKSIZE_Z;

  int nr_loops = (cell_end - cell_begin + THREADS_PER_BLOCK-1) / THREADS_PER_BLOCK;
  for (int l = 0; l < nr_loops; l++) {
    int i = cell_begin + tid + l * THREADS_PER_BLOCK;
    DECLARE_SHAPE_INFO;
    real vxi[3], qni_wni = 0.;
    if (i < cell_end) {
      struct d_particle p;
      LOAD_PARTICLE(p, d_particles, i);
      calc_j(ci, &p, d_particles, i, vxi, SHAPE_INFO_PARAMS, &qni_wni);
    } else {
      SI_SHIFT0Z = 0;
    }

    // ----------------------------------------------------------------------
    // JX

    for (int jz = -SW; jz <= SW; jz++) {
      if (i < cell_end) {
	z_calc_jxh(qni_wni, vxi[0], SHAPE_INFO_PARAMS, jz);
      } else {
	SDATA(tid,jz) = real(0.);
      }
    }
#ifdef CALC_CURRENT
    reduce_sum_sdata(sdata);
    real *scratch = d_scratch + bid * 3 * BLOCKSTRIDE;
    add_current_to_scratch_z(scratch, 0, 5);
#endif

    // ----------------------------------------------------------------------
    // JY

    for (int jz = -SW; jz <= SW; jz++) {
      if (i < cell_end) {
	z_calc_jyh(qni_wni, vxi[1], SHAPE_INFO_PARAMS, jz);
      } else {
	SDATA(tid,jz) = real(0.);
      }
    }
#ifdef CALC_CURRENT
    reduce_sum_sdata(sdata);
    scratch = d_scratch + (bid * 3 + 1) * BLOCKSTRIDE;
    add_current_to_scratch_z(scratch, 0, 5);
#endif

    // ----------------------------------------------------------------------
    // JZ

    if (i < cell_end) {
      z_calc_jzh(qni_wni, SHAPE_INFO_PARAMS);
    } else {
      for (int jz = -SW; jz < SW; jz++) SDATA(tid,jz) = real(0.);
    }
#ifdef CALC_CURRENT
    reduce_sum_sdata4(sdata);
    scratch = d_scratch + (bid * 3 + 2) * BLOCKSTRIDE;
    add_current_to_scratch_z(scratch, 0, 4);
#endif
  }
}

#elif DIM == DIM_YZ

// ----------------------------------------------------------------------

__device__ static void
yz_calc_jxh_z(real qni_wni, real vxi, SHAPE_INFO_ARGS, int jz)
{
  int tid = threadIdx.x;
  real fnqx = vxi * qni_wni * d_fnqs;

  // FIXME, can be simplified
  real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
  real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;

  for (int jy = -SW; jy <= SW; jy++) {
    real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
    real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
    real wx = s0y * s0z
      + real(.5) * s1y * s0z
      + real(.5) * s0y * s1z
      + real(.3333333333) * s1y * s1z;
    
    SDATA(tid,jy) = fnqx * wx;
  }
}

__device__ static void
yz_calc_jyh_z(real qni_wni, SHAPE_INFO_ARGS, int jz)
{
  int tid = threadIdx.x;
  real fnqy = qni_wni * d_fnqys;

  real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
  real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z);
  real tmp1 = real(.5) * (s0z + s1z);

  real last;
  { int jy = -2;
    if (SI_SHIFT0Y >= 0 && SI_SHIFT1Y >= 0) {
      last = 0.f;
    } else {
      real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
      real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
      real wy = s1y * tmp1;
      last = -fnqy*wy;
    }
    SDATA(tid,jy) = last;
  }
  for (int jy = -1; jy <= 0; jy++) {
    real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
    real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
    real wy = s1y * tmp1;
    last -= fnqy*wy;
    SDATA(tid,jy) = last;
  }
  { int jy = 1;
    if (SI_SHIFT0Y <= 0 && SI_SHIFT1Y <= 0) {
      last = 0.f;
    } else {
      real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
      real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
      real wy = s1y * tmp1;
      last -= fnqy*wy;
    }
    SDATA(tid,jy) = last;
  }
}

__device__ static void
yz_calc_jzh_y(real qni_wni, SHAPE_INFO_ARGS, int jy)
{
  int tid = threadIdx.x;
  real fnqz = qni_wni * d_fnqzs;

  real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
  real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y);
  real tmp1 = real(.5) * (s0y + s1y);

  real last;
  { int jz = -2;
    if (SI_SHIFT0Z >= 0 && SI_SHIFT1Z >= 0) {
      last = 0.f;
    } else {
      real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
      real wz = s1z * tmp1;
      last = -fnqz*wz;
    }
    SDATA(tid,jz) = last;
  }
  for (int jz = -1; jz <= 0; jz++) {
    real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
    real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
    real wz = s1z * tmp1;
    last -= fnqz*wz;
    SDATA(tid,jz) = last;
  }
  { int jz = 1;
    if (SI_SHIFT0Z <= 0 && SI_SHIFT1Z <= 0) {
      last = 0.f;
    } else {
      real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
      real wz = s1z * tmp1;
      last -= fnqz*wz;
    }
    SDATA(tid,jz) = last;
  }
}

// ----------------------------------------------------------------------

__shared__ real scurr[(BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Z + 2*SW) * 3];

#define scurr(m,jy,jz) (scurr[(m * (BLOCKSIZE_Z + 2*SW) + (jz)+SW)	\
			      * (BLOCKSIZE_Y + 2*SW) + (jy)+SW])

// ----------------------------------------------------------------------
// current_add

__device__ static void
current_add(int m, int jy, int jz, real val)
{
#if 1
  int tid = threadIdx.x;
  reduce_sum(val);
  if (tid == 0) {
    scurr(m,jy,jz) += sdata1[0];
  }
#else
#if __CUDA_ARCH__ >= 200 // for Fermi, atomicAdd supports floats
  atomicAdd(&scurr(m,jy,jz), val);
#else
  float *addr = &scurr(m,jy,jz);
  while ((val = atomicExch(addr, atomicExch(addr, 0.0f)+val))!=0.0f);
#endif
#endif
}

// ----------------------------------------------------------------------
// yz_calc_jx

__device__ static void
yz_calc_jx(real vxi, real qni_wni, SHAPE_INFO_ARGS)
{
  int tid = threadIdx.x;

  for (int jz = -SW; jz <= SW; jz++) {
    real fnqx = vxi * qni_wni * d_fnqs;
    
    // FIXME, can be simplified
    real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
    real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
    
    for (int jy = -SW; jy <= SW; jy++) {
      real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
      real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
      real wx = s0y * s0z
	+ real(.5) * s1y * s0z
	+ real(.5) * s0y * s1z
	+ real(.3333333333) * s1y * s1z;
      
      current_add(0, jy, jz, fnqx * wx);
    }
  }
}

// ----------------------------------------------------------------------
// yz_calc_jy

__device__ static void
yz_calc_jy(real qni_wni, SHAPE_INFO_ARGS)
{
  int tid = threadIdx.x;
  
  for (int jz = -SW; jz <= SW; jz++) {
    real fnqy = qni_wni * d_fnqys;
    
    real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
    real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z);
    real tmp1 = real(.5) * (s0z + s1z);
    
    real last;
    { int jy = -2;
      if (SI_SHIFT0Y >= 0 && SI_SHIFT1Y >= 0) {
	last = 0.f;
      } else {
	real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
	real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
	real wy = s1y * tmp1;
	last = -fnqy*wy;
      }
      current_add(1, jy, jz, last);
    }
    for (int jy = -1; jy <= 0; jy++) {
      real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
      real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
      real wy = s1y * tmp1;
      last -= fnqy*wy;
      current_add(1, jy, jz, last);
    }
    { int jy = 1;
      if (SI_SHIFT0Y <= 0 && SI_SHIFT1Y <= 0) {
	last = 0.f;
      } else {
	real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
	real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y) - s0y;
	real wy = s1y * tmp1;
	last -= fnqy*wy;
      }
      current_add(1, jy, jz, last);
    }
  }
}

// ----------------------------------------------------------------------
// yz_calc_jz

__device__ static void
yz_calc_jz(real qni_wni, SHAPE_INFO_ARGS)
{
  int tid = threadIdx.x;
  
  for (int jy = -SW; jy <= SW; jy++) {
    real fnqz = qni_wni * d_fnqzs;
    
    real s0y = pick_shape_coeff_(0, y, jy, SI_SHIFT0Y);
    real s1y = pick_shape_coeff_(1, y, jy, SI_SHIFT1Y);
    real tmp1 = real(.5) * (s0y + s1y);
    
    real last;
    { int jz = -2;
      if (SI_SHIFT0Z >= 0 && SI_SHIFT1Z >= 0) {
	last = 0.f;
      } else {
	real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
	real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
	real wz = s1z * tmp1;
	last = -fnqz*wz;
      }
      current_add(2, jy, jz, last);
    }
    for (int jz = -1; jz <= 0; jz++) {
      real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
      real wz = s1z * tmp1;
      last -= fnqz*wz;
      current_add(2, jy, jz, last);
    }
    { int jz = 1;
      if (SI_SHIFT0Z <= 0 && SI_SHIFT1Z <= 0) {
	last = 0.f;
      } else {
	real s0z = pick_shape_coeff_(0, z, jz, SI_SHIFT0Z);
	real s1z = pick_shape_coeff_(1, z, jz, SI_SHIFT1Z) - s0z;
	real wz = s1z * tmp1;
	last -= fnqz*wz;
      }
      current_add(2, jy, jz, last);
    }
  }
}

__global__ static void
push_part_p2(int n_particles, particles_cuda_dev_t d_particles, real *d_flds,
	     real *d_scratch, int block_stride, int block_start)
{
  int tid = threadIdx.x, bid = blockIdx.x * block_stride + block_start;

  int stride = (BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Z + 2*SW);
  // stride must be <= THREADS_PER_BLOCK!
  if (tid < stride) {
    for (int m = 0; m < 3; m++) {
      scurr[m * stride + tid] = real(0.);
    }
  }

  __shared__ int imax;
  if (tid == 0) {
    block_begin = d_particles.offsets[bid];
    block_end   = d_particles.offsets[bid+1];
    int nr_loops = (block_end - block_begin + THREADS_PER_BLOCK-1) / THREADS_PER_BLOCK;
    imax = block_begin + nr_loops * THREADS_PER_BLOCK;

    blockIdx_to_blockCrd(bid, ci0);
    ci0[0] *= BLOCKSIZE_X;
    ci0[1] *= BLOCKSIZE_Y;
    ci0[2] *= BLOCKSIZE_Z;
  }
  __syncthreads();

  for (int i = block_begin + tid; i < imax; i += THREADS_PER_BLOCK) {
    DECLARE_SHAPE_INFO;
    real vxi[3], qni_wni;
    int ci[3]; // index of this particle's cell relative to ci0
    calc_j(i, d_particles, vxi, &qni_wni, ci, SHAPE_INFO_PARAMS);
    yz_calc_jx(vxi[0], qni_wni, SHAPE_INFO_PARAMS);
    yz_calc_jy(qni_wni, SHAPE_INFO_PARAMS);
    yz_calc_jz(qni_wni, SHAPE_INFO_PARAMS);
  }

  __syncthreads();
  if (tid < stride) {
    for (int m = 0; m < 3; m++) {
      real *scratch = d_scratch + (bid * 3 + m) * BLOCKSTRIDE;
      int jz = tid / (BLOCKSIZE_Y + 2*SW) - SW;
      int jy = tid % (BLOCKSIZE_Y + 2*SW) - SW;
      scratch(0,jy,jz) += scurr[m*stride + tid];
    }
  }
}

#endif

// ----------------------------------------------------------------------
// collect_currents

__global__ static void
collect_currents(real *d_flds, real *d_scratch, int nr_blocks)
{
#if DIM == DIM_Z
  int jy = 0;
  int jz = threadIdx.x - SW;
#elif DIM == DIM_YZ
  int jy = threadIdx.x - SW;
  int jz = threadIdx.y - SW;
#endif

  for (int b = 0; b < nr_blocks; b++) {
    real *scratch = d_scratch + b * 3 * BLOCKSTRIDE;

    int ci[3];
    blockIdx_to_blockCrd(b, ci);
    ci[0] *= BLOCKSIZE_X;
    ci[1] *= BLOCKSIZE_Y;
    ci[2] *= BLOCKSIZE_Z;
    ci[0] += d_ilo[0];
    ci[1] += d_ilo[1];
    ci[2] += d_ilo[2];

    for (int m = 0; m < 3; m++) {
      F3_DEV(JXI+m, 0+ci[0],jy+ci[1],jz+ci[2]) += scratch(m,jy,jz);
    }
  }
}

