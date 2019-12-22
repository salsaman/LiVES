// alien_overlay.c
// Weed plugin
// (c) G. Finch (salsaman) 2005 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

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
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  int width = weed_channel_get_width(in_channel) * 3;
  int height = weed_channel_get_height(in_channel);
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
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    weed_free(sdata->inited);
    weed_free(sdata->old_pixel_data);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t alien_over_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0), *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  unsigned char *old_pixel_data;
  unsigned char val;
  int pal = weed_channel_get_palette(in_channel);
  int psize = pixel_size(pal);
  int width = weed_channel_get_width(in_channel) * psize;
  int height = weed_channel_get_height(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  unsigned char *end = dst + height * orowstride;
  int inplace = (src == dst);
  int offs = rgb_offset(pal);
  int row = 0;
  register int j, k;

  old_pixel_data = sdata->old_pixel_data;

  // threading
  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_channel);
    int dheight = weed_channel_get_height(out_channel);
    src += offset * irowstride;
    dst += offset * orowstride;
    end = dst + dheight * orowstride;
    old_pixel_data += width / psize * 3 * offset;
    row = offset;
  }

  /// the secret to this effect is that we deliberately cast the values to (char) rather than (unsigned char)
  /// when averaging the current frame with the prior one - the overflow when converted back produces some interesting visuals
  for (; dst < end; dst += orowstride) {
    for (j = offs; j < width; j += psize) {
      for (k = 0; k < 3; k++) {
        if (sdata->inited[row]) {
          if (!inplace) {
            dst[j + k] = ((char)(*old_pixel_data) + (char)(src[j + k])) >> 1;
            *old_pixel_data = src[j + k];
          } else {
            val = ((char)(*old_pixel_data) + (char)(src[j + k])) >> 1;
            *old_pixel_data = src[j + k];
            dst[j + k] = val;
          }
        } else *old_pixel_data = dst[j + k] = src[j + k];
        old_pixel_data++;
      }
    }

    sdata->inited[row++] = 1;
    src += irowstride;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_RGB_PALETTES;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("alien overlay", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD, palette_list, alien_over_init,
                               alien_over_process, alien_over_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

