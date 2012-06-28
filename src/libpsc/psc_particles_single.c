
#include "psc.h"
#include "psc_particles_single.h"
#include "psc_particles_c.h"

#include <mrc_io.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// ======================================================================
// psc_particles "single"

static void
psc_particles_single_setup(struct psc_particles *prts)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  sngl->n_alloced = prts->n_part * 1.2;
  sngl->particles = calloc(sngl->n_alloced, sizeof(*sngl->particles));
  sngl->particles_alt = calloc(sngl->n_alloced, sizeof(*sngl->particles_alt));
  sngl->b_idx = calloc(sngl->n_alloced, sizeof(*sngl->b_idx));

  for (int d = 0; d < 3; d++) {
    sngl->b_mx[d] = ppsc->patch[prts->p].ldims[d];
    sngl->b_dxi[d] = 1.f / ppsc->dx[d];
  }
  sngl->nr_blocks = sngl->b_mx[0] * sngl->b_mx[1] * sngl->b_mx[2];
  sngl->b_cnt = calloc(sngl->nr_blocks + 1, sizeof(*sngl->b_cnt));
}

static void
psc_particles_single_destroy(struct psc_particles *prts)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  free(sngl->particles);
  free(sngl->particles_alt);
  free(sngl->b_idx);
  free(sngl->b_cnt);
}

static void
psc_particles_single_reorder(struct psc_particles *prts,
			     unsigned int *b_idx, unsigned int *b_sums)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

#if 1
  for (int n = 0; n < prts->n_part; n++) {
    int n_new = b_sums[b_idx[n]]++;
    sngl->particles_alt[n_new] = sngl->particles[n];
  }
#else
  int *b_off = malloc(sngl->n_alloced * sizeof(*b_off));
  for (int n = 0; n < prts->n_part; n++) {
    int n_new = b_sums[b_idx[n]]++;
    //    b_off[n_new] = n;
    b_off[n] = n_new;
  }

  for (int n = 0; n < prts->n_part; n++) {
    //    particles_new[n] = sngl->particles[b_off[n]];
    sngl->particles_alt[b_off[n]] = sngl->particles[n];
  }
  free(b_off);
#endif
  
  // swap in alt array
  particle_single_t *tmp = sngl->particles;
  sngl->particles = sngl->particles_alt;
  sngl->particles_alt = tmp;
}

void
particles_single_realloc(struct psc_particles *prts, int new_n_part)
{
  struct psc_particles_single *sngl = psc_particles_single(prts);

  if (new_n_part <= sngl->n_alloced)
    return;

  sngl->n_alloced = new_n_part * 1.2;
  sngl->particles = realloc(sngl->particles, sngl->n_alloced * sizeof(*sngl->particles));
  sngl->b_idx = realloc(sngl->b_idx, sngl->n_alloced * sizeof(*sngl->b_idx));
  free(sngl->particles_alt);
  sngl->particles_alt = malloc(sngl->n_alloced * sizeof(*sngl->particles_alt));
}

static inline void
calc_vxi(particle_single_real_t vxi[3], particle_single_t *part)
{
  particle_single_real_t root =
    1.f / sqrtf(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

static void
psc_particles_single_copy_to_c(struct psc_particles *prts_base,
			       struct psc_particles *prts_c, unsigned int flags)
{
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  prts_c->n_part = prts_base->n_part;
  assert(prts_c->n_part <= psc_particles_c(prts_c)->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_single_t *part_base = particles_single_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_c_real_t qni = ppsc->kinds[part_base->kind].q;
    particle_c_real_t mni = ppsc->kinds[part_base->kind].m;
    particle_c_real_t wni = part_base->qni_wni / qni;
    
    particle_single_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part->xi  = part_base->xi - dth[0] * vxi[0];
    part->yi  = part_base->yi - dth[1] * vxi[1];
    part->zi  = part_base->zi - dth[2] * vxi[2];
    part->pxi = part_base->pxi;
    part->pyi = part_base->pyi;
    part->pzi = part_base->pzi;
    part->qni = qni;
    part->mni = mni;
    part->wni = wni;
    part->kind = part_base->kind;
  }
}

static void
psc_particles_single_copy_from_c(struct psc_particles *prts_base,
				 struct psc_particles *prts_c, unsigned int flags)
{
  particle_single_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }

  struct psc_particles_single *sngl = psc_particles_single(prts_base);
  prts_base->n_part = prts_c->n_part;
  assert(prts_base->n_part <= sngl->n_alloced);
  for (int n = 0; n < prts_base->n_part; n++) {
    particle_single_t *part_base = particles_single_get_one(prts_base, n);
    particle_c_t *part = particles_c_get_one(prts_c, n);
    
    particle_single_real_t qni_wni;
    if (part->qni != 0.) {
      qni_wni = part->qni * part->wni;
    } else {
      qni_wni = part->wni;
    }
    
    part_base->xi          = part->xi;
    part_base->yi          = part->yi;
    part_base->zi          = part->zi;
    part_base->pxi         = part->pxi;
    part_base->pyi         = part->pyi;
    part_base->pzi         = part->pzi;
    part_base->qni_wni     = qni_wni;
    part_base->kind        = part->kind;

    particle_single_real_t vxi[3];
    calc_vxi(vxi, part_base);
    part_base->xi += dth[0] * vxi[0];
    part_base->yi += dth[1] * vxi[1];
    part_base->zi += dth[2] * vxi[2];
  }
}

// ======================================================================
// psc_mparticles_single

#ifdef HAVE_LIBHDF5_HL

// FIXME. This is a rather bad break of proper layering, HDF5 should be all
// mrc_io business. OTOH, it could be called flexibility...

#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

static void
_psc_mparticles_single_write(struct psc_mparticles *mparticles, struct mrc_io *io)
{
  int ierr;
  const char *path = psc_mparticles_name(mparticles);
  mrc_io_write_obj_ref(io, path, "domain", (struct mrc_obj *) mparticles->domain);
  mrc_io_write_attr_int(io, path, "nr_patches", mparticles->nr_patches);
  mrc_io_write_attr_int(io, path, "flags", mparticles->flags);
  
  assert(sizeof(particle_single_t) / sizeof(particle_single_real_t) == 8);
  assert(sizeof(particle_single_real_t) == sizeof(float));

  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group = H5Gopen(h5_file, path, H5P_DEFAULT); H5_CHK(group);
  for (int p = 0; p < mparticles->nr_patches; p++) {
    struct psc_particles *prts = psc_mparticles_get_patch(mparticles, p);
    struct psc_particles_single *sngl = psc_particles_single(prts);
    char name[10]; sprintf(name, "p%d", p);

    hid_t groupp = H5Gcreate(group, name, H5P_DEFAULT, H5P_DEFAULT,
			     H5P_DEFAULT); H5_CHK(groupp);
    // save/restore n_alloced, too?
    ierr = H5LTset_attribute_int(groupp, ".", "n_part",
				 &prts->n_part, 1); CE;
    int flags = prts->flags;
    ierr = H5LTset_attribute_int(groupp, ".", "flags", &flags, 1); CE;
    if (prts->n_part > 0) {
      // in a rather ugly way, we write the long "kind" member as a float
      hsize_t hdims[2] = { prts->n_part, 8 };
      ierr = H5LTmake_dataset_float(groupp, "particles_single", 2, hdims,
				    (float *) sngl->particles); CE;
    }
    ierr = H5Gclose(groupp); CE;
  }

  ierr = H5Gclose(group); CE;
}

static void
_psc_mparticles_single_read(mparticles_single_t *mparticles, struct mrc_io *io)
{
  int ierr;
  const char *path = psc_mparticles_name(mparticles);
  mparticles->domain = (struct mrc_domain *)
    mrc_io_read_obj_ref(io, path, "domain", &mrc_class_mrc_domain);
  mrc_io_read_attr_int(io, path, "nr_patches", &mparticles->nr_patches);
  int flags;
  mrc_io_read_attr_int(io, path, "flags", &flags);
  mparticles->flags = flags;

  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);
  hid_t group = H5Gopen(h5_file, path, H5P_DEFAULT); H5_CHK(group);
  mparticles->prts = calloc(mparticles->nr_patches, sizeof(*mparticles->prts));
  mparticles->nr_particles_by_patch =
    calloc(mparticles->nr_patches, sizeof(*mparticles->nr_particles_by_patch));
  for (int p = 0; p < mparticles->nr_patches; p++) {
    char name[10]; sprintf(name, "p%d", p);
    hid_t groupp = H5Gopen(group, name, H5P_DEFAULT); H5_CHK(groupp);
    int n_part;
    ierr = H5LTget_attribute_int(groupp, ".", "n_part", &n_part); CE;
    struct psc_particles *prts = psc_particles_create(psc_mparticles_comm(mparticles));
    psc_particles_set_type(prts, "single");
    prts->n_part = n_part;
    prts->p = p;
    int flags;
    ierr = H5LTget_attribute_int(groupp, ".", "flags", &flags); CE;
    prts->flags = flags;
    psc_particles_setup(prts);
    mparticles->prts[p] = prts;
    if (n_part > 0) {
      ierr = H5LTread_dataset_float(groupp, "particles_single",
				    (float *) psc_particles_single(prts)->particles); CE;
    }

    ierr = H5Gclose(groupp); CE;
  }

  ierr = H5Gclose(group); CE;
}

#endif

// ======================================================================
// psc_mparticles: subclass "single"
  
struct psc_mparticles_ops psc_mparticles_single_ops = {
  .name                    = "single",
#ifdef HAVE_LIBHDF5_HL
  .write                   = _psc_mparticles_single_write,
  .read                    = _psc_mparticles_single_read,
#endif
};

// ======================================================================
// psc_particles: subclass "single"

static struct mrc_obj_method psc_particles_single_methods[] = {
  MRC_OBJ_METHOD("copy_to_c",   psc_particles_single_copy_to_c),
  MRC_OBJ_METHOD("copy_from_c", psc_particles_single_copy_from_c),
  {}
};

struct psc_particles_ops psc_particles_single_ops = {
  .name                    = "single",
  .size                    = sizeof(struct psc_particles_single),
  .methods                 = psc_particles_single_methods,
  .setup                   = psc_particles_single_setup,
  .destroy                 = psc_particles_single_destroy,
  .reorder                 = psc_particles_single_reorder,
};
