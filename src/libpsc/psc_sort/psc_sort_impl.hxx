
#include "psc_sort_private.h"
#include "sort.hxx"

#include <particle_iter.h>
#include <psc_particles.h>

#include <mrc_profile.h>
#include <cassert>

// ======================================================================
// SortCountsort

template<typename MP>
struct SortCountsort : SortBase
{
  using mparticles_t = MP;
  using particle_t = typename mparticles_t::particle_t;
  
  void operator()(mparticles_t mprts)
  {
    for (int p = 0; p < mprts->n_patches(); p++) {
      auto& prts = mprts[p];
      unsigned int n_prts = prts.size();

      unsigned int n_cells = prts.pi_.n_cells_;
      unsigned int *cnts = new unsigned int[n_cells]{};
    
      // count
      PARTICLE_ITER_LOOP(prt_iter, prts.begin(), prts.end()) {
	int cni = prts.validCellIndex(*prt_iter);
	cnts[cni]++;
      }
    
      // calc offsets
      int cur = 0;
      for (int i = 0; i < n_cells; i++) {
	int n = cnts[i];
	cnts[i] = cur;
	cur += n;
      }
      assert(cur == n_prts);
    
      // move into new position
      particle_t *particles2 = new particle_t[n_prts];
      PARTICLE_ITER_LOOP(prt_iter, prts.begin(), prts.end()) {
	unsigned int cni = prts.validCellIndex(*prt_iter);
	particles2[cnts[cni]] = *prt_iter;
	cnts[cni]++;
      }
    
      // back to in-place
      memcpy(&*prts.begin(), particles2, n_prts * sizeof(*particles2));
    
      delete[] particles2;
      delete[] cnts;
    }
  }

  void run(struct psc_mparticles *mprts_base) override
  {
    mparticles_t mprts = mprts_base->get_as<mparticles_t>();
    (*this)(mprts);
    mprts.put_as(mprts_base);
  }
};

// ======================================================================
// SortCountsort2
// use a separate array of cell indices

template<typename MP>
struct SortCountsort2 : SortBase
{
  using mparticles_t = MP;
  using particle_t = typename mparticles_t::particle_t;
  
  void operator()(mparticles_t mprts)
  {
    for (int p = 0; p < mprts->n_patches(); p++) {
      auto& prts = mprts[p];
      unsigned int n_prts = prts.size();
      
      unsigned int n_cells = prts.pi_.n_cells_;
      unsigned int *cnis = new unsigned int[n_prts];
      // FIXME, might as well merge counting here, too
      int i = 0;
      for (auto prt_iter = prts.begin(); prt_iter != prts.end(); ++prt_iter, ++i) {
	cnis[i] = prts.validCellIndex(*prt_iter);
      }
      
      unsigned int *cnts = new unsigned int[n_cells]{};
	
      // count
      for (int i = 0; i < n_prts; i++) {
	unsigned int cni = cnis[i];
	cnts[cni]++;
      }
	
      // calc offsets
      int cur = 0;
      for (int i = 0; i < n_cells; i++) {
	int n = cnts[i];
	cnts[i] = cur;
	cur += n;
      }
      assert(cur == n_prts);
      
      // move into new position
      particle_t *particles2 = new particle_t[n_prts];
      for (int i = 0; i < n_prts; i++) {
	unsigned int cni = cnis[i];
	int n = 1;
	while (i+n < n_prts && cnis[i+n] == cni) {
	  n++;
	}
	memcpy(&particles2[cnts[cni]], &prts[i], n * sizeof(*particles2));
	cnts[cni] += n;
	i += n - 1;
      }
      
      // back to in-place
      memcpy(&*prts.begin(), particles2, n_prts * sizeof(*particles2));
      
      delete[] particles2;
      delete[] cnis;
      delete[] cnts;
    }
  }

  void run(struct psc_mparticles *mprts_base) override
  {
    mparticles_t mprts = mprts_base->get_as<mparticles_t>();
    (*this)(mprts);
    mprts.put_as(mprts_base);
  }
};

// ======================================================================
// SortNone

template<typename MP>
struct SortNone : SortBase
{
  using mparticles_t = MP;

  void operator()(mparticles_t mprts) {}

  void run(struct psc_mparticles *mprts_base) override {}
};
    
