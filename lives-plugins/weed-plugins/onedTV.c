/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * 1DTV - scans line by line and generates amazing still image.
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 */

////////////////////////////////////////////////

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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

struct _sdata {
  int line;
  int dir;
  unsigned char *linebuf;
};

////////////////////////////////////////////

static weed_error_t oned_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int map_w, map_h;

  weed_plant_t *out_channel;
  int error;

  sdata = weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, &error);

  map_h = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, &error);
  map_w = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error);

  sdata->linebuf = weed_calloc(map_h * map_w, 1);

  if (sdata->linebuf == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->line = 0;
  sdata->dir = 1;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t oned_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->linebuf) weed_free(sdata->linebuf);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t oned_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;
  weed_plant_t **in_params;
  weed_plant_t *in_channel, *out_channel;
  unsigned char *osrc, *src, *dest;

  size_t size;

  int nlines, bounce;
  int width, height, irow, orow, psize = 3, pwidth, palette;

  register int i;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  osrc = src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);

  irow = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_ARGB32) psize = 4;

  size = orow * height;

  src += irow  * sdata->line;

  pwidth = width * psize;

  nlines = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  bounce = weed_get_boolean_value(in_params[1], WEED_LEAF_VALUE, NULL);
  weed_free(in_params);

  for (i = 0; i < nlines; i++) {
    // blit line(s) to linebuf
    weed_memcpy(sdata->linebuf + sdata->line * orow, src, pwidth);
    if (sdata->dir == -1) src -= irow;
    else src += irow;
    sdata->line += sdata->dir;
    if (sdata->line >= height) {
      if (bounce == WEED_FALSE) {
        sdata->line = 0;
        src = osrc;
      } else {
        sdata->dir = -sdata->dir;
        sdata->line += sdata->dir;
      }
    } else if (sdata->line <= 0) {
      if (bounce == WEED_FALSE) {
        sdata->line = height - 1;
        src = osrc + (height - 1) * irow;
      } else {
        sdata->dir = -sdata->dir;
        sdata->line += sdata->dir;
      }
    }
  }

  // copy linebuff to dest
  weed_memcpy(dest, sdata->linebuf, size);

  // draw green line
  dest += orow * sdata->line;

  switch (palette) {
  case WEED_PALETTE_RGBA32:
    for (i = 0; i < width; i++) {
      dest[0] = 0x00;
      dest[1] = 0xFF;
      dest[2] = 0x00;
      dest[3] = 0xFF;
      dest += 4;
    }
    break;
  case WEED_PALETTE_ARGB32:
    for (i = 0; i < width; i++) {
      dest[0] = 0xFF;
      dest[1] = 0x00;
      dest[2] = 0xFF;
      dest[3] = 0x00;
      dest += 4;
    }
    break;
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    for (i = 0; i < width; i++) {
      dest[0] = 0x00;
      dest[1] = 0xFF;
      dest[2] = 0x00;
      dest += 3;
    }
    break;
  default:
    break;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_END};
  weed_plant_t *in_params[] = {weed_integer_init("linerate", "_Line rate", 8, 1, 1024),
                               weed_switch_init("bounce", "_Bounce", WEED_FALSE), NULL
                              };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0",
                                  WEED_CHANNEL_REINIT_ON_SIZE_CHANGE |
                                  WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE,
                                  palette_list), NULL
                                 };

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("onedTV", "effectTV", 1, 0,
                               oned_init, oned_process, oned_deinit,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

