// colourspace.c
// LiVES
// (c) G. Finch 2004 - 2021 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// code for palette conversions

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

// *
// TODO -
//      - resizing of single plane (including bicubic) (maybe just triplicate the values and pretend it's RGB)
//      - external plugins for palette conversion, resizing
//      - RGB(A) float, YUV10, etc.

#include <math.h>

#include "main.h"

boolean weed_palette_is_sane(int pal);

#define USE_THREADS 1 ///< set to 0 to disable threading for pixbuf operations, 1 to enable. Other values are invalid.

#define USE_BIGBLOCKS 1

#ifdef USE_SWSCALE

#ifdef FF_API_PIX_FMT
typedef enum PixelFormat swpixfmt;
#else
typedef enum AVPixelFormat swpixfmt;
#endif

#if USE_SWSCALE
#include <pthread.h>
typedef struct {
  volatile boolean in_use;
  int num;
  int offset;
  int iwidth, iheight;
  int irow[4];
  swpixfmt ipixfmt;
  int width, height;
  int orow[4];
  swpixfmt opixfmt;
  int flags;
  int subspace;
  int iclamping, oclamp_hint;
  boolean match;
} swsctx_block;

#define MAX_SWS_BLOCKS 8192
#define MAX_SWS_CTX 65536

static volatile int nb = 0;
static volatile int swctx_count = 0;
static swsctx_block bloxx[MAX_SWS_BLOCKS];
static struct SwsContext *swscalep[MAX_SWS_CTX];
static pthread_mutex_t ctxcnt_mutex = PTHREAD_MUTEX_INITIALIZER;

static swsctx_block *sws_getblock(int nreq, int iwidth, int iheight, int *irow, swpixfmt ipixfmt, int width, int height,
                                  int *orow, swpixfmt opixfmt, int flags, int subspace, int iclamping, int oclamp_hint) {
  swsctx_block *block, *bestblock;
  int max = MAX_THREADS + 1, minbnum = max, mingnum = minbnum, minanum = mingnum, num;
  int i = -1, lastblock = THREADVAR(last_sws_block), j = 0, bestidx = -1;

  if (lastblock >= 0) j = lastblock;
  else i = 0;

  pthread_mutex_lock(&ctxcnt_mutex);

  for (; i < nb; j = ++i) {
    block = &bloxx[j];
    if (!block->in_use && (num = block->num) >= nreq) {
      if (iwidth == block->iwidth
          && iheight == block->iheight
          && ipixfmt == block->ipixfmt
          && width == block->width
          && height == block->height
          && opixfmt == block->opixfmt
          && flags == block->flags) {

        if (subspace == block->subspace
            && iclamping == block->iclamping
            && oclamp_hint == block->oclamp_hint
            && irow[0] == block->irow[0]
            && irow[1] == block->irow[1]
            && irow[2] == block->irow[2]
            && irow[3] == block->irow[3]
            && orow[0] == block->orow[0]
            && orow[1] == block->orow[1]
            && orow[2] == block->orow[2]
            && orow[3] == block->orow[3]
           ) {
          if (num < minbnum) {
            minbnum = num;
            bestidx = j;
            //g_print("%d is perfect match !\n", i);
            if (num == nreq) {
              //if (i == -1) g_print("BINGO !\n");
              break;
            }
          }
        } else {
          if (minbnum == max) {
            if (num < mingnum) {
              bestidx = j;
              mingnum = num;
	    // *INDENT-OFF*
	    }}}}
      else {
	if (minbnum == max && mingnum == max) {
	  if (num < minanum) {
	    bestidx = j;
	    minanum = num;
	}}}}}
  // *INDENT-ON*

  if (minbnum < max || mingnum < max) {
    bestblock = &bloxx[bestidx];
    bestblock->in_use = TRUE;
    pthread_mutex_unlock(&ctxcnt_mutex);
    bestblock->match = TRUE;
    THREADVAR(last_sws_block) = bestidx;
  } else {
    int startctx = swctx_count, endctx = startctx + nreq;
    if (endctx >= MAX_SWS_CTX
        || nb  >= MAX_SWS_BLOCKS - 1) {
      if (bestidx == -1) lives_abort("Unable to allocate SWS context block");
      bestblock = &bloxx[bestidx];
      bestblock->in_use = TRUE;
      pthread_mutex_unlock(&ctxcnt_mutex);
      THREADVAR(last_sws_block) = bestidx;
    } else {
      bestblock = &bloxx[nb++];
      bestblock->in_use = TRUE;
      swctx_count = endctx;
      pthread_mutex_unlock(&ctxcnt_mutex);

      bestblock->num = nreq;
      bestblock->offset = startctx;
      for (i = startctx; i < endctx; i++) swscalep[i] = NULL;
    }

    bestblock->iwidth = iwidth;
    bestblock->iheight = iheight;
    bestblock->ipixfmt = ipixfmt;
    bestblock->width = width;
    bestblock->height = height;
    bestblock->opixfmt = opixfmt;
    bestblock->flags = flags;

    bestblock->subspace = subspace;
    bestblock->iclamping = iclamping;
    bestblock->oclamp_hint = oclamp_hint;
    for (i = 0; i < 4; i++) {
      bestblock->irow[i] = irow[i];
      bestblock->orow[i] = orow[i];
    }
    bestblock->match = FALSE;
  }

  //g_print("NCTX = %d\n", swctx_count);
  return bestblock;
}

LIVES_LOCAL_INLINE void sws_freeblock(swsctx_block * block) {
  block->in_use = FALSE;
}

#else
static struct SwsContext *swscale = NULL;
#endif

#endif // USE_SWSCALE

#include "cvirtual.h"
#include "effects-weed.h"

static boolean unal_inited = FALSE;

#ifdef GUI_GTK
// from gdk-pixbuf.c
/* Always align rows to 32-bit boundaries */
# define get_pixbuf_rowstride_value(rowstride) ((rowstride + 3) & ~3)
#else
# define get_pixbuf_rowstride_value(rowstride) (rowstride)
#endif

#ifdef GUI_GTK
// from gdkpixbuf
#define get_last_pixbuf_rowstride_value(width, nchans) ((((width * nchans) << 3) + 7) >> 3)
#else
#define get_last_pixbuf_rowstride_value(width, nchans) (width * nchans)
#endif


static void lives_free_buffer(uint8_t *pixels, livespointer data) {
  lives_free(pixels);
}

#define CLAMP0255(a)  ((unsigned char)((((-a) >> 31) & a) | (255 - a) >> 31) )
#define CLAMP0255f(a)  (a > 255. ? 255.: a < 0. ? 0. : a)
#define CLAMP0255fi(a)  ((int)(a > 255. ? 255.: a < 0. ? 0. : a))

static inline uint8_t clamp0255f(float f) {
  if (f > 255.) f = 255.;
  if (f < 0.) f = 0;
  return f;
}


/* precomputed tables */

// generic
static int *Y_R;
static int *Y_G;
static int *Y_B;
static int *Cb_R;
static int *Cb_G;
static int *Cb_B;
static int *Cr_R;
static int *Cr_G;
static int *Cr_B;

static float *Yf_R;
static float *Yf_G;
static float *Yf_B;
static float *Cbf_R;
static float *Cbf_G;
static float *Cbf_B;
static float *Crf_R;
static float *Crf_G;
static float *Crf_B;

// clamped Y'CbCr
static int Y_Rc[256];
static int Y_Gc[256];
static int Y_Bc[256];
static int Cb_Rc[256];
static int Cb_Gc[256];
static int Cb_Bc[256];
static int Cr_Rc[256];
static int Cr_Gc[256];
static int Cr_Bc[256];

static float Yf_Rc[256];
static float Yf_Gc[256];
static float Yf_Bc[256];

#if 0
static float Cbf_Rc[256];
static float Cbf_Gc[256];
static float Cbf_Bc[256];
static float Crf_Rc[256];
static float Crf_Gc[256];
static float Crf_Bc[256];
#endif

// unclamped Y'CbCr
static int Y_Ru[256];
static int Y_Gu[256];
static int Y_Bu[256];
static int Cb_Ru[256];
static int Cb_Gu[256];
static int Cb_Bu[256];
static int Cr_Ru[256];
static int Cr_Gu[256];
static int Cr_Bu[256];

// clamped BT.709
static int HY_Rc[256];
static int HY_Gc[256];
static int HY_Bc[256];
static int HCb_Rc[256];
static int HCb_Gc[256];
static int HCb_Bc[256];
static int HCr_Rc[256];
static int HCr_Gc[256];
static int HCr_Bc[256];

// float - experimental
static float HYf_Rc[256];
static float HYf_Gc[256];
static float HYf_Bc[256];
static float HCbf_Rc[256];
static float HCbf_Gc[256];
static float HCbf_Bc[256];
static float HCrf_Rc[256];
static float HCrf_Gc[256];
static float HCrf_Bc[256];


// unclamped BT.709
static int HY_Ru[256];
static int HY_Gu[256];
static int HY_Bu[256];
static int HCb_Ru[256];
static int HCb_Gu[256];
static int HCb_Bu[256];
static int HCr_Ru[256];
static int HCr_Gu[256];
static int HCr_Bu[256];

static float HYf_Ru[256];
static float HYf_Gu[256];
static float HYf_Bu[256];
static float HCbf_Ru[256];
static float HCbf_Gu[256];
static float HCbf_Bu[256];
static float HCrf_Ru[256];
static float HCrf_Gu[256];
static float HCrf_Bu[256];

static boolean conv_RY_inited = FALSE;

// generic
static int *RGB_Y;
static int *R_Cr;
static int *G_Cb;
static int *G_Cr;
static int *B_Cb;

static float *RGBf_Y;
static float *Rf_Cr;
static float *Gf_Cb;
static float *Gf_Cr;
static float *Bf_Cb;

// clamped Y'CbCr
static int RGB_Yc[256];
static int R_Crc[256];
static int G_Cbc[256];
static int G_Crc[256];
static int B_Cbc[256];

// unclamped Y'CbCr
static int RGB_Yu[256];
static int R_Cru[256];
static int G_Cru[256];
static int G_Cbu[256];
static int B_Cbu[256];

// clamped BT.709
static int HRGB_Yc[256];
static int HR_Crc[256];
static int HG_Crc[256];
static int HG_Cbc[256];
static int HB_Cbc[256];

static float HRGBf_Yc[256];
static float HRf_Crc[256];
static float HGf_Crc[256];
static float HGf_Cbc[256];
static float HBf_Cbc[256];

// unclamped BT.709
static int HRGB_Yu[256];
static int HR_Cru[256];
static int HG_Cru[256];
static int HG_Cbu[256];
static int HB_Cbu[256];

static float HRGBf_Yu[256];
static float HRf_Cru[256];
static float HGf_Cru[256];
static float HGf_Cbu[256];
static float HBf_Cbu[256];

static boolean conv_YR_inited = FALSE;

static short min_Y, max_Y, min_UV, max_UV;

// averaging
static uint8_t *cavg;
static uint8_t cavgc[256][256];
static uint8_t cavgu[256][256];
static uint8_t cavgrgb[256][256];
static boolean avg_inited = FALSE;

// pre-post multiply alpha
static int unal[256][256];
static int al[256][256];
static int unalcy[256][256];
static int alcy[256][256];
static int unalcuv[256][256];
static int alcuv[256][256];

// clamping and subspace converters

// generic
static uint8_t *Y_to_Y;
static uint8_t *U_to_U;
static uint8_t *V_to_V;

// same subspace, clamped to unclamped
static uint8_t Yclamped_to_Yunclamped[256];
static uint8_t UVclamped_to_UVunclamped[256];

// same subspace, unclamped to clamped
static uint8_t Yunclamped_to_Yclamped[256];
static uint8_t UVunclamped_to_UVclamped[256];

static boolean conv_YY_inited = FALSE;


// gamma correction

/// linear -> gamma:
// x <= b ?  x * c : (a + 1) * powf(x, 1 / G) - a

/// gamma to linear:
// inv: x < d ? x / c : powf((x + a) / (a + 1), G)

/// b = d / c

// for sRGB:
// a = 0.055, b = 0.0031308, c = 12.92, d = 0.04045, G = 2.4

/// for bt709:
/// a = 0.099, b = 0.018, c = 4.5, d = 0.081, G = 2.22222222

static gamma_const_t gamma_tx[3];

static uint8_t *gamma_s2l = NULL;
static uint8_t *gamma_l2s = NULL;
static uint8_t *gamma_b2l = NULL;
static uint8_t *gamma_l2b = NULL;
static uint8_t *gamma_s2b = NULL;
static uint8_t *gamma_b2s = NULL;

static inline uint8_t *create_gamma_lut(double fileg, int gamma_from, int gamma_to) {
  uint8_t *gamma_lut = NULL;
  float inv_gamma = 0.;
  float a, x = 0.;

  if (fileg == 1.0) {
    if (gamma_to == gamma_from || gamma_to == WEED_GAMMA_UNKNOWN
        || gamma_from == WEED_GAMMA_UNKNOWN) return NULL;
    if (gamma_from == WEED_GAMMA_LINEAR && gamma_to == WEED_GAMMA_SRGB && gamma_l2s) gamma_lut = gamma_l2s;
    else if (gamma_from == WEED_GAMMA_LINEAR && gamma_to == WEED_GAMMA_BT709 && gamma_l2b) gamma_lut = gamma_l2b;
    else if (gamma_from == WEED_GAMMA_SRGB && gamma_to == WEED_GAMMA_LINEAR && gamma_s2l) gamma_lut = gamma_s2l;
    else if (gamma_from == WEED_GAMMA_SRGB && gamma_to == WEED_GAMMA_BT709 && gamma_s2b) gamma_lut = gamma_s2b;
    else if (gamma_from == WEED_GAMMA_BT709 && gamma_to == WEED_GAMMA_LINEAR && gamma_b2l) gamma_lut = gamma_b2l;
    else if (gamma_from == WEED_GAMMA_BT709 && gamma_to == WEED_GAMMA_SRGB && gamma_b2s) gamma_lut = gamma_b2s;
    if (gamma_lut) return gamma_lut;
  }

  gamma_lut = lives_calloc(4, 64);
  if (!gamma_lut) return NULL;

  if (gamma_to == WEED_GAMMA_MONITOR) {
    inv_gamma = 1. / (float)prefs->screen_gamma;
  }

  gamma_lut[0] = 0;

  for (int i = 1; i < 256; ++i) {
    /* if (gamma_from == gamma_to && fileg == 1.0) { */
    /*   gamma_lut[i] = i; */
    /*   continue; */
    /* } */

    x = a = (float)i / 255.;

    if (fileg != 1.0) {
      x = powf(a, fileg);
    }

    if (1) {
      switch (gamma_to) {
      // simple power law transformation
      case WEED_GAMMA_MONITOR:
      case WEED_GAMMA_SRGB:
        // sRGB gamma
        switch (gamma_from) {
        case WEED_GAMMA_BT709:
          // conv to linear first
          a = (a < gamma_tx[WEED_GAMMA_BT709].thresh) ? a / gamma_tx[WEED_GAMMA_BT709].lin
              : powf((a + gamma_tx[WEED_GAMMA_BT709].offs) / (1. + gamma_tx[WEED_GAMMA_BT709].offs),
                     gamma_tx[WEED_GAMMA_BT709].pf);
        case WEED_GAMMA_LINEAR:
          x = (a < (gamma_tx[WEED_GAMMA_SRGB].thresh) / gamma_tx[WEED_GAMMA_SRGB].lin)
              ? a * gamma_tx[WEED_GAMMA_SRGB].lin
              : powf((1. + gamma_tx[WEED_GAMMA_SRGB].offs) * a,
                     1. / gamma_tx[WEED_GAMMA_SRGB].pf) - gamma_tx[WEED_GAMMA_SRGB].offs;

          if (gamma_to == WEED_GAMMA_MONITOR)
            x = powf(a, inv_gamma);
          break;
        case WEED_GAMMA_MONITOR:
          x = powf(a, prefs->screen_gamma);
          break;
        default:
          break;
        }
        break;

      case WEED_GAMMA_LINEAR:
        switch (gamma_from) {
        case WEED_GAMMA_MONITOR:
          x = powf(a, prefs->screen_gamma);
          break;
        case WEED_GAMMA_SRGB:
          x = (a < gamma_tx[WEED_GAMMA_SRGB].thresh) ? a / gamma_tx[WEED_GAMMA_SRGB].lin
              : powf((a + gamma_tx[WEED_GAMMA_SRGB].offs) / (1. + gamma_tx[WEED_GAMMA_SRGB].offs),
                     gamma_tx[WEED_GAMMA_SRGB].pf);
          break;
        case WEED_GAMMA_BT709:
          x = (a < gamma_tx[WEED_GAMMA_BT709].thresh) ? a / gamma_tx[WEED_GAMMA_BT709].lin
              : powf((a + gamma_tx[WEED_GAMMA_BT709].offs) / (1. + gamma_tx[WEED_GAMMA_BT709].offs),
                     gamma_tx[WEED_GAMMA_BT709].pf);
          break;
        default:
          break;
        }
      // rec 709 gamma
      case WEED_GAMMA_BT709:
        switch (gamma_from) {
        case WEED_GAMMA_MONITOR:
          x = powf(a, prefs->screen_gamma);
          break;
        case WEED_GAMMA_SRGB:
          // convert first to linear
          a = (a < gamma_tx[WEED_GAMMA_SRGB].thresh) ? a / gamma_tx[WEED_GAMMA_SRGB].lin
              : powf((a + gamma_tx[WEED_GAMMA_SRGB].offs) / (1. + gamma_tx[WEED_GAMMA_SRGB].offs),
                     gamma_tx[WEED_GAMMA_SRGB].pf);
        case WEED_GAMMA_LINEAR:
          x = (a < (gamma_tx[WEED_GAMMA_BT709].thresh) / gamma_tx[WEED_GAMMA_BT709].lin)
              ? a * gamma_tx[WEED_GAMMA_BT709].lin
              : powf((1. + gamma_tx[WEED_GAMMA_BT709].offs) * a,
                     1. / gamma_tx[WEED_GAMMA_BT709].pf) - gamma_tx[WEED_GAMMA_BT709].offs;
        default:
          break;
        }
        break;

      default:
        break;
      }
    }
    gamma_lut[i] = CLAMP0255((int32_t)(255. * x + .5));
  }
  if (fileg == 1.) {
    if (gamma_from == WEED_GAMMA_LINEAR && gamma_to == WEED_GAMMA_SRGB && !gamma_l2s)
      gamma_l2s = gamma_lut;
    if (gamma_from == WEED_GAMMA_LINEAR && gamma_to == WEED_GAMMA_BT709 && !gamma_l2b)
      gamma_l2b = gamma_lut;
    if (gamma_from == WEED_GAMMA_SRGB && gamma_to == WEED_GAMMA_LINEAR && !gamma_s2l)
      gamma_s2l = gamma_lut;
    if (gamma_from == WEED_GAMMA_SRGB && gamma_to == WEED_GAMMA_BT709 && !gamma_s2b)
      gamma_s2b = gamma_lut;
    if (gamma_from == WEED_GAMMA_BT709 && gamma_to == WEED_GAMMA_LINEAR && !gamma_b2l)
      gamma_b2l = gamma_lut;
    if (gamma_from == WEED_GAMMA_BT709 && gamma_to == WEED_GAMMA_SRGB && !gamma_b2s)
      gamma_b2s = gamma_lut;
  }
  return gamma_lut;
}

static inline void lives_gamma_lut_free(uint8_t *lut) {
  if (lut && lut != gamma_l2s && lut != gamma_l2b && lut != gamma_s2l && lut != gamma_s2b
      && lut != gamma_b2s && lut != gamma_b2l) lives_free(lut);
}


static inline int32_t _spc_rnd(int32_t val, short quality) {
  // if USE_EXTEND is defined,
  // instead of shifting right by 16 bits, we multiplied x by scale_factor, ie., 0xFF -> 0xFFFFFF
  // to convert back we can either shift right 16 bits (less accurate), or divide by scale_factor
  // i.e divide by 65793 (the default)
  // We note that 65793 == 241 * 273  = (256 - 16 + 1) * (256 + 16 + 1)
  // val = x / (256 - 16 + 1)  / (256 + 16 - 1)
  // = x . (16 + 1) / ((256 - 16 + 1) . (16 + 1))  * (16 - 1) / ((256 + 16 + 1) . (16 - 1))
  // = x. (16 + 1) / (256 . 16 + 256 - 256 - 16 + 16 + 1)   * (16 - 1) / (256 * 16 - 256 + 256 - 16 + 16 - 1)
  // (x . 16 + x) / (256 . 16 + 1) . (16 - 1) . (256 * 16 - 1) ~= ((x << 4) + x) >> 12 * (16 - 1) / 256 * 16

  // let a = (x << 4) + x

  // (((a >> 12) << 4) - a >> 12) >> 12
  // ((((x >> 8 + x >> 12) << 4 - x >> 8 - x >> 12) >> 12

  // (x >> 4 + x >> 8 - x >> 8 - x >> 12) >> 12
  // (x >> 4 - x >> 12) >> 12
  // i.e (x  - (x >> 8)) >> 16
  // the net effect is that the highest bit is subtracted from the bit below the lsb.
  // e.g. 0xFFFFFF -> 0XFFFFFF - 0X00FFFF >> 16 = 0xFF
  // 0xA1B2C3 -> 0XA1B2C3 - 0x00A1B2 = 0XA11111 >> 16 = 0xA1
  // but: 0xB1A1A1 -> 0xB1A1A1 - 0x00B1A1 = 0xB0F000 >> 16 = 0xB0
  // i.e the lowest bit is rounded rather than simply truncated:
  // if we are adding several factors we can do the conversion after the addition
  // i.e the rounding error when converting from RGB to YUV goes from 1. / 255. ~= 0.4 % to half of that, i.e 0.2 %

  if (quality == PB_QUALITY_LOW) {
    return val >> FP_BITS;
  }
  if (quality == PB_QUALITY_MED) {
    uint32_t sig = val & 0x80000000;
    return (((val - (val >> 8)) >> 16) | sig);
  }
  return ((float)val / SCALE_FACTOR + .5);
}


#define spc_rnd(val) (_spc_rnd((val), prefs ? prefs->pb_quality : PB_QUALITY_HIGH))


LIVES_GLOBAL_INLINE int32_t round_special(int32_t val) {
  return spc_rnd(val);
}


static void init_RGB_to_YUV_tables(void) {
  register int i;
  // Digital Y'UV proper [ITU-R BT.601-5] for digital NTSC (NTSC analog uses YIQ I think)
  // a.k.a CCIR 601, aka bt470bg (with gamma = 2.8 ?), bt470m (gamma = 2.2), aka SD
  // uses Kr = 0.299 and Kb = 0.114
  // offs U,V = 128

  // (I call this subspace YUV_SUBSPACE_YCBCR)

  // this is used for e.g. theora encoding, and for most video cards

  // input is linear RGB, output is gamma corrected Y'UV

  // bt.709 (HD)

  // input is linear RGB, output is gamma corrected Y'UV

  // except for bt2020 which gamma corrects the Y (only) after conversion (?)

  // there is also smpte 170 / smpte 240 (NTSC), bt.1886 (?), smpte2084, and bt2020

  // bt.1886 : gamma 2.4

  // bt2020: UHD, 10/12 bit colour

  double fac;

  for (i = 0; i < 256; i++) {
    Y_Rc[i] = myround(KR_YCBCR * (double)i * CLAMP_FACTOR_Y * SCALE_FACTOR);   // Kr
    Y_Gc[i] = myround((1. - KR_YCBCR - KB_YCBCR) * (double)i * CLAMP_FACTOR_Y * SCALE_FACTOR);   // Kb
    Y_Bc[i] = myround((KB_YCBCR * (double)i * CLAMP_FACTOR_Y + YUV_CLAMP_MIN) * SCALE_FACTOR);

    fac = .5 / (1. - KB_YCBCR); // .564

    Cb_Rc[i] = myround(-fac * KR_YCBCR * (double)i * CLAMP_FACTOR_UV  * SCALE_FACTOR); // -.16736
    Cb_Gc[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR)  * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR); // -.331264
    Cb_Bc[i] = myround((0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS) * SCALE_FACTOR);

    fac = .5 / (1. - KR_YCBCR); // .713

    Cr_Rc[i] = myround((0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS) * SCALE_FACTOR);
    Cr_Gc[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR) * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR);
    Cr_Bc[i] = myround(-fac * KB_YCBCR * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR);
  }

  for (i = 0; i < 256; i++) {
    Y_Ru[i] = myround(KR_YCBCR * (double)i * SCALE_FACTOR);   // Kr
    Y_Gu[i] = myround((1. - KR_YCBCR - KB_YCBCR) * (double)i * SCALE_FACTOR);   // Kb
    Y_Bu[i] = myround(KB_YCBCR * (double)i * SCALE_FACTOR);

    fac = .5 / (1. - KB_YCBCR); // .564

    Cb_Ru[i] = myround(-fac * KR_YCBCR * (double)i * SCALE_FACTOR); // -.16736
    Cb_Gu[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR)  * (double)i * SCALE_FACTOR); // -.331264
    Cb_Bu[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);

    fac = .5 / (1. - KR_YCBCR); // .713

    Cr_Ru[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);
    Cr_Gu[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR) * (double)i * SCALE_FACTOR);
    Cr_Bu[i] = myround(-fac * KB_YCBCR * (double)i * SCALE_FACTOR);
  }

  // Different values are used for hdtv, I call this subspace YUV_SUBSPACE_BT709

  // Kr = 0.2126
  // Kb = 0.0722

  // converting from one subspace to another is not recommended.

  for (i = 0; i < 256; i++) {
    HY_Rc[i] = myround(KR_BT709 * (double)i * CLAMP_FACTOR_Y * SCALE_FACTOR);   // Kr
    HY_Gc[i] = myround((1. - KR_BT709 - KB_BT709) * (double)i * CLAMP_FACTOR_Y * SCALE_FACTOR);   // Kb
    HY_Bc[i] = myround((KB_BT709 * (double)i * CLAMP_FACTOR_Y + YUV_CLAMP_MIN) * SCALE_FACTOR);

    HYf_Rc[i] = KR_BT709 * (double)i * CLAMP_FACTOR_Y;
    HYf_Gc[i] = (1. - KR_BT709 - KB_BT709) * (double)i * CLAMP_FACTOR_Y;
    HYf_Bc[i] = KB_BT709 * (double)i * CLAMP_FACTOR_Y + YUV_CLAMP_MIN;

    fac = .5 / (1. - KB_BT709);

    HCb_Rc[i] = myround(-fac * KR_BT709 * (double)i * CLAMP_FACTOR_UV  * SCALE_FACTOR); // -.16736
    HCb_Gc[i] = myround(-fac * (1. - KB_BT709 - KR_BT709)  * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR); // -.331264
    HCb_Bc[i] = myround((0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS) * SCALE_FACTOR);

    HCbf_Rc[i] = -fac * KR_BT709 * (double)i * CLAMP_FACTOR_UV;
    HCbf_Gc[i] = -fac * (1. - KB_BT709 - KR_BT709)  * (double)i * CLAMP_FACTOR_UV;
    HCbf_Bc[i] = 0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS;

    fac = .5 / (1. - KR_BT709);

    HCr_Rc[i] = myround((0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS) * SCALE_FACTOR);
    HCr_Gc[i] = myround(-fac * (1. - KB_BT709 - KR_BT709) * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR);
    HCr_Bc[i] = myround(-fac * KB_BT709 * (double)i * CLAMP_FACTOR_UV * SCALE_FACTOR);

    HCrf_Rc[i] = 0.5 * (double)i * CLAMP_FACTOR_UV + UV_BIAS;
    HCrf_Gc[i] = -fac * (1. - KB_BT709 - KR_BT709) * (double)i * CLAMP_FACTOR_UV;
    HCrf_Bc[i] = -fac * KB_BT709 * (double)i * CLAMP_FACTOR_UV;
  }

  for (i = 0; i < 256; i++) {
    HY_Ru[i] = myround(KR_BT709 * (double)i * SCALE_FACTOR);   // Kr
    HY_Gu[i] = myround((1. - KR_BT709 - KB_BT709) * (double)i * SCALE_FACTOR);   // Kb
    HY_Bu[i] = myround(KB_BT709 * (double)i * SCALE_FACTOR);

    HYf_Ru[i] = KR_BT709 * (double)i;
    HYf_Gu[i] = (1. - KR_BT709 - KB_BT709) * (double)i;   // Kb
    HYf_Bu[i] = KB_BT709 * (double)i;

    fac = .5 / (1. - KB_BT709);

    HCb_Ru[i] = myround(-fac * KR_BT709 * (double)i * SCALE_FACTOR); // -.16736
    HCb_Gu[i] = myround(-fac * (1. - KB_BT709 - KR_BT709)  * (double)i * SCALE_FACTOR); // -.331264
    HCb_Bu[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);

    HCbf_Ru[i] = -fac * KR_BT709 * (double)i;
    HCbf_Gu[i] = -fac * (1. - KB_BT709 - KR_BT709)  * (double)i;
    HCbf_Bu[i] = 0.5 * (double)i + UV_BIAS;

    fac = .5 / (1. - KR_BT709);

    HCr_Ru[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);
    HCr_Gu[i] = myround(-fac * (1. - KB_BT709 - KR_BT709) * (double)i * SCALE_FACTOR);
    HCr_Bu[i] = myround(-fac * KB_BT709 * (double)i * SCALE_FACTOR);

    HCrf_Ru[i] = 0.5 * (double)i + UV_BIAS;
    HCrf_Gu[i] = -fac * (1. - KB_BT709 - KR_BT709) * (double)i;
    HCrf_Bu[i] = -fac * KB_BT709 * (double)i;
  }

  conv_RY_inited = TRUE;
}

static void init_YUV_to_RGB_tables(void) {
  register int i;

  // These values are for what I call YUV_SUBSPACE_YCBCR

  /* clip Y values under 16 */
  for (i = 0; i <= YUV_CLAMP_MINI; i++) RGB_Yc[i] = 0;

  for (; i < Y_CLAMP_MAXI; i++) {
    RGB_Yc[i] = myround(((double)i - YUV_CLAMP_MIN) / (Y_CLAMP_MAX - YUV_CLAMP_MIN) * 255. * SCALE_FACTOR);
  }
  /* clip Y values above 235 */
  for (; i < 256; i++) RGB_Yc[i] = 255 * SCALE_FACTOR;

  /* clip Cb/Cr values below 16 */
  for (i = 0; i <= YUV_CLAMP_MINI; i++) R_Crc[i] = G_Crc[i] = G_Cbc[i] = B_Cbc[i] = 0;

  for (; i < UV_CLAMP_MAXI; i++) {
    R_Crc[i] = myround(2. * (1. - KR_YCBCR) * ((((double)i - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)

    G_Cbc[i] = myround(-.5 / (1. + KB_YCBCR + KR_YCBCR) * ((((double)i - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);

    G_Crc[i] = myround(-.5 / (1. - KR_YCBCR) * ((((double)i - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);

    B_Cbc[i] = myround(2. * (1. - KB_YCBCR) * ((((double)i - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)
  }
  /* clip Cb/Cr values above 240 */
  for (; i < 256; i++) {
    R_Crc[i] = myround(2. * (1. - KR_YCBCR) * (((UV_CLAMP_MAX - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)
    G_Crc[i] = myround(-.5 / (1. - KR_YCBCR) * (((UV_CLAMP_MAX - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);
    G_Cbc[i] = myround(-.5 / (1. + KB_YCBCR + KR_YCBCR) * (((UV_CLAMP_MAX - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);
    B_Cbc[i] = myround(2. * (1. - KB_YCBCR) * (((UV_CLAMP_MAX - YUV_CLAMP_MIN) /
                       (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)
  }

  // unclamped Y'CbCr
  for (i = 0; i <= 255; i++) {
    RGB_Yu[i] = i * SCALE_FACTOR;
  }

  for (i = 0; i <= 255; i++) {
    R_Cru[i] = myround(2. * (1. - KR_YCBCR) * ((double)i - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)
    G_Cru[i] = myround(-.5 / (1. - KR_YCBCR) * ((double)i - UV_BIAS) * SCALE_FACTOR);
    G_Cbu[i] = myround(-.5 / (1. + KB_YCBCR + KR_YCBCR) * ((double)i - UV_BIAS) * SCALE_FACTOR);
    B_Cbu[i] = myround(2. * (1. - KB_YCBCR) * ((double)i - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)
  }

  // These values are for what I call YUV_SUBSPACE_BT709

  /* clip Y values under 16 */
  for (i = 0; i <= YUV_CLAMP_MINI; i++) HRGB_Yc[i] = 0;
  for (i = 0; i <= YUV_CLAMP_MINI; i++) HRGBf_Yc[i] = 0.;

  for (; i < Y_CLAMP_MAXI; i++) {
    HRGB_Yc[i] = myround(((double)i - YUV_CLAMP_MIN) / (Y_CLAMP_MAX - YUV_CLAMP_MIN) * 255. * SCALE_FACTOR);
    HRGBf_Yc[i] = ((double)i - YUV_CLAMP_MIN) / (Y_CLAMP_MAX - YUV_CLAMP_MIN) * 255.;
  }

  /* clip Y values above 235 */
  for (; i < 256; i++) HRGB_Yc[i] = 255 * SCALE_FACTOR;
  for (; i < 256; i++) HRGBf_Yc[i] = 255.;

  /* clip Cb/Cr values below 16 */
  for (i = 0; i <= YUV_CLAMP_MINI; i++) HR_Crc[i] = HG_Crc[i] = HG_Cbc[i] = HB_Cbc[i] = 0;
  for (i = 0; i <= YUV_CLAMP_MINI; i++) HRf_Crc[i] = HGf_Crc[i] = HGf_Cbc[i] = HBf_Cbc[i] = 0.;

  for (; i < UV_CLAMP_MAXI; i++) {
    HR_Crc[i] = myround(2. * (1. - KR_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                        (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)
    HG_Crc[i] = myround(-.5 / (1. - KR_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                        (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);
    HG_Cbc[i] = myround(-.5 / (1. + KB_BT709 + KB_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                        (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR);
    HB_Cbc[i] = myround(2. * (1. - KB_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                        (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)

    HRf_Crc[i] = 2. * (1. - KR_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                                          (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS);
    HGf_Crc[i] = -.5 / (1. - KR_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                                           (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS);
    HGf_Cbc[i] = -.5 / (1. + KB_BT709 + KB_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                 (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS);
    HBf_Cbc[i] = 2. * (1. - KB_BT709) * ((((double)i - YUV_CLAMP_MIN) /
                                          (UV_CLAMP_MAX - YUV_CLAMP_MIN) * 255.) - UV_BIAS);
  }
  /* clip Cb/Cr values above 240 */
  for (; i < 256; i++) {
    HR_Crc[i] = myround(2. * (1. - KR_BT709) * (255. - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)
    HG_Crc[i] = myround(-.5 / (1. - KR_BT709) * (255. - UV_BIAS) * SCALE_FACTOR);
    HG_Cbc[i] = myround(-.5 / (1. + KB_BT709 + KB_BT709) * (255. - UV_BIAS) * SCALE_FACTOR);
    HB_Cbc[i] = myround(2. * (1. - KB_BT709) * (255. - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)

    HRf_Crc[i] = 2. * (1. - KR_BT709) * (254. - UV_BIAS);
    HGf_Crc[i] = -.5 / (1. - KR_BT709) * (254. - UV_BIAS);
    HGf_Cbc[i] = -.5 / (1. + KB_BT709 + KB_BT709) * (254. - UV_BIAS);
    HBf_Cbc[i] = 2. * (1. - KB_BT709) * (254. - UV_BIAS);
  }

  // unclamped Y'CbCr
  for (i = 0; i <= 255; i++) HRGB_Yu[i] = i * SCALE_FACTOR;
  for (i = 0; i <= 255; i++) HRGBf_Yu[i] = i;

  for (i = 0; i <= 255; i++) {
    HR_Cru[i] = myround(2. * (1. - KR_BT709) * ((double)i - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kr)
    HG_Cru[i] = myround(-.5 / (1. - KR_BT709) * ((double)i - UV_BIAS) * SCALE_FACTOR);
    HG_Cbu[i] = myround(-.5 / (1. + KB_BT709 + KB_BT709) * ((double)i - UV_BIAS) * SCALE_FACTOR);
    HB_Cbu[i] = myround(2. * (1. - KB_BT709) * ((double)i - UV_BIAS) * SCALE_FACTOR); // 2*(1-Kb)

    HRf_Cru[i] = 2. * (1. - KR_BT709) * ((double)i - UV_BIAS);
    HGf_Cru[i] = -.5 / (1. - KR_BT709) * ((double)i - UV_BIAS);
    HGf_Cbu[i] = -.5 / (1. + KB_BT709 + KB_BT709) * ((double)i - UV_BIAS);
    HBf_Cbu[i] = 2. * (1. - KB_BT709) * ((double)i - UV_BIAS);
  }
  conv_YR_inited = TRUE;
}


static void init_YUV_to_YUV_tables(void) {
  register int i;

  // init clamped -> unclamped, same subspace
  for (i = 0; i <= YUV_CLAMP_MINI; i++) {
    Yclamped_to_Yunclamped[i] = 0;
  }
  for (; i < Y_CLAMP_MAXI; i++) {
    Yclamped_to_Yunclamped[i] = myround((i - YUV_CLAMP_MIN) * 255. / (Y_CLAMP_MAX - YUV_CLAMP_MIN));
  }
  for (; i < 256; i++) {
    Yclamped_to_Yunclamped[i] = 255;
  }

  for (i = 0; i < YUV_CLAMP_MINI; i++) {
    UVclamped_to_UVunclamped[i] = 0;
  }
  for (; i < UV_CLAMP_MAXI; i++) {
    UVclamped_to_UVunclamped[i] = myround((i - YUV_CLAMP_MIN) * 255. / (UV_CLAMP_MAX - YUV_CLAMP_MIN));
  }
  for (; i < 256; i++) {
    UVclamped_to_UVunclamped[i] = 255;
  }

  for (i = 0; i < 256; i++) {
    Yunclamped_to_Yclamped[i] = myround((i / 255.) * (Y_CLAMP_MAX - YUV_CLAMP_MIN) + YUV_CLAMP_MIN);
    UVunclamped_to_UVclamped[i] = myround((i / 255.) * (UV_CLAMP_MAX - YUV_CLAMP_MIN) + YUV_CLAMP_MIN);
  }

  conv_YY_inited = TRUE;
}


static void init_average(void) {
  for (int x = 0; x < 256; x++) {
    float fa = (float)(x - 128.) * 255. / 244.;
    short sa = (short)(x - 128);
    for (int y = 0; y < 256; y++) {
      float fb = (float)(y - 128.) * 255. / 244.;
      short sb = (short)(y - 128);
#ifdef MULT_AVG
      // values mixed in proportion to strength
      float fc = (fa + fb - ((fa * fb) >> 8)) * 224. / 512. + 128.;
      short c = (sa + sb - ((sa * sb) >> 8)) + 128;
#else
      // values mixed equally
      float fc = (fa + fb) * 224. / 512. + 128.;
      short c = ((sa + sb) >> 1) + 128;
#endif
      cavgc[x][y] = (uint8_t)(fc > 240. ? 240 : fc < 16. ? 16 : fc);
      cavgrgb[x][y] = cavgu[x][y] = (uint8_t)(c > 255 ? 255 : c < 0 ? 0 : c);
    }
  }
  avg_inited = TRUE;
}


static void init_unal(void) {
  // premult to postmult and vice-versa
  for (int i = 0; i < 256; i++) { //alpha val
    for (int j = 0; j < 256; j++) { // val to be converted
      unal[i][j] = (float)j * 255. / (float)i;
      al[i][j] = (float)j * (float)i / 255.;

      // clamped versions
      unalcy[i][j] = ((j - YUV_CLAMP_MIN) / (Y_CLAMP_MAX - YUV_CLAMP_MIN)) / (float)i;
      alcy[i][j] = ((j - YUV_CLAMP_MIN) / (Y_CLAMP_MAX - YUV_CLAMP_MIN)) * (float)i;
      unalcuv[i][j] = ((j - YUV_CLAMP_MIN) / (UV_CLAMP_MAX - YUV_CLAMP_MIN)) / (float)i;
      alcuv[i][j] = ((j - YUV_CLAMP_MIN) / (UV_CLAMP_MAX - YUV_CLAMP_MIN)) * (float)i;
    }
  }
  unal_inited = TRUE;
}


static void set_conversion_arrays(int clamping, int subspace) {
  // set conversion arrays for RGB <-> YUV, also min/max YUV values
  // depending on clamping and subspace

  switch (subspace) {
  case WEED_YUV_SUBSPACE_YUV: // assume YCBCR
  case WEED_YUV_SUBSPACE_YCBCR:
    if (clamping == WEED_YUV_CLAMPING_CLAMPED) {
      Y_R = Y_Rc;
      Y_G = Y_Gc;
      Y_B = Y_Bc;

      Yf_R = Yf_Rc;
      Yf_G = Yf_Gc;
      Yf_B = Yf_Bc;

      Cr_R = Cr_Rc;
      Cr_G = Cr_Gc;
      Cr_B = Cr_Bc;

      Cb_R = Cb_Rc;
      Cb_G = Cb_Gc;
      Cb_B = Cb_Bc;

      RGB_Y = RGB_Yc;

      R_Cr = R_Crc;
      G_Cr = G_Crc;

      G_Cb = G_Cbc;
      B_Cb = B_Cbc;
    } else {
      Y_R = Y_Ru;
      Y_G = Y_Gu;
      Y_B = Y_Bu;

      Cr_R = Cr_Ru;
      Cr_G = Cr_Gu;
      Cr_B = Cr_Bu;

      Cb_R = Cb_Ru;
      Cb_G = Cb_Gu;
      Cb_B = Cb_Bu;

      RGB_Y = RGB_Yu;

      R_Cr = R_Cru;
      G_Cr = G_Cru;
      G_Cb = G_Cbu;
      B_Cb = B_Cbu;
    }
    break;
  case WEED_YUV_SUBSPACE_BT709:
    if (clamping == WEED_YUV_CLAMPING_CLAMPED) {
      Y_R = HY_Rc;
      Y_G = HY_Gc;
      Y_B = HY_Bc;

      Yf_R = HYf_Rc;
      Yf_G = HYf_Gc;
      Yf_B = HYf_Bc;

      Cr_R = HCr_Rc;
      Cr_G = HCr_Gc;
      Cr_B = HCr_Bc;

      Crf_R = HCrf_Rc;
      Crf_G = HCrf_Gc;
      Crf_B = HCrf_Bc;

      Cb_R = HCb_Rc;
      Cb_G = HCb_Gc;
      Cb_B = HCb_Bc;

      Cbf_R = HCbf_Rc;
      Cbf_G = HCbf_Gc;
      Cbf_B = HCbf_Bc;

      RGB_Y = HRGB_Yc;
      RGBf_Y = HRGBf_Yc;

      R_Cr = HR_Crc;
      G_Cr = HG_Crc;
      G_Cb = HG_Cbc;
      B_Cb = HB_Cbc;

      Rf_Cr = HRf_Crc;
      Gf_Cr = HGf_Crc;
      Gf_Cb = HGf_Cbc;
      Bf_Cb = HBf_Cbc;
    } else {
      Y_R = HY_Ru;
      Y_G = HY_Gu;
      Y_B = HY_Bu;

      Yf_R = HYf_Ru;
      Yf_G = HYf_Gu;
      Yf_B = HYf_Bu;

      Y_R = HY_Ru;
      Y_G = HY_Gu;
      Y_B = HY_Bu;

      Yf_R = HYf_Ru;
      Yf_G = HYf_Gu;
      Yf_B = HYf_Bu;

      Cr_R = HCr_Ru;
      Cr_G = HCr_Gu;
      Cr_B = HCr_Bu;

      Crf_R = HCrf_Ru;
      Crf_G = HCrf_Gu;
      Crf_B = HCrf_Bu;

      Cb_R = HCb_Ru;
      Cb_G = HCb_Gu;
      Cb_B = HCb_Bu;

      Cbf_R = HCbf_Ru;
      Cbf_G = HCbf_Gu;
      Cbf_B = HCbf_Bu;

      RGB_Y = HRGB_Yu;
      RGBf_Y = HRGBf_Yu;

      R_Cr = HR_Cru;
      G_Cr = HG_Cru;
      G_Cb = HG_Cbu;
      B_Cb = HB_Cbu;

      Rf_Cr = HRf_Cru;
      Gf_Cr = HGf_Cru;
      Gf_Cb = HGf_Cbu;
      Bf_Cb = HBf_Cbu;
    }
    break;
  }

  if (!avg_inited) init_average();

  if (clamping == WEED_YUV_CLAMPING_CLAMPED) {
    min_Y = min_UV = YUV_CLAMP_MIN;
    max_Y = Y_CLAMP_MAX;
    max_UV = UV_CLAMP_MAX;
    cavg = (uint8_t *)cavgc;
  } else {
    min_Y = min_UV = 0;
    max_Y = max_UV = 255;
    cavg = (uint8_t *)cavgu;
  }
}


static void get_YUV_to_YUV_conversion_arrays(int iclamping, int isubspace, int oclamping, int osubspace) {
  // get conversion arrays for YUV -> YUV depending on in/out clamping and subspace
  // currently only clamped <-> unclamped conversions are catered for, subspace conversions are not yet done
  char *errmsg = NULL;
  if (!conv_YY_inited) init_YUV_to_YUV_tables();

  switch (isubspace) {
  case WEED_YUV_SUBSPACE_YUV:
    LIVES_WARN("YUV subspace input not specified, assuming Y'CbCr");
  case WEED_YUV_SUBSPACE_YCBCR:
    switch (osubspace) {
    case WEED_YUV_SUBSPACE_YUV:
      LIVES_WARN("YUV subspace output not specified, assuming Y'CbCr");
    case WEED_YUV_SUBSPACE_YCBCR:
      if (iclamping == WEED_YUV_CLAMPING_CLAMPED) {
        //Y'CbCr clamped -> Y'CbCr unclamped
        Y_to_Y = Yclamped_to_Yunclamped;
        U_to_U = V_to_V = UVclamped_to_UVunclamped;
      } else {
        //Y'CbCr unclamped -> Y'CbCr clamped
        Y_to_Y = Yunclamped_to_Yclamped;
        U_to_U = V_to_V = UVunclamped_to_UVclamped;
      }
      break;
    // TODO - other subspaces
    default:
      errmsg = lives_strdup_printf("Invalid YUV subspace conversion %d to %d", isubspace, osubspace);
      LIVES_ERROR(errmsg);
    }
    break;
  case WEED_YUV_SUBSPACE_BT709:
    switch (osubspace) {
    case WEED_YUV_SUBSPACE_YUV:
      LIVES_WARN("YUV subspace output not specified, assuming BT709");
    case WEED_YUV_SUBSPACE_BT709:
      if (iclamping == WEED_YUV_CLAMPING_CLAMPED) {
        //BT.709 clamped -> BT.709 unclamped
        Y_to_Y = Yclamped_to_Yunclamped;
        U_to_U = V_to_V = UVclamped_to_UVunclamped;
      } else {
        //BT.709 unclamped -> BT.709 clamped
        Y_to_Y = Yunclamped_to_Yclamped;
        U_to_U = V_to_V = UVunclamped_to_UVclamped;
      }
      break;
    // TODO - other subspaces
    default:
      errmsg = lives_strdup_printf("Invalid YUV subspace conversion %d to %d", isubspace, osubspace);
      LIVES_ERROR(errmsg);
    }
    break;
  default:
    errmsg = lives_strdup_printf("Invalid YUV subspace conversion %d to %d", isubspace, osubspace);
    LIVES_ERROR(errmsg);
    break;
  }
  if (errmsg) lives_free(errmsg);
}


struct XYZ xyzFromWavelength(double freq) {
  // appears to be freq * 10^11 Hz
  // (wavelength in amstrongs 10^-10 should give 10 X the value ??)
  //x max:
  // freq 5998 is 1.055946, 0.637099, 0.000044
  // y max:
  // freq 5542 is 0.503775, 0.998182, 0.006079
  // z max:
  // freq 4481 is 0.350801, 0.029932, 1.784213

  struct XYZ color;
  color.x = gaussian(freq,  1.056, 5998, 379, 310) +
            gaussian(freq,  0.362, 4420, 160, 267) +
            gaussian(freq, -0.065, 5011, 204, 262);
  color.y = gaussian(freq,  0.821, 5688, 469, 405) +
            gaussian(freq,  0.286, 5309, 163, 311);
  color.z = gaussian(freq,  1.217, 4370, 118, 360) +
            gaussian(freq,  0.681, 4590, 260, 138);
  return color;
}


static void rgb2xyz(uint8_t r, uint8_t g, uint8_t b, double *x, double *y, double *z) {
  double rr = (double)r / 255., gg = (double)g / 255., bb = (double)b / 255.;
  /* *x = (rr * 0.490 + gg * 0.310 + bb * 0.200) / 0.17697; */
  /* *y = (rr * 0.17697 + gg * 0.81240 + bb * 0.01063) / 0.17697; */
  /* *x = (gg * 0.010 + bb * 0.990) / 0.17697; */
  *x = (rr * 0.490 + gg * 0.17697 + bb) / 0.17697;
  *y = (rr * 0.310 + gg * 0.81240 + bb * 0.01) / 0.17697;
  *z = (rr * 0.2 + gg * 0.01063 * bb * 0.990) / 0.17697;
}

/* static void xyz2rgb(double *x, double *y, double *z, uint8_t *r, uint8_t *g, uint8_t *b) { */
/*   rr = x * .48147 +  */
/* } */


// xyz and lab, thanks to
// https://www.emanueleferonato.com/2009/09/08/color-difference-algorithm-part-2
#define LAB0 0.008856
#define LAB1 0.33333333333
#define LAB2 7.787
#define LAB3 0.13793103448 // 16. / 116.
LIVES_LOCAL_INLINE double lab_conv(double a) {return a > LAB0 ? pow(a, LAB1) : a * LAB2 + LAB3;}

static void xyz2lab(double x, double y, double z, double *l, double *a, double *b) {
  x = lab_conv(x); y = lab_conv(y); z = lab_conv(z);
  if (l) {*l = 116. * y - 16.;} if (a) {*a = 500. * (x - y);} if (b) {*b = 200. * (y - z);}
}

#define KL 1.0 // 2.0 for textiles
#define KC 1.0 // default
#define KH 1.0 // default
#define K1 0.045 // graphics arts, 0.048 textiles
#define K2 0.015 // graphics arts, 0.014 textiles
#define RNDFAC 0.0000000001
static double cdist94lab(double l0, double a0, double b0, double l1, double a1, double b1) {
  // CIE94
  double dl = l0 - l1;
  double c0 = sqrt(a0 * a0 + b0 * b0), c1 = sqrt(a1 * a1 + b1 * b1);
  double dc = c0 - c1, da = a0 - a1, db = b0 - b1;
  double dh = sqrt(da * da + db * db - dc * dc + RNDFAC);
  //dl /= KL;  // 1.0 default, 2.0 textiles
  dc /= (1. + K1 * c0);
  //dc /= KC;  // 1.0 default
  dh /= (1. + K2 * c1);
  //dh /= KH;  // 1.0 default
  return sqrt(dl * dl + dc * dc + dh * dh);
}


static uint8_t get_maxmin_diff(uint8_t a, uint8_t b, uint8_t c, uint8_t *max, uint8_t *min) {
  uint8_t lmax = a;
  uint8_t lmin = a;
  if (a > b) {
    if (b > c) lmin = c;
    else {
      lmin = b;
      if (c > a) lmax = c;
    }
  } else {
    if (b < c) lmax = c;
    else {
      lmax = b;
      if (c < a) lmin = c;
    }
  }
  if (max) *max = lmax;
  if (min) *min = lmin;
  return lmax - lmin;
}

double cdist94(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
  double dist;
  double x0 = 0., y0 = 0., z0 = 0.;
  double x1 = 0., y1 = 0., z1 = 0.;
  double L0 = 0., A0 = 0., B0 = 0.;
  double L1 = 0., A1 = 0., B1 = 0.;
  rgb2xyz(r0, g0, b0, &x0, &y0, &z0);
  rgb2xyz(r1, g1, b1, &x1, &y1, &z1);
  xyz2lab(x0 * 255., y0 * 255., z0 * 255., &L0, &A0, &B0);
  xyz2lab(x1 * 255., y1 * 255., z1 * 255., &L1, &A1, &B1);
  dist =  cdist94lab(L0, A0, B0, L1, A1, B1);
  return dist;
}


void rgb2hsv(uint8_t r, uint8_t g, uint8_t b, double *h, double *s, double *v) {
  // h, s, v = hue, saturation, value
  uint8_t cmax = 0, cmin = 0;
  uint8_t diff = get_maxmin_diff(r, g, b, &cmax, &cmin);
  double ddiff = (double)diff, dcmax = (double)cmax;
  if (h) {*h = 0.;} if (s) {*s = 0.;} if (v) {*v = 0.;}
  if (h && cmax != cmin) {
    if (cmax == r) *h = ((double)g - (double)b) / ddiff;
    else if (cmax == g) *h = 2. + ((double)b - (double)r) / ddiff;
    else *h = 4. + ((double)r - (double)g) / ddiff;
    *h = 60. * (*h < 0. ? (*h + 6.) : *h >= 6. ? (*h - 6.) : *h);
  }
  if (s && cmax) *s = (ddiff / dcmax) * 100.;
  if (v) *v = dcmax / 2.55;

#if CALC_HSL
  short a;
  if ((a = spc_rnd(Y_Ru[r] + Y_Gu[g] + Y_Bu[b])) > 255) a = 255;
  if (v) *v = (double)(a < 0 ? 0 : a) / 255.;
#endif

}

void hsv2rgb(double h, double s, double v, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (s < 0.000001) {
    *r = *g = *b = (v * 255. + .5);
    return;
  } else {
    int i = (int)h;
    double f = h - (double)i;
    double p = v * (1. - s);
    double dr, dg, db;
    if (i & 1) {
      double q = v * (1. - (s * f));
      switch (i) {
      case 1: dr = q; dg = v; db = p; break;
      case 3: dr = p; dg = q; db = v; break;
      default: dr = v; dg = p; db = q; break;
      }
    } else {
      double t = v * (1. - (s * (1. - f)));
      switch (i) {
      case 0: dr = v; dg = t; db = p; break;
      case 2: dr = p; dg = v; db = t; break;
      default: dr = t; dg = p; db = v; break;
      }
    }
    *r = (uint8_t)(dr * 255. + .5); *g = (uint8_t)(dg * 255. + .5); *b = (uint8_t)(db * 255. + .5);
  }
}


#define RDEL_MIN 8
#define GDEL_MIN 8
#define BDEL_MIN 8

#define RSCALE 1.4
#define GSCALE 0.7
#define BSCALE 1.3

boolean pick_nice_colour(ticks_t timeout, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *r1, uint8_t *g1, uint8_t *b1,
                         double max, double lmin, double lmax) {
  // given 2 colours a and b, calculate the cie94 distance (d) between them, then find a third colour
  // first calc the avg, calc d(a, b) with threshold > 1.
  // pick a random colour, the dist from avg must be approx. d(a, b) * max
  // da / db must be > ,9 and db / da also
  // finally luma should be between lmin and lmax
  // restrictions are gradually loosened
  //#define __VOLA volatile // for testing
#define __VOLA
#define DIST_THRESH 10.
#define RAT_START .9
#define RAT_TIO  .9999999
#define RAT_MIN .25
  uint8_t xr0, xg0, xb0, xr1, xg1, xb1;
  if (get_luma8(r0, g0, b0) > get_luma8(*r1, *g1, *b1)) {
    xr0 = *r1; xg0 = *g1; xb0 = *b1;
    xr1 = r0; xg1 = g0; xb1 = b0;
  } else {
    xr0 = r0; xg0 = g0; xb0 = b0;
    xr1 = *r1; xg1 = *g1; xb1 = *b1;
  }
  if (1) {
    lives_alarm_t alarm_handle = LIVES_NO_ALARM;
    __VOLA double gmm = 1. + lmax * 2., gmn = 1. + lmin;
    __VOLA uint8_t xr, xb, xg, ar, ag, ab;
    __VOLA uint8_t rmin = MIN(xr0, xr1) / 1.5, gmin = MIN(xg0, xg1) / gmm, bmin = MIN(xb0, xb1) / 1.5;
    __VOLA uint8_t rmax = MAX(xr0, xr1), gmax = MAX(xg0, xg1), bmax = MAX(xb0, xb1);
    __VOLA double l, da, db, z, rat = RAT_START, d = cdist94(xr0, xg0, xb0, xr1, xg1, xb1);

    if (timeout) alarm_handle = lives_alarm_set(timeout);

    if (d < DIST_THRESH) d = DIST_THRESH;
    max *= d;

    ar = (__VOLA double)(xr0 + xr1) / 2.;
    ag = (__VOLA double)(xg0 + xg1) / 2.;
    ab = (__VOLA double)(xb0 + xb1) / 2.;

    if (rmax - rmin < (RDEL_MIN << 1)) {
      rmin = ar <= RDEL_MIN ? 0 : ar - RDEL_MIN;
      rmax = rmax >= 255 - RDEL_MIN ? 255 : ar + RDEL_MIN;
    }

    if (gmax - gmin < (GDEL_MIN << 1)) {
      gmin = ag <= GDEL_MIN ? 0 : ag - GDEL_MIN;
      gmax = gmax >= 255 - GDEL_MIN ? 255 : ag + GDEL_MIN;
    }

    if (bmax - bmin < (BDEL_MIN << 1)) {
      bmin = ab <= BDEL_MIN ? 0 : ab - BDEL_MIN;
      bmax = bmax >= 255 - BDEL_MIN ? 255 : ab + BDEL_MIN;
    }

    // these values now become the range
    rmax = (rmax < 128 ? rmax << 1 : 255) - rmin;

    // special handling for geen, involving min luma
    gmax = (gmax < 255 / gmn ? gmax *gmn : 255) - gmin;

    bmax = (bmax < 128 ? bmax << 1 : 255) - bmin;

    //g_print("max %d min %d\n", bmax, bmin);

    while ((!timeout || lives_alarm_check(alarm_handle)) && (z = rat * RAT_TIO) > RAT_MIN) {
      // rat is initialized to RAT_START (default .9)
      // loop until timeout or rat * .9999 < RAT_MIN (def .25)

      rat = z;
      /// pick a random col
      xr = fastrand_int(bmax) + bmin;
      xg = fastrand_int(gmax) + gmin;
      xb = fastrand_int(bmax) + bmin;

      // calc perceptual dist. to averages
      da = cdist94(ar, ag, ab, xr, xg, xb) / 255.;

      //g_print("DA is %f; %f %f %f\n", da, max, rat, max / rat);

      // first we only consider points with dist < max from avg pt
      // we gradually increase max
      if (da > max / rat) continue;

      // calc dist. to both colours
      da = cdist94(xr0, xg0, xb0, xr, xg, xb);
      db = cdist94(xr1, xg1, xb1, xr, xg, xb);

      //g_print("DA2 is %f; %f %f %f %f\n", da, db, da * rat * lmax, db * rat * lmax, da);

      // next we only consider pts equidistant from the input colours
      // we assume however, that col2 is brighter tha col1
      // to help limit to lmax, da must be shorter than db in proportion
      // this gives a quick estimate of luma without having to calulate it each time
      // this check is loosened over cycles

      // if da X rat X lmax > db
      // or db X rat > da X lmax
      if (da * rat > db * lmax || db * rat > da) {
        /* g_print("failed equi check\n"); */
        /* if (da * rat * lmax > db) g_print("cond1\n"); */
        /* if (db * rat > lmax * da) g_print("cond2\n"); */
        continue;
      }
      // if we pass all the checks then we do a proper check of luma
      l = get_luma8(CLAMP0255f(xr * RSCALE), CLAMP0255f(xg * GSCALE), CLAMP0255f(xb * BSCALE));
      if (l < lmin || l > lmax) {
        //g_print("failed luma check %f %f %f %d %d %d\n", l, lmax, lmin, xr, xg, xb);
        continue;
      }
      *r1 = xr; *g1 = xg; *b1 = xb;
      return TRUE;
    }
    lives_alarm_clear(alarm_handle);
    if (prefs->show_dev_opts) {
      g_print("Failed to get harmonious colour\n");
    }
  }
  return FALSE;
}


//////////////////////////
// pixel conversions
#ifdef WEED_ADVANCED_PALETTES

static weed_macropixel_t advp[256];

void init_advanced_palettes(void) {
  lives_memset(advp, 0, 256 * sizeof(weed_macropixel_t));

  advp[0] = (weed_macropixel_t) {
    WEED_PALETTE_RGB24,
    {WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue}
  };

  advp[1] = (weed_macropixel_t) {
    WEED_PALETTE_BGR24,
    {WEED_VCHAN_blue, WEED_VCHAN_green, WEED_VCHAN_red}
  };

  advp[2] = (weed_macropixel_t) {
    WEED_PALETTE_RGBA32,
    {WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue, WEED_VCHAN_alpha}
  };

  advp[3] = (weed_macropixel_t) {
    WEED_PALETTE_BGRA32,
    {WEED_VCHAN_blue, WEED_VCHAN_green, WEED_VCHAN_red, WEED_VCHAN_alpha}
  };

  advp[4] = (weed_macropixel_t) {
    WEED_PALETTE_ARGB32,
    {WEED_VCHAN_alpha, WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue}
  };

  advp[5] = (weed_macropixel_t) {
    WEED_PALETTE_RGBFLOAT,
    {WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue},
    WEED_VCHAN_DESC_FP, {0}, {0}, 1, {32, 32, 32}
  };

  advp[6] = (weed_macropixel_t) {
    WEED_PALETTE_RGBAFLOAT,
    {WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue, WEED_VCHAN_alpha},
    WEED_VCHAN_DESC_FP, {0}, {0}, 1, {32, 32, 32, 32}
  };

  /// yuv planar
  advp[7] = (weed_macropixel_t) {
    WEED_PALETTE_YUV420P,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V},
    WEED_VCHAN_DESC_PLANAR, {1, 2, 2}, {1, 2, 2}
  };

  advp[8] = (weed_macropixel_t) {
    WEED_PALETTE_YVU420P,
    {WEED_VCHAN_Y, WEED_VCHAN_V, WEED_VCHAN_U},
    WEED_VCHAN_DESC_PLANAR, {1, 2, 2}, {1, 2, 2}
  };

  advp[9] = (weed_macropixel_t) {
    WEED_PALETTE_YUV422P,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V},
    WEED_VCHAN_DESC_PLANAR, {1, 2, 2}, {1, 1, 1}
  };

  advp[10] = (weed_macropixel_t) {
    WEED_PALETTE_YUV444P,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V}, WEED_VCHAN_DESC_PLANAR
  };

  advp[11] = (weed_macropixel_t) {
    WEED_PALETTE_YUVA4444P,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V, WEED_VCHAN_alpha},
    WEED_VCHAN_DESC_PLANAR
  };

  /// yuv packed
  advp[12] = (weed_macropixel_t) {
    WEED_PALETTE_UYVY,
    {WEED_VCHAN_U, WEED_VCHAN_Y, WEED_VCHAN_V, WEED_VCHAN_Y},
    0, {0}, {0}, 2
  };

  advp[13] = (weed_macropixel_t) {
    WEED_PALETTE_YUYV,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_Y, WEED_VCHAN_V},
    0, {0}, {0}, 2
  };

  advp[14] = (weed_macropixel_t) {WEED_PALETTE_YUV888, {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V}};

  advp[15] = (weed_macropixel_t) {
    WEED_PALETTE_YUVA8888,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V, WEED_VCHAN_alpha}
  };

  advp[16] = (weed_macropixel_t) {
    WEED_PALETTE_YUV411, {
      WEED_VCHAN_U, WEED_VCHAN_Y, WEED_VCHAN_Y,
      WEED_VCHAN_V, WEED_VCHAN_Y, WEED_VCHAN_Y
    },
    0, {0}, {0}, 4
  };

  /// alpha
  advp[17] = (weed_macropixel_t) {WEED_PALETTE_A8, {WEED_VCHAN_alpha}};

  advp[18] = (weed_macropixel_t) {WEED_PALETTE_A1, {WEED_VCHAN_alpha}, 0, {0}, {0}, 1, {1}};

  advp[19] = (weed_macropixel_t) {
    WEED_PALETTE_AFLOAT, {WEED_VCHAN_alpha},
                         WEED_VCHAN_DESC_FP, {0}, {0}, 1, {32}
  };

  advp[20] = (weed_macropixel_t) {WEED_PALETTE_END};

  // custom palettes (designed for future use or for testing)
  advp[21] = (weed_macropixel_t) {
    LIVES_PALETTE_ABGR32,
    {WEED_VCHAN_alpha, WEED_VCHAN_blue, WEED_VCHAN_green, WEED_VCHAN_red}
  };

  advp[22] = (weed_macropixel_t) {
    LIVES_PALETTE_YVU422P,
    {WEED_VCHAN_Y, WEED_VCHAN_V, WEED_VCHAN_U},
    WEED_VCHAN_DESC_PLANAR, {1, 2, 2}, {1, 1, 1}
  };

  advp[23] = (weed_macropixel_t) {
    LIVES_PALETTE_YUVA420P,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V, WEED_VCHAN_alpha},
    WEED_VCHAN_DESC_PLANAR, {1, 2, 2, 1}, {1, 2, 2, 1}
  };

  advp[24] = (weed_macropixel_t) {
    LIVES_PALETTE_AYUV8888,
    {WEED_VCHAN_alpha, WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V}
  };

  advp[25] = (weed_macropixel_t) {
    LIVES_PALETTE_YUVFLOAT,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V},
    WEED_VCHAN_DESC_FP, {0}, {0}, 1, {32, 32, 32}
  };

  advp[26] = (weed_macropixel_t) {
    LIVES_PALETTE_YUVAFLOAT,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V, WEED_VCHAN_alpha},
    WEED_VCHAN_DESC_FP, {0}, {0}, 1, {32, 32, 32, 32}
  };

  advp[27] = (weed_macropixel_t) {
    LIVES_PALETTE_RGB48,
    {WEED_VCHAN_red, WEED_VCHAN_green, WEED_VCHAN_blue},
    0, {0}, {0}, 1, {16, 16, 16}
  };

  advp[28] = (weed_macropixel_t) {
    LIVES_PALETTE_RGBA64, {
      WEED_VCHAN_red, WEED_VCHAN_green,
      WEED_VCHAN_blue, WEED_VCHAN_alpha
    },
    0, {0}, {0}, 1, {16, 16, 16, 16}
  };

  advp[29] = (weed_macropixel_t) {
    LIVES_PALETTE_YUV121010,
    {WEED_VCHAN_Y, WEED_VCHAN_U, WEED_VCHAN_V},
    0, {0}, {0}, 1, {12, 10, 10}
  };
}


LIVES_GLOBAL_INLINE const weed_macropixel_t *get_advanced_palette(int weed_palette) {
  for (int i = 0; advp[i].ext_ref != WEED_PALETTE_END; i++)
    if (advp[i].ext_ref == weed_palette) return &advp[i];
  return NULL;
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_valid(int pal) {
  return (get_advanced_palette(pal) != NULL);
}

LIVES_GLOBAL_INLINE int get_simple_palette(weed_macropixel_t *mpx) {
  if (mpx) return mpx->ext_ref;
  return WEED_PALETTE_NONE;
}

LIVES_LOCAL_INLINE boolean is_rgbchan(uint16_t ctype) {
  return (ctype == WEED_VCHAN_red || ctype == WEED_VCHAN_green || ctype == WEED_VCHAN_blue);
}

LIVES_LOCAL_INLINE boolean is_yuvchan(uint16_t ctype) {
  return (ctype == WEED_VCHAN_Y || ctype == WEED_VCHAN_U || ctype == WEED_VCHAN_V);
}

LIVES_GLOBAL_INLINE double pixel_size(int pal) {
  /// This is actually the MACRO pixel size in bytes, to get the real pixel size, divide by weed_palette_pixels_per_macropixel()
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (!mpx) return 0;
  else {
    double psize = 0;
    for (int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++)
      psize += mpx->bitsize[i] == 0. ? 1. : (double)(mpx->bitsize[i]) / 8.;
    return psize;
  }
}

LIVES_GLOBAL_INLINE int weed_palette_get_pixels_per_macropixel(int pal) {
  if (pal == WEED_PALETTE_END) return 1;
  else {
    int npix = 0;
    const weed_macropixel_t *mpx = get_advanced_palette(pal);
    if (!mpx) return 0;
    npix = mpx->npixels;
    return !npix ? 1 : npix;
  }
}

LIVES_GLOBAL_INLINE int weed_palette_get_bits_per_macropixel(int pal) {
  int ppm = weed_palette_get_pixels_per_macropixel(pal);
  if (ppm) return pixel_size(pal) * 8.;
  return 0;
}

LIVES_GLOBAL_INLINE double weed_palette_get_bytes_per_pixel(int pal) {
  int ppm = weed_palette_get_pixels_per_macropixel(pal);
  if (ppm) return pixel_size(pal) / (double)ppm;
  return 0.;
}

LIVES_GLOBAL_INLINE int weed_palette_get_nplanes(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  register int i = 0;
  if (mpx) {
    if (!(mpx->flags & WEED_VCHAN_DESC_PLANAR)) return 1;
    for (i = 0; i < MAXPPLANES && mpx->chantype[i]; i++);
  }
  return i;
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_alpha(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx && mpx->chantype[0] == WEED_VCHAN_alpha && !mpx->chantype[1]) return TRUE;
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean weed_palette_red_first(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) {
    for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++) {
      if (mpx->chantype[i] == WEED_VCHAN_red) return TRUE;
      if (mpx->chantype[i] == WEED_VCHAN_blue) return FALSE;
    }
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean weed_palettes_rbswapped(int pal0, int pal1) {
  return weed_palette_red_first(pal0) != weed_palette_red_first(pal1);
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_rgb(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) {
    for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++)
      if (is_rgbchan(mpx->chantype[i])) return TRUE;
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_yuv(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) {
    for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++)
      if (is_yuvchan(mpx->chantype[i])) return TRUE;
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean weed_palette_has_alpha(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) {
    for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++)
      if (mpx->chantype[i] == WEED_VCHAN_alpha) return TRUE;
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_float(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  return (mpx && (mpx->flags & WEED_VCHAN_DESC_FP));
}

LIVES_GLOBAL_INLINE double weed_palette_get_plane_ratio_horizontal(int pal, int plane) {
  uint8_t subsam = 0;
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) subsam = mpx->hsub[plane];
  if (subsam) return 1. / (double)(subsam);
  return 1.;
}

LIVES_GLOBAL_INLINE double weed_palette_get_plane_ratio_vertical(int pal, int plane) {
  uint8_t subsam = 0;
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) subsam = mpx->vsub[plane];
  if (subsam) return 1. / (double)(subsam);
  return 1.;
}

LIVES_LOCAL_INLINE int _get_alpha(int pal) {
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (mpx) {
    for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++)
      if (mpx->chantype[i] == WEED_VCHAN_alpha) return i;
  }
  return -1;
}

LIVES_GLOBAL_INLINE int weed_palette_get_alpha_plane(int pal) {
  if (weed_palette_is_planar(pal)) return _get_alpha(pal);
  return -1;
}

LIVES_GLOBAL_INLINE int weed_palette_get_alpha_offset(int pal) {
  if (!weed_palette_is_planar(pal)) return _get_alpha(pal);
  return -1;
}

LIVES_GLOBAL_INLINE boolean weed_palette_has_alpha_first(int pal) {
  return _get_alpha(pal) == 0;
}

LIVES_GLOBAL_INLINE boolean weed_palette_has_alpha_last(int pal) {
  return _get_alpha(pal) == 3;
}

LIVES_GLOBAL_INLINE boolean weed_palette_is_sane(int pal) {
  // first cpt must be alpha, red, blue, y, u, or v
  //
  // if first was alpha, 2nd must be NULL, red, blue or y, u, v
  // if first was red or blue, 2nd must be green
  // if first was y, 2nd must be y, u or v
  //
  // if second was green, 3rd must be blue or red, but != 1st
  // if second was y, u, or v, third must be y, u or v
  // if second was red or blue, 3rd must be green
  //
  // if third was red or blue, fourth may be alpha or NULL
  // if third was green, fourth must be red or blue but != 2nd
  // if third was y, u, or v, fourth may be y, u, v or alpha
  //
  // if fourth was red, blue or alpha, fifth must be NULL
  // if fourth was y, u, or v, fifth must be y, u, v or alpha
  //
  // if fifth was alpha, sixth must be NULL
  // if fifth was y, u, or v, sixth must be y, u, v or alpha
  //
  // etc. for 7 and 8

  // there must be some symmetry between u and v, e.g. yyuyyv is allowed but not yyuyv or yuy
  // 0, 1, or 2 y before / after a u or v, y numbers must match and u / v numbers too

  // future directions: allow single plane / cpt r, g, b or y
  boolean red = FALSE, blue = FALSE, alpha = FALSE;
  int nseqy = 0, nseqyu = 0, nseqyv = 0, nu = 0, nv = 0;
  const weed_macropixel_t *mpx = get_advanced_palette(pal);
  if (!mpx) return FALSE;
  for (register int i = 0; i < MAXPPLANES && mpx->chantype[i]; i++) {
    uint16_t ctype = mpx->chantype[i];
    if (i > 3 && alpha) return FALSE;
    switch (ctype) {
    case WEED_VCHAN_Y:
      if (red || blue) return FALSE;
      if (++nseqy > 2) return FALSE;
      break;
    case WEED_VCHAN_U:
      if (red || blue) return FALSE;
      if (nseqyv && nseqy && nseqy != nseqyv) return FALSE;
      nu++;
      nseqyu = nseqy;
      nseqy = nseqyv = 0;
      break;
    case WEED_VCHAN_V:
      if (red || blue) return FALSE;
      if (nseqyu && nseqy && nseqy != nseqyu) return FALSE;
      nv++;
      nseqyv = nseqy;
      nseqy = nseqyu = 0;
      break;
    default:
      switch (i) {
      case 0:
        switch (ctype) {
        case WEED_VCHAN_alpha: alpha = TRUE; break;
        case WEED_VCHAN_red: red = TRUE; break;
        case WEED_VCHAN_blue: blue = TRUE; break;
        default: return FALSE;
        }
        break;
      case 1:
        if (nu || nv || nseqy) return FALSE;
        switch (ctype) {
        case WEED_VCHAN_alpha: return FALSE;
        case WEED_VCHAN_green:
          if (!red && !blue) return FALSE;
          break;
        case WEED_VCHAN_red:
          if (!alpha) return FALSE;
          red = TRUE;
          break;
        case WEED_VCHAN_blue:
          if (!alpha) return FALSE;
          blue = TRUE;
          break;
        default: return FALSE;
        }
        break;
      case 2:
        if (nu || nv || nseqy) return FALSE;
        switch (ctype) {
        case WEED_VCHAN_alpha: return FALSE;
        case WEED_VCHAN_green:
          if (!red && !blue) return FALSE;
          break;
        case WEED_VCHAN_red:
          if (!blue) return FALSE;
          red = TRUE;
          break;
        case WEED_VCHAN_blue:
          if (!red) return FALSE;
          blue = TRUE;
          break;
        default: return FALSE;
        }
        break;
      case 3:
        switch (ctype) {
        case WEED_VCHAN_alpha:
          if (alpha) return FALSE;
          alpha = TRUE;
          break;
        case WEED_VCHAN_red:
          if (!blue) return FALSE;
          red = TRUE;
          break;
        case WEED_VCHAN_blue:
          if (!red) return FALSE;
          blue = TRUE;
          break;
        default: return FALSE;
        }
        break;
      default:
        if (ctype != WEED_VCHAN_alpha) return FALSE;
        alpha = TRUE;
        break;
      }
      break;
    }
  }
  if (red != blue || nv != nu || (nseqy != nseqyu && nseqy != nseqyv)) return FALSE;
  return TRUE;
}
#endif


static void init_gamma_tx(void) {
  gamma_tx[WEED_GAMMA_SRGB] = (gamma_const_t) {0.055, 12.92, 0.04045, 2.4};
  gamma_tx[WEED_GAMMA_BT709] = (gamma_const_t) {0.099, 4.5, 0.081, 1. / .45};
}

static void yuv2rgb_int(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) LIVES_HOT;

double get_luma8(uint8_t r, uint8_t g, uint8_t b) {
  /// return luma value between 0. (black) and 1. (white)
  short a = _spc_rnd(Y_Ru[r] + Y_Gu[g] + Y_Bu[b], PB_QUALITY_HIGH);
  if (a > 255) a = 255;
  return a < 0 ? 0. : (double)a / 255.;
}


double get_luma16(uint16_t r, uint16_t g, uint16_t b) {
  /// return luma value between 0. (black) and 1. (white)
  return get_luma8(r >> 8, g >> 8, b >> 8);
}

void init_colour_engine(void) {
  init_RGB_to_YUV_tables();
  init_YUV_to_RGB_tables();
  init_YUV_to_YUV_tables();
  init_average();
  init_unal();
  init_gamma_tx();
  init_conversions(OBJ_INTENTION_PLAY);
#ifdef WEED_ADVANCED_PALETTES
  init_advanced_palettes();
#endif
}

// internal thread fns
static void *convert_rgb_to_uyvy_frame_thread(void *cc_params);
static void *convert_bgr_to_uyvy_frame_thread(void *cc_params);
static void *convert_rgb_to_yuyv_frame_thread(void *cc_params);
static void *convert_bgr_to_yuyv_frame_thread(void *cc_params);
static void *convert_argb_to_uyvy_frame_thread(void *cc_params);
static void *convert_argb_to_yuyv_frame_thread(void *cc_params);

static void *convert_rgb_to_yuv_frame_thread(void *cc_params);
static void *convert_bgr_to_yuv_frame_thread(void *cc_params);
static void *convert_argb_to_yuv_frame_thread(void *cc_params);
static void *convert_rgb_to_yuvp_frame_thread(void *cc_params);
static void *convert_bgr_to_yuvp_frame_thread(void *cc_params);
static void *convert_argb_to_yuvp_frame_thread(void *cc_params);

static void *convert_uyvy_to_rgb_frame_thread(void *cc_params);
static void *convert_uyvy_to_bgr_frame_thread(void *cc_params);
static void *convert_uyvy_to_argb_frame_thread(void *cc_params);
static void *convert_yuyv_to_rgb_frame_thread(void *cc_params);
static void *convert_yuyv_to_bgr_frame_thread(void *cc_params);
static void *convert_yuyv_to_argb_frame_thread(void *cc_params);

static void *convert_yuv_planar_to_rgb_frame_thread(void *cc_params);
static void *convert_yuv_planar_to_bgr_frame_thread(void *cc_params);
static void *convert_yuv_planar_to_argb_frame_thread(void *cc_params);

static void *convert_yuv420p_to_rgb_frame_thread(void *cc_params);
static void *convert_yuv420p_to_bgr_frame_thread(void *cc_params);
static void *convert_yuv420p_to_argb_frame_thread(void *cc_params);

static void *convert_yuv888_to_rgb_frame_thread(void *cc_params);
static void *convert_yuv888_to_bgr_frame_thread(void *cc_params);
static void *convert_yuv888_to_argb_frame_thread(void *cc_params);
static void *convert_yuva8888_to_rgba_frame_thread(void *cc_params);
static void *convert_yuva8888_to_bgra_frame_thread(void *cc_params);
static void *convert_yuva8888_to_argb_frame_thread(void *cc_params);

static void *convert_swap3_frame_thread(void *cc_params);
static void *convert_swap4_frame_thread(void *cc_params);
static void *convert_swap3addpost_frame_thread(void *cc_params);
static void *convert_swap3addpre_frame_thread(void *cc_params);
static void *convert_swap3delpost_frame_thread(void *cc_params);
static void *convert_swap3delpre_frame_thread(void *cc_params);
static void *convert_addpre_frame_thread(void *cc_params);
static void *convert_addpost_frame_thread(void *cc_params);
static void *convert_delpre_frame_thread(void *cc_params);
static void *convert_delpost_frame_thread(void *cc_params);
static void *convert_swap3postalpha_frame_thread(void *cc_params);
#ifdef WEED_ADVANCED_PALETTES
static void *convert_swap3prealpha_frame_thread(void *cc_params);
#endif
static void *convert_swapprepost_frame_thread(void *cc_params);

static void *convert_swab_frame_thread(void *cc_params);

#if 0
static void rgb2yuv_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *lut) LIVES_HOT;
#endif
static void rgb2uyvy(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                     uyvy_macropixel *uyvy) LIVES_FLATTEN LIVES_HOT;
static void rgb2uyvy_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                                uyvy_macropixel *uyvy, uint8_t *lut) LIVES_FLATTEN LIVES_HOT;
static void rgb16_2uyvy(uint16_t r0, uint16_t g0, uint16_t b0, uint16_t r1, uint16_t g1, uint16_t b1,
                        uyvy_macropixel *uyvy) LIVES_FLATTEN LIVES_HOT;
#if 0
static void rgb16_2uyvy_with_gamma(uint16_t r0, uint16_t g0, uint16_t b0, uint16_t r1, uint16_t g1, uint16_t b1,
                                   uyvy_macropixel *uyvy, uint8_t *lut) LIVES_FLATTEN LIVES_HOT;
#endif

static void rgb2yuyv(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                     yuyv_macropixel *yuyv) LIVES_FLATTEN LIVES_HOT;
#if 0
static void rgb2yuyv_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                                yuyv_macropixel *yuyv, uint8_t *lut) LIVES_FLATTEN LIVES_HOT;
#endif
static void rgb2_411(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                     uint8_t r2, uint8_t g2, uint8_t b2, uint8_t r3, uint8_t g3, uint8_t b3, yuv411_macropixel *yuv) LIVES_HOT;

static void yuv2rgb_with_gamma(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *lut) LIVES_HOT;
static void uyvy2rgb(uyvy_macropixel *uyvy, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                     uint8_t *r1, uint8_t *g1, uint8_t *b1) LIVES_FLATTEN LIVES_HOT;
static void yuyv2rgb(yuyv_macropixel *yuyv, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                     uint8_t *r1, uint8_t *g1, uint8_t *b1) LIVES_FLATTEN LIVES_HOT;
static void yuv888_2_rgb(uint8_t *yuv, uint8_t *rgb, boolean add_alpha) LIVES_FLATTEN LIVES_HOT;
static void yuva8888_2_rgba(uint8_t *yuva, uint8_t *rgba, boolean del_alpha) LIVES_FLATTEN LIVES_HOT;
static void yuv888_2_bgr(uint8_t *yuv, uint8_t *bgr, boolean add_alpha) LIVES_FLATTEN LIVES_HOT;
static void yuva8888_2_bgra(uint8_t *yuva, uint8_t *bgra, boolean del_alpha) LIVES_FLATTEN LIVES_HOT;
static void yuv888_2_argb(uint8_t *yuv, uint8_t *argb) LIVES_FLATTEN LIVES_HOT;
static void yuva8888_2_argb(uint8_t *yuva, uint8_t *argb) LIVES_FLATTEN LIVES_HOT;
static void uyvy_2_yuv422(uyvy_macropixel *uyvy, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) LIVES_HOT;
static void yuyv_2_yuv422(yuyv_macropixel *yuyv, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) LIVES_HOT;

#define avg_chroma(x, y) ((uint8_t)(*(cavg + ((int)(x) << 8) + (int)(y))))
#define avg_chroma_3_1(x, y) ((uint8_t)(avg_chroma(x, avg_chroma(x, y))))
#define avg_chroma_1_3(x, y) ((uint8_t)(avg_chroma(avg_chroma(x, y), y)))

static uint8_t (*avg_chromaf)(uint8_t x, uint8_t y);

/* static uint8_t avg_chromaf_high(uint8_t x, uint8_t y) { */
/*   return (((float)(spc_rnd(((x) << 8) + ((y) << 8)))) * 128. + .5); */
/* } */

static uint8_t avg_chromaf_fast(uint8_t x, uint8_t y) {
  return avg_chroma(x, y);
}

LIVES_GLOBAL_INLINE void init_conversions(int intent) {
  avg_chromaf = avg_chromaf_fast;
  if (intent == OBJ_INTENTION_RENDER || intent == OBJ_INTENTION_TRANSCODE) {
    //avg_chromaf = avg_chromaf_high;
    if (prefs) prefs->pb_quality = PB_QUALITY_HIGH;
    /// set the 'effort' to as low as possible; if using adaptive quality
    // this ensures we render at the highest settings
    mainw->effort = -EFFORT_RANGE_MAX;
  } else {
    if (prefs) prefs->pb_quality = future_prefs->pb_quality;
  }
}

#define avg_chroma_3_1f(x, y) ((uint8_t)(avg_chromaf(x, avg_chromaf(x, y))))
#define avg_chroma_1_3f(x, y) ((uint8_t)(avg_chromaf(avg_chromaf(x, y), y)))

LIVES_INLINE void rgb2yuv(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *y, uint8_t *u, uint8_t *v) {
  short a;
  if ((a = spc_rnd(Y_R[r0] + Y_G[g0] + Y_B[b0])) > max_Y) * y = max_Y;
  else *y = a < min_Y ? min_Y : a;
  if ((a = spc_rnd(Cb_R[r0] + Cb_G[g0] + Cb_B[b0])) > max_UV) * u = max_UV;
  else *u = a < min_UV ? min_UV : a;
  if ((a = spc_rnd(Cr_R[r0] + Cr_G[g0] + Cr_B[b0])) > max_UV) * v = max_UV;
  else *v = a < min_UV ? min_UV : a;
}

#define bgr2yuv(b0, g0, r0, y, u, v) rgb2yuv(r0, g0, b0, y, u, v)


LIVES_INLINE void rgb2yuv_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *lut) {
  short a;
  if ((a = spc_rnd(Y_R[(r0 = lut[r0])] + Y_G[(g0 = lut[g0])] + Y_B[(b0 = lut[b0])])) > max_Y) * y = max_Y;
  else *y = a < min_Y ? min_Y : a;
  if ((a = spc_rnd(Cb_R[r0] + Cb_G[g0] + Cb_B[b0])) > max_UV) * u = max_UV;
  else *u = a < min_UV ? min_UV : a;
  if ((a = spc_rnd(Cr_R[r0] + Cr_G[g0] + Cr_B[b0])) > max_UV) * v = max_UV;
  else *v = a < min_UV ? min_UV : a;
}


#define bgr2yuv_with_gamma(b0, g0, r0, y, u, v) rgb2yuv(r0, g0, b0, y, u, v, lut)

LIVES_INLINE void rgb2uyvy(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uyvy_macropixel *uyvy) {
  short a;
  if ((a = spc_rnd(Cb_R[r0] + Cb_G[g0] + Cb_B[b0])) > max_UV) uyvy->u0 = max_UV;
  else uyvy->u0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[r0] + Y_G[g0] + Y_B[b0])) > max_Y) uyvy->y0 = max_Y;
  else uyvy->y0 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cr_R[r1] + Cr_G[g1] + Cr_B[b1])) > max_UV) uyvy->v0 = max_UV;
  else uyvy->v0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[r1] + Y_G[g1] + Y_B[b1])) > max_Y) uyvy->y1 = max_Y;
  else uyvy->y1 = a < min_Y ? min_Y : a;
}

LIVES_INLINE void rgb2uyvy_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                                      uyvy_macropixel *uyvy, uint8_t *lut) {
  short a;
  if ((a = spc_rnd(Cb_R[(r0 = lut[r0])] + Cb_G[(g0 = lut[g0])] + Cb_B[(b0 = lut[b0])])) > max_UV) uyvy->u0 = max_UV;
  else uyvy->u0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[r0] + Y_G[g0] + Y_B[b0])) > max_Y) uyvy->y0 = max_Y;
  else uyvy->y0 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cr_R[(r1 = lut[r1])] + Cr_G[(g1 = lut[g1])] + Cr_B[(b1 = lut[b1])])) > max_UV) uyvy->v0 = max_UV;
  else uyvy->v0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[r1] + Y_G[g1] + Y_B[b1])) > max_Y) uyvy->y1 = max_Y;
  else uyvy->y1 = a < min_Y ? min_Y : a;
}

LIVES_INLINE void rgb2yuyv(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, yuyv_macropixel *yuyv) {
  short a;
  if ((a = spc_rnd(Y_R[r0] + Y_G[g0] + Y_B[b0])) > max_Y) yuyv->y0 = max_Y;
  else yuyv->y0 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cb_R[r0] + Cb_G[g0] + Cb_B[b0])) > max_UV) yuyv->u0 = max_UV;
  yuyv->u0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[r1] + Y_G[g1] + Y_B[b1])) > max_Y) yuyv->y1 = max_Y;
  else yuyv->y1 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cr_R[r1] + Cr_G[g1] + Cr_B[b1])) > max_UV) yuyv->v0 = max_UV;
  yuyv->v0 = a < min_UV ? min_UV : a;
}


LIVES_INLINE void rgb2yuyv_with_gamma(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                                      yuyv_macropixel *yuyv, uint8_t *lut) {
  short a;
  if ((a = spc_rnd(Y_R[(r0 = lut[r0])] + Y_G[(g0 = lut[g0])] + Y_B[(b0 = lut[b0])])) > max_Y) yuyv->y0 = max_Y;
  else yuyv->y0 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cb_R[r0] + Cb_G[g0] + Cb_B[b0])) > max_UV) yuyv->u0 = max_UV;
  else yuyv->u0 = a < min_UV ? min_UV : a;

  if ((a = spc_rnd(Y_R[(r1 = lut[r1])] + Y_G[(g1 = lut[g1])] + Y_B[(b1 = lut[b1])])) > max_Y) yuyv->y1 = max_Y;
  else yuyv->y1 = a < min_Y ? min_Y : a;

  if ((a = spc_rnd(Cr_R[r1] + Cr_G[g1] + Cr_B[b1])) > max_UV) yuyv->v0  = max_UV;
  else yuyv->v0 = a < min_UV ? min_UV : a;
}


LIVES_INLINE void rgb16_2uyvy(uint16_t r0, uint16_t g0, uint16_t b0, uint16_t r1, uint16_t g1, uint16_t b1,
                              uyvy_macropixel *uyvy) {
  register short a;
  uint8_t rfrac0, gfrac0, bfrac0, rfrac1, gfrac1, bfrac1;
  uint8_t rr0, bb0, gg0, rr1, gg1, bb1;
  uint8_t *bytes;

  bytes = (uint8_t *)&r0;
  rfrac0 = bytes[1];
  bytes = (uint8_t *)&g0;
  gfrac0 = bytes[1];
  bytes = (uint8_t *)&b0;
  bfrac0 = bytes[1];
  bytes = (uint8_t *)&r1;
  rfrac1 = bytes[1];
  bytes = (uint8_t *)&g1;
  gfrac1 = bytes[1];
  bytes = (uint8_t *)&b1;
  bfrac1 = bytes[1];

  rr0 = (r0 & 0xFF) == 0xFF ? Y_R[255] : (Y_R[r0 & 0xFF] * rfrac0 + Y_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8;
  gg0 = (g0 & 0xFF) == 0xFF ? Y_G[255] : (Y_G[g0 & 0xFF] * gfrac0 + Y_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8;
  bb0 = (b0 & 0xFF) == 0xFF ? Y_B[255] : (Y_B[b0 & 0xFF] * bfrac0 + Y_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8;

  if ((a = spc_rnd(rr0 + gg0 + bb0)) > max_Y) uyvy->y0 = max_Y;
  else uyvy->y0 = a < min_Y ? min_Y : a;

  rr1 = (r1 & 0xFF) == 0xFF ? Y_R[255] : (Y_R[r1 & 0xFF] * rfrac1 + Y_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8;
  gg1 = (g1 & 0xFF) == 0xFF ? Y_G[255] : (Y_G[g1 & 0xFF] * gfrac1 + Y_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8;
  bb1 = (b1 & 0xFF) == 0xFF ? Y_B[255] : (Y_B[b1 & 0xFF] * bfrac1 + Y_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8;

  if ((a = spc_rnd(rr0 + gg0 + bb0)) > max_Y) uyvy->y1 = max_Y;
  else uyvy->y1 = a < min_Y ? min_Y : a;

  rr0 = (r0 & 0xFF) == 0xFF ? Cb_R[255] : (Cb_R[r0 & 0xFF] * rfrac0 + Cb_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8;
  gg0 = (g0 & 0xFF) == 0xFF ? Cb_G[255] : (Cb_G[g0 & 0xFF] * gfrac0 + Cb_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8;
  bb0 = (b0 & 0xFF) == 0xFF ? Cb_B[255] : (Cb_B[b0 & 0xFF] * bfrac0 + Cb_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8;

  rr1 = (r1 & 0xFF) == 0xFF ? Cb_R[255] : (Cb_R[r1 & 0xFF] * rfrac1 + Cb_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8;
  gg1 = (g1 & 0xFF) == 0xFF ? Cb_G[255] : (Cb_G[g1 & 0xFF] * gfrac1 + Cb_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8;
  bb1 = (b1 & 0xFF) == 0xFF ? Cb_B[255] : (Cb_B[b1 & 0xFF] * bfrac1 + Cb_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8;

  uyvy->u0 = avg_chroma_3_1f(rr0 + gg0 + bb0, rr1 + gg1 + bb1);

  rr0 = (r0 & 0xFF) == 0xFF ? Cr_R[255] : (Cr_R[r0 & 0xFF] * rfrac0 + Cr_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8;
  gg0 = (g0 & 0xFF) == 0xFF ? Cr_G[255] : (Cr_G[g0 & 0xFF] * gfrac0 + Cr_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8;
  bb0 = (b0 & 0xFF) == 0xFF ? Cr_B[255] : (Cr_B[b0 & 0xFF] * bfrac0 + Cr_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8;

  rr1 = (r1 & 0xFF) == 0xFF ? Cr_R[255] : (Cr_R[r1 & 0xFF] * rfrac1 + Cr_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8;
  gg1 = (g1 & 0xFF) == 0xFF ? Cr_G[255] : (Cr_G[g1 & 0xFF] * gfrac1 + Cr_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8;
  bb1 = (b1 & 0xFF) == 0xFF ? Cr_B[255] : (Cr_B[b1 & 0xFF] * bfrac1 + Cr_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8;

  uyvy->v0 = avg_chroma_1_3f(rr0 + gg0 + bb0, rr1 + gg1 + bb1);
}

#if 0
LIVES_INLINE void rgb16_2uyvy_with_gamma(uint16_t r0, uint16_t g0, uint16_t b0, uint16_t r1, uint16_t g1, uint16_t b1,
    uyvy_macropixel *uyvy, uint8_t *lut) {
  register short a;
  uint8_t rfrac0, gfrac0, bfrac0, rfrac1, gfrac1, bfrac1;
  uint8_t rr0, bb0, gg0, rr1, gg1, bb1;
  uint8_t *bytes;

  bytes = (uint8_t *)&r0;
  rfrac0 = bytes[1];
  bytes = (uint8_t *)&g0;
  gfrac0 = bytes[1];
  bytes = (uint8_t *)&b0;
  bfrac0 = bytes[1];
  bytes = (uint8_t *)&r1;
  rfrac1 = bytes[1];
  bytes = (uint8_t *)&g1;
  gfrac1 = bytes[1];
  bytes = (uint8_t *)&b1;
  bfrac1 = bytes[1];

  rr0 = lut[(r0 & 0xFF) == 0xFF ? Y_R[255] : (Y_R[r0 & 0xFF] * rfrac0 + Y_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8];
  gg0 = lut[(g0 & 0xFF) == 0xFF ? Y_G[255] : (Y_G[g0 & 0xFF] * gfrac0 + Y_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8];
  bb0 = lut[(b0 & 0xFF) == 0xFF ? Y_B[255] : (Y_B[b0 & 0xFF] * bfrac0 + Y_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8];

  if ((a = spc_rnd(rr0 + gg0 + bb0)) > max_Y) uyvy->y0 = max_Y;
  else uyvy->y0 = a < min_Y ? min_Y : a;

  rr1 = lut[(r1 & 0xFF) == 0xFF ? Y_R[255] : (Y_R[r1 & 0xFF] * rfrac1 + Y_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8];
  gg1 = lut[(g1 & 0xFF) == 0xFF ? Y_G[255] : (Y_G[g1 & 0xFF] * gfrac1 + Y_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8];
  bb1 = lut[(b1 & 0xFF) == 0xFF ? Y_B[255] : (Y_B[b1 & 0xFF] * bfrac1 + Y_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8];

  if ((a = spc_rnd(rr1 + gg1 + bb1)) > max_Y) uyvy->y1 = max_Y;
  else uyvy->y1 = a < min_Y ? min_Y : a;

  rr0 = lut[(r0 & 0xFF) == 0xFF ? Cb_R[255] : (Cb_R[r0 & 0xFF] * rfrac0 + Cb_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8];
  gg0 = lut[(g0 & 0xFF) == 0xFF ? Cb_G[255] : (Cb_G[g0 & 0xFF] * gfrac0 + Cb_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8];
  bb0 = lut[(b0 & 0xFF) == 0xFF ? Cb_B[255] : (Cb_B[b0 & 0xFF] * bfrac0 + Cb_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8];

  rr1 = lut[(r1 & 0xFF) == 0xFF ? Cb_R[255] : (Cb_R[r1 & 0xFF] * rfrac1 + Cb_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8];
  gg1 = lut[(g1 & 0xFF) == 0xFF ? Cb_G[255] : (Cb_G[g1 & 0xFF] * gfrac1 + Cb_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8];
  bb1 = lut[(b1 & 0xFF) == 0xFF ? Cb_B[255] : (Cb_B[b1 & 0xFF] * bfrac1 + Cb_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8];

  uyvy->u0 = avg_chroma_3_1f(rr0 + gg0 + bb0, rr1 + gg1 + bb1);

  rr0 = lut[(r0 & 0xFF) == 0xFF ? Cr_R[255] : (Cr_R[r0 & 0xFF] * rfrac0 + Cr_R[(r0 & 0xFF) + 1] * (255 - rfrac0)) >> 8];
  gg0 = lut[(g0 & 0xFF) == 0xFF ? Cr_G[255] : (Cr_G[g0 & 0xFF] * gfrac0 + Cr_G[(g0 & 0xFF) + 1] * (255 - gfrac0)) >> 8];
  bb0 = lut[(b0 & 0xFF) == 0xFF ? Cr_B[255] : (Cr_B[b0 & 0xFF] * bfrac0 + Cr_B[(b0 & 0xFF) + 1] * (255 - bfrac0)) >> 8];

  rr1 = lut[(r1 & 0xFF) == 0xFF ? Cr_R[255] : (Cr_R[r1 & 0xFF] * rfrac1 + Cr_R[(r1 & 0xFF) + 1] * (255 - rfrac1)) >> 8];
  gg1 = lut[(g1 & 0xFF) == 0xFF ? Cr_G[255] : (Cr_G[g1 & 0xFF] * gfrac1 + Cr_G[(g1 & 0xFF) + 1] * (255 - gfrac1)) >> 8];
  bb1 = lut[(b1 & 0xFF) == 0xFF ? Cr_B[255] : (Cr_B[b1 & 0xFF] * bfrac1 + Cr_B[(b1 & 0xFF) + 1] * (255 - bfrac1)) >> 8];

  uyvy->v0 = avg_chroma_1_3f(rr0 + gg0 + bb0, rr1 + gg1 + bb1);
}
#endif

LIVES_INLINE void rgb2_411(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2, uint8_t r3, uint8_t g3, uint8_t b3, yuv411_macropixel *yuv) {
  register int a;
  if ((a = ((Y_R[r0] + Y_G[g0] + Y_B[b0]) >> FP_BITS)) > max_Y) yuv->y0 = max_Y;
  else yuv->y0 = a < min_Y ? min_Y : a;
  if ((a = ((Y_R[r1] + Y_G[g1] + Y_B[b1]) >> FP_BITS)) > max_Y) yuv->y1 = max_Y;
  else yuv->y1 = a < min_Y ? min_Y : a;
  if ((a = ((Y_R[r2] + Y_G[g2] + Y_B[b2]) >> FP_BITS)) > max_Y) yuv->y2 = max_Y;
  else yuv->y2 = a < min_Y ? min_Y : a;
  if ((a = ((Y_R[r3] + Y_G[g3] + Y_B[b3]) >> FP_BITS)) > max_Y) yuv->y3 = max_Y;
  else yuv->y3 = a < min_Y ? min_Y : a;

  if ((a = ((((Cr_R[r0] + Cr_G[g0] + Cr_B[b0]) >> FP_BITS) + ((Cr_R[r1] + Cr_G[g1] + Cr_B[b1]) >> FP_BITS) +
             ((Cr_R[r2] + Cr_G[g2] + Cr_B[b2]) >> FP_BITS) + ((Cr_R[r3] + Cr_G[g3] + Cr_B[b3]) >> FP_BITS)) >> 2)) > max_UV)
    yuv->v2 = max_UV;
  else yuv->v2 = a < min_UV ? min_UV : a;
  if ((a = ((((Cb_R[r0] + Cb_G[g0] + Cb_B[b0]) >> FP_BITS) + ((Cb_R[r1] + Cb_G[g1] + Cb_B[b1]) >> FP_BITS) +
             ((Cb_R[r2] + Cb_G[g2] + Cb_B[b2]) >> FP_BITS) + ((Cb_R[r3] + Cb_G[g3] + Cb_B[b3]) >> FP_BITS)) >> 2)) > max_UV)
    yuv->u2 = max_UV;
  else yuv->u2 = a < min_UV ? min_UV : a;
}

LIVES_LOCAL_INLINE void yuv2rgb_int(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = CLAMP0255f(spc_rnd(RGB_Y[y] + R_Cr[v]));
  *g = CLAMP0255f(spc_rnd(RGB_Y[y] + G_Cb[u] + G_Cr[v]));
  *b = CLAMP0255f(spc_rnd(RGB_Y[y] + B_Cb[u]));
}

LIVES_LOCAL_INLINE void yuv2rgb_float(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = clamp0255f(RGBf_Y[y] + Rf_Cr[v]);
  *g = clamp0255f(RGBf_Y[y] + Gf_Cb[u] + Gf_Cr[v]);
  *b = clamp0255f(RGBf_Y[y] + Bf_Cb[u]);
}

#define yuv2rgb(y, u, v, r, g, b) (yuv2rgb_int((y), (u), (v), (r), (g), (b)))
//#define yuv2rgb(y, u, v, r, g, b) (yuv2rgb_float((y), (u), (v), (r), (g), (b)))
#define yuv2bgr(y, u, v, b, g, r) yuv2rgb(y, u, v, r, g, b)

LIVES_LOCAL_INLINE void yuv2rgb_with_gamma(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *lut) {
  *r = lut[(int)CLAMP0255f(spc_rnd(RGB_Y[y] + R_Cr[v]))];
  *g = lut[(int)CLAMP0255f(spc_rnd(RGB_Y[y] + G_Cb[u] + G_Cr[v]))];
  *b = lut[(int)CLAMP0255f(spc_rnd(RGB_Y[y] + B_Cb[u]))];
}

// nope...
/* LIVES_INLINE void yuv2rgb_with_gamma_float(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *lut) { */
/*   //yuv2rgb_with_gamma_int(y, u, v, r, g, b, lut); */
/*   *r = lut[(uint8_t)clamp0255f(RGBf_Y[y] + Rf_Cr[v])]; */
/*   *g = lut[(uint8_t)clamp0255f(RGBf_Y[y] + Gf_Cb[u] + Gf_Cr[v])]; */
/*   *b = lut[(uint8_t)clamp0255f(RGBf_Y[y] + Bf_Cb[u])]; */
/* } */

/* LIVES_INLINE void yuv2rgb_with_gamma(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *lut) { */
/*   if (prefs->pb_quality == PB_QUALITY_HIGH) return yuv2rgb_with_gamma_float(y, u, v, r, g, b, lut); */
/*   return yuv2rgb_with_gamma_int(y, u, v, r, g, b, lut); */
/* } */

#define yuv2bgr_with_gamma(y, u, v, b, g, r, lut) yuv2rgb_with_gamma(y, u, v, r, g, b, lut)

LIVES_INLINE void uyvy2rgb(uyvy_macropixel *uyvy, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                           uint8_t *r1, uint8_t *g1, uint8_t *b1) {
  yuv2rgb(uyvy->y0, uyvy->u0, uyvy->v0, r0, g0, b0);
  yuv2rgb(uyvy->y1, uyvy->u0, uyvy->v0, r1, g1, b1);
  //if (uyvy->y0>240||uyvy->u0>240||uyvy->v0>240||uyvy->y1>240) lives_printerr("got unclamped !\n");
}


LIVES_INLINE void yuyv2rgb(yuyv_macropixel *yuyv, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                           uint8_t *r1, uint8_t *g1, uint8_t *b1) {
  yuv2rgb(yuyv->y0, yuyv->u0, yuyv->v0, r0, g0, b0);
  yuv2rgb(yuyv->y1, yuyv->u0, yuyv->v0, r1, g1, b1);
}


LIVES_INLINE void yuv888_2_rgb(uint8_t *yuv, uint8_t *rgb, boolean add_alpha) {
  yuv2rgb(yuv[0], yuv[1], yuv[2], &(rgb[0]), &(rgb[1]), &(rgb[2]));
  if (add_alpha) rgb[3] = 255;
}


LIVES_INLINE void yuva8888_2_rgba(uint8_t *yuva, uint8_t *rgba, boolean del_alpha) {
  yuv2rgb(yuva[0], yuva[1], yuva[2], &(rgba[0]), &(rgba[1]), &(rgba[2]));
  if (!del_alpha) rgba[3] = yuva[3];
}


LIVES_INLINE void yuv888_2_bgr(uint8_t *yuv, uint8_t *bgr, boolean add_alpha) {
  yuv2bgr(yuv[0], yuv[1], yuv[2], &(bgr[0]), &(bgr[1]), &(bgr[2]));
  if (add_alpha) bgr[3] = 255;
}


LIVES_INLINE void yuva8888_2_bgra(uint8_t *yuva, uint8_t *bgra, boolean del_alpha) {
  yuv2bgr(yuva[0], yuva[1], yuva[2], &(bgra[0]), &(bgra[1]), &(bgra[2]));
  if (!del_alpha) bgra[3] = yuva[3];
}


LIVES_INLINE void yuv888_2_argb(uint8_t *yuv, uint8_t *argb) {
  argb[0] = 255;
  yuv2rgb(yuv[0], yuv[1], yuv[2], &(argb[1]), &(argb[2]), &(argb[3]));
}


LIVES_INLINE void yuva8888_2_argb(uint8_t *yuva, uint8_t *argb) {
  argb[0] = yuva[3];
  yuv2rgb(yuva[0], yuva[1], yuva[2], &(argb[1]), &(argb[2]), &(argb[3]));
}


LIVES_INLINE void uyvy_2_yuv422(uyvy_macropixel *uyvy, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) {
  *u0 = uyvy->u0;
  *y0 = uyvy->y0;
  *v0 = uyvy->v0;
  *y1 = uyvy->y1;
}


LIVES_INLINE void yuyv_2_yuv422(yuyv_macropixel *yuyv, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) {
  *y0 = yuyv->y0;
  *u0 = yuyv->u0;
  *y1 = yuyv->y1;
  *v0 = yuyv->v0;
}

/////////////////////////////////////////////////
//utilities


LIVES_GLOBAL_INLINE boolean weed_palette_is_painter_palette(int pal) {
#ifdef LIVES_PAINTER_IS_CAIRO
  if (pal == WEED_PALETTE_A8 || pal == WEED_PALETTE_A1) return TRUE;
  if (capable->hw.byte_order == LIVES_BIG_ENDIAN) {
    if (pal == WEED_PALETTE_ARGB32) return TRUE;
  } else {
    if (pal == WEED_PALETTE_BGRA32) return TRUE;
  }
#endif
  return FALSE;
}


boolean weed_palette_is_lower_quality(int p1, int p2) {
  // return TRUE if p1 is lower quality than p2
  // we don't yet handle float palettes, or RGB or alpha properly

  // currently only works well for YUV palettes

  if ((weed_palette_is_alpha(p1) && !weed_palette_is_alpha(p2)) ||
      (weed_palette_is_alpha(p2) && !weed_palette_is_alpha(p1))) return TRUE; // invalid conversion

  if (weed_palette_is_rgb(p1) && weed_palette_is_rgb(p2)) return FALSE;

  switch (p2) {
  case WEED_PALETTE_YUVA8888:
    if (p1 != WEED_PALETTE_YUVA8888 && p1 != WEED_PALETTE_YUVA4444P) return TRUE;
    break;
  case WEED_PALETTE_YUVA4444P:
    if (p1 != WEED_PALETTE_YUVA8888 && p1 != WEED_PALETTE_YUVA4444P) return TRUE;
    break;
  case WEED_PALETTE_YUV888:
    if (p1 != WEED_PALETTE_YUVA8888 && p1 != WEED_PALETTE_YUVA4444P && p1 != WEED_PALETTE_YUV444P
        && p1 != WEED_PALETTE_YUVA4444P)
      return TRUE;
    break;
  case WEED_PALETTE_YUV444P:
    if (p1 != WEED_PALETTE_YUVA8888 && p1 != WEED_PALETTE_YUVA4444P && p1 != WEED_PALETTE_YUV444P
        && p1 != WEED_PALETTE_YUVA4444P)
      return TRUE;
    break;

  case WEED_PALETTE_YUV422P:
  case WEED_PALETTE_UYVY8888:
  case WEED_PALETTE_YUYV8888:
    if (p1 != WEED_PALETTE_YUVA8888 && p1 != WEED_PALETTE_YUVA4444P && p1 != WEED_PALETTE_YUV444P &&
        p1 != WEED_PALETTE_YUVA4444P && p1 != WEED_PALETTE_YUV422P && p1 != WEED_PALETTE_UYVY8888
        && p1 != WEED_PALETTE_YUYV8888)
      return TRUE;
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    if (p1 == WEED_PALETTE_YUV411) return TRUE;
    break;
  case WEED_PALETTE_A8:
    if (p1 == WEED_PALETTE_A1) return TRUE;
  }
  return FALSE; // TODO
}

/////////////////////////////////////////////////////////


static boolean is_all_black_ish(int width, int height, int rstride, boolean has_alpha,
                                const uint8_t *pdata, boolean exact) {
  uint8_t a, b, c;
  int offs = 0;
  int psize = has_alpha ? 4 : 3;

  width *= psize;

  for (int j = 0; j < height; j++) {
    for (int i = offs; i < width; i += psize) {
      /** return FALSE if r >= 32, b >= 32 and g >= 24
        here we use a, b, and c for the first 3 bytes of the pixel. Since a and c are symmetric and we ignore byte 4,
        this will work for RGB, BGR, RGBA and BGRA (we could also check ARGB by setting offs to 1).

        Algorithm:
        (a & 0x1F) ^ a - nonzero iff a >= 32
        (c & 0x1F) ^ c - nonzero iff c >= 32

        ((a & c) & 0x1F) ^ (a & c) - nonzero only if both are true

        (b & 0x1F) ^ b  - nonzero iff b >= 32
        ((b << 1) & 0x1F) ^ (b << 1)  - nonzero iff b >= 16
        ((b << 2) & 0x1F) ^ (b << 2)  - nonzero iff b >= 8
        b & 0x0F - masks any values >= 32
      */
      a = pdata[rstride * j + i ];
      b = pdata[rstride * j + i + 1];
      c = pdata[rstride * j + i + 2];

      if (!exact) {
        if (((a & 0x1F) ^ a) & ((c & 0x1F) ^ c) & (((b & 0x1F) ^ b) | ((((b << 1) & 0x1F) ^ (b  << 1))
            & ((((b & 0x0F) << 2) & 0x1F)
               ^ ((b & 0x0F) << 2))))) return FALSE;
      } else {
        if (a | b | c) return FALSE;
      }
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf, boolean exact) {
  int width = lives_pixbuf_get_width(pixbuf);
  int height = lives_pixbuf_get_height(pixbuf);
  int rstride = lives_pixbuf_get_rowstride(pixbuf);
  boolean has_alpha = lives_pixbuf_get_has_alpha(pixbuf);
  const uint8_t *pdata = lives_pixbuf_get_pixels_readonly(pixbuf);
  return is_all_black_ish(width, height, rstride, has_alpha, pdata, exact);
}


LIVES_GLOBAL_INLINE boolean lives_layer_is_all_black(weed_layer_t *layer, boolean exact) {
  int pal = weed_layer_get_palette(layer);
  // check if palette is valid
  if (!weed_palette_is_rgb(pal) || weed_palette_is_float(pal)
      || weed_palette_get_alpha_offset(pal) != 0) return FALSE;
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int rstride = weed_layer_get_rowstride(layer);
  boolean has_alpha = weed_palette_has_alpha(pal);
  const uint8_t *pdata = weed_layer_get_pixel_data(layer);
  return is_all_black_ish(width, height, rstride, has_alpha, pdata, exact);
}


void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette, boolean contig) {
  // convert contiguous memory block planes to planar data
  // size is the byte size of the Y plane (width*height in pixels)

  switch (palette) {
  case WEED_PALETTE_YUV444P:
    if (contig) lives_memcpy(pixel_data[0], data, size * 3);
    else {
      lives_memcpy(pixel_data[0], data, size);
      lives_memcpy(pixel_data[1], (uint8_t *)data + size, size);
      lives_memcpy(pixel_data[2], (uint8_t *)data + size * 2, size);
    }
    break;
  case WEED_PALETTE_YUVA4444P:
    if (contig) lives_memcpy(pixel_data[0], data, size * 4);
    else {
      lives_memcpy(pixel_data[0], data, size);
      lives_memcpy(pixel_data[1], (uint8_t *)data + size, size);
      lives_memcpy(pixel_data[2], (uint8_t *)data + size * 2, size);
      lives_memcpy(pixel_data[3], (uint8_t *)data + size * 2, size);
    }
    break;
  case WEED_PALETTE_YUV422P:
    if (contig) lives_memcpy(pixel_data[0], data, size * 2);
    else {
      lives_memcpy(pixel_data[0], data, size);
      lives_memcpy(pixel_data[1], (uint8_t *)data + size, size / 2);
      lives_memcpy(pixel_data[2], (uint8_t *)data + size * 3 / 2, size / 2);
    }
    break;
  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    if (contig) lives_memcpy(pixel_data[0], data, size * 3 / 2);
    else {
      lives_memcpy(pixel_data[0], data, size);
      lives_memcpy(pixel_data[1], (uint8_t *)data + size, size / 4);
      lives_memcpy(pixel_data[2], (uint8_t *)data + size * 5 / 4, size / 4);
    }
    break;
  }
}


///////////////////////////////////////////////////////////
// frame conversions

static void convert_yuv888_to_rgb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
                                        int orowstride, uint8_t *dest, boolean add_alpha, int clamping, int subspace, int thread_id) {
  int x, y, i;
  size_t offs = 3;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1) {
    if (prefs->nfx_threads > 1) {
      lives_thread_t threads[prefs->nfx_threads];
      uint8_t *end = src + vsize * irowstride;
      int nthreads = 1;
      int dheight, xdheight;
      lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

      xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
      for (i = prefs->nfx_threads; i--;) {
        dheight = xdheight;

        if ((src + dheight * i * irowstride) < end) {
          ccparams[i].src = src + dheight * i * irowstride;
          ccparams[i].hsize = hsize;
          ccparams[i].dest = dest + dheight * i * orowstride;

          if (dheight * (i + 1) > (vsize - 4)) {
            dheight = vsize - (dheight * i);
          }

          ccparams[i].vsize = dheight;

          ccparams[i].irowstrides[0] = irowstride;
          ccparams[i].orowstrides[0] = orowstride;
          ccparams[i].out_alpha = add_alpha;
          ccparams[i].in_clamping = clamping;
          ccparams[i].in_subspace = subspace;
          ccparams[i].thread_id = i;

          if (i == 0) convert_yuv888_to_rgb_frame_thread(&ccparams[i]);
          else {
            lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv888_to_rgb_frame_thread, &ccparams[i]);
            nthreads++;
          }
        }
      }

      for (i = 1; i < nthreads; i++) {
        lives_thread_join(threads[i], NULL);
      }
      lives_free(ccparams);
      return;
    }
  }

  if (add_alpha) offs = 4;
  orowstride -= offs * hsize;
  irowstride -= hsize * 3;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuv888_2_rgb(src, dest, add_alpha);
      src += 3;
      dest += offs;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuv888_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv888_to_rgb_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                              ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->out_alpha,
                              ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuva8888_to_rgba_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, boolean del_alpha, int clamping, int subspace, int thread_id) {
  register int x, y, i;

  size_t offs = 4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + vsize * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = !del_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuva8888_to_rgba_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuva8888_to_rgba_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (del_alpha) offs = 3;
  orowstride -= offs * hsize;
  irowstride -= hsize * 4;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuva8888_2_rgba(src, dest, del_alpha);
      src += 4;
      dest += offs;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuva8888_to_rgba_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuva8888_to_rgba_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                 ccparams->orowstrides[0], (uint8_t *)ccparams->dest, !ccparams->out_alpha,
                                 ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuv888_to_bgr_frame(uint8_t *src, int hsize, int vsize, int irowstride,
                                        int orowstride, uint8_t *dest, boolean add_alpha, int clamping, int subspace, int thread_id) {
  int x, y, i;
  size_t offs = 3;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + vsize * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = add_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuv888_to_bgr_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv888_to_bgr_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (add_alpha) offs = 4;
  orowstride -= offs * hsize;
  irowstride -= hsize * 3;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuv888_2_bgr(src, dest, add_alpha);
      src += 3;
      dest += offs;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuv888_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv888_to_bgr_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                              ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->out_alpha,
                              ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuva8888_to_bgra_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, boolean del_alpha, int clamping, int subspace, int thread_id) {
  int x, y, i;

  size_t offs = 4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + vsize * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = !del_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuva8888_to_bgra_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuva8888_to_bgra_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (del_alpha) offs = 3;
  orowstride -= offs * hsize;
  irowstride -= 4 * hsize;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuva8888_2_bgra(src, dest, del_alpha);
      src += 4;
      dest += offs;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuva8888_to_bgra_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuva8888_to_bgra_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                 ccparams->orowstrides[0], (uint8_t *)ccparams->dest, !ccparams->out_alpha,
                                 ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuv888_to_argb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, int clamping, int subspace, int thread_id) {
  int x, y, i;
  size_t offs = 4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + vsize * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuv888_to_argb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv888_to_argb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= offs * hsize;
  irowstride -= hsize * 3;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuv888_2_argb(src, dest);
      src += 3;
      dest += 4;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuv888_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv888_to_argb_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                               ccparams->orowstrides[0], (uint8_t *)ccparams->dest,
                               ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuva8888_to_argb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, int clamping, int subspace, int thread_id) {
  int x, y, i;

  size_t offs = 4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + vsize * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuva8888_to_argb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuva8888_to_rgba_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= offs * hsize;
  irowstride -= hsize * 4;

  for (y = 0; y < vsize; y++) {
    for (x = 0; x < hsize; x++) {
      yuva8888_2_argb(src, dest);
      src += 4;
      dest += 4;
    }
    dest += orowstride;
    src += irowstride;
  }
}


static void *convert_yuva8888_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuva8888_to_argb_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                 ccparams->orowstrides[0], (uint8_t *)ccparams->dest,
                                 ccparams->in_clamping, ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_yuv420p_to_rgb_frame(uint8_t **LIVES_RESTRICT src, int width, int height, boolean is_bottom, int *istrides,
    int orowstride,
    uint8_t *dest, boolean add_alpha, boolean is_422, int sampling, int clamping, int subspace,
    int gamma, int tgamma, uint8_t *LIVES_RESTRICT gamma_lut, int thread_id) {
  int i, j;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  int opsize = 3;
  int irow = istrides[0];
  int r2 = 0;
  boolean even = TRUE;
  size_t uv_offs = 0, y_offs = 0;
  uint8_t y, u, v, next_u, next_v, last_u, last_v;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    set_conversion_arrays(clamping, subspace);
    if (tgamma) {
      gamma_lut = create_gamma_lut(1.0, gamma, tgamma);
    }
#if USE_THREADS
  }
#endif

  if (thread_id == -1) {
    if (prefs->nfx_threads > 1) {
      lives_thread_t threads[prefs->nfx_threads];
      uint8_t *end = src[0] + height * istrides[0];
      int nthreads = 1;
      int dheight, xdheight;
      lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

      xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
      for (i = prefs->nfx_threads; i--;) {
        dheight = xdheight;

        if ((src[0] + dheight * i * istrides[0]) < end) {
          ccparams[i].srcp[0] = src[0] + dheight * i * istrides[0];
          ccparams[i].srcp[1] = src[1] + dheight / 2 * i * istrides[1];
          ccparams[i].srcp[2] = src[2] + dheight / 2 * i * istrides[2];
          ccparams[i].hsize = width;
          ccparams[i].dest = dest + dheight * i * orowstride;

          if (dheight * (i + 1) > (height - 4)) {
            dheight = height - (dheight * i);
          }

          ccparams[i].vsize = dheight;
          if (i == prefs->nfx_threads - 1) {
            ccparams[i].is_bottom = TRUE;
          }
          ccparams[i].irowstrides[0] = istrides[0];
          ccparams[i].irowstrides[1] = istrides[1];
          ccparams[i].irowstrides[2] = istrides[2];
          ccparams[i].orowstrides[0] = orowstride;
          ccparams[i].out_alpha = add_alpha;
          ccparams[i].in_sampling = sampling;
          ccparams[i].in_clamping = clamping;
          ccparams[i].in_subspace = subspace;
          ccparams[i].is_422 = is_422;
          ccparams[i].lut = gamma_lut;
          ccparams[i].thread_id = i;

          if (i == 0) convert_yuv420p_to_rgb_frame_thread(&ccparams[i]);
          else {
            lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv420p_to_rgb_frame_thread, &ccparams[i]);
            nthreads++;
          }
        }
      }

      for (i = 1; i < nthreads; i++) {
        lives_thread_join(threads[i], NULL);
      }
      lives_free(ccparams);
      if (gamma_lut) lives_gamma_lut_free(gamma_lut);
      return;
    }
  }

  if (add_alpha) opsize = 4;
  width *= opsize;

  for (i = 0; i < height; i++) {
    if (!is_422) {
      if (!(i & 1)) even = TRUE;
      else even = FALSE;
    }

    uv_offs = y_offs = 0;

    for (j = 0; j < width; j += opsize) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width
      y = s_y[irow * i + y_offs++];
      /// even row, normal
      if (j > 0) {
        /// center = 3 : 1, left = only next, right = avg(last, next)
        u = avg_chroma_3_1(next_u, last_u);
        v = avg_chroma_3_1(next_v, last_v);
        last_u = next_u;
        last_v = next_v;
      } else {
        if (even) {
          last_u = next_u = u = s_u[istrides[1] * r2 + uv_offs];
          last_v = next_v = v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          if (is_bottom && i >= height - 3) {
            next_u = u = s_u[istrides[1] * r2 + uv_offs];
            next_v = v = s_v[istrides[2] * r2 + uv_offs];
          } else {
            next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
            next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
          }
        }
      }
      if (gamma_lut)
        yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                           &dest[orowstride * i + j + 2], gamma_lut);
      else
        yuv2rgb(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2]);
      if (add_alpha) dest[orowstride * i + j + 3] = 255;

      // second RGB pixel
      j += opsize;
      y = s_y[irow * i + y_offs++];

      last_u = next_u;
      last_v = next_v;

      if (j < width - 1) {
        if (even) {
          /// even row, normal
          next_u = s_u[istrides[1] * r2 + uv_offs];
          next_v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          if (is_bottom && i >= height - 3) {
            next_u = u = s_u[istrides[1] * r2 + uv_offs];
            next_v = v = s_v[istrides[2] * r2 + uv_offs];
          } else {
            //g_print("vals %ld and %d %d %d\n", uv_offs, istrides[2], i, j);
            next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
            next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
          }
        }
        /// center = 3 : 1, left = avg(last, next), right = only next
        u = avg_chroma_3_1(next_u, last_u);
        v = avg_chroma_3_1(next_v, last_v);
      } else {
        u = last_u;
        v = last_v;
      }
      if (gamma_lut)
        yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                           &dest[orowstride * i + j + 2], gamma_lut);
      else
        yuv2rgb(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                &dest[orowstride * i + j + 2]);
      if (add_alpha) dest[orowstride * i + j + 3] = 255;
      uv_offs++;
    }
    if (is_422 || !even) r2++;
  }
}

static void *convert_yuv420p_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv420p_to_rgb_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize,
                               ccparams->is_bottom, ccparams->irowstrides,
                               ccparams->orowstrides[0], (uint8_t *)ccparams->dest,
                               ccparams->out_alpha, ccparams->is_422, ccparams->in_sampling,
                               ccparams->in_clamping, ccparams->in_subspace, 0, 0,
                               ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_yuv420p_to_bgr_frame(uint8_t **LIVES_RESTRICT src, int width, int height, boolean is_bottom, int *istrides,
    int orowstride,
    uint8_t *dest, boolean add_alpha, boolean is_422, int sampling, int clamping, int subspace,
    int gamma, int tgamma, uint8_t *LIVES_RESTRICT gamma_lut, int thread_id) {
  int i, j;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  int opsize = 3;
  int irow = istrides[0];
  int r2 = 0;
  boolean even = TRUE;
  size_t uv_offs = 0, y_offs = 0;
  uint8_t y, u, v, next_u, next_v, last_u, last_v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    /// TODO: this is NOT threadsafe !!!!
    set_conversion_arrays(clamping, subspace);
    if (tgamma) gamma_lut = create_gamma_lut(1.0, gamma, tgamma);
#if USE_THREADS
  }
#endif

  if (thread_id == -1) {
    if (prefs->nfx_threads > 1) {
      lives_thread_t threads[prefs->nfx_threads];
      uint8_t *end = src[0] + height * istrides[0];
      int nthreads = 1;
      int dheight, xdheight;
      lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

      xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
      for (i = prefs->nfx_threads; i--;) {
        dheight = xdheight;

        if ((src[0] + dheight * i * istrides[0]) < end) {
          ccparams[i].srcp[0] = src[0] + dheight * i * istrides[0];
          ccparams[i].srcp[1] = src[1] + dheight / 2 * i * istrides[1];
          ccparams[i].srcp[2] = src[2] + dheight / 2 * i * istrides[2];
          ccparams[i].hsize = width;
          ccparams[i].dest = dest + dheight * i * orowstride;

          if (dheight * (i + 1) > (height - 4)) {
            dheight = height - (dheight * i);
          }

          ccparams[i].vsize = dheight;
          if (i == prefs->nfx_threads - 1) {
            ccparams[i].is_bottom = TRUE;
          }
          ccparams[i].irowstrides[0] = istrides[0];
          ccparams[i].irowstrides[1] = istrides[1];
          ccparams[i].irowstrides[2] = istrides[2];
          ccparams[i].orowstrides[0] = orowstride;
          ccparams[i].out_alpha = add_alpha;
          ccparams[i].in_sampling = sampling;
          ccparams[i].in_clamping = clamping;
          ccparams[i].in_subspace = subspace;
          ccparams[i].is_422 = is_422;
          ccparams[i].lut = gamma_lut;
          ccparams[i].thread_id = i;

          if (i == 0) convert_yuv420p_to_bgr_frame_thread(&ccparams[i]);
          else {
            lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv420p_to_bgr_frame_thread, &ccparams[i]);
            nthreads++;
          }
        }
      }

      for (i = 1; i < nthreads; i++) {
        lives_thread_join(threads[i], NULL);
      }
      lives_free(ccparams);
      if (gamma_lut) lives_gamma_lut_free(gamma_lut);
      return;
    }
  }

  if (add_alpha) opsize = 4;
  width *= opsize;

  for (i = 0; i < height; i++) {
    if (!is_422) {
      if (!(i & 1)) even = TRUE;
      else even = FALSE;
    }

    uv_offs = y_offs = 0;

    for (j = 0; j < width; j += opsize) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width
      y = s_y[irow * i + y_offs++];
      /// even row, normal
      if (j > 0) {
        /// center = 3 : 1, left = only next, right = avg(last, next)
        u = avg_chroma_3_1(next_u, last_u);
        v = avg_chroma_3_1(next_v, last_v);
        last_u = next_u;
        last_v = next_v;
      } else {
        if (even) {
          last_u = next_u = u = s_u[istrides[1] * r2 + uv_offs];
          last_v = next_v = v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          if (is_bottom && i >= height - 3) {
            next_u = u = s_u[istrides[1] * r2 + uv_offs];
            next_v = v = s_v[istrides[2] * r2 + uv_offs];
          } else {
            next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
            next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
          }
        }
      }

      if (gamma_lut)
        yuv2bgr_with_gamma(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                           &dest[orowstride * i + j + 2], gamma_lut);
      else
        yuv2bgr(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2]);
      if (add_alpha) dest[orowstride * i + j + 3] = 255;

      // second RGB pixel
      j += opsize;
      y = s_y[irow * i + y_offs++];

      last_u = next_u;
      last_v = next_v;

      if (j < width - 1) {
        if (even) {
          /// even row, normal
          next_u = s_u[istrides[1] * r2 + uv_offs];
          next_v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          if (is_bottom && i >= height - 3) {
            next_u = u = s_u[istrides[1] * r2 + uv_offs];
            next_v = v = s_v[istrides[2] * r2 + uv_offs];
          } else {
            //g_print("vals %ld and %d %d %d\n", uv_offs, istrides[2], i, j);
            next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
            next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
          }
        }
        /// center = 3 : 1, left = avg(last, next), right = only next
        u = avg_chroma_3_1(next_u, last_u);
        v = avg_chroma_3_1(next_v, last_v);
      } else {
        u = last_u;
        v = last_v;
      }

      if (gamma_lut)
        yuv2bgr_with_gamma(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                           &dest[orowstride * i + j + 2], gamma_lut);
      else
        yuv2bgr(y, u, v, &dest[orowstride * i + j], &dest[orowstride * i + j + 1],
                &dest[orowstride * i + j + 2]);
      if (add_alpha) dest[orowstride * i + j + 3] = 255;
      uv_offs++;
    }
    if (is_422 || !even) r2++;
  }
}

static void *convert_yuv420p_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv420p_to_bgr_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize,
                               ccparams->is_bottom, ccparams->irowstrides,
                               ccparams->orowstrides[0], (uint8_t *)ccparams->dest,
                               ccparams->out_alpha, ccparams->is_422, ccparams->in_sampling,
                               ccparams->in_clamping, ccparams->in_subspace, 0, 0,
                               ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_yuv420p_to_argb_frame(uint8_t **LIVES_RESTRICT src, int width, int height, boolean is_bottom, int *istrides,
    int orowstride,
    uint8_t *dest, boolean is_422, int sampling, int clamping, int subspace,
    int gamma, int tgamma, uint8_t *LIVES_RESTRICT gamma_lut, int thread_id) {
  int i, j;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  int opsize = 4;
  int irow = istrides[0];
  int r2 = 0;
  boolean even = TRUE;
  size_t uv_offs = 0, y_offs = 0;
  uint8_t y, u, v, next_u, next_v, last_u, last_v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    /// TODO: this is NOT threadsafe !!!!
    set_conversion_arrays(clamping, subspace);
    if (tgamma) gamma_lut = create_gamma_lut(1.0, gamma, tgamma);
#if USE_THREADS
  }
#endif

  if (thread_id == -1) {
    if (prefs->nfx_threads > 1) {
      lives_thread_t threads[prefs->nfx_threads];
      uint8_t *end = src[0] + height * istrides[0];
      int nthreads = 1;
      int dheight, xdheight;
      lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

      xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
      for (i = prefs->nfx_threads; i--;) {
        dheight = xdheight;

        if ((src[0] + dheight * i * istrides[0]) < end) {
          ccparams[i].srcp[0] = src[0] + dheight * i * istrides[0];
          ccparams[i].srcp[1] = src[1] + dheight / 2 * i * istrides[1];
          ccparams[i].srcp[2] = src[2] + dheight / 2 * i * istrides[2];
          ccparams[i].hsize = width;
          ccparams[i].dest = dest + dheight * i * orowstride;

          if (dheight * (i + 1) > (height - 4)) {
            dheight = height - (dheight * i);
          }

          ccparams[i].vsize = dheight;
          if (i == prefs->nfx_threads - 1) {
            ccparams[i].is_bottom = TRUE;
          }
          ccparams[i].irowstrides[0] = istrides[0];
          ccparams[i].irowstrides[1] = istrides[1];
          ccparams[i].irowstrides[2] = istrides[2];
          ccparams[i].orowstrides[0] = orowstride;
          ccparams[i].in_sampling = sampling;
          ccparams[i].in_clamping = clamping;
          ccparams[i].in_subspace = subspace;
          ccparams[i].is_422 = is_422;
          ccparams[i].lut = gamma_lut;
          ccparams[i].thread_id = i;

          if (i == 0) convert_yuv420p_to_argb_frame_thread(&ccparams[i]);
          else {
            lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv420p_to_argb_frame_thread, &ccparams[i]);
            nthreads++;
          }
        }
      }

      for (i = 1; i < nthreads; i++) {
        lives_thread_join(threads[i], NULL);
      }
      lives_free(ccparams);
      if (gamma_lut) lives_gamma_lut_free(gamma_lut);
      return;
    }
  }

  width *= opsize;

  for (i = 0; i < height; i++) {
    if (!is_422) {
      if (!(i & 1)) even = TRUE;
      else even = FALSE;
    }

    uv_offs = y_offs = 0;
    j = 0;

    // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
    // we know we can do this because Y must be even width
    y = s_y[irow * i + y_offs++];
    /// even row, normal

    if (even) {
      last_u = next_u = u = s_u[istrides[1] * r2 + uv_offs];
      last_v = next_v = v = s_v[istrides[2] * r2 + uv_offs];
    } else {
      if (is_bottom && i >= height - 3) {
        next_u = u = s_u[istrides[1] * r2 + uv_offs];
        next_v = v = s_v[istrides[2] * r2 + uv_offs];
      } else {
        next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
        next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
      }
    }

    dest[orowstride * i + j] = 255;
    if (gamma_lut)
      yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
                         &dest[orowstride * i + j + 3], gamma_lut);
    else
      yuv2rgb(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2], &dest[orowstride * i + j + 3]);

    // second RGB pixel
    j += opsize;
    y = s_y[irow * i + y_offs++];

    last_u = next_u;
    last_v = next_v;

    if (j < width - 1) {
      if (even) {
        /// even row, normal
        next_u = s_u[istrides[1] * r2 + uv_offs];
        next_v = s_v[istrides[2] * r2 + uv_offs];
      } else {
        if (is_bottom && i >= height - 3) {
          next_u = u = s_u[istrides[1] * r2 + uv_offs];
          next_v = v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          //g_print("vals %ld and %d %d %d\n", uv_offs, istrides[2], i, j);
          next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
          next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
        }
      }
      /// center = 3 : 1, left = avg(last, next), right = only next
      u = avg_chroma_3_1(next_u, last_u);
      v = avg_chroma_3_1(next_v, last_v);
    } else {
      u = last_u;
      v = last_v;
    }

    dest[orowstride * i + j] = 255;
    if (gamma_lut)
      yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
                         &dest[orowstride * i + j + 3], gamma_lut);
    else
      yuv2rgb(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
              &dest[orowstride * i + j + 3]);
    uv_offs++;

    for (j = opsize; j < width; j += opsize) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width
      y = s_y[irow * i + y_offs++];
      /// even row, normal
      /// center = 3 : 1, left = only next, right = avg(last, next)
      u = avg_chroma_3_1(next_u, last_u);
      v = avg_chroma_3_1(next_v, last_v);
      last_u = next_u;
      last_v = next_v;

      dest[orowstride * i + j] = 255;
      if (gamma_lut)
        yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
                           &dest[orowstride * i + j + 3], gamma_lut);
      else
        yuv2rgb(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2], &dest[orowstride * i + j + 3]);

      // second RGB pixel
      j += opsize;
      y = s_y[irow * i + y_offs++];

      last_u = next_u;
      last_v = next_v;

      if (j < width - 1) {
        if (even) {
          /// even row, normal
          next_u = s_u[istrides[1] * r2 + uv_offs];
          next_v = s_v[istrides[2] * r2 + uv_offs];
        } else {
          if (is_bottom && i >= height - 3) {
            next_u = u = s_u[istrides[1] * r2 + uv_offs];
            next_v = v = s_v[istrides[2] * r2 + uv_offs];
          } else {
            //g_print("vals %ld and %d %d %d\n", uv_offs, istrides[2], i, j);
            next_u = u = avg_chromaf(s_u[istrides[1] * r2 + uv_offs], s_u[istrides[1] * r2 + uv_offs + istrides[1]]);
            next_v = v = avg_chromaf(s_v[istrides[2] * r2 + uv_offs], s_v[istrides[2] * r2 + uv_offs + istrides[2]]);
          }
        }
        /// center = 3 : 1, left = avg(last, next), right = only next
        u = avg_chroma_3_1(next_u, last_u);
        v = avg_chroma_3_1(next_v, last_v);
      } else {
        u = last_u;
        v = last_v;
      }

      dest[orowstride * i + j] = 255;
      if (gamma_lut)
        yuv2rgb_with_gamma(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
                           &dest[orowstride * i + j + 3], gamma_lut);
      else
        yuv2rgb(y, u, v, &dest[orowstride * i + j + 1], &dest[orowstride * i + j + 2],
                &dest[orowstride * i + j + 3]);
      uv_offs++;
    }

    if (is_422 || !even) r2++;
  }
}

static void *convert_yuv420p_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv420p_to_argb_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize,
                                ccparams->is_bottom, ccparams->irowstrides,
                                ccparams->orowstrides[0], (uint8_t *)ccparams->dest,
                                ccparams->is_422, ccparams->in_sampling,
                                ccparams->in_clamping, ccparams->in_subspace, 0, 0,
                                ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                      uyvy_macropixel *u, boolean has_alpha, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 3, ipsize2;
  uint8_t *end;
  int i;

  int x = 3, y = 4, z = 5;
  hsize = (hsize >> 1) << 1;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    /// TODO: this is NOT threadsafe !!!!
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);
#if USE_THREADS
  }
#endif

  end = rgbdata + rowstride * vsize;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_rgb_to_uyvy_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_rgb_to_uyvy_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize = 4;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;
  for (int k = 0; k < vsize; k++) {
    for (i = 0; i < hs3; i += ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      if (gamma_lut)
        rgb2uyvy_with_gamma(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + x], rgbdata[i + y],
                            rgbdata[i + z], u++, gamma_lut);
      else
        rgb2uyvy(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + x], rgbdata[i + y], rgbdata[i + z], u++);
    }
    rgbdata += rowstride;
    u += orowstride;
  }
}


static void *convert_rgb_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_rgb_to_uyvy_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (uyvy_macropixel *)ccparams->dest, ccparams->in_alpha, ccparams->out_clamping, ccparams->lut,
                            ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                      yuyv_macropixel *u, boolean has_alpha, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 3, ipsize2;
  uint8_t *end = rgbdata + rowstride * vsize;
  int i;

  int x = 3, y = 4, z = 5;
  hsize = (hsize >> 1) << 1;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    /// TODO: this is NOT threadsafe !!!!
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_rgb_to_yuyv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_rgb_to_yuyv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize = 4;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipsize2) {
      // convert 6 RGBRGB bytes to 4 YUYV bytes
      if (gamma_lut)
        rgb2yuyv_with_gamma(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + x], rgbdata[i + y],
                            rgbdata[i + z], u++, gamma_lut);
      else
        rgb2yuyv(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + x], rgbdata[i + y], rgbdata[i + z], u++);
    }
    u += orowstride;
  }
}


static void *convert_rgb_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_rgb_to_yuyv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (yuyv_macropixel *)ccparams->dest, ccparams->in_alpha, ccparams->out_clamping, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_bgr_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                      uyvy_macropixel *u, boolean has_alpha, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 3, ipsize2;
  uint8_t *end = rgbdata + rowstride * vsize;
  int i;

  int x = 3, y = 4, z = 5;
  hsize = (hsize >> 1) << 1;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
#if USE_THREADS
  if (thread_id == -1) {
#endif
    /// TODO: this is NOT threadsafe !!!!
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);
#if USE_THREADS
  }
#endif

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_bgr_to_uyvy_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_bgr_to_uyvy_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize = 4;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipsize2) {
      //convert 6 RGBRGB bytes to 4 UYVY bytes
      if (gamma_lut)
        rgb2uyvy_with_gamma(rgbdata[i + 2], rgbdata[i + 1], rgbdata[i], rgbdata[i + z], rgbdata[i + y],
                            rgbdata[i + x], u++, gamma_lut);
      else
        rgb2uyvy(rgbdata[i + 2], rgbdata[i + 1], rgbdata[i], rgbdata[i + z], rgbdata[i + y], rgbdata[i + x], u++);
    }
    u += orowstride;
  }
}


static void *convert_bgr_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_bgr_to_uyvy_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (uyvy_macropixel *)ccparams->dest, ccparams->in_alpha, ccparams->out_clamping, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_bgr_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                      yuyv_macropixel *u, boolean has_alpha, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 3, ipsize2;

  uint8_t *end = rgbdata + rowstride * vsize;
  int i;

  int x = 3, y = 4, z = 5;

  hsize = (hsize >> 1) << 1;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_bgr_to_yuyv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_bgr_to_yuyv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize = 4;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      if (gamma_lut)
        rgb2yuyv_with_gamma(rgbdata[i + 2], rgbdata[i + 1], rgbdata[i], rgbdata[i + z], rgbdata[i + y],
                            rgbdata[i + x], u++, gamma_lut);
      else
        rgb2yuyv(rgbdata[i + 2], rgbdata[i + 1], rgbdata[i], rgbdata[i + z], rgbdata[i + y], rgbdata[i + x], u++);
    }
    u += orowstride;
  }
}


static void *convert_bgr_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_bgr_to_yuyv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (yuyv_macropixel *)ccparams->dest, ccparams->in_alpha, ccparams->out_clamping, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_argb_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                       uyvy_macropixel *u, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 4, ipsize2;
  uint8_t *end;
  int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  end = rgbdata + rowstride * vsize;
  hsize = (hsize >> 1) << 1;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_argb_to_uyvy_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_argb_to_uyvy_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      if (gamma_lut)
        rgb2uyvy_with_gamma(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], rgbdata[i + 5], rgbdata[i + 6],
                            rgbdata[i + 7], u++, gamma_lut);
      else
        rgb2uyvy(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], rgbdata[i + 5], rgbdata[i + 6], rgbdata[i + 7], u++);
    }
    u += orowstride;
  }
}


static void *convert_argb_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_argb_to_uyvy_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                             ccparams->orowstrides[0],
                             (uyvy_macropixel *)ccparams->dest, ccparams->out_clamping, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_argb_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orowstride,
                                       yuyv_macropixel *u, int clamping, uint8_t *gamma_lut, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3, ipsize = 4, ipsize2;
  uint8_t *end;
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  end = rgbdata + rowstride * vsize;
  hsize = (hsize >> 1) << 1;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orowstride / 4;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_clamping = clamping;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_argb_to_yuyv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_argb_to_yuyv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  ipsize2 = ipsize * 2;
  hs3 = hsize * ipsize;
  orowstride = orowstride / 2 - hsize;
  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      if (gamma_lut)
        rgb2yuyv_with_gamma(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], rgbdata[i + 5], rgbdata[i + 6],
                            rgbdata[i + 7], u++, gamma_lut);
      else
        rgb2yuyv(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], rgbdata[i + 5], rgbdata[i + 6], rgbdata[i + 7], u++);
    }
    u += orowstride;
  }
}


static void *convert_argb_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_argb_to_yuyv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                             ccparams->orowstrides[0],
                             (yuyv_macropixel *)ccparams->dest, ccparams->out_clamping, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                     uint8_t *u, boolean in_has_alpha, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 3, opsize = 3;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  int i;
  uint8_t in_alpha = 255;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orow;
        ccparams[i].in_alpha = in_has_alpha;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_rgb_to_yuv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_rgb_to_yuv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (in_has_alpha) ipsize = 4;
  if (out_has_alpha) opsize = 4;

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize * opsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      if (in_has_alpha) in_alpha = rgbdata[i + 3];
      if (out_has_alpha) u[3] = in_alpha;
      rgb2yuv(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], &(u[0]), &(u[1]), &(u[2]));
      u += opsize;
    }
    u += orow;
  }
}


static void *convert_rgb_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_rgb_to_yuv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                           ccparams->orowstrides[0],
                           (uint8_t *)ccparams->dest, ccparams->in_alpha, ccparams->out_alpha, ccparams->out_clamping,
                           ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                      uint8_t **yuvp, boolean in_has_alpha, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 3;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  register int i;
  uint8_t in_alpha = 255, *a = NULL;

  uint8_t *y, *u, *v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  y = yuvp[0];
  u = yuvp[1];
  v = yuvp[2];
  if (out_has_alpha) a = yuvp[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;

        ccparams[i].destp[0] = y + dheight * i * orow;
        ccparams[i].destp[1] = u + dheight * i * orow;
        ccparams[i].destp[2] = v + dheight * i * orow;
        if (out_has_alpha) ccparams[i].destp[3] = a + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orow;
        ccparams[i].in_alpha = in_has_alpha;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_rgb_to_yuvp_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_rgb_to_yuvp_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (in_has_alpha) ipsize = 4;

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      if (in_has_alpha) in_alpha = rgbdata[i + 3];
      if (out_has_alpha) *(a++) = in_alpha;
      rgb2yuv(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], y, u, v);
      y++;
      u++;
      v++;
    }
    y += orow;
    u += orow;
    v += orow;
    if (out_has_alpha) a += orow;
  }
}


static void *convert_rgb_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_rgb_to_yuvp_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (uint8_t **)ccparams->destp, ccparams->in_alpha, ccparams->out_alpha, ccparams->out_clamping,
                            ccparams->thread_id);
  return NULL;
}


static void convert_bgr_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                     uint8_t *u, boolean in_has_alpha, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 3, opsize = 3;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  register int i;
  uint8_t in_alpha = 255;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;
        ccparams[i].dest = u + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].in_alpha = in_has_alpha;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_bgr_to_yuv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_bgr_to_yuv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (in_has_alpha) ipsize = 4;
  if (out_has_alpha) opsize = 4;

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize * opsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      bgr2yuv(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], &(u[0]), &(u[1]), &(u[2]));
      if (in_has_alpha) in_alpha = rgbdata[i + 3];
      if (out_has_alpha) u[3] = in_alpha;
      u += opsize;
    }
    u += orow;
  }
}


static void *convert_bgr_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_bgr_to_yuv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                           ccparams->orowstrides[0],
                           (uint8_t *)ccparams->dest, ccparams->in_alpha, ccparams->out_alpha, ccparams->out_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_bgr_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                      uint8_t **yuvp, boolean in_has_alpha, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 3;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  register int i;
  uint8_t in_alpha = 255, *a = NULL;

  uint8_t *y, *u, *v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  y = yuvp[0];
  u = yuvp[1];
  v = yuvp[2];
  if (out_has_alpha) a = yuvp[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;

        ccparams[i].destp[0] = y + dheight * i * orow;
        ccparams[i].destp[1] = u + dheight * i * orow;
        ccparams[i].destp[2] = v + dheight * i * orow;
        if (out_has_alpha) ccparams[i].destp[3] = a + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orow;
        ccparams[i].in_alpha = in_has_alpha;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_bgr_to_yuvp_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_bgr_to_yuvp_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (in_has_alpha) ipsize = 4;

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      bgr2yuv(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], &(y[0]), &(u[0]), &(v[0]));
      if (in_has_alpha) in_alpha = rgbdata[i + 3];
      if (out_has_alpha) *(a++) = in_alpha;
      y++;
      u++;
      v++;
    }
    y += orow;
    u += orow;
    v += orow;
    if (out_has_alpha) a += orow;
  }
}


static void *convert_bgr_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_bgr_to_yuvp_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (uint8_t **)ccparams->destp, ccparams->in_alpha, ccparams->out_alpha, ccparams->out_clamping,
                            ccparams->thread_id);
  return NULL;
}


static void convert_argb_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                      uint8_t *u, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 4, opsize = 3;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;

        ccparams[i].dest = u + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orow;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_rgb_to_yuv_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_argb_to_yuv_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (out_has_alpha) opsize = 4;

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize * opsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      if (out_has_alpha) u[3] = rgbdata[i];
      rgb2yuv(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], &(u[0]), &(u[1]), &(u[2]));
      u += opsize;
    }
    u += orow;
  }
}


static void *convert_argb_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_argb_to_yuv_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest, ccparams->out_alpha, ccparams->out_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_argb_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int orow,
                                       uint8_t **yuvp, boolean out_has_alpha, int clamping, int thread_id) {
  int ipsize = 4;
  int iwidth;
  uint8_t *end = rgbdata + (rowstride * vsize);
  register int i;
  uint8_t *a = NULL;
  uint8_t *y, *u, *v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  y = yuvp[0];
  u = yuvp[1];
  v = yuvp[2];
  if (out_has_alpha) a = yuvp[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)vsize / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((rgbdata + dheight * i * rowstride) < end) {
        ccparams[i].src = rgbdata + dheight * i * rowstride;
        ccparams[i].hsize = hsize;

        ccparams[i].destp[0] = y + dheight * i * orow;
        ccparams[i].destp[1] = u + dheight * i * orow;
        ccparams[i].destp[2] = v + dheight * i * orow;
        if (out_has_alpha) ccparams[i].destp[3] = a + dheight * i * orow;

        if (dheight * (i + 1) > (vsize - 4)) {
          dheight = vsize - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].orowstrides[0] = orow;
        ccparams[i].out_alpha = out_has_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_argb_to_yuvp_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_argb_to_yuvp_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  hsize = (hsize >> 1) << 1;
  iwidth = hsize * ipsize;
  orow -= hsize;

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < iwidth; i += ipsize) {
      if (out_has_alpha) *(a++) = rgbdata[i];
      rgb2yuv(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], y, u, v);
      y++;
      u++;
      v++;
    }
    y += orow;
    u += orow;
    v += orow;
    if (out_has_alpha) a += orow;
  }
}


static void *convert_argb_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_argb_to_yuvp_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                             ccparams->orowstrides[0],
                             (uint8_t **)ccparams->destp, ccparams->out_alpha, ccparams->out_clamping,
                             ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int *ostrides,
                                        uint8_t **dest, boolean is_422, boolean has_alpha, int subspace, int clamping) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  uint16_t *rgbdata16 = NULL;
  uint8_t *y, *Cb, *Cr;
  uyvy_macropixel u;
  boolean chroma_row = TRUE;
  size_t hhsize;
  int hs3;
  int ipsize = 3, ipsize2;
  boolean is16bit = FALSE;
  register int i, j;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, subspace);

  if (has_alpha) ipsize = 4;
  if (hsize < 0) {
    is16bit = TRUE;
    hsize = -hsize;
    rgbdata16 = (uint16_t *)rgbdata;
  }

  // ensure width and height are both divisible by two
  hsize = (hsize >> 1) << 1;
  vsize = (vsize >> 1) << 1;

  y = dest[0];
  Cb = dest[1];
  Cr = dest[2];

  hhsize = hsize >> 1;
  ipsize2 = ipsize * 2;
  hs3 = (hsize * ipsize) - (ipsize2 - 1);

  for (i = 0; i < vsize; i++) {
    for (j = 0; j < hs3; j += ipsize2) {
      // mpeg style, Cb and Cr are co-located
      // convert 6 RGBRGB bytes to 4 UYVY bytes

      // TODO: for mpeg use rgb2yuv and write alternate u and v
      if (is16bit) {
        rgb16_2uyvy(rgbdata16[j], rgbdata16[j + 1], rgbdata16[j + 2], rgbdata16[j + ipsize], rgbdata16[j + ipsize + 1],
                    rgbdata16[j + ipsize + 2], &u);
      } else rgb2uyvy(rgbdata[j], rgbdata[j + 1], rgbdata[j + 2], rgbdata[j + ipsize], rgbdata[j + ipsize + 1],
                        rgbdata[j + ipsize + 2], &u);

      *(y++) = u.y0;
      *(y++) = u.y1;
      *(Cb++) = u.u0;
      *(Cr++) = u.v0;

      if (!is_422 && chroma_row && i > 0) {
        // average two rows
        Cb[-1 - ostrides[1]] = avg_chromaf(Cb[-1], Cb[-1 - ostrides[1]]);
        Cr[-1 - ostrides[1]] = avg_chromaf(Cr[-1], Cr[-1 - ostrides[1]]);
      }

    }
    if (!is_422) {
      if (chroma_row) {
        Cb -= hhsize;
        Cr -= hhsize;
      }
      chroma_row = !chroma_row;
    }
    rgbdata += rowstride;
  }
}


static void convert_argb_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int *ostrides,
    uint8_t **dest, boolean is_422, int subspace, int clamping) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  int hs3;

  uint8_t *y, *Cb, *Cr;
  uyvy_macropixel u;
  register int i, j;
  boolean chroma_row = TRUE;

  int ipsize = 4, ipsize2;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, subspace);

  // ensure width and height are both divisible by two
  hsize = (hsize >> 1) << 1;
  vsize = (vsize >> 1) << 1;

  y = dest[0];
  Cb = dest[1];
  Cr = dest[2];

  ipsize2 = ipsize * 2;
  hs3 = (hsize * ipsize) - (ipsize2 - 1);

  for (i = 0; i < vsize; i++) {
    for (j = 0; j < hs3; j += ipsize2) {
      // mpeg style, Cb and Cr are co-located
      // convert 6 RGBRGB bytes to 4 UYVY bytes

      // TODO: for mpeg use rgb2yuv and write alternate u and v

      rgb2uyvy(rgbdata[j + 1], rgbdata[j + 2], rgbdata[j + 3], rgbdata[j + 1 + ipsize], rgbdata[j + 2 + ipsize + 1],
               rgbdata[j + 3 + ipsize + 2], &u);

      *(y++) = u.y0;
      *(y++) = u.y1;
      *(Cb++) = u.u0;
      *(Cr++) = u.v0;

      if (!is_422 && chroma_row && i > 0) {
        // average two rows
        Cb[-1 - ostrides[1]] = avg_chromaf(Cb[-1], Cb[-1 - ostrides[1]]);
        Cr[-1 - ostrides[2]] = avg_chromaf(Cr[-1], Cr[-1 - ostrides[2]]);
      }

    }
    if (!is_422) {
      if (chroma_row) {
        Cb -= ostrides[1];
        Cr -= ostrides[2];
      }
      chroma_row = !chroma_row;
    }
    rgbdata += rowstride;
  }
}


static void convert_bgr_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride, int *ostrides,
                                        uint8_t **dest, boolean is_422, boolean has_alpha, int subspace, int clamping) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  int hs3;

  uint8_t *y, *Cb, *Cr;
  uyvy_macropixel u;
  register int i, j;
  int chroma_row = TRUE;
  int ipsize = 3, ipsize2;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, subspace);

  if (has_alpha) ipsize = 4;

  // ensure width and height are both divisible by two
  hsize = (hsize >> 1) << 1;
  vsize = (vsize >> 1) << 1;

  y = dest[0];
  Cb = dest[1];
  Cr = dest[2];

  ipsize2 = ipsize * 2;
  hs3 = (hsize * ipsize) - (ipsize2 - 1);
  for (i = 0; i < vsize; i++) {
    for (j = 0; j < hs3; j += ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2uyvy(rgbdata[j + 2], rgbdata[j + 1], rgbdata[j], rgbdata[j + ipsize + 2],
               rgbdata[j + ipsize + 1], rgbdata[j + ipsize], &u);

      *(y++) = u.y0;
      *(y++) = u.y1;
      *(Cb++) = u.u0;
      *(Cr++) = u.v0;

      if (!is_422 && chroma_row && i > 0) {
        // average two rows
        Cb[-1 - ostrides[1]] = avg_chromaf(Cb[-1], Cb[-1 - ostrides[1]]);
        Cr[-1 - ostrides[2]] = avg_chromaf(Cr[-1], Cr[-1 - ostrides[2]]);
      }
    }
    if (!is_422) {
      if (chroma_row) {
        Cb -= ostrides[1];
        Cr -= ostrides[1];
      }
      chroma_row = !chroma_row;
    }
    rgbdata += rowstride;
  }
}


static void convert_yuv422p_to_uyvy_frame(uint8_t **src, int width, int height, int *irows, int orow, uint8_t *dest) {
  // TODO - handle different in sampling types
  uint8_t *src_y = src[0];
  uint8_t *src_u = src[1];
  uint8_t *src_v = src[2];
  int i, j;

  irows[0] -= width;
  irows[1] -= width >> 1;
  irows[2] -= width >> 1;
  orow -= width * 4;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      *(dest++) = *(src_u++);
      *(dest++) = *(src_y++);
      *(dest++) = *(src_v++);
      *(dest++) = *(src_y++);
    }
    src_y += irows[0];
    src_u += irows[1];
    src_v += irows[2];
    dest += orow;
  }
}


static void convert_yuv422p_to_yuyv_frame(uint8_t **src, int width, int height, int *irows, int orow, uint8_t *dest) {
  // TODO - handle different in sampling types

  uint8_t *src_y = src[0];
  uint8_t *src_u = src[1];
  uint8_t *src_v = src[2];
  int i, j;

  irows[0] -= width;
  irows[1] -= width >> 1;
  irows[2] -= width >> 1;
  orow -= width * 4;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      *(dest++) = *(src_y++);
      *(dest++) = *(src_u++);
      *(dest++) = *(src_y++);
      *(dest++) = *(src_v++);
    }
    src_y += irows[0];
    src_u += irows[1];
    src_v += irows[2];
    dest += orow;
  }
}


static void convert_rgb_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        yuv411_macropixel *u, boolean has_alpha, int clamping) {
  // for odd sized widths, cut the rightmost one, two or three pixels. Widths should be divisible by 4.
  // TODO - handle different out sampling types
  int hs3 = (int)(hsize >> 2) * 12, ipstep = 12;

  uint8_t *end;
  register int i;

  int x = 3, y = 4, z = 5, a = 6, b = 7, c = 8, d = 9, e = 10, f = 11;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    a += 2;
    b += 2;
    c += 2;
    d += 3;
    e += 3;
    f += 3;
    hs3 = (int)(hsize >> 2) * 16;
    ipstep = 16;
  }
  end = rgbdata + (rowstride * vsize) + 1 - ipstep;
  hs3 -= (ipstep - 1);

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i], rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + x], rgbdata[i + y], rgbdata[i + z], rgbdata[i + a],
               rgbdata[i + b],
               rgbdata[i + c], rgbdata[i + d],
               rgbdata[i + e], rgbdata[i + f], u++);
    }
  }
}


static void convert_bgr_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        yuv411_macropixel *u, boolean has_alpha, int clamping) {
  // for odd sized widths, cut the rightmost one, two or three pixels
  // TODO - handle different out sampling types
  int hs3 = (int)(hsize >> 2) * 12, ipstep = 12;

  uint8_t *end;
  register int i;

  int x = 3, y = 4, z = 5, a = 6, b = 7, c = 8, d = 9, e = 10, f = 11;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    a += 2;
    b += 2;
    c += 2;
    d += 3;
    e += 3;
    f += 3;
    hs3 = (int)(hsize >> 2) * 16;
    ipstep = 16;
  }
  end = rgbdata + (rowstride * vsize) + 1 - ipstep;
  hs3 -= (ipstep - 1);

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i + 2], rgbdata[i + 1], rgbdata[i], rgbdata[i + z], rgbdata[i + y], rgbdata[i + x], rgbdata[i + c],
               rgbdata[i + b],
               rgbdata[i + a], rgbdata[i + f],
               rgbdata[i + e], rgbdata[i + d], u++);
    }
  }
}


static void convert_argb_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
    yuv411_macropixel *u, int clamping) {
  // for odd sized widths, cut the rightmost one, two or three pixels. Widths should be divisible by 4.
  // TODO - handle different out sampling types
  int hs3 = (int)(hsize >> 2) * 12, ipstep = 12;

  uint8_t *end;
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  hs3 = (int)(hsize >> 2) * 16;
  ipstep = 16;

  end = rgbdata + (rowstride * vsize) + 1 - ipstep;
  hs3 -= (ipstep - 1);

  for (; rgbdata < end; rgbdata += rowstride) {
    for (i = 0; i < hs3; i += ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i + 1], rgbdata[i + 2], rgbdata[i + 3], rgbdata[i + 5], rgbdata[i + 6], rgbdata[i + 7], rgbdata[i + 9],
               rgbdata[i + 10],
               rgbdata[i + 11],
               rgbdata[i + 13], rgbdata[i + 14], rgbdata[i + 15], u++);
    }
  }
}


static void convert_uyvy_to_rgb_frame(uyvy_macropixel *src, int width, int height, int irow, int orowstride,
                                      uint8_t *dest, boolean add_alpha, int clamping, int  subspace, int thread_id) {
  register int i, j;
  int psize = 6;
  int a = 3, b = 4, c = 5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, subspace);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = add_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].in_subspace = subspace;
        ccparams[i].thread_id = i;

        if (i == 0) convert_uyvy_to_rgb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_uyvy_to_rgb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (add_alpha) {
    psize = 8;
    a = 4;
    b = 5;
    c = 6;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      uyvy2rgb(src, &dest[0], &dest[1], &dest[2], &dest[a], &dest[b], &dest[c]);
      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_uyvy_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_uyvy_to_rgb_frame((uyvy_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                            ccparams->irowstrides[0], ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest, ccparams->out_alpha, ccparams->in_clamping,
                            ccparams->in_subspace, ccparams->thread_id);
  return NULL;
}


static void convert_uyvy_to_bgr_frame(uyvy_macropixel *src, int width, int height, int irow, int orowstride,
                                      uint8_t *dest, boolean add_alpha, int clamping, int thread_id) {
  register int i, j;
  int psize = 6;

  int a = 3, b = 4, c = 5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = add_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_uyvy_to_bgr_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_uyvy_to_bgr_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (add_alpha) {
    psize = 8;
    a = 4;
    b = 5;
    c = 6;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      uyvy2rgb(src, &dest[2], &dest[1], &dest[0], &dest[c], &dest[b], &dest[a]);
      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_uyvy_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_uyvy_to_bgr_frame((uyvy_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                            ccparams->irowstrides[0], ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest, ccparams->out_alpha, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_uyvy_to_argb_frame(uyvy_macropixel *src, int width, int height, int irow, int orowstride,
                                       uint8_t *dest, int clamping, int thread_id) {
  register int i, j;
  int psize = 8;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_uyvy_to_argb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_uyvy_to_argb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      uyvy2rgb(src, &dest[1], &dest[2], &dest[3], &dest[5], &dest[6], &dest[7]);
      dest[0] = dest[4] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_uyvy_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_uyvy_to_argb_frame((uyvy_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                             ccparams->irowstrides[0], ccparams->orowstrides[0],
                             (uint8_t *)ccparams->dest, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_rgb_frame(yuyv_macropixel *src, int width, int height, int irow, int orowstride,
                                      uint8_t *dest, boolean add_alpha, int clamping, int thread_id) {
  register int i, j;
  int psize = 6;
  int a = 3, b = 4, c = 5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;
        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = add_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuyv_to_rgb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuyv_to_rgb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (add_alpha) {
    psize = 8;
    a = 4;
    b = 5;
    c = 6;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuyv2rgb(src, &dest[0], &dest[1], &dest[2], &dest[a], &dest[b], &dest[c]);
      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_yuyv_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuyv_to_rgb_frame((yuyv_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                            ccparams->irowstrides[0], ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest, ccparams->out_alpha, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_bgr_frame(yuyv_macropixel *src, int width, int height, int irow, int orowstride,
                                      uint8_t *dest, boolean add_alpha, int clamping, int thread_id) {
  register int i, j;
  int psize = 6;
  int a = 3, b = 4, c = 5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].out_alpha = add_alpha;
        ccparams[i].in_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuyv_to_bgr_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuyv_to_bgr_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (add_alpha) {
    psize = 8;
    a = 4;
    b = 5;
    c = 6;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuyv2rgb(src, &dest[2], &dest[1], &dest[0], &dest[c], &dest[b], &dest[a]);
      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_yuyv_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuyv_to_bgr_frame((yuyv_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                            ccparams->irowstrides[0], ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest, ccparams->out_alpha, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_argb_frame(yuyv_macropixel *src, int width, int height, int irow, int orowstride,
                                       uint8_t *dest, int clamping, int thread_id) {
  register int i, j;
  int psize = 8;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((dheight * i) < height) {
        ccparams[i].src = src + dheight * i * irow / 4;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuyv_to_argb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuyv_to_argb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= width * psize;
  irow = irow / 4 - width;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuyv2rgb(src, &dest[1], &dest[2], &dest[3], &dest[5], &dest[6], &dest[7]);
      dest[0] = dest[4] = 255;
      dest += psize;
      src++;
    }
    src += irow;
    dest += orowstride;
  }
}


static void *convert_yuyv_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuyv_to_argb_frame((yuyv_macropixel *)ccparams->src, ccparams->hsize, ccparams->vsize,
                             ccparams->irowstrides[0], ccparams->orowstrides[0],
                             (uint8_t *)ccparams->dest, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuv420_to_uyvy_frame(uint8_t **src, int width, int height, int *irows, int orow,
    uyvy_macropixel *dest, int clamping) {
  register int i = 0, j;
  uint8_t *y, *u, *v, *end;
  int hwidth = width >> 1;
  boolean chroma = TRUE;

  // TODO - hasndle different in sampling types
  if (!avg_inited) init_average();

  y = src[0];
  u = src[1];
  v = src[2];

  end = y + height * irows[0];
  orow = orow / 4 - hwidth;
  irows[0] -= width;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (y < end) {
    for (j = 0; j < hwidth; j++) {
      dest->u0 = u[0];
      dest->y0 = y[0];
      dest->v0 = v[0];
      dest->y1 = y[1];

      if (chroma && i > 0) {
        dest[-hwidth].u0 = avg_chromaf(dest[-hwidth].u0, u[0]);
        dest[-hwidth].v0 = avg_chromaf(dest[-hwidth].v0, v[0]);
      }

      dest++;
      y += 2;
      u++;
      v++;
    }
    if (chroma) {
      u -= irows[1];
      v -= irows[2];
    }
    chroma = !chroma;
    y += irows[0];
    dest += orow;
  }
}


static void convert_yuv420_to_yuyv_frame(uint8_t **src, int width, int height, int *irows, int orow, yuyv_macropixel *dest,
    int clamping) {
  register int i = 0, j;
  uint8_t *y, *u, *v, *end;
  int hwidth = width >> 1;
  boolean chroma = TRUE;

  // TODO - handle different in sampling types
  if (!avg_inited) init_average();

  y = src[0];
  u = src[1];
  v = src[2];

  end = y + height * irows[0];
  orow = orow / 4 - hwidth;
  irows[0] -= width;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (y < end) {
    for (j = 0; j < hwidth; j++) {
      dest->y0 = y[0];
      dest->u0 = u[0];
      dest->y1 = y[1];
      dest->v0 = v[0];

      if (chroma && i > 0) {
        dest[-hwidth].u0 = avg_chromaf(dest[-hwidth].u0, u[0]);
        dest[-hwidth].v0 = avg_chromaf(dest[-hwidth].v0, v[0]);
      }

      dest++;
      y += 2;
      u++;
      v++;
    }
    if (chroma) {
      u -= irows[1];
      v -= irows[2];
    }
    chroma = !chroma;
    dest += orow;
  }
}


static void convert_yuv_planar_to_rgb_frame(uint8_t **src, int width, int height, int irowstride, int orowstride, uint8_t *dest,
    boolean in_alpha, boolean out_alpha, int clamping, int thread_id) {
  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];
  uint8_t *a = NULL;

  uint8_t *end = y + irowstride * height;

  size_t opstep = 3;
  register int i, j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (in_alpha) a = src[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((y + dheight * i * irowstride) < end) {
        ccparams[i].hsize = width;

        ccparams[i].srcp[0] = y + dheight * i * irowstride;
        ccparams[i].srcp[1] = u + dheight * i * irowstride;
        ccparams[i].srcp[2] = v + dheight * i * irowstride;
        if (in_alpha) ccparams[i].srcp[3] = a + dheight * i * irowstride;

        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = in_alpha;
        ccparams[i].out_alpha = out_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuv_planar_to_rgb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv_planar_to_rgb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (out_alpha) opstep = 4;

  orowstride -= width * opstep;
  irowstride -= width;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuv2rgb(*(y++), *(u++), *(v++), &dest[0], &dest[1], &dest[2]);
      if (out_alpha) {
        if (in_alpha) {
          dest[3] = *(a++);
        } else dest[3] = 255;
      }
      dest += opstep;
    }
    dest += orowstride;
    y += irowstride;
    u += irowstride;
    v += irowstride;
    if (a) a += irowstride;
  }
}


static void *convert_yuv_planar_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv_planar_to_rgb_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                  ccparams->orowstrides[0],
                                  (uint8_t *)ccparams->dest, ccparams->in_alpha, ccparams->out_alpha,
                                  ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuv_planar_to_bgr_frame(uint8_t **src, int width, int height, int irowstride, int orowstride, uint8_t *dest,
    boolean in_alpha, boolean out_alpha, int clamping, int thread_id) {
  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];
  uint8_t *a = NULL;

  uint8_t *end = y + irowstride * height;

  size_t opstep = 4;
  int i, j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();
  if (thread_id == -1)
    set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (in_alpha) a = src[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);

    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((y + dheight * i * irowstride) < end) {
        ccparams[i].hsize = width;

        ccparams[i].srcp[0] = y + dheight * i * irowstride;
        ccparams[i].srcp[1] = u + dheight * i * irowstride;
        ccparams[i].srcp[2] = v + dheight * i * irowstride;
        if (in_alpha) ccparams[i].srcp[3] = a + dheight * i * irowstride;

        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = in_alpha;
        ccparams[i].out_alpha = out_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuv_planar_to_bgr_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv_planar_to_bgr_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= width * opstep;
  irowstride -= width;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuv2bgr(*(y++), *(u++), *(v++), &dest[0], &dest[1], &dest[2]);
      if (out_alpha) {
        if (in_alpha) {
          dest[3] = *(a++);
        } else dest[3] = 255;
      }
      dest += opstep;
    }
    dest += orowstride;
    y += irowstride;
    u += irowstride;
    v += irowstride;
    if (a) a += irowstride;
  }
}


static void *convert_yuv_planar_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv_planar_to_bgr_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                  ccparams->orowstrides[0],
                                  (uint8_t *)ccparams->dest, ccparams->in_alpha, ccparams->out_alpha,
                                  ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuv_planar_to_argb_frame(uint8_t **src, int width, int height, int irowstride, int orowstride,
    uint8_t *dest,
    boolean in_alpha, int clamping, int thread_id) {
  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];
  uint8_t *a = NULL;

  uint8_t *end = y + irowstride * height;

  size_t opstep = 4;
  register int i, j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (in_alpha) a = src[3];

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((y + dheight * i * irowstride) < end) {
        ccparams[i].hsize = width;

        ccparams[i].srcp[0] = y + dheight * i * irowstride;
        ccparams[i].srcp[1] = u + dheight * i * irowstride;
        ccparams[i].srcp[2] = v + dheight * i * irowstride;
        if (in_alpha) ccparams[i].srcp[3] = a + dheight * i * irowstride;

        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].in_alpha = in_alpha;
        ccparams[i].out_clamping = clamping;
        ccparams[i].thread_id = i;

        if (i == 0) convert_yuv_planar_to_argb_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_yuv_planar_to_argb_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  orowstride -= width * opstep;
  orowstride -= width;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      yuv2rgb(*(y++), *(u++), *(v++), &dest[1], &dest[2], &dest[3]);
      if (in_alpha) {
        dest[0] = *(a++);
      } else dest[0] = 255;
      dest += opstep;
    }
    dest += orowstride;
    y += irowstride;
    u += irowstride;
    v += irowstride;
    if (a) a += irowstride;
  }
}


static void *convert_yuv_planar_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_yuv_planar_to_argb_frame((uint8_t **)ccparams->srcp, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                                   ccparams->orowstrides[0],
                                   (uint8_t *)ccparams->dest, ccparams->in_alpha, ccparams->in_clamping, ccparams->thread_id);
  return NULL;
}


static void convert_yuv_planar_to_uyvy_frame(uint8_t **src, int width, int height, int irowstride, int orowstride,
    uyvy_macropixel *uyvy, int clamping) {
  register int x, k;
  int size = (width * height) >> 1;

  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (irowstride == width && orowstride == width << 1) {
    for (x = 0; x < size; x++) {
      // subsample two u pixels
      uyvy->u0 = avg_chromaf(u[0], u[1]);
      u += 2;
      uyvy->y0 = *(y++);
      // subsample 2 v pixels
      uyvy->v0 = avg_chromaf(v[0], v[1]);
      v += 2;
      uyvy->y1 = *(y++);
      uyvy++;
    }
    return;
  }
  irowstride -= width;
  orowstride -= width << 1;
  for (k = 0; k < height; k++) {
    for (x = 0; x < width; x++) {
      // subsample two u pixels
      uyvy->u0 = avg_chromaf(u[0], u[1]);
      u += 2;
      uyvy->y0 = *(y++);
      // subsample 2 v pixels
      uyvy->v0 = avg_chromaf(v[0], v[1]);
      v += 2;
      uyvy->y1 = *(y++);
      uyvy++;
    }
    y += irowstride;
    u += irowstride;
    v += irowstride;
    uyvy += orowstride;
  }
}


static void convert_yuv_planar_to_yuyv_frame(uint8_t **src, int width, int height, int irowstride, int orowstride,
    yuyv_macropixel *yuyv, int clamping) {
  register int x, k;
  int hsize = (width * height) >> 1;

  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (irowstride == width && orowstride == (width << 1)) {
    for (x = 0; x < hsize; x++) {
      yuyv->y0 = *(y++);
      yuyv->u0 = avg_chromaf(u[0], u[1]);
      u += 2;
      yuyv->y1 = *(y++);
      yuyv->v0 = avg_chromaf(v[0], v[1]);
      v += 2;
      yuyv++;
    }
    return;
  }

  irowstride -= width;
  orowstride = orowstride / 4 - width;
  for (k = 0; k < height; k++) {
    for (x = 0; x < width; x++) {
      yuyv->y0 = *(y++);
      yuyv->u0 = avg_chromaf(u[0], u[1]);
      u += 2;
      yuyv->y1 = *(y++);
      yuyv->v0 = avg_chromaf(v[0], v[1]);
      v += 2;
      yuyv++;
    }
    y += irowstride;
    u += irowstride;
    v += irowstride;
    yuyv += orowstride;
  }
}


static void convert_combineplanes_frame(uint8_t **src, int width, int height, int irowstride,
                                        int orowstride, uint8_t *dest, boolean in_alpha, boolean out_alpha) {
  // turn 3 or 4 planes into packed pixels, src and dest can have alpha

  // e.g yuv444(4)p to yuv888(8)

  int size = width * height;

  uint8_t *y = src[0];
  uint8_t *u = src[1];
  uint8_t *v = src[2];
  uint8_t *a = NULL;
  int opsize = 3;
  register int x, k;

  if (in_alpha) a = src[3];
  if (out_alpha) opsize = 4;

  if (irowstride == width && orowstride == width * opsize) {
    for (x = 0; x < size; x++) {
      *(dest++) = *(y++);
      *(dest++) = *(u++);
      *(dest++) = *(v++);
      if (out_alpha) {
        if (in_alpha) *(dest++) = *(a++);
        else *(dest++) = 255;
      }
    }
  } else {
    irowstride -= width;
    orowstride -= width * opsize;
    for (k = 0; k < height; k++) {
      for (x = 0; x < width; x++) {
        *(dest++) = *(y++);
        *(dest++) = *(u++);
        *(dest++) = *(v++);
        if (out_alpha) {
          if (in_alpha) *(dest++) = *(a++);
          else *(dest++) = 255;
        }
      }
      dest += orowstride;
      y += irowstride;
      u += irowstride;
      v += irowstride;
    }
  }
}


static void convert_yuvap_to_yuvp_frame(uint8_t **src, int width, int height, int irowstride, int orowstride, uint8_t **dest) {
  size_t size = irowstride * height;

  uint8_t *ys = src[0];
  uint8_t *us = src[1];
  uint8_t *vs = src[2];

  uint8_t *yd = dest[0];
  uint8_t *ud = dest[1];
  uint8_t *vd = dest[2];

  register int y;

  if (orowstride == irowstride) {
    if (yd != ys) lives_memcpy(yd, ys, size);
    if (ud != us) lives_memcpy(ud, us, size);
    if (vd != vs) lives_memcpy(vd, vs, size);
    return;
  }
  for (y = 0; y < height; y++) {
    if (yd != ys) {
      lives_memcpy(yd, ys, width);
      yd += orowstride;
      ys += irowstride;
    }
    if (ud != us) {
      lives_memcpy(ud, us, width);
      ud += orowstride;
      us += irowstride;
    }
    if (vd != vs) {
      lives_memcpy(vd, vs, width);
      vd += orowstride;
      vs += irowstride;
    }
  }
}


static void convert_yuvp_to_yuvap_frame(uint8_t **src, int width, int height, int irowstride, int orowstride,  uint8_t **dest) {
  convert_yuvap_to_yuvp_frame(src, width, height, irowstride, orowstride, dest);
  lives_memset(dest[3], 255, orowstride * height);
}


static void convert_yuvp_to_yuv420_frame(uint8_t **src, int width, int height, int *irows, int *orows, uint8_t **dest,
    int clamping) {
  // halve the chroma samples vertically and horizontally, with sub-sampling

  // convert 444p to 420p

  // TODO - handle different output sampling types

  // y-plane should be copied before entering here

  int i, j;
  uint8_t *d_u, *d_v, *s_u = src[1], *s_v = src[2];
  short x_u, x_v;
  boolean chroma = FALSE;

  int hwidth = width >> 1;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (dest[0] != src[0]) {
    if (irows[0] == orows[0]) {
      lives_memcpy(dest[0], src[0], irows[0] * height);
    } else {
      uint8_t *d_y = dest[0];
      uint8_t *s_y = src[0];
      for (i = 0; i < height; i++) {
        lives_memcpy(d_y, s_y, width);
        d_y += orows[0];
        s_y += irows[0];
      }
    }
  }

  d_u = dest[1];
  d_v = dest[2];

  for (i = 0; i < height; i++) {
    for (j = 0; j < hwidth; j++) {
      if (!chroma) {
        // pass 1, copy row
        // average two dest pixels
        d_u[j] = avg_chromaf(s_u[j * 2], s_u[j * 2 + 1]);
        d_v[j] = avg_chromaf(s_v[j * 2], s_v[j * 2 + 1]);
      } else {
        // pass 2
        // average two dest pixels
        x_u = avg_chromaf(s_u[j * 2], s_u[j * 2 + 1]);
        x_v = avg_chromaf(s_v[j * 2], s_v[j * 2 + 1]);
        // average two dest rows
        d_u[j] = avg_chromaf(d_u[j], x_u);
        d_v[j] = avg_chromaf(d_v[j], x_v);
      }
    }
    if (chroma) {
      d_u += orows[1];
      d_v += orows[2];
    }
    chroma = !chroma;
    s_u += irows[1];
    s_v += irows[2];
  }
}


static void convert_yuvp_to_yuv411_frame(uint8_t **src, int width, int height, int irowstride,
    yuv411_macropixel *yuv, int clamping) {
  // quarter the chroma samples horizontally, with sub-sampling

  // convert 444p to 411 packed
  // TODO - handle different output sampling types

  register int i, j;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  register short x_u, x_v;

  int widtha = (width >> 1) << 1; // cut rightmost odd bytes
  int cbytes = width - widtha;

  irowstride -= width + cbytes;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  for (i = 0; i < height; i++) {
    for (j = 0; j < widtha; j += 4) {
      // average four dest pixels
      yuv->u2 = avg_chromaf(s_u[0], s_u[1]);
      x_u = avg_chromaf(s_u[2], s_u[3]);
      yuv->u2 = avg_chromaf(yuv->u2, x_u);

      s_u += 4;

      yuv->y0 = *(s_y++);
      yuv->y1 = *(s_y++);

      yuv->v2 = avg_chromaf(s_v[0], s_v[1]);
      x_v = avg_chromaf(s_v[2], s_v[3]);
      yuv->v2 = avg_chromaf(yuv->v2, x_v);

      s_v += 4;

      yuv->y2 = *(s_y++);
      yuv->y3 = *(s_y++);
    }
    s_u += irowstride;
    s_v += irowstride;
  }
}


static void convert_uyvy_to_yuvp_frame(uyvy_macropixel *uyvy, int width, int height, int irow, int *orow, uint8_t **dest,
                                       boolean add_alpha) {
  // TODO - avg_chroma

  uint8_t *y = dest[0];
  uint8_t *u = dest[1];
  uint8_t *v = dest[2];

  irow /= 4;

  for (int k = 0; k < height; k++) {
    for (int x = 0, x1 = 0; x < width; x++, x1 += 2) {
      u[k * orow[0] + x1] = u[k * orow[0] + x1 + 1] = uyvy[k * irow + x].u0;
      y[k * orow[1] + x1] = uyvy[k * irow + x].y0;
      v[k * orow[2] + x1] = v[k * orow[2] + x1 + 1] = uyvy[k * irow + x].v0;
      y[k * orow[0] + x1 + 1] = uyvy[k * irow + x].y1;
    }
  }
  if (add_alpha) lives_memset(dest[3], 255, orow[3] * height);
}


static void convert_yuyv_to_yuvp_frame(yuyv_macropixel *yuyv, int width, int height, int irow, int *orow,
                                       uint8_t **dest, boolean add_alpha) {
  // TODO - avg_chroma

  uint8_t *y = dest[0];
  uint8_t *u = dest[1];
  uint8_t *v = dest[2];

  irow /= 4;

  for (int k = 0; k < height; k++) {
    for (int x = 0, x1 = 0; x < width; x++, x1 += 2) {
      y[k * orow[1] + x1] = yuyv[k * irow + x].y0;
      u[k * orow[0] + x1] = u[k * orow[0] + x1 + 1] = yuyv[k * irow + x].u0;
      y[k * orow[0] + x1 + 1] = yuyv[k * irow + x].y1;
      v[k * orow[2] + x1] = v[k * orow[2] + x1 + 1] = yuyv[k * irow + x].v0;
    }
  }
  if (add_alpha) lives_memset(dest[3], 255, orow[3] * height);
}


static void convert_uyvy_to_yuv888_frame(uyvy_macropixel *uyvy, int width, int height, int irow, int orow,
    uint8_t *yuv, boolean add_alpha) {
  // no subsampling : TODO

  irow /= 4;

  for (int y = 0; y < height; y++) {
    for (int x = 0, x1 = 0; x < width; x++) {
      yuv[y * orow + x1++] = uyvy[y * irow + x].y0;
      yuv[y * orow + x1++] = uyvy[y * irow + x].u0;
      yuv[y * orow + x1++] = uyvy[y * irow + x].v0;
      if (add_alpha) yuv[y * orow + x1++] = 255;
      yuv[y * orow + x1++] = uyvy[y * irow + x].y1;
      yuv[y * orow + x1++] = uyvy[y * irow + x].u0;
      yuv[y * orow + x1++] = uyvy[y * irow + x].v0;
      if (add_alpha) yuv[y * orow + x1++] = 255;
    }
  }
}


static void convert_yuyv_to_yuv888_frame(yuyv_macropixel *yuyv, int width, int height, int irow, int orow,
    uint8_t *yuv, boolean add_alpha) {
  // no subsampling : TODO

  irow /= 4;

  for (int y = 0; y < height; y++) {
    for (int x = 0, x1 = 0; x < width; x++) {
      yuv[y * orow + x1++] = yuyv[y * irow + x].y0;
      yuv[y * orow + x1++] = yuyv[y * irow + x].u0;
      yuv[y * orow + x1++] = yuyv[y * irow + x].v0;
      if (add_alpha) yuv[y * orow + x1++] = 255;
      yuv[y * orow + x1++] = yuyv[y * irow + x].y1;
      yuv[y * orow + x1++] = yuyv[y * irow + x].u0;
      yuv[y * orow + x1++] = yuyv[y * irow + x].v0;
      if (add_alpha) yuv[y * orow + x1++] = 255;
    }
  }
}


static void convert_uyvy_to_yuv420_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t **yuv, int clamping) {
  // subsample vertically

  // TODO - handle different sampling types

  register int j;

  uint8_t *y = yuv[0];
  uint8_t *u = yuv[1];
  uint8_t *v = yuv[2];

  boolean chroma = TRUE;

  uint8_t *end = y + width * height * 2;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (y < end) {
    for (j = 0; j < width; j++) {
      if (chroma) *(u++) = uyvy->u0;
      else {
        *u = avg_chromaf(*u, uyvy->u0);
        u++;
      }
      *(y++) = uyvy->y0;
      if (chroma) *(v++) = uyvy->v0;
      else {
        *v = avg_chromaf(*v, uyvy->v0);
        v++;
      }
      *(y++) = uyvy->y1;
      uyvy++;
    }
    if (chroma) {
      u -= width;
      v -= width;
    }
    chroma = !chroma;
  }
}


static void convert_yuyv_to_yuv420_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t **yuv, int clamping) {
  // subsample vertically

  // TODO - handle different sampling types

  register int j;

  uint8_t *y = yuv[0];
  uint8_t *u = yuv[1];
  uint8_t *v = yuv[2];

  boolean chroma = TRUE;

  uint8_t *end = y + width * height * 2;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (y < end) {
    for (j = 0; j < width; j++) {
      *(y++) = yuyv->y0;
      if (chroma) *(u++) = yuyv->u0;
      else {
        *u = avg_chromaf(*u, yuyv->u0);
        u++;
      }
      *(y++) = yuyv->y1;
      if (chroma) *(v++) = yuyv->v0;
      else {
        *v = avg_chromaf(*v, yuyv->v0);
        v++;
      }
      yuyv++;
    }
    if (chroma) {
      u -= width;
      v -= width;
    }
    chroma = !chroma;
  }
}


static void convert_uyvy_to_yuv411_frame(uyvy_macropixel *uyvy, int width, int height, yuv411_macropixel *yuv, int clamping) {
  // subsample chroma horizontally

  uyvy_macropixel *end = uyvy + width * height;
  register int x;

  int widtha = (width << 1) >> 1;
  size_t cbytes = width - widtha;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  for (; uyvy < end; uyvy += cbytes) {
    for (x = 0; x < widtha; x += 2) {
      yuv->u2 = avg_chromaf(uyvy[0].u0, uyvy[1].u0);

      yuv->y0 = uyvy[0].y0;
      yuv->y1 = uyvy[0].y1;

      yuv->v2 = avg_chromaf(uyvy[0].v0, uyvy[1].v0);

      yuv->y2 = uyvy[1].y0;
      yuv->y3 = uyvy[1].y1;

      uyvy += 2;
      yuv++;
    }
  }
}


static void convert_yuyv_to_yuv411_frame(yuyv_macropixel *yuyv, int width, int height, yuv411_macropixel *yuv, int clamping) {
  // subsample chroma horizontally

  // TODO - handle different sampling types

  yuyv_macropixel *end = yuyv + width * height;
  register int x;

  int widtha = (width << 1) >> 1;
  size_t cybtes = width - widtha;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  for (; yuyv < end; yuyv += cybtes) {
    for (x = 0; x < widtha; x += 2) {
      yuv->u2 = avg_chromaf(yuyv[0].u0, yuyv[1].u0);

      yuv->y0 = yuyv[0].y0;
      yuv->y1 = yuyv[0].y1;

      yuv->v2 = avg_chromaf(yuyv[0].v0, yuyv[1].v0);

      yuv->y2 = yuyv[1].y0;
      yuv->y3 = yuyv[1].y1;

      yuyv += 2;
      yuv++;
    }
  }
}


static void convert_yuv888_to_yuv420_frame(uint8_t *yuv8, int width, int height, int irowstride, int *orows,
    uint8_t **yuv4, boolean src_alpha, int clamping) {
  // subsample vertically and horizontally

  //

  // yuv888(8) packed to 420p

  // TODO - handle different sampling types

  int j;
  short x_u, x_v;

  uint8_t *d_y, *d_u, *d_v, *end;

  boolean chroma = TRUE;

  size_t ipsize = 3, ipsize2;
  int widthx;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (src_alpha) ipsize = 4;

  d_y = yuv4[0];
  d_u = yuv4[1];
  d_v = yuv4[2];

  end = d_y + width * height;
  ipsize2 = ipsize * 2;
  widthx = width * ipsize;

  while (d_y < end) {
    for (j = 0; j < widthx; j += ipsize2) {
      *(d_y++) = yuv8[j];
      *(d_y++) = yuv8[j + ipsize];
      if (chroma) {
        *(d_u++) = avg_chromaf(yuv8[j + 1], yuv8[j + 1 + ipsize]);
        *(d_v++) = avg_chromaf(yuv8[j + 2], yuv8[j + 2 + ipsize]);
      } else {
        x_u = avg_chromaf(yuv8[j + 1], yuv8[j + 1 + ipsize]);
        *d_u = avg_chromaf(*d_u, x_u);
        d_u++;
        x_v = avg_chromaf(yuv8[j + 2], yuv8[j + 2 + ipsize]);
        *d_v = avg_chromaf(*d_v, x_v);
        d_v++;
      }
    }
    if (chroma) {
      d_u -= orows[1];
      d_v -= orows[2];
    }
    chroma = !chroma;
    yuv8 += irowstride;
  }
}


static void convert_uyvy_to_yuv422_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t **yuv) {
  int size = width * height; // y is twice this, u and v are equal

  uint8_t *y = yuv[0];
  uint8_t *u = yuv[1];
  uint8_t *v = yuv[2];

  register int x;

  for (x = 0; x < size; x++) {
    uyvy_2_yuv422(uyvy, y, u, v, y + 1);
    y += 2;
    u++;
    v++;
  }
}


static void convert_yuyv_to_yuv422_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t **yuv) {
  int size = width * height; // y is twice this, u and v are equal

  uint8_t *y = yuv[0];
  uint8_t *u = yuv[1];
  uint8_t *v = yuv[2];

  register int x;

  for (x = 0; x < size; x++) {
    yuyv_2_yuv422(yuyv, y, u, v, y + 1);
    y += 2;
    u++;
    v++;
  }
}


static void convert_yuv888_to_yuv422_frame(uint8_t *yuv8, int width, int height, int irowstride, int *ostrides,
    uint8_t **yuv4, boolean has_alpha, int clamping) {
  // 888(8) packed to 422p

  // TODO - handle different sampling types

  int size = width * height; // y is equal this, u and v are half, chroma subsampled horizontally

  uint8_t *y = yuv4[0];
  uint8_t *u = yuv4[1];
  uint8_t *v = yuv4[2];

  register int x, i, j;

  int offs = 0;
  size_t ipsize;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs = 1;

  ipsize = (3 + offs) << 1;

  if ((irowstride << 1) == width * ipsize && ostrides[0] == width && ostrides[1] == (width >> 1)) {
    for (x = 0; x < size; x += 2) {
      *(y++) = yuv8[0];
      *(y++) = yuv8[3 + offs];
      *(u++) = avg_chromaf(yuv8[1], yuv8[4 + offs]);
      *(v++) = avg_chromaf(yuv8[2], yuv8[5 + offs]);
      yuv8 += ipsize;
    }
  } else {
    width >>= 1;
    irowstride -= width * ipsize;
    ostrides[0] -= width;
    ostrides[1] -= width >> 1;
    ostrides[2] -= width >> 1;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        *(y++) = yuv8[0];
        *(y++) = yuv8[3 + offs];
        *(u++) = avg_chromaf(yuv8[1], yuv8[4 + offs]);
        *(v++) = avg_chromaf(yuv8[2], yuv8[5 + offs]);
        yuv8 += ipsize;
      }
      yuv8 += irowstride;
      y += ostrides[0];
      u += ostrides[1];
      v += ostrides[2];
    }
  }
}


static void convert_yuv888_to_uyvy_frame(uint8_t *yuv, int width, int height, int irowstride, int orow,
    uyvy_macropixel *uyvy, boolean has_alpha, int clamping) {
  int size = width * height;

  register int x, i, j;

  int offs = 0;
  size_t ipsize;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs = 1;

  ipsize = (3 + offs) << 1;

  if ((irowstride << 1) == width * ipsize && (width << 1) == orow) {
    for (x = 0; x < size; x += 2) {
      uyvy->u0 = avg_chromaf(yuv[1], yuv[4 + offs]);
      uyvy->y0 = yuv[0];
      uyvy->v0 = avg_chromaf(yuv[2], yuv[5 + offs]);
      uyvy->y1 = yuv[3 + offs];
      yuv += ipsize;
      uyvy++;
    }
  } else {
    orow -= width << 1;
    width >>= 1;
    irowstride -= width * ipsize;
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        uyvy->u0 = avg_chromaf(yuv[1], yuv[4 + offs]);
        uyvy->y0 = yuv[0];
        uyvy->v0 = avg_chromaf(yuv[2], yuv[5 + offs]);
        uyvy->y1 = yuv[3 + offs];
        yuv += ipsize;
        uyvy++;
      }
      uyvy += orow;
      yuv += irowstride;
    }
  }
}


static void convert_yuv888_to_yuyv_frame(uint8_t *yuv, int width, int height, int irowstride, int orow,
    yuyv_macropixel *yuyv, boolean has_alpha, int clamping) {
  int size = width * height;

  register int x, i, j;

  int offs = 0;
  size_t ipsize;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs = 1;

  ipsize = (3 + offs) << 1;

  if (irowstride << 1 == width * ipsize && (width << 1) == orow) {
    for (x = 0; x < size; x += 2) {
      yuyv->y0 = yuv[0];
      yuyv->u0 = avg_chromaf(yuv[1], yuv[4 + offs]);
      yuyv->y1 = yuv[3 + offs];
      yuyv->v0 = avg_chromaf(yuv[2], yuv[5 + offs]);
      yuv += ipsize;
      yuyv++;
    }
  } else {
    orow -= width << 1;
    width >>= 1;
    irowstride -= width * ipsize;
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        yuyv->y0 = yuv[0];
        yuyv->u0 = avg_chromaf(yuv[1], yuv[4 + offs]);
        yuyv->y1 = yuv[3 + offs];
        yuyv->v0 = avg_chromaf(yuv[2], yuv[5 + offs]);
        yuv += ipsize;
        yuyv++;
      }
      yuyv += orow;
      yuv += irowstride;
    }
  }
}


static void convert_yuv888_to_yuv411_frame(uint8_t *yuv8, int width, int height, int irowstride,
    yuv411_macropixel *yuv411, boolean has_alpha) {
  // yuv 888(8) packed to yuv411. Chroma pixels are averaged.

  // TODO - handle different sampling types

  uint8_t *end = yuv8 + width * height;
  register int x;
  size_t ipsize = 3;
  int widtha = (width >> 1) << 1; // cut rightmost odd bytes
  int cbytes = width - widtha;

  if (has_alpha) ipsize = 4;

  irowstride -= widtha * ipsize;

  for (; yuv8 < end; yuv8 += cbytes) {
    for (x = 0; x < widtha; x += 4) { // process 4 input pixels for one output macropixel
      yuv411->u2 = (yuv8[1] + yuv8[ipsize + 1] + yuv8[2 * ipsize + 1] + yuv8[3 * ipsize + 1]) >> 2;
      yuv411->y0 = yuv8[0];
      yuv411->y1 = yuv8[ipsize];
      yuv411->v2 = (yuv8[2] + yuv8[ipsize + 2] + yuv8[2 * ipsize + 2] + yuv8[3 * ipsize + 2]) >> 2;
      yuv411->y2 = yuv8[ipsize * 2];
      yuv411->y3 = yuv8[ipsize * 3];

      yuv411++;
      yuv8 += ipsize * 4;
    }
    yuv8 += irowstride;
  }
}


static void convert_yuv411_to_rgb_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
                                        uint8_t *dest, boolean add_alpha, int clamping) {
  uyvy_macropixel uyvy;
  int m = 3, n = 4, o = 5;
  uint8_t u, v, h_u, h_v, q_u, q_v, y0, y1;
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  size_t psize = 3, psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    m = 4;
    n = 5;
    o = 6;
    psize = 4;
  }

  orowstride -= width * 4 * psize;
  psize2 = psize << 1;

  while (yuv411 < end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3] = dest[7] = 255;

    uyvy.y0 = yuv411[0].y0;
    uyvy.y1 = yuv411[0].y1;
    uyvy.u0 = yuv411[0].u2;
    uyvy.v0 = yuv411[0].v2;
    uyvy2rgb(&uyvy, &dest[0], &(dest[1]), &dest[2], &dest[m], &dest[n], &dest[o]);
    dest += psize2;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;
      y1 = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j - 1].u2);
      q_v = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average again to get 1/8, 3/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2rgb(y0, u, v, &dest[0], &dest[1], &dest[2]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2rgb(y1, u, v, &dest[m], &dest[n], &dest[o]);

      dest += psize2;

      // set first 2 RGB pixels of this block

      y0 = yuv411[j].y0;
      y1 = yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j].u2);
      q_v = avg_chromaf(h_v, yuv411[j].v2);

      // average again to get 5/8, 7/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2rgb(y0, u, v, &dest[0], &dest[1], &dest[2]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2rgb(y1, u, v, &dest[m], &dest[n], &dest[o]);

      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize2;

    }
    // write last 2 pixels

    if (add_alpha) dest[3] = dest[7] = 255;

    uyvy.y0 = yuv411[j - 1].y2;
    uyvy.y1 = yuv411[j - 1].y3;
    uyvy.u0 = yuv411[j - 1].u2;
    uyvy.v0 = yuv411[j - 1].v2;
    uyvy2rgb(&uyvy, &dest[0], &(dest[1]), &dest[2], &dest[m], &dest[n], &dest[o]);

    dest += psize2 + orowstride;
    yuv411 += width;
  }
}


static void convert_yuv411_to_bgr_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
                                        uint8_t *dest, boolean add_alpha, int clamping) {
  uyvy_macropixel uyvy;
  int m = 3, n = 4, o = 5;
  uint8_t u, v, h_u, h_v, q_u, q_v, y0, y1;
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  size_t psize = 3, psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    m = 4;
    n = 5;
    o = 6;
    psize = 4;
  }

  orowstride -= width * 4 * psize;

  psize2 = psize << 1;

  while (yuv411 < end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3] = dest[7] = 255;

    uyvy.y0 = yuv411[0].y0;
    uyvy.y1 = yuv411[0].y1;
    uyvy.u0 = yuv411[0].u2;
    uyvy.v0 = yuv411[0].v2;
    uyvy2rgb(&uyvy, &dest[0], &(dest[1]), &dest[2], &dest[o], &dest[n], &dest[m]);
    dest += psize2;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;
      y1 = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j - 1].u2);
      q_v = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average again to get 1/8, 3/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2bgr(y0, u, v, &dest[0], &dest[1], &dest[2]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2bgr(y1, u, v, &dest[m], &dest[n], &dest[o]);

      dest += psize2;

      // set first 2 RGB pixels of this block

      y0 = yuv411[j].y0;
      y1 = yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j].u2);
      q_v = avg_chromaf(h_v, yuv411[j].v2);

      // average again to get 5/8, 7/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2bgr(y0, u, v, &dest[0], &dest[1], &dest[2]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2bgr(y1, u, v, &dest[m], &dest[n], &dest[o]);

      if (add_alpha) dest[3] = dest[7] = 255;
      dest += psize2;

    }
    // write last 2 pixels

    if (add_alpha) dest[3] = dest[7] = 255;

    uyvy.y0 = yuv411[j - 1].y2;
    uyvy.y1 = yuv411[j - 1].y3;
    uyvy.u0 = yuv411[j - 1].u2;
    uyvy.v0 = yuv411[j - 1].v2;
    uyvy2rgb(&uyvy, &dest[0], &(dest[1]), &dest[2], &dest[m], &dest[n], &dest[o]);

    dest += psize2 + orowstride;
    yuv411 += width;
  }
}


static void convert_yuv411_to_argb_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
    uint8_t *dest, int clamping) {
  uyvy_macropixel uyvy;
  uint8_t u, v, h_u, h_v, q_u, q_v, y0, y1;
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  size_t psize = 4, psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  orowstride -= width * 4 * psize;
  psize2 = psize << 1;

  while (yuv411 < end) {
    // write 2 ARGB pixels
    dest[0] = dest[4] = 255;

    uyvy.y0 = yuv411[0].y0;
    uyvy.y1 = yuv411[0].y1;
    uyvy.u0 = yuv411[0].u2;
    uyvy.v0 = yuv411[0].v2;
    uyvy2rgb(&uyvy, &dest[1], &(dest[2]), &dest[3], &dest[5], &dest[6], &dest[7]);
    dest += psize2;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 4 argb pixels

      // average first 2 ARGB pixels of this block and last 2 ARGB pixels of previous block

      y0 = yuv411[j - 1].y2;
      y1 = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j - 1].u2);
      q_v = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average again to get 1/8, 3/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2rgb(y0, u, v, &dest[1], &dest[2], &dest[3]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2rgb(y1, u, v, &dest[5], &dest[6], &dest[7]);

      dest += psize2;

      // set first 2 ARGB pixels of this block

      y0 = yuv411[j].y0;
      y1 = yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j].u2);
      q_v = avg_chromaf(h_v, yuv411[j].v2);

      // average again to get 5/8, 7/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      yuv2rgb(y0, u, v, &dest[1], &dest[2], &dest[3]);

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      yuv2rgb(y1, u, v, &dest[5], &dest[6], &dest[7]);

      dest[0] = dest[4] = 255;
      dest += psize2;

    }
    // write last 2 pixels

    dest[0] = dest[4] = 255;

    uyvy.y0 = yuv411[j - 1].y2;
    uyvy.y1 = yuv411[j - 1].y3;
    uyvy.u0 = yuv411[j - 1].u2;
    uyvy.v0 = yuv411[j - 1].v2;
    uyvy2rgb(&uyvy, &dest[1], &(dest[2]), &dest[3], &dest[5], &dest[6], &dest[7]);

    dest += psize2 + orowstride;
    yuv411 += width;
  }
}


static void convert_yuv411_to_yuv888_frame(yuv411_macropixel *yuv411, int width, int height,
    uint8_t *dest, boolean add_alpha, int clamping) {
  size_t psize = 3;
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t u, v, h_u, h_v, q_u, q_v, y0, y1;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) psize = 4;

  while (yuv411 < end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3] = dest[7] = 255;

    // write first 2 pixels
    dest[0] = yuv411[0].y0;
    dest[1] = yuv411[0].u2;
    dest[2] = yuv411[0].v2;
    dest += psize;

    dest[0] = yuv411[0].y1;
    dest[1] = yuv411[0].u2;
    dest[2] = yuv411[0].v2;
    dest += psize;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;
      y1 = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j - 1].u2);
      q_v = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average again to get 1/8, 3/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      dest[0] = y0;
      dest[1] = u;
      dest[2] = v;
      if (add_alpha) dest[3] = 255;

      dest += psize;

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      dest[0] = y1;
      dest[1] = u;
      dest[2] = v;
      if (add_alpha) dest[3] = 255;

      dest += psize;

      // set first 2 RGB pixels of this block

      y0 = yuv411[j].y0;
      y1 = yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j].u2);
      q_v = avg_chromaf(h_v, yuv411[j].v2);

      // average again to get 5/8, 7/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      dest[0] = y0;
      dest[1] = u;
      dest[2] = v;

      if (add_alpha) dest[3] = 255;
      dest += psize;

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      dest[0] = y1;
      dest[1] = u;
      dest[2] = v;

      if (add_alpha) dest[3] = 255;
      dest += psize;
    }
    // write last 2 pixels

    if (add_alpha) dest[3] = dest[7] = 255;

    dest[0] = yuv411[j - 1].y2;
    dest[1] = yuv411[j - 1].u2;
    dest[2] = yuv411[j - 1].v2;
    dest += psize;

    dest[0] = yuv411[j - 1].y3;
    dest[1] = yuv411[j - 1].u2;
    dest[2] = yuv411[j - 1].v2;

    dest += psize;
    yuv411 += width;
  }
}


static void convert_yuv411_to_yuvp_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest,
    boolean add_alpha, int clamping) {
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t u, v, h_u, h_v, q_u, q_v, y0;

  uint8_t *d_y = dest[0];
  uint8_t *d_u = dest[1];
  uint8_t *d_v = dest[2];
  uint8_t *d_a = dest[3];

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411 < end) {
    // write first 2 pixels
    *(d_y++) = yuv411[0].y0;
    *(d_u++) = yuv411[0].u2;
    *(d_v++) = yuv411[0].v2;
    if (add_alpha) *(d_a++) = 255;

    *(d_y++) = yuv411[0].y0;
    *(d_u++) = yuv411[0].u2;
    *(d_v++) = yuv411[0].v2;
    if (add_alpha) *(d_a++) = 255;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j - 1].u2);
      q_v = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average again to get 1/8, 3/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      *(d_y++) = y0;
      *(d_u++) = u;
      *(d_v++) = v;
      if (add_alpha) *(d_a++) = 255;

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      *(d_y++) = y0;
      *(d_u++) = u;
      *(d_v++) = v;
      if (add_alpha) *(d_a++) = 255;

      // set first 2 RGB pixels of this block

      y0 = yuv411[j].y0;

      // avg to get 3/4, 1/2

      q_u = avg_chromaf(h_u, yuv411[j].u2);
      q_v = avg_chromaf(h_v, yuv411[j].v2);

      // average again to get 5/8, 7/8

      u = avg_chromaf(q_u, yuv411[j - 1].u2);
      v = avg_chromaf(q_v, yuv411[j - 1].v2);

      *(d_y++) = y0;
      *(d_u++) = u;
      *(d_v++) = v;
      if (add_alpha) *(d_a++) = 255;

      u = avg_chromaf(q_u, yuv411[j].u2);
      v = avg_chromaf(q_v, yuv411[j].v2);

      *(d_y++) = y0;
      *(d_u++) = u;
      *(d_v++) = v;
      if (add_alpha) *(d_a++) = 255;
    }
    // write last 2 pixels
    *(d_y++) = yuv411[j - 1].y2;
    *(d_u++) = yuv411[j - 1].u2;
    *(d_v++) = yuv411[j - 1].v2;
    if (add_alpha) *(d_a++) = 255;

    *(d_y++) = yuv411[j - 1].y3;
    *(d_u++) = yuv411[j - 1].u2;
    *(d_v++) = yuv411[j - 1].v2;
    if (add_alpha) *(d_a++) = 255;

    yuv411 += width;
  }
}


static void convert_yuv411_to_uyvy_frame(yuv411_macropixel *yuv411, int width, int height,
    uyvy_macropixel *uyvy, int clamping) {
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t u, v, h_u, h_v, y0;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411 < end) {
    // write first uyvy pixel
    uyvy->u0 = yuv411->u2;
    uyvy->y0 = yuv411->y0;
    uyvy->v0 = yuv411->v2;
    uyvy->y1 = yuv411->y1;

    uyvy++;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 2 uyvy macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u = avg_chromaf(h_u, yuv411[j - 1].u2);
      v = avg_chromaf(h_v, yuv411[j - 1].v2);

      uyvy->u0 = u;
      uyvy->y0 = y0;
      uyvy->v0 = v;
      uyvy->y1 = y0;

      uyvy++;

      // average last pixel again to get 3/4

      u = avg_chromaf(h_u, yuv411[j].u2);
      v = avg_chromaf(h_v, yuv411[j].v2);

      // set first uyvy macropixel of this block

      y0 = yuv411[j].y0;

      uyvy->u0 = u;
      uyvy->y0 = y0;
      uyvy->v0 = v;
      uyvy->y1 = y0;

      uyvy++;
    }
    // write last uyvy macro pixel
    uyvy->u0 = yuv411[j - 1].u2;
    uyvy->y0 = yuv411[j - 1].y2;
    uyvy->v0 = yuv411[j - 1].v2;
    uyvy->y1 = yuv411[j - 1].y3;

    uyvy++;

    yuv411 += width;
  }
}


static void convert_yuv411_to_yuyv_frame(yuv411_macropixel *yuv411, int width, int height, yuyv_macropixel *yuyv,
    int clamping) {
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t u, v, h_u, h_v, y0;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411 < end) {
    // write first yuyv pixel
    yuyv->y0 = yuv411->y0;
    yuyv->u0 = yuv411->u2;
    yuyv->y1 = yuv411->y1;
    yuyv->v0 = yuv411->v2;

    yuyv++;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0 = yuv411[j - 1].y2;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u = avg_chromaf(h_u, yuv411[j - 1].u2);
      v = avg_chromaf(h_v, yuv411[j - 1].v2);

      yuyv->y0 = y0;
      yuyv->u0 = u;
      yuyv->y1 = y0;
      yuyv->v0 = v;

      yuyv++;

      // average last pixel again to get 3/4

      u = avg_chromaf(h_u, yuv411[j].u2);
      v = avg_chromaf(h_v, yuv411[j].v2);

      // set first yuyv macropixel of this block

      y0 = yuv411[j].y0;

      yuyv->y0 = y0;
      yuyv->u0 = u;
      yuyv->y1 = y0;
      yuyv->v0 = v;

      yuyv++;
    }
    // write last yuyv macro pixel
    yuyv->y0 = yuv411[j - 1].y2;
    yuyv->u0 = yuv411[j - 1].u2;
    yuyv->y1 = yuv411[j - 1].y3;
    yuyv->v0 = yuv411[j - 1].v2;

    yuyv++;

    yuv411 += width;
  }
}


static void convert_yuv411_to_yuv422_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest, int clamping) {
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t h_u, h_v;

  uint8_t *d_y = dest[0];
  uint8_t *d_u = dest[1];
  uint8_t *d_v = dest[2];

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411 < end) {
    // write first 2 y and 1 uv pixel
    *(d_y++) = yuv411->y0;
    *(d_y++) = yuv411->y1;
    *(d_u++) = yuv411->u2;
    *(d_v++) = yuv411->v2;

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      *(d_y++) = yuv411[j - 1].y2;
      *(d_y++) = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      *(d_u++) = avg_chromaf(h_u, yuv411[j - 1].u2);
      *(d_v++) = avg_chromaf(h_v, yuv411[j - 1].v2);

      // average first pixel to get 3/4

      *(d_y++) = yuv411[j].y0;
      *(d_y++) = yuv411[j].y1;

      *(d_u++) = avg_chromaf(h_u, yuv411[j].u2);
      *(d_v++) = avg_chromaf(h_v, yuv411[j].v2);

    }
    // write last pixels
    *(d_y++) = yuv411[j - 1].y2;
    *(d_y++) = yuv411[j - 1].y3;
    *(d_u++) = yuv411[j - 1].u2;
    *(d_v++) = yuv411[j - 1].v2;

    yuv411 += width;
  }
}


static void convert_yuv411_to_yuv420_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest,
    boolean is_yvu, int clamping) {
  register int j;
  yuv411_macropixel *end = yuv411 + width * height;
  uint8_t h_u, h_v, u, v;

  uint8_t *d_y = dest[0];
  uint8_t *d_u;
  uint8_t *d_v;

  boolean chroma = FALSE;

  size_t width2 = width << 1;

  if (!is_yvu) {
    d_u = dest[1];
    d_v = dest[2];
  } else {
    d_u = dest[2];
    d_v = dest[1];
  }

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411 < end) {
    // write first 2 y and 1 uv pixel
    *(d_y++) = yuv411->y0;
    *(d_y++) = yuv411->y1;

    u = yuv411->u2;
    v = yuv411->v2;

    if (!chroma) {
      *(d_u++) = u;
      *(d_v++) = v;
    } else {
      *d_u = avg_chromaf(*d_u, u);
      *d_v = avg_chromaf(*d_v, v);
    }

    for (j = 1; j < width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      *(d_y++) = yuv411[j - 1].y2;
      *(d_y++) = yuv411[j - 1].y3;

      h_u = avg_chromaf(yuv411[j - 1].u2, yuv411[j].u2);
      h_v = avg_chromaf(yuv411[j - 1].v2, yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u = avg_chromaf(h_u, yuv411[j - 1].u2);
      v = avg_chromaf(h_v, yuv411[j - 1].v2);

      if (!chroma) {
        *(d_u++) = u;
        *(d_v++) = v;
      } else {
        *d_u = avg_chromaf(*d_u, u);
        *d_v = avg_chromaf(*d_v, v);
      }

      // average first pixel to get 3/4

      *(d_y++) = yuv411[j].y0;
      *(d_y++) = yuv411[j].y1;

      u = avg_chromaf(h_u, yuv411[j].u2);
      v = avg_chromaf(h_v, yuv411[j].v2);

      if (!chroma) {
        *(d_u++) = u;
        *(d_v++) = v;
      } else {
        *d_u = avg_chromaf(*d_u, u);
        *d_v = avg_chromaf(*d_v, v);
      }

    }

    // write last pixels
    *(d_y++) = yuv411[j - 1].y2;
    *(d_y++) = yuv411[j - 1].y3;

    u = yuv411[j - 1].u2;
    v = yuv411[j - 1].v2;

    if (!chroma) {
      *(d_u++) = u;
      *(d_v++) = v;

      d_u -= width2;
      d_v -= width2;

    } else {
      *d_u = avg_chromaf(*d_u, u);
      *d_v = avg_chromaf(*d_v, v);
    }

    chroma = !chroma;
    yuv411 += width;
  }
}


static void convert_yuv420_to_yuv411_frame(uint8_t **src, int hsize, int vsize, yuv411_macropixel *dest,
    boolean is_422, int clamping) {
  // TODO -handle various sampling types

  register int i = 0, j;
  uint8_t *y, *u, *v, *end;
  boolean chroma = TRUE;

  size_t qwidth, hwidth;

  // TODO - handle different in sampling types
  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  y = src[0];
  u = src[1];
  v = src[2];

  end = y + hsize * vsize;

  hwidth = hsize >> 1;
  qwidth = hwidth >> 1;

  while (y < end) {
    for (j = 0; j < qwidth; j++) {
      dest->u2 = avg_chromaf(u[0], u[1]);
      dest->y0 = y[0];
      dest->y1 = y[1];
      dest->v2 = avg_chromaf(v[0], v[1]);
      dest->y2 = y[2];
      dest->y3 = y[3];

      if (!is_422 && chroma && i > 0) {
        dest[-qwidth].u2 = avg_chromaf(dest[-qwidth].u2, dest->u2);
        dest[-qwidth].v2 = avg_chromaf(dest[-qwidth].v2, dest->v2);
      }
      dest++;
      y += 4;
      u += 2;
      v += 2;
    }
    chroma = !chroma;
    if (!chroma && !is_422) {
      u -= hwidth;
      v -= hwidth;
    }
    i++;
  }
}


static void convert_splitplanes_frame(uint8_t *src, int width, int height, int irowstride, int *orowstrides,
                                      uint8_t **dest, boolean src_alpha, boolean dest_alpha) {
  // TODO - orowstrides
  // convert 888(8) packed to 444(4)P planar
  size_t size = width * height;
  int ipsize = 3;

  uint8_t *y = dest[0];
  uint8_t *u = dest[1];
  uint8_t *v = dest[2];
  uint8_t *a = dest_alpha ? dest[3] : NULL;

  uint8_t *end;

  register int i, j;

  if (src_alpha) ipsize = 4;

  if (irowstride == ipsize * width && irowstride == orowstrides[0] && irowstride == orowstrides[1]
      && irowstride == orowstrides[2] && (!dest_alpha || irowstride == orowstrides[3])) {
    for (end = src + size * ipsize; src < end;) {
      *(y++) = *(src++);
      *(u++) = *(src++);
      *(v++) = *(src++);
      if (dest_alpha) {
        if (src_alpha) *(a++) = *(src++);
        else *(a++) = 255;
      }
    }
  } else {
    orowstrides[0] -= width;
    orowstrides[1] -= width;
    orowstrides[2] -= width;
    width *= ipsize;
    irowstride -= width;
    if (dest_alpha) orowstrides[3] -= width;
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += ipsize) {
        *(y++) = *(src++);
        *(u++) = *(src++);
        *(v++) = *(src++);
        if (dest_alpha) {
          if (src_alpha) *(a++) = *(src++);
          else *(a++) = 255;
        }
      }
      y += orowstrides[0];
      u += orowstrides[1];
      v += orowstrides[2];
      if (dest_alpha) {
        a += orowstrides[3];
      }
      src += irowstride;
    }
  }
}


/////////////////////////////////////////////////////////////////
// RGB palette conversions

static void convert_swap3_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                uint8_t *dest, uint8_t *gamma_lut, int thread_id) {
  // swap 3 byte palette
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if (src == dest) {
    uint8_t tmp;
    int width3 = width * 3;
    orowstride -= width3;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width3; i += 3) {
        tmp = src[i];
        if (!gamma_lut) {
          dest[0] = src[i + 2]; // red
          dest[2] = tmp; // blue
        } else {
          dest[0] = gamma_lut[src[i + 2]]; // red
          dest[1] = gamma_lut[src[i + 1]]; // red
          dest[2] = gamma_lut[tmp]; // blue
        }
        dest += 3;
      }
      dest += orowstride;
    }
    return;
  }

  if ((irowstride == width * 3) && (orowstride == irowstride)) {
    // quick version
#ifdef ENABLE_OILx
    if (!gamma_lut) {
      oil_rgb2bgr(dest, src, width * height);
      return;
    }
#endif
    for (; src < end; src += 3) {
      if (!gamma_lut) {
        *(dest++) = src[2]; // red
        *(dest++) = src[1]; // green
        *(dest++) = src[0]; // blue
      } else {
        *(dest++) = gamma_lut[src[2]]; // red
        *(dest++) = gamma_lut[src[1]]; // green
        *(dest++) = gamma_lut[src[0]]; // blue
      }
    }
  } else {
    int width3 = width * 3;
    orowstride -= width3;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width3; i += 3) {
        if (!gamma_lut) {
          *(dest++) = src[i + 2]; // red
          *(dest++) = src[i + 1]; // green
          *(dest++) = src[i]; // blue
        } else {
          *(dest++) = gamma_lut[src[2]]; // red
          *(dest++) = gamma_lut[src[1]]; // green
          *(dest++) = gamma_lut[src[0]]; // blue
        }
      }
      dest += orowstride;
    }
  }
}


static void *convert_swap3_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                      ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_swap4_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                uint8_t *dest, int thread_id) {
  // swap 4 byte palette
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if ((height - dheight * i) < dheight) dheight = height - (dheight * i);

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap4_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap4_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if (src == dest) {
    uint8_t tmp[4];
    int width4 = width * 4;
    orowstride -= width4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        tmp[0] = src[i + 3]; // alpha
        tmp[1] = src[i + 2]; // red
        tmp[2] = src[i + 1]; // green
        tmp[3] = src[i]; // blue
        lives_memcpy(dest, tmp, 4);
        dest += 4;
      }
      dest += orowstride;
    }
    return;
  }

  if ((irowstride == width * 4) && (orowstride == irowstride)) {
    // quick version
    for (; src < end; src += 4) {
      *(dest++) = src[3]; // alpha
      *(dest++) = src[2]; // red
      *(dest++) = src[1]; // green
      *(dest++) = src[0]; // blue
    }
  } else {
    int width4 = width * 4;
    orowstride -= width4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        *(dest++) = src[i + 3]; // alpha
        *(dest++) = src[i + 2]; // red
        *(dest++) = src[i + 1]; // green
        *(dest++) = src[i]; // blue
      }
      dest += orowstride;
    }
  }
}


static void *convert_swap4_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap4_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                      ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swap3addpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                       uint8_t *dest, int thread_id) {
  // swap 3 bytes, add post alpha
  uint8_t *end = src + height * irowstride;
  int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3addpost_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3addpost_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride == width * 3) && (orowstride == width * 4)) {
    // quick version
    for (; src < end; src += 3) {
      *(dest++) = src[2]; // red
      *(dest++) = src[1]; // green
      *(dest++) = src[0]; // blue
      *(dest++) = 255; // alpha
    }
  } else {
    int j, k;
    for (j = 0; j < height; j++) {
      k = 0;
      for (i = 0; i < width * 3; i += 3) {
        dest[orowstride * j + k++] = src[irowstride * j + i + 2]; // red
        dest[orowstride * j + k++] = src[irowstride * j + i + 1]; // green
        dest[orowstride * j + k++] = src[irowstride * j + i]; // blue
        dest[orowstride * j + k++] = 255; // alpha
      }
    }
  }
}


static void *convert_swap3addpost_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3addpost_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                             ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swap3addpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, int thread_id) {
  // swap 3 bytes, add pre alpha
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3addpre_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3addpre_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }
  if ((irowstride == width * 3) && (orowstride == width * 4)) {
    // quick version
    for (; src < end; src += 3) {
      *(dest++) = 255; // alpha
      *(dest++) = src[2]; // red
      *(dest++) = src[1]; // green
      *(dest++) = src[0]; // blue
    }
  } else {
    int width3 = width * 3;
    orowstride -= width * 4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width3; i += 3) {
        *(dest++) = 255; // alpha
        *(dest++) = src[i + 2]; // red
        *(dest++) = src[i + 1]; // green
        *(dest++) = src[i]; // blue
      }
      dest += orowstride;
    }
  }
}


static void *convert_swap3addpre_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3addpre_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swap3postalpha_frame(uint8_t *src, int width, int height, int rowstride,
    int thread_id) {
  // swap 3 bytes, leave alpha
  uint8_t *end = src + height * rowstride, tmp;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * rowstride) < end) {
        ccparams[i].src = src + dheight * i * rowstride;
        ccparams[i].hsize = width;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3postalpha_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3postalpha_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  rowstride -= width << 2;
  for (; src < end; src += rowstride) {
    for (i = 0; i < width; i++) {
      tmp = src[0];
      src[0] = src[2];
      src[2] = tmp;
      src += 4;
    }
  }
}


static void *convert_swap3postalpha_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3postalpha_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                               ccparams->thread_id);
  return NULL;
}

#ifdef WEED_ADVANCED_PALETTES
static void convert_swap3prealpha_frame(uint8_t *src, int width, int height, int rowstride,
                                        int thread_id) {
  // swap 3 bytes, leave alpha
  uint8_t *end = src + height * rowstride, tmp;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * rowstride) < end) {
        ccparams[i].src = src + dheight * i * rowstride;
        ccparams[i].hsize = width;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = rowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3prealpha_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3prealpha_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  rowstride -= width << 2;

  for (; src < end; src += rowstride) {
    for (i = 0; i < width; i++) {
      tmp = src[1];
      src[1] = src[3];
      src[3] = tmp;
      src += 4;
    }
  }
}


static void *convert_swap3prealpha_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3prealpha_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize,
                              ccparams->irowstrides[0], ccparams->thread_id);
  return NULL;
}
#endif

static void convert_addpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                  uint8_t *dest, uint8_t *gamma_lut, int thread_id) {
  // add post alpha
  int i, end = height * irowstride, j = 0;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    uint8_t *end = src + height * irowstride;
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_addpost_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_addpost_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if ((irowstride == width * 3) && (orowstride == width * 4)) {
    // quick version
#ifdef ENABLE_OIL
    if (!gamma_lut) {
      oil_rgb2rgba(dest, src, width * height);
      return;
    }
#endif
    for (i = 0; i < end; i += 3) {
      if (!gamma_lut) {
        lives_memcpy(&dest[j], &src[i], 3);
        j += 3;
      } else {
        dest[j++] = gamma_lut[src[i]];
        dest[j++] = gamma_lut[src[i + 1]];
        dest[j++] = gamma_lut[src[i + 2]];
      }
      dest[j++] = 255; // alpha
    }
  } else {
    int width3 = width * 3;
    for (int k = 0; k < height; k++) {
      j = 0;
      for (i = 0; i < width3; i += 3) {
        if (!gamma_lut) {
          lives_memcpy(&dest[orowstride * k + j], &src[irowstride * k + i], 3);
        } else {
          dest[orowstride * k + j] = gamma_lut[src[irowstride * k + i]];
          dest[orowstride * k + j + 1] = gamma_lut[src[irowstride * k + i + 1]];
          dest[orowstride * k + j + 2] = gamma_lut[src[irowstride * k + i + 2]];
        }
        j += 3;
        dest[orowstride * k + j++] = 255; // alpha
      }
    }
  }
}


static void *convert_addpost_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_addpost_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                        ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_addpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                 uint8_t *dest, int thread_id) {
  // add pre alpha
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_addpre_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_addpre_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride == width * 3) && (orowstride == width * 4)) {
    // quick version
    for (; src < end; src += 3) {
      *(dest++) = 255; // alpha
      lives_memcpy(dest, src, 3);
      dest += 3;
    }
  } else {
    int width3 = width * 3;
    orowstride -= width * 4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width3; i += 3) {
        *(dest++) = 255; // alpha
        lives_memcpy(dest, src + i, 3);
        dest += 3;
      }
      dest += orowstride;
    }
  }
}


static void *convert_addpre_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_addpre_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                       ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swap3delpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                       uint8_t *dest, int thread_id) {
  // swap 3 bytes, delete post alpha
  int i, end = height * irowstride, j = 0;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    uint8_t *end = src + height * irowstride;
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3delpost_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3delpost_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride == width * 4) && (orowstride == width * 3)) {
    // quick version
    for (i = 0; i < end; i += 4) {
      dest[j++] = src[i + 2]; // red
      dest[j++] = src[i + 1]; // green
      dest[j++] = src[i]; // blue
    }
  } else {
    int width4 = width * 4;
    for (int k = 0; k < height; k++) {
      j = 0;
      for (i = 0; i < width4; i += 4) {
        dest[orowstride * k + j++] = src[irowstride * k + i + 2]; // red
        dest[orowstride * k + j++] = src[irowstride * k + i + 1]; // green
        dest[orowstride * k + j++] = src[irowstride * k + i]; // blue
      }
    }
  }
}


static void *convert_swap3delpost_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3delpost_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                             ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_delpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                  uint8_t *dest, uint8_t *gamma_lut, int thread_id) {
  // delete post alpha
  int i, end = height * irowstride, j = 0;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    uint8_t *end = src + height * irowstride;
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].lut = gamma_lut;
        ccparams[i].thread_id = i;

        if (i == 0) convert_delpost_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_delpost_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
    return;
  }

  if ((irowstride == width * 4) && (orowstride == width * 3)) {
    // quick version
    if (!gamma_lut) {
      for (i = 0; i < end; i += 4) {
        lives_memcpy(&dest[j], &src[i], 3);
        j += 3;
      }
    } else {
      int width4 = width * 4;
      for (int k = 0; k < height; k++) {
        j = 0;
        for (i = 0; i < width4; i += 4) {
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i]];
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i + 1]];
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i + 2]];
        }
      }
    }
  } else {
    int width4 = width * 4;
    for (int k = 0; k < height; k++) {
      j = 0;
      for (i = 0; i < width4; i += 4) {
        if (!gamma_lut) {
          lives_memcpy(&dest[orowstride * k + j], &src[irowstride * k + i], 3);
          j += 3;
        } else {
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i]];
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i + 1]];
          dest[orowstride * k + j++] = gamma_lut[src[irowstride * k + i + 2]];
        }
      }
    }
  }
}


static void *convert_delpost_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_delpost_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                        ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->lut, ccparams->thread_id);
  return NULL;
}


static void convert_delpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                 uint8_t *dest, int thread_id) {
  // delete pre alpha
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_delpre_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_delpre_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  src++;

  if ((irowstride == width * 4) && (orowstride == width * 3)) {
    // quick version
    for (; src < end; src += 4) {
      lives_memcpy(dest, src, 3);
      dest += 3;
    }
  } else {
    int width4 = width * 4;
    orowstride -= width * 3;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        lives_memcpy(dest, src + i, 3);
        dest += 3;
      }
      dest += orowstride;
    }
  }
}


static void *convert_delpre_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_delpre_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                       ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swap3delpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, int thread_id) {
  // delete pre alpha, swap last 3
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swap3delpre_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swap3delpre_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride == width * 4) && (orowstride == width * 3)) {
    // quick version
    for (; src < end; src += 4) {
      *(dest++) = src[3]; // red
      *(dest++) = src[2]; // green
      *(dest++) = src[1]; // blue
    }
  } else {
    int width4 = width * 4;
    orowstride -= width * 3;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        *(dest++) = src[i + 3]; // red
        *(dest++) = src[i + 2]; // green
        *(dest++) = src[i + 1]; // blue
      }
      dest += orowstride;
    }
  }
}


static void *convert_swap3delpre_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swap3delpre_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_swapprepost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, int thread_id) {
  // swap first and last bytes in a 4 byte palette
  uint64_t *uup;
  uint8_t *end = src + height * irowstride;
  register int i;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * irowstride) < end) {
        ccparams[i].src = src + dheight * i * irowstride;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orowstride;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;

        ccparams[i].irowstrides[0] = irowstride;
        ccparams[i].orowstrides[0] = orowstride;
        ccparams[i].thread_id = i;

        if (i == 0) convert_swapprepost_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swapprepost_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  uup = (uint64_t *)src;
  if ((void *)uup == (void *)src) {
    uint64_t uu;
    int width8 = width >> 3;
    orowstride -= width * 4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width8; i++) {
        uu = ((*uup & 0xFF000000FF000000) >> 24);
        uu |= ((*uup & 0x00FFFFFF00FFFFFF) << 8);
        lives_memcpy(dest, &uu, 8);
        dest += 8;
        uup++;
      }
      dest += orowstride;
    }
    return;
  }

  if (src == dest) {
    uint8_t tmp;
    int width4 = width << 2;
    orowstride -= width4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        tmp = dest[i];
        lives_memmove(&dest[i], &dest[i + 1], 3);
        dest[i + 3] = tmp;
      }
      dest += orowstride;
    }
    return;
  } else {
    uint8_t tmp;
    int width4 = width << 2;
    orowstride -= width4;
    for (; src < end; src += irowstride) {
      for (i = 0; i < width4; i += 4) {
        tmp = src[i];
        lives_memcpy(&dest[i], &src[i + 1], 3);
        dest[i + 3] = tmp;
      }
      dest += orowstride;
    }
  }
}


static void *convert_swapprepost_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swapprepost_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize, ccparams->irowstrides[0],
                            ccparams->orowstrides[0], (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


//////////////////////////
// generic YUV

static void convert_swab_frame(uint8_t *src, int width, int height, int irow, int orow, uint8_t *dest, int thread_id) {
  register int i;
  int width4 = width * 4;
  uint8_t *end = src + height * irow;

  if (thread_id == -1 && prefs->nfx_threads > 1) {
    lives_thread_t threads[prefs->nfx_threads];
    int nthreads = 1;
    int dheight, xdheight;
    lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(prefs->nfx_threads, sizeof(lives_cc_params));

    xdheight = CEIL((double)height / (double)prefs->nfx_threads, 4);
    for (i = prefs->nfx_threads; i--;) {
      dheight = xdheight;

      if ((src + dheight * i * width4) < end) {
        ccparams[i].src = src + dheight * i * irow;
        ccparams[i].hsize = width;
        ccparams[i].dest = dest + dheight * i * orow;

        if (dheight * (i + 1) > (height - 4)) {
          dheight = height - (dheight * i);
        }

        ccparams[i].vsize = dheight;
        ccparams[i].irowstrides[0] = irow;
        ccparams[i].orowstrides[0] = orow;

        ccparams[i].thread_id = i;

        if (i == 0) convert_swab_frame_thread(&ccparams[i]);
        else {
          lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, convert_swab_frame_thread, &ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i = 1; i < nthreads; i++) {
      lives_thread_join(threads[i], NULL);
    }
    lives_free(ccparams);
    return;
  }

  for (; src < end; src += irow) {
    for (i = 0; i < width4; i += 4) {
      swab4(&src[i], &dest[i], 1);
    }
    dest += orow;
  }
}


static void *convert_swab_frame_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  convert_swab_frame((uint8_t *)ccparams->src, ccparams->hsize, ccparams->vsize,
                     ccparams->irowstrides[0], ccparams->orowstrides[0],
                     (uint8_t *)ccparams->dest, ccparams->thread_id);
  return NULL;
}


static void convert_halve_chroma(uint8_t **src, int width, int height, int *istrides, int *ostrides, uint8_t **dest,
                                 int clamping) {
  // width and height here are width and height of src *chroma* planes, in bytes

  // halve the chroma samples vertically, with sub-sampling, e.g. 422p to 420p

  // TODO : handle different sampling methods in and out

  int i, j, i2 = 0;
  uint8_t *d_u = dest[1], *d_v = dest[2], *s_u = src[1], *s_v = src[2];
  boolean chroma = FALSE;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  for (i = 0; i < height; i++) {
    if (!chroma) {
      // pass 1, copy row
      lives_memcpy(&d_u[ostrides[1] * i2], &s_u[istrides[1] * i], width);
      lives_memcpy(&d_v[ostrides[2] * i2], &s_v[istrides[2] * i], width);
    } else {
      for (j = 0; j < width; j++) {
        // pass 2
        // average two dest rows
        d_u[ostrides[1] * i2 + j] = avg_chromaf(d_u[ostrides[1] * i2 + j], s_u[istrides[1] * i + j]);
        d_v[ostrides[2] * i2 + j] = avg_chromaf(d_v[ostrides[2] * i2 + j], s_v[istrides[2] * i + j]);
      }
    }
    if (chroma) i2++;
    chroma = !chroma;
  }
}


static void convert_double_chroma(uint8_t **src, int width, int height, int *istrides, int *ostrides, uint8_t **dest,
                                  int clamping) {
  // width and height here are width and height of src *chroma* planes, in bytes
  // double two chroma planes vertically, with interpolation: eg: 420p to 422p

  int i, j, i2 = 0;
  uint8_t *d_u = dest[1], *d_v = dest[2], *s_u = src[1], *s_v = src[2];
  boolean chroma = FALSE;
  int height2 = height << 1;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  for (i = 0; i < height2; i++) {
    lives_memcpy(&d_u[ostrides[1] * i], &s_u[istrides[1] * i2], width);
    lives_memcpy(&d_v[ostrides[2] * i], &s_v[istrides[2] * i2], width);
    if (!chroma && i > 0) {
      for (j = 0; j < width; j++) {
        // pass 2
        // average two src rows
        d_u[ostrides[1] * (i - 1) + j] = avg_chromaf(d_u[ostrides[1] * (i - 1) + j], d_u[ostrides[1] * i + j]);
        d_v[ostrides[2] * (i - 1) + j] = avg_chromaf(d_v[ostrides[2] * (i - 1) + j], d_v[ostrides[2] * i + j]);
      }
    }
    if (chroma) i2++;
    chroma = !chroma;
  }
}


static void convert_quad_chroma(uint8_t **src, int width, int height, int *istrides, int ostride, uint8_t **dest,
                                boolean add_alpha, int sampling, int clamping) {
  // width and height here are width and height of dest chroma planes, in bytes
  // double the chroma samples vertically and horizontally, with interpolation
  // output to planes, eg. 420p to 444p

  register int i, j;
  uint8_t *d_u = dest[1], *d_v = dest[2], *s_u = src[1], *s_v = src[2];
  int uv_offs, lastrow;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  width = (width >> 1) << 1;
  lastrow = (height >> 1) << 1;

  for (i = 0; i < height; i++) {
    uv_offs = 0;
    for (j = 0; j < width; j++) {
      if (!(i & 1) || i == lastrow) {
        if (uv_offs > 0) {
          // uv supersampling
          if (sampling == WEED_YUV_SAMPLING_JPEG) {
            d_u[j] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
            d_v[j] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
          } else {
            d_u[j] = avg_chroma_3_1f(s_u[uv_offs - 1], s_u[uv_offs]);
            d_v[j] = avg_chroma_1_3f(s_v[uv_offs - 1], s_v[uv_offs]);
          }
        } else {
          d_u[j] = s_u[uv_offs];
          d_v[j] = s_v[uv_offs];
        }
        ++uv_offs;
        j++;
        if (sampling == WEED_YUV_SAMPLING_JPEG) {
          d_u[j] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
          d_v[j] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
        } else {
          d_u[j] = avg_chroma_1_3f(s_u[uv_offs - 1], s_u[uv_offs]);
          d_v[j] = avg_chroma_3_1f(s_v[uv_offs - 1], s_v[uv_offs]);
        }
      } else if (i > 1) {
        // on odd rows we average row - 1 with row - 3  ==> row - 2
        int jj = j - (ostride << 1);
        d_u[jj] = avg_chromaf(d_u[jj + ostride], d_u[jj - ostride]);
        d_v[jj] = avg_chromaf(d_v[jj + ostride], d_v[jj - ostride]);
        jj++;
        d_u[jj] = avg_chromaf(d_u[jj + ostride], d_u[jj - ostride]);
        d_v[jj] = avg_chromaf(d_v[jj + ostride], d_v[jj - ostride]);
      }
    }

    if (i & 1) {
      // after an odd row we advance u, v
      s_u += istrides[1];
      s_v += istrides[2];
    }
    d_u += ostride;
    d_v += ostride;

  }
  if (i > lastrow) {
    // TRUE if we finished on an even row
    for (j = 0; j < width; j++) {
      d_u[j - ostride * 2] = avg_chromaf(d_u[j - ostride * 3], d_u[j - ostride]);
      d_v[j - ostride * 2] = avg_chromaf(d_v[j - ostride * 3], d_v[j - ostride]);
    }
  }
  if (add_alpha) lives_memset(dest[3], 255, ostride * height);
}


static void convert_quad_chroma_packed(uint8_t **src, int width, int height, int *istrides, int ostride,
                                       uint8_t *dest, boolean add_alpha, int sampling, int clamping) {
  // width and height here are width and height of dest chroma planes, in bytes
  // stretch (double) the chroma samples vertically and horizontally, with interpolation
  // output to packed pixels

  // e.g: 420p to 888(8)

  int i, j;
  int irow = istrides[0] - width;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  int opsize = 3, uv_offs;
  int lastrow = (height >> 1) << 1; // height if even, height - 1 if odd

  //int count;
  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) opsize = 4;

  width = ((width >> 1) << 1) * opsize;

  for (i = 0; i < height; i++) {
    uv_offs = 0;
    for (j = 0; j < width; j += opsize) {
      // implements jpeg / mpeg style subsampling : TODO - dvpal style
      if (!(i & 1) || i == lastrow) {
        // even rows (0, 2, 4, ...) are normal
        dest[j] = *(s_y++);
        if (uv_offs > 0) {
          // uv supersampling
          if (sampling == WEED_YUV_SAMPLING_JPEG) {
            dest[j + 1] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
            dest[j + 2] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
          } else {
            dest[j + 1] = avg_chroma_3_1f(s_u[uv_offs - 1], s_u[uv_offs]);
            dest[j + 2] = avg_chroma_1_3f(s_v[uv_offs - 1], s_v[uv_offs]);
          }
        } else {
          dest[j + 1] = s_u[uv_offs];
          dest[j + 2] = s_v[uv_offs];
        }

        if (add_alpha) dest[j + 3] = 255;
        ++uv_offs;
        j += opsize;
        dest[j] = *(s_y++);

        if (sampling == WEED_YUV_SAMPLING_JPEG) {
          dest[j + 1] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
        } else {
          dest[j + 1] = avg_chroma_1_3f(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chroma_3_1f(s_v[uv_offs - 1], s_v[uv_offs]);
        }
        if (add_alpha) dest[j + 3] = 255;
      } else {
        int jj = j - (ostride << 1);
        // y part is normal
        dest[j] = *(s_y++);
        if (i > 1) { // i >= 3
          // chroma part:
          // on odd rows we average row - 1 with row - 3  ==> row - 2
          dest[jj + 1] = avg_chroma(dest[jj + 1 + ostride], dest[jj + 1 - ostride]);
          dest[jj + 2] = avg_chroma(dest[jj + 2 + ostride], dest[jj + 2 - ostride]);
          jj += opsize;
        }
        j += opsize;
        dest[j] = *(s_y++);
        if (i > 1) {
          dest[jj + 1] = avg_chroma(dest[jj + 1 + ostride], dest[jj + 1 - ostride]);
          dest[jj + 2] = avg_chroma(dest[jj + 2 + ostride], dest[jj + 2 - ostride]);
        }
      }
    }

    if (i & 1) {
      // after an odd row we advance u, v
      s_u += istrides[1];
      s_v += istrides[2];
    }
    // y advances on every row
    s_y += irow;
    dest += ostride;
  }
  if (i > lastrow) {
    // TRUE if we finished on an even row
    for (j = 0; j < width; j += opsize) {
      // we would have done this on the next row, but there is no next row
      int jj = j - ostride;
      dest[jj + 1] = avg_chromaf(dest[jj + 1 + ostride], dest[jj + 1 - ostride]);
      dest[jj + 2] = avg_chromaf(dest[jj + 2 + ostride], dest[jj + 2 - ostride]);
    }
  }
}


static void convert_double_chroma_packed(uint8_t **src, int width, int height, int *istrides, int ostride, uint8_t *dest,
    boolean add_alpha, int sampling, int clamping) {
  // width and height here are width and height of dest chroma planes, in bytes
  // double the chroma samples horizontally, with interpolation

  // output to packed pixels

  // e.g 422p to 888(8)

  int i, j;
  uint8_t *s_y = src[0], *s_u = src[1], *s_v = src[2];
  int irow = istrides[0] - width;
  int opsize = 3, uv_offs;

  set_conversion_arrays(clamping, WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) opsize = 4;
  width *= opsize;

  for (i = 0; i < height; i++) {
    uv_offs = 0;
    for (j = 0; j < width; j += opsize) {
      dest[j] = *(s_y++);
      if (uv_offs > 0) {
        // uv supersampling
        if (sampling == WEED_YUV_SAMPLING_JPEG) {
          dest[j + 1] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
        } else {
          dest[j + 1] = avg_chroma_3_1f(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chroma_1_3f(s_v[uv_offs - 1], s_v[uv_offs]);
        }
      } else {
        dest[j + 1] = s_u[uv_offs];
        dest[j + 2] = s_v[uv_offs];
      }
      if (add_alpha) dest[j + 3] = 255;

      j += opsize;
      ++uv_offs;

      dest[j] = *(s_y++);
      if (uv_offs > 0) {
        // uv supersampling
        if (sampling == WEED_YUV_SAMPLING_JPEG) {
          dest[j + 1] = avg_chromaf(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chromaf(s_v[uv_offs - 1], s_v[uv_offs]);
        } else {
          dest[j + 1] = avg_chroma_3_1f(s_u[uv_offs - 1], s_u[uv_offs]);
          dest[j + 2] = avg_chroma_1_3f(s_v[uv_offs - 1], s_v[uv_offs]);
        }
      } else {
        dest[j + 1] = s_u[uv_offs];
        dest[j + 2] = s_v[uv_offs];
      }
    }
    s_y += irow;
    s_u += istrides[1];
    s_v += istrides[2];
    dest += ostride;
  }
}


static void switch_yuv_sampling(weed_layer_t *layer) {
  int sampling, clamping, subspace;
  int palette = weed_layer_get_palette_yuv(layer, &clamping, &sampling, &subspace);
  int width = weed_layer_get_width(layer) >> 1;
  int height = weed_layer_get_height(layer) >> 1;
  unsigned char **pixel_data, *dst;
  register int i, j, k;

  if (palette != WEED_PALETTE_YUV420P) return;

  pixel_data = (unsigned char **)weed_layer_get_pixel_data_planar(layer, NULL);

  if (sampling == WEED_YUV_SAMPLING_MPEG) {
    // jpeg is located centrally between Y, mpeg(2) and some flv are located on the left Y
    // so here we just set dst[0]=avg(src[0],src[1]), dst[1]=avg(src[1],src[2]), etc.
    // the last value is repeated once

    // however, I think the values alternate so u : 0, 2, 4....v: 1, 3, 5...
    // and we want u = 0.5, 2.5, 4.5....
    // so, starting from 0 u = 3/4 x + 1 /4 x + 1
    // and v = 1/4 x + 3/4 x + 1

    width--;
    for (k = 1; k < 3; k++) {
      dst = pixel_data[k];
      for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
          if (k == 1) dst[i] = avg_chroma_3_1f(dst[i], avg_chromaf(dst[i], dst[i + 1]));
          else dst[i] = avg_chroma_1_3f(avg_chromaf(dst[i], dst[i + 1]), dst[i + 1]);
        }
        dst += width + 1;
      }
    }
    weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_JPEG);
  } else if (sampling == WEED_YUV_SAMPLING_JPEG) {
    // the other way round is just the inverse
    width--;
    for (k = 1; k < 3; k++) {
      dst = pixel_data[k];
      for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
          if (k == 2) dst[i] = avg_chromaf(dst[i], avg_chromaf(dst[i], dst[i + 1]));
          else dst[i] = avg_chromaf(avg_chromaf(dst[i], dst[i + 1]), dst[i + 1]);
        }
        dst += width + 1;
      }
    }
    weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_MPEG);
  }
  lives_free(pixel_data);
}


static void switch_yuv_clamping_and_subspace(weed_layer_t *layer, int oclamping, int osubspace) {
  // currently subspace conversions are not performed - TODO
  // we assume subspace Y'CbCr
  int iclamping = weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL);
  int isubspace = weed_get_int_value(layer, WEED_LEAF_YUV_SUBSPACE, NULL);

  int palette = weed_get_int_value(layer, WEED_LEAF_CURRENT_PALETTE, NULL);
  int height = weed_get_int_value(layer, WEED_LEAF_HEIGHT, NULL);
  int rowstride = weed_layer_get_rowstride(layer);
  void **pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);

  uint8_t *src, *src1, *src2, *end;

  get_YUV_to_YUV_conversion_arrays(iclamping, isubspace, oclamping, osubspace);

  switch (palette) {
  case WEED_PALETTE_YUVA8888:
    src = (uint8_t *)pixel_data[0];
    end = src + height * rowstride;
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = U_to_U[*src];
      src++;
      *src = V_to_V[*src];
      src += 2;
    }
    break;
  case WEED_PALETTE_YUV888:
    src = (uint8_t *)pixel_data[0];
    end = src + height * rowstride;
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = U_to_U[*src];
      src++;
      *src = V_to_V[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUVA4444P:
  case WEED_PALETTE_YUV444P:
    src = (uint8_t *)pixel_data[0];
    src1 = (uint8_t *)pixel_data[1];
    src2 = (uint8_t *)pixel_data[2];
    end = src + height * rowstride;
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src1 = U_to_U[*src1];
      src1++;
      *src2 = V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_UYVY:
    src = (uint8_t *)pixel_data[0];
    end = src + height * rowstride;
    while (src < end) {
      *src = U_to_U[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = V_to_V[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUYV:
    src = (uint8_t *)pixel_data[0];
    end = src + height * rowstride;
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = U_to_U[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = V_to_V[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUV422P:
    src = (uint8_t *)pixel_data[0];
    src1 = (uint8_t *)pixel_data[1];
    src2 = (uint8_t *)pixel_data[2];
    end = src + height * rowstride;
    // TODO: u, v rowstrides
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src1 = U_to_U[*src1];
      src1++;
      *src2 = V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YVU420P:
    src = (uint8_t *)pixel_data[0];
    src1 = (uint8_t *)pixel_data[2];
    src2 = (uint8_t *)pixel_data[1];
    end = src + height * rowstride;
    // TODO: u, v rowstrides
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src1 = U_to_U[*src1];
      src1++;
      *src2 = V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YUV420P:
    src = (uint8_t *)pixel_data[0];
    src1 = (uint8_t *)pixel_data[1];
    src2 = (uint8_t *)pixel_data[2];
    end = src + height * rowstride;
    // TODO: u, v rowstrides
    while (src < end) {
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src1 = U_to_U[*src1];
      src1++;
      *src2 = V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YUV411:
    src = (uint8_t *)pixel_data[0];
    end = src + height * rowstride;
    while (src < end) {
      *src = U_to_U[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = V_to_V[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
      *src = Y_to_Y[*src];
      src++;
    }
    break;
  }
  weed_set_int_value(layer, WEED_LEAF_YUV_CLAMPING, oclamping);
  lives_free(pixel_data);
}


////////////////////////////////////////////////////////////////////////////////////////
// TODO - move into layers.c

/**
   a "layer" is CHANNEL type plant which is not created from a plugin CHANNEL_TEMPLATE.
   When we pass this to a plugin, we need to adjust it depending
   on the plugin's CHANNEL_TEMPLATE to which we will assign it.

   e.g.: memory may need aligning afterwards for particular plugins which set channel template flags:
   layer palette may need changing, layer may need resizing */

/**
   @brief fills the plane pointed to by ptr with bpix

   psize is sizeof(bpix), width, height and rowstride are the dimensions of the target plane
*/
LIVES_INLINE void fill_plane(uint8_t *ptr, int psize, int width, int height, int rowstride, unsigned char *bpix) {
  int j, widthp = width * psize;
  for (j = 0; j < width; j++) {
    lives_memcpy(&ptr[psize * j], bpix, psize);
  }
  for (j = 1; j < height; j++) {
    lives_memcpy(&ptr[rowstride * j], &ptr[rowstride * (j - 1)], widthp);
  }
}


static size_t blank_pixel(uint8_t *dst, int pal, int yuv_clamping, uint8_t *src) {
  // set src to non-null to preserve the alpha channel (if applicable)
  // yuv_clamping
  // only valid for non-planar (packed) palettes
  uint8_t y_black = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16;
  switch (pal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    dst[0] = dst[1] = dst[2] = 0;
    return 3;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    dst[0] = dst[1] = dst[2] = 0;
    dst[3] = src ? src[3] : 255;
    return 4;
  case WEED_PALETTE_ARGB32:
    dst[0] = src ? src[0] : 255;
    dst[1] = dst[2] = dst[3] = 0;
    return 4;
  case WEED_PALETTE_UYVY8888:
    dst[1] = dst[3] = y_black;
    dst[0] = dst[2] = 128;
    return 4;
  case WEED_PALETTE_YUYV8888:
    dst[0] = dst[2] = y_black;
    dst[1] = dst[3] = 128;
    dst += 4;
    break;
  case WEED_PALETTE_YUV888:
    dst[0] = y_black;
    dst[1] = dst[2] = 128;
    return 3;
  case WEED_PALETTE_YUVA8888:
    dst[0] = y_black;
    dst[1] = dst[2] = 128;
    dst[3] = src ? src[3] : 255;
    return 4;
  case WEED_PALETTE_YUV411:
    dst[0] = dst[3] = 128;
    dst[1] = dst[2] = dst[4] = dst[5] = y_black;
    return 6;
  default: break;
  }
  return 0;
}

static void blank_row(uint8_t **pdst, int width, int pal, int nplanes,
                      int yuv_clamping, int uvcopy, uint8_t **psrc) {
  // for YUV420 and YVU420, only set uvcopy for even rows,
  //and increment pdst[1], pdst[2] on the odd rows
  int p;
  uint8_t *dst = *pdst, *src = NULL, black[3];
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {
    lives_memset(dst, 0, width * 3);
    return;
  }
  if (!uvcopy && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P)) nplanes = 1;
  black[0] = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16;
  black[1] = black[2] = 128;
  for (p = 0; p < nplanes; p++) {
    dst = pdst[p];
    if (psrc) src = psrc[p];
    if (p == 3) {
      if (!src) lives_memset(dst, 255, width);
      else lives_memcpy(dst, src, width);
      break;
    }
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P
        || pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P)
      lives_memset(dst, black[p], width);
    else {
      // RGBA, BGRA, ARGB, YUV888, YUVA8888, UYVY, YUYV, YUV411
      size_t offs = 0;
      if (src) {
        for (int i = 0; i < width; i++) {
          offs += blank_pixel(&dst[offs], pal, yuv_clamping, &src[offs]);
        }
      } else {
        for (int i = 0; i < width; i++) {
          offs += blank_pixel(&dst[offs], pal, yuv_clamping, NULL);
        }
      }
      break;
    }
    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P
                   || pal == WEED_PALETTE_YUV422P))
      width >>= 1;
  }
}

static void blank_frame(void **pdata, int width, int height, int *rs,
                        int pal, int nplanes, int yuv_clamping) {
  uint8_t *pd2[4];
  boolean is_420 = (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P), even = TRUE;
  for (int j = 0; j < nplanes; j++) pd2[j] = (uint8_t *)pdata[j];
  for (int i = 0; i < height; i++) {
    blank_row(pd2, width, pal, nplanes, yuv_clamping, even, NULL);
    even = !even;
    for (int j = 0; j < nplanes; j++) {
      pd2[j] += rs[j];
      if (is_420 && even) break;
    }
  }
}


boolean weed_layer_clear_pixel_data(weed_layer_t *layer) {
  // reset the layer to black, without freeing the pixel_data
  void **pd;
  int *rs;
  int pal, clamping, nplanes;
  if (!layer) return FALSE;
  pd = weed_layer_get_pixel_data_planar(layer, &nplanes);
  if (!pd || !*pd || !nplanes) return FALSE;
  pal = weed_layer_get_palette_yuv(layer, &clamping, NULL, NULL);
  rs = weed_layer_get_rowstrides(layer, NULL);
  blank_frame(pd, weed_layer_get_width(layer), weed_layer_get_height(layer), rs,
              pal, nplanes, clamping);
  lives_free(rs);
  lives_free(pd);
  return TRUE;
}

/**
   @brief creates pixel data for layer

   @returns FALSE on memory error

   width, height, and current_palette must be pre-set in layer; width is in (macro) pixels of the palette
   width and height may be adjusted (rounded) in the function
   rowstrides will be set, and each plane will be aligned depending on THREADVAR(rowstride_alignment)
   if THREADVAR(rowstride_alignment_hint) is non 0 it will set THREADVAR(rowstride_alignment), which must be a power of 2
   the special value -1 for the hint will create compact frames (rowstride = width * pixel_size)

   if black_fill is set, fill with opaque black in the specified palette: for yuv palettes, YUV_clamping may be pre-set
   otherwise it will be set to WEED_YUV_CLAMPING_CLAMPED.

   may_contig should normally be set to TRUE, except for special uses during palette conversion
   if set, then for planar palettes, only plane 0 will be allocated but with size sufficient to
   contain all planes, and the pointers will point to locations inside the bigger block.
   with rowstrides properly aligned, and only the pointer to plane 0 should be freed.
   Amongst other things, this allows for more optimised memory use.

   In this case, the leaf LIVES_LEAF_PIXEL_DATA_CONTIGUOUS will be set to WEED_TRUE. If this is
   not set (or WEED_FALSE) then each plane should be freed individually.

   the allocated frames will be aligned to the pixel size for whatever palette
   and may be padded with extra bytes to guard against accidental overwrites.
   The default rowstride alignment is 16 (or RS_ALIGN_DEF) bytes
   NB.
   since and RGBA8 pixel is 4 bytes, this implies
   a rowstride multiple of 4 pixels. If this is converted to RGB8, each pixel is now 3 bytes,
   so 4 pixels would take up 12 bytes and there wull be an extra 4 bytes of space between rows.
   The other way round, if we start with RGB8 and go in terms of 4 bytes then we get 12 byte
   multiples, and when converting to RGBA this is only 3 pixels, which can casue problems for
   openGL for example. Thus it is better to use an alignment of 16 and add some blank space for
   RGB.
   If a thread wants a rowstride alignment other than 16, it can set rowstride_alignment_hint.
   This value must be a multiple of 4 and in the range RA_MIN <-> RA_MAX, otherwise it will be
   ignored. If the values is accepted, it will be reset to zero, and the current rowstride_alignment
   is always maintiened in threadvar rowstride_alignment.
   The memory blocks are allocated from the bigblock allocator if possible, and are aligned to
   either DEF_ALIGN or the machine cacheline size if known.
*/

boolean create_empty_pixel_data(weed_layer_t *layer, boolean black_fill, boolean may_contig) {
  int palette = weed_layer_get_palette(layer);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int rowstride, *rowstrides;
  int *fixed_rs = NULL;

  uint32_t pflags;
  int clamping = WEED_YUV_CLAMPING_CLAMPED;
  boolean compact = FALSE;

  uint8_t *pixel_data = NULL;
  uint8_t *memblock;
  uint8_t **pd_array;

  unsigned char black[6] = {0, 0, 0, 255, 255, 255};
  unsigned char yuv_black[6] = {16, 128, 128, 255, 255, 255};
  float blackf[4] = {0., 0., 0., 1.};

  size_t framesize, framesize2;

  int rowstride_alignment = 0;
  int align = HW_ALIGNMENT;
  int max_ra = RA_MAX;

  if (HW_ALIGNMENT > max_ra) max_ra = HW_ALIGNMENT;
  else max_ra -= max_ra % HW_ALIGNMENT;

  if (!weed_plant_has_leaf(layer, WEED_LEAF_NATURAL_SIZE)) {
    int nsize[2];
    // set "natural_size" in case a filter needs it (width is in PIXELS)
    nsize[0] = width * weed_palette_get_pixels_per_macropixel(palette);
    nsize[1] = height;
    weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
  }

  if (width <= 0 || height <= 0) return FALSE;

  if (weed_leaf_get_flags(layer, WEED_LEAF_ROWSTRIDES) & LIVES_FLAG_MAINTAIN_VALUE) {
    /// force use of fixed rowstrides, eg. decoder plugin
    fixed_rs = weed_layer_get_rowstrides(layer, NULL);
  } else {
    rowstride_alignment = THREADVAR(rowstride_alignment_hint);
    if (rowstride_alignment < RA_MIN || (rowstride_alignment & 3))
      rowstride_alignment = 0;
    if (!rowstride_alignment) rowstride_alignment = RS_ALIGN_DEF;
    if (rowstride_alignment > max_ra) rowstride_alignment = max_ra;
    THREADVAR(rowstride_alignment) = rowstride_alignment;
    THREADVAR(rowstride_alignment_hint) = 0;
  }
  if (weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC)) {
    weed_leaf_delete(layer, LIVES_LEAF_BBLOCKALLOC);
  }
  if (weed_plant_has_leaf(layer, LIVES_LEAF_PIXBUF_SRC)) {
    weed_leaf_delete(layer, LIVES_LEAF_PIXBUF_SRC);
  }
  if (weed_plant_has_leaf(layer, LIVES_LEAF_SURFACE_SRC)) {
    weed_leaf_delete(layer, LIVES_LEAF_SURFACE_SRC);
  }
  pflags = weed_leaf_get_flags(layer, WEED_LEAF_PIXEL_DATA);
  weed_leaf_set_flags(layer, WEED_LEAF_PIXEL_DATA, pflags & ~LIVES_FLAG_MAINTAIN_VALUE);

  if (black_fill) {
    if (weed_plant_has_leaf(layer, WEED_LEAF_YUV_CLAMPING))
      clamping = weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL);
    if (clamping != WEED_YUV_CLAMPING_CLAMPED) yuv_black[0] = 0;
  }

  switch (palette) {
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 4;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else
#endif
      pixel_data = lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    if (black_fill) {
      if (palette == WEED_PALETTE_ARGB32) {
        black[3] = black[0];
        black[0] = 255;
      }
      fill_plane(pixel_data, 4, width, height, rowstride, black);
    }
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    break;

  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 3;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
    //g_print("fs is %ld, rs %d. h %d, ra %d\n", framesize, rowstride, height, rowstride_alignment);
#ifdef USE_BIGBLOCKS
    //g_print("Ttry bb with %ld\n", framesize);
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else {
#endif
      g_print("fail %d\n", align);
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    }
    if (!pixel_data) return FALSE;
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_YUV888:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 3;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) fill_plane(pixel_data, 3, width, height, rowstride, yuv_black);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    break;

  case WEED_PALETTE_YUVA8888:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 4;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) fill_plane(pixel_data, 4, width, height, rowstride, yuv_black);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    break;

  case WEED_PALETTE_UYVY:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 4;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) {
      yuv_black[1] = yuv_black[3] = yuv_black[0];
      yuv_black[0] = yuv_black[2];
      fill_plane(pixel_data, 4, width, height, rowstride, yuv_black);
    }
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    break;

  case WEED_PALETTE_YUYV:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 4;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize)))
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) {
      yuv_black[2] = yuv_black[0];
      black[3] = yuv_black[1];
      fill_plane(pixel_data, 4, width, height, rowstride, yuv_black);
    }
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    width = (width >> 1) << 1;
    height = (height >> 1) << 1;
    weed_layer_set_size(layer, width, height);
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
    rowstrides = (int *)lives_malloc(sizint * 3);
    if (fixed_rs) {
      rowstrides[0] = fixed_rs[0];
      rowstride = rowstrides[1] = fixed_rs[1];
      rowstrides[2] = fixed_rs[2];
    } else {
      rowstrides[0] = rowstride;
      rowstride >>= 1;
      rowstrides[1] = rowstrides[2] = rowstride;
    }
    //if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    framesize2 = rowstride * (height >> 1);
    weed_set_int_array(layer, WEED_LEAF_ROWSTRIDES, 3, rowstrides);
    lives_free(rowstrides);

    pd_array = (uint8_t **)lives_malloc(3 * sizeof(uint8_t *));

    if (!may_contig) {
      weed_leaf_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
      pd_array[0] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[0]) {
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[1] = (uint8_t *)lives_calloc_safety(framesize2, align);
      if (!pd_array[1]) {
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[2] = (uint8_t *)lives_calloc_safety(framesize2, align);
      if (!pd_array[2]) {
        lives_free(pd_array[1]);
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
    } else {
      weed_set_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
#ifdef USE_BIGBLOCKS
      if ((memblock = calloc_bigblock(framesize + framesize2 * 2))) {
        weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
      } else
#endif
        memblock = (uint8_t *)lives_calloc_safety(framesize + framesize2 * 2, align);
      if (!memblock) return FALSE;
      pd_array[0] = (uint8_t *)memblock;
      pd_array[1] = (uint8_t *)(memblock + framesize);
      pd_array[2] = (uint8_t *)(memblock + framesize + framesize2);
    }
    if (black_fill) {
      if (yuv_black[0] != 0) lives_memset(pd_array[0], yuv_black[0], framesize);
      if (may_contig) {
        lives_memset(pd_array[1], yuv_black[1], framesize2 * 2); // fill both planes
      } else {
        lives_memset(pd_array[1], yuv_black[1], framesize2);
        lives_memset(pd_array[2], yuv_black[2], framesize2);
      }
    }

    weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 3, (void **)pd_array);
    lives_free(pd_array);
    break;

  case WEED_PALETTE_YUV422P:
    width = (width >> 1) << 1;
    weed_layer_set_width(layer, width);
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
    rowstrides = (int *)lives_malloc(sizint * 3);
    if (fixed_rs) rowstride = fixed_rs[1];
    else {
      rowstrides[0] = rowstride;
      rowstride = width >> 1;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize2 = rowstride * height;
    if (fixed_rs) {
      rowstrides[0] = fixed_rs[0];
      rowstrides[1] = fixed_rs[1];
      rowstrides[2] = fixed_rs[2];
    } else  rowstrides[1] = rowstrides[2] = rowstride;
    weed_set_int_array(layer, WEED_LEAF_ROWSTRIDES, 3, rowstrides);
    lives_free(rowstrides);
    pd_array = (uint8_t **)lives_malloc(3 * sizeof(uint8_t *));

    if (!may_contig) {
      weed_leaf_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
      pd_array[0] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[0]) {
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[1] = (uint8_t *)lives_calloc_safety(framesize2, align);
      if (!pd_array[1]) {
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[2] = (uint8_t *)lives_calloc_safety(framesize2, align);
      if (!pd_array[2]) {
        lives_free(pd_array[1]);
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
    } else {
      weed_set_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
#ifdef USE_BIGBLOCKS
      if ((memblock = calloc_bigblock(framesize + framesize2 * 2))) {
        weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
      } else
#endif
        memblock = (uint8_t *)lives_calloc_safety(framesize + framesize2 * 2, align);
      if (!memblock) return FALSE;
      pd_array[0] = (uint8_t *)memblock;
      pd_array[1] = (uint8_t *)(memblock + framesize);
      pd_array[2] = (uint8_t *)(memblock + framesize + framesize2);
    }
    if (black_fill) {
      if (yuv_black[0] != 0) lives_memset(pd_array[0], yuv_black[0], framesize);
      if (may_contig) {
        lives_memset(pd_array[1], yuv_black[1], framesize2 * 2);
      } else {
        lives_memset(pd_array[1], yuv_black[1], framesize2);
        lives_memset(pd_array[2], yuv_black[2], framesize2);
      }
    }
    weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 3, (void **)pd_array);
    lives_free(pd_array);
    break;

  case WEED_PALETTE_YUV444P:
    if (!fixed_rs) {
      rowstride = width;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    rowstrides = (int *)lives_malloc(sizint * 3);
    if (fixed_rs) {
      rowstride = rowstrides[0] = fixed_rs[0];
      rowstrides[1] = fixed_rs[1];
      rowstrides[2] = fixed_rs[2];
    } else rowstrides[0] = rowstrides[1] = rowstrides[2] = rowstride;
    weed_set_int_array(layer, WEED_LEAF_ROWSTRIDES, 3, rowstrides);
    lives_free(rowstrides);
    pd_array = (uint8_t **)lives_malloc(3 * sizeof(uint8_t *));
    framesize = rowstride * height;

    if (!may_contig) {
      weed_leaf_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
      pd_array[0] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[0]) {
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[1] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[1]) {
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[2] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[2]) {
        lives_free(pd_array[1]);
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
    } else {
      weed_set_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
#ifdef USE_BIGBLOCKS
      if ((memblock = calloc_bigblock(framesize * 3))) {
        weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
      } else
#endif
        memblock = (uint8_t *)lives_calloc_safety(framesize * 3, align);
      if (!memblock) return FALSE;
      pd_array[0] = memblock;
      pd_array[1] = memblock + framesize;
      pd_array[2] = memblock + framesize * 2;
    }
    if (black_fill) {
      if (yuv_black[0] != 0) lives_memset(pd_array[0], yuv_black[0], framesize);
      if (may_contig) {
        lives_memset(pd_array[1], yuv_black[1], framesize * 2);
      } else {
        lives_memset(pd_array[1], yuv_black[1], framesize);
        lives_memset(pd_array[2], yuv_black[2], framesize);
      }
    }
    weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 3, (void **)pd_array);
    lives_free(pd_array);
    break;

  case WEED_PALETTE_YUVA4444P:
    if (!fixed_rs) {
      rowstride = width;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    rowstrides = (int *)lives_malloc(sizint * 4);
    if (fixed_rs) {
      rowstride = rowstrides[0] = fixed_rs[0];
      rowstrides[1] = fixed_rs[1];
      rowstrides[2] = fixed_rs[2];
      rowstrides[3] = fixed_rs[3];
    } else rowstrides[0] = rowstrides[1] = rowstrides[2] = rowstrides[3] = rowstride;
    weed_set_int_array(layer, WEED_LEAF_ROWSTRIDES, 4, rowstrides);
    lives_free(rowstrides);
    pd_array = (uint8_t **)lives_malloc(4 * sizeof(uint8_t *));
    framesize = rowstride * height;

    if (!may_contig) {
      weed_leaf_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
      pd_array[0] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[0]) {
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[1] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[1]) {
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[2] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[2]) {
        lives_free(pd_array[1]);
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
      pd_array[3] = (uint8_t *)lives_calloc_safety(framesize, align);
      if (!pd_array[3]) {
        lives_free(pd_array[2]);
        lives_free(pd_array[1]);
        lives_free(pd_array[0]);
        lives_free(pd_array);
        return FALSE;
      }
    } else {
      weed_set_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
#ifdef USE_BIGBLOCKS
      if ((memblock = calloc_bigblock(framesize * 4))) {
        weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
      } else
#endif
        memblock = (uint8_t *)lives_calloc_safety(framesize * 4, align);
      if (!memblock) return FALSE;
      pd_array[0] = memblock;
      pd_array[1] = memblock + framesize;
      pd_array[2] = memblock + framesize * 2;
      pd_array[3] = memblock + framesize * 3;
    }
    if (black_fill) {
      if (yuv_black[0] != 0) {
        lives_memset(pd_array[0], yuv_black[0], framesize * 2);
      }
      if (may_contig) {
        lives_memset(pd_array[1], yuv_black[1], framesize * 2);
      } else {
        lives_memset(pd_array[1], yuv_black[1], framesize);
        lives_memset(pd_array[2], yuv_black[2], framesize);
      }
      lives_memset(pd_array[3], 255, framesize);
    }
    weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 4, (void **)pd_array);
    lives_free(pd_array);
    break;

  case WEED_PALETTE_YUV411:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 6; // a macro-pixel is 6 bytes, and contains 4 real pixels
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    weed_layer_set_width(layer, width);
    framesize = rowstride * height;
    framesize = rowstride * height;
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(framesize))) {
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    } else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) {
      yuv_black[3] = yuv_black[1];
      yuv_black[1] = yuv_black[2] = yuv_black[4] = yuv_black[5] = yuv_black[0];
      yuv_black[0] = yuv_black[3];
      fill_plane(pixel_data, 6, width, height, rowstride, black);
    }
    if (!pixel_data) return FALSE;
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_RGBFLOAT:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 3 * sizeof(float);
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    pixel_data = (uint8_t *)lives_calloc_safety(rowstride * height, align);
    if (!pixel_data) return FALSE;
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_RGBAFLOAT:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * 4 * sizeof(float);
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(rowstride * height))) {
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    } else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(rowstride * height, align);
    if (black_fill) {
      fill_plane(pixel_data, 4 * sizeof(float), width, height, rowstride, (uint8_t *)blackf);
    }
    if (!pixel_data) return FALSE;
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_AFLOAT:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width * sizeof(float);
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
#ifdef USE_BIGBLOCKS
    if ((pixel_data = calloc_bigblock(width * height))) {
      weed_set_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
    } else
#endif
      pixel_data = (uint8_t *)lives_calloc_safety(width * height, align);
    if (!pixel_data) return FALSE;
    if (black_fill) {
      blackf[0] = 1.;
      fill_plane(pixel_data, sizeof(float), width, height, rowstride, (uint8_t *)blackf);
    }
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_A8:
    if (fixed_rs) rowstride = fixed_rs[0];
    else {
      rowstride = width;
      if (!compact) rowstride = ALIGN_CEIL(rowstride, rowstride_alignment);
    }
    framesize = rowstride * height;
    pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    if (black_fill) {
      lives_memset(pixel_data, 255, rowstride * height);
    }
    if (!pixel_data) return FALSE;
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  case WEED_PALETTE_A1:
    if (fixed_rs) rowstride = fixed_rs[0];
    else rowstride = (width + 7) >> 3;
    framesize = rowstride * height;
    pixel_data = (uint8_t *)lives_calloc_safety(framesize, align);
    if (!pixel_data) return FALSE;
    lives_memset(pixel_data, 255, rowstride * height);
    weed_set_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, pixel_data);
    weed_set_int_value(layer, WEED_LEAF_ROWSTRIDES, rowstride);
    break;

  default:
    lives_printerr("Warning: asked to create empty pixel_data for palette %d !\n", palette);
    break_me("create_empty_pixel_data w. unknown pal");
  }
  return TRUE;
}


/**
   @brief fills layer with default values.

   If either width or height are zero, then dimensions will be taken from the layer or
   defaults used
   if layer has a palette set, that will be maintained, else it will be set to target_palette
   if targette palette is WEED_PALETTE_NONE then default will be set depending on image_ext
   if this is "jpg" then it will be RGB24, otherwise RGBA32
   finally we create the pixel data for layer */
weed_layer_t *create_blank_layer(weed_layer_t *layer, const char *image_ext,
                                 int width, int height, int target_palette) {
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
  if (!create_empty_pixel_data(layer, TRUE, TRUE)) weed_layer_nullify_pixel_data(layer);
  if (!weed_plant_has_leaf(layer, WEED_LEAF_GAMMA_TYPE)) {
    int clip = lives_layer_get_clip(layer);
    if (clip && IS_VALID_CLIP(clip))
      weed_layer_set_gamma(layer, mainw->files[clip]->gamma_type);
    else
      weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);
  }
  return layer;
}


LIVES_GLOBAL_INLINE boolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array) {
  // returns TRUE if the rowstrides differ
  if (!n1_array || !n2_array || n1 != n2) return TRUE;
  for (int i = 0; i < n1; i++) if (n1_array[i] != n2_array[i]) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_new(int layer_type) {
  weed_layer_t *layer = weed_plant_new(WEED_PLANT_LAYER);
  weed_set_int_value(layer, WEED_LEAF_LAYER_TYPE, layer_type);
  weed_add_refcounter(layer);
  return layer;
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
  weed_leaf_delete(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS);
  weed_leaf_delete(layer, LIVES_LEAF_PIXBUF_SRC);
  weed_leaf_delete(layer, LIVES_LEAF_SURFACE_SRC);
  weed_leaf_delete(layer, LIVES_LEAF_BBLOCKALLOC);
  weed_leaf_delete(layer, WEED_LEAF_NATURAL_SIZE);
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


LIVES_GLOBAL_INLINE void lives_layer_set_frame(weed_layer_t *layer, frames_t frame) {
  // TODO -> int64
  weed_set_int_value(layer, WEED_LEAF_FRAME, frame);
}


LIVES_GLOBAL_INLINE void lives_layer_set_clip(weed_layer_t *layer, int clip) {
  weed_set_int_value(layer, WEED_LEAF_CLIP, clip);
}


LIVES_GLOBAL_INLINE weed_layer_t *lives_layer_new_for_frame(int clip, frames_t frame) {
  // create a layer ready to receive a frame from a clip
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  lives_layer_set_clip(layer, clip);
  lives_layer_set_frame(layer, frame);
  return layer;
}


// returns TRUE on success
boolean copy_pixel_data(weed_layer_t *layer, weed_layer_t *old_layer, size_t alignment) {
  // copy (deep) old_layer -> layer

  int numplanes, xheight, xwidth;
  int *orowstrides, *rowstrides;;
  void **pixel_data, **npixel_data;
  int pal = weed_layer_get_palette(layer);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int i = numplanes, j;
  boolean newdata = FALSE;
  double psize = pixel_size(pal);

  if (alignment != 0 && !old_layer) {
    rowstrides = weed_layer_get_rowstrides(layer, &i);
    while (i > 0) if (rowstrides[--i] % alignment != 0) i = -1;
    lives_free(rowstrides);
    if (i == 0) return TRUE;
  }

  if (!old_layer) {
    newdata = TRUE;
    old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_layer_copy(old_layer, layer);
  }

  pixel_data = weed_layer_get_pixel_data_planar(old_layer, &numplanes);
  if (!pixel_data || !pixel_data[0]) {
    if (newdata) {
      weed_layer_nullify_pixel_data(old_layer);
      weed_layer_free(old_layer);
    }
    return FALSE;
  }

  orowstrides = weed_layer_get_rowstrides(old_layer, &numplanes);
  weed_layer_nullify_pixel_data(layer);

  if (alignment != 0) THREADVAR(rowstride_alignment_hint) = alignment;

  if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
    if (newdata) {
      weed_layer_copy(layer, old_layer);
      weed_layer_nullify_pixel_data(old_layer);
      weed_layer_free(old_layer);
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

  if (newdata) weed_layer_free(old_layer);
  lives_freep((void **)&npixel_data);
  lives_freep((void **)&pixel_data);
  lives_freep((void **)&orowstrides);
  lives_freep((void **)&rowstrides);
  return TRUE;
}


/** (un)premultply alpha using a lookup table

    if un is FALSE we go the other way, and do a pre-multiplication */
void alpha_unpremult(weed_layer_t *layer, boolean un) {
  /// this is only used when going from palette with alpha to one without
  int error;
  int aoffs, coffs, psize, psizel, widthx;
  int alpha;
  int flags = 0;
  int width = weed_get_int_value(layer, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(layer, WEED_LEAF_HEIGHT, NULL);
  int rowstride = weed_get_int_value(layer, WEED_LEAF_ROWSTRIDES, NULL);
  int pal = weed_get_int_value(layer, WEED_LEAF_CURRENT_PALETTE, NULL);

  int *rows;

  unsigned char *ptr;
  unsigned char **ptrp;

  boolean clamped;

  int i, j, p;

  if (!unal_inited) init_unal();

  if (weed_plant_has_leaf(layer, WEED_LEAF_YUV_CLAMPING))
    clamped = (weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, &error) == WEED_YUV_CLAMPING_CLAMPED);
  else clamped = TRUE;

  switch (pal) {
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    clamped = FALSE;
  case WEED_PALETTE_YUVA8888:
    widthx = width * 4;
    psize = 4;
    psizel = 3;
    coffs = 0;
    aoffs = 3;
    break;
  case WEED_PALETTE_ARGB32:
    widthx = width * 4;
    psize = 4;
    psizel = 4;
    coffs = 1;
    aoffs = 0;
    clamped = FALSE;
    break;
  case WEED_PALETTE_YUVA4444P:
    /// special case - planar with alpha
    ptrp = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    rows = weed_layer_get_rowstrides(layer, NULL);

    if (!clamped) {
      if (un) {
        for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
            alpha = ptrp[3][j];
            for (p = 0; p < 3; p++) {
              ptrp[p][j] = unal[alpha][ptrp[p][j]];
            }
          }
          for (p = 0; p < 4; p++) {
            ptrp[p] += rows[p];
          }
        }
      } else {
        for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
            alpha = ptrp[3][j];
            for (p = 0; p < 3; p++) {
              ptrp[p][j] = al[alpha][ptrp[p][j]];
            }
          }
          for (p = 0; p < 4; p++) {
            ptrp[p] += rows[p];
          }
        }
      }
    } else {
      if (un) {
        for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
            alpha = ptrp[3][j];
            ptrp[0][j] = unalcy[alpha][ptrp[0][j]];
            ptrp[1][j] = unalcuv[alpha][ptrp[0][j]];
            ptrp[2][j] = unalcuv[alpha][ptrp[0][j]];
          }
          for (p = 0; p < 4; p++) {
            ptrp[p] += rows[p];
          }
        }
      } else {
        for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
            alpha = ptrp[3][j];
            ptrp[0][j] = alcy[alpha][ptrp[0][j]];
            ptrp[1][j] = alcuv[alpha][ptrp[0][j]];
            ptrp[2][j] = alcuv[alpha][ptrp[0][j]];
          }
          for (p = 0; p < 4; p++) {
            ptrp[p] += rows[p];
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*

    return;
  default:
    return;
  }

  ptr = (unsigned char *)weed_layer_get_pixel_data(layer);

  if (!clamped) {
    if (un) {
      for (i = 0; i < height; i++) {
        for (j = 0; j < widthx; j += psize) {
          alpha = ptr[j + aoffs];
          for (p = coffs; p < psizel; p++) {
            ptr[j + p] = unal[alpha][ptr[j + p]];
          }
        }
        ptr += rowstride;
      }
    } else {
      for (i = 0; i < height; i++) {
        for (j = 0; j < widthx; j += psize) {
          alpha = ptr[j + aoffs];
          for (p = coffs; p < psizel; p++) {
            ptr[j + p] = al[alpha][ptr[j + p]];
          }
        }
        ptr += rowstride;
      }
    }
  } else {
    /// unclamped YUVA8888 (packed)
    if (un) {
      for (i = 0; i < height; i++) {
        for (j = 0; j < widthx; j += psize) {
          alpha = ptr[j + 3];
          ptr[j] = unalcy[alpha][ptr[j]];
          ptr[j + 1] = unalcuv[alpha][ptr[j]];
          ptr[j + 2] = unalcuv[alpha][ptr[j]];
        }
        ptr += rowstride;
      }
    } else {
      for (i = 0; i < height; i++) {
        for (j = 0; j < widthx; j += psize) {
          alpha = ptr[j + 3];
          ptr[j] = alcy[alpha][ptr[j]];
          ptr[j + 1] = alcuv[alpha][ptr[j]];
          ptr[j + 2] = alcuv[alpha][ptr[j]];
        }
        ptr += rowstride;
      }
    }
  }

  flags = weed_layer_get_flags(layer);

  if (!un) flags |= WEED_LAYER_ALPHA_PREMULT;
  else if (flags & WEED_LAYER_ALPHA_PREMULT) flags ^= WEED_LAYER_ALPHA_PREMULT;

  if (flags == 0) weed_leaf_delete(layer, LIVES_LEAF_HOST_FLAGS);
  else weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
}


static void swap_chroma_planes(weed_layer_t *layer) {
  int nplanes;
  void **pd_array = weed_layer_get_pixel_data_planar(layer, &nplanes);
  int *rowstrides, rtmp;
  uint8_t *tmp;
  if (nplanes < 3) return;
  tmp = pd_array[1];
  pd_array[1] = pd_array[2];
  pd_array[2] = tmp;
  weed_layer_set_pixel_data_planar(layer, pd_array, nplanes);
  lives_free(pd_array);
  rowstrides = weed_layer_get_rowstrides(layer, NULL);
  rtmp = rowstrides[1];
  rowstrides[1] = rowstrides[2];
  rowstrides[2] = rtmp;
}


LIVES_LOCAL_INLINE boolean can_inline_gamma(int inpl, int opal) {
  // TODO: rgb <-> bgra, bgr <-> rgba,
  if (inpl == WEED_PALETTE_YUV420P || inpl == WEED_PALETTE_YUV420P || inpl == WEED_PALETTE_YUV420P) {
    if (opal == WEED_PALETTE_RGB24 || opal == WEED_PALETTE_BGR24 || opal == WEED_PALETTE_RGBA32
        || opal == WEED_PALETTE_BGRA32 || opal == WEED_PALETTE_ARGB32) return TRUE;
  }
  if (opal == WEED_PALETTE_UYVY || opal == WEED_PALETTE_YUYV) {
    if (inpl == WEED_PALETTE_RGB24 || inpl == WEED_PALETTE_RGBA32
        || inpl == WEED_PALETTE_BGR24 || inpl == WEED_PALETTE_BGRA32
        || inpl == WEED_PALETTE_ARGB32
       ) return TRUE;
  }

  if ((inpl == WEED_PALETTE_RGB24 && opal == WEED_PALETTE_BGR24) || (inpl == WEED_PALETTE_BGR24
      && opal == WEED_PALETTE_RGB24)) return TRUE;
  if ((inpl == WEED_PALETTE_RGB24 && opal == WEED_PALETTE_RGBA32) || (inpl == WEED_PALETTE_BGR24
      && opal == WEED_PALETTE_BGRA32)) return TRUE;
  if ((inpl == WEED_PALETTE_RGBA32 && opal == WEED_PALETTE_RGB24) || (inpl == WEED_PALETTE_BGRA32
      && opal == WEED_PALETTE_BGR24)) return TRUE;

  return FALSE;
}


/**
   @brief convert the palette of a layer

   convert to/from the 5 non-float RGB palettes and 10 YUV palettes
   giving a total of 15*14=210 conversions

   in addition YUV can be converted from clamped to unclamped and vice-versa

   chroma sub and supersampling is implemented, and threading is used wherever possible

   all conversions are performed via lookup tables

   NOTE - if converting to YUV411, we cut pixels so (RGB) width is divisible by 4
   if converting to YUV420 or YVU420, we cut pixels so (RGB) width is divisible by 2
   if converting to YUV420 or YVU420, we cut pixels so height is divisible by 2

   returns FALSE if the palette conversion fails or if layer is NULL

   - original palette pixel_data is free()d (unless converting between YUV420 and YVU420, there the u and v pointers are
   simply swapped).

   current limitations:
   - chroma is assumed centred between luma for input and output
   - bt709 yuv is only implemented for conversions to / from rgb palettes and yuv420 / yvu420
   - rowstride values may be ignored for UYVY, YUYV and YUV411 planar palettes.
   - RGB float palettes not yet implemented

*/
boolean convert_layer_palette_full(weed_layer_t *layer, int outpl, int oclamping, int osampling, int osubspace, int tgamma) {
  // TODO: allow plugin candidates/delegates
  weed_layer_t *orig_layer;
  uint8_t *gusrc = NULL, **gusrc_array = NULL, *gudest = NULL, **gudest_array = NULL, *tmp;
  int width, height, orowstride, irowstride, *istrides, *ostrides = NULL;
  int nplanes;
  int error, inpl, flags = 0;
  int isampling, isubspace;
  int new_gamma_type = WEED_GAMMA_UNKNOWN, gamma_type = WEED_GAMMA_UNKNOWN;
  int iclamping;
  boolean contig = FALSE;

  if (!layer || !weed_layer_get_pixel_data(layer)) return FALSE;

  inpl = weed_layer_get_palette(layer);

  if (weed_plant_has_leaf(layer, WEED_LEAF_YUV_SAMPLING))
    isampling = weed_layer_get_yuv_sampling(layer);
  else isampling = WEED_YUV_SAMPLING_DEFAULT;

  if (weed_plant_has_leaf(layer, WEED_LEAF_YUV_CLAMPING))
    iclamping = weed_layer_get_yuv_clamping(layer);
  else iclamping = oclamping;

  if (weed_plant_has_leaf(layer, WEED_LEAF_YUV_SUBSPACE))
    isubspace = weed_layer_get_yuv_subspace(layer);
  else isubspace = WEED_YUV_SUBSPACE_YUV;

  width = weed_layer_get_width(layer);
  height = weed_layer_get_height(layer);

  //      #define DEBUG_PCONV
#ifdef DEBUG_PCONV
  g_print("converting %d X %d palette %s(%s) to %s(%s)\n", width, height, weed_palette_get_name(inpl),
          weed_yuv_clamping_get_name(iclamping),
          weed_palette_get_name(outpl),
          weed_yuv_clamping_get_name(oclamping));
#endif

  istrides = weed_layer_get_rowstrides(layer, &nplanes);
  if (!istrides) return FALSE;

  if (weed_palette_is_yuv(inpl) && weed_palette_is_yuv(outpl) && (iclamping != oclamping || isubspace != osubspace)) {
    if (isubspace == osubspace) {
#ifdef DEBUG_PCONV
      lives_printerr("converting clamping %d to %d\n", iclamping, oclamping);
#endif
      switch_yuv_clamping_and_subspace(layer, oclamping, osubspace);
      iclamping = oclamping;
    } else {
      // convert first to RGB(A)
      if (weed_palette_has_alpha(inpl)) {
        if (!convert_layer_palette(layer, WEED_PALETTE_RGBA32, 0)) goto memfail;
      } else {
        if (!convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) goto memfail;
      }
      inpl = weed_layer_get_palette(layer);
      isubspace = osubspace;
      isampling = osampling;
      iclamping = oclamping;
#ifdef DEBUG_PCONV
      g_print("subspace conversion via palette %s\n", weed_palette_get_name(inpl));
#endif
    }
  }

  if (inpl == outpl) {
#ifdef DEBUG_PCONV
    lives_printerr("not converting palette\n");
#endif
    if (!weed_palette_is_yuv(inpl) || (isampling == osampling &&
                                       (isubspace == osubspace || (osubspace != WEED_YUV_SUBSPACE_BT709)))) {
      if (inpl == WEED_PALETTE_YUV420P && ((isampling == WEED_YUV_SAMPLING_JPEG
                                            && osampling == WEED_YUV_SAMPLING_MPEG) ||
                                           (isampling == WEED_YUV_SAMPLING_MPEG && osampling == WEED_YUV_SAMPLING_JPEG))) {
        switch_yuv_sampling(layer);
      } else {
        char *tmp2 = lives_strdup_printf("Switch sampling types (%d %d) or subspace(%d %d): (%d) conversion not yet written !\n",
                                         isampling, osampling, isubspace, osubspace, inpl);
        LIVES_DEBUG(tmp2);
        lives_free(tmp2);
      }
    }
    lives_free(istrides);
    return TRUE;
  }

  flags = weed_layer_get_flags(layer);

  if (prefs->alpha_post) {
    if ((flags & WEED_LAYER_ALPHA_PREMULT) &&
        (weed_palette_has_alpha(inpl) && !(weed_palette_has_alpha(outpl)))) {
      // if we have pre-multiplied alpha, remove it when removing alpha channel
      alpha_unpremult(layer, TRUE);
    }
  } else {
    if (!weed_palette_has_alpha(inpl) && weed_palette_has_alpha(outpl)) {
      flags |= WEED_LAYER_ALPHA_PREMULT;
      weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
    }
  }

  if (weed_palette_has_alpha(inpl) && !(weed_palette_has_alpha(outpl)) && (flags & WEED_LAYER_ALPHA_PREMULT)) {
    flags ^= WEED_LAYER_ALPHA_PREMULT;
    if (flags == 0) weed_leaf_delete(layer, LIVES_LEAF_HOST_FLAGS);
    else weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
  }

  if (weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, &error) == WEED_TRUE)
    contig = TRUE;

  width = weed_layer_get_width(layer);
  height = weed_layer_get_height(layer);

  if (prefs->apply_gamma) {
    // gamma correction
    gamma_type = weed_layer_get_gamma(layer);
    if (gamma_type != WEED_GAMMA_UNKNOWN) {
      if (tgamma != WEED_GAMMA_UNKNOWN) {
        new_gamma_type = tgamma;
      } else {
        if (weed_palette_is_rgb(inpl) && !weed_palette_is_rgb(outpl)) {
          // gamma correction
          if (osubspace == WEED_YUV_SUBSPACE_BT709) {
            new_gamma_type = WEED_GAMMA_BT709;
          } else new_gamma_type = WEED_GAMMA_SRGB;
        } else new_gamma_type = gamma_type;
      }
      if (weed_palette_is_rgb(inpl) && !weed_palette_is_rgb(outpl)) {
        if (!can_inline_gamma(inpl, outpl)) {
          gamma_convert_layer(new_gamma_type, layer);
          gamma_type = new_gamma_type = weed_layer_get_gamma(layer);
        }
      }
    }
  }

  lives_free(istrides);
  istrides = weed_layer_get_rowstrides(layer, &nplanes);
  if (!istrides) return FALSE;

  irowstride = istrides[0];
  weed_layer_set_palette(layer, outpl);
  weed_layer_set_size(layer, width * weed_palette_get_pixels_per_macropixel(inpl)
                      / weed_palette_get_pixels_per_macropixel(outpl), height);
  // TODO: rowstrides for uyvy, yuyv, 422P, 411

  /// if V plane is before U, swap the pointers
#ifdef WEED_ADVANCED_PALETTES
  if (!weed_palette_is_sane(inpl) || !weed_palette_is_sane(outpl)) {
    if (!weed_palette_is_sane(outpl)) g_print("BAD pal %d\n", outpl);
    if (!weed_palette_is_sane(inpl)) g_print("BAD pal %d\n", inpl);
    return FALSE;
  }
  if (get_advanced_palette(inpl)->chantype[1] == WEED_VCHAN_V) swap_chroma_planes(layer);
#else
  if (inpl == WEED_PALETTE_YVU420P) swap_chroma_planes(layer);
#endif

  orig_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  weed_layer_copy(orig_layer, layer);

#ifdef WEED_ADVANCED_PALETTES
  // all RGB -> RGB conversions are now handled here
  if (weed_palette_is_rgb(inpl) && weed_palette_is_rgb(outpl)) {
    gusrc = weed_layer_get_pixel_data(layer);
    if (!weed_palette_has_alpha_first(inpl)) {
      if (!weed_palette_has_alpha_last(inpl)) {
        if (!weed_palette_has_alpha_first(outpl)) {
          if (!weed_palette_has_alpha_last(outpl)) {
            convert_swap3_frame(gusrc, width, height, irowstride, irowstride, gusrc,
                                gamma_type == new_gamma_type ? NULL :
                                create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          } else {
            // add post
            if (weed_palettes_rbswapped(inpl, outpl)) {
              if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
              orowstride = weed_layer_get_rowstride(layer);
              gudest = weed_layer_get_pixel_data(layer);
              convert_swap3addpost_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                         -USE_THREADS);
            } else {
              if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
              orowstride = weed_layer_get_rowstride(layer);
              gudest = weed_layer_get_pixel_data(layer);
              convert_addpost_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                    gamma_type == new_gamma_type ? NULL :
                                    create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                    -USE_THREADS);
            }
          }
        } else {
          /// add pre
          if (weed_palettes_rbswapped(inpl, outpl)) {
            if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
            orowstride = weed_layer_get_rowstride(layer);
            gudest = weed_layer_get_pixel_data(layer);
            convert_swap3addpre_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                      -USE_THREADS);
          } else {
            if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
            orowstride = weed_layer_get_rowstride(layer);
            gudest = weed_layer_get_pixel_data(layer);
            convert_addpre_frame(gusrc, width, height, irowstride, orowstride, gudest, -USE_THREADS);
          }
        }
      } else {
        /// inpl has post
        if (!weed_palette_has_alpha_first(outpl)) {
          if (!weed_palette_has_alpha_last(outpl)) {
            if (weed_palettes_rbswapped(inpl, outpl)) {
              if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
              orowstride = weed_layer_get_rowstride(layer);
              gudest = weed_layer_get_pixel_data(layer);
              convert_swap3delpost_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                         -USE_THREADS);
            } else {
              if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
              orowstride = weed_layer_get_rowstride(layer);
              gudest = weed_layer_get_pixel_data(layer);
              convert_delpost_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                    gamma_type == new_gamma_type ? NULL :
                                    create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                    -USE_THREADS);
            }
          } else {
            /// outpl has post
            convert_swap3postalpha_frame(gusrc, width, height, irowstride, -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          }
        } else {
          /// outpl has pre
          if (weed_palettes_rbswapped(inpl, outpl)) {
            convert_swap4_frame(gusrc, width, height, irowstride, irowstride, gusrc, -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          } else {
            convert_swapprepost_frame(gusrc, width, height, irowstride, irowstride, gusrc,
                                      -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          }
        }
      }
    } else {
      // inpl has pre
      if (!weed_palette_has_alpha_first(outpl)) {
        if (!weed_palette_has_alpha_last(outpl)) {
          if (weed_palettes_rbswapped(inpl, outpl)) {
            if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
            orowstride = weed_layer_get_rowstride(layer);
            gudest = weed_layer_get_pixel_data(layer);
            convert_swap3delpre_frame(gusrc, width, height, irowstride, orowstride, gudest,
                                      -USE_THREADS);
          } else {
            if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
            orowstride = weed_layer_get_rowstride(layer);
            gudest = weed_layer_get_pixel_data(layer);
            convert_delpre_frame(gusrc, width, height, irowstride, orowstride, gudest, -USE_THREADS);
          }
        } else {
          /// outpl has post
          if (weed_palettes_rbswapped(inpl, outpl)) {
            convert_swap4_frame(gusrc, width, height, irowstride, irowstride, gusrc, -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          } else {
            convert_swapprepost_frame(gusrc, width, height, irowstride, irowstride, gusrc,
                                      -USE_THREADS);
            weed_layer_nullify_pixel_data(orig_layer);
          }
        }
      } else {
        /// outpl has pre
        convert_swap3prealpha_frame(gusrc, width, height, irowstride, -USE_THREADS);
        weed_layer_nullify_pixel_data(orig_layer);
      }
    }
    goto conv_done;
  }
#endif

  switch (inpl) {
  case WEED_PALETTE_BGR24:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_uyvy_frame(gusrc, width, height, irowstride, orowstride,
                                (uyvy_macropixel *)gudest, FALSE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_yuyv_frame(gusrc, width, height, irowstride, orowstride,
                                (yuyv_macropixel *)gudest, FALSE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, FALSE,
                               oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, TRUE,
                               oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_bgr_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE,
                                  FALSE, WEED_YUV_SAMPLING_DEFAULT, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_bgr_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE,
                                  FALSE, osubspace, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, FALSE,
                                FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, FALSE,
                                TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest,
                                  FALSE, oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",
                     weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_RGBA32:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, TRUE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, TRUE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_rgb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, TRUE,
                                  WEED_YUV_SAMPLING_DEFAULT, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_rgb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, TRUE, osubspace, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, TRUE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, TRUE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, TRUE, oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_RGB24:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, FALSE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, FALSE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_rgb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, FALSE, osubspace, oclamping);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (weed_get_int_value(layer, LIVES_LEAF_PIXEL_BITS, NULL) == 16) width >>= 1;
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      if (weed_get_int_value(layer, LIVES_LEAF_PIXEL_BITS, NULL) == 16) width = -width;
      convert_rgb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE,
                                  FALSE, WEED_YUV_SAMPLING_DEFAULT, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, FALSE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_rgb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, FALSE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_rgb_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, FALSE, oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_BGRA32:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, TRUE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, TRUE, oclamping,
                                gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                -USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_bgr_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, TRUE,
                                  WEED_YUV_SAMPLING_DEFAULT, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_bgr_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, TRUE, osubspace, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, TRUE, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_bgr_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, TRUE, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_bgr_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, TRUE, oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_ARGB32:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_argb_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, oclamping,
                                 gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                 -USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_argb_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, oclamping,
                                 gamma_type == new_gamma_type ? NULL : create_gamma_lut(1.0, gamma_type, new_gamma_type),
                                 -USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_argb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_argb_to_yuv_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_argb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, FALSE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_argb_to_yuvp_frame(gusrc, width, height, irowstride, orowstride, gudest_array, TRUE, oclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_argb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE,
                                   WEED_YUV_SAMPLING_DEFAULT, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_argb_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, osubspace, oclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_argb_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUV444P:
    gusrc_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    switch (outpl) {
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, FALSE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 3, (void **)gudest_array);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_halve_chroma(gusrc_array, width, height, istrides, ostrides, gudest_array, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_rgb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, FALSE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_rgb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, TRUE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_bgr_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, FALSE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_bgr_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, TRUE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_argb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_uyvy_frame(gusrc_array, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_yuyv_frame(gusrc_array, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_combineplanes_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_combineplanes_frame(gusrc_array, width, height, irowstride, orowstride, gudest, FALSE, TRUE);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuvp_to_yuvap_frame(gusrc_array, width, height, irowstride, orowstride, gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuvp_to_yuv420_frame(gusrc_array, width, height, istrides, ostrides, gudest_array, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuvp_to_yuv411_frame(gusrc_array, width, height, irowstride, (yuv411_macropixel *)gudest, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUVA4444P:
    gusrc_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    switch (outpl) {
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, FALSE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_set_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, 3, (void **)gudest_array);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_halve_chroma(gusrc_array, width, height, istrides, ostrides, gudest_array, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 4);
      break;
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_rgb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, FALSE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_rgb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, TRUE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_bgr_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, FALSE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_bgr_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, TRUE, iclamping,
                                      -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_argb_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_uyvy_frame(gusrc_array, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv_planar_to_yuyv_frame(gusrc_array, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_combineplanes_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_combineplanes_frame(gusrc_array, width, height, irowstride, orowstride, gudest, TRUE, TRUE);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuvap_to_yuvp_frame(gusrc_array, width, height, irowstride, orowstride, gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuvp_to_yuv420_frame(gusrc_array, width, height, istrides, ostrides, gudest_array, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuvp_to_yuv411_frame(gusrc_array, width, height, irowstride, (yuv411_macropixel *)gudest, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_UYVY8888:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_YUYV8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_swab_frame(gusrc, width, height, irowstride, orowstride, gudest, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_uyvy_to_yuv422_frame((uyvy_macropixel *)gusrc, width, height, gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      //weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_rgb_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                FALSE, iclamping, isubspace, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_rgb_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                TRUE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_bgr_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                FALSE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_bgr_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                TRUE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_argb_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride,
                                 gudest, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_uyvy_to_yuvp_frame((uyvy_macropixel *)gusrc, width, height, irowstride, ostrides, gudest_array, FALSE);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_uyvy_to_yuvp_frame((uyvy_macropixel *)gusrc, width, height, irowstride, ostrides, gudest_array, TRUE);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_yuv888_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest, FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_uyvy_to_yuv888_frame((uyvy_macropixel *)gusrc, width, height, irowstride, orowstride, gudest, TRUE);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_uyvy_to_yuv420_frame((uyvy_macropixel *)gusrc, width, height, gudest_array, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_uyvy_to_yuv411_frame((uyvy_macropixel *)gusrc, width, height, (yuv411_macropixel *)gudest, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUYV8888:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_swab_frame(gusrc, width, height, irowstride, orowstride, gudest, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuyv_to_yuv422_frame((yuyv_macropixel *)gusrc, width, height, gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_rgb_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                FALSE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_rgb_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                TRUE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_bgr_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                FALSE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_bgr_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest,
                                TRUE, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_argb_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride,
                                 gudest, iclamping, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuyv_to_yuvp_frame((yuyv_macropixel *)gusrc, width, height, irowstride, ostrides, gudest_array, FALSE);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuyv_to_yuvp_frame((yuyv_macropixel *)gusrc, width, height, irowstride, ostrides, gudest_array, TRUE);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_yuv888_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest, FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_yuyv_to_yuv888_frame((yuyv_macropixel *)gusrc, width, height, irowstride, orowstride, gudest, TRUE);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuyv_to_yuv420_frame((yuyv_macropixel *)gusrc, width, height, gudest_array, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuyv_to_yuv411_frame((yuyv_macropixel *)gusrc, width, height, (yuv411_macropixel *)gudest, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUV888:
    // need to check rowstrides (may have been resized)
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_addpost_frame(gusrc, width, height, irowstride, orowstride, gudest, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_splitplanes_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, FALSE);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_splitplanes_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, TRUE);
      break;
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_rgb_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_rgb_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_bgr_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_bgr_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_argb_frame(gusrc, width, height, irowstride, orowstride, gudest, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_get_int_array(layer, WEED_LEAF_ROWSTRIDES, &error);
      convert_yuv888_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,WEED_LEAF_YUV_SAMPLING,osampling);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_get_int_array(layer, WEED_LEAF_ROWSTRIDES, &error);
      convert_yuv888_to_yuv422_frame(gusrc, width, height, irowstride, ostrides, gudest_array, FALSE, iclamping);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, FALSE, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, FALSE, iclamping);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, FALSE);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUVA8888:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_delpost_frame(gusrc, width, height, irowstride, orowstride, gudest, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_splitplanes_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, TRUE);
      break;
    case WEED_PALETTE_YUV444P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_splitplanes_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, FALSE);
      break;
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuva8888_to_rgba_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuva8888_to_rgba_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuva8888_to_bgra_frame(gusrc, width, height, irowstride, orowstride, gudest, TRUE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuva8888_to_bgra_frame(gusrc, width, height, irowstride, orowstride, gudest, FALSE, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuva8888_to_argb_frame(gusrc, width, height, irowstride, orowstride, gudest, iclamping, isampling, -USE_THREADS);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuv888_to_yuv420_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, iclamping);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,WEED_LEAF_YUV_SAMPLING,osampling);
      break;
    case WEED_PALETTE_YUV422P:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_yuv888_to_yuv422_frame(gusrc, width, height, irowstride, ostrides, gudest_array, TRUE, iclamping);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_uyvy_frame(gusrc, width, height, irowstride, orowstride, (uyvy_macropixel *)gudest, TRUE, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_yuyv_frame(gusrc, width, height, irowstride, orowstride, (yuyv_macropixel *)gudest, TRUE, iclamping);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv888_to_yuv411_frame(gusrc, width, height, irowstride, (yuv411_macropixel *)gudest, TRUE);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YVU420P:
  case WEED_PALETTE_YUV420P:
    gusrc_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_rgb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, FALSE, FALSE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_rgb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, TRUE, FALSE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_bgr_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, FALSE, FALSE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_bgr_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, TRUE, FALSE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_argb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, FALSE,
                                    isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420_to_uyvy_frame(gusrc_array, width, height, istrides, orowstride, (uyvy_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420_to_yuyv_frame(gusrc_array, width, height, istrides, orowstride, (yuyv_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUV422P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 3);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_double_chroma(gusrc_array, width >> 1, height >> 1, istrides, ostrides, gudest_array, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_YUV444P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 3);
      orowstride = weed_layer_get_rowstride(layer);
      convert_quad_chroma(gusrc_array, width, height, istrides, orowstride, gudest_array, FALSE, isampling, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_YUVA4444P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 4);
      orowstride = weed_layer_get_rowstride(layer);
      convert_quad_chroma(gusrc_array, width, height, istrides, orowstride, gudest_array, TRUE, isampling, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      if (contig && istrides[1] != istrides[2]) {
        uint8_t *gd0, *gd1, *gd2, *gs0 = gusrc_array[0], *gs1 = gusrc_array[1], *gs2 = gusrc_array[2];
        size_t hwidth = width >> 1;
        size_t ir0 = istrides[0] - width, ir1 = istrides[1] - hwidth, ir2 = istrides[2] - hwidth, or0, or1, or2;
        if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
        ostrides = weed_layer_get_rowstrides(layer, NULL);
        or0 = ostrides[0] - width;
        or1 = ostrides[1] - hwidth;
        or2 = ostrides[2] - hwidth;
        gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
        gd0 = gudest_array[0];
        gd1 = gudest_array[1];
        gd2 = gudest_array[2];
        for (int i = 0; i < height; i++) {
          lives_memcpy(gd0, gs0, width);
          gd0 += or0;
          gs0 += ir0;
          lives_memcpy(gd1, gs1, hwidth);
          gd1 += or1;
          gs1 += ir1;
          lives_memcpy(gd2, gs2, hwidth);
          gd2 += or2;
          gs2 += ir2;
          i++;
          lives_memcpy(gd0, gs0, width);
          gd0 += or0;
          gs0 += ir0;
        }
        break;
      } else {
        /// if we came in as YVU, we got swapped to YUV
        size_t frmsz = istrides[1] * (height >> 1);
        /// swap (again) and both become YVU
        swap_chroma_planes(layer);
        if (!(tmp = lives_calloc_safety(frmsz, 1))) goto memfail;
        /// swap again and both become YUV (YVU out will get swapped on more time)
        lives_memcpy(tmp, (void **)gusrc_array[1], frmsz);  // v plane
        lives_memcpy(gusrc_array[1], (void **)gusrc_array[2], frmsz);  //u -> v
        lives_memcpy(gusrc_array[2], tmp, frmsz); // v -> u
        lives_free(tmp);
      }
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_quad_chroma_packed(gusrc_array, width, height, istrides, orowstride, gudest, FALSE, isampling, iclamping);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_quad_chroma_packed(gusrc_array, width, height, istrides, orowstride, gudest, TRUE, isampling, iclamping);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420_to_yuv411_frame(gusrc_array, width, height, (yuv411_macropixel *)gudest, FALSE, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUV422P:
    gusrc_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_rgb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, FALSE, TRUE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_rgb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, TRUE, TRUE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_bgr_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, FALSE, TRUE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_bgr_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, TRUE, TRUE,
                                   isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420p_to_argb_frame(gusrc_array, width, height, TRUE, istrides, orowstride, gudest, TRUE,
                                    isampling, iclamping, isubspace, gamma_type, new_gamma_type, NULL, -USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv422p_to_uyvy_frame(gusrc_array, width, height, istrides, orowstride, gudest);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv422p_to_yuyv_frame(gusrc_array, width, height, istrides, orowstride, gudest);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 3);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_halve_chroma(gusrc_array, width >> 1, height >> 1, istrides, ostrides, gudest_array, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, isampling);
      break;
    case WEED_PALETTE_YUV444P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 3);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_double_chroma(gusrc_array, width >> 1, height >> 1, istrides, ostrides, gudest_array, iclamping);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_YUVA4444P:
      create_empty_pixel_data(layer, FALSE, FALSE);
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      lives_free(gudest_array[0]);
      gudest_array[0] = gusrc_array[0];
      weed_layer_set_pixel_data_planar(layer, (void **)gudest_array, 4);
      ostrides = weed_layer_get_rowstrides(layer, NULL);
      convert_double_chroma(gusrc_array, width >> 1, height >> 1, istrides, ostrides, gudest_array, iclamping);
      lives_memset(gudest_array[3], 255, width * height);
      gusrc_array[0] = NULL;
      weed_layer_set_pixel_data_planar(orig_layer, (void **)gusrc_array, 3);
      break;
    case WEED_PALETTE_YUV888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_double_chroma_packed(gusrc_array, width, height, istrides, orowstride, gudest, FALSE, isampling, iclamping);
      break;
    case WEED_PALETTE_YUVA8888:
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      orowstride = weed_layer_get_rowstride(layer);
      convert_double_chroma_packed(gusrc_array, width, height, istrides, orowstride, gudest, TRUE, isampling, iclamping);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width >> 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv420_to_yuv411_frame(gusrc_array, width, height, (yuv411_macropixel *)gudest, TRUE, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  case WEED_PALETTE_YUV411:
    gusrc = weed_layer_get_pixel_data(layer);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_rgb_frame((yuv411_macropixel *)gusrc, width, height, orowstride, gudest, FALSE, iclamping);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_rgb_frame((yuv411_macropixel *)gusrc, width, height, orowstride, gudest, TRUE, iclamping);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_bgr_frame((yuv411_macropixel *)gusrc, width, height, orowstride, gudest, FALSE, iclamping);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_bgr_frame((yuv411_macropixel *)gusrc, width, height, orowstride, gudest, TRUE, iclamping);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      orowstride = weed_layer_get_rowstride(layer);
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_argb_frame((yuv411_macropixel *)gusrc, width, height, orowstride, gudest, iclamping);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_yuv888_frame((yuv411_macropixel *)gusrc, width, height, gudest, FALSE, iclamping);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_yuv888_frame((yuv411_macropixel *)gusrc, width, height, gudest, TRUE, iclamping);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuv411_to_yuvp_frame((yuv411_macropixel *)gusrc, width, height, gudest_array, FALSE, iclamping);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuv411_to_yuvp_frame((yuv411_macropixel *)gusrc, width, height, gudest_array, TRUE, iclamping);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_uyvy_frame((yuv411_macropixel *)gusrc, width, height, (uyvy_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 1);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest = weed_layer_get_pixel_data(layer);
      convert_yuv411_to_yuyv_frame((yuv411_macropixel *)gusrc, width, height, (yuyv_macropixel *)gudest, iclamping);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuv411_to_yuv422_frame((yuv411_macropixel *)gusrc, width, height, gudest_array, iclamping);
      break;
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuv411_to_yuv420_frame((yuv411_macropixel *)gusrc, width, height, gudest_array, FALSE, iclamping);
      break;
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer, WEED_LEAF_WIDTH, width << 2);
      if (!create_empty_pixel_data(layer, FALSE, TRUE)) goto memfail;
      gudest_array = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
      convert_yuv411_to_yuv420_frame((yuv411_macropixel *)gusrc, width, height, gudest_array, TRUE, iclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      goto memfail;
    }
    break;
  default:
    lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n", weed_palette_get_name(inpl),
                   weed_palette_get_name(outpl));
    goto memfail;
  }

#ifdef WEED_ADVANCED_PALETTES
conv_done:
#endif

  lives_freep((void **)&ostrides);
  lives_freep((void **)&gusrc_array);

  //lives_freep((void **)&gudest_array);

  if (orig_layer) weed_layer_free(orig_layer);

  if (new_gamma_type != WEED_GAMMA_UNKNOWN && can_inline_gamma(inpl, outpl)) {
    weed_layer_set_gamma(layer, new_gamma_type);
    gamma_type = weed_layer_get_gamma(layer);
  }

  if (weed_palette_is_rgb(outpl)) {
    weed_leaf_delete(layer, WEED_LEAF_YUV_CLAMPING);
    weed_leaf_delete(layer, WEED_LEAF_YUV_SUBSPACE);
    weed_leaf_delete(layer, WEED_LEAF_YUV_SAMPLING);
  } else {
    weed_set_int_value(layer, WEED_LEAF_YUV_CLAMPING, oclamping);
    if (weed_palette_is_rgb(inpl)) {
      if (gamma_type == WEED_GAMMA_BT709)
        weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, WEED_YUV_SUBSPACE_BT709);
      else
        weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, WEED_YUV_SUBSPACE_YCBCR);
    }
    if (!weed_plant_has_leaf(layer, WEED_LEAF_YUV_SAMPLING)) weed_set_int_value(layer,
          WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT);
  }

  /// if V plane is before U, swap the pointers
#ifdef WEED_ADVANCED_PALETTES
  if (get_advanced_palette(outpl)->chantype[1] == WEED_VCHAN_V) swap_chroma_planes(layer);
#else
  if (outpl == WEED_PALETTE_YVU420P) swap_chroma_planes(layer);
#endif

  lives_free(istrides);
  return TRUE;

memfail:
  lives_freep((void **)&gudest_array);
  weed_layer_set_palette(layer, inpl);
  weed_layer_set_size(layer, width, height);
  if (gusrc) {
    weed_layer_set_pixel_data(layer, gusrc);
    weed_layer_set_rowstride(layer, istrides[0]);
  } else if (gusrc_array) {
    weed_layer_set_pixel_data_planar(layer, (void **)gusrc_array, nplanes);
    weed_layer_set_rowstrides(layer, istrides, nplanes);
  }
  lives_free(istrides);
  return FALSE;
}


boolean convert_layer_palette(weed_layer_t *layer, int outpl, int op_clamping) {
  return convert_layer_palette_full(layer, outpl, op_clamping, WEED_YUV_SAMPLING_DEFAULT, WEED_YUV_SUBSPACE_YUV,
                                    WEED_GAMMA_UNKNOWN);
}

/////////////////////////////////////////////////////////////////////////////////////


LiVESPixbuf *lives_pixbuf_new_blank(int width, int height, int palette) {
  LiVESPixbuf *pixbuf;
  int rowstride;
  uint8_t *pixels;
  size_t size;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    pixbuf = lives_pixbuf_new(FALSE, width, height);
    rowstride = lives_pixbuf_get_rowstride(pixbuf);
    pixels = lives_pixbuf_get_pixels(pixbuf);
    size = rowstride * (height - 1) + get_last_pixbuf_rowstride_value(width, 3);
    lives_memset(pixels, 0, size);
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    pixbuf = lives_pixbuf_new(TRUE, width, height);
    rowstride = lives_pixbuf_get_rowstride(pixbuf);
    pixels = lives_pixbuf_get_pixels(pixbuf);
    size = rowstride * (height - 1) + get_last_pixbuf_rowstride_value(width, 4);
    lives_memset(pixels, 0, size);
    break;
  case WEED_PALETTE_ARGB32:
    pixbuf = lives_pixbuf_new(TRUE, width, height);
    rowstride = lives_pixbuf_get_rowstride(pixbuf);
    pixels = lives_pixbuf_get_pixels(pixbuf);
    size = rowstride * (height - 1) + get_last_pixbuf_rowstride_value(width, 4);
    lives_memset(pixels, 0, size);
    break;
  default:
    return NULL;
  }
  return pixbuf;
}


LIVES_INLINE LiVESPixbuf *lives_pixbuf_cheat(boolean has_alpha, int width, int height, uint8_t *buf) {
  // we can cheat if our buffer is correctly sized
  LiVESPixbuf *pixbuf;
  int channels = has_alpha ? 4 : 3;
  int rowstride = get_pixbuf_rowstride_value(width * channels);

  pixbuf = lives_pixbuf_new_from_data(buf, has_alpha, width, height, rowstride,
                                      (LiVESPixbufDestroyNotify)lives_free_buffer, NULL);
  return pixbuf;
}


int weed_layer_get_gamma(weed_layer_t *layer) {
  int gamma_type = WEED_GAMMA_UNKNOWN;
  if (prefs->apply_gamma) {
    if (weed_plant_has_leaf(layer, WEED_LEAF_GAMMA_TYPE)) {
      gamma_type = weed_get_int_value(layer, WEED_LEAF_GAMMA_TYPE, NULL);
    }
    if (gamma_type == WEED_GAMMA_UNKNOWN) {
      break_me("weed_layer_get_gamma with unk. gamma");
      LIVES_WARN("Layer with unknown gamma !!");
      gamma_type = WEED_GAMMA_SRGB;
    }
  }
  return gamma_type;
}


void gamma_conv_params(int gamma_type, weed_layer_t *inst, boolean is_in) {
  if (!prefs->apply_gamma) return;
  else {
    // convert colour param values to gamma_type (only integer values)
    weed_layer_t **params;
    weed_layer_t *ptmpl, *param;
    uint8_t *gamma_lut = NULL;
    const char *type = is_in ? WEED_LEAF_IN_PARAMETERS : WEED_LEAF_OUT_PARAMETERS;

    int *ivals;
    int ogamma_type, oogamma_type = WEED_GAMMA_UNKNOWN;
    int pptype, pcspace, ptype, nvals, qvals;
    int nparms;

    if (!inst) return;
    params = weed_get_plantptr_array_counted(inst, type, &nparms);
    if (!nparms) return;

    for (int i = 0; i < nparms; i++) {
      param = params[i];
      if (!(nvals = weed_leaf_num_elements(param, WEED_LEAF_VALUE))) continue;
      ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, NULL);
      pptype = weed_paramtmpl_get_type(ptmpl);
      if (pptype != WEED_PARAM_COLOR) continue;

      ptype = weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT);

      if (ptype != WEED_SEED_INT) gamma_type = WEED_GAMMA_SRGB;

      if (!prefs->apply_gamma || !weed_plant_has_leaf(param, WEED_LEAF_GAMMA_TYPE)) {
        ogamma_type = WEED_GAMMA_SRGB;
      } else {
        ogamma_type = weed_get_int_value(param, WEED_LEAF_GAMMA_TYPE, NULL);
      }

      if (ogamma_type != oogamma_type && gamma_type != ogamma_type) {
        if (gamma_lut) lives_gamma_lut_free(gamma_lut);
        gamma_lut = create_gamma_lut(1.0, ogamma_type, gamma_type);
        if (!gamma_lut) break;
        oogamma_type = ogamma_type;
      }

      weed_set_int_value(param, WEED_LEAF_GAMMA_TYPE, gamma_type);
      weed_leaf_set_flags(param, WEED_LEAF_GAMMA_TYPE, (weed_leaf_get_flags(param, WEED_LEAF_GAMMA_TYPE) |
                          WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE));

      // no change needed
      if (gamma_type == ogamma_type) continue;

      qvals = 3;
      pcspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, NULL);
      if (pcspace == WEED_COLORSPACE_RGBA) qvals = 4;
      ivals = weed_get_int_array(param, WEED_LEAF_VALUE, NULL);
      if (gamma_lut) {
        for (int j = 0; j < nvals; j += qvals) {
          for (int k = 0; k < 3; k++) {
            ivals[j + k] = gamma_lut[ivals[j + k]];
          }
        }
        lives_gamma_lut_free(gamma_lut);
        gamma_lut = NULL;
        weed_set_int_array(param, WEED_LEAF_VALUE, nvals, ivals);
        lives_free(ivals);
        weed_set_int_value(param, WEED_LEAF_GAMMA_TYPE, gamma_type);
        weed_leaf_set_flags(param, WEED_LEAF_GAMMA_TYPE, (weed_leaf_get_flags(param, WEED_LEAF_GAMMA_TYPE) |
                            WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE));
      }
    }
    lives_free(params);
    if (gamma_lut) lives_gamma_lut_free(gamma_lut);
  }
}


static void *gamma_convert_layer_thread(void *data) {
  lives_cc_params *ccparams = (lives_cc_params *)data;
  uint8_t *pixels = (uint8_t *)ccparams->src;
  int rowstride = ccparams->orowstrides[0];
  int psize = ccparams->psize, px = 3;
  int widthx = ccparams->hsize * psize;
  int start = ccparams->xoffset;

  uint8_t *gamma_lut = ccparams->lut;
  if (!gamma_lut) return NULL;

  if (psize < px) px = psize;

  if (ccparams->alpha_first) start += 1;
  for (int i = 0; i < ccparams->vsize; i++) {
    //g_print("\n");
    for (int j = start; j < start + widthx; j += psize) {
      for (int k = 0; k < px; k++) {
        //g_print("  PX %p + %d , %d  = %d\t", pixels, j + k, pixels[j + k], gamma_lut[pixels[k + j]]);
        pixels[rowstride * i + j + k] = gamma_lut[pixels[rowstride * i + j + k]];
      }
      //g_print("\n");
    }
  }
  return NULL;
}


/**
   @brief alter the transfer function of a Weed layer, from current value to gamma_type

*/
boolean gamma_convert_sub_layer(int gamma_type, double fileg, weed_layer_t *layer, int x, int y, int width, int height,
                                boolean may_thread) {
  if (!prefs->apply_gamma) return TRUE;
  else {
    // convert layer from current gamma to target
    int pal;
    pal = weed_layer_get_palette(layer);
    if (!weed_palette_is_rgb(pal)) return FALSE; //  dont know how to convert in yuv space
    else {
      int lgamma_type = weed_layer_get_gamma(layer);
      //g_print("gam from %d to %d with fileg %f\n", lgamma_type, gamma_type, fileg);
      if (gamma_type == lgamma_type && fileg == 1.0) return TRUE;
      else {
        lives_thread_t threads[prefs->nfx_threads];
        int nfx_threads = may_thread ? prefs->nfx_threads : 1;
        uint8_t *pixels = weed_layer_get_pixel_data(layer);
        uint8_t *gamma_lut;
        int orowstride = weed_layer_get_rowstride(layer);
        int nthreads = 1;
        int dheight;
        double psize = pixel_size(pal);
        lives_cc_params *ccparams = (lives_cc_params *)lives_calloc(nfx_threads,
                                    sizeof(lives_cc_params));
        int xdheight = CEIL((double)height / (double)nfx_threads, 4);
        uint8_t *end;

        pixels += y * orowstride;
        end = pixels + (height - 1) * orowstride;

        if (gamma_type == WEED_GAMMA_VARIANT)
          gamma_lut = create_gamma_lut(fileg, lgamma_type, gamma_type);
        else
          gamma_lut = create_gamma_lut(1.0, lgamma_type, gamma_type);

        if (!gamma_lut) return TRUE;

        for (int i = nfx_threads; i--;) {
          dheight = xdheight;

          ccparams[i].src = pixels + xdheight * i * orowstride;
          ccparams[i].hsize = width;
          ccparams[i].xoffset = x * psize;

          if (pixels + dheight * i * orowstride >= end)
            dheight = end - 1 - pixels - dheight * (i - 1) * orowstride;

          ccparams[i].vsize = dheight;
          ccparams[i].psize = psize;
          ccparams[i].orowstrides[0] = orowstride;

          if (pal == WEED_PALETTE_ARGB32) ccparams->alpha_first = TRUE;
          ccparams[i].lut = (void *)gamma_lut;
          ccparams[i].thread_id = i;
          if (i == 0) {
            gamma_convert_layer_thread(&ccparams[i]);
          } else {
            lives_thread_create(&threads[i], LIVES_THRDATTR_NONE, gamma_convert_layer_thread, &ccparams[i]);
            nthreads++;
          }
        }
        for (int i = 1; i < nthreads; i++) {
          lives_thread_join(threads[i], NULL);
        }
        lives_free(ccparams);
        lives_gamma_lut_free(gamma_lut);
        if (gamma_type != WEED_GAMMA_VARIANT)
          weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, gamma_type);
        return TRUE;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


LIVES_GLOBAL_INLINE boolean gamma_convert_layer(int gamma_type, weed_layer_t *layer) {
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  return gamma_convert_sub_layer(gamma_type, 1.0, layer, 0, 0, width, height, TRUE);
}


LIVES_GLOBAL_INLINE boolean gamma_convert_layer_variant(double file_gamma, int tgamma, weed_layer_t *layer) {
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_LINEAR);
  return gamma_convert_sub_layer(tgamma, file_gamma, layer, 0, 0, width, height, TRUE);
}


LiVESPixbuf *layer_to_pixbuf(weed_layer_t *layer, boolean realpalette, boolean fordisplay) {
  // create a gdkpixbuf from a weed layer
  // layer "pixel_data" is then either copied to the pixbuf pixels, or the contents shared with the pixbuf and array value set to NULL
  // layer may safely be passed to weed_layer_free() since if the pixel data is shared then it will be set to NULL in the layer.
  // pixbuf should be unreffed after use as per normal

  LiVESPixbuf *pixbuf;

  uint8_t *pixel_data, *pixels;

  boolean cheat = FALSE, done, nocheat = FALSE;

  int palette, xpalette;
  int width;
  int height;
  int irowstride;
  int rowstride, orowstride;
  int n_channels;

  if (!layer) return NULL;

  palette = weed_layer_get_palette(layer);

  if (weed_plant_has_leaf(layer, LIVES_LEAF_PIXBUF_SRC) && (!realpalette || weed_palette_is_pixbuf_palette(palette))) {
    // our layer pixel_data originally came from a pixbuf, so just nullify the layer and return the original pixbuf
    pixbuf = (LiVESPixbuf *)weed_get_voidptr_value(layer, LIVES_LEAF_PIXBUF_SRC, NULL);
    weed_layer_nullify_pixel_data(layer);
    weed_leaf_delete(layer, LIVES_LEAF_PIXBUF_SRC);
    return pixbuf;
  }

  // otherwise we need to steal or copy the pixel_data

  if (!weed_layer_get_pixel_data(layer)) {
    layer = create_blank_layer(layer, NULL, 0, 0, WEED_PALETTE_END);
  }

  if (weed_plant_has_leaf(layer, LIVES_LEAF_BBLOCKALLOC)) nocheat = TRUE;

  do {
    width = weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(palette);
    height = weed_layer_get_height(layer);
    irowstride = weed_layer_get_rowstride(layer);
    pixel_data = (uint8_t *)weed_layer_get_pixel_data(layer);
    done = TRUE;
    xpalette = palette;
    if (realpalette) {
      if (!weed_palette_is_pixbuf_palette(palette)) {
        // force conversion to RGB24 or RGBA32
        xpalette = WEED_PALETTE_END;
      } else {
        if (prefs->apply_gamma) {
          gamma_convert_layer(WEED_GAMMA_SRGB, layer);
          if (fordisplay && prefs->use_screen_gamma)
            gamma_convert_layer(WEED_GAMMA_MONITOR, layer);
        }
      }
    }
    switch (xpalette) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_YUV888:
      if (!nocheat && irowstride == get_pixbuf_rowstride_value(width * 3)) {
        // rowstrides are OK, we can just steal the pixel_data
        pixbuf = lives_pixbuf_cheat(FALSE, width, height, pixel_data);
        weed_layer_nullify_pixel_data(layer);
        cheat = TRUE;
      } else {
        // otherwise we need to copy the data
        pixbuf = lives_pixbuf_new(FALSE, width, height);
      }
      n_channels = 3;
      break;
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
#ifdef USE_SWSCALE
    case WEED_PALETTE_ARGB32:
#else
#endif
    case WEED_PALETTE_YUVA8888:
      if (!nocheat && irowstride == get_pixbuf_rowstride_value(width * 4)) {
        // rowstrides are OK, we can just steal the pixel_data
        pixbuf = lives_pixbuf_cheat(TRUE, width, height, pixel_data);
        weed_layer_nullify_pixel_data(layer);
        cheat = TRUE;
      } else {
        // otherwise we need to copy the data
        pixbuf = lives_pixbuf_new(TRUE, width, height);
      }
      n_channels = 4;
      break;
    default:
      if (weed_palette_has_alpha(palette)) {
        if (!convert_layer_palette(layer, WEED_PALETTE_RGBA32, 0)) return NULL;
        palette = WEED_PALETTE_RGBA32;
      } else {
        if (!convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) return NULL;
        palette = WEED_PALETTE_RGB24;
      }
      done = FALSE;
    }
  } while (!done);

  if (!cheat && LIVES_IS_PIXBUF(pixbuf)) {
    // copy the pixel data
    int k;
    pixel_data = (uint8_t *)weed_layer_get_pixel_data(layer);
    pixels = lives_pixbuf_get_pixels(pixbuf);
    orowstride = lives_pixbuf_get_rowstride(pixbuf);

    if (irowstride > orowstride) rowstride = orowstride;
    else rowstride = irowstride;

    height--;

    for (k = 0; k < height; k++) {
      lives_memcpy(&pixels[orowstride * k], &pixel_data[irowstride * k], rowstride);
      if (rowstride < orowstride) lives_memset(&pixels[orowstride * k + rowstride], 255, orowstride - rowstride);
    }
    rowstride = get_last_pixbuf_rowstride_value(width, n_channels);
    lives_memcpy(&pixels[orowstride * k], &pixel_data[irowstride * k], rowstride);
  }

  return pixbuf;
}


LIVES_INLINE boolean weed_palette_is_resizable(int pal, int clamped, boolean in_out) {
  // in_out is TRUE for input, FALSE for output

  // in future we may also have resize candidates/delegates for other palettes
  // we will need to check for these

  if (pal == WEED_PALETTE_YUV888 && clamped == WEED_YUV_CLAMPING_UNCLAMPED) pal = WEED_PALETTE_RGB24;
  if (pal == WEED_PALETTE_YUVA8888 && clamped == WEED_YUV_CLAMPING_UNCLAMPED) pal = WEED_PALETTE_RGBA32;

#ifdef USE_SWSCALE
  if (in_out && sws_isSupportedInput(weed_palette_to_avi_pix_fmt(pal, &clamped))) return TRUE;
  else if (sws_isSupportedOutput(weed_palette_to_avi_pix_fmt(pal, &clamped))) return TRUE;
#endif
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGR24 ||
      pal == WEED_PALETTE_BGRA32) return TRUE;
  return FALSE;
}


void lives_pixbuf_set_opaque(LiVESPixbuf * pixbuf) {
  unsigned char *pdata = lives_pixbuf_get_pixels(pixbuf);
  int row = lives_pixbuf_get_rowstride(pixbuf);
  int height = lives_pixbuf_get_height(pixbuf);
  int offs;
#ifdef GUI_GTK
  offs = 3;
#endif

#ifdef GUI_QT
  offs = 0;
#endif

  register int i, j;
  for (i = 0; i < height; i++) {
    for (j = offs; j < row; j += 4) {
      pdata[j] = 255;
    }
    pdata += row;
  }
}


void lives_layer_set_opaque(weed_layer_t *layer) {
  int offs;
  boolean planar = FALSE;
  int pal = weed_layer_get_palette(layer);
  if (weed_palette_is_planar(pal)) {
    offs = weed_palette_get_alpha_plane(pal);
    planar = TRUE;
  } else offs = weed_palette_get_alpha_offset(pal);

  if (offs >= 0) {
    int width = weed_layer_get_width(layer);
    int height = weed_layer_get_height(layer);
    int rowstride;

    if (planar) {
      int *rowstrides = weed_layer_get_rowstrides(layer, NULL);
      void **pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);
      rowstride = rowstrides[offs];
      height *= weed_palette_get_plane_ratio_vertical(pal, offs);
      lives_memset(pixel_data, 255, rowstride * height);
      lives_free(rowstrides);
      lives_free(pixel_data);
    } else {
      ssize_t frsize;
      double psize = pixel_size(pal);
      uint8_t *pixel_data = weed_layer_get_pixel_data(layer);
      rowstride = weed_layer_get_rowstride(layer);
      frsize = height * rowstride;
      width *= psize;
      for (register int i = 0; i < frsize; i += rowstride) {
        for (register int j = offs; j < width; j += psize) {
          pixel_data[i + j] = 255;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
}


boolean compact_rowstrides(weed_layer_t *layer) {
  // remove any extra padding after the image data
  weed_layer_t *old_layer;
  void **pixel_data, **new_pixel_data;
  int *rowstrides = weed_layer_get_rowstrides(layer, NULL), *orows;
  int pal = weed_layer_get_palette(layer);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int xheight;
  int crow = width * weed_palette_get_bits_per_macropixel(pal) / 8;
  int cxrow;
  int nplanes;
  boolean needs_change = FALSE;
  int i, j;

  pixel_data = weed_layer_get_pixel_data_planar(layer, &nplanes);

  for (i = 0; i < nplanes; i++) {
    cxrow = crow * weed_palette_get_plane_ratio_horizontal(pal, i);
    if (cxrow != rowstrides[i]) {
      // nth plane has extra padding
      needs_change = TRUE;
    }
  }

  if (!needs_change) {
    lives_free(pixel_data);
    lives_free(rowstrides);
    return TRUE;
  }

  old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  if (!old_layer) return FALSE;
  if (!weed_layer_copy(old_layer, layer)) return FALSE;

  THREADVAR(rowstride_alignment_hint) = -1;
  weed_layer_nullify_pixel_data(layer);

  if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
    weed_layer_copy(layer, old_layer);
    weed_layer_nullify_pixel_data(old_layer);
    weed_layer_free(old_layer);
    return FALSE;
  }

  new_pixel_data = weed_layer_get_pixel_data_planar(layer, &nplanes);

  if (!new_pixel_data) {
    weed_layer_free(old_layer);
    lives_free(pixel_data);
    lives_free(rowstrides);
    return FALSE;
  }

  orows = weed_layer_get_rowstrides(layer, NULL);

  for (i = 0; i < nplanes; i++) {
    xheight = height * weed_palette_get_plane_ratio_vertical(pal, i);
    cxrow = orows[i];
    for (j = 0; j < xheight; j++) {
      lives_memcpy((uint8_t *)new_pixel_data[i] + j * cxrow, (uint8_t *)pixel_data[i] + j * rowstrides[i], cxrow);
      //for (int k = 3; k < cxrow; k += 4) ((uint8_t *)new_pixel_data[i])[j * cxrow + k] = 0;
    }
  }

  if (nplanes > 1) weed_set_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
  weed_layer_free(old_layer);
  lives_free(pixel_data);
  lives_free(new_pixel_data);
  lives_free(rowstrides);
  lives_free(orows);
  return TRUE;
}


#ifdef USE_SWSCALE
#if USE_THREADS
static void *swscale_threadfunc(void *arg) {
  lives_sw_params *swparams = (lives_sw_params *)arg;
#ifdef USE_RESTHREAD
  int scan = 0;
  if (swparams->layer) {
    int last = swparams->iheight * (swparams->thread_id + 1);
    while ((scan = weed_get_int_value(swparams->layer, WEED_LEAF_PROGSCAN, NULL)) > 0
           && scan < last) {
      lives_nanosleep(100);
    }
  }
#endif
  swparams->ret = sws_scale(swparams->swscale, (const uint8_t *const *)swparams->ipd, swparams->irw,
                            0, swparams->iheight, (uint8_t *const *)swparams->opd, swparams->orw);

  if (swparams->file_gamma != 1.) {
    gamma_convert_sub_layer(WEED_GAMMA_SRGB, 1. / swparams->file_gamma, swparams->layer, 0, 0, swparams->width,
                            swparams->ret, FALSE);
  }

  return NULL;
}
#endif
#endif

/**
   @brief resize a layer

   width is in PIXELS (not macropixels)

   opal_hint and oclamp_hint may be set to hint the desired output palette and YUV clamping
   this is simply to ensure more efficient resizing, and may be ignored

   - setting opal_hint to WEED_PALETTE_END will attempt to minimise palette changes

   "current_palette" should be checked on return as it may change

   NOTE: swscale may crash if widths are not even,
   and likes to have width in multiples of 8 (may only be when changing palettes ?)

   @return FALSE if we were unable to resize */
boolean resize_layer(weed_layer_t *layer, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint) {
  // TODO ** - see if there is a resize plugin candidate/delegate which supports this palette :
  // this allows e.g libabl or a hardware rescaler
  LiVESPixbuf *pixbuf = NULL;
  LiVESPixbuf *new_pixbuf = NULL;

  boolean retval = TRUE;

  int palette = weed_layer_get_palette(layer);

  // original width and height (in pixels)
  int iwidth = weed_layer_get_width_pixels(layer);
  int iheight = weed_layer_get_height(layer);
  int iclamping = weed_layer_get_yuv_clamping(layer);
  int new_gamma_type = WEED_GAMMA_UNKNOWN;

#ifdef USE_SWSCALE
  boolean resolved = FALSE;
  int xpalette, xopal_hint;
#endif
#ifdef USE_RESTHREAD
  boolean progscan = FALSE;
  if (weed_get_int_value(layer, WEED_LEAF_PROGSCAN, NULL) > 0) progscan = TRUE;
#endif

  if (!weed_plant_has_leaf(layer, WEED_LEAF_PIXEL_DATA)) {
    weed_layer_set_size(layer, width / weed_palette_get_pixels_per_macropixel(opal_hint), height);
    if (opal_hint != WEED_PALETTE_END) {
      weed_layer_set_palette(layer, opal_hint);
    }
    weed_layer_set_yuv_clamping(layer, oclamp_hint);
    return FALSE;
  }

  if (width <= 0 || height <= 0) {
    char *msg = lives_strdup_printf("unable to scale layer to %d x %d for palette %d\n", width, height, palette);
    LIVES_DEBUG(msg);
    lives_free(msg);
    return FALSE;
  }
  //        #define DEBUG_RESIZE
#ifdef DEBUG_RESIZE
  g_print("resizing layer size %d X %d with palette %s to %d X %d, hinted %s\n", iwidth, iheight,
          weed_palette_get_name_full(palette,
                                     iclamping, 0), width, height, weed_palette_get_name_full(opal_hint, oclamp_hint, 0));
#endif

  iwidth = (iwidth >> 1) << 1;
  iheight = (iheight >> 1) << 1;

  if (width < 4) width = 4;
  if (height < 4) height = 4;

  if (iwidth != width || iheight != height) {
    // prevent a crash in swscale
    height = (height >> 1) << 1;
  }
  if (iwidth == width && iheight == height) return TRUE; // no resize needed

  if (weed_plant_has_leaf(layer, WEED_LEAF_GAMMA_TYPE)) new_gamma_type  = weed_layer_get_gamma(layer);

  // if in palette is a YUV palette which we cannot scale, convert to YUV888 (unclamped) or YUVA8888 (unclamped)
  // we can always scale these by pretending they are RGB24 and RGBA32 respectively
  if (weed_palette_is_yuv(palette)) {
    if (!weed_palette_is_resizable(palette, iclamping, TRUE)) {
      if (opal_hint == WEED_PALETTE_END || weed_palette_is_yuv(opal_hint)) {
        if (weed_palette_has_alpha(palette)) {
          convert_layer_palette(layer, WEED_PALETTE_YUVA8888, WEED_YUV_CLAMPING_UNCLAMPED);
        } else {
          convert_layer_palette(layer, WEED_PALETTE_YUV888, WEED_YUV_CLAMPING_UNCLAMPED);
        }
        oclamp_hint = iclamping = (weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL));
      } else {
        if (weed_palette_has_alpha(palette)) {
          convert_layer_palette(layer, WEED_PALETTE_RGBA32, 0);
        } else {
          convert_layer_palette(layer, WEED_PALETTE_RGB24, 0);
        }
      }
      palette = weed_layer_get_palette(layer);
      iwidth = weed_layer_get_width_pixels(layer);
      iheight = weed_layer_get_height(layer);
      iwidth = (iwidth >> 1) << 1;
      iheight = (iheight >> 1) << 1;
      if (iwidth == width && iheight == height) return TRUE; // no resize needed
#ifdef DEBUG_RESIZE
      g_print("intermediate conversion 1 to %s\n", weed_palette_get_name_full(palette, iclamping, 0));
#endif
    }
  }

  // check if we can also convert to the output palette
#ifdef USE_SWSCALE
  // only swscale can convert and resize together
  if (opal_hint == WEED_PALETTE_END || !weed_palette_is_resizable(opal_hint, oclamp_hint, FALSE)) {
#endif
    opal_hint = palette;
    oclamp_hint = iclamping;
#ifdef USE_SWSCALE
  }
#endif

  // check if we can convert to the target palette/clamping
  if (!weed_palette_is_resizable(opal_hint, oclamp_hint, FALSE)) {
    opal_hint = palette;
    oclamp_hint = iclamping;
  }

#ifdef USE_SWSCALE
  // sws doesn't understand YUV888 or YUVA888, but if the output palette is also YUV888 or YUVA8888
  // then we can use unclamped values and  pretend they are RGB24 and RGBA32.
  // Otherwise we need to convert to YUV444P and YUVA4444P.

  // lookup values for av_pix_fmt
  xpalette = palette;
  xopal_hint = opal_hint;

  if (palette == WEED_PALETTE_YUV888) {
    if (opal_hint == WEED_PALETTE_YUV888 || opal_hint == WEED_PALETTE_YUVA8888) {
      if (iclamping == WEED_YUV_CLAMPING_CLAMPED) {
        convert_layer_palette(layer, WEED_PALETTE_YUV888, WEED_YUV_CLAMPING_UNCLAMPED);
        palette = weed_layer_get_palette_yuv(layer, &iclamping, NULL, NULL);
#ifdef DEBUG_RESIZE
        g_print("intermediate conversion 2 to %s\n", weed_palette_get_name_full(palette, iclamping, 0));
#endif
      }
      if (iclamping == WEED_YUV_CLAMPING_UNCLAMPED) {
        xpalette = WEED_PALETTE_RGB24;
        oclamp_hint = WEED_YUV_CLAMPING_UNCLAMPED;
        resolved = TRUE;
        if (opal_hint == WEED_PALETTE_YUV888) {
          xopal_hint = WEED_PALETTE_RGB24;
        } else {
          xopal_hint = WEED_PALETTE_RGBA32;
        }
#ifdef DEBUG_RESIZE
        g_print("masquerading as %s\n", weed_palette_get_name_full(xpalette, oclamp_hint, 0));
#endif
      }
    }
    if (!resolved) {
      convert_layer_palette(layer, WEED_PALETTE_YUV444P, iclamping);
      xpalette = palette = weed_layer_get_palette_yuv(layer, &iclamping, NULL, NULL);
#ifdef DEBUG_RESIZE
      g_print("intermediate conversion 3 to %s\n", weed_palette_get_name_full(xpalette, iclamping, 0));
#endif
    }
  } else if (palette == WEED_PALETTE_YUVA8888) {
    if (opal_hint == WEED_PALETTE_YUV888 || opal_hint == WEED_PALETTE_YUVA8888) {
      if (iclamping == WEED_YUV_CLAMPING_CLAMPED) {
        convert_layer_palette(layer, WEED_PALETTE_YUVA8888, WEED_YUV_CLAMPING_UNCLAMPED);
        xpalette = palette = weed_layer_get_palette_yuv(layer, &iclamping, NULL, NULL);
      }
      if (iclamping == WEED_YUV_CLAMPING_UNCLAMPED) {
        xpalette = WEED_PALETTE_RGBA32;
        oclamp_hint = WEED_YUV_CLAMPING_UNCLAMPED;
        resolved = TRUE;
        if (opal_hint == WEED_PALETTE_YUVA8888) {
          xopal_hint = WEED_PALETTE_RGBA32;
        } else {
          xopal_hint = WEED_PALETTE_RGB24;
        }
#ifdef DEBUG_RESIZE
        g_print("masquerading as %s\n", weed_palette_get_name_full(xpalette, oclamp_hint, 0));
#endif
      }
    }
    if (!resolved) {
      convert_layer_palette(layer, WEED_PALETTE_YUVA4444P, iclamping);
      xpalette = palette = weed_layer_get_palette_yuv(layer, &iclamping, NULL, NULL);
#ifdef DEBUG_RESIZE
      g_print("intermediate conversion 4 to %s\n", weed_palette_get_name_full(xpalette, iclamping, 0));
#endif
    }
  }

  // reget these after conversion, convert width from macropixels to pixels
  iwidth = weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(palette);
  iheight = weed_layer_get_height(layer);
  if (iwidth == width && iheight == height) return TRUE; // no resize needed

  if (opal_hint == WEED_PALETTE_YUV888) opal_hint = xopal_hint = WEED_PALETTE_YUV444P;
  else if (opal_hint == WEED_PALETTE_YUVA8888) opal_hint = xopal_hint = WEED_PALETTE_YUVA4444P;

  if (iwidth > 1 && iheight > 1 && weed_palette_is_resizable(palette, iclamping, TRUE) &&
      weed_palette_is_resizable(xopal_hint, oclamp_hint, FALSE)) {
    weed_layer_t *old_layer;

    void **in_pixel_data, **out_pixel_data;

    int *irowstrides, *orowstrides;

    //boolean store_ctx = FALSE;

    swpixfmt ipixfmt, opixfmt;

    int flags;

    const uint8_t *ipd[4], *opd[4];
    int irw[4], orw[4];

    int i;
    swsctx_block *ctxblock;
    int offset;
#if USE_THREADS
    lives_sw_params *swparams;
    lives_thread_t threads[prefs->nfx_threads];
    int nthrds = 1;
#else
    struct SwsContext *swscale;
#endif
    int subspace = WEED_YUV_SUBSPACE_YUV;
    int inplanes, oplanes;

    /// old layer will hold a ref to the original pixel_data. We will free it at the end since the pixel_data
    /// of layer will be recreated when we calll create_empty_pixel_data()

    // get current values
    in_pixel_data = weed_layer_get_pixel_data_planar(layer, &inplanes);
    irowstrides = weed_layer_get_rowstrides(layer, NULL);

    old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_layer_copy(old_layer, layer);

    av_log_set_level(AV_LOG_FATAL);

    THREADVAR(rowstride_alignment_hint) = 16;

    if (interp == LIVES_INTERP_BEST) {
      if (width > iwidth || height > iheight)
        flags = SWS_LANCZOS; // other opts SINC, LANCZOS, SPLINE; ACCURATE_RND, BITEXACT. ERROR_DIFFUSION
      else
        flags = SWS_BICUBIC;
    } else if (interp == LIVES_INTERP_FAST) flags = SWS_FAST_BILINEAR;
    else flags = SWS_BILINEAR;

    ipixfmt = weed_palette_to_avi_pix_fmt(xpalette, &iclamping);
    opixfmt = weed_palette_to_avi_pix_fmt(xopal_hint, &oclamp_hint);

    for (i = 0; i < 4; i++) {
      // swscale always likes 4 elements, even if fewer planes are used
      if (i < inplanes) {
        ipd[i] = in_pixel_data[i];
        irw[i] = irowstrides[i];
      } else {
        ipd[i] = NULL;
        irw[i] = 0;
      }
    }

    if (new_gamma_type != WEED_GAMMA_UNKNOWN) {
      if (weed_palette_is_rgb(palette) && !weed_palette_is_rgb(opal_hint) &&
          weed_layer_get_gamma(layer) == WEED_GAMMA_LINEAR) {
        // gamma correction
        if (prefs->apply_gamma) {
          gamma_convert_layer(WEED_GAMMA_SRGB, layer);
        }
      }
      new_gamma_type = weed_layer_get_gamma(layer);
    }

    // set new values

    if (palette != opal_hint && opal_hint != WEED_PALETTE_END) {
      weed_layer_set_palette(layer, opal_hint);
    }

    if (weed_palette_is_yuv(opal_hint)) weed_layer_set_yuv_clamping(layer, oclamp_hint);
    //weed_layer_set_size(layer, width, height);
    weed_layer_set_size(layer, width / weed_palette_get_pixels_per_macropixel(opal_hint), height);
    weed_layer_nullify_pixel_data(layer);

    if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
      weed_layer_copy(layer, old_layer);
      weed_layer_nullify_pixel_data(old_layer);
      weed_layer_free(old_layer);
      return FALSE;
    }

    out_pixel_data = weed_layer_get_pixel_data_planar(layer, &oplanes);
    orowstrides = weed_layer_get_rowstrides(layer, NULL);

    if (new_gamma_type != WEED_GAMMA_UNKNOWN) {
      //weed_layer_set_gamma(layer, new_gamma_type);
      if (weed_palette_is_rgb(palette)) {
        if (new_gamma_type == WEED_GAMMA_BT709) subspace = WEED_YUV_SUBSPACE_BT709;
      } else if (weed_palette_is_yuv(palette)) {
        if (new_gamma_type == WEED_GAMMA_BT709 || weed_layer_get_yuv_subspace(old_layer) == WEED_YUV_SUBSPACE_BT709)
          subspace = WEED_YUV_SUBSPACE_BT709;
      }
    }

    for (i = 0; i < 4; i++) {
      // swscale always likes 4 elements, even if fewer planes are used
      if (i < oplanes) {
        opd[i] = out_pixel_data[i];
        orw[i] = orowstrides[i];
      } else {
        opd[i] = NULL;
        orw[i] = 0;
      }
    }

    //width /= weed_palette_get_pixels_per_macropixel(palette);

#if USE_THREADS
    while ((nthrds << 1) <= prefs->nfx_threads) {
      if ((height | iheight) & 3) break;
      nthrds <<= 1;
      iheight >>= 1;
      height >>= 1;
    }
    swparams = (lives_sw_params *)lives_calloc(nthrds, sizeof(lives_sw_params));
#else
    // TODO - can we set the gamma ?
    //g_print("iht is %d, height = %d\n", iheight, height);
    ctxblock = sws_getblock(1, iwidth, iheight, irw, ipixfmt, width, height, orw, opixfmt, flags,
                            subspace, iclamping, oclamp_hint);
    offset = ctxblock->offset;
    if (!ctxblock->match)
      swscale = swscalep[offset] = sws_getCachedContext(swscalep[offset], iwidth, iheight, ipixfmt,
                                   width, height, opixfmt, flags,
                                   NULL, NULL, NULL);
    swscale = swscalep[offset];

    sws_setColorspaceDetails(swscale, sws_getCoefficients((subspace == WEED_YUV_SUBSPACE_BT709)
                             ? SWS_CS_ITU709 : SWS_CS_ITU601), iclamping,
                             sws_getCoefficients((subspace == WEED_YUV_SUBSPACE_BT709)
                                 ? SWS_CS_ITU709 : SWS_CS_ITU601), oclamp_hint,  0, 1 << 16, 1 << 16);

    if (!swscale) {
      LIVES_DEBUG("swscale is NULL !!");
    } else {
#endif

#ifdef DEBUG_RESIZE
    g_print("before resize with swscale: layer size %d X %d with palette %s to %d X %d, hinted %s,\n"
            "masquerading as %s (avpixfmt %d to avpixfmt %d)\n",
            iwidth, iheight, weed_palette_get_name_full(palette, iclamping, 0), width, height,
            weed_palette_get_name_full(opal_hint,
                                       oclamp_hint, 0),
            weed_palette_get_name_full(xopal_hint, oclamp_hint, 0), ipixfmt, opixfmt);
#endif
#if USE_THREADS
    ctxblock = sws_getblock(nthrds, iwidth, iheight, irw, ipixfmt, width, height, orw, opixfmt, flags,
                            subspace, iclamping, oclamp_hint);
    offset = ctxblock->offset;

    for (int sl = 0; sl < nthrds; sl++) {
      swparams[sl].thread_id = sl;
      swparams[sl].iheight = iheight;
      swparams[sl].width = width;
      swparams[sl].file_gamma = 1.;

      /// swsfreeContext always seems to leak memory..
      //sws_freeContext(swscalep[sl + offset]);
      if (!ctxblock->match)
        swparams[sl].swscale = swscalep[sl + offset] =
                                 sws_getCachedContext(swscalep[sl + offset], iwidth, iheight, ipixfmt, width,
                                     height, opixfmt, flags, NULL, NULL, NULL);
      else
        swparams[sl].swscale = swscalep[sl + offset];

      if (!swparams[sl].swscale) {
        LIVES_DEBUG("swscale is NULL !!");
      } else {
        //if (progscan) {
        swparams[sl].layer = layer;
        swparams[sl].file_gamma = weed_get_double_value(layer, "file_gamma", NULL);
        if (swparams[sl].file_gamma == 0.) swparams[sl].file_gamma = 1.;
        else swparams[sl].layer = NULL;
        sws_setColorspaceDetails(swparams[sl].swscale,
                                 sws_getCoefficients((subspace == WEED_YUV_SUBSPACE_BT709)
                                     ? SWS_CS_ITU709 : SWS_CS_ITU601), iclamping,
                                 sws_getCoefficients((subspace == WEED_YUV_SUBSPACE_BT709)
                                     ? SWS_CS_ITU709 : SWS_CS_ITU601), oclamp_hint,  0, 65536, 65536);
        for (i = 0; i < 4; i++) {
          if (i < inplanes)
            swparams[sl].ipd[i] =
              ipd[i] + (size_t)(sl * irw[i] * iheight
                                * weed_palette_get_plane_ratio_vertical(palette, i));
          else
            swparams[sl].ipd[i] = NULL;

          if (i < oplanes)
            swparams[sl].opd[i] =
              opd[i] + (size_t)(sl * orw[i] * height
                                * weed_palette_get_plane_ratio_vertical(opal_hint, i));
          else
            swparams[sl].opd[i] = NULL;
        }
        swparams[sl].irw = irw;
        swparams[sl].orw = orw;
        if (sl < nthrds - 1) lives_thread_create(&threads[sl], LIVES_THRDATTR_NONE,
              swscale_threadfunc, &swparams[sl]);
        else swscale_threadfunc(&swparams[sl]);
      }
    }
    iheight = height;

    for (int sl = 0; sl < nthrds - 1; sl++) {
      if (swparams[sl].swscale) {
        lives_thread_join(threads[sl], NULL);
        height += swparams[sl].ret;
      } else height += iheight;
    }

    sws_freeblock(ctxblock);
    lives_free(swparams);

#else
#ifdef USE_RESTHREAD
    if (progscan)
      while ((scan = weed_get_int_value(layer, WEED_LEAF_PROGSCAN, NULL)) > 0 && scan < iheight)
        lives_nanosleep(100);
#endif
    height = sws_scale(swscale, (const uint8_t *const *)ipd, irw, 0, iheight,
                       (uint8_t *const *)opd, orw);
  }
#endif

#ifdef DEBUG_RESIZE
    g_print("after resize with swscale: layer size %d X %d, palette %s (assumed successful)\n",
            width, height, weed_palette_get_name_full(opal_hint, oclamp_hint, 0));
#endif
    //if (store_ctx) swscale_add_context(iwidth, iheight, width, height, ipixfmt, opixfmt, flags, swscale);
    //weed_layer_set_width(layer, width);
    weed_layer_set_height(layer, height);

    /* for (int gg = 0; gg < height; gg++) { */
    /*   memset(&((uint8_t *)out_pixel_data[0])[gg * orowstrides[0]], 255, width * 3 / 2); */
    /* } */

    // this will properly free() in_pixel_data
    weed_layer_unref(old_layer);
    lives_free(out_pixel_data);
    lives_free(orowstrides);
    lives_free(irowstrides);
    lives_free(in_pixel_data);
    return TRUE;
  }
#endif

  // reget these after conversion, convert width from macropixels to pixels
  iwidth = weed_layer_get_width_pixels(layer);
  iheight = weed_layer_get_height(layer);
  if (iwidth == width && iheight == height) return TRUE; // no resize needed

  switch (palette) {
  // anything with 3 or 4 channels (alpha must be last)

  case WEED_PALETTE_YUV888:
  case WEED_PALETTE_YUVA8888:
    if (iclamping == WEED_YUV_CLAMPING_CLAMPED) {
      if (weed_palette_has_alpha(palette)) {
        convert_layer_palette(layer, WEED_PALETTE_YUVA8888, WEED_YUV_CLAMPING_UNCLAMPED);
      } else {
        convert_layer_palette(layer, WEED_PALETTE_YUV888, WEED_YUV_CLAMPING_UNCLAMPED);
      }
    }

  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:

    // create a new pixbuf
    pixbuf = layer_to_pixbuf(layer, FALSE, FALSE);

    threaded_dialog_spin(0.);
    new_pixbuf = lives_pixbuf_scale_simple(pixbuf, width, height, interp);
    threaded_dialog_spin(0.);
    if (new_pixbuf) {
      weed_layer_set_size(layer, lives_pixbuf_get_width(new_pixbuf), lives_pixbuf_get_height(new_pixbuf));
      weed_layer_set_rowstride(layer, lives_pixbuf_get_rowstride(new_pixbuf));
    }

    lives_widget_object_unref(pixbuf);

    break;
  default:
    lives_printerr("Warning: resizing unknown palette %d\n", palette);
    break_me("resize_layer with unk. pal");
    retval = FALSE;
  }

  if (!new_pixbuf || (width != weed_layer_get_width(layer) ||
                      height != weed_layer_get_height(layer)))  {
    lives_printerr("unable to scale layer to %d x %d for palette %d\n", width, height, palette);
    retval = FALSE;
  } else {
    if (weed_plant_has_leaf(layer, WEED_LEAF_HOST_ORIG_PDATA))
      weed_leaf_delete(layer, WEED_LEAF_HOST_ORIG_PDATA);
  }

  if (new_pixbuf) {
    if (!pixbuf_to_layer(layer, new_pixbuf)) lives_widget_object_unref(new_pixbuf);
  }

  return retval;
}


boolean letterbox_layer(weed_layer_t *layer, int nwidth, int nheight, int width, int height,
                        LiVESInterpType interp, int tpal, int tclamp) {
  // stretch or shrink layer to width/height, then overlay it in a black rectangle size nwidth/nheight
  // width, nwidth should be in pixels
  weed_layer_t *old_layer;
  int *rowstrides, *irowstrides;
  void **pixel_data;
  void **new_pixel_data;
  uint8_t *dst, *src;
  int lwidth, lheight;
  int offs_x = 0, offs_y = 0;
  int pal;

  int i;

  if (!width || !height || !nwidth || !nheight) return TRUE;
  if (nwidth < width) nwidth = width;
  if (nheight < height) nheight = height;

  /// no letterboxing needed - resize and return
  if (nheight == height && nwidth == width) {
    resize_layer(layer, width, height, interp, tpal, tclamp);
    return TRUE;
  }

  pal = weed_layer_get_palette(layer);
  lwidth = weed_layer_get_width_pixels(layer);
  lheight = weed_layer_get_height(layer);

  if (lwidth != width || lheight != height) {
    /// resize the inner rectangle
    if (!resize_layer(layer, width, height, interp, tpal, tclamp)) return FALSE;
    pal = weed_layer_get_palette(layer);
    lwidth = weed_layer_get_width_pixels(layer);
    lheight = weed_layer_get_height(layer);
  }

  // old layer will hold pointers to the original pixel data for layer
  old_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

  if (!old_layer) return FALSE;
  if (!weed_layer_copy(old_layer, layer)) return FALSE;

  pixel_data = weed_layer_get_pixel_data_planar(old_layer, NULL);

  if (!pixel_data) {
    weed_layer_free(old_layer);
    return FALSE;
  }

  width = lwidth;
  height = lheight;
  irowstrides = weed_layer_get_rowstrides(layer, NULL);

  /// create the outer rectangle in layer
  weed_layer_set_size(layer, nwidth / weed_palette_get_pixels_per_macropixel(pal), nheight);
  weed_layer_nullify_pixel_data(layer);
  if (!create_empty_pixel_data(layer, TRUE, TRUE)) goto memfail2;

  new_pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);

  /// get the actual size after any adjustments
  nwidth = weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(pal);
  nheight = weed_layer_get_height(layer);

  if (nwidth < width || nheight < height || !new_pixel_data) {
    /// this shouldn't happen, but if  the outer rectangle is smaller than the inner we have to abort
    goto memfail2;
  }

  offs_x = (nwidth - width + 1) >> 1;
  offs_y = (nheight - height + 1) >> 1;

  rowstrides = weed_layer_get_rowstrides(layer, NULL);

  switch (pal) {
  // 3 byte pixels, packed
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_YUV888:
    width *= 3;
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * 3;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  // 4 byte pixels, packed
  case WEED_PALETTE_UYVY:
  case WEED_PALETTE_YUYV:
    offs_x >>= 1;
    width >>= 1;

  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_YUVA8888:

    width *= 4;
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * 4;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  case WEED_PALETTE_YUV411:
    width *= 6;
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * 6;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  case WEED_PALETTE_YUV444P:
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[1] + offs_y * rowstrides[1] + offs_x;
    src = (uint8_t *)pixel_data[1];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[1] * i], &src[irowstrides[1] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[2] + offs_y * rowstrides[2] + offs_x;
    src = (uint8_t *)pixel_data[2];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[2] * i], &src[irowstrides[2] * i], width);
    }
    break;

  case WEED_PALETTE_YUVA4444P:
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[1] + offs_y * rowstrides[1] + offs_x;
    src = (uint8_t *)pixel_data[1];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[1] * i], &src[irowstrides[1] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[2] + offs_y * rowstrides[2] + offs_x;
    src = (uint8_t *)pixel_data[2];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[2] * i], &src[irowstrides[2] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[3] + offs_y * rowstrides[3] + offs_x;
    src = (uint8_t *)pixel_data[3];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[3] * i], &src[irowstrides[3] * i], width);
    }
    break;

  case WEED_PALETTE_YUV422P:
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    height >>= 1;
    offs_x >>= 1;
    dst = (uint8_t *)new_pixel_data[1] + offs_y * rowstrides[1] + offs_x;
    src = (uint8_t *)pixel_data[1];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[1] * i], &src[irowstrides[1] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[2] + offs_y * rowstrides[2] + offs_x;
    src = (uint8_t *)pixel_data[2];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[2] * i], &src[irowstrides[2] * i], width);
    }
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    height >>= 1;
    offs_x >>= 1;
    width >>= 1;
    offs_y >>= 1;
    dst = (uint8_t *)new_pixel_data[1] + offs_y * rowstrides[1] + offs_x;
    src = (uint8_t *)pixel_data[1];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[1] * i], &src[irowstrides[1] * i], width);
    }
    dst = (uint8_t *)new_pixel_data[2] + offs_y * rowstrides[2] + offs_x;
    src = (uint8_t *)pixel_data[2];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[2] * i], &src[irowstrides[2] * i], width);
    }
    break;

  case WEED_PALETTE_RGBFLOAT:
    width *= 3 * sizeof(float);
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * 3 * sizeof(float);
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  case WEED_PALETTE_RGBAFLOAT:
    width *= 4 * sizeof(float);
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * 4 * sizeof(float);
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  case WEED_PALETTE_AFLOAT:
    width *= sizeof(float);
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x * sizeof(float);
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  case WEED_PALETTE_A8:
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + offs_x;
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;

  // assume offs_x and width is a multiple of 8
  case WEED_PALETTE_A1:
    width >>= 3;
    dst = (uint8_t *)new_pixel_data[0] + offs_y * rowstrides[0] + (offs_x >> 3);
    src = (uint8_t *)pixel_data[0];
    for (i = 0; i < height; i++) {
      lives_memcpy(&dst[rowstrides[0] * i], &src[irowstrides[0] * i], width);
    }
    break;
  }

  if (mainw->frame_layer && weed_layer_get_pixel_data(mainw->frame_layer) == pixel_data[0]) {
    /// retain pixel_data if it belongs to mainw->frame_layer
    weed_layer_nullify_pixel_data(old_layer);
  }
  /// otherwise do not nullify, as we want to free old pixel_data
  weed_layer_free(old_layer);
  lives_free(pixel_data);
  lives_free(new_pixel_data);
  lives_free(irowstrides);
  lives_free(rowstrides);
  return TRUE;

memfail2:
  weed_layer_pixel_data_free(layer);
  weed_layer_copy(layer, old_layer);
  weed_layer_nullify_pixel_data(old_layer);
  weed_layer_free(old_layer);
  lives_free(pixel_data);
  lives_free(irowstrides);
  return FALSE;
}


/**
   @brief turn a (Gdk)Pixbuf into a Weed layer

   return TRUE if we can use the original pixbuf pixels; in this case the pixbuf pixels should only be freed via
   lives_layer_pixel_data_free() or lives_layer_free()
   see code example.

   code example:

   weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
   if (pixbuf) {
	if (!pixbuf_to_layer(layer, pixbuf)) lives_widget_object_unref(pixbuf);
	else do NOT unref the pixbuf !!!!
   }

   do something with layer...

   weed_layer_pixel_data_free(layer); unrefs the pixbuf
   or
   weed_layer_free(layer); also unrefs the pixbuf

*/
boolean pixbuf_to_layer(weed_layer_t *layer, LiVESPixbuf * pixbuf) {
  size_t framesize;
  void *pixel_data;
  void *in_pixel_data;
  int rowstride;
  int width;
  int height;
  int nchannels, palette;

  if (!LIVES_IS_PIXBUF(pixbuf)) {
    weed_layer_set_size(layer, 0, 0);
    weed_layer_set_rowstride(layer, 0);
    weed_layer_pixel_data_free(layer);
    return TRUE;
  }

  rowstride = lives_pixbuf_get_rowstride(pixbuf);
  width = lives_pixbuf_get_width(pixbuf);
  height = lives_pixbuf_get_height(pixbuf);
  nchannels = lives_pixbuf_get_n_channels(pixbuf);

  weed_layer_set_width(layer, width);
  weed_layer_set_height(layer, height);
  weed_layer_set_rowstride(layer, rowstride);

  if (weed_layer_get_palette(layer) == WEED_PALETTE_NONE) {
#ifdef GUI_GTK
    if (nchannels == 4) weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
    else weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
#endif
  }

  if (rowstride == get_last_pixbuf_rowstride_value(width, nchannels)) {
    in_pixel_data = (void *)lives_pixbuf_get_pixels(pixbuf);
    weed_layer_pixel_data_free(layer);
    weed_layer_set_pixel_data(layer, in_pixel_data);
    weed_set_voidptr_value(layer, LIVES_LEAF_PIXBUF_SRC, pixbuf);
    palette = weed_layer_get_palette(layer);
    if (weed_palette_is_rgb(palette)) weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);
    return TRUE;
  }

  framesize = rowstride * height;
  pixel_data = lives_calloc(framesize >> 5, 32);

  if (pixel_data) {
    in_pixel_data = (void *)lives_pixbuf_get_pixels_readonly(pixbuf);
    lives_memcpy(pixel_data, in_pixel_data, rowstride * (height - 1));
    // this part is needed because layers always have a memory size height*rowstride, whereas gdkpixbuf can have
    // a shorter last row
    lives_memcpy((uint8_t *)pixel_data + rowstride * (height - 1), (uint8_t *)in_pixel_data + rowstride * (height - 1),
                 get_last_pixbuf_rowstride_value(width, nchannels));
  }

  weed_layer_pixel_data_free(layer);
  weed_layer_set_pixel_data(layer, pixel_data);

  palette = weed_layer_get_palette(layer);
  if (weed_palette_is_rgb(palette)) weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

  return FALSE;
}


LIVES_LOCAL_INLINE int swap_red_blue(int pal) {
  if (pal == WEED_PALETTE_RGB24) return WEED_PALETTE_BGR24;
  if (pal == WEED_PALETTE_RGBA32) return WEED_PALETTE_BGRA32;
  if (pal == WEED_PALETTE_BGR24) return WEED_PALETTE_RGB24;
  if (pal == WEED_PALETTE_BGRA32) return WEED_PALETTE_RGBA32;
  return WEED_PALETTE_END;
}

/**
   @brief look for shortcuts in palette conversions
   instead of converting e.g RGB -> BGRA, we may be able to pretend that the
   input palette is BGR and thus the conversion to BGRA is slightly cheaper
   we can do this provided the caller can take into account that the red / blue
   components are now swapped.
   If this is possible, then either inpal or outpal mau ne altered, and TRUE is returned
*/
boolean consider_swapping(int *inpal, int *outpal) {
  if (*inpal == *outpal) return FALSE;
  if (!weed_palette_is_rgb(*inpal) || !weed_palette_is_rgb(*outpal)) return FALSE;
  switch (*inpal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_RGBA32:
    switch (*outpal) {
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_BGRA32:
      *inpal = swap_red_blue(*inpal);
      return TRUE;
    default: return FALSE;
    }
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_BGRA32:
    switch (*outpal) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_ARGB32:
      *inpal = swap_red_blue(*inpal);
      return TRUE;
    default: return FALSE;
    }
  case WEED_PALETTE_ARGB32:
    /// since we dont have ABGR, we can switch the out palette instead
    switch (*outpal) {
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_BGRA32:
      *outpal = swap_red_blue(*outpal);
      return TRUE;
    default: return FALSE;
    }
  default:
    break;
  }
  return FALSE;
}


#ifdef GUI_GTK

/**
   @brief convert a weed layer to lives_painter (a.k.a cairo)

   width, height and rowstrides of source layer may all change */
lives_painter_t *layer_to_lives_painter(weed_layer_t *layer) {
  lives_painter_surface_t *surf;
  lives_painter_t *cairo;
  lives_painter_format_t cform;
  int lform;
  uint8_t *src, *dst, *pixel_data;

  int irowstride, orowstride;
  int width, widthx;
  int height, pal;

  int i;

  if (weed_plant_has_leaf(layer, LIVES_LEAF_SURFACE_SRC)) {
    surf = (lives_painter_surface_t *)weed_get_voidptr_value(layer, LIVES_LEAF_SURFACE_SRC, NULL);
  } else {
    pal = weed_layer_get_palette(layer);
    width = weed_layer_get_width_pixels(layer);
    if (pal == WEED_PALETTE_A8) {
      cform = LIVES_PAINTER_FORMAT_A8;
      widthx = width;
    } else if (pal == WEED_PALETTE_A1) {
      cform = LIVES_PAINTER_FORMAT_A1;
      widthx = width >> 3;
    } else {
      lform = LIVES_PAINTER_COLOR_PALETTE(capable->hw.byte_order);
      convert_layer_palette(layer, lform, 0);
      cform = LIVES_PAINTER_FORMAT_ARGB32;
      widthx = width << 2;
    }

    height = weed_layer_get_height(layer);
    irowstride = weed_layer_get_rowstride(layer);
    orowstride = lives_painter_format_stride_for_width(cform, width);
    src = (uint8_t *)weed_layer_get_pixel_data(layer);

    if (irowstride == orowstride && !weed_plant_has_leaf(layer, LIVES_LEAF_PIXBUF_SRC) &&
        !weed_plant_has_leaf(layer, WEED_LEAF_HOST_ORIG_PDATA)) {
      pixel_data = src;
    } else {
      dst = pixel_data = (uint8_t *)lives_calloc(1, height * orowstride);
      if (!pixel_data) return NULL;
      for (i = 0; i < height; i++) {
        lives_memcpy(&dst[orowstride * i], &src[irowstride * i], widthx);
      }
      weed_layer_pixel_data_free(layer);
      weed_layer_set_pixel_data(layer, pixel_data);
      weed_layer_set_rowstride(layer, orowstride);
    }

    if (weed_palette_has_alpha(pal)) {
      int flags = weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);
      if (!(flags & WEED_LAYER_ALPHA_PREMULT)) {
        // if we have post-multiplied alpha, pre multiply
        alpha_unpremult(layer, FALSE);
        flags |= WEED_LAYER_ALPHA_PREMULT;
        weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, flags);
      }
    }
    surf = lives_painter_image_surface_create_for_data(pixel_data, cform, width, height, orowstride);
  }
  if (!surf) return NULL;

  cairo = lives_painter_create_from_surface(surf); // surf is refcounted
#ifdef DEBUG_CAIRO_SURFACE
  g_print("VALaa1 = %d %p\n", cairo_surface_get_reference_count(surf), surf);
#endif
  weed_layer_set_pixel_data(layer, lives_painter_image_surface_get_data(surf));
  weed_set_voidptr_value(layer, LIVES_LEAF_SURFACE_SRC, surf);
  return cairo;
}


/**
   @brief convert a lives_painter_t (a.k.a) cairo_t to a weed layer */
boolean lives_painter_to_layer(lives_painter_t *cr, weed_layer_t *layer) {
  // updates a weed_layer from a cr
  void *src;
  lives_painter_surface_t *surface = lives_painter_get_target(cr), *xsurface = NULL;
  lives_painter_format_t cform;

  int width, height, rowstride;

  /// flush to ensure all writing to the image surface was done
  lives_painter_surface_flush(surface);

  if (weed_plant_has_leaf(layer, LIVES_LEAF_SURFACE_SRC)) {
    xsurface = (lives_painter_surface_t *)weed_get_voidptr_value(layer, LIVES_LEAF_SURFACE_SRC, NULL);
  }
  if (xsurface != surface) weed_layer_pixel_data_free(layer);

  src = lives_painter_image_surface_get_data(surface);

  weed_layer_set_pixel_data(layer, src);
  weed_set_voidptr_value(layer, LIVES_LEAF_SURFACE_SRC, surface);

#ifdef DEBUG_CAIRO_SURFACE
  g_print("VALaa2 = %d %p\n", cairo_surface_get_reference_count(surface), surface);
#endif
  lives_painter_surface_reference(surface);
  lives_painter_destroy(cr);

  width = lives_painter_image_surface_get_width(surface);
  height = lives_painter_image_surface_get_height(surface);
  rowstride = lives_painter_image_surface_get_stride(surface);

  weed_layer_set_rowstride(layer, rowstride);
  weed_layer_set_size(layer, width, height);

  cform = lives_painter_image_surface_get_format(surface);

  switch (cform) {
  case LIVES_PAINTER_FORMAT_ARGB32:
    weed_layer_set_palette(layer, LIVES_PAINTER_COLOR_PALETTE(capable->hw.byte_order));
    weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

    if (prefs->alpha_post) {
      /// un-premultiply the alpha
      alpha_unpremult(layer, TRUE);
    } else {
      int flags = weed_layer_get_flags(layer);
      flags |= WEED_LAYER_ALPHA_PREMULT;
      weed_layer_set_flags(layer, flags);
    }
    break;

  case LIVES_PAINTER_FORMAT_A8:
    weed_layer_set_palette(layer, WEED_PALETTE_A8);
    break;

  case LIVES_PAINTER_FORMAT_A1:
    weed_layer_set_palette(layer, WEED_PALETTE_A1);
    break;

  default:
    break;
  }

  return TRUE;
}

#endif


int resize_all(int fileno, int width, int height, lives_img_type_t imgtype, boolean do_back, int *nbad, int *nmiss) {
  LiVESPixbuf *pixbuf = NULL;
  LiVESError *error = NULL;
  lives_clip_t *sfile;
  lives_img_type_t ximgtype;
  weed_layer_t *layer;
  char *fname;
  short pbq = prefs->pb_quality;
  boolean intimg = FALSE;
  int miss = 0, bad = 0;
  int nimty = (int)N_IMG_TYPES;
  int j, nres = 0;

  mainw->cancelled = CANCEL_NONE;
  if (!IS_VALID_CLIP(fileno)) return 0;
  sfile = mainw->files[fileno];
  prefs->pb_quality = PB_QUALITY_BEST;

#ifdef USE_LIBPNG
  // use internal image saver if we can
  if (sfile->img_type == IMG_TYPE_PNG) intimg = TRUE;
#endif

  pthread_mutex_lock(&sfile->frame_index_mutex);
  for (int i = 0; i < sfile->frames; i++) {
    threaded_dialog_spin((double)i / (double)sfile->frames);
    if (mainw->cancelled) {
      pthread_mutex_unlock(&sfile->frame_index_mutex);
      prefs->pb_quality = pbq;
      return nres;
    }
    if (sfile->frame_index && sfile->frame_index[i] != -1) continue;
    ximgtype = imgtype;
    fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(ximgtype));
    if (!lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      // check all img_types
      for (j = 1; j < nimty; j++) {
        ximgtype = (lives_img_type_t)j;
        if (ximgtype == imgtype) continue;
        lives_free(fname);
        fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(ximgtype));
        if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) break;
      }
      if (j == nimty) {
        miss++;
        lives_free(fname);
        continue;
      } else bad++;
    }
    layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
    weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, LIVES_LAYER_LOAD_IF_NEEDS_RESIZE);
    if (!weed_layer_create_from_file_progressive(layer, fname, width, height, WEED_PALETTE_END,
        get_image_ext_for_type(ximgtype))) {
      lives_free(fname);
      miss++;
      continue;
    }

    if (weed_layer_get_width(layer) == width
        && weed_layer_get_height(layer) == height) {
      weed_layer_free(layer);
      lives_free(fname);
      continue;
    }

    if (!resize_layer(layer, width, height, LIVES_INTERP_BEST, WEED_PALETTE_END,
                      WEED_YUV_CLAMPING_UNCLAMPED)) {
      weed_layer_free(layer);
      lives_free(fname);
      continue;
    }
    if (!intimg) {
      pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);
      weed_layer_free(layer);
    }
    if (intimg || pixbuf) {
      if (do_back) {
        char *fname_bak = make_image_file_name(sfile, i + 1, LIVES_FILE_EXT_BAK);
        if (lives_file_test(fname_bak, LIVES_FILE_TEST_EXISTS)) lives_rm(fname_bak);
        lives_mv(fname, fname_bak);
      }
      if (!intimg) {
        lives_pixbuf_save(pixbuf, fname, ximgtype, 100 - prefs->ocp, width, height, &error);
        lives_widget_object_unref(pixbuf);
      } else {
        save_to_png(layer, fname, 100 - prefs->ocp);
        weed_layer_free(layer);
      }
      if (error || THREADVAR(write_failed)) {
        THREADVAR(write_failed) = 0;
        if (error) {
          lives_error_free(error);
          error = NULL;
        }
        lives_free(fname);
        miss++;
        continue;
      }
      nres++;
    }
    lives_free(fname);
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  prefs->pb_quality = pbq;
  if (nbad) *nbad = bad;
  if (nmiss) *nmiss = miss;
  return nres;
}


//uint64_t *hash_img_rows(int clipno, frames_t frame, int *pheight) {
// with crows NULL, creates row hash and returns it
// with crows non NULL, compares row by row, on diff. returns NULL, else returns crows
uint64_t *hash_cmp_rows(uint64_t *crows, int clipno, frames_t frame) {
  if (CLIP_HAS_VIDEO(clipno)) {
    uint64_t *hashes = NULL;
    weed_timecode_t tc;
    lives_clip_t *sfile = mainw->files[clipno];
    weed_layer_t *layer;
    if (frame > sfile->frames) return NULL;
    layer = lives_layer_new_for_frame(clipno, frame);
    tc = ((frame - 1.)) / sfile->fps * TICKS_PER_SECOND;
    if (pull_frame_at_size(layer, get_image_ext_for_type(sfile->img_type),
                           tc, sfile->hsize, sfile->vsize, WEED_PALETTE_RGB24)) {
      uint8_t *pd = weed_layer_get_pixel_data(layer);
      int32_t width = weed_layer_get_width(layer) * 3;
      uint32_t height = weed_layer_get_height(layer);
      int row = weed_layer_get_rowstride(layer);

      if (height > MAX_FRAME_HEIGHT) height = MAX_FRAME_HEIGHT;
      if (!crows) hashes = lives_calloc(height, 8);

      if (crows || hashes) {
        for (int i = 0; i < height; i++) {
          if (!crows) hashes[i] = lives_bin_hash(&pd[row * i], width);
          else {
            if (crows[i] != lives_bin_hash(&pd[row * i], width)) {
              weed_layer_free(layer);
              return NULL;
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*
    if (layer) weed_layer_free(layer);
    if (!crows) return hashes;
    return crows;
  }
  return NULL;
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
      if (weed_leaf_set_flags(layer, WEED_LEAF_PIXEL_DATA,
                              weed_leaf_get_flags(slayer, WEED_LEAF_PIXEL_DATA)));
    } else weed_layer_nullify_pixel_data(layer);
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


/**
   @brief free pixel_data from layer

   we do not free if WEED_LEAF_HOST_ORIG_PDATA is set (data is an alpha in which "belongs" to another out param)

   take care of LIVES_LEAF_PIXEL_DATA_CONTIGUOUS
   take care of LIVES_LEAF_PIXBUF_SRC
   take care of LIVES_LEAF_SURFACE_SRC

   sets WEED_LEAF_PIXEL_DATA to NULL for the layer

   this function should always be used to free WEED_LEAF_PIXEL_DATA */
void weed_layer_pixel_data_free(weed_layer_t *layer) {
  void **pixel_data;
  int pd_elements;

  if (!layer) return;

  if (weed_leaf_get_flags(layer, WEED_LEAF_PIXEL_DATA) & LIVES_FLAG_MAINTAIN_VALUE)
    return;

  if (weed_get_boolean_value(layer, WEED_LEAF_HOST_ORIG_PDATA, NULL) == WEED_TRUE)
    return;

  weed_leaf_delete(layer, WEED_LEAF_NATURAL_SIZE);

  if (weed_get_boolean_value(layer, LIVES_LEAF_BBLOCKALLOC, NULL) == WEED_TRUE) {
    weed_leaf_delete(layer, LIVES_LEAF_BBLOCKALLOC);
    free_bigblock(weed_layer_get_pixel_data(layer));
    weed_layer_nullify_pixel_data(layer);
  } else {
    if ((pixel_data = weed_layer_get_pixel_data_planar(layer, &pd_elements)) != NULL) {
      if (pd_elements > 0) {
        if (weed_plant_has_leaf(layer, LIVES_LEAF_PIXBUF_SRC)) {
          LiVESPixbuf *pixbuf = (LiVESPixbuf *)weed_get_voidptr_value(layer, LIVES_LEAF_PIXBUF_SRC, NULL);
          if (pixbuf) lives_widget_object_unref(pixbuf);
        } else {
          if (weed_plant_has_leaf(layer, LIVES_LEAF_SURFACE_SRC)) {
            lives_painter_surface_t *surface = (lives_painter_surface_t *)weed_get_voidptr_value(layer,
                                               LIVES_LEAF_SURFACE_SRC, NULL);
            if (surface) {
              // this is where most surfaces die, as we convert from BGRA -> RGB
              uint8_t *pdata = lives_painter_image_surface_get_data(surface);
#ifdef DEBUG_CAIRO_SURFACE
              g_print("VALaa23rrr = %d %p\n", cairo_surface_get_reference_count(surface), surface);
#endif
              // call twice to remove our extra ref.
              lives_painter_surface_destroy(surface);
              lives_painter_surface_destroy(surface);
              lives_free(pdata);
            }
          } else {
            if (weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, NULL) == WEED_TRUE) {
              pd_elements = 1;
            }
            for (int i = 0; i < pd_elements; i++) {
              if (pixel_data[i]) lives_free(pixel_data[i]);
            }
          }
        }
        lives_free(pixel_data);
      }
    }
  }
  weed_layer_nullify_pixel_data(layer);
}


/**
   @brief frees pixel_data for a layer, then the layer itself

   if plant is freed
   returns (void *)NULL for convenience
*/

LIVES_GLOBAL_INLINE weed_layer_t *weed_layer_free(weed_layer_t *layer) {
  if (layer) weed_layer_unref(layer);
  return NULL;
}

int weed_layer_unref(weed_layer_t *layer) {
  int refs = weed_refcount_dec(layer);
  if (refs == -100) lives_abort("layer missing REF_COUNT_MUTEX in weed_layer_unref");
  if (refs != -1) return refs;
  weed_layer_pixel_data_free(layer);
  weed_refcounter_unlock(layer);
  weed_plant_free(layer);
  return refs;
}

LIVES_GLOBAL_INLINE int weed_layer_ref(weed_layer_t *layer) {
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


void test_conv(int clamping, int subspace, uint8_t r, uint8_t g, uint8_t b) {
  //#define TEST_CONV
#ifdef TEST_CONV
  for (int64_t f = 3000; f < 8000; f += 1) {
    struct XYZ xyz = xyzFromWavelength((double)f);
    g_print("freq %ld is %f, %f, %f\n", f, xyz.x * 100., xyz.y * 100., xyz.z * 100.);
  }
  if (1) {
    //int cr, cg, cb;
    //uint8_t r = 100, g, b, y, u, v, xr, xg, xb;;
    set_conversion_arrays(WEED_YUV_CLAMPING_UNCLAMPED, WEED_YUV_SUBSPACE_YCBCR);
    //prefs->pb_quality = PB_QUALITY_MED;
    for (cr = 100; cr < 256; cr++) {
      g = 0;
      for (cg = 0; cg < 256; cg++) {
        b = 0;
        for (cb = 0; cb < 256; cb++) {
          g_print("in: %d %d %d\n", r, g, b);
          rgb2yuv(r, g, b, &y, &u, &v);
          yuv2rgb(y, u, v, &xr, &xg, &xb);
          g_print("out: %d %d %d   %d %d %d\n", y, u, v, xr, xg, xb);
          b++;
        }
        g++;
      }
      r++;
    }
  }
#endif
}
