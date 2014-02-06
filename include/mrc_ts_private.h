
#ifndef MRC_TS_PRIVATE_H
#define MRC_TS_PRIVATE_H

#include <mrc_ts.h>

#include <stdio.h>

struct mrc_ts {
  struct mrc_obj obj;
  // parameters
  int max_steps;
  float max_time;

  int n; // current timestep number
  float time; // current integration time
  float dt; // current dt
  struct mrc_obj *x; // current state vector
  struct mrc_obj *ctx_obj;
  void *rhsf_ctx;
  void (*rhsf)(void *ctx, struct mrc_obj *rhs, float time, struct mrc_obj *x);

  struct mrc_obj *(*vec_duplicate)(struct mrc_obj *);
  void (*vec_copy)(struct mrc_obj *, struct mrc_obj *);
  void (*vec_axpy)(struct mrc_obj *, float, struct mrc_obj *);
  void (*vec_waxpy)(struct mrc_obj *, float, struct mrc_obj *, struct mrc_obj *);
  float (*vec_norm)(struct mrc_obj *);
  void (*vec_set)(struct mrc_obj *, float);

  list_t monitors;

  int nr_rhsf_evals; // statistics
};

// ----------------------------------------------------------------------
// helpers for subclasses

void mrc_ts_rhsf(struct mrc_ts *ts, struct mrc_obj *rhs, float time,
		 struct mrc_obj *x);
void mrc_ts_monitors_run(struct mrc_ts *ts);

// ----------------------------------------------------------------------
// basic vector ops

// FIXME, should be standard mrc_obj functionality
static inline struct mrc_obj *
mrc_ts_vec_duplicate(struct mrc_ts *ts, struct mrc_obj *vec)
{
  return ts->vec_duplicate(vec);
}

static inline void
mrc_ts_vec_copy(struct mrc_ts *ts, struct mrc_obj *vecx, struct mrc_obj *vecy)
{
  ts->vec_copy(vecx, vecy);
}

static inline void
mrc_ts_vec_axpy(struct mrc_ts *ts, struct mrc_obj *vecy, float alpha,
		struct mrc_obj *vecx)
{
  ts->vec_axpy(vecy, alpha, vecx);
}

static inline void
mrc_ts_vec_waxpy(struct mrc_ts *ts, struct mrc_obj *vecw, float alpha,
		 struct mrc_obj *vecx, struct mrc_obj *vecy)
{
  ts->vec_waxpy(vecw, alpha, vecx, vecy);
}

static inline float
mrc_ts_vec_norm(struct mrc_ts *ts, struct mrc_obj *vec)
{
  return ts->vec_norm(vec);
}

static inline void
mrc_ts_vec_set(struct mrc_ts *ts, struct mrc_obj *vec, float val)
{
  return ts->vec_set(vec, val);
}

// ----------------------------------------------------------------------

struct mrc_ts_ops {
  MRC_SUBCLASS_OPS(struct mrc_ts);
  void (*step)(struct mrc_ts *);
  void (*solve)(struct mrc_ts *);
};


#ifdef HAVE_PETSC
#include <petscts.h>
// mrc_ts_petsc - struct layout needed by any monitor that hooks into petsc
// ----------------------------------------------------------------------

// libmrc doesn't understand the distinction between global and local
// vectors, but petsc does, and any implicit solver will require
// a global vector to come in, so we actually need to copy the state
// vector in a hacky localtoglobal and feed that in.
struct mrc_ts_petsc {
  TS petsc_ts; // Petsc timestepper object 
  struct mrc_fld *F; // Computed RHS function will be stored here
  Vec F_vec;
  struct mrc_fld *xg; // Global (no ghost) version of the state vector
  Vec xg_vec;
  struct mrc_fld *xlocal; // temp X local for use in the calc_rhs wrapper.
  Mat J; // Jacobian Matrix.
  int debug_rhs;
  int (*calc_jac)(Mat J, Vec x, float t, void *ctx);
  int (*get_jac_mat)(void *ctx, Mat *M);
  void *jacf_ctx;
};

extern struct mrc_ts_ops mrc_ts_petsc_ops;

#endif

extern struct mrc_ts_ops mrc_ts_rk2_ops;
extern struct mrc_ts_ops mrc_ts_rk4_ops;
extern struct mrc_ts_ops mrc_ts_rkf45_ops;
extern struct mrc_ts_ops mrc_ts_ode45_ops;


#endif
