// negate.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2012
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
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static weed_error_t negate_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);
  int pal = weed_channel_get_palette(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  int psize = pixel_size(pal);
  width *= psize;

  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_channel);
    src += offset * irowstride;
    dst += offset * orowstride;
  }

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i += 3) {
      if (pal == WEED_PALETTE_ARGB32) {
	dst[orowstride * j + i] = src[irowstride * j + i];
	i++;
      }
      dst[orowstride * j + i] = 0xFF - src[irowstride * j + i];
      dst[orowstride * j + i + 1] = 0xFF - src[irowstride * j + i + 1];
      dst[orowstride * j + i + 2] = 0xFF - src[irowstride * j + i + 2];
      if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32) {
	dst[orowstride * j + i + 3] = src[irowstride * j + i + 3];
	i++;
      }
    }
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  char desc[128];
  int palette_list[] = ALL_RGB_PALETTES;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;
  weed_plant_t *filter_class = weed_filter_class_init("negate", "salsaman", 1, filter_flags, palette_list, NULL,
                               negate_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  snprintf(desc, 128, "Inverts the Red, Green and Blue values of each pixel");
  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

