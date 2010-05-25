
#ifndef PSC_SSE2_H
#define PSC_SSE2_H

#define FORT_FIELD(T,l,j,k) (float)psc.f_fields[(T)][((l)-psc.ilo[0]+psc.ibn[0]) + ((j)-psc.ilo[1]+psc.ibn[1])*(psc.img[0]) + ((k) - psc.ilo[2]+psc.ibn[2])*(psc.img[0]+psc.img[1])]

#include <assert.h>
#include <xmmintrin.h>
// Not including any SSE2 emulation at this time (finding an sse proc won't be hard, anything >= a P4 or AMD post 2005 will support these)

#include "psc.h"
// For now, we'll stick with the Array of Structs memory layout, and do our packing at the loop level.
// There is some question as to whether packing when the particles are brought in will have any impact on performance...

struct sse2_particle {
  real xi, yi, zi;
  real pxi, pyi, pzi;
  real qni;
  real mni;
  real wni;
};

// This is the 'C' way to do things, and may be better in the future, but not used now
/* struct sse2_fields { */
/*   real *ne, *ni, *nn; */
/*   real *jxi, *jyi, *jzi; */
/*   real *ex, *ey, *ez; */
/*   real *bx, *by, *bz; */
/* } */

struct psc_sse2 {
  struct sse2_particle *part;
  real *fields;
};

// to access the elements safely
union packed_vector {
  __m128 r;
  float v[4] ; //FIXME : Might break for any non gcc
} __attribute__ ((aligned (128)));

void sse2_push_part_yz_a();
void sse2_push_part_yz_b();

#endif
