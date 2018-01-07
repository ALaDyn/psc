
#include "psc.h"
#include "psc_debug.h"
#include "fields.hxx"

#include <mrc_bits.h>

#include <limits>

// FIXME, needs public access to Fields::ib, im
//#define DEBUG

using Fields = Fields3d<fields_t, dim_xz>; // FIXME dim_xz

static inline void fields_t_set_nan(fields_t::real_t *f)
{
  *f = std::numeric_limits<fields_t::real_t>::quiet_NaN();
}

static void
conducting_wall_E_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, pf->ib[0]); ix < MIN(patch->ldims[0] + 2, pf->ib[0] + pf->im[0]) ; ix++) {
	fields_t_set_nan(&F(EX, ix, -1,iz));
	fields_t_set_nan(&F(EX, ix, -2,iz));
	fields_t_set_nan(&F(EY, ix, -1,iz));
	fields_t_set_nan(&F(EY, ix, -2,iz));
	fields_t_set_nan(&F(EZ, ix, -1,iz));
	fields_t_set_nan(&F(EZ, ix, -2,iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      // FIXME, needs to be for other dir, too, and it's ugly
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(EX, ix, 0,iz) =  0.;
	F(EX, ix,-1,iz) =  F(EX, ix, 1,iz);
	F(EX, ix,-2,iz) =  F(EX, ix, 2,iz);
	F(EY, ix,-1,iz) = -F(EY, ix, 0,iz);
	F(EY, ix,-2,iz) = -F(EY, ix, 1,iz);
      }
    }

    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(EZ, ix, 0,iz) =  0.;
	F(EZ, ix,-1,iz) =  F(EZ, ix, 1,iz);
	F(EZ, ix,-2,iz) =  F(EZ, ix, 2,iz);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F(EX, ix, iy, -1));
	fields_t_set_nan(&F(EX, ix, iy, -2));
	fields_t_set_nan(&F(EY, ix, iy, -1));
	fields_t_set_nan(&F(EY, ix, iy, -2));
	fields_t_set_nan(&F(EZ, ix, iy, -1));
	fields_t_set_nan(&F(EZ, ix, iy, -2));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(EX, ix, iy, 0) =  0.;
	F(EX, ix, iy,-1) =  F(EX, ix, iy, 1);
	F(EZ, ix, iy,-1) = -F(EZ, ix, iy, 0);
	F(EZ, ix, iy,-2) = -F(EZ, ix, iy, 1);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(EY, ix, iy, 0) =  0.;
	F(EY, ix, iy,-1) =  F(EY, ix, iy, 1);
      }
    }
  } else  {
    assert(0);
  }
}

static void
conducting_wall_E_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

   if (d == 1) {
     int my  _mrc_unused = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
 	fields_t_set_nan(&F(EX, ix, my  , iz));
	fields_t_set_nan(&F(EX, ix, my+1, iz));
	fields_t_set_nan(&F(EY, ix, my  , iz));
	fields_t_set_nan(&F(EY, ix, my+1, iz));
	fields_t_set_nan(&F(EZ, ix, my  , iz));
	fields_t_set_nan(&F(EZ, ix, my+1, iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(EX, ix,my  ,iz) = 0.;
	F(EX, ix,my+1,iz) =  F(EX, ix, my-1,iz);
	F(EY, ix,my  ,iz) = -F(EY, ix, my-1,iz);
      }
    }

    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(EZ, ix,my  ,iz) = 0.;
	F(EZ, ix,my+1,iz) =  F(EZ, ix, my-1,iz);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F(EX, ix, iy, mz));
	fields_t_set_nan(&F(EX, ix, iy, mz+1));
	fields_t_set_nan(&F(EY, ix, iy, mz));
	fields_t_set_nan(&F(EY, ix, iy, mz+1));
	fields_t_set_nan(&F(EZ, ix, iy, mz));
	fields_t_set_nan(&F(EZ, ix, iy, mz+1));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(EX, ix, iy, mz) = 0.;
	F(EX, ix, iy, mz+1) =  F(EX, ix, iy, mz-1);
	F(EZ, ix, iy, mz)   = -F(EZ, ix, iy, mz-1);
	F(EZ, ix, iy, mz+1)   = -F(EZ, ix, iy, mz-2);
      }
    }

    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(EY, ix, iy, mz) = 0.;
	F(EY, ix, iy, mz+1) =  F(EY, ix, iy, mz-1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F(HX, ix, -1,iz));
	fields_t_set_nan(&F(HX, ix, -2,iz));
	fields_t_set_nan(&F(HY, ix, -1,iz));
	fields_t_set_nan(&F(HY, ix, -2,iz));
	fields_t_set_nan(&F(HZ, ix, -1,iz));
	fields_t_set_nan(&F(HZ, ix, -2,iz));
      }
    }
#endif
    for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HY, ix,-1,iz) =  F(HY, ix, 1,iz);
	F(HX, ix,-1,iz) = -F(HX, ix, 0,iz);
      }
    }
    for (int iz = -1; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HZ, ix,-1,iz) = -F(HZ, ix, 0,iz);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F(HX, ix, iy, -1));
	fields_t_set_nan(&F(HX, ix, iy, -2));
	fields_t_set_nan(&F(HY, ix, iy, -1));
	fields_t_set_nan(&F(HY, ix, iy, -2));
	fields_t_set_nan(&F(HZ, ix, iy, -1));
	fields_t_set_nan(&F(HZ, ix, iy, -2));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(HZ, ix, iy,-1) =  F(HZ, ix, iy, 1);
	F(HX, ix, iy,-1) = -F(HX, ix, iy, 0);
	F(HX, ix, iy,-2) = -F(HX, ix, iy, 1);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(HY, ix, iy,-1) = -F(HY, ix, iy, 0);
	F(HY, ix, iy,-2) = -F(HY, ix, iy, 1);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_H_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my _mrc_unused = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F(HX, ix, my  , iz));
	fields_t_set_nan(&F(HX, ix, my+1, iz));
	fields_t_set_nan(&F(HY, ix, my+1, iz));
	fields_t_set_nan(&F(HZ, ix, my  , iz));
	fields_t_set_nan(&F(HZ, ix, my+1, iz));
      }
    }
#endif
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HY, ix,my+1,iz) =  F(HY, ix, my-1,iz);
	F(HX, ix,my  ,iz) = -F(HX, ix, my-1,iz);
	F(HX, ix,my+1,iz) = -F(HX, ix, my-2,iz);
      }
    }
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HZ, ix,my  ,iz) = -F(HZ, ix, my-1,iz);
	F(HZ, ix,my+1,iz) = -F(HZ, ix, my-2,iz);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F(HX, ix, iy, mz  ));
	fields_t_set_nan(&F(HX, ix, iy, mz+1));
	fields_t_set_nan(&F(HY, ix, iy, mz  ));
	fields_t_set_nan(&F(HY, ix, iy, mz+1));
	fields_t_set_nan(&F(HZ, ix, iy, mz+1));
      }
    }
#endif
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(HZ, ix, iy, mz+1) =  F(HZ, ix, iy, mz-1);
	F(HX, ix, iy, mz) = -F(HX, ix, iy, mz-1);
	F(HX, ix, iy, mz+1) = -F(HX, ix, iy, mz-2);
      }
    }
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	F(HY, ix, iy, mz) = -F(HY, ix, iy, mz-1);
	F(HY, ix, iy, mz+1) = -F(HY, ix, iy, mz-2);
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_lo(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(JYI, ix, 1,iz) -= F(JYI, ix,-2,iz);
	F(JYI, ix, 0,iz) -= F(JYI, ix,-1,iz);
	F(JYI, ix,-1,iz) = 0.;
	F(JYI, ix,-2,iz) = 0.;
	F(JXI, ix, 1,iz) += F(JXI, ix,-1,iz);
	F(JXI, ix,-1,iz) = 0.;
	F(JZI, ix, 1,iz) += F(JZI, ix,-1,iz);
	F(JZI, ix,-1,iz) = 0.;
      }
    }
  } else if (d == 2) {
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(JZI, ix, iy, 0) -= F(JZI, ix, iy,-1);
	F(JZI, ix, iy, 0) -= F(JZI, ix, iy,-1);
	F(JZI, ix, iy,-1) = 0.;
	F(JXI, ix, iy, 1) += F(JXI, ix, iy,-1);
	F(JXI, ix, iy,-1) = 0.;
	F(JYI, ix, iy, 1) += F(JYI, ix, iy,-1);
	F(JYI, ix, iy,-1) = 0.;
      }
    }
  } else {
    assert(0);
  }
}

static void
conducting_wall_J_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my _mrc_unused = patch->ldims[1];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(JYI, ix,my-2,iz) -= F(JYI, ix,my+1,iz);
	F(JYI, ix,my-1,iz) -= F(JYI, ix,my  ,iz);
	F(JYI, ix,my  ,iz) = 0.;
	F(JYI, ix,my+1,iz) = 0.;
	F(JXI, ix,my-1,iz) += F(JXI, ix,my+1,iz);
	F(JXI, ix,my+1,iz) = 0.;
	F(JZI, ix,my-1,iz) += F(JZI, ix,my+1,iz);
	F(JZI, ix,my+1,iz) = 0.;
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(JZI, ix, iy, mz-1) -= F(JZI, ix, iy,mz);
	F(JZI, ix, iy, mz) = 0.;
	F(JXI, ix, iy, mz-1) += F(JXI, ix, iy,mz+1);
	F(JXI, ix, iy, mz+1) = 0.;
	F(JYI, ix, iy, mz-1) += F(JYI, ix, iy,mz+1);
	F(JYI, ix, iy, mz+1) = 0.;
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
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F(HX, ix, -1, iz));
	fields_t_set_nan(&F(HX, ix, -2, iz));
	fields_t_set_nan(&F(HY, ix, -1, iz));
	fields_t_set_nan(&F(HY, ix, -2, iz));
	fields_t_set_nan(&F(HZ, ix, -1, iz));
	fields_t_set_nan(&F(HZ, ix, -2, iz));
      }
    }
#endif
    fields_t::real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HX, ix,-1,iz) = (/* + 4.f * C_s_pulse_y1(x,y,z+0.5*dz,t) */
				- 2.f * F(EZ, ix,0,iz)
				/*- dt/dx * (F(HY, ix,0,iz) - F(HY, ix-1,0,iz)) */
				- (1.f - dt/dy) * F(HX, ix,0,iz)
				+ dt * F(JZI, ix,0,iz)) / (1.f + dt/dy);
	F(HZ, ix,-1,iz) = (/* + 4.f * C_p_pulse_y1(x+.5*dx,y,z,t) */
				+ 2.f * F(EX, ix,0,iz)
				- dt/dz * (F(HY, ix,0,iz) - F(HY, ix,0,iz-1))
				- (1.f - dt/dy) * F(HZ, ix,0,iz)
				+ dt * F(JXI, ix,0,iz)) / (1.f + dt/dy);
      }
    }
  } else if (d == 2) {
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F(HX, ix, iy, -1));
	fields_t_set_nan(&F(HX, ix, iy, -2));
	fields_t_set_nan(&F(HY, ix, iy, -1));
	fields_t_set_nan(&F(HY, ix, iy, -2));
	fields_t_set_nan(&F(HZ, ix, iy, -1));
	fields_t_set_nan(&F(HZ, ix, iy, -2));
      }
    }
#endif
    fields_t::real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HY, ix,iy,-1) = (/* + 4.f * C_s_pulse_z1(x+0.5*dx,y,z,t) */
				- 2.f * F(EX, ix,iy,0)
				- dt/dy * (F(HZ, ix,iy,0) - F(HZ, ix,iy-1,0))
				- (1.f - dt/dz) * F(HY, ix,iy,0)
				+ dt * F(JXI, ix,iy,0)) / (1.f + dt/dz);
	F(HX, ix,iy,-1) = (/* - 4.f * C_p_pulse_z1(x+0.5*dx,y,z,t) */
				+ 2.f * F(EY, ix,iy,0)
				/*- dt/dx * (F(HZ, ix,iy,0) - F(HZ, ix-1,iy,0)) FIXME not in yz 2d */
				- (1.f - dt/dz) * F(HY, ix,iy,0)
				- dt * F(JYI, ix,iy,0)) / (1.f + dt/dz);
      }
    }
  } else {
    assert(0);
  }
}

static void
open_H_hi(struct psc_bnd_fields *bnd, fields_t flds, int p, int d)
{
  Fields F(flds);
  struct psc_patch *patch = ppsc->patch + p;

  if (d == 1) {
    int my _mrc_unused = patch->ldims[1];
#ifdef DEBUG
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	fields_t_set_nan(&F(HX, ix, my  , iz));
	fields_t_set_nan(&F(HX, ix, my+1, iz));
	fields_t_set_nan(&F(HY, ix, my  , iz));
	fields_t_set_nan(&F(HY, ix, my+1, iz));
	fields_t_set_nan(&F(HZ, ix, my+1, iz));
      }
    }
#endif
    fields_t::real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iz = -2; iz < patch->ldims[2] + 2; iz++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HX, ix,my,iz) = (/* + 4.f * C_s_pulse_y2(x,y,z+0.5*dz,t) */
				+ 2.f * F(EZ, ix,my,iz)
				/*+ dt/dx * (F(HY, ix,my,iz) - F(HY, ix-1,my,iz)) */
				- (1.f - dt/dy) * F(HX, ix,my-1,iz)
				- dt * F(JZI, ix,my,iz)) / (1.f + dt/dy);
	F(HZ, ix,my,iz) = (/* + 4.f * C_p_pulse_y2(x+.5*dx,y,z,t) */
				- 2.f * F(EX, ix,my,iz)
				+ dt/dz * (F(HY, ix,my,iz) - F(HY, ix,my,iz-1))
				- (1.f - dt/dy) * F(HZ, ix,my-1,iz)
				- dt * F(JXI, ix,my,iz)) / (1.f + dt/dy);
      }
    }
  } else if (d == 2) {
    int mz = patch->ldims[2];
#ifdef DEBUG
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = -2; ix < patch->ldims[0] + 2; ix++) {
	fields_t_set_nan(&F(HX, ix, iy, mz  ));
	fields_t_set_nan(&F(HX, ix, iy, mz+1));
	fields_t_set_nan(&F(HY, ix, iy, mz  ));
	fields_t_set_nan(&F(HY, ix, iy, mz+1));
	fields_t_set_nan(&F(HZ, ix, iy, mz+1));
      }
    }
#endif
    fields_t::real_t dt = ppsc->dt, dy = ppsc->patch[p].dx[1], dz = ppsc->patch[p].dx[2];
    for (int iy = -2; iy < patch->ldims[1] + 2; iy++) {
      for (int ix = MAX(-2, flds.ib[0]); ix < MIN(patch->ldims[0] + 2, flds.ib[0] + flds.im[0]) ; ix++) {
	F(HY, ix,iy,mz) = (/* - 4.f * C_s_pulse_z2(x+0.5*dx,y,z,t) */
				+ 2.f * F(EX, ix,iy,mz)
				+ dt/dy * (F(HZ, ix,iy,mz) - F(HZ, ix,iy-1,mz))
				- (1.f - dt/dz) * F(HY, ix,iy,mz-1)
				- dt * F(JXI, ix,iy,mz)) / (1.f + dt/dz);
	F(HX, ix,iy,mz) = (/* + 4.f * C_p_pulse_z2(x+0.5*dx,y,z,t) */
				- 2.f * F(EY, ix,iy,mz)
				/*+ dt/dx * (F(HZ, ix,iy,mz) - F(HZ, ix-1,iy,mz)) FIXME not in yz 2d*/
				- (1.f - dt/dz) * F(HX, ix,iy,mz-1)
				+ dt * F(JYI, ix,iy,mz)) / (1.f + dt/dz);
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
  mfields_t mf(mflds);
  
  for (int p = 0; p < mflds->nr_patches; p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_E_lo(bnd, mf[p], p, d);
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
	  conducting_wall_E_hi(bnd, mf[p], p, d);
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
  mfields_t mf = mflds_base->get_as<mfields_t>(FIELDS_TYPE, HX, HX + 3);
  
  for (int p = 0; p < mf.nr_patches(); p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_H_lo(bnd, mf[p], p, d);
	  break;
	case BND_FLD_OPEN:
	  open_H_lo(bnd, mf[p], p, d);
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
	  conducting_wall_H_hi(bnd, mf[p], p, d);
	break;
	case BND_FLD_OPEN:
	  open_H_hi(bnd, mf[p], p, d);
	  break;
	default:
	assert(0);
	}
      }
    }
  }
  mf.put_as(mflds_base, HX, HX + 3);
}

static void
psc_bnd_fields_sub_add_ghosts_J(struct psc_bnd_fields *bnd, struct psc_mfields *mflds_base)
{
  // FIXME/OPT, if we don't need to do anything, we don't need to get
  struct psc_mfields *mflds = psc_mfields_get_as(mflds_base, FIELDS_TYPE, JXI, JXI + 3);
  mfields_t mf(mflds);

  for (int p = 0; p < mflds->nr_patches; p++) {
    // lo
    for (int d = 0; d < 3; d++) {
      if (ppsc->patch[p].off[d] == 0) {
	switch (ppsc->domain.bnd_fld_lo[d]) {
	case BND_FLD_PERIODIC:
	case BND_FLD_OPEN:
	  break;
	case BND_FLD_CONDUCTING_WALL:
	  conducting_wall_J_lo(bnd, mf[p], p, d);
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
	  conducting_wall_J_hi(bnd, mf[p], p, d);
	  break;
	default:
	  assert(0);
	}
      }
    }
  }

  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);
}

