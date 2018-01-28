
#include "cuda_particles_bnd_iface.h"
#include "cuda_mparticles.h"

// ----------------------------------------------------------------------
// setup

void cuda_particles_bnd::setup(ddcp_t* ddcp, cuda_mparticles* cmprts)
{
  cmprts->d_spine_cnts.resize(1 + cmprts->n_blocks * (CUDA_BND_STRIDE + 1));
  cmprts->d_spine_sums.resize(1 + cmprts->n_blocks * (CUDA_BND_STRIDE + 1));

  cmprts->bpatch = new cuda_bnd[cmprts->n_patches];
}

// ----------------------------------------------------------------------
// dtor

cuda_particles_bnd::~cuda_particles_bnd()
{
  //delete[] bpatch; FIXME, can be done when bpatch is moved into this class
}

// ----------------------------------------------------------------------
// prep

void cuda_particles_bnd::prep(ddcp_t* ddcp, cuda_mparticles* cmprts)
{
  static int pr_A, pr_B, pr_D, pr_B0, pr_B1;
  if (!pr_A) {
    pr_A = prof_register("xchg_bidx", 1., 0, 0);
    pr_B0= prof_register("xchg_reduce", 1., 0, 0);
    pr_B1= prof_register("xchg_n_send", 1., 0, 0);
    pr_B = prof_register("xchg_scan_send", 1., 0, 0);
    pr_D = prof_register("xchg_from_dev", 1., 0, 0);
  }

  //prof_start(pr_A);
  //cuda_mprts_find_block_keys(mprts);
  //prof_stop(pr_A);
  
  prof_start(pr_B0);
  spine_reduce(cmprts);
  prof_stop(pr_B0);

  prof_start(pr_B1);
  find_n_send(cmprts);
  prof_stop(pr_B1);

  prof_start(pr_B);
  scan_send_buf_total(cmprts);
  prof_stop(pr_B);

  prof_start(pr_D);
  copy_from_dev_and_convert(cmprts);
  prof_stop(pr_D);

  if (!ddcp) return; // FIXME testing hack
  for (int p = 0; p < ddcp->nr_patches; p++) {
    ddcp_patch *dpatch = &ddcp->patches[p];
    dpatch->m_buf = &cmprts->bpatch[p].buf;
    dpatch->m_begin = 0;
  }
}

// ----------------------------------------------------------------------
// post

void cuda_particles_bnd::post(ddcp_t* ddcp, cuda_mparticles* cmprts)
{
  static int pr_A, pr_D, pr_E, pr_D1;
  if (!pr_A) {
    pr_A = prof_register("xchg_to_dev", 1., 0, 0);
    pr_D = prof_register("xchg_sort", 1., 0, 0);
    pr_D1= prof_register("xchg_upd_off", 1., 0, 0);
    pr_E = prof_register("xchg_reorder", 1., 0, 0);
  }

  prof_start(pr_A);
  convert_and_copy_to_dev(cmprts);
  prof_stop(pr_A);

  prof_start(pr_D);
  sort_pairs_device(cmprts);
  cmprts->n_prts -= cmprts->n_prts_send;
  prof_stop(pr_D);

  prof_start(pr_D1);
  update_offsets(cmprts);
  prof_stop(pr_D1);
  
  prof_start(pr_E);
#if 0
  cmprts->reorder(cmprts);
  assert(cmprts->check_ordered());
#else
  cmprts->need_reorder = true;
#endif
  prof_stop(pr_E);

  if (!ddcp) return; // FIXME, testing hack
  for (int p = 0; p < ddcp->nr_patches; p++) {
    ddcp->patches[p].m_buf = NULL;
  }
}


