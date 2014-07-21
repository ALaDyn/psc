
#include "mrc_ddc_private.h"

#include <mrc_domain.h>
#include <mrc_params.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// ======================================================================
// mrc_ddc_amr

struct mrc_ddc_amr_row {
  int patch;
  int idx;
  int first_entry;
};

struct mrc_ddc_amr_entry {
  int patch;
  int idx;
  float val;
};

struct mrc_ddc_amr {
  struct mrc_ddc_amr_row *rows;
  struct mrc_ddc_amr_entry *entries;

  int nr_rows;
  int nr_entries;
  int nr_rows_alloced;
  int nr_entries_alloced;

  struct mrc_domain *domain;
  int sw[3];
  int ib[3], im[3];
};

#define mrc_ddc_amr(ddc) mrc_to_subobj(ddc, struct mrc_ddc_amr)

// ----------------------------------------------------------------------
// mrc_ddc_amr_set_domain

static void
mrc_ddc_amr_set_domain(struct mrc_ddc *ddc, struct mrc_domain *domain)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);
  sub->domain = domain;
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_get_domain

static struct mrc_domain *
mrc_ddc_amr_get_domain(struct mrc_ddc *ddc)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);
  return sub->domain;
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_setup

static void
mrc_ddc_amr_setup(struct mrc_ddc *ddc)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);
  assert(sub->domain);

  int ldims[3];
  mrc_domain_get_param_int3(sub->domain, "m", ldims);
  // needs to be compatible with how mrc_fld indexes its fields
  for (int d = 0; d < 3; d++) {
    sub->ib[d] = -sub->sw[d];
    sub->im[d] = ldims[d] + 2 * sub->sw[d];
  }

  sub->nr_rows_alloced = 1000;
  sub->nr_entries_alloced = 2000;

  sub->rows = calloc(sub->nr_rows_alloced, sizeof(*sub->rows));
  sub->entries = calloc(sub->nr_entries_alloced, sizeof(*sub->entries));

  sub->nr_entries = 0;
  sub->nr_rows = 0;
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_destroy

static void
mrc_ddc_amr_destroy(struct mrc_ddc *ddc)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);

  free(sub->rows);
  free(sub->entries);
}

// ----------------------------------------------------------------------
// mrc_ddc_add_value

void
mrc_ddc_amr_add_value(struct mrc_ddc *ddc,
		      int row_patch, int rowm, int row[3],
		      int col_patch, int colm, int col[3],
		      float val)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);

  // WARNING, all elements for any given row must be added contiguously!

  assert(row_patch >= 0);
  assert(col_patch >= 0);

  int row_idx = (((rowm * sub->im[2] + row[2] - sub->ib[2]) *
		  sub->im[1] + row[1] - sub->ib[1]) *
		 sub->im[0] + row[0] - sub->ib[0]);
  int col_idx = (((colm * sub->im[2] + col[2] - sub->ib[2]) *
		  sub->im[1] + col[1] - sub->ib[1]) *
		 sub->im[0] + col[0] - sub->ib[0]);
  
  if (sub->nr_rows == 0 ||
      sub->rows[sub->nr_rows - 1].idx != row_idx ||
      sub->rows[sub->nr_rows - 1].patch != row_patch) {
    // start new row
    if (sub->nr_rows >= sub->nr_rows_alloced - 1) {
      sub->nr_rows_alloced *= 2;
      sub->rows = realloc(sub->rows, sub->nr_rows_alloced * sizeof(*sub->rows));
    }
    sub->rows[sub->nr_rows].patch = row_patch;
    sub->rows[sub->nr_rows].idx = row_idx;
    sub->rows[sub->nr_rows].first_entry = sub->nr_entries;
    sub->nr_rows++;
  }

  // if we already have an entry for this column in the current row, just add to it
  for (int i = sub->rows[sub->nr_rows - 1].first_entry; i < sub->nr_entries; i++) {
    if (sub->entries[i].patch == col_patch && sub->entries[i].idx == col_idx) {
      sub->entries[i].val += val;
      return;
    }
  }

  // otherwise, need to append a new entry
  if (sub->nr_entries >= sub->nr_entries_alloced) {
    sub->nr_entries_alloced *= 2;
    sub->entries = realloc(sub->entries, sub->nr_entries_alloced * sizeof(*sub->entries));
  }
  sub->entries[sub->nr_entries].patch = col_patch;
  sub->entries[sub->nr_entries].idx = col_idx;
  sub->entries[sub->nr_entries].val = val;
  sub->nr_entries++;
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_assemble

void
mrc_ddc_amr_assemble(struct mrc_ddc *ddc)
{
  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);

  sub->rows[sub->nr_rows].first_entry = sub->nr_entries;
  mprintf("nr_rows %d nr_entries %d\n", sub->nr_rows, sub->nr_entries);
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_fill_ghosts

static void
mrc_ddc_amr_fill_ghosts(struct mrc_ddc *ddc, int mb, int me, void *ctx)
{
  // mb, me is meaningless here, make sure the caller knows
  assert(mb < 0 && me < 0);

  struct mrc_ddc_amr *sub = mrc_ddc_amr(ddc);

  if (ddc->size_of_type == sizeof(float)) {
    float **fldp = ctx;
    
    for (int row = 0; row < sub->nr_rows; row++) {
      int row_patch = sub->rows[row].patch;
      int row_idx = sub->rows[row].idx;
      float sum = 0.;
      for (int entry = sub->rows[row].first_entry;
	   entry < sub->rows[row + 1].first_entry; entry++) {
	int col_patch =  sub->entries[entry].patch;
	int col_idx = sub->entries[entry].idx;
	float val = sub->entries[entry].val;
	sum += val * fldp[col_patch][col_idx];
      }
      fldp[row_patch][row_idx] = sum;
    }
  } else if (ddc->size_of_type == sizeof(double)) {
    // FIXME, should have the coefficient ("val") as double, too, to avoid
    // conversions
    double **fldp = ctx;

    for (int row = 0; row < sub->nr_rows; row++) {
      int row_patch = sub->rows[row].patch;
      int row_idx = sub->rows[row].idx;
      float sum = 0.;
      for (int entry = sub->rows[row].first_entry;
	   entry < sub->rows[row + 1].first_entry; entry++) {
	int col_patch =  sub->entries[entry].patch;
	int col_idx = sub->entries[entry].idx;
	double val = sub->entries[entry].val;
	sum += val * fldp[col_patch][col_idx];
      }
      fldp[row_patch][row_idx] = sum;
    }
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_add_ghosts

static void
mrc_ddc_amr_add_ghosts(struct mrc_ddc *ddc, int mb, int me, void *ctx)
{
  // mb, me is meaningless here, make sure the caller knows
  assert(mb < 0 && me < 0);
}

// ----------------------------------------------------------------------
// mrc_ddc_amr_apply

void
mrc_ddc_amr_apply(struct mrc_ddc *ddc, struct mrc_fld *fld)
{
  assert(ddc->size_of_type == sizeof(float));

  float **fldp = malloc(mrc_fld_nr_patches(fld) * sizeof(*fldp));
  for (int p = 0; p < mrc_fld_nr_patches(fld); p++) {
    struct mrc_fld_patch *m3p = mrc_fld_patch_get(fld, p);
    fldp[p] = &MRC_M3(m3p, 0, fld->_ghost_offs[0], fld->_ghost_offs[1], fld->_ghost_offs[2]);
  }
  mrc_ddc_amr_fill_ghosts(ddc, -1, -1, fldp);

  free(fldp);
}

// ----------------------------------------------------------------------

#define VAR(x) (void *)offsetof(struct mrc_ddc_amr, x)
static struct param mrc_ddc_amr_descr[] = {
  { "sw"                     , VAR(sw)                      , PARAM_INT3(0, 0, 0)    },
  {},
};
#undef VAR

// ======================================================================
// mrc_ddc_amr_ops

struct mrc_ddc_ops mrc_ddc_amr_ops = {
  .name                  = "amr",
  .size                  = sizeof(struct mrc_ddc_amr),
  .param_descr           = mrc_ddc_amr_descr,
  .setup                 = mrc_ddc_amr_setup,
  .destroy               = mrc_ddc_amr_destroy,
  .set_domain            = mrc_ddc_amr_set_domain,
  .get_domain            = mrc_ddc_amr_get_domain,
  .fill_ghosts           = mrc_ddc_amr_fill_ghosts,
  .add_ghosts            = mrc_ddc_amr_add_ghosts,
};

