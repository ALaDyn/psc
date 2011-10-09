
// FIXME -> common.c

__device__ static void
find_idx_off_pos_1st(const real xi[3], int j[3], real h[3], real pos[3], real shift)
{
  int d;
  for (d = 0; d < 3; d++) {
    pos[d] = xi[d] * d_dxi[d] + shift;
    j[d] = cuda_fint(pos[d]);
    h[d] = pos[d] - j[d];
  }
}


__shared__ volatile bool do_read;
__shared__ volatile bool do_write;
__shared__ volatile bool do_reduce;
__shared__ volatile bool do_calc_jx;
__shared__ volatile bool do_calc_jy;
__shared__ volatile bool do_calc_jz;

// OPT: ci1 8 bit loads could be forced -> 32bit (e.g. ushort2)
// OPT: take i < cell_end condition out of load
// OPT: reduce two at a time
// OPT: try splitting current calc / measuring by itself
// OPT: get rid of block_stride

#define WARPS_PER_BLOCK (THREADS_PER_BLOCK / 32)

__shared__ int _cell_end[WARPS_PER_BLOCK]; // last particle in current cell valid in p2x
__shared__ int _i_end[WARPS_PER_BLOCK]; // end for loop counter
__shared__ uchar4 _ci1[WARPS_PER_BLOCK]; // relative offset of current cell in block
__shared__ int _ci1_off[WARPS_PER_BLOCK]; // offset into scurr, includes ci1 and SW

#define xBLOCKSTRIDE ((((BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Z + 2*SW) + 31) / 32) * 32)
__shared__ real _scurr[WARPS_PER_BLOCK * xBLOCKSTRIDE];

#define w_scurr_(wid, jy, jz) (_scurr[((jz)+SW) * (BLOCKSIZE_Y + 2*SW) + (jy)+SW	\
				      + (wid) * xBLOCKSTRIDE])

#define w_scurr(jy, jz) w_scurr_(threadIdx.x >> 5, jy, jz)

#define w_scurr_ci1(jy, jz) (_scurr[(jz) * (BLOCKSIZE_Y + 2*SW) + (jy) + w_ci1_off])


#define w_cell_end (_cell_end[threadIdx.x >> 5])
#define w_i_end (_i_end[threadIdx.x >> 5])
#define w_ci1 (_ci1[threadIdx.x >> 5])
#define w_ci1_off (_ci1_off[threadIdx.x >> 5])

__device__ static real
find_shape_coeff_d_shift_1st(int j, real h, short int shift)
{
  if (j ==  0 + shift) {
    return real(1.) - h;
  } else if (j == +1 + shift) {
    return h;
  } else {
    return real(0.);
  }
}

#if CACHE_SHAPE_ARRAYS == 7

struct shapeinfo_h {
  real xm[2], xp[2];
  real hy[2], hz[2];
};

struct shapeinfo_i {
  char4 shiftyz; // x: y[0] y: y[1] z: z[0] w: z[1]
};

#define DECLARE_SHAPE_INFO			\
  struct shapeinfo_h _si_h, *si_h = &_si_h;	\
  struct shapeinfo_i _si_i, *si_i = &_si_i

#define SHAPE_INFO_PARAMS si_h, si_i
#define SHAPE_INFO_ARGS struct shapeinfo_h *si_h, struct shapeinfo_i *si_i

#define D_SHAPEINFO_PARAMS d_si_h, d_si_i
#define D_SHAPEINFO_ARGS struct shapeinfo_h *d_si_h, struct shapeinfo_i *d_si_i

#define SI_SHIFT0Y si_i->shiftyz.x
#define SI_SHIFT1Y si_i->shiftyz.y
#define SI_SHIFT10Y (si_i->shiftyz.y - si_i->shiftyz.x)
#define SI_SHIFT0Z si_i->shiftyz.z
#define SI_SHIFT1Z si_i->shiftyz.w
#define SI_SHIFT10Z (si_i->shiftyz.w - si_i->shiftyz.z)

__device__ static void
cache_shape_arrays(SHAPE_INFO_ARGS, real *h0, real *h1,
		   real *xm, real *xp,
		   short int shift0y, short int shift0z,
		   short int shift1y, short int shift1z)
{
  si_h->xm[0] = xm[1];
  si_h->xm[1] = xm[2];
  si_h->xp[0] = xp[1];
  si_h->xp[1] = xp[2];
  si_h->hy[0] = h0[1];
  si_h->hy[1] = h1[1];
  si_h->hz[0] = h0[2];
  si_h->hz[1] = h1[2];
  SI_SHIFT0Y = shift0y;
  SI_SHIFT1Y = shift1y;
  SI_SHIFT0Z = shift0z;
  SI_SHIFT1Z = shift1z;
}

#define pick_shape_coeff(t, comp, j, shift) ({				\
      const int __y __attribute__((unused)) = 1;			\
      const int __z __attribute__((unused)) = 2;			\
      __pick_shape_coeff(j, shift, __ ## comp, si_h->h ## comp[t]);		\
  })


__device__ static real
__pick_shape_coeff(int j, int shift, int d, real h)
{
  real s;
  if (j == shift + 0) {
    s = find_shape_coeff_d_shift_1st( 0, h, 0);
  } else if (j == shift + 1) {
    s = find_shape_coeff_d_shift_1st(+1, h, 0);
  } else {
    s = real(0.);
  }
  return s;
}

// ======================================================================
#else
#error
#endif

// based on p2_noshift_4.c

#if DIM != DIM_YZ
#error TBD
#endif

__shared__ int ci0[3]; // cell index of lower-left cell in block

// ======================================================================

// ----------------------------------------------------------------------
// calc_shape_info

__device__ static void
calc_shape_info(int i, particles_cuda_dev_t d_particles,
		real *vxi, real *qni_wni, SHAPE_INFO_ARGS)
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
  real xm[3], xp[3];
  if (i < w_cell_end) {
    struct d_particle p;
    LOAD_PARTICLE(p, d_particles, i);
    *qni_wni = p.qni_wni;

    int j[3], k[3];
    calc_vxi(vxi, p);

    // x^(n+1.0), p^(n+1.0) -> x^(n+0.5), p^(n+1.0) 
    push_xi(&p, vxi, -.5f * d_dt);
    find_idx_off_pos_1st(p.xi, j, h0, xm, real(0.));
    
    // x^(n+0.5), p^(n+1.0) -> x^(n+1.5), p^(n+1.0) 
    push_xi(&p, vxi, d_dt);
    find_idx_off_pos_1st(p.xi, k, h1, xp, real(0.));

    shift0y = j[1] - (ci0[1] + w_ci1.y);
    shift0z = j[2] - (ci0[2] + w_ci1.z);
    shift1y = k[1] - (ci0[1] + w_ci1.y);
    shift1z = k[2] - (ci0[2] + w_ci1.z);
  } else {
    *qni_wni = real(0.);
    vxi[0] = real(0.);
    vxi[1] = real(0.);
    vxi[2] = real(0.);
    shift0y = 0;
    shift0z = 0;
    shift1y = 0;
    shift1z = 0;
    h0[1] = real(0.);
    h1[1] = real(0.);
    h0[2] = real(0.);
    h1[2] = real(0.);
    xm[1] = real(0.);
    xm[2] = real(0.);
    xp[1] = real(0.);
    xp[2] = real(0.);
  }
  cache_shape_arrays(SHAPE_INFO_PARAMS, h0, h1, xm, xp, shift0y, shift0z,
		     shift1y, shift1z);
}

// ======================================================================

#if DIM == DIM_YZ

// ----------------------------------------------------------------------
// current_add1

__device__ static void
current_add1(int jy, int jz, real val)
{
  int lid = threadIdx.x & 31;
  if (!do_write) {
    if (w_ci1_off < 0)
      w_scurr_ci1(jy, jz) += val;
  } else if (do_reduce) {
#if 0    
    val = reduce_sum_warp(val);
    if (lid == 0) {
      w_scurr_ci1(jy, jz) += val;
    }
#else
    for (int i = 0; i < 32; i++) {
      if (lid == i) {
    	w_scurr(jy, jz) += val;
      }
    }
#endif
  } else {
    w_scurr_ci1(jy, jz) += val;
  }
}

// ----------------------------------------------------------------------
// yz_calc_jx

__device__ static inline void
calc_jx_one(int jy, int jz, real fnqx, SHAPE_INFO_ARGS,
	    real s0z, real s1z)
{
  real s0y = pick_shape_coeff(0, y, jy, SI_SHIFT0Y);
  real s1y = pick_shape_coeff(1, y, jy, SI_SHIFT1Y) - s0y;
  real wx = s0y * s0z
    + real(.5) * s1y * s0z
    + real(.5) * s0y * s1z
    + real(.3333333333) * s1y * s1z;
  
  current_add1(jy + w_ci1.y, jz + w_ci1.z, fnqx * wx);
}

__device__ static void
yz_calc_jx(real vxi, real qni_wni, SHAPE_INFO_ARGS)
{
  real fnqx = vxi * qni_wni * d_fnqs;
  int jzl = (0 && SI_SHIFT0Z >= 0 && SI_SHIFT1Z >= 0) ?  0 : -1;
  int jzh = (0 && SI_SHIFT0Z <= 0 && SI_SHIFT1Z <= 0) ?  1 :  2;
  for (int jz = jzl; jz <= jzh; jz++) {
    
    // FIXME, can be simplified
    real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
    real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z) - s0z;
    
    for (int jy = -1; jy <= 1; jy++) {
      calc_jx_one(jy, jz, fnqx, SHAPE_INFO_PARAMS, s0z, s1z);
    }
    if (1 || SI_SHIFT0Y > 0 || SI_SHIFT1Y > 0) {
      calc_jx_one( 2, jz, fnqx, SHAPE_INFO_PARAMS, s0z, s1z);
    }
  }
}

// ----------------------------------------------------------------------
// yz_calc_jy

__device__ static void
calc_dx1(real dx1[2], real x[2], real dx[2], int off[2])
{
  if (off[1] == 0) {
    dx1[0] = .5 * off[0] - x[0];
    dx1[1] = dx[1] / dx[0] * dx1[0];
  } else {
    dx1[1] = .5 * off[1] - x[1];
    dx1[0] = dx[0] / dx[1] * dx1[1];
  }
}

__device__ static void
curr_2d_vb_cell(int i[2], real x[2], real dx[2], real qni_wni,
		real dxt[2], int off[2])
{
  real fnqy = qni_wni * d_fnqys;
  current_add1(i[0],i[1]  , fnqy * dx[0] * (.5f - x[1] - .5f * dx[1]));
  current_add1(i[0],i[1]+1, fnqy * dx[0] * (.5f + x[1] + .5f * dx[1]));
  //  F3(JZI, 0,i[0],i[1]  ) += fnq[1] * dx[1] * (.5f - x[0] - .5f * dx[0]);
  //  F3(JZI, 0,i[0]+1,i[1]) += fnq[1] * dx[1] * (.5f + x[0] + .5f * dx[0]);
  if (dxt) {
    dxt[0] -= dx[0];
    dxt[1] -= dx[1];
    x[0] += dx[0] - off[0];
    x[1] += dx[1] - off[1];
    i[0] += off[0];
    i[1] += off[1];
  }
}

__device__ static void
yz_calc_jy(real qni_wni, SHAPE_INFO_ARGS)
{
  int i[2] = { SI_SHIFT0Y + w_ci1.y + ci0[1], SI_SHIFT0Z + w_ci1.z + ci0[2] };
  int idiff[2] = { SI_SHIFT1Y - SI_SHIFT0Y, SI_SHIFT1Z - SI_SHIFT0Z };
  real *xp = si_h->xp, *xm = si_h->xm;
  real dx[2] = { xp[0] - xm[0], xp[1] - xm[1] };
  //  xm[0] = 2.5; xm[1] = 3.5;
  //  i[0] = 2; i[1] = 3;
  real x[2] = { xm[0] - (i[0] + real(.5)), xm[1] - (i[1] + real(.5)) };

  //  dx[0] = -0.01753; dx[1] = 0;
  //  x[0] = 0; x[1] = 0;
  i[0] -= ci0[1]; i[1] -= ci0[2];

  real dx1[2];
  int off[2];
  int first_dir, second_dir = -1;
  // FIXME, make sure we never div-by-zero?
  if (idiff[0] == 0 && idiff[1] == 0) {
    first_dir = -1;
  } else if (idiff[0] == 0) {
    first_dir = 1;
  } else if (idiff[1] == 0) {
    first_dir = 0;
  } else {
    dx1[0] = .5 * idiff[0] - x[0];
    dx1[1] = dx[1] / dx[0] * dx1[0];
    if (fabsf(x[1] + dx1[1]) > .5f) {
      first_dir = 1;
    } else {
      first_dir = 0;
    }
    second_dir = 1 - first_dir;
  }

  if (first_dir >= 0) {
    off[1-first_dir] = 0;
    off[first_dir] = idiff[first_dir];
    calc_dx1(dx1, x, dx, off);
    curr_2d_vb_cell(i, x, dx1, qni_wni, dx, off);
  }

  if (second_dir >= 0) {
    off[first_dir] = 0;
    off[second_dir] = idiff[second_dir];
    calc_dx1(dx1, x, dx, off);
    curr_2d_vb_cell(i, x, dx1, qni_wni, dx, off);
  }
    
  curr_2d_vb_cell(i, x, dx, qni_wni, NULL, NULL);
}

// ----------------------------------------------------------------------
// yz_calc_jz

__device__ static void
yz_calc_jz(real qni_wni, SHAPE_INFO_ARGS)
{
  for (int jy = -1; jy <= 2; jy++) {
    real fnqz = qni_wni * d_fnqzs;
    
    real s0y = pick_shape_coeff(0, y, jy, SI_SHIFT0Y);
    real s1y = pick_shape_coeff(1, y, jy, SI_SHIFT1Y);
    real tmp1 = real(.5) * (s0y + s1y);
    
    real last  = 0.f;
    for (int jz = -1; jz <= 0; jz++) {
      real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
      real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z) - s0z;
      real wz = s1z * tmp1;
      last -= fnqz*wz;
      current_add1(jy, jz, last);
    }
    { int jz = 1;
      if (SI_SHIFT0Z <= 0 && SI_SHIFT1Z <= 0) {
	last = 0.f;
      } else {
	real s0z = pick_shape_coeff(0, z, jz, SI_SHIFT0Z);
	real s1z = pick_shape_coeff(1, z, jz, SI_SHIFT1Z) - s0z;
	real wz = s1z * tmp1;
	last -= fnqz*wz;
      }
      current_add1(jy, jz, last);
    }
  }
}

__device__ static void
zero_scurr1()
{
  int i = threadIdx.x;
  int N = xBLOCKSTRIDE * WARPS_PER_BLOCK;
  while (i < N) {
    _scurr[i] = real(0.);
    i += THREADS_PER_BLOCK;
  }
}

__device__ static void
add_scurr_to_flds(real *d_flds)
{
  int i = threadIdx.x;
  int stride = (BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Z + 2*SW) * 3;
  while (i < stride) {
    int rem = i;
    int m = rem / ((BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Y + 2*SW));
    rem -= m * ((BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Y + 2*SW));
    int jz = rem / (BLOCKSIZE_Y + 2*SW);
    rem -= jz * (BLOCKSIZE_Y + 2*SW);
    int jy = rem;
    jz -= SW;
    jy -= SW;
    real val = w_scurr_(0, jy, jz);
    F3_DEV(JXI+m, 0,jy+ci0[1],jz+ci0[2]) += val;
    i += THREADS_PER_BLOCK;
  }
}

__device__ static void
add_scurr_to_flds1(real *d_flds, int m)
{
  int i = threadIdx.x;
  int stride = (BLOCKSIZE_Y + 2*SW) * (BLOCKSIZE_Z + 2*SW);
  while (i < stride) {
    int rem = i;
    int jz = rem / (BLOCKSIZE_Y + 2*SW);
    rem -= jz * (BLOCKSIZE_Y + 2*SW);
    int jy = rem;
    jz -= SW;
    jy -= SW;
    real val = real(0.);
    for (int wid = 0; wid < WARPS_PER_BLOCK; wid++) {
      val += w_scurr_(wid, jy, jz);
    }
    F3_DEV(JXI+m, 0,jy+ci0[1],jz+ci0[2]) += val;
    i += THREADS_PER_BLOCK;
  }
}

__device__ static void
shapeinfo_zero(SHAPE_INFO_ARGS)
{
  si_h->hy[0] = real(0.);
  si_h->hy[1] = real(0.);
  si_h->hz[0] = real(0.);
  si_h->hz[1] = real(0.);
  si_h->xm[0] = real(0.);
  si_h->xm[1] = real(0.);
  si_h->xp[0] = real(0.);
  si_h->xp[1] = real(0.);
  SI_SHIFT0Y = 0;
  SI_SHIFT1Y = 0;
  SI_SHIFT0Z = 0;
  SI_SHIFT1Z = 0;
}

#define push_part_p2_1							\
  do_read = true;							\
  do_reduce = true;							\
  do_write = true;							\
  do_calc_jx = true;							\
  do_calc_jy = true;							\
  do_calc_jz = true;							\
									\
  int tid = threadIdx.x;						\
  const int cells_per_block = BLOCKSIZE_Y * BLOCKSIZE_Z;		\
									\
  if (do_write) {							\
    zero_scurr1();							\
  }									\
									\
  __shared__ int bid;							\
  if (tid == 0) {							\
    bid = blockIdx.x * block_stride + block_start;			\
    blockIdx_to_cellPos(&d_particles, bid, ci0);			\
  }									\
  __syncthreads();							\
									\
  for (int cid = bid * cells_per_block + (tid >> 5);			\
       cid < (bid + 1) * cells_per_block; cid += WARPS_PER_BLOCK)

#define push_part_p2_2							\
  int ci1[3];								\
  cellIdx_to_cellCrd_rel(cid, ci1);					\
  int cell_begin = d_particles.c_offsets[cid];				\
  if (1||(tid & 31) == 0) {						\
    w_ci1.y = ci1[1];							\
    w_ci1.z = ci1[2];							\
    w_ci1_off = (ci1[2] + SW) * (BLOCKSIZE_Y + 2*SW) + (ci1[1] + SW)	\
      + (threadIdx.x >> 5) * xBLOCKSTRIDE;				\
    w_cell_end = d_particles.c_offsets[cid+1];				\
    int nr_loops = (w_cell_end - cell_begin + 32-1) / 32;		\
    w_i_end = cell_begin + nr_loops * 32;				\
  }									\
  									\
  for (int i = cell_begin + (tid&31); i < w_i_end + (tid&31); i += 32)

#define push_part_p2_3(m)			\
  if (do_write) {				\
    __syncthreads();				\
    add_scurr_to_flds1(d_flds, m);		\
  }

// ======================================================================

__global__ static void
push_part_p2x(int n_particles, particles_cuda_dev_t d_particles, real *d_flds,
	      int block_stride, int block_start)
{
  push_part_p2_1 {
    push_part_p2_2 {
      DECLARE_SHAPE_INFO;
      real vxi[3];
      real qni_wni;
      if (do_read && i < w_cell_end) {
	calc_shape_info(i, d_particles, vxi, &qni_wni, SHAPE_INFO_PARAMS);
      } else {
	shapeinfo_zero(SHAPE_INFO_PARAMS);
	vxi[0] = 0.f;
        qni_wni = 0.;
      }
      if (do_calc_jx) {
	yz_calc_jx(vxi[0], qni_wni, SHAPE_INFO_PARAMS);
      }
    }
  }
  push_part_p2_3(0);
}

// ======================================================================

__global__ static void
push_part_p2y(int n_particles, particles_cuda_dev_t d_particles, real *d_flds,
	      int block_stride, int block_start)
{
  push_part_p2_1 {
    push_part_p2_2 {
      DECLARE_SHAPE_INFO;
      real vxi[3];
      real qni_wni;
      if (do_read && i < w_cell_end) {
	calc_shape_info(i, d_particles, vxi, &qni_wni, SHAPE_INFO_PARAMS);
      } else {
	shapeinfo_zero(SHAPE_INFO_PARAMS);
        qni_wni = 0.;
      }
      if (do_calc_jy) {
	yz_calc_jy(qni_wni, SHAPE_INFO_PARAMS);
      }
    }
  }
  push_part_p2_3(1);
}

// ======================================================================

__global__ static void
push_part_p2z(int n_particles, particles_cuda_dev_t d_particles, real *d_flds,
	      int block_stride, int block_start)
{
}

#endif

