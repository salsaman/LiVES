/*
   EffecTV - Realtime Digital Video Effector
   Copyright (C) 2001-2005 FUKUCHI Kentaro

   RippleTV - Water ripple effect.
   Copyright (C) 2001-2002 FUKUCHI Kentaro

*/

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#define MAGIC_THRESHOLD 70

typedef unsigned int RGB32;

static int sqrtable[256];

#define POINT 16
#define IMPACT 2
#define DECAY 8
#define LOOPNUM 2

/////////////

struct _sdata {
  int *map;
  int *map1;
  int *map2;
  int *map3;
  int bgIsSet;
  int period;
  int rain_stat;
  unsigned int drop_prob;
  int drop_prob_increment;
  int drops_per_frame_max;
  int drops_per_frame;
  int drop_power;
  signed char *vtable;
  short *background;
  unsigned char *diff;
  int threshold;
  uint64_t fastrand_val;
};


static void image_bgset_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  register int i, j;
  int R, G, B;
  RGB32 *p;
  short *q;

  rowstride -= width;

  p = src;
  q = sdata->background;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      R = ((*p) & 0xff0000) >> (16 - 1);
      G = ((*p) & 0xff00) >> (8 - 2);
      B = (*p) & 0xff;
      *q = (short)(R + G + B);
      p++;
      q++;
    }
    p += rowstride;
  }
}


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

/////////////////////////////////////////////////////

static void setTable(void) {
  int i;
  for (i = 0; i < 128; i++) {
    sqrtable[i] = i * i;
  }
  for (i = 1; i <= 128; i++) {
    sqrtable[256 - i] = -i * i;
  }
}


static int setBackground(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  image_bgset_y(src, width, height, rowstride, sdata);
  sdata->bgIsSet = 1;

  return 0;
}


/////////////////////////////////////////////////////////////

static weed_error_t ripple_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int map_h;
  int map_w;
  //char *list[2]={"ripples","rain"};
  weed_plant_t *in_channel;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);

  map_h = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  map_w = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);

  sdata->map = (int *)weed_calloc(map_h * map_w * 3, sizeof(int));
  if (sdata->map == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->vtable = (signed char *)weed_calloc(map_h * map_w, 2 * sizeof(signed char));
  if (sdata->vtable == NULL) {
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->background = (short *)weed_calloc(map_h * map_w, sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->vtable);
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->diff = (unsigned char *)weed_calloc(map_h * map_w, 4 * sizeof(unsigned char));
  if (sdata->diff == NULL) {
    weed_free(sdata->background);
    weed_free(sdata->vtable);
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->map1 = sdata->map;
  sdata->map2 = sdata->map + map_h * map_w;
  sdata->map3 = sdata->map + map_w * map_h * 2;
  sdata->bgIsSet = 0;
  sdata->threshold = MAGIC_THRESHOLD * 7;
  sdata->period = 0;
  sdata->rain_stat = 0;
  sdata->drop_prob = 0;
  sdata->drop_prob_increment = 0;
  sdata->drops_per_frame_max = 0;
  sdata->drops_per_frame = 0;
  sdata->drop_power = 0;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t ripple_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    if (sdata->diff) weed_free(sdata->diff);
    if (sdata->background) weed_free(sdata->background);
    if (sdata->vtable) weed_free(sdata->vtable);
    if (sdata->map) weed_free(sdata->map);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static void motiondetect(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  unsigned char *diff;
  int *p, *q;
  register int x, y;
  int h;

  if (!sdata->bgIsSet) {
    setBackground(src, width, height, rowstride, sdata);
  }
  image_bgsubtract_update_y(src, width, height, rowstride, sdata);

  diff = sdata->diff;
  p = sdata->map1 + width + 1;
  q = sdata->map2 + width + 1;
  diff += width + 2;

  for (y = height - 2; y > 0; y--) {
    for (x = width - 2; x > 0; x--) {
      h = (int) * diff + (int) * (diff + 1) + (int) * (diff + width) + (int) * (diff + width + 1);
      if (h > 0) {
        *p = h << (POINT + IMPACT - 8);
        *q = *p;
      }
      p++;
      q++;
      diff += 2;
    }
    diff += width + 2;
    p += 2;
    q += 2;
  }
}


static inline void drop(int power, int width, int height, struct _sdata *sdata) {
  int x, y;
  int *p, *q;

  x = (sdata->fastrand_val = fastrand(sdata->fastrand_val)) % (width - 4) + 2;
  y = (sdata->fastrand_val = fastrand(sdata->fastrand_val)) % (height - 4) + 2;
  p = sdata->map1 + y * width + x;
  q = sdata->map2 + y * width + x;
  *p = power;
  *q = power;
  *(p - width) = *(p - 1) = *(p + 1) = *(p + width) = power / 2;
  *(p - width - 1) = *(p - width + 1) = *(p + width - 1) = *(p + width + 1) = power / 4;
  *(q - width) = *(q - 1) = *(q + 1) = *(q + width) = power / 2;
  *(q - width - 1) = *(q - width + 1) = *(q + width - 1) = *(p + width + 1) = power / 4;
}


static void raindrop(int width, int height, struct _sdata *sdata) {
  int i;
  if (sdata->period == 0) {
    switch (sdata->rain_stat) {
    case 0:
      sdata->period = ((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 55) + 100;
      sdata->drop_prob = 0;
      sdata->drop_prob_increment = 0x00ffffff / sdata->period;
      sdata->drop_power = (-((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 60) - 2) << POINT;
      sdata->drops_per_frame_max = 2 << ((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 62); // 2,4,8 or 16
      sdata->rain_stat = 1;
      break;
    case 1:
      sdata->drop_prob = 0x00ffffff;
      sdata->drops_per_frame = 1;
      sdata->drop_prob_increment = 1;
      sdata->period = (sdata->drops_per_frame_max - 1) * 16;
      sdata->rain_stat = 2;
      break;
    case 2:
      sdata->period = ((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 54) + 1000;
      sdata->drop_prob_increment = 0;
      sdata->rain_stat = 3;
      break;
    case 3:
      sdata->period = (sdata->drops_per_frame_max - 1) * 16;
      sdata->drop_prob_increment = -1;
      sdata->rain_stat = 4;
      break;
    case 4:
      sdata->period = ((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 56) + 60;
      sdata->drop_prob_increment = -(sdata->drop_prob / sdata->period);
      sdata->rain_stat = 5;
      break;
    case 5:
    default:
      sdata->period = ((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 55) + 500;
      sdata->drop_prob = 0;
      sdata->rain_stat = 0;
      break;
    }
  }
  switch (sdata->rain_stat) {
  default:
  case 0:
    break;
  case 1:
  case 5:
    if (((sdata->fastrand_val = fastrand(sdata->fastrand_val)) >> 40) < sdata->drop_prob) {
      drop(sdata->drop_power, width, height, sdata);
    }
    sdata->drop_prob += sdata->drop_prob_increment;
    break;
  case 2:
  case 3:
  case 4:
    for (i = sdata->drops_per_frame / 16; i > 0; i--) {
      drop(sdata->drop_power, width, height, sdata);
    }
    sdata->drops_per_frame += sdata->drop_prob_increment;
    break;
  }
  sdata->period--;
}


static weed_error_t ripple_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;
  RGB32 *src, *dest;
  weed_plant_t *in_channel, *out_channel, *in_param;
  int *p, *q, *r;
  signed char *vp;
  int dx, dy, h, v, width, height, irowstride, orowstride, orowstridex;
  int mode;
  register int x, y, i;

  mode = 0;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);

  irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL) / 4;
  orowstridex = orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL) / 4;

  //if (width%2!=0) orowstridex--;

  sdata->fastrand_val = fastrand(0);

  in_param = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  mode = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);

  /* impact from the motion or rain drop */
  if (mode) {
    raindrop(width, height, sdata);
  } else {
    motiondetect(src, width, height, irowstride, sdata);
  }

  /* simulate surface wave */

  /* This function is called only a few times per second. To increase a speed
    ss   * of wave, iterate this loop several times. */
  for (i = LOOPNUM; i > 0; i--) {
    /* wave simulation */
    p = sdata->map1 + width + 1;
    q = sdata->map2 + width + 1;
    r = sdata->map3 + width + 1;
    for (y = height - 2; y > 0; y--) {
      for (x = width - 2; x > 0; x--) {
        h = *(p - width - 1) + *(p - width + 1) + *(p + width - 1) + *(p + width + 1)
            + *(p - width) + *(p - 1) + *(p + 1) + *(p + width) - (*p) * 9;
        h = h >> 3;
        v = *p - *q;
        v += h - (v >> DECAY);
        *r = v + *p;
        p++;
        q++;
        r++;
      }
      p += 2;
      q += 2;
      r += 2;
    }

    /* low pass filter */
    p = sdata->map3 + width + 1;
    q = sdata->map2 + width + 1;
    for (y = height - 2; y > 0; y--) {
      for (x = width - 2; x > 0; x--) {
        h = *(p - width) + *(p - 1) + *(p + 1) + *(p + width) + (*p) * 60;
        *q = h >> 6;
        p++;
        q++;
      }
      p += 2;
      q += 2;
    }

    p = sdata->map1;
    sdata->map1 = sdata->map2;
    sdata->map2 = p;
  }

  vp = sdata->vtable;
  p = sdata->map1;
  for (y = height - 1; y > 0; y--) {
    for (x = width - 1; x > 0; x--) {
      /* difference of the height between two voxels. They are doubled to emphasise the wave. */
      vp[0] = sqrtable[((p[0] - p[1]) >> (POINT - 1)) & 0xff];
      vp[1] = sqrtable[((p[0] - p[width]) >> (POINT - 1)) & 0xff];
      p++;
      vp += 2;
    }
    p++;
    vp += 2;
  }

  vp = sdata->vtable;

  orowstridex -= width;

  /* draw refracted image. The vector table is stretched. */
  for (y = 0; y < height - 2; y += 2) {
    for (x = 0; x < width - 1; x += 2) {
      h = (int)vp[0];
      v = (int)vp[1];
      dx = x + h;
      dy = y + v / 2;
      if (dx < 0) dx = 0;
      if (dy < 0) dy = 0;
      if (dx >= width) dx = width - 1;
      if (dy >= height) dy = height - 1;
      dest[0] = src[dy * irowstride + dx];

      i = dx;

      dx = x + 1 + (h + (int)vp[2]) / 2;
      if (dx < 0) dx = 0;
      if (dx >= width) dx = width - 1;
      dest[1] = src[dy * irowstride + dx];

      dy = y + 1 + (v  + (int)vp[width * 2 + 1]) / 2;
      if (dy < 0) dy = 0;
      if (dy >= height) dy = height - 1;
      dest[orowstride] = src[dy * irowstride + i];
      dest[orowstride + 1] = src[dy * irowstride + dx];
      dest += 2;
      vp += 2;
    }
    dest += orowstridex + orowstride;
    vp++;
  }
  return WEED_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////

WEED_SETUP_START(200, 200) {
  const char *modes[] = {"ripples", "rain", NULL};
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("mode", "Ripple _mode", 0, modes), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("rippleTV", "effectTV", 1, 0, palette_list,
                               ripple_init, ripple_process, ripple_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);

  setTable();
}
WEED_SETUP_END;

