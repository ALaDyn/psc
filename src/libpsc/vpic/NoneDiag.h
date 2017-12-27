
#ifndef NONE_DIAG_H
#define NONE_DIAG_H

#include "vpic.h" // FIXME

// ----------------------------------------------------------------------
// NoneDiagMixin

template<class Particles>
struct NoneDiagMixin {
  typedef typename Particles::FieldArray FieldArray;
  typedef typename Particles::Interpolator Interpolator;
  typedef typename Particles::HydroArray HydroArray;

  NoneDiagMixin(vpic_simulation* simulaton) { }

  void diagnostics_init(int interval_) { }
  void diagnostics_setup() { }
  void diagnostics_run(FieldArray& fa, Particles& particles,
		       Interpolator& interpolator, HydroArray& hydro_array) { }
};

#endif
