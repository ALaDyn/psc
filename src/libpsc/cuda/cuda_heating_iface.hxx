
#pragma once

#include "heating_spot_foil.hxx"

struct cuda_heating_foil;

void cuda_heating_setup_foil(cuda_heating_foil *foil);

template<typename BS>
void cuda_heating_run_foil(cuda_heating_foil& foil, cuda_mparticles<BS>* cmprts);

// ======================================================================
// cuda_heating_run_foil

struct cuda_heating_foil : HeatingSpotFoilParams
{
  cuda_heating_foil() = default;
  
  cuda_heating_foil(const HeatingSpotFoilParams& params, int kind, double heating_dt)
    : HeatingSpotFoilParams(params), kind(kind), heating_dt(heating_dt)
  {
    cuda_heating_setup_foil(this);
  }
  
  // params
  int kind;

  // state (FIXME, shouldn't be part of the interface)
  float fac;
  float heating_dt;
};

