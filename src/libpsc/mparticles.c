
#include "psc.h"

#include "psc_particles_c.h"

#include <mrc_profile.h>
#include <mrc_params.h>
#include <stdlib.h>
#include <string.h>

// ======================================================================

void
psc_mparticles_set_domain_nr_particles(struct psc_mparticles *mparticles,
				       struct mrc_domain *domain,
				       int *nr_particles_by_patch)
{
  mparticles->domain = domain;
  mrc_domain_get_patches(domain, &mparticles->nr_patches);
  int *np = malloc(mparticles->nr_patches * sizeof(*np));
  for (int p = 0; p < mparticles->nr_patches; p++) {
    np[p] = nr_particles_by_patch[p];
  }
  mparticles->nr_particles_by_patch = np;
}

static void
_psc_mparticles_setup(struct psc_mparticles *mparticles)
{
  assert(mparticles->nr_particles_by_patch);
  struct psc_mparticles_ops *ops = psc_mparticles_ops(mparticles);

  if (ops->alloc_patch) {
    mparticles->patches = calloc(mparticles->nr_patches, sizeof(*mparticles->patches));
    for (int p = 0; p < mparticles->nr_patches; p++) {
      mparticles->patches[p] = ops->alloc_patch(p, mparticles->nr_particles_by_patch[p],
						mparticles->flags);
    }

    free(mparticles->nr_particles_by_patch);
    mparticles->nr_particles_by_patch = NULL;
  }

  if (ops->setup) {
    ops->setup(mparticles);
    return;
  }
}

static void
_psc_mparticles_destroy(struct psc_mparticles *mparticles)
{
  struct psc_mparticles_ops *ops = psc_mparticles_ops(mparticles);

  if (ops->free_patch) {
    for (int p = 0; p < mparticles->nr_patches; p++) {
      ops->free_patch(p, mparticles->patches[p]);
    }
    free(mparticles->patches);
  }
}

int
psc_mparticles_nr_particles(struct psc_mparticles *mparticles)
{
  int nr_part = 0;
  for (int p = 0; p < mparticles->nr_patches; p++) {
    nr_part += psc_mparticles_nr_particles_by_patch(mparticles, p);
  }
  return nr_part;
}

int
psc_mparticles_nr_particles_by_patch(struct psc_mparticles *mparticles, int p)
{
  struct psc_mparticles_ops *ops = psc_mparticles_ops(mparticles);
  assert(ops && ops->nr_particles_by_patch);
  return ops->nr_particles_by_patch(mparticles, p);
}

struct psc_mparticles *
psc_mparticles_get_as(struct psc_mparticles *mp_base, const char *type,
		      unsigned int flags)
{
  const char *type_base = psc_mparticles_type(mp_base);
  // If we're already the subtype, nothing to be done
  if (strcmp(type_base, type) == 0)
    return mp_base;

  static int pr;
  if (!pr) {
    pr = prof_register("mparticles_get_as", 1., 0, 0);
  }
  prof_start(pr);

  int *nr_particles_by_patch = malloc(mp_base->nr_patches * sizeof(int));
  for (int p = 0; p < mp_base->nr_patches; p++) {
    nr_particles_by_patch[p] = psc_mparticles_nr_particles_by_patch(mp_base, p);
  }
  struct psc_mparticles *mp =
    psc_mparticles_create(psc_mparticles_comm(mp_base));
  psc_mparticles_set_type(mp, type);
  psc_mparticles_set_domain_nr_particles(mp, mp_base->domain, nr_particles_by_patch);
  psc_mparticles_set_param_int(mp, "flags", flags);
  psc_mparticles_setup(mp);
  free(nr_particles_by_patch);

  if (!(flags & MP_DONT_COPY)) {
    char s[strlen(type) + 12]; sprintf(s, "copy_to_%s", type);
    psc_mparticles_copy_to_func_t copy_to = (psc_mparticles_copy_to_func_t)
      psc_mparticles_get_method(mp_base, s);
    if (copy_to) {
      for (int p = 0; p < mp_base->nr_patches; p++) {
	copy_to(p, mp_base, mp, flags);
      }
    } else {
      sprintf(s, "copy_from_%s", type_base);
      psc_mparticles_copy_to_func_t copy_from = (psc_mparticles_copy_from_func_t)
	psc_mparticles_get_method(mp, s);
      if (copy_from) {
      for (int p = 0; p < mp_base->nr_patches; p++) {
	copy_from(p, mp, mp_base, flags);
      }
      } else {
	fprintf(stderr, "ERROR: no 'copy_to_%s' in psc_mparticles '%s' and "
		"no 'copy_from_%s' in '%s'!\n",
		type, psc_mparticles_type(mp_base), type_base, psc_mparticles_type(mp));
	assert(0);
      }
    }
  }

  prof_stop(pr);
  return mp;
}

void
psc_mparticles_put_as(struct psc_mparticles *mp, struct psc_mparticles *mp_base,
		      unsigned int flags)
{
  // If we're already the subtype, nothing to be done
  const char *type = psc_mparticles_type(mp);
  const char *type_base = psc_mparticles_type(mp_base);
  if (strcmp(type_base, type) == 0)
    return;

  static int pr;
  if (!pr) {
    pr = prof_register("mparticles_put_as", 1., 0, 0);
  }
  prof_start(pr);

  if (!(flags & MP_DONT_COPY)) {
    char s[strlen(type) + 12]; sprintf(s, "copy_from_%s", type);
    psc_mparticles_copy_from_func_t copy_from = (psc_mparticles_copy_from_func_t)
      psc_mparticles_get_method(mp_base, s);
    if (copy_from) {
      for (int p = 0; p < mp_base->nr_patches; p++) {
	copy_from(p, mp_base, mp, MP_NEED_BLOCK_OFFSETS | MP_NEED_CELL_OFFSETS);
      }
    } else {
      sprintf(s, "copy_to_%s", type_base);
      psc_mparticles_copy_from_func_t copy_to = (psc_mparticles_copy_from_func_t)
	psc_mparticles_get_method(mp, s);
      if (copy_to) {
	for (int p = 0; p < mp_base->nr_patches; p++) {
	  copy_to(p, mp, mp_base, MP_NEED_BLOCK_OFFSETS | MP_NEED_CELL_OFFSETS);
	}
      } else {
	fprintf(stderr, "ERROR: no 'copy_from_%s' in psc_mparticles '%s' and "
		"no 'copy_to_%s' in '%s'!\n",
		type, psc_mparticles_type(mp_base), type_base, psc_mparticles_type(mp));
	assert(0);
      }
    }
  }
  psc_mparticles_destroy(mp);

  prof_stop(pr);
}

void
psc_mparticles_check(mparticles_base_t *particles_base)
{
  int fail_cnt = 0;

  mparticles_c_t *particles = psc_mparticles_get_c(particles_base, 0);

  psc_foreach_patch(ppsc, p) {
    struct psc_patch *patch = &ppsc->patch[p];
    particles_c_t *pp = psc_mparticles_get_patch_c(particles, p);
    f_real xb[3], xe[3];
    
    // New-style boundary requirements.
    // These will need revisiting when it comes to non-periodic domains.
    
    for (int d = 0; d < 3; d++) {
      xb[d] = patch->xb[d];
      xe[d] = patch->xb[d] + patch->ldims[d] * ppsc->dx[d];
    }
    
    for (int i = 0; i < pp->n_part; i++) {
      particle_c_t *part = particles_c_get_one(pp, i);
      if (part->xi < xb[0] || part->xi >= xe[0] || // FIXME xz only!
	  part->zi < xb[2] || part->zi >= xe[2]) {
	if (fail_cnt++ < 10) {
	  mprintf("FAIL: xi %g [%g:%g]\n", part->xi, xb[0], xe[0]);
	  mprintf("      zi %g [%g:%g]\n", part->zi, xb[2], xe[2]);
	}
      }
    }
  }
  assert(fail_cnt == 0);
  psc_mparticles_put_c(particles, particles_base, MP_DONT_COPY);
}

#define MAKE_MPARTICLES_GET_PUT(type)					\
									\
mparticles_##type##_t *							\
psc_mparticles_get_##type(struct psc_mparticles *particles_base,	\
			  unsigned int flags)				\
{									\
  return psc_mparticles_get_as(particles_base, #type, flags);		\
}									\
									\
void									\
psc_mparticles_put_##type(mparticles_##type##_t *particles,		\
			  struct psc_mparticles *particles_base,	\
			  unsigned int flags)				\
{									\
  psc_mparticles_put_as(particles, particles_base, flags);		\
}									\

MAKE_MPARTICLES_GET_PUT(c)
MAKE_MPARTICLES_GET_PUT(single)
MAKE_MPARTICLES_GET_PUT(double)
MAKE_MPARTICLES_GET_PUT(fortran)
#ifdef USE_CUDA
MAKE_MPARTICLES_GET_PUT(cuda)
#endif

// ======================================================================

static void
psc_mparticles_init()
{
  mrc_class_register_subclass(&mrc_class_psc_mparticles, &psc_mparticles_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_mparticles, &psc_mparticles_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_mparticles, &psc_mparticles_double_ops);
  mrc_class_register_subclass(&mrc_class_psc_mparticles, &psc_mparticles_fortran_ops);
#ifdef USE_CUDA
  mrc_class_register_subclass(&mrc_class_psc_mparticles, &psc_mparticles_cuda_ops);
#endif
}

#define VAR(x) (void *)offsetof(struct psc_mparticles, x)
static struct param psc_mparticles_descr[] = {
  { "flags"             , VAR(flags)           , PARAM_INT(0)       },
  {},
};
#undef VAR

struct mrc_class_psc_mparticles mrc_class_psc_mparticles = {
  .name             = "psc_mparticles_c",
  .size             = sizeof(struct psc_mparticles),
  .param_descr      = psc_mparticles_descr,
  .init             = psc_mparticles_init,
  .setup            = _psc_mparticles_setup,
  .destroy          = _psc_mparticles_destroy,
};

