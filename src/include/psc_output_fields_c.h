
#ifndef PSC_OUTPUT_FIELDS_C_H
#define PSC_OUTPUT_FIELDS_C_H

#include "psc_output_fields_private.h"
#include "psc_output_fields_item.h"

// ----------------------------------------------------------------------

enum {
  IO_TYPE_PFD,
  IO_TYPE_TFD,
  NR_IO_TYPES,
};

#define MAX_FIELDS_LIST 50

using psc_fields_list = std::vector<psc_mfields*>;

struct psc_output_fields_c {
  char *data_dir;
  char *output_fields;
  char *pfd_s;
  char *tfd_s;
  bool dowrite_pfield, dowrite_tfield;
  int pfield_first, tfield_first;
  int pfield_step, tfield_step;
  int tfield_length;
  int tfield_every;
  int rn[3];
  int rx[3];
	
  int pfield_next, tfield_next;
  // storage for output
  unsigned int naccum;
  psc_fields_list pfd, tfd;
  struct psc_output_fields_item *item[MAX_FIELDS_LIST];
  struct mrc_io *ios[NR_IO_TYPES];

  struct psc_bnd *bnd;
};

#endif
