/// experimental feature
// transcode.c
// LiVES
// (c) G. Finch 2008 - 2017 <salsaman_lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// fast transcoding via a plugin
#define TEST_TRANSREND

#ifdef LIBAV_TRANSCODE

#include "main.h"
#include "resample.h"
#include "effects.h"
#include "transcode.h"
#include "paramwindow.h"
#include "effects-weed.h"
#include "rfx-builder.h"


boolean send_layer(weed_layer_t *layer, _vid_playback_plugin *vpp, int64_t timecode) {
  // send a weed layer to a (prepared) video playback plugin
  // warning: will quite likely change the pixel_data of layer

  // returns: TRUE on rendering error

  boolean player_v2 = FALSE;
  boolean error = FALSE;

  if (vpp->play_frame) player_v2 = TRUE;

  if (layer) {
    if (player_v2) {
      if (!(*vpp->play_frame)(layer,  mainw->currticks - mainw->stream_ticks, NULL)) {
        error = TRUE;
      }
    } else {
      void **pd_array;
      // send pixel data to the video frame renderer
      THREADVAR(rowstride_alignment_hint) = -1;
      // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
      compact_rowstrides(layer);
      // get a void ** to the planar pixel_data
      pd_array = weed_layer_get_pixel_data_planar(layer, NULL);
      error = !(*vpp->render_frame)
              (weed_layer_get_width(layer), weed_layer_get_height(layer),
               timecode, pd_array, NULL, NULL);
    }
  }

  weed_layer_free(layer);
  return error;
}

static _vid_playback_plugin *ovpp;

boolean transcode_prep(void) {
  _vid_playback_plugin *vpp;
  mainw->suppress_dprint = TRUE;

  vpp = open_vid_playback_plugin(TRANSCODE_PLUGIN_NAME, FALSE);

  mainw->suppress_dprint = FALSE;
  mainw->no_switch_dprint = TRUE;

  if (!vpp) {
    d_print(_("Plugin %s not found.\n"), TRANSCODE_PLUGIN_NAME);
    mainw->no_switch_dprint = FALSE;
    return FALSE;
  }

  // for now we can only have one instance of a vpp: TODO - make vpp plugins re-entrant
  lives_memset(future_prefs->vpp_name, 0, 1);

  ovpp = mainw->vpp;
  mainw->vpp = vpp;
  return TRUE;
}


char *transcode_get_params(char *fname_def) {
  // create the param window for the plugin, configure it, and return the filename
  LiVESList *retvals = NULL;
  _vppaw *vppa;
  lives_rfx_t *rfx;
  char *fname;
  LiVESResponseType resp;

  vppa = on_vpp_advanced_clicked(NULL, NULL);
  if (!fname_def) fname_def = lives_build_filename(mainw->vid_save_dir, DEF_TRANSCODE_FILENAME, NULL);
  rfx = vppa->rfx;
  if (!rfx) {
    lives_widget_destroy(vppa->dialog);
    lives_free(vppa);
    return NULL;
  }

  /// we want to keep the rfx around even after the dialog is run, so we can get the filename
  vppa->keep_rfx = TRUE;

  // set the default value in the param window
  // TODO - consider setting in intentcaps ?
  set_rfx_value_by_name_string(rfx, TRANSCODE_PARAM_FILENAME, fname_def, TRUE);
  lives_free(fname_def);

  retvals = do_onchange_init(rfx);
  if (retvals) {
    // now apply visually anything we got from onchange_init
    param_demarshall(rfx, retvals, TRUE, TRUE);
    lives_list_free_all(&retvals);
  }

  // run the param window
  do {
    resp = lives_dialog_run(LIVES_DIALOG(vppa->dialog));
  } while (resp == LIVES_RESPONSE_RETRY);

  if (resp == LIVES_RESPONSE_OK) {
    prefs->twater_type = lives_combo_get_active_index(LIVES_COMBO(vppa->overlay_combo));
    on_vppa_ok_clicked(TRUE, vppa);
  }

  lives_widget_destroy(vppa->dialog);
  lives_free(vppa);

  /// need to clean this up here, since we asked for the rfx to be kept
  rfx_clean_exe(rfx);

  if (resp == LIVES_RESPONSE_CANCEL) {
    mainw->cancelled = CANCEL_USER;
    rfx_free(rfx);
    lives_free(rfx);
    return NULL;
  }

  mainw->cancelled = CANCEL_NONE;

  // get the value of the "filename" param (this is only so we can display it in status messages)
  get_rfx_value_by_name_string(rfx, TRANSCODE_PARAM_FILENAME, &fname);

  rfx_free(rfx);
  lives_free(rfx);
  return fname;
}


void transcode_cleanup(_vid_playback_plugin *vpp) {
  // close vpp, unless mainw->vpp
  if (!ovpp || (vpp->handle != ovpp->handle)) {
    close_vid_playback_plugin(vpp);
  }
  else {
    // we "borrowed" the playback plugin, so set these back how they were
    if (ovpp->set_fps)(*ovpp->set_fps)(ovpp->fixed_fpsd);
    if (ovpp->set_palette)(*ovpp->set_palette)(ovpp->palette);
    if (ovpp->set_yuv_palette_clamping)(*ovpp->set_yuv_palette_clamping)(ovpp->YUV_clamping);
  }

  mainw->vpp = ovpp;
}


static ticks_t startt;
static int tot_frames;
static int out_asamps;

static weed_layer_t *apply_watermark(weed_layer_t *layer, ticks_t currticks) {
  char *framestr, *timestr, *audstr;
  ticks_t curt;
  double elapsed_secs, fps_avg = 0.;
  int rgb[3];
  switch (prefs->twater_type) {
  case TWATER_TYPE_STATS:
    if (mainw->overlay_msg) lives_free(mainw->overlay_msg);

    if (mainw->num_tracks > 1) {
      framestr = lives_strdup_printf("%ld (fg), %ld (bg)", mainw->frame_index[0], mainw->frame_index[1]);
    } else {
      framestr = lives_strdup_printf("%ld", mainw->frame_index[0]);
    }
    curt = lives_get_current_ticks();
    elapsed_secs = (curt - startt) / TICKS_PER_SECOND_DBL;
    if (elapsed_secs) fps_avg = tot_frames / elapsed_secs;
    timestr = format_tstr(elapsed_secs, 0);

    if (cfile->achans && cfile->arps) {
      audstr = lives_strdup_printf(", audio: %d channels, %d bits per sample, %d Hz", cfile->achans, out_asamps, cfile->arps);
    } else audstr = lives_strdup("");

    mainw->overlay_msg = lives_strdup_printf("LiVES version %s powered by Weed ABI version %d, Weed Filter API version %d,\n"
                         "RFX version %s, Clip Header Version %d\n"
                         "Rendering clip size %d X %d @ %.3f fps%s\n"
                         "Render time %s, %.3f fps avg.\n"
                         "Timecode %.6f : original frame %s\n",
                         LiVES_VERSION, WEED_ABI_VERSION, WEED_FILTER_API_VERSION,
                         RFX_VERSION, LIVES_CLIP_HEADER_VERSION,
                         cfile->hsize, cfile->vsize, cfile->fps, audstr, timestr, fps_avg,
                         currticks / TICKS_PER_SECOND_DBL, framestr);
    lives_free(framestr); lives_free(timestr); lives_free(audstr);

    if (tot_frames & 1) lives_memset(rgb, 0, 12);
    else rgb[0] = rgb[1] = rgb[2] = 65535;
    weed_set_int_array(layer, "fg_col", 3, rgb);
    layer = render_text_overlay(layer, mainw->overlay_msg, DEF_OVERLAY_SCALING);
    break;
  default:
    break;
  }
  return layer;
}


#define COUNT_CHKVAL 100 ///< how many frames between storage space checks

boolean transcode_clip(int start, int end, boolean internal, char *def_pname) {
  _vid_playback_plugin *vpp;
  weed_layer_t *frame_layer = NULL;
  lives_proc_thread_t coder = NULL;
  void *abuff = NULL;
  short *sbuff = NULL;
  float **fltbuf = NULL;
  ticks_t currticks;
  ssize_t in_bytes;
  char *pname;
  const char *img_ext = NULL;
  char *afname = NULL;
  char *msg = NULL;
  double spf = 0., ospf;

  boolean audio = FALSE;
  boolean swap_endian = FALSE;
  boolean error = FALSE;
  boolean fx1_bool = mainw->fx1_bool;
  boolean needs_dprint = FALSE;
  boolean manage_ds = FALSE;

  int fd = -1;
  int asigned = 0, aendian = 0;
  int nsamps;
  int interp = LIVES_INTERP_FAST; /// TODO - get quality setting
  int width, height, pwidth, pheight;
  int tgamma = WEED_GAMMA_SRGB;
  int count = 0;
  int pbq = prefs->pb_quality;

  int i = 0, j;

  out_asamps = cfile->asampsize;

  if (cfile->asampsize == 32) cfile->asampsize = 16;

  THREAD_INTENTION = LIVES_INTENTION_TRANSCODE;

  if (def_pname) pname = lives_strdup(def_pname);
  else pname = NULL;

  if (!internal) {
    if (!transcode_prep()) {
      THREAD_INTENTION = LIVES_INTENTION_NOTHING;
      return FALSE;
    } else {
      lives_capacity_t *caps = lives_capacities_new();
      if (mainw->save_with_sound && cfile->achans * cfile->arps > 0) {
        lives_capacity_set_int(caps, LIVES_CAPACITY_AUDIO_RATE, cfile->arate);
        lives_capacity_set_int(caps, LIVES_CAPACITY_AUDIO_CHANS, cfile->achans);
      } else lives_capacity_set_int(caps, LIVES_CAPACITY_AUDIO_CHANS, cfile->achans);

      vpp = mainw->vpp;
      THREAD_CAPACITIES = caps;
      if (!(pname = transcode_get_params(pname))) {
        THREAD_INTENTION = LIVES_INTENTION_NOTHING;
        lives_capacities_free(caps);
        THREAD_CAPACITIES = NULL;
        goto tr_err2;
      }
      lives_capacities_free(caps);
      THREAD_CAPACITIES = NULL;
    }
  } else {
    vpp = mainw->vpp;
    mainw->transrend_ready = TRUE;
    lives_proc_thread_set_cancellable(mainw->transrend_proc);
    lives_nanosleep_while_false(!mainw->transrend_ready
                                || lives_proc_thread_get_cancelled(mainw->transrend_proc));
    if (lives_proc_thread_get_cancelled(mainw->transrend_proc)) goto tr_err2;
  }

  // (re)set these for the current clip
  // TODO: need to make sure this is an allowed value for the plugin !!!
  if (vpp->set_fps)(*vpp->set_fps)(cfile->fps);

  (*vpp->set_palette)(vpp->palette);
  if (weed_palette_is_yuv(vpp->palette)) {
    if (vpp->set_yuv_palette_clamping)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);
  }
  //
  if (vpp->init_audio && mainw->save_with_sound && cfile->achans * cfile->arps > 0) {
    int in_arate = cfile->arate;
    if ((*vpp->init_audio)(in_arate, cfile->achans, mainw->vpp->extra_argc, mainw->vpp->extra_argv)) {
      // we will buffer audio and send it in packets of one frame worth of audio
      // buffers will be used to convert to float audio

      audio = TRUE;
      ospf = spf = (double)cfile->arate / cfile->fps;

      afname = lives_build_filename(prefs->workdir, cfile->handle, CLIP_AUDIO_FILENAME, NULL);
      fd = lives_open_buffered_rdonly(afname);
      lives_buffered_rdonly_slurp(fd, 0);

      if (fd < 0) {
        do_read_failed_error_s(afname, lives_strerror(errno));
        error = TRUE;
        goto tr_err;
      }

      if (!internal)
        lives_lseek_buffered_rdonly(fd, (int)((double)(cfile->start - 1.) * spf) * cfile->achans * (cfile->asampsize >> 3));

      asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
      aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;

      if (cfile->asampsize > 8 && cfile->asampsize < 32) {
        if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
          swap_endian = TRUE;
        else swap_endian = FALSE;
      }

      abuff = lives_malloc((int)(spf + 1.) * cfile->achans * (cfile->asampsize >> 3)); // one extra sample to allow for rounding
      if (!abuff) {
        error = TRUE;
        goto tr_err;
      }
      fltbuf = lives_malloc(cfile->achans * sizeof(float *));
      if (!fltbuf) {
        error = TRUE;
        goto tr_err;
      }

      for (i = 0; i < cfile->achans; i++) {
        fltbuf[i] = (float *)lives_malloc((int)(spf + 1.) * cfile->achans * sizeof(float));  // one extra sample to allow for rounding
        if (!fltbuf[i]) {
          error = TRUE;
          goto tr_err;
        }
        if (cfile->asampsize == 8) {
          sbuff = (short *)lives_malloc((int)(spf + 1.) * cfile->achans * 2); // one extra sample to allow for rounding
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (!(*vpp->init_screen)(vpp->fwidth, vpp->fheight, FALSE, 0, vpp->extra_argc, vpp->extra_argv)) {
    error = TRUE;
    goto tr_err;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// plugin ready

  //av_log_set_level(AV_LOG_FATAL);
  prefs->pb_quality = PB_QUALITY_HIGH;
  THREADVAR(rowstride_alignment_hint) = 16;

  if (!internal) {
    // create a frame layer,
    frame_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_set_int_value(frame_layer, WEED_LEAF_CLIP, mainw->current_file);
    weed_layer_set_palette_yuv(frame_layer, vpp->palette, vpp->YUV_clamping, vpp->YUV_sampling, vpp->YUV_subspace);

    // need img_ext for pulling the frame
    img_ext = get_image_ext_for_type(cfile->img_type);

    mainw->cancel_type = CANCEL_SOFT; // force "Enough" button to be shown

    msg = lives_strdup_printf(_("Quick transcoding to %s..."), pname);
    do_threaded_dialog(msg, TRUE);
    d_print(msg);

    needs_dprint = TRUE;
  }

  width = pwidth = vpp->fwidth;
  height = pheight = vpp->fheight;

  lives_snprintf(cfile->save_file_name, PATH_MAX, "%s", pname);

  if (!internal) {
    // for internal, the renderer will check diskspace levels
    if (capable->mountpoint && *capable->mountpoint) {
      /// if output is on same volume as workdir
      // then we monitor continuously
      char *mpf = get_mountpoint_for(pname);
      if (mpf && *mpf && !lives_strcmp(mpf, capable->mountpoint)) {
        manage_ds = TRUE;
      }
    }
  }

  startt = lives_get_current_ticks();
  tot_frames = 0;
  // encoding loop
  for (i = start; !end || i <= end; i++) {
    if (manage_ds) {
      if (count++ == COUNT_CHKVAL)
        if (!check_storage_space(-1, TRUE)) break;
    }

    // set the frame number to pull
    currticks = q_gint64((i - start) / cfile->fps * TICKS_PER_SECOND_DBL,
                         cfile->fps);
    if (!internal) {
      weed_set_int_value(frame_layer, WEED_LEAF_FRAME, i);

      // - pull next frame (thread)
      pull_frame_threaded(frame_layer, img_ext, (weed_timecode_t)currticks, cfile->hsize, cfile->vsize);

      if (mainw->fx1_bool) {
        frame_layer = on_rte_apply(frame_layer, vpp->fwidth, vpp->fheight, (weed_timecode_t)currticks);
      }
    } else {
      lives_nanosleep_while_false(mainw->transrend_ready
                                  || lives_proc_thread_get_cancelled(mainw->transrend_proc));
      if (lives_proc_thread_get_cancelled(mainw->transrend_proc)) goto tr_err;
      if (mainw->cancelled != CANCEL_NONE) break;
      frame_layer = mainw->transrend_layer;
      weed_layer_ref(frame_layer);
    }

#ifdef MATCH_PALETTES
    if (i == start) {
      // try to match palettes
      int *pal_list = (*vpp->get_palette_list)();
      get_best_palette_match(weed_layer_get_palette(frame_layer), pal_list, &vpp->palette, &vpp->YUV_clamping);
      (*vpp->set_palette)(vpp->palette);
      if (weed_palette_is_yuv(vpp->palette)) {
        if (vpp->set_yuv_palette_clamping)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);
      }
      if (!(*vpp->init_screen)(vpp->fwidth, vpp->fheight, FALSE, 0, vpp->extra_argc, vpp->extra_argv)) {
        error = TRUE;
        goto tr_err;
      }
    }
#endif

    if (audio) {
      // - read 1 frame worth of audio, to float, send
      THREADVAR(read_failed) = FALSE;
      in_bytes = lives_read_buffered(fd, abuff, (size_t)spf * cfile->achans * (cfile->asampsize >> 3), TRUE);
      if (THREADVAR(read_failed) || in_bytes < 0) {
        error = TRUE;
        goto tr_err;
      }

      if (in_bytes == 0) {
        // eof, flush audio
        // exit_screen() will flush anything left over
        (*mainw->vpp->render_audio_frame_float)(NULL, 0);
      } else {
        nsamps = in_bytes / cfile->achans / (cfile->asampsize >> 3);

        for (j = 0; j < cfile->achans; j++) {
          // convert to float
          if (cfile->asampsize == 32) {
            float_deinterleave(fltbuf[j], (float *)abuff, (uint64_t)nsamps, 1.,
                               cfile->achans, lives_vol_from_linear(cfile->vol));
          } else if (cfile->asampsize == 16) {
            sample_move_d16_float(fltbuf[j], (short *)abuff, (uint64_t)nsamps,
                                  cfile->achans, asigned ? AFORM_SIGNED : AFORM_UNSIGNED, swap_endian,
                                  lives_vol_from_linear(cfile->vol));
          } else {
            sample_move_d8_d16(sbuff, (uint8_t *)abuff, (uint64_t)nsamps, in_bytes,
                               1.0, cfile->achans, cfile->achans, !asigned ? SWAP_U_TO_S : 0);
            sample_move_d16_float(fltbuf[j], sbuff, (uint64_t)nsamps, cfile->achans, AFORM_SIGNED, FALSE,
                                  lives_vol_from_linear(cfile->vol));
          }
        }

        if (!internal) {
          if (mainw->fx1_bool) {
            // apply any audio effects with in_channels
            if (has_audio_filters(AF_TYPE_ANY)) {
              weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
              weed_layer_set_audio_data(layer, fltbuf, cfile->arate, cfile->achans, nsamps);
              weed_apply_audio_effects_rt(layer, currticks, FALSE, FALSE);
              lives_free(fltbuf);
              fltbuf = (float **)weed_layer_get_audio_data(layer, NULL);
              weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
              weed_layer_free(layer);
            }
          }
        }
        (*mainw->vpp->render_audio_frame_float)(fltbuf, nsamps);
      }
      // account for rounding errors
      spf = ospf + (spf - (double)((int)spf));
    }

    // get frame, send it
    //if (deinterlace) weed_leaf_set(frame_layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
    // ensure all threads are complete. optionally deinterlace, optionally overlay subtitles.
    check_layer_ready(frame_layer);

    width = weed_layer_get_width_pixels(frame_layer);
    height = weed_layer_get_height(frame_layer);

    if (prefs->enc_letterbox && (pwidth != width || pheight != height)) {
      if (!internal)
        get_letterbox_sizes(&pwidth, &pheight, &width, &height, (mainw->vpp->capabilities & VPP_CAN_RESIZE));
      else
        calc_maxspect(pwidth, pheight, &width, &height);
      if (!letterbox_layer(frame_layer, pwidth, pheight, width, height, interp, vpp->palette, vpp->YUV_clamping)) goto tr_err;
    }

    if (((width ^ pwidth) >> 2) || ((height ^ pheight) >> 1)) {
      if (!resize_layer(frame_layer, pwidth, pheight, interp, vpp->palette, vpp->YUV_clamping)) {
        //break_me("tranres");
        goto tr_err;
      }
    }

    if (weed_palette_is_rgb(mainw->vpp->palette)) {
      if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
        tgamma = WEED_GAMMA_LINEAR;
    } else {
      if (vpp->YUV_subspace == WEED_YUV_SUBSPACE_BT709)
        tgamma = WEED_GAMMA_BT709;
    }

    convert_layer_palette_full(frame_layer, vpp->palette, vpp->YUV_clamping, vpp->YUV_sampling, vpp->YUV_subspace, tgamma);
    gamma_convert_layer(tgamma, frame_layer);

    if (coder) {
      error = lives_proc_thread_join_boolean(coder);
      lives_proc_thread_free(coder);
      coder = NULL;
    } else error = FALSE;

    if (!error) {
      weed_layer_t *copy_frame_layer = weed_layer_copy(NULL, frame_layer);

      if (internal) {
        weed_layer_unref(frame_layer);
        frame_layer = NULL;
      }

      copy_frame_layer = apply_watermark(copy_frame_layer, currticks);

      coder = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)send_layer,
                                       WEED_SEED_BOOLEAN, "PVI", copy_frame_layer, vpp, currticks);
    }

    if (error) goto tr_err;

    if (!internal) {
      // update progress dialog with fraction done
      // (for internal, the frames would be passed from the player, calling render_events
      // so there is a normal progress dialog which is updated in the player)
      threaded_dialog_spin(1. - (double)(cfile->end - i) / (double)(cfile->end - cfile->start + 1.));
    } else {
      frame_layer = NULL;
      mainw->transrend_ready = FALSE;
    }
    tot_frames++;
    if (mainw->cancelled != CANCEL_NONE) break;
  }

  //// encoding done

tr_err:
  if (internal && frame_layer) weed_layer_unref(frame_layer);

  mainw->cancel_type = CANCEL_KILL;
  prefs->pb_quality = pbq;

  mainw->fx1_bool = fx1_bool;

  if (coder) {
    // do b4 exit_screen
    error = lives_proc_thread_join_boolean(coder);
    lives_proc_thread_free(coder);
  }

  // flush streams, write headers, plugin cleanup
  if (vpp && vpp->exit_screen) {
    (*vpp->exit_screen)(0, 0);
  }

  if (!internal) {
    // terminate the progress dialog
    end_threaded_dialog();
  }

tr_err2:
  transcode_cleanup(vpp);
  THREAD_INTENTION = LIVES_INTENTION_NOTHING;
  if (!internal) {
    if (needs_dprint) {
      if (mainw->cancelled != CANCEL_NONE) {
        d_print_enough(i - start + 1);
        mainw->cancelled = CANCEL_NONE;
      } else {
        if (!error) d_print_done();
        else d_print_failed();
      }
    }
    mainw->no_switch_dprint = FALSE;
  }

  if (!error && mainw->cancelled == CANCEL_NONE) {
    global_recent_manager_add(pname);
  }

  lives_free(pname);
  lives_freep((void **)&msg);

  if (!internal && frame_layer) weed_layer_free(frame_layer);

  if (fd >= 0) lives_close_buffered(fd);

  lives_freep((void **)&afname);

  lives_freep((void **)&abuff);
  if (fltbuf) {
    for (i = 0; i < cfile->achans; lives_freep((void **) & (fltbuf[i++])));
    lives_free(fltbuf);
  }

  lives_freep((void **)&sbuff);

  if (internal) mainw->transrend_ready = FALSE;

  return !error;
}

#endif
