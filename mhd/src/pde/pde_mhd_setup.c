
// ======================================================================
// options

struct mhd_options {
  int eqn;
  int limiter;
  int riemann;
  int time_integrator;
};

// ----------------------------------------------------------------------
// eqn

static struct mrc_param_select opt_eqn_descr[] _mrc_unused = {
  { .val = OPT_EQN_MHD_FCONS    , .str = "mhd_fcons"   },
  { .val = OPT_EQN_MHD_SCONS    , .str = "mhd_scons"   },
  { .val = OPT_EQN_HD           , .str = "hd"          },
  {},
};

#ifdef OPT_EQN
static const int s_opt_eqn = OPT_EQN;
#else
static int s_opt_eqn _mrc_unused;
#endif

// ----------------------------------------------------------------------
// limiter

static struct mrc_param_select opt_limiter_descr[] _mrc_unused = {
  { .val = OPT_LIMITER_FLAT     , .str = "flat"        },
  { .val = OPT_LIMITER_GMINMOD  , .str = "gminmod"     },
  {},
};

#ifdef OPT_LIMITER
static const int s_opt_limiter = OPT_LIMITER;
#else
static int s_opt_limiter _mrc_unused;
#endif

// ----------------------------------------------------------------------
// riemann

static struct mrc_param_select opt_riemann_descr[] _mrc_unused = {
  { .val = OPT_RIEMANN_RUSANOV  , .str = "rusanov"     },
  { .val = OPT_RIEMANN_HLL      , .str = "hll"         },
  { .val = OPT_RIEMANN_HLLC     , .str = "hllc"        },
  { .val = OPT_RIEMANN_HLLD     , .str = "hlld"        },
  {},
};

#ifdef OPT_RIEMANN
static const int s_opt_riemann = OPT_RIEMANN;
#else
static int s_opt_riemann _mrc_unused;
#endif

// ----------------------------------------------------------------------
// time_integrator

static struct mrc_param_select opt_time_integrator_descr[] _mrc_unused = {
  { .val = OPT_TIME_INTEGRATOR_EULER   , .str = "euler"     },
  { .val = OPT_TIME_INTEGRATOR_PREDCORR, .str = "predcorr"  },
  { .val = OPT_TIME_INTEGRATOR_TVD_RK2 , .str = "tvd_rk2"   },
  {},
};

#ifdef OPT_TIME_INTEGRATOR
static const int s_opt_time_integrator = OPT_TIME_INTEGRATOR;
#else
static int s_opt_time_integrator _mrc_unused;
#endif

// ----------------------------------------------------------------------
// pde_mhd_set_options

static void _mrc_unused
pde_mhd_set_options(struct ggcm_mhd *mhd, struct mhd_options *opt)
{
  // limiter
#ifdef OPT_LIMITER
  assert(OPT_LIMITER == opt->limiter);
#else
  s_opt_limiter = opt->limiter;
#endif

  // riemann
#ifdef OPT_RIEMANN
  assert(OPT_RIEMANN == opt->riemann);
#else
  s_opt_riemann = opt->riemann;
#endif

  // time_integrator
#ifdef OPT_TIME_INTEGRATOR
  assert(OPT_TIME_INTEGRATOR == opt->time_integrator);
#else
  s_opt_time_integrator = opt->time_integrator;
#endif
}

// ======================================================================
// MHD parameters, we keep these around statically

static double s_gamma;  // adiabatic exponent

// ----------------------------------------------------------------------
// pde_mhd_setup

static void
pde_mhd_setup(struct ggcm_mhd *mhd)
{
  s_gamma = mhd->par.gamm;
}

