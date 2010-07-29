
#include "psc.h"

#include <assert.h>

#define PIC_set_variables_F77 F77_FUNC(pic_set_variables,PIC_SET_VARIABLES)
#define PIC_push_part_yz_F77 F77_FUNC(pic_push_part_yz,PIC_PUSH_PART_YZ)
#define PIC_push_part_yz_a_F77 F77_FUNC(pic_push_part_yz_a,PIC_PUSH_PART_YZ_A)
#define PIC_push_part_yz_b_F77 F77_FUNC(pic_push_part_yz_b,PIC_PUSH_PART_YZ_B)
#define PIC_push_part_z_F77 F77_FUNC(pic_push_part_z,PIC_PUSH_PART_Z)
#define PIC_sort_1_F77 F77_FUNC(pic_sort_1,PIC_SORT_1)
#define PIC_randomize_F77 F77_FUNC(pic_randomize,PIC_RANDOMIZE)
#define PIC_find_cell_indices_F77 F77_FUNC(pic_find_cell_indices,PIC_FIND_CELL_INDICES)
#define INIT_partition_F77 F77_FUNC_(init_partition, INIT_PARTITION)
#define INIT_idistr_F77 F77_FUNC_(init_idistr, INIT_IDISTR)
#define OUT_field_1_F77 F77_FUNC(out_field_1,OUT_FIELD_1)
#define SET_param_pml_F77 F77_FUNC(set_param_pml,SET_PARAM_PML)
#define GET_niloc_F77 F77_FUNC(get_niloc,GET_NILOC)
#define INIT_grid_map_F77 F77_FUNC(init_grid_map,INIT_GRID_MAP)
#define SET_subdomain_F77 F77_FUNC_(set_subdomain, SET_SUBDOMAIN)
#define GET_subdomain_F77 F77_FUNC_(get_subdomain, GET_SUBDOMAIN)
#define ALLOC_particles_F77 F77_FUNC_(alloc_particles, ALLOC_PARTICLES)

#define p_pulse_z1__F77 F77_FUNC(p_pulse_z1_,P_PULSE_Z1_)

#define C_init_vars_F77 F77_FUNC(c_init_vars,C_INIT_VARS)
#define C_push_part_yz_F77 F77_FUNC(c_push_part_yz,C_PUSH_PART_YZ)
#define C_push_part_z_F77 F77_FUNC(c_push_part_z,C_PUSH_PART_Z)
#define C_sort_F77 F77_FUNC(c_sort,C_SORT)
#define C_out_field_F77 F77_FUNC(c_out_field,C_OUT_FIELD)
#define C_alloc_particles_cb_F77 F77_FUNC(c_alloc_particles_cb,C_ALLOC_PARTICLES_CB)
#define C_p_pulse_z1_F77 F77_FUNC(c_p_pulse_z1,C_P_PULSE_Z1)

void PIC_set_variables_F77(f_int *i1mn, f_int *i2mn, f_int *i3mn,
			   f_int *i1mx, f_int *i2mx, f_int *i3mx,
			   f_int *rd1, f_int *rd2, f_int *rd3,
			   f_int *i1n, f_int *i2n, f_int *i3n,
			   f_int *i1x, f_int *i2x, f_int *i3x,
			   f_real *cori, f_real *alpha, f_real *eta, 
			   f_real *dt, f_real *dx, f_real *dy, f_real *dz,
			   f_real *wl, f_real *wp, f_int *n);

void PIC_push_part_yz_F77(f_int *niloc, struct f_particle *p_niloc,
			  f_real *p2A, f_real *p2B,
			  f_real *ne, f_real *ni, f_real *nn,
			  f_real *jxi, f_real *jyi, f_real *jzi,
			  f_real *ex, f_real *ey, f_real *ez,
			  f_real *bx, f_real *by, f_real *bz);

void PIC_push_part_yz_a_F77(f_int *niloc, struct f_particle *p_niloc,
			    f_real *p2A, f_real *p2B,
			    f_real *ne, f_real *ni, f_real *nn,
			    f_real *jxi, f_real *jyi, f_real *jzi,
			    f_real *ex, f_real *ey, f_real *ez,
			    f_real *bx, f_real *by, f_real *bz);

void PIC_push_part_yz_b_F77(f_int *niloc, struct f_particle *p_niloc,
			    f_real *p2A, f_real *p2B,
			    f_real *ne, f_real *ni, f_real *nn,
			    f_real *jxi, f_real *jyi, f_real *jzi,
			    f_real *ex, f_real *ey, f_real *ez,
			    f_real *bx, f_real *by, f_real *bz);

void PIC_push_part_z_F77(f_int *niloc, struct f_particle *p_niloc,
			 f_real *p2A, f_real *p2B,
			 f_real *ne, f_real *ni, f_real *nn,
			 f_real *jxi, f_real *jyi, f_real *jzi,
			 f_real *ex, f_real *ey, f_real *ez,
			 f_real *bx, f_real *by, f_real *bz);

void PIC_sort_1_F77(f_int *niloc, struct f_particle *p_niloc);
void PIC_randomize_F77(f_int *niloc, struct f_particle *p_niloc);
void PIC_find_cell_indices_F77(f_int *niloc, struct f_particle *p_niloc);
void INIT_partition_F77(f_int *part_label_off, f_int *rd1n, f_int *rd1x,
			f_int *rd2n, f_int *rd2x, f_int *rd3n, f_int *rd3x,
			f_int *niloc_new);
void INIT_idistr_F77(f_int *part_label_off, f_int *rd1n, f_int *rd1x,
		     f_int *rd2n, f_int *rd2x, f_int *rd3n, f_int *rd3x);
void OUT_field_1_F77(void);
void SET_param_pml_F77(f_int *thick, f_int *cushion, f_int *size, f_int *order);
void GET_niloc_F77(f_int *n_part);
void INIT_grid_map_F77(void);
void SET_subdomain_F77(f_int *i1mn, f_int *i1mx, f_int *i2mn, f_int *i2mx,
		       f_int *i3mn, f_int *i3mx);
void GET_subdomain_F77(f_int *i1mn, f_int *i1mx, f_int *i2mn, f_int *i2mx,
		       f_int *i3mn, f_int *i3mx);
void ALLOC_particles_F77(f_int *n_part);

f_real p_pulse_z1__F77(f_real *xx, f_real *yy, f_real *zz, f_real *tt);

// ----------------------------------------------------------------------
// Wrappers to be called from C that call into Fortran

static void
PIC_set_variables()
{
  int i0mx = psc.ihi[0] - 1, i1mx = psc.ihi[1] - 1, i2mx = psc.ihi[2] - 1;
  int i0x = psc.ghi[0] - 1, i1x = psc.ghi[1] - 1, i2x = psc.ghi[2] - 1;

  PIC_set_variables_F77(&psc.ilo[0], &psc.ilo[1], &psc.ilo[2],
			&i0mx, &i1mx, &i2mx,
			&psc.ibn[0], &psc.ibn[1], &psc.ibn[2],
			&psc.glo[0], &psc.glo[1], &psc.glo[2],
			&i0x, &i1x, &i2x,
			&psc.coeff.cori, &psc.coeff.alpha, &psc.coeff.eta,
			&psc.dt, &psc.dx[0], &psc.dx[1], &psc.dx[2],
			&psc.coeff.wl, &psc.coeff.wp, &psc.timestep);
}

void
PIC_push_part_yz()
{
  PIC_set_variables();
  PIC_push_part_yz_F77(&psc.n_part, &psc.f_part[-1], &psc.p2A, &psc.p2B,
		       psc.f_fields[NE], psc.f_fields[NI], psc.f_fields[NN],
		       psc.f_fields[JXI], psc.f_fields[JYI], psc.f_fields[JZI],
		       psc.f_fields[EX], psc.f_fields[EY], psc.f_fields[EZ],
		       psc.f_fields[BX], psc.f_fields[BY], psc.f_fields[BZ]);
}

void
PIC_push_part_yz_a()
{
  PIC_set_variables();
  PIC_push_part_yz_a_F77(&psc.n_part, &psc.f_part[-1], &psc.p2A, &psc.p2B,
			 psc.f_fields[NE], psc.f_fields[NI], psc.f_fields[NN],
			 psc.f_fields[JXI], psc.f_fields[JYI], psc.f_fields[JZI],
			 psc.f_fields[EX], psc.f_fields[EY], psc.f_fields[EZ],
			 psc.f_fields[BX], psc.f_fields[BY], psc.f_fields[BZ]);
}

void
PIC_push_part_yz_b()
{
  PIC_set_variables();
  PIC_push_part_yz_b_F77(&psc.n_part, &psc.f_part[-1], &psc.p2A, &psc.p2B,
			 psc.f_fields[NE], psc.f_fields[NI], psc.f_fields[NN],
			 psc.f_fields[JXI], psc.f_fields[JYI], psc.f_fields[JZI],
			 psc.f_fields[EX], psc.f_fields[EY], psc.f_fields[EZ],
			 psc.f_fields[BX], psc.f_fields[BY], psc.f_fields[BZ]);
}

void
PIC_push_part_z()
{
  PIC_set_variables();
  PIC_push_part_z_F77(&psc.n_part, &psc.f_part[-1], &psc.p2A, &psc.p2B,
		      psc.f_fields[NE], psc.f_fields[NI], psc.f_fields[NN],
		      psc.f_fields[JXI], psc.f_fields[JYI], psc.f_fields[JZI],
		      psc.f_fields[EX], psc.f_fields[EY], psc.f_fields[EZ],
		      psc.f_fields[BX], psc.f_fields[BY], psc.f_fields[BZ]);
}

void
PIC_sort_1()
{
  PIC_sort_1_F77(&psc.n_part, &psc.f_part[-1]);
}

void
PIC_randomize()
{
  PIC_randomize_F77(&psc.n_part, &psc.f_part[-1]);
}

void
PIC_find_cell_indices()
{
  PIC_set_variables();
  PIC_find_cell_indices_F77(&psc.n_part, &psc.f_part[-1]);
}

void
OUT_field_1()
{
  OUT_field_1_F77();
}

void
SET_param_pml()
{
  SET_param_pml_F77(&psc.pml.thick, &psc.pml.cushion, &psc.pml.size, &psc.pml.order);
}

void
GET_niloc(int *n_part)
{
  GET_niloc_F77(n_part);
}

void
INIT_grid_map(void)
{
  INIT_grid_map_F77();
}

real
PSC_p_pulse_z1(real xx, real yy, real zz, real tt)
{
  f_real _xx = xx, _yy = yy, _zz = zz, _tt = tt;
  return p_pulse_z1__F77(&_xx, &_yy, &_zz, &_tt);
}

static int rd1n, rd1x, rd2n, rd2x, rd3n, rd3x;
static int part_label_offset;

void
INIT_partition(int *n_part)
{
  INIT_partition_F77(&part_label_offset, &rd1n, &rd1x, &rd2n, &rd2x, &rd3n, &rd3x,
		     n_part);
  GET_subdomain();
}

void
INIT_idistr(void)
{
  INIT_idistr_F77(&part_label_offset, &rd1n, &rd1x, &rd2n, &rd2x, &rd3n, &rd3x);
}

void
SET_subdomain()
{
  f_int i1mn = psc.ilo[0];
  f_int i2mn = psc.ilo[1];
  f_int i3mn = psc.ilo[2];
  f_int i1mx = psc.ihi[0] - 1;
  f_int i2mx = psc.ihi[1] - 1;
  f_int i3mx = psc.ihi[2] - 1;
  SET_subdomain_F77(&i1mn, &i1mx, &i2mn, &i2mx, &i3mn, &i3mx);
}

void
GET_subdomain()
{
  f_int i1mn, i2mn, i3mn, i1mx, i2mx, i3mx;
  GET_subdomain_F77(&i1mn, &i1mx, &i2mn, &i2mx, &i3mn, &i3mx);
  psc.ilo[0] = i1mn;
  psc.ilo[1] = i2mn;
  psc.ilo[2] = i3mn;
  psc.ihi[0] = i1mx + 1;
  psc.ihi[1] = i2mx + 1;
  psc.ihi[2] = i3mx + 1;
}

// ----------------------------------------------------------------------
// Wrappers to be called from Fortran that continue to C

void
C_init_vars_F77(f_real *dt, f_real *dx, f_real *dy, f_real *dz,
		f_int *i1mn, f_int *i2mn, f_int *i3mn,
		f_int *i1mx, f_int *i2mx, f_int *i3mx,
		f_int *rd1, f_int *rd2, f_int *rd3,
		f_int *i1n, f_int *i2n, f_int *i3n,
		f_int *i1x, f_int *i2x, f_int *i3x,
		f_real *ne, f_real *ni, f_real *nn,
		f_real *jxi, f_real *jyi, f_real *jzi,
		f_real *ex, f_real *ey, f_real *ez,
		f_real *bx, f_real *by, f_real *bz,
		f_real *cori, f_real *alpha, f_real *beta,
		f_real *eta, f_real *wl, f_real *wp,
		f_int *n,
		f_int *dummy)
{
  // make sure we got passed the right number of arguments
  assert(*dummy == 99);

  // time step, grid spacing
  psc.dt = *dt;
  psc.dx[0] = *dx;
  psc.dx[1] = *dy;
  psc.dx[2] = *dz;

  // local domain size
  psc.ilo[0] = *i1mn; 
  psc.ilo[1] = *i2mn; 
  psc.ilo[2] = *i3mn; 
  psc.ihi[0] = *i1mx + 1; 
  psc.ihi[1] = *i2mx + 1; 
  psc.ihi[2] = *i3mx + 1; 
  psc.ibn[0] = *rd1;
  psc.ibn[1] = *rd2;
  psc.ibn[2] = *rd3;
  for (int d = 0; d < 3; d++) {
    psc.ilg[d] = psc.ilo[d] - psc.ibn[d];
    psc.ihg[d] = psc.ihi[d] + psc.ibn[d];
    psc.img[d] = psc.ihg[d] - psc.ilg[d];
  }
  psc.fld_size = psc.img[0] * psc.img[1] * psc.img[2];

  // global domain size
  psc.glo[0] = *i1n; 
  psc.glo[1] = *i2n; 
  psc.glo[2] = *i3n; 
  psc.ghi[0] = *i1x + 1; 
  psc.ghi[1] = *i2x + 1; 
  psc.ghi[2] = *i3x + 1; 

  // Fortran fields
  psc.f_fields[NE] = ne;
  psc.f_fields[NI] = ni;
  psc.f_fields[NN] = nn;
  psc.f_fields[JXI] = jxi;
  psc.f_fields[JYI] = jyi;
  psc.f_fields[JZI] = jzi;
  psc.f_fields[EX] = ex;
  psc.f_fields[EY] = ey;
  psc.f_fields[EZ] = ez;
  psc.f_fields[BX] = bx;
  psc.f_fields[BY] = by;
  psc.f_fields[BZ] = bz;

  // parameters etc
  psc.coeff.cori = *cori;
  psc.coeff.alpha = *alpha;
  psc.coeff.beta = *beta;
  psc.coeff.eta = *eta;
  psc.coeff.wl = *wl;
  psc.coeff.wp = *wp;

  // current timestep, I/O control
  psc.timestep = *n;
}

void
C_push_part_yz_F77(f_real *p2A, f_real *p2B,
		   f_int *niloc, struct f_particle *p_niloc,
		   f_int *dummy)
{
  // make sure we got passed the right number of arguments
  assert(*dummy == 99);

  psc.p2A = *p2A;
  psc.p2B = *p2B;

  psc.n_part = *niloc;
  psc.f_part = &p_niloc[1];

  psc_push_part_yz();

  *p2A = psc.p2A;
  *p2A = psc.p2B;
}

void
C_push_part_z_F77(f_real *p2A, f_real *p2B,
		  f_int *niloc, struct f_particle *p_niloc,
		  f_int *dummy)
{
  // make sure we got passed the right number of arguments
  assert(*dummy == 99);

  psc.p2A = *p2A;
  psc.p2B = *p2B;

  psc.n_part = *niloc;
  psc.f_part = &p_niloc[1];

  psc_push_part_z();

  *p2A = psc.p2A;
  *p2A = psc.p2B;
}

void
C_sort_F77(f_int *niloc, struct f_particle *p_niloc,
	   f_int *dummy)
{
  // make sure we got passed the right number of arguments
  assert(*dummy == 99);

  psc.n_part = *niloc;
  psc.f_part = &p_niloc[1];

  psc_sort();
}

void
C_out_field_F77(f_int *dummy)
{
  // make sure we got passed the right number of arguments
  assert(*dummy == 99);

  psc_out_field();
}

f_real
C_p_pulse_z1_F77(f_real *xx, f_real *yy, f_real *zz, f_real *tt)
{
  return psc_p_pulse_z1(*xx, *yy, *zz, *tt);
}

// ----------------------------------------------------------------------
// slightly hacky way to do the equivalent of "malloc" using
// Fortran's allocate()

static struct f_particle *__f_part;

struct f_particle *
ALLOC_particles(int n_part)
{
  ALLOC_particles_F77(&n_part);
  // the callback function below will have magically been called,
  // setting __f_part
  return __f_part;
}

void
C_alloc_particles_cb_F77(struct f_particle *p_niloc)
{
  __f_part = &p_niloc[1];
}

