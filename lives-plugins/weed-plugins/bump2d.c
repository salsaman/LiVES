// Based on code Copyright (C) 2002 W.P. van Paassen - peter@paassen.tmfweb.nl
// This effect was inspired by an article by Sqrt(-1) */
// 08-22-02 Optimized by WP
// bump2d.c - weed plugin
// (c) G. Finch (salsaman) 2006
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define NEED_PALETTE_CONVERSIONS
#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////
#include <math.h>

static short aSin[512];
static uint8_t reflectionmap[256][256];

typedef struct {
  uint16_t sin_index;
  uint16_t sin_index2;
} _sdata;

typedef struct {
  short x, y;
} BUMP;


static void bumpmap_setup(void) {
  int i;
  short x, y;
  float rad;

  /*create sin lookup table */
  // this is for the lissajous movement of the center point
  for (i = 0; i < 512; i++) {
    rad = (float)i * 0.0174532 * 0.703125;
    aSin[i] = (short)((sin(rad) * 100.0));
  }

  /* create reflection map */
  for (x = 0; x < 256; ++x) {
    for (y = 0; y < 256; ++y) {
      float X = (x - 128) / 128.0;
      float Y = (y - 128) / 128.0;
      // Z is the height of an elipsoid centered at (128, 128) with radii minor = 128, major = 256
      // this is projected on an edge-detected version of the original
      float Z =  1.0 - sqrt(X * X + Y * Y);
      Z *= 255.0;
      if (Z < 0.0) Z = 0.0;
      reflectionmap[x][y] = Z;
    }
  }
}


static weed_error_t bumpmap_init(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->sin_index = 0;
  sdata->sin_index2 = 80;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t bumpmap_deinit(weed_plant_t *inst) {
  _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }

  return WEED_SUCCESS;
}


static weed_error_t bumpmap_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst,
                               WEED_LEAF_OUT_CHANNELS,
                               NULL);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL), *isrc = src;
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);

  int inplace = (src == dst);

  if (height == 0 || width == 0 || dst == NULL || src == NULL) return WEED_SUCCESS;
  else {
    int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
    int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
    int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
    int yuv_clamping = weed_get_int_value(in_channel, "yuv_clamping", NULL);
    int psize = pixel_size(palette);
    int widthx = width * psize;
    int offs = palette == WEED_PALETTE_ARGB32 ? 1 : 0;
    _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

    uint16_t lightx, lighty, temp;
    short normalx, normaly, x, y, xx;

    float aspect = (float)width / (float)height;

    BUMP bumpmap[width][height];

    src += irowstride;

    /* create bump map from src*/
    for (y = 1; y < height - 1; y++) {
      xx = 1;
      for (x = 1; x < widthx - psize * 2; x += psize) {
        bumpmap[xx][y].x = calc_luma(&src[x + psize], palette, yuv_clamping) - calc_luma(&src[x], palette, yuv_clamping);
        bumpmap[xx][y].y = calc_luma(&src[x], palette, yuv_clamping) - calc_luma(&src[- irowstride + x], palette, yuv_clamping);
        xx++;
      }
      src += irowstride;
    }

    // position of center; this is the lissajous coords scaled to the image center and bubble center
    lightx = aSin[sdata->sin_index] / 100. * width / 2. + width / 2. + 128;
    lighty = aSin[sdata->sin_index2] / 100. * height / 2.  + height / 2 + 128. / aspect;

    src = isrc;

    blank_row(&dst, width, palette, yuv_clamping, 1, &src);
    dst += orowstride;
    src += irowstride;

    orowstride -= widthx - psize;
    irowstride -= widthx - psize;

    for (y = 1; y < height - 1; y++) {
      temp = lighty - y;
      blank_pixel(dst, palette, yuv_clamping, src);
      dst += psize;
      src += psize;

      for (x = 1; x < width - 1; x++) {
        normalx = bumpmap[x][y].x + (lightx - x) / aspect;
        normaly = bumpmap[x][y].y + temp;

        if (normalx < 0 || normalx > 255)
          normalx = 0;
        if (normaly < 0 || normaly > 255)
          normaly = 0;
        if (palette == WEED_PALETTE_YUV888 || palette == WEED_PALETTE_YUVA8888) {
          if (yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED)
            *dst = reflectionmap[normalx][normaly];
          else *dst = y_unclamped_to_clamped(reflectionmap[normalx][normaly]);
          dst[1] = dst[2] = 128;
        } else {
          if (palette == WEED_PALETTE_ARGB32) dst[0] = src[0];
          weed_memset(dst + offs, reflectionmap[normalx][normaly], 3);
        }
        if (!inplace && (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32
                         || palette == WEED_PALETTE_YUVA8888)) dst[3] = src[3];
        dst += psize;
        src += psize;
      }
      blank_pixel(dst, palette, yuv_clamping, src);
      dst += orowstride;
      src += irowstride;
    }

    blank_row(&dst, width, palette, yuv_clamping, 1, &src);

    sdata->sin_index += 3;
    sdata->sin_index &= 511;
    sdata->sin_index2 += 5;
    sdata->sin_index2 &= 511;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("bumpmap", "salsaman", 1, WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                               bumpmap_init, bumpmap_process, bumpmap_deinit,
                               in_chantmpls,
                               out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
  bumpmap_setup();
}
WEED_SETUP_END;
