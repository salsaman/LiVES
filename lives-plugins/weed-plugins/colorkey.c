// colorkey.c
// weed plugin
// (c) G. Finch (salsaman) 2006
//
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


static int ckey_process(weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, &error), *out_channel = weed_get_plantptr_value(inst,
                               WEED_LEAF_OUT_CHANNELS,
                               &error);
  unsigned char *src1 = weed_get_voidptr_value(in_channels[0], WEED_LEAF_PIXEL_DATA, &error);
  unsigned char *src2 = weed_get_voidptr_value(in_channels[1], WEED_LEAF_PIXEL_DATA, &error);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, &error);
  int width = weed_get_int_value(in_channels[0], WEED_LEAF_WIDTH, &error) * 3;
  int height = weed_get_int_value(in_channels[0], WEED_LEAF_HEIGHT, &error);
  int irowstride1 = weed_get_int_value(in_channels[0], WEED_LEAF_ROWSTRIDES, &error);
  int irowstride2 = weed_get_int_value(in_channels[1], WEED_LEAF_ROWSTRIDES, &error);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error);
  int palette = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, &error);
  unsigned char *end = src1 + height * irowstride1;
  int inplace = (src1 == dst);
  weed_plant_t **in_params;
  int b_red, b_green, b_blue;
  int red, green, blue;
  int b_red_min, b_green_min, b_blue_min;
  int b_red_max, b_green_max, b_blue_max;
  double delta, opac, opacx;
  int *carray;

  register int j;

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, &error);
  delta = weed_get_double_value(in_params[0], WEED_LEAF_VALUE, &error);
  opac = weed_get_double_value(in_params[1], WEED_LEAF_VALUE, &error);
  carray = weed_get_int_array(in_params[2], WEED_LEAF_VALUE, &error);
  b_red = carray[0];
  b_green = carray[1];
  b_blue = carray[2];
  weed_free(carray);

  b_red_min = b_red - (int)(b_red * delta + .5);
  b_green_min = b_green - (int)(b_green * delta + .5);
  b_blue_min = b_blue - (int)(b_blue * delta + .5);

  b_red_max = b_red + (int)((255 - b_red) * delta + .5);
  b_green_max = b_green + (int)((255 - b_green) * delta + .5);
  b_blue_max = b_blue + (int)((255 - b_blue) * delta + .5);

  for (; src1 < end; src1 += irowstride1) {
    for (j = 0; j < width; j += 3) {
      if (palette == WEED_PALETTE_RGB24) {
        red = src1[j];
        green = src1[j + 1];
        blue = src1[j + 2];
      } else {
        blue = src1[j];
        green = src1[j + 1];
        red = src1[j + 2];
      }
      if (red >= b_red_min && red <= b_red_max && green >= b_green_min && green <= b_green_max && blue >= b_blue_min && blue <= b_blue_max) {
        dst[j] = src1[j] * ((opacx = 1. - opac)) + src2[j] * opac;
        dst[j + 1] = src1[j + 1] * (opacx) + src2[j + 1] * opac;
        dst[j + 2] = src1[j + 2] * (opacx) + src2[j + 2] * opac;
      } else if (!inplace) weed_memcpy(&dst[j], &src1[j], 3);
    }
    src2 += irowstride2;
    dst += orowstride;
  }
  weed_free(in_channels);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), weed_channel_template_init("in channel 1", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  weed_plant_t *in_params[] = {weed_float_init("delta", "_Delta", .2, 0., 1.), weed_float_init("opacity", "_Opacity", 1., 0., 1.), weed_colRGBi_init("col", "_Colour", 0, 0, 255), NULL};

  weed_plant_t *filter_class;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  filter_class = weed_filter_class_init("colour key", "salsaman", 1, filter_flags, NULL, &ckey_process,
                                        NULL, in_chantmpls, out_chantmpls,
                                        in_params,
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);

}
WEED_SETUP_END;
