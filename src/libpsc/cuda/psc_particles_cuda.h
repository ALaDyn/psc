
#ifndef PSC_PARTICLES_CUDA_H
#define PSC_PARTICLES_CUDA_H

#include "particles.hxx"
#include "particles_traits.hxx"
#include "psc_bits.h"
#include "particles_simple.hxx"

#include <vector>

// ======================================================================
// particle_cuda_t

struct particle_cuda_t : psc_particle<float> {};

// ======================================================================
// psc_particle_cuda_buf_t

using psc_particle_cuda_buf_t = std::vector<particle_cuda_t>;

// ======================================================================
// cuda_mparticles_prt

struct cuda_mparticles_prt
{
  float xi[3];
  float pxi[3];
  int kind;
  float qni_wni;
};

struct cuda_mparticles;

// ======================================================================
// MparticlesCuda

struct MparticlesCuda : MparticlesBase
{
  using Self = MparticlesCuda;
  using particle_t = particle_cuda_t;
  using real_t = particle_t::real_t;
  using Real3 = Vec3<real_t>;
  using buf_t = psc_particle_cuda_buf_t;
  
  MparticlesCuda(const Grid_t& grid);

  MparticlesCuda(const MparticlesCuda&) = delete;
  ~MparticlesCuda();

  int get_n_prts() const override;
  void get_size_all(uint *n_prts_by_patch) const override;
  void reserve_all(const uint *n_prts_by_patch) override;
  void resize_all(const uint *n_prts_by_patch) override;
  void reset(const Grid_t& grid) override;

  void setup_internals();
  void inject_buf(cuda_mparticles_prt *buf, uint *buf_n_by_patch);
  void dump(const std::string& filename);

  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

  const int *patch_get_b_mx(int p);
  
  cuda_mparticles* cmprts() { return cmprts_; }

  struct patch_t
  {
    patch_t(MparticlesCuda& mp, int p)
      : mp_(mp), p_(p), pi_(mp.grid())
    {}

    buf_t& get_buf() { assert(0); static buf_t fake{}; return fake; } // FIXME

    int blockPosition(real_t xi, int d) const { return pi_.blockPosition(xi, d); }
    Int3 blockPosition(const Real3& xi) const { return pi_.blockPosition(xi); }
    int validCellIndex(const particle_t& prt) const { return pi_.validCellIndex(&prt.xi); }
  
    const int* get_b_mx() const;

  private:
    MparticlesCuda& mp_;
    int p_;
    ParticleIndexer<real_t> pi_;
  };

  const patch_t& operator[](int p) const { return patches_[p]; }
  patch_t&       operator[](int p)       { return patches_[p]; }

private:
  cuda_mparticles* cmprts_;
  std::vector<patch_t> patches_;

  template<typename MP>
  friend struct bnd_particles_policy_cuda;
};

template<>
struct Mparticles_traits<MparticlesCuda>
{
  static constexpr const char* name = "cuda";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};


#endif
