// preferences.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2010
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions dealing with getting/setting user preferences
// TODO - use atom type system for prefs

#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "support.h"
#include "resample.h"
#include "plugins.h"
#include <dlfcn.h>

#define PREFS_TIMEOUT 10000000 // 10 seconds

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

void on_osc_enable_toggled (GtkToggleButton *t1, gpointer t2) {
  if (prefs->osc_udp_started) return;
  gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (t1))||gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (t2)));
}


void
get_pref(const gchar *key, gchar *val, gint maxlen) {
  FILE *valfile;
  gchar *vfile;
  gchar *com;
  gint timeout;

  memset(val,0,maxlen);

  if (mainw->cached_list!=NULL) {
    gchar *prefval=get_val_from_cached_list(key,maxlen);
    g_snprintf(val,maxlen,"%s",prefval);
    g_free(prefval);
    return;
  }

  com=g_strdup_printf("smogrify get_pref %s %d %d",key,getuid(),getpid());
  timeout=PREFS_TIMEOUT/prefs->sleep_time;

  if (system(com)) {
    tempdir_warning();
    g_free(com);
    return;
  }

  vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,getuid(),getpid());
  do {
    if (!(valfile=fopen(vfile,"r"))) {
      if (!(mainw==NULL)) {
	weed_plant_t *frame_layer=mainw->frame_layer;
	mainw->frame_layer=NULL;
	while (g_main_context_iteration(NULL,FALSE));
	mainw->frame_layer=frame_layer;
      }
      g_usleep(prefs->sleep_time);
      timeout--;
    }
  } while (!valfile&&timeout>0);
  if (timeout<=0) {
    tempdir_warning();
  }
  else {
    dummychar=fgets(val,maxlen-1,valfile);
    fclose(valfile);
    unlink(vfile);
  }
  g_free(vfile);
  g_free(com);
}



void get_pref_utf8(const gchar *key, gchar *val, gint maxlen) {
  // get a pref in locale encoding, then convert it to utf8
  gchar *tmp;
  get_pref(key,val,maxlen);
  tmp=g_filename_to_utf8(val,-1,NULL,NULL,NULL);
  g_snprintf(val,maxlen,"%s",tmp);
  g_free(tmp);
}





void
get_pref_default(const gchar *key, gchar *val, gint maxlen) {
  FILE *valfile;
  gchar *vfile;
  gint timeout=PREFS_TIMEOUT/prefs->sleep_time;
  gchar *com=g_strdup_printf("smogrify get_pref_default %s",key);

  memset(val,0,1);

  if (system(com)) {
      tempdir_warning();
      g_free(com);
      return;
  }
  vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,getuid(),getgid());
  do {
    if (!(valfile=fopen(vfile,"r"))) {
      if (!(mainw==NULL)) {
	weed_plant_t *frame_layer=mainw->frame_layer;
	mainw->frame_layer=NULL;
	while (g_main_context_iteration(NULL,FALSE));
	mainw->frame_layer=frame_layer;
      }
      g_usleep(prefs->sleep_time);
      timeout--;
    }
  } while (!valfile&&timeout>0);
  if (timeout<=0) {
    tempdir_warning();
  }
  else {
    dummychar=fgets(val,maxlen,valfile);
    fclose(valfile);
    unlink(vfile);
  }
  g_free(vfile);
  g_free(com);
}


gboolean
get_boolean_pref(const gchar *key) {
  gchar buffer[16];
  get_pref(key,buffer,16);
  if (!strcmp(buffer,"true")) return TRUE;
  return FALSE;
}

gint
get_int_pref(const gchar *key) {
  gchar buffer[64];
  get_pref(key,buffer,64);
  if (strlen(buffer)==0) return 0;
  return atoi(buffer);
}

gdouble
get_double_pref(const gchar *key) {
  gchar buffer[64];
  get_pref(key,buffer,64);
  if (strlen(buffer)==0) return 0.;
  return strtod(buffer,NULL);
}


void
set_pref(const gchar *key, const gchar *value) {
  gchar *com=g_strdup_printf("smogrify set_pref %s \"%s\"",key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_int_pref(const gchar *key, gint value) {
  gchar *com=g_strdup_printf("smogrify set_pref %s %d",key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}

void
set_double_pref(const gchar *key, gdouble value) {
  gchar *com=g_strdup_printf("smogrify set_pref %s %.3f",key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_boolean_pref(const gchar *key, gboolean value) {
  gchar *com;

  if (value) {
    com=g_strdup_printf("smogrify set_pref %s %s",key,"true");
  }
  else {
    com=g_strdup_printf("smogrify set_pref %s %s",key,"false");
  }
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}



void set_vpp(gboolean set_in_prefs) {

  if (strlen (future_prefs->vpp_name)) {
    if (!g_strcasecmp(future_prefs->vpp_name,mainw->none_string)) {
      if (mainw->vpp!=NULL) {
	if (mainw->ext_playback) vid_playback_plugin_exit();
	close_vid_playback_plugin(mainw->vpp);
	mainw->vpp=NULL;
	if (set_in_prefs) set_pref ("vid_playback_plugin","none");
      }
    }
    else {
      _vid_playback_plugin *vpp;
      if ((vpp=open_vid_playback_plugin (future_prefs->vpp_name,TRUE))!=NULL) {
	mainw->vpp=vpp;
	if (set_in_prefs) {
	  set_pref ("vid_playback_plugin",mainw->vpp->name);
	  if (!mainw->ext_playback) do_error_dialog (_ ("\n\nVideo playback plugins are only activated in\nfull screen, separate window (fs) mode\n"));
	}
      }
    }
    if (set_in_prefs) mainw->write_vpp_file=TRUE;
  }

  if (future_prefs->vpp_argv!=NULL&&mainw->vpp!=NULL) {
    mainw->vpp->fwidth=future_prefs->vpp_fwidth;
    mainw->vpp->fheight=future_prefs->vpp_fheight;
    mainw->vpp->palette=future_prefs->vpp_palette;
    mainw->vpp->fixed_fpsd=future_prefs->vpp_fixed_fpsd;
    mainw->vpp->fixed_fps_numer=future_prefs->vpp_fixed_fps_numer;
    mainw->vpp->fixed_fps_denom=future_prefs->vpp_fixed_fps_denom;
    if (mainw->vpp->fixed_fpsd>0.) {
      if (mainw->fixed_fpsd!=-1.||!((*mainw->vpp->set_fps) (mainw->vpp->fixed_fpsd))) {
	do_vpp_fps_error();
	mainw->vpp->fixed_fpsd=-1.;
	mainw->vpp->fixed_fps_numer=0;
      }
    }
    if (!(*mainw->vpp->set_palette)(mainw->vpp->palette)) {
      do_vpp_palette_error();
    }
    mainw->vpp->YUV_clamping=future_prefs->vpp_YUV_clamping;
    
    if (mainw->vpp->set_yuv_palette_clamping!=NULL) (*mainw->vpp->set_yuv_palette_clamping)(mainw->vpp->YUV_clamping);

    mainw->vpp->extra_argc=future_prefs->vpp_argc;
    mainw->vpp->extra_argv=future_prefs->vpp_argv;
    if (set_in_prefs) mainw->write_vpp_file=TRUE;
  }

  memset (future_prefs->vpp_name,0,64);
  future_prefs->vpp_argv=NULL;
}





gboolean 
apply_prefs(gboolean skip_warn) {
  // set current prefs from prefs dialog
  int idx;

  gboolean needs_restart=FALSE;
  const gchar *video_open_command=gtk_entry_get_text(GTK_ENTRY(prefsw->video_open_entry));
  const gchar *audio_play_command=gtk_entry_get_text(GTK_ENTRY(prefsw->audio_command_entry));
  const gchar *def_vid_load_dir=gtk_entry_get_text(GTK_ENTRY(prefsw->vid_load_dir_entry));
  const gchar *def_vid_save_dir=gtk_entry_get_text(GTK_ENTRY(prefsw->vid_save_dir_entry));
  const gchar *def_audio_dir=gtk_entry_get_text(GTK_ENTRY(prefsw->audio_dir_entry));
  const gchar *def_image_dir=gtk_entry_get_text(GTK_ENTRY(prefsw->image_dir_entry));
  const gchar *def_proj_dir=gtk_entry_get_text(GTK_ENTRY(prefsw->proj_dir_entry));
  gchar tmpdir[256];
  const gchar *theme=gtk_entry_get_text(GTK_ENTRY((GTK_COMBO(prefsw->theme_combo))->entry));
  const gchar *audp=gtk_entry_get_text(GTK_ENTRY((GTK_COMBO(prefsw->audp_combo))->entry));
  const gchar *audio_codec=gtk_entry_get_text(GTK_ENTRY(prefsw->acodec_entry));
  const gchar *pb_quality=gtk_entry_get_text(GTK_ENTRY(prefsw->pbq_entry));
  gint pbq=PB_QUALITY_MED;

  gdouble default_fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_def_fps));
  gboolean pause_xmms=FALSE;
  gboolean antialias=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  gboolean stop_screensaver=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  gboolean open_maximised=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->open_maximised_check));
  gboolean fs_maximised=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->fs_max_check));
  gboolean show_recent=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->recent_check));

  gboolean warn_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_fps));
  gboolean warn_save_set=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_set));
  gboolean warn_fsize=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_fsize));
  gboolean warn_mplayer=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_mplayer));
  gboolean warn_save_quality=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_quality));
  gboolean warn_rendered_fx=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_rendered_fx));
  gboolean warn_encoders=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_encoders));
  gboolean warn_duplicate_set=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_dup_set));
  gboolean warn_layout_missing_clips=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_clips));
  gboolean warn_layout_close=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_close));
  gboolean warn_layout_delete=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_delete));
  gboolean warn_layout_shift=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_shift));
  gboolean warn_layout_alter=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_alter));
  gboolean warn_discard_layout=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_discard_layout));
  gboolean warn_after_dvgrab=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_dvgrab));
  gboolean warn_mt_achans=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_achans));
  gboolean warn_mt_no_jack=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_no_jack));
  gboolean warn_yuv4m_open=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_yuv4m_open));

  gboolean warn_layout_adel=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_adel));
  gboolean warn_layout_ashift=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_ashift));
  gboolean warn_layout_aalt=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_aalt));
  gboolean warn_layout_popup=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_popup));
  gboolean warn_mt_backup_space=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_backup_space));
  gboolean warn_after_crash=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_crash));

  gboolean midisynch=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->check_midi));
  gboolean instant_open=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_instant_open));
  gboolean auto_deint=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint));
  gboolean concat_images=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_concat_images));
  gboolean ins_speed=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->ins_speed));
  gboolean show_player_stats=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_show_stats));
  gboolean ext_jpeg=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->jpeg));
  gboolean show_tool=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->show_tool));
  gboolean mouse_scroll=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mouse_scroll));
  gint fsize_to_warn=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_warn_fsize));
  gint dl_bwidth=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_bwidth));
  gint ocp=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_ocp));
  gboolean rec_frames=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rframes));
  gboolean rec_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rfps));
  gboolean rec_effects=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->reffects));
  gboolean rec_clips=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rclips));
  gboolean rec_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->raudio));
  gboolean rec_desk_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rdesk_audio));

  gboolean mt_enter_prompt=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mt_enter_prompt));
  gboolean render_prompt=!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt));
  gint mt_def_width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_width));
  gint mt_def_height=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_height));
  gint mt_def_fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_fps));
  gint mt_def_arate=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
  gint mt_def_achans=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
  gint mt_def_asamps=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
  gint mt_def_signed_endian=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))*AFORM_UNSIGNED+gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))*AFORM_BIG_ENDIAN;
  gint mt_undo_buf=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_undo_buf));
  gboolean mt_exit_render=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render));
  gboolean mt_enable_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton));
  gboolean mt_pertrack_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->pertrack_checkbutton));
  gboolean mt_backaudio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->backaudio_checkbutton));

  gboolean mt_autoback_always=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mt_autoback_always));
  gboolean mt_autoback_never=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mt_autoback_never));
  gint mt_autoback_time=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time));

  gint gui_monitor=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  gint play_monitor=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  gboolean forcesmon=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->forcesmon));
  gboolean startup_ce=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rb_startup_ce));

#ifdef ENABLE_JACK
  gboolean jack_tstart=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_start_tjack));
  gboolean jack_astart=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack));
  gboolean jack_master=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_master));
  gboolean jack_client=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_client));
  gboolean jack_pwp=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_pwp));
  guint jack_opts=(JACK_OPTS_TRANSPORT_CLIENT*jack_client+JACK_OPTS_TRANSPORT_MASTER*jack_master+JACK_OPTS_START_TSERVER*jack_tstart+JACK_OPTS_START_ASERVER*jack_astart+JACK_OPTS_NOPLAY_WHEN_PAUSED*!jack_pwp);
#endif

#ifdef RT_AUDIO
  gboolean audio_follow_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_afollow));
  gboolean audio_follow_clips=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_aclips));
  guint audio_opts=(AUDIO_OPTS_FOLLOW_FPS*audio_follow_fps+AUDIO_OPTS_FOLLOW_CLIPS*audio_follow_clips);
#endif

#ifdef ENABLE_OSC
  gint osc_udp_port=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_osc_udp));
  gboolean osc_start=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->enable_OSC_start));
  gboolean osc_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->enable_OSC));
#endif

  gint rte_keys_virtual=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_rte_keys));

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  gboolean omc_js_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_omc_js));
  const gchar *omc_js_fname=gtk_entry_get_text(GTK_ENTRY(prefsw->omc_js_entry));
#endif


#ifdef OMC_MIDI_IMPL
  gboolean omc_midi_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_omc_midi));
  const gchar *omc_midi_fname=gtk_entry_get_text(GTK_ENTRY(prefsw->omc_midi_entry));
  gint midicr=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_midicr));
  gint midirpt=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_midirpt));

#ifdef ALSA_MIDI
  gboolean use_alsa_midi=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->alsa_midi));
#endif

#endif
#endif

  gchar audio_player[256];
  gint listlen=g_list_length (prefs->acodec_list);
  guint rec_opts=rec_frames*REC_FRAMES+rec_fps*REC_FPS+rec_effects*REC_EFFECTS+rec_clips*REC_CLIPS+rec_audio*REC_AUDIO;
  guint warn_mask;

  unsigned char *new_undo_buf;
  GList *ulist;

  gboolean needs_midi_restart=FALSE;
  gboolean set_omc_dev_opts=FALSE;

  gchar *tmp;

  gchar *cdplay_device=g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(prefsw->cdplay_entry)),-1,NULL,NULL,NULL);

  g_free(resaudw);
  resaudw=NULL;

  for (idx=0;idx<listlen&&strcmp(g_list_nth_data (prefs->acodec_list,idx),audio_codec);idx++);

  if (idx==listlen) future_prefs->encoder.audio_codec=0;
  else future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[idx];

  g_snprintf (tmpdir,256,"%s",(tmp=g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(prefsw->tmpdir_entry)),-1,NULL,NULL,NULL)));
  g_free(tmp);

  if (!strncmp(audp,"mplayer",7)) g_snprintf(audio_player,256,"mplayer");
  else if (!strncmp(audp,"jack",4)) g_snprintf(audio_player,256,"jack");
  else if (!strncmp(audp,"sox",3)) g_snprintf(audio_player,256,"sox");
  else if (!strncmp(audp,"pulse audio",11)) g_snprintf(audio_player,256,"pulse");
  
  if (rec_opts!=prefs->rec_opts) {
    prefs->rec_opts=rec_opts;
    set_int_pref("record_opts",prefs->rec_opts);
  }


  warn_mask=!warn_fps*WARN_MASK_FPS+!warn_save_set*WARN_MASK_SAVE_SET+!warn_save_quality*WARN_MASK_SAVE_QUALITY+!warn_fsize*WARN_MASK_FSIZE+!warn_mplayer*WARN_MASK_NO_MPLAYER+!warn_rendered_fx*WARN_MASK_RENDERED_FX+!warn_encoders*WARN_MASK_NO_ENCODERS+!warn_layout_missing_clips*WARN_MASK_LAYOUT_MISSING_CLIPS+!warn_duplicate_set*WARN_MASK_DUPLICATE_SET+!warn_layout_close*WARN_MASK_LAYOUT_CLOSE_FILE+!warn_layout_delete*WARN_MASK_LAYOUT_DELETE_FRAMES+!warn_layout_shift*WARN_MASK_LAYOUT_SHIFT_FRAMES+!warn_layout_alter*WARN_MASK_LAYOUT_ALTER_FRAMES+!warn_discard_layout*WARN_MASK_EXIT_MT+!warn_after_dvgrab*WARN_MASK_AFTER_DVGRAB+!warn_mt_achans*WARN_MASK_MT_ACHANS+!warn_mt_no_jack*WARN_MASK_MT_NO_JACK+!warn_layout_adel*WARN_MASK_LAYOUT_DELETE_AUDIO+!warn_layout_ashift*WARN_MASK_LAYOUT_SHIFT_AUDIO+!warn_layout_aalt*WARN_MASK_LAYOUT_ALTER_AUDIO+!warn_layout_popup*WARN_MASK_LAYOUT_POPUP+!warn_yuv4m_open*WARN_MASK_OPEN_YUV4M+!warn_mt_backup_space*WARN_MASK_MT_BACKUP_SPACE+!warn_after_crash*WARN_MASK_CLEAN_AFTER_CRASH;

  if (warn_mask!=prefs->warning_mask) {
    prefs->warning_mask=warn_mask;
    set_int_pref("lives_warning_mask",prefs->warning_mask);
  }

  if (fsize_to_warn!=(prefs->warn_file_size)) {
    prefs->warn_file_size=fsize_to_warn;
    set_int_pref("warn_file_size",fsize_to_warn);
  }

  if (dl_bwidth!=(prefs->dl_bandwidth)) {
    prefs->dl_bandwidth=dl_bwidth;
    set_int_pref("dl_bandwidth_K",dl_bwidth);
  }

  if (ocp!=(prefs->ocp)) {
    prefs->ocp=ocp;
    set_int_pref("open_compression_percent",ocp);
  }

  if (show_tool!=(future_prefs->show_tool)) {
    future_prefs->show_tool=prefs->show_tool=show_tool;
    set_boolean_pref("show_toolbar",show_tool);
  }

  if (mouse_scroll!=(prefs->mouse_scroll_clips)) {
    prefs->mouse_scroll_clips=mouse_scroll;
    set_boolean_pref("mouse_scroll_clips",mouse_scroll);
  }


  if (strcmp(tmpdir+strlen(tmpdir)-1,"/")) {
    g_strappend(tmpdir,256,"/");
  }

  get_dirname(tmpdir);

  if (strcmp(prefs->tmpdir,tmpdir)||strcmp (future_prefs->tmpdir,tmpdir)) {
    if (g_file_test (tmpdir, G_FILE_TEST_EXISTS)&&(strlen (tmpdir)<10||strncmp (tmpdir+strlen (tmpdir)-10,"/livestmp/",10))) g_strappend (tmpdir,256,"livestmp/");
    if (strcmp(prefs->tmpdir,tmpdir)||strcmp (future_prefs->tmpdir,tmpdir)) {
      gchar *msg;
      if (!check_dir_access (tmpdir)) {
	tmp=g_filename_to_utf8(tmpdir,-1,NULL,NULL,NULL);
	msg=g_strdup_printf (_ ("Unable to create or write to the new temporary directory.\nYou may need to create it as the root user first, e.g:\n\nmkdir %s; chmod 777 %s\n\nThe directory will not be changed now.\n"),tmp,tmp);
	g_free(tmp);
	do_blocking_error_dialog (msg);
      }
      else {
	g_snprintf(future_prefs->tmpdir,256,"%s",tmpdir);
	msg=g_strdup (_ ("You have chosen to change the temporary directory.\nPlease make sure you have no other copies of LiVES open.\n\nIf you do have other copies of LiVES open, please close them now, *before* pressing OK.\n\nAlternatively, press Cancel to restore the temporary directory to its original setting."));	
	if (!skip_warn) {
	  if (do_warning_dialog(msg)) {
	    mainw->prefs_changed=PREFS_TEMPDIR_CHANGED;
	    needs_restart=TRUE;
	  }
	  else {
	    g_snprintf(future_prefs->tmpdir,256,"%s",prefs->tmpdir);
	  }}}
      g_free (msg);
    }
  }

  // stop xscreensaver
  if (prefs->stop_screensaver!=stop_screensaver) {
    prefs->stop_screensaver=stop_screensaver;
    set_boolean_pref("stop_screensaver",prefs->stop_screensaver);
  }

  // antialias
  if (prefs->antialias!=antialias) {
    prefs->antialias=antialias;
    set_boolean_pref("antialias",antialias);
  }

  // open maximised
  if (prefs->open_maximised!=open_maximised) {
    prefs->open_maximised=open_maximised;
    set_boolean_pref("open_maximised",open_maximised);
  }

  // filesel maximised
  if (prefs->fileselmax!=fs_maximised) {
    prefs->fileselmax=fs_maximised;
    set_boolean_pref("filesel_maximised",fs_maximised);
  }


  // monitors

  if (forcesmon!=prefs->force_single_monitor) {
    prefs->force_single_monitor=forcesmon;
    set_boolean_pref("force_single_monitor",forcesmon);
    get_monitors();
  }

  if (mainw->nmonitors>1) {
    if (gui_monitor!=prefs->gui_monitor||play_monitor!=prefs->play_monitor) {
      gchar *str=g_strdup_printf("%d,%d",gui_monitor,play_monitor);
      set_pref("monitors",str);
      prefs->gui_monitor=gui_monitor;
      prefs->play_monitor=play_monitor;

      if (mainw->multitrack==NULL) {
	if (prefs->gui_monitor!=0) {
	  gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-mainw->LiVES->allocation.width)/2;
	  gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-mainw->LiVES->allocation.height)/2;
	  gtk_window_move(GTK_WINDOW(mainw->LiVES),xcen,ycen);
	  
	}
	if (prefs->open_maximised&&prefs->show_gui) {
	  gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
	}
      }
      else {
	if (prefs->gui_monitor!=0) {
	  gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-mainw->multitrack->window->allocation.width)/2;
	  gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-mainw->multitrack->window->allocation.height)/2;
	  gtk_window_move(GTK_WINDOW(mainw->multitrack->window),xcen,ycen);
	}
	
	
	if ((prefs->gui_monitor!=0||mainw->nmonitors<=1)&&prefs->open_maximised) {
	  gtk_window_maximize (GTK_WINDOW(mainw->multitrack->window));
	}
      }
      if (mainw->play_window!=NULL) resize_play_window();
    }
  }


  // fps stats
  if (prefs->show_player_stats!=show_player_stats) {
    prefs->show_player_stats=show_player_stats;
    set_boolean_pref("show_player_stats",show_player_stats);
  }

  // show recent
  if (prefs->show_recent!=show_recent) {
    prefs->show_recent=show_recent;
    set_boolean_pref("show_recent_files",show_recent);
    if (prefs->show_recent) {
      gtk_widget_show (mainw->recent_menu);
      if (mainw->multitrack!=NULL) gtk_widget_show(mainw->multitrack->recent_menu);
    }
    else {
      gtk_widget_hide (mainw->recent_menu);
      if (mainw->multitrack!=NULL) gtk_widget_hide(mainw->multitrack->recent_menu);
    }
  }

  if (capable->has_xmms) {
    pause_xmms=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->check_xmms_pause));
    // pause xmms during playback
    if (prefs->pause_xmms!=pause_xmms) {
      prefs->pause_xmms=pause_xmms;
      set_boolean_pref("pause_xmms_on_playback",pause_xmms);
    }
  }
  
  // midi synch
  if (prefs->midisynch!=midisynch) {
    prefs->midisynch=midisynch;
    set_boolean_pref("midisynch",midisynch);
  }

  // jpeg/png
  if (strcmp (prefs->image_ext,"jpg")&&ext_jpeg) {
    set_pref("default_image_format","jpeg");
    g_snprintf (prefs->image_ext,16,"jpg");
  }
  else if (!strcmp(prefs->image_ext,"jpg")&&!ext_jpeg) {
    set_pref("default_image_format","png");
    g_snprintf (prefs->image_ext,16,"png");
  }

  // instant open
  if (prefs->instant_open!=instant_open) {
    set_boolean_pref("instant_open",(prefs->instant_open=instant_open));
  }

  // auto deinterlace
  if (prefs->auto_deint!=auto_deint) {
    set_boolean_pref("auto_deinterlace",(prefs->auto_deint=auto_deint));
  }

  // concat images
  if (prefs->concat_images!=concat_images) {
    set_boolean_pref("concat_images",(prefs->concat_images=concat_images));
  }


  // encoder
  if (strcmp(prefs->encoder.name,future_prefs->encoder.name)) {
    g_snprintf(prefs->encoder.name,51,"%s",future_prefs->encoder.name);
    set_pref("encoder",prefs->encoder.name);
    g_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
    prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
  }

  // output format
  if (strcmp(prefs->encoder.of_name,future_prefs->encoder.of_name)) {
    g_snprintf(prefs->encoder.of_name,51,"%s",future_prefs->encoder.of_name);
    g_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
    g_snprintf(prefs->encoder.of_desc,128,"%s",future_prefs->encoder.of_desc);
    prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
    set_pref("output_type",prefs->encoder.of_name);
  }

  if (prefs->encoder.audio_codec!=future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec=future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec<AUDIO_CODEC_UNKNOWN) {
      set_int_pref ("encoder_acodec",prefs->encoder.audio_codec);
    }
  }

  // pb quality
  if (!strcmp(pb_quality,g_list_nth_data(prefsw->pbq_list,0))) pbq=PB_QUALITY_LOW;
  if (!strcmp(pb_quality,g_list_nth_data(prefsw->pbq_list,1))) pbq=PB_QUALITY_MED;
  if (!strcmp(pb_quality,g_list_nth_data(prefsw->pbq_list,2))) pbq=PB_QUALITY_HIGH;

  if (pbq!=prefs->pb_quality) {
    prefs->pb_quality=pbq;
    set_int_pref("pb_quality",pbq);
  }

  // video open command
  if (strcmp(prefs->video_open_command,video_open_command)) {
    g_snprintf(prefs->video_open_command,256,"%s",video_open_command);
    set_pref("video_open_command",prefs->video_open_command);
  }

  //playback plugin
  set_vpp(TRUE);

  // audio play command
  if (strcmp(prefs->audio_play_command,audio_play_command)) {
    g_snprintf(prefs->audio_play_command,256,"%s",audio_play_command);
    set_pref("audio_play_command",prefs->audio_play_command);
  }

  // cd play device
  if (strcmp(prefs->cdplay_device,cdplay_device)) {
    g_snprintf(prefs->cdplay_device,256,"%s",cdplay_device);
    set_pref("cdplay_device",prefs->cdplay_device);
  }

  g_free(cdplay_device);

  // default video load directory
  if (strcmp(prefs->def_vid_load_dir,def_vid_load_dir)) {
      g_snprintf(prefs->def_vid_load_dir,256,"%s/",def_vid_load_dir);
      get_dirname(prefs->def_vid_load_dir);
      set_pref("vid_load_dir",prefs->def_vid_load_dir);
      g_snprintf(mainw->vid_load_dir,256,"%s",prefs->def_vid_load_dir);
  }

  // default video save directory
  if (strcmp(prefs->def_vid_save_dir,def_vid_save_dir)) {
      g_snprintf(prefs->def_vid_save_dir,256,"%s/",def_vid_save_dir);
      get_dirname(prefs->def_vid_save_dir);
      set_pref("vid_save_dir",prefs->def_vid_save_dir);
      g_snprintf(mainw->vid_save_dir,256,"%s",prefs->def_vid_save_dir);
  }

  // default audio directory
  if (strcmp(prefs->def_audio_dir,def_audio_dir)) {
      g_snprintf(prefs->def_audio_dir,256,"%s/",def_audio_dir);
      get_dirname(prefs->def_audio_dir);
      set_pref("audio_dir",prefs->def_audio_dir);
      g_snprintf(mainw->audio_dir,256,"%s",prefs->def_audio_dir);
  }

  // default image directory
  if (strcmp(prefs->def_image_dir,def_image_dir)) {
      g_snprintf(prefs->def_image_dir,256,"%s/",def_image_dir);
      get_dirname(prefs->def_image_dir);
      set_pref("image_dir",prefs->def_image_dir);
      g_snprintf(mainw->image_dir,256,"%s",prefs->def_image_dir);
  }

  // default project directory - for backup and restore
  if (strcmp(prefs->def_proj_dir,def_proj_dir)) {
      g_snprintf(prefs->def_proj_dir,256,"%s/",def_proj_dir);
      get_dirname(prefs->def_proj_dir);
      set_pref("proj_dir",prefs->def_proj_dir);
      g_snprintf(mainw->proj_load_dir,256,"%s",prefs->def_proj_dir);
      g_snprintf(mainw->proj_save_dir,256,"%s",prefs->def_proj_dir);
  }

  // the theme
  if (strcmp(future_prefs->theme,theme)&&!(!strcasecmp(future_prefs->theme,"none")&&!strcmp(theme,mainw->none_string))) {
    if (strcmp(theme,mainw->none_string)) {
      g_snprintf(future_prefs->theme,64,"%s",theme);
    }
    else g_snprintf(future_prefs->theme,64,"none");
    set_pref("gui_theme",future_prefs->theme);
    mainw->prefs_changed|=PREFS_THEME_CHANGED;
  }

  // default fps
  if (prefs->default_fps!=default_fps) {
    prefs->default_fps=default_fps;
    set_double_pref("default_fps",prefs->default_fps);
  }

  // virtual rte keys
  if (prefs->rte_keys_virtual!=rte_keys_virtual) {
    // if we are showing the rte window, we must destroy and recreate it
    refresh_rte_window();

    prefs->rte_keys_virtual=rte_keys_virtual;
    set_int_pref("rte_keys_virtual",prefs->rte_keys_virtual);
  }

  if (ins_speed==prefs->ins_resample) {
    prefs->ins_resample=!ins_speed;
    set_boolean_pref ("insert_resample",prefs->ins_resample);
  }


#ifdef ENABLE_OSC
  if (osc_enable) {
    if (prefs->osc_udp_started&&osc_udp_port!=prefs->osc_udp_port) {
      // port number changed
      lives_osc_end();
      prefs->osc_udp_started=FALSE;
    }
    prefs->osc_udp_port=osc_udp_port;
    // try to start on new port number
    if (!prefs->osc_udp_started) prefs->osc_udp_started=lives_osc_init(prefs->osc_udp_port);
  }
  else {
    if (prefs->osc_udp_started) {
      lives_osc_end();
      prefs->osc_udp_started=FALSE;
    }
  }
  if (osc_start) {
    if (!future_prefs->osc_start) {
      set_boolean_pref("osc_start",TRUE);
      future_prefs->osc_start=TRUE;
    }
  }
  else {
    if (future_prefs->osc_start) {
      set_boolean_pref("osc_start",FALSE);
      future_prefs->osc_start=FALSE;
    }
  }
  if (prefs->osc_udp_port!=osc_udp_port) {
    prefs->osc_udp_port=osc_udp_port;
    set_int_pref ("osc_port",osc_udp_port);
  }
#endif

#ifdef RT_AUDIO
  if (prefs->audio_opts!=audio_opts) {
    prefs->audio_opts=audio_opts;
    set_int_pref("audio_opts",audio_opts);

#ifdef ENABLE_JACK
    if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->loop_cont) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->loop_cont) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
    }
#endif

  }

  if (rec_desk_audio!=prefs->rec_desktop_audio) {
    prefs->rec_desktop_audio=rec_desk_audio;
    set_boolean_pref("rec_desktop_audio",rec_desk_audio);
  }
#endif

  if (prefs->audio_player==AUD_PLAYER_JACK&&!capable->has_jackd) {
    do_error_dialog(_("\nUnable to switch audio players to jack - jackd must be installed first.\nSee http://jackaudio.org\n"));
  }
  else {
    if (prefs->audio_player==AUD_PLAYER_JACK&&strcmp(audio_player,"jack")) {
      do_error_dialog(_("\nSwitching audio players requires restart (jackd must not be running)\n"));
    }

    // switch to sox
    if (!(strcmp(audio_player,"sox"))&&prefs->audio_player!=AUD_PLAYER_SOX) {
      switch_aud_to_sox();
    }
    
    // switch to jack
    else if (!(strcmp(audio_player,"jack"))&&prefs->audio_player!=AUD_PLAYER_JACK) {
      // may fail
      if (!switch_aud_to_jack()) do_jack_noopen_warn();
    }
    
    // switch to mplayer audio
    else if (!(strcmp (audio_player,"mplayer"))&&prefs->audio_player!=AUD_PLAYER_MPLAYER) {
      switch_aud_to_mplayer();
    }

    // switch to pulse audio
    else if (!(strcmp (audio_player,"pulse"))&&prefs->audio_player!=AUD_PLAYER_PULSE) {
      if (!capable->has_pulse_audio) {
	do_error_dialog(_("\nUnable to switch audio players to pulse audio\npulseaudio must be installed first.\nSee http://www.pulseaudio.org\n"));
      }
      else switch_aud_to_pulse();
    }


  }

#ifdef ENABLE_JACK
  if (future_prefs->jack_opts!=jack_opts) {
    set_int_pref("jack_opts",jack_opts);
    future_prefs->jack_opts=prefs->jack_opts=jack_opts;
  }
#endif



#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  if (strcmp(omc_js_fname,prefs->omc_js_fname)) {
    g_snprintf(prefs->omc_js_fname,256,"%s",omc_js_fname);
    set_pref("omc_js_fname",omc_js_fname);
  }
  if (omc_js_enable!=((prefs->omc_dev_opts&OMC_DEV_JS)/OMC_DEV_JS)) {
    if (omc_js_enable) {
      prefs->omc_dev_opts|=OMC_DEV_JS;
      js_open();
    }
    else {
      prefs->omc_dev_opts^=OMC_DEV_JS;
      js_close();
    }
    set_omc_dev_opts=TRUE;
  }
#endif


#ifdef OMC_MIDI_IMPL
  if (strcmp(omc_midi_fname,prefs->omc_midi_fname)) {
    g_snprintf(prefs->omc_midi_fname,256,"%s",omc_midi_fname);
    set_pref("omc_midi_fname",omc_midi_fname);
  }

  if (midicr!=prefs->midi_check_rate) {
    prefs->midi_check_rate=midicr;
    set_int_pref("midi_check_rate",prefs->midi_check_rate);
  }

  if (midirpt!=prefs->midi_rpt) {
    prefs->midi_rpt=midirpt;
    set_int_pref("midi_rpt",prefs->midi_rpt);
  }

  if (omc_midi_enable!=((prefs->omc_dev_opts&OMC_DEV_MIDI)/OMC_DEV_MIDI)) {
    if (omc_midi_enable) {
      prefs->omc_dev_opts|=OMC_DEV_MIDI;
      needs_midi_restart=TRUE;
    }
    else {
      prefs->omc_dev_opts^=OMC_DEV_MIDI;
      midi_close();
    }
    set_omc_dev_opts=TRUE;
  }

#ifdef ALSA_MIDI
  if (use_alsa_midi==((prefs->omc_dev_opts&OMC_DEV_FORCE_RAW_MIDI)/OMC_DEV_FORCE_RAW_MIDI)) {
    if (!needs_midi_restart) {
      needs_midi_restart=(mainw->ext_cntl[EXT_CNTL_MIDI]);
      if (needs_midi_restart) midi_close();
    }

    if (!use_alsa_midi) {
      prefs->omc_dev_opts|=OMC_DEV_FORCE_RAW_MIDI;
      prefs->use_alsa_midi=TRUE;
    }
    else {
      prefs->omc_dev_opts^=OMC_DEV_FORCE_RAW_MIDI;
      prefs->use_alsa_midi=FALSE;
    }
    set_omc_dev_opts=TRUE;
  }
#endif

  if (needs_midi_restart) midi_open();

#endif
  if (set_omc_dev_opts) set_int_pref("omc_dev_opts",prefs->omc_dev_opts);
#endif

  if (mt_enter_prompt!=prefs->mt_enter_prompt) {
    prefs->mt_enter_prompt=mt_enter_prompt;
    set_boolean_pref("mt_enter_prompt",mt_enter_prompt);
  }

  if (mt_exit_render!=prefs->mt_exit_render) {
    prefs->mt_exit_render=mt_exit_render;
    set_boolean_pref("mt_exit_render",mt_exit_render);
  }

  if (render_prompt!=prefs->render_prompt) {
    prefs->render_prompt=render_prompt;
    set_boolean_pref("render_prompt",render_prompt);
  }

  if (mt_pertrack_audio!=prefs->mt_pertrack_audio) {
    prefs->mt_pertrack_audio=mt_pertrack_audio;
    set_boolean_pref("mt_pertrack_audio",mt_pertrack_audio);
  }

  if (mt_backaudio!=prefs->mt_backaudio) {
    prefs->mt_backaudio=mt_backaudio;
    set_int_pref("mt_backaudio",mt_backaudio);
  }

  if (mt_def_width!=prefs->mt_def_width) {
    prefs->mt_def_width=mt_def_width;
    set_int_pref("mt_def_width",mt_def_width);
  }
  if (mt_def_height!=prefs->mt_def_height) {
    prefs->mt_def_height=mt_def_height;
    set_int_pref("mt_def_height",mt_def_height);
  }
  if (mt_def_fps!=prefs->mt_def_fps) {
    prefs->mt_def_fps=mt_def_fps;
    set_double_pref("mt_def_fps",mt_def_fps);
  }
  if (!mt_enable_audio) mt_def_achans=0;
  if (mt_def_achans!=prefs->mt_def_achans) {
    prefs->mt_def_achans=mt_def_achans;
    set_int_pref("mt_def_achans",mt_def_achans);
  }
  if (mt_def_asamps!=prefs->mt_def_asamps) {
    prefs->mt_def_asamps=mt_def_asamps;
    set_int_pref("mt_def_asamps",mt_def_asamps);
  }
  if (mt_def_arate!=prefs->mt_def_arate) {
    prefs->mt_def_arate=mt_def_arate;
    set_int_pref("mt_def_arate",mt_def_arate);
  }
  if (mt_def_signed_endian!=prefs->mt_def_signed_endian) {
    prefs->mt_def_signed_endian=mt_def_signed_endian;
    set_int_pref("mt_def_signed_endian",mt_def_signed_endian);
  }
  
  if (mt_undo_buf!=prefs->mt_undo_buf) {
    if ((new_undo_buf=g_try_malloc(mt_undo_buf*1024*1024))==NULL) {
      do_mt_set_mem_error(mainw->multitrack!=NULL,skip_warn);
    }
    else {
      if (mainw->multitrack!=NULL) {
	if (mainw->multitrack->undo_mem!=NULL) {
	  if (mt_undo_buf<prefs->mt_undo_buf) {
	    ssize_t space_needed=mainw->multitrack->undo_buffer_used-(size_t)(mt_undo_buf*1024*1024);
	    if (space_needed>0) make_backup_space(mainw->multitrack,space_needed);
	    memcpy(new_undo_buf,mainw->multitrack->undo_mem,mt_undo_buf*1024*1024);
	  }
	  else memcpy(new_undo_buf,mainw->multitrack->undo_mem,prefs->mt_undo_buf*1024*1024);
	  ulist=mainw->multitrack->undos;
	  while (ulist!=NULL) {
	    ulist->data=new_undo_buf+((unsigned char *)ulist->data-mainw->multitrack->undo_mem);
	    ulist=ulist->next;
	  }
	  g_free(mainw->multitrack->undo_mem);
	  mainw->multitrack->undo_mem=new_undo_buf;
	}
	else {
	  mainw->multitrack->undo_mem=g_try_malloc(mt_undo_buf*1024*1024);
	  if (mainw->multitrack->undo_mem==NULL) {
	    do_mt_set_mem_error(TRUE,skip_warn);
	  }
	  else {
	    mainw->multitrack->undo_buffer_used=0;
	    mainw->multitrack->undos=NULL;
	    mainw->multitrack->undo_offset=0;
	  }
	}
      }
      prefs->mt_undo_buf=mt_undo_buf;
      set_int_pref("mt_undo_buf",mt_undo_buf);
    }
  }

  if (mt_autoback_always) mt_autoback_time=0;
  else if (mt_autoback_never) mt_autoback_time=-1;

  if (mt_autoback_time!=prefs->mt_auto_back) {
    prefs->mt_auto_back=mt_autoback_time;
    set_int_pref("mt_auto_back",mt_autoback_time);
  }

  if (startup_ce&&future_prefs->startup_interface!=STARTUP_CE) {
    future_prefs->startup_interface=STARTUP_CE;
    set_int_pref("startup_interface",STARTUP_CE);
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) write_backup_layout_numbering(mainw->multitrack);
  }
  else if (!startup_ce&&future_prefs->startup_interface!=STARTUP_MT) {
    future_prefs->startup_interface=STARTUP_MT;
    set_int_pref("startup_interface",STARTUP_MT);
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) write_backup_layout_numbering(mainw->multitrack);
  }

  return needs_restart;
}




void
save_future_prefs(void) {
  // save future prefs on exit, if they have changed

  // show_recent is a special case, future prefs has our original value
  if (!prefs->show_recent&&future_prefs->show_recent) {
    set_pref("recent1","");
    set_pref("recent2","");
    set_pref("recent3","");
    set_pref("recent4","");
  }

  if (strncmp(future_prefs->tmpdir,"NULL",4)) {
    set_pref("tempdir",future_prefs->tmpdir);
  }
  if (prefs->show_tool!=future_prefs->show_tool) {
    set_boolean_pref ("show_toolbar",future_prefs->show_tool);
  }


}


void 
rdet_acodec_changed (GtkEntry *acodec_entry, gpointer user_data) {
  gint listlen=g_list_length (prefs->acodec_list);
  int idx;
  const gchar *audio_codec=gtk_entry_get_text(acodec_entry);
  if (!strcmp(audio_codec,mainw->any_string)) return;

  for (idx=0;idx<listlen&&strcmp(g_list_nth_data (prefs->acodec_list,idx),audio_codec);idx++);

  if (idx==listlen) future_prefs->encoder.audio_codec=0;
  else future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[idx];

  if (prefs->encoder.audio_codec!=future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec=future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec<AUDIO_CODEC_UNKNOWN) {
      set_int_pref ("encoder_acodec",prefs->encoder.audio_codec);
    }
  }
}





void set_acodec_list_from_allowed (_prefsw *prefsw, render_details *rdet) {
  // could be done better, but no time...
  // get settings for current format


  int count=0,idx;
  gboolean is_allowed=FALSE;
  
  if (prefs->acodec_list!=NULL) {
    g_list_free (prefs->acodec_list);
    prefs->acodec_list=NULL;
  }

  if (future_prefs->encoder.of_allowed_acodecs==0) {
    prefs->acodec_list=g_list_append (prefs->acodec_list, g_strdup(mainw->none_string));
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0]=AUDIO_CODEC_NONE;

    if (prefsw!=NULL) {
      combo_set_popdown_strings (GTK_COMBO (prefsw->acodec_combo), prefs->acodec_list);
      gtk_entry_set_text (GTK_ENTRY (prefsw->acodec_entry),mainw->none_string);
    }
    if (rdet!=NULL) {
      combo_set_popdown_strings (GTK_COMBO (rdet->acodec_combo), prefs->acodec_list);
      gtk_entry_set_text (GTK_ENTRY (rdet->acodec_entry),mainw->none_string);
    }
    return;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_PCM)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup(_ ("PCM (highest quality; largest files)")));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_PCM;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_PCM) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_MP3)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("mp3"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_MP3;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_MP3) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_MP2)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("mp2"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_MP2;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_MP2) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_VORBIS)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("vorbis"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_VORBIS;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_VORBIS) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_AC3)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("ac3"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_AC3;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_AC3) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_AAC)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("aac"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_AAC;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_AAC) is_allowed=TRUE;
  }
  if (future_prefs->encoder.of_allowed_acodecs&(1<<AUDIO_CODEC_AMR_NB)) {
    prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup("amr_nb"));
    prefs->acodec_list_to_format[count++]=AUDIO_CODEC_AMR_NB;
    if (future_prefs->encoder.audio_codec==AUDIO_CODEC_AMR_NB) is_allowed=TRUE;
  }
  if (prefsw!=NULL) combo_set_popdown_strings (GTK_COMBO (prefsw->acodec_combo), prefs->acodec_list);
  if (rdet!=NULL) combo_set_popdown_strings (GTK_COMBO (rdet->acodec_combo), prefs->acodec_list);
  if (!is_allowed) {
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0];
  }

  for (idx=0;idx<g_list_length (prefs->acodec_list);idx++) {
    if (prefs->acodec_list_to_format[idx]==future_prefs->encoder.audio_codec) {
      if (prefsw!=NULL) gtk_entry_set_text (GTK_ENTRY (prefsw->acodec_entry),g_list_nth_data (prefs->acodec_list,idx));
      if (rdet!=NULL) gtk_entry_set_text (GTK_ENTRY (rdet->acodec_entry),g_list_nth_data (prefs->acodec_list,idx));
      break;
    }
  }
}


void after_vpp_changed (GtkEntry *vpp_entry, gpointer advbutton) {
  int i;

  if (!g_strcasecmp(gtk_entry_get_text(vpp_entry),mainw->none_string)) {
    gtk_widget_set_sensitive (GTK_WIDGET(advbutton), FALSE);
  }
  else gtk_widget_set_sensitive (GTK_WIDGET(advbutton), TRUE);

  g_snprintf (future_prefs->vpp_name,64,"%s",gtk_entry_get_text(GTK_ENTRY(vpp_entry)));

  if (future_prefs->vpp_argv!=NULL) {
    for (i=0;future_prefs->vpp_argv[i]!=NULL;g_free(future_prefs->vpp_argv[i++]));
    g_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv=NULL;
  }
  future_prefs->vpp_argc=0;
}



static void on_forcesmon_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  gtk_widget_set_sensitive(prefsw->spinbutton_gmoni,!gtk_toggle_button_get_active(tbutton));
  gtk_widget_set_sensitive(prefsw->spinbutton_pmoni,!gtk_toggle_button_get_active(tbutton));
}


static void on_mtbackevery_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  _prefsw *xprefsw;

  if (user_data!=NULL) xprefsw=(_prefsw *)user_data;
  else xprefsw=prefsw;

  gtk_widget_set_sensitive(xprefsw->spinbutton_mt_ab_time,gtk_toggle_button_get_active(tbutton));

}

#ifdef ALSA_MIDI
static void on_alsa_midi_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  _prefsw *xprefsw;

  if (user_data!=NULL) xprefsw=(_prefsw *)user_data;
  else xprefsw=prefsw;

  gtk_widget_set_sensitive(xprefsw->button_midid,!gtk_toggle_button_get_active(tbutton));
  gtk_widget_set_sensitive(xprefsw->omc_midi_entry,!gtk_toggle_button_get_active(tbutton));
  gtk_widget_set_sensitive(xprefsw->spinbutton_midicr,!gtk_toggle_button_get_active(tbutton));
  gtk_widget_set_sensitive(xprefsw->spinbutton_midirpt,!gtk_toggle_button_get_active(tbutton));
}
#endif

_prefsw *create_prefs_dialog (void) {
  GtkWidget *dialog_vbox8;
  GtkWidget *vbox7;
  GtkWidget *ins_resample;
  GtkWidget *hbox100;
  GtkWidget *hbox;
  GtkObject *spinbutton_adj;
  GtkObject *spinbutton_warn_fsize_adj;
  GtkObject *spinbutton_bwidth_adj;
  GtkWidget *label157;
  GtkWidget *hseparator;
  GtkWidget *hbox8;
  GtkWidget *pref_gui;
  GtkWidget *pref_multitrack;
  GtkWidget *vbox9;
  GtkWidget *vbox109;
  GtkWidget *vbox99;
  GtkWidget *vbox69;
  GtkWidget *vbox98;
  GtkWidget *frame4;
  GtkWidget *hbox109;
  GtkWidget *hbox99;
  GtkWidget *hbox19;
  GtkWidget *label133;
  GtkWidget *label134;
  GtkWidget *label31;
  GtkWidget *frame5;
  GtkWidget *hbox10;
  GtkWidget *label35;
  GtkWidget *label36;
  GtkWidget *label32;
  GtkWidget *label25;
  GtkWidget *label84;
  GtkWidget *label97;
  GtkWidget *label88;
  GtkWidget *vbox18;
  GtkWidget *vbox;
  GtkWidget *vbox12;
  GtkWidget *hbox11;
  GtkWidget *hbox94;
  GtkWidget *hbox101;
  GtkWidget *label37;
  GtkWidget *hseparator8;
  GtkWidget *hbox115;
  GtkWidget *label56;
  GtkWidget *label94;
  GtkWidget *hbox93;
  GtkWidget *table4;
  GtkWidget *label39;
  GtkWidget *label99;
  GtkWidget *label98;
  GtkWidget *label100;
  GtkWidget *label40;
  GtkWidget *label41;
  GtkWidget *label42;
  GtkWidget *label52;
  GtkWidget *label43;
  GtkWidget *label27;
  GtkWidget *vbox14;
  GtkWidget *hbox13;
  GtkWidget *scrollw;
  GtkWidget *label44;
  GtkObject *spinbutton_def_fps_adj;
  GtkWidget *label29;
  GtkWidget *dialog_action_area8;
  GtkWidget *okbutton5;
  GtkWidget *applybutton;
  GtkWidget *dirbutton1;
  GtkWidget *dirbutton2;
  GtkWidget *dirbutton3;
  GtkWidget *dirbutton4;
  GtkWidget *dirbutton5;
  GtkWidget *dirbutton6;
  GtkWidget *dirimage1;
  GtkWidget *dirimage2;
  GtkWidget *dirimage3;
  GtkWidget *dirimage4;
  GtkWidget *dirimage5;
  GtkWidget *dirimage6;
  GtkWidget *hseparator13;
  GtkWidget *hbox31;
  GtkWidget *label126;
  GtkWidget *pp_combo;
  GtkWidget *combo_entry4;
  GtkWidget *png;
  GtkWidget *frame;
  GtkWidget *label158;
  GtkWidget *label159;
  GtkWidget *hbox116;
  GtkWidget *vbox77;
  GtkWidget *mt_enter_defs;
  GtkObject *spinbutton_ocp_adj;
  GtkWidget *advbutton;
  GtkWidget *raw_midi_button;

  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *eventbox;

#ifdef ENABLE_OSC
  GtkWidget *buttond;
#ifdef OMC_JS_IMPL
  GtkWidget *vbox2;
#else
#ifdef OMC_MIDI_IMPL
  GtkWidget *vbox2;
#endif
#endif
#endif

  // radio button groups
  GSList *rb_group = NULL;
  GSList *jpeg_png = NULL;
  GSList *mt_enter_prompt = NULL;
  GSList *rb_group2 = NULL;
  GSList *alsa_midi_group = NULL;
  GSList *autoback_group = NULL;
  GSList *st_interface_group = NULL;

  // drop down lists
  GList *themes = NULL;
  GList *ofmt = NULL;
  GList *ofmt_all = NULL;
  GList *audp = NULL;
  GList *encoders = NULL;
  GList *vid_playback_plugins = NULL;
  
  gchar **array,*tmp;

  int i;

  gint pageno=0;

  gint nmonitors=mainw->nmonitors;
  gboolean pfsm;

  gchar *theme;

  gboolean has_ap_rec=FALSE;

  _prefsw *prefsw=(_prefsw*)(g_malloc(sizeof(_prefsw)));

  prefsw->prefs_dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->prefs_dialog), 10);
  gtk_window_set_title (GTK_WINDOW (prefsw->prefs_dialog), _("LiVES: - Preferences"));
  gtk_window_set_position (GTK_WINDOW (prefsw->prefs_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (prefsw->prefs_dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (prefsw->prefs_dialog), 660, 440);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(prefsw->prefs_dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(prefsw->prefs_dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  gtk_widget_modify_bg(prefsw->prefs_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_fg(prefsw->prefs_dialog, GTK_STATE_NORMAL, &palette->normal_fore);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(prefsw->prefs_dialog),FALSE);
  }

  dialog_vbox8 = GTK_DIALOG (prefsw->prefs_dialog)->vbox;
  gtk_widget_show (dialog_vbox8);

  prefsw->prefs_notebook = gtk_notebook_new ();
  gtk_widget_show (prefsw->prefs_notebook);
  gtk_box_pack_start (GTK_BOX (dialog_vbox8), prefsw->prefs_notebook, TRUE, TRUE, 0);
  gtk_notebook_set_tab_hborder (GTK_NOTEBOOK (prefsw->prefs_notebook), 10);
  gtk_notebook_popup_enable(GTK_NOTEBOOK(prefsw->prefs_notebook));
  gtk_notebook_set_scrollable(GTK_NOTEBOOK(prefsw->prefs_notebook),TRUE);


  // multitrack

  vbox77 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox77);
  gtk_container_set_border_width (GTK_CONTAINER (vbox77), 10);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox77), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("When entering Multitrack mode:"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 16);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 16);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  prefsw->mt_enter_prompt = gtk_radio_button_new_with_mnemonic (NULL, _("_Prompt me for width, height, fps and audio settings"));
  gtk_widget_show (prefsw->mt_enter_prompt);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->mt_enter_prompt), 8);
  gtk_box_pack_start (GTK_BOX (vbox77), prefsw->mt_enter_prompt, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->mt_enter_prompt), mt_enter_prompt);
  mt_enter_prompt = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_enter_prompt));

  mt_enter_defs = gtk_radio_button_new_with_mnemonic (mt_enter_prompt, _("_Always use the following values:"));
  gtk_widget_show (mt_enter_defs);
  gtk_container_set_border_width (GTK_CONTAINER (mt_enter_defs), 8);
  gtk_box_pack_start (GTK_BOX (vbox77), mt_enter_defs, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mt_enter_defs),!prefs->mt_enter_prompt);

  prefsw->checkbutton_render_prompt = gtk_check_button_new_with_mnemonic (_("Use these same _values for rendering a new clip"));
  gtk_widget_show (prefsw->checkbutton_render_prompt);
  gtk_box_pack_start (GTK_BOX (vbox77), prefsw->checkbutton_render_prompt, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->checkbutton_render_prompt), 12);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_render_prompt), !prefs->render_prompt);

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);

  gtk_box_pack_start (GTK_BOX (vbox77), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  label = gtk_label_new (_("Video"));
  gtk_widget_show (label);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("_Width           "));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  spinbutton_adj = gtk_adjustment_new (prefs->mt_def_width, 0, 8192, 1, 10, 0);
  prefsw->spinbutton_mt_def_width = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_mt_def_width);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_width, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_def_width);

  label = gtk_label_new_with_mnemonic (_("          _Height      "));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  spinbutton_adj = gtk_adjustment_new (prefs->mt_def_height, 0, 8192, 1, 10, 0);
  prefsw->spinbutton_mt_def_height = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_mt_def_height);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_height, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_def_height);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("          _FPS      "));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  spinbutton_adj = gtk_adjustment_new (prefs->mt_def_fps, 1, FPS_MAX, .1, 1, 0);
  prefsw->spinbutton_mt_def_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gtk_widget_show (prefsw->spinbutton_mt_def_fps);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_fps, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_def_fps);

  resaudw=create_resaudw(4,NULL,vbox77);

  prefsw->backaudio_checkbutton = gtk_check_button_new ();
  gtk_widget_show(prefsw->backaudio_checkbutton);
  eventbox=gtk_event_box_new();
  gtk_widget_show(eventbox);
  label=gtk_label_new_with_mnemonic (_("Enable backing audio track"));
  gtk_widget_show(label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->backaudio_checkbutton);
  
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    prefsw->backaudio_checkbutton);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start (GTK_BOX (vbox77), hbox, FALSE, FALSE, 10);
  
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->backaudio_checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (prefsw->backaudio_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->backaudio_checkbutton), prefs->mt_backaudio>0);

  gtk_widget_set_sensitive(prefsw->backaudio_checkbutton,gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  prefsw->pertrack_checkbutton = gtk_check_button_new ();
  gtk_widget_show(prefsw->pertrack_checkbutton);
  eventbox=gtk_event_box_new();
  gtk_widget_show(eventbox);
  label=gtk_label_new_with_mnemonic (_("Audio track per video track"));
  gtk_widget_show(label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->pertrack_checkbutton);
  
  gtk_widget_set_sensitive(prefsw->pertrack_checkbutton,gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    prefsw->pertrack_checkbutton);

  gtk_box_pack_end (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  gtk_box_pack_end (GTK_BOX (hbox), prefsw->pertrack_checkbutton, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (prefsw->pertrack_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->pertrack_checkbutton), prefs->mt_pertrack_audio);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (vbox77), hseparator, FALSE, TRUE, 10);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox77), hbox, TRUE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("    _Undo buffer size (MB)    "));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  spinbutton_adj = gtk_adjustment_new (prefs->mt_undo_buf, 0, G_MAXSIZE/(1024*1024), 1, 1, 0);
  prefsw->spinbutton_mt_undo_buf = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_mt_undo_buf);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_undo_buf, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_undo_buf);


  prefsw->checkbutton_mt_exit_render = gtk_check_button_new_with_mnemonic (_("_Exit multitrack mode after rendering"));
  gtk_widget_show (prefsw->checkbutton_mt_exit_render);
  gtk_box_pack_start (GTK_BOX (vbox77), prefsw->checkbutton_mt_exit_render, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->checkbutton_mt_exit_render), 22);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);



  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox77), hbox, TRUE, FALSE, 0);

  label = gtk_label_new (_("Auto backup layouts"));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);


  prefsw->mt_autoback_every = gtk_radio_button_new_with_mnemonic (NULL, _("_Every"));
  gtk_widget_show (prefsw->mt_autoback_every);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->mt_autoback_every), 8);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->mt_autoback_every, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_every), autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_every));


  spinbutton_adj = gtk_adjustment_new (30, 10, 1800, 1, 10, 0);
  prefsw->spinbutton_mt_ab_time = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_mt_ab_time);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_ab_time, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_ab_time);


  label = gtk_label_new_with_mnemonic (_("seconds"));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 10);

  add_fill_to_box(GTK_BOX(hbox));


  prefsw->mt_autoback_always = gtk_radio_button_new_with_mnemonic (NULL, _("After every _change"));
  gtk_widget_show (prefsw->mt_autoback_always);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->mt_autoback_always), 18);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->mt_autoback_always, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_always), autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_always));


  prefsw->mt_autoback_never = gtk_radio_button_new_with_mnemonic (NULL, _("_Never"));
  gtk_widget_show (prefsw->mt_autoback_never);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->mt_autoback_never), 18);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->mt_autoback_never, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_never), autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_never));

  g_signal_connect (GTK_OBJECT (prefsw->mt_autoback_every), "toggled",
		    G_CALLBACK (on_mtbackevery_toggled),
		    prefsw);

  if (prefs->mt_auto_back==0) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->mt_autoback_always),TRUE);
  }
  else if (prefs->mt_auto_back==-1) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->mt_autoback_never),TRUE);
  }
  else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->mt_autoback_every),TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time),prefs->mt_auto_back);
  }

  pref_multitrack = gtk_label_new (_("Multitrack/Render"));
  gtk_widget_show (pref_multitrack);
  gtk_label_set_justify (GTK_LABEL (pref_multitrack), GTK_JUSTIFY_LEFT);

  if (mainw->multitrack!=NULL) {
    gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox77);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), pref_multitrack);
  }

  // gui

  vbox7 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox7);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox7);
  gtk_container_set_border_width (GTK_CONTAINER (vbox7), 20);



  hbox8 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox8);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox8, TRUE, TRUE, 0);

  prefsw->fs_max_check = gtk_check_button_new_with_mnemonic (_("Open file selection maximised"));
  gtk_widget_show (prefsw->fs_max_check);
  gtk_box_pack_start (GTK_BOX (hbox8), prefsw->fs_max_check, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->fs_max_check), 22);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->fs_max_check), prefs->fileselmax);

  prefsw->recent_check = gtk_check_button_new_with_mnemonic (_("Show recent files in the File menu"));
  gtk_widget_show (prefsw->recent_check);
  gtk_box_pack_start (GTK_BOX (hbox8), prefsw->recent_check, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->recent_check), 22);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->recent_check), prefs->show_recent);



  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox, TRUE, TRUE, 0);

  prefsw->stop_screensaver_check = gtk_check_button_new_with_mnemonic (_("Stop screensaver on playback    "));
  gtk_widget_show (prefsw->stop_screensaver_check);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->stop_screensaver_check, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->stop_screensaver_check), 22);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->stop_screensaver_check), prefs->stop_screensaver);

  prefsw->open_maximised_check = gtk_check_button_new_with_mnemonic (_("Open main window maximised"));
  gtk_widget_show (prefsw->open_maximised_check);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->open_maximised_check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->open_maximised_check), prefs->open_maximised);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox, TRUE, TRUE, 0);

  prefsw->show_tool = gtk_check_button_new_with_mnemonic (_("Show toolbar when background is blanked"));
  gtk_widget_show (prefsw->show_tool);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->show_tool, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->show_tool), 22);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->show_tool), future_prefs->show_tool);


  prefsw->mouse_scroll = gtk_check_button_new_with_mnemonic (_("Allow mouse wheel to switch clips"));
  gtk_widget_show (prefsw->mouse_scroll);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->mouse_scroll, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->mouse_scroll), 22);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->mouse_scroll), prefs->mouse_scroll_clips);




  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox, TRUE, FALSE, 0);

  label = gtk_label_new (_("Startup mode:"));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  add_fill_to_box(GTK_BOX(hbox));

  prefsw->rb_startup_ce = gtk_radio_button_new_with_mnemonic (NULL, _("_Clip editor"));
  gtk_widget_show (prefsw->rb_startup_ce);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rb_startup_ce), 8);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->rb_startup_ce, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->rb_startup_ce), st_interface_group);
  st_interface_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->rb_startup_ce));

  add_fill_to_box(GTK_BOX(hbox));

  prefsw->rb_startup_mt = gtk_radio_button_new_with_mnemonic (NULL, _("_Multitrack mode"));
  gtk_widget_show (prefsw->rb_startup_mt);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rb_startup_mt), 8);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->rb_startup_mt, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->rb_startup_mt), st_interface_group);
  st_interface_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->rb_startup_mt));


  if (future_prefs->startup_interface==STARTUP_MT) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->rb_startup_mt),TRUE);
  }
  else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->rb_startup_ce),TRUE);
  }



  // multihead support

  pfsm=prefs->force_single_monitor;
  prefs->force_single_monitor=FALSE;
  get_monitors();
  prefs->force_single_monitor=pfsm;

  if (mainw->nmonitors!=nmonitors) {

    prefs->gui_monitor=0;
    prefs->play_monitor=0;

    if (mainw->nmonitors>1) {
      gchar buff[256];
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
  }

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (vbox7), hseparator, FALSE, TRUE, 20);

  label = gtk_label_new (_("Multi-head support"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox7), label, FALSE, TRUE, 10);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox, TRUE, TRUE, 20);

  spinbutton_adj = gtk_adjustment_new (prefs->gui_monitor, 1, mainw->nmonitors, 1, 1, 0);
  prefsw->spinbutton_gmoni = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_gmoni);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_gmoni, FALSE, TRUE, 0);

  label = gtk_label_new (_ (" monitor number for LiVES interface"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  label = gtk_label_new (_ (" monitor number for playback"));
  gtk_widget_show (label);
  gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  spinbutton_adj = gtk_adjustment_new (prefs->play_monitor, 0, mainw->nmonitors==1?0:mainw->nmonitors, 1, 1, 0);
  prefsw->spinbutton_pmoni = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_pmoni);
  gtk_box_pack_end (GTK_BOX (hbox), prefsw->spinbutton_pmoni, FALSE, TRUE, 0);

  label = gtk_label_new (_("A setting of 0 means use all available monitors\n(only works with some playback plugins)."));
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
  gtk_box_pack_start (GTK_BOX (vbox7), label, FALSE, TRUE, 10);

  prefsw->forcesmon = gtk_check_button_new_with_mnemonic (_ ("Force single monitor"));
  gtk_box_pack_start (GTK_BOX (vbox7), prefsw->forcesmon, FALSE, FALSE, 0);

  gtk_tooltips_set_tip (mainw->tooltips, prefsw->forcesmon, (_("Force single monitor mode")), NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->forcesmon),prefs->force_single_monitor);

  gtk_widget_show (label);

  if (mainw->nmonitors<=1) {
    gtk_widget_set_sensitive(prefsw->spinbutton_gmoni,FALSE);
    gtk_widget_set_sensitive(prefsw->spinbutton_pmoni,FALSE);
  }
  else {
    gtk_widget_show (prefsw->forcesmon);
  }

  if (prefs->force_single_monitor) get_monitors();


  g_signal_connect (GTK_OBJECT (prefsw->forcesmon), "toggled",
		    G_CALLBACK (on_forcesmon_toggled),
		    NULL);



  pref_gui = gtk_label_new (_("GUI"));
  gtk_widget_show (pref_gui);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), pref_gui);
  gtk_label_set_justify (GTK_LABEL (pref_gui), GTK_JUSTIFY_LEFT);


  // decoding

  vbox109 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox109);
  gtk_container_set_border_width (GTK_CONTAINER (vbox109), 20);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox109);

  hbox109 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox109);
  gtk_box_pack_start (GTK_BOX (vbox109), hbox109, TRUE, FALSE, 0);

  label133 = gtk_label_new (_("Video open command             "));
  gtk_widget_show (label133);
  gtk_box_pack_start (GTK_BOX (hbox109), label133, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label133), GTK_JUSTIFY_LEFT);

  prefsw->video_open_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->video_open_entry),255);
  gtk_widget_show (prefsw->video_open_entry);
  gtk_box_pack_start (GTK_BOX (hbox109), prefsw->video_open_entry, FALSE, TRUE, 0);
  gtk_entry_set_text(GTK_ENTRY(prefsw->video_open_entry),prefs->video_open_command);

  if (prefs->ocp==-1) prefs->ocp=get_int_pref ("open_compression_percent");

  hbox116 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox116);
  gtk_box_pack_start (GTK_BOX (vbox109), hbox116, TRUE, FALSE, 0);

  label158 = gtk_label_new (_("Open/render compression                  "));
  gtk_widget_show (label158);
  gtk_label_set_justify (GTK_LABEL (label158), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox116), label158, FALSE, TRUE, 0);

  spinbutton_ocp_adj = gtk_adjustment_new (prefs->ocp, 0, 100, 1, 5, 0);
  prefsw->spinbutton_ocp = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_ocp_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_ocp);
  gtk_box_pack_start (GTK_BOX (hbox116), prefsw->spinbutton_ocp, FALSE, TRUE, 0);

  label159 = gtk_label_new (_ (" %     ( lower = slower, larger files; for jpeg, higher quality )"));
  gtk_widget_show (label159);
  gtk_box_pack_start (GTK_BOX (hbox116), label159, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label159), GTK_JUSTIFY_LEFT);

  hbox115 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox115);
  gtk_box_pack_start (GTK_BOX (vbox109), hbox115, TRUE, FALSE, 0);

  label157 = gtk_label_new (_("Default image format          "));
  gtk_widget_show (label157);
  gtk_label_set_justify (GTK_LABEL (label157), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox115), label157, FALSE, TRUE, 0);

  prefsw->jpeg = gtk_radio_button_new_with_mnemonic (NULL, _("_jpeg"));
  gtk_widget_show (prefsw->jpeg);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->jpeg), 18);
  gtk_box_pack_start (GTK_BOX (hbox115), prefsw->jpeg, FALSE, FALSE, 0);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->jpeg), jpeg_png);
  jpeg_png = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->jpeg));

  png = gtk_radio_button_new_with_mnemonic (jpeg_png, _("_png"));
  gtk_widget_show (png);
  gtk_container_set_border_width (GTK_CONTAINER (png), 18);
  gtk_box_pack_start (GTK_BOX (hbox115), png, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (png),!strcmp (prefs->image_ext,"png"));


  hbox = gtk_hbox_new (FALSE, 120);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox109), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_instant_open = gtk_check_button_new_with_mnemonic (_ ("Use instant opening when possible"));
  gtk_widget_show (prefsw->checkbutton_instant_open);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_instant_open, FALSE, FALSE, 0);

  gtk_tooltips_set_tip (mainw->tooltips, prefsw->checkbutton_instant_open, (_("Enable instant opening of some file types using decoder plugins")), NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_instant_open),prefs->instant_open);


  prefsw->checkbutton_auto_deint = gtk_check_button_new_with_mnemonic (_ ("Enable automatic deinterlacing when possible"));
  gtk_widget_show (prefsw->checkbutton_auto_deint);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_auto_deint, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_auto_deint),prefs->auto_deint);

  gtk_tooltips_set_tip (mainw->tooltips, prefsw->checkbutton_auto_deint, (_("Automatically deinterlace frames when a plugin suggests it")), NULL);


  hseparator13 = gtk_hseparator_new ();
  gtk_widget_show (hseparator13);
  gtk_box_pack_start (GTK_BOX (vbox109), hseparator13, TRUE, TRUE, 0);

  prefsw->checkbutton_concat_images = gtk_check_button_new_with_mnemonic (_ ("When opening multiple files, concatenate images into one clip"));
  gtk_widget_show (prefsw->checkbutton_concat_images);
  gtk_box_pack_start (GTK_BOX (vbox109), prefsw->checkbutton_concat_images, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_concat_images),prefs->concat_images);


  label126 = gtk_label_new (_("Decoding"));
  gtk_widget_show (label126);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label126);
  gtk_label_set_justify (GTK_LABEL (label126), GTK_JUSTIFY_LEFT);


  // playback

  vbox9 = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox9);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox9);

  frame4 = gtk_frame_new (NULL);
  gtk_widget_show (frame4);
  gtk_box_pack_start (GTK_BOX (vbox9), frame4, TRUE, TRUE, 0);

  vbox69=gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox69);
  gtk_container_add (GTK_CONTAINER (frame4), vbox69);
  gtk_container_set_border_width (GTK_CONTAINER (vbox69), 10);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (vbox69), hbox);

  combo = gtk_combo_new ();
  gtk_tooltips_set_tip (mainw->tooltips, combo, (_("The preview quality for video playback - affects resizing")), NULL);
  
  label = gtk_label_new_with_mnemonic (_("Preview _quality"));

  gtk_tooltips_copy(label,combo);
  
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 10);
  gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (combo))->entry),FALSE);
  gtk_entry_set_activates_default(GTK_ENTRY((GTK_COMBO(combo))->entry),TRUE);
  
  prefsw->pbq_list=NULL;

  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("Low - can improve performance on slower machines")))); // translators - video quality, max len 50
  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("Normal - recommended for most users"))));  // translators - video quality, max len 50
  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("High - can improve quality on very fast machines")))); // translators - video quality, max len 50

  combo_set_popdown_strings (GTK_COMBO (combo), prefsw->pbq_list);

  prefsw->pbq_entry=(GTK_COMBO(combo))->entry;
  gtk_entry_set_width_chars (GTK_ENTRY (prefsw->pbq_entry),50);

  gtk_widget_show (hbox);
  gtk_widget_show (combo);
  gtk_widget_show (label);

  switch (prefs->pb_quality) {
  case PB_QUALITY_HIGH:
    gtk_entry_set_text(GTK_ENTRY(prefsw->pbq_entry),g_list_nth_data(prefsw->pbq_list,2));
    break;
  case PB_QUALITY_MED:
    gtk_entry_set_text(GTK_ENTRY(prefsw->pbq_entry),g_list_nth_data(prefsw->pbq_list,1));
  }

  hbox101 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox101);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox101, FALSE, FALSE, 0);

  prefsw->checkbutton_show_stats = gtk_check_button_new_with_mnemonic (_ ("_Show FPS statistics"));
  gtk_widget_show (prefsw->checkbutton_show_stats);
  gtk_box_pack_start (GTK_BOX (hbox101), prefsw->checkbutton_show_stats, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_show_stats),prefs->show_player_stats);

  add_fill_to_box(GTK_BOX(hbox101));

  hseparator13 = gtk_hseparator_new ();
  gtk_widget_show (hseparator13);
  gtk_box_pack_start (GTK_BOX (vbox69), hseparator13, TRUE, TRUE, 0);

  hbox31 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox31);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox31, FALSE, FALSE, 0);

  label126 = gtk_label_new_with_mnemonic (_("_Plugin"));
  gtk_widget_show (label126);
  gtk_box_pack_start (GTK_BOX (hbox31), label126, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label126), GTK_JUSTIFY_LEFT);

  pp_combo = gtk_combo_new ();
  gtk_widget_show (pp_combo);
  gtk_box_pack_start (GTK_BOX (hbox31), pp_combo, FALSE, FALSE, 20);

  combo_entry4 = GTK_COMBO (pp_combo)->entry;
  gtk_editable_set_editable (GTK_EDITABLE(combo_entry4),FALSE);
  gtk_widget_show (combo_entry4);

  vid_playback_plugins=get_plugin_list (PLUGIN_VID_PLAYBACK,TRUE,NULL,"-so");
  vid_playback_plugins=g_list_prepend (vid_playback_plugins,g_strdup(mainw->none_string));
  combo_set_popdown_strings (GTK_COMBO (pp_combo), vid_playback_plugins);
  g_list_free_strings (vid_playback_plugins);
  g_list_free (vid_playback_plugins);

  advbutton = gtk_button_new_with_mnemonic (_("_Advanced"));
  gtk_widget_show (advbutton);
  gtk_box_pack_start (GTK_BOX (hbox31), advbutton, FALSE, FALSE, 40);

  g_signal_connect (GTK_OBJECT (advbutton), "clicked",
		    G_CALLBACK (on_vpp_advanced_clicked),
		    NULL);

  if (mainw->vpp!=NULL) {
    gtk_entry_set_text (GTK_ENTRY(combo_entry4),mainw->vpp->name);
  }
  else {
    gtk_entry_set_text (GTK_ENTRY(combo_entry4),mainw->none_string);
    gtk_widget_set_sensitive (advbutton, FALSE);
  }

  g_signal_connect_after (G_OBJECT (combo_entry4), "changed", G_CALLBACK (after_vpp_changed), (gpointer) advbutton);

  label31 = gtk_label_new (_("VIDEO"));
  gtk_widget_show (label31);
  gtk_frame_set_label_widget (GTK_FRAME (frame4), label31);
  gtk_label_set_justify (GTK_LABEL (label31), GTK_JUSTIFY_LEFT);

  frame5 = gtk_frame_new (NULL);
  gtk_widget_show (frame5);
  gtk_box_pack_start (GTK_BOX (vbox9), frame5, TRUE, TRUE, 0);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame5), vbox);

  hbox10 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox10, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox10), 10);

  label35 = gtk_label_new_with_mnemonic (_("_Player"));
  gtk_widget_show (label35);
  gtk_box_pack_start (GTK_BOX (hbox10), label35, FALSE, FALSE, 20);
  gtk_label_set_justify (GTK_LABEL (label35), GTK_JUSTIFY_LEFT);


#ifdef HAVE_PULSE_AUDIO
  audp = g_list_append (audp, g_strdup_printf("pulse audio (%s)",mainw->recommended_string));
  has_ap_rec=TRUE;
#endif

#ifdef ENABLE_JACK
  if (!has_ap_rec) audp = g_list_append (audp, g_strdup_printf("jack (%s)",mainw->recommended_string));
  else audp = g_list_append (audp, g_strdup_printf("jack"));
  has_ap_rec=TRUE;
#endif

  if (capable->has_sox) {
    if (has_ap_rec) audp = g_list_append (audp, g_strdup("sox"));
    else audp = g_list_append (audp, g_strdup_printf("sox (%s)",mainw->recommended_string));
  }

  if (capable->has_mplayer) {
    audp = g_list_append (audp, g_strdup("mplayer"));
  }

  prefsw->audp_combo = gtk_combo_new ();
  combo_set_popdown_strings (GTK_COMBO (prefsw->audp_combo), audp);
  gtk_box_pack_start (GTK_BOX (hbox10), prefsw->audp_combo, TRUE, TRUE, 20);
  gtk_widget_show(prefsw->audp_combo);

  gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO(prefsw->audp_combo))->entry),FALSE);

  has_ap_rec=FALSE;

  add_fill_to_box(GTK_BOX(hbox10));

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    prefsw->audp_name=g_strdup_printf("pulse audio (%s)",mainw->recommended_string);
  }
  has_ap_rec=TRUE;
#endif


#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    if (!has_ap_rec)
      prefsw->audp_name=g_strdup_printf("jack (%s)",mainw->recommended_string);
    else prefsw->audp_name=g_strdup_printf("jack");
  }
  has_ap_rec=TRUE;
#endif

  if (prefs->audio_player==AUD_PLAYER_SOX) {
    if (!has_ap_rec) prefsw->audp_name=g_strdup_printf("sox (%s)",mainw->recommended_string);
    else prefsw->audp_name=g_strdup_printf("sox");
  }

  if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
    prefsw->audp_name=g_strdup(_ ("mplayer"));
  }

  gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(prefsw->audp_combo))->entry),prefsw->audp_name);

  hbox10 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox10, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox10), 10);

  label36 = gtk_label_new_with_mnemonic (_("Audio play _command"));
  gtk_widget_show (label36);
  gtk_box_pack_start (GTK_BOX (hbox10), label36, FALSE, FALSE, 20);
  gtk_label_set_justify (GTK_LABEL (label36), GTK_JUSTIFY_LEFT);

  prefsw->audio_command_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->audio_command_entry),255);
  gtk_widget_show (prefsw->audio_command_entry);
  gtk_box_pack_start (GTK_BOX (hbox10), prefsw->audio_command_entry, TRUE, TRUE, 0);

  add_fill_to_box(GTK_BOX(hbox10));

  // get from prefs
  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) gtk_entry_set_text(GTK_ENTRY(prefsw->audio_command_entry),prefs->audio_play_command);
  else {
    gtk_entry_set_text(GTK_ENTRY(prefsw->audio_command_entry),(_("- internal -")));
    gtk_widget_set_sensitive(prefsw->audio_command_entry,FALSE);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  prefsw->checkbutton_afollow = gtk_check_button_new_with_mnemonic (_ ("Audio follows video _rate/direction"));
  gtk_widget_show (prefsw->checkbutton_afollow);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_afollow, FALSE, FALSE, 10);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_afollow),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)?TRUE:FALSE);
  gtk_widget_set_sensitive(prefsw->checkbutton_afollow,prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE);

  prefsw->checkbutton_aclips = gtk_check_button_new_with_mnemonic (_ ("Audio follows _clip switches"));
  gtk_widget_show (prefsw->checkbutton_aclips);
  gtk_box_pack_end (GTK_BOX (hbox), prefsw->checkbutton_aclips, FALSE, FALSE, 10);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_aclips),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)?TRUE:FALSE);
  gtk_widget_set_sensitive(prefsw->checkbutton_aclips,prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE);

  label32 = gtk_label_new (_("AUDIO"));
  gtk_widget_show (label32);
  gtk_frame_set_label_widget (GTK_FRAME (frame5), label32);
  gtk_label_set_justify (GTK_LABEL (label32), GTK_JUSTIFY_LEFT);

  label25 = gtk_label_new (_("Playback"));
  gtk_widget_show (label25);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label25);
  gtk_label_set_justify (GTK_LABEL (label25), GTK_JUSTIFY_LEFT);


  // recording
  vbox12 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox12);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox12), 20);

  prefsw->rdesk_audio = gtk_check_button_new_with_mnemonic (_("Record audio when capturing an e_xternal window\n (requires jack or pulse audio)"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rdesk_audio), 18);
  gtk_widget_show (prefsw->rdesk_audio);

#ifndef RT_AUDIO
  gtk_widget_set_sensitive (prefsw->rdesk_audio,FALSE);
#endif

  gtk_box_pack_start (GTK_BOX (vbox12), prefsw->rdesk_audio, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rdesk_audio),prefs->rec_desktop_audio);

  hseparator8 = gtk_hseparator_new ();
  gtk_widget_show (hseparator8);
  gtk_box_pack_start (GTK_BOX (vbox12), hseparator8, TRUE, TRUE, 0);

  label37 = gtk_label_new (_("      What to record when 'r' is pressed   "));
  gtk_widget_show (label37);
  gtk_box_pack_start (GTK_BOX (vbox12), label37, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label37), GTK_JUSTIFY_LEFT);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  prefsw->rframes = gtk_check_button_new_with_mnemonic (_("_Frame changes"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rframes), 18);
  gtk_widget_show (prefsw->rframes);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->rframes, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rframes),prefs->rec_opts&REC_FRAMES);

  if (prefs->rec_opts&REC_FPS||prefs->rec_opts&REC_CLIPS) gtk_widget_set_sensitive (prefsw->rframes,FALSE); // we must record these if recording fps changes or clip switches
  if (mainw->playing_file>0&&mainw->record) gtk_widget_set_sensitive (prefsw->rframes,FALSE);

  prefsw->rfps = gtk_check_button_new_with_mnemonic (_("F_PS changes"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rfps), 18);
  gtk_widget_show (prefsw->rfps);

  if (prefs->rec_opts&REC_CLIPS) gtk_widget_set_sensitive (prefsw->rfps,FALSE); // we must record these if recording clip switches
  if (mainw->playing_file>0&&mainw->record) gtk_widget_set_sensitive (prefsw->rfps,FALSE);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->rfps, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rfps),prefs->rec_opts&REC_FPS);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  prefsw->reffects = gtk_check_button_new_with_mnemonic (_("_Real time effects"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->reffects), 18);
  gtk_widget_show (prefsw->reffects);
  if (mainw->playing_file>0&&mainw->record) gtk_widget_set_sensitive (prefsw->reffects,FALSE);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->reffects, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->reffects),prefs->rec_opts&REC_EFFECTS);

  prefsw->rclips = gtk_check_button_new_with_mnemonic (_("_Clip switches"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->rclips), 18);
  gtk_widget_show (prefsw->rclips);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->rclips, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rclips),prefs->rec_opts&REC_CLIPS);
  if (mainw->playing_file>0&&mainw->record) gtk_widget_set_sensitive (prefsw->rclips,FALSE);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  prefsw->raudio = gtk_check_button_new_with_mnemonic (_("_Audio (requires jack or pulse audio player)"));
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->raudio), 18);
  gtk_widget_show (prefsw->raudio);

  gtk_box_pack_start (GTK_BOX (hbox), prefsw->raudio, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->raudio),prefs->rec_opts&REC_AUDIO);

  if (mainw->playing_file>0&&mainw->record) gtk_widget_set_sensitive (prefsw->raudio,FALSE);

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) gtk_widget_set_sensitive (prefsw->raudio,FALSE);


  label = gtk_label_new (_("Recording"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  

  // encoding



  vbox12 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox12);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox12), 20);

  hbox11 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox11);
  gtk_box_pack_start (GTK_BOX (vbox12), hbox11, TRUE, TRUE, 0);

  label37 = gtk_label_new (_("      Encoder                  "));
  gtk_widget_show (label37);
  gtk_box_pack_start (GTK_BOX (hbox11), label37, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label37), GTK_JUSTIFY_LEFT);

  prefsw->encoder_combo = gtk_combo_new ();
  gtk_box_pack_start (GTK_BOX (hbox11), prefsw->encoder_combo, FALSE, FALSE, 0);

  if (capable->has_encoder_plugins) {
    // scan for encoder plugins
    if ((encoders=get_plugin_list (PLUGIN_ENCODERS,TRUE,NULL,NULL))==NULL) {
      do_plugin_encoder_error(NULL);
    }
    else {
      encoders=filter_encoders_by_img_ext(encoders,prefs->image_ext);
      combo_set_popdown_strings (GTK_COMBO (prefsw->encoder_combo), encoders);
      g_list_free_strings (encoders);
      g_list_free (encoders);
    }
  }

  gtk_widget_show(prefsw->encoder_combo);

  gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO(prefsw->encoder_combo))->entry),FALSE);
  gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(prefsw->encoder_combo))->entry),prefs->encoder.name);

  hseparator8 = gtk_hseparator_new ();
  gtk_widget_show (hseparator8);
  gtk_box_pack_start (GTK_BOX (vbox12), hseparator8, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 0);

  label56 = gtk_label_new (_("Output format"));
  gtk_box_pack_start (GTK_BOX (hbox), label56, TRUE, FALSE, 0);
  
  prefsw->ofmt_combo = gtk_combo_new ();

  if (capable->has_encoder_plugins) {
    // reqest formats from the encoder plugin
    if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
      for (i=0;i<g_list_length(ofmt_all);i++) {
	if (get_token_count (g_list_nth_data (ofmt_all,i),'|')>2) {
	  array=g_strsplit (g_list_nth_data (ofmt_all,i),"|",-1);
	  if (!strcmp(array[0],prefs->encoder.of_name)) {
	    prefs->encoder.of_allowed_acodecs=atoi(array[2]);
	  }
	  ofmt=g_list_append(ofmt,g_strdup (array[1]));
	  g_strfreev (array);
	}
      }
      w_memcpy (&future_prefs->encoder,&prefs->encoder,sizeof(_encoder));
      combo_set_popdown_strings (GTK_COMBO (prefsw->ofmt_combo), ofmt);
      g_list_free_strings(ofmt);
      g_list_free(ofmt);
      g_list_free_strings(ofmt_all);
      g_list_free(ofmt_all);
    }
    else {
      do_plugin_encoder_error(prefs->encoder.name);
      future_prefs->encoder.of_allowed_acodecs=0;
    }
    
    gtk_box_pack_start (GTK_BOX (hbox), prefsw->ofmt_combo, TRUE, TRUE, 10);
    add_fill_to_box (GTK_BOX (hbox));
    gtk_widget_show_all(hbox);

    gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO(prefsw->ofmt_combo))->entry),FALSE);
    gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(prefsw->ofmt_combo))->entry),prefs->encoder.of_name);
  
    prefsw->acodec_combo = gtk_combo_new ();
    prefsw->acodec_entry=(GtkWidget*)(GTK_ENTRY((GTK_COMBO(prefsw->acodec_combo))->entry));
    prefs->acodec_list=NULL;

    set_acodec_list_from_allowed(prefsw,rdet);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 0);

    label = gtk_label_new (_("Audio codec"));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 10);
    
    gtk_box_pack_start (GTK_BOX (hbox), prefsw->acodec_combo, TRUE, TRUE, 10);
    add_fill_to_box (GTK_BOX (hbox));
    gtk_widget_show_all (hbox);

    gtk_editable_set_editable (GTK_EDITABLE(prefsw->acodec_entry),FALSE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->acodec_entry);
  }

  label = gtk_label_new (_("Encoding"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  

  // effects

  vbox12 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox12);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox12), 20);
  
  prefsw->checkbutton_antialias = gtk_check_button_new_with_mnemonic (_("Use _antialiasing when resizing"));
  gtk_widget_show (prefsw->checkbutton_antialias);
  gtk_box_pack_start (GTK_BOX (vbox12), prefsw->checkbutton_antialias, FALSE, FALSE, 10);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_antialias),prefs->antialias);

  hbox = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox12), hbox, TRUE, TRUE, 0);
  
  label = gtk_label_new_with_mnemonic (_("Number of _real time effect keys"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

  spinbutton_adj = gtk_adjustment_new (prefs->rte_keys_virtual, FX_KEYS_PHYSICAL, FX_KEYS_MAX_VIRTUAL, 1, 1, 0);
  
  prefsw->spinbutton_rte_keys = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_rte_keys);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_rte_keys, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_rte_keys);
  gtk_tooltips_set_tip (mainw->tooltips, prefsw->spinbutton_rte_keys, _("The number of \"virtual\" real time effect keys. They can be controlled through the real time effects window, or via network (OSC)."), NULL);

  label = gtk_label_new (_("Effects"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  table4 = gtk_table_new (9, 3, FALSE);
  gtk_widget_show (table4);
  gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), table4);
  gtk_container_set_border_width (GTK_CONTAINER (table4), 20);
  
  label39 = gtk_label_new (_("      Video load directory (default)      "));
  gtk_widget_show (label39);
  gtk_table_attach (GTK_TABLE (table4), label39, 0, 1, 0, 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label39), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label39), 0, 0.5);
  
  label40 = gtk_label_new (_("      Video save directory (default) "));
  gtk_widget_show (label40);
  gtk_table_attach (GTK_TABLE (table4), label40, 0, 1, 1, 2,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label40), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label40), 0, 0.5);
  
  label41 = gtk_label_new (_("      Audio load directory (default) "));
  gtk_widget_show (label41);
  gtk_table_attach (GTK_TABLE (table4), label41, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label41), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label41), 0, 0.5);
  
  label42 = gtk_label_new (_("      Image directory (default) "));
  gtk_widget_show (label42);
  gtk_table_attach (GTK_TABLE (table4), label42, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label42), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label42), 0, 0.5);
  
  label52 = gtk_label_new (_("      Backup/Restore directory (default) "));
  gtk_widget_show (label52);
  gtk_table_attach (GTK_TABLE (table4), label52, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label52), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label52), 0, 0.5);
  
  label43 = gtk_label_new (_("      Temp directory (do not remove) "));
  gtk_widget_show (label43);
  gtk_table_attach (GTK_TABLE (table4), label43, 0, 1, 7, 8,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label43), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label43), 0, 0.5);

  prefsw->vid_load_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->vid_load_dir_entry),255);
  gtk_widget_show (prefsw->vid_load_dir_entry);
  gtk_table_attach (GTK_TABLE (table4), prefsw->vid_load_dir_entry, 1, 2, 0, 1,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);

  gtk_tooltips_set_tip (mainw->tooltips, prefsw->vid_load_dir_entry, _("The default directory for loading video clips from"), NULL);


  label = gtk_label_new (_("The temp directory is LiVES working directory where opened clips and sets are stored.\nIt should be in a partition with plenty of free disk space.\n\nTip: avoid setting it inside /tmp, since frequently /tmp is cleared on system shutdown."));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table4), label, 0, 3, 5, 7,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  
  // directories

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->vid_load_dir_entry),prefs->def_vid_load_dir);
  
  prefsw->vid_save_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->vid_save_dir_entry),255);
  gtk_widget_show (prefsw->vid_save_dir_entry);
  gtk_table_attach (GTK_TABLE (table4), prefsw->vid_save_dir_entry, 1, 2, 1, 2,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  gtk_tooltips_set_tip (mainw->tooltips, prefsw->vid_save_dir_entry, _("The default directory for saving encoded clips to"), NULL);

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->vid_save_dir_entry),prefs->def_vid_save_dir);
  
  prefsw->audio_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->audio_dir_entry),255);
  gtk_widget_show (prefsw->audio_dir_entry);
  gtk_table_attach (GTK_TABLE (table4), prefsw->audio_dir_entry, 1, 2, 2, 3,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  gtk_tooltips_set_tip (mainw->tooltips, prefsw->audio_dir_entry, _("The default directory for loading and saving audio"), NULL);

  // get from prefs
   gtk_entry_set_text(GTK_ENTRY(prefsw->audio_dir_entry),prefs->def_audio_dir);
   
   prefsw->image_dir_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->image_dir_entry),255);
   gtk_widget_show (prefsw->image_dir_entry);
   gtk_table_attach (GTK_TABLE (table4), prefsw->image_dir_entry, 1, 2, 3, 4,
		     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		     (GtkAttachOptions) (0), 0, 0);
   
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->image_dir_entry, _("The default directory for saving frameshots to"), NULL);

   // get from prefs
   gtk_entry_set_text(GTK_ENTRY(prefsw->image_dir_entry),prefs->def_image_dir);
   
   prefsw->proj_dir_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->proj_dir_entry),255);
   gtk_widget_show (prefsw->proj_dir_entry);
   gtk_table_attach (GTK_TABLE (table4), prefsw->proj_dir_entry, 1, 2, 4, 5,
		     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		     (GtkAttachOptions) (0), 0, 0);
   
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->proj_dir_entry, _("The default directory for backing up/restoring single clips"), NULL);

   // get from prefs
   gtk_entry_set_text(GTK_ENTRY(prefsw->proj_dir_entry),prefs->def_proj_dir);
   
   prefsw->tmpdir_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->tmpdir_entry),255);
   gtk_widget_show (prefsw->tmpdir_entry);
   gtk_table_attach (GTK_TABLE (table4), prefsw->tmpdir_entry, 1, 2, 7, 8,
		     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		     (GtkAttachOptions) (0), 0, 0);
   
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->tmpdir_entry, _("LiVES working directory."), NULL);

   // get from prefs
   gtk_entry_set_text(GTK_ENTRY(prefsw->tmpdir_entry),(tmp=g_filename_to_utf8(future_prefs->tmpdir,-1,NULL,NULL,NULL)));
   
   dirbutton1 = gtk_button_new ();
   gtk_widget_show (dirbutton1);
   
   
   dirimage1 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage1);
   gtk_container_add (GTK_CONTAINER (dirbutton1), dirimage1);
   
   gtk_table_attach (GTK_TABLE (table4), dirbutton1, 2, 3, 0, 1,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
   
   
   dirbutton2 = gtk_button_new ();
   gtk_widget_show (dirbutton2);
   
   
   dirimage2 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage2);
   gtk_container_add (GTK_CONTAINER (dirbutton2), dirimage2);
   
   gtk_table_attach (GTK_TABLE (table4), dirbutton2, 2, 3, 1, 2,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
   
   
   dirbutton3 = gtk_button_new ();
   gtk_widget_show (dirbutton3);

   
   dirimage3 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage3);
   gtk_container_add (GTK_CONTAINER (dirbutton3), dirimage3);
   
   gtk_table_attach (GTK_TABLE (table4), dirbutton3, 2, 3, 2, 3,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
   
   
   dirbutton4 = gtk_button_new ();
   gtk_widget_show (dirbutton4);
   
   
   dirimage4 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage4);
   gtk_container_add (GTK_CONTAINER (dirbutton4), dirimage4);
   
   gtk_table_attach (GTK_TABLE (table4), dirbutton4, 2, 3, 3, 4,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
   

   dirbutton5 = gtk_button_new ();
   gtk_widget_show (dirbutton5);

   
   dirimage5 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage5);
   gtk_container_add (GTK_CONTAINER (dirbutton5), dirimage5);
  
   gtk_table_attach (GTK_TABLE (table4), dirbutton5, 2, 3, 4, 5,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
  
  
   dirbutton6 = gtk_button_new ();
   gtk_widget_show (dirbutton6);

   
   dirimage6 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (dirimage6);
   gtk_container_add (GTK_CONTAINER (dirbutton6), dirimage6);
   
   gtk_table_attach (GTK_TABLE (table4), dirbutton6, 2, 3, 7, 8,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);

   
   label27 = gtk_label_new (_("Directories"));
   gtk_widget_show (label27);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label27);
   gtk_label_set_justify (GTK_LABEL (label27), GTK_JUSTIFY_LEFT);
   
   vbox18 = gtk_vbox_new (FALSE, 10);
   gtk_widget_show (vbox18);

   scrollw = gtk_scrolled_window_new (NULL, NULL);
   gtk_widget_show (scrollw);

   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

   gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollw), vbox18);

   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), scrollw);
   gtk_container_set_border_width (GTK_CONTAINER (vbox18), 20);
   
   prefsw->checkbutton_warn_fps = gtk_check_button_new_with_mnemonic (_("Warn on Insert / Merge if _frame rate of clipboard does not match frame rate of selection"));
   gtk_widget_show (prefsw->checkbutton_warn_fps);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_fps, FALSE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_fps), !(prefs->warning_mask&WARN_MASK_FPS));
   
   hbox100 = gtk_hbox_new(FALSE,10);
   gtk_box_pack_start (GTK_BOX (vbox18), hbox100, FALSE, FALSE, 0);
   gtk_widget_show(hbox100);
   prefsw->checkbutton_warn_fsize = gtk_check_button_new_with_mnemonic (_("Warn on Open if file _size exceeds "));
   gtk_widget_show (prefsw->checkbutton_warn_fsize);
   gtk_box_pack_start (GTK_BOX (hbox100), prefsw->checkbutton_warn_fsize, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_fsize), !(prefs->warning_mask&WARN_MASK_FSIZE));
   spinbutton_warn_fsize_adj = gtk_adjustment_new (prefs->warn_file_size, 1, 2048, 1, 10, 0);
   
   prefsw->spinbutton_warn_fsize = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_warn_fsize_adj), 1, 0);
   gtk_widget_show (prefsw->spinbutton_warn_fsize);
   gtk_box_pack_start (GTK_BOX (hbox100), prefsw->spinbutton_warn_fsize, FALSE, TRUE, 0);
   
   label100 = gtk_label_new (_ (" MB"));
   gtk_widget_show (label100);
   gtk_box_pack_start (GTK_BOX (hbox100), label100, FALSE, FALSE, 0);
   gtk_label_set_justify (GTK_LABEL (label100), GTK_JUSTIFY_LEFT);
   
   prefsw->checkbutton_warn_save_quality = gtk_check_button_new_with_mnemonic (_("Warn about loss of _quality when saving to an existing file"));
   gtk_widget_show (prefsw->checkbutton_warn_save_quality);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_save_quality, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_save_quality), !(prefs->warning_mask&WARN_MASK_SAVE_QUALITY));
   
   prefsw->checkbutton_warn_save_set = gtk_check_button_new_with_mnemonic (_("Show a warning before saving a se_t"));
   gtk_widget_show (prefsw->checkbutton_warn_save_set);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_save_set, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_save_set), !(prefs->warning_mask&WARN_MASK_SAVE_SET));
   
   prefsw->checkbutton_warn_mplayer = gtk_check_button_new_with_mnemonic (_("Show a warning if _mplayer is not found when LiVES is started."));
   gtk_widget_show (prefsw->checkbutton_warn_mplayer);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_mplayer, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mplayer), !(prefs->warning_mask&WARN_MASK_NO_MPLAYER));

   prefsw->checkbutton_warn_rendered_fx = gtk_check_button_new_with_mnemonic (_("Show a warning if no _rendered effects are found at startup."));
   gtk_widget_show (prefsw->checkbutton_warn_rendered_fx);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_rendered_fx, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_rendered_fx), !(prefs->warning_mask&WARN_MASK_RENDERED_FX));


   prefsw->checkbutton_warn_encoders = gtk_check_button_new_with_mnemonic (_("Show a warning if no _encoder plugins are found at startup."));
   gtk_widget_show (prefsw->checkbutton_warn_encoders);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_encoders, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_rendered_fx), !(prefs->warning_mask&WARN_MASK_NO_ENCODERS));

   prefsw->checkbutton_warn_dup_set = gtk_check_button_new_with_mnemonic (_("Show a warning if a _duplicate set name is entered."));
   gtk_widget_show (prefsw->checkbutton_warn_dup_set);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_dup_set, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_dup_set), !(prefs->warning_mask&WARN_MASK_DUPLICATE_SET));

   prefsw->checkbutton_warn_layout_clips = gtk_check_button_new_with_mnemonic (_("When a set is loaded, warn if clips are missing from _layouts."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_clips);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_clips, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_clips), !(prefs->warning_mask&WARN_MASK_LAYOUT_MISSING_CLIPS));

   prefsw->checkbutton_warn_layout_close = gtk_check_button_new_with_mnemonic (_("Warn if a clip used in a layout is about to be closed."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_close);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_close, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_close), !(prefs->warning_mask&WARN_MASK_LAYOUT_CLOSE_FILE));

   prefsw->checkbutton_warn_layout_delete = gtk_check_button_new_with_mnemonic (_("Warn if frames used in a layout are about to be deleted."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_delete);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_delete, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_delete), !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES));

   prefsw->checkbutton_warn_layout_shift = gtk_check_button_new_with_mnemonic (_("Warn if frames used in a layout are about to be shifted."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_shift);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_shift, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_shift), !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES));

   prefsw->checkbutton_warn_layout_alter = gtk_check_button_new_with_mnemonic (_("Warn if frames used in a layout are about to be altered."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_alter);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_alter, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_alter), !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES));

   prefsw->checkbutton_warn_layout_adel = gtk_check_button_new_with_mnemonic (_("Warn if audio used in a layout is about to be deleted."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_adel);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_adel, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_adel), !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO));

   prefsw->checkbutton_warn_layout_ashift = gtk_check_button_new_with_mnemonic (_("Warn if audio used in a layout is about to be shifted."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_ashift);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_ashift, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_ashift), !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO));

   prefsw->checkbutton_warn_layout_aalt = gtk_check_button_new_with_mnemonic (_("Warn if audio used in a layout is about to be altered."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_aalt);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_aalt, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_aalt), !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO));

   prefsw->checkbutton_warn_layout_popup = gtk_check_button_new_with_mnemonic (_("Popup layout errors after clip changes."));
   gtk_widget_show (prefsw->checkbutton_warn_layout_popup);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_layout_popup, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_popup), !(prefs->warning_mask&WARN_MASK_LAYOUT_POPUP));

   prefsw->checkbutton_warn_discard_layout = gtk_check_button_new_with_mnemonic (_("Warn if the layout has not been saved when leaving multitrack mode."));
   gtk_widget_show (prefsw->checkbutton_warn_discard_layout);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_discard_layout, FALSE, TRUE, 0);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_discard_layout), !(prefs->warning_mask&WARN_MASK_EXIT_MT));

   prefsw->checkbutton_warn_mt_achans = gtk_check_button_new_with_mnemonic (_("Warn if multitrack has no audio channels, and a layout with audio is loaded."));
   gtk_widget_show (prefsw->checkbutton_warn_mt_achans);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_mt_achans, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_achans), !(prefs->warning_mask&WARN_MASK_MT_ACHANS));

   prefsw->checkbutton_warn_mt_no_jack = gtk_check_button_new_with_mnemonic (_("Warn if multitrack has audio channels, and your audio player is not \"jack\" or \"pulse audio\"."));
   gtk_widget_show (prefsw->checkbutton_warn_mt_no_jack);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_mt_no_jack, FALSE, TRUE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_no_jack), !(prefs->warning_mask&WARN_MASK_MT_NO_JACK));


   prefsw->checkbutton_warn_after_dvgrab = gtk_check_button_new_with_mnemonic (_("Show info message after importing from firewire device."));
   gtk_widget_show (prefsw->checkbutton_warn_after_dvgrab);
#ifdef HAVE_LDVGRAB
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_after_dvgrab, FALSE, TRUE, 0);
#endif   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_after_dvgrab), !(prefs->warning_mask&WARN_MASK_AFTER_DVGRAB));

   prefsw->checkbutton_warn_yuv4m_open = gtk_check_button_new_with_mnemonic (_("Show a warning before opening a yuv4mpeg stream (advanced)."));
   gtk_widget_show (prefsw->checkbutton_warn_yuv4m_open);
#ifdef HAVE_YUV4MPEG
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_yuv4m_open, FALSE, TRUE, 0);
#endif   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_yuv4m_open), !(prefs->warning_mask&WARN_MASK_OPEN_YUV4M));


   prefsw->checkbutton_warn_mt_backup_space = gtk_check_button_new_with_mnemonic (_("Show a warning when multitrack is low on backup space."));
   gtk_widget_show (prefsw->checkbutton_warn_mt_backup_space);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_mt_backup_space, FALSE, TRUE, 0);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_backup_space), !(prefs->warning_mask&WARN_MASK_MT_BACKUP_SPACE));


   prefsw->checkbutton_warn_after_crash = gtk_check_button_new_with_mnemonic (_("Show a warning advising cleaning of disk space after a crash."));
   gtk_widget_show (prefsw->checkbutton_warn_after_crash);
   gtk_box_pack_start (GTK_BOX (vbox18), prefsw->checkbutton_warn_after_crash, FALSE, TRUE, 0);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_after_crash), !(prefs->warning_mask&WARN_MASK_CLEAN_AFTER_CRASH));


   label84 = gtk_label_new (_("Warnings"));
   gtk_widget_show (label84);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label84);
   gtk_label_set_justify (GTK_LABEL (label84), GTK_JUSTIFY_LEFT);
   
   vbox14 = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox14);
   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox14);
   gtk_container_set_border_width (GTK_CONTAINER (vbox14), 20);
   
   prefsw->check_midi = gtk_check_button_new_with_mnemonic (_("Midi synch (requires the files midistart and midistop)"));
   gtk_widget_show (prefsw->check_midi);
   gtk_box_pack_start (GTK_BOX (vbox14), prefsw->check_midi, FALSE, FALSE, 16);
   gtk_container_set_border_width (GTK_CONTAINER (prefsw->check_midi), 10);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->check_midi), prefs->midisynch);
   gtk_widget_set_sensitive(prefsw->check_midi,capable->has_midistartstop);
   
   hbox99 = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox99);
   gtk_box_pack_start (GTK_BOX (vbox14), hbox99, TRUE, TRUE, 0);
   
   label97 = gtk_label_new (_("When inserting/merging frames:  "));
   gtk_widget_show (label97);
   gtk_box_pack_start (GTK_BOX (hbox99), label97, FALSE, FALSE, 16);
   gtk_label_set_justify (GTK_LABEL (label97), GTK_JUSTIFY_LEFT);
   
   prefsw->ins_speed = gtk_radio_button_new_with_mnemonic (NULL, _("_Speed Up/Slow Down Insertion"));
   gtk_widget_show (prefsw->ins_speed);
   gtk_container_set_border_width (GTK_CONTAINER (prefsw->ins_speed), 18);
   gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->ins_speed), rb_group2);
   rb_group2 = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->ins_speed));
   gtk_box_pack_start (GTK_BOX (hbox99), prefsw->ins_speed, FALSE, FALSE, 0);
   
   ins_resample = gtk_radio_button_new_with_mnemonic (NULL, _("_Resample Insertion"));
   gtk_widget_show (ins_resample);
   gtk_container_set_border_width (GTK_CONTAINER (ins_resample), 18);
   gtk_radio_button_set_group (GTK_RADIO_BUTTON (ins_resample), rb_group2);
   rb_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (ins_resample));
   gtk_box_pack_start (GTK_BOX (hbox99), ins_resample, FALSE, FALSE, 0);
   gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(ins_resample),prefs->ins_resample);
   
   prefsw->check_xmms_pause = gtk_check_button_new_with_mnemonic (_("Pause xmms during audio playback"));
   gtk_box_pack_start (GTK_BOX (vbox14), prefsw->check_xmms_pause, FALSE, FALSE, 16);
   gtk_container_set_border_width (GTK_CONTAINER (prefsw->check_xmms_pause), 10);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->check_xmms_pause), prefs->pause_xmms);
   
   if (capable->has_xmms) {
     gtk_widget_show (prefsw->check_xmms_pause);
   }
   
   hbox19 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox14), hbox19, TRUE, TRUE, 0);
   
   label134 = gtk_label_new (_("CD device           "));
   gtk_widget_show (label134);
   gtk_box_pack_start (GTK_BOX (hbox19), label134, FALSE, FALSE, 18);
   gtk_label_set_justify (GTK_LABEL (label134), GTK_JUSTIFY_LEFT);
   
   prefsw->cdplay_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->cdplay_entry),255);
   gtk_box_pack_start (GTK_BOX (hbox19), prefsw->cdplay_entry, TRUE, TRUE, 20);

   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox19), vbox2, FALSE, TRUE, 0);
   add_fill_to_box (GTK_BOX (vbox2));
   gtk_widget_show (vbox2);

   buttond = gtk_file_chooser_button_new(_("LiVES: Choose CD device"),GTK_FILE_CHOOSER_ACTION_OPEN);
   gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(buttond),LIVES_DEVICE_DIR);
   gtk_box_pack_start(GTK_BOX(vbox2),buttond,TRUE,FALSE,0);
   gtk_widget_show (buttond);
   add_fill_to_box (GTK_BOX (vbox2));
   gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(buttond),16);

   g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),(gpointer)prefsw->cdplay_entry);

   if (capable->has_cdda2wav) {
     gtk_widget_show (prefsw->cdplay_entry);
     gtk_widget_show (hbox19);
   }
   else {
     gtk_widget_hide (prefsw->cdplay_entry);
     gtk_widget_hide (hbox19);
   }
   
   // get from prefs
   gtk_entry_set_text(GTK_ENTRY(prefsw->cdplay_entry),(tmp=g_filename_to_utf8(prefs->cdplay_device,-1,NULL,NULL,NULL)));
   g_free(tmp);
   
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->cdplay_entry, _("LiVES can load audio tracks from this CD"), NULL);
   
   hbox13 = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox13);
   gtk_box_pack_start (GTK_BOX (vbox14), hbox13, TRUE, TRUE, 0);
   
   label44 = gtk_label_new (_("Default FPS        "));
   gtk_widget_show (label44);
   gtk_box_pack_start (GTK_BOX (hbox13), label44, FALSE, FALSE, 18);
   gtk_label_set_justify (GTK_LABEL (label44), GTK_JUSTIFY_LEFT);
   
   spinbutton_def_fps_adj = gtk_adjustment_new (prefs->default_fps, 1, 2048, 1, 10, 0);
   
   prefsw->spinbutton_def_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_def_fps_adj), 1, 2);
   gtk_widget_show (prefsw->spinbutton_def_fps);
   gtk_box_pack_start (GTK_BOX (hbox13), prefsw->spinbutton_def_fps, FALSE, TRUE, 0);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->spinbutton_def_fps, _("Frames per second to use when none is specified"), NULL);
   
   label29 = gtk_label_new (_("Misc"));
   gtk_widget_show (label29);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label29);
   gtk_label_set_justify (GTK_LABEL (label29), GTK_JUSTIFY_LEFT);
   
   vbox99 = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox99);
   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox99);
   gtk_container_set_border_width (GTK_CONTAINER (vbox99), 20);
   
   hbox93 = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox93);
   gtk_box_pack_start (GTK_BOX (vbox99), hbox93, TRUE, TRUE, 0);
   
   label94 = gtk_label_new (_("New theme:           "));
   gtk_widget_show (label94);
   gtk_box_pack_start (GTK_BOX (hbox93), label94, FALSE, FALSE, 0);
   gtk_label_set_justify (GTK_LABEL (label94), GTK_JUSTIFY_LEFT);
   
   prefsw->theme_combo = gtk_combo_new ();
   

   // scan for themes
   themes=get_plugin_list (PLUGIN_THEMES,TRUE,NULL,NULL);
   themes=g_list_prepend (themes, g_strdup(mainw->none_string));
   
   combo_set_popdown_strings (GTK_COMBO (prefsw->theme_combo), themes);

   g_list_free_strings (themes);
   g_list_free (themes);
   
   gtk_box_pack_start (GTK_BOX (hbox93), prefsw->theme_combo, FALSE, FALSE, 0);
   gtk_widget_show(prefsw->theme_combo);
   
   gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO(prefsw->theme_combo))->entry),FALSE);

   if (strcasecmp(future_prefs->theme,"none")) {
     theme=g_strdup(future_prefs->theme);
   }
   else theme=g_strdup(mainw->none_string);

   gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(prefsw->theme_combo))->entry),theme);
   g_free(theme);

   label99 = gtk_label_new (_("Themes"));
   gtk_widget_show (label99);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label99);
   gtk_label_set_justify (GTK_LABEL (label99), GTK_JUSTIFY_LEFT);
   

   // streaming/networking
   vbox98 = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox98);
   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox98);
   gtk_container_set_border_width (GTK_CONTAINER (vbox98), 20);
   
   hbox94 = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox94);
   gtk_box_pack_start (GTK_BOX (vbox98), hbox94, TRUE, TRUE, 0);
   
   label88 = gtk_label_new (_("Download bandwidth (Kb/s)       "));
   gtk_widget_show (label88);
   gtk_box_pack_start (GTK_BOX (hbox94), label88, FALSE, FALSE, 0);
   gtk_label_set_justify (GTK_LABEL (label88), GTK_JUSTIFY_LEFT);
   
   spinbutton_bwidth_adj = gtk_adjustment_new (prefs->dl_bandwidth, 0, 100000, 1, 10, 0);
   
   prefsw->spinbutton_bwidth = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_bwidth_adj), 1, 0);
   gtk_widget_show (prefsw->spinbutton_bwidth);
   gtk_box_pack_start (GTK_BOX (hbox94), prefsw->spinbutton_bwidth, FALSE, TRUE, 0);
   
   hseparator = gtk_hseparator_new ();
   gtk_widget_show (hseparator);
   gtk_box_pack_start (GTK_BOX (vbox98), hseparator, FALSE, TRUE, 0);
   
#ifndef ENABLE_OSC
   label = gtk_label_new (_("LiVES must be compiled without \"configure --disable-OSC\" to use OMC"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 0);
#endif
   
   hbox = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox);
   gtk_box_pack_start (GTK_BOX (vbox98), hbox, TRUE, TRUE, 0);
   
   prefsw->enable_OSC = gtk_check_button_new_with_mnemonic (_("OMC remote control enabled"));
   gtk_widget_show (prefsw->enable_OSC);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->enable_OSC, FALSE, FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (prefsw->enable_OSC), 22);
   
   label = gtk_label_new (_("UDP port       "));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   
   spinbutton_adj = gtk_adjustment_new (prefs->osc_udp_port, 1, 65535, 1, 10, 0);
   
   prefsw->spinbutton_osc_udp = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
   gtk_widget_show (prefsw->spinbutton_osc_udp);
   
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_osc_udp, FALSE, TRUE, 0);
   
   prefsw->enable_OSC_start = gtk_check_button_new_with_mnemonic (_("Start OMC on startup"));
   gtk_widget_show (prefsw->enable_OSC_start);
   gtk_box_pack_start (GTK_BOX (vbox98), prefsw->enable_OSC_start, FALSE, FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (prefsw->enable_OSC_start), 22);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->enable_OSC_start), future_prefs->osc_start);
   
#ifndef ENABLE_OSC
   gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,FALSE);
   gtk_widget_set_sensitive (prefsw->enable_OSC,FALSE);
   gtk_widget_set_sensitive (prefsw->enable_OSC_start,FALSE);
   gtk_widget_set_sensitive (label,FALSE);
#else
   if (prefs->osc_udp_started) {
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->enable_OSC), TRUE);
     gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,FALSE);
     gtk_widget_set_sensitive (prefsw->enable_OSC,FALSE);
   }
#endif
   
   label98 = gtk_label_new (_("Streaming/Networking"));
   gtk_widget_show (label98);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label98);
   gtk_label_set_justify (GTK_LABEL (label98), GTK_JUSTIFY_LEFT);
   


   // jack
   vbox98 = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox98);
   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox98);
   gtk_container_set_border_width (GTK_CONTAINER (vbox98), 20);

   label = gtk_label_new (_("Jack transport"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 10);
   
#ifndef ENABLE_JACK_TRANSPORT
   label = gtk_label_new (_("LiVES must be compiled with jack/transport.h and jack/jack.h present to use jack transport"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 10);
#else
   hbox = gtk_hbox_new (FALSE,0);
   gtk_widget_show (hbox);
   gtk_box_pack_start (GTK_BOX (vbox98), hbox, TRUE, TRUE, 0);

   label = gtk_label_new_with_mnemonic (_("Jack _transport config file"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

   prefsw->jack_tserver_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->jack_tserver_entry),255);

   gtk_entry_set_text(GTK_ENTRY(prefsw->jack_tserver_entry),prefs->jack_tserver);
   gtk_widget_set_sensitive(prefsw->jack_tserver_entry,FALSE);
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->jack_tserver_entry);

   gtk_widget_show (prefsw->jack_tserver_entry);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->jack_tserver_entry, FALSE, FALSE, 0);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->jack_tserver_entry, _("The name of the jack server which can control LiVES transport"), NULL);

   prefsw->checkbutton_start_tjack = gtk_check_button_new_with_mnemonic (_ ("Start _server on LiVES startup"));
   gtk_widget_show (prefsw->checkbutton_start_tjack);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_start_tjack, TRUE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_start_tjack),(future_prefs->jack_opts&JACK_OPTS_START_TSERVER)?TRUE:FALSE);

   prefsw->checkbutton_jack_master = gtk_check_button_new_with_mnemonic (_ ("Jack transport _master"));
   gtk_widget_show (prefsw->checkbutton_jack_master);
   gtk_box_pack_start (GTK_BOX (vbox98), prefsw->checkbutton_jack_master, TRUE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_master),(future_prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER)?TRUE:FALSE);

   prefsw->checkbutton_jack_client = gtk_check_button_new_with_mnemonic (_ ("Jack transport _client"));
   gtk_widget_show (prefsw->checkbutton_jack_client);
   gtk_box_pack_start (GTK_BOX (vbox98), prefsw->checkbutton_jack_client, TRUE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_client),(future_prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)?TRUE:FALSE);
#endif

   hseparator = gtk_hseparator_new ();
   gtk_widget_show (hseparator);
   gtk_box_pack_start (GTK_BOX (vbox98), hseparator, FALSE, TRUE, 0);

   label = gtk_label_new (_("Jack audio"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 10);

#ifndef ENABLE_JACK
   label = gtk_label_new (_("LiVES must be compiled with jack/jack.h present to use jack audio"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 10);
#else
   label = gtk_label_new (_("You MUST set the audio player to \"jack\" in the Playback tab to use jack audio"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox98), label, FALSE, FALSE, 10);

   hbox = gtk_hbox_new (FALSE,0);
   gtk_widget_show (hbox);
   gtk_box_pack_start (GTK_BOX (vbox98), hbox, TRUE, TRUE, 0);

   label = gtk_label_new_with_mnemonic (_("Jack _audio server config file"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

   prefsw->jack_aserver_entry = gtk_entry_new ();
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->jack_aserver_entry),255);

   gtk_entry_set_text(GTK_ENTRY(prefsw->jack_aserver_entry),prefs->jack_aserver);
   gtk_widget_set_sensitive(prefsw->jack_aserver_entry,FALSE);
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->jack_aserver_entry);

   gtk_widget_show (prefsw->jack_aserver_entry);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->jack_aserver_entry, FALSE, FALSE, 0);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->jack_aserver_entry, _("The name of the jack server for audio"), NULL);

   prefsw->checkbutton_start_ajack = gtk_check_button_new_with_mnemonic (_ ("Start _server on LiVES startup"));
   gtk_widget_show (prefsw->checkbutton_start_ajack);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_start_ajack, TRUE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_start_ajack),(future_prefs->jack_opts&JACK_OPTS_START_ASERVER)?TRUE:FALSE);

   prefsw->checkbutton_jack_pwp = gtk_check_button_new_with_mnemonic (_ ("Play audio even when _paused"));
   gtk_widget_show (prefsw->checkbutton_jack_pwp);
   gtk_box_pack_start (GTK_BOX (vbox98), prefsw->checkbutton_jack_pwp, TRUE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_pwp),(future_prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE);
   gtk_widget_set_sensitive (prefsw->checkbutton_jack_pwp,prefs->audio_player==AUD_PLAYER_JACK);
#endif

   label98 = gtk_label_new (_("Jack Integration"));
   gtk_widget_show (label98);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label98);
   gtk_label_set_justify (GTK_LABEL (label98), GTK_JUSTIFY_LEFT);


   if (mainw->multitrack==NULL) {
     gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox77);
     gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), pref_multitrack);
   }
   


   // MIDI/js learner
   vbox = gtk_vbox_new (FALSE, 20);
   gtk_widget_show (vbox);
   gtk_container_add (GTK_CONTAINER (prefsw->prefs_notebook), vbox);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 20);

   label = gtk_label_new (_("Events to respond to:"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
   prefsw->checkbutton_omc_js = gtk_check_button_new_with_mnemonic (_("_Joystick events"));
   gtk_widget_show (prefsw->checkbutton_omc_js);
   gtk_box_pack_start (GTK_BOX (vbox), prefsw->checkbutton_omc_js, FALSE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_omc_js), prefs->omc_dev_opts&OMC_DEV_JS);



   hbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
   gtk_widget_show (hbox);
   
   label = gtk_label_new_with_mnemonic (_("_Joystick device"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 18);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

   prefsw->omc_js_entry = gtk_entry_new ();
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->omc_js_entry);
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->omc_js_entry),255);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->omc_js_entry, TRUE, TRUE, 20);
   gtk_widget_show (prefsw->omc_js_entry);
   if (strlen(prefs->omc_js_fname)!=0) gtk_entry_set_text (GTK_ENTRY (prefsw->omc_js_entry),prefs->omc_js_fname);
   
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->omc_js_entry, _("The joystick device, e.g. /dev/input/js0"), NULL);

   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, TRUE, 0);
   gtk_widget_show (vbox2);
   add_fill_to_box (GTK_BOX (vbox2));

   buttond = gtk_file_chooser_button_new(_("LiVES: Choose joystick device"),GTK_FILE_CHOOSER_ACTION_OPEN);
   gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(buttond),LIVES_DEVICE_DIR);
   gtk_box_pack_start(GTK_BOX(vbox2),buttond,TRUE,FALSE,0);
   gtk_widget_show (buttond);
   add_fill_to_box (GTK_BOX (vbox2));
   gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(buttond),16);
   
   g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),(gpointer)prefsw->omc_js_entry);
   


#endif


#ifdef OMC_MIDI_IMPL

   prefsw->checkbutton_omc_midi = gtk_check_button_new_with_mnemonic (_("_MIDI events"));
   gtk_widget_show (prefsw->checkbutton_omc_midi);
   gtk_box_pack_start (GTK_BOX (vbox), prefsw->checkbutton_omc_midi, FALSE, FALSE, 0);
   
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_omc_midi), prefs->omc_dev_opts&OMC_DEV_MIDI);


   hbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 20);

#ifdef ALSA_MIDI
   gtk_widget_show (hbox);
#endif

   prefsw->alsa_midi = gtk_radio_button_new_with_mnemonic (NULL, _("Use _ALSA MIDI (recommended)"));
   gtk_widget_show (prefsw->alsa_midi);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->alsa_midi, TRUE, TRUE, 20);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->alsa_midi, (_("Create an ALSA MIDI port which other MIDI devices can be connected to")), NULL);

   gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefsw->alsa_midi), alsa_midi_group);
   alsa_midi_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->alsa_midi));


   raw_midi_button = gtk_radio_button_new_with_mnemonic (alsa_midi_group, _("Use _raw MIDI"));
   gtk_widget_show (raw_midi_button);
   gtk_box_pack_start (GTK_BOX (hbox), raw_midi_button, TRUE, TRUE, 20);
   gtk_tooltips_set_tip (mainw->tooltips, raw_midi_button, (_("Read directly from the MIDI device")), NULL);

#ifdef ALSA_MIDI
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (raw_midi_button),!prefs->use_alsa_midi);
#endif

   hbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
   gtk_widget_show (hbox);
   
   label = gtk_label_new_with_mnemonic (_("_MIDI device"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 18);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   
   prefsw->omc_midi_entry = gtk_entry_new ();
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->omc_midi_entry);
   gtk_entry_set_max_length(GTK_ENTRY(prefsw->omc_midi_entry),255);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->omc_midi_entry, TRUE, TRUE, 20);
   gtk_widget_show (prefsw->omc_midi_entry);
   if (strlen(prefs->omc_midi_fname)!=0) gtk_entry_set_text (GTK_ENTRY (prefsw->omc_midi_entry),prefs->omc_midi_fname);

   gtk_tooltips_set_tip (mainw->tooltips, prefsw->omc_midi_entry, _("The MIDI device, e.g. /dev/input/midi0"), NULL);

   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, TRUE, 0);
   gtk_widget_show (vbox2);
   add_fill_to_box (GTK_BOX (vbox2));

   prefsw->button_midid = gtk_file_chooser_button_new(_("LiVES: Choose MIDI device"),GTK_FILE_CHOOSER_ACTION_OPEN);
   gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(prefsw->button_midid),LIVES_DEVICE_DIR);
   gtk_box_pack_start(GTK_BOX(vbox2),prefsw->button_midid,TRUE,FALSE,0);
   gtk_widget_show (prefsw->button_midid);
   add_fill_to_box (GTK_BOX (vbox2));
   gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(prefsw->button_midid),16);

   g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),(gpointer)prefsw->omc_midi_entry);
   

   hseparator = gtk_hseparator_new ();
   gtk_widget_show (hseparator);
   gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 10);

   
   label = gtk_label_new (_("Advanced"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);

   hbox = gtk_hbox_new (FALSE,0);
   gtk_widget_show (hbox);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
   
   label = gtk_label_new_with_mnemonic (_("MIDI check _rate"));
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
   
   spinbutton_adj = gtk_adjustment_new (prefs->midi_check_rate, 1, 2000, 10, 100, 0);
   
   prefsw->spinbutton_midicr = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 10, 0);
   gtk_widget_show (prefsw->spinbutton_midicr);
   gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_midicr, FALSE, TRUE, 0);
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_midicr);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->spinbutton_midicr, _("Number of MIDI checks per keyboard tick. Increasing this may improve MIDI responsiveness, but may slow down playback."), NULL);

   label = gtk_label_new (_("(Warning: setting this value too high can slow down playback.)"));
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 40);
   


   label = gtk_label_new_with_mnemonic (_("MIDI repeat"));
   gtk_widget_show (label);
   
   spinbutton_adj = gtk_adjustment_new (prefs->midi_rpt, 1, 10000, 100, 1000, 0);
   
   prefsw->spinbutton_midirpt = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 100, 0);
   gtk_widget_show (prefsw->spinbutton_midirpt);
   gtk_box_pack_end (GTK_BOX (hbox), prefsw->spinbutton_midirpt, FALSE, TRUE, 0);
   gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 10);
   gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_midirpt);
   gtk_tooltips_set_tip (mainw->tooltips, prefsw->spinbutton_midirpt, _("Number of non-reads allowed between succesive reads."), NULL);



#ifdef ALSA_MIDI
   g_signal_connect (GTK_OBJECT (prefsw->alsa_midi), "toggled",
		     G_CALLBACK (on_alsa_midi_toggled),
		     NULL);

   on_alsa_midi_toggled(GTK_TOGGLE_BUTTON(prefsw->alsa_midi),prefsw);
#endif

   
#endif
#endif

   label = gtk_label_new (_("MIDI/Joystick learner"));
   gtk_widget_show (label);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefsw->prefs_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (prefsw->prefs_notebook), pageno++), label);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);


   // end

   dialog_action_area8 = GTK_DIALOG (prefsw->prefs_dialog)->action_area;
   gtk_widget_show (dialog_action_area8);
   gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area8), GTK_BUTTONBOX_END);
   
   prefsw->cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
   gtk_widget_show (prefsw->cancelbutton);
   gtk_dialog_add_action_widget (GTK_DIALOG (prefsw->prefs_dialog), prefsw->cancelbutton, GTK_RESPONSE_CANCEL);
   GTK_WIDGET_SET_FLAGS (prefsw->cancelbutton, GTK_CAN_DEFAULT);
   
   applybutton = gtk_button_new_from_stock ("gtk-apply");
   gtk_widget_show (applybutton);
   gtk_dialog_add_action_widget (GTK_DIALOG (prefsw->prefs_dialog), applybutton, 0);
   GTK_WIDGET_SET_FLAGS (applybutton, GTK_CAN_DEFAULT);
   
   okbutton5 = gtk_button_new_from_stock ("gtk-ok");
   gtk_widget_show (okbutton5);
   gtk_dialog_add_action_widget (GTK_DIALOG (prefsw->prefs_dialog), okbutton5, GTK_RESPONSE_OK);
   GTK_WIDGET_SET_FLAGS (okbutton5, GTK_CAN_DEFAULT);
   
   
   g_signal_connect(dirbutton1, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->vid_load_dir_entry);
   g_signal_connect(dirbutton2, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->vid_save_dir_entry);
   g_signal_connect(dirbutton3, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->audio_dir_entry);
   g_signal_connect(dirbutton4, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->image_dir_entry);
   g_signal_connect(dirbutton5, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->proj_dir_entry);
   g_signal_connect(dirbutton6, "clicked", G_CALLBACK (on_filesel_complex_clicked),prefsw->tmpdir_entry);
   
   
   if (capable->has_encoder_plugins) {
     prefsw->encoder_name_fn=g_signal_connect (GTK_OBJECT((GTK_COMBO(prefsw->encoder_combo))->entry),"changed",G_CALLBACK (on_encoder_entry_changed),NULL);
     prefsw->encoder_ofmt_fn=g_signal_connect (GTK_OBJECT((GTK_COMBO(prefsw->ofmt_combo))->entry),"changed",G_CALLBACK (on_encoder_ofmt_changed),NULL);
   }
   
   prefsw->audp_entry_func=g_signal_connect (GTK_OBJECT((GTK_COMBO(prefsw->audp_combo))->entry),"changed",G_CALLBACK (on_audp_entry_changed),NULL);

#ifdef ENABLE_OSC
   g_signal_connect (GTK_OBJECT (prefsw->enable_OSC), "toggled",
		     G_CALLBACK (on_osc_enable_toggled),
		     (gpointer)prefsw->enable_OSC_start);
#endif
   g_signal_connect (GTK_OBJECT (prefsw->cancelbutton), "clicked",
		     G_CALLBACK (on_prefs_cancel_clicked),
		     NULL);
   
   
   g_signal_connect (GTK_OBJECT (okbutton5), "clicked",
		     G_CALLBACK (on_prefs_ok_clicked),
		     NULL);
   
   g_signal_connect (GTK_OBJECT (applybutton), "clicked",
		     G_CALLBACK (on_prefs_apply_clicked),
		     NULL);
   
   g_signal_connect (GTK_OBJECT (prefsw->prefs_dialog), "delete_event",
		     G_CALLBACK (on_prefs_delete_event),
		     prefsw);

   g_list_free_strings (audp);
   g_list_free (audp);

   return prefsw;
}




void
on_preferences_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (prefsw!=NULL&&prefsw->prefs_dialog!=NULL) {
    gtk_window_present(GTK_WINDOW(prefsw->prefs_dialog));
    gdk_window_raise(prefsw->prefs_dialog->window);
    return;
  }

  prefsw=create_prefs_dialog();
  gtk_widget_show(prefsw->prefs_dialog);

}


void
on_prefs_ok_clicked                   (GtkButton       *button,
				       gpointer         user_data)
{
  gboolean needs_restart;
  gboolean dont_show_warnings=FALSE;
  weed_plant_t *frame_layer;

  if (user_data!=NULL) {
    dont_show_warnings=TRUE;
  }
  needs_restart=apply_prefs(dont_show_warnings);

  frame_layer=mainw->frame_layer;
  mainw->frame_layer=NULL;
  mainw->frame_layer=frame_layer;

  on_prefs_cancel_clicked(button,NULL);

  if (needs_restart&&!dont_show_warnings) {
    do_blocking_error_dialog(_("\nLiVES will now shut down. You need to restart it for the directory change to take effect.\nClick OK to continue.\n"));
    on_quit_activate (NULL,NULL);
  }
  else {
    if (mainw->prefs_changed&PREFS_THEME_CHANGED&&!dont_show_warnings) {
      do_error_dialog(_("Theme changes will not take effect until the next time you start LiVES."));
    }
    if (mainw->prefs_changed&PREFS_JACK_CHANGED&&!dont_show_warnings) {
      do_error_dialog(_("Jack options will not take effect until the next time you start LiVES."));
    }
    if (!dont_show_warnings) mainw->prefs_changed=0;
  }
}

void
on_prefs_apply_clicked                   (GtkButton       *button,
					  gpointer         user_data)
{
  gint current_page=gtk_notebook_get_current_page(GTK_NOTEBOOK(prefsw->prefs_notebook));

  on_prefs_ok_clicked (button, GINT_TO_POINTER (1));
  on_preferences_activate (NULL, NULL);

  gtk_notebook_set_current_page (GTK_NOTEBOOK(prefsw->prefs_notebook),current_page);

}

void
on_prefs_cancel_clicked                   (GtkButton       *button,
					   gpointer         user_data)
{
  int i;
  if (future_prefs->vpp_argv!=NULL) {
    for (i=0;future_prefs->vpp_argv[i]!=NULL;g_free(future_prefs->vpp_argv[i++]));
    g_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv=NULL;
  }
  memset (future_prefs->vpp_name,0,64);

  if (prefs->acodec_list!=NULL) {
    g_list_free_strings (prefs->acodec_list);
    g_list_free (prefs->acodec_list);
  }
  prefs->acodec_list=NULL;

  if (prefsw->pbq_list!=NULL) g_list_free(prefsw->pbq_list);
  prefsw->pbq_list=NULL;

  g_free(prefsw->audp_name);

  on_cancel_button1_clicked(button,prefsw);

  prefsw=NULL;
}

gboolean
on_prefs_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)


{
  if (prefs->acodec_list!=NULL) {
    g_list_free_strings (prefs->acodec_list);
    g_list_free (prefs->acodec_list);
  }
  prefs->acodec_list=NULL;
  g_free(prefsw->audp_name);
  on_cancel_button1_clicked(GTK_BUTTON (((_prefsw *)user_data)->cancelbutton),user_data);
  prefsw=NULL;
  return FALSE;
}


