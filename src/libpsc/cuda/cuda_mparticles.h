
#ifndef CUDA_MPARTICLES_H
#define CUDA_MPARTICLES_H

#include "cuda_iface.h"

#include "grid.hxx"
#include "particles.hxx"
#include "psc_bits.h"
#include "cuda_bits.h"

#include <thrust/device_vector.h>

// ======================================================================
// cuda_mparticles_base

struct cuda_mparticles_base
{
  using particle_t = particle_cuda_t;
  using real_t = particle_t::real_t;
  using Real3 = Vec3<real_t>;

  cuda_mparticles_base(const Grid_t& grid, const Int3& bs);
  // copy constructor would work fine, be don't want to copy everything
  // by accident
  cuda_mparticles_base(const cuda_mparticles&) = delete;

  void reserve_all();
  void resize_all(const uint *n_prts_by_patch);
  void get_size_all(uint *n_prts_by_patch);
  void set_particles(uint n_prts, uint off,
		     void (*get_particle)(cuda_mparticles_prt *prt, int n, void *ctx),
		     void *ctx);

  template<typename F>
  void set_particles(uint p, F getter);

  void get_particles(uint n_prts, uint off,
		     void (*put_particle)(cuda_mparticles_prt *, int, void *),
		     void *ctx);

  template<typename F>
  void get_particles(uint p, F setter);
  
  // protected:
  void to_device(float4 *xi4, float4 *pxi4, uint n_prts, uint off);
  void from_device(float4 *xi4, float4 *pxi4, uint n_prts, uint off);
  
  // per particle
  thrust::device_vector<float4> d_xi4;
  thrust::device_vector<float4> d_pxi4;

  // per block
  thrust::device_vector<uint> d_off;     // particles per block
                                         // are at indices [offsets[block] .. offsets[block+1]-1[

  uint n_prts = {};                      // total # of particles across all patches
  uint n_alloced = {};                   // size of particle-related arrays as allocated
  Int3 b_mx;                             // number of blocks per direction in each patch
  Int3 bs;                               // number of blocks per patch
  uint n_blocks_per_patch;               // number of blocks per patch
  uint n_blocks;                         // number of blocks in all patches in mprts
  Real3 b_dxi;                           // inverse of block size (in actual length units)

  const uint n_patches;
  const Grid_t& grid_;
};

// ======================================================================
// bnd

#define CUDA_BND_S_NEW (9)
#define CUDA_BND_S_OOB (10)
#define CUDA_BND_STRIDE (10)

// ----------------------------------------------------------------------
// cuda_bnd

struct cuda_bnd {
  psc_particle_cuda_buf_t buf;
  int n_recv;
  int n_send;
};

// ----------------------------------------------------------------------
// cuda_mparticles_bnd

struct cuda_mparticles;

struct cuda_mparticles_bnd
{
  ~cuda_mparticles_bnd();

  void setup(cuda_mparticles *cmprts);
  void reserve_all(cuda_mparticles *cmprts);

  void scan_send_buf_total(cuda_mparticles *cmprts);
  void scan_send_buf_total_gold(cuda_mparticles *cmprts);
  void spine_reduce(cuda_mparticles *cmprts);
  void sort_pairs_device(cuda_mparticles *cmprts);

  void spine_reduce_gold(cuda_mparticles *cmprts);
  void sort_pairs_gold(cuda_mparticles *cmprts);

  void reorder_send_by_id(struct cuda_mparticles *cmprts);
  void find_n_send(cuda_mparticles *cmprts);
  void copy_from_dev_and_convert(cuda_mparticles *cmprts);
  void reorder_send_by_id_gold(cuda_mparticles *cmprts);
  void convert_and_copy_to_dev(cuda_mparticles *cmprts);
  void sort(cuda_mparticles *cmprts, int *n_prts_by_patch);
  void update_offsets(cuda_mparticles *cmprts);
  void update_offsets_gold(cuda_mparticles *cmprts);
  void count_received(cuda_mparticles *cmprts);
  void count_received_gold(cuda_mparticles *cmprts);
  void scan_scatter_received(cuda_mparticles *cmprts);
  void scan_scatter_received_gold(cuda_mparticles *cmprts);
  void reorder_send_buf_total(cuda_mparticles *cmprts);
  
public:
  thrust::device_vector<uint> d_alt_bidx;
  thrust::device_vector<uint> d_sums; // FIXME, too many arrays, consolidation would be good

  uint n_prts_send;
  uint n_prts_recv;

  thrust::device_vector<uint> d_spine_cnts;
  thrust::device_vector<uint> d_spine_sums;

  struct cuda_bnd *bpatch;
};

// ----------------------------------------------------------------------
// cuda_mparticles

struct cuda_mparticles : cuda_mparticles_base, cuda_mparticles_bnd
{
  cuda_mparticles(const Grid_t& grid, const Int3& bs);

  void reserve_all(const uint *n_prts_by_patch);
  uint get_n_prts();
  void setup_internals();
  void inject(cuda_mparticles_prt *buf, uint *buf_n_by_patch);
  const particle_cuda_real_t *patch_get_b_dxi(int p);
  const int *patch_get_b_mx(int p);

  psc_particle_cuda_buf_t *bnd_get_buffer(int p);
  void bnd_prep();
  void bnd_post();
  
  void dump();
  void dump_by_patch(uint *n_prts_by_patch);

public:
  void find_block_indices_ids();
  void reorder_and_offsets();
  void check_in_patch_unordered_slow(uint *nr_prts_by_patch);
  void check_bidx_id_unordered_slow(uint *n_prts_by_patch);
  void check_ordered();
  void check_ordered_slow();
  
public:
  // per particle
  thrust::device_vector<float4> d_alt_xi4;  // storage for out-of-place reordering of particle data
  thrust::device_vector<float4> d_alt_pxi4;
  thrust::device_vector<uint> d_bidx;       // block index (incl patch) per particle
  thrust::device_vector<uint> d_id;         // particle id for sorting

  std::vector<Real3> xb_by_patch; // lower left corner for each patch

  bool need_reorder;              // particles haven't yet been put into their sorted order
};

void cuda_mparticles_swap_alt(struct cuda_mparticles *cmprts);
void cuda_mparticles_reorder(struct cuda_mparticles *cmprts);


// ======================================================================
// cuda_mparticles_base implementation

// ----------------------------------------------------------------------
// set_particles

template<typename F>
void cuda_mparticles_base::set_particles(uint p, F getter)
{
  // FIXME, doing the copy here all the time would be nice to avoid
  // making sue we actually have a valid d_off would't hurt, either
  thrust::host_vector<uint> h_off(d_off);

  uint off = h_off[p * n_blocks_per_patch];
  uint n_prts = h_off[(p+1) * n_blocks_per_patch] - off;
  
  float4 *xi4  = new float4[n_prts];
  float4 *pxi4 = new float4[n_prts];

  for (int n = 0; n < n_prts; n++) {
    struct cuda_mparticles_prt prt = getter(n);

    for (int d = 0; d < 3; d++) {
      int bi = fint(prt.xi[d] * b_dxi[d]);
      if (bi < 0 || bi >= b_mx[d]) {
	printf("XXX xi %g %g %g\n", prt.xi[0], prt.xi[1], prt.xi[2]);
	printf("XXX n %d d %d xi4[n] %g biy %d // %d\n",
	       n, d, prt.xi[d], bi, b_mx[d]);
	if (bi < 0) {
	  prt.xi[d] = 0.f;
	} else {
	  prt.xi[d] *= (1. - 1e-6);
	}
      }
      bi = floorf(prt.xi[d] * b_dxi[d]);
      assert(bi >= 0 && bi < b_mx[d]);
    }

    xi4[n].x  = prt.xi[0];
    xi4[n].y  = prt.xi[1];
    xi4[n].z  = prt.xi[2];
    xi4[n].w  = cuda_int_as_float(prt.kind);
    pxi4[n].x = prt.pxi[0];
    pxi4[n].y = prt.pxi[1];
    pxi4[n].z = prt.pxi[2];
    pxi4[n].w = prt.qni_wni;
  }

  to_device(xi4, pxi4, n_prts, off);
  
  delete[] xi4;
  delete[] pxi4;
}

// ----------------------------------------------------------------------
// get_particles

template<typename F>
void cuda_mparticles_base::get_particles(uint p, F setter)
{
  // FIXME, doing the copy here all the time would be nice to avoid
  // making sue we actually have a valid d_off would't hurt, either
  thrust::host_vector<uint> h_off(d_off);

  uint off = h_off[p * n_blocks_per_patch];
  uint n_prts = h_off[(p+1) * n_blocks_per_patch] - off;
  
  float4 *xi4  = new float4[n_prts];
  float4 *pxi4 = new float4[n_prts];

  cuda_mparticles_reorder(static_cast<cuda_mparticles*>(this)); // FIXME
  from_device(xi4, pxi4, n_prts, off);
  
  for (int n = 0; n < n_prts; n++) {
    struct cuda_mparticles_prt prt;
    prt.xi[0]   = xi4[n].x;
    prt.xi[1]   = xi4[n].y;
    prt.xi[2]   = xi4[n].z;
    prt.kind    = cuda_float_as_int(xi4[n].w);
    prt.pxi[0]  = pxi4[n].x;
    prt.pxi[1]  = pxi4[n].y;
    prt.pxi[2]  = pxi4[n].z;
    prt.qni_wni = pxi4[n].w;

    setter(n, prt);

#if 0
    for (int d = 0; d < 3; d++) {
      int bi = fint(prt.xi[d] * b_dxi[d]);
      if (bi < 0 || bi >= b_mx[d]) {
	MHERE;
	mprintf("XXX xi %.10g %.10g %.10g\n", prt.xi[0], prt.xi[1], prt.xi[2]);
	mprintf("XXX n %d d %d xi %.10g b_dxi %.10g bi %d // %d\n",
		n, d, prt.xi[d] * b_dxi[d], b_dxi[d], bi, b_mx[d]);
      }
    }
#endif
  }

  delete[] (xi4);
  delete[] (pxi4);
}



#endif
