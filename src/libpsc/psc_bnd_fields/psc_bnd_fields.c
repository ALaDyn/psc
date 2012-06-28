
#include "psc_bnd_fields_private.h"
#include "psc_pulse.h"
#include "psc_fields_as_c.h"

// ----------------------------------------------------------------------
// psc_bnd_fields_create

static void
_psc_bnd_fields_create(struct psc_bnd_fields *bnd)
{
  bnd->pulse_x1 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_x1, "pulse_x1");
  psc_pulse_set_param_double3(bnd->pulse_x1, "k",  (double[3]) {  1., 0., 0.});

  bnd->pulse_x2 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_x2, "pulse_x2");
  psc_pulse_set_param_double3(bnd->pulse_x2, "k",  (double[3]) { -1., 0., 0.});

  bnd->pulse_y1 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_y1, "pulse_y1");
  psc_pulse_set_param_double3(bnd->pulse_y1, "k",  (double[3]) { 0.,  1., 0.});

  bnd->pulse_y2 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_y2, "pulse_y2");
  psc_pulse_set_param_double3(bnd->pulse_y2, "k",  (double[3]) { 0., -1., 0.});

  bnd->pulse_z1 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_z1, "pulse_z1");
  psc_pulse_set_param_double3(bnd->pulse_z1, "k",  (double[3]) { 0., 0.,  1.});

  bnd->pulse_z2 = psc_pulse_create(psc_bnd_fields_comm(bnd));
  psc_pulse_set_name(bnd->pulse_z2, "pulse_z2");
  psc_pulse_set_param_double3(bnd->pulse_z2, "k",  (double[3]) { 0., 0., -1.});
}

// ----------------------------------------------------------------------
// psc_bnd_fields_set_from_options

static void
_psc_bnd_fields_set_from_options(struct psc_bnd_fields *bnd)
{
  psc_pulse_set_from_options(bnd->pulse_x1);
  psc_pulse_set_from_options(bnd->pulse_x2);
  psc_pulse_set_from_options(bnd->pulse_y1);
  psc_pulse_set_from_options(bnd->pulse_y2);
  psc_pulse_set_from_options(bnd->pulse_z1);
  psc_pulse_set_from_options(bnd->pulse_z2);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_setup

static void
_psc_bnd_fields_setup(struct psc_bnd_fields *bnd)
{
  psc_pulse_setup(bnd->pulse_x1);
  psc_pulse_setup(bnd->pulse_x2);
  psc_pulse_setup(bnd->pulse_y1);
  psc_pulse_setup(bnd->pulse_y2);
  psc_pulse_setup(bnd->pulse_z1);
  psc_pulse_setup(bnd->pulse_z2);

  psc_bnd_fields_setup_fields(bnd, ppsc->flds);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_view

static void
_psc_bnd_fields_view(struct psc_bnd_fields *bnd)
{
  psc_pulse_view(bnd->pulse_x1);
  psc_pulse_view(bnd->pulse_x2);
  psc_pulse_view(bnd->pulse_y1);
  psc_pulse_view(bnd->pulse_y2);
  psc_pulse_view(bnd->pulse_z1);
  psc_pulse_view(bnd->pulse_z2);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_destroy

static void
_psc_bnd_fields_destroy(struct psc_bnd_fields *bnd)
{
  psc_pulse_destroy(bnd->pulse_x1);
  psc_pulse_destroy(bnd->pulse_x2);
  psc_pulse_destroy(bnd->pulse_y1);
  psc_pulse_destroy(bnd->pulse_y2);
  psc_pulse_destroy(bnd->pulse_z1);
  psc_pulse_destroy(bnd->pulse_z2);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_get_pulse_*

struct psc_pulse *
psc_bnd_fields_get_pulse_x1(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_x1;
}

struct psc_pulse *
psc_bnd_fields_get_pulse_x2(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_x2;
}

struct psc_pulse *
psc_bnd_fields_get_pulse_y1(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_y1;
}

struct psc_pulse *
psc_bnd_fields_get_pulse_y2(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_y2;
}

struct psc_pulse *
psc_bnd_fields_get_pulse_z1(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_z1;
}

struct psc_pulse *
psc_bnd_fields_get_pulse_z2(struct psc_bnd_fields *bnd)
{
  return bnd->pulse_z2;
}

// ======================================================================
// forward to subclass

void
psc_bnd_fields_fill_ghosts_a_E(struct psc_bnd_fields *bnd, mfields_base_t *flds)
{
  struct psc_bnd_fields_ops *ops = psc_bnd_fields_ops(bnd);
  if (ops->fill_ghosts_a_E) {
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->fill_ghosts_a_E(bnd, psc_mfields_get_patch(flds, p));
    }
  }
}

void
psc_bnd_fields_fill_ghosts_a_H(struct psc_bnd_fields *bnd, mfields_base_t *flds)
{
  struct psc_bnd_fields_ops *ops = psc_bnd_fields_ops(bnd);
  if (ops->fill_ghosts_a_H) {
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->fill_ghosts_a_H(bnd, psc_mfields_get_patch(flds, p));
    }
  }
}

void
psc_bnd_fields_fill_ghosts_b_H(struct psc_bnd_fields *bnd, mfields_base_t *flds)
{
  struct psc_bnd_fields_ops *ops = psc_bnd_fields_ops(bnd);
  if (ops->fill_ghosts_b_H) {
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->fill_ghosts_b_H(bnd, psc_mfields_get_patch(flds, p));
    }
  }
}

void
psc_bnd_fields_fill_ghosts_b_E(struct psc_bnd_fields *bnd, mfields_base_t *flds)
{
  struct psc_bnd_fields_ops *ops = psc_bnd_fields_ops(bnd);
  if (ops->fill_ghosts_b_E) {
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->fill_ghosts_b_E(bnd, psc_mfields_get_patch(flds, p));
    }
  }
}

void
psc_bnd_fields_add_ghosts_J(struct psc_bnd_fields *bnd, mfields_base_t *flds)
{
  struct psc_bnd_fields_ops *ops = psc_bnd_fields_ops(bnd);
  if (ops->add_ghosts_J) {
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->add_ghosts_J(bnd, psc_mfields_get_patch(flds, p));
    }
  }
}

void
_psc_bnd_fields_setup_patch(struct psc_bnd_fields *bnd_fields, int p,
			    fields_t *pf_base, double t)
{
  struct psc_pulse *pulse;
  struct psc *psc = ppsc;

  struct psc_fields *pf = psc_fields_get_as(pf_base, FIELDS_TYPE, EX, HX + 3);
  psc_foreach_3d_g(psc, p, jx, jy, jz){
    double dx = psc->dx[0], dy = psc->dx[1], dz = psc->dx[2];
    double xx = CRDX(p, jx), yy = CRDY(p, jy), zz = CRDZ(p, jz);
    
    pulse = psc_bnd_fields_get_pulse_x1(bnd_fields);
    F3(pf, EZ, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx        , yy        , zz + .5*dz, t);
    F3(pf, HY, jx,jy,jz) += -psc_pulse_field_p(pulse, xx + .5*dx, yy        , zz + .5*dz, t);
    F3(pf, EY, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx        , yy + .5*dy, zz        , t);
    F3(pf, HZ, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx + .5*dx, yy + .5*dy, zz        , t);
    
    pulse = psc_bnd_fields_get_pulse_x2(bnd_fields);
    F3(pf, EZ, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx        , yy        , zz + .5*dz, t);
    F3(pf, HY, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx + .5*dx, yy        , zz + .5*dz, t);
    F3(pf, EY, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx        , yy + .5*dy, zz        , t);
    F3(pf, HZ, jx,jy,jz) += -psc_pulse_field_s(pulse, xx + .5*dx, yy + .5*dy, zz        , t);
    
    pulse = psc_bnd_fields_get_pulse_y1(bnd_fields);
    F3(pf, EX, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx + .5*dx, yy        , zz        , t);
    F3(pf, HZ, jx,jy,jz) += -psc_pulse_field_p(pulse, xx + .5*dx, yy + .5*dy, zz        , t);
    F3(pf, EZ, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx        , yy        , zz + .5*dz, t);
    F3(pf, HX, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx        , yy + .5*dy, zz + .5*dz, t);
    
    pulse = psc_bnd_fields_get_pulse_y2(bnd_fields);
    F3(pf, EX, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx + .5*dx, yy        , zz        , t);
    F3(pf, HZ, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx + .5*dx, yy + .5*dy, zz        , t);
    F3(pf, EZ, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx        , yy        , zz + .5*dz, t);
    F3(pf, HX, jx,jy,jz) += -psc_pulse_field_s(pulse, xx        , yy + .5*dy, zz + .5*dz, t);
    
    pulse = psc_bnd_fields_get_pulse_z1(bnd_fields);
    F3(pf, EY, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx        , yy + .5*dy, zz        , t);
    F3(pf, HX, jx,jy,jz) += -psc_pulse_field_p(pulse, xx        , yy + .5*dy, zz + .5*dz, t);
    F3(pf, EX, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx + .5*dx, yy        , zz        , t);
    F3(pf, HY, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx + .5*dx, yy        , zz + .5*dz, t);
    
    pulse = psc_bnd_fields_get_pulse_z2(bnd_fields);
    F3(pf, EY, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx        , yy + .5*dy, zz        , t);
    F3(pf, HX, jx,jy,jz) +=  psc_pulse_field_p(pulse, xx        , yy + .5*dy, zz + .5*dz, t);
    F3(pf, EX, jx,jy,jz) +=  psc_pulse_field_s(pulse, xx + .5*dx, yy        , zz        , t);
    F3(pf, HY, jx,jy,jz) += -psc_pulse_field_s(pulse, xx + .5*dx, yy        , zz + .5*dz, t);
  } psc_foreach_3d_g_end;
  psc_fields_put_as(pf, pf_base, EX, HX + 3);
}

void
psc_bnd_fields_setup_patch(struct psc_bnd_fields *bnd_fields, int p,
			   mfields_base_t *flds_base, double t)
{
  _psc_bnd_fields_setup_patch(bnd_fields, p, psc_mfields_get_patch(flds_base, p), t);
}

void
psc_bnd_fields_setup_fields(struct psc_bnd_fields *bnd_fields, mfields_base_t *flds_base)
{
  psc_foreach_patch(ppsc, p) {
    _psc_bnd_fields_setup_patch(bnd_fields, p, psc_mfields_get_patch(flds_base, p), 0.);
  }
}

// ======================================================================
// psc_bnd_fields_init

static void
psc_bnd_fields_init()
{
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_auto_ops);
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_fortran_ops);
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_none_ops);
#ifdef USE_CUDA
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_bnd_fields, &psc_bnd_fields_mix_ops);
#endif
}

// ======================================================================
// psc_bnd_fields class

struct mrc_class_psc_bnd_fields mrc_class_psc_bnd_fields = {
  .name             = "psc_bnd_fields",
  .size             = sizeof(struct psc_bnd_fields),
  .init             = psc_bnd_fields_init,
  .create           = _psc_bnd_fields_create,
  .destroy          = _psc_bnd_fields_destroy,
  .set_from_options = _psc_bnd_fields_set_from_options,
  .setup            = _psc_bnd_fields_setup,
  .view             = _psc_bnd_fields_view,
};

