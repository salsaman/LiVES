/*
   EffecTV - Realtime Digital Video Effector
   Copyright (C) 2001 FUKUCHI Kentarou

   FireTV - clips incoming objects and burn them.
   Copyright (C) 2001 FUKUCHI Kentarou

   Fire routine is taken from Frank Jan Sorensen's demo program.
*/

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <math.h>

//////////////////////////////////////////////

// these effecTV plugins are a nightmare, they use (unsigned int)RGBA as a format
// but it's actually: ARGB (!), meaning on little-endian systems the input palette is BGRA
// and on big-endian it would be ARGB
// they also totally ignore rowstrides

typedef unsigned int RGB32;
static RGB32 palette[256];

#define MaxColor 120
#define Decay 15
#define MAGIC_THRESHOLD 50

struct _sdata {
  unsigned char *buffer;
  short *background;
  unsigned char *diff;
  int threshold;
  uint64_t fv, fv2;
};


//////////////////////////////////////////////////////

static void HSItoRGB(double H, double S, double I, int *r, int *g, int *b) {
  double T, Rv, Gv, Bv;
  T = H;
  Rv = 1 + S * sin(T - 2 * M_PI / 3);
  Gv = 1 + S * sin(T);
  Bv = 1 + S * sin(T + 2 * M_PI / 3);
  T = 255.1009 * I / 2;
  *r = trunc(Rv * T);
  *g = trunc(Gv * T);
  *b = trunc(Bv * T);
}

static void makePalette() {
  int i, r, g, b;

  for (i = 0; i < MaxColor; i++) {
    // uses some weird HSI formula to make 0 - 119
    HSItoRGB(4.6 - 1.5 * i / MaxColor, (double)i / MaxColor, (double)i / MaxColor, &r, &g, &b);
    palette[i] = ((r << 16) | (g << 8) | b) & 0xffffff; // palette is (A)RGB
  }
  for (i = MaxColor; i < 256; i++) {
    // and then for 120 - 255, r increases by 3 while g,b increase by 2
    // ...meaning r gets saturated at least by 200, and g and b at least by 250
    if (r < 255)r++;
    if (r < 255)r++;
    if (r < 255)r++;
    if (g < 255)g++;
    if (g < 255)g++;
    if (b < 255)b++;
    if (b < 255)b++;
    palette[i] = ((r << 16) | (g << 8) | b) & 0xffffff;
  }
}


static void image_bgsubtract_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  register int i, j;
  int R, G, B;
  RGB32 *p;
  short *q;
  unsigned char *r;
  int v;

  rowstride -= width;

  p = src;
  q = sdata->background;
  r = sdata->diff;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      R = ((*p) & 0xff0000) >> (16 - 1);
      G = ((*p) & 0xff00) >> (8 - 2);
      B = (*p) & 0xff;
      v = (R + G + B) - (int)(*q);
      *q = (short)(R + G + B);
      *r = ((v + sdata->threshold) >> 24) | ((sdata->threshold - v) >> 24);
      p++;
      q++;
      r++;
    }
    p += rowstride;
  }


  /* The origin of subtraction function is;
     diff(src, dest) = (abs(src - dest) > threshold) ? 0xff : 0;

     This functions is transformed to;
     (threshold > (src - dest) > -threshold) ? 0 : 0xff;

     (v + threshold)>>24 is 0xff when v is less than -threshold.
     (v - threshold)>>24 is 0xff when v is less than threshold.
     So, ((v + threshold)>>24) | ((threshold - v)>>24) will become 0xff when
     abs(src - dest) > threshold.
  */
}


static weed_error_t fire_init(weed_plant_t *inst) {
  int error;
  int map_h;
  int map_w;
  struct _sdata *sdata;

  weed_plant_t *in_channel;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error);

  map_h = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);
  map_w = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error);

  sdata->buffer = (unsigned char *)weed_calloc(map_h * map_w, sizeof(unsigned char));
  if (sdata->buffer == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->background = (short *)weed_calloc(map_h * map_w, sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->buffer);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->diff = (unsigned char *)weed_calloc(map_h * map_w, sizeof(unsigned char));
  if (sdata->diff == NULL) {
    weed_free(sdata->background);
    weed_free(sdata->buffer);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->threshold = MAGIC_THRESHOLD * 7;

  sdata->fv = fastrand(0);
  sdata->fv2 = fastrand(0);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t fire_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) {
    weed_free(sdata->buffer);
    weed_free(sdata->diff);
    weed_free(sdata->background);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t fire_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;

  unsigned char v;
  weed_plant_t *in_channel, *out_channel;

  RGB32 *src, *dest;

  int width, height, irow, orow;
  int video_area;
  int error;
  register int i, x, y;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error);
  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, &error);

  src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, &error);
  dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, &error);

  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);

  irow = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, &error) / 4;
  orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error) / 4;

  video_area = width * height;

  sdata->fv = fastrand(sdata->fv2);
  sdata->fv2 = fastrand(sdata->fv);

  image_bgsubtract_y(src, width, height, irow, sdata);

  for (i = 0; i < video_area - width; i++) {
    sdata->buffer[i] |= sdata->diff[i];
  }
  for (x = 1; x < width - 1; x++) {
    i = width + x;
    for (y = 1; y < height; y++) {
      v = sdata->buffer[i];
      if (v < Decay)
        sdata->buffer[i - width] = 0;
      else {
        sdata->fv = fastrand(sdata->fv);
        sdata->fv2 = fastrand(sdata->fv2);
        sdata->buffer[i - width + (sdata->fv & 0xFFFF) % 3 - 1] = v - ((sdata->fv2 & 0xFFFF) & Decay);
      }
      i += width;
    }
  }
  for (y = 0; y < height; y++) {
    for (x = 1; x < width - 1; x++) {
      dest[y * orow + x] = (src[y * irow + x] & 0xff000000) | palette[sdata->buffer[y * width + x]];
    }
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGRA32, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("fireTV", "effectTV", 1, 0, palette_list,
                               fire_init, fire_process, fire_deinit, in_chantmpls,
                               out_chantmpls,
                               NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);

  makePalette();
}
WEED_SETUP_END;

