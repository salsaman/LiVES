// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

#ifndef HAS_LIVES_COLOURSPACE_H
#define HAS_LIVES_COLOURSPACE_H


typedef struct {
  guchar u0;
  guchar y0;
  guchar v0;
  guchar y1;
} uyvy_macropixel;

typedef struct {
  guchar y0;
  guchar u0;
  guchar y1;
  guchar v0;
} yuyv_macropixel;


typedef struct {
  guchar u2;
  guchar y0;
  guchar y1;
  guchar v2;
  guchar y2;
  guchar y3;
} yuv411_macropixel;




typedef struct {
  void *src;
  gint hsize;
  gint vsize;
  gint irowstrides[4];
  gint orowstrides[4];
  void *dest;
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


// internal thread fns - data points to a lives_cc_params struct
void *convert_rgb_to_uyvy_frame_thread(void *data);
void *convert_bgr_to_uyvy_frame_thread(void *data);
void *convert_rgb_to_yuyv_frame_thread(void *data);
void *convert_bgr_to_yuyv_frame_thread(void *data);

void *convert_rgb_to_yuv_frame_thread(void *data);
void *convert_bgr_to_yuv_frame_thread(void *data);

void *convert_uyvy_to_rgb_frame_thread(void *data);
void *convert_uyvy_to_bgr_frame_thread(void *data);
void *convert_yuyv_to_rgb_frame_thread(void *data);
void *convert_yuyv_to_bgr_frame_thread(void *data);


void *convert_swap3_frame_thread(void *data);
void *convert_swap4_frame_thread(void *data);
void *convert_swap3addpost_frame_thread(void *data);
void *convert_swap3addpre_frame_thread(void *data);
void *convert_swap3delpost_frame_thread(void *data);
void *convert_swap3delpre_frame_thread(void *data);
void *convert_addpre_frame_thread(void *data);
void *convert_addpost_frame_thread(void *data);
void *convert_delpre_frame_thread(void *data);
void *convert_delpost_frame_thread(void *data);
void *convert_swap3postalpha_frame_thread(void *data);
void *convert_swapprepost_frame_thread(void *data);

void *convert_swab_frame_thread(void *data);



///////////////////////////////////////
// these functions should be used in future
boolean convert_layer_palette(weed_plant_t *layer, int outpl, int op_clamping);
boolean convert_layer_palette_with_sampling(weed_plant_t *layer, int outpl, int out_sampling);
boolean convert_layer_palette_full(weed_plant_t *layer, int outpl, int osamtype, boolean oclamping, int osubspace);
boolean apply_gamma (weed_plant_t *ilayer, weed_plant_t *olayer, double gamma); ///< not used
void resize_layer (weed_plant_t *layer, int width, int height, GdkInterpType interp);
void letterbox_layer (weed_plant_t *layer, int width, int height, int nwidth, int nheight);
void compact_rowstrides(weed_plant_t *layer);
void create_empty_pixel_data(weed_plant_t *layer, boolean black_fill, boolean may_contig);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette);
LiVESPixbuf *layer_to_pixbuf (weed_plant_t *layer);
boolean pixbuf_to_layer(weed_plant_t *layer, LiVESPixbuf *);

weed_plant_t *weed_layer_copy (weed_plant_t *dlayer, weed_plant_t *slayer);
void weed_layer_free (weed_plant_t *layer);
weed_plant_t *weed_layer_new(int width, int height, int *rowstrides, int current_palette);
int weed_layer_get_palette(weed_plant_t *layer);

// palette information functions
boolean weed_palette_is_valid_palette(int pal);
boolean weed_palette_is_alpha_palette(int pal);
boolean weed_palette_is_rgb_palette(int pal);
boolean weed_palette_is_yuv_palette(int pal);
boolean weed_palette_is_float_palette(int pal);
boolean weed_palette_has_alpha_channel(int pal);
gint weed_palette_get_bits_per_macropixel(int pal);
gint weed_palette_get_pixels_per_macropixel(int pal);
gint weed_palette_get_numplanes(int pal);
gdouble weed_palette_get_plane_ratio_horizontal(int pal, int plane);
gdouble weed_palette_get_plane_ratio_vertical(int pal, int plane);
boolean weed_palette_is_lower_quality(int p1, int p2);  ///< return TRUE if p1 is lower quality than p2
boolean weed_palette_is_resizable(int pal);
gdouble weed_palette_get_compression_ratio (int pal);

#define BLACK_THRESH 20 ///< if R,G and B values are all <= this, we consider it a "black" pixel
boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf);


void lives_pixbuf_set_opaque(LiVESPixbuf *pixbuf);

const char *weed_palette_get_name(int pal);
const char *weed_yuv_clamping_get_name(int clamping);
const char *weed_yuv_subspace_get_name(int subspace);
gchar *weed_palette_get_name_full(int pal, int clamped, int subspace);


#ifdef USE_SWSCALE
void sws_free_context(void);
#endif


#endif
