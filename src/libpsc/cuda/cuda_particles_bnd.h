
#ifndef CUDA_PARTICLES_BND_H
#define CUDA_PARTICLES_BND_H

#include "psc_particles_cuda.h"
#include "ddc_particles.hxx"

struct cuda_particles_bnd
{
  using mparticles_t = mparticles_cuda_t;
  using ddcp_t = ddc_particles<mparticles_t>;
  using ddcp_patch = typename ddcp_t::patch;

  void prep(ddcp_t* ddcp, cuda_mparticles* cmprts);
  void post(ddcp_t* ddcp, cuda_mparticles* cmprts);

  // pieces for prep
  void spine_reduce(cuda_mparticles *cmprts);
  void find_n_send(cuda_mparticles *cmprts);
  void scan_send_buf_total(cuda_mparticles *cmprts);
  void reorder_send_by_id(cuda_mparticles *cmprts);
  void copy_from_dev_and_convert(cuda_mparticles *cmprts);

  void spine_reduce_gold(cuda_mparticles *cmprts);
  void scan_send_buf_total_gold(cuda_mparticles *cmprts);
  void reorder_send_by_id_gold(cuda_mparticles *cmprts);
};

#endif

