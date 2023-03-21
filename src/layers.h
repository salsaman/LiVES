// layers.h
// LiVES
// (c) G. Finch 2004 - 2020 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// layers - containers for an indivual vidoe frame and / or audio data packet

#ifndef HAS_LIVES_LAYERS_H
#define HAS_LIVES_LAYERS_H

/////////////////////////////////////// LAYERS ///////////////////////////////////////

#define WEED_PLANT_LAYER 128

#define WEED_LEAF_LAYER_TYPE "layer_type"
#define WEED_LAYER_TYPE_NONE	0
#define WEED_LAYER_TYPE_VIDEO	1
#define WEED_LAYER_TYPE_AUDIO	2

#define WEED_LEAF_CLIP "clip"
#define WEED_LEAF_FRAME "frame"
#define LIVES_LEAF_TRACK "track"

#define WEED_IS_LAYER(plant) (weed_plant_get_type(plant) == WEED_PLANT_LAYER)

#define WEED_IS_XLAYER(plant) (weed_plant_get_type(plant) == WEED_PLANT_LAYER || weed_plant_get_type(plant) == WEED_PLANT_CHANNEL)

// create / destroy / copy layers
weed_layer_t *weed_layer_new(int layer_type);
int weed_layer_get_type(weed_layer_t *);
weed_layer_t *create_blank_layer(weed_layer_t *, const char *image_ext, int width, int height, int target_palette);
weed_layer_t *create_blank_layer_precise(int width, int height, int *rowstrides, int tgt_gamma, int target_palette);
weed_layer_t *weed_layer_create(int width, int height, int *rowstrides, int current_palette);
weed_layer_t *weed_layer_create_full(int width, int height, int *rowstrides, int current_palette,
                                     int YUV_clamping, int YUV_sampling, int YUV_subspace, int gamma_type);
weed_layer_t *lives_layer_create_with_metadata(int clipno, frames_t frame);
void lives_layer_copy_metadata(weed_layer_t *dest, weed_layer_t *src);

weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer);
weed_layer_t *weed_layer_free(weed_layer_t *);
int weed_layer_unref(weed_layer_t *);
int weed_layer_ref(weed_layer_t *);
int weed_layer_count_refs(weed_layer_t *);

// lives specific
weed_layer_t *lives_layer_new_for_frame(int clip, frames_t frame);

void lives_layer_set_track(weed_layer_t *, int track);
void lives_layer_set_clip(weed_layer_t *, int clip);
void lives_layer_set_frame(weed_layer_t *, frames_t frame);

int lives_layer_get_track(weed_layer_t *);
int lives_layer_get_clip(weed_layer_t *);
frames_t lives_layer_get_frame(weed_layer_t *);

void lives_layer_set_source(weed_layer_t *, lives_clip_src_t *);
lives_clip_src_t *lives_layer_get_source(weed_layer_t *);
void lives_layer_unset_source(weed_layer_t *);

boolean weed_layer_check_valid(weed_layer_t *);
void weed_layer_set_invalid(weed_layer_t *, boolean);

/// private flags
#define LIVES_LAYER_LOAD_IF_NEEDS_RESIZE	1
#define LIVES_LAYER_GET_SIZE_ONLY		2
#define LIVES_LAYER_INVALID			4

// layer info
int weed_layer_is_video(weed_layer_t *);
int weed_layer_is_audio(weed_layer_t *);
int weed_layer_get_palette(weed_layer_t *);
int weed_layer_get_palette_yuv(weed_layer_t *, int *clamping, int *sampling, int *subspace);
int weed_layer_get_yuv_clamping(weed_layer_t *);
int weed_layer_get_yuv_sampling(weed_layer_t *);
int weed_layer_get_yuv_subspace(weed_layer_t *);
uint8_t *weed_layer_get_pixel_data(weed_layer_t *);
void **weed_layer_get_pixel_data_planar(weed_layer_t *, int *nplanes);
float **weed_layer_get_audio_data(weed_layer_t *, int *naudchans);
int weed_layer_get_audio_rate(weed_layer_t *layer);
int weed_layer_get_naudchans(weed_layer_t *layer);
int weed_layer_get_audio_length(weed_layer_t *layer);
int *weed_layer_get_rowstrides(weed_layer_t *, int *nplanes);
int weed_layer_get_rowstride(weed_layer_t *); ///< for packed palettes
int weed_layer_get_width(weed_layer_t *);
int weed_layer_get_width_pixels(weed_layer_t *);
int weed_layer_get_height(weed_layer_t *);
int weed_layer_get_palette(weed_layer_t *);
int weed_layer_get_gamma(weed_layer_t *);
int weed_layer_get_flags(weed_layer_t *);

// weed_layer_get_rowstride

/// functions all return the input layer for convenience; no checking for valid values is done
/// if layer is NULL or not weed_layer then NULL is returned
weed_layer_t *weed_layer_set_palette(weed_layer_t *, int palette);
weed_layer_t *weed_layer_set_palette_yuv(weed_layer_t *, int palette, int clamping, int sampling, int subspace);
weed_layer_t *weed_layer_set_yuv_clamping(weed_layer_t *, int clamping);
weed_layer_t *weed_layer_set_yuv_sampling(weed_layer_t *, int sampling);
weed_layer_t *weed_layer_set_yuv_subspace(weed_layer_t *, int subspace);
weed_layer_t *weed_layer_set_gamma(weed_layer_t *, int gamma_type);

/// width in macropixels of the layer palette
weed_layer_t *weed_layer_set_width(weed_layer_t *, int width);
weed_layer_t *weed_layer_set_height(weed_layer_t *, int height);
weed_layer_t *weed_layer_set_size(weed_layer_t *, int width, int height);
weed_layer_t *weed_layer_set_rowstrides(weed_layer_t *, int *rowstrides, int nplanes);
weed_layer_t *weed_layer_set_rowstride(weed_layer_t *, int rowstride);

weed_layer_t *weed_layer_set_flags(weed_layer_t *, int flags);

weed_layer_t *weed_layer_set_pixel_data_planar(weed_layer_t *, void **pixel_data, int nplanes);
weed_layer_t *weed_layer_set_pixel_data(weed_layer_t *, void *pixel_data);
weed_layer_t *weed_layer_nullify_pixel_data(weed_layer_t *);
weed_layer_t *weed_layer_set_audio_data(weed_layer_t *, float **data, int arate, int naudchans, weed_size_t nsamps);

#endif
