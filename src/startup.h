// plugins.h
// LiVES
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _HAS_STARTUP_H
#define _HAS_STARTUP_H



gboolean do_tempdir_query(void);
gboolean do_audio_choice_dialog(short startup_phase);
gboolean do_startup_tests(gboolean tshoot);
void do_startup_interface_query(void);

void on_troubleshoot_activate (GtkMenuItem *, gpointer);


#endif
