// multi_blends.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////////////////

static weed_error_t common_init(weed_plant_t *inst) {
  return WEED_SUCCESS;
}


static weed_error_t common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL), *out_channel = weed_get_plantptr_value(inst,
                               WEED_LEAF_OUT_CHANNELS,
                               NULL);
  unsigned char *src1 = weed_get_voidptr_value(in_channels[0], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *src2 = weed_get_voidptr_value(in_channels[1], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int width = weed_get_int_value(in_channels[0], WEED_LEAF_WIDTH, NULL) * 3;
  int height = weed_get_int_value(in_channels[0], WEED_LEAF_HEIGHT, NULL);
  int palette = weed_get_int_value(in_channels[0], WEED_LEAF_CURRENT_PALETTE, NULL);
  int irowstride1 = weed_get_int_value(in_channels[0], WEED_LEAF_ROWSTRIDES, NULL);
  int irowstride2 = weed_get_int_value(in_channels[1], WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  unsigned char *end = src1 + height * irowstride1;
  weed_plant_t *in_param;
  int intval;

  register int j;

  int bf;
  unsigned char luma1, luma2;

  unsigned char pixel[3];
  unsigned char blend_factor, blend1, blend2, blendneg1, blendneg2;

  in_param = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  bf = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);
  blend_factor = (unsigned char)bf;

  blend1 = blend_factor * 2;
  blendneg1 = 255 - blend_factor * 2;

  blend2 = (255 - blend_factor) * 2;
  blendneg2 = (blend_factor - 128) * 2;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
    src1 += offset * irowstride1;
    end = src1 + dheight * irowstride1;
    src2 += offset * irowstride2;
    dst += offset * orowstride;
  }

  for (; src1 < end; src1 += irowstride1) {
    for (j = 0; j < width; j += 3) {
      switch (type) {
      case 0:
        // multiply
        pixel[0] = (unsigned char)((src2[j] * src1[j]) >> 8);
        pixel[1] = (unsigned char)((src2[j + 1] * src1[j + 1]) >> 8);
        pixel[2] = (unsigned char)((src2[j + 2] * src1[j + 2]) >> 8);
        break;
      case 1:
        // screen
        pixel[0] = (unsigned char)(255 - (((255 - src2[j]) * (255 - src1[j])) >> 8));
        pixel[1] = (unsigned char)(255 - (((255 - src2[j + 1]) * (255 - src1[j + 1])) >> 8));
        pixel[2] = (unsigned char)(255 - (((255 - src2[j + 2]) * (255 - src1[j + 2])) >> 8));
        break;
      case 2:
        // darken
        luma1 = (unsigned char)(calc_luma(&src1[j], palette, 0));
        luma2 = (unsigned char)(calc_luma(&src2[j], palette, 0));
        if (luma1 <= luma2) weed_memcpy(pixel, &src1[j], 3);
        else weed_memcpy(pixel, &src2[j], 3);
        break;
      case 3:
        // lighten
        luma1 = (unsigned char)(calc_luma(&src1[j], palette, 0));
        luma2 = (unsigned char)(calc_luma(&src2[j], palette, 0));
        if (luma1 >= luma2) weed_memcpy(pixel, &src1[j], 3);
        else weed_memcpy(pixel, &src2[j], 3);
        break;
      case 4:
        // overlay
        luma1 = calc_luma(&src1[j], palette, 0);
        if (luma1 < 128) {
          // mpy
          pixel[0] = (unsigned char)((src2[j] * src1[j]) >> 8);
          pixel[1] = (unsigned char)((src2[j + 1] * src1[j + 1]) >> 8);
          pixel[2] = (unsigned char)((src2[j + 2] * src1[j + 2]) >> 8);
        } else {
          // screen
          pixel[0] = (unsigned char)(255 - (((255 - src2[j]) * (255 - src1[j])) >> 8));
          pixel[1] = (unsigned char)(255 - (((255 - src2[j + 1]) * (255 - src1[j + 1])) >> 8));
          pixel[2] = (unsigned char)(255 - (((255 - src2[j + 2]) * (255 - src1[j + 2])) >> 8));
        }
        break;
      case 5:
        // dodge
        if (src2[j] == 255) pixel[0] = 255;
        else {
          intval = ((int)(src1[j]) << 8) / (int)(255 - src2[j]);
          pixel[0] = intval > 255 ? 255 : (unsigned char)intval;
        }
        if (src2[j + 1] == 255) pixel[1] = 255;
        else {
          intval = ((int)(src1[j + 1]) << 8) / (int)(255 - src2[j + 1]);
          pixel[1] = intval > 255 ? 255 : (unsigned char)intval;
        }
        if (src2[j + 2] == 255) pixel[2] = 255;
        else {
          intval = ((int)(src1[j + 2]) << 8) / (int)(255 - src2[j + 2]);
          pixel[2] = intval > 255 ? 255 : (unsigned char)intval;
        }
        break;
      case 6:
        // burn
        if (src2[j] == 0) pixel[0] = 0;
        else {
          intval = 255 - (255 - ((int)(src1[j]) << 8)) / (int)(src2[j]);
          pixel[0] = intval < 0 ? 0 : (unsigned char)intval;
        }
        if (src2[j + 1] == 0) pixel[1] = 0;
        else {
          intval = 255 - (255 - ((int)(src1[j + 1]) << 8)) / (int)(src2[j + 1]);
          pixel[1] = intval < 0 ? 0 : (unsigned char)intval;
        }
        if (src2[j + 2] == 0) pixel[2] = 0;
        else {
          intval = 255 - (255 - ((int)(src1[j + 2]) << 8)) / (int)(src2[j + 2]);
          pixel[2] = intval < 0 ? 0 : (unsigned char)intval;
        }
        break;
      }
      if (blend_factor < 128) {
        dst[j] = (blend1 * pixel[0] + blendneg1 * src1[j]) >> 8;
        dst[j + 1] = (blend1 * pixel[1] + blendneg1 * src1[j + 1]) >> 8;
        dst[j + 2] = (blend1 * pixel[2] + blendneg1 * src1[j + 2]) >> 8;
      } else {
        dst[j] = (blend2 * pixel[0] + blendneg2 * src2[j]) >> 8;
        dst[j + 1] = (blend2 * pixel[1] + blendneg2 * src2[j + 1]) >> 8;
        dst[j + 2] = (blend2 * pixel[2] + blendneg2 * src2[j + 2]) >> 8;
      }
    }
    src2 += irowstride2;
    dst += orowstride;
  }
  weed_free(in_channels);
  return WEED_SUCCESS;
}


static weed_error_t common_deinit(weed_plant_t *filter_instance) {
  return WEED_SUCCESS;
}


static weed_error_t mpy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0, inst, timestamp);
}


static weed_error_t screen_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1, inst, timestamp);
}


static weed_error_t darken_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2, inst, timestamp);
}


static weed_error_t lighten_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(3, inst, timestamp);
}


static weed_error_t overlay_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(4, inst, timestamp);
}


static weed_error_t dodge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(5, inst, timestamp);
}


static weed_error_t burn_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(6, inst, timestamp);
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2, **clone3;

  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), weed_channel_template_init("in channel 1", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_params1[] = {weed_integer_init("amount", "Blend _amount", 128, 0, 255), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("blend_multiply", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list, NULL,
                               mpy_process, NULL, in_chantmpls, out_chantmpls, in_params1, NULL);

  weed_set_boolean_value(in_params1[0], WEED_LEAF_IS_TRANSITION, WEED_TRUE);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("blend_screen", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, screen_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  filter_class = weed_filter_class_init("blend_darken", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, darken_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  filter_class = weed_filter_class_init("blend_lighten", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, lighten_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  filter_class = weed_filter_class_init("blend_overlay", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, overlay_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  filter_class = weed_filter_class_init("blend_dodge", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, dodge_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  filter_class = weed_filter_class_init("blend_burn", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        NULL, burn_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params1)), NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

