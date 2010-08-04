
#include "psc_testing.h"

#include <math.h>
#include <limits.h>

// ----------------------------------------------------------------------
// assert_equal
//
// make sure that the two values are almost equal.

void
__assert_equal(double x, double y, const char *xs, const char *ys, double thres)
{
  double max = fmax(fabs(x), fabs(y)) + 1e-10;
  double eps = fabs((x - y) / max);
  if (eps > thres) {
    fprintf(stderr, "assert_equal: fail %s = %g, %s = %g rel err = %g thres = %g\n",
	    xs, x, ys, y, eps, thres);
    abort();
  }
}

static particle_base_t *particle_ref;
static f_real *field_ref[NR_FIELDS];

// ----------------------------------------------------------------------
// psc_save_particles_ref
//
// save current particle data as reference solution

void
psc_save_particles_ref()
{
  if (!particle_ref) {
    particle_ref = calloc(psc.pp.n_part, sizeof(*particle_ref));
  }
  for (int i = 0; i < psc.pp.n_part; i++) {
    particle_ref[i] = *particles_base_get_one(&psc.pp, i);
  }
}

// ----------------------------------------------------------------------
// psc_save_fields_ref
//
// save current field data as reference solution

void
psc_save_fields_ref()
{
  if (!field_ref[EX]) { //FIXME this is bad mojo
    for (int m = 0; m < NR_FIELDS; m++) {
      field_ref[m] = calloc(psc.fld_size, sizeof(f_real));
    }
  }
  for (int m = 0; m < NR_FIELDS; m++) {
    for (int iz = psc.ilg[2]; iz < psc.ihg[2]; iz++) {
      for (int iy = psc.ilg[1]; iy < psc.ihg[1]; iy++) {
	for (int ix = psc.ilg[0]; ix < psc.ihg[0]; ix++) {
	  _FF3(field_ref[m], ix,iy,iz) = F3_BASE(m, ix,iy,iz);
	}
      }
    }
  }
} 

// ----------------------------------------------------------------------
// psc_check_particles_ref
//
// check current particle data agains previously saved reference solution

void
psc_check_particles_ref(double thres)
{
  assert(particle_ref);
  for (int i = 0; i < psc.pp.n_part; i++) {
    particle_base_t *part = particles_base_get_one(&psc.pp, i);
    //    printf("i = %d\n", i);
    assert_equal(part->xi , particle_ref[i].xi, thres);
    assert_equal(part->yi , particle_ref[i].yi, thres);
    assert_equal(part->zi , particle_ref[i].zi, thres);
    assert_equal(part->pxi, particle_ref[i].pxi, thres);
    assert_equal(part->pyi, particle_ref[i].pyi, thres);
    assert_equal(part->pzi, particle_ref[i].pzi, thres);
  }
}


// ----------------------------------------------------------------------
// psc_check_fields_ref
//
// check field data against previously saved reference solution

void
psc_check_fields_ref(int *flds, double thres)
{
  assert(field_ref[EX]);
  for (int i = 0; flds[i] >= 0; i++) {
    int m = flds[i];
    for (int iz = psc.ilo[2]; iz < psc.ihi[2]; iz++) {
      for (int iy = psc.ilo[1]; iy < psc.ihi[1]; iy++) {
	for (int ix = psc.ilo[0]; ix < psc.ihi[0]; ix++) {
	  //printf("m %d %d,%d,%d\n", m, ix,iy,iz);
	  assert_equal(F3_BASE(m, ix,iy,iz), _FF3(field_ref[m], ix,iy,iz), thres);
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// psc_check_currents_ref
//
// check current current density data agains previously saved reference solution

void
psc_check_currents_ref(double thres)
{
  assert(field_ref[JXI]);
  for (int m = JXI; m <= JZI; m++){
    for (int iz = psc.ilg[2]; iz < psc.ihg[2]; iz++) {
      for (int iy = psc.ilg[1]; iy < psc.ihg[1]; iy++) {
	for (int ix = psc.ilg[0]; ix < psc.ihg[0]; ix++) {
	  //	  printf("m %d %d,%d,%d\n", m, ix,iy,iz);
	  assert_equal(F3_BASE(m, ix,iy,iz), _FF3(field_ref[m], ix,iy,iz), thres);
	}
      }
    }
  }
}

void
psc_check_currents_ref_noghost(double thres)
{
  assert(field_ref[JXI]);
  for (int m = JXI; m <= JZI; m++){
    for (int iz = psc.ilo[2]; iz < psc.ihi[2]; iz++) {
      for (int iy = psc.ilo[1]; iy < psc.ihi[1]; iy++) {
	for (int ix = psc.ilo[0]; ix < psc.ihi[0]; ix++) {
	  //	  printf("m %d %d,%d,%d\n", m, ix,iy,iz);
	  assert_equal(F3_BASE(m, ix,iy,iz), _FF3(field_ref[m], ix,iy,iz), thres);
	}
      }
    }
  }
}

// ----------------------------------------------------------------------
// psc_check_particles_sorted
//
// checks particles are sorted by cell index

void
psc_check_particles_sorted()
{
#if PARTICLES_BASE == PARTICLES_FORTRAN
  int last = INT_MIN;

  for (int i = 0; i < psc.pp.n_part; i++) {
    assert(psc.pp.particles[i].cni >= last);
    last = psc.pp.particles[i].cni;
  }
#else
  assert(0);
#endif
}

// ----------------------------------------------------------------------
// psc_create_test_xz

void
psc_create_test_xz(struct psc_mod_config *conf)
{
  // make sure if we call it again, we really get the same i.c.
  srandom(0);

  psc_create(conf);
  psc_init("test_xz");
}

// ----------------------------------------------------------------------
// psc_create_test_yz

void
psc_create_test_yz(struct psc_mod_config *conf)
{
  // make sure if we call it again, we really get the same i.c.
  srandom(0);

  psc_create(conf);
  psc_init("test_yz");
}

