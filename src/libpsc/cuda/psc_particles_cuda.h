
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

template<typename BS>
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
  using BS = BS144;
  
  MparticlesCuda(const Grid_t& grid);

  MparticlesCuda(const MparticlesCuda&) = delete;
  ~MparticlesCuda();

  int get_n_prts() const override;
  void get_size_all(uint *n_prts_by_patch) const override;
  void reserve_all(const uint *n_prts_by_patch) override;
  void resize_all(const uint *n_prts_by_patch) override;
  void reset(const Grid_t& grid) override;

  void inject_buf(cuda_mparticles_prt *buf, uint *buf_n_by_patch);
  void dump(const std::string& filename);

  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

  cuda_mparticles<BS>* cmprts() { return cmprts_; }

  struct patch_t
  {
    patch_t(const MparticlesCuda& mp, int p)
      : mp_(mp)
    {}

    const ParticleIndexer<real_t>& particleIndexer() const { return mp_.pi_; }

  private:
    const MparticlesCuda& mp_;
  };

  patch_t operator[](int p) const { return patch_t{*this, p}; }

private:
  cuda_mparticles<BS>* cmprts_;
  ParticleIndexer<real_t> pi_;
};

template<>
struct Mparticles_traits<MparticlesCuda>
{
  static constexpr const char* name = "cuda";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};


#endif
