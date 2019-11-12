// ccorect.c
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

  register int i;

  sdata = weed_malloc(sizeof(_sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i = 0; i < 256; i++) {
    sdata->r[i] = sdata->g[i] = sdata->b[i] = 0;
  }
  sdata->ored = sdata->ogreen = sdata->oblue = 0.;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t ccorrect_deinit(weed_plant_t *inst) {
  _sdata *sdata;
  int error;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) weed_free(sdata);
  return WEED_SUCCESS;
}


static weed_error_t ccorrect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  _sdata *sdata;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, &error), *out_channel = weed_get_plantptr_value(inst,
                             WEED_LEAF_OUT_CHANNELS,
                             &error);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, &error);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, &error);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, &error) * 3;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, &error);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, &error);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, &error);
  int psize = 4;

  unsigned char *end = src + height * irowstride;
  int inplace = (dst == src);
  int offs = 0;
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, &error);
  weed_plant_t **params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, &error);

  double red = weed_get_double_value(params[0], WEED_LEAF_VALUE, &error);
  double green = weed_get_double_value(params[1], WEED_LEAF_VALUE, &error);
  double blue = weed_get_double_value(params[2], WEED_LEAF_VALUE, &error);

  register int i;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);

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
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, &error);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, &error);

    src += offset * irowstride;
    dst += offset * orowstride;
    end = src + dheight * irowstride;
  }

  if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) psize = 3;
  if (palette == WEED_PALETTE_ARGB32) offs = 1;

  for (; src < end; src += irowstride) {
    for (i = 0; i < width; i += psize) {
      if (palette == WEED_PALETTE_BGR24 || palette == WEED_PALETTE_BGRA32) {
        dst[i] = sdata->b[src[i]];
        dst[i + 1] = sdata->g[src[i + 1]];
        dst[i + 2] = sdata->r[src[i + 2]];
        if (!inplace && palette == WEED_PALETTE_BGRA32) dst[i + 3] = src[i + 3];
      } else {
        if (!inplace && palette == WEED_PALETTE_ARGB32) dst[i] = src[i];
        dst[i + offs] = sdata->r[src[i + offs]];
        dst[i + 1 + offs] = sdata->g[src[i + 1 + offs]];
        dst[i + 2 + offs] = sdata->b[src[i + 2 + offs]];
        if (!inplace && palette == WEED_PALETTE_RGBA32) dst[i + 3] = src[i + 3];
      }
    }
    dst += orowstride;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};

  weed_plant_t *in_params[] = {weed_float_init("red", "_Red factor", 1.0, 0.0, 2.0), weed_float_init("green", "_Green factor", 1.0, 0.0, 2.0), weed_float_init("blue", "_Blue factor", 1.0, 0.0, 2.0), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("colour correction", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, &ccorrect_init,
                               &ccorrect_process, &ccorrect_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

