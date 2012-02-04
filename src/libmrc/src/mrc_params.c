
#include <mrc_common.h>
#include <mrc_params.h>
#include <mrc_list.h>
#include <mrc_io.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

struct option {
  char *name;
  char *value;
  bool used;
  list_t entry;
};

static LIST_HEAD(option_list);
static bool do_print_help;
static int mpi_rank = -1;

static void __attribute__ ((format (printf, 1, 2)))
print_help(const char *fmt, ...)
{
  if (do_print_help && mpi_rank == 0) {
    va_list ap;
    va_start(ap, fmt); 
    printf("[0] ");
    vprintf(fmt, ap);
    va_end(ap);
  }
}

static void __attribute__ ((format (printf, 1, 2)))
error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt); 
  fprintf(stderr, "[%d] ERROR: ", mpi_rank);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(-1);
}

static void __attribute__ ((format (printf, 1, 2)))
warn(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt); 
  fprintf(stderr, "[%d] WARNING: ", mpi_rank);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

void
libmrc_params_init(int argc, char **argv)
{
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--", 2) != 0) {
      error("expected argument '%s' to specify an option like '--something'\n",
	    argv[i]);
    }
    struct option *opt = malloc(sizeof(*opt));
    opt->name = strdup(argv[i] + 2);
    opt->value = NULL;
    opt->used = false;

    // another arg left, which doesn't start with --
    if (i < argc - 1 && strncmp(argv[i+1], "--", 2) != 0) {
      opt->value = strdup(argv[i+1]);
      i++;
    }
    list_add_tail(&opt->entry, &option_list);
  }

  // check whether "--help" is given
  mrc_params_get_option_bool("help", &do_print_help);
}

void
libmrc_params_finalize()
{
  while (!list_empty(&option_list)) {
    struct option *opt = list_entry(option_list.next, struct option, entry);
    if(!opt->used)
      mpi_printf(MPI_COMM_WORLD,"Did not use given parameter: %s\n",opt->name);
    list_del(&opt->entry);
    free(opt->name);
    free(opt->value);
    free(opt);
  }
}

void
mrc_params_insert_option(const char *name, const char *val)
{
  struct option *opt = malloc(sizeof(*opt));
  opt->name = strdup(name);
  opt->value = val ? strdup(val) : NULL;
  opt->used = false;
  list_add_tail(&opt->entry, &option_list);
}

void
mrc_params_print_all(MPI_Comm comm)
{
  mpi_printf(comm, "%-20s| %s\n", "parameter", "value");
  mpi_printf(comm, "--------------------+----------------------------------------\n");
  struct option *p;
  __list_for_each_entry(p, &option_list, entry, struct option) {
    mpi_printf(comm, "%-20s| %s\n", p->name, p->value);
  }
  mpi_printf(comm, "\n");
}

static struct option *
find_option(const char *name, bool deprecated)
{
  struct option *p;
  __list_for_each_entry(p, &option_list, entry, struct option) {
    if (strcmp(p->name, name) == 0) {
      p->used = true;
      if (deprecated) {
	warn("option --%s is deprecated! You probably need to add a proper prefix.\n", name);
      }
      return p;
    }
  }
  return NULL;
}

static int
_mrc_params_get_option_int(const char *name, int *pval, bool deprecated, const char *help)
{
  struct option *p = find_option(name, deprecated);
  
  if (p) {
    int rv = sscanf(p->value, "%d", pval);
    if (rv != 1) {
      error("cannot parse int from '%s'\n", p->value);
    }
    print_help("--%s: <%d> %s\n", name, *pval, help ? help : "");
    return 0;
  } else {
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: <%d> (default) %s\n", name, *pval, help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_int(const char *name, int *pval)
{
  return _mrc_params_get_option_int(name, pval, false, NULL);
}

int
mrc_params_get_option_int_help(const char *name, int *pval, const char *help)
{
  return _mrc_params_get_option_int(name, pval, false, help);
}

int
_mrc_params_get_option_float(const char *name, float *pval, bool deprecated,
			     const char *help)
{
  struct option *p = find_option(name, deprecated);
  
  if (p) {
    int rv = sscanf(p->value, "%g", pval);
    if (rv != 1) {
      error("cannot parse float from '%s'\n", p->value);
    }
    print_help("--%s: <%g> %s\n", name, *pval, help ? help : "");
    return 0;
  } else {
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: <%g> (default) %s\n", name, *pval,
		 help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_float(const char *name, float *pval)
{
  return _mrc_params_get_option_float(name, pval, false, NULL);
}

int
mrc_params_get_option_float_help(const char *name, float *pval,
				 const char *help)
{
  return _mrc_params_get_option_float(name, pval, false, help);
}

int
_mrc_params_get_option_double(const char *name, double *pval, bool deprecated,
			      const char *help)
{
  struct option *p = find_option(name, deprecated);
  
  if (p) {
    int rv = sscanf(p->value, "%lg", pval);
    if (rv != 1) {
      error("cannot parse double from '%s'\n", p->value);
    }
    print_help("--%s: <%g> %s\n", name, *pval, help ? help : "");
    return 0;
  } else {
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: <%g> (default) %s\n", name, *pval,
		 help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_double(const char *name, double *pval)
{
  return _mrc_params_get_option_double(name, pval, false, NULL);
}

int
mrc_params_get_option_double_help(const char *name, double *pval,
				  const char *help)
{
  return _mrc_params_get_option_double(name, pval, false, help);
}

int
_mrc_params_get_option_string(const char *name, const char **pval, bool deprecated,
			      const char *help)
{
  struct option *p = find_option(name, deprecated);
  
  if (p) {
    *pval = p->value;
    print_help("--%s: <%s> %s\n", name, *pval ? *pval : "NULL",
	       help ? help : "");
    return 0;
  } else {
    // FIXME, *pval may not be initialized, in which case we'd crash trying to print it
    // we should disallow this use in the future.
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: (default) %s\n", name, help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_string(const char *name, const char **pval)
{
  return _mrc_params_get_option_string(name, pval, false, NULL);
}

int
mrc_params_get_option_string_help(const char *name, const char **pval,
				  const char *help)
{
  return _mrc_params_get_option_string(name, pval, false, help);
}

int
_mrc_params_get_option_bool(const char *name, bool *pval, bool deprecated,
			    const char *help)
{
  struct option *p = find_option(name, deprecated);

  if (p) {
    if (!p->value) { // just "--something"
      *pval = true;
    } else {
      if (strcasecmp(p->value, "yes") == 0 ||
	  strcasecmp(p->value, "true") == 0) {
	*pval = true;
      } else if (strcasecmp(p->value, "no") == 0 ||
		 strcasecmp(p->value, "false") == 0) {
	*pval = false;
      } else {
	error("cannot parse bool from '%s'\n", p->value);
      }
    }
    print_help("--%s: <%s> %s\n", name, *pval ? "true" : "false",
	       help ? help : "");
    return 0;
  } else {
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: <%s> (default) %s\n", name, *pval ? "true" : "false",
		 help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_bool(const char *name, bool *pval)
{
  return _mrc_params_get_option_bool(name, pval, false, NULL);
}

int
mrc_params_get_option_bool_help(const char *name, bool *pval, const char *help)
{
  return _mrc_params_get_option_bool(name, pval, false, help);
}

int
_mrc_params_get_option_select(const char *name, struct mrc_param_select *descr,
			      int *pval, bool deprecated, const char *help)
{
  struct option *p = find_option(name, deprecated);
  if (p)  {
    if (!p->value) {
      error("need to specify value for '%s'\n", name);
    }
    
    for (int i = 0; descr[i].str; i++) {
      if (strcasecmp(descr[i].str, p->value) == 0) {
	*pval = descr[i].val;
	print_help("--%s: <%s> %s\n", name, descr[i].str, help ? help : "");
	return 0;
      }
    }
    
    fprintf(stderr, "ERROR: Select value '%s' not found. Valid options are:",
	    p->value);
    for (int i = 0; descr[i].str; i++) {
      fprintf(stderr, " '%s'", descr[i].str);
    }
    fprintf(stderr, "\n");
    exit(-1);
  } else {
    for (int i = 0; descr[i].str; i++) {
      if (*pval == descr[i].val) {
	print_help("--%s: <%s> (default) %s\n", name, descr[i].str, help ? help : "");
	return -1;
      }
    }
    // FIXME, can happen if no default was set, but we should disallow this.
    if (!deprecated) { // don't advertise deprecated un-prefixed options
      print_help("--%s: (default) %s\n", name, help ? help : "");
    }
    return -1;
  }
}

int
mrc_params_get_option_select(const char *name, struct mrc_param_select *descr,
			     int *pval)
{
  return _mrc_params_get_option_select(name, descr, pval, false, NULL);
}

int
mrc_params_get_option_select_help(const char *name, struct mrc_param_select *descr,
				  int *pval, const char *help)
{
  return _mrc_params_get_option_select(name, descr, pval, false, help);
}

int
_mrc_params_get_option_int3(const char *name, int *pval, bool deprecated,
			    const char *help)
{
  int retval = -1;
  char namex[strlen(name) + 2];
  for (int d = 0; d < 3; d++) {
    sprintf(namex, "%s%c", name, 'x' + d);
    struct option *p = find_option(namex, deprecated);
  
    if (!p) {
      if (!deprecated) { // don't advertise deprecated un-prefixed options
	print_help("--%s: <%d> (default) %s\n", namex, pval[d],
		   help ? help : "");
      }
      continue;
    }
    
    retval = 0;
    int rv = sscanf(p->value, "%d", pval + d);
    if (rv != 1) {
      error("cannot parse integer from '%s'\n", p->value);
    }
    print_help("--%s: <%d> %s\n", namex, pval[d], help ? help : "");
  }
  return retval;
}

int
mrc_params_get_option_int3(const char *name, int *pval)
{
  return _mrc_params_get_option_int3(name, pval, false, NULL);
}

int
mrc_params_get_option_int3_help(const char *name, int *pval,
				const char *help)
{
  return _mrc_params_get_option_int3(name, pval, false, help);
}

int
_mrc_params_get_option_float3(const char *name, float *pval, bool deprecated,
			      const char *help)
{
  int retval = -1;
  char namex[strlen(name) + 2];
  for (int d = 0; d < 3; d++) {
    sprintf(namex, "%s%c", name, 'x' + d);
    struct option *p = find_option(namex, deprecated);
  
    if (!p) {
      if (!deprecated) { // don't advertise deprecated un-prefixed options
	print_help("--%s: <%g> (default) %s\n", namex, pval[d],
		   help ? help : "");
      }
      continue;
    }

    retval = 0;
    int rv = sscanf(p->value, "%g", pval + d);
    if (rv != 1) {
      fprintf(stderr, "error: cannot parse float from '%s'\n", p->value);
    }
    print_help("--%s: <%g> %s\n", namex, pval[d], help ? help : "");
  }
  return retval;
}

int
mrc_params_get_option_float3(const char *name, float *pval)
{
  return _mrc_params_get_option_float3(name, pval, false, NULL);
}

int
mrc_params_get_option_float3_help(const char *name, float *pval,
				  const char *help)
{
  return _mrc_params_get_option_float3(name, pval, false, help);
}

int
_mrc_params_get_option_double3(const char *name, double *pval, bool deprecated,
			       const char *help)
{
  int retval = -1;
  char namex[strlen(name) + 2];
  for (int d = 0; d < 3; d++) {
    sprintf(namex, "%s%c", name, 'x' + d);
    struct option *p = find_option(namex, deprecated);
  
    if (!p) {
      if (!deprecated) { // don't advertise deprecated un-prefixed options
	print_help("--%s: <%g> (default) %s\n", namex, pval[d],
		   help ? help : "");
      }
      continue;
    }

    retval = 0;
    int rv = sscanf(p->value, "%lg", pval + d);
    if (rv != 1) {
      fprintf(stderr, "error: cannot parse double from '%s'\n", p->value);
    }
    print_help("--%s: <%g> %s\n", namex, pval[d], help ? help : "");
  }
  return retval;
}

int
mrc_params_get_option_double3(const char *name, double *pval)
{
  return _mrc_params_get_option_double3(name, pval, false, NULL);
}

int
mrc_params_get_option_double3_help(const char *name, double *pval,
				   const char *help)
{
  return _mrc_params_get_option_double3(name, pval, false, help);
}

void
mrc_params_get_option_ptr(const char *name, void** pval)
{
  assert(0);
}

void
mrc_params_set_default(void *p, struct param *params)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    switch (params[i].type) {
    case PT_INT:
      pv->u_int = params[i].u.ini_int;
      break;
    case PT_BOOL:
      pv->u_bool = params[i].u.ini_bool;
      break;
    case PT_FLOAT:
      pv->u_float = params[i].u.ini_float;
      break;
    case PT_DOUBLE:
      pv->u_double = params[i].u.ini_double;
      break;
    case PT_STRING:
      pv->u_string = params[i].u.ini_string;
      break;
    case PT_SELECT:
      pv->u_select = params[i].u.ini_select;
      break;
    case PT_INT3:
      for (int d = 0; d < 3; d++) {
	pv->u_int3[d] = params[i].u.ini_int3[d];
      }
      break;
    case PT_FLOAT3:
      for (int d = 0; d < 3; d++) {
	pv->u_float3[d] = params[i].u.ini_float3[d];
      }
      break;
    case PT_DOUBLE3:
      for (int d = 0; d < 3; d++) {
	pv->u_double3[d] = params[i].u.ini_double3[d];
      }
      break;
    case PT_PTR:
      pv->u_ptr = params[i].u.ini_ptr;
      break;
    default:
      assert(0);
    }
  }
}

int
mrc_params_set_type(void *p, struct param *params, const char *name,
		    int type, union param_u *pval)
{
  for (int i = 0; params[i].name; i++) {
    if (strcmp(params[i].name, name) != 0)
      continue;

    union param_u *pv = p + (unsigned long) params[i].var;
    // types have to match, except a PT_SELECT can be set by a PT_INT
    if (params[i].type != type &&
	!(params[i].type == PT_SELECT && type == PT_INT)) {
      error("option '%s' is not of type %d!\n", name, type);
    }
    switch (type) {
    case PT_INT:
      pv->u_int = pval->u_int;
      break;
    case PT_FLOAT:
      pv->u_float = pval->u_float;
      break;
    case PT_DOUBLE:
      pv->u_double = pval->u_double;
      break;
    case PT_STRING:
      pv->u_string = pval->u_string;
      break;
    case PT_BOOL:
      pv->u_bool = pval->u_bool;
      break;
    case PT_SELECT:
      pv->u_select = pval->u_select;
      break;
    case PT_INT3:
      for (int d = 0; d < 3; d++) {
	pv->u_int3[d] = pval->u_int3[d];
      }
      break;
    case PT_FLOAT3:
      for (int d = 0; d < 3; d++) {
	pv->u_float3[d] = pval->u_float3[d];
      }
      break;
    case PT_DOUBLE3:
      for (int d = 0; d < 3; d++) {
	pv->u_double3[d] = pval->u_double3[d];
      }
      break;
    case PT_PTR:
      pv->u_ptr = pval->u_ptr;
      break;
    default:
      assert(0);
    }
    return 0;
  }
  return -1; // not found
}

int
mrc_params_get_type(void *p, struct param *params, const char *name,
		    int type, union param_u *pval)
{
  for (int i = 0; params[i].name; i++) {
    if (strcmp(params[i].name, name) != 0)
      continue;

    union param_u *pv = p + (unsigned long) params[i].var;
    // types have to match, except a PT_SELECT can be set by a PT_INT
    if (params[i].type != type &&
	!(params[i].type == PT_SELECT && type == PT_INT)) {
      error("option '%s' is not of type %d!\n", name, type);
    }
    switch (type) {
    case PT_INT:
      pval->u_int = pv->u_int;
      break;
    case PT_FLOAT:
      pval->u_float = pv->u_float;
      break;
    case PT_STRING:
      pval->u_string = pv->u_string;
      break;
    case PT_SELECT:
      pval->u_select = pv->u_select;
      break;
    case PT_INT3:
      for (int d = 0; d < 3; d++) {
	pval->u_int3[d] = pv->u_int3[d];
      }
      break;
    case PT_PTR:
      pval->u_ptr = pv->u_ptr;
      break;
    default:
      assert(0);
    }
    return 0;
  }
  return -1; // not found
}

int
mrc_params_set_int(void *p, struct param *params, const char *name, int val)
{
  union param_u uval = { .u_int = val };
  return mrc_params_set_type(p, params, name, PT_INT, &uval);
}

int
mrc_params_set_bool(void *p, struct param *params, const char *name, bool val)
{
  union param_u uval = { .u_bool = val };
  return mrc_params_set_type(p, params, name, PT_BOOL, &uval);
}

int
mrc_params_set_string(void *p, struct param *params, const char *name, const char *val)
{
  union param_u uval = { .u_string = val };
  return mrc_params_set_type(p, params, name, PT_STRING, &uval);
}

void
mrc_params_parse(void *p, struct param *params, const char *title,
		 MPI_Comm comm)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    switch (params[i].type) {
    case PT_INT:
      pv->u_int = params[i].u.ini_int;
      mrc_params_get_option_int(params[i].name, &pv->u_int);
      break;
    case PT_BOOL:
      pv->u_bool = params[i].u.ini_bool;
      mrc_params_get_option_bool(params[i].name, &pv->u_bool);
      break;
    case PT_FLOAT:
      pv->u_float = params[i].u.ini_float;
      mrc_params_get_option_float(params[i].name, &pv->u_float);
      break;
    case PT_DOUBLE:
      pv->u_double = params[i].u.ini_double;
      mrc_params_get_option_double(params[i].name, &pv->u_double);
      break;
    case PT_STRING:
      pv->u_string = params[i].u.ini_string;
      mrc_params_get_option_string(params[i].name, &pv->u_string);
      break;
    case PT_SELECT:
      pv->u_select = params[i].u.ini_select;
      mrc_params_get_option_select(params[i].name, params[i].descr, &pv->u_select);
      break;
    default:
      assert(0);
    }
  }
}

void
mrc_params_parse_nodefault(void *p, struct param *params, const char *title,
			   MPI_Comm comm)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    switch (params[i].type) {
    case PT_INT:
      _mrc_params_get_option_int(params[i].name, &pv->u_int, true, NULL);
      break;
    case PT_BOOL:
      _mrc_params_get_option_bool(params[i].name, &pv->u_bool, true, NULL);
      break;
    case PT_FLOAT:
      _mrc_params_get_option_float(params[i].name, &pv->u_float, true, NULL);
      break;
    case PT_DOUBLE:
      _mrc_params_get_option_double(params[i].name, &pv->u_double, true, NULL);
      break;
    case PT_STRING:
      _mrc_params_get_option_string(params[i].name, &pv->u_string, true, NULL);
      break;
    case PT_SELECT:
      _mrc_params_get_option_select(params[i].name, params[i].descr, &pv->u_select,
				    true, NULL);
      break;
    case PT_INT3:
      _mrc_params_get_option_int3(params[i].name, &pv->u_int3[0], true, NULL);
      break;
    case PT_FLOAT3:
      _mrc_params_get_option_float3(params[i].name, &pv->u_float3[0], true, NULL);
      break;
    case PT_DOUBLE3:
      _mrc_params_get_option_double3(params[i].name, &pv->u_double3[0], true, NULL);
      break;
    case PT_PTR:
      break;
    default:
      assert(0);
    }
  }
}

void
mrc_params_parse_pfx(void *p, struct param *params, const char *title,
		     MPI_Comm comm)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    char name[strlen(params[i].name) + strlen(title) + 2];
    sprintf(name, "%s_%s", title, params[i].name);
    switch (params[i].type) {
    case PT_INT:
      mrc_params_get_option_int_help(name, &pv->u_int, params[i].help);
      break;
    case PT_BOOL:
      mrc_params_get_option_bool_help(name, &pv->u_bool, params[i].help);
      break;
    case PT_FLOAT:
      mrc_params_get_option_float_help(name, &pv->u_float, params[i].help);
      break;
    case PT_DOUBLE:
      mrc_params_get_option_double_help(name, &pv->u_double, params[i].help);
      break;
    case PT_STRING:
      mrc_params_get_option_string_help(name, &pv->u_string, params[i].help);
      break;
    case PT_SELECT:
      mrc_params_get_option_select_help(name, params[i].descr, &pv->u_select,
					params[i].help);
      break;
    case PT_INT3:
      mrc_params_get_option_int3_help(name, &pv->u_int3[0], params[i].help);
      break;
    case PT_FLOAT3:
      mrc_params_get_option_float3_help(name, &pv->u_float3[0], params[i].help);
      break;
    case PT_DOUBLE3:
      mrc_params_get_option_double3_help(name, &pv->u_double3[0], params[i].help);
      break;
    case PT_PTR:
      break;
    default:
      assert(0);
    }
  }
}

void
mrc_params_print(void *p, struct param *params, const char *title, MPI_Comm comm)
{
  mpi_printf(comm, "\n");
  mpi_printf(comm, "%-20s| %s\n", "parameter", "value");
  mpi_printf(comm, "--------------------+---------------------------------------- %s\n", title);
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    switch (params[i].type) {
    case PT_INT:
      mpi_printf(comm, "%-20s| %d\n", params[i].name, pv->u_int);
      break;
    case PT_BOOL:
      mpi_printf(comm, "%-20s| %s\n", params[i].name, pv->u_bool ? "yes" : "no");
      break;
    case PT_FLOAT:
      mpi_printf(comm, "%-20s| %g\n", params[i].name, pv->u_float);
      break;
    case PT_DOUBLE:
      mpi_printf(comm, "%-20s| %g\n", params[i].name, pv->u_double);
      break;
    case PT_STRING:
      mpi_printf(comm, "%-20s| %s\n", params[i].name, pv->u_string);
      break;
    case PT_SELECT:
      mpi_printf(comm, "%-20s| %s\n", params[i].name,
		 params[i].descr[pv->u_select].str);
      break;
    case PT_INT3:
      mpi_printf(comm, "%-20s| %d, %d, %d\n", params[i].name,
		 pv->u_int3[0], pv->u_int3[1], pv->u_int3[2]);
      break;
    case PT_FLOAT3:
      mpi_printf(comm, "%-20s| %g, %g, %g\n", params[i].name,
		 pv->u_float3[0], pv->u_float3[1], pv->u_float3[2]);
      break;
    case PT_DOUBLE3:
      mpi_printf(comm, "%-20s| %g, %g, %g\n", params[i].name,
		 pv->u_double3[0], pv->u_double3[1], pv->u_double3[2]);
      break;
    case PT_PTR:
      mpi_printf(comm, "%-20s| %p\n", params[i].name, pv->u_ptr);
      break;
    }
  }
}

void
mrc_params_write(void *p, struct param *params, const char *title, struct mrc_io *io)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    mrc_io_write_attr(io, title, params[i].type, params[i].name, pv);
  }
}

void
mrc_params_read(void *p, struct param *params, const char *title, struct mrc_io *io)
{
  for (int i = 0; params[i].name; i++) {
    union param_u *pv = p + (unsigned long) params[i].var;
    mrc_io_read_attr(io, title, params[i].type, params[i].name, pv);
  }
}

