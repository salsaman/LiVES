// plugins.h
// LiVES
// (c) G. Finch 2010 - 2016 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_STARTUP_H
#define HAS_LIVES_STARTUP_H

#define LIVES_TEST_VIDEO_NAME "vidtest.avi"

boolean do_workdir_query(void);
LiVESResponseType check_workdir_valid(char **pdirname, LiVESDialog *, boolean full);
boolean do_audio_choice_dialog(short startup_phase);
boolean do_startup_tests(boolean tshoot);
void do_startup_interface_query(void);

void on_troubleshoot_activate(LiVESMenuItem *, livespointer);


void do_bad_dir_perms_error(const char *dirname);
void dir_toolong_error(char *dirname, const char *dirtype, size_t max, boolean allow_retry);

#endif
