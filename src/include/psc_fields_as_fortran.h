
#ifndef PSC_FIELDS_AS_FORTRAN_H
#define PSC_FIELDS_AS_FORTRAN_H

#include "psc_fields_fortran.h"

typedef fields_fortran_real_t fields_real_t;
typedef fields_fortran_t      fields_t;
#define fields_t_from_psc_fields fields_fortran_t_from_psc_fields
#define fields_t_mflds           fields_fortran_t_mflds
#define fields_t_zero_range      fields_fortran_t_zero_range

#define F3(pf, fldnr, jx,jy,jz) F3_FORTRAN(pf, fldnr, jx,jy,jz)
#define _F3(pf, fldnr, jx,jy,jz) _F3_FORTRAN(pf, fldnr, jx,jy,jz)

#define psc_mfields_get_from          psc_mfields_fortran_get_from
#define psc_mfields_put_to            psc_mfields_fortran_put_to
#define FIELDS_TYPE                   "fortran"

#define PSC_FIELDS_AS_FORTRAN 1

#endif
