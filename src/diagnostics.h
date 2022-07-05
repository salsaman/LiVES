// diagnostics.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _HAVE_DIAG_H_
#define _HAVE_DIAG_H_

#include "main.h"
#include "mainwindow.h"

boolean debug_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                       livespointer statep);

void explain_missing_activate(LiVESMenuItem *, livespointer user_data);

void run_diagnostic(LiVESWidget *, const char *testname);

char *get_stats_msg(boolean calc_only);
double get_inst_fps(boolean get_msg);

void reset_timer_info(void);
double show_timer_info(void);

int run_weed_startup_tests(void);

#ifdef WEED_STARTUP_TESTS
int test_palette_conversions(void);
#endif

void check_random(void);

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
char *bundle_to_header(lives_bundle_t *, const char *tname);

void md5test(void);

#endif
