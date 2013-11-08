// osc.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2013 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef IS_MINGW
#include <netinet/in.h>
#endif

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"

#ifdef ENABLE_OSC

#include "osc.h"
#include "htmsocket.h"
#include "callbacks.h"
#include "effects.h"
#include "support.h"
#include "rte_window.h"
#include "resample.h"
#include "paramwindow.h"
#include "ce_thumbs.h"

void *status_socket;
void *notify_socket;

static lives_osc *livesOSC=NULL;

#define CONSTLEN 8

static char constval[CONSTLEN];

static boolean via_shortcut=FALSE;

#define OSC_STRING_SIZE 256

#define FX_MAX FX_KEYS_MAX_VIRTUAL-1



/* convert a big endian 32 bit string to an int for internal use */

static int toInt(const char* b) {
  if (capable->byte_order==LIVES_LITTLE_ENDIAN) {
    return (( (int) b[3] ) & 0xff ) + ((((int) b[2]) & 0xff) << 8) + ((((int) b[1]) & 0xff) << 16) +
      ((((int) b[0] ) & 0xff) << 24);
  }
  return (( (int) b[0] ) & 0xff ) + ((((int) b[1]) & 0xff) << 8) + ((((int) b[2]) & 0xff) << 16) +
    ((((int) b[3] ) & 0xff) << 24);
}


// wrapper for correct typing
static LIVES_INLINE void *lives_size_malloc(int size) {
  return lives_malloc((size_t)size);
}

static boolean using_types;
static int osc_header_len;
static int offset;

static LIVES_INLINE G_GNUC_CONST int pad4(int val) {
  return (int)((val+4)/4)*4;
}

static int lives_osc_get_num_arguments(const void *vargs) {
  // check if using type tags and get num_arguments
  const char *args=(const char *)vargs;
  if (args[0]!=0x2c) return 0;
  return strlen(args)-1;
}


static boolean lives_osc_check_arguments(int arglen, const void *vargs, const char *check_pattern, boolean calc_header_len) {
  // check if using type tags and get header_len
  // should be called from each cb that uses parameters
  const char *args=(const char *)vargs;
  int header_len;

  osc_header_len=0;
  offset=0;

  using_types=FALSE;
  if (arglen<4||args[0] != 0x2c) return FALSE; // missing comma or typetags
  using_types=TRUE;

  header_len=pad4(strlen(check_pattern)+1);

  if (arglen<header_len) return FALSE;
  if (!strncmp (check_pattern,++args,strlen (check_pattern))) {
    if (calc_header_len) osc_header_len=header_len;
    return TRUE;
  }
  return FALSE;
}


/* not used yet */
/*static void lives_osc_parse_char_argument(const void *vargs, char *dst)
  {
  const char *args = (char*)vargs;
  strncpy(dst, args+osc_header_len+offset,1);
  offset+=4;
  }*/



static void lives_osc_parse_string_argument(const void *vargs, char *dst)
{
  const char *args = (char*)vargs;
  g_snprintf(dst, OSC_STRING_SIZE, "%s", args+osc_header_len+offset);
  offset+=pad4(strlen (dst));
}



static void lives_osc_parse_int_argument(const void *vargs, int *arguments)
{
  const char *args = (char*)vargs;
  arguments[0] = toInt( args + osc_header_len + offset );
  offset+=4;
}

static void lives_osc_parse_float_argument(const void *vargs, float *arguments)
{
  const char *args = (char*)vargs;
  arguments[0] = LEFloat_to_BEFloat( *((float*)(args + osc_header_len + offset)) );
  offset+=4;
}






/* memory allocation functions of libOMC_dirty (OSC) */


void *_lives_osc_time_malloc(int num_bytes) {
  return g_malloc( num_bytes );
}

void *_lives_osc_rt_malloc(int num_bytes) {
  return g_malloc( num_bytes );
}


// status returns

boolean lives_status_send (const char *msgstring) {
  if (status_socket==NULL) return FALSE;
  else {
    char *msg=g_strdup_printf("%s\n",msgstring);
    boolean retval = lives_stream_out (status_socket,strlen (msg)+1,(void *)msg);
    g_free(msg);
    return retval;
  }
}


boolean lives_osc_notify (int msgnumber,const char *msgstring) {
  if (notify_socket==NULL) return FALSE;
  if (!prefs->omc_events&&(msgnumber!=LIVES_OSC_NOTIFY_SUCCESS
			   &&msgnumber!=LIVES_OSC_NOTIFY_FAILED)) return FALSE;
  else {
    char *msg;
    boolean retval;
    if (msgstring!=NULL) {
      msg=g_strdup_printf("%d|%s\n",msgnumber,msgstring);
    }
    else msg=g_strdup_printf("%d\n",msgnumber);
    retval = lives_stream_out (notify_socket,strlen (msg)+1,(void *)msg);
    g_free(msg);
    return retval;
  }
}

void lives_osc_notify_success (const char *msg) {
  if (prefs->omc_noisy)
    lives_osc_notify(LIVES_OSC_NOTIFY_SUCCESS,msg);
}

void lives_osc_notify_failure (void) {
  if (prefs->omc_noisy)
    lives_osc_notify(LIVES_OSC_NOTIFY_FAILED,NULL);
}

/* unused */
/*
  void lives_osc_notify_cancel (void) {
  if (prefs->omc_noisy);
  lives_osc_notify(LIVES_OSC_NOTIFY_CANCELLED,NULL);
  }*/



void lives_osc_close_status_socket (void) {
  if (status_socket!=NULL) CloseHTMSocket (status_socket);
  status_socket=NULL;
}

void lives_osc_close_notify_socket (void) {
  if (notify_socket!=NULL) CloseHTMSocket (notify_socket);
  notify_socket=NULL;
}



static LIVES_INLINE const char *get_value_of(const int what) {
  snprintf(constval,CONSTLEN,"%d",what);
  return (const char *)&constval;
}





static const char *get_omc_const(const char *cname) {

  // looping modes
  if (!strcmp(cname,"LIVES_LOOP_NONE")) return "0";
  if (!strcmp(cname,"LIVES_LOOP_FIT")) return "2";
  if (!strcmp(cname,"LIVES_LOOP_CONT")) return "1";

  // interface modes
  if (!strcmp(cname,"LIVES_MODE_CLIPEDIT")) return "0";
  if (!strcmp(cname,"LIVES_MODE_MULTITRACK")) return "1";

  // parameter types
  if (!strcmp(cname,"LIVES_PARAM_TYPE_INTEGER")) 
    return get_value_of((const int)WEED_HINT_INTEGER);
  if (!strcmp(cname,"LIVES_PARAM_TYPE_FLOAT")) 
    return get_value_of((const int)WEED_HINT_FLOAT);
  if (!strcmp(cname,"LIVES_PARAM_TYPE_BOOL")) 
    return get_value_of((const int)WEED_HINT_SWITCH);
  if (!strcmp(cname,"LIVES_PARAM_TYPE_STRING")) 
    return get_value_of((const int)WEED_HINT_TEXT);
  if (!strcmp(cname,"LIVES_PARAM_TYPE_COLOR")) 
    return get_value_of((const int)WEED_HINT_COLOR);

  // colorspaces
  if (!strcmp(cname,"LIVES_COLORSPACE_RGB_INT")) 
    return "1";
  if (!strcmp(cname,"LIVES_COLORSPACE_RGBA_INT")) 
    return "2";
  if (!strcmp(cname,"LIVES_COLORSPACE_RGB_FLOAT")) 
    return "3";
  if (!strcmp(cname,"LIVES_COLORSPACE_RGBA_FLOAT")) 
    return "4";

  // boolean values
  if (!strcmp(cname,"LIVES_TRUE")) return "1";
  if (!strcmp(cname,"LIVES_FALSE")) return "0";

  // parameter flags
  if (!strcmp(cname,"LIVES_PARAM_FLAGS_REINIT_ON_VALUE_CHANGE")) 
    return get_value_of((const int)WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  if (!strcmp(cname,"LIVES_PARAM_FLAGS_VARIABLE_ELEMENTS"))
    return get_value_of((const int)WEED_PARAMETER_VARIABLE_ELEMENTS);
  if (!strcmp(cname,"LIVES_PARAM_FLAGS_ELEMENT_PER_CHANNEL"))
    return get_value_of((const int)WEED_PARAMETER_ELEMENT_PER_CHANNEL);

  // notification types
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_SUCCESS")) 
    return get_value_of((const int)LIVES_OSC_NOTIFY_SUCCESS);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_FAILED")) 
    return get_value_of((const int)LIVES_OSC_NOTIFY_FAILED);

  // notification events
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_FRAME_SYNCH")) 
    return get_value_of((const int)LIVES_OSC_NOTIFY_FRAME_SYNCH);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_PLAYBACK_STARTED")) 
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STARTED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_PLAYBACK_STOPPED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STOPPED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_RECORD_STARTED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_RECORD_STARTED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_RECORD_STOPPED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_RECORD_STOPPED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_QUIT"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_QUIT);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_CLIP_OPENED")) 
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIP_OPENED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_CLIP_CLOSED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIP_CLOSED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_CLIPSET_OPENED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIPSET_OPENED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_CLIPSET_SAVED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIPSET_SAVED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_SUCCESS"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_SUCCESS);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_FAILED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_FAILED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_CANCELLED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CANCELLED);
  if (!strcmp(cname,"LIVES_OSC_NOTIFY_MODE_CHANGED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_MODE_CHANGED);

  // generic constants
  if (!strcmp(cname,"LIVES_FPS_MAX")) 
    return get_value_of((const int)FPS_MAX);

  if (!strcmp(cname,"LIVES_DEFAULT_OVERRIDDEN")) 
    return "2";

  lives_osc_notify_failure();

  return "";
}


static char *lives_osc_format_result(weed_plant_t *plant, const char *key, int st, int end) {
  int stype;
  int error,i;

  char *retval=NULL,*tmp;

  if (end==-1) end=weed_leaf_num_elements(plant,key);

  if (end<=st) return g_strdup("");

  stype=weed_leaf_seed_type(plant,key);

  switch (stype) {
  case WEED_SEED_INT:
    {
      int *vals=weed_get_int_array(plant,key,&error);
      for (i=st;i<end;i++) {
	if (retval==NULL) tmp=g_strdup_printf("%d",vals[i]);
	else {
	  tmp=g_strdup_printf("%s,%d",retval,vals[i]);
	  g_free(retval);
	}
	retval=tmp;
      }
      weed_free(vals);
      break;
    }

  case WEED_SEED_DOUBLE:
    {
      double *vals=weed_get_double_array(plant,key,&error);
      for (i=st;i<end;i++) {
	if (retval==NULL) tmp=g_strdup_printf("%f",vals[i]);
	else {
	  tmp=g_strdup_printf("%s,%f",retval,vals[i]);
	  g_free(retval);
	}
	retval=tmp;
      }
      weed_free(vals);
      break;
    }

  case WEED_SEED_BOOLEAN:
    {
      int *vals=weed_get_boolean_array(plant,key,&error);
      for (i=st;i<end;i++) {
	if (retval==NULL) tmp=g_strdup_printf("%d",vals[i]==WEED_TRUE);
	else {
	  tmp=g_strdup_printf("%s,%d",retval,vals[i]==WEED_TRUE);
	  g_free(retval);
	}
	retval=tmp;
      }
      weed_free(vals);
      break;
    }

  case WEED_SEED_STRING:
    {
      char **vals=weed_get_string_array(plant,key,&error);
      char *tmp2;
      for (i=st;i<end;i++) {
	if (retval==NULL) tmp=g_strdup_printf("\"%s\"",(tmp2=subst(vals[i],"\"","\\\"")));
	else {
	  tmp=g_strdup_printf("%s,\"%s\"",retval,(tmp2=subst(vals[i],"\"","\\\"")));
	  g_free(retval);
	}
	g_free(tmp2);
	retval=tmp;
	weed_free(vals[i]);
      }
      weed_free(vals);
      break;
    }
  }
  return retval;
}








///////////////////////////////////// CALLBACKS FOR OSC ////////////////////////////////////////
// TODO - handle clipboard playback

void lives_osc_cb_test(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int val=lives_osc_get_num_arguments (vargs);
  g_printerr("got %d\n",val);
}

/* /video/play */
void lives_osc_cb_play (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  float ent, stt;
  double entd,sttd;

  if (mainw->go_away) return lives_osc_notify_failure();
  mainw->osc_auto=TRUE; ///< request early notifiction of success

  if (mainw->current_file<=0||mainw->playing_file!=-1) return lives_osc_notify_failure();

  mainw->play_start=calc_frame_from_time(mainw->current_file,
					 cfile->pointer_time);
  mainw->play_end=cfile->frames;

  if (!lives_osc_check_arguments (arglen,vargs,"ff",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"f",FALSE)) {
      if (!lives_osc_check_arguments (arglen,vargs,"",FALSE)) {
	return lives_osc_notify_failure();
      }
    }
    else {
      lives_osc_check_arguments (arglen,vargs,"f",TRUE);
      lives_osc_parse_float_argument(vargs,&stt);
      sttd=(double)stt;
      mainw->play_start=calc_frame_from_time(mainw->current_file,
					     sttd);
    }
  }
  else{
    lives_osc_check_arguments (arglen,vargs,"ff",TRUE);
    lives_osc_parse_float_argument(vargs,&stt);
    lives_osc_parse_float_argument(vargs,&ent);
    sttd=(double)stt;
    entd=(double)ent;
    mainw->play_end=calc_frame_from_time(mainw->current_file,
					 entd);
    mainw->play_start=calc_frame_from_time(mainw->current_file,
					   sttd);


  }

  // re - add the timer, as we will hang here, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_timeout_remove (mainw->kb_timer);
  mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
  mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif  
  on_playall_activate(NULL,NULL);
#if GTK_CHECK_VERSION(3,0,0)
    mainw->kb_timer_end=TRUE;
#endif

  mainw->osc_auto=FALSE;
}

void lives_osc_cb_playsel (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return lives_osc_notify_failure();
  if (mainw->playing_file==-1&&mainw->current_file>0) {
    mainw->osc_auto=TRUE; ///< request early notifiction of success

    // re - add the timer, as we will hang here, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
    gtk_timeout_remove (mainw->kb_timer);
    mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
    mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif

    if (mainw->multitrack==NULL) on_playsel_activate(NULL,NULL);
    else multitrack_play_sel(NULL, mainw->multitrack);
#if GTK_CHECK_VERSION(3,0,0)
    mainw->kb_timer_end=TRUE;
#endif
    mainw->osc_auto=FALSE; ///< request early notifiction of success
  }
}

void lives_osc_cb_play_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->current_file<0||((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||mainw->playing_file==-1)) if (mainw->playing_file==-1) lives_osc_notify_failure();
  dirchange_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(TRUE));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgplay_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file||mainw->playing_file==-1) if (mainw->playing_file==-1) lives_osc_notify_failure();

  if (mainw->current_file<0||(mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)) if (mainw->playing_file==-1) lives_osc_notify_failure();

  mainw->files[mainw->blend_file]->pb_fps=-mainw->files[mainw->blend_file]->pb_fps;
                                                                                                                          
  lives_osc_notify_success(NULL);
                                                                                             
}


void lives_osc_cb_play_forward (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) lives_osc_notify_failure(); // not ready to play yet

  if (mainw->current_file<0||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) if (mainw->playing_file==1) lives_osc_notify_failure();

  if (mainw->playing_file==-1&&mainw->current_file>0) {
    mainw->osc_auto=TRUE; ///< request early notifiction of success

    // re - add the timer, as we will hang here, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
    gtk_timeout_remove (mainw->kb_timer);
    mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
    mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif

    on_playall_activate(NULL,NULL);

#if GTK_CHECK_VERSION(3,0,0)
    mainw->kb_timer_end=TRUE;
#endif
    mainw->osc_auto=FALSE;
  }
  else if (mainw->current_file>0) {
    if (cfile->pb_fps<0||(cfile->play_paused&&cfile->freeze_fps<0)) dirchange_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(TRUE));
    if (cfile->play_paused) freeze_callback(NULL,NULL,0,(LiVESModifierType)0,NULL);
    lives_osc_notify_success(NULL);
  }
  else lives_osc_notify_failure();

}


void lives_osc_cb_play_backward (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<0||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  if (mainw->playing_file==-1&&mainw->current_file>0) {
    mainw->reverse_pb=TRUE;

    // re - add the timer, as we will hang here, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_timeout_remove (mainw->kb_timer);
  mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
  mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif  
    mainw->osc_auto=TRUE; ///< request early notifiction of success

    on_playall_activate(NULL,NULL);
#if GTK_CHECK_VERSION(3,0,0)
    mainw->kb_timer_end=TRUE;
#endif
    mainw->osc_auto=FALSE;
  }
  else if (mainw->current_file>0) {
    if (cfile->pb_fps>0||(cfile->play_paused&&cfile->freeze_fps>0)) dirchange_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(TRUE));
    if (cfile->play_paused) freeze_callback(NULL,NULL,0,(LiVESModifierType)0,NULL);
    lives_osc_notify_success(NULL);
  }
  else lives_osc_notify_failure();

}


void lives_osc_cb_play_faster (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  on_faster_pressed(NULL,LIVES_INT_TO_POINTER(1));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgplay_faster (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  on_faster_pressed(NULL,LIVES_INT_TO_POINTER(2));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_play_slower (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  on_slower_pressed(NULL,LIVES_INT_TO_POINTER(1));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgplay_slower (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  on_slower_pressed(NULL,LIVES_INT_TO_POINTER(2));
  lives_osc_notify_success(NULL);

}



void lives_osc_cb_play_reset (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  fps_reset_callback(NULL,NULL,0,(LiVESModifierType)0,NULL);
  if (cfile->pb_fps<0||(cfile->play_paused&&cfile->freeze_fps<0)) dirchange_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(TRUE));
  if (cfile->play_paused) freeze_callback(NULL,NULL,0,(LiVESModifierType)0,NULL);

  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgplay_reset (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->playing_file==-1) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  if (mainw->files[mainw->blend_file]->play_paused) {
    mainw->files[mainw->blend_file]->play_paused=FALSE;
  }

  if (mainw->files[mainw->blend_file]->pb_fps>=0.) mainw->files[mainw->blend_file]->pb_fps=mainw->files[mainw->blend_file]->fps;
  else mainw->files[mainw->blend_file]->pb_fps=-mainw->files[mainw->blend_file]->fps;
  lives_osc_notify_success(NULL);

}




/* /video/stop */
void lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file>-1) {
    on_stop_activate(NULL,NULL); // should send play stop event
    lives_osc_notify_success(NULL);
  }
  else lives_osc_notify_failure();
}



void lives_osc_cb_set_loop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&lmode);
  }
  else return lives_osc_notify_failure();

  if (lmode==atoi(get_omc_const("LIVES_LOOP_NONE"))) {
    if (mainw->loop_cont) on_loop_cont_activate(NULL,NULL);
    if (mainw->loop) on_loop_video_activate(NULL,NULL);
  }
  else {
    if (lmode & atoi(get_omc_const("LIVES_LOOP_CONT"))) {
      if (!mainw->loop_cont) on_loop_cont_activate(NULL,NULL);
    }
    else if (mainw->loop_cont) on_loop_cont_activate(NULL,NULL);

    if (lmode & atoi(get_omc_const("LIVES_LOOP_FIT"))) {
      if (mainw->loop) on_loop_video_activate(NULL,NULL);
    }
    else if (!mainw->loop) on_loop_video_activate(NULL,NULL);
  }

  lives_osc_notify_success(NULL);

}




void lives_osc_cb_get_loop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode=0;
  char *lmodes;

  if (mainw->loop) lmode|=atoi(get_omc_const("LIVES_LOOP_FIT"));
  if (mainw->loop_cont) lmode|=atoi(get_omc_const("LIVES_LOOP_CONT"));

  lmodes=g_strdup_printf("%d",lmode);
  lives_status_send(lmodes);
  g_free(lmodes);

}


void lives_osc_cb_set_pingpong(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&lmode);
  }
  else return lives_osc_notify_failure();

  if ((lmode && !mainw->ping_pong) || (!lmode && mainw->ping_pong)) on_ping_pong_activate(NULL,NULL);

  lives_osc_notify_success(NULL);
}



void lives_osc_cb_get_pingpong(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->ping_pong) lives_status_send(get_omc_const("LIVES_TRUE"));
  else lives_status_send(get_omc_const("LIVES_FALSE"));
}




void lives_osc_cb_set_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs,&fps);
  }

  if (mainw->playing_file>-1) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(double)(fps));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgset_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs,&fps);
  }

  mainw->files[mainw->blend_file]->pb_fps=(double)fps;
  lives_osc_notify_success(NULL);
}



void lives_osc_cb_set_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs,&fps);
  }

  if (mainw->playing_file>-1) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(double)(fps)*mainw->files[mainw->playing_file]->fps);
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_bgset_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs,&fps);
  }

  mainw->files[mainw->blend_file]->pb_fps=mainw->files[mainw->blend_file]->fps*(double)fps;

}






void lives_osc_cb_fx_reset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (!mainw->osc_block) rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(0));
  if (prefs->omc_noisy) lives_osc_notify_success(NULL);

}

void lives_osc_cb_fx_map_clear(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->osc_block) on_clear_all_clicked(NULL,NULL);
  if (prefs->omc_noisy) lives_osc_notify_success(NULL);
}

void lives_osc_cb_fx_map(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  char effect_name[OSC_STRING_SIZE];

  if (!lives_osc_check_arguments (arglen,vargs,"is",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_string_argument(vargs,effect_name);
  if (!mainw->osc_block) weed_add_effectkey(effect_key,effect_name,FALSE); // allow partial matches
  if (prefs->omc_noisy) lives_osc_notify_success(NULL);
}

void lives_osc_cb_fx_enable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // if via_shortcut and not playing, we ignore unless a generator starts (which starts playback)
#if GTK_CHECK_VERSION(3,0,0)
  boolean new_timer_added=FALSE;
#endif
  int count;
  int effect_key;
  int grab=mainw->last_grabable_effect;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (!(mainw->rte&(GU641<<(effect_key-1)))) {
    weed_plant_t *filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
    if (filter==NULL) return lives_osc_notify_failure();
    count=enabled_in_channels(filter, FALSE);
    if (mainw->playing_file==-1&&via_shortcut&&count!=0) return lives_osc_notify_failure(); // is no generator
    
    if (!mainw->osc_block) {
      if (mainw->playing_file==-1&&count==0) {
	// re - add the timer, as we hang here if a generator is started, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
	gtk_timeout_remove (mainw->kb_timer);
	mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
	mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
	new_timer_added=TRUE;
#endif
      }
      // TODO ***
      //mainw->osc_auto=TRUE; ///< request early notifiction of success
      rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
      mainw->osc_auto=FALSE;
#if GTK_CHECK_VERSION(3,0,0)
      if (new_timer_added)
	mainw->kb_timer_end=TRUE;
#endif
    }
  }
  mainw->last_grabable_effect=grab;

  if (prefs->omc_noisy) lives_osc_notify_success(NULL);
}

void lives_osc_cb_fx_disable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (mainw->rte&(GU641<<(effect_key-1))) {
    if (!mainw->osc_block) rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
  }
  if (prefs->omc_noisy) lives_osc_notify_success(NULL);
}


void lives_osc_cb_fx_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // if not playing and via_shortcut, see if fx key points to generator

  int count=0;
  int effect_key;
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (!(mainw->rte&(GU641<<(effect_key-1)))&&mainw->playing_file==-1) {
    weed_plant_t *filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
    if (filter==NULL) return lives_osc_notify_failure();
    count=enabled_in_channels(filter, FALSE);
    if (via_shortcut&&count!=0) return lives_osc_notify_failure(); // is no generator
  }
  if (!mainw->osc_block) {
    if (!(mainw->rte&(GU641<<(effect_key-1)))&&mainw->playing_file==-1&&count==0&&via_shortcut) {
      // re - add the timer, as we hang here if a generator is started, and we want to receive messages still during playback
#if !GTK_CHECK_VERSION(3,0,0)
      gtk_timeout_remove (mainw->kb_timer);
      mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
      mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif
    }
    // TODO ***
    //mainw->osc_auto=TRUE; ///< request early notifiction of success
    rte_on_off_callback_hook(NULL,LIVES_INT_TO_POINTER(effect_key));
#if GTK_CHECK_VERSION(3,0,0)
    mainw->kb_timer_end=TRUE;
#endif
  }
  if (prefs->omc_noisy) lives_osc_notify_success(NULL);
}

// *_set will allow setting of invalid clips - in this case nothing happens
//*_select will index only valid clips


void lives_osc_cb_fgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip

  int clip;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES-1) {
    if (mainw->files[clip]!=NULL&&(mainw->files[clip]->clip_type==CLIP_TYPE_DISK||mainw->files[clip]->clip_type==CLIP_TYPE_FILE)) {
      if (mainw->playing_file!=0) {
	switch_clip(1,clip);
	if (prefs->omc_noisy) {
	  char *msg=g_strdup_printf("%d",clip);
	  lives_osc_notify_success(msg);
	  g_free(msg);
	}
	return;
      }
    }
  }
  lives_osc_notify_failure();
}


void lives_osc_cb_bgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch bg clip

  int clip;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES-1) {
    if (mainw->files[clip]!=NULL&&(mainw->files[clip]->clip_type==CLIP_TYPE_DISK||mainw->files[clip]->clip_type==CLIP_TYPE_FILE)) {
      switch_clip(2,clip);
      if (prefs->omc_noisy) {
	char *msg=g_strdup_printf("%d",clip);
	lives_osc_notify_success(msg);
	g_free(msg);
      }
      return;
    }
  }
  lives_osc_notify_failure();
}


void lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip
  int clip,i;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip<1||mainw->cliplist==NULL) return lives_osc_notify_failure();

  if (mainw->scrap_file!=-1&&clip>=mainw->scrap_file) clip++;
  if (mainw->ascrap_file!=-1&&clip>=mainw->ascrap_file) clip++;

  if (clip>g_list_length(mainw->cliplist)) return lives_osc_notify_failure();

  i=GPOINTER_TO_INT(g_list_nth_data(mainw->cliplist,clip-1));
    
  if (i==mainw->current_file) return lives_osc_notify_failure();
  if (mainw->playing_file!=0) {
    switch_clip(1,i);
    if (prefs->omc_noisy) {
      char *msg=g_strdup_printf("%d",i);
      lives_osc_notify_success(msg);
      g_free(msg);
      return;
    }
  }
  lives_osc_notify_failure();
}




void lives_osc_cb_bgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch bg clip
  int clip,i;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure(); // etc

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip<1||mainw->cliplist==NULL) return lives_osc_notify_failure();

  if (mainw->scrap_file!=-1&&clip>=mainw->scrap_file) clip++;
  if (mainw->ascrap_file!=-1&&clip>=mainw->ascrap_file) clip++;

  if (clip>g_list_length(mainw->cliplist)) return lives_osc_notify_failure();

  if (mainw->num_tr_applied<1) return lives_osc_notify_failure();

  i=GPOINTER_TO_INT(g_list_nth_data(mainw->cliplist,clip-1));
    
  if (i==mainw->blend_file) return lives_osc_notify_failure();

  switch_clip(2,i);

  if (prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",i);
    lives_osc_notify_success(msg);
    g_free(msg);
    return;
  }

}


void lives_osc_cb_clip_resample(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fps;
  float fpsf;
  double fpsd;


  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"f",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&fps);
    fpsd=(double)(fps*1.);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"f",TRUE);
    lives_osc_parse_float_argument(vargs,&fpsf);
    fpsd=(double)fpsf;
  }

  if (fpsd<1.&&fpsd>FPS_MAX) return lives_osc_notify_failure();
  
  cfile->undo1_dbl=fpsd;

  on_resample_vid_ok(NULL,NULL);

  return lives_osc_notify_success(NULL);
}



void lives_osc_cb_clip_close(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio=0;
  int clipno=mainw->current_file;
  int current_file=clipno;

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (mainw->multitrack!=NULL||!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->multitrack!=NULL&&clipno==mainw->multitrack->render_file)) return lives_osc_notify_failure();

  if (clipno==current_file) current_file=-1;

  mainw->current_file=clipno;

  close_current_file(current_file);
  return lives_osc_notify_success(NULL);
}





void lives_osc_cb_fgclip_copy(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio=0;
  int clipno=mainw->current_file;
  int start,end,current_file=clipno;
  boolean ccpd;

  if (mainw->playing_file>-1||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  mainw->current_file=clipno;
  start=cfile->start;
  end=cfile->end;

  cfile->start=1;
  cfile->end=cfile->frames;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_copy_activate(NULL,NULL);

  mainw->ccpd_with_sound=ccpd;

  cfile->start=start;
  cfile->end=end;

  mainw->current_file=current_file;

  lives_osc_notify_success(NULL);

}



void lives_osc_cb_fgclipsel_copy(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  int current_file=clipno;
  boolean ccpd;

  if (mainw->playing_file>-1||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_copy_activate(NULL,NULL);

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

  lives_osc_notify_success(NULL);
}





void lives_osc_cb_fgclipsel_cut(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  int current_file=clipno;
  boolean ccpd;

  if (mainw->playing_file>-1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  mainw->osc_auto=TRUE;
  on_cut_activate(NULL,NULL);
  mainw->osc_auto=FALSE;

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}




void lives_osc_cb_fgclipsel_delete(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  int current_file=clipno;
  boolean ccpd;

  if (mainw->playing_file>-1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  mainw->osc_auto=TRUE;
  on_delete_activate(NULL,NULL);
  mainw->osc_auto=FALSE;

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}




void lives_osc_cb_clipbd_paste(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio=0;
  boolean ccpd;

  if (mainw->playing_file>-1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (clipboard==NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_paste_as_new_activate(NULL,NULL);

  mainw->ccpd_with_sound=ccpd;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }

}




void lives_osc_cb_clipbd_insertb(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int times=1;
  int clipno=mainw->current_file;
  int current_file=clipno;


  if (mainw->playing_file>-1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&times);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&times);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  if (times==0||times<-1) return lives_osc_notify_failure();

  mainw->current_file=clipno;

  mainw->insert_after=FALSE;

  if (clipboard->achans==0&&cfile->achans==0) noaudio=TRUE;

  mainw->fx1_bool=(times==-1); // fit to audio
  mainw->fx1_val=times;       // times to insert otherwise
  mainw->fx2_bool=!noaudio;  // with audio

  on_insert_activate(NULL,NULL);

  mainw->current_file=current_file;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}





void lives_osc_cb_clipbd_inserta(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int times=1;
  int clipno=mainw->current_file;
  int current_file=clipno;


  if (mainw->playing_file>-1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&times);
    lives_osc_parse_int_argument(vargs,&clipno);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
    lives_osc_parse_int_argument(vargs,&times);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return lives_osc_notify_failure();
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  if (times==0||times<-1) return lives_osc_notify_failure();

  mainw->current_file=clipno;

  mainw->insert_after=TRUE;

  if (clipboard->achans==0&&cfile->achans==0) noaudio=TRUE;

  mainw->fx1_bool=(times==-1); // fit to audio
  mainw->fx1_val=times;       // times to insert otherwise
  mainw->fx2_bool=!noaudio;  // with audio

  mainw->fx1_start=1;
  mainw->fx2_start=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);

  on_insert_activate(NULL,NULL);

  mainw->current_file=current_file;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}




void lives_osc_cb_fgclip_retrigger (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip and reset framenumber

  if (mainw->playing_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_cb_fgclip_select(context,arglen,vargs,when,ra);

  if (cfile->pb_fps>0.||(cfile->play_paused&&cfile->freeze_fps>0.)) cfile->frameno=cfile->last_frameno=1;
  else cfile->frameno=cfile->last_frameno=cfile->frames;

#ifdef RT_AUDIO
  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(cfile->frameno);
  }
#endif
}



void lives_osc_cb_bgclip_retrigger (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch bg clip and reset framenumber

  if (mainw->playing_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_cb_bgclip_select(context,arglen,vargs,when,ra);

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  if (mainw->files[mainw->blend_file]->pb_fps>0.||(mainw->files[mainw->blend_file]->play_paused&&mainw->files[mainw->blend_file]->freeze_fps>0.)) mainw->files[mainw->blend_file]->frameno=mainw->files[mainw->blend_file]->last_frameno=1;
  else mainw->files[mainw->blend_file]->frameno=mainw->files[mainw->blend_file]->last_frameno=mainw->files[mainw->blend_file]->frames;
  lives_osc_notify_success(NULL);
}




void lives_osc_cb_fgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  nextclip_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(1));

  if (mainw->playing_file==-1&&prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",mainw->current_file);
    lives_osc_notify_success(msg);
    g_free(msg);
  }

}


void lives_osc_cb_bgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  nextclip_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(2));

  if (mainw->playing_file==-1&&prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",mainw->blend_file);
    lives_osc_notify_success(msg);
    g_free(msg);
  }

}



void lives_osc_cb_fgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  
  prevclip_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(1));

  if (mainw->playing_file==-1&&prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",mainw->current_file);
    lives_osc_notify_success(msg);
    g_free(msg);
  }


}


void lives_osc_cb_bgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL) return lives_osc_notify_failure();

  prevclip_callback(NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(2));

  if (mainw->playing_file==-1&&prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",mainw->blend_file);
    lives_osc_notify_success(msg);
    g_free(msg);
  }
}







void lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  mainw->only_close=mainw->no_exit=FALSE;
  mainw->leave_recovery=FALSE;

  if (mainw->was_set) {
    on_save_set_activate(NULL,mainw->set_name);
  }
  else mainw->leave_files=FALSE;
  lives_exit();

}

void lives_osc_cb_getname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (status_socket==NULL) return;
  lives_status_send (PACKAGE_NAME);
}


void lives_osc_cb_getversion(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (status_socket==NULL) return;
  lives_status_send (VERSION);
}


void lives_osc_cb_getconst(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  const char *retval;
  char cname[OSC_STRING_SIZE];

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_string_argument(vargs,cname);
  retval=get_omc_const(cname);
  lives_status_send (retval);
}



void lives_osc_cb_open_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments (arglen,vargs,"si",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    g_snprintf (host,OSC_STRING_SIZE,"localhost");
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"si",TRUE);
    lives_osc_parse_string_argument(vargs,host);
  }
  lives_osc_parse_int_argument(vargs,&port);

  if (status_socket!=NULL) {
    LIVES_INFO("OMC status socket already opened");
    return lives_osc_notify_failure();
  }

  if (!(status_socket=OpenHTMSocket (host,port,TRUE))) LIVES_WARN ("Unable to open status socket !");
  else if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }

}

void lives_osc_cb_open_notify_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments (arglen,vargs,"si",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
    g_snprintf (host,OSC_STRING_SIZE,"localhost");
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"si",TRUE);
    lives_osc_parse_string_argument(vargs,host);
  }
  lives_osc_parse_int_argument(vargs,&port);

  if (notify_socket!=NULL) {
    LIVES_INFO("OMC notify socket already opened");
    return;
  }

  if (!(notify_socket=OpenHTMSocket (host,port,TRUE))) LIVES_WARN ("Unable to open notify socket !");
  prefs->omc_noisy=FALSE; // default for confirms is OFF

}

void lives_osc_cb_close_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  lives_osc_close_status_socket();
  lives_osc_notify_success(NULL);
}


void lives_osc_cb_notify_c(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int state;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&state);
  if (state>0) {
    prefs->omc_noisy=TRUE;
    lives_osc_notify_success(NULL);
  }
  else prefs->omc_noisy=FALSE;
}


void lives_osc_cb_notify_e(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int state;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&state);
  if (state>0) {
    prefs->omc_events=TRUE;
  }
  else prefs->omc_events=FALSE;
  lives_osc_notify_success(NULL);
}



void lives_osc_cb_clip_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (status_socket==NULL) return;
  lives_status_send ((tmp=g_strdup_printf ("%d",mainw->clips_available)));
  g_free(tmp);

}

void lives_osc_cb_clip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int frame;
  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&frame);

  if (frame<1||frame>cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  cfile->last_frameno=cfile->frameno=frame;

#ifdef RT_AUDIO
  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(frame);
  }
#endif
  lives_osc_notify_success(NULL);
}


void lives_osc_cb_clip_getframe(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  if (status_socket==NULL) return;
  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) lives_status_send ("0");
  else {
    lives_status_send ((tmp=g_strdup_printf ("%d",mainw->actual_frame)));
    g_free(tmp);
  }
}


void lives_osc_cb_clip_getfps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (mainw->current_file<0) lives_status_send ((tmp=g_strdup_printf ("%.3f",0.)));
  else if (mainw->preview||mainw->playing_file<1) lives_status_send ((tmp=g_strdup_printf ("%.3f",cfile->fps)));
  else lives_status_send ((tmp=g_strdup_printf ("%.3f",cfile->pb_fps)));
  g_free(tmp);
}


void lives_osc_cb_clip_get_ifps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  file *sfile;
  int clip=mainw->current_file;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];
  if (mainw->preview||mainw->playing_file<1) lives_status_send ((tmp=g_strdup_printf ("%.3f",sfile->fps)));
  else lives_status_send ((tmp=g_strdup_printf ("%.3f",sfile->pb_fps)));
  g_free(tmp);
}


void lives_osc_cb_get_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (mainw->current_file<0) lives_status_send ((tmp=g_strdup_printf ("%.4f",0.)));
  else if (mainw->preview||mainw->playing_file<1) lives_status_send ((tmp=g_strdup_printf ("%.4f",1.)));
  else lives_status_send ((tmp=g_strdup_printf ("%.4f",cfile->pb_fps/cfile->fps)));
  g_free(tmp);
}


void lives_osc_cb_bgget_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (mainw->current_file<0||mainw->blend_file<0||mainw->files[mainw->blend_file]==NULL) 
    lives_status_send ((tmp=g_strdup_printf ("%.4f",0.)));
  else if (mainw->preview||mainw->playing_file<1) lives_status_send ((tmp=g_strdup_printf ("%.4f",1.)));
  else lives_status_send ((tmp=g_strdup_printf ("%.4f",mainw->files[mainw->blend_file]->pb_fps/
						mainw->files[mainw->blend_file]->fps)));
  g_free(tmp);
}


void lives_osc_cb_bgclip_getframe(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1||mainw->blend_file<0||
      mainw->files[mainw->blend_file]==NULL) lives_status_send ("0");
  else {
    lives_status_send ((tmp=g_strdup_printf ("%d",mainw->files[mainw->blend_file]->frameno)));
    g_free(tmp);
  }
}


void lives_osc_cb_bgclip_getfps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1) return lives_osc_notify_failure();
  if (mainw->blend_file<0||mainw->files[mainw->blend_file]==NULL) lives_status_send ((tmp=g_strdup_printf ("%.3f",0.)));
  else if (mainw->preview||mainw->playing_file<1) lives_status_send ((tmp=g_strdup_printf ("%.3f",mainw->files[mainw->blend_file]->fps)));
  else lives_status_send ((tmp=g_strdup_printf ("%.3f",mainw->files[mainw->blend_file]->pb_fps)));
  g_free(tmp);
}


void lives_osc_cb_getmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (status_socket==NULL) return;

  if (mainw->multitrack!=NULL) lives_status_send(get_omc_const("LIVES_MODE_MULTITRACK"));
  else lives_status_send(get_omc_const("LIVES_MODE_CLIPEDIT"));
}




void lives_osc_cb_setmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *modes;
  int mode;
  int cliped=atoi(get_omc_const("LIVES_MODE_CLIPEDIT")); // 0
  int mt=atoi(get_omc_const("LIVES_MODE_MULTITRACK")); // 1

  if (mainw->preview||mainw->is_processing||mainw->playing_file>-1) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&mode);

  if (mode!=cliped && mode!=mt) return lives_osc_notify_failure();

  // these two will also send a status changed message
  if (mode==mt && mainw->multitrack==NULL) on_multitrack_activate(NULL,NULL);
  else if (mode==cliped && mainw->multitrack!=NULL) multitrack_delete(mainw->multitrack,FALSE);

  modes=g_strdup_printf("%d",mode);
  lives_osc_notify_success(modes);
  g_free(modes);
}


void lives_osc_cb_clearlay(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file>-1||mainw->preview||mainw->is_processing||mainw->multitrack==NULL) return lives_osc_notify_failure();
  wipe_layout(mainw->multitrack);
  return lives_osc_notify_success(NULL);
}


void lives_osc_cb_blockcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  int nblocks;
  int track;
  if (mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&track);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    track=mainw->multitrack->current_track;
  } else return lives_osc_notify_failure();

  nblocks=mt_get_block_count(mainw->multitrack,track);

  tmp=g_strdup_printf("%d",nblocks);
  lives_status_send(tmp);
  g_free(tmp);

}


void lives_osc_cb_blockinsert(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int clip;
  int opt;

  char *tmp;

  if (mainw->playing_file>-1||mainw->preview||mainw->is_processing||mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
    lives_osc_parse_int_argument(vargs,&opt);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else return lives_osc_notify_failure();

  if (clip<1||mainw->files[clip]==NULL||clip==mainw->current_file||clip==mainw->scrap_file||clip==mainw->ascrap_file) 
    return lives_osc_notify_failure();

  mainw->multitrack->clip_selected=clip-1;
  mt_clip_select(mainw->multitrack,TRUE);
  multitrack_insert(NULL,mainw->multitrack);

  tmp=g_strdup_printf("%d|%d",mainw->multitrack->current_track, 
		      mt_get_last_block_number(mainw->multitrack, mainw->multitrack->current_track));

  lives_osc_notify_success(tmp);
  g_free(tmp);

}

void lives_osc_cb_mtctimeset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  float time;
  char *msg;

  if (mainw->playing_file>-1||mainw->preview||mainw->is_processing||mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"f",TRUE)) { 
    lives_osc_parse_float_argument(vargs,&time);
  }
  else return lives_osc_notify_failure();

  if (time<0.) return lives_osc_notify_failure();

  time=q_dbl(time,mainw->multitrack->fps)/U_SEC;
  mt_tl_move(mainw->multitrack,time-lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline)));

  msg=g_strdup_printf("%.8f",lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline)));
  lives_osc_notify_success(msg);
  g_free(msg);
}

void lives_osc_cb_mtctimeget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;

  if (mainw->multitrack==NULL) return lives_osc_notify_failure();

  msg=g_strdup_printf("%.8f",lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline)));
  lives_status_send(msg);
  g_free(msg);
}

void lives_osc_cb_mtctrackset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  char *msg;

  if (mainw->playing_file>-1||mainw->preview||mainw->is_processing||mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    lives_osc_parse_int_argument(vargs,&track);
  }
  else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack,track)||mt_track_is_audio(mainw->multitrack,track)) {
    mainw->multitrack->current_track=track;
    track_select(mainw->multitrack);
    msg=g_strdup_printf("%d",track);
    lives_osc_notify_success(msg);
    g_free(msg);
  }
  else return lives_osc_notify_failure();
}


void lives_osc_cb_mtctrackget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;
  if (mainw->multitrack==NULL) return lives_osc_notify_failure();
  
  msg=g_strdup_printf("%d",mainw->multitrack->current_track);
  lives_status_send(msg);
  g_free(msg);

}



void lives_osc_cb_blockstget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  int nblock;
  double sttime;
  char *tmp;
  if (mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&track);
    lives_osc_parse_int_argument(vargs,&nblock);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    track=mainw->multitrack->current_track;
    lives_osc_parse_int_argument(vargs,&nblock);
  }
  else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack,track)||mt_track_is_audio(mainw->multitrack,track)) {
    sttime=mt_get_block_sttime(mainw->multitrack,track,nblock);
    if (sttime<0.) return lives_osc_notify_failure();
    tmp=g_strdup_printf("%.8f",sttime);
    lives_status_send(tmp);
    g_free(tmp);
    return;
  }
  return lives_osc_notify_failure(); ///< invalid track
}


void lives_osc_cb_blockenget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  int nblock;
  double entime;
  char *tmp;
  if (mainw->multitrack==NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&track);
    lives_osc_parse_int_argument(vargs,&nblock);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    track=mainw->multitrack->current_track;
    lives_osc_parse_int_argument(vargs,&nblock);
  }
  else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack,track)||mt_track_is_audio(mainw->multitrack,track)) {
    entime=mt_get_block_entime(mainw->multitrack,track,nblock);
    if (entime<0.) return lives_osc_notify_failure();
    tmp=g_strdup_printf("%.8f",entime);
    lives_status_send(tmp);
    g_free(tmp);
    return;
  }
  return lives_osc_notify_failure(); ///< invalid track
}



void lives_osc_cb_get_playtime(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) return lives_osc_notify_failure();

  lives_status_send ((tmp=g_strdup_printf ("%.8f",(double)mainw->currticks/U_SEC)));
  g_free(tmp);
}


void lives_osc_cb_bgclip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int frame;
  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&frame);

  if (frame<1||frame>mainw->files[mainw->blend_file]->frames||(mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  mainw->files[mainw->blend_file]->last_frameno=mainw->files[mainw->blend_file]->frameno=frame;

}


void lives_osc_cb_clip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (status_socket==NULL) return;
  lives_status_send ((tmp=g_strdup_printf ("%d",mainw->current_file<0?0:mainw->current_file)));
  g_free(tmp);

}


void lives_osc_cb_bgclip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  lives_status_send ((tmp=g_strdup_printf ("%d",mainw->blend_file<0?0:mainw->blend_file)));
  g_free(tmp);

}



void lives_osc_cb_clip_set_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  int frame;

  file *sfile;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    lives_osc_parse_int_argument(vargs,&frame);
  }
  else return lives_osc_notify_failure();

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  if (frame<1||(sfile->clip_type!=CLIP_TYPE_DISK&&sfile->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  if (frame>sfile->frames) frame=sfile->frames;

  if (clip==mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),frame);
  else sfile->start=frame;

  if (prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",frame);
    lives_osc_notify_success(msg);
    g_free(msg);
  }
}



void lives_osc_cb_clip_get_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  char *tmp;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  lives_status_send ((tmp=g_strdup_printf ("%d",sfile->start)));
  g_free(tmp);
}


void lives_osc_cb_clip_set_end(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  int frame;

  file *sfile;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) { 
    lives_osc_parse_int_argument(vargs,&frame);
  }
  else return lives_osc_notify_failure();

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  if (frame<1||(sfile->clip_type!=CLIP_TYPE_DISK&&sfile->clip_type!=CLIP_TYPE_FILE)) return lives_osc_notify_failure();

  if (frame>sfile->frames) frame=sfile->frames;

  if (clip==mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),frame);
  else sfile->end=frame;

  if (prefs->omc_noisy) {
    char *msg=g_strdup_printf("%d",frame);
    lives_osc_notify_success(msg);
    g_free(msg);
  }
}

void lives_osc_cb_clip_get_end(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  char *tmp;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  lives_status_send ((tmp=g_strdup_printf ("%d",sfile->end)));
  g_free(tmp);
}

void lives_osc_cb_clip_get_size(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  char *tmp;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  lives_status_send ((tmp=g_strdup_printf ("%d|%d",sfile->hsize,sfile->vsize)));
  g_free(tmp);
}

void lives_osc_cb_clip_get_name(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (mainw->multitrack!=NULL) return lives_osc_notify_failure();


  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  lives_status_send (sfile->name);
}



void lives_osc_cb_clip_set_name(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  char name[OSC_STRING_SIZE];

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"si",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"si",TRUE);
    lives_osc_parse_string_argument(vargs,name);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"s",TRUE)) { 
    if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
    lives_osc_parse_string_argument(vargs,name);
  }
  else return lives_osc_notify_failure();

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  mainw->current_file=clip;
  on_rename_set_name(NULL,(gpointer)name);

  if (clip==current_file) set_main_title(name,0);
  else mainw->current_file=current_file;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(name);
  }

}



void lives_osc_cb_clip_get_frames(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  char *tmp;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (mainw->multitrack!=NULL) return lives_osc_notify_failure();


  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();

  sfile=mainw->files[clip];

  lives_status_send ((tmp=g_strdup_printf ("%d",sfile->frames)));
  g_free(tmp);
}


void lives_osc_cb_clip_save_frame(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  int frame,width=-1,height=-1;
  char fname[OSC_STRING_SIZE];
  boolean retval;

  file *sfile;

  if (mainw->current_file<1) return lives_osc_notify_failure();
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"is",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"iis",FALSE)) {
      if (!lives_osc_check_arguments (arglen,vargs,"isii",FALSE)) {
	if (!lives_osc_check_arguments (arglen,vargs,"iisii",TRUE)) {
	  return lives_osc_notify_failure();
	}
	lives_osc_parse_int_argument(vargs,&frame);
	lives_osc_parse_int_argument(vargs,&clip);
	lives_osc_parse_string_argument(vargs,fname);
	lives_osc_parse_int_argument(vargs,&width);
	lives_osc_parse_int_argument(vargs,&height);
      }
      else {
	lives_osc_check_arguments (arglen,vargs,"isii",TRUE);
	if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
	lives_osc_parse_int_argument(vargs,&frame);
	lives_osc_parse_string_argument(vargs,fname);
	lives_osc_parse_int_argument(vargs,&width);
	lives_osc_parse_int_argument(vargs,&height);
      }
    }
    else {
      lives_osc_check_arguments (arglen,vargs,"iis",TRUE);
      lives_osc_parse_int_argument(vargs,&frame);
      lives_osc_parse_int_argument(vargs,&clip);
      lives_osc_parse_string_argument(vargs,fname);
    }
  }
  else {
    if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
    lives_osc_check_arguments (arglen,vargs,"is",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
    lives_osc_parse_string_argument(vargs,fname);
  }

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return lives_osc_notify_failure();
  sfile=mainw->files[clip];

  if (frame<1||frame>sfile->frames||(sfile->clip_type!=CLIP_TYPE_DISK&&sfile->clip_type!=CLIP_TYPE_FILE)) 
    return lives_osc_notify_failure();

  retval=save_frame_inner(clip,frame,fname,width,height,TRUE);

  if (retval) lives_osc_notify_success(NULL);
  else lives_osc_notify_failure();

}


void lives_osc_cb_clip_select_all(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return lives_osc_notify_failure();
  if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||!cfile->frames) return lives_osc_notify_failure();

  on_select_all_activate (NULL,NULL);

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}

void lives_osc_cb_clip_isvalid(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int clip;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES&&mainw->files[clip]!=NULL&&!(mainw->multitrack!=NULL&&clip==mainw->multitrack->render_file)&&clip!=mainw->scrap_file) 
    lives_status_send (get_omc_const("LIVES_TRUE"));
  else lives_status_send (get_omc_const("LIVES_FALSE"));

}

void lives_osc_cb_rte_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects - (only those assigned to keys for now)
  char *tmp;
  if (status_socket==NULL) return;

  lives_status_send ((tmp=g_strdup_printf ("%d",prefs->rte_keys_virtual)));
  g_free(tmp);

}

void lives_osc_cb_rteuser_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects
  char *tmp;
  if (status_socket==NULL) return;

  lives_status_send ((tmp=g_strdup_printf ("%d",FX_MAX)));
  g_free(tmp);

}


void lives_osc_cb_fssepwin_enable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->sep_win) {
    on_sepwin_pressed (NULL,NULL);
  }

  if (!mainw->fs) {
    on_full_screen_pressed (NULL,NULL);
  }
  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}


void lives_osc_cb_fssepwin_disable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->fs) {
    on_full_screen_pressed (NULL,NULL);
  }
  if (mainw->sep_win) {
    on_sepwin_pressed (NULL,NULL);
  }
  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}

void lives_osc_cb_op_fps_set(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fps;
  float fpsf;
  double fpsd;
  char *tmp;

  if (mainw->fixed_fpsd>0.) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments (arglen,vargs,"f",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&fps);
    fpsd=(double)(fps*1.);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"f",TRUE);
    lives_osc_parse_float_argument(vargs,&fpsf);
    fpsd=(double)fpsf;
  }
  if (fpsd>0.&&fpsd<=FPS_MAX) {
    mainw->fixed_fpsd=fpsd;
    d_print ((tmp=g_strdup_printf (_("Syncing to external framerate of %.8f frames per second.\n"),fpsd)));
    g_free(tmp);
  }
  else if (fpsd==0.) mainw->fixed_fpsd=-1.; ///< 0. to release
  else lives_osc_notify_failure();
  if (prefs->omc_noisy) {
    char *msg=g_strdup_printf("%.3f",fpsd);
    lives_osc_notify_success(msg);
    g_free(msg);
  }
}


void lives_osc_cb_freeze(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file<1) return lives_osc_notify_failure();

  if (!mainw->osc_block) {
    freeze_callback(NULL,NULL,0,(LiVESModifierType)0,NULL);
  }
  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}

void lives_osc_cb_op_nodrope(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  
  mainw->noframedrop=TRUE;

  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }
}


void lives_osc_cb_op_nodropd(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  
  mainw->noframedrop=FALSE;
  if (prefs->omc_noisy) {
    lives_osc_notify_success(NULL);
  }

}

void lives_osc_cb_fx_getname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *retval;
  int fidx;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&fidx);
  }
  else return lives_osc_notify_failure();

  retval=make_weed_hashname(fidx, FALSE, FALSE);

  lives_status_send(retval);

  g_free(retval);
}

void lives_osc_cb_clip_encodeas(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char fname[OSC_STRING_SIZE];

  if (mainw->playing_file>-1||mainw->current_file<1) return lives_osc_notify_failure();

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (cfile==NULL || cfile->opening) return lives_osc_notify_failure();

  mainw->osc_enc_width=mainw->osc_enc_height=0;
  mainw->osc_enc_fps=0.;

  if (!lives_osc_check_arguments (arglen,vargs,"siif",FALSE)) { 
    if (!lives_osc_check_arguments (arglen,vargs,"sii",FALSE)) { 
      if (!lives_osc_check_arguments (arglen,vargs,"sf",FALSE)) { 
	if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) 
	  return lives_osc_notify_failure();
	lives_osc_parse_string_argument(vargs,fname);
      }
      lives_osc_check_arguments (arglen,vargs,"sf",TRUE);
      lives_osc_parse_string_argument(vargs,fname);
      lives_osc_parse_float_argument(vargs,&mainw->osc_enc_fps);
    }
    else {
      lives_osc_check_arguments (arglen,vargs,"sii",TRUE);
      lives_osc_parse_string_argument(vargs,fname);
      lives_osc_parse_int_argument(vargs,&mainw->osc_enc_width);
      lives_osc_parse_int_argument(vargs,&mainw->osc_enc_height);
    }
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"siif",TRUE);
    lives_osc_parse_string_argument(vargs,fname);
    lives_osc_parse_int_argument(vargs,&mainw->osc_enc_width);
    lives_osc_parse_int_argument(vargs,&mainw->osc_enc_height);
    lives_osc_parse_float_argument(vargs,&mainw->osc_enc_fps);
  }

  if (cfile->frames==0) {
    // TODO
    on_export_audio_activate (NULL,NULL);
    lives_osc_notify_success(NULL);
    return;
  }

  mainw->osc_auto=TRUE;
  save_file(mainw->current_file,1,cfile->frames,fname);
  lives_osc_notify_success(NULL);
  mainw->osc_auto=FALSE;

}



void lives_osc_cb_rte_setmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&mode);
  if (effect_key<1||effect_key>=FX_KEYS_MAX_VIRTUAL||mode<1||mode>rte_getmodespk()) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode (effect_key,mode-1);

  lives_osc_notify_success(NULL);
}



void lives_osc_cb_rte_nextmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (effect_key<1||effect_key>=FX_KEYS_MAX_VIRTUAL) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode (effect_key,-1);

  lives_osc_notify_success(NULL);
}


void lives_osc_cb_rte_prevmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (effect_key<1||effect_key>=FX_KEYS_MAX_VIRTUAL) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode (effect_key,-2);

  lives_osc_notify_success(NULL);
}


///////////////////////////////////////////////////////////////


static boolean setfx (weed_plant_t *plant, weed_plant_t *tparam, int pnum, int nargs, const void *vargs, int skip) {
  // set value of "value" leaf for tparam, or "host_default" for a template
  // set it to vargs (length nargs)

  int valuei;
  float valuef;
  int error,i;
  weed_plant_t *ptmpl,*inst=NULL;
  int hint,cspace=-1;
  int x=0;
  int copyto=-1;
  int defargs;
  int maxi_r=255,maxi_g=255,maxi_b=255,maxi_a=255,mini_r=0,mini_g=0,mini_b=0,mini_a=0,mini,maxi;

  double maxd_r=1.,maxd_g=1.,maxd_b=1.,maxd_a=1.,mind_r=0.,mind_g=0.,mind_b=0.,mind_a=0.,mind,maxd;
  char values[OSC_STRING_SIZE];
  const char *pattern;

  if (nargs<=0) return FALSE; // must set at least one value

  if (WEED_PLANT_IS_FILTER_CLASS(plant)) {
    ptmpl=tparam;
  }
  else {
    ptmpl=weed_get_plantptr_value(tparam,"template",&error);
    inst=plant;
  }

  hint=weed_get_int_value(ptmpl,"hint",&error);
  if (hint==WEED_HINT_COLOR) cspace=weed_get_int_value(ptmpl,"colorspace",&error);

  if (!(weed_parameter_has_variable_elements_strict(inst,ptmpl))) {
    if (nargs>(defargs=weed_leaf_num_elements(ptmpl,"default"))) {
      if (!(hint==WEED_HINT_COLOR&&defargs==1&&((cspace==WEED_COLORSPACE_RGB&&(nargs%3==0))||(cspace==WEED_COLORSPACE_RGBA&&(nargs%4==0)))))
	// error: parameter does not have variable elements, and the user sent too many values
	return FALSE;
    }
  }

  pattern=(char *)vargs+skip; // skip comma,int,(int)

  switch (hint) {
  case WEED_HINT_INTEGER:
    {
      int *valuesi=(int *)g_malloc(nargs*sizint);

      while (pattern[x]!=0) {
	if (pattern[x]=='f') {
	  // we wanted an int but we got a float
	  //so we will round to the nearest value
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuesi[x]=myround(valuef);
	}
	else if (pattern[x]=='i') {
	  lives_osc_parse_int_argument(vargs,&valuesi[x]);
	}
	else {
	  g_free(valuesi);
	  return FALSE;
	}
	mini=weed_get_int_value(ptmpl,"min",&error);
	maxi=weed_get_int_value(ptmpl,"max",&error);
      
	if (valuesi[x]<mini) valuesi[x]=mini;
	if (valuesi[x]>maxi) valuesi[x]=maxi;
	x++;
      }

      if (inst!=NULL) {
	copyto=set_copy_to(inst,pnum,FALSE);
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_int_array(tparam,"value",nargs,valuesi);
	pthread_mutex_unlock(&mainw->data_mutex);

	set_copy_to(inst,pnum,TRUE);

	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
      }
      else {
	weed_set_int_array(tparam,"host_default",nargs,valuesi);
      }

      g_free(valuesi);

      break;
    }

  case WEED_HINT_SWITCH:
    {
      int group=0;
      int *valuesb=(int *)g_malloc(nargs*sizint);

      while (pattern[x]!=0) {
	if (pattern[x]=='i') {
	  lives_osc_parse_int_argument(vargs,&valuesb[x]);
	}
	else {
	  g_free(valuesb);
	  return FALSE;
	}
	valuesb[x]=!!valuesb[x];
	x++;
      }

      if (weed_plant_has_leaf(ptmpl,"group")) 
	group=weed_get_int_value(ptmpl,"group",&error);

      if (group!=0&&valuesb[0]==WEED_FALSE) goto grpinvalid;

      if (inst!=NULL) {
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_boolean_array(tparam,"value",nargs,valuesb);
	pthread_mutex_unlock(&mainw->data_mutex);

	copyto=set_copy_to(inst,pnum,TRUE);

	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}

	if (group!=0) {
	  // set all other values in group to WEED_FALSE
	  weed_plant_t *filter=weed_instance_get_filter(inst,FALSE),*xtparam;
	  int nparams=num_in_params(filter,FALSE,TRUE);

	  for (pnum=0;pnum<nparams;pnum++) {
	    xtparam=weed_inst_in_param(inst,pnum,FALSE,TRUE);

	    if (xtparam!=tparam) {
	      ptmpl=weed_get_plantptr_value(xtparam,"template",&error);
	      hint=weed_get_int_value(ptmpl,"hint",&error);
	      if (hint==WEED_HINT_SWITCH) {
		int xgroup=0;

		if (weed_plant_has_leaf(ptmpl,"group")) 
		  xgroup=weed_get_int_value(ptmpl,"group",&error);

		if (xgroup==group) {
		  pthread_mutex_lock(&mainw->data_mutex);
		  weed_set_boolean_value(xtparam,"value",WEED_FALSE);
		  pthread_mutex_unlock(&mainw->data_mutex);
		  
		  copyto=set_copy_to(inst,pnum,TRUE);
		  
		  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
		    // if we are recording, add this change to our event_list
		    rec_param_change(inst,pnum);
		    if (copyto!=-1) rec_param_change(inst,copyto);
		  }
		}
	      }
	    }
	  }
	}
      }
      else {
	weed_set_boolean_array(tparam,"host_default",nargs,valuesb);

	if (group!=0) {
	  // set all other values in group to WEED_FALSE
	  weed_plant_t *filter=plant,*xtparam;
	  int nparams=num_in_params(filter,FALSE,TRUE);

	  for (pnum=0;pnum<nparams;pnum++) {
	    xtparam=weed_filter_in_paramtmpl(inst,pnum,TRUE);

	    if (xtparam!=tparam) {
	      hint=weed_get_int_value(xtparam,"hint",&error);
	      if (hint==WEED_HINT_SWITCH) {
		int xgroup=0;

		if (weed_plant_has_leaf(ptmpl,"group")) 
		  xgroup=weed_get_int_value(ptmpl,"group",&error);

		if (xgroup==group) {
		  weed_set_boolean_value(tparam,"host_default",WEED_FALSE);
		}
	      }
	    }
	  }
	}
      }

    grpinvalid:

      g_free(valuesb);

      break;

    }
    
  case WEED_HINT_FLOAT:
    {
      double *valuesd=(double *)g_malloc(nargs*sizdbl);

      while (pattern[x]!=0) {
	if (pattern[x]=='i') {
	  // we wanted an float but we got an int
	  //so we will convert
	  lives_osc_parse_int_argument(vargs,&valuei);
	  valuesd[x]=(double)(valuei);
	}
	else if (pattern[x]=='f') {
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuesd[x]=(double)(valuef);
	}
	else {
	  g_free(valuesd);
	  return FALSE;
	}
	mind=weed_get_double_value(ptmpl,"min",&error);
	maxd=weed_get_double_value(ptmpl,"max",&error);
      
	if (valuesd[x]<mind) valuesd[x]=mind;
	if (valuesd[x]>maxd) valuesd[x]=maxd;
	x++;
      }


      if (inst!=NULL) {
	copyto=set_copy_to(inst,pnum,FALSE);

	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_double_array(tparam,"value",nargs,valuesd);
	pthread_mutex_unlock(&mainw->data_mutex);

	set_copy_to(inst,pnum,TRUE);

	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
      }
      else {
	weed_set_double_array(tparam,"host_default",nargs,valuesd);
      }

      g_free(valuesd);

      break;

    }


  case WEED_HINT_TEXT:
    {
      char **valuess=(char **)g_malloc(nargs*sizeof(char *));

      while (pattern[x]!=0) {
	if (pattern[x]=='i') {
	  // we wanted a string but we got an int
	  //so we will convert
	  lives_osc_parse_int_argument(vargs,&valuei);
	  valuess[x]=g_strdup_printf("%d",valuei);
	}
	else if (pattern[x]=='f') {
	  // we wanted a string but we got a float
	  //so we will convert
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuess[x]=g_strdup_printf("%f",valuef);
	}
	else if (pattern[x]=='s') {
	  lives_osc_parse_string_argument(vargs,values);
	  valuess[x]=g_strdup(values);
	}
	else {
	  for (i=0;i<x;i++) g_free(valuess[i]);
	  g_free(valuess);
	  return FALSE;
	}

	x++;
      }


      if (inst!=NULL) {
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_string_array(tparam,"value",nargs,valuess);
	pthread_mutex_unlock(&mainw->data_mutex);

	copyto=set_copy_to(inst,pnum,TRUE);
	
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
      }
      else {
	weed_set_string_array(tparam,"host_default",nargs,valuess);
      }

      for (i=0;i<x;i++) g_free(valuess[i]);
      g_free(valuess);

      break;

    }

    // COLOR is the most complicated, as we can have 3-values (RGB) or 4-values (RGBA), and we can have 
    // an int or a float type. Also min and max can have 1,n or N values.

  case WEED_HINT_COLOR:
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (nargs%3 != 0) return FALSE; //nargs must be a multiple of 3

      if (weed_leaf_seed_type(ptmpl,"default")==WEED_SEED_INT) {
	// RGB, int type
	int *valuesi=(int *)g_malloc(nargs*sizint);
	int nmins=weed_leaf_num_elements(ptmpl,"min");
	int nmaxs=weed_leaf_num_elements(ptmpl,"max");
	int *minis=NULL,*maxis=NULL;

	// get min and max values - 3 possibilities: 1 value, 3 values or N values

	if (nmins==1) {
	  mini_r=mini_g=mini_b=weed_get_int_value(ptmpl,"min",&error);
	}
	else {
	  minis=weed_get_int_array(ptmpl,"min",&error);
	  if (nmins==3) {
	    mini_r=minis[0];
	    mini_g=minis[1];
	    mini_b=minis[2];
	    weed_free(minis);
	  }
	}

	if (nmaxs==1) {
	  maxi_r=maxi_g=maxi_b=weed_get_int_value(ptmpl,"max",&error);
	}
	else {
	  maxis=weed_get_int_array(ptmpl,"max",&error);
	  if (nmaxs==3) {
	    maxi_r=maxis[0];
	    maxi_g=maxis[1];
	    maxi_b=maxis[2];
	    weed_free(maxis);
	  }
	}

	// read vals from OSC message
	while (pattern[x]!=0) {

	  // get next 3 values
	  for (i=0;i<3;i++) {
	    if (pattern[x+i]=='f') {
	      // we wanted int but we got floats
	      lives_osc_parse_float_argument(vargs,&valuef);
	      valuesi[x+i]=myround(valuef);
	    }
	    else { 
	      lives_osc_parse_int_argument(vargs,&valuesi[x+i]);
	    } 
	  }

	  if (nmins<=3) {
	    if (valuesi[x]<mini_r) valuesi[x]=mini_r;
	    if (valuesi[x+1]<mini_g) valuesi[x+1]=mini_g;
	    if (valuesi[x+2]<mini_b) valuesi[x+2]=mini_b;
	  }
	  else {
	    if (valuesi[x]<minis[x])   valuesi[x]=minis[x];
	    if (valuesi[x+1]<minis[x+1]) valuesi[x+1]=minis[x+1];
	    if (valuesi[x+2]<minis[x+2]) valuesi[x+2]=minis[x+2];
	  }

	  if (nmaxs<=3) {
	    if (valuesi[x]>maxi_r) valuesi[x]=maxi_r;
	    if (valuesi[x+1]>maxi_g) valuesi[x+1]=maxi_g;
	    if (valuesi[x+2]>maxi_b) valuesi[x+2]=maxi_b;
	  }
	  else {
	    if (valuesi[x]>maxis[x+i])   valuesi[x]=maxis[x+i];
	    if (valuesi[x+1]>maxis[x+1]) valuesi[x+1]=maxis[x+1];
	    if (valuesi[x+2]>maxis[x+2]) valuesi[x+2]=maxis[x+2];
	  }
	  x+=3;
	}

	if (inst!=NULL) {
	  copyto=set_copy_to(inst,pnum,FALSE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	
	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_int_array(tparam,"value",nargs,valuesi);
	  pthread_mutex_unlock(&mainw->data_mutex);
	
	  set_copy_to(inst,pnum,TRUE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	}
	else {
	  weed_set_int_array(tparam,"host_default",nargs,valuesi);
	}

	g_free(valuesi);

	if (nmins>3) g_free(minis);
	if (nmaxs>3) g_free(maxis);
      }
      else {
	// RGB, float type
	double *valuesd=(double *)g_malloc(nargs*sizeof(double));
	int nmins=weed_leaf_num_elements(ptmpl,"min");
	int nmaxs=weed_leaf_num_elements(ptmpl,"max");
	double *minds=NULL,*maxds=NULL;
	// get min and max values - 3 possibilities: 1 value, 3 values or N values

	if (nmins==1) {
	  mind_r=mind_g=mind_b=weed_get_double_value(ptmpl,"min",&error);
	}
	else {
	  minds=weed_get_double_array(ptmpl,"min",&error);
	  if (nmins==3) {
	    mind_r=minds[0];
	    mind_g=minds[1];
	    mind_b=minds[2];
	    weed_free(minds);
	  }
	}

	if (nmaxs==1) {
	  maxd_r=maxd_g=maxd_b=weed_get_double_value(ptmpl,"max",&error);
	}
	else {
	  maxds=weed_get_double_array(ptmpl,"max",&error);
	  if (nmaxs==3) {
	    maxd_r=maxds[0];
	    maxd_g=maxds[1];
	    maxd_b=maxds[2];
	    weed_free(maxds);
	  }
	}

	// read vals from OSC message
	while (pattern[x]!=0) {

	  // get next 3 values
	  for (i=0;i<3;i++) {
	    if (pattern[x+i]=='i') {
	      // we wanted float but we got floats
	      lives_osc_parse_int_argument(vargs,&valuei);
	      valuesd[x+i]=(double)valuei;
	    }
	    else { 
	      lives_osc_parse_float_argument(vargs,&valuef);
	      valuesd[x+i]=(double)valuef;
	    } 
	  }

	  if (nmins<=3) {
	    if (valuesd[x]<mind_r) valuesd[x]=mind_r;
	    if (valuesd[x+1]<mind_g) valuesd[x+1]=mind_g;
	    if (valuesd[x+2]<mind_b) valuesd[x+2]=mind_b;
	  }
	  else {
	    if (valuesd[x]<minds[x])   valuesd[x]=minds[x];
	    if (valuesd[x+1]<minds[x+1]) valuesd[x+1]=minds[x+1];
	    if (valuesd[x+2]<minds[x+2]) valuesd[x+2]=minds[x+2];
	  }

	  if (nmaxs<=3) {
	    if (valuesd[x]>maxd_r) valuesd[x]=maxd_r;
	    if (valuesd[x+1]>maxd_g) valuesd[x+1]=maxd_g;
	    if (valuesd[x+2]>maxd_b) valuesd[x+2]=maxd_b;
	  }
	  else {
	    if (valuesd[x]>maxds[x])   valuesd[x]=maxds[x];
	    if (valuesd[x+1]>maxds[x+1]) valuesd[x+1]=maxds[x+1];
	    if (valuesd[x+2]>maxds[x+2]) valuesd[x+2]=maxds[x+2];
	  }
	  x+=3;
	}

	if (inst!=NULL) {
	  copyto=set_copy_to(inst,pnum,FALSE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(tparam,"value",nargs,valuesd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	
	  set_copy_to(inst,pnum,TRUE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	}
	else {
	  weed_set_double_array(tparam,"host_default",nargs,valuesd);
	}

	g_free(valuesd);

	if (nmins>3) g_free(minds);
	if (nmaxs>3) g_free(maxds);

      }

      break;

      // RGBA colorspace


    case WEED_COLORSPACE_RGBA:
      if (nargs%4 != 0) return FALSE; //nargs must be a multiple of 4

      if (weed_leaf_seed_type(ptmpl,"default")==WEED_SEED_INT) {
	// RGBA, int type
	int *valuesi=(int *)g_malloc(nargs*sizint);
	int nmins=weed_leaf_num_elements(ptmpl,"min");
	int nmaxs=weed_leaf_num_elements(ptmpl,"max");
	int *minis=NULL,*maxis=NULL;

	// get min and max values - 3 possibilities: 1 value, 4 values or N values

	if (nmins==1) {
	  mini_r=mini_g=mini_b=mini_a=weed_get_int_value(ptmpl,"min",&error);
	}
	else {
	  minis=weed_get_int_array(ptmpl,"min",&error);
	  if (nmins==4) {
	    mini_r=minis[0];
	    mini_g=minis[1];
	    mini_b=minis[2];
	    mini_a=minis[3];
	    weed_free(minis);
	  }
	}

	if (nmaxs==1) {
	  maxi_r=maxi_g=maxi_b=maxi_a=weed_get_int_value(ptmpl,"max",&error);
	}
	else {
	  maxis=weed_get_int_array(ptmpl,"max",&error);
	  if (nmaxs==4) {
	    maxi_r=maxis[0];
	    maxi_g=maxis[1];
	    maxi_b=maxis[2];
	    maxi_a=maxis[3];
	    weed_free(maxis);
	  }
	}

	// read vals from OSC message
	while (pattern[x]!=0) {

	  // get next 4 values
	  for (i=0;i<4;i++) {
	    if (pattern[x]=='f') {
	      // we wanted int but we got floats
	      lives_osc_parse_float_argument(vargs,&valuef);
	      valuesi[x+i]=myround(valuef);
	    }
	    else { 
	      lives_osc_parse_int_argument(vargs,&valuesi[x+i]);
	    } 
	  }

	  if (nmins<=3) {
	    if (valuesi[x]<mini_r) valuesi[x]=mini_r;
	    if (valuesi[x+1]<mini_g) valuesi[x+1]=mini_g;
	    if (valuesi[x+2]<mini_b) valuesi[x+2]=mini_b;
	    if (valuesi[x+3]<mini_a) valuesi[x+3]=mini_a;
	  }
	  else {
	    if (valuesi[x]<minis[x])   valuesi[x]=minis[x];
	    if (valuesi[x+1]<minis[x+1]) valuesi[x+1]=minis[x+1];
	    if (valuesi[x+2]<minis[x+2]) valuesi[x+2]=minis[x+2];
	    if (valuesi[x+3]<minis[x+3]) valuesi[x+3]=minis[x+3];
	  }

	  if (nmaxs<=4) {
	    if (valuesi[x]>maxi_r) valuesi[x]=maxi_r;
	    if (valuesi[x+1]>maxi_g) valuesi[x+1]=maxi_g;
	    if (valuesi[x+2]>maxi_b) valuesi[x+2]=maxi_b;
	    if (valuesi[x+3]>maxi_a) valuesi[x+3]=maxi_a;
	  }
	  else {
	    if (valuesi[x]>maxis[x])   valuesi[x]=maxis[x];
	    if (valuesi[x+1]>maxis[x+1]) valuesi[x+1]=maxis[x+1];
	    if (valuesi[x+2]>maxis[x+2]) valuesi[x+2]=maxis[x+2];
	    if (valuesi[x+3]>maxis[x+3]) valuesi[x+3]=maxis[x+3];
	  }
	  x+=4;
	}

	if (inst!=NULL) {
	  copyto=set_copy_to(inst,pnum,FALSE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	
	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_int_array(tparam,"value",nargs,valuesi);
	  pthread_mutex_unlock(&mainw->data_mutex);
	  
	  set_copy_to(inst,pnum,TRUE);
	  
	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	}
	else {
	  weed_set_int_array(tparam,"host_default",nargs,valuesi);
	}

	g_free(valuesi);
	
	if (nmins>4) g_free(minis);
	if (nmaxs>4) g_free(maxis);
	  
      }
      else {
	// RGBA, float type
	double *valuesd=(double *)g_malloc(nargs*sizdbl);
	int nmins=weed_leaf_num_elements(ptmpl,"min");
	int nmaxs=weed_leaf_num_elements(ptmpl,"max");
	double *minds=NULL,*maxds=NULL;

	// get min and max values - 3 possibilities: 1 value, 3 values or N values

	if (nmins==1) {
	  mind_r=mind_g=mind_b=mind_a=weed_get_double_value(ptmpl,"min",&error);
	}
	else {
	  minds=weed_get_double_array(ptmpl,"min",&error);
	  if (nmins==4) {
	    mind_r=minds[0];
	    mind_g=minds[1];
	    mind_b=minds[2];
	    mind_a=minds[3];
	    weed_free(minds);
	  }
	}

	if (nmaxs==1) {
	  maxd_r=maxd_g=maxd_b=mind_a=weed_get_double_value(ptmpl,"max",&error);
	}
	else {
	  maxds=weed_get_double_array(ptmpl,"max",&error);
	  if (nmaxs==4) {
	    maxd_r=maxds[0];
	    maxd_g=maxds[1];
	    maxd_b=maxds[2];
	    maxd_a=maxds[3];
	    weed_free(maxds);
	  }
	}

	// read vals from OSC message
	while (pattern[x]!=0) {

	  // get next 4 values
	  for (i=0;i<4;i++) {
	    if (pattern[x]=='i') {
	      // we wanted float but we got floats
	      lives_osc_parse_int_argument(vargs,&valuei);
	      valuesd[x+i]=(double)valuei;
	    }
	    else { 
	      lives_osc_parse_float_argument(vargs,&valuef);
	      valuesd[x+i]=(double)valuef;
	    } 
	  }

	  if (nmins<=4) {
	    if (valuesd[x]<mind_r) valuesd[x]=mind_r;
	    if (valuesd[x+1]<mind_g) valuesd[x+1]=mind_g;
	    if (valuesd[x+2]<mind_b) valuesd[x+2]=mind_b;
	    if (valuesd[x+3]<mind_a) valuesd[x+3]=mind_a;
	  }
	  else {
	    if (valuesd[x]<minds[x])   valuesd[x]=minds[x];
	    if (valuesd[x+1]<minds[x+1]) valuesd[x+1]=minds[x+1];
	    if (valuesd[x+2]<minds[x+2]) valuesd[x+2]=minds[x+2];
	    if (valuesd[x+3]<minds[x+3]) valuesd[x+3]=minds[x+3];
	  }

	  if (nmaxs<=4) {
	    if (valuesd[x]>maxd_r) valuesd[x]=maxd_r;
	    if (valuesd[x+1]>maxd_g) valuesd[x+1]=maxd_g;
	    if (valuesd[x+2]>maxd_b) valuesd[x+2]=maxd_b;
	    if (valuesd[x+3]>maxd_a) valuesd[x+3]=maxd_a;
	  }
	  else {
	    if (valuesd[x]>maxds[x])   valuesd[x]=maxds[x];
	    if (valuesd[x+1]>maxds[x+1]) valuesd[x+1]=maxds[x+1];
	    if (valuesd[x+2]>maxds[x+2]) valuesd[x+2]=maxds[x+2];
	    if (valuesd[x+3]>maxds[x+3]) valuesd[x+3]=maxds[x+3];
	  }
	  x+=4;
	}

	if (inst!=NULL) {
	  copyto=set_copy_to(inst,pnum,FALSE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	    
	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(tparam,"value",nargs,valuesd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	    
	  set_copy_to(inst,pnum,TRUE);

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }
	}
	else {
	  weed_set_double_array(tparam,"host_default",nargs,valuesd);
	}

	g_free(valuesd);

	if (nmins>4) g_free(minds);
	if (nmaxs>4) g_free(maxds);

      }

      break;

    default:
      // invalid colorspace
      return FALSE;
      
    }
    
  default:
    // invalid param hint
    return FALSE;
  }

  return TRUE;
}




void lives_osc_cb_rte_getparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int hint,error;
  int nparams;
  int effect_key;
  int mode;
  int pnum;

  const char *retval;

  // TODO - handle compound fx


  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }
  
  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);
  
  hint=weed_get_int_value(ptmpl,"hint",&error);

  switch (hint) {
  case WEED_HINT_INTEGER: retval=get_omc_const("LIVES_PARAM_TYPE_INT"); break;
  case WEED_HINT_FLOAT: retval=get_omc_const("LIVES_PARAM_TYPE_FLOAT"); break;
  case WEED_HINT_TEXT: retval=get_omc_const("LIVES_PARAM_TYPE_STRING"); break;
  case WEED_HINT_SWITCH: retval=get_omc_const("LIVES_PARAM_TYPE_BOOL"); break;
  case WEED_HINT_COLOR: retval=get_omc_const("LIVES_PARAM_TYPE_COLOR"); break;
  default: return lives_osc_notify_failure();
  }

  lives_status_send (retval);
}


void lives_osc_cb_rte_getoparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;
  int hint,error;
  int nparams;
  int effect_key;
  int mode;
  int pnum;

  const char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }
  
  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
  
  ptmpl=out_ptmpls[pnum];
  hint=weed_get_int_value(ptmpl,"hint",&error);
  weed_free(out_ptmpls);

  switch (hint) {
  case WEED_HINT_INTEGER: retval=get_omc_const("LIVES_PARAM_TYPE_INT"); break;
  case WEED_HINT_FLOAT: retval=get_omc_const("LIVES_PARAM_TYPE_FLOAT"); break;
  case WEED_HINT_TEXT: retval=get_omc_const("LIVES_PARAM_TYPE_STRING"); break;
  case WEED_HINT_SWITCH: retval=get_omc_const("LIVES_PARAM_TYPE_BOOL"); break;
  case WEED_HINT_COLOR: retval=get_omc_const("LIVES_PARAM_TYPE_COLOR"); break;
  default: return lives_osc_notify_failure();
  }

  lives_status_send (retval);
}



void lives_osc_cb_rte_getpparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  // playback plugin params
  weed_plant_t *ptmpl,*param;
  int hint,error;
  int pnum;

  const char *retval;

  if (mainw->vpp==NULL||mainw->vpp->num_play_params==0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  hint=weed_get_int_value(ptmpl,"hint",&error);

  switch (hint) {
  case WEED_HINT_INTEGER: retval=get_omc_const("LIVES_PARAM_TYPE_INT"); break;
  case WEED_HINT_FLOAT: retval=get_omc_const("LIVES_PARAM_TYPE_FLOAT"); break;
  case WEED_HINT_TEXT: retval=get_omc_const("LIVES_PARAM_TYPE_STRING"); break;
  case WEED_HINT_SWITCH: retval=get_omc_const("LIVES_PARAM_TYPE_BOOL"); break;
  case WEED_HINT_COLOR: retval=get_omc_const("LIVES_PARAM_TYPE_COLOR"); break;
  default: return lives_osc_notify_failure();
  }

  lives_status_send (retval);
}



void lives_osc_cb_rte_getnparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint,error;
  int effect_key;
  int pnum,i;

  const char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return lives_osc_notify_failure();
  i=get_nth_simple_param(filter,pnum);
  if (i==-1) return lives_osc_notify_failure();
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  ptmpl=in_ptmpls[i];

  hint=weed_get_int_value(ptmpl,"hint",&error);
  weed_free(in_ptmpls);

  switch (hint) {
  case WEED_HINT_INTEGER: retval=get_omc_const("LIVES_PARAM_TYPE_INT"); break;
  case WEED_HINT_FLOAT: retval=get_omc_const("LIVES_PARAM_TYPE_FLOAT"); break;
  default: return lives_osc_notify_failure();
  }

  lives_status_send (retval);
}



void lives_osc_cb_rte_getparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				     NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int hint,error;
  int nparams;
  int effect_key;
  int mode;
  int pnum,cspace;
  int stype;

  const char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint!=WEED_HINT_COLOR) {
    return lives_osc_notify_failure();
  }
  cspace=weed_get_int_value(ptmpl,"colorspace",&error);

  stype=weed_leaf_seed_type(ptmpl,"default");

  if (cspace==WEED_COLORSPACE_RGB) {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  }
  else {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  lives_status_send (retval);

}


void lives_osc_cb_rte_getparamgrp(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				     NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int hint,error;
  int nparams;
  int effect_key;
  int mode;
  int pnum,grp;

  const char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint!=WEED_HINT_SWITCH) {
    return lives_osc_notify_failure();
  }
  grp=weed_get_int_value(ptmpl,"group",&error);

  retval=g_strdup_printf("%d",grp);

  lives_status_send (retval);

}


void lives_osc_cb_rte_getoparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				     NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;
  int hint,error;
  int nparams;
  int effect_key;
  int mode;
  int pnum,cspace;
  int stype;

  const char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();

  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
  ptmpl=out_ptmpls[pnum];

  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint!=WEED_HINT_COLOR) {
    weed_free(out_ptmpls);
    return lives_osc_notify_failure();
  }
  cspace=weed_get_int_value(ptmpl,"colorspace",&error);

  stype=weed_leaf_seed_type(ptmpl,"default");

  if (cspace==WEED_COLORSPACE_RGB) {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  }
  else {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  lives_status_send (retval);
  weed_free(out_ptmpls);

}




void lives_osc_cb_rte_getpparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				      NetworkReturnAddressPtr ra) {
  // playback plugin params
  weed_plant_t *ptmpl,*param;
  int hint,error;
  int pnum,cspace;
  int stype;

  const char *retval;

  if (mainw->vpp==NULL||mainw->vpp->num_play_params==0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint!=WEED_HINT_COLOR) {
    return lives_osc_notify_failure();
  }
  cspace=weed_get_int_value(ptmpl,"colorspace",&error);

  stype=weed_leaf_seed_type(ptmpl,"default");

  if (cspace==WEED_COLORSPACE_RGB) {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  }
  else {
    if (stype==WEED_SEED_INT) retval=get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval=get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  lives_status_send (retval);

}



void lives_osc_cb_rte_getparamflags(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int error;
  int nparams;
  int effect_key;
  int mode;
  int pnum,flags=0;

  char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (weed_plant_has_leaf(ptmpl,"flags"))
    flags=weed_get_int_value(ptmpl,"flags",&error);

  retval=g_strdup_printf("%d",flags);
  lives_status_send (retval);
  g_free(retval);
}



void lives_osc_cb_rte_getpparamflags(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				     NetworkReturnAddressPtr ra) {
  weed_plant_t *ptmpl,*param;
  int error;
  int pnum,flags=0;

  char *retval;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"flags"))
    flags=weed_get_int_value(ptmpl,"flags",&error);

  retval=g_strdup_printf("%d",flags);
  lives_status_send (retval);
  g_free(retval);
}


void lives_osc_cb_rte_getparamname(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int error;
  int nparams;
  int effect_key;
  int mode;
  int pnum;

  char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  retval=weed_get_string_value(ptmpl,"name",&error);

  lives_status_send (retval);

  weed_free(retval);

}


void lives_osc_cb_rte_getoparamname(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;
  int error;
  int nparams;
  int effect_key;
  int mode;
  int pnum;

  char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();

  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
  ptmpl=out_ptmpls[pnum];

  retval=weed_get_string_value(ptmpl,"name",&error);

  lives_status_send (retval);

  weed_free(retval);
  weed_free(out_ptmpls);

}



void lives_osc_cb_rte_getpparamname(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  weed_plant_t *ptmpl,*param;
  int error;
  int pnum;

  char *retval;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  retval=weed_get_string_value(ptmpl,"name",&error);

  lives_status_send (retval);

  weed_free(retval);

}



void lives_osc_cb_rte_getnparamname(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int error;
  int effect_key;
  int pnum,i;

  char *retval;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return lives_osc_notify_failure();

  i=get_nth_simple_param(filter,pnum);
  if (i==-1) return lives_osc_notify_failure();

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  ptmpl=in_ptmpls[i];

  retval=weed_get_string_value(ptmpl,"name",&error);

  lives_status_send (retval);

  weed_free(in_ptmpls);
  weed_free(retval);
}




void lives_osc_cb_rte_setparam(void *context, int arglen, const void *vargs, OSCTimeTag when, 
			       NetworkReturnAddressPtr ra) {

  weed_plant_t *inst,*filter;
  weed_plant_t *tparam;
  int nparams;

  int effect_key;
  int pnum,nargs;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  nargs=lives_osc_get_num_arguments(vargs);
  if (nargs<3) return lives_osc_notify_failure();

  osc_header_len=pad4(nargs+1); // add comma

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));

  if (inst==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  tparam=weed_inst_in_param(inst,pnum,FALSE,TRUE);

  if (!mainw->osc_block) {
    if (!setfx(inst,tparam,pnum,nargs-2,vargs,3)) {
      return lives_osc_notify_failure();
    }
  }
  else {
    return lives_osc_notify_failure();
  }

  if (fx_dialog[1]!=NULL) {
    lives_rfx_t *rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
    if (!rfx->is_template) {
      int keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
      int modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
      if (keyw==effect_key&&modew==rte_key_getmode(effect_key))
	mainw->vrfx_update=rfx;
    }
  }

  if (mainw->ce_thumbs) ce_thumbs_register_rfx_change(effect_key,rte_key_getmode(effect_key));

  lives_osc_notify_success(NULL);

}


void lives_osc_cb_rte_setparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  weed_plant_t *filter;
  weed_plant_t *tptmpl;
  int nparams;

  int effect_key;
  int mode;
  int pnum,nargs,skip;

  nargs=lives_osc_get_num_arguments(vargs);

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    if (nargs<3) return lives_osc_notify_failure();
    osc_header_len=pad4(nargs+1); // add comma
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
    skip=3;
  }
  else {
    if (nargs<4) return lives_osc_notify_failure();
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    osc_header_len=pad4(nargs+1); // add comma
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&pnum);
    skip=4;
  }


  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);

  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  tptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (!setfx(filter,tptmpl,pnum,nargs-2,vargs,skip)) {
    return lives_osc_notify_failure();
  }

  lives_osc_notify_success(NULL);

}



void lives_osc_cb_rte_setpparam(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				NetworkReturnAddressPtr ra) {
  // set playback plugin param
  weed_plant_t *param;
  int pnum,nargs;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  nargs=lives_osc_get_num_arguments(vargs);
  if (nargs<2) return lives_osc_notify_failure();

  osc_header_len=pad4(nargs+1); // add comma

  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  if (!mainw->osc_block) {
    if (!setfx(NULL,param,pnum,nargs-1,vargs,2)) return lives_osc_notify_failure();
  }
  else return lives_osc_notify_failure();
  
  lives_osc_notify_success(NULL);
}



void lives_osc_cb_rte_setnparam(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i,nargs;
  int error;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *inst,*param;
  weed_plant_t **in_params;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  nargs=lives_osc_get_num_arguments(vargs);
  if (nargs<3) return lives_osc_notify_failure();

  osc_header_len=pad4(nargs+1); // add comma

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));
  if (inst==NULL) return lives_osc_notify_failure();

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  i=get_nth_simple_param(inst,pnum);

  param=in_params[i];

  if (i!=-1 && !mainw->osc_block) {
    if (!setfx(inst,param,pnum,nargs-2,vargs,3)) return lives_osc_notify_failure();
  }
  else lives_osc_notify_failure();

  lives_osc_notify_success(NULL);

}




void lives_osc_cb_rte_setnparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  weed_plant_t *filter;
  weed_plant_t *tptmpl;
  int nparams;

  int effect_key;
  int mode;
  int pnum,nargs,skip;

  nargs=lives_osc_get_num_arguments(vargs);

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    if (nargs<3) return lives_osc_notify_failure();
    osc_header_len=pad4(nargs+1); // add comma
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
    skip=3;
  }
  else {
    if (nargs<4) return lives_osc_notify_failure();
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    osc_header_len=pad4(nargs+1); // add comma
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&pnum);
    skip=4;
  }


  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);

  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  tptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (!setfx(filter,tptmpl,pnum,nargs-2,vargs,skip)) {
    return lives_osc_notify_failure();
  }

  lives_osc_notify_success(NULL);

}



void lives_osc_cb_rte_paramcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key,mode;
    
  int count=0;
  weed_plant_t *filter;
  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();


  count=num_in_params(filter,FALSE,TRUE);

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}


void lives_osc_cb_rte_oparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key,mode;
  int count=0;
  weed_plant_t *filter;
  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (weed_plant_has_leaf(filter,"out_parameter_templates")) {
    count=weed_leaf_num_elements(filter,"out_parameter_templates");
  }

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}



void lives_osc_cb_rte_getinpal(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key,mode,cnum,count,error;
  weed_plant_t **ctmpls;
  weed_plant_t *filter,*inst,*ctmpl,*chan=NULL;
  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&cnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&cnum);
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst=rte_keymode_get_instance(effect_key,mode);

  if (inst!=NULL) {
    chan=get_enabled_channel(inst,cnum,TRUE);
    ctmpl=weed_get_plantptr_value(chan,"template",&error);
  }
  else {
    filter=rte_keymode_get_filter(effect_key,mode);
    if (filter==NULL) return lives_osc_notify_failure();

    if (!weed_plant_has_leaf(filter,"in_channel_templates")) return lives_osc_notify_failure();
    count=weed_leaf_num_elements(filter,"in_channel_templates");
    if (cnum>=count) return lives_osc_notify_failure();
    ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
    ctmpl=ctmpls[cnum];
    weed_free(ctmpls);
  }

  if (weed_plant_has_leaf(ctmpl,"is_audio")) {
      msg=g_strdup_printf("%d",WEED_PALETTE_END);
      lives_status_send(msg);
      g_free(msg);
      return;
  }

  if (inst!=NULL) {
    msg=g_strdup_printf("%d",weed_get_int_value(chan,"current_palette",&error));
    lives_status_send(msg);
    g_free(msg);
    return;
  }

  msg=lives_osc_format_result(ctmpl,"palette_list",0,-1);
  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getoutpal(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key,mode,cnum,count,error;
  weed_plant_t **ctmpls;
  weed_plant_t *filter,*inst,*ctmpl,*chan=NULL;
  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&cnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&cnum);
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst=rte_keymode_get_instance(effect_key,mode);

  if (inst!=NULL) {
    chan=get_enabled_channel(inst,cnum,FALSE);
    ctmpl=weed_get_plantptr_value(chan,"template",&error);
  }
  else {
    filter=rte_keymode_get_filter(effect_key,mode);
    if (filter==NULL) return lives_osc_notify_failure();

    if (!weed_plant_has_leaf(filter,"out_channel_templates")) return lives_osc_notify_failure();
    count=weed_leaf_num_elements(filter,"out_channel_templates");
    if (cnum>=count) return lives_osc_notify_failure();
    ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
    ctmpl=ctmpls[cnum];
    weed_free(ctmpls);
  }

  if (weed_plant_has_leaf(ctmpl,"is_audio")) {
      msg=g_strdup_printf("%d",WEED_PALETTE_END);
      lives_status_send(msg);
      g_free(msg);
      return;
  }

  if (inst!=NULL) {
    msg=g_strdup_printf("%d",weed_get_int_value(chan,"current_palette",&error));
    lives_status_send(msg);
    g_free(msg);
    return;
  }

  msg=lives_osc_format_result(ctmpl,"palette_list",0,-1);
  lives_status_send(msg);
  g_free(msg);

}



void lives_osc_cb_rte_pparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when,
				  NetworkReturnAddressPtr ra) {
  // return num playback plugin params
  int count=0;
  char *msg;

  if (mainw->vpp==NULL) {
    lives_status_send("0");
    return;
  }

  count=mainw->vpp->num_play_params;

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}



void lives_osc_cb_rte_nparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  int effect_key;
  int count=-1,i;

  // return number of numeric single valued, non-reinit
  // i.e. simple numeric parameters

  weed_plant_t *filter;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  do {
    i=get_nth_simple_param(filter,++count);
  } while (i!=-1);

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}


void lives_osc_cb_rte_getnchannels(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {

  int effect_key,mode;
  int count;

  weed_plant_t *plant;

  char *msg;
  
 if (!lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  plant=rte_keymode_get_instance(effect_key,mode);
  if (plant==NULL) plant=rte_keymode_get_filter(effect_key,mode);
  if (plant==NULL) return lives_osc_notify_failure();

  count=enabled_in_channels(plant, FALSE);

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}


void lives_osc_cb_rte_getnochannels(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				    NetworkReturnAddressPtr ra) {

  int effect_key;
  int count;
  int error;

  weed_plant_t *plant;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);
  plant=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));

  // handle compound fx
  if (plant!=NULL) while (weed_plant_has_leaf(plant,"host_next_instance")) plant=weed_get_plantptr_value(plant,"host_next_instance",&error);
  else plant=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (plant==NULL) return lives_osc_notify_failure();

  count=enabled_out_channels(plant, FALSE);

  msg=g_strdup_printf("%d",count);
  lives_status_send(msg);
  g_free(msg);
}



void lives_osc_cb_rte_getparammin(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (!weed_plant_has_leaf(ptmpl,"min")) {
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"min",0,-1);

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getoparammin(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();

  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];

  if (!weed_plant_has_leaf(ptmpl,"min")) {
    weed_free(out_ptmpls);
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"min",0,-1);

  lives_status_send(msg);
  g_free(msg);
  weed_free(out_ptmpls);

}


void lives_osc_cb_rte_getohasparammin(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				      NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();

  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];
  weed_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl,"min")) lives_status_send(get_omc_const("LIVES_FALSE"));
  else lives_status_send(get_omc_const("LIVES_TRUE"));

}


void lives_osc_cb_rte_getpparammin(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {

  int pnum;

  int error;
  weed_plant_t *ptmpl,*param;

  char *msg;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (!weed_plant_has_leaf(ptmpl,"min")) {
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"min",0,-1);

  lives_status_send(msg);
  g_free(msg);
}


void lives_osc_cb_rte_getparammax(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (!weed_plant_has_leaf(ptmpl,"max")) {
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"max",0,-1);

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getoparammax(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];

  if (!weed_plant_has_leaf(ptmpl,"max")) {
    weed_free(out_ptmpls);
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"max",0,-1);

  lives_status_send(msg);
  g_free(msg);
  weed_free(out_ptmpls);

}


void lives_osc_cb_rte_getohasparammax(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];
  weed_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl,"max")) lives_status_send(get_omc_const("LIVES_FALSE"));
  else lives_status_send(get_omc_const("LIVES_TRUE"));

}



void lives_osc_cb_rte_getpparammax(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  // playback plugin param max

  int pnum;

  int error;
  weed_plant_t *ptmpl,*param;

  char *msg;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&pnum);

  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (!weed_plant_has_leaf(ptmpl,"max")) {
    return lives_osc_notify_failure();
  }

  msg=lives_osc_format_result(ptmpl,"max",0,-1);

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum,nvals;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (weed_plant_has_leaf(ptmpl,"host_default")) {
    msg=lives_osc_format_result(ptmpl,"host_default",0,-1);
  }
  else {
    nvals=weed_leaf_num_elements(ptmpl,"default");
    if (nvals>0)
      msg=lives_osc_format_result(ptmpl,"default",0,nvals);
    else {
      // default can have 0 values if param has variable elements; in this case we use "new_default"
      msg=lives_osc_format_result(ptmpl,"new_default",0,-1);
    }
  }

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getoparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum,nvals;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];

  if (weed_plant_has_leaf(ptmpl,"host_default")) {
    msg=lives_osc_format_result(ptmpl,"host_default",0,-1);
  }
  else {
    if (!weed_plant_has_leaf(ptmpl,"default")) {
      weed_free(out_ptmpls);
      return lives_osc_notify_failure();
    }

    nvals=weed_leaf_num_elements(ptmpl,"default");
    if (nvals>0)
      msg=lives_osc_format_result(ptmpl,"default",0,nvals);
    else {
      // default can have 0 values if param has variable elements; in this case we use "new_default"
      msg=lives_osc_format_result(ptmpl,"new_default",0,-1);
    }
  }

  lives_status_send(msg);
  g_free(msg);
  weed_free(out_ptmpls);

}


void lives_osc_cb_rte_gethasparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				      NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (weed_plant_has_leaf(ptmpl,"host_default")) {
    if (weed_leaf_num_elements(ptmpl,"host_default")==0) lives_status_send(get_omc_const("LIVES_FALSE"));
    lives_status_send(get_omc_const("LIVES_DEFAULT_OVERRIDDEN"));
    return;
  }
  if (!weed_plant_has_leaf(ptmpl,"default")||weed_leaf_num_elements(ptmpl,"default")==0) lives_status_send(get_omc_const("LIVES_FALSE"));
  else lives_status_send(get_omc_const("LIVES_TRUE"));

}


void lives_osc_cb_rte_getohasparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				      NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error,nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];
  weed_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl,"host_default")&&!weed_plant_has_leaf(ptmpl,"default")) lives_status_send(get_omc_const("LIVES_FALSE"));
  else lives_status_send(get_omc_const("LIVES_TRUE"));

}




void lives_osc_cb_rte_getpparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  // default for playback plugin param

  int pnum,nvals;

  int error;
  weed_plant_t *param;
  weed_plant_t *ptmpl;

  char *msg;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();
  
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&pnum);


  if (pnum<0||pnum>=mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  nvals=weed_leaf_num_elements(ptmpl,"default");
  if (nvals>0)
    msg=lives_osc_format_result(ptmpl,"default",0,nvals);
  else {
    // default can have 0 values if param has variable elements; in this case we use "new_default"
    msg=lives_osc_format_result(ptmpl,"new_default",0,-1);
  }

  lives_status_send(msg);
  g_free(msg);

}




void lives_osc_cb_rte_getparamval(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				  NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,st=0,end=1,hint,cspace;

  int error,nparams;
  weed_plant_t *inst,*filter;
  weed_plant_t *param,*ptmpl;
  char *msg;
  
  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"iii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    lives_osc_parse_int_argument(vargs,&st);
    end=st+1;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();
  
  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));

  if (inst==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  param=weed_inst_in_param(inst,pnum,FALSE,TRUE);
  ptmpl=weed_get_plantptr_value(param,"template",&error);

  hint=weed_get_int_value(ptmpl,"hint",&error);
  if (hint==WEED_HINT_COLOR) {
    int valsize=4;
    cspace=weed_get_int_value(ptmpl,"colorspace",&error);
    if (cspace==WEED_COLORSPACE_RGB) valsize=3;
    st*=valsize;
    end=st+valsize;
  }

  if (end>weed_leaf_num_elements(param,"value")) return lives_osc_notify_failure();

  msg=lives_osc_format_result(param,"value",st,end);

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getoparamval(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,st=0,end=1,hint,cspace;

  int error,nparams;
  weed_plant_t *inst,*filter;
  weed_plant_t **out_params,**out_ptmpls;
  weed_plant_t *param,*ptmpl;
  char *msg;
  
  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"iii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    lives_osc_parse_int_argument(vargs,&st);
    end=st+1;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return lives_osc_notify_failure();
  nparams=weed_leaf_num_elements(filter,"out_parameter_templates");
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));
  if (inst==NULL) return lives_osc_notify_failure();

  out_ptmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_ptmpls[pnum];

  out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  param=out_params[pnum];

  weed_free(out_ptmpls);
  weed_free(out_params);

  hint=weed_get_int_value(ptmpl,"hint",&error);
  if (hint==WEED_HINT_COLOR) {
    int valsize=4;
    cspace=weed_get_int_value(ptmpl,"colorspace",&error);
    if (cspace==WEED_COLORSPACE_RGB) valsize=3;
    st*=valsize;
    end=st+valsize;
  }

  if (end>weed_leaf_num_elements(param,"value")) return lives_osc_notify_failure();

  msg=lives_osc_format_result(param,"value",st,end);

  lives_status_send(msg);
  g_free(msg);

}



void lives_osc_cb_rte_getpparamval(void *context, int arglen, const void *vargs, OSCTimeTag when, 
				   NetworkReturnAddressPtr ra) {
  // playback plugin param value
  int pnum,st=0,end=1,hint,cspace;

  int error;
  weed_plant_t *param,*ptmpl;
  char *msg;

  if (!mainw->ext_playback||mainw->vpp->play_params==NULL) return lives_osc_notify_failure();
  
  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&pnum);
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&pnum);
    lives_osc_parse_int_argument(vargs,&st);
    end=st+1;
  }

  param=(weed_plant_t *)pp_get_param(mainw->vpp->play_params,pnum);

  ptmpl=weed_get_plantptr_value(param,"template",&error);

  hint=weed_get_int_value(ptmpl,"hint",&error);
  if (hint==WEED_HINT_COLOR) {
    int valsize=4;
    cspace=weed_get_int_value(ptmpl,"colorspace",&error);
    if (cspace==WEED_COLORSPACE_RGB) valsize=3;
    st*=valsize;
    end=st+valsize;
  }

  if (end>weed_leaf_num_elements(param,"value")) return lives_osc_notify_failure();

  msg=lives_osc_format_result(param,"value",st,end);

  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getnparam(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  int error;
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint;

  int vali;
  double vald;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return lives_osc_notify_failure();

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    vali=weed_get_int_value(ptmpl,"value",&error);
    msg=g_strdup_printf("%d",vali);
  }
  else {
    vald=weed_get_double_value(ptmpl,"value",&error);
    msg=g_strdup_printf("%f",vald);
  }
  lives_status_send(msg);
  g_free(msg);
  weed_free(in_ptmpls);

}



void lives_osc_cb_rte_getnparammin(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  int error;
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint;

  int vali;
  double vald;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return lives_osc_notify_failure();

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    vali=weed_get_int_value(ptmpl,"min",&error);
    msg=g_strdup_printf("%d",vali);
  }
  else {
    vald=weed_get_double_value(ptmpl,"min",&error);
    msg=g_strdup_printf("%f",vald);
  }
  lives_status_send(msg);
  g_free(msg);
  weed_free(in_ptmpls);

}



void lives_osc_cb_rte_getnparammax(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  int error;
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint;

  int vali;
  double vald;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return lives_osc_notify_failure();

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    vali=weed_get_int_value(ptmpl,"max",&error);
    msg=g_strdup_printf("%d",vali);
  }
  else {
    vald=weed_get_double_value(ptmpl,"max",&error);
    msg=g_strdup_printf("%f",vald);
  }
  lives_status_send(msg);
  g_free(msg);
  weed_free(in_ptmpls);
}


void lives_osc_cb_rte_getnparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  int error;
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint;

  int vali;
  double vald;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return lives_osc_notify_failure();

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    if (!weed_plant_has_leaf(ptmpl,"host_default")) vali=weed_get_int_value(ptmpl,"default",&error);
    else vali=weed_get_int_value(ptmpl,"host_default",&error);
    msg=g_strdup_printf("%d",vali);
  }
  else {
    if (!weed_plant_has_leaf(ptmpl,"host_default")) vald=weed_get_double_value(ptmpl,"default",&error);
    else vald=weed_get_double_value(ptmpl,"host_default",&error);
    msg=g_strdup_printf("%f",vald);
  }

  lives_status_send(msg);
  g_free(msg);
  weed_free(in_ptmpls);
}


void lives_osc_cb_rte_getnparamtrans(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *filter;
  int nparams;

  boolean res=FALSE;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();

  if (pnum==get_transition_param(filter,TRUE)) res=TRUE;

  msg=g_strdup_printf("%d",res);
  lives_status_send(msg);
  g_free(msg);
  

}



void lives_osc_cb_rte_getparamtrans(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int mode;
  int pnum;

  int error;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int nparams;

  boolean res=FALSE;

  char *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&pnum);
    mode=rte_key_getmode(effect_key);
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
    lives_osc_parse_int_argument(vargs,&effect_key);
    lives_osc_parse_int_argument(vargs,&mode);
    lives_osc_parse_int_argument(vargs,&pnum);
    if (mode<1||mode>rte_key_getmaxmode(effect_key)+1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key<1||effect_key>FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,mode);
  if (filter==NULL) return lives_osc_notify_failure();

  nparams=num_in_params(filter,FALSE,TRUE);
  if (nparams==0) return lives_osc_notify_failure();
  if (pnum<0||pnum>=nparams) return lives_osc_notify_failure();
  
  ptmpl=weed_filter_in_paramtmpl(filter,pnum,TRUE);

  if (weed_plant_has_leaf(ptmpl,"transition")&&weed_get_boolean_value(ptmpl,"transition",&error)==WEED_TRUE) res=TRUE;
  msg=g_strdup_printf("%d",res);
  lives_status_send(msg);
  g_free(msg);

}


void lives_osc_cb_rte_getmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_MAX) {
    lives_status_send ("0");
    return;
  }

  lives_status_send ((tmp=g_strdup_printf ("%d",rte_key_getmode (effect_key)+1)));
  g_free(tmp);

}


void lives_osc_cb_rte_getstate(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_KEYS_MAX_VIRTUAL) {
    lives_status_send (get_omc_const("LIVES_FALSE"));
    return;
  }
  if (rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key))==NULL) lives_status_send(get_omc_const("LIVES_FALSE"));
  else lives_status_send(get_omc_const("LIVES_TRUE"));
}



void lives_osc_cb_rte_get_keyfxname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  char *tmp;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&mode);
  if (effect_key<1||effect_key>FX_MAX||mode<1||mode>rte_getmodespk()) return lives_osc_notify_failure();
  lives_status_send ((tmp=g_strdup_printf ("%s",rte_keymode_get_filter_name (effect_key,mode-1))));
  g_free(tmp);
}


void lives_osc_cb_rte_getmodespk(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  char *tmp;

  if (!lives_osc_check_arguments (arglen,vargs,"i",FALSE)) {
    if (lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
      lives_status_send ((tmp=g_strdup_printf ("%d",rte_getmodespk ())));
      g_free(tmp);
    }
    return;
  }

  lives_osc_check_arguments (arglen,vargs,"i",TRUE);
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key>FX_KEYS_MAX_VIRTUAL||effect_key<1) {
    lives_status_send ("0");
    return;
  }

  lives_status_send ((tmp=g_strdup_printf ("%d",rte_key_getmaxmode (effect_key)+1)));
  g_free(tmp);

}



void lives_osc_cb_rte_addpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  weed_plant_t *ofilter,*ifilter;

  int key0,mode0,pnum0;
  int key1,mode1,pnum1;
  int autoscale;

  if (!lives_osc_check_arguments (arglen,vargs,"iiiiiii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&key0);
  lives_osc_parse_int_argument(vargs,&mode0);
  lives_osc_parse_int_argument(vargs,&pnum0);

  lives_osc_parse_int_argument(vargs,&autoscale);

  lives_osc_parse_int_argument(vargs,&key1);
  lives_osc_parse_int_argument(vargs,&mode1);
  lives_osc_parse_int_argument(vargs,&pnum1);

  if (key0<1||key0>=FX_KEYS_MAX_VIRTUAL||mode0<1||mode0>rte_getmodespk()) return lives_osc_notify_failure();
  if (key1<-2||key1==0||key1>=FX_KEYS_MAX_VIRTUAL||mode1<1||mode1>rte_getmodespk()) return lives_osc_notify_failure();

  if (key0==key1) lives_osc_notify_failure();

  if (autoscale!=TRUE&&autoscale!=FALSE) lives_osc_notify_failure();

  mode0--;
  mode1--;


  ofilter=rte_keymode_get_filter(key0,mode0);
  if (ofilter==NULL) return lives_osc_notify_failure();

  if (pnum0>=num_out_params(ofilter)) return lives_osc_notify_failure();

  if (key1==-1) {
    // connecting to the playback plugin
    if (mode1>1||mainw->vpp==NULL||pnum1>=mainw->vpp->num_play_params) return lives_osc_notify_failure();
  }
  else if (key1==-2) {
    // connecting to subtitler
    if (mode1>1||pnum1>0) return lives_osc_notify_failure();
  }
  else {
    ifilter=rte_keymode_get_filter(key1,mode1);
    if (ifilter==NULL) return lives_osc_notify_failure();
    
    if (pnum1>=num_in_params(ifilter,FALSE,TRUE)) return lives_osc_notify_failure();
  }

  if (pnum0<-EXTRA_PARAMS_OUT||pnum1<-EXTRA_PARAMS_IN) return lives_osc_notify_failure();

  if (pconx_check_connection(ofilter,pnum0,key1,mode1,pnum1,FALSE,NULL,NULL)) return lives_osc_notify_failure();

  key0--;
  key1--;

  pconx_add_connection(key0,mode0,pnum0,key1,mode1,pnum1,autoscale);
  lives_osc_notify_success(NULL);
}

void lives_osc_cb_rte_delpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int key0,mode0,pnum0;
  int key1,mode1,pnum1;

  if (!lives_osc_check_arguments (arglen,vargs,"iiiiii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&key0);
  lives_osc_parse_int_argument(vargs,&mode0);
  lives_osc_parse_int_argument(vargs,&pnum0);
  lives_osc_parse_int_argument(vargs,&key1);
  lives_osc_parse_int_argument(vargs,&mode1);
  lives_osc_parse_int_argument(vargs,&pnum1);

  if (key0<0||key0>=FX_KEYS_MAX_VIRTUAL||mode0<1||mode0>rte_getmodespk()) return lives_osc_notify_failure();
  if (key1<-2||key1>=FX_KEYS_MAX_VIRTUAL||mode1<1||mode1>rte_getmodespk()) return lives_osc_notify_failure();

  if (pnum0<-EXTRA_PARAMS_OUT||pnum1<-EXTRA_PARAMS_IN) return lives_osc_notify_failure();

  pconx_delete(key0==0?FX_DATA_WILDCARD:--key0,--mode0,pnum0,key1==0?FX_DATA_WILDCARD:--key1,--mode1,pnum1);
  lives_osc_notify_success(NULL);

}

void lives_osc_cb_rte_listpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int okey,omode,opnum;
  char *msg;
 
  if (!lives_osc_check_arguments (arglen,vargs,"iii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&okey);
  lives_osc_parse_int_argument(vargs,&omode);
  lives_osc_parse_int_argument(vargs,&opnum);

  if (okey<1||okey>=FX_KEYS_MAX_VIRTUAL||omode<1||omode>rte_getmodespk()) return lives_osc_notify_failure();

  msg=pconx_list(okey,omode,opnum);

  if (strlen(msg)==0) {
    g_free(msg);
    msg=g_strdup("0 0 0 0");
  }

  lives_status_send(msg);
  g_free(msg);

}

void lives_osc_cb_rte_addcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int key0,mode0,cnum0;
  int key1,mode1,cnum1;
  weed_plant_t *filter;

  if (!lives_osc_check_arguments (arglen,vargs,"iiiiii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&key0);
  lives_osc_parse_int_argument(vargs,&mode0);
  lives_osc_parse_int_argument(vargs,&cnum0);
  lives_osc_parse_int_argument(vargs,&key1);
  lives_osc_parse_int_argument(vargs,&mode1);
  lives_osc_parse_int_argument(vargs,&cnum1);

  if (key0<1||key0>=FX_KEYS_MAX_VIRTUAL||mode0<1||mode0>rte_getmodespk()) return lives_osc_notify_failure();
  if (key1<-1||key1==0||key1>=FX_KEYS_MAX_VIRTUAL||mode1<1||mode1>rte_getmodespk()) return lives_osc_notify_failure();

  if (key0==key1) lives_osc_notify_failure();

  mode0--;
  mode1--;

  filter=rte_keymode_get_filter(key0,mode0);
  if (filter==NULL) return lives_osc_notify_failure();

  if (cnum0>=enabled_out_channels(filter,FALSE)) return lives_osc_notify_failure();

  if (key1==-1) {
    // connecting to the playback plugin
    if (mode1>1||mainw->vpp==NULL||cnum1>=mainw->vpp->num_alpha_chans) return lives_osc_notify_failure();
  }
  else {
    filter=rte_keymode_get_filter(key1,mode1);
    if (filter==NULL) return lives_osc_notify_failure();

    if (cnum1>=enabled_in_channels(filter,FALSE)) return lives_osc_notify_failure();
  }

  if (cconx_check_connection(key1,mode1,cnum1,FALSE,NULL,NULL)) return lives_osc_notify_failure();

  key0--;
  key1--;

  cconx_add_connection(key0,mode0,cnum0,key1,mode1,cnum1);
  lives_osc_notify_success(NULL);

}

void lives_osc_cb_rte_delcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int key0,mode0,cnum0;
  int key1,mode1,cnum1;

  if (!lives_osc_check_arguments (arglen,vargs,"iiiiii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&key0);
  lives_osc_parse_int_argument(vargs,&mode0);
  lives_osc_parse_int_argument(vargs,&cnum0);
  lives_osc_parse_int_argument(vargs,&key1);
  lives_osc_parse_int_argument(vargs,&mode1);
  lives_osc_parse_int_argument(vargs,&cnum1);

  if (key0<0||key0>=FX_KEYS_MAX_VIRTUAL||mode0<1||mode0>rte_getmodespk()) return lives_osc_notify_failure();
  if (key1<-2||key1>=FX_KEYS_MAX_VIRTUAL||mode1<1||mode1>rte_getmodespk()) return lives_osc_notify_failure();

  cconx_delete(key0==0?FX_DATA_WILDCARD:--key0,--mode0,cnum0,key1==0?FX_DATA_WILDCARD:--key1,--mode1,cnum1);

  lives_osc_notify_success(NULL);
}

void lives_osc_cb_rte_listcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int okey,omode,ocnum;
  char *msg;

  if (!lives_osc_check_arguments (arglen,vargs,"iii",TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs,&okey);
  lives_osc_parse_int_argument(vargs,&omode);
  lives_osc_parse_int_argument(vargs,&ocnum);

  if (okey<1||okey>=FX_KEYS_MAX_VIRTUAL||omode<1||omode>rte_getmodespk()) return lives_osc_notify_failure();

  msg=cconx_list(okey,omode,ocnum);

  if (strlen(msg)==0) {
    g_free(msg);
    msg=g_strdup("0 0 0");
  }

  lives_status_send(msg);
  g_free(msg);
}


void lives_osc_cb_swap(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  swap_fg_bg_callback (NULL,NULL,0,(LiVESModifierType)0,NULL);
  lives_osc_notify_success(NULL);
}


void lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  record_toggle_callback (NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER((int)TRUE));
  lives_osc_notify_success(NULL);
  // TODO - send record start and record stop events
}


void lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  record_toggle_callback (NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER((int)FALSE));
  lives_osc_notify_success(NULL);
}

void lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack!=NULL) return lives_osc_notify_failure();
  record_toggle_callback (NULL,NULL,0,(LiVESModifierType)0,LIVES_INT_TO_POINTER(!mainw->record));
  lives_osc_notify_success(NULL);
}


void lives_osc_cb_ping(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (status_socket==NULL) return;
  lives_status_send ("pong");
}


void lives_osc_cb_open_file(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char filename[OSC_STRING_SIZE];
  float starttime=0.;
  int numframes=0; // all frames by default

  int type=0;

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"sfi",FALSE)) {
    type++;
    if (!lives_osc_check_arguments (arglen,vargs,"sf",FALSE)) {
      type++;
      if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) return lives_osc_notify_failure();
    }
    else lives_osc_check_arguments (arglen,vargs,"sf",TRUE);
  }
  else lives_osc_check_arguments (arglen,vargs,"sfi",TRUE);

  lives_osc_parse_string_argument(vargs,filename);
  if (type<2) {
    lives_osc_parse_float_argument(vargs,&starttime);
    if (type<1) {
      lives_osc_parse_int_argument(vargs,&numframes);
    }
  }
  deduce_file(filename,starttime,numframes);
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_new_audio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char filename[OSC_STRING_SIZE];

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (mainw->current_file<1 || cfile==NULL || cfile->opening || cfile->frames==0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_string_argument(vargs,filename);
  on_open_new_audio_clicked(NULL,filename);
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_loadset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char setname[OSC_STRING_SIZE];

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (strlen(mainw->set_name)>0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) {
    return lives_osc_notify_failure();
  }
  lives_osc_parse_string_argument(vargs,setname);

  mainw->osc_auto=TRUE;
  if (!is_legal_set_name(setname,TRUE)) {
    mainw->osc_auto=FALSE;
    return lives_osc_notify_failure();
  }
  mainw->osc_auto=FALSE;

  g_snprintf(mainw->set_name,128,"%s",setname);

  on_load_set_ok(NULL,LIVES_INT_TO_POINTER((int)FALSE));
  lives_osc_notify_success(NULL);

}


void lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char setname[OSC_STRING_SIZE];

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return lives_osc_notify_failure();

  if (mainw->playing_file>-1) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) {
    return lives_osc_notify_failure();
  }

  lives_osc_parse_string_argument(vargs,setname);
  if (is_legal_set_name(setname,TRUE)) {
    mainw->only_close=TRUE;
    on_save_set_activate(NULL,setname);
    mainw->only_close=FALSE;
  }
  lives_osc_notify_success(NULL);

}




static struct 
{
  char	 *descr;
  char	 *name;
  void	 (*cb)(void *ctx, int len, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra);
  int		 leave; // leaf
} osc_methods[] = 
  {
    { "/record/enable",		"enable",		lives_osc_record_start,			3	},	
    { "/record/disable",	"disable",		lives_osc_record_stop,			3	},	
    { "/record/toggle",	        "toggle",		lives_osc_record_toggle,			3	},	
    { "/video/play",		"play",		lives_osc_cb_play,			5	},	
    { "/video/selection/play",		"play",		lives_osc_cb_playsel,			46	},	
    { "/video/play/forwards",		"forwards",		lives_osc_cb_play_forward,			36	},	
    { "/video/play/backwards",		"backwards",		lives_osc_cb_play_backward,			36	},	
    { "/video/play/faster",		"faster",		lives_osc_cb_play_faster,			36	},	
    { "/clip/foreground/fps/faster",		"faster",		lives_osc_cb_play_faster,			61	},	
    { "/clip/foreground/fps/get",		"get",		lives_osc_cb_clip_getfps,			61	},	
    { "/clip/background/fps/faster",		"faster",		lives_osc_cb_bgplay_faster,			63	},	
    { "/clip/background/fps/get",		"get",		lives_osc_cb_bgclip_getfps,			63	},	
    { "/video/play/slower",		"slower",		lives_osc_cb_play_slower,			36	},	
    { "/clip/foreground/fps/slower",		"slower",		lives_osc_cb_play_slower,			61	},	
    { "/clip/background/fps/slower",		"slower",		lives_osc_cb_bgplay_slower,			63	},	
    { "/video/play/reset",		"reset",		lives_osc_cb_play_reset,			36	},	
    { "/video/play/parameter/count",		"set",		lives_osc_cb_rte_pparamcount,			140	},
    { "/video/play/parameter/value/set",		"set",		lives_osc_cb_rte_setpparam,			140	},
    { "/video/play/parameter/flags/get",		"get",	lives_osc_cb_rte_getpparamflags,        141	},
    { "/video/play/parameter/min/get",		"get",	lives_osc_cb_rte_getpparammin,		        142	},
    { "/video/play/parameter/max/get",		"get",	lives_osc_cb_rte_getpparammax,		        143	},
    { "/video/play/parameter/type/get",		"get",	lives_osc_cb_rte_getpparamtype,		        144	},
    { "/video/play/parameter/name/get",		"get",	lives_osc_cb_rte_getpparamname,		        145	},
    { "/video/play/parameter/colorspace/get",		"get",	lives_osc_cb_rte_getpparamcspace,      	        146	},
    { "/video/play/parameter/default/get",		"get",	lives_osc_cb_rte_getpparamdef,		        147	},
    { "/video/play/parameter/value/get",		"get",	lives_osc_cb_rte_getpparamval,		        140	},
    { "/clip/foreground/fps/reset",		"reset",		lives_osc_cb_play_reset,			61	},	
    { "/clip/background/fps/reset",		"reset",		lives_osc_cb_bgplay_reset,			63	},	
    { "/video/stop",		"stop",	        lives_osc_cb_stop,				5	},
    { "/video/fps/set",	       "set",	lives_osc_cb_set_fps,			40	},
    { "/video/fps/get",	       "get",	lives_osc_cb_clip_getfps,			40	},
    { "/video/loop/set",	       "set",	lives_osc_cb_set_loop,			38	},
    { "/video/loop/get",	       "get",	lives_osc_cb_get_loop,			38	},
    { "/video/pingpong/set",	       "set",	lives_osc_cb_set_pingpong,			39	},
    { "/video/pingpong/get",	       "get",	lives_osc_cb_get_pingpong,			39	},
    { "/lives/mode/set",	       "set",	lives_osc_cb_setmode,			103	},
    { "/lives/mode/get",	       "get",	lives_osc_cb_getmode,			103	},
    { "/video/fps/ratio/set",	       "set",	lives_osc_cb_set_fps_ratio,			65	},
    { "/video/fps/ratio/get",	       "get",	lives_osc_cb_get_fps_ratio,			65	},
    { "/video/play/time/get",	       "get",	lives_osc_cb_get_playtime,			67	},
    { "/clip/foreground/fps/set",	"set",	lives_osc_cb_set_fps,			61	},
    { "/clip/background/fps/set",	"set",	lives_osc_cb_bgset_fps,			63	},
    { "/clip/foreground/fps/ratio/set",	"set",	lives_osc_cb_set_fps_ratio,			64	},
    { "/clip/foreground/fps/ratio/get",	"get",	lives_osc_cb_get_fps_ratio,			64	},
    { "/clip/background/fps/ratio/set",	"set",	lives_osc_cb_bgset_fps_ratio,			66	},
    { "/clip/background/fps/ratio/get",	"get",	lives_osc_cb_bgget_fps_ratio,			66	},
    { "/video/play/reverse",		"reverse",	lives_osc_cb_play_reverse,		36	},
    { "/clip/foreground/fps/reverse",	"reverse",	lives_osc_cb_play_reverse,		61	},
    { "/clip/background/fps/reverse",	"reverse",	lives_osc_cb_bgplay_reverse,		63	},
    { "/video/freeze/toggle",		"toggle", lives_osc_cb_freeze,		37	},
    { "/effects/realtime/name/get",		"get",	lives_osc_cb_fx_getname,			115	},
    { "/effect_key/map",		"map",	lives_osc_cb_fx_map,			25	},
    { "/effect_key/map/clear",		"clear",	lives_osc_cb_fx_map_clear,			32	},
    { "/effect_key/reset",		"reset",	lives_osc_cb_fx_reset,			25	},
    { "/effect_key/enable",		"enable",	lives_osc_cb_fx_enable,		        25	},
    { "/effect_key/disable",		"disable",	lives_osc_cb_fx_disable,		        25	},
    { "/effect_key/toggle",		"toggle",	lives_osc_cb_fx_toggle,		        25	},
    { "/effect_key/count",		"count",	lives_osc_cb_rte_count,		        25	},
    { "/effect_key/parameter/value/set",		"set",	lives_osc_cb_rte_setparam,		        42	},
    { "/effect_key/parameter/type/get",		"get",	lives_osc_cb_rte_getparamtype,		        68	},
    { "/effect_key/outparameter/type/get",		"get",	lives_osc_cb_rte_getoparamtype,		        153	},
    { "/effect_key/nparameter/type/get",		"get",	lives_osc_cb_rte_getnparamtype,		        116	},
    { "/effect_key/parameter/name/get",		"get",	lives_osc_cb_rte_getparamname,		        71	},
    { "/effect_key/outparameter/name/get",		"get",	lives_osc_cb_rte_getoparamname,		        152	},
    { "/effect_key/nparameter/name/get",		"get",	lives_osc_cb_rte_getnparamname,		        72	},
    { "/effect_key/parameter/colorspace/get",		"get",	lives_osc_cb_rte_getparamcspace,		        73	},
    { "/effect_key/outparameter/colorspace/get",		"get",	lives_osc_cb_rte_getoparamcspace,		        154	},
    { "/effect_key/parameter/flags/get",		"get",	lives_osc_cb_rte_getparamflags,		        74	},
    { "/effect_key/parameter/min/get",		"get",	lives_osc_cb_rte_getparammin,		        75	},
    { "/effect_key/parameter/max/get",		"get",	lives_osc_cb_rte_getparammax,		        76	},
    { "/effect_key/parameter/default/get",		"get",	lives_osc_cb_rte_getparamdef,		        77	},
    { "/effect_key/parameter/default/set",		"set",	lives_osc_cb_rte_setparamdef,		        77	},
    { "/effect_key/parameter/group/get",		"get",	lives_osc_cb_rte_getparamgrp,		        78	},
    { "/effect_key/outparameter/min/get",		"get",	lives_osc_cb_rte_getoparammin,		        156	},
    { "/effect_key/outparameter/max/get",		"get",	lives_osc_cb_rte_getoparammax,		        157	},
    { "/effect_key/outparameter/default/get",		"get",	lives_osc_cb_rte_getoparamdef,		        158	},
    { "/effect_key/outparameter/has_min",		"has_min",	lives_osc_cb_rte_getohasparammin,		        150	},
    { "/effect_key/outparameter/has_max",		"has_max",	lives_osc_cb_rte_getohasparammax,		        150	},
    { "/effect_key/outparameter/has_default",		"has_default",	lives_osc_cb_rte_getohasparamdef,		        150	},
    { "/effect_key/parameter/has_default",		"has_default",	lives_osc_cb_rte_gethasparamdef,		        41	},
    { "/effect_key/parameter/value/get",		"get",	lives_osc_cb_rte_getparamval,		        42	},
    { "/effect_key/outparameter/value/get",		"get",	lives_osc_cb_rte_getoparamval,		        155	},
    { "/effect_key/nparameter/count",		"count",	lives_osc_cb_rte_nparamcount,		        91	},
    { "/effect_key/parameter/count",		"count",	lives_osc_cb_rte_paramcount,		        41	},
    { "/effect_key/outparameter/count",		"count",	lives_osc_cb_rte_oparamcount,		        150	},
    { "/effect_key/nparameter/value/set",		"set",	lives_osc_cb_rte_setnparam,		        92	},
    { "/effect_key/nparameter/value/get",		"get",	lives_osc_cb_rte_getnparam,		        92	},
    { "/effect_key/nparameter/min/get",		"get",	lives_osc_cb_rte_getnparammin,		        93	},
    { "/effect_key/nparameter/max/get",		"get",	lives_osc_cb_rte_getnparammax,		        94	},
    { "/effect_key/nparameter/default/get",		"get",	lives_osc_cb_rte_getnparamdef,		        95	},
    { "/effect_key/nparameter/default/set",		"set",	lives_osc_cb_rte_setnparamdef,		        95	},
    { "/effect_key/nparameter/is_transition",		"is_transition",	lives_osc_cb_rte_getnparamtrans,		        91	},
    { "/effect_key/parameter/is_transition",		"is_transition",	lives_osc_cb_rte_getparamtrans,		        41	},
    { "/effect_key/inchannel/active/count",		"count",	lives_osc_cb_rte_getnchannels,		        131	},
    { "/effect_key/inchannel/palette/get",		"get",	lives_osc_cb_rte_getinpal,		        132	},
    { "/effect_key/outchannel/active/count",		"count",	lives_osc_cb_rte_getnochannels,		        171	},
    { "/effect_key/outchannel/palette/get",		"get",	lives_osc_cb_rte_getoutpal,		        162	},
    { "/effect_key/mode/set",		"set",	lives_osc_cb_rte_setmode,		        43	},
    { "/effect_key/mode/get",		"get",	lives_osc_cb_rte_getmode,		        43	},
    { "/effect_key/mode/next",		"next",	lives_osc_cb_rte_nextmode,		        43	},
    { "/effect_key/mode/previous",		"previous",	lives_osc_cb_rte_prevmode,		        43	},
    { "/effect_key/name/get",		"get",	lives_osc_cb_rte_get_keyfxname,		        44	},
    { "/effect_key/maxmode/get",		"get",	lives_osc_cb_rte_getmodespk,		        45	},
    { "/effect_key/state/get",		"get",	lives_osc_cb_rte_getstate,		        56	},
    { "/effect_key/outparameter/connection/add",		"add",	lives_osc_cb_rte_addpconnection,		        151	},
    { "/effect_key/outparameter/connection/delete",		"delete",	lives_osc_cb_rte_delpconnection,		        151	},
    { "/effect_key/outparameter/connection/list",		"list",	lives_osc_cb_rte_listpconnection,		        151	},
    { "/effect_key/outchannel/connection/add",		        "add",	lives_osc_cb_rte_addcconnection,		        161	},
    { "/effect_key/outchannel/connection/delete",		"delete",	lives_osc_cb_rte_delcconnection,		        161	},
    { "/effect_key/outchannel/connection/list",		"list",	lives_osc_cb_rte_listcconnection,		        161	},
    { "/clip/encode_as",		"encode_as",	lives_osc_cb_clip_encodeas,			1	},
    { "/clip/select",		"select",	lives_osc_cb_fgclip_select,			1	},
    { "/clip/close",		"close",	lives_osc_cb_clip_close,	  		        1	},
    { "/clip/copy",		"copy",	lives_osc_cb_fgclip_copy,	  		        1	},
    { "/clip/selection/copy",		"copy",	lives_osc_cb_fgclipsel_copy,	  		        55	},
    { "/clip/selection/cut",		"cut",	lives_osc_cb_fgclipsel_cut,	  		        55	},
    { "/clip/selection/delete",		"delete",	lives_osc_cb_fgclipsel_delete,	  		        55	},
    { "/clipboard/paste",		"paste",	lives_osc_cb_clipbd_paste,			70	},
    { "/clipboard/insert_before",		"insert_before",	lives_osc_cb_clipbd_insertb,			70	},
    { "/clipboard/insert_after",		"insert_after",	lives_osc_cb_clipbd_inserta,			70	},
    { "/clip/retrigger",		"retrigger",	lives_osc_cb_fgclip_retrigger,			1	},
    { "/clip/resample",		        "resample",	lives_osc_cb_clip_resample,			1	},
    { "/clip/select/next",		"next",	lives_osc_cb_fgclip_select_next,			54	},
    { "/clip/select/previous",		"previous",	lives_osc_cb_fgclip_select_previous,			54	},
    { "/clip/foreground/select",		"select",	lives_osc_cb_fgclip_select,			47	},
    { "/clip/background/select",		"select",	lives_osc_cb_bgclip_select,			48	},
    { "/clip/foreground/retrigger",		"retrigger",	lives_osc_cb_fgclip_retrigger,			47	},
    { "/clip/background/retrigger",		"retrigger",	lives_osc_cb_bgclip_retrigger,			48	},
    { "/clip/foreground/set",		"set",	lives_osc_cb_fgclip_set,			47	},
    { "/clip/background/set",		"set",	lives_osc_cb_bgclip_set,			48	},
    { "/clip/foreground/get",		"get",	lives_osc_cb_clip_get_current,			47	},
    { "/clip/background/get",		"get",	lives_osc_cb_bgclip_get_current,			48	},
    { "/clip/foreground/next",		"next",	lives_osc_cb_fgclip_select_next,			47	},
    { "/clip/background/next",		"next",	lives_osc_cb_bgclip_select_next,			48	},
    { "/clip/foreground/previous",		"previous",	lives_osc_cb_fgclip_select_previous,			47	},
    { "/clip/background/previous",		"previous",	lives_osc_cb_bgclip_select_previous,			48	},
    { "/lives/quit",	         "quit",	        lives_osc_cb_quit,			21	},
    { "/lives/version/get",	         "get",	        lives_osc_cb_getversion,			24	},
    { "/lives/constant/value/get",	         "get",	        lives_osc_cb_getconst,			121	},
    { "/app/quit",	         "quit",	           lives_osc_cb_quit,			22	},
    { "/app/name",	         "name",	           lives_osc_cb_getname,			22	},
    { "/app/name/get",	         "get",	           lives_osc_cb_getname,			23	},
    { "/app/version/get",	         "get",	           lives_osc_cb_getversion,			125	},
    { "/quit",	         "quit",	        lives_osc_cb_quit,			2	},
    { "/reply_to",	         "reply_to",	 lives_osc_cb_open_status_socket,			2	},
    { "/lives/open_status_socket",	         "open_status_socket",	        lives_osc_cb_open_status_socket,			21	},
    { "/app/open_status_socket",	         "open_status_socket",	        lives_osc_cb_open_status_socket,			22	},
    { "/app/ping",	         "ping",	           lives_osc_cb_ping,			22	},
    { "/lives/ping",	         "ping",	           lives_osc_cb_ping,			21	},
    { "/ping",	         "ping",	           lives_osc_cb_ping,			2	},
    { "/notify_to",	         "notify_to",	   lives_osc_cb_open_notify_socket,			2	},
    { "/lives/open_notify_socket",	         "open_notify_socket",	        lives_osc_cb_open_notify_socket,			21	},
    { "/notify/confirmations/set",	         "set",	        lives_osc_cb_notify_c,			101	},
    { "/notify/events/set",	         "set",	        lives_osc_cb_notify_e,			102	},
    { "/clip/count",	         "count",	        lives_osc_cb_clip_count,			1  },
    { "/clip/goto",	         "goto",	        lives_osc_cb_clip_goto,			1	},
    { "/clip/foreground/frame/set",	         "set",	        lives_osc_cb_clip_goto,			60	},
    { "/clip/foreground/frame/get",	         "get",	        lives_osc_cb_clip_getframe,			60	},
    { "/clip/background/frame/set",	         "set",	        lives_osc_cb_bgclip_goto,			62	},
    { "/clip/background/frame/get",	         "get",	        lives_osc_cb_bgclip_getframe,			62	},
    { "/clip/is_valid/get",	         "get",	        lives_osc_cb_clip_isvalid,			49	},
    { "/clip/frame/count",	         "count",	        lives_osc_cb_clip_get_frames,			57	},
    { "/clip/frame/save_as_image",	         "save_as_image",	        lives_osc_cb_clip_save_frame,			57	},
    { "/clip/select_all",	         "select_all",	        lives_osc_cb_clip_select_all,			1	},
    { "/clip/start/set",	 "set",	        lives_osc_cb_clip_set_start,			50	},
    { "/clip/start/get",	 "get",	        lives_osc_cb_clip_get_start,			50	},
    { "/clip/end/set",	 "set",	        lives_osc_cb_clip_set_end,			51	},
    { "/clip/end/get",	 "get",	        lives_osc_cb_clip_get_end,			51	},
    { "/clip/size/get",	 "get",	        lives_osc_cb_clip_get_size,			58	},
    { "/clip/name/get",	 "get",	        lives_osc_cb_clip_get_name,			59	},
    { "/clip/name/set",	 "set",	        lives_osc_cb_clip_set_name,			59	},
    { "/clip/fps/get",	 "get",	        lives_osc_cb_clip_get_ifps,			113	},
    { "/clip/open/file",	 "file",	        lives_osc_cb_open_file,			33	},
    { "/clip/audio/new",	 "new", 	        lives_osc_cb_new_audio,			108	},
    { "/output/fullscreen/enable",		"enable",	lives_osc_cb_fssepwin_enable,		28	},
    { "/output/fullscreen/disable",		"disable",	lives_osc_cb_fssepwin_disable,       	28	},
    { "/output/fps/set",		"set",	lives_osc_cb_op_fps_set,       	52	},
    { "/output/nodrop/enable",		"enable",	lives_osc_cb_op_nodrope,       	30	},
    { "/output/nodrop/disable",		"disable",	lives_osc_cb_op_nodropd,       	30	},
    { "/clip/foreground/background/swap",		"swap",	lives_osc_cb_swap,       	53	},
    { "/clipset/load",		"load",	lives_osc_cb_loadset,       	35	},
    { "/clipset/save",		"save",	lives_osc_cb_saveset,       	35	},
    { "/layout/clear",		"clear",	lives_osc_cb_clearlay,       	104	},
    { "/block/count",		"count",	lives_osc_cb_blockcount,       	105	},
    { "/block/insert",		"insert",	lives_osc_cb_blockinsert,       	105	},
    { "/block/start/time/get",		"get",	lives_osc_cb_blockstget,       	111	},
    { "/block/end/time/get",		"get",	lives_osc_cb_blockenget,       	112	},
    { "/mt/time/get",		"get",	lives_osc_cb_mtctimeget,       	201	},
    { "/mt/time/set",		"set",	lives_osc_cb_mtctimeset,       	201	},
    { "/mt/ctrack/get",		"get",	lives_osc_cb_mtctrackget,       	201	},
    { "/mt/ctrack/set",		"set",	lives_osc_cb_mtctrackset,       	201	},
    { "/test",		"",	lives_osc_cb_test,       	500	},
    
    { NULL,					NULL,		NULL,							0	},
  };


static struct
{
  char *comment; // leaf comment
  char *name;  // leaf name
  int  leave; // leaf number
  int  att;  // attached to parent number
  int  it; // ???
} osc_cont[] = 
  {
    {	"/",	 	"",	                 2, -1,0   	},
    {	"/video/",	 	"video",	 5, -1,0   	},
    {	"/video/selection/",	 	"selection",	 46, 5,0   	},
    {	"/video/fps/",	 	"fps",	 40, 5,0   	},
    {	"/video/fps/ratio/",	 	"ratio",	 65, 40,0   	},
    {	"/video/play/ start video playback",	 	"play",	         36, 5,0   	},
    {	"/video/play/time",	 	"time",	         67, 36,0   	},
    {	"/video/play/parameter",	 	"parameter",	         69, 36,0   	},
    {	"/video/play/parameter/value",	 	"value",	         140, 69,0   	},
    {	"/video/play/parameter/flags",	 	"flags",	         141, 69,0   	},
    {	"/video/play/parameter/min",	 	"min",	         142, 69,0   	},
    {	"/video/play/parameter/max",	 	"max",	         143, 69,0   	},
    {	"/video/play/parameter/type",	 	"type",	         144, 69,0   	},
    {	"/video/play/parameter/name",	 	"name",	         145, 69,0   	},
    {	"/video/play/parameter/colorspace",	 	"colorspace",	 146, 69,0   	},
    {	"/video/play/parameter/default",	"default",	 147, 69,0   	},
    {	"/video/freeze/",	"freeze",        37, 5,0   	},
    {	"/video/loop/",	"loop",        38, 5,0   	},
    {	"/video/pingpong/",	"pingpong",        39, 5,0   	},
    {	"/clip/", 		"clip",		 1, -1,0	},
    {	"/clip/fps/", 		"fps",		 113, 1,0	},
    {	"/clip/foreground/", 	"foreground",    47, 1,0	},
    {	"/clip/foreground/valid/", 	"valid",    80, 1,0	},
    {	"/clip/foreground/background/",  "background",    53, 47,0	},
    {	"/clip/foreground/frame/",  "frame",    60, 47,0	},
    {	"/clip/foreground/fps/",  "fps",    61, 47,0	},
    {	"/clip/foreground/fps/ratio/",  "ratio",    64, 61,0	},
    {	"/clip/background/", 	"background",    48, 1,0	},
    {	"/clip/background/valid/", 	"valid",    81, 1,0	},
    {	"/clip/background/frame/",  "frame",    62, 48,0	},
    {	"/clip/background/fps/",  "fps",    63, 48,0	},
    {	"/clip/background/fps/ratio/",  "ratio",    66, 63,0	},
    {	"/clip/is_valid/", 	"is_valid",      49, 1,0	},
    {	"/clip/frame/", 	"frame",      57, 1,0	},
    {	"/clip/start/", 	"start",         50, 1,0	},
    {	"/clip/end/", 	        "end",           51, 1,0	},
    {	"/clip/select/", 	        "select",           54, 1,0	},
    {	"/clip/selection/", 	        "selection",           55, 1,0	},
    {	"/clip/size/", 	        "size",           58, 1,0	},
    {	"/clip/name/", 	        "name",           59, 1,0	},
    {	"/clip/audio/", 	"audio",           108, 1,0	},
    {	"/clipboard/", 		"clipboard",		 70, -1,0	},
    {	"/record/", 		"record",	 3, -1,0	},
    {	"/effect/" , 		"effects",	 4, -1,0	},
    {	"/effect/realtime/" , 		"realtime",	 114, 4,0	},
    {	"/effect/realtime/name/" , 		"name",	 115, 114,0	},
    {	"/effect_key/" , 		"effect_key",	 25, -1,0	},
    {	"/effect_key/inchannel/" , 	"inchannel",	 130, 25,0	},
    {	"/effect_key/inchannel/active/" , 	"active",	 131, 130,0	},
    {	"/effect_key/inchannel/palette/" , 	"palette",	 132, 130,0	},
    {	"/effect_key/parameter/" , 	"parameter",	 41, 25,0	},
    {	"/effect_key/parameter/value/" ,"value",	 42, 41,0	},
    {	"/effect_key/parameter/type/" ,"type",	 68, 41,0	},
    {	"/effect_key/parameter/name/" ,"name",	 71, 41,0	},
    {	"/effect_key/parameter/colorspace/" ,"colorspace",	 73, 41,0	},
    {	"/effect_key/parameter/flags/" ,"flags",	 74, 41,0	},
    {	"/effect_key/parameter/min/" ,"min",	 75, 41,0	},
    {	"/effect_key/parameter/max/" ,"max",	 76, 41,0	},
    {	"/effect_key/parameter/default/" ,"default",	 77, 41,0	},
    {	"/effect_key/parameter/group/" ,"group",	 78, 41,0	},
    {	"/effect_key/nparameter/" , 	"nparameter",	 91, 25,0	},
    {	"/effect_key/nparameter/name/" ,"name",	 72, 91,0	},
    {	"/effect_key/nparameter/value/" ,"value",	 92, 91,0	},
    {	"/effect_key/nparameter/type/" ,"type",	 116, 91,0	},
    {	"/effect_key/nparameter/min/" ,"min",	 93, 91,0	},
    {	"/effect_key/nparameter/max/" ,"max",	 94, 91,0	},
    {	"/effect_key/nparameter/default/" ,"default",	 95, 91,0	},
    {	"/effect_key/map/" , 		"map",	 32, 25,0	},
    {	"/effect_key/mode/" , 		"mode",	 43, 25,0	},
    {	"/effect_key/name/" , 		"name",	 44, 25,0	},
    {	"/effect_key/maxmode/" , 	"maxmode",	 45, 25,0	},
    {	"/effect_key/state/" , 	"state",	 56, 25,0	},
    {	"/effect_key/outchannel/" , 	"outchannel",	 160, 25,0	},
    {	"/effect_key/outchannel/connection/" , 	"connection",	 161, 160,0	},
    {	"/effect_key/outchannel/palette/" , 	"palette",	 162, 160,0	},
    {	"/effect_key/outchannel/active/" , 	"active",	 171, 160,0	},
    {	"/effect_key/outparameter/" , 	"outparameter",	 150, 25,0	},
    {	"/effect_key/outparameter/connection/" , 	"connection",	 151, 150,0	},
    {	"/effect_key/outparameter/name/" , 	"name",	 152, 150,0	},
    {	"/effect_key/outparameter/type/" , 	"type",	 153, 150,0	},
    {	"/effect_key/outparameter/colorspace/" , 	"colorspace",	 154, 150,0	},
    {	"/effect_key/outparameter/value/" , 	"value",	 155, 150,0	},
    {	"/effect_key/outparameter/min/" , 	"min",	 156, 150,0	},
    {	"/effect_key/outparameter/max/" , 	"max",	 157, 150,0	},
    {	"/effect_key/outparameter/default/" , 	"default",	 158, 150,0	},
    {	"/lives/" , 		"lives",	 21, -1,0	},
    {	"/lives/version/" , 		"version",	 24, 21,0	},
    {	"/lives/mode/" , 		"mode",	 103, 21,0	},
    {	"/lives/constant/" , 		"constant",	 120, 21,0	},
    {	"/lives/constant/value/" , 		"value",	 121, 120,0	},
    {	"/clipset/" , 		"clipset",	 35, -1,0	},
    {	"/app/" , 		"app",	         22, -1,0	},
    {	"/app/name/" , 		"name",	         23, 22,0	},
    {	"/app/version/" , 		"version",	         125, 22,0	},
    {	"/output/" , 	"output",	 27, -1,0	},
    {	"/output/fullscreen/" , 	"fullscreen",	 28, 27,0	},
    {	"/output/fps/" , 	        "fps",	 52, 27,0	},
    {	"/output/nodrop/" , 	"nodrop",	 30, 27 ,0	},
    {	"/clip/open/",   		"open",		 33, 1,0	},
    {	"/notify/",   		"notify",		 100, -1,0	},
    {	"/notify/confirmations/",   		"confirmations",		 101, 100,0	},
    {	"/notify/events/",   		"events",		 102, 100,0	},
    {	"/layout/",   		"layout",		 104, -1,0	},
    {	"/block/",   		"block",		 105, -1,0	},
    {	"/block/start/",   		"start",		 106, 105,0	},
    {	"/block/start/time/",   		"time",		 111, 106,0	},
    {	"/block/end/",   		"end",		 107, 105,0	},
    {	"/block/end/time/",   		"time",		 112, 107,0	},
    {	"/mt/",   		"mt",		 200, -1,0	},
    {	"/mt/ctime/",   		"ctime",		 201, 200,0	},
    {	"/mt/ctrack/",   		"ctrack",		 202, 200,0	},
    {	"/test/",   		"test",		 500, -1,0	},
    {	NULL,			NULL,		0, -1,0		},
  };


int lives_osc_build_cont( lives_osc *o )
{ 
  /* Create containers /video , /clip, /chain and /tag */
  int i;
  for( i = 0; osc_cont[i].name != NULL ; i ++ )
    {
      if ( osc_cont[i].it == 0 )
	{
	  o->cqinfo.comment = osc_cont[i].comment;
	  
	  // add a container to a leaf
	  if ( ( o->leaves[ osc_cont[i].leave ] =
		 OSCNewContainer( osc_cont[i].name,
				  (osc_cont[i].att == -1 ? o->container : o->leaves[ osc_cont[i].att ] ),
				  &(o->cqinfo) ) ) == 0 )
	    {
	      if(osc_cont[i].att == - 1)
		{
		  g_printerr( "Cannot create container %d (%s) \n",
			      i, osc_cont[i].name );
		  return 0;
		}
	      else
		{
		  g_printerr( "Cannot add branch %s to  container %d)\n",
			      osc_cont[i].name, osc_cont[i].att );  
		  return 0;
		}
	    }
	}
      else
	{
	  int n = osc_cont[i].it;
	  int j;
	  int base = osc_cont[i].leave;
	  char name[50];
	  char comment[50];
	  
	  for ( j = 0; j < n ; j ++ )
	    {
	      sprintf(name, "N%d", j);	
	      sprintf(comment, "<%d>", j);
	      g_printerr( "Try cont.%d  '%s', %d %d\n",j, name,
			  base + j, base );	
	      o->cqinfo.comment = comment;
	      if ((	o->leaves[ base + j ] = OSCNewContainer( name,
								 o->leaves[ osc_cont[i].att ] ,
								 &(o->cqinfo) ) ) == 0 )
		{
		  g_printerr( "Cannot auto numerate container %s \n",
			      osc_cont[i].name );
		  return 0;
		  
		}
	    }
	}
    }
  return 1;
}


int lives_osc_attach_methods( lives_osc *o ) {
  int i;

  for( i = 0; osc_methods[i].name != NULL ; i ++ ) {
    o->ris.description = osc_methods[i].descr;
    OSCNewMethod( osc_methods[i].name, 
		  o->leaves[ osc_methods[i].leave ],
		  osc_methods[i].cb , 
		  NULL, // this is the context which is reurned but it seems to be unused
		  &(o->ris));
    
    
  }
  return 1;
}	





/* initialization, setup a UDP socket and invoke OSC magic */
lives_osc* lives_osc_allocate(int port_id) {
  lives_osc *o;
  char *tmp;

  if (livesOSC==NULL) {
    o = (lives_osc*)g_malloc(sizeof(lives_osc));
    //o->osc_args = (osc_arg*)g_malloc(50 * sizeof(*o->osc_args));
    o->osc_args=NULL;
    o->rt.InitTimeMemoryAllocator = _lives_osc_time_malloc;
    o->rt.RealTimeMemoryAllocator = _lives_osc_rt_malloc;
    o->rt.receiveBufferSize = 1024;
    o->rt.numReceiveBuffers = 100;
    o->rt.numQueuedObjects = 100;
    o->rt.numCallbackListNodes = 200;
    o->leaves = (OSCcontainer*) g_malloc(sizeof(OSCcontainer) * 1000);
    o->t.initNumContainers = 1000;
    o->t.initNumMethods = 2000;
    o->t.InitTimeMemoryAllocator = lives_size_malloc;
    o->t.RealTimeMemoryAllocator = lives_size_malloc;
    
    if(!OSCInitReceive( &(o->rt))) {
      d_print( _ ("Cannot initialize OSC receiver\n"));
      return NULL;
    } 
    o->packet = OSCAllocPacketBuffer();

    /* Top level container / */
    o->container = OSCInitAddressSpace(&(o->t));
    
    OSCInitContainerQueryResponseInfo( &(o->cqinfo) );
    o->cqinfo.comment = "Video commands";
    
    if( !lives_osc_build_cont( o ))
      return NULL;
    
    OSCInitMethodQueryResponseInfo( &(o->ris));
    
    
    if( !lives_osc_attach_methods( o ))
      return NULL;
  }
  else o=livesOSC;

  if (port_id>0) {
    if(NetworkStartUDPServer( o->packet, port_id) != TRUE) {
      tmp=g_strdup_printf (_ ("WARNING: Cannot start OSC server at UDP port %d\n"),port_id);
      d_print( tmp);
      g_free(tmp);
    }
    else {
      d_print( (tmp=g_strdup_printf (_ ("Started OSC server at UDP port %d\n"),port_id)));
      g_free(tmp);
    }
  }
  
  return o;
}	







void lives_osc_dump()
{
  OSCPrintWholeAddressSpace();
}

 


// CALL THIS PERIODICALLY, will read all queued messages and call callbacks


/* get a packet */
static int lives_osc_get_packet(lives_osc *o) {
  //OSCTimeTag tag;
  
  /* see if there is something to read , this is effectivly NetworkPacketWaiting */
  // if(ioctl( o->sockfd, FIONREAD, &bytes,0 ) == -1) return 0;
  // if(bytes==0) return 0;
  if(NetworkPacketWaiting(o->packet)) {
    /* yes, receive packet from UDP */
    if(NetworkReceivePacket(o->packet)) {
      /* OSC must accept this packet (OSC will auto-invoke it, see source !) */
      OSCAcceptPacket(o->packet);

#ifdef DEBUG_OSC
      g_print("got osc msg %s\n",OSCPacketBufferGetBuffer((OSCPacketBuffer)o->packet));
#endif
      /* Is this really productive ? */
      OSCBeProductiveWhileWaiting();
      // here we call the callbacks
      
      /* tell caller we had 1 packet */	
      return 1;
    }
  }
  return 0;
}


static void oscbuf_to_packet(OSCbuf *obuf, OSCPacketBuffer packet) {
  int *psize=OSCPacketBufferGetSize(packet);
  int bufsize=OSC_packetSize(obuf);

  if (bufsize>100) {
    LIVES_ERROR("error, OSC msglen > 100 !");
  }

  memcpy(OSCPacketBufferGetBuffer(packet),OSC_getPacket(obuf),bufsize);
  *psize=bufsize;
}




boolean lives_osc_act(OSCbuf *obuf) {
  // this is a shortcut route to make LiVES carry out the OSC message in msg

  OSCPacketBuffer packet;

  if (livesOSC==NULL) lives_osc_init(0);

  packet=livesOSC->packet;

  oscbuf_to_packet(obuf,packet);
  
  OSCAcceptPacket(packet);

  via_shortcut=TRUE;
  OSCBeProductiveWhileWaiting();
  via_shortcut=FALSE;

  return TRUE;
}





void lives_osc_free(lives_osc *c)
{
  if(c==NULL) return;
  if(c->leaves) free(c->leaves);
  if(c) free(c);
  c = NULL;
}




////////////////////////////// API public functions /////////////////////



boolean lives_osc_init(guint udp_port) {
  char *tmp;

  if (livesOSC!=NULL&&udp_port!=0) {
    /*  OSCPacketBuffer packet=livesOSC->packet;
	if (shutdown (packet->returnAddr->sockfd,SHUT_RDWR)) {
	d_print( g_strdup_printf (_ ("Cannot shut down OSC/UDP server\n"));
	}
    */
    if (NetworkStartUDPServer( livesOSC->packet, udp_port) != TRUE) {
      d_print( (tmp=g_strdup_printf (_ ("Cannot start OSC/UDP server at port %d \n"),udp_port)));
      g_free(tmp);
    }
  }
  else { 
    livesOSC=lives_osc_allocate(udp_port);
    if (livesOSC==NULL) return FALSE;
    status_socket=NULL;
    notify_socket=NULL;
  }
  return TRUE;
}


boolean lives_osc_poll(gpointer data) {
  // data is always NULL
  // must return TRUE
  if (!mainw->osc_block&&livesOSC!=NULL) lives_osc_get_packet(livesOSC);
  return TRUE;
}

void lives_osc_end(void) {
  if (notify_socket!=NULL) {
    lives_osc_notify (LIVES_OSC_NOTIFY_QUIT,"");
    lives_osc_close_notify_socket();
  }
  if (status_socket!=NULL) {
    lives_osc_close_status_socket();
  }

  if (livesOSC!=NULL) lives_osc_free(livesOSC);
  livesOSC=NULL;
}

#endif
