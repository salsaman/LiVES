// main.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 or higher as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
*/

#include <glib.h>

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"
#endif

#define NEED_DEF_WIDGET_OPTS

#include "main.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"

#include "effects.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "paramwindow.h"
#include "stream.h"
#include "startup.h"
#include "cvirtual.h"

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif

#include <getopt.h>


#ifdef IS_DARWIN
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

#ifdef USE_LIBPNG
#include <png.h>
#include <setjmp.h>
#endif


#include <sys/stat.h>

////////////////////////////////
capability *capable;
_palette *palette;
size_t sizint, sizdbl, sizshrt;
mainwindow *mainw;


//////////////////////////////////////////

static boolean no_recover=FALSE,auto_recover=FALSE;
static boolean upgrade_error=FALSE;
static gchar start_file[PATH_MAX];
static gdouble start=0.;
static gint end=0;

static boolean theme_expected;

static _ign_opts ign_opts;

static int zargc;
static char **zargv;

static gint xxwidth=0,xxheight=0;

/* not static */
GtkTargetEntry target_table[]  = {
  { "STRING",                     GTK_TARGET_OTHER_APP, 0 },
  { "text/uri-list",              GTK_TARGET_OTHER_APP, 0 },
};



/////////////////////////////////
#ifdef NO_COMPILE // never compile this
void tr_msg(void) {
  // TRANSLATORS: do not translate this message
  gchar *msg=(_("Attention Translators !\nThis message is intended for you, so please do not translate it.\n\nAll translators should read the LiVES translation notes at\nhttp://lives.sourceforge.net/TRANS-README.txt"));
}
#endif



static void lives_log_handler (const char *domain, LiVESLogLevelFlags level, const char *message,  gpointer data) {
  gchar *msg;

#ifdef LIVES_NO_DEBUG
  if (level>=LIVES_LOG_LEVEL_WARNING ) return;
#else
  if ((level&LIVES_LOG_LEVEL_MASK)==LIVES_LOG_LEVEL_WARNING) 
    msg=g_strdup_printf(_("%s Warning: %s\n"),domain,message);
#endif
  else {
    if ((level&LIVES_LOG_LEVEL_MASK)==LIVES_LOG_LEVEL_CRITICAL) 
      msg=g_strdup_printf(_("%s Critical error: %s\n"),domain,message);
    else msg=g_strdup_printf(_("%s Fatal error: %s\n"),domain,message);
  }

  if (mainw->is_ready) {
    d_print(msg);
  }

  g_printerr("%s",msg);
  g_free(msg);

  if (level&LIVES_LOG_FATAL_MASK) raise(LIVES_SIGSEGV);
}

#ifdef IS_MINGW
typedef void (*SignalHandlerPointer)(int);
#endif

void catch_sigint(int signum) {
  // trap for ctrl-C and others 
  if (mainw!=NULL) {
    if (mainw->LiVES!=NULL) {
      if (mainw->foreign) {
	exit (signum);
      }

      if (mainw->multitrack!=NULL) mainw->multitrack->idlefunc=0;
      mainw->fatal=TRUE;

      if (signum==LIVES_SIGABRT||signum==LIVES_SIGSEGV) {
	signal (LIVES_SIGSEGV, SIG_DFL);
	signal (LIVES_SIGABRT, SIG_DFL);
	g_printerr("%s",_("\nUnfortunately LiVES crashed.\nPlease report this bug at http://sourceforge.net/tracker/?group_id=64341&atid=507139\nThanks. Recovery should be possible if you restart LiVES.\n"));
	g_printerr("%s",_("\n\nWhen reporting crashes, please include details of your operating system, distribution, and the LiVES version (" LiVES_VERSION ")\n"));

	if (capable->has_gdb) {
	  if (mainw->debug) g_printerr("%s",_("and any information shown below:\n\n"));
	  else g_printerr("%s","Please try running LiVES with the -debug option to collect more information.\n\n");
	}
	else {
	  g_printerr("%s",_("Please install gdb and then run LiVES with the -debug option to collect more information.\n\n"));
	}
	if (mainw->debug) {
	  g_on_error_stack_trace(capable->myname_full);
	}
      }
      
      if (mainw->was_set) {
	g_printerr ("%s",_("Preserving set.\n"));
      }

      mainw->leave_recovery=mainw->leave_files=TRUE;
      
      mainw->only_close=FALSE;
      lives_exit();
    }
  }
  exit(signum);
}



void get_monitors(void) {
  GSList *dlist,*dislist;
  GdkDisplay *disp;
  GdkScreen *screen;
#if GTK_CHECK_VERSION(3,0,0)
  GdkDeviceManager *devman;
  GList *devlist;
  register int k;
#endif

  gint nscreens,nmonitors;
  register int i,j,idx=0;

  if (mainw->mgeom!=NULL) g_free(mainw->mgeom);
  mainw->mgeom=NULL;

  dlist=dislist=gdk_display_manager_list_displays(gdk_display_manager_get());

  capable->nmonitors=0;

  // for each display get list of screens

  while (dlist!=NULL) {
    disp=(GdkDisplay *)dlist->data;

    // get screens
    nscreens=gdk_display_get_n_screens(disp);
    for (i=0;i<nscreens;i++) {
      screen=gdk_display_get_screen(disp,i);
      capable->nmonitors+=gdk_screen_get_n_monitors(screen);
    }
    dlist=dlist->next;
  }

  if (prefs->force_single_monitor) capable->nmonitors=1; // force for clone mode

  mainw->mgeom=(lives_mgeometry_t *)g_malloc(capable->nmonitors*sizeof(lives_mgeometry_t));
  dlist=dislist;

  while (dlist!=NULL) {
    disp=(GdkDisplay *)dlist->data;

#if GTK_CHECK_VERSION(3,0,0)
    devman=gdk_display_get_device_manager(disp);
    devlist=gdk_device_manager_list_devices(devman,GDK_DEVICE_TYPE_MASTER);
#endif
    // get screens
    nscreens=gdk_display_get_n_screens(disp);
    for (i=0;i<nscreens;i++) {
      screen=gdk_display_get_screen(disp,i);
      nmonitors=gdk_screen_get_n_monitors(screen);
      for (j=0;j<nmonitors;j++) {
	GdkRectangle rect;
	gdk_screen_get_monitor_geometry(screen,j,&(rect));
	mainw->mgeom[idx].x=rect.x;
	mainw->mgeom[idx].y=rect.y;
	mainw->mgeom[idx].width=rect.width;
	mainw->mgeom[idx].height=rect.height;
	mainw->mgeom[idx].mouse_device=NULL;
#if GTK_CHECK_VERSION(3,0,0)
	// get (virtual) mouse device for this screen
	for (k=0;k<g_list_length(devlist);k++) {
	  GdkDevice *device=(GdkDevice *)g_list_nth_data(devlist,k);
	  if (gdk_device_get_display(device)==disp&&
	      gdk_device_get_source(device)==GDK_SOURCE_MOUSE) { 
	    mainw->mgeom[idx].mouse_device=device;
	    break;
	  }
	}
#endif
	mainw->mgeom[idx].disp=disp;
	mainw->mgeom[idx].screen=screen;
	idx++;
      }
    }
#if GTK_CHECK_VERSION(3,0,0)
    g_list_free(devlist);
#endif
    dlist=dlist->next;
  }

  g_slist_free(dislist);
}






static boolean pre_init(void) {
  // stuff which should be done *before* mainwindow is created
  // returns TRUE if we expect to load a theme

  pthread_mutexattr_t mattr;

  gchar buff[256];
  int i;
  boolean needs_update=FALSE;

  sizint=sizeof(gint);
  sizdbl=sizeof(gdouble);
  sizshrt=sizeof(gshort);

  mainw=(mainwindow*)(g_malloc0(sizeof(mainwindow)));
  mainw->is_ready=mainw->fatal=FALSE;

  mainw->free_fn=lives_free_normal;
  mainw->do_not_free=NULL;
  mainw->alt_vtable.malloc=lives_malloc;
  mainw->alt_vtable.realloc=realloc;
  mainw->alt_vtable.free=lives_free;  ///< wrapper for mainw->free_fn
  mainw->alt_vtable.calloc=NULL;
  mainw->alt_vtable.try_malloc=NULL;
  mainw->alt_vtable.try_realloc=NULL;

  g_mem_set_vtable(&mainw->alt_vtable);

  // set to allow multiple locking by the same thread
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE); 
  pthread_mutex_init(&mainw->gtk_mutex,&mattr);

  pthread_mutex_init(&mainw->interp_mutex,NULL);

  pthread_mutex_init(&mainw->abuf_mutex,NULL);
  pthread_mutex_init(&mainw->afilter_mutex,&mattr); // mattr because audio filters can pull values from data connections
  pthread_mutex_init(&mainw->data_mutex,&mattr); // mattr because audio filters can pull values from data connections

  mainw->vrfx_update=NULL;

  mainw->kb_timer=-1;

  prefs=(_prefs *)g_malloc0(sizeof(_prefs));
  future_prefs=(_future_prefs *)g_malloc(sizeof(_future_prefs));

  prefs->wm=NULL;
  prefs->sleep_time=1000;
  mainw->cached_list=NULL;

  mainw->splash_window=NULL;

  mainw->threaded_dialog=FALSE;
  clear_mainw_msg();

  
#ifdef IS_MINGW
  // TODO - for mingw we will get from the registry, whatever was set at install time
  g_snprintf (prefs->prefix_dir,PATH_MAX,"%s",PREFIX_DEFAULT);
#endif

  // check the backend is there, get some system details and prefs
  capable=get_capabilities();

  palette=(_palette*)(g_malloc(sizeof(_palette)));

  widget_opts = def_widget_opts;
  widget_opts.border_width*=widget_opts.scale;
  widget_opts.packing_width*=widget_opts.scale;
  widget_opts.packing_height*=widget_opts.scale;
  widget_opts.filler_len*=widget_opts.scale;


  prefs->show_gui=TRUE;
  prefs->show_splash=TRUE;
  prefs->show_playwin=TRUE;
  prefs->sepwin_type=1;
  prefs->show_framecount=TRUE;
  prefs->audio_player=AUD_PLAYER_SOX;
  g_snprintf(prefs->aplayer,512,"%s","sox");
  prefs->open_decorated=TRUE;

#ifdef ENABLE_GIW
  prefs->lamp_buttons=TRUE;
#else
  prefs->lamp_buttons=FALSE;
#endif

  prefs->autoload_subs=TRUE;
  prefs->show_subtitles=TRUE;

  prefs->letterbox=FALSE;
  prefs->bigendbug=0;

#ifdef HAVE_YUV4MPEG
  memset(prefs->yuvin,0,1);
#endif

  if (!capable->smog_version_correct||!capable->can_write_to_tempdir) {
    g_snprintf(prefs->theme,64,"none");
    return FALSE;
  }

  // from here onwards we can use get_pref() and friends  //////
#ifndef IS_MINGW
  capable->rcfile=g_strdup_printf("%s/.lives",capable->home_dir);
#else
  capable->rcfile=g_strdup_printf("%s/LiVES.ini",capable->home_dir);
#endif
  cache_file_contents(capable->rcfile);

  get_pref("gui_theme",prefs->theme,64);
  if (!strlen(prefs->theme)) {
    g_snprintf(prefs->theme,64,"none");
  }
  // get some prefs we need to set menu options
  future_prefs->show_recent=prefs->show_recent=get_boolean_pref ("show_recent_files");

#ifndef IS_MINGW  
  get_pref ("prefix_dir",prefs->prefix_dir,PATH_MAX);
  
  if (!strlen (prefs->prefix_dir)) {
    if (strcmp (PREFIX,"NONE")) {
      g_snprintf (prefs->prefix_dir,PATH_MAX,"%s",PREFIX);
    }
    else {
      g_snprintf (prefs->prefix_dir,PATH_MAX,"%s",PREFIX_DEFAULT);
    }
    needs_update=TRUE;
  }
  
  if (ensure_isdir(prefs->prefix_dir)) needs_update=TRUE;


  if (needs_update) set_pref("prefix_dir",prefs->prefix_dir);

  needs_update=FALSE;

  get_pref ("lib_dir",prefs->lib_dir,PATH_MAX);
  
  if (!strlen (prefs->lib_dir)) {

    g_snprintf (prefs->lib_dir,PATH_MAX,"%s",LIVES_LIBDIR);
    needs_update=TRUE;
  }

  if (ensure_isdir(prefs->lib_dir)) needs_update=TRUE;
  if (needs_update) set_pref("lib_dir",prefs->lib_dir);

#else
    g_snprintf (prefs->lib_dir,PATH_MAX,"%s",prefs->prefix_dir);
#endif
  
  needs_update=FALSE;

  set_palette_colours();

  get_pref("cdplay_device",prefs->cdplay_device,256);
  prefs->warning_mask=(guint)get_int_pref("lives_warning_mask");

  get_pref("audio_player",buff,256);

  if (!strcmp(buff,"mplayer"))
    prefs->audio_player=AUD_PLAYER_MPLAYER;
  if (!strcmp(buff,"jack"))
    prefs->audio_player=AUD_PLAYER_JACK;
  if (!strcmp(buff,"pulse"))
    prefs->audio_player=AUD_PLAYER_PULSE;
  g_snprintf(prefs->aplayer,512,"%s",buff);

#ifdef HAVE_PULSE_AUDIO
  if ((prefs->startup_phase==1||prefs->startup_phase==-1)&&capable->has_pulse_audio) {
    prefs->audio_player=AUD_PLAYER_PULSE;
    g_snprintf(prefs->aplayer,512,"%s","pulse");
    set_pref("audio_player","pulse");
  }
  else {
#endif
  


#ifdef ENABLE_JACK
    if ((prefs->startup_phase==1||prefs->startup_phase==-1)&&capable->has_jackd) {
      prefs->audio_player=AUD_PLAYER_JACK;
      g_snprintf(prefs->aplayer,512,"%s","jack");
      set_pref("audio_player","jack");
    }
#endif

#ifdef HAVE_PULSE_AUDIO
  }
#endif

  future_prefs->jack_opts=get_int_pref("jack_opts");
  prefs->jack_opts=future_prefs->jack_opts;

  prefs->gui_monitor=0;
  prefs->play_monitor=0;

  mainw->mgeom=NULL;
  prefs->virt_height=1;

  prefs->force_single_monitor=get_boolean_pref("force_single_monitor");

  get_monitors();

  if (capable->nmonitors>1) {

    get_pref("monitors",buff,256);

    if (strlen(buff)==0||get_token_count(buff,',')==1) {
      prefs->gui_monitor=1;
      prefs->play_monitor=2;
    }
    else {
      gchar **array=g_strsplit(buff,",",2);
      prefs->gui_monitor=atoi(array[0]);
      prefs->play_monitor=atoi(array[1]);
      g_strfreev(array);
    }

    if (prefs->gui_monitor<1) prefs->gui_monitor=1;
    if (prefs->play_monitor<0) prefs->play_monitor=0;
    if (prefs->gui_monitor>capable->nmonitors) prefs->gui_monitor=capable->nmonitors;
    if (prefs->play_monitor>capable->nmonitors) prefs->play_monitor=capable->nmonitors;
  }

  mainw->scr_width=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].width;
  mainw->scr_height=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].height;

  for (i=0;i<MAX_FX_CANDIDATE_TYPES;i++) {
    mainw->fx_candidates[i].delegate=-1;
    mainw->fx_candidates[i].list=NULL;
    mainw->fx_candidates[i].func=0l;
    mainw->fx_candidates[i].rfx=NULL;
  }

  for (i=0;i<MAX_EXT_CNTL;i++) mainw->ext_cntl[i]=FALSE;

  prefs->omc_dev_opts=get_int_pref("omc_dev_opts");

  get_pref_utf8("omc_js_fname",prefs->omc_js_fname,256);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
#ifndef IS_MINGW
  if (strlen(prefs->omc_js_fname)==0) {
    const gchar *tmp=get_js_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_js_fname,256,"%s",tmp);
      }
  }
#endif
#endif
#endif
  
  get_pref_utf8("omc_midi_fname",prefs->omc_midi_fname,256);
#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifndef IS_MINGW
  if (strlen(prefs->omc_midi_fname)==0) {
    const gchar *tmp=get_midi_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_midi_fname,256,"%s",tmp);
    }
  }
#endif
#endif
#endif

#ifdef ALSA_MIDI
    prefs->use_alsa_midi=TRUE;
    mainw->seq_handle=NULL;

    if (prefs->omc_dev_opts&OMC_DEV_FORCE_RAW_MIDI) prefs->use_alsa_midi=FALSE;

#endif

  mainw->volume=1.f;
  mainw->ccpd_with_sound=TRUE;

  mainw->loop=TRUE;
  mainw->loop_cont=FALSE;
  mainw->target_table=target_table;

  prefs->max_modes_per_key=0;
  mainw->debug=FALSE;

  mainw->next_free_alarm=0;

  for (i=0;i<LIVES_MAX_ALARMS;i++) {
    mainw->alarms[i]=LIVES_NO_ALARM_TICKS;
  }

  needs_update=needs_update; // stop compiler warnings

  if (!g_ascii_strcasecmp(prefs->theme,"none")) return FALSE;

  return TRUE;

}



static void replace_with_delegates (void) {
  int resize_fx;
  weed_plant_t *filter;
  lives_rfx_t *rfx;
  gchar mtext[256];
  int i;

  int deint_idx;

  if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate!=-1) {
    
    resize_fx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_RESIZER].list,
					      mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate));
    filter=get_weed_filter(resize_fx);
    rfx=weed_to_rfx(filter,TRUE);
    
    rfx->is_template=FALSE;
    rfx->props|=RFX_PROPS_MAY_RESIZE;

    g_free(rfx->action_desc);
    rfx->action_desc=g_strdup(_("Resizing"));

    rfx->min_frames=1;

    g_free(rfx->menu_text);

    if (mainw->resize_menuitem==NULL) {
      rfx->menu_text=g_strdup(_("_Resize All Frames"));
      mainw->resize_menuitem = gtk_menu_item_new_with_mnemonic(rfx->menu_text);
      gtk_widget_show(mainw->resize_menuitem);
      gtk_menu_shell_insert (GTK_MENU_SHELL (mainw->tools_menu), mainw->resize_menuitem, RFX_TOOL_MENU_POSN);
    }
    else {
      get_menu_text(mainw->resize_menuitem,mtext);
      
      // remove trailing dots
      for (i=strlen(mtext)-1;i>0&&!strncmp(&mtext[i],".",1);i--) memset(&mtext[i],0,1);
      
      rfx->menu_text=g_strdup(mtext);

      // disconnect old menu entry
      g_signal_handler_disconnect(mainw->resize_menuitem,mainw->fx_candidates[FX_CANDIDATE_RESIZER].func);
      
    }
    // connect new menu entry
    mainw->fx_candidates[FX_CANDIDATE_RESIZER].func=g_signal_connect (GTK_OBJECT (mainw->resize_menuitem), "activate",
								      G_CALLBACK (on_render_fx_pre_activate),
								      (gpointer)rfx);
    mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx=rfx;
  }

  deint_idx=weed_get_idx_for_hashname("deinterlacedeinterlace",FALSE);
  if (deint_idx>-1) {
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list=g_list_append(mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list,GINT_TO_POINTER(deint_idx));
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate=0;
  }
  

}



static void lives_init(_ign_opts *ign_opts) {
  // init mainwindow data
  int i;
  int randfd;
  int naudp=0;

  boolean needs_free;

  ssize_t randres;
  gchar buff[256];
  uint32_t rseed;
  GList *encoders=NULL;
  GList *encoder_capabilities=NULL;

  gchar *weed_plugin_path;
  gchar *frei0r_path;
  gchar *ladspa_path;


  for (i=0;i<=MAX_FILES;mainw->files[i++]=NULL);
  mainw->fs=FALSE;
  mainw->prefs_changed=FALSE;
  mainw->last_dprint_file=mainw->current_file=mainw->playing_file=-1;
  mainw->first_free_file=1;
  mainw->insert_after=TRUE;
  mainw->mute=FALSE;
  mainw->faded=FALSE;
  mainw->save_with_sound=TRUE;
  mainw->preview=FALSE;
  mainw->selwidth_locked=FALSE;
  mainw->untitled_number=mainw->cap_number=1;
  mainw->sel_start=0;
  mainw->sel_move=SEL_MOVE_AUTO;
  mainw->record_foreign=FALSE;
  mainw->play_window=NULL;
  mainw->opwx=mainw->opwy=-1;
  mainw->frame_layer=NULL;
  mainw->in_fs_preview=FALSE;
  mainw->effects_paused=FALSE;
  mainw->play_start=0;
  mainw->opening_loc=FALSE;
  mainw->toy_type=LIVES_TOY_NONE;
  mainw->framedraw=mainw->framedraw_spinbutton=NULL;
  mainw->fd_layer=NULL;
  mainw->fd_layer_orig=NULL;
  mainw->is_processing=FALSE;
  mainw->is_rendering=FALSE;
  mainw->is_generating=FALSE;
  mainw->resizing=FALSE;
  mainw->switch_during_pb=FALSE;
  mainw->playing_sel=FALSE;
  mainw->aframeno=0;
  if (capable->byte_order==LIVES_LITTLE_ENDIAN) {
    mainw->endian=0;
  }
  else {
    mainw->endian=AFORM_BIG_ENDIAN;
  }

  mainw->leave_files=TRUE;
  mainw->was_set=FALSE;
  mainw->toy_go_wild=FALSE;

  for (i=0;i<FN_KEYS-1;i++) {
    mainw->clipstore[i]=0;
  }

  mainw->ping_pong=FALSE;

  mainw->nervous=FALSE;
  fx_dialog[0]=fx_dialog[1]=NULL;

  mainw->rte_keys=-1;
  rte_window=NULL;

  mainw->rte=EFFECT_NONE;

  mainw->must_resize=FALSE;

  mainw->preview_box = NULL;
  mainw->prv_link=PRV_FREE;

  mainw->internal_messaging=FALSE;
  mainw->progress_fn=NULL;

  mainw->last_grabable_effect=-1;
  mainw->blend_file=-1;

  mainw->pre_src_file=-2;
  mainw->pre_src_audio_file=-1;

  mainw->size_warn=FALSE;
  mainw->dvgrab_preview=FALSE;

  mainw->file_open_params=NULL;
  mainw->whentostop=NEVER_STOP;

  mainw->audio_start=mainw->audio_end=0;
  mainw->cliplist=NULL;

  // rendered_fx number of last transition
  mainw->last_transition_idx=-1;
  mainw->last_transition_loops=1;
  mainw->last_transition_align_start=TRUE;
  mainw->last_transition_loop_to_fit=mainw->last_transition_ins_frames=FALSE;
  mainw->num_tr_applied=0;

  mainw->blend_factor=0.;

  mainw->fixed_fps_numer=-1;
  mainw->fixed_fps_denom=1;
  mainw->fixed_fpsd=-1.;
  mainw->noswitch=FALSE;
  mainw->osc_block=FALSE;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_KILL;

  mainw->framedraw_reset=NULL;

  // setting this to TRUE can possibly increase smoothness for lower framerates
  // needs more testing and a preference - TODO
  // can now be set through OSC: /output/nodrop/enable
  mainw->noframedrop=FALSE;

  prefs->omc_noisy=FALSE;
  prefs->omc_events=TRUE;

  if (!ign_opts->ign_osc) {
    prefs->osc_udp_started=FALSE;
    prefs->osc_udp_port=0;
#ifdef ENABLE_OSC
    if (!mainw->foreign) {
      prefs->osc_udp_port=get_int_pref ("osc_port");
      future_prefs->osc_start=prefs->osc_start=get_boolean_pref("osc_start");
    }
    else {
      future_prefs->osc_start=prefs->osc_start=FALSE;
    }
#endif
  }

  prefs->ignore_tiny_fps_diffs=1;
  prefs->rec_opts=get_int_pref("record_opts");

  if (prefs->rec_opts==-1) {
    prefs->rec_opts=REC_FPS|REC_FRAMES|REC_EFFECTS|REC_CLIPS|REC_AUDIO;
    set_int_pref("record_opts",prefs->rec_opts);
  }

  prefs->rec_opts|=(REC_FPS+REC_FRAMES);

  prefs->audio_src=get_int_pref("audio_src");

  if (!((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    prefs->audio_src=AUDIO_SRC_INT;
  }

  mainw->new_clip=-1;
  mainw->record=FALSE;
  mainw->event_list=NULL;
  mainw->clip_switched=FALSE;
  mainw->scrap_file=-1;
  mainw->ascrap_file=-1;

  mainw->multitrack=NULL;

  mainw->jack_can_stop=FALSE;
  mainw->jack_can_start=TRUE;

  mainw->video_seek_ready=FALSE;

  mainw->filter_map=NULL; // filter map for video rendering
  mainw->afilter_map=NULL; // filter map for audio rendering
  mainw->audio_event=NULL;

  mainw->did_rfx_preview=FALSE;
  mainw->invis=NULL;

  prefsw=NULL;
  rdet=NULL;
  resaudw=NULL;

  mainw->actual_frame=0;

  mainw->scratch=SCRATCH_NONE;

  mainw->clip_index=mainw->frame_index=NULL;

  mainw->affected_layouts_map=mainw->current_layouts_map=NULL;

  mainw->recovery_file=g_strdup_printf("%s/recovery.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),lives_getpid());
  mainw->leave_recovery=TRUE;

  mainw->pchains=NULL;

  mainw->preview_frame=0;

  mainw->unordered_blocks=FALSE;

  mainw->only_close=FALSE;

  mainw->no_exit=FALSE;

  mainw->multi_opts.set=FALSE;

  mainw->clip_header=NULL;

  mainw->new_blend_file=-1;

  mainw->jackd=mainw->jackd_read=NULL;

  mainw->pulsed=mainw->pulsed_read=NULL;

  mainw->suppress_dprint=FALSE;

  // TRANSLATORS: text saying "Any", for encoder and output format (as in "does not matter")
  mainw->string_constants[LIVES_STRING_CONSTANT_ANY]=g_strdup(_("Any"));
  // TRANSLATORS: text saying "None", for playback plugin name (as in "none specified")
  mainw->string_constants[LIVES_STRING_CONSTANT_NONE]=g_strdup(_("None"));
  // TRANSLATORS: text saying "recommended", for plugin names, etc.
  mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]=g_strdup(_("recommended"));
  // TRANSLATORS: text saying "disabled", (as in "not enabled")
  mainw->string_constants[LIVES_STRING_CONSTANT_DISABLED]=g_strdup(_("disabled !"));
  // TRANSLATORS: text saying "**The current layout**", to warn users that the current layout is affected
  mainw->string_constants[LIVES_STRING_CONSTANT_CL]=g_strdup(_("**The current layout**"));
  // TRANSLATORS: adjective for "Built in" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]=g_strdup(_("Builtin"));
  // TRANSLATORS: adjective for "Custom" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]=g_strdup(_("Custom"));
  // TRANSLATORS: adjective for "Test" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_TEST]=g_strdup(_("Test"));

  mainw->opening_frames=-1;

  mainw->show_procd=TRUE;

  mainw->framedraw_preview=mainw->framedraw_reset=NULL;

  mainw->block_param_updates=mainw->no_interp=FALSE;

  mainw->cevent_tc=0;

  mainw->opening_multi=FALSE;

  mainw->img_concat_clip=-1;

  mainw->record_paused=mainw->record_starting=FALSE;

  mainw->gen_to_clipboard=FALSE;

  mainw->open_deint=FALSE;

  mainw->write_vpp_file=FALSE;

  mainw->stream_ticks=-1;

  mainw->keep_pre=FALSE;

  mainw->reverse_pb=FALSE;

  mainw->osc_auto=FALSE;
  mainw->osc_enc_width=mainw->osc_enc_height=0;


  mainw->no_switch_dprint=FALSE;

  mainw->rte_textparm=NULL;


  mainw->abufs_to_fill=0;

  mainw->recoverable_layout=FALSE;

  mainw->soft_debug=FALSE;

  mainw->iochan=NULL;


  mainw->stored_event_list=NULL;
  mainw->stored_event_list_changed=mainw->stored_event_list_auto_changed=FALSE;
  mainw->stored_layout_save_all_vals=TRUE;

  mainw->affected_layout_marks=NULL;

  mainw->xlays=NULL;

  mainw->stored_layout_undos=NULL;
  mainw->sl_undo_mem=NULL;
  mainw->sl_undo_buffer_used=0;
  mainw->sl_undo_offset=0;

  mainw->go_away=TRUE;

  mainw->aud_file_to_kill=-1;

  mainw->aud_rec_fd=-1;

  mainw->decoders_loaded=FALSE;
  mainw->decoder_list=NULL;

  mainw->subt_save_file=NULL;

  mainw->fonts_array=get_font_list();

  mainw->nfonts=0;
  if (mainw->fonts_array!=NULL)
    while (mainw->fonts_array[mainw->nfonts++]!=NULL);

  mainw->videodevs=NULL;

  mainw->camframe=NULL;

  if (!ign_opts->ign_vppdefs)
    g_snprintf(mainw->vpp_defs_file,PATH_MAX,"%s/%svpp_defaults",capable->home_dir,LIVES_CONFIG_DIR);

  mainw->has_custom_tools=FALSE;
  mainw->has_custom_gens=FALSE;
  mainw->has_custom_utilities=FALSE;

  mainw->log_fd=-2;

  mainw->last_display_ticks=0;

  mainw->jack_trans_poll=FALSE;

  mainw->toy_alives_pid=0;
  mainw->autolives_reset_fx=FALSE;

  mainw->aplayer_broken=FALSE;

  mainw->com_failed=mainw->read_failed=mainw->write_failed=mainw->chdir_failed=FALSE;

  mainw->read_failed_file=mainw->write_failed_file=NULL;

  mainw->bad_aud_file=NULL;

  mainw->render_error=LIVES_RENDER_ERROR_NONE;
  
  mainw->is_exiting=FALSE;
  
  mainw->add_clear_ds_button=FALSE;
  mainw->add_clear_ds_adv=FALSE;
  mainw->tried_ds_recover=FALSE;

  mainw->foreign_visual=NULL;

  mainw->pconx=NULL;
  mainw->cconx=NULL;

  cached_key=cached_mod=0;

  mainw->agen_key=0;
  mainw->agen_needs_reinit=FALSE;
  mainw->agen_samps_count=0;

  mainw->draw_blocked=FALSE;

  mainw->ce_frame_height=mainw->ce_frame_width=-1;

  mainw->overflow_height=0;

  mainw->cursor_style=LIVES_CURSOR_NORMAL;

  /////////////////////////////////////////////////// add new stuff just above here ^^


  memset (mainw->set_name,0,1);
  mainw->clips_available=0;

  prefs->pause_effect_during_preview=FALSE;

  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {

    gint pb_quality=get_int_pref("pb_quality");
    
    prefs->pb_quality=PB_QUALITY_MED;
    if (pb_quality==PB_QUALITY_LOW) prefs->pb_quality=PB_QUALITY_LOW;
    else if (pb_quality==PB_QUALITY_HIGH) prefs->pb_quality=PB_QUALITY_HIGH;

    mainw->vpp=NULL;
    mainw->ext_playback=mainw->ext_keyboard=FALSE;

    get_pref("default_image_format",buff,256);
    if (!strcmp(buff,"jpeg")) g_snprintf (prefs->image_ext,16,"%s","jpg");
    else g_snprintf (prefs->image_ext,16,"%s",buff);

    prefs->loop_recording=TRUE;
    prefs->no_bandwidth=FALSE;
    prefs->ocp=get_int_pref ("open_compression_percent");

    lives_widget_color_copy(&palette->fade_colour,&palette->black);

    // we set the theme here in case it got reset to 'none'
    set_pref("gui_theme",prefs->theme);
    g_snprintf(future_prefs->theme,64,"%s",prefs->theme);

    prefs->stop_screensaver=get_boolean_pref("stop_screensaver");
    prefs->open_maximised=get_boolean_pref("open_maximised");
    future_prefs->show_tool=prefs->show_tool=get_boolean_pref("show_toolbar");
    memset (future_prefs->vpp_name,0,64);
    future_prefs->vpp_argv=NULL;

    if (prefs->gui_monitor!=0) {
      gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-
						      lives_widget_get_allocation_width(mainw->LiVES))/2;
      gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
						      lives_widget_get_allocation_height(mainw->LiVES))/2;
      gtk_window_set_screen(GTK_WINDOW(mainw->LiVES),mainw->mgeom[prefs->gui_monitor-1].screen);
      gtk_window_move(GTK_WINDOW(mainw->LiVES),xcen,ycen);

    }
    if (prefs->open_maximised&&prefs->show_gui) {
      gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
    }

    prefs->default_fps=get_double_pref("default_fps");
    if (prefs->default_fps<1.) prefs->default_fps=1.;
    if (prefs->default_fps>FPS_MAX) prefs->default_fps=FPS_MAX;

    prefs->q_type=Q_SMOOTH;
    prefs->bar_height=5;

    prefs->event_window_show_frame_events=TRUE;
    if (!mainw->foreign) prefs->crash_recovery=TRUE;
    else prefs->crash_recovery=FALSE;

    prefs->acodec_list=NULL;

    prefs->collate_images=TRUE;
    
    prefs->render_audio=TRUE;
    prefs->normalise_audio=TRUE;

    prefs->num_rtaudiobufs=4;

    prefs->safe_symlinks=FALSE; // set to TRUE for dynebolic and other live CDs

    prefs->ce_maxspect=get_boolean_pref("ce_maxspect");;

    prefs->rec_stop_gb=get_int_pref("rec_stop_gb");

    if (prefs->max_modes_per_key==0) prefs->max_modes_per_key=8;

    get_pref("def_autotrans",prefs->def_autotrans,256);

    prefs->nfx_threads=get_int_pref("nfx_threads");
    if (prefs->nfx_threads==0) prefs->nfx_threads=capable->ncpus;
    future_prefs->nfx_threads=prefs->nfx_threads;

    prefs->stream_audio_out=get_boolean_pref("stream_audio_out");

    prefs->unstable_fx=FALSE;

    prefs->disabled_decoders=get_list_pref("disabled_decoders");

    prefs->enc_letterbox=FALSE;

    get_pref("ds_warn_level",buff,256);
    if (!strlen(buff)) prefs->ds_warn_level=DEF_DS_WARN_LEVEL;
    else prefs->ds_warn_level=strtol(buff,NULL,10);

    mainw->next_ds_warn_level=prefs->ds_warn_level;

    get_pref("ds_crit_level",buff,256);
    if (!strlen(buff)) prefs->ds_crit_level=DEF_DS_CRIT_LEVEL;
    else prefs->ds_crit_level=strtol(buff,NULL,10);

    prefs->clear_disk_opts=get_int_pref("clear_disk_opts");

    prefs->force_system_clock=FALSE;  ///< prefer soundcard timing

    prefs->alpha_post=FALSE; ///< allow pre-multiplied alpha internally

    prefs->auto_trim_audio=TRUE;

    prefs->force64bit=FALSE;

    prefs->present=FALSE;

    //////////////////////////////////////////////////////////////////

    weed_memory_init();

    if (!mainw->foreign) {

      randres=-1;

      // try to get randomness from /dev/urandom
      randfd=open("/dev/urandom",O_RDONLY);
 
     if (randfd>-1) {
	randres=read(randfd,&rseed,sizint);
	close(randfd);
      }

      if (randres!=sizint) {
	gettimeofday(&tv,NULL);
	rseed=tv.tv_sec+tv.tv_usec;
      }

      lives_srandom(rseed);

      randres=-1;

      randfd=open("/dev/urandom",O_RDONLY);

      if (randfd>-1) {
	randres=read(randfd,&rseed,sizint);
	close(randfd);
      }
      if (randres!=sizint) {
	gettimeofday(&tv,NULL);
	rseed=tv.tv_sec+tv.tv_usec;
      }

      fastsrand(rseed);

      prefs->midi_check_rate=get_int_pref("midi_check_rate");
      if (prefs->midi_check_rate==0) prefs->midi_check_rate=DEF_MIDI_CHECK_RATE;
      
      if (prefs->midi_check_rate<1) prefs->midi_check_rate=1;
      
      prefs->midi_rpt=get_int_pref("midi_rpt");
      if (prefs->midi_rpt==0) prefs->midi_rpt=DEF_MIDI_RPT;

      prefs->mouse_scroll_clips=get_boolean_pref("mouse_scroll_clips");
      
      prefs->mt_auto_back=get_int_pref("mt_auto_back");

      get_pref ("vid_playback_plugin",buff,256);
      if (strlen (buff)&&strcmp (buff,"(null)")&&strcmp(buff,"none")) {
	mainw->vpp=open_vid_playback_plugin (buff,TRUE);
      }

      get_pref("video_open_command",prefs->video_open_command,256);

      if (!ign_opts->ign_aplayer) {
	get_pref("audio_play_command",prefs->audio_play_command,256);
      }

      if (!strlen(prefs->video_open_command)&&capable->has_mplayer) {
	get_location("mplayer",prefs->video_open_command,256);
	set_pref("video_open_command",prefs->video_open_command);
      }
      
      prefs->warn_file_size=get_int_pref("warn_file_size");
      if (prefs->warn_file_size==0) {
	prefs->warn_file_size=WARN_FILE_SIZE;
      }

      prefs->rte_keys_virtual=get_int_pref("rte_keys_virtual");
      if (prefs->rte_keys_virtual<FX_KEYS_PHYSICAL) prefs->rte_keys_virtual=FX_KEYS_PHYSICAL;
      if (prefs->rte_keys_virtual>FX_KEYS_MAX_VIRTUAL) prefs->rte_keys_virtual=FX_KEYS_MAX_VIRTUAL;

      prefs->show_rdet=TRUE;

      prefs->move_effects=TRUE;

      prefs->mt_undo_buf=get_int_pref("mt_undo_buf");

      prefs->mt_enter_prompt=get_boolean_pref("mt_enter_prompt");

      prefs->mt_def_width=get_int_pref("mt_def_width");
      prefs->mt_def_height=get_int_pref("mt_def_height");
      prefs->mt_def_fps=get_double_pref("mt_def_fps");
      prefs->mt_def_arate=get_int_pref("mt_def_arate");
      prefs->mt_def_achans=get_int_pref("mt_def_achans");
      prefs->mt_def_asamps=get_int_pref("mt_def_asamps");
      prefs->mt_def_signed_endian=get_int_pref("mt_def_signed_endian");

      if (prefs->mt_def_width==0) prefs->mt_def_width=DEFAULT_FRAME_HSIZE;
      if (prefs->mt_def_height==0) prefs->mt_def_height=DEFAULT_FRAME_VSIZE;
      if (prefs->mt_def_fps==0.) prefs->mt_def_fps=prefs->default_fps;
      if (prefs->mt_def_arate==0) prefs->mt_def_arate=DEFAULT_AUDIO_RATE;
      if (prefs->mt_def_asamps==0) prefs->mt_def_asamps=DEFAULT_AUDIO_SAMPS;

      prefs->mt_exit_render=get_boolean_pref("mt_exit_render");
      prefs->render_prompt=get_boolean_pref("render_prompt");

      prefs->mt_pertrack_audio=get_boolean_pref("mt_pertrack_audio");
      prefs->mt_backaudio=get_int_pref("mt_backaudio");

      prefs->instant_open=get_boolean_pref("instant_open");
      prefs->auto_deint=get_boolean_pref("auto_deinterlace");
      prefs->auto_nobord=get_boolean_pref("auto_cut_borders");

      if (!ign_opts->ign_clipset) {
	get_pref("ar_clipset",prefs->ar_clipset_name,128);
	if (strlen(prefs->ar_clipset_name)) prefs->ar_clipset=TRUE;
	else prefs->ar_clipset=FALSE;
      }

      get_pref("ar_layout",prefs->ar_layout_name,PATH_MAX);
      if (strlen(prefs->ar_layout_name)) prefs->ar_layout=TRUE;
      else prefs->ar_layout=FALSE;

      prefs->rec_desktop_audio=get_boolean_pref("rec_desktop_audio");

      future_prefs->startup_interface=get_int_pref("startup_interface");
      if (!ign_opts->ign_stmode) {
	prefs->startup_interface=future_prefs->startup_interface;
      }

      // scan for encoder plugins
#ifndef IS_MINGW
      if ((encoders=get_plugin_list (PLUGIN_ENCODERS,FALSE,NULL,NULL))!=NULL) {
#else
      if ((encoders=get_plugin_list (PLUGIN_ENCODERS,TRUE,NULL,NULL))!=NULL) {
#endif
	capable->has_encoder_plugins=TRUE;
	g_list_free_strings (encoders);
	g_list_free (encoders);
      }
      
      memset(prefs->encoder.of_name,0,1);

      if ((prefs->startup_phase==1||prefs->startup_phase==-1)&&capable->has_encoder_plugins&&capable->has_python) {
	GList *ofmt_all=NULL;
	gchar **array;
	g_snprintf(prefs->encoder.name,52,"%s","multi_encoder");

	// need to change the output format

	if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
	  
	  set_pref("encoder",prefs->encoder.name);

	  for (i=0;i<g_list_length(ofmt_all);i++) {
	    if (get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|')>2) {
	      array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);

	      if (!strcmp(array[0],"hi-theora")) {
		g_snprintf(prefs->encoder.of_name,51,"%s",array[0]);
		g_strfreev (array);
		break;
	      }
	      if (!strcmp(array[0],"hi-mpeg")) {
		g_snprintf(prefs->encoder.of_name,51,"%s",array[0]);
	      }
	      else if (!strcmp(array[0],"hi_h-mkv")&&strcmp(prefs->encoder.of_name,"hi-mpeg")) {
		g_snprintf(prefs->encoder.of_name,51,"%s",array[0]);
	      }
	      else if (!strcmp(array[0],"hi_h-avi")&&strcmp(prefs->encoder.of_name,"hi-mpeg")&&strcmp(prefs->encoder.of_name,"hi_h-mkv")) {
		g_snprintf(prefs->encoder.of_name,51,"%s",array[0]);
	      }
	      else if (!strlen(prefs->encoder.of_name)) {
		g_snprintf(prefs->encoder.of_name,51,"%s",array[0]);
	      }

	      g_strfreev (array);
	    }
	  }

	  set_pref("output_type",prefs->encoder.of_name);

	  g_list_free_strings (ofmt_all);
	  g_list_free (ofmt_all);
	}
      }

      if (!strlen(prefs->encoder.of_name)) {
	get_pref("encoder",prefs->encoder.name,51);
	get_pref("output_type",prefs->encoder.of_name,51);	
      }
      
      future_prefs->encoder.audio_codec=prefs->encoder.audio_codec=get_int_pref("encoder_acodec");
      prefs->encoder.capabilities=0;
      prefs->encoder.of_allowed_acodecs=AUDIO_CODEC_UNKNOWN;

      g_snprintf(future_prefs->encoder.name,52,"%s",prefs->encoder.name);

      memset (future_prefs->encoder.of_restrict,0,1);
      memset (prefs->encoder.of_restrict,0,1);

      if (capable->has_encoder_plugins) {
	gchar **array;
	gint numtok;
	GList *ofmt_all,*dummy_list;

	dummy_list=plugin_request("encoders",prefs->encoder.name,"init");
	if (dummy_list!=NULL) {
	  g_list_free_strings(dummy_list);
	  g_list_free(dummy_list);
	}
	if (!((encoder_capabilities=plugin_request(PLUGIN_ENCODERS,prefs->encoder.name,"get_capabilities"))==NULL)) {
	  prefs->encoder.capabilities=atoi ((gchar *)g_list_nth_data (encoder_capabilities,0));
	  g_list_free_strings (encoder_capabilities);
	  g_list_free (encoder_capabilities);
	  if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
	    // get any restrictions for the current format
	    for (i=0;i<g_list_length(ofmt_all);i++) {
	      if ((numtok=get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|'))>2) {
		array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);
		if (!strcmp(array[0],prefs->encoder.of_name)) {
		  if (numtok>1) {
		    g_snprintf(prefs->encoder.of_desc,128,"%s",array[1]);
		  }
		  g_strfreev(array);
		  break;
		}
		g_strfreev(array);
	      }
	    }
	    g_list_free_strings(ofmt_all);
	    g_list_free (ofmt_all);
	  }
	}
      }

      get_pref_utf8("vid_load_dir",prefs->def_vid_load_dir,PATH_MAX);
      if (!strlen(prefs->def_vid_load_dir)) {
#if GLIB_CHECK_VERSION(2,14,0)
	g_snprintf(prefs->def_vid_load_dir,PATH_MAX,"%s",g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
#else
	g_snprintf(prefs->def_vid_load_dir,PATH_MAX,"%s",capable->home_dir);
#endif
	set_pref("vid_load_dir",prefs->def_vid_load_dir);
      }
      g_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",prefs->def_vid_load_dir);
      ensure_isdir(mainw->vid_load_dir);

      get_pref_utf8("vid_save_dir",prefs->def_vid_save_dir,PATH_MAX);
      if (!strlen(prefs->def_vid_save_dir)) {
#if GLIB_CHECK_VERSION(2,14,0)
	g_snprintf(prefs->def_vid_save_dir,PATH_MAX,"%s",g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
#else
	g_snprintf(prefs->def_vid_save_dir,PATH_MAX,"%s",capable->home_dir);
#endif
	set_pref("vid_save_dir",prefs->def_vid_save_dir);
      }
      g_snprintf(mainw->vid_save_dir,PATH_MAX,"%s",prefs->def_vid_save_dir);
      ensure_isdir(mainw->vid_save_dir);

      g_snprintf(mainw->vid_dl_dir,PATH_MAX,"%s",mainw->vid_save_dir);
      
      get_pref_utf8("audio_dir",prefs->def_audio_dir,PATH_MAX);
      if (!strlen(prefs->def_audio_dir)) {
#if GLIB_CHECK_VERSION(2,14,0)
	g_snprintf(prefs->def_audio_dir,PATH_MAX,"%s",g_get_user_special_dir(G_USER_DIRECTORY_MUSIC));
#else
	g_snprintf(prefs->def_audio_dir,PATH_MAX,"%s",capable->home_dir);
#endif
	set_pref("audio_dir",prefs->def_audio_dir);
      }
      g_snprintf(mainw->audio_dir,PATH_MAX,"%s",prefs->def_audio_dir);
      ensure_isdir(mainw->audio_dir);
      
      get_pref_utf8("image_dir",prefs->def_image_dir,PATH_MAX);
      if (!strlen(prefs->def_image_dir)) {
#if GLIB_CHECK_VERSION(2,14,0)
	g_snprintf(prefs->def_image_dir,PATH_MAX,"%s",g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));
#else
	g_snprintf(prefs->def_image_dir,PATH_MAX,"%s",capable->home_dir);
#endif
	set_pref("image_dir",prefs->def_image_dir);
      }
      g_snprintf(mainw->image_dir,PATH_MAX,"%s",prefs->def_image_dir);
      ensure_isdir(mainw->image_dir);

      get_pref_utf8("proj_dir",prefs->def_proj_dir,PATH_MAX);
      if (!strlen(prefs->def_proj_dir)) {
	g_snprintf(prefs->def_proj_dir,PATH_MAX,"%s",capable->home_dir);
	set_pref("proj_dir",prefs->def_proj_dir);
      }
      g_snprintf(mainw->proj_load_dir,PATH_MAX,"%s",prefs->def_proj_dir);
      ensure_isdir(mainw->proj_load_dir);
      g_snprintf(mainw->proj_save_dir,PATH_MAX,"%s",mainw->proj_load_dir);
      
      prefs->show_player_stats=get_boolean_pref ("show_player_stats");
      
      prefs->dl_bandwidth=get_int_pref ("dl_bandwidth_K");
      prefs->fileselmax=get_boolean_pref("filesel_maximised");

      prefs->midisynch=get_boolean_pref ("midisynch");
      if (prefs->midisynch&&!capable->has_midistartstop) {
	set_boolean_pref("midisynch",FALSE);
	prefs->midisynch=FALSE;
      }
      

      prefs->discard_tv=FALSE;
      
      // conserve disk space ?
      prefs->conserve_space=get_boolean_pref ("conserve_space");
      prefs->ins_resample=get_boolean_pref ("insert_resample");
      
      // need better control of audio channels first
      prefs->pause_during_pb=FALSE;

      // should we always use the last directory ?
      // TODO - add to GUI
      prefs->save_directories=get_boolean_pref ("save_directories");
      prefs->antialias=get_boolean_pref("antialias");

      prefs->concat_images=get_boolean_pref("concat_images");

      prefs->safer_preview=TRUE;

      prefs->fxdefsfile=NULL;
      prefs->fxsizesfile=NULL;
      
      needs_free=FALSE;
      weed_plugin_path=getenv("WEED_PLUGIN_PATH");
      if (weed_plugin_path==NULL) {
	get_pref("weed_plugin_path",prefs->weed_plugin_path,PATH_MAX);
	if (strlen(prefs->weed_plugin_path)==0) weed_plugin_path=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_WEED_FX_BUILTIN,NULL);
	else weed_plugin_path=g_strdup(prefs->weed_plugin_path);
	lives_setenv("WEED_PLUGIN_PATH",weed_plugin_path);
	needs_free=TRUE;
      }
      snprintf(prefs->weed_plugin_path,PATH_MAX,"%s",weed_plugin_path);
      if (needs_free) g_free(weed_plugin_path);
      
      needs_free=FALSE;
      frei0r_path=getenv("FREI0R_PATH");
      if (frei0r_path==NULL) {
	get_pref("frei0r_path",prefs->frei0r_path,PATH_MAX);
	if (strlen(prefs->frei0r_path)==0) frei0r_path=g_strdup_printf("/usr/lib/frei0r-1:/usr/local/lib/frei0r-1:%s/frei0r-1",capable->home_dir);
	else frei0r_path=g_strdup(prefs->frei0r_path);
	lives_setenv("FREI0R_PATH",frei0r_path);
	needs_free=TRUE;
      }
      snprintf(prefs->frei0r_path,PATH_MAX,"%s",frei0r_path);
      if (needs_free) g_free(frei0r_path);
      
      needs_free=FALSE;
      ladspa_path=getenv("LADSPA_PATH");
      if (ladspa_path==NULL||strlen(ladspa_path)==0) {
	get_pref("ladspa_path",prefs->ladspa_path,PATH_MAX);
	if (strlen(prefs->ladspa_path)==0) ladspa_path=g_build_filename(prefs->lib_dir,"ladspa",NULL);
	else ladspa_path=g_strdup(prefs->ladspa_path);
	lives_setenv("LADSPA_PATH",ladspa_path);
	needs_free=TRUE;
      }
      snprintf(prefs->ladspa_path,PATH_MAX,"%s",ladspa_path);
      if (needs_free) g_free(ladspa_path);

      splash_msg(_("Loading realtime effect plugins..."),.6);
      weed_load_all();

      // replace any multi choice effects with their delegates
      replace_with_delegates();

      threaded_dialog_spin();
      load_default_keymap();
      threaded_dialog_spin();

      prefs->audio_opts=get_int_pref("audio_opts");
#ifdef ENABLE_JACK
      g_snprintf(prefs->jack_aserver,256,"%s/.jackdrc",capable->home_dir);
      g_snprintf(prefs->jack_tserver,256,"%s/.jackdrc",capable->home_dir);

#endif

      get_pref("current_autotrans",buff,256);
      if (strlen(buff)==0) prefs->atrans_fx=-1;
      else prefs->atrans_fx=weed_get_idx_for_hashname(buff,FALSE);

      if ((prefs->startup_phase==1||prefs->startup_phase==-1)) {
	splash_end();
	// get initial tempdir
	if (!do_tempdir_query()) {
	  lives_exit();
	}
	prefs->startup_phase=2;
	set_int_pref("startup_phase",2);
      }


      if (prefs->startup_phase>0&&prefs->startup_phase<3) {
	splash_end();
	if (!do_startup_tests(FALSE)) {
	  lives_exit();
	}
	prefs->startup_phase=3;
	set_int_pref("startup_phase",3);
      }


      if (capable->has_jackd) naudp++;
      if (capable->has_pulse_audio) naudp++;
      if (capable->has_sox_play) naudp++;
      if (capable->has_mplayer) naudp++;

      if (naudp>1) {
	if (prefs->startup_phase>0&&prefs->startup_phase<=4) {
	  splash_end();
	  if (!do_audio_choice_dialog(prefs->startup_phase)) {
	    lives_exit();
	  }
	  if (prefs->audio_player==AUD_PLAYER_JACK) future_prefs->jack_opts=prefs->jack_opts=JACK_OPTS_START_ASERVER;
	  else future_prefs->jack_opts=prefs->jack_opts=0;
	  set_int_pref("jack_opts",prefs->jack_opts);

	  prefs->startup_phase=4;
	  set_int_pref("startup_phase",4);
	}

#ifdef ENABLE_JACK
	if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER||prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT||prefs->jack_opts&JACK_OPTS_START_ASERVER||prefs->jack_opts&JACK_OPTS_START_TSERVER) {
	  // start jack transport polling
	  if (prefs->jack_opts&JACK_OPTS_START_ASERVER) splash_msg(_("Starting jack audio server..."),.8);
	  else {
	    if (prefs->jack_opts&JACK_OPTS_START_TSERVER) splash_msg(_("Starting jack transport server..."),.8);
	    else splash_msg(_("Connecting to jack transport server..."),.8);
	  }
	  if (!lives_jack_init()) {
	    if ((prefs->jack_opts&JACK_OPTS_START_ASERVER)||(prefs->jack_opts&JACK_OPTS_START_TSERVER)) do_jack_noopen_warn();
	    else do_jack_noopen_warn3();
	    if (prefs->startup_phase==4) {
	      do_jack_noopen_warn2();
	    }
	    future_prefs->jack_opts=0; // jack is causing hassle, get rid of it
	    set_int_pref("jack_opts",0);
	    lives_exit();
	  }
	}

	if (prefs->audio_player==AUD_PLAYER_JACK) {
	  jack_audio_init();
	  jack_audio_read_init();
	  mainw->jackd=jack_get_driver(0,TRUE);
	  if (mainw->jackd!=NULL) {
	    if (jack_open_device(mainw->jackd)) mainw->jackd=NULL;

	    if (((!(prefs->jack_opts&JACK_OPTS_START_TSERVER)&&!(prefs->jack_opts&JACK_OPTS_START_ASERVER))||
		 mainw->jackd==NULL)&&prefs->startup_phase==0) {
#ifdef HAVE_PULSE_AUDIO
	      gchar *otherbit="\"lives -aplayer pulse\".";
#else
	      gchar *otherbit="\"lives -aplayer sox\".";
#endif
	      gchar *tmp;

	      gchar *msg=g_strdup_printf(_("\n\nManual start of jackd required. Please make sure jackd is running, \nor else change the value of <jack_opts> in %s to 16\nand restart LiVES.\n\nAlternatively, try to start lives with either \"lives -jackopts 16\", or "),(tmp=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
	      g_printerr("%s%s\n\n",msg,otherbit);
	      g_free(msg);
	      g_free(tmp);

	    }

	    if (mainw->jackd==NULL) {
	      do_jack_noopen_warn3();
	      if (prefs->startup_phase==4) {
		do_jack_noopen_warn2();
	      }
	      else do_jack_noopen_warn4();
	      lives_exit();
	    }

	    mainw->jackd->whentostop=&mainw->whentostop;
	    mainw->jackd->cancelled=&mainw->cancelled;
	    mainw->jackd->in_use=FALSE;
	    mainw->jackd->play_when_stopped=(prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE;
	  }
	}
#endif
      }


#ifdef HAVE_PULSE_AUDIO
	if (prefs->audio_player==AUD_PLAYER_PULSE) {
	  splash_msg(_("Starting pulse audio server..."),.8);

	  if (!lives_pulse_init(prefs->startup_phase)) {
	    if (prefs->startup_phase==4) {
	      lives_exit();
	    }
	  }
	  else {
	    pulse_audio_init();
	    pulse_audio_read_init();
	    mainw->pulsed=pulse_get_driver(TRUE);
	    
	    mainw->pulsed->whentostop=&mainw->whentostop;
	    mainw->pulsed->cancelled=&mainw->cancelled;
	    mainw->pulsed->in_use=FALSE;
	  }
	}
#endif

    }


    if (prefs->startup_phase!=0) {
      gchar *txt;

      splash_end();
      set_int_pref("startup_phase",5);
      prefs->startup_phase=5;
      do_startup_interface_query();
      txt=get_new_install_msg();
      startup_message_nonfatal(txt);
      g_free(txt);

      set_int_pref("startup_phase",100); // tell backend to delete this
      prefs->startup_phase=100;
    }

    if (mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) mainw->vpp->audio_codec=get_best_audio(mainw->vpp);

    // toolbar buttons
    lives_widget_set_bg_color (mainw->tb_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->toolbar, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_stopbutton, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_bckground, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_sepwin, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_double, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_fullscreen, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_faster, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_slower, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_forward, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_back, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    lives_widget_set_bg_color (mainw->t_infobutton, LIVES_WIDGET_STATE_PRELIGHT, &palette->fade_colour);
    

  }
}



void do_start_messages(void) {
  gchar *endian;

  d_print("\n");
  d_print(_("Checking optional dependencies:"));
  if (capable->has_mplayer) d_print(_ ("mplayer...detected..."));
  else d_print(_ ("mplayer...NOT DETECTED..."));
  if (capable->has_convert) d_print(_ ("convert...detected..."));
  else d_print(_ ("convert...NOT DETECTED..."));
  if (capable->has_composite) d_print(_ ("composite...detected..."));
  else d_print(_ ("composite...NOT DETECTED..."));
  if (capable->has_sox_sox) d_print(_ ("sox...detected\n"));
  else d_print(_ ("sox...NOT DETECTED\n"));
  if (capable->has_cdda2wav) d_print(_ ("cdda2wav...detected..."));
  else d_print(_ ("cdda2wav...NOT DETECTED..."));
  if (capable->has_jackd) d_print(_ ("jackd...detected..."));
  else d_print(_ ("jackd...NOT DETECTED..."));
  if (capable->has_pulse_audio) d_print(_ ("pulse audio...detected..."));
  else d_print(_ ("pulse audio...NOT DETECTED..."));
  if (capable->has_python) d_print(_ ("python...detected..."));
  else d_print(_ ("python...NOT DETECTED..."));
  if (capable->has_dvgrab) d_print(_ ("dvgrab...detected..."));
  else d_print(_ ("dvgrab...NOT DETECTED..."));
  if (capable->has_xwininfo) d_print(_ ("xwininfo...detected..."));
  else d_print(_ ("xwininfo...NOT DETECTED..."));

#ifdef GDK_WINDOWING_X11
  prefs->wm=g_strdup(gdk_x11_screen_get_window_manager_name (gdk_screen_get_default()));
#else
#ifdef IS_MINGW
  prefs->wm=g_strdup_printf(_("Windows version %04X"),WINVER);
#else
  prefs->wm=g_strdup((_("UNKNOWN - please patch me !")));
#endif
#endif

  g_snprintf(mainw->msg,512,_ ("\n\nWindow manager reports as \"%s\"; "),prefs->wm);
  d_print(mainw->msg);

  g_snprintf(mainw->msg,512,_("number of monitors detected: %d\n"),capable->nmonitors);
  d_print(mainw->msg);

  g_snprintf(mainw->msg,512,_("Number of CPUs detected: %d "),capable->ncpus);
  d_print(mainw->msg);

  if (capable->byte_order==LIVES_LITTLE_ENDIAN) endian=g_strdup(_("little endian"));
  else endian=g_strdup(_("big endian"));
  g_snprintf(mainw->msg,512,_("(%d bits, %s)\n"),capable->cpu_bits,endian);
  d_print(mainw->msg);
  g_free(endian);

  g_snprintf(mainw->msg,512,"%s",_("GUI type is: "));
  d_print(mainw->msg);

#ifdef GUI_GTK
  g_snprintf(mainw->msg,512,_("GTK+ "
#if GTK_CHECK_VERSION(3,0,0)
"version %d.%d.%d ("
#endif
"compiled with %d.%d.%d"
#if GTK_CHECK_VERSION(3,0,0)
")"
#endif
),
#if GTK_CHECK_VERSION(3,0,0)
	     gtk_get_major_version(),
	     gtk_get_minor_version(),
	     gtk_get_micro_version(),
#endif
	     GTK_MAJOR_VERSION,
	     GTK_MINOR_VERSION,
	     GTK_MICRO_VERSION
	     );
  d_print(mainw->msg);
#endif

#ifdef PAINTER_CAIRO
  g_snprintf(mainw->msg,512,"%s",_(", with cairo support"));
  d_print(mainw->msg);
#endif

  g_snprintf(mainw->msg,512,"\n");
  d_print(mainw->msg);

  g_snprintf(mainw->msg,512,_ ("Temp directory is %s\n"),prefs->tmpdir);
  d_print(mainw->msg);

#ifndef RT_AUDIO
  d_print(_("WARNING - this version of LiVES was compiled without either\njack or pulse audio support.\nMany audio features will be unavailable.\n"));
# else
#ifdef ENABLE_JACK
  d_print(_("Compiled with jack support, good !\n"));
#endif
#ifdef HAVE_PULSE_AUDIO
  d_print(_("Compiled with pulse audio support, wonderful !\n"));
#endif
#endif

  g_snprintf(mainw->msg,512,_ ("Welcome to LiVES version %s.\n\n"),LiVES_VERSION);
  d_print(mainw->msg);

}




// TODO - allow user definable themes
void set_palette_colours (void) {

  // set configurable colours and theme colours for the app
  lives_color_parse ("black", &palette->black);
  lives_color_parse ("white", &palette->white);
  lives_color_parse ("SeaGreen3", &palette->light_green);
  lives_color_parse ("dark red", &palette->dark_red);
  lives_color_parse ("light blue", &palette->light_blue);
  lives_color_parse ("light yellow", &palette->light_yellow);
  lives_color_parse ("grey10", &palette->grey20);
  lives_color_parse ("grey25", &palette->grey25);
  lives_color_parse ("grey45", &palette->grey45);
  lives_color_parse ("grey60", &palette->grey60);
  lives_color_parse ("pink", &palette->pink);
  lives_color_parse ("salmon", &palette->light_red);
  lives_color_parse ("DarkOrange4", &palette->dark_orange);

  lives_widget_color_copy(&palette->banner_fade_text,&palette->white);
  palette->style=STYLE_PLAIN;

  // STYLE_PLAIN will overwrite this
  if (!(strcmp(prefs->theme,"pinks"))) {
    
    palette->normal_back.red=LIVES_WIDGET_COLOR_SCALE_255(228.);
    palette->normal_back.green=LIVES_WIDGET_COLOR_SCALE_255(196.);
    palette->normal_back.blue=LIVES_WIDGET_COLOR_SCALE_255(196.);
#if GTK_CHECK_VERSION(3,0,0)
    palette->normal_back.alpha=1.;
#endif      
    lives_widget_color_copy(&palette->normal_fore,&palette->black);
    lives_widget_color_copy(&palette->menu_and_bars,&palette->pink);
    lives_widget_color_copy(&palette->info_text,&palette->normal_fore);
    lives_widget_color_copy(&palette->info_base,&palette->normal_back);
    palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_5;
  }
  else {
    if (!(strcmp(prefs->theme,"cutting_room"))) {

      palette->normal_back.red=LIVES_WIDGET_COLOR_SCALE_255(224.);
      palette->normal_back.green=LIVES_WIDGET_COLOR_SCALE_255(224.);
      palette->normal_back.blue=LIVES_WIDGET_COLOR_SCALE_255(128.);
#if GTK_CHECK_VERSION(3,0,0)
      palette->normal_back.alpha=1.;
#endif      
	  
      lives_widget_color_copy(&palette->normal_fore,&palette->black);
      lives_widget_color_copy(&palette->menu_and_bars,&palette->white);
      lives_widget_color_copy(&palette->info_text,&palette->normal_fore);
      lives_widget_color_copy(&palette->info_base,&palette->white);
      palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
    }
    else {
      if (!(strcmp(prefs->theme,"camera"))) {

	palette->normal_back.red=LIVES_WIDGET_COLOR_SCALE_255(30.);
	palette->normal_back.green=LIVES_WIDGET_COLOR_SCALE_255(144.);
	palette->normal_back.blue=LIVES_WIDGET_COLOR_SCALE_255(232.);
#if GTK_CHECK_VERSION(3,0,0)
	palette->normal_back.alpha=1.;
#endif      
	  
	lives_widget_color_copy(&palette->normal_fore,&palette->black);
	lives_widget_color_copy(&palette->menu_and_bars,&palette->white);
	lives_widget_color_copy(&palette->info_base,&palette->normal_back);
	lives_widget_color_copy(&palette->info_text,&palette->normal_fore);
	palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
      }
      else {
	if (!(strcmp(prefs->theme,"editor"))) {
	  lives_widget_color_copy(&palette->normal_back,&palette->grey25);
	  lives_widget_color_copy(&palette->normal_fore,&palette->white);
	  lives_widget_color_copy(&palette->menu_and_bars,&palette->grey60);
	  lives_widget_color_copy(&palette->info_base,&palette->grey20);
	  lives_widget_color_copy(&palette->info_text,&palette->white);
	  palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_5;
	}
	else {
	  if (!(strcmp(prefs->theme,"crayons-bright"))) {
	    lives_widget_color_copy(&palette->normal_back,&palette->black);
	    lives_widget_color_copy(&palette->normal_fore,&palette->white);

	    palette->menu_and_bars.red=LIVES_WIDGET_COLOR_SCALE_255(225.);
	    palette->menu_and_bars.green=LIVES_WIDGET_COLOR_SCALE_255(160.);
	    palette->menu_and_bars.blue=LIVES_WIDGET_COLOR_SCALE_255(80.);
#if GTK_CHECK_VERSION(3,0,0)
	    palette->menu_and_bars.alpha=1.;
#endif      

	    palette->info_base.red=LIVES_WIDGET_COLOR_SCALE_255(200.);
	    palette->info_base.green=LIVES_WIDGET_COLOR_SCALE_255(190.);
	    palette->info_base.blue=LIVES_WIDGET_COLOR_SCALE_255(52.);
#if GTK_CHECK_VERSION(3,0,0)
	    palette->info_base.alpha=1.;
#endif      
	      
	    lives_widget_color_copy(&palette->info_text,&palette->black);

	    palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
	  }
	  else {
	    if (!(strcmp(prefs->theme,"crayons"))) {
	      lives_widget_color_copy(&palette->normal_back,&palette->grey25);
	      lives_widget_color_copy(&palette->normal_fore,&palette->white);
	      lives_widget_color_copy(&palette->menu_and_bars,&palette->grey60);
	      lives_widget_color_copy(&palette->info_base,&palette->grey20);
	      lives_widget_color_copy(&palette->info_text,&palette->white);

	      palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_5;

	    }
	    else {
	      palette->style=STYLE_PLAIN;
	    }}}}}}
}


static boolean is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return FALSE;
  return TRUE;
}


capability *get_capabilities (void) {
  // get capabilities of backend system
  gchar *safer_bfile;
  gchar **array;
  
  gchar buffer[4096+PATH_MAX];
  FILE *bootfile;
  gchar string[256];
  int err;
  gint numtok;
  gchar *tmp;

#ifdef IS_DARWIN
  processor_info_array_t processorInfo;
  mach_msg_type_number_t numProcessorInfo;
  natural_t numProcessors = 0U;
  kern_return_t kerr;
#else
#ifndef IS_MINGW
  size_t len;
  FILE *tfile;
#endif
#endif

  capable=(capability *)g_malloc(sizeof(capability));


  // this is _compile time_ bits, not runtime bits
  capable->cpu_bits=32;
  if (sizeof(void *)==8) capable->cpu_bits=64;

  // _runtime_ byte order
  if (is_big_endian()) 
    capable->byte_order=LIVES_BIG_ENDIAN;
  else 
    capable->byte_order=LIVES_LITTLE_ENDIAN;

  capable->has_smogrify=FALSE;
  capable->smog_version_correct=FALSE;

  // required
  capable->can_write_to_tmp=FALSE;
  capable->can_write_to_tempdir=FALSE;
  capable->can_write_to_config=FALSE;
  capable->can_read_from_config=FALSE;

#ifndef IS_MINGW
  g_snprintf(capable->home_dir,PATH_MAX,"%s",g_get_home_dir());
#else
  // TODO/REG - we will get from registry

  g_snprintf(capable->home_dir,PATH_MAX,"%s\\Application Data\\LiVES",g_get_home_dir());
#endif

  g_snprintf(capable->system_tmpdir,PATH_MAX,"%s",g_get_tmp_dir());

  memset(capable->startup_msg,0,1);

  // optional
  capable->has_mplayer=FALSE;
  capable->has_convert=FALSE;
  capable->has_composite=FALSE;
  capable->has_identify=FALSE;
  capable->has_sox_play=FALSE;
  capable->has_sox_sox=FALSE;
  capable->has_dvgrab=FALSE;
  capable->has_cdda2wav=FALSE;
  capable->has_autolives=FALSE;
  capable->has_jackd=FALSE;
  capable->has_gdb=FALSE;
  capable->has_pulse_audio=FALSE;
  capable->has_xwininfo=FALSE;
  capable->has_midistartstop=FALSE;
  capable->has_encoder_plugins=FALSE;
  capable->has_python=FALSE;
  capable->python_version=0;
  capable->has_stderr=TRUE;
  capable->has_gconftool_2=FALSE;
  capable->has_xdg_screensaver=FALSE;

#ifndef IS_MINGW
  safer_bfile=g_strdup_printf("%s"G_DIR_SEPARATOR_S".smogrify.%d.%d",capable->system_tmpdir,lives_getuid(),lives_getgid());
#else
  safer_bfile=g_strdup_printf("%s"G_DIR_SEPARATOR_S"smogrify.%d.%d",capable->system_tmpdir,lives_getuid(),lives_getgid());
#endif
  unlink (safer_bfile);

  // check that we can write to /tmp
  if (!check_file (safer_bfile,FALSE)) return capable;
  capable->can_write_to_tmp=TRUE;

#ifndef IS_MINGW
  g_snprintf(prefs->backend_sync,PATH_MAX,"%s","smogrify");
  g_snprintf(prefs->backend,PATH_MAX,"%s","smogrify");
  if ((tmp=g_find_program_in_path ("smogrify"))==NULL) return capable;
  g_free(tmp);

  g_snprintf(string,256,"%s report \"%s\" 2>/dev/null",prefs->backend_sync,
	     (tmp=g_filename_from_utf8 (safer_bfile,-1,NULL,NULL,NULL)));

  g_free(tmp);

  g_snprintf(string,256,"%s report \"%s\" 2>/dev/null",prefs->backend_sync,
	     (tmp=g_filename_from_utf8 (safer_bfile,-1,NULL,NULL,NULL)));
#else

  g_snprintf(prefs->backend_sync,PATH_MAX,"perl \"%s\\smogrify\"",prefs->prefix_dir);
  g_snprintf(prefs->backend,PATH_MAX,"START /MIN /B perl \"%s\\smogrify\"",prefs->prefix_dir);

  g_snprintf(string,256,"%s report \"%s\" 2>NUL",prefs->backend_sync,
	     (tmp=g_filename_from_utf8 (safer_bfile,-1,NULL,NULL,NULL)));

#endif


  g_free(tmp);

  err=system(string);

  if (err==32512||err==32256) {
    return capable;
  }
  
  capable->has_smogrify=TRUE;

  if (err==512) {
    return capable;
  }

  capable->can_read_from_config=TRUE;

  if (err==768) {
    return capable;
  }

  capable->can_write_to_config=TRUE;

  if (err==1024) {
    return capable;
  }


  if (err!=1280) {
    capable->can_write_to_tempdir=TRUE;
  }

  if (!(bootfile=fopen(safer_bfile,"r"))) {
    g_free(safer_bfile);
    return capable;
  }

  mainw->read_failed=FALSE;
  lives_fgets(buffer,8192,bootfile);
  fclose(bootfile);

  unlink (safer_bfile);
  g_free(safer_bfile);

  if (mainw->read_failed) return capable;

  // get backend version, tempdir, and any startup message
  numtok=get_token_count (buffer,'|');
  array=g_strsplit(buffer,"|",numtok);

  g_snprintf(string,256,"%s",array[0]);

  if (strcmp(string,LiVES_VERSION)) {
    g_strfreev(array);
    return capable;
  }

  capable->smog_version_correct=TRUE;

  g_snprintf(prefs->tmpdir,PATH_MAX,"%s",array[1]);
  g_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);

  prefs->startup_phase=atoi (array[2]);

  if (numtok>3&&strlen (array[3])) {
    if (!strcmp(array[3],"!updmsg")) {
      gchar *text=get_upd_msg();
      g_snprintf(capable->startup_msg,256,"%s\n\n",text);
      g_free(text);

      if (numtok>4&&strlen (array[4])) {
	g_strappend (capable->startup_msg,256,array[4]);
      }
    }
    else {
      g_snprintf(capable->startup_msg,256,"%s\n\n",array[3]);
      if (numtok>4&&strlen (array[4])) {
	g_strappend (capable->startup_msg,256,array[4]);
      }
    }
  }
  g_strfreev(array);

  if (!capable->can_write_to_tempdir) return capable;

  get_location("mplayer",string,256);
  if (strlen(string)) capable->has_mplayer=TRUE;

#ifndef IS_MINGW
  get_location("convert",string,256);
#else
  get_location("mgkvert",string,256);
#endif
  if (strlen(string)) capable->has_convert=TRUE;

  get_location("composite",string,256);
  if (strlen(string)) capable->has_composite=TRUE;

  get_location("identify",string,256);
  if (strlen(string)) capable->has_identify=TRUE;

  ///////////////////////////////////////////////////////

  get_location("play",string,256);
  if (strlen(string)) capable->has_sox_play=TRUE;

  get_location("sox",string,256);
  if (strlen(string)) capable->has_sox_sox=TRUE;

  get_location("dvgrab",string,256);
  if (strlen(string)) capable->has_dvgrab=TRUE;

  get_location("cdda2wav",string,256);
  if (strlen(string)) capable->has_cdda2wav=TRUE;

  get_location("jackd",string,256);
  if (strlen(string)) capable->has_jackd=TRUE;

  get_location("gdb",string,256);
  if (strlen(string)) capable->has_gdb=TRUE;

  get_location("pulseaudio",string,256);
  if (strlen(string)) capable->has_pulse_audio=TRUE;

  get_location("python",string,256);
  if (strlen(string)) {
    capable->has_python=TRUE;
    capable->python_version=get_version_hash("python -V 2>&1"," ",1);
  }

  get_location("xwininfo",string,256);
  if (strlen(string)) capable->has_xwininfo=TRUE;

  get_location("gconftool-2",string,256);
  if (strlen(string)) capable->has_gconftool_2=TRUE;

  get_location("xdg-screensaver",string,256);
  if (strlen(string)) capable->has_xdg_screensaver=TRUE;

  get_location("midistart",string,256);
  if (strlen(string)) {
    get_location("midistop",string,256);
    if (strlen(string)) {
      capable->has_midistartstop=TRUE;
    }
  }

  capable->ncpus=0;

#ifdef IS_DARWIN
  kerr = host_processor_info(mach_host_self(), PROCESSOR_BASIC_INFO, &numProcessors, &processorInfo, &numProcessorInfo);
  if (kerr == KERN_SUCCESS) {
    vm_deallocate(mach_task_self(), (vm_address_t) processorInfo, numProcessorInfo * sizint);
  }
  capable->ncpus=(gint)numProcessors;
#else

#ifdef IS_MINGW
  // FIXME:

  //capable->ncpus=lives_win32_get_num_logical_cpus();
#else

  tfile=popen("cat /proc/cpuinfo 2>/dev/null | grep processor 2>/dev/null | wc -l 2>/dev/null","r");
  len=fread((void *)buffer,1,1024,tfile);
  pclose(tfile);

  memset(buffer+len,0,1);
  capable->ncpus=atoi(buffer);
#endif
#endif

  if (capable->ncpus==0) capable->ncpus=1;

  return capable;
}

void print_notice() {
  g_printerr("\nLiVES %s\n",LiVES_VERSION);
  g_printerr("Copyright 2002-2013 Gabriel Finch (salsaman@gmail.com) and others.\n");
  g_printerr("LiVES comes with ABSOLUTELY NO WARRANTY\nThis is free software, and you are welcome to redistribute it\nunder certain conditions; see the file COPYING for details.\n\n");
}


void print_opthelp(void) {
  print_notice();
  g_printerr(_("\nStartup syntax is: %s [opts] [filename [start_time] [frames]]\n"),capable->myname);
  g_printerr("%s",_("Where: filename is the name of a media file or backup file.\n"));
  g_printerr("%s",_("start_time : filename start time in seconds\n"));
  g_printerr("%s",_("frames : maximum number of frames to open\n"));
  g_printerr("%s","\n");
  g_printerr("%s",_("opts can be:\n"));
  g_printerr("%s",_("-help            : show this help text and exit\n"));
  g_printerr("%s",_("-tmpdir <tempdir>: use alternate working directory (e.g /var/ramdisk)\n"));
  g_printerr("%s",_("-set <setname>   : autoload clip set setname\n"));
  g_printerr("%s",_("-noset           : do not load any set on startup\n"));
  g_printerr("%s",_("-norecover       : force no-loading of crash recovery\n"));
  g_printerr("%s",_("-recover         : force loading of crash recovery\n"));
  g_printerr("%s",_("-nothreaddialog  : doe nothing - retained for backwards compatibility\n"));
  g_printerr("%s",_("-nogui           : do not show the gui\n"));
  g_printerr("%s",_("-nosplash        : do not show the splash window\n"));
  g_printerr("%s",_("-noplaywin       : do not show the play window\n"));
  g_printerr("%s",_("-startup-ce      : start in clip editor mode\n"));
  g_printerr("%s",_("-startup-mt      : start in multitrack mode\n"));
  g_printerr("%s",_("-fxmodesmax <n>  : allow <n> modes per effect key (minimum is 1, default is 8)\n"));
#ifdef ENABLE_OSC
  g_printerr("%s",_("-oscstart <port> : start OSC listener on UDP port <port>\n"));
  g_printerr("%s",_("-nooscstart      : do not start OSC listener\n"));
#endif
  g_printerr("%s",_("-aplayer <ap>    : start with selected audio player. <ap> can be mplayer"));
#ifdef HAVE_PULSE_AUDIO
  // TRANSLATORS: pulse (audio)
  g_printerr("%s",_(", pulse"));
#endif
#ifdef ENABLE_JACK
  g_printerr("%s",_(", sox or jack\n"));
  g_printerr("%s",_("-jackopts <opts>    : opts is a bitmap of jack startup options [1 = jack transport client, 2 = jack transport master, 4 = start jack transport server, 8 = pause audio when video paused, 16 = start jack audio server] \n"));
#else
  g_printerr("%s",_(" or sox\n"));
#endif
  g_printerr("%s",_("-devicemap <mapname>          : autoload devicemap\n"));
  g_printerr("%s",_("-vppdefaults <file>          : load video playback plugin defaults from <file> (Note: only sets the settings, not the plugin type)\n"));
  g_printerr("%s",_("-debug            : try to debug crashes (requires 'gdb' installed)\n"));

  g_printerr("%s","\n");
}

//// things to do - on startup
#ifdef HAVE_YUV4MPEG
static boolean open_yuv4m_startup(gpointer data) {
  on_open_yuv4m_activate(NULL,data);
  return FALSE;
}
#endif


/////////////////////////////////


static boolean lives_startup(gpointer data) {
  GError *gerr=NULL;
  boolean got_files=FALSE;
  gchar *tmp;
  gchar *icon;

  if (!mainw->foreign) {
    if (prefs->show_splash) splash_init();
    print_notice();
  }

  splash_msg(_("Starting GUI..."),0.);

  create_LiVES ();

  icon=g_build_filename(prefs->prefix_dir,DESKTOP_ICON_DIR,"lives.png",NULL);
  gtk_window_set_default_icon_from_file(icon,&gerr);
  g_free(icon);

  if (gerr!=NULL) g_error_free(gerr);

  gtk_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();

  mainw->startup_error=FALSE;

  if (capable->smog_version_correct) {
    if (theme_expected&&palette->style==STYLE_PLAIN&&!mainw->foreign) {
      // non-fatal errors
      gchar *tmp2;
      gchar *err=g_strdup_printf(_ ("\n\nThe theme you requested could not be located. Please make sure you have the themes installed in\n%s/%s.\n(Maybe you need to change the value of <prefix_dir> in your %s file)\n"),(tmp=g_filename_to_utf8(prefs->prefix_dir,-1,NULL,NULL,NULL)),THEME_DIR,(tmp2=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
      g_free(tmp2);
      g_free(tmp);
      startup_message_nonfatal (err);
      g_free(err);
      g_snprintf(prefs->theme,64,"none");
      upgrade_error=TRUE;
    }
    lives_init(&ign_opts);
  }

  if (!mainw->foreign) {
    // fatal errors
    if (!capable->can_write_to_tmp) {
      startup_message_fatal((tmp=g_strdup_printf(_ ("\nLiVES was unable to write a small file to %s\nPlease make sure you have write access to %s and try again.\n"),capable->system_tmpdir)));
      g_free(tmp);
    }
    else {
      if (!capable->has_smogrify) {
	gchar *err=g_strdup (_ ("\n`smogrify` must be in your path, and be executable\n\nPlease review the README file which came with this package\nbefore running LiVES.\n"));
	startup_message_fatal(err);
	g_free(err);
      }
      else {
	if (!capable->can_read_from_config) {
	  gchar *err=g_strdup_printf(_ ("\nLiVES was unable to read from its configuration file\n%s\n\nPlease check the file permissions for this file and try again.\n"),(tmp=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
	  g_free(tmp);
	  startup_message_fatal(err);
	  g_free(err);
	}
	else {
	  if (!capable->can_write_to_config) {
	    gchar *err=g_strdup_printf(_ ("\nLiVES was unable to write to its configuration file\n%s\n\nPlease check the file permissions for this file and directory\nand try again.\n"),(tmp=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
	    g_free(tmp);
	    startup_message_fatal(err);
	    g_free(err);
	  }
	  else {
	    if (!capable->can_write_to_tempdir) {
	      gchar *extrabit;
	      gchar *err;
	      if (!mainw->has_session_tmpdir) {
		extrabit=g_strdup_printf(_("Please check the <tempdir> setting in \n%s\nand try again.\n"),
				  (tmp=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
		g_free(tmp);
	      }
	      else 
		extrabit=g_strdup("");

	      err=g_strdup_printf(_ ("\nLiVES was unable to use the temporary directory\n%s\n\n%s"),
				  prefs->tmpdir,extrabit);
	      g_free(extrabit);
	      startup_message_fatal(err);
	      g_free(err);
	    }
	    else {
	      if (!capable->smog_version_correct) {
		startup_message_fatal(_ ("\nAn incorrect version of smogrify was found in your path.\n\nPlease review the README file which came with this package\nbefore running LiVES.\n\nThankyou.\n"));
	      }
	      else {
		#ifndef IS_MINGW
		if ((!capable->has_sox_sox||!capable->has_sox_play)&&!capable->has_mplayer){
		  startup_message_fatal(_ ("\nLiVES currently requires either 'mplayer' or 'sox' to function. Please install one or other of these, and try again.\n"));
		}
		#else
		if (!capable->has_sox_sox||!capable->has_mplayer){
		  startup_message_fatal(_ ("\nLiVES currently requires both 'mplayer' and 'sox' to function. Please install these, and try again.\n"));
		}
		#endif
		else {
		  if (strlen(capable->startup_msg)) {
		    startup_message_nonfatal (capable->startup_msg);
		  }
		  else {
		    // non-fatal errors
		    if (!capable->has_mplayer&&!(prefs->warning_mask&WARN_MASK_NO_MPLAYER)) {
		      startup_message_nonfatal_dismissable (_ ("\nLiVES was unable to locate 'mplayer'. You may wish to install mplayer to use LiVES more fully.\n"),WARN_MASK_NO_MPLAYER);
		    }
		    if (!capable->has_convert) {
		      startup_message_nonfatal_dismissable (_ ("\nLiVES was unable to locate 'convert'. You should install convert and image-magick if you want to use rendered effects.\n"),WARN_MASK_NO_MPLAYER);
		    }
		    if (!capable->has_composite) {
		      startup_message_nonfatal_dismissable (_ ("\nLiVES was unable to locate 'composite'. You should install composite and image-magick if you want to use the merge function.\n"),WARN_MASK_NO_MPLAYER);
		    }
		    if (!capable->has_sox_sox) {
		      startup_message_nonfatal_dismissable (_ ("\nLiVES was unable to locate 'sox'. Some audio features may not work. You should install 'sox'.\n"),WARN_MASK_NO_MPLAYER);
		    }
		    if (!capable->has_encoder_plugins) {
		      gchar *err=g_strdup_printf(_ ("\nLiVES was unable to find any encoder plugins.\nPlease check that you have them installed correctly in\n%s%s%s/\nYou will not be able to 'Save' without them.\nYou may need to change the value of <lib_dir> in %s\n"),prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,(tmp=g_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
		      g_free(tmp);
		      startup_message_nonfatal_dismissable (err,WARN_MASK_NO_ENCODERS);
		      g_free(err);
		      upgrade_error=TRUE;
		    }

		    if (mainw->next_ds_warn_level>0) {
		      guint64 dsval;
		      lives_storage_status_t ds=get_storage_status(prefs->tmpdir,mainw->next_ds_warn_level,&dsval);
		      if (ds==LIVES_STORAGE_STATUS_WARNING) {
			gchar *err;
			guint64 curr_ds_warn=mainw->next_ds_warn_level;
			mainw->next_ds_warn_level>>=1;
			if (mainw->next_ds_warn_level>(dsval>>1)) mainw->next_ds_warn_level=dsval>>1;
			if (mainw->next_ds_warn_level<prefs->ds_crit_level) mainw->next_ds_warn_level=prefs->ds_crit_level;
			tmp=ds_warning_msg(prefs->tmpdir,dsval,curr_ds_warn,mainw->next_ds_warn_level);
			err=g_strdup_printf("\n%s\n",tmp);
			g_free(tmp);
			startup_message_nonfatal(err);
			g_free(err);
		      }
		      else if (ds==LIVES_STORAGE_STATUS_CRITICAL) {
			gchar *err;
			tmp=ds_critical_msg(prefs->tmpdir,dsval);
			err=g_strdup_printf("\n%s\n",tmp);
			g_free(tmp);
			startup_message_fatal(err);
			g_free(err);
		      }
		    }
		  }

		  if (prefs->startup_interface!=STARTUP_MT) {
		    if (prefs->show_gui) {
		      gtk_widget_show (mainw->LiVES);
		    }
#ifdef ENABLE_OSC
		    lives_osc_notify(LIVES_OSC_NOTIFY_MODE_CHANGED,(tmp=g_strdup_printf("%d",STARTUP_MT)));
		    g_free(tmp);
#endif
		  }
		}}}}}}}}

  else {
    // capture mode
    mainw->foreign_key=atoi(zargv[2]);

#if GTK_CHECK_VERSION(3,0,0)
    mainw->foreign_id=(Window)atoi(zargv[3]);
#else
    mainw->foreign_id=(GdkNativeWindow)atoi(zargv[3]);
#endif
    mainw->foreign_width=atoi(zargv[4]);
    mainw->foreign_height=atoi(zargv[5]);
    g_snprintf(prefs->image_ext,16,"%s",zargv[6]);
    mainw->foreign_bpp=atoi(zargv[7]);
    mainw->rec_vid_frames=atoi(zargv[8]);
    mainw->rec_fps=strtod(zargv[9],NULL);
    mainw->rec_arate=atoi(zargv[10]);
    mainw->rec_asamps=atoi(zargv[11]);
    mainw->rec_achans=atoi(zargv[12]);
    mainw->rec_signed_endian=atoi(zargv[13]);

    if (zargc>14) {
      mainw->foreign_visual=g_strdup(zargv[14]);
      if (!strcmp(mainw->foreign_visual,"(null)")) {
	g_free(mainw->foreign_visual);
	mainw->foreign_visual=NULL;
      }
    }

#ifdef ENABLE_JACK
    if (prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd&&mainw->rec_achans>0) {
      jack_audio_read_init();
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio&&mainw->rec_achans>0) {
      pulse_audio_read_init();
    }
#endif
    gtk_widget_show (mainw->LiVES);
    on_capture2_activate();  // exits
  }

#ifdef NOTTY
  if (!mainw->foreign) {
    close (2);
    capable->has_stderr=FALSE;
  }
#endif

  do_start_messages();

  if (mainw->cached_list!=NULL) {
    g_list_free_strings(mainw->cached_list);
    g_list_free(mainw->cached_list);
    mainw->cached_list=NULL;
  }

  if (!prefs->show_gui) gtk_widget_hide(mainw->LiVES);

  if (prefs->startup_phase==100) {
#ifndef IS_MINGW
    if (upgrade_error) {
      do_upgrade_error_dialog();
    }
#endif
    prefs->startup_phase=0;
  }

  if (strlen (start_file)&&strcmp(start_file,"-")) {
    splash_end();
    deduce_file(start_file,start,end);
    got_files=TRUE;
  } else {
    set_main_title(NULL,0);
  }

  splash_end();

  if (prefs->crash_recovery&&!no_recover) got_files=check_for_recovery_files(auto_recover);

  if (!mainw->foreign&&!got_files&&prefs->ar_clipset) {
    gchar *msg=g_strdup_printf(_("Autoloading set %s..."),prefs->ar_clipset_name);
    d_print(msg);
    splash_msg(msg,1.);
    g_free(msg);
    g_snprintf(mainw->set_name,128,"%s",prefs->ar_clipset_name);
    on_load_set_ok(NULL,GINT_TO_POINTER(FALSE));
    if (mainw->current_file==-1) {
      set_pref("ar_clipset","");
      prefs->ar_clipset=FALSE;
    }
  }

#ifdef ENABLE_OSC
  if (prefs->osc_start) prefs->osc_udp_started=lives_osc_init(prefs->osc_udp_port);
#endif

  if (mainw->recoverable_layout) do_layout_recover_dialog();

  if (!prefs->show_gui&&prefs->startup_interface==STARTUP_CE) mainw->is_ready=TRUE;

  mainw->go_away=FALSE;

#if GTK_CHECK_VERSION(3,0,0)
  mainw->kb_timer_end=FALSE;
  mainw->kb_timer=g_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#else
  mainw->kb_timer=gtk_timeout_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);
#endif

#ifdef HAVE_YUV4MPEG
  if (strlen(prefs->yuvin)>0) g_idle_add(open_yuv4m_startup,NULL);
#endif

  gdk_window_add_filter(NULL, filter_func, NULL);

  return FALSE;
}



int main (int argc, char *argv[]) {
#ifdef ENABLE_OSC
#ifdef IS_MINGW
  WSADATA wsaData;
  int iResult;
#endif
#endif
  ssize_t mynsize;
  gchar fbuff[PATH_MAX];

#ifndef IS_MINGW
  sigset_t smask;

  struct sigaction sact;

  sigemptyset(&smask);

  sigaddset(&smask,LIVES_SIGINT);
  sigaddset(&smask,LIVES_SIGTERM);
  sigaddset(&smask,LIVES_SIGSEGV);
  sigaddset(&smask,LIVES_SIGABRT);

  sact.sa_handler=catch_sigint;
  sact.sa_flags=0;
  sact.sa_mask=smask;


  sigaction (LIVES_SIGINT, &sact, NULL);
  sigaction (LIVES_SIGTERM, &sact, NULL);
  sigaction (LIVES_SIGSEGV, &sact, NULL);
  sigaction (LIVES_SIGABRT, &sact, NULL);

#else
  typedef void (*SignalHandlerPointer)(int);

  SignalHandlerPointer previousHandler;

  previousHandler = signal(LIVES_SIGINT, catch_sigint);
  previousHandler = signal(LIVES_SIGTERM, catch_sigint);
  previousHandler = signal(LIVES_SIGSEGV, catch_sigint);
  previousHandler = signal(LIVES_SIGABRT, catch_sigint);

  previousHandler = previousHandler; // shut gcc up
#endif

  ign_opts.ign_clipset=ign_opts.ign_osc=ign_opts.ign_aplayer=ign_opts.ign_stmode=ign_opts.ign_vppdefs=FALSE;

#ifdef ENABLE_OIL
  oil_init();
#endif
  
  zargc=argc;
  zargv=argv;

  mainw=NULL;

#ifdef ENABLE_NLS
  trString=NULL;
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef UTF8_CHARSET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
  textdomain (GETTEXT_PACKAGE);
#endif

  // force decimal point to be a "."
  putenv ("LC_NUMERIC=C");

#ifdef ENABLE_OSC
#ifdef IS_MINGW

  // Initialize Winsock
  iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    exit (1);

  }
#endif
#endif

  gtk_init (&argc, &argv);

#ifdef LIVES_NO_DEBUG
  // don't crash on GTK+ fatals
  g_log_set_always_fatal ((GLogLevelFlags)0);
#endif

  g_log_set_default_handler(lives_log_handler,NULL);

  theme_expected=pre_init();

  // mainw->foreign is set if we are grabbing an external window
  mainw->foreign=FALSE;
  memset (start_file,0,1);
  mainw->has_session_tmpdir=FALSE;

#ifndef IS_MINGW
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S".info.%d",prefs->tmpdir,getpid());
#else
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S"info.%d",prefs->tmpdir,getpid());
#endif

  // what's my name ?
  capable->myname_full=g_find_program_in_path(argv[0]);

  if ((mynsize=lives_readlink(capable->myname_full,fbuff,PATH_MAX))!=-1) {
    // no. i mean, what's my real name ?
    memset(fbuff+mynsize,0,1);
    g_free(capable->myname_full);
    capable->myname_full=g_strdup(fbuff);
  }

  // what's my short name (without the path) ?
  g_snprintf(fbuff,PATH_MAX,"%s",capable->myname_full);
  get_basename(fbuff);
  capable->myname=g_strdup(fbuff);


  /* TRANSLATORS: localised name may be used here */
  g_set_application_name(_("LiVES"));


  // format is:
  // lives [opts] [filename [start_time] [frames]]

  if (argc>1) {
    if (!strcmp(argv[1],"-capture")) {
      // special mode for grabbing external window
      mainw->foreign=TRUE;
    }
    else if (!strcmp(argv[1],"-help")||!strcmp(argv[1],"--help")) {
      print_opthelp();
      exit (0);
    }
    else if (!strcmp(argv[1],"-version")||!strcmp(argv[1],"--version")) {
      print_notice();
      exit (0);
    }
    else {
      struct option longopts[] = {
	{"aplayer", 1, 0, 0},
	{"tmpdir", 1, 0, 0},
	{"yuvin", 1, 0, 0},
	{"set", 1, 0, 0},
	{"noset", 0, 0, 0},
#ifdef ENABLE_OSC
        {"devicemap", 1, 0, 0},
#endif
        {"vppdefaults", 1, 0, 0},
	{"recover", 0, 0, 0},
	{"norecover", 0, 0, 0},
	{"nothreaddialog", 0, 0, 0},
	{"nogui", 0, 0, 0},
	{"nosplash", 0, 0, 0},
	{"noplaywin", 0, 0, 0},
	{"startup-ce", 0, 0, 0},
	{"startup-mt", 0, 0, 0},
	{"debug", 0, 0, 0},
	{"bigendbug", 1, 0, 0},
	{"fxmodesmax", 1, 0, 0},
#ifdef ENABLE_OSC
	{"oscstart", 1, 0, 0},
	{"nooscstart", 0, 0, 0},
#endif
#ifdef ENABLE_JACK
	{"jackopts", 1, 0, 0},
#endif
	{0, 0, 0, 0}
      };
      int option_index=0;
      const char *charopt;
      int c;

      while (1) {
	c=getopt_long_only(argc,argv,"",longopts,&option_index);
	if (c==-1) break; // end of options
	charopt=longopts[option_index].name;
	if (!strcmp(charopt,"norecover")) {
	  // auto no-recovery
	  no_recover=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"debug")) {
	  // debug crashes
	  mainw->debug=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"recover")) {
	  // auto recovery
	  auto_recover=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"tmpdir")) {
	  mainw->has_session_tmpdir=TRUE;
	  // override tempdir setting
	  g_snprintf(prefs->tmpdir,PATH_MAX,"%s",optarg);
	  g_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);
	  set_pref("session_tempdir",prefs->tmpdir);

	  if (g_mkdir_with_parents(prefs->tmpdir,S_IRWXU)==-1) {
	    if (!check_dir_access(prefs->tmpdir)) {
	      // abort if we cannot create the new subdir
	      LIVES_ERROR("Could not create directory");
	      LIVES_ERROR(prefs->tmpdir);
	    }
	    capable->can_write_to_tempdir=FALSE;
	  }
	  else {
	    LIVES_INFO("Created directory");
	    LIVES_INFO(prefs->tmpdir);
	  }
	  mainw->com_failed=FALSE;
	  continue;
	}
	if (!strcmp(charopt,"yuvin")) {
#ifdef HAVE_YUV4MPEG
	  g_snprintf(prefs->yuvin,PATH_MAX,"%s",optarg);
	  prefs->startup_interface=STARTUP_CE;
	  ign_opts.ign_stmode=TRUE;
#else
	  LIVES_ERROR("Must have mjpegtools installed for yuvin to work");
#endif
	  continue;
	}
	if (!strcmp(charopt,"noset")) {
	  // override clipset loading
	  memset(prefs->ar_clipset_name,0,1);
	  prefs->ar_clipset=FALSE;
	  ign_opts.ign_clipset=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"set")&&optarg!=NULL) {
	  // force clipset loading
	  g_snprintf(prefs->ar_clipset_name,128,"%s",optarg);
	  prefs->ar_clipset=TRUE;
	  ign_opts.ign_clipset=TRUE;
	  continue;
	}
#ifdef ENABLE_OSC
        if (!strcmp(charopt,"devicemap")&&optarg!=NULL) {
          // force devicemap loading
          on_midi_load_activate(NULL, optarg);
          continue;
        }
#endif
        if (!strcmp(charopt,"vppdefaults")&&optarg!=NULL) {
          // load alternate vpp file
	  g_snprintf(mainw->vpp_defs_file,PATH_MAX,"%s",optarg);
	  ign_opts.ign_vppdefs=TRUE;
          continue;
        }
	if (!strcmp(charopt,"aplayer")) {
	  gchar buff[256];
	  boolean apl_valid=FALSE;

	  g_snprintf(buff,256,"%s",optarg);
	  // override aplayer default
	  if (!strcmp(buff,"sox")) {
	    switch_aud_to_sox(TRUE);
	    apl_valid=TRUE;
	  }
	  if (!strcmp(buff,"mplayer")) {
	    switch_aud_to_mplayer(TRUE);
	    apl_valid=TRUE;
	  }
	  if (!strcmp(buff,"jack")) {
#ifdef ENABLE_JACK
	    prefs->audio_player=AUD_PLAYER_JACK;
	    g_snprintf(prefs->aplayer,512,"%s","jack");
	    set_pref("audio_player","jack");
	    apl_valid=TRUE;
#endif
	  }
	  if (!strcmp(buff,"pulse")) {
#ifdef HAVE_PULSE_AUDIO
	    prefs->audio_player=AUD_PLAYER_PULSE;
	    set_pref("audio_player","pulse");
	    g_snprintf(prefs->aplayer,512,"%s","pulse");
	    apl_valid=TRUE;
#endif
	  }
	  if (apl_valid) ign_opts.ign_aplayer=TRUE;
	  else g_printerr(_("Invalid audio player %s\n"),buff);
	  continue;
	}
	if (!strcmp(charopt,"nogui")) {
	  // force headless mode
	  prefs->show_gui=FALSE;
	  continue;
	}
	if (!strcmp(charopt,"nosplash")) {
	  // do not show splash
	  prefs->show_splash=FALSE;
	  continue;
	}
	if (!strcmp(charopt,"noplaywin")) {
	  // do not show the play window
	  prefs->show_playwin=FALSE;
	  continue;
	}
	if (!strcmp(charopt,"nothreaddialog")) {
	  // disable threaded dialog (does nothing now)
	  continue;
	}
	if (!strcmp(charopt,"fxmodesmax")&&optarg!=NULL) {
	  // set number of fx modes
	  prefs->max_modes_per_key=atoi(optarg);
	  if (prefs->max_modes_per_key<1) prefs->max_modes_per_key=1;
	  ign_opts.ign_osc=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"bigendbug")) {
	  if (optarg!=NULL) {
	    // set bigendbug
	    prefs->bigendbug=atoi(optarg);
	  }
	  else prefs->bigendbug=1;
	  continue;
	}
#ifdef ENABLE_OSC
	if (!strcmp(charopt,"oscstart")&&optarg!=NULL) {
	  // force OSC start
	  prefs->osc_udp_port=atoi(optarg);
	  prefs->osc_start=TRUE;
	  ign_opts.ign_osc=TRUE;
	  continue;
	}
	if (!strcmp(charopt,"nooscstart")) {
	  // force no OSC start
	  prefs->osc_start=FALSE;
	  ign_opts.ign_osc=TRUE;
	  continue;
	}
#endif

#ifdef ENABLE_JACK
	if (!strcmp(charopt,"jackopts")&&optarg!=NULL) {
	  // override jackopts in config file
	  prefs->jack_opts=atoi(optarg);
	  continue;
	}
#endif
	if (!strcmp(charopt,"startup-ce")) {
	  // force start in clip editor mode
	  if (!ign_opts.ign_stmode) {
	    prefs->startup_interface=STARTUP_CE;
	    ign_opts.ign_stmode=TRUE;
	  }
	  continue;
	}
	if (!strcmp(charopt,"startup-mt")) {
	  // force start in multitrack mode
	  if (!ign_opts.ign_stmode) {
	    prefs->startup_interface=STARTUP_MT;
	    ign_opts.ign_stmode=TRUE;
	  }
	  continue;
	}

      }
      if (optind<argc) {
	// remaining opts are filename [start_time] [end_frame]
	g_snprintf(start_file,PATH_MAX,"%s",argv[optind++]); // filename
	if (optind<argc) start=g_strtod(argv[optind++],NULL); // start time (seconds)
	if (optind<argc) end=atoi(argv[optind++]); // number of frames
      }}}

  g_idle_add(lives_startup,NULL);

  gtk_main ();
  return 0;
}




boolean startup_message_fatal(const gchar *msg) {
  splash_end();
  do_blocking_error_dialog (msg);
  mainw->startup_error=TRUE;
  lives_exit();
  return TRUE;
}

boolean startup_message_nonfatal(const gchar *msg) {
  mainw->is_ready=TRUE;
  do_error_dialog (msg);
  mainw->is_ready=FALSE;
  return TRUE;
}

boolean startup_message_nonfatal_dismissable(const gchar *msg, gint warning_mask) {
  mainw->is_ready=TRUE;
  do_error_dialog_with_check (msg, warning_mask);
  mainw->is_ready=FALSE;
  return TRUE;
}


void set_main_title(const gchar *file, gint untitled) {
  gchar *title,*tmp;
  gchar short_file[256];

  if (file!=NULL) {
    if (untitled) title=g_strdup_printf(_ ("LiVES-%s: <Untitled%d> %dx%d : %d frames %d bpp %.3f fps"),LiVES_VERSION,untitled,
					cfile->hsize,cfile->vsize,cfile->frames,cfile->bpp,cfile->fps);
    else {
      g_snprintf(short_file,256,"%s",file);
      if (cfile->restoring||(cfile->opening&&cfile->frames==123456789)) {
	title=g_strdup_printf(_("LiVES-%s: <%s> %dx%d : ??? frames ??? bpp %.3f fps"),LiVES_VERSION,
			      (tmp=g_path_get_basename(file)),cfile->hsize,cfile->vsize,cfile->fps);
      }
      else {
	title=g_strdup_printf(_ ("LiVES-%s: <%s> %dx%d : %d frames %d bpp %.3f fps"),LiVES_VERSION,
			      cfile->clip_type!=CLIP_TYPE_VIDEODEV?(tmp=g_path_get_basename(file))
			      :(tmp=g_strdup(file)),cfile->hsize,cfile->vsize,cfile->frames,cfile->bpp,cfile->fps);
      }
      g_free(tmp);
    }
  }
  else {
    title=g_strdup_printf(_ ("LiVES-%s: <No File>"),LiVES_VERSION);
  }

  gtk_window_set_title (GTK_WINDOW (mainw->LiVES), title);

  if (mainw->playing_file==-1&&mainw->play_window!=NULL) gtk_window_set_title(GTK_WINDOW(mainw->play_window),title);

  g_free(title);
}


void sensitize(void) {
  // sensitize main window controls
  // READY MODE
  int i;

  if (mainw->multitrack!=NULL) return;

  gtk_widget_set_sensitive (mainw->open, TRUE);
  gtk_widget_set_sensitive (mainw->open_sel, TRUE);
  gtk_widget_set_sensitive (mainw->open_vcd_menu, TRUE);
#ifdef HAVE_WEBM
  gtk_widget_set_sensitive (mainw->open_loc_menu, TRUE);
#else
  gtk_widget_set_sensitive (mainw->open_loc, TRUE);
#endif
  gtk_widget_set_sensitive (mainw->open_device_menu, TRUE);
  gtk_widget_set_sensitive (mainw->restore, TRUE);
  gtk_widget_set_sensitive (mainw->recent_menu, TRUE);
  gtk_widget_set_sensitive (mainw->save_as, mainw->current_file>0&&capable->has_encoder_plugins);
  gtk_widget_set_sensitive (mainw->backup, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->save_selection, mainw->current_file>0&&cfile->frames>0&&capable->has_encoder_plugins);
  gtk_widget_set_sensitive (mainw->clear_ds, TRUE);
  gtk_widget_set_sensitive (mainw->playsel, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->copy, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->cut, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->rev_clipboard, !(clipboard==NULL));
  gtk_widget_set_sensitive (mainw->playclip, !(clipboard==NULL));
  gtk_widget_set_sensitive (mainw->paste_as_new, !(clipboard==NULL));
  gtk_widget_set_sensitive (mainw->insert, !(clipboard==NULL));
  gtk_widget_set_sensitive (mainw->merge,(clipboard!=NULL&&cfile->frames>0));
  gtk_widget_set_sensitive (mainw->xdelete, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->playall, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->m_playbutton, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->m_playselbutton, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->m_rewindbutton, mainw->current_file>0&&cfile->pointer_time>0.);
  gtk_widget_set_sensitive (mainw->m_loopbutton, TRUE);
  gtk_widget_set_sensitive (mainw->m_mutebutton, TRUE);
  if (mainw->preview_box!=NULL) {
    gtk_widget_set_sensitive (mainw->p_playbutton, mainw->current_file>0);
    gtk_widget_set_sensitive (mainw->p_playselbutton, mainw->current_file>0&&cfile->frames>0);
    gtk_widget_set_sensitive (mainw->p_rewindbutton, mainw->current_file>0&&cfile->pointer_time>0.);
    gtk_widget_set_sensitive (mainw->p_loopbutton, TRUE);
    gtk_widget_set_sensitive (mainw->p_mutebutton, TRUE);
  }

  gtk_widget_set_sensitive (mainw->rewind, mainw->current_file>0&&cfile->pointer_time>0.);
  gtk_widget_set_sensitive (mainw->show_file_info, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->show_file_comments, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->full_screen, TRUE);
  gtk_widget_set_sensitive (mainw->mt_menu, TRUE);
  gtk_widget_set_sensitive (mainw->add_live_menu,TRUE);
  gtk_widget_set_sensitive (mainw->export_proj, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->import_proj, mainw->current_file==-1);
  
  if (!mainw->foreign) {
    for (i=1;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+
	   mainw->num_rendered_effects_test;i++) 
      if (mainw->rendered_fx[i].menuitem!=NULL&&mainw->rendered_fx[i].min_frames>=0) 
	gtk_widget_set_sensitive(mainw->rendered_fx[i].menuitem,mainw->current_file>0&&cfile->frames>0);

    if (mainw->current_file>0&&((has_video_filters(FALSE)&&!has_video_filters(TRUE))||
				(cfile->achans>0&&prefs->audio_src==AUDIO_SRC_INT&&has_audio_filters(FALSE))||
				mainw->agen_key!=0)) {
      
      gtk_widget_set_sensitive(mainw->rendered_fx[0].menuitem,TRUE);
    }
    else gtk_widget_set_sensitive(mainw->rendered_fx[0].menuitem,FALSE);
  }
  
  gtk_widget_set_sensitive (mainw->record_perf, TRUE);
  gtk_widget_set_sensitive (mainw->export_submenu, mainw->current_file>0&&(cfile->achans>0));
  gtk_widget_set_sensitive (mainw->recaudio_submenu, TRUE);
  gtk_widget_set_sensitive (mainw->recaudio_sel, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->append_audio, mainw->current_file>0&&cfile->achans>0);
  gtk_widget_set_sensitive (mainw->trim_submenu, mainw->current_file>0&&cfile->achans>0);
  gtk_widget_set_sensitive (mainw->fade_aud_in, mainw->current_file>0&&cfile->achans>0);
  gtk_widget_set_sensitive (mainw->fade_aud_out, mainw->current_file>0&&cfile->achans>0);
  gtk_widget_set_sensitive (mainw->trim_audio, mainw->current_file>0&&(cfile->achans*cfile->frames>0));
  gtk_widget_set_sensitive (mainw->trim_to_pstart, mainw->current_file>0&&(cfile->achans&&cfile->pointer_time>0.));
  gtk_widget_set_sensitive (mainw->delaudio_submenu, mainw->current_file>0&&cfile->achans>0);
  gtk_widget_set_sensitive (mainw->delsel_audio, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->resample_audio, mainw->current_file>0&&(cfile->achans>0&&capable->has_sox_sox));
  gtk_widget_set_sensitive (mainw->dsize, !(mainw->fs));
  gtk_widget_set_sensitive (mainw->fade, !(mainw->fs));
  gtk_widget_set_sensitive (mainw->mute_audio, TRUE);
  gtk_widget_set_sensitive (mainw->loop_video, mainw->current_file>0&&(cfile->achans>0&&cfile->frames>0));
  gtk_widget_set_sensitive (mainw->loop_continue, TRUE);
  gtk_widget_set_sensitive (mainw->load_audio, TRUE);
  gtk_widget_set_sensitive (mainw->load_subs, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->erase_subs, mainw->current_file>0&&cfile->subt!=NULL);
  if (capable->has_cdda2wav&&strlen (prefs->cdplay_device)) gtk_widget_set_sensitive (mainw->load_cdtrack, TRUE);
  gtk_widget_set_sensitive (mainw->rename, mainw->current_file>0&&!cfile->opening);
  gtk_widget_set_sensitive (mainw->change_speed, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->resample_video, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->ins_silence, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->close, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->select_submenu, mainw->current_file>0&&!mainw->selwidth_locked&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->select_all, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->select_start_only, TRUE);
  gtk_widget_set_sensitive (mainw->select_end_only, TRUE);
  gtk_widget_set_sensitive (mainw->select_from_start, TRUE);
  gtk_widget_set_sensitive (mainw->select_to_end, TRUE);
#ifdef HAVE_YUV4MPEG
  gtk_widget_set_sensitive (mainw->open_yuv4m, TRUE);
#endif

  gtk_widget_set_sensitive (mainw->select_new, mainw->current_file>0&&(cfile->insert_start>0));
  gtk_widget_set_sensitive (mainw->select_last, mainw->current_file>0&&(cfile->undo_start>0));
  gtk_widget_set_sensitive (mainw->lock_selwidth, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->undo, mainw->current_file>0&&cfile->undoable);
  gtk_widget_set_sensitive (mainw->redo, mainw->current_file>0&&cfile->redoable);
  gtk_widget_set_sensitive (mainw->show_clipboard_info, !(clipboard==NULL));
  gtk_widget_set_sensitive (mainw->capture,TRUE);
  gtk_widget_set_sensitive (mainw->vj_save_set, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->vj_load_set, !mainw->was_set);
  gtk_widget_set_sensitive (mainw->toy_tv, TRUE);
  gtk_widget_set_sensitive (mainw->toy_autolives, TRUE);
  gtk_widget_set_sensitive (mainw->toy_random_frames, TRUE);
  gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
  gtk_widget_set_sensitive (mainw->gens_submenu, TRUE);
  gtk_widget_set_sensitive (mainw->troubleshoot, TRUE);

  if (mainw->current_file>0&&(cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
    gtk_widget_set_sensitive(mainw->select_invert,TRUE);
  }
  else {
    gtk_widget_set_sensitive(mainw->select_invert,FALSE);
  }
 
  if (mainw->current_file>0&&!(cfile->menuentry==NULL)) {
    g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),1,cfile->frames);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
    g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
    
    g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),1,cfile->frames);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
    g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

    gtk_widget_set_sensitive(mainw->spinbutton_start,TRUE);
    gtk_widget_set_sensitive(mainw->spinbutton_end,TRUE);

    if (mainw->play_window!=NULL&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
      // unblock spinbutton in play window
      gtk_widget_set_sensitive(mainw->preview_spinbutton,TRUE);
    }
  }
 
  // clips menu
  for (i=1;i<MAX_FILES;i++) {
    if (!(mainw->files[i]==NULL)) {
      if (!(mainw->files[i]->menuentry==NULL)) {
	gtk_widget_set_sensitive (mainw->files[i]->menuentry, TRUE);
      }
    }
  }
}


void desensitize(void) {
  // desensitize the main window when we are playing/processing a clip
  int i;

  if (mainw->multitrack!=NULL) return;

  //gtk_widget_set_sensitive (mainw->open, mainw->playing_file>-1);
  gtk_widget_set_sensitive (mainw->open, FALSE);
  gtk_widget_set_sensitive (mainw->open_sel, FALSE);
  gtk_widget_set_sensitive (mainw->open_vcd_menu, FALSE);
#ifdef HAVE_WEBM
  gtk_widget_set_sensitive (mainw->open_loc_menu, FALSE);
#else
  gtk_widget_set_sensitive (mainw->open_loc, FALSE);
#endif
  gtk_widget_set_sensitive (mainw->open_device_menu, FALSE);
#ifdef HAVE_YUV4MPEG
  gtk_widget_set_sensitive (mainw->open_yuv4m, FALSE);
#endif

  gtk_widget_set_sensitive (mainw->add_live_menu, FALSE);

  gtk_widget_set_sensitive (mainw->recent_menu, FALSE);
  gtk_widget_set_sensitive (mainw->restore, FALSE);
  gtk_widget_set_sensitive (mainw->clear_ds, FALSE);
  gtk_widget_set_sensitive (mainw->save_as, FALSE);
  gtk_widget_set_sensitive (mainw->backup, FALSE);
  gtk_widget_set_sensitive (mainw->playsel, FALSE);
  gtk_widget_set_sensitive (mainw->playclip, FALSE);
  gtk_widget_set_sensitive (mainw->copy, FALSE);
  gtk_widget_set_sensitive (mainw->cut, FALSE);
  gtk_widget_set_sensitive (mainw->rev_clipboard, FALSE);
  gtk_widget_set_sensitive (mainw->insert, FALSE);
  gtk_widget_set_sensitive (mainw->merge, FALSE);
  gtk_widget_set_sensitive (mainw->xdelete, FALSE);
  if (!prefs->pause_during_pb) {
    gtk_widget_set_sensitive (mainw->playall, FALSE);
  }
  gtk_widget_set_sensitive (mainw->rewind,FALSE);
  if (!mainw->foreign) {
    for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+
	   mainw->num_rendered_effects_test;i++) 
      if (mainw->rendered_fx[i].menuitem!=NULL&&mainw->rendered_fx[i].menuitem!=NULL&&
	  mainw->rendered_fx[i].min_frames>=0) 
	gtk_widget_set_sensitive(mainw->rendered_fx[i].menuitem,FALSE);
  }

  gtk_widget_set_sensitive (mainw->export_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->recaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->append_audio, FALSE);
  gtk_widget_set_sensitive (mainw->trim_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->delaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->gens_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->troubleshoot, FALSE);
  gtk_widget_set_sensitive (mainw->resample_audio, FALSE);
  gtk_widget_set_sensitive (mainw->fade_aud_in, FALSE);
  gtk_widget_set_sensitive (mainw->fade_aud_out, FALSE);
  gtk_widget_set_sensitive (mainw->ins_silence, FALSE);
  gtk_widget_set_sensitive (mainw->loop_video, (prefs->audio_player==AUD_PLAYER_JACK||
						prefs->audio_player==AUD_PLAYER_PULSE));
  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) 
    gtk_widget_set_sensitive (mainw->mute_audio, FALSE);
  gtk_widget_set_sensitive (mainw->load_audio, FALSE);
  gtk_widget_set_sensitive (mainw->load_subs, FALSE);
  gtk_widget_set_sensitive (mainw->erase_subs, FALSE);
  gtk_widget_set_sensitive (mainw->save_selection, FALSE);
  gtk_widget_set_sensitive (mainw->close, FALSE);
  gtk_widget_set_sensitive (mainw->change_speed, FALSE);
  gtk_widget_set_sensitive (mainw->resample_video, FALSE);
  gtk_widget_set_sensitive (mainw->undo, FALSE);
  gtk_widget_set_sensitive (mainw->redo, FALSE);
  gtk_widget_set_sensitive (mainw->paste_as_new, FALSE);
  gtk_widget_set_sensitive (mainw->capture, FALSE);
  gtk_widget_set_sensitive (mainw->toy_tv, FALSE);
  gtk_widget_set_sensitive (mainw->vj_save_set, FALSE);
  gtk_widget_set_sensitive (mainw->vj_load_set, FALSE);
  gtk_widget_set_sensitive (mainw->export_proj, FALSE);
  gtk_widget_set_sensitive (mainw->import_proj, FALSE);
  gtk_widget_set_sensitive (mainw->recaudio_sel,FALSE);
  gtk_widget_set_sensitive (mainw->mt_menu,FALSE);

  if (mainw->current_file>=0&&(mainw->playing_file==-1||mainw->foreign)) {
    //  if (!cfile->opening||mainw->dvgrab_preview||mainw->preview||cfile->opening_only_audio) {
      // disable the 'clips' menu entries
      for (i=1;i<MAX_FILES;i++) {
	if (!(mainw->files[i]==NULL)) {
	  if (!(mainw->files[i]->menuentry==NULL)) {
	    if (!(i==mainw->current_file)) {
	      gtk_widget_set_sensitive (mainw->files[i]->menuentry, FALSE);
	    }}}}}
  //}
}


void 
procw_desensitize(void) {
  // switch on/off a few extra widgets in the processing dialog

  if (mainw->multitrack!=NULL) return;

  if (mainw->current_file>0&&(cfile->menuentry!=NULL||cfile->opening)&&!mainw->preview) {
    // an effect etc,
    gtk_widget_set_sensitive (mainw->loop_video, cfile->achans>0&&cfile->frames>0);
    gtk_widget_set_sensitive (mainw->loop_continue, TRUE);

    if (cfile->achans>0&&cfile->frames>0) {
      mainw->loop=gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (mainw->loop_video));
    }
    if (cfile->achans>0&&cfile->frames>0) {
      mainw->mute=gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (mainw->mute_audio));
    }
  }
  if (mainw->current_file>0&&cfile->menuentry==NULL) {
    gtk_widget_set_sensitive (mainw->rename, FALSE);
    if (cfile->opening||cfile->restoring) {
      // loading, restoring etc
      gtk_widget_set_sensitive (mainw->lock_selwidth, FALSE);
      gtk_widget_set_sensitive (mainw->show_file_comments, FALSE);
      if (!cfile->opening_only_audio) {
	gtk_widget_set_sensitive (mainw->toy_random_frames, FALSE);
      }
    }
  }
  // stop the start and end from being changed
  // better to clamp the range than make insensitive, this way we stop
  // other widgets (like the video bar) updating it
  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end,cfile->end);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start,cfile->start);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

  if (mainw->play_window!=NULL&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
    // block spinbutton in play window
    gtk_widget_set_sensitive(mainw->preview_spinbutton,FALSE);
  }

  gtk_widget_set_sensitive (mainw->select_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->toy_tv, FALSE);
  gtk_widget_set_sensitive (mainw->toy_autolives, FALSE);
  gtk_widget_set_sensitive (mainw->trim_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->delaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->load_cdtrack, FALSE);
  gtk_widget_set_sensitive (mainw->open_lives2lives, FALSE);
  gtk_widget_set_sensitive (mainw->record_perf, FALSE);

  if (mainw->current_file>0&&cfile->nopreview) {
    gtk_widget_set_sensitive (mainw->m_playbutton, FALSE);
    if (mainw->preview_box!=NULL) gtk_widget_set_sensitive (mainw->p_playbutton, FALSE);
    gtk_widget_set_sensitive (mainw->m_playselbutton, FALSE);
    if (mainw->preview_box!=NULL) gtk_widget_set_sensitive (mainw->p_playselbutton, FALSE);
  }
}




void set_ce_frame_from_pixbuf(GtkImage *image, GdkPixbuf *pixbuf, lives_painter_t *cairo) {
#if GTK_CHECK_VERSION(3,0,0)
  
  int rwidth=lives_widget_get_allocation_width(LIVES_WIDGET(image));
  int rheight=lives_widget_get_allocation_height(LIVES_WIDGET(image));

  lives_painter_t *cr;

  LiVESWidgetColor color;

  if (cairo==NULL) cr=lives_painter_create_from_widget (LIVES_WIDGET(image));
  else cr=cairo;

  if (cr==NULL) return;

  lives_widget_get_bg_color(LIVES_WIDGET(image),&color);
  lives_painter_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);
  lives_painter_rectangle(cr,0,0,
			  rwidth,
			  rheight);
  lives_painter_fill(cr);
  
  if (pixbuf!=NULL) {
    int width=lives_pixbuf_get_width(pixbuf);
    int height=lives_pixbuf_get_height(pixbuf);
    int cx=(rwidth-width)/2;
    int cy=(rheight-height)/2;

    lives_painter_set_source_pixbuf (cr, pixbuf, cx, cy);
    lives_painter_rectangle(cr,cx,cy,
			    width,
			    height);
    lives_painter_fill(cr);
  }
  if (cairo==NULL) lives_painter_destroy(cr);
#else
  gtk_image_set_from_pixbuf(image,pixbuf);
#endif
}






void load_start_image(gint frame) {
  GdkPixbuf *start_pixbuf=NULL;
  weed_plant_t *layer;
  weed_timecode_t tc;
  gint rwidth,rheight,width,height;
  boolean noswitch=mainw->noswitch;
  LiVESInterpType interp;


  if (!prefs->show_gui) return;

  if (mainw->multitrack!=NULL) return;

  if (mainw->current_file>-1&&(cfile->clip_type==CLIP_TYPE_YUV4MPEG||cfile->clip_type==CLIP_TYPE_VIDEODEV)) {
    if (mainw->camframe==NULL) {
      GError *error=NULL;
      gchar *tmp=g_build_filename(prefs->prefix_dir,THEME_DIR,"camera","frame.jpg",NULL);
      mainw->camframe=gdk_pixbuf_new_from_file(tmp,&error);
      if (mainw->camframe!=NULL) gdk_pixbuf_saturate_and_pixelate(mainw->camframe,mainw->camframe,0.0,FALSE);
      g_free(tmp);
    }
    set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image272),mainw->camframe,NULL);
    return;
  }

  if (mainw->current_file<0||cfile==NULL||frame<1||frame>cfile->frames||
      (cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
    threaded_dialog_spin();
    if (!(mainw->imframe==NULL)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image272),mainw->imframe,NULL);
    }
    else {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image272),NULL,NULL);
    }
    threaded_dialog_spin();
    return;
  }

  tc=((frame-1.))/cfile->fps*U_SECL;

  if (!prefs->ce_maxspect||(mainw->double_size&&mainw->playing_file>-1)) {
    threaded_dialog_spin();
    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",frame);
    if (pull_frame_at_size(layer,get_image_ext_for_type(cfile->img_type),tc,cfile->hsize,cfile->vsize,
			   WEED_PALETTE_RGB24)) {
      interp=get_interp_value(prefs->pb_quality);
      resize_layer(layer,cfile->hsize,cfile->vsize,interp,WEED_PALETTE_RGB24,0);
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
      start_pixbuf=layer_to_pixbuf(layer);
    }
    weed_plant_free(layer);
  
    if (GDK_IS_PIXBUF(start_pixbuf)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image272),start_pixbuf,NULL);
    }
    if (start_pixbuf!=NULL) {
      if (G_IS_OBJECT(start_pixbuf)) {
	lives_object_unref(start_pixbuf);
      }
    }
    threaded_dialog_spin();
    return;
  }

  mainw->noswitch=TRUE;

  threaded_dialog_spin();

  do {
    width=cfile->hsize;
    height=cfile->vsize;

    // TODO *** - if width*height==0, show broken frame image


#if GTK_CHECK_VERSION(3,0,0)
    rwidth=mainw->ce_frame_width-H_RESIZE_ADJUST*2;
    rheight=mainw->ce_frame_height-V_RESIZE_ADJUST*2;
#else
    rwidth=lives_widget_get_allocation_width(mainw->image272);
    rheight=lives_widget_get_allocation_height(mainw->image272);
#endif

    calc_maxspect(rwidth,rheight,&width,&height);

    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",frame);

    if (pull_frame_at_size(layer,get_image_ext_for_type(cfile->img_type),tc,width,height,WEED_PALETTE_RGB24)) {
      interp=get_interp_value(prefs->pb_quality);
      resize_layer(layer,width,height,interp,WEED_PALETTE_RGB24,0);
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
      start_pixbuf=layer_to_pixbuf(layer);
    }
    weed_plant_free(layer);
  
    if (GDK_IS_PIXBUF(start_pixbuf)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image272),start_pixbuf,NULL);
    }
    if (start_pixbuf!=NULL) {
      if (G_IS_OBJECT(start_pixbuf)) {
	lives_object_unref(start_pixbuf);
      }
    }

    start_pixbuf=NULL;

#if !GTK_CHECK_VERSION(3,0,0)
    gtk_widget_queue_resize(mainw->image272);

    lives_widget_context_update();
    if (mainw->current_file==-1) {
      // user may close file
      load_start_image(0);
      return;
    }
  } while (rwidth!=lives_widget_get_allocation_width(mainw->image272)||rheight!=lives_widget_get_allocation_height(mainw->image272));
#else
  } while (FALSE);
#endif
  threaded_dialog_spin();


  mainw->noswitch=noswitch;


}



void load_end_image(gint frame) {
  GdkPixbuf *end_pixbuf=NULL;
  weed_plant_t *layer;
  weed_timecode_t tc;
  gint rwidth,rheight,width,height;
  boolean noswitch=mainw->noswitch;
  LiVESInterpType interp;

  if (!prefs->show_gui) return;

  if (mainw->multitrack!=NULL) return;

  if (mainw->current_file>-1&&(cfile->clip_type==CLIP_TYPE_YUV4MPEG||cfile->clip_type==CLIP_TYPE_VIDEODEV)) {
    if (mainw->camframe==NULL) {
      GError *error=NULL;
      gchar *tmp=g_build_filename(prefs->prefix_dir,THEME_DIR,"camera","frame.jpg",NULL);
      mainw->camframe=gdk_pixbuf_new_from_file(tmp,&error);
      if (mainw->camframe!=NULL) gdk_pixbuf_saturate_and_pixelate(mainw->camframe,mainw->camframe,0.0,FALSE);
      g_free(tmp);
    }
    set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image273),mainw->camframe,NULL);
    return;
  }

  if (mainw->current_file<0||cfile==NULL||frame<1||frame>cfile->frames||
      (cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
    threaded_dialog_spin();
    if (!(mainw->imframe==NULL)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image273),mainw->imframe,NULL);
    }
    else {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image273),NULL,NULL);
    }
    threaded_dialog_spin();
    return;
  }


  tc=((frame-1.))/cfile->fps*U_SECL;

  if (!prefs->ce_maxspect||(mainw->double_size&&mainw->playing_file>-1)) {
    threaded_dialog_spin();
    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",frame);

    if (pull_frame_at_size(layer,get_image_ext_for_type(cfile->img_type),tc,cfile->hsize,cfile->vsize,
			   WEED_PALETTE_RGB24)) {
      interp=get_interp_value(prefs->pb_quality);
      resize_layer(layer,cfile->hsize,cfile->vsize,interp,WEED_PALETTE_RGB24,0);
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
      end_pixbuf=layer_to_pixbuf(layer);
    }
    weed_plant_free(layer);
  
    if (GDK_IS_PIXBUF(end_pixbuf)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image273),end_pixbuf,NULL);
    }
    if (end_pixbuf!=NULL) {
      if (G_IS_OBJECT(end_pixbuf)) {
	lives_object_unref(end_pixbuf);
      }
    }
    threaded_dialog_spin();
    return;
  }

  mainw->noswitch=TRUE;

  threaded_dialog_spin();
  do {
    width=cfile->hsize;
    height=cfile->vsize;

#if GTK_CHECK_VERSION(3,0,0)
    rwidth=mainw->ce_frame_width-H_RESIZE_ADJUST*2;
    rheight=mainw->ce_frame_height-V_RESIZE_ADJUST*2;
#else
    rwidth=lives_widget_get_allocation_width(mainw->image273);
    rheight=lives_widget_get_allocation_height(mainw->image273);
#endif

    calc_maxspect(rwidth,rheight,&width,&height);

    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",frame);

    if (pull_frame_at_size(layer,get_image_ext_for_type(cfile->img_type),tc,width,height,WEED_PALETTE_RGB24)) {
      interp=get_interp_value(prefs->pb_quality);
      resize_layer(layer,width,height,interp,WEED_PALETTE_RGB24,0);
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);  
      end_pixbuf=layer_to_pixbuf(layer);
    }

    weed_plant_free(layer);
  
    if (GDK_IS_PIXBUF(end_pixbuf)) {
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image273),end_pixbuf,NULL);
    }
    if (end_pixbuf!=NULL) {
      if (G_IS_OBJECT(end_pixbuf)) {
	lives_object_unref(end_pixbuf);
      }
    }

    end_pixbuf=NULL;

#if !GTK_CHECK_VERSION(3,0,0)
    gtk_widget_queue_resize(mainw->image273);

    lives_widget_context_update();
    if (mainw->current_file==-1) {
      // user may close file
      load_end_image(0);
      return;
    }
  } while (rwidth!=lives_widget_get_allocation_width(mainw->image273)||rheight!=lives_widget_get_allocation_height(mainw->image273));
#else
  } while (FALSE);
#endif
  threaded_dialog_spin();
  mainw->noswitch=noswitch;


}






void load_preview_image(boolean update_always) {
  // this is for the sepwin preview
  // update_always==TRUE = update widgets from mainw->preview_frame
  GdkPixbuf *pixbuf=NULL;
  gint preview_frame;

  if (!prefs->show_gui) return;


  if (mainw->current_file>-1&&cfile!=NULL&&(cfile->clip_type==CLIP_TYPE_YUV4MPEG||cfile->clip_type==CLIP_TYPE_VIDEODEV)) {
    if (mainw->camframe==NULL) {
      GError *error=NULL;
      gchar *tmp=g_strdup_printf("%s/%s/camera/frame.jpg",prefs->prefix_dir,THEME_DIR);
      mainw->camframe=gdk_pixbuf_new_from_file(tmp,&error);
      if (mainw->camframe!=NULL) gdk_pixbuf_saturate_and_pixelate(mainw->camframe,mainw->camframe,0.0,FALSE);
      g_free(tmp);
    }
    pixbuf=lives_pixbuf_scale_simple(mainw->camframe,mainw->pwidth,mainw->pheight,LIVES_INTERP_BEST);
    set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->preview_image),pixbuf,NULL);
    lives_object_unref(pixbuf);
    mainw->preview_frame=1;
    g_signal_handler_block(mainw->preview_spinbutton,mainw->preview_spin_func);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->preview_spinbutton),1,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->preview_spinbutton),1);
    g_signal_handler_unblock(mainw->preview_spinbutton,mainw->preview_spin_func);
    gtk_widget_set_size_request(mainw->preview_image,mainw->pwidth,mainw->pheight);
    return;
  }

  if (mainw->current_file<0||cfile==NULL||!cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&
							   cfile->clip_type!=CLIP_TYPE_FILE)) {

    mainw->preview_frame=0;
    g_signal_handler_block(mainw->preview_spinbutton,mainw->preview_spin_func);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->preview_spinbutton),0,0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->preview_spinbutton),0);
    g_signal_handler_unblock(mainw->preview_spinbutton,mainw->preview_spin_func);
    if (mainw->imframe!=NULL) {
      gtk_widget_set_size_request(mainw->preview_image,lives_pixbuf_get_width(mainw->imframe),lives_pixbuf_get_height(mainw->imframe));
      set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->preview_image), mainw->imframe, NULL);
    }
    else set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->preview_image), NULL, NULL);
    return;
  }

  if (!update_always) {
    // set current frame from spins, set range
    // set mainw->preview_frame to 0 before calling to force an update (e.g after a clip switch)
      switch (mainw->prv_link) {
      case PRV_END:
	preview_frame=cfile->end;
	break;
      case PRV_PTR:
	preview_frame=calc_frame_from_time(mainw->current_file,cfile->pointer_time);
	break;
      case PRV_START:
	preview_frame=cfile->start;
	break;
      default:
	preview_frame=mainw->preview_frame>0?mainw->preview_frame:1;
	if (preview_frame>cfile->frames) preview_frame=cfile->frames;
	break;
      }

      g_signal_handler_block(mainw->preview_spinbutton,mainw->preview_spin_func);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->preview_spinbutton),1,cfile->frames);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->preview_spinbutton),preview_frame);
      g_signal_handler_unblock(mainw->preview_spinbutton,mainw->preview_spin_func);

      mainw->preview_frame=preview_frame;
  }
  
  if (mainw->preview_frame<1||mainw->preview_frame>cfile->frames) {
    pixbuf=lives_pixbuf_scale_simple(mainw->imframe,cfile->hsize,cfile->vsize,LIVES_INTERP_BEST);
  }
  else {
    weed_plant_t *layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_timecode_t tc=((mainw->preview_frame-1.))/cfile->fps*U_SECL;
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",mainw->preview_frame);
    if (pull_frame_at_size(layer,get_image_ext_for_type(cfile->img_type),tc,mainw->pwidth,mainw->pheight,
			   WEED_PALETTE_RGB24)) {
      LiVESInterpType interp=get_interp_value(prefs->pb_quality);
      resize_layer(layer,mainw->pwidth,mainw->pheight,interp,WEED_PALETTE_RGB24,0);
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
      pixbuf=layer_to_pixbuf(layer);
    }
    weed_plant_free(layer);
  }

  set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->preview_image), pixbuf, NULL);
  gtk_widget_set_size_request(mainw->preview_image,mainw->pwidth,mainw->pheight);

  if (update_always) {
    // set spins from current frame
    switch (mainw->prv_link) {
    case PRV_PTR:
      //cf. hrule_reset
      cfile->pointer_time=calc_time_from_frame(mainw->current_file,mainw->preview_frame);
      lives_ruler_set_value(LIVES_RULER(mainw->hruler),cfile->pointer_time);
      if (cfile->pointer_time>0.) {
	gtk_widget_set_sensitive (mainw->rewind, TRUE);
	gtk_widget_set_sensitive (mainw->trim_to_pstart, cfile->achans>0);
	gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
	if (mainw->preview_box!=NULL) {
	  gtk_widget_set_sensitive (mainw->p_rewindbutton, TRUE);
	}
	get_play_times();
      }
      break;
	
    case PRV_START:
      if (cfile->start!=mainw->preview_frame) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),mainw->preview_frame);
	get_play_times();
      }
      break;
      

    case PRV_END:
      if (cfile->end!=mainw->preview_frame) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),mainw->preview_frame);
	get_play_times();
      }
      break;

    default:
      gtk_widget_set_sensitive (mainw->rewind, FALSE);
      gtk_widget_set_sensitive (mainw->trim_to_pstart, FALSE);
      gtk_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
      if (mainw->preview_box!=NULL) {
	gtk_widget_set_sensitive (mainw->p_rewindbutton, FALSE);
      }
      break;
    }
  }
  if (pixbuf!=NULL) lives_object_unref(pixbuf);
}

#ifndef NO_PROG_LOAD


#ifdef USE_LIBPNG
static void png_row_callback(png_structp png_ptr,
			     png_uint_32 row, int pass) {
  sched_yield();
}

#endif

static void pbsize_set(GdkPixbufLoader *pbload, gint width, gint height, gpointer ptr) {
  if (xxwidth*xxheight>0) gdk_pixbuf_loader_set_size(pbload,xxwidth,xxheight);
}


#endif




#ifdef USE_LIBPNG

boolean layer_from_png(FILE *fp, weed_plant_t *layer, boolean prog) {
  png_structp png_ptr;
  png_infop info_ptr;

  int width, height;
  int color_type, bit_depth;
  int rowstrides[1];

  int i;

  unsigned char buff[8];

  unsigned char *mem,*ptr;
  unsigned char **row_ptrs;

  size_t bsize=fread(buff,1,8,fp),framesize;
  boolean is_png = !png_sig_cmp(buff, 0, bsize);

  float screen_gamma=2.2;
  double file_gamma;

  if (!is_png) return FALSE;

  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, 
				      (png_voidp)NULL, NULL, NULL);
  
  if (!png_ptr) return FALSE;
  
  info_ptr = png_create_info_struct(png_ptr);
  
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr,
			    (png_infopp)NULL, (png_infopp)NULL);
    return FALSE;
  }

  
  if (setjmp(png_jmpbuf(png_ptr)))
    {
      // libpng will longjump to here on error
      png_destroy_read_struct(&png_ptr, &info_ptr,
			      (png_infopp)NULL);
      return FALSE;
    }
  
  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, bsize);
  
  // progressive read calls png_row_callback on each row
  if (prog) png_set_read_status_fn(png_ptr, png_row_callback);

#if PNG_LIBPNG_VER >= 10504
  png_set_alpha_mode(png_ptr, PNG_ALPHA_STANDARD, PNG_DEFAULT_sRGB);
  png_set_gamma(png_ptr, screen_gamma, 1.0/screen_gamma);
#else
  png_set_gamma(png_ptr, screen_gamma, 1.0/screen_gamma);
#endif
  
  // read header info
  png_read_info(png_ptr, info_ptr);

  // want to convert everything (greyscale, RGB, RGBA64 etc.) to RGBA32
  color_type = png_get_color_type(png_ptr, info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);
  
  if (png_get_valid(png_ptr, info_ptr,
		    PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
  
  if (color_type == PNG_COLOR_TYPE_GRAY &&
      bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
 
  if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
       png_set_gray_to_rgb(png_ptr);


  if (bit_depth == 16) {
#if PNG_LIBPNG_VER >= 10504
    png_set_scale_16(png_ptr);
#else
    png_set_strip_16(png_ptr);
#endif
  }

  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA && 
      color_type !=PNG_COLOR_TYPE_GRAY_ALPHA ) 
    png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);

  png_set_interlace_handling(png_ptr);


  // read updated info with the new palette
  png_read_update_info(png_ptr, info_ptr);

  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);
  *rowstrides = png_get_rowbytes(png_ptr, info_ptr);
  
  png_get_gAMA(png_ptr, info_ptr, &file_gamma);

  weed_set_int_value(layer,"width",width);
  weed_set_int_value(layer,"height",height);
  weed_set_int_value(layer,"rowstrides",*rowstrides);
  weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGBA32);


  // here we allocate ourselves, instead of calling create_empty_pixel data - in case rowbytes is different

  // some things, like swscale, expect all frames to be a multiple of 32 bytes
  // so here we round up
  framesize=CEIL(*rowstrides*height,32);

  ptr=mem=(unsigned char *)g_malloc(framesize);
  weed_set_voidptr_value(layer, "pixel_data", mem);
  
  // libpng needs pointers to each row
  row_ptrs=(unsigned char **)g_malloc(height*sizeof(unsigned char *));
  for (i=0;i<height;i++) {
    row_ptrs[i]=ptr;
    ptr+=*rowstrides;
  }

  // read in the image
  png_read_image(png_ptr, row_ptrs);

  // end read
  png_read_end(png_ptr, (png_infop)NULL);
  
  // do we need this ? maybe only when compositing
  //if (file_gamma!=0.) {
    //ungamma_layer(layer,file_gamma);
  //}

  g_free(row_ptrs);

  png_destroy_read_struct(&png_ptr, &info_ptr,
			  (png_infopp)NULL);
  

  if (prefs->alpha_post) {
    // un-premultiply the alpha
    alpha_unpremult(layer,TRUE);
  }
  else {
    int flags=0,error;
    if (weed_plant_has_leaf(layer,"flags"))
      flags=weed_get_int_value(layer,"flags",&error);

    flags|=WEED_CHANNEL_ALPHA_PREMULT;
    weed_set_int_value(layer,"flags",flags);
  }


  return TRUE;
}

#endif


static boolean weed_layer_new_from_file_progressive(weed_plant_t *layer, 
						    const char *fname, int width, 
						    int height, 
						    const char *img_ext, 
						    GError **gerror) {


  LiVESPixbuf *pixbuf=NULL;
  boolean do_not_free=TRUE;

#ifndef NO_PROG_LOAD
  GdkPixbufLoader *pbload;
  guchar buff[IMG_BUFF_SIZE];
  size_t bsize;
  FILE *fp=fopen(fname,"rb");
  if (!fp) return FALSE;

  xxwidth=width;
  xxheight=height;

  if (!strcmp(img_ext,"png")) {
#ifdef USE_LIBPNG
    boolean ret=layer_from_png(fp,layer,TRUE);
    fclose(fp);
    return ret;
#endif

    pbload=gdk_pixbuf_loader_new_with_type("png",gerror);
  }
  else if (!strcmp(img_ext,"jpg")) pbload=gdk_pixbuf_loader_new_with_type("jpeg",gerror);
  else pbload=gdk_pixbuf_loader_new();

  g_signal_connect (G_OBJECT (pbload), "size_prepared",
                      G_CALLBACK (pbsize_set),
                      NULL);

  while (1) {
    if (!(bsize=fread(buff,1,IMG_BUFF_SIZE,fp))) break;
    sched_yield();
    if (!gdk_pixbuf_loader_write(pbload,buff,bsize,gerror)) {
      fclose (fp);
      return FALSE;
    }
    sched_yield();

  }

  sched_yield();

  fclose(fp);

  if (!gdk_pixbuf_loader_close(pbload,gerror)) return FALSE;

  pixbuf=(GdkPixbuf *)g_object_ref(gdk_pixbuf_loader_get_pixbuf(pbload));
  g_object_unref(pbload);

# else

#ifdef USE_LIBPNG
  {
    boolean ret;
    FILE *fp=fopen(fname,"rb");
    if (!fp) return FALSE;
    ret=layer_from_png(fp,layer,FALSE);
    fclose(fp);
    return ret;
  }
#endif

  pixbuf=lives_pixbuf_new_from_file_at_scale(fname,width>0?width:-1,height>0?height:-1,FALSE,gerror);

#endif

  if (*gerror!=NULL) {
    g_error_free(*gerror);
    pixbuf=NULL;
  }

  if (pixbuf==NULL) return FALSE;

  if (lives_pixbuf_get_has_alpha(pixbuf)) {
    /* unfortunately gdk pixbuf loader does not preserve the original alpha channel, instead it adds its own. We need to hence reset it back to opaque */
    lives_pixbuf_set_opaque(pixbuf);
    weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGBA32);
  }
  else weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGB24);


  do_not_free=pixbuf_to_layer(layer,pixbuf);

  if (do_not_free) {
    mainw->do_not_free=(gpointer)lives_pixbuf_get_pixels_readonly(pixbuf);
    mainw->free_fn=lives_free_with_check;
  }
  g_object_unref(pixbuf);
  mainw->do_not_free=NULL;
  mainw->free_fn=lives_free_normal;


  return TRUE;

}



static weed_plant_t *render_subs_from_file(file *sfile, double xtime, weed_plant_t *layer) {
    // render subtitles from whatever (.srt or .sub) file
    // uses default values for colours, fonts, size, etc.

    // TODO - allow prefs settings for colours, fonts, size, alpha

    //char *sfont=mainw->font_list[prefs->sub_font];
    const char *sfont="Sans";
    lives_colRGBA32_t col_white,col_black_a;
    
    int error,size;

    xtime-=(double)sfile->subt->offset/sfile->fps;

    // round to 2 dp
    xtime=(double)((int)(xtime*100.+.5))/100.;

    if (xtime<0.||(sfile->subt->last_time>-1.&&xtime>sfile->subt->last_time)) return layer;

    switch (sfile->subt->type) {
    case SUBTITLE_TYPE_SRT:
      get_srt_text(sfile,xtime);
      break;
    case SUBTITLE_TYPE_SUB:
      get_sub_text(sfile,xtime);
      break;
    default:
      return layer;
    }

    /////////// use plugin //////////////

    size=weed_get_int_value(layer,"width",&error)/32;

    col_white.red=col_white.green=col_white.blue=col_white.alpha=255;
    col_black_a.red=col_black_a.green=col_black_a.blue=0;
    col_black_a.alpha=80;
    
    if (sfile->subt->text!=NULL) {
      gchar *tmp;
      layer=render_text_to_layer(layer,(tmp=g_strdup_printf(" %s ",sfile->subt->text)),sfont,size,
				 LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND,&col_white,&col_black_a,TRUE,TRUE,0);
      g_free(tmp);
    }


    return layer;
}



boolean pull_frame_at_size (weed_plant_t *layer, const gchar *image_ext, weed_timecode_t tc, int width, int height, 
			     int target_palette) {
  // pull a frame from an external source into a layer
  // the "clip" and "frame" leaves must be set in layer
  // tc is used instead of "frame" for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")
  // width and height are hints only, the caller should resize if necessary
  // target_palette is also a hint

  // if we pull from a decoder plugin, then we may also deinterlace

  GError *gerror=NULL;
  int error;
  int clip=weed_get_int_value(layer,"clip",&error);
  int frame=weed_get_int_value(layer,"frame",&error);
  int clip_type;
  int *rowstrides;
  weed_plant_t *vlayer;
  void **pixel_data;
  file *sfile=NULL;

  weed_set_voidptr_value(layer,"pixel_data",NULL);

  mainw->osc_block=TRUE;
  if (clip<0&&frame==0) clip_type=CLIP_TYPE_DISK;
  else {
    sfile=mainw->files[clip];
    if (sfile==NULL) {
      mainw->osc_block=FALSE;
      return FALSE;
    }
    clip_type=sfile->clip_type;
  }

  switch (clip_type) {
  case CLIP_TYPE_DISK:
  case CLIP_TYPE_FILE:
    // frame number can be 0 during rendering
    if (frame==0) {
      mainw->osc_block=FALSE;
      if ((width==0||height==0)&&weed_plant_has_leaf(layer,"width")&&weed_plant_has_leaf(layer,"height")) {
	width=weed_get_int_value(layer,"width",&error);
	height=weed_get_int_value(layer,"height",&error);
      }
      if (width==0) width=4;
      if (height==0) height=4;
      weed_set_int_value(layer,"width",width);
      weed_set_int_value(layer,"height",height);
      if (!weed_plant_has_leaf(layer,"current_palette")) {
	if (image_ext==NULL||!strcmp(image_ext,"jpg")) weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGB24);
	else weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGBA32);
      }
      create_empty_pixel_data(layer,TRUE,TRUE);
      return TRUE;
    }
    else if (clip==mainw->scrap_file) {
      return load_from_scrap_file(layer,frame);
    }
    else {
      if (sfile->clip_type==CLIP_TYPE_FILE&&sfile->frame_index!=NULL&&frame>0&&
	  frame<=sfile->frames&&sfile->frame_index[frame-1]>=0) {
	lives_decoder_t *dplug=(lives_decoder_t *)sfile->ext_src;
	if (target_palette!=dplug->cdata->current_palette) {
	  // try to switch palette
	  if (decplugin_supports_palette(dplug,target_palette)) {
	    // switch palettes and re-read clip_data
	    dplug->cdata->current_palette=target_palette;
	    dplug->cdata=(*dplug->decoder->get_clip_data)(dplug->cdata->URI,dplug->cdata);
	  }
	  else {
	    if (dplug->cdata->current_palette!=dplug->cdata->palettes[0]&&
		((weed_palette_is_rgb_palette(target_palette)&&
		  weed_palette_is_rgb_palette(dplug->cdata->palettes[0]))||
		 (weed_palette_is_yuv_palette(target_palette)&&
		  weed_palette_is_yuv_palette(dplug->cdata->palettes[0])))) {
	      dplug->cdata->current_palette=dplug->cdata->palettes[0];
	      dplug->cdata=(*dplug->decoder->get_clip_data)(dplug->cdata->URI,dplug->cdata);
	    }
	  }
	}
	width=dplug->cdata->frame_width/weed_palette_get_pixels_per_macropixel(dplug->cdata->current_palette);
	height=dplug->cdata->frame_height;
	weed_set_int_value(layer,"width",width);
	weed_set_int_value(layer,"height",height);
	weed_set_int_value(layer,"current_palette",dplug->cdata->current_palette);
	if (weed_palette_is_yuv_palette(dplug->cdata->current_palette)) {
	  weed_set_int_value(layer,"YUV_sampling",dplug->cdata->YUV_sampling);
	  weed_set_int_value(layer,"YUV_clamping",dplug->cdata->YUV_clamping);
	  weed_set_int_value(layer,"YUV_subspace",dplug->cdata->YUV_subspace);
	}
	create_empty_pixel_data(layer,FALSE,TRUE);

	pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
	rowstrides=weed_get_int_array(layer,"rowstrides",&error);

	(*dplug->decoder->get_frame)(dplug->cdata,(int64_t)(sfile->frame_index[frame-1]),
				     rowstrides,sfile->vsize,pixel_data);

	weed_free(pixel_data);
	weed_free(rowstrides);

        // deinterlace
	if (sfile->deinterlace||(prefs->auto_deint&&dplug->cdata->interlace!=LIVES_INTERLACE_NONE)) 
	  deinterlace_frame(layer,tc);

        // render subtitles from file
	if (prefs->show_subtitles&&sfile->subt!=NULL&&sfile->subt->tfile!=NULL) {
	  double xtime=(double)(frame-1)/sfile->fps;
	  layer=render_subs_from_file(sfile,xtime,layer);
	}


	mainw->osc_block=FALSE;
	return TRUE;
      }
      else {
	boolean ret;
	gchar *fname=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,sfile->handle,frame,image_ext);
	if (height*width==0) {
	  ret=weed_layer_new_from_file_progressive(layer,fname,0,0,image_ext,&gerror);
	}
	else {
	  ret=weed_layer_new_from_file_progressive(layer,fname,width,height,image_ext,&gerror);
	}
	g_free(fname);
	mainw->osc_block=FALSE;
	if (ret==FALSE) return FALSE;
      }
    }
    break;
#ifdef HAVE_YUV4MPEG
  case CLIP_TYPE_YUV4MPEG:
    weed_layer_set_from_yuv4m(layer,sfile);
    if (sfile->deinterlace) deinterlace_frame(layer,tc);
    mainw->osc_block=FALSE;
    return TRUE;
#endif
#ifdef HAVE_UNICAP
  case CLIP_TYPE_VIDEODEV:
    weed_layer_set_from_lvdev(layer,sfile,4./cfile->pb_fps);
    if (sfile->deinterlace) deinterlace_frame(layer,tc);
    mainw->osc_block=FALSE;
    return TRUE;
#endif
  case CLIP_TYPE_LIVES2LIVES:
    weed_layer_set_from_lives2lives(layer,clip,(lives_vstream_t *)sfile->ext_src);
    mainw->osc_block=FALSE;
    return TRUE;
  case CLIP_TYPE_GENERATOR:
    // special handling for clips where host controls size
    // Note: vlayer is actually the out channel of the generator, so we should
    // never free it
    vlayer=weed_layer_new_from_generator((weed_plant_t *)sfile->ext_src,tc);
    weed_layer_copy(layer,vlayer); // layer is non-NULL, so copy by reference
    weed_set_voidptr_value(vlayer,"pixel_data",NULL);
    mainw->osc_block=FALSE;
    return TRUE;
  default:
    mainw->osc_block=FALSE;
    return FALSE;
  }
  mainw->osc_block=FALSE;


  // render subtitles from file
  if (prefs->show_subtitles&&sfile->subt!=NULL&&sfile->subt->tfile!=NULL) {
    double xtime=(double)(frame-1)/sfile->fps;
    layer=render_subs_from_file(sfile,xtime,layer);
  }

  return TRUE;
}


boolean pull_frame (weed_plant_t *layer, const gchar *image_ext, weed_timecode_t tc) {
  // pull a frame from an external source into a layer
  // the "clip" and "frame" leaves must be set in layer
  // tc is used instead of "frame" for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")

  return pull_frame_at_size(layer,image_ext,tc,0,0,WEED_PALETTE_END);
}


LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, weed_timecode_t tc, 
				       int width, int height, LiVESInterpType interp) {
  // return a correctly sized (Gdk)Pixbuf (RGB24 for jpeg, RGBA32 for png) for the given clip and frame
  // tc is used instead of "frame" for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")
  // pixbuf will be sized to width x height pixels using interp

  LiVESPixbuf *pixbuf=NULL;
  weed_plant_t *layer=weed_plant_new(WEED_PLANT_CHANNEL);
  int palette;

  weed_set_int_value(layer,"clip",clip);
  weed_set_int_value(layer,"frame",frame);

  if (!strcmp(image_ext,"png")) palette=WEED_PALETTE_RGBA32;
  else palette=WEED_PALETTE_RGB24;

  if (pull_frame_at_size(layer,image_ext,tc,width,height,palette)) {
    convert_layer_palette(layer,palette,0);
    pixbuf=layer_to_pixbuf(layer);
  }
  weed_plant_free(layer);
  if (pixbuf!=NULL&&((width!=0&&lives_pixbuf_get_width(pixbuf)!=width)
		     ||(height!=0&&lives_pixbuf_get_height(pixbuf)!=height))) {
    GdkPixbuf *pixbuf2;
    threaded_dialog_spin();
    // TODO - could use resize plugin here
    pixbuf2=lives_pixbuf_scale_simple(pixbuf,width,height,interp);
    lives_object_unref(pixbuf);
    threaded_dialog_spin();
    pixbuf=pixbuf2;
  }
  return pixbuf;
}

LIVES_INLINE LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, weed_timecode_t tc) {
  return pull_lives_pixbuf_at_size(clip,frame,image_ext,tc,0,0,LIVES_INTERP_NORMAL);
}

static void get_max_opsize(int *opwidth, int *opheight) {
  // calc max output size for display
  // if we are rendering or saving to disk
  gint pmonitor;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->is_rendering) {
	*opwidth=cfile->hsize;
	*opheight=cfile->vsize;
    }
    else {
      if (!mainw->fs||mainw->play_window==NULL||mainw->ext_playback) {
	if (mainw->play_window==NULL) {
	  *opwidth=mainw->files[mainw->multitrack->render_file]->hsize;
	  *opheight=mainw->files[mainw->multitrack->render_file]->vsize;
	  calc_maxspect(mainw->multitrack->play_width,mainw->multitrack->play_height,opwidth,opheight);
	}
	else {
	  *opwidth=cfile->hsize;
	  *opheight=cfile->vsize;
	}
	
	if (!mainw->fs&&mainw->play_window!=NULL&&(mainw->pwidth<*opwidth||mainw->pheight<*opheight)) {
	  *opwidth=mainw->pwidth;
	  *opheight=mainw->pheight;
	}
      }
      else {
	if (prefs->play_monitor==0) {
	  *opwidth=mainw->scr_width;
	  *opheight=mainw->scr_height;
	}
	else {
	  if (mainw->play_window!=NULL) pmonitor=prefs->play_monitor;
	  else pmonitor=prefs->gui_monitor;
	  *opwidth=mainw->mgeom[pmonitor-1].width;
	  *opheight=mainw->mgeom[pmonitor-1].height;
	}
      }
    }
  }
  if (mainw->ext_playback) {
    if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) {
      if (mainw->vpp->capabilities&VPP_CAN_RESIZE) {
	if (mainw->fs) {
	  if (prefs->play_monitor==0) {
	    *opwidth=mainw->scr_width;
	    *opheight=mainw->scr_height;
	  }
	  else {
	    *opwidth=mainw->mgeom[prefs->play_monitor-1].width;
	    *opheight=mainw->mgeom[prefs->play_monitor-1].height;
	  }
	}
      }
      else {
	// ext plugin can't resize
	*opwidth=mainw->vpp->fwidth;
	*opheight=mainw->vpp->fheight;
      }
    }
    else {
      if (!(mainw->vpp->capabilities&VPP_CAN_RESIZE)) {
	*opwidth=mainw->vpp->fwidth;
	*opheight=mainw->vpp->fheight;
      }
    }
  }
  if (mainw->multitrack==NULL) {
    // no playback plugin
    if (mainw->fs&&!mainw->is_rendering) {
      if (!mainw->sep_win) {
	do {
	  *opwidth=lives_widget_get_allocation_width(mainw->playframe);
	  *opheight=lives_widget_get_allocation_height(mainw->playframe);
	  if (*opwidth * *opheight==0) {
	    lives_widget_context_update();
	  }
	} while (*opwidth * *opheight == 0);
      }
      else {
	if (prefs->play_monitor==0) {
	  if (mainw->scr_width>*opwidth) *opwidth=mainw->scr_width;
	  if (mainw->scr_height>*opheight) *opheight=mainw->scr_height;
	}
	else {
	  if (mainw->play_window!=NULL) pmonitor=prefs->play_monitor;
	  else pmonitor=prefs->gui_monitor;
	  if (mainw->mgeom[pmonitor-1].width>*opwidth) *opwidth=mainw->mgeom[pmonitor-1].width;
	  if (mainw->mgeom[pmonitor-1].height>*opheight) *opheight=mainw->mgeom[pmonitor-1].height;
	}
      }
    }
    else {
      if (mainw->is_rendering) {
	if (cfile->hsize>*opwidth) *opwidth=cfile->hsize;
	if (cfile->vsize>*opheight) *opheight=cfile->vsize;
      }
      else {
	if (!mainw->sep_win) {
#if GTK_CHECK_VERSION(3,0,0)
	  int rwidth=mainw->ce_frame_width;
	  int rheight=mainw->ce_frame_height;
	  *opwidth=cfile->hsize;
	  *opheight=cfile->vsize;
	  calc_maxspect(rwidth,rheight,opwidth,opheight);
#else
	  do {
	    *opwidth=lives_widget_get_allocation_width(mainw->playframe);
	    *opheight=lives_widget_get_allocation_height(mainw->playframe);
	    if (*opwidth * *opheight==0) {
	      lives_widget_context_update();
	    }
	  } while (*opwidth * *opheight == 0);
#endif
	}
	else {
	  if (mainw->pwidth<*opwidth||mainw->pheight<*opheight||*opwidth==0||*opheight==0) {
	    *opwidth=mainw->pwidth;
	    *opheight=mainw->pheight;
	  }
	}
      }
    }
  }

}






void load_frame_image(gint frame) {
  // this is where we do the actual load/record of a playback frame
  // it is called every 1/fps from do_progress_dialog() via process_one() in dialogs.c

  // for the multitrack window we set mainw->frame_image; this is used to display the
  // preview image




  // NOTE: we should be careful if load_frame_image() is called from anywhere inside load_frame_image()
  // e.g. by calling g_main_context_iteration() --> user presses sepwin button --> load_frame_image() is called
  // this is because mainw->frame_layer is global and gets freed() before exit from load_frame_image()
  // - we should never call load_frame_image() if mainw->noswitch is TRUE

  void **pd_array,**retdata=NULL;

  LiVESPixbuf *pixbuf=NULL;

  gchar *framecount=NULL,*tmp;
  gchar *fname_next=NULL,*info_file=NULL;
  const gchar *img_ext=NULL;

  LiVESInterpType interp;

  boolean was_preview;
  boolean rec_after_pb=FALSE;
  int weed_error;
  int retval;
  int layer_palette,cpal;

  int opwidth=0,opheight=0;
  boolean noswitch=mainw->noswitch;
  gint pmonitor;
  int pwidth,pheight;
  int lb_width=0,lb_height=0;
  int bad_frame_count=0;

#define BFC_LIMIT 1000

  if (G_UNLIKELY(cfile->frames==0&&!mainw->foreign&&!mainw->is_rendering)) {
    if (mainw->record&&!mainw->record_paused) {
      // add blank frame
      weed_plant_t *event=get_last_event(mainw->event_list);
      weed_plant_t *event_list=insert_blank_frame_event_at(mainw->event_list,mainw->currticks,&event);
      if (mainw->rec_aclip!=-1&&(prefs->rec_opts&REC_AUDIO)&&!mainw->record_starting&&prefs->audio_src==AUDIO_SRC_INT&&!(has_audio_filters(FALSE))) {
	// we are recording, and the audio clip changed; add audio
	if (mainw->event_list==NULL) mainw->event_list=event_list;
	insert_audio_event_at(mainw->event_list,event,-1,mainw->rec_aclip,mainw->rec_aseek,mainw->rec_avel);
	mainw->rec_aclip=-1;
      }
    }
    get_play_times();
    return;
  }

  if (!mainw->foreign) {
    mainw->actual_frame=cfile->last_frameno=frame;

    if (!(was_preview=mainw->preview)||mainw->is_rendering) {

      /////////////////////////////////////////////////////////

      // normal play

      if (G_UNLIKELY(mainw->nervous)) {
	// nervous mode

	if ((mainw->actual_frame+=(-10+(int) (21.*rand()/(RAND_MAX+1.0))))>cfile->frames||
	    mainw->actual_frame<1) mainw->actual_frame=frame;
	else {
	  frame=mainw->actual_frame;
#ifdef ENABLE_JACK
	  if (prefs->audio_player==AUD_PLAYER_JACK&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)&&
	      mainw->jackd!=NULL&&cfile->achans>0 &&
	      !(prefs->audio_src==AUDIO_SRC_EXT||mainw->agen_key!=0)) {
	    if (!jack_audio_seek_frame(mainw->jackd,frame)) {
	      if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd,frame);
	    }

	    if (!(mainw->record&&!mainw->record_paused&&has_audio_filters(FALSE))) {
	      mainw->rec_aclip=mainw->current_file;
	      mainw->rec_avel=cfile->pb_fps/cfile->fps;
	      mainw->rec_aseek=cfile->aseek_pos/(cfile->arate*cfile->achans*cfile->asampsize/8);
	    }
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (prefs->audio_player==AUD_PLAYER_PULSE&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)&&
	      mainw->pulsed!=NULL&&cfile->achans>0 &&
	      !(prefs->audio_src==AUDIO_SRC_EXT||mainw->agen_key!=0)) {

	    if (!pulse_audio_seek_frame(mainw->pulsed,mainw->play_start)) {
	      if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed,mainw->play_start);
	      else mainw->aplayer_broken=TRUE;
	    }

	    if (!(mainw->record&&!mainw->record_paused&&has_audio_filters(FALSE))) {
	      mainw->rec_aclip=mainw->current_file;
	      mainw->rec_avel=cfile->pb_fps/cfile->fps;
	      mainw->rec_aseek=cfile->aseek_pos/(cfile->arate*cfile->achans*cfile->asampsize/8);
	    }
	  }
#endif
	}
      }
      if (mainw->opening_loc||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
	framecount=g_strdup_printf("%9d",mainw->actual_frame);
      }
      else {
	framecount=g_strdup_printf("%9d/%d",mainw->actual_frame,cfile->frames);
      }

      mainw->noswitch=TRUE;

      /////////////////////////////////////////////////
      
      // record performance
      if ((mainw->record&&!mainw->record_paused)||mainw->record_starting) {
	gint fg_file=mainw->current_file;
	gint fg_frame=mainw->actual_frame;
	gint bg_file=mainw->blend_file>0&&mainw->blend_file!=mainw->current_file&&
	  mainw->files[mainw->blend_file]!=NULL?mainw->blend_file:-1;
	gint bg_frame=bg_file>0&&bg_file!=mainw->current_file?mainw->files[bg_file]->frameno:0;
	int numframes;
	int *clips,*frames;
	weed_plant_t *event_list;

	// should we record the output from the playback plugin ?
	if (mainw->record && (prefs->rec_opts&REC_AFTER_PB) && mainw->ext_playback && 
	    (mainw->vpp->capabilities&VPP_CAN_RETURN)) {
	  rec_after_pb=TRUE;
	}

	if (rec_after_pb||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||
	    (prefs->rec_opts&REC_EFFECTS&&bg_file!=-1&&(mainw->files[bg_file]->clip_type!=CLIP_TYPE_DISK&&
							mainw->files[bg_file]->clip_type!=CLIP_TYPE_FILE))) {
	  // TODO - handle non-opening of scrap_file
	  if (mainw->scrap_file==-1) open_scrap_file();
	  fg_file=mainw->scrap_file;
	  fg_frame=mainw->files[mainw->scrap_file]->frames+1;
	  bg_file=-1;
	  bg_frame=0;
	}

	if (mainw->record_starting) {
	  // mark record start
	  event_list=append_marker_event(mainw->event_list, mainw->currticks, EVENT_MARKER_RECORD_START);
	  if (mainw->event_list==NULL) mainw->event_list=event_list;
	  
	  if (prefs->rec_opts&REC_EFFECTS) {
	    // add init events and pchanges for all active fx
	    add_filter_init_events(mainw->event_list,mainw->currticks);
	  }

#ifdef ENABLE_JACK
	  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&
	      (prefs->rec_opts&REC_AUDIO)&&prefs->audio_src==AUDIO_SRC_INT&&mainw->rec_aclip!=mainw->ascrap_file) {
	    // get current seek postion
	    jack_get_rec_avals(mainw->jackd);
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&
	      (prefs->rec_opts&REC_AUDIO)&&prefs->audio_src==AUDIO_SRC_INT&&mainw->rec_aclip!=mainw->ascrap_file) {
	    // get current seek postion
	    pulse_get_rec_avals(mainw->pulsed);
	  }
#endif
	  mainw->record=TRUE;
	  mainw->record_paused=FALSE;
	  mainw->record_starting=FALSE;
	}

	numframes=(bg_file==-1)?1:2;
	clips=(int *)g_malloc(numframes*sizint);
	frames=(int *)g_malloc(numframes*sizint);
	
	clips[0]=fg_file;
	frames[0]=fg_frame;
	if (numframes==2) {
	  clips[1]=bg_file;
	  frames[1]=bg_frame;
	}
	if (framecount!=NULL) g_free(framecount);
	if ((event_list=append_frame_event (mainw->event_list,mainw->currticks,numframes,clips,frames))!=NULL) {
	  if (mainw->event_list==NULL) mainw->event_list=event_list;
	  if (mainw->rec_aclip!=-1&&((prefs->rec_opts&REC_AUDIO))) {
	    weed_plant_t *event=get_last_frame_event(mainw->event_list);

	    if (mainw->rec_aclip==mainw->ascrap_file) {
	      mainw->rec_aseek=(double)mainw->files[mainw->ascrap_file]->aseek_pos/
		(double)(mainw->files[mainw->ascrap_file]->arps*mainw->files[mainw->ascrap_file]->achans*mainw->files[mainw->ascrap_file]->asampsize>>3);

	    }
	    insert_audio_event_at(mainw->event_list,event,-1,mainw->rec_aclip,mainw->rec_aseek,mainw->rec_avel);
	    mainw->rec_aclip=-1;
	  }
	  /* TRANSLATORS: rec(ord) */
	  framecount=g_strdup_printf(_("rec %9d/%d"),mainw->actual_frame,
				     cfile->frames>mainw->actual_frame?cfile->frames:mainw->actual_frame);
	}
	/* TRANSLATORS: out of memory (rec(ord)) */
	else (framecount=g_strdup_printf(_("!rec %9d/%d"),mainw->actual_frame,cfile->frames));
	g_free(clips);
	g_free(frames);
      }
      else {
	if (mainw->toy_type!=LIVES_TOY_NONE) {
	  if (mainw->toy_type==LIVES_TOY_MAD_FRAMES&&!mainw->fs&&(cfile->clip_type==CLIP_TYPE_DISK||
								  cfile->clip_type==CLIP_TYPE_FILE)) {
	    gint current_file=mainw->current_file;
	    if (mainw->toy_go_wild) {
	      int i,other_file;
	      for (i=0;i<11;i++) {
		other_file=(1+(int) ((double)(mainw->clips_available)*rand()/(RAND_MAX+1.0)));
		other_file=GPOINTER_TO_INT(g_list_nth_data(mainw->cliplist,other_file));
		if (mainw->files[other_file]!=NULL) {
		  // steal a frame from another clip
		  mainw->current_file=other_file;
		}
	      }
	    }
	    load_start_image (1+(int) ((double)cfile->frames*rand()/(RAND_MAX+1.0)));
	    load_end_image (1+(int) ((double)cfile->frames*rand()/(RAND_MAX+1.0)));
	    mainw->current_file=current_file;
	  }
	}
      }
      
      if ((!mainw->fs||prefs->play_monitor!=prefs->gui_monitor||
	   (mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))
	  &&prefs->show_framecount) {
	gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),framecount);
      }
      g_free(framecount);
      framecount=NULL;
    }

    if (was_preview) {
#ifndef IS_MINGW
      info_file=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
#else
      info_file=g_build_filename(prefs->tmpdir,cfile->handle,"status",NULL);
#endif
      // preview
      if (prefs->safer_preview&&cfile->proc_ptr!=NULL&&cfile->proc_ptr->frames_done>0&&
	  frame>=(cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start)) {
	mainw->cancelled=CANCEL_PREVIEW_FINISHED;
	mainw->noswitch=noswitch;
	if (framecount!=NULL) g_free(framecount);
	return;
      }

      // play preview
      if (cfile->opening||(cfile->next_event!=NULL&&cfile->proc_ptr==NULL)) {

	fname_next=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,cfile->handle,frame+1,prefs->image_ext);

	if (!mainw->fs&&prefs->show_framecount&&!mainw->is_rendering) {
	  if (framecount!=NULL) g_free(framecount);
	  if (cfile->frames>0&&cfile->frames!=123456789) {
	    framecount=g_strdup_printf("%9d/%d",frame,cfile->frames);
	  }
	  else {
	    framecount=g_strdup_printf("%9d",frame);
	  }
	  gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),framecount);
	  g_free(framecount);
	  framecount=NULL;
	}
	if (mainw->toy_type!=LIVES_TOY_NONE) {
	  // TODO - move into toys.c
	  if (mainw->toy_type==LIVES_TOY_MAD_FRAMES&&!mainw->fs) {
	    if (cfile->opening_only_audio) {
	      load_start_image  (1+(int) ((double)cfile->frames*rand()/(RAND_MAX+1.0)));
	      load_end_image (1+(int) ((double)cfile->frames*rand()/(RAND_MAX+1.0)));
	    }
	    else {
	      load_start_image (1+(int) ((double)frame*rand()/(RAND_MAX+1.0)));
	      load_end_image (1+(int) ((double)frame*rand()/(RAND_MAX+1.0)));
	    }
	  }
	}
      }
      else {
	if (mainw->is_rendering||mainw->is_generating) {
	  fname_next=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,cfile->handle,frame+1,prefs->image_ext);
	}
	else {
	  if (!mainw->keep_pre) {
	    img_ext="mgk";
	  }
	  else {
	    img_ext="pre";
	  }
	  fname_next=g_strdup_printf("%s/%s/%08d.pre",prefs->tmpdir,cfile->handle,frame+1);
	}
      }
      mainw->actual_frame=frame;
    }

    // maybe the performance finished and we weren't looping
    if ((mainw->actual_frame<1||mainw->actual_frame>cfile->frames)&&
	(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)&&!mainw->is_rendering) {
      mainw->noswitch=noswitch;
      if (framecount!=NULL) g_free(framecount);
      return;
    }
  

    // limit max frame size unless we are saving to disk or rendering

    // frame_layer will in any case be equal to or smaller than this depending on maximum source frame size 

    if (!(mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&
	  ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||
	   (mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&
	    mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&
	    mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)))) {
      get_max_opsize(&opwidth,&opheight);
    }


    ////////////////////////////////////////////////////////////
    // load a frame from disk buffer

    do {
      if (mainw->frame_layer!=NULL) {
	weed_layer_free(mainw->frame_layer);
	mainw->frame_layer=NULL;
      }

      if (mainw->is_rendering&&!(cfile->proc_ptr!=NULL&&mainw->preview)) {
	// here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
	weed_timecode_t tc=mainw->cevent_tc;

	if (mainw->clip_index[0]==mainw->scrap_file&&mainw->clip_index[0]>-1&&mainw->num_tracks==1) {
	  // do not apply fx, just pull frame
	  mainw->frame_layer=weed_plant_new(WEED_PLANT_CHANNEL);
	  weed_set_int_value(mainw->frame_layer,"clip",mainw->clip_index[0]);
	  weed_set_int_value(mainw->frame_layer,"frame",mainw->frame_index[0]);
	  if (!pull_frame(mainw->frame_layer,get_image_ext_for_type(cfile->img_type),tc)) {
	    weed_plant_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;
	  }
	}
	else {
	  int i;
	  weed_plant_t **layers;
	  layers=(weed_plant_t **)g_malloc((mainw->num_tracks+1)*sizeof(weed_plant_t *));
	  for (i=0;i<mainw->num_tracks;i++) {
	    layers[i]=weed_plant_new(WEED_PLANT_CHANNEL);
	    weed_set_int_value(layers[i],"clip",mainw->clip_index[i]);
	    weed_set_int_value(layers[i],"frame",mainw->frame_index[i]);
	    weed_set_int_value(layers[i],"current_palette",(mainw->clip_index[i]==-1||
							    mainw->files[mainw->clip_index[i]]->img_type==
							    IMG_TYPE_JPEG)?WEED_PALETTE_RGB24:WEED_PALETTE_RGBA32);
	    weed_set_voidptr_value(layers[i],"pixel_data",NULL);
	  }
	  layers[i]=NULL;
	  
	  mainw->frame_layer=weed_apply_effects(layers,mainw->filter_map,tc,opwidth,opheight,mainw->pchains);
	  
	  for (i=0;layers[i]!=NULL;i++) if (layers[i]!=mainw->frame_layer) weed_plant_free(layers[i]);
	  g_free(layers);
	}

	if (mainw->internal_messaging) {
	  // this happens if we are calling from multitrack, or apply rte.  We get our mainw->frame_layer and exit.
	  mainw->noswitch=noswitch;
	  if (framecount!=NULL) g_free(framecount);
	  return;
	}
      }
      else {
	// normal playback in the clip editor, or applying a non-realtime effect
	if (!mainw->preview||g_file_test(fname_next,G_FILE_TEST_EXISTS)) {
	  mainw->frame_layer=weed_plant_new(WEED_PLANT_CHANNEL);
	  weed_set_int_value(mainw->frame_layer,"clip",mainw->current_file);
	  weed_set_int_value(mainw->frame_layer,"frame",mainw->actual_frame);
	  if (img_ext==NULL) img_ext=get_image_ext_for_type(cfile->img_type);
	  if (!pull_frame_at_size(mainw->frame_layer,img_ext,(weed_timecode_t)mainw->currticks,
				  cfile->hsize,cfile->vsize,WEED_PALETTE_END)) {
	    if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;

	    if (cfile->opening && cfile->img_type==IMG_TYPE_PNG && sget_file_size(fname_next)==0) {
	      if (++bad_frame_count>BFC_LIMIT) {
		mainw->cancelled=check_for_bad_ffmpeg();
		bad_frame_count=0;
	      }
	      else g_usleep(prefs->sleep_time);
	    }
	  }
	}

	if ((cfile->next_event==NULL&&mainw->is_rendering&&!mainw->switch_during_pb&&
	     (mainw->multitrack==NULL||(!mainw->multitrack->is_rendering&&!mainw->is_generating)))||
	    ((!mainw->is_rendering||(mainw->multitrack!=NULL&&mainw->multitrack->is_rendering))&&
	     mainw->preview&&mainw->frame_layer==NULL)) {
	  // preview ended
	  if (!cfile->opening) mainw->cancelled=CANCEL_NO_MORE_PREVIEW;
	  if (mainw->cancelled) {
	    g_free(fname_next);
	    g_free(info_file);
	    mainw->noswitch=noswitch;
	    if (framecount!=NULL) g_free(framecount);
	    return;
	  }
	  gettimeofday(&tv, NULL);
	  mainw->currticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
	  mainw->startticks=mainw->currticks+mainw->deltaticks;
	}
	  
	img_ext=NULL;
	
	if (mainw->internal_messaging) {
	  mainw->noswitch=noswitch;
	  if (framecount!=NULL) g_free(framecount);
	  return;
	}
	
	if (mainw->frame_layer==NULL&&(!mainw->preview||(mainw->multitrack!=NULL&&!cfile->opening))) {
	  mainw->noswitch=noswitch;
	  if (framecount!=NULL) g_free(framecount);
	  return;
	}

	if (mainw->preview&&mainw->frame_layer==NULL&&(mainw->event_list==NULL||cfile->opening)) {
	  FILE *fd;
	  // non-realtime effect preview
	  // check effect to see if it finished yet
	  if ((fd=fopen(info_file,"r"))) {
	    clear_mainw_msg();
	    do {
	      retval=0;
	      mainw->read_failed=FALSE;
	      lives_fgets(mainw->msg,512,fd);
	      if (mainw->read_failed) retval=do_read_failed_error_s_with_retry(info_file,NULL,NULL);
	    } while (retval==LIVES_RETRY);
	    fclose(fd);
	    if (!strncmp(mainw->msg,"completed",9)||!strncmp(mainw->msg,"error",5)) {
	      // effect completed whilst we were busy playing a preview
	      if (mainw->preview_box!=NULL) gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Play"));
	      gtk_widget_set_tooltip_text( mainw->m_playbutton,_ ("Play"));
	      if (cfile->opening&&!cfile->is_loaded) {
		if (mainw->toy_type==LIVES_TOY_TV) {
		  on_toy_activate(NULL,GINT_TO_POINTER(LIVES_TOY_NONE));
		}
	      }
	      mainw->preview=FALSE;
	    }
	    else {
	      g_usleep(prefs->sleep_time);
	    }
	  }
	  else {
	    g_usleep(prefs->sleep_time);
	  }
	  
	  // or we reached the end of the preview
	  if ((!cfile->opening&&frame>=(cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start))||
	      (cfile->opening&&(mainw->toy_type==LIVES_TOY_TV||!mainw->preview||mainw->effects_paused))) {
	    if (mainw->toy_type==LIVES_TOY_TV) {
	      // force a loop (set mainw->cancelled to 100 to play selection again)
	      mainw->cancelled=CANCEL_KEEP_LOOPING;
	    }
	    else mainw->cancelled=CANCEL_NO_MORE_PREVIEW;
	    g_free(info_file);
	    g_free(fname_next);
	    if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;
	    mainw->noswitch=noswitch;
	    if (framecount!=NULL) g_free(framecount);
	    return;
	  }
	  else if (mainw->preview||cfile->opening) lives_widget_context_update();
	}
      }
    } while (mainw->frame_layer==NULL&&mainw->cancelled==CANCEL_NONE&&cfile->clip_type==CLIP_TYPE_DISK);

    
    // from this point onwards we don't need to keep mainw->frame_layer around when we return

    if (G_UNLIKELY((mainw->frame_layer==NULL)||mainw->cancelled>0)) {
      if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
      mainw->frame_layer=NULL;
      mainw->noswitch=noswitch;
      if (framecount!=NULL) g_free(framecount);
      return;
    }

    if (was_preview) {
      g_free(fname_next);
      g_free(info_file);
    }

    if (prefs->show_player_stats) {
      mainw->fps_measure++;
    }

    // OK. Here is the deal now. We have a layer from the current file, current frame. 
    // We will pass this into the effects, and we will get back a layer.
    // The palette of the effected layer could be any Weed palette. 
    // We will pass the layer to all playback plugins.
    // Finally we may want to end up with a GkdPixbuf (unless the playback plugin is VPP_DISPLAY_LOCAL 
    // and we are in full screen mode).

    if ((mainw->current_file!=mainw->scrap_file||mainw->multitrack!=NULL)&&
	!(mainw->is_rendering&&!(cfile->proc_ptr!=NULL&&mainw->preview))&&!(mainw->multitrack!=NULL&&cfile->opening)) {
      if (is_virtual_frame(mainw->current_file,mainw->actual_frame)||
	  ((weed_get_int_value(mainw->frame_layer,"height",&weed_error)==cfile->vsize)&&
	   (weed_get_int_value(mainw->frame_layer,"width",&weed_error)*
	    weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->frame_layer)))==cfile->hsize)) {
	if ((mainw->rte!=0||mainw->is_rendering)&&(mainw->current_file!=mainw->scrap_file||mainw->multitrack!=NULL)) {
	  mainw->frame_layer=on_rte_apply (mainw->frame_layer, opwidth, opheight, (weed_timecode_t)mainw->currticks);
	}
      }
      else {
	if (!mainw->resizing&&!cfile->opening) {
	  // warn the user after playback that badly sized frames were found
	  mainw->size_warn=TRUE;
	}
      }
    }

    ////////////////////////
#ifdef ENABLE_JACK
    if (!mainw->foreign&&mainw->jackd!=NULL&&prefs->audio_player==AUD_PLAYER_JACK) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) jack_try_reconnect();

      lives_alarm_clear(alarm_handle);
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (!mainw->foreign&&mainw->pulsed!=NULL&&prefs->audio_player==AUD_PLAYER_PULSE) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	sched_yield(); // wait for seek
      }

      if (timeout) pulse_try_reconnect();

      lives_alarm_clear(alarm_handle);
    }

#endif

    // save to scrap_file now if we have to
    if (mainw->record&&!rec_after_pb&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&
	((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||
	 (mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&
	  mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&
	  mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE))) {
      if (!rec_after_pb) save_to_scrap_file (mainw->frame_layer);
      get_max_opsize(&opwidth,&opheight);
    }
 
    if (mainw->playing_file>-1&&prefs->letterbox&&mainw->fs&&
	(mainw->multitrack==NULL||mainw->sep_win)&&
	(!mainw->ext_playback||(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY))) {
      // consider letterboxing
      lb_width=cfile->hsize;
      lb_height=cfile->vsize;

      if (mainw->multitrack==NULL&&mainw->is_rendering&&
	  !(cfile->proc_ptr!=NULL&&mainw->preview)&&mainw->clip_index[0]>-1) {
	if (mainw->clip_index[0]==mainw->scrap_file&&mainw->num_tracks==1) {
	  // scrap file playback - use original clip size
	  lb_width=weed_get_int_value(mainw->frame_layer,"width",&weed_error);
	  lb_height=weed_get_int_value(mainw->frame_layer,"height",&weed_error);
	}
	else {
	  // playing from event list, use original clip size
	  lb_width=mainw->files[mainw->clip_index[0]]->hsize;
	  lb_height=mainw->files[mainw->clip_index[0]]->vsize;
	}
      }

      // calc inner frame size
      calc_maxspect(opwidth,opheight,&lb_width,&lb_height);
      if (lb_width==opwidth&&lb_height==opheight) lb_width=lb_height=0;
    }

    if (mainw->ext_playback&&(mainw->vpp->capabilities&VPP_CAN_RESIZE)&&lb_width==0) {

      // here we are outputting video through a video playback plugin which can resize: thus we just send whatever we have
      // we need only to convert the palette to whatever was agreed with the plugin when we called set_palette() 
      // in plugins.c
      // 
      // if we want letterboxing we do this ourselves, later in code


      weed_plant_t *frame_layer=NULL;
      weed_plant_t *return_layer=NULL;

      layer_palette=weed_layer_get_palette(mainw->frame_layer);

      if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) && 
	  ((weed_palette_is_rgb_palette(layer_palette) && 
	    !(weed_palette_is_rgb_palette(mainw->vpp->palette))) || 
	(weed_palette_is_lower_quality(mainw->vpp->palette,layer_palette)))) {
	// mainw->frame_layer is RGB and so is our screen, but plugin is YUV
	// so copy layer and convert, retaining original
	frame_layer=weed_layer_copy(NULL,mainw->frame_layer);
      }
      else frame_layer=mainw->frame_layer;

      convert_layer_palette(frame_layer,mainw->vpp->palette,mainw->vpp->YUV_clamping);
      
      // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
      compact_rowstrides(frame_layer);

      pd_array=weed_get_voidptr_array(frame_layer,"pixel_data",&weed_error);

      if (mainw->stream_ticks==-1) mainw->stream_ticks=(mainw->currticks);

      if (rec_after_pb) {
	// record output from playback plugin
	int retwidth=mainw->pwidth/weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
	int retheight=mainw->pheight;

	return_layer=weed_layer_new(retwidth,retheight,NULL,mainw->vpp->palette);

	if (weed_palette_is_yuv_palette(mainw->vpp->palette)) {
	  weed_set_int_value(return_layer,"YUV_clamping",mainw->vpp->YUV_clamping);
	  weed_set_int_value(return_layer,"YUV_subspace",mainw->vpp->YUV_subspace);
	  weed_set_int_value(return_layer,"YUV_sampling",mainw->vpp->YUV_sampling);
	}

	create_empty_pixel_data(return_layer,FALSE,TRUE);

	// vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
	compact_rowstrides(return_layer);

	retdata=weed_get_voidptr_array(return_layer,"pixel_data",&weed_error);

      }

      // chain any data to the playback plugin
      if (!(mainw->preview||mainw->is_rendering)) {
	// chain any data pipelines
	if (mainw->pconx!=NULL) {
	  pthread_mutex_lock(&mainw->data_mutex);
	  pconx_chain_data(-2,0);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	if (mainw->cconx!=NULL) cconx_chain_data(-2,0);
      }

      if (!(*mainw->vpp->render_frame)(weed_get_int_value(frame_layer,"width",&weed_error),
				       weed_get_int_value(mainw->frame_layer,"height",&weed_error),
				       mainw->currticks-mainw->stream_ticks,pd_array,retdata,mainw->vpp->play_params)) {
	vid_playback_plugin_exit();
	if (return_layer!=NULL) weed_layer_free(return_layer);
	weed_free(retdata);
	return_layer=NULL;
      }
      weed_free(pd_array);

      if (frame_layer!=mainw->frame_layer) {
	weed_layer_free(frame_layer);
      }

      if (return_layer!=NULL) {
	int width=MIN(weed_get_int_value(frame_layer,"width",&weed_error),
		      weed_get_int_value(return_layer,"width",&weed_error));
	int height=MIN(weed_get_int_value(mainw->frame_layer,"height",&weed_error),
		       weed_get_int_value(return_layer,"height",&weed_error));
	resize_layer(return_layer,width,height,LIVES_INTERP_FAST,WEED_PALETTE_END,0); 

	save_to_scrap_file (return_layer);
	weed_layer_free(return_layer);
	weed_free(retdata);
	return_layer=NULL;
      }

      if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) {      
	if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
	mainw->frame_layer=NULL;
	mainw->noswitch=noswitch;
	
	if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor)&&
	    mainw->current_file!=mainw->scrap_file) get_play_times();
	if (mainw->multitrack!=NULL&&!cfile->opening) animate_multitrack(mainw->multitrack);
	if (framecount!=NULL) g_free(framecount);
	
#ifdef ENABLE_OSC
	// format is now msg|timecode|fgclip|fgframe|fgfps|
	lives_osc_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH,(const gchar *)
			 (tmp=g_strdup_printf("%.8f|%d|%d|%.3f|",(double)mainw->currticks/U_SEC,
					      mainw->current_file,mainw->actual_frame,cfile->pb_fps)));
	g_free(tmp);
#endif
	
	return;
      }

    }

    if ((mainw->multitrack==NULL&&mainw->double_size&&(!prefs->ce_maxspect||mainw->sep_win))||
	(mainw->fs&&(!mainw->ext_playback||!(mainw->vpp->capabilities&VPP_CAN_RESIZE)))||
	(mainw->must_resize&&((mainw->multitrack==NULL&&mainw->sep_win)||
			      (mainw->multitrack!=NULL&&!mainw->sep_win)))) {
      if (!mainw->ext_playback||(mainw->pwidth!=mainw->vpp->fwidth||mainw->pheight!=mainw->vpp->fheight)) {
	if (mainw->multitrack!=NULL) {
	  if (!mainw->fs||mainw->play_window==NULL) {
	    if (mainw->play_window==NULL) {
	      mainw->pwidth=mainw->files[mainw->multitrack->render_file]->hsize;
	      mainw->pheight=mainw->files[mainw->multitrack->render_file]->vsize;
	      calc_maxspect(mainw->multitrack->play_width,mainw->multitrack->play_height,&mainw->pwidth,&mainw->pheight);
	    }
	    else {
	      mainw->pwidth=cfile->hsize;
	      mainw->pheight=cfile->vsize;
	    }
	  }
	  else {
	    if (prefs->play_monitor==0) {
	      mainw->pwidth=mainw->scr_width;
	      mainw->pheight=mainw->scr_height;
	    }
	    else {
	      if (mainw->play_window!=NULL) pmonitor=prefs->play_monitor;
	      else pmonitor=prefs->gui_monitor;
	      mainw->pwidth=mainw->mgeom[pmonitor-1].width;
	      mainw->pheight=mainw->mgeom[pmonitor-1].height;
	    }
	  }
	}
      }
    }
    else {
      boolean size_ok=FALSE;

      pmonitor=prefs->play_monitor;

      mainw->pwidth=cfile->hsize;
      mainw->pheight=cfile->vsize;
   
      if (!mainw->is_rendering) {
	do {
	  if (pmonitor==0) {
	    if (mainw->pwidth>mainw->scr_width-SCR_WIDTH_SAFETY||
		mainw->pheight>mainw->scr_height-SCR_HEIGHT_SAFETY) { 
	      mainw->pheight=(mainw->pheight>>2)<<1;
	      mainw->pwidth=(mainw->pwidth>>2)<<1;
	      mainw->sepwin_scale/=2.;
	    }
	    else size_ok=TRUE;
	  }
	  else {
	    if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-SCR_WIDTH_SAFETY||
		mainw->pheight>mainw->mgeom[pmonitor-1].height-SCR_HEIGHT_SAFETY) {
	      mainw->pheight=(mainw->pheight>>2)<<1;
	      mainw->pwidth=(mainw->pwidth>>2)<<1;
	      mainw->sepwin_scale/=2.;
	    }
	    else size_ok=TRUE;
	  }
	} while (!size_ok);
      }

      if (mainw->multitrack==NULL&&mainw->play_window==NULL&&prefs->ce_maxspect) {
#if GTK_CHECK_VERSION(3,0,0)
	int rwidth=mainw->ce_frame_width-H_RESIZE_ADJUST*2;
	int rheight=mainw->ce_frame_height-V_RESIZE_ADJUST*2;
	if (mainw->double_size) {
	  rwidth*=4;
	  rheight*=4;
	}
#else
	int rwidth=lives_widget_get_allocation_width(mainw->image274);
	int rheight=lives_widget_get_allocation_height(mainw->image274);
#endif
	if (mainw->double_size) {
	  mainw->pwidth=(mainw->pwidth-H_RESIZE_ADJUST)*4+H_RESIZE_ADJUST;
	  mainw->pheight=(mainw->pheight-V_RESIZE_ADJUST)*4+H_RESIZE_ADJUST;
	}
	calc_maxspect(rwidth,rheight,&mainw->pwidth,&mainw->pheight);
	if (mainw->pwidth<2) mainw->pwidth=weed_get_int_value(mainw->frame_layer,"width",&weed_error);
	if (mainw->pheight<2) mainw->pheight=weed_get_int_value(mainw->frame_layer,"height",&weed_error);
      }

    }


    if (mainw->ext_playback&&(!(mainw->vpp->capabilities&VPP_CAN_RESIZE)||lb_width!=0)) {
      // here we are playing through an external video playback plugin which cannot resize
      // we must resize to whatever width and height we set when we called init_screen() in the plugin
      // i.e. mainw->vpp->fwidth, mainw->vpp fheight

      // both dimensions are in RGB(A) pixels, so we must adjust here and send 
      // macropixel size in the plugin's render_frame()

      // - this is also used if we are letterboxing to fullscreen with an external plugin

      weed_plant_t *frame_layer=NULL;
      weed_plant_t *return_layer=NULL;

      layer_palette=weed_layer_get_palette(mainw->frame_layer);

      interp=get_interp_value(prefs->pb_quality);

      if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) && mainw->vpp->fwidth>0 && mainw->vpp->fheight>0 && 
	  ((mainw->vpp->fwidth/weed_palette_get_pixels_per_macropixel(layer_palette)<
	   mainw->pwidth) || 
	  (mainw->vpp->fheight/weed_palette_get_pixels_per_macropixel(layer_palette)<
	   mainw->pheight))) {
	// mainw->frame_layer will be downsized for the plugin but upsized for screen
	// so copy layer and convert, retaining original
	frame_layer=weed_layer_copy(NULL,mainw->frame_layer);
      }
      else frame_layer=mainw->frame_layer;

      if (frame_layer==mainw->frame_layer && !(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) && 
	  ((weed_palette_is_rgb_palette(layer_palette) && 
	  !(weed_palette_is_rgb_palette(mainw->vpp->palette))) || 
	(weed_palette_is_lower_quality(mainw->vpp->palette,layer_palette)))) {
	// mainw->frame_layer is RGB and so is our screen, but plugin is YUV
	// so copy layer and convert, retaining original
	frame_layer=weed_layer_copy(NULL,mainw->frame_layer);
      }

      if ((mainw->vpp->fwidth>0&&mainw->vpp->fheight>0)&&lb_width==0) {
	resize_layer(frame_layer,mainw->vpp->fwidth/weed_palette_get_pixels_per_macropixel(layer_palette),
		     mainw->vpp->fheight,interp,mainw->vpp->palette,mainw->vpp->YUV_clamping);
      }

      // resize_layer can change palette
      layer_palette=weed_get_int_value(frame_layer,"current_palette",&weed_error);
    
      if (frame_layer==mainw->frame_layer && !(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) && 
	  ((weed_palette_is_rgb_palette(layer_palette) && 
	  !(weed_palette_is_rgb_palette(mainw->vpp->palette))) || 
	(weed_palette_is_lower_quality(mainw->vpp->palette,layer_palette)))) {
	// mainw->frame_layer is RGB and so is our screen, but plugin is YUV
	// so copy layer and convert, retaining original
	frame_layer=weed_layer_copy(NULL,mainw->frame_layer);
      }

      pwidth=weed_get_int_value(frame_layer,"width",&weed_error)*
	weed_palette_get_pixels_per_macropixel(layer_palette);
      pheight=weed_get_int_value(frame_layer,"height",&weed_error);

      if (mainw->fs&&(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)) {
	mainw->vpp->fwidth=mainw->scr_width;
	mainw->vpp->fheight=mainw->scr_height;
      }

      convert_layer_palette(frame_layer,mainw->vpp->palette,mainw->vpp->YUV_clamping);

      if (mainw->vpp->fwidth!=pwidth||mainw->vpp->fheight!=pheight||lb_width!=0) {

	if (lb_width==0) {
	  lb_width=pwidth;
	  lb_height=pheight;

	  calc_maxspect(mainw->vpp->fwidth,mainw->vpp->fheight,&lb_width,&lb_height);
	}

	letterbox_layer(frame_layer,lb_width/
			weed_palette_get_pixels_per_macropixel(layer_palette),
			lb_height,mainw->vpp->fwidth/
			weed_palette_get_pixels_per_macropixel(layer_palette),
			mainw->vpp->fheight);

      }

      if (mainw->stream_ticks==-1) mainw->stream_ticks=(mainw->currticks);

      // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
      compact_rowstrides(frame_layer);

      if (rec_after_pb) {
	// record output from playback plugin
	int retwidth=mainw->pwidth/weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
	int retheight=mainw->pheight;
	
	return_layer=weed_layer_new(retwidth,retheight,NULL,mainw->vpp->palette);

	if (weed_palette_is_yuv_palette(mainw->vpp->palette)) {
	  weed_set_int_value(return_layer,"YUV_clamping",mainw->vpp->YUV_clamping);
	  weed_set_int_value(return_layer,"YUV_subspace",mainw->vpp->YUV_subspace);
	  weed_set_int_value(return_layer,"YUV_sampling",mainw->vpp->YUV_sampling);
	}

	create_empty_pixel_data(return_layer,FALSE,TRUE);

	// vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
	compact_rowstrides(return_layer);

	retdata=weed_get_voidptr_array(return_layer,"pixel_data",&weed_error);
      }

      // chain any data to the playback plugin
      if (!(mainw->preview||mainw->is_rendering)) {
	// chain any data pipelines
	if (mainw->pconx!=NULL) {
	  pthread_mutex_lock(&mainw->data_mutex);
	  pconx_chain_data(-2,0);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	if (mainw->cconx!=NULL) cconx_chain_data(-2,0);
      }

      if (!(*mainw->vpp->render_frame)(weed_get_int_value(frame_layer,"width",&weed_error),
				       weed_get_int_value(frame_layer,"height",&weed_error),
				       mainw->currticks-mainw->stream_ticks,
				       (pd_array=weed_get_voidptr_array(frame_layer,"pixel_data",&weed_error)),
				       retdata,mainw->vpp->play_params)) {
	vid_playback_plugin_exit();
	if (return_layer!=NULL) {
	  weed_layer_free(return_layer);
	  weed_free(retdata);
	  return_layer=NULL;
	}
      }
      g_free(pd_array);

      if (frame_layer!=mainw->frame_layer) {
	weed_layer_free(frame_layer);
      }

      if (return_layer!=NULL) {
	int width=MIN(weed_get_int_value(frame_layer,"width",&weed_error),
		      weed_get_int_value(return_layer,"width",&weed_error));
	int height=MIN(weed_get_int_value(mainw->frame_layer,"height",&weed_error),
		       weed_get_int_value(return_layer,"height",&weed_error));
	resize_layer(return_layer,width,height,LIVES_INTERP_FAST,WEED_PALETTE_END,0); 

	save_to_scrap_file (return_layer);
	weed_layer_free(return_layer);
	weed_free(retdata);
	return_layer=NULL;
      }

      if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) {
	mainw->noswitch=noswitch;

	if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor)&&
	    mainw->current_file!=mainw->scrap_file) get_play_times();
	if (mainw->multitrack!=NULL&&!cfile->opening) animate_multitrack(mainw->multitrack);
	if (framecount!=NULL) g_free(framecount);
      
#ifdef ENABLE_OSC
	// format is now msg|timecode|fgclip|fgframe|fgfps|
	lives_osc_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH,(const gchar *)
			 (tmp=g_strdup_printf("%.8f|%d|%d|%.3f|",(double)mainw->currticks/U_SEC,
					      mainw->current_file,mainw->actual_frame,cfile->pb_fps)));
	g_free(tmp);
#endif
	
	return;
      }
    }
  
    ////////////////////////////////////////////////////////
    
    // local display - either we are playing with no playback plugin, or else the playback plugin has no 
    // local display of its own


    layer_palette=weed_layer_get_palette(mainw->frame_layer);

    if (cfile->img_type==IMG_TYPE_JPEG||!weed_palette_has_alpha_channel(layer_palette)) cpal=WEED_PALETTE_RGB24;
    else cpal=WEED_PALETTE_RGBA32;

    if (mainw->fs&&!mainw->ext_playback&&(mainw->multitrack==NULL||mainw->sep_win)) {
      // set again, in case vpp was turned off because of preview conditions
      if (!mainw->sep_win) {
	do {
	  mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe);
	  mainw->pheight=lives_widget_get_allocation_height(mainw->playframe);
	  if (mainw->pwidth * mainw->pheight==0) {
	    lives_widget_context_update();
	  }
	} while (mainw->pwidth * mainw->pheight == 0);
      }
      else {
	if (prefs->play_monitor==0) {
	  mainw->pwidth=mainw->scr_width;
	  mainw->pheight=mainw->scr_height;
	}
	else {
	  if (mainw->play_window!=NULL) pmonitor=prefs->play_monitor;
	  else pmonitor=prefs->gui_monitor;
	  mainw->pwidth=mainw->mgeom[pmonitor-1].width;
	  mainw->pheight=mainw->mgeom[pmonitor-1].height;
	}
      }
    }

    if (mainw->play_window!=NULL&&!mainw->fs) {
      mainw->pwidth=lives_widget_get_allocation_width(mainw->play_window);
      mainw->pheight=lives_widget_get_allocation_height(mainw->play_window);
    }

    pwidth=weed_get_int_value(mainw->frame_layer,"width",&weed_error)*
      weed_palette_get_pixels_per_macropixel(layer_palette);
    pheight=weed_get_int_value(mainw->frame_layer,"height",&weed_error)*
      weed_palette_get_pixels_per_macropixel(layer_palette);

    if (pwidth!=mainw->pwidth||pheight!=mainw->pheight||lb_width!=0) {
      if (lb_width!=0) {
	convert_layer_palette(mainw->frame_layer,cpal,0);

	letterbox_layer(mainw->frame_layer,lb_width/
			weed_palette_get_pixels_per_macropixel(layer_palette),
			lb_height,mainw->pwidth/
			weed_palette_get_pixels_per_macropixel(layer_palette),
			mainw->pheight);
      }
      else {
	interp=get_interp_value(prefs->pb_quality);
	resize_layer(mainw->frame_layer,mainw->pwidth/weed_palette_get_pixels_per_macropixel(layer_palette),
		     mainw->pheight,interp,cpal,0);
	convert_layer_palette(mainw->frame_layer,cpal,0);
      }
    }
    else {
      if (mainw->play_window!=NULL&&GDK_IS_WINDOW (lives_widget_get_xwindow(mainw->play_window))) {
	interp=get_interp_value(prefs->pb_quality);
	resize_layer(mainw->frame_layer,mainw->pwidth/weed_palette_get_pixels_per_macropixel(layer_palette),
		     mainw->pheight,interp,cpal,0);
	convert_layer_palette(mainw->frame_layer,cpal,0);
      }
    }

    pixbuf=layer_to_pixbuf(mainw->frame_layer);
    weed_plant_free(mainw->frame_layer);
    mainw->frame_layer=NULL;

    mainw->noswitch=noswitch;

    // internal player, double size or fullscreen, or multitrack

    if (mainw->play_window!=NULL&&GDK_IS_WINDOW (lives_widget_get_xwindow(mainw->play_window))) {
      lives_painter_t *cr = lives_painter_create_from_widget(mainw->play_window);
      block_expose();

      lives_painter_set_source_pixbuf (cr, pixbuf, 0, 0);
      lives_painter_paint (cr);
      lives_painter_destroy (cr);

      unblock_expose();
    }
    else set_ce_frame_from_pixbuf(GTK_IMAGE(mainw->image274),pixbuf,NULL);

    if (mainw->multitrack!=NULL&&!cfile->opening) animate_multitrack(mainw->multitrack);

    else if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor||
			     (mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))&&
			     mainw->current_file!=mainw->scrap_file) 
      get_play_times();
    
    lives_object_unref(pixbuf);
    
#ifdef ENABLE_OSC
    // format is now msg|timecode|fgclip|fgframe|fgfps|
    lives_osc_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH,(const gchar *)
		     (tmp=g_strdup_printf("%.8f|%d|%d|%.3f|",(double)mainw->currticks/U_SEC,
					  mainw->current_file,mainw->actual_frame,cfile->pb_fps)));
    g_free(tmp);
#endif
    if (framecount!=NULL) g_free(framecount);
    return;
  }

  // record external window
  if (mainw->record_foreign) {
    gchar fname[PATH_MAX];
    gint xwidth,xheight;
    GError *gerror=NULL;
    lives_painter_t *cr = lives_painter_create_from_widget (mainw->playarea);
      

    if (mainw->rec_vid_frames==-1) gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),(tmp=g_strdup_printf("%9d",frame)));
    else {
      if (frame>mainw->rec_vid_frames) {
	mainw->cancelled=CANCEL_KEEP;
	if (cfile->frames>0) cfile->frames=mainw->rec_vid_frames;
	return;
      }

      gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),(tmp=g_strdup_printf("%9d/%9d",frame,mainw->rec_vid_frames)));
      g_free(tmp);
    }


#if GTK_CHECK_VERSION(3,0,0)
    xwidth=gdk_window_get_width(mainw->foreign_window);
    xheight=gdk_window_get_height(mainw->foreign_window);
    if ((pixbuf=gdk_pixbuf_get_from_window (mainw->foreign_window,
					      0,0,
					      xwidth,
					      xheight
					      ))!=NULL) {
#else
    gdk_window_get_size(mainw->foreign_window,&xwidth,&xheight);
    if ((pixbuf=gdk_pixbuf_get_from_drawable (NULL,GDK_DRAWABLE(mainw->foreign_window),
					      mainw->foreign_cmap,0,0,0,0,
					      xwidth,
					      xheight
					      ))!=NULL) {
#endif
      g_snprintf(fname,PATH_MAX,"%s/%s/%08d.%s",prefs->tmpdir,cfile->handle,frame,prefs->image_ext);
      do {
	if (gerror!=NULL) g_error_free(gerror);
	if (!strcmp(prefs->image_ext,"jpg")) lives_pixbuf_save(pixbuf, fname, IMG_TYPE_JPEG, 100, FALSE, &gerror);
	else if (!strcmp(prefs->image_ext,"png")) 
	  lives_pixbuf_save(pixbuf, fname, IMG_TYPE_PNG, 100, FALSE, &gerror);
      } while (gerror!=NULL);


      lives_painter_set_source_pixbuf (cr, pixbuf, 0, 0);
      lives_painter_paint (cr);
      lives_painter_destroy (cr);

      lives_object_unref(pixbuf);
      cfile->frames=frame;
    }
    else {
      do_error_dialog(_ ("LiVES was unable to capture this image\n\n"));
      mainw->cancelled=CANCEL_CAPTURE_ERROR;
    }

    if (frame==mainw->rec_vid_frames) mainw->cancelled=CANCEL_KEEP;

  }
  if (framecount!=NULL) g_free(framecount);
}


/** Save a pixbuf to a file using the specified imgtype and the specified quality/compression value */

GError *lives_pixbuf_save(GdkPixbuf *pixbuf, gchar *fname, lives_image_type_t imgtype, int quality, boolean do_chmod, 
			  GError **gerrorptr) {
  // CALLER should check for errors

  // fname should be in local charset

  // if do_chmod, we try to set permissions to default

#ifndef IS_MINGW
  mode_t xumask=0;

  if (do_chmod) {
    xumask=umask(DEF_FILE_UMASK);
  }
#endif

  if (imgtype==IMG_TYPE_JPEG) {
    gchar *qstr=g_strdup_printf("%d",quality);
    gdk_pixbuf_save (pixbuf, fname, "jpeg", gerrorptr, "quality", qstr, NULL);
    g_free(qstr);
  }
  else if (imgtype==IMG_TYPE_PNG) {
    gchar *cstr=g_strdup_printf("%d",(gint)((100.-(gdouble)quality+5.)/10.));
    gdk_pixbuf_save (pixbuf, fname, "png", gerrorptr, "compression", cstr, NULL);
    g_free(cstr);
  }
  else {
    //gdk_pixbuf_save_to_callback(...);
  }

#ifndef IS_MINGW
  if (do_chmod) {
    umask(xumask);
  }
#endif

  return *gerrorptr;
}



void close_current_file(gint file_to_switch_to) {
  // close the current file, and free the file struct and all sub storage
  gchar *com;
  gint index=-1;
  gint old_file=mainw->current_file;
  GList *list_index;
  boolean need_new_blend_file=FALSE;

  if (mainw->playing_file==-1) {
    if (mainw->current_file!=mainw->scrap_file) desensitize();
    gtk_widget_set_sensitive (mainw->playall, FALSE);
    gtk_widget_set_sensitive (mainw->m_playbutton, FALSE);
    gtk_widget_set_sensitive (mainw->m_playselbutton, FALSE);
    gtk_widget_set_sensitive (mainw->m_loopbutton, FALSE);
    gtk_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
    if (mainw->preview_box!=NULL) {
      gtk_widget_set_sensitive (mainw->p_playbutton, FALSE);
      gtk_widget_set_sensitive (mainw->p_playselbutton, FALSE);
      gtk_widget_set_sensitive (mainw->p_loopbutton, FALSE);
      gtk_widget_set_sensitive (mainw->p_rewindbutton, FALSE);
    }
    gtk_widget_set_sensitive (mainw->rewind, FALSE);
    gtk_widget_set_sensitive (mainw->select_submenu, FALSE);
    gtk_widget_set_sensitive (mainw->trim_submenu, FALSE);
    gtk_widget_set_sensitive (mainw->delaudio_submenu, FALSE);
    gtk_widget_set_sensitive (mainw->lock_selwidth, FALSE);
    gtk_widget_set_sensitive (mainw->show_file_info, FALSE);
    gtk_widget_set_sensitive (mainw->show_file_comments, FALSE);
    gtk_widget_set_sensitive (mainw->rename, FALSE);
    gtk_widget_set_sensitive (mainw->open, TRUE);
    gtk_widget_set_sensitive (mainw->capture, TRUE);
    gtk_widget_set_sensitive (mainw->preferences, TRUE);
    gtk_widget_set_sensitive (mainw->dsize, !mainw->fs);
    gtk_widget_set_sensitive (mainw->rev_clipboard, !(clipboard==NULL));
    gtk_widget_set_sensitive (mainw->show_clipboard_info, !(clipboard==NULL));
    gtk_widget_set_sensitive (mainw->playclip, !(clipboard==NULL));
    gtk_widget_set_sensitive (mainw->paste_as_new, !(clipboard==NULL));
    gtk_widget_set_sensitive (mainw->open_sel, TRUE);
    gtk_widget_set_sensitive (mainw->recaudio_submenu, TRUE);
    gtk_widget_set_sensitive (mainw->open_vcd_menu, TRUE);
    gtk_widget_set_sensitive (mainw->full_screen, TRUE);
#ifdef HAVE_WEBM
    gtk_widget_set_sensitive (mainw->open_loc_menu, TRUE);
#else
    gtk_widget_set_sensitive (mainw->open_loc, TRUE);
#endif
    gtk_widget_set_sensitive (mainw->open_device_menu, TRUE);
    gtk_widget_set_sensitive (mainw->recent_menu, TRUE);
    gtk_widget_set_sensitive (mainw->restore, TRUE);
    gtk_widget_set_sensitive (mainw->toy_tv, TRUE);
    gtk_widget_set_sensitive (mainw->toy_autolives, TRUE);
    gtk_widget_set_sensitive (mainw->toy_random_frames, TRUE);
    gtk_widget_set_sensitive (mainw->vj_load_set, !mainw->was_set);
    gtk_widget_set_sensitive (mainw->clear_ds, TRUE);
    gtk_widget_set_sensitive (mainw->gens_submenu, TRUE);
    gtk_widget_set_sensitive (mainw->mt_menu, TRUE);
    gtk_widget_set_sensitive (mainw->add_live_menu,TRUE);
    gtk_widget_set_sensitive (mainw->troubleshoot, TRUE);
#ifdef HAVE_YUV4MPEG
    gtk_widget_set_sensitive (mainw->open_yuv4m, TRUE);
#endif
  }
  //update the bar text
  if (mainw->current_file>-1) {
    int i;
    gchar *tmp;
    if (cfile->clip_type!=CLIP_TYPE_GENERATOR&&mainw->current_file!=mainw->scrap_file&&
	(mainw->multitrack==NULL||mainw->current_file!=mainw->multitrack->render_file)) {
      d_print ((tmp=g_strdup_printf(_ ("Closed file %s\n"),cfile->file_name)));
      g_free(tmp);
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_CLOSED,"");
#endif
    }
    
    // resize frame widgets to default
    cfile->hsize=mainw->def_width-H_RESIZE_ADJUST;
    cfile->vsize=mainw->def_height-V_RESIZE_ADJUST;
    
    for (i=0;i<FN_KEYS-1;i++) {
      if (mainw->clipstore[i]==mainw->current_file) mainw->clipstore[i]=0;
    }

    // this must all be done last...
    if (cfile->menuentry!=NULL) {
      // c.f. on_prevclip_activate
      list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
      do {
	if ((list_index=g_list_previous(list_index))==NULL) list_index=g_list_last (mainw->cliplist);
	index=GPOINTER_TO_INT (list_index->data);
      } while ((mainw->files[index]==NULL||mainw->files[index]->opening||mainw->files[index]->restoring||
		(index==mainw->scrap_file&&index>-1)||(!mainw->files[index]->frames&&mainw->playing_file>-1))&&
	       index!=mainw->current_file);
      if (index==mainw->current_file) index=-1;
      if (mainw->current_file!=mainw->scrap_file) remove_from_winmenu();
    }

    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->ext_src!=NULL) {
      gchar *cwd=g_get_current_dir();
      gchar *ppath=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
      lives_chdir(ppath,FALSE);
      g_free(ppath);
      close_decoder_plugin((lives_decoder_t *)cfile->ext_src);
      cfile->ext_src=NULL;

      lives_chdir(cwd,FALSE);
      g_free(cwd);
    }

    if (cfile->frame_index!=NULL) g_free(cfile->frame_index);
    if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);

    if (cfile->op_dir!=NULL) g_free(cfile->op_dir);

    if (cfile->clip_type!=CLIP_TYPE_GENERATOR&&!mainw->only_close) {
#ifdef IS_MINGW
      // kill any active processes: for other OSes the backend does this
      // get pid from backend
      FILE *rfile;
      ssize_t rlen;
      char val[16];
      int pid;
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);
      lives_win32_kill_subprocesses(pid,TRUE);
  
#endif

      com=g_strdup_printf("%s close \"%s\"",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE);
      g_free(com); 

      if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
      if (cfile->event_list!=NULL) event_list_free (cfile->event_list);

      if (cfile->layout_map!=NULL) {
	g_list_free_strings(cfile->layout_map);
	g_list_free(cfile->layout_map);
      }

    }

    if (cfile->subt!=NULL) subtitles_free(cfile);

    if (cfile->clip_type==CLIP_TYPE_YUV4MPEG) {
#ifdef HAVE_YUV4MPEG
      lives_yuv_stream_stop_read((lives_yuv4m_t *)cfile->ext_src);
      g_free (cfile->ext_src);
#endif
    }

    if (cfile->clip_type==CLIP_TYPE_VIDEODEV) {
#ifdef HAVE_UNICAP
      lives_vdev_free((lives_vdev_t *)cfile->ext_src);
      g_free (cfile->ext_src);
#endif
    }

    if (cfile->laudio_drawable!=NULL) {
      lives_painter_surface_destroy(cfile->laudio_drawable);
    }

    if (cfile->raudio_drawable!=NULL) {
      lives_painter_surface_destroy(cfile->raudio_drawable);
    }

    g_free(cfile);
    cfile=NULL;

    if (mainw->multitrack!=NULL&&mainw->current_file!=mainw->multitrack->render_file) {
      mt_delete_clips(mainw->multitrack,mainw->current_file);
    }

    if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
      mainw->first_free_file=mainw->current_file;

    if (!mainw->only_close) {
      if (file_to_switch_to>0&&mainw->files[file_to_switch_to]!=NULL) {
	if (mainw->playing_file==-1) {
	  switch_to_file((mainw->current_file=0),file_to_switch_to);
	  d_print("");
	}
	else do_quick_switch(file_to_switch_to);

	if (mainw->multitrack!=NULL&&old_file!=mainw->multitrack->render_file) {
	  mt_clip_select(mainw->multitrack,TRUE);
	}

	return;
      }
    }
    
    if (mainw->current_file==mainw->blend_file) {
      need_new_blend_file=TRUE;
    }

    mainw->preview_frame=0;
    
    if (!mainw->only_close) {
      // find another clip to switch to
      if (index>-1) {
	if (mainw->playing_file==-1) {
	  switch_to_file((mainw->current_file=0),index);
	  d_print("");
	}
	else do_quick_switch(index);
	if (need_new_blend_file) mainw->blend_file=mainw->current_file;

	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->clip_selected=-mainw->multitrack->clip_selected;
	  mt_clip_select(mainw->multitrack,TRUE);
	}

	return;
      }
      if (mainw->clips_available>0) {
	for (i=mainw->current_file-1;i>0;i--) {
	  if (!(mainw->files[i]==NULL)) {
	    if (mainw->playing_file==-1) {
	      switch_to_file((mainw->current_file=0),i);
	      d_print("");
	    }
	    else do_quick_switch(index);
	    if (need_new_blend_file) mainw->blend_file=mainw->current_file;

	    if (mainw->multitrack!=NULL) {
	      mainw->multitrack->clip_selected=-mainw->multitrack->clip_selected;
	      mt_clip_select(mainw->multitrack,TRUE);
	    }

	    return;
	  }
	}
	for (i=1;i<MAX_FILES;i++) {
	  if (!(mainw->files[i]==NULL)) {
	    if (mainw->playing_file==-1) {
	      switch_to_file((mainw->current_file=0),i);
	      d_print("");
	    }
	    else do_quick_switch(index);
	    if (need_new_blend_file) mainw->blend_file=mainw->current_file;

	    if (mainw->multitrack!=NULL) {
	      mainw->multitrack->clip_selected=-mainw->multitrack->clip_selected;
	      mt_clip_select(mainw->multitrack,TRUE);
	    }

	    return;
	  }
	}
      }
    }

    // no other clips
    mainw->current_file=-1;
    mainw->blend_file=-1;
    set_main_title(NULL,0);

    gtk_widget_set_sensitive (mainw->vj_save_set, FALSE);
    gtk_widget_set_sensitive (mainw->vj_load_set, TRUE);
    gtk_widget_set_sensitive (mainw->export_proj, FALSE);
    gtk_widget_set_sensitive (mainw->import_proj, FALSE);

    if (mainw->multitrack!=NULL) gtk_widget_set_sensitive (mainw->multitrack->load_set,TRUE);

    // can't use set_undoable, as we don't have a cfile
    set_menu_text(mainw->undo,_ ("_Undo"),TRUE);
    set_menu_text(mainw->redo,_ ("_Redo"),TRUE);
    gtk_widget_hide(mainw->redo);
    gtk_widget_show(mainw->undo);
    gtk_widget_set_sensitive(mainw->undo,FALSE);


    if (mainw->playing_file==-1&&mainw->play_window!=NULL) {
      // if the clip is loaded
      if (mainw->preview_box==NULL) {
	// create the preview box that shows frames...
	make_preview_box();
      }
      // add it the play window...
      if (lives_widget_get_parent(mainw->preview_box)==NULL) {
	gtk_widget_queue_draw(mainw->play_window);
	gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
      }

      gtk_widget_hide(mainw->preview_controls);

      // and resize it
      resize_play_window();

      play_window_set_title();

      load_preview_image(FALSE);
      gtk_widget_queue_resize(mainw->preview_box);

      //lives_widget_context_update();
    }
      
    if (mainw->multitrack==NULL) {
      resize(1);
      load_start_image (0);
      load_end_image (0);
    }
  }

  set_sel_label(mainw->sel_label);

  gtk_label_set_text(GTK_LABEL(mainw->vidbar),_ ("Video"));
  gtk_label_set_text(GTK_LABEL(mainw->laudbar),_ ("Left Audio"));
  gtk_label_set_text(GTK_LABEL(mainw->raudbar),_ ("Right Audio"));
    
  zero_spinbuttons();
  gtk_widget_hide (mainw->hruler);
  gtk_widget_hide (mainw->eventbox5);

  if (palette->style&STYLE_1) {
    gtk_widget_hide (mainw->vidbar);
    gtk_widget_hide (mainw->laudbar);
    gtk_widget_hide (mainw->raudbar);
  }
  else {
    gtk_widget_show (mainw->vidbar);
    gtk_widget_show (mainw->laudbar);
    gtk_widget_show (mainw->raudbar);
  }
  if (!mainw->only_close) {
    gtk_widget_queue_draw (mainw->LiVES);
    if (mainw->playing_file==-1) d_print("");

    if (mainw->multitrack!=NULL) {
      mainw->multitrack->clip_selected=-mainw->multitrack->clip_selected;
      mt_clip_select(mainw->multitrack,TRUE);
    }
  }

}


  
void switch_to_file(gint old_file, gint new_file) {
  // this function is used for full clip switching (during non-playback or non fs)
  gchar title[256];
  GtkWidget *active_image;
  gint orig_file=mainw->current_file;

  // should use close_current_file
  if (new_file==-1||new_file>MAX_FILES) {
    g_printerr("warning - attempt to switch to invalid clip %d\n",new_file);
    return;
  }

  if (mainw->files[new_file]==NULL) return;

  if (cfile!=NULL&&old_file*new_file>0&&cfile->opening) {
    if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
      do_error_dialog(_ ("\n\nLiVES cannot switch clips whilst opening if the audio player is set to mplayer.\nPlease adjust the playback options in Preferences and try again.\n"));
      return;
    }
  }

  mainw->current_file=new_file;

  if (mainw->playing_file==-1&&mainw->multitrack!=NULL) return;

  if (cfile->frames) {
    mainw->pwidth=cfile->hsize;
    mainw->pheight=cfile->vsize;

    mainw->play_start=1;
    mainw->play_end=cfile->frames;

    if (mainw->playing_file>-1) {
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
      changed_fps_during_pb (GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);
    }

    if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->event_list!=NULL&&!mainw->record)) 
      mainw->play_end=INT_MAX;
  }

  if (old_file!=new_file) {
    if (old_file*new_file) mainw->preview_frame=0;
    if (old_file!=-1) {
      if (old_file>0&&mainw->files[old_file]!=NULL&&mainw->files[old_file]->menuentry!=NULL&&
	  (mainw->files[old_file]->clip_type==CLIP_TYPE_DISK||mainw->files[old_file]->clip_type==CLIP_TYPE_FILE)) {
	gchar menutext[32768];
	get_menu_text_long(mainw->files[old_file]->menuentry,menutext);

	if (!mainw->files[old_file]->opening) {
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[old_file]->menuentry), NULL);
	}
	else {
	  active_image = gtk_image_new_from_stock ("gtk-no", GTK_ICON_SIZE_MENU);
	  gtk_widget_show (active_image);
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[old_file]->menuentry), active_image);
	}
	set_menu_text(mainw->files[old_file]->menuentry,menutext,FALSE);
      }
      gtk_widget_set_sensitive (mainw->select_new, (cfile->insert_start>0));
      gtk_widget_set_sensitive (mainw->select_last, (cfile->undo_start>0));
      if ((cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
	gtk_widget_set_sensitive(mainw->select_invert,TRUE);
      }
      else {
	gtk_widget_set_sensitive(mainw->select_invert,FALSE);
      }
      if (new_file*old_file>0&&mainw->files[old_file]!=NULL&&mainw->files[old_file]->opening) {
	// switch while opening - come out of processing dialog
	if (!(mainw->files[old_file]->proc_ptr==NULL)) {
	  gtk_widget_destroy (mainw->files[old_file]->proc_ptr->processing);
	  g_free (mainw->files[old_file]->proc_ptr);
	  mainw->files[old_file]->proc_ptr=NULL;
	}
      }
    }
  }

  if (!mainw->switch_during_pb&&!cfile->opening) {
    sensitize();
  }

  if ((mainw->playing_file==-1&&mainw->play_window!=NULL&&cfile->is_loaded)
      &&orig_file!=new_file) {
    // if the clip is loaded
    if (mainw->preview_box==NULL) {
      // create the preview box that shows frames...
      make_preview_box();
    }
    // add it the play window...
    if (lives_widget_get_parent(mainw->preview_box)==NULL) {
      gtk_widget_queue_draw(mainw->play_window);
      gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
    }

    gtk_widget_show(mainw->preview_controls);

    // and resize it
    resize_play_window();

    play_window_set_title();

    load_preview_image(FALSE);
    gtk_widget_queue_resize(mainw->preview_box);

    //lives_widget_context_update();
  }
  
  if (new_file>0) {
    lives_ruler_set_value (LIVES_RULER(mainw->hruler),cfile->pointer_time);
    if (!cfile->opening&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
      active_image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
    }
    else {
      active_image = gtk_image_new_from_stock ("gtk-yes", GTK_ICON_SIZE_MENU);
      load_start_image(0);
      load_end_image(0);
      gtk_widget_set_sensitive (mainw->rename, FALSE);
    }
    gtk_widget_show (active_image);
    if (cfile->menuentry!=NULL) {
      gchar menutext[32768];
      get_menu_text_long(cfile->menuentry,menutext);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (cfile->menuentry), active_image);
      set_menu_text(cfile->menuentry,menutext,FALSE);
    }
    if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) {
     reget_afilesize (mainw->current_file);
    }
  }

  if (!mainw->switch_during_pb) {
    // switch on/off loop video if we have/don't have audio
    if (cfile->achans==0) {
      mainw->loop=FALSE;
    }
    else {
      mainw->loop=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mainw->loop_video));
    }
    
    gtk_widget_set_sensitive (mainw->undo, cfile->undoable);
    gtk_widget_set_sensitive (mainw->redo, cfile->redoable);
    gtk_widget_set_sensitive (mainw->export_submenu, (cfile->achans>0));
    gtk_widget_set_sensitive (mainw->recaudio_submenu, TRUE);
    gtk_widget_set_sensitive (mainw->recaudio_sel, (cfile->frames>0));
    gtk_widget_set_sensitive (mainw->export_selaudio, (cfile->frames>0));
    gtk_widget_set_sensitive (mainw->append_audio, (cfile->achans>0));
    gtk_widget_set_sensitive (mainw->trim_submenu, (cfile->achans>0));
    gtk_widget_set_sensitive (mainw->trim_audio, mainw->current_file>0&&(cfile->achans*cfile->frames>0));
    gtk_widget_set_sensitive (mainw->trim_to_pstart, (cfile->achans>0&&cfile->pointer_time>0.));
    gtk_widget_set_sensitive (mainw->delaudio_submenu, (cfile->achans>0));
    gtk_widget_set_sensitive (mainw->delsel_audio, (cfile->frames>0));
    gtk_widget_set_sensitive (mainw->resample_audio, (cfile->achans>0&&capable->has_sox_sox));
    gtk_widget_set_sensitive (mainw->fade_aud_in, cfile->achans>0);
    gtk_widget_set_sensitive (mainw->fade_aud_out, cfile->achans>0);
    gtk_widget_set_sensitive (mainw->loop_video, (cfile->achans>0&&cfile->frames>0));
  }

  set_menu_text(mainw->undo,cfile->undo_text,TRUE);
  set_menu_text(mainw->redo,cfile->redo_text,TRUE);
  
  set_sel_label(mainw->sel_label);

  gtk_widget_show(mainw->vidbar);
  gtk_widget_show(mainw->laudbar);

  if (cfile->achans<2) {
    gtk_widget_hide(mainw->raudbar);
  }
  else {
    gtk_widget_show(mainw->raudbar);
  }

  if (cfile->redoable) {
    gtk_widget_show(mainw->redo);
    gtk_widget_hide(mainw->undo);
  }
  else {
    gtk_widget_hide(mainw->redo);
    gtk_widget_show(mainw->undo);
  }

  if (new_file>0) {
    if (cfile->menuentry!=NULL) {
      get_menu_text(cfile->menuentry,title);
      set_main_title(title,0);
    }
   else set_main_title(cfile->file_name,0);
  }

  if (cfile->frames==0) {
    zero_spinbuttons();
  }

  if (mainw->multitrack==NULL) resize(1);

  if (mainw->playing_file>-1) {
      if (mainw->fs) {
	//on_full_screen_activate (NULL,GINT_TO_POINTER (1));
      }
      else {
	if (!mainw->faded&&cfile->frames>0) {
	  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
	  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),1,cfile->frames);
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
	  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
	  
	  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
	  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),1,cfile->frames);
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
	  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);
          load_start_image(cfile->start);
          load_end_image(cfile->end);
	  load_frame_image(cfile->frameno);
	}
	if (mainw->double_size) {
	  frame_size_update();
	}
     }
  }
  else {
    // it is unfortunate that we might resize after this, and our timer bars will then look the wrong width
    if (mainw->is_ready) {
      get_play_times();
    }
    // ...so we need an expose event in callbacks.c
  }

  // if the file was opening, continue...
  if (cfile->opening) {
    open_file(cfile->file_name);
  }
}


void switch_audio_clip(gint new_file, boolean activate) {

  if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    if (mainw->jackd!=NULL) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      if (!activate) mainw->jackd->in_use=FALSE;

      while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) jack_try_reconnect();
      lives_alarm_clear(alarm_handle);

      if (mainw->jackd->playing_file>0) {
	jack_message.command=ASERVER_CMD_FILE_CLOSE;
	jack_message.data=NULL;
	jack_message.next=NULL;
	mainw->jackd->msgq=&jack_message;
	
	lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
	while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	  sched_yield(); // wait for seek
	}
	if (timeout) jack_try_reconnect();
	lives_alarm_clear(alarm_handle);
	
      }

      if (new_file<0||mainw->files[new_file]==NULL) {
	mainw->jackd->in_use=FALSE;
	return;
      }
      
      if (activate) mainw->jackd->in_use=TRUE;
      
      if (mainw->files[new_file]->achans>0) { 
	gint asigned=!(mainw->files[new_file]->signed_endian&AFORM_UNSIGNED);
	gint aendian=!(mainw->files[new_file]->signed_endian&AFORM_BIG_ENDIAN);
	mainw->jackd->num_input_channels=mainw->files[new_file]->achans;
	mainw->jackd->bytes_per_channel=mainw->files[new_file]->asampsize/8;
	if (activate&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) {
	  if (!mainw->files[new_file]->play_paused) 
	    mainw->jackd->sample_in_rate=mainw->files[new_file]->arate*mainw->files[new_file]->pb_fps/
	      mainw->files[new_file]->fps;
	  else mainw->jackd->sample_in_rate=mainw->files[new_file]->arate*mainw->files[new_file]->freeze_fps/
		 mainw->files[new_file]->fps;
	}
	else mainw->jackd->sample_in_rate=mainw->files[new_file]->arate;
	mainw->jackd->usigned=!asigned;
	mainw->jackd->seek_end=mainw->files[new_file]->afilesize;

	if ((aendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
	    (!aendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN))) 
	  mainw->jackd->reverse_endian=TRUE;
	else mainw->jackd->reverse_endian=FALSE;

	if (mainw->ping_pong) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
	else mainw->jackd->loop=AUDIO_LOOP_FORWARD;

	// tell jack server to open audio file and start playing it

	jack_message.command=ASERVER_CMD_FILE_OPEN;

	jack_message.data=g_strdup_printf("%d",new_file);

	jack_message2.command=ASERVER_CMD_FILE_SEEK;
	jack_message.next=&jack_message2;
	jack_message2.data=g_strdup_printf("%"PRId64,mainw->files[new_file]->aseek_pos);
	jack_message2.next=NULL;

	mainw->jackd->msgq=&jack_message;
	mainw->jackd->in_use=TRUE;

	if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
	  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
	    mainw->jackd->is_paused=mainw->files[new_file]->play_paused;
	    mainw->jackd->is_silent=FALSE;
	  }
	  
	  mainw->rec_aclip=new_file;
	  mainw->rec_avel=mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
	  mainw->rec_aseek=(gdouble)mainw->files[new_file]->aseek_pos/
	    (gdouble)(mainw->files[new_file]->arate*mainw->files[new_file]->achans*mainw->files[new_file]->asampsize/8);
	}
      }
      else {
	if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
	  mainw->rec_aclip=mainw->current_file;
	  mainw->rec_avel=0.;
	  mainw->rec_aseek=0.;
	}
      }
    }
#endif
  }

  // switch audio clip
 if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed!=NULL) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) pulse_try_reconnect();
      lives_alarm_clear(alarm_handle);

      if (mainw->pulsed->fd>0) {
	pulse_message.command=ASERVER_CMD_FILE_CLOSE;
	pulse_message.data=NULL;
	pulse_message.next=NULL;
	mainw->pulsed->msgq=&pulse_message;

	lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
	while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	  sched_yield(); // wait for seek
	}
	if (timeout) pulse_try_reconnect();
	lives_alarm_clear(alarm_handle);
      }

      if (new_file<0||mainw->files[new_file]==NULL) {
	mainw->pulsed->in_use=FALSE;
	return;
      }

      mainw->pulsed->in_use=TRUE;
      
      if (mainw->files[new_file]->achans>0) { 
	gint asigned=!(mainw->files[new_file]->signed_endian&AFORM_UNSIGNED);
	gint aendian=!(mainw->files[new_file]->signed_endian&AFORM_BIG_ENDIAN);
	mainw->pulsed->in_achans=mainw->files[new_file]->achans;
	mainw->pulsed->in_asamps=mainw->files[new_file]->asampsize;
	if (activate&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) {
	  if (!mainw->files[new_file]->play_paused) 
	    mainw->pulsed->in_arate=mainw->files[new_file]->arate*mainw->files[new_file]->pb_fps/
	      mainw->files[new_file]->fps;
	  else mainw->pulsed->in_arate=mainw->files[new_file]->arate*mainw->files[new_file]->freeze_fps/
		 mainw->files[new_file]->fps;
	}
	else mainw->pulsed->in_arate=mainw->files[new_file]->arate;
	mainw->pulsed->usigned=!asigned;
	mainw->pulsed->seek_end=mainw->files[new_file]->afilesize;
	if (mainw->files[new_file]->opening) mainw->pulsed->is_opening=TRUE;
	else mainw->pulsed->is_opening=FALSE;

	if ((aendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
	    (!aendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN))) 
	  mainw->pulsed->reverse_endian=TRUE;
	else mainw->pulsed->reverse_endian=FALSE;

	if (mainw->ping_pong) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
	else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;

	// tell pulse server to open audio file and start playing it

	pulse_message.command=ASERVER_CMD_FILE_OPEN;

	if (mainw->files[new_file]->opening) {
	  mainw->pulsed->is_opening=TRUE;
	}
	pulse_message.data=g_strdup_printf("%d",new_file);

	pulse_message2.command=ASERVER_CMD_FILE_SEEK;
	pulse_message.next=&pulse_message2;
	pulse_message2.data=g_strdup_printf("%"PRId64,mainw->files[new_file]->aseek_pos);
	pulse_message2.next=NULL;
	mainw->pulsed->msgq=&pulse_message;
	mainw->pulsed->in_use=TRUE;

	if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
	  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
	    mainw->pulsed->is_paused=mainw->files[new_file]->play_paused;
	  }
	    
	  mainw->rec_aclip=new_file;
	  mainw->rec_avel=mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
	  mainw->rec_aseek=(gdouble)mainw->files[new_file]->aseek_pos/
	    (gdouble)(mainw->files[new_file]->arate*mainw->files[new_file]->achans*mainw->files[new_file]->asampsize/8);
	}
      }
      else {
	if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
	  mainw->rec_aclip=mainw->current_file;
	  mainw->rec_avel=0.;
	  mainw->rec_aseek=0.;
	}
      }
    }
#endif
 }
}



void do_quick_switch (gint new_file) {
  // handle clip switching during playback

  gint ovsize=mainw->pheight;
  gint ohsize=mainw->pwidth;
  boolean osc_block;

  if (mainw->current_file<1||mainw->files[new_file]==NULL) return;

  if (mainw->noswitch||(mainw->record&&!mainw->record_paused&&!(prefs->rec_opts&REC_CLIPS))||
      mainw->foreign||(mainw->preview&&!mainw->is_rendering&&mainw->multitrack==NULL)) return;

  if (!mainw->sep_win&&mainw->multitrack==NULL) {
    gtk_widget_show(mainw->playframe);
  }

  if (new_file==mainw->current_file) {
    if (!((mainw->fs&&prefs->gui_monitor==prefs->play_monitor)||(mainw->faded&&mainw->double_size)||
	  mainw->multitrack!=NULL)) {
      switch_to_file (mainw->current_file=0, new_file);
      if (mainw->play_window!=NULL&&!mainw->double_size&&!mainw->fs&&mainw->current_file!=-1&&cfile!=NULL&&
	  (ohsize!=cfile->hsize||ovsize!=cfile->vsize)) {
	// for single size sepwin, we resize frames to fit the window
	mainw->must_resize=TRUE;
	mainw->pheight=ovsize;
	mainw->pwidth=ohsize;
      }
      else if (mainw->multitrack==NULL) mainw->must_resize=FALSE;
    }
    return;
  }

  // reset old info file
  if (cfile!=NULL) {
    gchar *tmp;
#ifndef IS_MINGW
      tmp=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
#else
      tmp=g_build_filename(prefs->tmpdir,cfile->handle,"status",NULL);
#endif
    g_snprintf(cfile->info_file,PATH_MAX,"%s",tmp);
    g_free(tmp);
  }

  osc_block=mainw->osc_block;
  mainw->osc_block=TRUE;
 
  // switch audio clip
  if ((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE)&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)&&!mainw->is_rendering&&
      (mainw->preview||!(mainw->agen_key!=0||prefs->audio_src==AUDIO_SRC_EXT))) {
    switch_audio_clip(new_file,TRUE);
  }

  mainw->whentostop=NEVER_STOP;

  if (cfile!=NULL&&cfile->clip_type==CLIP_TYPE_GENERATOR&&new_file!=mainw->current_file&&
      new_file!=mainw->blend_file&&!mainw->is_rendering) {
    if (mainw->files[new_file]->clip_type==CLIP_TYPE_DISK||mainw->files[new_file]->clip_type==CLIP_TYPE_FILE) 
      mainw->pre_src_file=new_file;

    if (rte_window!=NULL) rtew_set_keych(rte_fg_gen_key(),FALSE);
    if (mainw->current_file==mainw->blend_file) mainw->new_blend_file=new_file;
    weed_generator_end ((weed_plant_t *)cfile->ext_src);
    if (mainw->current_file==-1) {
      mainw->osc_block=osc_block;
      return;
    }
  }
  
  mainw->switch_during_pb=TRUE;
  mainw->clip_switched=TRUE;

  if (mainw->fs||(mainw->faded&&mainw->double_size)||mainw->multitrack!=NULL) {
    mainw->current_file=new_file;
    if (!mainw->sep_win) {
      if (cfile->menuentry!=NULL) {
	gchar title[256];
	get_menu_text(cfile->menuentry,title);
	set_main_title(title,0);
      }
      else set_main_title(cfile->file_name,0);
    }
  }
  else {
    // force update of labels, prevent widgets becoming sensitized
    switch_to_file (mainw->current_file, new_file);
  }

  mainw->play_start=1;
  mainw->play_end=cfile->frames;

  if (mainw->play_window!=NULL) {
    gchar *title=g_strdup(_("LiVES: - Play Window"));
    gtk_window_set_title (GTK_WINDOW (mainw->play_window), title);
    g_free(title);
    if (mainw->double_size&&!mainw->fs&&(ohsize!=cfile->hsize||ovsize!=cfile->vsize)) {
      // for single size sepwin, we resize frames to fit the window
      mainw->must_resize=TRUE;
      mainw->pheight=ovsize;
      mainw->pwidth=ohsize;
    }
  }
  else if (mainw->multitrack==NULL) mainw->must_resize=FALSE;

  if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->event_list!=NULL&&!mainw->record)) 
    mainw->play_end=INT_MAX;

  // act like we are not playing a selection (but we will try to keep to 
  // selection bounds)
  mainw->playing_sel=FALSE;
  
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
  changed_fps_during_pb (GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  if (!cfile->frameno&&cfile->frames) cfile->frameno=1;
  cfile->last_frameno=cfile->frameno;

  mainw->playing_file=new_file;

  cfile->next_event=NULL;
  mainw->deltaticks=0;
  mainw->startticks=mainw->currticks;
  // force loading of a frame from the new clip
  if (!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
    weed_plant_t *frame_layer=mainw->frame_layer;
    mainw->frame_layer=NULL;
    load_frame_image (cfile->frameno);
    mainw->frame_layer=frame_layer;
  }

  mainw->switch_during_pb=FALSE;
  mainw->osc_block=osc_block;
}





void resize (gdouble scale) {
  // resize the frame widgets
  // set scale<0. to _force_ the playback frame to expand (for external capture)
  gdouble oscale=scale;
  gint xsize;
  gint bx,by;
  GdkPixbuf *sepbuf;
  gint hspace=((sepbuf=gtk_image_get_pixbuf (GTK_IMAGE (mainw->sep_image)))!=NULL)?lives_pixbuf_get_height (sepbuf):0;
  // maximum values
  gint hsize,vsize;
  gint scr_width,scr_height;

  if (!prefs->show_gui||mainw->multitrack!=NULL) return;
  get_border_size (mainw->LiVES,&bx,&by);

  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
    scr_height=mainw->scr_height;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  hsize=(scr_width-(V_RESIZE_ADJUST*2+bx))/3;
  vsize=(scr_height-(CE_FRAME_HSPACE+hspace+by));

  if (scale<0.) {
    // foreign capture
    scale=-scale;
    hsize=(scr_width-H_RESIZE_ADJUST-bx)/scale;
    vsize=(scr_height-V_RESIZE_ADJUST-by)/scale;
  }

  if (mainw->current_file==-1||cfile->hsize==0) {
    hsize=mainw->def_width-H_RESIZE_ADJUST;
  }
  else {
    if (cfile->hsize<hsize) {
      hsize=cfile->hsize;
    }
  }

  if (mainw->current_file==-1||cfile->vsize==0) {
    vsize=mainw->def_height-V_RESIZE_ADJUST;
  }
  else {
    if (cfile->hsize>0&&(cfile->vsize*hsize/cfile->hsize<vsize)) {
      vsize=cfile->vsize*hsize/cfile->hsize;
    }
  }

  mainw->ce_frame_width=hsize;
  mainw->ce_frame_height=vsize;

  if (!mainw->is_ready) return;



  gtk_widget_set_size_request (mainw->playframe, (gint)hsize*scale+H_RESIZE_ADJUST, (gint)vsize*scale+V_RESIZE_ADJUST);

  if (oscale==2.) {
    if (hsize*4<scr_width-70) {
      scale=1.;
    }
  }

  if (oscale>0.) {
    gtk_widget_set_size_request (mainw->frame1, (gint)hsize/scale+H_RESIZE_ADJUST, vsize/scale+V_RESIZE_ADJUST);
    gtk_widget_set_size_request (mainw->eventbox3, (gint)hsize/scale+H_RESIZE_ADJUST, vsize+V_RESIZE_ADJUST);
    gtk_widget_set_size_request (mainw->frame2, (gint)hsize/scale+H_RESIZE_ADJUST, vsize/scale+V_RESIZE_ADJUST);
    gtk_widget_set_size_request (mainw->eventbox4, (gint)hsize/scale+H_RESIZE_ADJUST, vsize+V_RESIZE_ADJUST);
    mainw->ce_frame_width=(gint)hsize/scale+H_RESIZE_ADJUST;
    mainw->ce_frame_height=vsize/scale+V_RESIZE_ADJUST;
  }

  else {
    xsize=(scr_width-hsize*-oscale-H_RESIZE_ADJUST)/2;
    if (xsize>0) {
      gtk_widget_set_size_request (mainw->frame1, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->eventbox3, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->frame2, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->eventbox4, xsize/scale, vsize+V_RESIZE_ADJUST);
      mainw->ce_frame_width=xsize/scale;
      mainw->ce_frame_height=vsize+V_RESIZE_ADJUST;
    }
    else {
      // this is for foreign capture
      gtk_widget_hide(mainw->frame1);
      gtk_widget_hide(mainw->frame2);
      gtk_widget_hide(mainw->eventbox3);
      gtk_widget_hide(mainw->eventbox4);
      gtk_container_set_border_width (GTK_CONTAINER (mainw->playframe), 0);
    }
  }

  if (!mainw->foreign&&mainw->playing_file==-1&&mainw->current_file>0&&(!cfile->opening||cfile->clip_type==CLIP_TYPE_FILE)) {
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      load_start_image(cfile->start);
      load_end_image(cfile->end);
  }
  
}






