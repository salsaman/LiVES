// plasma.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// based on code * Copyright (C) 2002 W.P. van Paassen - peter@paassen.tmfweb.nl

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

#include <math.h>

typedef struct {
  uint16_t pos1;
  uint16_t pos2;
  uint16_t pos3;
  uint16_t pos4;
  uint16_t tpos1;
  uint16_t tpos2;
  uint16_t tpos3;
  uint16_t tpos4;
} _sdata;

typedef struct {
  short r;
  short g;
  short b;
} color_t;

static int aSin[512];
static color_t colors[256];

static void plasma_prep(void) {
  int i;
  float rad;

  /*create sin lookup table */
  for (i = 0; i < 512; i++) {
    rad = ((float)i * 0.703125) * 0.0174532; /* 360 / 512 * degree to rad, 360 degrees spread over 512 values to b
						   e able to use AND 512-1 instead of using modulo 360*/
    aSin[i] = sin(rad) * 1024; /*using fixed point math with 1024 as base*/
  }

  /* create palette */
  for (i = 0; i < 64; ++i) {
    colors[i].r = i << 2;
    colors[i].g = 255 - ((i << 2) + 1);
    colors[i + 64].r = 255;
    colors[i + 64].g = (i << 2) + 1;
    colors[i + 128].r = 255 - ((i << 2) + 1);
    colors[i + 128].g = 255 - ((i << 2) + 1);
    colors[i + 192].g = (i << 2) + 1;
  }
}


static weed_error_t plasma_init(weed_plant_t *inst) {
  _sdata *sd = (_sdata *)weed_malloc(sizeof(_sdata));
  if (sd == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->pos1 = sd->pos2 = sd->pos3 = sd->pos4 = 0;
  weed_set_voidptr_value(inst, "plugin_internal", sd);

  return WEED_SUCCESS;
}


static weed_error_t plasma_deinit(weed_plant_t *inst) {
  _sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sd) {
    weed_free(sd);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t plasma_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", NULL);
  int width = weed_get_int_value(out_channel, "width", NULL);
  int height = weed_get_int_value(out_channel, "height", NULL);
  int palette = weed_get_int_value(out_channel, "current_palette", NULL);
  _sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  int rowstride = weed_get_int_value(out_channel, "rowstrides", NULL);
  uint8_t index;
  int widthx = width * 3;
  int offs, x;
  unsigned char *end;
  register int j;

  if (palette == WEED_PALETTE_RGBA32) widthx = width * 4;

  offs = rowstride - widthx;

  sd->tpos4 = sd->pos4;
  sd->tpos3 = sd->pos3;

  end = dst + height * widthx;

  while (dst < end) {
    sd->tpos1 = sd->pos1 + 5;
    sd->tpos2 = sd->pos2 + 3;
    sd->tpos3 &= 511;
    sd->tpos4 &= 511;

    for (j = 0; j < width; ++j) {
      sd->tpos1 &= 511;
      sd->tpos2 &= 511;
      x = aSin[sd->tpos1] + aSin[sd->tpos2] + aSin[sd->tpos3] + aSin[sd->tpos4]; /*actual plasma calculation*/
      index = 128 + (x >> 4); /*fixed point multiplication but optimized so basically it says (x * (64 * 1024
				    ) / (1024 * 1024)), x is already multiplied by 1024*/

      *dst++ = colors[index].r;
      *dst++ = colors[index].g;
      *dst++ = 0;
      if (palette == WEED_PALETTE_RGBA32) *dst++ = 255;

      sd->tpos1 += 5;
      sd->tpos2 += 3;
    }
    dst += offs;
    sd->tpos4 += 3;
    sd->tpos3 += 1;
  }

  sd->pos1 += 9;
  sd->pos3 += 8;

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_END};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *filter_class;
  int filter_flags = 0;

  filter_class = weed_filter_class_init("plasma", "salsaman/w.p van paasen", 1, filter_flags, &plasma_init, &plasma_process,
                                        &plasma_deinit, NULL,
                                        out_chantmpls, NULL, NULL);
  weed_set_double_value(filter_class, "target_fps", 50.); // set reasonable default fps

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
  plasma_prep();
}
WEED_SETUP_END;

