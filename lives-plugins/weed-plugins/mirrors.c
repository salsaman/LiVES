// mirrors.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#else
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

int mirrorx_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error), *out_channel = weed_get_plantptr_value(inst,
                             WEED_LEAF_OUT_CHANNELS,
                             &error);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, &error);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, &error);
  int inplace = (src == dst);
  int pal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, &error);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error), hwidth;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, &error);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error);
  int psize = 4;
  unsigned char *end = src + height * irowstride;
  register int i;

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) psize = 3;

  width *= psize;
  hwidth = (width >> 2) << 1;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, &error);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, &error);

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


int mirrory_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error), *out_channel = weed_get_plantptr_value(inst,
                             WEED_LEAF_OUT_CHANNELS,
                             &error);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, &error);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, &error);
  int inplace = (src == dst);
  int pal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, &error);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, &error);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error);
  int psize = 4;
  unsigned char *end = dst + height * orowstride / 2, *oend = dst + (height - 1) * orowstride;

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) psize = 3;
  if (pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV) width >>= 1; // 2 pixels per macropixel

  width *= psize;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, &error);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, &error);
    src += offset * irowstride;
    oend = end = dst + (dheight + offset) * orowstride;
    if (end > dst + height * orowstride / 2) end = dst + height * orowstride / 2;
    dst += offset * orowstride;
  }

  if (weed_plant_has_leaf(inst, "plugin_combined") && weed_get_boolean_value(inst, "plugin_combined", &error) == WEED_TRUE) {
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


int mirrorxy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int ret = mirrory_process(inst, timestamp);
  if (ret != WEED_SUCCESS) return ret;
  weed_set_boolean_value(inst, "plugin_combined", WEED_TRUE);
  ret = mirrorx_process(inst, timestamp);
  return ret;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2;
  // all planar palettes
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_RGBA32, WEED_PALETTE_ARGB32, WEED_PALETTE_BGRA32, WEED_PALETTE_UYVY, WEED_PALETTE_YUYV, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("mirrorx", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, NULL, &mirrorx_process, NULL,
                               in_chantmpls, out_chantmpls,
                               NULL, NULL);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("mirrory", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, NULL, &mirrory_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);

  filter_class = weed_filter_class_init("mirrorxy", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, NULL, &mirrorxy_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


