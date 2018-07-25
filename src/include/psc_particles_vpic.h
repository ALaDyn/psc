
#ifndef PSC_PARTICLES_VPIC_H
#define PSC_PARTICLES_VPIC_H

#include "psc_particles_private.h"
#include "psc_particles_single.h"

#include "particles.hxx"
#include "particles_traits.hxx"

#include "../libpsc/vpic/vpic_iface.h" // FIXME path

#include "psc_method.h"

void vpic_mparticles_get_size_all(Particles *vmprts, int n_patches,
				  uint *n_prts_by_patch);

struct particle_vpic_t
{
  using real_t = float;
};

struct MparticlesVpic : MparticlesBase
{
  using Self = MparticlesVpic;
  using particle_t = particle_vpic_t; // FIXME, don't have it, but needed here...

  Particles *vmprts;
  Simulation *sim;

  MparticlesVpic(const Grid_t& grid)
    : MparticlesBase(grid)
  {
    psc_method_get_param_ptr(ppsc->method, "sim", (void **) &sim);
    vmprts = Simulation_get_particles(sim);
  }

  static mrc_obj_method methods[];

  int get_n_prts() const override
  {
    return Simulation_mprts_get_nr_particles(sim, vmprts);
  }
  
  void get_size_all(uint *n_prts_by_patch) const override
  {
    vpic_mparticles_get_size_all(vmprts, 1, n_prts_by_patch);
  }

  void reserve_all(const uint *n_prts_by_patch) override
  {
    Simulation_mprts_reserve_all(sim, vmprts, 1, n_prts_by_patch);
  }

  void resize_all(const uint *n_prts_by_patch) override
  {
    Simulation_mprts_resize_all(sim, vmprts, 1, n_prts_by_patch);
  }

  void inject_reweight(int p, const psc_particle_inject& prt) override
  {
    Simulation_inject_particle(sim, vmprts, p, &prt);
  }

  static void push_back(Particles* vmprts, const struct vpic_mparticles_prt *prt);

  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }
};

template<>
struct Mparticles_traits<MparticlesVpic>
{
  static constexpr const char* name = "vpic";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};

#endif
