
#define ORDER_1ST 1
#define ORDER_2ND 2

#define VARIANT_SFF 1

#define PRTS_STAGGERED 1

#define CACHE_EM_J 1

#define DIM_X 1
#define DIM_Y 2
#define DIM_Z 4
#define DIM_XY (DIM_X | DIM_Y)
#define DIM_XZ (DIM_X | DIM_Z)
#define DIM_YZ (DIM_Y | DIM_Z)
#define DIM_XYZ (DIM_X | DIM_Y | DIM_Z)

// ----------------------------------------------------------------------

#if (DIM & DIM_X)
#define IF_DIM_X(s) s do{} while(0)
#define IF_NOT_DIM_X(s) do{} while(0)
#else
#define IF_DIM_X(s) do{} while(0)
#define IF_NOT_DIM_X(s) s do{} while(0)
#endif

#if (DIM & DIM_Y)
#define IF_DIM_Y(s) s do{} while(0)
#define IF_NOT_DIM_Y(s) do{} while(0)
#else
#define IF_DIM_Y(s) do{} while(0)
#define IF_NOT_DIM_Y(s) s do{} while(0)
#endif

#if (DIM & DIM_Z)
#define IF_DIM_Z(s) s do{} while(0)
#define IF_NOT_DIM_Z(s) do{} while(0)
#else
#define IF_DIM_Z(s) do{} while(0)
#define IF_NOT_DIM_Z(s) s do{} while(0)
#endif

// ----------------------------------------------------------------------
// static vars

static particle_real_t xl, yl, zl;

#if ORDER == ORDER_1ST

#if DIM == DIM_XZ
#if VARIANT == VARIANT_SFF
#define psc_push_particles_push_mprts psc_push_particles_1sff_push_mprts_xz
#define do_push_part do_push_part_1sff_xz
#define PROF_NAME "push_mprts_1sff_xz"
#else
#define psc_push_particles_push_mprts psc_push_particles_1st_push_mprts_xz
#define do_push_part do_push_part_1st_xz
#define PROF_NAME "push_mprts_1st_xz"
#endif
#elif DIM == DIM_YZ
#define psc_push_particles_push_mprts psc_push_particles_1st_push_mprts_yz
#define do_push_part do_push_part_1st_yz
#define PROF_NAME "push_mprts_1st_yz"
#endif

#elif ORDER == ORDER_2ND

#if DIM == DIM_Y
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_y
#define do_push_part do_push_part_genc_y
#define PROF_NAME "genc_push_mprts_y"
#elif DIM == DIM_Z
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_z
#define do_push_part do_push_part_genc_z
#define PROF_NAME "genc_push_mprts_z"
#elif DIM == DIM_XY
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_xy
#define do_push_part do_push_part_genc_xy
#define PROF_NAME "genc_push_mprts_xy"
#elif DIM == DIM_XZ
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_xz
#define do_push_part do_push_part_genc_xz
#define PROF_NAME "genc_push_mprts_xz"
#elif DIM == DIM_XYZ
#define psc_push_particles_push_mprts psc_push_particles_generic_c_push_mprts_xyz
#define do_push_part do_push_part_genc_xyz
#define PROF_NAME "genc_push_mprts_xyz"
#endif

#endif

// ----------------------------------------------------------------------
// calc_v

static inline void
calc_v(particle_real_t *v, const particle_real_t *p)
{
  particle_real_t root = 1.f / particle_real_sqrt(1.f + sqr(p[0]) + sqr(p[1]) + sqr(p[2]));
  for (int d = 0; d < 3; d++) {
    v[d] = p[d] * root;
  }
}

// ----------------------------------------------------------------------
// push_x

static inline void
push_x(particle_real_t *x, const particle_real_t *v)
{
  IF_DIM_X( x[0] += v[0] * xl; );
  IF_DIM_Y( x[1] += v[1] * yl; );
  IF_DIM_Z( x[2] += v[2] * zl; );
}

// ----------------------------------------------------------------------
// push_p

static inline void
push_p(particle_real_t *p, particle_real_t *E, particle_real_t *H, particle_real_t dq)
{
  particle_real_t pxm = p[0] + dq * E[0];
  particle_real_t pym = p[1] + dq * E[1];
  particle_real_t pzm = p[2] + dq * E[2];

  particle_real_t root = dq / particle_real_sqrt(1.f + pxm*pxm + pym*pym + pzm*pzm);
  particle_real_t taux = H[0] * root;
  particle_real_t tauy = H[1] * root;
  particle_real_t tauz = H[2] * root;
  
  particle_real_t tau = 1.f / (1.f + taux*taux + tauy*tauy + tauz*tauz);
  particle_real_t pxp = ((1.f+taux*taux-tauy*tauy-tauz*tauz)*pxm + 
			 (2.f*taux*tauy+2.f*tauz)*pym + 
			 (2.f*taux*tauz-2.f*tauy)*pzm)*tau;
  particle_real_t pyp = ((2.f*taux*tauy-2.f*tauz)*pxm +
			 (1.f-taux*taux+tauy*tauy-tauz*tauz)*pym +
			 (2.f*tauy*tauz+2.f*taux)*pzm)*tau;
  particle_real_t pzp = ((2.f*taux*tauz+2.f*tauy)*pxm +
			 (2.f*tauy*tauz-2.f*taux)*pym +
			 (1.f-taux*taux-tauy*tauy+tauz*tauz)*pzm)*tau;
  
  p[0] = pxp + dq * E[0];
  p[1] = pyp + dq * E[1];
  p[2] = pzp + dq * E[2];
}

// ----------------------------------------------------------------------
// charge density 

#if ORDER == ORDER_1ST

#define N_RHO 4
#define S_OFF 1

#elif ORDER == ORDER_2ND

#define N_RHO 5
#define S_OFF 2

#endif

#define S(s, off) s[off + S_OFF]

// ----------------------------------------------------------------------
// interpolation

#if ORDER == ORDER_1ST

#if DIM == DIM_XZ
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.v0*(gx##x.v0*_F3(flds, m, l##gx##1  ,0,l##gz##3  ) +		\
	     gx##x.v1*_F3(flds, m, l##gx##1+1,0,l##gz##3  )) +		\
   gz##z.v1*(gx##x.v0*_F3(flds, m, l##gx##1  ,0,l##gz##3+1) +		\
	     gx##x.v1*_F3(flds, m, l##gx##1+1,0,l##gz##3+1)))
#elif DIM == DIM_YZ
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.v0*(gy##y.v0*_F3(flds, m, 0,l##gy##2  ,l##gz##3  ) +		\
	     gy##y.v1*_F3(flds, m, 0,l##gy##2+1,l##gz##3  )) +		\
   gz##z.v1*(gy##y.v0*_F3(flds, m, 0,l##gy##2  ,l##gz##3+1) +		\
	     gy##y.v1*_F3(flds, m, 0,l##gy##2+1,l##gz##3+1)))
#endif

#else

#if DIM == DIM_Y
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gy##y.vm*_F3(flds, m, 0,l##gy##2-1,0) +				\
   gy##y.v0*_F3(flds, m, 0,l##gy##2  ,0) +				\
   gy##y.vp*_F3(flds, m, 0,l##gy##2+1,0))
#elif DIM == DIM_Z
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.vm*_F3(flds, m, 0,0,l##gz##3-1) +				\
   gz##z.v0*_F3(flds, m, 0,0,l##gz##3  ) +				\
   gz##z.vp*_F3(flds, m, 0,0,l##gz##3+1))
#elif DIM == DIM_XY
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gy##y.vm*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2-1,0) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2-1,0) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2-1,0)) +		\
   gy##y.v0*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2  ,0) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2  ,0) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2  ,0)) +		\
   gy##y.vp*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2+1,0) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2+1,0) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2+1,0)))
#elif DIM == DIM_XZ
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.vm*(gx##x.vm*_F3(flds, m, l##gx##1-1,0,l##gz##3-1) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,0,l##gz##3-1) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,0,l##gz##3-1)) +		\
   gz##z.v0*(gx##x.vm*_F3(flds, m, l##gx##1-1,0,l##gz##3  ) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,0,l##gz##3  ) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,0,l##gz##3  )) +		\
   gz##z.vp*(gx##x.vm*_F3(flds, m, l##gx##1-1,0,l##gz##3+1) +		\
	     gx##x.v0*_F3(flds, m, l##gx##1  ,0,l##gz##3+1) +		\
	     gx##x.vp*_F3(flds, m, l##gx##1+1,0,l##gz##3+1)))
#elif DIM == DIM_YZ
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.vm*(gy##y.vm*_F3(flds, m, 0,l##gy##2-1,l##gz##3-1) +		\
	     gy##y.v0*_F3(flds, m, 0,l##gy##2  ,l##gz##3-1) +		\
	     gy##y.vp*_F3(flds, m, 0,l##gy##2+1,l##gz##3-1)) +		\
   gz##z.v0*(gy##y.vm*_F3(flds, m, 0,l##gy##2-1,l##gz##3  ) +		\
	     gy##y.v0*_F3(flds, m, 0,l##gy##2  ,l##gz##3  ) +		\
	     gy##y.vp*_F3(flds, m, 0,l##gy##2+1,l##gz##3  )) +		\
   gz##z.vp*(gy##y.vm*_F3(flds, m, 0,l##gy##2-1,l##gz##3+1) +		\
	     gy##y.v0*_F3(flds, m, 0,l##gy##2  ,l##gz##3+1) +		\
	     gy##y.vp*_F3(flds, m, 0,l##gy##2+1,l##gz##3+1)))
#elif DIM == DIM_XYZ
#define IP_FIELD(flds, m, gx, gy, gz)					\
  (gz##z.vm*(gy##y.vm*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2-1,l##gz##3-1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2-1,l##gz##3-1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2-1,l##gz##3-1)) + \
	     gy##y.v0*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2  ,l##gz##3-1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2  ,l##gz##3-1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2  ,l##gz##3-1)) + \
	     gy##y.vp*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2+1,l##gz##3-1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2+1,l##gz##3-1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2+1,l##gz##3-1))) + \
   gz##z.v0*(gy##y.vm*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2-1,l##gz##3  ) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2-1,l##gz##3  ) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2-1,l##gz##3  )) + \
	     gy##y.v0*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2  ,l##gz##3  ) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2  ,l##gz##3  ) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2  ,l##gz##3  )) + \
	     gy##y.vp*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2+1,l##gz##3  ) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2+1,l##gz##3  ) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2+1,l##gz##3  ))) + \
   gz##z.vp*(gy##y.vm*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2-1,l##gz##3+1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2-1,l##gz##3+1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2-1,l##gz##3+1)) + \
	     gy##y.v0*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2  ,l##gz##3+1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2  ,l##gz##3+1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2  ,l##gz##3+1)) + \
	     gy##y.vp*(gx##x.vm*_F3(flds, m, l##gx##1-1,l##gy##2+1,l##gz##3+1) + \
		       gx##x.v0*_F3(flds, m, l##gx##1  ,l##gy##2+1,l##gz##3+1) + \
		       gx##x.vp*_F3(flds, m, l##gx##1+1,l##gy##2+1,l##gz##3+1))))

#endif

#endif // ORDER

struct ip_coeff {
#if ORDER == ORDER_1ST
  particle_real_t v0, v1;
#elif ORDER == ORDER_2ND
  particle_real_t vm, v0, vp, h;
#endif
};

// ----------------------------------------------------------------------
// get_nint_remainder

static inline void
get_nint_remainder(int *lg1, particle_real_t *h1, particle_real_t u)
{
  int l = particle_real_nint(u);
  *lg1 = l;
  *h1 = l-u;
}

// ----------------------------------------------------------------------
// get_fint_remainder

static inline void
get_fint_remainder(int *lg1, particle_real_t *h1, particle_real_t u)
{
  int l = particle_real_fint(u);
  *lg1 = l;
  *h1 = u-l;
}

// ----------------------------------------------------------------------
// ip_coeff

static inline void
ip_coeff(int *lg, struct ip_coeff *gg, particle_real_t u)
{
  int l;
  particle_real_t h;

#if ORDER == ORDER_1ST
  get_fint_remainder(&l, &h, u);
  gg->v0 = 1.f - h;
  gg->v1 = h;
#elif ORDER == ORDER_2ND
  get_nint_remainder(&l, &h, u);
  gg->h  = h;
  gg->vm = .5f * (.5f+h)*(.5f+h);
  gg->v0 = .75f - h*h;
  gg->vp = .5f * (.5f-h)*(.5f-h);
#endif
  *lg = l;
}

// ----------------------------------------------------------------------
// ZERO_S1

#define ZERO_S1 do {				\
    for (int i = -S_OFF; i < -S_OFF + N_RHO; i++) {	\
      IF_DIM_X( S(s1x, i) = 0.f; );		\
      IF_DIM_Y( S(s1y, i) = 0.f; );		\
      IF_DIM_Z( S(s1z, i) = 0.f; );		\
    }						\
  } while (0)

// ----------------------------------------------------------------------
// SUBTR_S1_S0

#define SUBTR_S1_S0 do {			\
    for (int i = -S_OFF + 1; i <= 1; i++) {	\
      IF_DIM_X( S(s1x, i) -= S(s0x, i); );	\
      IF_DIM_Y( S(s1y, i) -= S(s0y, i); );	\
      IF_DIM_Z( S(s1z, i) -= S(s0z, i); );	\
    }						\
  } while (0)

// ----------------------------------------------------------------------
// set_S

static inline void
set_S(particle_real_t *s0, int shift, struct ip_coeff gg)
{
#if ORDER == ORDER_1ST
  S(s0, shift  ) = gg.v0;
  S(s0, shift+1) = gg.v1;
#elif ORDER == ORDER_2ND
  // FIXME: It appears that gm/g0/g1 can be used instead of what's calculated here
  // but it needs checking.
  particle_real_t h = gg.h;
  S(s0, shift-1) = .5f*(1.5f-particle_real_abs(h-1.f))*(1.5f-particle_real_abs(h-1.f));
  S(s0, shift  ) = .75f-particle_real_abs(h)*particle_real_abs(h);
  S(s0, shift+1) = .5f*(1.5f-particle_real_abs(h+1.f))*(1.5f-particle_real_abs(h+1.f));
#endif
}

// ----------------------------------------------------------------------
// find_l_minmax

static inline void
find_l_minmax(int *l1min, int *l1max, int k1, int lg1)
{
#if ORDER == ORDER_1ST
  if (k1 == lg1) {
    *l1min = 0; *l1max = +1;
  } else if (k1 == lg1 - 1) {
    *l1min = -1; *l1max = +1;
  } else { // (k1 == lg1 + 1)
    *l1min = 0; *l1max = +2;
  }
#elif ORDER == ORDER_2ND
  if (k1 == lg1) {
    *l1min = -1; *l1max = +1;
  } else if (k1 == lg1 - 1) {
    *l1min = -2; *l1max = +1;
  } else { // (k1 == lg1 + 1)
    *l1min = -1; *l1max = +2;
  }
#endif
}

#define DEPOSIT_AND_IP_COEFFS(lg1, lh1, gx, hx, d, dxi, s0x)	\
  int lg1, lh1;							\
  struct ip_coeff gx, hx;					\
  ip_coeff(&lg1, &gx, x[d] * dxi);				\
  set_S(s0x, 0, gx);						\
  ip_coeff(&lh1, &hx, x[d] * dxi - .5f)
  
#define DEPOSIT(xx, k1, gx, d, dxi, s1x, lg1)		\
    int k1;						\
    ip_coeff(&k1, &gx, xx[d] * dxi);			\
    set_S(s1x, k1-lg1, gx)
    

// ======================================================================
// VARIANT SFF

#if VARIANT == VARIANT_SFF

// FIXME, calculation of f_avg could be done at the level where we do caching, too
// (if that survives, anyway...)

#define VARIANT_SFF_PREP					\
  struct psc_patch *patch = &ppsc->patch[p];			\
  								\
  /* FIXME, eventually no ghost points should be needed (?) */	\
  fields_t flds_avg = fields_t_ctor((int[3]) { -1, 0, -1 },		\
				    (int[3]) { patch->ldims[0] + 2, 1, patch->ldims[2] + 1 },\
				    6);					\
									\
  for (int iz = -1; iz < patch->ldims[2] + 1; iz++) {			\
    for (int ix = -1; ix < patch->ldims[0] + 1; ix++) {			\
      _F3(flds_avg, 0, ix,0,iz) = .5 * (_F3(flds, EX, ix,0,iz) + _F3(flds, EX, ix-1,0,iz)); \
      _F3(flds_avg, 1, ix,0,iz) = _F3(flds, EY, ix,0,iz);		\
      _F3(flds_avg, 2, ix,0,iz) = .5 * (_F3(flds, EZ, ix,0,iz) + _F3(flds, EZ, ix,0,iz-1)); \
      _F3(flds_avg, 3, ix,0,iz) = .5 * (_F3(flds, HX, ix,0,iz) + _F3(flds, HX, ix,0,iz-1)); \
      _F3(flds_avg, 4, ix,0,iz) = .25 * (_F3(flds, HY, ix  ,0,iz) + _F3(flds, HY, ix  ,0,iz-1) + \
					 _F3(flds, HY, ix-1,0,iz) + _F3(flds, HY, ix-1,0,iz-1)); \
      _F3(flds_avg, 5, ix,0,iz) = .5 * (_F3(flds, HZ, ix,0,iz) + _F3(flds, HZ, ix-1,0,iz)); \
    }									\
  }

#define VARIANT_SFF_POST			\
  fields_t_dtor(&flds_avg)

#define INTERPOLATE_FIELDS						\
  /* FIXME, we don't really need h coeffs in this case, either, though	\
   * the compiler may be smart enough to figure that out */		\
  particle_real_t E[3] = { IP_FIELD(flds_avg, EX-EX, g, g, g),		\
			   IP_FIELD(flds_avg, EY-EX, g, g, g),		\
			   IP_FIELD(flds_avg, EZ-EX, g, g, g), };	\
  particle_real_t H[3] = { IP_FIELD(flds_avg, HX-EX, g, g, g),		\
			   IP_FIELD(flds_avg, HY-EX, g, g, g),		\
			   IP_FIELD(flds_avg, HZ-EX, g, g, g), }
#else

#define VARIANT_SFF_PREP do {} while (0)
#define VARIANT_SFF_POST do {} while (0)
#define INTERPOLATE_FIELDS						\
  particle_real_t E[3] = { IP_FIELD(flds, EX, h, g, g),			\
			   IP_FIELD(flds, EY, g, h, g),			\
			   IP_FIELD(flds, EZ, g, g, h), };		\
  particle_real_t H[3] = { IP_FIELD(flds, HX, g, h, h),			\
			   IP_FIELD(flds, HY, h, g, h),			\
			   IP_FIELD(flds, HZ, h, h, g), }

#endif

// ======================================================================
// current

#define CURRENT_PREP_DIM(l1min, l1max, k1, lg1, fnqx, fnqxs)	\
    int l1min, l1max; find_l_minmax(&l1min, &l1max, k1, lg1);	\
    particle_real_t fnqx = particle_qni_wni(part) * fnqxs;	\

#define CURRENT_PREP							\
  IF_DIM_X( CURRENT_PREP_DIM(l1min, l1max, k1, lg1, fnqx, fnqxs); );	\
  IF_DIM_Y( CURRENT_PREP_DIM(l2min, l2max, k2, lg2, fnqy, fnqys); );	\
  IF_DIM_Z( CURRENT_PREP_DIM(l3min, l3max, k3, lg3, fnqz, fnqzs); );	\
									\
  IF_NOT_DIM_X( particle_real_t fnqxx = vv[0] * particle_qni_wni(part) * fnqs; ); \
  IF_NOT_DIM_Y( particle_real_t fnqyy = vv[1] * particle_qni_wni(part) * fnqs; ); \
  IF_NOT_DIM_Z( particle_real_t fnqzz = vv[2] * particle_qni_wni(part) * fnqs; )

#define CURRENT_2ND_Y						\
  particle_real_t jyh = 0.f;					\
								\
  for (int l2 = l2min; l2 <= l2max; l2++) {			\
    particle_real_t wx = S(s0y, l2) + .5f * S(s1y, l2);		\
    particle_real_t wy = S(s1y, l2);				\
    particle_real_t wz = S(s0y, l2) + .5f * S(s1y, l2);		\
    								\
    particle_real_t jxh = fnqxx*wx;				\
    jyh -= fnqy*wy;						\
    particle_real_t jzh = fnqzz*wz;				\
    								\
    _F3(flds, JXI, 0,lg2+l2,0) += jxh;				\
    _F3(flds, JYI, 0,lg2+l2,0) += jyh;				\
    _F3(flds, JZI, 0,lg2+l2,0) += jzh;				\
  }

#define CURRENT_2ND_Z						\
  particle_real_t jzh = 0.f;					\
  for (int l3 = l3min; l3 <= l3max; l3++) {			\
    particle_real_t wx = S(s0z, l3) + .5f * S(s1z, l3);		\
    particle_real_t wy = S(s0z, l3) + .5f * S(s1z, l3);		\
    particle_real_t wz = S(s1z, l3);				\
    								\
    particle_real_t jxh = fnqxx*wx;				\
    particle_real_t jyh = fnqyy*wy;				\
    jzh -= fnqz*wz;						\
    								\
    _F3(flds, JXI, 0,0,lg3+l3) += jxh;				\
    _F3(flds, JYI, 0,0,lg3+l3) += jyh;				\
    _F3(flds, JZI, 0,0,lg3+l3) += jzh;				\
  }

#define CURRENT_2ND_XY							\
  for (int l2 = l2min; l2 <= l2max; l2++) {				\
    particle_real_t jxh = 0.f;						\
    for (int l1 = l1min; l1 <= l1max; l1++) {				\
      particle_real_t wx = S(s1x, l1) * (S(s0y, l2) + .5f*S(s1y, l2));	\
      particle_real_t wz = S(s0x, l1) * S(s0y, l2)			\
	+ .5f * S(s1x, l1) * S(s0y, l2)					\
	+ .5f * S(s0x, l1) * S(s1y, l2)					\
	+ (1.f/3.f) * S(s1x, l1) * S(s1y, l2);				\
      									\
      jxh -= fnqx*wx;							\
      _F3(flds, JXI, lg1+l1,lg2+l2,0) += jxh;				\
      _F3(flds, JZI, lg1+l1,lg2+l2,0) += fnqzz * wz;			\
    }									\
  }									\
  for (int l1 = l1min; l1 <= l1max; l1++) {				\
    particle_real_t jyh = 0.f;						\
    for (int l2 = l2min; l2 <= l2max; l2++) {				\
      particle_real_t wy = S(s1y, l2) * (S(s0x, l1) + .5f*S(s1x, l1));	\
      									\
      jyh -= fnqy*wy;							\
      _F3(flds, JYI, lg1+l1,lg2+l2,0) += jyh;				\
    }									\
  }

#define CURRENT_XZ				\
  for (int l3 = l3min; l3 <= l3max; l3++) {	\
    particle_real_t jxh = 0.f;						\
    for (int l1 = l1min; l1 < l1max; l1++) {				\
      particle_real_t wx = S(s1x, l1) * (S(s0z, l3) + .5f*S(s1z, l3));	\
      jxh -= fnqx*wx;							\
      _F3(flds, JXI, lg1+l1,0,lg3+l3) += jxh;				\
    }									\
  }									\
									\
  for (int l3 = l3min; l3 <= l3max; l3++) {				\
    for (int l1 = l1min; l1 <= l1max; l1++) {				\
      particle_real_t wy = S(s0x, l1) * S(s0z, l3)			\
	+ .5f * S(s1x, l1) * S(s0z, l3)					\
	+ .5f * S(s0x, l1) * S(s1z, l3)					\
	+ (1.f/3.f) * S(s1x, l1) * S(s1z, l3);				\
      particle_real_t jyh = fnqyy * wy;					\
      _F3(flds, JYI, lg1+l1,0,lg3+l3) += jyh;				\
    }									\
  }									\
  for (int l1 = l1min; l1 <= l1max; l1++) {				\
    particle_real_t jzh = 0.f;						\
    for (int l3 = l3min; l3 < l3max; l3++) {				\
      particle_real_t wz = S(s1z, l3) * (S(s0x, l1) + .5f*S(s1x, l1));	\
      jzh -= fnqz*wz;							\
      _F3(flds, JZI, lg1+l1,0,lg3+l3) += jzh;				\
    }									\
  }

#define CURRENT_1ST_YZ							\
  for (int l3 = l3min; l3 <= l3max; l3++) {				\
    for (int l2 = l2min; l2 <= l2max; l2++) {				\
      particle_real_t wx = S(s0y, l2) * S(s0z, l3)			\
	+ .5f * S(s1y, l2) * S(s0z, l3)					\
	+ .5f * S(s0y, l2) * S(s1z, l3)					\
	+ (1.f/3.f) * S(s1y, l2) * S(s1z, l3);				\
      particle_real_t jxh = fnqxx * wx;					\
      _F3(flds, JXI, 0,lg2+l2,lg3+l3) += jxh;				\
    }									\
  }									\
  									\
  for (int l3 = l3min; l3 <= l3max; l3++) {				\
    particle_real_t jyh = 0.f;						\
    for (int l2 = l2min; l2 < l2max; l2++) {				\
      particle_real_t wy = S(s1y, l2) * (S(s0z, l3) + .5f*S(s1z, l3));	\
      jyh -= fnqy*wy;							\
      _F3(flds, JYI, 0,lg2+l2,lg3+l3) += jyh;				\
    }									\
  }									\
									\
  for (int l2 = l2min; l2 <= l2max; l2++) {				\
    particle_real_t jzh = 0.f;						\
    for (int l3 = l3min; l3 < l3max; l3++) {				\
      particle_real_t wz = S(s1z, l3) * (S(s0y, l2) + .5f*S(s1y, l2));	\
      jzh -= fnqz*wz;							\
      _F3(flds, JZI, 0,lg2+l2,lg3+l3) += jzh;				\
    }									\
  }

#define JZH(i) jzh[i+2]
#define CURRENT_2ND_YZ							\
    particle_real_t jxh;						\
    particle_real_t jyh;						\
    particle_real_t jzh[5];						\
									\
    for (int l2 = l2min; l2 <= l2max; l2++) {				\
      JZH(l2) = 0.f;							\
    }									\
    for (int l3 = l3min; l3 <= l3max; l3++) {				\
      jyh = 0.f;							\
      for (int l2 = l2min; l2 <= l2max; l2++) {				\
	particle_real_t wx = S(s0y, l2) * S(s0z, l3)			\
	  + .5f * S(s1y, l2) * S(s0z, l3)				\
	  + .5f * S(s0y, l2) * S(s1z, l3)				\
	+ (1.f/3.f) * S(s1y, l2) * S(s1z, l3);				\
	particle_real_t wy = S(s1y, l2) * (S(s0z, l3) + .5f*S(s1z, l3)); \
	particle_real_t wz = S(s1z, l3) * (S(s0y, l2) + .5f*S(s1y, l2)); \
									\
	jxh = fnqxx*wx;							\
	jyh -= fnqy*wy;							\
	JZH(l2) -= fnqz*wz;						\
									\
	_F3(flds, JXI, 0,lg2+l2,lg3+l3) += jxh;				\
	_F3(flds, JYI, 0,lg2+l2,lg3+l3) += jyh;				\
	_F3(flds, JZI, 0,lg2+l2,lg3+l3) += JZH(l2);			\
      }									\
    }									\

#define CURRENT_2ND_XYZ							\
  for (int l3 = l3min; l3 <= l3max; l3++) {				\
    for (int l2 = l2min; l2 <= l2max; l2++) {				\
      particle_real_t jxh = 0.f;					\
      for (int l1 = l1min; l1 <= l1max; l1++) {				\
	particle_real_t wx = S(s1x, l1) * (S(s0y, l2) * S(s0z, l3) +	\
					   .5f * S(s1y, l2) * S(s0z, l3) + \
					   .5f * S(s0y, l2) * S(s1z, l3) + \
					   (1.f/3.f) * S(s1y, l2) * S(s1z, l3)); \
									\
	jxh -= fnqx*wx;							\
	_F3(flds, JXI, lg1+l1,lg2+l2,lg3+l3) += jxh;			\
      }									\
    }									\
  }									\
  									\
  for (int l3 = l3min; l3 <= l3max; l3++) {				\
    for (int l1 = l1min; l1 <= l1max; l1++) {				\
      particle_real_t jyh = 0.f;					\
      for (int l2 = l2min; l2 <= l2max; l2++) {				\
	particle_real_t wy = S(s1y, l2) * (S(s0x, l1) * S(s0z, l3) +	\
					   .5f * S(s1x, l1) * S(s0z, l3) + \
					   .5f * S(s0x, l1) * S(s1z, l3) + \
					   (1.f/3.f) * S(s1x, l1)*S(s1z, l3)); \
									\
	jyh -= fnqy*wy;							\
	_F3(flds, JYI, lg1+l1,lg2+l2,lg3+l3) += jyh;			\
      }									\
    }									\
  }									\
									\
  for (int l2 = l2min; l2 <= l2max; l2++) {				\
    for (int l1 = l1min; l1 <= l1max; l1++) {				\
      particle_real_t jzh = 0.f;					\
      for (int l3 = l3min; l3 <= l3max; l3++) {				\
	particle_real_t wz = S(s1z, l3) * (S(s0x, l1) * S(s0y, l2) +	\
					   .5f * S(s1x, l1) * S(s0y, l2) +\
					   .5f * S(s0x, l1) * S(s1y, l2) +\
					   (1.f/3.f) * S(s1x, l1)*S(s1y, l2)); \
									\
	jzh -= fnqz*wz;							\
	_F3(flds, JZI, lg1+l1,lg2+l2,lg3+l3) += jzh;			\
      }									\
    }									\
  }

#if DIM == DIM_Y
#if ORDER == ORDER_2ND
#define CURRENT CURRENT_2ND_Y
#endif
#elif DIM == DIM_Z
#if ORDER == ORDER_2ND
#define CURRENT CURRENT_2ND_Z
#endif
#elif DIM == DIM_XY
#if ORDER == ORDER_2ND
#define CURRENT CURRENT_2ND_XY
#endif
#elif DIM == DIM_XZ
#define CURRENT CURRENT_XZ
#elif DIM == DIM_YZ

#if ORDER == ORDER_1ST
#define CURRENT CURRENT_1ST_YZ
#elif ORDER == ORDER_2ND
#define CURRENT CURRENT_2ND_YZ

#endif
#elif DIM == DIM_XYZ
#if ORDER == ORDER_2ND
#define CURRENT CURRENT_2ND_XYZ
#endif
#endif

#ifdef do_push_part

static void
do_push_part(int p, fields_t flds, particle_range_t prts)
{
#include "push_part_common_vars.c"

  VARIANT_SFF_PREP;
  
  PARTICLE_ITER_LOOP(prt_iter, prts.begin, prts.end) {
    particle_t *part = particle_iter_deref(prt_iter);
    particle_real_t *x = &part->xi;
    particle_real_t vv[3];

#if PRTS != PRTS_STAGGERED
    // x^n, p^n -> x^(n+.5), p^n
    calc_v(vv, &part->pxi);
    push_x(x, vv);
#endif
    
    // CHARGE DENSITY FORM FACTOR AT (n+.5)*dt 

    IF_DIM_X( DEPOSIT_AND_IP_COEFFS(lg1, lh1, gx, hx, 0, dxi, s0x); );
    IF_DIM_Y( DEPOSIT_AND_IP_COEFFS(lg2, lh2, gy, hy, 0, dyi, s0y); );
    IF_DIM_Z( DEPOSIT_AND_IP_COEFFS(lg3, lh3, gz, hz, 0, dzi, s0z); );

    // FIELD INTERPOLATION

    INTERPOLATE_FIELDS;

    // x^(n+0.5), p^n -> x^(n+0.5), p^(n+1.0) 
    particle_real_t dq = dqs * particle_qni_div_mni(part);
    push_p(&part->pxi, E, H, dq);

    // x^(n+0.5), p^(n+1.0) -> x^(n+1.0), p^(n+1.0) 
    calc_v(vv, &part->pxi);

#if PRTS == PRTS_STAGGERED
    // FIXME, inelegant way of pushing full dt
    push_x(x, vv);
    push_x(x, vv);

    // CHARGE DENSITY FORM FACTOR AT (n+1.5)*dt 
    ZERO_S1;
    IF_DIM_X( DEPOSIT(x, k1, gx, 0, dxi, s1x, lg1); );
    IF_DIM_Y( DEPOSIT(x, k2, gy, 1, dyi, s1y, lg2); );
    IF_DIM_Z( DEPOSIT(x, k3, gz, 2, dzi, s1z, lg3); );

#else
    push_x(x, vv);

    // x^(n+1), p^(n+1) -> x^(n+1.5f), p^(n+1)
    particle_real_t xn[3] = { x[0], x[1], x[2] };
    push_x(xn, vv);

    // CHARGE DENSITY FORM FACTOR AT (n+1.5)*dt 
    ZERO_S1;
    IF_DIM_X( DEPOSIT(xn, k1, gx, 0, dxi, s1x, lg1); );
    IF_DIM_Y( DEPOSIT(xn, k2, gy, 1, dyi, s1y, lg2); );
    IF_DIM_Z( DEPOSIT(xn, k3, gz, 2, dzi, s1z, lg3); );
#endif
    
    // CURRENT DENSITY AT (n+1.0)*dt

    SUBTR_S1_S0;
    CURRENT_PREP;
    CURRENT;
  }

  VARIANT_SFF_POST;
}

#endif

#if CACHE == CACHE_EM_J

#if DIM == DIM_YZ
static fields_t
cache_fields_from_em(fields_t flds)
{
  fields_t fld = fields_t_ctor(flds.ib, flds.im, 9);
  // FIXME, can do -1 .. 2? NO!, except maybe for 1st order
  // Has to be at least -2 .. +3 because of staggering
  // FIXME, get rid of caching since it's no different from the actual
  // fields...
  for (int iz = fld.ib[2]; iz < fld.ib[2] + fld.im[2]; iz++) {
    for (int iy = fld.ib[1]; iy < fld.ib[1] + fld.im[1]; iy++) {
      _F3(fld, EX, 0,iy,iz) = _F3(flds, EX, 0,iy,iz);
      _F3(fld, EY, 0,iy,iz) = _F3(flds, EY, 0,iy,iz);
      _F3(fld, EZ, 0,iy,iz) = _F3(flds, EZ, 0,iy,iz);
      _F3(fld, HX, 0,iy,iz) = _F3(flds, HX, 0,iy,iz);
      _F3(fld, HY, 0,iy,iz) = _F3(flds, HY, 0,iy,iz);
      _F3(fld, HZ, 0,iy,iz) = _F3(flds, HZ, 0,iy,iz);
    }
  }
  return fld;
}

static void
cache_fields_to_j(fields_t fld, fields_t flds)
{
  for (int iz = fld.ib[2]; iz < fld.ib[2] + fld.im[2]; iz++) {
    for (int iy = fld.ib[1]; iy < fld.ib[1] + fld.im[1]; iy++) {
      _F3(flds, JXI, 0,iy,iz) += _F3(fld, JXI, 0,iy,iz);
      _F3(flds, JYI, 0,iy,iz) += _F3(fld, JYI, 0,iy,iz);
      _F3(flds, JZI, 0,iy,iz) += _F3(fld, JZI, 0,iy,iz);
    }
  }
}
#endif

#endif

#ifdef psc_push_particles_push_mprts

// ----------------------------------------------------------------------
// psc_push_particles_push_mprts

void
psc_push_particles_push_mprts(struct psc_push_particles *push,
			      struct psc_mparticles *mprts,
			      struct psc_mfields *mflds)
{
  static int pr;
  if (!pr) {
    pr = prof_register(PROF_NAME, 1., 0, 0);
  }
  
  prof_start(pr);
  for (int p = 0; p < mprts->nr_patches; p++) {
    fields_t flds = fields_t_mflds(mflds, p);
    particle_range_t prts = particle_range_mprts(mprts, p);
#if CACHE == CACHE_EM_J
    // FIXME, can't we just skip this and just set j when copying back?
    fields_t_zero_range(flds, JXI, JXI + 3);
    fields_t flds_cache = cache_fields_from_em(flds);
    do_push_part(p, flds_cache, prts);
    cache_fields_to_j(flds_cache, flds);
    fields_t_dtor(&flds_cache);
#else
    fields_t_zero_range(flds, JXI, JXI + 3);
    do_push_part(p, flds, prts);
#endif
  }
  prof_stop(pr);
}

#endif
