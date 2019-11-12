// alien_overlay.c
// Weed plugin
// (c) G. Finch (salsaman) 2005 - 2019
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
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdio.h>

typedef struct {
  uint8_t *inited;
  unsigned char *old_pixel_data;
} static_data;

//////////////////////////////////////////////

static weed_error_t alien_over_init(weed_plant_t *inst) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error);

  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error) * 3;

  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->old_pixel_data = (unsigned char *)weed_malloc(height * width);
  if (sdata->old_pixel_data == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->inited = (uint8_t *)weed_malloc(height);
  if (sdata->inited == NULL) {
    weed_free(sdata);
    weed_free(sdata->old_pixel_data);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->inited, 0, height);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t alien_over_deinit(weed_plant_t *inst) {
  int error;
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) {
    weed_free(sdata->inited);
    weed_free(sdata->old_pixel_data);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }

  return WEED_SUCCESS;
}


static weed_error_t alien_over_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL), *out_channel = weed_get_plantptr_value(inst,
                             WEED_LEAF_OUT_CHANNELS,
                             NULL);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL) * 3;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int inplace = (src == dst);
  unsigned char val;

  unsigned char *old_pixel_data;
  unsigned char *end = src + height * irowstride;
  static_data *sdata;
  register int j, i = 0;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  old_pixel_data = sdata->old_pixel_data;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

    src += offset * irowstride;
    dst += offset * orowstride;
    end = src + dheight * irowstride;
    old_pixel_data += width * offset;
    i = offset;
  }

  for (; src < end; src += irowstride) {
    for (j = 0; j < width; j++) {
      if (sdata->inited[i]) {
        if (!inplace) {
          dst[j] = ((char)(old_pixel_data[j]) + (char)(src[j])) >> 1;
          old_pixel_data[j] = src[j];
        } else {
          val = ((char)(old_pixel_data[j]) + (char)(src[j])) >> 1;
          old_pixel_data[j] = src[j];
          dst[j] = val;
        }
      } else old_pixel_data[j] = dst[j] = src[j];
    }
    sdata->inited[i++] = 1;
    dst += orowstride;
    old_pixel_data += width;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("alien overlay", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD, alien_over_init,
                               alien_over_process, alien_over_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

