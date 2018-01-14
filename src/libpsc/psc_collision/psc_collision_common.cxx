
#include "psc_collision_private.h"
#include "psc_output_fields_item_private.h"
#include "fields.hxx"

#include <mrc_profile.h>
#include <mrc_params.h>

using Fields = Fields3d<fields_t>;
using real_t = mparticles_t::real_t;

struct psc_collision_sub {
  // parameters
  int every;
  double nu;

  // internal
  struct psc_mfields *mflds;
  struct psc_mfields *mflds_rei;
};

#define psc_collision_sub(o) mrc_to_subobj(o, struct psc_collision_sub)

#define VAR(x) (void *)offsetof(struct psc_collision_sub, x)
static struct param psc_collision_sub_descr[] = {
  { "every"         , VAR(every)       , PARAM_INT(1)     },
  { "nu"            , VAR(nu)          , PARAM_DOUBLE(-1.) },
  {},
};
#undef VAR

enum {
  STATS_MIN,
  STATS_MED,
  STATS_MAX,
  STATS_NLARGE,
  STATS_NCOLL,
  NR_STATS,
};

struct psc_collision_stats {
  real_t s[NR_STATS];
};


// ----------------------------------------------------------------------
// calc_stats

static int
compare(const void *_a, const void *_b)
{
  const real_t *a = (const real_t *) _a;
  const real_t *b = (const real_t *) _b;

  if (*a < *b) {
    return -1;
  } else if (*a > *b) {
    return 1;
  } else {
    return 0;
  }
}

static void
calc_stats(struct psc_collision_stats *stats, real_t *nudts, int cnt)
{
  qsort(nudts, cnt, sizeof(*nudts), compare);
  stats->s[STATS_NLARGE] = 0;
  for (int n = cnt - 1; n >= 0; n--) {
    if (nudts[n] < 1.) {
      break;
    }
    stats->s[STATS_NLARGE]++;
  }
  stats->s[STATS_MIN] = nudts[0];
  stats->s[STATS_MAX] = nudts[cnt-1];
  stats->s[STATS_MED] = nudts[cnt/2];
  stats->s[STATS_NCOLL] = cnt;
  /* mprintf("min %g med %g max %g nlarge %g ncoll %g\n", */
  /* 	  stats->s[STATS_MIN], */
  /* 	  stats->s[STATS_MED], */
  /* 	  stats->s[STATS_MAX], */
  /* 	  stats->s[STATS_NLARGE], */
  /* 	  stats->s[STATS_NCOLL]); */
}

// ----------------------------------------------------------------------
// find_cell_index

static inline int
find_cell_index(particle_t *prt, real_t *dxi, int ldims[3])
{
  int pos[3];
  real_t *xi = &prt->xi;
  for (int d = 0; d < 3; d++) {
    pos[d] = fint(xi[d] * dxi[d]);
    assert(pos[d] >= 0 && pos[d] < ldims[d]);
  }
  return (pos[2] * ldims[1] + pos[1]) * ldims[0] + pos[0];
}

// ----------------------------------------------------------------------
// find_cell_offsets

static void
find_cell_offsets(int offsets[], mparticles_t mprts, int p)
{
  mparticles_t::patch_t& prts = mprts[p];

  real_t dxi[3];
  for (int d = 0; d < 3; d++) {
    dxi[d] = 1.f / ppsc->patch[p].dx[d];
  }
  int *ldims = ppsc->patch[p].ldims;
  int last = 0;
  offsets[last] = 0;
  int n_prts = prts.size();
  for (int n = 0; n < n_prts; n++) {
    particle_t *prt = &prts[n];
    int cell_index = find_cell_index(prt, dxi, ldims);
    assert(cell_index >= last);
    while (last < cell_index) {
      offsets[++last] = n;
    }
  }
  while (last < ldims[0] * ldims[1] * ldims[2]) {
    offsets[++last] = n_prts;
  }
}

// ----------------------------------------------------------------------
// randomize_in_cell

static void
randomize_in_cell(mparticles_t::patch_t& prts, int n_start, int n_end)
{
  int nn = n_end - n_start;
  for (int n = 0; n < nn - 1; n++) {
    int n_partner = n + random() % (nn - n);
    if (n != n_partner) {
      // swap n, n_partner
      std::swap(prts[n_start + n], prts[n_start + n_partner]);
    }
  }
}

// ----------------------------------------------------------------------
// bc

static real_t
bc(mparticles_t::patch_t& prts, real_t nudt1, int n1, int n2)
{
  real_t nudt;
    
  real_t pn1,pn2,pn3,pn4;
  real_t p01,p02,pc01,pc02,pc03,pc04;//p03,p04
  real_t px1,py1,pz1,pcx1,pcy1,pcz1;
  real_t px2,py2,pz2,pcx2,pcy2,pcz2;
  real_t px3,py3,pz3,pcx3,pcy3,pcz3;
  real_t px4,py4,pz4,pcx4,pcy4,pcz4;
  real_t h1,h2,h3,h4,ppc,qqc,ss;
  
  real_t m1,m2,m3,m4;
  real_t q1,q2;//,q3,q4;
//  real_t w1,w2,w3,w4,ww;
  real_t vcx,vcy,vcz;
  real_t bet,gam;
  
  real_t psi,nu;
  real_t nx,ny,nz,nnorm;
  real_t nn1,nx1,ny1,nz1;
  real_t nn2,nx2,ny2,nz2;
  real_t nn3,nx3,ny3,nz3;
  real_t vcx1,vcy1,vcz1;
  real_t vcx2,vcy2,vcz2;
  real_t vcn,vcxr,vcyr,vczr,vcr;
  real_t m12,q12;
  real_t ran1,ran2;
    
  particle_t *prt1 = &prts[n1];
  particle_t *prt2 = &prts[n2];
  
  
  px1=prt1->pxi;
  py1=prt1->pyi;
  pz1=prt1->pzi;
  q1 =particle_qni(prt1);
  m1 =particle_mni(prt1);

  px2=prt2->pxi;
  py2=prt2->pyi;
  pz2=prt2->pzi;
  q2 =particle_qni(prt2);
  m2 =particle_mni(prt2);

  if (q1*q2 == 0.) {
    return 0.; // no Coulomb collisions with neutrals
  }
 
  px1=m1*px1;
  py1=m1*py1;
  pz1=m1*pz1;
  px2=m2*px2;
  py2=m2*py2;
  pz2=m2*pz2;
  
  
// determine absolute value of pre-collision momentum in cm-frame
    
  p01=sqrt(m1*m1+px1*px1+py1*py1+pz1*pz1);
  p02=sqrt(m2*m2+px2*px2+py2*py2+pz2*pz2);
  h1=p01*p02-px1*px2-py1*py2-pz1*pz2;
  ss=m1*m1+m2*m2+2.0*h1;
  h2=ss-m1*m1-m2*m2;
  h3=(h2*h2-4.0*m1*m1*m2*m2)/(4.0*ss);
  if (h3 < 0.) {
    mprintf("WARNING: ss %g (m1+m1)^2 %g in psc_collision_c.c\n",
	    ss, (m1+m2)*(m1+m2));
    return 0.; // nudt = 0 because no collision
  }
  ppc=sqrt(h3);
  
  
// determine cm-velocity
    
  vcx=(px1+px2)/(p01+p02);
  vcy=(py1+py2)/(p01+p02);
  vcz=(pz1+pz2)/(p01+p02);
  
  nnorm=sqrt(vcx*vcx+vcy*vcy+vcz*vcz);
  if (nnorm>0.0) {
    nx=vcx/nnorm;
    ny=vcy/nnorm;
    nz=vcz/nnorm;
  } else {
    nx=0.0;
    ny=0.0;
    nz=0.0;
  }
  bet=nnorm;
  gam=1.0/sqrt(1.0-bet*bet);
  
  
// determine pre-collision momenta in cm-frame
  
      
  pn1=px1*nx+py1*ny+pz1*nz;
  pn2=px2*nx+py2*ny+pz2*nz;
  pc01=sqrt(m1*m1+ppc*ppc);
  pcx1=px1+(gam-1.0)*pn1*nx-gam*vcx*p01;
  pcy1=py1+(gam-1.0)*pn1*ny-gam*vcy*p01;
  pcz1=pz1+(gam-1.0)*pn1*nz-gam*vcz*p01;
  pc02=sqrt(m2*m2+ppc*ppc);
  pcx2=px2+(gam-1.0)*pn2*nx-gam*vcx*p02;
  pcy2=py2+(gam-1.0)*pn2*ny-gam*vcy*p02;
  pcz2=pz2+(gam-1.0)*pn2*nz-gam*vcz*p02;
      
      
//  introduce right-handed coordinate system
      
      
  nn1=sqrt(pcx1*pcx1+pcy1*pcy1+pcz1*pcz1);
  nn2=sqrt(pcx1*pcx1+pcy1*pcy1);
  nn3=nn1*nn2;
  
  if (nn2 != 0.0) {
    nx1=pcx1/nn1;
    ny1=pcy1/nn1;
    nz1=pcz1/nn1;
    
    nx2=pcy1/nn2;
    ny2=-pcx1/nn2;
    nz2=0.0;
    
    nx3=-pcx1*pcz1/nn3;
    ny3=-pcy1*pcz1/nn3;
    nz3=nn2*nn2/nn3;
  } else {
    nx1=0.0;
    ny1=0.0;
    nz1=1.0;
    
    nx2=0.0;
    ny2=1.0;
    nz2=0.0;
    
    nx3=1.0;
    ny3=0.0;
    nz3=0.0;
  }
    
          
// determine relative particle velocity in cm-frame
          
          
  vcx1=pcx1/pc01;
  vcy1=pcy1/pc01;
  vcz1=pcz1/pc01;
  vcx2=pcx2/pc02;
  vcy2=pcy2/pc02;
  vcz2=pcz2/pc02;
  
  vcn=1.0/(1.0-(vcx1*vcx2+vcy1*vcy2+vcz1*vcz2));
  vcxr=vcn*(vcx1-vcx2);
  vcyr=vcn*(vcy1-vcy2);
  vczr=vcn*(vcz1-vcz2);
  vcr = sqrt(vcxr*vcxr+vcyr*vcyr+vczr*vczr);
  if (vcr < 1.0e-20) {
    vcr=1.0e-20;
  }
  
  m3=m1;
  m4=m2;
  /* q3=q1; */
  /* q4=q2; */
//  c      w3=w1
//  c      w4=w2
          
          
// determine absolute value of post-collision momentum in cm-frame
          
  h2=ss-m3*m3-m4*m4;
  h3=(h2*h2-4.0*m3*m3*m4*m4)/(4.0*ss);
  if (h3 < 0.) {
    mprintf("WARNING: ss %g (m3+m4)^2 %g in psc_collision_c.c\n",
	    ss, (m3+m4)*(m3+m4));
    return 0.; // nudt = 0 because no collision
  }
          
  qqc=sqrt(h3);
  m12=m1*m2/(m1+m2);
  q12=q1*q2;
    
  nudt=nudt1 * q12*q12/(m12*m12*vcr*vcr*vcr);
  
  // event generator of angles for post collision vectors
  
  ran1 = (1.0 * random()) / RAND_MAX ;
  ran2 = (1.0 * random()) / RAND_MAX ;
  if (ran2 < 1e-20) {
    ran2 = 1e-20;
  }
  
  nu = 6.28318530717958623200 * ran1;
  
  if(nudt<1.0) {                   // small angle collision
    psi=2.0*atan(sqrt(-0.5*nudt*log(1.0-ran2)));
  } else {
    psi=acos(1.0-2.0*ran2);          // isotropic angles
  }
            
// determine post-collision momentum in cm-frame
                    
  h1=cos(psi);
  h2=sin(psi);
  h3=sin(nu);
  h4=cos(nu);
  
  pc03=sqrt(m3*m3+qqc*qqc);
  pcx3=qqc*(h1*nx1+h2*h3*nx2+h2*h4*nx3);
  pcy3=qqc*(h1*ny1+h2*h3*ny2+h2*h4*ny3);
  pcz3=qqc*(h1*nz1+h2*h3*nz2+h2*h4*nz3);
  
  pc04=sqrt(m4*m4+qqc*qqc);
//  c      pcx4=-qqc*(h1*nx1+h2*h3*nx2+h2*h4*nx3)
//  c      pcy4=-qqc*(h1*ny1+h2*h3*ny2+h2*h4*ny3)
//  c      pcz4=-qqc*(h1*nz1+h2*h3*nz2+h2*h4*nz3)
  pcx4=-pcx3;
  pcy4=-pcy3;
  pcz4=-pcz3;
  
// determine post-collision momentum in lab-frame
  
  
  pn3=pcx3*nx+pcy3*ny+pcz3*nz;
  pn4=pcx4*nx+pcy4*ny+pcz4*nz;
  /* p03=gam*(pc03+bet*pn3); */
  px3=pcx3+(gam-1.0)*pn3*nx+gam*vcx*pc03;
  py3=pcy3+(gam-1.0)*pn3*ny+gam*vcy*pc03;
  pz3=pcz3+(gam-1.0)*pn3*nz+gam*vcz*pc03;
  /* p04=gam*(pc04+bet*pn4); */
  px4=pcx4+(gam-1.0)*pn4*nx+gam*vcx*pc04;
  py4=pcy4+(gam-1.0)*pn4*ny+gam*vcy*pc04;
  pz4=pcz4+(gam-1.0)*pn4*nz+gam*vcz*pc04;
  
  px3=px3/m3;
  py3=py3/m3;
  pz3=pz3/m3;
  px4=px4/m4;
  py4=py4/m4;
  pz4=pz4/m4;
  
  prt1->pxi=px3;
  prt1->pyi=py3;
  prt1->pzi=pz3;
  prt2->pxi=px4;
  prt2->pyi=py4;
  prt2->pzi=pz4;
  
  
  return nudt;
}

// ----------------------------------------------------------------------
// update_rei_before

static void
update_rei_before(struct psc_collision *collision,
		  mparticles_t::patch_t& prts, int n_start, int n_end,
		  int p, int i, int j, int k)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  real_t fnqs = ppsc->coeff.cori;
  mfields_t mf_rei(coll->mflds_rei);
  Fields F(mf_rei[p]);
  F(0, i,j,k) = 0.;
  F(1, i,j,k) = 0.;
  F(2, i,j,k) = 0.;
  for (int n = n_start; n < n_end; n++) {
    particle_t& prt = prts[n];
    F(0, i,j,k) -= prt.pxi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
    F(1, i,j,k) -= prt.pyi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
    F(2, i,j,k) -= prt.pzi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
  }
}

// ----------------------------------------------------------------------
// update_rei_after

static void
update_rei_after(struct psc_collision *collision,
		 mparticles_t::patch_t& prts, int n_start, int n_end,
		 int p, int i, int j, int k)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  real_t fnqs = ppsc->coeff.cori;
  mfields_t mf_rei(coll->mflds_rei);
  Fields F(mf_rei[p]);
  for (int n = n_start; n < n_end; n++) {
    particle_t& prt = prts[n];
    F(0, i,j,k) += prt.pxi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
    F(1, i,j,k) += prt.pyi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
    F(2, i,j,k) += prt.pzi * particle_mni((&prt)) * particle_wni(&prt) * fnqs;
  }
  F(0, i,j,k) /= (coll->every * ppsc->dt);
  F(1, i,j,k) /= (coll->every * ppsc->dt);
  F(2, i,j,k) /= (coll->every * ppsc->dt);
}

// ----------------------------------------------------------------------
// collide_in_cell

static void
collide_in_cell(struct psc_collision *collision,
		mparticles_t::patch_t& prts, int n_start, int n_end,
		struct psc_collision_stats *stats)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  int nn = n_end - n_start;
  
  int n = 0;
  if (nn < 2) { // can't collide only one (or zero) particles
    return;
  }

  // all particles need to have same weight!
  real_t wni = particle_wni(&prts[n_start]);
  real_t nudt1 = wni / ppsc->prm.nicell * nn * coll->every * ppsc->dt * coll->nu;

  real_t *nudts = (real_t *) malloc((nn / 2 + 2) * sizeof(*nudts));
  int cnt = 0;

  if (nn % 2 == 1) { // odd # of particles: do 3-collision
    nudts[cnt++] = bc(prts, .5 * nudt1, n_start    , n_start + 1);
    nudts[cnt++] = bc(prts, .5 * nudt1, n_start    , n_start + 2);
    nudts[cnt++] = bc(prts, .5 * nudt1, n_start + 1, n_start + 2);
    n = 3;
  }
  for (; n < nn;  n += 2) { // do remaining particles as pair
    nudts[cnt++] = bc(prts, nudt1, n_start + n, n_start + n + 1);
  }

  calc_stats(stats, nudts, cnt);
  free(nudts);
}

// ----------------------------------------------------------------------
// psc_collision_sub_setup

static void
psc_collision_sub_setup(struct psc_collision *collision)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  coll->mflds = psc_mfields_create(psc_collision_comm(collision));
  psc_mfields_set_type(coll->mflds, FIELDS_TYPE);
  psc_mfields_set_param_obj(coll->mflds, "domain", ppsc->mrc_domain);
  psc_mfields_set_param_int(coll->mflds, "nr_fields", NR_STATS);
  psc_mfields_set_param_int3(coll->mflds, "ibn", ppsc->ibn);
  psc_mfields_setup(coll->mflds);
  psc_mfields_set_comp_name(coll->mflds, 0, "coll_nudt_min");
  psc_mfields_set_comp_name(coll->mflds, 1, "coll_nudt_med");
  psc_mfields_set_comp_name(coll->mflds, 2, "coll_nudt_max");
  psc_mfields_set_comp_name(coll->mflds, 3, "coll_nudt_nlarge");
  psc_mfields_set_comp_name(coll->mflds, 4, "coll_nudt_ncoll");
  psc_mfields_list_add(&psc_mfields_base_list, &coll->mflds);

  coll->mflds_rei = psc_mfields_create(psc_collision_comm(collision));
  psc_mfields_set_type(coll->mflds_rei, FIELDS_TYPE);
  psc_mfields_set_param_obj(coll->mflds_rei, "domain", ppsc->mrc_domain);
  psc_mfields_set_param_int(coll->mflds_rei, "nr_fields", 3);
  psc_mfields_set_param_int3(coll->mflds_rei, "ibn", ppsc->ibn);
  psc_mfields_setup(coll->mflds_rei);
  psc_mfields_set_comp_name(coll->mflds_rei, 0, "coll_rei_x");
  psc_mfields_set_comp_name(coll->mflds_rei, 1, "coll_rei_y");
  psc_mfields_set_comp_name(coll->mflds_rei, 2, "coll_rei_z");
  psc_mfields_list_add(&psc_mfields_base_list, &coll->mflds_rei);
}

// ----------------------------------------------------------------------
// psc_collision_sub_destroy

static void
psc_collision_sub_destroy(struct psc_collision *collision)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  psc_mfields_destroy(coll->mflds);
  psc_mfields_destroy(coll->mflds_rei);
}

// ----------------------------------------------------------------------
// psc_collision_sub_run

static void
psc_collision_sub_run(struct psc_collision *collision,
		      struct psc_mparticles *mprts_base)
{
  struct psc_collision_sub *coll = psc_collision_sub(collision);

  static int pr;
  if (!pr) {
    pr = prof_register("collision", 1., 0, 0);
  }

  assert(coll->nu > 0.);

  if (ppsc->timestep % coll->every != 0) {
    return;
  }

  mparticles_t mprts = mprts_base->get_as<mparticles_t>();

  prof_start(pr);

  mfields_t mf_coll(coll->mflds);
  for (int p = 0; p < mprts.n_patches(); p++) {
    mparticles_t::patch_t& prts = mprts[p];
  
    int *ldims = ppsc->patch[p].ldims;
    int nr_cells = ldims[0] * ldims[1] * ldims[2];
    int *offsets = (int *) calloc(nr_cells + 1, sizeof(*offsets));
    struct psc_collision_stats stats_total = {};
    
    find_cell_offsets(offsets, mprts, p);
    
    Fields F(mf_coll[p]);
    psc_foreach_3d(ppsc, p, ix, iy, iz, 0, 0) {
      int c = (iz * ldims[1] + iy) * ldims[0] + ix;
      randomize_in_cell(prts, offsets[c], offsets[c+1]);

      update_rei_before(collision, prts, offsets[c], offsets[c+1], p, ix,iy,iz);
      
      struct psc_collision_stats stats = {};
      collide_in_cell(collision, prts, offsets[c], offsets[c+1], &stats);
      
      update_rei_after(collision, prts, offsets[c], offsets[c+1], p, ix,iy,iz);

      for (int s = 0; s < NR_STATS; s++) {
	F(s, ix,iy,iz) = stats.s[s];
	stats_total.s[s] += stats.s[s];
      }
    } psc_foreach_3d_end;
    
#if 0
    mprintf("p%d: min %g med %g max %g nlarge %g ncoll %g\n", p,
	    stats_total.s[STATS_MIN] / nr_cells,
	    stats_total.s[STATS_MED] / nr_cells,
	    stats_total.s[STATS_MAX] / nr_cells,
	    stats_total.s[STATS_NLARGE] / nr_cells,
	    stats_total.s[STATS_NCOLL] / nr_cells);
#endif
  
    free(offsets);
  }
    
  prof_stop(pr);
    
  mprts.put_as(mprts_base);
}

// ======================================================================
// psc_collision: subclass "c" / "single"

struct psc_collision_ops_ : psc_collision_ops {
  psc_collision_ops_() {
    name                  = PARTICLE_TYPE;
    size                  = sizeof(struct psc_collision_sub);
    param_descr           = psc_collision_sub_descr;
    setup                 = psc_collision_sub_setup;
    destroy               = psc_collision_sub_destroy;
    run                   = psc_collision_sub_run;
  }
} psc_collision_sub_ops;

// ======================================================================

static void
copy_stats(struct psc_output_fields_item *item, struct psc_mfields *mflds_base,
	   struct psc_mparticles *mprts_base, struct psc_mfields *mres)
{
  struct psc_collision *collision = ppsc->collision;
  assert(psc_collision_ops(collision) == &psc_collision_single_ops);

  struct psc_collision_sub *coll = psc_collision_sub(collision);

  mfields_t mr = mres->get_as<mfields_t>(0, 0);

  for (int m = 0; m < NR_STATS; m++) {
    // FIXME, copy could be avoided (?)
    mr.mflds()->copy(m, coll->mflds, m);
  }

  mr.put_as(mres, 0, NR_STATS);
}

// ======================================================================
// psc_output_fields_item: subclass "coll_stats"

struct psc_output_fields_item_ops_coll : psc_output_fields_item_ops {
  psc_output_fields_item_ops_coll() {
    name      = "coll_stats_" FIELDS_TYPE;
    nr_comp   = NR_STATS;
    fld_names[0] = "coll_nudt_min";
    fld_names[1] = "coll_nudt_med";
    fld_names[2] = "coll_nudt_max";
    fld_names[3] = "coll_nudt_large";
    fld_names[4] = "coll_ncoll";
    run_all   = copy_stats;
  }
} psc_output_fields_item_coll_stats_ops;

// ======================================================================

static void
copy_rei(struct psc_output_fields_item *item, struct psc_mfields *mflds_base,
	 struct psc_mparticles *mprts_base, struct psc_mfields *mres)
{
  struct psc_collision *collision = ppsc->collision;
  assert(psc_collision_ops(collision) == &psc_collision_single_ops);

  struct psc_collision_sub *coll = psc_collision_sub(collision);

  mfields_t mr = mres->get_as<mfields_t>(0, 0);

  for (int m = 0; m < 3; m++) {
    // FIXME, copy could be avoided (?)
    mr.mflds()->copy(m, coll->mflds_rei, m);
  }

  mr.put_as(mres, 0, 3);
}

// ======================================================================
// psc_output_fields_item: subclass "coll_rei"

struct psc_output_fields_item_ops_coll_rei : psc_output_fields_item_ops {
  psc_output_fields_item_ops_coll_rei() {
    name      = "coll_rei_" FIELDS_TYPE;
    nr_comp   = 3;
    fld_names[0] = "coll_rei_x";
    fld_names[1] = "coll_rei_y";
    fld_names[2] = "coll_rei_z";
    run_all   = copy_rei;
  }
} psc_output_fields_item_coll_rei_ops;

