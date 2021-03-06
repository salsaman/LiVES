/////////////////////////////////////////////////////////////////////////////
// Weed targeted_zoom plugin, version
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by salsaman
/////////////////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

/////////////////////////////////////////////////////////////////////////////
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

#include "weed-plugin-utils.c"
//////////////////////////////////////////////////////////////////

#include <math.h>
enum {
  P_scale,
  P_xoffs,
  P_yoffs,
};

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////////////////////

static weed_error_t targeted_zoom_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);

  double scale = weed_param_get_value_double(in_params[P_scale]);
  double xoffs = weed_param_get_value_double(in_params[P_xoffs]);
  double yoffs = weed_param_get_value_double(in_params[P_yoffs]);
  weed_free(in_params);

  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_chan);
    src += offset * irow;
    dst += offset * orow;
  }

  if (1) {
    float offsx = (float)xoffs, offsy = (float)yoffs, fscale = (float)scale, fscalex;
    int dx, dy, offset, widthx;

    if (fscale < 1.) fscale = 1.;
    if (offsx < 0.) offsx = 0.;
    if (offsx + 1. / fscale > 1.) offsx = 1. - 1. / fscale;
    if (offsy < 0.) offsy = 0.;
    if (offsy + 1. / fscale > 1.) offsy = 1. - 1. / fscale;

    offsx *= width;
    offsy *= iheight;

    widthx = width * psize;
    fscalex = fscale * psize;
    offset = weed_channel_get_offset(out_chan);
    height += offset;

    for (int y = offset; y < height; y++) {
      dy = (int)(offsy + (float)y / fscale + .5);
      for (int x = 0; x < widthx; x += psize) {
        dx = (int)(offsx + (float)x / fscalex + .5);
        weed_memcpy(&dst[(y - offset) * orow + x], &src[irow * dy + dx * psize], psize);
      }
    }
  }

  return WEED_SUCCESS;
}


static weed_error_t targeted_zoom_disp_log(weed_plant_t *inst, weed_plant_t *param, int inverse) {
  if (!weed_plant_has_leaf(param, WEED_LEAF_VALUE)) return WEED_ERROR_NOSUCH_LEAF;
  else {
    double val = weed_param_get_value_double(param);
    if (inverse == WEED_FALSE) {
      if (val <= 0.) return WEED_ERROR_NOT_READY;
      val = log(val) + 1.;
    } else {
      val = exp(val - 1.);
    }
    return weed_set_double_value(weed_param_get_gui(param), WEED_LEAF_DISPLAY_VALUE, val);
  }
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  int palette_list[] = ALL_PACKED_PALETTES;
  weed_plant_t *in_chantmpls[] = {
    weed_channel_template_init("in_channel0", 0),
    NULL
  };
  weed_plant_t *out_chantmpls[] = {
    weed_channel_template_init("out_channel0", WEED_CHANNEL_CAN_DO_INPLACE),
    NULL
  };
  weed_plant_t *in_paramtmpls[] = {
    weed_float_init("scale", "_Scale", 1., 1., exp(3)),
    weed_float_init("xoffs", "_X center", .5, 0., 1.),
    weed_float_init("yoffs", "_Y center", .5, 0., 1.),
    NULL
  };
  weed_plant_t *gui;
  char *rfx_strings[] = {
    "special|framedraw|scaledpoint|1|2|0|",
    NULL
  };
  weed_plant_t *pgui;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  verbosity = weed_get_host_verbosity(host_info);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_scale]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);
  weed_set_funcptr_value(pgui, WEED_LEAF_DISPLAY_VALUE_FUNC, (weed_funcptr_t)targeted_zoom_disp_log);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_xoffs]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_yoffs]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  filter_class = weed_filter_class_init("targeted zoom", "salsaman", 1, filter_flags, palette_list,
                                        NULL, targeted_zoom_process, NULL, in_chantmpls, out_chantmpls, in_paramtmpls, NULL);

  gui = weed_filter_get_gui(filter_class);
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 1, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

