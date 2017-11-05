
#ifndef CUDA_IFACE_BND_H
#define CUDA_IFACE_BND_H

#ifdef __cplusplus
extern "C" {
#endif
#if 0 // hack to fix indentation
}
#endif

void __fields_cuda_from_device_inside(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds,
				       int mb, int me);
void __fields_cuda_from_device_inside_only(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds,
					    int mb, int me);
void __fields_cuda_to_device_outside(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds,
				      int mb, int me);
void __fields_cuda_to_device_inside(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds,
				     int mb, int me);
void __fields_cuda_fill_ghosts_setup(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds);
void __fields_cuda_fill_ghosts_local(struct cuda_mfields_bnd *cbnd, struct cuda_mfields *cmflds,
				      int mb, int me);
void cuda_fill_ghosts_periodic_yz(struct cuda_mfields *cmflds, int p, int mb, int me);
void cuda_fill_ghosts_periodic_z(struct cuda_mfields *cmflds, int p, int mb, int me);
void cuda_add_ghosts_periodic_yz(struct cuda_mfields *cmflds, int p, int mb, int me);
void cuda_add_ghosts_periodic_z(struct cuda_mfields *cmflds, int p, int mb, int me);

// These fields are for field boundary exchange, so they could, eventually, miss the
// interior part

#undef F3_CF_BOUNDS_CHECK

#ifdef F3_CF_BOUNDS_CHECK
#define F3_CF_OFF(cf, fldnr, jx,jy,jz) ({				\
  assert(jx == 0); /* FIXME yz only! */				        \
  assert(jx >= (cf)->ib[0] && jx < (cf)->ib[0] + (cf)->im[0]);		\
  assert(jy >= (cf)->ib[1] && jy < (cf)->ib[1] + (cf)->im[1]);		\
  assert(jz >= (cf)->ib[2] && jz < (cf)->ib[2] + (cf)->im[2]);		\
  int __off = (((fldnr) * (cf)->im[2] + (jz)) * (cf)->im[1] + (jy)) * (cf)->im[0] + (jx); \
  __off; })

#else
#define F3_CF_OFF(cf, fldnr, jx,jy,jz)					\
  ((((fldnr) * (cf)->im[2] + (jz)) * (cf)->im[1] + (jy)) * (cf)->im[0] + (jx))
#endif

#define F3_CF(cf, fldnr, jx,jy,jz)					\
  ((cf)->arr_off[F3_CF_OFF(cf, fldnr, jx,jy,jz)])


#if 0
{
#endif
#ifdef __cplusplus
}
#endif

#endif
