
#ifndef PSC_PARTICLES_CUDA_H
#define PSC_PARTICLES_CUDA_H

#include "particles.hxx"
#include "particles_traits.hxx"
#include "psc_bits.h"
#include "particles_simple.hxx"

#include <vector>

struct BS144
{
  using x = std::integral_constant<unsigned int, 1>;
  using y = std::integral_constant<unsigned int, 4>;
  using z = std::integral_constant<unsigned int, 4>;
};

struct BS444
{
  using x = std::integral_constant<unsigned int, 4>;
  using y = std::integral_constant<unsigned int, 4>;
  using z = std::integral_constant<unsigned int, 4>;
};

// ======================================================================
// particle_cuda_t

using particle_cuda_t = psc_particle<float>;

// ======================================================================
// cuda_mparticles_prt

struct cuda_mparticles_prt
{
  using real_t = float;
  using Real3 = Vec3<real_t>;

  cuda_mparticles_prt(Real3 x, Real3 p, real_t w, int kind)
    : x(x), p(p), w(w), kind(kind)
  {}
  
  Real3 x;
  real_t w;
  Real3 p; 
  int kind;
};

template<typename BS>
struct cuda_mparticles;

// ======================================================================
// MparticlesCuda

template<typename _BS>
struct MparticlesCuda : MparticlesBase
{
  using Self = MparticlesCuda;
  using BS = _BS;
  using particle_t = particle_cuda_t;
  using real_t = particle_t::real_t;
  using Real3 = Vec3<real_t>;
  using buf_t = std::vector<particle_cuda_t>;
  using CudaMparticles = cuda_mparticles<BS>;
  
  MparticlesCuda(const Grid_t& grid);
  ~MparticlesCuda();

  int get_n_prts() const override;
  void get_size_all(uint *n_prts_by_patch) const override;
  void reserve_all(const uint *n_prts_by_patch) override;
  void resize_all(const uint *n_prts_by_patch) override;
  void reset(const Grid_t& grid) override;

  void inject_buf(cuda_mparticles_prt *buf, uint *buf_n_by_patch);
  void dump(const std::string& filename);
  void push_back(int p, const particle_t& prt);
  bool check_after_push();

  void define_species(const char *name, double q, double m,
		      double max_local_np, double max_local_nm,
		      double sort_interval, double sort_out_of_place)
  {}
  
  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

  CudaMparticles* cmprts() { return cmprts_; }

  struct patch_t
  {
    using Double3 = Vec3<double>;
    
    struct const_accessor
    {
      const_accessor(const particle_t& prt, const patch_t& prts)
	: prt_{prt}, prts_{prts}
      {}
      
      Real3 u()   const { return prt_.p; }
      real_t w()  const { return prt_.w; }
      int kind()  const { return prt_.kind; }
      
      Double3 position() const
      {
	auto& patch = prts_.mp_.grid().patches[prts_.p_];
	
	return patch.xb + Double3{prt_.x};
      }
    
    private:
      const particle_t& prt_;
      const patch_t& prts_;
    };
  
    struct const_accessor_range
    {
      struct const_iterator : std::iterator<std::random_access_iterator_tag,
					    const_accessor,  // value type
					    ptrdiff_t,       // difference type
					    const_accessor*, // pointer type
					    const_accessor&> // reference type
      
      {
	const_iterator(const patch_t& prts, uint n)
	  : prts_{prts}, n_{n}
	{}
	
	bool operator==(const_iterator other) const { return n_ == other.n_; }
	bool operator!=(const_iterator other) const { return !(*this == other); }
	
	const_iterator& operator++() { n_++; return *this; }
	const_iterator operator++(int) { auto retval = *this; ++(*this); return retval; }
	//const_accessor operator*() { return {prts_[n_], prts_}; }
	
      private:
	const patch_t& prts_;
	uint n_;
      };
    
      const_accessor_range(const patch_t& prts)
	: prts_{prts}
      {}

      const_iterator begin() const;
      const_iterator end()   const;
      
    private:
      const patch_t& prts_;
    };

    patch_t(const MparticlesCuda& mp, int p)
      : mp_(mp), p_(p)
    {}

    const ParticleIndexer<real_t>& particleIndexer() const { return mp_.pi_; }

    void push_back(const particle_t& prt) { assert(0); }

    uint size()
    {
      uint n_prts_by_patch[mp_.grid().n_patches()];
      mp_.get_size_all(n_prts_by_patch);
      return n_prts_by_patch[p_];
    }

    const_accessor_range get() const { return {*this}; }
    
  private:
    const MparticlesCuda& mp_;
    int p_;
  };

  patch_t operator[](int p) const { return patch_t{*this, p}; }

private:
  CudaMparticles* cmprts_;
  ParticleIndexer<real_t> pi_;
};

template<>
struct Mparticles_traits<MparticlesCuda<BS144>>
{
  static constexpr const char* name = "cuda";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};


template<>
struct Mparticles_traits<MparticlesCuda<BS444>>
{
  static constexpr const char* name = "cuda444";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};


#endif
