
#include "psc_push_fields_private.h"

#include "psc_bnd.h"
#include "psc_bnd_fields.h"
#include <mrc_profile.h>
#include <mrc_params.h>

static void
_psc_push_fields_create(struct psc_push_fields *push)
{
  push->bnd_fields = psc_bnd_fields_create(psc_push_fields_comm(push));
  psc_push_fields_add_child(push, (struct mrc_obj *) push->bnd_fields);
}

struct psc_bnd_fields *
psc_push_fields_get_bnd_fields(struct psc_push_fields *push)
{
  return push->bnd_fields;
}

// ======================================================================
// forward to subclass

static void
psc_push_fields_push_E(struct psc_push_fields *push, mfields_base_t *flds)
{
  struct psc_push_fields_ops *ops = psc_push_fields_ops(push);
  static int pr;
  if (!pr) {
    pr = prof_register("push_fields_E", 1., 0, 0);
  }
  assert(ops->push_E);
  
  psc_stats_start(st_time_field);
  prof_start(pr);
  for (int p = 0; p < flds->nr_patches; p++) {
    ops->push_E(push, psc_mfields_get_patch(flds, p));
  }
  prof_stop(pr);
  psc_stats_start(st_time_field);
}

static void
psc_push_fields_push_H(struct psc_push_fields *push, mfields_base_t *flds)
{
  struct psc_push_fields_ops *ops = psc_push_fields_ops(push);
  static int pr;
  if (!pr) {
    pr = prof_register("push_fields_H", 1., 0, 0);
  }
  assert(ops->push_H);
  
  psc_stats_start(st_time_field);
  prof_start(pr);
  for (int p = 0; p < flds->nr_patches; p++) {
    ops->push_H(push, psc_mfields_get_patch(flds, p));
  }
  prof_stop(pr);
  psc_stats_start(st_time_field);
}

// ======================================================================
// default
// 
// That's the traditional way of how things have been done

static void
psc_push_fields_step_a_default(struct psc_push_fields *push, mfields_base_t *flds)
{
  psc_push_fields_push_E(push, flds);
  psc_bnd_fields_fill_ghosts_a_E(push->bnd_fields, flds);
  psc_bnd_fill_ghosts(ppsc->bnd, flds, EX, EX + 3);
  
  psc_push_fields_push_H(push, flds);
  psc_bnd_fields_fill_ghosts_a_H(push->bnd_fields, flds);
  psc_bnd_fill_ghosts(ppsc->bnd, flds, HX, HX + 3);
}

static void
psc_push_fields_step_b1_default(struct psc_push_fields *push, mfields_base_t *flds)
{
}

static void
psc_push_fields_step_b2_default(struct psc_push_fields *push, mfields_base_t *flds)
{
  psc_push_fields_push_H(push, flds);
  psc_bnd_fields_fill_ghosts_b_H(push->bnd_fields, flds);
  psc_bnd_fill_ghosts(ppsc->bnd, flds, HX, HX + 3);
  
  psc_bnd_fields_add_ghosts_J(push->bnd_fields, flds);
  psc_bnd_add_ghosts(ppsc->bnd, flds, JXI, JXI + 3);
  psc_bnd_fill_ghosts(ppsc->bnd, flds, JXI, JXI + 3);
  
  psc_push_fields_push_E(push, flds);
  psc_bnd_fields_fill_ghosts_b_E(push->bnd_fields, flds);
  psc_bnd_fill_ghosts(ppsc->bnd, flds, EX, EX + 3);
}

// ======================================================================
// opt
//
// This way does only the minimum amount of communication needed,
// and does all the communication (including particles) at the same time

static void
psc_push_fields_step_a_opt(struct psc_push_fields *push, struct psc_mfields *mflds)
{
  psc_push_fields_push_E(push, mflds);
  psc_bnd_fields_fill_ghosts_a_E(push->bnd_fields, mflds);

  psc_push_fields_push_H(push, mflds);
  psc_bnd_fields_fill_ghosts_a_H(push->bnd_fields, mflds);
}

static void
psc_push_fields_step_b1_opt(struct psc_push_fields *push, struct psc_mfields *mflds)
{
  psc_push_fields_push_H(push, mflds);
}

static void
psc_push_fields_step_b2_opt(struct psc_push_fields *push, struct psc_mfields *mflds)
{
  // fill ghosts for H
  psc_bnd_fields_fill_ghosts_b_H(push->bnd_fields, mflds);
  psc_bnd_fill_ghosts(ppsc->bnd, mflds, HX, HX + 3);
  
  // add and fill ghost for J
  psc_bnd_fields_add_ghosts_J(push->bnd_fields, mflds);
  psc_bnd_add_ghosts(ppsc->bnd, mflds, JXI, JXI + 3);
  psc_bnd_fill_ghosts(ppsc->bnd, mflds, JXI, JXI + 3);

  psc_push_fields_push_E(push, mflds);
  psc_bnd_fields_fill_ghosts_b_E(push->bnd_fields, mflds);
}

// ======================================================================

void
psc_push_fields_step_a(struct psc_push_fields *push, mfields_base_t *flds)
{
  if (ppsc->domain.use_pml) {
    // FIXME, pml routines sehould be split into E, H push + ghost points, too
    // pml could become a separate push_fields subclass
    struct psc_push_fields_ops *ops = psc_push_fields_ops(push);
    assert(ops->pml_a);
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->pml_a(push, psc_mfields_get_patch(flds, p));
    }
  } else if (push->variant == 0) {
    psc_push_fields_step_a_default(push, flds);
  } else if (push->variant == 1) {
    psc_push_fields_step_a_opt(push, flds);
  } else {
    assert(0);
  }
}

void
psc_push_fields_step_b1(struct psc_push_fields *push, mfields_base_t *flds)
{
  if (ppsc->domain.use_pml) {
  } else if (push->variant == 0) {
    psc_push_fields_step_b1_default(push, flds);
  } else if (push->variant == 1) {
    psc_push_fields_step_b1_opt(push, flds);
  } else {
    assert(0);
  }
}

void
psc_push_fields_step_b2(struct psc_push_fields *push, mfields_base_t *flds)
{
  if (ppsc->domain.use_pml) {
    struct psc_push_fields_ops *ops = psc_push_fields_ops(push);
    assert(ops->pml_b);
    for (int p = 0; p < flds->nr_patches; p++) {
      ops->pml_b(push, psc_mfields_get_patch(flds, p));
    }
  } else if (push->variant == 0) {
    psc_push_fields_step_b2_default(push, flds);
  } else if (push->variant == 1) {
    psc_push_fields_step_b2_opt(push, flds);
  } else {
    assert(0);
  }
}

// ======================================================================
// psc_push_fields_init

static void
psc_push_fields_init()
{
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_auto_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_c_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_single_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_fortran_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_none_ops);
#ifdef USE_CBE
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_cbe_ops);
#endif
#ifdef USE_CUDA
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_cuda_ops);
  mrc_class_register_subclass(&mrc_class_psc_push_fields, &psc_push_fields_mix_ops);
#endif
}

// ======================================================================

#define VAR(x) (void *)offsetof(struct psc_push_fields, x)
static struct param psc_push_fields_descr[] = {
  { "variant"          , VAR(variant)          , PARAM_INT(0),
  .help = "selects different variants of the EM solver: "
  "(0): default (traditional way with 4 fill_ghosts()) "
  "(1): optimized with only 1 fill_ghosts(), 1st order "
  "particle shape only" },
  {},
};
#undef VAR

// ======================================================================
// psc_push_fields class

struct mrc_class_psc_push_fields mrc_class_psc_push_fields = {
  .name             = "psc_push_fields",
  .size             = sizeof(struct psc_push_fields),
  .param_descr      = psc_push_fields_descr,
  .init             = psc_push_fields_init,
  .create           = _psc_push_fields_create,
};

