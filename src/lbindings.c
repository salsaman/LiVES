// lbindings.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#define NEED_ENDIAN_TEST

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
  // i, c
  ulong id;
  int val;
  char *string;
} lset;


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
  // i, i, b
  // bitmapped pref
  ulong id;
  int prefidx;
  int bitfield;
  boolean val;
} bmpref;


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
  int track;
  double time;
} mblockdata;



/////////////////////////////////////////
/// extern functions with no headers

boolean lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
boolean lives_osc_cb_play(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);

boolean lives_osc_cb_clip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
boolean lives_osc_cb_bgclip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);



/// osc utils




int padup(char **str, int arglen) {
  int newlen = pad4(arglen);
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  lives_free(ostr);
  return newlen;
}


int add_int_arg(char **str, int arglen, int val) {
  int newlen = arglen + 4;
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  if (!IS_BIG_ENDIAN) {
    (*str)[arglen] = (unsigned char)((val&0xFF000000)>>3);
    (*str)[arglen+1] = (unsigned char)((val&0x00FF0000)>>2);
    (*str)[arglen+2] = (unsigned char)((val&0x0000FF00)>>1);
    (*str)[arglen+3] = (unsigned char)(val&0x000000FF);
  } else {
    lives_memcpy(*str + arglen, &val, 4);
  }
  lives_free(ostr);
  return newlen;
}


static int add_string_arg(char **str, int arglen, const char *val) {
  int newlen = arglen + strlen(val) + 1;
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  lives_memcpy(*str + arglen, val, strlen(val));
  lives_free(ostr);
  return newlen;
}


/////////// value return functions ///////////////////////


static void ext_caller_return_int(ulong caller_id, int ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %d",caller_id,ret);
  if (mainw!=NULL) binding_cb(LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
  lives_free(msgstring);
}


static void ext_caller_return_ulong(ulong caller_id, ulong ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %lu",caller_id,ret);
  if (mainw!=NULL) binding_cb(LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
  lives_free(msgstring);
}


static void ext_caller_return_string(ulong caller_id, const char *ret) {
  // this is for the C++ binding
  char *msgstring = lives_strdup_printf("%lu %s",caller_id,ret);
  if (mainw!=NULL) binding_cb(LIVES_CALLBACK_PRIVATE, msgstring, mainw->id);
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


boolean start_player(void) {
  boolean ret;
  int arglen = 1;
  char **vargs=(char **)lives_malloc(sizeof(char *));
  *vargs = strdup(",");
  arglen = padup(vargs, arglen);

  // this will set our idlefunc and return
  ret = lives_osc_cb_play(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
  if (ret) {
    while (!mainw->error && mainw->playing_file < 0) lives_usleep(prefs->sleep_time);
    if (mainw->error) ret=FALSE;
  }

  lives_free(*vargs);
  lives_free(vargs);
  return ret;
}

enum {
  const_domain_notify,
  const_domain_response,
  const_domain_grav,
  const_domain_insert_mode
};

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

  if (domain==const_domain_grav) {
    if (consta==LIVES_GRAVITY_NORMAL) return GRAV_MODE_NORMAL;
    if (consta==LIVES_GRAVITY_LEFT) return GRAV_MODE_LEFT;
    if (consta==LIVES_GRAVITY_RIGHT) return GRAV_MODE_RIGHT;
  }


  if (domain==const_domain_insert_mode) {
    if (consta==LIVES_INSERT_MODE_NORMAL) return INSERT_MODE_NORMAL;
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
    ret=trans_constant(ret,const_domain_notify);
  }
  ext_caller_return_int(minfo->id,ret);
  lives_free(minfo->msg);
  lives_free(data);
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
  lives_free((char *)*((char **)(oscd->vargs)));
  lives_free((char **)(oscd->vargs));
  lives_free(data);
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
    chooser=choose_file_with_preview(fdata->dir, fdata->title, NULL, preview_type);
    response=lives_dialog_run(LIVES_DIALOG(chooser));
    end_fs_preview();
    mainw->fs_playarea=NULL;

    if (response == LIVES_RESPONSE_ACCEPT) {
      fname=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
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
  } else ext_caller_return_string(ud->id,"");
  lives_free(data);
  return FALSE;
}


static boolean call_set_set_name(livespointer data) {
  ulong uid=(ulong)data;
  boolean ret=FALSE;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1) {
    ret=set_new_set_name(mainw->multitrack);
  } else ext_caller_return_int(uid,(int)ret);
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
  lives_free(opfi->fname);
  lives_free(data);
  return FALSE;
}


static boolean call_reload_set(livespointer data) {
  msginfo *mdata = (msginfo *)data;
  boolean resp=FALSE;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    mainw->osc_auto=1;
    if (!strlen(mdata->msg)) {
      lives_free(mdata->msg);
      mdata->msg=on_load_set_activate(NULL,(livespointer)1);
      if (mdata->msg==NULL) mdata->msg=lives_strdup("");
    }
    if (!is_legal_set_name(mdata->msg,TRUE)) {
      mainw->osc_auto=0;
      resp=FALSE;
    } else {
      mainw->osc_auto=0;
      resp=reload_set(mdata->msg);
    }
  }
  lives_free(mdata->msg);
  ext_caller_return_int(mdata->id,resp);
  lives_free(data);
  return FALSE;
}


static boolean call_set_interactive(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    mainw->interactive=sint->setting;
    set_interactive(mainw->interactive);
    ext_caller_return_int(sint->id,TRUE);
  } else ext_caller_return_int(sint->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_sepwin(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    if (mainw->sep_win!=sint->setting)
      on_sepwin_pressed(NULL,NULL);
    ext_caller_return_int(sint->id,TRUE);
  } else ext_caller_return_int(sint->id,FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_set_fullscreen(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    mainw->interactive=sint->setting;
    if (mainw->fs!=sint->setting)
      on_full_screen_pressed(NULL,NULL);
    ext_caller_return_int(sint->id,TRUE);
  } else ext_caller_return_int(sint->id,FALSE);
  lives_free(data);
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
  } else ext_caller_return_int(sint->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_ping_pong(livespointer data) {
  sintdata *sint = (sintdata *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    if (mainw->ping_pong!=sint->setting)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_ping_pong),!mainw->ping_pong);
    ext_caller_return_int(sint->id,TRUE);
  } else ext_caller_return_int(sint->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_pref_bool(livespointer data) {
  bpref *bdata=(bpref *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    pref_factory_bool(bdata->prefidx, bdata->val);
    ext_caller_return_int(bdata->id,TRUE);
  } else ext_caller_return_int(bdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_pref_int(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    pref_factory_int(idata->prefidx, idata->val);
    ext_caller_return_int(idata->id,TRUE);
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_set_pref_bitmapped(livespointer data) {
  bmpref *bmdata=(bmpref *)data;
  if (mainw!=NULL&&!mainw->go_away) {
    pref_factory_bitmapped(bmdata->prefidx, bmdata->bitfield, bmdata->val);
    ext_caller_return_int(bmdata->id,TRUE);
  } else ext_caller_return_int(bmdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_gravity(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&mainw->multitrack!=NULL) {
    lives_mt_grav_mode_t grav=trans_constant(idata->val,const_domain_grav);
    mainw->multitrack->opts.grav_mode=grav;
    update_grav_mode(mainw->multitrack);
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(idata);
  return FALSE;
}


static boolean call_set_insert_mode(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&mainw->multitrack!=NULL) {
    lives_mt_insert_mode_t mode=trans_constant(idata->val,const_domain_insert_mode);
    mainw->multitrack->opts.insert_mode=mode;
    update_insert_mode(mainw->multitrack);
  } else ext_caller_return_int(idata->id,FALSE);
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
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_insert_vtrack(livespointer data) {
  bpref *bdata=(bpref *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1&&mainw->multitrack!=NULL) {
    int tnum;
    if (!bdata->val) tnum=add_video_track_behind(NULL, mainw->multitrack);
    else tnum=add_video_track_front(NULL, mainw->multitrack);
    ext_caller_return_int(bdata->id,tnum);
  } else ext_caller_return_int(bdata->id,-1);
  lives_free(data);
  return FALSE;
}


static boolean call_mt_set_track_label(livespointer data) {
  lset *ldata=(lset *)data;
  if (mainw!=NULL&&!mainw->go_away&&mainw->multitrack!=NULL && (mt_track_is_video(mainw->multitrack, ldata->val)
      || mt_track_is_audio(mainw->multitrack, ldata->val))) {
    if (ldata->string==NULL) {
      int current_track=mainw->multitrack->current_track;
      mainw->multitrack->current_track=ldata->val;
      on_rename_track_activate(NULL,(livespointer)mainw->multitrack);
      mainw->multitrack->current_track=current_track;
    } else {
      set_track_label_string(mainw->multitrack,ldata->val,ldata->string);
    }
    ext_caller_return_int(ldata->id,TRUE);
  } else ext_caller_return_int(ldata->id,FALSE);
  if (ldata->string!=NULL) lives_free(ldata->string);
  lives_free(data);
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
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_switch_clip(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    switch_clip(idata->prefidx,idata->val,FALSE);
    ext_caller_return_int(idata->id,TRUE);
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_current_time(livespointer data) {
  opfidata *idata=(opfidata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&!mainw->preview&&mainw->playing_file==-1) {
    if (mainw->multitrack!=NULL) {
      if (idata->stime >=0.) {
        if (idata->stime>mainw->multitrack->end_secs) set_timeline_end_secs(mainw->multitrack,idata->stime);
        mt_tl_move(mainw->multitrack,idata->stime);
      }
    } else {
      if (mainw->current_file>0 && idata->stime>=0. && idata->stime<=cfile->total_time) {
        cfile->pointer_time=idata->stime;
        lives_ruler_set_value(LIVES_RULER(mainw->hruler),cfile->pointer_time);
        lives_widget_queue_draw(mainw->hruler);
        get_play_times();
      }
    }
    ext_caller_return_int(idata->id,TRUE);
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_current_audio_time(livespointer data) {
  opfidata *idata=(opfidata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&!mainw->preview&&mainw->playing_file>0&&
      is_realtime_aplayer(prefs->audio_player)&&mainw->multitrack==NULL&&!(mainw->record&&prefs->audio_src==AUDIO_SRC_EXT)&&
      idata->stime>=0.&&idata->stime<=cfile->laudio_time) {
    resync_audio((int)(idata->stime*cfile->fps+.5)+1);
    ext_caller_return_int(idata->id,TRUE);
  } else ext_caller_return_int(idata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_unmap_effects(livespointer data) {
  ulong id=(ulong)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    on_clear_all_clicked(NULL,NULL);
    ext_caller_return_int(id,TRUE);
  } else ext_caller_return_int(id,FALSE);
  return FALSE;
}


static boolean call_stop_playback(livespointer data) {
  ulong id=(ulong)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    on_stop_activate(NULL,NULL); // should send play stop event
    ext_caller_return_int(id,TRUE);
  } else ext_caller_return_int(id,FALSE);
  return FALSE;
}


static boolean call_quit_app(livespointer data) {
  if (mainw!=NULL) {
    mainw->only_close=mainw->no_exit=FALSE;
    mainw->leave_recovery=FALSE;

    if (mainw->was_set) {
      on_save_set_activate(NULL,mainw->set_name);
    } else mainw->leave_files=FALSE;
    lives_exit(0);
  }
  return FALSE;
}


static boolean call_map_effect(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    char *hashname=make_weed_hashname(fxdata->idx,TRUE,FALSE);
    int error=rte_switch_keymode(fxdata->key,fxdata->mode,hashname);
    ext_caller_return_int(fxdata->id,(error==0));
    lives_free(hashname);
  } else ext_caller_return_int(fxdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_unmap_effect(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&rte_keymode_valid(fxdata->key,fxdata->mode,TRUE)) {
    int idx=fxdata->key*rte_getmodespk()+fxdata->mode;
    on_clear_clicked(NULL,LIVES_INT_TO_POINTER(idx));
    ext_caller_return_int(fxdata->id,TRUE);
  } else ext_caller_return_int(fxdata->id,FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_fx_setmode(livespointer data) {
  fxmapdata *fxdata=(fxmapdata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    boolean ret=rte_key_setmode(fxdata->key,fxdata->mode);
    ext_caller_return_int(fxdata->id,(int)ret);
  } else ext_caller_return_int(fxdata->id,FALSE);
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
    } else {
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




static boolean call_choose_layout(livespointer data) {
  iblock *fxdata=(iblock *)data;
  char *lname=lives_strdup("");
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->multitrack!=NULL&&strlen(mainw->set_name)>0) {
    lives_free(lname);
    lname=get_eload_filename(mainw->multitrack,FALSE);
    if (lname==NULL) lname=lives_strdup("");
  }
  ext_caller_return_string(fxdata->id,lname);
  lives_free(lname);
  lives_free(data);
  return FALSE;
}




static boolean call_render_layout(livespointer data) {
  iblock *bdata=(iblock *)data;
  ulong uid=0l;
  boolean ret;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->multitrack!=NULL) {
    ulong new_uid=mainw->files[mainw->multitrack->render_file]->unique_id;
    boolean ra=mainw->multitrack->opts.render_audp;
    boolean rn=mainw->multitrack->opts.normalise_audp;

    mainw->multitrack->opts.render_audp=bdata->with_audio;
    mainw->multitrack->opts.normalise_audp=bdata->ign_sel;

    ret=on_render_activate(LIVES_MENU_ITEM(1),mainw->multitrack);
    if (ret) uid=new_uid;

    mainw->multitrack->opts.render_audp=ra;
    mainw->multitrack->opts.normalise_audp=rn;

  }
  ext_caller_return_ulong(bdata->id,uid);
  lives_free(data);
  return FALSE;
}




static boolean call_reload_layout(livespointer data) {
  msginfo *mdata = (msginfo *)data;
  boolean resp=FALSE;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->multitrack!=NULL&&strlen(mainw->set_name)>0) {
    if (!strlen(mdata->msg)) {
      lives_free(mdata->msg);
      mdata->msg=get_eload_filename(mainw->multitrack,FALSE);
      if (mdata->msg==NULL) mdata->msg=lives_strdup("");
    }
    if (strlen(mainw->msg)) {
      mainw->multitrack->force_load_name=mainw->msg;
      resp=on_load_event_list_activate(NULL,mainw->multitrack);
      mainw->multitrack->force_load_name=NULL;
    }
  }
  lives_free(mdata->msg);
  ext_caller_return_int(mdata->id,resp);
  lives_free(mdata);
  return resp;
}



static boolean call_save_layout(livespointer data) {
  msginfo *mdata = (msginfo *)data;
  boolean resp=FALSE;

  char *lname=lives_strdup("");

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing) {
    if (mdata->msg!=NULL) {
      if (mainw->multitrack!=NULL) lives_snprintf(mainw->multitrack->layout_name,PATH_MAX,"%s",mdata->msg);
      else lives_snprintf(mainw->stored_layout_name,PATH_MAX,"%s",mdata->msg);
    }
    resp=on_save_event_list_activate(NULL,mainw->multitrack);
    if (resp) {
      lives_free(lname);
      lname=lives_strdup(mainw->recent_file);
    }
  }

  lives_free(mdata->msg);
  ext_caller_return_string(mdata->id,lname);
  lives_free(lname);
  lives_free(mdata);
  return resp;
}




static boolean call_set_current_fps(livespointer data) {
  opfidata *idata=(opfidata *)data;
  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file>-1&&mainw->multitrack==NULL) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),idata->stime);
    ext_caller_return_int(idata->id,(int)TRUE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}


static boolean call_set_current_frame(livespointer data) {
  bpref *bdata=(bpref *)data;
  boolean ret;

  char **vargs;

  int arglen=2;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;

  vargs=(char **)lives_malloc(sizeof(char *));

  *vargs = strdup(",i");
  arglen = padup(vargs, arglen);
  arglen = add_int_arg(vargs, arglen, bdata->prefidx);

  if (!bdata->val)
    ret=lives_osc_cb_clip_goto(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
  else
    ret=lives_osc_cb_bgclip_goto(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);

  ext_caller_return_int(bdata->id,(int)ret);
  lives_free(data);
  lives_free((char *)*vargs);
  lives_free(vargs);
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
    } else ext_caller_return_int(idata->id,(int)FALSE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
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
    } else ext_caller_return_int(idata->id,(int)FALSE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
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
    } else ext_caller_return_int(idata->id,(int)FALSE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
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
  } else ext_caller_return_ulong(idata->id,0l);
  lives_free(data);
  return FALSE;
}




static boolean call_remove_block(livespointer data) {
  mblockdata *rdata=(mblockdata *)data;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1&&mainw->multitrack!=NULL) {
    track_rect *oblock=mainw->multitrack->block_selected;
    mainw->multitrack->block_selected=rdata->block;
    delete_block_cb(NULL,(livespointer)mainw->multitrack);
    if (oblock!=rdata->block) mainw->multitrack->block_selected=oblock;
    ext_caller_return_int(rdata->id,(int)TRUE);
  } else ext_caller_return_int(rdata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_move_block(livespointer data) {
  mblockdata *mdata=(mblockdata *)data;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&mainw->playing_file==-1&&mainw->multitrack!=NULL) {
    track_rect *bsel=mainw->multitrack->block_selected;
    int track=get_track_for_block(mdata->block);
    ulong block_uid=mdata->block->uid;
    track_rect *nblock=move_block(mainw->multitrack,mdata->block,mdata->time,track,mdata->track);
    mainw->multitrack->block_selected=bsel;
    if (nblock==NULL) ext_caller_return_int(mdata->id,(int)FALSE);
    else {
      nblock->uid=block_uid;
      ext_caller_return_int(mdata->id,(int)TRUE);
    }
  }
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
          } else {
            ret=rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
            mainw->last_grabbable_effect=grab;
          }
        }
      }
    } else {
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
    } else {
      if (lmode & LIVES_LOOP_MODE_CONTINUOUS) {
        if (!mainw->loop_cont) on_loop_button_activate(NULL,NULL);
      } else if (mainw->loop_cont) on_loop_button_activate(NULL,NULL);

      if (lmode & LIVES_LOOP_MODE_FIT_AUDIO) {
        if (mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
      } else if (!mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
    }
    ext_caller_return_int(idata->id,(int)TRUE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_resync_fps(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw!=NULL&& mainw->playing_file>-1) {
    fps_reset_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    ext_caller_return_int(idata->id,(int)TRUE);
  } else ext_caller_return_int(idata->id,(int)FALSE);
  lives_free(data);
  return FALSE;
}



static boolean call_cancel_proc(livespointer data) {
  ipref *idata=(ipref *)data;
  if (mainw==NULL||mainw->current_file==-1||cfile==NULL||cfile->proc_ptr==NULL||
      !lives_widget_is_visible(cfile->proc_ptr->cancel_button)) {
    ext_caller_return_int(idata->id,(int)FALSE);
  } else {
    on_cancel_keep_button_clicked(NULL,NULL);
    ext_caller_return_int(idata->id,(int)TRUE);
  }
  lives_free(data);
  return FALSE;
}




//////////////////////////////////////////////////////////////////

/// idlefunc hooks


boolean idle_show_info(const char *text, boolean blocking, ulong id) {
  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;
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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  info = (ipref *)lives_malloc(sizeof(ipref));
  info->id = id;
  info->val = tnum;
  lives_idle_add(call_mt_set_track,(livespointer)info);
  return TRUE;
}


boolean idle_set_track_label(int tnum, const char *label, ulong id) {
  lset *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data = (lset *)lives_malloc(sizeof(lset));
  data->id = id;
  data->val=tnum;
  if (label!=NULL) data->string=lives_strdup(label);
  else data->string=NULL;
  lives_idle_add(call_mt_set_track_label,(livespointer)data);
  return TRUE;
}


boolean idle_insert_vtrack(boolean in_front, ulong id) {
  bpref *data;

  if (mainw==NULL||mainw->playing_file == -1) return FALSE;
  if (mainw->multitrack != NULL) return FALSE;

  data=(bpref *)lives_malloc(sizeof(bpref));
  data->val=in_front;
  lives_idle_add(call_insert_vtrack,(livespointer)data);
  return TRUE;
}



boolean idle_set_current_time(double time, ulong id) {
  opfidata *info;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;

  info = (opfidata *)lives_malloc(sizeof(opfidata));
  info->id = id;
  info->stime=time;
  lives_idle_add(call_set_current_time,(livespointer)info);
  return TRUE;
}


boolean idle_set_current_audio_time(double time, ulong id) {
  opfidata *info;

  if (mainw!=NULL&&!mainw->go_away&&!mainw->is_processing&&!mainw->preview&&mainw->playing_file>0&&
      is_realtime_aplayer(prefs->audio_player)&&mainw->multitrack==NULL&&!(mainw->record&&prefs->audio_src==AUDIO_SRC_EXT)&&
      time>=0.&&time<=cfile->laudio_time) {
    info = (opfidata *)lives_malloc(sizeof(opfidata));
    info->id = id;
    info->stime=time;
    lives_idle_add(call_set_current_audio_time,(livespointer)info);
    return TRUE;
  }
  return FALSE;
}


boolean idle_unmap_effects(ulong id) {
  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;

  lives_idle_add(call_unmap_effects,(livespointer)id);

  return TRUE;
}


boolean idle_stop_playback(ulong id) {
  if (mainw==NULL||mainw->go_away||mainw->is_processing) return FALSE;

  lives_idle_add(call_stop_playback,(livespointer)id);

  return TRUE;
}



boolean idle_quit(pthread_t *gtk_thread) {
  if (mainw==NULL) return FALSE;

  lives_idle_add(call_quit_app,NULL);

  pthread_join(*gtk_thread,NULL);

  return TRUE;
}


boolean idle_save_set(const char *name, boolean force_append, ulong id) {
  oscdata *data;
  char **vargs;

  int arglen=3;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;

  vargs=(char **)lives_malloc(sizeof(char *));

  *vargs = strdup(",si");
  arglen = padup(vargs, arglen);
  arglen = add_string_arg(vargs, arglen, name);
  arglen = add_int_arg(vargs, arglen, force_append);

  data= (oscdata *)lives_malloc(sizeof(oscdata));
  data->id=id;
  data->arglen=arglen;
  data->vargs=vargs;
  lives_idle_add(call_osc_save_set,(livespointer)data);

  return TRUE;
}


boolean idle_choose_file_with_preview(const char *dirname, const char *title, int preview_type, ulong id) {
  fprev *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;

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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->was_set) return FALSE;

  data=(udata *)lives_malloc(sizeof(udata));
  data->id=id;
  lives_idle_add(call_choose_set,(livespointer)data);
  return TRUE;
}



boolean idle_set_set_name(ulong id) {
  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file) return FALSE;
  lives_idle_add(call_set_set_name,(livespointer)id);
  return TRUE;
}


boolean idle_open_file(const char *fname, double stime, int frames, ulong id) {
  opfidata *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->was_set) return FALSE;

  if (setname==NULL) return FALSE;

  if (strlen(setname)&&!is_legal_set_name(setname,TRUE)) return FALSE;

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



boolean idle_set_gravity(int grav, ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=grav;
  lives_idle_add(call_set_gravity,(livespointer)data);
  return TRUE;
}


boolean idle_set_insert_mode(int mode, ulong id) {
  ipref *data;

  if (mainw==NULL||mainw->go_away) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=mode;
  lives_idle_add(call_set_insert_mode,(livespointer)data);
  return TRUE;
}



boolean idle_map_fx(int key, int mode, int idx, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;

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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;

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

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=mode;
  data->id=id;

  lives_idle_add(call_fx_setmode,(livespointer)data);
  return TRUE;
}



boolean idle_fx_enable(int key, boolean setting, ulong id) {
  fxmapdata *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing) return FALSE;

  data=(fxmapdata *)lives_malloc(sizeof(fxmapdata));
  data->key=key;
  data->mode=setting;
  data->id=id;

  lives_idle_add(call_fx_enable,(livespointer)data);
  return TRUE;
}



boolean idle_set_pref_bool(int prefidx, boolean val, ulong id) {
  bpref *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away) return FALSE;

  data=(bpref *)lives_malloc(sizeof(bpref));
  data->id=id;
  data->prefidx=prefidx;
  data->val=val;
  lives_idle_add(call_set_pref_bool,(livespointer)data);
  return TRUE;
}


boolean idle_set_pref_int(int prefidx, int val, ulong id) {
  ipref *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->prefidx=prefidx;
  data->val=val;
  lives_idle_add(call_set_pref_int,(livespointer)data);
  return TRUE;
}


boolean idle_set_pref_bitmapped(int prefidx, int bitfield, boolean val, ulong id) {
  bmpref *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away) return FALSE;

  data=(bmpref *)lives_malloc(sizeof(bmpref));
  data->id=id;
  data->prefidx=prefidx;
  data->bitfield=bitfield;
  data->val=val;
  lives_idle_add(call_set_pref_bitmapped,(livespointer)data);
  return TRUE;
}



boolean idle_set_if_mode(lives_interface_mode_t mode, ulong id) {
  ipref *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=(int)mode;
  lives_idle_add(call_set_if_mode,(livespointer)data);
  return TRUE;
}



boolean idle_insert_block(int clipno, boolean ign_sel, boolean with_audio, ulong id) {
  iblock *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
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
  mblockdata *data;
  track_rect *tr;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  tr=find_block_by_uid(mainw->multitrack, uid);
  if (tr==NULL) return FALSE;

  data=(mblockdata *)lives_malloc(sizeof(mblockdata));
  data->id=id;
  data->block=tr;
  lives_idle_add(call_remove_block,(livespointer)data);
  return TRUE;
}


boolean idle_move_block(ulong uid, int track, double time, ulong id) {
  mblockdata *data;
  track_rect *tr;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  tr=find_block_by_uid(mainw->multitrack, uid);
  if (tr==NULL) return FALSE;

  if (track>=lives_list_length(mainw->multitrack->video_draws)||track<-mainw->multitrack->opts.back_audio_tracks) return FALSE;

  if (time<0.) return FALSE;

  data=(mblockdata *)lives_malloc(sizeof(mblockdata));
  data->id=id;
  data->block=tr;
  data->track=track;
  data->time=time;
  lives_idle_add(call_move_block,(livespointer)data);
  return TRUE;
}


boolean idle_wipe_layout(boolean force, ulong id) {
  iblock *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(iblock *)lives_malloc(sizeof(iblock));
  data->ign_sel=force;
  data->id=id;
  lives_idle_add(call_wipe_layout,(livespointer)data);
  return TRUE;
}



boolean idle_choose_layout(ulong id) {
  iblock *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(iblock *)lives_malloc(sizeof(iblock));
  data->id=id;
  lives_idle_add(call_choose_layout,(livespointer)data);
  return TRUE;
}



boolean idle_reload_layout(const char *lname, ulong id) {
  msginfo *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(msginfo *)lives_malloc(sizeof(msginfo));
  data->id=id;
  data->msg=lives_strdup(lname);
  lives_idle_add(call_reload_layout,(livespointer)data);
  return TRUE;
}




boolean idle_save_layout(const char *lname, ulong id) {
  msginfo *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(msginfo *)lives_malloc(sizeof(msginfo));
  data->id=id;
  if (lname!=NULL) data->msg=lives_strdup(lname);
  else data->msg=NULL;
  lives_idle_add(call_save_layout,(livespointer)data);
  return TRUE;
}



boolean idle_render_layout(boolean with_aud, boolean normalise_aud, ulong id) {
  iblock *data;

  if (mainw==NULL||(mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))||mainw->go_away||mainw->is_processing||
      mainw->playing_file>-1) return FALSE;
  if (mainw->multitrack==NULL) return FALSE;

  data=(iblock *)lives_malloc(sizeof(iblock));
  data->with_audio=with_aud;
  data->ign_sel=normalise_aud;
  data->id=id;
  lives_idle_add(call_render_layout,(livespointer)data);
  return TRUE;
}



boolean idle_select_all(int cnum, ulong id) {
  ipref *data;

  if (mainw==NULL||((mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))&&mainw->multitrack==NULL)||mainw->go_away||
      mainw->is_processing) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=cnum;
  lives_idle_add(call_select_all,(livespointer)data);
  return TRUE;
}


boolean idle_select_start(int cnum, int frame, ulong id) {
  ipref *data;

  if (mainw==NULL||((mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))&&mainw->multitrack==NULL)||mainw->go_away||
      mainw->is_processing) return FALSE;

  data=(ipref *)lives_malloc(sizeof(ipref));
  data->id=id;
  data->val=cnum;
  data->prefidx=frame;
  lives_idle_add(call_select_start,(livespointer)data);
  return TRUE;
}



boolean idle_select_end(int cnum, int frame, ulong id) {
  ipref *data;

  if (mainw==NULL||((mainw->preview||(mainw->multitrack==NULL&&mainw->event_list!=NULL))&&mainw->multitrack==NULL)||mainw->go_away||
      mainw->is_processing) return FALSE;

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


boolean idle_set_current_frame(int frame, boolean bg, ulong id) {
  bpref *data;

  if (mainw==NULL||mainw->playing_file == -1) return FALSE;
  if (mainw->multitrack != NULL) return FALSE;

  data=(bpref *)lives_malloc(sizeof(bpref));
  data->prefidx=frame;
  data->val=bg;
  lives_idle_add(call_set_current_frame,(livespointer)data);
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
