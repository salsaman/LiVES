// fg_bg_removal.c
// Weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS
#define NEED_PALETTE_UTILS
#define NEED_RANDOM

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

typedef struct {
  uint8_t *av_luma_data;
  unsigned int av_count;
  uint64_t fv;
} static_data;


static weed_error_t common_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int height, width;

  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);

  sdata->av_luma_data = (uint8_t *)weed_calloc(width * height * 3, 1);
  if (sdata->av_luma_data == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->av_count = 0;

  //
  //inst->num_in_parameters=1;
  //inst->in_parameters=malloc(sizeof(weed_parameter_t));
  //inst->in_parameters[0]=weed_plugin_info_integer_init("Threshold",64,0,255);
  //
  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  sdata->fv = fastrand(0);

  return WEED_SUCCESS;
}


static weed_error_t common_deinit(weed_plant_t *inst) {
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    weed_free(sdata->av_luma_data);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t common_process(int type, weed_plant_t *inst, weed_timecode_t timestamp) {
  static_data *sdata;
  uint8_t *av_luma_data;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL), *in_param;
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL) * 3;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  unsigned char *end = src + height * irowstride;
  int inplace = (src == dest);
  int red = 0, blue = 2;

  register int j;

  uint8_t luma;
  uint8_t av_luma;
  int bf;
  uint8_t luma_threshold = 128;

  if (palette == WEED_PALETTE_BGR24 || palette == WEED_PALETTE_BGRA32) {
    red = 2;
    blue = 0;
  }

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

    src += offset * irowstride;
    dest += offset * orowstride;
    end = src + dheight * irowstride;
  }

  in_param = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  bf = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);
  luma_threshold = (uint8_t)bf;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  av_luma_data = sdata->av_luma_data;

  for (; src < end; src += irowstride) {
    for (j = 0; j < width - 2; j += 3) {
      sdata->fv = fastrand(sdata->fv);
      luma = calc_luma(&src[j], palette, 0);
      av_luma = (uint8_t)((double)luma / (double)sdata->av_count + (double)(av_luma_data[j / 3] * sdata->av_count) / (double)(
                            sdata->av_count + 1));
      sdata->av_count++;
      av_luma_data[j / 3] = av_luma;
      if (ABS(luma - av_luma) < (luma_threshold)) {
        switch (type) {
        case 1:
          // fire-ish effect
          dest[j + red] = (uint8_t)((uint8_t)((sdata->fv & 0x7f00) >> 8) + (dest[j + 1] = (uint8_t)((fastrand(
                                      sdata->fv) & 0x7f00) >> 8))); //R & G
          dest[j + blue] = (uint8_t)0;                   //B
          break;
        //
        case 2:
          // blue glow
          dest[j + red] = dest[j + 1] = (uint8_t)((sdata->fv & 0xff00) >> 8);                                       //R&G
          dest[j + blue] = (uint8_t)255; //B
          break;
        case 0:
          // make moving things black
          blank_pixel(&dest[j], palette, 0, NULL);
          break;
        }
      } else {
        if (!inplace) weed_memcpy(&dest[j], &src[j], 3);
      }
    }
    dest += orowstride;
    av_luma_data += width;
  }
  return WEED_SUCCESS;
}


static weed_error_t t1_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0, inst, timestamp);
}

static weed_error_t t2_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1, inst, timestamp);
}

static weed_error_t t3_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2, inst, timestamp);
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2, **clone3;
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};

  weed_plant_t *in_params[] = {weed_integer_init("threshold", "_Threshold", 64, 0, 255), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("fg_bg_removal type 1", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                               common_init, t1_process, common_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  // we must clone the arrays for the next filter
  filter_class = weed_filter_class_init("fg_bg_removal type 2", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                                        common_init, t2_process, common_deinit,
                                        (clone1 = weed_clone_plants(in_chantmpls)), (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)), NULL);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  // we must clone the arrays for the next filter
  filter_class = weed_filter_class_init("fg_bg_removal type 3", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD | WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                                        common_init, t3_process, common_deinit,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), (clone3 = weed_clone_plants(in_params)), NULL);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

