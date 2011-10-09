
#include "psc_output_fields_c.h"
#include "psc_output_format.h"
#include "psc_moments.h"
#include "psc_fields_as_c.h"

#include <mrc_profile.h>
#include <mrc_params.h>
#include <mrc_io.h>
#include <string.h>

#define to_psc_output_fields_c(out) ((struct psc_output_fields_c *)((out)->obj.subctx))

// ======================================================================

#define define_dxdydz(dx, dy, dz)					\
  int dx __unused = (psc->domain.gdims[0] == 1) ? 0 : 1;			\
  int dy __unused = (psc->domain.gdims[1] == 1) ? 0 : 1;			\
  int dz __unused = (psc->domain.gdims[2] == 1) ? 0 : 1

#define JX_CC(ix,iy,iz) (.5f * (F3(pf, JXI,ix,iy,iz) + F3(pf, JXI,ix-dx,iy,iz)))
#define JY_CC(ix,iy,iz) (.5f * (F3(pf, JYI,ix,iy,iz) + F3(pf, JYI,ix,iy-dy,iz)))
#define JZ_CC(ix,iy,iz) (.5f * (F3(pf, JZI,ix,iy,iz) + F3(pf, JZI,ix,iy,iz-dz)))

static void
calc_j(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, JXI, JXI + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = JX_CC(ix,iy,iz);
      F3(ff, 1, ix,iy,iz) = JY_CC(ix,iy,iz);
      F3(ff, 2, ix,iy,iz) = JZ_CC(ix,iy,iz);
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

#define EX_CC(ix,iy,iz) (.5f * (F3(pf,  EX,ix,iy,iz) + F3(pf,  EX,ix-dx,iy,iz)))
#define EY_CC(ix,iy,iz) (.5f * (F3(pf,  EY,ix,iy,iz) + F3(pf,  EY,ix,iy-dy,iz)))
#define EZ_CC(ix,iy,iz) (.5f * (F3(pf,  EZ,ix,iy,iz) + F3(pf,  EZ,ix,iy,iz-dz)))

static void
calc_E(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, EX, EX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = EX_CC(ix,iy,iz);
      F3(ff, 1, ix,iy,iz) = EY_CC(ix,iy,iz);
      F3(ff, 2, ix,iy,iz) = EZ_CC(ix,iy,iz);
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

#define HX_CC(ix,iy,iz) (.25f*(F3(pf, HX,ix,iy,iz   ) + F3(pf, HX,ix,iy-dy,iz   ) + \
			       F3(pf, HX,ix,iy,iz-dz) + F3(pf, HX,ix,iy-dy,iz-dz)))
#define HY_CC(ix,iy,iz) (.25f*(F3(pf, HY,ix,iy,iz   ) + F3(pf, HY,ix-dx,iy,iz   ) + \
			       F3(pf, HY,ix,iy,iz-dz) + F3(pf, HY,ix-dx,iy,iz-dz)))
#define HZ_CC(ix,iy,iz) (.25f*(F3(pf, HZ,ix,iy   ,iz) + F3(pf, HZ,ix-dx,iy   ,iz) + \
			       F3(pf, HZ,ix,iy-dy,iz) + F3(pf, HZ,ix-dx,iy-dy,iz)))

static void
calc_H(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, HX, HX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = HX_CC(ix,iy,iz);
      F3(ff, 1, ix,iy,iz) = HY_CC(ix,iy,iz);
      F3(ff, 2, ix,iy,iz) = HZ_CC(ix,iy,iz);
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

static void
calc_jdote(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, JXI, EX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = JX_CC(ix,iy,iz) * EX_CC(ix,iy,iz);
      F3(ff, 1, ix,iy,iz) = JY_CC(ix,iy,iz) * EY_CC(ix,iy,iz);
      F3(ff, 2, ix,iy,iz) = JZ_CC(ix,iy,iz) * EZ_CC(ix,iy,iz);
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

static void
calc_poyn(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, EX, HX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = (EY_CC(ix,iy,iz) * HZ_CC(ix,iy,iz) - 
			      EZ_CC(ix,iy,iz) * HY_CC(ix,iy,iz));
      F3(ff, 1, ix,iy,iz) = (EZ_CC(ix,iy,iz) * HX_CC(ix,iy,iz) -
			      EX_CC(ix,iy,iz) * HZ_CC(ix,iy,iz));
      F3(ff, 2, ix,iy,iz) = (EX_CC(ix,iy,iz) * HY_CC(ix,iy,iz) -
			      EY_CC(ix,iy,iz) * HX_CC(ix,iy,iz));
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

static void
calc_E2(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, EX, EX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = sqr(EX_CC(ix,iy,iz));
      F3(ff, 1, ix,iy,iz) = sqr(EY_CC(ix,iy,iz));
      F3(ff, 2, ix,iy,iz) = sqr(EZ_CC(ix,iy,iz));
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

static void
calc_H2(struct psc *psc, mfields_base_t *flds_base, mparticles_base_t *particles, mfields_t *f)
{
  define_dxdydz(dx, dy, dz);
  mfields_t *flds = psc_mfields_get_cf(flds_base, HX, HX + 3);
  psc_foreach_patch(psc, p) {
    fields_t *ff = psc_mfields_get_patch(f, p);
    fields_t *pf = psc_mfields_get_patch(flds, p);
    psc_foreach_3d(psc, p, ix, iy, iz, 0, 0) {
      F3(ff, 0, ix,iy,iz) = sqr(HX_CC(ix,iy,iz));
      F3(ff, 1, ix,iy,iz) = sqr(HY_CC(ix,iy,iz));
      F3(ff, 2, ix,iy,iz) = sqr(HZ_CC(ix,iy,iz));
    } foreach_3d_end;
  }
  psc_mfields_put_cf(flds, flds_base, 0, 0);
}

struct output_field {
  char *name;
  int nr_comp;
  char *fld_names[6];
  void (*calc)(struct psc *psc, mfields_base_t *flds, mparticles_base_t *particles,
	       mfields_c_t *f);
};

static void
calc_densities(struct psc *psc, mfields_base_t *flds, mparticles_base_t *particles,
	       mfields_c_t *res)
{
  psc_moments_calc_densities(psc->moments, flds, particles, res);
}

static void
calc_v(struct psc *psc, mfields_base_t *flds, mparticles_base_t *particles,
	       mfields_c_t *res)
{
  psc_moments_calc_v(psc->moments, flds, particles, res);
}

static void
calc_vv(struct psc *psc, mfields_base_t *flds, mparticles_base_t *particles,
	mfields_c_t *res)
{
  psc_moments_calc_vv(psc->moments, flds, particles, res);
}

static void
calc_photon_n(struct psc *psc, mfields_base_t *flds, mparticles_base_t *particles,
	      mfields_c_t *res)
{
  psc_moments_calc_photon_n(psc->moments, psc->mphotons, res);
}

static struct output_field output_fields[] = {
  { .name = "n"    , .nr_comp = 3, .fld_names = { "ne", "ni", "nn" },
    .calc = calc_densities },
  { .name = "v"    , .nr_comp = 6, .fld_names = { "vex", "vey", "vez", "vix", "viy", "viz" },
    .calc = calc_v },
  { .name = "vv"   , .nr_comp = 6, .fld_names = { "vexvex", "veyvey", "vezvez",
						  "vixvix", "viyviy", "vizviz" },
    .calc = calc_vv },
  { .name = "j"    , .nr_comp = 3, .fld_names = { "jx", "jy", "jz" },
    .calc = calc_j },
  { .name = "e"    , .nr_comp = 3, .fld_names = { "ex", "ey", "ez" },
    .calc = calc_E },
  { .name = "h"    , .nr_comp = 3, .fld_names = { "hx", "hy", "hz" },
    .calc = calc_H },
  { .name = "jdote", .nr_comp = 3, .fld_names = { "jxex", "jyey", "jzez" },
    .calc = calc_jdote },
  { .name = "poyn" , .nr_comp = 3, .fld_names = { "poynx", "poyny", "poynz" },
    .calc = calc_poyn },
  { .name = "e2"   , .nr_comp = 3, .fld_names = { "ex2", "ey2", "ez2" },
    .calc = calc_E2 },
  { .name = "h2"   , .nr_comp = 3, .fld_names = { "hx2", "hy2", "hz2" },
    .calc = calc_H2 },
  { .name = "photon_n", .nr_comp = 1, .fld_names = { "photon_n" },
    .calc = calc_photon_n },
  {},
};

static struct output_field *
find_output_field(const char *name)
{
  for (int i = 0; output_fields[i].name; i++) {
    struct output_field *of = &output_fields[i];
    if (strcasecmp(of->name, name) == 0) {
      return of;
    }
  }
  fprintf(stderr, "ERROR: output_field '%s' unknown!\n", name);
  abort();
}

// ----------------------------------------------------------------------
// psc_output_fields_c_create

static void
psc_output_fields_c_create(struct psc_output_fields *out)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  out_c->format = psc_output_format_create(psc_output_fields_comm(out));
}

// ----------------------------------------------------------------------
// psc_output_fields_c_destroy

static void
psc_output_fields_c_destroy(struct psc_output_fields *out)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);

  struct psc_fields_list *pfd = &out_c->pfd;
  for (int i = 0; i < pfd->nr_flds; i++) {
    psc_mfields_list_del(&psc_mfields_base_list, &pfd->flds[i]);
    psc_mfields_destroy(pfd->flds[i]);
  }
  struct psc_fields_list *tfd = &out_c->tfd;
  for (int i = 0; i < tfd->nr_flds; i++) {
    psc_mfields_list_del(&psc_mfields_base_list, &tfd->flds[i]);
    psc_mfields_destroy(tfd->flds[i]);
  }

  psc_output_format_destroy(out_c->format);
}

// ----------------------------------------------------------------------
// psc_output_fields_c_set_from_options

static void
psc_output_fields_c_set_from_options(struct psc_output_fields *out)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  psc_output_format_set_from_options(out_c->format);
}

// ----------------------------------------------------------------------
// psc_output_fields_c_setup

static void
psc_output_fields_c_setup(struct psc_output_fields *out)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  struct psc *psc = out->psc;

  out_c->pfield_next = out_c->pfield_first;
  out_c->tfield_next = out_c->tfield_first;

  struct psc_fields_list *pfd = &out_c->pfd;

  // setup pfd according to output_fields as given
  // (potentially) on the command line
  pfd->nr_flds = 0;
  // parse comma separated list of fields
  char *s_orig = strdup(out_c->output_fields), *p, *s = s_orig;
  while ((p = strsep(&s, ", "))) {
    struct output_field *of = find_output_field(p);
    mfields_c_t *flds = psc_mfields_create(mrc_domain_comm(psc->mrc_domain));
    psc_mfields_set_type(flds, "c");
    psc_mfields_set_domain(flds, psc->mrc_domain);
    psc_mfields_set_param_int(flds, "nr_fields", of->nr_comp);
    psc_mfields_set_param_int3(flds, "ibn", psc->ibn);
    psc_mfields_setup(flds);
    out_c->out_flds[pfd->nr_flds] = of;
    pfd->flds[pfd->nr_flds] = flds;
    // FIXME, should be del'd eventually
    psc_mfields_list_add(&psc_mfields_base_list, &pfd->flds[pfd->nr_flds]);
    pfd->nr_flds++;
    psc_foreach_patch(psc, pp) {
      for (int m = 0; m < of->nr_comp; m++) {
	psc_mfields_get_patch(flds, pp)->name[m] = strdup(of->fld_names[m]);
      }
    }
  }
  free(s_orig);

  // create tfd to look just like pfd
  // FIXME, only if necessary
  struct psc_fields_list *tfd = &out_c->tfd;
  tfd->nr_flds = pfd->nr_flds;
  for (int i = 0; i < pfd->nr_flds; i++) {
    assert(psc->nr_patches > 0);
    mfields_c_t *flds = psc_mfields_create(mrc_domain_comm(psc->mrc_domain));
    psc_mfields_set_type(flds, "c");
    psc_mfields_set_domain(flds, psc->mrc_domain);
    psc_mfields_set_param_int(flds, "nr_fields", pfd->flds[i]->nr_fields);
    psc_mfields_set_param_int3(flds, "ibn", psc->ibn);
    psc_mfields_setup(flds);
    tfd->flds[i] = flds;
    // FIXME, should be del'd eventually
    psc_mfields_list_add(&psc_mfields_base_list, &tfd->flds[i]);
    psc_foreach_patch(psc, pp) {
      for (int m = 0; m < pfd->flds[i]->nr_fields; m++) {
	psc_mfields_get_patch(flds, pp)->name[m] = 
	  strdup(psc_mfields_get_patch(pfd->flds[i], pp)->name[m]);
      }
    }
  }
  out_c->naccum = 0;
}

// ----------------------------------------------------------------------
// psc_output_fields_c_view

static void
psc_output_fields_c_view(struct psc_output_fields *out)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  psc_output_format_view(out_c->format);
}

// ----------------------------------------------------------------------
// psc_output_fields_c_write

static void
psc_output_fields_c_write(struct psc_output_fields *out, struct mrc_io *io)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  const char *path = psc_output_fields_name(out);
  mrc_io_write_attr_int(io, path, "pfield_next", out_c->pfield_next);
  mrc_io_write_attr_int(io, path, "tfield_next", out_c->tfield_next);
}

// ----------------------------------------------------------------------
// psc_output_fields_c_read

static void
psc_output_fields_c_read(struct psc_output_fields *out, struct mrc_io *io)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  const char *path = psc_output_fields_name(out);
  
  // FIXME, this is very hacky, instead of restoring state, we'll put
  // next into first and setup() will do the right thing. This is because
  // we can't call setup() right now, since other stuff in psc isn't set up
  // yet :(((
  mrc_io_read_attr_int(io, path, "pfield_next", &out_c->pfield_first);
  mrc_io_read_attr_int(io, path, "tfield_next", &out_c->tfield_first);
}

// ----------------------------------------------------------------------
// make_fields_list

static void
make_fields_list(struct psc *psc, struct psc_fields_list *list,
		 struct psc_fields_list *list_in)
{
  // the only thing this still does is to flatten
  // the list so that it only contains 1-component entries
  // FIXME, slow and unnec

  list->nr_flds = 0;
  for (int i = 0; i < list_in->nr_flds; i++) {
    mfields_c_t *flds_in = list_in->flds[i];
    for (int m = 0; m < flds_in->nr_fields; m++) {
      mfields_c_t *flds = psc_mfields_create(psc_comm(psc));
      psc_mfields_set_type(flds, "c");
      psc_mfields_set_domain(flds, psc->mrc_domain);
      psc_mfields_set_param_int3(flds, "ibn", psc->ibn);
      psc_mfields_setup(flds);
      psc_mfields_copy_comp(flds, 0, flds_in, m);
      list->flds[list->nr_flds++] = flds;
      psc_foreach_patch(psc, p) {
	psc_mfields_get_patch(flds, p)->name[0] = strdup(psc_mfields_get_patch(flds_in, p)->name[m]);
      }
    }
  }
}

// ----------------------------------------------------------------------
// free_fields_list

static void
free_fields_list(struct psc *psc, struct psc_fields_list *list)
{
  for (int m = 0; m < list->nr_flds; m++) {
    psc_mfields_destroy(list->flds[m]);
  }
}

// ----------------------------------------------------------------------
// psc_output_fields_c_run

static void
psc_output_fields_c_run(struct psc_output_fields *out,
			mfields_base_t *flds, mparticles_base_t *particles)
{
  struct psc_output_fields_c *out_c = to_psc_output_fields_c(out);
  struct psc *psc = out->psc;

  static int pr;
  if (!pr) {
    pr = prof_register("output_c_field", 1., 0, 0);
  }
  prof_start(pr);

  if ((out_c->dowrite_pfield && psc->timestep >= out_c->pfield_next) ||
      out_c->dowrite_tfield) {
    struct psc_fields_list *pfd = &out_c->pfd;
    for (int i = 0; i < pfd->nr_flds; i++) {
      out_c->out_flds[i]->calc(psc, flds, particles, pfd->flds[i]);
    }
  }
  
  if (out_c->dowrite_pfield) {
    if (psc->timestep >= out_c->pfield_next) {
       out_c->pfield_next += out_c->pfield_step;
       struct psc_fields_list flds_list;
       make_fields_list(psc, &flds_list, &out_c->pfd);
       psc_output_format_write_fields(out_c->format, out_c, &flds_list, "pfd");
       free_fields_list(psc, &flds_list);
    }
  }

  if (out_c->dowrite_tfield) {
    // tfd += pfd
    for (int m = 0; m < out_c->tfd.nr_flds; m++) {
      psc_mfields_axpy(out_c->tfd.flds[m], 1., out_c->pfd.flds[m]);
    }
    out_c->naccum++;
    if (psc->timestep >= out_c->tfield_next) {
      out_c->tfield_next += out_c->tfield_step;

      // convert accumulated values to correct temporal mean
      for (int m = 0; m < out_c->tfd.nr_flds; m++) {
	psc_mfields_scale(out_c->tfd.flds[m], 1. / out_c->naccum);
      }

      struct psc_fields_list flds_list;
      make_fields_list(psc, &flds_list, &out_c->tfd);
      psc_output_format_write_fields(out_c->format, out_c, &flds_list, "tfd");
      free_fields_list(psc, &flds_list);
      for (int m = 0; m < out_c->tfd.nr_flds; m++) {
	for (int mm = 0; mm < out_c->tfd.flds[m]->nr_fields; mm++) {
	  psc_mfields_zero(out_c->tfd.flds[m], mm);
	}
      }
      out_c->naccum = 0;
    }
  }
  
  prof_stop(pr);
}

// ======================================================================
// psc_output_fields: subclass "c"

#define VAR(x) (void *)offsetof(struct psc_output_fields_c, x)

// FIXME pfield_out_[xyz]_{min,max} aren't for pfield only, better init to 0,
// use INT3

static struct param psc_output_fields_c_descr[] = {
  { "data_dir"           , VAR(data_dir)             , PARAM_STRING(".")       },
  { "output_fields"      , VAR(output_fields)        , PARAM_STRING("n,j,e,h") },
  { "write_pfield"       , VAR(dowrite_pfield)       , PARAM_BOOL(1)           },
  { "pfield_first"       , VAR(pfield_first)         , PARAM_INT(0)            },
  { "pfield_step"        , VAR(pfield_step)          , PARAM_INT(10)           },
  { "write_tfield"       , VAR(dowrite_tfield)       , PARAM_BOOL(1)           },
  { "tfield_first"       , VAR(tfield_first)         , PARAM_INT(0)            },
  { "tfield_step"        , VAR(tfield_step)          , PARAM_INT(10)           },
  { "pfield_out_x_min"   , VAR(rn[0])                , PARAM_INT(0)            },  
  { "pfield_out_x_max"   , VAR(rx[0])                , PARAM_INT(1000000000)  },     // a big number to change it later to domain.ihi or command line number
  { "pfield_out_y_min"   , VAR(rn[1])                , PARAM_INT(0)           }, 
  { "pfield_out_y_max"   , VAR(rx[1])                , PARAM_INT(1000000000)  },
  { "pfield_out_z_min"   , VAR(rn[2])                , PARAM_INT(0)            }, 
  { "pfield_out_z_max"   , VAR(rx[2])                , PARAM_INT(1000000000)  },
  {},
};
#undef VAR

struct psc_output_fields_ops psc_output_fields_c_ops = {
  .name                  = "c",
  .size                  = sizeof(struct psc_output_fields_c),
  .param_descr           = psc_output_fields_c_descr,
  .create                = psc_output_fields_c_create,
  .setup                 = psc_output_fields_c_setup,
  .set_from_options      = psc_output_fields_c_set_from_options,
  .destroy               = psc_output_fields_c_destroy,
  .write                 = psc_output_fields_c_write,
  .read                  = psc_output_fields_c_read,
  .view                  = psc_output_fields_c_view,
  .run                   = psc_output_fields_c_run,
};
