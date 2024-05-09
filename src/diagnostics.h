// diagnostics.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _HAVE_DIAG_H_
#define _HAVE_DIAG_H_

#include "main.h"
#include "mainwindow.h"

//#define TEST_RTM_CODE

#define TEST_WEED		(1ull << 0)
#define TEST_RNG		(1ull << 1)
#define TEST_LSD		(1ull << 2)
#define TEST_PAL_CONV		(1ull << 3)
#define TEST_BUNDLES		(1ull << 4)
#define TEST_WEED_UTILS		(1ull << 6)

#define TEST_POINT_2		(1ull << 16)
#define TEST_PROCTHRDS		(1ull << 17)

#ifdef TEST_RTM_CODE
#define TEST_RTM		(1ull << 32)
#endif

#define ABORT_AFTER		(1ull << 60)

lives_result_t do_startup_diagnostics(uint64_t tests_to_run);

#define DIAG_ALL		(uint64_t)-1
#define DIAG_MEMORY		(1ull << 0)
#define DIAG_THREADS		(1ull << 1)
#define DIAG_APP_STATUS		(1ull << 2)

void print_diagnostics(uint64_t types);

char *md5_print(void *md5sum);

//////////////////////////////////
double check_thrd_latency(double *act_time);

void test_procthreads(void);

boolean debug_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                       livespointer statep);

void explain_missing_activate(LiVESMenuItem *, livespointer user_data);

// auditing
#define STATS_LIST		0
#define STATS_FREQ		1
#define STATS_ATAG		2

void show_weed_stats(int oper);
void upd_statsplant(const char *key);

void add_to_audit(audit_tag *, void *data); 
void remove_from_audit(void *);

void show_audit(weed_plant_t *);

//////////////////////

void show_all_leaves(weed_plant_t *);

void run_diagnostic(LiVESWidget *, const char *testname);

void list_leaves(weed_plant_t *);

char *get_stats_msg(boolean calc_only);
double get_inst_fps(boolean get_msg);

int run_weed_startup_tests(void);

#ifdef WEED_STARTUP_TESTS
int test_palette_conversions(void);
#endif

typedef uint64_t (*lives_randfunc_t)(void);

void test_random(void);

int benchmark_rng(int ntests, lives_randfunc_t rfunc, double *q);

void lives_struct_test(void);

void benchmark(void);

void hash_test(void);

#ifdef WEED_WIDGETS
void show_widgets_info(void);
#endif

void show_struct_sizes(void);

void do_lsd_tests(void) LIVES_NEVER_INLINE;

void bundle_test(void);

////////////////// INFO /////////////////
char *cl_flags_desc(uint64_t clflags);
char *hs_op_flags_desc(uint64_t opflags);

char *weed_leaf_stringify(weed_plant_t *pl, const char *key);

void lpt_desc_state(lives_proc_thread_t);

char *weed_plant_to_header(weed_plant_t *, const char *tname);

char *lives_funcdef_explain(const lives_funcdef_t *);

char *lives_funcinst_show_func_call(lives_funcinst_t *finst);

char *lives_proc_thread_show_func_call(lives_proc_thread_t lpt);

char *funcinst_paramstr(lives_funcinst_t *, funcsig_t sig);

lives_result_t lives_describe_hook_stack(lives_hook_stack_t **hstacks, int type);

const char *hs_pattern_name(hook_stack_pattern_t pattern);

void dump_hook_stack(lives_hook_stack_t **, int type);
void dump_hook_stack_for(lives_proc_thread_t, int type);

void analyse_weed_plant(weed_plant_t *, int *xtype, int64_t *xsubtype);

void list_prefs(void);

char *weed_plant_to_header(weed_plant_t *, const char *tname);

boolean validate_args_fmt(const char *args_fmt, const char *funcname, const char **pnames);
char *args_fmt_filter(const char *args_fmt, boolean strict);

#endif
