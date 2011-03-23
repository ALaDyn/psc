
#ifndef PSC_PULSE_H
#define PSC_PULSE_H

#include "psc.h"
#include <mrc_params.h>

#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------------
// psc_pulse

MRC_CLASS_DECLARE(psc_pulse, struct psc_pulse);

struct psc_pulse_ops {
  const char *name;
  size_t ctx_size;
  struct param *ctx_descr;
  void (*destroy)(struct psc_pulse *);
  void (*setup)(struct psc_pulse *);
  double (*field_s)(struct psc_pulse *,
		    double x, double y, double z, double t);
  double (*field_p)(struct psc_pulse *,
		    double x, double y, double z, double t);
};  

struct psc_pulse {
  struct mrc_obj obj;
  bool is_setup;
  void *ctx;
  struct psc_pulse_ops *ops;
};

void psc_pulse_ini(struct psc_pulse *pulse, struct psc_pulse_ops *ops, void *prm);

static inline double
psc_pulse_field_s(struct psc_pulse *pulse, double x, double y, double z, double t)
{
  if (!pulse->is_setup) {
    psc_pulse_setup(pulse);
  }
  return pulse->ops->field_s(pulse, x, y, z, t);
}

static inline double
psc_pulse_field_p(struct psc_pulse *pulse, double x, double y, double z, double t)
{
  if (!pulse->is_setup) {
    psc_pulse_setup(pulse);
  }
  return pulse->ops->field_p(pulse, x, y, z, t);
}

// ----------------------------------------------------------------------
// psc_pulse_gauss

struct psc_pulse_gauss {
  double xm, ym, zm; // location of pulse center at time 0 in m 
  double dxm, dym, dzm; // width of pulse in m
  double amplitude_p;   // max amplitude, p-polarization
  double amplitude_s;   // max amplitude, s-polarization
  double phase_p;       // CEP-phase  (from -pi to pi)
  double phase_s;       // CEP-phase  (from -pi to pi)
  double k[3];
};

struct psc_pulse *psc_pulse_gauss_create(struct psc_pulse_gauss *ctx);

// ----------------------------------------------------------------------
// psc_pulse_flattop

struct psc_pulse_flattop {
  double xm, ym, zm; // location of pulse center at time 0 in m 
  double dxm, dym, dzm; // slope of pulse in m
  double zb; // width of pulse in m
  double amplitude_p;   // max amplitude, p-polarization
  double amplitude_s;   // max amplitude, s-polarization
  double phase_p;       // CEP-phase  (from -pi to pi)
  double phase_s;       // CEP-phase  (from -pi to pi)
};

struct psc_pulse *psc_pulse_flattop_create(struct psc_pulse_flattop *prm);

#endif
