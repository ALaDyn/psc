
#ifndef MRC_FLD_AS_DOUBLE_H
#define MRC_FLD_AS_DOUBLE_H

typedef double mrc_fld_data_t;
#define mrc_fld_abs fabs
#define mrc_fld_max fmax

#define F3(f, m, i,j,k) MRC_D4(f, i,j,k, m)
#define M3(f, m, i,j,k, p) MRC_D5(f, i,j,k, m, p)
#define FLD_TYPE "double"

#endif
