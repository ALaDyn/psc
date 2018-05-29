
#pragma once

#include "psc_fields_cuda.h"
#include "cuda_mfields.h"
#include "fields.hxx"

#include "mrc_ddc_private.h"

#include <thrust/gather.h>
#include <thrust/scatter.h>

using real_t = float;

#define mrc_ddc_multi(ddc) mrc_to_subobj(ddc, struct mrc_ddc_multi)

static void
mrc_ddc_multi_set_mpi_type(struct mrc_ddc *ddc)
{
  if (ddc->size_of_type == sizeof(float)) {
    ddc->mpi_type = MPI_FLOAT;
  } else if (ddc->size_of_type == sizeof(double)) {
    ddc->mpi_type = MPI_DOUBLE;
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// mrc_ddc_multi_free_buffers

static void
mrc_ddc_multi_free_buffers(struct mrc_ddc *ddc, struct mrc_ddc_pattern2 *patt2)
{
  free(patt2->send_buf);
  free(patt2->recv_buf);
  free(patt2->local_buf);
}

// ----------------------------------------------------------------------
// mrc_ddc_multi_alloc_buffers

static void
mrc_ddc_multi_alloc_buffers(struct mrc_ddc *ddc, struct mrc_ddc_pattern2 *patt2,
			    int n_fields)
{
  if (ddc->size_of_type > patt2->max_size_of_type ||
      n_fields > patt2->max_n_fields) {

    if (ddc->size_of_type > patt2->max_size_of_type) {
      patt2->max_size_of_type = ddc->size_of_type;
    }
    if (n_fields > patt2->max_n_fields) {
      patt2->max_n_fields = n_fields;
    }

    mrc_ddc_multi_free_buffers(ddc, patt2);

    patt2->recv_buf = malloc(patt2->n_recv * patt2->max_n_fields * patt2->max_size_of_type);
    patt2->send_buf = malloc(patt2->n_send * patt2->max_n_fields * patt2->max_size_of_type);
    patt2->local_buf = malloc(patt2->local_buf_size * patt2->max_n_fields * patt2->max_size_of_type);
  }
}

// ----------------------------------------------------------------------
// ddc_run_begin

static void
ddc_run_begin(struct mrc_ddc *ddc, struct mrc_ddc_pattern2 *patt2,
	      int mb, int me,
	      cuda_mfields& cmflds, thrust::host_vector<real_t>& h_flds,
	      void (*to_buf)(int mb, int me, int p, int ilo[3], int ihi[3], void *buf,
			     cuda_mfields& cmflds, thrust::host_vector<real_t>& h_flds))
{
  struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc);
  struct mrc_ddc_rank_info *ri = patt2->ri;

  // communicate aggregated buffers
  // post receives
  patt2->recv_cnt = 0;
  char* p = (char*) patt2->recv_buf;
  for (int r = 0; r < sub->mpi_size; r++) {
    if (r != sub->mpi_rank && ri[r].n_recv_entries) {
      MPI_Irecv(p, ri[r].n_recv * (me - mb), ddc->mpi_type,
		r, 0, ddc->obj.comm, &patt2->recv_req[patt2->recv_cnt++]);
      p += ri[r].n_recv * (me - mb) * ddc->size_of_type;
    }
  }  
  assert(p == (char*)patt2->recv_buf + patt2->n_recv * (me - mb) * ddc->size_of_type);

  // post sends
  patt2->send_cnt = 0;
  p = (char*) patt2->send_buf;
  for (int r = 0; r < sub->mpi_size; r++) {
    if (r != sub->mpi_rank && ri[r].n_send_entries) {
      void *p0 = p;
      for (int i = 0; i < ri[r].n_send_entries; i++) {
	struct mrc_ddc_sendrecv_entry *se = &ri[r].send_entry[i];
	to_buf(mb, me, se->patch, se->ilo, se->ihi, p, cmflds, h_flds);
	p += se->len * (me - mb) * ddc->size_of_type;
      }
      MPI_Isend(p0, ri[r].n_send * (me - mb), ddc->mpi_type,
		r, 0, ddc->obj.comm, &patt2->send_req[patt2->send_cnt++]);
    }
  }  
  assert(p == (char*) patt2->send_buf + patt2->n_send * (me - mb) * ddc->size_of_type);
}

// ----------------------------------------------------------------------
// ddc_run_end

static void
ddc_run_end(struct mrc_ddc *ddc, struct mrc_ddc_pattern2 *patt2,
	    int mb, int me, cuda_mfields& cmflds,
	    thrust::host_vector<real_t>& h_flds,
	    void (*from_buf)(int mb, int me, int p, int ilo[3], int ihi[3], void *buf,
			     cuda_mfields& cmflds, thrust::host_vector<real_t>& h_flds))
{
  struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc);
  struct mrc_ddc_rank_info *ri = patt2->ri;

  MPI_Waitall(patt2->recv_cnt, patt2->recv_req, MPI_STATUSES_IGNORE);

  char* p = (char*) patt2->recv_buf;
  for (int r = 0; r < sub->mpi_size; r++) {
    if (r != sub->mpi_rank) {
      for (int i = 0; i < ri[r].n_recv_entries; i++) {
	struct mrc_ddc_sendrecv_entry *re = &ri[r].recv_entry[i];
	from_buf(mb, me, re->patch, re->ilo, re->ihi, p, cmflds, h_flds);
	p += re->len * (me - mb) * ddc->size_of_type;
      }
    }
  }

  MPI_Waitall(patt2->send_cnt, patt2->send_req, MPI_STATUSES_IGNORE);
}

// ======================================================================
// CudaBnd

struct CudaBnd
{
  using Mfields = MfieldsCuda;
  using fields_t = typename Mfields::fields_t;
  using real_t = typename Mfields::real_t;
  using Fields = Fields3d<fields_t>;

  // ----------------------------------------------------------------------
  // ctor
  
  CudaBnd(const Grid_t& grid, mrc_domain* domain, int ibn[3])
  {
    static struct mrc_ddc_funcs ddc_funcs;

    ddc_ = mrc_domain_create_ddc(domain);
    mrc_ddc_set_funcs(ddc_, &ddc_funcs);
    mrc_ddc_set_param_int3(ddc_, "ibn", ibn);
    mrc_ddc_set_param_int(ddc_, "max_n_fields", 24);
    mrc_ddc_set_param_int(ddc_, "size_of_type", sizeof(real_t));
    mrc_ddc_setup(ddc_);
  }

  // ----------------------------------------------------------------------
  // dtor
  
  ~CudaBnd()
  {
    mrc_ddc_destroy(ddc_);
  }

  // ----------------------------------------------------------------------
  // add_ghosts
  
  void add_ghosts(Mfields& mflds, int mb, int me)
  {
    cuda_mfields& cmflds = *mflds.cmflds;
    thrust::device_ptr<real_t> d_flds{cmflds.data()};
    thrust::host_vector<real_t> h_flds{d_flds, d_flds + cmflds.n_fields * cmflds.n_cells};

    struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc_);
    
    mrc_ddc_multi_set_mpi_type(ddc_);
    mrc_ddc_multi_alloc_buffers(ddc_, &sub->add_ghosts2, me - mb);
    ddc_run_begin(ddc_, &sub->add_ghosts2, mb, me, cmflds, h_flds, copy_to_buf);
    add_local(&sub->add_ghosts2, mb, me, h_flds, cmflds);
    ddc_run_end(ddc_, &sub->add_ghosts2, mb, me, cmflds, h_flds, add_from_buf);
    thrust::copy(h_flds.begin(), h_flds.end(), d_flds);
  }
  
  void add_local(struct mrc_ddc_pattern2 *patt2, int mb, int me,
		 thrust::host_vector<real_t>& h_flds, cuda_mfields& cmflds)
  {
    struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc_);
    struct mrc_ddc_rank_info *ri = patt2->ri;
    
    for (int i = 0; i < ri[sub->mpi_rank].n_send_entries; i++) {
      struct mrc_ddc_sendrecv_entry *se = &ri[sub->mpi_rank].send_entry[i];
      struct mrc_ddc_sendrecv_entry *re = &ri[sub->mpi_rank].recv_entry[i];
      assert(se->ihi[0] - se->ilo[0] == re->ihi[0] - re->ilo[0]);
      assert(se->ihi[1] - se->ilo[1] == re->ihi[1] - re->ilo[1]);
      assert(se->ihi[2] - se->ilo[2] == re->ihi[2] - re->ilo[2]);
      assert(se->nei_patch == re->patch);
      if (se->ilo[0] == se->ihi[0] ||
	  se->ilo[1] == se->ihi[1] ||
	  se->ilo[2] == se->ihi[2]) { // FIXME, we shouldn't even create these
	continue;
      }

      uint size = (me - mb) * (se->ihi[0] - se->ilo[0]) * (se->ihi[1] - se->ilo[1]) * (se->ihi[2] - se->ilo[2]);
      std::vector<uint> map_send(size);
      std::vector<uint> map_recv(size);
      map_setup(map_send, mb, me, se->patch, se->ilo, se->ihi, cmflds);
      map_setup(map_recv, mb, me, re->patch, re->ilo, re->ihi, cmflds);

      std::vector<real_t> buf(size);
#if 0
      thrust::gather(map_send.begin(), map_send.end(), h_flds, buf);
#else
      for (int i = 0; i < map_send.size(); i++) {
	buf[i] = h_flds[map_send[i]];
      }
#endif
      auto p = buf.begin();
      for (auto idx : map_recv) {
	h_flds[idx] += *p++;
      }
    }
  }

  // ----------------------------------------------------------------------
  // fill_ghosts

  void fill_ghosts(Mfields& mflds, int mb, int me)
  {
    cuda_mfields& cmflds = *mflds.cmflds;
    thrust::device_ptr<real_t> d_flds{cmflds.data()};
    thrust::host_vector<real_t> h_flds{d_flds, d_flds + cmflds.n_fields * cmflds.n_cells};
    // FIXME
    // I don't think we need as many points, and only stencil star
    // rather then box
    struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc_);
    
    mrc_ddc_multi_set_mpi_type(ddc_);
    mrc_ddc_multi_alloc_buffers(ddc_, &sub->fill_ghosts2, me - mb);
    ddc_run_begin(ddc_, &sub->fill_ghosts2, mb, me, cmflds, h_flds, copy_to_buf);
    fill_local(&sub->fill_ghosts2, mb, me, h_flds, cmflds);
    ddc_run_end(ddc_, &sub->fill_ghosts2, mb, me, cmflds, h_flds, copy_from_buf);

    thrust::copy(h_flds.begin(), h_flds.end(), d_flds);
  }

  void fill_local(struct mrc_ddc_pattern2 *patt2, int mb, int me,
		  thrust::host_vector<real_t>& h_flds, cuda_mfields& cmflds)
  {
    struct mrc_ddc_multi *sub = mrc_ddc_multi(ddc_);
    struct mrc_ddc_rank_info *ri = patt2->ri;

    uint buf_size = 0;
    for (int i = 0; i < ri[sub->mpi_rank].n_send_entries; i++) {
      struct mrc_ddc_sendrecv_entry *se = &ri[sub->mpi_rank].send_entry[i];
      if (se->ilo[0] == se->ihi[0] ||
	  se->ilo[1] == se->ihi[1] ||
	  se->ilo[2] == se->ihi[2]) { // FIXME, we shouldn't even create these
	continue;
      }
      uint size = (me - mb) * (se->ihi[0] - se->ilo[0]) * (se->ihi[1] - se->ilo[1]) * (se->ihi[2] - se->ilo[2]);
      buf_size += size;
    }

    std::vector<uint> map_send(buf_size);
    std::vector<uint> map_recv(buf_size);
    std::vector<real_t> buf(buf_size);

    uint off = 0;
    for (int i = 0; i < ri[sub->mpi_rank].n_send_entries; i++) {
      struct mrc_ddc_sendrecv_entry *se = &ri[sub->mpi_rank].send_entry[i];
      struct mrc_ddc_sendrecv_entry *re = &ri[sub->mpi_rank].recv_entry[i];
      if (se->ilo[0] == se->ihi[0] ||
	  se->ilo[1] == se->ihi[1] ||
	  se->ilo[2] == se->ihi[2]) { // FIXME, we shouldn't even create these
	continue;
      }
      uint size = (me - mb) * (se->ihi[0] - se->ilo[0]) * (se->ihi[1] - se->ilo[1]) * (se->ihi[2] - se->ilo[2]);
      map_setup(map_send, mb, me, se->patch, se->ilo, se->ihi, cmflds);
      map_setup(map_recv, mb, me, re->patch, re->ilo, re->ihi, cmflds);
#if 0
      thrust::gather(map_send.begin(), map_send.end(), h_flds, buf);
      thrust::scatter(buf, buf + size, map_recv.begin(), h_flds);
#else
      for (int i = 0; i < size; i++) {
	buf[off + i] = h_flds[map_send[i]];
      }
      for (int i = 0; i < size; i++) {
	h_flds[map_recv[i]] = buf[off + i];
      }
#endif
      off += size;
    }
  }

  static void map_setup(std::vector<uint>& map, int mb, int me, int p, int ilo[3], int ihi[3],
			cuda_mfields& cmflds)
  {
    auto cur = map.begin();
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    *cur++ = cmflds.index(m, ix,iy,iz, p);
	  }
	}
      }
    }
  }

  // ----------------------------------------------------------------------
  // copy_to_buf

  static void copy_to_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			  void *_buf, cuda_mfields& cmflds,
			  thrust::host_vector<real_t>& h_flds)
  {
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    uint idx = cmflds.index(m, ix,iy,iz, p);
	    *buf++ = h_flds[idx];
	  }
	}
      }
    }
  }

  // ----------------------------------------------------------------------
  // add_from_buf

  static void add_from_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			   void *_buf, cuda_mfields& cmflds,
			  thrust::host_vector<real_t>& h_flds)
  {
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    uint idx = cmflds.index(m, ix,iy,iz, p);
	    h_flds[idx] += *buf++;
	  }
	}
      }
    }
  }
  
  // ----------------------------------------------------------------------
  // copy_from_buf

  static void copy_from_buf(int mb, int me, int p, int ilo[3], int ihi[3],
			    void *_buf, cuda_mfields& cmflds,
			    thrust::host_vector<real_t>& h_flds)
  {
    real_t *buf = static_cast<real_t*>(_buf);
    
    for (int m = mb; m < me; m++) {
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {
	  for (int ix = ilo[0]; ix < ihi[0]; ix++) {
	    uint idx = cmflds.index(m, ix,iy,iz, p);
	    h_flds[idx] = *buf++;
	  }
	}
      }
    }
  }

private:
  mrc_ddc* ddc_;
};
