
#include "psc_particle_common.h"

#include <stdlib.h>
#include <assert.h>
#include <mrc_bits.h>
#include <algorithm>

#if PTYPE == PTYPE_SINGLE

#define particle_PTYPE_real_t particle_single_real_t
#define particle_PTYPE_t particle_single_t

#define psc_particle_PTYPE_buf_t psc_particle_single_buf_t
#define psc_particle_PTYPE_buf_dtor psc_particle_single_buf_dtor
#define psc_particle_PTYPE_range_t psc_particle_single_range_t

#elif PTYPE == PTYPE_DOUBLE

#define particle_PTYPE_real_t particle_double_real_t
#define particle_PTYPE_t particle_double_t

#define psc_particle_PTYPE_buf_t psc_particle_double_buf_t
#define psc_particle_PTYPE_buf_dtor psc_particle_double_buf_dtor
#define psc_particle_PTYPE_range_t psc_particle_double_range_t

#elif PTYPE == PTYPE_SINGLE_BY_BLOCK

#define particle_PTYPE_real_t particle_single_by_block_real_t
#define particle_PTYPE_t particle_single_by_block_t

#define psc_particle_PTYPE_buf_t psc_particle_single_by_block_buf_t
#define psc_particle_PTYPE_buf_dtor psc_particle_single_by_block_buf_dtor
#define psc_particle_PTYPE_range_t psc_particle_single_by_block_range_t

#elif PTYPE == PTYPE_FORTRAN

#define particle_PTYPE_real_t particle_fortran_real_t
#define particle_PTYPE_t particle_fortran_t

#define psc_particle_PTYPE_buf_t psc_particle_fortran_buf_t
#define psc_particle_PTYPE_buf_dtor psc_particle_fortran_buf_dtor

#elif PTYPE == PTYPE_CUDA

#define particle_PTYPE_real_t particle_cuda_real_t
#define particle_PTYPE_t particle_cuda_t

#define psc_particle_PTYPE_buf_t psc_particle_cuda_buf_t
#define psc_particle_PTYPE_buf_dtor psc_particle_cuda_buf_dtor

#endif

// ======================================================================
// psc_particle_PTYPE_buf_t

struct psc_particle_PTYPE_buf_t
{
  using particle_t = particle_PTYPE_t;

  psc_particle_PTYPE_buf_t()
    : m_data(), m_size(), m_capacity()
  {
  }

  psc_particle_PTYPE_buf_t(const psc_particle_PTYPE_buf_t&) = delete;
  
  void resize(unsigned int new_size)
  {
    assert(new_size <= m_capacity);
    m_size = new_size;
  }

  void reserve(unsigned int new_capacity)
  {
    if (new_capacity <= m_capacity)
      return;

    new_capacity = std::max(new_capacity, m_capacity * 2);
    
    m_data = (particle_PTYPE_t *) realloc(m_data, new_capacity * sizeof(*m_data));
    m_capacity = new_capacity;
  }

  void push_back(const particle_PTYPE_t& prt)
  {
    unsigned int n = m_size;
    if (n >= m_capacity) {
      reserve(n + 1);
    }
    m_data[n++] = prt;
    m_size = n;
  }

  particle_PTYPE_t& operator[](int n)
  {
    return m_data[n];
  }

  particle_PTYPE_t *m_data;
  unsigned int m_size;
  unsigned int m_capacity;

  unsigned int size() const { return m_size; }
  unsigned int capacity() const { return m_capacity; }

};

// ----------------------------------------------------------------------
// psc_particle_PTYPE_buf_dtor

static inline void
psc_particle_PTYPE_buf_dtor(psc_particle_PTYPE_buf_t *buf)
{
  free(buf->m_data);
}

#undef particle_PTYPE_real_t
#undef particle_PTYPE_t

#undef psc_particle_PTYPE_buf_t
#undef psc_particle_PTYPE_buf_dtor
#undef psc_particle_PTYPE_range_t

