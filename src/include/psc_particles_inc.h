
#if PSC_PARTICLES_AS_DOUBLE

#define PFX(x) psc_mparticles_double_ ## x
#define psc_mparticles_sub psc_mparticles_double
using mparticles_t = mparticles_double_t;

#elif PSC_PARTICLES_AS_SINGLE

#define PFX(x) psc_mparticles_single_ ## x
#define psc_mparticles_sub psc_mparticles_single
using mparticles_t = mparticles_single_t;

#elif PSC_PARTICLES_AS_FORTRAN

#define PFX(x) psc_mparticles_fortran_ ## x
#define psc_mparticles_sub psc_mparticles_fortran
using mparticles_t = mparticles_fortran_t;

#endif

template<typename MP, typename F>
void psc_mparticles_copy_from(mparticles_t mprts_to, MP mprts_from, unsigned int flags,
			      F convert_from)
{
  int n_patches = mprts_to.n_patches();
  
  for (int p = 0; p < n_patches; p++) {
    mparticles_t::patch_t& prts = mprts_to[p];
    int n_prts = prts.size();
    for (int n = 0; n < n_prts; n++) {
      convert_from(&prts[n], n, mprts_from, p);
    }
  }
}

template<typename MP, typename F>
void psc_mparticles_copy_to(mparticles_t mprts_from, MP mprts_to, unsigned int flags,
			    F convert_to)
{
  int n_patches = mprts_from.n_patches();
  
  for (int p = 0; p < n_patches; p++) {
    mparticles_t::patch_t& prts = mprts_from[p];
    int n_prts = prts.size();
    for (int n = 0; n < n_prts; n++) {
      convert_to(&prts[n], n, mprts_to, p);
    }
  }
}


