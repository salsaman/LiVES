// ccorect.c
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

#include "weed-plugin-utils.c"

///////////////////////////////////////////////////////////////////

typedef struct _sdata {
  double ored;
  double ogreen;
  double oblue;

  unsigned char r[256];
  unsigned char g[256];
  unsigned char b[256];
} _sdata;

/////////////////////////////////////////////////////////////

static void make_table(unsigned char *tab, double val) {
  unsigned int ival;
  register int i;
  for (i = 0; i < 256; i++) {
    ival = (val * i + .5);
    tab[i] = ival > 255 ? (unsigned char)255 : (unsigned char)ival;
  }
}


static weed_error_t ccorrect_init(weed_plant_t *inst) {
  _sdata *sdata;

  sdata = weed_malloc(sizeof(_sdata));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
  for (int i = 0; i < 256; i++) {
    sdata->r[i] = sdata->g[i] = sdata->b[i] = 0;
  }
  sdata->ored = sdata->ogreen = sdata->oblue = 0.;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t ccorrect_deinit(weed_plant_t *inst) {
  _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) weed_free(sdata);
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t ccorrect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);
  weed_plant_t **params = weed_get_in_params(inst, NULL);

  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  int palette = weed_channel_get_palette(in_channel);
  int psize = pixel_size(palette);

  int inplace = (dst == src);
  int offs = 0;

  double red = weed_param_get_value_double(params[0]);
  double green = weed_param_get_value_double(params[1]);
  double blue = weed_param_get_value_double(params[2]);

  weed_free(params);

  if (red != sdata->ored) {
    make_table(sdata->r, red);
    sdata->ored = red;
  }

  if (green != sdata->ogreen) {
    make_table(sdata->g, green);
    sdata->ogreen = green;
  }

  if (blue != sdata->oblue) {
    make_table(sdata->b, blue);
    sdata->oblue = blue;
  }

  // new threading arch
  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_channel);
    src += offset * irowstride;
    dst += offset * orowstride;
  }

  offs = rgb_offset(palette);
  width *= psize;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i += psize) {
      if (palette == WEED_PALETTE_BGR24 || palette == WEED_PALETTE_BGRA32) {
        dst[j * orowstride + i] = sdata->b[src[j * irowstride + i]];
        dst[j * orowstride + i + 1] = sdata->g[src[j * irowstride + i + 1]];
        dst[j * orowstride + i + 2] = sdata->r[src[j * irowstride + i + 2]];
        if (!inplace && palette == WEED_PALETTE_BGRA32) dst[j * orowstride + i + 3] = src[j * irowstride + i + 3];
      } else {
        if (!inplace && palette == WEED_PALETTE_ARGB32) dst[i] = src[j * irowstride + i];
        dst[j * orowstride + i + offs] = sdata->r[src[j * irowstride + i + offs]];
        dst[j * orowstride + i + 1 + offs] = sdata->g[src[j * irowstride + i + 1 + offs]];
        dst[j * orowstride + i + 2 + offs] = sdata->b[src[j * irowstride + i + 2 + offs]];
        if (!inplace && palette == WEED_PALETTE_RGBA32) dst[j * orowstride + i + 3] = src[j * irowstride + i + 3];
      }
    }
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0",
                                   WEED_CHANNEL_CAN_DO_INPLACE), NULL
                                  };

  weed_plant_t *in_params[] = {weed_float_init("red", "_Red factor", 1.0, 0.0, 2.0),
                               weed_float_init("green", "_Green factor", 1.0, 0.0, 2.0),
                               weed_float_init("blue", "_Blue factor", 1.0, 0.0, 2.0), NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("colour correction", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD,
                               palette_list,
                               ccorrect_init, ccorrect_process, ccorrect_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

