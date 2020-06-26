// alpha_to_grey
// weed plugin
// (c) G. Finch (salsaman) 2020
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static weed_error_t a2g_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(out_channel) * 4;
  int height = weed_channel_get_height(out_channel);
  int pal = weed_channel_get_palette(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  int alpha = 3;
  uint8_t aval;

  unsigned char *end;

  register int i;

  if (pal == WEED_PALETTE_ARGB32) alpha = 0;

  // threading
  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_channel);
    src += offset * irowstride;
    dst += offset * orowstride;
  }

  end = src + height * irowstride;

  for (; src < end; src += irowstride) {
    for (i = 0; i < width; i += 4) {
      aval = src[i + alpha];
      weed_memset(dst + i, aval, 4);
    }
    dst += orowstride;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  char desc[128];
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;
  weed_plant_t *filter_class = weed_filter_class_init("alpha_to_grey", "salsaman", 1, filter_flags, palette_list, NULL,
                               a2g_process, NULL,
                               in_chantmpls,
                               out_chantmpls, NULL, NULL);

  snprintf(desc, 128, "Sets the Red, Green and Blue values of each pixel equal to the alpha value");
  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

