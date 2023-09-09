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

void print_diagnostics(uint64_t types);

//

void test_procthreads(void);

boolean debug_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                       livespointer statep);

void explain_missing_activate(LiVESMenuItem *, livespointer user_data);

void show_all_leaves(weed_plant_t *);
void show_audit(weed_plant_t *);

void run_diagnostic(LiVESWidget *, const char *testname);

void list_leaves(weed_plant_t *);

char *get_stats_msg(boolean calc_only);
double get_inst_fps(boolean get_msg);

void reset_timer_info(void);
double show_timer_info(void);

int run_weed_startup_tests(void);

#ifdef WEED_STARTUP_TESTS
int test_palette_conversions(void);
#endif

typedef uint64_t (*lives_randfunc_t)(void);

void test_random(void);

int benchmark_rng(int ntests, lives_randfunc_t rfunc, char **tmp, double *q);

void lives_struct_test(void);

void benchmark(void);

void hash_test(void);

#ifdef WEED_WIDGETS
void show_widgets_info(void);
#endif

void show_struct_sizes(void);

void do_lsd_tests(void) LIVES_NEVER_INLINE;

void bundle_test(void);

char *weed_plant_to_header(weed_plant_t *, const char *tname);

#endif
