// startup.h
// LiVES
// (c) G. Finch 2010 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_STARTUP_H
#define HAS_LIVES_STARTUP_H

#define LIVES_TEST_VIDEO_NAME "vidtest.avi"

boolean migrate_config(const char *old_vhash, const char *newconfigfile);
void cleanup_old_config(uint64_t oldver);
boolean build_init_config(const char *config_datadir, boolean prompt);

boolean do_workdir_query(void);
LiVESResponseType check_workdir_valid(char **pdirname, LiVESDialog *, boolean full);

#ifdef ENABLE_JACK
boolean do_jack_config(boolean is_setup, boolean is_trans);
#endif

boolean do_audio_choice_dialog(short startup_phase);
boolean do_startup_tests(boolean tshoot);
boolean do_startup_interface_query(void);

void run_lives_setup_wizard(int page);

void on_troubleshoot_activate(LiVESMenuItem *, livespointer);
void explain_missing_activate(LiVESMenuItem *menuitem, livespointer user_data);

void do_bad_dir_perms_error(const char *dirname);
void dir_toolong_error(const char *dirname, const char *dirtype, size_t max, boolean can_retry);
void filename_toolong_error(const char *fname, const char *ftype, size_t max, boolean can_retry);

#endif
