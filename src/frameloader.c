// frameloader.c
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"
#include "cvirtual.h"
#include "interface.h"
#include "effects-weed.h"
#include "effects.h"
#include "videodev.h"
#include "stream.h"
#include "callbacks.h"
#include "startup.h"
#include "ce_thumbs.h"

#include <png.h>
#include <setjmp.h>

static int xxwidth = 0, xxheight = 0;

////////////////// internal filenaming

LIVES_GLOBAL_INLINE const char *get_image_ext_for_type(lives_img_type_t imgtype) {
  switch (imgtype) {
  case IMG_TYPE_JPEG: return LIVES_FILE_EXT_JPG; // "jpg"
  case IMG_TYPE_PNG: return LIVES_FILE_EXT_PNG; // "png"
  default: return "";
  }
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_ext_to_img_type(const char *img_ext) {
  return lives_image_type_to_img_type(image_ext_to_lives_image_type(img_ext));
}


LIVES_GLOBAL_INLINE const char *image_ext_to_lives_image_type(const char *img_ext) {
  if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) return LIVES_IMAGE_TYPE_PNG;
  if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) return LIVES_IMAGE_TYPE_JPEG;
  return LIVES_IMAGE_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_type_to_img_type(const char *lives_img_type) {
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_PNG)) return IMG_TYPE_PNG;
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_JPEG)) return IMG_TYPE_JPEG;
  return IMG_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE char *make_image_file_name(lives_clip_t *sfile, frames_t frame,
    const char *img_ext) {
  char *fname, *ret;
  const char *ximg_ext = img_ext;
  if (!ximg_ext || !*ximg_ext) {
    sfile->img_type = resolve_img_type(sfile);
    ximg_ext = get_image_ext_for_type(sfile->img_type);
  }
  fname = lives_strdup_printf("%08d.%s", frame, ximg_ext);
  ret = lives_build_filename(prefs->workdir, sfile->handle, fname, NULL);
  lives_free(fname);
  return ret;
}


LIVES_GLOBAL_INLINE char *make_image_short_name(lives_clip_t *sfile, frames_t frame, const char *img_ext) {
  const char *ximg_ext = img_ext;
  if (!ximg_ext) ximg_ext = get_image_ext_for_type(sfile->img_type);
  return lives_strdup_printf("%08d.%s", frame, ximg_ext);
}


/** @brief check number of frames is correct
    for files of type CLIP_TYPE_DISK
    - check the image files (e.g. jpeg or png)

    use a "goldilocks" algorithm (just the right frames, not too few and not too many)

    ignores gaps */
boolean check_frame_count(int idx, boolean last_checked) {
  /// make sure nth frame is there...
  char *frame;
  if (mainw->files[idx]->frames > 0) {
    frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames,
                                 get_image_ext_for_type(mainw->files[idx]->img_type));
    if (!lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
      // not enough frames
      lives_free(frame);
      return FALSE;
    }
    lives_free(frame);
  }

  /// ...make sure n + 1 th frame is not
  frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames + 1,
                               get_image_ext_for_type(mainw->files[idx]->img_type));

  if (lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
    /// too many frames
    lives_free(frame);
    return FALSE;
  }
  lives_free(frame);

  /// just right
  return TRUE;
}


/////// disk checks


/** @brief sets mainw->files[idx]->frames with current framecount

    calls smogrify which physically finds the last frame using a (fast) O(log n) binary search method
    for CLIP_TYPE_DISK only
    (CLIP_TYPE_FILE should use the decoder plugin frame count) */
frames_t get_frame_count(int idx, int start) {
  ssize_t bytes;
  char *com = lives_strdup_printf("%s count_frames \"%s\" %s %d", prefs->backend_sync, mainw->files[idx]->handle,
                                  get_image_ext_for_type(mainw->files[idx]->img_type), start);

  bytes = lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (THREADVAR(com_failed)) return 0;

  if (bytes > 0) return atoi(mainw->msg);
  return 0;
}


boolean get_frames_sizes(int fileno, frames_t frame, int *hsize, int *vsize) {
  // get the actual physical frame size
  lives_clip_t *sfile = mainw->files[fileno];
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  char *fname = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, LIVES_LAYER_GET_SIZE_ONLY);
  if (!weed_layer_create_from_file_progressive(layer, fname, 0, 0, WEED_PALETTE_END,
      get_image_ext_for_type(sfile->img_type))) {
    lives_free(fname);
    return FALSE;
  }
  lives_free(fname);
  *hsize = weed_layer_get_width(layer);
  *vsize = weed_layer_get_height(layer);
  weed_layer_unref(layer);
  return TRUE;
}


LIVES_GLOBAL_INLINE void *set_md5_for_frame(int clipno, frames_t frame, weed_layer_t *layer) {
  if (!!layer) return NULL;
  else {
    void *xmd5sum = weed_get_voidptr_value(layer, LIVES_LEAF_MD5SUM, NULL);
    if (!xmd5sum) {
      lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clipno);
      if (sfile && !is_virtual_frame(clipno, frame)) {
        int64_t fsize;
        char *fname = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
        xmd5sum = lives_md5_sum(fname, &fsize);
        lives_free(fname);
        if (xmd5sum) {
          weed_set_voidptr_value(layer, LIVES_LEAF_MD5SUM, xmd5sum);
          weed_set_int64_value(layer, LIVES_LEAF_MD5_CHKSIZE, fsize);
        }
      } else {
        weed_leaf_delete(layer, LIVES_LEAF_MD5SUM);
        weed_leaf_delete(layer, LIVES_LEAF_MD5_CHKSIZE);
      }
    }
    return xmd5sum;
  }
}


weed_layer_t *set_if_md5_valid(int clipno, frames_t frame, weed_layer_t *layer) {
  if (!layer) return NULL;
  if (!weed_plant_has_leaf(layer, LIVES_LEAF_MD5SUM)) {
    set_md5_for_frame(clipno, frame, layer);
    return layer;
  } else {
    lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clipno);
    if (sfile && !is_virtual_frame(clipno, frame)) {
      char *fname = NULL;
      void *xmd5sum;
      if (weed_plant_has_leaf(layer, LIVES_LEAF_MD5_CHKSIZE)) {
        int64_t fsize = weed_get_int64_value(layer, LIVES_LEAF_MD5_CHKSIZE, NULL);
        if (fsize > 0) {
          fname = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
          if (!fname) return NULL;
          if (fsize != sget_file_size(fname)) {
            lives_free(fname);
            return NULL;
          }
        }
      }
      xmd5sum = weed_get_voidptr_value(layer, LIVES_LEAF_MD5SUM, NULL);
      if (xmd5sum) {
        if (!fname) fname  = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
        if (fname) {
          void *md5sum = get_md5sum(fname);
          lives_free(fname);
          if (md5sum) {
            if (!lives_memcmp(md5sum, xmd5sum, MD5_SIZE)) {
              lives_free(md5sum);
              return layer;
            }
            lives_free(md5sum);
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  return NULL;
}

//////////////////////////////// GUI frame functions ////

void showclipimgs(void) {
  mainw->gui_much_events = TRUE;
  if (CURRENT_CLIP_IS_VALID) {
    load_end_image(cfile->end);
    load_start_image(cfile->start);
  } else {
    load_end_image(0);
    load_start_image(0);
  }
  lives_widget_queue_draw_and_update(mainw->start_image);
  lives_widget_queue_draw_and_update(mainw->end_image);
  mainw->gui_much_events = FALSE;
}


void load_start_image(frames_t frame) {
  LiVESPixbuf *start_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  weed_layer_t *layer = NULL;
  weed_timecode_t tc;
  LiVESInterpType interp;
  char *fname = NULL;
  boolean expose = FALSE;
  boolean cache_it = TRUE;
  int rwidth, rheight, width, height;
  int tries = 2;
  frames_t xpf;

  if (!prefs->show_gui) return;
  if (mainw->multitrack) return;

  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((prefs->gui_monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1) &&
                                        (!mainw->ext_playback ||
                                         (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return;
  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->start_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int bx, by, hsize, vsize;
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;
    int vspace = get_vspace();
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    bx = abs(bx);
    by = abs(by);
    if (!mainw->hdrbar) {
      if (by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
                          + (prefs->show_msg_area ? mainw->mbar_res : 0)
                          + widget_opts.border_width * 2);
    if (LIVES_IS_PLAYING && mainw->double_size) {
      hsize /= 2;
      vsize /= 2;
    }
    lives_widget_set_size_request(mainw->start_image, hsize, vsize);
    lives_widget_set_size_request(mainw->frame1, hsize, vsize);
    lives_widget_set_size_request(mainw->eventbox3, hsize, vsize);

    lives_widget_set_hexpand(mainw->frame1, FALSE);
    lives_widget_set_vexpand(mainw->frame1, FALSE);
  }

  if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
    if (!mainw->camframe) {
      LiVESError *error = NULL;
      char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
      char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
      mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
      if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
      lives_free(tmp); lives_free(fname);
    }
    set_drawing_area_from_pixbuf(mainw->start_image, mainw->camframe, mainw->si_surface);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(mainw->start_image, mainw->imframe, mainw->si_surface);
    } else {
      set_drawing_area_from_pixbuf(mainw->start_image, NULL, mainw->si_surface);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->start_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->start_image, 0.4);
    }
    return;
  }

  xpf = ABS(get_indexed_frame(mainw->current_file, frame));

check_stcache:
  if (mainw->st_fcache) {
    if (lives_layer_get_clip(mainw->st_fcache) == mainw->current_file
        && lives_layer_get_frame(mainw->st_fcache) == xpf) {
      if (is_virtual_frame(mainw->current_file, frame)) layer = mainw->st_fcache;
      else {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          layer = set_if_md5_valid(mainw->current_file, frame, mainw->st_fcache);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-OFF*

  if (!layer) {
    if (mainw->st_fcache) {
      if (mainw->st_fcache != mainw->en_fcache && mainw->st_fcache != mainw->pr_fcache)
        weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) mainw->st_fcache = mainw->pr_fcache;
      else mainw->st_fcache = mainw->en_fcache;
      goto check_stcache;
    }
  }
  lives_freep((void **)&fname);

  tc = ((frame - 1.)) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect && !prefs->letterbox) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      lives_decoder_t *dplug = NULL;
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
#ifdef MULTI_SRC
      if (!mainw->go_away && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
        dplug = add_decoder_clone(mainw->current_file);
        if (dplug) weed_set_voidptr_value(layer, WEED_LEAF_HOST_DECODER, dplug);
      }
#endif
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          return;
        }
      } else if (dplug) {
        free_decoder_clone(mainw->current_file, dplug);
        weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
      }
    }

    if (!start_pixbuf) start_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(start_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->start_image, start_pixbuf, mainw->si_surface);
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, start_pixbuf)))
        lives_widget_object_unref(start_pixbuf);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (mainw->st_fcache && mainw->st_fcache != layer
          && mainw->en_fcache != mainw->st_fcache && mainw->pr_fcache != mainw->st_fcache)
        weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = NULL;
      if (layer) weed_layer_unref(layer);
    } else {
      if (!mainw->st_fcache) {
        mainw->st_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
	    set_md5_for_frame(mainw->current_file, frame, layer);
	    // *INDENT-OFF*
	  }}
        lives_layer_set_frame(layer, xpf);
	lives_layer_set_clip(layer, mainw->current_file);
      }}
    // *INDENT-ON*
    return;
  }

  do {
    width = cfile->hsize;
    height = cfile->vsize;

    // TODO *** - if width*height==0, show broken frame image

    rwidth = lives_widget_get_allocation_width(mainw->start_image);
    rheight = lives_widget_get_allocation_height(mainw->start_image);

    calc_maxspect(rwidth, rheight, &width, &height);
    width = (width >> 2) << 2;
    height = (height >> 2) << 2;

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        // TODO: make background thread
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      lives_decoder_t *dplug = NULL;
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
#ifdef MULTI_SRC
      if (!mainw->go_away && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
        dplug = add_decoder_clone(mainw->current_file);
        if (dplug) weed_set_voidptr_value(layer, WEED_LEAF_HOST_DECODER, dplug);
      }
#endif
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height, WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          weed_layer_unref(layer);
          return;
        }
      } else if (dplug) {
        free_decoder_clone(mainw->current_file, dplug);
        weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
      }
    }

    if (!start_pixbuf || lives_pixbuf_get_width(start_pixbuf) != width || lives_pixbuf_get_height(start_pixbuf) != height) {
      if (!orig_pixbuf) {
        if (layer) orig_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        else orig_pixbuf = start_pixbuf;
      }
      if (start_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(start_pixbuf)) {
        lives_widget_object_unref(start_pixbuf);
        start_pixbuf = NULL;
      }
      if (LIVES_IS_PIXBUF(orig_pixbuf)) {
        if (lives_pixbuf_get_width(orig_pixbuf) == width && lives_pixbuf_get_height(orig_pixbuf) == height)
          start_pixbuf = orig_pixbuf;
        else {
          start_pixbuf = lives_pixbuf_scale_simple(orig_pixbuf, width, height, LIVES_INTERP_BEST);
        }
      }
    }

    if (LIVES_IS_PIXBUF(start_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->start_image, start_pixbuf, mainw->si_surface);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_queue_resize(mainw->start_image);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } while (rwidth != lives_widget_get_allocation_width(mainw->start_image) ||
           rheight != lives_widget_get_allocation_height(mainw->start_image));
#else
  }
  while (FALSE);
#endif
  if (start_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(start_pixbuf)) {
    lives_widget_object_unref(start_pixbuf);
  }

  if (LIVES_IS_PIXBUF(orig_pixbuf)) {
    if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
      lives_widget_object_unref(orig_pixbuf);
  } else cache_it = FALSE;

  if (!cache_it) {
    if (mainw->st_fcache && mainw->st_fcache != layer
        && mainw->st_fcache != mainw->en_fcache && mainw->pr_fcache != mainw->st_fcache)
      weed_layer_unref(mainw->st_fcache);
    mainw->st_fcache = NULL;
    weed_layer_unref(layer);
  } else {
    if (!mainw->st_fcache) {
      mainw->st_fcache = layer;
      if (!is_virtual_frame(mainw->current_file, frame)) {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          set_md5_for_frame(mainw->current_file, frame, layer);
        }
      // *INDENT-OFF*
    }}
  lives_layer_set_frame(layer, xpf);
  lives_layer_set_clip(layer, mainw->current_file);
 }
// *INDENT-ON*
}


void load_end_image(frames_t frame) {
  LiVESPixbuf *end_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  weed_layer_t *layer = NULL;
  weed_timecode_t tc;
  LiVESInterpType interp;
  char *fname = NULL;
  boolean expose = FALSE;
  boolean cache_it = TRUE;
  int rwidth, rheight, width, height;
  int tries = 2;
  frames_t xpf;

  if (!prefs->show_gui) return;
  if (mainw->multitrack) return;

  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((prefs->gui_monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1) &&
                                        (!mainw->ext_playback ||
                                         (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return;
  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->end_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int bx, by, hsize, vsize;
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;
    int vspace = get_vspace();
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    bx = abs(bx);
    by = abs(by);
    if (!mainw->hdrbar) {
      if (by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
                          + (prefs->show_msg_area ? mainw->mbar_res : 0)
                          + widget_opts.border_width * 2);
    if (LIVES_IS_PLAYING && mainw->double_size) {
      hsize /= 2;
      vsize /= 2;
    }
    lives_widget_set_size_request(mainw->end_image, hsize, vsize);
    lives_widget_set_size_request(mainw->frame2, hsize, vsize);
    lives_widget_set_size_request(mainw->eventbox4, hsize, vsize);

    lives_widget_set_hexpand(mainw->frame2, FALSE);
    lives_widget_set_vexpand(mainw->frame2, FALSE);
  }

  if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
    if (!mainw->camframe) {
      LiVESError *error = NULL;
      char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
      char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
      mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
      if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
      lives_free(tmp); lives_free(fname);
    }
    set_drawing_area_from_pixbuf(mainw->end_image, mainw->camframe, mainw->ei_surface);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(mainw->end_image, mainw->imframe, mainw->ei_surface);
    } else {
      set_drawing_area_from_pixbuf(mainw->end_image, NULL, mainw->ei_surface);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->end_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->end_image, 0.4);
    }
    return;
  }

  xpf = ABS(get_indexed_frame(mainw->current_file, frame));
check_encache:
  if (mainw->en_fcache) {
    if (lives_layer_get_clip(mainw->en_fcache) == mainw->current_file
        && lives_layer_get_frame(mainw->en_fcache) == xpf) {
      if (is_virtual_frame(mainw->current_file, frame)) layer = mainw->en_fcache;
      else {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          layer = set_if_md5_valid(mainw->current_file, frame, mainw->en_fcache);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (!layer) {
    if (mainw->en_fcache) {
      if (mainw->en_fcache != mainw->st_fcache && mainw->en_fcache != mainw->pr_fcache)
        weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) mainw->en_fcache = mainw->pr_fcache;
      else mainw->en_fcache = mainw->st_fcache;
      goto check_encache;
    }
  }

  lives_freep((void **)&fname);

  tc = (frame - 1.) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect && !prefs->letterbox) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      lives_decoder_t *dplug = NULL;
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
#ifdef MULTI_SRC
      if (!mainw->go_away && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
        dplug = add_decoder_clone(mainw->current_file);
        if (dplug) weed_set_voidptr_value(layer, WEED_LEAF_HOST_DECODER, dplug);
      }

#endif
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          return;
        }
      } else {
        if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
      }
    }

    if (!end_pixbuf) end_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(end_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->end_image, end_pixbuf, mainw->ei_surface);
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, end_pixbuf)))
        lives_widget_object_unref(end_pixbuf);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (mainw->en_fcache && mainw->en_fcache != layer
          && mainw->en_fcache != mainw->st_fcache && mainw->pr_fcache != mainw->en_fcache)
        weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
      if (layer) weed_layer_unref(layer);
    } else {
      if (!mainw->en_fcache) {
        mainw->en_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            set_md5_for_frame(mainw->current_file, frame, layer);
	    // *INDENT-OFF*
	  }}
	// *INDENT-OFF*
	lives_layer_set_frame(layer, xpf);
	lives_layer_set_clip(layer, mainw->current_file);
      }}
    // *INDENT-ON*
    return;
  }

  do {
    width = cfile->hsize;
    height = cfile->vsize;

    rwidth = lives_widget_get_allocation_width(mainw->end_image);
    rheight = lives_widget_get_allocation_height(mainw->end_image);

    calc_maxspect(rwidth, rheight, &width, &height);
    width = (width >> 2) << 2;
    height = (height >> 2) << 2;

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      lives_decoder_t *dplug = NULL;
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
#ifdef MULTI_SRC
      if (!mainw->go_away && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
        dplug = add_decoder_clone(mainw->current_file);
        if (dplug) weed_set_voidptr_value(layer, WEED_LEAF_HOST_DECODER, dplug);
      }
#endif
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type),
                             tc, width, height, WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          weed_layer_unref(layer);
          return;
        }
      } else if (dplug) {
        free_decoder_clone(mainw->current_file, dplug);
        weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
      }
    }

    if (!end_pixbuf || lives_pixbuf_get_width(end_pixbuf) != width || lives_pixbuf_get_height(end_pixbuf) != height) {
      if (!orig_pixbuf) {
        if (layer) orig_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        else orig_pixbuf = end_pixbuf;
      }
      if (end_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(end_pixbuf)) {
        lives_widget_object_unref(end_pixbuf);
        end_pixbuf = NULL;
      }
      if (LIVES_IS_PIXBUF(orig_pixbuf)) {
        if (lives_pixbuf_get_width(orig_pixbuf) == width && lives_pixbuf_get_height(orig_pixbuf) == height)
          end_pixbuf = orig_pixbuf;
        else {
          end_pixbuf = lives_pixbuf_scale_simple(orig_pixbuf, width, height, LIVES_INTERP_BEST);
        }
      }
    }

    if (LIVES_IS_PIXBUF(end_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->end_image, end_pixbuf, mainw->ei_surface);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_queue_resize(mainw->end_image);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } while (rwidth != lives_widget_get_allocation_width(mainw->end_image) ||
           rheight != lives_widget_get_allocation_height(mainw->end_image));
#if 0
  {
#endif
#else
  } while (FALSE);
#endif

    if (end_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(end_pixbuf)) {
      lives_widget_object_unref(end_pixbuf);
    }

    if (LIVES_IS_PIXBUF(orig_pixbuf)) {
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
        lives_widget_object_unref(orig_pixbuf);
    } else cache_it = FALSE;


    if (!cache_it) {
      if (mainw->en_fcache && mainw->en_fcache != layer
          && mainw->en_fcache != mainw->st_fcache && mainw->pr_fcache != mainw->en_fcache)
        weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
      weed_layer_unref(layer);
    } else {
      if (!mainw->en_fcache) {
        mainw->en_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            set_md5_for_frame(mainw->current_file, frame, layer);
          }
	// *INDENT-OFF*
      }}
    lives_layer_set_frame(layer, xpf);
    lives_layer_set_clip(layer, mainw->current_file);
  }
  // *INDENT-ON*
  }


  void load_preview_image(boolean update_always) {
    // this is for the sepwin preview
    // update_always==TRUE = update widgets from mainw->preview_frame
    LiVESPixbuf *pixbuf = NULL;
    weed_layer_t *layer = NULL;
    boolean cache_it = TRUE;
    char *fname = NULL;
    frames_t xpf = -1;
    int tries = 2;
    int preview_frame;
    int width, height;

    if (!prefs->show_gui) return;
    if (LIVES_IS_PLAYING) return;

    lives_widget_set_opacity(mainw->preview_image, 1.);

    if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
      if (!mainw->camframe) {
        LiVESError *error = NULL;
        char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
        char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
        mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
        if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
        lives_free(tmp); lives_free(fname);
        fname = NULL;
      }
      pixbuf = lives_pixbuf_scale_simple(mainw->camframe, mainw->pwidth, mainw->pheight, LIVES_INTERP_BEST);
      set_drawing_area_from_pixbuf(mainw->preview_image, pixbuf, mainw->pi_surface);
      if (pixbuf) lives_widget_object_unref(pixbuf);
      mainw->preview_frame = 1;
      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1, 1);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_widget_set_size_request(mainw->preview_image, mainw->pwidth, mainw->pheight);
      return;
    }

    if (!CURRENT_CLIP_IS_NORMAL || !CURRENT_CLIP_HAS_VIDEO) {
      mainw->preview_frame = 0;
      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 0, 0);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 0);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);
      if (mainw->imframe) {
        lives_widget_set_size_request(mainw->preview_image, lives_pixbuf_get_width(mainw->imframe),
                                      lives_pixbuf_get_height(mainw->imframe));
        set_drawing_area_from_pixbuf(mainw->preview_image, mainw->imframe, mainw->pi_surface);
      } else set_drawing_area_from_pixbuf(mainw->preview_image, NULL, mainw->pi_surface);
      if (!palette || !(palette->style & STYLE_LIGHT)) {
        lives_widget_set_opacity(mainw->preview_image, 0.8);
      } else {
        lives_widget_set_opacity(mainw->preview_image, 0.4);
      }
      return;
    }

    if (!update_always) {
      // set current frame from spins, set range
      // set mainw->preview_frame to 0 before calling to force an update (e.g after a clip switch)
      switch (mainw->prv_link) {
      case PRV_END:
        preview_frame = cfile->end;
        break;
      case PRV_PTR:
        preview_frame = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
        break;
      case PRV_START:
        preview_frame = cfile->start;
        break;
      default:
        preview_frame = mainw->preview_frame > 0 ? mainw->preview_frame : 1;
        if (preview_frame > cfile->frames) preview_frame = cfile->frames;
        break;
      }

      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1, cfile->frames);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), preview_frame);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);

      mainw->preview_frame = preview_frame;
    }

    if (mainw->preview_frame < 1 || mainw->preview_frame > cfile->frames) {
      pixbuf = lives_pixbuf_scale_simple(mainw->imframe, cfile->hsize, cfile->vsize, LIVES_INTERP_BEST);
      if (!palette || !(palette->style & STYLE_LIGHT)) {
        lives_widget_set_opacity(mainw->preview_image, 0.4);
      } else {
        lives_widget_set_opacity(mainw->preview_image, 0.8);
      }
    } else {
      weed_timecode_t tc;
      xpf = ABS(get_indexed_frame(mainw->current_file, mainw->preview_frame));
check_prcache:
      if (mainw->pr_fcache) {
        if (lives_layer_get_clip(mainw->pr_fcache) == mainw->current_file
            && lives_layer_get_frame(mainw->pr_fcache) == xpf) {
          if (is_virtual_frame(mainw->current_file, mainw->preview_frame)) layer = mainw->pr_fcache;
          else {
            if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
              layer = set_if_md5_valid(mainw->current_file, mainw->preview_frame, mainw->pr_fcache);
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*
      if (!layer) {
        if (mainw->pr_fcache) {
          if (mainw->pr_fcache != mainw->st_fcache && mainw->pr_fcache != mainw->en_fcache)
            weed_layer_unref(mainw->pr_fcache);
          mainw->pr_fcache = NULL;
        }
        if (tries--) {
          if (tries == 1) mainw->pr_fcache = mainw->st_fcache;
          else mainw->pr_fcache = mainw->en_fcache;
          goto check_prcache;
        }
      }
      lives_freep((void **)&fname);

      // if we are not playing, and it would be slow to seek to the frame, convert it to an image
      if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE &&
          is_virtual_frame(mainw->current_file, mainw->preview_frame) &&
          cfile->ext_src) {
        lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
        if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
          virtual_to_images(mainw->current_file, mainw->preview_frame, mainw->preview_frame, FALSE, &pixbuf);
          cache_it = FALSE;
        }
      }

      width = cfile->hsize;
      height = cfile->vsize;
      if (prefs->ce_maxspect && !prefs->letterbox) {
        width = mainw->pwidth;
        height = mainw->pheight;
      }
      if (prefs->letterbox) {
        calc_maxspect(mainw->pwidth, mainw->pheight, &width, &height);
      }

      if (!layer && !pixbuf) {
        lives_decoder_t *dplug = NULL;
        layer = lives_layer_new_for_frame(mainw->current_file, mainw->preview_frame);
#ifdef MULTI_SRC
        if (cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, mainw->preview_frame)) {
          dplug = add_decoder_clone(mainw->current_file);
          if (dplug) weed_set_voidptr_value(layer, WEED_LEAF_HOST_DECODER, dplug);
        }
#endif
        tc = ((mainw->preview_frame - 1.)) / cfile->fps * TICKS_PER_SECOND;
        if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height,
                               WEED_PALETTE_RGB24)) {
          LiVESInterpType interp = get_interp_value(prefs->pb_quality, TRUE);
          check_layer_ready(layer);
          if (dplug) {
            free_decoder_clone(mainw->current_file, dplug);
            weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
          }
          if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
              !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
            weed_layer_unref(layer);
            return;
          }
        } else if (dplug) {
          free_decoder_clone(mainw->current_file, dplug);
          weed_leaf_delete(layer, WEED_LEAF_HOST_DECODER);
        }
      }

      if (!pixbuf || lives_pixbuf_get_width(pixbuf) != mainw->pwidth
          || lives_pixbuf_get_height(pixbuf) != mainw->pheight) {
        if (!pixbuf) {
          if (layer) pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        }
      }

      if (LIVES_IS_PIXBUF(pixbuf)) {
        LiVESPixbuf *pr_pixbuf = NULL;
        if (lives_pixbuf_get_width(pixbuf) == width && lives_pixbuf_get_height(pixbuf) == height)
          pr_pixbuf = pixbuf;
        else {
          pr_pixbuf = lives_pixbuf_scale_simple(pixbuf, width, height, LIVES_INTERP_BEST);
        }
        if (LIVES_IS_PIXBUF(pr_pixbuf)) {
          lives_widget_set_size_request(mainw->preview_image, MAX(width, mainw->sepwin_minwidth), height);
          set_drawing_area_from_pixbuf(mainw->preview_image, pr_pixbuf, mainw->pi_surface);
          if (pr_pixbuf != pixbuf) lives_widget_object_unref(pr_pixbuf);
          if (!layer || !cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, pixbuf)))
            lives_widget_object_unref(pixbuf);
        } else cache_it = FALSE;
      } else cache_it = FALSE;
    }

    if (!cache_it) {
      weed_layer_unref(layer);
      mainw->pr_fcache = NULL;
    } else {
      if (!mainw->pr_fcache) {
        mainw->pr_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, mainw->preview_frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            set_md5_for_frame(mainw->current_file, mainw->preview_frame, layer);
          }
        }
        lives_layer_set_frame(layer, xpf);
        lives_layer_set_clip(layer, mainw->current_file);
      }
    }
    // *INDENT-ON*

    if (update_always) {
      // set spins from current frame
      switch (mainw->prv_link) {
      case PRV_PTR:
        //cf. hrule_reset
        cfile->pointer_time = lives_ce_update_timeline(mainw->preview_frame, 0.);
        if (cfile->frames > 0) cfile->frameno = calc_frame_from_time(mainw->current_file,
                                                  cfile->pointer_time);
        if (cfile->pointer_time > 0.) {
          lives_widget_set_sensitive(mainw->rewind, TRUE);
          lives_widget_set_sensitive(mainw->trim_to_pstart, CURRENT_CLIP_HAS_AUDIO);
          lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
          if (mainw->preview_box) {
            lives_widget_set_sensitive(mainw->p_rewindbutton, TRUE);
          }
        }
        mainw->ptrtime = cfile->pointer_time;
        lives_widget_queue_draw(mainw->eventbox2);
        break;

      case PRV_START:
        if (mainw->st_fcache && mainw->st_fcache != mainw->pr_fcache && mainw->st_fcache != mainw->en_fcache) {
          weed_layer_unref(mainw->st_fcache);
        }
        mainw->st_fcache = mainw->pr_fcache;
        if (cfile->start != mainw->preview_frame) {
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->preview_frame);
          lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_start));
          get_play_times();
        }
        break;

      case PRV_END:
        if (mainw->en_fcache && mainw->en_fcache != mainw->pr_fcache && mainw->en_fcache != mainw->st_fcache) {
          weed_layer_unref(mainw->en_fcache);
        }
        mainw->en_fcache = mainw->pr_fcache;
        if (cfile->end != mainw->preview_frame) {
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->preview_frame);
          lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_end));
          get_play_times();
        }
        break;

      default:
        lives_widget_set_sensitive(mainw->rewind, FALSE);
        lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
        lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
        if (mainw->preview_box) {
          lives_widget_set_sensitive(mainw->p_rewindbutton, FALSE);
        }
        break;
      }
    }
  }


#define SCRAP_CHECK 30
  ticks_t lscrap_check;
  extern uint64_t free_mb; // MB free to write
  double ascrap_mb;  // MB written to audio file

  void add_to_ascrap_mb(uint64_t bytes) {ascrap_mb += bytes / 1000000.;}
  double get_ascrap_mb(void) {return ascrap_mb;}

  boolean load_from_scrap_file(weed_layer_t *layer, frames_t frame) {
    // load raw frame data from scrap file

    // this will also set cfile width and height - for letterboxing etc.

    // return FALSE if the frame does not exist/we are unable to read it

    char *oname;

    lives_clip_t *scrapfile = mainw->files[mainw->scrap_file];

    int fd;
    if (!IS_VALID_CLIP(mainw->scrap_file)) return FALSE;

    if (!scrapfile->ext_src) {
      oname = make_image_file_name(scrapfile, 1, LIVES_FILE_EXT_SCRAP);
      fd = lives_open_buffered_rdonly(oname);
      lives_free(oname);
      if (fd < 0) return FALSE;
#ifdef HAVE_POSIX_FADVISE
      posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
      scrapfile->ext_src = LIVES_INT_TO_POINTER(fd);
      scrapfile->ext_src_type = LIVES_EXT_SRC_FILE_BUFF;
    } else fd = LIVES_POINTER_TO_INT(scrapfile->ext_src);

    if (frame < 0 || !layer) return TRUE; /// just open fd

    if (!weed_plant_deserialise(fd, NULL, layer)) {
      //g_print("bad scrapfile frame\n");
      return FALSE;
    }
    return TRUE;
  }


  static boolean sf_writeable = TRUE;

  static int64_t _save_to_scrap_file(weed_layer_t *layer) {
    // returns frame number
    // dump the raw layer (frame) data to disk

    // TODO: run as bg thread

    size_t pdata_size;

    lives_clip_t *scrapfile = mainw->files[mainw->scrap_file];

    //int flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd;

    weed_layer_ref(layer);
    if (!scrapfile->ext_src) {
      char *oname = make_image_file_name(scrapfile, 1, LIVES_FILE_EXT_SCRAP), *dirname;

#ifdef O_NOATIME
      //flags |= O_NOATIME;
#endif

      dirname = get_clip_dir(mainw->scrap_file);
      lives_mkdir_with_parents(dirname, capable->umask);
      lives_free(dirname);

      fd = lives_create_buffered_nosync(oname, DEF_FILE_PERMS);
      lives_free(oname);

      if (fd < 0) {
        weed_layer_unref(layer);
        weed_layer_unref(layer);
        return scrapfile->f_size;
      }

      scrapfile->ext_src = LIVES_INT_TO_POINTER(fd);
      scrapfile->ext_src_type = LIVES_EXT_SRC_FILE_BUFF;

#ifdef HAVE_POSIX_FADVISE
      posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    } else fd = LIVES_POINTER_TO_INT(scrapfile->ext_src);

    // serialise entire frame to scrap file
    pdata_size = weed_plant_serialise(fd, layer, NULL);

    weed_layer_unref(layer);
    weed_layer_unref(layer);

    // check free space every 2048 frames or after SCRAP_CHECK seconds (whichever comes first)
    if (lscrap_check == -1) lscrap_check = mainw->clock_ticks;
    else {
      if (mainw->clock_ticks - lscrap_check >= SCRAP_CHECK * TICKS_PER_SECOND
          || (scrapfile->frames & 0x800) == 0x800) {
        char *dir = get_clip_dir(mainw->scrap_file);
        free_mb = (double)get_ds_free(dir) / 1000000.;
        if (free_mb == 0) sf_writeable = is_writeable_dir(dir);
        lives_free(dir);
        lscrap_check = mainw->clock_ticks;
      }
    }

    return pdata_size;
  }


  int save_to_scrap_file(weed_layer_t *layer) {
    static weed_layer_t *orig_layer = NULL;
    static boolean checked_disk = FALSE;

    lives_clip_t *scrapfile = mainw->files[mainw->scrap_file];
    char *framecount;

    if (!IS_VALID_CLIP(mainw->scrap_file)) return -1;
    if (!layer) {
      if (!scrapfile->frames) {
        orig_layer = NULL;
        checked_disk = FALSE;
      }
      return scrapfile->frames;
    }

    if ((scrapfile->frames & 0x3F) == 0x3F && !checked_disk) {
      /// check every 64 frames for quota overrun
      checked_disk = TRUE;
      if (!check_for_disk_space(TRUE)) return scrapfile->frames;
    }

    if (!mainw->scrap_file_proc) {
      mainw->scrap_file_proc =
        lives_proc_thread_create(LIVES_THRDATTR_IDLEFUNC | LIVES_THRDATTR_START_UNQUEUED,
                                 (lives_funcptr_t)_save_to_scrap_file,
                                 WEED_SEED_INT64, "v", &orig_layer);
    }

    checked_disk = FALSE;
    check_for_disk_space(FALSE);

    if (!lives_proc_thread_is_unqueued(mainw->scrap_file_proc)) {
      return scrapfile->frames;
    }

    orig_layer = weed_layer_copy(NULL, layer);
    scrapfile->f_size += lives_proc_thread_join_int64(mainw->scrap_file_proc);
    lives_proc_thread_queue(mainw->scrap_file_proc, 0);

    if ((!mainw->fs || (prefs->play_monitor != widget_opts.monitor + 1 && capable->nmonitors > 1))
        && !prefs->hide_framebar && !mainw->faded) {
      double scrap_mb = (double)scrapfile->f_size / (double)ONE_MILLION;
      if ((scrap_mb + ascrap_mb) < (double)free_mb * .75) {
        // TRANSLATORS: rec(ord) %.2f M(ega)B(ytes)
        framecount = lives_strdup_printf(_("rec %.2f MB"), scrap_mb + ascrap_mb);
      } else {
        // warn if scrap_file > 3/4 of free space
        // TRANSLATORS: !rec(ord) %.2f M(ega)B(ytes)
        if (sf_writeable)
          framecount = lives_strdup_printf(_("!rec %.2f MB"), scrap_mb + ascrap_mb);
        else
          // TRANSLATORS: rec(ord) ?? M(ega)B(ytes)
          framecount = (_("rec ?? MB"));
      }
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
      lives_free(framecount);
    }

    mainw->scrap_file_size = scrapfile->f_size;

    return ++scrapfile->frames;
  }


  LIVES_GLOBAL_INLINE boolean flush_scrap_file(void) {
    if (!IS_VALID_CLIP(mainw->scrap_file)) return FALSE;
    if (mainw->scrap_file_proc) {
      mainw->files[mainw->scrap_file]->f_size += lives_proc_thread_join_int64(mainw->scrap_file_proc);
      lives_proc_thread_unref(mainw->scrap_file_proc);
      mainw->scrap_file_proc = NULL;
    }
    return TRUE;
  }


#ifndef NO_PROG_LOAD

#ifdef GUI_GTK
  static void pbsize_set(GdkPixbufLoader * pbload, int xxwidth, int xxheight, livespointer ptr) {
    weed_layer_t *layer = (weed_layer_t *)ptr;
    int nsize[2];
    nsize[0] = xxwidth;
    nsize[1] = xxheight;
    weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
  }
#endif

#endif

#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
  static png_uint_32 png_flags;
#endif
  static int png_flagstate = 0;

  static void png_init(png_structp png_ptr) {
    png_uint_32 mask = 0;
#if defined(PNG_LIBPNG_VER) && (PNG_LIBPNG_VER >= 10200)
#ifdef PNG_SELECT_READ
    int selection = PNG_SELECT_READ;// | PNG_SELECT_WRITE;
    int mmxsupport = png_mmx_support(); // -1 = not compiled, 0 = not on machine, 1 = OK
    mask = png_get_asm_flagmask(selection);

    if (mmxsupport < 1) {
      int compilerID;
      mask &= ~(png_get_mmx_flagmask(selection, &compilerID));
      /* if (prefs->show_dev_opts) { */
      /*   g_printerr(" without MMX features (%d)\n", mmxsupport); */
      /* } */
    } else {
      /* if (prefs->show_dev_opts) { */
      /*   g_printerr(" with MMX features\n"); */
      /* } */
    }
#endif
#endif

#if defined(PNG_USE_PNGGCCRD) && defined(PNG_ASSEMBLER_CODE_SUPPORTED)	\
  && defined(PNG_THREAD_UNSAFE_OK)
    /* Disable thread-unsafe features of pnggccrd */
    if (png_access_version() >= 10200) {
      mask &= ~(PNG_ASM_FLAG_MMX_READ_COMBINE_ROW
                | PNG_ASM_FLAG_MMX_READ_FILTER_SUB
                | PNG_ASM_FLAG_MMX_READ_FILTER_AVG
                | PNG_ASM_FLAG_MMX_READ_FILTER_PAETH);

      if (prefs->show_dev_opts) {
        g_printerr("Thread unsafe features of libpng disabled.\n");
      }
    }
#endif

    if (prefs->show_dev_opts && mask != 0) {
      uint64_t xmask = (uint64_t)mask;
      g_printerr("enabling png opts %lu\n", xmask);
    }

    if (!mask) png_flagstate = -1;
    else {
#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
      png_flags = png_get_asm_flags(png_ptr);
      png_flags |= mask;
      png_flagstate = 1;
#endif
    }
  }

#define PNG_BIO
#ifdef PNG_BIO
  static void png_read_func(png_structp png_ptr, png_bytep data, png_size_t length) {
    int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
    ssize_t bread;
    //lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
    if ((bread = lives_read_buffered(fd, data, length, TRUE)) < length) {
      char *errmsg = lives_strdup_printf("png read_fn error, read only %ld bytes of %ld\n", bread, length);
      png_error(png_ptr, errmsg);
      lives_free(errmsg);
    }
  }
#endif

  typedef struct {
    weed_layer_t *layer;
    int width, height;
    LiVESInterpType interp;
    int pal, clamp;
  } resl_priv_data;


#if USE_RESTHREAD
  static void res_thrdfunc(void *arg) {
    resl_priv_data *priv = (resl_priv_data *)arg;
    resize_layer(priv->layer, priv->width, priv->height, priv->interp, priv->pal, priv->clamp);
    weed_set_voidptr_value(priv->layer, WEED_LEAF_RESIZE_THREAD, NULL);
    lives_free(priv);
  }


  static void reslayer_thread(weed_layer_t *layer, int twidth, int theight, LiVESInterpType interp,
                              int tpalette, int clamp, double file_gamma) {
    resl_priv_data *priv = (resl_priv_data *)lives_malloc(sizeof(resl_priv_data));
    lives_proc_thread_t resthread;
    weed_set_double_value(layer, "file_gamma", file_gamma);
    priv->layer = layer;
    priv->width = twidth;
    priv->height = theight;
    priv->interp = interp;
    priv->pal = tpalette;
    priv->clamp = clamp;
    resthread = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI, (lives_funcptr_t)res_thrdfunc, -1, "v", priv);
    weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, resthread);
  }
#endif


  boolean layer_from_png(int fd, weed_layer_t *layer, int twidth, int theight, int tpalette, boolean prog) {
    png_structp png_ptr;
    png_infop info_ptr;
    double file_gamma;
    int gval;
#ifndef PNG_BIO
    FILE *fp = fdopen(fd, "rb");
    size_t bsize = fread(ibuff, 1, 8, fp);
    boolean is_png = TRUE;
    unsigned char ibuff[8];
#endif

    unsigned char **row_ptrs;
    unsigned char *ptr;

    boolean is16bit = FALSE;

    png_uint_32 xwidth, xheight;

    int width, height;
    int color_type, bit_depth;
    int rowstride;
    int flags, privflags;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

    if (!png_ptr) {
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

#if defined(PNG_LIBPNG_VER) && (PNG_LIBPNG_VER >= 10200)
    if (!png_flagstate) png_init(png_ptr);
#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
    if (png_flagstate == 1) png_set_asm_flags(png_ptr, png_flags);
#endif
#endif

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
      // libpng will longjump to here on error
#if USE_RESTHREAD
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 0);
#endif
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

#ifdef PNG_BIO
    png_set_read_fn(png_ptr, LIVES_INT_TO_POINTER(fd), png_read_func);
#ifndef VALGRIND_ON
    if (mainw->debug)
      png_set_sig_bytes(png_ptr, 0);
    else
      png_set_sig_bytes(png_ptr, 8);
#else
    png_set_sig_bytes(png_ptr, 0);
#endif
#else
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, bsize);
#endif

    // read header info
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &xwidth, &xheight,
                 NULL, NULL, NULL, NULL, NULL);
    if (xwidth > 0 && xheight > 0) {
      weed_set_int_value(layer, WEED_LEAF_WIDTH, xwidth);
      weed_set_int_value(layer, WEED_LEAF_HEIGHT, xheight);

      if (!weed_plant_has_leaf(layer, WEED_LEAF_NATURAL_SIZE)) {
        int nsize[2];
        // set "natural_size" in case a filter needs it
        //g_print("setnatsize %p\n", layer);
        if (mainw->rte) mainw->debug = TRUE;
        nsize[0] = xwidth;
        nsize[1] = xheight;
        weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
      }

      privflags = weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);

      if (privflags == LIVES_LAYER_GET_SIZE_ONLY
          || (privflags == LIVES_LAYER_LOAD_IF_NEEDS_RESIZE
              && (int)xwidth == twidth && (int)xheight == theight)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifndef PNG_BIO
        fclose(fp);
#endif
        return TRUE;
      }
    }

    flags = weed_layer_get_flags(layer);

#if PNG_LIBPNG_VER >= 10504
    if (prefs->alpha_post) {
      if (flags & WEED_LAYER_ALPHA_PREMULT) flags ^= WEED_LAYER_ALPHA_PREMULT;
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);
    } else {
      flags |= WEED_LAYER_ALPHA_PREMULT;
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PREMULTIPLIED, PNG_DEFAULT_sRGB);
    }
#endif

    weed_set_int_value(layer, WEED_LEAF_FLAGS, flags);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    gval = png_get_gAMA(png_ptr, info_ptr, &file_gamma);

    if (!gval) {
      // b > a, brighter
      //png_set_gamma(png_ptr, 1.0, .45455); /// default, seemingly: sRGB -> linear
      png_set_gamma(png_ptr, 1.0, 1.0);
    }

    if (gval == PNG_INFO_gAMA) {
      weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_LINEAR);
    }

    // want to convert everything (greyscale, RGB, RGBA64 etc.) to RGBA32 (or RGB24)
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
      // if tpalette is YUV, then recreate the pixel_data with double the width
      // and mark as 16bpc, then >> 8 when doing the conversion
#ifdef xUSE_16BIT_PCONV
      /// needs testing
      if (weed_palette_is_yuv(tpalette)) {
        width *= 2;
        is16bit = TRUE;
      } else {
#endif
#if PNG_LIBPNG_VER >= 10504
        png_set_scale_16(png_ptr);
#else
        png_set_strip_16(png_ptr);
#endif
#ifdef xUSE_16BIT_PCONV
      }
#endif
    }

    if (tpalette != WEED_PALETTE_END) {
      if (weed_palette_has_alpha(tpalette)) {
        // if target has alpha, add a channel
        if (color_type != PNG_COLOR_TYPE_RGB_ALPHA &&
            color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
          if (tpalette == WEED_PALETTE_ARGB32)
            png_set_add_alpha(png_ptr, 255, PNG_FILLER_BEFORE);
          else
            png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);
          color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        } else {
          if (tpalette == WEED_PALETTE_ARGB32) {
            png_set_swap_alpha(png_ptr);
          }
        }
      } else {
        // else remove it
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
          png_set_strip_alpha(png_ptr);
          color_type = PNG_COLOR_TYPE_RGB;
        }
      }
      if (tpalette == WEED_PALETTE_BGR24 || tpalette == WEED_PALETTE_BGRA32) {
        png_set_bgr(png_ptr);
      }
    }

    // unnecessary for read_image or if we set npass
    //png_set_interlace_handling(png_ptr);

    // read updated info with the new palette
    png_read_update_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    weed_set_int_value(layer, WEED_LEAF_WIDTH, width);
    weed_set_int_value(layer, WEED_LEAF_HEIGHT, height);

    if (weed_palette_is_rgb(tpalette)) {
      weed_layer_set_palette(layer, tpalette);
    } else {
      if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
      } else {
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }
    }

    if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
      create_blank_layer(layer, LIVES_FILE_EXT_PNG, width, height, weed_layer_get_palette(layer));
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

    // TODO: rowstride must be at least png_get_rowbytes(png_ptr, info_ptr)

    rowstride = weed_layer_get_rowstride(layer);
    ptr = weed_layer_get_pixel_data(layer);

    // libpng needs pointers to each row
    row_ptrs = (unsigned char **)lives_calloc(height,  sizeof(unsigned char *));
    for (int j = 0; j < height; j++) {
      row_ptrs[j] = &ptr[rowstride * j];
    }

#if USE_RESTHREAD
    if (weed_threadsafe && twidth * theight != 0 && (twidth != width || theight != height) &&
        !png_get_interlace_type(png_ptr, info_ptr)) {
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 1);
      reslayer_thread(layer, twidth, theight, get_interp_value(prefs->pb_quality, TRUE),
                      tpalette, weed_layer_get_yuv_clamping(layer),
                      gval == PNG_INFO_gAMA ? file_gamma : 1.);
      for (int j = 0; j < height; j++) {
        png_read_row(png_ptr, row_ptrs[j], NULL);
        weed_set_int_value(layer, WEED_LEAF_PROGSCAN, j + 1);
      }
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, -1);
    } else
#endif
      png_read_image(png_ptr, row_ptrs);

    //png_read_end(png_ptr, NULL);

    // end read

    lives_free(row_ptrs);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

    if (gval == PNG_INFO_gAMA) {
      /// if gAMA is set, then we need to convert to *linear* using the file gamma
      weed_layer_set_gamma(layer, WEED_GAMMA_LINEAR);
    } else weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

    if (is16bit) {
#if USE_RESTHREAD
      lives_proc_thread_t resthread;
#endif
      int clamping, sampling, subspace;
      weed_layer_get_palette_yuv(layer, &clamping, &sampling, &subspace);
      weed_set_int_value(layer, LIVES_LEAF_PIXEL_BITS, 16);
      if (weed_palette_has_alpha(tpalette)) tpalette = WEED_PALETTE_YUVA4444P;
      else {
        if (tpalette != WEED_PALETTE_YUV420P) tpalette = WEED_PALETTE_YUV444P;
      }
#if USE_RESTHREAD
      if ((resthread = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
        lives_proc_thread_join(resthread);
        weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL);
      }
#endif
      // convert RGBA -> YUVA4444P or RGB -> 444P or 420
      // 16 bit conversion
      convert_layer_palette_full(layer, tpalette, clamping, sampling, subspace, WEED_GAMMA_UNKNOWN);
    }
#ifndef PNG_BIO
    fclose(fp);
#endif
    return TRUE;
  }



#if 0
  static void png_write_func(png_structp png_ptr, png_bytep data, png_size_t length) {
    int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
    if (lives_write_buffered(fd, (const char *)data, length, TRUE) < length) {
      png_error(png_ptr, "write_fn error");
    }
  }

  static void png_flush_func(png_structp png_ptr) {
    int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
    if (lives_buffered_flush(fd) < 0) {
      png_error(png_ptr, "flush_fn error");
    }
  }
#endif

  static boolean layer_to_png_inner(FILE * fp, weed_layer_t *layer, int comp) {
    // comp is 0 (none) - 9 (full)
    png_structp png_ptr;
    png_infop info_ptr;

    unsigned char **row_ptrs;
    unsigned char *ptr;

    int width, height, palette;
#if PNG_LIBPNG_VER >= 10504
    int flags = 0;
#endif
    int rowstride;

    int compr = (int)((100. - (double)comp + 5.) / 10.);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

    if (!png_ptr) {
      return FALSE;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      return FALSE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
      // libpng will longjump to here on error
      if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
      THREADVAR(write_failed) = fileno(fp) + 1;
      return FALSE;
    }

    //png_set_write_fn(png_ptr, LIVES_INT_TO_POINTER(fd), png_write_func, png_flush_func);

    //FILE *fp = fdopen(fd, "wb");

    png_init_io(png_ptr, fp);

    width = weed_layer_get_width(layer);
    height = weed_layer_get_height(layer);
    rowstride = weed_layer_get_rowstride(layer);
    palette = weed_layer_get_palette(layer);

    if (width <= 0 || height <= 0 || rowstride <= 0) {
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
      LIVES_WARN("Cannot make png with 0 width or height");
      return FALSE;
    }

    if (palette == WEED_PALETTE_ARGB32) png_set_swap_alpha(png_ptr);

    switch (palette) {
    case WEED_PALETTE_BGR24:
      png_set_bgr(png_ptr);
    case WEED_PALETTE_RGB24:
      png_set_IHDR(png_ptr, info_ptr, width, height,
                   8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      break;
    case WEED_PALETTE_BGRA32:
      png_set_bgr(png_ptr);
    case WEED_PALETTE_RGBA32:
      png_set_IHDR(png_ptr, info_ptr, width, height,
                   8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      break;
    default:
      LIVES_ERROR("Bad png palette !\n");
      break;
    }

    png_set_compression_level(png_ptr, compr);

#if PNG_LIBPNG_VER >= 10504
    flags = weed_layer_get_flags(layer);
    if (flags & WEED_LAYER_ALPHA_PREMULT) {
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PREMULTIPLIED, PNG_DEFAULT_sRGB);
    } else {
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);
    }
#endif

    if (weed_layer_get_gamma(layer) == WEED_GAMMA_LINEAR)
      png_set_gAMA(png_ptr, info_ptr, 1.0);

    png_write_info(png_ptr, info_ptr);

    ptr = (unsigned char *)weed_layer_get_pixel_data(layer);

    // Write image data

    /* for (i = 0 ; i < height ; i++) { */
    /*   png_write_row(png_ptr, &ptr[rowstride * i]); */
    /* } */

    row_ptrs = (unsigned char **)lives_calloc(height, sizeof(unsigned char *));
    for (int j = 0; j < height; j++) {
      row_ptrs[j] = &ptr[rowstride * j];
    }
    png_write_image(png_ptr, row_ptrs);
    lives_free(row_ptrs);

    // end write
    png_write_end(png_ptr, (png_infop)NULL);

    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);

    return TRUE;
  }

  boolean layer_to_png(weed_layer_t *layer, const char *fname, int comp) {
    //int fd = lives_create_buffered(fname, DEF_FILE_PERMS);
    FILE *fp = fopen(fname, "wb");
    boolean ret = layer_to_png_inner(fp, layer, comp);
    fclose(fp);
    return ret;
  }


  boolean layer_to_png_threaded(savethread_priv_t *saveargs) {
    return layer_to_png(saveargs->layer, saveargs->fname, saveargs->compression);
  }


  boolean weed_layer_create_from_file_progressive(weed_layer_t *layer, const char *fname, int width,
      int height, int tpalette, const char *img_ext) {
    LiVESPixbuf *pixbuf = NULL;
    LiVESError *gerror = NULL;
    char *shmpath = NULL;
    boolean ret = TRUE;
#ifndef VALGRIND_ON
    boolean is_png = FALSE;
#endif
    int fd = -1;

#ifndef NO_PROG_LOAD
#ifdef GUI_GTK
    GdkPixbufLoader *pbload;
#endif
    uint8_t ibuff[IMG_BUFF_SIZE];
    size_t bsize;

#ifndef VALGRIND_ON
    if (!mainw->debug)
      if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) is_png = TRUE;
#endif

#ifdef PNG_BIO
    fd = lives_open_buffered_rdonly(fname);
    if (fd < 0) break_me(fname);
    if (fd < 0) return FALSE;
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#ifndef VALGRIND_ON
    if (is_png) lives_buffered_rdonly_slurp(fd, 8);
    else lives_buffered_rdonly_slurp(fd, 0);
#endif
#else
    fd = lives_open2(fname, O_RDONLY);
#endif
    if (fd < 0) return FALSE;
#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif

    xxwidth = width;
    xxheight = height;

    if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) {
      tpalette = weed_layer_get_palette(layer);
      ret = layer_from_png(fd, layer, width, height, tpalette, TRUE);
      goto fndone;
    }
#ifdef GUI_GTK
    else if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) pbload = gdk_pixbuf_loader_new_with_type(LIVES_IMAGE_TYPE_JPEG, &gerror);
    else pbload = gdk_pixbuf_loader_new();

    lives_signal_connect(LIVES_WIDGET_OBJECT(pbload), LIVES_WIDGET_SIZE_PREPARED_SIGNAL,
                         LIVES_GUI_CALLBACK(pbsize_set), layer);

    while (1) {
#ifndef PNG_BIO
      if ((bsize = read(fd, ibuff, IMG_BUFF_SIZE)) <= 0) break;
#else
      if ((bsize = lives_read_buffered(fd, ibuff, IMG_BUFF_SIZE, TRUE)) <= 0) break;
#endif
      if (!gdk_pixbuf_loader_write(pbload, ibuff, bsize, &gerror)) {
        ret = FALSE;
        goto fndone;
      }
    }

    if (!gdk_pixbuf_loader_close(pbload, &gerror)) {
      ret = FALSE;
      goto fndone;
    }
    pixbuf = gdk_pixbuf_loader_get_pixbuf(pbload);
    lives_widget_object_ref(pixbuf);
    if (pbload) lives_widget_object_unref(pbload);

#endif

# else //PROG_LOAD
    {
#ifdef PNG_BIO
      fd = lives_open_buffered_rdonly(fname);
#else
      fd = lives_open2(fname, O_RDONLY);

      if (fd < 0) return FALSE;

#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
      posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif
      tpalette = weed_layer_get_palette(layer);
      ret = layer_from_png(fd, layer, width, height, tpalette, FALSE);
      goto fndone;
    }
#endif

      pixbuf = lives_pixbuf_new_from_file_at_scale(fname, width > 0 ? width : -1, height > 0 ? height : -1, FALSE, gerror);
#endif

    if (gerror) {
      LIVES_ERROR(gerror->message);
      lives_error_free(gerror);
      pixbuf = NULL;
    }

    if (!pixbuf) {
      ret = FALSE;
      goto fndone;
    }

    if (lives_pixbuf_get_has_alpha(pixbuf)) {
      /* unfortunately gdk pixbuf loader does not preserve the original alpha channel, instead it adds its own.
         We need to hence reset it back to opaque */
      lives_pixbuf_set_opaque(pixbuf);
      weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
    } else weed_layer_set_palette(layer, WEED_PALETTE_RGB24);

    if (!pixbuf_to_layer(layer, pixbuf)) {
      lives_widget_object_unref(pixbuf);
    }

fndone:
#ifdef PNG_BIO
    if (ret && fd >= 0) {
      lives_close_buffered(fd);
    }
#else
    if (fd >= 0) close(fd);
#endif
    if (shmpath) {
      lives_rm(fname);
      lives_rmdir_with_parents(shmpath);
    }
    return ret;
  }



  static weed_plant_t *render_subs_from_file(lives_clip_t *sfile, double xtime, weed_layer_t *layer) {
    // render subtitles from whatever (.srt or .sub) file
    // uses default values for colours, fonts, size, etc.

    // TODO - allow prefs settings for colours, fonts, size, alpha (use plugin for this)

    //char *sfont=mainw->font_list[prefs->sub_font];
    const char *sfont = "Sans";
    lives_colRGBA64_t col_white, col_black_a;

    int error, size;

    xtime -= (double)sfile->subt->offset / sfile->fps;

    // round to 2 dp
    xtime = (double)((int)(xtime * 100. + .5)) / 100.;

    if (xtime < 0.) return layer;

    get_subt_text(sfile, xtime);

    if (!sfile->subt->text) return layer;

    size = weed_get_int_value(layer, WEED_LEAF_WIDTH, &error) / 32;

    col_white = lives_rgba_col_new(65535, 65535, 65535, 65535);
    col_black_a = lives_rgba_col_new(0, 0, 0, SUB_OPACITY);

    if (prefs->apply_gamma && prefs->pb_quality == PB_QUALITY_HIGH) {
      // make it look nicer by dimming relative to luma
      gamma_convert_layer(WEED_GAMMA_LINEAR, layer);
    }

    layer = render_text_to_layer(layer, sfile->subt->text, sfont, size,
                                 LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND, &col_white, &col_black_a, TRUE, TRUE, 0.);
    return layer;
  }


  boolean pull_frame_at_size(weed_layer_t *layer, const char *image_ext, weed_timecode_t tc, int width, int height,
                             int target_palette) {
    // pull a frame from an external source into a layer
    // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
    // tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
    // image_ext is used if the source is an image file (eg. "jpg" or "png")
    // width and height are hints only, the caller should resize if necessary
    // target_palette is also a hint

    // if we pull from a decoder plugin, then we may also deinterlace
    weed_layer_t *vlayer, *xlayer = NULL;
    lives_clip_t *sfile = NULL;
    int clip;
    frames_t frame;
    int clip_type;

    boolean is_thread = FALSE;
    static boolean norecurse = FALSE;

    //weed_layer_pixel_data_free(layer);
    weed_layer_ref(layer);
    weed_layer_set_size(layer, width, height);

    clip = lives_layer_get_clip(layer);
    frame = lives_layer_get_frame(layer);

    // the default unless overridden
    weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

    if (weed_plant_has_leaf(layer, LIVES_LEAF_PROC_THREAD)) is_thread = TRUE;

    weed_leaf_delete(layer, WEED_LEAF_NATURAL_SIZE);

    sfile = RETURN_VALID_CLIP(clip);

    if (!sfile) {
      if (target_palette != WEED_PALETTE_END) weed_layer_set_palette(layer, target_palette);
      goto fail;
    }

    clip_type = sfile->clip_type;

    switch (clip_type) {
    case CLIP_TYPE_NULL_VIDEO:
      create_blank_layer(layer, image_ext, width, height, target_palette);
      goto success;
    case CLIP_TYPE_DISK:
    case CLIP_TYPE_FILE:
      // frame number can be 0 during rendering
      if (frame == 0) {
        create_blank_layer(layer, image_ext, width, height, target_palette);
        goto success;
      } else if (clip == mainw->scrap_file) {
        boolean res = load_from_scrap_file(layer, frame);
        if (!res) goto fail;
        weed_leaf_delete(layer, LIVES_LEAF_PIXBUF_SRC);
        weed_leaf_delete(layer, LIVES_LEAF_SURFACE_SRC);
        // clip width and height may vary dynamically
        if (LIVES_IS_PLAYING) {
          sfile->hsize = weed_layer_get_width(layer);
          sfile->vsize = weed_layer_get_height(layer);
        }
        // realign
        copy_pixel_data(layer, NULL, THREADVAR(rowstride_alignment));
        goto success;
      } else {
        boolean need_unlock = TRUE;
        pthread_mutex_lock(&sfile->frame_index_mutex);
        if (clip_type == CLIP_TYPE_FILE && sfile->frame_index && frame > 0 &&
            frame <= sfile->frames && is_virtual_frame(clip, frame)) {
          if (prefs->vj_mode && mainw->loop_locked && mainw->ping_pong && sfile->pb_fps >= 0. && !norecurse) {
            lives_proc_thread_t tinfo = THREADVAR(proc_thread);
            LiVESPixbuf *pixbuf = NULL;
            THREADVAR(proc_thread) = NULL;
            norecurse = TRUE;
            virtual_to_images(clip, frame, frame, FALSE, &pixbuf);
            pthread_mutex_unlock(&sfile->frame_index_mutex);
            need_unlock = FALSE;
            norecurse = FALSE;
            THREADVAR(proc_thread) = tinfo;
            if (pixbuf) {
              if (!pixbuf_to_layer(layer, pixbuf)) {
                lives_widget_object_unref(pixbuf);
              }
              break;
            }
          }
          if (1) {
            // pull frame from video clip
            ///
#ifdef USE_REC_RS
            int nplanes;
#endif
            void **pixel_data;
            lives_decoder_t *dplug = NULL;
            int *rowstrides;
            double timex = 0, xtimex, est = -1.;
            frames_t iframe;
            boolean res = TRUE;
            /// HOST_DECODER is set in mulitrack, there is 1 decoder per track since multiple tracks can have the same clip
            dplug = (lives_decoder_t *)weed_get_voidptr_value(layer, WEED_LEAF_HOST_DECODER, NULL);
            if (!dplug)  dplug = (lives_decoder_t *)sfile->ext_src;
            iframe = get_indexed_frame(clip, frame);
            if (!dplug || !dplug->cdata || iframe >= dplug->cdata->nframes) {
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
              goto fail;
            }
            if (target_palette != dplug->cdata->current_palette) {
              if (dplug->dpsys->set_palette) {
                int opal = dplug->cdata->current_palette;
                int pal = best_palette_match(dplug->cdata->palettes, -1, target_palette);
                if (pal != opal) {
                  dplug->cdata->current_palette = pal;
                  if (!(*dplug->dpsys->set_palette)(dplug->cdata)) {
                    dplug->cdata->current_palette = opal;
                    (*dplug->dpsys->set_palette)(dplug->cdata);
                  } else if (dplug->cdata->rec_rowstrides) {
                    lives_free(dplug->cdata->rec_rowstrides);
                    dplug->cdata->rec_rowstrides = NULL;
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*

            width = dplug->cdata->width / weed_palette_get_pixels_per_macropixel(dplug->cdata->current_palette);
            height = dplug->cdata->height;

            weed_layer_set_size(layer, width, height);

            if (weed_palette_is_yuv(dplug->cdata->current_palette))
              weed_layer_set_palette_yuv(layer, dplug->cdata->current_palette,
                                         dplug->cdata->YUV_clamping,
                                         dplug->cdata->YUV_sampling,
                                         dplug->cdata->YUV_subspace);
            else weed_layer_set_palette(layer, dplug->cdata->current_palette);

#ifdef USE_REC_RS
            nplanes = weed_palette_get_nplanes(dplug->cdata->current_palette);
            if (!dplug->cdata->rec_rowstrides) {
              dplug->cdata->rec_rowstrides = lives_calloc(nplanes, sizint);
            } else {
              if (dplug->cdata->rec_rowstrides[0]) {
                weed_layer_set_rowstrides(layer, dplug->cdata->rec_rowstrides, nplanes);
                weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
                lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
              }
            }
#endif
            // TODO - free this after playback
            if (create_empty_pixel_data(layer, TRUE, TRUE)) {
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
              need_unlock = FALSE;
#ifdef USE_REC_RS
              weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
              pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);
            } else {
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
#ifdef USE_REC_RS
              weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
              goto fail;
            }

            if (!pixel_data || !pixel_data[0]) {
              char *msg = lives_strdup_printf("NULL pixel data for layer size %d X %d, palette %s\n", width, height,
                                              weed_palette_get_name_full(dplug->cdata->current_palette,
                                                  dplug->cdata->YUV_clamping, dplug->cdata->YUV_subspace));
              LIVES_WARN(msg);
              lives_free(msg);
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
              goto fail;
            }

            rowstrides = weed_layer_get_rowstrides(layer, NULL);
            pthread_mutex_lock(&dplug->mutex);

            if (dplug->dpsys->estimate_delay) {
              timex = lives_get_current_ticks() / TICKS_PER_SECOND_DBL;
              est = (*dplug->dpsys->estimate_delay)(dplug->cdata, iframe);
            } else if (prefs->dev_show_timing) timex = lives_get_current_ticks() / TICKS_PER_SECOND_DBL;
            if (prefs->dev_show_timing) g_printerr("get_frame pre %d / %d with %p  @ %f\n", clip, iframe, dplug, timex);

            if (!(*dplug->dpsys->get_frame)(dplug->cdata, (int64_t)iframe, rowstrides, sfile->vsize, pixel_data)) {
              pthread_mutex_unlock(&dplug->mutex);
              if (prefs->show_dev_opts) {
                g_print("Error loading frame %d (index value %d)\n", frame,
                        sfile->frame_index[frame - 1]);
                break_me("load error");
              }
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
              need_unlock = FALSE;
#ifdef USE_REC_RS
              if (dplug->cdata->rec_rowstrides)
                lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
#endif
              // if get_frame fails, return a black frame
              goto fail;
            } else {
              pthread_mutex_unlock(&dplug->mutex);
              if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
              need_unlock = FALSE;
              propogate_timing_data(dplug);

              if (est > 0.) {
                xtimex = lives_get_current_ticks() / TICKS_PER_SECOND_DBL;
                timex = xtimex - timex;
                //g_print("\n\nERROR DELTAS: %f and %f\n\n", timex - est, timex / est);
              } else if (prefs->dev_show_timing) xtimex = lives_get_current_ticks() / TICKS_PER_SECOND_DBL;
              if (prefs->dev_show_timing) g_printerr("get_frame post %d / %d with %p  @ %f\n", clip, iframe, dplug,
                                                       xtimex);
            }

            //g_print("ACT %f EST %f\n",  (double)(lives_get_current_ticks() - timex) / TICKS_PER_SECOND_DBL, est_time);

            lives_free(pixel_data);
            lives_free(rowstrides);
            if (res) {
              if (prefs->apply_gamma && prefs->pb_quality != PB_QUALITY_LOW) {
                if (dplug->cdata->frame_gamma != WEED_GAMMA_UNKNOWN) {
                  weed_layer_set_gamma(layer, dplug->cdata->frame_gamma);
                } else if (dplug->cdata->YUV_subspace == WEED_YUV_SUBSPACE_BT709) {
                  weed_layer_set_gamma(layer, WEED_GAMMA_BT709);
                }
              }

              // get_frame may now update YUV_clamping, YUV_sampling, YUV_subspace
              if (weed_palette_is_yuv(dplug->cdata->current_palette)) {
                weed_layer_set_palette_yuv(layer, dplug->cdata->current_palette,
                                           dplug->cdata->YUV_clamping,
                                           dplug->cdata->YUV_sampling,
                                           dplug->cdata->YUV_subspace);
                if (prefs->apply_gamma && prefs->pb_quality != PB_QUALITY_LOW) {
                  if (weed_get_int_value(layer, WEED_LEAF_GAMMA_TYPE, NULL) == WEED_GAMMA_BT709) {
                    weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, WEED_YUV_SUBSPACE_BT709);
                  }
                  if (weed_get_int_value(layer, WEED_LEAF_YUV_SUBSPACE, NULL) == WEED_YUV_SUBSPACE_BT709) {
                    weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_BT709);
                  }
                }
              }
              // deinterlace
              if (sfile->deinterlace || (prefs->auto_deint && dplug->cdata->interlace != LIVES_INTERLACE_NONE)) {
                if (!is_thread) {
                  deinterlace_frame(layer, tc);
                } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
              }
            }
            if (!res) goto fail;
            goto success;
          }
        } else {
          // pull frame from decoded images
          int64_t timex = lives_get_current_ticks();
          double img_decode_time;
          boolean ret;
          char *fname;
#if USE_RESTHREAD
          lives_proc_thread_t resthread;
#endif
          pthread_mutex_unlock(&sfile->frame_index_mutex);
          if (!image_ext || !*image_ext) image_ext = get_image_ext_for_type(sfile->img_type);
          fname = make_image_file_name(sfile, frame, image_ext);
          ret = weed_layer_create_from_file_progressive(layer, fname, width, height, target_palette, image_ext);

#if USE_RESTHREAD
          if ((resthread = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
            lives_proc_thread_join(resthread);
            weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL);
          }
#endif

          lives_free(fname);
          if (!ret) goto fail;

          img_decode_time = (double)(lives_get_current_ticks() - timex) / TICKS_PER_SECOND_DBL;
          if (!sfile->img_decode_time) sfile->img_decode_time = img_decode_time;
          else sfile->img_decode_time = (sfile->img_decode_time + img_decode_time) / 2.;
        }
      }
      break;

      // handle other types of sources

#ifdef HAVE_YUV4MPEG
    case CLIP_TYPE_YUV4MPEG:
      weed_layer_set_from_yuv4m(layer, sfile);
      if (sfile->deinterlace) {
        if (!is_thread) {
          deinterlace_frame(layer, tc);
        } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
      }
      goto success;
#endif
#ifdef HAVE_UNICAP
    case CLIP_TYPE_VIDEODEV:
      weed_layer_set_from_lvdev(layer, sfile, 4. / cfile->pb_fps);
      if (sfile->deinterlace) {
        if (!is_thread) {
          deinterlace_frame(layer, tc);
        } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
      }
      goto success;
#endif
    case CLIP_TYPE_LIVES2LIVES:
      weed_layer_set_from_lives2lives(layer, clip, (lives_vstream_t *)sfile->ext_src);
      goto success;
    case CLIP_TYPE_GENERATOR: {
      // special handling for clips where host controls size
      // Note: vlayer is actually the out channel of the generator, so we should
      // never free it !
      weed_plant_t *inst;
      weed_instance_ref((inst = (weed_plant_t *)sfile->ext_src));
      if (inst) {
        int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
        filter_mutex_lock(key);
        if (!IS_VALID_CLIP(clip)) {
          // gen was switched off
          create_blank_layer(layer, image_ext, width, height, target_palette);
        } else {
          vlayer = weed_layer_create_from_generator(inst, tc, clip);
          weed_layer_copy(layer, vlayer); // layer is non-NULL, so copy by reference
          weed_layer_nullify_pixel_data(vlayer);
        }
        filter_mutex_unlock(key);
        weed_instance_unref(inst);
      } else {
        create_blank_layer(layer, image_ext, width, height, target_palette);
      }
    }
    goto success;
    default:
      goto fail;
    }

    if (!is_thread) {
      // render subtitles from file
      if (prefs->show_subtitles && sfile->subt && sfile->subt->tfile > 0) {
        double xtime = (double)(frame - 1) / sfile->fps;
        xlayer = render_subs_from_file(sfile, xtime, layer);
        if (xlayer == layer) xlayer = NULL;
      }
    }

success:
    weed_layer_unref(layer);
    if (xlayer) layer = xlayer;
    return TRUE;

fail:
    weed_layer_pixel_data_free(layer);
    create_blank_layer(layer, image_ext, width, height, target_palette);
    weed_layer_unref(layer);
    return FALSE;
  }



  /**
     @brief pull a frame from an external source into a layer
     the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
     tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
     image_ext is used if the source is an image file (eg. "jpg" or "png")
  */
  LIVES_GLOBAL_INLINE boolean pull_frame(weed_layer_t *layer, const char *image_ext, weed_timecode_t tc) {
    return pull_frame_at_size(layer, image_ext, tc, 0, 0, WEED_PALETTE_END);
  }


  LIVES_GLOBAL_INLINE boolean is_layer_ready(weed_layer_t *layer) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)weed_get_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL);
    if (lpt) return lives_proc_thread_is_done(lpt);
    return TRUE;
  }


  /**
     @brief block until layer pixel_data is ready.
     This function should always be called for threaded layers, prior to freeing the layer, reading it's  properties, pixel data,
     resizing etc.

     We may also deinterlace and overlay subs here
     for the blend layer, we may also resize, convert palette, apply gamma in preparation for combining with the main layer

     if effects were applied then the frame_layer can depend on other layers, however
     these will have been checked already when the effects were applied

     see also MACRO: is_layer_ready(layer) which can be called first to avoid the block, e.g.

     while (!is_layer_ready(layer)) {
     do_something_else();
     }
     check_layer_ready(layer); // won't block

     This function must be called at some point for every threaded frame, otherwise thread resources will be leaked.

     N.B. the name if this function is not the best, it will probably get renamed in th future to something like
     finish_layer.
  */
  boolean check_layer_ready(weed_layer_t *layer) {
    int clip;
    boolean ready = TRUE;
    lives_clip_t *sfile;
#if USE_RESTHREAD
    lives_proc_thread_t resthread;
    boolean cancelled = FALSE;
#endif
    if (!layer) return FALSE;
    else {
      lives_proc_thread_t lpt = (lives_proc_thread_t)weed_get_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL);
      if (lpt) {
#if USE_RESTHREAD
        if (lives_proc_thread_get_cancel_requested(lpt)) {
          cancelled = TRUE;
          resthread = weed_get_voidptr_value(layer, LIVES_LEAF_RESIZE_THREAD, NULL);
          if (resthread && !lives_proc_thread_get_cancel_requested(resthread))
            lives_proc_thread_request_cancel(resthread);
        }
#endif

        lives_proc_thread_join_boolean(lpt);
        weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL);
      }
    }

#if USE_RESTHREAD
    if ((resthread = weed_get_voidptr_value(layer, LIVES_LEAF_RESIZE_THREAD, NULL))) {
      if (resthread && cancelled && !lives_proc_thread_get_cancel_requested(resthread))
        lives_proc_thread_request_cancel(resthread);
      ready = FALSE;
      weed_set_voidptr_value(layer, LIVES_LEAF_RESIZE_THREAD, NULL);
      lives_proc_thread_join(resthread);
    }
#endif

    if (ready) return TRUE;

    if (weed_get_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, NULL) == WEED_TRUE) {
      weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_HOST_TC, NULL);
      deinterlace_frame(layer, tc);
      weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_FALSE);
    }

    if (weed_plant_has_leaf(layer, WEED_LEAF_CLIP)) {
      // TODO: we should render subs before display, to avoid the text size changing
      clip = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);
      if (IS_VALID_CLIP(clip)) {
        sfile = mainw->files[clip];
        // render subtitles from file
        if (sfile->subt && sfile->subt->tfile >= 0 && prefs->show_subtitles) {
          frames_t frame = weed_get_int_value(layer, WEED_LEAF_FRAME, NULL);
          double xtime = (double)(frame - 1) / sfile->fps;
          layer = render_subs_from_file(sfile, xtime, layer);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

    return TRUE;
  }


  static boolean pft_thread(weed_layer_t *layer, const char *img_ext) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)weed_get_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL);
    weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_TIMECODE, NULL);
    int width = weed_layer_get_width(layer);
    int height = weed_layer_get_height(layer);

    lives_proc_thread_set_cancellable(lpt);

    /// if loading the blend frame in clip editor, then we recall the palette details and size @ injection,
    //and prepare it in this thread
    if (mainw->blend_file != -1 && mainw->blend_palette != WEED_PALETTE_END
        && LIVES_IS_PLAYING && !mainw->multitrack && mainw->blend_file != mainw->current_file
        && weed_get_int_value(layer, WEED_LEAF_CLIP, NULL) == mainw->blend_file) {
      int tgamma = WEED_GAMMA_UNKNOWN;
      short interp = get_interp_value(prefs->pb_quality, TRUE);
      pull_frame_at_size(layer, img_ext, tc, mainw->blend_width, mainw->blend_height, mainw->blend_palette);
      if (lives_proc_thread_get_cancel_requested(lpt)) return FALSE;
      if (is_layer_ready(layer)) {
        resize_layer(layer, mainw->blend_width,
                     mainw->blend_height, interp, mainw->blend_palette, mainw->blend_clamping);
      }
      if (lives_proc_thread_get_cancel_requested(lpt)) return FALSE;
      if (mainw->blend_palette != WEED_PALETTE_END) {
        if (weed_palette_is_rgb(mainw->blend_palette))
          tgamma = mainw->blend_gamma;
      }
      if (mainw->blend_palette != WEED_PALETTE_END) {
        if (is_layer_ready(layer) && weed_layer_get_width(layer) == mainw->blend_width
            && weed_layer_get_height(layer) == mainw->blend_height) {
          convert_layer_palette_full(layer, mainw->blend_palette, mainw->blend_clamping, mainw->blend_sampling,
                                     mainw->blend_subspace, tgamma);
        }
      }
      if (lives_proc_thread_get_cancel_requested(lpt)) return FALSE;
      if (tgamma != WEED_GAMMA_UNKNOWN && is_layer_ready(layer)
          && weed_layer_get_width(layer) == mainw->blend_width
          && weed_layer_get_height(layer) == mainw->blend_height
          && weed_layer_get_palette(layer) == mainw->blend_palette)
        gamma_convert_layer(mainw->blend_gamma, layer);
    } else {
      pull_frame_at_size(layer, img_ext, tc, width, height, WEED_PALETTE_END);
    }
    return TRUE;
  }


  void pull_frame_threaded(weed_layer_t *layer, const char *img_ext, weed_timecode_t tc, int width, int height) {
    // pull a frame from an external source into a layer
    // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer

    // done in a threaded fashion
    // call check_layer_ready() to block until the frame thread is completed
#ifdef NO_FRAME_THREAD
    pull_frame(layer, img_ext, tc);
    return;
#else
    lives_proc_thread_t lpt;
    weed_layer_pixel_data_free(layer);
    weed_set_int64_value(layer, WEED_LEAF_HOST_TC, tc);
    weed_layer_set_size(layer, width, height);
    //
    lpt = lives_proc_thread_create(LIVES_THRDATTR_PRIORITY | LIVES_THRDATTR_NO_GUI
                                   | LIVES_THRDATTR_START_UNQUEUED, (lives_funcptr_t)pft_thread,
                                   WEED_SEED_BOOLEAN, "vs", layer, img_ext);
    weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, lpt);
    lives_proc_thread_queue(lpt, 0);
#endif
  }


  LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, weed_timecode_t tc,
                                         int width, int height, LiVESInterpType interp, boolean fordisp) {
    // return a correctly sized (Gdk)Pixbuf (RGB24 for jpeg, RGB24 / RGBA32 for png) for the given clip and frame
    // tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
    // image_ext is used if the source is an image file (eg. "jpg" or "png")
    // pixbuf will be sized to width x height pixels using interp

    LiVESPixbuf *pixbuf = NULL;
    weed_layer_t *layer = lives_layer_new_for_frame(clip, frame);
    int palette;

#ifndef ALLOW_PNG24
    if (!strcmp(image_ext, LIVES_FILE_EXT_PNG)) palette = WEED_PALETTE_RGBA32;
    else palette = WEED_PALETTE_RGB24;
#else
    if (strcmp(image_ext, LIVES_FILE_EXT_PNG)) palette = WEED_PALETTE_RGB24;
    else palette = WEED_PALETTE_END;
#endif

    if (pull_frame_at_size(layer, image_ext, tc, width, height, palette)) {
      check_layer_ready(layer);
      pixbuf = layer_to_pixbuf(layer, TRUE, fordisp);
    }
    weed_layer_unref(layer);
    if (pixbuf && ((width != 0 && lives_pixbuf_get_width(pixbuf) != width)
                   || (height != 0 && lives_pixbuf_get_height(pixbuf) != height))) {
      LiVESPixbuf *pixbuf2;
      // TODO - could use resize plugin here
      pixbuf2 = lives_pixbuf_scale_simple(pixbuf, width, height, interp);
      if (pixbuf) lives_widget_object_unref(pixbuf);
      pixbuf = pixbuf2;
    }

    return pixbuf;
  }


  LIVES_GLOBAL_INLINE LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, weed_timecode_t tc) {
    return pull_lives_pixbuf_at_size(clip, frame, image_ext, tc, 0, 0, LIVES_INTERP_NORMAL, FALSE);
  }


  /**
     @brief Save a pixbuf to a file using the specified imgtype and the specified quality/compression value
  */
  boolean pixbuf_to_png(LiVESPixbuf * pixbuf, char *fname, lives_img_type_t imgtype, int quality, int width, int height,
                        LiVESError **gerrorptr) {
    ticks_t timeout;
    lives_alarm_t alarm_handle;
    boolean retval = TRUE;
    int fd;

    // CALLER should check for errors
    // fname should be in local charset

    if (!LIVES_IS_PIXBUF(pixbuf)) {
      /// invalid pixbuf, we will save a blank image
      const char *img_ext = get_image_ext_for_type(imgtype);
      weed_layer_t  *layer = create_blank_layer(NULL, img_ext, width, height, WEED_PALETTE_END);
      pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);
      weed_layer_unref(layer);
      retval = FALSE;
    }

    fd = lives_open3(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
    while (flock(fd, LOCK_EX) && (timeout = lives_alarm_check(alarm_handle)) > 0) {
      lives_nanosleep(LIVES_QUICK_NAP);
    }
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) return FALSE;

    if (imgtype == IMG_TYPE_JPEG) {
      char *qstr = lives_strdup_printf("%d", quality);
#ifdef GUI_GTK
      gdk_pixbuf_save(pixbuf, fname, LIVES_IMAGE_TYPE_JPEG, gerrorptr, "quality", qstr, NULL);
#endif
#ifdef GUI_QT
      qt_jpeg_save(pixbuf, fname, gerrorptr, quality);
#endif
      lives_free(qstr);
    } else if (imgtype == IMG_TYPE_PNG) {
      char *cstr = lives_strdup_printf("%d", (int)((100. - (double)quality + 5.) / 10.));
      if (LIVES_IS_PIXBUF(pixbuf)) {
#ifdef GUI_GTK
        gdk_pixbuf_save(pixbuf, fname, LIVES_IMAGE_TYPE_PNG, gerrorptr, "compression", cstr, NULL);
#endif
      } else retval = FALSE;
      lives_free(cstr);
    } else {
      //gdk_pixbuf_save_to_callback(...);
    }

    close(fd);
    if (*gerrorptr) return FALSE;
    return retval;
  }


  boolean pixbuf_to_png_threaded(savethread_priv_t *saveargs) {
    return pixbuf_to_png(saveargs->pixbuf, saveargs->fname, saveargs->img_type, saveargs->compression,
                         saveargs->width, saveargs->height, saveargs->error);
  }


  static int find_next_clip(int index, int old_file) {
    // when a clip becomes invalid, find another clip to switch to
    if (IS_VALID_CLIP(index)) {
      if (mainw->multitrack) {
        if (old_file != mainw->multitrack->render_file) {
          mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
          mt_clip_select(mainw->multitrack, TRUE);
        }
        return mainw->current_file;
      }
      if (!mainw->noswitch) {
        switch_clip(1, index, TRUE);
        if (mainw->blend_file == old_file)
          switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
        d_print("");
      }
      return index;
    }

    if (mainw->clips_available > 0) {
      for (int i = mainw->current_file; --i;) {
        if (mainw->files[i]) {
          if (!mainw->noswitch) {
            switch_clip(1, i, TRUE);
            if (mainw->blend_file == old_file)
              switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
            d_print("");
          }
          return i;
        }
      }

      for (int i = 1; i < MAX_FILES; i++) {
        if (mainw->files[i]) {
          if (!mainw->noswitch) {
            switch_clip(1, i, TRUE);
            if (mainw->blend_file == old_file)
              switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
            d_print("");
          }
          return i;
        }
      }
    }
    return -1;
  }


  // return the new current_file - if the player is active then we do not switch, but let the player do that
  int close_current_file(int file_to_switch_to) {
    // close the current file, and free the file struct and all sub storage
    LiVESList *list_index;
    char *com;
    boolean need_new_blend_file = FALSE;
    boolean noswitch = mainw->noswitch;
    int index = -1;
    int old_file = mainw->current_file;
    int i;

    if (mainw->close_this_clip == mainw->current_file) mainw->close_this_clip = -1;

    if (!CURRENT_CLIP_IS_VALID) return -1;

    if (cfile->clip_type == CLIP_TYPE_TEMP) {
      close_temp_handle(file_to_switch_to);
      return file_to_switch_to;
    }

    if (mainw->noswitch && !mainw->can_switch_clips) {
      mainw->new_clip = file_to_switch_to;
      mainw->close_this_clip = mainw->current_file;
      return mainw->new_clip;
    }

    if (cfile->clip_type != CLIP_TYPE_GENERATOR && mainw->current_file != mainw->scrap_file &&
        mainw->current_file != mainw->ascrap_file && mainw->current_file != 0 &&
        (!mainw->multitrack || mainw->current_file != mainw->multitrack->render_file)) {
      d_print(_("Closed clip %s\n"), cfile->file_name);
      lives_notify(LIVES_OSC_NOTIFY_CLIP_CLOSED, "");
    }

    cfile->hsize = mainw->def_width;
    cfile->vsize = mainw->def_height;

    if (cfile->laudio_drawable) {
      if (mainw->laudio_drawable == cfile->laudio_drawable
          || mainw->drawsrc == mainw->current_file) mainw->laudio_drawable = NULL;
      if (cairo_surface_get_reference_count(cfile->laudio_drawable))
        lives_painter_surface_destroy(cfile->laudio_drawable);
      cfile->laudio_drawable = NULL;
    }
    if (cfile->raudio_drawable) {
      if (mainw->raudio_drawable == cfile->raudio_drawable
          || mainw->drawsrc == mainw->current_file) mainw->raudio_drawable = NULL;
      if (cairo_surface_get_reference_count(cfile->raudio_drawable))
        lives_painter_surface_destroy(cfile->raudio_drawable);
      cfile->raudio_drawable = NULL;
    }
    if (mainw->drawsrc == mainw->current_file) mainw->drawsrc = -1;

    if (mainw->st_fcache) {
      if (mainw->en_fcache == mainw->st_fcache) mainw->en_fcache = NULL;
      if (mainw->pr_fcache == mainw->st_fcache) mainw->pr_fcache = NULL;
      weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = NULL;
    }
    if (mainw->en_fcache) {
      if (mainw->pr_fcache == mainw->en_fcache) mainw->pr_fcache = NULL;
      weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
    }
    if (mainw->pr_fcache) {
      weed_layer_unref(mainw->pr_fcache);
      mainw->pr_fcache = NULL;
    }

    for (i = 0; i < FN_KEYS - 1; i++) {
      if (mainw->clipstore[i][0] == mainw->current_file) mainw->clipstore[i][0] = -1;
    }

    // this must all be done last...
    if (cfile->menuentry) {
      // c.f. on_prevclip_activate
      list_index = lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
      do {
        if (!(list_index = lives_list_previous(list_index))) list_index = lives_list_last(mainw->cliplist);
        index = LIVES_POINTER_TO_INT(lives_list_nth_data(list_index, 0));
      } while ((mainw->files[index] || mainw->files[index]->opening || mainw->files[index]->restoring ||
                (index == mainw->scrap_file && index > -1) || (index == mainw->ascrap_file && index > -1)
                || (mainw->files[index]->frames == 0 &&
                    LIVES_IS_PLAYING)) &&
               index != mainw->current_file);
      if (index == mainw->current_file) index = -1;
      if (mainw->current_file != mainw->scrap_file && mainw->current_file != mainw->ascrap_file) remove_from_clipmenu();
    }

    if (mainw->blend_file == mainw->current_file) {
      // set blend_file to -1. This is in case the file is a generator - we need to distinguish between the cases where
      // the generator is the blend file and we switch because it was deinited, and when we switch fg <-> bg
      // in the former case the generator is killed off, in the latter it survives
      need_new_blend_file = TRUE;
      track_decoder_free(1, mainw->blend_file);
      mainw->blend_file = -1;
      weed_layer_unref(mainw->blend_layer);
      mainw->blend_layer = NULL;
    }

    if (CURRENT_CLIP_IS_PHYSICAL && (cfile->ext_src || cfile->old_ext_src)) {
      if (!cfile->ext_src && cfile->old_ext_src_type == LIVES_EXT_SRC_DECODER) {
        cfile->ext_src = cfile->old_ext_src;
        cfile->ext_src_type = cfile->old_ext_src_type;
      }
      if (cfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
        close_clip_decoder(mainw->current_file);
      }
    }
    free_thumb_cache(mainw->current_file, 0);
    lives_freep((void **)&cfile->frame_index);
    lives_freep((void **)&cfile->frame_index_back);

    if (cfile->clip_type != CLIP_TYPE_GENERATOR && !mainw->close_keep_frames) {
      char *clipd = get_clip_dir(mainw->current_file);
      if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
        // as a safety feature we create a special file which allows the back end to delete the directory
        char *temp_backend;
        char *permitname = lives_build_filename(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
        lives_touch(permitname);
        lives_free(permitname);
        temp_backend = use_staging_dir_for(mainw->current_file);
        com = lives_strdup_printf("%s close \"%s\"", temp_backend, cfile->handle);
        lives_free(temp_backend);
        lives_system(com, TRUE);
        lives_free(com);
        temp_backend = lives_build_path(LIVES_DEVICE_DIR, LIVES_SHM_DIR, cfile->handle, NULL);
        if (!lives_file_test(temp_backend, LIVES_FILE_TEST_IS_DIR)) {
          lives_rmdir_with_parents(temp_backend);
        }
        lives_free(temp_backend);
      }
      lives_free(clipd);
      if (cfile->event_list_back) event_list_free(cfile->event_list_back);
      if (cfile->event_list) event_list_free(cfile->event_list);

      lives_list_free_all(&cfile->layout_map);
    }

    if (cfile->subt) subtitles_free(cfile);

    if (cfile->clip_type == CLIP_TYPE_YUV4MPEG) {
#ifdef HAVE_YUV4MPEG
      lives_yuv_stream_stop_read((lives_yuv4m_t *)cfile->ext_src);
      lives_free(cfile->ext_src);
#endif
    }

    if (cfile->clip_type == CLIP_TYPE_VIDEODEV) {
#ifdef HAVE_UNICAP
      if (cfile->ext_src) lives_vdev_free((lives_vdev_t *)cfile->ext_src);
#endif
    }

    if (cfile->audio_waveform) {
      cancel_tl_redraw();
      for (i = 0; i < cfile->achans; i++) lives_freep((void **)&cfile->audio_waveform[i]);
      lives_freep((void **)&cfile->audio_waveform);
      lives_freep((void **)&cfile->aw_sizes);
      cfile->aw_sizes = NULL;
    }

    if (mainw->files[mainw->current_file]) {
      lives_free(mainw->files[mainw->current_file]);
      mainw->files[mainw->current_file] = NULL;
    }

    if (mainw->multitrack && mainw->current_file != mainw->multitrack->render_file) {
      mt_delete_clips(mainw->multitrack, mainw->current_file);
    }

    if ((mainw->first_free_file == ALL_USED || mainw->first_free_file > mainw->current_file) && mainw->current_file > 0)
      mainw->first_free_file = mainw->current_file;

    if (!mainw->only_close) {
      if (IS_VALID_CLIP(file_to_switch_to) && file_to_switch_to > 0) {
        if (!mainw->multitrack) {
          if (!mainw->noswitch) {
            mainw->current_file = file_to_switch_to;
            switch_clip(1, file_to_switch_to, TRUE);
            d_print("");
          } else {
            if (file_to_switch_to != mainw->playing_file) mainw->new_clip = file_to_switch_to;
          }
        } else if (old_file != mainw->multitrack->render_file) {
          mt_clip_select(mainw->multitrack, TRUE);
        }
        return file_to_switch_to;
      }
    }
    // file we were asked to switch to is invalid, thus we must find one

    //if (mainw->only_close) mainw->noswitch = TRUE;

    file_to_switch_to = find_next_clip(index, old_file);

    if (mainw->noswitch) {
      mainw->noswitch = noswitch;
      return file_to_switch_to;
    }

    if (need_new_blend_file) mainw->blend_file = mainw->current_file;

    if (CURRENT_CLIP_IS_VALID) return mainw->current_file;

    // no other clips
    mainw->preview_frame = 0;
    mainw->current_file = mainw->blend_file = -1;
    set_main_title(NULL, 0);

    if (mainw->play_window) {
      lives_widget_hide(mainw->preview_controls);
      load_preview_image(FALSE);
      resize_play_window();
    }
    lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
    lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
    lives_widget_set_sensitive(mainw->export_proj, FALSE);
    lives_widget_set_sensitive(mainw->import_proj, FALSE);

    if (mainw->multitrack && old_file != mainw->multitrack->render_file)
      lives_widget_set_sensitive(mainw->multitrack->load_set, TRUE);

    // can't use set_undoable, as we don't have a cfile
    lives_menu_item_set_text(mainw->undo, _("_Undo"), TRUE);
    lives_menu_item_set_text(mainw->redo, _("_Redo"), TRUE);
    lives_widget_hide(mainw->redo);
    lives_widget_show(mainw->undo);
    lives_widget_set_sensitive(mainw->undo, FALSE);

    if (!mainw->is_ready || mainw->recovering_files) return -1;

    if (LIVES_IS_PLAYING) mainw->cancelled = CANCEL_GENERATOR_END;

    if (!mainw->multitrack) {
      //resize(1);
      lives_widget_set_opacity(mainw->playframe, 0.);
      //lives_widget_hide(mainw->playframe);
      load_start_image(0);
      load_end_image(0);
      if (prefs->show_msg_area && !mainw->only_close) {
        if (mainw->idlemax == 0) {
          lives_idle_add(resize_message_area, NULL);
        }
        mainw->idlemax = DEF_IDLE_MAX;
      }
    }

    set_sel_label(mainw->sel_label);

    zero_spinbuttons();
    show_playbar_labels(-1);

    if (!mainw->only_close) {
      lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
      if (!LIVES_IS_PLAYING) d_print("");

      if (mainw->multitrack) {
        if (old_file != mainw->multitrack->render_file) {
          mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
          mt_clip_select(mainw->multitrack, TRUE);
        }
      }
    }
    if (!LIVES_IS_PLAYING && !mainw->is_processing && !mainw->preview) {
      if (mainw->multitrack) {
        if (old_file != mainw->multitrack->render_file) {
          mt_sensitise(mainw->multitrack);
        }
      } else sensitize();
    }
    return mainw->current_file;
  }


  void switch_to_file(int old_file, int new_file) {
    // this function is used for full clip switching (during non-playback or non fs)

    // calling this function directly is now deprecated in favour of switch_clip()

    int orig_file = mainw->current_file;

    // should use close_current_file
    if (!IS_VALID_CLIP(new_file)) {
      char *msg = lives_strdup_printf("attempt to switch to invalid clip %d", new_file);
      LIVES_WARN(msg);
      lives_free(msg);
      return;
    }

    if (!LIVES_IS_PLAYING) {
      if (old_file != new_file) {
        if (CURRENT_CLIP_IS_VALID) {
          mainw->laudio_drawable = cfile->laudio_drawable;
          mainw->raudio_drawable = cfile->raudio_drawable;
          mainw->drawsrc = mainw->current_file;
        }
      }
    }

    if (mainw->multitrack) return;

    if (mainw->noswitch) {
      mainw->new_clip = new_file;
      return;
    }

    mainw->current_file = new_file;

    if (old_file != new_file) {
      if (CURRENT_CLIP_IS_VALID) {
        mainw->laudio_drawable = cfile->laudio_drawable;
        mainw->raudio_drawable = cfile->raudio_drawable;
        mainw->drawsrc = mainw->current_file;
      }
      if (old_file != 0 && new_file != 0) mainw->preview_frame = 0;
      lives_widget_set_sensitive(mainw->select_new, (cfile->insert_start > 0));
      lives_widget_set_sensitive(mainw->select_last, (cfile->undo_start > 0));
      if ((cfile->start == 1 || cfile->end == cfile->frames) && !(cfile->start == 1 && cfile->end == cfile->frames)) {
        lives_widget_set_sensitive(mainw->select_invert, TRUE);
      } else {
        lives_widget_set_sensitive(mainw->select_invert, FALSE);
      }
      if (IS_VALID_CLIP(old_file) && mainw->files[old_file]->opening) {
        // switch while opening - come out of processing dialog
        if (mainw->proc_ptr) {
          lives_widget_destroy(mainw->proc_ptr->processing);
          lives_freep((void **)&mainw->proc_ptr->text);
          lives_freep((void **)&mainw->proc_ptr);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

    if (mainw->play_window && cfile->is_loaded && orig_file != new_file) {
      // if the clip is loaded
      if (!mainw->preview_box) {
        // create the preview box that shows frames...
        make_preview_box();
      }
      // add it the play window...
      if (!lives_widget_get_parent(mainw->preview_box)) {
        lives_widget_queue_draw(mainw->play_window);
        lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
      }

      lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
      lives_widget_show_now(mainw->preview_box);
      lives_widget_context_update();
      lives_widget_set_no_show_all(mainw->preview_controls, TRUE);

      // and resize it
      lives_widget_grab_focus(mainw->preview_spinbutton);

      resize_play_window();
      load_preview_image(FALSE);
    }

    if (!mainw->go_away && CURRENT_CLIP_IS_NORMAL) {
      mainw->no_context_update = TRUE;
      reget_afilesize(mainw->current_file);
      mainw->no_context_update = FALSE;
    }

    if (!CURRENT_CLIP_IS_VALID) return;
    //chill_decoder_plugin(mainw->current_file);

    if (!CURRENT_CLIP_IS_NORMAL || cfile->opening) {
      lives_widget_set_sensitive(mainw->rename, FALSE);
    }

    if (cfile->menuentry) {
      reset_clipmenu();
    }

    if (!mainw->clip_switched && !cfile->opening) sensitize();

    lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
    lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);

    set_sel_label(mainw->sel_label);

    if (mainw->eventbox5) lives_widget_show(mainw->eventbox5);
    lives_widget_show(mainw->hruler);
    lives_widget_show(mainw->vidbar);
    lives_widget_show(mainw->laudbar);

    if (cfile->achans < 2) {
      lives_widget_hide(mainw->raudbar);
    } else {
      lives_widget_show(mainw->raudbar);
    }

    if (cfile->redoable) {
      lives_widget_show(mainw->redo);
      lives_widget_hide(mainw->undo);
    } else {
      lives_widget_hide(mainw->redo);
      lives_widget_show(mainw->undo);
    }

    if (new_file > 0) {
      if (cfile->menuentry) {
        set_main_title(cfile->name, 0);
      } else set_main_title(cfile->file_name, 0);
    }

    set_start_end_spins(mainw->current_file);

    resize(1);
    if (!mainw->go_away) {
      get_play_times();
    }

    // if the file was opening, continue...
    if (cfile->opening) {
      open_file(cfile->file_name);
    } else {
      showclipimgs();
      //lives_widget_context_update();
      lives_ce_update_timeline(0, cfile->pointer_time);
      mainw->ptrtime = cfile->pointer_time;
      lives_widget_queue_draw(mainw->eventbox2);
    }

    if (!mainw->multitrack && !mainw->reconfig) {
      if (prefs->show_msg_area && !mainw->only_close) {
        reset_message_area(); // necessary
        if (mainw->idlemax == 0) {
          lives_idle_add(resize_message_area, NULL);
        }
        mainw->idlemax = DEF_IDLE_MAX;
      }
      redraw_timeline(mainw->current_file);
    }
  }


  boolean switch_audio_clip(int new_file, boolean activate) {
    //ticks_t cticks;
    lives_clip_t *sfile;
    int aplay_file;

    if (AUD_SRC_EXTERNAL) return FALSE;

    aplay_file = get_aplay_clipno();

    if ((aplay_file == new_file && (!(prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
                                    || !CLIP_HAS_AUDIO(aplay_file)))
        || (aplay_file != new_file && !(prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS))) {
      return FALSE;
    }

    sfile = RETURN_VALID_CLIP(new_file);
    if (!sfile) return FALSE;

    if (new_file == aplay_file) {
      if (sfile->pb_fps > 0. || (sfile->play_paused && sfile->freeze_fps > 0.))
        sfile->adirection = LIVES_DIRECTION_FORWARD;
      else sfile->adirection = LIVES_DIRECTION_REVERSE;
    } else if (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP) {
      mainw->scratch = SCRATCH_JUMP;
    }

    if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd) {
        if (!activate) mainw->jackd->in_use = FALSE;

        if (new_file != aplay_file) {
          if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }
          if (mainw->jackd->playing_file > 0) {
            if (!CLIP_HAS_AUDIO(new_file)) {
              jack_get_rec_avals(mainw->jackd);
              mainw->rec_avel = 0.;
            }
            jack_message.command = ASERVER_CMD_FILE_CLOSE;
            jack_message.data = NULL;
            jack_message.next = NULL;
            mainw->jackd->msgq = &jack_message;

            if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
              mainw->cancelled = handle_audio_timeout();
              return FALSE;
            }
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

      if (!IS_VALID_CLIP(new_file)) {
        mainw->jackd->in_use = FALSE;
        return FALSE;
      }

      if (new_file == aplay_file) return TRUE;

      if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
        // tell jack server to open audio file and start playing it

        jack_message.command = ASERVER_CMD_FILE_OPEN;

        jack_message.data = lives_strdup_printf("%d", new_file);

        jack_message2.command = ASERVER_CMD_FILE_SEEK;

        jack_message.next = &jack_message2;
        jack_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
        jack_message2.next = NULL;

        mainw->jackd->msgq = &jack_message;

        mainw->jackd->in_use = TRUE;

        if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
          mainw->cancelled = handle_audio_timeout();
          return FALSE;
        }

        //jack_time_reset(mainw->jackd, 0);

        mainw->jackd->is_paused = sfile->play_paused;
        mainw->jackd->is_silent = FALSE;
      } else {
        mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
      }
#endif
    }

    if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed) {
        if (!activate) mainw->pulsed->in_use = FALSE;

        if (new_file != aplay_file) {
          if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }

          if (IS_PHYSICAL_CLIP(aplay_file)) {
            lives_clip_t *afile = mainw->files[aplay_file];
            if (!CLIP_HAS_AUDIO(new_file)) {
              pulse_get_rec_avals(mainw->pulsed);
              mainw->rec_avel = 0.;
            }
            pulse_message.command = ASERVER_CMD_FILE_CLOSE;
            pulse_message.data = NULL;
            pulse_message.next = NULL;
            mainw->pulsed->msgq = &pulse_message;

            if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
              mainw->cancelled = handle_audio_timeout();
              return FALSE;
            }

            afile->sync_delta = lives_pulse_get_time(mainw->pulsed) - mainw->startticks;
            afile->aseek_pos = mainw->pulsed->seek_pos;
          }

          if (!IS_VALID_CLIP(new_file)) {
            mainw->pulsed->in_use = FALSE;
            return FALSE;
          }

          if (new_file == aplay_file) return TRUE;

          if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
            // tell pulse server to open audio file and start playing it

            mainw->video_seek_ready = FALSE;

            // have to set this, else the msgs are not acted on
            pulse_driver_uncork(mainw->pulsed);

            pulse_message.command = ASERVER_CMD_FILE_OPEN;
            pulse_message.data = lives_strdup_printf("%d", new_file);

            pulse_message2.command = ASERVER_CMD_FILE_SEEK;
            /* if (LIVES_IS_PLAYING && !mainw->preview) { */
            /*   pulse_message2.tc = cticks; */
            /*   pulse_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST; */
            /* } */
            pulse_message.next = &pulse_message2;
            pulse_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
            pulse_message2.next = NULL;

            mainw->pulsed->msgq = &pulse_message;

            mainw->pulsed->is_paused = sfile->play_paused;

            //pa_time_reset(mainw->pulsed, 0);
          } else {
            mainw->video_seek_ready = TRUE;
            video_sync_ready();
          }
        }
      }
#endif
    }

#if 0

    if (prefs->audio_player == AUD_PLAYER_NONE) {
      if (!IS_VALID_CLIP(new_file)) {
        mainw->nullaudio_playing_file = -1;
        return FALSE;
      }
      if (mainw->nullaudio->playing_file == new_file) return FALSE;
      nullaudio_clip_set(new_file);
      if (activate && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
        if (!sfile->play_paused)
          nullaudio_arate_set(sfile->arate * sfile->pb_fps / sfile->fps);
        else nullaudio_arate_set(sfile->arate * sfile->freeze_fps / sfile->fps);
      } else nullaudio_arate_set(sfile->arate);
      nullaudio_seek_set(sfile->aseek_pos);
      if (CLIP_HAS_AUDIO(new_file)) {
        nullaudio_get_rec_avals();
      } else {
        nullaudio_get_rec_avals();
        mainw->rec_avel = 0.;
      }
    }
#endif

    return TRUE;
  }


  void do_quick_switch(int new_file) {
    // handle clip switching during playback
    // calling this function directly is now deprecated in favour of switch_clip()
    lives_clip_t *sfile = NULL;
    boolean osc_block;
    int old_file = mainw->playing_file;

    if (!LIVES_IS_PLAYING) {
      switch_to_file(mainw->current_file, new_file);
      return;
    }

    if (old_file == new_file || old_file < 1 || !IS_VALID_CLIP(new_file)) return;

    if (mainw->multitrack
        || (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_CLIPS)) ||
        mainw->foreign || (mainw->preview && !mainw->is_rendering)) return;

    if (mainw->noswitch && !mainw->can_switch_clips) {
      mainw->new_clip = new_file;
      return;
    }

    if (IS_VALID_CLIP(old_file)) sfile = mainw->files[old_file];

    mainw->whentostop = NEVER_STOP;
    mainw->blend_palette = WEED_PALETTE_END;

    if (old_file != mainw->blend_file && !mainw->is_rendering) {
      if (sfile && sfile->clip_type == CLIP_TYPE_GENERATOR && sfile->ext_src) {
        if (new_file != mainw->blend_file) {
          // switched from generator to another clip, end the generator
          weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
          mainw->gen_started_play = FALSE;
          weed_generator_end(inst);
        } else {
          // switch fg to bg
          rte_swap_fg_bg();
        }
      }

      if (new_file == mainw->blend_file && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR
          && mainw->files[mainw->blend_file]->ext_src) {
        // switch bg to fg
        rte_swap_fg_bg();
      }
    }

    osc_block = mainw->osc_block;
    mainw->osc_block = TRUE;

    if (mainw->loop_locked) {
      unlock_loop_lock();
    }

    mainw->clip_switched = TRUE;

    if (mainw->frame_layer_preload && mainw->frame_layer_preload != mainw->frame_layer) {
      check_layer_ready(mainw->frame_layer_preload);
      weed_layer_unref(mainw->frame_layer_preload);
      mainw->frame_layer_preload = NULL;
      mainw->pred_clip = -1;
      mainw->pred_frame = 0;
    }

    mainw->drawsrc = mainw->playing_file = mainw->current_file = new_file;
    mainw->laudio_drawable = cfile->laudio_drawable;
    mainw->raudio_drawable = cfile->raudio_drawable;

    lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), mainw->files[new_file]->pb_fps);
    lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps));
    lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

    // switch audio clip
    if (AUD_SRC_INTERNAL && (!mainw->event_list || mainw->record)) {
      if (APLAYER_REALTIME && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
          && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)
          && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || mainw->agen_needs_reinit))) {
        switch_audio_clip(new_file, TRUE);
      }
    }

    set_main_title(cfile->name, 0);

    if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_FOREGROUND) ce_thumbs_highlight_current_clip();

    if (CURRENT_CLIP_IS_NORMAL) {
      char *tmp;
      tmp = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
      lives_snprintf(cfile->info_file, PATH_MAX, "%s", tmp);
      lives_free(tmp);
    }

    if (!CURRENT_CLIP_IS_NORMAL || (mainw->event_list && !mainw->record))
      mainw->play_end = INT_MAX;

    // act like we are not playing a selection (but we will try to keep to
    // selection bounds)
    mainw->playing_sel = FALSE;

    if (cfile->last_play_sequence != mainw->play_sequence) {
      cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
    }

    changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), LIVES_INT_TO_POINTER(1));

    cfile->next_event = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
    if (LIVES_IS_PLAYING && !mainw->play_window && (!IS_VALID_CLIP(old_file)
        || !CURRENT_CLIP_IS_VALID || cfile->hsize != mainw->files[old_file]->hsize
        || cfile->vsize != mainw->files[old_file]->vsize)) {
      clear_widget_bg(mainw->play_image, mainw->play_surface);
    }
#endif

    if (CURRENT_CLIP_HAS_VIDEO) {
      if (!mainw->fs && !mainw->faded) {
        set_start_end_spins(mainw->current_file);
        if (!mainw->play_window && mainw->double_size) {
          //frame_size_update();
          resize(2.);
        } else resize(1);
      }
    } else resize(1);

    if (!mainw->fs && !mainw->faded) showclipimgs();

    if (new_file == mainw->blend_file || old_file == mainw->blend_file) {
      if (mainw->blend_layer) {
        check_layer_ready(mainw->blend_layer);
        weed_layer_unref(mainw->blend_layer);
        mainw->blend_layer = NULL;
      }
      track_decoder_free(1, mainw->blend_file);
      if (new_file == mainw->blend_file)
        mainw->blend_file = old_file;
    }

    if (mainw->play_window && prefs->show_playwin) {
      lives_window_present(LIVES_WINDOW(mainw->play_window));
      lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
    }

    mainw->osc_block = osc_block;
    lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);

    redraw_timeline(mainw->current_file);
  }


  void resize(double scale) {
    // resize the frame widgets
    // set scale < 0. to _force_ the playback frame to expand (for external capture)
    LiVESXWindow *xwin;
    double oscale = scale;

    int xsize, vspace;
    int bx, by;

    // height of the separator imeage

    // maximum values
    int hsize, vsize;
    int w, h;
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;

    if (!prefs->show_gui || mainw->multitrack) return;
    vspace = get_vspace();

    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    bx = 2 * abs(bx);
    by = abs(by);

    if (!mainw->hdrbar) {
      if (prefs->open_maximised && by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }

    w = lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET);
    h = lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET);

    // resize the main window so it fits the gui monitor
    if (prefs->open_maximised)
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    else if (w > scr_width - bx || h > scr_height - by) {
      w = scr_width - bx;
      h = scr_height - by - mainw->mbar_res;
      lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
    }

    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
                          + (prefs->show_msg_area ? mainw->mbar_res : 0)
                          + widget_opts.border_width * 2);

    if (scale < 0.) {
      // foreign capture
      scale = -scale;
      hsize = (scr_width - H_RESIZE_ADJUST - bx) / scale;
      vsize = (scr_height - V_RESIZE_ADJUST - by) / scale;
    }

    mainw->ce_frame_width = hsize;
    mainw->ce_frame_height = vsize;

    if (oscale == 2.) {
      if (hsize * 2 > scr_width - SCR_WIDTH_SAFETY) {
        scale = 1.;
      }
    }

    if (oscale > 0.) {
      if (scale > 1.) {
        // this is the size for the start and end frames
        // they shrink when scale is 2.0
        mainw->ce_frame_width = hsize / scale;
        mainw->ce_frame_height = vsize / scale + V_RESIZE_ADJUST;

        lives_widget_set_margin_left(mainw->playframe, widget_opts.packing_width);
        lives_widget_set_margin_right(mainw->playframe, widget_opts.packing_width);
      }

      if (CURRENT_CLIP_IS_VALID) {
        if (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV) {
          if (!mainw->camframe) {
            LiVESError *error = NULL;
            char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
            char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
            mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
            if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
            lives_free(tmp); lives_free(fname);
          }
        }
      }

      // THE SIZES OF THE FRAME CONTAINERS
      lives_widget_set_size_request(mainw->frame1, mainw->ce_frame_width, mainw->ce_frame_height);
      lives_widget_set_size_request(mainw->eventbox3, mainw->ce_frame_width, mainw->ce_frame_height);
      lives_widget_set_size_request(mainw->frame2, mainw->ce_frame_width, mainw->ce_frame_height);
      lives_widget_set_size_request(mainw->eventbox4, mainw->ce_frame_width, mainw->ce_frame_height);

      lives_widget_set_size_request(mainw->start_image, mainw->ce_frame_width, mainw->ce_frame_height);
      lives_widget_set_size_request(mainw->end_image, mainw->ce_frame_width, mainw->ce_frame_height);

      // use unscaled size in dblsize
      if (scale > 1.) {
        hsize *= scale;
        vsize *= scale;
      }
      lives_widget_set_size_request(mainw->playframe, hsize, vsize);
      lives_widget_set_size_request(mainw->pl_eventbox, hsize, vsize);
      lives_widget_set_size_request(mainw->playarea, hsize, vsize);

      // IMPORTANT (or the entire image will not be shown)
      lives_widget_set_size_request(mainw->play_image, hsize, vsize);
      xwin = lives_widget_get_xwindow(mainw->play_image);
      if (LIVES_IS_XWINDOW(xwin)) {
        if (mainw->play_surface) lives_painter_surface_destroy(mainw->play_surface);
        mainw->play_surface =
          lives_xwindow_create_similar_surface(xwin, LIVES_PAINTER_CONTENT_COLOR,
                                               hsize, vsize);
        clear_widget_bg(mainw->play_image, mainw->play_surface);
      }
    } else {
      // capture window size
      xsize = (scr_width - hsize * -oscale - H_RESIZE_ADJUST) / 2;
      if (xsize > 0) {
        lives_widget_set_size_request(mainw->frame1, xsize / scale, vsize + V_RESIZE_ADJUST);
        lives_widget_set_size_request(mainw->eventbox3, xsize / scale, vsize + V_RESIZE_ADJUST);
        lives_widget_set_size_request(mainw->frame2, xsize / scale, vsize + V_RESIZE_ADJUST);
        lives_widget_set_size_request(mainw->eventbox4, xsize / scale, vsize + V_RESIZE_ADJUST);
        mainw->ce_frame_width = xsize / scale;
        mainw->ce_frame_height = vsize + V_RESIZE_ADJUST;
      } else {
        lives_widget_hide(mainw->frame1);
        lives_widget_hide(mainw->frame2);
        lives_widget_hide(mainw->eventbox3);
        lives_widget_hide(mainw->eventbox4);
      }
    }

    if (!mainw->foreign && mainw->current_file == -1) {
      lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
      load_start_image(0);
      load_end_image(0);
    }

    //  update_sel_menu();

    if (scale != oscale) {
      lives_widget_context_update();
      if (prefs->open_maximised)
        lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      else if (w > scr_width - bx || h > scr_height - by) {
        w = scr_width - bx;
        h = scr_height - by;
        lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
        lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
      }
    }
  }


