// layers.c
// LiVES
// (c) G. Finch 2004 - 2022 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// layers - an implementation of conatianers for idnvidual video frames and / or audio packets

/*
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
*/

#include "main.h"
#include "effects-weed.h"
#include "effects.h"
#include "cvirtual.h"

LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_new(int layer_type) {
  weed_layer_t *layer = weed_plant_new(WEED_PLANT_LAYER);
  weed_set_int_value(layer, WEED_LEAF_LAYER_TYPE, layer_type);
  weed_add_refcounter(layer);
  if (layer_type == WEED_LAYER_TYPE_VIDEO) {
    LIVES_CALLOC_TYPE(pthread_mutex_t, copylist_mutex, 1);
    pthread_mutex_init(copylist_mutex, NULL);
    weed_set_voidptr_value(layer, "copylist_mutex", copylist_mutex);
    g_print("new layer %p\n", layer);
  }
  return layer;
}


LIVES_GLOBAL_INLINE boolean lives_layer_plan_controlled(lives_layer_t *layer) {
  if (!layer) return FALSE;
  return weed_get_boolean_value(layer, LIVES_LEAF_PLAN_CONTROL, NULL);
}

LIVES_GLOBAL_INLINE void lives_layer_set_frame(weed_layer_t *layer, frames_t frame) {
  // TODO -> int64
  weed_set_int_value(layer, WEED_LEAF_FRAME, frame);
}


LIVES_GLOBAL_INLINE void lives_layer_set_clip(weed_layer_t *layer, int clip) {
  weed_set_int_value(layer, WEED_LEAF_CLIP, clip);
}


LIVES_GLOBAL_INLINE void lives_layer_set_track(weed_layer_t *layer, int track) {
  weed_set_int_value(layer, LIVES_LEAF_TRACK, track);
}


LIVES_GLOBAL_INLINE weed_layer_t *lives_layer_new_for_frame(int clip, frames_t frame) {
  // create a layer ready to receive a frame from a clip
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  lives_layer_set_clip(layer, clip);
  lives_layer_set_frame(layer, frame);
  return layer;
}


LIVES_GLOBAL_INLINE void lives_layer_set_srcgrp(weed_layer_t *layer, lives_clipsrc_group_t *srcgrp) {
  if (layer) {
    weed_set_voidptr_value(layer, LIVES_LEAF_SRCGRP, srcgrp);
    if (srcgrp) srcgrp->layer = layer;
  }
}


LIVES_GLOBAL_INLINE void lives_layer_unset_srcgrp(weed_layer_t *layer) {
  if (layer) {
    lives_clipsrc_group_t *srcgrp = weed_get_voidptr_value(layer, LIVES_LEAF_SRCGRP, NULL);
    if (srcgrp) {
      srcgrp->layer = NULL;
      weed_set_voidptr_value(layer, LIVES_LEAF_SRCGRP, NULL);
    }
  }
}


LIVES_GLOBAL_INLINE lives_clipsrc_group_t *lives_layer_get_srcgrp(weed_layer_t *layer) {
  return layer ? weed_get_voidptr_value(layer, LIVES_LEAF_SRCGRP, NULL) : NULL;
}


void lives_layer_copy_metadata(weed_layer_t *dest, weed_layer_t *src) {
  // copy width, height, clip, frame, full palette, etc
  if (src && dest) {
    lives_leaf_dup(dest, src, WEED_LEAF_CURRENT_PALETTE);
    lives_leaf_dup(dest, src, WEED_LEAF_WIDTH);
    lives_leaf_dup(dest, src, WEED_LEAF_HEIGHT);

    // copy rowstrides, though these may get overwritten if we create new pixel data
    lives_leaf_dup(dest, src, WEED_LEAF_ROWSTRIDES);

    lives_leaf_copy_or_delete(dest, WEED_LEAF_CLIP, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_FRAME, src);

    lives_leaf_copy_or_delete(dest, WEED_LEAF_GAMMA_TYPE, src);
    lives_leaf_copy_or_delete(dest, LIVES_LEAF_HOST_FLAGS, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_CLAMPING, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_SUBSPACE, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_SAMPLING, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_PAR, src);
  }
}


/**
   @brief fills layer with default values.

   If either width or height are zero, then dimensions will be taken from the layer or
   defaults used
   if layer has a palette set, that will be maintained, else it will be set to target_palette
   if target palette is WEED_PALETTE_NONE then default will be set depending on image_ext
   if this is "jpg" then it will be RGB24, otherwise RGBA32
   finally we create the pixel data for layer */
static weed_layer_t *_create_blank_layer(weed_layer_t *layer, const char *image_ext,
    int width, int height, int *rowstrides, int tgt_gamma, int target_palette) {
  lives_clip_t *sfile = NULL;
  if (!layer) layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  else {
    if (!width) width = weed_layer_get_width(layer);
    if (!height) height = weed_layer_get_height(layer);
    if (!width || !height) {
      int clip = lives_layer_get_clip(layer);
      sfile = RETURN_VALID_CLIP(clip);
      if (sfile) {
        width = sfile->hsize;
        height = sfile->vsize;
      }
    }
  }

  if (!width) width = DEF_FRAME_HSIZE_UNSCALED;
  if (!height) height = DEF_FRAME_VSIZE_UNSCALED;

  weed_layer_set_size(layer, width, height);

  if (weed_layer_get_palette(layer) == WEED_PALETTE_NONE) {
    if (target_palette != WEED_PALETTE_NONE && target_palette != WEED_PALETTE_ANY)
      weed_layer_set_palette(layer, target_palette);
    else {
      if (!image_ext && sfile && sfile->bpp == 32)
        weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
      else  {
        if (!image_ext || !strcmp(image_ext, LIVES_FILE_EXT_JPG))
          weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
        else weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
      }
    }
  }

  if (rowstrides) {
    int nplanes = weed_palette_get_nplanes(target_palette);
    weed_layer_set_rowstrides(layer, rowstrides, nplanes);
    weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);
  }

  if (!create_empty_pixel_data(layer, TRUE, TRUE)) weed_layer_nullify_pixel_data(layer);
  weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);

  if (prefs->apply_gamma) {
    if (tgt_gamma != WEED_GAMMA_UNKNOWN)
      weed_layer_set_gamma(layer, tgt_gamma);
    else {
      if (sfile) weed_layer_set_gamma(layer, sfile->gamma_type);
      else weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);
    }
  }
  return layer;
}


// similaer to create_empty_pixel_data with blank == TRUE, but we try to guess the frame size,
// palette and gamma. Rowstride values can alse be preset and flag WEED_LEAF_CONST_VALUE will
// leave them as set, then clear the flagbit
weed_layer_t *create_blank_layer(weed_layer_t *layer, const char *image_ext,
                                 int width, int height, int target_palette) {
  return _create_blank_layer(layer, image_ext, width, height, NULL, WEED_GAMMA_UNKNOWN, target_palette);
}


weed_layer_t *create_blank_layer_precise(int width, int height, int *rowstrides,
    int tgt_gamma, int target_palette) {
  return _create_blank_layer(NULL, NULL,  width, height, rowstrides, tgt_gamma, target_palette);
}


LIVES_GLOBAL_INLINE int weed_layer_get_type(weed_layer_t *layer) {
  if (!layer || !WEED_IS_LAYER(layer)) return WEED_LAYER_TYPE_NONE;
  return weed_get_int_value(layer, WEED_LEAF_LAYER_TYPE, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_is_video(weed_layer_t *layer) {
  if (layer && WEED_IS_LAYER(layer) && weed_layer_get_type(layer) == WEED_LAYER_TYPE_VIDEO) return WEED_TRUE;
  return WEED_FALSE;
}


LIVES_GLOBAL_INLINE int weed_layer_is_audio(weed_layer_t *layer) {
  if (layer && WEED_IS_LAYER(layer) && weed_layer_get_type(layer) == WEED_LAYER_TYPE_AUDIO) return WEED_TRUE;
  return WEED_FALSE;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_audio_data(weed_layer_t *layer, float **data,
    int arate, int naudchans, weed_size_t nsamps) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  if (data) weed_set_voidptr_array(layer, WEED_LEAF_AUDIO_DATA, naudchans, (void **)data);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_RATE, arate);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_DATA_LENGTH, nsamps);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_CHANNELS, naudchans);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_flags(weed_layer_t *layer, int flags) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
  return layer;
}


LIVES_GLOBAL_INLINE int weed_layer_get_flags(weed_layer_t *layer) {
  if (!layer || !WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);
}


LIVES_GLOBAL_INLINE int lives_layer_get_clip(weed_layer_t *layer) {
  if (!layer || !WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);
}


LIVES_GLOBAL_INLINE int lives_layer_get_track(weed_layer_t *layer) {
  if (!layer || !WEED_IS_LAYER(layer)) return -1;
  return weed_plant_has_leaf(layer, LIVES_LEAF_TRACK) ? weed_get_int_value(layer, LIVES_LEAF_TRACK, NULL) : -1;
}


LIVES_GLOBAL_INLINE frames_t lives_layer_get_frame(weed_layer_t *layer) {
  if (!layer || !WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, WEED_LEAF_FRAME, NULL);
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_width(weed_layer_t *layer, int width) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_WIDTH, width);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_height(weed_layer_t *layer, int height) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_HEIGHT, height);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_size(weed_layer_t *layer, int width, int height) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  if (width > 0 && !height) abort();
  weed_layer_set_width(layer, width);
  weed_layer_set_height(layer, height);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_pixel_data_planar(weed_layer_t *layer, void **pixel_data, int nplanes) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, nplanes, pixel_data);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_pixel_data(weed_layer_t *layer, void *pixel_data) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
  return layer;
}


LIVES_GLOBAL_INLINE lives_sync_list_t *lives_layer_get_copylist(lives_layer_t *layer) {
  if (!layer) return NULL;
  return (lives_sync_list_t *)weed_get_voidptr_value(layer, LIVES_LEAF_COPYLIST, NULL);
}


// remove layer from a copylist containing it
// delete the copylist leaf from layer
// if layer is the final member of copylist,
//
// otherwise we nullify pixel_data, and if there is a PIXBUG_SRC, unref pixbuf, removing the extra ref,
//
// if there is only one member left in copylist after removing layer, we recursively call the function,
// passing final layer as layer
//
// after removing layer, we then check if there is only a single member remaining
// if so, we check also if this is the proxy_layer for a pixbuf, if so we unref pixbuf, removing the ref we
// added when creating it or copying it to a layer
// we then free the copylist with its single member and delete the copylist leaf in layer
//

//

// removes a layer from a copylist
// - if it iss in a copylist, then there are other layers sharing the data so we must not free pixeldata
// - but we do need to nullify it to ensure thiss

// then, if only one other layer reains in the copylist, we must also remove that

// the remaining layer may be freed as normal
// NB. when freeing a layer, we must check if it has PIXBUF_SRC
// then the next action, depends on whether pixeldata is set or not
// if set, the layer MUST only be freed when the pixbuf is freed,
// since it may be a bigblock data
/// this is taken care of in weed_layer_unrefX(), the callback for freeing pixbuf pixels,
// there we free thee the proxy layer instead of freeing pixels

// for layers with link to pixbuf_src and NULL pdata, this indicates
// created FROM pixbuf, - there is no callback for free
// so we add an extra ref for pixbuf, meaning that proxy_layer is always freed BEFORE pixbuf
// and proxy_layer is freed only when it is the final member in copylist
// once that happens we free the layer then unref pixbuf, removing the extra ref

// return TRUE IF there are NO OTHER LAYERS sharing with layer (pixdata may be freed)
// returns FALSE if there are STILL LAYERS SHARING the pixel-data (pixdata should only be nullified)
LIVES_LOCAL_INLINE boolean _lives_copylist_remove(lives_sync_list_t *copylist, lives_layer_t *layer) {
  pthread_mutex_t *copylist_mutex = (pthread_mutex_t *)weed_get_voidptr_value(layer, "copylist_mutex", NULL);
  LiVESPixbuf *pixbuf;

  pixbuf = (LiVESPixbuf *)weed_get_voidptr_value(layer, LIVES_LEAF_PIXBUF_SRC, NULL);
  weed_leaf_delete(layer, LIVES_LEAF_COPYLIST);
  copylist = lives_sync_list_remove(copylist, layer, FALSE);
  if (copylist_mutex) pthread_mutex_unlock(copylist_mutex);

  // should only happen if we recursed
  if ((!copylist && !pixbuf)) return TRUE;

  if (pixbuf) {
    // - we must unref this layer, then unref pixbuf, removing our added "safety" ref
    weed_layer_set_pixel_data(layer, NULL);
    weed_layer_unref(layer);
    lives_widget_object_unref(pixbuf);
    return TRUE;
  }

  weed_layer_set_pixel_data(layer, NULL);

  if (copylist->last != copylist->list) return FALSE;
  /////////////

  // if layer was penultimate meber of copylist, remove last member from list
  // but leave its pixel_data untouched

  weed_layer_t *xlayer = (weed_layer_t *)copylist->list->data;
  copylist_mutex = (pthread_mutex_t *)weed_get_voidptr_value(xlayer, "copylist_mutex", NULL);
  if (copylist_mutex) pthread_mutex_lock(copylist_mutex);
  return _lives_copylist_remove(copylist, xlayer);
}


LIVES_GLOBAL_INLINE boolean lives_copylist_remove(lives_sync_list_t *copylist, lives_layer_t *layer) {
  // with copylist mutex held, remvoe layer from its copylist
  if (copylist && layer) return _lives_copylist_remove(copylist, layer);
  return TRUE;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_nullify_pixel_data(weed_layer_t *layer) {
  lives_sync_list_t *copylist;
  pthread_mutex_t *copylist_mutex;

  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;

  wait_layer_ready(layer, TRUE);

  copylist_mutex = (pthread_mutex_t *)weed_get_voidptr_value(layer, "copylist_mutex", NULL);
  if (copylist_mutex) pthread_mutex_lock(copylist_mutex);

  copylist = lives_layer_get_copylist(layer);
  g_print("nullify layer %p has copylisst %p\n", layer, copylist);

  if (copylist) {
    g_print("rem layer %p from copylisst %p\n", layer, copylist);
    // if FALSE is returned, then we have detected other layers sharing with this
    // pixels will have been set to NULL, and we fall through to next section
    //
    // if TRUE is returned, then there are no other layers sharing with this one, so we can go ahead
    // pixel_data isnt set to NULL, so we catch this below and free the data instead
    lives_copylist_remove(copylist, (void *)layer);
  }
  if (copylist_mutex) pthread_mutex_unlock(copylist_mutex);

  // nullifying a layer with no copies and having pixel_data is not permitted here
  // else this could result in a memory leak
  // so if we detect this, we free the pixel_data instead, which also nullifies it
  // afterwards
  if (!copylist && weed_layer_get_pixel_data(layer)) {
    LIVES_WARN("NULLIFY non-copied layer with pixdata - freeing first!");
    weed_layer_pixel_data_free(layer);
    return layer;
  }

  weed_layer_set_pixel_data(layer, NULL);
  weed_plant_sanitize(layer, FALSE);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_rowstrides(weed_layer_t *layer, int *rowstrides, int nplanes) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_array(layer, WEED_LEAF_ROWSTRIDES, nplanes, rowstrides);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_rowstride(weed_layer_t *layer, int rowstride) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_palette(weed_layer_t *layer, int palette) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_CURRENT_PALETTE, palette);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_gamma(weed_layer_t *layer, int gamma_type) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, gamma_type);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_yuv_clamping(weed_layer_t *layer, int clamping) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_YUV_CLAMPING, clamping);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_yuv_sampling(weed_layer_t *layer, int sampling) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, sampling);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_yuv_subspace(weed_layer_t *layer, int subspace) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, subspace);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_palette_yuv(weed_layer_t *layer, int palette,
    int clamping, int sampling, int subspace) {
  if (!weed_layer_set_palette(layer, palette)) return NULL;
  weed_layer_set_yuv_clamping(layer, clamping);
  weed_layer_set_yuv_sampling(layer, sampling);
  weed_layer_set_yuv_subspace(layer, subspace);
  return layer;
}


///// layer pixel operations ////


// returns TRUE on success
boolean copy_pixel_data(weed_layer_t *layer, weed_layer_t *old_layer, size_t alignment) {
  // copy (deep) old_layer -> layer
  // if old_layer is NULL, and layer is non-NULL, we can change rowstride alignment for layer
  int numplanes, xheight;
  int *rowstrides;;
  void **pixel_data, **npixel_data;
  int pal, height, i;
  boolean newdata = TRUE;

  if (alignment && !old_layer) {
    // check if we need to align memory
    rowstrides = weed_layer_get_rowstrides(layer, &i);
    while (i > 0) if (rowstrides[--i] % alignment) i = -1;
    lives_free(rowstrides);
    if (!i) return TRUE;
  }

  if (!old_layer) {
    // do alignment
    newdata = FALSE;
    old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_layer_copy(old_layer, layer);
  }

  pixel_data = weed_layer_get_pixel_data_planar(old_layer, &numplanes);
  if (!pixel_data || !pixel_data[0]) {
    if (!newdata) weed_layer_unref(old_layer);
    return TRUE;
  }

  if (alignment) THREADVAR(rowstride_alignment_hint) = alignment;

  lives_layer_copy_metadata(layer, old_layer);

  weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);

  // will free / nullify pixel_data for layer
  if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
    weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);
    if (!newdata) {
      weed_layer_copy(layer, old_layer);
      weed_layer_unref(old_layer);
    }
    return FALSE;
  }

  weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_CONST_VALUE);

  rowstrides = weed_layer_get_rowstrides(layer, &numplanes);
  npixel_data = weed_layer_get_pixel_data_planar(layer, &numplanes);
  height = weed_layer_get_height(layer);
  pal = weed_layer_get_palette(layer);

  for (i = 0; i < numplanes; i++) {
    xheight = height * weed_palette_get_plane_ratio_vertical(pal, i);
    lives_memcpy(npixel_data[i], pixel_data[i], xheight *  rowstrides[i]);
  }

  if (!newdata) weed_layer_unref(old_layer);

  lives_freep((void **)&npixel_data);
  lives_freep((void **)&pixel_data);
  lives_freep((void **)&rowstrides);
  return TRUE;
}


static boolean remove_lpt_cb(lives_proc_thread_t lpt, lives_layer_t *layer) {
  // whenever an asyn op acts on a layer, we should set
  //
  // weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, lpt);
  // lives_proc_thread_add_hook(lpt, COMPLETED_HOOK, 0, remove_lpt_cb, layer);
  // lives_layer_set_status(layer, LAYER_STATUS_QUEUED);
  //
  // the proc_thread can then be queued, the status will change to busy,
  //  then eveantually ready (or invalid)
  lock_layer_status(layer);
  if (lives_layer_plan_controlled(layer)) {
    if (_lives_layer_get_status(layer) == LAYER_STATUS_BUSY
        || _lives_layer_get_status(layer) == LAYER_STATUS_LOADING)
      _lives_layer_set_status(layer, LAYER_STATUS_READY);
  }
  weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL);
  unlock_layer_status(layer);
  return TRUE;
}


void lives_layer_async_auto(lives_layer_t *layer, lives_proc_thread_t lpt) {
  if (layer && lpt) {
    lock_layer_status(layer);
    _lives_layer_set_status(layer, LAYER_STATUS_QUEUED);
    weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, lpt);
    //lives_proc_thread_t hlpt =
    lives_proc_thread_add_hook(lpt, COMPLETED_HOOK, 0, remove_lpt_cb, layer);
    //if (!mainw->debug_ptr) mainw->debug_ptr = hlpt;
    lives_proc_thread_queue(lpt, LIVES_THRDATTR_PRIORITY);
    unlock_layer_status(layer);
  }
}


void lives_layer_reset_timing_data(weed_layer_t *layer) {
  if (layer) {
    weed_timecode_t *timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
    if (!timing_data) {
      timing_data =
        (weed_timecode_t *)lives_calloc(N_LAYER_STATUSES, sizeof(weed_timecode_t));
    }
    for (int i = 0; i < N_LAYER_STATUSES; i++) timing_data[i] = 0;
    weed_set_int64_array(layer, LIVES_LEAF_TIMING_DATA, N_LAYER_STATUSES, timing_data);
    lives_free(timing_data);
  }
}


void lives_layer_update_timing_data(weed_layer_t *layer, int status, ticks_t tc) {
  if (layer && status >= 0 && status < N_LAYER_STATUSES) {
    weed_timecode_t *timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
    if (!timing_data) {
      lives_layer_reset_timing_data(layer);
      timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
      if (tc <= 0) tc = lives_get_session_ticks();
    }
    timing_data[status] = tc;
    weed_set_int64_array(layer, LIVES_LEAF_TIMING_DATA, N_LAYER_STATUSES, timing_data);
    lives_free(timing_data);
  }
}


/**
   @brief create a layer, setting the most important properties */
weed_layer_t *weed_layer_create(int width, int height, int *rowstrides, int palette) {
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

  weed_layer_set_width(layer, width);
  weed_layer_set_height(layer, height);

  if (palette != WEED_PALETTE_END) {
    weed_layer_set_palette(layer, palette);
    if (rowstrides) weed_layer_set_rowstrides(layer, rowstrides, weed_palette_get_nplanes(palette));
  }
  return layer;
}


weed_layer_t *weed_layer_create_full(int width, int height, int *rowstrides, int palette,
                                     int YUV_clamping, int YUV_sampling, int YUV_subspace, int gamma_type) {
  weed_layer_t *layer = weed_layer_create(width, height, rowstrides, palette);
  weed_layer_set_palette_yuv(layer, palette, YUV_clamping, YUV_sampling, YUV_subspace);
  weed_layer_set_gamma(layer, gamma_type);
  return layer;
}


/**
   @brief copy source layer slayer to dest layer dlayer

   if dlayer is NULL, we return a new layer, otherwise we return dlayer
   for a newly created layer, this is a deep copy, since the pixel_data array is also copied
   for an existing dlayer, we copy pixel_data by reference.
   all the other relevant attributes are also copied

   For shallow copy, both layers share the same pixel_data. There is a mechanism to ensure that
   the pixel_data is only freed when the last copy is freed (otherwise it will be nullified).
*/
static weed_layer_t *_weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer, boolean shared) {
  lives_sync_list_t *copylist = NULL;
  pthread_mutex_t *copylist_mutex = NULL, *copylist_mutex2 = NULL;
  boolean full_copy = FALSE;

  if (!slayer || !WEED_IS_XLAYER(slayer)) return NULL;
  if (dlayer && !WEED_IS_XLAYER(dlayer)) return NULL;

  if (!dlayer) {
    /// deep copy
    int height = weed_layer_get_height(slayer);
    int width = weed_layer_get_width(slayer);
    int palette = weed_layer_get_palette(slayer);
    int *rowstrides = weed_layer_get_rowstrides(slayer, NULL);

    if (height <= 0 || width < 0 || !rowstrides || !weed_palette_is_valid(palette))
      return NULL;

    dlayer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    if (!copy_pixel_data(dlayer, slayer, 0)) {
      weed_layer_free(dlayer);
      return NULL;
    }
    return dlayer;
  }
  /// shallow copy

  if (!shared && weed_plant_has_leaf(slayer, LIVES_LEAF_SURFACE_SRC)) {
    // if sharing with a painter (cairo) urface, we cannot shar onwards
    // so we will make a copy and unpremultiply alpha
    full_copy = TRUE;
  }

  weed_layer_pixel_data_free(dlayer);

  // locking this prevents dlayer from being reomved from any copy list as, "last member"
  // since we are now going to add dlayer to copylist, this is superfluous
  // if slsayer and dlayer are already in same copylist, when dlayer is removed
  // if slayer is last member, the finction will try to lock slayer to remove it,
  // however the lock will fail and slayer will maintain its list

  if (!full_copy  && !shared) {
    copylist_mutex = (pthread_mutex_t *)weed_get_voidptr_value(slayer, "copylist_mutex", NULL);
    if (copylist_mutex) pthread_mutex_lock(copylist_mutex);

    copylist_mutex2 = (pthread_mutex_t *)weed_get_voidptr_value(dlayer, "copylist_mutex", NULL);
    if (copylist_mutex2) pthread_mutex_lock(copylist_mutex2);

    copylist = lives_layer_get_copylist(slayer);

    if (copylist && copylist == lives_layer_get_copylist(dlayer)) {
      // dlayer is already a copy of slayer, so do nothing
      if (copylist_mutex2) pthread_mutex_unlock(copylist_mutex2);
      if (copylist_mutex) pthread_mutex_unlock(copylist_mutex);
      return dlayer;
    }

    if (copylist_mutex2) pthread_mutex_unlock(copylist_mutex2);
    if (copylist_mutex) pthread_mutex_unlock(copylist_mutex);
  }

  lives_layer_copy_metadata(dlayer, slayer);

  lives_leaf_dup(dlayer, slayer, WEED_LEAF_PIXEL_DATA);

  if (weed_layer_get_pixel_data(slayer)) {
    if (full_copy) copy_pixel_data(dlayer, slayer, 0);
    else {
      if (!shared) {
        if (!copylist) {
          copylist = lives_sync_list_push(NULL, (void *)slayer);
          weed_set_voidptr_value(slayer, LIVES_LEAF_COPYLIST, (void *)copylist);
          g_print("add %p to  copylist %p\n", slayer, copylist);
        }

        lives_sync_list_push(copylist, (void *)dlayer);
        g_print("add2 %p to  copylist %p\n", dlayer, copylist);

        // not part of the standard metadata set
        lives_leaf_dup(dlayer, slayer, LIVES_LEAF_COPYLIST);
      }

      lives_leaf_dup(dlayer, slayer, LIVES_LEAF_BBLOCKALLOC);
      lives_leaf_dup(dlayer, slayer, WEED_LEAF_HOST_ORIG_PDATA);

      lives_leaf_dup(dlayer, slayer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
      weed_leaf_set_flags(dlayer, WEED_LEAF_PIXEL_DATA,
                          weed_leaf_get_flags(slayer, WEED_LEAF_PIXEL_DATA));
    }
  }

  //g_print("LAYERS: %p duplicated to %p, bb %d %d\n", slayer, layer, weed_plant_has_leaf(slayer, LIVES_LEAF_BBLOCKALLOC),
  //	    weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));

  return dlayer;
}


weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer)
{return _weed_layer_copy(dlayer, slayer, FALSE);}


LIVES_GLOBAL_INLINE lives_result_t weed_pixel_data_share(weed_plant_t *dst, weed_plant_t *src) {
  // similar to weed_layer_copy, but here we share between layers and channels
  // there are two scenarios:
  // inplace: layer shares with in channel, in channel shares with out channel
  // in chan nullifies, out chan nullifies
  // non-inplace: layer shares with in channel, in chan nullifies, layer frees
  // out channel shares with layer, out chan nullifies
  // combining:
  // layer shares with in_chan,
  //		(inplace): in chan shares with out chan
  //   <-fx processing->
  // in chan nullified
  //		(non inplace): layer frees pixdata, out chan shares with layer,
  // out chan nullified
  //
  // thus there is no need to do anything special with copylist
  // except that we dont add shared objects to copylist
  // nor do we nullify sharer pixdata
  weed_layer_t *l1 = _weed_layer_copy(dst, src, TRUE);
  if (l1) return LIVES_RESULT_SUCCESS;
  return LIVES_RESULT_FAIL;
}


LIVES_GLOBAL_INLINE void lock_layer_status(weed_layer_t *layer) {
  pthread_mutex_t *lst_mutex = (pthread_mutex_t *)weed_get_voidptr_value(layer, LIVES_LEAF_LST_MUTEX, NULL);
  if (!lst_mutex) {
    lst_mutex = LIVES_CALLOC_SIZEOF(pthread_mutex_t, 1);
    pthread_mutex_init(lst_mutex, NULL);
    weed_set_voidptr_value(layer, LIVES_LEAF_LST_MUTEX, lst_mutex);
  }
  pthread_mutex_lock(lst_mutex);
}


LIVES_GLOBAL_INLINE void unlock_layer_status(weed_layer_t *layer) {
  pthread_mutex_t *lst_mutex = (pthread_mutex_t *)weed_get_voidptr_value(layer, LIVES_LEAF_LST_MUTEX, NULL);
  if (lst_mutex) pthread_mutex_unlock(lst_mutex);
}


LIVES_GLOBAL_INLINE void _lives_layer_set_status(weed_layer_t *layer, int status) {
  if (status >= 0) {
    ticks_t *tm_data = _get_layer_timing(layer);
    tm_data[status] = lives_get_session_time();
    _set_layer_timing(layer, tm_data);
    lives_free(tm_data);
  }
  weed_set_int_value(layer, LIVES_LEAF_LAYER_STATUS, status);
}



LIVES_GLOBAL_INLINE void lives_layer_set_status(weed_layer_t *layer, int status) {
  if (layer && status >= -1 && status < N_LAYER_STATUSES) {
    lock_layer_status(layer);
    _lives_layer_set_status(layer, status);
    unlock_layer_status(layer);
  }
}


LIVES_GLOBAL_INLINE int _lives_layer_get_status(weed_layer_t *layer) {
  return weed_get_int_value(layer, LIVES_LEAF_LAYER_STATUS, NULL);
}


LIVES_GLOBAL_INLINE int lives_layer_get_status(weed_layer_t *layer) {
  int st = 0;
  if (layer) {
    lock_layer_status(layer);
    st = _lives_layer_get_status(layer);
    unlock_layer_status(layer);
  }
  return st;
}


LIVES_GLOBAL_INLINE ticks_t *_get_layer_timing(lives_layer_t *layer) {
  ticks_t *timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
  if (!timing_data) {
    _reset_layer_timing(layer);
    timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
  }
  return timing_data;
}


LIVES_GLOBAL_INLINE ticks_t *get_layer_timing(lives_layer_t *layer) {
  if (layer) {
    ticks_t *timing_data;
    lock_layer_status(layer);
    timing_data = _get_layer_timing(layer);
    unlock_layer_status(layer);
    return timing_data;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void _set_layer_timing(lives_layer_t *layer, ticks_t *timing_data) {
  weed_set_int64_array(layer, LIVES_LEAF_TIMING_DATA, N_LAYER_STATUSES, timing_data);
  weed_leaf_set_autofree(layer, LIVES_LEAF_TIMING_DATA, TRUE);
}


LIVES_GLOBAL_INLINE void set_layer_timing(lives_layer_t *layer, ticks_t *timing_data) {
  if (layer) {
    lock_layer_status(layer);
    _set_layer_timing(layer, timing_data);
    unlock_layer_status(layer);
  }
}


LIVES_GLOBAL_INLINE void _reset_layer_timing(lives_layer_t *layer) {

  ticks_t *timing_data = (ticks_t *)lives_calloc(N_LAYER_STATUSES, sizeof(ticks_t));
  _set_layer_timing(layer, timing_data);
  lives_free(timing_data);
}


LIVES_GLOBAL_INLINE void reset_layer_timing(lives_layer_t *layer) {
  if (layer) {
    lock_layer_status(layer);
    _reset_layer_timing(layer);
    unlock_layer_status(layer);
  }
}


LIVES_GLOBAL_INLINE int weed_layer_count_refs(weed_layer_t *layer) {
  return weed_refcount_query(layer);
}


LIVES_GLOBAL_INLINE void weed_layer_set_invalid(weed_layer_t *layer, boolean is) {
  if (layer) {
    if (is) {
      (weed_layer_set_flags(layer, weed_layer_get_flags(layer) | LIVES_LAYER_INVALID));
      lives_layer_unset_srcgrp(layer);
    } else
      weed_layer_set_flags(layer, weed_layer_get_flags(layer) & ~LIVES_LAYER_INVALID);
  }
}


LIVES_GLOBAL_INLINE boolean weed_layer_check_valid(weed_layer_t *layer) {
  return layer ? !(weed_layer_get_flags(layer) & LIVES_LAYER_INVALID) : FALSE;
}

/**
   @brief frees pixel_data for a layer, then the layer itself

   if plant is freed
   returns (void *)NULL for convenience
*/

LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_free(weed_layer_t *layer) {
  if (layer) {
    int lstatus;
    pthread_mutex_t *mutexp;
    g_print("free layer %p\n", layer);

    if (mainw->debug_ptr == layer) {
      g_print("FREE %p\n", layer);
      break_me("free dbg");
      mainw->debug_ptr = NULL;
    }
    lock_layer_status(layer);
    lstatus = _lives_layer_get_status(layer);
    if (lstatus == LAYER_STATUS_CONVERTING) {
      // cannot free layer if loading or converting, but when op is finsished,
      // caller must check refconut, and if 0 must lives_layer_free(layer)
      unlock_layer_status(layer);
      return layer;
    }
    unlock_layer_status(layer);
#ifdef DEBUG_LAYER_REFS
    while (weed_layer_count_refs(layer) > 0) {
      if (!weed_layer_unref(layer)) return NULL;
    }
#endif
    if (weed_layer_get_pixel_data(layer)) weed_layer_pixel_data_free(layer);
    else weed_layer_nullify_pixel_data(layer);
    //g_print("LAYERS: %p freed, bb %d\n", layer, weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));
    mutexp = weed_get_voidptr_value(layer, "copylist_mutex", NULL);
    if (mutexp) lives_free(mutexp);
    mutexp = weed_get_voidptr_value(layer, LIVES_LEAF_LST_MUTEX, NULL);
    if (mutexp) lives_free(mutexp);
    weed_plant_free(layer);
  }
  return NULL;
}


#ifdef DEBUG_LAYER_REFS
int _weed_layer_unref(weed_layer_t *layer) {
  ____FUNC_ENTRY____(_weed_layer_unref, "i", "p");
#else
LIVES_GLOBAL_INLINE int weed_layer_unref(weed_layer_t *layer) {
#endif
#if 0
}
#endif
int refs = weed_refcount_dec(layer);
if (layer == mainw->debug_ptr) {
  break_me("unref dbg");
  g_print("nrefs is %d\n", refs);
}
if (!refs) weed_layer_free(layer);
#ifdef DEBUG_LAYER_REFS
____FUNC_EXIT____;
#endif
return refs;
}


#ifdef DEBUG_LAYER_REFS
int _weed_layer_ref(weed_layer_t *layer) {
  ____FUNC_ENTRY____(_weed_layer_ref, "i", "p");
#else
LIVES_GLOBAL_INLINE int weed_layer_ref(weed_layer_t *layer) {
#endif
#if 0
}
#endif
//if (!layer) break_me("null layer");
//g_print("refff layer %p\n", layer);
#ifdef DEBUG_LAYER_REFS
____FUNC_EXIT____;
#endif
//if (LIVES_IS_PLAYING && mainw->layers && layer == mainw->layers[0]) break_me("ref mwfl");
return weed_refcount_inc(layer);
}


LIVES_GLOBAL_INLINE void **weed_layer_get_pixel_data_planar(weed_layer_t *layer, int *nplanes) {
  void **pd;
  int xnplanes = 0;
  if (nplanes) *nplanes = 0;
  if (!layer) return NULL;
  pd = weed_get_voidptr_array_counted(layer, WEED_LEAF_PIXEL_DATA, &xnplanes);
  if (nplanes) *nplanes = xnplanes;
  if (xnplanes == 1 && !pd[0]) {
    lives_free(pd);
    return NULL;
  }
  return pd;
}


LIVES_GLOBAL_INLINE uint8_t *weed_layer_get_pixel_data(weed_layer_t *layer) {
  if (!layer)  return NULL;
  return (uint8_t *)weed_get_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, NULL);
}


LIVES_GLOBAL_INLINE float **weed_layer_get_audio_data(weed_layer_t *layer, int *naudchans) {
  if (naudchans) *naudchans = 0;
  if (!layer)  return NULL;
  return (float **)weed_get_voidptr_array_counted(layer, WEED_LEAF_AUDIO_DATA, naudchans);
}


LIVES_GLOBAL_INLINE int *weed_layer_get_rowstrides(weed_layer_t *layer, int *nplanes) {
  if (nplanes) *nplanes = 0;
  if (!layer)  return NULL;
  return weed_get_int_array_counted(layer, WEED_LEAF_ROWSTRIDES, nplanes);
}


LIVES_GLOBAL_INLINE int weed_layer_get_rowstride(weed_layer_t *layer) {
  if (!layer)  return 0;
  return weed_get_int_value(layer, WEED_LEAF_ROWSTRIDES, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_width(weed_layer_t *layer) {
  if (!layer)  return -1;
  return weed_get_int_value(layer, WEED_LEAF_WIDTH, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_width_pixels(weed_layer_t *layer) {
  if (!layer)  return -1;
  int pal = weed_layer_get_palette(layer);
  return weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(pal);
}


LIVES_GLOBAL_INLINE int weed_layer_get_height(weed_layer_t *layer) {
  if (!layer)  return -1;
  return weed_get_int_value(layer, WEED_LEAF_HEIGHT, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_yuv_clamping(weed_layer_t *layer) {
  if (!layer)  return 0;
  return weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_yuv_sampling(weed_layer_t *layer) {
  if (!layer)  return 0;
  return weed_get_int_value(layer, WEED_LEAF_YUV_SAMPLING, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_yuv_subspace(weed_layer_t *layer) {
  if (!layer)  return 0;
  return weed_get_int_value(layer, WEED_LEAF_YUV_SUBSPACE, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_palette(weed_layer_t *layer) {
  if (!layer)  return WEED_PALETTE_END;
  return weed_get_int_value(layer, WEED_LEAF_CURRENT_PALETTE, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_palette_yuv(weed_layer_t *layer, int *clamping, int *sampling, int *subspace) {
  if (!layer)  return WEED_PALETTE_END;
  if (clamping) *clamping = weed_layer_get_yuv_clamping(layer);
  if (sampling) *sampling = weed_layer_get_yuv_sampling(layer);
  if (subspace) *subspace = weed_layer_get_yuv_subspace(layer);
  return weed_get_int_value(layer, WEED_LEAF_CURRENT_PALETTE, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_gamma(weed_layer_t *layer) {
  int gamma_type = WEED_GAMMA_UNKNOWN;
  if (prefs->apply_gamma) {
    if (weed_plant_has_leaf(layer, WEED_LEAF_GAMMA_TYPE)) {
      gamma_type = weed_get_int_value(layer, WEED_LEAF_GAMMA_TYPE, NULL);
    }
    /* if (gamma_type == WEED_GAMMA_UNKNOWN) { */
    /*   break_me("weed_layer_get_gamma with unk. gamma"); */
    /*   LIVES_WARN("Layer with unknown gamma !!"); */
    /*   gamma_type = WEED_GAMMA_SRGB; */
    /* } */
  }
  return gamma_type;
}


LIVES_GLOBAL_INLINE int weed_layer_get_audio_rate(weed_layer_t *layer) {
  if (!WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, WEED_LEAF_AUDIO_RATE, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_naudchans(weed_layer_t *layer) {
  if (!WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, WEED_LEAF_AUDIO_CHANNELS, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_audio_length(weed_layer_t *layer) {
  if (!WEED_IS_LAYER(layer)) return 0;
  return weed_get_int_value(layer, WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
}


int lives_layer_guess_palette(weed_layer_t *layer) {
  // attempt to pre empt the layer palette before it is loaded, using information from the layer_source and the
  // clip and frame data
  // TODO -use src_type

  if (layer) {
    lives_clip_t *sfile;
    int pal, clip;
    if ((pal = weed_layer_get_palette(layer)) != WEED_PALETTE_NONE) return pal;
    clip = lives_layer_get_clip(layer);
    sfile = RETURN_VALID_CLIP(clip);

    if (sfile) {
      frames_t frame = lives_layer_get_clip(layer);
      switch (sfile->clip_type) {
      case CLIP_TYPE_FILE:
        if (frame > 0 && is_virtual_frame(clip, frame)) {
          lives_clip_data_t *cdata = get_clip_cdata(clip);
          if (cdata) return cdata->current_palette;
        }
      case CLIP_TYPE_DISK:
        // use RGB24 or RGBA32
        if (sfile->bpp == 32) return WEED_PALETTE_RGBA32;
        return WEED_PALETTE_RGB24;
        break;
      case CLIP_TYPE_GENERATOR: {
        weed_instance_t *inst = (weed_instance_t *)((get_primary_src(clip))->actor);
        if (inst) {
          weed_channel_t *channel = get_enabled_channel(inst, 0, FALSE);
          if (channel) {
            g_print("get pal for channel %p\n", channel);
            return weed_channel_get_palette(channel);
          }
        }
      }
      return WEED_PALETTE_INVALID;
      case CLIP_TYPE_NULL_VIDEO:
        // we can generate blank frames in any palette,
        // so we will return special value WEED_PALETTE_ANY
        return WEED_PALETTE_ANY;
      case CLIP_TYPE_YUV4MPEG:
      case CLIP_TYPE_LIVES2LIVES:
      case CLIP_TYPE_VIDEODEV:
      default: break;
      }
    }
  }
  return WEED_PALETTE_NONE;
}

