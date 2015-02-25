// lbindings.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"


typedef boolean Boolean;

#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>


boolean lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);


void ext_caller_check(int ret) {
  if (mainw->ext_caller) {
    // this is for the C++ binding
    char *tmp = lives_strdup_printf("%lu %d",mainw->ext_caller,ret);
    lives_notify(LIVES_NOTIFY_PRIVATE,(const char *)tmp);
    lives_free(tmp);
    mainw->ext_caller=0l;
  }
}



ulong *get_unique_ids(void) {
  // return array of unique_id (ulong) for all "real" clips
  int i=0;
  ulong *uids;
  pthread_mutex_lock(&mainw->clip_list_mutex);
  LiVESList *list=mainw->cliplist;
  uids=(ulong *)lives_malloc((lives_list_length(mainw->cliplist) + 1)*sizeof(ulong));
  while (list!=NULL) {
    int uid=0;
    int cnum=LIVES_POINTER_TO_INT(list->data);
    if (mainw->files[cnum]!=NULL) uid=mainw->files[cnum]->unique_id;
    list=list->next;
    if (uid==0) continue;
    uids[i++]=uid;
  }
  uids[i]=0;
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  return uids;
}


int cnum_for_uid(ulong uid) {
  pthread_mutex_lock(&mainw->clip_list_mutex);
  LiVESList *list=mainw->cliplist;
  while (list!=NULL) {
    int cnum=LIVES_POINTER_TO_INT(list->data);
    if (mainw->files[cnum]!=NULL && uid==mainw->files[cnum]->unique_id) return cnum;
    list=list->next;
  }
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  return -1;
}



static boolean osc_show_info(livespointer text) {
  // function that is picked up on idle
  do_info_dialog(text);
  return FALSE;
}



static boolean osc_show_blocking_info(livespointer data) {
  // function that is picked up on idle
  msginfo *minfo = (msginfo *)data;
  mainw->ext_caller=minfo->id;
  do_blocking_info_dialog(minfo->msg);
  lives_free(minfo->msg);
  lives_free(minfo);
  return FALSE;
}


static boolean call_osc_save_set(livespointer data) {
  // function that is picked up on idle
  oscdata *oscd = (oscdata *)data;
  mainw->ext_caller=oscd->id;
  lives_osc_cb_saveset(NULL, oscd->arglen, oscd->vargs, OSCTT_CurrentTime(), NULL);
  return FALSE;
}


void idle_show_info(const char *text, boolean blocking, ulong id) {
  if (!blocking) lives_idle_add(osc_show_info,(livespointer)text);
  else {
    msginfo *minfo = (msginfo *)lives_malloc(sizeof(msginfo));
    minfo->msg = strdup(text);
    minfo->id = id;
    lives_idle_add(osc_show_blocking_info,(livespointer)minfo);
  }
}


void idle_save_set(const char *name, int arglen, const void *vargs, ulong id) {
  oscdata *data = (oscdata *)lives_malloc(sizeof(oscdata));
  data->id=id;
  data->arglen=arglen;
  data->vargs=vargs;
  lives_idle_add(call_osc_save_set,(livespointer)data);
}


