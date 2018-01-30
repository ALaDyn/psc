
#ifndef BND_PARTICLES_ORDERED_IMPL_HXX
#define BND_PARTICLES_ORDERED_IMPL_HXX

template<typename MP>
struct bnd_particles_policy_ordered
{
  using mparticles_t = MP;
  using particle_t = typename mparticles_t::particle_t;
  using ddcp_t = ddc_particles<mparticles_t>;
  using ddcp_patch = typename ddcp_t::patch;
  
  // ----------------------------------------------------------------------
  // find_block_indices_count

  static void
  find_block_indices_count(unsigned int *b_idx, unsigned int *b_cnts,
			   struct psc_mparticles *mprts, int p, int off)
  {
    typename mparticles_t::patch_t& prts = mparticles_t(mprts)[p];

    unsigned int n_prts = prts.size();
    int *b_mx = prts.b_mx;
    for (int i = off; i < n_prts; i++) {
      particle_t *part = &prts[i];
      int b_pos[3];
      prts.get_block_pos(&part->xi, b_pos);
      assert(b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	     b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	     b_pos[2] >= 0 && b_pos[2] < b_mx[2]);
      b_idx[i] = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
      b_cnts[b_idx[i]]++;
    }
  }

  // ----------------------------------------------------------------------
  // find_block_indices_count_reorder

  static void _mrc_unused
  find_block_indices_count_reorder(struct psc_mparticles *mprts, int p)
  {
    typename mparticles_t::patch_t& prts = mparticles_t(mprts)[p];

    unsigned int n_prts = prts.size();
    unsigned int cnt = n_prts;
    int *b_mx = prts.b_mx;
    memset(prts.b_cnt, 0, (prts.nr_blocks + 1) * sizeof(*prts.b_cnt));

    for (int i = 0; i < n_prts; i++) {
      particle_t *part = &prts[i];
      int b_pos[3];
      prts.get_block_pos(&part->xi, b_pos);
      if (b_pos[0] >= 0 && b_pos[0] < b_mx[0] &&
	  b_pos[1] >= 0 && b_pos[1] < b_mx[1] &&
	  b_pos[2] >= 0 && b_pos[2] < b_mx[2]) {
	prts.b_idx[i] = (b_pos[2] * b_mx[1] + b_pos[1]) * b_mx[0] + b_pos[0];
      } else { // out of bounds
	prts.b_idx[i] = prts.nr_blocks;
	prts[cnt] = *part;
	cnt++;
      }
      prts.b_cnt[prts.b_idx[i]]++;
    }
  }

  static void _mrc_unused
  count_and_reorder_to_back(struct psc_mparticles *mprts, int p)
  {
    typename mparticles_t::patch_t& prts = mparticles_t(mprts)[p];

    memset(prts.b_cnt, 0, (prts.nr_blocks + 1) * sizeof(*prts.b_cnt));
    unsigned int n_prts = prts.size();
    unsigned int cnt = n_prts;
    for (int i = 0; i < n_prts; i++) {
      if (prts.b_idx[i] == prts.nr_blocks) {
	prts[cnt] = prts[i];
	cnt++;
      }
      prts.b_cnt[prts.b_idx[i]]++;
    }
  }

  static void _mrc_unused
  reorder_to_back(struct psc_mparticles *mprts, int p)
  {
    typename mparticles_t::patch_t& prts = mparticles_t(mprts)[p];

    unsigned int n_prts = prts.size();
    unsigned int cnt = n_prts;
    for (int i = 0; i < n_prts; i++) {
      if (prts.b_idx[i] == prts.nr_blocks) {
	prts[cnt] = prts[i];
	cnt++;
      }
    }
  }

  // ----------------------------------------------------------------------
  // count_block_indices

  static void
  count_block_indices(unsigned int *b_cnts, unsigned int *b_idx, int n_prts, int off)
  {
    for (int i = off; i < n_prts; i++) {
      b_cnts[b_idx[i]]++;
    }
  }

  // ----------------------------------------------------------------------
  // exclusive_scan

  static void
  exclusive_scan(unsigned int *b_cnts, int n)
  {
    unsigned int sum = 0;
    for (int i = 0; i < n; i++) {
      unsigned int cnt = b_cnts[i];
      b_cnts[i] = sum;
      sum += cnt;
    }
  }

  // ----------------------------------------------------------------------
  // sort_indices

  static void
  sort_indices(unsigned int *b_idx, unsigned int *b_sum, unsigned int *b_ids, int n_prts)
  {
    for (int n = 0; n < n_prts; n++) {
      unsigned int n_new = b_sum[b_idx[n]]++;
      assert(n_new < n_prts);
      b_ids[n_new] = n;
    }
  }

  // ----------------------------------------------------------------------
  // exchange_mprts_prep
  
  void exchange_mprts_prep(ddcp_t* ddcp, mparticles_t mprts)
  {
    for (int p = 0; p < mprts.n_patches(); p++) {
      mparticles_single_t::patch_t& prts = mprts[p];
      ddcp_patch *dpatch = &ddcp->patches[p];
      
      if (1) {
	//      find_block_indices_count_reorderx(prts);
	count_and_reorder_to_back(mprts.mprts(), p);
      }
      dpatch->buf = prts.get_buf();
      dpatch->begin = dpatch->buf.size();
      
      unsigned int n_send = prts.b_cnt[prts.nr_blocks];
      dpatch->buf.resize(dpatch->begin + n_send);
    }
  }
  
  // ----------------------------------------------------------------------
  // exchange_mprts_post
  
  void exchange_mprts_post(ddcp_t* ddcp, mparticles_t mprts)
  {
    for (int p = 0; p < mprts.n_patches(); p++) {
      mparticles_single_t::patch_t& prts = mprts[p];
      ddcp_patch *dpatch = &ddcp->patches[p];
      
      int n_prts = dpatch->buf.size();
      
      find_block_indices_count(prts.b_idx, prts.b_cnt, mprts.mprts(), p, dpatch->begin);
      exclusive_scan(prts.b_cnt, prts.nr_blocks + 1);
      sort_indices(prts.b_idx, prts.b_cnt, prts.b_ids, n_prts);
      
      // FIXME, why?
      mprts[p].resize(prts.b_cnt[prts.nr_blocks - 1]);
      prts.need_reorder = true; // FIXME, need to honor before get()/put()
    }
  }
};

template<typename MP>
struct psc_bnd_particles_ordered : psc_bnd_particles_sub<MP>, bnd_particles_policy_ordered<MP>
{
  using mparticles_t = MP;

  using psc_bnd_particles_sub<MP>::ddcp;

  // ----------------------------------------------------------------------
  // exchange_particles

  void exchange_particles(mparticles_t mprts)
  {
    static int pr_A, pr_B;
    if (!pr_A) {
      pr_A = prof_register("xchg_mprts_prep", 1., 0, 0);
      pr_B = prof_register("xchg_mprts_post", 1., 0, 0);
    }
    
    prof_restart(pr_time_step_no_comm);
    prof_start(pr_A);
    this->exchange_mprts_prep(ddcp, mprts);
    prof_stop(pr_A);
    
    this->process_and_exchange(mprts);
    
    prof_restart(pr_time_step_no_comm);
    prof_start(pr_B);
    this->exchange_mprts_post(ddcp, mprts);
    prof_stop(pr_B);
    prof_stop(pr_time_step_no_comm);
  }

  // ======================================================================
  // interface to psc_bnd_particles
  // repeated here since there's no way to do this somehow virtual at
  // this spoint
  
  // ----------------------------------------------------------------------
  // create

  static void create(struct psc_bnd_particles *bnd)
  {
    auto sub = static_cast<psc_bnd_particles_ordered*>(bnd->obj.subctx);

    new(sub) psc_bnd_particles_ordered();
  }
  
  // ----------------------------------------------------------------------
  // destroy

  static void destroy(struct psc_bnd_particles *bnd)
  {
    auto sub = static_cast<psc_bnd_particles_ordered*>(bnd->obj.subctx);

    sub->~psc_bnd_particles_ordered();
  }
  
  // ----------------------------------------------------------------------
  // exchange_particles

  static void exchange_particles(struct psc_bnd_particles *bnd,
				 struct psc_mparticles *mprts_base)
  {
    auto sub = static_cast<psc_bnd_particles_ordered*>(bnd->obj.subctx);
    mparticles_t mprts = mprts_base->get_as<mparticles_t>();
    
    sub->exchange_particles(mprts);
    
    mprts.put_as(mprts_base);
  }
};

#endif
