
#pragma once

#include "psc_particles_single.h"
#include "psc_particles_double.h"
#include "psc_fields_c.h"

#include "../libpsc/psc_sort/psc_sort_impl.hxx"
#include "../libpsc/psc_collision/psc_collision_impl.hxx"
#include "../libpsc/psc_push_particles/push_part_common.c"
#include "../libpsc/psc_push_particles/1vb/psc_push_particles_1vb.h"
#include "../libpsc/psc_push_particles/1vb.c"
#include "psc_push_fields_impl.hxx"
#include "../libpsc/psc_bnd/psc_bnd_impl.hxx"
#include "../libpsc/psc_bnd_fields/psc_bnd_fields_impl.hxx"
#include "bnd_particles_impl.hxx"
#include "../libpsc/psc_balance/psc_balance_impl.hxx"
#include "../libpsc/psc_push_fields/marder_impl.hxx"

#ifdef USE_CUDA
#include "../libpsc/cuda/push_particles_cuda_impl.hxx"
#include "../libpsc/cuda/push_fields_cuda_impl.hxx"
#include "../libpsc/cuda/bnd_cuda_impl.hxx"
#include "../libpsc/cuda/bnd_cuda_2_impl.hxx"
#include "../libpsc/cuda/bnd_cuda_3_impl.hxx"
#include "../libpsc/cuda/bnd_particles_cuda_impl.hxx"
#include "../libpsc/cuda/sort_cuda_impl.hxx"
#include "../libpsc/cuda/collision_cuda_impl.hxx"
#include "../libpsc/cuda/checks_cuda_impl.hxx"
#include "../libpsc/cuda/marder_cuda_impl.hxx"
#endif

struct SimulationNone
{
  using Species = void;
};

template<typename DIM, typename Mparticles, typename Mfields>
struct PscConfigPushParticles2nd
{
  using PushParticles_t = PushParticles__<Config2nd<Mparticles, Mfields, DIM>>;
};

template<typename DIM, typename Mparticles, typename Mfields>
struct PscConfigPushParticles1vbec
{
  using PushParticles_t = PushParticles1vb<Config1vbec<Mparticles, Mfields, DIM>>;
};

template<typename DIM, typename Mparticles, typename Mfields>
struct PscConfigPushParticlesCuda
{
};

// need to use Config1vbecSplit when for dim_xyz, dim_xz

template<typename Mparticles, typename Mfields>
struct PscConfigPushParticles1vbec<dim_xyz, Mparticles, Mfields>
{
  using PushParticles_t = PushParticles1vb<Config1vbecSplit<Mparticles, Mfields, dim_xyz>>;
};

template<typename Mparticles, typename Mfields>
struct PscConfigPushParticles1vbec<dim_xz, Mparticles, Mfields>
{
  using PushParticles_t = PushParticles1vb<Config1vbecSplit<Mparticles, Mfields, dim_xz>>;
};

template<typename DIM, typename Mparticles, typename Mfields, template<typename...> class ConfigPushParticles,
	 typename _Simulation = SimulationNone>
struct PscConfig_
{
  using dim_t = DIM;
  using Mparticles_t = Mparticles;
  using Mfields_t = Mfields;
  using MfieldsState = Mfields;
  using ConfigPushp = ConfigPushParticles<DIM, Mparticles, Mfields>;
  using PushParticles_t = typename ConfigPushp::PushParticles_t;
  using checks_order = typename PushParticles_t::checks_order;
  using Sort_t = SortCountsort2<Mparticles_t>;
  using Collision_t = Collision_<Mparticles_t, Mfields_t>;
  using PushFields_t = PushFields<Mfields_t>;
  using BndParticles_t = BndParticles_<Mparticles_t>;
  using Bnd_t = Bnd_<Mfields_t>;
  using BndFields_t = BndFieldsNone<Mfields_t>;
  using Balance_t = Balance_<Mparticles_t, Mfields_t>;
  using Checks_t = Checks_<Mparticles_t, Mfields_t, checks_order>;
  using Marder_t = Marder_<Mparticles_t, Mfields_t>;
  using Simulation = _Simulation;
};

#ifdef USE_CUDA

template<typename DIM, typename Mparticles, typename Mfields>
struct PscConfig_<DIM, Mparticles, Mfields, PscConfigPushParticlesCuda>
{
  using dim_t = DIM;
  using BS = typename Mparticles::BS;
  using Mparticles_t = Mparticles;
  using Mfields_t = Mfields;
  using PushParticles_t = PushParticlesCuda<CudaConfig1vbec3d<dim_t, BS>>;
  using Sort_t = SortCuda<BS>;
  using Collision_t = CollisionCuda<Mparticles>;
  using PushFields_t = PushFieldsCuda;
  using BndParticles_t = BndParticlesCuda<Mparticles, dim_t>;
  using Bnd_t = BndCuda3<Mfields_t>;
  using BndFields_t = BndFieldsNone<Mfields_t>;
  using Balance_t = Balance_<MparticlesSingle, MfieldsSingle>;
  using Checks_t = ChecksCuda<Mparticles>;
  using Marder_t = MarderCuda<BS>;
};

template<typename Mparticles, typename Mfields>
struct PscConfig_<dim_xyz, Mparticles, Mfields, PscConfigPushParticlesCuda>
{
  using dim_t = dim_xyz;
  using BS = typename Mparticles::BS;
  using Mparticles_t = Mparticles;
  using Mfields_t = Mfields;
  using PushParticles_t = PushParticlesCuda<CudaConfig1vbec3dGmem<dim_t, BS>>;
  using Sort_t = SortCuda<BS>;
  using Collision_t = CollisionCuda<Mparticles>;
  using PushFields_t = PushFieldsCuda;
  using BndParticles_t = BndParticlesCuda<Mparticles, dim_t>;
  using Bnd_t = BndCuda3<Mfields>;
  using BndFields_t = BndFieldsNone<Mfields_t>;
  using Balance_t = Balance_<MparticlesSingle, MfieldsSingle>;
  using Checks_t = ChecksCuda<Mparticles>;
  using Marder_t = MarderCuda<BS>;
};

#endif


template<typename dim>
using PscConfig2ndDouble = PscConfig_<dim, MparticlesDouble, MfieldsC, PscConfigPushParticles2nd>;

template<typename dim>
using PscConfig2ndSingle = PscConfig_<dim, MparticlesSingle, MfieldsSingle, PscConfigPushParticles2nd>;

template<typename dim>
using PscConfig1vbecSingle = PscConfig_<dim, MparticlesSingle, MfieldsSingle, PscConfigPushParticles1vbec>;

#ifdef USE_CUDA

template<typename dim>
struct PscConfig1vbecCuda : PscConfig_<dim, MparticlesCuda<BS144>, MfieldsCuda, PscConfigPushParticlesCuda>
{};

template<>
struct PscConfig1vbecCuda<dim_xyz> : PscConfig_<dim_xyz, MparticlesCuda<BS444>, MfieldsCuda, PscConfigPushParticlesCuda>
{};

#endif

#ifdef USE_VPIC

#include <psc_particles_vpic.h>
#include <psc_fields_vpic.h>
#include "../libpsc/vpic/sort_vpic.hxx"
#include "../libpsc/vpic/collision_vpic.hxx"
#include "../libpsc/vpic/push_particles_vpic.hxx"
#include "../libpsc/vpic/push_fields_vpic.hxx"
#include "../libpsc/vpic/bnd_vpic.hxx"
#include "../libpsc/vpic/bnd_fields_vpic.hxx"
#include "../libpsc/vpic/bnd_particles_vpic.hxx"
#include "../libpsc/vpic/marder_vpic.hxx"

struct PscConfigVpic
{
  using Mparticles_t = MparticlesVpic;
  using Mfields_t = MfieldsVpic;
  using MfieldsState = MfieldsStateVpic;
  using Balance_t = Balance_<MparticlesSingle, MfieldsSingle>;
  using Sort_t = SortVpic;
  using Collision_t = PscCollisionVpic;
  using PushParticles_t = PushParticlesVpic;
  using PushFields_t = PushFieldsVpic;
  using Bnd_t = BndVpic;
  using BndFields_t = BndFieldsVpic;
  using BndParticles_t = BndParticlesVpic;
  using Checks_t = Checks_<MparticlesSingle, MfieldsSingle, checks_order_1st>;
  using Marder_t = MarderVpic;
  using Simulation = Simulation;
};

#endif
