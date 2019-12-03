
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
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
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

  irowstride /= psize;
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
      origin = (b * irowstride + a) * psize;

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
  unsigned char *src, *dst;
  weed_plant_t *in_channel, *out_channel;
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  int width, height, irowstride, orowstride, palette;
  int psize = 3;
  int zoom, autozoom;

  int path = weed_get_int_value(inst, "plugin_path", NULL);
  int zpath = weed_get_int_value(inst, "plugin_zpath", NULL);

  int offset = 0, dheight;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  src = (unsigned char *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  dst = (unsigned char *)weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  autozoom = weed_get_boolean_value(in_params[1], WEED_LEAF_VALUE, NULL);

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
    dst += offset * orowstride;
  } else dheight = height;

  if (autozoom == WEED_TRUE) {
    weed_set_int_value(inst, "plugin_zpath", (zpath + 1) & 255);
  } else {
    zpath = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
    weed_set_int_value(inst, "plugin_zpath", zpath);
  }
  zoom = roto2[zpath];

  if (palette == WEED_PALETTE_UYVY || palette == WEED_PALETTE_YUYV) width >>= 1; // 2 pixels per macropixel

  if (palette == WEED_PALETTE_ARGB32 || palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32 ||
      palette == WEED_PALETTE_YUVA8888 || palette == WEED_PALETTE_UYVY || palette == WEED_PALETTE_YUYV) psize = 4;

  draw_tile(roto[path], roto[(path + 128) & 0xFF], zoom, src, dst, width, irowstride, orowstride, height, dheight, offset, psize);

  weed_set_int_value(inst, "plugin_path", (path - 1) & 255);

  weed_free(in_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int i;
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
  for (i = 0; i < 256; i++) {
    float rad = (float)i * 1.41176 * 0.0174532;
    float c = sin(rad);
    roto[i] = (c + 0.8) * 4096.0;
    roto2[i] = (2.0 * c) * 4096.0;
  }
}
WEED_SETUP_END;




