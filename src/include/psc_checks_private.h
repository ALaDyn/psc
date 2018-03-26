
#ifndef PSC_CHECKS_PRIVATE_H
#define PSC_CHECKS_PRIVATE_H

#include <psc_checks.h>

struct ChecksParams
{
  int continuity_every_step;   // check charge continuity eqn every so many steps
  double continuity_threshold; // acceptable error in continuity eqn
  bool continuity_verbose;     // always print continuity error, even if acceptable
  bool continuity_dump_always; // always dump d_rho, div_j, even if acceptable

  int gauss_every_step;   // check Gauss's Law every so many steps
  double gauss_threshold; // acceptable error in Gauss's Law
  bool gauss_verbose;     // always print Gauss's Law error, even if acceptable
  bool gauss_dump_always; // always dump E, div_rho, even if acceptable
};

struct psc_checks
{
  struct mrc_obj obj;

  ChecksParams params;
};

struct psc_checks_ops {
  MRC_SUBCLASS_OPS(struct psc_checks);
  void (*continuity_before_particle_push)(struct psc_checks *checks, struct psc *psc);
  void (*continuity_after_particle_push)(struct psc_checks *checks, struct psc *psc);
  void (*gauss)(struct psc_checks *checks, struct psc *psc);
};

#define psc_checks_ops(checks) ((struct psc_checks_ops *)((checks)->obj.ops))

#endif
