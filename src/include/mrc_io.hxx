
#pragma once

#include <mrc_io.h>

// ======================================================================
// MrcIo
//
// C++ wrapper around mrc_io

struct MrcIo
{
  MrcIo(const char* pfx, const char* outdir = ".")
  {
    io_ = mrc_io_create(MPI_COMM_WORLD);
    mrc_io_set_param_string(io_, "basename", pfx);
    mrc_io_set_param_string(io_, "outdir", outdir);
    mrc_io_set_from_options(io_);
    mrc_io_setup(io_);
    mrc_io_view(io_);
  }

  ~MrcIo()
  {
    mrc_io_destroy(io_);
  }

  void open(Int3 rn = {}, Int3 rx = {1000000, 1000000, 1000000})
  {
    int gdims[3];
    mrc_domain_get_global_dims(ppsc->mrc_domain_, gdims);
    int slab_off[3], slab_dims[3];
    for (int d = 0; d < 3; d++) {
      if (rx[d] > gdims[d])
	rx[d] = gdims[d];
      
      slab_off[d] = rn[d];
      slab_dims[d] = rx[d] - rn[d];
    }
    
    mrc_io_open(io_, "w", ppsc->timestep, ppsc->timestep * ppsc->grid().dt);
    
    // save some basic info about the run in the output file
    struct mrc_obj *obj = mrc_obj_create(mrc_io_comm(io_));
    mrc_obj_set_name(obj, "psc");
    mrc_obj_dict_add_int(obj, "timestep", ppsc->timestep);
    mrc_obj_dict_add_float(obj, "time", ppsc->timestep * ppsc->grid().dt);
    mrc_obj_dict_add_float(obj, "cc", ppsc->grid().norm.cc);
    mrc_obj_dict_add_float(obj, "dt", ppsc->grid().dt);
    mrc_obj_write(obj, io_);
    mrc_obj_destroy(obj);
    
    if (strcmp(mrc_io_type(io_), "xdmf_collective") == 0) {
      mrc_io_set_param_int3(io_, "slab_off", slab_off);
      mrc_io_set_param_int3(io_, "slab_dims", slab_dims);
    }
  }

  void close()
  {
    mrc_io_close(io_);
  }

  template <typename Mfields>
  void write_mflds(Mfields& mflds, const std::string& name, const std::vector<std::string>& comp_names)
  {
    write_mflds(io_, mflds, name, comp_names);
  }

  // static version so it can be used elsewhere without MrcIo wrapper
  template <typename Mfields>
  static void write_mflds(mrc_io* io, Mfields& mflds,
			  const std::string& name, const std::vector<std::string>& comp_names)
  {
    int n_comps = comp_names.size();
    // FIXME, should generally equal the # of component in mflds,
    // but this way allows us to write fewer components, useful to hack around 16-bit vpic material ids,
    // stored together as AOS with floats...
    
    mrc_fld* fld = mrc_domain_m3_create(ppsc->mrc_domain_);
    mrc_fld_set_name(fld, name.c_str());
    mrc_fld_set_param_int(fld, "nr_ghosts", 0);
    mrc_fld_set_param_int(fld, "nr_comps", n_comps);
    mrc_fld_setup(fld);
    for (int m = 0; m < n_comps; m++) {
      mrc_fld_set_comp_name(fld, m, comp_names[m].c_str());
    }
    
    for (int p = 0; p < mflds.n_patches(); p++) {
      mrc_fld_patch *m3p = mrc_fld_patch_get(fld, p);
      mrc_fld_foreach(fld, i,j,k, 0,0) {
	for (int m = 0; m < n_comps; m++) {
	  MRC_M3(m3p ,m , i,j,k) = mflds[p](m, i,j,k);
	}
      } mrc_fld_foreach_end;
      mrc_fld_patch_put(fld);
    }
    
    mrc_fld_write(fld, io);
    mrc_fld_destroy(fld);
  }

  mrc_io* io_;
};

