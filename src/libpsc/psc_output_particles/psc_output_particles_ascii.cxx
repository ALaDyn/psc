
#include "psc_particles_double.h"
#include "output_particles.hxx"

#include <mrc_params.h>
#include <string.h>

#include <string.h>

#define to_psc_output_particles_ascii(out) \
  mrc_to_subobj(out, struct psc_output_particles_ascii)

struct psc_output_particles_ascii : OutputParticlesParams, OutputParticlesBase
{
  psc_output_particles_ascii(const OutputParticlesParams& params)
    : OutputParticlesParams(params),
      comm_{psc_comm(ppsc)}
  {}

  // ----------------------------------------------------------------------
  // run

  void run(MparticlesBase& mprts_base) override
  {
    if (every_step < 0 ||
	ppsc->timestep % every_step != 0) {
      return;
    }
    
    int rank;
    MPI_Comm_rank(comm_, &rank);
    char filename[strlen(data_dir) + strlen(basename) + 19];
    sprintf(filename, "%s/%s.%06d_p%06d.asc", data_dir,
	    basename, ppsc->timestep, rank);
    
    auto& mprts = mprts_base.get_as<MparticlesDouble>();
    auto& grid = mprts.grid();
    
    FILE *file = fopen(filename, "w");
    for (int p = 0; p < mprts.n_patches(); p++) {
      int n = 0;
      for (auto& prt : mprts[p]) {
	fprintf(file, "%d %g %g %g %g %g %g %g %d\n",
		n, prt.x[0], prt.x[1], prt.x[2],
		prt.pxi, prt.pyi, prt.pzi,
		prt.qni_wni(grid), prt.kind());
	n++;
      }
    }
    
    mprts_base.put_as(mprts, MP_DONT_COPY);
    
    fclose(file);
  }

private:
  MPI_Comm comm_;
};

// ======================================================================
// psc_output_particles: subclass "ascii"

struct psc_output_particles_ops_ascii : psc_output_particles_ops {
  using Wrapper_t = OutputParticlesWrapper<psc_output_particles_ascii>;
  psc_output_particles_ops_ascii() {
    name                  = "ascii";
    size                  = Wrapper_t::size;
    setup                 = Wrapper_t::setup;
    destroy               = Wrapper_t::destroy;
  }
} psc_output_particles_ascii_ops;
