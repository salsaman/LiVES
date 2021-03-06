/////////////////////////////////////////////////////////////////////////////
// Weed revTV plugin, version
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by effecTV
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

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

enum {
  P_lspace,
  P_vscale,
};

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////////////////////

static weed_error_t revTV_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);

  int lspace = weed_param_get_value_int(in_params[P_lspace]);
  double vscale = weed_param_get_value_double(in_params[P_vscale]);
  weed_free(in_params);

  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_chan);
    src += offset * irow;
    dst += offset * orow;
  }

  if (1) {
    short val;
    int red = 0, green = 1, blue = 2, yoffset, yval;

    // split threads horizontally instead of vertically
    int offset = weed_channel_get_offset(out_chan);
    int xst = (offset * width / iheight) * psize;
    int xend = ((offset + height - 1) * width / iheight) * psize;
    if (xend > width * psize) xend = width * psize;

    height = iheight;

    src -= irow * offset;
    dst -= orow * offset;

    vscale /= 200.;
    if (pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_BGRA32) {
      red = 2; blue = 0;
    } else if (pal == WEED_PALETTE_ARGB32) {
      red = 1; green = 2; blue = 3;
    }

    for (int y = 0; y < height; y += lspace) {
      for (int x = xst; x < xend; x += psize) {
        if (pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888)
          val = src[irow * y + x] * 7;
        else val = (short)((src[irow * y + x + red] << 1) + (src[irow * y + x + green] << 2)
                             + src[irow * y + x + blue]);
        yval = y - val * vscale;
        if (yval >= 0) {
          weed_memcpy(&dst[orow * yval + x], &src[irow * y + x], psize);
        }
      }
    }
  }

  return WEED_SUCCESS;
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
    weed_channel_template_init("out_channel0", 0),
    NULL
  };
  weed_plant_t *in_paramtmpls[] = {
    weed_integer_init("lspace", "_Line spacing", 6, 1, 16),
    weed_float_init("vscale", "_Vertical scale factor", 2., 0., 4.),
    NULL
  };
  weed_plant_t *pgui;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  verbosity = weed_get_host_verbosity(host_info);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_vscale]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  filter_class = weed_filter_class_init("revTV", "effecTV", 1, filter_flags, palette_list,
                                        NULL, revTV_process, NULL, in_chantmpls, out_chantmpls, in_paramtmpls, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

