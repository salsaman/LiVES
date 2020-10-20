/*
   EffecTV - Realtime Digital Video Effector
   Copyright (C) 2001-2005 FUKUCHI Kentaro

   RadioacTV - motion-enlightment effect.
   Copyright (C) 2001-2002 FUKUCHI Kentaro

   I referred to "DUNE!" by QuoVadis for this effect.
*/

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#define COLORS 32
#define PATTERN 4
#define MAGIC_THRESHOLD 40
#define RATIO 0.95

typedef unsigned int RGB32;

#define PIXEL_SIZE (sizeof(RGB32))

/////////////////////////////////

typedef struct _sdata {
  unsigned char *blurzoombuf;
  int *blurzoomx;
  int *blurzoomy;
  RGB32 *snapframe;
  int buf_width;
  int buf_height;
  int buf_width_blocks;  // block width is 32
  int buf_margin_right;
  int buf_margin_left;
  short *background;
  unsigned char *diff;
  int snapTime;
  int snapInterval;
  int threshold;
} sdata;

static RGB32 *palette;
static RGB32 palettes[256];

#define VIDEO_HWIDTH (buf_width/2)
#define VIDEO_HHEIGHT (buf_height/2)


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


/* this table assumes that video_width is times of 32 */
static void setTable(struct _sdata *sdata) {
  unsigned int bits;
  int x, y, tx, ty, xx;
  int ptr, prevptr;
  int buf_width = sdata->buf_width;
  int buf_height = sdata->buf_height;

  prevptr = (int)(0.5 + RATIO * (-VIDEO_HWIDTH) + VIDEO_HWIDTH);
  for (xx = 0; xx < (sdata->buf_width_blocks); xx++) {
    bits = 0;
    for (x = 0; x < 32; x++) {
      ptr = (int)(0.5 + RATIO * (xx * 32 + x - VIDEO_HWIDTH) + VIDEO_HWIDTH);
#ifdef USE_NASM
      bits = bits << 1;
      if (ptr != prevptr)
        bits |= 1;
#else
      bits = bits >> 1;
      if (ptr != prevptr)
        bits |= 0x80000000;
#endif /* USE_NASM */
      prevptr = ptr;
    }
    sdata->blurzoomx[xx] = bits;
  }

  ty = (int)(0.5 + RATIO * (-VIDEO_HHEIGHT) + VIDEO_HHEIGHT);
  tx = (int)(0.5 + RATIO * (-VIDEO_HWIDTH) + VIDEO_HWIDTH);
  xx = (int)(0.5 + RATIO * (buf_width - 1 - VIDEO_HWIDTH) + VIDEO_HWIDTH);
  sdata->blurzoomy[0] = ty * buf_width + tx;
  prevptr = ty * buf_width + xx;
  for (y = 1; y < buf_height; y++) {
    ty = (int)(0.5 + RATIO * (y - VIDEO_HHEIGHT) + VIDEO_HHEIGHT);
    sdata->blurzoomy[y] = ty * buf_width + tx - prevptr;
    prevptr = ty * buf_width + xx;
  }
}


#ifndef USE_NASM
/* following code is a replacement of blurzoomcore.nas. */
static void blur(struct _sdata *sdata) {
  register int x, y;
  int width;
  register unsigned char *p, *q;
  register unsigned char v;

  width = sdata->buf_width;
  p = sdata->blurzoombuf + width + 1;
  q = p + sdata->buf_width * sdata->buf_height;

  for (y = sdata->buf_height - 2; y > 0; y--) {
    for (x = width - 2; x > 0; x--) {
      if ((v = (*(p - width) + * (p - 1) + * (p + 1) + * (p + width)) / 4 - 1) == 255) v = 0;
      *(q++) = v;
      p++;
    }
    p += 2;
    q += 2;
  }
}


static void zoom(struct _sdata *sdata) {
  register int b, x, y;
  register unsigned char *p, *q;
  int blocks, height;
  register int dx;

  p = sdata->blurzoombuf + sdata->buf_width * sdata->buf_height;
  q = sdata->blurzoombuf;
  height = sdata->buf_height;
  blocks = sdata->buf_width_blocks;

  for (y = 0; y < height; y++) {
    p += sdata->blurzoomy[y];
    for (b = 0; b < blocks; b++) {
      dx = sdata->blurzoomx[b];
      for (x = 0; x < 32; x++) {
        p += (dx & 1);
        *q++ = *p;
        dx = dx >> 1;
      }
    }
  }
}


static inline void blurzoomcore(struct _sdata *sdata) {
  blur(sdata);
  zoom(sdata);
}
#endif /* USE_NASM */

static void makePalette(int pal) {
  int i;

#define DELTA (255/(COLORS/2-1))

  for (i = 0; i < 256; i++) {
    palettes[i] = 0;
  }

  for (i = 0; i < COLORS / 2; i++) {
    if (pal == WEED_PALETTE_RGBA32) {
      palettes[i] = (i * DELTA) << 16;
      palettes[COLORS * 2 + i] = i * DELTA;
    } else {
      palettes[i] = i * DELTA;
      palettes[COLORS * 2 + i] = (i * DELTA) << 16;
    }
    palettes[COLORS + i] = (i * DELTA) << 8;
  }
  for (i = 0; i < COLORS / 2; i++) {
    if (pal == WEED_PALETTE_RGBA32) {
      palettes[i + COLORS / 2] = (255 << 16) | (i * DELTA) << 8 | i * DELTA;
      palettes[COLORS * 2 + i + COLORS / 2] = 255 | (i * DELTA) << 16 | (i * DELTA) << 8;
    } else {
      palettes[i + COLORS / 2] = 255 | (i * DELTA) << 16 | (i * DELTA) << 8;
      palettes[COLORS * 2 + i + COLORS / 2] = (255 << 16) | (i * DELTA) << 8 | i * DELTA;
    }
    palettes[COLORS + i + COLORS / 2] = (255 << 8) | (i * DELTA) << 16 | i * DELTA;
  }
  for (i = 0; i < COLORS; i++) {
    palettes[COLORS * 3 + i] = (255 * i / COLORS) * 0x10101;
  }
  for (i = 0; i < COLORS * PATTERN; i++) {
    palettes[i] = palettes[i] & 0xfefeff;
  }
}


static weed_error_t blurzoom_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height, video_width, video_area;
  int buf_area;
  weed_plant_t *in_channel;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_in_channel(inst, 0);

  video_width = weed_channel_get_width(in_channel);
  video_height = weed_channel_get_height(in_channel);
  video_area = video_width * video_height;

  sdata->buf_width_blocks = (video_width / 32);
  if (sdata->buf_width_blocks > 255) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->buf_width = sdata->buf_width_blocks * 32;
  sdata->buf_height = video_height;
  sdata->buf_margin_left = (video_width - sdata->buf_width) / 2;
  sdata->buf_margin_right = video_width - sdata->buf_width - sdata->buf_margin_left;
  buf_area = sdata->buf_width * sdata->buf_height;

  sdata->blurzoombuf = (unsigned char *)weed_calloc(buf_area * 2, 1);
  if (!sdata->blurzoombuf) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->blurzoomx = (int *)weed_calloc(sdata->buf_width, sizeof(int));
  if (!sdata->blurzoomx) {
    weed_free(sdata->blurzoombuf);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->blurzoomy = (int *)weed_calloc(sdata->buf_height, sizeof(int));
  if (!sdata->blurzoomy) {
    weed_free(sdata->blurzoombuf);
    weed_free(sdata->blurzoomx);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->blurzoombuf, 0, buf_area * 2);

  sdata->threshold = MAGIC_THRESHOLD * 7;

  sdata->snapframe = (RGB32 *)weed_calloc(video_area, PIXEL_SIZE);
  if (!sdata->snapframe) {
    weed_free(sdata->blurzoombuf);
    weed_free(sdata->blurzoomy);
    weed_free(sdata->blurzoomx);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->background = (short *)weed_calloc(video_height * video_width, sizeof(short));
  if (!sdata->background) {
    weed_free(sdata->blurzoombuf);
    weed_free(sdata->blurzoomy);
    weed_free(sdata->blurzoomx);
    weed_free(sdata->snapframe);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->diff = (unsigned char *)weed_calloc(video_height * video_width, 4 * sizeof(unsigned char));
  if (!sdata->diff) {
    weed_free(sdata->background);
    weed_free(sdata->blurzoombuf);
    weed_free(sdata->blurzoomy);
    weed_free(sdata->blurzoomx);
    weed_free(sdata->snapframe);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  setTable(sdata);
  makePalette(weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL));
  palette = palettes;

  sdata->snapTime = 0;
  sdata->snapInterval = 3;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t blurzoom_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    weed_free(sdata->diff);
    weed_free(sdata->background);
    weed_free(sdata->blurzoombuf);
    weed_free(sdata->blurzoomy);
    weed_free(sdata->blurzoomx);
    weed_free(sdata->snapframe);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t blurzoom_process(weed_plant_t *inst, weed_timecode_t timecode) {
  struct _sdata *sdata;
  weed_plant_t *in_channel, *out_channel, **in_params;
  RGB32 *src, *dest;
  size_t snap_offs = 0, src_offs = 0;
  int video_width, video_height, irow, orow;
  int mode = 0, pattern;
  int x, y;
  RGB32 a, b;
  unsigned char *diff, *p;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  in_channel = weed_get_in_channel(inst, 0);
  out_channel = weed_get_out_channel(inst, 0);

  src = weed_channel_get_pixel_data(in_channel);
  dest = weed_channel_get_pixel_data(out_channel);

  video_width = weed_channel_get_width(in_channel);
  video_height = weed_channel_get_height(in_channel);

  irow = weed_channel_get_stride(in_channel) / 4;
  orow = weed_channel_get_stride(out_channel) / 4;

  diff = sdata->diff;

  in_params = weed_get_in_params(inst, NULL);
  mode = weed_param_get_value_int(in_params[0]);
  pattern = weed_param_get_value_int(in_params[1]);
  weed_free(in_params);

  if (mode != 2 || sdata->snapTime <= 0) {
    image_bgsubtract_update_y(src, video_width, video_height, irow, sdata);
    if (mode == 0 || sdata->snapTime <= 0) {
      diff += sdata->buf_margin_left;
      p = sdata->blurzoombuf;
      for (y = 0; y < sdata->buf_height; y++) {
        for (x = 0; x < sdata->buf_width; x++) {
          p[x] |= diff[x] >> 3;
        }
        diff += video_width;
        p += sdata->buf_width;
      }
      if (mode == 1 || mode == 2) {
        for (x = 0; x < video_height; x++) {
          weed_memcpy(sdata->snapframe + snap_offs, src + src_offs, video_width * PIXEL_SIZE);
          snap_offs += video_width;
          src_offs += irow;
        }
      }
    }
  }

  blurzoomcore(sdata);

  irow -= video_width;
  orow -= video_width;

  if (mode == 1 || mode == 2) {
    src = sdata->snapframe;
  }
  p = sdata->blurzoombuf;
  for (y = 0; y < video_height; y++) {
    for (x = 0; x < sdata->buf_margin_left; x++) {
      *dest++ = *src++;
    }
    for (x = 0; x < sdata->buf_width; x++) {
      a = *src & 0xfefeff;
      b = palette[COLORS * pattern + *p++];
      a += b;
      b = a & 0x10101;
      *dest++ = (*src++ & 0xff000000) | ((a | (b - (b >> 8))) & 0xffffff);
    }
    for (x = 0; x < sdata->buf_margin_right; x++) {
      *dest++ = *src++;
    }

    src += irow;
    dest += orow;
  }

  if (mode == 1 || mode == 2) {
    sdata->snapTime--;
    if (sdata->snapTime < 0) {
      sdata->snapTime = sdata->snapInterval;
    }
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  const char *modes[] = {"normal", "strobe", "strobe2", "trigger", NULL};
  const char *patterns[] = {"blue", "green", "red", "white", NULL};

  int palette_list[] = {WEED_PALETTE_BGRA32, WEED_PALETTE_RGBA32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("mode", "Trigger _Mode", 0, modes),
                               weed_string_list_init("color", "_Color", 0, patterns), NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("blurzoom", "effectTV", 1, WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                               blurzoom_init, blurzoom_process, &blurzoom_deinit,
                               in_chantmpls,
                               out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

