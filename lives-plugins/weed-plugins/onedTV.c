/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * 1DTV - scans line by line and generates amazing still image.
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 */


////////////////////////////////////////////////

/* modified for Livido by G. Finch (salsaman)
   modifications (c) G. Finch */


// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions = 2; // number of different weed api versions supported
static int api_versions[] = {131, 100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

typedef unsigned int RGB32;

union memtest {
  int32_t num;
  char chr[4];
};


static int is_big_endian() {
  union memtest mm;
  mm.num = 0x12345678;
  if (mm.chr[0] == 0x78) return 0;
  return 1;
}

struct _sdata {
  int line;
  int dir;
  unsigned char *linebuf;
};

////////////////////////////////////////////

int oned_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int map_w, map_h;

  weed_plant_t *out_channel;
  int error;

  sdata = weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  out_channel = weed_get_plantptr_value(inst, "out_channels", &error);

  map_h = weed_get_int_value(out_channel, "height", &error);
  map_w = weed_get_int_value(out_channel, "rowstrides", &error);

  sdata->linebuf = weed_malloc(map_h * map_w);

  if (sdata->linebuf == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->linebuf, 0, map_w * map_h);

  sdata->line = 0;
  sdata->dir = 1;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_NO_ERROR;
}



int oned_deinit(weed_plant_t *inst) {
  struct _sdata *sdata;
  int error;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) {
    weed_free(sdata->linebuf);
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}


int oned_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params;
  weed_plant_t *in_channel, *out_channel;
  struct _sdata *sdata;
  unsigned char *osrc, *src, *dest;

  size_t size;

  int nlines, bounce;
  int width, height, irow, orow, psize = 3, pwidth, palette;
  int error;

  register int i;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  in_channel = weed_get_plantptr_value(inst, "in_channels", &error);
  out_channel = weed_get_plantptr_value(inst, "out_channels", &error);

  in_params = weed_get_plantptr_array(inst, "in_parameters", &error);

  osrc = src = weed_get_voidptr_value(in_channel, "pixel_data", &error);
  dest = weed_get_voidptr_value(out_channel, "pixel_data", &error);

  width = weed_get_int_value(in_channel, "width", &error);
  height = weed_get_int_value(in_channel, "height", &error);

  irow = weed_get_int_value(in_channel, "rowstrides", &error);
  orow = weed_get_int_value(out_channel, "rowstrides", &error);

  palette = weed_get_int_value(in_channel, "current_palette", &error);

  if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_ARGB32) psize = 4;

  size = orow * height;

  src += irow  * sdata->line;

  pwidth = width * psize;

  nlines = weed_get_int_value(in_params[0], "value", &error);
  bounce = weed_get_boolean_value(in_params[1], "value", &error);
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

  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, num_versions, api_versions);
  if (plugin_info != NULL) {
    int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_END};
    weed_plant_t *in_params[] = {weed_integer_init("linerate", "_Line rate", 8, 1, 1024), weed_switch_init("bounce", "_Bounce", WEED_FALSE), NULL};

    weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE | WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE
                                    , palette_list), NULL
                                   };
    weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
    weed_plant_t *filter_class = weed_filter_class_init("onedTV", "effectTV", 1, 0, &oned_init, &oned_process, &oned_deinit, in_chantmpls,
                                 out_chantmpls,
                                 in_params, NULL);

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    weed_set_int_value(plugin_info, "version", package_version);
  }
  return plugin_info;
}

