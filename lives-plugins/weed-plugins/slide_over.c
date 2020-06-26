// simple_blend.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM
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

static inline int pick_direction(uint64_t fastrand_val) {
  return ((fastrand_val >> 24) & 0x03) + 1;
}


static weed_error_t sover_init(weed_plant_t *inst) {
  int dirpref;
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  if (weed_get_boolean_value(in_params[1], WEED_LEAF_VALUE, NULL) == WEED_TRUE) dirpref = 0;
  else if (weed_get_boolean_value(in_params[2], WEED_LEAF_VALUE, NULL) == WEED_TRUE) dirpref = 1; // left to right
  else if (weed_get_boolean_value(in_params[3], WEED_LEAF_VALUE, NULL) == WEED_TRUE) dirpref = 2; // right to left
  else if (weed_get_boolean_value(in_params[4], WEED_LEAF_VALUE, NULL) == WEED_TRUE) dirpref = 3; // top to bottom
  else dirpref = 4; // bottom to top

  weed_set_int_value(inst, "plugin_direction", dirpref);
  return WEED_SUCCESS;
}


static weed_error_t sover_process(weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL),
                 *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  unsigned char *src1 = weed_get_voidptr_value(in_channels[0], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *src2 = weed_get_voidptr_value(in_channels[1], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int width = weed_get_int_value(in_channels[0], WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channels[0], WEED_LEAF_HEIGHT, NULL);
  int irowstride1 = weed_get_int_value(in_channels[0], WEED_LEAF_ROWSTRIDES, NULL);
  int irowstride2 = weed_get_int_value(in_channels[1], WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int palette = weed_get_int_value(in_channels[0], WEED_LEAF_CURRENT_PALETTE, NULL);
  weed_plant_t **in_params;

  register int j;

  int transval;
  int dirn;
  int mvlower, mvupper;
  int bound;
  int psize = pixel_size(palette);

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  transval = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  dirn = weed_get_int_value(inst, "plugin_direction", NULL);
  mvlower = weed_get_boolean_value(in_params[6], WEED_LEAF_VALUE, NULL);
  mvupper = weed_get_boolean_value(in_params[7], WEED_LEAF_VALUE, NULL);

  if (dirn == 0) {
    dirn = pick_direction(fastrand(0)); // random
    weed_set_int_value(inst, "plugin_direction", dirn);
  }

  // upper is src1, lower is src2
  // if mvupper, src1 moves, otherwise it stays fixed
  // if mvlower, src2 moves, otherwise it stays fixed

  // direction tells which way bound moves
  // bound is dividing line between src1 and src2

  switch (dirn) {
  case 3:
    // top to bottom
    bound = (float)height * (1. - transval / 255.); // how much of src1 to show
    if (mvupper) src1 += irowstride1 * (height - bound); // if mvupper, slide a part off the top
    for (j = 0; j < bound; j++) {
      weed_memcpy(dst, src1, width * psize);
      src1 += irowstride1;
      if (!mvlower) src2 += irowstride2; // if !mvlower, cover part of src2
      dst += orowstride;
    }
    for (j = bound; j < height; j++) {
      weed_memcpy(dst, src2, width * psize);
      src2 += irowstride2;
      dst += orowstride;
    }
    break;
  case 4:
    // bottom to top
    bound = (float)height * (transval / 255.);
    if (mvlower) src2 += irowstride2 * (height - bound); // if mvlower we slide in src2 from the top
    if (!mvupper) src1 += irowstride1 * bound;
    for (j = 0; j < bound; j++) {
      weed_memcpy(dst, src2, width * psize);
      src2 += irowstride2;
      dst += orowstride;
    }
    for (j = bound; j < height; j++) {
      weed_memcpy(dst, src1, width * psize);
      src1 += irowstride1;
      dst += orowstride;
    }
    break;
  case 1:
    // left to right
    bound = (float)width * (1. - transval / 255.);
    for (j = 0; j < height; j++) {
      weed_memcpy(dst, src1 + (width - bound) * psize * mvupper, bound * psize);
      weed_memcpy(dst + bound * psize, src2 + bound * psize * !mvlower, (width - bound) * psize);
      src1 += irowstride1;
      src2 += irowstride2;
      dst += orowstride;
    }
    break;
  case 2:
    // right to left
    bound = (float)width * (transval / 255.);
    for (j = 0; j < height; j++) {
      weed_memcpy(dst, src2 + (width - bound) * psize * mvlower, bound * psize);
      weed_memcpy(dst + bound * psize, src1 + !mvupper * bound * psize, (width - bound) * psize);
      src1 += irowstride1;
      src2 += irowstride2;
      dst += orowstride;
    }
    break;
  }

  weed_free(in_params);
  weed_free(in_channels);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES_PLUS;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0),
                                  weed_channel_template_init("in channel 1", 0), NULL
                                 };
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_integer_init("amount", "Transition _value", 0, 0, 255),
                               weed_radio_init("dir_rand", "_Random", 1, 1),
                               weed_radio_init("dir_r2l", "_Right to left", 0, 1),
                               weed_radio_init("dir_l2r", "_Left to right", 0, 1),
                               weed_radio_init("dir_b2t", "_Bottom to top", 0, 1),
                               weed_radio_init("dir_t2b", "_Top to bottom", 0, 1),
                               weed_switch_init("mlower", "_Slide lower clip", WEED_TRUE),
                               weed_switch_init("mupper", "_Slide upper clip", WEED_FALSE), NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("slide over", "salsaman", 1, 0, palette_list,
                               sover_init, sover_process, NULL, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plant_t *gui = weed_filter_get_gui(filter_class);

  char *rfx_strings[] = {"layout|p0|", "layout|hseparator|", "layout|fill|\"Slide direction\"|fill|",
                         "layout|p1|", "layout|p2|p3|", "layout|p4|p5|", "layout|hseparator|"
                        };

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 7, rfx_strings);

  weed_set_boolean_value(in_params[0], WEED_LEAF_IS_TRANSITION, WEED_TRUE);

  weed_set_int_value(in_params[1], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  weed_set_int_value(in_params[2], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  weed_set_int_value(in_params[3], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  weed_set_int_value(in_params[4], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  weed_set_int_value(in_params[5], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

