
#ifndef PARTICLES_HXX
#define PARTICLES_HXX

#include "psc_particles.h"

#include <vector>

// ======================================================================
// particles_base

template<typename P>
struct particles_base
{
  using particle_t = P;
  using real_t = typename particle_t::real_t;
  using buf_t = std::vector<particle_t>;
  using iterator = typename buf_t::iterator;
  
  buf_t buf;

  int b_mx[3];
  real_t b_dxi[3];

  struct psc_mparticles *mprts;
  int p;

  particles_base() = default;
  particles_base(const particles_base&) = delete;
  
  particle_t& operator[](int n) { return buf[n]; }
  iterator begin() { return buf.begin(); }
  iterator end() { return buf.end(); }
  unsigned int size() const { return buf.size(); }
  void reserve(unsigned int new_capacity) { buf.reserve(new_capacity); }
  void push_back(const particle_t& prt) { buf.push_back(prt); }

  void resize(unsigned int new_size)
  {
    assert(new_size <= buf.capacity());
    buf.resize(new_size);
  }

  buf_t& get_buf()
  {
    return buf;
  }

  const int* get_b_mx() const { return b_mx; }
  const real_t* get_b_dxi() const { return b_dxi; }
};

template<typename P>
struct psc_mparticles_patch : particles_base<P>
{
};

// ======================================================================
// mparticles_base

template<typename S>
struct mparticles_base
{
  using sub_t = S;

  explicit mparticles_base(psc_mparticles *mprts) : mprts_(mprts) { }

  void put_as(psc_mparticles *mprts_base, unsigned int flags = 0)
  {
    psc_mparticles_put_as(mprts_, mprts_base, flags);
  }

  unsigned int n_patches() { return mprts_->nr_patches; }
  
  psc_mparticles *mprts() { return mprts_; }
  
  sub_t* sub() { return mrc_to_subobj(mprts(), sub_t); }

private:
  psc_mparticles *mprts_;
};

// ======================================================================
// psc_particle

template<class R>
struct psc_particle
{
  using real_t = R;

  real_t xi, yi, zi;
  real_t qni_wni;
  real_t pxi, pyi, pzi;
  int kind_;

  int kind() { return kind_; }
};

// ======================================================================
// mparticles

template<typename S>
struct mparticles : mparticles_base<S>
{
  using Base = mparticles_base<S>;
  using particles_t = typename Base::sub_t::particles_t;
  using patch_t = particles_t;
  using particle_buf_t = typename particles_t::buf_t;
  
  mparticles(psc_mparticles *mprts) : mparticles_base<S>(mprts) { }

  particles_t& operator[](int p)
  {
    return this->sub()->patch[p];
  }

};

#endif

