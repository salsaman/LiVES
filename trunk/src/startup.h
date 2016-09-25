// plugins.h
// LiVES
// (c) G. Finch 2010 - 2016 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_STARTUP_H
#define HAS_LIVES_STARTUP_H

boolean do_workdir_query(void);
boolean do_audio_choice_dialog(short startup_phase);
boolean do_startup_tests(boolean tshoot);
void do_startup_interface_query(void);

void on_troubleshoot_activate(LiVESMenuItem *, livespointer);


#endif
