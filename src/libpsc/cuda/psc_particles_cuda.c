
#include "psc.h"
#include "psc_cuda.h"
#include "psc_bnd_cuda.h"
#include "psc_particles_cuda.h"
#include "psc_particles_single.h"
#include "psc_push_particles.h"

EXTERN_C void cuda_init(int rank);

// ======================================================================
// psc_particles "cuda"

static void
psc_particles_cuda_setup(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  if (prts->p == 0) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    cuda_init(rank);
  }

  struct psc_patch *patch = &ppsc->patch[prts->p];

  if (!prts->flags) {
    // FIXME, they get set to early, so auto-dispatch "1vb" doesn't work
    prts->flags = MP_NEED_BLOCK_OFFSETS | MP_BLOCKSIZE_4X4X4 | MP_NO_CHECKERBOARD;
  }

  int bs[3];
  for (int d = 0; d < 3; d++) {
    switch (prts->flags & MP_BLOCKSIZE_MASK) {
    case MP_BLOCKSIZE_1X1X1: bs[d] = 1; break;
    case MP_BLOCKSIZE_2X2X2: bs[d] = 2; break;
    case MP_BLOCKSIZE_4X4X4: bs[d] = 4; break;
    case MP_BLOCKSIZE_8X8X8: bs[d] = 8; break;
    default: assert(0);
    }
    if (ppsc->domain.gdims[d] == 1) {
      bs[d] = 1;
    }
    cuda->blocksize[d] = bs[d];
    assert(patch->ldims[d] % bs[d] == 0); // not sure what breaks if not
    cuda->b_mx[d] = (patch->ldims[d] + bs[d] - 1) / bs[d];
    cuda->b_dxi[d] = 1.f / (cuda->blocksize[d] * ppsc->dx[d]);
  }
  cuda->nr_blocks = cuda->b_mx[0] * cuda->b_mx[1] * cuda->b_mx[2];

  for (int d = 0; d < 3; d++) {
    if (prts->flags & MP_NO_CHECKERBOARD) {
      bs[d] = 1;
    } else {
      bs[d] = (patch->ldims[d] == 1) ? 1 : 2;
    }
  }
  cell_map_init(&cuda->map, cuda->b_mx, bs);

  __particles_cuda_alloc(prts, true, true); // FIXME, need separate flags

  cuda_alloc_block_indices(prts, &cuda->d_part.bidx); // FIXME, merge into ^^^
  cuda_alloc_block_indices(prts, &cuda->d_part.ids);
  cuda_alloc_block_indices(prts, &cuda->d_part.alt_bidx);
  cuda_alloc_block_indices(prts, &cuda->d_part.alt_ids);
  cuda_alloc_block_indices(prts, &cuda->d_part.sums);

  cuda->d_part.sort_ctx = sort_pairs_create(cuda->b_mx);
}

static void
psc_particles_cuda_destroy(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  cuda_free_block_indices(cuda->d_part.bidx);
  cuda_free_block_indices(cuda->d_part.ids);
  cuda_free_block_indices(cuda->d_part.alt_bidx);
  cuda_free_block_indices(cuda->d_part.alt_ids);
  cuda_free_block_indices(cuda->d_part.sums);
  sort_pairs_destroy(cuda->d_part.sort_ctx);
  __particles_cuda_free(prts);
  cell_map_free(&cuda->map);
}

// FIXME, should go away and always be done within cuda for consistency

static inline int
find_cellIdx(struct psc_patch *patch, struct cell_map *map,
	     struct psc_particles *pp, int n)
{
  particle_c_t *p = particles_c_get_one(pp, n);
  particle_c_real_t dxi = 1.f / ppsc->dx[0];
  particle_c_real_t dyi = 1.f / ppsc->dx[1];
  particle_c_real_t dzi = 1.f / ppsc->dx[2];
  particle_c_real_t xi[3] = { p->xi * dxi, p->yi * dyi, p->zi * dzi };
  int pos[3];
  for (int d = 0; d < 3; d++) {
    pos[d] = cuda_fint(xi[d]);
  }
  
  return cell_map_3to1(map, pos);
}

static inline int
find_blockIdx(struct psc_patch *patch, struct cell_map *map,
	      struct psc_particles *pp, int n, int blocksize[3])
{
  int cell_idx = find_cellIdx(patch, map, pp, n);
  return cell_idx / (blocksize[0] * blocksize[1] * blocksize[2]);
}

static inline void
blockIdx_to_blockCrd(struct psc_patch *patch, struct cell_map *map,
		     int bidx, int bi[3], int blocksize[3])
{
  int cidx = bidx * (blocksize[0] * blocksize[1] * blocksize[2]);
  cell_map_1to3(map, cidx, bi);
  for (int d = 0; d < 3; d++) {
    bi[d] /= blocksize[d];
  }
}

// ======================================================================
// conversion to "c"

static inline void
calc_vxi(particle_c_real_t vxi[3], particle_c_t *part)
{
  particle_c_real_t root =
    1.f / particle_c_real_sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

static void
psc_particles_cuda_copy_from_c(struct psc_particles *prts_cuda,
			       struct psc_particles *prts_c, unsigned int flags)
{
  int p = prts_cuda->p;
  struct psc_patch *patch = &ppsc->patch[p];
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts_cuda);
  prts_cuda->n_part = prts_c->n_part;
  assert(prts_cuda->n_part <= cuda->n_alloced);
  
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }
  
  float4 *xi4  = calloc(prts_c->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts_c->n_part, sizeof(float4));
  
  for (int n = 0; n < prts_c->n_part; n++) {
    particle_c_t *part_c = particles_c_get_one(prts_c, n);
    
    particle_c_real_t vxi[3];
    calc_vxi(vxi, part_c);

    xi4[n].x  = part_c->xi + dth[0] * vxi[0];
    xi4[n].y  = part_c->yi + dth[1] * vxi[1];
    xi4[n].z  = part_c->zi + dth[2] * vxi[2];
    xi4[n].w  = cuda_int_as_float(part_c->kind);
    pxi4[n].x = part_c->pxi;
    pxi4[n].y = part_c->pyi;
    pxi4[n].z = part_c->pzi;
    pxi4[n].w = part_c->qni * part_c->wni;
    
    // FIXME, we should just sort the original particles,
    // and then use bnd_exchange to exchange, update indices, etc.
    // in particular, due to the time shift, particles may really end up
    // out of bounds, which isn't otherwise fixable.
    float xi[3] = { xi4[n].x, xi4[n].y, xi4[n].z };
    for (int d = 0; d < 3; d++) {
      int bi = cuda_fint(xi[d] * cuda->b_dxi[d]);
      if (bi < 0 || bi >= cuda->b_mx[d]) {
	printf("XXX p %d xi %g %g %g\n", p, xi[0], xi[1], xi[2]);
	printf("XXX p %d n %d d %d xi4[n] %g biy %d // %d\n",
	       p, n, d, xi[d], bi, cuda->b_mx[d]);
	if (bi < 0) {
	  xi[d] = 0.f;
	} else {
	  xi[d] *= (1. - 1e-6);
	}
      }
      bi = cuda_fint(xi[d] * cuda->b_dxi[d]);
      assert(bi >= 0 && bi < cuda->b_mx[d]);
    }
    xi4[n].x = xi[0];
    xi4[n].y = xi[1];
    xi4[n].z = xi[2];
  }
  
  int bs[3];
  for (int d = 0; d < 3; d++) {
    bs[d] = cuda->blocksize[d];
    if (bs[d] != 1) {
      bs[d] *= 2; // sort not only within blocks, but also on lowest block
      // bit, so we can do the checkerboard passes
    }
  }
  struct cell_map map;
  cell_map_init(&map, patch->ldims, bs); // FIXME, already have it elsewhere
  
  int *offsets = NULL;
  if (0 && (flags & MP_NEED_BLOCK_OFFSETS)) {
    // FIXME, should go away and can be taken over by c_offsets
    offsets = calloc(cuda->nr_blocks + 1, sizeof(*offsets));
    int last_block = -1;
    for (int n = 0; n <= prts_cuda->n_part; n++) {
      int block;
      if (n < prts_cuda->n_part) {
	block = find_blockIdx(patch, &map, prts_c, n, cuda->blocksize);
      } else {
	block = cuda->nr_blocks;
      }
      assert(last_block <= block);
      while (last_block < block) {
	offsets[last_block+1] = n;
	last_block++;
      }
    }
    
#if 0
    for (int b = 0; b < pp->nr_blocks; b++) {
      int bi[3];
      blockIdx_to_blockCrd(patch, &map, b, bi, pp->blocksize);
      printf("block %d [%d,%d,%d]: %d:%d\n", b, bi[0], bi[1], bi[2],
	     offsets[b], offsets[b+1]);
    }
#endif
  }

  // FIXME, could be computed on the cuda side
  int *c_pos = calloc(map.N * 3, sizeof(*c_pos));
  for (int cidx = 0; cidx < map.N; cidx++) {
    int ci[3];
    cell_map_1to3(&map, cidx, ci);
    c_pos[3*cidx + 0] = ci[0];
    c_pos[3*cidx + 1] = ci[1];
    c_pos[3*cidx + 2] = ci[2];
  }
  
  int *c_offsets = NULL;
  if (0 && (flags & MP_NEED_CELL_OFFSETS)) {
    const int cells_per_block = cuda->blocksize[0] * cuda->blocksize[1] * cuda->blocksize[2];
    c_offsets = calloc(cuda->nr_blocks * cells_per_block + 1, sizeof(*c_offsets));
    int last_block = -1;
    for (int n = 0; n <= prts_cuda->n_part; n++) {
      int block;
      if (n < prts_cuda->n_part) {
	block = find_cellIdx(patch, &map, prts_c, n);
      } else {
	block = map.N;
      }
      assert(block <= cuda->nr_blocks * cells_per_block);
      assert(last_block <= block);
      while (last_block < block) {
	c_offsets[last_block+1] = n;
	last_block++;
      }
    }
  }
  cell_map_free(&map);
  
  __particles_cuda_to_device(prts_cuda, xi4, pxi4, offsets, c_offsets, c_pos);
  
  if (prts_cuda->flags & MP_NEED_BLOCK_OFFSETS) {
    cuda_sort_patch(p, prts_cuda);
  }
  if (prts_cuda->flags & MP_NEED_CELL_OFFSETS) {
    cuda_sort_patch_by_cell(p, prts_cuda);
  }
  // FIXME, sorting twice because we need both would be suboptimal
  if ((prts_cuda->flags & MP_NEED_CELL_OFFSETS) && 
      (prts_cuda->flags & MP_NEED_BLOCK_OFFSETS)) {
    MHERE;
  }
  
  free(offsets);
  free(c_pos);
  free(c_offsets);
  free(xi4);
  free(pxi4);
}

static void
psc_particles_cuda_copy_to_c(struct psc_particles *prts_cuda,
			     struct psc_particles *prts_c, unsigned int flags)
{
  struct psc_particles_c *c = psc_particles_c(prts_c);
  prts_c->n_part = prts_cuda->n_part;
  assert(prts_c->n_part <= c->n_alloced);
  
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }
  
  float4 *xi4  = calloc(prts_cuda->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts_cuda->n_part, sizeof(float4));
  
  __particles_cuda_from_device(prts_cuda, xi4, pxi4);
  
  for (int n = 0; n < prts_c->n_part; n++) {
    particle_c_real_t qni_wni = pxi4[n].w;
    unsigned int kind = cuda_float_as_int(xi4[n].w);
    
    particle_c_t *part_base = particles_c_get_one(prts_c, n);
    part_base->xi  = xi4[n].x;
    part_base->yi  = xi4[n].y;
    part_base->zi  = xi4[n].z;
    part_base->pxi = pxi4[n].x;
    part_base->pyi = pxi4[n].y;
    part_base->pzi = pxi4[n].z;
    part_base->qni = ppsc->kinds[kind].q;
    part_base->mni = ppsc->kinds[kind].m;
    part_base->wni = qni_wni / part_base->qni;
    part_base->kind = kind;

    particle_c_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part_base->xi -= dth[0] * vxi[0];
    part_base->yi -= dth[1] * vxi[1];
    part_base->zi -= dth[2] * vxi[2];
  }

  free(xi4);
  free(pxi4);
}

// ======================================================================
// conversion to "single"

static void
psc_particles_cuda_copy_from_single(struct psc_particles *prts_cuda,
				    struct psc_particles *prts, unsigned int flags)
{
  int p = prts_cuda->p;
  struct psc_patch *patch = &ppsc->patch[p];
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts_cuda);
  prts_cuda->n_part = prts->n_part;
  assert(prts_cuda->n_part <= cuda->n_alloced);
  
  float4 *xi4  = calloc(prts->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts->n_part, sizeof(float4));
  
  for (int n = 0; n < prts->n_part; n++) {
    particle_single_t *part = particles_single_get_one(prts, n);
    
    xi4[n].x  = part->xi;
    xi4[n].y  = part->yi;
    xi4[n].z  = part->zi;
    xi4[n].w  = cuda_int_as_float(part->kind);
    pxi4[n].x = part->pxi;
    pxi4[n].y = part->pyi;
    pxi4[n].z = part->pzi;
    pxi4[n].w = part->qni_wni;
    
    float xi[3] = { xi4[n].x, xi4[n].y, xi4[n].z };
    for (int d = 0; d < 3; d++) {
      int bi = cuda_fint(xi[d] * cuda->b_dxi[d]);
      if (bi < 0 || bi >= cuda->b_mx[d]) {
	printf("XXX p %d xi %g %g %g\n", p, xi[0], xi[1], xi[2]);
	printf("XXX p %d n %d d %d xi4[n] %g biy %d // %d\n",
	       p, n, d, xi[d], bi, cuda->b_mx[d]);
	if (bi < 0) {
	  xi[d] = 0.f;
	} else {
	  xi[d] *= (1. - 1e-6);
	}
      }
      bi = cuda_fint(xi[d] * cuda->b_dxi[d]);
      assert(bi >= 0 && bi < cuda->b_mx[d]);
    }
    xi4[n].x = xi[0];
    xi4[n].y = xi[1];
    xi4[n].z = xi[2];
  }
  
  int bs[3];
  for (int d = 0; d < 3; d++) {
    bs[d] = cuda->blocksize[d];
    if (bs[d] != 1) {
      bs[d] *= 2; // sort not only within blocks, but also on lowest block
      // bit, so we can do the checkerboard passes
    }
  }
  struct cell_map map;
  cell_map_init(&map, patch->ldims, bs); // FIXME, already have it elsewhere
  
  // FIXME, could be computed on the cuda side
  int *c_pos = calloc(map.N * 3, sizeof(*c_pos));
  for (int cidx = 0; cidx < map.N; cidx++) {
    int ci[3];
    cell_map_1to3(&map, cidx, ci);
    c_pos[3*cidx + 0] = ci[0];
    c_pos[3*cidx + 1] = ci[1];
    c_pos[3*cidx + 2] = ci[2];
  }
  
  cell_map_free(&map);
  
  __particles_cuda_to_device(prts_cuda, xi4, pxi4, NULL, NULL, c_pos);
  
  if (prts_cuda->flags & MP_NEED_BLOCK_OFFSETS) {
    cuda_sort_patch(p, prts_cuda);
  }
  if (prts_cuda->flags & MP_NEED_CELL_OFFSETS) {
    cuda_sort_patch_by_cell(p, prts_cuda);
  }
  // FIXME, sorting twice because we need both would be suboptimal
  if ((prts_cuda->flags & MP_NEED_CELL_OFFSETS) && 
      (prts_cuda->flags & MP_NEED_BLOCK_OFFSETS)) {
    MHERE;
  }
  
  free(c_pos);
  free(xi4);
  free(pxi4);
}

static void
psc_particles_cuda_copy_to_single(struct psc_particles *prts_cuda,
				  struct psc_particles *prts, unsigned int flags)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);
  prts->n_part = prts_cuda->n_part;
  assert(prts->n_part <= sngl->n_alloced);
  
  float4 *xi4  = calloc(prts_cuda->n_part, sizeof(float4));
  float4 *pxi4 = calloc(prts_cuda->n_part, sizeof(float4));
  
  __particles_cuda_from_device(prts_cuda, xi4, pxi4);
  
  for (int n = 0; n < prts->n_part; n++) {
    particle_single_t *part_base = particles_single_get_one(prts, n);

    part_base->xi  = xi4[n].x;
    part_base->yi  = xi4[n].y;
    part_base->zi  = xi4[n].z;
    part_base->kind = cuda_float_as_int(xi4[n].w);
    part_base->pxi = pxi4[n].x;
    part_base->pyi = pxi4[n].y;
    part_base->pzi = pxi4[n].z;
    part_base->qni_wni = pxi4[n].w;
  }

  free(xi4);
  free(pxi4);
}

// ======================================================================
// psc_mparticles: subclass "cuda"
  
struct psc_mparticles_ops psc_mparticles_cuda_ops = {
  .name                    = "cuda",
};

// ======================================================================
// psc_particles: subclass "cuda"

static struct mrc_obj_method psc_particles_cuda_methods[] = {
  MRC_OBJ_METHOD("copy_to_c"       , psc_particles_cuda_copy_to_c),
  MRC_OBJ_METHOD("copy_from_c"     , psc_particles_cuda_copy_from_c),
  MRC_OBJ_METHOD("copy_to_single"  , psc_particles_cuda_copy_to_single),
  MRC_OBJ_METHOD("copy_from_single", psc_particles_cuda_copy_from_single),
  {}
};

struct psc_particles_ops psc_particles_cuda_ops = {
  .name                    = "cuda",
  .size                    = sizeof(struct psc_particles_cuda),
  .methods                 = psc_particles_cuda_methods,
  .setup                   = psc_particles_cuda_setup,
  .destroy                 = psc_particles_cuda_destroy,
};
