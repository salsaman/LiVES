// shift.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

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

static void add_bg_pixel(unsigned char *ptr, int pal, int clamping, int trans) {
  if (trans == WEED_TRUE) trans = 0;
  else trans = 255;

  switch (pal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    weed_memset(ptr, 0, 3);
    break;

  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    weed_memset(ptr, 0, 3);
    ptr[3] = trans;
    break;

  case WEED_PALETTE_ARGB32:
    weed_memset(ptr + 1, 0, 3);
    ptr[0] = trans;
    break;

  case WEED_PALETTE_YUV888:
    if (clamping != WEED_YUV_CLAMPING_CLAMPED) ptr[0] = 0;
    else ptr[0] = 16;
    ptr[1] = ptr[2] = 128;
    break;

  case WEED_PALETTE_YUVA8888:
    if (clamping != WEED_YUV_CLAMPING_CLAMPED) ptr[0] = 0;
    else ptr[0] = 16;
    ptr[1] = ptr[2] = 128;
    ptr[3] = trans;
    break;
  }
}


static void add_bg_row(unsigned char *ptr, int xwidth, int pal, int clamping, int trans) {
  register int i;
  int psize = 4;
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) psize = 3;
  for (i = 0; i < xwidth; i += psize) add_bg_pixel(ptr + i, pal, clamping, trans);
}


static weed_error_t shift_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  unsigned char *src = (unsigned char *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = (unsigned char *)weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int sheight = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  unsigned char *dend;

  size_t send = irowstride * sheight;

  int x = (int)(weed_get_double_value(in_params[0], WEED_LEAF_VALUE, NULL) * (double)width + .5);
  int y = (int)(weed_get_double_value(in_params[1], WEED_LEAF_VALUE, NULL) * (double)sheight + .5) * irowstride;
  int trans = weed_get_boolean_value(in_params[2], WEED_LEAF_VALUE, NULL);

  int offset = 0;
  int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL); // may differ because of threading

  int pal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  int psize = 4;

  int sx, sy, ypos;

  int istart, iend;

  int clamping = WEED_YUV_CLAMPING_CLAMPED;

  weed_free(in_params);

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    dst += offset * orowstride;
  }

  dend = dst + dheight * orowstride;

  ypos = (offset - 1) * irowstride;

  psize = pixel_size(pal);

  if (pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888)
    clamping = weed_get_int_value(in_channel, WEED_LEAF_YUV_CLAMPING, NULL);

  x *= psize;
  width *= psize;

  if (x < 0) {
    // shift left
    istart = 0;
    iend = width + x;
    if (iend < 0) iend = 0;
  } else {
    // shift right
    if (x >= width) x = width;
    istart = x;
    iend = width;
  }

  for (; dst < dend; dst += orowstride) {
    ypos += irowstride;
    sy = ypos - y;

    if (sy < 0 || sy >= send) {
      add_bg_row(dst, width, pal, clamping, trans);
      continue;
    }

    if (x > 0) {
      add_bg_row(dst, x, pal, clamping, trans);
      sx = 0;
    } else sx = -x;

    if (istart < iend) {
      weed_memcpy(&dst[istart], src + sy + sx, iend - istart);
    }

    if (iend < width) add_bg_row(&dst[iend], width - iend, pal, clamping, trans);
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("xshift", "_X shift (ratio)", 0., -1., 1.),
                               weed_float_init("yshift", "_Y shift (ratio)", 0., -1., 1.),
                               weed_switch_init("transbg", "_Transparent edges", WEED_FALSE),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("shift", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, shift_process, NULL,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

