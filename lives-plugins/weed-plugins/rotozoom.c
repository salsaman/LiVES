
// rotozoom.c
// original code (c) Kentaro Fukuchi (effecTV)
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
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

static int roto[256];
static int roto2[256];

/////////////////////////////////////////////////////////////

static void draw_tile(int stepx, int stepy, int zoom, unsigned char *src, unsigned char *dst,
                      int video_width, int irowstride, int orowstride, int video_height,
                      int dheight, int offset, int psize) {
  int x, y, xd, yd, a, b, sx = 0, sy = 0;

  int origin;

  register int i, j;

  orowstride -= video_width * psize;

  xd = (stepx * zoom) >> 12;
  yd = (stepy * zoom) >> 12;

  sx = -yd * offset;
  sy = xd * offset;

  /* Stepping across and down the screen, each screen row has a
     starting coordinate in the texture: (sx, sy).  As each screen
     row is traversed, the current texture coordinate (x, y) is
     modified by (xd, yd), which are (sin(rot), cos(rot)) multiplied
     by the current zoom factor.  For each vertical step, (xd, yd)
     is rotated 90 degrees, to become (-yd, xd).

     More fun can be had by playing around with x, y, xd, and yd as
     you move about the image.
  */

  for (j = 0; j < dheight; j++) {
    x = sx;
    y = sy;
    for (i = 0; i < video_width; i++) {
      a = ((x >> 12 & 255) * video_width) >> 8;
      b = ((y >> 12 & 255) * video_height) >> 8;
      //       a*=video_width/64;
      //      b*=video_height/64;
      origin = b * irowstride + a * psize;

      weed_memcpy(dst, &src[origin], psize);
      dst += psize;
      x += xd;
      y += yd;
    }
    dst += orowstride;
    sx -= yd;
    sy += xd;
  }
}

//////////////////////////////////////////////////////////


static weed_error_t rotozoom_init(weed_plant_t *inst) {
  weed_set_int_value(inst, "plugin_path", 0);
  weed_set_int_value(inst, "plugin_zpath", 0);
  return WEED_SUCCESS;
}


static weed_error_t rotozoom_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  int palette = weed_channel_get_palette(in_channel);
  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);

  int psize = pixel_size(palette);
  int zoom, autozoom;

  int path = weed_get_int_value(inst, "plugin_path", NULL);
  int zpath = weed_get_int_value(inst, "plugin_zpath", NULL);

  int offset = 0, dheight;
  unsigned char *src, *dst;

  src = (unsigned char *)weed_channel_get_pixel_data(in_channel);
  dst = (unsigned char *)weed_channel_get_pixel_data(out_channel);

  autozoom = weed_param_get_value_boolean(in_params[1]);

  // threading
  if (weed_is_threading(inst)) {
    offset = weed_channel_get_offset(out_channel);
    dheight = weed_channel_get_height(out_channel);
    dst += offset * orowstride;
  } else dheight = height;

  if (autozoom == WEED_TRUE) {
    weed_set_int_value(inst, "plugin_zpath", (zpath + 1) & 255);
  } else {
    zpath = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
    weed_set_int_value(inst, "plugin_zpath", zpath);
  }
  zoom = roto2[zpath];

  draw_tile(roto[path], roto[(path + 128) & 0xFF], zoom, src, dst, width, irowstride, orowstride, height, dheight, offset, psize);

  weed_set_int_value(inst, "plugin_path", (path - 1) & 255);

  weed_free(in_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES_PLUS;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_integer_init("zoom", "_Zoom value", 128, 0, 255),
                               weed_switch_init("autozoom", "_Auto zoom", WEED_TRUE), NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("rotozoom", "effectTV", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               rotozoom_init, rotozoom_process, NULL, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);

  // static data for all instances
  for (register int i = 0; i < 256; i++) {
    float rad = (float)i * 1.41176 * 0.0174532;
    float c = sin(rad);
    roto[i] = (c + 0.8) * 4096.0;
    roto2[i] = (2.0 * c) * 4096.0;
  }
}
WEED_SETUP_END;




