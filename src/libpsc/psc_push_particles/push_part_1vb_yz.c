
#include "psc_push_particles_1st.h"
#include <mrc_profile.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PARTICLE_TYPE "c"

typedef fields_c_t fields_cache_t;
#define F3_CACHE F3_C
typedef fields_c_t fields_curr_t;
#define F3_CURR F3_C

#define cache_fields_from_em cache_fields_c_from_em
#define cache_fields_to_j cache_fields_c_to_j

#include "c_common_push.c"

static void
do_push_part_1vb_yz(int p, fields_cache_t *pf, struct psc_particles *pp)
{
  particle_real_t dt = ppsc->dt;
  particle_real_t dqs = .5f * ppsc->coeff.eta * dt;
  particle_real_t fnqs = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  particle_real_t fnqys = ppsc->dx[1] * fnqs / dt;
  particle_real_t fnqzs = ppsc->dx[2] * fnqs / dt;
  particle_real_t dxi[3] = { 1.f / ppsc->dx[0], 1.f / ppsc->dx[1], 1.f / ppsc->dx[2] };

  struct psc_patch *patch = &ppsc->patch[p];
  for (int n = 0; n < pp->n_part; n++) {
    particle_t *part = particles_get_one(pp, n);
    particle_real_t vxi[3];

    // x^n, p^n -> x^(n+.5), p^n
    calc_vxi(vxi, part);
    push_xi(part, vxi, .5f * dt);

    // field interpolation

    int lg[3], lh[3];
    particle_real_t og[3], oh[3], xm[3];
    find_idx_off_pos_1st(&part->xi, lg, og, xm, 0.f, patch->xb, dxi); // FIXME passing xi hack
    find_idx_off_1st(&part->xi, lh, oh, -.5f, patch->xb, dxi);

    // FIELD INTERPOLATION

    INTERPOLATE_SETUP_1ST;

    particle_real_t exq = INTERPOLATE_FIELD_1ST_CACHE(EX, g, g);
    particle_real_t eyq = INTERPOLATE_FIELD_1ST_CACHE(EY, h, g);
    particle_real_t ezq = INTERPOLATE_FIELD_1ST_CACHE(EZ, g, h);

    particle_real_t hxq = INTERPOLATE_FIELD_1ST_CACHE(HX, h, h);
    particle_real_t hyq = INTERPOLATE_FIELD_1ST_CACHE(HY, g, h);
    particle_real_t hzq = INTERPOLATE_FIELD_1ST_CACHE(HZ, h, g);

    // x^(n+0.5), p^n -> x^(n+0.5), p^(n+1.0) 
    particle_real_t dq = dqs * particle_qni_div_mni(part);
    push_pxi(part, exq, eyq, ezq, hxq, hyq, hzq, dq);

    // x^(n+0.5), p^(n+1.0) -> x^(n+1.0), p^(n+1.0) 
    calc_vxi(vxi, part);
    push_xi(part, vxi, .5f * dt);

    // OUT OF PLANE CURRENT DENSITY AT (n+1.0)*dt

    int lf[3];
    particle_real_t of[3];
    find_idx_off_1st(&part->xi, lf, of, 0.f, patch->xb, dxi);

    particle_real_t fnqx = vxi[0] * part->qni * part->wni * fnqs;
    F3_CURR(pf, JXI, 0,lf[1]  ,lf[2]  ) += (1.f - of[1]) * (1.f - of[2]) * fnqx;
    F3_CURR(pf, JXI, 0,lf[1]+1,lf[2]  ) += (      of[1]) * (1.f - of[2]) * fnqx;
    F3_CURR(pf, JXI, 0,lf[1]  ,lf[2]+1) += (1.f - of[1]) * (      of[2]) * fnqx;
    F3_CURR(pf, JXI, 0,lf[1]+1,lf[2]+1) += (      of[1]) * (      of[2]) * fnqx;

    // x^(n+1), p^(n+1) -> x^(n+1.5f), p^(n+1)

    particle_real_t xi[3] = { 0.f,
		    part->yi + vxi[1] * .5f * dt,
		    part->zi + vxi[2] * .5f * dt };

    particle_real_t xp[3];
    find_idx_off_pos_1st(xi, lf, of, xp, 0.f, patch->xb, dxi);

    // OUT OF PLANE CURRENT DENSITY BETWEEN (n+.5)*dt and (n+1.5)*dt

    int i[2] = { lg[1], lg[2] };
    int idiff[2] = { lf[1] - lg[1], lf[2] - lg[2] };
    particle_real_t dx[2] = { xp[1] - xm[1], xp[2] - xm[2] };
    particle_real_t x[2] = { xm[1] - (i[0] + .5f), xm[2] - (i[1] + .5f) };

    particle_real_t dx1[2];
    int off[2];
    int first_dir, second_dir = -1;
    // FIXME, make sure we never div-by-zero?
    if (idiff[0] == 0 && idiff[1] == 0) {
      first_dir = -1;
    } else if (idiff[0] == 0) {
      first_dir = 1;
    } else if (idiff[1] == 0) {
      first_dir = 0;
    } else {
      dx1[0] = .5f * idiff[0] - x[0];
      dx1[1] = dx[1] / dx[0] * dx1[0];
      if (particle_real_abs(x[1] + dx1[1]) > .5f) {
	first_dir = 1;
      } else {
	first_dir = 0;
      }
      second_dir = 1 - first_dir;
    }

    particle_real_t fnq[2] = { part->qni * part->wni * fnqys,
			       part->qni * part->wni * fnqzs };

    if (first_dir >= 0) {
      off[1-first_dir] = 0;
      off[first_dir] = idiff[first_dir];
      calc_dx1(dx1, x, dx, off);
      curr_2d_vb_cell(pf, i, x, dx1, fnq, dx, off);
    }

    if (second_dir >= 0) {
      off[first_dir] = 0;
      off[second_dir] = idiff[second_dir];
      calc_dx1(dx1, x, dx, off);
      curr_2d_vb_cell(pf, i, x, dx1, fnq, dx, off);
    }
    
    curr_2d_vb_cell(pf, i, x, dx, fnq, NULL, NULL);
  }
}

void
psc_push_particles_1vb_push_yz(struct psc_push_particles *push,
			       mparticles_base_t *particles_base,
			       mfields_base_t *flds_base)
{
  mfields_t *flds = psc_mfields_get_cf(flds_base, EX, EX + 6);

  static int pr;
  if (!pr) {
    pr = prof_register(PARTICLE_TYPE "_1vb_push_yz", 1., 0, 0);
  }
  prof_start(pr);
  psc_mfields_zero(flds, JXI);
  psc_mfields_zero(flds, JYI);
  psc_mfields_zero(flds, JZI);

  psc_foreach_patch(ppsc, p) {
    fields_cache_t fld;
    cache_fields_from_em(p, &fld, psc_mfields_get_patch(flds, p));
    struct psc_particles *prts =
      psc_particles_get_as(psc_mparticles_get_patch(particles_base, p), PARTICLE_TYPE, 0);
    do_push_part_1vb_yz(p, &fld, prts);
    psc_particles_put_as(prts, psc_mparticles_get_patch(particles_base, p), 0);
    cache_fields_to_j(p, &fld, psc_mfields_get_patch(flds, p));
  }
  prof_stop(pr);

  psc_mfields_put_cf(flds, flds_base, JXI, JXI + 3);
}

