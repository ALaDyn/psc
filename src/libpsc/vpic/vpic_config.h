
#ifndef VPIC_CONFIG_H
#define VPIC_CONFIG_H

//#define DO_VPIC 1

#define HAVE_VPIC

#ifdef HAVE_VPIC
#include "util/profile/profile.h"
#else
//#include "util/profile/profile.h"
//#undef TIC
//#undef TOC
#define TIC
#define TOC(a, b)
#endif

#include "simulation.h"

#include "PscRng.h"

#include "PscGridBase.h"
#include "PscMaterial.h"

#include "PscFieldArrayBase.h"
#include "PscFieldArrayLocalOps.h"
#include "PscFieldArrayRemoteOps.h"
#include "PscFieldArray.h"
#include "PscPushFieldsOps.hxx"

#include "PscParticleBc.h"

#include "PscParticlesBase.h"
#include "PscParticlesOps.h"

#include "PscInterpolatorBase.h"
#include "PscInterpolator.h"

#include "PscAccumulator.h"
#include "mfields_accumulator_vpic.hxx"

#include "PscHydroArray.h"
#include "mfields_hydro.hxx"

#include "NoneDiag.h"

#include "PscSimulationBase.h"

#ifdef HAVE_VPIC

#include "VpicRng.h"

#include "VpicGridBase.h"
#include "VpicMaterial.h"

#include "VpicFieldArrayLocalOps.h"
#include "VpicFieldArrayRemoteOps.h"
#include "VpicFieldArray.h"
#include "VpicPushFieldsOps.hxx"

#include "VpicParticleBc.h"

#include "VpicParticlesBase.h"
#include "VpicParticlesOps.h"

#include "VpicInterpolatorBase.h"
#include "VpicInterpolator.h"

#include "VpicAccumulatorBase.h"
#include "VpicAccumulator.h"

#include "VpicHydroArray.h"

#include "VpicDiag.h"

#include "VpicSimulationBase.h"

#include "mfields_hydro_vpic.hxx"

#undef particle_t

#endif

#ifdef DO_VPIC
using Grid = VpicGridBase;
#else
using Grid = PscGridBase;
#endif

#if 1
using MaterialList = PscMaterialList;
using FieldArray = PscFieldArrayBase<Grid, MaterialList>;
using FieldArrayLocalOps = PscFieldArrayLocalOps<FieldArray>;
using FieldArrayRemoteOps = PscFieldArrayRemoteOps<FieldArray>;
using PushFieldsOps = PscPushFieldsOps<FieldArray, FieldArrayLocalOps, FieldArrayRemoteOps>;
using DiagOps = PscDiagOps<FieldArray>;
using AccumulateOps = PscAccumulateOps<FieldArray, FieldArrayLocalOps, FieldArrayRemoteOps>;
using CleanDivOps = PscCleanDivOps<FieldArray, FieldArrayLocalOps, FieldArrayRemoteOps>;
#else
using FieldArray = VpicFieldArrayBase<Grid, VpicMaterialList>;
using PushFieldsOps = VpicPushFieldsOps<FieldArray>;
using DiagOps = VpicDiagOps<FieldArray>;
using AccumulateOps = VpicAccumulateOps<FieldArray>;
using CleanDivOps = VpicCleanDivOps<FieldArray>;
#endif

using Interpolator = PscInterpolatorBase<Grid>;
using InterpolatorOps = PscInterpolatorOps<Interpolator, FieldArray>;

using Accumulator = PscAccumulatorBase<Grid>;
using MfieldsAccumulator = PscAccumulatorBase<Grid>;
using AccumulatorOps = PscAccumulatorOps<MfieldsAccumulator, FieldArray>;

#ifdef DO_VPIC
using MfieldsHydro = MfieldsHydroVpic;
using HydroArrayOps = VpicHydroArrayOps<MfieldsHydro>;
#else
using MfieldsHydro = MfieldsHydroPsc<Grid>;
using HydroArrayOps = PscHydroArrayOps<MfieldsHydro>;
#endif

#if 1
using ParticleBcList = PscParticleBcList;
#else
using ParticleBcList = VpicParticleBcList;
#endif

using Particles = PscParticlesBase<Grid, ParticleBcList>;
#if 1
using ParticlesOps = PscParticlesOps<Particles, FieldArray, Interpolator, MfieldsAccumulator, MfieldsHydro>;
#else
using ParticlesOps = VpicParticlesOps<Particles. FieldArray, Interpolator, MfieldsAccumulator, MfieldsHydro>;
#endif

#if 1
typedef VpicDiagMixin<Particles, FieldArray, Interpolator, MfieldsHydro,
		      DiagOps, ParticlesOps, HydroArrayOps> DiagMixin;
#else
typedef NoneDiagMixin<Particles> DiagMixin;
#endif

#if 1
typedef PscSimulationMixin<Particles, MaterialList> SimulationMixin;
#else
typedef VpicSimulationMixin<Particles> SimulationMixin;
#endif

#if 1
typedef PscRng Rng;
typedef PscRngPool<Rng> RngPool;
#else
typedef VpicRng Rng;
typedef VpicRngPool<Rng> RngPool;
#endif

typedef VpicSimulation<Particles, FieldArray, Interpolator, MfieldsHydro, RngPool, SimulationMixin, DiagMixin> Simulation;

#endif

