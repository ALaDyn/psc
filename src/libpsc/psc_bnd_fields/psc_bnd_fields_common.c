
#include "psc.h"
#include "psc_debug.h"

#include <mrc_bits.h>

//#define DEBUG

#define F3(flds, m, i,j,k) _F3(flds, m, i,0,k)

static void
conducting_wall_E_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, pf->ib[0]); ix < MIN(patch->ldims[0] + 2, pf->ib[0] + pf->im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, EX, ix, -1,iz));
	fields_t_set_nan(&F3(flds, EX, ix, -2,iz));
	fields_t_set_nan(&F3(flds, EY, ix, -1,iz));
	fields_t_set_nan(&F3(flds, EY, ix, -2,iz));
	fields_t_set_nan(&F3(flds, EZ, ix, -1,iz));
	fields_t_set_nan(&F3(flds, EZ, ix, -2,iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      // FIXME, needs to be for other dir, too, and it's ugly
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, EX, ix, 0,iz) =  0.;
	F3(flds, EX, ix,-1,iz) =  F3(flds, EX, ix, 1,iz);
	F3(flds, EX, ix,-2,iz) =  F3(flds, EX, ix, 2,iz);
	F3(flds, EY, ix,-1,iz) = -F3(flds, EY, ix, 0,iz);
	F3(flds, EY, ix,-2,iz) = -F3(flds, EY, ix, 1,iz);
      }
    }

    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, EZ, ix, 0,iz) =  0.;
	F3(flds, EZ, ix,-1,iz) =  F3(flds, EZ, ix, 1,iz);
	F3(flds, EZ, ix,-2,iz) =  F3(flds, EZ, ix, 2,iz);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F3(flds, EX, ix, iy, -1));
	fields_t_set_nan(&F3(flds, EX, ix, iy, -2));
	fields_t_set_nan(&F3(flds, EY, ix, iy, -1));
	fields_t_set_nan(&F3(flds, EY, ix, iy, -2));
	fields_t_set_nan(&F3(flds, EZ, ix, iy, -1));
	fields_t_set_nan(&F3(flds, EZ, ix, iy, -2));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, EX, ix, iy, 0) =  0.;
	F3(flds, EX, ix, iy,-1) =  F3(flds, EX, ix, iy, 1);
	F3(flds, EZ, ix, iy,-1) = -F3(flds, EZ, ix, iy, 0);
	F3(flds, EZ, ix, iy,-2) = -F3(flds, EZ, ix, iy, 1);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, EY, ix, iy, 0) =  0.;
	F3(flds, EY, ix, iy,-1) =  F3(flds, EY, ix, iy, 1);
      }
    }
  } else  {
    assert(0);
  }
}

static void
conducting_wall_E_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

   if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
 	fields_t_set_nan(&F3(flds, EX, ix, my  , iz));
	fields_t_set_nan(&F3(flds, EX, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, EY, ix, my  , iz));
	fields_t_set_nan(&F3(flds, EY, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, EZ, ix, my  , iz));
	fields_t_set_nan(&F3(flds, EZ, ix, my+1, iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, EX, ix,my  ,iz) = 0.;
	F3(flds, EX, ix,my+1,iz) =  F3(flds, EX, ix, my-1,iz);
	F3(flds, EY, ix,my  ,iz) = -F3(flds, EY, ix, my-1,iz);
      }
    }

    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, EZ, ix,my  ,iz) = 0.;
	F3(flds, EZ, ix,my+1,iz) =  F3(flds, EZ, ix, my-1,iz);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F3(flds, EX, ix, iy, mz));
	fields_t_set_nan(&F3(flds, EX, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, EY, ix, iy, mz));
	fields_t_set_nan(&F3(flds, EY, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, EZ, ix, iy, mz));
	fields_t_set_nan(&F3(flds, EZ, ix, iy, mz+1));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, EX, ix, iy, mz) = 0.;
	F3(flds, EX, ix, iy, mz+1) =  F3(flds, EX, ix, iy, mz-1);
	F3(flds, EZ, ix, iy, mz)   = -F3(flds, EZ, ix, iy, mz-1);
	F3(flds, EZ, ix, iy, mz+1)   = -F3(flds, EZ, ix, iy, mz-2);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, EY, ix, iy, mz) = 0.;
	F3(flds, EY, ix, iy, mz+1) =  F3(flds, EY, ix, iy, mz-1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, -1,iz));
	fields_t_set_nan(&F3(flds, HX, ix, -2,iz));
	fields_t_set_nan(&F3(flds, HY, ix, -1,iz));
	fields_t_set_nan(&F3(flds, HY, ix, -2,iz));
	fields_t_set_nan(&F3(flds, HZ, ix, -1,iz));
	fields_t_set_nan(&F3(flds, HZ, ix, -2,iz));
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HY, ix,-1,iz) =  F3(flds, HY, ix, 1,iz);
	F3(flds, HX, ix,-1,iz) = -F3(flds, HX, ix, 0,iz);
      }
    }
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HZ, ix,-1,iz) = -F3(flds, HZ, ix, 0,iz);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HX, ix, iy, -2));
	fields_t_set_nan(&F3(flds, HY, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HY, ix, iy, -2));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, -2));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, HZ, ix, iy,-1) =  F3(flds, HZ, ix, iy, 1);
	F3(flds, HX, ix, iy,-1) = -F3(flds, HX, ix, iy, 0);
	F3(flds, HX, ix, iy,-2) = -F3(flds, HX, ix, iy, 1);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, HY, ix, iy,-1) = -F3(flds, HY, ix, iy, 0);
	F3(flds, HY, ix, iy,-2) = -F3(flds, HY, ix, iy, 1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, my  , iz));
	fields_t_set_nan(&F3(flds, HX, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, HY, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, HZ, ix, my  , iz));
	fields_t_set_nan(&F3(flds, HZ, ix, my+1, iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HY, ix,my+1,iz) =  F3(flds, HY, ix, my-1,iz);
	F3(flds, HX, ix,my  ,iz) = -F3(flds, HX, ix, my-1,iz);
	F3(flds, HX, ix,my+1,iz) = -F3(flds, HX, ix, my-2,iz);
      }
    }
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HZ, ix,my  ,iz) = -F3(flds, HZ, ix, my-1,iz);
	F3(flds, HZ, ix,my+1,iz) = -F3(flds, HZ, ix, my-2,iz);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, iy, mz  ));
	fields_t_set_nan(&F3(flds, HX, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, HY, ix, iy, mz  ));
	fields_t_set_nan(&F3(flds, HY, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, mz+1));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, HZ, ix, iy, mz+1) =  F3(flds, HZ, ix, iy, mz-1);
	F3(flds, HX, ix, iy, mz) = -F3(flds, HX, ix, iy, mz-1);
	F3(flds, HX, ix, iy, mz+1) = -F3(flds, HX, ix, iy, mz-2);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F3(flds, HY, ix, iy, mz) = -F3(flds, HY, ix, iy, mz-1);
	F3(flds, HY, ix, iy, mz+1) = -F3(flds, HY, ix, iy, mz-2);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, JYI, ix, 1,iz) -= F3(flds, JYI, ix,-2,iz);
	F3(flds, JYI, ix, 0,iz) -= F3(flds, JYI, ix,-1,iz);
	F3(flds, JYI, ix,-1,iz) = 0.;
	F3(flds, JYI, ix,-2,iz) = 0.;
	F3(flds, JXI, ix, 1,iz) += F3(flds, JXI, ix,-1,iz);
	F3(flds, JXI, ix,-1,iz) = 0.;
	F3(flds, JZI, ix, 1,iz) += F3(flds, JZI, ix,-1,iz);
	F3(flds, JZI, ix,-1,iz) = 0.;
      }
    }
  } else if (d == 2) {
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, JZI, ix, iy, 0) -= F3(flds, JZI, ix, iy,-1);
	F3(flds, JZI, ix, iy, 0) -= F3(flds, JZI, ix, iy,-1);
	F3(flds, JZI, ix, iy,-1) = 0.;
	F3(flds, JXI, ix, iy, 1) += F3(flds, JXI, ix, iy,-1);
	F3(flds, JXI, ix, iy,-1) = 0.;
	F3(flds, JYI, ix, iy, 1) += F3(flds, JYI, ix, iy,-1);
	F3(flds, JYI, ix, iy,-1) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, JYI, ix,my-2,iz) -= F3(flds, JYI, ix,my+1,iz);
	F3(flds, JYI, ix,my-1,iz) -= F3(flds, JYI, ix,my  ,iz);
	F3(flds, JYI, ix,my  ,iz) = 0.;
	F3(flds, JYI, ix,my+1,iz) = 0.;
	F3(flds, JXI, ix,my-1,iz) += F3(flds, JXI, ix,my+1,iz);
	F3(flds, JXI, ix,my+1,iz) = 0.;
	F3(flds, JZI, ix,my-1,iz) += F3(flds, JZI, ix,my+1,iz);
	F3(flds, JZI, ix,my+1,iz) = 0.;
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, JZI, ix, iy, mz-1) -= F3(flds, JZI, ix, iy,mz);
	F3(flds, JZI, ix, iy, mz) = 0.;
	F3(flds, JXI, ix, iy, mz-1) += F3(flds, JXI, ix, iy,mz+1);
	F3(flds, JXI, ix, iy, mz+1) = 0.;
	F3(flds, JYI, ix, iy, mz-1) += F3(flds, JYI, ix, iy,mz+1);
	F3(flds, JYI, ix, iy, mz+1) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

// ======================================================================
// open

// ----------------------------------------------------------------------
// open_H_lo

static void
open_H_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, -1, iz));
	fields_t_set_nan(&F3(flds, HX, ix, -2, iz));
	fields_t_set_nan(&F3(flds, HY, ix, -1, iz));
	fields_t_set_nan(&F3(flds, HY, ix, -2, iz));
	fields_t_set_nan(&F3(flds, HZ, ix, -1, iz));
	fields_t_set_nan(&F3(flds, HZ, ix, -2, iz));
      }
    }
#endif
    fields_real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HX, ix,-1,iz) = (/* + 4.f * C_s_pulse_y1(x,y,z+0.5*dz,t) */
				- 2.f * F3(flds, EZ, ix,0,iz)
				/*- dt/dx * (F3(flds, HY, ix,0,iz) - F3(flds, HY, ix-1,0,iz)) */
				- (1.f - dt/dy) * F3(flds, HX, ix,0,iz)
				+ dt * F3(flds, JZI, ix,0,iz)) / (1.f + dt/dy);
	F3(flds, HZ, ix,-1,iz) = (/* + 4.f * C_p_pulse_y1(x+.5*dx,y,z,t) */
				+ 2.f * F3(flds, EX, ix,0,iz)
				- dt/dz * (F3(flds, HY, ix,0,iz) - F3(flds, HY, ix,0,iz-1))
				- (1.f - dt/dy) * F3(flds, HZ, ix,0,iz)
				+ dt * F3(flds, JXI, ix,0,iz)) / (1.f + dt/dy);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HX, ix, iy, -2));
	fields_t_set_nan(&F3(flds, HY, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HY, ix, iy, -2));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, -1));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, -2));
      }
    }
#endif
    fields_real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HY, ix,iy,-1) = (/* + 4.f * C_s_pulse_z1(x+0.5*dx,y,z,t) */
				- 2.f * F3(flds, EX, ix,iy,0)
				- dt/dy * (F3(flds, HZ, ix,iy,0) - F3(flds, HZ, ix,iy-1,0))
				- (1.f - dt/dz) * F3(flds, HY, ix,iy,0)
				+ dt * F3(flds, JXI, ix,iy,0)) / (1.f + dt/dz);
	F3(flds, HX, ix,iy,-1) = (/* - 4.f * C_p_pulse_z1(x+0.5*dx,y,z,t) */
				+ 2.f * F3(flds, EY, ix,iy,0)
				/*- dt/dx * (F3(flds, HZ, ix,iy,0) - F3(flds, HZ, ix-1,iy,0)) FIXME not in yz 2d */
				- (1.f - dt/dz) * F3(flds, HY, ix,iy,0)
				- dt * F3(flds, JYI, ix,iy,0)) / (1.f + dt/dz);
      }
    }
  } else {
    assert(0);
  }
}

static void
open_H_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, my  , iz));
	fields_t_set_nan(&F3(flds, HX, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, HY, ix, my  , iz));
	fields_t_set_nan(&F3(flds, HY, ix, my+1, iz));
	fields_t_set_nan(&F3(flds, HZ, ix, my+1, iz));
      }
    }
#endif
    fields_real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HX, ix,my,iz) = (/* + 4.f * C_s_pulse_y2(x,y,z+0.5*dz,t) */
				+ 2.f * F3(flds, EZ, ix,my,iz)
				/*+ dt/dx * (F3(flds, HY, ix,my,iz) - F3(flds, HY, ix-1,my,iz)) */
				- (1.f - dt/dy) * F3(flds, HX, ix,my-1,iz)
				- dt * F3(flds, JZI, ix,my,iz)) / (1.f + dt/dy);
	F3(flds, HZ, ix,my,iz) = (/* + 4.f * C_p_pulse_y2(x+.5*dx,y,z,t) */
				- 2.f * F3(flds, EX, ix,my,iz)
				+ dt/dz * (F3(flds, HY, ix,my,iz) - F3(flds, HY, ix,my,iz-1))
				- (1.f - dt/dy) * F3(flds, HZ, ix,my-1,iz)
				- dt * F3(flds, JXI, ix,my,iz)) / (1.f + dt/dy);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F3(flds, HX, ix, iy, mz  ));
	fields_t_set_nan(&F3(flds, HX, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, HY, ix, iy, mz  ));
	fields_t_set_nan(&F3(flds, HY, ix, iy, mz+1));
	fields_t_set_nan(&F3(flds, HZ, ix, iy, mz+1));
      }
    }
#endif
    fields_real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F3(flds, HY, ix,iy,mz) = (/* - 4.f * C_s_pulse_z2(x+0.5*dx,y,z,t) */
				+ 2.f * F3(flds, EX, ix,iy,mz)
				+ dt/dy * (F3(flds, HZ, ix,iy,mz) - F3(flds, HZ, ix,iy-1,mz))
				- (1.f - dt/dz) * F3(flds, HY, ix,iy,mz-1)
				- dt * F3(flds, JXI, ix,iy,mz)) / (1.f + dt/dz);
	F3(flds, HX, ix,iy,mz) = (/* + 4.f * C_p_pulse_z2(x+0.5*dx,y,z,t) */
				- 2.f * F3(flds, EY, ix,iy,mz)
				/*+ dt/dx * (F3(flds, HZ, ix,iy,mz) - F3(flds, HZ, ix-1,iy,mz)) FIXME not in yz 2d*/
				- (1.f - dt/dz) * F3(flds, HX, ix,iy,mz-1)
				+ dt * F3(flds, JYI, ix,iy,mz)) / (1.f + dt/dz);
      }
    }
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_bnd_fields_sub_fill_ghosts_E

static void
psc_bnd_fields_sub_fill_ghosts_E(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, FIELDS_TYPE, EX, EX + 3);

  for (int p = 0; p < mflds->nr_patches; p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_E_lo(bnd, fields_t_mflds(mflds, p), p, d);
	break;
	case BND_FLD_OPEN:
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
	  conducting_wall_E_hi(bnd, fields_t_mflds(mflds, p), p, d);
	  break;
	case BND_FLD_OPEN:
	  break;
	default:
	  assert(0);
	}
      }
    }
  }
  psc_mfields_put_as(mflds, mflds_base, EX, EX + 3);
}

// ----------------------------------------------------------------------
// psc_bnd_fields_sub_fill_ghosts_H

static void
psc_bnd_fields_sub_fill_ghosts_H(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, FIELDS_TYPE, HX, HX + 3);

  for (int p = 0; p < mflds->nr_patches; p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_H_lo(bnd, fields_t_mflds(mflds, p), p, d);
	  break;
	case BND_FLD_OPEN:
	  open_H_lo(bnd, fields_t_mflds(mflds, p), p, d);
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
	  conducting_wall_H_hi(bnd, fields_t_mflds(mflds, p), p, d);
	break;
	case BND_FLD_OPEN:
	  open_H_hi(bnd, fields_t_mflds(mflds, p), p, d);
	  break;
	default:
	assert(0);
	}
      }
    }
  }
  psc_mfields_put_as(mflds, mflds_base, HX, HX + 3);
}

static void
psc_bnd_fields_sub_add_ghosts_J(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, FIELDS_TYPE, JXI, JXI + 3);

  for (int p = 0; p < mflds->nr_patches; p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	case BND_FLD_OPEN:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_J_lo(bnd, fields_t_mflds(mflds, p), p, d);
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
	case BND_FLD_OPEN:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_J_hi(bnd, fields_t_mflds(mflds, p), p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
  }

  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);
}

