// lbindings.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "interface.h"
#include "callbacks.h"
#include "rte_window.h"
#include "effects-weed.h"
#include "effects.h"

#include "liblives.hpp"


typedef boolean Boolean;

#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>



typedef struct {
  // c
  ulong id;
  char *msg;
} msginfo;


typedef struct {
  // i,v
  ulong id;
  int arglen;
  const void *vargs;
} oscdata;


typedef struct {
  // c, d, i
  ulong id;
  char *fname;
  double stime;
  int frames;
} opfidata;


typedef struct {
  // c, c, i
  ulong id;
  char *dir;
  char *title;
  int preview_type;
} fprev;


typedef struct {
  // b
  ulong id;
  boolean setting;
} sintdata;


typedef struct {
  ulong id;
} udata;


typedef struct {
  // i, i, i
  ulong id;
  int key;
  int mode;
  int idx;
} fxmapdata;


typedef struct {
  // i, b
  // boolean pref
  ulong id;
  int prefidx;
  boolean val;
} bpref;


typedef struct {
  // i, i
  // int pref
  ulong id;
  int prefidx;
  int val;
} ipref;


typedef struct {
  // i, b, b
  ulong id;
  int clip;
  boolean ign_sel;
  boolean with_audio;
} iblock;


typedef struct {
  ulong id;
  track_rect *block;
} rblockdata;



/////////////////////////////////////////
/// extern functions with no headers

boolean lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);


/////////// value return functions ///////////////////////


static void ext_caller_return_int(ulong caller_id, int ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %d",caller_id,ret);
  if (mainw!=NULL) binding_cb (LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
  lives_free(msgstring);
}


static void ext_caller_return_ulong(ulong caller_id, ulong ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %lu",caller_id,ret);
  if (mainw!=NULL) binding_cb (LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
  lives_free(msgstring);
}


static void ext_caller_return_string(ulong caller_id, const char *ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %s",caller_id,ret);
  if (mainw!=NULL) binding_cb (LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
  lives_free(msgstring);
}


///////////////////////////////////////////////////////////////
/// utility functions for liblives /////

ulong *get_unique_ids(void) {
  // return array of unique_id (ulong) for all "real" clips
  int i=0;
  ulong *uids=NULL;
  LiVESList *list;
  if (mainw!=NULL&&!mainw->go_away) {
    pthread_mutex_lock(&mainw->clip_list_mutex);
    list=mainw->cliplist;
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
  }
  return uids;
}


int cnum_for_uid(ulong uid) {
  LiVESList *list;
  if (mainw!=NULL&&!mainw->go_away) {
    pthread_mutex_lock(&mainw->clip_list_mutex);
    list=mainw->cliplist;
    while (list!=NULL) {
      int cnum=LIVES_POINTER_TO_INT(list->data);
      if (mainw->files[cnum]!=NULL && uid==mainw->files[cnum]->unique_id) {
	pthread_mutex_unlock(&mainw->clip_list_mutex);
	return cnum;
      }
      list=list->next;
    }
    pthread_mutex_unlock(&mainw->clip_list_mutex);
  }
  return -1;
}


#define const_domain_notify 1
#define const_domain_response 1

inline int trans_rev(int consta, int a, int b) {
  if (consta==a) return b;
  else return consta;
}

int trans_constant(int consta, int domain) {
  int nconsta;
  if (domain==const_domain_notify) {
    nconsta=trans_rev(consta,LIVES_CALLBACK_FRAME_SYNCH,LIVES_OSC_NOTIFY_FRAME_SYNCH);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_PLAYBACK_STARTED,LIVES_OSC_NOTIFY_PLAYBACK_STARTED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_PLAYBACK_STOPPED,LIVES_OSC_NOTIFY_PLAYBACK_STOPPED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_PLAYBACK_STOPPED_RD,LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_RECORD_STARTED,LIVES_OSC_NOTIFY_RECORD_STARTED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_RECORD_STOPPED,LIVES_OSC_NOTIFY_RECORD_STOPPED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_APP_QUIT,LIVES_OSC_NOTIFY_QUIT);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_CLIP_OPENED,LIVES_OSC_NOTIFY_CLIP_OPENED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_CLIP_CLOSED,LIVES_OSC_NOTIFY_CLIP_CLOSED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_CLIPSET_OPENED,LIVES_OSC_NOTIFY_CLIPSET_OPENED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_CLIPSET_SAVED,LIVES_OSC_NOTIFY_CLIPSET_SAVED);
    if (nconsta!=consta) return nconsta;
    nconsta=trans_rev(consta,LIVES_CALLBACK_MODE_CHANGED,LIVES_OSC_NOTIFY_MODE_CHANGED);
    if (nconsta!=consta) return nconsta;
  }
  if (domain==const_domain_response) {
    if (consta==LIVES_RESPONSE_NONE) return LIVES_DIALOG_RESPONSE_NONE;
    if (consta==LIVES_RESPONSE_OK) return LIVES_DIALOG_RESPONSE_OK;
    if (consta==LIVES_RESPONSE_CANCEL) return LIVES_DIALOG_RESPONSE_CANCEL;
    if (consta==LIVES_RESPONSE_ACCEPT) return LIVES_DIALOG_RESPONSE_ACCEPT;
    if (consta==LIVES_RESPONSE_YES) return LIVES_DIALOG_RESPONSE_YES;
    if (consta==LIVES_RESPONSE_NO) return LIVES_DIALOG_RESPONSE_NO;

    // positive values for custom responses
    if (consta==LIVES_RESPONSE_INVALID) return LIVES_DIALOG_RESPONSE_INVALID;
    if (consta==LIVES_RESPONSE_RETRY) return LIVES_DIALOG_RESPONSE_RETRY;
    if (consta==LIVES_RESPONSE_ABORT) return LIVES_DIALOG_RESPONSE_ABORT;
    if (consta==LIVES_RESPONSE_RESET) return LIVES_DIALOG_RESPONSE_RESET;
    if (consta==LIVES_RESPONSE_SHOW_DETAILS) return LIVES_DIALOG_RESPONSE_SHOW_DETAILS;
  }


  return consta;
}


int get_first_fx_matched(const char *package, const char *fxname, const char *author, int version) {
  int *allvals=weed_get_indices_from_template(package,fxname,author,version);
  int fval=allvals[0];
  lives_free(allvals);
  return fval;
}


// 1 based key here
int get_num_mapped_modes_for_key(int key) {
  return rte_key_getmaxmode(key)+1;
}


int get_current_mode_for_key(int key) {
  return rte_key_getmode(key);
}


boolean get_rte_key_is_enabled(int key) {
  return rte_key_is_enabled(key);
}


//// interface callers


static boolean call_osc_show_info(livespointer text) {
  // function that is picked up on idle
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    do_info_dialog(text);
  }
  lives_free(text);
  return FALSE;
}



static boolean call_osc_show_blocking_info(livespointer data) {
  // function that is picked up on idle
  int ret=LIVES_RESPONSE_INVALID;
  msginfo *minfo = (msginfo *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    ret=do_blocking_info_dialog(minfo->msg);
    lives_free(minfo->msg);
    ret=trans_constant(ret,const_domain_notify);
  }
  ext_caller_return_int(minfo->id,ret);
  lives_free(minfo);
  return FALSE;
}


static boolean call_osc_save_set(livespointer data) {
  // function that is picked up on idle
  boolean ret=FALSE;
  oscdata *oscd = (oscdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    ret=lives_osc_cb_saveset(NULL, oscd->arglen, oscd->vargs, OSCTT_CurrentTime(), NULL);
  }
  ext_caller_return_int(oscd->id,(int)ret);
  lives_free(oscd);
  return FALSE;
}


static boolean call_file_choose_with_preview(livespointer data) {
  LiVESWidget *chooser;
  fprev *fdata = (fprev *)data;

  char *fname=NULL,*rstr=NULL;

  int preview_type;
  int response;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    if (fdata->preview_type==LIVES_FILE_CHOOSER_VIDEO_AUDIO) preview_type=LIVES_FILE_SELECTION_VIDEO_AUDIO;
    else preview_type=LIVES_FILE_SELECTION_AUDIO_ONLY;
    chooser=choose_file_with_preview(fdata->dir, fdata->title, preview_type);
    response=lives_dialog_run(LIVES_DIALOG(chooser));
    end_fs_preview();
    mainw->fs_playarea=NULL;

    if (response == LIVES_RESPONSE_ACCEPT) {
      fname=lives_file_chooser_get_filename (LIVES_FILE_CHOOSER(chooser));
    }
    if (fname==NULL) fname=lives_strdup("");
  
    on_filechooser_cancel_clicked(chooser);

    if (fdata->dir!=NULL) lives_free(fdata->dir);
    if (fdata->title!=NULL) lives_free(fdata->title);

    rstr = lives_strdup_printf("%s %d", fname, mainw->open_deint);
  }
  ext_caller_return_string(fdata->id,rstr);
  if (rstr!=NULL) lives_free(rstr);
  if (fdata!=NULL) lives_free(fdata);
  if (fname!=NULL) free(fname);
  if (data!=NULL) lives_free(data);
  return FALSE;
}


static boolean call_choose_set(livespointer data) {
  udata *ud=(udata *)data;
  if (mainw!=NULL&&!mainw->was_set&&!mainw->go_away&&!mainw->is_processing) {
    char *setname=on_load_set_activate(NULL,(livespointer)1);
    if (setname==NULL) setname=lives_strdup("");
    ext_caller_return_string(ud->id,setname);
    lives_free(setname);
  }
  else ext_caller_return_string(ud->id,"");
  lives_free(ud);
  return FALSE;
}


static boolean call_open_file(livespointer data) {
  opfidata *opfi = (opfidata *)data;
  ulong uid=0l;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    uid=open_file_sel(opfi->fname,opfi->stime,opfi->frames);
    if (opfi->fname!=NULL) lives_free(opfi->fname);
  }
  ext_caller_return_ulong(opfi->id,uid);
  lives_free(opfi);
  return FALSE;
}


static boolean call_reload_set(livespointer data) {
  msginfo *mdata = (msginfo *)data;
  boolean resp=LIVES_RESPONSE_INVALID;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    mainw->osc_auto=1;
    if (!is_legal_set_name(mdata->msg,TRUE)) {
      mainw->osc_auto=0;
      resp=FALSE;
    }
    else {
      mainw->osc_auto=0;
      resp=reload_set(mdata->msg);
    }
  }
  lives_free(mdata->msg);
  ext_caller_return_int(mdata->id,resp);
  lives_free(mdata);
  return FALSE;
}


static boolean call_set_interactive(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    mainw->interactive=sint->setting;
    set_interactive(mainw->interactive);
    ext_caller_return_int(sint->id,TRUE);
  }
  else ext_caller_return_int(sint->id,FALSE);
  lives_free(sint);
  return FALSE;
}


static boolean call_set_sepwin(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    if (mainw->sep_win!=sint->setting)
      on_sepwin_pressed(NULL,NULL);
    ext_caller_return_int(sint->id,TRUE);
  }
  else ext_caller_return_int(sint->id,FALSE);
  lives_free(sint);
  return FALSE;
}



static boolean call_set_fullscreen(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    mainw->interactive=sint->setting;
    if (mainw->fs!=sint->setting)
      on_full_screen_pressed(NULL,NULL);
    ext_caller_return_int(sint->id,TRUE);
  }
  else ext_caller_return_int(sint->id,FALSE);
  lives_free(sint);
  return FALSE;
}


static boolean call_set_fullscreen_sepwin(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    mainw->interactive=sint->setting;
    if (mainw->sep_win!=sint->setting)
      on_sepwin_pressed(NULL,NULL);
    if (mainw->fs!=sint->setting)
      on_full_screen_pressed(NULL,NULL);
    ext_caller_return_int(sint->id,TRUE);
  }
  else ext_caller_return_int(sint->id,FALSE);
  lives_free(sint);
  return FALSE;
}


static boolean call_set_ping_pong(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    if (mainw->ping_pong!=sint->setting)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_ping_pong),!mainw->ping_pong);
    ext_caller_return_int(sint->id,TRUE);
  }
  else ext_caller_return_int(sint->id,FALSE);
  lives_free(sint);
  return FALSE;
}


static boolean call_set_pref_bool(livespointer data) {
  bpref *bdata=(bpref *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    pref_factory_bool(bdata->prefidx, bdata->val);
    ext_caller_return_int(bdata->id,TRUE);
  }
  else ext_caller_return_int(bdata->id,FALSE);
  lives_free(bdata);
  return FALSE;
}


static boolean call_set_pref_int(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    pref_factory_int(idata->prefidx, idata->val);
    ext_caller_return_int(idata->id,TRUE);
  }
  else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_mt_set_track(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&mainw->multitrack!=NULL && (mt_track_is_video(mainw->multitrack, idata->val) 
								|| mt_track_is_audio(mainw->multitrack, idata->val))) {
    mainw->multitrack->current_track=idata->val;
    track_select(mainw->multitrack);
    ext_caller_return_int(idata->id,TRUE);
  }
  else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_set_if_mode(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    if (idata->val==LIVES_INTERFACE_MODE_CLIPEDIT&&mainw->multitrack!=NULL) {
      multitrack_delete(mainw->multitrack,FALSE);
    }
    if (idata->val==LIVES_INTERFACE_MODE_MULTITRACK&&mainw->multitrack==NULL) {
      on_multitrack_activate(NULL,NULL);
      while (mainw->multitrack==NULL || !mainw->multitrack->is_ready) lives_usleep(prefs->sleep_time);
    }
    ext_caller_return_int(idata->id,TRUE);
  }
  else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_switch_clip(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    switch_clip(idata->prefidx,idata->val);
    ext_caller_return_int(idata->id,TRUE);
  }
  else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_set_current_time(livespointer data) {
  opfidata *idata=(opfidata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    if (mainw->multitrack!=NULL) {
      if (idata->stime >=0.) {
	if (idata->stime>mainw->multitrack->end_secs) set_timeline_end_secs(mainw->multitrack,idata->stime);
	mt_tl_move(mainw->multitrack,idata->stime);
      }
    }
    else {
      if (mainw->current_file>0 && idata->stime>=0. && idata->stime<=cfile->total_time) {
	cfile->pointer_time=idata->stime;
	lives_ruler_set_value(LIVES_RULER (mainw->hruler),cfile->pointer_time);
	lives_widget_queue_draw (mainw->hruler);
	get_play_times();
      }
    }
    ext_caller_return_int(idata->id,TRUE);
  }
  else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_unmap_effects(livespointer data) {
  ulong id=(ulong)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    on_clear_all_clicked(NULL,NULL);
    ext_caller_return_int(id,TRUE);
  }
  else ext_caller_return_int(id,FALSE);
  return FALSE;
}


static boolean call_map_effect(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    char *hashname=make_weed_hashname(fxdata->idx,TRUE,FALSE);
    int error=rte_switch_keymode(fxdata->key,fxdata->mode,hashname);
    ext_caller_return_int(fxdata->id,(error==0));
    lives_free(hashname);
  }
  else ext_caller_return_int(fxdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_unmap_effect(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&rte_keymode_valid(fxdata->key,fxdata->mode,TRUE)) {
    int idx=fxdata->key*rte_getmodespk()+fxdata->mode;
    on_clear_clicked(NULL,LIVES_INT_TO_POINTER(idx));
    ext_caller_return_int(fxdata->id,TRUE);
  }
  else ext_caller_return_int(fxdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_fx_setmode(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    boolean ret=rte_key_setmode(fxdata->key,fxdata->mode);
    ext_caller_return_int(fxdata->id,(int)ret);
  }
  else ext_caller_return_int(fxdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_wipe_layout(livespointer data) {
  iblock *fxdata=(iblock *)data;
  boolean force=fxdata->ign_sel;
  char *lname=lives_strdup("");
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->multitrack!=NULL) {
    if (force) {
      wipe_layout(mainw->multitrack);
    }
    else {
      memset(mainw->recent_file,0,1);
      check_for_layout_del(mainw->multitrack, FALSE);
      if (strlen(mainw->recent_file)) {
	lives_free(lname);
	lname=strdup(mainw->recent_file);
      }
    }
  }
  ext_caller_return_string(fxdata->id,lname);
  lives_free(lname);
  lives_free(data);
  return FALSE;
}



static boolean call_set_current_fps(livespointer data) {
  opfidata *idata=(opfidata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file>-1) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),idata->stime);
    ext_caller_return_int(idata->id,(int)TRUE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_select_all(livespointer data) {
  ipref *idata=(ipref *)data;
  int cnum = idata->val;

  if (mainw!=NULL) {
    lives_clip_t *sfile=mainw->files[cnum];
    boolean selwidth_locked=mainw->selwidth_locked;

    if (!mainw->go_away&&!mainw->is_processing&&sfile!=NULL) {
      mainw->selwidth_locked=FALSE;
      if (cnum==mainw->current_file) on_select_all_activate(NULL,NULL);
      else {
	sfile->start=sfile->frames>0?1:0;
	sfile->end=sfile->frames;
      }
      mainw->selwidth_locked=selwidth_locked;
      ext_caller_return_int(idata->id,(int)TRUE);
    }
    else ext_caller_return_int(idata->id,(int)FALSE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_select_start(livespointer data) {
  ipref *idata=(ipref *)data;

  int cnum = idata->val;
  int frame=idata->prefidx;

  if (mainw!=NULL) {
    lives_clip_t *sfile=mainw->files[cnum];
    boolean selwidth_locked=mainw->selwidth_locked;
    if (!mainw->go_away&&!mainw->is_processing&&sfile!=NULL) {
      mainw->selwidth_locked=FALSE;
      if (cnum==mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),frame);
      else {
	if (frame>sfile->frames) frame=sfile->frames;
	if (sfile->end<frame) sfile->end=frame;
	sfile->start=frame;
      }
      mainw->selwidth_locked=selwidth_locked;
      ext_caller_return_int(idata->id,(int)TRUE);
    }
    else ext_caller_return_int(idata->id,(int)FALSE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_select_end(livespointer data) {
  ipref *idata=(ipref *)data;

  int cnum = idata->val;
  int frame=idata->prefidx;
  if (mainw!=NULL) {
    lives_clip_t *sfile=mainw->files[cnum];
    boolean selwidth_locked=mainw->selwidth_locked;

    if (!mainw->go_away&&!mainw->is_processing&&sfile!=NULL) {
      mainw->selwidth_locked=FALSE;
      if (cnum==mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),frame);
      else {
	if (frame>sfile->frames) frame=sfile->frames;
	if (sfile->start>frame) sfile->start=frame;
	sfile->end=frame;
      }
      mainw->selwidth_locked=selwidth_locked;
      ext_caller_return_int(idata->id,(int)TRUE);
    }
    else ext_caller_return_int(idata->id,(int)FALSE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_insert_block(livespointer data) {
  iblock *idata=(iblock *)data;
  boolean ins_audio;
  boolean ign_ins_sel;

  ulong block_uid;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1&&mainw->multitrack!=NULL) {
    mainw->multitrack->clip_selected=idata->clip-1;
    mt_clip_select(mainw->multitrack,TRUE);

    ins_audio=mainw->multitrack->opts.insert_audio;
    ign_ins_sel=mainw->multitrack->opts.ign_ins_sel;

    mainw->multitrack->opts.insert_audio=idata->with_audio;
    mainw->multitrack->opts.ign_ins_sel=idata->ign_sel;

    multitrack_insert(NULL,mainw->multitrack);

    mainw->multitrack->opts.ign_ins_sel=ign_ins_sel;
    mainw->multitrack->opts.insert_audio=ins_audio;

    block_uid=mt_get_last_block_uid(mainw->multitrack);
    ext_caller_return_ulong(idata->id,block_uid);
  }
  else ext_caller_return_ulong(idata->id,0l);
  lives_free(data);
  return FALSE;
}




static boolean call_remove_block(livespointer data) {
  rblockdata *rdata=(rblockdata *)data;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1&&mainw->multitrack!=NULL) {
    track_rect *oblock=mainw->multitrack->block_selected;
    mainw->multitrack->block_selected=rdata->block;
    delete_block_cb(NULL,(livespointer)mainw->multitrack);
    if (oblock!=rdata->block) mainw->multitrack->block_selected=oblock;
    ext_caller_return_int(rdata->id,(int)TRUE);
  }
  else ext_caller_return_int(rdata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_fx_enable(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  boolean nstate=(boolean)fxdata->mode;
  boolean ret=FALSE;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    int grab=mainw->last_grabbable_effect;
    int effect_key=fxdata->key;

    if (nstate) {
      // fx is on
      if (!(mainw->rte&(GU641<<(effect_key-1)))) {
	weed_plant_t *filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
	if (filter==NULL) ret=FALSE;
	else {
	  int count=enabled_in_channels(filter, FALSE);
	  if (mainw->playing_file==-1&&count==0) {
	    // return value first because...
	    ext_caller_return_int(fxdata->id,(int)TRUE);
	    // ...we are going to hang here until playback ends
	    ret=rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
	    return FALSE;
	  }
	  else {
	    ret=rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
	    mainw->last_grabbable_effect=grab;
	  }
	}
      }
    }
    else {
      if (mainw->rte&(GU641<<(effect_key-1))) {
	ret=rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
      }
    }
  }
  ext_caller_return_int(fxdata->id,(int)ret);
  lives_free(data);
  return FALSE;
}


static boolean call_set_loop_mode(livespointer data) {
  ipref *idata=(ipref *)data;
  int lmode=idata->val;

  if (mainw!=NULL&&!mainw->go_away) {
    if (lmode==LIVES_LOOP_MODE_NONE) {
      if (mainw->loop_cont) on_loop_button_activate(NULL,NULL);
      if (mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
    }
    else {
      if (lmode & LIVES_LOOP_MODE_CONTINUOUS) {
	if (!mainw->loop_cont) on_loop_button_activate(NULL,NULL);
      }
      else if (mainw->loop_cont) on_loop_button_activate(NULL,NULL);

      if (lmode & LIVES_LOOP_MODE_FIT_AUDIO) {
	if (mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
      }
      else if (!mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
    }
    ext_caller_return_int(idata->id,(int)TRUE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_resync_fps(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&& mainw->playing_file>-1) {
    fps_reset_callback (NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    ext_caller_return_int(idata->id,(int)TRUE);
  }
  else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_cancel_proc(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw==NULL||mainw->current_file==-1||cfile==NULL||cfile->proc_ptr==NULL||
      !lives_widget_is_visible(cfile->proc_ptr->cancel_button)) {
    ext_caller_return_int(idata->id,(int)FALSE);
  }
  else {
    on_cancel_keep_button_clicked(NULL,NULL);
    ext_caller_return_int(idata->id,(int)TRUE);
  }
  lives_free(data);
  return FALSE;
}




//////////////////////////////////////////////////////////////////

/// idlefunc hooks


boolean idle_show_info(const char *text, boolean blocking, ulong id) {
  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (text==NULL) return FALSE;
  if (!blocking) lives_idle_add(call_osc_show_info,(livespointer)text);
  else {
    msginfo *minfo = (msginfo *)lives_malloc(sizeof(msginfo));
    minfo->msg = strdup(text);
    minfo->id = id;
    lives_idle_add(call_osc_show_blocking_info,(livespointer)minfo);
  }
  return TRUE;
}


boolean idle_switch_clip(int type, int cnum, ulong id) {
  ipref *info;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;
  if (mainw->multitrack!=NULL) return FALSE;

  info = (ipref *)lives_malloc(sizeof(ipref));
  info->id = id;
  info->val = cnum;
  info->prefidx = type;
  lives_idle_add(call_switch_clip,(livespointer)info);
  return TRUE;
}


boolean idle_mt_set_track(int tnum, ulong id) {
  ipref *info;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  info = (ipref *)lives_malloc(sizeof(ipref));
  info->id = id;
  info->val = tnum;
  lives_idle_add(call_mt_set_track,(livespointer)info);
  return TRUE;
}


boolean idle_set_current_time(double time, ulong id) {
  opfidata *info;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  info = (opfidata *)lives_malloc(sizeof(opfidata));
  info->id = id;
  info->stime=time;
  lives_idle_add(call_set_current_time,(livespointer)info);
  return TRUE;
}


boolean idle_unmap_effects(ulong id) {
  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;

  lives_idle_add(call_unmap_effects,(livespointer)id);

  return TRUE;
}


boolean idle_save_set(const char *name, int arglen, const void *vargs, ulong id) {
  oscdata *data;
  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  data= (oscdata *)lives_malloc(sizeof(oscdata));
  data->id=id;
  data->arglen=arglen;
  data->vargs=vargs;
  lives_idle_add(call_osc_save_set,(livespointer)data);

  return TRUE;
}


boolean idle_choose_file_with_preview(const char *dirname, const char *title, int preview_type, ulong id) {
  fprev *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  data= (fprev *)lives_malloc(sizeof(fprev));
  data->id=id;

  if (dirname!=NULL && strlen(dirname) > 0) data->dir=strdup(dirname);
  else data->dir=NULL;
  
  if (title!=NULL && strlen(title) > 0) data->title=strdup(title);
  else data->title=NULL;

  data->preview_type=preview_type;
  lives_idle_add(call_file_choose_with_preview,(livespointer)data);
  return TRUE;
}


boolean idle_choose_set(ulong id) {
  udata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (mainw->was_set) return FALSE;

  data=(udata *)lives_malloc(sizeof(udata));
  data->id=id;
  lives_idle_add(call_choose_set,(livespointer)data);
  return TRUE;
}


boolean idle_open_file(const char *fname, double stime, int frames, ulong id) {
  opfidata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (fname==NULL || strlen(fname)==0) return FALSE;

  data= (opfidata *)lives_malloc(sizeof(opfidata));
  data->id=id;
  data->fname=strdup(fname);
  data->stime=stime;
  data->frames=frames;

  lives_idle_add(call_open_file,(livespointer)data);
  return TRUE;
}


boolean idle_reload_set(const char *setname, ulong id) {
  msginfo *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (mainw->was_set) return FALSE;

  if (setname==NULL || strlen(setname)==0) return FALSE;

  if (!is_legal_set_name(setname,TRUE)) return FALSE;

  data=(msginfo *)lives_malloc(sizeof(msginfo));
  data->id=id;
  data->msg=strdup(setname);

  lives_idle_add(call_reload_set,(livespointer)data);
  return TRUE;
}



boolean idle_set_interactive(boolean setting, ulong id) {
  sintdata *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(sintdata *)lives_malloc(sizeof(sintdata));
  data->id=id;
  data->setting=setting;
  lives_idle_add(call_set_interactive,(livespointer)data);
  return TRUE;
}


boolean idle_set_sepwin(boolean setting, ulong id) {
  sintdata *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(sintdata *)lives_malloc(sizeof(sintdata));
  data->id=id;
  data->setting=setting;
  lives_idle_add(call_set_sepwin,(livespointer)data);
  return TRUE;
}


boolean idle_set_fullscreen(boolean setting, ulong id) {
  sintdata *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(sintdata *)lives_malloc(sizeof(sintdata));
  data->id=id;
  data->setting=setting;
  lives_idle_add(call_set_fullscreen,(livespointer)data);
  return TRUE;
}


boolean idle_set_fullscreen_sepwin(boolean setting, ulong id) {
  sintdata *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(sintdata *)lives_malloc(sizeof(sintdata));
  data->id=id;
  data->setting=setting;
  lives_idle_add(call_set_fullscreen_sepwin,(livespointer)data);
  return TRUE;
}



boolean idle_set_ping_pong(boolean setting, ulong id) {
  sintdata *data;
  
  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(sintdata *)lives_malloc(sizeof(sintdata));
  data->id=id;
  data->setting=setting;
  lives_idle_add(call_set_ping_pong,(livespointer)data);
  return TRUE;
}



boolean idle_map_fx(int key, int mode, int idx, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=mode;
  data->idx=idx;
  data->id=id;
  lives_idle_add(call_map_effect,(livespointer)data);
  return TRUE;
}



boolean idle_unmap_fx(int key, int mode, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;

  if (!rte_keymode_valid(key,mode,TRUE)) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=mode;
  data->id=id;
  lives_idle_add(call_unmap_effect,(livespointer)data);
  return TRUE;
}



boolean idle_fx_setmode(int key, int mode, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=mode;
  data->id=id;

  lives_idle_add(call_fx_setmode,(livespointer)data);
  return TRUE;
}



boolean idle_fx_enable(int key, boolean setting, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=setting;
  data->id=id;

  lives_idle_add(call_fx_enable,(livespointer)data);
  return TRUE;
}




boolean idle_set_pref_bool(int prefidx, boolean val, ulong id) {
  bpref *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  data=(bpref *)lives_malloc(sizeof(bpref));
  data->id=id;
  data->prefidx=prefidx;
  data->val=val;
  lives_idle_add(call_set_pref_bool,(livespointer)data);
  return TRUE;
}


boolean idle_set_pref_int(int prefidx, int val, ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->prefidx=prefidx;
  data->val=val;
  lives_idle_add(call_set_pref_int,(livespointer)data);
  return TRUE;
}


boolean idle_set_if_mode(lives_interface_mode_t mode, ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=(int)mode;
  lives_idle_add(call_set_if_mode,(livespointer)data);
  return TRUE;
}



boolean idle_insert_block(int clipno, boolean ign_sel, boolean with_audio, ulong id) {
  iblock *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(iblock *)lives_malloc(sizeof(iblock));
  data->id=id;
  data->clip=clipno;
  data->ign_sel=ign_sel;
  data->with_audio=with_audio;
  lives_idle_add(call_insert_block,(livespointer)data);
  return TRUE;
}


boolean idle_remove_block(ulong uid, ulong id) {
  rblockdata *data;
  track_rect *tr;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  tr=find_block_by_uid(mainw->multitrack, uid);
  if (tr==NULL) return FALSE;

  data=(rblockdata *)lives_malloc(sizeof(iblock));
  data->id=id;
  data->block=tr;
  lives_idle_add(call_remove_block,(livespointer)data);
  return TRUE;
}


boolean idle_wipe_layout(boolean force, ulong id) {
  iblock *data;

  if (mainw==NULL||mainw->preview||mainw->go_away||mainw->is_processing||mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(iblock *)lives_malloc(sizeof(iblock));
  data->ign_sel=force;
  data->id=id;
  lives_idle_add(call_wipe_layout,(livespointer)data);
  return TRUE;
}


boolean idle_select_all(int cnum, ulong id) {
  ipref *data;

  if (mainw==NULL||(mainw->preview&&mainw->multitrack==NULL)||mainw->go_away||mainw->is_processing) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=cnum;
  lives_idle_add(call_select_all,(livespointer)data);
  return TRUE;
}


boolean idle_select_start(int cnum, int frame, ulong id) {
  ipref *data;

  if (mainw==NULL||(mainw->preview&&mainw->multitrack==NULL)||mainw->go_away||mainw->is_processing) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=cnum;
  data->prefidx=frame;
  lives_idle_add(call_select_start,(livespointer)data);
  return TRUE;
}



boolean idle_select_end(int cnum, int frame, ulong id) {
  ipref *data;

  if (mainw==NULL||(mainw->preview&&mainw->multitrack==NULL)||mainw->go_away||mainw->is_processing) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=cnum;
  data->prefidx=frame;
  lives_idle_add(call_select_end,(livespointer)data);
  return TRUE;
}



boolean idle_set_current_fps(double fps, ulong id) {
  opfidata *data;

  if (mainw==NULL||mainw->playing_file == -1) return FALSE;
  if (mainw->multitrack != NULL) return FALSE;

  data=(opfidata *)lives_malloc(sizeof(opfidata));
  data->id=id;
  data->stime=fps;
  lives_idle_add(call_set_current_fps,(livespointer)data);
  return TRUE;
}



boolean idle_set_loop_mode(int mode, ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=mode;
  lives_idle_add(call_set_loop_mode,(livespointer)data);
  return TRUE;
}



boolean idle_resync_fps(ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->playing_file==-1) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  lives_idle_add(call_resync_fps,(livespointer)data);
  return TRUE;
}



boolean idle_cancel_proc(ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->current_file==-1||cfile==NULL||cfile->proc_ptr==NULL||
      !lives_widget_is_visible(cfile->proc_ptr->cancel_button)) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  lives_idle_add(call_cancel_proc,(livespointer)data);
  return TRUE;
}
