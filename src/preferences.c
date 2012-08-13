// preferences.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2012
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions dealing with getting/setting user preferences
// TODO - use atom type system for prefs

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed-palettes.h"
#endif

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "support.h"
#include "resample.h"
#include "plugins.h"
#include <dlfcn.h>

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef ENABLE_OSC
static void on_osc_enable_toggled (GtkToggleButton *t1, gpointer t2) {
  if (prefs->osc_udp_started) return;
  gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,gtk_toggle_button_get_active (t1)||
			    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (t2)));
}
#endif

static void instopen_toggled (GtkToggleButton *t1, GtkWidget *button) {
  gtk_widget_set_sensitive (button,gtk_toggle_button_get_active (t1));
}


void get_pref(const gchar *key, gchar *val, gint maxlen) {
  FILE *valfile;
  gchar *vfile;
  gchar *com;
  int retval;
  int alarm_handle;
  gboolean timeout;

  memset(val,0,maxlen);

  if (mainw->cached_list!=NULL) {
    gchar *prefval=get_val_from_cached_list(key,maxlen);
    if (prefval!=NULL) {
      g_snprintf(val,maxlen,"%s",prefval);
      g_free(prefval);
    }
    return;
  }

  com=g_strdup_printf("%s get_pref \"%s\" %d %d",prefs->backend_sync,key,lives_getuid(),lives_getpid());

  if (system(com)) {
    tempdir_warning();
    g_free(com);
    return;
  }

#ifndef IS_MINGW
  vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,lives_getuid(),lives_getpid());
#else
  vfile=g_strdup_printf("%s/smogval.%d.%d",prefs->tmpdir,lives_getuid(),lives_getpid());
#endif

  do {
    retval=0;
    alarm_handle=lives_alarm_set(LIVES_PREFS_TIMEOUT);
    timeout=FALSE;
    mainw->read_failed=FALSE;

    do {

      if (!((valfile=fopen(vfile,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	if (!timeout) {
	  if (!(mainw==NULL)) {
	    weed_plant_t *frame_layer=mainw->frame_layer;
	    mainw->frame_layer=NULL;
	    while (g_main_context_iteration(NULL,FALSE));
	    mainw->frame_layer=frame_layer;
	  }
	  g_usleep(prefs->sleep_time);
	}
	else break;
      }
      else break;
    } while (!valfile);

    lives_alarm_clear(alarm_handle);

    if (timeout) {
      retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
    }
    else {
      mainw->read_failed=FALSE;
      lives_fgets(val,maxlen,valfile);
      fclose(valfile);
      unlink(vfile);
      if (mainw->read_failed) {
	retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
      }
    }
  } while (retval==LIVES_RETRY);

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



GList *get_list_pref(const gchar *key) {
  // get a list of values from a preference
  gchar **array;
  gchar buf[65536];
  int nvals,i;

  GList *retlist=NULL;

  get_pref(key,buf,65535);
  if (!strlen(buf)) return NULL;

  nvals=get_token_count(buf,'\n');
  array=g_strsplit(buf,"\n",-1);
  for (i=0;i<nvals;i++) {
    retlist=g_list_append(retlist,g_strdup(array[i]));
  }

  g_strfreev(array);

  return retlist;
}





void get_pref_default(const gchar *key, gchar *val, gint maxlen) {
  FILE *valfile;
  gchar *vfile;
  gchar *com=g_strdup_printf("%s get_pref_default \"%s\"",prefs->backend_sync,key);

  int retval;
  int alarm_handle;
  gboolean timeout;

  memset(val,0,1);

  if (system(com)) {
    tempdir_warning();
    g_free(com);
    return;
  }

#ifndef IS_MINGW
  vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,lives_getuid(),lives_getpid());
#else
  vfile=g_strdup_printf("%s/smogval.%d.%d",prefs->tmpdir,lives_getuid(),lives_getpid());
#endif

  do {
    retval=0;
    timeout=FALSE;
    mainw->read_failed=FALSE;

    alarm_handle=lives_alarm_set(LIVES_PREFS_TIMEOUT);

    do {
      if (!((valfile=fopen(vfile,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	if (!timeout) {
	  if (!(mainw==NULL)) {
	    weed_plant_t *frame_layer=mainw->frame_layer;
	    mainw->frame_layer=NULL;
	    while (g_main_context_iteration(NULL,FALSE));
	    mainw->frame_layer=frame_layer;
	  }
	  g_usleep(prefs->sleep_time);
	}
	else break;
      }
      else break;
    } while (!valfile);

    lives_alarm_clear(alarm_handle);

    if (timeout) {
      retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
    }
    else {
      mainw->read_failed=FALSE;
      lives_fgets(val,maxlen,valfile);
      fclose(valfile);
      unlink(vfile);
      if (mainw->read_failed) {
	retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
      }
    }
  } while (retval==LIVES_RETRY);

  if (!strcmp(val,"NULL")) memset(val,0,1);

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
delete_pref(const gchar *key) {
  gchar *com=g_strdup_printf("%s delete_pref \"%s\"",prefs->backend_sync,key);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}

void set_pref(const gchar *key, const gchar *value) {
  gchar *com=g_strdup_printf("%s set_pref \"%s\" \"%s\"",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_int_pref(const gchar *key, gint value) {
  gchar *com=g_strdup_printf("%s set_pref \"%s\" %d",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_int64_pref(const gchar *key, gint64 value) {
  gchar *com=g_strdup_printf("%s set_pref \"%s\" %"PRId64,prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_double_pref(const gchar *key, gdouble value) {
  gchar *com=g_strdup_printf("%s set_pref \"%s\" %.3f",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}


void
set_boolean_pref(const gchar *key, gboolean value) {
  gchar *com;

  if (value) {
    com=g_strdup_printf("%s set_pref \"%s\" true",prefs->backend_sync,key);
  }
  else {
    com=g_strdup_printf("%s set_pref \"%s\" false",prefs->backend_sync,key);
  }
  if (system(com)) {
    tempdir_warning();
  }
  g_free(com);
}



void set_list_pref(const char *key, GList *values) {
  // set pref from a list of values
  GList *xlist=values;
  gchar *string=NULL,*tmp;

  while (xlist!=NULL) {
    if (string==NULL) string=g_strdup((gchar *)xlist->data);
    else {
      tmp=g_strdup_printf("%s\n%s",string,(gchar *)xlist->data);
      g_free(string);
      string=tmp;
    }
    xlist=xlist->next;
  }

  if (string==NULL) string=g_strdup("");

  set_pref(key,string);

  g_free(string);
}





void set_vpp(gboolean set_in_prefs) {
  // Video Playback Plugin

  if (strlen (future_prefs->vpp_name)) {
    if (!g_ascii_strcasecmp(future_prefs->vpp_name,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
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
	  if (!mainw->ext_playback) 
	    do_error_dialog_with_check_transient 
	      (_ ("\n\nVideo playback plugins are only activated in\nfull screen, separate window (fs) mode\n"),
	       TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
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



static void set_temp_label_text(GtkLabel *label) {
  gchar *free_ds;
  gchar *tmpx1,*tmpx2;
  gchar *dir=future_prefs->tmpdir;
  char *markup;

  // use g_strdup* since the translation string is auto-freed() 

  if (!is_writeable_dir(dir)) {
    tmpx2=g_strdup(_("\n\n\n(Free space = UNKNOWN)"));
  }
  else {
    free_ds=lives_format_storage_space_string(get_fs_free(dir));
    tmpx2=g_strdup_printf(_("\n\n\n(Free space = %s)"),free_ds);
    g_free(free_ds);
  }

  tmpx1=g_strdup(_("The temp directory is LiVES working directory where opened clips and sets are stored.\nIt should be in a partition with plenty of free disk space.\n"));

  markup = g_markup_printf_escaped ("<span background=\"white\" foreground=\"red\"><b>%s</b></span>%s",tmpx1,tmpx2);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  g_free (markup);
  g_free(tmpx1);
  g_free(tmpx2);
}




gboolean apply_prefs(gboolean skip_warn) {
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
  const gchar *wp_path=gtk_entry_get_text(GTK_ENTRY(prefsw->wpp_entry));
  const gchar *frei0r_path=gtk_entry_get_text(GTK_ENTRY(prefsw->frei0r_entry));
  const gchar *ladspa_path=gtk_entry_get_text(GTK_ENTRY(prefsw->ladspa_entry));
  gchar tmpdir[PATH_MAX];
  const gchar *theme = gtk_combo_box_get_active_text( GTK_COMBO_BOX(prefsw->theme_combo) );
  const gchar *audp = gtk_combo_box_get_active_text( GTK_COMBO_BOX(prefsw->audp_combo) );
  const gchar *audio_codec=NULL;
  const gchar *pb_quality = gtk_combo_box_get_active_text( GTK_COMBO_BOX(prefsw->pbq_combo) );

  gint pbq=PB_QUALITY_MED;

  gdouble default_fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_def_fps));
  gboolean pause_xmms=FALSE;
  gboolean antialias=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_antialias));
  gboolean fx_threads=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_threads));
  gint nfx_threads=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_nfx_threads));
  gboolean stop_screensaver=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  gboolean open_maximised=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->open_maximised_check));
  gboolean fs_maximised=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->fs_max_check));
  gboolean show_recent=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->recent_check));
  gboolean stream_audio_out=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio));
  gboolean rec_after_pb=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb));

  guint64 ds_warn_level=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_warn_ds))*1000000;
  guint64 ds_crit_level=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_crit_ds))*1000000;

  gboolean warn_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_fps));
  gboolean warn_save_set=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_set));
  gboolean warn_fsize=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_fsize));
  gboolean warn_mplayer=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_mplayer));
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
  gboolean warn_no_pulse=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_warn_no_pulse));

  gboolean midisynch=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->check_midi));
  gboolean instant_open=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_instant_open));
  gboolean auto_deint=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint));
  gboolean auto_nobord=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_nobord));
  gboolean concat_images=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_concat_images));
  gboolean ins_speed=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->ins_speed));
  gboolean show_player_stats=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_show_stats));
  gboolean ext_jpeg=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->jpeg));
  gboolean show_tool=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->show_tool));
  gboolean mouse_scroll=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mouse_scroll));
  gboolean ce_maxspect=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_ce_maxspect));
  gint fsize_to_warn=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_warn_fsize));
  gint dl_bwidth=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_bwidth));
  gint ocp=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_ocp));
  gboolean rec_frames=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rframes));
  gboolean rec_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rfps));
  gboolean rec_effects=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->reffects));
  gboolean rec_clips=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rclips));
  gboolean rec_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->raudio));
  gboolean rec_ext_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rextaudio));
#ifdef RT_AUDIO
  gboolean rec_desk_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->rdesk_audio));
#endif

  gboolean mt_enter_prompt=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->mt_enter_prompt));
  gboolean render_prompt=!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt));
  gint mt_def_width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_width));
  gint mt_def_height=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_height));
  gint mt_def_fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(prefsw->spinbutton_mt_def_fps));
  gint mt_def_arate=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
  gint mt_def_achans=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
  gint mt_def_asamps=atoi(gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
  gint mt_def_signed_endian=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))*
    AFORM_UNSIGNED+gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))*AFORM_BIG_ENDIAN;
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
  gboolean jack_tb_start=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start));
  gboolean jack_tb_client=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client));
  gboolean jack_pwp=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_pwp));
  guint jack_opts=(JACK_OPTS_TRANSPORT_CLIENT*jack_client+JACK_OPTS_TRANSPORT_MASTER*jack_master+
		   JACK_OPTS_START_TSERVER*jack_tstart+JACK_OPTS_START_ASERVER*jack_astart+
		   JACK_OPTS_NOPLAY_WHEN_PAUSED*!jack_pwp+JACK_OPTS_TIMEBASE_START*jack_tb_start+
		   JACK_OPTS_TIMEBASE_CLIENT*jack_tb_client);
#endif

#ifdef RT_AUDIO
  gboolean audio_follow_fps=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_afollow));
  gboolean audio_follow_clips=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_aclips));
  guint audio_opts=(AUDIO_OPTS_FOLLOW_FPS*audio_follow_fps+AUDIO_OPTS_FOLLOW_CLIPS*audio_follow_clips);
#endif

#ifdef ENABLE_OSC
  guint osc_udp_port=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_osc_udp));
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

  gint rec_gb=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefsw->spinbutton_rec_gb));

  gchar audio_player[256];
  gint listlen=g_list_length (prefs->acodec_list);
  gint rec_opts=rec_frames*REC_FRAMES+rec_fps*REC_FPS+rec_effects*REC_EFFECTS+rec_clips*REC_CLIPS+rec_audio*REC_AUDIO+
    rec_ext_audio*REC_EXT_AUDIO+rec_after_pb*REC_AFTER_PB;
  guint warn_mask;

  unsigned char *new_undo_buf;
  GList *ulist;

 
#ifdef ENABLE_OSC
  gboolean set_omc_dev_opts=FALSE;
#ifdef OMC_MIDI_IMPL
  gboolean needs_midi_restart=FALSE;
#endif
#endif

  gchar *tmp;

  gchar *cdplay_device=g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(prefsw->cdplay_entry)),-1,NULL,NULL,NULL);

  if (capable->has_encoder_plugins) {
    audio_codec = gtk_combo_box_get_active_text( GTK_COMBO_BOX(prefsw->acodec_combo) );

    for (idx=0;idx<listlen&&strcmp((gchar *)g_list_nth_data (prefs->acodec_list,idx),audio_codec);idx++);

    if (idx==listlen) future_prefs->encoder.audio_codec=0;
    else future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[idx];

  }
  else future_prefs->encoder.audio_codec=0;

  g_snprintf (tmpdir,PATH_MAX,"%s",(tmp=g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(prefsw->tmpdir_entry)),
							     -1,NULL,NULL,NULL)));
  g_free(tmp);

  if (audp==NULL) memset(audio_player,0,1);
  else if (!strncmp(audp,"mplayer",7)) g_snprintf(audio_player,256,"mplayer");
  else if (!strncmp(audp,"jack",4)) g_snprintf(audio_player,256,"jack");
  else if (!strncmp(audp,"sox",3)) g_snprintf(audio_player,256,"sox");
  else if (!strncmp(audp,"pulse audio",11)) g_snprintf(audio_player,256,"pulse");
  
  if (!((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    if (prefs->rec_opts&=REC_EXT_AUDIO) rec_opts^=REC_EXT_AUDIO;
  }

  if (rec_opts!=prefs->rec_opts) {
    prefs->rec_opts=rec_opts;
    set_int_pref("record_opts",prefs->rec_opts);
  }

  warn_mask=!warn_fps*WARN_MASK_FPS+!warn_save_set*WARN_MASK_SAVE_SET+!warn_fsize*WARN_MASK_FSIZE+!warn_mplayer*
    WARN_MASK_NO_MPLAYER+!warn_rendered_fx*WARN_MASK_RENDERED_FX+!warn_encoders*
    WARN_MASK_NO_ENCODERS+!warn_layout_missing_clips*WARN_MASK_LAYOUT_MISSING_CLIPS+!warn_duplicate_set*
    WARN_MASK_DUPLICATE_SET+!warn_layout_close*WARN_MASK_LAYOUT_CLOSE_FILE+!warn_layout_delete*
    WARN_MASK_LAYOUT_DELETE_FRAMES+!warn_layout_shift*WARN_MASK_LAYOUT_SHIFT_FRAMES+!warn_layout_alter*
    WARN_MASK_LAYOUT_ALTER_FRAMES+!warn_discard_layout*WARN_MASK_EXIT_MT+!warn_after_dvgrab*
    WARN_MASK_AFTER_DVGRAB+!warn_mt_achans*WARN_MASK_MT_ACHANS+!warn_mt_no_jack*
    WARN_MASK_MT_NO_JACK+!warn_layout_adel*WARN_MASK_LAYOUT_DELETE_AUDIO+!warn_layout_ashift*
    WARN_MASK_LAYOUT_SHIFT_AUDIO+!warn_layout_aalt*WARN_MASK_LAYOUT_ALTER_AUDIO+!warn_layout_popup*
    WARN_MASK_LAYOUT_POPUP+!warn_yuv4m_open*WARN_MASK_OPEN_YUV4M+!warn_mt_backup_space*
    WARN_MASK_MT_BACKUP_SPACE+!warn_after_crash*WARN_MASK_CLEAN_AFTER_CRASH+!warn_no_pulse*WARN_MASK_NO_PULSE_CONNECT;

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

  if (ce_maxspect!=(prefs->ce_maxspect)) {
    prefs->ce_maxspect=ce_maxspect;
    set_boolean_pref("ce_maxspect",ce_maxspect);
    if (mainw->current_file>-1) {
      gint current_file=mainw->current_file;
      switch_to_file((mainw->current_file=0),current_file);
    }
  }

  if (strcmp(wp_path,prefs->weed_plugin_path)) {
    set_pref("weed_plugin_path",wp_path);
    snprintf(prefs->weed_plugin_path,PATH_MAX,"%s",wp_path);
  }

  if (strcmp(frei0r_path,prefs->frei0r_path)) {
    set_pref("frei0r_path",frei0r_path);
    snprintf(prefs->frei0r_path,PATH_MAX,"%s",frei0r_path);
  }

  if (strcmp(ladspa_path,prefs->ladspa_path)) {
    set_pref("ladspa_path",ladspa_path);
    snprintf(prefs->ladspa_path,PATH_MAX,"%s",ladspa_path);
  }

  ensure_isdir(tmpdir);
  ensure_isdir(prefs->tmpdir);
  ensure_isdir(future_prefs->tmpdir);

  if (strcmp(prefs->tmpdir,tmpdir)||strcmp (future_prefs->tmpdir,tmpdir)) {
    if (g_file_test (tmpdir, G_FILE_TEST_EXISTS)&&(strlen (tmpdir)<10||
						   strncmp (tmpdir+strlen (tmpdir)-10,"/"LIVES_TMP_NAME"/",10))) 
      g_strappend (tmpdir,PATH_MAX,LIVES_TMP_NAME"/");

    if (strcmp(prefs->tmpdir,tmpdir)||strcmp (future_prefs->tmpdir,tmpdir)) {
      gchar *msg;

      if (!check_dir_access (tmpdir)) {
	tmp=g_filename_to_utf8(tmpdir,-1,NULL,NULL,NULL);
#ifndef IS_MINGW
	msg=g_strdup_printf (_ ("Unable to create or write to the new temporary directory.\nYou may need to create it as the root user first, e.g:\n\nsudo mkdir -p %s; sudo chmod 777 %s\n\nThe directory will not be changed now.\n"),tmp,tmp);
#else
	msg=g_strdup_printf (_ ("Unable to create or write to the new temporary directory.\n%s\nPlease try another directory or contact your system administrator.\n\nThe directory will not be changed now.\n"),tmp);
#endif

	g_free(tmp);
	do_blocking_error_dialog (msg);
      }
      else {
	g_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",tmpdir);
	set_temp_label_text(GTK_LABEL(prefsw->temp_label));
	gtk_widget_queue_draw(prefsw->temp_label);
	while (g_main_context_iteration(NULL,FALSE)); // update prefs window before showing confirmation box

	msg=g_strdup (_ ("You have chosen to change the temporary directory.\nPlease make sure you have no other copies of LiVES open.\n\nIf you do have other copies of LiVES open, please close them now, *before* pressing OK.\n\nAlternatively, press Cancel to restore the temporary directory to its original setting."));	
        if (do_warning_dialog(msg)) {
	  mainw->prefs_changed=PREFS_TEMPDIR_CHANGED;
	  needs_restart=TRUE;
	}
	else {
	  g_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);
          gtk_entry_set_text(GTK_ENTRY(prefsw->tmpdir_entry), prefs->tmpdir);
	}
      }
      g_free (msg);
    }
  }

  // disabled_decoders
  if (string_lists_differ(prefs->disabled_decoders,future_prefs->disabled_decoders)) {
    if (prefs->disabled_decoders!=NULL) {
      g_list_free_strings(prefs->disabled_decoders);
      g_list_free(prefs->disabled_decoders);
    }
    prefs->disabled_decoders=g_list_copy_strings(future_prefs->disabled_decoders);
    if (prefs->disabled_decoders!=NULL) set_list_pref("disabled_decoders",prefs->disabled_decoders);
    else delete_pref("disabled_decoders");
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

  // fx_threads
  if (!fx_threads) nfx_threads=1;
  if (prefs->nfx_threads!=nfx_threads) {
    future_prefs->nfx_threads=nfx_threads;
    set_int_pref("nfx_threads",nfx_threads);
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

  if (capable->nmonitors>1) {
    if (gui_monitor!=prefs->gui_monitor||play_monitor!=prefs->play_monitor) {
      gchar *str=g_strdup_printf("%d,%d",gui_monitor,play_monitor);
      set_pref("monitors",str);
      prefs->gui_monitor=gui_monitor;
      prefs->play_monitor=play_monitor;

      if (mainw->multitrack==NULL) {
	if (prefs->gui_monitor!=0) {
	  gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-
							  mainw->LiVES->allocation.width)/2;
	  gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
							  mainw->LiVES->allocation.height)/2;
	  gtk_window_move(GTK_WINDOW(mainw->LiVES),xcen,ycen);
	  
	}
	if (prefs->open_maximised&&prefs->show_gui) {
	  gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
	}
      }
      else {
	if (prefs->gui_monitor!=0) {
	  gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-
							  mainw->multitrack->window->allocation.width)/2;
	  gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
							  mainw->multitrack->window->allocation.height)/2;
	  gtk_window_move(GTK_WINDOW(mainw->multitrack->window),xcen,ycen);
	}
	
	
	if ((prefs->gui_monitor!=0||capable->nmonitors<=1)&&prefs->open_maximised) {
	  gtk_window_maximize (GTK_WINDOW(mainw->multitrack->window));
	}
      }
      if (mainw->play_window!=NULL) {
	resize_play_window();
      }
    }
  }

  // fps stats
  if (prefs->show_player_stats!=show_player_stats) {
    prefs->show_player_stats=show_player_stats;
    set_boolean_pref("show_player_stats",show_player_stats);
  }

  if (prefs->stream_audio_out!=stream_audio_out) {
    prefs->stream_audio_out=stream_audio_out;
    set_boolean_pref("stream_audio_out",stream_audio_out);
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

  // auto border cut
  if (prefs->auto_nobord!=auto_nobord) {
    set_boolean_pref("auto_cut_borders",(prefs->auto_nobord=auto_nobord));
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
  if (!strcmp(pb_quality,(gchar *)g_list_nth_data(prefsw->pbq_list,0))) pbq=PB_QUALITY_LOW;
  if (!strcmp(pb_quality,(gchar *)g_list_nth_data(prefsw->pbq_list,1))) pbq=PB_QUALITY_MED;
  if (!strcmp(pb_quality,(gchar *)g_list_nth_data(prefsw->pbq_list,2))) pbq=PB_QUALITY_HIGH;

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
    g_snprintf(prefs->def_vid_load_dir,PATH_MAX,"%s/",def_vid_load_dir);
    get_dirname(prefs->def_vid_load_dir);
    set_pref("vid_load_dir",prefs->def_vid_load_dir);
    g_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",prefs->def_vid_load_dir);
  }

  // default video save directory
  if (strcmp(prefs->def_vid_save_dir,def_vid_save_dir)) {
    g_snprintf(prefs->def_vid_save_dir,PATH_MAX,"%s/",def_vid_save_dir);
    get_dirname(prefs->def_vid_save_dir);
    set_pref("vid_save_dir",prefs->def_vid_save_dir);
    g_snprintf(mainw->vid_save_dir,PATH_MAX,"%s",prefs->def_vid_save_dir);
  }

  // default audio directory
  if (strcmp(prefs->def_audio_dir,def_audio_dir)) {
    g_snprintf(prefs->def_audio_dir,PATH_MAX,"%s/",def_audio_dir);
    get_dirname(prefs->def_audio_dir);
    set_pref("audio_dir",prefs->def_audio_dir);
    g_snprintf(mainw->audio_dir,PATH_MAX,"%s",prefs->def_audio_dir);
  }

  // default image directory
  if (strcmp(prefs->def_image_dir,def_image_dir)) {
    g_snprintf(prefs->def_image_dir,PATH_MAX,"%s/",def_image_dir);
    get_dirname(prefs->def_image_dir);
    set_pref("image_dir",prefs->def_image_dir);
    g_snprintf(mainw->image_dir,PATH_MAX,"%s",prefs->def_image_dir);
  }

  // default project directory - for backup and restore
  if (strcmp(prefs->def_proj_dir,def_proj_dir)) {
    g_snprintf(prefs->def_proj_dir,PATH_MAX,"%s/",def_proj_dir);
    get_dirname(prefs->def_proj_dir);
    set_pref("proj_dir",prefs->def_proj_dir);
    g_snprintf(mainw->proj_load_dir,PATH_MAX,"%s",prefs->def_proj_dir);
    g_snprintf(mainw->proj_save_dir,PATH_MAX,"%s",prefs->def_proj_dir);
  }

  // the theme
  if (strcmp(future_prefs->theme,theme)&&!(!g_ascii_strcasecmp(future_prefs->theme,"none")&&!strcmp(theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) {
    if (strcmp(theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
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


  if (prefs->rec_stop_gb!=rec_gb) {
    // disk free level at which we must stop recording
    prefs->rec_stop_gb=rec_gb;
    set_int_pref("rec_stop_gb",prefs->rec_stop_gb);
  }

  if (ins_speed==prefs->ins_resample) {
    prefs->ins_resample=!ins_speed;
    set_boolean_pref ("insert_resample",prefs->ins_resample);
  }


  if (ds_warn_level!=prefs->ds_warn_level) {
    prefs->ds_warn_level=ds_warn_level;
    mainw->next_ds_warn_level=prefs->ds_warn_level;
    set_int64_pref("ds_warn_level",ds_warn_level);
  }


  if (ds_crit_level!=prefs->ds_crit_level) {
    prefs->ds_crit_level=ds_crit_level;
    set_int64_pref("ds_crit_level",ds_crit_level);
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
    do_error_dialog_with_check_transient
      (_("\nUnable to switch audio players to jack - jackd must be installed first.\nSee http://jackaudio.org\n"),
       TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
  }
  else {
    if (prefs->audio_player==AUD_PLAYER_JACK&&strcmp(audio_player,"jack")) {
      do_error_dialog_with_check_transient
	(_("\nSwitching audio players requires restart (jackd must not be running)\n"),
	 TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
    }

    // switch to sox
    if (!(strcmp(audio_player,"sox"))&&prefs->audio_player!=AUD_PLAYER_SOX) {
      switch_aud_to_sox(TRUE);
    }
    
    // switch to jack
    else if (!(strcmp(audio_player,"jack"))&&prefs->audio_player!=AUD_PLAYER_JACK) {
      // may fail
      if (!switch_aud_to_jack()) {
	do_jack_noopen_warn();
	lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
      }
    }
    
    // switch to mplayer audio
    else if (!(strcmp (audio_player,"mplayer"))&&prefs->audio_player!=AUD_PLAYER_MPLAYER) {
      switch_aud_to_mplayer(TRUE);
    }

    // switch to pulse audio
    else if (!(strcmp (audio_player,"pulse"))&&prefs->audio_player!=AUD_PLAYER_PULSE) {
      if (!capable->has_pulse_audio) {
	do_error_dialog_with_check_transient
	  (_("\nUnable to switch audio players to pulse audio\npulseaudio must be installed first.\nSee http://www.pulseaudio.org\n"),
	   TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
      }
      else {
	if (!switch_aud_to_pulse()) {
	  // revert text
	  lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
	}
      }
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
    if ((new_undo_buf=(unsigned char *)g_try_malloc(mt_undo_buf*1024*1024))==NULL) {
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
	  mainw->multitrack->undo_mem=(unsigned char *)g_try_malloc(mt_undo_buf*1024*1024);
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
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
      write_backup_layout_numbering(mainw->multitrack);
  }
  else if (!startup_ce&&future_prefs->startup_interface!=STARTUP_MT) {
    future_prefs->startup_interface=STARTUP_MT;
    set_int_pref("startup_interface",STARTUP_MT);
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
      write_backup_layout_numbering(mainw->multitrack);
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
rdet_acodec_changed (GtkComboBox *acodec_combo, gpointer user_data) {
  gint listlen=g_list_length (prefs->acodec_list);
  int idx;
  const gchar *audio_codec = gtk_combo_box_get_active_text(acodec_combo);
  if (!strcmp(audio_codec,mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) return;

  for (idx=0;idx<listlen&&strcmp((gchar *)g_list_nth_data (prefs->acodec_list,idx),audio_codec);idx++);

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
    prefs->acodec_list = g_list_append (prefs->acodec_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0]=AUDIO_CODEC_NONE;

    if (prefsw!=NULL) {
      lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
      gtk_combo_box_set_active(GTK_COMBO_BOX(prefsw->acodec_combo), 0);
    }
    if (rdet!=NULL) {
      lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
      gtk_combo_box_set_active(GTK_COMBO_BOX(rdet->acodec_combo), 0);
    }
    return;
  }
  for (idx=0;strlen(anames[idx]);idx++) {
    if (future_prefs->encoder.of_allowed_acodecs&(1<<idx)) {
      if (idx==AUDIO_CODEC_PCM) prefs->acodec_list=g_list_append (prefs->acodec_list,
								  g_strdup(_ ("PCM (highest quality; largest files)")));
      else prefs->acodec_list=g_list_append (prefs->acodec_list,g_strdup(anames[idx]));
      prefs->acodec_list_to_format[count++]=idx;
      if (future_prefs->encoder.audio_codec==idx) is_allowed=TRUE;
    }
  }

  if (prefsw != NULL){
    lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
  }
  if (rdet != NULL){
    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
  }
  if (!is_allowed) {
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0];
  }

  for (idx=0; idx < g_list_length(prefs->acodec_list); idx++) {
    if (prefs->acodec_list_to_format[idx]==future_prefs->encoder.audio_codec) {
      if (prefsw!=NULL){
        gtk_combo_box_set_active(GTK_COMBO_BOX(prefsw->acodec_combo), idx);
      }
      if (rdet!=NULL){
        gtk_combo_box_set_active(GTK_COMBO_BOX(rdet->acodec_combo), idx);
      }
      break;
    }
  }
}


void after_vpp_changed (GtkWidget *vpp_combo, gpointer advbutton) {
  const gchar *newvpp=gtk_combo_box_get_active_text(GTK_COMBO_BOX(vpp_combo));
  _vid_playback_plugin *tmpvpp;

  if (!g_ascii_strcasecmp(newvpp,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    gtk_widget_set_sensitive (GTK_WIDGET(advbutton), FALSE);
  }
  else {
    gtk_widget_set_sensitive (GTK_WIDGET(advbutton), TRUE);

    // will call set_astream_settings
    if ((tmpvpp=open_vid_playback_plugin (newvpp, FALSE))==NULL) return;
    close_vid_playback_plugin(tmpvpp);
  }
  g_snprintf (future_prefs->vpp_name,64,"%s",newvpp);

  if (future_prefs->vpp_argv!=NULL) {
    int i;
    for (i=0;future_prefs->vpp_argv[i]!=NULL;g_free(future_prefs->vpp_argv[i++]));
    g_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv=NULL;
  }
  future_prefs->vpp_argc=0;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),FALSE);

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

#ifdef ENABLE_JACK_TRANSPORT
static void after_jack_client_toggled(GtkToggleButton *tbutton, gpointer user_data) {

  if (!gtk_toggle_button_get_active(tbutton)) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),FALSE);
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,FALSE);
  }
  else {
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_tb_start),
				  (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_START)?TRUE:FALSE);
  }
}

static void after_jack_tb_start_toggled(GtkToggleButton *tbutton, gpointer user_data) {

  if (!gtk_toggle_button_get_active(tbutton)) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),FALSE);
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,FALSE);
  }
  else {
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_tb_client),
				  (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)?TRUE:FALSE);
  }
}
#endif


#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
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
#endif
#endif


static void on_audp_entry_changed (GtkWidget *audp_combo, gpointer ptr) {
  const gchar *audp = gtk_combo_box_get_active_text(GTK_COMBO_BOX(audp_combo));

  if (!strlen(audp)||!strcmp(audp,prefsw->audp_name)) return;

  if (mainw->playing_file>-1) {
    do_aud_during_play_error();
    g_signal_handler_block(audp_combo, prefsw->audp_entry_func);

    lives_combo_set_active_string(LIVES_COMBO(audp_combo), prefsw->audp_name);

    //gtk_widget_queue_draw(audp_entry);
    g_signal_handler_unblock(audp_combo, prefsw->audp_entry_func);
    return;
  }

#ifdef RT_AUDIO
  if (!strncmp(audp,"jack",4)||!strncmp(audp,"pulse",5)) {
    gtk_widget_set_sensitive(prefsw->checkbutton_aclips,TRUE);
    gtk_widget_set_sensitive(prefsw->checkbutton_afollow,TRUE);
    gtk_widget_set_sensitive(prefsw->raudio,!(prefs->rec_opts&REC_EXT_AUDIO));
    gtk_widget_set_sensitive(prefsw->rextaudio,!(prefs->rec_opts&REC_AUDIO));
  }
  else {
    gtk_widget_set_sensitive(prefsw->checkbutton_aclips,FALSE);
    gtk_widget_set_sensitive(prefsw->checkbutton_afollow,FALSE);
    gtk_widget_set_sensitive(prefsw->raudio,FALSE);
    gtk_widget_set_sensitive(prefsw->rextaudio,FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->rextaudio),FALSE);
  }
  if (!strncmp(audp,"jack",4)) {
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_pwp,TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack),TRUE);
    gtk_widget_show(prefsw->jack_int_label);
  }
  else {
    gtk_widget_set_sensitive(prefsw->checkbutton_jack_pwp,FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack),FALSE);
    gtk_widget_hide(prefsw->jack_int_label);
  }
#endif
  g_free(prefsw->audp_name);
  prefsw->audp_name=g_strdup(gtk_combo_box_get_active_text(GTK_COMBO_BOX(audp_combo)));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),FALSE);
}


static void stream_audio_toggled(GtkToggleButton *togglebutton,
				 gpointer  user_data) {
  // if audio streaming is enabled, check requisites


  if (gtk_toggle_button_get_active(togglebutton)) {
    // init vpp, get audio codec, check requisites
    _vid_playback_plugin *tmpvpp;
    guint32 orig_acodec=AUDIO_CODEC_NONE;

    if (strlen(future_prefs->vpp_name)) {
      if ((tmpvpp=open_vid_playback_plugin (future_prefs->vpp_name, FALSE))==NULL) return;
    }
    else {
      tmpvpp=mainw->vpp;
      orig_acodec=mainw->vpp->audio_codec;
      get_best_audio(mainw->vpp); // check again because audio player may differ
    }

    if (tmpvpp->audio_codec!=AUDIO_CODEC_NONE) {
      // make audiostream plugin name
      char buf[1024];
      gchar *com;
      FILE *rfile;
      size_t rlen;

      gchar *astreamer=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,"audiostreamer.pl",NULL);
      
      com=g_strdup_printf("\"%s\" check %d",astreamer,tmpvpp->audio_codec);
      g_free(astreamer);
      
      rfile=popen(com,"r");
      if (!rfile) {
	// command failed
	do_system_failed_error(com,0,NULL);
	g_free(com);
	return;
      }

      rlen=fread(buf,1,1023,rfile);
      pclose(rfile);
      memset(buf+rlen,0,1);
      g_free(com);

      if (rlen>0) {
	gtk_toggle_button_set_active(togglebutton, FALSE);
      }
    }

    if (tmpvpp!=NULL) {
      if (tmpvpp!=mainw->vpp) {
	// close the temp current vpp
	close_vid_playback_plugin(tmpvpp);
      }
      else {
	// restore current codec
	mainw->vpp->audio_codec=orig_acodec;
      }
    }

  }

}


void prefsw_set_astream_settings(_vid_playback_plugin *vpp) {

  if (vpp!=NULL&&vpp->audio_codec!=AUDIO_CODEC_NONE) {
    gtk_widget_set_sensitive(prefsw->checkbutton_stream_audio,TRUE);
    //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  }
  else {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),FALSE);
    gtk_widget_set_sensitive(prefsw->checkbutton_stream_audio,FALSE);
  }
}


void prefsw_set_rec_after_settings(_vid_playback_plugin *vpp) {
  if (vpp!=NULL&&(vpp->capabilities&VPP_CAN_RETURN)) {
    gtk_widget_set_sensitive(prefsw->checkbutton_rec_after_pb,TRUE);
    //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  }
  else {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_rec_after_pb),FALSE);
    gtk_widget_set_sensitive(prefsw->checkbutton_rec_after_pb,FALSE);
  }
}


/*
 * Initialize preferences dialog list
 */
static void pref_init_list(GtkWidget *list)
{
  GtkCellRenderer *renderer, *pixbufRenderer;
  GtkTreeViewColumn *column1, *column2;
  GtkListStore *store;

  renderer = gtk_cell_renderer_text_new();
  pixbufRenderer = gtk_cell_renderer_pixbuf_new();

  column1 = gtk_tree_view_column_new_with_attributes("List Icons", pixbufRenderer, "pixbuf", LIST_ICON, NULL);
  column2 = gtk_tree_view_column_new_with_attributes("List Items", renderer, "text", LIST_ITEM, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list), column1);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list), column2);
  gtk_tree_view_column_set_sizing(column2, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(column2, 150);

  store = gtk_list_store_new(N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_UINT);

  gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));

  g_object_unref(store);
}

/*
 * Adds entry to preferences dialog list 
 */
static void prefs_add_to_list(GtkWidget *list, GdkPixbuf *pix, const gchar *str, guint idx)
{
  GtkListStore *store;
  GtkTreeIter iter;

  store = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW(list)));

  gtk_list_store_insert(store, &iter, idx);
  gtk_list_store_set(store, &iter, LIST_ICON, pix, LIST_ITEM, str, LIST_NUM, idx, -1);
}

/*
 * Callback function called when preferences list row changed
 */
void on_prefDomainChanged(GtkTreeSelection *widget, gpointer dummy) 
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint idx;

  if (gtk_tree_selection_get_selected( widget, &model, &iter)) {
    gtk_tree_model_get(model, &iter, LIST_NUM, &idx, -1);
    //
    // Hide currently shown widget
    if (prefsw->right_shown){
      gtk_widget_hide_all(prefsw->right_shown);
    }
    //
    switch (idx){
    case LIST_ENTRY_MULTITRACK:
      gtk_widget_show_all(prefsw->scrollw_right_multitrack);
      prefsw->right_shown = prefsw->scrollw_right_multitrack;
      break;
    case LIST_ENTRY_GUI:
      gtk_widget_show_all(prefsw->scrollw_right_gui);
      prefsw->right_shown = prefsw->scrollw_right_gui;
      break;
    case LIST_ENTRY_DECODING:
      gtk_widget_show_all(prefsw->scrollw_right_decoding);
      prefsw->right_shown = prefsw->scrollw_right_decoding;
      break;
    case LIST_ENTRY_PLAYBACK:
      gtk_widget_show_all(prefsw->scrollw_right_playback);
      prefsw->right_shown = prefsw->scrollw_right_playback;
      break;
    case LIST_ENTRY_RECORDING:
      gtk_widget_show_all(prefsw->scrollw_right_recording);
      prefsw->right_shown = prefsw->scrollw_right_recording;
      break;
    case LIST_ENTRY_ENCODING:
      gtk_widget_show_all(prefsw->scrollw_right_encoding);
      prefsw->right_shown = prefsw->scrollw_right_encoding;
      break;
    case LIST_ENTRY_EFFECTS:
      gtk_widget_show_all(prefsw->scrollw_right_effects);
      prefsw->right_shown = prefsw->scrollw_right_effects;
      break;
    case LIST_ENTRY_DIRECTORIES:
      gtk_widget_show_all(prefsw->scrollw_right_directories);
      prefsw->right_shown = prefsw->scrollw_right_directories;
      break;
    case LIST_ENTRY_WARNINGS:
      gtk_widget_show_all(prefsw->scrollw_right_warnings);
      prefsw->right_shown = prefsw->scrollw_right_warnings;
      break;
    case LIST_ENTRY_MISC:
      gtk_widget_show_all(prefsw->scrollw_right_misc);
      prefsw->right_shown = prefsw->scrollw_right_misc;
      break;
    case LIST_ENTRY_THEMES:
      gtk_widget_show_all(prefsw->scrollw_right_themes);
      prefsw->right_shown = prefsw->scrollw_right_themes;
      break;
    case LIST_ENTRY_NET:
      gtk_widget_show_all(prefsw->scrollw_right_net);
      prefsw->right_shown = prefsw->scrollw_right_net;
      break;
    case LIST_ENTRY_JACK:
      gtk_widget_show_all(prefsw->scrollw_right_jack);
      prefsw->right_shown = prefsw->scrollw_right_jack;
      break;
    case LIST_ENTRY_MIDI:
      gtk_widget_show_all(prefsw->scrollw_right_midi);
      prefsw->right_shown = prefsw->scrollw_right_midi;
      break;
    default:
      gtk_widget_show_all(prefsw->scrollw_right_gui);
      prefsw->right_shown = prefsw->scrollw_right_gui;
    }
  }

  gtk_widget_queue_draw(prefsw->prefs_dialog);

}

/*
 * Function makes apply button sensitive
 */
void apply_button_set_enabled(GtkWidget *widget, gpointer func_data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->applybutton), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->cancelbutton), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->closebutton), FALSE);
}

// toggle sets other widget sensitive/insensitive
static void toggle_set_sensitive(GtkWidget *widget, gpointer func_data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(func_data), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

// toggle sets other widget insensitive/sensitive
static void toggle_set_insensitive(GtkWidget *widget, gpointer func_data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(func_data), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}



static void spinbutton_crit_ds_value_changed (GtkSpinButton *crit_ds, gpointer user_data) {
  gdouble myval=gtk_spin_button_get_value(crit_ds);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (prefsw->spinbutton_warn_ds),myval,DS_WARN_CRIT_MAX);
  apply_button_set_enabled(NULL,NULL);
}


/*
 * Function creates preferences dialog 
 */
_prefsw *create_prefs_dialog (void) {
  GtkWidget *dialog_vbox_main;
  GtkWidget *dialog_table;
  GtkWidget *dialog_hpaned;
  GtkWidget *list_scroll;

  GdkPixbuf *pixbuf_multitrack;
  GdkPixbuf *pixbuf_gui;
  GdkPixbuf *pixbuf_decoding;
  GdkPixbuf *pixbuf_playback;
  GdkPixbuf *pixbuf_recording;
  GdkPixbuf *pixbuf_encoding;
  GdkPixbuf *pixbuf_effects;
  GdkPixbuf *pixbuf_directories;
  GdkPixbuf *pixbuf_warnings;
  GdkPixbuf *pixbuf_misc;
  GdkPixbuf *pixbuf_themes;
  GdkPixbuf *pixbuf_net;
  GdkPixbuf *pixbuf_jack;
  GdkPixbuf *pixbuf_midi;
  gchar *icon;

  GtkWidget *ins_resample;
  GtkWidget *hbox100;
  GtkWidget *hbox;
  GObject *spinbutton_adj;
  GObject *spinbutton_warn_fsize_adj;
  GObject *spinbutton_bwidth_adj;
  GtkWidget *label157;
  GtkWidget *hseparator;
  GtkWidget *hbox1;
  GtkWidget *hbox2;
  GtkWidget *hbox3;
  GtkWidget *hbox4;
  GtkWidget *hbox5;
  GtkWidget *hbox6;
  GtkWidget *hbox7;
  GtkWidget *vbox69;
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
  GtkWidget *label97;
  GtkWidget *label88;
  GtkWidget *vbox;
  GtkWidget *hbox11;
  GtkWidget *hbox94;
  GtkWidget *hbox101;
  GtkWidget *label37;
  GtkWidget *hbox115;
  GtkWidget *label56;
  GtkWidget *label94;
  GtkWidget *hbox93;
  GtkWidget *label39;
  GtkWidget *label100;
  GtkWidget *label40;
  GtkWidget *label41;
  GtkWidget *label42;
  GtkWidget *label52;
  GtkWidget *label43;
  GtkWidget *hbox13;
  GtkWidget *label44;
  GObject *spinbutton_def_fps_adj;
  GtkWidget *dialog_action_area8;
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
  GtkWidget *hbox31;
  GtkWidget *label126;
  GtkWidget *pp_combo;
  GtkWidget *png;
  GtkWidget *frame;
  GtkWidget *label158;
  GtkWidget *label159;
  GtkWidget *hbox116;
  GtkWidget *mt_enter_defs;
  GObject *spinbutton_ocp_adj;
  GtkWidget *advbutton;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  GtkWidget *raw_midi_button;
#endif
#endif

  GtkWidget *label;
  GtkWidget *eventbox;
  GtkWidget *vbox2;
  GtkWidget *buttond;

  // radio button groups
  //GSList *rb_group = NULL;
  GSList *jpeg_png = NULL;
  GSList *mt_enter_prompt = NULL;
  GSList *rb_group2 = NULL;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  GSList *alsa_midi_group = NULL;
#endif
#endif

  GSList *autoback_group = NULL;
  GSList *st_interface_group = NULL;

  // drop down lists
  GList *themes = NULL;
  GList *ofmt = NULL;
  GList *ofmt_all = NULL;
  GList *audp = NULL;
  GList *encoders = NULL;
  GList *vid_playback_plugins = NULL;
  
  gchar **array,*tmp,*tmp2;

  int i;

  gint nmonitors = capable->nmonitors; ///< number of screen monitors
  gboolean pfsm;

  gchar *theme;

  gboolean has_ap_rec = FALSE;

  GdkGeometry hints;

  GtkTreeIter iter;
  GtkTreeModel *model;
  guint selected_idx;

  // Allocate memory for the preferences structure
  prefsw = (_prefsw*)(g_malloc(sizeof(_prefsw)));
  prefsw->right_shown = NULL;
  mainw->prefs_need_restart = FALSE;

  // Create new modal dialog window and set some attributes
  prefsw->prefs_dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->prefs_dialog), 10);
  gtk_window_set_title (GTK_WINDOW (prefsw->prefs_dialog), _("LiVES: - Preferences"));
  gtk_window_set_position (GTK_WINDOW (prefsw->prefs_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (prefsw->prefs_dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (prefsw->prefs_dialog), PREF_WIN_WIDTH, PREF_WIN_HEIGHT);
  gtk_window_set_resizable (GTK_WINDOW (prefsw->prefs_dialog), FALSE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(prefsw->prefs_dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(prefsw->prefs_dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  gtk_widget_modify_bg(prefsw->prefs_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_fg(prefsw->prefs_dialog, GTK_STATE_NORMAL, &palette->normal_fore);

  if (palette->style & STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(prefsw->prefs_dialog), FALSE);
  }
  // Get dialog's vbox and show it
  dialog_vbox_main = lives_dialog_get_content_area(GTK_DIALOG(prefsw->prefs_dialog));
  gtk_widget_show (dialog_vbox_main);
  // Set geometry hints for dialog's vbox
  // This prevents shrinking of dialog window in some cases
  hints.min_width = PREF_WIN_WIDTH;
  hints.min_height = PREF_WIN_HEIGHT;
  gtk_window_set_geometry_hints (GTK_WINDOW(prefsw->prefs_dialog), GTK_WIDGET (dialog_vbox_main), &hints, GDK_HINT_MIN_SIZE);

  // Create dialog horizontal panels
  dialog_hpaned = gtk_hpaned_new();
  gtk_widget_show (dialog_hpaned);
  gtk_paned_set_position(GTK_PANED(dialog_hpaned),PREFS_PANED_POS);


  // Create dialog table for the right panel controls placement
  dialog_table = gtk_table_new(1, 1, FALSE);
  gtk_widget_show(dialog_table);

  // Create preferences list with invisible headers
  prefsw->prefs_list = gtk_tree_view_new();
  gtk_widget_show(prefsw->prefs_list);

  //gtk_widget_set_size_request (prefsw->prefs_list, 200, 600);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(prefsw->prefs_list), FALSE);

  // Place panels into main vbox
  gtk_box_pack_start (GTK_BOX (dialog_vbox_main), dialog_hpaned, TRUE, TRUE, 0);

  // Place list on the left panel
  pref_init_list(prefsw->prefs_list);
  
  list_scroll = gtk_scrolled_window_new(gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(prefsw->prefs_list)), NULL);
  gtk_widget_show(list_scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (list_scroll), prefsw->prefs_list);

  gtk_paned_pack1(GTK_PANED(dialog_hpaned), list_scroll, FALSE, FALSE);
  // Place table on the right panel
  //gtk_paned_pack2(GTK_PANED(dialog_hpaned), dummy_scroll, TRUE, FALSE);
  gtk_paned_pack2(GTK_PANED(dialog_hpaned), dialog_table, TRUE, FALSE);

  // -------------------,
  // gui controls       |
  // -------------------'
  prefsw->vbox_right_gui = gtk_vbox_new (FALSE, 10);

  prefsw->scrollw_right_gui = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_gui), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_gui), prefsw->vbox_right_gui);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_gui)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_gui)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_widget_show (prefsw->vbox_right_gui);
  prefsw->right_shown = prefsw->vbox_right_gui;
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_gui), 20);
  // ---
  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox1, TRUE, TRUE, 10);
  // ---


  // TODO - copy pattern to all other checkboxes
  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->fs_max_check = 
    lives_standard_check_button_new(_("Open file selection maximised"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->fs_max_check), prefs->fileselmax);
  // ---



  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->recent_check = 
    lives_standard_check_button_new(_("Show recent files in the File menu"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->recent_check), prefs->show_recent);



  // ---
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox2, TRUE, TRUE, 10);

  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->stop_screensaver_check = 
    lives_standard_check_button_new(_("Stop screensaver on playback    "),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX(hbox2), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->stop_screensaver_check), prefs->stop_screensaver);


  // ---
  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->open_maximised_check = 
    lives_standard_check_button_new(_("Open main window maximised"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox2), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->open_maximised_check), prefs->open_maximised);

  // --
  hbox3 = gtk_hbox_new(FALSE, 0);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox3, TRUE, TRUE, 10);


  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->show_tool = 
    lives_standard_check_button_new(_("Show toolbar when background is blanked"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->show_tool), future_prefs->show_tool);
  // ---


  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->mouse_scroll = 
    lives_standard_check_button_new(_("Allow mouse wheel to switch clips"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->mouse_scroll), prefs->mouse_scroll_clips);

  // ---
  hbox4 = gtk_hbox_new(FALSE, 0);
  gtk_widget_show (hbox4);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox4, TRUE, FALSE, 10);
  // ---

  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->checkbutton_ce_maxspect = 
    lives_standard_check_button_new(_("Shrink previews to fit in interface"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox4), hbox, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_ce_maxspect), prefs->ce_maxspect);

  // ---
  hbox5 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox5, TRUE, FALSE, 10);
  // ---
  label = gtk_label_new (_("Startup mode:"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox5), label, FALSE, TRUE, 0);
  add_fill_to_box(GTK_BOX(hbox5));
  // --- 
  prefsw->rb_startup_ce = gtk_radio_button_new(st_interface_group);
  st_interface_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->rb_startup_ce));
  label = gtk_label_new_with_mnemonic(_("_Clip editor"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->rb_startup_ce);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->rb_startup_ce);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->rb_startup_ce, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(hbox5), hbox, TRUE, TRUE, 0);
  add_fill_to_box(GTK_BOX(hbox5));
  // ---
  prefsw->rb_startup_mt = gtk_radio_button_new(st_interface_group);
  st_interface_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->rb_startup_mt));
  label = gtk_label_new_with_mnemonic(_("_Multitrack mode"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->rb_startup_ce);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->rb_startup_mt);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->rb_startup_mt, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(hbox5), hbox, TRUE, TRUE, 0);
  // ---
  if (future_prefs->startup_interface==STARTUP_MT) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->rb_startup_mt),TRUE);
  }
  else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->rb_startup_ce),TRUE);
  }
  // 
  // multihead support (inside Gui part)
  //
  pfsm=prefs->force_single_monitor;
  prefs->force_single_monitor=FALSE;
  get_monitors();
  prefs->force_single_monitor=pfsm;

  if (capable->nmonitors!=nmonitors) {

    prefs->gui_monitor=0;
    prefs->play_monitor=0;

    if (capable->nmonitors>1) {
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
      if (prefs->gui_monitor>capable->nmonitors) prefs->gui_monitor=capable->nmonitors;
      if (prefs->play_monitor>capable->nmonitors) prefs->play_monitor=capable->nmonitors;
    }
  }
  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hseparator, FALSE, TRUE, 20);
  // ---
  label = gtk_label_new (_("Multi-head support"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), label, FALSE, TRUE, 10);
  // ---
  hbox6 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox6);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox6, TRUE, TRUE, 20);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->gui_monitor, 1, capable->nmonitors, 1, 1, 0);
  prefsw->spinbutton_gmoni = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  label = gtk_label_new (_ (" monitor number for LiVES interface"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->spinbutton_gmoni);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_gmoni, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox6), hbox, FALSE, TRUE, 0);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->play_monitor, 0, capable->nmonitors==1?0:capable->nmonitors, 1, 1, 0);
  prefsw->spinbutton_pmoni = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  label = gtk_label_new (_ (" monitor number for playback"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->spinbutton_pmoni);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_pmoni, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_end (GTK_BOX (hbox6), hbox, FALSE, TRUE, 0);
  // ---
  label = gtk_label_new (_("A setting of 0 means use all available monitors (only works with some playback plugins)."));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), label, TRUE, TRUE, 0);
  // ---
  hbox7 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox7);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_gui), hbox7, TRUE, TRUE, 20);
  // ---
  prefsw->forcesmon = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Force single monitor"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->forcesmon);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->forcesmon);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->forcesmon, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox7), hbox, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text( prefsw->forcesmon, (_("Force single monitor mode")));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->forcesmon), prefs->force_single_monitor);
  // ---
  if (capable->nmonitors<=1) {
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

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_gui.png", NULL);
  pixbuf_gui = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_gui, _("GUI"), LIST_ENTRY_GUI);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_gui);

  // -----------------------,
  // multitrack controls    |
  // -----------------------'

  prefsw->vbox_right_multitrack = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_multitrack), 10);

  prefsw->scrollw_right_multitrack = gtk_scrolled_window_new (NULL, NULL);
  
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_multitrack), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_multitrack), 
					 prefsw->vbox_right_multitrack);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_multitrack)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_multitrack)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox1, FALSE, FALSE, 0);

  label = gtk_label_new (_("When entering Multitrack mode:"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 16);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  // ---
  add_fill_to_box(LIVES_BOX(hbox1));
  // ---
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox2, TRUE, FALSE, 0);
  // ---
  prefsw->mt_enter_prompt = gtk_radio_button_new (mt_enter_prompt);
  mt_enter_prompt = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_enter_prompt));
  label = gtk_label_new_with_mnemonic(_("_Prompt me for width, height, fps and audio settings"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->mt_enter_prompt);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->mt_enter_prompt);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->mt_enter_prompt, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(hbox2), hbox, TRUE, TRUE, 0);
  // ---
  mt_enter_defs = gtk_radio_button_new(mt_enter_prompt);
  mt_enter_prompt = gtk_radio_button_get_group (GTK_RADIO_BUTTON (mt_enter_defs));
  label = gtk_label_new_with_mnemonic(_("_Always use the following values:"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), mt_enter_defs);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), mt_enter_defs);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), mt_enter_defs, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(hbox2), hbox, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mt_enter_defs), !prefs->mt_enter_prompt);

  prefsw->checkbutton_render_prompt = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Use these same _values for rendering a new clip"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_render_prompt);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_render_prompt);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_render_prompt, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_render_prompt), !prefs->render_prompt);
  // ---
  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  label = gtk_label_new (_("Video"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(label, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_bg(frame, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_widget_show (label);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  // ---
  hbox3 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox3, TRUE, FALSE, 0);
  // ---
  label = gtk_label_new_with_mnemonic (_("_Width           "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->mt_def_width, 0, 8192, 1, 10, 0);
  prefsw->spinbutton_mt_def_width = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefsw->spinbutton_mt_def_width);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_width, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, FALSE, TRUE, 0);
  // ---
  label = gtk_label_new_with_mnemonic (_("          _Height      "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->mt_def_height, 0, 8192, 1, 10, 0);
  prefsw->spinbutton_mt_def_height = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefsw->spinbutton_mt_def_height);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_height, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, FALSE, TRUE, 0);
  // ---
  label = gtk_label_new_with_mnemonic (_("          _FPS"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->mt_def_fps, 1, FPS_MAX, .1, 1, 0);
  prefsw->spinbutton_mt_def_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_def_fps);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_def_fps, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, FALSE, TRUE, 0);
  // --- 
  prefsw->backaudio_checkbutton = gtk_check_button_new ();
  prefsw->pertrack_checkbutton = gtk_check_button_new();
  resaudw=create_resaudw(4, NULL, prefsw->vbox_right_multitrack);
  // ---
  hbox4 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox4);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox4, TRUE, TRUE, 0);
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic (_("Enable backing audio track"));
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), prefsw->backaudio_checkbutton);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event", G_CALLBACK (label_act_toggle), 
		    prefsw->backaudio_checkbutton); 
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->backaudio_checkbutton, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox4), hbox, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS (prefsw->backaudio_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->backaudio_checkbutton), prefs->mt_backaudio>0);
  gtk_widget_set_sensitive(prefsw->backaudio_checkbutton, 
			   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic (_("Audio track per video track"));
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), prefsw->pertrack_checkbutton);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event", G_CALLBACK (label_act_toggle), 
		    prefsw->pertrack_checkbutton); 
  gtk_widget_set_sensitive(prefsw->pertrack_checkbutton, 
			   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), eventbox, FALSE, FALSE, 5);
  gtk_box_pack_end (GTK_BOX (hbox), prefsw->pertrack_checkbutton, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox4), hbox, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS (prefsw->pertrack_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->pertrack_checkbutton), prefs->mt_pertrack_audio);
  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hseparator, FALSE, TRUE, 10);
  // ---
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox, TRUE, FALSE, 0);
  // ---
  label = gtk_label_new_with_mnemonic (_("    _Undo buffer size (MB)    "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->mt_undo_buf, 0, G_MAXSIZE/(1024*1024), 1, 1, 0);
  prefsw->spinbutton_mt_undo_buf = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_mt_undo_buf);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_undo_buf, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_mt_undo_buf);
  // ---
  prefsw->checkbutton_mt_exit_render = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("_Exit multitrack mode after rendering"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_mt_exit_render);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_mt_exit_render);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_mt_exit_render, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);
  // ---
  hbox5 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_multitrack), hbox5, TRUE, FALSE, 0);
  // ---
  label = gtk_label_new (_("Auto backup layouts"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox5), label, FALSE, TRUE, 0);
  // ---
  prefsw->mt_autoback_every = gtk_radio_button_new(autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_every));
  label = gtk_label_new_with_mnemonic(_("_Every"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->mt_autoback_every);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->mt_autoback_every);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->mt_autoback_every, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, TRUE, TRUE, 0);
  // ---
  label = gtk_label_new_with_mnemonic (_("seconds"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  // ---
  spinbutton_adj = (GObject *)gtk_adjustment_new (30, 10, 1800, 1, 10, 0);
  prefsw->spinbutton_mt_ab_time = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefsw->spinbutton_mt_ab_time);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_mt_ab_time, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 20);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, FALSE, TRUE, 0);
  add_fill_to_box(GTK_BOX(hbox5));
  // ---
  prefsw->mt_autoback_always = gtk_radio_button_new(autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_always));
  label = gtk_label_new_with_mnemonic(_("After every _change"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->mt_autoback_always);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->mt_autoback_always);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->mt_autoback_always, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, TRUE, TRUE, 0);
  // ---
  prefsw->mt_autoback_never = gtk_radio_button_new(autoback_group);
  autoback_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->mt_autoback_never));
  label = gtk_label_new_with_mnemonic(_("_Never"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->mt_autoback_never);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->mt_autoback_never);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->mt_autoback_never, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, TRUE, TRUE, 0);
  // ---
  g_signal_connect (GTK_OBJECT (prefsw->mt_autoback_every), "toggled", G_CALLBACK (on_mtbackevery_toggled), prefsw); 

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
  // ---
  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_multitrack.png", NULL);
  pixbuf_multitrack = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_multitrack, _("Multitrack/Render"), LIST_ENTRY_MULTITRACK);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_multitrack);


  // ---------------,
  // decoding       |
  // ---------------'

  prefsw->vbox_right_decoding = gtk_vbox_new (FALSE, 20);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_decoding), 20);

  prefsw->scrollw_right_decoding = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_decoding), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_decoding), prefsw->vbox_right_decoding);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_decoding)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_decoding)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }


  // ---
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox, TRUE, TRUE, 0);
  // ---


  prefsw->checkbutton_instant_open = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Use instant opening when possible"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_instant_open);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_instant_open);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_instant_open, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_set_tooltip_text( prefsw->checkbutton_instant_open, 
			       (_("Enable instant opening of some file types using decoder plugins")));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_instant_open),prefs->instant_open);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);


  // advanced instant opening
  advbutton = gtk_button_new_with_mnemonic (_("_Advanced"));
  gtk_widget_show (advbutton);
  gtk_box_pack_start (GTK_BOX (hbox), advbutton, FALSE, FALSE, 40);

  g_signal_connect (GTK_OBJECT (advbutton), "clicked",
		    G_CALLBACK (on_decplug_advanced_clicked),
		    NULL);

  gtk_widget_set_sensitive(advbutton,prefs->instant_open);

  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_instant_open), "toggled", GTK_SIGNAL_FUNC(instopen_toggled), advbutton);




  hbox109 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox109);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox109, TRUE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER (hbox109), 20);
  // ---
  label133 = gtk_label_new (_("Video open command             "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label133, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label133);
  gtk_box_pack_start (GTK_BOX (hbox109), label133, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL(label133), GTK_JUSTIFY_LEFT);
  // ---
  prefsw->video_open_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->video_open_entry),255);
  gtk_widget_show (prefsw->video_open_entry);
  gtk_box_pack_start (GTK_BOX (hbox109), prefsw->video_open_entry, FALSE, TRUE, 0);
  gtk_entry_set_text(GTK_ENTRY(prefsw->video_open_entry),prefs->video_open_command);

  if (prefs->ocp==-1) prefs->ocp=get_int_pref ("open_compression_percent");
  // ---
  hbox116 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox116);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox116, TRUE, FALSE, 0);
  // ---
  label158 = gtk_label_new (_("Open/render compression                  "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label158, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label158);
  gtk_label_set_justify (GTK_LABEL (label158), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox116), label158, FALSE, TRUE, 0);
  // ---
  spinbutton_ocp_adj = (GObject *)gtk_adjustment_new (prefs->ocp, 0, 100, 1, 5, 0);
  prefsw->spinbutton_ocp = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_ocp_adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox116), prefsw->spinbutton_ocp, FALSE, TRUE, 0);

  label159 = gtk_label_new (_ (" %     ( lower = slower, larger files; for jpeg, higher quality )"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label159, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (hbox116), label159, FALSE, FALSE, 5);
  gtk_label_set_justify (GTK_LABEL (label159), GTK_JUSTIFY_LEFT);
  gtk_container_set_border_width(GTK_CONTAINER (hbox116), 20);
  gtk_widget_show_all(hbox116);
  // ---
  hbox115 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox115);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox115, TRUE, FALSE, 5);
  // ---
  label157 = gtk_label_new (_("Default image format          "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label157, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label157), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox115), label157, FALSE, TRUE, 5);
  // ---
  prefsw->jpeg = gtk_radio_button_new(jpeg_png);
  jpeg_png = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->jpeg));
  label = gtk_label_new_with_mnemonic(_("_jpeg"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->jpeg);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  gtk_box_pack_start(GTK_BOX(hbox115), eventbox, FALSE, FALSE, 5);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->jpeg);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox115), prefsw->jpeg, FALSE, FALSE, 5);
  // ---
  png = gtk_radio_button_new(jpeg_png);
  jpeg_png = gtk_radio_button_get_group (GTK_RADIO_BUTTON (png));
  label = gtk_label_new_with_mnemonic(_("_png"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), png);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  gtk_box_pack_start(GTK_BOX(hbox115), eventbox, FALSE, FALSE, 5);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), png);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox115), png, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox115), 20);
  gtk_widget_show_all(hbox115);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (png),!strcmp (prefs->image_ext,"png"));
  // ---
  hbox115 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox115);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox115, TRUE, FALSE, 0);
  // ---
  label157 = gtk_label_new (_("(Check Help/Troubleshoot to see which image formats are supported)"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label157, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label157);
  gtk_label_set_justify (GTK_LABEL (label157), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox115), label157, FALSE, TRUE, 20);
  // ---
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox, TRUE, TRUE, 0);
  // ---

  prefsw->checkbutton_auto_deint = gtk_check_button_new();
  eventbox = gtk_event_box_new();

  label = gtk_label_new_with_mnemonic(_("Enable automatic deinterlacing when possible"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_auto_deint);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_auto_deint);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_auto_deint, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_auto_deint),prefs->auto_deint);
  gtk_widget_set_tooltip_text( prefsw->checkbutton_auto_deint, (_("Automatically deinterlace frames when a plugin suggests it")));
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);


  // ---
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox, TRUE, TRUE, 0);
  // ---

  prefsw->checkbutton_nobord = gtk_check_button_new();
  eventbox = gtk_event_box_new();

  label = gtk_label_new_with_mnemonic(_("Ignore blank borders when possible"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_nobord);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_nobord);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->checkbutton_nobord, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_nobord),prefs->auto_nobord);
  gtk_widget_set_tooltip_text( prefsw->checkbutton_nobord, (_("Clip any blank borders from frames where possible")));
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);

  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hseparator, TRUE, TRUE, 0);
  // ---
  prefsw->checkbutton_concat_images = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("When opening multiple files, concatenate images into one clip"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_concat_images);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_concat_images);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_concat_images, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_decoding), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_concat_images),prefs->concat_images);
  // ---
  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_decoding.png", NULL);
  pixbuf_decoding = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_decoding, _("Decoding"), LIST_ENTRY_DECODING);
  //gtk_table_attach(GTK_TABLE(dialog_table), prefsw->vbox_right_decoding, 0, 1, 0, 1, GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_decoding);

  // ---------------,
  // playback       |
  // ---------------'

  prefsw->vbox_right_playback = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_playback), 20);

  prefsw->scrollw_right_playback = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_playback), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_playback), prefsw->vbox_right_playback);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_playback)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_playback)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }


  frame4 = gtk_frame_new (NULL);
  gtk_widget_show (frame4);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_playback), frame4, TRUE, TRUE, 0);

  vbox69=gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox69);
  gtk_container_add (GTK_CONTAINER (frame4), vbox69);
  gtk_container_set_border_width (GTK_CONTAINER (vbox69), 10);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (vbox69), hbox);

  prefsw->pbq_combo = lives_combo_new();
  gtk_widget_set_tooltip_text( prefsw->pbq_combo, (_("The preview quality for video playback - affects resizing")));
  
  label = gtk_label_new_with_mnemonic (_("Preview _quality"));
  if (palette->style & STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  lives_tooltips_copy(label, prefsw->pbq_combo);
  
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->pbq_combo, FALSE, FALSE, 10);
  
  prefsw->pbq_list=NULL;
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("Low - can improve performance on slower machines"))));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("Normal - recommended for most users"))));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=g_list_append(prefsw->pbq_list,g_strdup((_("High - can improve quality on very fast machines"))));

  lives_combo_populate(LIVES_COMBO(prefsw->pbq_combo), prefsw->pbq_list);
  gtk_combo_box_set_active(GTK_COMBO_BOX(prefsw->pbq_combo), 0);

  gtk_widget_show (hbox);
  gtk_widget_show (prefsw->pbq_combo);
  gtk_widget_show (label);

  switch (prefs->pb_quality) {
  case PB_QUALITY_HIGH:
    gtk_combo_box_set_active(GTK_COMBO_BOX(prefsw->pbq_combo), 2);
    break;
  case PB_QUALITY_MED:
    gtk_combo_box_set_active(GTK_COMBO_BOX(prefsw->pbq_combo), 1);
  }
  // ---
  hbox101 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox101);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox101, FALSE, FALSE, 0);

  prefsw->checkbutton_show_stats = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("_Show FPS statistics"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_show_stats);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_show_stats);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_show_stats, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox101), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_show_stats),prefs->show_player_stats);

  add_fill_to_box(GTK_BOX(hbox101));
  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (vbox69), hseparator, TRUE, TRUE, 0);
  // ---
  hbox31 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox31);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox31, FALSE, FALSE, 0);

  label126 = gtk_label_new_with_mnemonic (_("_Plugin"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label126, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label126);
  gtk_box_pack_start (GTK_BOX (hbox31), label126, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label126), GTK_JUSTIFY_LEFT);
  // ---
  pp_combo = lives_combo_new();
  gtk_box_pack_start (GTK_BOX (hbox31), pp_combo, FALSE, FALSE, 20);
  // ---
#ifndef IS_MINGW
  vid_playback_plugins = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-so");
#else
  vid_playback_plugins = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-dll");
#endif
  vid_playback_plugins = g_list_prepend (vid_playback_plugins, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  lives_combo_populate(LIVES_COMBO(pp_combo), vid_playback_plugins);

  gtk_widget_show(pp_combo);

  advbutton = gtk_button_new_with_mnemonic (_("_Advanced"));
  gtk_widget_show (advbutton);
  gtk_box_pack_start (GTK_BOX (hbox31), advbutton, FALSE, FALSE, 40);

  g_signal_connect (GTK_OBJECT (advbutton), "clicked",
		    G_CALLBACK (on_vpp_advanced_clicked),
		    NULL);

  if (mainw->vpp != NULL) {
    lives_combo_set_active_string(LIVES_COMBO(pp_combo), mainw->vpp->name);
  }
  else {
    gtk_combo_box_set_active(GTK_COMBO_BOX(pp_combo), 0);
    gtk_widget_set_sensitive (advbutton, FALSE);
  }
  g_list_free_strings (vid_playback_plugins);
  g_list_free (vid_playback_plugins);

  g_signal_connect_after (G_OBJECT (pp_combo), "changed", G_CALLBACK (after_vpp_changed), (gpointer) advbutton);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_stream_audio = 
    lives_standard_check_button_new((tmp=g_strdup(_("Stream audio"))),
				    TRUE,(LiVESBox *)hbox,
				    (tmp2=g_strdup
				     (_("Stream audio to playback plugin"))));
  g_free(tmp);
  g_free(tmp2);


  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),prefs->stream_audio_out);

  prefsw_set_astream_settings(mainw->vpp);

  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_stream_audio), "toggled", GTK_SIGNAL_FUNC(stream_audio_toggled), NULL);




  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox69), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_rec_after_pb = 
    lives_standard_check_button_new((tmp=g_strdup(_("Record player output"))),
				    TRUE,(LiVESBox *)hbox,
				    (tmp2=g_strdup
				     (_("Record output from player instead of input to player"))));
  g_free(tmp);
  g_free(tmp2);
								     
								     

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_rec_after_pb),(prefs->rec_opts&REC_AFTER_PB));

  prefsw_set_rec_after_settings(mainw->vpp);

  label31 = gtk_label_new (_("VIDEO"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label31, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(label31, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(frame4, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_widget_show (label31);
  gtk_frame_set_label_widget (GTK_FRAME (frame4), label31);
  gtk_label_set_justify (GTK_LABEL(label31), GTK_JUSTIFY_LEFT);

  frame5 = gtk_frame_new (NULL);
  gtk_widget_show (frame5);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_playback), frame5, TRUE, TRUE, 0);

  vbox = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame5), vbox);

  hbox10 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox10, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox10), 10);

  label35 = gtk_label_new_with_mnemonic (_("_Player"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label35, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label35);
  gtk_box_pack_start (GTK_BOX (hbox10), label35, FALSE, FALSE, 20);
  gtk_label_set_justify (GTK_LABEL (label35), GTK_JUSTIFY_LEFT);

#ifdef HAVE_PULSE_AUDIO
  audp = g_list_append (audp, g_strdup_printf("pulse audio (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  has_ap_rec=TRUE;
#endif

#ifdef ENABLE_JACK
  if (!has_ap_rec) audp = g_list_append (audp, g_strdup_printf("jack (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  else audp = g_list_append (audp, g_strdup_printf("jack"));
  has_ap_rec=TRUE;
#endif

  if (capable->has_sox_play) {
    if (has_ap_rec) audp = g_list_append (audp, g_strdup("sox"));
    else audp = g_list_append (audp, g_strdup_printf("sox (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  }

  if (capable->has_mplayer) {
    audp = g_list_append (audp, g_strdup("mplayer"));
  }

  prefsw->audp_combo = lives_combo_new();
  lives_combo_populate(LIVES_COMBO(prefsw->audp_combo), audp);

  gtk_box_pack_start (GTK_BOX (hbox10), prefsw->audp_combo, TRUE, TRUE, 20);
  gtk_widget_show(prefsw->audp_combo);

  has_ap_rec=FALSE;

  add_fill_to_box(GTK_BOX(hbox10));

  prefsw->jack_int_label=gtk_label_new(_("(See also the Jack Integration tab for jack startup options)"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(prefsw->jack_int_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (vbox), prefsw->jack_int_label, FALSE, FALSE, 0);

  gtk_widget_hide(prefsw->jack_int_label);


  prefsw->audp_name=NULL;

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    prefsw->audp_name=g_strdup_printf("pulse audio (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
  }
  has_ap_rec=TRUE;
#endif

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    if (!has_ap_rec)
      prefsw->audp_name=g_strdup_printf("jack (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name=g_strdup_printf("jack");
    gtk_widget_show(prefsw->jack_int_label);
  }
  has_ap_rec=TRUE;
#endif

  if (prefs->audio_player==AUD_PLAYER_SOX) {
    if (!has_ap_rec) prefsw->audp_name=g_strdup_printf("sox (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name=g_strdup_printf("sox");
  }

  if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
    prefsw->audp_name=g_strdup(_ ("mplayer"));
  }
  // ---
  if (prefsw->audp_name!=NULL) 
    lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->audp_name);
  prefsw->orig_audp_name=g_strdup(prefsw->audp_name);
  //---
  hbox10 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox10, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox10), 10);

  label36 = gtk_label_new_with_mnemonic (_("Audio play _command"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label36, GTK_STATE_NORMAL, &palette->normal_fore);
  }
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

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, TRUE, TRUE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  // ---
  prefsw->checkbutton_afollow = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Audio follows video _rate/direction"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_afollow);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_afollow);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_afollow, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, FALSE, FALSE, 10);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_afollow),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)?TRUE:FALSE);
  gtk_widget_set_sensitive(prefsw->checkbutton_afollow,prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE);

  // ---
  prefsw->checkbutton_aclips = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Audio follows _clip switches"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_aclips);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_aclips);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_aclips, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_end (GTK_BOX (hbox1), hbox, FALSE, FALSE, 10);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_aclips),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)?TRUE:FALSE);
  gtk_widget_set_sensitive(prefsw->checkbutton_aclips,prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE);

  label32 = gtk_label_new (_("AUDIO"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label32, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(label31, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_bg(frame5, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_widget_show (label32);
  gtk_frame_set_label_widget (GTK_FRAME (frame5), label32);
  gtk_label_set_justify (GTK_LABEL (label32), GTK_JUSTIFY_LEFT);

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_playback.png", NULL);
  pixbuf_playback = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_playback, _("Playback"), LIST_ENTRY_PLAYBACK);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_playback);

  // ---------------,
  // recording      |
  // ---------------'

  prefsw->vbox_right_recording = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_recording), 20);

  prefsw->scrollw_right_recording = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_recording), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_recording), 
					 prefsw->vbox_right_recording);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_recording)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_recording)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->rdesk_audio = lives_standard_check_button_new(_("Record audio when capturing an e_xternal window\n (requires jack or pulse audio)"),TRUE,(LiVESBox *)hbox,NULL);

  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);

#ifndef RT_AUDIO
  gtk_widget_set_sensitive (prefsw->rdesk_audio,FALSE);
#endif

  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rdesk_audio),prefs->rec_desktop_audio);

  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hseparator, TRUE, TRUE, 0);
  // ---
  label37 = gtk_label_new (_("      What to record when 'r' is pressed   "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label37, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label37);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), label37, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label37), GTK_JUSTIFY_LEFT);
  // ---
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox2, TRUE, TRUE, 6);
  // ---

  hbox = gtk_hbox_new(FALSE, 0);

  prefsw->rframes = lives_standard_check_button_new(_("_Frame changes"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox2), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rframes),prefs->rec_opts&REC_FRAMES);

  // ---
  if (prefs->rec_opts&REC_FPS||prefs->rec_opts&REC_CLIPS){
    gtk_widget_set_sensitive (prefsw->rframes,FALSE); // we must record these if recording fps changes or clip switches
  }
  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->rframes,FALSE);
  }

  // ---
  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->rfps = lives_standard_check_button_new(_("F_PS changes"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  // ---
  if (prefs->rec_opts&REC_CLIPS) {
    gtk_widget_set_sensitive (prefsw->rfps,FALSE); // we must record these if recording clip switches
  }

  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->rfps,FALSE);
  }
  // ---
  gtk_box_pack_start (GTK_BOX (hbox2), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rfps),prefs->rec_opts&REC_FPS);

  // ---
  hbox3 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox3, TRUE, TRUE, 6);
  // ---

  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->reffects = lives_standard_check_button_new(_("_Real time effects"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);

  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->reffects,FALSE);
  }
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->reffects),prefs->rec_opts&REC_EFFECTS);
  // ---

  hbox = gtk_hbox_new(FALSE, 0);
  prefsw->rclips = lives_standard_check_button_new(_("_Clip switches"),TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rclips),prefs->rec_opts&REC_CLIPS);

  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->rclips,FALSE);
  }

  // ---

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox, TRUE, TRUE, 6);
  prefsw->raudio = lives_standard_check_button_new(_("_Internal Audio (requires jack or pulse audio player)"),
						   TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->raudio),prefs->rec_opts&REC_AUDIO);


  if (prefs->rec_opts&REC_EXT_AUDIO) {
    gtk_widget_set_sensitive (prefsw->raudio,FALSE);
  }

  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->raudio,FALSE);
  }

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE){
    gtk_widget_set_sensitive (prefsw->raudio,FALSE);
  }


  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox, TRUE, TRUE, 6);
  prefsw->rextaudio = lives_standard_check_button_new(_("_External Audio (requires jack or pulse audio player)"),
						   TRUE,(LiVESBox *)hbox,NULL);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->rextaudio),prefs->rec_opts&REC_EXT_AUDIO);

  if (prefs->rec_opts&REC_AUDIO) {
    gtk_widget_set_sensitive (prefsw->rextaudio,FALSE);
  }

  if (mainw->playing_file>0&&mainw->record){
    gtk_widget_set_sensitive (prefsw->rextaudio,FALSE);
  }

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE){
    gtk_widget_set_sensitive (prefsw->rextaudio,FALSE);
  }

  g_signal_connect(GTK_OBJECT(prefsw->raudio), "toggled", GTK_SIGNAL_FUNC(toggle_set_insensitive), prefsw->rextaudio);
  g_signal_connect(GTK_OBJECT(prefsw->rextaudio), "toggled", GTK_SIGNAL_FUNC(toggle_set_insensitive), prefsw->raudio);

  // ---
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hseparator, TRUE, TRUE, 0);
  // ---
  hbox5 = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_recording), hbox5, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Pause recording if free disk space falls below"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox5), label, FALSE, FALSE, 10);

  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->rec_stop_gb, 0, 1024., 1., 10., 0.);
  
  prefsw->spinbutton_rec_gb = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_rec_gb);
  gtk_box_pack_start (GTK_BOX (hbox5), prefsw->spinbutton_rec_gb, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefsw->spinbutton_rec_gb);

  label = gtk_label_new_with_mnemonic (_("GB")); // translators - gigabytes
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox5), label, FALSE, FALSE, 10);

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_record.png", NULL);
  pixbuf_recording = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_recording, _("Recording"), LIST_ENTRY_RECORDING);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_recording);

  // ---------------,
  // encoding       |
  // ---------------'

  prefsw->vbox_right_encoding = gtk_vbox_new (FALSE, 30);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_encoding), 20);

  prefsw->scrollw_right_encoding = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_encoding), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_encoding), 
					 prefsw->vbox_right_encoding);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_encoding)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_encoding)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox11 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox11);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_encoding), hbox11, FALSE, FALSE, 20);

  label37 = gtk_label_new (_("      Encoder                  "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label37, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label37);
  gtk_box_pack_start (GTK_BOX (hbox11), label37, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label37), GTK_JUSTIFY_LEFT);

  prefsw->encoder_combo = lives_combo_new();
  gtk_box_pack_start(GTK_BOX(hbox11), prefsw->encoder_combo, FALSE, FALSE, 0);

  if (capable->has_encoder_plugins) {
    // scan for encoder plugins
    if ((encoders=get_plugin_list (PLUGIN_ENCODERS,TRUE,NULL,NULL))!=NULL) {
      encoders=filter_encoders_by_img_ext(encoders,prefs->image_ext);
      lives_combo_populate(LIVES_COMBO(prefsw->encoder_combo), encoders);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      g_list_free_strings (encoders);
      g_list_free (encoders);
    }
  }

  gtk_widget_show(prefsw->encoder_combo);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_encoding), hseparator, FALSE, FALSE, 20);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_encoding), hbox, FALSE, FALSE, 20);

  label56 = gtk_label_new (_("Output format"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label56, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (hbox), label56, TRUE, FALSE, 0);
  
  prefsw->ofmt_combo = lives_combo_new();

  if (capable->has_encoder_plugins) {
    // reqest formats from the encoder plugin
    if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
      for (i=0;i<g_list_length(ofmt_all);i++) {
	if (get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|')>2) {
	  array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);
	  if (!strcmp(array[0],prefs->encoder.of_name)) {
	    prefs->encoder.of_allowed_acodecs=atoi(array[2]);
	  } 
	  ofmt = g_list_append(ofmt, g_strdup(array[1]));
	  g_strfreev (array);
	}
      }
      lives_memcpy (&future_prefs->encoder,&prefs->encoder,sizeof(_encoder));
      lives_combo_populate(LIVES_COMBO(prefsw->ofmt_combo), ofmt);
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

    lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), prefs->encoder.of_desc);

    prefsw->acodec_combo = lives_combo_new();
    prefs->acodec_list=NULL;

    set_acodec_list_from_allowed(prefsw, rdet);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_encoding), hbox, FALSE, FALSE, 20);

    label = gtk_label_new (_("Audio codec"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 10);
    
    gtk_box_pack_start (GTK_BOX (hbox), prefsw->acodec_combo, TRUE, TRUE, 10);
    add_fill_to_box (GTK_BOX (hbox));
    gtk_widget_show_all (hbox);
  }
  else prefsw->acodec_combo=NULL;

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_encoding.png", NULL);
  pixbuf_encoding = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_encoding, _("Encoding"), LIST_ENTRY_ENCODING);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_encoding);

  // ---------------,
  // effects        |
  // ---------------'

  prefsw->vbox_right_effects = gtk_vbox_new (FALSE, 20);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_effects), 20);
  
  prefsw->scrollw_right_effects = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_effects), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_effects), 
					 prefsw->vbox_right_effects);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_effects)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_effects)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hbox, FALSE, FALSE, 10);

  prefsw->checkbutton_antialias = lives_standard_check_button_new(_("Use _antialiasing when resizing"),TRUE,LIVES_BOX(hbox),NULL);
  gtk_widget_show_all(hbox);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_antialias), prefs->antialias);
  //

  hbox = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hbox, TRUE, TRUE, 0);
  
  prefsw->spinbutton_rte_keys = lives_standard_spin_button_new 
    ((tmp=g_strdup(_("Number of _real time effect keys"))),TRUE,prefs->rte_keys_virtual, FX_KEYS_PHYSICAL, 
     FX_KEYS_MAX_VIRTUAL, 1., 1., 0, LIVES_BOX(hbox),
    (tmp2=g_strdup(_("The number of \"virtual\" real time effect keys. They can be controlled through the real time effects window, or via network (OSC)."))));
  g_free(tmp);
  g_free(tmp2);

  gtk_widget_show_all(hbox);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hbox, FALSE, FALSE, 10);

  prefsw->checkbutton_threads = lives_standard_check_button_new(_("Use _threads where possible when applying effects"),TRUE,LIVES_BOX(hbox),NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_threads), future_prefs->nfx_threads>1);
  gtk_widget_show_all (hbox);

  //

  hbox = gtk_hbox_new (FALSE,0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hbox, TRUE, TRUE, 0);

  prefsw->spinbutton_nfx_threads = lives_standard_spin_button_new (_("Number of _threads"),TRUE,future_prefs->nfx_threads, 2., 65536., 1., 1., 0, 
								   LIVES_BOX(hbox),NULL);

  gtk_widget_show_all (hbox);

  if (future_prefs->nfx_threads==1) gtk_widget_set_sensitive(prefsw->spinbutton_nfx_threads,FALSE);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hseparator, FALSE, FALSE, 0);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_effects), hbox, FALSE, FALSE, 0);

  label = lives_standard_label_new (_("Restart is required if any of the following paths are changed:"));

  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 16);
  // ---
  add_fill_to_box(LIVES_BOX(hbox));

  gtk_widget_show_all(hbox);




  prefsw->wpp_entry=lives_standard_entry_new(_("Weed plugin path"),TRUE,prefs->weed_plugin_path,40,PATH_MAX,LIVES_BOX(prefsw->vbox_right_effects),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(prefsw->wpp_entry));

  prefsw->frei0r_entry=lives_standard_entry_new(_("Frei0r plugin path"),TRUE,prefs->frei0r_path,40,PATH_MAX,LIVES_BOX(prefsw->vbox_right_effects),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(prefsw->frei0r_entry));

  prefsw->ladspa_entry=lives_standard_entry_new(_("LADSPA plugin path"),TRUE,prefs->ladspa_path,40,PATH_MAX,LIVES_BOX(prefsw->vbox_right_effects),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(prefsw->ladspa_entry));


  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_effects.png", NULL);
  pixbuf_effects = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_effects, _("Effects"), LIST_ENTRY_EFFECTS);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_effects);

  // -------------------,
  // Directories        |
  // -------------------'

  prefsw->table_right_directories = gtk_table_new (10, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->table_right_directories), 20);
  
  prefsw->scrollw_right_directories = gtk_scrolled_window_new (NULL, NULL);
  
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_directories), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_directories), 
					 prefsw->table_right_directories);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_directories)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_directories)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }


  label39 = gtk_label_new (_("      Video load directory (default)      "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label39, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label39);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label39, 0, 1, 4, 5,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label39), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label39), 0, 0.5);
  
  label40 = gtk_label_new (_("      Video save directory (default) "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label40, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label40);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label40, 0, 1, 5, 6,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label40), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label40), 0, 0.5);
  
  label41 = gtk_label_new (_("      Audio load directory (default) "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label41, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label41);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label41, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label41), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label41), 0, 0.5);
  
  label42 = gtk_label_new (_("      Image directory (default) "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label42, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label42);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label42, 0, 1, 7, 8,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label42), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label42), 0, 0.5);
  
  label52 = gtk_label_new (_("      Backup/Restore directory (default) "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label52, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label52);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label52, 0, 1, 8, 9,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label52), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label52), 0, 0.5);
  
  label43 = gtk_label_new (_("      Temp directory (do not remove) "));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label43, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label43);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label43, 0, 1, 3, 4,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label43), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label43), 0, 0.5);

  prefsw->vid_load_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->vid_load_dir_entry),255);
  gtk_widget_show (prefsw->vid_load_dir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->vid_load_dir_entry, 1, 2, 4, 5,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);

  gtk_widget_set_tooltip_text( prefsw->vid_load_dir_entry, _("The default directory for loading video clips from"));



  // tempdir warning label


  label = gtk_label_new ("");

  set_temp_label_text(GTK_LABEL(label));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), label, 0, 3, 0, 2,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.65);
 
  prefsw->temp_label=label;








 
  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->vid_load_dir_entry),prefs->def_vid_load_dir);
  
  prefsw->vid_save_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->vid_save_dir_entry),PATH_MAX);
  gtk_widget_show (prefsw->vid_save_dir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->vid_save_dir_entry, 1, 2, 5, 6,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  gtk_widget_set_tooltip_text( prefsw->vid_save_dir_entry, 
			       _("The default directory for saving encoded clips to"));

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->vid_save_dir_entry),prefs->def_vid_save_dir);
  
  prefsw->audio_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->audio_dir_entry),PATH_MAX);
  gtk_widget_show (prefsw->audio_dir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->audio_dir_entry, 1, 2, 6, 7,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  gtk_widget_set_tooltip_text( prefsw->audio_dir_entry, 
			       _("The default directory for loading and saving audio"));

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->audio_dir_entry),prefs->def_audio_dir);
   
  prefsw->image_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->image_dir_entry),PATH_MAX);
  gtk_widget_show (prefsw->image_dir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->image_dir_entry, 1, 2, 7, 8,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
   
  gtk_widget_set_tooltip_text( prefsw->image_dir_entry, 
			       _("The default directory for saving frameshots to"));

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->image_dir_entry),prefs->def_image_dir);
   
  prefsw->proj_dir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->proj_dir_entry),PATH_MAX);
  gtk_widget_show (prefsw->proj_dir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->proj_dir_entry, 1, 2, 8, 9,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
   
  gtk_widget_set_tooltip_text( prefsw->proj_dir_entry, 
			       _("The default directory for backing up/restoring single clips"));

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->proj_dir_entry),prefs->def_proj_dir);
   
  prefsw->tmpdir_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->tmpdir_entry),PATH_MAX);
  gtk_widget_show (prefsw->tmpdir_entry);
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), prefsw->tmpdir_entry, 1, 2, 3, 4,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
   
  gtk_widget_set_tooltip_text( prefsw->tmpdir_entry, _("LiVES working directory."));

  // get from prefs
  gtk_entry_set_text(GTK_ENTRY(prefsw->tmpdir_entry),(tmp=g_filename_to_utf8(future_prefs->tmpdir,-1,NULL,NULL,NULL)));
  g_free(tmp);
   
  dirbutton1 = gtk_button_new ();
  gtk_widget_show (dirbutton1);
   
   
  dirimage1 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage1);
  gtk_container_add (GTK_CONTAINER (dirbutton1), dirimage1);
   
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton1, 2, 3, 4, 5,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);
   
  dirbutton2 = gtk_button_new ();
  gtk_widget_show (dirbutton2);
   
  dirimage2 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage2);
  gtk_container_add (GTK_CONTAINER (dirbutton2), dirimage2);
   
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton2, 2, 3, 5, 6,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);
   
  dirbutton3 = gtk_button_new ();
  gtk_widget_show (dirbutton3);
   
  dirimage3 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage3);
  gtk_container_add (GTK_CONTAINER (dirbutton3), dirimage3);
   
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton3, 2, 3, 6, 7,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);
   
  dirbutton4 = gtk_button_new ();
  gtk_widget_show (dirbutton4);
   
  dirimage4 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage4);
  gtk_container_add (GTK_CONTAINER (dirbutton4), dirimage4);
   
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton4, 2, 3, 7, 8,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);

  dirbutton5 = gtk_button_new ();
  gtk_widget_show (dirbutton5);
   
  dirimage5 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage5);
  gtk_container_add (GTK_CONTAINER (dirbutton5), dirimage5);
  
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton5, 2, 3, 8, 9,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);
  
  dirbutton6 = gtk_button_new ();
  gtk_widget_show (dirbutton6);
   
  dirimage6 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (dirimage6);
  gtk_container_add (GTK_CONTAINER (dirbutton6), dirimage6);
   
  gtk_table_attach (GTK_TABLE (prefsw->table_right_directories), dirbutton6, 2, 3, 3, 4,
		    (GtkAttachOptions) (0),
		    (GtkAttachOptions) (0), 0, 0);
   
  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_directory.png", NULL);
  pixbuf_directories = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_directories, _("Directories"), LIST_ENTRY_DIRECTORIES);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_directories);

  // ---------------,
  // Warnings       |
  // ---------------'
   
  prefsw->vbox_right_warnings = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (prefsw->vbox_right_warnings);

  prefsw->scrollw_right_warnings = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_warnings), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_warnings), 
					 prefsw->vbox_right_warnings);

  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_warnings)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_warnings)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_warnings), 20);
   

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 20);
  // ---
  label = gtk_label_new_with_mnemonic(_("Warn if diskspace falls below: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new ((gdouble)prefs->ds_warn_level/1000000., 
				       (gdouble)prefs->ds_crit_level/1000000.,
				       DS_WARN_CRIT_MAX, 1., 10., 0.);
   
  prefsw->spinbutton_warn_ds = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_warn_ds);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_warn_ds, FALSE, TRUE, 0);
   
  label = gtk_label_new (_ (" MB [set to 0 to disable]"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  gtk_widget_show_all(hbox);
   

  // ---

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 20);
  // ---
  label = gtk_label_new_with_mnemonic(_("Diskspace critical level: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);


  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new ((gdouble)prefs->ds_crit_level/1000000., 0.,
				       DS_WARN_CRIT_MAX, 1., 10., 0.);
   
  prefsw->spinbutton_crit_ds = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_crit_ds);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_crit_ds, FALSE, TRUE, 0);
   
  label = gtk_label_new (_ (" MB [set to 0 to disable]"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  gtk_widget_show_all(hbox);


  // ---

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hseparator, FALSE, FALSE, 20);



  prefsw->checkbutton_warn_fps = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn on Insert / Merge if _frame rate of clipboard does not match frame rate of selection"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_fps);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_fps);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_fps, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_fps), !(prefs->warning_mask&WARN_MASK_FPS));
  // ---
  hbox100 = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox100);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox100, FALSE, TRUE, 0);
  // ---
  prefsw->checkbutton_warn_fsize = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn on Open if file _size exceeds "));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_fsize);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_fsize);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_fsize, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox100), hbox, FALSE, TRUE, 0);
   
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_fsize), !(prefs->warning_mask&WARN_MASK_FSIZE));
  spinbutton_warn_fsize_adj = (GObject *)gtk_adjustment_new (prefs->warn_file_size, 1, 2048, 1, 10, 0);
   
  prefsw->spinbutton_warn_fsize = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_warn_fsize_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_warn_fsize);
  gtk_box_pack_start (GTK_BOX (hbox100), prefsw->spinbutton_warn_fsize, FALSE, TRUE, 0);
   
  label100 = gtk_label_new (_ (" MB"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label100, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label100);
  gtk_box_pack_start (GTK_BOX (hbox100), label100, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label100), GTK_JUSTIFY_LEFT);
  // ---
  prefsw->checkbutton_warn_save_set = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning before saving a se_t"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_save_set);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_save_set);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_save_set, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_save_set), !(prefs->warning_mask&WARN_MASK_SAVE_SET));
  // ---
  prefsw->checkbutton_warn_mplayer = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning if _mplayer, sox, composite or convert is not found when LiVES is started."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_mplayer);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_mplayer);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_mplayer, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mplayer), !(prefs->warning_mask&WARN_MASK_NO_MPLAYER));
  // ---
  prefsw->checkbutton_warn_rendered_fx = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning if no _rendered effects are found at startup."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_rendered_fx);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_rendered_fx);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_rendered_fx, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_rendered_fx), !(prefs->warning_mask&WARN_MASK_RENDERED_FX));
  // ---
  prefsw->checkbutton_warn_encoders = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning if no _encoder plugins are found at startup."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_encoders);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_encoders);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_encoders, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_rendered_fx), !(prefs->warning_mask&WARN_MASK_NO_ENCODERS));
  // ---
  prefsw->checkbutton_warn_dup_set = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning if a _duplicate set name is entered."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_dup_set);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_dup_set);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_dup_set, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_dup_set), !(prefs->warning_mask&WARN_MASK_DUPLICATE_SET));
  // ---
  prefsw->checkbutton_warn_layout_clips = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("When a set is loaded, warn if clips are missing from _layouts."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_clips);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_clips);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_clips, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_widget_show (prefsw->checkbutton_warn_layout_clips);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_clips), !(prefs->warning_mask&WARN_MASK_LAYOUT_MISSING_CLIPS));
  // ---
  prefsw->checkbutton_warn_layout_close = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if a clip used in a layout is about to be closed."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_close);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_close);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_close, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_close), !(prefs->warning_mask&WARN_MASK_LAYOUT_CLOSE_FILE));
  // ---
  prefsw->checkbutton_warn_layout_delete = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if frames used in a layout are about to be deleted."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_delete);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_delete);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_delete, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_delete), !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES));
  // ---
  prefsw->checkbutton_warn_layout_shift = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if frames used in a layout are about to be shifted."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_shift);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_shift);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_shift, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_shift), !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES));
  // ---
  prefsw->checkbutton_warn_layout_alter = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if frames used in a layout are about to be altered."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_alter);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_alter);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_alter, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_alter), !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES));
  // ---
  prefsw->checkbutton_warn_layout_adel = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if audio used in a layout is about to be deleted."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_adel);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_adel);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_adel, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_adel), !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO));
  // ---
  prefsw->checkbutton_warn_layout_ashift = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if audio used in a layout is about to be shifted."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_ashift);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_ashift);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_ashift, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_ashift), !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO));
  // ---
  prefsw->checkbutton_warn_layout_aalt = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if audio used in a layout is about to be altered."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_aalt);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_aalt);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_aalt, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_aalt), !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO));
  // ---
  prefsw->checkbutton_warn_layout_popup = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Popup layout errors after clip changes."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_layout_popup);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_layout_popup);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_layout_popup, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_layout_popup), !(prefs->warning_mask&WARN_MASK_LAYOUT_POPUP));
  // ---
  prefsw->checkbutton_warn_discard_layout = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if the layout has not been saved when leaving multitrack mode."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_discard_layout);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_discard_layout);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_discard_layout, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_discard_layout), !(prefs->warning_mask&WARN_MASK_EXIT_MT));
  // ---
  prefsw->checkbutton_warn_mt_achans = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if multitrack has no audio channels, and a layout with audio is loaded."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_mt_achans);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_mt_achans);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_mt_achans, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_achans), !(prefs->warning_mask&WARN_MASK_MT_ACHANS));
  // ---
  prefsw->checkbutton_warn_mt_no_jack = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Warn if multitrack has audio channels, and your audio player is not \"jack\" or \"pulse audio\"."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_mt_no_jack);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_mt_no_jack);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_mt_no_jack, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_no_jack), !(prefs->warning_mask&WARN_MASK_MT_NO_JACK));
  // ---
  prefsw->checkbutton_warn_after_dvgrab = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show info message after importing from firewire device."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_after_dvgrab);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_after_dvgrab);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_after_dvgrab, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
#ifdef HAVE_LDVGRAB
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
#endif   
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_after_dvgrab), !(prefs->warning_mask&WARN_MASK_AFTER_DVGRAB));
  // ---
  prefsw->checkbutton_warn_yuv4m_open = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning before opening a yuv4mpeg stream (advanced)."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_yuv4m_open);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_yuv4m_open);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_yuv4m_open, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
#ifdef HAVE_YUV4MPEG
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
#endif   
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_yuv4m_open), !(prefs->warning_mask&WARN_MASK_OPEN_YUV4M));
  // ---
  prefsw->checkbutton_warn_mt_backup_space = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning when multitrack is low on backup space."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_mt_backup_space);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_mt_backup_space);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_mt_backup_space, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_mt_backup_space), !(prefs->warning_mask&WARN_MASK_MT_BACKUP_SPACE));
  // ---
  prefsw->checkbutton_warn_after_crash = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning advising cleaning of disk space after a crash."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_after_crash);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_after_crash);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_after_crash, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_after_crash), !(prefs->warning_mask&WARN_MASK_CLEAN_AFTER_CRASH));

  // ---
  prefsw->checkbutton_warn_no_pulse = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Show a warning if unable to connect to pulseaudio player."));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_warn_no_pulse);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_warn_no_pulse);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_warn_no_pulse, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_warnings), hbox, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_warn_no_pulse), !(prefs->warning_mask&WARN_MASK_NO_PULSE_CONNECT));
  // ---


  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_warning.png", NULL);
  pixbuf_warnings = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_warnings, _("Warnings"), LIST_ENTRY_WARNINGS);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_warnings);

  // -----------,
  // Misc       |
  // -----------'
   
  prefsw->vbox_right_misc = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_misc), 20);
   
  prefsw->scrollw_right_misc = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_misc), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_misc), prefsw->vbox_right_misc);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_misc)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_misc)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  prefsw->check_midi = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Midi synch (requires the files midistart and midistop)"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->check_midi);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->check_midi);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->check_midi, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_misc), hbox, FALSE, FALSE, 16);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->check_midi), prefs->midisynch);

  gtk_widget_set_sensitive(prefsw->check_midi,capable->has_midistartstop);
  gtk_widget_set_sensitive(label,capable->has_midistartstop);
   
  hbox99 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox99);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_misc), hbox99, TRUE, TRUE, 0);
   
  label97 = gtk_label_new (_("When inserting/merging frames:  "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label97, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label97);
  gtk_box_pack_start (GTK_BOX (hbox99), label97, FALSE, FALSE, 16);
  gtk_label_set_justify (GTK_LABEL (label97), GTK_JUSTIFY_LEFT);
   
  prefsw->ins_speed = gtk_radio_button_new(rb_group2);
  rb_group2 = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->ins_speed));
  label = gtk_label_new_with_mnemonic(_("_Speed Up/Slow Down Insertion"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->ins_speed);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->ins_speed);
  gtk_box_pack_start (GTK_BOX (hbox99), prefsw->ins_speed, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox99), eventbox, FALSE, FALSE, 5);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  // ---
  ins_resample = gtk_radio_button_new(rb_group2);
  rb_group2 = gtk_radio_button_get_group (GTK_RADIO_BUTTON (ins_resample));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(ins_resample),prefs->ins_resample);
  label = gtk_label_new_with_mnemonic(_("_Resample Insertion"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->ins_speed);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), ins_resample);
  gtk_box_pack_start (GTK_BOX (hbox99), ins_resample, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox99), eventbox, FALSE, FALSE, 5);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_container_set_border_width(GTK_CONTAINER (hbox99), 20);
  gtk_widget_show_all(hbox99);
  // ---
  prefsw->check_xmms_pause = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Pause xmms during audio playback"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->check_xmms_pause);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->check_xmms_pause);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->check_xmms_pause, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, 10);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->check_xmms_pause), prefs->pause_xmms);
   
  if (capable->has_xmms) {
    gtk_widget_show_all(hbox);
  }
  // ---  
  hbox19 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_misc), hbox19, TRUE, TRUE, 0);
   
  label134 = gtk_label_new (_("CD device           "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label134, GTK_STATE_NORMAL, &palette->normal_fore);
  }
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

  g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",
		    G_CALLBACK (on_fileread_clicked),(gpointer)prefsw->cdplay_entry);

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
   
  gtk_widget_set_tooltip_text( prefsw->cdplay_entry, _("LiVES can load audio tracks from this CD"));
   
  hbox13 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox13);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_misc), hbox13, TRUE, TRUE, 0);
   
  label44 = gtk_label_new (_("Default FPS        "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label44, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label44);
  gtk_box_pack_start (GTK_BOX (hbox13), label44, FALSE, FALSE, 18);
  gtk_label_set_justify (GTK_LABEL (label44), GTK_JUSTIFY_LEFT);
   
  spinbutton_def_fps_adj = (GObject *)gtk_adjustment_new (prefs->default_fps, 1, 2048, 1, 10, 0);
   
  prefsw->spinbutton_def_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_def_fps_adj), 1, 2);
  gtk_widget_show (prefsw->spinbutton_def_fps);
  gtk_box_pack_start (GTK_BOX (hbox13), prefsw->spinbutton_def_fps, FALSE, TRUE, 0);
  gtk_widget_set_tooltip_text( prefsw->spinbutton_def_fps, _("Frames per second to use when none is specified"));
   
  icon = g_strdup_printf("%s%s/pref_misc.png", prefs->prefix_dir, ICON_DIR);
  pixbuf_misc = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_misc, _("Misc"), LIST_ENTRY_MISC);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_misc);

  // -----------,
  // Themes     |
  // -----------'
   
  prefsw->vbox_right_themes = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_themes), 20);
   
  prefsw->scrollw_right_themes = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_themes), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
   
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_themes), prefsw->vbox_right_themes);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_themes)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_themes)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox93 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox93);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_themes), hbox93, TRUE, FALSE, 0);
   
  label94 = gtk_label_new (_("New theme:           "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label94, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label94);
  gtk_box_pack_start (GTK_BOX (hbox93), label94, FALSE, FALSE, 40);
  gtk_label_set_justify (GTK_LABEL (label94), GTK_JUSTIFY_LEFT);
   
  prefsw->theme_combo = lives_combo_new();
   
  // scan for themes
  themes = get_plugin_list(PLUGIN_THEMES, TRUE, NULL, NULL);
  themes = g_list_prepend(themes, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));
   
  lives_combo_populate(LIVES_COMBO(prefsw->theme_combo), themes);

  gtk_box_pack_start (GTK_BOX (hbox93), prefsw->theme_combo, FALSE, FALSE, 0);
  gtk_widget_show(prefsw->theme_combo);
   
  if (g_ascii_strcasecmp(future_prefs->theme, "none")) {
    theme = g_strdup(future_prefs->theme);
  }
  else theme = g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  // ---
  lives_combo_set_active_string(LIVES_COMBO(prefsw->theme_combo), theme);
  //---
  g_free(theme);
  g_list_free_strings (themes);
  g_list_free (themes);
   
  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_themes.png", NULL);
  pixbuf_themes = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_themes, _("Themes"), LIST_ENTRY_THEMES);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_themes);

  // --------------------------,
  // streaming/networking      |
  // --------------------------'

  prefsw->vbox_right_net = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_net), 20);

  prefsw->scrollw_right_net = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_net), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
   
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_net), prefsw->vbox_right_net);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_net)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_net)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox94 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox94);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_net), hbox94, FALSE, FALSE, 20);
   
  label88 = gtk_label_new (_("Download bandwidth (Kb/s)       "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label88, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label88);
  gtk_box_pack_start (GTK_BOX (hbox94), label88, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label88), GTK_JUSTIFY_LEFT);
   
  spinbutton_bwidth_adj = (GObject *)gtk_adjustment_new (prefs->dl_bandwidth, 0, 100000, 1, 10, 0);
   
  prefsw->spinbutton_bwidth = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_bwidth_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_bwidth);
  gtk_box_pack_start (GTK_BOX (hbox94), prefsw->spinbutton_bwidth, FALSE, TRUE, 0);
   
  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_net), hseparator, FALSE, FALSE, 20);
   
#ifndef ENABLE_OSC
  label = gtk_label_new (_("LiVES must be compiled without \"configure --disable-OSC\" to use OMC"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_net), label, FALSE, FALSE, 0);
#endif
   
  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_net), hbox1, FALSE, FALSE, 20);
   
  prefsw->enable_OSC = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("OMC remote control enabled"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->enable_OSC);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->enable_OSC);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->enable_OSC, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX (hbox1), hbox, FALSE, FALSE, 0);
#ifndef ENABLE_OSC
  gtk_widget_set_sensitive (prefsw->enable_OSC,FALSE);
  gtk_widget_set_sensitive (label,FALSE);
#endif
  // ---
  label = gtk_label_new (_("UDP port       "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 20);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->osc_udp_port, 1, 65535, 1, 10, 0);
   
  prefsw->spinbutton_osc_udp = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (prefsw->spinbutton_osc_udp);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_osc_udp, FALSE, TRUE, 0);
  // ---
  prefsw->enable_OSC_start = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Start OMC on startup"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->enable_OSC_start);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->enable_OSC_start);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->enable_OSC_start, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_net), hbox, FALSE, FALSE, 20);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->enable_OSC_start), future_prefs->osc_start);
   
#ifndef ENABLE_OSC
  gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,FALSE);
  gtk_widget_set_sensitive (prefsw->enable_OSC_start,FALSE);
  gtk_widget_set_sensitive (label,FALSE);
#else
  if (prefs->osc_udp_started) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->enable_OSC), TRUE);
    gtk_widget_set_sensitive (prefsw->spinbutton_osc_udp,FALSE);
    gtk_widget_set_sensitive (prefsw->enable_OSC,FALSE);
  }
#endif
   
  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_net.png", NULL);
  pixbuf_net = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_net, _("Streaming/Networking"), LIST_ENTRY_NET);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_net);

   
  // ----------,
  // jack      |
  // ----------'

  prefsw->vbox_right_jack = gtk_vbox_new (FALSE, 20);
  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_jack), 20);

  prefsw->scrollw_right_jack = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_jack), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
   
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_jack), prefsw->vbox_right_jack);
   
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_jack)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_jack)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }


  label = gtk_label_new (_("Jack transport"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);

  prefsw->jack_tserver_entry = gtk_entry_new ();
  prefsw->checkbutton_start_tjack = gtk_check_button_new();
  prefsw->checkbutton_jack_master = gtk_check_button_new();
  prefsw->checkbutton_jack_client = gtk_check_button_new();
  prefsw->checkbutton_jack_tb_start = gtk_check_button_new();
  prefsw->checkbutton_jack_tb_client = gtk_check_button_new();
   
#ifndef ENABLE_JACK_TRANSPORT
  label = gtk_label_new (_("LiVES must be compiled with jack/transport.h and jack/jack.h present to use jack transport"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);
#else
  hbox1 = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hbox1, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Jack _transport config file"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 10);

  gtk_entry_set_max_length(GTK_ENTRY(prefsw->jack_tserver_entry),255);

  gtk_entry_set_text(GTK_ENTRY(prefsw->jack_tserver_entry),prefs->jack_tserver);
  gtk_widget_set_sensitive(prefsw->jack_tserver_entry,FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->jack_tserver_entry);

  gtk_widget_show (prefsw->jack_tserver_entry);
  gtk_box_pack_start (GTK_BOX (hbox1), prefsw->jack_tserver_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text( prefsw->jack_tserver_entry, _("The name of the jack server which can control LiVES transport"));

  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Start _server on LiVES startup"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_start_tjack);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_start_tjack);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_start_tjack, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_start_tjack), 
				(future_prefs->jack_opts&JACK_OPTS_START_TSERVER)?TRUE:FALSE);
  // ---
  hbox2 = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hbox2, TRUE, TRUE, 0);
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Jack transport _master (start and stop)"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_jack_master);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_jack_master);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_jack_master, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox2), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_master), 
				(future_prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER)?TRUE:FALSE);
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Jack transport _client (start and stop)"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_jack_client);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_jack_client);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_jack_client, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox2), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_client), 
				(future_prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)?TRUE:FALSE);
  // ---
  hbox3 = gtk_hbox_new (FALSE,0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hbox3, TRUE, TRUE, 0);
  add_fill_to_box(GTK_BOX(hbox3));
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Jack transport sets start position"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_jack_tb_start);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_jack_tb_start);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_jack_tb_start, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_tb_start),
				(future_prefs->jack_opts&JACK_OPTS_TIMEBASE_START)?
				(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_client))):FALSE);

  gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_start, 
			   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_client)));
  gtk_widget_set_sensitive(label, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_client)));

  g_signal_connect_after (GTK_OBJECT (prefsw->checkbutton_jack_client), "toggled",
			  G_CALLBACK (after_jack_client_toggled),
			  NULL);
  // ---
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Jack transport timebase slave"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_jack_tb_client);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_jack_tb_client);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_jack_tb_client, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox3), hbox, TRUE, FALSE, 0);
   
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_tb_client), 
				(future_prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)?
				(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start))):FALSE);

  gtk_widget_set_sensitive(prefsw->checkbutton_jack_tb_client, 
			   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)));
  gtk_widget_set_sensitive(label, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)));
  // ---
  label = gtk_label_new (_("(See also Playback -> Audio follows video rate/direction)"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);

  g_signal_connect_after (GTK_OBJECT (prefsw->checkbutton_jack_tb_start), "toggled",
			  G_CALLBACK (after_jack_tb_start_toggled),
			  NULL);

  //add_fill_to_box(GTK_BOX(hbox));
  //gtk_widget_show_all (hbox);

#endif

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hseparator, FALSE, TRUE, 0);

  label = gtk_label_new (_("Jack audio"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);

#ifndef ENABLE_JACK
  label = gtk_label_new (_("LiVES must be compiled with jack/jack.h present to use jack audio"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);
#else
  label = gtk_label_new (_("You MUST set the audio player to \"jack\" in the Playback tab to use jack audio"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), label, FALSE, FALSE, 10);

  hbox4 = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox4);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hbox4, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("Jack _audio server config file"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox4), label, FALSE, FALSE, 10);

  prefsw->jack_aserver_entry = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->jack_aserver_entry),255);

  gtk_entry_set_text(GTK_ENTRY(prefsw->jack_aserver_entry),prefs->jack_aserver);
  gtk_widget_set_sensitive(prefsw->jack_aserver_entry,FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->jack_aserver_entry);

  gtk_widget_show (prefsw->jack_aserver_entry);
  gtk_box_pack_start (GTK_BOX (hbox4), prefsw->jack_aserver_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text( prefsw->jack_aserver_entry, _("The name of the jack server for audio"));
  // ---
  hbox5 = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_jack), hbox5, TRUE, TRUE, 0);
  // ---
  prefsw->checkbutton_start_ajack = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Start _server on LiVES startup"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_start_ajack);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_start_ajack);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_start_ajack, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, TRUE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_start_ajack), 
				(future_prefs->jack_opts&JACK_OPTS_START_ASERVER)?TRUE:FALSE);
  // ---
  prefsw->checkbutton_jack_pwp = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("Play audio even when transport is _paused"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_jack_pwp);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_jack_pwp);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_jack_pwp, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start (GTK_BOX (hbox5), hbox, TRUE, FALSE, 0);
   
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_jack_pwp), 
				(future_prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE);

  gtk_widget_set_sensitive (prefsw->checkbutton_jack_pwp, prefs->audio_player==AUD_PLAYER_JACK);
  gtk_widget_set_sensitive (label, prefs->audio_player==AUD_PLAYER_JACK);
#endif

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_jack.png", NULL);
  pixbuf_jack = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_jack, _("Jack Integration"), LIST_ENTRY_JACK);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_jack);

  // ----------------------,
  // MIDI/js learner       |
  // ----------------------'


  // TODO - copy pattern to all
  prefsw->vbox_right_midi = gtk_vbox_new (FALSE, 10);

  prefsw->scrollw_right_midi = gtk_scrolled_window_new (NULL, NULL);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_midi), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (prefsw->scrollw_right_midi), prefsw->vbox_right_midi);

  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(prefsw->scrollw_right_midi)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(prefsw->scrollw_right_midi)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_container_set_border_width (GTK_CONTAINER (prefsw->vbox_right_midi), 20);

  label = gtk_label_new (_("Events to respond to:"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), label, FALSE, FALSE, 10);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  prefsw->checkbutton_omc_js = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("_Joystick events"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_omc_js);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->checkbutton_omc_js);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_omc_js, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_omc_js), prefs->omc_dev_opts&OMC_DEV_JS);
  // ---
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);
   
  label = gtk_label_new_with_mnemonic (_("_Joystick device"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 18);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  prefsw->omc_js_entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->omc_js_entry);
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->omc_js_entry),255);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->omc_js_entry, TRUE, TRUE, 20);
  gtk_widget_show (prefsw->omc_js_entry);
  if (strlen(prefs->omc_js_fname)!=0) gtk_entry_set_text (GTK_ENTRY (prefsw->omc_js_entry),prefs->omc_js_fname);
   
  gtk_widget_set_tooltip_text( prefsw->omc_js_entry, _("The joystick device, e.g. /dev/input/js0"));

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
   
  g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),
		    (gpointer)prefsw->omc_js_entry);

#endif

#ifdef OMC_MIDI_IMPL

  prefsw->checkbutton_omc_midi = gtk_check_button_new();
  eventbox = gtk_event_box_new();
  label = gtk_label_new_with_mnemonic(_("_MIDI events"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->checkbutton_omc_midi);
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), 
		   prefsw->checkbutton_omc_midi);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), prefsw->checkbutton_omc_midi, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 5);
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefsw->checkbutton_omc_midi), prefs->omc_dev_opts&OMC_DEV_MIDI);
  // ---
  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox1, TRUE, TRUE, 20);

#ifdef ALSA_MIDI
  gtk_widget_show (hbox1);
#endif

  prefsw->alsa_midi = gtk_radio_button_new(alsa_midi_group);
  alsa_midi_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefsw->alsa_midi));
  gtk_widget_set_tooltip_text( prefsw->alsa_midi, 
			       (_("Create an ALSA MIDI port which other MIDI devices can be connected to")));
  label = gtk_label_new_with_mnemonic(_("Use _ALSA MIDI (recommended)"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefsw->alsa_midi);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), prefsw->alsa_midi);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->alsa_midi, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(hbox1), hbox, TRUE, TRUE, 0);
  // ---
  raw_midi_button = gtk_radio_button_new(alsa_midi_group);
  alsa_midi_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (raw_midi_button));
  label = gtk_label_new_with_mnemonic(_("Use _raw MIDI"));
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), raw_midi_button);
  eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);
  g_signal_connect(GTK_OBJECT(eventbox), "button_press_event", G_CALLBACK(label_act_toggle), raw_midi_button);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), raw_midi_button, TRUE, TRUE, 10);
  gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 10);
  gtk_widget_set_tooltip_text( raw_midi_button, (_("Read directly from the MIDI device")));
  gtk_widget_show_all(hbox);
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 20);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, TRUE, TRUE, 0);

#ifdef ALSA_MIDI
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (raw_midi_button),!prefs->use_alsa_midi);
#endif
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);
   
  label = gtk_label_new_with_mnemonic (_("_MIDI device"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 18);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   
  prefsw->omc_midi_entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->omc_midi_entry);
  gtk_entry_set_max_length(GTK_ENTRY(prefsw->omc_midi_entry),255);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->omc_midi_entry, TRUE, TRUE, 20);
  gtk_widget_show (prefsw->omc_midi_entry);
  if (strlen(prefs->omc_midi_fname)!=0) gtk_entry_set_text (GTK_ENTRY (prefsw->omc_midi_entry),prefs->omc_midi_fname);

  gtk_widget_set_tooltip_text( prefsw->omc_midi_entry, _("The MIDI device, e.g. /dev/input/midi0"));

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

  g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",
		    G_CALLBACK (on_fileread_clicked),(gpointer)prefsw->omc_midi_entry);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hseparator, FALSE, TRUE, 10);
   
  label = gtk_label_new (_("Advanced"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), label, FALSE, FALSE, 10);

  hbox = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, TRUE, TRUE, 0);
   
  label = gtk_label_new_with_mnemonic (_("MIDI check _rate"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
   
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->midi_check_rate, 1, 2000, 10, 100, 0);
   
  prefsw->spinbutton_midicr = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 10, 0);
  gtk_widget_show (prefsw->spinbutton_midicr);
  gtk_box_pack_start (GTK_BOX (hbox), prefsw->spinbutton_midicr, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_midicr);
  gtk_widget_set_tooltip_text( prefsw->spinbutton_midicr, _("Number of MIDI checks per keyboard tick. Increasing this may improve MIDI responsiveness, but may slow down playback."));

  label = gtk_label_new_with_mnemonic (_("MIDI repeat"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
   
  spinbutton_adj = (GObject *)gtk_adjustment_new (prefs->midi_rpt, 1, 10000, 100, 1000, 0);
   
  prefsw->spinbutton_midirpt = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 100, 0);
  gtk_widget_show (prefsw->spinbutton_midirpt);
  gtk_box_pack_end (GTK_BOX (hbox), prefsw->spinbutton_midirpt, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),prefsw->spinbutton_midirpt);
  gtk_widget_set_tooltip_text( prefsw->spinbutton_midirpt, _("Number of non-reads allowed between succesive reads."));
  //
  hbox = gtk_hbox_new (FALSE,0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (prefsw->vbox_right_midi), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("(Warning: setting this value too high can slow down playback.)"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

#ifdef ALSA_MIDI
  g_signal_connect (GTK_OBJECT (prefsw->alsa_midi), "toggled",
		    G_CALLBACK (on_alsa_midi_toggled),
		    NULL);

  on_alsa_midi_toggled(GTK_TOGGLE_BUTTON(prefsw->alsa_midi),prefsw);
#endif

#endif
#endif

  icon = g_build_filename(prefs->prefix_dir, ICON_DIR, "pref_midi.png", NULL);
  pixbuf_midi = gdk_pixbuf_new_from_file(icon, NULL);
  g_free(icon);

  /* TRANSLATORS: please keep this string short */
  prefs_add_to_list(prefsw->prefs_list, pixbuf_midi, _("MIDI/Joystick learner"), LIST_ENTRY_MIDI);
  gtk_container_add (GTK_CONTAINER (dialog_table), prefsw->scrollw_right_midi);

  prefsw->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(prefsw->prefs_list));
  gtk_tree_selection_set_mode(prefsw->selection, GTK_SELECTION_SINGLE);

  // In multitrack mode multitrack/render settings should be selected by default!
  if (mainw->multitrack != NULL){
    select_pref_list_row(LIST_ENTRY_MULTITRACK);
  }

  // 
  // end
  //

  g_signal_connect(prefsw->selection, "changed", G_CALLBACK(on_prefDomainChanged), NULL);
  //

  dialog_action_area8 = GTK_DIALOG (prefsw->prefs_dialog)->action_area;
  gtk_widget_show (dialog_action_area8);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area8), GTK_BUTTONBOX_END);
   
  // Preferences 'Revert' button
  prefsw->cancelbutton = gtk_button_new_from_stock ("gtk-revert-to-saved");
  gtk_widget_show (prefsw->cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (prefsw->prefs_dialog), prefsw->cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (prefsw->cancelbutton, GTK_CAN_DEFAULT);
  // Set 'Close' button as inactive since there is no changes yet
  gtk_widget_set_sensitive(prefsw->cancelbutton, FALSE);
   
  // Preferences 'Apply' button
  prefsw->applybutton = gtk_button_new_from_stock ("gtk-apply");
  gtk_widget_show (prefsw->applybutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (prefsw->prefs_dialog), prefsw->applybutton, 0);
  GTK_WIDGET_SET_FLAGS (prefsw->applybutton, GTK_CAN_DEFAULT);
  // Set 'Apply' button as inactive since there is no changes yet
  gtk_widget_set_sensitive(prefsw->applybutton, FALSE);
   
  // Preferences 'Close' button
  prefsw->closebutton = gtk_button_new_from_stock ("gtk-close");
  gtk_widget_show(prefsw->closebutton);
  gtk_dialog_add_action_widget(GTK_DIALOG(prefsw->prefs_dialog), prefsw->closebutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (prefsw->closebutton, GTK_CAN_DEFAULT);

   
  g_signal_connect(dirbutton1, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->vid_load_dir_entry);
  g_signal_connect(dirbutton2, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->vid_save_dir_entry);
  g_signal_connect(dirbutton3, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->audio_dir_entry);
  g_signal_connect(dirbutton4, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->image_dir_entry);
  g_signal_connect(dirbutton5, "clicked", G_CALLBACK (on_filesel_simple_clicked),prefsw->proj_dir_entry);
  g_signal_connect(dirbutton6, "clicked", G_CALLBACK (on_filesel_complex_clicked),prefsw->tmpdir_entry);

  // Connect signals for 'Apply' button activity handling
  g_signal_connect(GTK_OBJECT(prefsw->wpp_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->frei0r_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->ladspa_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->fs_max_check), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->recent_check), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->stop_screensaver_check), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->open_maximised_check), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->show_tool), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->mouse_scroll), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_ce_maxspect), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rb_startup_ce), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rb_startup_mt), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_crit_ds), "value_changed", 
		   GTK_SIGNAL_FUNC(spinbutton_crit_ds_value_changed), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_gmoni), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_pmoni), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->forcesmon), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_stream_audio), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_rec_after_pb), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_warn_ds), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->mt_enter_prompt), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(mt_enter_defs), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_render_prompt), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_mt_def_width), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled),
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_mt_def_height), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled),
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_mt_def_fps), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->backaudio_checkbutton), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->pertrack_checkbutton), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_mt_undo_buf), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_mt_exit_render), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_mt_ab_time), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->mt_autoback_always), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->mt_autoback_never), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->mt_autoback_every), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->video_open_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_ocp), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->jpeg), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(png), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_instant_open), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_auto_deint), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_nobord), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_concat_images), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->pbq_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_show_stats), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(pp_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->audp_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->audio_command_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_afollow), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_aclips), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rdesk_audio), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rframes), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rfps), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->reffects), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rclips), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->raudio), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->rextaudio), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_rec_gb), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->encoder_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->ofmt_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);

  if (prefsw->acodec_combo!=NULL) 
    g_signal_connect(GTK_OBJECT(prefsw->acodec_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_antialias), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_rte_keys), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_threads), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_threads), "toggled", GTK_SIGNAL_FUNC(toggle_set_sensitive), 
		   (gpointer)prefsw->spinbutton_nfx_threads);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_nfx_threads), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->vid_load_dir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->vid_save_dir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->audio_dir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->image_dir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->proj_dir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->tmpdir_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_fps), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_fsize), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_warn_fsize), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_save_set), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_mplayer), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_rendered_fx), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_encoders), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_dup_set), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_clips), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_close), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_delete), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled),
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_shift), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_alter), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_adel), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_ashift), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled),
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_aalt), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_layout_popup), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled),
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_discard_layout), "toggled", 
		   GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_mt_achans), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_mt_no_jack), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_after_dvgrab), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_yuv4m_open), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_mt_backup_space), "toggled", 
		   GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_after_crash), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_warn_no_pulse), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->check_midi), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->ins_speed), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(ins_resample), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->check_xmms_pause), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->cdplay_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_def_fps), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->theme_combo), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_bwidth), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
#ifdef ENABLE_OSC
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_osc_udp), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->enable_OSC_start), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->enable_OSC), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
#endif

#ifndef ENABLE_JACK_TRANSPORT
  g_signal_connect(GTK_EDITABLE(prefsw->jack_tserver_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_start_tjack), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_jack_master), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_jack_client), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_jack_tb_start), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_jack_tb_client), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
#endif

#ifdef ENABLE_JACK
  g_signal_connect(GTK_EDITABLE(prefsw->jack_aserver_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_start_ajack), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_jack_pwp), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_omc_js), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->omc_js_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
#endif
#ifdef OMC_MIDI_IMPL
  g_signal_connect(GTK_OBJECT(prefsw->checkbutton_omc_midi), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->alsa_midi), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(raw_midi_button), "toggled", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_EDITABLE(prefsw->omc_midi_entry), "changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_midicr), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), NULL);
  g_signal_connect(GTK_OBJECT(prefsw->spinbutton_midirpt), "value_changed", GTK_SIGNAL_FUNC(apply_button_set_enabled), 
		   NULL);
#endif
#endif
   
  if (capable->has_encoder_plugins) {
    prefsw->encoder_name_fn = g_signal_connect(GTK_OBJECT(GTK_COMBO_BOX(prefsw->encoder_combo)), "changed", 
					       G_CALLBACK(on_encoder_entry_changed), NULL);
    // ---
    prefsw->encoder_ofmt_fn = g_signal_connect(GTK_OBJECT(GTK_COMBO_BOX(prefsw->ofmt_combo)), "changed", 
					       G_CALLBACK(on_encoder_ofmt_changed), NULL);
  }
   
  prefsw->audp_entry_func = g_signal_connect(GTK_OBJECT(GTK_COMBO_BOX(prefsw->audp_combo)), "changed", 
					     G_CALLBACK(on_audp_entry_changed), NULL);

#ifdef ENABLE_OSC
  g_signal_connect (GTK_OBJECT (prefsw->enable_OSC), "toggled",
		    G_CALLBACK (on_osc_enable_toggled),
		    (gpointer)prefsw->enable_OSC_start);
#endif
  g_signal_connect (GTK_OBJECT (prefsw->cancelbutton), "clicked",
		    G_CALLBACK (on_prefs_revert_clicked),
		    NULL);
   
   
  g_signal_connect (GTK_OBJECT (prefsw->closebutton), "clicked",
		    G_CALLBACK (on_prefs_close_clicked),
		    prefsw);
   
  g_signal_connect (GTK_OBJECT (prefsw->applybutton), "clicked",
		    G_CALLBACK (on_prefs_apply_clicked),
		    NULL);
   
  g_signal_connect (GTK_OBJECT (prefsw->prefs_dialog), "delete_event",
		    G_CALLBACK (on_prefs_delete_event),
		    prefsw);

  g_list_free_strings (audp);
  g_list_free (audp);


  // Get currently selected row number
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(prefsw->prefs_list));
  if (gtk_tree_selection_get_selected(prefsw->selection, &model, &iter)) {
    gtk_tree_model_get(model, &iter, LIST_NUM, &selected_idx, -1);
  }
  else{
    if (mainw->multitrack == NULL)
      selected_idx = LIST_ENTRY_GUI;
    else
      selected_idx = LIST_ENTRY_MULTITRACK;
  }

  select_pref_list_row(selected_idx);

  on_prefDomainChanged(prefsw->selection,NULL);

  return prefsw;
}


void
on_preferences_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  if (prefsw != NULL && prefsw->prefs_dialog != NULL) {
    gtk_window_present(GTK_WINDOW(prefsw->prefs_dialog));
    gdk_window_raise(prefsw->prefs_dialog->window);
    return;
  }

  future_prefs->disabled_decoders=g_list_copy_strings(prefs->disabled_decoders);

  prefsw = create_prefs_dialog();
  gtk_widget_show(prefsw->prefs_dialog);

}

/*!
 * Closes preferences dialog window
 */
void
on_prefs_close_clicked(GtkButton *button, gpointer user_data)
{
  if (prefs->acodec_list!=NULL) {
    g_list_free_strings (prefs->acodec_list);
    g_list_free (prefs->acodec_list);
  }
  prefs->acodec_list=NULL;
  g_free(prefsw->audp_name);
  g_free(prefsw->orig_audp_name);

  g_free(resaudw);
  resaudw=NULL;

  if (future_prefs->disabled_decoders!=NULL) {
    g_list_free_strings (future_prefs->disabled_decoders);
    g_list_free (future_prefs->disabled_decoders);
  }

  on_cancel_button1_clicked(button, user_data);

  prefsw=NULL;

  if (mainw->prefs_need_restart) {
    do_blocking_error_dialog(_("\nLiVES will now shut down. You need to restart it for the directory change to take effect.\nClick OK to continue.\n"));
    on_quit_activate (NULL,NULL);
  }
}

/*!
 *
 */
void
on_prefs_apply_clicked(GtkButton *button, gpointer user_data)
{
  gboolean needs_restart;

  GtkTreeIter iter;
  GtkTreeModel *model;
  guint selected_idx;
 
  // Applying preferences, so 'Apply' and 'Revert' buttons are getting disabled
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->applybutton), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->cancelbutton), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->closebutton), TRUE);

  // Get currently selected row number
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(prefsw->prefs_list));
  if (gtk_tree_selection_get_selected(prefsw->selection, &model, &iter)) {
    gtk_tree_model_get(model, &iter, LIST_NUM, &selected_idx, -1);
  }
  else{
    if (mainw->multitrack == NULL)
      selected_idx = LIST_ENTRY_GUI;
    else
      selected_idx = LIST_ENTRY_MULTITRACK;
  }
  // Apply preferences
  needs_restart = apply_prefs(FALSE);

  // do this again in case anything was changed or reverted
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->applybutton), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->cancelbutton), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(prefsw->closebutton), TRUE);

  if (FALSE == mainw->prefs_need_restart){
    mainw->prefs_need_restart = needs_restart;
  }

  if (needs_restart) {
    do_blocking_error_dialog(_("For the directory change to take effect LiVES will restart when preferences dialog closes."));
  }

  if (mainw->prefs_changed & PREFS_THEME_CHANGED) {
    do_blocking_error_dialog(_("Theme changes will not take effect until the next time you start LiVES."));
  }

  if (mainw->prefs_changed & PREFS_JACK_CHANGED) {
    do_blocking_error_dialog(_("Jack options will not take effect until the next time you start LiVES."));
  }

  mainw->prefs_changed = 0;
  // Select row, that was previously selected

  select_pref_list_row(selected_idx);

}

/*
 * Function is used to select particular row in preferences selection list
 * selection is performed according to provided index which is one of LIST_ENTRY_* constants
 */
void
select_pref_list_row(guint selected_idx)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean valid;
  guint idx;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(prefsw->prefs_list));
  valid = gtk_tree_model_get_iter_first(model, &iter);
  while(valid){
    gtk_tree_model_get(model, &iter, LIST_NUM, &idx, -1);
    //
    if (idx == selected_idx){
      gtk_tree_selection_select_iter(prefsw->selection, &iter);
      break;
    }
    //
    valid = gtk_tree_model_iter_next(model, &iter);
  }
}

void
on_prefs_revert_clicked(GtkButton *button, gpointer user_data)
{
  int i;

  if (future_prefs->vpp_argv != NULL) {
    for ( i = 0; future_prefs->vpp_argv[i] != NULL; g_free(future_prefs->vpp_argv[i++]) );

    g_free(future_prefs->vpp_argv);

    future_prefs->vpp_argv = NULL;
  }
  memset(future_prefs->vpp_name, 0, 64);

  if (prefs->acodec_list != NULL) {
    g_list_free_strings (prefs->acodec_list);
    g_list_free (prefs->acodec_list);
  }
  prefs->acodec_list = NULL;

  if (prefsw->pbq_list != NULL) {
    g_list_free(prefsw->pbq_list);
  }
  prefsw->pbq_list = NULL;

  g_free(prefsw->audp_name);
  g_free(prefsw->orig_audp_name);

  if (future_prefs->disabled_decoders != NULL) {
    g_list_free_strings (future_prefs->disabled_decoders);
    g_list_free (future_prefs->disabled_decoders);
  }

  lives_set_cursor_style(LIVES_CURSOR_BUSY,prefsw->prefs_dialog->window);
  while (g_main_context_iteration(NULL,FALSE)); // force busy cursor

  on_cancel_button1_clicked(button, prefsw);

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
  while (g_main_context_iteration(NULL,FALSE)); // force busy cursor

  prefsw = NULL;

  on_preferences_activate(NULL, NULL);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
}

gboolean
on_prefs_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  on_prefs_close_clicked(GTK_BUTTON (((_prefsw *)user_data)->closebutton), user_data);

  return FALSE;
}


gboolean lives_ask_permission(int what) {
  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    return ask_permission_dialog(what);
  default:
    LIVES_WARN("Unknown permission requested");
  }
  return FALSE;
}

