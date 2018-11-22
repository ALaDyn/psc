
#pragma once

// ======================================================================
// InjectorBuffered

template<typename Mparticles>
struct InjectorBuffered
{
  using particle_t = typename Mparticles::particle_t;
  using real_t = typename particle_t::real_t;
  using Real3 = typename particle_t::Real3;
  using Double3 = Vec3<double>;
  
  struct Patch
  {
    Patch(InjectorBuffered& injector, int p)
      : injector_(injector), p_{p}, n_prts_{0} // FIXME, why (), {} does not work
    {}
    
    ~Patch()
    {
      injector_.n_prts_by_patch_[p_] += n_prts_;
    }
  
    void raw(const particle_t& prt)
    {
      injector_.buf_.push_back(prt);
      n_prts_++;
    }
    
    void operator()(const particle_inject& new_prt)
    {
      auto& patch = injector_.mprts_.grid().patches[p_];
      auto x = Double3::fromPointer(new_prt.x) - patch.xb;
      auto u = Double3::fromPointer(new_prt.u);
      raw({Real3(x), Real3(u), real_t(new_prt.w), new_prt.kind});
    }

    // FIXME do we want to keep this? or just have a particle_inject version instead?
    void raw(const std::vector<particle_t>& buf)
    {
      injector_.buf_.insert(injector_.buf_.end(), buf.begin(), buf.end());
      n_prts_ += buf.size();
    }
    
  private:
    InjectorBuffered& injector_;
    const int p_;
    uint n_prts_;
  };
  
  InjectorBuffered(Mparticles& mprts)
    : n_prts_by_patch_(mprts.n_patches()), last_patch_{0}, mprts_{mprts}
  {}

  ~InjectorBuffered()
  {
    assert(n_prts_by_patch_.size() == mprts_.n_patches());
    mprts_.inject(buf_, n_prts_by_patch_);
  }

  Patch operator[](int p)
  {
    // ensure that we inject particles into patches in ascending order
    assert(p >= last_patch_);
    last_patch_ = p;
    return {*this, p};
  }

private:
  std::vector<particle_t> buf_;
  std::vector<uint> n_prts_by_patch_;
  uint last_patch_;
  Mparticles& mprts_;
};

