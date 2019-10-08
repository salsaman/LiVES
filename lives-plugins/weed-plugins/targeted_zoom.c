// targetted_zoom.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions = 2; // number of different weed api versions supported
static int api_versions[] = {131, 100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////


int tzoom_process(weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", &error);

  unsigned char *src = weed_get_voidptr_value(in_channel, "pixel_data", &error), *osrc = src;
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", &error);

  int pal = weed_get_int_value(in_channel, "current_palette", &error);

  int width = weed_get_int_value(in_channel, "width", &error), widthx;
  int height = weed_get_int_value(in_channel, "height", &error);

  int irowstride = weed_get_int_value(in_channel, "rowstrides", &error);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", &error);
  //int palette=weed_get_int_value(out_channel,"current_palette",&error);

  weed_plant_t **in_params;

  double offsx, offsy, scale, scalex;

  int dx, dy;

  int offset = 0, dheight = height;

  int psize = 4;

  register int x, y;

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) psize = 3;

  if (pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV) width >>= 1; // 2 pixels per macropixel

  in_params = weed_get_plantptr_array(inst, "in_parameters", &error);

  scale = weed_get_double_value(in_params[0], "value", &error);
  if (scale < 1.) scale = 1.;

  offsx = weed_get_double_value(in_params[1], "value", &error) - 0.5 / scale;
  offsy = weed_get_double_value(in_params[2], "value", &error) - 0.5 / scale;
  weed_free(in_params);

  if (offsx < 0.) offsx = 0.;
  if (offsx + 1. / scale > 1.) offsx = 1. - 1. / scale;
  if (offsy < 0.) offsy = 0.;
  if (offsy + 1. / scale > 1.) offsy = 1. - 1. / scale;

  offsx *= width;
  offsy *= height;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, "offset")) {
    offset = weed_get_int_value(out_channel, "offset", &error);
    dheight = weed_get_int_value(out_channel, "height", &error) + offset;
    dst += offset * orowstride;
  }

  widthx = width * psize;
  scalex = scale * psize;

  for (y = offset; y < dheight; y++) {
    dy = (int)(offsy + (double)y / scale + .5);
    if (dy > height - 1) dy = height - 1;
    src = osrc + dy * irowstride;

    for (x = 0; x < widthx; x += psize) {
      dx = (int)(offsx + (double)x / scalex + .5);
      //if (dx > width - 1) dx = width - 1;
      weed_memcpy(dst + x, src + dx * psize, psize);
    }
    dst += orowstride;
  }

  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, num_versions, api_versions);
  if (plugin_info != NULL) {
    // all planar palettes
    int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_RGBA32, WEED_PALETTE_ARGB32, WEED_PALETTE_BGRA32, WEED_PALETTE_UYVY, WEED_PALETTE_YUYV, WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
    weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};

    weed_plant_t *in_params[] = {weed_float_init("scale", "_Scale", 1., 1., 16.), weed_float_init("xoffs", "_X center", 0.5, 0., 1.), weed_float_init("yoffs", "_Y center", 0.5, 0., 1.), NULL};

    weed_plant_t *filter_class = weed_filter_class_init("targeted zoom", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, NULL, &tzoom_process, NULL,
                                 in_chantmpls, out_chantmpls, in_params, NULL);

    weed_plant_t *gui = weed_filter_class_get_gui(filter_class), *pgui;

    // define RFX layout
    char *rfx_strings[] = {"layout|p0|", "layout|p1|p2|", "special|framedraw|scaledpoint|1|2|"};

    weed_set_string_value(gui, "layout_scheme", "RFX");
    weed_set_string_value(gui, "rfx_delim", "|");
    weed_set_string_array(gui, "rfx_strings", 3, rfx_strings);

    pgui = weed_parameter_template_get_gui(in_params[0]);
    weed_set_double_value(pgui, "step_size", 0.1);

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    weed_set_int_value(plugin_info, "version", package_version);

  }

  return plugin_info;
}
