
#include "profile.h"

#include <assert.h>
#include <string.h>

struct prof_data {
  const char *name;
  float simd;
  int flops;
  int bytes;
};

static int prof_inited;
static int nr_prof_data;
static struct prof_data prof_data[MAX_PROF];

#ifdef HAVE_LIBPAPI

#include <papi.h>

static const int events[] = {
  PAPI_TOT_CYC, // this one should be kept unchanged
  PAPI_TOT_INS, 
#ifdef PROF_UOPS
  0x4000805b,
  0x4000405b,
#else
  PAPI_FP_OPS,
#endif
};

void
prof_init(void)
{
  int rc;

  if (prof_inited)
    return;

  // assert(ARRAYSIZE(events) == NR_EVENTS);

  rc = PAPI_library_init(PAPI_VER_CURRENT);
  assert(rc == PAPI_VER_CURRENT);

  memset(&prof_globals, 0, sizeof(prof_globals));
  prof_globals.event_set = PAPI_NULL;
  rc = PAPI_create_eventset(&prof_globals.event_set);
  assert(rc == PAPI_OK);
  
  for (int i = 0; i < NR_EVENTS; i++) {
    rc = PAPI_add_event(prof_globals.event_set, events[i]);
    assert(rc == PAPI_OK);
  }

  rc = PAPI_start(prof_globals.event_set);
  assert(rc == PAPI_OK);

  prof_inited = 1;
}

void
prof_print_file(FILE *f)
{
  int rc;

  fprintf(f, "%15s %7s %4s %7s", "", "tottime", "cnt", "time");
  for (int i = 0; i < NR_EVENTS; i++) {
    char name[200];
    rc = PAPI_event_code_to_name(events[i], name);
    assert(rc == PAPI_OK);
    fprintf(f, " %12s", name);
  }
  fprintf(f, " %12s", "FLOPS");
  fprintf(f, " %12s", "MBytes");
  fprintf(f, "\n");

  for (int pr = 0; pr < nr_prof_data; pr++) {
    struct prof_info *pinfo = &prof_globals.info[pr];
    int cnt = pinfo->cnt;
    if (!cnt)
      continue;

    long long cycles = pinfo->counters[0];
    float rtime = pinfo->time;
    fprintf(f, "%15s %7g %4d %7g", prof_data[pr].name, rtime/1e3, cnt, 
	    rtime / 1e3 / cnt);
    for (int i = 0; i < NR_EVENTS; i++) {
      long long counter = pinfo->counters[i];
      if (events[i] == PAPI_FP_OPS) {
	counter *= prof_data[pr].simd;
      }
      fprintf(f, " %12lld", counter / cnt);
    }
    fprintf(f, " %12d", prof_data[pr].flops);
    fprintf(f, " %12g", prof_data[pr].bytes / 1e6);
    fprintf(f, "\n");

    fprintf(f, "%15s %7s %4s %7s", "", "", "", "");
    for (int i = 0; i < NR_EVENTS; i++) {
      long long counter = pinfo->counters[i];
      if (events[i] == PAPI_FP_OPS) {
	counter *= prof_data[pr].simd;
      }
      fprintf(f, " %12g", (float) counter / cycles);
    }
    fprintf(f, " %12g", (float) prof_data[pr].flops / (cycles/cnt));
    fprintf(f, " %12g", (float) prof_data[pr].bytes / (cycles/cnt));
    fprintf(f, " / cycle\n");

    fprintf(f, "%15s %7s %4s %7s", "", "", "", "");
    for (int i = 0; i < NR_EVENTS; i++) {
      long long counter = pinfo->counters[i];
      if (events[i] == PAPI_FP_OPS) {
	counter *= prof_data[pr].simd;
      }
      fprintf(f, " %12g", counter / rtime);
    }
    fprintf(f, " %12g", (float) prof_data[pr].flops / (rtime/cnt));
    fprintf(f, " %12g", (float) prof_data[pr].bytes / (rtime/cnt));
    fprintf(f, " / usec\n");
  }
}

#else // !LIBPAPI

void
prof_init(void)
{
  prof_inited = 1;
}

void
prof_print_file(FILE *f)
{
  fprintf(f, "%19s %7s %4s %7s", "", "tottime", "cnt", "time");
  fprintf(f, " %12s", "FLOPS");
  fprintf(f, " %12s", "MFLOPS/sec");
  fprintf(f, " %12s", "MBytes/sec");
  fprintf(f, "\n");

  for (int pr = 0; pr < nr_prof_data; pr++) {
    struct prof_info *pinfo = &prof_globals.info[pr];
    int cnt = pinfo->cnt;
    float rtime = pinfo->time;
    if (!cnt || rtime == 0.)
      continue;

    fprintf(f, "%-19s %7g %4d %7g", prof_data[pr].name, rtime/1e3, cnt, 
	   rtime / 1e3 / cnt);
    fprintf(f, " %12d", prof_data[pr].flops);
    fprintf(f, " %12g", (float) prof_data[pr].flops / (rtime/cnt));
    fprintf(f, " %12g", prof_data[pr].bytes / (rtime/cnt));
    fprintf(f, "\n");
  }
}

#endif

int
prof_register(const char *name, float simd, int flops, int bytes)
{
  if (!prof_inited) {
    prof_init();
  }

  assert(nr_prof_data < MAX_PROF);
  struct prof_data *p = &prof_data[nr_prof_data++];

  p->name = name;
  p->simd = simd;
  p->flops = flops;
  p->bytes = bytes;

  return nr_prof_data;
}

void
prof_print()
{
  prof_print_file(stdout);
}
