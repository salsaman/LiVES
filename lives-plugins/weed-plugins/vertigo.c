/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2005 FUKUCHI Kentaro
 *
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
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

typedef unsigned int RGB32;

#define PIXEL_SIZE (sizeof(RGB32))

typedef struct _sdata {
  int dx, dy;
  int sx, sy;
  RGB32 *buffer;
  RGB32 *current_buffer, *alt_buffer;
  double phase;
} sdata;

////////////////////////////////////////////////

static void setParams(int video_width, int video_height, sdata *sdata, double phase_increment, double zoomrate) {
  double vx, vy;
  double t;
  double x, y;
  double dizz;

  dizz = sin(sdata->phase) * 10 + sin(sdata->phase * 1.9 + 5) * 5;

  x = video_width / 2.;
  y = video_height / 2.;
  t = (x * x + y * y) * zoomrate;
  if (video_width > video_height) {
    if (dizz >= 0) {
      if (dizz > x) dizz = x;
      vx = (x * (x - dizz) + y * y) / t;
    } else {
      if (dizz < -x) dizz = -x;
      vx = (x * (x + dizz) + y * y) / t;
    }
    vy = (dizz * y) / t;
  } else {
    if (dizz >= 0) {
      if (dizz > y) dizz = y;
      vx = (x * x + y * (y - dizz)) / t;
    } else {
      if (dizz < -y) dizz = -y;
      vx = (x * x + y * (y + dizz)) / t;
    }
    vy = (dizz * x) / t;
  }
  sdata->dx = vx * 65536.;
  sdata->dy = vy * 65536.;

  sdata->sx = (-vx * x + vy * y + x + cos(sdata->phase * 5.) * 2.) * 65536.;
  sdata->sy = (-vx * y - vy * x + y + sin(sdata->phase * 6.) * 2.) * 65536.;

  sdata->phase += phase_increment;
  if (sdata->phase > 5700000.) sdata->phase = 0.;
}


static weed_error_t vertigo_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height, video_width, video_area;
  int error;
  weed_plant_t *in_channel;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, "in_channels", &error);

  video_height = weed_get_int_value(in_channel, "height", &error);
  video_width = weed_get_int_value(in_channel, "width", &error);
  video_area = video_width * video_height;

  sdata->buffer = (RGB32 *)weed_calloc(video_area, PIXEL_SIZE * 2);

  if (sdata->buffer == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->current_buffer = sdata->buffer;
  sdata->alt_buffer = sdata->buffer + video_area;
  sdata->phase = 0.;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t vertigo_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    if (sdata->buffer) weed_free(sdata->buffer);
    weed_free(sdata);
    weed_get_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t vertigo_process(weed_plant_t *inst, weed_timecode_t timecode) {
  struct _sdata *sdata;
  weed_plant_t *in_channel, *out_channel, **in_params;

  double pinc, zoomrate;

  size_t offs = 0;

  RGB32 v;
  RGB32 *p, *src, *dest;

  int video_width, video_height, video_area, irow, orow;

  int ox, oy, i;

  register int x, y;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  in_channel = weed_get_plantptr_value(inst, "in_channels", NULL);
  out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);

  src = weed_get_voidptr_value(in_channel, "pixel_data", NULL);
  dest = weed_get_voidptr_value(out_channel, "pixel_data", NULL);

  video_width = weed_get_int_value(in_channel, "width", NULL);
  video_height = weed_get_int_value(in_channel, "height", NULL);

  irow = weed_get_int_value(in_channel, "rowstrides", NULL) / 4 - video_width;
  orow = weed_get_int_value(out_channel, "rowstrides", NULL) / 4;

  video_area = video_width * video_height;

  in_params = weed_get_plantptr_array(inst, "in_parameters", NULL);
  pinc = weed_get_double_value(in_params[0], "value", NULL);
  zoomrate = weed_get_double_value(in_params[1], "value", NULL);
  weed_free(in_params);

  setParams(video_width, video_height, sdata, pinc, zoomrate);

  p = sdata->alt_buffer;
  for (y = video_height; y > 0; y--) {
    ox = sdata->sx;
    oy = sdata->sy;
    for (x = video_width; x > 0; x--) {
      if ((i = (oy >> 16) * video_width + (ox >> 16)) < 0) i = 0;
      if (i >= video_area) i = video_area;
      v = ((sdata->current_buffer[i] & 0xfcfcff) * 3) + ((*src) & 0xfcfcff);
      *p++ = (v >> 2) | (*src++ & 0xff000000);
      ox += sdata->dx;
      oy += sdata->dy;
    }

    src += irow;
    sdata->sx -= sdata->dy;
    sdata->sy += sdata->dx;
  }

  for (y = 0; y < video_height; y++) {
    weed_memcpy(dest, sdata->alt_buffer + offs, video_width * PIXEL_SIZE);
    dest += orow;
    offs += video_width;
  }

  p = sdata->current_buffer;
  sdata->current_buffer = sdata->alt_buffer;
  sdata->alt_buffer = p;

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *in_params[] = {weed_float_init("pinc", "_Phase increment", 0.2, 0.1, 1.0),
                               weed_float_init("zoom", "_Zoom", 1.01, 1.01, 1.10), NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("vertigo", "effectTV", 1, 0, vertigo_init, vertigo_process, vertigo_deinit,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;


