// plugins.c
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <dlfcn.h>
#include <errno.h>

#include "main.h"
#include "resample.h"
#include "effects.h"
#include "interface.h"

#include "rfx-builder.h"
#include "paramwindow.h"

#include "lsd-tab.h"

// *INDENT-OFF*
const char *const anames[AUDIO_CODEC_MAX] = {"mp3", "pcm", "mp2", "vorbis", "AC3", "AAC", "AMR_NB",
                                             "raw", "wma2", "opus", ""};
// *INDENT-ON*

static boolean list_plugins;

///////////////////////
// command-line plugins

LiVESList *get_plugin_result(const char *command, const char *delim, boolean allow_blanks, boolean strip) {
  LiVESList *list = NULL;
  char buffer[65536];

  //threaded_dialog_spin(0.);

  lives_popen(command, !mainw->is_ready && !list_plugins, buffer, 65535);

  if (THREADVAR(com_failed)) return NULL;

  //threaded_dialog_spin(0.);
  list = buff_to_list(buffer, delim, allow_blanks, strip);
  //threaded_dialog_spin(0.);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *plugin_request_with_blanks(const char *plugin_type, const char *plugin_name,
    const char *request) {
  // allow blanks in a list
  return plugin_request_common(plugin_type, plugin_name, request, "|", TRUE);
}


LIVES_GLOBAL_INLINE LiVESList *plugin_request(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "|", FALSE);
}


LIVES_GLOBAL_INLINE LiVESList *plugin_request_by_line(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "\n", FALSE);
}


LIVES_GLOBAL_INLINE LiVESList *plugin_request_by_space(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, " ", FALSE);
}


LiVESList *plugin_request_common(const char *plugin_type, const char *plugin_name, const char *request,
                                 const char *delim, boolean allow_blanks) {
  // returns a LiVESList of responses to -request, or NULL on error

  // NOTE: request must not be quoted here, since it contains a list of parameters
  // instead, caller should ensure that any strings in *request are suitably escaped and quoted
  // e.g. by calling param_marshall()

  LiVESList *reslist = NULL;
  char *com, *comfile;

  if (plugin_type) {
    if (!plugin_name || !strlen(plugin_name)) {
      return reslist;
    }

    // some types live in the config directory...
    if (!strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_CUSTOM) || !strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_TEST)) {
      comfile = lives_build_filename(prefs->config_datadir, plugin_type, plugin_name, NULL);
    } else if (!strcmp(plugin_type, PLUGIN_RFX_SCRAP)) {
      // scraps are in the workdir
      comfile = lives_build_filename(prefs->workdir, plugin_name, NULL);
    } else {
      comfile = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, plugin_type, plugin_name, NULL);
    }

    //#define DEBUG_PLUGINS
#ifdef DEBUG_PLUGINS
    com = lives_strdup_printf("\"%s\" %s", comfile, request);
    lives_printerr("will run: %s\n", com);
#else
    com = lives_strdup_printf("\"%s\" %s 2>/dev/null", comfile, request);
#endif
    lives_free(comfile);
  } else com = lives_strdup(request);
  list_plugins = FALSE;
  reslist = get_plugin_result(com, delim, allow_blanks, TRUE);
  lives_free(com);
  //threaded_dialog_spin(0.);
  return reslist;
}


//////////////////
// get list of plugins of various types

LiVESList *get_plugin_list(const char *plugin_type, boolean allow_nonex, const char *plugdir, const char *filter_ext) {
  // returns a LiVESList * of plugins of type plugin_type
  // returns empty list if there are no plugins of that type

  // allow_nonex to allow non-executable files (e.g. libs)
  // filter_ext can be non-NULL to filter for files ending .filter_ext

  // TODO - use enum for plugin_type

  // format is: allow_nonex (0 or 1) allow_subdirs (0 or 1)  plugindir   ext

  char *com, *tmp;
  LiVESList *pluglist;

  const char *ext = (filter_ext == NULL) ? "" : filter_ext;

  if (!strcmp(plugin_type, PLUGIN_THEMES)) {
    // must not allow_nonex, otherwise we get splash image etc (just want dirs)
    com = lives_strdup_printf("%s list_plugins 0 1 \"%s%s\" \"\"", prefs->backend_sync, prefs->prefix_dir, THEME_DIR);
  } else if (!strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS) ||
             !strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS) ||
             !strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_CUSTOM) ||
             !strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_TEST) ||
             !strcmp(plugin_type, PLUGIN_COMPOUND_EFFECTS_CUSTOM)
            ) {
    // look in home
    tmp = lives_build_path(prefs->config_datadir, plugin_type, NULL);
    com = lives_strdup_printf("%s list_plugins %d 0 \"%s\" \"%s\"", prefs->backend_sync, allow_nonex, tmp, ext);
    lives_free(tmp);
  } else if (!strcmp(plugin_type, PLUGIN_THEMES_CUSTOM)) {
    tmp = lives_build_path(prefs->config_datadir, PLUGIN_THEMES, NULL);
    com = lives_strdup_printf("%s list_plugins 0 1 \"%s\"", prefs->backend_sync, tmp);
    lives_free(tmp);
  } else if (!strcmp(plugin_type, PLUGIN_EFFECTS_WEED)) {
    com = lives_strdup_printf("%s list_plugins 1 1 \"%s\" \"%s\"", prefs->backend_sync,
                              (tmp = lives_filename_from_utf8((char *)plugdir, -1, NULL, NULL, NULL)), ext);
    lives_free(tmp);
  } else if (!strcmp(plugin_type, PLUGIN_DECODERS)) {
    com = lives_strdup_printf("%s list_plugins 1 0 \"%s\" \"%s\"", prefs->backend_sync,
                              (tmp = lives_filename_from_utf8((char *)plugdir, -1, NULL, NULL, NULL)), ext);
    lives_free(tmp);
  } else if (!strcmp(plugin_type, PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS)) {
    com = lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"", prefs->backend_sync, allow_nonex, prefs->prefix_dir,
                              PLUGIN_SCRIPTS_DIR, plugin_type, ext);
  } else if (!strcmp(plugin_type, PLUGIN_COMPOUND_EFFECTS_BUILTIN)) {
    com = lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"", prefs->backend_sync, allow_nonex, prefs->prefix_dir,
                              PLUGIN_COMPOUND_DIR, plugin_type, ext);
  } else {
    com = lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"", prefs->backend_sync, allow_nonex, prefs->lib_dir,
                              PLUGIN_EXEC_DIR, plugin_type, ext);
  }
  list_plugins = TRUE;

  //g_print("\n\n\nLIST CMD: %s\n",com);

  pluglist = get_plugin_result(com, "|", FALSE, TRUE);
  lives_free(com);
  //threaded_dialog_spin(0.);
  pluglist = lives_list_sort_alpha(pluglist, TRUE);
  return pluglist;
}


///////////////////
// video plugins

void save_vpp_defaults(_vid_playback_plugin *vpp, char *vpp_file) {
  // format is:
  // nbytes (string) : LiVES vpp defaults file version 2\n
  // for each video playback plugin:
  // 4 bytes (int) name length
  // n bytes name
  // 4 bytes (int) version length
  // n bytes version
  // 4 bytes (int) palette
  // 4 bytes (int) YUV sampling
  // 4 bytes (int) YUV clamping
  // 4 bytes (int) YUV subspace
  // 4 bytes (int) width
  // 4 bytes (int) height
  // 8 bytes (double) fps
  // 4 bytes (int) fps_numerator [0 indicates use fps double, >0 use fps_numer/fps_denom ]
  // 4 bytes (int) fps_denominator
  // 4 bytes argc (count of argv, may be 0)
  //
  // for each argv (extra params):
  // 4 bytes (int) length
  // n bytes string param value

  int fd;
  int32_t len;
  const char *version;
  int i;
  char *msg;
  int intzero = 0;
  double dblzero = 0.;

  if (!vpp) {
    lives_rm(vpp_file);
    return;
  }

  if ((fd = lives_open3(vpp_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
    msg = lives_strdup_printf(_("\n\nUnable to write video playback plugin defaults file\n%s\nError code %d\n"), vpp_file, errno);
    LIVES_ERROR(msg);
    lives_free(msg);
    return;
  }

  msg = lives_strdup_printf(_("Updating video playback plugin defaults in %s\n"), vpp_file);
  LIVES_INFO(msg);
  lives_free(msg);

  msg = lives_strdup("LiVES vpp defaults file version 2\n");
  if (!lives_write(fd, msg, strlen(msg), FALSE)) return;
  lives_free(msg);

  len = strlen(vpp->name);
  if (lives_write_le(fd, &len, 4, FALSE) < 4) return;
  if (lives_write(fd, vpp->name, len, FALSE) < len) return;

  version = (*vpp->version)();
  len = strlen(version);
  if (lives_write_le(fd, &len, 4, FALSE) < 4) return;
  if (lives_write(fd, version, len, FALSE) < len) return;

  if (lives_write_le(fd, &(vpp->palette), 4, FALSE) < 4) return;
  if (lives_write_le(fd, &(vpp->YUV_sampling), 4, FALSE) < 4) return;
  if (lives_write_le(fd, &(vpp->YUV_clamping), 4, FALSE) < 4) return;
  if (lives_write_le(fd, &(vpp->YUV_subspace), 4, FALSE) < 4) return;

  if (lives_write_le(fd, vpp->fwidth <= 0 ? &intzero : & (vpp->fwidth), 4, FALSE) < 4) return;
  if (lives_write_le(fd, vpp->fheight <= 0 ? &intzero : & (vpp->fheight), 4, FALSE) < 4) return;

  if (lives_write_le(fd, vpp->fixed_fpsd <= 0. ? &dblzero : & (vpp->fixed_fpsd), 8, FALSE) < 8) return;
  if (lives_write_le(fd, vpp->fixed_fps_numer <= 0 ? &intzero : & (vpp->fixed_fps_numer), 4, FALSE) < 4) return;
  if (lives_write_le(fd, vpp->fixed_fps_denom <= 0 ? &intzero : & (vpp->fixed_fps_denom), 4, FALSE) < 4) return;

  if (lives_write_le(fd, &(vpp->extra_argc), 4, FALSE) < 4) return;

  for (i = 0; i < vpp->extra_argc; i++) {
    len = strlen(vpp->extra_argv[i]);
    if (lives_write_le(fd, &len, 4, FALSE) < 4) return;
    if (lives_write(fd, vpp->extra_argv[i], len, FALSE) < len) return;
  }

  close(fd);
}


void load_vpp_defaults(_vid_playback_plugin *vpp, char *vpp_file) {
  ssize_t len;
  const char *version;

  char buf[512];

  char *msg;

  int retval;
  int fd;

  register int i;

  if (!lives_file_test(vpp_file, LIVES_FILE_TEST_EXISTS)) {
    return;
  }

  d_print(_("Loading video playback plugin defaults from %s..."), vpp_file);

  do {
    retval = 0;
    if ((fd = lives_open2(vpp_file, O_RDONLY)) == -1) {
      retval = do_read_failed_error_s_with_retry(vpp_file, lives_strerror(errno));
      if (retval == LIVES_RESPONSE_CANCEL) {
        vpp = NULL;
        return;
      }
    } else {
      do {
        // only do this loop once, so we can use break to escape it

        THREADVAR(read_failed) = FALSE;
        msg = lives_strdup("LiVES vpp defaults file version 2\n");
        len = lives_read(fd, buf, strlen(msg), FALSE);
        if (len < 0) len = 0;
        lives_memset(buf + len, 0, 1);

        if (THREADVAR(read_failed)) break;

        // identifier string
        if (strcmp(msg, buf)) {
          lives_free(msg);
          d_print_file_error_failed();
          close(fd);
          return;
        }
        lives_free(msg);

        // plugin name
        lives_read_le(fd, &len, 4, FALSE);
        if (THREADVAR(read_failed)) break;
        lives_read(fd, buf, len, FALSE);
        lives_memset(buf + len, 0, 1);

        if (THREADVAR(read_failed)) break;

        if (strcmp(buf, vpp->name)) {
          d_print_file_error_failed();
          close(fd);
          return;
        }

        // version string
        version = (*vpp->version)();
        lives_read_le(fd, &len, 4, FALSE);
        if (THREADVAR(read_failed)) break;
        lives_read(fd, buf, len, FALSE);

        if (THREADVAR(read_failed)) break;

        lives_memset(buf + len, 0, 1);

        if (strcmp(buf, version)) {
          msg = lives_strdup_printf(
                  _("\nThe %s video playback plugin has been updated.\nPlease check your settings in\n"
                    "Tools|Preferences|Playback|Playback Plugins Advanced\n\n"),
                  vpp->name);
          widget_opts.non_modal = TRUE;
          do_error_dialog(msg);
          widget_opts.non_modal = FALSE;
          lives_free(msg);
          lives_rm(vpp_file);
          d_print_failed();
          close(fd);
          return;
        }

        lives_read_le(fd, &(vpp->palette), 4, FALSE);
        lives_read_le(fd, &(vpp->YUV_sampling), 4, FALSE);
        lives_read_le(fd, &(vpp->YUV_clamping), 4, FALSE);
        lives_read_le(fd, &(vpp->YUV_subspace), 4, FALSE);
        lives_read_le(fd, &(vpp->fwidth), 4, FALSE);
        lives_read_le(fd, &(vpp->fheight), 4, FALSE);
        lives_read_le(fd, &(vpp->fixed_fpsd), 8, FALSE);
        lives_read_le(fd, &(vpp->fixed_fps_numer), 4, FALSE);
        lives_read_le(fd, &(vpp->fixed_fps_denom), 4, FALSE);

        if (THREADVAR(read_failed)) break;

        lives_read_le(fd, &(vpp->extra_argc), 4, FALSE);

        if (THREADVAR(read_failed)) break;

        if (vpp->extra_argv) {
          for (i = 0; vpp->extra_argv[i]; i++) {
            lives_free(vpp->extra_argv[i]);
          }
          lives_free(vpp->extra_argv);
        }

        vpp->extra_argv = (char **)lives_calloc((vpp->extra_argc + 1), (sizeof(char *)));

        for (i = 0; i < vpp->extra_argc; i++) {
          lives_read_le(fd, &len, 4, FALSE);
          if (THREADVAR(read_failed)) break;
          vpp->extra_argv[i] = (char *)lives_malloc(len + 1);
          lives_read(fd, vpp->extra_argv[i], len, FALSE);
          if (THREADVAR(read_failed)) break;
          lives_memset((vpp->extra_argv[i]) + len, 0, 1);
        }

        vpp->extra_argv[i] = NULL;

        close(fd);
      } while (FALSE);

      if (THREADVAR(read_failed)) {
        close(fd);
        retval = do_read_failed_error_s_with_retry(vpp_file, NULL);
        if (retval == LIVES_RESPONSE_CANCEL) {
          THREADVAR(read_failed) = FALSE;
          vpp = NULL;
          d_print_file_error_failed();
          return;
        }
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  d_print_done();
}


void on_vppa_cancel_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw = (_vppaw *)user_data;
  _vid_playback_plugin *vpp = vppw->plugin;

  lives_widget_destroy(vppw->dialog);
  lives_widget_context_update();
  if (vpp && vpp != mainw->vpp) {
    // close the temp current vpp
    close_vid_playback_plugin(vpp);
  }

  if (vppw->rfx) {
    if (!vppw->keep_rfx) {
      rfx_free(vppw->rfx);
      lives_free(vppw->rfx);
    }
  }

  lives_free(vppw);

  if (prefsw) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
  }
}


void on_vppa_ok_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw = (_vppaw *)user_data;
  uint64_t xwinid = 0;

  const char *fixed_fps = NULL;
  const char *tmp;

  char *cur_pal = NULL;

  int *pal_list, i = 0;

  boolean ext_audio = FALSE;

  _vid_playback_plugin *vpp = vppw->plugin;

  if (vppw->rfx && mainw->textwidget_focus) {
    // make sure text widgets are updated if they activate the default
    LiVESWidget *textwidget = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus),
                              TEXTWIDGET_KEY);
    after_param_text_changed(textwidget, vppw->rfx);
  }

  if (!special_cleanup(TRUE)) {
    // check for file overwrites with special type "filewrite"
    // if user declines, will return with LIVES_RESPONSE_RETRY
    if (LIVES_IS_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))))
      lives_dialog_response(LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))), LIVES_RESPONSE_RETRY);
    return;
  }

  mainw->textwidget_focus = NULL;

  if (vpp == mainw->vpp) {
    if (vppw->spinbuttonw) mainw->vpp->fwidth
        = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonw));
    if (vppw->spinbuttonh) mainw->vpp->fheight
        = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonh));
    if (vppw->apply_fx) mainw->fx1_bool = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(vppw->apply_fx));
    if (vppw->fps_entry) fixed_fps = lives_entry_get_text(LIVES_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry) {
      cur_pal = lives_strdup(lives_entry_get_text(LIVES_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal, ' ') > 1) {
        char **array = lives_strsplit(cur_pal, " ", 2);
        char *clamping = lives_strdup(array[1] + 1);
        lives_free(cur_pal);
        cur_pal = lives_strdup(array[0]);
        lives_memset(clamping + strlen(clamping) - 1, 0, 1);
        do {
          tmp = weed_yuv_clamping_get_name(i);
          if (tmp && !strcmp(clamping, tmp)) {
            vpp->YUV_clamping = i;
            break;
          }
          i++;
        } while (tmp);
        lives_strfreev(array);
        lives_free(clamping);
      }
    }

    if (vppw->fps_entry) {
      if (!strlen(fixed_fps)) {
        mainw->vpp->fixed_fpsd = -1.;
        mainw->vpp->fixed_fps_numer = 0;
      } else {
        if (get_token_count((char *)fixed_fps, ':') > 1) {
          char **array = lives_strsplit(fixed_fps, ":", 2);
          mainw->vpp->fixed_fps_numer = atoi(array[0]);
          mainw->vpp->fixed_fps_denom = atoi(array[1]);
          lives_strfreev(array);
          mainw->vpp->fixed_fpsd = get_ratio_fps((char *)fixed_fps);
        } else {
          mainw->vpp->fixed_fpsd = lives_strtod(fixed_fps, NULL);
          mainw->vpp->fixed_fps_numer = 0;
        }
      }
    } else {
      mainw->vpp->fixed_fpsd = -1.;
      mainw->vpp->fixed_fps_numer = 0;
    }

    if (mainw->vpp->fixed_fpsd > 0. && (mainw->fixed_fpsd > 0. ||
                                        (mainw->vpp->set_fps &&
                                         !((*mainw->vpp->set_fps)(mainw->vpp->fixed_fpsd))))) {
      do_vpp_fps_error();
      mainw->error = TRUE;
      mainw->vpp->fixed_fpsd = -1.;
      mainw->vpp->fixed_fps_numer = 0;
    }

    if (vppw->pal_entry) {
      if (vpp->get_palette_list && (pal_list = (*vpp->get_palette_list)())) {
        for (i = 0; pal_list[i] != WEED_PALETTE_END; i++) {
          if (!strcmp(cur_pal, weed_palette_get_name(pal_list[i]))) {
            vpp->palette = pal_list[i];
            if (mainw->ext_playback) {
              pthread_mutex_lock(&mainw->vpp_stream_mutex);
              mainw->ext_audio = FALSE;
              pthread_mutex_unlock(&mainw->vpp_stream_mutex);
              lives_grab_remove(LIVES_MAIN_WINDOW_WIDGET);
              if (mainw->vpp->exit_screen) {
                (*mainw->vpp->exit_screen)(mainw->ptr_x, mainw->ptr_y);
              }

#ifdef RT_AUDIO
              stop_audio_stream();
#endif
              mainw->stream_ticks = -1;
              mainw->vpp->palette = pal_list[i];
              if (!(*vpp->set_palette)(vpp->palette)) {
                do_vpp_palette_error();
                mainw->error = TRUE;
              }

              if (prefs->play_monitor != 0) {
                if (mainw->play_window) {
                  xwinid = lives_widget_get_xwinid(mainw->play_window, "Unsupported display type for playback plugin");
                  if (xwinid == -1) {
                    lives_dialog_response(LIVES_DIALOG(vppw->dialog), LIVES_RESPONSE_CANCEL);
                    return;
                  }
                }
              }

#ifdef RT_AUDIO
              if (vpp->set_yuv_palette_clamping)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

              if (mainw->vpp->audio_codec != AUDIO_CODEC_NONE && prefs->stream_audio_out) {
                start_audio_stream();
              }

#endif
              if (vpp->init_audio && prefs->stream_audio_out) {
#ifdef HAVE_PULSE_AUDIO
                if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
                  if ((*vpp->init_audio)(mainw->pulsed->out_arate, mainw->pulsed->out_achans, vpp->extra_argc, vpp->extra_argv))
                    mainw->ext_audio = TRUE;
                }
#endif
#ifdef ENABLE_JACK
                if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
                  if ((*vpp->init_audio)(mainw->jackd->sample_out_rate, mainw->jackd->num_output_channels,
                                         vpp->extra_argc, vpp->extra_argv))
                    ext_audio = TRUE;
                }
#endif
              }

              if (vpp->init_screen) {
                (*vpp->init_screen)(mainw->vpp->fwidth > 0 ? mainw->vpp->fwidth : mainw->pwidth,
                                    mainw->vpp->fheight > 0 ? mainw->vpp->fheight : mainw->pheight,
                                    TRUE, xwinid, vpp->extra_argc, vpp->extra_argv);
              }
              mainw->ext_audio = ext_audio; // cannot set this until after init_screen()
              if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY && prefs->play_monitor == 0) {
                lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window), TRUE);
                lives_grab_add(LIVES_MAIN_WINDOW_WIDGET);
              }
            } else {
              mainw->vpp->palette = pal_list[i];
              if (!(*vpp->set_palette)(vpp->palette)) {
                do_vpp_palette_error();
                mainw->error = TRUE;
              }
              if (vpp->set_yuv_palette_clamping)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);
            }
            break;
          }
        }
      }
      lives_free(cur_pal);
    }
    if (vpp->extra_argv) {
      for (i = 0; vpp->extra_argv[i]; i++) lives_free(vpp->extra_argv[i]);
      lives_free(vpp->extra_argv);
      vpp->extra_argv = NULL;
    }
    vpp->extra_argc = 0;
    if (vppw->rfx) {
      vpp->extra_argv = param_marshall_to_argv(vppw->rfx);
      for (i = 0; vpp->extra_argv[i]; vpp->extra_argc = ++i);
    }
    mainw->write_vpp_file = TRUE;
  } else {
    if (vppw->spinbuttonw)
      future_prefs->vpp_fwidth = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonw));
    else future_prefs->vpp_fwidth = -1;
    if (vppw->spinbuttonh)
      future_prefs->vpp_fheight = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonh));
    else future_prefs->vpp_fheight = -1;
    if (vppw->fps_entry) fixed_fps = lives_entry_get_text(LIVES_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry) {
      cur_pal = lives_strdup(lives_entry_get_text(LIVES_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal, ' ') > 1) {
        char **array = lives_strsplit(cur_pal, " ", 2);
        char *clamping = lives_strdup(array[1] + 1);
        lives_free(cur_pal);
        cur_pal = lives_strdup(array[0]);
        lives_memset(clamping + strlen(clamping) - 1, 0, 1);
        do {
          tmp = weed_yuv_clamping_get_name(i);
          if (tmp && !strcmp(clamping, tmp)) {
            future_prefs->vpp_YUV_clamping = i;
            break;
          }
          i++;
        } while (tmp);
        lives_strfreev(array);
        lives_free(clamping);
      }
    }

    if (fixed_fps) {
      if (get_token_count((char *)fixed_fps, ':') > 1) {
        char **array = lives_strsplit(fixed_fps, ":", 2);
        future_prefs->vpp_fixed_fps_numer = atoi(array[0]);
        future_prefs->vpp_fixed_fps_denom = atoi(array[1]);
        lives_strfreev(array);
        future_prefs->vpp_fixed_fpsd = get_ratio_fps((char *)fixed_fps);
      } else {
        future_prefs->vpp_fixed_fpsd = lives_strtod(fixed_fps, NULL);
        future_prefs->vpp_fixed_fps_numer = 0;
      }
    } else {
      future_prefs->vpp_fixed_fpsd = -1.;
      future_prefs->vpp_fixed_fps_numer = 0;
    }

    if (cur_pal) {
      if (vpp->get_palette_list && (pal_list = (*vpp->get_palette_list)())) {
        for (i = 0; pal_list[i] != WEED_PALETTE_END; i++) {
          if (!strcmp(cur_pal, weed_palette_get_name(pal_list[i]))) {
            future_prefs->vpp_palette = pal_list[i];
            break;
          }
        }
      }
      lives_free(cur_pal);
    } else future_prefs->vpp_palette = WEED_PALETTE_END;

    if (future_prefs->vpp_argv) {
      for (i = 0; future_prefs->vpp_argv[i]; i++) lives_free(future_prefs->vpp_argv[i]);
      lives_free(future_prefs->vpp_argv);
      future_prefs->vpp_argv = NULL;
    }

    future_prefs->vpp_argc = 0;
    if (vppw->rfx) {
      future_prefs->vpp_argv = param_marshall_to_argv(vppw->rfx);
      if (future_prefs->vpp_argv) {
        for (i = 0; future_prefs->vpp_argv[i]; future_prefs->vpp_argc = ++i);
      }
    } else {
      future_prefs->vpp_argv = vpp->extra_argv;
      vpp->extra_argv = NULL;
      vpp->extra_argc = 0;
    }
  }
  if (button && !mainw->error) on_vppa_cancel_clicked(button, user_data);
  else lives_dialog_response(LIVES_DIALOG(vppw->dialog), LIVES_RESPONSE_OK);
  if (button) mainw->error = FALSE;
}


void on_vppa_save_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw = (_vppaw *)user_data;
  _vid_playback_plugin *vpp = vppw->plugin;
  char *save_file;

  // apply
  mainw->error = FALSE;
  on_vppa_ok_clicked(NULL, user_data);
  if (mainw->error) {
    mainw->error = FALSE;
    return;
  }

  // get filename
  save_file = choose_file(NULL, NULL, NULL, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
  if (!save_file) return;

  // save
  d_print(_("Saving playback plugin defaults to %s..."), save_file);
  save_vpp_defaults(vpp, save_file);
  d_print_done();
  lives_free(save_file);

}


_vppaw *on_vpp_advanced_clicked(LiVESButton *button, livespointer user_data) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *combo;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *savebutton;

  LiVESAccelGroup *accel_group;

  _vppaw *vppa;

  _vid_playback_plugin *tmpvpp;

  int *pal_list;

  double wscale = 1., hscale = 1.;

  int intention = LIVES_INTENTION_PLAY;
  int hsize, vsize;

  LiVESList *fps_list_strings = NULL;
  LiVESList *pal_list_strings = NULL;

  const char *string;
  const char *pversion;
  const char *desc;
  const char *fps_list;

  char *title;
  char *tmp, *tmp2;

  char *ctext = NULL;

  // TODO - set default values from tmpvpp

  if (user_data) intention = LIVES_POINTER_TO_INT(user_data);

  if (strlen(future_prefs->vpp_name)) {
    if (!(tmpvpp = open_vid_playback_plugin(future_prefs->vpp_name, FALSE))) return NULL;
  } else {
    if (!mainw->vpp) return NULL;
    tmpvpp = mainw->vpp;
  }

  vppa = (_vppaw *)(lives_calloc(1, sizeof(_vppaw)));

  vppa->plugin = tmpvpp;
  /* vppa->rfx = NULL; */
  /* vppa->spinbuttonh = vppa->spinbuttonw = NULL; */
  /* vppa->apply_fx = NULL; */
  /* vppa->pal_entry = vppa->fps_entry = NULL; */
  /* vppa->keep_rfx = FALSE; */

  vppa->intention = intention;

  pversion = (tmpvpp->version)();

  if (intention == LIVES_INTENTION_PLAY)
    title = lives_strdup_printf("%s", pversion);
  else {
    // LIVES_INTENTION_TRANSCODE
    title = (_("Quick Transcoding"));
    wscale = 2. * widget_opts.scale;
    hscale = 1.5;
  }

  vppa->dialog = lives_standard_dialog_new(title, FALSE, DEF_DIALOG_WIDTH * wscale, DEF_DIALOG_HEIGHT * hscale);
  lives_free(title);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(vppa->dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(vppa->dialog));

  // the filling...
  if (intention == LIVES_INTENTION_PLAY && tmpvpp->get_description) {
    desc = (tmpvpp->get_description)();
    if (desc) {
      label = lives_standard_label_new(desc);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    }
  }
  if (intention == LIVES_INTENTION_TRANSCODE) {
    tmp = lives_big_and_bold("%s", _("Quick transcode provides a rapid, high quality preview of the selected frames and audio."));
    widget_opts.use_markup = TRUE;
    label = lives_standard_label_new(tmp);
    widget_opts.use_markup = FALSE;
    lives_free(tmp);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
  }

  if (tmpvpp->get_fps_list && (fps_list = (*tmpvpp->get_fps_list)(tmpvpp->palette))) {
    int nfps, i;
    char **array = lives_strsplit(fps_list, "|", -1);

    nfps = get_token_count((char *)fps_list, '|');
    for (i = 0; i < nfps; i++) {
      if (strlen(array[i]) && strcmp(array[i], "\n")) {
        if (get_token_count(array[i], ':') == 0) {
          fps_list_strings = lives_list_append(fps_list_strings, remove_trailing_zeroes(lives_strtod(array[i], NULL)));
        } else fps_list_strings = lives_list_append(fps_list_strings, lives_strdup(array[i]));
      }
    }

    if (intention == LIVES_INTENTION_PLAY) {
      // fps
      combo = lives_standard_combo_new((tmp = (_("_FPS"))), fps_list_strings,
                                       LIVES_BOX(dialog_vbox), (tmp2 = (_("Fixed framerate for plugin.\n"))));
      lives_free(tmp);
      lives_free(tmp2);
      vppa->fps_entry = lives_combo_get_entry(LIVES_COMBO(combo));
      lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(combo))), COMBOWIDTHCHARS);

      lives_list_free_all(&fps_list_strings);
      lives_strfreev(array);

      if (tmpvpp->fixed_fps_numer > 0) {
        char *tmp = lives_strdup_printf("%d:%d", tmpvpp->fixed_fps_numer, tmpvpp->fixed_fps_denom);
        lives_entry_set_text(LIVES_ENTRY(vppa->fps_entry), tmp);
        lives_free(tmp);
      } else {
        char *tmp = remove_trailing_zeroes(tmpvpp->fixed_fpsd);
        lives_entry_set_text(LIVES_ENTRY(vppa->fps_entry), tmp);
        lives_free(tmp);
      }
    }
  }

  // frame size

  if (!(tmpvpp->capabilities & VPP_LOCAL_DISPLAY)) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    add_fill_to_box(LIVES_BOX(hbox));

    hsize = tmpvpp->fwidth > 0 ? tmpvpp->fwidth :
            intention == LIVES_INTENTION_TRANSCODE ? cfile->hsize : DEF_VPP_HSIZE;

    vppa->spinbuttonw = lives_standard_spin_button_new(_("_Width"),
                        hsize,
                        4., MAX_FRAME_WIDTH, 4., 16., 0, LIVES_BOX(hbox), NULL);

    add_fill_to_box(LIVES_BOX(hbox));

    vsize = tmpvpp->fheight > 0 ? tmpvpp->fheight :
            intention == LIVES_INTENTION_TRANSCODE ? cfile->vsize : DEF_VPP_VSIZE;

    vppa->spinbuttonh = lives_standard_spin_button_new(_("_Height"),
                        vsize,
                        4., MAX_FRAME_HEIGHT, 4., 16., 0, LIVES_BOX(hbox), NULL);

    if (intention == LIVES_INTENTION_TRANSCODE) {
      if (mainw->event_list) {
        lives_widget_set_no_show_all(hbox, TRUE);
      } else {
        // add aspect ratio butto
        lives_special_aspect_t *aspect;
        /* hbox = lives_hbox_new(FALSE, 0); */
        /* lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height); */
        aspect = (lives_special_aspect_t *)add_aspect_ratio_button(LIVES_SPIN_BUTTON(vppa->spinbuttonw),
                 LIVES_SPIN_BUTTON(vppa->spinbuttonh), LIVES_BOX(hbox));
        // don't reset the aspect params when we make_param_box
        aspect->no_reset = TRUE;
      }
    }
    add_fill_to_box(LIVES_BOX(hbox));
  }

  if (intention == LIVES_INTENTION_PLAY) {
    if (tmpvpp->get_palette_list && (pal_list = (*tmpvpp->get_palette_list)())) {
      int i;

      for (i = 0; pal_list[i] != WEED_PALETTE_END; i++) {
        int j = 0;
        string = weed_palette_get_name(pal_list[i]);
        if (weed_palette_is_yuv(pal_list[i]) && tmpvpp->get_yuv_palette_clamping) {
          int *clampings = (*tmpvpp->get_yuv_palette_clamping)(pal_list[i]);
          while (clampings[j] != -1) {
            char *string2 = lives_strdup_printf("%s (%s)", string, weed_yuv_clamping_get_name(clampings[j]));
            pal_list_strings = lives_list_append(pal_list_strings, string2);
            j++;
          }
        }
        if (j == 0) {
          pal_list_strings = lives_list_append(pal_list_strings, lives_strdup(string));
        }
      }

      combo = lives_standard_combo_new((tmp = (_("_Colourspace"))), pal_list_strings,
                                       LIVES_BOX(dialog_vbox), tmp2 = (_("Colourspace input to the plugin.\n")));
      lives_free(tmp);
      lives_free(tmp2);
      vppa->pal_entry = lives_combo_get_entry(LIVES_COMBO(combo));

      if (tmpvpp->get_yuv_palette_clamping && weed_palette_is_yuv(tmpvpp->palette)) {
        int *clampings = tmpvpp->get_yuv_palette_clamping(tmpvpp->palette);
        if (clampings[0] != -1)
          ctext = lives_strdup_printf("%s (%s)", weed_palette_get_name(tmpvpp->palette),
                                      weed_yuv_clamping_get_name(tmpvpp->YUV_clamping));
      }
      if (!ctext) ctext = lives_strdup(weed_palette_get_name(tmpvpp->palette));
      lives_entry_set_text(LIVES_ENTRY(vppa->pal_entry), ctext);
      lives_free(ctext);
      lives_list_free_all(&pal_list_strings);
    }
  }
  if (intention == LIVES_INTENTION_TRANSCODE) {
    vppa->apply_fx = lives_standard_check_button_new(_("Apply current realtime effects"),
                     FALSE, LIVES_BOX(dialog_vbox), NULL);
    if (mainw->event_list) lives_widget_set_no_show_all(widget_opts.last_container, TRUE);
  }

  // extra params
  if (tmpvpp->get_init_rfx) {
    LiVESWidget *vbox = lives_vbox_new(FALSE, 0);
    /* LiVESWidget *scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V / 2, vbox); */
    lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);
    plugin_run_param_window((*tmpvpp->get_init_rfx)(intention), LIVES_VBOX(vbox), &(vppa->rfx));
    if (intention != LIVES_INTENTION_TRANSCODE) {
      char *fnamex = lives_build_filename(prefs->workdir, vppa->rfx->name, NULL);
      if (lives_file_test(fnamex, LIVES_FILE_TEST_EXISTS))
        lives_rm(fnamex);
      lives_free(fnamex);
    }
    if (tmpvpp->extra_argv && tmpvpp->extra_argc > 0) {
      // update with defaults
      LiVESList *plist = argv_to_marshalled_list(vppa->rfx, tmpvpp->extra_argc, tmpvpp->extra_argv);
      param_demarshall(vppa->rfx, plist, FALSE, FALSE); // set defaults
      param_demarshall(vppa->rfx, plist, FALSE, TRUE); // update widgets
      lives_list_free_all(&plist);
    }
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(vppa->dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  if (intention == LIVES_INTENTION_PLAY) {
    savebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(vppa->dialog), LIVES_STOCK_SAVE_AS, NULL,
                 LIVES_RESPONSE_BROWSE);

    lives_widget_set_tooltip_text(savebutton, _("Save settings to an alternate file.\n"));
    lives_signal_connect(LIVES_GUI_OBJECT(savebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_vppa_save_clicked),
                         vppa);
  }

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(vppa->dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_vppa_cancel_clicked), vppa);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_vppa_ok_clicked), vppa);

  lives_widget_show_all(vppa->dialog);
  lives_window_present(LIVES_WINDOW(vppa->dialog));
  lives_xwindow_raise(lives_widget_get_xwindow(vppa->dialog));

  return vppa;
}


void close_vid_playback_plugin(_vid_playback_plugin *vpp) {
  register int i;

  if (vpp) {
    if (vpp == mainw->vpp) {
      lives_grab_remove(LIVES_MAIN_WINDOW_WIDGET);
      if (mainw->ext_playback) {
        pthread_mutex_lock(&mainw->vpp_stream_mutex);
        mainw->ext_audio = FALSE;
        mainw->ext_playback = FALSE;
        pthread_mutex_unlock(&mainw->vpp_stream_mutex);
        if (mainw->vpp->exit_screen) {
          (*mainw->vpp->exit_screen)(mainw->ptr_x, mainw->ptr_y);
        }
#ifdef RT_AUDIO
        stop_audio_stream();
#endif
        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)
          if (mainw->play_window && prefs->play_monitor == 0)
            lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window), FALSE);
      }
      mainw->stream_ticks = -1;
      mainw->vpp = NULL;
    }
    if (vpp->module_unload)(vpp->module_unload)();
    dlclose(vpp->handle);

    if (vpp->extra_argv) {
      for (i = 0; vpp->extra_argv[i]; i++) {
        lives_free(vpp->extra_argv[i]);
      }
      lives_free(vpp->extra_argv);
    }

    for (i = 0; i < vpp->num_play_params + vpp->num_alpha_chans; i++) {
      weed_plant_free(vpp->play_params[i]);
    }

    lives_freep((void **)&vpp->play_params);
    lives_free(vpp);
  }
}


const weed_plant_t *pp_get_param(weed_plant_t **pparams, int idx) {
  register int i = 0;
  while (pparams[i]) {
    if (WEED_PLANT_IS_PARAMETER(pparams[i])) {
      if (--idx < 0) return pparams[i];
    }
    i++;
  }
  return NULL;
}


const weed_plant_t *pp_get_chan(weed_plant_t **pparams, int idx) {
  register int i = 0;
  while (pparams[i]) {
    if (WEED_PLANT_IS_CHANNEL(pparams[i])) {
      if (--idx < 0) return pparams[i];
    }
    i++;
  }
  return NULL;
}


boolean vpp_try_match_palette(_vid_playback_plugin *vpp, weed_layer_t *layer) {
  int *pal_list, i = 0;
  if (vpp->get_palette_list && (pal_list = (*vpp->get_palette_list)())) {
    int palette = weed_layer_get_palette(layer);
    for (i = 0; pal_list[i] != WEED_PALETTE_END; i++) {
      if (pal_list[i] == palette) break;
    }
    if (pal_list[i] == WEED_PALETTE_END) {
      if (!i) return FALSE;
      palette = best_palette_match(pal_list, i, palette);
    }
    if (palette == WEED_PALETTE_END) return FALSE;
    if (palette != vpp->palette) {
      if (!(*vpp->set_palette)(palette)) {
        return FALSE;
      }
    }
    vpp->palette = palette;
    if (weed_palette_is_yuv(palette) && vpp->get_yuv_palette_clamping) {
      int *yuv_clamping_types = (*vpp->get_yuv_palette_clamping)(vpp->palette);
      int lclamping = weed_layer_get_yuv_clamping(layer);
      for (i = 0; yuv_clamping_types[i] != -1; i++) {
        if (yuv_clamping_types[i] == lclamping) {
          if ((*vpp->set_yuv_palette_clamping)(lclamping))
            vpp->YUV_clamping = lclamping;
          break;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
  return FALSE;
}


_vid_playback_plugin *open_vid_playback_plugin(const char *name, boolean in_use) {
  // this is called on startup or when the user selects a new playback plugin

  // if in_use is TRUE, it is our active vpp

  // TODO - if in_use, get fixed_fps,fwidth,fheight,palette,argc and argv from a file

  _vid_playback_plugin *vpp;
  char **array;
  const char *fps_list;
  const char *pl_error;
  void *handle;
  int *palette_list;
  char *msg, *tmp;
  char *plugname;
  int dlflags = RTLD_LAZY;
  boolean OK = TRUE;

  if (in_use && LIVES_IS_PLAYING && mainw->noswitch) {
    mainw->new_vpp = name;
    return NULL;
  }

  tmp = lives_strdup_printf("%s.%s", name, DLL_NAME);
  plugname = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_VID_PLAYBACK, tmp, NULL);
  lives_free(tmp);

  handle = dlopen(plugname, dlflags);

  if (!handle) {
    char *msg = lives_strdup_printf(_("\n\nFailed to open playback plugin %s\nError was %s\n"
                                      "Playback plugin will be disabled,\n"
                                      "it can be re-enabled in Preferences / Playback.\n"), plugname, dlerror());
    if (prefs->startup_phase != 1 && prefs->startup_phase != -1) {
      if (!prefsw) widget_opts.non_modal = TRUE;
      do_error_dialog(msg);
      widget_opts.non_modal = FALSE;
    }
    LIVES_ERROR(msg);
    lives_free(msg);
    lives_free(plugname);
    lives_snprintf(future_prefs->vpp_name, 64, "%s", mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
    set_vpp(TRUE);
    return NULL;
  }

  vpp = (_vid_playback_plugin *) lives_calloc(sizeof(_vid_playback_plugin), 1);

  vpp->play_paramtmpls = NULL;
  vpp->get_init_rfx = NULL;
  vpp->play_params = NULL;
  vpp->alpha_chans = NULL;
  vpp->num_play_params = vpp->num_alpha_chans = 0;
  vpp->extra_argv = NULL;

  if ((vpp->module_check_init = (const char *(*)())dlsym(handle, "module_check_init")) == NULL) {
    OK = FALSE;
  }
  if ((vpp->version = (const char *(*)())dlsym(handle, "version")) == NULL) {
    OK = FALSE;
  }
  if ((vpp->get_palette_list = (int *(*)())dlsym(handle, "get_palette_list")) == NULL) {
    OK = FALSE;
  }
  if ((vpp->set_palette = (boolean(*)(int))dlsym(handle, "set_palette")) == NULL) {
    OK = FALSE;
  }
  if ((vpp->get_capabilities = (uint64_t (*)(int))dlsym(handle, "get_capabilities")) == NULL) {
    OK = FALSE;
  }
  if ((vpp->render_frame = (boolean(*)(int, int, int64_t, void **, void **, weed_plant_t **))
                           dlsym(handle, "render_frame")) == NULL) {
    if ((vpp->play_frame = (boolean(*)(weed_layer_t *, ticks_t, weed_layer_t *))
                           dlsym(handle, "play_frame")) == NULL) {
      OK = FALSE;
    }
  }
  if ((vpp->get_fps_list = (const char *(*)(int))dlsym(handle, "get_fps_list"))) {
    if ((vpp->set_fps = (boolean(*)(double))dlsym(handle, "set_fps")) == NULL) {
      OK = FALSE;
    }
  }

  if (!OK) {
    char *msg = lives_strdup_printf
                (_("\n\nPlayback module %s\nis missing a mandatory function.\nUnable to use it.\n"), plugname);
    set_string_pref(PREF_VID_PLAYBACK_PLUGIN, "none");
    do_error_dialog(msg);
    lives_free(msg);
    dlclose(handle);
    lives_free(vpp);
    vpp = NULL;
    lives_free(plugname);
    return NULL;
  }

  if ((pl_error = (*vpp->module_check_init)())) {
    msg = lives_strdup_printf(_("Video playback plugin failed to initialise.\nError was: %s\n"), pl_error);
    if (prefs->startup_phase != 1 && prefs->startup_phase != -1) {
      do_error_dialog(msg);
    } else {
      LIVES_ERROR(msg);
    }
    lives_free(msg);
    dlclose(handle);
    lives_free(vpp);
    vpp = NULL;
    lives_free(plugname);
    return NULL;
  }

  // now check for optional functions
  vpp->weed_setup = (weed_plant_t *(*)(weed_bootstrap_f))dlsym(handle, "weed_setup");

  if (vpp->weed_setup) {
    weed_set_host_info_callback(host_info_cb, LIVES_INT_TO_POINTER(100));
    (*vpp->weed_setup)(weed_bootstrap);
  }

  vpp->get_description = (const char *(*)())dlsym(handle, "get_description");
  vpp->get_init_rfx = (const char *(*)())dlsym(handle, "get_init_rfx");

  vpp->get_play_params = (const weed_plant_t **(*)(weed_bootstrap_f))dlsym(handle, "get_play_params");

  vpp->get_yuv_palette_clamping = (int *(*)(int))dlsym(handle, "get_yuv_palette_clamping");
  vpp->set_yuv_palette_clamping = (int (*)(int))dlsym(handle, "set_yuv_palette_clamping");
  vpp->get_audio_fmts = (int *(*)())dlsym(handle, "get_audio_fmts");
  vpp->init_screen = (boolean(*)(int, int, boolean, uint64_t, int, char **))dlsym(handle, "init_screen");
  vpp->init_audio = (boolean(*)(int, int, int, char **))dlsym(handle, "init_audio");
  vpp->render_audio_frame_float = (boolean(*)(float **, int))dlsym(handle, "render_audio_frame_float");
  vpp->exit_screen = (void (*)(uint16_t, uint16_t))dlsym(handle, "exit_screen");
  vpp->module_unload = (void (*)())dlsym(handle, "module_unload");

  vpp->YUV_sampling = 0;
  vpp->YUV_subspace = 0;

  palette_list = (*vpp->get_palette_list)();

  if (future_prefs->vpp_argv) {
    vpp->palette = future_prefs->vpp_palette;
    vpp->YUV_clamping = future_prefs->vpp_YUV_clamping;
  } else {
    if (!in_use && mainw->vpp && !(strcmp(name, mainw->vpp->name))) {
      vpp->palette = mainw->vpp->palette;
      vpp->YUV_clamping = mainw->vpp->YUV_clamping;
    } else {
      vpp->palette = palette_list[0];
      vpp->YUV_clamping = -1;
    }
  }

  vpp->audio_codec = AUDIO_CODEC_NONE;
  vpp->capabilities = (*vpp->get_capabilities)(vpp->palette);

  if (vpp->capabilities & VPP_CAN_RESIZE && vpp->capabilities & VPP_LOCAL_DISPLAY) {
    vpp->fwidth = vpp->fheight = -1;
  } else {
    vpp->fwidth = vpp->fheight = 0;
  }
  if (future_prefs->vpp_argv) {
    vpp->fwidth = future_prefs->vpp_fwidth;
    vpp->fheight = future_prefs->vpp_fheight;
  } else if (!in_use && mainw->vpp && !(strcmp(name, mainw->vpp->name))) {
    vpp->fwidth = mainw->vpp->fwidth;
    vpp->fheight = mainw->vpp->fheight;
  }
  if (vpp->fwidth == -1 && !(vpp->capabilities & VPP_CAN_RESIZE && vpp->capabilities & VPP_LOCAL_DISPLAY)) {
    vpp->fwidth = vpp->fheight = 0;
  }

  vpp->fixed_fpsd = -1.;
  vpp->fixed_fps_numer = 0;

  if (future_prefs->vpp_argv) {
    vpp->fixed_fpsd = future_prefs->vpp_fixed_fpsd;
    vpp->fixed_fps_numer = future_prefs->vpp_fixed_fps_numer;
    vpp->fixed_fps_denom = future_prefs->vpp_fixed_fps_denom;
  } else if (!in_use && mainw->vpp && !(strcmp(name, mainw->vpp->name))) {
    vpp->fixed_fpsd = mainw->vpp->fixed_fpsd;
    vpp->fixed_fps_numer = mainw->vpp->fixed_fps_numer;
    vpp->fixed_fps_denom = mainw->vpp->fixed_fps_denom;
  }

  vpp->handle = handle;
  lives_snprintf(vpp->name, 256, "%s", name);

  if (future_prefs->vpp_argv) {
    vpp->extra_argc = future_prefs->vpp_argc;
    vpp->extra_argv = (char **)lives_calloc((vpp->extra_argc + 1), (sizeof(char *)));
    for (register int i = 0; i <= vpp->extra_argc; i++) vpp->extra_argv[i] = lives_strdup(future_prefs->vpp_argv[i]);
  } else {
    if (!in_use && mainw->vpp && !(strcmp(name, mainw->vpp->name))) {
      vpp->extra_argc = mainw->vpp->extra_argc;
      vpp->extra_argv = (char **)lives_calloc((mainw->vpp->extra_argc + 1), (sizeof(char *)));
      for (register int i = 0; i <= vpp->extra_argc; i++) vpp->extra_argv[i] = lives_strdup(mainw->vpp->extra_argv[i]);
    } else {
      vpp->extra_argc = 0;
      vpp->extra_argv = (char **)lives_malloc(sizeof(char *));
      vpp->extra_argv[0] = NULL;
    }
  }
  // see if plugin is using fixed fps

  if (vpp->fixed_fpsd <= 0. && vpp->get_fps_list) {
    // fixed fps

    if ((fps_list = (*vpp->get_fps_list)(vpp->palette))) {
      array = lives_strsplit(fps_list, "|", -1);
      if (get_token_count(array[0], ':') > 1) {
        char **array2 = lives_strsplit(array[0], ":", 2);
        vpp->fixed_fps_numer = atoi(array2[0]);
        vpp->fixed_fps_denom = atoi(array2[1]);
        lives_strfreev(array2);
        vpp->fixed_fpsd = get_ratio_fps(array[0]);
      } else {
        vpp->fixed_fpsd = lives_strtod(array[0], NULL);
        vpp->fixed_fps_numer = 0;
      }
      lives_strfreev(array);
    }
  }

  if (vpp->YUV_clamping == -1) {
    vpp->YUV_clamping = WEED_YUV_CLAMPING_CLAMPED;

    if (vpp->get_yuv_palette_clamping && weed_palette_is_yuv(vpp->palette)) {
      int *yuv_clamping_types = (*vpp->get_yuv_palette_clamping)(vpp->palette);
      if (yuv_clamping_types[0] != -1) vpp->YUV_clamping = yuv_clamping_types[0];
    }
  }

  if (vpp->get_audio_fmts && mainw->is_ready) vpp->audio_codec = get_best_audio(vpp);
  if (prefsw) {
    prefsw_set_astream_settings(vpp, prefsw);
    prefsw_set_rec_after_settings(vpp, prefsw);
  }

  /// get the play parameters (and alpha channels) if any and convert to weed params
  if (vpp->get_play_params) {
    vpp->play_paramtmpls = (*vpp->get_play_params)(NULL);
  }

  // create vpp->play_params
  if (vpp->play_paramtmpls) {
    register int i;
    weed_plant_t *ptmpl;
    for (i = 0; (ptmpl = (weed_plant_t *)vpp->play_paramtmpls[i]); i++) {
      vpp->play_params = (weed_plant_t **)lives_realloc(vpp->play_params, (i + 2) * sizeof(weed_plant_t *));
      if (WEED_PLANT_IS_PARAMETER_TEMPLATE(ptmpl)) {
        // is param template, create a param
        vpp->play_params[i] = weed_plant_new(WEED_PLANT_PARAMETER);
        weed_leaf_copy(vpp->play_params[i], WEED_LEAF_VALUE, ptmpl, WEED_LEAF_DEFAULT);
        weed_set_plantptr_value(vpp->play_params[i], WEED_LEAF_TEMPLATE, ptmpl);
        vpp->num_play_params++;
      } else {
        // must be an alpha channel
        vpp->play_params[i] = weed_plant_new(WEED_PLANT_CHANNEL);
        weed_set_plantptr_value(vpp->play_params[i], WEED_LEAF_TEMPLATE, ptmpl);
        vpp->num_alpha_chans++;
      }
    }
    vpp->play_params[i] = NULL;
  }

  if (!in_use) return vpp;

  if (!mainw->is_ready) {
    double fixed_fpsd = vpp->fixed_fpsd;
    int fwidth = vpp->fwidth;
    int fheight = vpp->fheight;

    mainw->vpp = vpp;
    load_vpp_defaults(vpp, mainw->vpp_defs_file);
    if (fixed_fpsd < 0.) vpp->fixed_fpsd = fixed_fpsd;
    if (fwidth < 0) vpp->fwidth = fwidth;
    if (fheight < 0) vpp->fheight = fheight;
  }

  if (!(*vpp->set_palette)(vpp->palette)) {
    do_vpp_palette_error();
    close_vid_playback_plugin(vpp);
    lives_free(plugname);
    return NULL;
  }

  if (vpp->set_yuv_palette_clamping)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

  if (vpp->get_fps_list) {
    if (mainw->fixed_fpsd > 0. || (vpp->fixed_fpsd > 0. && vpp->set_fps &&
                                   !((*vpp->set_fps)(vpp->fixed_fpsd)))) {
      do_vpp_fps_error();
      vpp->fixed_fpsd = -1.;
      vpp->fixed_fps_numer = 0;
    }
  }

  cached_key = cached_mod = 0;

  // TODO: - support other YUV subspaces
  d_print(_("*** Using %s plugin for fs playback, agreed to use palette type %d ( %s ). ***\n"), name,
          vpp->palette, (tmp = weed_palette_get_name_full(vpp->palette, vpp->YUV_clamping,
                               WEED_YUV_SUBSPACE_YCBCR)));
  lives_free(tmp);

  lives_free(plugname);

  if (mainw->is_ready && in_use && mainw->vpp) {
    close_vid_playback_plugin(mainw->vpp);
  }

  return vpp;
}


void vid_playback_plugin_exit(void) {
  // external plugin
  if (mainw->ext_playback) {
    pthread_mutex_lock(&mainw->vpp_stream_mutex);
    mainw->ext_audio = FALSE;
    pthread_mutex_unlock(&mainw->vpp_stream_mutex);
    lives_grab_remove(LIVES_MAIN_WINDOW_WIDGET);

    if (mainw->vpp->exit_screen) {
      (*mainw->vpp->exit_screen)(mainw->ptr_x, mainw->ptr_y);
    }
#ifdef RT_AUDIO
    stop_audio_stream();
#endif
    mainw->ext_playback = FALSE;
    if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)
      if (mainw->play_window && prefs->play_monitor == 0)
        lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window), FALSE);
  }
  mainw->stream_ticks = -1;

  if (LIVES_IS_PLAYING && mainw->fs && mainw->sep_win) lives_window_fullscreen(LIVES_WINDOW(mainw->play_window));
  if (mainw->play_window) {
    play_window_set_title();
  }
}


int64_t get_best_audio(_vid_playback_plugin * vpp) {
  // find best audio from video plugin list, matching with audiostream plugins

  // i.e. cross-check video list with astreamer list

  // only for plugins which want to stream audiom but dont provide a render_audio_frame()

  int *fmts, *sfmts;
  int ret = AUDIO_CODEC_NONE;
  int i, j = 0, nfmts;
  char *astreamer, *com;
  char buf[1024];
  char **array;

  if (vpp && vpp->get_audio_fmts) {
    fmts = (*vpp->get_audio_fmts)(); // const, so do not free()

    // make audiostream plugin name
    astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, AUDIO_STREAMER_NAME, NULL);

    // create sfmts array and nfmts

    com = lives_strdup_printf("\"%s\" get_formats", astreamer);
    lives_popen(com, FALSE, buf, 1024);
    lives_free(com);

    nfmts = get_token_count(buf, '|');
    array = lives_strsplit(buf, "|", nfmts);
    sfmts = (int *)lives_calloc(nfmts, sizint);

    for (i = 0; i < nfmts; i++) {
      if (array[i] && *array[i]) sfmts[j++] = atoi(array[i]);
    }

    nfmts = j;
    lives_strfreev(array);

    for (i = 0; fmts[i] != -1; i++) {
      // traverse video list and see if audiostreamer supports each one
      if (int_array_contains_value(sfmts, nfmts, fmts[i])) {

        com = lives_strdup_printf("\"%s\" check %d", astreamer, fmts[i]);
        lives_popen(com, FALSE, buf, 1024);
        lives_free(com);

        if (THREADVAR(com_failed)) {
          lives_free(astreamer);
          lives_free(com);
          lives_free(sfmts);
          return ret;
        }

        if (*buf) {
          if (i == 0 && prefsw) {
            do_error_dialog(buf);
            d_print(_("Audio stream unable to use preferred format '%s'\n"), anames[fmts[i]]);
          }
          continue;
        }

        if (i > 0 && prefsw) {
          d_print(_("Using format '%s' instead.\n"), anames[fmts[i]]);
        }
        ret = fmts[i];
        break;
      }
    }

    if (fmts[i] == -1) {
      //none suitable, stick with first
      for (i = 0; fmts[i] != -1; i++) {
        // traverse video list and see if audiostreamer supports each one
        if (int_array_contains_value(sfmts, nfmts, fmts[i])) {
          ret = fmts[i];
          break;
        }
      }
    }

    lives_free(sfmts);
    lives_free(astreamer);
  }

  return ret;
}


///////////////////////
// encoder plugins

void do_plugin_encoder_error(const char *plugin_name) {
  char *msg, *tmp;

  if (!plugin_name) {
    msg = lives_strdup_printf(
            _("LiVES was unable to find its encoder plugins. Please make sure you have the plugins installed in\n"
              "%s%s%s\nor change the value of <lib_dir> in %s\n"),
            prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_ENCODERS,
            (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
    lives_free(tmp);
    widget_opts.non_modal = TRUE;
    do_error_dialog(msg);
    lives_free(msg);
    return;
  }

  msg = lives_strdup_printf(
          _("LiVES did not receive a response from the encoder plugin called '%s'.\n"
            "Please make sure you have that plugin installed correctly in\n%s%s%s\n"
            "or switch to another plugin using Tools|Preferences|Encoding\n"),
          plugin_name, prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_ENCODERS);
  do_error_dialog(msg);
  lives_free(msg);
}


boolean check_encoder_restrictions(boolean get_extension, boolean user_audio, boolean save_all) {
  LiVESList *ofmt_all = NULL;
  char **checks;
  char **array = NULL;
  char **array2;
  char aspect_buffer[512];

  // for auto resizing/resampling
  double best_fps = 0.;
  double fps;
  double best_fps_delta = 0.;

  boolean sizer = FALSE;
  boolean allow_aspect_override = FALSE;
  boolean calc_aspect = FALSE;
  boolean swap_endian = FALSE;

  int best_arate = 0;
  int width, owidth;
  int height, oheight;
  int best_arate_delta = 0;
  int hblock = 2, vblock = 2;
  int i, r, val;
  int pieces, numtok;
  int best_fps_num = 0, best_fps_denom = 0;
  int arate, achans, asampsize, asigned = 0;

  if (!rdet) {
    width = owidth = cfile->hsize;
    height = oheight = cfile->vsize;
    fps = cfile->fps;
  } else {
    width = owidth = rdet->width;
    height = oheight = rdet->height;
    fps = rdet->fps;
    rdet->suggestion_followed = FALSE;
  }

  if (mainw->osc_auto) {
    if (mainw->osc_enc_width > 0) {
      width = mainw->osc_enc_width;
      height = mainw->osc_enc_height;
    }
    if (mainw->osc_enc_fps != 0.) fps = mainw->osc_enc_fps;
  }

  // TODO - allow lists for size
  lives_snprintf(prefs->encoder.of_restrict, 5, "none");
  if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats"))) {
    // get any restrictions for the current format
    for (i = 0; i < lives_list_length(ofmt_all); i++) {
      if ((numtok = get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|')) > 2) {
        array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);
        if (!strcmp(array[0], prefs->encoder.of_name)) {
          if (numtok > 5) {
            lives_snprintf(prefs->encoder.ptext, 512, "%s", array[5]);
          } else {
            lives_memset(prefs->encoder.ptext, 0, 1);
          }
          if (numtok > 4) {
            lives_snprintf(prefs->encoder.of_def_ext, 16, "%s", array[4]);
          } else {
            lives_memset(prefs->encoder.of_def_ext, 0, 1);
          }
          if (numtok > 3) {
            lives_snprintf(prefs->encoder.of_restrict, 128, "%s", array[3]);
          } else {
            lives_snprintf(prefs->encoder.of_restrict, 128, "none");
          }
          prefs->encoder.of_allowed_acodecs = atoi(array[2]);
          lives_list_free_all(&ofmt_all);
          lives_strfreev(array);
          break;
        }
        lives_strfreev(array);
      }
    }
  }

  if (get_extension) {
    return TRUE; // just wanted file extension
  }

  if (!rdet && mainw->save_with_sound && prefs->encoder.audio_codec != AUDIO_CODEC_NONE) {
    if (!(prefs->encoder.of_allowed_acodecs & (1 << prefs->encoder.audio_codec))) {
      do_encoder_acodec_error();
      return FALSE;
    }
  }

  if (user_audio && future_prefs->encoder.of_allowed_acodecs == 0) best_arate = -1;

  if ((!*prefs->encoder.of_restrict || !strcmp(prefs->encoder.of_restrict, "none")) && best_arate > -1) {
    return TRUE;
  }

  if (!rdet) {
    arate = cfile->arate;
    achans = cfile->achans;
    asampsize = cfile->asampsize;
  } else {
    arate = rdet->arate;
    achans = rdet->achans;
    asampsize = rdet->asamps;
  }

  // audio endianness check - what should we do for big-endian machines ?
  if (((mainw->save_with_sound || rdet) && (!resaudw || resaudw->aud_checkbutton ||
       lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton))))
      && prefs->encoder.audio_codec != AUDIO_CODEC_NONE && arate != 0 && achans != 0 && asampsize != 0) {
    if (rdet && !rdet->is_encoding) {
      if (mainw->endian != AFORM_BIG_ENDIAN && (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))))
        swap_endian = TRUE;
    } else {
      if (mainw->endian != AFORM_BIG_ENDIAN && (cfile->signed_endian & AFORM_BIG_ENDIAN)) swap_endian = TRUE;
      //if (mainw->endian==AFORM_BIG_ENDIAN && (cfile->signed_endian&AFORM_BIG_ENDIAN)) swap_endian=TRUE; // needs test
    }
  }

  if (*prefs->encoder.of_restrict) {
    pieces = get_token_count(prefs->encoder.of_restrict, ',');
    checks = lives_strsplit(prefs->encoder.of_restrict, ",", pieces);

    for (r = 0; r < pieces; r++) {
      // check each restriction in turn

      if (!strncmp(checks[r], "fps=", 4)) {
        double allowed_fps;
        int mbest_num = 0, mbest_denom = 0;
        int numparts;
        char *fixer;

        best_fps_delta = 1000000000.;
        array = lives_strsplit(checks[r], "=", 2);
        numtok = get_token_count(array[1], ';');
        array2 = lives_strsplit(array[1], ";", numtok);
        for (i = 0; i < numtok; i++) {
          mbest_num = mbest_denom = 0;
          if ((numparts = get_token_count(array2[i], ':')) > 1) {
            char **array3 = lives_strsplit(array2[i], ":", 2);
            mbest_num = atoi(array3[0]);
            mbest_denom = atoi(array3[1]);
            lives_strfreev(array3);
            if (mbest_denom == 0) continue;
            allowed_fps = (mbest_num * 1.) / (mbest_denom * 1.);
          } else allowed_fps = lives_strtod(array2[i], NULL);

          // convert to 8dp
          fixer = lives_strdup_printf("%.8f %.8f", allowed_fps, fps);
          lives_free(fixer);

          if (allowed_fps >= fps) {
            if (allowed_fps - fps < best_fps_delta) {
              best_fps_delta = allowed_fps - fps;
              if (mbest_denom > 0) {
                best_fps_num = mbest_num;
                best_fps_denom = mbest_denom;
                best_fps = 0.;
                if (!rdet) cfile->ratio_fps = TRUE;
                else rdet->ratio_fps = TRUE;
              } else {
                best_fps_num = best_fps_denom = 0;
                best_fps = allowed_fps;
                if (!rdet) cfile->ratio_fps = FALSE;
                else rdet->ratio_fps = FALSE;
              }
            }
          } else if ((best_fps_denom == 0 && allowed_fps > best_fps) || (best_fps_denom > 0
                     && allowed_fps > (best_fps_num * 1.) /
                     (best_fps_denom * 1.))) {
            best_fps_delta = fps - allowed_fps;
            if (mbest_denom > 0) {
              best_fps_num = mbest_num;
              best_fps_denom = mbest_denom;
              best_fps = 0.;
              if (!rdet) cfile->ratio_fps = TRUE;
              else rdet->ratio_fps = TRUE;
            } else {
              best_fps = allowed_fps;
              best_fps_num = best_fps_denom = 0;
              if (!rdet) cfile->ratio_fps = FALSE;
              else rdet->ratio_fps = FALSE;
            }
          }
          if (best_fps_delta <= prefs->fps_tolerance) {
            best_fps_delta = 0.;
            best_fps_denom = best_fps_num = 0;
          }
          if (best_fps_delta == 0.) break;
        }
        lives_strfreev(array);
        lives_strfreev(array2);
        continue;
      }

      if (!strncmp(checks[r], "size=", 5)) {
        // TODO - allow list for size
        array = lives_strsplit(checks[r], "=", 2);
        array2 = lives_strsplit(array[1], "x", 2);
        width = atoi(array2[0]);
        height = atoi(array2[1]);
        lives_strfreev(array2);
        lives_strfreev(array);
        sizer = TRUE;
        continue;
      }

      if (!strncmp(checks[r], "minw=", 5)) {
        array = lives_strsplit(checks[r], "=", 2);
        val = atoi(array[1]);
        if (width < val) width = val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "minh=", 5)) {
        array = lives_strsplit(checks[r], "=", 2);
        val = atoi(array[1]);
        if (height < val) height = val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "maxh=", 5)) {
        array = lives_strsplit(checks[r], "=", 2);
        val = atoi(array[1]);
        if (height > val) height = val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "maxw=", 5)) {
        array = lives_strsplit(checks[r], "=", 2);
        val = atoi(array[1]);
        if (width > val) width = val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "asigned=", 8) &&
          ((mainw->save_with_sound || rdet) && (!resaudw ||
              !resaudw->aud_checkbutton ||
              lives_toggle_button_get_active
              (LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)))) &&
          prefs->encoder.audio_codec != AUDIO_CODEC_NONE
          && arate != 0 && achans != 0 && asampsize != 0) {
        array = lives_strsplit(checks[r], "=", 2);
        if (!strcmp(array[1], "signed")) {
          asigned = 1;
        }

        if (!strcmp(array[1], "unsigned")) {
          asigned = 2;
        }

        lives_strfreev(array);

        if (asigned != 0 && !capable->has_sox_sox) {
          do_encoder_sox_error();
          lives_strfreev(checks);
          return FALSE;
        }
        continue;
      }

      if (!strncmp(checks[r], "arate=", 6) && ((mainw->save_with_sound || rdet) && (!resaudw ||
          !resaudw->aud_checkbutton ||
          lives_toggle_button_get_active
          (LIVES_TOGGLE_BUTTON
           (resaudw->aud_checkbutton)))) &&
          prefs->encoder.audio_codec != AUDIO_CODEC_NONE && arate != 0 && achans != 0 && asampsize != 0) {
        // we only perform this test if we are encoding with audio
        // find next highest allowed rate from list,
        // if none are higher, use the highest
        int allowed_arate;
        best_arate_delta = 1000000000;

        array = lives_strsplit(checks[r], "=", 2);
        numtok = get_token_count(array[1], ';');
        array2 = lives_strsplit(array[1], ";", numtok);
        for (i = 0; i < numtok; i++) {
          allowed_arate = atoi(array2[i]);
          if (allowed_arate >= arate) {
            if (allowed_arate - arate < best_arate_delta) {
              best_arate_delta = allowed_arate - arate;
              best_arate = allowed_arate;
            }
          } else if (allowed_arate > best_arate) best_arate = allowed_arate;
        }
        lives_strfreev(array2);
        lives_strfreev(array);

        if (!capable->has_sox_sox) {
          do_encoder_sox_error();
          lives_strfreev(checks);
          return FALSE;
        }
        continue;
      }

      if (!strncmp(checks[r], "hblock=", 7)) {
        // width must be a multiple of this
        array = lives_strsplit(checks[r], "=", 2);
        hblock = atoi(array[1]);
        width = (int)(width / hblock + .5) * hblock;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "vblock=", 7)) {
        // height must be a multiple of this
        array = lives_strsplit(checks[r], "=", 2);
        vblock = atoi(array[1]);
        height = (int)(height / vblock + .5) * vblock;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r], "aspect=", 7)) {
        // we calculate the nearest smaller frame size using aspect,
        // hblock and vblock
        calc_aspect = TRUE;
        array = lives_strsplit(checks[r], "=", 2);
        lives_snprintf(aspect_buffer, 512, "%s", array[1]);
        lives_strfreev(array);
        continue;
      }
    }

    /// end restrictions
    lives_strfreev(checks);

    if (!mainw->osc_auto && calc_aspect && !sizer) {
      // we calculate this last, after getting hblock and vblock sizes
      char **array3;
      double allowed_aspect;
      int xwidth = width;
      int xheight = height;

      width = height = 1000000;

      numtok = get_token_count(aspect_buffer, ';');
      array2 = lives_strsplit(aspect_buffer, ";", numtok);

      // see if we can get a width:height which is nearer an aspect than
      // current width:height

      for (i = 0; i < numtok; i++) {
        array3 = lives_strsplit(array2[i], ":", 2);
        allowed_aspect = lives_strtod(array3[0], NULL) / lives_strtod(array3[1], NULL);
        lives_strfreev(array3);
        minimise_aspect_delta(allowed_aspect, hblock, vblock, xwidth, xheight, &width, &height);
      }
      lives_strfreev(array2);

      // allow override if current width and height are integer multiples of blocks
      if (owidth % hblock == 0 && oheight % vblock == 0) allow_aspect_override = TRUE;

      // end recheck
    }

    // fps can't be altered if we have a multitrack event_list
    if (mainw->multitrack && mainw->multitrack->event_list) best_fps_delta = 0.;

    if (sizer) allow_aspect_override = FALSE;
  }

  // if we have min or max size, make sure we fit within that

  if (((width != owidth || height != oheight) && width * height > 0) || (best_fps_delta > 0.) || (best_arate_delta > 0 &&
      best_arate > 0) ||
      best_arate < 0 || asigned != 0 || swap_endian) {
    boolean ofx1_bool = mainw->fx1_bool;
    mainw->fx1_bool = FALSE;
    if ((width != owidth || height != oheight) && width * height > 0) {
      if (!capable->has_convert && !rdet && mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate == -1) {
        if (allow_aspect_override) {
          width = owidth;
          height = oheight;
        }
      }
    }
    if (rdet && !rdet->is_encoding) {
      rdet->arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
      rdet->achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
      rdet->asamps = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
      rdet->aendian = get_signed_endian(lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned)),
                                        lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend)));

      if (swap_endian || width != rdet->width || height != rdet->height || best_fps_delta != 0. || best_arate != rdet->arate ||
          ((asigned == 1 && (rdet->aendian & AFORM_UNSIGNED)) || (asigned == 2 && !(rdet->aendian & AFORM_SIGNED)))) {

        if (rdet_suggest_values(width, height, best_fps, best_fps_num, best_fps_denom, best_arate, asigned, swap_endian,
                                allow_aspect_override, (best_fps_delta == 0.))) {
          char *arate_string;
          rdet->width = width;
          rdet->height = height;
          if (best_arate != -1) rdet->arate = best_arate;
          else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), FALSE);

          if (asigned == 1) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
          else if (asigned == 2) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned), TRUE);

          if (swap_endian) {
            if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend)))
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend), TRUE);
            else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend), TRUE);
          }

          if (best_fps_delta > 0.) {
            if (best_fps_denom > 0) {
              rdet->fps = (best_fps_num * 1.) / (best_fps_denom * 1.);
            } else rdet->fps = best_fps;
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_fps), rdet->fps);
          }
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_width), rdet->width);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_height), rdet->height);
          if (best_arate != -1) {
            arate_string = lives_strdup_printf("%d", best_arate);
            lives_entry_set_text(LIVES_ENTRY(resaudw->entry_arate), arate_string);
            lives_free(arate_string);
          }
          rdet->suggestion_followed = TRUE;
          return TRUE;
        }
      }
      return FALSE;
    }

    if (mainw->osc_auto || do_encoder_restrict_dialog(width, height, best_fps, best_fps_num, best_fps_denom, best_arate,
        asigned, swap_endian, allow_aspect_override, save_all)) {
      if (!mainw->fx1_bool && mainw->osc_enc_width == 0) {
        width = owidth;
        height = oheight;
      }

      if (!auto_resample_resize(width, height, best_fps, best_fps_num, best_fps_denom, best_arate, asigned, swap_endian)) {
        mainw->fx1_bool = ofx1_bool;
        return FALSE;
      }
    } else {
      mainw->fx1_bool = ofx1_bool;
      return FALSE;
    }
  }
  return TRUE;
}


LiVESList *filter_encoders_by_img_ext(LiVESList * encoders, const char *img_ext) {
  LiVESList *encoder_capabilities = NULL;
  LiVESList *list = encoders, *listnext;
  int caps;

  register int i;

  char *blacklist[] = {
    NULL,
    NULL
  };

  // something broke as of python 2.7.2, and python 3 files now just hang
  if (capable->python_version < 3000000) blacklist[0] = lives_strdup("multi_encoder3");

  while (list) {
    boolean skip = FALSE;
    i = 0;

    listnext = list->next;

    while (blacklist[i]) {
      if (!strcmp((const char *)list->data, blacklist[i])) {
        // skip blacklisted encoders
        lives_free((livespointer)list->data);
        encoders = lives_list_delete_link(encoders, list);
        skip = TRUE;
        break;
      }
      i++;
    }
    if (skip) {
      list = listnext;
      continue;
    }

    if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) {
      list = listnext;
      continue;
    }

    if ((encoder_capabilities = plugin_request(PLUGIN_ENCODERS, (char *)list->data, "get_capabilities")) == NULL) {
      lives_free((livespointer)list->data);
      encoders = lives_list_delete_link(encoders, list);
    } else {
      caps = atoi((char *)lives_list_nth_data(encoder_capabilities, 0));
      if (!(caps & CAN_ENCODE_PNG) && !strcmp(img_ext, LIVES_FILE_EXT_PNG)) {
        lives_free((livespointer)list->data);
        encoders = lives_list_delete_link(encoders, list);
      }

      lives_list_free_all(&encoder_capabilities);
    }

    list = listnext;
  }

  for (i = 0; blacklist[i]; i++) lives_free(blacklist[i]);

  return encoders;
}


//////////////////////////////////////////////////////
// decoder plugins

boolean decoder_plugin_move_to_first(const char *name) {
  LiVESList *decoder_plugin, *last_decoder_plugin = NULL;
  decoder_plugin = mainw->decoder_list;
  while (decoder_plugin) {
    if (!strcmp((const char *)(decoder_plugin->data), name)) {
      if (last_decoder_plugin) {
        last_decoder_plugin->next = decoder_plugin->next;
        decoder_plugin->next = mainw->decoder_list;
        mainw->decoder_list = decoder_plugin;
      }
      return TRUE;
    }
    last_decoder_plugin = decoder_plugin;
    decoder_plugin = decoder_plugin->next;
  }
  return FALSE;
}



LiVESList *load_decoders(void) {
  lives_decoder_sys_t *dplug;
  char *decplugdir = lives_strdup_printf("%s%s%s", prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_DECODERS);
  LiVESList *dlist = NULL;
  LiVESList *decoder_plugins_o = get_plugin_list(PLUGIN_DECODERS, TRUE, decplugdir, "-" DLL_NAME);
  LiVESList *decoder_plugins = decoder_plugins_o;

  char *blacklist[3] = {
    "zyavformat_decoder",
    "ogg_theora_decoder",
    NULL
  };

  const char *dplugname;
  boolean skip;

  register int i;

  while (decoder_plugins) {
    skip = FALSE;
    dplugname = (const char *)(decoder_plugins->data);
    for (i = 0; blacklist[i]; i++) {
      if (!strcmp(dplugname, blacklist[i])) {
        // skip blacklisted decoders
        skip = TRUE;
        break;
      }
    }
    if (!skip) {
      dplug = open_decoder_plugin((char *)decoder_plugins->data);
      if (dplug) dlist = lives_list_append(dlist, (livespointer)dplug);
    }
    lives_free((livespointer)decoder_plugins->data);
    decoder_plugins = decoder_plugins->next;
  }

  lives_list_free(decoder_plugins_o);

  if (!dlist) {
    char *msg = lives_strdup_printf(_("\n\nNo decoders found in %s !\n"), decplugdir);
    LIVES_WARN(msg);
    d_print(msg);
    lives_free(msg);
  }

  lives_free(decplugdir);
  return dlist;
}


static void set_cdata_memfuncs(lives_clip_data_t *cdata) {
  // set specific memory functions for decoder plugins to use
  static malloc_f  ext_malloc  = (malloc_f)  _ext_malloc;
  static free_f    ext_free    = (free_f)    _ext_free;
  static memcpy_f  ext_memcpy  = (memcpy_f)  _ext_memcpy;
  static memset_f  ext_memset = (memset_f)  _ext_memset;
  static memmove_f ext_memmove = (memmove_f) _ext_memmove;
  static realloc_f ext_realloc = (realloc_f) _ext_realloc;
  static calloc_f  ext_calloc  = (calloc_f)  _ext_calloc;
  if (!cdata) return;
  cdata->ext_malloc  = &ext_malloc;
  cdata->ext_free    = &ext_free;
  cdata->ext_memcpy  = &ext_memcpy;
  cdata->ext_memset  = &ext_memset;
  cdata->ext_memmove = &ext_memmove;
  cdata->ext_realloc = &ext_realloc;
  cdata->ext_calloc  = &ext_calloc;
}


static boolean sanity_check_cdata(lives_clip_data_t *cdata) {
  if (cdata->nframes <= 0 || cdata->nframes >= INT_MAX) {
    return FALSE;
  }

  // no usable palettes found
  if (cdata->palettes[0] == WEED_PALETTE_END) return FALSE;

  // all checks passed - OK
  return TRUE;
}


typedef struct {
  LiVESList *disabled;
  lives_decoder_t *dplug;
  lives_clip_t *sfile;
} tdp_data;

lives_decoder_t *clone_decoder(int fileno) {
  lives_decoder_t *dplug;
  const lives_decoder_sys_t *dpsys;
  lives_clip_data_t *cdata;

  if (!mainw->files[fileno] || !mainw->files[fileno]->ext_src) return NULL;

  cdata = ((lives_decoder_sys_t *)((lives_decoder_t *)mainw->files[fileno]->ext_src)->decoder)->get_clip_data
          (NULL, ((lives_decoder_t *)mainw->files[fileno]->ext_src)->cdata);

  if (!cdata) return NULL;

  dplug = (lives_decoder_t *)lives_calloc(1, sizeof(lives_decoder_t));
  dpsys = ((lives_decoder_t *)mainw->files[fileno]->ext_src)->decoder;

  dplug->decoder = dpsys;
  dplug->cdata = cdata;
  dplug->refs = 1;
  set_cdata_memfuncs((lives_clip_data_t *)cdata);
  cdata->rec_rowstrides = NULL;
  return dplug;
}


static lives_decoder_t *try_decoder_plugins(char *file_name, LiVESList * disabled, const lives_clip_data_t *fake_cdata) {
  // here we test each decoder in turn to see if it can open "file_name"

  // if we are reopening a clip, then fake cdata is a partially initialised cdata, but with only the frame count and fps set
  // this allows the decoder plugins to startup quicker as they don't have to seek to the last frame or calculate the fps.

  // we pass this to each decoder in turn and check what it returns. If the values look sane then we use that decoder,
  // otherwise we try the next one.

  // when reloading clips we try the decoder which last opened them first, otherwise they could get picked up by another
  // decoder and the frames could come out different

  lives_decoder_t *dplug = (lives_decoder_t *)lives_calloc(1, sizeof(lives_decoder_t));
  LiVESList *decoder_plugin = mainw->decoder_list;
  //LiVESList *last_decoder_plugin = NULL;

  dplug->refs = 1;

  if (fake_cdata) {
    set_cdata_memfuncs((lives_clip_data_t *)fake_cdata);
    //if (prefs->vj_mode) {
    ((lives_clip_data_t *)fake_cdata)->seek_flag = LIVES_SEEK_FAST;
    //}
  }

  while (decoder_plugin) {
    lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)decoder_plugin->data;
    if (lives_list_strcmp_index(disabled, dpsys->name, FALSE) != -1) {
      // check if (user) disabled this decoder
      decoder_plugin = decoder_plugin->next;
      continue;
    }

    //#define DEBUG_DECPLUG
#ifdef DEBUG_DECPLUG
    g_print("trying decoder %s\n", dpsys->name);
#endif

    dplug->cdata = (dpsys->get_clip_data)(file_name, fake_cdata);

    // use fake_cdata only on first (reloaded)
    fake_cdata = NULL;

    if (dplug->cdata) {
      // check for sanity
      //g_print("Checking return data from %s\n", dpsys->name);
      if (lsd_check_match((lives_struct_def_t *)get_lsd(LIVES_STRUCT_CLIP_DATA_T),
                          &dplug->cdata->lsd)) {
        g_printerr("Error in cdata received from decoder plugin:\n%s\nAborting.", dpsys->name);
        abort();
      }
      if (!sanity_check_cdata(dplug->cdata)) {
        //last_decoder_plugin = decoder_plugin;
        decoder_plugin = decoder_plugin->next;
        continue;
      }
      set_cdata_memfuncs(dplug->cdata);
      //////////////////////

      dplug->decoder = dpsys;

      /* if (strncmp(dpsys->name, "zz", 2)) { */
      /* 	// if libav decoder opened us, move it to the first, since it can save time */
      /* 	if (last_decoder_plugin) { */
      /* 	  last_decoder_plugin->next = decoder_plugin->next; */
      /* 	  decoder_plugin->next = mainw->decoder_list; */
      /* 	  mainw->decoder_list = decoder_plugin; */
      /* } */
      break;
    }
    //last_decoder_plugin = decoder_plugin;
    decoder_plugin = decoder_plugin->next;
  }

  if (!decoder_plugin) {
    lives_freep((void **)&dplug);
  } else {
    dplug->cdata->rec_rowstrides = NULL;
  }

  return dplug;
}


const lives_clip_data_t *get_decoder_cdata(int fileno, LiVESList * disabled, const lives_clip_data_t *fake_cdata) {
  // pass file to each decoder (demuxer) plugin in turn, until we find one that can parse
  // the file
  // NULL is returned if no decoder plugin recognises the file - then we
  // fall back to other methods

  // otherwise we return data for the clip as supplied by the decoder plugin

  // If the file does not exist, we set mainw->error=TRUE and return NULL

  // If we find a plugin we also set sfile->ext_src to point to a newly created decoder_plugin_t

  lives_decoder_t *dplug;

  LiVESList *xdisabled;

  lives_clip_t *sfile = mainw->files[fileno];

  char decplugname[PATH_MAX];

  boolean use_fake_cdata = FALSE;

  mainw->error = FALSE;

  if (!lives_file_test(sfile->file_name, LIVES_FILE_TEST_EXISTS)) {
    mainw->error = TRUE;
    return NULL;
  }

  lives_memset(decplugname, 0, 1);

  // check sfile->file_name against each decoder plugin,
  // until we get non-NULL cdata

  sfile->ext_src = NULL;
  sfile->ext_src_type = LIVES_EXT_SRC_NONE;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  xdisabled = lives_list_copy(disabled);

  if (fake_cdata) {
    get_clip_value(fileno, CLIP_DETAILS_DECODER_NAME, decplugname, PATH_MAX);
    if (*decplugname) {
      LiVESList *decoder_plugin = mainw->decoder_list;
      xdisabled = lives_list_remove(xdisabled, decplugname);
      while (decoder_plugin) {
        lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)decoder_plugin->data;
        if (!lives_strcmp(dpsys->name, decplugname)) {
          mainw->decoder_list = lives_list_move_to_first(mainw->decoder_list, decoder_plugin);
          use_fake_cdata = TRUE;
          break;
        }
        decoder_plugin = decoder_plugin->next;
      }
    }
  }

  /// TODO: background thread so we can animate GUI
  dplug = try_decoder_plugins((!fake_cdata || !use_fake_cdata) ? sfile->file_name
                              : NULL, xdisabled, use_fake_cdata ? fake_cdata : NULL);

  if (xdisabled) lives_list_free(xdisabled);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  if (dplug) {
    d_print(_(" using %s"), dplug->decoder->version());
    sfile->ext_src = dplug;
    sfile->ext_src_type = LIVES_EXT_SRC_DECODER;
    return dplug->cdata;
  }

  if (dplug) return dplug->cdata;
  return NULL;
}


// close one instance of dplug
void close_decoder_plugin(lives_decoder_t *dplug) {
  lives_clip_data_t *cdata;

  if (!dplug) return;

  dplug->refs--;
  if (dplug->refs) return;

  cdata = dplug->cdata;

  if (cdata) {
    if (cdata->rec_rowstrides) {
      lives_free(cdata->rec_rowstrides);
      cdata->rec_rowstrides = NULL;
    }
    (*dplug->decoder->clip_data_free)(cdata);
  }
  lives_free(dplug);
}


void close_clip_decoder(int clipno) {
  if (!IS_VALID_CLIP(clipno)) return;
  else {
    lives_clip_t *sfile = mainw->files[clipno];
    if (sfile->ext_src && sfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
      char *cwd = lives_get_current_dir();
      char *ppath = lives_build_filename(prefs->workdir, sfile->handle, NULL);
      lives_chdir(ppath, FALSE);
      lives_free(ppath);
      close_decoder_plugin((lives_decoder_t *)sfile->ext_src);
      sfile->ext_src = NULL;
      sfile->ext_src_type = LIVES_EXT_SRC_NONE;
      lives_chdir(cwd, FALSE);
      lives_free(cwd);
    }
  }
}


static void unload_decoder_plugin(lives_decoder_sys_t *dplug) {
  if (dplug->module_unload)(*dplug->module_unload)();

  lives_freep((void **)&dplug->name);

  dlclose(dplug->handle);
  lives_free(dplug);
}


void unload_decoder_plugins(void) {
  LiVESList *dplugs = mainw->decoder_list;

  while (dplugs) {
    unload_decoder_plugin((lives_decoder_sys_t *)dplugs->data);
    dplugs = dplugs->next;
  }

  lives_list_free(mainw->decoder_list);
  mainw->decoder_list = NULL;
  mainw->decoders_loaded = FALSE;
}


boolean chill_decoder_plugin(int fileno) {
  lives_clip_t *sfile = mainw->files[fileno];
  if (IS_NORMAL_CLIP(fileno) && sfile->clip_type == CLIP_TYPE_FILE && sfile->ext_src) {
    lives_decoder_t *dplug = (lives_decoder_t *)sfile->ext_src;
    lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)dplug->decoder;
    lives_clip_data_t *cdata = dplug->cdata;
    if (cdata)
      if (dpsys->chill_out) return (*dpsys->chill_out)(cdata);
  }
  return FALSE;
}


lives_decoder_sys_t *open_decoder_plugin(const char *plname) {
  lives_decoder_sys_t *dplug;
  char *plugname, *tmp;
  boolean OK = TRUE;
  const char *err;
  int dlflags = RTLD_NOW | RTLD_LOCAL;

  dplug = (lives_decoder_sys_t *)lives_calloc(1, sizeof(lives_decoder_sys_t));

  dplug->name = NULL;

  tmp = lives_strdup_printf("%s.%s", plname, DLL_NAME);
  plugname = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_DECODERS, tmp, NULL);
  lives_free(tmp);

#ifdef RTLD_DEEPBIND
  dlflags |= RTLD_DEEPBIND;
#endif

  dplug->handle = dlopen(plugname, dlflags);
  lives_free(plugname);

  if (!dplug->handle) {
    d_print(_("\n\nFailed to open decoder plugin %s\nError was %s\n"), plname, dlerror());
    lives_free(dplug);
    return NULL;
  }

  if ((dplug->version = (const char *(*)())dlsym(dplug->handle, "version")) == NULL) {
    OK = FALSE;
  }
  if ((dplug->get_clip_data = (lives_clip_data_t *(*)(char *, const lives_clip_data_t *))
                              dlsym(dplug->handle, "get_clip_data")) == NULL) {
    OK = FALSE;
  }
  if ((dplug->get_frame = (boolean(*)(const lives_clip_data_t *, int64_t, int *, int, void **))
                          dlsym(dplug->handle, "get_frame")) == NULL) {
    OK = FALSE;
  }
  if ((dplug->clip_data_free = (void (*)(lives_clip_data_t *))dlsym(dplug->handle, "clip_data_free")) == NULL) {
    OK = FALSE;
  }

  if (!OK) {
    d_print(_("\n\nDecoder plugin %s\nis missing a mandatory function.\nUnable to use it.\n"), plname);
    unload_decoder_plugin(dplug);
    lives_free(dplug);
    return NULL;
  }

  // optional
  dplug->module_check_init = (const char *(*)())dlsym(dplug->handle, "module_check_init");
  dplug->chill_out = (boolean(*)(const lives_clip_data_t *))dlsym(dplug->handle, "chill_out");
  dplug->set_palette = (boolean(*)(lives_clip_data_t *))dlsym(dplug->handle, "set_palette");
  dplug->module_unload = (void (*)())dlsym(dplug->handle, "module_unload");
  dplug->rip_audio = (int64_t (*)(const lives_clip_data_t *, const char *, int64_t, int64_t, unsigned char **))
                     dlsym(dplug->handle, "rip_audio");
  dplug->rip_audio_cleanup = (void (*)(const lives_clip_data_t *))dlsym(dplug->handle, "rip_audio_cleanup");

  if (dplug->module_check_init) {
    err = (*dplug->module_check_init)();

    if (err) {
      lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", err);
      unload_decoder_plugin(dplug);
      lives_free(dplug);
      return NULL;
    }
  }

  dplug->name = lives_strdup(plname);
  return dplug;
}


void get_mime_type(char *text, int maxlen, const lives_clip_data_t *cdata) {
  char *audname;

  if (!*cdata->container_name) lives_snprintf(text, maxlen, "%s", _("unknown"));
  else lives_snprintf(text, maxlen, "%s", cdata->container_name);

  if (!*cdata->video_name && !*cdata->audio_name) return;

  if (!*cdata->video_name) lives_strappend(text, maxlen, _("/unknown"));
  else {
    char *vidname = lives_strdup_printf("/%s", cdata->video_name);
    lives_strappend(text, maxlen, vidname);
    lives_free(vidname);
  }

  if (!*cdata->audio_name) {
    if (cfile->achans == 0) return;
    audname = lives_strdup_printf("/%s", _("unknown"));
  } else
    audname = lives_strdup_printf("/%s", cdata->audio_name);
  lives_strappend(text, maxlen, audname);
  lives_free(audname);
}


static void dpa_ok_clicked(LiVESButton * button, livespointer user_data) {
  lives_general_button_clicked(button, NULL);

  if (prefsw) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
    if (string_lists_differ(future_prefs->disabled_decoders, future_prefs->disabled_decoders_new))
      apply_button_set_enabled(NULL, NULL);
  }

  lives_list_free_all(&future_prefs->disabled_decoders);

  future_prefs->disabled_decoders = future_prefs->disabled_decoders_new;
}


static void dpa_cancel_clicked(LiVESButton * button, livespointer user_data) {
  lives_general_button_clicked(button, NULL);

  if (prefsw) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
  }

  lives_list_free_all(&future_prefs->disabled_decoders_new);
}


static void on_dpa_cb_toggled(LiVESToggleButton * button, const char *decname) {
  if (!lives_toggle_button_get_active(button))
    // unchecked is disabled
    future_prefs->disabled_decoders_new = lives_list_append(future_prefs->disabled_decoders_new, lives_strdup(decname));
  else
    future_prefs->disabled_decoders_new = lives_list_delete_string(future_prefs->disabled_decoders_new, decname);
}


void on_decplug_advanced_clicked(LiVESButton * button, livespointer user_data) {
  LiVESList *decoder_plugin;

  LiVESWidget *hbox;
  LiVESWidget *vbox;
  LiVESWidget *checkbutton;
  LiVESWidget *scrolledwindow;
  LiVESWidget *label;
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  char *ltext;
  char *decplugdir = lives_strdup_printf("%s%s%s", prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_DECODERS);

  if (!mainw->decoders_loaded) {
    mainw->decoder_list = load_decoders();
    mainw->decoders_loaded = TRUE;
  }

  decoder_plugin = mainw->decoder_list;

  dialog = lives_standard_dialog_new(_("Decoder Plugins"), FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  vbox = lives_vbox_new(FALSE, 0);

  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, vbox);

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrolledwindow);

  label = lives_standard_label_new(_("Enabled Video Decoders (uncheck to disable)"));
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);

  while (decoder_plugin) {
    lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)decoder_plugin->data;
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    ltext = lives_strdup_printf("%s   (%s)", dpsys->name, (*dpsys->version)());

    widget_opts.mnemonic_label = FALSE;
    checkbutton = lives_standard_check_button_new(ltext, lives_list_strcmp_index(future_prefs->disabled_decoders, dpsys->name,
                  FALSE) == -1,
                  LIVES_BOX(hbox), NULL);
    widget_opts.mnemonic_label = TRUE;

    lives_free(ltext);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_dpa_cb_toggled),
                                    (livespointer)dpsys->name);

    decoder_plugin = decoder_plugin->next;
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(dpa_cancel_clicked),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(dpa_ok_clicked),
                       NULL);

  lives_widget_show_all(dialog);
  lives_window_present(LIVES_WINDOW(dialog));
  lives_xwindow_raise(lives_widget_get_xwindow(dialog));

  future_prefs->disabled_decoders_new = lives_list_copy_strings(future_prefs->disabled_decoders);

  lives_free(decplugdir);
}

///////////////////////////////////////////////////////
// rfx plugin functions


boolean check_rfx_for_lives(lives_rfx_t *rfx) {
  // check that an RFX is suitable for loading (cf. check_for_lives in effects-weed.c)
  if (rfx->num_in_channels == 2 && rfx->props & RFX_PROPS_MAY_RESIZE) {
    d_print(_("Failed to load %s, transitions may not resize.\n"), rfx->name);
    return FALSE;
  }
  return TRUE;
}


void do_rfx_cleanup(lives_rfx_t *rfx) {
  char *com;
  char *dir = NULL;

  /// skip cleanup if menuentry is "apply current realtime effects"
  if (rfx == &mainw->rendered_fx[0]) return;

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    dir = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, NULL);
    com = lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"", prefs->backend_sync,
                              cfile->handle, cfile->start, cfile->end, dir,
                              PLUGIN_RENDERED_EFFECTS_BUILTIN, rfx->name);
    break;
  case RFX_STATUS_CUSTOM:
    com = lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"", prefs->backend_sync,
                              cfile->handle, cfile->start, cfile->end, prefs->config_datadir,
                              PLUGIN_RENDERED_EFFECTS_CUSTOM, rfx->name);
    break;
  case RFX_STATUS_TEST:
    com = lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"", prefs->backend_sync,
                              cfile->handle, cfile->start, cfile->end, prefs->config_datadir,
                              PLUGIN_RENDERED_EFFECTS_TEST, rfx->name);
    break;
  default:
    return;
  }

  lives_freep((void **)&dir);

  // if the command fails we just give a warning
  lives_system(com, FALSE);
  lives_free(com);
}


void render_fx_get_params(lives_rfx_t *rfx, const char *plugin_name, short status) {
  // create lives_param_t array from plugin supplied values
  LiVESList *parameter_list, *list;
  int param_idx, i;
  lives_param_t *cparam;
  char **param_array;
  char *line;
  int len;

  switch (status) {
  case RFX_STATUS_BUILTIN:
    parameter_list = plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_BUILTIN, plugin_name, "get_parameters");
    break;
  case RFX_STATUS_CUSTOM:
    parameter_list = plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_CUSTOM, plugin_name, "get_parameters");
    break;
  case RFX_STATUS_SCRAP:
    parameter_list = plugin_request_by_line(PLUGIN_RFX_SCRAP, plugin_name, "get_parameters");
    break;
  case RFX_STATUS_INTERNAL:
    parameter_list = plugin_request_by_line(PLUGIN_RFX_SCRAP, plugin_name, "get_parameters");
    break;
  default:
    parameter_list = plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_TEST, plugin_name, "get_parameters");
    break;
  }

  if (!parameter_list) {
    rfx->num_params = 0;
    rfx->params = NULL;
    return;
  }

  //threaded_dialog_spin(0.);
  rfx->num_params = lives_list_length(parameter_list);
  rfx->params = (lives_param_t *)lives_calloc(rfx->num_params, sizeof(lives_param_t));
  list = parameter_list;

  for (param_idx = 0; param_idx < rfx->num_params; param_idx++) {
    line = (char *)list->data;
    list = list->next;
    len = get_token_count(line, (unsigned int)rfx->delim[0]);

    if (len < 3) continue;

    param_array = lives_strsplit(line, rfx->delim, -1);

    cparam = &rfx->params[param_idx];
    cparam->name = lives_strdup(param_array[0]);
    cparam->label = lives_strdup(param_array[1]);
    cparam->desc = NULL;
    cparam->use_mnemonic = TRUE;
    cparam->hidden = 0;
    cparam->wrap = FALSE;
    cparam->transition = FALSE;
    cparam->step_size = 1.;
    cparam->group = 0;
    cparam->max = 0.;
    cparam->reinit = FALSE;
    cparam->changed = FALSE;
    cparam->edited = FALSE;
    cparam->change_blocked = FALSE;
    cparam->source = NULL;
    cparam->source_type = LIVES_RFX_SOURCE_RFX;
    cparam->special_type = LIVES_PARAM_SPECIAL_TYPE_NONE;
    cparam->special_type_index = 0;
    cparam->def = NULL;
    cparam->value = NULL;

#ifdef DEBUG_RENDER_FX_P
    lives_printerr("Got parameter %s\n", cparam->name);
#endif
    cparam->dp = 0;
    cparam->list = NULL;

    cparam->type = LIVES_PARAM_UNKNOWN;

    if (!strncmp(param_array[2], "num", 3)) {
      cparam->dp = atoi(param_array[2] + 3);
      cparam->type = LIVES_PARAM_NUM;
    } else if (!strncmp(param_array[2], "bool", 4)) {
      cparam->type = LIVES_PARAM_BOOL;
    } else if (!strncmp(param_array[2], "colRGB24", 8)) {
      cparam->type = LIVES_PARAM_COLRGB24;
    } else if (!strncmp(param_array[2], "string", 8)) {
      cparam->type = LIVES_PARAM_STRING;
    } else if (!strncmp(param_array[2], "string_list", 8)) {
      cparam->type = LIVES_PARAM_STRING_LIST;
    } else continue;

    if (cparam->dp) {
      double val;
      if (len < 6) continue;
      val = lives_strtod(param_array[3], NULL);
      cparam->value = lives_malloc(sizdbl);
      cparam->def = lives_malloc(sizdbl);
      set_double_param(cparam->def, val);
      set_double_param(cparam->value, val);
      cparam->min = lives_strtod(param_array[4], NULL);
      cparam->max = lives_strtod(param_array[5], NULL);
      if (len > 6) {
        cparam->step_size = lives_strtod(param_array[6], NULL);
        if (cparam->step_size == 0.) cparam->step_size = 1. / (double)lives_10pow(cparam->dp);
        else if (cparam->step_size < 0.) {
          cparam->step_size = -cparam->step_size;
          cparam->wrap = TRUE;
        }
      }
    } else if (cparam->type == LIVES_PARAM_COLRGB24) {
      short red;
      short green;
      short blue;
      if (len < 6) continue;
      red = (short)atoi(param_array[3]);
      green = (short)atoi(param_array[4]);
      blue = (short)atoi(param_array[5]);
      cparam->value = lives_malloc(sizeof(lives_colRGB48_t));
      cparam->def = lives_malloc(sizeof(lives_colRGB48_t));
      set_colRGB24_param(cparam->def, red, green, blue);
      set_colRGB24_param(cparam->value, red, green, blue);
    } else if (cparam->type == LIVES_PARAM_STRING) {
      if (len < 4) continue;
      cparam->value = (_(param_array[3]));
      cparam->def = (_(param_array[3]));
      if (len > 4) cparam->max = (double)atoi(param_array[4]);
      if (cparam->max == 0. || cparam->max > RFX_MAXSTRINGLEN) cparam->max = RFX_MAXSTRINGLEN;
    } else if (cparam->type == LIVES_PARAM_STRING_LIST) {
      if (len < 4) continue;
      cparam->value = lives_malloc(sizint);
      cparam->def = lives_malloc(sizint);
      *(int *)cparam->def = atoi(param_array[3]);
      if (len > 4) {
        cparam->list = array_to_string_list(param_array, 3, len);
      } else {
        set_int_param(cparam->def, 0);
      }
      set_int_param(cparam->value, get_int_param(cparam->def));
    } else {
      // int or bool
      int val;
      if (len < 4) continue;
      val = atoi(param_array[3]);
      cparam->value = lives_malloc(sizint);
      cparam->def = lives_malloc(sizint);
      set_int_param(cparam->def, val);
      set_int_param(cparam->value, val);
      if (cparam->type == LIVES_PARAM_BOOL) {
        cparam->min = 0;
        cparam->max = 1;
        if (len > 4) cparam->group = atoi(param_array[4]);
      } else {
        if (len < 6) continue;
        cparam->min = (double)atoi(param_array[4]);
        cparam->max = (double)atoi(param_array[5]);
        if (len > 6) {
          cparam->step_size = (double)atoi(param_array[6]);
          if (cparam->step_size == 0.) cparam->step_size = 1.;
          else if (cparam->step_size < 0.) {
            cparam->step_size = -cparam->step_size;
            cparam->wrap = TRUE;
          }
        }
      }
    }

    for (i = 0; i < MAX_PARAM_WIDGETS; i++) {
      cparam->widgets[i] = NULL;
    }
    cparam->onchange = FALSE;
    lives_strfreev(param_array);
  }
  lives_list_free_all(&parameter_list);
  //threaded_dialog_spin(0.);
}


LiVESList *array_to_string_list(char **array, int offset, int len) {
  // build a LiVESList from an array.
  int i;

  char *string, *tmp;
  LiVESList *slist = NULL;

  for (i = offset + 1; i < len; i++) {
    string = subst((tmp = L2U8(array[i])), "\\n", "\n");
    lives_free(tmp);

    // omit a last empty string
    if (i < len - 1 || *string) {
      slist = lives_list_append(slist, string);
    } else lives_free(string);
  }

  return slist;
}


static int cmp_menu_entries(livesconstpointer a, livesconstpointer b) {
  return lives_utf8_strcmpfunc(((lives_rfx_t *)a)->menu_text, ((lives_rfx_t *)b)->menu_text, LIVES_INT_TO_POINTER(TRUE));
}


void sort_rfx_array(lives_rfx_t *in, int num) {
  // sort rfx array into UTF-8 order by menu entry
  lives_rfx_t *rfx;
  int sorted = 1;
  register int i;

  LiVESList *rfx_list = NULL, *xrfx_list;
  for (i = num; i > 0; i--) {
    rfx_list = lives_list_prepend(rfx_list, (livespointer)&in[i]);
  }
  rfx_list = lives_list_sort(rfx_list, cmp_menu_entries);
  rfx = mainw->rendered_fx = (lives_rfx_t *)lives_calloc((num + 1), sizeof(lives_rfx_t));
  rfx_copy(rfx, in, FALSE);
  xrfx_list = rfx_list;
  while (xrfx_list) {
    rfx_copy(&mainw->rendered_fx[sorted++], (lives_rfx_t *)(xrfx_list->data), FALSE);
    xrfx_list = xrfx_list->next;
  }
  lives_list_free(rfx_list);
}


void rfx_copy(lives_rfx_t *dest, lives_rfx_t *src, boolean full) {
  // Warning, does not copy all fields (full will do that)
  lives_memcpy(dest->delim, src->delim, 2);
  dest->source = src->source;
  if (!full) {
    // ref. assigned memory
    src->source = NULL;
    dest->name = src->name;
    src->name = NULL;
    dest->menu_text = src->menu_text;
    src->menu_text = NULL;
    dest->action_desc = src->action_desc;
    src->action_desc = NULL;
    dest->params = src->params;
    src->params = NULL;
  } else {
    // deep copy
    if (dest->source_type == LIVES_RFX_SOURCE_WEED && dest->source) weed_instance_ref(dest->source);
    dest->name = lives_strdup(src->name);
    dest->menu_text = lives_strdup(src->menu_text);
    dest->action_desc = lives_strdup(src->action_desc);
    // TODO - copy params
  }

  lives_snprintf(dest->rfx_version, 64, "%s", src->rfx_version);
  dest->min_frames = src->min_frames;
  dest->num_in_channels = src->num_in_channels;
  dest->status = src->status;
  dest->props = src->props;
  dest->source_type = src->source_type;
  dest->num_params = src->num_params;
  dest->is_template = src->is_template;
  dest->menuitem = src->menuitem;
  dest->gui_strings = lives_list_copy(src->gui_strings);
  dest->onchange_strings = lives_list_copy(src->onchange_strings);
  if (!full) return;

  // TODO

}


void rfx_params_free(lives_rfx_t *rfx) {
  register int i;
  for (i = 0; i < rfx->num_params; i++) {
    if (rfx->params[i].type == LIVES_PARAM_UNDISPLAYABLE || rfx->params[i].type == LIVES_PARAM_UNKNOWN) continue;
    lives_free(rfx->params[i].name);
    lives_freep((void **)&rfx->params[i].def);
    lives_freep((void **)&rfx->params[i].value);
    lives_freep((void **)&rfx->params[i].label);
    lives_freep((void **)&rfx->params[i].desc);
    lives_list_free_all(&rfx->params[i].list);
  }
}


void rfx_free(lives_rfx_t *rfx) {
  if (!rfx) return;

  if (mainw->vrfx_update == rfx) mainw->vrfx_update = NULL;

  lives_freep((void **)&rfx->name);
  lives_freep((void **)&rfx->menu_text);
  lives_freep((void **)&rfx->action_desc);

  if (rfx->params) {
    rfx_params_free(rfx);
    lives_free(rfx->params);
  }

  if (rfx->gui_strings) lives_list_free_all(&rfx->gui_strings);
  if (rfx->onchange_strings) lives_list_free_all(&rfx->onchange_strings);

  if (rfx->source_type == LIVES_RFX_SOURCE_WEED && rfx->source) {
    weed_instance_unref((weed_plant_t *)rfx->source); // remove the ref we held
  }
}


void rfx_free_all(void) {
  register int i;
  for (i = 0; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom
       + mainw->num_rendered_effects_test; i++) {
    rfx_free(&mainw->rendered_fx[i]);
  }
  lives_freep((void **)&mainw->rendered_fx);
}


void param_copy(lives_param_t *dest, lives_param_t *src, boolean full) {
  // rfxbuilder.c uses this to copy params to a temporary copy and back again

  dest->name = lives_strdup(src->name);
  dest->label = lives_strdup(src->label);
  dest->group = src->group;
  dest->onchange = src->onchange;
  dest->type = src->type;
  dest->dp = src->dp;
  dest->min = src->min;
  dest->max = src->max;
  dest->step_size = src->step_size;
  dest->wrap = src->wrap;
  dest->source = src->source;
  dest->reinit = src->reinit;
  dest->source_type = src->source_type;
  dest->value = dest->def = NULL;
  dest->list = NULL;

  switch (dest->type) {
  case LIVES_PARAM_BOOL:
    dest->dp = 0;
  case LIVES_PARAM_NUM:
    if (!dest->dp) {
      dest->def = lives_malloc(sizint);
      lives_memcpy(dest->def, src->def, sizint);
    } else {
      dest->def = lives_malloc(sizdbl);
      lives_memcpy(dest->def, src->def, sizdbl);
    }
    break;
  case LIVES_PARAM_COLRGB24:
    dest->def = lives_malloc(sizeof(lives_colRGB48_t));
    lives_memcpy(dest->def, src->def, sizeof(lives_colRGB48_t));
    break;
  case LIVES_PARAM_STRING:
    dest->def = lives_strdup((char *)src->def);
    break;
  case LIVES_PARAM_STRING_LIST:
    dest->def = lives_malloc(sizint);
    set_int_param(dest->def, get_int_param(src->def));
    if (src->list) dest->list = lives_list_copy(src->list);
    break;
  default:
    break;
  }
  if (!full) return;
  // TODO - copy value, copy widgets

}


boolean get_bool_param(void *value) {
  boolean ret;
  lives_memcpy(&ret, value, 4);
  return ret;
}


int get_int_param(void *value) {
  int ret;
  lives_memcpy(&ret, value, sizint);
  return ret;
}


double get_double_param(void *value) {
  double ret;
  lives_memcpy(&ret, value, sizdbl);
  return ret;
}


void get_colRGB24_param(void *value, lives_colRGB48_t *rgb) {
  lives_memcpy(rgb, value, sizeof(lives_colRGB48_t));
}


void get_colRGBA32_param(void *value, lives_colRGBA64_t *rgba) {
  lives_memcpy(rgba, value, sizeof(lives_colRGBA64_t));
}


void set_bool_param(void *value, boolean _const) {
  set_int_param(value, !!_const);
}


void set_string_param(void **value_ptr, const char *_const, size_t maxlen) {
  lives_freep(value_ptr);
  *value_ptr = lives_strndup(_const, maxlen);
}


void set_int_param(void *value, int _const) {
  lives_memcpy(value, &_const, sizint);
}


void set_double_param(void *value, double _const) {
  lives_memcpy(value, &_const, sizdbl);
}


boolean set_rfx_param_by_name_string(lives_rfx_t *rfx, const char *name, const char *value, boolean update_visual) {
  size_t len = strlen(value);
  lives_param_t *param = find_rfx_param_by_name(rfx, name);
  if (!param) return FALSE;
  set_string_param((void **)&param->value, value, len > RFX_MAXSTRINGLEN ? RFX_MAXSTRINGLEN : len);
  if (update_visual) {
    LiVESList *list = NULL;
    char *tmp, *tmp2;
    list = lives_list_append(list, lives_strdup_printf("\"%s\"", (tmp = U82L(tmp2 = subst(value, "\"", "\\\"")))));
    lives_free(tmp); lives_free(tmp2);
    set_param_from_list(list, param, 0, FALSE, TRUE);
    lives_list_free_all(&list);
  }
  return TRUE;
}


boolean get_rfx_param_by_name_string(lives_rfx_t *rfx, const char *name, char **return_value) {
  lives_param_t *param = find_rfx_param_by_name(rfx, name);
  if (!param) return FALSE;
  *return_value = lives_strndup(param->value, RFX_MAXSTRINGLEN);
  return TRUE;
}


void set_colRGB24_param(void *value, short red, short green, short blue) {
  lives_colRGB48_t *rgbp = (lives_colRGB48_t *)value;

  if (red < 0) red = 0;
  if (red > 255) red = 255;
  if (green < 0) green = 0;
  if (green > 255) green = 255;
  if (blue < 0) blue = 0;
  if (blue > 255) blue = 255;

  rgbp->red = red;
  rgbp->green = green;
  rgbp->blue = blue;

}


void set_colRGBA32_param(void *value, short red, short green, short blue, short alpha) {
  lives_colRGBA64_t *rgbap = (lives_colRGBA64_t *)value;
  rgbap->red = red;
  rgbap->green = green;
  rgbap->blue = blue;
  rgbap->alpha = alpha;
}


///////////////////////////////////////////////////////////////

int find_rfx_plugin_by_name(const char *name, short status) {
  int i;
  for (i = 1; i < mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
       mainw->num_rendered_effects_test; i++) {
    if (mainw->rendered_fx[i].name && !strcmp(mainw->rendered_fx[i].name, name)
        && mainw->rendered_fx[i].status == status)
      return (int)i;
  }
  return -1;
}


lives_param_t *find_rfx_param_by_name(lives_rfx_t *rfx, const char *name) {
  int i;
  if (!rfx) return NULL;
  for (i = 0; i < rfx->num_params; i++) {
    if (!strcmp(name, rfx->params[i].name)) {
      return &rfx->params[i];
    }
  }
  return NULL;
}


lives_param_t *weed_params_to_rfx(int npar, weed_plant_t *inst, boolean show_reinits) {
  int i, j;
  lives_param_t *rpar = (lives_param_t *)lives_calloc(npar, sizeof(lives_param_t));
  int param_type;
  char **list;
  LiVESList *gtk_list = NULL;
  char *string;
  int vali;
  double vald;
  weed_plant_t *gui = NULL;
  int listlen;
  int cspace, *cols = NULL, red_min = 0, red_max = 255, green_min = 0, green_max = 255, blue_min = 0, blue_max = 255,
               *maxi = NULL, *mini = NULL;
  double *colsd;
  double red_mind = 0., red_maxd = 1., green_mind = 0., green_maxd = 1., blue_mind = 0., blue_maxd = 1.,
         *maxd = NULL, *mind = NULL;
  int flags = 0;
  int nwpars = 0, poffset = 0;
  boolean col_int;

  weed_plant_t *wtmpl;
  weed_plant_t **wpars = NULL, *wpar = NULL;

  weed_plant_t *chann, *ctmpl;
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);

  wpars = weed_instance_get_in_params(inst, &nwpars);

  for (i = 0; i < npar; i++) {
    if (i - poffset >= nwpars) {
      // handling for compound fx
      poffset += nwpars;
      if (wpars) lives_free(wpars);
      inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
      wpars = weed_instance_get_in_params(inst, &nwpars);
      i--;
      continue;
    }

    if (!wpars) {
      lives_free(rpar);
      return NULL;
    }

    wpar = wpars[i - poffset];
    wtmpl = weed_param_get_template(wpar);

    flags = weed_paramtmpl_get_flags(wtmpl);

    rpar[i].flags = flags;

    gui = weed_paramtmpl_get_gui(wtmpl, FALSE);

    rpar[i].group = 0;

    rpar[i].use_mnemonic = FALSE;
    rpar[i].hidden = 0;
    rpar[i].step_size = 1.;
    if (enabled_in_channels(filter, FALSE) == 2 && get_transition_param(filter, FALSE) == i) rpar[i].transition = TRUE;
    else rpar[i].transition = FALSE;
    rpar[i].wrap = FALSE;
    rpar[i].reinit = FALSE;
    rpar[i].change_blocked = FALSE;
    rpar[i].source = wtmpl;
    rpar[i].source_type = LIVES_RFX_SOURCE_WEED;
    rpar[i].special_type = LIVES_PARAM_SPECIAL_TYPE_NONE;
    rpar[i].special_type_index = 0;
    rpar[i].value = NULL;
    rpar[i].def = NULL;

    if (flags & WEED_PARAMETER_VARIABLE_SIZE && !(flags & WEED_PARAMETER_VALUE_PER_CHANNEL)) {
      rpar[i].hidden |= HIDDEN_MULTI;
      rpar[i].multi = PVAL_MULTI_ANY;
    } else if (flags & WEED_PARAMETER_VALUE_PER_CHANNEL) {
      rpar[i].hidden |= HIDDEN_MULTI;
      rpar[i].multi = PVAL_MULTI_PER_CHANNEL;
    } else rpar[i].multi = PVAL_MULTI_NONE;

    chann = get_enabled_channel(inst, 0, TRUE);
    ctmpl = weed_get_plantptr_value(chann, WEED_LEAF_TEMPLATE, NULL);

    if (weed_get_boolean_value(ctmpl, WEED_LEAF_IS_AUDIO, NULL) == WEED_TRUE) {
      // dont hide multivalued params for audio effects
      rpar[i].hidden = 0;
    }

    rpar[i].dp = 0;
    rpar[i].min = 0.;
    rpar[i].max = 0.;
    rpar[i].list = NULL;

    rpar[i].reinit = 0;
    if (flags & WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)
      rpar[i].reinit = REINIT_FUNCTIONAL;
    if (gui && (weed_gui_get_flags(gui) & WEED_GUI_REINIT_ON_VALUE_CHANGE))
      rpar[i].reinit |= REINIT_VISUAL;

    if (!show_reinits && rpar[i].reinit != 0) rpar[i].hidden |= HIDDEN_NEEDS_REINIT;

    ///////////////////////////////
    param_type = weed_paramtmpl_get_type(wtmpl);

    switch (param_type) {
    case WEED_PARAM_SWITCH:
      if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 1) {
        rpar[i].hidden |= HIDDEN_MULTI;
      }
      rpar[i].type = LIVES_PARAM_BOOL;
      rpar[i].value = lives_malloc(sizint);
      rpar[i].def = lives_malloc(sizint);
      if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT))
        vali = weed_get_boolean_value(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
      else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
        vali = weed_get_boolean_value(wtmpl, WEED_LEAF_DEFAULT, NULL);
      else vali = weed_get_boolean_value(wtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      set_int_param(rpar[i].def, vali);
      vali = weed_get_boolean_value(wpar, WEED_LEAF_VALUE, NULL);
      set_int_param(rpar[i].value, vali);
      if (weed_plant_has_leaf(wtmpl, WEED_LEAF_GROUP)) rpar[i].group = weed_get_int_value(wtmpl, WEED_LEAF_GROUP, NULL);
      break;
    case WEED_PARAM_INTEGER:
      if (weed_plant_has_leaf(wtmpl, WEED_LEAF_DEFAULT) && weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 1) {
        rpar[i].hidden |= HIDDEN_MULTI;
      }
      rpar[i].type = LIVES_PARAM_NUM;
      rpar[i].value = lives_malloc(sizint);
      rpar[i].def = lives_malloc(sizint);
      if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT)) {
        vali = weed_get_int_value(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
      } else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
        vali = weed_get_int_value(wtmpl, WEED_LEAF_DEFAULT, NULL);
      else vali = weed_get_int_value(wtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      set_int_param(rpar[i].def, vali);
      vali = weed_get_int_value(wpar, WEED_LEAF_VALUE, NULL);
      set_int_param(rpar[i].value, vali);
      rpar[i].min = (double)weed_get_int_value(wtmpl, WEED_LEAF_MIN, NULL);
      rpar[i].max = (double)weed_get_int_value(wtmpl, WEED_LEAF_MAX, NULL);
      if (weed_paramtmpl_does_wrap(wtmpl)) rpar[i].wrap = TRUE;
      if (gui) {
        if (weed_plant_has_leaf(gui, WEED_LEAF_CHOICES)) {
          listlen = weed_leaf_num_elements(gui, WEED_LEAF_CHOICES);
          list = weed_get_string_array(gui, WEED_LEAF_CHOICES, NULL);
          for (j = 0; j < listlen; j++) {
            gtk_list = lives_list_append(gtk_list, list[j]);
          }
          lives_free(list);
          rpar[i].list = lives_list_copy(gtk_list);
          lives_list_free(gtk_list);
          gtk_list = NULL;
          rpar[i].type = LIVES_PARAM_STRING_LIST;
          rpar[i].max = listlen;
        } else if (weed_plant_has_leaf(gui, WEED_LEAF_STEP_SIZE))
          rpar[i].step_size = (double)weed_get_int_value(gui, WEED_LEAF_STEP_SIZE, NULL);
        if (rpar[i].step_size == 0.) rpar[i].step_size = 1.;
      }
      break;
    case WEED_PARAM_FLOAT:
      if (weed_plant_has_leaf(wtmpl, WEED_LEAF_DEFAULT) && weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 1) {
        rpar[i].hidden |= HIDDEN_MULTI;
      }
      rpar[i].type = LIVES_PARAM_NUM;
      rpar[i].value = lives_malloc(sizdbl);
      rpar[i].def = lives_malloc(sizdbl);
      if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT))
        vald = weed_get_double_value(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
      else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
        vald = weed_get_double_value(wtmpl, WEED_LEAF_DEFAULT, NULL);
      else vald = weed_get_double_value(wtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      set_double_param(rpar[i].def, vald);
      vald = weed_get_double_value(wpar, WEED_LEAF_VALUE, NULL);
      set_double_param(rpar[i].value, vald);
      rpar[i].min = weed_get_double_value(wtmpl, WEED_LEAF_MIN, NULL);
      rpar[i].max = weed_get_double_value(wtmpl, WEED_LEAF_MAX, NULL);
      if (weed_paramtmpl_does_wrap(wtmpl)) rpar[i].wrap = TRUE;
      rpar[i].step_size = 0.;
      rpar[i].dp = 2;
      if (gui) {
        if (weed_plant_has_leaf(gui, WEED_LEAF_STEP_SIZE))
          rpar[i].step_size = weed_get_double_value(gui, WEED_LEAF_STEP_SIZE, NULL);
        if (weed_plant_has_leaf(gui, WEED_LEAF_DECIMALS))
          rpar[i].dp = weed_get_int_value(gui, WEED_LEAF_DECIMALS, NULL);
      }
      if (rpar[i].step_size == 0.) {
        if (rpar[i].max - rpar[i].min > 10. && !(rpar[i].min >= -10. && rpar[i].max <= 10.))
          rpar[i].step_size = 1.;
        else if (rpar[i].max - rpar[i].min > 1. && !(rpar[i].min >= -1. && rpar[i].max <= 1.))
          rpar[i].step_size = .1;
        else rpar[i].step_size = 1. / (double)lives_10pow(rpar[i].dp);
      }
      break;
    case WEED_PARAM_TEXT:
      if (weed_plant_has_leaf(wtmpl, WEED_LEAF_DEFAULT) && weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 1) {
        rpar[i].hidden |= HIDDEN_MULTI;
      }
      rpar[i].type = LIVES_PARAM_STRING;
      if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT))
        string = weed_get_string_value(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
      else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
        string = weed_get_string_value(wtmpl, WEED_LEAF_DEFAULT, NULL);
      else string = weed_get_string_value(wtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      rpar[i].def = string;
      string = weed_get_string_value(wpar, WEED_LEAF_VALUE, NULL);
      rpar[i].value = string;
      rpar[i].max = 0.;
      if (gui && weed_plant_has_leaf(gui, WEED_LEAF_MAXCHARS)) {
        rpar[i].max = (double)weed_get_int_value(gui, WEED_LEAF_MAXCHARS, NULL);
        if (rpar[i].max < 0.) rpar[i].max = 0.;
      }
      break;
    case WEED_PARAM_COLOR:
      cspace = weed_get_int_value(wtmpl, WEED_LEAF_COLORSPACE, NULL);
      switch (cspace) {
      case WEED_COLORSPACE_RGB:
        if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 3) {
          rpar[i].hidden |= HIDDEN_MULTI;
        }
        rpar[i].type = LIVES_PARAM_COLRGB24;
        rpar[i].value = lives_malloc(3 * sizint);
        rpar[i].def = lives_malloc(3 * sizint);

        if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
          if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT)) {
            cols = weed_get_int_array(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
          } else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
            cols = weed_get_int_array(wtmpl, WEED_LEAF_DEFAULT, NULL);
          else cols = weed_get_int_array(wtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
          if (weed_leaf_num_elements(wtmpl, WEED_LEAF_MAX) == 1) {
            red_max = green_max = blue_max = weed_get_int_value(wtmpl, WEED_LEAF_MAX, NULL);
          } else {
            maxi = weed_get_int_array(wtmpl, WEED_LEAF_MAX, NULL);
            red_max = maxi[0];
            green_max = maxi[1];
            blue_max = maxi[2];
          }
          if (weed_leaf_num_elements(wtmpl, WEED_LEAF_MIN) == 1) {
            red_min = green_min = blue_min = weed_get_int_value(wtmpl, WEED_LEAF_MIN, NULL);
          } else {
            mini = weed_get_int_array(wtmpl, WEED_LEAF_MIN, NULL);
            red_min = mini[0];
            green_min = mini[1];
            blue_min = mini[2];
          }
          if (cols[0] < red_min) cols[0] = red_min;
          if (cols[1] < green_min) cols[1] = green_min;
          if (cols[2] < blue_min) cols[2] = blue_min;
          if (cols[0] > red_max) cols[0] = red_max;
          if (cols[1] > green_max) cols[1] = green_max;
          if (cols[2] > blue_max) cols[2] = blue_max;
          cols[0] = (double)(cols[0] - red_min) / (double)(red_max - red_min) * 255. + .49999;
          cols[1] = (double)(cols[1] - green_min) / (double)(green_max - green_min) * 255. + .49999;
          cols[2] = (double)(cols[2] - blue_min) / (double)(blue_max - blue_min) * 255. + .49999;
          col_int = TRUE;
        } else {
          if (!weed_paramtmpl_value_irrelevant(wtmpl) && weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_DEFAULT))
            colsd = weed_get_double_array(wtmpl, WEED_LEAF_HOST_DEFAULT, NULL);
          else if (weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT) > 0)
            colsd = weed_get_double_array(wtmpl, WEED_LEAF_DEFAULT, NULL);
          else colsd = weed_get_double_array(wtmpl, WEED_LEAF_DEFAULT, NULL);
          if (weed_leaf_num_elements(wtmpl, WEED_LEAF_MAX) == 1) {
            red_maxd = green_maxd = blue_maxd = weed_get_double_value(wtmpl, WEED_LEAF_MAX, NULL);
          } else {
            maxd = weed_get_double_array(wtmpl, WEED_LEAF_MAX, NULL);
            red_maxd = maxd[0];
            green_maxd = maxd[1];
            blue_maxd = maxd[2];
          }
          if (weed_leaf_num_elements(wtmpl, WEED_LEAF_MIN) == 1) {
            red_mind = green_mind = blue_mind = weed_get_double_value(wtmpl, WEED_LEAF_MIN, NULL);
          } else {
            mind = weed_get_double_array(wtmpl, WEED_LEAF_MIN, NULL);
            red_mind = mind[0];
            green_mind = mind[1];
            blue_mind = mind[2];
          }
          if (colsd[0] < red_mind) colsd[0] = red_mind;
          if (colsd[1] < green_mind) colsd[1] = green_mind;
          if (colsd[2] < blue_mind) colsd[2] = blue_mind;
          if (colsd[0] > red_maxd) colsd[0] = red_maxd;
          if (colsd[1] > green_maxd) colsd[1] = green_maxd;
          if (colsd[2] > blue_maxd) colsd[2] = blue_maxd;
          cols = (int *)lives_malloc(3 * sizint);
          cols[0] = (colsd[0] - red_mind) / (red_maxd - red_mind) * 255. + .49999;
          cols[1] = (colsd[1] - green_mind) / (green_maxd - green_mind) * 255. + .49999;
          cols[2] = (colsd[2] - blue_mind) / (blue_maxd - blue_mind) * 255. + .49999;
          col_int = FALSE;
        }
        set_colRGB24_param(rpar[i].def, cols[0], cols[1], cols[2]);

        if (col_int) {
          lives_free(cols);
          cols = weed_get_int_array(wpar, WEED_LEAF_VALUE, NULL);
          if (cols[0] < red_min) cols[0] = red_min;
          if (cols[1] < green_min) cols[1] = green_min;
          if (cols[2] < blue_min) cols[2] = blue_min;
          if (cols[0] > red_max) cols[0] = red_max;
          if (cols[1] > green_max) cols[1] = green_max;
          if (cols[2] > blue_max) cols[2] = blue_max;
          cols[0] = (double)(cols[0] - red_min) / (double)(red_max - red_min) * 255. + .49999;
          cols[1] = (double)(cols[1] - green_min) / (double)(green_max - green_min) * 255. + .49999;
          cols[2] = (double)(cols[2] - blue_min) / (double)(blue_max - blue_min) * 255. + .49999;
        } else {
          colsd = weed_get_double_array(wpar, WEED_LEAF_VALUE, NULL);
          if (colsd[0] < red_mind) colsd[0] = red_mind;
          if (colsd[1] < green_mind) colsd[1] = green_mind;
          if (colsd[2] < blue_mind) colsd[2] = blue_mind;
          if (colsd[0] > red_maxd) colsd[0] = red_maxd;
          if (colsd[1] > green_maxd) colsd[1] = green_maxd;
          if (colsd[2] > blue_maxd) colsd[2] = blue_maxd;
          cols[0] = (colsd[0] - red_mind) / (red_maxd - red_mind) * 255. + .49999;
          cols[1] = (colsd[1] - green_mind) / (green_maxd - green_mind) * 255. + .49999;
          cols[2] = (colsd[2] - blue_mind) / (blue_maxd - blue_mind) * 255. + .49999;
        }
        set_colRGB24_param(rpar[i].value, (short)cols[0], (short)cols[1], (short)cols[2]);
        lives_free(cols);

        lives_freep((void **)&maxi);
        lives_freep((void **)&mini);
        lives_freep((void **)&maxd);
        lives_freep((void **)&mind);
        break;
      }
      break;

    default:
      rpar[i].type = LIVES_PARAM_UNKNOWN; // TODO - try to get default
    }

    string = weed_get_string_value(wtmpl, WEED_LEAF_NAME, NULL);
    rpar[i].name = string;
    rpar[i].label = lives_strdup(string);

    if (weed_plant_has_leaf(wtmpl, WEED_LEAF_DESCRIPTION)) {
      string = weed_get_string_value(wtmpl, WEED_LEAF_DESCRIPTION, NULL);
      rpar[i].desc = string;
    } else rpar[i].desc = NULL;

    if (is_hidden_param(inst, i)) rpar[i].hidden |= HIDDEN_GUI_PERM;

    // gui part /////////////////////

    if (gui) {
      if (weed_plant_has_leaf(gui, WEED_LEAF_LABEL)) {
        string = weed_get_string_value(gui, WEED_LEAF_LABEL, NULL);
        lives_free(rpar[i].label);
        rpar[i].label = string;
      }
      if (weed_plant_has_leaf(gui, WEED_LEAF_USE_MNEMONIC)) {
        rpar[i].use_mnemonic = weed_get_boolean_value(gui, WEED_LEAF_USE_MNEMONIC, NULL);
      }
    }

    for (j = 0; j < MAX_PARAM_WIDGETS; j++) {
      rpar[i].widgets[j] = NULL;
    }
    rpar[i].onchange = FALSE;
  }

  lives_free(wpars);

  return rpar;
}


lives_rfx_t *weed_to_rfx(weed_plant_t *plant, boolean show_reinits) {
  // return an RFX for a weed effect; set rfx->source to an INSTANCE of the filter (first instance for compound fx)
  // instance should be refcounted
  weed_plant_t *filter, *inst;

  char *string;
  lives_rfx_t *rfx = (lives_rfx_t *)lives_calloc(1, sizeof(lives_rfx_t));
  rfx->is_template = FALSE;
  if (weed_get_int_value(plant, WEED_LEAF_TYPE, NULL) == WEED_PLANT_FILTER_INSTANCE) {
    filter = weed_instance_get_filter(plant, TRUE);
    inst = plant;
  } else {
    filter = plant;
    inst = weed_instance_from_filter(filter);
    // init and deinit the effect to allow the plugin to hide parameters, etc.
    // rfx will inherit the refcount
    weed_reinit_effect(inst, TRUE);
    weed_instance_unref(inst);
    rfx->is_template = TRUE;
  }

  string = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
  rfx->name = string;
  rfx->menu_text = lives_strdup(string);
  rfx->action_desc = lives_strdup("no action");
  rfx->min_frames = -1;
  rfx->num_in_channels = enabled_in_channels(filter, FALSE);
  rfx->status = RFX_STATUS_WEED;
  rfx->props = 0;
  rfx->menuitem = NULL;
  rfx->num_params = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);
  if (rfx->num_params > 0) rfx->params = weed_params_to_rfx(rfx->num_params, inst, show_reinits);
  else rfx->params = NULL;
  rfx->source = (void *)inst;
  rfx->source_type = LIVES_RFX_SOURCE_WEED;
  rfx->gui_strings = NULL;
  rfx->onchange_strings = NULL;
  rfx->flags = 0;
  rfx->needs_reinit = FALSE;
  return rfx;
}

/**
   @brief get the interface hints set by a Weed filter in the filter_class.

    for a compound effect we get the gui elements from each internal filter in sequence,
    inserting internal|nextfilter after each filter

    - the filter MUST have set LAYOUT_SCHEME to RFX in the filter class.
    - it must have set the leaf RFX_DELIM with the string delimiter (and anything after the first character is ignored)
    - the layout must be set in the RFX_STRINGS array, using the delimiter

    returns a LiVESList of the results
*/
LiVESList *get_external_window_hints(lives_rfx_t *rfx) {
  LiVESList *hints = NULL;

  if (rfx->status == RFX_STATUS_WEED) {
    weed_plant_t *gui;
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
    int *filters = NULL;
    char *string, **rfx_strings, *delim;
    int nfilters;
    int num_hints;
    int i;

    if ((nfilters = num_compound_fx(filter)) > 1) {
      // handle compound fx
      filters = weed_get_int_array(filter, WEED_LEAF_HOST_FILTER_LIST, NULL);
    }

    for (i = 0; i < nfilters; i++) {
      if (filters) {
        filter = get_weed_filter(filters[i]);
      }

      if (!weed_plant_has_leaf(filter, WEED_LEAF_GUI)) continue;
      gui = weed_get_plantptr_value(filter, WEED_LEAF_GUI, NULL);

      if (!weed_plant_has_leaf(gui, WEED_LEAF_LAYOUT_SCHEME)) continue;

      string = weed_get_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, NULL);
      if (strcmp(string, "RFX")) {
        lives_free(string);
        continue;
      }
      lives_free(string);

      if (!weed_plant_has_leaf(gui, WEED_LEAF_RFX_DELIM)) continue;
      delim = weed_get_string_value(gui, WEED_LEAF_RFX_DELIM, NULL);
      lives_snprintf(rfx->delim, 2, "%s", delim);
      lives_free(delim);

      rfx_strings = weed_get_string_array_counted(gui, WEED_LEAF_RFX_STRINGS, &num_hints);
      if (!num_hints) continue;

      for (i = 0; i < num_hints; i++) {
        hints = lives_list_append(hints, rfx_strings[i]);
      }
      lives_free(rfx_strings);

      if (filters) hints = lives_list_append(hints, lives_strdup("internal|nextfilter"));
    }

    lives_freep((void **)&filters);
  }

  return hints;
}


void rfx_clean_exe(lives_rfx_t *rfx) {
  if (rfx) {
    char *fnamex = lives_build_filename(prefs->workdir, rfx->name, NULL);
    if (lives_file_test(fnamex, LIVES_FILE_TEST_EXISTS)) lives_rm(fnamex);
    lives_free(fnamex);
  }
}


/**
   @brief create an interface window for a plugin; possibly run it, and return the parameters

    N.B. this is NOT for rendered effects, those have their own functions.

    -- currently used for: encoder plugins and video playback plugins.

    Given an RFX script in scrap_text, (generally retrieved by some means from the plugin),
    will create an rfx effect, building the parameters from the <params> section of scrap_text,
    using the layout hints (optional) from <param_window>, and construct a parameter interface.

    The function has two modes of operation:

    If vbox is not NULL it should point to a LiVESVBox into which the parameter box will be added.
    The function will return NULL, and the rfx can be retrieved from ret_rfx.

    If vbox is NULL, the param window will be run, and if the user clicks "OK", the parameter values are returned in a marshalled list.
    If the user closes the window with Cancel, NULL is returned instead.

    If the plugin has no user adjustable parameters, the an empty string is returned.

    If <onchange> exists then the init | trigger will be run
    to let the plugin update default values (for vpps only currently)

    The onchange code is currently run by generating a perl scrap and running that. In future the code could
    be run in different languages or internally by using a simple parser like the one in the data_processor plugin.


    NOTE: if vbox is not NULL, we create the window inside vbox, without running it
    in this case, vbox should be packed in its own dialog window, which should then be run

    called from plugins.c (vpp opts) and saveplay.c (encoder opts) */
char *plugin_run_param_window(const char *scrap_text, LiVESVBox * vbox, lives_rfx_t **ret_rfx) {
  FILE *sfile;

  lives_rfx_t *rfx = (lives_rfx_t *)lives_calloc(1, sizeof(lives_rfx_t));

  char *string;
  char *rfx_scrapname, *rfx_scriptname;
  char *rfxfile;
  char *com;
  char *fnamex = NULL;
  char *res_string = NULL;
  char buff[32];

  int res;
  int retval;

  if (!check_for_executable(&capable->has_mktemp, EXEC_MKTEMP)) {
    do_program_not_found_error(EXEC_MKTEMP);
    return NULL;
  }

  rfx_scrapname = get_worktmpfile("rfx.");
  if (!rfx_scrapname) {
    workdir_warning();
    return NULL;
  }

  rfx_scriptname = lives_strdup_printf("%s.%s", rfx_scrapname, LIVES_FILE_EXT_RFX_SCRIPT);

  rfxfile = lives_build_path(prefs->workdir, rfx_scriptname, NULL);
  lives_free(rfx_scriptname);

  rfx->name = NULL;

  string = lives_strdup_printf("<name>\n%s\n</name>\n", rfx_scrapname);

  do {
    retval = 0;
    sfile = fopen(rfxfile, "w");
    if (!sfile) {
      retval = do_write_failed_error_s_with_retry(rfxfile, lives_strerror(errno));
      if (retval == LIVES_RESPONSE_CANCEL) {
        lives_free(string);
        return NULL;
      }
    } else {
      THREADVAR(write_failed) = FALSE;
      lives_fputs(string, sfile);
      if (scrap_text) {
        char *data = subst(scrap_text, "\\n", "\n");
        lives_fputs(data, sfile);
        lives_free(data);
      }
      fclose(sfile);
      lives_free(string);
      if (THREADVAR(write_failed)) {
        retval = do_write_failed_error_s_with_retry(rfxfile, NULL);
        if (retval == LIVES_RESPONSE_CANCEL) {
          return NULL;
        }
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  // OK, we should now have an RFX fragment in a file, we can compile it, then build a parameter window from it

  // call RFX_BUILDER program to compile the script, passing parameters input_filename and output_directory
  com = lives_strdup_printf("\"%s\" \"%s\" \"%s\" >%s", EXEC_RFX_BUILDER, rfxfile, prefs->workdir, LIVES_DEVNULL);
  res = lives_system(com, TRUE);
  lives_free(com);

  lives_rm(rfxfile);
  lives_free(rfxfile);

  if (res == 0) {
    // the script compiled correctly

    // now we pop up the parameter window, get the values of our parameters, and marshall them as extra_params

    // first create a lives_rfx_t from the scrap
    rfx->name = lives_strdup(rfx_scrapname);
    rfx->menu_text = NULL;
    rfx->action_desc = NULL;
    rfx->gui_strings = NULL;
    rfx->onchange_strings = NULL;
    lives_snprintf(rfx->rfx_version, 64, "%s", RFX_VERSION);
    rfx->flags = 0;
    rfx->status = RFX_STATUS_SCRAP;

    rfx->num_in_channels = 0;
    rfx->min_frames = -1;

    rfx->flags = RFX_FLAGS_NO_SLIDERS;

    fnamex = lives_build_filename(prefs->workdir, rfx_scrapname, NULL);
    com = lives_strdup_printf("\"%s\" get_define", fnamex);
    lives_free(fnamex);

    if (!lives_popen(com, TRUE, buff, 32)) {
      THREADVAR(com_failed) = TRUE;
    }
    lives_free(com);

    // command to get_define failed
    if (THREADVAR(com_failed)) {
      rfx_clean_exe(rfx);
      return NULL;
    }

    lives_snprintf(rfx->delim, 2, "%s", buff);

    // ok, this might need adjusting afterwards
    rfx->menu_text = (vbox == NULL ? lives_strdup_printf(_("%s advanced settings"), prefs->encoder.of_desc) : lives_strdup(""));
    rfx->is_template = FALSE;

    rfx->source = NULL;
    rfx->source_type = LIVES_RFX_SOURCE_RFX;

#if 0
    render_fx_get_params(rfx, scrap_text, RFX_STATUS_INTERNAL);
#else
    render_fx_get_params(rfx, rfx_scrapname, RFX_STATUS_SCRAP);
#endif

    /// check if we actually have params to display
    if (!make_param_box(NULL, rfx)) {
      res_string = lives_strdup("");
      goto prpw_done;
    }

    // now we build our window and get param values
    if (!vbox) {
      _fx_dialog *fxdialog = on_fx_pre_activate(rfx, TRUE, NULL);
      LiVESWidget *dialog = fxdialog->dialog;
      if (prefs->show_gui) {
        lives_window_set_transient_for(LIVES_WINDOW(dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      }
      lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

      do {
        res = lives_dialog_run(LIVES_DIALOG(dialog));
      } while (res == LIVES_RESPONSE_RETRY);

      if (res == LIVES_RESPONSE_OK) {
        // marshall our params for passing to the plugin
        res_string = param_marshall(rfx, FALSE);
      }

      lives_widget_destroy(dialog);

      if (fx_dialog[1]) {
        lives_freep((void **)&fx_dialog[1]);
      }
    } else {
      make_param_box(vbox, rfx);
    }

prpw_done:
    if (ret_rfx) {
      *ret_rfx = rfx;
    } else {
      rfx_clean_exe(rfx);
      rfx_free(rfx);
      lives_free(rfx);
    }
  } else {
    if (ret_rfx) {
      *ret_rfx = NULL;
    } else {
      res_string = lives_strdup("");
    }
    if (rfx) {
      lives_free(rfx);
    }
  }
  lives_free(rfx_scrapname);
  return res_string;
}

