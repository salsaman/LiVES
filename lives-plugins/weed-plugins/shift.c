/////////////////////////////////////////////////////////////////////////////
// Weed shift plugin, version 1
// Compiled with Builder version 3.2.1-pre
// autogenerated from script shift.script

// released under the GNU GPL version 3 or higher
// see file COPYING or www.gnu.org for details

// (c) salsaman 2022
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////

#define _UNIQUE_ID_ "0X50D8DEA3BA293FD4"
#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

//////////////////////////////////////////////////////////////////

#include <stdio.h>

static int verbosity = WEED_VERBOSITY_ERROR;
enum {
  P_xshift,
  P_yshift,
  P_transbg,
};

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
  int psize = pixel_size(pal);
  for (int i = 0; i < xwidth; i += psize) add_bg_pixel(ptr + i, pal, clamping, trans);
}

/////////////////////////////////////////////////////////////////////////////

static weed_error_t shift_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int is_threading = weed_is_threading(inst);
  int offset = 0;
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  double xshift = weed_param_get_value_double(in_params[P_xshift]);
  double yshift = weed_param_get_value_double(in_params[P_yshift]);
  int transbg = weed_param_get_value_boolean(in_params[P_transbg]);
  weed_free(in_params);

  if (is_threading) {
    offset = weed_channel_get_offset(out_chan);
    src += offset * irow;
    dst += offset * orow;
  }

  if (1) {
    int sx, sy, ypos;
    int istart, iend;
    int send = irow * iheight;
    int clamping = WEED_YUV_CLAMPING_CLAMPED;
    int x = (int)(xshift * (double)width + .5);
    int y = (int)(yshift * (double)iheight + .5) * irow;
    int offset = weed_channel_get_offset(out_chan);

    if (weed_palette_is_yuv(pal)) clamping = weed_channel_get_yuv_clamping(in_chan);
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

    for (int i = 0; i < height; i++) {
      ypos = i * irow;
      sy = ypos - y;

      if (sy < 0 || sy >= send) {
        add_bg_row(dst, width, pal, clamping, transbg);
        continue;
      }

      if (x > 0) {
        add_bg_row(dst, x, pal, clamping, transbg);
        sx = 0;
      } else sx = -x;

      if (istart < iend) {
        weed_memcpy(&dst[istart], src + sy + sx, iend - istart);
      }

      if (iend < width) add_bg_row(&dst[iend], width - iend, pal, clamping, transbg);
    }
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  uint64_t unique_id;
  int palette_list[] = ALL_PACKED_PALETTES;
  weed_plant_t *in_chantmpls[] = {
      weed_channel_template_init("in_channel0", 0),
      NULL};
  weed_plant_t *out_chantmpls[] = {
      weed_channel_template_init("out_channel0", 0),
      NULL};
  weed_plant_t *in_paramtmpls[] = {
      weed_float_init("xshift", "_X shift (ratio)", 0., -1., 1.),
      weed_float_init("yshift", "_Y shift (ratio)", 0., -1., 1.),
      weed_switch_init("transbg", "_Transparent edges", WEED_FALSE),
      NULL};
  weed_plant_t *pgui;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  verbosity = weed_get_host_verbosity(host_info);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_xshift]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_yshift]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  filter_class = weed_filter_class_init("shift", "salsaman", 1, filter_flags, palette_list,
    NULL, shift_process, NULL, in_chantmpls, out_chantmpls, in_paramtmpls, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  if (!sscanf(_UNIQUE_ID_, "0X%lX", &unique_id) || !sscanf(_UNIQUE_ID_, "0x%lx", &unique_id)) {
    weed_set_int64_value(plugin_info, WEED_LEAF_UNIQUE_ID, unique_id);
  }

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

