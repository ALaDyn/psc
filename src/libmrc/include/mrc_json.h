
#ifndef MRC_JSON_H
#define MRC_JSON_H

#include <unistd.h>

enum mrc_json_type {
  // FIXME? This have to match the json.h types, but I don't to want to include that here
  MRC_JSON_NONE,
  MRC_JSON_OBJECT,
  MRC_JSON_ARRAY,
  MRC_JSON_INTEGER,
  MRC_JSON_DOUBLE,
  MRC_JSON_STRING,
  MRC_JSON_BOOLEAN,
  MRC_JSON_NULL,
};

typedef struct mrc_json mrc_json_t;

struct mrc_json_ops {
  int (*get_type)(mrc_json_t json);
  int (*get_integer)(mrc_json_t json);
  double (*get_double)(mrc_json_t json);
  const char * (*get_string)(mrc_json_t json);
  unsigned int (*get_object_length)(mrc_json_t json);
  const char * (*get_object_entry_name)(mrc_json_t json, unsigned int i);
  mrc_json_t (*get_object_entry_value)(mrc_json_t json, unsigned int i);
  unsigned int (*get_array_length)(mrc_json_t json);
  mrc_json_t (*get_array_entry)(mrc_json_t json, unsigned int i);
};

struct mrc_json {
  void *value;
  struct mrc_json_ops *ops;
};

int mrc_json_get_type(mrc_json_t value);

int mrc_json_get_integer(mrc_json_t value);
double mrc_json_get_double(mrc_json_t value);
const char *mrc_json_get_string(mrc_json_t value);

unsigned int mrc_json_get_object_length(mrc_json_t value);
const char *mrc_json_get_object_entry_name(mrc_json_t value, unsigned int i);
mrc_json_t mrc_json_get_object_entry_value(mrc_json_t value, unsigned int i);

unsigned int mrc_json_get_array_length(mrc_json_t value);
mrc_json_t mrc_json_get_array_entry(mrc_json_t value, unsigned int i);

void mrc_json_print(mrc_json_t value, unsigned int depth);
char *mrc_json_to_string(mrc_json_t json);

// parse from actual JSON string
mrc_json_t mrc_json_parse(const char *buf);

#endif
