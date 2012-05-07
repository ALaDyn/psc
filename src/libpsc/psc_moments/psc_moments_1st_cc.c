
#include "psc_moments_private.h"
#include "psc_particles_as_c.h"
#include "psc_fields_as_c.h"

#include "psc_bnd.h"
#include <mrc_profile.h>
#include <math.h>

// ======================================================================

typedef fields_c_real_t creal;

#define DEPOSIT_TO_GRID_1ST_CC(part, pf, m, val) do {			\
    creal u = (part->xi - patch->xb[0]) * dxi - .5;			\
    creal v = (part->yi - patch->xb[1]) * dyi - .5;			\
    creal w = (part->zi - patch->xb[2]) * dzi - .5;			\
    int jx = particle_real_fint(u);					\
    int jy = particle_real_fint(v);					\
    int jz = particle_real_fint(w);					\
    creal h1 = u - jx;							\
    creal h2 = v - jy;							\
    creal h3 = w - jz;							\
    									\
    creal g0x = 1.f - h1;						\
    creal g0y = 1.f - h2;						\
    creal g0z = 1.f - h3;						\
    creal g1x = h1;							\
    creal g1y = h2;							\
    creal g1z = h3;							\
    									\
    if (ppsc->domain.gdims[0] == 1) {					\
      jx = 0; g0x = 1.; g1x = 0.;					\
    }									\
    if (ppsc->domain.gdims[1] == 1) {					\
      jy = 0; g0y = 1.; g1y = 0.;					\
    }									\
    if (ppsc->domain.gdims[2] == 1) {					\
      jz = 0; g0z = 1.; g1z = 0.;					\
    }									\
    									\
    assert(jx >= -1 && jx < patch->ldims[0]);				\
    assert(jy >= -1 && jy < patch->ldims[1]);				\
    assert(jz >= -1 && jz < patch->ldims[2]);				\
    									\
    creal fnq = part->wni * fnqs;					\
									\
    F3(pf, m, jx  ,jy  ,jz  ) += fnq*g0x*g0y*g0z * (val);		\
    F3(pf, m, jx+1,jy  ,jz  ) += fnq*g1x*g0y*g0z * (val);		\
    F3(pf, m, jx  ,jy+1,jz  ) += fnq*g0x*g1y*g0z * (val);		\
    F3(pf, m, jx+1,jy+1,jz  ) += fnq*g1x*g1y*g0z * (val);		\
    F3(pf, m, jx  ,jy  ,jz+1) += fnq*g0x*g0y*g1z * (val);		\
    F3(pf, m, jx+1,jy  ,jz+1) += fnq*g1x*g0y*g1z * (val);		\
    F3(pf, m, jx  ,jy+1,jz+1) += fnq*g0x*g1y*g1z * (val);		\
    F3(pf, m, jx+1,jy+1,jz+1) += fnq*g1x*g1y*g1z * (val);		\
  } while (0)

// FIXME, this function exists about 100x all over the place, should
// be consolidated

static inline void
psc_particle_c_calc_vxi(particle_c_t *part, particle_c_real_t vxi[3])
{
  particle_c_real_t root =
    1.f / sqrt(1.f + sqr(part->pxi) + sqr(part->pyi) + sqr(part->pzi));
  vxi[0] = part->pxi * root;
  vxi[1] = part->pyi * root;
  vxi[2] = part->pzi * root;
}

int
psc_particle_c_kind(particle_c_t *part)
{
  if (part->qni < 0.) {
    return 0;
  } else if (part->qni > 0.) {
    return 1;
  } else {
    assert(0);
  }
}

// ======================================================================

static void
do_1st_calc_densities(int p, fields_t *pf, particles_t *pp)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0], dyi = 1.f / ppsc->dx[1], dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < pp->n_part; n++) {
    particle_t *part = particles_get_one(pp, n);
    int m = psc_particle_c_kind(part);

    DEPOSIT_TO_GRID_1ST_CC(part, pf, m, part->qni);
  }
}

static void
do_1st_calc_v(int p, fields_t *pf, particles_t *pp)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0], dyi = 1.f / ppsc->dx[1], dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < pp->n_part; n++) {
    particle_t *part = particles_get_one(pp, n);
    int mm = psc_particle_c_kind(part) * 3;

    creal vxi[3];
    psc_particle_c_calc_vxi(part, vxi);

    for (int m = 0; m < 3; m++) {
      DEPOSIT_TO_GRID_1ST_CC(part, pf, mm + m, vxi[m]);
    }
  }
}

static void
do_1st_calc_vv(int p, fields_t *pf, particles_t *pp)
{
  creal fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  creal dxi = 1.f / ppsc->dx[0], dyi = 1.f / ppsc->dx[1], dzi = 1.f / ppsc->dx[2];

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < pp->n_part; n++) {
    particle_t *part = particles_get_one(pp, n);
    int mm = psc_particle_c_kind(part) * 3;
      
    creal vxi[3];
    psc_particle_c_calc_vxi(part, vxi);

    for (int m = 0; m < 3; m++) {
      DEPOSIT_TO_GRID_1ST_CC(part, pf, mm + m, vxi[m] * vxi[m]);
    }
  }
}

static void
add_ghosts_reflecting_lo(mfields_c_t *res, int p, int d, int mb, int me)
{
  fields_t *pf = psc_mfields_get_patch(res, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    for (int iz = 0; iz < patch->ldims[2]; iz++) {
      for (int ix = 0; ix < patch->ldims[0]; ix++) {
	int iy = 0; {
	  for (int m = mb; m < me; m++) {
	    F3(pf, m, ix,iy,iz) += F3(pf, m, ix,iy-1,iz);
	  }
	}
      }
    }
  } else if (d == 2) {
    for (int iy = 0; iy < patch->ldims[1]; iy++) {
      for (int ix = 0; ix < patch->ldims[0]; ix++) {
	int iz = 0; {
	  for (int m = mb; m < me; m++) {
	    F3(pf, m, ix,iy,iz) += F3(pf, m, ix,iy,iz-1);
	  }
	}
      }
    }
  } else {
    assert(0);
  }
}

static void
add_ghosts_reflecting_hi(mfields_c_t *res, int p, int d, int mb, int me)
{
  fields_t *pf = psc_mfields_get_patch(res, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    for (int iz = 0; iz < patch->ldims[2]; iz++) {
      for (int ix = 0; ix < patch->ldims[0]; ix++) {
	int iy = patch->ldims[1] - 1; {
	  for (int m = mb; m < me; m++) {
	    F3(pf, m, ix,iy,iz) += F3(pf, m, ix,iy+1,iz);
	  }
	}
      }
    }
  } else if (d == 2) {
    for (int iy = 0; iy < patch->ldims[1]; iy++) {
      for (int ix = 0; ix < patch->ldims[0]; ix++) {
	int iz = patch->ldims[2] - 1; {
	  for (int m = mb; m < me; m++) {
	    F3(pf, m, ix,iy,iz) += F3(pf, m, ix,iy,iz+1);
	  }
	}
      }
    }
  } else {
    assert(0);
  }
}

static void
add_ghosts_boundary(mfields_c_t *res, int mb, int me)
{
  psc_foreach_patch(ppsc, p) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	if (ppsc->domain.bnd_part_lo[d] == BND_PART_REFLECTING) {
	  add_ghosts_reflecting_lo(res, p, d, mb, me);
	}
      }
    }
    // hi
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	if (ppsc->domain.bnd_part_hi[d] == BND_PART_REFLECTING) {
	  add_ghosts_reflecting_hi(res, p, d, mb, me);
	}
      }
    }
  }
}

static void
psc_moments_1st_cc_calc_densities(struct psc_moments *moments, mfields_base_t *flds,
				  mparticles_base_t *particles_base, mfields_c_t *res)
{
  static int pr;
  if (!pr) {
    pr = prof_register("c_densities", 1., 0, 0);
  }

  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  prof_start(pr);
  psc_mfields_zero(res, 0);
  psc_mfields_zero(res, 1);
  psc_mfields_zero(res, 2);
  
  psc_foreach_patch(ppsc, p) {
    do_1st_calc_densities(p, psc_mfields_get_patch_c(res, p),
			psc_mparticles_get_patch(particles, p));
  }
  prof_stop(pr);

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(moments->bnd, res, 0, 3);
  add_ghosts_boundary(res, 0, 3);
}

static void
psc_moments_1st_cc_calc_v(struct psc_moments *moments, mfields_base_t *flds,
			  mparticles_base_t *particles_base, mfields_c_t *res)
{
  static int pr;
  if (!pr) {
    pr = prof_register("c_moments_v", 1., 0, 0);
  }

  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  prof_start(pr);
  for (int m = 0; m < 6; m++) {
    psc_mfields_zero(res, m); // FIXME, pass range
  }
  
  psc_foreach_patch(ppsc, p) {
    do_1st_calc_v(p, psc_mfields_get_patch_c(res, p),
		  psc_mparticles_get_patch(particles, p));
  }
  prof_stop(pr);

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(moments->bnd, res, 0, 6);
  // FIXME, this is probably only right for densities, but needs sign
  // adjustments for velocities
  add_ghosts_boundary(res, 0, 6);
}

static void
psc_moments_1st_cc_calc_vv(struct psc_moments *moments, mfields_base_t *flds,
			  mparticles_base_t *particles_base, mfields_c_t *res)
{
  static int pr;
  if (!pr) {
    pr = prof_register("c_moments_vv", 1., 0, 0);
  }

  mparticles_t *particles = psc_mparticles_get_cf(particles_base, 0);

  prof_start(pr);
  for (int m = 0; m < 6; m++) {
    psc_mfields_zero(res, m); // FIXME, pass range
  }
  
  psc_foreach_patch(ppsc, p) {
    do_1st_calc_vv(p, psc_mfields_get_patch_c(res, p),
		   psc_mparticles_get_patch(particles, p));
  }
  prof_stop(pr);

  psc_mparticles_put_cf(particles, particles_base, MP_DONT_COPY);

  psc_bnd_add_ghosts(moments->bnd, res, 0, 6);
  // FIXME, this is probably only right for densities, but needs sign
  // adjustments for vv moments
  add_ghosts_boundary(res, 0, 6);
}

// ======================================================================
// psc_moments: subclass "1st_cc"

struct psc_moments_ops psc_moments_1st_cc_ops = {
  .name                  = "1st_cc",
  .calc_densities        = psc_moments_1st_cc_calc_densities,
  .calc_v                = psc_moments_1st_cc_calc_v,
  .calc_vv               = psc_moments_1st_cc_calc_vv,
};
