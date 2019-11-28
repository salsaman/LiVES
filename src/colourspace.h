// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2017 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

#ifndef HAS_LIVES_COLOURSPACE_H
#define HAS_LIVES_COLOURSPACE_H

#define WEED_LEAF_HOST_PIXEL_DATA_CONTIGUOUS "host_pixel_data_contiguous"
#define WEED_LEAF_HOST_PIXBUF_SRC "host_pixbuf_src"
#define WEED_LEAF_HOST_SURFACE_SRC "host_surface_src"

#define DEF_SCREEN_GAMMA 2.2

#define WEED_PLANT_IS_LAYER(lay) WEED_PLANT_IS_CHANNEL(lay)

/// rowstride alignment values
#define ALIGN_MIN 4
#define ALIGN_DEF 16

// rgb / yuv conversion factors ////////////
#define FP_BITS 16 /// max fp bits
#define SCALE_FACTOR (1 << FP_BITS)

#define KR_YCBCR 0.299
#define KB_YCBCR 0.114

#define KR_BT709 0.2126
#define KB_BT709 0.0722

#define KR_BT2020 0.2627
#define KB_BT2020 0.0593

#define YUV_CLAMP_MIN 16.
#define YUV_CLAMP_MINI 16

#define Y_CLAMP_MAX 235.

#define UV_CLAMP_MAX 240.
#define UV_CLAMP_MAXI 240

#define CLAMP_FACTOR_Y ((Y_CLAMP_MAX-YUV_CLAMP_MIN)/255.) // unclamped -> clamped
#define CLAMP_FACTOR_UV ((UV_CLAMP_MAX-YUV_CLAMP_MIN)/255.) // unclamped -> clamped

#define UV_BIAS 128.

/////////////////////////////////////////////

typedef struct {
  uint8_t u0;
  uint8_t y0;
  uint8_t v0;
  uint8_t y1;
} uyvy_macropixel;

typedef struct {
  uint8_t y0;
  uint8_t u0;
  uint8_t y1;
  uint8_t v0;
} yuyv_macropixel;

typedef struct {
  uint8_t u2;
  uint8_t y0;
  uint8_t y1;
  uint8_t v2;
  uint8_t y2;
  uint8_t y3;
} yuv411_macropixel;

typedef struct {
  void *src;
  void *srcp[4];
  int hsize;
  int vsize;
  int irowstrides[4];
  int orowstrides[4];
  void *dest;
  void *destp[4];
  boolean in_alpha;
  boolean out_alpha;
  boolean in_clamped;
  boolean out_clamped;
  int in_subspace;
  int out_subspace;
  int in_sampling;
  int out_sampling;
  boolean alpha_first;
  int thread_id;
} lives_cc_params;

// internal thread fns
void *convert_rgb_to_uyvy_frame_thread(void *cc_params);
void *convert_bgr_to_uyvy_frame_thread(void *cc_params);
void *convert_rgb_to_yuyv_frame_thread(void *cc_params);
void *convert_bgr_to_yuyv_frame_thread(void *cc_params);
void *convert_argb_to_uyvy_frame_thread(void *cc_params);
void *convert_argb_to_yuyv_frame_thread(void *cc_params);

void *convert_rgb_to_yuv_frame_thread(void *cc_params);
void *convert_bgr_to_yuv_frame_thread(void *cc_params);
void *convert_argb_to_yuv_frame_thread(void *cc_params);
void *convert_rgb_to_yuvp_frame_thread(void *cc_params);
void *convert_bgr_to_yuvp_frame_thread(void *cc_params);
void *convert_argb_to_yuvp_frame_thread(void *cc_params);

void *convert_uyvy_to_rgb_frame_thread(void *cc_params);
void *convert_uyvy_to_bgr_frame_thread(void *cc_params);
void *convert_uyvy_to_argb_frame_thread(void *cc_params);
void *convert_yuyv_to_rgb_frame_thread(void *cc_params);
void *convert_yuyv_to_bgr_frame_thread(void *cc_params);
void *convert_yuyv_to_argb_frame_thread(void *cc_params);

void *convert_yuv_planar_to_rgb_frame_thread(void *cc_params);
void *convert_yuv_planar_to_bgr_frame_thread(void *cc_params);
void *convert_yuv_planar_to_argb_frame_thread(void *cc_params);

void *convert_yuv888_to_rgb_frame_thread(void *cc_params);
void *convert_yuv888_to_bgr_frame_thread(void *cc_params);
void *convert_yuv888_to_argb_frame_thread(void *cc_params);
void *convert_yuva8888_to_rgba_frame_thread(void *cc_params);
void *convert_yuva8888_to_bgra_frame_thread(void *cc_params);
void *convert_yuva8888_to_argb_frame_thread(void *cc_params);

void *convert_swap3_frame_thread(void *cc_params);
void *convert_swap4_frame_thread(void *cc_params);
void *convert_swap3addpost_frame_thread(void *cc_params);
void *convert_swap3addpre_frame_thread(void *cc_params);
void *convert_swap3delpost_frame_thread(void *cc_params);
void *convert_swap3delpre_frame_thread(void *cc_params);
void *convert_addpre_frame_thread(void *cc_params);
void *convert_addpost_frame_thread(void *cc_params);
void *convert_delpre_frame_thread(void *cc_params);
void *convert_delpost_frame_thread(void *cc_params);
void *convert_swap3postalpha_frame_thread(void *cc_params);
void *convert_swapprepost_frame_thread(void *cc_params);

void *convert_swab_frame_thread(void *cc_params);

///////////////////////////////////////

typedef weed_plant_t weed_layer_t;

// these functions should be the defaults in future
boolean convert_layer_palette(weed_layer_t *, int outpl, int op_clamping);
boolean convert_layer_palette_with_sampling(weed_layer_t *, int outpl, int out_sampling);
boolean convert_layer_palette_full(weed_layer_t *, int outpl, int osamtype, int oclamping, int osubspace);
boolean resize_layer(weed_layer_t *, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint);
void letterbox_layer(weed_layer_t *layer, int width, int height, int nwidth, int nheight, LiVESInterpType interp, int tpal, int tclamp);
void compact_rowstrides(weed_layer_t *);
void weed_layer_pixel_data_free(weed_layer_t *);
boolean create_empty_pixel_data(weed_layer_t *, boolean black_fill, boolean may_contig);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette, boolean dest_contig);
LiVESPixbuf *layer_to_pixbuf(weed_layer_t *, boolean realpalette);
boolean pixbuf_to_layer(weed_layer_t *, LiVESPixbuf *) WARN_UNUSED;

weed_plant_t *weed_layer_new(void);
weed_plant_t *weed_layer_new_for_frame(int clip, int frame);
weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer);
void *weed_layer_free(weed_layer_t *);
weed_layer_t *weed_layer_create(int width, int height, int *rowstrides, int current_palette);
weed_layer_t *weed_layer_create_full(int width, int height, int *rowstrides, int current_palette,
                                     int YUV_clamping, int YUV_sampling, int YUV_subspace, int gamma_type);

int weed_layer_get_palette(weed_layer_t *);
void **weed_layer_get_pixel_data(weed_plant_t *layer);
int *weed_layer_get_rowstrides(weed_plant_t *layer);
int weed_layer_get_width(weed_plant_t *layer);
int weed_layer_get_height(weed_plant_t *layer);
int weed_layer_get_palette(weed_plant_t *layer);

lives_painter_t *layer_to_lives_painter(weed_layer_t *);
boolean lives_painter_to_layer(lives_painter_t *cairo, weed_layer_t *);

void create_blank_layer(weed_layer_t *, const char *image_ext, int width, int height, int target_palette);

void alpha_unpremult(weed_layer_t *, boolean un);

boolean align_pixel_data(weed_layer_t *, size_t alignment);

boolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array);

// palette information functions
#define weed_palette_is_pixbuf_palette(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_RGBA32) ? TRUE : FALSE)

boolean weed_palette_is_lower_quality(int p1, int p2);

boolean weed_palette_is_painter_palette(int pal);

boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf);

void lives_pixbuf_set_opaque(LiVESPixbuf *pixbuf);

#ifdef USE_SWSCALE
void sws_free_context(void);
#endif

// gamma correction
boolean gamma_convert_layer(int gamma_type, weed_layer_t *);
void gamma_conv_params(int gamma_type, weed_plant_t *inst, boolean is_in);
int get_layer_gamma(weed_layer_t *);


#define WEED_LAYER_ALPHA_PREMULT 1

#endif
