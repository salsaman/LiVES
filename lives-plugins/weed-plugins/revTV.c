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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static weed_error_t revtv_process(weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t *in_channel, *out_channel, **in_params;
  unsigned char *src, *dest;
  short val;

  int yval, offset;
  int width, height, irow, orow, pal;
  int red = 0, green = 1, blue = 2, psize = 4;
  int linespace;

  double vscale;

  register int x, y;

  in_channel = weed_get_plantptr_value(inst, "in_channels", NULL);
  out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);

  src = (unsigned char *)weed_get_voidptr_value(in_channel, "pixel_data", NULL);
  dest = (unsigned char *)weed_get_voidptr_value(out_channel, "pixel_data", NULL);

  width = weed_get_int_value(in_channel, "width", NULL);
  height = weed_get_int_value(in_channel, "height", NULL);

  pal = weed_get_int_value(in_channel, "current_palette", NULL);

  irow = weed_get_int_value(in_channel, "rowstrides", NULL);
  orow = weed_get_int_value(out_channel, "rowstrides", NULL);

  in_params = weed_get_plantptr_array(inst, "in_parameters", NULL);
  linespace = weed_get_int_value(in_params[0], "value", NULL);
  vscale = weed_get_double_value(in_params[1], "value", NULL);
  vscale = vscale * vscale / 200.;
  weed_free(in_params);

  if (pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_BGRA32) {
    red = 2;
    blue = 0;
  } else if (pal == WEED_PALETTE_ARGB32) {
    red = 1;
    green = 2;
    blue = 3;
  }

  if (pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_YUV888) psize = 3;

  width *= psize;

  irow *= linespace;

  for (y = 0; y < height; y += linespace) {
    for (x = 0; x < width; x += psize) {
      if (pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888) val = src[0] * 7;
      else val = (short)((src[red] << 1) + (src[green] << 2) + src[blue]);
      yval = y - val * vscale;
      if ((offset = yval * orow + x) >= 0) weed_memcpy(&dest[offset], src, psize);
      src += psize;
    }
    src += irow - width;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_RGB24, WEED_PALETTE_BGR24,
                        WEED_PALETTE_ARGB32, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *in_params[] = {weed_integer_init("lspace", "_Line spacing", 6, 1, 16),
                               weed_float_init("vscale", "_Vertical scale factor", 2., 0., 4.), NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("revTV", "effectTV", 1, 0, NULL, revtv_process, NULL,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;



