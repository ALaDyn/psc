
#pragma once

#include "cuda_iface.h"
#include "cuda_mparticles_indexer.h"
#include "cuda_mparticles_sort.cuh"

#include "particles.hxx"
#include "psc_bits.h"
#include "cuda_bits.h"

#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/binary_search.h>

// ======================================================================
// MparticlesCudaStorage

struct MparticlesCudaStorage
{
  void resize(size_t n)
  {
    xi4.resize(n);
    pxi4.resize(n);
  }
  
  DParticleCuda load(int n) const
  {
    float4 _xi4 = xi4[n];
    float4 _pxi4 = pxi4[n];
    return {{_xi4.x, _xi4.y, _xi4.z}, {_pxi4.x, _pxi4.y, _pxi4.z}, _pxi4.w, cuda_float_as_int(_xi4.w)};
  }

  void store(const DParticleCuda& prt, int n)
  {
    float4 _xi4 = { prt.x()[0], prt.x()[1], prt.x()[2], cuda_int_as_float(prt.kind()) };
    float4 _pxi4 = { prt.u()[0], prt.u()[1], prt.u()[2], prt.qni_wni() };
    xi4[n] = _xi4;
    pxi4[n] = _pxi4;
  }
  
  thrust::device_vector<float4> xi4;
  thrust::device_vector<float4> pxi4;
};

// ======================================================================
// HMparticlesCudaStorage

struct HMparticlesCudaStorage
{
  HMparticlesCudaStorage(size_t n)
    : xi4{n}, pxi4{n}
  {}

  HMparticlesCudaStorage(const MparticlesCudaStorage& storage)
    : xi4{storage.xi4}, pxi4{storage.pxi4}
  {}

  // FIXME, why so many warnings?
  void resize(size_t n)
  {
    xi4.resize(n);
    pxi4.resize(n);
  }
  
  DParticleCuda load(int n) const
  {
    float4 _xi4 = xi4[n];
    float4 _pxi4 = pxi4[n];
    return {{_xi4.x, _xi4.y, _xi4.z}, {_pxi4.x, _pxi4.y, _pxi4.z}, _pxi4.w, cuda_float_as_int(_xi4.w)};
  }

  void store(const DParticleCuda& prt, int n)
  {
    float4 _xi4 = { prt.x()[0], prt.x()[1], prt.x()[2], cuda_int_as_float(prt.kind()) };
    float4 _pxi4 = { prt.u()[0], prt.u()[1], prt.u()[2], prt.qni_wni() };
    xi4[n] = _xi4;
    pxi4[n] = _pxi4;
  }
  
  thrust::host_vector<float4> xi4;
  thrust::host_vector<float4> pxi4;
};

// ======================================================================
// DMparticlesCudaStorage

struct DMparticlesCudaStorage
{
  // FIXME, could be operator[]
  __host__ __device__
  DParticleCuda load(int n)
  {
    float4 _xi4 = xi4[n];
    float4 _pxi4 = pxi4[n];
    return {{_xi4.x, _xi4.y, _xi4.z}, {_pxi4.x, _pxi4.y, _pxi4.z}, _pxi4.w, cuda_float_as_int(_xi4.w)};
  }

  __host__ __device__
  void store(const DParticleCuda& prt, int n)
  {
    store_position(prt, n);
    store_momentum(prt, n);
  }
  
  __host__ __device__
  void store_position(const DParticleCuda& prt, int n)
  {
    float4 _xi4 = { prt.x()[0], prt.x()[1], prt.x()[2], cuda_int_as_float(prt.kind()) };
    xi4[n] = _xi4;
  }

  __host__ __device__
  void store_momentum(const DParticleCuda& prt, int n)
  {
    float4 _pxi4 = { prt.u()[0], prt.u()[1], prt.u()[2], prt.qni_wni() };
    pxi4[n] = _pxi4;
  }
  
  float4 *xi4;
  float4 *pxi4;
};

// ======================================================================
// cuda_mparticles_base

template<typename BS>
struct cuda_mparticles_base : cuda_mparticles_indexer<BS>
{
  cuda_mparticles_base(const Grid_t& grid);
  // copy constructor would work fine, but we don't want to copy everything
  // by accident
  cuda_mparticles_base(const cuda_mparticles<BS>&) = delete;

protected:
  void resize(uint size);

public:
  void get_size_all(uint *n_prts_by_patch);

  // per particle
  MparticlesCudaStorage storage;

  // per block
  cuda_mparticles_sort2 by_block_;

  uint n_prts = 0;                       // total # of particles across all patches
  const Grid_t& grid_;
};

template<typename _Mparticles>
struct ConstPatchCuda
{
  using Mparticles = _Mparticles;
  using particle_t = typename Mparticles::particle_t;
  using real_t = typename particle_t::real_t;
  using Real3 = typename particle_t::Real3;

  struct const_accessor
  {
    using Double3 = Vec3<double>;
    
    const_accessor(const particle_t& prt, const ConstPatchCuda& patch)
      : prt_{prt}, patch_{patch}
    {}

    Real3 x()   const { return prt_.x(); }
    Real3 u()   const { return prt_.u(); }
    real_t w()  const { return prt_.qni_wni() / patch_.grid().kinds[prt_.kind()].q; }
    int kind()  const { return prt_.kind(); }
    
    Double3 position() const
    {
      auto& patch = patch_.mprts_.grid().patches[patch_.p_];
      
      return patch.xb + Double3(prt_.x());
    }
    
  private:
    particle_t prt_;
    const ConstPatchCuda patch_;
  };
  
  struct const_accessor_range
  {
    struct const_iterator : std::iterator<std::random_access_iterator_tag,
      const_accessor,  // value type
      ptrdiff_t,       // difference type
      const_accessor*, // pointer type
      const_accessor&> // reference type
    
    {
      const_iterator(const const_accessor_range& range, uint n)
	: range_{range}, n_{n}
      {}
      
      bool operator==(const_iterator other) const { return n_ == other.n_; }
      bool operator!=(const_iterator other) const { return !(*this == other); }
      
      const_iterator& operator++() { n_++; return *this; }
      const_iterator operator++(int) { auto retval = *this; ++(*this); return retval; }
      const_accessor operator*() { return {range_.data_[n_], {range_.mprts_, range_.p_}}; }
      
    private:
      const const_accessor_range range_;
      uint n_;
    };
    
    const_accessor_range(const Mparticles& mprts, int p)
      : mprts_{mprts}, p_{p}, data_{const_cast<Mparticles&>(mprts_).get_particles(p_)}
    // FIXME, const hacking around reorder may change state...
    {}

    const_iterator begin() const { return {*this, 0}; }
    const_iterator end()   const { return {*this, uint(data_.size())}; }
    
  private:
    const Mparticles& mprts_;
    int p_;
    const std::vector<particle_t> data_;
  };

  ConstPatchCuda(const Mparticles& mprts, int p)
    : mprts_{mprts}, p_(p)
  {}
  
  const_accessor_range get()
  {
    return {mprts_, p_};
  }
  
private:
  const Mparticles& mprts_;
  int p_;
};

// ----------------------------------------------------------------------
// cuda_mparticles

template<typename BS>
struct DMparticlesCuda;

template<typename _BS>
struct cuda_mparticles : cuda_mparticles_base<_BS>
{
  using BS = _BS;
  using particle_t = DParticleCuda;
  using real_t = particle_t::real_t;
  using Real3 = Vec3<real_t>;
  using DMparticles = DMparticlesCuda<BS>;
  using ConstPatch = ConstPatchCuda<cuda_mparticles>;

  cuda_mparticles(const Grid_t& grid);

  ConstPatch operator[](int p) const { return {*this, p}; }
  InjectorBuffered<cuda_mparticles> injector() { return {*this}; }

  uint get_n_prts();
  void inject(const std::vector<particle_t>& buf, const std::vector<uint>& buf_n_by_patch);

  std::vector<particle_t> get_particles(int beg, int end);
  std::vector<particle_t> get_particles(int p);

  uint start(int p);

  void dump(const std::string& filename) const;
  void dump_by_patch(uint *n_prts_by_patch);

  // internal / testing use
  void inject_initial(const std::vector<particle_t>& buf,
		      const std::vector<uint>& n_prts_by_patch);
  void setup_internals();

public:
  void find_block_indices_ids(thrust::device_vector<uint>& d_idx, thrust::device_vector<uint>& d_id);
  void find_cell_indices_ids(thrust::device_vector<uint>& d_idx, thrust::device_vector<uint>& d_id);
  void reorder();
  void reorder(const thrust::device_vector<uint>& d_id);
  void reorder_and_offsets(const thrust::device_vector<uint>& d_idx, const thrust::device_vector<uint>& d_id,
			   thrust::device_vector<uint>& d_off);
  void reorder_and_offsets_slow();
  void swap_alt();

  bool check_in_patch_unordered_slow();
  bool check_bidx_id_unordered_slow();
  bool check_ordered();
  bool check_bidx_after_push();

  void resize(uint n_prts);

  const Grid_t& grid() const { return this->grid_; }

public:
  MparticlesCudaStorage alt_storage; // storage for out-of-place reordering of particle data

  std::vector<Real3> xb_by_patch; // lower left corner for each patch

  bool need_reorder = { false };            // particles haven't yet been put into their sorted order
};

template<typename BS_>
struct DMparticlesCuda : DParticleIndexer<BS_>
{
  using BS = BS_;
  using typename DParticleIndexer<BS>::real_t;
  
  static const int MAX_N_KINDS = 4;

  DMparticlesCuda(cuda_mparticles<BS>& cmprts)
    : DParticleIndexer<BS>{cmprts},
      dt_(cmprts.grid_.dt),
      fnqs_(cmprts.grid_.norm.fnqs),
      fnqxs_(cmprts.grid_.domain.dx[0] * fnqs_ / dt_),
      fnqys_(cmprts.grid_.domain.dx[1] * fnqs_ / dt_),
      fnqzs_(cmprts.grid_.domain.dx[2] * fnqs_ / dt_),
      dqs_(.5f * cmprts.grid_.norm.eta * dt_),
      storage{cmprts.storage.xi4.data().get(), cmprts.storage.pxi4.data().get()},
      alt_storage{cmprts.alt_storage.xi4.data().get(), cmprts.alt_storage.pxi4.data().get()},
      off_(cmprts.by_block_.d_off.data().get()),
      bidx_(cmprts.by_block_.d_idx.data().get()),
      id_(cmprts.by_block_.d_id.data().get()),
      n_blocks_(cmprts.n_blocks)
  {
    auto& grid = cmprts.grid_;
    
    int n_kinds = grid.kinds.size();
    assert(n_kinds <= MAX_N_KINDS);
    for (int k = 0; k < n_kinds; k++) {
      dq_[k] = dqs_ * grid.kinds[k].q / grid.kinds[k].m;
      q_inv_[k] = 1.f / grid.kinds[k].q;
      q_[k] = grid.kinds[k].q;
      m_[k] = grid.kinds[k].m;
    }
  }

  __device__ real_t dt() const { return dt_; }
  __device__ real_t fnqs() const { return fnqs_; }
  __device__ real_t fnqxs() const { return fnqxs_; }
  __device__ real_t fnqys() const { return fnqys_; }
  __device__ real_t fnqzs() const { return fnqzs_; }
  __device__ real_t q_inv(int k) const { return q_inv_[k]; }
  __device__ real_t dq(int k) const { return dq_[k]; }
  __device__ real_t q(int k) const { return q_[k]; }
  __device__ real_t m(int k) const { return m_[k]; }

private:
  real_t dt_;
  real_t fnqs_;
  real_t fnqxs_, fnqys_, fnqzs_;
  real_t dqs_;
  real_t dq_[MAX_N_KINDS];
  real_t q_inv_[MAX_N_KINDS];
  real_t q_[MAX_N_KINDS];
  real_t m_[MAX_N_KINDS];
public:
  DMparticlesCudaStorage storage;
  DMparticlesCudaStorage alt_storage;
  uint *off_;
  uint *bidx_;
  uint *id_;
  uint n_blocks_;
};

