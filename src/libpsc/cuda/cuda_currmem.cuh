
#pragma once

// ----------------------------------------------------------------------
// SCurr

// OPT: take i < cell_end condition out of load
// OPT: reduce two at a time
// OPT: try splitting current calc / measuring by itself

// OPT: don't need as many ghost points for current and EM fields (?)

#define NR_CBLOCKS 16
#define CBLOCK_ID (threadIdx.x & (NR_CBLOCKS - 1))
#define CBLOCK_SIZE_Y (BS_Y + N_GHOSTS_L + N_GHOSTS_R)
#define CBLOCK_SIZE_Z (BS_Z + N_GHOSTS_L + N_GHOSTS_R)
#define CBLOCK_SIZE (CBLOCK_SIZE_Y * CBLOCK_SIZE_Z * (NR_CBLOCKS))

template<typename BS>
class SCurr
{
  static const int BS_X = BS::x::value, BS_Y = BS::y::value, BS_Z = BS::z::value;
  static const uint N_GHOSTS_L = 1;
  static const uint N_GHOSTS_R = 2;

public:
  static const int shared_size = 3 * CBLOCK_SIZE;

  float *scurr;
  DFields d_flds;

  __device__ SCurr(float *_scurr, DFields _d_flds) :
    scurr(_scurr), d_flds(_d_flds)
  {
    int i = threadIdx.x;
    while (i < shared_size) {
      scurr[i] = float(0.);
      i += THREADS_PER_BLOCK;
    }
  }

  __device__ void add_to_fld(const int *ci0)
  {
    __syncthreads();				\
    int i = threadIdx.x;
    int stride = CBLOCK_SIZE_Y * CBLOCK_SIZE_Z;
    while (i < stride) {
      int rem = i;
      int jz = rem / CBLOCK_SIZE_Y;
      rem -= jz * CBLOCK_SIZE_Y;
      int jy = rem;
      jz -= N_GHOSTS_L;
      jy -= N_GHOSTS_L;
      for (int m = 0; m < 3; m++) {
	float val = float(0.);
	// FIXME, OPT
	for (int wid = 0; wid < NR_CBLOCKS; wid++) {
	  val += (*this)(wid, jy, jz, m);
	}
	d_flds(JXI+m, 0,jy+ci0[1],jz+ci0[2]) += val;
	i += THREADS_PER_BLOCK;
      }
    }
  }

  __device__ float operator()(int wid, int jy, int jz, int m) const
  {
    uint off = index(jy, jz, m, wid);
    return scurr[off];
  }

  __device__ float& operator()(int wid, int jy, int jz, int m)
  {
    uint off = index(jy, jz, m, wid);
    return scurr[off];
  }

  __device__ void add(int m, int jy, int jz, float val, const int *ci0)
  {
    float *addr = &(*this)(CBLOCK_ID, jy, jz, m);
    atomicAdd(addr, val);
  }

private:
  __device__ uint index(int jy, int jz, int m, int wid)
  {
    uint off = ((((m)
	      * CBLOCK_SIZE_Z + ((jz) + N_GHOSTS_L))
	     * CBLOCK_SIZE_Y + ((jy) + N_GHOSTS_L))
	    * (NR_CBLOCKS) + wid);
    if (off >= shared_size) {
      printf("CUDA_ERROR off %d %d wid %d %d:%d m %d\n", off, shared_size, wid, jy, jz, m);
    }
    return off;
  }
};

// ----------------------------------------------------------------------
// GCurr

template<typename BS>
class GCurr
{
public:
  static const int shared_size = 1;

  float *scurr;
  DFields d_flds;

  __device__ GCurr(float *_scurr, DFields _d_flds) :
    scurr(_scurr), d_flds(_d_flds)
  {
  }

  __device__ void add_to_fld(int *ci0)
  {
  }

  __device__ void add(int m, int jy, int jz, float val, int *ci0)
  {
    float *addr = &d_flds(JXI+m, 0,jy+ci0[1],jz+ci0[2]);
    atomicAdd(addr, val);
  }
};

// ======================================================================
// CurrmemShared

struct CurrmemShared
{
  template<typename BS>
  using Curr = SCurr<BS>;

  template<typename BS>
  using Block = BlockQ<BS>;
};

// ======================================================================
// CurrmemGlobal

struct CurrmemGlobal
{
  template<typename BS>
  using Curr = GCurr<BS>;

  template<typename BS>
  using Block = BlockSimple<BS>;
};

