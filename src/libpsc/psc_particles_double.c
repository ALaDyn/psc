
#include "psc.h"
#include "psc_particles_double.h"
#include "psc_particles_c.h"

#include <mrc_io.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// ======================================================================
// psc_particles "double"

static void
psc_particles_double_setup(struct psc_particles *prts)
{
  struct psc_particles_double *sub = psc_particles_double(prts);

  prts->n_alloced = psc_particles_size(prts) * 1.2 + 1000000;
  sub->particles = calloc(prts->n_alloced, sizeof(*sub->particles));
}

static void
psc_particles_double_destroy(struct psc_particles *prts)
{
  struct psc_particles_double *sub = psc_particles_double(prts);

  free(sub->particles);
}

void
particles_double_realloc(struct psc_particles *prts, int new_n_part)
{
  struct psc_particles_double *sub = psc_particles_double(prts);

  if (new_n_part <= prts->n_alloced)
    return;

  prts->n_alloced = new_n_part * 1.2;
  sub->particles = realloc(sub->particles, prts->n_alloced * sizeof(*sub->particles));
}

// ======================================================================

#ifdef HAVE_LIBHDF5_HL

// FIXME. This is a rather bad break of proper layering, HDF5 should be all
// mrc_io business. OTOH, it could be called flexibility...

#include <hdf5.h>
#include <hdf5_hl.h>

#define H5_CHK(ierr) assert(ierr >= 0)
#define CE assert(ierr == 0)

// ----------------------------------------------------------------------
// psc_particles_double_write

static void
psc_particles_double_write(struct psc_particles *prts, struct mrc_io *io)
{
  int ierr;
  assert(sizeof(particle_double_t) / sizeof(particle_double_real_t) == 8);
  assert(sizeof(particle_double_real_t) == sizeof(double));

  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);

  hid_t group = H5Gopen(h5_file, mrc_io_obj_path(io, prts), H5P_DEFAULT); H5_CHK(group);
  // save/restore n_alloced, too?
  ierr = H5LTset_attribute_int(group, ".", "p", &prts->p, 1); CE;
  int n_prts = psc_particles_size(prts);
  ierr = H5LTset_attribute_int(group, ".", "n_part", &n_prts, 1); CE;
  ierr = H5LTset_attribute_uint(group, ".", "flags", &prts->flags, 1); CE;
  if (n_prts > 0) {
    // in a rather ugly way, we write the int "kind/tag" members together as double
    hsize_t hdims[2] = { n_prts, 8 };
    ierr = H5LTmake_dataset_double(group, "particles_double", 2, hdims,
				  (double *) particles_double_get_one(prts, 0)); CE;
  }
  ierr = H5Gclose(group); CE;
}

// ----------------------------------------------------------------------
// psc_particles_double_read

static void
psc_particles_double_read(struct psc_particles *prts, struct mrc_io *io)
{
  int ierr;
  long h5_file;
  mrc_io_get_h5_file(io, &h5_file);

  hid_t group = H5Gopen(h5_file, mrc_io_obj_path(io, prts), H5P_DEFAULT); H5_CHK(group);
  ierr = H5LTget_attribute_int(group, ".", "p", &prts->p); CE;
  int n_prts;
  ierr = H5LTget_attribute_int(group, ".", "n_part", &n_prts); CE;
  psc_particles_resize(prts, n_prts);
  ierr = H5LTget_attribute_uint(group, ".", "flags", &prts->flags); CE;
  psc_particles_setup(prts);
  if (n_prts > 0) {
    ierr = H5LTread_dataset_double(group, "particles_double",
				  (double *) particles_double_get_one(prts, 0)); CE;
  }
  ierr = H5Gclose(group); CE;
}

#endif

static void
copy_from(int p, struct psc_mparticles *mprts,
	  struct psc_mparticles *mprts_dbl, unsigned int flags,
	  void (*get_particle)(particle_double_t *prt, int n, struct psc_particles *prts))
{
  struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles *prts_dbl = psc_mparticles_get_patch(mprts_dbl, p);
  int n_prts = psc_particles_size(prts_dbl);
  psc_particles_resize(prts, n_prts);
  for (int n = 0; n < n_prts; n++) {
    particle_double_t *prt = particles_double_get_one(prts, n);
    get_particle(prt, n, prts_dbl);
  }
}

static void
copy_to(int p, struct psc_mparticles *mprts,
	struct psc_mparticles *mprts_dbl, unsigned int flags,
	void (*put_particle)(particle_double_t *prt, int n, struct psc_particles *prts))
{
  struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
  struct psc_particles *prts_dbl = psc_mparticles_get_patch(mprts_dbl, p);
  int n_prts = psc_particles_size(prts);
  psc_particles_resize(prts_dbl, n_prts);
  for (int n = 0; n < n_prts; n++) {
    particle_double_t *prt = particles_double_get_one(prts, n);
    put_particle(prt, n, prts_dbl);
  }
}


// ======================================================================
// conversion to/from "c"

static inline void
calc_vxi(particle_double_real_t vxi[3], particle_double_t *part)
{
  particle_double_real_t root =
    1.f / sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

static void
get_particle_c(particle_double_t *prt, int n, struct psc_particles *prts_c)
{
  particle_double_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }
  
  particle_c_t *prt_c = particles_c_get_one(prts_c, n);

  prt->xi      = prt_c->xi;
  prt->yi      = prt_c->yi;
  prt->zi      = prt_c->zi;
  prt->pxi     = prt_c->pxi;
  prt->pyi     = prt_c->pyi;
  prt->pzi     = prt_c->pzi;
  prt->kind    = prt_c->kind;
  prt->qni_wni = prt_c->qni * prt_c->wni;

  particle_double_real_t vxi[3];
  calc_vxi(vxi, prt);
  prt->xi += dth[0] * vxi[0];
  prt->yi += dth[1] * vxi[1];
  prt->zi += dth[2] * vxi[2];
}

static void
put_particle_c(particle_double_t *prt, int n, struct psc_particles *prts_c)
{
  particle_double_real_t dth[3] = { .5 * ppsc->dt, .5 * ppsc->dt, .5 * ppsc->dt };
  // don't shift in invariant directions
  for (int d = 0; d < 3; d++) {
    if (ppsc->domain.gdims[d] == 1) {
      dth[d] = 0.;
    }
  }
  
  particle_double_real_t vxi[3];
  calc_vxi(vxi, prt);

  particle_c_t *prt_c = particles_c_get_one(prts_c, n);

  particle_c_real_t qni = ppsc->kinds[prt->kind].q;
  particle_c_real_t mni = ppsc->kinds[prt->kind].m;
  particle_c_real_t wni = prt->qni_wni / qni;

  prt_c->xi      = prt->xi - dth[0] * vxi[0];
  prt_c->yi      = prt->yi - dth[1] * vxi[1];
  prt_c->zi      = prt->zi - dth[2] * vxi[2];
  prt_c->pxi     = prt->pxi;
  prt_c->pyi     = prt->pyi;
  prt_c->pzi     = prt->pzi;
  prt_c->kind    = prt->kind;
  prt_c->qni     = qni;
  prt_c->wni     = wni;
  prt_c->mni     = mni;
}

static void
psc_mparticles_double_copy_to_c(int p, struct psc_mparticles *mprts,
				struct psc_mparticles *mprts_c, unsigned int flags)
{
  copy_to(p, mprts, mprts_c, flags, put_particle_c);
}

static void
psc_mparticles_double_copy_from_c(int p, struct psc_mparticles *mprts,
				  struct psc_mparticles *mprts_c, unsigned int flags)
{
  copy_from(p, mprts, mprts_c, flags, get_particle_c);
}

// ======================================================================
// psc_particles: subclass "double"

struct psc_particles_ops psc_particles_double_ops = {
  .name                    = "double",
  .size                    = sizeof(struct psc_particles_double),
  .setup                   = psc_particles_double_setup,
  .destroy                 = psc_particles_double_destroy,
#ifdef HAVE_LIBHDF5_HL
  .read                    = psc_particles_double_read,
  .write                   = psc_particles_double_write,
#endif
};

// ======================================================================
// psc_mparticles: subclass "double"
  
static struct mrc_obj_method psc_particles_double_methods[] = {
  MRC_OBJ_METHOD("copy_to_c",   psc_mparticles_double_copy_to_c),
  MRC_OBJ_METHOD("copy_from_c", psc_mparticles_double_copy_from_c),
  {}
};

struct psc_mparticles_ops psc_mparticles_double_ops = {
  .name                    = "double",
  .methods                 = psc_particles_double_methods,
};

