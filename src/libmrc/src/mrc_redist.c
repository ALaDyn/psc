
#include <mrc_redist.h>
#include <mrc_domain.h>

#include <stdlib.h>

void
mrc_redist_init(struct mrc_redist *redist, struct mrc_domain *domain,
		int slab_offs[3], int slab_dims[3], int nr_writers)
{
  redist->domain = domain;
  redist->comm = mrc_domain_comm(domain);
  MPI_Comm_rank(redist->comm, &redist->rank);
  MPI_Comm_size(redist->comm, &redist->size);

  redist->nr_writers = nr_writers;
  redist->writers = calloc(nr_writers, sizeof(*redist->writers));
  // setup writers, just use first nr_writers ranks,
  // could do something fancier in the future
  redist->is_writer = 0;
  for (int i = 0; i < nr_writers; i++) {
    redist->writers[i] = i;
    if (i == redist->rank) {
      redist->is_writer = 1;
    }
  }
  
  MPI_Comm_split(redist->comm, redist->is_writer, redist->rank, &redist->comm_writers);

  int gdims[3];
  mrc_domain_get_global_dims(domain, gdims);
  int nr_patches;
  mrc_domain_get_patches(domain, &nr_patches);
  int nr_global_patches;
  mrc_domain_get_nr_global_patches(domain, &nr_global_patches);
  for (int d = 0; d < 3; d++) {
    if (slab_dims[d]) {
      redist->slab_dims[d] = slab_dims[d];
    } else {
      redist->slab_dims[d] = gdims[d];
    }
    redist->slab_offs[d] = slab_offs[d];
  }
  redist->slow_dim = 2;
  while (gdims[redist->slow_dim] == 1) {
    redist->slow_dim--;
  }
  assert(redist->slow_dim >= 0);
  int total_slow_indices = redist->slab_dims[redist->slow_dim];
  redist->slow_indices_per_writer = total_slow_indices / nr_writers;
  redist->slow_indices_rmndr = total_slow_indices % nr_writers;
}

void
mrc_redist_destroy(struct mrc_redist *redist)
{
  free(redist->writers);
  MPI_Comm_free(&redist->comm_writers);
}

static void
mrc_redist_writer_offs_dims(struct mrc_redist *redist, int writer,
			    int *writer_offs, int *writer_dims)
{
  for (int d = 0; d < 3; d++) {
    writer_dims[d] = redist->slab_dims[d];
    writer_offs[d] = redist->slab_offs[d];
  }
  writer_dims[redist->slow_dim] = redist->slow_indices_per_writer + (writer < redist->slow_indices_rmndr);
  if (writer < redist->slow_indices_rmndr) {
    writer_offs[redist->slow_dim] += (redist->slow_indices_per_writer + 1) * writer;
  } else {
    writer_offs[redist->slow_dim] += redist->slow_indices_rmndr +
      redist->slow_indices_per_writer * writer;
  }
}

#define BUFLOOP(ix, iy, iz, ilo, hi) \
      for (int iz = ilo[2]; iz < ihi[2]; iz++) {\
	for (int iy = ilo[1]; iy < ihi[1]; iy++) {\
	  for (int ix = ilo[0]; ix < ihi[0]; ix++)

#define BUFLOOP_END }}

// ----------------------------------------------------------------------
// mrc_redist_write_send_init

static void
mrc_redist_write_send_init(struct mrc_redist *redist, struct mrc_fld *m3)
{
  struct mrc_redist_write_send *send = &redist->write_send;

  send->writers = calloc(redist->nr_writers, sizeof(*send->writers));
  send->reqs = calloc(redist->nr_writers, sizeof(*send->reqs));

  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(redist->domain, &nr_patches);

  for (int writer = 0; writer < redist->nr_writers; writer++) {
    // don't send to self
    if (redist->writers[writer] == redist->rank) {
      continue;
    }
    int writer_offs[3], writer_dims[3];
    mrc_redist_writer_offs_dims(redist, writer, writer_offs, writer_dims);

    // find buf_size per writer
    int buf_n = 0;
    for (int p = 0; p < nr_patches; p++) {
      int ilo[3], ihi[3];
      bool has_intersection =
	find_intersection(ilo, ihi, patches[p].off, patches[p].ldims,
			  writer_offs, writer_dims);
      if (!has_intersection)
	continue;

      int *ldims = patches[p].ldims;
      buf_n += ldims[0] * ldims[1] * ldims[2];
    }
    if (buf_n == 0) {
      continue;
    }

    // allocate buf per writer
    //mprintf("to writer %d buf_size %d\n", writer, buf_sizes[writer]);
    send->writers[writer].buf_size = buf_n;
    send->writers[writer].buf = malloc(buf_n * m3->_nd->size_of_type);
    assert(send->writers[writer].buf);
  }
}

// ----------------------------------------------------------------------
// mrc_redist_write_send_destroy

static void
mrc_redist_write_send_destroy(struct mrc_redist *redist)
{
  struct mrc_redist_write_send *send = &redist->write_send;

  for (int writer = 0; writer < redist->nr_writers; writer++) {
    free(send->writers[writer].buf);
  }
  free(send->writers);
  free(send->reqs);
}

// ----------------------------------------------------------------------
// mrc_redist_write_send_begin

static void
mrc_redist_write_send_begin(struct mrc_redist *redist, struct mrc_fld *m3, int m)
{
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(m3->_domain, &nr_patches);

  struct mrc_redist_write_send *send = &redist->write_send;

  for (int writer = 0; writer < redist->nr_writers; writer++) {
    if (!send->writers[writer].buf) {
      send->reqs[writer] = MPI_REQUEST_NULL;
      continue;
    }
    int writer_offs[3], writer_dims[3];
    mrc_redist_writer_offs_dims(redist, writer, writer_offs, writer_dims);

    // fill buf per writer
    int buf_n = 0;
    for (int p = 0; p < nr_patches; p++) {
      int ilo[3], ihi[3];
      int *off = patches[p].off;
      bool has_intersection =
	find_intersection(ilo, ihi, off, patches[p].ldims,
			  writer_offs, writer_dims);
      if (!has_intersection)
	continue;

      struct mrc_patch_info info;
      mrc_domain_get_local_patch_info(m3->_domain, p, &info);
      assert(!m3->_aos);
      switch (mrc_fld_data_type(m3)) {
      case MRC_NT_FLOAT:
      {
      	float *buf_ptr = (float *) send->writers[writer].buf + buf_n;
      	BUFLOOP(ix, iy, iz, ilo, ihi) {
    	    *buf_ptr++ = MRC_S5(m3, ix-off[0],iy-off[1],iz-off[2], m, p);
      	} BUFLOOP_END
      	break;
      }
      case MRC_NT_DOUBLE:
      {
      	double *buf_ptr = (double *) send->writers[writer].buf + buf_n;
      	BUFLOOP(ix, iy, iz, ilo, ihi) {
          *buf_ptr++ = MRC_D5(m3, ix-off[0],iy-off[1],iz-off[2], m, p);
      	} BUFLOOP_END
      	break;
      }
      case MRC_NT_INT:
      {
      	int *buf_ptr = (int *) send->writers[writer].buf + buf_n;
      	BUFLOOP(ix, iy, iz, ilo, ihi) {
    	    *buf_ptr++ = MRC_I5(m3, ix-off[0],iy-off[1],iz-off[2], m, p);
      	} BUFLOOP_END
      	break;
      }
      default:
      {
      	assert(0);
      }
      }
      buf_n += (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
    }
    assert(buf_n == send->writers[writer].buf_size);
    
    MPI_Datatype mpi_dtype;
    switch (mrc_fld_data_type(m3)) {
    case MRC_NT_FLOAT:
      mpi_dtype = MPI_FLOAT;
      break;
    case MRC_NT_DOUBLE:
      mpi_dtype = MPI_DOUBLE;
      break;
    case MRC_NT_INT:
      mpi_dtype = MPI_INT;
      break;
    default:
      assert(0);
    }

    mprintf("isend to %d\n", redist->writers[writer]);
    MPI_Isend(send->writers[writer].buf, send->writers[writer].buf_size, mpi_dtype,
	      redist->writers[writer], 0x1000, redist->comm,
	      &send->reqs[writer]);
  }
}

// ----------------------------------------------------------------------
// mrc_redist_write_send_end

static void
mrc_redist_write_send_end(struct mrc_redist *redist, struct mrc_fld *m3, int m)
{
  struct mrc_redist_write_send *send = &redist->write_send;

  mprintf("send_end: waitall cnt = %d\n", redist->nr_writers);
  MPI_Waitall(redist->nr_writers, send->reqs, MPI_STATUSES_IGNORE);
}
    
// ----------------------------------------------------------------------
// mrc_redist_write_recv_init

static void
mrc_redist_write_recv_init(struct mrc_redist *redist, struct mrc_ndarray *nd,
			   int size_of_type)
{
  struct mrc_redist_write_recv *recv = &redist->write_recv;

  // find out who's sending, OPT: this way is not very scalable
  // could also be optimized by just looking at slow_dim
  // FIXME, figure out pattern and cache, at least across components

  int nr_global_patches;
  mrc_domain_get_nr_global_patches(redist->domain, &nr_global_patches);

  recv->n_recv_patches = 0;
  for (int gp = 0; gp < nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(redist->domain, gp, &info);

    int ilo[3], ihi[3];
    int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					     mrc_ndarray_offs(nd), mrc_ndarray_dims(nd));
    if (!has_intersection) {
      continue;
    }

    recv->n_recv_patches++;
  }
  mprintf("n_recv_patches %d\n", recv->n_recv_patches);

  recv->recv_patches = calloc(recv->n_recv_patches, sizeof(*recv->recv_patches));

  struct mrc_redist_block **recv_patches_by_rank = calloc(redist->size + 1, sizeof(*recv_patches_by_rank));

  int cur_rank = -1;
  recv->n_recv_patches = 0;
  for (int gp = 0; gp < nr_global_patches; gp++) {
    struct mrc_patch_info info;
    mrc_domain_get_global_patch_info(redist->domain, gp, &info);

    int ilo[3], ihi[3];
    int has_intersection = find_intersection(ilo, ihi, info.off, info.ldims,
					     mrc_ndarray_offs(nd), mrc_ndarray_dims(nd));
    if (!has_intersection) {
      continue;
    }

    assert(info.rank >= cur_rank);
    while (cur_rank < info.rank) {
      cur_rank++;
      recv_patches_by_rank[cur_rank] = &recv->recv_patches[recv->n_recv_patches];
      //mprintf("rank %d patches start at %d\n", cur_rank, recv->n_recv_patches);
    }

    struct mrc_redist_block *recv_patch = &recv->recv_patches[recv->n_recv_patches];
    for (int d = 0; d < 3; d++) {
      recv_patch->ilo[d] = ilo[d];
      recv_patch->ihi[d] = ihi[d];
    }
    recv->n_recv_patches++;
  }

  while (cur_rank < redist->size) {
    cur_rank++;
    recv_patches_by_rank[cur_rank] = &recv->recv_patches[recv->n_recv_patches];
    //mprintf("rank %d patches start at %d\n", cur_rank, recv->n_recv_patches);
  }

  recv->n_peers = 0;
  for (int rank = 0; rank < redist->size; rank++) {
    struct mrc_redist_block *begin = recv_patches_by_rank[rank];
    struct mrc_redist_block *end   = recv_patches_by_rank[rank+1];

    if (begin == end) {
      continue;
    }

    //mprintf("peer rank %d # = %ld\n", rank, end - begin);
    recv->n_peers++;
  }
  mprintf("n_peers %d\n", recv->n_peers);

  recv->peers = calloc(recv->n_peers, sizeof(*recv->peers));
  recv->n_peers = 0;
  for (int rank = 0; rank < redist->size; rank++) {
    struct mrc_redist_block *begin = recv_patches_by_rank[rank];
    struct mrc_redist_block *end   = recv_patches_by_rank[rank+1];

    if (begin == end) {
      continue;
    }

    struct mrc_redist_peer *peer = &recv->peers[recv->n_peers];
    peer->rank = rank;
    peer->begin = begin;
    peer->end = end;

    // for remote patches, allocate buffer
    if (peer->rank != redist->rank) {
      peer->buf_size = 0;
      for (struct mrc_redist_block *recv_patch = peer->begin; recv_patch < peer->end; recv_patch++) {
	int *ilo = recv_patch->ilo, *ihi = recv_patch->ihi;
	peer->buf_size += (size_t) (ihi[0] - ilo[0]) * (ihi[1] - ilo[1]) * (ihi[2] - ilo[2]);
      }
      
      // alloc aggregate recv buffers
      peer->buf = malloc(peer->buf_size * size_of_type);
    }

    recv->n_peers++;
  }
  free(recv_patches_by_rank);

  recv->reqs = calloc(recv->n_peers, sizeof(*recv->reqs));
}

// ----------------------------------------------------------------------
// mrc_redist_write_recv_begin

static void
mrc_redist_write_recv_begin(struct mrc_redist *redist, struct mrc_ndarray *nd,
			    struct mrc_fld *m3)
{
  struct mrc_redist_write_recv *recv = &redist->write_recv;
  
  for (struct mrc_redist_peer *peer = recv->peers; peer < recv->peers + recv->n_peers; peer++) {
    // skip local patches
    if (peer->rank == redist->rank) {
      recv->reqs[peer - recv->peers] = MPI_REQUEST_NULL;
      continue;
    }

    MPI_Datatype mpi_dtype;
    switch (mrc_fld_data_type(m3)) {
    case MRC_NT_FLOAT:
      mpi_dtype = MPI_FLOAT;
      break;
    case MRC_NT_DOUBLE:
      mpi_dtype = MPI_DOUBLE;
      break;
    case MRC_NT_INT:
      mpi_dtype = MPI_INT;
      break;
    default:
      assert(0);
    }
    
    // recv aggregate buffers
    mprintf("irecv from %d\n", peer->rank);
    MPI_Irecv(peer->buf, peer->buf_size, mpi_dtype, peer->rank, 0x1000, redist->comm,
	      &recv->reqs[peer - recv->peers]);
  }
}

// ----------------------------------------------------------------------
// mrc_redist_write_recv_end

static void
mrc_redist_write_recv_end(struct mrc_redist *redist, struct mrc_ndarray *nd,
			  struct mrc_fld *m3, int m)
{
  struct mrc_redist_write_recv *recv = &redist->write_recv;

  mprintf("recv_end: waitall cnt = %d\n", recv->n_peers);
  MPI_Waitall(recv->n_peers, recv->reqs, MPI_STATUSES_IGNORE);

  for (struct mrc_redist_peer *peer = recv->peers; peer < recv->peers + recv->n_peers; peer++) {
    // skip local patches
    if (peer->rank == redist->rank) {
      continue;
    }

    switch (mrc_fld_data_type(m3)) {
    case MRC_NT_FLOAT: {
      float *buf = peer->buf;
      for (struct mrc_redist_block *recv_patch = peer->begin; recv_patch < peer->end; recv_patch++) {
	int *ilo = recv_patch->ilo, *ihi = recv_patch->ihi;
	BUFLOOP(ix, iy, iz, ilo, ihi) {
	  MRC_S3(nd, ix,iy,iz) = *buf++;
	} BUFLOOP_END;
      }
      break;
    }
    case MRC_NT_DOUBLE: {
      double *buf = peer->buf;
      for (struct mrc_redist_block *recv_patch = peer->begin; recv_patch < peer->end; recv_patch++) {
	int *ilo = recv_patch->ilo, *ihi = recv_patch->ihi;
	BUFLOOP(ix, iy, iz, ilo, ihi) {
	  MRC_D3(nd, ix,iy,iz) = *buf++;
	} BUFLOOP_END;
      }
      break;
    }
    case MRC_NT_INT: {
      int *buf = peer->buf;
      for (struct mrc_redist_block *recv_patch = peer->begin; recv_patch < peer->end; recv_patch++) {
	int *ilo = recv_patch->ilo, *ihi = recv_patch->ihi;
	BUFLOOP(ix, iy, iz, ilo, ihi) {
	  MRC_I3(nd, ix,iy,iz) = *buf++;
	} BUFLOOP_END;
      }
      break;
    }
    default:
      assert(0);
    }    
  }
}

// ----------------------------------------------------------------------
// mrc_redist_write_destroy

static void
mrc_redist_write_destroy(struct mrc_redist *redist)
{
  struct mrc_redist_write_recv *recv = &redist->write_recv;

  free(recv->reqs);
  
  for (struct mrc_redist_peer *peer = recv->peers; peer < recv->peers + recv->n_peers; peer++) {
    free(peer->buf);
  }
  free(recv->peers);

  free(recv->recv_patches);
}

// ----------------------------------------------------------------------
// mrc_redist_write_comm_local

static void
mrc_redist_write_comm_local(struct mrc_redist *redist, struct mrc_ndarray *nd,
			    struct mrc_fld *m3, int m)
{
  struct mrc_redist_write_recv *recv = &redist->write_recv;

  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(m3->_domain, &nr_patches);

  for (int p = 0; p < nr_patches; p++) {
    struct mrc_patch *patch = &patches[p];
    int *off = patch->off, *ldims = patch->ldims;

    int ilo[3], ihi[3];
    bool has_intersection =
      find_intersection(ilo, ihi, off, ldims, mrc_ndarray_offs(nd), mrc_ndarray_dims(nd));
    if (!has_intersection) {
      continue;
    }
    switch (mrc_fld_data_type(m3)) {
    case MRC_NT_FLOAT:
    {
      BUFLOOP(ix, iy, iz, ilo, ihi) {
        MRC_S3(nd, ix,iy,iz) =
          MRC_S5(m3, ix - off[0], iy - off[1], iz - off[2], m, p);
      } BUFLOOP_END
      break;
    }
    case MRC_NT_DOUBLE:
    {
      BUFLOOP(ix, iy, iz, ilo, ihi) {
        MRC_D3(nd, ix,iy,iz) =
          MRC_D5(m3, ix - off[0], iy - off[1], iz - off[2], m, p);
      } BUFLOOP_END
      break;     
    }
    case MRC_NT_INT:
    {
      BUFLOOP(ix, iy, iz, ilo, ihi) {
        MRC_I3(nd, ix,iy,iz) =
          MRC_I5(m3, ix - off[0], iy - off[1], iz - off[2], m, p);
      } BUFLOOP_END
      break;
    }
    default:
    {
      	assert(0);
    }
    }    
  }
}

// ----------------------------------------------------------------------
// mrc_redist_get_ndarray

struct mrc_ndarray *
mrc_redist_get_ndarray(struct mrc_redist *redist, struct mrc_fld *m3)
{
  mrc_redist_write_send_init(redist, m3);

  if (!redist->is_writer) {
    return NULL;
  }
  
  struct mrc_ndarray *nd = mrc_ndarray_create(redist->comm_writers);

  int writer;
  MPI_Comm_rank(redist->comm_writers, &writer);
  int writer_dims[3], writer_off[3];
  mrc_redist_writer_offs_dims(redist, writer, writer_off, writer_dims);
  mprintf("writer_off %d %d %d dims %d %d %d\n",
	  writer_off[0], writer_off[1], writer_off[2],
	  writer_dims[0], writer_dims[1], writer_dims[2]);

  mrc_ndarray_set_param_int_array(nd, "dims", 3, writer_dims);
  mrc_ndarray_set_param_int_array(nd, "offs", 3, writer_off);
  
  switch (mrc_fld_data_type(m3)) {
  case MRC_NT_FLOAT: mrc_ndarray_set_type(nd, "float"); break;
  case MRC_NT_DOUBLE: mrc_ndarray_set_type(nd, "double"); break;
  case MRC_NT_INT: mrc_ndarray_set_type(nd, "int"); break;
  default: assert(0);
  }
  mrc_ndarray_setup(nd);

  mrc_redist_write_recv_init(redist, nd, m3->_nd->size_of_type);

  return nd;
}

// ----------------------------------------------------------------------
// mrc_redist_put_ndarray

void
mrc_redist_put_ndarray(struct mrc_redist *redist, struct mrc_ndarray *nd)
{
  mrc_redist_write_send_destroy(redist);

  if (redist->is_writer) {
    mrc_redist_write_destroy(redist);
    mrc_ndarray_destroy(nd);
  }
}

// ----------------------------------------------------------------------

void
mrc_redist_run(struct mrc_redist *redist, struct mrc_ndarray *nd,
	       struct mrc_fld *m3, int m)
{
  if (redist->is_writer) {
    mrc_redist_write_recv_begin(redist, nd, m3);
    mrc_redist_write_send_begin(redist, m3, m);
    mrc_redist_write_comm_local(redist, nd, m3, m);
    mrc_redist_write_recv_end(redist, nd, m3, m);
    mrc_redist_write_send_end(redist, m3, m);
  } else {
    mrc_redist_write_send_begin(redist, m3, m);
    mrc_redist_write_send_end(redist, m3, m);
  }
}



