
#ifndef CUDA_MFIELDS_H
#define CUDA_MFIELDS_H

#include "cuda_iface.h"
#include "cuda_iface_bnd.h"

#define MAX_BND_FIELDS (17)
#define MAX_BND_COMPONENTS (3)

#include <thrust/device_vector.h>

// ----------------------------------------------------------------------
// cuda_mfields_bnd_map
//
// the maps differs depending on the number of components in the field
// (incorporates specific strides)

struct cuda_mfields_bnd_map {
  int *h_map_out; // maps thread id to a particular offset for ghosts in the flds array 
  int *d_map_out;
  int nr_map_out; // number of entries in the map
  int *h_map_in; // maps thread id to a particular offset for ghosts in the flds array 
  int *d_map_in;
  int nr_map_in; // number of entries in the map
};

// ----------------------------------------------------------------------
// cuda_mfields_bnd

struct cuda_mfields_bnd {
  int n_patches;
  int im[3];
  int ib[3];
  struct cuda_mfields_bnd_patch *bnd_by_patch;
  fields_cuda_real_t *d_bnd_buf;
  fields_cuda_real_t *h_bnd_buf;
  int *h_nei_patch;
  int *d_nei_patch;
  struct cuda_mfields_bnd_map map[MAX_BND_FIELDS];
};

// ----------------------------------------------------------------------
// cuda_mfields

struct cuda_mfields
{
  using real_t = float;

  cuda_mfields(Grid_t& grid, int n_fields, const Int3& ibn);
  cuda_mfields(const cuda_mfields&) = delete;

  void axpy_comp_yz(int ym, float a, cuda_mfields *x, int xm);
  void zero_comp_yz(int xm);

  fields_single_t get_host_fields();
  void copy_to_device(int p, fields_single_t h_flds, int mb, int me);
  void copy_from_device(int p, fields_single_t h_flds, int mb, int me);

  mrc_json_t to_json();
  void dump(const char *filename);

  thrust::device_vector<fields_cuda_real_t> d_flds;
  int ib[3], im[3]; // FIXME, should be called off, ldims
  int n_patches;
  int n_fields;
  int n_cells_per_patch;
  int n_cells;
  Int3 ldims;                     // number of cells per direction in each patch
  float dx[3];                    // cell size (in actual length units)
};

#endif
