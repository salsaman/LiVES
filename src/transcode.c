/// experimental feature
// transcode.c
// LiVES
// (c) G. Finch 2008 - 2017 <salsaman_lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// fast transcoding via a plugin

#ifdef LIBAV_TRANSCODE

#include "main.h"
#include "resample.h"
#include "effects.h"
#include "transcode.h"
#include "paramwindow.h"

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-palettes.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-host.h"
#endif

#include "effects-weed.h"


boolean send_layer(weed_plant_t *layer, _vid_playback_plugin *vpp, int64_t timecode) {
  // send a weed layer to a (prepared) video playback plugin
  // warning: will quite likely change the pixel_data of layer

  // returns: TRUE on rendering error

  void **pd_array;
  boolean error = FALSE;

  // convert to the plugin's palette
  convert_layer_palette(layer, vpp->palette, vpp->YUV_clamping);

  if (weed_palette_is_rgb_palette(mainw->vpp->palette)) {
    if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
      gamma_correct_layer(WEED_GAMMA_LINEAR, layer);
    else
      gamma_correct_layer(WEED_GAMMA_SRGB, layer);
  }

  // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
  compact_rowstrides(layer);

  // get a void ** to the planar pixel_data
  pd_array = weed_layer_get_pixel_data(layer);

  if (pd_array != NULL) {
    // send pixel data to the video frame renderer
    error = !(*vpp->render_frame)(weed_layer_get_width(layer),
                                  weed_layer_get_height(layer),
                                  timecode, pd_array, NULL, NULL);

    lives_free(pd_array);
  }
  return error;
}


boolean transcode(int start, int end) {
  _vid_playback_plugin *vpp, *ovpp;
  _vppaw *vppa;
  weed_plant_t *frame_layer = NULL;

  lives_rfx_t *rfx = NULL;

  void *abuff = NULL;

  short *sbuff = NULL;

  float **fltbuf = NULL;

  int64_t currticks;

  ssize_t in_bytes;

  LiVESList *retvals = NULL;

  const char *img_ext = NULL;

  char *afname = NULL;
  char *pname = NULL;
  char *msg = NULL, *tmp;

  double spf = 0., ospf;

  boolean audio = FALSE;
  boolean swap_endian = FALSE;
  boolean error = FALSE;
  boolean fx1_bool = mainw->fx1_bool;
  boolean needs_dprint = FALSE;

  int fd = -1;
  int resp;
  int asigned = 0, aendian = 0;
  int nsamps;

  register int i = 0, j;

  mainw->suppress_dprint = TRUE;

  ovpp = mainw->vpp;
  vpp = open_vid_playback_plugin(TRANSCODE_PLUGIN_NAME, FALSE);

  mainw->suppress_dprint = FALSE;
  mainw->no_switch_dprint = TRUE;

  if (vpp == NULL) {
    mainw->vpp = ovpp;
    d_print(_("Plugin %s not found.\n"), TRANSCODE_PLUGIN_NAME);
    mainw->no_switch_dprint = FALSE;
    return FALSE;
  }

  pname = lives_build_filename(mainw->vid_save_dir, DEF_TRANSCODE_FILENAME, NULL);

  // for now we can only have one instance of a vpp: TODO - make vpp plugins re-entrant
  memset(future_prefs->vpp_name, 0, 1);
  mainw->vpp = vpp;

  // create the param window for the plugin
  vppa = on_vpp_advanced_clicked(NULL, LIVES_INT_TO_POINTER(1));

  // keep this, stop it from being freed
  rfx = vppa->rfx;
  if (rfx == NULL) goto tr_err2;
  vppa->keep_rfx = TRUE;

  // set the default value in the param window
  set_rfx_param_by_name_string(rfx, TRANSCODE_PARAM_FILENAME, pname, TRUE);
  lives_freep((void **)&pname);

  retvals = do_onchange_init(rfx);
  if (retvals != NULL) {
    // now apply visually anything we got from onchange_init
    param_demarshall(rfx, retvals, TRUE, TRUE);
    lives_list_free_all(&retvals);
  }

  // run the param window
  do {
    resp = lives_dialog_run(LIVES_DIALOG(vppa->dialog));
  } while (resp == LIVES_RESPONSE_RETRY);

  if (resp == LIVES_RESPONSE_CANCEL) {
    mainw->cancelled = CANCEL_USER;
    rfx_free(rfx);
    lives_free(rfx);
    goto tr_err2;
  }

  mainw->cancelled = CANCEL_NONE;
  // get the param value ourselves
  get_rfx_param_by_name_string(rfx, TRANSCODE_PARAM_FILENAME, (char **)&pname);
  tmp = lives_build_filename(prefs->workdir, rfx->name, NULL);
  lives_rm(tmp);
  lives_free(tmp);
  rfx_free(rfx);
  lives_free(rfx);

  // (re)set these for the current clip
  if (vpp->set_fps != NULL)(*vpp->set_fps)(cfile->fps);

  if (vpp->set_palette != NULL)(*vpp->set_palette)(vpp->palette);
  if (vpp->set_yuv_palette_clamping != NULL)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

  if (vpp->init_audio != NULL && mainw->save_with_sound && cfile->achans * cfile->arps > 0) {
    int in_arate = (int)((float)cfile->arps / (float)cfile->arate * (float)cfile->arps);
    if ((*vpp->init_audio)(in_arate, cfile->achans, mainw->vpp->extra_argc, mainw->vpp->extra_argv)) {
      // we will buffer audio and send it in packets of one frame worth of audio
      // buffers will be used to convert to float audio

      audio = TRUE;
      ospf = spf = (double)in_arate / cfile->fps;

      afname = lives_build_filename(prefs->workdir, cfile->handle, CLIP_AUDIO_FILENAME, NULL);
      fd = lives_open_buffered_rdonly(afname);

      if (fd < 0) {
        do_read_failed_error_s(afname, lives_strerror(errno));
        error = TRUE;
        goto tr_err;
      }

      lives_lseek_buffered_rdonly(fd, (int)((double)(cfile->start - 1.) * spf) * cfile->achans * (cfile->asampsize >> 3));

      asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
      aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;

      if (cfile->asampsize > 8) {
        if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
          swap_endian = TRUE;
        else swap_endian = FALSE;
      }

      abuff = lives_malloc((int)(spf + 1.) * cfile->achans * (cfile->asampsize >> 3)); // one extra sample to allow for rounding
      if (abuff == NULL) {
        error = TRUE;
        goto tr_err;
      }
      fltbuf = lives_malloc(cfile->achans * sizeof(float *));
      if (fltbuf == NULL) {
        error = TRUE;
        goto tr_err;
      }

      for (i = 0; i < cfile->achans; i++) {
        fltbuf[i] = (float *)lives_malloc((int)(spf + 1.) * cfile->achans * sizeof(float));  // one extra sample to allow for rounding
        if (fltbuf[i] == NULL) {
          error = TRUE;
          goto tr_err;
        }
        if (cfile->asampsize == 8) {
          sbuff = (short *)lives_malloc((int)(spf + 1.) * cfile->achans * 2); // one extra sample to allow for rounding
        }
      }
    }
  }

  if (!(*vpp->init_screen)(vpp->fwidth, vpp->fheight, FALSE, 0, vpp->extra_argc, vpp->extra_argv)) {
    error = TRUE;
    goto tr_err;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// plugin ready

  //av_log_set_level(AV_LOG_FATAL);
  mainw->rowstride_alignment_hint = 16;

  // create a frame layer,
  frame_layer = weed_layer_new();
  weed_set_int_value(frame_layer, WEED_LEAF_CLIP, mainw->current_file);

  // need img_ext for pulling the frame
  img_ext = get_image_ext_for_type(cfile->img_type);

  mainw->cancel_type = CANCEL_SOFT; // force "Enough" button to be shown

  msg = lives_strdup_printf(_("Quick transcoding to %s..."), pname);
  do_threaded_dialog(msg, TRUE);
  d_print(msg);

  needs_dprint = TRUE;

  // encoding loop
  for (i = start; i <= end; i++) {
    // set the frame number to pull
    weed_set_int_value(frame_layer, WEED_LEAF_FRAME, i);

    // - pull next frame (thread)
    pull_frame_threaded(frame_layer, img_ext, (weed_timecode_t)(currticks = q_gint64((i - start) / cfile->fps * TICKS_PER_SECOND_DBL,
                        cfile->fps)));

    if (mainw->fx1_bool) {
      frame_layer = on_rte_apply(frame_layer, cfile->hsize, cfile->vsize, (weed_timecode_t)currticks);
    }

    if (audio) {
      // - read 1 frame worth of audio, to float, send
      mainw->read_failed = FALSE;
      in_bytes = lives_read_buffered(fd, abuff, (size_t)spf * cfile->achans * (cfile->asampsize >> 3), TRUE);
      if (mainw->read_failed || in_bytes < 0) {
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
          if (cfile->asampsize == 16) {
            sample_move_d16_float(fltbuf[j], (short *)abuff, (uint64_t)nsamps, cfile->achans, asigned ? AFORM_SIGNED : AFORM_UNSIGNED, swap_endian,
                                  1.0);
          } else {
            sample_move_d8_d16(sbuff, (uint8_t *)abuff, (uint64_t)nsamps, in_bytes,
                               1.0, cfile->achans, cfile->achans, !asigned ? SWAP_U_TO_S : 0);
            sample_move_d16_float(fltbuf[j], sbuff, (uint64_t)nsamps, cfile->achans, AFORM_SIGNED, FALSE, 1.0);
          }
        }

        if (mainw->fx1_bool) {
          // apply any audio effects with in_channels
          if (has_audio_filters(AF_TYPE_ANY)) weed_apply_audio_effects_rt(fltbuf, cfile->achans, nsamps, cfile->arate, currticks, FALSE);
        }
        (*mainw->vpp->render_audio_frame_float)(fltbuf, nsamps);
      }
      // account for rounding errors
      spf = ospf + (spf - (double)((int)spf));
    }

    // get frame, send it
    //if (deinterlace) weed_leaf_set(frame_layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
    check_layer_ready(frame_layer); // ensure all threads are complete. optionally deinterlace, optionally overlay subtitles.

    gamma_correct_layer(WEED_GAMMA_SRGB, frame_layer);

    error = send_layer(frame_layer, vpp, currticks);

    // free pixel_data, but keep same layer around
    weed_layer_pixel_data_free(frame_layer);

    if (error) goto tr_err;

    // update progress dialog with fraction done
    threaded_dialog_spin(1. - (double)(cfile->end - i) / (double)(cfile->end - cfile->start + 1.));

    if (mainw->cancelled != CANCEL_NONE) {
      break;
    }
  }

  //// encoding done

tr_err:
  mainw->cancel_type = CANCEL_KILL;

  mainw->fx1_bool = fx1_bool;

  // flush streams, write headers, plugin cleanup
  if (vpp != NULL && vpp->exit_screen != NULL) {
    (*vpp->exit_screen)(0, 0);
  }

  // terminate the progress dialog
  end_threaded_dialog();

tr_err2:
  // close vpp, unless mainw->vpp
  if (ovpp == NULL || (vpp->handle != ovpp->handle)) {
    close_vid_playback_plugin(vpp);
  }

  if (ovpp != NULL && (vpp->handle == ovpp->handle)) {
    // we "borrowed" the playback plugin, so set these back how they were
    if (ovpp->set_fps != NULL)(*ovpp->set_fps)(ovpp->fixed_fpsd);
    if (ovpp->set_palette != NULL)(*ovpp->set_palette)(ovpp->palette);
    if (ovpp->set_yuv_palette_clamping != NULL)(*ovpp->set_yuv_palette_clamping)(ovpp->YUV_clamping);
  }

  mainw->vpp = ovpp;

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

  lives_freep((void **)&pname);
  lives_freep((void **)&msg);

  weed_layer_free(frame_layer);

  if (fd >= 0) lives_close_buffered(fd);

  lives_freep((void **)&afname);

  lives_freep((void **)&abuff);
  if (fltbuf != NULL) {
    for (i = 0; i < cfile->achans; lives_freep((void **) & (fltbuf[i++])));
    lives_free(fltbuf);
  }

  lives_freep((void **)&sbuff);

  return !error;
}

#endif
