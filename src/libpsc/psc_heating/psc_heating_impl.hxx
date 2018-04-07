
#include "heating.hxx"

#include <stdlib.h>

// ======================================================================
// Heating__

template<typename MP>
struct Heating__ : HeatingBase
{
  using Mparticles = MP;
  using real_t = typename Mparticles::real_t;
  using particle_t = typename Mparticles::particle_t;
  
  // ----------------------------------------------------------------------
  // ctor

  template<typename FUNC>
  Heating__(int interval, int kind, FUNC get_H)
    : get_H_(get_H)
  {
    heating_dt_ = interval * ppsc->dt;
  }
  
  // ----------------------------------------------------------------------
  // kick_particle

  void kick_particle(particle_t& prt, real_t H)
  {
    float ran1, ran2, ran3, ran4, ran5, ran6;
    do {
      ran1 = random() / ((float) RAND_MAX + 1);
      ran2 = random() / ((float) RAND_MAX + 1);
      ran3 = random() / ((float) RAND_MAX + 1);
      ran4 = random() / ((float) RAND_MAX + 1);
      ran5 = random() / ((float) RAND_MAX + 1);
      ran6 = random() / ((float) RAND_MAX + 1);
    } while (ran1 >= 1.f || ran2 >= 1.f || ran3 >= 1.f ||
	     ran4 >= 1.f || ran5 >= 1.f || ran6 >= 1.f);

    real_t ranx = sqrtf(-2.f*logf(1.0-ran1)) * cosf(2.f*M_PI*ran2);
    real_t rany = sqrtf(-2.f*logf(1.0-ran3)) * cosf(2.f*M_PI*ran4);
    real_t ranz = sqrtf(-2.f*logf(1.0-ran5)) * cosf(2.f*M_PI*ran6);

    real_t Dpxi = sqrtf(H * heating_dt_);
    real_t Dpyi = sqrtf(H * heating_dt_);
    real_t Dpzi = sqrtf(H * heating_dt_);

    prt.pxi += Dpxi * ranx;
    prt.pyi += Dpyi * rany;
    prt.pzi += Dpzi * ranz;
  }

  void operator()(Mparticles& mprts)
  {
    for (int p = 0; p < mprts.n_patches(); p++) {
      auto& prts = mprts[p];
      auto& patch = mprts.grid().patches[p];
      for (auto prt_iter = prts.begin(); prt_iter != prts.end(); ++prt_iter) {
	particle_t& prt = *prt_iter;
	if (prt.kind() != kind_) {
	  continue;
	}
      
	double xx[3] = {
	  prt.xi + patch.xb[0],
	  prt.yi + patch.xb[1],
	  prt.zi + patch.xb[2],
	};

	double H = get_H_(xx);
	if (H > 0) {
	  kick_particle(prt, H);
	}
      }
    }
  }
  
  void run(PscMparticlesBase mprts_base) override
  {
    auto& mprts = mprts_base->get_as<Mparticles>();
    (*this)(mprts);
    mprts_base->put_as(mprts);
  }
  
private:
  int kind_;
  real_t heating_dt_;
  std::function<double(const double*)> get_H_;
};

