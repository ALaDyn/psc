
#include "psc.h"
#include "psc_cuda.h"
#include "psc_bnd_cuda.h"
#include "psc_particles_cuda.h"
#include "psc_particles_single.h"
#include "psc_push_particles.h"
#include "particles_cuda.h"

#include <mrc_io.h>

EXTERN_C void cuda_init(int rank);

// ======================================================================
// psc_particles "cuda"

static void
psc_particles_cuda_setup(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  struct psc_patch *patch = &ppsc->patch[prts->p];

  if (!prts->flags) {
    // FIXME, they get set too early, so auto-dispatch "1vb" doesn't work
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
}

static void
psc_particles_cuda_destroy(struct psc_particles *prts)
{
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts);

  cell_map_free(&cuda->map);
}

#ifdef HAVE_LIBHDF5_HL

// FIXME. This is a rather bad break of proper layering, HDF5 should be all
// mrc_io business. OTOH, it could be called flexibility...

#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

// ----------------------------------------------------------------------
// psc_particles_cuda_write

static void
psc_particles_cuda_write(struct psc_particles *prts, struct mrc_io *io)
{
  int ierr;
  assert(sizeof(particle_cuda_real_t) == sizeof(float));

  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);

  hid_t group = H5Gopen(h5_file, psc_particles_name(prts), H5P_DEFAULT); H5_CHK(group);
  // save/restore n_alloced, too?
  ierr = H5LTset_attribute_int(group, ".", "p", &prts->p, 1); CE;
  ierr = H5LTset_attribute_int(group, ".", "n_part", &prts->n_part, 1); CE;
  ierr = H5LTset_attribute_uint(group, ".", "flags", &prts->flags, 1); CE;
  if (prts->n_part > 0) {
    float4 *xi4  = calloc(prts->n_part, sizeof(float4));
    float4 *pxi4 = calloc(prts->n_part, sizeof(float4));
  
    __particles_cuda_from_device(prts, xi4, pxi4);
  
    hsize_t hdims[2] = { prts->n_part, 4 };
    ierr = H5LTmake_dataset_float(group, "xi4", 2, hdims, (float *) xi4); CE;
    ierr = H5LTmake_dataset_float(group, "pxi4", 2, hdims, (float *) pxi4); CE;

    free(xi4);
    free(pxi4);
  }
  ierr = H5Gclose(group); CE;
}

// ----------------------------------------------------------------------
// psc_particles_cuda_read

static void
psc_particles_cuda_read(struct psc_particles *prts, struct mrc_io *io)
{
  int ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);

  hid_t group = H5Gopen(h5_file, psc_particles_name(prts), H5P_DEFAULT); H5_CHK(group);
  ierr = H5LTget_attribute_int(group, ".", "p", &prts->p); CE;
  ierr = H5LTget_attribute_int(group, ".", "n_part", &prts->n_part); CE;
  ierr = H5LTget_attribute_uint(group, ".", "flags", &prts->flags); CE;
  psc_particles_setup(prts);
  if (prts->n_part > 0) {
    float4 *xi4  = calloc(prts->n_part, sizeof(float4));
    float4 *pxi4 = calloc(prts->n_part, sizeof(float4));
  
    ierr = H5LTread_dataset_float(group, "xi4", (float *) xi4); CE;
    ierr = H5LTread_dataset_float(group, "pxi4", (float *) pxi4); CE;

    __particles_cuda_to_device(prts, xi4, pxi4);

    assert(0); // need to do on mprts basis
    // to restore offsets etc.
    if (prts->flags & MP_NEED_BLOCK_OFFSETS) {
      cuda_sort_patch(prts->p, prts);
    }
    if (prts->flags & MP_NEED_CELL_OFFSETS) {
      cuda_sort_patch_by_cell(prts->p, prts);
    }

    free(xi4);
    free(pxi4);
  }
  ierr = H5Gclose(group); CE;
}

#endif

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
    pos[d] = particle_c_real_fint(xi[d]);
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
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts_cuda);
  assert(prts_cuda->n_part == prts_c->n_part);
  
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
      int bi = particle_c_real_fint(xi[d] * cuda->b_dxi[d]);
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
      bi = particle_c_real_fint(xi[d] * cuda->b_dxi[d]);
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
  
  __particles_cuda_to_device(prts_cuda, xi4, pxi4);
  
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
  struct psc_particles_cuda *cuda = psc_particles_cuda(prts_cuda);
  assert(prts_cuda->n_part == prts->n_part);
  
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
      int bi = particle_single_real_fint(xi[d] * cuda->b_dxi[d]);
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
      bi = particle_single_real_fint(xi[d] * cuda->b_dxi[d]);
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
  __particles_cuda_to_device(prts_cuda, xi4, pxi4);
  
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
#ifdef HAVE_LIBHDF5_HL
  .read                    = psc_particles_cuda_read,
  .write                   = psc_particles_cuda_write,
#endif
};

// ======================================================================
// psc_mparticles "cuda"

// ----------------------------------------------------------------------
// psc_mparticles_cuda_setup

static void
psc_mparticles_cuda_setup(struct psc_mparticles *mprts)
{
  psc_mparticles_setup_super(mprts);
  __psc_mparticles_cuda_setup(mprts);
}

// ----------------------------------------------------------------------
// psc_mparticles_cuda_destroy

static void
psc_mparticles_cuda_destroy(struct psc_mparticles *mprts)
{
  __psc_mparticles_cuda_free(mprts);
}

// ----------------------------------------------------------------------
// psc_mparticles_cuda_setup_internals

static void
psc_mparticles_cuda_setup_internals(struct psc_mparticles *mprts)
{
  assert((mprts->flags & MP_NEED_BLOCK_OFFSETS) &&
	 !(mprts->flags & MP_NEED_CELL_OFFSETS));

  cuda_mprts_sort_initial(mprts);
}

// ======================================================================
// psc_mparticles: subclass "cuda"
  
struct psc_mparticles_ops psc_mparticles_cuda_ops = {
  .name                    = "cuda",
  .size                    = sizeof(struct psc_mparticles_cuda),
  .setup                   = psc_mparticles_cuda_setup,
  .destroy                 = psc_mparticles_cuda_destroy,
  .setup_internals         = psc_mparticles_cuda_setup_internals,
};

