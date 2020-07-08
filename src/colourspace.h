// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2020 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

#ifndef HAS_LIVES_COLOURSPACE_H
#define HAS_LIVES_COLOURSPACE_H

#ifdef USE_16BIT_PCONV
#define USE_EXTEND
#endif

//#define WEED_ADVANCED_PALETTES

#define WEED_LEAF_CLIP "clip"
#define WEED_LEAF_FRAME "frame"
#define WEED_LEAF_HOST_PIXEL_DATA_CONTIGUOUS "host_contiguous"
#define WEED_LEAF_HOST_PIXBUF_SRC "host_pixbuf_src"
#define WEED_LEAF_HOST_SURFACE_SRC "host_surface_src"
#define WEED_LEAF_PIXEL_BITS "pixel_bits"
#define WEED_LEAF_RESIZE_THREAD "res_thread"
#define WEED_LEAF_PROGSCAN "progscan"

#define DEF_SCREEN_GAMMA 2.2

/// rowstride alignment values
#define ALIGN_MIN 4
#define ALIGN_DEF 16

// rgb / yuv conversion factors ////////////
#define FP_BITS 16 /// max fp bits

#ifdef USE_EXTEND
#define SCALE_FACTOR 65793. /// (2 ^ 24 - 1) / (2 ^ 8 - 1), also 0xFF * SCALE_FACTOR = 0xFFFFFF
#else
#define SCALE_FACTOR (1 << FP_BITS)
#endif

#define KR_YCBCR 0.2989
#define KB_YCBCR 0.114

#define KR_I240 0.212
#define KB_I240 0.087

#define KR_BT709 0.2126
#define KB_BT709 0.0722

#define KR_BT2020 0.2627
#define KB_BT2020 0.0593

#define YUV_CLAMP_MIN 16.
#define YUV_CLAMP_MINI 16

#define Y_CLAMP_MAX 235.
#define Y_CLAMP_MAXI 235

#define UV_CLAMP_MAX 240.
#define UV_CLAMP_MAXI 240

#define CLAMP_FACTOR_Y ((Y_CLAMP_MAX-YUV_CLAMP_MIN)/255.) // unclamped -> clamped
#define CLAMP_FACTOR_UV ((UV_CLAMP_MAX-YUV_CLAMP_MIN)/255.) // unclamped -> clamped

#define UV_BIAS 128.

#define MAX_THREADS 65536

typedef weed_plant_t weed_layer_t;

/////////////////////////////////////////////

typedef struct {
  float offs, lin, thresh, pf;
} gamma_const_t;

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
  size_t hsize;
  size_t vsize;
  size_t psize;
  size_t xoffset;
  int irowstrides[4];
  int orowstrides[4];
  void *dest;
  void *destp[4];
  boolean in_alpha;
  boolean out_alpha;
  boolean in_clamping;
  boolean out_clamping;
  int in_subspace;
  int out_subspace;
  int in_sampling;
  int out_sampling;
  boolean alpha_first;
  void *lut;
  int thread_id;
} lives_cc_params;

#ifdef USE_SWSCALE
#include <libswscale/swscale.h>

typedef struct {
  weed_layer_t *layer;
  int thread_id;
  int iheight;
  struct SwsContext *swscale;
  const uint8_t *ipd[4];
  const uint8_t  *opd[4];
  const int *irw;
  const int *orw;
  int ret;
} lives_sw_params;

#endif

void rgb2hsv(uint8_t r, uint8_t g, uint8_t b, double *h, double *s, double *v);
void hsv2rgb(double h, double s, double v, uint8_t *r, uint8_t *g, uint8_t *b);
boolean pick_nice_colour(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *r1, uint8_t *g1, uint8_t *b1,
                         double max, double lmin, double lmax);

double cdist94(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1);

#ifdef WEED_ADVANCED_PALETTES
#define LIVES_VCHAN_grey	       	2048
#define LIVES_VCHAN_mono_b      	2049
#define LIVES_VCHAN_mono_w      	2050

#define LIVES_VCHAN_cc			3000
#define LIVES_VCHAN_mm			3001
#define LIVES_VCHAN_yy			3002
#define LIVES_VCHAN_kk			3003

#define LIVES_VCHAN_xxx  		4000
#define LIVES_VCHAN_yyy  		4001
#define LIVES_VCHAN_zzz		     	4002

#define LIVES_VCHAN_hh  		4000
#define LIVES_VCHAN_ss  		4001
#define LIVES_VCHAN_vv		     	4002

/// for fun / testing
#define LIVES_PALETTE_ABGR32		6
#define LIVES_PALETTE_YUV121010		8211
#define LIVES_PALETTE_RGB48		9001
#define LIVES_PALETTE_RGBA64		9003
#define LIVES_PALETTE_YUVA420P		9512
#define LIVES_PALETTE_YVU422P		9522
#define LIVES_PALETTE_AYUV8888		9545
#define LIVES_PALETTE_YUVFLOAT		9564
#define LIVES_PALETTE_YUVAFLOAT		9565

const weed_macropixel_t *get_advanced_palette(int weed_palette);
boolean weed_palette_is_valid(int pal);
int get_simple_palette(weed_macropixel_t *mpx);
size_t pixel_size(int pal);
int weed_palette_get_pixels_per_macropixel(int pal);
int weed_palette_get_bits_per_macropixel(int pal);
int weed_palette_get_nplanes(int pal);
boolean weed_palette_is_rgb(int pal);
boolean weed_palette_is_yuv(int pal);
boolean weed_palette_is_alpha(int pal);
boolean weed_palette_has_alpha(int pal);
boolean weed_palette_is_float(int pal);
double weed_palette_get_plane_ratio_horizontal(int pal, int plane);
double weed_palette_get_plane_ratio_vertical(int pal, int plane);

int weed_palette_get_alpha_plane(int pal);
int weed_palette_get_alpha_offset(int pal);
boolean weed_palette_red_first(int pal);
boolean weed_palettes_rbswapped(int pal0, int pal1);
boolean weed_palette_has_alpha_first(int pal);
boolean weed_palette_has_alpha_last(int pal);
#endif

int32_t round_special(int32_t val);

void init_conversions(int intent);

void init_colour_engine(void);

/////////////////////////////////////// LAYERS ///////////////////////////////////////

#define WEED_PLANT_LAYER 128

#define WEED_LEAF_LAYER_TYPE "layer_type"
#define WEED_LAYER_TYPE_NONE	0
#define WEED_LAYER_TYPE_VIDEO	1
#define WEED_LAYER_TYPE_AUDIO	2

#define WEED_IS_LAYER(plant) (weed_plant_get_type(plant) == WEED_PLANT_LAYER)

// create / destroy / copy layers
weed_layer_t *weed_layer_new(int layer_type);
weed_layer_t *create_blank_layer(weed_layer_t *, const char *image_ext, int width, int height, int target_palette);
weed_layer_t *weed_layer_create(int width, int height, int *rowstrides, int current_palette);
weed_layer_t *weed_layer_create_full(int width, int height, int *rowstrides, int current_palette,
                                     int YUV_clamping, int YUV_sampling, int YUV_subspace, int gamma_type);
weed_layer_t *weed_layer_copy(weed_layer_t *dlayer, weed_layer_t *slayer);
void *weed_layer_free(weed_layer_t *);

// lives specific
weed_layer_t *lives_layer_new_for_frame(int clip, frames_t frame);

void lives_layer_set_clip(weed_layer_t *, int clip);
void lives_layer_set_frame(weed_layer_t *, frames_t frame);

int lives_layer_get_clip(weed_layer_t *);
frames_t lives_layer_get_frame(weed_layer_t *);

// pixel_data
/// layer should be pre-set with palette, width in MACROPIXELS, and height
/// gamma_type will be set WEED_GAMMA_SRGB, old pixel_data will not be freed.
boolean create_empty_pixel_data(weed_layer_t *, boolean black_fill, boolean may_contig);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette, boolean dest_contig);
void weed_layer_pixel_data_free(weed_layer_t *);

#define WEED_GAMMA_MONITOR 1024
#define WEED_GAMMA_FILE 1025
#define WEED_GAMMA_VARIANT 2048
#define WEED_LAYER_ALPHA_PREMULT 1

#define DEF_FILE_GAMMA 1.22

// layer transformation functions
void alpha_unpremult(weed_layer_t *, boolean un);
boolean copy_pixel_data(weed_layer_t *dst, weed_layer_t *src_or_null, size_t alignment);
boolean gamma_convert_layer(int gamma_type, weed_layer_t *);
boolean gamma_convert_layer_variant(double file_gamma, weed_layer_t *);
boolean gamma_convert_sub_layer(int gamma_type, double fileg, weed_layer_t *, int x, int y, int width, int height);
boolean convert_layer_palette(weed_layer_t *, int outpl, int op_clamping);
boolean convert_layer_palette_with_sampling(weed_layer_t *, int outpl, int out_sampling);
boolean convert_layer_palette_full(weed_layer_t *, int outpl, int oclamping, int osampling, int osubspace, int tgamma);
void lives_layer_set_opaque(weed_layer_t *);
boolean consider_swapping(int *inpal, int *outpal);

/// widths in PIXELS
boolean resize_layer(weed_layer_t *, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint);
boolean letterbox_layer(weed_layer_t *, int nwidth, int nheight, int width, int height, LiVESInterpType interp, int tpal,
                        int tclamp);
boolean compact_rowstrides(weed_layer_t *);

void gamma_conv_params(int gamma_type, weed_layer_t *inst, boolean is_in);

// palette information functions
boolean weed_palette_is_lower_quality(int p1, int p2);
boolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array);

// lives_painter (cairo) functions
boolean weed_palette_is_painter_palette(int pal);
lives_painter_t *layer_to_lives_painter(weed_layer_t *);
boolean lives_painter_to_layer(lives_painter_t *cairo, weed_layer_t *);

// pixbuf functions
#define weed_palette_is_pixbuf_palette(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_RGBA32) ? TRUE : FALSE)
boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf);
void lives_pixbuf_set_opaque(LiVESPixbuf *pixbuf);

LiVESPixbuf *layer_to_pixbuf(weed_layer_t *, boolean realpalette, boolean fordisp);
boolean pixbuf_to_layer(weed_layer_t *, LiVESPixbuf *) WARN_UNUSED;

// layer info
int weed_layer_is_video(weed_layer_t *);
int weed_layer_is_audio(weed_layer_t *);
int weed_layer_get_palette(weed_layer_t *);
int weed_layer_get_palette_yuv(weed_layer_t *, int *clamping, int *sampling, int *subspace);
int weed_layer_get_yuv_clamping(weed_layer_t *);
int weed_layer_get_yuv_sampling(weed_layer_t *);
int weed_layer_get_yuv_subspace(weed_layer_t *);
uint8_t *weed_layer_get_pixel_data_packed(weed_layer_t *);
void **weed_layer_get_pixel_data(weed_layer_t *, int *nplanes);
float **weed_layer_get_audio_data(weed_layer_t *, int *naudchans);
int weed_layer_get_audio_rate(weed_layer_t *layer);
int weed_layer_get_naudchans(weed_layer_t *layer);
int weed_layer_get_audio_length(weed_layer_t *layer);
int *weed_layer_get_rowstrides(weed_layer_t *, int *nplanes);
int weed_layer_get_rowstride(weed_layer_t *); ///< for packed palettes
int weed_layer_get_width(weed_layer_t *);
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

weed_layer_t *weed_layer_set_pixel_data(weed_layer_t *, void **pixel_data, int nplanes);
weed_layer_t *weed_layer_set_pixel_data_packed(weed_layer_t *, void *pixel_data);
weed_layer_t *weed_layer_nullify_pixel_data(weed_layer_t *);
weed_layer_t *weed_layer_set_audio_data(weed_layer_t *, float **data, int arate, int naudchans, weed_size_t nsamps);

#endif
