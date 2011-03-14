
#ifndef VECTOR_PRIVATE_H
#define VECTOR_PRIVATE_H

#include "vector.h"

// ======================================================================
// vector class

struct vector {
  struct mrc_obj obj;
  int nr_elements;
  double *elements;
};

// ======================================================================
// vector subclass

struct vector_ops {
  MRC_SUBCLASS_OPS(struct vector);
};

extern struct vector_ops vector_double_ops;

#endif
