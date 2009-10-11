// main.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2009

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 or higher as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"

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

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#include <getopt.h>

////////////////////////////////

static gboolean no_recover=FALSE,auto_recover=FALSE;
static gboolean upgrade_error=FALSE;
static gchar start_file[256];
static gdouble start=0.;
static gint end=0;

static gboolean theme_expected;

static  _ign_opts ign_opts;

static int zargc;
static char **zargv;


/////////////////////////////////


void catch_sigint(int signum) {
  // trap for ctrl-C and others 
  if (!(mainw==NULL)) {
    if (!(mainw->LiVES==NULL)) {
      if (mainw->foreign) {
	exit (signum);
      }
      if (signum==0||signum==SIGINT||signum==SIGSEGV) {
	if (signum==SIGSEGV) {
	  signal (SIGSEGV, SIG_DFL);
	  g_printerr("%s",_("\nUnfortunately LiVES crashed.\nPlease report this bug at http://www.sourceforge.net/projects/lives/\nThanks.\nRecovery should be possible if you restart LiVES.\n"));
	  g_printerr("%s",_("\n\nWhen reporting crashes, please include details of your operating system, distribution, the LiVES version (" LiVES_VERSION ")\n"));
	  g_printerr("%s",_("and if possible obtain a backtrace using gdb.\n\n\n\n"));
	  mainw->leave_recovery=TRUE;
	}
	//#define DEBUG_CRASHES
#ifdef DEBUG_CRASHES
	g_on_error_query(NULL);
#endif

	if (mainw->was_set) {
	  g_printerr ("%s",_("Preserving set.\n"));
	  mainw->leave_files=TRUE;
	}
      }
      mainw->only_close=FALSE;
      lives_exit();
    }
  }
}



void get_monitors(void) {
  GSList *dlist,*dislist;
  GdkDisplay *disp;
  GdkScreen *screen;
  gint nscreens,nmonitors;
  int i,j,idx=0;

  if (mainw->mgeom!=NULL) g_free(mainw->mgeom);
  mainw->mgeom=NULL;

  dlist=dislist=gdk_display_manager_list_displays(gdk_display_manager_get());

  mainw->nmonitors=0;

  // for each display get list of screens

  while (dlist!=NULL) {
    disp=(GdkDisplay *)dlist->data;

    // get screens
    nscreens=gdk_display_get_n_screens(disp);
    for (i=0;i<nscreens;i++) {
      screen=gdk_display_get_screen(disp,i);
      mainw->nmonitors+=gdk_screen_get_n_monitors(screen);
    }
    dlist=dlist->next;
  }

  if (prefs->force_single_monitor) mainw->nmonitors=1; // force for clone mode

  if (mainw->nmonitors>1) {
    mainw->mgeom=(lives_mgeometry_t *)g_malloc(mainw->nmonitors*sizeof(lives_mgeometry_t));
    dlist=dislist;

    while (dlist!=NULL) {
      disp=(GdkDisplay *)dlist->data;

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
	  mainw->mgeom[idx].screen=screen;
	  idx++;
	}
      }
      dlist=dlist->next;
    }
  }

  g_slist_free(dislist);
}






static gboolean pre_init(void) {
  // stuff which should be done *before* mainwindow is created
  // returns TRUE if we expect to load a theme
  pthread_mutexattr_t mattr;

  gchar buff[256];
  int i;
  gboolean needs_update=FALSE;
  gchar *rcfile;

  sizint=sizeof(gint);
  sizdbl=sizeof(gdouble);
  sizshrt=sizeof(gshort);

  mainw=(mainwindow*)(g_malloc(sizeof(mainwindow)));
  mainw->is_ready=FALSE;

  mainw->free_fn=free;
  mainw->do_not_free=NULL;
  mainw->alt_vtable.malloc=malloc;
  mainw->alt_vtable.realloc=realloc;
  mainw->alt_vtable.free=lives_free;
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

  prefs=(_prefs *)g_malloc(sizeof(_prefs));
  future_prefs=(_future_prefs *)g_malloc(sizeof(_future_prefs));

  prefs->sleep_time=1000;
  mainw->cached_list=NULL;

  mainw->splash_window=NULL;

  mainw->threaded_dialog=FALSE;

  // check the backend is there, get some system details and prefs
  capable=get_capabilities();

  palette=(_palette*)(g_malloc(sizeof(_palette)));

  prefs->show_gui=TRUE;
  prefs->sepwin_type=1;
  prefs->show_framecount=TRUE;
  prefs->audio_player=AUD_PLAYER_SOX;
  prefs->open_decorated=TRUE;

  if (!capable->smog_version_correct||!capable->can_write_to_tempdir) {
    g_snprintf(prefs->theme,64,"none");
    return FALSE;
  }

  // from here onwards we can use get_pref() and friends  //////
  rcfile=g_strdup_printf("%s/.lives",capable->home_dir);
  cache_file_contents(rcfile);
  g_free(rcfile);

  get_pref("gui_theme",prefs->theme,64);
  if (!strlen(prefs->theme)) {
    g_snprintf(prefs->theme,64,"none");
  }
  // get some prefs we need to set menu options
  future_prefs->show_recent=prefs->show_recent=get_boolean_pref ("show_recent_files");
  
  get_pref ("prefix_dir",prefs->prefix_dir,256);
  
  if (!strlen (prefs->prefix_dir)) {
    if (strcmp (PREFIX,"NONE")) {
      g_snprintf (prefs->prefix_dir,256,"%s",PREFIX);
    }
    else {
      g_snprintf (prefs->prefix_dir,256,"%s",PREFIX_DEFAULT);
    }
    needs_update=TRUE;
  }
  
  if (ensure_isdir(prefs->prefix_dir)) needs_update=TRUE;
  if (needs_update) set_pref("prefix_dir",prefs->prefix_dir);
  
  needs_update=FALSE;
  
  get_pref ("lib_dir",prefs->lib_dir,256);
  
  if (!strlen (prefs->lib_dir)) {
    g_snprintf (prefs->lib_dir,256,"%s",LIVES_LIBDIR);
    needs_update=TRUE;
  }
  
  if (ensure_isdir(prefs->lib_dir)) needs_update=TRUE;
  if (needs_update) set_pref("lib_dir",prefs->lib_dir);
  
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
  
#ifdef HAVE_PULSE_AUDIO
  if (prefs->startup_phase==1&&capable->has_pulse_audio) {
    prefs->audio_player=AUD_PLAYER_PULSE;
    set_pref("audio_player","pulse");
  }
  else {
#endif
  


#ifdef ENABLE_JACK
    if (prefs->startup_phase==1&&capable->has_jackd) {
      prefs->audio_player=AUD_PLAYER_JACK;
      set_pref("audio_player","jack");
    }
#endif

#ifdef HAVE_PULSE_AUDIO
  }
#endif

  prefs->gui_monitor=0;
  prefs->play_monitor=0;

  mainw->mgeom=NULL;
  prefs->virt_height=1;

  prefs->force_single_monitor=get_boolean_pref("force_single_monitor");

  get_monitors();

  if (mainw->nmonitors>1) {

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
    if (prefs->gui_monitor>mainw->nmonitors) prefs->gui_monitor=mainw->nmonitors;
    if (prefs->play_monitor>mainw->nmonitors) prefs->play_monitor=mainw->nmonitors;
  }


  for (i=0;i<MAX_FX_CANDIDATE_TYPES;i++) {
    mainw->fx_candidates[i].delegate=-1;
    mainw->fx_candidates[i].list=NULL;
    mainw->fx_candidates[i].func=0l;
    mainw->fx_candidates[i].rfx=NULL;
  }

  mainw->cursor=NULL;

  for (i=0;i<MAX_EXT_CNTL;i++) mainw->ext_cntl[i]=FALSE;

  prefs->omc_dev_opts=get_int_pref("omc_dev_opts");

  get_pref("omc_js_fname",prefs->omc_js_fname,256);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  if (strlen(prefs->omc_js_fname)==0) {
    gchar *tmp=get_js_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_js_fname,256,"%s",tmp);
      }
  }
#endif
#endif
  
  get_pref("omc_midi_fname",prefs->omc_midi_fname,256);
#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  if (strlen(prefs->omc_midi_fname)==0) {
    gchar *tmp=get_midi_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_midi_fname,256,"%s",tmp);
    }
  }
#endif
#endif

#ifdef ALSA_MIDI
    prefs->use_alsa_midi=TRUE;
    mainw->seq_handle=NULL;

    if (prefs->omc_dev_opts&OMC_DEV_FORCE_RAW_MIDI) prefs->use_alsa_midi=FALSE;

#endif

  mainw->volume=1.f;

  if (!strcasecmp(prefs->theme,"none")) return FALSE;
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
    
    resize_fx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_RESIZER].list,mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate));
    filter=get_weed_filter(resize_fx);
    rfx=weed_to_rfx(filter,TRUE);
    
    rfx->is_template=FALSE;
    rfx->props|=RFX_PROPS_MAY_RESIZE;

    g_free(rfx->action_desc);
    rfx->action_desc=g_strdup(_("Resizing"));

    rfx->min_frames=1;

    g_free(rfx->menu_text);

    if (mainw->resize_menuitem==NULL) {
      mainw->resize_menuitem = gtk_menu_item_new_with_mnemonic(_("_Resize All Frames"));
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
  gchar buff[256];
  GList *encoders=NULL;
  GList *encoder_capabilities=NULL;

  signal (SIGHUP,SIG_IGN);
  signal (SIGPIPE,SIG_IGN);
  signal (SIGINT, catch_sigint);
  signal (SIGTERM, catch_sigint);
  signal (SIGSEGV, catch_sigint);

  // initialise the mainwindow data
  mainw->scr_width=gdk_screen_width();
  mainw->scr_height=gdk_screen_height();

  for (i=0;i<=MAX_FILES;mainw->files[i++]=NULL);
  mainw->fs=FALSE;
  mainw->loop=mainw->loop_cont=FALSE;
  mainw->prefs_changed=FALSE;
  mainw->last_dprint_file=mainw->current_file=mainw->playing_file=-1;
  mainw->first_free_file=1;
  mainw->insert_after=TRUE;
  mainw->mute=FALSE;
  mainw->faded=FALSE;
  mainw->save_with_sound=TRUE;
  mainw->preview=FALSE;
  mainw->selwidth_locked=FALSE;
  mainw->xwin=0;
  mainw->untitled_number=mainw->cap_number=1;
  mainw->sel_start=0;
  mainw->sel_move=SEL_MOVE_AUTO;
  mainw->record_foreign=FALSE;
  mainw->ccpd_with_sound=FALSE;
  mainw->play_window=NULL;
  mainw->opwx=mainw->opwy=-1;
  mainw->save_all=TRUE;
  mainw->frame_layer=NULL;
  mainw->in_fs_preview=FALSE;
  mainw->effects_paused=FALSE;
  mainw->play_start=0;
  mainw->opening_loc=FALSE;
  mainw->toy_type=TOY_NONE;
  mainw->framedraw=mainw->framedraw_spinbutton=NULL;
  mainw->framedraw_copy_pixmap=mainw->framedraw_orig_pixmap=mainw->framedraw_bitmap=NULL;
  mainw->framedraw_bitmapgc=NULL;
  mainw->framedraw_colourgc=NULL;
  mainw->is_processing=FALSE;
  mainw->is_rendering=FALSE;
  mainw->is_generating=FALSE;
  mainw->resizing=FALSE;
  mainw->switch_during_pb=FALSE;
  mainw->playing_sel=FALSE;
  if (G_BYTE_ORDER==G_LITTLE_ENDIAN) {
    mainw->endian=0;
  }
  else {
    mainw->endian=AFORM_BIG_ENDIAN;
  }

  mainw->leave_files=FALSE;
  mainw->was_set=FALSE;
  mainw->toy_go_wild=FALSE;

  for (i=0;i<FN_KEYS-1;i++) {
    mainw->clipstore[i]=0;
  }

  mainw->loop=TRUE;
  mainw->ping_pong=FALSE;

  mainw->nervous=FALSE;
  fx_dialog[0]=fx_dialog[1]=NULL;

  mainw->rte_keys=-1;
  rte_window=NULL;

  mainw->rte=EFFECT_NONE;

  mainw->must_resize=FALSE;
  mainw->gc=NULL;

  mainw->preview_box = NULL;
  mainw->prv_link=PRV_FREE;

  mainw->internal_messaging=FALSE;
  mainw->progress_fn=NULL;
  mainw->origticks=1;

  mainw->last_grabable_effect=-1;
  mainw->blend_file=-1;

  mainw->pre_src_file=-2;

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

  // install our key_snooper, this will do for ctrl-arrow key autorepeat
  mainw->ksnoop=gtk_key_snooper_install (key_snooper, NULL);

  prefs->omc_noisy=FALSE;
  if (!ign_opts->ign_osc) {
    prefs->osc_udp_started=FALSE;
    prefs->osc_udp_port=0;
#ifdef ENABLE_OSC
    prefs->osc_udp_port=get_int_pref ("osc_port");
    future_prefs->osc_start=prefs->osc_start=get_boolean_pref("osc_start");
#endif
  }

  prefs->ignore_tiny_fps_diffs=1;
  prefs->rec_opts=get_int_pref("record_opts");

  if (prefs->rec_opts==-1) {
    prefs->rec_opts=REC_FPS|REC_FRAMES|REC_EFFECTS|REC_CLIPS|REC_AUDIO;
    set_int_pref("record_opts",prefs->rec_opts);
  }

  prefs->rec_opts|=(REC_FPS+REC_FRAMES);


  mainw->new_clip=-1;
  mainw->record=FALSE;
  mainw->event_list=NULL;
  mainw->clip_switched=FALSE;
  mainw->scrap_file=-1;

  mainw->multitrack=NULL;

  mainw->jack_can_stop=FALSE;
  mainw->jack_can_start=TRUE;

  mainw->video_seek_ready=FALSE;

  mainw->filter_map=NULL; // filter map for rendering

  mainw->did_rfx_preview=FALSE;
  mainw->invis=NULL;

  prefsw=NULL;
  rdet=NULL;
  resaudw=NULL;

  mainw->actual_frame=0;

  mainw->scratch=SCRATCH_NONE;

  mainw->clip_index=mainw->frame_index=NULL;

  mainw->affected_layouts_map=mainw->current_layouts_map=NULL;

  mainw->recovery_file=g_strdup_printf("%s/recovery.%d.%d.%d",prefs->tmpdir,getuid(),getgid(),getpid());
  mainw->leave_recovery=FALSE;

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

  mainw->any_string=g_strdup(_("Any"));  // note to translators - text saying "Any", for encoder and output format
  mainw->none_string=g_strdup(_("None"));  // note to translators - text saying "None", for playback plugin name
  mainw->recommended_string=g_strdup(_("recommended"));  // note to translators - text saying "recommended", for playback plugin name
  mainw->disabled_string=g_strdup(_("disabled !"));  // note to translators - text saying "disabled", for playback plugin name

  mainw->opening_frames=-1;

  mainw->general_gc=NULL;

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

  hidden_cursor=NULL;

  mainw->keep_pre=FALSE;

  mainw->reverse_pb=FALSE;

  mainw->osc_auto=FALSE;
  mainw->osc_enc_width=mainw->osc_enc_height=0;


  mainw->no_switch_dprint=FALSE;

  mainw->rte_textparm=NULL;


  mainw->abufs_to_fill=0;

  mainw->recoverable_layout=FALSE;

  mainw->soft_debug=FALSE;
  /////////////////////////////////////////////////// add new stuff just above here ^^

  g_snprintf(mainw->first_info_file,255,"%s/.info.%d",prefs->tmpdir,getpid());
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

    colour_equal(&palette->fade_colour,&palette->black);

    // we set the theme here in case it got reset to 'none'
    set_pref("gui_theme",prefs->theme);
    g_snprintf(future_prefs->theme,64,"%s",prefs->theme);

    prefs->stop_screensaver=get_boolean_pref("stop_screensaver");
    prefs->pause_xmms=get_boolean_pref("pause_xmms_on_playback");
    prefs->open_maximised=get_boolean_pref("open_maximised");
    future_prefs->show_tool=prefs->show_tool=get_boolean_pref("show_toolbar");
    memset (future_prefs->vpp_name,0,64);
    future_prefs->vpp_argv=NULL;

    if (prefs->gui_monitor!=0) {
      gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-mainw->LiVES->allocation.width)/2;
      gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-mainw->LiVES->allocation.height)/2;
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

    prefs->osc_inv_latency=10;

    prefs->midi_check_rate=get_int_pref("midi_check_rate");
    if (prefs->midi_check_rate==0) prefs->midi_check_rate=DEF_MIDI_CHECK_RATE;

    if (prefs->midi_check_rate<1) prefs->midi_check_rate=1;

    prefs->midi_rpt=get_int_pref("midi_rpt");
    if (prefs->midi_rpt==0) prefs->midi_rpt=DEF_MIDI_RPT;

    prefs->num_rtaudiobufs=4;

    prefs->safe_symlinks=FALSE; // set to TRUE for dynebolic and othe live CDs

    prefs->mouse_scroll_clips=get_boolean_pref("mouse_scroll_clips");

    prefs->mt_auto_back=get_int_pref("mt_auto_back");

    //////////////////////////////////////////////////////////////////

    weed_memory_init();

    if (!mainw->foreign) {

      gettimeofday(&tv,NULL);
      fastsrand(tv.tv_sec);

      srandom(tv.tv_sec);

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

      future_prefs->jack_opts=get_int_pref("jack_opts");
      if (!ign_opts->ign_jackopts) {
	prefs->jack_opts=future_prefs->jack_opts;
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

      if (!ign_opts->ign_clipset) {
	get_pref("ar_clipset",prefs->ar_clipset_name,128);
	if (strlen(prefs->ar_clipset_name)) prefs->ar_clipset=TRUE;
	else prefs->ar_clipset=FALSE;
      }

      get_pref("ar_layout",prefs->ar_layout_name,128);
      if (strlen(prefs->ar_layout_name)) prefs->ar_layout=TRUE;
      else prefs->ar_layout=FALSE;

      prefs->rec_desktop_audio=get_boolean_pref("rec_desktop_audio");

      // scan for encoder plugins
      if ((encoders=get_plugin_list (PLUGIN_ENCODERS,FALSE,NULL,NULL))!=NULL) {
	capable->has_encoder_plugins=TRUE;
	g_list_free_strings (encoders);
	g_list_free (encoders);
      }
      
      if (prefs->startup_phase==1&&capable->has_encoder_plugins&&capable->has_python) {
	g_snprintf(prefs->encoder.name,52,"%s","multi_encoder");
	set_pref("encoder",prefs->encoder.name);
      }
      else get_pref("encoder",prefs->encoder.name,51);

      prefs->debug_encoders=get_boolean_pref("debug_encoders");
      
      get_pref("output_type",prefs->encoder.of_name,51);
      
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
	  prefs->encoder.capabilities=atoi (g_list_nth_data (encoder_capabilities,0));
	  g_list_free_strings (encoder_capabilities);
	  g_list_free (encoder_capabilities);
	  if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
	    // get any restrictions for the current format
	    for (i=0;i<g_list_length(ofmt_all);i++) {
	      if ((numtok=get_token_count (g_list_nth_data (ofmt_all,i),'|'))>2) {
		array=g_strsplit (g_list_nth_data (ofmt_all,i),"|",-1);
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

      get_pref("vid_load_dir",prefs->def_vid_load_dir,256);
      g_snprintf(mainw->vid_load_dir,256,"%s",prefs->def_vid_load_dir);
      
      get_pref("vid_save_dir",prefs->def_vid_save_dir,256);
      g_snprintf(mainw->vid_save_dir,256,"%s",prefs->def_vid_save_dir);
      
      get_pref("audio_dir",prefs->def_audio_dir,256);
      g_snprintf(mainw->audio_dir,256,"%s",prefs->def_audio_dir);
      g_snprintf(mainw->xmms_dir,256,"%s",mainw->audio_dir);
      
      get_pref("image_dir",prefs->def_image_dir,256);
      g_snprintf(mainw->image_dir,256,"%s",prefs->def_image_dir);
      
      get_pref("proj_dir",prefs->def_proj_dir,256);
      g_snprintf(mainw->proj_load_dir,256,"%s",prefs->def_proj_dir);
      g_snprintf(mainw->proj_save_dir,256,"%s",prefs->def_proj_dir);
      
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
      
      splash_msg(_("Loading realtime effect plugins..."),.6);
      weed_load_all();

      // replace any multi choice effects with their delegates
      replace_with_delegates();

      load_default_keymap();

      prefs->audio_opts=get_int_pref("audio_opts");
#ifdef ENABLE_JACK
      g_snprintf(prefs->jack_aserver,256,"%s/.jackdrc",capable->home_dir);
      g_snprintf(prefs->jack_tserver,256,"%s/.jackdrc",capable->home_dir);

#endif

      if (((capable->has_jackd||capable->has_pulse_audio)&&(capable->has_sox||capable->has_mplayer))||(capable->has_jackd&&capable->has_pulse_audio)) {
	if (prefs->startup_phase>0&&prefs->startup_phase<3) {
	  splash_end();
	  if (!do_audio_choice_dialog(prefs->startup_phase)) {
	    lives_exit();
	  }
	  if (prefs->audio_player==AUD_PLAYER_JACK) prefs->jack_opts=JACK_OPTS_START_ASERVER;
	  prefs->jack_opts=0;
	  set_int_pref("jack_opts",prefs->jack_opts);
	}

	if (prefs->startup_phase==1) {
	  prefs->startup_phase=2;
	  set_int_pref("startup_phase",2);
	}

#ifdef ENABLE_JACK
	if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER||prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT||prefs->jack_opts&JACK_OPTS_START_ASERVER) {
	  // start jack transport polling
	  splash_msg(_("Starting jack audio server..."),.8);
	  lives_jack_init();
	}

	if (prefs->audio_player==AUD_PLAYER_JACK) {
	  jack_audio_init();
	  jack_audio_read_init();
	  mainw->jackd=jack_get_driver(0,TRUE);
	  if (mainw->jackd!=NULL) {
	    if (jack_open_device(mainw->jackd)) mainw->jackd=NULL;

	    if (((!(prefs->jack_opts&JACK_OPTS_START_TSERVER)&&!(prefs->jack_opts&JACK_OPTS_START_ASERVER))||mainw->jackd==NULL)&&prefs->startup_phase==0) {
	      g_printerr("%s",_("\n\nManual start of jackd required. Please make sure jackd is running, \nor else change the value of <jack_opts> in ~/.lives to 16\nand restart LiVES.\n\nAlternatively, try to start lives with: lives -aplayer sox\n\n"));
	    }

	    if (mainw->jackd==NULL) {
	      if (prefs->startup_phase==2) {
		do_jack_noopen_warn();
		do_jack_noopen_warn2();
	      }
	      lives_exit();
	    }

	    mainw->jackd->whentostop=&mainw->whentostop;
	    mainw->jackd->cancelled=&mainw->cancelled;
	    mainw->jackd->in_use=FALSE;
	    mainw->jackd->play_when_stopped=(prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE;
	  }
	}
      }
#endif


#ifdef HAVE_PULSE_AUDIO
	if (prefs->audio_player==AUD_PLAYER_PULSE) {
	  splash_msg(_("Starting pulse audio server..."),.8);

	  if (!lives_pulse_init(prefs->startup_phase)) {
	    if (prefs->startup_phase==2) {
	      lives_exit();
	    }
	  }

	  pulse_audio_init();
	  pulse_audio_read_init();
	  mainw->pulsed=pulse_get_driver(TRUE);

	  mainw->pulsed->whentostop=&mainw->whentostop;
	  mainw->pulsed->cancelled=&mainw->cancelled;
	  mainw->pulsed->in_use=FALSE;
	}
#endif

    }

    if (prefs->startup_phase!=0) {
      set_int_pref("startup_phase",100); // tell backend to delete this
      prefs->startup_phase=100;
    }

    // toolbar buttons
    gtk_widget_modify_bg (mainw->tb_hbox, GTK_STATE_NORMAL, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->toolbar, GTK_STATE_NORMAL, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_stopbutton, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_bckground, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_sepwin, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_double, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_fullscreen, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_faster, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_slower, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_forward, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_back, GTK_STATE_PRELIGHT, &palette->fade_colour);
    gtk_widget_modify_bg (mainw->t_infobutton, GTK_STATE_PRELIGHT, &palette->fade_colour);


  }
}



void do_start_messages(void) {
  d_print("\n");
  d_print(_("Checking optional dependencies:"));
  if (capable->has_mplayer) d_print(_ ("mplayer...detected..."));
  else d_print(_ ("mplayer...NOT DETECTED..."));
  if (capable->has_convert) d_print(_ ("convert...detected..."));
  else d_print(_ ("convert...NOT DETECTED..."));
  if (capable->has_composite) d_print(_ ("composite...detected..."));
  else d_print(_ ("composite...NOT DETECTED..."));
  if (capable->has_sox) d_print(_ ("sox...detected\n"));
  else d_print(_ ("sox...NOT DETECTED\n"));
  if (capable->has_xmms) d_print(_ ("xmms...detected..."));
  else d_print(_ ("xmms...NOT DETECTED..."));
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

  prefs->wm=gdk_x11_screen_get_window_manager_name (gdk_screen_get_default());
  g_snprintf(mainw->msg,512,_ ("\n\nWindow manager reports as \"%s\"; "),prefs->wm);
  d_print(mainw->msg);

  g_snprintf(mainw->msg,512,_("number of monitors detected: %d\n"),mainw->nmonitors);
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
  // bitmap transparencies
  GdkColor opaque={0,0,0,0};
  GdkColor trans={0xFF,0xFF,0xFF,0xFF};
  colour_equal (&palette->bm_opaque,&opaque);
  colour_equal (&palette->bm_trans,&trans);

  // set configurable colours and theme colours for the app
  gdk_color_parse ("black", &palette->black);
  gdk_color_parse ("white", &palette->white);
  gdk_color_parse ("light blue", &palette->light_blue);
  gdk_color_parse ("light yellow", &palette->light_yellow);
  gdk_color_parse ("grey10", &palette->grey20);
  gdk_color_parse ("grey25", &palette->grey25);
  gdk_color_parse ("grey45", &palette->grey45);
  gdk_color_parse ("grey60", &palette->grey60);
  gdk_color_parse ("pink", &palette->pink);
  gdk_color_parse ("salmon", &palette->light_red);
  gdk_color_parse ("DarkOrange4", &palette->dark_orange);

  colour_equal(&palette->banner_fade_text,&palette->white);
  palette->style=STYLE_PLAIN;

  // STYLE_PLAIN will overwrite this
  if (!(strcmp(prefs->theme,"pinks"))) {
    
    palette->normal_back.red=228*256;
    palette->normal_back.green=196*256;
    palette->normal_back.blue=196*256;
      
    colour_equal(&palette->normal_fore,&palette->black);
    colour_equal(&palette->menu_and_bars,&palette->pink);
    colour_equal(&palette->info_text,&palette->normal_fore);
    colour_equal(&palette->info_base,&palette->normal_back);
    palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_6;
  }
  else {
    if (!(strcmp(prefs->theme,"cutting_room"))) {

      palette->normal_back.red=224*256;
      palette->normal_back.green=224*256;
      palette->normal_back.blue=128*256;
	  
      colour_equal(&palette->normal_fore,&palette->black);
      colour_equal(&palette->menu_and_bars,&palette->white);
      colour_equal(&palette->info_text,&palette->normal_fore);
      colour_equal(&palette->info_base,&palette->white);
      palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
    }
    else {
      if (!(strcmp(prefs->theme,"camera"))) {

	palette->normal_back.red=30*256;
	palette->normal_back.green=144*256;
	palette->normal_back.blue=232*256;
	  
	colour_equal(&palette->normal_fore,&palette->black);
	colour_equal(&palette->menu_and_bars,&palette->white);
	colour_equal(&palette->info_base,&palette->normal_back);
	colour_equal(&palette->info_text,&palette->normal_fore);
	palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
      }
      else {
	if (!(strcmp(prefs->theme,"editor"))) {
	  colour_equal(&palette->normal_back,&palette->grey25);
	  colour_equal(&palette->normal_fore,&palette->white);
	  colour_equal(&palette->menu_and_bars,&palette->grey60);
	  colour_equal(&palette->info_base,&palette->grey20);
	  colour_equal(&palette->info_text,&palette->white);
	  palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_5;
	}
	else {
	  if (!(strcmp(prefs->theme,"crayons-bright"))) {
	    colour_equal(&palette->normal_back,&palette->black);
	    colour_equal(&palette->normal_fore,&palette->white);

	    palette->menu_and_bars.red=225*256;
	    palette->menu_and_bars.green=160*256;
	    palette->menu_and_bars.blue=80*256;

	    palette->info_base.red=200*256;
	    palette->info_base.green=190*256;
	    palette->info_base.blue=52*256;
	      
	    colour_equal(&palette->info_text,&palette->black);

	    palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4;
	  }
	  else {
	    if (!(strcmp(prefs->theme,"crayons"))) {
	      colour_equal(&palette->normal_back,&palette->grey25);
	      colour_equal(&palette->normal_fore,&palette->white);
	      colour_equal(&palette->menu_and_bars,&palette->grey60);
	      colour_equal(&palette->info_base,&palette->grey20);
	      colour_equal(&palette->info_text,&palette->white);

	      palette->style=STYLE_1|STYLE_2|STYLE_3|STYLE_4|STYLE_5;

	    }
	    else {
	      palette->style=STYLE_PLAIN;
	    }}}}}}
}



capability *get_capabilities (void) {
  // get capabilities of backend system
  gchar *safer_bfile;
  gchar **array;
  
  gchar buffer[8192];
  FILE *bootfile;
  gchar string[256];
  int err;
  gint numtok;
  gchar *tmp;

  capable=(capability *)g_malloc(sizeof(capability));

  capable->cpu_bits=32;
  if (sizeof(void *)==8) capable->cpu_bits=64;

  capable->has_smogrify=FALSE;
  capable->smog_version_correct=FALSE;

  // required
  capable->can_write_to_tmp=FALSE;
  capable->can_write_to_tempdir=FALSE;
  capable->can_write_to_config=FALSE;
  capable->can_read_from_config=FALSE;

  g_snprintf(capable->home_dir,256,"%s",g_get_home_dir());

  memset(capable->startup_msg,0,1);

  // optional
  capable->has_mplayer=FALSE;
  capable->has_convert=FALSE;
  capable->has_composite=FALSE;
  capable->has_sox=FALSE;
  capable->has_xmms=FALSE;
  capable->has_dvgrab=FALSE;
  capable->has_cdda2wav=FALSE;
  capable->has_jackd=FALSE;
  capable->has_pulse_audio=FALSE;
  capable->has_xwininfo=FALSE;
  capable->has_midistartstop=FALSE;
  capable->has_encoder_plugins=FALSE;
  capable->has_python=FALSE;

  safer_bfile=g_strdup_printf("%s.%d.%d",BOOTSTRAP_NAME,getuid(),getgid());
  unlink (safer_bfile);

  // check that we can write to /tmp
  if (!check_file (safer_bfile,FALSE)) return capable;
  capable->can_write_to_tmp=TRUE;

  if ((tmp=g_find_program_in_path ("smogrify"))==NULL) return capable;
  g_free(tmp);
  
  g_snprintf(string,256,"smogrify report \"%s\" 2>/dev/null",(tmp=g_filename_from_utf8 (safer_bfile,-1,NULL,NULL,NULL)));
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

  dummychar=fgets(buffer,8192,bootfile);
  fclose(bootfile);

  unlink (safer_bfile);
  g_free(safer_bfile);


  // get backend version, tempdir, and any startup message
  numtok=get_token_count (buffer,'|');
  array=g_strsplit(buffer,"|",numtok);

  g_snprintf(string,256,"%s",array[0]);

  if (strcmp(string,LiVES_VERSION)) {
    g_strfreev(array);
    return capable;
  }

  capable->smog_version_correct=TRUE;

  g_snprintf(prefs->tmpdir,256,"%s",array[1]);
  g_snprintf(future_prefs->tmpdir,256,"%s",prefs->tmpdir);

  prefs->startup_phase=atoi (array[2]);

  if (numtok>3&&strlen (array[3])) {
    g_snprintf(capable->startup_msg,256,"%s\n\n",array[3]);
      if (numtok>4&&strlen (array[4])) {
	g_strappend (capable->startup_msg,256,array[4]);
      }
  }
  g_strfreev(array);

  if (!capable->can_write_to_tempdir) return capable;

  get_location("mplayer",string,256);
  if (strlen(string)) capable->has_mplayer=TRUE;

  get_location("convert",string,256);
  if (strlen(string)) capable->has_convert=TRUE;

  get_location("composite",string,256);
  if (strlen(string)) capable->has_composite=TRUE;

  ///////////////////////////////////////////////////////

  get_location("play",string,256);
  if (strlen(string)) capable->has_sox=TRUE;

  get_location("xmms",string,256);
  if (strlen(string)) capable->has_xmms=TRUE;

  get_location("dvgrab",string,256);
  if (strlen(string)) capable->has_dvgrab=TRUE;

  get_location("cdda2wav",string,256);
  if (strlen(string)) capable->has_cdda2wav=TRUE;

  get_location("jackd",string,256);
  if (strlen(string)) capable->has_jackd=TRUE;

  get_location("pulseaudio",string,256);
  if (strlen(string)) capable->has_pulse_audio=TRUE;

  get_location("python",string,256);
  if (strlen(string)) capable->has_python=TRUE;

  get_location("xwininfo",string,256);
  if (strlen(string)) capable->has_xwininfo=TRUE;

  get_location("midistart",string,256);
  if (strlen(string)) {
    get_location("midistop",string,256);
    if (strlen(string)) {
      capable->has_midistartstop=TRUE;
    }
  }

  return capable;
}

void print_notice() {
  g_printerr("\nLiVES %s\n",LiVES_VERSION);
  g_printerr("Copyright 2002-2009 Gabriel Finch (salsaman@xs4all.nl) and others.\n");
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
  g_printerr("%s",_("-set <setname>   : autoload clip set setname\n"));
  g_printerr("%s",_("-noset           : do not load any set on startup\n"));
  g_printerr("%s",_("-norecover       : force no-loading of crash recovery\n"));
  g_printerr("%s",_("-recover         : force loading of crash recovery\n"));
  g_printerr("%s",_("-nogui           : do not show the gui\n"));
#ifdef ENABLE_OSC
  g_printerr("%s",_("-oscstart <port> : start OSC listener on UDP port <port>\n"));
  g_printerr("%s",_("-nooscstart      : do not start OSC listener\n"));
#endif
  g_printerr("%s",_("-aplayer <ap>    : start with selected audio player. <ap> can be mplayer"));
#ifdef HAVE_PULSE_AUDIO
  g_printerr("%s",_(", pulse"));  // translators - pulse audio
#endif
#ifdef ENABLE_JACK
  g_printerr("%s",_(", sox or jack\n"));
  g_printerr("%s",_("-jackopts <opts>    : opts is a bitmap of jack startup options [1 = jack transport client, 2 = jack transport master, 4 = start jack transport server, 8 = pause audio when video paused, 16 = start jack audio server] \n"));
#else
  g_printerr("%s",_(" or sox\n"));
#endif
  g_printerr("%s",_("-devicemap <mapname>          : autoload devicemap\n"));

  g_printerr("%s","\n");
}




static gboolean lives_startup(gpointer data) {
  gboolean got_files=FALSE;

  if (!mainw->foreign) {
    splash_init();
    print_notice();
  }

  splash_msg(_("Starting GUI..."),0.);

  create_LiVES ();

  gtk_widget_queue_draw(mainw->LiVES);
  while (g_main_context_iteration(NULL,FALSE));

  if (capable->smog_version_correct) {
    if (theme_expected&&palette->style==STYLE_PLAIN&&!mainw->foreign) {
      // non-fatal errors
      if (prefs->startup_phase==0) {
	gchar *err=g_strdup_printf(_ ("\n\nThe theme you requested could not be located. Please make sure you have the themes installed in\n%s/%s.\n(Maybe you need to change the value of <prefix_dir> in your ~/.lives file)\n"),prefs->prefix_dir,THEME_DIR);
	startup_message_nonfatal (g_strdup (err));
	g_free(err);
	g_snprintf(prefs->theme,64,"none");
      }
      else {
	upgrade_error=TRUE;
      }
    }
    lives_init(&ign_opts);
  }

  if (!mainw->foreign) {
    // fatal errors
    if (!capable->can_write_to_tmp) {
      startup_message_fatal(_ ("\nLiVES was unable to write a small file to /tmp\nPlease make sure you have write access to /tmp and try again.\n"));
    }
    else {
      if (!capable->has_smogrify) {
	gchar *err=g_strdup (_ ("\n`smogrify` must be in your path, and be executable\n\nPlease review the README file which came with this package\nbefore running LiVES.\n"));
	startup_message_fatal(err);
	g_free(err);
      }
      else {
	if (!capable->can_read_from_config) {
	  gchar *err=g_strdup_printf(_ ("\nLiVES was unable to read from its configuration file\n%s/.lives\n\nPlease check the file permissions for this file and try again.\n"),capable->home_dir);
	  startup_message_fatal(err);
	  g_free(err);
	}
	else {
	  if (!capable->can_write_to_config) {
	    gchar *err=g_strdup_printf(_ ("\nLiVES was unable to write to its configuration file\n%s/.lives\n\nPlease check the file permissions for this file and directory\nand try again.\n"),capable->home_dir);
	    startup_message_fatal(err);
	    g_free(err);
	  }
	  else {
	    if (!capable->can_write_to_tempdir) {
	      gchar *err=g_strdup_printf(_ ("\nLiVES was unable to use the temporary directory\n%s\n\nPlease check the <tempdir> setting in \n%s/.lives\nand try again.\n"),prefs->tmpdir,capable->home_dir);
	      startup_message_fatal(err);
	      g_free(err);
	    }
	    else {
	      if (!capable->smog_version_correct) {
		startup_message_fatal(_ ("\nAn incorrect version of smogrify was found in your path.\n\nPlease review the README file which came with this package\nbefore running LiVES.\n\nThankyou.\n"));
	      }
	      else {
		if (!capable->has_sox&&!capable->has_mplayer){
		  startup_message_fatal(_ ("\nLiVES currently requires either 'mplayer' or 'sox' to function. Please install one or other of these, and try again.\n"));
		}
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
		      startup_message_nonfatal (_ ("\nLiVES was unable to locate 'convert'. You should install convert and image-magick if you want to use rendered effects.\n"));
		    }
		    if (!capable->has_composite) {
		      startup_message_nonfatal (_ ("\nLiVES was unable to locate 'composite'. You should install composite and image-magick if you want to use the merge function.\n"));
		    }
		    if (!capable->has_sox) {
		      startup_message_nonfatal (_ ("\nLiVES was unable to locate 'sox'. Some audio features may not work. You should install 'sox'.\n"));
		    }
		    if (!capable->has_encoder_plugins) {
		      if (prefs->startup_phase==0) {
			gchar *err=g_strdup_printf(_ ("\nLiVES was unable to find any encoder plugins.\nPlease check that you have them installed correctly in\n%s%s%s/\nYou will not be able to 'Save' without them.\nYou may need to change the value of <lib_dir> in ~/.lives\n"),prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS);
			startup_message_nonfatal_dismissable (err,WARN_MASK_NO_ENCODERS);
			g_free(err);
		      }
		      else {
			upgrade_error=TRUE;
		      }
		    }
		  }
		    
		  if (prefs->show_gui) gtk_widget_show (mainw->LiVES);
		}}}}}}}}
  
  else {
  // capture mode
    mainw->foreign_key=atoi(zargv[2]);
    mainw->foreign_id=atoi(zargv[3]);
    mainw->foreign_width=atoi(zargv[4]);
    mainw->foreign_height=atoi(zargv[5]);
    mainw->foreign_bpp=atoi(zargv[6]);
    mainw->rec_vid_frames=atoi(zargv[7]);
    mainw->rec_fps=strtod(zargv[8],NULL);
    mainw->rec_arate=atoi(zargv[9]);
    mainw->rec_asamps=atoi(zargv[10]);
    mainw->rec_achans=atoi(zargv[11]);
    mainw->rec_signed_endian=atoi(zargv[12]);

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
    if (upgrade_error) {
      do_upgrade_error_dialog();
    }
    else {
      do_firstever_dialog();
    }
  }

  if (strlen (start_file)) {
    splash_end();
    deduce_file(start_file,start,end);
    got_files=TRUE;
  } else {
    set_main_title(NULL,0);
  }

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

  splash_end();

  if (mainw->recoverable_layout) do_layout_recover_dialog();

  return FALSE;
}



int main (int argc, char *argv[]) {
  gchar *myname;

  ign_opts.ign_clipset=ign_opts.ign_osc=ign_opts.ign_jackopts=ign_opts.ign_aplayer=FALSE;

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

  gtk_set_locale ();
  putenv ("LC_NUMERIC=C");

  gtk_init (&argc, &argv);

  theme_expected=pre_init();

  mainw->foreign=FALSE;
  memset (start_file,0,1);

  capable->myname_full=g_strdup(argv[0]);
  if ((myname=strrchr(argv[0],'/'))==NULL) capable->myname=g_strdup(argv[0]);
  else capable->myname=g_strdup(++myname);

  g_set_application_name("LiVES");

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
	{"set", 1, 0, 0},
	{"noset", 0, 0, 0},
        {"devicemap", 1, 0, 0},
	{"recover", 0, 0, 0},
	{"norecover", 0, 0, 0},
	{"nogui", 0, 0, 0},
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
	if (!strcmp(charopt,"recover")) {
	  // auto recovery
	  auto_recover=TRUE;
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
        if (!strcmp(charopt,"devicemap")&&optarg!=NULL) {
          // force devicemap loading
          on_midi_load_activate(NULL, optarg);
          continue;
        }
	if (!strcmp(charopt,"aplayer")) {
	  gchar buff[256];
	  gboolean apl_valid=FALSE;

	  g_snprintf(buff,256,"%s",optarg);
	  // override aplayer default
	  if (!strcmp(buff,"sox")) {
	    switch_aud_to_sox();
	    apl_valid=TRUE;
	  }
	  if (!strcmp(buff,"mplayer")) {
	    switch_aud_to_mplayer();
	    apl_valid=TRUE;
	  }
	  if (!strcmp(buff,"jack")) {
#ifdef ENABLE_JACK
	    switch_aud_to_jack();
	    apl_valid=TRUE;
#endif
	  }
	  if (!strcmp(buff,"pulse")) {
#ifdef HAVE_PULSE_AUDIO
	    switch_aud_to_pulse();
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
	  ign_opts.ign_jackopts=TRUE;
	  continue;
	}
#endif
      }
      if (optind<argc) {
	// remaining opts are filename [start_time] [end_frame]
	g_snprintf(start_file,256,"%s",argv[optind++]); // filename
	if (optind<argc) start=g_strtod(argv[optind++],NULL); // start time (seconds)
	if (optind<argc) end=atoi(argv[optind++]); // number of frames
      }}}
  

  g_idle_add(lives_startup,NULL);

  gtk_main ();
  return 0;
}




gboolean startup_message_fatal(gchar *msg) {
  do_blocking_error_dialog (msg);
  lives_exit();
  return TRUE;
}

gboolean startup_message_nonfatal(gchar *msg) {
  do_error_dialog (msg);
  return TRUE;
}

gboolean startup_message_nonfatal_dismissable(gchar *msg, gint warning_mask) {
  do_error_dialog_with_check (msg, warning_mask);
  return TRUE;
}


void set_main_title(const gchar *file, gint untitled) {
  gchar *title,*tmp;
  gchar short_file[256];

  if (file!=NULL) {
    if (untitled) title=g_strdup_printf(_ ("LiVES-%s: <Untitled%d> %dx%d : %d frames %d bpp %.3f fps"),LiVES_VERSION,untitled,cfile->hsize,cfile->vsize,cfile->frames,cfile->bpp,cfile->fps);
    else {
      g_snprintf(short_file,256,"%s",file);
      if (cfile->restoring||(cfile->opening&&cfile->frames==123456789)) {
	title=g_strdup_printf(_("LiVES-%s: <%s> %dx%d : ??? frames ??? bpp %.3f fps"),LiVES_VERSION,(tmp=g_path_get_basename(file)),cfile->hsize,cfile->vsize,cfile->fps);
      }
      else {
	title=g_strdup_printf(_ ("LiVES-%s: <%s> %dx%d : %d frames %d bpp %.3f fps"),LiVES_VERSION,(tmp=g_path_get_basename(file)),cfile->hsize,cfile->vsize,cfile->frames,cfile->bpp,cfile->fps);
      }
      g_free(tmp);
    }
  }
  else {
    title=g_strdup_printf(_ ("LiVES-%s: <No File>"),LiVES_VERSION);
  }

  gtk_window_set_title (GTK_WINDOW (mainw->LiVES), title);
  g_free(title);
}


void sensitize(void) {
  // sensitize main window controls
  // READY MODE
  int i;

  gtk_widget_set_sensitive (mainw->open, TRUE);
  gtk_widget_set_sensitive (mainw->open_sel, TRUE);
  gtk_widget_set_sensitive (mainw->open_vcd_menu, TRUE);
  gtk_widget_set_sensitive (mainw->open_loc, TRUE);
  gtk_widget_set_sensitive (mainw->open_device_menu, TRUE);
  gtk_widget_set_sensitive (mainw->restore, TRUE);
  gtk_widget_set_sensitive (mainw->recent_menu, TRUE);
  gtk_widget_set_sensitive (mainw->save_as, mainw->current_file>0&&capable->has_encoder_plugins);
  gtk_widget_set_sensitive (mainw->backup, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->save, mainw->current_file>0&&!(cfile->is_untitled)&&capable->has_encoder_plugins);
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
  gtk_widget_set_sensitive (mainw->delete, mainw->current_file>0&&cfile->frames>0);
  gtk_widget_set_sensitive (mainw->playall, mainw->current_file>0);
  if (mainw->multitrack==NULL) gtk_widget_set_sensitive (mainw->m_playbutton, mainw->current_file>0);
  else mt_swap_play_pause(mainw->multitrack,FALSE);
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
  gtk_widget_set_sensitive (mainw->mt_menu, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->export_proj, mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->import_proj, mainw->current_file==-1);
  gtk_widget_set_sensitive (mainw->mt_menu, TRUE);

  if (!mainw->foreign) {
    for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test;i++) if (mainw->rendered_fx[i].menuitem!=NULL) gtk_widget_set_sensitive(mainw->rendered_fx[i].menuitem,mainw->current_file>0&&cfile->frames>0);
  }

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
  gtk_widget_set_sensitive (mainw->resample_audio, mainw->current_file>0&&(cfile->achans>0&&capable->has_sox));
  gtk_widget_set_sensitive (mainw->dsize, !(mainw->fs));
  gtk_widget_set_sensitive (mainw->fade, !(mainw->fs));
  gtk_widget_set_sensitive (mainw->mute_audio, TRUE);
  gtk_widget_set_sensitive (mainw->loop_video, mainw->current_file>0&&(cfile->achans>0&&cfile->frames>0));
  gtk_widget_set_sensitive (mainw->loop_continue, TRUE);
  gtk_widget_set_sensitive (mainw->load_audio, TRUE);
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
  gtk_widget_set_sensitive (mainw->toy_random_frames, TRUE);
  gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
  gtk_widget_set_sensitive (mainw->gens_submenu, TRUE);

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
  //gtk_widget_set_sensitive (mainw->open, mainw->playing_file>-1);
  gtk_widget_set_sensitive (mainw->open, FALSE);
  gtk_widget_set_sensitive (mainw->open_sel, FALSE);
  gtk_widget_set_sensitive (mainw->open_vcd_menu, FALSE);
  gtk_widget_set_sensitive (mainw->open_loc, FALSE);
  gtk_widget_set_sensitive (mainw->open_device_menu, FALSE);
#ifdef HAVE_YUV4MPEG
  gtk_widget_set_sensitive (mainw->open_yuv4m, FALSE);
#endif
  gtk_widget_set_sensitive (mainw->recent_menu, FALSE);
  gtk_widget_set_sensitive (mainw->restore, FALSE);
  gtk_widget_set_sensitive (mainw->clear_ds, FALSE);
  gtk_widget_set_sensitive (mainw->save_as, FALSE);
  gtk_widget_set_sensitive (mainw->backup, FALSE);
  gtk_widget_set_sensitive (mainw->save, FALSE);
  gtk_widget_set_sensitive (mainw->playsel, FALSE);
  gtk_widget_set_sensitive (mainw->playclip, FALSE);
  gtk_widget_set_sensitive (mainw->copy, FALSE);
  gtk_widget_set_sensitive (mainw->cut, FALSE);
  gtk_widget_set_sensitive (mainw->rev_clipboard, FALSE);
  gtk_widget_set_sensitive (mainw->insert, FALSE);
  gtk_widget_set_sensitive (mainw->merge, FALSE);
  gtk_widget_set_sensitive (mainw->delete, FALSE);
  if (!prefs->pause_during_pb) {
    gtk_widget_set_sensitive (mainw->playall, FALSE);
  }
  gtk_widget_set_sensitive (mainw->rewind,FALSE);
  if (!mainw->foreign) {
    for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test;i++) if (mainw->rendered_fx[i].menuitem!=NULL&&mainw->rendered_fx[i].menuitem!=NULL&&mainw->rendered_fx[i].min_frames>=0) gtk_widget_set_sensitive(mainw->rendered_fx[i].menuitem,FALSE);
  }
  gtk_widget_set_sensitive (mainw->mt_menu, FALSE);
  gtk_widget_set_sensitive (mainw->export_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->recaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->append_audio, FALSE);
  gtk_widget_set_sensitive (mainw->trim_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->delaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->gens_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->resample_audio, FALSE);
  gtk_widget_set_sensitive (mainw->fade_aud_in, FALSE);
  gtk_widget_set_sensitive (mainw->fade_aud_out, FALSE);
  gtk_widget_set_sensitive (mainw->ins_silence, FALSE);
  gtk_widget_set_sensitive (mainw->loop_video, (prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE));
  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) gtk_widget_set_sensitive (mainw->mute_audio, FALSE);
  gtk_widget_set_sensitive (mainw->load_audio, FALSE);
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
  gtk_widget_set_sensitive (mainw->mt_menu, FALSE);
  gtk_widget_set_sensitive(mainw->recaudio_sel,FALSE);

  if (mainw->current_file>=0&&(mainw->playing_file==-1||mainw->foreign)) {
    if (!cfile->opening||mainw->dvgrab_preview||mainw->preview||cfile->opening_only_audio) {
      // disable the 'clips' menu entries
      for (i=1;i<MAX_FILES;i++) {
	if (!(mainw->files[i]==NULL)) {
	  if (!(mainw->files[i]->menuentry==NULL)) {
	    if (!(i==mainw->current_file)) {
	      gtk_widget_set_sensitive (mainw->files[i]->menuentry, FALSE);
	    }}}}}}
}


void 
procw_desensitize(void) {
  // switch on/off a few extra widgets in the processing dialog
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
  gtk_widget_set_sensitive (mainw->trim_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->delaudio_submenu, FALSE);
  gtk_widget_set_sensitive (mainw->load_cdtrack, FALSE);
  gtk_widget_set_sensitive (mainw->open_lives2lives, FALSE);

  if (mainw->current_file>0&&cfile->nopreview) {
    gtk_widget_set_sensitive (mainw->m_playbutton, FALSE);
    if (mainw->preview_box!=NULL) gtk_widget_set_sensitive (mainw->p_playbutton, FALSE);
    gtk_widget_set_sensitive (mainw->m_playselbutton, FALSE);
    if (mainw->preview_box!=NULL) gtk_widget_set_sensitive (mainw->p_playselbutton, FALSE);
  }
}


void load_start_image(gint frame) {
  GdkPixbuf *start_pixbuf=NULL;
  weed_plant_t *layer;
  weed_timecode_t tc;


  if (frame<1||frame>cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
    if (!(mainw->imframe==NULL)) {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image272),mainw->imframe);
    }
    else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image272),NULL);
    }
    return;
  }

  layer=weed_plant_new(WEED_PLANT_CHANNEL);
  tc=((frame-1.))/cfile->fps*U_SECL;
  weed_set_int_value(layer,"clip",mainw->current_file);
  weed_set_int_value(layer,"frame",frame);

  if (pull_frame(layer,prefs->image_ext,tc)) {
    convert_layer_palette(layer,WEED_PALETTE_RGB24,0);  
    start_pixbuf=layer_to_pixbuf(layer);
  }
  weed_plant_free(layer);

  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image272),start_pixbuf);
  if (start_pixbuf!=NULL) {
    gdk_pixbuf_unref(start_pixbuf);
  }

}



void load_end_image(gint frame) {
  GdkPixbuf *end_pixbuf=NULL;
  weed_plant_t *layer;
  weed_timecode_t tc;

  if (frame<1||frame>cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
    if (!(mainw->imframe==NULL)) {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image273),mainw->imframe);
    }
    else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image273),NULL);
    }
    return;
  }

  tc=((frame-1.))/cfile->fps*U_SECL;

  layer=weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(layer,"clip",mainw->current_file);
  weed_set_int_value(layer,"frame",frame);
  if (pull_frame(layer,prefs->image_ext,tc)) {
    convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
    end_pixbuf=layer_to_pixbuf(layer);
  }
  weed_plant_free(layer);

  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image273),end_pixbuf);
  if (end_pixbuf!=NULL) {
    gdk_pixbuf_unref(end_pixbuf);
  }
}




void 
load_preview_image(gboolean update_always) {
  // this is for the sepwin preview
  // update_always==TRUE = update widgets from mainw->preview_frame
  GdkPixbuf *pixbuf=NULL;
  gint preview_frame;

  if (!cfile->frames||(cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->preview_image), NULL);
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
	if (preview_frame>cfile->frames) preview_frame=mainw->preview_frame=cfile->frames;
	break;
      }

      g_signal_handler_block(mainw->preview_spinbutton,mainw->preview_spin_func);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->preview_spinbutton),1,cfile->frames);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->preview_spinbutton),preview_frame);
      g_signal_handler_unblock(mainw->preview_spinbutton,mainw->preview_spin_func);

      if (mainw->preview_frame>cfile->frames&&preview_frame<=cfile->frames) {
	g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
	mainw->pw_exp_is_blocked=TRUE;
      }
      mainw->preview_frame=preview_frame;
  }
  
  if (mainw->preview_frame<1||mainw->preview_frame>cfile->frames) {
    pixbuf=gdk_pixbuf_scale_simple(mainw->imframe,cfile->hsize,cfile->vsize,GDK_INTERP_HYPER);
  }
  else {
    weed_plant_t *layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_timecode_t tc=((mainw->preview_frame-1.))/cfile->fps*U_SECL;
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",mainw->preview_frame);
    if (pull_frame(layer,prefs->image_ext,tc)) {
      convert_layer_palette(layer,WEED_PALETTE_RGB24,0);  
      pixbuf=layer_to_pixbuf(layer);
    }
    weed_plant_free(layer);
  }
  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->preview_image), pixbuf);

  if (update_always) {
    // set spins from current frame
    switch (mainw->prv_link) {
    case PRV_PTR:
      //cf. hrule_reset
      if ((GTK_RULER (mainw->hruler)->position=cfile->pointer_time=calc_time_from_frame(mainw->current_file,mainw->preview_frame))>0.) {
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
	gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image272),pixbuf);
	get_play_times();
      }
      break;
      

    case PRV_END:
      if (cfile->end!=mainw->preview_frame) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),mainw->preview_frame);
	gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image273),pixbuf);
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
  if (pixbuf!=NULL) gdk_pixbuf_unref(pixbuf);
}




gboolean pull_frame_at_size (weed_plant_t *layer, gchar *image_ext, weed_timecode_t tc, int width, int height, int target_palette) {
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
  int nplanes;
  GdkPixbuf *pixbuf=NULL;
  gboolean do_not_free=TRUE;
  weed_plant_t *vlayer;
  int i;
  void **pixel_data;
  int clip_type;

  file *sfile=NULL;

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
      if (!weed_plant_has_leaf(layer,"current_palette")) weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGB24);
      create_empty_pixel_data(layer);
      return TRUE;
    }
    else {
      if (sfile->clip_type==CLIP_TYPE_FILE&&sfile->frame_index!=NULL&&frame>0&&frame<=sfile->frames&&sfile->frame_index[frame-1]>=0) {
	gchar *tmp;
	_decoder_plugin *dplug=(_decoder_plugin *)sfile->ext_src;

	if (target_palette!=dplug->current_palette&&dplug->set_palette!=NULL) {
	  // try to switch palette
	  if ((*dplug->set_palette)(target_palette)) {
	    // sucess ! re-read clip data
	    const lives_clip_data_t *cdata=(*dplug->get_clip_data)((tmp=(char *)g_filename_from_utf8 (sfile->file_name,-1,NULL,NULL,NULL)));
	    g_free(tmp);
	    
	    dplug->YUV_sampling=cdata->YUV_sampling;
	    dplug->YUV_clamping=cdata->YUV_clamping;
	    dplug->YUV_subspace=cdata->YUV_subspace;
	    dplug->interlace=cdata->interlace;
	    dplug->current_palette=target_palette;
	  }
	  else {
	    if (dplug->current_palette!=dplug->preferred_palette&&((weed_palette_is_rgb_palette(target_palette)&&weed_palette_is_rgb_palette(dplug->preferred_palette))||(weed_palette_is_yuv_palette(target_palette)&&weed_palette_is_yuv_palette(dplug->preferred_palette)))) {
	      if ((*dplug->set_palette)(dplug->preferred_palette)) {
		// sucess ! re-read clip data
		const lives_clip_data_t *cdata=(*dplug->get_clip_data)((tmp=(char *)g_filename_from_utf8 (sfile->file_name,-1,NULL,NULL,NULL)));
		g_free(tmp);
		
		dplug->YUV_sampling=cdata->YUV_sampling;
		dplug->YUV_clamping=cdata->YUV_clamping;
		dplug->YUV_subspace=cdata->YUV_subspace;
		dplug->interlace=cdata->interlace;
		dplug->current_palette=dplug->preferred_palette;
	      }
	    }
	  }
	}
	width=sfile->hsize/weed_palette_get_pixels_per_macropixel(dplug->current_palette);
	height=sfile->vsize;
	weed_set_int_value(layer,"width",width);
	weed_set_int_value(layer,"height",height);
	weed_set_int_value(layer,"current_palette",dplug->current_palette);
	weed_set_int_value(layer,"YUV_sampling",dplug->YUV_sampling);
	weed_set_int_value(layer,"YUV_clamping",dplug->YUV_clamping);
	weed_set_int_value(layer,"YUV_subspace",dplug->YUV_subspace);
	create_empty_pixel_data(layer);

	pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

	(*dplug->get_frame)((tmp=(char *)g_filename_from_utf8 (sfile->file_name,-1,NULL,NULL,NULL)),(int64_t)(sfile->frame_index[frame-1]),pixel_data);

	g_free(tmp);
	g_free(pixel_data);

	if (sfile->deinterlace||(prefs->auto_deint&&dplug->interlace!=LIVES_INTERLACE_NONE)) deinterlace_frame(layer,tc);

	mainw->osc_block=FALSE;
	return TRUE;
      }
      else {
	gchar *fname=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,sfile->handle,frame,image_ext);
	if (height*width==0) {
	  pixbuf=gdk_pixbuf_new_from_file(fname,&gerror);
	}
	else {
	  pixbuf=gdk_pixbuf_new_from_file_at_scale(fname,width,height,FALSE,&gerror);
	}
	g_free(fname);
	mainw->osc_block=FALSE;
	if (gerror!=NULL) {
	  g_error_free(gerror);
	  pixbuf=NULL;
	}
      }
    }
    break;
#ifdef HAVE_YUV4MPEG
  case CLIP_TYPE_YUV4MPEG:
    weed_layer_set_from_yuv4m(layer,sfile->ext_src);
    mainw->osc_block=FALSE;
    return TRUE;
#endif
  case CLIP_TYPE_LIVES2LIVES:
    weed_layer_set_from_lives2lives(layer,clip,sfile->ext_src);
    mainw->osc_block=FALSE;
    return TRUE;
  case CLIP_TYPE_GENERATOR:
    // special handling for clips where host controls size
    vlayer=weed_layer_new_from_generator((weed_plant_t *)sfile->ext_src,tc);
    weed_layer_copy(layer,vlayer);
    nplanes=weed_leaf_num_elements(vlayer,"pixel_data");
    pixel_data=weed_get_voidptr_array(vlayer,"pixel_data",&error);
    for (i=0;i<nplanes;i++) g_free(pixel_data[i]);
    g_free(pixel_data);
    weed_set_voidptr_value(vlayer,"pixel_data",NULL);
    mainw->osc_block=FALSE;
    return TRUE;
  default:
    mainw->osc_block=FALSE;
    return FALSE;
  }
  mainw->osc_block=FALSE;

  if (pixbuf==NULL) return FALSE;

  do_not_free=pixbuf_to_layer(layer,pixbuf);

  if (do_not_free) {
    mainw->do_not_free=gdk_pixbuf_get_pixels(pixbuf);
    mainw->free_fn=lives_free_with_check;
  }
  g_object_unref(pixbuf);
  mainw->do_not_free=NULL;
  mainw->free_fn=free;

  return TRUE;
}


gboolean pull_frame (weed_plant_t *layer, gchar *image_ext, weed_timecode_t tc) {
  // pull a frame from an external source into a layer
  // the "clip" and "frame" leaves must be set in layer
  // tc is used instead of "frame" for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")

  return pull_frame_at_size(layer,image_ext,tc,0,0,WEED_PALETTE_END);
}


GdkPixbuf *pull_gdk_pixbuf_at_size(gint clip, gint frame, gchar *image_ext, weed_timecode_t tc, gint width, gint height, GdkInterpType interp) {
  // return a correctly sized GdkPixbuf (RGB24) for the given clip and frame
  // tc is used instead of "frame" for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")
  // pixbuf will be sized to width x height pixels using interp

  GdkPixbuf *pixbuf=NULL;
  weed_plant_t *layer=weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(layer,"clip",clip);
  weed_set_int_value(layer,"frame",frame);
  if (pull_frame_at_size(layer,image_ext,tc,width,height,WEED_PALETTE_RGB24)) {
    convert_layer_palette(layer,WEED_PALETTE_RGB24,0);  
    pixbuf=layer_to_pixbuf(layer);
  }
  weed_plant_free(layer);
  if (pixbuf!=NULL&&(gdk_pixbuf_get_width(pixbuf)!=width||gdk_pixbuf_get_height(pixbuf)!=height)) {
    GdkPixbuf *pixbuf2=gdk_pixbuf_scale_simple(pixbuf,width,height,interp);
    gdk_pixbuf_unref(pixbuf);
    pixbuf=pixbuf2;
  }
  return pixbuf;
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
	  *opwidth=mainw->multitrack->play_width;
	  *opheight=mainw->multitrack->play_height;
	}
	else {
	  *opwidth=cfile->hsize;
	  *opheight=cfile->vsize;
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
      if (prefs->play_monitor==0) {
	if (mainw->scr_width>*opwidth) *opwidth=mainw->scr_width;
	if (mainw->scr_height>*opheight) *opwidth=mainw->scr_width;
      }
      else {
	if (mainw->play_window!=NULL) pmonitor=prefs->play_monitor;
	else pmonitor=prefs->gui_monitor;
	if (mainw->mgeom[pmonitor-1].width>*opwidth) *opwidth=mainw->mgeom[pmonitor-1].width;
	if (mainw->mgeom[pmonitor-1].height>*opheight) *opheight=mainw->mgeom[pmonitor-1].height;
      }
    }
    else {
      if (cfile->hsize>*opwidth) *opwidth=cfile->hsize;
      if (cfile->vsize>*opheight) *opheight=cfile->vsize;
    }
  }
}






void load_frame_image(gint frame, gint last_frame) {
  // this is where we do the actual load/record of a playback frame
  // it is called every 1/fps from do_processing_dialog() in dialogs.c

  // for the multitrack window we set mainw->frame_image; this is used to display the
  // preview image

  // NOTE: we should be careful if load_frame_image() is called from anywhere inside load_frame_image()
  // e.g. by calling g_main_context_iteration() --> user presses sepwin button --> load_frame_image() is called
  // this is because mainw->frame_layer is global and gets freed() before exit from load_frame_image()
  // - we should never call load_frame_image() if mainw->noswitch is TRUE


  gchar *framecount=NULL;
  gboolean was_preview;
  gchar *fname_next=NULL,*info_file=NULL;
  int weed_error;
  void **pd_array;
  int fx_layer_palette;
  gchar *img_ext=NULL;
  GdkPixbuf *pixbuf=NULL;
  gboolean saved_to_scrap_file=FALSE;
  int opwidth=0,opheight=0;
  GdkInterpType interp;
  gboolean noswitch=mainw->noswitch;
  gint pmonitor;

  if (G_UNLIKELY(cfile->frames==0&&!mainw->foreign&&!mainw->is_rendering)) {
    if (mainw->record&&!mainw->record_paused) {
      // add blank frame
      weed_plant_t *event=get_last_event(mainw->event_list);
      weed_plant_t *event_list=insert_blank_frame_event_at(mainw->event_list,mainw->currticks-mainw->origticks,&event);
      if (mainw->rec_aclip!=-1&&(prefs->rec_opts&REC_AUDIO)&&!mainw->record_starting) {
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
	if ((mainw->actual_frame+=(-10+(int) (21.*rand()/(RAND_MAX+1.0))))>cfile->frames||mainw->actual_frame<1) mainw->actual_frame=frame;
	else {
	  frame=mainw->actual_frame;
#ifdef ENABLE_JACK
	  if (prefs->audio_player==AUD_PLAYER_JACK&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)&&mainw->jackd!=NULL&&cfile->achans>0) {
	    jack_audio_seek_frame(mainw->jackd,frame);
	    mainw->rec_aclip=mainw->current_file;
	    mainw->rec_avel=cfile->pb_fps/cfile->fps;
	    mainw->rec_aseek=cfile->aseek_pos/(cfile->arate*cfile->achans*cfile->asampsize/8);
	    
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (prefs->audio_player==AUD_PLAYER_PULSE&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)&&mainw->pulsed!=NULL&&cfile->achans>0) {
	    pulse_audio_seek_frame(mainw->pulsed,frame);
	    mainw->rec_aclip=mainw->current_file;
	    mainw->rec_avel=cfile->pb_fps/cfile->fps;
	    mainw->rec_aseek=cfile->aseek_pos/(cfile->arate*cfile->achans*cfile->asampsize/8);
	    
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
	gint bg_file=mainw->blend_file>0&&mainw->blend_file!=mainw->current_file?mainw->blend_file:-1;
	gint bg_frame=mainw->blend_file>0&&mainw->blend_file!=-1?mainw->files[mainw->blend_file]->frameno:0;
	int numframes;
	int *clips,*frames;
	weed_plant_t *event_list;

	if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(prefs->rec_opts&REC_EFFECTS&&bg_file!=-1&&(mainw->files[bg_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[bg_file]->clip_type!=CLIP_TYPE_FILE))) {
	  // TODO - handle non-opening of scrap_file
	  if (mainw->scrap_file==-1) open_scrap_file();
	  fg_file=mainw->scrap_file;
	  fg_frame=mainw->files[mainw->scrap_file]->frames+1;
	  bg_file=-1;
	  bg_frame=0;
	}

	if (mainw->record_starting) {
	  event_list=append_marker_event(mainw->event_list, mainw->currticks-mainw->origticks, EVENT_MARKER_RECORD_START); // mark record start
	  if (mainw->event_list==NULL) mainw->event_list=event_list;
	  
	  if (prefs->rec_opts&REC_EFFECTS) {
	    // add init events and pchanges for all active fx
	    add_filter_init_events(mainw->event_list,mainw->currticks-mainw->origticks);
	  }

#ifdef ENABLE_JACK
	  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&prefs->rec_opts&REC_AUDIO&&mainw->rec_aclip!=-1) {
	    // get current seek postion
	    jack_get_rec_avals(mainw->jackd);
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&prefs->rec_opts&REC_AUDIO&&mainw->rec_aclip!=-1) {
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
	if ((event_list=append_frame_event (mainw->event_list,mainw->currticks-mainw->origticks,numframes,clips,frames))!=NULL) {
	  if (mainw->event_list==NULL) mainw->event_list=event_list;
	  if (mainw->rec_aclip!=-1&&(prefs->rec_opts&REC_AUDIO)) {
	    weed_plant_t *event=get_last_event(mainw->event_list);
	    insert_audio_event_at(mainw->event_list,event,-1,mainw->rec_aclip,mainw->rec_aseek,mainw->rec_avel);
	    mainw->rec_aclip=-1;
	  }
	  framecount=g_strdup_printf("rec %9d/%d",mainw->actual_frame,cfile->frames>mainw->actual_frame?cfile->frames:mainw->actual_frame);
	}
	else (framecount=g_strdup_printf("!rec %9d/%d",mainw->actual_frame,cfile->frames)); // out of memory
	g_free(clips);
	g_free(frames);
      }
      else {
	if (mainw->toy_type!=TOY_NONE) {
	  if (mainw->toy_type==TOY_RANDOM_FRAMES&&!mainw->fs&&cfile->clip_type==CLIP_TYPE_DISK) {
	    gint current_file=mainw->current_file;
	    if (mainw->toy_go_wild) {
	      int i,other_file;
	      for (i=0;i<11;i++) {
		other_file=(1+(int) ((double)(mainw->clips_available)*rand()/(RAND_MAX+1.0)));
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
      
      if ((!mainw->fs||prefs->play_monitor!=prefs->gui_monitor)&&prefs->show_framecount) {
	gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),framecount);
      }
      g_free(framecount);
      framecount=NULL;
    }

    if (was_preview) {
      info_file=g_strdup_printf("%s/%s/.status",prefs->tmpdir,cfile->handle);
      // preview
      if (prefs->safer_preview&&cfile->proc_ptr!=NULL&&cfile->proc_ptr->frames_done>0&&frame>=(cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start)) {
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
	if (mainw->toy_type!=TOY_NONE) {
	  // TODO - move into toys.c
	  if (mainw->toy_type==TOY_RANDOM_FRAMES&&!mainw->fs) {
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
	    img_ext=g_strdup("mgk");
	    fname_next=g_strdup_printf("%s/%s/%08d.mgk",prefs->tmpdir,cfile->handle,frame+1);
	  }
	  else {
	    img_ext=g_strdup("pre");
	    fname_next=g_strdup_printf("%s/%s/%08d.pre",prefs->tmpdir,cfile->handle,frame+1);
	  }
	}
      }
      mainw->actual_frame=frame;
    }

    // maybe the performance finished and we weren't looping
    if ((mainw->actual_frame<1||mainw->actual_frame>cfile->frames)&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)&&!mainw->is_rendering) {
      if (img_ext!=NULL) g_free(img_ext);
      mainw->noswitch=noswitch;
      if (framecount!=NULL) g_free(framecount);
      return;
    }


    // limit max frame size unless we are saving to disk or rendering

    // frame_layer will in any case be equal to or smaller than this depending on maximum source frame size 

    if (!(mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE)))) {
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

	if (mainw->clip_index[0]==mainw->scrap_file&&mainw->clip_index[0]>-1) {
	  // do not apply fx, just pull frame
	  mainw->frame_layer=weed_plant_new(WEED_PLANT_CHANNEL);
	  weed_set_int_value(mainw->frame_layer,"clip",mainw->clip_index[0]);
	  weed_set_int_value(mainw->frame_layer,"frame",mainw->frame_index[0]);
	  if (!pull_frame(mainw->frame_layer,prefs->image_ext,tc)) {
	    weed_plant_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;
	  }
	}
	else {
	  int i;
	  weed_plant_t **layers=g_malloc((mainw->num_tracks+1)*sizeof(weed_plant_t *));
	  for (i=0;i<mainw->num_tracks;i++) {
	    layers[i]=weed_plant_new(WEED_PLANT_CHANNEL);
	    weed_set_int_value(layers[i],"clip",mainw->clip_index[i]);
	    weed_set_int_value(layers[i],"frame",mainw->frame_index[i]);
	    weed_set_int_value(layers[i],"current_palette",WEED_PALETTE_RGB24);
	    weed_set_voidptr_value(layers[i],"pixel_data",NULL);
	  }
	  layers[i]=NULL;
	  
	  mainw->frame_layer=weed_apply_effects(layers,mainw->filter_map,tc,opwidth,opheight,mainw->pchains);
	  
	  for (i=0;layers[i]!=NULL;i++) if (layers[i]!=mainw->frame_layer) weed_plant_free(layers[i]);
	  g_free(layers);
	}

	if (mainw->internal_messaging) {
	  // this happens if we are calling from multitrack, or apply rte.  We get our mainw->frame_layer and exit.
	  if (img_ext!=NULL) g_free(img_ext);
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
	  if (img_ext==NULL) img_ext=g_strdup(prefs->image_ext);
	  if (!pull_frame_at_size(mainw->frame_layer,img_ext,(weed_timecode_t)(mainw->currticks-mainw->origticks),cfile->hsize,cfile->vsize,WEED_PALETTE_END)) {
	    if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;
	  }
	}

	if ((cfile->next_event==NULL&&mainw->is_rendering&&!mainw->switch_during_pb&&(mainw->multitrack==NULL||!mainw->multitrack->is_rendering))||((!mainw->is_rendering||(mainw->multitrack!=NULL&&mainw->multitrack->is_rendering))&&mainw->preview&&mainw->frame_layer==NULL)) {
	  // preview ended
	  
	  if (!cfile->opening) mainw->cancelled=CANCEL_NO_MORE_PREVIEW;
	  if (mainw->cancelled) {
	    g_free(fname_next);
	    g_free(info_file);
	    if (img_ext!=NULL) g_free(img_ext);
	    mainw->noswitch=noswitch;
	    if (framecount!=NULL) g_free(framecount);
	    return;
	  }
	  gettimeofday(&tv, NULL);
	  mainw->currticks=U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO;
	  mainw->startticks=mainw->currticks+mainw->deltaticks;
	}
	  
	if (img_ext!=NULL) g_free(img_ext);
	
	if (mainw->internal_messaging) {
	  mainw->noswitch=noswitch;
	  if (framecount!=NULL) g_free(framecount);
	  return;
	}
	
	if (mainw->frame_layer==NULL&&(!mainw->preview||mainw->multitrack!=NULL)) {
	  mainw->noswitch=noswitch;
	  if (framecount!=NULL) g_free(framecount);
	  return;
	}
	
	if (mainw->preview&&mainw->frame_layer==NULL&&mainw->event_list==NULL) {
	  FILE *fd;
	  // non-realtime effect preview
	  // check effect to see if it finished yet
	  if ((fd=fopen(info_file,"r"))) {
	    clear_mainw_msg();
	    dummychar=fgets(mainw->msg,512,fd);
	    fclose(fd);
	    if (!strncmp(mainw->msg,"completed",9)||!strncmp(mainw->msg,"error",5)) {
	      // effect completed whilst we were busy playing a preview
	      if (mainw->preview_box!=NULL) gtk_tooltips_set_tip (mainw->tooltips, mainw->p_playbutton,_ ("Play"), NULL);
	      gtk_tooltips_set_tip (mainw->tooltips, mainw->m_playbutton,_ ("Play"), NULL);
	      if (cfile->opening&&!cfile->is_loaded) {
		if (mainw->toy_type==TOY_TV) {
		  on_toy_activate(NULL,LIVES_TOY_NONE);
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
	  if ((!cfile->opening&&frame>=(cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start))||(cfile->opening&&(mainw->toy_type==LIVES_TOY_TV||!mainw->preview))) {
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
	  else if (mainw->preview||cfile->opening) while (g_main_context_iteration (NULL, FALSE));
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

    // OK. Here is the deal now. We have a layer from the current file, current frame. We will pass this into the effects, and we will get back a layer.
    // The palette of the effected layer could be any Weed palette. We will pass the layer to all playback plugins.
    // Finally we may want to end up with another GkdPixbuf (unless the playback plugin is VPP_DISPLAY_LOCAL and we are in full screen mode).
    // We also need a GdkPixbuf if we are saving to the scrap_file (for now).

    if ((mainw->current_file!=mainw->scrap_file||mainw->multitrack!=NULL)&&!(mainw->is_rendering&&!(cfile->proc_ptr!=NULL&&mainw->preview))) {
      if ((weed_get_int_value(mainw->frame_layer,"height",&weed_error)==cfile->vsize)&&(weed_get_int_value(mainw->frame_layer,"width",&weed_error)*weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->frame_layer)))==cfile->hsize) {
	if ((mainw->rte!=0||mainw->is_rendering)&&mainw->current_file!=mainw->scrap_file) {
	  mainw->frame_layer=on_rte_apply (mainw->frame_layer, opwidth, opheight, (weed_timecode_t)(mainw->currticks-mainw->origticks));
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
    if (!mainw->foreign&&mainw->jackd!=NULL&&prefs->audio_player==AUD_PLAYER_JACK) while (jack_get_msgq(mainw->jackd)!=NULL);
#endif
#ifdef HAVE_PULSE_AUDIO
    if (!mainw->foreign&&mainw->pulsed!=NULL&&prefs->audio_player==AUD_PLAYER_PULSE) while (pulse_get_msgq(mainw->pulsed)!=NULL);
#endif

     fx_layer_palette=weed_layer_get_palette(mainw->frame_layer);

    // if our output layer is RGB24, save to scrap_file now if we have to
     if (mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE))) {
       if (fx_layer_palette==WEED_PALETTE_RGB24||fx_layer_palette==WEED_PALETTE_RGBA32) {
	 pixbuf=layer_to_pixbuf(mainw->frame_layer);
	 save_to_scrap_file (pixbuf);
	 mainw->do_not_free=gdk_pixbuf_get_pixels(pixbuf);
	 mainw->free_fn=lives_free_with_check;
	 g_object_unref(pixbuf);
	 pixbuf=NULL;
	 mainw->do_not_free=NULL;
	 mainw->free_fn=free;
	 saved_to_scrap_file=TRUE;
       }
     }
     
    if (mainw->ext_playback&&(mainw->vpp->capabilities&VPP_CAN_RESIZE)) {
      // here we are outputting video through a video playback plugin which can resize: thus we just send whatever we have
      // we need only to convert the palette to whatever was agreed with the plugin when we called set_palette() in plugins.c

      // 

      weed_plant_t *fx_layer_copy;
      
      if (weed_palette_is_yuv_palette(mainw->vpp->palette)&&(weed_palette_is_rgb_palette(fx_layer_palette)||weed_palette_is_lower_quality(mainw->vpp->palette,fx_layer_palette))) {
	// copy layer to another layer, unless we are going to a higher quality yuv palette, or we keep in rgb cspace
	fx_layer_copy=weed_layer_copy(NULL,mainw->frame_layer);
      }
      else fx_layer_copy=mainw->frame_layer;
     
      convert_layer_palette(fx_layer_copy,mainw->vpp->palette,mainw->vpp->YUV_clamping);
      
      pd_array=weed_get_voidptr_array(fx_layer_copy,"pixel_data",&weed_error);

      if (mainw->stream_ticks==-1) mainw->stream_ticks=(mainw->currticks);


      if (!(*mainw->vpp->render_frame)(weed_get_int_value(fx_layer_copy,"width",&weed_error),weed_get_int_value(fx_layer_copy,"height",&weed_error),mainw->currticks-mainw->stream_ticks,pd_array,NULL)) {
	if (mainw->vpp->exit_screen!=NULL) (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
	mainw->stream_ticks=-1;
	mainw->ext_playback=FALSE;
	mainw->ext_keyboard=FALSE;
      }
      weed_free(pd_array);
      
      if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY) {
	if (mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE))) {
	  if (!saved_to_scrap_file) {
	    if (mainw->vpp->palette==WEED_PALETTE_RGB24||mainw->vpp->palette==WEED_PALETTE_RGBA32) {
	      pixbuf=layer_to_pixbuf(fx_layer_copy);
	      weed_plant_free(fx_layer_copy);
	      if (mainw->frame_layer==fx_layer_copy) mainw->frame_layer=NULL;
	      fx_layer_copy=NULL;
	      save_to_scrap_file (pixbuf);
	      gdk_pixbuf_unref(pixbuf);
	    }
	    else {
	      if (fx_layer_palette!=WEED_PALETTE_RGB24&&fx_layer_palette!=WEED_PALETTE_RGBA32) convert_layer_palette(mainw->frame_layer,WEED_PALETTE_RGB24,0);
	      pixbuf=layer_to_pixbuf(mainw->frame_layer);
	      weed_plant_free(mainw->frame_layer);
	      mainw->frame_layer=NULL;
	      save_to_scrap_file (pixbuf);
	      gdk_pixbuf_unref(pixbuf);
	    }
	  }
	}
	if (fx_layer_copy!=mainw->frame_layer&&fx_layer_copy!=NULL) weed_layer_free(fx_layer_copy);
	if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
	mainw->frame_layer=NULL;
	mainw->noswitch=noswitch;

	if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor)&&mainw->current_file!=mainw->scrap_file) get_play_times();
	if (mainw->multitrack!=NULL) animate_multitrack(mainw->multitrack);
	if (framecount!=NULL) g_free(framecount);

	return;
      }
    }

    if ((mainw->multitrack==NULL&&(mainw->double_size))||(mainw->fs&&(!mainw->ext_playback||!(mainw->vpp->capabilities&VPP_CAN_RESIZE)))||(mainw->must_resize&&((!mainw->multitrack&&mainw->sep_win)||(mainw->multitrack!=NULL&&!mainw->sep_win)))) {
      if (!mainw->ext_playback||(mainw->pwidth!=mainw->vpp->fwidth||mainw->pheight!=mainw->vpp->fheight)) {
	if (mainw->multitrack!=NULL) {
	  if (!mainw->fs||mainw->play_window==NULL) {
	    if (mainw->play_window==NULL) {
	      mainw->pwidth=mainw->multitrack->play_width;
	      mainw->pheight=mainw->multitrack->play_height;
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
      mainw->pwidth=cfile->hsize;
      mainw->pheight=cfile->vsize;
    }


    if (mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_CAN_RESIZE)) {
      // here we are playing through an external video playback plugin which cannot resize
      // we must resize to whatever width and height we set when we called init_screen() in the plugin
      // i.e. mainw->vpp->fwidth, mainw->vpp fheight

      // both dimensions are in RGB(A) pixels, so we must adjust here and send macropixel size in the plugin's render_frame()

      weed_plant_t *fx_layer_copy;
      
      fx_layer_palette=weed_layer_get_palette(mainw->frame_layer);

      if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)&&((weed_palette_is_yuv_palette(mainw->vpp->palette)&&(weed_palette_is_rgb_palette(fx_layer_palette)||weed_palette_is_lower_quality(mainw->vpp->palette,fx_layer_palette)))||mainw->pwidth!=weed_get_int_value(mainw->frame_layer,"width",&weed_error)||mainw->pheight!=weed_get_int_value(mainw->frame_layer,"width",&weed_error))) {
	// copy layer to another layer, unless we are going to a higher quality yuv palette, or we keep in rgb cspace
	fx_layer_copy=weed_layer_copy(NULL,mainw->frame_layer);
      }
      else fx_layer_copy=mainw->frame_layer;

      if (prefs->pb_quality==PB_QUALITY_HIGH) {
	interp=GDK_INTERP_HYPER;
      }
      else {
	if (prefs->pb_quality==PB_QUALITY_MED) {
	  interp=GDK_INTERP_BILINEAR;
	}
	else {
	  interp=GDK_INTERP_NEAREST;
	}
      }

      if (mainw->vpp->fwidth>0&&mainw->vpp->fheight>0) resize_layer(fx_layer_copy,mainw->vpp->fwidth/weed_palette_get_pixels_per_macropixel(fx_layer_palette),mainw->vpp->fheight,interp);
      convert_layer_palette(fx_layer_copy,mainw->vpp->palette,mainw->vpp->YUV_clamping);

      if (mainw->stream_ticks==-1) mainw->stream_ticks=(mainw->currticks);

      if (!(*mainw->vpp->render_frame)(weed_get_int_value(fx_layer_copy,"width",&weed_error),weed_get_int_value(fx_layer_copy,"height",&weed_error),mainw->currticks-mainw->stream_ticks,(pd_array=weed_get_voidptr_array(fx_layer_copy,"pixel_data",&weed_error)),NULL)) {
	if (mainw->vpp->exit_screen!=NULL) (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
	mainw->stream_ticks=-1;
	mainw->ext_playback=FALSE;
	mainw->ext_keyboard=FALSE;
      }
      g_free(pd_array);


      if (mainw->ext_playback&&(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)) {
	if (mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE))) {
	  if (!saved_to_scrap_file) {
	    fx_layer_palette=weed_layer_get_palette(fx_layer_copy);
	    if (fx_layer_palette!=WEED_PALETTE_RGB24&&fx_layer_palette!=WEED_PALETTE_RGBA32) convert_layer_palette(fx_layer_copy,WEED_PALETTE_RGB24,0);
	    pixbuf=layer_to_pixbuf(fx_layer_copy);
	    save_to_scrap_file (pixbuf);
	    if (fx_layer_copy!=mainw->frame_layer) weed_layer_free(mainw->frame_layer);
	    mainw->frame_layer=NULL;
	    weed_plant_free(fx_layer_copy);
	    fx_layer_copy=NULL;
	    gdk_pixbuf_unref(pixbuf);
	  }
	}
	if (fx_layer_copy!=NULL) weed_layer_free(fx_layer_copy);
	if (mainw->frame_layer!=fx_layer_copy) weed_layer_free(mainw->frame_layer);
	mainw->frame_layer=NULL;
	mainw->noswitch=noswitch;

	if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor)&&mainw->current_file!=mainw->scrap_file) get_play_times();
	if (mainw->multitrack!=NULL) animate_multitrack(mainw->multitrack);
	if (framecount!=NULL) g_free(framecount);

	return;
      }
      if (mainw->frame_layer!=fx_layer_copy) weed_layer_free(fx_layer_copy);
    }
  
    ////////////////////////////////////////////////////////

    fx_layer_palette=weed_layer_get_palette(mainw->frame_layer);
    if (fx_layer_palette!=WEED_PALETTE_RGB24) convert_layer_palette(mainw->frame_layer,WEED_PALETTE_RGB24,0);

    pixbuf=layer_to_pixbuf(mainw->frame_layer);
    weed_plant_free(mainw->frame_layer);
    mainw->frame_layer=NULL;

    if (mainw->record&&!mainw->record_paused&&(prefs->rec_opts&REC_EFFECTS)&&((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_DISK&&mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_FILE))) {
      mainw->noswitch=noswitch;
      if (!saved_to_scrap_file) save_to_scrap_file (pixbuf);
    }
    mainw->noswitch=noswitch;

    if (mainw->fs&&!mainw->ext_playback&&(mainw->multitrack==NULL||mainw->sep_win)) {
      // set again, in case vpp was turned off because of preview conditions
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

    if (gdk_pixbuf_get_width(pixbuf)!=mainw->pwidth||gdk_pixbuf_get_height(pixbuf)!=mainw->pheight) {
      GdkPixbuf *pixbuf2=lives_scale_simple(pixbuf,mainw->pwidth,mainw->pheight);
      gdk_pixbuf_unref(pixbuf);
      pixbuf=pixbuf2;
    }

    // internal player, double size or fullscreen, or multitrack

    if (mainw->play_window!=NULL&&GDK_IS_WINDOW (mainw->play_window->window)) {
      block_expose();
      gdk_draw_pixbuf (GDK_DRAWABLE (mainw->play_window->window),mainw->gc,GDK_PIXBUF (pixbuf),0,0,0,0,-1,-1,GDK_RGB_DITHER_NONE,0,0);
      unblock_expose();
    }
    else gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),pixbuf);

    if (mainw->multitrack!=NULL) animate_multitrack(mainw->multitrack);

    else if (!mainw->faded&&(!mainw->fs||prefs->gui_monitor!=prefs->play_monitor)&&mainw->current_file!=mainw->scrap_file) get_play_times();
    
    gdk_pixbuf_unref(pixbuf);
    
#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH,"");
#endif
    if (framecount!=NULL) g_free(framecount);
    return;
  }

  // record external window
  if (mainw->record_foreign) {
    GError *gerror=NULL;

    if (mainw->rec_vid_frames==-1) gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),g_strdup_printf("%9d",frame));
    else gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),g_strdup_printf("%9d/%9d",frame,mainw->rec_vid_frames));

    if ((pixbuf=gdk_pixbuf_get_from_drawable (NULL,GDK_DRAWABLE(mainw->foreign_map),mainw->foreign_cmap,0,0,0,0,mainw->playarea->allocation.width,mainw->playarea->allocation.height))!=NULL) {
      gchar fname[256];
      g_snprintf(fname,256,"%s/%s/%08d.%s",prefs->tmpdir,cfile->handle,frame,prefs->image_ext);
      do {
	if (gerror!=NULL) g_error_free(gerror);
	gerror=NULL;
	if (!strcmp (prefs->image_ext,"jpg")) {
	  gdk_pixbuf_save (pixbuf, fname, "jpeg", &gerror,"quality", "100", NULL);
	}
	else if (!strcmp (prefs->image_ext,"png")) {
	  gdk_pixbuf_save (pixbuf, fname, "png", &gerror, NULL);
	}
	else {
	  //gdk_pixbuf_save_to_callback(...);
	}
      } while (gerror!=NULL); // TODO ** - check for disk space error
      gdk_pixbuf_unref(pixbuf);
    }
    else {
      do_error_dialog(_ ("LiVES was unable to capture this image\n\n"));
      mainw->cancelled=CANCEL_CAPTURE_ERROR;
    }
  }
  if (framecount!=NULL) g_free(framecount);
}


GdkPixbuf *lives_scale_simple (GdkPixbuf *pixbuf, gint width, gint height) {
  if (prefs->pb_quality==PB_QUALITY_HIGH) {
    return gdk_pixbuf_scale_simple(pixbuf,width,height,GDK_INTERP_HYPER);
  }
  if (prefs->pb_quality==PB_QUALITY_MED) {
    return gdk_pixbuf_scale_simple(pixbuf,width,height,GDK_INTERP_BILINEAR);
  }
  return gdk_pixbuf_scale_simple(pixbuf,width,height,GDK_INTERP_NEAREST);
}



void close_current_file(gint file_to_switch_to) {
  // close the current file, and free the file struct and all sub storage
  gchar *com;
  gint index=-1;
  GList *list_index;
  gboolean need_new_blend_file=FALSE;

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
    gtk_widget_set_sensitive (mainw->open_loc, TRUE);
    gtk_widget_set_sensitive (mainw->open_device_menu, TRUE);
    gtk_widget_set_sensitive (mainw->recent_menu, TRUE);
    gtk_widget_set_sensitive (mainw->restore, TRUE);
    gtk_widget_set_sensitive (mainw->toy_tv, TRUE);
    gtk_widget_set_sensitive (mainw->toy_random_frames, TRUE);
    gtk_widget_set_sensitive (mainw->vj_load_set, !mainw->was_set);
    gtk_widget_set_sensitive (mainw->clear_ds, TRUE);
#ifdef HAVE_YUV4MPEG
    gtk_widget_set_sensitive (mainw->open_yuv4m, TRUE);
#endif
  }
  //update the bar text
  if (mainw->current_file>-1) {
    int i;
    
    if (cfile->clip_type!=CLIP_TYPE_GENERATOR&&mainw->current_file!=mainw->scrap_file&&mainw->multitrack==NULL) {
      d_print (g_strdup_printf(_ ("Closed file %s\n"),cfile->file_name));
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_CLOSED,"");
#endif
    }
    
    // resize frame widgets to default
    cfile->hsize=mainw->def_width-H_RESIZE_ADJUST;
    cfile->vsize=mainw->def_height-V_RESIZE_ADJUST;
    
    // this must all be done last...
    if (cfile->menuentry!=NULL) {
      // c.f. on_prevclip_activate
      list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
      do {
	if ((list_index=g_list_previous(list_index))==NULL) list_index=g_list_last (mainw->cliplist);
	index=GPOINTER_TO_INT (list_index->data);
      } while ((mainw->files[index]==NULL||mainw->files[index]->opening||mainw->files[index]->restoring||(index==mainw->scrap_file&&index>-1)||(!mainw->files[index]->frames&&mainw->playing_file>-1))&&index!=mainw->current_file);
      if (index==mainw->current_file) index=-1;
      if (mainw->current_file!=mainw->scrap_file) remove_from_winmenu();
    }

    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->ext_src!=NULL) {
      close_decoder_plugin(cfile,cfile->ext_src);
    }

    if (cfile->frame_index!=NULL) g_free(cfile->frame_index);
    if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);

    if (cfile->clip_type!=CLIP_TYPE_GENERATOR&&!mainw->only_close) {
      com=g_strdup_printf("smogrify close %s",cfile->handle);
      dummyvar=system(com);
      g_free(com); 

      if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
      if (cfile->event_list!=NULL) event_list_free (cfile->event_list);

      if (cfile->layout_map!=NULL) {
	g_list_free_strings(cfile->layout_map);
	g_list_free(cfile->layout_map);
      }

    }

    g_free(cfile);
    cfile=NULL;

    if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) mainw->first_free_file=mainw->current_file;

    if (!mainw->only_close) {
      if (file_to_switch_to>0&&mainw->files[file_to_switch_to]!=NULL) {
	if (mainw->playing_file==-1) {
	  switch_to_file((mainw->current_file=0),file_to_switch_to);
	  d_print("");
	}
	else do_quick_switch(file_to_switch_to);
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

    // can't use set_undoable, as we don't have a cfile
    set_menu_text(mainw->undo,_ ("_Undo"),TRUE);
    set_menu_text(mainw->redo,_ ("_Redo"),TRUE);
    gtk_widget_hide(mainw->redo);
    gtk_widget_show(mainw->undo);
    gtk_widget_set_sensitive(mainw->undo,FALSE);
    
    if (mainw->preview_box!=NULL&&mainw->preview_box->parent!=NULL) {
      g_object_unref(mainw->preview_box);
      gtk_container_remove (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
      mainw->preview_box=NULL;
    }
    if (mainw->play_window!=NULL&&!mainw->fs) {
      g_signal_handlers_block_matched(mainw->play_window,G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_UNBLOCKED,0,0,0,(gpointer)expose_play_window,NULL);
      g_signal_handler_unblock(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=FALSE;
      resize_play_window();
    }

    resize(1);
    load_start_image (0);
    load_end_image (0);
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

    if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->event_list!=NULL&&!mainw->record)) mainw->play_end=INT_MAX;
  }

  if (old_file!=new_file) {
    if (old_file*new_file) mainw->preview_frame=0;
    if (old_file!=-1) {
      if (old_file>0&&mainw->files[old_file]!=NULL&&mainw->files[old_file]->menuentry!=NULL&&(mainw->files[old_file]->clip_type==CLIP_TYPE_DISK||mainw->files[old_file]->clip_type==CLIP_TYPE_FILE)) {
	if (!mainw->files[old_file]->opening) {
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[old_file]->menuentry), NULL);
	}
	else {
	  active_image = gtk_image_new_from_stock ("gtk-no", GTK_ICON_SIZE_MENU);
	  gtk_widget_show (active_image);
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[old_file]->menuentry), active_image);
	}
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

  if ((mainw->playing_file==-1&&prefs->sepwin_type==1&&mainw->sep_win&&new_file>0&&cfile->is_loaded)&&orig_file!=new_file) {
    // if the clip is loaded
    if (mainw->preview_box==NULL) {
      // create the preview box that shows frames...
      make_preview_box();
    }
    // add it the play window...
    if (mainw->preview_box->parent==NULL) {
      gtk_widget_queue_draw(mainw->play_window);
      gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
      if (old_file==-1) {
	g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
	mainw->pw_exp_is_blocked=TRUE;
      }
    }
    // and resize it
    load_preview_image(FALSE);
    resize_play_window();
    gtk_widget_queue_resize(mainw->preview_box);
    while (g_main_context_iteration (NULL,FALSE));
  }
  
  if (new_file>0) {
    GTK_RULER (mainw->hruler)->position=cfile->pointer_time;
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
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (cfile->menuentry), active_image);
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
    
    gtk_widget_set_sensitive (mainw->save, !(cfile->is_untitled)&&capable->has_encoder_plugins);
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
    gtk_widget_set_sensitive (mainw->resample_audio, (cfile->achans>0&&capable->has_sox));
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

  resize(1);

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



void do_quick_switch (gint new_file) {
  // handle clip switching during playback

  gint ovsize=mainw->pheight;
  gint ohsize=mainw->pwidth;
  gboolean osc_block;

  if (mainw->current_file<1||mainw->files[new_file]==NULL) return;

  if (mainw->noswitch||(mainw->record&&!mainw->record_paused&&!(prefs->rec_opts&REC_CLIPS))||mainw->foreign||(mainw->preview&&!mainw->is_rendering&&mainw->multitrack==NULL)) return;

  if (new_file==mainw->current_file) {
    if (!((mainw->fs&&prefs->gui_monitor==prefs->play_monitor)||(mainw->faded&&mainw->double_size)||mainw->multitrack!=NULL)) {
      switch_to_file (mainw->current_file=0, new_file);
      if (mainw->play_window!=NULL&&!mainw->double_size&&!mainw->fs&&mainw->current_file!=-1&&cfile!=NULL&&(ohsize!=cfile->hsize||ovsize!=cfile->vsize)) {
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
  if (cfile!=NULL) g_snprintf(cfile->info_file,256,"%s/%s/.status",prefs->tmpdir,cfile->handle);
  osc_block=mainw->osc_block;
  mainw->osc_block=TRUE;

  if (prefs->audio_player==AUD_PLAYER_JACK&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)&&!mainw->is_rendering) {
#ifdef ENABLE_JACK
  if (mainw->jackd!=NULL) {
      aserver_message_t jack_message;
      while (jack_get_msgq(mainw->jackd)!=NULL);
      if (mainw->jackd->fd>0) {
	  jack_message.command=ASERVER_CMD_FILE_CLOSE;
          jack_message.data=NULL;
	  jack_message.next=NULL;
	  mainw->jackd->msgq=&jack_message;
	  while (jack_get_msgq(mainw->jackd)!=NULL);
     }
     if (cfile!=NULL) cfile->aseek_pos=mainw->jackd->seek_pos;

     mainw->jackd->in_use=TRUE;

     if (mainw->files[new_file]->achans>0) { 
        gint asigned=!(mainw->files[new_file]->signed_endian&AFORM_UNSIGNED);
        gint aendian=!(mainw->files[new_file]->signed_endian&AFORM_BIG_ENDIAN);
        mainw->jackd->num_input_channels=mainw->files[new_file]->achans;
        mainw->jackd->bytes_per_channel=mainw->files[new_file]->asampsize/8;
        if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
          if (!mainw->files[new_file]->play_paused) mainw->jackd->sample_in_rate=mainw->files[new_file]->arate*mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
          else mainw->jackd->sample_in_rate=mainw->files[new_file]->arate*mainw->files[new_file]->freeze_fps/mainw->files[new_file]->fps;
        }
        else mainw->jackd->sample_in_rate=mainw->files[new_file]->arate;
	mainw->jackd->usigned=!asigned;
	mainw->jackd->seek_end=mainw->files[new_file]->afilesize;
	if (mainw->files[new_file]->opening) mainw->jackd->is_opening=TRUE;
	else mainw->jackd->is_opening=FALSE;

	if ((aendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(!aendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->jackd->reverse_endian=TRUE;
	else mainw->jackd->reverse_endian=FALSE;

        if (mainw->ping_pong) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
        else mainw->jackd->loop=AUDIO_LOOP_FORWARD;

	// tell jack server to open audio file and start playing it

	jack_message.command=ASERVER_CMD_FILE_OPEN;
	jack_message.next=NULL;
	if (mainw->files[new_file]->opening) {
          mainw->jackd->is_opening=TRUE;
        }
        jack_message.data=g_strdup_printf("%d",new_file);
        mainw->jackd->msgq=&jack_message;
        mainw->files[new_file]->aseek_pos=jack_audio_seek_bytes(mainw->jackd,mainw->files[new_file]->aseek_pos);
        mainw->jackd->in_use=TRUE;

     if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
       mainw->jackd->is_paused=mainw->files[new_file]->play_paused;
       mainw->jackd->is_silent=FALSE;
     }

       mainw->rec_aclip=new_file;
       mainw->rec_avel=mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
       mainw->rec_aseek=(gdouble)mainw->files[new_file]->aseek_pos/(gdouble)(mainw->files[new_file]->arate*mainw->files[new_file]->achans*mainw->files[new_file]->asampsize/8);
     }
    else {
      mainw->rec_aclip=mainw->current_file;
      mainw->rec_avel=0.;
      mainw->rec_aseek=0.;
     }
    }
#endif
  }

  if (prefs->audio_player==AUD_PLAYER_PULSE&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)&&!mainw->is_rendering) {
#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed!=NULL) {
      aserver_message_t pulse_message;
      while (pulse_get_msgq(mainw->pulsed)!=NULL);
      if (mainw->pulsed->fd>0) {
	  pulse_message.command=ASERVER_CMD_FILE_CLOSE;
          pulse_message.data=NULL;
	  pulse_message.next=NULL;
	  mainw->pulsed->msgq=&pulse_message;
	  while (pulse_get_msgq(mainw->pulsed)!=NULL);
     }
     if (cfile!=NULL) cfile->aseek_pos=mainw->pulsed->seek_pos;

     mainw->pulsed->in_use=TRUE;

     if (mainw->files[new_file]->achans>0) { 
        gint asigned=!(mainw->files[new_file]->signed_endian&AFORM_UNSIGNED);
        gint aendian=!(mainw->files[new_file]->signed_endian&AFORM_BIG_ENDIAN);
        mainw->pulsed->in_achans=mainw->files[new_file]->achans;
        mainw->pulsed->in_asamps=mainw->files[new_file]->asampsize;
        if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
          if (!mainw->files[new_file]->play_paused) mainw->pulsed->in_arate=mainw->files[new_file]->arate*mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
          else mainw->pulsed->in_arate=mainw->files[new_file]->arate*mainw->files[new_file]->freeze_fps/mainw->files[new_file]->fps;
        }
        else mainw->pulsed->in_arate=mainw->files[new_file]->arate;
	mainw->pulsed->usigned=!asigned;
	mainw->pulsed->seek_end=mainw->files[new_file]->afilesize;
	if (mainw->files[new_file]->opening) mainw->pulsed->is_opening=TRUE;
	else mainw->pulsed->is_opening=FALSE;

	if ((aendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(!aendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->pulsed->reverse_endian=TRUE;
	else mainw->pulsed->reverse_endian=FALSE;

        if (mainw->ping_pong) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
        else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;

	// tell pulse server to open audio file and start playing it

	pulse_message.command=ASERVER_CMD_FILE_OPEN;
	pulse_message.next=NULL;
	if (mainw->files[new_file]->opening) {
          mainw->pulsed->is_opening=TRUE;
        }
        pulse_message.data=g_strdup_printf("%d",new_file);
        mainw->pulsed->msgq=&pulse_message;
        mainw->files[new_file]->aseek_pos=pulse_audio_seek_bytes(mainw->pulsed,mainw->files[new_file]->aseek_pos);
        mainw->pulsed->in_use=TRUE;

     if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
       mainw->pulsed->is_paused=mainw->files[new_file]->play_paused;
       mainw->pulsed->is_silent=FALSE;
     }

       mainw->rec_aclip=new_file;
       mainw->rec_avel=mainw->files[new_file]->pb_fps/mainw->files[new_file]->fps;
       mainw->rec_aseek=(gdouble)mainw->files[new_file]->aseek_pos/(gdouble)(mainw->files[new_file]->arate*mainw->files[new_file]->achans*mainw->files[new_file]->asampsize/8);
     }
    else {
      mainw->rec_aclip=mainw->current_file;
      mainw->rec_avel=0.;
      mainw->rec_aseek=0.;
     }
    }
#endif
  }

  mainw->whentostop=NEVER_STOP;

  if (cfile!=NULL&&cfile->clip_type==CLIP_TYPE_GENERATOR&&new_file!=mainw->current_file&&new_file!=mainw->blend_file&&!mainw->is_rendering) {
    if (mainw->files[new_file]->clip_type==CLIP_TYPE_DISK||mainw->files[new_file]->clip_type==CLIP_TYPE_FILE) mainw->pre_src_file=new_file;

    if (rte_window!=NULL) rtew_set_keych(rte_fg_gen_key(),FALSE);
    if (mainw->current_file==mainw->blend_file) mainw->new_blend_file=new_file;
    weed_generator_end (cfile->ext_src);
    if (mainw->current_file==-1) {
      mainw->osc_block=osc_block;
      return;
    }
  }
  
  mainw->switch_during_pb=TRUE;
  mainw->clip_switched=TRUE;

  if (mainw->fs||(mainw->faded&&mainw->double_size)||mainw->multitrack!=NULL) {
    mainw->current_file=new_file;
  }
  else {
    // force update of labels, prevent widgets becoming sensitized
    switch_to_file (mainw->current_file, new_file);
  }

  mainw->play_start=1;
  mainw->play_end=cfile->frames;

  if (mainw->play_window!=NULL&&!mainw->double_size&&!mainw->fs&&(ohsize!=cfile->hsize||ovsize!=cfile->vsize)) {
    // for single size sepwin, we resize frames to fit the window
    mainw->must_resize=TRUE;
    mainw->pheight=ovsize;
    mainw->pwidth=ohsize;
  }
  else if (mainw->multitrack==NULL) mainw->must_resize=FALSE;

  if ((cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE)||(mainw->event_list!=NULL&&!mainw->record)) mainw->play_end=INT_MAX;

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
    load_frame_image (cfile->frameno,cfile->frameno);
    mainw->frame_layer=frame_layer;
  }
  mainw->switch_during_pb=FALSE;
  mainw->osc_block=osc_block;
}





void
resize (gdouble scale) {
  // resize the frame widgets
  // set scale<0. to _force_ the playback frame to expand (for external capture)
#define HSPACE 300
  gdouble oscale=scale;
  gint xsize;
  gint bx,by;
  GdkPixbuf *sepbuf;
  gint hspace=((sepbuf=gtk_image_get_pixbuf (GTK_IMAGE (mainw->sep_image)))!=NULL)?gdk_pixbuf_get_height (sepbuf):0;
  // maximum values
  gint hsize,vsize;
  gint scr_width,scr_height;

  if (!prefs->show_gui) return;
  get_border_size (mainw->LiVES,&bx,&by);

  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
    scr_height=mainw->scr_height;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  hsize=(scr_width-(70+bx))/3;
  vsize=(scr_height-(HSPACE+hspace+by));

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
  }

  else {
    xsize=(scr_width-hsize*-oscale-H_RESIZE_ADJUST)/2;
    if (xsize>0) {
      gtk_widget_set_size_request (mainw->frame1, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->eventbox3, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->frame2, xsize/scale, vsize+V_RESIZE_ADJUST);
      gtk_widget_set_size_request (mainw->eventbox4, xsize/scale, vsize+V_RESIZE_ADJUST);
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

  if (!mainw->foreign&&mainw->playing_file==-1&&mainw->current_file>0&&!cfile->opening) {
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      load_start_image(cfile->start);
      load_end_image(cfile->end);
  }
  
}






