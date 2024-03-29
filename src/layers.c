// layers.c
// LiVES
// (c) G. Finch 2004 - 2023 <salsaman+lives@gmail.com>
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


LIVES_GLOBAL_INLINE boolean weed_layer_contiguous(weed_layer_t *layer) {
  return layer && weed_plant_has_leaf(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
}


void lives_layer_copy_metadata(weed_layer_t *dest, weed_layer_t *src, boolean full) {
  // copy width, height, clip, frame, full palette, etc
  // set full if pixdata is also beign copied
  if (src && dest) {
    lives_leaf_dup(dest, src, WEED_LEAF_CURRENT_PALETTE);
    lives_leaf_dup(dest, src, WEED_LEAF_WIDTH);
    lives_leaf_dup(dest, src, WEED_LEAF_HEIGHT);

    // copy rowstrides, though these may get overwritten if we create new pixel data
    lives_leaf_dup(dest, src, WEED_LEAF_ROWSTRIDES);

    /* lives_leaf_copy_or_delete(dest, WEED_LEAF_CLIP, src); */
    /* lives_leaf_copy_or_delete(dest, WEED_LEAF_FRAME, src); */

    lives_leaf_copy_or_delete(dest, WEED_LEAF_GAMMA_TYPE, src);
    lives_leaf_copy_or_delete(dest, LIVES_LEAF_HOST_FLAGS, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_CLAMPING, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_SUBSPACE, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_YUV_SAMPLING, src);
    lives_leaf_copy_or_delete(dest, WEED_LEAF_PAR, src);

    if (full) {
      //lives_leaf_copy_or_delete(dest, LIVES_LEAF_BBLOCKALLOC, src);
      lives_leaf_copy_or_delete(dest, LIVES_LEAF_SURFACE_SRC, src);
      lives_leaf_copy_or_delete(dest, LIVES_LEAF_PIXBUF_SRC, src);
      lives_leaf_copy_or_delete(dest, WEED_LEAF_HOST_ORIG_PDATA, src);
      lives_leaf_copy_or_delete(dest, WEED_LEAF_PIXEL_DATA, src);
      lives_leaf_copy_or_delete(dest, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, src);
      weed_leaf_set_flags(dest, WEED_LEAF_PIXEL_DATA,
                          weed_leaf_get_flags(src, WEED_LEAF_PIXEL_DATA));
    }
  }
}


/**
   @brief fills layer with default values.
   if layer is NULL, create and return a new layer, else reet values in layer

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
  else weed_set_voidptr_value(layer, WEED_LEAF_AUDIO_DATA, NULL);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_RATE, arate);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_DATA_LENGTH, nsamps);
  weed_set_int_value(layer, WEED_LEAF_AUDIO_CHANNELS, naudchans);
  return layer;
}


static weed_layer_t *_weed_layer_set_flags(weed_layer_t *layer, int flags) {
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
  return layer;
}

weed_layer_t *weed_layer_set_flags(weed_layer_t *layer, int flags) {
  if (!layer || !WEED_IS_LAYER(layer)) return NULL;
  lock_layer_status(layer);
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
  unlock_layer_status(layer);
  return layer;
}


static int _weed_layer_get_flags(weed_layer_t *layer) {
  return weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);
}


LIVES_GLOBAL_INLINE int weed_layer_get_flags(weed_layer_t *layer) {
  int ret = 0;
  if (layer && WEED_IS_LAYER(layer)) {
    lock_layer_status(layer);
    ret = weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);
    unlock_layer_status(layer);
  }
  return ret;
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
  if (!pixel_data) weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, NULL);
  else weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, nplanes, pixel_data);
  return layer;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_set_pixel_data(weed_layer_t *layer, void *pixel_data) {
  weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
  return layer;
}


LIVES_LOCAL_INLINE lives_sync_list_t **lives_layer_get_copylist_array(lives_layer_t *layer, int *nplanes)
{return layer ? (lives_sync_list_t **)weed_get_voidptr_array_counted(layer, LIVES_LEAF_COPYLIST, nplanes) : NULL;}


boolean lives_layer_has_copylist(lives_layer_t *layer) {
  if (layer && weed_plant_has_leaf(layer, LIVES_LEAF_COPYLIST)) {
    int i, j, nplanes, ntot = 0;
    lives_sync_list_t **copylists = lives_layer_get_copylist_array(layer, &nplanes);
    LIVES_CALLOC_TYPE(int, nvals, nplanes);
    for (i = 0; i < nplanes; i++) {
      if (copylists[i]) {
        nvals[i] = lives_sync_list_get_nvals(copylists[i]);
        // exclude 'self'
        ntot += nvals[i] - 1;
      }
    }
    if (ntot && !(ntot & 1)) {
      for (i = 0; i < nplanes; i++) {
        if (nvals[i]) {
          for (j = i + 1; j < nplanes; j++) {
            if (copylists[i] == copylists[j]) {
              ntot -= 2;
              if (!ntot) break;
              nvals[i]--;
              nvals[j]--;
            }
          }
          if (!ntot) break;
        }
      }
    }
    ////
    lives_free(nvals);
    lives_free(copylists);
    return !!ntot;
  }
  return FALSE;
}


void weed_layer_copy_single_plane(weed_layer_t *dest, weed_layer_t *src, int plane) {
  int nplanes;
  lives_sync_list_t *copylist = NULL, **copylists;
  void **pd = weed_layer_get_pixel_data_planar(src, &nplanes);
  void *spd = NULL, *real = NULL;

  if (pd) {
    copylists = lives_layer_get_copylist_array(src, &nplanes);
    spd = pd[plane];

    if (!copylists) copylists = LIVES_CALLOC_SIZEOF(lives_sync_list_t *, nplanes);

    if (!copylists[plane]) {
      copylist = copylists[plane] = lives_sync_list_push(NULL, (void *)src);
      lives_sync_list_set_priv(copylist, spd);
    }

    lives_sync_list_push(copylists[plane], (void *)dest);

    weed_set_voidptr_array(src, LIVES_LEAF_COPYLIST, nplanes, (void **)copylists);
    lives_free(copylists);
    lives_free(pd);

    copylists = lives_layer_get_copylist_array(dest, &nplanes);

    if (copylists && copylists[plane]) {
      real = lives_sync_list_get_priv(copylists[plane]);
      if (copylists[plane] &&  lives_sync_list_remove(copylists[plane], dest, FALSE))
        real = NULL;
    } else if (pd) real = pd[plane];
  }

  ///////////
  if (real) lives_free_maybe_big(real);
  ///////////////////

  pd = weed_layer_get_pixel_data_planar(dest, &nplanes);
  copylists = lives_layer_get_copylist_array(dest, &nplanes);

  if (!copylist) {
    pd[plane] = NULL;
  } else {
    pd[plane] = spd;
    copylists[plane] = copylist;
    weed_set_voidptr_array(dest, LIVES_LEAF_COPYLIST, nplanes, (void **)copylists);
  }
  weed_layer_set_pixel_data_planar(dest, pd, nplanes);
  lives_free(pd); lives_free(copylists);
}


// when freeing or nullifying pixel_data, we remove planes from copylists
void lives_layer_check_remove_copylists(lives_layer_t *layer) {
  if (!weed_plant_has_leaf(layer, WEED_LEAF_PIXEL_DATA)) return;
  int nplanes;
  void **pd = weed_layer_get_pixel_data_planar(layer, &nplanes), *real;
  if (pd) {
    lives_sync_list_t **copylists = lives_layer_get_copylist_array(layer, NULL);
    if (copylists) {
      for (int i = nplanes; i--;) {
        real = NULL;
        if (copylists && copylists[i]) {
          real = lives_sync_list_get_priv(copylists[i]);
          //g_print("check clist for %d\n", i);
          if ((copylists[i] = lives_sync_list_remove(copylists[i], layer, FALSE))) continue;
          //g_print("now empty clist for %d\n", i);
        } else real = pd[i];
        //g_print("free clist for %d %p\n", i, real);
        if (real) lives_free_maybe_big(real);
      }
      weed_leaf_delete(layer, LIVES_LEAF_COPYLIST);
      lives_free(copylists);
    } else for (int i = nplanes; i--;) if (pd[i]) lives_free_maybe_big(pd[i]);
    lives_free(pd);
  }
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_nullify_pixel_data(weed_layer_t *layer) {
  lives_layer_check_remove_copylists(layer);
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


static lives_result_t copy_pixel_data_full(weed_layer_t *dst_layer, weed_layer_t *src_layer, int x_off,
    int y_off, int width, int height, boolean inc_rs) {
  // copy (deep) old_layer -> layer
  // layer should be created prior to this

  int *rowstrides, *orows;;
  weed_flags_t lflags;
  void **pixel_data, **npixel_data;
  int pal, xheight, xwidth, nplanes, rs_hint = 0;
  boolean rem_new_rs = FALSE;

  if (!src_layer || !dst_layer) return LIVES_RESULT_ERROR;

  if (x_off < 0 || y_off < 0 || !width || width < -1 || !height || height < -1)
    return LIVES_RESULT_INVALID;

  xwidth = weed_layer_get_width(src_layer);
  xheight = weed_layer_get_height(src_layer);

  if ((width == -1 && x_off >= xwidth) || (height == -1 && y_off >= xheight))
    return LIVES_RESULT_INVALID;

  if ((width > 0 && width + x_off > xwidth) || (height > 0 && height + y_off > xheight))
    return LIVES_RESULT_INVALID;

  rowstrides = weed_layer_get_rowstrides(src_layer, &nplanes);
  pixel_data = weed_layer_get_pixel_data_planar(src_layer, &nplanes);
  pal = weed_layer_get_palette(src_layer);

  lflags = weed_leaf_get_flags(dst_layer, WEED_LEAF_ROWSTRIDES);

  lives_layer_copy_metadata(dst_layer, src_layer, FALSE);

  if (width == -1) width = xwidth - x_off;
  if (height == -1) height = xheight - y_off;

  weed_layer_set_width(dst_layer, width);
  weed_layer_set_height(dst_layer, height);

  if (!inc_rs) {
    rs_hint = THREADVAR(rowstride_alignment_hint);
    THREADVAR(rowstride_alignment_hint) = -1;
  } else {
    if (!(weed_leaf_get_flags(dst_layer, WEED_LEAF_ROWSTRIDES) & LIVES_FLAG_CONST_VALUE)
        || !weed_plant_has_leaf(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES)) {
      if (!(weed_leaf_get_flags(dst_layer, WEED_LEAF_ROWSTRIDES) & LIVES_FLAG_CONST_VALUE)) {
        if (weed_leaf_get_flags(src_layer, WEED_LEAF_ROWSTRIDES) & LIVES_FLAG_CONST_VALUE)
          weed_leaf_set_flags(dst_layer, WEED_LEAF_ROWSTRIDES, lflags | LIVES_FLAG_CONST_VALUE);
      }
      if (!weed_plant_has_leaf(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES)) {
        rem_new_rs = TRUE;
        lives_leaf_dup_nocheck(dst_layer, src_layer, LIVES_LEAF_NEW_ROWSTRIDES);
        if (!weed_plant_has_leaf(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES)) {
          weed_set_int_array(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES, nplanes, rowstrides);
          weed_leaf_set_flags(dst_layer, WEED_LEAF_ROWSTRIDES, lflags | LIVES_FLAG_CONST_VALUE);
        }
      }
    }
  }
  // will free / nullify pixel_data for layer
  if (!create_empty_pixel_data(dst_layer, FALSE, TRUE)) {
    if (!inc_rs) THREADVAR(rowstride_alignment_hint) = rs_hint;
    else if (rem_new_rs) weed_leaf_delete(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES);
    weed_leaf_set_flags(dst_layer, WEED_LEAF_ROWSTRIDES, lflags);
    return LIVES_RESULT_FAIL;
  }

  if (!inc_rs) THREADVAR(rowstride_alignment_hint) = rs_hint;
  else if (rem_new_rs) weed_leaf_delete(dst_layer, LIVES_LEAF_NEW_ROWSTRIDES);
  weed_leaf_set_flags(dst_layer, WEED_LEAF_ROWSTRIDES, lflags);

  npixel_data = weed_layer_get_pixel_data_planar(dst_layer, &nplanes);
  orows = weed_layer_get_rowstrides(src_layer, &nplanes);

  for (int i = 0; i < nplanes; i++) {
    void *src = pixel_data[i] + y_off * rowstrides[i];
    void *dst = npixel_data[i];

    xheight = height * weed_palette_get_plane_ratio_vertical(pal, i);

    if (!x_off && orows[i] == rowstrides[i] && width == xwidth)
      lives_memcpy(dst, src, xheight * orows[i]);
    else {
      int dx = x_off * pixel_size(pal) * weed_palette_get_plane_ratio_horizontal(pal, i);
      xwidth = width * pixel_size(pal) * weed_palette_get_plane_ratio_horizontal(pal, i);
      for (int j = 0; j < xheight; j++) {
        void *xdst = (char *)dst + j * orows[i];
        void *xsrc = (char *)src + j * rowstrides[i] + dx;
        lives_memcpy(xdst, xsrc, xwidth);
      }
    }
  }

  lives_freep((void **)&npixel_data);
  lives_freep((void **)&pixel_data);
  lives_freep((void **)&rowstrides);
  lives_freep((void **)&orows);
  return LIVES_RESULT_SUCCESS;
}


lives_result_t copy_pixel_data(weed_layer_t *dst, weed_layer_t *src)
{return copy_pixel_data_full(dst, src, 0, 0, -1, -1, TRUE);}


lives_result_t copy_pixel_data_slice(weed_layer_t *dst, weed_layer_t *src,
                                     int offs_x, int offs_y, int width, int height,
                                     boolean inc_rs)
// layer must be created prior, any existing pixel_data will be freed or nullified
// if inc_rs is TRUE, pad rows with rowstrides
// LIVES_LEAF_NEW_ROWSTRIDES may be set in dst_layer to force a specific value
// CONST_VALUE flag may be et in src t force dst to adopt same rowstrides
// otherwise rowstrides are set according to width and src layer palette
// if inc_rs is not set, rowstrides are set to width * pixel_size
// width, height of -1 will copy to the end from  dst (adjusting for non zero off_x, off_y)
// if off_x or off_y are < 0, or if off_x + width > src_width or off_y + height > rc_height
// LIVES_RESULT_INVALID is returned
// palette, gamma_type are copied from src,
{return copy_pixel_data_full(dst, src, offs_x, offs_y, width, height, FALSE);}


LIVES_GLOBAL_INLINE void lives_layer_set_proc_thread(lives_layer_t *layer, lives_proc_thread_t lpt) {
  if (layer) {
    lives_proc_thread_t  oldlpt = lives_layer_get_proc_thread(layer);
    if (oldlpt) lives_proc_thread_unref(oldlpt);
    if (lpt) lives_proc_thread_ref(lpt);
    weed_set_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, lpt);
  }
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_layer_get_proc_thread(lives_layer_t *layer) {
  return layer ? weed_get_voidptr_value(layer, LIVES_LEAF_PROC_THREAD, NULL) : NULL;
}


boolean layer_processed_cb(lives_proc_thread_t lpt, lives_layer_t *layer) {
  if (layer) {
    lock_layer_status(layer);

    lives_layer_unset_srcgrp(layer);

    if (!_weed_layer_check_valid(layer)) {
      _lives_layer_set_status(layer, LAYER_STATUS_INVALID);
      unlock_layer_status(layer);
      lives_layer_set_proc_thread(layer, NULL);
      weed_layer_unref(layer);
      return FALSE;
    }

    if (lives_layer_plan_controlled(layer)) {
      if (_lives_layer_get_status(layer) == LAYER_STATUS_LOADING)
        _lives_layer_set_status(layer, LAYER_STATUS_LOADED);
      if (_lives_layer_get_status(layer) == LAYER_STATUS_PROCESSED)
        _lives_layer_set_status(layer, LAYER_STATUS_READY);
    } else _lives_layer_set_status(layer, LAYER_STATUS_READY);

    unlock_layer_status(layer);
    lives_layer_set_proc_thread(layer, NULL);
    weed_layer_unref(layer);
  }
  return TRUE;
}


void lives_layer_async_auto(lives_layer_t *layer, lives_proc_thread_t lpt) {
  if (layer && lpt) {
    weed_layer_ref(layer);
    lives_layer_set_status(layer, LAYER_STATUS_QUEUED);
    lives_layer_set_proc_thread(layer, lpt);
    lives_proc_thread_add_hook(lpt, COMPLETED_HOOK, 0, layer_processed_cb, layer);
    lives_proc_thread_queue(lpt, LIVES_THRDATTR_PRIORITY);
  }
}


void lives_layer_reset_timing_data(weed_layer_t *layer) {
  if (layer) {
    weed_timecode_t *timing_data = weed_get_int64_array(layer, LIVES_LEAF_TIMING_DATA, NULL);
    if (!timing_data)
      timing_data = LIVES_CALLOC_SIZEOF(weed_timecode_t, N_LAYER_STATUSES);
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
static weed_layer_t *_weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer, int off_x, int off_y,
                                      int width, int height, boolean inc_rs) {
  if (!height || !width || width < -1 || height < -1) return NULL;
  if (!slayer || !WEED_IS_XLAYER(slayer)) return NULL;
  if (dlayer && !WEED_IS_XLAYER(dlayer)) return NULL;

  int xwidth = weed_layer_get_width(slayer);
  int xheight = weed_layer_get_height(slayer);
  int palette = weed_layer_get_palette(slayer);
  int *rowstrides;

  if (off_x > xwidth || (width > -1 && width + off_x > xwidth))
    return NULL;

  if (off_y > xheight || (height > -1 && height + off_y > xheight))
    return NULL;

  rowstrides = weed_layer_get_rowstrides(slayer, NULL);
  if (!rowstrides || !weed_palette_is_valid(palette)) {
    if (rowstrides) lives_free(rowstrides);
    return NULL;
  }
  if (rowstrides) lives_free(rowstrides);

  if (!dlayer) {
    /// deep copy
    dlayer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

    if (weed_layer_get_pixel_data(slayer))  {
      // sets metadata
      lives_result_t res = copy_pixel_data_slice(dlayer, slayer, off_x, off_y, width, height, inc_rs);
      if (res != LIVES_RESULT_SUCCESS) {
        weed_layer_free(dlayer);
        return NULL;
      }
    }
    return dlayer;
  }
  /// shallow copy

  weed_layer_pixel_data_free(dlayer);

  lives_layer_copy_metadata(dlayer, slayer, TRUE);

  // for slices, when doing a shallow copy, the only values which can vary are
  // y_off and height

  if (height != -1) weed_layer_set_height(dlayer, height);
  if (weed_layer_get_pixel_data(slayer)) {
    int nplanes;
    uint8_t **pd;
    lives_sync_list_t **copylists;
    int *irows = weed_layer_get_rowstrides(slayer, &nplanes);

    copylists = lives_layer_get_copylist_array(slayer, &nplanes);
    pd = (uint8_t **)weed_layer_get_pixel_data_planar(slayer, &nplanes);

    if (!copylists) copylists = LIVES_CALLOC_SIZEOF(lives_sync_list_t *, nplanes);

    for (int i = 0; i < nplanes; i++) {
      if (!copylists[i]) {
        copylists[i] = lives_sync_list_push(NULL, (void *)slayer);
        lives_sync_list_set_priv(copylists[i], pd[i]);
      }
      pd[i] += irows[i] * off_y;
      lives_sync_list_push(copylists[i], (void *)dlayer);
    }
    weed_set_voidptr_array(slayer, LIVES_LEAF_COPYLIST, nplanes, (void **)copylists);
    weed_layer_set_pixel_data_planar(dlayer, (void **)pd, nplanes);

    // not part of the standard metadata set
    lives_leaf_dup_nocheck(dlayer, slayer, LIVES_LEAF_COPYLIST);
    //pthread_mutex_unlock(&copylist_mutex);
    lives_free(copylists);
    lives_free(pd);
    lives_free(irows);
  }

  //g_print("LAYERS: %p duplicated to %p, bb %d %d\n", slayer, layer, weed_plant_has_leaf(slayer, LIVES_LEAF_BBLOCKALLOC),
  //	    weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));

  return dlayer;
}


weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer)
{return _weed_layer_copy(dlayer, slayer, 0, 0, -1, -1, TRUE);}


weed_layer_t *weed_layer_copy_slice(weed_layer_t *dlayer, weed_layer_t *slayer,
                                    int off_x, int off_y, int width, int height)
{return _weed_layer_copy(dlayer, slayer, off_x, off_y, width, height, FALSE);}


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

  weed_layer_pixel_data_free(dst);
  lives_layer_copy_metadata(dst, src, TRUE);

  if (weed_layer_get_pixel_data(src)) {
    int nplanes;
    uint8_t **pd;
    lives_sync_list_t **copylists;
    int *irows = weed_layer_get_rowstrides(src, &nplanes);

    copylists = lives_layer_get_copylist_array(src, &nplanes);
    pd = (uint8_t **)weed_layer_get_pixel_data_planar(src, &nplanes);

    if (!copylists) copylists = LIVES_CALLOC_SIZEOF(lives_sync_list_t *, nplanes);

    for (int i = 0; i < nplanes; i++) {
      if (!copylists[i]) {
        copylists[i] = lives_sync_list_push(NULL, (void *)src);
        lives_sync_list_set_priv(copylists[i], pd[i]);
      }
      lives_sync_list_push(copylists[i], (void *)dst);
    }
    weed_set_voidptr_array(src, LIVES_LEAF_COPYLIST, nplanes, (void **)copylists);

    //weed_layer_set_pixel_data_planar(dst, (void **)pd, nplanes);

    // not part of the standard metadata set
    lives_leaf_dup_nocheck(dst, src, LIVES_LEAF_COPYLIST);
    //pthread_mutex_unlock(&copylist_mutex);
    lives_free(copylists);
    lives_free(pd);
    lives_free(irows);
  }
  return LIVES_RESULT_SUCCESS;
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


void _weed_layer_set_invalid(weed_layer_t *layer, boolean is) {
  if (is) {
    _weed_layer_set_flags(layer, _weed_layer_get_flags(layer) | LIVES_LAYER_INVALID);
    if (_lives_layer_get_status(layer) != LAYER_STATUS_NONE)
      _lives_layer_set_status(layer, LAYER_STATUS_INVALID);
    lives_layer_unset_srcgrp(layer);
  } else {
    _weed_layer_set_flags(layer, _weed_layer_get_flags(layer) & ~LIVES_LAYER_INVALID);
    if (_lives_layer_get_status(layer) == LAYER_STATUS_INVALID)
      _lives_layer_set_status(layer, LAYER_STATUS_NONE);
  }
}


void weed_layer_set_invalid(weed_layer_t *layer, boolean is) {
  if (layer) {
    lock_layer_status(layer);
    _weed_layer_set_invalid(layer, is);
    unlock_layer_status(layer);
  }
}


boolean _weed_layer_check_valid(weed_layer_t *layer) {
  if (_weed_layer_get_flags(layer) & LIVES_LAYER_INVALID) return FALSE;
  if (_lives_layer_get_status(layer) == LAYER_STATUS_INVALID) return FALSE;
  return TRUE;
}


boolean weed_layer_check_valid(weed_layer_t *layer) {
  boolean ret = FALSE;
  if (layer) {
    lock_layer_status(layer);
    ret = _weed_layer_check_valid(layer);
    unlock_layer_status(layer);
  }
  return ret;
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

    if (weed_plant_has_leaf(layer, LIVES_LEAF_SURFACE_SRC)) {
      LIVES_WARN("Attempt to free layer with active painter surface source");
      return layer;
    }

    /* if (mainw->debug_ptr == layer) { */
    /*   g_print("FREE %p\n", layer); */
    /*   BREAK_ME("free dbg"); */
    /*   mainw->debug_ptr = NULL; */
    /* } */

    lock_layer_status(layer);
    lstatus = _lives_layer_get_status(layer);
    if (lstatus == LAYER_STATUS_CONVERTING || lstatus == LAYER_STATUS_LOADING) {
      // cannot free layer if loading or converting, but when op is finsished,
      // caller must check refconut, and if 0 must lives_layer_free(layer)
      unlock_layer_status(layer);
      BREAK_ME("badinv");
      LIVES_WARN("Attempt to free layer which is being converted or loading; INVALIDATE layer first please");
      return layer;
    }
    unlock_layer_status(layer);
#ifdef DEBUG_LAYER_REFS
    while (weed_layer_count_refs(layer) > 0) {
      if (!weed_layer_unref(layer)) return NULL;
    }
#endif
    if (lives_layer_get_proc_thread(layer)) lives_layer_set_proc_thread(layer, NULL);

    lives_layer_unset_srcgrp(layer);

    if (weed_layer_get_type(layer) == WEED_LAYER_TYPE_VIDEO) {
      if (weed_layer_get_pixel_data(layer)) weed_layer_pixel_data_free(layer);
      else weed_layer_nullify_pixel_data(layer);
    } else {
      // audio layer
      int naudchans;
      float **adata = weed_layer_get_audio_data(layer, &naudchans);
      if (adata) {
        for (int i = 0; i < naudchans; i++) if (adata[i]) lives_free(adata[i]);
        lives_free(adata);
      }
    }
    //g_print("LAYERS: %p freed, bb %d\n", layer, weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));
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
LIVES_ASSERT(refs >= 0);
if (layer == mainw->debug_ptr) {
  BREAK_ME("unref dbg");
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
//g_print("refff layer %p\n", layer);
#ifdef DEBUG_LAYER_REFS
____FUNC_EXIT____;
#endif
//if (LIVES_IS_PLAYING && mainw->layers && layer == mainw->layers[0]) BREAK_ME("ref mwfl");
return weed_refcount_inc(layer);
}


LIVES_GLOBAL_INLINE void **weed_layer_get_pixel_data_planar(weed_layer_t *layer, int *nplanes) {
  void **pd;
  int xnplanes = 0;
  if (nplanes) *nplanes = 0;
  if (!layer) return NULL;
  pd = weed_get_voidptr_array_counted(layer, WEED_LEAF_PIXEL_DATA, &xnplanes);
  if (xnplanes == 1 && pd && !pd[0]) {
    xnplanes = 0;
    lives_free(pd);
    pd = NULL;
  }
  if (nplanes && xnplanes > 0) *nplanes = xnplanes;
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


LIVES_GLOBAL_INLINE int weed_layer_get_rowstride(weed_layer_t *layer)
{return layer ? weed_get_int_value(layer, WEED_LEAF_ROWSTRIDES, NULL) : 0;}


LIVES_GLOBAL_INLINE int weed_layer_get_width(weed_layer_t *layer)
{return layer ? weed_get_int_value(layer, WEED_LEAF_WIDTH, NULL) : -1;}


int weed_layer_get_width_bytes(weed_layer_t *layer) {
  return layer ? weed_get_int_value(layer, WEED_LEAF_WIDTH, NULL)
         * pixel_size(weed_layer_get_palette(layer)) : -1;
}


int weed_layer_get_plane_size(weed_layer_t *layer, int nplane, boolean inc_rs) {
  if (layer && nplane >= 0 && nplane < WEED_MAXPPLANES) {
    int pal = weed_layer_get_palette(layer);
    if (pal != WEED_PALETTE_INVALID) {
      int xplanes;
      int *rs = weed_layer_get_rowstrides(layer, &xplanes);
      if (rs && xplanes > 0 && xplanes < WEED_MAXPPLANES
          && nplane < xplanes) {
        int width = inc_rs ? weed_layer_get_width(layer)
                    * pixel_size(weed_layer_get_palette(layer))
                    * weed_palette_get_plane_ratio_horizontal(pal, nplane)
                    : rs[nplane];
        int height = weed_layer_get_height(layer)
                     * weed_palette_get_plane_ratio_vertical(pal, nplane);
        return width * height;
      }
    }
  }
  return -1;
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
    /*   BREAK_ME("weed_layer_get_gamma with unk. gamma"); */
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

