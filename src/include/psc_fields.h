
#ifndef PSC_FIELDS_H
#define PSC_FIELDS_H

// ----------------------------------------------------------------------
// psc_fields class

MRC_CLASS_DECLARE(psc_fields, struct psc_fields);

unsigned int psc_fields_size(struct psc_fields *pf);
void psc_fields_zero_comp(struct psc_fields *pf, int m);
void psc_fields_set_comp(struct psc_fields *pf, int m, double alpha);
void psc_fields_scale_comp(struct psc_fields *pf, int m, double alpha);
void psc_fields_copy_comp(struct psc_fields *to, int mto,
			  struct psc_fields *from, int mfrom);
void psc_fields_axpy_comp(struct psc_fields *yf, int ym, double alpha,
			  struct psc_fields *xf, int xm);

// ----------------------------------------------------------------------
// psc_mfields class

struct psc_mfields {
  struct mrc_obj obj;
  struct psc_fields **flds;
  int nr_patches;
  struct mrc_domain *domain;
  int nr_fields; //> number of field components
  char **comp_name; //> name for each field component
  int ibn[3];
  int first_comp; //> The first component in this field (normally 0)
};

MRC_CLASS_DECLARE(psc_mfields, struct psc_mfields);

struct psc_mfields_ops {
  MRC_SUBCLASS_OPS(struct psc_mfields);
};

void psc_mfields_set_domain(struct psc_mfields *flds,
			    struct mrc_domain *domain);
void psc_mfields_zero_comp(struct psc_mfields *flds, int m);
void psc_mfields_zero_range(struct psc_mfields *flds, int mb, int me);
void psc_mfields_set_comp(struct psc_mfields *flds, int m, double alpha);
void psc_mfields_scale(struct psc_mfields *flds, double alpha);
void psc_mfields_copy_comp(struct psc_mfields *to, int mto,
			   struct psc_mfields *from, int mfrom);
void psc_mfields_axpy(struct psc_mfields *yf, double alpha,
		      struct psc_mfields *xf);
void psc_mfields_axpy_comp(struct psc_mfields *yf, int ym, double alpha,
			   struct psc_mfields *xf, int xm);
void psc_mfields_set_comp_name(struct psc_mfields *flds, int m, const char *s);
const char *psc_mfields_comp_name(struct psc_mfields *flds, int m);

static inline struct psc_fields *
psc_mfields_get_patch(struct psc_mfields *flds, int p)
{
  return flds->flds[p];
}

#define MAKE_MFIELDS_TYPE(type)						\
typedef struct psc_mfields mfields_##type##_t;			        \
extern struct psc_mfields_ops psc_mfields_##type##_ops;		        \
									\
struct psc_mfields *						        \
psc_mfields_get_##type(struct psc_mfields *mflds_base, int mb, int me); \
void psc_mfields_put_##type(struct psc_mfields *mflds,			\
			    struct psc_mfields *mflds_base,		\
			    int mb, int me);				\

MAKE_MFIELDS_TYPE(c)
MAKE_MFIELDS_TYPE(fortran)
MAKE_MFIELDS_TYPE(single)
MAKE_MFIELDS_TYPE(cuda)
typedef struct psc_mfields mfields_base_t;

struct psc_mfields_list_entry {
  struct psc_mfields **flds_p;
  list_t entry;
};

void psc_mfields_list_add(list_t *head, struct psc_mfields **flds_p);
void psc_mfields_list_del(list_t *head, struct psc_mfields **flds_p);

#define psc_mfields_ops(flds) (struct psc_mfields_ops *) ((flds)->obj.ops)

extern list_t psc_mfields_base_list;

#endif


