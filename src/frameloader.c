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
#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#include <png.h>

static int xxwidth = 0, xxheight = 0;

lives_result_t lives_blankframe_srcfunc(weed_layer_t *layer, boolean async) {
  weed_layer_t *xlayer = create_blank_layer(layer, NULL, 0, 0, WEED_GAMMA_UNKNOWN);
  if (xlayer) return LIVES_RESULT_SUCCESS;
  return LIVES_RESULT_ERROR;
}

// async
lives_result_t lives_imgloader_srcfunc(weed_layer_t *layer, boolean async) {
  if (layer) {
    if (async) return pull_frame_threaded(layer, 0, 0);
    else {
      int clipno = lives_layer_get_clip(layer);
      lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
      if (sfile) {
        weed_timecode_t tc;
        ticks_t *timing_data;
        const char *img_ext = get_image_ext_for_type(sfile->img_type);
        boolean res;

        lives_layer_set_status(layer, LAYER_STATUS_QUEUED);

        lock_layer_status(layer);
        timing_data = _get_layer_timing(layer);
        tc = timing_data[LAYER_STATUS_TREF];
        if (!tc) tc = timing_data[LAYER_STATUS_TREF] = timing_data[LAYER_STATUS_QUEUED];
        _set_layer_timing(layer, timing_data);
        lives_free(timing_data);
        unlock_layer_status(layer);

        weed_layer_pixel_data_free(layer);
        weed_set_int64_value(layer, WEED_LEAF_HOST_TC, tc);
        //
#ifdef NO_FRAME_THREAD
        res = pull_frame(layer, tc);
#else
        res = pull_frame_at_size(layer, img_ext, tc, weed_layer_get_width(layer),
                                 weed_layer_get_height(layer),
                                 weed_layer_get_palette(layer));
#endif
        if (res) return LIVES_RESULT_SUCCESS;
      }
    }
  }
  return LIVES_RESULT_ERROR;
}


/* static void scrapfile_loader(weed_layer_t *layer, frames_t frame) { */
/*   lives_result_t res; */
/*   lives_layer_set_status(layer, LAYER_STATUS_LOADING); */
/*   res = load_from_scrap_file(layer, frame); */
/*   if (!res) weed_layer_set_invalid(layer, TRUE); */
/*   else { */
/*     // realign */
/*     lives_layer_set_status(layer, LAYER_STATUS_LOADED); */
/*     copy_pixel_data(layer, NULL, THREADVAR(rowstride_alignment)); */
/*     lives_layer_set_status(layer, LAYER_STATUS_READY); */
/*   } */
/* } */


/* lives_result_t lives_filebuff_srcfunc(weed_layer_t *layer) { */
/*   boolean res; */
/*   frames_t frame = lives_layer_get_frame(layer); */
/*   lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED, */
/* 				 (lives_funcptr_t)scrapfile_loader, */
/* 				 0, "vi", &orig_layer); */
/* } */


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

  bytes = lives_popen(com, FALSE, mainw->msg);
  lives_free(com);

  if (THREADVAR(com_failed)) return 0;

  if (bytes > 0) return atoi(mainw->msg);
  return 0;
}


lives_result_t get_frames_sizes(int clipno, frames_t frame, int *hsize, int *vsize) {
  // get the actual physical frame size
  weed_layer_t *layer = lives_layer_new_for_frame(clipno, frame);
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, LIVES_LAYER_GET_SIZE_ONLY);
  if (!weed_layer_create_from_file_progressive(layer, NULL))
    return LIVES_RESULT_FAIL;
  if (hsize) *hsize = weed_layer_get_width(layer);
  if (vsize) *vsize = weed_layer_get_height(layer);
  weed_layer_unref(layer);
  return LIVES_RESULT_SUCCESS;
}


// TODO - use md5_frame(weed_layer_t *layer), then compare with sfile->frame_md5s

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
          weed_leaf_set_autofree(layer, LIVES_LEAF_MD5SUM, TRUE);
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


LIVES_GLOBAL_INLINE void copy_md5(lives_layer_t *dl, lives_layer_t *sl) {
  if (sl && dl) {
    uint8_t *smd5buf = weed_get_voidptr_value(sl, LIVES_LEAF_MD5SUM, NULL);
    if (smd5buf) {
      uint8_t *dmd5buf = weed_get_voidptr_value(dl, LIVES_LEAF_MD5SUM, NULL);
      if (!dmd5buf) {
        dmd5buf = lives_malloc(MD5_SIZE);
        if (!dmd5buf) return;
        weed_set_voidptr_value(dl, LIVES_LEAF_MD5SUM, dmd5buf);
        weed_leaf_set_autofree(dl, LIVES_LEAF_MD5SUM, TRUE);
      }
      lives_memcpy(dmd5buf, smd5buf, MD5_SIZE);
      lives_leaf_dup_nocheck(dl, sl, LIVES_LEAF_MD5_CHKSIZE);
    }
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
  if (CURRENT_CLIP_IS_VALID) {
    load_end_image(cfile->end);
    load_start_image(cfile->start);
  } else {
    load_end_image(0);
    load_start_image(0);
  }
}


void load_start_image(frames_t frame) {
  LiVESPixbuf *start_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  pthread_mutex_t *mutex;
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

  mutex = lives_widget_get_mutex(mainw->start_image);
  pthread_mutex_lock(mutex);

  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->start_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int hsize, vsize;
    get_gui_framesize(&hsize, &vsize);

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
    pthread_mutex_unlock(mutex);
    set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->start_image), mainw->camframe);
    //lives_widget_queue_draw(mainw->start_image);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    pthread_mutex_unlock(mutex);
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->start_image), mainw->imframe);
    } else {
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->start_image), NULL);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->start_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->start_image, 0.4);
    }
    //lives_widget_queue_draw(mainw->start_image);
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
      weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) {
	if (mainw->pr_fcache) {
	  mainw->st_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
	  weed_layer_ref(mainw->pr_fcache);
	  weed_layer_copy(mainw->st_fcache, mainw->pr_fcache);
	  copy_md5(mainw->st_fcache, mainw->pr_fcache);
	  weed_layer_unref(mainw->pr_fcache);
	}
      }
      if (!mainw->st_fcache) {
	if (mainw->en_fcache) {
	  mainw->st_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
	  weed_layer_ref(mainw->en_fcache);
	  weed_layer_copy(mainw->st_fcache, mainw->en_fcache);
	  copy_md5(mainw->st_fcache, mainw->en_fcache);
	  weed_layer_unref(mainw->en_fcache);
	}
	tries = 0;
      }
      if (mainw->st_fcache) goto check_stcache;
    }
  }
  lives_freep((void **)&fname);

  tc = ((frame - 1.)) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect && !prefs->letterbox) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE
	&& is_virtual_frame(mainw->current_file, frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (!mainw->go_away) {
	lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
	if (!srcgrp) srcgrp = clone_srcgrp(mainw->current_file, mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
	lives_layer_set_srcgrp(layer, srcgrp);
	weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
	interp = get_interp_value(prefs->pb_quality, TRUE);
	if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
	    !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
	  if (layer) weed_layer_free(layer);
	  pthread_mutex_unlock(mutex);
	  return;
	}
      } else weed_layer_set_invalid(layer, TRUE);
    }

    // layer maybe a cached layer, and also shared, e.g st_fcache == en_fcache
    // when we convert to pixbuf, this may be done by the cheat method, pxibuf
    // adds a proxy layer, adds the proxy to the copylist,
    // this layer is nullifed, removing it from the copy list
    // we then copy back pixbuf to layer - pixdata is shared back to layer so we re-add layer to copylist
    // when lyaer is freed, it ie removed from copy list, so pixbuf and othe cachers have pixdata
    // when pixbuf is unreffed, proxy layer is removed from copylist

    if (!start_pixbuf) start_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(start_pixbuf)) {
      pthread_mutex_unlock(mutex);
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->start_image), start_pixbuf);
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, start_pixbuf)))
        lives_widget_object_unref(start_pixbuf);
      //lives_widget_queue_draw(mainw->start_image);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (mainw->st_fcache && mainw->st_fcache != layer)
        weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = NULL;
      if (layer) {
	weed_layer_unref(layer);
	layer = NULL;
      }
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
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        // TODO: make background thread
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (!mainw->go_away) {
        lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        if (!srcgrp) srcgrp = clone_srcgrp(mainw->current_file, mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        lives_layer_set_srcgrp(layer, srcgrp);
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }

      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height, WEED_PALETTE_RGB24)) {
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (layer) weed_layer_free(layer);
          pthread_mutex_unlock(mutex);
          return;
        }
      } else weed_layer_set_invalid(layer, TRUE);
    }

    if (!start_pixbuf || lives_pixbuf_get_width(start_pixbuf) != width || lives_pixbuf_get_height(start_pixbuf) != height) {
      if (!orig_pixbuf) {
        if (layer) {
          orig_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        } else orig_pixbuf = start_pixbuf;
      }
      if (start_pixbuf && start_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(start_pixbuf)) {
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
      pthread_mutex_unlock(mutex);
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->start_image), start_pixbuf);
      pthread_mutex_lock(mutex);
      //      lives_widget_queue_draw(mainw->start_image);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    //lives_widget_queue_resize(mainw->start_image);
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
    if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, orig_pixbuf))) {
      lives_widget_object_unref(orig_pixbuf);
    }
  } else cache_it = FALSE;

  if (!cache_it) {
    if (mainw->st_fcache && mainw->st_fcache != layer)
      weed_layer_unref(mainw->st_fcache);
    mainw->st_fcache = NULL;
    if (layer) {
      weed_layer_unref(layer);
      layer = NULL;
    }
  } else {
    if (mainw->st_fcache != layer) {
      if (mainw->st_fcache) weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = layer;
      if (!is_virtual_frame(mainw->current_file, frame)) {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          set_md5_for_frame(mainw->current_file, frame, layer);
        }
      // *INDENT-OFF*
    }
    lives_layer_set_frame(layer, xpf);
    lives_layer_set_clip(layer, mainw->current_file);
  }
 }
// *INDENT-ON*
  pthread_mutex_unlock(mutex);
}

void load_end_image(frames_t frame) {
  LiVESPixbuf *end_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  pthread_mutex_t *mutex;
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

  mutex = lives_widget_get_mutex(mainw->end_image);
  pthread_mutex_lock(mutex);

  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->end_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int hsize, vsize;
    get_gui_framesize(&hsize, &vsize);

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
    pthread_mutex_unlock(mutex);
    set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->end_image), mainw->camframe);
    //lives_widget_queue_draw(mainw->end_image);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    pthread_mutex_unlock(mutex);
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->end_image), mainw->imframe);
    } else {
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->end_image), NULL);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->end_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->end_image, 0.4);
    }
    //lives_widget_queue_draw(mainw->end_image);
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
      weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) {
        if (mainw->pr_fcache) {
          mainw->en_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
          weed_layer_ref(mainw->pr_fcache);
          weed_layer_copy(mainw->en_fcache, mainw->pr_fcache);
          copy_md5(mainw->en_fcache, mainw->pr_fcache);
          weed_layer_unref(mainw->pr_fcache);
        }
      }
      if (!mainw->en_fcache) {
        if (mainw->st_fcache) {
          mainw->en_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
          weed_layer_ref(mainw->st_fcache);
          weed_layer_copy(mainw->en_fcache, mainw->st_fcache);
          copy_md5(mainw->en_fcache, mainw->st_fcache);
          weed_layer_unref(mainw->st_fcache);
        }
        tries = 0;
      }
      if (mainw->en_fcache) goto check_encache;
    }
  }
  lives_freep((void **)&fname);

  tc = (frame - 1.) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect && !prefs->letterbox) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (!mainw->go_away) {
        lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        if (!srcgrp) srcgrp = clone_srcgrp(mainw->current_file, mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        lives_layer_set_srcgrp(layer, srcgrp);
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (layer) weed_layer_free(layer);
          pthread_mutex_unlock(mutex);
          return;
        }
      } else weed_layer_set_invalid(layer, TRUE);
    }

    if (!end_pixbuf) end_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(end_pixbuf)) {
      pthread_mutex_unlock(mutex);
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->end_image), end_pixbuf);
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, end_pixbuf)))
        lives_widget_object_unref(end_pixbuf);
      //lives_widget_queue_draw(mainw->end_image);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (mainw->en_fcache && mainw->en_fcache != layer)
        weed_layer_unref(mainw->en_fcache);
      mainw->en_fcache = NULL;
      if (layer) {
        weed_layer_unref(layer);
        layer = NULL;
      }
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
    height = (height >> 1) << 1;

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      //mainw->debug_ptr = layer;
      if (!mainw->go_away) {
        lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        if (!srcgrp) srcgrp = clone_srcgrp(mainw->current_file, mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        lives_layer_set_srcgrp(layer, srcgrp);
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type),
                             tc, width, height, WEED_PALETTE_RGB24)) {
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (layer) weed_layer_free(layer);
          pthread_mutex_unlock(mutex);
          return;
        }
      } else weed_layer_set_invalid(layer, TRUE);
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
      pthread_mutex_unlock(mutex);
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->end_image), end_pixbuf);
      pthread_mutex_lock(mutex);
      //lives_widget_queue_draw(mainw->end_image);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    //lives_widget_queue_resize(mainw->end_image);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } while (rwidth != lives_widget_get_allocation_width(mainw->end_image) ||
           rheight != lives_widget_get_allocation_height(mainw->end_image));
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
    if (mainw->en_fcache && mainw->en_fcache != layer)
      weed_layer_unref(mainw->en_fcache);
    mainw->en_fcache = NULL;
    if (layer) {
      weed_layer_unref(layer);
      layer = NULL;
    }
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
  pthread_mutex_unlock(mutex);

  //mainw->debug_ptr = mainw->end_image;
}


void load_preview_image(boolean update_always) {
  // this is for the sepwin preview
  // update_always==TRUE = update widgets from mainw->preview_frame
  LiVESInterpType interp;
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
  interp = get_interp_value(prefs->pb_quality, TRUE);

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
    set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->preview_image), pixbuf);
    if (pixbuf) lives_widget_object_unref(pixbuf);
    //lives_widget_queue_draw(mainw->preview_image);
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
      set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->preview_image), mainw->imframe);
    } else set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->preview_image), NULL);
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->preview_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->preview_image, 0.4);
    }
    //lives_widget_queue_draw(mainw->preview_image);
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
        weed_layer_unref(mainw->pr_fcache);
        mainw->pr_fcache = NULL;
      }
      if (tries--) {
        if (tries == 1) {
          if (mainw->st_fcache) {
            mainw->pr_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
            weed_layer_ref(mainw->st_fcache);
            weed_layer_copy(mainw->pr_fcache, mainw->st_fcache);
            copy_md5(mainw->pr_fcache, mainw->st_fcache);
            weed_layer_unref(mainw->st_fcache);
          }
        }
        if (!mainw->pr_fcache) {
          if (mainw->en_fcache) {
            mainw->pr_fcache = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
            weed_layer_ref(mainw->en_fcache);
            weed_layer_copy(mainw->pr_fcache, mainw->en_fcache);
            copy_md5(mainw->pr_fcache, mainw->en_fcache);
            weed_layer_unref(mainw->en_fcache);
          }
          tries = 0;
        }
        if (mainw->pr_fcache) goto check_prcache;
      }
    }

    lives_freep((void **)&fname);

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE &&
        is_virtual_frame(mainw->current_file, mainw->preview_frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
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
      layer = lives_layer_new_for_frame(mainw->current_file, mainw->preview_frame);
      if (!mainw->go_away) {
        lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        if (!srcgrp) srcgrp = clone_srcgrp(mainw->current_file, mainw->current_file, -1, SRC_PURPOSE_THUMBNAIL);
        lives_layer_set_srcgrp(layer, srcgrp);
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      }
      tc = ((mainw->preview_frame - 1.)) / cfile->fps * TICKS_PER_SECOND;
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height, WEED_PALETTE_RGB24)) {
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (layer) weed_layer_free(layer);
          return;
        }
      } else weed_layer_set_invalid(layer, TRUE);
    }

    if (!pixbuf) {
      if (layer) pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
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
        set_drawing_area_from_pixbuf(LIVES_DRAWING_AREA(mainw->preview_image), pr_pixbuf);
        if (pr_pixbuf != pixbuf) lives_widget_object_unref(pr_pixbuf);
        if (!layer || !cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, pixbuf)))
          lives_widget_object_unref(pixbuf);
      } else cache_it = FALSE;
    } else cache_it = FALSE;
  }

  if (!cache_it) {
    if (layer) {
      weed_layer_unref(layer);
      layer = NULL;
    }
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
      //lives_widget_queue_draw(mainw->eventbox2);
      break;

    case PRV_START:
      if (mainw->st_fcache) weed_layer_unref(mainw->st_fcache);
      mainw->st_fcache = mainw->pr_fcache;
      if (cfile->start != mainw->preview_frame) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->preview_frame);
        lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_start));
        get_play_times();
      }
      break;

    case PRV_END:
      if (mainw->en_fcache) weed_layer_unref(mainw->en_fcache);
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
    //lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);
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
  lives_clip_t *scrapfile = RETURN_VALID_CLIP(mainw->scrap_file);
  int fd;
  if (!scrapfile) return FALSE;

  if (!get_primary_inst(scrapfile)) {
    oname = make_image_file_name(scrapfile, 1, LIVES_FILE_EXT_SCRAP);
    fd = lives_open_buffered_rdonly(oname);
    lives_free(oname);
    if (fd < 0) return FALSE;
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    add_primary_inst(mainw->scrap_file, NULL,
                     LIVES_INT_TO_POINTER(fd), LIVES_SRC_TYPE_FILE_BUFF);
  } else fd = LIVES_POINTER_TO_INT(get_primary_inst(scrapfile));

  if (frame < 0 || !layer) return TRUE; /// just open fd

  if (!weed_plant_deserialise(fd, NULL, layer)) {
    //g_print("bad scrapfile frame\n");
    return FALSE;
  }
  return TRUE;
}


static boolean sf_writeable = TRUE;

static int64_t _save_to_scrap_file(weed_layer_t **layerptr) {
  // returns frame number
  // dump the raw layer (frame) data to disk

  // TODO: run as bg thread

  size_t pdata_size;
  weed_layer_t *layer = *layerptr;
  lives_clip_t *scrapfile = mainw->files[mainw->scrap_file];
  int fd;

  weed_layer_ref(layer);

  if (!get_primary_src(mainw->scrap_file)) {
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

    add_primary_inst(mainw->scrap_file, NULL,
                     LIVES_INT_TO_POINTER(fd), LIVES_SRC_TYPE_FILE_BUFF);

#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
  } else fd = LIVES_POINTER_TO_INT(get_primary_inst(scrapfile));

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

static weed_layer_t *orig_layer = NULL;

int save_to_scrap_file(weed_layer_t *layer) {
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

  if (!orig_layer) orig_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

  if (!mainw->scrap_file_proc) {
    mainw->scrap_file_proc =
      lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED, (lives_funcptr_t)_save_to_scrap_file,
                               WEED_SEED_INT64, "v", &orig_layer);
  }

  checked_disk = FALSE;
  check_for_disk_space(FALSE);

  // if saver thrd still busy, skip this frame
  // TODO - should buffer a few frames
  if (!lives_proc_thread_is_unqueued(mainw->scrap_file_proc)
      && !lives_proc_thread_check_finished(mainw->scrap_file_proc)) {
    return scrapfile->frames;
  }

  if (lives_proc_thread_check_finished(mainw->scrap_file_proc))
    scrapfile->f_size += lives_proc_thread_join_int64(mainw->scrap_file_proc);

  weed_layer_copy(orig_layer, layer);
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
  if (orig_layer) {
    weed_layer_unref(orig_layer);
    orig_layer = NULL;
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
  weed_set_int_array(layer, "loaded_size", 2, nsize);
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
  lives_clip_t *sfile;
  png_structp png_ptr;
  png_infop info_ptr;
  double file_gamma;

#ifndef PNG_BIO
  FILE *fp = fdopen(fd, "rb");
  size_t bsize = fread(ibuff, 1, 8, fp);
  boolean is_png = TRUE;
  uint8_t ibuff[8];
#endif

  uint8_t **row_ptrs, *ptr;
  png_uint_32 xwidth, xheight;

  boolean is16bit = FALSE;
  int gval;
  int width, height, rowstride;
  int color_type, bit_depth, clip, layer_gamma;
  int flags, privflags;

  clip = lives_layer_get_clip(layer);
  sfile = RETURN_VALID_CLIP(clip);

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

    if (1) {
      int nsize[2];
      // set "natural_size" in case a filter needs it
      //g_print("setnatsize %p\n", layer);
      nsize[0] = xwidth;
      nsize[1] = xheight;
      weed_set_int_array(layer, "loaded_size", 2, nsize);
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
    if (layer_gamma != WEED_GAMMA_FILE) {
      lives_clip_src_t *mysrc = NULL;
      layer_gamma = WEED_GAMMA_FILE;
      weed_layer_set_gamma(layer, layer_gamma);
      mysrc = get_clip_src(NULL, clip, ACTOR_UID_IMG_DECODER_PNG,
                           LIVES_SRC_TYPE_IMAGE, NULL, NULL);
      if (mysrc) {
        mysrc->gamma_type = layer_gamma;
        mysrc->file_gamma = file_gamma;
        update_gamma_in_all_srcgrps(clip, mysrc);
      }
    }
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

  if (tpalette != WEED_PALETTE_NONE && tpalette != WEED_PALETTE_ANY) {
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
      if (sfile) sfile->bpp = 32;
      weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
    } else {
      if (sfile) sfile->bpp = 24;
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
  layer_gamma = weed_layer_get_gamma(layer);

  if (gval == PNG_INFO_gAMA) {
    /// if gAMA is set, then we need to convert to *linear* using the file gamma
    if (layer_gamma != WEED_GAMMA_FILE) {
      lives_clip_src_t *mysrc = NULL;
      layer_gamma = WEED_GAMMA_FILE;
      weed_layer_set_gamma(layer, layer_gamma);
      mysrc = get_clip_src(NULL, clip, ACTOR_UID_IMG_DECODER_PNG,
                           LIVES_SRC_TYPE_IMAGE, NULL, NULL);
      if (mysrc) {
        mysrc->gamma_type = layer_gamma;
        mysrc->file_gamma = file_gamma;
        update_gamma_in_all_srcgrps(clip, mysrc);
      }
    }
  } else {
    if (layer_gamma != WEED_GAMMA_SRGB) {
      lives_clip_src_t *mysrc = NULL;
      layer_gamma = WEED_GAMMA_SRGB;
      weed_layer_set_gamma(layer, layer_gamma);
      mysrc = get_clip_src(NULL, clip, ACTOR_UID_IMG_DECODER_PNG,
                           LIVES_SRC_TYPE_IMAGE, NULL, NULL);
      if (mysrc) {
        mysrc->gamma_type = layer_gamma;
        update_gamma_in_all_srcgrps(clip, mysrc);
      }
    }
  }
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
      lives_proc_thread_unref(resthread);
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


boolean weed_layer_create_from_file_progressive(weed_layer_t *layer, const char *img_ext) {//, const char *fname, int width,
  //int height, int tpalette, const char *img_ext) {
  LiVESPixbuf *pixbuf = NULL;
  LiVESError *gerror = NULL;
  lives_clip_t *sfile;
  char *shmpath = NULL;
  char *fname;
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
  int clipno = lives_layer_get_clip(layer);
  frames_t frame = lives_layer_get_frame(layer);
  int width = weed_layer_get_width_pixels(layer);
  int height = weed_layer_get_height(layer);
  int tpalette = weed_layer_get_palette(layer);
  int privflags;

  sfile = RETURN_VALID_CLIP(clipno);
  if (!sfile) return FALSE;

  if (!img_ext || !*img_ext) img_ext = get_image_ext_for_type(sfile->img_type);
  fname = make_image_file_name(sfile, frame, img_ext);

  privflags = weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);
  if (privflags == LIVES_LAYER_GET_SIZE_ONLY) {
#ifdef GUI_GTK
    /* struct GdkPixbufFormat { */
    /*   gchar* name; */
    /*   GdkPixbufModulePattern* signature; */
    /*   gchar* domain; */
    /*   gchar* description; */
    /*   gchar** mime_types; */
    /*   gchar** extensions; */
    /*   guint32 flags; */
    /*   gboolean disabled; */
    /*   gchar* license; */
    /* } */
    GdkPixbufFormat *pixform = gdk_pixbuf_get_file_info(fname, &width, &height);
    if (pixform) {
      weed_layer_set_size(layer, width, height);
      if (fname) lives_free(fname);
      return TRUE;
    }
#endif
  }

#ifndef VALGRIND_ON
  if (!mainw->debug)
    if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) is_png = TRUE;
#endif

#ifdef PNG_BIO
  fd = lives_open_buffered_rdonly(fname);
  if (fd < 0) BREAK_ME(fname);
  if (fd < 0) {
    if (fname) lives_free(fname);
    return FALSE;
  }
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
  if (fd < 0) {
    if (fname) lives_free(fname);
    return FALSE;
  }
#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif

  xxwidth = width;
  xxheight = height;

  if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) {
    ret = layer_from_png(fd, layer, width, height, tpalette, TRUE);
    goto fndone;
  }
#ifdef GUI_GTK
  else if (!strcmp(img_ext, LIVES_FILE_EXT_JPG))
    pbload = gdk_pixbuf_loader_new_with_type(LIVES_IMAGE_TYPE_JPEG, &gerror);
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

    if (fd < 0) {
      if (fname) lives_free(fname);
      return FALSE;
    }
#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif
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
    if (sfile) sfile->bpp = 32;
  } else {
    weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
    if (sfile) sfile->bpp = 24;
  }
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
    if (fname) lives_rm(fname);
    lives_rmdir_with_parents(shmpath);
  }
  if (fname) lives_free(fname);
  return ret;
}


void md5_frame(weed_layer_t *layer) {
  // TODO - realloc array[0] when frame count changes
  // TODO - combine with othe md5 func
  if (layer) {
    int clip = lives_layer_get_clip(layer);
    lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clip);
    if (sfile) {
      int nplanes;
      int frame = lives_layer_get_frame(layer);
      int height = weed_layer_get_height(layer);
      int *rowstrides = weed_layer_get_rowstrides(layer, &nplanes);
      frames_t xframe = get_indexed_frame(clip, frame);
      int whichmd5 = 1;
      size_t pd_size;
      void *pdata;
      void *md5sum;

      if (xframe < 0) {
        xframe = - xframe - 1;
        whichmd5 = 0;
      }
      if (!sfile->frame_md5s[whichmd5]) {
        if (whichmd5 == 0) {
          sfile->frame_md5s[0] = (char **)lives_calloc(sfile->frames, sizeof(char *));
        } else if (whichmd5 == 1) {
          lives_clip_data_t *cdata = get_clip_cdata(clip);
          if (cdata) sfile->frame_md5s[1] = (char **)lives_calloc(cdata->nframes, sizeof(char *));
          if (1) {
            int width = weed_layer_get_width(layer);
            int pal = weed_layer_get_palette(layer);
            int xgamma = weed_layer_get_gamma(layer);
            weed_layer_t *blayer = create_blank_layer_precise(width, height, rowstrides,
                                   xgamma, pal);
            pd_size = rowstrides[0] * height;
            pdata = weed_layer_get_pixel_data(blayer);
            md5sum = (void *)tinymd5(pdata, pd_size);
            lives_memcpy(sfile->blank_md5s[1], md5sum, MD5_SIZE);
            lives_free(md5sum);
            weed_layer_free(blayer);
          }
        }
      }
      if (!sfile->frame_md5s[whichmd5][xframe]) {
        pd_size = rowstrides[0] * height;
        pdata = weed_layer_get_pixel_data(layer);
        md5sum = (void *)tinymd5(pdata, pd_size);
        sfile->frame_md5s[whichmd5][xframe] = (char *)lives_malloc(MD5_SIZE);
        lives_memcpy(sfile->frame_md5s[whichmd5][xframe], md5sum, MD5_SIZE);

        if (whichmd5 == 0) {
          if (*sfile->blank_md5s[1] && !*sfile->blank_md5s[0]
              && !lives_memcmp(md5sum, sfile->blank_md5s[0], MD5_SIZE)) {
            set_md5_for_frame(clip, xframe, layer);
            lives_memcpy(sfile->blank_md5s[1],
                         weed_get_voidptr_value(layer, LIVES_LEAF_MD5SUM, NULL),
                         MD5_SIZE);
          }
        }
        lives_free(md5sum);
      }
      lives_free(rowstrides);
    }
  }
}


// callers: pull_frame, pth_thread. load_start_image, load_end_image
boolean pull_frame_at_size(weed_layer_t *layer, const char *image_ext, weed_timecode_t tc, int width, int height,
                           int target_palette) {
  // SYNCRONOUS FUNC if called directly or via pull_frame()
  //
  // ASYNC func if called via pull_frame-threaded()
  //
  // pull a frame from an external source into a layer
  // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
  // tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
  // image_ext is used if the source is an image file (eg. "jpg" or "png")
  // width and height are hints only, the caller should resize if necessary
  // target_palette is also a hint

  // if we pull from a decoder plugin, then we may also deinterlace
  //
  // if the planner is running, then there is no need to call this,
  //simply set the desired frame number in mainw-<frame_index[track]
  //
  RECURSE_GUARD_START;
  lives_clip_t *sfile = NULL;
  boolean is_thread = FALSE;

  frames_t frame;
  int clip, clip_type, track;
  int lstatus = LAYER_STATUS_NONE;
  int errpt = 0;

  ____FUNC_ENTRY____(pull_frame_at_size, "b", "vvIiii");

  if (!layer) {
    errpt = 1;
    goto fail;
  }

  track = lives_layer_get_track(layer);
  clip = lives_layer_get_clip(layer);
  frame = lives_layer_get_frame(layer);

  if (!weed_layer_check_valid(layer)) {
    g_printerr("layer on track %d (clip %d, frame %d) became invalid during loading\n",
               track, clip, frame);
    errpt = 2;
    goto fail;
  }

  //weed_layer_pixel_data_free(layer);
  if (width && height) weed_layer_set_size(layer, width, height);

  if (weed_layer_get_gamma(layer) == WEED_GAMMA_UNKNOWN)
    // the default unless overridden
    weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

  if (lives_layer_get_proc_thread(layer)) is_thread = TRUE;

  sfile = RETURN_VALID_CLIP(clip);

  if (!sfile) {
    if (target_palette != WEED_PALETTE_NONE && target_palette != WEED_PALETTE_ANY)
      weed_layer_set_palette(layer, target_palette);
    errpt = 3;
    goto fail;
  }

  lives_layer_set_status(layer, LAYER_STATUS_LOADING);

  clip_type = sfile->clip_type;

retry:

  switch (clip_type) {
  case CLIP_TYPE_NULL_VIDEO:
    errpt = 4;
    goto fail;
  case CLIP_TYPE_DISK:
  case CLIP_TYPE_FILE:
    // frame number can be 0 during rendering
    if (frame == 0) {
      errpt = 5;
      goto fail;
    }
    if (clip == mainw->scrap_file) {
      boolean res = load_from_scrap_file(layer, frame);
      if (res) goto success;
      errpt = 6;
      goto fail;
    } else {
      frames_t xframe;
      frame = clamp_frame(clip, frame);
      xframe = -frame;
      pthread_mutex_lock(&sfile->frame_index_mutex);
      if (LIVES_IS_PLAYING && prefs->skip_rpts && sfile->alt_frame_index)
        xframe = get_alt_indexed_frame(clip, frame);
      else if (sfile->frame_index) xframe = get_indexed_frame(clip, frame);

      pthread_mutex_unlock(&sfile->frame_index_mutex);
      if (xframe >= 0) {
        if (prefs->vj_mode && mainw->loop_locked && mainw->ping_pong && sfile->pb_fps >= 0.) {
          LiVESPixbuf *pixbuf = NULL;
          RECURSE_GUARD_ARM;

          pthread_mutex_lock(&sfile->frame_index_mutex);
          virtual_to_images(clip, xframe, xframe, FALSE, &pixbuf);
          pthread_mutex_unlock(&sfile->frame_index_mutex);

          RECURSE_GUARD_END;
          if (pixbuf) {
            if (!pixbuf_to_layer(layer, pixbuf)) {
              lives_widget_object_unref(pixbuf);
            } else if (LIVES_IS_PLAYING && prefs->skip_rpts) md5_frame(layer);
            break;
          }
        }
        if (1) {
          // pull frame from video clip
          ///
#ifdef USE_REC_RS
          int nplanes;
#endif
          lives_decoder_t *dplug = NULL;
          const lives_decoder_sys_t *dpsys;
          lives_clipsrc_group_t *srcgrp;
          void **pixel_data;
          int *rowstrides;
          uint64_t dec_uid;
          int cpal, layer_gamma;
          boolean res = TRUE;

          srcgrp = lives_layer_get_srcgrp(layer);
          if (!srcgrp) {
            if (track >= 0 && track < mainw->num_tracks) {
              srcgrp = mainw->track_sources[track];
              lives_layer_set_srcgrp(layer, srcgrp);
            }
          }

          if (srcgrp) {
            lives_clip_src_t *mysrc = get_clip_src(srcgrp, clip, 0, LIVES_SRC_TYPE_DECODER, NULL, NULL);
            if (mysrc) dplug = (lives_decoder_t *)mysrc->actor;
          }

          if (!dplug) {
            if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_DECODER)
              dplug = (lives_decoder_t *)get_primary_actor(sfile);
          }

          if (!dplug || !dplug->cdata || xframe >= dplug->cdata->nframes) {
            if (dplug && dplug->cdata && xframe >= dplug->cdata->nframes) {
              if (frame > 1 && frame <= dplug->cdata->nframes
                  && get_indexed_frame(clip, frame - 1) == frame - 1) xframe = frame;
            } else {
              errpt = 7;
              goto fail;
            }
          }

          dpsys = (const lives_decoder_sys_t *)dplug->dpsys;
          dec_uid = dpsys->id->uid;

          cpal = dplug->cdata->current_palette;

          width = dplug->cdata->width / weed_palette_get_pixels_per_macropixel(cpal);
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
          if (!dplug->cdata->rec_rowstrides)
            dplug->cdata->rec_rowstrides = lives_calloc(nplanes, sizint);
          else {
            if (dplug->cdata->rec_rowstrides[0]) {
              weed_layer_set_rowstrides(layer, dplug->cdata->rec_rowstrides, nplanes);
              weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);
              lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
            }
          }
#endif
          if (create_empty_pixel_data(layer, TRUE, TRUE)) {
            if (!weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, NULL))
              BREAK_ME("non contig");
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);
#endif
            pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);
          } else {
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);
#endif
            errpt = 8;
            goto fail;
          }

          if (!pixel_data || !pixel_data[0]) {
            char *msg = lives_strdup_printf("NULL pixel data for layer size %d X %d, palette %s\n", width, height,
                                            weed_palette_get_name_full(dplug->cdata->current_palette,
                                                dplug->cdata->YUV_clamping,
                                                dplug->cdata->YUV_subspace));
            LIVES_WARN(msg);
            lives_free(msg);
            errpt = 9;
            goto fail;
          }

          rowstrides = weed_layer_get_rowstrides(layer, NULL);
          pthread_mutex_lock(&dplug->mutex);

          lock_layer_status(layer);
          lstatus = _lives_layer_get_status(layer);
          if (lstatus != LAYER_STATUS_NONE && lstatus != LAYER_STATUS_LOADING)
            _lives_layer_set_status(layer, LAYER_STATUS_LOADING);
          unlock_layer_status(layer);

          if (!(*dplug->dpsys->get_frame)(dplug->cdata, (int64_t)xframe, rowstrides, sfile->vsize, pixel_data)) {
            pthread_mutex_unlock(&dplug->mutex);
            if (prefs->show_dev_opts) {
              g_print("Error loading frame %d (index value %d)\n", frame,
                      xframe);
              BREAK_ME("load error");
            }

#ifdef USE_REC_RS
            if (dplug->cdata->rec_rowstrides)
              lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
#endif
            if (frame == cfile->frames) {
              if (prefs->repl_missing_frames) {
                g_print("final frame missing for clip %d\n", clip);
                g_print("replacing with blank image\n");
                THREAD_INTENTION = OBJ_INTENTION_REPLACE;
                insert_blank_frames(clip, 1, frame - 1, WEED_PALETTE_ANY);
                THREAD_INTENTION = OBJ_INTENTION_NONE;
                if (get_indexed_frame(clip, frame) < 0) goto retry;
              }
            }

            // if get_frame fails, return a black frame
            errpt = 10;
            goto fail;
          } else {
            pthread_mutex_unlock(&dplug->mutex);

            propogate_timing_data(dplug);
            if (LIVES_IS_PLAYING && prefs->skip_rpts) md5_frame(layer);
          }

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
            layer_gamma = weed_layer_get_gamma(layer);
            // get_frame may now update YUV_clamping, YUV_sampling, YUV_subspace
            if (weed_palette_is_yuv(dplug->cdata->current_palette)) {
              weed_layer_set_palette_yuv(layer, dplug->cdata->current_palette,
                                         dplug->cdata->YUV_clamping,
                                         dplug->cdata->YUV_sampling,
                                         dplug->cdata->YUV_subspace);
              if (prefs->apply_gamma && prefs->pb_quality != PB_QUALITY_LOW) {
                if (layer_gamma != WEED_GAMMA_BT709) {
                  lives_clip_src_t *mysrc = NULL;
                  layer_gamma = WEED_GAMMA_BT709;
                  weed_layer_set_gamma(layer, layer_gamma);
                  mysrc = get_clip_src(NULL, clip, dec_uid, LIVES_SRC_TYPE_DECODER, NULL, NULL);
                  weed_layer_set_yuv_subspace(layer, WEED_YUV_SUBSPACE_BT709);
                  if (mysrc) {
                    mysrc->gamma_type = layer_gamma;
                    update_gamma_in_all_srcgrps(clip, mysrc);
                  }
                }
                if (weed_layer_get_yuv_subspace(layer) == WEED_YUV_SUBSPACE_BT709) {
                  if (layer_gamma != WEED_GAMMA_BT709) {
                    lives_clip_src_t *mysrc = NULL;
                    layer_gamma = WEED_GAMMA_BT709;
                    weed_layer_set_gamma(layer, layer_gamma);
                    mysrc = get_clip_src(NULL, clip, dec_uid, LIVES_SRC_TYPE_DECODER, NULL, NULL);
                    weed_layer_set_yuv_subspace(layer, WEED_YUV_SUBSPACE_BT709);
                    if (mysrc) {
                      mysrc->gamma_type = layer_gamma;
                      update_gamma_in_all_srcgrps(clip, mysrc);
                    }
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
            if (!res) {
              errpt = 11;
              goto fail;
            }
            goto success;
          }
        }
      } else {
        // pull frame from decoded images
        int64_t timex;
        double img_decode_time;
        boolean ret;

        pthread_mutex_unlock(&sfile->frame_index_mutex);
        xframe = -xframe;

        timex = lives_get_session_time();

        ret = weed_layer_create_from_file_progressive(layer, image_ext);
        img_decode_time = lives_get_session_time() - timex;

        if (!ret) {
          errpt = 12;
          goto fail;
        }
        if (!sfile->img_decode_time) sfile->img_decode_time = img_decode_time;
        else sfile->img_decode_time = (sfile->img_decode_time * 3 + img_decode_time) / 4.;
        if (LIVES_IS_PLAYING && prefs->skip_rpts) md5_frame(layer);
      }
    }
    goto success;
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
      } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, TRUE);
    }
    goto success;
#endif
  case CLIP_TYPE_LIVES2LIVES:
    weed_layer_set_from_lives2lives(layer, clip, (lives_vstream_t *)get_primary_actor(sfile));
    goto success;
  case CLIP_TYPE_GENERATOR: {
    // special handling for clips where host controls size
    // Note: vlayer is actually the out channel of the generator, so we should
    // never free it !
    lives_filter_error_t res = LIVES_RESULT_FAIL;
    weed_plant_t *inst = (weed_instance_t *)get_primary_inst(sfile);
    if (!inst) {
      errpt = 13;
      goto fail;
    }
    weed_instance_ref(inst);

    int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
    filter_mutex_lock(key);

    if (IS_VALID_CLIP(clip))
      res = lives_layer_fill_from_generator(layer, inst, tc);

    filter_mutex_unlock(key);
    weed_instance_unref(inst);

    if (res != FILTER_SUCCESS) {
      errpt = 14;
      goto fail;
    }
    goto success;
  }
  default: goto fail;
  }

success:

  if (!is_thread) {
    if (weed_get_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, NULL) == WEED_TRUE) {
      weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_HOST_TC, NULL);
      deinterlace_frame(layer, tc);
      weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_FALSE);
    }

    // render subtitles from file
    if (prefs->show_subtitles && sfile->subt && sfile->subt->tfile > 0) {
      // TODO - should subs be in chronological order, or in reordered (alt_frame_index / frame_index order)
      double xtime = (double)(frame - 1) / sfile->fps;
      render_subs_from_file(sfile, xtime, layer);
    }
    lives_layer_set_status(layer, LAYER_STATUS_READY);
  }

  ____FUNC_EXIT_VAL____("b", TRUE);
  return TRUE;

fail:
  g_print("FAILED to load frame error was %d\n", errpt);
  lock_layer_status(layer);
  lstatus = _lives_layer_get_status(layer);
  if (lstatus != LAYER_STATUS_NONE) _weed_layer_set_invalid(layer, TRUE);
  unlock_layer_status(layer);

  weed_layer_pixel_data_free(layer);
  create_blank_layer(layer, image_ext, width, height, target_palette);

  ____FUNC_EXIT_VAL____("b", FALSE);
  return FALSE;
}


/**
   @brief pull a frame from an external source into a layer
   the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
   tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
   image_ext is used if the source is an image file (eg. "jpg" or "png")
*/
LIVES_GLOBAL_INLINE boolean pull_frame(weed_layer_t *layer, const char *img_ext, weed_timecode_t tc) {
  int clip = lives_layer_get_clip(layer);
  lives_clip_t *sfile = RETURN_VALID_CLIP(clip);
  if (sfile) {
    if (!img_ext) img_ext = get_image_ext_for_type(sfile->img_type);
    return pull_frame_at_size(layer, img_ext, tc, 0, 0, WEED_PALETTE_ANY);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE lives_result_t is_layer_ready(weed_layer_t *layer) {
  if (!layer) return LIVES_RESULT_ERROR;
  if (!weed_layer_check_valid(layer)) return LIVES_RESULT_INVALID;
  if (lives_layer_get_status(layer) == LAYER_STATUS_READY) return LIVES_RESULT_SUCCESS;
  return LIVES_RESULT_FAIL;
}


/**
   @brief block until layer pixel_data is ready.
   This function should always be called for threaded layers,
   prior to freeing the layer, reading it's  properties, pixel dat
*/

lives_result_t wait_layer_ready(weed_layer_t *layer, boolean allow_loaded) {
  lives_result_t res = LIVES_RESULT_SUCCESS;
  if (!layer) return LIVES_RESULT_ERROR;
  if (lives_layer_get_status(layer) != LAYER_STATUS_NONE) {
    GET_PROC_THREAD_SELF(self);
    lives_proc_thread_t lpt = NULL;
    // spin while state is queued, prepared, loading, loaded, converting
    // if state is invalid, cancel any thread running
    //   completed - SUCCESS
    lpt = lives_layer_get_proc_thread(layer);

    if (lives_proc_thread_ref(lpt) > 1) {
      if (lpt == self) {
        lives_proc_thread_unref(lpt);
        return LIVES_RESULT_SUCCESS;
      }
      while (1) {
        int lstatus;
        if (!lives_layer_get_proc_thread(layer)) {
          lock_layer_status(layer);
          lstatus = _lives_layer_get_status(layer);
          if (lstatus == LAYER_STATUS_READY ||
              (allow_loaded && lstatus == LAYER_STATUS_LOADED)) {
            unlock_layer_status(layer);
            break;
          }
          _weed_layer_set_invalid(layer, TRUE);
          unlock_layer_status(layer);
          res = LIVES_RESULT_FAIL;
          break;
        }
        lives_millisleep;
      }
      lives_proc_thread_unref(lpt);
    }
  }
  return res;
}


static void pft_thread(weed_layer_t *layer, const char *img_ext) {
  weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_TIMECODE, NULL);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  pull_frame_at_size(layer, img_ext, tc, width, height, WEED_PALETTE_ANY);
}


lives_result_t pull_frame_threaded(weed_layer_t *layer, int width, int height) {
  // ASYNC_FUNC
  // pull a frame from an external source into a layer
  // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer

  int clipno = lives_layer_get_clip(layer);
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);

  if (sfile) {
#ifndef NO_FRAME_THREAD
    lives_proc_thread_t lpt;
#endif
    weed_timecode_t tc = lives_get_session_ticks();
    ticks_t *timing_data;
    const char *img_ext = get_image_ext_for_type(sfile->img_type);

    lives_layer_set_status(layer, LAYER_STATUS_QUEUED);
    weed_layer_pixel_data_free(layer);
    lock_layer_status(layer);

    timing_data = _get_layer_timing(layer);
    tc = timing_data[LAYER_STATUS_TREF];
    if (!tc) tc = timing_data[LAYER_STATUS_TREF] = timing_data[LAYER_STATUS_QUEUED];
    _set_layer_timing(layer, timing_data);
    lives_free(timing_data);

    unlock_layer_status(layer);
    weed_set_int64_value(layer, WEED_LEAF_HOST_TC, tc);
    weed_layer_set_size(layer, width, height);

#ifdef NO_FRAME_THREAD
    if (!pull_fram(layer, tc)) return LIVES_RESULT_ERROR;
#else

    lpt = lives_proc_thread_create(LIVES_THRDATTR_PRIORITY | LIVES_THRDATTR_NO_GUI
                                   | LIVES_THRDATTR_START_UNQUEUED, (lives_funcptr_t)pft_thread,
                                   0, "vs", layer, img_ext);
    lives_layer_async_auto(layer, lpt);

#endif
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
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

  if (strcmp(image_ext, LIVES_FILE_EXT_PNG)) palette = WEED_PALETTE_RGB24;
  else palette = WEED_PALETTE_ANY;

  if (pull_frame_at_size(layer, image_ext, tc, width, height, palette)) {
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
  boolean retval = TRUE;
  int fd;

  // CALLER should check for errors
  // fname should be in local charset

  if (!LIVES_IS_PIXBUF(pixbuf)) {
    /// invalid pixbuf, we will save a blank image
    const char *img_ext = get_image_ext_for_type(imgtype);
    weed_layer_t  *layer = create_blank_layer(NULL, img_ext, width, height, WEED_PALETTE_ANY);
    pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);
    weed_layer_unref(layer);
    retval = FALSE;
  }

  fd = lives_open3(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

  lives_microsleep_until_zero_timeout(LIVES_SHORTEST_TIMEOUT, flock(fd, LOCK_EX));
  return_val_if_triggered(FALSE);

  if (imgtype == IMG_TYPE_JPEG) {
    char *qstr = lives_strdup_printf("%d", quality);
#ifdef GUI_GTK
    gdk_pixbuf_save(pixbuf, fname, LIVES_IMAGE_TYPE_JPEG, gerrorptr, "quality", qstr, NULL);
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
                       saveargs->width, saveargs->height, &saveargs->error);
}

