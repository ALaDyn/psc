
#include <cmath>

#include "inc_params.c"
#include "pushp.hxx"

template<typename real_t>
class PI
{
public:
  // ----------------------------------------------------------------------
  // find_idx_off_1st_rel

  void find_idx_off_1st_rel(real_t xi[3], int lg[3], real_t og[3], real_t shift)
  {
    for (int d = 0; d < 3; d++) {
      real_t pos = xi[d] * c_prm.dxi[d] + shift;
      lg[d] = fint(pos);
      og[d] = pos - lg[d];
    }
  }

  // ----------------------------------------------------------------------
  // find_idx_off_pos_1st_rel

  void find_idx_off_pos_1st_rel(real_t xi[3], int lg[3], real_t og[3], real_t pos[3], real_t shift)
  {
    for (int d = 0; d < 3; d++) {
      pos[d] = xi[d] * c_prm.dxi[d] + shift;
      lg[d] = fint(pos[d]);
      og[d] = pos[d] - lg[d];
    }
  }
};

