/////////////////////////////////////////////////////////////////////////////
// Weed alien_overlay plugin, version 1
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by salsaman
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////

#define _UNIQUE_ID_ "0XFCEDBE929BCF32A4"

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

//////////////////////////////////////////////////////////////////

#include <stdio.h>

static int verbosity = WEED_VERBOSITY_ERROR;

typedef struct {
  uint8_t *inited;
  unsigned char *old_pixel_data;
} sdata_t;


static weed_error_t alien_overlay_init(weed_plant_t *inst) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata;
  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    sdata = (sdata_t *)weed_calloc(1, sizeof(sdata_t));
    if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

    if (1) {
      sdata->old_pixel_data = (unsigned char *)weed_malloc(height * width * 3);
      if (!sdata->old_pixel_data) {
        weed_free(sdata);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }

      sdata->inited = (uint8_t *)weed_calloc(height, 1);
      if (!sdata->inited) {
        weed_free(sdata);
        weed_free(sdata->old_pixel_data);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
    }
    weed_set_voidptr_value(inst, "plugin_internal", sdata);
  }
  else sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t alien_overlay_deinit(weed_plant_t *inst) {
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->inited) weed_free(sdata->inited);
    if (sdata->old_pixel_data) weed_free(sdata->old_pixel_data);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t alien_overlay_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int is_threading = weed_is_threading(inst);
  int offset = 0;
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (!sdata) return WEED_ERROR_REINIT_NEEDED;
  else {

    if (is_threading) {
      offset = weed_channel_get_offset(out_chan);
      src += offset * irow;
      dst += offset * orow;
    }

    if (1) {
      /// the secret to this effect is that we deliberately cast the values to (char)
      /// rather than (unsigned char)
      /// when averaging the current frame with the prior one
      // - the overflow when converted back produces some interesting visuals
      int offset = weed_channel_get_offset(out_chan);
      int offs = rgb_offset(pal);
      int xwidth = width * psize;
      int row = offset;
      unsigned char *old_pixel_data, val;
      width *= 3;
      old_pixel_data = sdata->old_pixel_data + width * offset;

      for (int i = 0; i < height; i++) {
        int l = 0;
        for (int j = offs; j < xwidth; j += psize, l += 3) {
          for (int k = 0; k < 3; k++) {
            if (sdata->inited[row]) {
              if (!inplace) {
                dst[orow * i + j + k] = ((char)(old_pixel_data[width * i + l + k])
                  + (char)(src[irow * i + j + k])) >> 1;
                old_pixel_data[width * i + l + k] = src[irow * i + j + k];
              } else {
                val = ((char)(old_pixel_data[width * i + l + k]) + (char)(src[irow * i + l + k])) >> 1;
                old_pixel_data[width * i + l + k] = src[irow * i + j + k];
                dst[orow * i + j + k] = val;
              }
            } else old_pixel_data[width * i + l + k] = dst[orow * i + j + k] = src[irow * i + j + k];
          }
        }
        sdata->inited[row + i] = 1;
      }
    }
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  uint64_t unique_id;
  int palette_list[] = ALL_RGB_PALETTES;
  weed_plant_t *in_chantmpls[] = {
      weed_channel_template_init("in_channel0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE),
      NULL};
  weed_plant_t *out_chantmpls[] = {
      weed_channel_template_init("out_channel0", WEED_CHANNEL_CAN_DO_INPLACE),
      NULL};
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  verbosity = weed_get_host_verbosity(host_info);

  filter_class = weed_filter_class_init("alien overlay", "salsaman", 1, filter_flags, palette_list,
    alien_overlay_init, alien_overlay_process, alien_overlay_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  if (!sscanf(_UNIQUE_ID_, "0X%lX", &unique_id) || !sscanf(_UNIQUE_ID_, "0x%lx", &unique_id)) {
    weed_set_int64_value(plugin_info, WEED_LEAF_UNIQUE_ID, unique_id);
  }

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

