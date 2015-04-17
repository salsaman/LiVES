// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

#ifndef HAS_LIVES_COLOURSPACE_H
#define HAS_LIVES_COLOURSPACE_H

#define SCREEN_GAMMA 2.2

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
// these functions should be used in future
boolean convert_layer_palette(weed_plant_t *layer, int outpl, int op_clamping);
boolean convert_layer_palette_with_sampling(weed_plant_t *layer, int outpl, int out_sampling);
boolean convert_layer_palette_full(weed_plant_t *layer, int outpl, int osamtype, boolean oclamping, int osubspace);
//boolean apply_gamma (weed_plant_t *ilayer, weed_plant_t *olayer, double gamma); ///< not used
boolean resize_layer(weed_plant_t *layer, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint);
void letterbox_layer(weed_plant_t *layer, int width, int height, int nwidth, int nheight);
void compact_rowstrides(weed_plant_t *layer);
void weed_layer_pixel_data_free(weed_plant_t *layer);
void create_empty_pixel_data(weed_plant_t *layer, boolean black_fill, boolean may_contig);
void insert_blank_frames(int sfileno, int nframes, int after);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette);
LiVESPixbuf *layer_to_pixbuf(weed_plant_t *layer);
boolean pixbuf_to_layer(weed_plant_t *layer, LiVESPixbuf *) WARN_UNUSED;

weed_plant_t *weed_layer_copy(weed_plant_t *dlayer, weed_plant_t *slayer);
void weed_layer_free(weed_plant_t *layer);
weed_plant_t *weed_layer_new(int width, int height, int *rowstrides, int current_palette);
int weed_layer_get_palette(weed_plant_t *layer);

lives_painter_t *layer_to_lives_painter(weed_plant_t *layer);
boolean lives_painter_to_layer(lives_painter_t *cairo, weed_plant_t *layer);

void alpha_unpremult(weed_plant_t *layer, boolean un);


// palette information functions
boolean weed_palette_is_valid_palette(int pal);
boolean weed_palette_is_alpha_palette(int pal);
boolean weed_palette_is_rgb_palette(int pal);
boolean weed_palette_is_yuv_palette(int pal);
boolean weed_palette_is_float_palette(int pal);
boolean weed_palette_has_alpha_channel(int pal);
int weed_palette_get_bits_per_macropixel(int pal);
int weed_palette_get_pixels_per_macropixel(int pal);
int weed_palette_get_numplanes(int pal);
double weed_palette_get_plane_ratio_horizontal(int pal, int plane);
double weed_palette_get_plane_ratio_vertical(int pal, int plane);
boolean weed_palette_is_lower_quality(int p1, int p2);  ///< return TRUE if p1 is lower quality than p2
boolean weed_palette_is_resizable(int pal, int clamped, boolean in_out);
double weed_palette_get_compression_ratio(int pal);

int get_weed_palette_for_lives_painter(void);

#define BLACK_THRESH 20 ///< if R,G and B values are all <= this, we consider it a "black" pixel
boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf);


void lives_pixbuf_set_opaque(LiVESPixbuf *pixbuf);

const char *weed_palette_get_name(int pal);
const char *weed_yuv_clamping_get_name(int clamping);
const char *weed_yuv_subspace_get_name(int subspace);
char *weed_palette_get_name_full(int pal, int clamped, int subspace);


#ifdef USE_SWSCALE
void sws_free_context(void);
#endif


#endif
