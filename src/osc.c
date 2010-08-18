// osc.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

#ifdef ENABLE_OSC
#include <netinet/in.h>

#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"

#include "osc.h"
#include "htmsocket.h"
#include "callbacks.h"
#include "effects.h"
#include "support.h"
#include "rte_window.h"


void *status_socket;
void *notify_socket;

static lives_osc *livesOSC=NULL;

#define OSC_STRING_SIZE 255

#define FX_MAX FX_KEYS_MAX_VIRTUAL-1



/* convert a big endian 32 bit string to an int for internal use */

static int toInt(const char* b) {
  if (G_BYTE_ORDER==G_LITTLE_ENDIAN) {
    return (( (int) b[3] ) & 0xff ) + ((((int) b[2]) & 0xff) << 8) + ((((int) b[1]) & 0xff) << 16) +
      ((((int) b[0] ) & 0xff) << 24);
  }
    return (( (int) b[0] ) & 0xff ) + ((((int) b[1]) & 0xff) << 8) + ((((int) b[2]) & 0xff) << 16) +
      ((((int) b[3] ) & 0xff) << 24);
}


// wrapper for correct typing
void *lives_malloc(int size) {
  return g_malloc(size);
}

static gboolean using_types;
static gint osc_header_len;
static gint offset;

inline gint pad4(gint val) {
  return (gint)((val+4)/4)*4;
}

static gboolean lives_osc_check_arguments(int arglen, const void *vargs, const gchar *check_pattern, gboolean calc_header_len) {
  // check if using type tags and get header_len
  // should be called from each cb that uses parameters
  const char *args=vargs;
  gint header_len;

  osc_header_len=0;
  offset=0;

  if (arglen<4||args[0] != 0x2c) return (!(using_types=FALSE)); // comma
  using_types=TRUE;

  header_len=pad4(strlen(check_pattern)+(args[0]==0x2c));

  if (arglen<header_len) return FALSE;
  if (!strncmp (check_pattern,++args,strlen (check_pattern))) {
    if (calc_header_len) osc_header_len=header_len;
    return TRUE;
  }
  return FALSE;
}


/* not used yet */
/*static void lives_osc_parse_char_argument(const void *vargs, gchar *dst)
{
  const char *args = (char*)vargs;
  strncpy(dst, args+osc_header_len+offset,1);
  offset+=4;
  }*/



static void lives_osc_parse_string_argument(const void *vargs, gchar *dst)
{
  const char *args = (char*)vargs;
  g_snprintf(dst, OSC_STRING_SIZE, "%s", args+osc_header_len+offset);
  offset+=pad4(strlen (dst));
}



static void lives_osc_parse_int_argument(const void *vargs, gint *arguments)
{
  const char *args = (char*)vargs;
  arguments[0] = toInt( args + osc_header_len + offset );
  offset+=4;
}

static void lives_osc_parse_float_argument(const void *vargs, gfloat *arguments)
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

gboolean lives_status_send (gchar *msg) {
  return lives_stream_out (status_socket,strlen (msg),msg);
}


gboolean lives_osc_notify (int msgnumber,const gchar *msgstring) {
  if (notify_socket==NULL) return FALSE;
  else {
    gchar *sep=((msgstring==NULL||strlen(msgstring)==0)?"":" ");
    gchar *msg=g_strdup_printf("%d%s%s\n",msgnumber,sep,msgstring);
    gboolean retval = lives_stream_out (notify_socket,strlen (msg),msg);
    g_free(msg);
    return retval;
  }
}



void lives_osc_close_status_socket (void) {
  if (status_socket!=NULL) CloseHTMSocket (status_socket);
  status_socket=NULL;
}

void lives_osc_close_notify_socket (void) {
  if (notify_socket!=NULL) CloseHTMSocket (notify_socket);
  notify_socket=NULL;
}



///////////////////////////////////// CALLBACKS FOR OSC ////////////////////////////////////////


/* /video/play */
void lives_osc_cb_play (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return;
  
  if (mainw->playing_file==-1&&mainw->current_file>0) on_playall_activate(NULL,NULL);

}

void lives_osc_cb_play_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->current_file<0||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return;
  dirchange_callback(NULL,NULL,0,0,GINT_TO_POINTER(TRUE));

}


void lives_osc_cb_bgplay_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (mainw->current_file<0||(mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)) return;

  mainw->files[mainw->blend_file]->pb_fps=-mainw->files[mainw->blend_file]->pb_fps;

}


void lives_osc_cb_play_forward (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return;

  if (mainw->current_file<0||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return;

  if (mainw->playing_file==-1&&mainw->current_file>0) {
    on_playall_activate(NULL,NULL);
  }
  else if (mainw->current_file>0) {
    if (cfile->pb_fps<0||(cfile->play_paused&&cfile->freeze_fps<0)) dirchange_callback(NULL,NULL,0,0,GINT_TO_POINTER(TRUE));
    if (cfile->play_paused) freeze_callback(NULL,NULL,0,0,NULL);
  }

}


void lives_osc_cb_play_backward (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return;

  if (mainw->current_file<0||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return;

  if (mainw->playing_file==-1&&mainw->current_file>0) {
    mainw->reverse_pb=TRUE;
    on_playall_activate(NULL,NULL);
  }
  else if (mainw->current_file>0) {
    if (cfile->pb_fps>0||(cfile->play_paused&&cfile->freeze_fps>0)) dirchange_callback(NULL,NULL,0,0,GINT_TO_POINTER(TRUE));
    if (cfile->play_paused) freeze_callback(NULL,NULL,0,0,NULL);
  }

}


void lives_osc_cb_play_faster (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  on_faster_pressed(NULL,GINT_TO_POINTER(1));

}


void lives_osc_cb_bgplay_faster (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  on_faster_pressed(NULL,GINT_TO_POINTER(2));

}


void lives_osc_cb_play_slower (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  on_slower_pressed(NULL,GINT_TO_POINTER(1));

}


void lives_osc_cb_bgplay_slower (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  on_slower_pressed(NULL,GINT_TO_POINTER(2));

}



void lives_osc_cb_play_reset (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  fps_reset_callback(NULL,NULL,0,0,NULL);
  if (cfile->pb_fps<0||(cfile->play_paused&&cfile->freeze_fps<0)) dirchange_callback(NULL,NULL,0,0,GINT_TO_POINTER(TRUE));
  if (cfile->play_paused) freeze_callback(NULL,NULL,0,0,NULL);


}


void lives_osc_cb_bgplay_reset (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file==-1) return;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (mainw->files[mainw->blend_file]->play_paused) {
    mainw->files[mainw->blend_file]->play_paused=FALSE;
  }

  if (mainw->files[mainw->blend_file]->pb_fps>=0.) mainw->files[mainw->blend_file]->pb_fps=mainw->files[mainw->blend_file]->fps;
  else mainw->files[mainw->blend_file]->pb_fps=-mainw->files[mainw->blend_file]->fps;

}




/* /video/stop */
void lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file>-1) on_stop_activate(NULL,NULL);
}


void lives_osc_cb_set_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gint fpsi;
  gfloat fps;
  if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return;
    lives_osc_parse_float_argument(vargs,&fps);
  }

  if (mainw->playing_file>-1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),(gdouble)(fps));

}


void lives_osc_cb_bgset_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gint fpsi;
  gfloat fps;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return;
    lives_osc_parse_float_argument(vargs,&fps);
  }

  mainw->files[mainw->blend_file]->pb_fps=(gdouble)fps;

}



void lives_osc_cb_set_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gint fpsi;
  gfloat fps;
  if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return;
    lives_osc_parse_float_argument(vargs,&fps);
  }

  if (mainw->playing_file>-1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),(gdouble)(fps)*mainw->files[mainw->playing_file]->fps);

}


void lives_osc_cb_bgset_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gint fpsi;
  gfloat fps;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    lives_osc_parse_int_argument(vargs,&fpsi);
    fps=(float)fpsi;
  }
  else {
    if (!lives_osc_check_arguments (arglen,vargs,"f",TRUE)) return;
    lives_osc_parse_float_argument(vargs,&fps);
  }

  mainw->files[mainw->blend_file]->pb_fps=mainw->files[mainw->blend_file]->fps*(gdouble)fps;

}






void lives_osc_cb_fx_reset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (!mainw->osc_block) rte_on_off_callback(NULL,NULL,0,0,GINT_TO_POINTER(0));

}

void lives_osc_cb_fx_map_clear(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->osc_block) on_clear_all_clicked(NULL,NULL);
}

void lives_osc_cb_fx_map(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  gchar effect_name[OSC_STRING_SIZE];

  if (!lives_osc_check_arguments (arglen,vargs,"is",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_string_argument(vargs,effect_name);
  if (!mainw->osc_block) weed_add_effectkey(effect_key,effect_name,FALSE); // allow partial matches
}

void lives_osc_cb_fx_enable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  gint grab=mainw->last_grabable_effect;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (!(mainw->rte&(GU641<<(effect_key-1)))) {
    if (!mainw->osc_block) rte_on_off_callback(NULL,NULL,0,0,GINT_TO_POINTER(effect_key));
  }
  mainw->last_grabable_effect=grab;
}

void lives_osc_cb_fx_disable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (mainw->rte&(GU641<<(effect_key-1))) {
    if (!mainw->osc_block) rte_on_off_callback(NULL,NULL,0,0,GINT_TO_POINTER(effect_key));
  }
}


void lives_osc_cb_fx_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (!mainw->osc_block) rte_on_off_callback(NULL,NULL,0,0,GINT_TO_POINTER(effect_key));
}

// *_set will allow setting of invalid clips - in this case nothing happens
//*_select will index only valid clips


void lives_osc_cb_fgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip

  int clip;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES-1) {
    if (mainw->files[clip]!=NULL&&(mainw->files[clip]->clip_type==CLIP_TYPE_DISK||mainw->files[clip]->clip_type==CLIP_TYPE_FILE)) {
      if (mainw->playing_file>0) {
	mainw->pre_src_file=clip;
	mainw->new_clip=clip;
      }
      else if (mainw->playing_file==-1) {
	switch_to_file(mainw->current_file,clip);
      }
    }
  }
}


void lives_osc_cb_bgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip

  int clip;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES-1) {
    if (mainw->files[clip]!=NULL&&(mainw->files[clip]->clip_type==CLIP_TYPE_DISK||mainw->files[clip]->clip_type==CLIP_TYPE_FILE)) {
      mainw->blend_file=clip;
    }
  }
}


void lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip
  int clip,i;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip<1||mainw->cliplist==NULL) return;

  if (mainw->scrap_file!=-1&&clip>=mainw->scrap_file) clip++;

  if (clip>g_list_length(mainw->cliplist)) return;

  i=GPOINTER_TO_INT(g_list_nth_data(mainw->cliplist,clip-1));
    
  if (i==mainw->current_file) return;
  if (mainw->playing_file>0) {
    mainw->pre_src_file=i;
    mainw->new_clip=i;
  }
  else if (mainw->playing_file==-1) {
    switch_to_file(mainw->current_file,i);
  }
}




void lives_osc_cb_bgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch bg clip
  int clip,i;
  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip<1||mainw->cliplist==NULL) return;

  if (mainw->scrap_file!=-1&&clip>=mainw->scrap_file) clip++;

  if (clip>g_list_length(mainw->cliplist)) return;

  if (mainw->num_tr_applied<1) return;

  i=GPOINTER_TO_INT(g_list_nth_data(mainw->cliplist,clip-1));
    
  if (i==mainw->blend_file) return;

  mainw->blend_file=i;

}




void lives_osc_cb_fgclip_close(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio=0;
  int clipno=mainw->current_file;
  gint current_file=clipno;

  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL) return;

  if (clipno==current_file) current_file=-1;

  mainw->current_file=clipno;

  close_current_file(current_file);

}





void lives_osc_cb_fgclip_copy(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio=0;
  int clipno=mainw->current_file;
  gint start,end,current_file=clipno;
  gboolean ccpd;

  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

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

}



void lives_osc_cb_fgclipsel_copy(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  gint current_file=clipno;
  gboolean ccpd;

  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_copy_activate(NULL,NULL);

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

}





void lives_osc_cb_fgclipsel_cut(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  gint current_file=clipno;
  gboolean ccpd;

  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_cut_activate(GINT_TO_POINTER(1),NULL);

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

}




void lives_osc_cb_fgclipsel_delete(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int clipno=mainw->current_file;
  gint current_file=clipno;
  gboolean ccpd;

  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

  mainw->current_file=clipno;

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_delete_activate(GINT_TO_POINTER(1),NULL);

  mainw->ccpd_with_sound=ccpd;

  mainw->current_file=current_file;

}




void lives_osc_cb_clipbd_paste(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio=0;
  gboolean ccpd;

  if (mainw->playing_file>-1) return;

  if (clipboard==NULL) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

  if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&noaudio);
  }
  else if (!lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
    return;
  }

  ccpd=mainw->ccpd_with_sound;

  mainw->ccpd_with_sound=!noaudio;

  on_paste_as_new_activate(NULL,NULL);

  mainw->ccpd_with_sound=ccpd;

}




void lives_osc_cb_clipbd_insertb(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int times=1;
  int clipno=mainw->current_file;
  gint current_file=clipno;


  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

  if (times==0||times<-1) return;

  mainw->current_file=clipno;

  mainw->insert_after=FALSE;

  if (clipboard->achans==0&&cfile->achans==0) noaudio=TRUE;

  mainw->fx1_bool=(times==-1); // fit to audio
  mainw->fx1_val=times;       // times to insert otherwise
  mainw->fx2_bool=!noaudio;  // with audio

  on_insert_activate(NULL,NULL);

  mainw->current_file=current_file;

}





void lives_osc_cb_clipbd_inserta(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {

  int noaudio=0;
  int times=1;
  int clipno=mainw->current_file;
  gint current_file=clipno;


  if (mainw->playing_file>-1) return;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

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
    return;
  }

  if (clipno<1||clipno>MAX_FILES||mainw->files[clipno]==NULL||(mainw->files[clipno]->clip_type!=CLIP_TYPE_DISK&&mainw->files[clipno]->clip_type!=CLIP_TYPE_FILE)) return;

  if (times==0||times<-1) return;

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

}




void lives_osc_cb_fgclip_retrigger (void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip and reset framenumber

  if (mainw->playing_file<1||mainw->preview||mainw->is_processing) return;
  if (!lives_osc_check_arguments (arglen,vargs,"i",FALSE)) return;

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

  if (mainw->playing_file<1||mainw->preview||mainw->is_processing) return;
  if (!lives_osc_check_arguments (arglen,vargs,"i",FALSE)) return;

  lives_osc_cb_bgclip_select(context,arglen,vargs,when,ra);

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (mainw->files[mainw->blend_file]->pb_fps>0.||(mainw->files[mainw->blend_file]->play_paused&&mainw->files[mainw->blend_file]->freeze_fps>0.)) mainw->files[mainw->blend_file]->frameno=mainw->files[mainw->blend_file]->last_frameno=1;
  else mainw->files[mainw->blend_file]->frameno=mainw->files[mainw->blend_file]->last_frameno=mainw->files[mainw->blend_file]->frames;
}




void lives_osc_cb_fgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  nextclip_callback(NULL,NULL,0,0,GINT_TO_POINTER(1));

}


void lives_osc_cb_bgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO
  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL) return;

  nextclip_callback(NULL,NULL,0,0,GINT_TO_POINTER(2));

}



void lives_osc_cb_fgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO
  
  prevclip_callback(NULL,NULL,0,0,GINT_TO_POINTER(1));

}


void lives_osc_cb_bgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return; // TODO

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL) return;

  prevclip_callback(NULL,NULL,0,0,GINT_TO_POINTER(2));

}







void lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->playing_file>-1) return;

  mainw->only_close=mainw->no_exit=FALSE;
  mainw->leave_recovery=FALSE;

  if (mainw->was_set) {
    on_save_set_activate(NULL,mainw->set_name);
  }
  else mainw->leave_files=FALSE;
  lives_exit();

}


void lives_osc_cb_open_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments (arglen,vargs,"si",TRUE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
    g_snprintf (host,OSC_STRING_SIZE,"localhost");
  }
  else lives_osc_parse_string_argument(vargs,host);
  lives_osc_parse_int_argument(vargs,&port);

  if (notify_socket!=NULL) {
    g_printerr("Status socket already opened !\n");
    return;
  }

  if (!(status_socket=OpenHTMSocket (host,port,TRUE))) g_printerr ("Unable to open status socket !\n");

}

void lives_osc_cb_open_notify_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments (arglen,vargs,"si",TRUE)) {
    if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
    g_snprintf (host,OSC_STRING_SIZE,"localhost");
  }
  else lives_osc_parse_string_argument(vargs,host);
  lives_osc_parse_int_argument(vargs,&port);

  if (notify_socket!=NULL) {
    g_printerr("Notify socket already opened !\n");
    return;
  }

  if (!(notify_socket=OpenHTMSocket (host,port,TRUE))) g_printerr ("Unable to open notify socket !\n");

}

void lives_osc_cb_close_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  lives_osc_close_status_socket();
}






void lives_osc_cb_clip_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar *tmp;

  if (status_socket==NULL) return;
  lives_status_send ((tmp=g_strdup_printf ("%d\n",mainw->clips_available)));
  g_free(tmp);

}

void lives_osc_cb_clip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int frame;
  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) return;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&frame);

  if (frame<1||frame>cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) return;

  cfile->last_frameno=cfile->frameno=frame;

#ifdef RT_AUDIO
  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(frame);
  }
#endif
}


void lives_osc_cb_bgclip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int frame;
  if (mainw->current_file<1||mainw->preview||mainw->playing_file<1) return;

  if (mainw->blend_file<1||mainw->files[mainw->blend_file]==NULL||mainw->blend_file==mainw->current_file) return;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&frame);

  if (frame<1||frame>mainw->files[mainw->blend_file]->frames||(mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)) return;

  mainw->files[mainw->blend_file]->last_frameno=mainw->files[mainw->blend_file]->frameno=frame;

}


void lives_osc_cb_clip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar *tmp;

  if (status_socket==NULL) return;
  lives_status_send ((tmp=g_strdup_printf ("%d\n",mainw->current_file<0?0:mainw->current_file)));
  g_free(tmp);

}


void lives_osc_cb_bgclip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar *tmp;

  if (status_socket==NULL) return;
  lives_status_send ((tmp=g_strdup_printf ("%d\n",mainw->blend_file<0?0:mainw->blend_file)));
  g_free(tmp);

}



void lives_osc_cb_clip_set_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  int frame;

  file *sfile;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
  }
  else return;

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return;

  sfile=mainw->files[clip];

  if (frame<1||(sfile->clip_type!=CLIP_TYPE_DISK&&sfile->clip_type!=CLIP_TYPE_FILE)) return;

  if (frame>sfile->frames) frame=sfile->frames;

  if (clip==mainw->current_file) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),frame);
  else sfile->start=frame;
}

void lives_osc_cb_clip_set_end(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file=mainw->current_file;
  int clip=current_file;
  int frame;

  file *sfile;

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;

  if (lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"ii",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
    lives_osc_parse_int_argument(vargs,&clip);
  }
  else if (lives_osc_check_arguments (arglen,vargs,"i",FALSE)) { 
    lives_osc_check_arguments (arglen,vargs,"i",TRUE);
    lives_osc_parse_int_argument(vargs,&frame);
  }
  else return;

  if (clip<1||clip>MAX_FILES||mainw->files[clip]==NULL) return;

  sfile=mainw->files[clip];

  if (frame<1||(sfile->clip_type!=CLIP_TYPE_DISK&&sfile->clip_type!=CLIP_TYPE_FILE)) return;

  if (frame>sfile->frames) frame=sfile->frames;

  if (clip==mainw->current_file) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),frame);
  else sfile->end=frame;

}


void lives_osc_cb_clip_select_all(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->current_file<1||mainw->preview||mainw->is_processing) return;
  if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||!cfile->frames) return;

  on_select_all_activate (NULL,NULL);
}

void lives_osc_cb_clip_isvalid(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int clip;
  if (status_socket==NULL) return;

  lives_osc_check_arguments (arglen,vargs,"i",TRUE);
  lives_osc_parse_int_argument(vargs,&clip);

  if (clip>0&&clip<MAX_FILES&&mainw->files[clip]!=NULL) lives_status_send ("1\n");
  else lives_status_send ("0\n");

}

void lives_osc_cb_rte_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects - (only those assigned to keys for now)
  gchar *tmp;
  if (status_socket==NULL) return;

  lives_status_send ((tmp=g_strdup_printf ("%d\n",prefs->rte_keys_virtual)));
  g_free(tmp);

}

void lives_osc_cb_rteuser_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects
  gchar *tmp;
  if (status_socket==NULL) return;

  lives_status_send ((tmp=g_strdup_printf ("%d\n",FX_MAX)));
  g_free(tmp);

}


void lives_osc_cb_fssepwin_enable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->sep_win) {
    on_sepwin_pressed (NULL,NULL);
  }

  if (!mainw->fs) {
    on_full_screen_pressed (NULL,NULL);
  }
}


void lives_osc_cb_fssepwin_disable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (mainw->fs) {
      on_full_screen_pressed (NULL,NULL);
  }
  if (mainw->sep_win) {
    on_sepwin_pressed (NULL,NULL);
  }
}

void lives_osc_cb_op_fps_set(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  // TODO - make float, show messages
  int fps;
  gdouble fpsd;
  gchar *tmp;

  if (mainw->fixed_fpsd>0.) return;
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&fps);
  fpsd=(gdouble)(fps*1.);
  if (fps>0&&fpsd<=FPS_MAX) {
    mainw->fixed_fpsd=fpsd;
    d_print ((tmp=g_strdup_printf (_("Syncing to external framerate of %.8f frames per second.\n"),fpsd)));
    g_free(tmp);
  }
  else if (fps==0) mainw->fixed_fpsd=-1.;

}


void lives_osc_cb_freeze(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file<1) return;

  if (!mainw->osc_block) {
    freeze_callback(NULL,NULL,0,0,NULL);
  }

}

void lives_osc_cb_op_nodrope(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  
  mainw->noframedrop=TRUE;

}


void lives_osc_cb_op_nodropd(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  
  mainw->noframedrop=FALSE;

}


void lives_osc_cb_clip_encodeas(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char fname[OSC_STRING_SIZE];


  if (mainw->playing_file>-1||mainw->current_file<1) return;

  if (!lives_osc_check_arguments (arglen,vargs,"sii",FALSE)) { 
    if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) 
      return;
    lives_osc_parse_string_argument(vargs,fname);
    mainw->osc_enc_width=mainw->osc_enc_height=0;
  }
  else {
    lives_osc_check_arguments (arglen,vargs,"sii",TRUE);
    lives_osc_parse_string_argument(vargs,fname);
    lives_osc_parse_int_argument(vargs,&mainw->osc_enc_width);
    lives_osc_parse_int_argument(vargs,&mainw->osc_enc_height);
  }

  if (cfile->frames==0) {
    // TODO
    on_export_audio_activate (NULL,NULL);
    return;
  }

  mainw->save_all=TRUE;

  mainw->osc_auto=TRUE;
  save_file(TRUE,fname);
  mainw->osc_auto=FALSE;

}



void lives_osc_cb_rte_setmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&mode);
  if (effect_key<1||effect_key>FX_MAX||mode<1||mode>rte_getmodespk()) return;
  if (!mainw->osc_block) rte_key_setmode (effect_key,mode-1);

}



void lives_osc_cb_rte_nextmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (effect_key<1||effect_key>FX_MAX) return;
  if (!mainw->osc_block) rte_key_setmode (effect_key,-1);

}


void lives_osc_cb_rte_prevmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  if (effect_key<1||effect_key>FX_MAX) return;
  if (!mainw->osc_block) rte_key_setmode (effect_key,-2);

}


///////////////////////////////////////////////////////////////


static void setfx (gint effect_key, gint pnum, int arglen, const void *vargs) {
  int valuei;
  float valuef;
  int error;
  weed_plant_t *inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));
  weed_plant_t **in_params;
  weed_plant_t *tparam;
  weed_plant_t *tparamtmpl;
  int hint,cspace;
  int nparams;
  int valuesi[4];
  float valuesf[4];
  double valuesd[4];
  
  if (!weed_plant_has_leaf(inst,"in_parameters")) return;
  nparams=weed_leaf_num_elements(inst,"in_parameters");
  if (pnum>=nparams) return;
  
  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  
  tparam=in_params[pnum];
  tparamtmpl=weed_get_plantptr_value(tparam,"template",&error);
  hint=weed_get_int_value(tparamtmpl,"hint",&error);
  
  switch (hint) {
  case WEED_HINT_INTEGER:
    if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) {
      if (!lives_osc_check_arguments (arglen,vargs,"iif",FALSE)) return;
      // we wanted an int but we got a float
      //so we will round to the nearest value
      lives_osc_check_arguments (arglen,vargs,"iif",TRUE);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_float_argument(vargs,&valuef);
      valuei=myround(valuef);

    }
    else {
      lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
    }

    if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
      rec_param_change(inst,pnum);
    }

    weed_set_int_value(tparam,"value",valuei);

    if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
      rec_param_change(inst,pnum);
    }



    break;
    
  case WEED_HINT_FLOAT:
    if (!lives_osc_check_arguments (arglen,vargs,"iif",FALSE)) {
      if (!lives_osc_check_arguments (arglen,vargs,"iii",FALSE)) return;
      // we wanted a float but we got an int, we can convert
      lives_osc_check_arguments (arglen,vargs,"iii",TRUE);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
      valuef=(gdouble)valuei;
    }
    else {
      lives_osc_check_arguments (arglen,vargs,"iif",TRUE);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_int_argument(vargs,&valuei);
      lives_osc_parse_float_argument(vargs,&valuef);
    }

    if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
      rec_param_change(inst,pnum);
    }

    weed_set_double_value(tparam,"value",(double)valuef);

    if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
      rec_param_change(inst,pnum);
    }

    break;
    
  case WEED_HINT_COLOR:
    cspace=weed_get_int_value(tparamtmpl,"colorspace",&error);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (weed_leaf_seed_type(tparamtmpl,"default")==WEED_SEED_INT) {
	if (!lives_osc_check_arguments (arglen,vargs,"iiiii",FALSE)) {
	  if (!lives_osc_check_arguments (arglen,vargs,"iifff",FALSE)) return;
	  // we wanted ints but we got floats
	  lives_osc_check_arguments (arglen,vargs,"iifff",TRUE);
	  lives_osc_parse_int_argument(vargs,&valuei);
	  lives_osc_parse_int_argument(vargs,&valuei);
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuesi[0]=myround(valuef);
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuesi[1]=myround(valuef);
	  lives_osc_parse_float_argument(vargs,&valuef);
	  valuesi[2]=myround(valuef);
	}
	else { 
	  lives_osc_check_arguments (arglen,vargs,"iiiii",TRUE);
	  lives_osc_parse_int_argument(vargs,&valuei);
	  lives_osc_parse_int_argument(vargs,&valuei);
	  lives_osc_parse_int_argument(vargs,&valuesi[0]);
	  lives_osc_parse_int_argument(vargs,&valuesi[1]);
	  lives_osc_parse_int_argument(vargs,&valuesi[2]);
	} 
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	}
	
	
	weed_set_int_array(tparam,"value",3,valuesi);
	
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	}
	
      }
      else {
	if (!lives_osc_check_arguments (arglen,vargs,"iifff",TRUE)) return;
	lives_osc_parse_int_argument(vargs,&valuei);
	lives_osc_parse_int_argument(vargs,&valuei);
	lives_osc_parse_float_argument(vargs,&valuesf[0]);
	lives_osc_parse_float_argument(vargs,&valuesf[1]);
	lives_osc_parse_float_argument(vargs,&valuesf[2]);
	valuesd[0]=(double)valuesf[0];
	valuesd[1]=(double)valuesf[1];
	valuesd[2]=(double)valuesf[2];

	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	}

	weed_set_double_array(tparam,"value",3,valuesd);
	
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	}

      }
      break;
    default:
      // TODO
      return;
    }
  default:
    // TODO
    return;
  }

  if (fx_dialog[1]!=NULL) {
    lives_rfx_t *rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
    if (!rfx->is_template) {
      gint keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
      gint modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
      if (keyw==effect_key&&modew==rte_key_getmode(effect_key))
	update_visual_params(rfx,FALSE);
    }
  }
  weed_free(in_params);
}






void lives_osc_cb_rte_setparam(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) return;
  osc_header_len=8; // hdr len might be longer for arrays

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  if (!mainw->osc_block) setfx(effect_key,pnum,arglen,vargs);

}



void lives_osc_cb_rte_setnparam(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *inst;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",FALSE)) return;
  osc_header_len=8; // hdr len might be longer for arrays

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  inst=rte_keymode_get_instance(effect_key,rte_key_getmode(effect_key));

  i=get_nth_simple_param(inst,pnum);

  if (i!=-1 && !mainw->osc_block) setfx(effect_key,i,arglen,vargs);

}



void lives_osc_cb_rte_nparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int count;

  // return number of numeric single valued, non-reinit
  // i.e. simple numeric parameters

  weed_plant_t *filter;

  gchar *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;

  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));

  count=count_simple_params(filter);

  msg=g_strdup_printf("%d\n",count);
  lives_status_send(msg);
  g_free(msg);
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

  gchar *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return;

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    vali=weed_get_int_value(ptmpl,"minimum",&error);
    msg=g_strdup_printf("%d\n",vali);
  }
  else {
    vald=weed_get_double_value(ptmpl,"minimum",&error);
    msg=g_strdup_printf("%.8f\n",vald);
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

  gchar *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));
 
  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return;

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    vali=weed_get_int_value(ptmpl,"maximum",&error);
    msg=g_strdup_printf("%d\n",vali);
  }
  else {
    vald=weed_get_double_value(ptmpl,"maximum",&error);
    msg=g_strdup_printf("%.8f\n",vald);
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

  gchar *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));

  i=get_nth_simple_param(filter,pnum);

  if (i==-1) return;

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  ptmpl=in_ptmpls[i];
  hint=weed_get_int_value(ptmpl,"hint",&error);

  if (hint==WEED_HINT_INTEGER) {
    if (!weed_plant_has_leaf(ptmpl,"host_default")) vali=weed_get_int_value(ptmpl,"default",&error);
    else vali=weed_get_int_value(ptmpl,"host_default",&error);
    msg=g_strdup_printf("%d\n",vali);
  }
  else {
    if (!weed_plant_has_leaf(ptmpl,"host_default")) vald=weed_get_double_value(ptmpl,"default",&error);
    else vald=weed_get_double_value(ptmpl,"host_default",&error);
    msg=g_strdup_printf("%.8f\n",vald);
  }

  lives_status_send(msg);
  g_free(msg);
  weed_free(in_ptmpls);
}


void lives_osc_cb_rte_getnparamtrans(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  int pnum,i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  int error;
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int hint,flags;
  int nparams;

  gboolean res=FALSE;

  gchar *msg;
  
  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;

  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&pnum);

  if (effect_key<1||effect_key>FX_MAX) return;
  //g_print("key %d pnum %d\n",effect_key,pnum);

  filter=rte_keymode_get_filter(effect_key,rte_key_getmode(effect_key));

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return;
  nparams=weed_leaf_num_elements(filter,"in_parameter_templates");
  if (pnum>=nparams) return;
  
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  
  for (i=0;i<nparams;i++) {
    ptmpl=in_ptmpls[i];
    hint=weed_get_int_value(ptmpl,"hint",&error);
    flags=weed_get_int_value(ptmpl,"flags",&error);
    if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(ptmpl,"default")==1&&
	!is_hidden_param(filter,i)) { // c.f effects-weed.c, get_nth_simple_param()
      if (pnum==0) {
	if (weed_plant_has_leaf(ptmpl,"transition")&&weed_get_boolean_value(ptmpl,"transition",&error)==WEED_TRUE) res=TRUE;
	msg=g_strdup_printf("%d\n",res);
	lives_status_send(msg);
	g_free(msg);
	weed_free(in_ptmpls);
	return;
      }
      pnum--;
    }
  }
}


void lives_osc_cb_rte_getmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar *tmp;

  int effect_key;
  if (status_socket==NULL) return;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key<1||effect_key>FX_MAX) {
    lives_status_send ("0\n");
    return;
  }

  lives_status_send ((tmp=g_strdup_printf ("%d\n",rte_key_getmode (effect_key))));
  g_free(tmp);

}



void lives_osc_cb_rte_get_keyfxname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  gchar *tmp;

  if (status_socket==NULL) return;

  if (!lives_osc_check_arguments (arglen,vargs,"ii",TRUE)) return;
  lives_osc_parse_int_argument(vargs,&effect_key);
  lives_osc_parse_int_argument(vargs,&mode);
  if (effect_key<1||effect_key>FX_MAX||mode<1||mode>rte_getmodespk()) return;
  lives_status_send ((tmp=g_strdup_printf ("%s\n",rte_keymode_get_filter_name (effect_key,mode-1))));
  g_free(tmp);
}


void lives_osc_cb_rte_getmodespk(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  int effect_key;
  gchar *tmp;

  if (status_socket==NULL) return;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    if (lives_osc_check_arguments (arglen,vargs,"",TRUE)) {
      lives_status_send ((tmp=g_strdup_printf ("%d\n",rte_getmodespk ())));
      g_free(tmp);
    }
    return;
  }

  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key>FX_MAX||effect_key<1) {
    lives_status_send ("0\n");
    return;
  }

  lives_status_send ((tmp=g_strdup_printf ("%d\n",rte_getmodespk ())));
  g_free(tmp);

}



// deprecated - do not use
void lives_osc_cb_rte_getusermode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int nmode;
  int effect_key;
  gchar *tmp;

  if (status_socket==NULL) return;

  if (!lives_osc_check_arguments (arglen,vargs,"i",TRUE)) {
    return;
  }

  lives_osc_parse_int_argument(vargs,&effect_key);

  if (effect_key>FX_MAX||effect_key<1) {
    lives_status_send ("0\n");
    return;
  }

  nmode=rte_key_getmaxmode(effect_key);

  lives_status_send ((tmp=g_strdup_printf ("%d\n",nmode)));
  g_free(tmp);

}


void lives_osc_cb_swap(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
 swap_fg_bg_callback (NULL,NULL,0,0,NULL);
}


void lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
 record_toggle_callback (NULL,NULL,0,0,GINT_TO_POINTER((gint)TRUE));
}


void lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
 record_toggle_callback (NULL,NULL,0,0,GINT_TO_POINTER((gint)FALSE));
}

void lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  record_toggle_callback (NULL,NULL,0,0,GINT_TO_POINTER(!mainw->record));
}


void lives_osc_cb_ping(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {

  if (status_socket==NULL) return;
  lives_status_send ("pong\n");
}


void lives_osc_cb_open_file(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar filename[OSC_STRING_SIZE];
  int startframe;
  int numframes;

  int type=0;

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return;

  if (mainw->playing_file>-1) return;


  if (!lives_osc_check_arguments (arglen,vargs,"sii",TRUE)) {
    type++;
    if (!lives_osc_check_arguments (arglen,vargs,"si",TRUE)) {
      if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) return;
      type++;
    }
  }
  lives_osc_parse_string_argument(vargs,filename);
  if (type>0) {
    lives_osc_parse_int_argument(vargs,&startframe);
    if (type>1) {
      lives_osc_parse_int_argument(vargs,&numframes);
    }
  }
  deduce_file(filename,startframe,numframes);

}


void lives_osc_cb_loadset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar setname[OSC_STRING_SIZE];

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return;

  if (mainw->playing_file>-1) return;

  if (strlen(mainw->set_name)>0) return;

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) {
    return;
  }
  lives_osc_parse_string_argument(vargs,setname);

  g_snprintf(mainw->set_name,256,"%s",setname);

  on_load_set_ok(NULL,GINT_TO_POINTER((gint)FALSE));


}


void lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  gchar setname[OSC_STRING_SIZE];

  if (mainw->preview||mainw->is_processing||mainw->multitrack!=NULL) return;

  if (mainw->playing_file>-1) return;

  if (!lives_osc_check_arguments (arglen,vargs,"s",TRUE)) {
    return;
  }

  lives_osc_parse_string_argument(vargs,setname);
  if (is_legal_set_name(setname,TRUE)) {
    mainw->only_close=TRUE;
    on_save_set_activate(NULL,setname);
    mainw->only_close=FALSE;
  }
}




static struct 
{
  char	 *descr;
  char	 *name;
  void	 (*cb)(void *ctx, int len, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra);
  int		 leave;
} osc_methods[] = 
  {
    { "/record/enable",		"enable",		lives_osc_record_start,			3	},	
    { "/record/disable",	"disable",		lives_osc_record_stop,			3	},	
    { "/record/toggle",	        "toggle",		lives_osc_record_toggle,			3	},	
    { "/video/play",		"play",		lives_osc_cb_play,			5	},	
    { "/video/play/forwards",		"forwards",		lives_osc_cb_play_forward,			36	},	
    { "/video/play/backwards",		"backwards",		lives_osc_cb_play_backward,			36	},	
    { "/video/play/faster",		"faster",		lives_osc_cb_play_faster,			36	},	
    { "/clip/foreground/fps/faster",		"faster",		lives_osc_cb_play_faster,			61	},	
    { "/clip/background/fps/faster",		"faster",		lives_osc_cb_bgplay_faster,			63	},	
    { "/video/play/slower",		"slower",		lives_osc_cb_play_slower,			36	},	
    { "/clip/foreground/fps/slower",		"slower",		lives_osc_cb_play_slower,			61	},	
    { "/clip/background/fps/slower",		"slower",		lives_osc_cb_bgplay_slower,			63	},	
    { "/video/play/reset",		"reset",		lives_osc_cb_play_reset,			36	},	
    { "/clip/foreground/fps/reset",		"reset",		lives_osc_cb_play_reset,			61	},	
    { "/clip/background/fps/reset",		"reset",		lives_osc_cb_bgplay_reset,			63	},	
    { "/video/stop",		"stop",	        lives_osc_cb_stop,				5	},
    { "/video/fps/set",	       "set",	lives_osc_cb_set_fps,			40	},
    { "/video/fps/ratio/set",	       "set",	lives_osc_cb_set_fps_ratio,			65	},
    { "/clip/foreground/fps/set",	"set",	lives_osc_cb_set_fps,			61	},
    { "/clip/background/fps/set",	"set",	lives_osc_cb_bgset_fps,			63	},
    { "/clip/foreground/fps/ratio/set",	"set",	lives_osc_cb_set_fps_ratio,			64	},
    { "/clip/background/fps/ratio/set",	"set",	lives_osc_cb_bgset_fps_ratio,			66	},
    { "/video/play/reverse",		"reverse",	lives_osc_cb_play_reverse,		36	},
    { "/clip/foreground/fps/reverse",	"reverse",	lives_osc_cb_play_reverse,		61	},
    { "/clip/background/fps/reverse",	"reverse",	lives_osc_cb_bgplay_reverse,		63	},
    { "/video/freeze/toggle",		"toggle", lives_osc_cb_freeze,		37	},
    { "/effect_key/map",		"map",	lives_osc_cb_fx_map,			25	},
    { "/effect_key/map/clear",		"clear",	lives_osc_cb_fx_map_clear,			32	},
    { "/effect_key/reset",		"reset",	lives_osc_cb_fx_reset,			25	},
    { "/effect_key/enable",		"enable",	lives_osc_cb_fx_enable,		        25	},
    { "/effect_key/disable",		"disable",	lives_osc_cb_fx_disable,		        25	},
    { "/effect_key/toggle",		"toggle",	lives_osc_cb_fx_toggle,		        25	},
    { "/effect_key/count",		"count",	lives_osc_cb_rte_count,		        25	},
    { "/effect_key/parameter/value/set",		"set",	lives_osc_cb_rte_setparam,		        42	},
    { "/effect_key/nparameter/count",		"count",	lives_osc_cb_rte_nparamcount,		        91	},
    { "/effect_key/nparameter/value/set",		"set",	lives_osc_cb_rte_setnparam,		        92	},
    { "/effect_key/nparameter/min/get",		"get",	lives_osc_cb_rte_getnparammin,		        93	},
    { "/effect_key/nparameter/max/get",		"get",	lives_osc_cb_rte_getnparammax,		        94	},
    { "/effect_key/nparameter/default/get",		"get",	lives_osc_cb_rte_getnparamdef,		        95	},
    { "/effect_key/nparameter/is_transition/get",		"get",	lives_osc_cb_rte_getnparamtrans,		        96	},
    { "/effect_key/mode/set",		"set",	lives_osc_cb_rte_setmode,		        43	},
    { "/effect_key/mode/get",		"get",	lives_osc_cb_rte_getmode,		        43	},
    { "/effect_key/mode/next",		"next",	lives_osc_cb_rte_nextmode,		        43	},
    { "/effect_key/mode/previous",		"previous",	lives_osc_cb_rte_prevmode,		        43	},
    { "/effect_key/name/get",		"get",	lives_osc_cb_rte_get_keyfxname,		        44	},
    { "/effect_key/maxmode/get",		"get",	lives_osc_cb_rte_getmodespk,		        45	},
    { "/effect_key/usermode/get",		"get",	lives_osc_cb_rte_getusermode,		        56	},
    { "/clip/encode_as",		"encode_as",	lives_osc_cb_clip_encodeas,			1	},
    { "/clip/select",		"select",	lives_osc_cb_fgclip_select,			1	},
    { "/clip/close",		"close",	lives_osc_cb_fgclip_close,	  		        1	},
    { "/clip/copy",		"copy",	lives_osc_cb_fgclip_copy,	  		        1	},
    { "/clip/selection/copy",		"copy",	lives_osc_cb_fgclipsel_copy,	  		        55	},
    { "/clip/selection/cut",		"cut",	lives_osc_cb_fgclipsel_cut,	  		        55	},
    { "/clip/selection/delete",		"delete",	lives_osc_cb_fgclipsel_delete,	  		        55	},
    { "/clipboard/paste",		"paste",	lives_osc_cb_clipbd_paste,			70	},
    { "/clipboard/insert_before",		"insert_before",	lives_osc_cb_clipbd_insertb,			70	},
    { "/clipboard/insert_after",		"insert_after",	lives_osc_cb_clipbd_inserta,			70	},
    { "/clip/retrigger",		"retrigger",	lives_osc_cb_fgclip_retrigger,			1	},
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
    { "/app/quit",	         "quit",	           lives_osc_cb_quit,			22	},
    { "/quit",	         "quit",	        lives_osc_cb_quit,			2	},
    { "/reply_to",	         "reply_to",	 lives_osc_cb_open_status_socket,			2	},
    { "/lives/open_status_socket",	         "open_status_socket",	        lives_osc_cb_open_status_socket,			21	},
    { "/app/open_status_socket",	         "open_status_socket",	        lives_osc_cb_open_status_socket,			22	},
    { "/app/ping",	         "ping",	           lives_osc_cb_ping,			22	},
    { "/lives/ping",	         "ping",	           lives_osc_cb_ping,			21	},
    { "/ping",	         "ping",	           lives_osc_cb_ping,			2	},
    { "/notify_to",	         "notify_to",	   lives_osc_cb_open_notify_socket,			2	},
    { "/lives/open_notify_socket",	         "open_notify_socket",	        lives_osc_cb_open_notify_socket,			21	},
    { "/clip/count",	         "count",	        lives_osc_cb_clip_count,			1  },
    { "/clip/goto",	         "goto",	        lives_osc_cb_clip_goto,			1	},
    { "/clip/foreground/frame/set",	         "set",	        lives_osc_cb_clip_goto,			60	},
    { "/clip/background/frame/set",	         "set",	        lives_osc_cb_bgclip_goto,			62	},
    { "/clip/is_valid/get",	         "get",	        lives_osc_cb_clip_isvalid,			49	},
    { "/clip/select_all",	         "select_all",	        lives_osc_cb_clip_select_all,			1	},
    { "/clip/start/set",	 "set",	        lives_osc_cb_clip_set_start,			50	},
    { "/clip/end/set",	 "set",	        lives_osc_cb_clip_set_end,			51	},
    { "/clip/open/file",	 "file",	        lives_osc_cb_open_file,			33	},
    { "/output/fullscreen/enable",		"enable",	lives_osc_cb_fssepwin_enable,		28	},
      { "/output/fullscreen/disable",		"disable",	lives_osc_cb_fssepwin_disable,       	28	},
      { "/output/fps/set",		"set",	lives_osc_cb_op_fps_set,       	52	},
      { "/output/nodrop/enable",		"enable",	lives_osc_cb_op_nodrope,       	30	},
      { "/output/nodrop/disable",		"disable",	lives_osc_cb_op_nodropd,       	30	},
      { "/clip/foreground/background/swap",		"swap",	lives_osc_cb_swap,       	53	},
      { "/clipset/load",		"load",	lives_osc_cb_loadset,       	35	},
      { "/clipset/save",		"save",	lives_osc_cb_saveset,       	35	},
    
    { NULL,					NULL,		NULL,							0	},
  };


static struct
{
  char *comment;
  char *name;	
  int  leave;
  int  att; 
  int  it;
} osc_cont[] = 
  {
    {	"/",	 	"",	                 2, -1,0   	},
    {	"/video/",	 	"video",	 5, -1,0   	},
    {	"/video/fps/",	 	"fps",	 40, 5,0   	},
    {	"/video/fps/ratio/",	 	"ratio",	 65, 40,0   	},
    {	"/video/play/ start video playback",	 	"play",	         36, 5,0   	},
    {	"/video/freeze/",	"freeze",        37, 5,0   	},
    {	"/clip/", 		"clip",		 1, -1,0	},
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
    {	"/clip/start/", 	"start",         50, 1,0	},
    {	"/clip/end/", 	        "end",           51, 1,0	},
    {	"/clip/select/", 	        "select",           54, 1,0	},
    {	"/clip/selection/", 	        "selection",           55, 1,0	},
    {	"/clipboard/", 		"clipboard",		 70, -1,0	},
    {	"/record/", 		"record",	 3, -1,0	},
    {	"/effect/" , 		"effect",	 4, -1,0	},
    {	"/effect_key/" , 		"effect_key",	 25, -1,0	},
    {	"/effect_key/parameter/" , 	"parameter",	 41, 25,0	},
    {	"/effect_key/parameter/value/" ,"value",	 42, 41,0	},
    {	"/effect_key/nparameter/" , 	"nparameter",	 91, 25,0	},
    {	"/effect_key/nparameter/value/" ,"value",	 92, 91,0	},
    {	"/effect_key/nparameter/min/" ,"min",	 93, 91,0	},
    {	"/effect_key/nparameter/max/" ,"max",	 94, 91,0	},
    {	"/effect_key/nparameter/default/" ,"default",	 95, 91,0	},
    {	"/effect_key/nparameter/is_transition/" ,"is_transition", 96, 91,0  },
    {	"/effect_key/map/" , 		"map",	 32, 25,0	},
    {	"/effect_key/mode/" , 		"mode",	 43, 25,0	},
    {	"/effect_key/name/" , 		"name",	 44, 25,0	},
    {	"/effect_key/maxmode/" , 	"maxmode",	 45, 25,0	},
    {	"/effect_key/usermode/" , 	"usermode",	 56, 25,0	},
    {	"/lives/" , 		"lives",	 21, -1,0	},
    {	"/clipset/" , 		"clipset",	 35, -1,0	},
    {	"/app/" , 		"app",	         22, -1,0	},
    {	"/output/" , 	"output",	 27, -1,0	},
    {	"/output/fullscreen/" , 	"fullscreen",	 28, 27,0	},
    {	"/output/fps/" , 	        "fps",	 52, 27,0	},
    {	"/output/nodrop/" , 	"nodrop",	 30, 27 ,0	},
    {	"/clip/open/",   		"open",		 33, 1,0	},
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
		  &(o->osc_args[0]),
		  &(o->ris));
    
    
  }
  return 1;
}	





/* initialization, setup a UDP socket and invoke OSC magic */
lives_osc* lives_osc_allocate(int port_id) {
  lives_osc *o;
  gchar *tmp;

  if (livesOSC==NULL) {
    o = (lives_osc*)g_malloc(sizeof(lives_osc));
    o->osc_args = (osc_arg*)g_malloc(50 * sizeof(*o->osc_args));
    o->rt.InitTimeMemoryAllocator = _lives_osc_time_malloc;
    o->rt.RealTimeMemoryAllocator = _lives_osc_rt_malloc;
    o->rt.receiveBufferSize = 1024;
    o->rt.numReceiveBuffers = 100;
    o->rt.numQueuedObjects = 100;
    o->rt.numCallbackListNodes = 200;
    o->leaves = (OSCcontainer*) g_malloc(sizeof(OSCcontainer) * 100);
    o->t.initNumContainers = 1000;
    o->t.initNumMethods = 2000;
    o->t.InitTimeMemoryAllocator = lives_malloc;
    o->t.RealTimeMemoryAllocator = lives_malloc;
    
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
  struct timeval tv;
  tv.tv_sec=0;
  tv.tv_usec = 0;
  
  /* see if there is something to read , this is effectivly NetworkPacketWaiting */
  // if(ioctl( o->sockfd, FIONREAD, &bytes,0 ) == -1) return 0;
  // if(bytes==0) return 0;
  if(NetworkPacketWaiting(o->packet)) {
    /* yes, receive packet from UDP */
    if(NetworkReceivePacket(o->packet)) {
      /* OSC must accept this packet (OSC will auto-invoke it, see source !) */
      OSCAcceptPacket(o->packet);
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
    g_printerr("error, OSC msglen > 100 !\n");
  }

  memcpy(OSCPacketBufferGetBuffer(packet),OSC_getPacket(obuf),bufsize);
  *psize=bufsize;
}




gboolean lives_osc_act(OSCbuf *obuf) {
  // this is a shortcut route to make LiVES carry out the OSC message in msg

  OSCPacketBuffer packet;

  if (livesOSC==NULL) lives_osc_init(0);

  packet=livesOSC->packet;

  oscbuf_to_packet(obuf,packet);
  
  OSCAcceptPacket(packet);

  OSCBeProductiveWhileWaiting();

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



gboolean lives_osc_init(guint udp_port) {
  gchar *tmp;

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
    if (udp_port!=0) gtk_timeout_add(KEY_RPT_INTERVAL,&lives_osc_poll,NULL);
  }

  return TRUE;
}


gboolean lives_osc_poll(gpointer data) {
  // data is always NULL
  // must return TRUE
  if (!mainw->osc_block&&mainw->multitrack==NULL&&livesOSC!=NULL) lives_osc_get_packet(livesOSC);
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
