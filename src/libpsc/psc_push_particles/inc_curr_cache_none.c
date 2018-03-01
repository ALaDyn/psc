
struct curr_cache_t : flds_curr_t
{
  curr_cache_t(const flds_curr_t& f)
    : flds_curr_t(f.ib(), f.im(), f.n_comps(), f.data_)
  {}
  
};

#ifndef CURR_CACHE_DIM
#define CURR_CACHE_DIM DIM_XYZ
#endif

CUDA_DEVICE static inline void
curr_cache_add(curr_cache_t curr_cache, int m, int i, int j, int k,
	       curr_cache_t::real_t val)
{
#if CURR_CACHE_DIM == DIM_XYZ
  using FieldsJ = Fields3d<curr_cache_t, dim_xyz>;
#elif CURR_CACHE_DIM == DIM_XZ
  using FieldsJ = Fields3d<curr_cache_t, dim_xz>;
#elif CURR_CACHE_DIM == DIM_1
  using FieldsJ = Fields3d<curr_cache_t, dim_1>;
#else
#error unhandled CURR_CACHE_DIM
#endif

  FieldsJ J(curr_cache);
  curr_cache_t::real_t *addr = &J(JXI+m, i,j,k);
  atomicAdd(addr, val);
}

