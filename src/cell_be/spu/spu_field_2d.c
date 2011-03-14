#include "psc_spu.h"
#include "psc_spu_2d.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <simdmath.h>

#ifdef __SPU__
#include <spu_mfcio.h>
#else
#include "spu_mfcio_c.h"
#endif

// Local store field indexing. 
enum{
  ls_EX, ls_EY, ls_EZ,
  ls_HX, ls_HY, ls_HZ,
  ls_JXI, ls_JYI, ls_JZI,
  NR_LSFLDS,
};

#define NDIM 32

// For dma purposes, we need C fields access macro here on the spu, but I don't
// want to contaminate the spu code, so it's just copied here. 
#define F3_OFF_C(pf, fldnr, jx,jy,jz)					\
  ((((((fldnr)								\
       * (pf)->im[2] + ((jz)-(pf)->ib[2]))				\
      * (pf)->im[1] + ((jy)-(pf)->ib[1]))				\
     * (pf)->im[0] + ((jx)-(pf)->ib[0]))))



// FIXME Assumes xy plane!!
// We need a macro to access the offsets in the local store. 
// The fields in the local store will only be 2d, and right now I'm going to 
// force them to be 2d in the xy plane, life is too messy otherwise.
#define F2_SPU_OFF(fldnr, jx, jy)					\
  (((((fldnr)								\
      * NDIM + (jy))							\
     * NDIM + (jx))))


// The actual compute kernel function.
int
spu_push_field_a_nopml(void)
{
#if PRINT_DEBUG

  printf("[[%#llx] start ea: %#llx end ea: %#llx \n", spu_ctx.spe_id, psc_block.part_start, psc_block.part_end);

#endif

  // Local store for the fields. At this point, contains E,H, and J.
  fields_c_real_t ls_fld[NR_LSFLDS*32*32] __attribute__((aligned(128)));

  // If we try to dma in all the field data at once, we run into the 
  // mfc 16KB per request limit. For now, we'll split it into 6 different
  // requests. Depending on whether the limit is 16KB = 1000 * 16B 
  // or 1024*16B, we could group these two by two.
  
  // There might be a faster way to do this using dma lists. One would
  // think we're stalling pretty bad here, but until I get a good idea 
  // of the bandwidth and latency of the mfc I can't really say for sure.

  // This could be pretty easily generalized for precision switching, but 
  // right now it won't work with single precision. 

  // FIXME: This assumes the index in the symmetry direction is at 0.

  spu_dma_get(ls_fld, 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EX,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_get(&ls_fld[ls_EY*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EY,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_get(&ls_fld[ls_EZ*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EZ,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_get(&ls_fld[ls_HX*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HX,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_get(&ls_fld[ls_HY*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HY,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_get(&ls_fld[ls_HZ*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HZ,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));


  // Do EX push. Differences along the y (slow running) direction.
  // So, run along that direction on the interior so we can minimize 
  // loads. 

  const vector unsigned char fast_pat = 
    {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, // second word from first vec
     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}; // first word from second vec


  v_real cnx = spu_splats(0.5 * spu_ctx.dt / spu_ctx.dx[0]);
  v_real cny = spu_splats(0.5 * spu_ctx.dt / spu_ctx.dx[1]);
  v_real cnz = spu_splats(0.5 * spu_ctx.dt / spu_ctx.dx[2]);
  v_real half_dt = spu_splats(0.5 * spu_ctx.dt);

  v_real buff_pre, buff_curr, buff_minus, J,store;


  for(int jx = 2; jx < NDIM; jx += 2){
    buff_curr = *((v_real *)(ls_fld + F2_SPU_OFF(ls_HZ, jx, 1)));
    buff_pre =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_HZ, jx, 2)));
    for(int jy = 2; jy < NDIM - 2; jy++){
      J =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_JXI, jx, jy)));
      store =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_EX, jx, jy)));
      buff_minus = buff_curr;
      buff_curr = buff_pre;
      buff_pre =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_HZ, jx, jy+1)));
      store += cny * (buff_curr - buff_minus) - half_dt * J;
      *((v_real *)(ls_fld + F2_SPU_OFF(ls_EX, jx, jy))) = store;
    }
    // Do the last row by hand so we don't have to preload
    J =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_JXI, jx, NDIM-2)));
    store =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_EX, jx, NDIM-2)));
    buff_minus = buff_curr;
    buff_curr = buff_pre;
    store += cny * (buff_curr - buff_minus) - half_dt * J;
    *((v_real *)(ls_fld + F2_SPU_OFF(ls_EX, jx, NDIM-2))) = store;
  }

  // Now for the fast running index which is, ironically, more difficult
  
  for(int jy = 2; jy < NDIM; jy++){
    buff_curr = *((v_real *)(ls_fld + F2_SPU_OFF(ls_HZ, 0, jy)));
    buff_pre = *((v_real *)(ls_fld + F2_SPU_OFF(ls_HZ, 2, jy)));
    for(int jx = 2; jx < NDIM - 2; jx +=2) {
      buff_minus = spu_shuffle(buff_pre,buff_curr,fast_pat);
      buff_curr = buff_pre;
      buff_pre = *((v_real *)(ls_fld + F2_SPU_OFF(ls_HY, jx+2, jy)));
      J =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_JYI, jx, jy)));
      store =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_EY, jx, jy)));
      store -= cnx * (buff_curr - buff_minus) + half_dt * J;
      *((v_real *)(ls_fld + F2_SPU_OFF(ls_EY, jx, jy))) = store;
    }
    buff_minus = spu_shuffle(buff_pre,buff_curr,fast_pat);
    buff_curr = buff_pre;
    J =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_JYI, NDIM - 2, jy)));
    store =  *((v_real *)(ls_fld + F2_SPU_OFF(ls_EY, NDIM - 2, jy)));
    store -= cnx * (buff_curr - buff_minus) + half_dt * J;
    *((v_real *)(ls_fld + F2_SPU_OFF(ls_EY, NDIM - 2, jy))) = store;
  }
  // Write out the fields
  
  // FIXME: This assumes the index of the plane in 0 in the symmetry direction.
  spu_dma_put(&ls_fld[ls_EX*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EX,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_put(&ls_fld[ls_EY*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EY,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_put(&ls_fld[ls_EZ*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,EZ,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));
  

  spu_dma_put(&ls_fld[ls_HX*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HX,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_put(&ls_fld[ls_HY*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HY,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));

  spu_dma_put(&ls_fld[ls_HZ*32*32], 
	      (psc_block.wb_flds + 
	       sizeof(fields_c_real_t) * F3_OFF_C(&psc_block,HZ,
						      psc_block.ib[0],
						      psc_block.ib[1],
						      0)),
	      32*32*sizeof(fields_c_real_t));
  


  // Then we just wait to make sure the particles have 
  // finished storing. 
  end_wait_particles_stored();
#if PRINT_DEBUG
  fprintf(stderr, "[[%#llx] ran %d particles\n", spu_ctx.spe_id, n);
#endif
  return 0;

}

#undef F2_SPU_OFF
