#include "psc.h"
#include "psc_ppu.h"


void cbe_setup_layout(void)
{
  assert(!spu_ctl.layout);
  spu_ctl.layout = calloc(1, sizeof(struct cbe_block_layout));
  
  assert(psc.img[0] == 7);
  assert(psc.img[1] == 110);
  assert(psc.img[2] == 110);

  struct cbe_block_layout *layout = spu_ctl.layout;  

  layout->nblocks = 16;

  layout->block_size[0] = 1;
  layout->block_size[1] = 26;
  layout->block_size[2] = 26;

  layout->block_grid[0] = 1;
  layout->block_grid[1] = 4;
  layout->block_grid[2] = 4;
}


void 
cbe_blocks_create(void)
{

  assert(!spu_ctl.blocks_inited);
  
  if(!spu_ctl.layout) 
    cbe_setup_layout();

  int nblocks = spu_ctl.layout->nblocks; 

  spu_ctl.block_list = calloc(nblocks+1, sizeof(psc_cell_block_t*));

  psc_cell_block_t **curr_block = spu_ctl.block_list;

  int cache_size = 1 * 32 * 32 * NR_FIELDS; 

  for(int i = 0; i < nblocks; i++){
    *curr_block = calloc(1,sizeof(psc_cell_block_t));
    assert(*curr_block != NULL);
    int cni[3];
    cni[1] = i % layout->block_grid[1];
    cni[2] = cni / layout->block_grid[2];
    (*curr_block)->ib[0] = 0;
    (*curr_block)->ib[1] = cni[1] * layout->block_size[1] - 3;
    (*curr_block)->ib[2] = cni[2] * layout->block_size[2] - 3;
    (*curr_block)->im[0] = 1;
    (*curr_block)->im[1] = 32;
    (*curr_block)->im[2] = 32;
    void *m;
    int ierr = posix_memalign(&m, 128, cache_size * sizeof(fields_c_real_t));
    assert(ierr == 0);
    (*curr_block)->wb_flds = (fields_c_real_t *) m;
    curr_block++;
  }
  
  *curr_block = NULL;

  spu_ctl.blocks_inited = 1; 

}

void 
cbe_blocks_destroy(void)
{

  assert(spu_ctl.blocks_inited); 

  psc_cell_block_t ** active; 
  active = spu_ctl.block_list; 

  while(*active != NULL){
    free((*active)->wb_flds);
    free(*active);
    *active = NULL; 
    active++;
  }
}

void 
cbe_assign_parts_to_blocks(particles_cbe_t * pp)
{
  if(! spu_ctl.layout)
    cbe_setup_layout();
  
  if(! spu_ctl.blocks_inited)
    cbe_blocks_create();

  if(! spu_ctl.particles_coarse_sorted)
    psc_sort();

  unsigned int *cnts = spu_ctl.cnts;

  particle_cbe_t *fp = pp->particles; 

  psc_cell_block_t ** curr = spu_ctl.block_list;

  (*curr)->part_start =  (unsigned long long) fp;
  (*curr)->part_end = (unsigned long long) (fp + cnts[0]);
#if PRINT_DEBUG
  fprintf(stderr, "nblocks %d fp: %p \n", spu_ctl.layout->nblocks, fp);
  fprintf(stderr, "[0] cnts[0] %d\n", cnts[0]);
  fprintf(stderr, "[0] start %p end %p\n", (*curr)->part_start, (*curr)->part_end);
#endif
  curr++;
  
  for(int i = 1; i < spu_ctl.layout->nblocks; i++){
    (*curr)->part_start = (unsigned long long)(fp + cnts[i-1]);
    (*curr)->part_end = (unsigned long long)(fp + cnts[i]);
#if PRINT_DEBUG
    fprintf(stderr, "[%d] ctns[%d - 1] %d cnts[%d] %d\n", i, i, cnts[i-1], i, cnts[i]);
    fprintf(stderr, "[%d] start %p end %p\n", i, (*curr)->part_start, (*curr)->part_end);
#endif
    curr++;
  }
  
}

void
cbe_field_blocks_get(fields_c_t *pf, int mb, int me)
{  

  if(! spu_ctl.layout)
    cbe_setup_layout();
  
  if(! spu_ctl.blocks_inited)
    cbe_blocks_create();

  psc_cell_block_t ** curr = spu_ctl.block_list; 

  while ((*curr) != NULL) {
    fields_c_real_t *cache = (fields_c_real_t *) (*curr)->wb_flds; 
    
    int xlo = (*curr)->ib[0];
    int ylo = (*curr)->ib[1];
    int zlo = (*curr)->ib[2];
    
    int xhi = xlo + (*curr)->im[0];
    int yhi = ylo + (*curr)->im[1];
    int zhi = zlo + (*curr)->im[2];
    

    for (int m = mb; m < me; m++) {
      for (int jz = zlo; jz < zhi; jz++) {
	for (int jy = ylo; jy < yhi; jy++){
	  for (int jx = xlo; jx < xhi; jx++){
	    F2_BLOCK((*curr),m,jx,jy,jz) = F3_C(pf,m,jx,jy,jz);
	  }
	}
      }
    }
    
    curr++;
  }
}

void
cbe_field_blocks_put(fields_c_t *pf, int mb, int me)
{  
  psc_cell_block_t ** curr = spu_ctl.block_list; 

  while ((*curr) != NULL) {
    fields_c_real_t *cache = (fields_c_real_t *) (*curr)->wb_flds; 
    
    int xlo = (*curr)->ib[0];
    int ylo = (*curr)->ib[1];
    int zlo = (*curr)->ib[2];
    
    int xhi = xlo + (*curr)->im[0];
    int yhi = ylo + (*curr)->im[1];
    int zhi = zlo + (*curr)->im[2];
    

    for (int m = mb; m < me; m++) {
      for (int jz = zlo; jz < zhi; jz++) {
	for (int jy = ylo; jy < yhi; jy++){
	  for (int jx = xlo; jx < xhi; jx++){
	    F3_C(pf,m,jx,jy,jz) = F2_BLOCK((*curr),m,jx,jy,jz);
	  }
	}
      }
    }
    
    curr++;
  }
}

