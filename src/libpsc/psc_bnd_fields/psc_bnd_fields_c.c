
#include "psc_bnd_fields_private.h"
#include "psc_fields_as_c.h"

#include "psc.h"
#include <mrc_profile.h>

#define DEBUG

static void
conducting_wall_E_lo(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

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
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, EX, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EX, ix, iy, -2) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, iy, -2) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, iy, -2) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EX, ix, iy, 0) =  0.;
	F3(pf, EX, ix, iy,-1) =  F3(pf, EX, ix, iy, 1);
	F3(pf, EZ, ix, iy,-1) = -F3(pf, EZ, ix, iy, 0);
	F3(pf, EZ, ix, iy,-2) = -F3(pf, EZ, ix, iy, 1);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EY, ix, iy, 0) =  0.;
	F3(pf, EY, ix, iy,-1) =  F3(pf, EY, ix, iy, 1);
      }
    }
  } else  {
    assert(0);
  }
}

static void
conducting_wall_E_hi(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

   if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, EX, ix, my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EX, ix, my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, my  ,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, my+1,iz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, my  ,iz) = 0x7ff8000000000000;
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
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, EX, ix, iy, mz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EX, ix, iy, mz+1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, iy, mz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EY, ix, iy, mz+1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, iy, mz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, EZ, ix, iy, mz+1) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EX, ix, iy, mz) = 0.;
	F3(pf, EX, ix, iy, mz+1) =  F3(pf, EX, ix, iy, mz-1);
	F3(pf, EZ, ix, iy, mz)   = -F3(pf, EZ, ix, iy, mz-1);
	F3(pf, EZ, ix, iy, mz+1)   = -F3(pf, EZ, ix, iy, mz-2);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, EY, ix, iy, mz) = 0.;
	F3(pf, EY, ix, iy, mz+1) =  F3(pf, EY, ix, iy, mz-1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_lo(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

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
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, HX, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HX, ix, iy, -2) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix, iy, -2) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, iy, -1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, iy, -2) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HZ, ix, iy,-1) =  F3(pf, HZ, ix, iy, 1);
	F3(pf, HX, ix, iy,-1) = -F3(pf, HX, ix, iy, 0);
	F3(pf, HX, ix, iy,-2) = -F3(pf, HX, ix, iy, 1);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HY, ix, iy,-1) = -F3(pf, HY, ix, iy, 0);
	F3(pf, HY, ix, iy,-2) = -F3(pf, HY, ix, iy, 1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_hi(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

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
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HY, ix,my+1,iz) =  F3(pf, HY, ix, my-1,iz);
	F3(pf, HX, ix,my  ,iz) = -F3(pf, HX, ix, my-1,iz);
      }
    }
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HZ, ix,my  ,iz) = -F3(pf, HZ, ix, my-1,iz);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	*(long int *)&F3(pf, HX, ix, iy, mz) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HX, ix, iy, mz+1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HY, ix, iy, mz+1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, iy, mz+1) = 0x7ff8000000000000;
	*(long int *)&F3(pf, HZ, ix, iy, mz+1) = 0x7ff8000000000000;
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HZ, ix, iy, mz+1) =  F3(pf, HZ, ix, iy, mz-1);
	F3(pf, HX, ix, iy, mz) = -F3(pf, HX, ix, iy, mz-1);
	F3(pf, HX, ix, iy, mz+1) = -F3(pf, HX, ix, iy, mz-2);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, HY, ix, iy, mz) = -F3(pf, HY, ix, iy, mz-1);
	F3(pf, HY, ix, iy, mz+1) = -F3(pf, HY, ix, iy, mz-2);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_lo(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

  if (d == 1) {
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -1; ix < patch->ldims[0] + 1; ix++) {
	F3(pf, JYI, ix, 0,iz) -= F3(pf, JYI, ix,-1,iz);
	F3(pf, JYI, ix,-1,iz) = 0.;
	// FIXME, JXI/JZI?
      }
    }
  } else  if (d == 2) {
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, JZI, ix, iy, 0) -= F3(pf, JZI, ix, iy,-1);
	F3(pf, JZI, ix, iy,-1) = 0.;
	F3(pf, JXI, ix, iy, 1) += F3(pf, JXI, ix, iy,-1);
	F3(pf, JXI, ix, iy,-1) = 0.;
	F3(pf, JYI, ix, iy, 1) += F3(pf, JYI, ix, iy,-1);
	F3(pf, JYI, ix, iy,-1) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_hi(struct psc_bnd_fields *bnd, struct psc_fields *pf, int d)
{
  struct psc_patch *patch = ppsc->patch + pf->p;

  if (d == 1) {
    int my = patch->ldims[1];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = -1; ix < patch->ldims[0] + 1; ix++) {
	F3(pf, JYI, ix,my-1,iz) -= F3(pf, JYI, ix,my,iz);
	F3(pf, JYI, ix,my  ,iz) = 0.;
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(pf, JZI, ix, iy, mz-1) -= F3(pf, JZI, ix, iy,mz);
	F3(pf, JZI, ix, iy, mz) = 0.;
	F3(pf, JXI, ix, iy, mz-1) += F3(pf, JXI, ix, iy,mz+1);
	F3(pf, JXI, ix, iy, mz+1) = 0.;
	F3(pf, JYI, ix, iy, mz-1) += F3(pf, JYI, ix, iy,mz+1);
	F3(pf, JYI, ix, iy, mz+1) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_fields_c_fill_ghosts_E

static void
psc_bnd_fields_c_fill_ghosts_E(struct psc_bnd_fields *bnd, struct psc_fields *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_fields *flds = psc_fields_get_as(flds_base, FIELDS_TYPE, EX, EX + 3);

  // lo
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] == 0) {
      switch (ppsc->domain.bnd_fld_lo[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_E_lo(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }
  // hi
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] + ppsc->patch[flds->p].ldims[d] == ppsc->domain.gdims[d]) {
      switch (ppsc->domain.bnd_fld_hi[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_E_hi(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }
  psc_fields_put_as(flds, flds_base, EX, EX + 3);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_c_fill_ghosts_H

static void
psc_bnd_fields_c_fill_ghosts_H(struct psc_bnd_fields *bnd, struct psc_fields *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_fields *flds = psc_fields_get_as(flds_base, FIELDS_TYPE, HX, HX + 3);

  // lo
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] == 0) {
      switch (ppsc->domain.bnd_fld_lo[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_H_lo(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }
  // hi
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] + ppsc->patch[flds->p].ldims[d] == ppsc->domain.gdims[d]) {
      switch (ppsc->domain.bnd_fld_hi[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_H_hi(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }
  psc_fields_put_as(flds, flds_base, HX, HX + 3);
}

static void
psc_bnd_fields_c_add_ghosts_J(struct psc_bnd_fields *bnd, struct psc_fields *flds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_fields *flds = psc_fields_get_as(flds_base, FIELDS_TYPE, JXI, JXI + 3);

  // lo
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] == 0) {
      switch (ppsc->domain.bnd_fld_lo[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_J_lo(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }
  // hi
  for (int d = 0; d < 3; d++) {
    if (ppsc->patch[flds->p].off[d] + ppsc->patch[flds->p].ldims[d] == ppsc->domain.gdims[d]) {
      switch (ppsc->domain.bnd_fld_hi[d]) {
      case BND_FLD_PERIODIC:
	break;
      case BND_FLD_CONDUCTING_WALL:
	conducting_wall_J_hi(bnd, flds, d);
	break;
      default:
	assert(0);
      }
    }
  }

  psc_fields_put_as(flds, flds_base, JXI, JXI + 3);
}

// ======================================================================
// psc_bnd_fields: subclass "c"

struct psc_bnd_fields_ops psc_bnd_fields_c_ops = {
  .name                  = "c",
  .fill_ghosts_a_E       = psc_bnd_fields_c_fill_ghosts_E,
  .fill_ghosts_a_H       = psc_bnd_fields_c_fill_ghosts_H,
  // OPT fill_ghosts_b_E is probably not needed except for proper output
  // fill_ghosts_b_H: ?
  .fill_ghosts_b_E       = psc_bnd_fields_c_fill_ghosts_E,
  .fill_ghosts_b_H       = psc_bnd_fields_c_fill_ghosts_H,
  .add_ghosts_J          = psc_bnd_fields_c_add_ghosts_J,
};
