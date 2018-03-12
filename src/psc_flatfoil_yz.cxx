
#include <psc.h>
#include <psc_push_fields.h>
#include <psc_bnd_fields.h>
#include <psc_heating.h>
#include <psc_heating_spot_private.h>
#include <psc_inject.h>
#include <psc_target_private.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define USE_OWN_PSC_STEP

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
  struct psc_flatfoil *sub = psc_flatfoil(psc);

  psc_default_dimensionless(psc);

  psc->prm.nmax = 210001;
  psc->prm.nicell = 100;
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

  psc_target_set_type(sub->target, "foil");

  struct psc_heating_spot *spot = psc_heating_get_spot(sub->heating);
  psc_heating_spot_set_type(spot, "foil");
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
  psc->kinds[MY_ELECTRON].name = strdup("e");

  psc->kinds[MY_ION     ].q = sub->Zi;
  psc->kinds[MY_ION     ].m = 100. * sub->Zi;  // FIXME, hardcoded mass ratio 100
  psc->kinds[MY_ION     ].name = strdup("i");

  sub->d_i = sqrt(psc->kinds[MY_ION].m / psc->kinds[MY_ION].q);

  psc_target_set_param_double(sub->target, "yl", sub->target_yl * sub->d_i);
  psc_target_set_param_double(sub->target, "yh", sub->target_yh * sub->d_i);
  psc_target_set_param_double(sub->target, "zl", - sub->target_zwidth * sub->d_i);
  psc_target_set_param_double(sub->target, "zh",   sub->target_zwidth * sub->d_i);

  psc_inject_set_param_int(sub->inject, "kind_n", MY_ELECTRON);
  psc_inject_set_param_obj(sub->inject, "target", sub->target);

  psc_heating_set_param_int(sub->heating, "kind", MY_ELECTRON);

  struct psc_heating_spot *spot = psc_heating_get_spot(sub->heating);
  psc_heating_spot_set_param_double(spot, "zl", sub->heating_zl * sub->d_i);
  psc_heating_spot_set_param_double(spot, "zh", sub->heating_zh * sub->d_i);
  psc_heating_spot_set_param_double(spot, "xc", sub->heating_xc * sub->d_i);
  psc_heating_spot_set_param_double(spot, "yc", sub->heating_yc * sub->d_i);
  psc_heating_spot_set_param_double(spot, "rH", sub->heating_rH * sub->d_i);
  psc_heating_spot_set_param_double(spot, "Mi", psc->kinds[MY_ION].m);

  psc_setup_super(psc);
  psc_setup_member_objs_sub(psc);

  MPI_Comm comm = psc_comm(psc);
  mpi_printf(comm, "d_e = %g, d_i = %g\n", 1., sub->d_i);
  mpi_printf(comm, "lambda_De (background) = %g\n", sqrt(sub->background_Te));
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

  if (sub->no_initial_target) {
    return;
  }

  if (psc_target_is_inside(target, x)) {
    // replace values above by target values
    psc_target_init_npt(target, pop, x, npt);
  }
}

static void psc_flatfoil_integrate(struct psc *psc);

// ----------------------------------------------------------------------
// psc_ops "flatfoil"

struct psc_ops_flatfoil : psc_ops {
  psc_ops_flatfoil() {
    name             = "flatfoil";
    size             = sizeof(struct psc_flatfoil);
    param_descr      = psc_flatfoil_descr;
    create           = psc_flatfoil_create;
    setup            = psc_flatfoil_setup;
    read             = psc_flatfoil_read;
    init_field       = psc_flatfoil_init_field;
    init_npt         = psc_flatfoil_init_npt;
#ifdef USE_OWN_PSC_STEP
    integrate        = psc_flatfoil_integrate;
#endif
  }
} psc_flatfoil_ops;

// ======================================================================
// psc_target subclass "foil"

struct psc_target_foil {
  // params
  double yl;
  double yh;
  double zl;
  double zh;
  double n;
  double Te;
  double Ti;
};

#define psc_target_foil(target) mrc_to_subobj(target, struct psc_target_foil)

#define VAR(x) (void *)offsetof(struct psc_target_foil, x)
static struct param psc_target_foil_descr[] _mrc_unused = {
  { "yl"           , VAR(yl)           , PARAM_DOUBLE(0.)       },
  { "yh"           , VAR(yh)           , PARAM_DOUBLE(0.)       },
  { "zl"           , VAR(zl)           , PARAM_DOUBLE(0.)       },
  { "zh"           , VAR(zh)           , PARAM_DOUBLE(0.)       },
  { "n"            , VAR(n)            , PARAM_DOUBLE(1.)       },
  { "Te"           , VAR(Te)           , PARAM_DOUBLE(.001)     },
  { "Ti"           , VAR(Ti)           , PARAM_DOUBLE(.001)     },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_target_foil_is_inside

static bool
psc_target_foil_is_inside(struct psc_target *target, double x[3])
{
  struct psc_target_foil *sub = psc_target_foil(target);
  
  return (x[1] >= sub->yl && x[1] <= sub->yh &&
	  x[2] >= sub->zl && x[2] <= sub->zh);
}

// ----------------------------------------------------------------------
// psc_target_foil_init_npt

static void
psc_target_foil_init_npt(struct psc_target *target, int pop, double x[3],
			 struct psc_particle_npt *npt)
{
  struct psc_target_foil *sub = psc_target_foil(target);

  if (!psc_target_foil_is_inside(target, x)) {
    npt->n = 0;
    return;
  }

  switch (pop) {
  case MY_ION:
    npt->n    = sub->n;
    npt->T[0] = sub->Ti;
    npt->T[1] = sub->Ti;
    npt->T[2] = sub->Ti;
    break;
  case MY_ELECTRON:
    npt->n    = sub->n;
    npt->T[0] = sub->Te;
    npt->T[1] = sub->Te;
    npt->T[2] = sub->Te;
    break;
  default:
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_target "foil"

struct psc_target_ops_foil : psc_target_ops {
  psc_target_ops_foil() {
    name                = "foil";
    size                = sizeof(struct psc_target_foil);
    param_descr         = psc_target_foil_descr;
    is_inside           = psc_target_foil_is_inside;
    init_npt            = psc_target_foil_init_npt;
  }
} psc_target_ops_foil;

// ======================================================================
// psc_heating_spot subclass "foil"

struct psc_heating_spot_foil {
  // params
  double zl; // in internal units (d_e)
  double zh;
  double xc;
  double yc;
  double rH;
  double T;
  double Mi;

  // state
  double fac;
};

#define psc_heating_spot_foil(heating) mrc_to_subobj(heating, struct psc_heating_spot_foil)

// ----------------------------------------------------------------------
// psc_heating_spot_foil_setup

static void
psc_heating_spot_foil_setup(struct psc_heating_spot *heating)
{
  struct psc_heating_spot_foil *sub = psc_heating_spot_foil(heating);
  
  double width = sub->zh - sub->zl;
  sub->fac = (8.f * pow(sub->T, 1.5)) / (sqrt(sub->Mi) * width);
  // FIXME, I don't understand the sqrt(Mi) in here

  psc_heating_spot_setup_super(heating);
}

// ----------------------------------------------------------------------
// psc_heating_spot_foil_get_H

static double
psc_heating_spot_foil_get_H(struct psc_heating_spot *heating, double *xx)
{
  struct psc_heating_spot_foil *sub = psc_heating_spot_foil(heating);
  
  double zl = sub->zl;
  double zh = sub->zh;
  double xc = sub->xc;
  double yc = sub->yc;
  double rH = sub->rH;
  double fac = sub->fac;
  double x = xx[0], y = xx[1], z = xx[2];

  if (z <= zl || z >= zh) {
    return 0;
  }

  return fac * exp(-(sqr(x-xc) + sqr(y-yc)) / sqr(rH));
}
  
#define VAR(x) (void *)offsetof(struct psc_heating_spot_foil, x)
static struct param psc_heating_spot_foil_descr[] _mrc_unused = {
  { "zl"                , VAR(zl)                , PARAM_DOUBLE(0.)       },
  { "zh"                , VAR(zh)                , PARAM_DOUBLE(0.)       },
  { "xc"                , VAR(xc)                , PARAM_DOUBLE(0.)       },
  { "yc"                , VAR(yc)                , PARAM_DOUBLE(0.)       },
  { "rH"                , VAR(rH)                , PARAM_DOUBLE(0.)       },
  { "T"                 , VAR(T)                 , PARAM_DOUBLE(.04)      },
  { "Mi"                , VAR(Mi)                , PARAM_DOUBLE(1.)       },
  {},
};
#undef VAR

// ----------------------------------------------------------------------
// psc_heating_spot "foil"

struct psc_heating_spot_ops_foil : psc_heating_spot_ops {
  psc_heating_spot_ops_foil() {
    name                = "foil";
    size                = sizeof(struct psc_heating_spot_foil);
    param_descr         = psc_heating_spot_foil_descr;
    setup               = psc_heating_spot_foil_setup;
    get_H               = psc_heating_spot_foil_get_H;
  }
} psc_heating_spot_ops_foil;

// ======================================================================
// main

int
main(int argc, char **argv)
{
  mrc_class_register_subclass(&mrc_class_psc_target,
			      &psc_target_ops_foil);
  mrc_class_register_subclass(&mrc_class_psc_heating_spot,
			      &psc_heating_spot_ops_foil);
  return psc_main(&argc, &argv, &psc_flatfoil_ops);
}

// ======================================================================

#include <psc_balance.h>
#include <psc_sort.h>
#include <psc_collision.h>
#include <psc_checks.h>
#include <psc_bnd_particles.h>
#include <psc_marder.h>
#include <psc_method.h>

#include <balance.hxx>
#include <particles.hxx>
#include <fields3d.hxx>
#include <push_particles.hxx>
#include <push_fields.hxx>
#include <sort.hxx>
#include <collision.hxx>
#include <bnd_particles.hxx>
#include <bnd.hxx>
#include <bnd_fields.hxx>
#include <inject.hxx>
#include <heating.hxx>

#include "psc_particles_double.h"
#include "psc_fields_c.h"
#include "../libpsc/psc_sort/psc_sort_impl.hxx"
#include "../libpsc/psc_collision/psc_collision_impl.hxx"
#include "../libpsc/psc_push_particles/push_config.hxx"
#include "../libpsc/psc_push_particles/push_part_common.c"
#include "psc_push_fields_impl.hxx"
#include "bnd_particles_impl.hxx"
#include "../libpsc/psc_bnd/psc_bnd_impl.hxx"
#include "../libpsc/psc_bnd_fields/psc_bnd_fields_impl.hxx"
#include "../libpsc/psc_inject/psc_inject_impl.hxx"
#include "../libpsc/psc_heating/psc_heating_impl.hxx"

// ======================================================================
// PscFlatfoil
//
// eventually, a Psc replacement / derived class, but for now just
// pretending to be something like that
//
// things are missing from the generic step():
// - timing
// - psc_checks
// - pushp prep
// - marder

struct PscFlatfoil : Params
{
  using Mparticles_t = MparticlesDouble;
  using Mfields_t = MfieldsC;
  using Sort_t = SortCountsort2<Mparticles_t>;
  using Collision_t = Collision_<Mparticles_t, Mfields_t>;
  using PushParticles_t = PushParticles__<Config2nd<dim_yz>>;
  using PushFields_t = PushFields<Mfields_t>;
  using BndParticles_t = psc_bnd_particles_sub<Mparticles_t>;
  using Bnd_t = Bnd_<Mfields_t>;
  using BndFields_t = BndFieldsNone<Mfields_t>;
  using Inject_t = Inject_<Mparticles_t, Mfields_t>;
  using Heating_t = Heating_<Mparticles_t>;

  PscFlatfoil(psc *psc)
    : Params(psc->params),
      psc_{psc},
      sub_{psc_flatfoil(psc)},
      mprts_{dynamic_cast<Mparticles_t&>(*PscMparticlesBase{psc->particles}.sub())},
      mflds_{dynamic_cast<Mfields_t&>(*PscMfieldsBase{psc->flds}.sub())},
      sort_{dynamic_cast<Sort_t&>(*PscSortBase{psc->sort}.sub())},
      collision_{dynamic_cast<Collision_t&>(*PscCollisionBase{psc->collision}.sub())},
      pushp_{*new PushParticles_t{}}, // FIXME
      pushf_{dynamic_cast<PushFields_t&>(*PscPushFieldsBase{psc->push_fields}.sub())},
      bndp_{dynamic_cast<BndParticles_t&>(*PscBndParticlesBase{psc->bnd_particles}.sub())},
      bnd_{dynamic_cast<Bnd_t&>(*PscBndBase{psc->bnd}.sub())},
      bndf_{dynamic_cast<BndFields_t&>(*PscBndFieldsBase{psc->push_fields->bnd_fields}.sub())}, // !!!
      inject_{dynamic_cast<Inject_t&>(*PscInjectBase{sub_->inject}.sub())},
      heating_{dynamic_cast<Heating_t&>(*PscHeatingBase{sub_->heating}.sub())}
  {}
  
  void step()
  {
    // state is at: x^{n+1/2}, p^{n}, E^{n+1/2}, B^{n+1/2}
    
    int timestep = psc_->timestep;
    
    if (sort_interval > 0 && timestep % sort_interval == 0) {
      sort_(mprts_);
    }
    
    collision_(mprts_);
    
    // === particle propagation p^{n} -> p^{n+1}, x^{n+1/2} -> x^{n+3/2}
    pushp_.push_mprts(mprts_, mflds_);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1/2}, j^{n+1}
    
    // === field propagation B^{n+1/2} -> B^{n+1}
    pushf_.push_H<dim_yz>(mflds_, .5);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+1/2}, B^{n+1}, j^{n+1}
    
    bndp_(mprts_);
    
    inject_(mprts_);
    heating_(mprts_);
    
    // === field propagation E^{n+1/2} -> E^{n+3/2}
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    
    bndf_.add_ghosts_J(mflds_);
    bnd_.add_ghosts(mflds_, JXI, JXI + 3);
    bnd_.fill_ghosts(mflds_, JXI, JXI + 3);
    
    pushf_.push_E<dim_yz>(mflds_, 1.);
    
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+1}
    
    // === field propagation B^{n+1} -> B^{n+3/2}
    bndf_.fill_ghosts_E(mflds_);
    bnd_.fill_ghosts(mflds_, EX, EX + 3);
    
    pushf_.push_H<dim_yz>(mflds_, .5);
    
    bndf_.fill_ghosts_H(mflds_);
    bnd_.fill_ghosts(mflds_, HX, HX + 3);
    // state is now: x^{n+3/2}, p^{n+1}, E^{n+3/2}, B^{n+3/2}


    //psc_checks_continuity_after_particle_push(psc->checks, psc);

    // E at t^{n+3/2}, particles at t^{n+3/2}
    // B at t^{n+3/2} (Note: that is not it's natural time,
    // but div B should be == 0 at any time...)
    //psc_marder_run(psc->marder, psc->flds, psc->particles);
    
    //psc_checks_gauss(psc->checks, psc);

    //psc_push_particles_prep(psc->push_particles, psc->particles, psc->flds);
  }

private:
  psc* psc_;
  psc_flatfoil* sub_;
  Mparticles_t& mprts_;
  Mfields_t& mflds_;
  Sort_t& sort_;
  Collision_t& collision_;
  PushParticles_t& pushp_;
  PushFields_t& pushf_;
  BndParticles_t& bndp_;
  Bnd_t& bnd_;
  BndFields_t& bndf_;
  Inject_t& inject_;
  Heating_t& heating_;
};

// ----------------------------------------------------------------------
// psc_flatfoil_integrate

static void psc_flatfoil_integrate(struct psc *psc)
{
  psc_method_initialize(psc->method, psc);
  mpi_printf(psc_comm(psc), "Initialization complete.\n");
  
  static int pr;
  if (!pr) {
    pr = prof_register("psc_step", 1., 0, 0);
  }

  int st_nr_particles = psc_stats_register("nr particles");
  int st_time_step = psc_stats_register("time entire step");

  // generic stats categories
  st_time_particle = psc_stats_register("time particle update");
  st_time_field = psc_stats_register("time field update");
  st_time_comm = psc_stats_register("time communication");
  st_time_output = psc_stats_register("time output");

  mpi_printf(psc_comm(psc), "*** Advancing\n");
  double elapsed = MPI_Wtime();

  PscFlatfoil flatfoil(psc);

  bool first_iteration = true;
  while (psc->timestep < psc->prm.nmax) {
    prof_start(pr);
    psc_stats_start(st_time_step);

    if (!first_iteration &&
	psc->prm.write_checkpoint_every_step > 0 &&
	psc->timestep % psc->prm.write_checkpoint_every_step == 0) {
      psc_write_checkpoint(psc);
    }
    first_iteration = false;

    mpi_printf(psc_comm(psc), "**** Step %d / %d, Time %g\n", psc->timestep + 1,
	       psc->prm.nmax, psc->timestep * psc->dt);

    auto balance = PscBalanceBase{psc->balance};
    balance(psc);
    
    prof_start(pr_time_step_no_comm);
    prof_stop(pr_time_step_no_comm); // actual measurements are done w/ restart

    flatfoil.step();

    psc->timestep++; // FIXME, too hacky
    psc_output(psc);
    
    psc_stats_stop(st_time_step);
    prof_stop(pr);

    PscMparticlesBase mprts(psc->particles);
    psc_stats_val[st_nr_particles] = mprts->get_n_prts();

    if (psc->timestep % psc->prm.stats_every == 0) {
      psc_stats_log(psc);
      psc_print_profiling(psc);
    }

    if (psc->prm.wallclock_limit > 0.) {
      double wallclock_elapsed = MPI_Wtime() - psc->time_start;
      double wallclock_elapsed_max;
      MPI_Allreduce(&wallclock_elapsed, &wallclock_elapsed_max, 1, MPI_DOUBLE, MPI_MAX,
		    MPI_COMM_WORLD);
      
      if (wallclock_elapsed_max > psc->prm.wallclock_limit) {
	mpi_printf(MPI_COMM_WORLD, "WARNING: Max wallclock time elapsed!\n");
	break;
      }
    }
  }

  if (psc->prm.write_checkpoint) {
    psc_write_checkpoint(psc);
  }

  // FIXME, merge with existing handling of wallclock time
  elapsed = MPI_Wtime() - elapsed;

  int  s = (int)elapsed, m  = s/60, h  = m/60, d  = h/24, w = d/ 7;
  /**/ s -= m*60,        m -= h*60, h -= d*24, d -= w*7;
  mpi_printf(psc_comm(psc), "*** Finished (%gs / %iw:%id:%ih:%im:%is elapsed)\n",
	     elapsed, w, d, h, m, s );
  
}

