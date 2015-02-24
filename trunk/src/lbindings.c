// lbindings.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

static boolean osc_show_info(livespointer text) {
  do_info_dialog(text);
  return FALSE;
}

static boolean osc_show_blocking_info(livespointer text) {
  do_blocking_info_dialog(text);
  return FALSE;
}


// TODO - move into bindings.c
void idle_show_info(const char *text, boolean blocking) {
  if (!blocking) lives_idle_add(osc_show_info,(livespointer)text);
  else lives_idle_add(osc_show_blocking_info,(livespointer)text);
}



void lives_notify(int msgnumber,const char *msgstring) {
#ifdef IS_LIBLIVES
  binding_cb(msgnumber, msgstring, mainw->id);
#endif
#ifdef ENABLE_OSC
  lives_osc_notify(msgnumber,msgstring);
#endif
}
