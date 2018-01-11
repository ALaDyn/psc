
#define PTYPE_SINGLE          1
#define PTYPE_DOUBLE          2
#define PTYPE_SINGLE_BY_BLOCK 3
#define PTYPE_FORTRAN         5
#define PTYPE_CUDA            6

#if PTYPE == PTYPE_SINGLE

#define particle_PTYPE_real_t particle_single_real_t
#define particle_PTYPE_t particle_single_t

#define psc_mparticles_PTYPE_patch psc_mparticles_single_patch
#define psc_mparticles_PTYPE psc_mparticles_single
#define psc_mparticles_PTYPE_ops psc_mparticles_single_ops
#define psc_particle_PTYPE_iter_t psc_particle_single_iter_t
#define psc_particle_PTYPE_range_t psc_particle_single_range_t

#elif PTYPE == PTYPE_DOUBLE

#define particle_PTYPE_real_t particle_double_real_t
#define particle_PTYPE_t particle_double_t

#define psc_mparticles_PTYPE_patch psc_mparticles_double_patch
#define psc_mparticles_PTYPE psc_mparticles_double
#define psc_mparticles_PTYPE_ops psc_mparticles_double_ops
#define psc_particle_PTYPE_iter_t psc_particle_double_iter_t
#define psc_particle_PTYPE_range_t psc_particle_double_range_t 

#elif PTYPE == PTYPE_SINGLE_BY_BLOCK

#define particle_PTYPE_real_t particle_single_by_block_real_t
#define particle_PTYPE_t particle_single_by_block_t

#define psc_mparticles_PTYPE_patch psc_mparticles_single_by_block_patch
#define psc_mparticles_PTYPE psc_mparticles_single_by_block
#define psc_mparticles_PTYPE_ops psc_mparticles_single_by_block_ops
#define psc_particle_PTYPE_iter_t psc_particle_single_by_block_iter_t
#define psc_particle_PTYPE_range_t psc_particle_single_by_block_range_t 

#elif PTYPE == PTYPE_FORTRAN

#define particle_PTYPE_real_t particle_fortran_real_t
#define particle_PTYPE_t particle_fortran_t

#define psc_mparticles_PTYPE_patch psc_mparticles_fortran_patch
#define psc_mparticles_PTYPE psc_mparticles_fortran
#define psc_mparticles_PTYPE_ops psc_mparticles_fortran_ops
#define psc_particle_PTYPE_iter_t psc_particle_fortran_iter_t
#define psc_particle_PTYPE_range_t psc_particle_fortran_range_t 

#endif

// ======================================================================

struct psc_mparticles_PTYPE_patch;

using psc_particle_PTYPE_range_t = psc_mparticles_PTYPE_patch&;

// ----------------------------------------------------------------------
// psc_mparticles_PTYPE_patch

struct psc_mparticles_PTYPE_patch
{
  using particle_t = particle_PTYPE_t;
  using buf_t = std::vector<particle_t>;
  using iterator = typename buf_t::iterator;
  
  buf_t buf;

  int b_mx[3];
  particle_PTYPE_real_t b_dxi[3];

  struct psc_mparticles *mprts;
  int p;
  
#if PTYPE == PTYPE_SINGLE
  particle_PTYPE_t *prt_array_alt;
  int nr_blocks;
  unsigned int *b_idx;
  unsigned int *b_ids;
  unsigned int *b_cnt;
  bool need_reorder;
#endif
  
#if PTYPE == PTYPE_SINGLE_BY_BLOCK
  particle_PTYPE_t *prt_array_alt;
  int nr_blocks;
  unsigned int *b_idx;
  unsigned int *b_ids;
  unsigned int *b_cnt;
  unsigned int *b_off;
  bool need_reorder;
#endif

  ~psc_mparticles_PTYPE_patch();

  particle_PTYPE_t& operator[](int n) { return buf[n]; }
  iterator begin() { return buf.begin(); }
  iterator end() { return buf.end(); }
  unsigned int size() const { return buf.size(); }

  void resize(unsigned int new_size)
  {
    assert(new_size <= buf.capacity());
    buf.resize(new_size);
  }

  void reserve(unsigned int new_capacity)
  {
    unsigned int old_capacity = buf.capacity();
    buf.reserve(new_capacity);
    new_capacity = buf.capacity();
    
    if (new_capacity == old_capacity) {
      return;
    }
    
#if PTYPE == PTYPE_SINGLE
    free(prt_array_alt);
    prt_array_alt = (particle_PTYPE_t *) malloc(new_capacity * sizeof(*prt_array_alt));
    b_idx = (unsigned int *) realloc(b_idx, new_capacity * sizeof(*b_idx));
    b_ids = (unsigned int *) realloc(b_ids, new_capacity * sizeof(*b_ids));
#endif
    
#if PTYPE == PTYPE_SINGLE_BY_BLOCK
    free(prt_array_alt);
    prt_array_alt = (particle_PTYPE_t *) malloc(new_capacity * sizeof(*prt_array_alt));
    b_idx = (unsigned int *) realloc(b_idx, new_capacity * sizeof(*b_idx));
    b_ids = (unsigned int *) realloc(b_ids, new_capacity * sizeof(*b_ids));
#endif
  }

  void push_back(const particle_PTYPE_t& prt)
  {
    buf.push_back(prt);
  }

  buf_t& get_buf()
  {
    return buf;
  }

  psc_particle_PTYPE_range_t range();

  const int* get_b_mx() const
  {
    return b_mx;
  }

  const particle_PTYPE_real_t* get_b_dxi() const
  {
    return b_dxi;
  }
};

// ----------------------------------------------------------------------
// psc_mparticles_PTYPE

struct psc_mparticles_PTYPE
{
  using particles_t = psc_mparticles_PTYPE_patch;
  
  particles_t *patch;
};

// ----------------------------------------------------------------------
// psc_mparticles_PTYPE_patch::range

inline psc_particle_PTYPE_range_t psc_mparticles_PTYPE_patch::range()
{
  return psc_particle_PTYPE_range_t(*this);
}

#include <math.h>

#undef particle_PTYPE_real_t
#undef particle_PTYPE_t

#undef psc_mparticles_PTYPE_patch
#undef psc_mparticles_PTYPE
#undef psc_mparticles_PTYPE_ops
#undef psc_mparticles_PTYPE_patch_capacity
#undef psc_particle_PTYPE_iter_t
#undef psc_particle_PTYPE_range_t 

