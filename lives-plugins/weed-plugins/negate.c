// negate.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

static weed_error_t negate_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error), *out_channel = weed_get_plantptr_value(inst,
                             "out_channels",
                             &error);
  unsigned char *src = weed_get_voidptr_value(in_channel, "pixel_data", &error);
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", &error);

  int width = weed_get_int_value(in_channel, "width", &error);
  int height = weed_get_int_value(in_channel, "height", &error);
  int pal = weed_get_int_value(in_channel, "current_palette", &error);
  int irowstride = weed_get_int_value(in_channel, "rowstrides", &error);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", &error);
  int psize = 4, start = 0, alpha = 3;

  unsigned char *end = src + height * irowstride;

  register int i;

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) psize = 3;
  if (pal == WEED_PALETTE_ARGB32) {
    start = 1;
    alpha = -1;
  }
  width *= psize;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, "offset")) {
    int offset = weed_get_int_value(out_channel, "offset", &error);
    int dheight = weed_get_int_value(out_channel, "height", &error);

    src += offset * irowstride;
    dst += offset * orowstride;
    end = src + dheight * irowstride;
  }

  for (; src < end; src += irowstride) {
    for (i = start; i < width; i += psize) {
      dst[i] = src[i] ^ 0xFF;
      dst[i + 1] = src[i + 1] ^ 0xFF;
      dst[i + 2] = src[i + 2] ^ 0xFF;
      if (psize == 4) dst[i + alpha] = src[i + alpha];
    }
    dst += orowstride;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  static weed_plant_t *filter_class;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  filter_class = weed_filter_class_init("negate", "salsaman", 1, filter_flags, NULL,
                                        &negate_process, NULL,
                                        in_chantmpls,
                                        out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;

