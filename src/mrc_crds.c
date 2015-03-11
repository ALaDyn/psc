
#include "mrc_crds_gen_private.h"
#include <mrc_crds.h>
#include <mrc_crds_gen.h>
#include <mrc_params.h>
#include <mrc_domain.h>
#include <mrc_io.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

// static inline
// struct mrc_crds_ops *mrc_crds_ops(struct mrc_crds *crds)
// {
//   return (struct mrc_crds_ops *) crds->obj.ops;
// }

// ----------------------------------------------------------------------
// mrc_crds_* wrappers

static void
_mrc_crds_create(struct mrc_crds *crds)
{
  for (int d = 0; d < 3; d++) {
    char s[20];

    sprintf(s, "crd[%d]", d);
    mrc_fld_set_name(crds->crd[d], s);

    sprintf(s, "dcrd[%d]", d);
    mrc_fld_set_name(crds->dcrd[d], s);
    
    crds->global_crd[d] = mrc_fld_create(mrc_crds_comm(crds));
    sprintf(s, "global_crd[%d]", d);
    mrc_fld_set_name(crds->global_crd[d], s);

    sprintf(s, "crds_gen_%c", 'x' + d);
    mrc_crds_gen_set_name(crds->crds_gen[d], s);
    mrc_crds_gen_set_param_int(crds->crds_gen[d], "d", d);
    mrc_crds_gen_set_param_obj(crds->crds_gen[d], "crds", crds);
  }
}

static void
_mrc_crds_read(struct mrc_crds *crds, struct mrc_io *io)
{
  mrc_crds_read_member_objs(crds, io);
  // this is a carbon copy on all nodes that run the crd_gen, so this
  // write is only for checkpointing
  if (strcmp(mrc_io_type(io), "hdf5_serial") == 0) { // FIXME  
    crds->global_crd[0] = mrc_io_read_ref(io, crds, "global_crd[0]", mrc_fld);
    crds->global_crd[1] = mrc_io_read_ref(io, crds, "global_crd[1]", mrc_fld);
    crds->global_crd[2] = mrc_io_read_ref(io, crds, "global_crd[2]", mrc_fld);
  }
}

static void
_mrc_crds_write(struct mrc_crds *crds, struct mrc_io *io)
{
  int slab_off_save[3], slab_dims_save[3];
  if (strcmp(mrc_io_type(io), "xdmf_collective") == 0) { // FIXME
    mrc_io_get_param_int3(io, "slab_off", slab_off_save);
    mrc_io_get_param_int3(io, "slab_dims", slab_dims_save);
    mrc_io_set_param_int3(io, "slab_off", (int[3]) { 0, 0, 0});
    mrc_io_set_param_int3(io, "slab_dims", (int[3]) { 0, 0, 0 });
  }

  for (int d = 0; d < 3; d++) {
    struct mrc_fld *crd_cc = crds->crd[d];
    if (strcmp(mrc_io_type(io), "xdmf_collective") == 0) { // FIXME
      struct mrc_fld *crd_nc = crds->crd_nc[d];
      if (!crd_nc) {
	crd_nc = mrc_fld_create(mrc_crds_comm(crds)); // FIXME, leaked
	crds->crd_nc[d] = crd_nc;
	char s[10];
	sprintf(s, "crd%d_nc", d);
	mrc_fld_set_name(crd_nc, s);
	mrc_fld_set_param_obj(crd_nc, "domain", crds->domain);
	mrc_fld_set_param_int(crd_nc, "nr_spatial_dims", 1);
	mrc_fld_set_param_int(crd_nc, "dim", d);
	mrc_fld_set_param_int(crd_nc, "nr_ghosts", 1);
	mrc_fld_setup(crd_nc);
	mrc_fld_set_comp_name(crd_nc, 0, s);

	mrc_m1_foreach_patch(crd_nc, p) {
	  if (crds->sw > 0) {
	    mrc_m1_foreach(crd_cc, i, 0, 1) {
	      MRC_M1(crd_nc,0, i, p) = .5 * (MRC_M1(crd_cc,0, i-1, p) + MRC_M1(crd_cc,0, i, p));
	    } mrc_m1_foreach_end;
	  } else {
	    mrc_m1_foreach(crd_cc, i, -1, 0) {
	      MRC_M1(crd_nc,0, i, p) = .5 * (MRC_M1(crd_cc,0, i-1, p) + MRC_M1(crd_cc,0, i, p));
	    } mrc_m1_foreach_end;
	    int ld = mrc_fld_dims(crd_nc)[0];
	    // extrapolate
	    MRC_M1(crd_nc,0, 0 , p) = MRC_M1(crd_cc,0, 0   , p)
	      - .5 * (MRC_M1(crd_cc,0,    1, p) - MRC_M1(crd_cc,0, 0   , p));
	    MRC_M1(crd_nc,0, ld, p) = MRC_M1(crd_cc,0, ld-1, p)
	      + .5 * (MRC_M1(crd_cc,0, ld-1, p) - MRC_M1(crd_cc,0, ld-2, p));
	  }
	}
      }
      int gdims[3];
      mrc_domain_get_global_dims(crds->domain, gdims);
      // FIXME, this is really too hacky... should per m1 / m3, not per mrc_io
      mrc_io_set_param_int3(io, "slab_off", (int[3]) { 0, 0, 0});
      mrc_io_set_param_int3(io, "slab_dims", (int[3]) { gdims[d] + 1, 0, 0 });
      mrc_fld_write(crd_nc, io);
    }
  }

  // this is a carbon copy on all nodes that run the crd_gen, so this
  // write is only for checkpointing
  if (strcmp(mrc_io_type(io), "hdf5_serial") == 0) { // FIXME
    mrc_io_write_ref(io, crds, "global_crd[0]", crds->global_crd[0]);
    mrc_io_write_ref(io, crds, "global_crd[1]", crds->global_crd[1]);
    mrc_io_write_ref(io, crds, "global_crd[2]", crds->global_crd[2]);
  }

  if (strcmp(mrc_io_type(io), "xdmf_collective") == 0) { // FIXME
    mrc_io_set_param_int3(io, "slab_off", slab_off_save);
    mrc_io_set_param_int3(io, "slab_dims", slab_dims_save);
  }
}

// FIXME, should go away / superseded by mrc_crds_get_dx()
void
mrc_crds_get_dx_base(struct mrc_crds *crds, double dx[3])
{
  int gdims[3];
  mrc_domain_get_global_dims(crds->domain, gdims);
  // FIXME, only makes sense for uniform coords, should be dispatched!!!
  for (int d = 0; d < 3; d++) {
    dx[d] = (crds->xh[d] - crds->xl[d]) / gdims[d];
  }
}

void
mrc_crds_get_dx(struct mrc_crds *crds, int p, double dx[3])
{
  // FIXME, only for uniform crds, should be dispatched!
  dx[0] = MRC_MCRDX(crds, 1, p) - MRC_MCRDX(crds, 0, p);
  dx[1] = MRC_MCRDY(crds, 1, p) - MRC_MCRDY(crds, 0, p);
  dx[2] = MRC_MCRDZ(crds, 1, p) - MRC_MCRDZ(crds, 0, p);
}

// allocate the coordinate fields common to all crds types.
static void
mrc_crds_setup_alloc_only(struct mrc_crds *crds)
{
  assert(crds->domain && mrc_domain_is_setup(crds->domain));

  for (int d = 0; d < 3; d++) {
    mrc_fld_set_param_obj(crds->crd[d], "domain", crds->domain);
    mrc_fld_set_param_int(crds->crd[d], "nr_spatial_dims", 1);
    mrc_fld_set_param_int(crds->crd[d], "dim", d);
    mrc_fld_set_param_int(crds->crd[d], "nr_ghosts", crds->sw);
    mrc_fld_set_comp_name(crds->crd[d], 0, mrc_fld_name(crds->crd[d]));
    mrc_fld_setup(crds->crd[d]);

    // alloc double version of coords
    mrc_fld_set_type(crds->dcrd[d], "double");
    mrc_fld_set_param_obj(crds->dcrd[d], "domain", crds->domain);
    mrc_fld_set_param_int(crds->dcrd[d], "nr_spatial_dims", 1);
    mrc_fld_set_param_int(crds->dcrd[d], "dim", d);
    mrc_fld_set_param_int(crds->dcrd[d], "nr_ghosts", crds->sw);
    mrc_fld_set_comp_name(crds->dcrd[d], 0, mrc_fld_name(crds->dcrd[d]));
    mrc_fld_setup(crds->dcrd[d]);

  }
}

// Allocate global coordinate fields (for domains that they make sense)
static void
mrc_crds_setup_alloc_global_array(struct mrc_crds *crds)
{
  int gdims[3];
  mrc_domain_get_global_dims(crds->domain, gdims);
  
  for (int d = 0; d < 3; d++) {
    mrc_fld_set_type(crds->global_crd[d], "double");
    mrc_fld_set_param_int_array(crds->global_crd[d], "dims", 2, (int[2]) { gdims[d], 2 });
    mrc_fld_set_param_int_array(crds->global_crd[d], "sw"  , 2, (int[2]) { crds->sw, 0 });
    mrc_fld_setup(crds->global_crd[d]);
  }
}

static void
_mrc_crds_setup(struct mrc_crds *crds)
{
  int gdims[3];
  double xl[3], xh[3];
  int nr_patches;
  struct mrc_patch *patches;

  mrc_crds_setup_alloc_only(crds);
  mrc_crds_setup_alloc_global_array(crds);

  mrc_domain_get_global_dims(crds->domain, gdims);
  patches = mrc_domain_get_patches(crds->domain, &nr_patches);

  for (int d = 0; d < 3; d ++) {
    struct mrc_fld *x = crds->global_crd[d];

    struct mrc_crds_gen *gen = crds->crds_gen[d];
    mrc_crds_get_param_double3(gen->crds, "l", xl);
    mrc_crds_get_param_double3(gen->crds, "h", xh);

    mrc_crds_gen_set_param_int(gen, "n", gdims[d]);
    mrc_crds_gen_set_param_int(gen, "sw", gen->crds->sw);
    mrc_crds_gen_set_param_double(gen, "xl", xl[d]);
    mrc_crds_gen_set_param_double(gen, "xh", xh[d]);
    mrc_crds_gen_run(gen, &MRC_D2(x, 0, 0), &MRC_D2(x, 0, 1));

    mrc_fld_foreach_patch(crds->crd[d], p) {
      // shift to beginning of local domain
      int off = patches[p].off[d];

      mrc_m1_foreach_bnd(crds->crd[d], ix) {
	MRC_DMCRD(crds, d, ix, p) = MRC_D2(x, ix + off, 0);
	MRC_MCRD(crds, d, ix, p) = (float)MRC_D2(x, ix + off, 0);
      } mrc_m1_foreach_end;
    }
  }
}

static void
_mrc_crds_destroy(struct mrc_crds *crds)
{
  for (int d=0; d < 3; d++) {
    mrc_fld_destroy(crds->global_crd[d]);
    mrc_fld_destroy(crds->crd_nc[d]);
  }
}


// ======================================================================
// mrc_crds_uniform

static void
mrc_crds_uniform_setup(struct mrc_crds *crds)
{
  for (int d = 0; d < 3; d++) {
    assert(strcmp(mrc_crds_gen_type(crds->crds_gen[d]), "uniform") == 0);
  }

  mrc_crds_setup_super(crds);
}

static struct mrc_crds_ops mrc_crds_uniform_ops = {
  .name  = "uniform",
  .setup = mrc_crds_uniform_setup,
};

// ======================================================================
// mrc_crds_rectilinear

static struct mrc_crds_ops mrc_crds_rectilinear_ops = {
  .name       = "rectilinear",
};

// ======================================================================
// mrc_crds_amr_uniform

// FIXME, this should use mrc_a1 not mrc_m1

static void
mrc_crds_amr_uniform_setup(struct mrc_crds *crds)
{
  mrc_crds_setup_alloc_only(crds);

  int gdims[3];
  mrc_domain_get_global_dims(crds->domain, gdims);
  double *xl = crds->xl, *xh = crds->xh;

  for (int d = 0; d < 3; d++) {
    struct mrc_fld *mcrd = crds->crd[d];
    struct mrc_fld *dcrd = crds->dcrd[d];
    mrc_m1_foreach_patch(mcrd, p) {
      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(crds->domain, p, &info);
      double xb = (double) info.off[d] / (1 << info.level);
      double xe = (double) (info.off[d] + info.ldims[d]) / (1 << info.level);
      double dx = (xe - xb) / info.ldims[d];

      mrc_m1_foreach_bnd(mcrd, i) {
	MRC_D3(dcrd,i, 0, p) = xl[d] + (xb + (i + .5) * dx) / gdims[d] * (xh[d] - xl[d]);
	MRC_M1(mcrd,0, i, p) = (float)MRC_D3(dcrd,i, 0, p);
      } mrc_m1_foreach_end;
    }
  }
}

static struct mrc_crds_ops mrc_crds_amr_uniform_ops = {
  .name  = "amr_uniform",
  .setup = mrc_crds_amr_uniform_setup,
};


// ======================================================================
// mrc_crds_mb
// We need a new type of coords to be able to handle multi-block domains,
// since they don't generally have a global coordinate system. We'll iterate
// through and create each block local coordinate system.


static void
mrc_crds_mb_create(struct mrc_crds *crds)
{
  // Name the standard 3 crds_gen to "UNUSED" to (hopefully) avoid some confusion
  for (int d = 0; d < 3; d++) {
    mrc_crds_gen_set_name(crds->crds_gen[d], "UNUSED");
  }
}

static void
mrc_crds_mb_read(struct mrc_crds *crds, struct mrc_io *io)
{
  // need to make sure subclass create doesn't get called at read, but
  // the superclass read does.
  mrc_crds_read_super(crds, io);
}

static void
mrc_crds_mb_setup(struct mrc_crds *crds)
{
  // this should still work
  mrc_crds_setup_alloc_only(crds);

  typedef void (*dgb_t)(struct mrc_domain *, struct MB_block **pblock, int *nr_blocks);
  dgb_t domain_get_blocks = (dgb_t) mrc_domain_get_method(crds->domain, "get_blocks");

  int nr_blocks;
  struct MB_block *blocks;

  domain_get_blocks(crds->domain, &blocks, &nr_blocks);

  int nr_patches;
  mrc_domain_get_patches(crds->domain, &nr_patches);
  int sw = crds->sw;

  for (int b = 0; b < nr_blocks; b++)
    {
      struct MB_block *block = &(blocks[b]);

      for (int d = 0; d < 3; d ++) {
	struct mrc_fld *x = mrc_fld_create(MPI_COMM_SELF);
	mrc_fld_set_type(x, "double");
	mrc_fld_set_param_int_array(x, "dims", 2, (int[2]) { block->mx[d] + 1, 2 });
	mrc_fld_set_param_int_array(x, "sw"  , 2, (int[2]) { sw, 0 });
	mrc_fld_setup(x);

	// If I had my way, I'd kill off the original crds_gen children to minimize confusion,
	// but I guess I'll just have to write the docs to make it clear what's going on here.
	assert(block->coord_gen[d]);

	mrc_crds_gen_set_param_int(block->coord_gen[d], "n", block->mx[d]);
	mrc_crds_gen_set_param_int(block->coord_gen[d], "d", d);
	mrc_crds_gen_set_param_int(block->coord_gen[d], "sw", sw);
	mrc_crds_gen_set_param_obj(block->coord_gen[d], "crds", crds);
	mrc_crds_gen_set_param_double(block->coord_gen[d], "xl", block->xl[d]);
	mrc_crds_gen_set_param_double(block->coord_gen[d], "xh", block->xh[d]);

	mrc_crds_gen_run(block->coord_gen[d], &MRC_D2(x, 0, 0), &MRC_D2(x, 0, 1));
	
	mrc_m1_foreach_patch(crds->crd[d], p) {
	  struct mrc_patch_info info;
	  mrc_domain_get_local_patch_info(crds->domain, p, &info);
	  if (b == info.p_block) {
	    // This is offset of the patch in the block
	    int off = info.p_ix[d];
	    mrc_m1_foreach_bnd(crds->crd[d], ix) {
	      MRC_DMCRD(crds, d, ix, p) = MRC_D2(x, ix + off, 0);
	      MRC_MCRD(crds, d, ix, p) = (float)MRC_D2(x, ix + off, 0);
	    } mrc_m1_foreach_end;

	  }
	}
	mrc_fld_destroy(x);
      }
    }

 }


static struct mrc_crds_ops mrc_crds_mb_ops = {
  .name      = "mb",
  .create    = mrc_crds_mb_create,
  .setup     = mrc_crds_mb_setup,
  .read      = mrc_crds_mb_read,
};

// ======================================================================
// mrc_crds_init

extern struct mrc_crds_ops mrc_crds_two_gaussian_ops;
extern struct mrc_crds_ops mrc_crds_gaussian_ops;
extern struct mrc_crds_ops mrc_crds_gaussian_2D_ops;

static void
mrc_crds_init()
{
  mrc_class_register_subclass(&mrc_class_mrc_crds, &mrc_crds_uniform_ops);
  mrc_class_register_subclass(&mrc_class_mrc_crds, &mrc_crds_rectilinear_ops);
  mrc_class_register_subclass(&mrc_class_mrc_crds, &mrc_crds_amr_uniform_ops);
  mrc_class_register_subclass(&mrc_class_mrc_crds, &mrc_crds_mb_ops);
}

// ======================================================================
// mrc_crds class

#define VAR(x) (void *)offsetof(struct mrc_crds, x)
static struct param mrc_crds_params_descr[] = {
  { "l"              , VAR(xl)            , PARAM_DOUBLE3(0., 0., 0.) },
  { "h"              , VAR(xh)            , PARAM_DOUBLE3(1., 1., 1.) },
  { "sw"             , VAR(sw)            , PARAM_INT(0)             },
  { "domain"         , VAR(domain)        , PARAM_OBJ(mrc_domain)    },

  { "crd[0]"         , VAR(crd[0])        , MRC_VAR_OBJ(mrc_fld)     },
  { "crd[1]"         , VAR(crd[1])        , MRC_VAR_OBJ(mrc_fld)     },
  { "crd[2]"         , VAR(crd[2])        , MRC_VAR_OBJ(mrc_fld)     },

  { "dcrd[0]"        , VAR(dcrd[0])       , MRC_VAR_OBJ(mrc_fld)     },
  { "dcrd[1]"        , VAR(dcrd[1])       , MRC_VAR_OBJ(mrc_fld)     },
  { "dcrd[2]"        , VAR(dcrd[2])       , MRC_VAR_OBJ(mrc_fld)     },

  { "crds_gen_x"     , VAR(crds_gen[0])   , MRC_VAR_OBJ(mrc_crds_gen)},
  { "crds_gen_y"     , VAR(crds_gen[1])   , MRC_VAR_OBJ(mrc_crds_gen)},
  { "crds_gen_z"     , VAR(crds_gen[2])   , MRC_VAR_OBJ(mrc_crds_gen)},

  {},
};
#undef VAR

struct mrc_class_mrc_crds mrc_class_mrc_crds = {
  .name         = "mrc_crds",
  .size         = sizeof(struct mrc_crds),
  .param_descr  = mrc_crds_params_descr,
  .init         = mrc_crds_init,
  .destroy      = _mrc_crds_destroy,
  .create       = _mrc_crds_create,
  .write        = _mrc_crds_write,
  .read         = _mrc_crds_read,
  .setup        = _mrc_crds_setup,
};

