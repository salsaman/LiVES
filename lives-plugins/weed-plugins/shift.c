// shift.c
// weed plugin
// (c) G. Finch (salsaman) 2012
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
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", NULL),
                *out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", NULL);

  unsigned char *src = (unsigned char *)weed_get_voidptr_value(in_channel, "pixel_data", NULL);
  unsigned char *dst = (unsigned char *)weed_get_voidptr_value(out_channel, "pixel_data", NULL);

  int width = weed_get_int_value(in_channel, "width", NULL);
  int sheight = weed_get_int_value(in_channel, "height", NULL);
  int irowstride = weed_get_int_value(in_channel, "rowstrides", NULL);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", NULL);

  unsigned char *dend;

  size_t send = irowstride * sheight;

  int x = (int)(weed_get_double_value(in_params[0], "value", NULL) * (double)width + .5);
  int y = (int)(weed_get_double_value(in_params[1], "value", NULL) * (double)sheight + .5) * irowstride;
  int trans = weed_get_boolean_value(in_params[2], "value", NULL);

  int offset = 0;
  int dheight = weed_get_int_value(out_channel, "height", NULL); // may differ because of threading

  int pal = weed_get_int_value(in_channel, "current_palette", NULL);

  int psize = 4;

  int sx, sy, ypos;

  int istart, iend;

  int clamping = WEED_YUV_CLAMPING_CLAMPED;

  weed_free(in_params);

  // new threading arch
  if (weed_plant_has_leaf(out_channel, "offset")) {
    offset = weed_get_int_value(out_channel, "offset", NULL);
    dst += offset * orowstride;
  }

  dend = dst + dheight * orowstride;

  ypos = (offset - 1) * irowstride;

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) psize = 3;

  if (pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888)
    clamping = weed_get_int_value(in_channel, "YUV_clamping", NULL);

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
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32,
                        WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *in_params[] = {weed_float_init("xshift", "_X shift (ratio)", 0., -1., 1.),
                               weed_float_init("yshift", "_Y shift (ratio)", 0., -1., 1.),
                               weed_switch_init("transbg", "_Transparent edges", WEED_FALSE),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("shift", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD,
                               NULL, shift_process, NULL,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;

