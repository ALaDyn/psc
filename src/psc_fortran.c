
#include "psc.h"
#include <mrc_profile.h>

static void
fortran_push_part_xy(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xy", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_xy(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_xz(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xz", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_xz(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_yz(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_yz(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_xyz(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_xyz", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_xyz(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_z(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_z", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_z(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_yz_a(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz_a", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_yz_a(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

static void
fortran_push_part_yz_b(mfields_base_t *flds_base, mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  fields_fortran_t pf;
  fields_fortran_get(&pf, EX, EX + 6, flds_base);
  
  static int pr;
  if (!pr) {
    pr = prof_register("fort_part_yz_b", 1., 0, 0);
  }
  prof_start(pr);
  PIC_push_part_yz_b(&pp, &pf);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
  fields_fortran_put(&pf, JXI, JXI + 3, flds_base);
}

struct psc_ops psc_ops_fortran = {
  .name = "fortran",
  .push_part_xy           = fortran_push_part_xy,
  .push_part_xz           = fortran_push_part_xz,
  .push_part_yz           = fortran_push_part_yz,
  .push_part_xyz          = fortran_push_part_xyz,
  .push_part_z            = fortran_push_part_z,
  .push_part_yz_a         = fortran_push_part_yz_a,
  .push_part_yz_b         = fortran_push_part_yz_b,
};

// ======================================================================
// fortran randomize

static void
fortran_randomize(mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_randomize", 1., 0, 0);
  }
  prof_start(pr);
  PIC_randomize(&pp);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
}

struct psc_randomize_ops psc_randomize_ops_fortran = {
  .name      = "fortran",
  .randomize = fortran_randomize,
};

// ======================================================================
// fortran sort

static void
fortran_sort(mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_sort", 1., 0, 0);
  }
  prof_start(pr);
  PIC_find_cell_indices(&pp);
  PIC_sort(&pp);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
}

struct psc_sort_ops psc_sort_ops_fortran = {
  .name = "fortran",
  .sort = fortran_sort,
};

// ======================================================================
// fortran collision

static void
fortran_collision(mparticles_base_t *particles_base)
{
  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_collision", 1., 0, 0);
  }
  prof_start(pr);
  PIC_bin_coll(&pp);
  prof_stop(pr);

  particles_fortran_put(&pp, particles_base);
}

struct psc_collision_ops psc_collision_ops_fortran = {
  .name      = "fortran",
  .collision = fortran_collision,
};

// ======================================================================
// fortran push field

static void
fortran_push_field_a(mfields_base_t *flds_base)
{
  if (psc.domain.use_pml) {
    fields_fortran_t pf;
    fields_fortran_get(&pf, JXI, MU + 1, flds_base);
    
    static int pr;
    if (!pr) {
      pr = prof_register("fort_field_pml_a", 1., 0, 0);
    }
    prof_start(pr);
    PIC_pml_msa(&pf);
    prof_stop(pr);
    
    fields_fortran_put(&pf, EX, BZ + 1, flds_base);
  } else {
    fields_fortran_t pf;
    fields_fortran_get(&pf, JXI, HZ + 1, flds_base);
    
    static int pr;
    if (!pr) {
      pr = prof_register("fort_field_a", 1., 0, 0);
    }
    prof_start(pr);
    PIC_msa(&pf);
    prof_stop(pr);
    
    fields_fortran_put(&pf, EX, HZ + 1, flds_base);
  }
}

static void
fortran_push_field_b(mfields_base_t *flds_base)
{
  if (psc.domain.use_pml) {
    fields_fortran_t pf;
    fields_fortran_get(&pf, JXI, MU + 1, flds_base);
    
    static int pr;
    if (!pr) {
      pr = prof_register("fort_field_pml_b", 1., 0, 0);
    }
    prof_start(pr);
    PIC_pml_msb(&pf);
    prof_stop(pr);
    
    fields_fortran_put(&pf, EX, BZ + 1, flds_base);
  } else {
    fields_fortran_t pf;
    fields_fortran_get(&pf, JXI, HZ + 1, flds_base);
    
    static int pr;
    if (!pr) {
      pr = prof_register("fort_field_b", 1., 0, 0);
    }
    prof_start(pr);
    PIC_msb(&pf);
    prof_stop(pr);
    
    fields_fortran_put(&pf, EX, HZ + 1, flds_base);
  }
}

struct psc_push_field_ops psc_push_field_ops_fortran = {
  .name         = "fortran",
  .push_field_a = fortran_push_field_a,
  .push_field_b = fortran_push_field_b,
};

// ======================================================================
// fortran output

static void
fortran_out_field()
{
  assert(FIELDS_BASE == FIELDS_FORTRAN);
  static int pr;
  if (!pr) {
    pr = prof_register("fort_out_field", 1., 0, 0);
  }
  prof_start(pr);
  OUT_field();
  prof_stop(pr);
}

struct psc_output_ops psc_output_ops_fortran = {
  .name      = "fortran",
  .out_field = fortran_out_field,
};

// ======================================================================
// fortran bnd

static void
fortran_add_ghosts(mfields_base_t *flds_base, int mb, int me)
{
  assert(psc.nr_patches == 1);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_add_ghosts", 1., 0, 0);
  }
  prof_start(pr);

  fields_fortran_t pf;
  fields_fortran_get(&pf, mb, me, flds_base);

  for (int m = mb; m < me; m++) {
    if (psc.domain.gdims[0] > 1) {
      PIC_fax(&pf, m);
    }
    if (psc.domain.gdims[1] > 1) {
      PIC_fay(&pf, m);
    }
    if (psc.domain.gdims[2] > 1) {
      PIC_faz(&pf, m);
    }
  }

  fields_fortran_put(&pf, mb, me, flds_base);

  prof_stop(pr);
}

static void
fortran_fill_ghosts(mfields_base_t *flds_base, int mb, int me)
{
  assert(psc.nr_patches == 1);

  static int pr;
  if (!pr) {
    pr = prof_register("fort_fill_ghosts", 1., 0, 0);
  }
  prof_start(pr);

  fields_fortran_t pf;
  fields_fortran_get(&pf, mb, me, flds_base);

  for (int m = mb; m < me; m++) {
    if (psc.domain.gdims[0] > 1) {
      PIC_fex(&pf, m);
    }
    if (psc.domain.gdims[1] > 1) {
      PIC_fey(&pf, m);
    }
    if (psc.domain.gdims[2] > 1) {
      PIC_fez(&pf, m);
    }
  }

  fields_fortran_put(&pf, mb, me, flds_base);

  prof_stop(pr);
}

static void
fortran_exchange_particles(mparticles_base_t *particles_base)
{
  static int pr;
  if (!pr) {
    pr = prof_register("fort_xchg_part", 1., 0, 0);
  }
  prof_start(pr);

  particles_fortran_t pp;
  particles_fortran_get(&pp, particles_base);
  
  SET_param_coeff();
  SET_niloc(pp.n_part);

  if (psc.domain.gdims[0] > 1) {
    PIC_pex();
  }
  if (psc.domain.gdims[1] > 1) {
    PIC_pey();
  }
  if (psc.domain.gdims[2] > 1) {
    PIC_pez();
  }

  GET_niloc(&pp.n_part);
  // don't really reallocate, just get the new array pointer
  // if PIC_pe[xyz]() reallocated during the previous calls
  pp.particles = REALLOC_particles(pp.n_part);
  particles_fortran_put(&pp, particles_base);

  prof_stop(pr);
}

struct psc_bnd_ops psc_bnd_ops_fortran = {
  .name               = "fortran",
  .add_ghosts         = fortran_add_ghosts,
  .fill_ghosts        = fortran_fill_ghosts,
  .exchange_particles = fortran_exchange_particles,
};

// ======================================================================
// fortran moment

static void
fortran_calc_densities(int p, fields_base_t *pf_base, particles_base_t *pp_base,
		       fields_base_t *pf)
{
  static int pr;
  if (!pr) {
    pr = prof_register("fort_densities", 1., 0, 0);
  }
  prof_start(pr);

  particles_fortran_t pp;
  particles_fortran_get(&pp, &psc.particles);
  fields_fortran_t pf_fortran;
  fields_fortran_get_from(&pf_fortran, 0, 0, pf, 0);

  CALC_densities(&pp, &pf_fortran);

  particles_fortran_put(&pp, &psc.particles);
  fields_fortran_put_to(&pf_fortran, NE, NE + 3, pf, 0);

  prof_stop(pr);
}

struct psc_moment_ops psc_moment_ops_fortran = {
  .name               = "fortran",
  .calc_densities     = fortran_calc_densities,
};

