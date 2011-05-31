
#include "mrctest.h"

#include <mrc_domain.h>
#include <mrc_fld.h>
#include <mrc_params.h>
#include <mrc_profile.h>
#include <mrc_io.h>
#include <mrc_mod.h>

#include <math.h>
#include <assert.h>
#include <string.h>

void
mrc_f3_init_values(struct mrc_f3 *f3, struct mrc_f3_init_values_info *iv_info)
{
  struct mrc_domain *domain = f3->domain;
  assert(domain);
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  assert(crds);

  mrc_f3_set(f3, 0.);

  for (int i = 0; iv_info->ini_flds[i].ini; i++) {
    int m = iv_info->ini_flds[i].m;
    mrc_f3_foreach(f3, ix,iy,iz, 2, 2) {
      float xx = MRC_CRDX(crds, ix), yy = MRC_CRDY(crds, iy), zz = MRC_CRDZ(crds, iz);
      MRC_F3(f3,m, ix,iy,iz) = iv_info->ini_flds[i].ini(xx, yy, zz);
    } mrc_f3_foreach_end;
  }
}

void
mrctest_init(int *argc, char ***argv)
{
  MPI_Init(argc, argv);
  libmrc_params_init(*argc, *argv);
}

void
mrctest_finalize()
{
  prof_print();
  MPI_Finalize();
}

// ----------------------------------------------------------------------
// mrctest_domain

#define VAR(x) (void *)offsetof(struct mrctest_domain_params, x)
static struct param mrctest_domain_params_descr[] = {
  { "mx"              , VAR(gdims[0])        , PARAM_INT(128)        },
  { "my"              , VAR(gdims[1])        , PARAM_INT(64)         },
  { "mz"              , VAR(gdims[2])        , PARAM_INT(32)         },
  { "npx"             , VAR(nproc[0])        , PARAM_INT(1)          },
  { "npy"             , VAR(nproc[1])        , PARAM_INT(1)          },
  { "npz"             , VAR(nproc[2])        , PARAM_INT(1)          },
  { "use_diagsrv"     , VAR(use_diagsrv)     , PARAM_BOOL(false)     },
  {},
};
#undef VAR

void
mrctest_domain_init(struct mrctest_domain_params *par)
{
  mrc_params_parse(par, mrctest_domain_params_descr, "mrctest_domain", MPI_COMM_WORLD);
  mrc_params_print(par, mrctest_domain_params_descr, "mrctest_domain", MPI_COMM_WORLD);
}

struct mrc_domain *
mrctest_create_domain(MPI_Comm comm, struct mrctest_domain_params *par)
{
  struct mrc_domain *domain = mrc_domain_create(comm);
  mrc_domain_set_type(domain, "simple");
  mrc_domain_set_param_int3(domain, "m", par->gdims);
  mrc_domain_set_param_int3(domain, "np", par->nproc);
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  mrc_crds_set_param_int(crds, "sw", SW_2);
  mrc_crds_set_param_float3(crds, "l", (float[3]) { -30., -20., -20. });
  mrc_crds_set_param_float3(crds, "h", (float[3]) {  50.,  20.,  20. });
  mrc_domain_set_from_options(domain);
  mrc_domain_setup(domain);

  return domain;
}

struct mrc_domain *
mrctest_create_domain_rectilinear(MPI_Comm comm, struct mrctest_domain_params *par)
{
  struct mrc_domain *domain = mrc_domain_create(comm);
  mrc_domain_set_type(domain, "simple");
  mrc_domain_set_param_int3(domain, "m", par->gdims);
  mrc_domain_set_param_int3(domain, "np", par->nproc);
  struct mrc_crds *crds = mrc_domain_get_crds(domain);
  mrc_crds_set_type(crds, "rectilinear");
  mrc_crds_set_param_int(crds, "sw", SW_2);
  mrc_crds_set_param_float3(crds, "l", (float[3]) { -30., -20., -20. });
  mrc_crds_set_param_float3(crds, "h", (float[3]) {  50.,  20.,  20. });
  mrc_domain_set_from_options(domain);
  mrc_domain_setup(domain);
  int sw;
  mrc_crds_get_param_int(crds, "sw", &sw);
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(domain, &nr_patches);
  assert(nr_patches == 1);
  for (int ix = -sw; ix < patches[0].ldims[0] + sw; ix++) {
    MRC_CRDX(crds, ix) = ix*ix;
  }

  return domain;
}

void
mrctest_domain_init_values_0(struct mrc_f3 *f)
{
  struct mrc_crds *crds = mrc_domain_get_crds(f->domain);

  mrc_f3_foreach(f, ix,iy,iz, 0, 0) {
    float xx = MRC_CRDX(crds, ix);

    MRC_F3(f,0, ix,iy,iz) = 2.f + .2f * sin(xx);
  } mrc_f3_foreach_end;
}

static void
mrctest_domain_init_values_1(struct mrc_f3 *f)
{
  struct mrc_crds *crds = mrc_domain_get_crds(f->domain);

  mrc_f3_foreach(f, ix,iy,iz, 0, 0) {
    float yy = MRC_CRDY(crds, iy);

    MRC_F3(f,1, ix,iy,iz) = 2.f + .2f * sin(yy);
  } mrc_f3_foreach_end;
}

struct mrc_f3 *
mrctest_create_field_1(struct mrc_domain *domain)
{
  struct mrc_f3 *f3 = mrc_domain_f3_create(domain, SW_2);
  mrc_f3_set_comp_name(f3, 0, "test");
  mrc_f3_setup(f3);
  mrctest_domain_init_values_0(f3);
  return f3;
}

struct mrc_f3 *
mrctest_create_field_2(struct mrc_domain *domain)
{
  struct mrc_f3 *f3 = mrc_domain_f3_create(domain, SW_2);
  mrc_f3_set_nr_comps(f3, 2);
  mrc_f3_set_comp_name(f3, 0, "test0");
  mrc_f3_set_comp_name(f3, 1, "test1");
  mrc_f3_setup(f3);
  mrctest_domain_init_values_0(f3);
  mrctest_domain_init_values_1(f3);
  return f3;
}

struct mrc_m1 *
mrctest_create_m1_1(struct mrc_domain *domain, int dim)
{
  struct mrc_m1 *m1 = mrc_domain_m1_create(domain);
  mrc_m1_set_param_int(m1, "sw", 2);
  mrc_m1_set_param_int(m1, "dim", dim);
  mrc_m1_setup(m1);
  mrc_m1_set_comp_name(m1, 0, "test");
  
  mrc_m1_foreach_patch(m1, p) {
    struct mrc_m1_patch *m1p = mrc_m1_patch_get(m1, p);
    mrc_m1_foreach(m1p, ix, 2, 2) {
      MRC_M1(m1p, 0, ix) = 1.f + ix * ix;
    } mrc_m1_foreach_end;
    mrc_m1_patch_put(m1);
  }
  return m1;
}

static void
mod_diagsrv(struct mrc_mod *mod, void *arg)
{
  int nr_procs_domain = mrc_mod_get_nr_procs(mod, "domain");
  mrc_io_server("xdmf_serial", "cache", nr_procs_domain);
}

void
mrctest_domain(void (*mod_domain)(struct mrc_mod *mod, void *arg))
{
  struct mrctest_domain_params par;
  mrctest_domain_init(&par);

  int nproc_domain = par.nproc[0] * par.nproc[1] * par.nproc[2];

  struct mrc_mod *mod = mrc_mod_create(MPI_COMM_WORLD);
  mrc_mod_register(mod, "domain", nproc_domain, mod_domain, &par);
  if (par.use_diagsrv) {
    mrc_mod_register(mod, "diagsrv", 1, mod_diagsrv, &par);
  }
  mrc_mod_view(mod);
  mrc_mod_setup(mod);
  mrc_mod_run(mod);
  mrc_mod_destroy(mod);
}

// ----------------------------------------------------------------------
// mrctest_f3_compare

void
mrctest_f3_compare(struct mrc_f3 *f1, struct mrc_f3 *f2, float eps)
{
  assert(f1->nr_comp == f2->nr_comp);
  for (int m = 0; m < f1->nr_comp; m++) {
    float diff = 0.;
    mrc_f3_foreach(f1, ix,iy,iz, 0, 0) {
      diff = fmaxf(diff, fabsf(MRC_F3(f1,m, ix,iy,iz) - MRC_F3(f2,m, ix,iy,iz)));
    } mrc_f3_foreach_end;
    if (diff > eps) {
      mprintf("mrctest_f3_compare: m = %d diff = %g\n", m, diff);
      assert(0);
    }
  }
}

// ----------------------------------------------------------------------
// mrctest_m1_compare

void
mrctest_m1_compare(struct mrc_m1 *m1_1, struct mrc_m1 *m1_2, float eps)
{
  assert(mrc_m1_same_shape(m1_1, m1_2));
  int sw;
  mrc_m1_get_param_int(m1_1, "sw", &sw);
  for (int m = 0; m < m1_2->nr_comp; m++) {
    float diff = 0.;
    mrc_m1_foreach_patch(m1_1, p) {
      struct mrc_m1_patch *m1p_1 = mrc_m1_patch_get(m1_1, p);
      struct mrc_m1_patch *m1p_2 = mrc_m1_patch_get(m1_2, p);
      
      mrc_m1_foreach(m1p_1, ix, sw, sw) {
	diff = fmaxf(diff, fabsf(MRC_M1(m1p_1, m, ix) - MRC_M1(m1p_2, m, ix)));
      } mrc_m1_foreach_end;

      mrc_m1_patch_put(m1_1);
      mrc_m1_patch_put(m1_2);
    }
    if (diff > eps) {
      mprintf("mrctest_m1_compare: m = %d diff = %g\n", m, diff);
      assert(0);
    }
  }
}

// ----------------------------------------------------------------------
// mrctest_crds_compare

void
mrctest_crds_compare(struct mrc_crds *crds1, struct mrc_crds *crds2)
{
  int sw = crds1->par.sw;

  assert(crds1->par.sw == crds2->par.sw);
  for (int d = 0; d < 3; d++) {
    assert(crds1->par.xl[d] == crds2->par.xl[d]);
    assert(crds1->par.xh[d] == crds2->par.xh[d]);
  }

  assert(strcmp(mrc_crds_type(crds1), mrc_crds_type(crds2)) == 0);
  int nr_patches;
  struct mrc_patch *patches = mrc_domain_get_patches(crds1->domain, &nr_patches);
  for (int d = 0; d < 3; d++) {
    if (crds1->crd[d]) {
      assert(nr_patches == 1);
      float diff = 0.;
      for (int ix = -sw; ix < patches[0].ldims[d] + sw; ix++) {
	diff = fmaxf(diff, fabsf(MRC_CRD(crds1, d, ix) - MRC_CRD(crds2, d, ix)));
	if (diff > 0.) {
	  mprintf("mrctest_crds_compare: ix = %d diff = %g\n", ix, diff);
	  assert(0);
	}
      }
    } else {
      mrc_m1_foreach_patch(crds1->mcrd[d], p) {
	struct mrc_m1_patch *m1p_1 = mrc_m1_patch_get(crds1->mcrd[d], p);
	struct mrc_m1_patch *m1p_2 = mrc_m1_patch_get(crds2->mcrd[d], p);
	float diff = 0.;
	mrc_m1_foreach(m1p_1, ix, sw, sw) {
	  diff = fmaxf(diff, fabsf(MRC_M1(m1p_1, 0, ix) - MRC_M1(m1p_2, 0, ix)));
	  if (diff > 0.) {
	    mprintf("mrctest_crds_compare: ix = %d diff = %g %g/%g\n", ix, diff,
		    MRC_M1(m1p_1, 0, ix), MRC_M1(m1p_2, 0, ix));
	    assert(0);
	  }
	} mrc_m1_foreach_end;
	mrc_m1_patch_put(crds1->mcrd[d]);
	mrc_m1_patch_put(crds2->mcrd[d]);
      }
    }
  }
}

