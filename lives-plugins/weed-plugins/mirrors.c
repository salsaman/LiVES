// mirrors.c
// weed plugin
// (c) G. Finch (salsaman) 2005
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

static weed_error_t mirrorx_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  int inplace = (src == dst);
  int pal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL), hwidth;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int psize;

  unsigned char *end = src + height * irowstride;
  register int i;

  psize = pixel_size(pal);

  width *= psize;
  hwidth = (width >> 2) << 1;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

    src += offset * irowstride;
    dst += offset * orowstride;
    end = src + dheight * irowstride;
  }

  for (; src < end; src += irowstride) {
    for (i = 0; i < hwidth; i += psize) {
      weed_memcpy(&dst[width - i - psize], &src[i], psize);
      if (!inplace) weed_memcpy(&dst[i], &src[i], psize);
    }
    dst += orowstride;
  }
  return WEED_SUCCESS;
}


static weed_error_t mirrory_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  int palette = weed_channel_get_palette(in_channel);
  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);

  int psize = pixel_size(pal);
  unsigned char *end = dst + height * orowstride / 2, *oend = dst + (height - 1) * orowstride;

  width *= psize;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
    src += offset * irowstride;
    oend = end = dst + (dheight + offset) * orowstride;
    if (end > dst + height * orowstride / 2) end = dst + height * orowstride / 2;
    dst += offset * orowstride;
  }

  if (weed_plant_has_leaf(inst, "plugin_combined") && weed_get_boolean_value(inst, "plugin_combined", NULL) == WEED_TRUE) {
    inplace = WEED_TRUE;
    src = dst;
    irowstride = orowstride;
  }

  if (!inplace) {
    for (; dst < end; dst += orowstride) {
      weed_memcpy(dst, src, width);
      src += irowstride;
    }
  } else {
    src = dst = end;
  }
  end = oend;

  for (; dst < end; dst += orowstride) {
    weed_memcpy(dst, src, width);
    src -= irowstride;
  }
  return WEED_SUCCESS;
}


static weed_error_t mirrorxy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_error_t ret = mirrory_process(inst, timestamp);
  if (ret != WEED_SUCCESS) return ret;
  weed_set_boolean_value(inst, "plugin_combined", WEED_TRUE);
  ret = mirrorx_process(inst, timestamp);
  return ret;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2;
  // all planar palettes
  int palette_list[] = ALL_PACKED_PALETTES_PLUS;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("mirrorx", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, mirrorx_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("mirrory", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        NULL, mirrory_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);

  filter_class = weed_filter_class_init("mirrorxy", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        NULL, mirrorxy_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


