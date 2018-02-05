
#include "psc_debug.h"

#include "interpolate.hxx"

// ======================================================================
// InterpolateEM_Helper: 2nd std

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_y

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_y>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gy.vm*EM(m, 0,gy.l-1,0) +
	    gy.v0*EM(m, 0,gy.l  ,0) +
	    gy.vp*EM(m, 0,gy.l+1,0));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_z

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_z>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gz.vm*EM(m, 0,0,gz.l-1) +
	    gz.v0*EM(m, 0,0,gz.l  ) +
	    gz.vp*EM(m, 0,0,gz.l+1));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_xy

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_xy>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gy.vm*(gx.vm*EM(m, gx.l-1,gy.l-1,0) +
		   gx.v0*EM(m, gx.l  ,gy.l-1,0) +
		   gx.vp*EM(m, gx.l+1,gy.l-1,0)) +
	    gy.v0*(gx.vm*EM(m, gx.l-1,gy.l  ,0) +
		   gx.v0*EM(m, gx.l  ,gy.l  ,0) +
		   gx.vp*EM(m, gx.l+1,gy.l  ,0)) +
	    gy.vp*(gx.vm*EM(m, gx.l-1,gy.l+1,0) +
		   gx.v0*EM(m, gx.l  ,gy.l+1,0) +
		   gx.vp*EM(m, gx.l+1,gy.l+1,0)));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_xz

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_xz>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gz.vm*(gx.vm*EM(m, gx.l-1,0,gz.l-1) +
		   gx.v0*EM(m, gx.l  ,0,gz.l-1) +
		   gx.vp*EM(m, gx.l+1,0,gz.l-1)) +
	    gz.v0*(gx.vm*EM(m, gx.l-1,0,gz.l  ) +
		   gx.v0*EM(m, gx.l  ,0,gz.l  ) +
		   gx.vp*EM(m, gx.l+1,0,gz.l  )) +
	    gz.vp*(gx.vm*EM(m, gx.l-1,0,gz.l+1) +
		   gx.v0*EM(m, gx.l  ,0,gz.l+1) +
		   gx.vp*EM(m, gx.l+1,0,gz.l+1)));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_yz

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_yz>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gz.vm*(gy.vm*EM(m, 0,gy.l-1,gz.l-1) +
		   gy.v0*EM(m, 0,gy.l  ,gz.l-1) +
		   gy.vp*EM(m, 0,gy.l+1,gz.l-1)) +
	    gz.v0*(gy.vm*EM(m, 0,gy.l-1,gz.l  ) +
		   gy.v0*EM(m, 0,gy.l  ,gz.l  ) +
		   gy.vp*EM(m, 0,gy.l+1,gz.l  )) +
	    gz.vp*(gy.vm*EM(m, 0,gy.l-1,gz.l+1) +
		   gy.v0*EM(m, 0,gy.l  ,gz.l+1) +
		   gy.vp*EM(m, 0,gy.l+1,gz.l+1)));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ----------------------------------------------------------------------
// InterpolateEM_Helper: 2nd std, dim_xyz

template<typename F, typename IP>
struct InterpolateEM_Helper<F, IP, opt_ip_2nd, dim_xyz>
{
  using real_t = typename F::real_t;
  using ip_coeff_t = typename IP::ip_coeff_t;

  static real_t cc(const ip_coeff_t& gx, const ip_coeff_t& gy, const ip_coeff_t& gz,
		   F EM, int m)
  {
    return (gz.vm*(gy.vm*(gx.vm*EM(m, gx.l-1,gy.l-1,gz.l-1) +
			  gx.v0*EM(m, gx.l  ,gy.l-1,gz.l-1) +
			  gx.vp*EM(m, gx.l+1,gy.l-1,gz.l-1)) +
		   gy.v0*(gx.vm*EM(m, gx.l-1,gy.l  ,gz.l-1) +
			  gx.v0*EM(m, gx.l  ,gy.l  ,gz.l-1) +
			  gx.vp*EM(m, gx.l+1,gy.l  ,gz.l-1)) +
		   gy.vp*(gx.vm*EM(m, gx.l-1,gy.l+1,gz.l-1) +
			  gx.v0*EM(m, gx.l  ,gy.l+1,gz.l-1) +
			  gx.vp*EM(m, gx.l+1,gy.l+1,gz.l-1))) +
	    gz.v0*(gy.vm*(gx.vm*EM(m, gx.l-1,gy.l-1,gz.l  ) +
			  gx.v0*EM(m, gx.l  ,gy.l-1,gz.l  ) +
			  gx.vp*EM(m, gx.l+1,gy.l-1,gz.l  )) +
		   gy.v0*(gx.vm*EM(m, gx.l-1,gy.l  ,gz.l  ) +
			  gx.v0*EM(m, gx.l  ,gy.l  ,gz.l  ) +
			  gx.vp*EM(m, gx.l+1,gy.l  ,gz.l  )) +
		   gy.vp*(gx.vm*EM(m, gx.l-1,gy.l+1,gz.l  ) +
			  gx.v0*EM(m, gx.l  ,gy.l+1,gz.l  ) +
			  gx.vp*EM(m, gx.l+1,gy.l+1,gz.l  ))) +
	    gz.vp*(gy.vm*(gx.vm*EM(m, gx.l-1,gy.l-1,gz.l+1) +
			  gx.v0*EM(m, gx.l  ,gy.l-1,gz.l+1) +
			  gx.vp*EM(m, gx.l+1,gy.l-1,gz.l+1)) +
		   gy.v0*(gx.vm*EM(m, gx.l-1,gy.l  ,gz.l+1) +
			  gx.v0*EM(m, gx.l  ,gy.l  ,gz.l+1) +
			  gx.vp*EM(m, gx.l+1,gy.l  ,gz.l+1)) +
		   gy.vp*(gx.vm*EM(m, gx.l-1,gy.l+1,gz.l+1) +
			  gx.v0*EM(m, gx.l  ,gy.l+1,gz.l+1) +
			  gx.vp*EM(m, gx.l+1,gy.l+1,gz.l+1))));
  }

  static real_t ex(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.g, EM, EX); }
  static real_t ey(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.g, EM, EY); }
  static real_t ez(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.g, ip.cz.h, EM, EZ); }
  static real_t hx(const IP& ip, F EM) { return cc(ip.cx.g, ip.cy.h, ip.cz.h, EM, HX); }
  static real_t hy(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.g, ip.cz.h, EM, HY); }
  static real_t hz(const IP& ip, F EM) { return cc(ip.cx.h, ip.cy.h, ip.cz.g, EM, HZ); }
};

// ======================================================================
// InterpolateEM

template<typename F, typename OPT_IP, typename OPT_DIM>
struct InterpolateEM
{
  using IP = InterpolateEM<F, OPT_IP, OPT_DIM>;
  using real_t = typename F::real_t;
  using ip_coeffs_t = ip_coeffs<real_t, OPT_IP>;
  using ip_coeff_t = typename ip_coeffs_t::ip_coeff_t;

  void set_coeffs(real_t xm[3])
  {
    IF_DIM_X( cx.set(xm[0]); );
    IF_DIM_Y( cy.set(xm[1]); );
    IF_DIM_Z( cz.set(xm[2]); );
  }

  using Helper = InterpolateEM_Helper<F, IP, OPT_IP, OPT_DIM>;
  real_t ex(F EM) { return Helper::ex(*this, EM); }
  real_t ey(F EM) { return Helper::ey(*this, EM); }
  real_t ez(F EM) { return Helper::ez(*this, EM); }
  real_t hx(F EM) { return Helper::hx(*this, EM); }
  real_t hy(F EM) { return Helper::hy(*this, EM); }
  real_t hz(F EM) { return Helper::hz(*this, EM); }

  ip_coeffs_t cx, cy, cz;
};

using IP = InterpolateEM<Fields3d<fields_t>, opt_ip, opt_dim>;

