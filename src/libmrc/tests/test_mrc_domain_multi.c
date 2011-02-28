
#include <mrc_params.h>
#include <mrc_domain.h>
#include <mrc_fld.h>

#include <stdio.h>
#include <assert.h>

static void
set_m3(struct mrc_m3 *m3)
{
  struct mrc_patch *patches = mrc_domain_get_patches(m3->domain, NULL);

  mrc_m3_foreach_patch(m3, p) {
    struct mrc_m3_patch *m3p = mrc_m3_patch_get(m3, p);
    int *off = patches[p].off;
    mrc_m3_foreach(m3p, ix,iy,iz, 0,0) {
      MRC_M3(m3p, 0, ix,iy,iz) =
	(iz + off[2]) * 10000 + (iy + off[1]) * 100 + (ix + off[0]);
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(m3);
  }
}

static void
check_m3(struct mrc_m3 *m3)
{
  struct mrc_patch *patches = mrc_domain_get_patches(m3->domain, NULL);

  mrc_m3_foreach_patch(m3, p) {
    struct mrc_m3_patch *m3p = mrc_m3_patch_get(m3, p);
    int *off = patches[p].off;
    mrc_m3_foreach(m3p, ix,iy,iz, 0,0) {
      assert(MRC_M3(m3p, 0, ix,iy,iz) ==
	     (iz + off[2]) * 10000 + (iy + off[1]) * 100 + (ix + off[0]));
    } mrc_m3_foreach_end;
    mrc_m3_patch_put(m3);
  }
}

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  struct mrc_domain *domain = mrc_domain_create(MPI_COMM_WORLD);
  mrc_domain_set_type(domain, "multi");
  mrc_domain_set_from_options(domain);
  mrc_domain_setup(domain);
  mrc_domain_view(domain);

  struct mrc_m3 *m3 = mrc_domain_m3_create(domain, 0);
  mrc_m3_setup(m3);
  mrc_m3_view(m3);

  set_m3(m3);
  check_m3(m3);

  mrc_m3_destroy(m3);

  mrc_domain_destroy(domain);

  MPI_Finalize();
}
