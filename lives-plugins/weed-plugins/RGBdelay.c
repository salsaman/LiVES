// RGBdelay.c
// weed plugin
// (c) G. Finch (salsaman) 2011 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include <stdio.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

#include <stdlib.h>

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

typedef struct {
  int tcache;
  int ccache;
  unsigned char **cache;
  int *is_bgr;
  unsigned char lut[3][256];
} _sdata;


static void make_lut(unsigned char *lut, double val) {
  int i;
  for (i = 0; i < 256; i++) {
    lut[i] = (unsigned char)((double)i * val + .5);
  }
}


static int realloc_cache(_sdata *sdata, int newsize, int width, int height) {
  // create frame cache
  int i;
  int oldsize = sdata->tcache;

  for (i = oldsize; i > newsize; i--) {
    if (sdata->cache[i - 1] != NULL) {
      weed_free(sdata->cache[i - 1]);
    }
  }

  sdata->tcache = 0;

  if (newsize == 0) {
    weed_free(sdata->cache);
    sdata->cache = NULL;
    return WEED_NO_ERROR;
  }

  sdata->cache = (unsigned char **)realloc(sdata->cache, newsize * sizeof(unsigned char *));
  if (sdata->cache == NULL) {
    weed_free(sdata->is_bgr);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = oldsize; i < newsize; i++) {
    sdata->cache[i] = weed_malloc(width * height * 3);
    if (sdata->cache[i] == NULL) {
      for (--i; i >= 0; i--) weed_free(sdata->cache[i]);
      weed_free(sdata->cache);
      weed_free(sdata->is_bgr);
      weed_free(sdata);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }
  }
  sdata->tcache = newsize;
  return WEED_NO_ERROR;
}


static int RGBd_init(weed_plant_t *inst) {
  int error;
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", &error), *gui, *ptmpl;
  int maxcache = weed_get_int_value(in_params[0], "value", &error);
  register int i;

  _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->ccache = sdata->tcache = 0;
  sdata->cache = NULL;
  sdata->is_bgr = weed_malloc(maxcache * sizeof(int));

  if (sdata->is_bgr == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = 0; i < maxcache; i++) {
    sdata->is_bgr[i] = 0;
  }

  maxcache *= 4;

  for (i = 0; i < 205; i++) {
    ptmpl = weed_get_plantptr_value(in_params[i], "template", &error);
    gui = weed_parameter_template_get_gui(ptmpl);
    weed_set_boolean_value(gui, "hidden", i > maxcache ? WEED_TRUE : WEED_FALSE);
  }

  weed_free(in_params);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_NO_ERROR;
}


static int RGBd_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error), *out_channel = weed_get_plantptr_value(inst,
                             "out_channels",
                             &error);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", &error);
  unsigned char *src = weed_get_voidptr_value(in_channel, "pixel_data", &error), *osrc;
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", &error), *odst;
  int width = weed_get_int_value(in_channel, "width", &error) * 3;
  int height = weed_get_int_value(in_channel, "height", &error);
  int palette = weed_get_int_value(in_channel, "current_palette", &error);
  int irowstride = weed_get_int_value(in_channel, "rowstrides", &error);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", &error);
  unsigned char *end = src + height * irowstride;
  unsigned char *tmpcache = NULL;
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);
  register int i, j;
  double tstra = 0., tstrb = 0., tstrc = 0., cstr, cstra, cstrb, cstrc;
  int a = 1, b = 2, c = 3, crossed;
  size_t x = 0;
  int inplace = (src == dst);
  int b1, b2, b3;
  int maxcache = weed_get_int_value(in_params[0], "value", &error);
  int maxneeded = 0;

  if (maxcache < 0) maxcache = 0;
  if (maxcache > 50) maxcache = 50;

  for (i = 1; i < maxcache; i++) {
    if (weed_get_boolean_value(in_params[i * 4 + 1], "value", &error) == WEED_TRUE ||
        weed_get_boolean_value(in_params[i * 4 + 2], "value", &error) == WEED_TRUE ||
        weed_get_boolean_value(in_params[i * 4 + 3], "value", &error) == WEED_TRUE) {
      maxneeded = i + 1;
    }
  }

  if (maxneeded != sdata->tcache) {
    int ret = realloc_cache(sdata, maxneeded, width, height);
    if (ret != WEED_NO_ERROR) return ret;
    if (sdata->ccache > sdata->tcache) sdata->ccache = sdata->tcache;
  }

  if (sdata->ccache > 0) {
    tmpcache = sdata->cache[sdata->ccache - 1];
  }

  for (i = sdata->tcache; i >= 0; i--) {
    if (i > 0 && i < sdata->ccache) {
      sdata->cache[i] = sdata->cache[i - 1];
      sdata->is_bgr[i] = sdata->is_bgr[i - 1];
    }

    // normalise the blend strength for each colour channel
    if (weed_get_boolean_value(in_params[i * 4 + 1], "value", &error) == WEED_TRUE) {
      if (sdata->is_bgr[i])
        tstrc += weed_get_double_value(in_params[(i + 1) * 4], "value", &error);
      else tstra += weed_get_double_value(in_params[(i + 1) * 4], "value", &error);
    }

    if (weed_get_boolean_value(in_params[i * 4 + 2], "value", &error) == WEED_TRUE)
      tstrb += weed_get_double_value(in_params[(i + 1) * 4], "value", &error);

    if (weed_get_boolean_value(in_params[i * 4 + 3], "value", &error) == WEED_TRUE) {
      if (sdata->is_bgr[i])
        tstra += weed_get_double_value(in_params[(i + 1) * 4], "value", &error);
      else tstrc += weed_get_double_value(in_params[(i + 1) * 4], "value", &error);
    }
  }

  if (sdata->ccache > 0) {
    sdata->cache[0] = tmpcache;
  }

  if (palette == WEED_PALETTE_BGR24) {
    // red/blue values swapped
    sdata->is_bgr[0] = 1;
    a = 3;
    c = 1;
  } else sdata->is_bgr[0] = 0;

  if (sdata->ccache < sdata->tcache) {
    // do nothing until we have cached enough frames
    for (; src < end; src += irowstride) {
      weed_memcpy(sdata->cache[0] + x, src, width);
      if (!inplace) weed_memcpy(dst, src, width);
      dst += orowstride;
      x += width;
    }
    sdata->ccache++;
    return WEED_NO_ERROR;
  }

  osrc = src;
  odst = dst;

  a = 1;
  b = 2;
  c = 3;

  if (palette == WEED_PALETTE_BGR24) {
    a = 3;
    c = 1;
  }

  b1 = weed_get_boolean_value(in_params[a], "value", &error);
  b2 = weed_get_boolean_value(in_params[b], "value", &error);
  b3 = weed_get_boolean_value(in_params[c], "value", &error);

  if (b1 == WEED_TRUE || b2 == WEED_TRUE || b3 == WEED_TRUE || inplace) {
    if (b1 == WEED_TRUE)
      tstra += weed_get_double_value(in_params[4], "value", &error);
    if (b2 == WEED_TRUE)
      tstrb += weed_get_double_value(in_params[4], "value", &error);
    if (b3 == WEED_TRUE)
      tstrc += weed_get_double_value(in_params[4], "value", &error);

    cstr = weed_get_double_value(in_params[4], "value", &error);
    cstra = cstr / tstra;
    cstrb = cstr / tstrb;
    cstrc = cstr / tstrc;

    make_lut(sdata->lut[0], cstra);
    make_lut(sdata->lut[1], cstrb);
    make_lut(sdata->lut[2], cstrc);

    for (; src < end; src += irowstride) {
      if (sdata->tcache > 0) weed_memcpy(sdata->cache[0] + x, src, width);

      for (i = 0; i < width; i += 3) {
        if (b1 == WEED_TRUE) dst[i] = sdata->lut[0][src[i]];
        else if (inplace) dst[i] = 0;
        if (b2 == WEED_TRUE) dst[i + 1] = sdata->lut[1][src[i + 1]];
        else if (inplace) dst[i + 1] = 0;
        if (b3 == WEED_TRUE) dst[i + 2] = sdata->lut[2][src[i + 2]];
        else if (inplace) dst[i + 2] = 0;
      }
      x += width;
      dst += orowstride;
    }
    src = osrc;
    dst = odst;
    x = 0;
  }

  for (j = 1; j < sdata->ccache; j++) {
    // maybe overlay something from j frames ago

    a += 4;
    b += 4;
    c += 4;

    b1 = weed_get_boolean_value(in_params[a], "value", &error);
    b2 = weed_get_boolean_value(in_params[b], "value", &error);
    b3 = weed_get_boolean_value(in_params[c], "value", &error);

    if (b1 == WEED_FALSE && b2 == WEED_FALSE && b3 == WEED_FALSE)
      continue;

    if ((palette == WEED_PALETTE_RGB24 && sdata->is_bgr[j]) || (palette == WEED_PALETTE_BGR24 && !sdata->is_bgr[j]))
      crossed = 1;
    else crossed = 0;

    cstr = weed_get_double_value(in_params[(j + 1) * 4], "value", &error);
    cstrb = cstr / tstrb;

    if (!sdata->is_bgr[j]) {
      cstra = cstr / tstra;
      cstrc = cstr / tstrc;
    } else {
      cstrc = cstr / tstra;
      cstra = cstr / tstrc;
    }

    make_lut(sdata->lut[0], cstra);
    make_lut(sdata->lut[1], cstrb);
    make_lut(sdata->lut[2], cstrc);

    for (; src < end; src += irowstride) {
      for (i = 0; i < width; i += 3) {
        if (b1 == WEED_TRUE) dst[i] += sdata->lut[0][sdata->cache[j][x + (crossed ? (i + 2) : i)]];
        if (b2 == WEED_TRUE) dst[i + 1] += sdata->lut[1][sdata->cache[j][x + i + 1]];
        if (b3 == WEED_TRUE) dst[i + 2] += sdata->lut[2][sdata->cache[j][x + (crossed ? i : (i + 2))]];
      }
      x += width;
      dst += orowstride;
    }
    src = osrc;
    dst = odst;
    x = 0;
  }
  if (palette == WEED_PALETTE_YUV888) {
    // YUV black
    int clamping = weed_get_int_value(in_channel, "YUV_clamping", &error);
    for (; src < end; src += irowstride) {
      for (i = 0; i < width; i += 3) {
        if (dst[i] == 0 && clamping == WEED_YUV_CLAMPING_CLAMPED) dst[i] = 16;
        if (dst[i + 1] == 0) dst[i + 1] = 128;
        if (dst[i + 2] == 0) dst[i + 2] = 128;
      }
      dst += orowstride;
    }
  }
  weed_free(in_params);
  return WEED_NO_ERROR;
}


static int RGBd_deinit(weed_plant_t *inst) {
  int error, i;
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) {
    if (sdata->cache != NULL) {
      for (i = 0; i < sdata->tcache; i++) {
        weed_free(sdata->cache[i]);
      }
    }
    weed_free(sdata->cache);

    if (sdata->is_bgr != NULL) weed_free(sdata->is_bgr);

    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, num_versions, api_versions);
  weed_plant_t **clone;

  if (plugin_info != NULL) {
    int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
    int palette_list2[] = {WEED_PALETTE_YUV888, WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE, palette_list), NULL};
    weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list), NULL};
    weed_plant_t *in_chantmpls2[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE, palette_list2), NULL};
    weed_plant_t *out_chantmpls2[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE, palette_list2), NULL};
    weed_plant_t *filter_class, *gui;

    weed_plant_t *in_params[206];
    char *rfx_strings[54];
    char label[256];
    int i, j;

    in_params[0] = weed_integer_init("fcsize", "Frame _Cache Size (max)", 20, 0, 50);
    weed_set_int_value(in_params[0], "flags", WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

    for (i = 1; i < 205; i += 4) {
      for (j = 0; j < 3; j++) {
        if (j == 2) snprintf(label, 256, "        Frame -%-2d       ", (i - 1) / 4);
        else weed_memset(label, 0, 1);
        in_params[i + j] = weed_switch_init("", label, (i + j == 1 || i + j == 18 || i + j == 35) ? WEED_TRUE : WEED_FALSE);
      }
      in_params[i + j] = weed_float_init("", "", 1., 0., 1.);

      if (i >= 80) {
        gui = weed_parameter_template_get_gui(in_params[i]);
        weed_set_boolean_value(gui, "hidden", WEED_TRUE);
        gui = weed_parameter_template_get_gui(in_params[i + 1]);
        weed_set_boolean_value(gui, "hidden", WEED_TRUE);
        gui = weed_parameter_template_get_gui(in_params[i + 2]);
        weed_set_boolean_value(gui, "hidden", WEED_TRUE);
        gui = weed_parameter_template_get_gui(in_params[i + 3]);
        weed_set_boolean_value(gui, "hidden", WEED_TRUE);
      }
    }

    in_params[205] = NULL;

    filter_class = weed_filter_class_init("RGBdelay", "salsaman", 1, 0, &RGBd_init, &RGBd_process, &RGBd_deinit, in_chantmpls, out_chantmpls,
                                          in_params,
                                          NULL);

    gui = weed_filter_class_get_gui(filter_class);
    rfx_strings[0] = "layout|p0";
    rfx_strings[1] = "layout|hseparator|";
    rfx_strings[2] = "layout|\"  R\"|fill|\"         G \"|fill|\"         B \"|fill|\"Blend Strength\"|fill|";

    for (i = 3; i < 54; i++) {
      rfx_strings[i] = weed_malloc(1024);
      snprintf(rfx_strings[i], 1024, "layout|p%d|p%d|p%d|p%d|", (i - 3) * 4 + 1, (i - 3) * 4 + 2, (i - 3) * 4 + 3, (i - 2) * 4);
    }

    weed_set_string_value(gui, "layout_scheme", "RFX");
    weed_set_string_value(gui, "rfx_delim", "|");
    weed_set_string_array(gui, "rfx_strings", 54, rfx_strings);

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    rfx_strings[2] = "layout|\"  Y\"|fill|\"         U \"|fill|\"         V \"|fill|\"Blend Strength\"|fill|";

    filter_class = weed_filter_class_init("YUVdelay", "salsaman", 1, 0, &RGBd_init, &RGBd_process, &RGBd_deinit, in_chantmpls2, out_chantmpls2,
                                          (clone = weed_clone_plants(in_params)), NULL);
    weed_free(clone);

    gui = weed_filter_class_get_gui(filter_class);
    weed_set_string_value(gui, "layout_scheme", "RFX");
    weed_set_string_value(gui, "rfx_delim", "|");
    weed_set_string_array(gui, "rfx_strings", 54, rfx_strings);

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    for (i = 3; i < 54; i++) weed_free(rfx_strings[i]);

    weed_set_int_value(plugin_info, "version", package_version);
  }
  return plugin_info;
}

