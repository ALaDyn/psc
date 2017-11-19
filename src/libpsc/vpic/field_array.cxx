
#include "field_array.h"

#include <mrc_common.h>

struct Field3D {
  Field3D(FieldArray& fa)
    : sx_(fa.g->nx + 2), sy_(fa.g->ny + 2),
      f_(fa.data())
  {
  }

  int voxel(int i, int j, int k) const
  {
    return i + sx_ * (j + sy_ * (k));
  }

  field_t& operator()(FieldArray &fa, int i, int j, int k)
  {
    return fa.f[voxel(i,j,k)];
  }
  
  field_t operator()(FieldArray &fa, int i, int j, int k) const
  {
    return fa.f[voxel(i,j,k)];
  }
  
  float& operator()(int m, int i, int j, int k)
  {
    return f_[m + FieldArray::N_COMP * voxel(i,j,k)];
  }
  
  float operator()(int m, int i, int j, int k) const
  {
    return f_[m + FieldArray::N_COMP * voxel(i,j,k)];
  }

private:
  int sx_, sy_;
  float * RESTRICT f_;
};

#define DECLARE_STENCIL()						\
  Field3D F(*this);							\
  const int   nx   = g->nx;						\
  const int   ny   = g->ny;						\
  const int   nz   = g->nz;						\
  									\
  const float px   = (nx>1) ? frac*g->cvac*g->dt*g->rdx : 0;		\
  const float py   = (ny>1) ? frac*g->cvac*g->dt*g->rdy : 0;		\
  const float pz   = (nz>1) ? frac*g->cvac*g->dt*g->rdz : 0

#define UPDATE_CBX() F(CBX, i,j,k) -= (py*(F(EZ, i,j+1,k) - F(EZ ,i,j,k)) - pz*(F(EY, i,j,k+1) - F(EY, i,j,k)))
#define UPDATE_CBY() F(CBY, i,j,k) -= (pz*(F(EX, i,j,k+1) - F(EX ,i,j,k)) - px*(F(EZ, i+1,j,k) - F(EZ, i,j,k)))
#define UPDATE_CBZ() F(CBZ, i,j,k) -= (px*(F(EY, i+1,j,k) - F(EY, i,j,k)) - py*(F(EX, i,j+1,k) - F(EX, i,j,k)))

void FieldArray::advanceB_interior(float frac)
{
  DECLARE_STENCIL();

  for (int k = 1; k <= nz; k++) {
    for (int j = 1; j <= ny; j++) {
      for (int i = 1; i <= nx; i++) {
	UPDATE_CBX(); UPDATE_CBY(); UPDATE_CBZ();
      }
    }
  }
}

void FieldArray::advanceB(float frac)
{
  advanceB_interior(frac);

  DECLARE_STENCIL();
  
  // leftover bx
  { int i = nx + 1;
    for (int k = 1; k <= nz; k++) {
      for (int j = 1; j <= ny; j++) {
	UPDATE_CBX();
      }
    }
  }

  // leftover by
  { int j = ny + 1;
    for (int k = 1; k <= nz; k++) {
      for (int i = 1; i <= nx; i++) {
	UPDATE_CBY();
      }
    }
  }

  // leftover bz
  { int k = nz + 1;
    for (int j = 1; j <= ny; j++) {
      for (int i = 1; i <= nx; i++) {
	UPDATE_CBZ();
      }
    }
  }
  
  local_adjust_norm_b( f, g );
}

