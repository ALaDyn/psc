#include spu_particles.h

static void
spu_push_part_2d(unsigned long ea){

  unsigned long long cp_ea = ea;
  unsigned long long np_ea; 

  struct particle_cbe_t _bufferA[2], _bufferB[2], _bufferC[2];

  buff.plb1 = &(_bufferA[0]);
  buff.plb2 = &(_bufferA[1]);
  buff.lb1 = &(_bufferB[0]); 
  buff.lb2 = &(_bufferB[1]);
  buff.sb1 = &(_bufferC[0]);
  buff.sb2 = &(_bufferC[1]);

  // Get the first two particles
  mfc_get(buff.lb1, cp_ea, 2*sizeof(particle_cbe_t), 
	  tagp_get, 0, 0);

  np_ea = cp_ea + 2*sizeof(particle_cbe_t);

  // we have some stuff to do while we wait for
  // it to come in.

  // insert assignment, promotions, and constant 
  // calculations here.

  mfc_write_tag_mask(1 << tagp_get);
  unsigned int mask = mfc_read_tag_status_any();


  unsigned long long end = psc_block.part_end; 

  int run = 1;

  do {

    // issue dma request for particle we will need 
    // next time through the loop.
    if(__builtin_expect((np_ea < end),1)) {
      mfc_write_tag_mask(1 << tagp_get);
      mask = mfc_read_tag_status_any();
      mfc_get(buff.plb1, np_ea, 2 * sizeof(particle_cbe_t), 
	      tagp_get, 0, 0);
    }    
    // we may need to insert some padding here, so we have to stop and check.
    // The last particle is going to be very slow (probably).
    else if(__builtin_expect(((end - cp_ea) != sizeof(particle_cbe_t)),0)){ 
      buff->lp2 = &null_part;
    }

    v_real xi, yi, zi, pxi, pyi, pzi, qni, mni, wni;
  
    LOAD_PARTICLES_SPU;
    
    v_real vxi,vyi,vzi,root,tmpx,tmpy,tmpz; 
    
    tmpx = spu_mul(pxi,pxi);
    tmpy = spu_mul(pyi,pyi);
    tmpz = spu_mul(pzi, pzi);
    
    tmpx = spu_add(tmpx, tmpy);
    tmpz = spu_add(one, tmpz);
    tmpx = spu_add(tmpx, tmpz);
    root = spu_sqrt(tmpx);
    
    vxi = spu_div(pxi, root);
    vyi = spu_div(pyi, root);
    vzi = spu_div(pzi, root);
    
    tmpy = spu_mul(vyi, yl);
    tmpz = spu_mul(vzi, zl);
    
    yi = spu_add(yi, tmpy);
    zi = spu_add(zi, tmpz);
    
    STORE_PARTICLES_SPU;

    np_ea = cp_ea +  2 * sizeof(particle_cbe_t);
    // At this point, np_ea is one ahead of the
    // current particle.

    
    // rotate the buffers 
    unsigned long btmp1, btmp2;
    btmp1 = buff->sb1;
    btmp2 = buff->sb2;
    buff->sb1 = buff->lb1;
    buff->sb2 = buff->lb2;
    buff->lb1 = buff->plb1;
    buff->lb2 = buff->plb2;
    buff->plb1 = btmp1;
    buff->plb2 = btmp2;
    
    if(__builtin_expect((np_ea >= end),0)) { // if we've run out of particles
      mfc_write_tag_mask(1 << tagp_put);
      mask = mfc_read_tag_status_any();
      mfc_put(buff->sb1, cp_ea, (unsigned size_t) (end - cp_ea),
	      tagp_put, 0, 0);
      run = 0; 
    }
    else {
      mfc_write_tag_mask(1 << tagp_put);
      mask = mfc_read_tag_status_any();
      mfc_put(buff->sb1, cp_ea, 2 * sizeof(particle_cbe_t),
	      tagp_put, 0, 0);

      cp_ea = np_ea;
      np_ea += 2*sizeof(particle_cbe_t);
      // cp_ea now points to the particle which will be used
      // next time in the loop. 
      // np_ea points to the one which needs to be pre-loaded.
    }

  } while(__builtin_expect((run),1));
}
