
#ifndef PSC_OUTPUT_FIELDS_C_H
#define PSC_OUTPUT_FIELDS_C_H

#include "psc_output_fields_private.h"

#define MAX_FIELDS_LIST 30

struct psc_fields_list {
  int nr_flds;
  mfields_c_t *flds[MAX_FIELDS_LIST];
};

struct psc_output_fields_c {
  char *data_dir;
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

  struct psc_output_format *format;
};

// ======================================================================

extern struct _psc_output_format_ops psc_output_format_ops_mrc;
extern struct _psc_output_format_ops psc_output_format_ops_binary;
extern struct _psc_output_format_ops psc_output_format_ops_hdf5;
extern struct _psc_output_format_ops psc_output_format_ops_xdmf;
extern struct _psc_output_format_ops psc_output_format_ops_vtk;
extern struct _psc_output_format_ops psc_output_format_ops_vtk_points;
extern struct _psc_output_format_ops psc_output_format_ops_vtk_cells;
extern struct _psc_output_format_ops psc_output_format_ops_vtk_binary;

void write_fields_combine(struct psc_fields_list *list, 
			  void (*write_field)(void *ctx, mfields_c_t *fld),
			  void *ctx);



#endif
