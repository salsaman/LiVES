/*
   EffecTV - Realtime Digital Video Effector
   Copyright (C) 2001 FUKUCHI Kentarou

   LifeTV - Play John Horton Conway's `Life' game with video input.
   Copyright (C) 2001 FUKUCHI Kentarou

   This idea is stolen from Nobuyuki Matsushita's installation program of
   ``HoloWall''. (See http://www.csl.sony.co.jp/person/matsu/index.html)
*/

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <strings.h>

typedef unsigned int RGB32;

#define PIXEL_SIZE (sizeof(RGB32))

struct _sdata {
  unsigned char *field;
  unsigned char *field1;
  unsigned char *field2;
  short *background;
  unsigned char *diff;
  unsigned char *diff2;
  int threshold;
};


/* Background image is refreshed every frame */
static void image_bgsubtract_update_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
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
}


/* noise filter for subtracted image. */
void image_diff_filter(struct _sdata *sdata, int width, int height) {
  int x, y;
  unsigned char *src, *dest;
  unsigned int count;
  unsigned int sum1, sum2, sum3;

  src = sdata->diff;
  dest = sdata->diff2 + width + 1;
  for (y = 1; y < height - 1; y++) {
    sum1 = src[0] + src[width] + src[width * 2];
    sum2 = src[1] + src[width + 1] + src[width * 2 + 1];
    src += 2;
    for (x = 1; x < width - 1; x++) {
      sum3 = src[0] + src[width] + src[width * 2];
      count = sum1 + sum2 + sum3;
      sum1 = sum2;
      sum2 = sum3;
      *dest++ = (0xff * 3 - count) >> 24;
      src++;
    }
    dest += 2;
  }
}

/////////////////////////////////////////////////////////////

static weed_error_t lifetv_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int height, width, video_area;
  weed_plant_t *in_channel;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);

  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);

  video_area = width * height;

  sdata->field = (unsigned char *)weed_calloc(video_area, 2);
  if (sdata->field == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->diff = (unsigned char *)weed_malloc(video_area * sizeof(unsigned char));

  if (sdata->diff == NULL) {
    weed_free(sdata->field);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->diff2 = (unsigned char *)weed_malloc(video_area * sizeof(unsigned char));

  if (sdata->diff2 == NULL) {
    weed_free(sdata->diff);
    weed_free(sdata->field);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }


  sdata->background = (short *)weed_malloc(video_area * sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->field);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  if (sdata->background == NULL) {
    weed_free(sdata->field);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->diff, 0, video_area * sizeof(unsigned char));

  sdata->threshold = 280;
  sdata->field1 = sdata->field;
  sdata->field2 = sdata->field + video_area;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


int lifetv_deinit(weed_plant_t *inst) {
  struct _sdata *sdata;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    weed_free(sdata->background);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata->field);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


int lifetv_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;

  weed_plant_t *in_channel, *out_channel;

  unsigned char *p, *q, v;
  unsigned char sum, sum1, sum2, sum3;

  RGB32 *src, *dest;

  RGB32 pix;

  int width, height, video_area, irow, orow;

  register int x, y;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);

  irow = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL) / 4;
  orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL) / 4 - width;

  video_area = width * height;

  image_bgsubtract_update_y(src, width, height, irow, sdata);
  image_diff_filter(sdata, width, height);
  p = sdata->diff2;

  irow -= width;

  for (x = 0; x < video_area; x++) {
    sdata->field1[x] |= p[x];
  }

  p = sdata->field1 + 1;
  q = sdata->field2 + width + 1;
  dest += width + 1;
  src += width + 1;
  /* each value of cell is 0 or 0xff. 0xff can be treated as -1, so
     following equations treat each value as negative number. */
  for (y = 1; y < height - 1; y++) {
    sum1 = 0;
    sum2 = p[0] + p[width] + p[width * 2];
    for (x = 1; x < width - 1; x++) {
      sum3 = p[1] + p[width + 1] + p[width * 2 + 1];
      sum = sum1 + sum2 + sum3;
      v = 0 - ((sum == 0xfd) | ((p[width] != 0) & (sum == 0xfc)));
      *q++ = v;
      pix = (signed char)v;
      //			pix = pix >> 8;
      *dest++ = pix | *src++;
      sum1 = sum2;
      sum2 = sum3;
      p++;
    }
    p += 2;
    q += 2;
    src += 2 + irow;
    dest += 2 + orow;
  }
  p = sdata->field1;
  sdata->field1 = sdata->field2;
  sdata->field2 = p;

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("lifeTV", "effectTV", 1, 0, palette_list,
                               lifetv_init, lifetv_process, lifetv_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


