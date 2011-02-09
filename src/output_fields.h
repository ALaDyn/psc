
#ifndef OUTPUT_FIELDS_H
#define OUTPUT_FIELDS_H

#include "psc.h"

#define MAX_FIELDS_LIST 30

struct psc_fields_list {
  int nr_flds;
  fields_base_t flds[MAX_FIELDS_LIST];
};

struct psc_output_c;

struct psc_output_format_ops {
  const char *name;
  void (*create)(void);
  void (*destroy)(void);
  void (*write_fields)(struct psc_output_c *out, struct psc_fields_list *flds,
		       const char *prefix);
};

extern struct psc_output_format_ops psc_output_format_ops_binary;
extern struct psc_output_format_ops psc_output_format_ops_hdf5;
extern struct psc_output_format_ops psc_output_format_ops_xdmf;
extern struct psc_output_format_ops psc_output_format_ops_vtk;
extern struct psc_output_format_ops psc_output_format_ops_vtk_points;
extern struct psc_output_format_ops psc_output_format_ops_vtk_cells;
extern struct psc_output_format_ops psc_output_format_ops_vtk_binary;

struct psc_output_c {
  char *data_dir;
  char *output_format;
  char *output_fields;
  bool dowrite_pfield, dowrite_tfield;
  int pfield_first, tfield_first;
  int pfield_step, tfield_step;
  int rn[3];
  int rx[3];
	
  int pfield_next, tfield_next;
  // storage for output
  unsigned int naccum;
  struct psc_fields_list pfd, tfd;
  struct output_field *out_flds[MAX_FIELDS_LIST];

  struct psc_output_format_ops *format_ops;
};

void write_fields_combine(struct psc_fields_list *list, 
			  void (*write_field)(void *ctx, fields_base_t *fld),
			  void *ctx);

#endif
