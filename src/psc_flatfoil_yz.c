
#include <psc.h>
#include <psc_push_fields.h>
#include <psc_bnd_fields.h>
#include <psc_event_generator_private.h>
#include <psc_heating.h>
#include <psc_target_private.h>
#include <psc_output_fields_item.h>
#include <psc_bnd.h>

#include <psc_particles_as_single.h> // FIXME
#include <psc_fields_c.h> // FIXME

#include <mrc_io.h>
#include <mrc_params.h>

#include <math.h>
#include <stdlib.h>

// ======================================================================
// psc_inject

MRC_CLASS_DECLARE(psc_inject, struct psc_inject);

struct psc_inject {
  struct mrc_obj obj;

  // params
  bool do_inject; // whether to inject particles at all
  int every_step; // inject every so many steps
  int tau; // in steps
};

#define VAR(x) (void *)offsetof(struct psc_inject, x)
static struct param psc_inject_descr[] _mrc_unused = {
  { "do_inject"  , VAR(do_inject)  , PARAM_BOOL(true)         },
  { "every_step" , VAR(every_step) , PARAM_INT(20)            },
  { "tau"        , VAR(tau)        , PARAM_INT(40)            },

  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_inject class

struct mrc_class_psc_inject mrc_class_psc_inject = {
  .name             = "psc_inject",
  .size             = sizeof(struct psc_inject),
  .param_descr      = psc_inject_descr,
};

// ======================================================================
// psc subclass "flatfoil"

struct psc_flatfoil {
  double BB;
  double Zi;
  double LLf;
  double LLz;
  double LLy;

  double background_n;
  double background_Te;
  double background_Ti;

  bool no_initial_target; // for testing, the target can be turned off in the initial condition

  double target_yl;
  double target_yh;
  double target_zwidth;
  struct psc_target *target;

  struct psc_inject *inject;

  double heating_zl; // this is ugly as these are used to set the corresponding
  double heating_zh; // quantities in psc_heating, but having them here we can rescale
  double heating_xc; // them from d_i to internal (d_e) units
  double heating_yc;
  double heating_rH;
  struct psc_heating *heating;
  
  // state
  double d_i;
  double LLs;
  double LLn;

  struct psc_mfields *mflds_n;
  struct psc_output_fields_item *item_n;
  struct psc_bnd *item_n_bnd;
};

#define psc_flatfoil(psc) mrc_to_subobj(psc, struct psc_flatfoil)

#define VAR(x) (void *)offsetof(struct psc_flatfoil, x)
static struct param psc_flatfoil_descr[] = {
  { "BB"                , VAR(BB)                , PARAM_DOUBLE(.0)         },
  { "Zi"                , VAR(Zi)                , PARAM_DOUBLE(1.)         },
  { "LLf"               , VAR(LLf)               , PARAM_DOUBLE(25.)        },
  { "LLz"               , VAR(LLz)               , PARAM_DOUBLE(400.*4)     },
  { "LLy"               , VAR(LLy)               , PARAM_DOUBLE(400.)       },

  { "background_n"      , VAR(background_n)      , PARAM_DOUBLE(.002)       },
  { "background_Te"     , VAR(background_Te)     , PARAM_DOUBLE(.001)       },
  { "background_Ti"     , VAR(background_Ti)     , PARAM_DOUBLE(.001)       },

  { "target_yl"         , VAR(target_yl)         , PARAM_DOUBLE(-100000.)   },
  { "target_yh"         , VAR(target_yh)         , PARAM_DOUBLE( 100000.)   },
  { "target_zwidth"     , VAR(target_zwidth)     , PARAM_DOUBLE(1.)         },

  { "no_initial_target" , VAR(no_initial_target) , PARAM_BOOL(false)        },

  { "heating_zl"        , VAR(heating_zl)        , PARAM_DOUBLE(-1.)        },
  { "heating_zh"        , VAR(heating_zh)        , PARAM_DOUBLE(1.)         },
  { "heating_xc"        , VAR(heating_xc)        , PARAM_DOUBLE(0.)         },
  { "heating_yc"        , VAR(heating_yc)        , PARAM_DOUBLE(0.)         },
  { "heating_rH"        , VAR(heating_rH)        , PARAM_DOUBLE(3.)         },

  { "LLs"               , VAR(LLs)               , MRC_VAR_DOUBLE           },
  { "LLn"               , VAR(LLn)               , MRC_VAR_DOUBLE           },
  { "target"            , VAR(target)            , MRC_VAR_OBJ(psc_target)  },
  { "inject"            , VAR(inject)            , MRC_VAR_OBJ(psc_inject)  },
  { "heating"           , VAR(heating)           , MRC_VAR_OBJ(psc_heating) },
  {},
};
#undef VAR

enum {
  MY_ION,
  MY_ELECTRON,
  N_MY_KINDS,
};

// ----------------------------------------------------------------------
// psc_flatfoil_create

static void
psc_flatfoil_create(struct psc *psc)
{
  psc_default_dimensionless(psc);

  psc->prm.nmax = 210001;
  psc->prm.nicell = 100;
  psc->prm.gdims_in_terms_of_cells = true;
  psc->prm.nr_populations = N_MY_KINDS;
  psc->prm.fractional_n_particles_per_cell = true;
  psc->prm.cfl = 0.75;

  psc->domain.gdims[0] = 1;
  psc->domain.gdims[1] = 1600;
  psc->domain.gdims[2] = 1600*4;

  psc->domain.bnd_fld_lo[0] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_hi[0] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_lo[1] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_hi[1] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_lo[2] = BND_FLD_PERIODIC;
  psc->domain.bnd_fld_hi[2] = BND_FLD_PERIODIC;
  psc->domain.bnd_part_lo[0] = BND_PART_PERIODIC;
  psc->domain.bnd_part_hi[0] = BND_PART_PERIODIC;
  psc->domain.bnd_part_lo[1] = BND_PART_PERIODIC;
  psc->domain.bnd_part_hi[1] = BND_PART_PERIODIC;
  psc->domain.bnd_part_lo[2] = BND_PART_PERIODIC;
  psc->domain.bnd_part_hi[2] = BND_PART_PERIODIC;

  struct psc_bnd_fields *bnd_fields = 
    psc_push_fields_get_bnd_fields(psc->push_fields);
  psc_bnd_fields_set_type(bnd_fields, "none");

  psc_event_generator_set_type(psc->event_generator, "flatfoil");
}

// ----------------------------------------------------------------------
// psc_flatfoil_setup

static void
psc_flatfoil_setup(struct psc *psc)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  sub->LLs = 4. * sub->LLf;
  sub->LLn = .5 * sub->LLf;
  
  psc->domain.length[0] = 1.;
  psc->domain.length[1] = sub->LLy;
  psc->domain.length[2] = sub->LLz;

  // center around origin
  for (int d = 0; d < 3; d++) {
    psc->domain.corner[d] = -.5 * psc->domain.length[d];
  }

  // last population is neutralizing
  psc->kinds[MY_ELECTRON].q = -1.;
  psc->kinds[MY_ELECTRON].m = 1.;
  psc->kinds[MY_ELECTRON].name = "e";

  psc->kinds[MY_ION     ].q = sub->Zi;
  psc->kinds[MY_ION     ].m = 100. * sub->Zi;  // FIXME, hardcoded mass ratio 100
  psc->kinds[MY_ION     ].name = "i";

  sub->d_i = sqrt(psc->kinds[MY_ION].m / psc->kinds[MY_ION].q);

  psc_target_set_param_double(sub->target, "yl", sub->target_yl * sub->d_i);
  psc_target_set_param_double(sub->target, "yh", sub->target_yh * sub->d_i);
  psc_target_set_param_double(sub->target, "zl", - sub->target_zwidth * sub->d_i);
  psc_target_set_param_double(sub->target, "zh",   sub->target_zwidth * sub->d_i);

  psc_heating_set_param_double(sub->heating, "zl", sub->heating_zl * sub->d_i);
  psc_heating_set_param_double(sub->heating, "zh", sub->heating_zh * sub->d_i);
  psc_heating_set_param_double(sub->heating, "xc", sub->heating_xc * sub->d_i);
  psc_heating_set_param_double(sub->heating, "yc", sub->heating_yc * sub->d_i);
  psc_heating_set_param_double(sub->heating, "rH", sub->heating_rH * sub->d_i);
  psc_heating_set_param_double(sub->heating, "Mi", psc->kinds[MY_ION].m);
  psc_heating_set_param_int(sub->heating, "kind", MY_ELECTRON);

  psc_setup_super(psc);
  psc_setup_member_objs_sub(psc);

  MPI_Comm comm = psc_comm(psc);
  mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., sub->d_i);
  mpi_printf(comm, "lambda_De (background) = %g\n", sqrt(sub->background_Te));

  // set up necessary bits for calculating / averaging density moment

  sub->item_n_bnd = psc_bnd_create(psc_comm(psc));
  psc_bnd_set_type(sub->item_n_bnd, "single");
  psc_bnd_set_psc(sub->item_n_bnd, psc);
  psc_bnd_setup(sub->item_n_bnd);

  sub->item_n = psc_output_fields_item_create(psc_comm(psc));
  psc_output_fields_item_set_type(sub->item_n, "n_1st_single");
  psc_output_fields_item_set_psc_bnd(sub->item_n, sub->item_n_bnd);
  psc_output_fields_item_setup(sub->item_n);

  sub->mflds_n = psc_output_fields_item_create_mfields(sub->item_n);
  psc_mfields_set_name(sub->mflds_n, "mflds_n");
}

// ----------------------------------------------------------------------
// psc_flatfoil_destroy

static void
psc_flatfoil_destroy(struct psc *psc)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  psc_mfields_destroy(sub->mflds_n);
  psc_output_fields_item_destroy(sub->item_n);
  psc_bnd_destroy(sub->item_n_bnd);
}

// ----------------------------------------------------------------------
// psc_flatfoil_read

static void
psc_flatfoil_read(struct psc *psc, struct mrc_io *io)
{
  psc_read_super(psc, io);
}

// ----------------------------------------------------------------------
// psc_flatfoil_init_field

static double
psc_flatfoil_init_field(struct psc *psc, double x[3], int m)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  double BB = sub->BB;

  switch (m) {
  case HY:
    return BB;

  default:
    return 0.;
  }
}

// ----------------------------------------------------------------------
// psc_flatfoil_init_npt

static void
psc_flatfoil_init_npt(struct psc *psc, int pop, double x[3],
		    struct psc_particle_npt *npt)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);
  struct psc_target *target = sub->target;

  switch (pop) {
  case MY_ION:
    npt->n    = sub->background_n;
    npt->T[0] = sub->background_Ti;
    npt->T[1] = sub->background_Ti;
    npt->T[2] = sub->background_Ti;
    break;
  case MY_ELECTRON:
    npt->n    = sub->background_n;
    npt->T[0] = sub->background_Te;
    npt->T[1] = sub->background_Te;
    npt->T[2] = sub->background_Te;
    break;
  default:
    assert(0);
  }

  if (!psc_target_is_inside(target, x)) {
    return;
  }

  if (sub->no_initial_target && psc->timestep == 0) {
    return;
  }

  // replace values above by target values

  switch (pop) {
  case MY_ION:
    npt->n    = target->n;
    npt->T[0] = target->Ti;
    npt->T[1] = target->Ti;
    npt->T[2] = target->Ti;
    break;
  case MY_ELECTRON:
    npt->n    = target->n;
    npt->T[0] = target->Te;
    npt->T[1] = target->Te;
    npt->T[2] = target->Te;
    break;
  default:
    assert(0);
  }
}

// ----------------------------------------------------------------------
// debug_dump

static void
copy_to_mrc_fld(struct mrc_fld *m3, struct psc_mfields *mflds)
{
  psc_foreach_patch(ppsc, p) {
    struct psc_fields *flds = psc_mfields_get_patch(mflds, p);
    struct mrc_fld_patch *m3p = mrc_fld_patch_get(m3, p);
    mrc_fld_foreach(m3, ix,iy,iz, 0,0) {
      for (int m = 0; m < mflds->nr_fields; m++) {
	MRC_M3(m3p, m, ix,iy,iz) = F3_C(flds, m, ix,iy,iz);
      }
    } mrc_fld_foreach_end;
    mrc_fld_patch_put(m3);
  }
}

static void _mrc_unused
debug_dump(struct mrc_io *io, struct psc_mfields *mflds)
{
  /* if (ppsc->timestep % debug_every_step != 0) { */
  /*   return; */
  /* } */

  struct mrc_fld *mrc_fld = mrc_domain_m3_create(ppsc->mrc_domain);
  mrc_fld_set_name(mrc_fld, psc_mfields_name(mflds));
  mrc_fld_set_param_int(mrc_fld, "nr_ghosts", 2);
  mrc_fld_set_param_int(mrc_fld, "nr_comps", mflds->nr_fields);
  mrc_fld_setup(mrc_fld);
  for (int m = 0; m < mflds->nr_fields; m++) {
    mrc_fld_set_comp_name(mrc_fld, m, psc_mfields_comp_name(mflds, m));
  }
  copy_to_mrc_fld(mrc_fld, mflds);
  mrc_fld_write(mrc_fld, io);
  mrc_fld_destroy(mrc_fld);
}

// ----------------------------------------------------------------------
// calc_n

static void
calc_n(struct psc *psc, struct psc_mparticles *mprts_base,
	struct psc_mfields *mflds_base)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  psc_output_fields_item_run(sub->item_n, mflds_base, mprts_base, sub->mflds_n);
#if 0
  static struct mrc_io *io;
  if (!io) {
    io = mrc_io_create(psc_comm(ppsc));
    mrc_io_set_type(io, "xdmf_collective");
    mrc_io_set_param_string(io, "basename", "flatfoil");
    mrc_io_set_from_options(io);
    mrc_io_setup(io);
  }

  mrc_io_open(io, "w", psc->timestep, psc->timestep * psc->dt);
  debug_dump(io, sub->mflds_n);
  mrc_io_close(io);
#endif
}

// ----------------------------------------------------------------------
// do_add_particles

static bool
do_add_particles(struct psc *psc)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);
  struct psc_inject *inject = sub->inject;
  
  return inject->do_inject && psc->timestep % inject->every_step == 0;
}

// ----------------------------------------------------------------------
// get_n_in_cell
//
// helper function for partition / particle setup FIXME duplicated

static inline int
get_n_in_cell(struct psc *psc, struct psc_particle_npt *npt)
{
  if (psc->prm.const_num_particles_per_cell) {
    return psc->prm.nicell;
  }
  if (npt->particles_per_cell) {
    return npt->n * npt->particles_per_cell + .5;
  }
  if (psc->prm.fractional_n_particles_per_cell) {
    int n_prts = npt->n / psc->coeff.cori;
    float rmndr = npt->n / psc->coeff.cori - n_prts;
    float ran = random() / ((float) RAND_MAX + 1);
    if (ran < rmndr) {
      n_prts++;
    }
    return n_prts;
  }
  return npt->n / psc->coeff.cori + .5;
}

// FIXME duplicated

static void
_psc_setup_particle(struct psc *psc, particle_t *prt, struct psc_particle_npt *npt,
		   int p, double xx[3])
{
  double beta = psc->coeff.beta;

  float ran1, ran2, ran3, ran4, ran5, ran6;
  do {
    ran1 = random() / ((float) RAND_MAX + 1);
    ran2 = random() / ((float) RAND_MAX + 1);
    ran3 = random() / ((float) RAND_MAX + 1);
    ran4 = random() / ((float) RAND_MAX + 1);
    ran5 = random() / ((float) RAND_MAX + 1);
    ran6 = random() / ((float) RAND_MAX + 1);
  } while (ran1 >= 1.f || ran2 >= 1.f || ran3 >= 1.f ||
	   ran4 >= 1.f || ran5 >= 1.f || ran6 >= 1.f);
	      
  double pxi = npt->p[0] +
    sqrtf(-2.f*npt->T[0]/npt->m*sqr(beta)*logf(1.0-ran1)) * cosf(2.f*M_PI*ran2);
  double pyi = npt->p[1] +
    sqrtf(-2.f*npt->T[1]/npt->m*sqr(beta)*logf(1.0-ran3)) * cosf(2.f*M_PI*ran4);
  double pzi = npt->p[2] +
    sqrtf(-2.f*npt->T[2]/npt->m*sqr(beta)*logf(1.0-ran5)) * cosf(2.f*M_PI*ran6);

  if (psc->prm.initial_momentum_gamma_correction) {
    double gam;
    if (sqr(pxi) + sqr(pyi) + sqr(pzi) < 1.) {
      gam = 1. / sqrt(1. - sqr(pxi) - sqr(pyi) - sqr(pzi));
      pxi *= gam;
      pyi *= gam;
      pzi *= gam;
    }
  }
  
  assert(npt->kind >= 0 && npt->kind < psc->nr_kinds);
  prt->kind = npt->kind;
  assert(npt->q == psc->kinds[prt->kind].q);
  assert(npt->m == psc->kinds[prt->kind].m);
  /* prt->qni = psc->kinds[prt->kind].q; */
  /* prt->mni = psc->kinds[prt->kind].m; */
  prt->xi = xx[0] - psc->patch[p].xb[0];
  prt->yi = xx[1] - psc->patch[p].xb[1];
  prt->zi = xx[2] - psc->patch[p].xb[2];
  prt->pxi = pxi * cos(psc->prm.theta_xz) + pzi * sin(psc->prm.theta_xz);
  prt->pyi = pyi;
  prt->pzi = - pxi * sin(psc->prm.theta_xz) + pzi * cos(psc->prm.theta_xz);
}	      

// ----------------------------------------------------------------------
// psc_flatfoil_particle_source
//
// FIXME mostly duplicated from psc_setup_particles

static void
psc_flatfoil_particle_source(struct psc *psc, struct psc_mparticles *mprts_base,
			     struct psc_mfields *mflds_base)
{
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  if (!do_add_particles(psc)) {
    return;
  }

  struct psc_target *target = sub->target;
  struct psc_inject *inject = sub->inject;

  calc_n(psc, mprts_base, mflds_base);
  
  struct psc_mparticles *mprts = psc_mparticles_get_as(mprts_base, PARTICLE_TYPE, 0);
  
  psc_foreach_patch(psc, p) {
    struct psc_particles *prts = psc_mparticles_get_patch(mprts, p);
    struct psc_fields *flds_n = psc_mfields_get_patch(sub->mflds_n, p);
    int *ldims = psc->patch[p].ldims;
    
    int i = prts->n_part;
    int nr_pop = psc->prm.nr_populations;
    for (int jz = 0; jz < ldims[2]; jz++) {
      for (int jy = 0; jy < ldims[1]; jy++) {
	for (int jx = 0; jx < ldims[0]; jx++) {
	  double xx[3] = { .5 * (CRDX(p, jx) + CRDX(p, jx+1)),
			   .5 * (CRDY(p, jy) + CRDY(p, jy+1)),
			   .5 * (CRDZ(p, jz) + CRDZ(p, jz+1)) };
	  // FIXME, the issue really is that (2nd order) particle pushers
	  // don't handle the invariant dim right
	  if (psc->domain.gdims[0] == 1) xx[0] = CRDX(p, jx);
	  if (psc->domain.gdims[1] == 1) xx[1] = CRDY(p, jy);
	  if (psc->domain.gdims[2] == 1) xx[2] = CRDZ(p, jz);

	  if (!psc_target_is_inside(target, xx)) {
	    continue;
	  }

	  int n_q_in_cell = 0;
	  for (int kind = 0; kind < nr_pop; kind++) {
	    struct psc_particle_npt npt = {};
	    if (kind < psc->nr_kinds) {
	      npt.kind = kind;
	      npt.q    = psc->kinds[kind].q;
	      npt.m    = psc->kinds[kind].m;
	      npt.n    = psc->kinds[kind].n;
	      npt.T[0] = psc->kinds[kind].T;
	      npt.T[1] = psc->kinds[kind].T;
	      npt.T[2] = psc->kinds[kind].T;
	    };
	    psc_ops(psc)->init_npt(psc, kind, xx, &npt);
	    
	    int n_in_cell;
	    if (kind != psc->prm.neutralizing_population) {
	      if (psc->timestep >= 0) {
		npt.n -= F3_C(flds_n, MY_ELECTRON, jx,jy,jz);
		if (npt.n < 0) {
		  n_in_cell = 0;
		} else {
		  // this rounds down rather than trying to get fractional particles
		  // statistically right...
		  n_in_cell = npt.n / psc->coeff.cori *
		    (inject->every_step * psc->dt / inject->tau) /
		    (1. + inject->every_step * psc->dt / inject->tau);
		}
	      } else {
		n_in_cell = get_n_in_cell(psc, &npt);
	      }
	      n_q_in_cell += npt.q * n_in_cell;
	    } else {
	      // FIXME, should handle the case where not the last population is neutralizing
	      assert(psc->prm.neutralizing_population == nr_pop - 1);
	      n_in_cell = -n_q_in_cell / npt.q;
	    }
	    particles_realloc(prts, i + n_in_cell);
	    for (int cnt = 0; cnt < n_in_cell; cnt++) {
	      particle_t *prt = particles_get_one(prts, i++);
	      
	      _psc_setup_particle(psc, prt, &npt, p, xx);
	      assert(psc->prm.fractional_n_particles_per_cell);
	      prt->qni_wni = psc->kinds[prt->kind].q;
	    }
	  }
	}
      }
    }
    prts->n_part = i;
  }

  psc_mparticles_put_as(mprts, mprts_base, 0);
}

// ----------------------------------------------------------------------
// psc_ops "flatfoil"

struct psc_ops psc_flatfoil_ops = {
  .name             = "flatfoil",
  .size             = sizeof(struct psc_flatfoil),
  .param_descr      = psc_flatfoil_descr,
  .create           = psc_flatfoil_create,
  .setup            = psc_flatfoil_setup,
  .destroy          = psc_flatfoil_destroy,
  .read             = psc_flatfoil_read,
  .init_field       = psc_flatfoil_init_field,
  .init_npt         = psc_flatfoil_init_npt,
};

// ======================================================================
// psc_event_generator subclass "flatfoil"

// ----------------------------------------------------------------------
// psc_event_generator_flatfoil_run

void
psc_event_generator_flatfoil_run(struct psc_event_generator *gen,
				 mparticles_base_t *mprts, mfields_base_t *mflds,
				 mphotons_t *mphotons)
{
  struct psc *psc = ppsc; // FIXME
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  psc_flatfoil_particle_source(psc, mprts, mflds);
  psc_heating_run(sub->heating, mprts, mflds);
}

// ----------------------------------------------------------------------
// psc_event_generator_ops "flatfoil"

struct psc_event_generator_ops psc_event_generator_flatfoil_ops = {
  .name                  = "flatfoil",
  .run                   = psc_event_generator_flatfoil_run,
};


// ======================================================================
// main

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_psc_event_generator,
			      &psc_event_generator_flatfoil_ops);
  return psc_main(&argc, &argv, &psc_flatfoil_ops);
}
