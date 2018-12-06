
#pragma once

#include "particles.hxx"
#include "particle_simple.hxx"
#include "particle_indexer.hxx"

#include <iterator>

// ======================================================================
// InjectorSimple

template<typename Mparticles>
struct InjectorSimple
{
  using Particle = typename Mparticles::particle_t;
  using real_t = typename Particle::real_t;
  
  struct Patch
  {
    Patch(Mparticles& mprts, int p)
      : mprts_{mprts}, p_{p}
    {}
    
    void operator()(const particle_inject& new_prt)
    {
      const auto& patch = mprts_.grid().patches[p_];
      for (int d = 0; d < 3; d++) {
	assert(new_prt.x[d] >= patch.xb[d]);
	assert(new_prt.x[d] <= patch.xe[d]);
      }
      
      auto prt = Particle{{real_t(new_prt.x[0] - patch.xb[0]), real_t(new_prt.x[1] - patch.xb[1]), real_t(new_prt.x[2] - patch.xb[2])},
			  {real_t(new_prt.u[0]), real_t(new_prt.u[1]), real_t(new_prt.u[2])},
			  real_t(new_prt.w * mprts_.grid().kinds[new_prt.kind].q),
			  new_prt.kind};
      mprts_[p_].push_back(prt);
    }
    
    void reweight(const particle_inject& new_prt)
    {
      auto& grid = mprts_.grid();
      real_t dVi = 1.f / (grid.domain.dx[0] * grid.domain.dx[1] * grid.domain.dx[2]);
      auto prt = new_prt;
      assert(0); // FIXME, have to actually do reweighting
      (*this)(prt);
    }
    
  private:
    Mparticles& mprts_;
    int p_;
  };
  
  InjectorSimple(Mparticles& mprts)
    : mprts_{mprts}
  {}

  Patch operator[](int p) const { return {mprts_, p}; }

private:
  Mparticles& mprts_;
};

// ======================================================================
// ConstAcessorSimple

template<typename Mparticles>
struct ConstAccessorSimple
{
  using Particle = typename Mparticles::particle_t;
  using mparticles_patch = typename Mparticles::Patch;
  using real_t = typename Mparticles::real_t;
  using Real3 = Vec3<real_t>;
  using Double3 = Vec3<double>;
  
  struct const_accessor
  {
    const_accessor(const Particle& prt, const mparticles_patch& prts)
      : prt_{prt}, prts_{prts}
    {}

    Real3 x()   const { return prt_.x(); }
    Real3 u()   const { return prt_.u(); }
    real_t w()  const { return prt_.qni_wni() / prts_.grid().kinds[prt_.kind()].q; }
    real_t qni_wni() const { return prt_.qni_wni(); }
    int kind()  const { return prt_.kind(); }

    Double3 position() const
    {
      auto& patch = prts_.grid().patches[prts_.p()]; // FIXME, generally, it'd be nice to have a better way to get this

      return patch.xb +	Double3(prt_.x());
    }
    
  private:
    const Particle& prt_;
    const mparticles_patch& prts_;
  };
  
  struct Patch
  {
    struct const_iterator : std::iterator<std::random_access_iterator_tag,
					  const_accessor,  // value type
					  ptrdiff_t,       // difference type
					  const_accessor*, // pointer type
					  const_accessor&> // reference type
					   
    {
      const_iterator(const mparticles_patch& prts, uint n)
	: prts_{prts}, n_{n}
      {}
      
      bool operator==(const_iterator other) const { return n_ == other.n_; }
      bool operator!=(const_iterator other) const { return !(*this == other); }

      const_iterator& operator++()  { n_++; return *this; }
      const_iterator operator++(int) { auto retval = *this; ++(*this); return retval; }
      const_accessor operator*() { return {prts_[n_], prts_}; }

    private:
      const mparticles_patch& prts_;
      uint n_;
    };
    
    Patch(const mparticles_patch& prts)
      : prts_{prts}
    {}

    const_iterator begin() const { return {prts_, 0}; }
    const_iterator end()   const { return {prts_, prts_.size()}; }
    uint size() const { return prts_.size(); }

  private:
    const mparticles_patch& prts_;
  };

  ConstAccessorSimple(Mparticles& mprts)
    : mprts_{mprts}
  {}

  Patch operator[](int p) { return {mprts_[p]}; }

private:
  Mparticles& mprts_;
};

// ======================================================================
// mparticles_patch

template<typename P>
struct Mparticles;

template<typename P>
struct mparticles_patch
{
  using Particle = P;
  using real_t = typename Particle::real_t;
  using Real3 = Vec3<real_t>;
  using Double3 = Vec3<double>;
  using buf_t = std::vector<Particle>;
  using iterator = typename buf_t::iterator;
  using const_iterator = typename buf_t::const_iterator;

  // FIXME, I would like to delete the copy ctor because I don't
  // want to copy Patch by mistake, but that doesn't play well with
  // putting the patches into std::vector
  // mparticles_patch_base(const mparticles_patch_base&) = delete;

  mparticles_patch(Mparticles<P>* mprts, int p)
    : mprts_(mprts),
      p_(p),
      grid_(&mprts->grid())
  {}

  Particle& operator[](int n) { return buf[n]; }
  const Particle& operator[](int n) const { return buf[n]; }
  const_iterator begin() const { return buf.begin(); }
  iterator begin() { return buf.begin(); }
  const_iterator end() const { return buf.end(); }
  iterator end() { return buf.end(); }
  unsigned int size() const { return buf.size(); }
  void reserve(unsigned int new_capacity) { buf.reserve(new_capacity); }

  void push_back(const Particle& new_prt)
  {
    // need to copy because we modify it
    auto prt = new_prt;
    checkInPatchMod(prt);
    validCellIndex(prt);
    buf.push_back(prt);
  }

  void resize(unsigned int new_size)
  {
    assert(new_size <= buf.capacity());
    buf.resize(new_size);
  }

  void check() const
  {
    for (auto& prt : buf) {
      validCellIndex(prt);
    }
  }

  // ParticleIndexer functionality
  int cellPosition(real_t xi, int d) const { return mprts_->pi_.cellPosition(xi, d); }
  int validCellIndex(const Particle& prt) const { return mprts_->pi_.validCellIndex(prt.x()); }

  void checkInPatchMod(Particle& prt) const { return mprts_->pi_.checkInPatchMod(prt.x()); }

  // FIXME, grid is always double precision, so this will switch precision
  // where not desired. should use same info stored in mprts at right precision
  real_t prt_qni(const Particle& prt) const { return grid().kinds[prt.kind()].q; }
  real_t prt_mni(const Particle& prt) const { return grid().kinds[prt.kind()].m; }
  real_t prt_wni(const Particle& prt) const { return prt.qni_wni() / prt_qni(prt); }
  real_t prt_qni_wni(const Particle& prt) const { return prt.qni_wni(); }

  const Grid_t& grid() const { return *grid_; }
  int p() const { return p_; }

  buf_t buf;

private:
  Mparticles<P>* mprts_;
  int p_;
  const Grid_t* grid_;
};

// ======================================================================
// Mparticles

template<typename P>
struct Mparticles : MparticlesBase
{
  using Self = Mparticles<P>;
  using particle_t = P;
  using real_t = typename particle_t::real_t;
  using Real3 = Vec3<real_t>;
  using Patch = mparticles_patch<particle_t>;
  using BndpParticle = P;
  using buf_t = typename Patch::buf_t;

  Mparticles(const Grid_t& grid)
    : MparticlesBase(grid),
      pi_(grid)
  {
    patches_.reserve(grid.n_patches());
    for (int p = 0; p < grid.n_patches(); p++) {
      patches_.emplace_back(this, p);
    }
  }

  void reset(const Grid_t& grid) override
  {
    MparticlesBase::reset(grid);
    patches_.clear();
    patches_.reserve(grid.n_patches());
    for (int p = 0; p < grid.n_patches(); p++) {
      patches_.emplace_back(this, p);
    }
  }

  const Patch& operator[](int p) const { return patches_[p]; }
  Patch&       operator[](int p)       { return patches_[p]; }

  void reserve_all(const std::vector<uint> &n_prts_by_patch)
  {
    for (int p = 0; p < patches_.size(); p++) {
      patches_[p].reserve(n_prts_by_patch[p]);
    }
  }

  void resize_all(const std::vector<uint>& n_prts_by_patch)
  {
    for (int p = 0; p < patches_.size(); p++) {
      patches_[p].resize(n_prts_by_patch[p]);
    }
  }

  void reset() // FIXME, "reset" is used for two very different functions
  {
    for (int p = 0; p < patches_.size(); p++) {
      patches_[p].resize(0);
    }
  }

  std::vector<uint> get_size_all() const override
  {
    std::vector<uint> n_prts_by_patch(n_patches());
    for (int p = 0; p < patches_.size(); p++) {
      n_prts_by_patch[p] = patches_[p].size();
    }
    return n_prts_by_patch;
  }

  int get_n_prts() const override
  {
    int n_prts = 0;
    for (auto const& patch : patches_) {
      n_prts += patch.size();
    }
    return n_prts;
  }

  const ParticleIndexer<real_t>& particleIndexer() const { return pi_; }
  
  InjectorSimple<Mparticles> injector() { return {*this}; }
  ConstAccessorSimple<Mparticles> accessor() { return {*this}; }
  
  void check() const
  {
    for (auto& patch: patches_) {
      patch.check();
    }
  }

  void dump(const std::string& filename)
  {
    FILE* file = fopen(filename.c_str(), "w");
    assert(file);

    for (int p = 0; p < n_patches(); p++) {
      auto& prts = (*this)[p];
      fprintf(file, "mparticles_dump: p%d n_prts = %d\n", p, prts.size());
      for (int n = 0; n < prts.size(); n++) {
	auto& prt = prts[n];
	fprintf(file, "mparticles_dump: [%d] %g %g %g // %d // %g %g %g // %g\n",
		n, prt.xi, prt.yi, prt.zi, prt.kind_,
		prt.pxi, prt.pyi, prt.pzi, prt.qni_wni_);
      }
    }
    fclose(file);
  }
  
  void define_species(const char *name, double q, double m,
		      double max_local_np, double max_local_nm,
		      double sort_interval, double sort_out_of_place)
  {}
  
  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

private:
  std::vector<Patch> patches_;
public: // FIXME
  ParticleIndexer<real_t> pi_;
};

