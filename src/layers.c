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
#include "cvirtual.h"

LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_new(int layer_type) {
  weed_layer_t *layer = weed_plant_new(WEED_PLANT_LAYER);
  weed_set_int_value(layer, WEED_LEAF_LAYER_TYPE, layer_type);
  weed_add_refcounter(layer);
  return layer;
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


LIVES_GLOBAL_INLINE void lives_layer_set_source(weed_layer_t *layer, lives_clip_src_t *source) {
  if (layer) {
    weed_set_voidptr_value(layer, LIVES_LEAF_SOURCE, source);
    if (source) source->layer = layer;
  }
}


LIVES_GLOBAL_INLINE void lives_layer_unset_source(weed_layer_t *layer) {
  if (layer) {
    lives_clip_src_t *source = weed_get_voidptr_value(layer, LIVES_LEAF_SOURCE, NULL);
    if (source) {
      source->layer = NULL;
      weed_set_voidptr_value(layer, LIVES_LEAF_SOURCE, NULL);
    }
  }
}


LIVES_GLOBAL_INLINE lives_clip_src_t *lives_layer_get_source(weed_layer_t *layer) {
  return layer ?  weed_get_voidptr_value(layer, LIVES_LEAF_SOURCE, NULL) : NULL;
}


LIVES_GLOBAL_INLINE boolean lives_layer_source_set_status(weed_layer_t *layer, int status) {
  if (layer) {
    lives_clip_src_t *source = lives_layer_get_source(layer);
    if (source) return source->status;
  }
}


LIVES_GLOBAL_INLINE weed_layer_t *lives_layer_create_with_metadata(int clipno, frames_t frame) {
  // create a layer and try to set natural_size and palette
  weed_layer_t *layer;
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  lives_clip_src_t *dsource;
  int width = 0, height = 0, pal = 0;
  if (!sfile) return NULL;
  dsource  = sfile->primary_src;
  layer = lives_layer_new_for_frame(clipno, frame);
  switch (sfile->clip_type) {
  case CLIP_TYPE_FILE:
    if (is_virtual_frame(clipno, frame)) {
      lives_clip_data_t *cdata = get_clip_cdata(clipno);
      pal = cdata->current_palette;
      // layer width is always measured in MACROpixels
      width = cdata->width / weed_palette_get_pixels_per_macropixel(pal);
      height = cdata->height;
      break;
    }
  // for non virtual frames, fall through
  case CLIP_TYPE_DISK:
    width = sfile->hsize;
    height = sfile->vsize;
    if (sfile->bpp == 24) pal = WEED_PALETTE_RGB24;
    else pal = WEED_PALETTE_RGBA32;
    break;
  case CLIP_TYPE_GENERATOR:
    if (dsource) {
      weed_instance_t *inst = (weed_instance_t *)dsource->source;
      if (inst) {
        weed_channel_t *channel = get_enabled_channel(inst, 0, FALSE);
        pal = weed_channel_get_palette(channel);
        width = weed_channel_get_width_pixels(channel);
        height = weed_channel_get_height(channel);
      }
    }
    if (!width || !height) {
      width = sfile->hsize;
      height = sfile->vsize;
    }
    break;
  default: break;
  }

  if (width && height) {
    int nsize[2];
    weed_layer_set_size(layer, width, height);
    nsize[0] = width;
    nsize[1] = height;
    weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
  }

  if (pal) weed_layer_set_palette(layer, pal);

  return layer;
}


void lives_layer_copy_metadata(weed_layer_t *dest, weed_layer_t *src) {
  if (src && dest) {
    int width = weed_layer_get_width(src);
    int height = weed_layer_get_height(src);
    weed_layer_set_size(dest, width, height);
    if (weed_plant_has_leaf(src, WEED_LEAF_NATURAL_SIZE))
      weed_leaf_dup(dest, src, WEED_LEAF_NATURAL_SIZE);
    else {
      int nsize[2];
      nsize[0] = width;
      nsize[1] = height;
      weed_set_int_array(dest, WEED_LEAF_NATURAL_SIZE, 2, nsize);
    }
    weed_leaf_dup(dest, src, WEED_LEAF_CURRENT_PALETTE);
    weed_leaf_dup(dest, src, WEED_LEAF_YUV_SUBSPACE);
    weed_leaf_dup(dest, src, WEED_LEAF_YUV_SAMPLING);
    weed_leaf_dup(dest, src, WEED_LEAF_YUV_CLAMPING);
  }
}


/**
   @brief fills layer with default values.

   If either width or height are zero, then dimensions will be taken from the layer or
   defaults used
   if layer has a palette set, that will be maintained, else it will be set to target_palette
   if targette palette is WEED_PALETTE_NONE then default will be set depending on image_ext
   if this is "jpg" then it will be RGB24, otherwise RGBA32
   finally we create the pixel data for layer */
static weed_layer_t *_create_blank_layer(weed_layer_t *layer, const char *image_ext,
    int width, int height, int *rowstrides, int tgt_gamma, int target_palette) {
  if (!layer) layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  else {
    if (!width) width = weed_layer_get_width(layer);
    if (!height) height = weed_layer_get_height(layer);
    if (!width || !height) {
      int clip = lives_layer_get_clip(layer);
      if (clip && IS_VALID_CLIP(clip)) {
        width = mainw->files[clip]->hsize;
        height = mainw->files[clip]->vsize;
      }
    }
  }
  if (!width) width = DEF_FRAME_HSIZE_UNSCALED;
  if (!height) height = DEF_FRAME_VSIZE_UNSCALED;
  weed_layer_set_size(layer, width, height);
  if (weed_layer_get_palette(layer) == WEED_PALETTE_NONE) {
    if (target_palette != WEED_PALETTE_NONE) weed_layer_set_palette(layer, target_palette);
    else {
      if (!image_ext || !strcmp(image_ext, LIVES_FILE_EXT_JPG))
        weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
      else weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
    }
  }

  if (rowstrides) {
    int nplanes = weed_palette_get_nplanes(target_palette);
    weed_layer_set_rowstrides(layer, rowstrides, nplanes);
    weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
  }
  if (!create_empty_pixel_data(layer, TRUE, TRUE)) weed_layer_nullify_pixel_data(layer);
  weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);

  if (prefs->apply_gamma && tgt_gamma != WEED_GAMMA_UNKNOWN) {
    weed_layer_set_gamma(layer, tgt_gamma);
  }

  return layer;
}


weed_layer_t *create_blank_layer(weed_layer_t *layer, const char *image_ext,
                                 int width, int height, int target_palette) {
  int tgt_gamma = WEED_GAMMA_UNKNOWN;
  if (layer && !weed_plant_has_leaf(layer, WEED_LEAF_GAMMA_TYPE)) {
    int clip = lives_layer_get_clip(layer);
    if (clip && IS_VALID_CLIP(clip))
      tgt_gamma = mainw->files[clip]->gamma_type;
    else tgt_gamma = WEED_GAMMA_SRGB;
  }
  return _create_blank_layer(layer, image_ext, width, height, NULL, tgt_gamma, target_palette);
}


weed_layer_t *create_blank_layer_precise(int width, int height, int *rowstrides, int tgt_gamma, int target_palette) {
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


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_nullify_pixel_data(weed_layer_t *layer) {
  if (!layer || !WEED_IS_XLAYER(layer)) return NULL;
  weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 0, NULL);
  weed_plant_sanitize(layer, FALSE);
  //g_print("LAYERS: %p nullfied, bb %d\n", layer, weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));
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

  int numplanes, xheight, xwidth;
  int *orowstrides, *rowstrides;;
  void **pixel_data, **npixel_data;
  int pal = weed_layer_get_palette(layer);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int i, j;
  boolean newdata = FALSE;
  double psize = pixel_size(pal);

  if (alignment != 0 && !old_layer) {
    // check if we need to align memory
    rowstrides = weed_layer_get_rowstrides(layer, &i);
    while (i > 0) if (rowstrides[--i] % alignment != 0) i = -1;
    lives_free(rowstrides);
    if (i == 0) return TRUE;
  }

  if (!old_layer) {
    // do alginment
    newdata = TRUE;
    old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_layer_copy(old_layer, layer);
  }

  pixel_data = weed_layer_get_pixel_data_planar(old_layer, &numplanes);
  if (!pixel_data || !pixel_data[0]) {
    if (newdata) {
      weed_layer_nullify_pixel_data(old_layer);
      weed_layer_unref(old_layer);
    }
    return FALSE;
  }

  orowstrides = weed_layer_get_rowstrides(old_layer, &numplanes);

  if (alignment != 0) THREADVAR(rowstride_alignment_hint) = alignment;

  /// should we free ?
  weed_layer_nullify_pixel_data(layer);

  weed_layer_set_palette(layer, pal);
  weed_layer_set_size(layer, width / weed_palette_get_pixels_per_macropixel(pal), height);

  if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
    if (newdata) {
      weed_layer_copy(layer, old_layer);
      weed_layer_nullify_pixel_data(old_layer);
      weed_layer_unref(old_layer);
    }
    return FALSE;
  }

  rowstrides = weed_layer_get_rowstrides(layer, &numplanes);
  npixel_data = weed_layer_get_pixel_data_planar(layer, &numplanes);
  width = weed_layer_get_width(layer);
  height = weed_layer_get_height(layer);

  for (i = 0; i < numplanes; i++) {
    xheight = height * weed_palette_get_plane_ratio_vertical(pal, i);
    if (rowstrides[i] == orowstrides[i])
      lives_memcpy(npixel_data[i], pixel_data[i], xheight *  rowstrides[i]);
    else {
      uint8_t *dst = (uint8_t *)npixel_data[i];
      uint8_t *src = (uint8_t *)pixel_data[i];
      xwidth = width * psize * weed_palette_get_plane_ratio_horizontal(pal, i);
      for (j = 0; j < xheight; j++) {
        lives_memcpy(&dst[rowstrides[i] * j], &src[orowstrides[i] * j], xwidth);
      }
    }
  }

  weed_leaf_dup(layer, old_layer, WEED_LEAF_NATURAL_SIZE);

  if (newdata) {

    // CHECK THIS
    if (mainw->frame_layer && layer != mainw->frame_layer
        && weed_layer_get_pixel_data(mainw->frame_layer) == pixel_data[0]) {
      /// retain orig pixel_data if it belongs to mainw->frame_layer
      weed_layer_nullify_pixel_data(old_layer);
    }
    weed_layer_unref(old_layer);
  }
  lives_freep((void **)&npixel_data);
  lives_freep((void **)&pixel_data);
  lives_freep((void **)&orowstrides);
  lives_freep((void **)&rowstrides);
  return TRUE;
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

   For shallow copy, both layers share the same pixel_data. Thus we must be careful NOT to call weed_layer_unref
   on both layers without first nullifying the pixel_data of one or other of the copies,
   or replacing its pixel_data via a call to create_rmpty_pixel_data
*/
weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer) {
  weed_layer_t *layer;
  void **pd_array = NULL;

  if (!slayer || !WEED_IS_XLAYER(slayer)) return NULL;

  if (dlayer) {
    if (!WEED_IS_XLAYER(dlayer)) return NULL;
    layer = dlayer;
  }

  pd_array = weed_layer_get_pixel_data_planar(slayer, NULL);

  if (!dlayer) {
    /// deep copy
    int height = weed_layer_get_height(slayer);
    int width = weed_layer_get_width(slayer);
    int palette = weed_layer_get_palette(slayer);
    int *rowstrides = weed_layer_get_rowstrides(slayer, NULL);
    if (height <= 0 || width < 0 || !rowstrides || !weed_palette_is_valid(palette)) {
      if (pd_array) lives_free(pd_array);
      return NULL;
    } else {
      layer = weed_layer_create(width, height, rowstrides, palette);
      if (!pd_array) weed_layer_nullify_pixel_data(layer);
      else copy_pixel_data(layer, slayer, 0);
      lives_free(rowstrides);
    }
  } else {
    /// shallow copy
    weed_layer_pixel_data_free(layer);
    weed_leaf_dup(layer, slayer, WEED_LEAF_ROWSTRIDES);
    weed_leaf_dup(layer, slayer, WEED_LEAF_PIXEL_DATA);
    weed_leaf_dup(layer, slayer, WEED_LEAF_NATURAL_SIZE);
    weed_leaf_copy_or_delete(layer, WEED_LEAF_HEIGHT, slayer);
    weed_leaf_copy_or_delete(layer, WEED_LEAF_WIDTH, slayer);
    weed_leaf_copy_or_delete(layer, WEED_LEAF_CURRENT_PALETTE, slayer);
    if (pd_array) {
      weed_leaf_copy_or_delete(layer, LIVES_LEAF_BBLOCKALLOC, slayer);
      weed_leaf_copy_or_delete(layer, LIVES_LEAF_PIXBUF_SRC, slayer);
      weed_leaf_copy_or_delete(layer, WEED_LEAF_HOST_ORIG_PDATA, slayer);
      weed_leaf_copy_or_delete(layer, LIVES_LEAF_SURFACE_SRC, slayer);
      weed_leaf_copy_or_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, slayer);
      weed_leaf_set_flags(layer, WEED_LEAF_PIXEL_DATA,
                          weed_leaf_get_flags(slayer, WEED_LEAF_PIXEL_DATA));
    }
    //g_print("LAYERS: %p duplicated to %p, bb %d %d\n", slayer, layer, weed_plant_has_leaf(slayer, LIVES_LEAF_BBLOCKALLOC),
    //	    weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));
  }

  weed_leaf_copy_or_delete(layer, WEED_LEAF_GAMMA_TYPE, slayer);
  weed_leaf_copy_or_delete(layer, LIVES_LEAF_HOST_FLAGS, slayer);
  weed_leaf_copy_or_delete(layer, WEED_LEAF_YUV_CLAMPING, slayer);
  weed_leaf_copy_or_delete(layer, WEED_LEAF_YUV_SUBSPACE, slayer);
  weed_leaf_copy_or_delete(layer, WEED_LEAF_YUV_SAMPLING, slayer);
  weed_leaf_copy_or_delete(layer, WEED_LEAF_PIXEL_ASPECT_RATIO, slayer);

  if (pd_array) lives_free(pd_array);
  return layer;
}


LIVES_GLOBAL_INLINE int weed_layer_count_refs(weed_layer_t *layer) {
  return weed_refcount_query(layer);
}


LIVES_GLOBAL_INLINE void weed_layer_set_invalid(weed_layer_t *layer, boolean is) {
  if (layer) {
    if (is) {
      (weed_layer_set_flags(layer, weed_layer_get_flags(layer) | LIVES_LAYER_INVALID));
      lives_layer_unset_source(layer);
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
    /* if (mainw->debug_ptr == layer) { */
    /*   g_print("FREE %p\n", layer); */
    /*   break_me("free dbg"); */
    /*   mainw->debug_ptr = NULL; */
    /* } */
    weed_layer_pixel_data_free(layer);
    //g_print("LAYERS: %p freed, bb %d\n", layer, weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC));
    weed_plant_free(layer);
  }
  return NULL;
}

int weed_layer_unref(weed_layer_t *layer) {
  int refs = weed_refcount_dec(layer);
  /* if (layer == mainw->debug_ptr) { */
  /*   //break_me("unref dbg"); */
  /*   g_print("nrefs is %d\n", refs); */
  /* } */
  if (!refs) weed_layer_free(layer);
  return refs;
}

LIVES_GLOBAL_INLINE int weed_layer_ref(weed_layer_t *layer) {
  if (!layer) break_me("null layer");
  //g_print("refff layer %p\n", layer);
  return weed_refcount_inc(layer);
}


LIVES_GLOBAL_INLINE void **weed_layer_get_pixel_data_planar(weed_layer_t *layer, int *nplanes) {
  if (nplanes) *nplanes = 0;
  if (!layer)  return NULL;
  return weed_get_voidptr_array_counted(layer, WEED_LEAF_PIXEL_DATA, nplanes);
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

