// preferences.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2016
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions dealing with getting/setting user preferences
// TODO - use atom type system for prefs

#include <dlfcn.h>

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
#include "rte_window.h"
#include "interface.h"

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

static int nmons;

static uint32_t prefs_current_page;

static void select_pref_list_row(uint32_t selected_idx);

#ifdef ENABLE_OSC
static void on_osc_enable_toggled(LiVESToggleButton *t1, livespointer t2) {
  if (prefs->osc_udp_started) return;
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp,lives_toggle_button_get_active(t1)||
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(t2)));
}
#endif

static void instopen_toggled(LiVESToggleButton *t1, LiVESWidget *button) {
  lives_widget_set_sensitive(button,lives_toggle_button_get_active(t1));
}


static int get_pref_inner(const char *filename, const char *key, char *val, int maxlen) {
  FILE *valfile;
  char *vfile;
  char *com;
  int retval;
  int alarm_handle;
  boolean timeout;

  memset(val,0,maxlen);


  if (filename==NULL) {
    if (mainw->cached_list!=NULL) {
      char *prefval=get_val_from_cached_list(key,maxlen);
      if (prefval!=NULL) {
	lives_snprintf(val,maxlen,"%s",prefval);
	lives_free(prefval);
      }
      return LIVES_RESPONSE_NONE;
    }
    com=lives_strdup_printf("%s get_pref \"%s\" %d %d",prefs->backend_sync,key,lives_getuid(),capable->mainpid);
  }
  else {
    com=lives_strdup_printf("%s get_clip_value \"%s\" %d %d \"%s\"",prefs->backend_sync,key,
                            lives_getuid(),capable->mainpid,filename);

  }
  
  if (system(com)) {
    tempdir_warning();
    lives_free(com);
    return LIVES_RESPONSE_INVALID;
  }

#ifndef IS_MINGW
  vfile=lives_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,lives_getuid(),capable->mainpid);
#else
  vfile=lives_strdup_printf("%s/smogval.%d.%d",prefs->tmpdir,lives_getuid(),capable->mainpid);
#endif

  do {
    retval=0;
    alarm_handle=lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
    timeout=FALSE;
    mainw->read_failed=FALSE;

    do {

      if (!((valfile=fopen(vfile,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
        if (!timeout) {
          if (!(mainw==NULL)) {
            weed_plant_t *frame_layer=mainw->frame_layer;
            mainw->frame_layer=NULL;
            lives_widget_context_update();
            mainw->frame_layer=frame_layer;
          }
          lives_usleep(prefs->sleep_time);
        } else break;
      } else break;
    } while (!valfile);

    lives_alarm_clear(alarm_handle);

    if (timeout) {
      retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
    } else {
      mainw->read_failed=FALSE;
      lives_fgets(val,maxlen,valfile);
      fclose(valfile);
      lives_rm(vfile);
      if (mainw->read_failed) {
        retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
      }
    }
  } while (retval==LIVES_RESPONSE_RETRY);

  lives_free(vfile);
  lives_free(com);

  return retval;
}


void get_pref(const char *key, char *val, int maxlen) {
  get_pref_inner(NULL,key,val,maxlen);
}

int get_pref_from_file(const char *filename, const char *key, char *val, int maxlen) {
  return get_pref_inner(filename,key,val,maxlen);
}


  
void get_pref_utf8(const char *key, char *val, int maxlen) {
  // get a pref in locale encoding, then convert it to utf8
  char *tmp;
  get_pref(key,val,maxlen);
  tmp=lives_filename_to_utf8(val,-1,NULL,NULL,NULL);
  lives_snprintf(val,maxlen,"%s",tmp);
  lives_free(tmp);
}



LiVESList *get_list_pref(const char *key) {
  // get a list of values from a preference
  char **array;
  char buf[65536];
  int nvals,i;

  LiVESList *retlist=NULL;

  get_pref(key,buf,65535);
  if (!strlen(buf)) return NULL;

  nvals=get_token_count(buf,'\n');
  array=lives_strsplit(buf,"\n",-1);
  for (i=0; i<nvals; i++) {
    retlist=lives_list_append(retlist,lives_strdup(array[i]));
  }

  lives_strfreev(array);

  return retlist;
}





void get_pref_default(const char *key, char *val, int maxlen) {
  FILE *valfile;
  char *vfile;
  char *com=lives_strdup_printf("%s get_pref_default \"%s\"",prefs->backend_sync,key);

  int retval;
  int alarm_handle;
  boolean timeout;

  memset(val,0,1);

  if (system(com)) {
    tempdir_warning();
    lives_free(com);
    return;
  }

#ifndef IS_MINGW
  vfile=lives_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,lives_getuid(),capable->mainpid);
#else
  vfile=lives_strdup_printf("%s/smogval.%d.%d",prefs->tmpdir,lives_getuid(),capable->mainpid);
#endif

  do {
    retval=0;
    timeout=FALSE;
    mainw->read_failed=FALSE;

    alarm_handle=lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

    do {
      if (!((valfile=fopen(vfile,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
        if (!timeout) {
          if (!(mainw==NULL)) {
            weed_plant_t *frame_layer=mainw->frame_layer;
            mainw->frame_layer=NULL;
            lives_widget_context_update();
            mainw->frame_layer=frame_layer;
          }
          lives_usleep(prefs->sleep_time);
        } else break;
      } else break;
    } while (!valfile);

    lives_alarm_clear(alarm_handle);

    if (timeout) {
      retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
    } else {
      mainw->read_failed=FALSE;
      lives_fgets(val,maxlen,valfile);
      fclose(valfile);
      lives_rm(vfile);
      if (mainw->read_failed) {
        retval=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
      }
    }
  } while (retval==LIVES_RESPONSE_RETRY);

  if (!strcmp(val,"NULL")) memset(val,0,1);

  lives_free(vfile);
  lives_free(com);
}


boolean get_boolean_pref(const char *key) {
  char buffer[16];
  get_pref(key,buffer,16);
  if (!strcmp(buffer,"true")) return TRUE;
  return FALSE;
}

int get_int_pref(const char *key) {
  char buffer[64];
  get_pref(key,buffer,64);
  if (strlen(buffer)==0) return 0;
  return atoi(buffer);
}

double get_double_pref(const char *key) {
  char buffer[64];
  get_pref(key,buffer,64);
  if (strlen(buffer)==0) return 0.;
  return strtod(buffer,NULL);
}


boolean get_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  char buffer[64];
  char **array;
  
  get_pref(key,buffer,64);
  if (strlen(buffer)==0) return FALSE;
  if (get_token_count(buffer,' ')<4) return FALSE;

  array=lives_strsplit(buffer," ",4);
  lcol->red=atoi(array[0]);
  lcol->green=atoi(array[1]);
  lcol->blue=atoi(array[2]);
  lcol->alpha=atoi(array[3]);
  lives_strfreev(array);
  
  return TRUE;
}


boolean get_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol) {
  char buffer[64];
  char **array;
  
  get_pref_from_file(themefile,key,buffer,64);
  if (strlen(buffer)==0) return FALSE;
  if (get_token_count(buffer,' ')<4) return FALSE;

  array=lives_strsplit(buffer," ",4);
  lcol->red=atoi(array[0]);
  lcol->green=atoi(array[1]);
  lcol->blue=atoi(array[2]);
  lcol->alpha=atoi(array[3]);
  lives_strfreev(array);
  
  return TRUE;
}



void delete_pref(const char *key) {
  char *com=lives_strdup_printf("%s delete_pref \"%s\"",prefs->backend_sync,key);
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}

void set_pref(const char *key, const char *value) {
  char *com=lives_strdup_printf("%s set_pref \"%s\" \"%s\"",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}


void set_int_pref(const char *key, int value) {
  char *com=lives_strdup_printf("%s set_pref \"%s\" %d",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}


void set_int64_pref(const char *key, int64_t value) {
  char *com=lives_strdup_printf("%s set_pref \"%s\" %"PRId64,prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}


void set_double_pref(const char *key, double value) {
  char *com=lives_strdup_printf("%s set_pref \"%s\" %.3f",prefs->backend_sync,key,value);
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}


void set_boolean_pref(const char *key, boolean value) {
  char *com;

  if (value) {
    com=lives_strdup_printf("%s set_pref \"%s\" true",prefs->backend_sync,key);
  } else {
    com=lives_strdup_printf("%s set_pref \"%s\" false",prefs->backend_sync,key);
  }
  if (system(com)) {
    tempdir_warning();
  }
  lives_free(com);
}



void set_list_pref(const char *key, LiVESList *values) {
  // set pref from a list of values
  LiVESList *xlist=values;
  char *string=NULL,*tmp;

  while (xlist!=NULL) {
    if (string==NULL) string=lives_strdup((char *)xlist->data);
    else {
      tmp=lives_strdup_printf("%s\n%s",string,(char *)xlist->data);
      lives_free(string);
      string=tmp;
    }
    xlist=xlist->next;
  }

  if (string==NULL) string=lives_strdup("");

  set_pref(key,string);

  lives_free(string);
}




void set_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol) {
  char *com;
  char *myval;

  myval=lives_strdup_printf("%d %d %d %d",lcol->red,lcol->green,lcol->blue,lcol->alpha);
  com=lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"",prefs->backend_sync,themefile,key,myval);
  lives_system(com,FALSE);

  lives_free(com); lives_free(myval);
}



void set_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  char *com;
  char *myval;

  myval=lives_strdup_printf("%d %d %d %d",lcol->red,lcol->green,lcol->blue,lcol->alpha);
  com=lives_strdup_printf("%s set_pref \"%s\" \"%s\"",prefs->backend_sync,key,myval);
  lives_system(com,FALSE);

  lives_free(com); lives_free(myval);
}




void set_palette_prefs(void) {
  lives_colRGBA64_t lcol;

  lcol.red=palette->style;
  lcol.green=lcol.blue=lcol.alpha=0;
  
  set_colour_pref(THEME_DETAIL_STYLE,&lcol);

  set_pref(THEME_DETAIL_SEPWIN_IMAGE,mainw->sepimg_path);
  set_pref(THEME_DETAIL_FRAMEBLANK_IMAGE,mainw->frameblank_path);

  widget_color_to_lives_rgba(&lcol,&palette->normal_fore);
  set_colour_pref(THEME_DETAIL_NORMAL_FORE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->normal_back);
  set_colour_pref(THEME_DETAIL_NORMAL_BACK,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->menu_and_bars_fore);
  set_colour_pref(THEME_DETAIL_ALT_FORE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->menu_and_bars);
  set_colour_pref(THEME_DETAIL_ALT_BACK,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->info_text);
  set_colour_pref(THEME_DETAIL_INFO_TEXT,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->info_base);
  set_colour_pref(THEME_DETAIL_INFO_BASE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->mt_timecode_fg);
  set_colour_pref(THEME_DETAIL_MT_TCFG,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->mt_timecode_bg);
  set_colour_pref(THEME_DETAIL_MT_TCBG,&lcol);

  set_colour_pref(THEME_DETAIL_AUDCOL,&palette->audcol);
  set_colour_pref(THEME_DETAIL_VIDCOL,&palette->vidcol);
  set_colour_pref(THEME_DETAIL_FXCOL,&palette->fxcol);
      
  set_colour_pref(THEME_DETAIL_MT_TLREG,&palette->mt_timeline_reg);
  set_colour_pref(THEME_DETAIL_MT_MARK,&palette->mt_mark);
  set_colour_pref(THEME_DETAIL_MT_EVBOX,&palette->mt_evbox);

  set_colour_pref(THEME_DETAIL_FRAME_SURROUND,&palette->frame_surround);

  set_colour_pref(THEME_DETAIL_CE_SEL,&palette->ce_sel);
  set_colour_pref(THEME_DETAIL_CE_UNSEL,&palette->ce_unsel);

  set_pref(THEME_DETAIL_SEPWIN_IMAGE,mainw->sepimg_path);
  set_pref(THEME_DETAIL_FRAMEBLANK_IMAGE,mainw->frameblank_path);
}



void set_vpp(boolean set_in_prefs) {
  // Video Playback Plugin

  if (strlen(future_prefs->vpp_name)) {
    if (!lives_ascii_strcasecmp(future_prefs->vpp_name,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      if (mainw->vpp!=NULL) {
        if (mainw->ext_playback) vid_playback_plugin_exit();
        close_vid_playback_plugin(mainw->vpp);
        mainw->vpp=NULL;
        if (set_in_prefs) set_pref("vid_playback_plugin","none");
      }
    } else {
      _vid_playback_plugin *vpp;
      if ((vpp=open_vid_playback_plugin(future_prefs->vpp_name,TRUE))!=NULL) {
        mainw->vpp=vpp;
        if (set_in_prefs) {
          set_pref("vid_playback_plugin",mainw->vpp->name);
          if (!mainw->ext_playback)
            do_error_dialog_with_check_transient
            (_("\n\nVideo playback plugins are only activated in\nfull screen, separate window (fs) mode\n"),
             TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):LIVES_WINDOW(mainw->LiVES));
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
      if (mainw->fixed_fpsd!=-1.||!((*mainw->vpp->set_fps)(mainw->vpp->fixed_fpsd))) {
        do_vpp_fps_error();
        mainw->vpp->fixed_fpsd=-1.;
        mainw->vpp->fixed_fps_numer=0;
      }
    }
    if (!(*mainw->vpp->set_palette)(mainw->vpp->palette)) {
      do_vpp_palette_error();
    }
    mainw->vpp->YUV_clamping=future_prefs->vpp_YUV_clamping;

    if (mainw->vpp->set_yuv_palette_clamping!=NULL)(*mainw->vpp->set_yuv_palette_clamping)(mainw->vpp->YUV_clamping);

    mainw->vpp->extra_argc=future_prefs->vpp_argc;
    mainw->vpp->extra_argv=future_prefs->vpp_argv;
    if (set_in_prefs) mainw->write_vpp_file=TRUE;
  }

  memset(future_prefs->vpp_name,0,64);
  future_prefs->vpp_argv=NULL;
}



static void set_temp_label_text(LiVESLabel *label) {
  char *free_ds;
  char *tmpx1,*tmpx2;
  char *dir=future_prefs->tmpdir;
  char *markup;

  // use lives_strdup* since the translation string is auto-freed()

  if (!is_writeable_dir(dir)) {
    tmpx2=lives_strdup(_("\n\n\n(Free space = UNKNOWN)"));
  } else {
    free_ds=lives_format_storage_space_string(get_fs_free(dir));
    tmpx2=lives_strdup_printf(_("\n\n\n(Free space = %s)"),free_ds);
    lives_free(free_ds);
  }

  tmpx1=lives_strdup(
          _("The temp directory is LiVES working directory where opened clips and sets are stored.\nIt should be in a partition with plenty of free disk space.\n"));

#ifdef GUI_GTK
  markup = g_markup_printf_escaped("<span background=\"white\" foreground=\"red\"><b>%s</b></span>%s",tmpx1,tmpx2);
#endif
#ifdef GUI_QT
  QString qs = QString("<span background=\"white\" foreground=\"red\"><b>%s</b></span>%s").arg(tmpx1).arg(tmpx2);
  markup=strdup((const char *)qs.toHtmlEscaped().constData());
#endif

  lives_label_set_markup(LIVES_LABEL(label), markup);
  lives_free(markup);
  lives_free(tmpx1);
  lives_free(tmpx2);
}



void pref_factory_bool(int prefidx, boolean newval) {

  switch (prefidx) {
  case PREF_REC_EXT_AUDIO: {
    boolean rec_ext_audio=newval;
    if (rec_ext_audio&&prefs->audio_src==AUDIO_SRC_INT) {
      prefs->audio_src=AUDIO_SRC_EXT;
      set_int_pref("audio_src",AUDIO_SRC_EXT);

      if (mainw->playing_file==-1) {
        if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
          if (prefs->perm_audio_reader) {
            // create reader connection now, if permanent
            jack_rec_audio_to_clip(-1,-1,RECA_EXTERNAL);
          }
#endif
        }
        if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
          if (prefs->perm_audio_reader) {
            // create reader connection now, if permanent
            pulse_rec_audio_to_clip(-1,-1,RECA_EXTERNAL);
          }
#endif
        }
      }

    } else if (!rec_ext_audio&&prefs->audio_src==AUDIO_SRC_EXT) {
      prefs->audio_src=AUDIO_SRC_INT;
      set_int_pref("audio_src",AUDIO_SRC_INT);

      mainw->aud_rec_fd=-1;
      if (prefs->perm_audio_reader) {
#ifdef ENABLE_JACK
        jack_rec_audio_end(TRUE,TRUE);
#endif
#ifdef HAVE_PULSE_AUDIO
        pulse_rec_audio_end(TRUE,TRUE);
#endif
      }

    }
    if (prefsw!=NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio),prefs->audio_src==AUDIO_SRC_EXT);

  }
  break;
  case PREF_SEPWIN_STICKY: {
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sticky),newval);
  }
  break;
  case PREF_MT_EXIT_RENDER: {
    prefs->mt_exit_render=newval;
    if (prefsw!=NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);
  }
  break;
  default:
    break;
  }
}


void pref_factory_int(int prefidx, int newval) {
  // TODO
}



void pref_factory_bitmapped(int prefidx, int bitfield, boolean newval) {
  switch (prefidx) {
  case PREF_AUDIO_OPTS: {
    if (newval&&!(prefs->audio_opts&bitfield)) prefs->audio_opts&=bitfield;
    else if (!newval&&(prefs->audio_opts&bitfield)) prefs->audio_opts^=bitfield;
    if (prefsw!=NULL) {
      if (bitfield==AUDIO_OPTS_FOLLOW_FPS)
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)?TRUE:FALSE);
      else if (bitfield==AUDIO_OPTS_FOLLOW_CLIPS)
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)?TRUE:FALSE);
    }
  }
  break;
  default:
    break;
  }
}



boolean apply_prefs(boolean skip_warn) {
  // set current prefs from prefs dialog
  const char *video_open_command=lives_entry_get_text(LIVES_ENTRY(prefsw->video_open_entry));
  const char *audio_play_command=lives_entry_get_text(LIVES_ENTRY(prefsw->audio_command_entry));
  const char *def_vid_load_dir=lives_entry_get_text(LIVES_ENTRY(prefsw->vid_load_dir_entry));
  const char *def_vid_save_dir=lives_entry_get_text(LIVES_ENTRY(prefsw->vid_save_dir_entry));
  const char *def_audio_dir=lives_entry_get_text(LIVES_ENTRY(prefsw->audio_dir_entry));
  const char *def_image_dir=lives_entry_get_text(LIVES_ENTRY(prefsw->image_dir_entry));
  const char *def_proj_dir=lives_entry_get_text(LIVES_ENTRY(prefsw->proj_dir_entry));
  const char *wp_path=lives_entry_get_text(LIVES_ENTRY(prefsw->wpp_entry));
  const char *frei0r_path=lives_entry_get_text(LIVES_ENTRY(prefsw->frei0r_entry));
  const char *ladspa_path=lives_entry_get_text(LIVES_ENTRY(prefsw->ladspa_entry));

  const char *sepimg_path=lives_entry_get_text(LIVES_ENTRY(prefsw->sepimg_entry));
  const char *frameblank_path=lives_entry_get_text(LIVES_ENTRY(prefsw->frameblank_entry));

  char tmpdir[PATH_MAX];
  char *theme = lives_combo_get_active_text(LIVES_COMBO(prefsw->theme_combo));
  char *audp = lives_combo_get_active_text(LIVES_COMBO(prefsw->audp_combo));
  char *audio_codec=NULL;
  char *pb_quality = lives_combo_get_active_text(LIVES_COMBO(prefsw->pbq_combo));

  LiVESWidgetColor colf,colb,colf2,colb2,coli,colt,col,coltcfg,coltcbg;
  lives_colRGBA64_t lcol;
  
  int pbq=PB_QUALITY_MED;
  int idx;

  boolean needs_restart=FALSE;

  double default_fps=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_def_fps));

  boolean antialias=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_antialias));
  boolean fx_threads=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads));

  int nfx_threads=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_nfx_threads));

  boolean stop_screensaver=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  boolean open_maximised=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->open_maximised_check));
  boolean fs_maximised=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->fs_max_check));
  boolean show_recent=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check));
  boolean stream_audio_out=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio));
  boolean rec_after_pb=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb));

  uint64_t ds_warn_level=(uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds))*1000000;
  uint64_t ds_crit_level=(uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_crit_ds))*1000000;

  boolean warn_fps=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fps));
  boolean warn_save_set=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_set));
  boolean warn_fsize=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fsize));
  boolean warn_mplayer=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mplayer));
  boolean warn_rendered_fx=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_rendered_fx));
  boolean warn_encoders=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_encoders));
  boolean warn_duplicate_set=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_dup_set));
  boolean warn_layout_missing_clips=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_clips));
  boolean warn_layout_close=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_close));
  boolean warn_layout_delete=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_delete));
  boolean warn_layout_shift=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_shift));
  boolean warn_layout_alter=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_alter));
  boolean warn_discard_layout=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_discard_layout));
  boolean warn_after_dvgrab=
#ifdef HAVE_LDVGRAB
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_dvgrab));
#else
    !(prefs->warning_mask&WARN_MASK_AFTER_DVGRAB);
#endif
  boolean warn_mt_achans=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_achans));
  boolean warn_mt_no_jack=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_no_jack));
  boolean warn_yuv4m_open=
#ifdef HAVE_YUV4MPEG
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_yuv4m_open));
#else
    !(prefs->warning_mask&WARN_MASK_OPEN_YUV4M);
#endif

  boolean warn_layout_adel=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_adel));
  boolean warn_layout_ashift=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_ashift));
  boolean warn_layout_aalt=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_aalt));
  boolean warn_layout_popup=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_popup));
  boolean warn_mt_backup_space=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_backup_space));
  boolean warn_after_crash=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_crash));
  boolean warn_no_pulse=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_no_pulse));
  boolean warn_layout_wipe=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_wipe));

  boolean midisynch=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->check_midi));
  boolean instant_open=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open));
  boolean auto_deint=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint));
  boolean auto_trim=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_trim));
  boolean auto_nobord=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_nobord));
  boolean concat_images=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_concat_images));
  boolean ins_speed=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ins_speed));
  boolean show_player_stats=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_stats));
  boolean ext_jpeg=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jpeg));
  boolean show_tool=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->show_tool));
  boolean mouse_scroll=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mouse_scroll));
  boolean ce_maxspect=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_ce_maxspect));
  boolean show_button_icons=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_button_icons));

  int fsize_to_warn=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_fsize));
  int dl_bwidth=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_bwidth));
  int ocp=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_ocp));

  boolean rec_frames=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rframes));
  boolean rec_fps=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rfps));
  boolean rec_effects=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->reffects));
  boolean rec_clips=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rclips));
  boolean rec_audio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->raudio));
  boolean rec_ext_audio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio));
#ifdef RT_AUDIO
  boolean rec_desk_audio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rdesk_audio));
#endif

  boolean mt_enter_prompt=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_enter_prompt));
  boolean render_prompt=!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt));

  int mt_def_width=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_width));
  int mt_def_height=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_height));
  int mt_def_fps=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_fps));
  int mt_def_arate=atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  int mt_def_achans=atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  int mt_def_asamps=atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  int mt_def_signed_endian=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))*
                           AFORM_UNSIGNED+lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))*AFORM_BIG_ENDIAN;
  int mt_undo_buf=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_undo_buf));

  boolean mt_exit_render=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render));
  boolean mt_enable_audio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton));
  boolean mt_pertrack_audio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton));
  boolean mt_backaudio=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton));

  boolean mt_autoback_always=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always));
  boolean mt_autoback_never=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never));

  int mt_autoback_time=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time));
  int max_disp_vtracks=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_max_disp_vtracks));
  int gui_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));

  boolean ce_thumbs=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ce_thumbs));

  boolean forcesmon=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon));
  boolean startup_ce=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce));


#ifdef ENABLE_JACK_TRANSPORT
  boolean jack_tstart=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_tjack));
  boolean jack_master=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master));
  boolean jack_client=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client));
  boolean jack_tb_start=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start));
  boolean jack_tb_client=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client));
#else
#ifdef ENABLE_JACK
  boolean jack_tstart=FALSE;
  boolean jack_master=FALSE;
  boolean jack_client=FALSE;
  boolean jack_tb_start=FALSE;
  boolean jack_tb_client=FALSE;
#endif
#endif

#ifdef ENABLE_JACK
  boolean jack_astart=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack));
  boolean jack_pwp=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_pwp));
  boolean jack_read_autocon=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_read_autocon));
  uint32_t jack_opts=(JACK_OPTS_TRANSPORT_CLIENT*jack_client+JACK_OPTS_TRANSPORT_MASTER*jack_master+
                      JACK_OPTS_START_TSERVER*jack_tstart+JACK_OPTS_START_ASERVER*jack_astart+
                      JACK_OPTS_NOPLAY_WHEN_PAUSED*!jack_pwp+JACK_OPTS_TIMEBASE_START*jack_tb_start+
                      JACK_OPTS_TIMEBASE_CLIENT*jack_tb_client+JACK_OPTS_NO_READ_AUTOCON*!jack_read_autocon);
#endif

#ifdef RT_AUDIO
  boolean audio_follow_fps=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow));
  boolean audio_follow_clips=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips));
  uint32_t audio_opts=(AUDIO_OPTS_FOLLOW_FPS*audio_follow_fps+AUDIO_OPTS_FOLLOW_CLIPS*audio_follow_clips);
#endif

#ifdef ENABLE_OSC
  uint32_t osc_udp_port=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_osc_udp));
  boolean osc_start=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC_start));
  boolean osc_enable=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC));
#endif

  int rte_keys_virtual=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rte_keys));

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  boolean omc_js_enable=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_js));
  const char *omc_js_fname=lives_entry_get_text(LIVES_ENTRY(prefsw->omc_js_entry));
#endif


#ifdef OMC_MIDI_IMPL
  boolean omc_midi_enable=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_midi));
  const char *omc_midi_fname=lives_entry_get_text(LIVES_ENTRY(prefsw->omc_midi_entry));
  int midicr=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midicr));
  int midirpt=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midirpt));

#ifdef ALSA_MIDI
  boolean use_alsa_midi=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi));
#endif

#endif
#endif

  boolean pstyle2;
  boolean pstyle3=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style3));
  boolean pstyle4=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style4));

  int rec_gb=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rec_gb));

  char audio_player[256];
  int listlen=lives_list_length(prefs->acodec_list);
  int rec_opts=rec_frames*REC_FRAMES+rec_fps*REC_FPS+rec_effects*REC_EFFECTS+rec_clips*REC_CLIPS+rec_audio*REC_AUDIO
               +rec_after_pb*REC_AFTER_PB;
  uint32_t warn_mask;

  unsigned char *new_undo_buf;
  LiVESList *ulist;


#ifdef ENABLE_OSC
  boolean set_omc_dev_opts=FALSE;
#ifdef OMC_MIDI_IMPL
  boolean needs_midi_restart=FALSE;
#endif
#endif

  char *tmp;

  char *cdplay_device=lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->cdplay_entry)),-1,NULL,NULL,NULL);

  if (prefsw->theme_style2!=NULL)
    pstyle2=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style2));
  else
    pstyle2=0;

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fore),&colf);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_back),&colb);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mabf),&colf2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mab),&colb2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infob),&coli);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infot),&colt);

  if (lives_ascii_strcasecmp(future_prefs->theme,"none")) {
    if (!lives_widget_color_equal(&colf,&palette->normal_fore)||
	!lives_widget_color_equal(&colb,&palette->normal_back)||
	!lives_widget_color_equal(&colf2,&palette->menu_and_bars_fore)||
	!lives_widget_color_equal(&colb2,&palette->menu_and_bars)||
	!lives_widget_color_equal(&coli,&palette->info_text)||
	!lives_widget_color_equal(&colb,&palette->info_base)||
	(pstyle2!=(palette->style&STYLE_2))||
	(pstyle3!=(palette->style&STYLE_3))||
	(pstyle4!=(palette->style&STYLE_4))
	) {
      lives_widget_color_copy(&palette->normal_fore,&colf);
      lives_widget_color_copy(&palette->normal_back,&colb);
      lives_widget_color_copy(&palette->menu_and_bars_fore,&colf2);
      lives_widget_color_copy(&palette->menu_and_bars,&colb2);
      lives_widget_color_copy(&palette->info_base,&coli);
      lives_widget_color_copy(&palette->info_text,&colt);

      palette->style=STYLE_1|(pstyle2*STYLE_2)|(pstyle3*STYLE_3)|(pstyle4*STYLE_4);
      mainw->prefs_changed|=PREFS_COLOURS_CHANGED;

    }
  }
  
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_cesel),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->ce_sel)) {
    lives_rgba_copy(&palette->ce_sel,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_ceunsel),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->ce_unsel)) {
    lives_rgba_copy(&palette->ce_unsel,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fsur),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->frame_surround)) {
    lives_rgba_copy(&palette->frame_surround,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcfg),&coltcfg);
  if (!lives_widget_color_equal(&coltcfg,&palette->mt_timecode_fg)) {
    lives_widget_color_copy(&palette->mt_timecode_fg,&coltcfg);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcbg),&coltcbg);
  if (!lives_widget_color_equal(&coltcbg,&palette->mt_timecode_bg)) {
    lives_widget_color_copy(&palette->mt_timecode_bg,&coltcbg);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mtmark),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->mt_mark)) {
    lives_rgba_copy(&palette->mt_mark,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_evbox),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->mt_evbox)) {
    lives_rgba_copy(&palette->mt_evbox,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tlreg),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->mt_timeline_reg)) {
    lives_rgba_copy(&palette->mt_timeline_reg,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_vidcol),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->vidcol)) {
    lives_rgba_copy(&palette->vidcol,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_audcol),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->audcol)) {
    lives_rgba_copy(&palette->audcol,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fxcol),&col);
  widget_color_to_lives_rgba(&lcol,&col);
  if (!lives_rgba_equal(&lcol,&palette->fxcol)) {
    lives_rgba_copy(&palette->fxcol,&lcol);
    mainw->prefs_changed|=PREFS_XCOLOURS_CHANGED;
  }


  
  if (capable->has_encoder_plugins) {
    audio_codec = lives_combo_get_active_text(LIVES_COMBO(prefsw->acodec_combo));

    for (idx=0; idx<listlen&&strcmp((char *)lives_list_nth_data(prefs->acodec_list,idx),audio_codec); idx++);
    lives_free(audio_codec);

    if (idx==listlen) future_prefs->encoder.audio_codec=0;
    else future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[idx];
  } else future_prefs->encoder.audio_codec=0;

  lives_snprintf(tmpdir,PATH_MAX,"%s",(tmp=lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->tmpdir_entry)),
                                       -1,NULL,NULL,NULL)));
  lives_free(tmp);

  if (audp==NULL) memset(audio_player,0,1);
  else if (!strncmp(audp,"mplayer",7)) lives_snprintf(audio_player,256,"mplayer");
  else if (!strncmp(audp,"mplayer2",8)) lives_snprintf(audio_player,256,"mplayer2");
  else if (!strncmp(audp,"jack",4)) lives_snprintf(audio_player,256,"jack");
  else if (!strncmp(audp,"sox",3)) lives_snprintf(audio_player,256,"sox");
  else if (!strncmp(audp,"pulse audio",11)) lives_snprintf(audio_player,256,"pulse");

  lives_free(audp);

  if (!((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    if (prefs->audio_src==AUDIO_SRC_EXT) prefs->audio_src=AUDIO_SRC_INT;
  }

  if (rec_opts!=prefs->rec_opts) {
    prefs->rec_opts=rec_opts;
    set_int_pref("record_opts",prefs->rec_opts);
  }

  pref_factory_bool(PREF_REC_EXT_AUDIO, rec_ext_audio);

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
            WARN_MASK_MT_BACKUP_SPACE+!warn_after_crash*WARN_MASK_CLEAN_AFTER_CRASH+!warn_no_pulse*WARN_MASK_NO_PULSE_CONNECT
            +!warn_layout_wipe*WARN_MASK_LAYOUT_WIPE;

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

  if (show_button_icons!=(prefs->show_button_images)) {
    prefs->show_button_images=show_button_icons;
    set_boolean_pref("show_button_icons",show_button_icons);
  }

  if (ce_maxspect!=(prefs->ce_maxspect)) {
    prefs->ce_maxspect=ce_maxspect;
    set_boolean_pref("ce_maxspect",ce_maxspect);
    if (mainw->current_file>-1) {
      int current_file=mainw->current_file;
      switch_to_file((mainw->current_file=0),current_file);
    }
  }

  if (strcmp(wp_path,prefs->weed_plugin_path)) {
    set_pref("weed_plugin_path",wp_path);
    lives_snprintf(prefs->weed_plugin_path,PATH_MAX,"%s",wp_path);
  }

  if (strcmp(frei0r_path,prefs->frei0r_path)) {
    set_pref("frei0r_path",frei0r_path);
    lives_snprintf(prefs->frei0r_path,PATH_MAX,"%s",frei0r_path);
  }

  if (strcmp(ladspa_path,prefs->ladspa_path)) {
    set_pref("ladspa_path",ladspa_path);
    lives_snprintf(prefs->ladspa_path,PATH_MAX,"%s",ladspa_path);
  }

  if (strcmp(sepimg_path,mainw->sepimg_path)) {
    lives_snprintf(mainw->sepimg_path,PATH_MAX,"%s",sepimg_path);
    mainw->prefs_changed|=PREFS_IMAGES_CHANGED;
  }

  if (strcmp(frameblank_path,mainw->frameblank_path)) {
    lives_snprintf(mainw->frameblank_path,PATH_MAX,"%s",frameblank_path);
    mainw->prefs_changed|=PREFS_IMAGES_CHANGED;
  }

  ensure_isdir(tmpdir);
  ensure_isdir(prefs->tmpdir);
  ensure_isdir(future_prefs->tmpdir);

  if (strcmp(prefs->tmpdir,tmpdir)||strcmp(future_prefs->tmpdir,tmpdir)) {
    if (lives_file_test(tmpdir, LIVES_FILE_TEST_EXISTS)&&(strlen(tmpdir)<10||
        strncmp(tmpdir+strlen(tmpdir)-10,"/"LIVES_TMP_NAME"/",10)))
      lives_strappend(tmpdir,PATH_MAX,LIVES_TMP_NAME"/");

    if (strcmp(prefs->tmpdir,tmpdir)||strcmp(future_prefs->tmpdir,tmpdir)) {
      char *msg;

      if (!check_dir_access(tmpdir)) {
        tmp=lives_filename_to_utf8(tmpdir,-1,NULL,NULL,NULL);
#ifndef IS_MINGW
        msg=lives_strdup_printf(
              _("Unable to create or write to the new temporary directory.\nYou may need to create it as the root user first, e.g:\n\nsudo mkdir -p %s; sudo chmod 777 %s\n\nThe directory will not be changed now.\n"),
              tmp,tmp);
#else
        msg=lives_strdup_printf(
              _("Unable to create or write to the new temporary directory.\n%s\nPlease try another directory or contact your system administrator.\n\nThe directory will not be changed now.\n"),
              tmp);
#endif

        lives_free(tmp);
        do_blocking_error_dialog(msg);
      } else {
        lives_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",tmpdir);
        set_temp_label_text(LIVES_LABEL(prefsw->temp_label));
        lives_widget_queue_draw(prefsw->temp_label);
        lives_widget_context_update(); // update prefs window before showing confirmation box

        msg=lives_strdup(
              _("You have chosen to change the temporary directory.\nPlease make sure you have no other copies of LiVES open.\n\nIf you do have other copies of LiVES open, please close them now, *before* pressing OK.\n\nAlternatively, press Cancel to restore the temporary directory to its original setting."));
        if (do_warning_dialog(msg)) {
          mainw->prefs_changed=PREFS_TEMPDIR_CHANGED;
          needs_restart=TRUE;
        } else {
          lives_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);
          lives_entry_set_text(LIVES_ENTRY(prefsw->tmpdir_entry), prefs->tmpdir);
        }
      }
      lives_free(msg);
    }
  }

  // disabled_decoders
  if (string_lists_differ(prefs->disabled_decoders,future_prefs->disabled_decoders)) {
    if (prefs->disabled_decoders!=NULL) {
      lives_list_free_strings(prefs->disabled_decoders);
      lives_list_free(prefs->disabled_decoders);
    }
    prefs->disabled_decoders=lives_list_copy_strings(future_prefs->disabled_decoders);
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
    if (capable->nmonitors==0) resize_widgets_for_monitor(TRUE);
  }

  if (capable->nmonitors>1) {
    if (gui_monitor!=prefs->gui_monitor||play_monitor!=prefs->play_monitor) {
      char *str=lives_strdup_printf("%d,%d",gui_monitor,play_monitor);
      set_pref("monitors",str);
      prefs->gui_monitor=gui_monitor;
      prefs->play_monitor=play_monitor;

      resize_widgets_for_monitor(TRUE);
    }
  }

  if (ce_thumbs!=prefs->ce_thumb_mode) {
    prefs->ce_thumb_mode=ce_thumbs;
    set_boolean_pref("ce_thumb_mode",ce_thumbs);
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
      lives_widget_show(mainw->recent_menu);
      if (mainw->multitrack!=NULL) lives_widget_show(mainw->multitrack->recent_menu);
    } else {
      lives_widget_hide(mainw->recent_menu);
      if (mainw->multitrack!=NULL) lives_widget_hide(mainw->multitrack->recent_menu);
    }
  }

  // midi synch
  if (prefs->midisynch!=midisynch) {
    prefs->midisynch=midisynch;
    set_boolean_pref("midisynch",midisynch);
  }

  // jpeg/png
  if (strcmp(prefs->image_ext,LIVES_FILE_EXT_JPG)&&ext_jpeg) {
    set_pref("default_image_format","jpeg");
    lives_snprintf(prefs->image_ext,16,LIVES_FILE_EXT_JPG);
  } else if (!strcmp(prefs->image_ext,LIVES_FILE_EXT_JPG)&&!ext_jpeg) {
    set_pref("default_image_format","png");
    lives_snprintf(prefs->image_ext,16,LIVES_FILE_EXT_PNG);
  }

  // instant open
  if (prefs->instant_open!=instant_open) {
    set_boolean_pref("instant_open",(prefs->instant_open=instant_open));
  }

  // auto deinterlace
  if (prefs->auto_deint!=auto_deint) {
    set_boolean_pref("auto_deinterlace",(prefs->auto_deint=auto_deint));
  }

  // auto deinterlace
  if (prefs->auto_trim_audio!=auto_trim) {
    set_boolean_pref("auto_trim_pad_audio",(prefs->auto_trim_audio=auto_trim));
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
    lives_snprintf(prefs->encoder.name,51,"%s",future_prefs->encoder.name);
    set_pref("encoder",prefs->encoder.name);
    lives_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
    prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
  }

  // output format
  if (strcmp(prefs->encoder.of_name,future_prefs->encoder.of_name)) {
    lives_snprintf(prefs->encoder.of_name,51,"%s",future_prefs->encoder.of_name);
    lives_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
    lives_snprintf(prefs->encoder.of_desc,128,"%s",future_prefs->encoder.of_desc);
    prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
    set_pref("output_type",prefs->encoder.of_name);
  }

  if (prefs->encoder.audio_codec!=future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec=future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec<AUDIO_CODEC_UNKNOWN) {
      set_int_pref("encoder_acodec",prefs->encoder.audio_codec);
    }
  }

  // pb quality
  if (!strcmp(pb_quality,(char *)lives_list_nth_data(prefsw->pbq_list,0))) pbq=PB_QUALITY_LOW;
  if (!strcmp(pb_quality,(char *)lives_list_nth_data(prefsw->pbq_list,1))) pbq=PB_QUALITY_MED;
  if (!strcmp(pb_quality,(char *)lives_list_nth_data(prefsw->pbq_list,2))) pbq=PB_QUALITY_HIGH;

  lives_free(pb_quality);

  if (pbq!=prefs->pb_quality) {
    prefs->pb_quality=pbq;
    set_int_pref("pb_quality",pbq);
  }

  // video open command
  if (strcmp(prefs->video_open_command,video_open_command)) {
    lives_snprintf(prefs->video_open_command,256,"%s",video_open_command);
    set_pref("video_open_command",prefs->video_open_command);
  }

  //playback plugin
  set_vpp(TRUE);

  // audio play command
  if (strcmp(prefs->audio_play_command,audio_play_command)) {
    lives_snprintf(prefs->audio_play_command,256,"%s",audio_play_command);
    set_pref("audio_play_command",prefs->audio_play_command);
  }

  // cd play device
  if (strcmp(prefs->cdplay_device,cdplay_device)) {
    lives_snprintf(prefs->cdplay_device,256,"%s",cdplay_device);
    set_pref("cdplay_device",prefs->cdplay_device);
  }

  lives_free(cdplay_device);

  // default video load directory
  if (strcmp(prefs->def_vid_load_dir,def_vid_load_dir)) {
    lives_snprintf(prefs->def_vid_load_dir,PATH_MAX,"%s/",def_vid_load_dir);
    get_dirname(prefs->def_vid_load_dir);
    set_pref("vid_load_dir",prefs->def_vid_load_dir);
    lives_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",prefs->def_vid_load_dir);
  }

  // default video save directory
  if (strcmp(prefs->def_vid_save_dir,def_vid_save_dir)) {
    lives_snprintf(prefs->def_vid_save_dir,PATH_MAX,"%s/",def_vid_save_dir);
    get_dirname(prefs->def_vid_save_dir);
    set_pref("vid_save_dir",prefs->def_vid_save_dir);
    lives_snprintf(mainw->vid_save_dir,PATH_MAX,"%s",prefs->def_vid_save_dir);
  }

  // default audio directory
  if (strcmp(prefs->def_audio_dir,def_audio_dir)) {
    lives_snprintf(prefs->def_audio_dir,PATH_MAX,"%s/",def_audio_dir);
    get_dirname(prefs->def_audio_dir);
    set_pref("audio_dir",prefs->def_audio_dir);
    lives_snprintf(mainw->audio_dir,PATH_MAX,"%s",prefs->def_audio_dir);
  }

  // default image directory
  if (strcmp(prefs->def_image_dir,def_image_dir)) {
    lives_snprintf(prefs->def_image_dir,PATH_MAX,"%s/",def_image_dir);
    get_dirname(prefs->def_image_dir);
    set_pref("image_dir",prefs->def_image_dir);
    lives_snprintf(mainw->image_dir,PATH_MAX,"%s",prefs->def_image_dir);
  }

  // default project directory - for backup and restore
  if (strcmp(prefs->def_proj_dir,def_proj_dir)) {
    lives_snprintf(prefs->def_proj_dir,PATH_MAX,"%s/",def_proj_dir);
    get_dirname(prefs->def_proj_dir);
    set_pref("proj_dir",prefs->def_proj_dir);
    lives_snprintf(mainw->proj_load_dir,PATH_MAX,"%s",prefs->def_proj_dir);
    lives_snprintf(mainw->proj_save_dir,PATH_MAX,"%s",prefs->def_proj_dir);
  }

  // the theme
  if (strcmp(future_prefs->theme,theme)&&!(!lives_ascii_strcasecmp(future_prefs->theme,"none")&&
      !strcmp(theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) {
    if (strcmp(theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      lives_snprintf(prefs->theme,64,"%s",theme);
      lives_snprintf(future_prefs->theme,64,"%s",theme);
      set_pref("gui_theme",future_prefs->theme);
      widget_opts.apply_theme=TRUE;
      set_palette_colours(TRUE);
      load_theme_images();
      mainw->prefs_changed|=PREFS_COLOURS_CHANGED|PREFS_IMAGES_CHANGED;
    } else {
      lives_snprintf(future_prefs->theme,64,"none");
      set_pref("gui_theme",future_prefs->theme);
      delete_pref(THEME_DETAIL_STYLE);
      delete_pref(THEME_DETAIL_SEPWIN_IMAGE);
      delete_pref(THEME_DETAIL_FRAMEBLANK_IMAGE);
      delete_pref(THEME_DETAIL_NORMAL_FORE);
      delete_pref(THEME_DETAIL_NORMAL_BACK);
      delete_pref(THEME_DETAIL_ALT_FORE);
      delete_pref(THEME_DETAIL_ALT_BACK);
      delete_pref(THEME_DETAIL_INFO_TEXT);
      delete_pref(THEME_DETAIL_INFO_BASE);
      mainw->prefs_changed|=PREFS_THEME_CHANGED;
    }
  }

  lives_free(theme);

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
    set_boolean_pref("insert_resample",prefs->ins_resample);
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
  } else {
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
  } else {
    if (future_prefs->osc_start) {
      set_boolean_pref("osc_start",FALSE);
      future_prefs->osc_start=FALSE;
    }
  }
  if (prefs->osc_udp_port!=osc_udp_port) {
    prefs->osc_udp_port=osc_udp_port;
    set_int_pref("osc_port",osc_udp_port);
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
     TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):LIVES_WINDOW(mainw->LiVES));
  } else {
    if (prefs->audio_player==AUD_PLAYER_JACK&&strcmp(audio_player,"jack")) {
      do_error_dialog_with_check_transient
      (_("\nSwitching audio players requires restart (jackd must not be running)\n"),
       TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):LIVES_WINDOW(mainw->LiVES));
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
    else if (!(strcmp(audio_player,"mplayer"))&&prefs->audio_player!=AUD_PLAYER_MPLAYER) {
      switch_aud_to_mplayer(TRUE);
    }

    // switch to pulse audio
    else if (!(strcmp(audio_player,"pulse"))&&prefs->audio_player!=AUD_PLAYER_PULSE) {
      if (!capable->has_pulse_audio) {
        do_error_dialog_with_check_transient
        (_("\nUnable to switch audio players to pulse audio\npulseaudio must be installed first.\nSee http://www.pulseaudio.org\n"),
         TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):LIVES_WINDOW(mainw->LiVES));
      } else {
        if (!switch_aud_to_pulse()) {
          // revert text
          lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
        }
      }
    }

    // switch to mplayer2 audio
    else if (!(strcmp(audio_player,"mplayer2"))&&prefs->audio_player!=AUD_PLAYER_MPLAYER2) {
      switch_aud_to_mplayer2(TRUE);
    }
    //

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
    lives_snprintf(prefs->omc_js_fname,256,"%s",omc_js_fname);
    set_pref("omc_js_fname",omc_js_fname);
  }
  if (omc_js_enable!=((prefs->omc_dev_opts&OMC_DEV_JS)/OMC_DEV_JS)) {
    if (omc_js_enable) {
      prefs->omc_dev_opts|=OMC_DEV_JS;
      js_open();
    } else {
      prefs->omc_dev_opts^=OMC_DEV_JS;
      js_close();
    }
    set_omc_dev_opts=TRUE;
  }
#endif


#ifdef OMC_MIDI_IMPL
  if (strcmp(omc_midi_fname,prefs->omc_midi_fname)) {
    lives_snprintf(prefs->omc_midi_fname,256,"%s",omc_midi_fname);
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
    } else {
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
    } else {
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
    if ((new_undo_buf=(unsigned char *)lives_try_malloc(mt_undo_buf*1024*1024))==NULL) {
      do_mt_set_mem_error(mainw->multitrack!=NULL,skip_warn);
    } else {
      if (mainw->multitrack!=NULL) {
        if (mainw->multitrack->undo_mem!=NULL) {
          if (mt_undo_buf<prefs->mt_undo_buf) {
            ssize_t space_needed=mainw->multitrack->undo_buffer_used-(size_t)(mt_undo_buf*1024*1024);
            if (space_needed>0) make_backup_space(mainw->multitrack,space_needed);
            memcpy(new_undo_buf,mainw->multitrack->undo_mem,mt_undo_buf*1024*1024);
          } else memcpy(new_undo_buf,mainw->multitrack->undo_mem,prefs->mt_undo_buf*1024*1024);
          ulist=mainw->multitrack->undos;
          while (ulist!=NULL) {
            ulist->data=new_undo_buf+((unsigned char *)ulist->data-mainw->multitrack->undo_mem);
            ulist=ulist->next;
          }
          lives_free(mainw->multitrack->undo_mem);
          mainw->multitrack->undo_mem=new_undo_buf;
        } else {
          mainw->multitrack->undo_mem=(unsigned char *)lives_try_malloc(mt_undo_buf*1024*1024);
          if (mainw->multitrack->undo_mem==NULL) {
            do_mt_set_mem_error(TRUE,skip_warn);
          } else {
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

  if (max_disp_vtracks!=prefs->max_disp_vtracks) {
    prefs->max_disp_vtracks=max_disp_vtracks;
    set_int_pref("max_disp_vtracks",max_disp_vtracks);
    if (mainw->multitrack!=NULL) scroll_tracks(mainw->multitrack,mainw->multitrack->top_track,FALSE);
  }

  if (startup_ce&&future_prefs->startup_interface!=STARTUP_CE) {
    future_prefs->startup_interface=STARTUP_CE;
    set_int_pref("startup_interface",STARTUP_CE);
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL)
      write_backup_layout_numbering(mainw->multitrack);
  } else if (!startup_ce&&future_prefs->startup_interface!=STARTUP_MT) {
    future_prefs->startup_interface=STARTUP_MT;
    set_int_pref("startup_interface",STARTUP_MT);
    if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL)
      write_backup_layout_numbering(mainw->multitrack);
  }

  return needs_restart;
}




void save_future_prefs(void) {
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
    set_boolean_pref("show_toolbar",future_prefs->show_tool);
  }


}


void rdet_acodec_changed(LiVESCombo *acodec_combo, livespointer user_data) {
  int listlen=lives_list_length(prefs->acodec_list);
  int idx;
  char *audio_codec = lives_combo_get_active_text(acodec_combo);
  if (!strcmp(audio_codec,mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    lives_free(audio_codec);
    return;
  }

  for (idx=0; idx<listlen&&strcmp((char *)lives_list_nth_data(prefs->acodec_list,idx),audio_codec); idx++);
  lives_free(audio_codec);

  if (idx==listlen) future_prefs->encoder.audio_codec=0;
  else future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[idx];

  if (prefs->encoder.audio_codec!=future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec=future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec<AUDIO_CODEC_UNKNOWN) {
      set_int_pref("encoder_acodec",prefs->encoder.audio_codec);
    }
  }
}





void set_acodec_list_from_allowed(_prefsw *prefsw, render_details *rdet) {
  // could be done better, but no time...
  // get settings for current format


  int count=0,idx;
  boolean is_allowed=FALSE;

  if (prefs->acodec_list!=NULL) {
    lives_list_free(prefs->acodec_list);
    prefs->acodec_list=NULL;
  }

  if (future_prefs->encoder.of_allowed_acodecs==0) {
    prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0]=AUDIO_CODEC_NONE;

    if (prefsw!=NULL) {
      lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), 0);
    }
    if (rdet!=NULL) {
      lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), 0);
    }
    return;
  }
  for (idx=0; strlen(anames[idx]); idx++) {
    if (future_prefs->encoder.of_allowed_acodecs&(1<<idx)) {
      if (idx==AUDIO_CODEC_PCM) prefs->acodec_list=lives_list_append(prefs->acodec_list,
            lives_strdup(_("PCM (highest quality; largest files)")));
      else prefs->acodec_list=lives_list_append(prefs->acodec_list,lives_strdup(anames[idx]));
      prefs->acodec_list_to_format[count++]=idx;
      if (future_prefs->encoder.audio_codec==idx) is_allowed=TRUE;
    }
  }

  if (prefsw != NULL) {
    lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
  }
  if (rdet != NULL) {
    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
  }
  if (!is_allowed) {
    future_prefs->encoder.audio_codec=prefs->acodec_list_to_format[0];
  }

  for (idx=0; idx < lives_list_length(prefs->acodec_list); idx++) {
    if (prefs->acodec_list_to_format[idx]==future_prefs->encoder.audio_codec) {
      if (prefsw!=NULL) {
        lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), idx);
      }
      if (rdet!=NULL) {
        lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), idx);
      }
      break;
    }
  }
}


void after_vpp_changed(LiVESWidget *vpp_combo, livespointer advbutton) {
  char *newvpp=lives_combo_get_active_text(LIVES_COMBO(vpp_combo));
  _vid_playback_plugin *tmpvpp;

  if (!lives_ascii_strcasecmp(newvpp,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), FALSE);
  } else {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), TRUE);

    // will call set_astream_settings
    if ((tmpvpp=open_vid_playback_plugin(newvpp, FALSE))==NULL) {
      lives_free(newvpp);
      return;
    }
    close_vid_playback_plugin(tmpvpp);
  }
  lives_snprintf(future_prefs->vpp_name,64,"%s",newvpp);
  lives_free(newvpp);

  if (future_prefs->vpp_argv!=NULL) {
    register int i;
    for (i=0; future_prefs->vpp_argv[i]!=NULL; lives_free(future_prefs->vpp_argv[i++]));
    lives_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv=NULL;
  }
  future_prefs->vpp_argc=0;

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),FALSE);

}



static void on_forcesmon_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  int gui_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->spinbutton_gmoni,!lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->spinbutton_pmoni,!lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->ce_thumbs,!lives_toggle_button_get_active(tbutton)&&
                             play_monitor!=gui_monitor&&
                             play_monitor!=0&&capable->nmonitors>0);
}

static void pmoni_gmoni_changed(LiVESWidget *sbut, livespointer advbutton) {
  int gui_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor=lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->ce_thumbs,play_monitor!=gui_monitor&&
                             play_monitor!=0&&!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon))&&
                             capable->nmonitors>0);
}

static void on_mtbackevery_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data!=NULL) xprefsw=(_prefsw *)user_data;
  else xprefsw=prefsw;

  lives_widget_set_sensitive(xprefsw->spinbutton_mt_ab_time,lives_toggle_button_get_active(tbutton));

}

#ifdef ENABLE_JACK_TRANSPORT
static void after_jack_client_toggled(LiVESToggleButton *tbutton, livespointer user_data) {

  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,FALSE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),
                                   (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_START)?TRUE:FALSE);
  }
}

static void after_jack_tb_start_toggled(LiVESToggleButton *tbutton, livespointer user_data) {

  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,FALSE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),
                                   (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)?TRUE:FALSE);
  }
}
#endif


#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifdef ALSA_MIDI
static void on_alsa_midi_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data!=NULL) xprefsw=(_prefsw *)user_data;
  else xprefsw=prefsw;

  lives_widget_set_sensitive(xprefsw->button_midid,!lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->omc_midi_entry,!lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midicr,!lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midirpt,!lives_toggle_button_get_active(tbutton));
}
#endif
#endif
#endif


static void on_audp_entry_changed(LiVESWidget *audp_combo, livespointer ptr) {
  char *audp = lives_combo_get_active_text(LIVES_COMBO(audp_combo));

  if (!strlen(audp)||!strcmp(audp,prefsw->audp_name)) {
    lives_free(audp);
    return;
  }

  if (mainw->playing_file>-1) {
    do_aud_during_play_error();
    lives_signal_handler_block(audp_combo, prefsw->audp_entry_func);

    lives_combo_set_active_string(LIVES_COMBO(audp_combo), prefsw->audp_name);

    //lives_widget_queue_draw(audp_entry);
    lives_signal_handler_unblock(audp_combo, prefsw->audp_entry_func);
    lives_free(audp);
    return;
  }

#ifdef RT_AUDIO
  if (!strncmp(audp,"jack",4)||!strncmp(audp,"pulse",5)) {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips,TRUE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow,TRUE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips,FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow,FALSE);
    lives_widget_set_sensitive(prefsw->raudio,FALSE);
    lives_widget_set_sensitive(prefsw->rextaudio,FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio),FALSE);
  }
  if (!strncmp(audp,"jack",4)) {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp,TRUE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack),TRUE);
    lives_widget_show(prefsw->jack_int_label);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp,FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon,FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack),FALSE);
    lives_widget_hide(prefsw->jack_int_label);
  }
#endif
  lives_free(prefsw->audp_name);
  prefsw->audp_name=lives_combo_get_active_text(LIVES_COMBO(audp_combo));
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),FALSE);
  lives_free(audp);

}


static void stream_audio_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  // if audio streaming is enabled, check requisites

  if (lives_toggle_button_get_active(togglebutton)) {
    // init vpp, get audio codec, check requisites
    _vid_playback_plugin *tmpvpp;
    uint32_t orig_acodec=AUDIO_CODEC_NONE;

    if (strlen(future_prefs->vpp_name)) {
      if ((tmpvpp=open_vid_playback_plugin(future_prefs->vpp_name, FALSE))==NULL) return;
    } else {
      tmpvpp=mainw->vpp;
      orig_acodec=mainw->vpp->audio_codec;
      get_best_audio(mainw->vpp); // check again because audio player may differ
    }

    if (tmpvpp->audio_codec!=AUDIO_CODEC_NONE) {
      // make audiostream plugin name
      FILE *rfile;
      size_t rlen;

      char buf[1024];
      char *com;

      char *astreamer=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,"audiostreamer.pl",NULL);

      com=lives_strdup_printf("\"%s\" check %d",astreamer,tmpvpp->audio_codec);
      lives_free(astreamer);

      rfile=popen(com,"r");
      if (!rfile) {
        // command failed
        do_system_failed_error(com,0,NULL);
        lives_free(com);
        return;
      }

      rlen=fread(buf,1,1023,rfile);
      pclose(rfile);
      memset(buf+rlen,0,1);
      lives_free(com);

      if (rlen>0) {
        lives_toggle_button_set_active(togglebutton, FALSE);
      }
    }

    if (tmpvpp!=NULL) {
      if (tmpvpp!=mainw->vpp) {
        // close the temp current vpp
        close_vid_playback_plugin(tmpvpp);
      } else {
        // restore current codec
        mainw->vpp->audio_codec=orig_acodec;
      }
    }

  }

}


void prefsw_set_astream_settings(_vid_playback_plugin *vpp) {

  if (vpp!=NULL&&vpp->audio_codec!=AUDIO_CODEC_NONE) {
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio,TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio,FALSE);
  }
}


void prefsw_set_rec_after_settings(_vid_playback_plugin *vpp) {
  if (vpp!=NULL&&(vpp->capabilities&VPP_CAN_RETURN)) {
    lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb,TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb,FALSE);
  }
}


/*
 * Initialize preferences dialog list
 */
static void pref_init_list(LiVESWidget *list) {
  LiVESCellRenderer *renderer, *pixbufRenderer;
  LiVESTreeViewColumn *column1, *column2;
  LiVESListStore *store;

  renderer = lives_cell_renderer_text_new();
  pixbufRenderer = lives_cell_renderer_pixbuf_new();

  column1 = lives_tree_view_column_new_with_attributes("List Icons", pixbufRenderer, "pixbuf", LIST_ICON, NULL);
  column2 = lives_tree_view_column_new_with_attributes("List Items", renderer, "text", LIST_ITEM, NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column1);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column2);
  lives_tree_view_column_set_sizing(column2, LIVES_TREE_VIEW_COLUMN_FIXED);
  lives_tree_view_column_set_fixed_width(column2, 150.*widget_opts.scale);

  store = lives_list_store_new(N_COLUMNS, LIVES_COL_TYPE_PIXBUF, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_UINT);

  lives_tree_view_set_model(LIVES_TREE_VIEW(list), LIVES_TREE_MODEL(store));

  //lives_object_unref(store);
}

/*
 * Adds entry to preferences dialog list
 */
static void prefs_add_to_list(LiVESWidget *list, LiVESPixbuf *pix, const char *str, uint32_t idx) {
  LiVESListStore *store;
  LiVESTreeIter iter;

  store = LIVES_LIST_STORE(lives_tree_view_get_model(LIVES_TREE_VIEW(list)));

  lives_list_store_insert(store, &iter, idx);
  lives_list_store_set(store, &iter, LIST_ICON, pix, LIST_ITEM, str, LIST_NUM, idx, -1);
}

/*
 * Callback function called when preferences list row changed
 */
void on_prefDomainChanged(LiVESTreeSelection *widget, livespointer dummy) {
  LiVESTreeIter iter;
  LiVESTreeModel *model;

  register int i;

  for (i=0; i<2; i++) {
    // for some reason gtk+ needs us to do this twice..
    if (lives_tree_selection_get_selected(widget, &model, &iter)) {
      lives_tree_model_get(model, &iter, LIST_NUM, &prefs_current_page, -1);
      //
      // Hide currently shown widget
      if (prefsw->right_shown) {
        lives_widget_hide(prefsw->right_shown);
      }
      //

      switch (prefs_current_page) {
      case LIST_ENTRY_MULTITRACK:
        lives_widget_show_all(prefsw->scrollw_right_multitrack);
        prefsw->right_shown = prefsw->scrollw_right_multitrack;
        break;
      case LIST_ENTRY_DECODING:
        lives_widget_show_all(prefsw->scrollw_right_decoding);
        prefsw->right_shown = prefsw->scrollw_right_decoding;
        break;
      case LIST_ENTRY_PLAYBACK:
        lives_widget_show_all(prefsw->scrollw_right_playback);
        prefsw->right_shown = prefsw->scrollw_right_playback;
        break;
      case LIST_ENTRY_RECORDING:
        lives_widget_show_all(prefsw->scrollw_right_recording);
        prefsw->right_shown = prefsw->scrollw_right_recording;
        break;
      case LIST_ENTRY_ENCODING:
        lives_widget_show_all(prefsw->scrollw_right_encoding);
        prefsw->right_shown = prefsw->scrollw_right_encoding;
        break;
      case LIST_ENTRY_EFFECTS:
        lives_widget_show_all(prefsw->scrollw_right_effects);
        prefsw->right_shown = prefsw->scrollw_right_effects;
        break;
      case LIST_ENTRY_DIRECTORIES:
        lives_widget_show_all(prefsw->scrollw_right_directories);
        prefsw->right_shown = prefsw->scrollw_right_directories;
        break;
      case LIST_ENTRY_WARNINGS:
        lives_widget_show_all(prefsw->scrollw_right_warnings);
        prefsw->right_shown = prefsw->scrollw_right_warnings;
        break;
      case LIST_ENTRY_MISC:
        lives_widget_show_all(prefsw->scrollw_right_misc);
        prefsw->right_shown = prefsw->scrollw_right_misc;
        if (!capable->has_cdda2wav) {
          lives_widget_hide(prefsw->cdda_hbox);
        }
        break;
      case LIST_ENTRY_THEMES:
        lives_widget_show_all(prefsw->scrollw_right_themes);
        prefsw->right_shown = prefsw->scrollw_right_themes;
        break;
      case LIST_ENTRY_NET:
        lives_widget_show_all(prefsw->scrollw_right_net);
        prefsw->right_shown = prefsw->scrollw_right_net;
        break;
      case LIST_ENTRY_JACK:
        lives_widget_show_all(prefsw->scrollw_right_jack);
        lives_widget_hide(prefsw->jack_int_label);

#ifdef ENABLE_JACK
        if (prefs->audio_player==AUD_PLAYER_JACK) {
          lives_widget_show(prefsw->jack_int_label);
        }
#endif

        prefsw->right_shown = prefsw->scrollw_right_jack;
        break;
      case LIST_ENTRY_MIDI:
        lives_widget_show_all(prefsw->scrollw_right_midi);
        prefsw->right_shown = prefsw->scrollw_right_midi;
#ifdef OMC_MIDI_IMPL
#ifndef ALSA_MIDI
        lives_widget_hide(prefsw->midi_hbox);
#endif
#endif
        break;
      case LIST_ENTRY_GUI:
      default:
        lives_widget_show_all(prefsw->scrollw_right_gui);
        prefsw->right_shown = prefsw->scrollw_right_gui;
        lives_widget_show_all(prefsw->scrollw_right_gui);
        if (nmons<=1) {
          lives_widget_hide(prefsw->forcesmon_hbox);
#if !LIVES_HAS_GRID_WIDGET
          lives_widget_hide(prefsw->ce_thumbs);
#endif
        }
        prefs_current_page=LIST_ENTRY_GUI;
      }
    }
  }
  lives_widget_queue_draw(prefsw->prefs_dialog);

}

/*
 * Function makes apply button sensitive
 */
void apply_button_set_enabled(LiVESWidget *widget, livespointer func_data) {
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->cancelbutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), FALSE);
}

// toggle sets other widget sensitive/insensitive
static void toggle_set_sensitive(LiVESWidget *widget, livespointer func_data) {
  lives_widget_set_sensitive(LIVES_WIDGET(func_data), lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget)));
}

// toggle sets other widget insensitive/sensitive
static void toggle_set_insensitive(LiVESWidget *widget, livespointer func_data) {
  lives_widget_set_sensitive(LIVES_WIDGET(func_data), !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget)));
}



static void spinbutton_crit_ds_value_changed(LiVESSpinButton *crit_ds, livespointer user_data) {
  double myval=lives_spin_button_get_value(crit_ds);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds),myval,DS_WARN_CRIT_MAX);
  apply_button_set_enabled(NULL,NULL);
}


/*
 * Function creates preferences dialog
 */
_prefsw *create_prefs_dialog(void) {
  LiVESWidget *dialog_vbox_main;
  LiVESWidget *dialog_table;
  LiVESWidget *dialog_hpaned;
  LiVESWidget *list_scroll;

  LiVESPixbuf *pixbuf_multitrack;
  LiVESPixbuf *pixbuf_gui;
  LiVESPixbuf *pixbuf_decoding;
  LiVESPixbuf *pixbuf_playback;
  LiVESPixbuf *pixbuf_recording;
  LiVESPixbuf *pixbuf_encoding;
  LiVESPixbuf *pixbuf_effects;
  LiVESPixbuf *pixbuf_directories;
  LiVESPixbuf *pixbuf_warnings;
  LiVESPixbuf *pixbuf_misc;
  LiVESPixbuf *pixbuf_themes;
  LiVESPixbuf *pixbuf_net;
  LiVESPixbuf *pixbuf_jack;
  LiVESPixbuf *pixbuf_midi;
  char *icon;

  LiVESWidget *ins_resample;
  LiVESWidget *hbox;

  LiVESWidget *hbox1;
  LiVESWidget *hbox2;
  LiVESWidget *vbox;

  LiVESWidget *dirbutton;
  LiVESWidget *filebutton;

  LiVESWidget *pp_combo;
  LiVESWidget *png;
  LiVESWidget *frame;
  LiVESWidget *mt_enter_defs;

  LiVESWidget *advbutton;
  LiVESWidget *rbutton;

  LiVESWidget *sp_red,*sp_green,*sp_blue;
  
#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  LiVESWidget *raw_midi_button;
#endif
#endif

  LiVESWidget *label;
  LiVESWidget *buttond;

  // radio button groups
  //LiVESSList *rb_group = NULL;
  LiVESSList *jpeg_png = NULL;
  LiVESSList *mt_enter_prompt = NULL;
  LiVESSList *rb_group2 = NULL;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  LiVESSList *alsa_midi_group = NULL;
#endif
#endif

  LiVESSList *autoback_group = NULL;
  LiVESSList *st_interface_group = NULL;

  LiVESSList *asrc_group=NULL;

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  // drop down lists
  LiVESList *themes = NULL;
  LiVESList *ofmt = NULL;
  LiVESList *ofmt_all = NULL;
  LiVESList *audp = NULL;
  LiVESList *encoders = NULL;
  LiVESList *vid_playback_plugins = NULL;

  lives_colRGBA64_t rgba;

  char **array;
  char *tmp,*tmp2,*tmp3;
  char *theme;

  boolean pfsm;
  boolean has_ap_rec = FALSE;

  int dph;

  register int i;

  // Allocate memory for the preferences structure
  prefsw = (_prefsw *)(lives_malloc(sizeof(_prefsw)));
  prefsw->right_shown = NULL;
  mainw->prefs_need_restart = FALSE;

  // Create new modal dialog window and set some attributes
  prefsw->prefs_dialog = lives_standard_dialog_new(_("LiVES: - Preferences"),FALSE,PREF_WIN_WIDTH, PREF_WIN_HEIGHT);
  lives_window_add_accel_group(LIVES_WINDOW(prefsw->prefs_dialog), accel_group);

  lives_window_set_default_size(LIVES_WINDOW(prefsw->prefs_dialog), PREF_WIN_WIDTH, PREF_WIN_HEIGHT);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(prefsw->prefs_dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(prefsw->prefs_dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  // Get dialog's vbox and show it
  dialog_vbox_main = lives_dialog_get_content_area(LIVES_DIALOG(prefsw->prefs_dialog));
  lives_widget_show(dialog_vbox_main);

  // Create dialog horizontal panels
  dialog_hpaned = lives_hpaned_new();
  lives_widget_show(dialog_hpaned);



  // Create dialog table for the right panel controls placement
  dialog_table = lives_table_new(1, 1, FALSE);
  lives_widget_show(dialog_table);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(dialog_hpaned, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }


  // Create preferences list with invisible headers
  prefsw->prefs_list = lives_tree_view_new();
  lives_widget_show(prefsw->prefs_list);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(prefsw->prefs_list, LIVES_WIDGET_STATE_SELECTED, &palette->normal_back);
    lives_widget_set_fg_color(prefsw->prefs_list, LIVES_WIDGET_STATE_SELECTED, &palette->normal_fore);
  }

  lives_tree_view_set_headers_visible(LIVES_TREE_VIEW(prefsw->prefs_list), FALSE);

  // Place panels into main vbox
  lives_box_pack_start(LIVES_BOX(dialog_vbox_main), dialog_hpaned, TRUE, TRUE, 0);

  // Place list on the left panel
  pref_init_list(prefsw->prefs_list);

  list_scroll = lives_scrolled_window_new(lives_tree_view_get_hadjustment(LIVES_TREE_VIEW(prefsw->prefs_list)), NULL);
  lives_widget_show(list_scroll);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(list_scroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);
  lives_container_add(LIVES_CONTAINER(list_scroll), prefsw->prefs_list);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(prefsw->prefs_list, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_fg_color(prefsw->prefs_list, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }

  lives_paned_pack(1, LIVES_PANED(dialog_hpaned), list_scroll, TRUE, FALSE);
  // Place table on the right panel

  lives_paned_pack(2, LIVES_PANED(dialog_hpaned), dialog_table, TRUE, FALSE);


  lives_paned_set_position(LIVES_PANED(dialog_hpaned),PREFS_PANED_POS);

  // -------------------,
  // gui controls       |
  // -------------------'
  prefsw->vbox_right_gui = lives_vbox_new(FALSE, widget_opts.packing_height);

  prefsw->scrollw_right_gui = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_gui);

  lives_widget_show(prefsw->vbox_right_gui);
  prefsw->right_shown = prefsw->vbox_right_gui;
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_gui), widget_opts.packing_width*2);

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->fs_max_check =
    lives_standard_check_button_new(_("Open file selection maximised"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->fs_max_check), prefs->fileselmax);
  // ---

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->recent_check =
    lives_standard_check_button_new(_("Show recent files in the File menu"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check), prefs->show_recent);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->stop_screensaver_check =
    lives_standard_check_button_new(_("Stop screensaver on playback    "),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->stop_screensaver_check), prefs->stop_screensaver);

  add_fill_to_box(LIVES_BOX(hbox));

  // ---

  prefsw->open_maximised_check=lives_standard_check_button_new(_("Open main window maximised"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->open_maximised_check), prefs->open_maximised);

  // --
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->show_tool =
    lives_standard_check_button_new(_("Show toolbar when background is blanked"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->show_tool), future_prefs->show_tool);

  add_fill_to_box(LIVES_BOX(hbox));

  // ---

  prefsw->mouse_scroll =
    lives_standard_check_button_new(_("Allow mouse wheel to switch clips"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mouse_scroll), prefs->mouse_scroll_clips);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---


  prefsw->checkbutton_ce_maxspect =
    lives_standard_check_button_new(_("Shrink previews to fit in interface"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_ce_maxspect), prefs->ce_maxspect);

  add_fill_to_box(LIVES_BOX(hbox));


  prefsw->checkbutton_button_icons =
    lives_standard_check_button_new(_("Show icons in buttons"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_button_icons), prefs->show_button_images);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  // ---
  label = lives_standard_label_new(_("Startup mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

  // ---
  prefsw->rb_startup_ce = lives_standard_radio_button_new(_("_Clip editor"),TRUE,st_interface_group,LIVES_BOX(hbox),NULL);
  st_interface_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->rb_startup_ce));

  // ---
  prefsw->rb_startup_mt = lives_standard_radio_button_new(_("_Multitrack mode"),TRUE,st_interface_group,LIVES_BOX(hbox),NULL);

  // ---
  if (future_prefs->startup_interface==STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_mt),TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce),TRUE);
  }

  add_fill_to_box(LIVES_BOX(hbox));

  //
  // multihead support (inside Gui part)
  //

  pfsm=prefs->force_single_monitor;
  prefs->force_single_monitor=FALSE;
  get_monitors();
  nmons=capable->nmonitors;

  // ---
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));
  // ---
  label = lives_standard_label_new(_("Multi-head support"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), label, FALSE, FALSE, widget_opts.packing_height);
  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height*2);
  // ---


  prefsw->spinbutton_gmoni = lives_standard_spin_button_new(_(" monitor number for LiVES interface"), TRUE, prefs->gui_monitor, 1, nmons,
                             1., 1., 0, LIVES_BOX(hbox),NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_pmoni = lives_standard_spin_button_new(_(" monitor number for playback"), TRUE, prefs->play_monitor, 0,
                             nmons==1?0:nmons,
                             1., 1., 0, LIVES_BOX(hbox),NULL);



  prefs->force_single_monitor=pfsm;
  get_monitors();


  // ---

  label = lives_standard_label_new(_("A setting of 0 means use all available monitors (only works with some playback plugins)."));

  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
  // ---

  // ---

  prefsw->forcesmon_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), prefsw->forcesmon_hbox, FALSE, FALSE, widget_opts.packing_height*2);

  prefsw->forcesmon = lives_standard_check_button_new((tmp=lives_strdup(_("Force single monitor"))),FALSE,LIVES_BOX(prefsw->forcesmon_hbox),
                      (tmp2=lives_strdup(_("Ignore all except the first monitor."))));
  lives_free(tmp);
  lives_free(tmp2);

  add_fill_to_box(LIVES_BOX(hbox));

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon), prefs->force_single_monitor);

  // ---
  if (nmons<=1) {
    lives_widget_set_sensitive(prefsw->spinbutton_gmoni,FALSE);
    lives_widget_set_sensitive(prefsw->spinbutton_pmoni,FALSE);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->forcesmon), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_forcesmon_toggled),
                       NULL);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->ce_thumbs = lives_standard_check_button_new(_("Show clip thumbnails during playback"),TRUE,
                      LIVES_BOX(hbox),NULL);

  lives_widget_set_sensitive(prefsw->ce_thumbs, prefs->play_monitor!=prefs->gui_monitor&&
                             prefs->play_monitor!=0&&!prefs->force_single_monitor&&
                             capable->nmonitors>0);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->ce_thumbs), prefs->ce_thumb_mode);

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_gui.png", NULL);
  pixbuf_gui = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_gui, _("GUI"), LIST_ENTRY_GUI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_gui);

  // -----------------------,
  // multitrack controls    |
  // -----------------------'

  prefsw->vbox_right_multitrack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_multitrack), widget_opts.border_width);

  prefsw->scrollw_right_multitrack = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_multitrack);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When entering Multitrack mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  // ---
  add_fill_to_box(LIVES_BOX(hbox));
  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->mt_enter_prompt = lives_standard_radio_button_new(_("_Prompt me for width, height, fps and audio settings"),TRUE,
                            mt_enter_prompt,LIVES_BOX(hbox),NULL);
  mt_enter_prompt = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->mt_enter_prompt));


  // ---
  mt_enter_defs = lives_standard_radio_button_new(_("_Always use the following values:"),TRUE,
                  mt_enter_prompt,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt_enter_defs), !prefs->mt_enter_prompt);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_render_prompt = lives_standard_check_button_new(_("Use these same _values for rendering a new clip"),TRUE,
                                      LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt), !prefs->render_prompt);




  frame=add_video_options(&prefsw->spinbutton_mt_def_width,prefs->mt_def_width,&prefsw->spinbutton_mt_def_height,
                          prefs->mt_def_height,&prefsw->spinbutton_mt_def_fps,prefs->mt_def_fps,FALSE);


  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), frame, FALSE, FALSE, widget_opts.packing_height);



  hbox=add_audio_options(&prefsw->backaudio_checkbutton,&prefsw->pertrack_checkbutton);


  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton), prefs->mt_backaudio>0);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton), prefs->mt_pertrack_audio);


  // must be done after creating check buttons
  resaudw=create_resaudw(4, NULL, prefsw->vbox_right_multitrack);
  // ---


  // must be done after resaudw
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);


  lives_widget_set_sensitive(prefsw->backaudio_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  lives_widget_set_sensitive(prefsw->pertrack_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  // ---
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_multitrack));
  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  // ---
  prefsw->spinbutton_mt_undo_buf = lives_standard_spin_button_new(_("    _Undo buffer size (MB)    "),TRUE,
                                   prefs->mt_undo_buf, 0., LIVES_MAXSIZE/(1024.*1024.), 1., 1., 0,
                                   LIVES_BOX(hbox),NULL);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  // ---
  prefsw->checkbutton_mt_exit_render = lives_standard_check_button_new(_("_Exit multitrack mode after rendering"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);
  // ---

  hbox2 = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox2, FALSE, FALSE, widget_opts.packing_height);
  // ---
  label = lives_standard_label_new(_("Auto backup layouts"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, TRUE, 0);
  // ---

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->mt_autoback_every = lives_standard_radio_button_new(_("_Every"),TRUE,autoback_group,LIVES_BOX(hbox),NULL);
  autoback_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->mt_autoback_every));
  lives_box_pack_start(LIVES_BOX(hbox2), hbox, TRUE, TRUE, 0);

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  widget_opts.swap_label=TRUE;
  prefsw->spinbutton_mt_ab_time = lives_standard_spin_button_new(_("seconds"),FALSE,120.,10.,1800.,1.,10.,0,LIVES_BOX(hbox),NULL);
  widget_opts.swap_label=FALSE;
  lives_box_pack_start(LIVES_BOX(hbox2), hbox, TRUE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox2));
  // ---

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->mt_autoback_always = lives_standard_radio_button_new(_("After every _change"),TRUE,autoback_group,LIVES_BOX(hbox),NULL);
  autoback_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->mt_autoback_always));
  lives_box_pack_start(LIVES_BOX(hbox2), hbox, TRUE, TRUE, 0);
  // ---

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->mt_autoback_never = lives_standard_radio_button_new(_("_Never"),TRUE,autoback_group,LIVES_BOX(hbox),NULL);
  autoback_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->mt_autoback_never));
  lives_box_pack_start(LIVES_BOX(hbox2), hbox, TRUE, TRUE, 0);


  // ---
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_every), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(on_mtbackevery_toggled),
                       prefsw);

  if (prefs->mt_auto_back==0) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always),TRUE);
  } else if (prefs->mt_auto_back==-1) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never),TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_every),TRUE);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time),prefs->mt_auto_back);
  }


  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox2, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_max_disp_vtracks=lives_standard_spin_button_new(_("Maximum number of visible tracks"), FALSE, prefs->max_disp_vtracks,
                                      5., 15.,
                                      1., 1., 0, LIVES_BOX(hbox2),NULL);

  // ---
  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_multitrack.png", NULL);
  pixbuf_multitrack = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_multitrack, _("Multitrack/Render"), LIST_ENTRY_MULTITRACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_multitrack);


  // ---------------,
  // decoding       |
  // ---------------'

  prefsw->vbox_right_decoding = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_decoding), widget_opts.border_width*2);

  prefsw->scrollw_right_decoding = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_decoding);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---


  prefsw->checkbutton_instant_open = lives_standard_check_button_new((tmp=lives_strdup(_("Use instant opening when possible"))),FALSE,
                                     LIVES_BOX(hbox),
                                     (tmp2=lives_strdup(_("Enable instant opening of some file types using decoder plugins"))));
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open),prefs->instant_open);

  lives_free(tmp);
  lives_free(tmp2);

  // advanced instant opening
  advbutton = lives_button_new_from_stock(LIVES_STOCK_PREFERENCES,_("_Advanced"));
  lives_box_pack_start(LIVES_BOX(hbox), advbutton, FALSE, FALSE, widget_opts.packing_width*4);

  lives_signal_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_decplug_advanced_clicked),
                       NULL);

  lives_widget_set_sensitive(advbutton,prefs->instant_open);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_instant_open), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(instopen_toggled),
                       advbutton);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  // ---
  prefsw->video_open_entry = lives_standard_entry_new(_("Video open command (fallback)"),FALSE,
                             prefs->video_open_command,-1,255,
                             LIVES_BOX(hbox),NULL);


  if (prefs->ocp==-1) prefs->ocp=get_int_pref("open_compression_percent");

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->spinbutton_ocp = lives_standard_spin_button_new(_("Open/render compression"), FALSE, prefs->ocp, 0., 100., 1., 5., 0,
                           LIVES_BOX(hbox),NULL);

  label = lives_standard_label_new(_(" %     ( lower = slower, larger files; for jpeg, higher quality )"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width>>1);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));



  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  label = lives_standard_label_new(_("Default image format"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  // ---
  prefsw->jpeg = lives_standard_radio_button_new(_("_jpeg"),TRUE,jpeg_png,LIVES_BOX(hbox),NULL);
  jpeg_png = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->jpeg));

  png = lives_standard_radio_button_new(_("_png"),TRUE,jpeg_png,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(png),!strcmp(prefs->image_ext,LIVES_FILE_EXT_PNG));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height*2);
  // ---
  label = lives_standard_label_new(_("(Check Help/Troubleshoot to see which image formats are supported)"));
  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->checkbutton_auto_deint = lives_standard_check_button_new((tmp=lives_strdup(_("Enable automatic deinterlacing when possible"))),
                                   FALSE,
                                   LIVES_BOX(hbox),
                                   (tmp2=lives_strdup(_("Automatically deinterlace frames when a plugin suggests it"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint),prefs->auto_deint);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->checkbutton_auto_trim = lives_standard_check_button_new((tmp=lives_strdup(
                                    _("Automatic trimming / padding of audio when possible"))),FALSE,
                                  LIVES_BOX(hbox),
                                  (tmp2=lives_strdup(_("Automatically trim or pad audio when a plugin suggests it"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_trim),prefs->auto_trim_audio);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->checkbutton_nobord = lives_standard_check_button_new((tmp=lives_strdup(_("Ignore blank borders when possible"))),FALSE,
                               LIVES_BOX(hbox),
                               (tmp2=lives_strdup(_("Clip any blank borders from frames where possible"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_nobord),prefs->auto_nobord);

  // ---
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));
  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_concat_images = lives_standard_check_button_new(_("When opening multiple files, concatenate images into one clip"),
                                      FALSE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_concat_images),prefs->concat_images);

  // ---
  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_decoding.png", NULL);
  pixbuf_decoding = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);


  prefs_add_to_list(prefsw->prefs_list, pixbuf_decoding, _("Decoding"), LIST_ENTRY_DECODING);

  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_decoding);

  // ---------------,
  // playback       |
  // ---------------'

  prefsw->vbox_right_playback = lives_vbox_new(FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_playback), widget_opts.border_width*2);

  prefsw->scrollw_right_playback = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_playback);

  frame = lives_frame_new(NULL);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, FALSE, FALSE, widget_opts.packing_height);

  vbox=lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  prefsw->pbq_list=NULL;
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=lives_list_append(prefsw->pbq_list,lives_strdup((_("Low - can improve performance on slower machines"))));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=lives_list_append(prefsw->pbq_list,lives_strdup((_("Normal - recommended for most users"))));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list=lives_list_append(prefsw->pbq_list,lives_strdup((_("High - can improve quality on very fast machines"))));

  widget_opts.expand=LIVES_EXPAND_EXTRA;
  prefsw->pbq_combo = lives_standard_combo_new((tmp=lives_strdup(_("Preview _quality"))),TRUE,prefsw->pbq_list,LIVES_BOX(vbox),
                      (tmp2=lives_strdup(_("The preview quality for video playback - affects resizing"))));
  widget_opts.expand=LIVES_EXPAND_DEFAULT;

  lives_free(tmp);
  lives_free(tmp2);

  switch (prefs->pb_quality) {
  case PB_QUALITY_HIGH:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 2);
    break;
  case PB_QUALITY_MED:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 1);
  }

  // ---

  prefsw->checkbutton_show_stats = lives_standard_check_button_new(_("_Show FPS statistics"),TRUE,LIVES_BOX(vbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_stats),prefs->show_player_stats);

  // ---
  add_hsep_to_box(LIVES_BOX(vbox));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  // ---
#ifndef IS_MINGW
  vid_playback_plugins = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-so");
#else
  vid_playback_plugins = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-dll");
#endif
  vid_playback_plugins = lives_list_prepend(vid_playback_plugins, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  widget_opts.expand=LIVES_EXPAND_EXTRA;
  pp_combo = lives_standard_combo_new(_("_Plugin"),TRUE,vid_playback_plugins,LIVES_BOX(hbox),NULL);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;

  advbutton = lives_button_new_from_stock(LIVES_STOCK_PREFERENCES,_("_Advanced"));
  lives_box_pack_start(LIVES_BOX(hbox), advbutton, FALSE, FALSE, 40);

  lives_signal_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vpp_advanced_clicked),
                       NULL);


  if (mainw->vpp != NULL) {
    lives_combo_set_active_string(LIVES_COMBO(pp_combo), mainw->vpp->name);
  } else {
    lives_combo_set_active_index(LIVES_COMBO(pp_combo), 0);
    lives_widget_set_sensitive(advbutton, FALSE);
  }
  lives_list_free_strings(vid_playback_plugins);
  lives_list_free(vid_playback_plugins);

  lives_signal_connect_after(LIVES_WIDGET_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(after_vpp_changed),
                             (livespointer) advbutton);

  prefsw->checkbutton_stream_audio =
    lives_standard_check_button_new((tmp=lives_strdup(_("Stream audio"))),
                                    TRUE,LIVES_BOX(vbox),
                                    (tmp2=lives_strdup
                                          (_("Stream audio to playback plugin"))));
  lives_free(tmp);
  lives_free(tmp2);


  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio),prefs->stream_audio_out);

  prefsw_set_astream_settings(mainw->vpp);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_stream_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(stream_audio_toggled), NULL);



  prefsw->checkbutton_rec_after_pb =
    lives_standard_check_button_new((tmp=lives_strdup(_("Record player output"))),
                                    TRUE,LIVES_BOX(vbox),
                                    (tmp2=lives_strdup
                                          (_("Record output from player instead of input to player"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb),(prefs->rec_opts&REC_AFTER_PB));

  prefsw_set_rec_after_settings(mainw->vpp);

  label = lives_standard_label_new(_("VIDEO"));

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_frame_set_label_widget(LIVES_FRAME(frame), label);


  //-

  frame = lives_frame_new(NULL);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, TRUE, TRUE, 0);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);

#ifdef HAVE_PULSE_AUDIO
  audp = lives_list_append(audp, lives_strdup_printf("pulse audio (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  has_ap_rec=TRUE;
#endif

#ifdef ENABLE_JACK
  if (!has_ap_rec) audp = lives_list_append(audp, lives_strdup_printf("jack (%s)",
                            mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  else audp = lives_list_append(audp, lives_strdup_printf("jack"));
  has_ap_rec=TRUE;
#endif

  if (capable->has_sox_play) {
    if (has_ap_rec) audp = lives_list_append(audp, lives_strdup("sox"));
    else audp = lives_list_append(audp, lives_strdup_printf("sox (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  }

  if (capable->has_mplayer) {
    audp = lives_list_append(audp, lives_strdup("mplayer"));
  }

  if (capable->has_mplayer2) {
    audp = lives_list_append(audp, lives_strdup("mplayer2"));
  }

  widget_opts.expand=LIVES_EXPAND_EXTRA;
  prefsw->audp_combo = lives_standard_combo_new(_("_Player"),TRUE,audp,LIVES_BOX(vbox),NULL);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;

  has_ap_rec=FALSE;

  prefsw->jack_int_label=lives_standard_label_new(_("(See also the Jack Integration tab for jack startup options)"));
  lives_box_pack_start(LIVES_BOX(vbox), prefsw->jack_int_label, FALSE, FALSE, widget_opts.packing_height);

  prefsw->audp_name=NULL;

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    prefsw->audp_name=lives_strdup_printf("pulse audio (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
  }
  has_ap_rec=TRUE;
#endif

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    if (!has_ap_rec)
      prefsw->audp_name=lives_strdup_printf("jack (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name=lives_strdup_printf("jack");
  }
  has_ap_rec=TRUE;
#endif

  if (prefs->audio_player==AUD_PLAYER_SOX) {
    if (!has_ap_rec) prefsw->audp_name=lives_strdup_printf("sox (%s)",mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name=lives_strdup_printf("sox");
  }

  if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
    prefsw->audp_name=lives_strdup(_("mplayer"));
  }
  // ---
  if (prefsw->audp_name!=NULL)
    lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->audp_name);
  prefsw->orig_audp_name=lives_strdup(prefsw->audp_name);


  //---

  if (prefs->audio_player==AUD_PLAYER_MPLAYER2) {
    prefsw->audp_name=lives_strdup(_("mplayer2"));
  }
  // ---
  prefsw->audio_command_entry = lives_standard_entry_new(_("Audio play _command"),TRUE,"",-1,255,LIVES_BOX(vbox),NULL);


  // get from prefs
  if (!is_realtime_aplayer(prefs->audio_player))
    lives_entry_set_text(LIVES_ENTRY(prefsw->audio_command_entry),prefs->audio_play_command);
  else {
    lives_entry_set_text(LIVES_ENTRY(prefsw->audio_command_entry),(_("- internal -")));
    lives_widget_set_sensitive(prefsw->audio_command_entry,FALSE);
  }


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->checkbutton_afollow = lives_standard_check_button_new(_("Audio follows video _rate/direction"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)?TRUE:FALSE);
  lives_widget_set_sensitive(prefsw->checkbutton_afollow,is_realtime_aplayer(prefs->audio_player));

  add_fill_to_box(LIVES_BOX(hbox));

  // ---
  prefsw->checkbutton_aclips = lives_standard_check_button_new(_("Audio follows _clip switches"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips),(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)?TRUE:FALSE);
  lives_widget_set_sensitive(prefsw->checkbutton_aclips,is_realtime_aplayer(prefs->audio_player));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  label=lives_standard_label_new(_("Source:"));
  lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));

  rbutton=lives_standard_radio_button_new(_("_Internal"),TRUE,asrc_group,LIVES_BOX(hbox),NULL);

  asrc_group=lives_radio_button_get_group(LIVES_RADIO_BUTTON(rbutton));
  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rextaudio = lives_standard_radio_button_new(_("_External (requires jack or pulse audio player)"),
                      TRUE,asrc_group,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio),prefs->audio_src==AUDIO_SRC_EXT);
  add_fill_to_box(LIVES_BOX(hbox));

  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->rextaudio,FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->rextaudio,FALSE);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rextaudio), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(toggle_set_insensitive),
                       prefsw->checkbutton_aclips);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rextaudio), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(toggle_set_insensitive),
                       prefsw->checkbutton_afollow);



  label = lives_standard_label_new(_("AUDIO"));
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  lives_frame_set_label_widget(LIVES_FRAME(frame), label);

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_playback.png", NULL);
  pixbuf_playback = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_playback, _("Playback"), LIST_ENTRY_PLAYBACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_playback);


  lives_widget_hide(prefsw->jack_int_label);

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    lives_widget_show(prefsw->jack_int_label);
  }
#endif


  // ---------------,
  // recording      |
  // ---------------'

  prefsw->vbox_right_recording = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_recording), widget_opts.border_width*2);

  prefsw->scrollw_right_recording = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_recording);

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->rdesk_audio = lives_standard_check_button_new(_("Record audio when capturing an e_xternal window\n (requires jack or pulse audio)"),
                        TRUE,LIVES_BOX(hbox),NULL);


#ifndef RT_AUDIO
  lives_widget_set_sensitive(prefsw->rdesk_audio,FALSE);
#endif

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rdesk_audio),prefs->rec_desktop_audio);

  // ---
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  // ---
  label = lives_standard_label_new(_("      What to record when 'r' is pressed   "));

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), label, FALSE, FALSE, widget_opts.packing_height);

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->rframes = lives_standard_check_button_new(_("_Frame changes"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rframes),prefs->rec_opts&REC_FRAMES);

  // ---
  if (prefs->rec_opts&REC_FPS||prefs->rec_opts&REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rframes,FALSE);  // we must record these if recording fps changes or clip switches
  }
  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->rframes,FALSE);
  }

  add_fill_to_box(LIVES_BOX(hbox));

  // ---
  prefsw->rfps = lives_standard_check_button_new(_("F_PS changes"),TRUE,LIVES_BOX(hbox),NULL);

  // ---
  if (prefs->rec_opts&REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rfps,FALSE);  // we must record these if recording clip switches
  }

  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->rfps,FALSE);
  }
  // ---
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rfps),prefs->rec_opts&REC_FPS);

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->reffects = lives_standard_check_button_new(_("_Real time effects"),TRUE,LIVES_BOX(hbox),NULL);

  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->reffects,FALSE);
  }

  add_fill_to_box(LIVES_BOX(hbox));

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->reffects),prefs->rec_opts&REC_EFFECTS);
  // ---

  prefsw->rclips = lives_standard_check_button_new(_("_Clip switches"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rclips),prefs->rec_opts&REC_CLIPS);

  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->rclips,FALSE);
  }


  // ---

  prefsw->raudio = lives_standard_check_button_new(_("_Audio (requires jack or pulse audio player)"),
                   TRUE,LIVES_BOX(prefsw->vbox_right_recording),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->raudio),prefs->rec_opts&REC_AUDIO);

  if (mainw->playing_file>0&&mainw->record) {
    lives_widget_set_sensitive(prefsw->raudio,FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->raudio,FALSE);
  }

  // ---
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));
  // ---

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height*4);

  prefsw->spinbutton_rec_gb = lives_standard_spin_button_new(_("Pause recording if free disk space falls below"),FALSE,
                              prefs->rec_stop_gb, 0., 1024., 1., 10., 0,
                              LIVES_BOX(hbox),NULL);


  // TRANSLATORS: gigabytes
  label = lives_standard_label_new(_("GB"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_record.png", NULL);
  pixbuf_recording = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_recording, _("Recording"), LIST_ENTRY_RECORDING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_recording);

  // ---------------,
  // encoding       |
  // ---------------'

  prefsw->vbox_right_encoding = lives_vbox_new(FALSE, widget_opts.packing_height*4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_encoding), widget_opts.border_width);

  prefsw->scrollw_right_encoding = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_encoding);

  if (capable->has_encoder_plugins) {
    // scan for encoder plugins
    encoders=get_plugin_list(PLUGIN_ENCODERS,TRUE,NULL,NULL);
  }

  widget_opts.expand=LIVES_EXPAND_EXTRA;
  prefsw->encoder_combo = lives_standard_combo_new(_("Encoder"),FALSE,encoders,
                          LIVES_BOX(prefsw->vbox_right_encoding),NULL);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;


  if (encoders!=NULL) {
    lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
    lives_list_free_strings(encoders);
    lives_list_free(encoders);
  }

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_encoding));

  if (capable->has_encoder_plugins) {
    // reqest formats from the encoder plugin
    if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
      for (i=0; i<lives_list_length(ofmt_all); i++) {
        if (get_token_count((char *)lives_list_nth_data(ofmt_all,i),'|')>2) {
          array=lives_strsplit((char *)lives_list_nth_data(ofmt_all,i),"|",-1);
          if (!strcmp(array[0],prefs->encoder.of_name)) {
            prefs->encoder.of_allowed_acodecs=atoi(array[2]);
            lives_snprintf(prefs->encoder.of_restrict,1024,"%s",array[3]);
          }
          ofmt = lives_list_append(ofmt, lives_strdup(array[1]));
          lives_strfreev(array);
        }
      }
      lives_memcpy(&future_prefs->encoder,&prefs->encoder,sizeof(_encoder));
    } else {
      do_plugin_encoder_error(prefs->encoder.name);
      future_prefs->encoder.of_allowed_acodecs=0;
    }


    widget_opts.expand=LIVES_EXPAND_EXTRA;
    prefsw->ofmt_combo = lives_standard_combo_new(_("Output format"),FALSE,ofmt,LIVES_BOX(prefsw->vbox_right_encoding),NULL);
    widget_opts.expand=LIVES_EXPAND_DEFAULT;

    if (ofmt!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), prefs->encoder.of_desc);
      lives_list_free_strings(ofmt);
      lives_list_free(ofmt);
    }

    if (ofmt_all!=NULL) {
      lives_list_free_strings(ofmt_all);
      lives_list_free(ofmt_all);
    }


    widget_opts.expand=LIVES_EXPAND_EXTRA;
    prefsw->acodec_combo = lives_standard_combo_new(_("Audio codec"),FALSE,NULL,LIVES_BOX(prefsw->vbox_right_encoding),NULL);
    widget_opts.expand=LIVES_EXPAND_DEFAULT;
    prefs->acodec_list=NULL;

    set_acodec_list_from_allowed(prefsw, rdet);

  } else prefsw->acodec_combo=NULL;

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_encoding.png", NULL);
  pixbuf_encoding = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_encoding, _("Encoding"), LIST_ENTRY_ENCODING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_encoding);

  // ---------------,
  // effects        |
  // ---------------'

  prefsw->vbox_right_effects = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_effects), widget_opts.border_width);

  prefsw->scrollw_right_effects = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_effects);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_antialias = lives_standard_check_button_new(_("Use _antialiasing when resizing"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_antialias), prefs->antialias);
  //

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_rte_keys = lives_standard_spin_button_new
                                ((tmp=lives_strdup(_("Number of _real time effect keys"))),TRUE,prefs->rte_keys_virtual, FX_KEYS_PHYSICAL,
                                 FX_KEYS_MAX_VIRTUAL, 1., 1., 0, LIVES_BOX(hbox),
                                 (tmp2=lives_strdup(
                                         _("The number of \"virtual\" real time effect keys. They can be controlled through the real time effects window, or via network (OSC)."))));
  lives_free(tmp);
  lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_threads = lives_standard_check_button_new(_("Use _threads where possible when applying effects"),TRUE,LIVES_BOX(hbox),
                                NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads), future_prefs->nfx_threads>1);

  //

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->spinbutton_nfx_threads = lives_standard_spin_button_new(_("Number of _threads"),TRUE,future_prefs->nfx_threads, 2., 65536., 1., 1.,
                                   0,
                                   LIVES_BOX(hbox),NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  if (future_prefs->nfx_threads==1) lives_widget_set_sensitive(prefsw->spinbutton_nfx_threads,FALSE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_effects));


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Restart is required if any of the following paths are changed:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
  // ---
  add_fill_to_box(LIVES_BOX(hbox));


  dph=widget_opts.packing_height;
  widget_opts.packing_height*=4;

  prefsw->wpp_entry=lives_standard_entry_new(_("Weed plugin path"),TRUE,prefs->weed_plugin_path,-1,PATH_MAX,
                    LIVES_BOX(prefsw->vbox_right_effects),NULL);

  prefsw->frei0r_entry=lives_standard_entry_new(_("Frei0r plugin path"),TRUE,prefs->frei0r_path,-1,PATH_MAX,
                       LIVES_BOX(prefsw->vbox_right_effects),NULL);

  prefsw->ladspa_entry=lives_standard_entry_new(_("LADSPA plugin path"),TRUE,prefs->ladspa_path,-1,PATH_MAX,
                       LIVES_BOX(prefsw->vbox_right_effects),NULL);

  widget_opts.packing_height=dph;


  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_effects.png", NULL);
  pixbuf_effects = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_effects, _("Effects"), LIST_ENTRY_EFFECTS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_effects);

  // -------------------,
  // Directories        |
  // -------------------'

  prefsw->table_right_directories = lives_table_new(10, 3, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->table_right_directories), widget_opts.border_width*2);
  lives_table_set_col_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_width);
  lives_table_set_row_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_height*4);

  prefsw->scrollw_right_directories = lives_standard_scrolled_window_new(0,0,prefsw->table_right_directories);

  label = lives_standard_label_new(_("      Video load directory (default)      "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 4, 5,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("      Video save directory (default) "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 5, 6,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("      Audio load directory (default) "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 6, 7,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("      Image directory (default) "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 7, 8,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("      Backup/Restore directory (default) "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 8, 9,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("      Temp directory (do not remove) "));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);


  prefsw->vid_load_dir_entry = lives_entry_new();
  lives_entry_set_max_length(LIVES_ENTRY(prefsw->vid_load_dir_entry),PATH_MAX);
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_load_dir_entry, 1, 2, 4, 5,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_widget_set_tooltip_text(prefsw->vid_load_dir_entry, _("The default directory for loading video clips from"));



  // tempdir warning label


  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new("");
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
  set_temp_label_text(LIVES_LABEL(label));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 3, 0, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.65);

  prefsw->temp_label=label;


  // get from prefs
  lives_entry_set_text(LIVES_ENTRY(prefsw->vid_load_dir_entry),prefs->def_vid_load_dir);



  prefsw->vid_save_dir_entry = lives_standard_entry_new(NULL,FALSE,prefs->def_vid_save_dir,-1,PATH_MAX,
                               NULL,_("The default directory for saving encoded clips to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_save_dir_entry, 1, 2, 5, 6,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->vid_save_dir_entry),FALSE);

  prefsw->audio_dir_entry = lives_standard_entry_new(NULL,FALSE,prefs->def_audio_dir,-1,PATH_MAX,
                            NULL,_("The default directory for loading and saving audio"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->audio_dir_entry, 1, 2, 6, 7,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->audio_dir_entry),FALSE);

  prefsw->image_dir_entry = lives_standard_entry_new(NULL,FALSE,prefs->def_image_dir,-1,PATH_MAX,
                            NULL,_("The default directory for saving frameshots to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->image_dir_entry, 1, 2, 7, 8,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->image_dir_entry),FALSE);

  prefsw->proj_dir_entry = lives_standard_entry_new(NULL,FALSE,prefs->def_proj_dir,-1,PATH_MAX,
                           NULL,_("The default directory for backing up/restoring single clips"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->proj_dir_entry, 1, 2, 8, 9,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->proj_dir_entry),FALSE);

  prefsw->tmpdir_entry = lives_standard_entry_new(NULL,FALSE,(tmp=lives_filename_to_utf8(future_prefs->tmpdir,-1,NULL,NULL,NULL)),-1,PATH_MAX,
                         NULL,(tmp2=lives_strdup(_("LiVES working directory."))));

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->tmpdir_entry, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_free(tmp);
  lives_free(tmp2);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->tmpdir_entry),FALSE);

  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 4, 5,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->vid_load_dir_entry);


  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 5, 6,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->vid_save_dir_entry);


  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 6, 7,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->audio_dir_entry);


  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 7, 8,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->image_dir_entry);


  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 8, 9,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->proj_dir_entry);


  dirbutton = lives_standard_file_button_new(TRUE,NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 3, 4,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_complex_clicked),prefsw->tmpdir_entry);

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_directory.png", NULL);
  pixbuf_directories = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_directories, _("Directories"), LIST_ENTRY_DIRECTORIES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_directories);

  // ---------------,
  // Warnings       |
  // ---------------'

  prefsw->vbox_right_warnings = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_warnings = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_warnings);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->spinbutton_warn_ds = lives_standard_spin_button_new(_("Warn if diskspace falls below: "),FALSE,
                               (double)prefs->ds_warn_level/1000000.,
                               (double)prefs->ds_crit_level/1000000.,
                               DS_WARN_CRIT_MAX, 1., 10., 0,
                               LIVES_BOX(hbox),NULL);

  label = lives_standard_label_new(_(" MB [set to 0 to disable]"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width>>1);

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->spinbutton_crit_ds = lives_standard_spin_button_new(_("Diskspace critical level: "),FALSE,
                               (double)prefs->ds_crit_level/1000000., 0.,
                               DS_WARN_CRIT_MAX, 1., 10., 0,
                               LIVES_BOX(hbox),NULL);

  label = lives_standard_label_new(_(" MB [set to 0 to disable]"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width>>1);


  // ---

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_warnings));



  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_fps = lives_standard_check_button_new(
                                   _("Warn on Insert / Merge if _frame rate of clipboard does not match frame rate of selection"),
                                   TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fps), !(prefs->warning_mask&WARN_MASK_FPS));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  // ---
  prefsw->checkbutton_warn_fsize = lives_standard_check_button_new(_("Warn on Open if file _size exceeds "),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fsize), !(prefs->warning_mask&WARN_MASK_FSIZE));

  prefsw->spinbutton_warn_fsize = lives_standard_spin_button_new(NULL,FALSE,
                                  prefs->warn_file_size, 1., 2048., 1., 10., 0,
                                  LIVES_BOX(hbox),NULL);


  label = lives_standard_label_new(_(" MB"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width>>1);

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_save_set = lives_standard_check_button_new(_("Show a warning before saving a se_t"),
                                      TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_set), !(prefs->warning_mask&WARN_MASK_SAVE_SET));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_mplayer = lives_standard_check_button_new
                                     (_("Show a warning if _mplayer/mplayer2, sox, composite or convert is not found when LiVES is started."),
                                      TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mplayer), !(prefs->warning_mask&WARN_MASK_NO_MPLAYER));


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_rendered_fx = lives_standard_check_button_new
                                         (_("Show a warning if no _rendered effects are found at startup."),
                                          TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_rendered_fx), !(prefs->warning_mask&WARN_MASK_RENDERED_FX));


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);


  prefsw->checkbutton_warn_encoders = lives_standard_check_button_new
                                      (_("Show a warning if no _encoder plugins are found at startup."),
                                       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_encoders), !(prefs->warning_mask&WARN_MASK_NO_ENCODERS));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_dup_set = lives_standard_check_button_new
                                     (_("Show a warning if a _duplicate set name is entered."),
                                      TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_dup_set), !(prefs->warning_mask&WARN_MASK_DUPLICATE_SET));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);


  prefsw->checkbutton_warn_layout_clips = lives_standard_check_button_new
                                          (_("When a set is loaded, warn if clips are missing from _layouts."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_clips),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_MISSING_CLIPS));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_close = lives_standard_check_button_new
                                          (_("Warn if a clip used in a layout is about to be closed."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_close),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_CLOSE_FILE));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_delete = lives_standard_check_button_new
      (_("Warn if frames used in a layout are about to be deleted."),
       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_delete),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_shift = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be shifted."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_shift),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_alter = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be altered."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_alter),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_adel = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be deleted."),
                                          TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_adel),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO));

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_ashift = lives_standard_check_button_new
      (_("Warn if audio used in a layout is about to be shifted."),
       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_ashift),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_aalt = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be altered."),
                                          TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_aalt),
                                 !(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_popup = lives_standard_check_button_new
                                          (_("Popup layout errors after clip changes."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_popup), !(prefs->warning_mask&WARN_MASK_LAYOUT_POPUP));

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_discard_layout = lives_standard_check_button_new
      (_("Warn if the layout has not been saved when leaving multitrack mode."),
       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_discard_layout), !(prefs->warning_mask&WARN_MASK_EXIT_MT));

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_mt_achans = lives_standard_check_button_new
                                       (_("Warn if multitrack has no audio channels, and a layout with audio is loaded."),
                                        TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_achans), !(prefs->warning_mask&WARN_MASK_MT_ACHANS));

  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_mt_no_jack = lives_standard_check_button_new
                                        (_("Warn if multitrack has audio channels, and your audio player is not \"jack\" or \"pulse audio\"."),
                                         TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_no_jack), !(prefs->warning_mask&WARN_MASK_MT_NO_JACK));

  // ---

#ifdef HAVE_LDVGRAB
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_after_dvgrab = lives_standard_check_button_new
                                          (_("Show info message after importing from firewire device."),
                                              TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_dvgrab), !(prefs->warning_mask&WARN_MASK_AFTER_DVGRAB));

#else
  prefsw->checkbutton_warn_after_dvgrab = NULL;
#endif

  // ---

#ifdef HAVE_YUV4MPEG
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_yuv4m_open = lives_standard_check_button_new
                                        (_("Show a warning before opening a yuv4mpeg stream (advanced)."),
                                         TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_yuv4m_open), !(prefs->warning_mask&WARN_MASK_OPEN_YUV4M));
#else
  prefsw->checkbutton_warn_yuv4m_open = NULL;
#endif


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_mt_backup_space = lives_standard_check_button_new
      (_("Show a warning when multitrack is low on backup space."),
       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_backup_space),
                                 !(prefs->warning_mask&WARN_MASK_MT_BACKUP_SPACE));

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_after_crash = lives_standard_check_button_new
                                         (_("Show a warning advising cleaning of disk space after a crash."),
                                          TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_crash),
                                 !(prefs->warning_mask&WARN_MASK_CLEAN_AFTER_CRASH));

  // ---

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_no_pulse = lives_standard_check_button_new
                                      (_("Show a warning if unable to connect to pulseaudio player."),
                                       TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_no_pulse), !(prefs->warning_mask&WARN_MASK_NO_PULSE_CONNECT));

  // ---


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_warnings), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

  prefsw->checkbutton_warn_layout_wipe = lives_standard_check_button_new
                                         (_("Show a warning before wiping a layout which has unsaved changes."),
                                          TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_wipe), !(prefs->warning_mask&WARN_MASK_LAYOUT_WIPE));

  // ---


  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_warning.png", NULL);
  pixbuf_warnings = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_warnings, _("Warnings"), LIST_ENTRY_WARNINGS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_warnings);


  // -----------,
  // Misc       |
  // -----------'

  prefsw->vbox_right_misc = lives_vbox_new(FALSE, widget_opts.packing_height*4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_misc), widget_opts.border_width*2);

  prefsw->scrollw_right_misc = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_misc);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);


  prefsw->check_midi = lives_standard_check_button_new
                       (_("Midi synch (requires the files midistart and midistop)"),
                        TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->check_midi), prefs->midisynch);

  lives_widget_set_sensitive(prefsw->check_midi,capable->has_midistartstop);



  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When inserting/merging frames:  "));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width*2);

  prefsw->ins_speed = lives_standard_radio_button_new(_("_Speed Up/Slow Down Insertion"),TRUE,rb_group2,LIVES_BOX(hbox),NULL);
  rb_group2 = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->ins_speed));

  // ---
  ins_resample = lives_standard_radio_button_new(_("_Resample Insertion"),TRUE,rb_group2,LIVES_BOX(hbox),NULL);

  // ---

  prefsw->cdda_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), prefsw->cdda_hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->cdplay_entry = lives_standard_entry_new((tmp=lives_strdup(_("CD device           "))),FALSE,
                         (tmp2=lives_filename_to_utf8(prefs->cdplay_device,-1,NULL,NULL,NULL)),
                         -1,PATH_MAX,LIVES_BOX(prefsw->cdda_hbox),
                         (tmp3=lives_strdup(_("LiVES can load audio tracks from this CD"))));
  lives_free(tmp);
  lives_free(tmp2);
  lives_free(tmp3);

  buttond = lives_standard_file_button_new(FALSE,LIVES_DEVICE_DIR);
  lives_box_pack_start(LIVES_BOX(prefsw->cdda_hbox),buttond,FALSE,FALSE,widget_opts.packing_width);

  lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       (livespointer)prefsw->cdplay_entry);



  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_def_fps = lives_standard_spin_button_new((tmp=lives_strdup(_("Default FPS        "))),TRUE,
                               prefs->default_fps, 1., FPS_MAX, 1., 1., 3,
                               LIVES_BOX(hbox),
                               (tmp2=lives_strdup(_("Frames per second to use when none is specified"))));
  lives_free(tmp);
  lives_free(tmp2);


  icon = lives_strdup_printf("%s%s/pref_misc.png", prefs->prefix_dir, ICON_DIR);
  pixbuf_misc = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_misc, _("Misc"), LIST_ENTRY_MISC);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_misc);


  if (!capable->has_cdda2wav) {
    lives_widget_hide(prefsw->cdda_hbox);
  }


  // -----------,
  // Themes     |
  // -----------'

  prefsw->vbox_right_themes = lives_vbox_new(FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_themes), widget_opts.border_width*2);

  prefsw->scrollw_right_themes = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_themes);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), hbox, TRUE, FALSE, widget_opts.packing_height);

  // scan for themes
  themes = get_plugin_list(PLUGIN_THEMES, TRUE, NULL, NULL);
  themes = lives_list_prepend(themes, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  prefsw->theme_combo = lives_standard_combo_new(_("New theme:           "),FALSE,themes,LIVES_BOX(hbox),NULL);
  

  if (lives_ascii_strcasecmp(future_prefs->theme, "none")) {
    theme = lives_strdup(future_prefs->theme);
  } else theme = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  // ---
  lives_combo_set_active_string(LIVES_COMBO(prefsw->theme_combo), theme);
  //---
  lives_free(theme);
  lives_list_free_strings(themes);
  lives_list_free(themes);

  //
  frame = lives_frame_new(NULL);
  label = lives_standard_label_new(_("Main Theme Details"));
  lives_frame_set_label_widget(LIVES_FRAME(frame), label);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox=lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  ///////////////////
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->normal_fore);
  prefsw->cbutton_fore = lives_standard_color_button_new(LIVES_BOX(hbox),_("        _Foreground Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_fore,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->normal_back);
  prefsw->cbutton_back = lives_standard_color_button_new(LIVES_BOX(hbox),_("        _Background Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_back,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->menu_and_bars_fore);
  prefsw->cbutton_mabf = lives_standard_color_button_new(LIVES_BOX(hbox),_("_Alt Foreground Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_mabf,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->menu_and_bars);
  prefsw->cbutton_mab = lives_standard_color_button_new(LIVES_BOX(hbox),_("_Alt Background Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_mab,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->info_text);
  prefsw->cbutton_infot = lives_standard_color_button_new(LIVES_BOX(hbox),_("              Info _Text Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,
                          NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_infot,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->info_base);
  prefsw->cbutton_infob = lives_standard_color_button_new(LIVES_BOX(hbox),_("              Info _Base Color"),TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,
                          NULL);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(prefsw->cbutton_infob,FALSE);
    lives_widget_set_sensitive(sp_red,FALSE);
    lives_widget_set_sensitive(sp_green,FALSE);
    lives_widget_set_sensitive(sp_blue,FALSE);
  }
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->theme_style3=lives_standard_check_button_new((tmp=lives_strdup(_("Theme is _light"))),TRUE,LIVES_BOX(hbox),
						       (tmp2=lives_strdup(_("Affects some contrast details of the timeline"))));
  lives_free(tmp);
  lives_free(tmp2);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style3), palette->style&STYLE_3);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(prefsw->theme_style3,FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

#if !GTK_CHECK_VERSION(3,0,0)
  prefsw->theme_style2=lives_standard_check_button_new(_("Color the start/end frame spinbuttons (requires restart)"),FALSE,LIVES_BOX(hbox),
                       NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style2), palette->style&STYLE_2);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(prefsw->theme_style3,FALSE);
#else
  prefsw->theme_style2=NULL;
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->theme_style4=lives_standard_check_button_new(_("Highlight horizontal separators in multitrack"),FALSE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style4), (palette->style&STYLE_4));
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(prefsw->theme_style4,FALSE);


  //
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->frameblank_entry = lives_standard_entry_new((tmp=lives_strdup(_("Frame blank image"))),TRUE,mainw->frameblank_path,
                             -1,PATH_MAX,LIVES_BOX(hbox),
                             (tmp2=lives_strdup(_("The frame image which is shown when there is no clip loaded."))));
  lives_free(tmp);
  lives_free(tmp2);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(prefsw->frameblank_entry,FALSE);

  filebutton = lives_standard_file_button_new(FALSE,NULL);
  lives_box_pack_start(LIVES_BOX(hbox), filebutton, FALSE, FALSE, widget_opts.packing_width);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(filebutton,FALSE);

  lives_signal_connect(filebutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->frameblank_entry);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(filebutton),"filter",widget_opts.image_filter);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(filebutton),"filesel_type",LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->sepimg_entry = lives_standard_entry_new((tmp=lives_strdup(_("Separator image"))),TRUE,mainw->sepimg_path,
                         -1,PATH_MAX,LIVES_BOX(hbox),
                         (tmp2=lives_strdup(_("The image shown in the center of the interface."))));
  lives_free(tmp);
  lives_free(tmp2);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(prefsw->sepimg_entry,FALSE);

  filebutton = lives_standard_file_button_new(FALSE,NULL);
  lives_box_pack_start(LIVES_BOX(hbox), filebutton, FALSE, FALSE, widget_opts.packing_width);
  if (!lives_ascii_strcasecmp(future_prefs->theme,mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))
    lives_widget_set_sensitive(filebutton,FALSE);

  lives_signal_connect(filebutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),prefsw->sepimg_entry);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(filebutton),"filter",widget_opts.image_filter);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(filebutton),"filesel_type",LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));



  frame = lives_frame_new(NULL);
  label = lives_standard_label_new(_("Extended Theme Details"));
  lives_frame_set_label_widget(LIVES_FRAME(frame), label);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox=lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  ///////////////////
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_cesel = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Selected frames/audio (clip editor)"))),
							  TRUE,FALSE,&palette->ce_sel,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_ceunsel = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Unselected frames/audio (clip editor)"))),
							  TRUE,FALSE,&palette->ce_unsel,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_evbox = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Track background (multitrack)"))),
							   TRUE,FALSE,&palette->mt_evbox,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_vidcol = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Video block (multitrack)"))),
							   TRUE,FALSE,&palette->vidcol,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_audcol = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Audio block (multitrack)"))),
							   TRUE,FALSE,&palette->audcol,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_fxcol = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Effects block (multitrack)"))),
							  TRUE,FALSE,&palette->fxcol,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_mtmark = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Timeline mark (multitrack)"))),
							   TRUE,FALSE,&palette->mt_mark,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_tlreg = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Timeline selection (multitrack)"))),
							  TRUE,FALSE,&palette->mt_timeline_reg,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->mt_timecode_bg);
  prefsw->cbutton_tcbg = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Timecode background (multitrack)"))),
							 TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_color_to_lives_rgba(&rgba,&palette->mt_timecode_fg);
  prefsw->cbutton_tcfg = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Timecode foreground (multitrack)"))),
							 TRUE,FALSE,&rgba,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->cbutton_fsur = lives_standard_color_button_new(LIVES_BOX(hbox),(tmp=lives_strdup(_("Frame surround"))),
							 TRUE,FALSE,&palette->frame_surround,&sp_red,&sp_green,&sp_blue,NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(sp_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  
  

  
  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_themes.png", NULL);
  pixbuf_themes = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_themes, _("Themes/Colors"), LIST_ENTRY_THEMES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_themes);


  // --------------------------,
  // streaming/networking      |
  // --------------------------'

  prefsw->vbox_right_net = lives_vbox_new(FALSE, widget_opts.packing_height*4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_net), widget_opts.border_width);

  prefsw->scrollw_right_net = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_net);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_bwidth = lives_standard_spin_button_new(_("Download bandwidth (Kb/s)       "),FALSE,
                              prefs->dl_bandwidth, 0, 100000, 1, 10, 0,
                              LIVES_BOX(hbox),NULL);


  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_net));

#ifndef ENABLE_OSC
  label = lives_standard_label_new(_("LiVES must be compiled without \"configure --disable-OSC\" to use OMC"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), label, FALSE, FALSE, widget_opts.packing_height);
#endif

  hbox1 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox1, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, 0);

  prefsw->enable_OSC = lives_standard_check_button_new(_("OMC remote control enabled"),FALSE,LIVES_BOX(hbox),NULL);


#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->enable_OSC,FALSE);
#endif
  // ---

  prefsw->spinbutton_osc_udp = lives_standard_spin_button_new(_("UDP port       "),FALSE,
                               prefs->osc_udp_port, 1., 65535., 1., 10., 0,
                               LIVES_BOX(hbox),NULL);


  // ---
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->enable_OSC_start = lives_standard_check_button_new(_("Start OMC on startup"),FALSE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC_start), future_prefs->osc_start);

#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp,FALSE);
  lives_widget_set_sensitive(prefsw->enable_OSC_start,FALSE);
  lives_widget_set_sensitive(label,FALSE);
#else
  if (prefs->osc_udp_started) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC), TRUE);
    lives_widget_set_sensitive(prefsw->spinbutton_osc_udp,FALSE);
    lives_widget_set_sensitive(prefsw->enable_OSC,FALSE);
  }
#endif

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_net.png", NULL);
  pixbuf_net = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_net, _("Streaming/Networking"), LIST_ENTRY_NET);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_net);

  // ----------,
  // jack      |
  // ----------'

  prefsw->vbox_right_jack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_jack), widget_opts.packing_width*2);

  prefsw->scrollw_right_jack = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_jack);

  label = lives_standard_label_new(_("Jack transport"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);

#ifndef ENABLE_JACK_TRANSPORT
  label = lives_standard_label_new(_("LiVES must be compiled with jack/transport.h and jack/jack.h present to use jack transport"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);
#else
  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->jack_tserver_entry = lives_standard_entry_new(_("Jack _transport config file"),TRUE,prefs->jack_tserver,-1,255,LIVES_BOX(hbox),
                               NULL);

  lives_widget_set_sensitive(prefsw->jack_tserver_entry,FALSE); // unused for now


  // -

  prefsw->checkbutton_start_tjack=lives_standard_check_button_new(_("Start _server on LiVES startup"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_tjack),
                                 (future_prefs->jack_opts&JACK_OPTS_START_TSERVER)?TRUE:FALSE);



  // ---
  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);
  // ---

  prefsw->checkbutton_jack_master=lives_standard_check_button_new(_("Jack transport _master (start and stop)"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master),
                                 (future_prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER)?TRUE:FALSE);


  // ---
  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_client=lives_standard_check_button_new(_("Jack transport _client (start and stop)"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client),
                                 (future_prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)?TRUE:FALSE);


  lives_signal_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_jack_client_toggled),
                             NULL);
  // ---

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_tb_start=lives_standard_check_button_new(_("Jack transport sets start position"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),
                                 (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_START)?
                                 (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client))):FALSE);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client)));

  lives_signal_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_jack_tb_start_toggled),
                             NULL);

  // ---

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->checkbutton_jack_tb_client=lives_standard_check_button_new(_("Jack transport timebase slave"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),
                                 (future_prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)?
                                 (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start))):FALSE);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)));


  // ---
  label = lives_standard_label_new(_("(See also Playback -> Audio follows video rate/direction)"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);

#endif

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_jack));


  label = lives_standard_label_new(_("Jack audio"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);

#ifndef ENABLE_JACK
  label = lives_standard_label_new(_("LiVES must be compiled with jack/jack.h present to use jack audio"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);
#else
  label = lives_standard_label_new(_("You MUST set the audio player to \"jack\" in the Playback tab to use jack audio"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->jack_aserver_entry = lives_standard_entry_new(_("Jack _audio server config file"),TRUE,prefs->jack_aserver,-1,255,LIVES_BOX(hbox),
                               NULL);

  lives_widget_set_sensitive(prefsw->jack_aserver_entry,FALSE);

  // ---
  prefsw->checkbutton_start_ajack = lives_standard_check_button_new(_("Start _server on LiVES startup"),TRUE,LIVES_BOX(hbox),NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack),
                                 (future_prefs->jack_opts&JACK_OPTS_START_ASERVER)?TRUE:FALSE);
  // ---
  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_pwp = lives_standard_check_button_new(_("Play audio even when transport is _paused"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_pwp),
                                 (future_prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp, prefs->audio_player==AUD_PLAYER_JACK);

  // ---
  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_read_autocon = lives_standard_check_button_new
                                          (_("Automatically connect to System Out ports when 'playing' External Audio"),FALSE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_read_autocon),
                                 (future_prefs->jack_opts&JACK_OPTS_NO_READ_AUTOCON)?FALSE:TRUE);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon, prefs->audio_player==AUD_PLAYER_JACK);

#endif

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_jack.png", NULL);
  pixbuf_jack = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_jack, _("Jack Integration"), LIST_ENTRY_JACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_jack);

  // ----------------------,
  // MIDI/js learner       |
  // ----------------------'


  prefsw->vbox_right_midi = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_midi = lives_standard_scrolled_window_new(0,0,prefsw->vbox_right_midi);

  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_midi), widget_opts.border_width*2);

  label = lives_standard_label_new(_("Events to respond to:"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_js = lives_standard_check_button_new(_("_Joystick events"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_js), prefs->omc_dev_opts&OMC_DEV_JS);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->omc_js_entry = lives_standard_entry_new((tmp=lives_strdup(_("_Joystick device")))
                         ,TRUE,prefs->omc_js_fname,-1,PATH_MAX,LIVES_BOX(hbox),
                         (tmp2=lives_strdup(_("The joystick device, e.g. /dev/input/js0"))));
  lives_free(tmp);
  lives_free(tmp2);

  buttond = lives_standard_file_button_new(FALSE,LIVES_DEVICE_DIR);
  lives_box_pack_start(LIVES_BOX(hbox),buttond,FALSE,FALSE,widget_opts.packing_width);

  lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       (livespointer)prefsw->omc_js_entry);

#ifdef OMC_MIDI_IMPL
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));
#endif

#endif

#ifdef OMC_MIDI_IMPL
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_midi = lives_standard_check_button_new(_("_MIDI events"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_midi), prefs->omc_dev_opts&OMC_DEV_MIDI);

  // ---

  prefsw->midi_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), prefsw->midi_hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->alsa_midi = lives_standard_radio_button_new((tmp=lives_strdup(_("Use _ALSA MIDI (recommended)"))),TRUE,alsa_midi_group,
                      LIVES_BOX(prefsw->midi_hbox),
                      (tmp2=lives_strdup(_("Create an ALSA MIDI port which other MIDI devices can be connected to"))));

  lives_free(tmp);
  lives_free(tmp2);

  alsa_midi_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(prefsw->alsa_midi));

  // ---

  raw_midi_button = lives_standard_radio_button_new((tmp=lives_strdup(_("Use _raw MIDI"))),TRUE,alsa_midi_group,
                    LIVES_BOX(prefsw->midi_hbox),
                    (tmp2=lives_strdup(_("Read directly from the MIDI device"))));

  lives_free(tmp);
  lives_free(tmp2);

#ifdef ALSA_MIDI
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(raw_midi_button),!prefs->use_alsa_midi);
#endif


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->omc_midi_entry = lives_standard_entry_new((tmp=lives_strdup(_("_MIDI device"))),TRUE,prefs->omc_midi_fname,
                           -1,PATH_MAX,LIVES_BOX(hbox),
                           (tmp2=lives_strdup(_("The MIDI device, e.g. /dev/input/midi0"))));

  lives_free(tmp);
  lives_free(tmp2);

  prefsw->button_midid = lives_standard_file_button_new(FALSE,LIVES_DEVICE_DIR);
  lives_box_pack_start(LIVES_BOX(hbox),prefsw->button_midid,FALSE,FALSE,widget_opts.packing_width);
  lives_widget_show(prefsw->button_midid);

  lives_signal_connect(prefsw->button_midid, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       (livespointer)prefsw->omc_midi_entry);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));

  label = lives_standard_label_new(_("Advanced"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_midicr = lives_standard_spin_button_new((tmp=lives_strdup(_("MIDI check _rate"))),TRUE,
                              prefs->midi_check_rate, 1., 2000., 10., 100., 0,
                              LIVES_BOX(hbox),
                              (tmp2=lives_strdup(
                                      _("Number of MIDI checks per keyboard tick. Increasing this may improve MIDI responsiveness, but may slow down playback."))));

  lives_free(tmp);
  lives_free(tmp2);


  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->spinbutton_midirpt = lives_standard_spin_button_new((tmp=lives_strdup(_("MIDI repeat"))),FALSE,
                               prefs->midi_rpt, 1., 10000., 100., 1000., 0,
                               LIVES_BOX(hbox),
                               (tmp2=lives_strdup(_("Number of non-reads allowed between succesive reads."))));
  lives_free(tmp);
  lives_free(tmp2);

  label = lives_standard_label_new(_("(Warning: setting this value too high can slow down playback.)"));

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ALSA_MIDI
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_alsa_midi_toggled),
                       NULL);

  on_alsa_midi_toggled(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi),prefsw);
#endif

#endif
#endif

  icon = lives_build_filename(prefs->prefix_dir, ICON_DIR, "pref_midi.png", NULL);
  pixbuf_midi = lives_pixbuf_new_from_file(icon, NULL);
  lives_free(icon);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_midi, _("MIDI/Joystick learner"), LIST_ENTRY_MIDI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_midi);




  prefsw->selection = lives_tree_view_get_selection(LIVES_TREE_VIEW(prefsw->prefs_list));
  lives_tree_selection_set_mode(prefsw->selection, LIVES_SELECTION_SINGLE);

  lives_signal_connect(prefsw->selection, LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_prefDomainChanged), NULL);
  //


  // Preferences 'Revert' button
  prefsw->cancelbutton = lives_button_new_from_stock(LIVES_STOCK_REVERT_TO_SAVED,NULL);
  lives_widget_show(prefsw->cancelbutton);
  lives_dialog_add_action_widget(LIVES_DIALOG(prefsw->prefs_dialog), prefsw->cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_size_request(prefsw->cancelbutton, DEF_BUTTON_WIDTH*2, -1);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->cancelbutton), widget_opts.border_width);


  lives_widget_set_can_focus(prefsw->cancelbutton,TRUE);

  // Set 'Close' button as inactive since there are no changes yet
  lives_widget_set_sensitive(prefsw->cancelbutton, FALSE);

  // Preferences 'Apply' button
  prefsw->applybutton = lives_button_new_from_stock(LIVES_STOCK_APPLY,NULL);
  lives_widget_show(prefsw->applybutton);
  lives_dialog_add_action_widget(LIVES_DIALOG(prefsw->prefs_dialog), prefsw->applybutton, 0);
  lives_widget_set_size_request(prefsw->applybutton, DEF_BUTTON_WIDTH*2, -1);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->applybutton), widget_opts.border_width);

  lives_widget_set_can_focus_and_default(prefsw->applybutton);
  // Set 'Apply' button as inactive since there is no changes yet
  lives_widget_set_sensitive(prefsw->applybutton, FALSE);

  // Preferences 'Close' button
  prefsw->closebutton = lives_button_new_from_stock(LIVES_STOCK_CLOSE,NULL);
  lives_widget_show(prefsw->closebutton);
  lives_dialog_add_action_widget(LIVES_DIALOG(prefsw->prefs_dialog), prefsw->closebutton, LIVES_RESPONSE_OK);
  lives_widget_set_size_request(prefsw->closebutton, DEF_BUTTON_WIDTH*2, -1);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->closebutton), widget_opts.border_width);

  lives_widget_set_can_focus_and_default(prefsw->closebutton);

  lives_widget_add_accelerator(prefsw->closebutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


  // Connect signals for 'Apply' button activity handling
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fore), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_back), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mabf), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mab), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infot), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infob), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mtmark), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_evbox), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tlreg), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fsur), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcbg), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcfg), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_cesel), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_ceunsel), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_vidcol), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_audcol), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fxcol), LIVES_WIDGET_COLOR_SET_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  if (prefsw->theme_style2!=NULL)
    lives_signal_connect(LIVES_GUI_OBJECT(prefsw->theme_style2), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->theme_style3), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->theme_style4), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);


  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->wpp_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->frei0r_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->ladspa_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->fs_max_check), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->recent_check), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->stop_screensaver_check), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->open_maximised_check), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->show_tool), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mouse_scroll), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_ce_maxspect), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_button_icons), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rb_startup_ce), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rb_startup_mt), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_crit_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(spinbutton_crit_ds_value_changed), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_crit_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_gmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_pmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_gmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(pmoni_gmoni_changed),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_pmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(pmoni_gmoni_changed),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->forcesmon), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_stream_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_rec_after_pb), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_warn_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mt_enter_prompt), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt_enter_defs), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_render_prompt), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_width), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_height), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->backaudio_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->pertrack_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_undo_buf), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_mt_exit_render), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_ab_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_max_disp_vtracks), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_always), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_never), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_every), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->video_open_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->frameblank_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->sepimg_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_ocp), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->jpeg), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(png), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_instant_open), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_auto_deint), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_auto_trim), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_nobord), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_concat_images), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->pbq_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_show_stats), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->audp_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->audio_command_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_afollow), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_aclips), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rdesk_audio), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rframes), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rfps), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->reffects), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rclips), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->raudio), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->rextaudio), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_rec_gb), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->encoder_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->ofmt_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  if (prefsw->acodec_combo!=NULL)
    lives_signal_connect(LIVES_GUI_OBJECT(prefsw->acodec_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                         NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_antialias), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_rte_keys), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_threads), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_threads), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(toggle_set_sensitive),
                       (livespointer)prefsw->spinbutton_nfx_threads);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_nfx_threads), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->vid_load_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->vid_save_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->audio_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->image_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->proj_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->tmpdir_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_fps), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_fsize), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_warn_fsize), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_save_set), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mplayer), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_rendered_fx), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_encoders), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_dup_set), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_clips), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_close), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_delete), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_shift), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_alter), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_adel), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_ashift), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_aalt), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_popup), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_discard_layout), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mt_achans), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mt_no_jack), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#ifdef HAVE_LDVGRAB
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_after_dvgrab), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#endif
#ifdef HAVE_YUV4MPEG
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_yuv4m_open), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#endif
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mt_backup_space), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_after_crash), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_no_pulse), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_wipe), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->check_midi), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->ins_speed), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(ins_resample), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cdplay_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_def_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->theme_combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_bwidth), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#ifdef ENABLE_OSC
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_osc_udp), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC_start), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#endif

#ifdef ENABLE_JACK_TRANSPORT
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->jack_tserver_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_start_tjack), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_master), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#endif

#ifdef ENABLE_JACK
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->jack_aserver_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_start_ajack), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_pwp), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_read_autocon), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_omc_js), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->omc_js_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#endif
#ifdef OMC_MIDI_IMPL
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_omc_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(raw_midi_button), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->omc_midi_entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_midicr), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_midirpt), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(apply_button_set_enabled),
                       NULL);
#endif
#endif

  if (capable->has_encoder_plugins) {
    prefsw->encoder_name_fn = lives_signal_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->encoder_combo)), LIVES_WIDGET_CHANGED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_encoder_entry_changed), NULL);
    // ---
    prefsw->encoder_ofmt_fn = lives_signal_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->ofmt_combo)), LIVES_WIDGET_CHANGED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_encoder_ofmt_changed), NULL);
  }

  prefsw->audp_entry_func = lives_signal_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->audp_combo)), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_audp_entry_changed), NULL);

#ifdef ENABLE_OSC
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_osc_enable_toggled),
                       (livespointer)prefsw->enable_OSC_start);
#endif
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prefs_revert_clicked),
                       NULL);


  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->closebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prefs_close_clicked),
                       prefsw);

  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->applybutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prefs_apply_clicked),
                       NULL);


  lives_list_free_strings(audp);
  lives_list_free(audp);



  if (prefs_current_page==-1) {
    if (mainw->multitrack == NULL)
      select_pref_list_row(LIST_ENTRY_GUI);
    else
      select_pref_list_row(LIST_ENTRY_MULTITRACK);
  } else select_pref_list_row(prefs_current_page);

  on_prefDomainChanged(prefsw->selection,NULL);
  lives_widget_queue_draw(prefsw->prefs_list);

  return prefsw;
}


void on_preferences_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (menuitem!=NULL) prefs_current_page=-1;

  if (prefsw != NULL && prefsw->prefs_dialog != NULL) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
    return;
  }

  future_prefs->disabled_decoders=lives_list_copy_strings(prefs->disabled_decoders);

  prefsw = create_prefs_dialog();
  lives_widget_show(prefsw->prefs_dialog);

}

/*!
 * Closes preferences dialog window
 */
void on_prefs_close_clicked(LiVESButton *button, livespointer user_data) {
  if (prefs->acodec_list!=NULL) {
    lives_list_free_strings(prefs->acodec_list);
    lives_list_free(prefs->acodec_list);
  }
  prefs->acodec_list=NULL;
  lives_free(prefsw->audp_name);
  lives_free(prefsw->orig_audp_name);

  lives_free(resaudw);
  resaudw=NULL;

  if (future_prefs->disabled_decoders!=NULL) {
    lives_list_free_strings(future_prefs->disabled_decoders);
    lives_list_free(future_prefs->disabled_decoders);
  }

  lives_general_button_clicked(button, user_data);

  prefsw=NULL;

  if (mainw->prefs_need_restart) {
    do_blocking_info_dialog(
      _("\nLiVES will now shut down. You need to restart it for the directory change to take effect.\nClick OK to continue.\n"));
    on_quit_activate(NULL,NULL);
  }
}

/*!
 *
 */
void on_prefs_apply_clicked(LiVESButton *button, livespointer user_data) {
  boolean needs_restart;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,prefsw->prefs_dialog);
  
  // Apply preferences
  needs_restart = apply_prefs(FALSE);

  // do this now in case anything was changed or reverted
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->cancelbutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), TRUE);

  if (!mainw->prefs_need_restart) {
    mainw->prefs_need_restart = needs_restart;
  }

  if (needs_restart) {
    do_blocking_info_dialog(_("For the directory change to take effect LiVES will restart when preferences dialog closes."));
  }

  if (mainw->prefs_changed & PREFS_THEME_CHANGED) {
    lives_widget_set_sensitive(mainw->export_theme,FALSE);
    do_blocking_info_dialog(_("Disabling the theme will not take effect until the next time you start LiVES."));
  }
  else
    lives_widget_set_sensitive(mainw->export_theme,TRUE);

  
  if (mainw->prefs_changed & PREFS_JACK_CHANGED) {
    do_blocking_info_dialog(_("Jack options will not take effect until the next time you start LiVES."));
  }


  if (!(mainw->prefs_changed & PREFS_THEME_CHANGED)&&
      ((mainw->prefs_changed & PREFS_IMAGES_CHANGED)||
      (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED)||
      (mainw->prefs_changed & PREFS_COLOURS_CHANGED))) {
    // set details in prefs
    set_palette_prefs();
    if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
      load_theme_images();
    }
  }
  
  if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
    if (prefs->show_gui) {
      if (mainw->current_file==-1) {
        load_start_image(0);
        load_end_image(0);
        if (mainw->preview_box!=NULL) load_preview_image(FALSE);
      }
      lives_widget_queue_draw(mainw->LiVES);
      if (mainw->multitrack!=NULL) {
        lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->multitrack->sep_image),mainw->imsep);
        mt_show_current_frame(mainw->multitrack,FALSE);
        lives_widget_queue_draw(mainw->multitrack->window);
      }
    }
  }

  if (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED) {
    // minor colours changed
    if (mainw->multitrack!=NULL) {
      resize_timeline(mainw->multitrack);
      set_mt_colours(mainw->multitrack);
    }
    else {
      lives_widget_queue_draw(mainw->LiVES);
    }
  }

  if (mainw->prefs_changed & PREFS_COLOURS_CHANGED) {
    // major coulours changed
    // force reshow of window
    set_colours(&palette->normal_fore,&palette->normal_back,&palette->menu_and_bars_fore,&palette->menu_and_bars, \
                &palette->info_base,&palette->info_text);

    if (mainw->preview_box!=NULL) {
      set_preview_box_colours();
    }

    if (prefs->show_gui) {
      if (mainw->multitrack!=NULL) {
        set_mt_colours(mainw->multitrack);
        scroll_tracks(mainw->multitrack,mainw->multitrack->top_track,FALSE);
        track_select(mainw->multitrack);
        mt_clip_select(mainw->multitrack,FALSE);
      }
    }

    on_prefs_revert_clicked(button,NULL);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  }
  else lives_set_cursor_style(LIVES_CURSOR_NORMAL,prefsw->prefs_dialog);

  
  mainw->prefs_changed = 0;


}

/*
 * Function is used to select particular row in preferences selection list
 * selection is performed according to provided index which is one of LIST_ENTRY_* constants
 */
static void select_pref_list_row(uint32_t selected_idx) {
  LiVESTreeIter iter;
  LiVESTreeModel *model;
  boolean valid;
  uint32_t idx;

  model = lives_tree_view_get_model(LIVES_TREE_VIEW(prefsw->prefs_list));
  valid = lives_tree_model_get_iter_first(model, &iter);
  while (valid) {
    lives_tree_model_get(model, &iter, LIST_NUM, &idx, -1);
    //
    if (idx == selected_idx) {
      lives_tree_selection_select_iter(prefsw->selection, &iter);
      break;
    }
    //
    valid = lives_tree_model_iter_next(model, &iter);
  }
}


void on_prefs_revert_clicked(LiVESButton *button, livespointer user_data) {
  register int i;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  if (future_prefs->vpp_argv != NULL) {
    for (i = 0; future_prefs->vpp_argv[i] != NULL; lives_free(future_prefs->vpp_argv[i++]));

    lives_free(future_prefs->vpp_argv);

    future_prefs->vpp_argv = NULL;
  }
  memset(future_prefs->vpp_name, 0, 64);

  if (prefs->acodec_list != NULL) {
    lives_list_free_strings(prefs->acodec_list);
    lives_list_free(prefs->acodec_list);
  }
  prefs->acodec_list = NULL;

  if (prefsw->pbq_list != NULL) {
    lives_list_free(prefsw->pbq_list);
  }
  prefsw->pbq_list = NULL;

  lives_free(prefsw->audp_name);
  lives_free(prefsw->orig_audp_name);

  if (future_prefs->disabled_decoders != NULL) {
    lives_list_free_strings(future_prefs->disabled_decoders);
    lives_list_free(future_prefs->disabled_decoders);
  }

  lives_general_button_clicked(button, prefsw);

  prefsw = NULL;

  on_preferences_activate(NULL, NULL);
  
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
}


boolean lives_ask_permission(int what) {
  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    return ask_permission_dialog(what);
  default:
    LIVES_WARN("Unknown permission requested");
  }
  return FALSE;
}

