
struct d_consts {
  real dt;
  real dxi[3];
  real dqs;
  real fnqs;
  real fnqys, fnqzs;
  int mx[3];
  int ilg[3];
  int ilo[3];
  int b_mx[3];
};

__constant__ static struct d_consts d_consts;

EXTERN_C void
PFX(set_constants)(struct psc_particles *prts, struct psc_fields *pf)
{
  struct d_consts consts = {
    .dt     = ppsc->dt,
    .dxi    = { 1.f / ppsc->patch[pf->p].dx[0],
		1.f / ppsc->patch[pf->p].dx[1],
		1.f / ppsc->patch[pf->p].dx[2] },
    .dqs    = .5f * ppsc->coeff.eta * ppsc->dt,
  };
  consts.fnqs   = sqr(ppsc->coeff.alpha) * ppsc->coeff.cori / ppsc->coeff.eta;
  consts.fnqys  = ppsc->patch[pf->p].dx[1] * consts.fnqs / ppsc->dt;
  consts.fnqzs  = ppsc->patch[pf->p].dx[2] * consts.fnqs / ppsc->dt;
  for (int d = 0; d < 3; d++) {
    consts.mx[d] = pf->im[d];
    consts.ilg[d] = pf->ib[d];
    consts.ilo[d] = pf->ib[d] + ppsc->ibn[d];
  }

  if (prts) {
    struct psc_particles_cuda *cuda = psc_particles_cuda(prts);
    for (int d = 0; d < 3; d++) {
      consts.b_mx[d] = cuda->b_mx[d];
    }
  }

  check(cudaMemcpyToSymbol(d_consts, &consts, sizeof(consts)));
}

