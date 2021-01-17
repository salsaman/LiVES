// targetted_zoom.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

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

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

static weed_error_t tzoom_process(weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);

  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);

  int pal = weed_channel_get_palette(in_channel);

  int width = weed_channel_get_width(in_channel), widthx;
  int height = weed_channel_get_height(in_channel);

  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);

  weed_plant_t **in_params;

  float offsx, offsy, scale, scalex;

  int dx, dy;

  int offset = 0, dheight = height;
  int psize = pixel_size(pal);

  in_params = weed_get_in_params(inst, NULL);

  scale = weed_param_get_value_double(in_params[0]);
  if (scale < 1.) scale = 1.;

  offsx = weed_param_get_value_double(in_params[1]) - 0.5 / scale;
  offsy = weed_param_get_value_double(in_params[2]) - 0.5 / scale;
  weed_free(in_params);

  if (offsx < 0.) offsx = 0.;
  if (offsx + 1. / scale > 1.) offsx = 1. - 1. / scale;
  if (offsy < 0.) offsy = 0.;
  if (offsy + 1. / scale > 1.) offsy = 1. - 1. / scale;

  offsx *= width;
  offsy *= height;

  // new threading arch
  if (weed_is_threading(inst)) {
    offset = weed_channel_get_offset(out_channel);
    dheight = weed_channel_get_height(out_channel) + offset;
    dst += offset * orowstride;
  }

  widthx = width * psize;
  scalex = scale * psize;

  for (int y = offset; y < dheight; y++) {
    dy = (int)(offsy + (float)y / scale + .5);
    for (int x = 0; x < widthx; x += psize) {
      dx = (int)(offsx + (float)x / scalex + .5);
      weed_memcpy(&dst[(y - offset) * orowstride + x], &src[irowstride * dy + dx * psize], psize);
    }
  }
  return WEED_SUCCESS;
}

  //weed_memcpy(&dst[r + x], &src[irowstride * dy + dx * psize], psize);

static weed_error_t tzoom_disp(weed_plant_t *inst, weed_plant_t *param, int inverse) {
  if (!weed_plant_has_leaf(param, WEED_LEAF_VALUE)) return WEED_ERROR_NOSUCH_LEAF;
  else {
    double dval = weed_param_get_value_double(param);
    if (inverse == WEED_FALSE) {
      if (dval <= 0.) return WEED_ERROR_NOT_READY;
      dval = log(dval);
    }
    else dval = exp(dval);
    return weed_set_double_value(weed_param_get_gui(param), "display_value", dval);
  }
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};

  weed_plant_t *in_params[] = {weed_float_init("scale", "_Scale", 1., 1., exp(3)),
                               weed_float_init("xoffs", "_X center", 0.5, 0., 1.),
			       weed_float_init("yoffs", "_Y center", 0.5, 0., 1.), NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("targeted zoom", "salsaman", 1,
						      WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, tzoom_process, NULL,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plant_t *gui = weed_filter_get_gui(filter_class);

  // define RFX layout
  char *rfx_strings[] = {"special|framedraw|scaledpoint|1|2|0|"};

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 1, rfx_strings);

  gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_set_double_value(gui, WEED_LEAF_STEP_SIZE, 0.1);
  weed_set_funcptr_value(gui, "display_func", (weed_funcptr_t)tzoom_disp);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
