
#ifndef MRC_TS_PRIVATE_H
#define MRC_TS_PRIVATE_H

#include <mrc_ts.h>

#include <mrc_io.h>
#include <stdio.h>

struct mrc_ts {
  struct mrc_obj obj;
  // parameters
  int out_every;
  int diag_every;
  int max_steps;
  char *diag_filename;

  struct mrc_io *io; // for writing state output
  int n; // current timestep number
  float dt; // current dt
  struct mrc_f1 *x; // current state vector
  void *ctx; // FIXME, should be mrc_obj?
  void (*rhsf)(void *ctx, struct mrc_f1 *x, struct mrc_f1 *rhs);
  FILE *f_diag;
  void (*diagf)(void *ctx, float time, struct mrc_f1 *x, FILE *file);

  // RK2
  struct mrc_f1 *rhs;
  struct mrc_f1 *xm;
};

#endif
