
#pragma once

struct SortVpic
{
  using Mparticles = MparticlesVpic;

  void operator()(Mparticles& mprts)
  {
    auto step = ppsc->timestep;
    // Sort the particles for performance if desired.
    
    for (auto& sp : mprts) {
      if (sp.sort_interval > 0 && (step % sp.sort_interval) == 0) {
	mpi_printf(MPI_COMM_WORLD, "Performance sorting \"%s\"\n", sp.name);
	TIC ParticlesOps::sort_p(&sp); TOC(sort_p, 1);
      }
    }
  }
};

