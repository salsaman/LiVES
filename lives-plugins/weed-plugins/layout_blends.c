// simple_blend.c
// weed plugin
// (c) G. Finch (salsaman) 2005
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


static weed_error_t common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, "in_channels", NULL), *out_channel = weed_get_plantptr_value(inst,
                               "out_channels",
                               NULL);
  unsigned char *src1 = weed_get_voidptr_value(in_channels[0], "pixel_data", NULL);
  unsigned char *src2 = weed_get_voidptr_value(in_channels[1], "pixel_data", NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", NULL);

  int width = weed_get_int_value(in_channels[0], "width", NULL) * 3;
  int height = weed_get_int_value(in_channels[0], "height", NULL);
  int irowstride1 = weed_get_int_value(in_channels[0], "rowstrides", NULL);
  int irowstride2 = weed_get_int_value(in_channels[1], "rowstrides", NULL);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", NULL);

  unsigned char *end = src1 + height * irowstride1;
  int palette = weed_get_int_value(out_channel, "current_palette", NULL);
  int inplace = (src1 == dst);

  weed_plant_t **in_params;
  double xstart, xend, bw, tmp;
  int sym, vert;
  int tmpi, *bc;
  unsigned char *tbs, *tbe, *bbs, *bbe;

  register int j;

  switch (type) {
  case 0:
    // triple split
    in_params = weed_get_plantptr_array(inst, "in_parameters", NULL);
    xstart = weed_get_double_value(in_params[0], "value", NULL);
    sym = weed_get_boolean_value(in_params[1], "value", NULL);
    xend = weed_get_double_value(in_params[3], "value", NULL);
    vert = weed_get_boolean_value(in_params[4], "value", NULL);
    bw = weed_get_double_value(in_params[5], "value", NULL);
    bc = weed_get_int_array(in_params[6], "value", NULL);

    if (sym) {
      xstart /= 2.;
      xend = 1. - xstart;
    }

    if (xstart > xend) {
      tmp = xend;
      xend = xstart;
      xstart = tmp;
    }

    if (palette == WEED_PALETTE_BGR24) {
      tmpi = bc[2];
      bc[2] = bc[0];
      bc[0] = tmpi;
    }

    tbs = tbe = bbs = bbe = end;

    if (vert) {
      tbs = src1 + (int)(height * (xstart - bw) + .5) * irowstride1;
      tbe = src1 + (int)(height * (xstart + bw) + .5) * irowstride1;
      bbs = src1 + (int)(height * (xend - bw) + .5) * irowstride1;
      bbe = src1 + (int)(height * (xend + bw) + .5) * irowstride1;
      xstart = xend = -bw;
    }

    // new threading arch
    if (weed_plant_has_leaf(out_channel, "offset")) {
      int offset = weed_get_int_value(out_channel, "offset", NULL);
      int dheight = weed_get_int_value(out_channel, "height", NULL);

      src1 += offset * irowstride1;
      end = src1 + dheight * irowstride1;

      src2 += offset * irowstride2;
      dst += offset * orowstride;
    }

    for (; src1 < end; src1 += irowstride1) {
      for (j = 0; j < width; j += 3) {
        if ((j < width * (xstart - bw) || j >= width * (xend + bw)) && (src1 <= tbs || src1 >= bbe)) {
          weed_memcpy(&dst[j], &src2[j], 3);
          continue;
        }
        if ((j > width * (xstart + bw) && j < width * (xend - bw)) || (src1 > tbe && src1 < bbs)) {
          if (!inplace) weed_memcpy(&dst[j], &src1[j], 3);
          continue;
        }
        dst[j] = bc[0];
        dst[j + 1] = bc[1];
        dst[j + 2] = bc[2];
      }
      src2 += irowstride2;
      dst += orowstride;
    }
    weed_free(in_params);
    weed_free(bc);
    break;
  }

  weed_free(in_channels);
  return WEED_SUCCESS;
}


static weed_error_t tsplit_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0, inst, timestamp);
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), weed_channel_template_init("in channel 1", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  weed_plant_t *in_params1[] = {weed_float_init("start", "_Start", 0.666667, 0., 1.), weed_radio_init("sym", "Make s_ymmetrical", 1, 1), weed_radio_init("usend", "Use _end value", 0, 1), weed_float_init("end", "_End", 0.333333, 0., 1.), weed_switch_init("vert", "Split _horizontally", WEED_FALSE), weed_float_init("borderw", "Border _width", 0., 0., 0.5), weed_colRGBi_init("borderc", "Border _colour", 0, 0, 0), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("triple split", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD, NULL, tsplit_process, NULL,
                               in_chantmpls, out_chantmpls, in_params1, NULL);

  weed_plant_t *gui = weed_filter_class_get_gui(filter_class);
  char *rfx_strings[] = {"layout|p0|", "layout|p1|", "layout|p2|p3|", "layout|p4|", "layout|hseparator|"};

  weed_set_string_value(gui, "layout_scheme", "RFX");
  weed_set_string_value(gui, "rfx_delim", "|");
  weed_set_string_array(gui, "rfx_strings", 5, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;
