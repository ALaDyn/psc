
#define PARTICLE_ITER_LOOP(prt_iter, prt_begin, prt_end)	      \
  for (particle_iter_t prt_iter = prt_begin;			      \
       !particle_iter_equal(prt_iter, prt_end);			      \
       prt_iter = particle_iter_next(prt_iter))


// FIXME, abuse of this file...

// ----------------------------------------------------------------------
// find_block_position

static inline void
particle_xi_get_block_pos(particle_real_t xi[3], particle_real_t b_dxi[3], int b_pos[3])
{
  for (int d = 0; d < 3; d++) {
    b_pos[d] = particle_real_fint(xi[d] * b_dxi[d]);
  }
}

