// plugins.h
// LiVES
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _HAS_STARTUP_H
#define _HAS_STARTUP_H



gboolean do_tempdir_query(void);
gboolean do_audio_choice_dialog(short startup_phase);
gboolean do_startup_tests(void);
void do_startup_interface_query(void);



#endif
