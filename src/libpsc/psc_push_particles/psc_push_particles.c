
#include "psc_push_particles_private.h"

#include "psc_push_fields_private.h"
#include "psc_bnd_fields.h"
#include "psc_bnd.h"

#include <mrc_profile.h>

// ======================================================================
// forward to subclass

extern int pr_time_step_no_comm;
extern double *psc_balance_comp_time_by_patch;

static void
push_a_yz(struct psc_push_particles *push, struct psc_mparticles *mprts,
	  struct psc_mfields *mflds)
{
  struct psc_push_particles_ops *ops = psc_push_particles_ops(push);

  prof_restart(pr_time_step_no_comm);
  if (ops->push_a_yz) {
#pragma omp parallel for
    for (int p = 0; p < mprts->nr_patches; p++) {
      psc_balance_comp_time_by_patch[p] -= MPI_Wtime();
      ops->push_a_yz(push, psc_mparticles_get_patch(mprts, p),
		     psc_mfields_get_patch(mflds, p));
      psc_balance_comp_time_by_patch[p] += MPI_Wtime();
    }
  } else {
    assert(ops->push_mprts_yz);
    ops->push_mprts_yz(push, mprts, mflds);
  }
  prof_stop(pr_time_step_no_comm);
}

void
psc_push_particles_run(struct psc_push_particles *push,
		       mparticles_base_t *mprts, mfields_base_t *mflds)
{
  static int pr;
  if (!pr) {
    pr = prof_register("push_particles_run", 1., 0, 0);
  }  

  prof_start(pr);
  psc_stats_start(st_time_particle);
  struct psc_push_particles_ops *ops = psc_push_particles_ops(push);
  int *im = ppsc->domain.gdims;

  if (im[0] == 1 && im[1] > 1 && im[2] > 1) { // yz
    push_a_yz(push, mprts, mflds);
  } else {
#pragma omp parallel for
    for (int p = 0; p < mprts->nr_patches; p++) {
      struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
      struct psc_fields *flds = psc_mfields_get_patch(mflds, p);
      if (im[0] > 1 && im[1] > 1 && im[2] > 1) { // xyz
	assert(ops->push_a_xyz);
	ops->push_a_xyz(push, prts, flds);
      } else if (im[0] > 1 && im[2] > 1) { // xz
	assert(ops->push_a_xz);
	ops->push_a_xz(push, prts, flds);
      } else if (im[0] > 1 && im[1] > 1) { // xy
	assert(ops->push_a_xy);
	ops->push_a_xy(push, prts, flds);
      } else if (im[2] > 1) { // z
	assert(ops->push_a_z);
	ops->push_a_z(push, prts, flds);
      } else if (im[1] > 1) { // y
	assert(ops->push_a_y);
	ops->push_a_y(push, prts, flds);
      } else {
	assert(0);
      }
    }
  }

  psc_stats_stop(st_time_particle);
  prof_stop(pr);
}

void
psc_push_particles_run_b(struct psc_push_particles *push,
			 mparticles_base_t *particles, mfields_base_t *mflds)
{
  psc_stats_start(st_time_particle);
  struct psc_push_particles_ops *ops = psc_push_particles_ops(push);
  int *im = ppsc->domain.gdims;
#pragma omp parallel for
  for (int p = 0; p < particles->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(particles, p);
    struct psc_fields *flds = psc_mfields_get_patch(mflds, p);
    if (im[0] > 1 && im[1] > 1 && im[2] > 1) { // xyz
    } else if (im[0] > 1 && im[2] > 1) { // xz
    } else if (im[0] > 1 && im[1] > 1) { // xy
    } else if (im[1] > 1 && im[2] > 1) { // yz
      if (ops->push_b_yz) {
	ops->push_b_yz(push, prts, flds);
      }
    } else if (im[2] > 1) { // z
    } else {
    }
  }
  psc_stats_stop(st_time_particle);
}

unsigned int
psc_push_particles_get_mp_flags(struct psc_push_particles *push)
{
  struct psc_push_particles_ops *ops = psc_push_particles_ops(push);
  return ops->mp_flags;
}

// ======================================================================
// psc_push_particles_init

static void
psc_push_particles_init()
{
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_generic_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_2nd_double_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1p5_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1st_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1sff_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_double_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb2_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec_double_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_double_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_single_by_block_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_fortran_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_vay_ops);
#ifdef USE_SSE2
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_ps_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_ps2_ops);
#endif
#ifdef USE_CBE
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_cbe_ops);
#endif
#ifdef USE_CUDA
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_4x4_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_2x2_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_4x4_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_8x8_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_2x2_gmem_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_4x4_gmem_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vbec3d_8x8_gmem_cuda_ops);
#ifdef USE_SSE2
  mrc_class_register_subclass(&mrc_class_psc_push_particles, &psc_push_particles_1vb_mix_ops);
#endif
#endif
}

// ======================================================================
// psc_push_particles class

struct mrc_class_psc_push_particles mrc_class_psc_push_particles = {
  .name             = "psc_push_particles",
  .size             = sizeof(struct psc_push_particles),
  .init             = psc_push_particles_init,
};

