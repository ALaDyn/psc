
#include "psc.h"
#include "psc_particles_cuda2.h"
#include "psc_cuda2.h"

// for conversions
#include "psc_particles_single.h"
#include "psc_particles_cuda.h"
#include "../cuda/psc_cuda.h"

#include <stdlib.h>

// ======================================================================
// psc_particles "cuda2"

// ----------------------------------------------------------------------
// psc_mparticles_cuda2_copy_to_device

void
psc_mparticles_cuda2_copy_to_device(struct psc_mparticles *mprts)
{
#if 0
  struct psc_mparticles_cuda2 *sub = psc_mparticles_cuda2(mflds);

  cuda_memcpy_device_from_host(sub->d_xi4, sub->h_xi4, mflds->n_parts * sizeof(*sub->d_xi4));
#endif
}

// ----------------------------------------------------------------------
// psc_mparticles_cuda2_copy_to_host

void
psc_mparticles_cuda2_copy_to_host(struct psc_mparticles *mprts)
{
}

// ----------------------------------------------------------------------
// find_idx_off_1st_rel
//
// FIXME, duplicated and only need fixed shift, no og here

static inline void
find_idx_off_1st_rel(particle_cuda2_real_t xi[3], int lg[3], particle_cuda2_real_t og[3],
		     particle_cuda2_real_t shift, particle_cuda2_real_t dxi[3])
{
  for (int d = 0; d < 3; d++) {
    particle_cuda2_real_t pos = xi[d] * dxi[d] + shift;
    lg[d] = particle_cuda2_real_fint(pos);
    og[d] = pos - lg[d];
  }
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_get_b_idx

static unsigned int
psc_particles_cuda2_get_b_idx(struct psc_particles *prts, int n)
{
  // FIXME, a particle which ends up exactly on the high boundary
  // should not be considered oob -- well, it's a complicated business,
  // but at least for certain b.c.s it's legal
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts);

  particle_cuda2_t prt;
  _LOAD_PARTICLE_POS(prt, sub->h_xi4, n);
  _LOAD_PARTICLE_MOM(prt, sub->h_pxi4, n);
  particle_cuda2_real_t of[3];
  int b_pos[3], *b_mx = sub->b_mx;
  find_idx_off_1st_rel(&prt.xi4.x, b_pos, of, 0.f, sub->dxi);
  for (int d = 0; d < 3; d++) {
    b_pos[d] /= psc_particles_cuda2_bs[d];
  }

  if (b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
      b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
      b_pos[2] >= 0 && b_pos[2] < b_mx[2]) {
    unsigned int b_idx = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
    assert(b_idx < sub->nr_blocks);
    return b_idx;
  } else { // out of bounds
    return sub->nr_blocks;
  }
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_sort
//
// This function will take an unsorted list of particles and
// sort it by block index, and set up the b_off array that
// describes the particle index range for each block.
//
// b_idx, b_ids and b_cnt are not valid after returning
// (they shouldn't be needed for anything, either)

static void
psc_particles_cuda2_sort(struct psc_particles *prts)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts);

  for (int b = 0; b < sub->nr_blocks; b++) {
    sub->b_cnt[b] = 0;
  }

  // calculate block indices for each particle and count
  for (int n = 0; n < prts->n_part; n++) {
    unsigned int b_idx = psc_particles_cuda2_get_b_idx(prts, n);
    assert(b_idx < sub->nr_blocks);
    sub->b_idx[n] = b_idx;
    sub->b_cnt[b_idx]++;
  }
  
  // b_off = prefix sum(b_cnt), zero b_cnt
  int sum = 0;
  for (int b = 0; b <= sub->nr_blocks; b++) {
    sub->b_off[b] = sum;
    sum += sub->b_cnt[b];
    // temporarily abuse b_cnt for next step
    // (the array will be altered, so we use b_cnt rather than
    // b_off, which we'd like to preserve)
    sub->b_cnt[b] = sub->b_off[b];
  }
  sub->b_off[sub->nr_blocks + 1] = sum;

  // find target position for each particle
  for (int n = 0; n < prts->n_part; n++) {
    unsigned int b_idx = sub->b_idx[n];
    sub->b_ids[n] = sub->b_cnt[b_idx]++;
  }

  // reorder into alt particle array
  // WARNING: This is reversed to what reorder() does!
  for (int n = 0; n < prts->n_part; n++) {
    sub->h_xi4_alt [sub->b_ids[n]] = sub->h_xi4 [n];
    sub->h_pxi4_alt[sub->b_ids[n]] = sub->h_pxi4[n];
  }
  
  // swap in alt array
  float4 *tmp_xi4 = sub->h_xi4;
  float4 *tmp_pxi4 = sub->h_pxi4;
  sub->h_xi4 = sub->h_xi4_alt;
  sub->h_pxi4 = sub->h_pxi4_alt;
  sub->h_xi4_alt = tmp_xi4;
  sub->h_pxi4_alt = tmp_pxi4;
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_check
//
// make sure that all particles are within the local patch (ie.,
// they have a valid block idx, and that they are sorted by
// block_idx, and that that b_off properly contains the range of
// particles in each block.

static void
psc_particles_cuda2_check(struct psc_particles *prts)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts);

  assert(prts->n_part <= sub->n_alloced);

  int block = 0;
  for (int n = 0; n < prts->n_part; n++) {
    while (n >= sub->b_off[block + 1]) {
      block++;
      assert(block < sub->nr_blocks);
    }
    assert(n >= sub->b_off[block] && n < sub->b_off[block + 1]);
    assert(block < sub->nr_blocks);
    unsigned int b_idx = psc_particles_cuda2_get_b_idx(prts, n);
    assert(b_idx < sub->nr_blocks);
    assert(b_idx == block);
  }
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_copy_to_single

static void
psc_particles_cuda2_copy_to_single(struct psc_particles *prts_base,
				   struct psc_particles *prts, unsigned int flags)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts_base);

  prts->n_part = prts_base->n_part;
  assert(prts->n_part <= psc_particles_single(prts)->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_cuda2_t prt_base;
    _LOAD_PARTICLE_POS(prt_base, sub->h_xi4, n);
    _LOAD_PARTICLE_MOM(prt_base, sub->h_pxi4, n);
    particle_single_t *part = particles_single_get_one(prts, n);
    
    part->xi      = prt_base.xi4.x;
    part->yi      = prt_base.xi4.y;
    part->zi      = prt_base.xi4.z;
    part->kind    = cuda_float_as_int(prt_base.xi4.w);
    part->pxi     = prt_base.pxi4.x;
    part->pyi     = prt_base.pxi4.y;
    part->pzi     = prt_base.pxi4.z;
    part->qni_wni = prt_base.pxi4.w;
  }
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_copy_from_single

static void
psc_particles_cuda2_copy_from_single(struct psc_particles *prts_base,
				     struct psc_particles *prts, unsigned int flags)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts_base);

  prts_base->n_part = prts->n_part;
  assert(prts_base->n_part <= sub->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_cuda2_t prt_base;
    particle_single_t *part = particles_single_get_one(prts, n);

    prt_base.xi4.x   = part->xi;
    prt_base.xi4.y   = part->yi;
    prt_base.xi4.z   = part->zi;
    prt_base.xi4.w   = cuda_int_as_float(part->kind);
    prt_base.pxi4.x  = part->pxi;
    prt_base.pxi4.y  = part->pyi;
    prt_base.pxi4.z  = part->pzi;
    prt_base.pxi4.w  = part->qni_wni;

    _STORE_PARTICLE_POS(prt_base, sub->h_xi4, n);
    _STORE_PARTICLE_MOM(prt_base, sub->h_pxi4, n);
  }

  psc_particles_cuda2_sort(prts_base);
  // make sure there are no oob particles
  assert(sub->b_off[sub->nr_blocks] == sub->b_off[sub->nr_blocks+1]);
  psc_particles_cuda2_check(prts_base);
}

// ----------------------------------------------------------------------
// psc_particles_cuda2_copy_to_cuda

static void
psc_particles_cuda2_copy_to_cuda(struct psc_particles *prts,
				 struct psc_particles *prts_cuda, unsigned int flags)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts);

  assert(prts_cuda->n_part == prts->n_part);
  
  float4 *xi4  = calloc(prts->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts->n_part, sizeof(float4));
  
  for (int n = 0; n < prts->n_part; n++) {
    particle_cuda2_t prt;
    _LOAD_PARTICLE_POS(prt, sub->h_xi4, n);
    _LOAD_PARTICLE_MOM(prt, sub->h_pxi4, n);
    
    xi4[n].x  = prt.xi4.x;
    xi4[n].y  = prt.xi4.y;
    xi4[n].z  = prt.xi4.z;
    xi4[n].w  = prt.xi4.w;
    pxi4[n].x = prt.pxi4.x;
    pxi4[n].y = prt.pxi4.y;
    pxi4[n].z = prt.pxi4.z;
    pxi4[n].w = prt.pxi4.w;
  }
  
  __particles_cuda_to_device(prts_cuda, xi4, pxi4);
  
  free(xi4);
  free(pxi4);
}

static void
psc_particles_cuda2_copy_from_cuda(struct psc_particles *prts,
				   struct psc_particles *prts_cuda, unsigned int flags)
{
  struct psc_particles_cuda2 *sub = psc_particles_cuda2(prts);

  prts->n_part = prts_cuda->n_part;
  assert(prts->n_part <= sub->n_alloced);
  
  float4 *xi4  = calloc(prts_cuda->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts_cuda->n_part, sizeof(float4));
  
  __particles_cuda_from_device(prts_cuda, xi4, pxi4);
  
  for (int n = 0; n < prts->n_part; n++) {
    particle_cuda2_t prt;

    prt.xi4.x   = xi4[n].x;
    prt.xi4.y   = xi4[n].y;
    prt.xi4.z   = xi4[n].z;
    prt.xi4.w   = xi4[n].w;
    prt.pxi4.x  = pxi4[n].x;
    prt.pxi4.y  = pxi4[n].y;
    prt.pxi4.z  = pxi4[n].z;
    prt.pxi4.w  = pxi4[n].w;

    _STORE_PARTICLE_POS(prt, sub->h_xi4, n);
    _STORE_PARTICLE_MOM(prt, sub->h_pxi4, n);
  }

  free(xi4);
  free(pxi4);
}

// ----------------------------------------------------------------------
// psc_particles: subclass "cuda2"

static struct mrc_obj_method psc_particles_cuda2_methods[] = {
  MRC_OBJ_METHOD("copy_to_single"  , psc_particles_cuda2_copy_to_single),
  MRC_OBJ_METHOD("copy_from_single", psc_particles_cuda2_copy_from_single),
  MRC_OBJ_METHOD("copy_to_cuda"    , psc_particles_cuda2_copy_to_cuda),
  MRC_OBJ_METHOD("copy_from_cuda"  , psc_particles_cuda2_copy_from_cuda),
  {}
};

struct psc_particles_ops psc_particles_cuda2_ops = {
  .name                    = "cuda2",
  .size                    = sizeof(struct psc_particles_cuda2),
  .methods                 = psc_particles_cuda2_methods,
#if 0
#ifdef HAVE_LIBHDF5_HL
  .read                    = psc_particles_cuda2_read,
  .write                   = psc_particles_cuda2_write,
#endif
  .reorder                 = psc_particles_cuda2_reorder,
#endif
};

// ======================================================================
// psc_mparticles: subclass "cuda2"
  
// ----------------------------------------------------------------------
// psc_mparticles_cuda2_setup

static void
psc_mparticles_cuda2_setup(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda2 *sub = psc_mparticles_cuda2(mprts);

  psc_mparticles_setup_super(mprts);

  if (mprts->nr_patches == 0) {
    return;
  }
  
  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda2 *prts_sub = psc_particles_cuda2(prts);

    prts_sub->n_alloced = prts->n_part * 1.2;

    for (int d = 0; d < 3; d++) {
      prts_sub->dxi[d] = 1.f / ppsc->patch[prts->p].dx[d];
      assert(ppsc->patch[prts->p].ldims[d] % psc_particles_cuda2_bs[d] == 0);
      prts_sub->b_mx[d] = ppsc->patch[prts->p].ldims[d] / psc_particles_cuda2_bs[d];
    }
    prts_sub->nr_blocks = prts_sub->b_mx[0] * prts_sub->b_mx[1] * prts_sub->b_mx[2];

    // on host
    prts_sub->h_xi4 = calloc(prts_sub->n_alloced, sizeof(*prts_sub->h_xi4));
    prts_sub->h_pxi4 = calloc(prts_sub->n_alloced, sizeof(*prts_sub->h_pxi4));
    prts_sub->h_xi4_alt = calloc(prts_sub->n_alloced, sizeof(*prts_sub->h_xi4_alt));
    prts_sub->h_pxi4_alt = calloc(prts_sub->n_alloced, sizeof(*prts_sub->h_pxi4_alt));
    prts_sub->b_idx = calloc(prts_sub->n_alloced, sizeof(*prts_sub->b_idx));
    prts_sub->b_ids = calloc(prts_sub->n_alloced, sizeof(*prts_sub->b_ids));
    prts_sub->b_cnt = calloc(prts_sub->nr_blocks + 1, sizeof(*prts_sub->b_cnt));
    prts_sub->b_off = calloc(prts_sub->nr_blocks + 2, sizeof(*prts_sub->b_off));
  
    // on device
    prts_sub->d_xi4 = cuda_calloc(prts_sub->n_alloced, sizeof(*prts_sub->d_xi4));
    prts_sub->d_pxi4 = cuda_calloc(prts_sub->n_alloced, sizeof(*prts_sub->d_pxi4));
    prts_sub->d_b_off = cuda_calloc(prts_sub->nr_blocks + 2, sizeof(*prts_sub->b_off));
  }
}

// ----------------------------------------------------------------------
// psc_mparticles_cuda2_destroy

static void
psc_mparticles_cuda2_destroy(struct psc_mparticles *mprts)
{
  struct psc_mparticles_cuda2 *sub = psc_mparticles_cuda2(mprts);

  for (int p = 0; p < mprts->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_particles_cuda2 *prts_sub = psc_particles_cuda2(prts);

    free(prts_sub->h_xi4);
    free(prts_sub->h_pxi4);
    free(prts_sub->h_xi4_alt);
    free(prts_sub->h_pxi4_alt);
    free(prts_sub->b_idx);
    free(prts_sub->b_ids);
    free(prts_sub->b_cnt);
    free(prts_sub->b_off);
    
    cuda_free(prts_sub->d_xi4);
    cuda_free(prts_sub->d_pxi4);
    cuda_free(prts_sub->d_b_off);
  }
}

// ----------------------------------------------------------------------
// psc_mparticles: subclass "cuda2"

struct psc_mparticles_ops psc_mparticles_cuda2_ops = {
  .name                    = "cuda2",
  .size                    = sizeof(struct psc_mparticles_cuda2),
  .setup                   = psc_mparticles_cuda2_setup,
  .destroy                 = psc_mparticles_cuda2_destroy,
};

