// palette_test.c
// weed plugin
// (c) G. Finch (salsaman) 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// test plugin for testing various palettes

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static weed_error_t ptest_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, &error);
  void **src = weed_get_voidptr_array(in_channel, WEED_LEAF_PIXEL_DATA, &error);
  void **dst = weed_get_voidptr_array(out_channel, WEED_LEAF_PIXEL_DATA, &error);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);

  if (src[0] != dst[0]) weed_memcpy(dst[0], src[0], width * height * 4);
  /*
    if (src[1]!=dst[1]) weed_memcpy(dst[1],src[1],width*height);
    if (src[2]!=dst[2]) weed_memcpy(dst[2],src[2],width*height);
  */

  weed_free(src);
  weed_free(dst);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_YUYV, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("Palette testing plugin", "salsaman", 1, 0, palette_list,
                               NULL, ptest_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);
  weed_plant_t *gui;

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(in_chantmpls[0], WEED_LEAF_YUV_CLAMPING, WEED_YUV_CLAMPING_CLAMPED);
  weed_set_int_value(out_chantmpls[0], WEED_LEAF_YUV_CLAMPING, WEED_YUV_CLAMPING_CLAMPED);

  gui = weed_filter_get_gui(filter_class);
  weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;
