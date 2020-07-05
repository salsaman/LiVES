// preferences.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details
// functions dealing with getting/setting user preferences
// TODO - use atom type system for prefs

#include <dlfcn.h>

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "resample.h"
#include "plugins.h"
#include "rte_window.h"
#include "interface.h"
#include "startup.h"
#include "effects-weed.h"

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

static LiVESWidget *saved_closebutton;
static LiVESWidget *saved_applybutton;
static LiVESWidget *saved_revertbutton;
static  boolean mt_needs_idlefunc;

static int nmons;

static uint32_t prefs_current_page;

static void select_pref_list_row(uint32_t selected_idx, _prefsw *prefsw);

#define ACTIVE(widget, signal) lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->widget), LIVES_WIDGET_ ##signal## \
						    _SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL)


/** @brief callback to set to make a togglebutton or check_menu_item directly control a boolean pref

  widget is either a togge_button (sets temporary) or a check_menuitem (sets permanent)
  pref must have a corresponding entry in pref_factory_bool() */
void toggle_sets_pref(LiVESWidget *widget, livespointer prefidx) {
  if (LIVES_IS_TOGGLE_BUTTON(widget))
    pref_factory_bool((const char *)prefidx,
                      lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget)), FALSE);
  else if (LIVES_IS_CHECK_MENU_ITEM(widget))
    pref_factory_bool((const char *)prefidx,
                      lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(widget)), TRUE);
}


#ifdef ENABLE_OSC
static void on_osc_enable_toggled(LiVESToggleButton *t1, livespointer t2) {
  if (prefs->osc_udp_started) return;
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, lives_toggle_button_get_active(t1) ||
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(t2)));
}
#endif


static LiVESResponseType get_pref_inner(const char *filename, const char *key, char *val, int maxlen) {
  char *com;

  memset(val, 0, maxlen);
  if (filename == NULL) {
    if (mainw->cached_list != NULL) {
      char *prefval = get_val_from_cached_list(key, maxlen);
      if (prefval != NULL) {
        lives_snprintf(val, maxlen, "%s", prefval);
        lives_free(prefval);
        return LIVES_RESPONSE_YES;
      }
      return LIVES_RESPONSE_NO;
    }
    com = lives_strdup_printf("%s get_pref \"%s\" -", prefs->backend_sync, key);
  } else {
    com = lives_strdup_printf("%s get_clip_value \"%s\" - - \"%s\"", prefs->backend_sync, key,
                              filename);
  }

  lives_popen(com, TRUE, val, maxlen);

  lives_free(com);
  return LIVES_RESPONSE_NONE;
}


LIVES_GLOBAL_INLINE LiVESResponseType get_string_pref(const char *key, char *val, int maxlen) {
  return get_pref_inner(NULL, key, val, maxlen);
}


LIVES_GLOBAL_INLINE LiVESResponseType get_string_prefd(const char *key, char *val, int maxlen, const char *def) {
  int ret = get_pref_inner(NULL, key, val, maxlen);
  if (ret == LIVES_RESPONSE_NO) lives_snprintf(val, maxlen, "%s", def);
  return ret;
}


LIVES_GLOBAL_INLINE LiVESResponseType get_pref_from_file(const char *filename, const char *key, char *val, int maxlen) {
  return get_pref_inner(filename, key, val, maxlen);
}


LiVESResponseType get_utf8_pref(const char *key, char *val, int maxlen) {
  // get a pref in locale encoding, then convert it to utf8
  char *tmp;
  int retval = get_string_pref(key, val, maxlen);
  tmp = lives_filename_to_utf8(val, -1, NULL, NULL, NULL);
  lives_snprintf(val, maxlen, "%s", tmp);
  lives_free(tmp);
  return retval;
}


LiVESList *get_list_pref(const char *key) {
  // get a list of values from a preference
  char **array;
  char buf[65536];
  int nvals, i;

  LiVESList *retlist = NULL;

  if (get_string_pref(key, buf, 65535) == LIVES_RESPONSE_NO) return NULL;
  if (!(*buf)) return NULL;

  nvals = get_token_count(buf, '\n');
  array = lives_strsplit(buf, "\n", nvals);
  for (i = 0; i < nvals; i++) {
    retlist = lives_list_append(retlist, lives_strdup(array[i]));
  }

  lives_strfreev(array);

  return retlist;
}


LIVES_GLOBAL_INLINE boolean get_boolean_pref(const char *key) {
  char buffer[16];
  get_string_pref(key, buffer, 16);
  if (!strcmp(buffer, "true")) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean get_boolean_prefd(const char *key, boolean defval) {
  char buffer[16];
  get_string_pref(key, buffer, 16);
  if (!(*buffer)) return defval;
  if (!strcmp(buffer, "true")) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE int get_int_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return 0;
  return atoi(buffer);
}


LIVES_GLOBAL_INLINE int get_int_prefd(const char *key, int defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return atoi(buffer);
}


LIVES_GLOBAL_INLINE int64_t get_int64_prefd(const char *key, int64_t defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return atol(buffer);
}


LIVES_GLOBAL_INLINE double get_double_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return 0.;
  return strtod(buffer, NULL);
}


LIVES_GLOBAL_INLINE double get_double_prefd(const char *key, double defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return strtod(buffer, NULL);
}


LIVES_GLOBAL_INLINE boolean has_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return FALSE;
  return TRUE;
}


boolean get_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  char buffer[64];
  char **array;
  int ntoks;

  if (get_string_pref(key, buffer, 64) == LIVES_RESPONSE_NO) return FALSE;
  if (!(*buffer)) return FALSE;
  if ((ntoks = get_token_count(buffer, ' ')) < 3) return FALSE;

  array = lives_strsplit(buffer, " ", 4);
  lcol->red = atoi(array[0]);
  lcol->green = atoi(array[1]);
  lcol->blue = atoi(array[2]);
  if (ntoks == 4) lcol->alpha = atoi(array[3]);
  else lcol->alpha = 65535;
  lives_strfreev(array);

  return TRUE;
}


boolean get_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol) {
  char buffer[64];
  char **array;
  int ntoks;

  if (get_pref_from_file(themefile, key, buffer, 64) == LIVES_RESPONSE_NO) return FALSE;
  if (!(*buffer)) return FALSE;
  if ((ntoks = get_token_count(buffer, ' ')) < 3) return FALSE;

  array = lives_strsplit(buffer, " ", 4);
  lcol->red = atoi(array[0]);
  lcol->green = atoi(array[1]);
  lcol->blue = atoi(array[2]);
  if (ntoks == 4) lcol->alpha = atoi(array[3]);
  else lcol->alpha = 65535;
  lives_strfreev(array);

  return TRUE;
}


static int run_prefs_command(const char *com) {
  int ret = 0;
  do {
    ret = lives_system(com, TRUE) >> 8;
    if (mainw != NULL && mainw->is_ready) {
      if (ret == 4) {
        // error writing to temp config file
        char *newrcfile = ensure_extension(capable->rcfile, LIVES_FILE_EXT_NEW);
        ret = do_write_failed_error_s_with_retry(newrcfile, NULL, NULL);
        lives_free(newrcfile);
      } else if (ret == 3) {
        // error writing to config file
        ret = do_write_failed_error_s_with_retry(capable->rcfile, NULL, NULL);
      } else if (ret != 0) {
        // error reading from config file
        ret = do_read_failed_error_s_with_retry(capable->rcfile, NULL, NULL);
      }
    } else ret = 0;
  } while (ret == LIVES_RESPONSE_RETRY);
  return ret;
}


int delete_pref(const char *key) {
  char *com = lives_strdup_printf("%s delete_pref \"%s\"", prefs->backend_sync, key);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_string_pref(const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_string_pref_priority(const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_pref_priority \"%s\" \"%s\"", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_utf8_pref(const char *key, const char *value) {
  // convert to locale encoding
  char *tmp = U82F(value);
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, tmp);
  int ret = run_prefs_command(com);
  lives_free(com);
  lives_free(tmp);
  return ret;
}


void set_theme_pref(const char *themefile, const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, themefile, key, value);
  int ret = 0;
  do {
    if (lives_system(com, TRUE)) {
      ret = do_write_failed_error_s_with_retry(themefile, NULL, NULL);
    }
  } while (ret == LIVES_RESPONSE_RETRY);
  lives_free(com);
}


int set_int_pref(const char *key, int value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" %d", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_int64_pref(const char *key, int64_t value) {
  // not used
  char *com = lives_strdup_printf("%s set_pref \"%s\" %"PRId64, prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_double_pref(const char *key, double value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" %.3f", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_boolean_pref(const char *key, boolean value) {
  char *com;
  int ret;
  if (value) {
    com = lives_strdup_printf("%s set_pref \"%s\" true", prefs->backend_sync, key);
  } else {
    com = lives_strdup_printf("%s set_pref \"%s\" false", prefs->backend_sync, key);
  }
  ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_list_pref(const char *key, LiVESList *values) {
  // set pref from a list of values
  LiVESList *xlist = values;
  char *string = NULL, *tmp;
  int ret;

  while (xlist != NULL) {
    if (string == NULL) string = lives_strdup((char *)xlist->data);
    else {
      tmp = lives_strdup_printf("%s\n%s", string, (char *)xlist->data);
      lives_free(string);
      string = tmp;
    }
    xlist = xlist->next;
  }

  if (string == NULL) string = lives_strdup("");

  ret = set_string_pref(key, string);

  lives_free(string);
  return ret;
}


void set_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol) {
  char *myval = lives_strdup_printf("%d %d %d", lcol->red, lcol->green, lcol->blue);
  char *com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, themefile, key, myval);
  lives_system(com, FALSE);
  lives_free(com);
  lives_free(myval);
}


int set_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  char *myval = lives_strdup_printf("%d %d %d %d", lcol->red, lcol->green, lcol->blue, lcol->alpha);
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, myval);
  int ret = run_prefs_command(com);
  lives_free(com);
  lives_free(myval);
  return ret;
}


void set_palette_prefs(void) {
  lives_colRGBA64_t lcol;

  lcol.red = palette->style;
  lcol.green = lcol.blue = lcol.alpha = 0;

  if (set_colour_pref(THEME_DETAIL_STYLE, &lcol)) return;

  if (set_string_pref(THEME_DETAIL_SEPWIN_IMAGE, mainw->sepimg_path)) return;
  if (set_string_pref(THEME_DETAIL_FRAMEBLANK_IMAGE, mainw->frameblank_path)) return;

  widget_color_to_lives_rgba(&lcol, &palette->normal_fore);
  if (set_colour_pref(THEME_DETAIL_NORMAL_FORE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->normal_back);
  if (set_colour_pref(THEME_DETAIL_NORMAL_BACK, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars_fore);
  if (set_colour_pref(THEME_DETAIL_ALT_FORE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars);
  if (set_colour_pref(THEME_DETAIL_ALT_BACK, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->info_text);
  if (set_colour_pref(THEME_DETAIL_INFO_TEXT, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->info_base);
  if (set_colour_pref(THEME_DETAIL_INFO_BASE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_fg);
  if (set_colour_pref(THEME_DETAIL_MT_TCFG, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_bg);
  if (set_colour_pref(THEME_DETAIL_MT_TCBG, &lcol)) return;

  if (set_colour_pref(THEME_DETAIL_AUDCOL, &palette->audcol)) return;
  if (set_colour_pref(THEME_DETAIL_VIDCOL, &palette->vidcol)) return;
  if (set_colour_pref(THEME_DETAIL_FXCOL, &palette->fxcol)) return;

  if (set_colour_pref(THEME_DETAIL_MT_TLREG, &palette->mt_timeline_reg)) return;
  if (set_colour_pref(THEME_DETAIL_MT_MARK, &palette->mt_mark)) return;
  if (set_colour_pref(THEME_DETAIL_MT_EVBOX, &palette->mt_evbox)) return;

  if (set_colour_pref(THEME_DETAIL_FRAME_SURROUND, &palette->frame_surround)) return;

  if (set_colour_pref(THEME_DETAIL_CE_SEL, &palette->ce_sel)) return;
  if (set_colour_pref(THEME_DETAIL_CE_UNSEL, &palette->ce_unsel)) return;

  if (set_string_pref(THEME_DETAIL_SEPWIN_IMAGE, mainw->sepimg_path)) return;
  if (set_string_pref(THEME_DETAIL_FRAMEBLANK_IMAGE, mainw->frameblank_path)) return;
}


void set_vpp(boolean set_in_prefs) {
  // Video Playback Plugin

  if (*future_prefs->vpp_name) {
    if (!lives_utf8_strcasecmp(future_prefs->vpp_name, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      if (mainw->vpp != NULL) {
        if (mainw->ext_playback) vid_playback_plugin_exit();
        close_vid_playback_plugin(mainw->vpp);
        mainw->vpp = NULL;
        if (set_in_prefs) set_string_pref(PREF_VID_PLAYBACK_PLUGIN, "none");
      }
    } else {
      _vid_playback_plugin *vpp;
      if ((vpp = open_vid_playback_plugin(future_prefs->vpp_name, TRUE)) != NULL) {
        mainw->vpp = vpp;
        if (set_in_prefs) {
          set_string_pref(PREF_VID_PLAYBACK_PLUGIN, mainw->vpp->name);
          if (!mainw->ext_playback)
            do_error_dialog_with_check_transient
            (_("\n\nVideo playback plugins are only activated in\nfull screen, separate window (fs) mode\n"),
             TRUE, 0, prefsw != NULL ? LIVES_WINDOW(prefsw->prefs_dialog) : LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
        }
      }
    }
    if (set_in_prefs) mainw->write_vpp_file = TRUE;
  }

  if (future_prefs->vpp_argv != NULL && mainw->vpp != NULL) {
    mainw->vpp->fwidth = future_prefs->vpp_fwidth;
    mainw->vpp->fheight = future_prefs->vpp_fheight;
    mainw->vpp->palette = future_prefs->vpp_palette;
    mainw->vpp->fixed_fpsd = future_prefs->vpp_fixed_fpsd;
    mainw->vpp->fixed_fps_numer = future_prefs->vpp_fixed_fps_numer;
    mainw->vpp->fixed_fps_denom = future_prefs->vpp_fixed_fps_denom;
    if (mainw->vpp->fixed_fpsd > 0.) {
      if (mainw->fixed_fpsd != -1. || !((*mainw->vpp->set_fps)(mainw->vpp->fixed_fpsd))) {
        do_vpp_fps_error();
        mainw->vpp->fixed_fpsd = -1.;
        mainw->vpp->fixed_fps_numer = 0;
      }
    }
    if (!(*mainw->vpp->set_palette)(mainw->vpp->palette)) {
      do_vpp_palette_error();
    }
    mainw->vpp->YUV_clamping = future_prefs->vpp_YUV_clamping;

    if (mainw->vpp->set_yuv_palette_clamping != NULL)(*mainw->vpp->set_yuv_palette_clamping)(mainw->vpp->YUV_clamping);

    mainw->vpp->extra_argc = future_prefs->vpp_argc;
    mainw->vpp->extra_argv = future_prefs->vpp_argv;
    if (set_in_prefs) mainw->write_vpp_file = TRUE;
  }

  memset(future_prefs->vpp_name, 0, 64);
  future_prefs->vpp_argv = NULL;
}


static void set_workdir_label_text(LiVESLabel *label, const char *dir) {
  char *free_ds;
  char *tmpx1, *tmpx2;
  char *markup;

  // use lives_strdup* since the translation string is auto-freed()

  if (!is_writeable_dir(dir)) {
    tmpx2 = (_("\n\n\n(Free space = UNKNOWN)"));
  } else {
    free_ds = lives_format_storage_space_string(get_ds_free(dir));
    tmpx2 = lives_strdup_printf(_("\n\n\n(Free space = %s)"), free_ds);
    lives_free(free_ds);
  }

  tmpx1 = lives_strdup(
            _("The work directory is LiVES working directory where opened clips and sets are stored.\n"
              "It should be in a partition with plenty of free disk space.\n"));

#ifdef GUI_GTK
  markup = g_markup_printf_escaped("<span background=\"white\" foreground=\"red\"><big><b>%s</b></big></span>%s", tmpx1, tmpx2);
#endif
#ifdef GUI_QT
  QString qs = QString("<span background=\"white\" foreground=\"red\"><b>%s</b></span>%s").arg(tmpx1).arg(tmpx2);
  markup = strdup((const char *)qs.toHtmlEscaped().constData());
#endif

  lives_label_set_markup(LIVES_LABEL(label), markup);
  lives_free(markup);
  lives_free(tmpx1);
  lives_free(tmpx2);
}


boolean pref_factory_string(const char *prefidx, const char *newval, boolean permanent) {
  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_AUDIO_PLAYER)) {
    const char *audio_player = newval;

    if (!(lives_strcmp(audio_player, AUDIO_PLAYER_NONE)) && prefs->audio_player != AUD_PLAYER_NONE) {
      // switch to none
      switch_aud_to_none(permanent);
#if 0
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) mainw->nullaudio_loop = AUDIO_LOOP_PINGPONG;
      else mainw->nullaudio_loop = AUDIO_LOOP_FORWARD;
#endif
      update_all_host_info(); // let fx plugins know about the change
      goto success1;
    } else if (!(lives_strcmp(audio_player, AUDIO_PLAYER_SOX)) && prefs->audio_player != AUD_PLAYER_SOX) {
      // switch to sox
      if (switch_aud_to_sox(permanent)) goto success1;
      // revert text
      if (prefsw != NULL) {
        lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
      }
      goto fail1;
    }

#ifdef ENABLE_JACK
    if (!(lives_strcmp(audio_player, AUDIO_PLAYER_JACK)) && prefs->audio_player != AUD_PLAYER_JACK) {
      // switch to jack
      if (!capable->has_jackd) {
        do_blocking_error_dialogf(_("\nUnable to switch audio players to %s\n%s must be installed first.\nSee %s\n"),
                                  AUDIO_PLAYER_JACK,
                                  EXEC_JACKD,
                                  JACK_URL);
        goto fail1;
      } else {
        if (prefs->audio_player == AUD_PLAYER_JACK && lives_strcmp(audio_player, AUDIO_PLAYER_JACK)) {
          do_blocking_error_dialogf(_("\nSwitching audio players requires restart (%s must not be running)\n"), EXEC_JACKD);
          // revert text
          if (prefsw != NULL) {
            lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
            lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
          }
          goto fail1;
        }
      }
      if (!switch_aud_to_jack(permanent)) {
        // failed
        do_jack_noopen_warn();
        // revert text
        if (prefsw != NULL) {
          lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
        }
        goto fail1;
      } else {
        // success
        if (mainw->loop_cont) {
          if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
          else mainw->jackd->loop = AUDIO_LOOP_FORWARD;
        }
        update_all_host_info(); // let fx plugins know about the change
        goto success1;
      }
      goto fail1;
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if ((!lives_strcmp(audio_player, AUDIO_PLAYER_PULSE) || !lives_strcmp(audio_player, AUDIO_PLAYER_PULSE_AUDIO)) &&
        prefs->audio_player != AUD_PLAYER_PULSE) {
      // switch to pulseaudio
      if (!capable->has_pulse_audio) {
        do_blocking_error_dialogf(_("\nUnable to switch audio players to %s\n%s must be installed first.\nSee %s\n"),
                                  AUDIO_PLAYER_PULSE_AUDIO,
                                  AUDIO_PLAYER_PULSE_AUDIO,
                                  PULSE_AUDIO_URL);
        // revert text
        if (prefsw != NULL) {
          lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
        }
        goto fail1;
      } else {
        if (!switch_aud_to_pulse(permanent)) {
          // revert text
          if (prefsw != NULL) {
            lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
            lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
          }
          goto fail1;
        } else {
          // success
          if (mainw->loop_cont) {
            if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
            else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;
          }
          update_all_host_info(); // let fx plugins know about the change
          goto success1;
        }
      }
    }
#endif
    goto fail1;
  }

#ifdef HAVE_PULSE_AUDIO
  if (!lives_strcmp(prefidx, PREF_PASTARTOPTS)) {
    if (lives_strcmp(newval, prefs->pa_start_opts)) {
      lives_snprintf(prefs->pa_start_opts, 255, "%s", newval);
      if (permanent) set_string_pref(PREF_PASTARTOPTS, newval);
      goto success1;
    }
    goto fail1;
  }
#endif

  if (!lives_strcmp(prefidx, PREF_OMC_JS_FNAME)) {
    if (lives_strcmp(newval, prefs->omc_js_fname)) {
      lives_snprintf(prefs->omc_js_fname, PATH_MAX, "%s", newval);
      if (permanent) set_utf8_pref(PREF_OMC_JS_FNAME, newval);
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_OMC_MIDI_FNAME)) {
    if (lives_strcmp(newval, prefs->omc_midi_fname)) {
      lives_snprintf(prefs->omc_midi_fname, PATH_MAX, "%s", newval);
      if (permanent) set_utf8_pref(PREF_OMC_MIDI_FNAME, newval);
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_MIDI_RCV_CHANNEL)) {
    if (strlen(newval) > 2) {
      if (prefs->midi_rcv_channel != -1) {
        prefs->midi_rcv_channel = -1;
        if (permanent) set_int_pref(PREF_MIDI_RCV_CHANNEL, prefs->midi_rcv_channel);
        goto success1;
      }
    } else if (prefs->midi_rcv_channel != atoi(newval)) {
      prefs->midi_rcv_channel = atoi(newval);
      if (permanent) set_int_pref(PREF_MIDI_RCV_CHANNEL, prefs->midi_rcv_channel);
      goto success1;
    }
    goto fail1;
  }

fail1:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success1:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  return TRUE;
}


boolean pref_factory_bool(const char *prefidx, boolean newval, boolean permanent) {
  // this is called from lbindings.c which in turn is called from liblives.cpp

  // can also be called from other places

  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_SHOW_BUTTON_ICONS)) {
    if (prefs->show_button_images == newval) goto fail2;
    prefs->show_button_images = widget_opts.show_button_images = newval;
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    if (prefsw != NULL) lives_widget_queue_draw(prefsw->prefs_dialog);
    goto success2;
  }

  // show recent
  if (!lives_strcmp(prefidx, PREF_SHOW_RECENT_FILES)) {
    if (prefs->show_recent == newval) goto fail2;
    prefs->show_recent = newval;
    if (newval) {
      lives_widget_show(mainw->recent_menu);
      if (mainw->multitrack) lives_widget_show(mainw->multitrack->recent_menu);
    } else {
      lives_widget_hide(mainw->recent_menu);
      if (mainw->multitrack != NULL) lives_widget_hide(mainw->multitrack->recent_menu);
    }
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check), newval);
  }

  if (!lives_strcmp(prefidx, PREF_MSG_PBDIS)) {
    if (prefs->msgs_pbdis == newval) goto fail2;
    prefs->msgs_pbdis = newval;
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_pbdis), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SRGB_GAMMA)) {
    if (prefs->gamma_srgb == newval) goto fail2;
    prefs->gamma_srgb = newval;
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_srgb), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_LETTERBOX)) {
    if (prefs->letterbox == newval) goto fail2;
    prefs->letterbox = newval;
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lb), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_LETTERBOXMT)) {
    if (prefs->letterbox_mt == newval) goto fail2;
    prefs->letterbox_mt = newval;
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbmt), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_PBQ_ADAPTIVE)) {
    if (prefs->pbq_adaptive == newval) goto fail2;
    prefs->pbq_adaptive = newval;
    if (prefsw != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pbq_adaptive), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_VJMODE)) {
    if (future_prefs->vj_mode == newval) goto fail2;
    if (mainw != NULL && mainw->vj_mode  != NULL)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->vj_mode), newval);
    future_prefs->vj_mode = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_DEVOPTS)) {
    if (prefs->show_dev_opts == newval) goto fail2;
    if (mainw != NULL && mainw->show_devopts !=  NULL)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->show_devopts), newval);
    prefs->show_dev_opts = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_REC_EXT_AUDIO)) {
    boolean success = FALSE;
    boolean rec_ext_audio = newval;
    if (rec_ext_audio && prefs->audio_src == AUDIO_SRC_INT) {
      prefs->audio_src = AUDIO_SRC_EXT;

      if (permanent) {
        set_int_pref(PREF_AUDIO_SRC, AUDIO_SRC_EXT);
        future_prefs->audio_src = prefs->audio_src;
      }

      if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
        if (prefs->perm_audio_reader) {
          // create reader connection now, if permanent
          jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);
        }
#endif
      }
      if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
        if (prefs->perm_audio_reader) {
          // create reader connection now, if permanent
          pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);
        }
#endif
      }
      success = TRUE;
    } else if (!rec_ext_audio && prefs->audio_src == AUDIO_SRC_EXT) {
      prefs->audio_src = AUDIO_SRC_INT;

      if (permanent) {
        set_int_pref(PREF_AUDIO_SRC, AUDIO_SRC_INT);
        future_prefs->audio_src = prefs->audio_src;
      }

      mainw->aud_rec_fd = -1;
      if (prefs->perm_audio_reader) {
#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          jack_rec_audio_end(TRUE, TRUE);
        }
#endif
#ifdef HAVE_PULSE_AUDIO
        if (prefs->audio_player == AUD_PLAYER_PULSE) {
          pulse_rec_audio_end(TRUE, TRUE);
        }
#endif
      }
      success = TRUE;
    }
    if (success) {
      if (prefsw != NULL && permanent) {
        if (prefs->audio_src == AUDIO_SRC_EXT)
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), TRUE);
        else
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rintaudio), TRUE);
      }
      lives_signal_handler_block(mainw->ext_audio_checkbutton, mainw->ext_audio_func);
      lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_checkbutton),
                                          prefs->audio_src == AUDIO_SRC_EXT);
      lives_signal_handler_unblock(mainw->ext_audio_checkbutton, mainw->ext_audio_func);

      lives_signal_handler_block(mainw->int_audio_checkbutton, mainw->int_audio_func);
      lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->int_audio_checkbutton),
                                          prefs->audio_src == AUDIO_SRC_INT);
      lives_signal_handler_unblock(mainw->int_audio_checkbutton, mainw->int_audio_func);
      goto success2;
    }
    goto fail2;
  }

  if (!lives_strcmp(prefidx, PREF_MT_EXIT_RENDER)) {
    if (prefs->mt_exit_render == newval) goto fail2;
    prefs->mt_exit_render = newval;
    if (prefsw != NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_PUSH_AUDIO_TO_GENS)) {
    if (prefs->push_audio_to_gens == newval) goto fail2;
    prefs->push_audio_to_gens = newval;
    if (prefsw != NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pa_gens), prefs->push_audio_to_gens);
    goto success2;
  }

#ifdef HAVE_PULSE_AUDIO
  if (!lives_strcmp(prefidx, PREF_PARESTART)) {
    if (prefs->pa_restart == newval) goto fail2;
    prefs->pa_restart = newval;
    if (prefsw != NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart), prefs->pa_restart);
    goto success2;
  }
#endif

  if (!lives_strcmp(prefidx, PREF_SHOW_ASRC)) {
    if (prefs->show_asrc == newval) goto fail2;
    prefs->show_asrc = newval;
    if (prefsw != NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_asrc), prefs->show_asrc);
    if (!newval) {
      lives_widget_hide(mainw->int_audio_checkbutton);
      lives_widget_hide(mainw->ext_audio_checkbutton);
      lives_widget_hide(mainw->l1_tb);
      lives_widget_hide(mainw->l2_tb);
      lives_widget_hide(mainw->l3_tb);
    } else {
      lives_widget_show(mainw->int_audio_checkbutton);
      lives_widget_show(mainw->ext_audio_checkbutton);
      lives_widget_show(mainw->l1_tb);
      lives_widget_show(mainw->l2_tb);
      lives_widget_show(mainw->l3_tb);
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_TOOLTIPS)) {
    if (prefs->show_tooltips == newval) goto fail2;
    else {
      if (newval) prefs->show_tooltips = newval;
      if (prefsw != NULL) {
        if (prefsw->checkbutton_show_ttips != NULL)
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_ttips), newval);
        set_tooltips_state(prefsw->prefs_dialog, newval);
      }
      set_tooltips_state(mainw->top_vbox, newval);
      if (mainw->multitrack != NULL) set_tooltips_state(mainw->multitrack->top_vbox, newval);
      if (fx_dialog[0] != NULL && LIVES_IS_WIDGET(fx_dialog[0]->dialog)) set_tooltips_state(fx_dialog[0]->dialog, newval);
      if (fx_dialog[1] != NULL && LIVES_IS_WIDGET(fx_dialog[1]->dialog)) set_tooltips_state(fx_dialog[1]->dialog, newval);
      if (rte_window != NULL) set_tooltips_state(rte_window, newval);
    }
    // turn off after, or we cannot nullify the ttips
    if (!newval) prefs->show_tooltips = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_HFBWNP)) {
    if (prefs->hfbwnp == newval) goto fail2;
    prefs->hfbwnp = newval;
    if (prefsw != NULL)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_hfbwnp), prefs->hfbwnp);
    if (newval) {
      if (!LIVES_IS_PLAYING) {
        lives_widget_hide(mainw->framebar);
      }
    } else {
      if (!LIVES_IS_PLAYING || (LIVES_IS_PLAYING && !prefs->hide_framebar &&
                                (!mainw->fs || (mainw->ext_playback && mainw->vpp != NULL &&
                                    !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
                                    !(mainw->vpp->capabilities & VPP_CAN_RESIZE))))) {
        lives_widget_show(mainw->framebar);
      }
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_AR_CLIPSET)) {
    if (prefs->ar_clipset == newval) goto fail2;
    prefs->ar_clipset = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_AR_LAYOUT)) {
    if (prefs->ar_layout == newval) goto fail2;
    prefs->ar_layout = newval;
    goto success2;
  }

fail2:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success2:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_boolean_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_color_button(lives_colRGBA64_t *pcol, LiVESColorButton *cbutton) {
  LiVESWidgetColor col;
  lives_colRGBA64_t lcol;

  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_rgba_equal(widget_color_to_lives_rgba(&lcol, lives_color_button_get_color(cbutton, &col)), pcol)) {
    lives_rgba_copy(pcol, &lcol);
    if (prefsw != NULL) {
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
      prefsw->ignore_apply = FALSE;
    }
    return TRUE;
  }

  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;
}


boolean pref_factory_int(const char *prefidx, int newval, boolean permanent) {
  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_MT_AUTO_BACK)) {
    if (newval == prefs->mt_auto_back) goto fail3;
    if (mainw->multitrack != NULL) {
      if (newval <= 0 && prefs->mt_auto_back > 0) {
        if (mainw->multitrack->idlefunc > 0) {
          lives_source_remove(mainw->multitrack->idlefunc);
          mainw->multitrack->idlefunc = 0;
        }
        if (newval == 0) {
          prefs->mt_auto_back = newval;
          mt_auto_backup(mainw->multitrack);
        }
      }
      if (newval > 0 && prefs->mt_auto_back <= 0 && mainw->multitrack->idlefunc > 0) {
        mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
      }
    }
    prefs->mt_auto_back = newval;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_MAX_MSGS)) {
    if (newval == prefs->max_messages) goto fail3;
    if (newval < mainw->n_messages && newval >= 0) {
      free_n_msgs(mainw->n_messages - newval);
      if (prefs->show_msg_area)
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
    }
    prefs->max_messages = newval;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_SEPWIN_TYPE)) {
    if (prefs->sepwin_type == newval) goto fail3;
    prefs->sepwin_type = newval;
    if (newval == SEPWIN_TYPE_STICKY) {
      if (mainw->sep_win) {
        if (!LIVES_IS_PLAYING) {
          make_play_window();
        }
      } else {
        if (mainw->play_window != NULL) {
          play_window_set_title();
        }
      }
    } else {
      if (mainw->sep_win) {
        if (!LIVES_IS_PLAYING) {
          kill_play_window();
        } else {
          play_window_set_title();
        }
      }
    }

    if (permanent) future_prefs->sepwin_type = prefs->sepwin_type;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_MIDI_CHECK_RATE)) {
    if (newval != prefs->midi_check_rate) {
      prefs->midi_check_rate = newval;
      goto success3;
    }
    goto fail3;
  }

  if (!lives_strcmp(prefidx, PREF_MIDI_RPT)) {
    if (newval != prefs->midi_rpt) {
      prefs->midi_rpt = newval;
      goto success3;
    }
    goto fail3;
  }


fail3:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success3:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_int_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_string_choice(const char *prefidx, LiVESList *list, const char *strval, boolean permanent) {
  int idx = lives_list_strcmp_index(list, (livesconstpointer)strval, TRUE);
  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_MSG_TEXTSIZE)) {
    if (idx == prefs->msg_textsize) goto fail4;
    prefs->msg_textsize = idx;
    if (permanent) future_prefs->msg_textsize = prefs->msg_textsize;
    if (prefs->show_msg_area) {
      if (mainw->multitrack)
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->multitrack->msg_adj), mainw->multitrack->msg_area);
      else
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
    }
    goto success4;
  }

fail4:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success4:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_int_pref(prefidx, idx);
  return TRUE;
}


boolean pref_factory_float(const char *prefidx, float newval, boolean permanent) {
  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_MASTER_VOLUME)) {
    char *ttip;
    if ((LIVES_IS_PLAYING && future_prefs->volume == newval) || (!LIVES_IS_PLAYING && prefs->volume == (double)newval)) goto fail5;
    future_prefs->volume = newval;
    ttip = lives_strdup_printf(_("Audio volume (%.2f)"), newval);
    lives_widget_set_tooltip_text(mainw->vol_toolitem, _(ttip));
    lives_free(ttip);
    if (!LIVES_IS_PLAYING) {
      prefs->volume = newval;
    } else permanent = FALSE;
    if (LIVES_IS_RANGE(mainw->volume_scale)) {
      lives_range_set_value(LIVES_RANGE(mainw->volume_scale), newval);
    } else {
      lives_scale_button_set_value(LIVES_SCALE_BUTTON(mainw->volume_scale), newval);
    }
    goto success5;
  }

  if (!lives_strcmp(prefidx, PREF_AHOLD_THRESHOLD)) {
    if (prefs->ahold_threshold == newval) goto fail5;
    prefs->ahold_threshold = newval;
    goto success5;
  }

  if (!lives_strcmp(prefidx, PREF_SCREEN_GAMMA)) {
    if (prefs->screen_gamma == newval) goto fail5;
    prefs->screen_gamma = newval;
    goto success5;
  }

fail5:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success5:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_double_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_bitmapped(const char *prefidx, int bitfield, boolean newval, boolean permanent) {
  if (prefsw != NULL) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_AUDIO_OPTS)) {
    if (newval && !(prefs->audio_opts & bitfield)) prefs->audio_opts &= bitfield;
    else if (!newval && (prefs->audio_opts & bitfield)) prefs->audio_opts ^= bitfield;
    else goto fail6;

    if (permanent) set_int_pref(PREF_AUDIO_OPTS, prefs->audio_opts);

    if (prefsw != NULL) {
      if (bitfield == AUDIO_OPTS_FOLLOW_FPS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow),
                                       (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_FOLLOW_CLIPS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips),
                                       (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) ? TRUE : FALSE);
      }
    }
    goto success6;
  }

  if (!lives_strcmp(prefidx, PREF_OMC_DEV_OPTS)) {
    if (newval && !(prefs->omc_dev_opts & bitfield)) prefs->omc_dev_opts &= bitfield;
    else if (!newval && (prefs->omc_dev_opts & bitfield)) prefs->omc_dev_opts ^= bitfield;
    else goto fail6;

    if (permanent) set_int_pref(PREF_OMC_DEV_OPTS, prefs->omc_dev_opts);

    if (bitfield == OMC_DEV_JS) {
      if (newval) js_open();
      else js_close();
    } else if (bitfield == OMC_DEV_MIDI) {
      if (!newval) midi_close();
    }
#ifdef ALSA_MIDI
    else if (bitfield == OMC_DEV_FORCE_RAW_MIDI) {
      prefs->use_alsa_midi = !newval;
    } else if (bitfield == OMC_DEV_MIDI_DUMMY) {
      prefs->alsa_midi_dummy = newval;
    }
#endif
    goto success6;
  }

fail6:
  if (prefsw != NULL) prefsw->ignore_apply = FALSE;
  return FALSE;

success6:
  if (prefsw != NULL) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    prefsw->ignore_apply = FALSE;
  }
  return TRUE;
}


boolean apply_prefs(boolean skip_warn) {
  // set current prefs from prefs dialog
  char prefworkdir[PATH_MAX];

  const char *video_open_command = lives_entry_get_text(LIVES_ENTRY(prefsw->video_open_entry));
  /* const char *audio_play_command = lives_entry_get_text(LIVES_ENTRY(prefsw->audio_command_entry)); */
  const char *def_vid_load_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->vid_load_dir_entry));
  const char *def_vid_save_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->vid_save_dir_entry));
  const char *def_audio_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->audio_dir_entry));
  const char *def_image_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->image_dir_entry));
  const char *def_proj_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->proj_dir_entry));
  const char *wp_path = lives_entry_get_text(LIVES_ENTRY(prefsw->wpp_entry));
  const char *frei0r_path = lives_entry_get_text(LIVES_ENTRY(prefsw->frei0r_entry));
  const char *ladspa_path = lives_entry_get_text(LIVES_ENTRY(prefsw->ladspa_entry));

  const char *sepimg_path = lives_entry_get_text(LIVES_ENTRY(prefsw->sepimg_entry));
  const char *frameblank_path = lives_entry_get_text(LIVES_ENTRY(prefsw->frameblank_entry));

  char workdir[PATH_MAX];
  char *theme = lives_combo_get_active_text(LIVES_COMBO(prefsw->theme_combo));
  char *audp = lives_combo_get_active_text(LIVES_COMBO(prefsw->audp_combo));
  char *audio_codec = NULL;
  char *pb_quality = lives_combo_get_active_text(LIVES_COMBO(prefsw->pbq_combo));

  LiVESWidgetColor colf, colb, colf2, colb2, coli, colt, col, coltcfg, coltcbg;
  lives_colRGBA64_t lcol;

  boolean pbq_adap = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pbq_adaptive));
  int pbq = PB_QUALITY_MED;
  int idx;

  boolean needs_restart = FALSE;

  double default_fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_def_fps));
  double ext_aud_thresh = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_ext_aud_thresh)) / 100.;
  boolean load_rfx = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_load_rfx));
  boolean apply_gamma = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_apply_gamma));
  boolean antialias = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_antialias));
  boolean fx_threads = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads));

  boolean lbox = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lb));
  boolean lboxmt = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbmt));
  boolean srgb = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_srgb));
  double gamma = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gamma));

  int nfx_threads = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_nfx_threads));

  boolean stop_screensaver = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  boolean open_maximised = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->open_maximised_check));
  boolean fs_maximised = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->fs_max_check));
  boolean show_recent = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check));
  boolean stream_audio_out = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio));
  boolean rec_after_pb = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb));

  int max_msgs = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->nmessages_spin));
  char *msgtextsize = lives_combo_get_active_text(LIVES_COMBO(prefsw->msg_textsize_combo));
  boolean msgs_unlimited = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_unlimited));
  boolean msgs_pbdis = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_pbdis));

  uint64_t ds_warn_level = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds)) * 1000000;
  uint64_t ds_crit_level = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_crit_ds)) * 1000000;

  boolean warn_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fps));
  boolean warn_save_set = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_save_set));
  boolean warn_fsize = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_fsize));
  boolean warn_mplayer = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mplayer));
  boolean warn_rendered_fx = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_rendered_fx));
  boolean warn_encoders = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_encoders));
  boolean warn_duplicate_set = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_dup_set));
  boolean warn_layout_missing_clips = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_clips));
  boolean warn_layout_close = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_close));
  boolean warn_layout_delete = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_delete));
  boolean warn_layout_shift = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_shift));
  boolean warn_layout_alter = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_alter));
  boolean warn_discard_layout = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_discard_layout));
  boolean warn_after_dvgrab =
#ifdef HAVE_LDVGRAB
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_dvgrab));
#else
    !(prefs->warning_mask & WARN_MASK_AFTER_DVGRAB);
#endif
  boolean warn_mt_achans = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_achans));
  boolean warn_mt_no_jack = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_no_jack));
  boolean warn_yuv4m_open =
#ifdef HAVE_YUV4MPEG
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_yuv4m_open));
#else
    !(prefs->warning_mask & WARN_MASK_OPEN_YUV4M);
#endif

  boolean warn_layout_adel = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_adel));
  boolean warn_layout_ashift = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_ashift));
  boolean warn_layout_aalt = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_aalt));
  boolean warn_layout_popup = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_popup));
  boolean warn_mt_backup_space
    = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_mt_backup_space));
  boolean warn_after_crash = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_crash));
  boolean warn_no_pulse = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_no_pulse));
  boolean warn_layout_wipe = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_wipe));
  boolean warn_layout_gamma = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_layout_gamma));
  boolean warn_vjmode_enter = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_vjmode_enter));

  boolean midisynch = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->check_midi));
  boolean instant_open = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open));
  boolean auto_deint = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint));
  boolean auto_trim = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_trim));
  boolean auto_nobord = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_nobord));
  boolean concat_images = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_concat_images));
  boolean ins_speed = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ins_speed));
  boolean show_player_stats = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_stats));
  boolean ext_jpeg = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jpeg));
  boolean show_tool = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->show_tool));
  boolean mouse_scroll = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mouse_scroll));
  boolean ce_maxspect = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_ce_maxspect));
  boolean show_button_icons = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_button_icons));
  boolean show_asrc = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_asrc));
  boolean show_ttips = prefsw->checkbutton_show_ttips == NULL ? FALSE : lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(
                         prefsw->checkbutton_show_ttips));
  boolean hfbwnp = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_hfbwnp));

  int fsize_to_warn = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_fsize));
  int dl_bwidth = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_bwidth));
  int ocp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_ocp));

  boolean rec_frames = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rframes));
  boolean rec_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rfps));
  boolean rec_effects = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->reffects));
  boolean rec_clips = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rclips));
  boolean rec_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->raudio));
  boolean pa_gens = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pa_gens));
  boolean rec_ext_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio));
#ifdef RT_AUDIO
  boolean rec_desk_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rdesk_audio));
#endif

  boolean mt_enter_prompt = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_enter_prompt));
  boolean render_prompt = !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt));

  int mt_def_width = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_width));
  int mt_def_height = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_height));
  int mt_def_fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_fps));
  int mt_def_arate = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  int mt_def_achans = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  int mt_def_asamps = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  int mt_def_signed_endian = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned)) *
                             AFORM_UNSIGNED + lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))
                             * AFORM_BIG_ENDIAN;
  int mt_undo_buf = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_undo_buf));

  boolean mt_exit_render = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render));
  boolean mt_enable_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton));
  boolean mt_pertrack_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton));
  boolean mt_backaudio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton));

  boolean mt_autoback_always = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always));
  boolean mt_autoback_never = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never));

  int mt_autoback_time = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time));
  int max_disp_vtracks = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_max_disp_vtracks));
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));

  boolean ce_thumbs = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ce_thumbs));

  boolean forcesmon = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon));
  boolean startup_ce = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce));

#ifdef ENABLE_JACK_TRANSPORT
  boolean jack_tstart = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_tjack));
  boolean jack_master = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master));
  boolean jack_client = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client));
  boolean jack_tb_start = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start));
  boolean jack_tb_client = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client));
#else
#ifdef ENABLE_JACK
  boolean jack_tstart = FALSE;
  boolean jack_master = FALSE;
  boolean jack_client = FALSE;
  boolean jack_tb_start = FALSE;
  boolean jack_tb_client = FALSE;
#endif
#endif

#ifdef ENABLE_JACK
  boolean jack_astart = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack));
  boolean jack_pwp = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_pwp));
  boolean jack_read_autocon = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_read_autocon));
  uint32_t jack_opts = (JACK_OPTS_TRANSPORT_CLIENT * jack_client + JACK_OPTS_TRANSPORT_MASTER * jack_master +
                        JACK_OPTS_START_TSERVER * jack_tstart + JACK_OPTS_START_ASERVER * jack_astart +
                        JACK_OPTS_NOPLAY_WHEN_PAUSED * !jack_pwp + JACK_OPTS_TIMEBASE_START * jack_tb_start +
                        JACK_OPTS_TIMEBASE_CLIENT * jack_tb_client + JACK_OPTS_NO_READ_AUTOCON * !jack_read_autocon);
#endif

#ifdef RT_AUDIO
  boolean audio_follow_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow));
  boolean audio_follow_clips = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips));
  uint32_t audio_opts = (AUDIO_OPTS_FOLLOW_FPS * audio_follow_fps + AUDIO_OPTS_FOLLOW_CLIPS * audio_follow_clips);
#endif

#ifdef ENABLE_OSC
  uint32_t osc_udp_port = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_osc_udp));
  boolean osc_start = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC_start));
  boolean osc_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC));
#endif

  int rte_keys_virtual = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rte_keys));

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  boolean omc_js_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_js));
  const char *omc_js_fname = lives_entry_get_text(LIVES_ENTRY(prefsw->omc_js_entry));
#endif

#ifdef OMC_MIDI_IMPL
  boolean omc_midi_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_midi));
  const char *omc_midi_fname = lives_entry_get_text(LIVES_ENTRY(prefsw->omc_midi_entry));
  int midicr = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midicr));
  int midirpt = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midirpt));
  char *midichan = lives_combo_get_active_text(LIVES_COMBO(prefsw->midichan_combo));

#ifdef ALSA_MIDI
  boolean use_alsa_midi = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi));
  boolean alsa_midi_dummy = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi_dummy));
#endif

#endif
#endif

  boolean pstyle2 = palette->style & STYLE_2;
  boolean pstyle3 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style3));
  boolean pstyle4 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style4));

  int rec_gb = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rec_gb));

  char audio_player[256];
  int listlen = lives_list_length(prefs->acodec_list);
  int rec_opts = rec_frames * REC_FRAMES + rec_fps * REC_FPS + rec_effects * REC_EFFECTS + rec_clips * REC_CLIPS + rec_audio *
                 REC_AUDIO
                 + rec_after_pb * REC_AFTER_PB;
  uint64_t warn_mask;

  unsigned char *new_undo_buf;
  LiVESList *ulist;

#ifdef ENABLE_OSC
  boolean set_omc_dev_opts = FALSE;
#ifdef OMC_MIDI_IMPL
  boolean needs_midi_restart = FALSE;
#endif
#endif

  char *tmp;

  char *cdplay_device = lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->cdplay_entry)), -1, NULL, NULL,
                        NULL);

  lives_snprintf(prefworkdir, PATH_MAX, "%s", prefs->workdir);
  ensure_isdir(prefworkdir);

  // TODO: move all into pref_factory_* functions
  mainw->no_context_update = TRUE;

  if (prefsw->theme_style2 != NULL)
    pstyle2 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style2));

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fore), &colf);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_back), &colb);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mabf), &colf2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mab), &colb2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infob), &coli);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infot), &colt);

  if (strcasecmp(future_prefs->theme, "none")) {
    if (!lives_widget_color_equal(&colf, &palette->normal_fore) ||
        !lives_widget_color_equal(&colb, &palette->normal_back) ||
        !lives_widget_color_equal(&colf2, &palette->menu_and_bars_fore) ||
        !lives_widget_color_equal(&colb2, &palette->menu_and_bars) ||
        !lives_widget_color_equal(&colt, &palette->info_text) ||
        !lives_widget_color_equal(&coli, &palette->info_base) ||
        ((pstyle2 * STYLE_2) != (palette->style & STYLE_2)) ||
        ((pstyle3 * STYLE_3) != (palette->style & STYLE_3)) ||
        ((pstyle4 * STYLE_4) != (palette->style & STYLE_4))
       ) {

      lives_widget_color_copy(&palette->normal_fore, &colf);
      lives_widget_color_copy(&palette->normal_back, &colb);
      lives_widget_color_copy(&palette->menu_and_bars_fore, &colf2);
      lives_widget_color_copy(&palette->menu_and_bars, &colb2);
      lives_widget_color_copy(&palette->info_base, &coli);
      lives_widget_color_copy(&palette->info_text, &colt);

      palette->style = STYLE_1 | (pstyle2 * STYLE_2) | (pstyle3 * STYLE_3) | (pstyle4 * STYLE_4);
      mainw->prefs_changed |= PREFS_COLOURS_CHANGED;
    }
  }

  if (pref_factory_color_button(&palette->ce_unsel, LIVES_COLOR_BUTTON(prefsw->cbutton_cesel)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_ceunsel), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->ce_unsel)) {
    lives_rgba_copy(&palette->ce_unsel, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fsur), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->frame_surround)) {
    lives_rgba_copy(&palette->frame_surround, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcfg), &coltcfg);
  if (!lives_widget_color_equal(&coltcfg, &palette->mt_timecode_fg)) {
    lives_widget_color_copy(&palette->mt_timecode_fg, &coltcfg);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcbg), &coltcbg);
  if (!lives_widget_color_equal(&coltcbg, &palette->mt_timecode_bg)) {
    lives_widget_color_copy(&palette->mt_timecode_bg, &coltcbg);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mtmark), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->mt_mark)) {
    lives_rgba_copy(&palette->mt_mark, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_evbox), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->mt_evbox)) {
    lives_rgba_copy(&palette->mt_evbox, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tlreg), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->mt_timeline_reg)) {
    lives_rgba_copy(&palette->mt_timeline_reg, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_vidcol), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->vidcol)) {
    lives_rgba_copy(&palette->vidcol, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_audcol), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->audcol)) {
    lives_rgba_copy(&palette->audcol, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fxcol), &col);
  widget_color_to_lives_rgba(&lcol, &col);
  if (!lives_rgba_equal(&lcol, &palette->fxcol)) {
    lives_rgba_copy(&palette->fxcol, &lcol);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  if (capable->has_encoder_plugins) {
    audio_codec = lives_combo_get_active_text(LIVES_COMBO(prefsw->acodec_combo));

    for (idx = 0; idx < listlen && strcmp((char *)lives_list_nth_data(prefs->acodec_list, idx), audio_codec); idx++);
    lives_free(audio_codec);

    if (idx == listlen) future_prefs->encoder.audio_codec = 0;
    else future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[idx];
  } else future_prefs->encoder.audio_codec = 0;

  lives_snprintf(workdir, PATH_MAX, "%s",
                 (tmp = lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->workdir_entry)),
                        -1, NULL, NULL, NULL)));
  lives_free(tmp);

  if (audp == NULL ||
      !strncmp(audp, mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
               strlen(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) lives_snprintf(audio_player, 256, AUDIO_PLAYER_NONE);
  else if (!strncmp(audp, AUDIO_PLAYER_JACK, strlen(AUDIO_PLAYER_JACK))) lives_snprintf(audio_player, 256, AUDIO_PLAYER_JACK);
  else if (!strncmp(audp, AUDIO_PLAYER_SOX, strlen(AUDIO_PLAYER_SOX))) lives_snprintf(audio_player, 256, AUDIO_PLAYER_SOX);
  else if (!strncmp(audp, AUDIO_PLAYER_PULSE_AUDIO, strlen(AUDIO_PLAYER_PULSE_AUDIO))) lives_snprintf(audio_player, 256,
        AUDIO_PLAYER_PULSE);

  lives_free(audp);

  if (!((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) || (prefs->audio_player == AUD_PLAYER_PULSE &&
        capable->has_pulse_audio))) {
    if (prefs->audio_src == AUDIO_SRC_EXT) future_prefs->audio_src = prefs->audio_src = AUDIO_SRC_INT;
  }

  if (rec_opts != prefs->rec_opts) {
    prefs->rec_opts = rec_opts;
    set_int_pref(PREF_RECORD_OPTS, prefs->rec_opts);
  }

  if (mainw->multitrack == NULL) {
    pref_factory_bool(PREF_REC_EXT_AUDIO, rec_ext_audio, TRUE);
  } else {
    future_prefs->audio_src = rec_ext_audio ? AUDIO_SRC_EXT : AUDIO_SRC_INT;
  }

  warn_mask = !warn_fps * WARN_MASK_FPS + !warn_save_set * WARN_MASK_SAVE_SET
              + !warn_fsize * WARN_MASK_FSIZE + !warn_mplayer *
              WARN_MASK_NO_MPLAYER + !warn_rendered_fx * WARN_MASK_RENDERED_FX + !warn_encoders *
              WARN_MASK_NO_ENCODERS + !warn_layout_missing_clips * WARN_MASK_LAYOUT_MISSING_CLIPS + !warn_duplicate_set *
              WARN_MASK_DUPLICATE_SET + !warn_layout_close * WARN_MASK_LAYOUT_CLOSE_FILE + !warn_layout_delete *
              WARN_MASK_LAYOUT_DELETE_FRAMES + !warn_layout_shift * WARN_MASK_LAYOUT_SHIFT_FRAMES + !warn_layout_alter *
              WARN_MASK_LAYOUT_ALTER_FRAMES + !warn_discard_layout * WARN_MASK_EXIT_MT + !warn_after_dvgrab *
              WARN_MASK_AFTER_DVGRAB + !warn_mt_achans * WARN_MASK_MT_ACHANS + !warn_mt_no_jack *
              WARN_MASK_MT_NO_JACK + !warn_layout_adel * WARN_MASK_LAYOUT_DELETE_AUDIO + !warn_layout_ashift *
              WARN_MASK_LAYOUT_SHIFT_AUDIO + !warn_layout_aalt * WARN_MASK_LAYOUT_ALTER_AUDIO + !warn_layout_popup *
              WARN_MASK_LAYOUT_POPUP + !warn_yuv4m_open * WARN_MASK_OPEN_YUV4M + !warn_mt_backup_space *
              WARN_MASK_MT_BACKUP_SPACE + !warn_after_crash * WARN_MASK_CLEAN_AFTER_CRASH
              + !warn_no_pulse * WARN_MASK_NO_PULSE_CONNECT
              + !warn_layout_wipe * WARN_MASK_LAYOUT_WIPE + !warn_layout_gamma * WARN_MASK_LAYOUT_GAMMA + !warn_vjmode_enter *
              WARN_MASK_VJMODE_ENTER;

  if (warn_mask != prefs->warning_mask) {
    prefs->warning_mask = warn_mask;
    set_int64_pref(PREF_LIVES_WARNING_MASK, prefs->warning_mask);
  }

  if (msgs_unlimited) {
    pref_factory_int(PREF_MAX_MSGS, -max_msgs, TRUE);
  } else {
    pref_factory_int(PREF_MAX_MSGS, max_msgs, TRUE);
  }

  pref_factory_bool(PREF_MSG_PBDIS, msgs_pbdis, TRUE);

  pref_factory_bool(PREF_LETTERBOX, lbox, TRUE);
  pref_factory_bool(PREF_LETTERBOXMT, lboxmt, TRUE);

  pref_factory_bool(PREF_SRGB_GAMMA, srgb, TRUE);
  pref_factory_float(PREF_SCREEN_GAMMA, gamma, TRUE);

  ulist = get_textsizes_list();
  pref_factory_string_choice(PREF_MSG_TEXTSIZE, ulist, msgtextsize, TRUE);
  lives_list_free_all(&ulist);
  lives_free(msgtextsize);

  if (fsize_to_warn != (prefs->warn_file_size)) {
    prefs->warn_file_size = fsize_to_warn;
    set_int_pref(PREF_WARN_FILE_SIZE, fsize_to_warn);
  }

  if (dl_bwidth != (prefs->dl_bandwidth)) {
    prefs->dl_bandwidth = dl_bwidth;
    set_int_pref(PREF_DL_BANDWIDTH_K, dl_bwidth);
  }

  if (ocp != (prefs->ocp)) {
    prefs->ocp = ocp;
    set_int_pref(PREF_OPEN_COMPRESSION_PERCENT, ocp);
  }

  if (show_tool != (future_prefs->show_tool)) {
    future_prefs->show_tool = prefs->show_tool = show_tool;
    set_boolean_pref(PREF_SHOW_TOOLBAR, show_tool);
  }

  if (mouse_scroll != (prefs->mouse_scroll_clips)) {
    prefs->mouse_scroll_clips = mouse_scroll;
    set_boolean_pref(PREF_MOUSE_SCROLL_CLIPS, mouse_scroll);
  }

  pref_factory_bool(PREF_PUSH_AUDIO_TO_GENS, pa_gens, TRUE);

  pref_factory_bool(PREF_SHOW_BUTTON_ICONS, show_button_icons, TRUE);

#ifdef HAVE_PULSE_AUDIO
  pref_factory_bool(PREF_PARESTART, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart)),
                    TRUE);
  if (prefs->pa_restart)
    pref_factory_string(PREF_PASTARTOPTS, lives_entry_get_text(LIVES_ENTRY(prefsw->audio_command_entry)), TRUE);
#endif

  if (show_asrc != (prefs->show_asrc)) {
    pref_factory_bool(PREF_SHOW_ASRC, show_asrc, TRUE);
  }

#if GTK_CHECK_VERSION(2, 12, 0)
  if (show_ttips != (prefs->show_tooltips)) {
    pref_factory_bool(PREF_SHOW_TOOLTIPS, show_ttips, TRUE);
  }
#endif

  if (hfbwnp != (prefs->hfbwnp)) {
    pref_factory_bool(PREF_HFBWNP, hfbwnp, TRUE);
  }

  if (ce_maxspect != (prefs->ce_maxspect)) {
    prefs->ce_maxspect = ce_maxspect;
    set_boolean_pref(PREF_CE_MAXSPECT, ce_maxspect);
    if (mainw->multitrack == NULL) {
      if (mainw->current_file > -1) {
        int current_file = mainw->current_file;
        switch_to_file((mainw->current_file = 0), current_file);
      }
    }
  }

  if (lives_strcmp(wp_path, prefs->weed_plugin_path)) {
    set_string_pref(PREF_WEED_PLUGIN_PATH, wp_path);
    lives_snprintf(prefs->weed_plugin_path, PATH_MAX, "%s", wp_path);
  }

  if (lives_strcmp(frei0r_path, prefs->frei0r_path)) {
    set_string_pref(PREF_FREI0R_PATH, frei0r_path);
    lives_snprintf(prefs->frei0r_path, PATH_MAX, "%s", frei0r_path);
  }

  if (lives_strcmp(ladspa_path, prefs->ladspa_path)) {
    set_string_pref(PREF_LADSPA_PATH, ladspa_path);
    lives_snprintf(prefs->ladspa_path, PATH_MAX, "%s", ladspa_path);
  }

  if (lives_strcmp(sepimg_path, mainw->sepimg_path)) {
    lives_snprintf(mainw->sepimg_path, PATH_MAX, "%s", sepimg_path);
    mainw->prefs_changed |= PREFS_IMAGES_CHANGED;
  }

  if (lives_strcmp(frameblank_path, mainw->frameblank_path)) {
    lives_snprintf(mainw->frameblank_path, PATH_MAX, "%s", frameblank_path);
    mainw->prefs_changed |= PREFS_IMAGES_CHANGED;
  }

  ensure_isdir(workdir);

  if (lives_strcmp(prefworkdir, workdir)) {
    char *xworkdir = lives_strdup(workdir);
    if (check_workdir_valid(&xworkdir, LIVES_DIALOG(prefsw->prefs_dialog), FALSE) == LIVES_RESPONSE_OK) {
      char *msg = workdir_ch_warning();
      lives_snprintf(workdir, PATH_MAX, "%s", xworkdir);
      set_workdir_label_text(LIVES_LABEL(prefsw->workdir_label), xworkdir);
      lives_free(xworkdir);

      lives_widget_queue_draw(prefsw->workdir_label);
      lives_widget_context_update(); // update prefs window before showing confirmation box

      if (do_warning_dialog(msg)) {
        lives_snprintf(future_prefs->workdir, PATH_MAX, "%s", workdir);
        mainw->prefs_changed = PREFS_WORKDIR_CHANGED;
        needs_restart = TRUE;
      } else {
        future_prefs->workdir[0] = '\0';
        set_workdir_label_text(LIVES_LABEL(prefsw->workdir_label), prefs->workdir);
      }
      lives_free(msg);
    }
  }

  // disabled_decoders
  if (string_lists_differ(prefs->disabled_decoders, future_prefs->disabled_decoders)) {
    lives_list_free_all(&prefs->disabled_decoders);
    prefs->disabled_decoders = lives_list_copy_strings(future_prefs->disabled_decoders);
    if (prefs->disabled_decoders != NULL) set_list_pref(PREF_DISABLED_DECODERS, prefs->disabled_decoders);
    else delete_pref(PREF_DISABLED_DECODERS);
  }

  // stop xscreensaver
  if (prefs->stop_screensaver != stop_screensaver) {
    prefs->stop_screensaver = stop_screensaver;
    set_boolean_pref(PREF_STOP_SCREENSAVER, prefs->stop_screensaver);
  }

  // antialias
  if (prefs->antialias != antialias) {
    prefs->antialias = antialias;
    set_boolean_pref(PREF_ANTIALIAS, antialias);
  }

  // load rfx
  if (prefs->load_rfx_builtin != load_rfx) {
    prefs->load_rfx_builtin = load_rfx;
    set_boolean_pref(PREF_LOAD_RFX_BUILTIN, load_rfx);
  }

  // apply gamma correction
  if (prefs->apply_gamma != apply_gamma) {
    prefs->apply_gamma = apply_gamma;
    set_boolean_pref(PREF_APPLY_GAMMA, apply_gamma);
    needs_restart = TRUE;
  }

  // fx_threads
  if (!fx_threads) nfx_threads = 1;
  if (prefs->nfx_threads != nfx_threads) {
    future_prefs->nfx_threads = nfx_threads;
    set_int_pref(PREF_NFX_THREADS, nfx_threads);
  }

  // open maximised
  if (prefs->open_maximised != open_maximised) {
    prefs->open_maximised = open_maximised;
    set_boolean_pref(PREF_OPEN_MAXIMISED, open_maximised);
  }

  // filesel maximised
  if (prefs->fileselmax != fs_maximised) {
    prefs->fileselmax = fs_maximised;
    set_boolean_pref(PREF_FILESEL_MAXIMISED, fs_maximised);
  }

  // monitors

  if (forcesmon != prefs->force_single_monitor) {
    prefs->force_single_monitor = forcesmon;
    set_boolean_pref(PREF_FORCE_SINGLE_MONITOR, forcesmon);
    get_monitors(FALSE);
    if (capable->nmonitors == 0) resize_widgets_for_monitor(TRUE);
  }

  if (capable->nmonitors > 1) {
    if (gui_monitor != prefs->gui_monitor || play_monitor != prefs->play_monitor) {
      char *str = lives_strdup_printf("%d,%d", gui_monitor, play_monitor);
      set_string_pref(PREF_MONITORS, str);
      prefs->gui_monitor = gui_monitor;
      prefs->play_monitor = play_monitor;
      widget_opts.monitor = prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : capable->primary_monitor;
      resize_widgets_for_monitor(TRUE);
    }
  }

  if (ce_thumbs != prefs->ce_thumb_mode) {
    prefs->ce_thumb_mode = ce_thumbs;
    set_boolean_pref(PREF_CE_THUMB_MODE, ce_thumbs);
  }

  // fps stats
  if (prefs->show_player_stats != show_player_stats) {
    prefs->show_player_stats = show_player_stats;
    set_boolean_pref(PREF_SHOW_PLAYER_STATS, show_player_stats);
  }

  if (prefs->stream_audio_out != stream_audio_out) {
    prefs->stream_audio_out = stream_audio_out;
    set_boolean_pref(PREF_STREAM_AUDIO_OUT, stream_audio_out);
  }

  pref_factory_bool(PREF_SHOW_RECENT_FILES, show_recent, TRUE);

  // midi synch
  if (prefs->midisynch != midisynch) {
    prefs->midisynch = midisynch;
    set_boolean_pref(PREF_MIDISYNCH, midisynch);
  }

  // jpeg/png
  if (strcmp(prefs->image_ext, LIVES_FILE_EXT_JPG) && ext_jpeg) {
    set_string_pref(PREF_DEFAULT_IMAGE_FORMAT, LIVES_IMAGE_TYPE_JPEG);
    lives_snprintf(prefs->image_ext, 16, LIVES_FILE_EXT_JPG);
  } else if (!strcmp(prefs->image_ext, LIVES_FILE_EXT_JPG) && !ext_jpeg) {
    set_string_pref(PREF_DEFAULT_IMAGE_FORMAT, LIVES_IMAGE_TYPE_PNG);
    lives_snprintf(prefs->image_ext, 16, LIVES_FILE_EXT_PNG);
  }

  // instant open
  if (prefs->instant_open != instant_open) {
    set_boolean_pref(PREF_INSTANT_OPEN, (prefs->instant_open = instant_open));
  }

  // auto deinterlace
  if (prefs->auto_deint != auto_deint) {
    set_boolean_pref(PREF_AUTO_DEINTERLACE, (prefs->auto_deint = auto_deint));
  }

  // auto deinterlace
  if (prefs->auto_trim_audio != auto_trim) {
    set_boolean_pref(PREF_AUTO_TRIM_PAD_AUDIO, (prefs->auto_trim_audio = auto_trim));
  }

  // auto border cut
  if (prefs->auto_nobord != auto_nobord) {
    set_boolean_pref(PREF_AUTO_CUT_BORDERS, (prefs->auto_nobord = auto_nobord));
  }

  // concat images
  if (prefs->concat_images != concat_images) {
    set_boolean_pref(PREF_CONCAT_IMAGES, (prefs->concat_images = concat_images));
  }

  // encoder
  if (strcmp(prefs->encoder.name, future_prefs->encoder.name)) {
    lives_snprintf(prefs->encoder.name, 64, "%s", future_prefs->encoder.name);
    set_string_pref(PREF_ENCODER, prefs->encoder.name);
    lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
    prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
  }

  // output format
  if (strcmp(prefs->encoder.of_name, future_prefs->encoder.of_name)) {
    lives_snprintf(prefs->encoder.of_name, 64, "%s", future_prefs->encoder.of_name);
    lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
    lives_snprintf(prefs->encoder.of_desc, 128, "%s", future_prefs->encoder.of_desc);
    prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
    set_string_pref(PREF_OUTPUT_TYPE, prefs->encoder.of_name);
  }

  if (prefs->encoder.audio_codec != future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec = future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec < AUDIO_CODEC_UNKNOWN) {
      set_int_pref(PREF_ENCODER_ACODEC, prefs->encoder.audio_codec);
    }
  }

  // pb quality
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 0))) pbq = PB_QUALITY_LOW;
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 1))) pbq = PB_QUALITY_MED;
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 2))) pbq = PB_QUALITY_HIGH;

  lives_free(pb_quality);

  if (pbq != prefs->pb_quality) {
    future_prefs->pb_quality = prefs->pb_quality = pbq;
    set_int_pref(PREF_PB_QUALITY, pbq);
  }

  pref_factory_bool(PREF_PBQ_ADAPTIVE, pbq_adap, TRUE);

  // video open command
  if (lives_strcmp(prefs->video_open_command, video_open_command)) {
    lives_snprintf(prefs->video_open_command, PATH_MAX * 2, "%s", video_open_command);
    set_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command);
  }

  //playback plugin
  set_vpp(TRUE);

  /* // audio play command */
  /* if (strcmp(prefs->audio_play_command, audio_play_command)) { */
  /*   lives_snprintf(prefs->audio_play_command, PATH_MAX * 2, "%s", audio_play_command); */
  /*   set_string_pref(PREF_AUDIO_PLAY_COMMAND, prefs->audio_play_command); */
  /* } */

  // cd play device
  if (lives_strcmp(prefs->cdplay_device, cdplay_device)) {
    lives_snprintf(prefs->cdplay_device, PATH_MAX, "%s", cdplay_device);
    set_string_pref(PREF_CDPLAY_DEVICE, prefs->cdplay_device);
  }

  lives_free(cdplay_device);

  // default video load directory
  if (lives_strcmp(prefs->def_vid_load_dir, def_vid_load_dir)) {
    lives_snprintf(prefs->def_vid_load_dir, PATH_MAX, "%s/", def_vid_load_dir);
    get_dirname(prefs->def_vid_load_dir);
    set_utf8_pref(PREF_VID_LOAD_DIR, prefs->def_vid_load_dir);
    lives_snprintf(mainw->vid_load_dir, PATH_MAX, "%s", prefs->def_vid_load_dir);
  }

  // default video save directory
  if (lives_strcmp(prefs->def_vid_save_dir, def_vid_save_dir)) {
    lives_snprintf(prefs->def_vid_save_dir, PATH_MAX, "%s/", def_vid_save_dir);
    get_dirname(prefs->def_vid_save_dir);
    set_utf8_pref(PREF_VID_SAVE_DIR, prefs->def_vid_save_dir);
    lives_snprintf(mainw->vid_save_dir, PATH_MAX, "%s", prefs->def_vid_save_dir);
  }

  // default audio directory
  if (lives_strcmp(prefs->def_audio_dir, def_audio_dir)) {
    lives_snprintf(prefs->def_audio_dir, PATH_MAX, "%s/", def_audio_dir);
    get_dirname(prefs->def_audio_dir);
    set_utf8_pref(PREF_AUDIO_DIR, prefs->def_audio_dir);
    lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", prefs->def_audio_dir);
  }

  // default image directory
  if (lives_strcmp(prefs->def_image_dir, def_image_dir)) {
    lives_snprintf(prefs->def_image_dir, PATH_MAX, "%s/", def_image_dir);
    get_dirname(prefs->def_image_dir);
    set_utf8_pref(PREF_IMAGE_DIR, prefs->def_image_dir);
    lives_snprintf(mainw->image_dir, PATH_MAX, "%s", prefs->def_image_dir);
  }

  // default project directory - for backup and restore
  if (lives_strcmp(prefs->def_proj_dir, def_proj_dir)) {
    lives_snprintf(prefs->def_proj_dir, PATH_MAX, "%s/", def_proj_dir);
    get_dirname(prefs->def_proj_dir);
    set_utf8_pref(PREF_PROJ_DIR, prefs->def_proj_dir);
    lives_snprintf(mainw->proj_load_dir, PATH_MAX, "%s", prefs->def_proj_dir);
    lives_snprintf(mainw->proj_save_dir, PATH_MAX, "%s", prefs->def_proj_dir);
  }

  // the theme
  if (lives_utf8_strcmp(future_prefs->theme, theme) && !(!strcasecmp(future_prefs->theme, "none") &&
      !lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) {
    if (lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      lives_snprintf(prefs->theme, 64, "%s", theme);
      lives_snprintf(future_prefs->theme, 64, "%s", theme);
      set_string_pref(PREF_GUI_THEME, future_prefs->theme);
      widget_opts.apply_theme = TRUE;
      set_palette_colours(TRUE);
      if (mainw->multitrack != NULL) {
        if (mainw->multitrack->frame_pixbuf == mainw->imframe) mainw->multitrack->frame_pixbuf = NULL;
      }
      load_theme_images();
      mainw->prefs_changed |= PREFS_COLOURS_CHANGED | PREFS_IMAGES_CHANGED;
    } else {
      lives_snprintf(future_prefs->theme, 64, "none");
      set_string_pref(PREF_GUI_THEME, future_prefs->theme);
      delete_pref(THEME_DETAIL_STYLE);
      delete_pref(THEME_DETAIL_SEPWIN_IMAGE);
      delete_pref(THEME_DETAIL_FRAMEBLANK_IMAGE);
      delete_pref(THEME_DETAIL_NORMAL_FORE);
      delete_pref(THEME_DETAIL_NORMAL_BACK);
      delete_pref(THEME_DETAIL_ALT_FORE);
      delete_pref(THEME_DETAIL_ALT_BACK);
      delete_pref(THEME_DETAIL_INFO_TEXT);
      delete_pref(THEME_DETAIL_INFO_BASE);
      mainw->prefs_changed |= PREFS_THEME_CHANGED;
    }
  }

  lives_free(theme);

  // default fps
  if (prefs->default_fps != default_fps) {
    prefs->default_fps = default_fps;
    set_double_pref(PREF_DEFAULT_FPS, prefs->default_fps);
  }

  // ahold
  pref_factory_float(PREF_AHOLD_THRESHOLD, ext_aud_thresh, TRUE);

  // virtual rte keys
  if (prefs->rte_keys_virtual != rte_keys_virtual) {
    // if we are showing the rte window, we must destroy and recreate it
    refresh_rte_window();

    prefs->rte_keys_virtual = rte_keys_virtual;
    set_int_pref(PREF_RTE_KEYS_VIRTUAL, prefs->rte_keys_virtual);
  }

  if (prefs->rec_stop_gb != rec_gb) {
    // disk free level at which we must stop recording
    prefs->rec_stop_gb = rec_gb;
    set_int_pref(PREF_REC_STOP_GB, prefs->rec_stop_gb);
  }

  if (ins_speed == prefs->ins_resample) {
    prefs->ins_resample = !ins_speed;
    set_boolean_pref(PREF_INSERT_RESAMPLE, prefs->ins_resample);
  }

  if (ds_warn_level != prefs->ds_warn_level) {
    prefs->ds_warn_level = ds_warn_level;
    mainw->next_ds_warn_level = prefs->ds_warn_level;
    set_int64_pref(PREF_DS_WARN_LEVEL, ds_warn_level);
  }

  if (ds_crit_level != prefs->ds_crit_level) {
    prefs->ds_crit_level = ds_crit_level;
    set_int64_pref(PREF_DS_CRIT_LEVEL, ds_crit_level);
  }

#ifdef ENABLE_OSC
  if (osc_enable) {
    if (prefs->osc_udp_started && osc_udp_port != prefs->osc_udp_port) {
      // port number changed
      lives_osc_end();
      prefs->osc_udp_started = FALSE;
    }
    prefs->osc_udp_port = osc_udp_port;
    // try to start on new port number
    if (!prefs->osc_udp_started) prefs->osc_udp_started = lives_osc_init(prefs->osc_udp_port);
  } else {
    if (prefs->osc_udp_started) {
      lives_osc_end();
      prefs->osc_udp_started = FALSE;
    }
  }
  if (osc_start) {
    if (!future_prefs->osc_start) {
      set_boolean_pref(PREF_OSC_START, TRUE);
      future_prefs->osc_start = TRUE;
    }
  } else {
    if (future_prefs->osc_start) {
      set_boolean_pref(PREF_OSC_START, FALSE);
      future_prefs->osc_start = FALSE;
    }
  }
  if (prefs->osc_udp_port != osc_udp_port) {
    prefs->osc_udp_port = osc_udp_port;
    set_int_pref(PREF_OSC_PORT, osc_udp_port);
  }
#endif

#ifdef RT_AUDIO
  if (prefs->audio_opts != audio_opts) {
    prefs->audio_opts = audio_opts;
    set_int_pref(PREF_AUDIO_OPTS, audio_opts);
  }

  if (rec_desk_audio != prefs->rec_desktop_audio) {
    prefs->rec_desktop_audio = rec_desk_audio;
    set_boolean_pref(PREF_REC_DESKTOP_AUDIO, rec_desk_audio);
  }
#endif

  pref_factory_string(PREF_AUDIO_PLAYER, audio_player, TRUE);

#ifdef ENABLE_JACK
  if (future_prefs->jack_opts != jack_opts) {
    set_int_pref(PREF_JACK_OPTS, jack_opts);
    future_prefs->jack_opts = prefs->jack_opts = jack_opts;
  }
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  pref_factory_string(PREF_OMC_JS_FNAME, omc_js_fname, TRUE);
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_JS, omc_js_enable, FALSE))
    set_omc_dev_opts = TRUE;
#endif

#ifdef OMC_MIDI_IMPL
  pref_factory_string(PREF_MIDI_RCV_CHANNEL, midichan, TRUE);
  lives_free(midichan);

  pref_factory_string(PREF_OMC_MIDI_FNAME, omc_midi_fname, TRUE);

  pref_factory_int(PREF_MIDI_CHECK_RATE, midicr, TRUE);
  pref_factory_int(PREF_MIDI_RPT, midirpt, TRUE);

  if (omc_midi_enable && !(prefs->omc_dev_opts & OMC_DEV_MIDI)) needs_midi_restart = TRUE;
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_MIDI, omc_midi_enable, FALSE))
    set_omc_dev_opts = TRUE;

#ifdef ALSA_MIDI
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_FORCE_RAW_MIDI, !use_alsa_midi, FALSE))
    set_omc_dev_opts = TRUE;

  if (use_alsa_midi == ((prefs->omc_dev_opts & OMC_DEV_FORCE_RAW_MIDI) / OMC_DEV_FORCE_RAW_MIDI)) {
    if (!needs_midi_restart) {
      needs_midi_restart = (mainw->ext_cntl[EXT_CNTL_MIDI]);
    }
  }

  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_MIDI_DUMMY, alsa_midi_dummy, FALSE)) {
    set_omc_dev_opts = TRUE;
    if (!needs_midi_restart) {
      needs_midi_restart = (mainw->ext_cntl[EXT_CNTL_MIDI]);
    }
  }
#endif

  if (needs_midi_restart) {
    midi_close();
    midi_open();
  }

#endif
  if (set_omc_dev_opts) set_int_pref(PREF_OMC_DEV_OPTS, prefs->omc_dev_opts);
#endif

  if (mt_enter_prompt != prefs->mt_enter_prompt) {
    prefs->mt_enter_prompt = mt_enter_prompt;
    set_boolean_pref(PREF_MT_ENTER_PROMPT, mt_enter_prompt);
  }

  pref_factory_bool(PREF_MT_EXIT_RENDER, mt_exit_render, TRUE);

  if (render_prompt != prefs->render_prompt) {
    prefs->render_prompt = render_prompt;
    set_boolean_pref(PREF_RENDER_PROMPT, render_prompt);
  }

  if (mt_pertrack_audio != prefs->mt_pertrack_audio) {
    prefs->mt_pertrack_audio = mt_pertrack_audio;
    set_boolean_pref(PREF_MT_PERTRACK_AUDIO, mt_pertrack_audio);
  }

  if (mt_backaudio != prefs->mt_backaudio) {
    prefs->mt_backaudio = mt_backaudio;
    set_int_pref(PREF_MT_BACKAUDIO, mt_backaudio);
  }

  if (mt_def_width != prefs->mt_def_width) {
    prefs->mt_def_width = mt_def_width;
    set_int_pref(PREF_MT_DEF_WIDTH, mt_def_width);
  }
  if (mt_def_height != prefs->mt_def_height) {
    prefs->mt_def_height = mt_def_height;
    set_int_pref(PREF_MT_DEF_HEIGHT, mt_def_height);
  }
  if (mt_def_fps != prefs->mt_def_fps) {
    prefs->mt_def_fps = mt_def_fps;
    set_double_pref(PREF_MT_DEF_FPS, mt_def_fps);
  }
  if (!mt_enable_audio) mt_def_achans = 0;
  if (mt_def_achans != prefs->mt_def_achans) {
    prefs->mt_def_achans = mt_def_achans;
    set_int_pref(PREF_MT_DEF_ACHANS, mt_def_achans);
  }
  if (mt_def_asamps != prefs->mt_def_asamps) {
    prefs->mt_def_asamps = mt_def_asamps;
    set_int_pref(PREF_MT_DEF_ASAMPS, mt_def_asamps);
  }
  if (mt_def_arate != prefs->mt_def_arate) {
    prefs->mt_def_arate = mt_def_arate;
    set_int_pref(PREF_MT_DEF_ARATE, mt_def_arate);
  }
  if (mt_def_signed_endian != prefs->mt_def_signed_endian) {
    prefs->mt_def_signed_endian = mt_def_signed_endian;
    set_int_pref(PREF_MT_DEF_SIGNED_ENDIAN, mt_def_signed_endian);
  }

  if (mt_undo_buf != prefs->mt_undo_buf) {
    if ((new_undo_buf = (unsigned char *)lives_malloc(mt_undo_buf * 1024 * 1024)) == NULL) {
      do_mt_set_mem_error(mainw->multitrack != NULL, skip_warn);
    } else {
      if (mainw->multitrack != NULL) {
        if (mainw->multitrack->undo_mem != NULL) {
          if (mt_undo_buf < prefs->mt_undo_buf) {
            ssize_t space_needed = mainw->multitrack->undo_buffer_used - (size_t)(mt_undo_buf * 1024 * 1024);
            if (space_needed > 0) make_backup_space(mainw->multitrack, space_needed);
            lives_memcpy(new_undo_buf, mainw->multitrack->undo_mem, mt_undo_buf * 1024 * 1024);
          } else lives_memcpy(new_undo_buf, mainw->multitrack->undo_mem, prefs->mt_undo_buf * 1024 * 1024);
          ulist = mainw->multitrack->undos;
          while (ulist != NULL) {
            ulist->data = new_undo_buf + ((unsigned char *)ulist->data - mainw->multitrack->undo_mem);
            ulist = ulist->next;
          }
          lives_free(mainw->multitrack->undo_mem);
          mainw->multitrack->undo_mem = new_undo_buf;
        } else {
          mainw->multitrack->undo_mem = (unsigned char *)lives_malloc(mt_undo_buf * 1024 * 1024);
          if (mainw->multitrack->undo_mem == NULL) {
            do_mt_set_mem_error(TRUE, skip_warn);
          } else {
            mainw->multitrack->undo_buffer_used = 0;
            mainw->multitrack->undos = NULL;
            mainw->multitrack->undo_offset = 0;
          }
        }
      }
      prefs->mt_undo_buf = mt_undo_buf;
      set_int_pref(PREF_MT_UNDO_BUF, mt_undo_buf);
    }
  }

  if (mt_autoback_always) mt_autoback_time = 0;
  else if (mt_autoback_never) mt_autoback_time = -1;

  pref_factory_int(PREF_MT_AUTO_BACK, mt_autoback_time, TRUE);

  if (max_disp_vtracks != prefs->max_disp_vtracks) {
    prefs->max_disp_vtracks = max_disp_vtracks;
    set_int_pref(PREF_MAX_DISP_VTRACKS, max_disp_vtracks);
    if (mainw->multitrack != NULL) scroll_tracks(mainw->multitrack, mainw->multitrack->top_track, FALSE);
  }

  if (startup_ce && future_prefs->startup_interface != STARTUP_CE) {
    future_prefs->startup_interface = STARTUP_CE;
    set_int_pref(PREF_STARTUP_INTERFACE, STARTUP_CE);
    if ((mainw->multitrack != NULL && mainw->multitrack->event_list != NULL) || mainw->stored_event_list != NULL)
      write_backup_layout_numbering(mainw->multitrack);
  } else if (!startup_ce && future_prefs->startup_interface != STARTUP_MT) {
    future_prefs->startup_interface = STARTUP_MT;
    set_int_pref(PREF_STARTUP_INTERFACE, STARTUP_MT);
    if ((mainw->multitrack != NULL && mainw->multitrack->event_list != NULL) || mainw->stored_event_list != NULL)
      write_backup_layout_numbering(mainw->multitrack);
  }

  mainw->no_context_update = FALSE;

  return needs_restart;
}


void save_future_prefs(void) {
  // save future prefs on exit, if they have changed

  // show_recent is a special case, future prefs has our original value
  if (!prefs->show_recent && future_prefs->show_recent) {
    for (register int i = 1; i < + N_RECENT_FILES; i++)  {
      char *prefname = lives_strdup_printf("%s%d", PREF_RECENT, i);
      set_string_pref(prefname, "");
      lives_free(prefname);
    }
  }

  if ((*future_prefs->workdir)) {
    set_string_pref_priority(PREF_WORKING_DIR, future_prefs->workdir);
    set_string_pref(PREF_WORKING_DIR_OLD, future_prefs->workdir);
  }
  if (prefs->show_tool != future_prefs->show_tool) {
    set_boolean_pref(PREF_SHOW_TOOLBAR, future_prefs->show_tool);
  }
}


void rdet_acodec_changed(LiVESCombo *acodec_combo, livespointer user_data) {
  int listlen = lives_list_length(prefs->acodec_list);
  int idx;
  char *audio_codec = lives_combo_get_active_text(acodec_combo);
  if (!strcmp(audio_codec, mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    lives_free(audio_codec);
    return;
  }

  for (idx = 0; idx < listlen && strcmp((char *)lives_list_nth_data(prefs->acodec_list, idx), audio_codec); idx++);
  lives_free(audio_codec);

  if (idx == listlen) future_prefs->encoder.audio_codec = 0;
  else future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[idx];

  if (prefs->encoder.audio_codec != future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec = future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec < AUDIO_CODEC_UNKNOWN) {
      set_int_pref(PREF_ENCODER_ACODEC, prefs->encoder.audio_codec);
    }
  }
}


void set_acodec_list_from_allowed(_prefsw *prefsw, render_details *rdet) {
  // could be done better, but no time...
  // get settings for current format

  int count = 0, idx;
  boolean is_allowed = FALSE;

  if (prefs->acodec_list != NULL) {
    lives_list_free(prefs->acodec_list);
    prefs->acodec_list = NULL;
  }

  if (future_prefs->encoder.of_allowed_acodecs == 0) {
    prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));
    future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[0] = AUDIO_CODEC_NONE;

    if (prefsw != NULL) {
      lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), 0);
    }
    if (rdet != NULL) {
      lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), 0);
    }
    return;
  }
  for (idx = 0; strlen(anames[idx]); idx++) {
    if (future_prefs->encoder.of_allowed_acodecs & (1 << idx)) {
      if (idx == AUDIO_CODEC_PCM) prefs->acodec_list = lives_list_append(prefs->acodec_list,
            (_("PCM (highest quality; largest files)")));
      else prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(anames[idx]));
      prefs->acodec_list_to_format[count++] = idx;
      if (future_prefs->encoder.audio_codec == idx) is_allowed = TRUE;
    }
  }

  if (prefsw != NULL) {
    lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
  }
  if (rdet != NULL) {
    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
  }
  if (!is_allowed) {
    future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[0];
  }

  for (idx = 0; idx < lives_list_length(prefs->acodec_list); idx++) {
    if (prefs->acodec_list_to_format[idx] == future_prefs->encoder.audio_codec) {
      if (prefsw != NULL) {
        lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), idx);
      }
      if (rdet != NULL) {
        lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), idx);
      }
      break;
    }
  }
}


void after_vpp_changed(LiVESWidget *vpp_combo, livespointer advbutton) {
  char *newvpp = lives_combo_get_active_text(LIVES_COMBO(vpp_combo));
  _vid_playback_plugin *tmpvpp;

  if (!lives_utf8_strcasecmp(newvpp, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), FALSE);
  } else {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), TRUE);

    // will call set_astream_settings
    if ((tmpvpp = open_vid_playback_plugin(newvpp, FALSE)) == NULL) {
      lives_free(newvpp);
      lives_combo_set_active_string(LIVES_COMBO(vpp_combo), mainw->vpp->name);
      return;
    }
    close_vid_playback_plugin(tmpvpp);
  }
  lives_snprintf(future_prefs->vpp_name, 64, "%s", newvpp);
  lives_free(newvpp);

  if (future_prefs->vpp_argv != NULL) {
    register int i;
    for (i = 0; future_prefs->vpp_argv[i] != NULL; lives_free(future_prefs->vpp_argv[i++]));
    lives_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv = NULL;
  }
  future_prefs->vpp_argc = 0;

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
}


static void on_forcesmon_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->spinbutton_gmoni, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->spinbutton_pmoni, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->ce_thumbs, !lives_toggle_button_get_active(tbutton) &&
                             play_monitor != gui_monitor &&
                             play_monitor != 0 && capable->nmonitors > 1);
}


static void pmoni_gmoni_changed(LiVESWidget *sbut, livespointer user_data) {
  _prefsw *prefsw = (_prefsw *)user_data;
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->ce_thumbs, play_monitor != gui_monitor &&
                             play_monitor != 0 && !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon)) &&
                             capable->nmonitors > 1);
}


static void on_mtbackevery_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data != NULL) xprefsw = (_prefsw *)user_data;
  else xprefsw = prefsw;

  lives_widget_set_sensitive(xprefsw->spinbutton_mt_ab_time, lives_toggle_button_get_active(tbutton));

}


#ifdef ENABLE_JACK_TRANSPORT
static void after_jack_client_toggled(LiVESToggleButton *tbutton, livespointer user_data) {

  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start, FALSE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_START) ? TRUE : FALSE);
  }
}


static void after_jack_tb_start_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client, FALSE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) ? TRUE : FALSE);
  }
}
#endif


#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifdef ALSA_MIDI
static void on_alsa_midi_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data != NULL) xprefsw = (_prefsw *)user_data;
  else xprefsw = prefsw;

  lives_widget_set_sensitive(xprefsw->button_midid, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->alsa_midi_dummy, lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->omc_midi_entry, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midicr, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midirpt, !lives_toggle_button_get_active(tbutton));
}
#endif
#endif
#endif


static void on_audp_entry_changed(LiVESWidget *audp_combo, livespointer ptr) {
  char *audp = lives_combo_get_active_text(LIVES_COMBO(audp_combo));

  if (!(*audp) || !strcmp(audp, prefsw->audp_name)) {
    lives_free(audp);
    return;
  }

  if (LIVES_IS_PLAYING) {
    do_aud_during_play_error();
    lives_signal_handler_block(audp_combo, prefsw->audp_entry_func);

    lives_combo_set_active_string(LIVES_COMBO(audp_combo), prefsw->audp_name);

    //lives_widget_queue_draw(audp_entry);
    lives_signal_handler_unblock(audp_combo, prefsw->audp_entry_func);
    lives_free(audp);
    return;
  }

#ifdef RT_AUDIO
  if (!strcmp(audp, AUDIO_PLAYER_JACK) || !strcmp(audp, AUDIO_PLAYER_PULSE_AUDIO)) {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips, TRUE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow, TRUE);
    lives_widget_set_sensitive(prefsw->raudio, TRUE);
    lives_widget_set_sensitive(prefsw->pa_gens, TRUE);
    lives_widget_set_sensitive(prefsw->rextaudio, TRUE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips, FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow, FALSE);
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
    lives_widget_set_sensitive(prefsw->pa_gens, FALSE);
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), FALSE);
  }
  if (!strcmp(audp, AUDIO_PLAYER_JACK)) {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp, TRUE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack), TRUE);
    lives_widget_show(prefsw->jack_int_label);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp, FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon, FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_start_ajack), FALSE);
    lives_widget_hide(prefsw->jack_int_label);
  }
  if (!strcmp(audp, AUDIO_PLAYER_PULSE_AUDIO)) {
    lives_widget_set_sensitive(prefsw->checkbutton_parestart, TRUE);
    lives_widget_set_sensitive(prefsw->audio_command_entry,
                               lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart)));
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_parestart, FALSE);
    lives_widget_set_sensitive(prefsw->audio_command_entry, FALSE);
  }
#endif
  lives_free(prefsw->audp_name);
  prefsw->audp_name = lives_combo_get_active_text(LIVES_COMBO(audp_combo));
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
  lives_free(audp);
}


static void stream_audio_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  // if audio streaming is enabled, check requisites

  if (lives_toggle_button_get_active(togglebutton)) {
    // init vpp, get audio codec, check requisites
    _vid_playback_plugin *tmpvpp;
    uint32_t orig_acodec = AUDIO_CODEC_NONE;

    if (*future_prefs->vpp_name) {
      if ((tmpvpp = open_vid_playback_plugin(future_prefs->vpp_name, FALSE)) == NULL) return;
    } else {
      tmpvpp = mainw->vpp;
      orig_acodec = mainw->vpp->audio_codec;
      get_best_audio(mainw->vpp); // check again because audio player may differ
    }

    if (tmpvpp->audio_codec != AUDIO_CODEC_NONE) {
      // make audiostream plugin name
      size_t rlen;

      char buf[1024];
      char *com;

      char *astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, AUDIO_STREAMER_NAME, NULL);

      com = lives_strdup_printf("\"%s\" check %d", astreamer, tmpvpp->audio_codec);
      lives_free(astreamer);

      rlen = lives_popen(com, TRUE, buf, 1024);
      lives_free(com);
      if (rlen > 0) {
        lives_toggle_button_set_active(togglebutton, FALSE);
      }
    }

    if (tmpvpp != NULL) {
      if (tmpvpp != mainw->vpp) {
        // close the temp current vpp
        close_vid_playback_plugin(tmpvpp);
      } else {
        // restore current codec
        mainw->vpp->audio_codec = orig_acodec;
      }
    }
  }
}


void prefsw_set_astream_settings(_vid_playback_plugin *vpp, _prefsw *prefsw) {
  if (vpp != NULL && (vpp->audio_codec != AUDIO_CODEC_NONE || vpp->init_audio != NULL)) {
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio, TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio, FALSE);
  }
}


void prefsw_set_rec_after_settings(_vid_playback_plugin *vpp, _prefsw *prefsw) {
  if (vpp != NULL && (vpp->capabilities & VPP_CAN_RETURN)) {
    lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb, TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb, FALSE);
  }
}


/*
   Initialize preferences dialog list
*/
static void pref_init_list(LiVESWidget *list) {
  LiVESCellRenderer *renderer, *pixbufRenderer;
  LiVESTreeViewColumn *column1, *column2;
  LiVESListStore *store;

  renderer = lives_cell_renderer_text_new();
  pixbufRenderer = lives_cell_renderer_pixbuf_new();

  column1 = lives_tree_view_column_new_with_attributes("List Icons", pixbufRenderer, LIVES_TREE_VIEW_COLUMN_PIXBUF, LIST_ICON,
            NULL);
  column2 = lives_tree_view_column_new_with_attributes("List Items", renderer, LIVES_TREE_VIEW_COLUMN_TEXT, LIST_ITEM, NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column1);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column2);
  lives_tree_view_column_set_sizing(column2, LIVES_TREE_VIEW_COLUMN_FIXED);
  lives_tree_view_column_set_fixed_width(column2, 150. * widget_opts.scale);

  store = lives_list_store_new(N_COLUMNS, LIVES_COL_TYPE_PIXBUF, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_UINT);

  lives_tree_view_set_model(LIVES_TREE_VIEW(list), LIVES_TREE_MODEL(store));
}


/*
   Adds entry to preferences dialog list
*/
static void prefs_add_to_list(LiVESWidget *list, LiVESPixbuf *pix, const char *str, uint32_t idx) {
  LiVESListStore *store;
  LiVESTreeIter iter;

  char *tmp = lives_strdup_printf("\n  %s\n", str);

  store = LIVES_LIST_STORE(lives_tree_view_get_model(LIVES_TREE_VIEW(list)));

  lives_list_store_insert(store, &iter, idx);
  lives_list_store_set(store, &iter, LIST_ICON, pix, LIST_ITEM, tmp, LIST_NUM, idx, -1);
  lives_free(tmp);
}


/*
   Callback function called when preferences list row changed
*/
void on_prefDomainChanged(LiVESTreeSelection *widget, livespointer xprefsw) {
  LiVESTreeIter iter;
  LiVESTreeModel *model;

  register int i;
  char *name;
  _prefsw *prefsw = (_prefsw *)xprefsw;

  for (i = 0; i < 2; i++) {
    // for some reason gtk+ needs us to do this twice..
    if (lives_tree_selection_get_selected(widget, &model, &iter)) {
      lives_tree_model_get(model, &iter, LIST_NUM, &prefs_current_page, LIST_ITEM, &name, -1);

      // Hide currently shown widget
      if (prefsw->right_shown) {
        lives_widget_hide(prefsw->right_shown);
      }

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

#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK) {
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
        if (nmons <= 1) {
          lives_widget_hide(prefsw->forcesmon_hbox);
#if !LIVES_HAS_GRID_WIDGET
          lives_widget_hide(lives_widget_get_parent(prefsw->ce_thumbs));
#endif
        }
        prefs_current_page = LIST_ENTRY_GUI;
      }
    }
  }
  lives_label_set_text(LIVES_LABEL(prefsw->tlabel), name);
  lives_widget_queue_draw(prefsw->prefs_dialog);
}


/*
   Function makes apply button sensitive
*/
void apply_button_set_enabled(LiVESWidget *widget, livespointer func_data) {
  if (prefsw->ignore_apply) return;
  lives_button_grab_default_special(prefsw->applybutton); // need to do this first or the button doesnt get its colour
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->revertbutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), FALSE);
}


static void spinbutton_crit_ds_value_changed(LiVESSpinButton *crit_ds, livespointer user_data) {
  double myval = lives_spin_button_get_value(crit_ds);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds), myval, DS_WARN_CRIT_MAX);
  apply_button_set_enabled(NULL, NULL);
}


static void theme_widgets_set_sensitive(LiVESCombo *combo, livespointer xprefsw) {
  _prefsw *prefsw = (_prefsw *)xprefsw;
  char *theme = lives_combo_get_active_text(combo);
  boolean theme_set = TRUE;
  if (!lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) theme_set = FALSE;
  lives_free(theme);
  lives_widget_set_sensitive(prefsw->cbutton_fxcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_audcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_vidcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_evbox, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_ceunsel, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_cesel, theme_set);
  lives_widget_set_sensitive(prefsw->fb_filebutton, theme_set);
  lives_widget_set_sensitive(prefsw->sepimg_entry, theme_set);
  lives_widget_set_sensitive(prefsw->se_filebutton, theme_set);
  lives_widget_set_sensitive(prefsw->frameblank_entry, theme_set);
  lives_widget_set_sensitive(prefsw->theme_style4, theme_set);
  if (prefsw->theme_style2 != NULL) {
    lives_widget_set_sensitive(prefsw->theme_style2, theme_set);
  }
  lives_widget_set_sensitive(prefsw->theme_style3, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_back, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_fore, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mab, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mabf, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infot, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infob, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infot, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mtmark, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tlreg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tcfg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tcbg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_fsur, theme_set);
}


static boolean check_txtsize(LiVESWidget *combo) {
  LiVESList *list = get_textsizes_list();
  char *msgtextsize = lives_combo_get_active_text(LIVES_COMBO(combo));
  int idx = lives_list_strcmp_index(list, (livesconstpointer)msgtextsize, TRUE);
  lives_list_free_all(&list);
  lives_free(msgtextsize);

  if (idx > mainw->max_textsize) {
    show_warn_image(combo, _("Text size may be too large for the screen size"));
    return TRUE;
  }
  show_warn_image(combo, NULL);
  return FALSE;
}


/*
   Function creates preferences dialog
*/
_prefsw *create_prefs_dialog(LiVESWidget *saved_dialog) {
  LiVESWidget *dialog_vbox_main;
  LiVESWidget *dialog_table;
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

  LiVESWidget *ins_resample;
  LiVESWidget *hbox;

  LiVESWidget *layout;

  LiVESWidget *hbox1;
  LiVESWidget *vbox;

  LiVESWidget *dirbutton;

  LiVESWidget *pp_combo;
  LiVESWidget *png;
  LiVESWidget *frame;
  LiVESWidget *mt_enter_defs;

  LiVESWidget *advbutton;

  LiVESWidget *sp_red, *sp_green, *sp_blue;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  LiVESWidget *raw_midi_button;
#endif
#endif

  LiVESWidget *label;

  // radio button groups
  //LiVESSList *rb_group = NULL;
  LiVESSList *jpeg_png = NULL;
  LiVESSList *mt_enter_prompt = NULL;
  LiVESSList *rb_group2 = NULL;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  LiVESSList *alsa_midi_group = NULL;
  LiVESList *mchanlist = NULL;
#endif
#endif

  LiVESSList *autoback_group = NULL;
  LiVESSList *st_interface_group = NULL;

  LiVESSList *asrc_group = NULL;

  // drop down lists
  LiVESList *themes = NULL;
  LiVESList *ofmt = NULL;
  LiVESList *ofmt_all = NULL;
  LiVESList *audp = NULL;
  LiVESList *encoders = NULL;
  LiVESList *vid_playback_plugins = NULL;
  LiVESList *textsizes_list;

  lives_colRGBA64_t rgba;

  char **array;
  char *tmp, *tmp2, *tmp3;
  char *theme;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  char *midichan;
#endif
#endif

  boolean pfsm;
  boolean has_ap_rec = FALSE;

  int woph;

  register int i;

  // Allocate memory for the preferences structure
  _prefsw *prefsw = (_prefsw *)(lives_malloc(sizeof(_prefsw)));
  prefsw->right_shown = NULL;
  mainw->prefs_need_restart = FALSE;

  prefsw->accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  woph = widget_opts.packing_height;

  if (saved_dialog == NULL) {
    // Create new modal dialog window and set some attributes
    prefsw->prefs_dialog = lives_standard_dialog_new(_("Preferences"), FALSE, PREFWIN_WIDTH, PREFWIN_HEIGHT);
    lives_window_add_accel_group(LIVES_WINDOW(prefsw->prefs_dialog), prefsw->accel_group);
    lives_window_set_default_size(LIVES_WINDOW(prefsw->prefs_dialog), PREFWIN_WIDTH, PREFWIN_HEIGHT);
  } else prefsw->prefs_dialog = saved_dialog;

  prefsw->ignore_apply = FALSE;
  //prefs->cb_is_switch = TRUE; // TODO: intervept TOGGLED handler

  // Get dialog's vbox and show it
  dialog_vbox_main = lives_dialog_get_content_area(LIVES_DIALOG(prefsw->prefs_dialog));
  lives_widget_show(dialog_vbox_main);

  // Create dialog horizontal panels
  prefsw->dialog_hpaned = lives_hpaned_new();
  lives_widget_show(prefsw->dialog_hpaned);

  // Create dialog table for the right panel controls placement
  dialog_table = lives_vbox_new(FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_table), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  prefsw->tlabel = lives_standard_label_new(NULL);
  lives_widget_apply_theme2(prefsw->tlabel, LIVES_WIDGET_STATE_NORMAL, TRUE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), prefsw->tlabel, TRUE, TRUE, 0);

  lives_widget_show_all(dialog_table);
  lives_widget_set_no_show_all(dialog_table, TRUE);

  // Create preferences list with invisible headers
  prefsw->prefs_list = lives_tree_view_new();

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme(prefsw->prefs_list, LIVES_WIDGET_STATE_SELECTED);
  }

  lives_tree_view_set_headers_visible(LIVES_TREE_VIEW(prefsw->prefs_list), FALSE);

  // Place panels into main vbox
  lives_box_pack_start(LIVES_BOX(dialog_vbox_main), prefsw->dialog_hpaned, TRUE, TRUE, 0);

  // Place list on the left panel
  pref_init_list(prefsw->prefs_list);

  list_scroll = lives_scrolled_window_new(lives_tree_view_get_hadjustment(LIVES_TREE_VIEW(prefsw->prefs_list)), NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(list_scroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);
  lives_container_add(LIVES_CONTAINER(list_scroll), prefsw->prefs_list);

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme3(prefsw->prefs_list, LIVES_WIDGET_STATE_NORMAL);
  }

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme(dialog_table, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(prefsw->dialog_hpaned, LIVES_WIDGET_STATE_NORMAL);
  }

  lives_paned_pack(1, LIVES_PANED(prefsw->dialog_hpaned), list_scroll, TRUE, FALSE);
  // Place table on the right panel

  lives_paned_pack(2, LIVES_PANED(prefsw->dialog_hpaned), dialog_table, TRUE, FALSE);

#if GTK_CHECK_VERSION(3, 0, 0)
  lives_paned_set_position(LIVES_PANED(prefsw->dialog_hpaned), PREFS_PANED_POS);
#else
  lives_paned_set_position(LIVES_PANED(prefsw->dialog_hpaned), PREFS_PANED_POS / 2);
#endif
  // -------------------,
  // gui controls       |
  // -------------------'
  prefsw->vbox_right_gui = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_gui = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_gui);
  prefsw->right_shown = prefsw->vbox_right_gui;

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->fs_max_check =
    lives_standard_check_button_new(_("Open file selection maximised"), prefs->fileselmax, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->recent_check =
    lives_standard_check_button_new(_("Show recent files in the File menu"), prefs->show_recent, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->stop_screensaver_check =
    lives_standard_check_button_new(_("Stop screensaver on playback    "), prefs->stop_screensaver, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->open_maximised_check = lives_standard_check_button_new(_("Open main window maximised"), prefs->open_maximised,
                                 LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->show_tool =
    lives_standard_check_button_new(_("Show toolbar when background is blanked"), future_prefs->show_tool, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->mouse_scroll =
    lives_standard_check_button_new(_("Allow mouse wheel to switch clips"), prefs->mouse_scroll_clips, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_ce_maxspect =
    lives_standard_check_button_new(_("Shrink previews to fit in interface"), prefs->ce_maxspect, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_button_icons =
    lives_standard_check_button_new(_("Show icons in buttons"), widget_opts.show_button_images, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_hfbwnp =
    lives_standard_check_button_new(_("Hide framebar when not playing"), prefs->hfbwnp, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_show_asrc =
    lives_standard_check_button_new(_("Show audio source in toolbar"), prefs->show_asrc, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

#if GTK_CHECK_VERSION(2, 12, 0)
  prefsw->checkbutton_show_ttips =
    lives_standard_check_button_new(_("Show tooltips"), prefs->show_tooltips, LIVES_BOX(hbox), NULL);
#else
  prefsw->checkbutton_show_ttips = NULL;
#endif

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Startup mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rb_startup_ce = lives_standard_radio_button_new(_("_Clip editor"), &st_interface_group, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rb_startup_mt = lives_standard_radio_button_new(_("_Multitrack mode"), &st_interface_group, LIVES_BOX(hbox), NULL);

  if (future_prefs->startup_interface == STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_mt), TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce), TRUE);
  }

  add_fill_to_box(LIVES_BOX(hbox));

  //
  // multihead support (inside Gui part)
  //

  pfsm = prefs->force_single_monitor;
  prefs->force_single_monitor = FALSE;
  get_monitors(FALSE);
  nmons = capable->nmonitors;

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  label = lives_standard_label_new(_("Multi-head support"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), label, FALSE, FALSE, widget_opts.packing_height);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_gmoni = lives_standard_spin_button_new(_("Monitor number for LiVES interface"), prefs->gui_monitor, 1, nmons,
                             1., 1., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_pmoni =
    lives_standard_spin_button_new(_("Monitor number for playback"),
                                   prefs->play_monitor, 0,
                                   nmons == 1 ? 0 : nmons, 1., 1., 0, LIVES_BOX(hbox),
                                   (tmp = lives_strdup(_("#A setting of 0 means use all available "
                                          "monitors (only works with some playback "
                                          "plugins)."))));
  lives_free(tmp);
  prefs->force_single_monitor = pfsm;

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->forcesmon_hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->forcesmon = lives_standard_check_button_new((tmp = (_("Force single monitor"))),
                      prefs->force_single_monitor,
                      LIVES_BOX(prefsw->forcesmon_hbox),
                      (tmp2 = (_("Ignore all except the first monitor."))));
  lives_free(tmp);
  lives_free(tmp2);

  if (nmons <= 1) {
    lives_widget_set_sensitive(prefsw->spinbutton_gmoni, FALSE);
    lives_widget_set_sensitive(prefsw->spinbutton_pmoni, FALSE);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->forcesmon), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_forcesmon_toggled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->ce_thumbs = lives_standard_check_button_new(_("Show clip thumbnails during playback"), prefs->ce_thumb_mode,
                      LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->ce_thumbs, prefs->play_monitor != prefs->gui_monitor &&
                             prefs->play_monitor != 0 && !prefs->force_single_monitor &&
                             capable->nmonitors > 1);

  pmoni_gmoni_changed(NULL, (livespointer)prefsw);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));
  add_fill_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->nmessages_spin = lives_standard_spin_button_new(_("Number of _Info Messages to Buffer"),
                           ABS(prefs->max_messages), 0., 100000., 1., 1., 0,
                           LIVES_BOX(hbox), NULL);
  ACTIVE(nmessages_spin, VALUE_CHANGED);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->msgs_unlimited = lives_standard_check_button_new(_("_Unlimited"),
                           prefs->max_messages < 0, LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_unlimited), prefsw->nmessages_spin, TRUE);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->nmessages_spin), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(widget_inact_toggle), prefsw->msgs_unlimited);
  ACTIVE(msgs_unlimited, TOGGLED);

  textsizes_list = get_textsizes_list();

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->msg_textsize_combo = lives_standard_combo_new(_("Message Area _Font Size"), textsizes_list,
                               LIVES_BOX(hbox), NULL);

  lives_combo_set_active_index(LIVES_COMBO(prefsw->msg_textsize_combo), prefs->msg_textsize);

  check_txtsize(prefsw->msg_textsize_combo);
  lives_signal_connect_after(LIVES_WIDGET_OBJECT(prefsw->msg_textsize_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(check_txtsize), NULL);

  lives_list_free_all(&textsizes_list);

  ACTIVE(msg_textsize_combo, CHANGED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->msgs_pbdis = lives_standard_check_button_new(_("_Disable message output during playback"),
                       prefs->msgs_pbdis, LIVES_BOX(hbox), NULL);
  ACTIVE(msgs_pbdis, TOGGLED);

  pixbuf_gui = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_GUI, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_gui, _("GUI"), LIST_ENTRY_GUI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_gui);

  // -----------------------,
  // multitrack controls    |
  // -----------------------'

  prefsw->vbox_right_multitrack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_multitrack), widget_opts.border_width * 2);

  prefsw->scrollw_right_multitrack = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_multitrack);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When entering Multitrack mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(hbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->mt_enter_prompt = lives_standard_radio_button_new(_("_Prompt me for width, height, fps and audio settings"),
                            &mt_enter_prompt, LIVES_BOX(hbox), NULL);

  mt_enter_defs = lives_standard_radio_button_new(_("_Always use the following values:"),
                  &mt_enter_prompt, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt_enter_defs), !prefs->mt_enter_prompt);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_render_prompt = lives_standard_check_button_new(_("Use these same _values for rendering a new clip"),
                                      !prefs->render_prompt, LIVES_BOX(hbox), NULL);

  frame = add_video_options(&prefsw->spinbutton_mt_def_width, mainw->multitrack == NULL ? prefs->mt_def_width : cfile->hsize,
                            &prefsw->spinbutton_mt_def_height,
                            mainw->multitrack == NULL ? prefs->mt_def_height : cfile->vsize, &prefsw->spinbutton_mt_def_fps,
                            mainw->multitrack == NULL ? prefs->mt_def_fps : cfile->fps, NULL, 0, FALSE, NULL);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), frame, FALSE, FALSE, widget_opts.packing_height);

  hbox = add_audio_options(&prefsw->backaudio_checkbutton, &prefsw->pertrack_checkbutton);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton), prefs->mt_backaudio > 0);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton), prefs->mt_pertrack_audio);

  // must be done after creating check buttons
  resaudw = create_resaudw(4, NULL, prefsw->vbox_right_multitrack);

  // must be done after resaudw
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_widget_set_sensitive(prefsw->backaudio_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  lives_widget_set_sensitive(prefsw->pertrack_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_multitrack));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_multitrack));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_mt_undo_buf = lives_standard_spin_button_new(_("_Undo buffer size (MB)"),
                                   prefs->mt_undo_buf, 0., ONE_MILLION, 1., 1., 0,
                                   LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_mt_exit_render = lives_standard_check_button_new(_("_Exit multitrack mode after rendering"),
                                       prefs->mt_exit_render, LIVES_BOX(hbox),
                                       NULL);


  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_("Auto backup layouts"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, widget_opts.packing_width * 2);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  int wopw = widget_opts.packing_width;
  widget_opts.packing_width >>= 1;
  prefsw->mt_autoback_every = lives_standard_radio_button_new(_("_Every"), &autoback_group, LIVES_BOX(hbox), NULL);

  widget_opts.swap_label = TRUE;
  prefsw->spinbutton_mt_ab_time = lives_standard_spin_button_new(_("seconds"), 120., 10., 1800., 1., 10., 0, LIVES_BOX(hbox),
                                  NULL);
  widget_opts.swap_label = FALSE;
  widget_opts.packing_width = wopw;

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->mt_autoback_always = lives_standard_radio_button_new(_("After every _change"), &autoback_group, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->mt_autoback_never = lives_standard_radio_button_new(_("_Never"), &autoback_group, LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_every), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_mtbackevery_toggled),
                            prefsw);

  if (prefs->mt_auto_back == 0) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always), TRUE);
  } else if (prefs->mt_auto_back == -1) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never), TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_every), TRUE);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time), prefs->mt_auto_back);
  }

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_max_disp_vtracks = lives_standard_spin_button_new(_("Maximum number of visible tracks"),
                                        prefs->max_disp_vtracks,
                                        5., 15.,
                                        1., 1., 0, LIVES_BOX(hbox), NULL);

  pixbuf_multitrack = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MULTITRACK, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_multitrack, _("Multitrack/Render"), LIST_ENTRY_MULTITRACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_multitrack);

  // ---------------,
  // decoding       |
  // ---------------'

  prefsw->vbox_right_decoding = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_decoding), widget_opts.border_width * 2);

  prefsw->scrollw_right_decoding = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_decoding);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_instant_open = lives_standard_check_button_new((tmp = (_("Use instant opening when possible"))),
                                     prefs->instant_open,
                                     LIVES_BOX(hbox),
                                     (tmp2 = (_("Enable instant opening of some file types using decoder plugins"))));

  lives_free(tmp);
  lives_free(tmp2);

  // advanced instant opening
  advbutton = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES, _("_Advanced"),
              DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT,
              LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_decplug_advanced_clicked),
                       NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open), advbutton, FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->video_open_entry = lives_standard_entry_new(_("Video open command (fallback)"),
                             prefs->video_open_command, -1, PATH_MAX * 2,
                             LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new_with_tooltips(_("Fallback image format"), LIVES_BOX(hbox),
          _("The image format to be used when opening clips\n"
            "for which there is no instant decoder candidate."));

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->jpeg = lives_standard_radio_button_new(_("_jpeg"), &jpeg_png, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  png = lives_standard_radio_button_new(_("_png"), &jpeg_png, LIVES_BOX(hbox), NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(png), !strcmp(prefs->image_ext, LIVES_FILE_EXT_PNG));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

  label = lives_standard_label_new(_("(Check Help/Troubleshoot to see which image formats are supported)"));
  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, widget_opts.packing_width);

  if (prefs->ocp == -1) prefs->ocp = get_int_pref(PREF_OPEN_COMPRESSION_PERCENT);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Open/render compression"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  widget_opts.swap_label = TRUE;
  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
  prefsw->spinbutton_ocp = lives_standard_spin_button_new(("  %  "), prefs->ocp, 0., 100., 1., 5., 0,
                           LIVES_BOX(hbox), NULL);
  widget_opts.swap_label = FALSE;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  add_fill_to_box(LIVES_BOX(hbox));

  label = lives_standard_label_new(_("( lower = slower, larger files; for jpeg, higher quality )"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));
  add_fill_to_box(LIVES_BOX(prefsw->vbox_right_decoding));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_auto_deint = lives_standard_check_button_new((tmp = lives_strdup(
                                     _("Enable automatic deinterlacing when possible"))),
                                   prefs->auto_deint,
                                   LIVES_BOX(hbox),
                                   (tmp2 = (_("Automatically deinterlace frames when a plugin suggests it"))));
  lives_free(tmp);
  lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_auto_trim = lives_standard_check_button_new((tmp = lives_strdup(
                                    _("Automatic trimming / padding of audio when possible"))),
                                  prefs->auto_trim_audio,
                                  LIVES_BOX(hbox),
                                  (tmp2 = (_("Automatically trim or pad audio when a plugin suggests it"))));
  lives_free(tmp);
  lives_free(tmp2);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_nobord = lives_standard_check_button_new((tmp = (_("Ignore blank borders when possible"))),
                               prefs->auto_nobord, LIVES_BOX(hbox),
                               (tmp2 = (_("Clip any blank borders from frames where possible"))));
  lives_free(tmp);
  lives_free(tmp2);


  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));
  add_fill_to_box(LIVES_BOX(prefsw->vbox_right_decoding));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_concat_images = lives_standard_check_button_new(
                                        _("When opening multiple files, concatenate images into one clip"),
                                        prefs->concat_images, LIVES_BOX(hbox), NULL);

  pixbuf_decoding = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_DECODING, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_decoding, _("Decoding"), LIST_ENTRY_DECODING);

  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_decoding);

  // ---------------,
  // playback       |
  // ---------------'

  prefsw->vbox_right_playback = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_playback = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_playback);

  frame = lives_standard_frame_new(_("VIDEO"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, FALSE, FALSE, 0);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->pbq_adaptive =
    lives_standard_check_button_new(
      (tmp = lives_strdup_printf(_("_Enable adaptive quality (%s)"),
                                 mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED])),
      prefs->pbq_adaptive, LIVES_BOX(hbox),
      (tmp2 = (_(
                 "Allows the quality to be adjusted automatically, prioritising smooth playback"))));
  lives_free(tmp);
  lives_free(tmp2);

  prefsw->pbq_list = NULL;
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("Low - can improve performance on slower machines")));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("Normal - recommended for most users")));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("High - can improve quality on very fast machines")));

  widget_opts.expand = LIVES_EXPAND_EXTRA;
  prefsw->pbq_combo = lives_standard_combo_new((tmp = (_("Preview _quality"))), prefsw->pbq_list, LIVES_BOX(hbox),
                      (tmp2 = (_("The preview quality for video playback - affects resizing"))));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_free(tmp);
  lives_free(tmp2);

  switch (future_prefs->pb_quality) {
  case PB_QUALITY_HIGH:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 2);
    break;
  case PB_QUALITY_MED:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 1);
  }

  prefsw->checkbutton_show_stats = lives_standard_check_button_new(_("_Show FPS statistics"), prefs->show_player_stats,
                                   LIVES_BOX(vbox),
                                   NULL);

  add_hsep_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Letterbox by default:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  prefsw->checkbutton_lb = lives_standard_check_button_new(_("In Clip Edit Mode"),
                           prefs->letterbox, LIVES_BOX(hbox), NULL);

  prefsw->checkbutton_lbmt = lives_standard_check_button_new(_("In Multitrack Mode"),
                             prefs->letterbox_mt, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Monitor gamma setting:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  prefsw->checkbutton_srgb = lives_standard_check_button_new(_("sRGB"),
                             prefs->gamma_srgb, LIVES_BOX(hbox), NULL);

  prefsw->spinbutton_gamma = lives_standard_spin_button_new(_("Inverse power law"),
                             prefs->screen_gamma, 1.2, 3.0, .01, .1, 2,
                             LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_srgb), prefsw->spinbutton_gamma, TRUE);

  add_hsep_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  vid_playback_plugins = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-"DLL_NAME);
  vid_playback_plugins = lives_list_prepend(vid_playback_plugins,
                         lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  widget_opts.expand = LIVES_EXPAND_EXTRA;
  pp_combo = lives_standard_combo_new(_("_Plugin"), vid_playback_plugins, LIVES_BOX(hbox), NULL);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  advbutton = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES, _("_Advanced"),
              DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT,
              LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vpp_advanced_clicked),
                       LIVES_INT_TO_POINTER(LIVES_INTENTION_PLAY));

  if (mainw->vpp != NULL) {
    lives_combo_set_active_string(LIVES_COMBO(pp_combo), mainw->vpp->name);
  } else {
    lives_combo_set_active_index(LIVES_COMBO(pp_combo), 0);
    lives_widget_set_sensitive(advbutton, FALSE);
  }
  lives_list_free_all(&vid_playback_plugins);

  lives_signal_connect_after(LIVES_WIDGET_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_vpp_changed), (livespointer) advbutton);

  prefsw->checkbutton_stream_audio =
    lives_standard_check_button_new((tmp = (_("Stream audio"))),
                                    prefs->stream_audio_out, LIVES_BOX(vbox),
                                    (tmp2 = lives_strdup
                                        (_("Stream audio to playback plugin"))));
  lives_free(tmp);
  lives_free(tmp2);

  prefsw_set_astream_settings(mainw->vpp, prefsw);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_stream_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(stream_audio_toggled), NULL);

  prefsw->checkbutton_rec_after_pb =
    lives_standard_check_button_new((tmp = (_("Record player output"))),
                                    (prefs->rec_opts & REC_AFTER_PB), LIVES_BOX(vbox),
                                    (tmp2 = lives_strdup
                                        (_("Record output from player instead of input to player"))));
  lives_free(tmp);
  lives_free(tmp2);

  prefsw_set_rec_after_settings(mainw->vpp, prefsw);

  //-

  frame = lives_standard_frame_new(_("AUDIO"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);

  audp = lives_list_append(audp, lives_strdup_printf("%s", mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

#ifdef HAVE_PULSE_AUDIO
  audp = lives_list_append(audp, lives_strdup_printf("%s (%s)", AUDIO_PLAYER_PULSE_AUDIO,
                           mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  has_ap_rec = TRUE;
#endif

#ifdef ENABLE_JACK
  if (!has_ap_rec) audp = lives_list_append(audp, lives_strdup_printf("%s (%s)", AUDIO_PLAYER_JACK,
                            mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  else audp = lives_list_append(audp, lives_strdup_printf(AUDIO_PLAYER_JACK));
  has_ap_rec = TRUE;
#endif

  if (capable->has_sox_play) {
    if (has_ap_rec) audp = lives_list_append(audp, lives_strdup(AUDIO_PLAYER_SOX));
    else audp = lives_list_append(audp, lives_strdup_printf("%s (%s)", AUDIO_PLAYER_SOX,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  }

  widget_opts.expand = LIVES_EXPAND_EXTRA;
  prefsw->audp_combo = lives_standard_combo_new(_("_Player"), audp, LIVES_BOX(vbox), NULL);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  has_ap_rec = FALSE;

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->jack_int_label = lives_standard_label_new(_("(See also the Jack Integration tab for jack startup options)"));
  lives_box_pack_start(LIVES_BOX(hbox), prefsw->jack_int_label, FALSE, FALSE, widget_opts.packing_width);
  lives_widget_set_no_show_all(prefsw->jack_int_label, TRUE);

  prefsw->audp_name = NULL;

  if (prefs->audio_player == AUD_PLAYER_NONE) {
    prefsw->audp_name = lives_strdup_printf("%s", mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  }

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    prefsw->audp_name = lives_strdup_printf("%s (%s)", AUDIO_PLAYER_PULSE_AUDIO,
                                            mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
  }
  has_ap_rec = TRUE;
#endif

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    if (!has_ap_rec)
      prefsw->audp_name = lives_strdup_printf("%s (%s)", AUDIO_PLAYER_JACK,
                                              mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name = lives_strdup_printf(AUDIO_PLAYER_JACK);
  }
  has_ap_rec = TRUE;
#endif

  if (prefs->audio_player == AUD_PLAYER_SOX) {
    if (!has_ap_rec) prefsw->audp_name = lives_strdup_printf("%s (%s)", AUDIO_PLAYER_SOX,
                                           mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name = lives_strdup_printf(AUDIO_PLAYER_SOX);
  }

  if (prefsw->audp_name != NULL)
    lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->audp_name);
  prefsw->orig_audp_name = lives_strdup(prefsw->audp_name);

  //---

#ifdef HAVE_PULSE_AUDIO
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_parestart = lives_standard_check_button_new((tmp = (_("Restart pulseaudio on LiVES startup"))),
                                  prefs->pa_restart, LIVES_BOX(hbox),
                                  (tmp2 = (_("Recommended, but may interfere with other running "
                                          "audio applications"))));
  lives_free(tmp);
  lives_free(tmp2);
  ACTIVE(checkbutton_parestart, TOGGLED);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->audio_command_entry = lives_standard_entry_new(_("Pulseaudio restart command"),
                                prefs->pa_start_opts, -1, PATH_MAX * 2,
                                LIVES_BOX(hbox), NULL);
  ACTIVE(audio_command_entry, CHANGED);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart), prefsw->audio_command_entry, FALSE);
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_afollow = lives_standard_check_button_new(_("Audio follows video _rate/direction"),
                                (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) ? TRUE : FALSE, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->checkbutton_aclips = lives_standard_check_button_new(_("Audio follows _clip switches"),
                               (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) ? TRUE : FALSE, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(vbox));
  add_hsep_to_box(LIVES_BOX(vbox));
  add_fill_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
  label = lives_standard_label_new(_("Audio Source (clip editor only):"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(vbox));

  prefsw->rintaudio = lives_standard_radio_button_new(_("_Internal"), &asrc_group, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rextaudio = lives_standard_radio_button_new(_("_External [monitor]"),
                      &asrc_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), future_prefs->audio_src == AUDIO_SRC_EXT);
  add_fill_to_box(LIVES_BOX(hbox));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->checkbutton_aclips, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->checkbutton_afollow, TRUE);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
  }

  pixbuf_playback = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_PLAYBACK, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_playback, _("Playback"), LIST_ENTRY_PLAYBACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_playback);

  lives_widget_hide(prefsw->jack_int_label);

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    lives_widget_show(prefsw->jack_int_label);
  }
#endif

  // ---------------,
  // recording      |
  // ---------------'

  prefsw->vbox_right_recording = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_recording), widget_opts.border_width * 2);

  prefsw->scrollw_right_recording = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_recording);

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->rdesk_audio = lives_standard_check_button_new(
                          _("Record audio when capturing an e_xternal window\n (requires jack or pulseaudio)"),
                          prefs->rec_desktop_audio, LIVES_BOX(hbox), NULL);

#ifndef RT_AUDIO
  lives_widget_set_sensitive(prefsw->rdesk_audio, FALSE);
#endif

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("What to record when 'r' is pressed"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_recording));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rframes = lives_standard_check_button_new(_("_Frame changes"), (prefs->rec_opts & REC_FRAMES), LIVES_BOX(hbox), NULL);

  if (prefs->rec_opts & REC_FPS || prefs->rec_opts & REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rframes, FALSE); // we must record these if recording fps changes or clip switches
  }
  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rframes, FALSE);
  }

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rfps = lives_standard_check_button_new(_("F_PS changes"), (prefs->rec_opts & REC_FPS), LIVES_BOX(hbox), NULL);

  if (prefs->rec_opts & REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rfps, FALSE); // we must record these if recording clip switches
  }

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rfps, FALSE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->reffects = lives_standard_check_button_new(_("_Real time effects"), (prefs->rec_opts & REC_EFFECTS), LIVES_BOX(hbox),
                     NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->reffects, FALSE);
  }

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rclips = lives_standard_check_button_new(_("_Clip switches"), (prefs->rec_opts & REC_CLIPS), LIVES_BOX(hbox), NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rclips, FALSE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->raudio = lives_standard_check_button_new(_("_Audio (requires jack or pulseaudio player)"),
                   (prefs->rec_opts & REC_AUDIO), LIVES_BOX(hbox), NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
  }

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height * 4);

  prefsw->spinbutton_rec_gb = lives_standard_spin_button_new(_("Pause recording if free disk space falls below"),
                              prefs->rec_stop_gb, 0., 1024., 1., 10., 0,
                              LIVES_BOX(hbox), NULL);

  // TRANSLATORS: gigabytes
  label = lives_standard_label_new(_("GB"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height * 4);

  label = lives_standard_label_new(_("External Audio Source"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height * 2);
  prefsw->spinbutton_ext_aud_thresh = lives_standard_spin_button_new(
                                        _("Delay recording playback start until external audio volume reaches "),
                                        prefs->ahold_threshold * 100., 0., 100., 1., 10., 0,
                                        LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new("%");
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));

  pixbuf_recording = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_RECORD, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_recording, _("Recording"), LIST_ENTRY_RECORDING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_recording);

  // ---------------,
  // encoding       |
  // ---------------'

  prefsw->vbox_right_encoding = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_encoding = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_encoding);

  widget_opts.packing_height <<= 2;
  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_encoding));

  lives_layout_add_label(LIVES_LAYOUT(layout),
                         _("You can also change these values when encoding a clip"), FALSE);

  if (capable->has_encoder_plugins) {
    // scan for encoder plugins
    encoders = get_plugin_list(PLUGIN_ENCODERS, TRUE, NULL, NULL);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  label = lives_standard_label_new(_("Encoder"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->encoder_combo = lives_standard_combo_new(NULL, encoders,
                          LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  if (encoders != NULL) {
    lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
    lives_list_free_all(&encoders);
  }

  if (capable->has_encoder_plugins) {
    // reqest formats from the encoder plugin
    if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats")) != NULL) {
      for (i = 0; i < lives_list_length(ofmt_all); i++) {
        if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
          array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);
          if (!strcmp(array[0], prefs->encoder.of_name)) {
            prefs->encoder.of_allowed_acodecs = atoi(array[2]);
            lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", array[3]);
          }
          ofmt = lives_list_append(ofmt, lives_strdup(array[1]));
          lives_strfreev(array);
        }
      }
      lives_memcpy(&future_prefs->encoder, &prefs->encoder, sizeof(_encoder));
    } else {
      do_plugin_encoder_error(prefs->encoder.name);
      future_prefs->encoder.of_allowed_acodecs = 0;
    }

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    label = lives_standard_label_new(_("Output format"));
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    prefsw->ofmt_combo = lives_standard_combo_new(NULL, ofmt, LIVES_BOX(hbox), NULL);

    if (ofmt != NULL) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), prefs->encoder.of_desc);
      lives_list_free_all(&ofmt);
    }

    lives_list_free_all(&ofmt_all);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    label = lives_standard_label_new(_("Audio codec"));
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    prefsw->acodec_combo = lives_standard_combo_new(NULL, NULL, LIVES_BOX(hbox), NULL);
    prefs->acodec_list = NULL;

    set_acodec_list_from_allowed(prefsw, rdet);

  } else prefsw->acodec_combo = NULL;
  widget_opts.packing_height >>= 2;

  pixbuf_encoding = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_ENCODING, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_encoding, _("Encoding"), LIST_ENTRY_ENCODING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_encoding);

  // ---------------,
  // effects        |
  // ---------------'

  prefsw->vbox_right_effects = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_effects = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_effects);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_load_rfx = lives_standard_check_button_new(_("Load rendered effects on startup"), prefs->load_rfx_builtin,
                                 LIVES_BOX(hbox), NULL);

  prefsw->checkbutton_antialias = lives_standard_check_button_new(_("Use _antialiasing when resizing"), prefs->antialias,
                                  LIVES_BOX(hbox), NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_effects));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_apply_gamma = lives_standard_check_button_new(_("Automatic gamma correction (requires restart)"),
                                    prefs->apply_gamma, LIVES_BOX(hbox), (tmp = (_("Also affects the monitor gamma !! (for now...)"))));
  lives_free(tmp);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_rte_keys = lives_standard_spin_button_new
                                ((tmp = (_("Number of _real time effect keys"))), prefs->rte_keys_virtual, FX_KEYS_PHYSICAL,
                                 FX_KEYS_MAX_VIRTUAL, 1., 1., 0, LIVES_BOX(hbox),
                                 (tmp2 = lives_strdup(
                                     _("The number of \"virtual\" real time effect keys. "
                                       "They can be controlled through the real time effects window, or via network (OSC)."))));
  lives_free(tmp);
  lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_threads = lives_standard_check_button_new(_("Use _threads where possible when applying effects"),
                                future_prefs->nfx_threads > 1, LIVES_BOX(hbox),
                                NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->spinbutton_nfx_threads = lives_standard_spin_button_new(_("Number of _threads"), future_prefs->nfx_threads, 2., 65536.,
                                   1.,
                                   1.,
                                   0,
                                   LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads), prefsw->spinbutton_nfx_threads, FALSE);

  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));

  if (future_prefs->nfx_threads == 1) lives_widget_set_sensitive(prefsw->spinbutton_nfx_threads, FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->pa_gens = lives_standard_check_button_new(_("Push audio to video generators that support it"),
                    prefs->push_audio_to_gens, LIVES_BOX(hbox),
                    NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_effects));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Restart is required if any of the following paths are changed:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  widget_opts.packing_height *= 2;

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_effects));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->wpp_entry = lives_standard_direntry_new(_("Weed plugin path"), prefs->weed_plugin_path,
                      LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->frei0r_entry = lives_standard_direntry_new(_("Frei0r plugin path"), prefs->frei0r_path,
                         LONG_ENTRY_WIDTH, PATH_MAX,
                         LIVES_BOX(hbox), NULL);

  widget_opts.packing_height = woph;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_layout_add_label(LIVES_LAYOUT(layout),
                         _("(Frei0r directories should be separated by ':', ordered from lowest to highest priority)"),
                         FALSE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->ladspa_entry = lives_standard_direntry_new(_("LADSPA plugin path"), prefs->ladspa_path, LONG_ENTRY_WIDTH, PATH_MAX,
                         LIVES_BOX(hbox), NULL);

  widget_opts.packing_height = woph;

  pixbuf_effects = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_EFFECTS, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_effects, _("Effects"), LIST_ENTRY_EFFECTS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_effects);

  // -------------------,
  // Directories        |
  // -------------------'

  prefsw->table_right_directories = lives_table_new(10, 3, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->table_right_directories), widget_opts.border_width * 2);
  lives_table_set_col_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_width * 2);
  lives_table_set_row_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_height * 4);

  prefsw->scrollw_right_directories = lives_standard_scrolled_window_new(0, 0, prefsw->table_right_directories);

  label = lives_standard_label_new(_("Video load directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 4, 5,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Video save directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 5, 6,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Audio load directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 6, 7,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Image directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 7, 8,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Backup/Restore directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 8, 9,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Working directory (do not remove)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  /////
  prefsw->vid_load_dir_entry = lives_standard_entry_new(NULL, prefs->def_vid_load_dir, -1, PATH_MAX,
                               NULL,  _("The default directory for loading video clips from"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_load_dir_entry, 1, 2, 4, 5,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->vid_load_dir_entry), FALSE);

  // workdir warning label
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new("");
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  set_workdir_label_text(LIVES_LABEL(label), prefs->workdir);
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 3, 0, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  prefsw->workdir_label = label;

  prefsw->vid_save_dir_entry = lives_standard_entry_new(NULL, prefs->def_vid_save_dir, -1, PATH_MAX,
                               NULL, _("The default directory for saving encoded clips to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_save_dir_entry, 1, 2, 5, 6,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->vid_save_dir_entry), FALSE);

  prefsw->audio_dir_entry = lives_standard_entry_new(NULL, prefs->def_audio_dir, -1, PATH_MAX,
                            NULL, _("The default directory for loading and saving audio"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->audio_dir_entry, 1, 2, 6, 7,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->audio_dir_entry), FALSE);

  prefsw->image_dir_entry = lives_standard_entry_new(NULL, prefs->def_image_dir, -1, PATH_MAX,
                            NULL, _("The default directory for saving frameshots to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->image_dir_entry, 1, 2, 7, 8,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->image_dir_entry), FALSE);

  prefsw->proj_dir_entry = lives_standard_entry_new(NULL, prefs->def_proj_dir, -1, PATH_MAX,
                           NULL, _("The default directory for backing up/restoring single clips"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->proj_dir_entry, 1, 2, 8, 9,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->proj_dir_entry), FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->workdir_entry = lives_standard_direntry_new("",
                          (tmp = lives_filename_to_utf8(strlen(future_prefs->workdir) > 0 ? future_prefs->workdir : prefs->workdir, -1, NULL, NULL,
                                 NULL)),
                          LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), (tmp2 = (_("LiVES working directory."))));

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), hbox, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_free(tmp);
  lives_free(tmp2);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->workdir_entry), FALSE);

  dirbutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dirbutton), "filesel_type",
                               LIVES_INT_TO_POINTER(LIVES_DIR_SELECTION_WORKDIR));

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 4, 5,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->vid_load_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 5, 6,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->vid_save_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 6, 7,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->audio_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 7, 8,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->image_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 8, 9,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->proj_dir_entry);

  pixbuf_directories = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_DIRECTORY, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_directories, _("Directories"), LIST_ENTRY_DIRECTORIES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_directories);

  // ---------------,
  // Warnings       |
  // ---------------'

  prefsw->vbox_right_warnings = lives_vbox_new(FALSE, widget_opts.packing_height >> 2);
  prefsw->scrollw_right_warnings = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_warnings);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_warnings));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_warn_ds = lives_standard_spin_button_new(_("Warn if available diskspace falls below: "),
                               (double)prefs->ds_warn_level / 1000000.,
                               (double)prefs->ds_crit_level / 1000000.,
                               DS_WARN_CRIT_MAX, 1., 10., 0,
                               LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_("MB [set to 0 to disable]"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width >> 1);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_crit_ds = lives_standard_spin_button_new(_("Diskspace critical level: "),
                               (double)prefs->ds_crit_level / 1000000., 0.,
                               DS_WARN_CRIT_MAX, 1., 10., 0,
                               LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_("MB [set to 0 to disable]"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width >> 1);

  lives_layout_add_separator(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_fps = lives_standard_check_button_new(
                                   _("Warn on Insert / Merge if _frame rate of clipboard does not match frame rate of selection"),
                                   !(prefs->warning_mask & WARN_MASK_FPS), LIVES_BOX(hbox), NULL);


  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_fsize = lives_standard_check_button_new(
                                     _("Warn on Open if Instant Opening is not available, and the file _size exceeds "),
                                     !(prefs->warning_mask & WARN_MASK_FSIZE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_warn_fsize = lives_standard_spin_button_new(NULL,
                                  prefs->warn_file_size, 1., 2048., 1., 10., 0,
                                  LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_(" MB"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width >> 1);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_save_set = lives_standard_check_button_new(_("Show a warning before saving a se_t"),
                                      !(prefs->warning_mask & WARN_MASK_SAVE_SET), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mplayer = lives_standard_check_button_new
                                     (_("Show a warning if _mplayer/mplayer2, sox, composite or convert is not found when LiVES is started."),
                                      !(prefs->warning_mask & WARN_MASK_NO_MPLAYER), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_rendered_fx = lives_standard_check_button_new
                                         (_("Show a warning if no _rendered effects are found at startup."),
                                          !(prefs->warning_mask & WARN_MASK_RENDERED_FX), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_encoders = lives_standard_check_button_new
                                      (_("Show a warning if no _encoder plugins are found at startup."),
                                       !(prefs->warning_mask & WARN_MASK_NO_ENCODERS), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_dup_set = lives_standard_check_button_new
                                     (_("Show a warning if a _duplicate set name is entered."),
                                      !(prefs->warning_mask & WARN_MASK_DUPLICATE_SET), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_clips = lives_standard_check_button_new
                                          (_("When a set is loaded, warn if clips are missing from _layouts."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_MISSING_CLIPS), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_close = lives_standard_check_button_new
                                          (_("Warn if a clip used in a layout is about to be closed."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_CLOSE_FILE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_delete = lives_standard_check_button_new
      (_("Warn if frames used in a layout are about to be deleted."),
       !(prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_shift = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be shifted."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_alter = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be altered."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_adel = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be deleted."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_ashift = lives_standard_check_button_new
      (_("Warn if audio used in a layout is about to be shifted."),
       !(prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_aalt = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be altered."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_popup = lives_standard_check_button_new
                                          (_("Popup layout errors after clip changes."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_POPUP), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_discard_layout = lives_standard_check_button_new
      (_("Warn if the layout has not been saved when leaving multitrack mode."),
       !(prefs->warning_mask & WARN_MASK_EXIT_MT), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_achans = lives_standard_check_button_new
                                       (_("Warn if multitrack has no audio channels, and a layout with audio is loaded."),
                                        !(prefs->warning_mask & WARN_MASK_MT_ACHANS), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_no_jack = lives_standard_check_button_new
                                        (_("Warn if multitrack has audio channels, and your audio player is not \"jack\" or \"pulseaudio\"."),
                                         !(prefs->warning_mask & WARN_MASK_MT_NO_JACK), LIVES_BOX(hbox), NULL);

#ifdef HAVE_LDVGRAB
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_after_dvgrab = lives_standard_check_button_new
                                          (_("Show info message after importing from firewire device."),
                                              !(prefs->warning_mask & WARN_MASK_AFTER_DVGRAB), LIVES_BOX(hbox), NULL);

#else
  prefsw->checkbutton_warn_after_dvgrab = NULL;
#endif

#ifdef HAVE_YUV4MPEG
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_yuv4m_open = lives_standard_check_button_new
                                        (_("Show a warning before opening a yuv4mpeg stream (advanced)."),
                                         !(prefs->warning_mask & WARN_MASK_OPEN_YUV4M), LIVES_BOX(hbox), NULL);
#else
  prefsw->checkbutton_warn_yuv4m_open = NULL;
#endif

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_backup_space = lives_standard_check_button_new
      (_("Show a warning when multitrack is low on backup space."),
       !(prefs->warning_mask & WARN_MASK_MT_BACKUP_SPACE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_after_crash = lives_standard_check_button_new
                                         (_("Show a warning advising cleaning of disk space after a crash."),
                                          !(prefs->warning_mask & WARN_MASK_CLEAN_AFTER_CRASH), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_no_pulse = lives_standard_check_button_new
                                      (_("Show a warning if unable to connect to pulseaudio player."),
                                       !(prefs->warning_mask & WARN_MASK_NO_PULSE_CONNECT), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_wipe = lives_standard_check_button_new
                                         (_("Show a warning before wiping a layout which has unsaved changes."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_WIPE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_gamma = lives_standard_check_button_new
                                          (_("Show a warning if a loaded layout has incompatible gamma settings."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_GAMMA), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_vjmode_enter = lives_standard_check_button_new
                                          (_("Show a warning when the menu option Restart in VJ Mode becomes activated."),
                                              !(prefs->warning_mask & WARN_MASK_VJMODE_ENTER), LIVES_BOX(hbox), NULL);

  pixbuf_warnings = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_WARNING, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_warnings, _("Warnings"), LIST_ENTRY_WARNINGS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_warnings);

  // -----------,
  // Misc       |
  // -----------'

  prefsw->vbox_right_misc = lives_vbox_new(FALSE, widget_opts.packing_height * 4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_misc), widget_opts.border_width * 2);

  prefsw->scrollw_right_misc = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_misc);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When inserting/merging frames:  "));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  prefsw->ins_speed = lives_standard_radio_button_new(_("_Speed Up/Slow Down Insertion"), &rb_group2, LIVES_BOX(hbox), NULL);

  ins_resample = lives_standard_radio_button_new(_("_Resample Insertion"), &rb_group2, LIVES_BOX(hbox), NULL);

  prefsw->cdda_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), prefsw->cdda_hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->cdplay_entry = lives_standard_fileentry_new((tmp = (_("CD device           "))),
                         (tmp2 = lives_filename_to_utf8(prefs->cdplay_device, -1, NULL, NULL, NULL)),
                         LIVES_DEVICE_DIR, MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(prefsw->cdda_hbox),
                         (tmp3 = (_("LiVES can load audio tracks from this CD"))));
  lives_free(tmp);
  lives_free(tmp2);
  lives_free(tmp3);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_def_fps = lives_standard_spin_button_new((tmp = (_("Default FPS        "))),
                               prefs->default_fps, 1., FPS_MAX, 1., 1., 3,
                               LIVES_BOX(hbox),
                               (tmp2 = (_("Frames per second to use when none is specified"))));
  lives_free(tmp);
  lives_free(tmp2);

  pixbuf_misc = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MISC, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_misc, _("Misc"), LIST_ENTRY_MISC);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_misc);

  if (!capable->has_cdda2wav) {
    lives_widget_hide(prefsw->cdda_hbox);
  }

  // -----------,
  // Themes     |
  // -----------'

  prefsw->vbox_right_themes = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_themes), widget_opts.border_width * 2);

  prefsw->scrollw_right_themes = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_themes);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), hbox, TRUE, FALSE, widget_opts.packing_height);

  // scan for themes
  themes = get_plugin_list(PLUGIN_THEMES_CUSTOM, TRUE, NULL, NULL);
  themes = lives_list_concat(themes, get_plugin_list(PLUGIN_THEMES, TRUE, NULL, NULL));
  themes = lives_list_prepend(themes, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  prefsw->theme_combo = lives_standard_combo_new(_("New theme:           "), themes, LIVES_BOX(hbox), NULL);

  if (strcasecmp(future_prefs->theme, "none")) {
    theme = lives_strdup(future_prefs->theme);
  } else theme = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);

  lives_list_free_all(&themes);

  frame = lives_standard_frame_new(_("Main Theme Details"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget_color_to_lives_rgba(&rgba, &palette->normal_fore);
  prefsw->cbutton_fore = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Foreground Color"), FALSE, &rgba, &sp_red,
                         &sp_green,
                         &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fore), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->normal_back);
  prefsw->cbutton_back = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Background Color"), FALSE, &rgba, &sp_red,
                         &sp_green,
                         &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_back), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->menu_and_bars_fore);
  prefsw->cbutton_mabf = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Alt Foreground Color"), FALSE, &rgba, &sp_red,
                         &sp_green,
                         &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mabf), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->menu_and_bars);
  prefsw->cbutton_mab = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Alt Background Color"), FALSE, &rgba, &sp_red,
                        &sp_green,
                        &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mab), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->info_text);
  prefsw->cbutton_infot = lives_standard_color_button_new(LIVES_BOX(hbox), _("Info _Text Color"), FALSE, &rgba, &sp_red,
                          &sp_green, &sp_blue,
                          NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infot), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->info_base);
  prefsw->cbutton_infob = lives_standard_color_button_new(LIVES_BOX(hbox), _("Info _Base Color"), FALSE, &rgba, &sp_red,
                          &sp_green, &sp_blue,
                          NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infob), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
  prefsw->theme_style3 = lives_standard_check_button_new((tmp = (_("Theme is _light"))), (palette->style & STYLE_3),
                         LIVES_BOX(hbox),
                         (tmp2 = (_("Affects some contrast details of the timeline"))));
  lives_free(tmp);
  lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  prefsw->theme_style2 = NULL;
#if GTK_CHECK_VERSION(3, 0, 0)
  prefsw->theme_style2 = lives_standard_check_button_new(_("Color the start/end frame spinbuttons (requires restart)"),
                         (palette->style & STYLE_2), LIVES_BOX(hbox),
                         NULL);
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  prefsw->theme_style4 = lives_standard_check_button_new(_("Highlight horizontal separators in multitrack"),
                         (palette->style & STYLE_4), LIVES_BOX(hbox), NULL);
  layout = lives_layout_new(LIVES_BOX(vbox));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Frame blank image"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->frameblank_entry = lives_standard_fileentry_new(" ", mainw->frameblank_path,
                             prefs->def_image_dir, MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                             (tmp2 = (_("The frame image which is shown when there is no clip loaded."))));
  lives_free(tmp2);

  prefsw->fb_filebutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->fb_filebutton), "filter", widget_opts.image_filter);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->fb_filebutton), "filesel_type",
                               LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Separator image"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->sepimg_entry = lives_standard_fileentry_new(" ", mainw->sepimg_path,
                         prefs->def_image_dir, MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                         (tmp2 = (_("The image shown in the center of the interface."))));
  lives_free(tmp2);

  prefsw->se_filebutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));
  lives_signal_connect(prefsw->se_filebutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                       prefsw->sepimg_entry);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->se_filebutton), "filter", widget_opts.image_filter);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->se_filebutton), "filesel_type",
                               LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));

  frame = lives_standard_frame_new(_("Extended Theme Details"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_cesel = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Selected frames/audio (clip editor)"))),
                          FALSE, &palette->ce_sel, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_cesel), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_ceunsel = lives_standard_color_button_new(LIVES_BOX(hbox),
                            (tmp = (_("Unselected frames/audio (clip editor)"))),
                            FALSE, &palette->ce_unsel, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_ceunsel), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_evbox = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Track background (multitrack)"))),
                          FALSE, &palette->mt_evbox, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_evbox), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_vidcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Video block (multitrack)"))),
                           FALSE, &palette->vidcol, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_vidcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_audcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Audio block (multitrack)"))),
                           FALSE, &palette->audcol, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_audcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_fxcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Effects block (multitrack)"))),
                          FALSE, &palette->fxcol, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fxcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_mtmark = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Timeline mark (multitrack)"))),
                           FALSE, &palette->mt_mark, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mtmark), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_tlreg = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Timeline selection (multitrack)"))),
                          FALSE, &palette->mt_timeline_reg, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tlreg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->mt_timecode_bg);
  prefsw->cbutton_tcbg = lives_standard_color_button_new(LIVES_BOX(hbox),
                         (tmp = (_("Timecode background (multitrack)"))),
                         FALSE, &rgba, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcbg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->mt_timecode_fg);
  prefsw->cbutton_tcfg = lives_standard_color_button_new(LIVES_BOX(hbox),
                         (tmp = (_("Timecode foreground (multitrack)"))),
                         FALSE, &rgba, &sp_red, &sp_green, &sp_blue, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcfg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_fsur = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Frame surround"))),
                         FALSE, &palette->frame_surround, &sp_red, &sp_green, &sp_blue, NULL);
  lives_free(tmp);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fsur), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  // change in value of theme combo should set other widgets sensitive / insensitive
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->theme_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(theme_widgets_set_sensitive), (livespointer)prefsw);
  lives_combo_set_active_string(LIVES_COMBO(prefsw->theme_combo), theme);
  lives_free(theme);

  pixbuf_themes = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_THEMES, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_themes, _("Themes/Colors"), LIST_ENTRY_THEMES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_themes);

  // --------------------------,
  // streaming/networking      |
  // --------------------------'

  prefsw->vbox_right_net = lives_vbox_new(FALSE, widget_opts.packing_height * 4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_net), widget_opts.border_width * 2);

  prefsw->scrollw_right_net = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_net);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_bwidth = lives_standard_spin_button_new(_("Download bandwidth (Kb/s)       "),
                              prefs->dl_bandwidth, 0, 100000000., 1, 10, 0,
                              LIVES_BOX(hbox), NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_net));

#ifndef ENABLE_OSC
  label = lives_standard_label_new(_("LiVES must be compiled without \"configure --disable-OSC\" to use OMC"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), label, FALSE, FALSE, widget_opts.packing_height);
#endif

  hbox1 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox1, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, 0);

  prefsw->enable_OSC = lives_standard_check_button_new(_("OMC remote control enabled"), FALSE, LIVES_BOX(hbox), NULL);

#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->enable_OSC, FALSE);
#endif

  prefsw->spinbutton_osc_udp = lives_standard_spin_button_new(_("UDP port:       "),
                               prefs->osc_udp_port, 1., 65535., 1., 10., 0,
                               LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->enable_OSC_start = lives_standard_check_button_new(_("Start OMC on startup"), future_prefs->osc_start, LIVES_BOX(hbox),
                             NULL);

#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, FALSE);
  lives_widget_set_sensitive(prefsw->enable_OSC_start, FALSE);
  lives_widget_set_sensitive(label, FALSE);
#else
  if (prefs->osc_udp_started) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC), TRUE);
    lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, FALSE);
    lives_widget_set_sensitive(prefsw->enable_OSC, FALSE);
  }
#endif

  pixbuf_net = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_NET, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_net, _("Streaming/Networking"), LIST_ENTRY_NET);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_net);

  // ----------,
  // jack      |
  // ----------'

  prefsw->vbox_right_jack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_jack), widget_opts.border_width * 2);

  prefsw->scrollw_right_jack = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_jack);

  label = lives_standard_label_new(_("Jack transport"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);

#ifndef ENABLE_JACK_TRANSPORT
  label = lives_standard_label_new(
            _("LiVES must be compiled with jack/transport.h and jack/jack.h present to use jack transport"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), label, FALSE, FALSE, widget_opts.packing_height);
#else
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->jack_tserver_entry = lives_standard_entry_new(_("Jack _transport config file"), prefs->jack_tserver, -1, PATH_MAX,
                               LIVES_BOX(hbox),
                               NULL);

  lives_widget_set_sensitive(prefsw->jack_tserver_entry, FALSE); // unused for now

  prefsw->checkbutton_start_tjack = lives_standard_check_button_new(_("Start _server on LiVES startup"),
                                    (future_prefs->jack_opts & JACK_OPTS_START_TSERVER) ? TRUE : FALSE,
                                    LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_master = lives_standard_check_button_new(_("Jack transport _master (start and stop)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER) ? TRUE : FALSE,
                                    LIVES_BOX(hbox),
                                    NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_client = lives_standard_check_button_new(_("Jack transport _client (start and stop)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT) ? TRUE : FALSE,
                                    LIVES_BOX(hbox),
                                    NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_jack_client_toggled),
                             NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_tb_start = lives_standard_check_button_new(_("Jack transport sets start position"),
                                      (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_START) ?
                                      (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client))) : FALSE,
                                      LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_start,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client)));

  lives_signal_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_jack_tb_start_toggled),
                             NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->checkbutton_jack_tb_client = lives_standard_check_button_new(_("Jack transport timebase slave"),
                                       (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) ?
                                       (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start))) : FALSE,
                                       LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)));

  label = lives_standard_label_new(_("(See also Playback -> Audio follows video rate/direction)\n\n\n\n"));
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

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->jack_aserver_entry = lives_standard_entry_new(_("Jack _audio server config file"), prefs->jack_aserver, -1, PATH_MAX,
                               LIVES_BOX(hbox),
                               NULL);

  lives_widget_set_sensitive(prefsw->jack_aserver_entry, FALSE);

  prefsw->checkbutton_start_ajack = lives_standard_check_button_new(_("Start _server on LiVES startup"),
                                    (future_prefs->jack_opts & JACK_OPTS_START_ASERVER) ? TRUE : FALSE,
                                    LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_pwp = lives_standard_check_button_new(_("Play audio even when transport is _paused"),
                                 (future_prefs->jack_opts & JACK_OPTS_NOPLAY_WHEN_PAUSED) ? FALSE : TRUE,
                                 LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_pwp, prefs->audio_player == AUD_PLAYER_JACK);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_jack_read_autocon = lives_standard_check_button_new
                                          (_("Automatically connect to System Out ports when 'playing' External Audio"),
                                              (future_prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON) ? FALSE : TRUE,
                                              LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_read_autocon, prefs->audio_player == AUD_PLAYER_JACK);

#endif

  pixbuf_jack = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_JACK, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_jack, _("Jack Integration"), LIST_ENTRY_JACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_jack);

  // ----------------------,
  // MIDI/js learner       |
  // ----------------------'

  prefsw->vbox_right_midi = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_midi = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_midi);

  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_midi), widget_opts.border_width * 2);

  label = lives_standard_label_new(_("Events to respond to:"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_js = lives_standard_check_button_new(_("_Joystick events"), (prefs->omc_dev_opts & OMC_DEV_JS),
                               LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new(_("Leave blank to use defaults"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->omc_js_entry = lives_standard_fileentry_new((tmp = (_("_Joystick device")))
                         , prefs->omc_js_fname, LIVES_DEVICE_DIR, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                         (tmp2 = (_("The joystick device, e.g. /dev/input/js0"))));
  lives_free(tmp);
  lives_free(tmp2);

#ifdef OMC_MIDI_IMPL
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));
#endif

#endif

#ifdef OMC_MIDI_IMPL
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_midi = lives_standard_check_button_new(_("_MIDI events"), (prefs->omc_dev_opts & OMC_DEV_MIDI),
                                 LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  if (!(mainw->midi_channel_lock &&
        prefs->midi_rcv_channel > -1)) mchanlist = lives_list_append(mchanlist, (_("All channels")));
  for (i = 0; i < 16; i++) {
    midichan = lives_strdup_printf("%d", i);
    mchanlist = lives_list_append(mchanlist, midichan);
  }

  midichan = lives_strdup_printf("%d", prefs->midi_rcv_channel);

  prefsw->midichan_combo = lives_standard_combo_new(_("MIDI receive _channel"), mchanlist, LIVES_BOX(hbox), NULL);

  lives_list_free_all(&mchanlist);

  if (prefs->midi_rcv_channel > -1) {
    lives_combo_set_active_string(LIVES_COMBO(prefsw->midichan_combo), midichan);
  }

  lives_free(midichan);

  if (mainw->midi_channel_lock && prefs->midi_rcv_channel == -1) lives_widget_set_sensitive(prefsw->midichan_combo, FALSE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));

  prefsw->midi_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), prefsw->midi_hbox, FALSE, FALSE, widget_opts.packing_height);

#ifdef ALSA_MIDI
  prefsw->alsa_midi = lives_standard_radio_button_new((tmp = (_("Use _ALSA MIDI (recommended)"))), &alsa_midi_group,
                      LIVES_BOX(prefsw->midi_hbox),
                      (tmp2 = (_("Create an ALSA MIDI port which other MIDI devices can be connected to"))));

  lives_free(tmp);
  lives_free(tmp2);

  prefsw->alsa_midi_dummy = lives_standard_check_button_new((tmp = (_("Create dummy MIDI output"))),
                            prefs->alsa_midi_dummy,
                            LIVES_BOX(prefsw->midi_hbox),
                            (tmp2 = (_("Create a dummy ALSA MIDI output port."))));

  lives_free(tmp);
  lives_free(tmp2);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));
#endif

  hbox = lives_hbox_new(FALSE, 0);

  raw_midi_button = lives_standard_radio_button_new((tmp = (_("Use _raw MIDI"))), &alsa_midi_group,
                    LIVES_BOX(hbox),
                    (tmp2 = (_("Read directly from the MIDI device"))));

  lives_free(tmp);
  lives_free(tmp2);

#ifdef ALSA_MIDI
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(raw_midi_button), !prefs->use_alsa_midi);
#endif

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->omc_midi_entry = lives_standard_fileentry_new((tmp = (_("_MIDI device"))), prefs->omc_midi_fname,
                           LIVES_DEVICE_DIR, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                           (tmp2 = (_("The MIDI device, e.g. /dev/input/midi0"))));

  lives_free(tmp);
  lives_free(tmp2);

  prefsw->button_midid = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));

  label = lives_standard_label_new(_("Advanced"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_midicr = lives_standard_spin_button_new((tmp = (_("MIDI check _rate"))),
                              prefs->midi_check_rate, 1., 2000., 1., 10., 0,
                              LIVES_BOX(hbox),
                              (tmp2 = lives_strdup(
                                        _("Number of MIDI checks per keyboard tick. Increasing this may improve MIDI responsiveness, "
                                          "but may slow down playback."))));

  lives_free(tmp);
  lives_free(tmp2);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->spinbutton_midirpt = lives_standard_spin_button_new((tmp = (_("MIDI repeat"))),
                               prefs->midi_rpt, 1., 10000., 10., 100., 0,
                               LIVES_BOX(hbox),
                               (tmp2 = (_("Number of non-reads allowed between succesive reads."))));
  lives_free(tmp);
  lives_free(tmp2);

  label = lives_standard_label_new(_("(Warning: setting this value too high can slow down playback.)"));

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ALSA_MIDI
  lives_signal_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_alsa_midi_toggled),
                       NULL);

  on_alsa_midi_toggled(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi), prefsw);
#endif

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));

#endif
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->check_midi = lives_standard_check_button_new
                       (_("Midi program synch (requires the files midistart and midistop)"),
                        prefs->midisynch, LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(prefsw->check_midi, capable->has_midistartstop);

  pixbuf_midi = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MIDI, LIVES_ICON_SIZE_CUSTOM, -1, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_midi, _("MIDI/Joystick learner"), LIST_ENTRY_MIDI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_midi);

  prefsw->selection = lives_tree_view_get_selection(LIVES_TREE_VIEW(prefsw->prefs_list));
  lives_tree_selection_set_mode(prefsw->selection, LIVES_SELECTION_SINGLE);

  lives_signal_connect(prefsw->selection, LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_prefDomainChanged),
                       (livespointer)prefsw);

  if (saved_dialog == NULL) {
    widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
    // Preferences 'Revert' button
    prefsw->revertbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog),
                           LIVES_STOCK_REVERT_TO_SAVED, NULL,
                           LIVES_RESPONSE_CANCEL);
    lives_widget_show(prefsw->revertbutton);

    // Preferences 'Apply' button
    prefsw->applybutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog), LIVES_STOCK_APPLY, NULL,
                          LIVES_RESPONSE_ACCEPT);
    lives_widget_show(prefsw->applybutton);

    // Preferences 'Close' button
    prefsw->closebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog), LIVES_STOCK_CLOSE, NULL,
                          LIVES_RESPONSE_OK);
    lives_widget_show(prefsw->closebutton);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  } else {
    prefsw->revertbutton = saved_revertbutton;
    prefsw->applybutton = saved_applybutton;
    prefsw->closebutton = saved_closebutton;
    lives_widget_set_sensitive(prefsw->closebutton, TRUE);
  }
  lives_button_grab_default_special(prefsw->closebutton);

  prefs->cb_is_switch = FALSE;


  lives_widget_add_accelerator(prefsw->closebutton, LIVES_WIDGET_CLICKED_SIGNAL, prefsw->accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  // Set 'Revert' button as inactive since there are no changes yet
  lives_widget_set_sensitive(prefsw->revertbutton, FALSE);
  // Set 'Apply' button as inactive since there are no changes yet
  lives_widget_set_sensitive(prefsw->applybutton, FALSE);

  // Connect signals for 'Apply' button activity handling
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fore), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_back), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mabf), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mab), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infot), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_infob), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_mtmark), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_evbox), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tlreg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fsur), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcbg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_tcfg), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_cesel), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_ceunsel), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_vidcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_audcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cbutton_fxcol), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  if (prefsw->theme_style2 != NULL)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->theme_style2), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->theme_style3), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->theme_style4), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->wpp_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->frei0r_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->ladspa_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->fs_max_check), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  ACTIVE(recent_check, TOGGLED);
  ACTIVE(stop_screensaver_check, TOGGLED);
  ACTIVE(open_maximised_check, TOGGLED);
  ACTIVE(show_tool, TOGGLED);
  ACTIVE(mouse_scroll, TOGGLED);
  ACTIVE(checkbutton_ce_maxspect, TOGGLED);
  ACTIVE(ce_thumbs, TOGGLED);
  ACTIVE(checkbutton_button_icons, TOGGLED);
  ACTIVE(checkbutton_hfbwnp, TOGGLED);
  ACTIVE(checkbutton_show_asrc, TOGGLED);
  ACTIVE(checkbutton_show_ttips, TOGGLED);
  ACTIVE(rb_startup_mt, TOGGLED);
  ACTIVE(rb_startup_ce, TOGGLED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_crit_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(spinbutton_crit_ds_value_changed), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_crit_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_gmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_pmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_gmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(pmoni_gmoni_changed),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_pmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(pmoni_gmoni_changed),
                            NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->forcesmon), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_stream_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_rec_after_pb), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_warn_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_enter_prompt), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mt_enter_defs), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_render_prompt), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_width), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_height), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_def_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->backaudio_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->pertrack_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_undo_buf), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_mt_exit_render), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_mt_ab_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_max_disp_vtracks), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_always), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_never), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_every), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->video_open_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->frameblank_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->sepimg_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_ocp), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jpeg), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(png), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_instant_open), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_auto_deint), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_auto_trim), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_nobord), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_concat_images), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);

  ACTIVE(checkbutton_lb, TOGGLED);
  ACTIVE(checkbutton_lbmt, TOGGLED);
  ACTIVE(checkbutton_srgb, TOGGLED);
  ACTIVE(spinbutton_gamma, VALUE_CHANGED);
  ACTIVE(pbq_adaptive, TOGGLED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->pbq_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_show_stats), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->audp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  /* lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->audio_command_entry), LIVES_WIDGET_CHANGED_SIGNAL, */
  /*                      LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL); */
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_afollow), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_aclips), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->rdesk_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->rframes), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->rfps), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->reffects), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->rclips), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->raudio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->pa_gens), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->rextaudio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_ext_aud_thresh), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_rec_gb), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->encoder_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  if (capable->has_encoder_plugins) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->ofmt_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                              LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

    if (prefsw->acodec_combo != NULL)
      lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->acodec_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(apply_button_set_enabled),
                                NULL);
  }
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_antialias), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_load_rfx), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_apply_gamma), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_rte_keys), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_threads), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_nfx_threads), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->vid_load_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->vid_save_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->audio_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->image_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->proj_dir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->workdir_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_fps), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_fsize), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_warn_fsize), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_save_set), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mplayer), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_rendered_fx), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_encoders), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_dup_set), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_clips), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_close), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_delete), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_shift), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_alter), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_adel), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_ashift), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_aalt), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_layout_popup), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_discard_layout), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mt_achans), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_mt_no_jack), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#ifdef HAVE_LDVGRAB
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_after_dvgrab), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif
#ifdef HAVE_YUV4MPEG
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_warn_yuv4m_open), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif
  ACTIVE(checkbutton_warn_layout_gamma, TOGGLED);
  ACTIVE(checkbutton_warn_layout_wipe, TOGGLED);
  ACTIVE(checkbutton_warn_no_pulse, TOGGLED);
  ACTIVE(checkbutton_warn_after_crash, TOGGLED);
  ACTIVE(checkbutton_warn_mt_backup_space, TOGGLED);
  ACTIVE(checkbutton_warn_vjmode_enter, TOGGLED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->check_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->midichan_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->ins_speed), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(ins_resample), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->cdplay_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_def_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->theme_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_bwidth), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#ifdef ENABLE_OSC
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_osc_udp), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#endif

#ifdef ENABLE_JACK_TRANSPORT
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jack_tserver_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_start_tjack), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_master), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_client), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif

#ifdef ENABLE_JACK
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jack_aserver_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_start_ajack), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_pwp), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_read_autocon), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_omc_js), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->omc_js_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif
#ifdef OMC_MIDI_IMPL
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_omc_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
#ifdef ALSA_MIDI
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi_dummy), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif
  lives_signal_sync_connect(LIVES_GUI_OBJECT(raw_midi_button), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->omc_midi_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_midicr), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_midirpt), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled),
                            NULL);
#endif
#endif

  if (capable->has_encoder_plugins) {
    prefsw->encoder_name_fn = lives_signal_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->encoder_combo)),
                              LIVES_WIDGET_CHANGED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_encoder_entry_changed), NULL);

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
  if (saved_dialog == NULL) {
    lives_signal_connect(LIVES_GUI_OBJECT(prefsw->revertbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_prefs_revert_clicked),
                         NULL);

    lives_signal_connect(LIVES_GUI_OBJECT(prefsw->prefs_dialog), LIVES_WIDGET_DELETE_EVENT,
                         LIVES_GUI_CALLBACK(on_prefs_close_clicked),
                         NULL);

    lives_signal_connect(LIVES_GUI_OBJECT(prefsw->applybutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_prefs_apply_clicked),
                         NULL);
  }

  prefsw->close_func = lives_signal_connect(LIVES_GUI_OBJECT(prefsw->closebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prefs_close_clicked),
                       prefsw);

  lives_list_free_all(&audp);

  if (prefs_current_page == -1) {
    if (mainw->multitrack == NULL)
      select_pref_list_row(LIST_ENTRY_GUI, prefsw);
    else
      select_pref_list_row(LIST_ENTRY_MULTITRACK, prefsw);
  } else select_pref_list_row(prefs_current_page, prefsw);

  lives_widget_show_all(prefsw->prefs_dialog);
  on_prefDomainChanged(prefsw->selection, prefsw);
  lives_widget_queue_draw(prefsw->prefs_list);

  return prefsw;
}


void on_preferences_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *saved_dialog = (LiVESWidget *)user_data;
  mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (menuitem != NULL) prefs_current_page = -1;

  if (prefsw != NULL && prefsw->prefs_dialog != NULL) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
    return;
  }

  future_prefs->disabled_decoders = lives_list_copy_strings(prefs->disabled_decoders);
  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_context_update();

  prefsw = create_prefs_dialog(saved_dialog);
  lives_widget_show(prefsw->prefs_dialog);
  lives_window_set_position(LIVES_WINDOW(prefsw->prefs_dialog), LIVES_WIN_POS_CENTER_ALWAYS);
  lives_widget_queue_draw(prefsw->prefs_dialog);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, prefsw->prefs_dialog);
}


/*!
   Closes preferences dialog window
*/
void on_prefs_close_clicked(LiVESButton *button, livespointer user_data) {
  lives_list_free_all(&prefs->acodec_list);
  lives_list_free_all(&prefsw->pbq_list);
  lives_tree_view_set_model(LIVES_TREE_VIEW(prefsw->prefs_list), NULL);
  lives_free(prefsw->audp_name);
  lives_free(prefsw->orig_audp_name);
  lives_freep((void **)&resaudw);
  lives_list_free_all(&future_prefs->disabled_decoders);

  lives_general_button_clicked(button, user_data);

  prefsw = NULL;

  if (mainw->prefs_need_restart) {
    do_shutdown_msg();
    on_quit_activate(NULL, NULL);
  }
  if (mainw->multitrack != NULL) {
    mt_sensitise(mainw->multitrack);
    if (mt_needs_idlefunc) {
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }
  }
}


void pref_change_images(void) {
  if (prefs->show_gui) {
    if (mainw->current_file == -1) {
      load_start_image(0);
      load_end_image(0);
      if (mainw->preview_box != NULL) load_preview_image(FALSE);
    }
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    if (mainw->multitrack != NULL) {
      lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->multitrack->sep_image), mainw->imsep);
      mt_show_current_frame(mainw->multitrack, FALSE);
      lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    }
  }
}


void pref_change_xcolours(void) {
  // minor colours changed
  if (prefs->show_gui) {
    if (mainw->multitrack != NULL) {
      resize_timeline(mainw->multitrack);
      set_mt_colours(mainw->multitrack);
    } else {
      update_play_times();
      lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    }
  }
}


void pref_change_colours(void) {
  if (mainw->preview_box != NULL) {
    set_preview_box_colours();
  }

  if (prefs->show_gui) {
    set_colours(&palette->normal_fore, &palette->normal_back, &palette->menu_and_bars_fore, &palette->menu_and_bars, \
                &palette->info_base, &palette->info_text);

    if (mainw->multitrack != NULL) {
      set_mt_colours(mainw->multitrack);
      scroll_tracks(mainw->multitrack, mainw->multitrack->top_track, FALSE);
      track_select(mainw->multitrack);
      mt_clip_select(mainw->multitrack, FALSE);
    } else update_play_times();
  }
}


void on_prefs_apply_clicked(LiVESButton *button, livespointer user_data) {
  boolean needs_restart = FALSE;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, prefsw->prefs_dialog);

  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->revertbutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), FALSE);

  // Apply preferences
  needs_restart = apply_prefs(FALSE);

  if (!mainw->prefs_need_restart) {
    mainw->prefs_need_restart = needs_restart;
  }

  if (needs_restart) {
    //do_blocking_info_dialog(_("For the directory change to take effect LiVES will restart when preferences dialog closes."));
    do_blocking_info_dialog(_("LiVES will restart when preferences dialog closes."));
  }

  if (mainw->prefs_changed & PREFS_THEME_CHANGED) {
    lives_widget_set_sensitive(mainw->export_theme, FALSE);
    do_blocking_info_dialog(_("Disabling the theme will not take effect until the next time you start LiVES."));
  } else
    lives_widget_set_sensitive(mainw->export_theme, TRUE);

  if (mainw->prefs_changed & PREFS_JACK_CHANGED) {
    do_blocking_info_dialog(_("Jack options will not take effect until the next time you start LiVES."));
  }

  if (!(mainw->prefs_changed & PREFS_THEME_CHANGED) &&
      ((mainw->prefs_changed & PREFS_IMAGES_CHANGED) ||
       (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED) ||
       (mainw->prefs_changed & PREFS_COLOURS_CHANGED))) {
    // set details in prefs
    set_palette_prefs();
    if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
      load_theme_images();
    }
  }

  if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
    pref_change_images();
  }

  if (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED) {
    pref_change_xcolours();
  }

  if (mainw->prefs_changed & PREFS_COLOURS_CHANGED) {
    // major coulours changed
    // force reshow of window
    pref_change_colours();
    on_prefs_revert_clicked(button, NULL);
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, prefsw->prefs_dialog);

  lives_button_grab_default_special(prefsw->closebutton);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), TRUE);

  mainw->prefs_changed = 0;
}


/*
   Function is used to select particular row in preferences selection list
   selection is performed according to provided index which is one of LIST_ENTRY_* constants
*/
static void select_pref_list_row(uint32_t selected_idx, _prefsw *prefsw) {
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
  LiVESWidget *saved_dialog;
  register int i;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  if (future_prefs->vpp_argv != NULL) {
    for (i = 0; future_prefs->vpp_argv[i] != NULL; lives_free(future_prefs->vpp_argv[i++]));

    lives_free(future_prefs->vpp_argv);

    future_prefs->vpp_argv = NULL;
  }
  memset(future_prefs->vpp_name, 0, 64);

  lives_list_free_all(&prefs->acodec_list);
  lives_list_free_all(&prefsw->pbq_list);
  lives_tree_view_set_model(LIVES_TREE_VIEW(prefsw->prefs_list), NULL);

  lives_free(prefsw->audp_name);
  lives_free(prefsw->orig_audp_name);

  lives_list_free_all(&future_prefs->disabled_decoders);

  saved_dialog = prefsw->prefs_dialog;
  saved_revertbutton = prefsw->revertbutton;
  saved_applybutton = prefsw->applybutton;
  saved_closebutton = prefsw->closebutton;
  lives_signal_handler_disconnect(prefsw->closebutton, prefsw->close_func);
  lives_widget_remove_accelerator(prefsw->closebutton, prefsw->accel_group, LIVES_KEY_Escape, (LiVESXModifierType)0);

  lives_widget_destroy(prefsw->dialog_hpaned);
  lives_freep((void **)&prefsw);

  on_preferences_activate(NULL, saved_dialog);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
}


static int text_to_lives_perm(const char *text) {
  if (!text || !*text) return LIVES_PERM_INVALID;
  if (!strcmp(text, "DOWNLOADLOCAL")) return LIVES_PERM_DOWNLOAD_LOCAL;
  return LIVES_PERM_INVALID;
}

boolean lives_ask_permission(char **argv, int argc, int offs) {
  const char *sudocom = NULL;
  char *msg;
  boolean ret;
  int what = atoi(argv[offs]);
  if (what == LIVES_PERM_INVALID && *argv[offs]) {
    what = text_to_lives_perm(argv[offs]);
  }

  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    return ask_permission_dialog(what);
  case LIVES_PERM_DOWNLOAD_LOCAL:
    if (argc >= 5 && strstr(argv[4], "_TRY_SUDO_")) sudocom = (const char *)argv[2];
    ret = ask_permission_dialog_complex(what, argv, argc, ++offs, sudocom);
    return ret;
  default:
    msg = lives_strdup_printf("Unknown permission (%d) requested", what);
    LIVES_WARN(msg);
    lives_free(msg);
  }
  return FALSE;
}

