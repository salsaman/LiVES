// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2020 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

#ifndef HAS_LIVES_COLOURSPACE_H
#define HAS_LIVES_COLOURSPACE_H

//#ifdef USE_16BIT_PCONV
#define USE_EXTEND
//#endif

#define LIVES_RESTRICT __restrict__

#define WEED_LAYER_ALPHA_PREMULT 1

#define WEED_GAMMA_MONITOR 1024
#define WEED_GAMMA_FILE 1025
#define WEED_GAMMA_VARIANT 2048

#define LIVES_LEAF_PIXEL_DATA_CONTIGUOUS "host_contiguous"
#define LIVES_LEAF_PIXBUF_SRC "host_pixbuf_src"
#define LIVES_LEAF_SURFACE_SRC "host_surface_src"
#define LIVES_LEAF_PIXEL_BITS "pixel_bits"
#define LIVES_LEAF_HOST_FLAGS "host_flags"
#define LIVES_LEAF_RESIZE_THREAD "res_thread"
#define LIVES_LEAF_PROGSCAN "progscan"
#define LIVES_LEAF_BBLOCKALLOC "bblockalloc"
#define LIVES_LEAF_ALTSRC "alt_src"
#define LIVES_LEAF_NEW_ROWSTRIDE "new_rowstride"

#define DEF_SCREEN_GAMMA 1.4 // extra gammm boost

/// rowstride alignment values

#define RS_ALIGN_DEF 32
#define RA_MIN 4
#define RA_MAX 128

// rgb / yuv conversion factors ////////////
#define FP_BITS 16 /// max fp bits

#ifdef USE_EXTEND
/// (2 ^ 24 - 1) / (2 ^ 8 - 1), also 0xFF * SCALE_FACTOR = 0xFFFFFF
// so we have expanded range 0 - 0xFFFFFF instead of 0 to 0xFF0000
// thus we can use (int)(val / 256.) instead of (int)(val / 256. + 5)
// in the former 0.0 - 0.9999 -> 0, in the latter 0.0 - 0.49999 -> 0
// so we have a while unit that maps to 0, instead of half a unit, and a whole unit which maps to 255,
// instead of half a unit
// we can take (float)a * 65793. then take (int). now we can add the valuess together, then take result >> 16
// and get correct answers.
#define SCALE_FACTOR 65793.
#else
#define SCALE_FACTOR (1 << FP_BITS)
#endif

struct _conv_array {
  int *Yx_R, *Yx_G, *Yx_B;
  int *Cbx_R, *Cbx_G, *Cbx_B;
  int *Crx_R, *Crx_G, *Crx_B;

  float *Yf_R, *Yf_G, *Yf_B;
  float *Cbf_R, *Cbf_G, *Cbf_B;
  float *Crf_R, *Crf_G, *Crf_B;

  short min_Y, max_Y, min_UV, max_UV;
  uint8_t *cavgx;

  int *RGBx_Y, *Rx_Cr, *Gx_Cb, *Gx_Cr, *Bx_Cb;

  float *RGBf_Y, *Rf_Cr, *Gf_Cb, *Gf_Cr, *Bf_Cb;

  uint8_t *Yx_to_Y, *Ux_to_U, *Vx_to_V;
};

#define KR_YCBCR 0.299
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

#define CLAMP_FACTOR_Y ((Y_CLAMP_MAX - YUV_CLAMP_MIN) / 255.) // unclamped -> clamped
#define CLAMP_FACTOR_UV ((UV_CLAMP_MAX - YUV_CLAMP_MIN) / 255.) // unclamped -> clamped

#define UV_BIAS 128.

#define MAX_THREADS 65536

/////////////////////////////////////////////

#define _PAL_IDX 		0
#define _PAL_CLAMPING 		1
#define _PAL_SAMPLING 		2
#define _PAL_SUBSPACE 		3

typedef union {
  int vals[4];
  int pal, clamping, sampling, subspace;
} full_pal_t;

typedef struct {
  float offs, lin, thresh, pf;
} gamma_const_t;

// o = ((t/l) ^ 1/pf - t) / (1 - (t/l) ^1/pf)
#define INIT_GAMMA(gtype) gamma_tx[gtype##_IDX] = (gamma_const_t) {0., GAMMA_CONSTS_##gtype}; \
  gamma_tx[gtype##_IDX].offs = (powf((gamma_tx[gtype##_IDX].thresh / gamma_tx[gtype##_IDX].lin), \
				     (1. / gamma_tx[gtype##_IDX].pf)) - gamma_tx[gtype##_IDX].thresh) \
    / (1. - (powf((gamma_tx[gtype##_IDX].thresh / gamma_tx[gtype##_IDX].lin), \
		  (1. / gamma_tx[gtype##_IDX].pf)))); gamma_idx[gtype##_IDX] = gtype;

extern gamma_const_t gamma_tx[];
extern int gamma_idx[];

// additional gamma_types an be defined here /////////////////

#define GAMMA_CONSTS_WEED_GAMMA_SRGB 12.92, 0.04045, 2.4
#define GAMMA_CONSTS_WEED_GAMMA_BT709 4.5, 0.018, 1. / .45
//#define GAMMA_CONSTS_MYGAMMA lin, thresh, pf
// (offs will be derived as the point at which val_line(x) ~= val_pf(x)

enum {
  WEED_GAMMA_SRGB_IDX,
  WEED_GAMMA_BT709_IDX,
  // MYGAMMA_IDX,
  N_GAMMA_TYPES,
};

static inline void init_gamma_tx(void) {
  INIT_GAMMA(WEED_GAMMA_SRGB);
  INIT_GAMMA(WEED_GAMMA_BT709);
  //INIT_GAMMA(MYGAMMA);
}

//////////////////////////////////////////////////

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
  int pal;
  int clamping;
  int sampling;
  int subspace;
  int gamma;
} lives_pal_full_t;

lives_pal_full_t *make_full_pal(int pal, int clamping, int sampling, int subspace, int gamme);

typedef struct {
  void *src;
  void *srcp[4];
  size_t hsize;
  size_t vsize;
  boolean is_bottom;
  size_t psize;
  size_t xoffset;
  int irowstrides[4];
  int orowstrides[4];
  void *dest;
  void *destp[4];
  struct _conv_array conv_arrays;
  boolean in_alpha;
  boolean out_alpha;
  boolean in_clamping;
  boolean out_clamping;
  int in_subspace;
  int out_subspace;
  int in_sampling;
  int out_sampling;
  boolean alpha_first;
  boolean is_422;
  uint8_t *lut;
  int thread_id;
  uint64_t padding[6];
} lives_cc_params;

#ifdef USE_SWSCALE
#include <libswscale/swscale.h>

typedef struct {
  weed_layer_t *layer;
  int thread_id;
  int iheight;
  int width;
  double file_gamma;
  struct SwsContext *swscale;
  const uint8_t *ipd[4];
  const uint8_t  *opd[4];
  const int *irw;
  const int *orw;
  int ret;
} lives_sw_params;

#endif

struct XYZ {double x, y, z;};

void rgb2hsv(uint8_t r, uint8_t g, uint8_t b, double *h, double *s, double *v) LIVES_HOT;
void hsv2rgb(double h, double s, double v, uint8_t *r, uint8_t *g, uint8_t *b) LIVES_HOT;
boolean pick_nice_colour(ticks_t timeout, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *r1, uint8_t *g1, uint8_t *b1,
                         double max, double lmin, double lmax);

double cdist94(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1);

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

#define MAX_N_PALS 256

// convert from advanced palette to simple palette and vice versa
const weed_macropixel_t *get_advanced_palette(int weed_palette);
int get_simple_palette(weed_macropixel_t *mpx);
int get_enum_palette(int weed_palette);

boolean weed_palette_is_valid(int pal);

#define pixel_size(pal) ((int)weed_palette_get_bytes_per_macropixel(pal))

int weed_palette_get_pixels_per_macropixel(int pal);

int weed_palette_get_bits_per_macropixel(int pal);
int weed_palette_get_bits_per_pixel(int pal);

int weed_palette_get_bits_per_pixel_planar(int pal, int n);

double weed_palette_get_bytes_per_macropixel(int pal);
double weed_palette_get_bytes_per_pixel(int pal);

int weed_palette_get_nplanes(int pal);

boolean weed_palette_conv_resizable(int ipal, int opal);
boolean weed_palette_is_resizable(int pal, int clamped, boolean in_out);

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

int32_t round_special(int32_t val);

void init_conversions(int intent);

void init_colour_engine(void);

double get_luma8(uint8_t r, uint8_t g, uint8_t b);
double get_luma16(uint16_t r, uint16_t g, uint16_t b);

boolean consider_swapping(int *inpal, int *outpal);

#define is_useable_palette(p) (((p) == WEED_PALETTE_RGB24 || (p) == WEED_PALETTE_RGBA32 \
				|| (p) == WEED_PALETTE_BGR24 || (p) == WEED_PALETTE_BGRA32 || (p) == WEED_PALETTE_ARGB32 \
				|| (p) == WEED_PALETTE_YUV888 || (p) == WEED_PALETTE_YUVA8888 || (p) == WEED_PALETTE_YUV422P \
				|| (p) == WEED_PALETTE_YUV420P || (p) == WEED_PALETTE_YVU420P || (p) == WEED_PALETTE_YUV411 \
				|| (p) == WEED_PALETTE_YUV444P || (p) == WEED_PALETTE_YUVA4444P || (p) == WEED_PALETTE_YUV411 \
				|| (p) == WEED_PALETTE_UYVY || (p) == WEED_PALETTE_YUYV) \
  ? TRUE : FALSE)

// palette information functions
boolean weed_palette_is_lower_quality(int p1, int p2);
boolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array);

// lives_painter (cairo) functions
boolean weed_palette_is_painter_palette(int pal);
lives_painter_t *layer_to_lives_painter(weed_layer_t *);
lives_layer_t *lives_painter_to_layer(weed_layer_t *, lives_painter_t *);

boolean lives_painter_surface_check(lives_painter_surface_t *reado);

// layer transformation functions

// pixel_data
/// layer should be pre-set with palette, width in MACROPIXELS, and height
/// gamma_type will be set WEED_GAMMA_SRGB, old pixel_data will not be freed.
boolean create_empty_pixel_data(weed_layer_t *, boolean black_fill, boolean may_contig);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette, boolean dest_contig);
void weed_layer_pixel_data_free(weed_layer_t *);

// layer needed only if fixed rs
int *calc_rowstrides(int width, int pal, weed_layer_t *, int *nplanes);

size_t lives_frame_calc_bytesize(int width, int height, int pal, boolean inc_roswstrides, size_t **planes);

// direction - LIVES_DIRECTION_FOREWARD / LIVES_DIRECTION_REVERSE
void alpha_premult(weed_layer_t *, int direction);

boolean copy_pixel_data(weed_layer_t *dst, weed_layer_t *src_or_null, size_t alignment);

boolean gamma_convert_layer(int gamma_type, weed_layer_t *);
boolean gamma_convert_layer_variant(double file_gamma, int tgt_gamma, weed_layer_t *);
boolean gamma_convert_sub_layer(int gamma_type, double fileg, weed_layer_t *, int x, int y,
                                int width, int height, boolean may_thread);
boolean convert_layer_palette(weed_layer_t *, int outpl, int op_clamping);
boolean convert_layer_palette_with_sampling(weed_layer_t *, int outpl, int out_sampling);
boolean convert_layer_palette_full(weed_layer_t *, int outpl, int oclamping, int osampling, int osubspace, int tgt_gamma);
boolean weed_layer_clear_pixel_data(weed_layer_t *);

void lives_layer_set_opaque(weed_layer_t *);

boolean can_inline_gamma(int inpl, int opal);
boolean pconv_can_inplace(int inpl, int outpl);

/// widths in PIXELS
lives_result_t get_resizable(int *ppalette, int *pxpal, int *oclamp_hint, int *opal,
                             int *pxopal, boolean upscale);

int get_tgt_gamma(int ipal, int opal);

boolean resize_layer_full(weed_layer_t *layer, int width, int height,
                          LiVESInterpType interp, int opal_hint, int oclamp_hint,
                          int osamp_hint, int osubs_hint, int tgt_gamma);

boolean resize_layer(weed_layer_t *, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint);

boolean letterbox_layer(weed_layer_t *, int nwidth, int nheight, int width, int height, LiVESInterpType interp, int tpal,
                        int tclamp);

boolean unletterbox_layer(weed_layer_t *layer, int opwidth, int opheight, int top, int bottom, int left, int right);

boolean compact_rowstrides(weed_layer_t *);
void gamma_conv_params(int gamma_type, weed_layer_t *inst, boolean is_in);

boolean lives_layer_is_all_black(weed_layer_t *layer, boolean exact);

// pixbuf functions
#define weed_palette_is_pixbuf_palette(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_RGBA32) ? TRUE : FALSE)
boolean lives_pixbuf_is_all_black(LiVESPixbuf *, boolean exact);
void lives_pixbuf_set_opaque(LiVESPixbuf *);

typedef void(*ext_free_func_t)(void *, void *data);

typedef void *(*ext_creator_func_t)(void *pixel_data, uint32_t palette, int width, int height,
                                    int rowstride, ext_free_func_t, void *func_data);

void *lives_pixbuf_new_from_data_wrapper(void *pixel_data, int pal, int width, int height,
    int rowstride, ext_free_func_t ext_free_func, void *func_data);

void *layer_to_extern(lives_layer_t *, ext_creator_func_t, boolean realpalette, boolean fordisp);

LiVESPixbuf *layer_to_pixbuf(weed_layer_t *, boolean realpalette, boolean fordisp);

lives_result_t pixbuf_to_layer(weed_layer_t *, LiVESPixbuf *) WARN_UNUSED;

typedef struct {
  // struct for quickly comparing (planar) layer pdata
  // uses - compare two layers
  // check if layer is blank
  // equate undecoded layer with decoded layer
  // blank row removal
  // count blanks above and below image
  int nrows;
  int width;
  // hash64 of each row
  uint64_t *crows;
  // parity formed by XORing crows
  uint64_t parity;
} lives_row_hash_t;

lives_row_hash_t *hash_cmp_rows(lives_row_hash_t *, int clipno, frames_t frame);
lives_row_hash_t *hash_cmp_layer(lives_layer_t *, lives_row_hash_t *);

/// utility funcs for GUI
int resize_all(int fileno, int width, int height, lives_img_type_t imgtype, boolean do_back, int *nbad, int *nmiss);

#endif
