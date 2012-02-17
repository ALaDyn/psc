
#include "psc_bnd_fields_private.h"
#include "psc_fields_as_c.h"

#include "psc.h"
#include <mrc_profile.h>

#define DEBUG

static void
conducting_wall_E_lo(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, EX, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EX, ix, -2,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, -2,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, -2,iz) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EX, ix, 0,iz) =  0.;
	F3(pf, EX, ix,-1,iz) =  F3(pf, EX, ix, 1,iz);
	F3(pf, EY, ix,-1,iz) = -F3(pf, EY, ix, 0,iz);
      }
    }

    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EZ, ix, 0,iz) =  0.;
	F3(pf, EZ, ix,-1,iz) =  F3(pf, EZ, ix, 1,iz);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_E_hi(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, EX, ix, my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EX, ix, my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, my+1,iz) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EX, ix,my  ,iz) = 0.;
	F3(pf, EX, ix,my+1,iz) =  F3(pf, EX, ix, my-1,iz);
	F3(pf, EY, ix,my,iz)   = -F3(pf, EY, ix, my-1,iz);
      }
    }

    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EZ, ix,my,iz) = 0.;
	F3(pf, EZ, ix,my+1,iz) =  F3(pf, EZ, ix, my-1,iz);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_lo(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, HX, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HX, ix, -2,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix, -2,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, -1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, -2,iz) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HY, ix,-1,iz) =  F3(pf, HY, ix, 1,iz);
	F3(pf, HX, ix,-1,iz) = -F3(pf, HX, ix, 0,iz);
      }
    }
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HZ, ix,-1,iz) = -F3(pf, HZ, ix, 0,iz);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_hi(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, HX, ix,my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HX, ix,my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix,my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix,my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix,my+1,iz) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HY, ix,my+1,iz) =  F3(pf, HY, ix, my-1,iz);
	F3(pf, HX, ix,my  ,iz) = -F3(pf, HX, ix, my-1,iz);
      }
    }
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HZ, ix,my  ,iz) = -F3(pf, HZ, ix, my-1,iz);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_lo(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -1; ix < patch->ldims[0] + 1; ix++) {
	F3(pf, JYI, ix, 0,iz) -= F3(pf, JYI, ix,-1,iz);
	F3(pf, JYI, ix,-1,iz) = 0.;
	// FIXME, JXI/JZI?
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_hi(struct psc_bnd_fields *bnd, mfields_t *flds,
		     int p, int d)
{
  fields_t *pf = psc_mfields_get_patch(flds, p);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -1; ix < patch->ldims[0] + 1; ix++) {
	F3(pf, JYI, ix,my-1,iz) -= F3(pf, JYI, ix,my,iz);
	F3(pf, JYI, ix,my  ,iz) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_fields_c_fill_ghosts_E

static void
psc_bnd_fields_c_fill_ghosts_E(struct psc_bnd_fields *bnd, mfields_base_t *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  mfields_t *flds = psc_mfields_get_cf(flds_base, EX, EX + 3);

  psc_foreach_patch(ppsc, p) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_E_lo(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
    // hi
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	switch (ppsc->domain.bnd_fld_hi[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_E_hi(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
  }
  psc_mfields_put_cf(flds, flds_base, EX, EX + 3);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_c_fill_ghosts_H

static void
psc_bnd_fields_c_fill_ghosts_H(struct psc_bnd_fields *bnd, mfields_base_t *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  mfields_t *flds = psc_mfields_get_cf(flds_base, HX, HX + 3);

  psc_foreach_patch(ppsc, p) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_H_lo(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
    // hi
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	switch (ppsc->domain.bnd_fld_hi[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_H_hi(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
  }
  psc_mfields_put_cf(flds, flds_base, HX, HX + 3);
}

static void
psc_bnd_fields_c_add_ghosts_J(struct psc_bnd_fields *bnd, mfields_base_t *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  mfields_t *flds = psc_mfields_get_cf(flds_base, JXI, JXI + 3);

  psc_foreach_patch(ppsc, p) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_J_lo(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
    // hi
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] + ppsc->patch[p].ldims[d] == ppsc->domain.gdims[d]) {
	switch (ppsc->domain.bnd_fld_hi[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_J_hi(bnd, flds, p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
  }
  psc_mfields_put_cf(flds, flds_base, JXI, JXI + 3);
}

// ======================================================================
// psc_bnd_fields: subclass "c"

struct psc_bnd_fields_ops psc_bnd_fields_c_ops = {
  .name                  = "c",
  .fill_ghosts_a_E       = psc_bnd_fields_c_fill_ghosts_E,
  .fill_ghosts_a_H       = psc_bnd_fields_c_fill_ghosts_H,
  .add_ghosts_J          = psc_bnd_fields_c_add_ghosts_J,
};
