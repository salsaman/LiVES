// RGBdelay.c
// weed plugin
// (c) G. Finch (salsaman) 2011 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include <stdio.h>

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include <stdlib.h>

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

typedef struct {
  int tcache;
  int ccache;
  unsigned char **cache;
  int *is_bgr;
  unsigned char lut[3][256];
} _sdata;


static void make_lut(unsigned char *lut, double val, int min) {
  int i, mina = min, minb = 0;
  double rnd = 0.5, rval;

  if (min < 0) {
    mina = 0;
    minb = -min;
    rnd += (double)minb;
  }

  for (i = 0; i < 256; i++) {
    rval = (double)(i - mina) * val + rnd;
    if (rval < 0.) rval = 0.;
    if (rval > 255.) rval = 255.;
    lut[i] = (unsigned char)rval;
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
    return WEED_SUCCESS;
  }

  sdata->cache = (unsigned char **)weed_realloc(sdata->cache, newsize * sizeof(unsigned char *));
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
  return WEED_SUCCESS;
}


static weed_error_t RGBd_init(weed_plant_t *inst) {
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

  return WEED_SUCCESS;
}

#define RED_ON(i) (i * 4 + 1)
#define GREEN_ON(i) (i * 4 + 2)
#define BLUE_ON(i) (i * 4 + 3)
#define STRENGTH(i) (i * 4 + 4)

static weed_error_t RGBd_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error), *out_channel = weed_get_plantptr_value(inst,
                             "out_channels",
                             &error);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", &error);

  size_t x = 0;

  double tstr_red = 0., tstr_green = 0., tstr_blue = 0., cstr_red, cstr_green, cstr_blue, cstr;
  double yscale = 1., uvscale = 1.;

  int width = weed_get_int_value(in_channel, "width", &error) * 3;
  int height = weed_get_int_value(in_channel, "height", &error);
  int irowstride = weed_get_int_value(in_channel, "rowstrides", &error);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", &error);
  int palette = weed_get_int_value(in_channel, "current_palette", &error);

  unsigned char *src = weed_get_voidptr_value(in_channel, "pixel_data", &error), *osrc = src;
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", &error), *odst = dst;
  unsigned char *end = src + height * irowstride;
  unsigned char *tmpcache = NULL;

  int maxcache = weed_get_int_value(in_params[0], "value", &error);
  int inplace = (src == dst);

  int cross, red = 0, blue = 2;
  int b1, b2, b3, bx;
  int maxneeded = 0;
  int is_bgr = 0, is_yuv = 0, yuvmin = 0, uvmin = 0;
  int is_easing = 0;

  register int i, j, k;
  if (weed_get_int_value(inst, WEED_LEAF_EASE_OUT, NULL) > 0) {
    is_easing = 1;
    fprintf(stderr, "easing %d\n", sdata->ccache);
  }

  if (maxcache < 0) maxcache = 0;
  else if (maxcache > 50) maxcache = 50;

  if (!is_easing) {
    for (i = 1; i < maxcache; i++) {
      if (weed_get_boolean_value(in_params[RED_ON(i)], "value", &error) == WEED_TRUE ||
          weed_get_boolean_value(in_params[GREEN_ON(i)], "value", &error) == WEED_TRUE ||
          weed_get_boolean_value(in_params[BLUE_ON(i)], "value", &error) == WEED_TRUE) {
        maxneeded = i + 1;
      }
    }

    if (maxneeded != sdata->tcache) {
      int ret = realloc_cache(sdata, maxneeded, width, height); // sets sdata->tcache
      if (ret != WEED_SUCCESS) {
        weed_free(in_params);
        return ret;
      }
      if (sdata->ccache > sdata->tcache) sdata->ccache = sdata->tcache;
    }
  }

  if (sdata->tcache > 1) {
    tmpcache = sdata->cache[sdata->tcache - 1];
  }

  for (i = sdata->tcache - 1; i >= 0; i--) {
    if (i > 0) {
      // rotate the cache pointers
      sdata->cache[i] = sdata->cache[i - 1];
      sdata->is_bgr[i] = sdata->is_bgr[i - 1];
    }

    // normalise the blend strength for each colour channel, so the total doesnt exceed 1.0
    // tstr_* hold the overall totals
    if (weed_get_boolean_value(in_params[RED_ON(i)], "value", &error) == WEED_TRUE) {
      tstr_red += weed_get_double_value(in_params[STRENGTH(i)], "value", &error);
    }

    if (weed_get_boolean_value(in_params[GREEN_ON(i)], "value", &error) == WEED_TRUE)
      tstr_green += weed_get_double_value(in_params[STRENGTH(i)], "value", &error);

    if (weed_get_boolean_value(in_params[BLUE_ON(i)], "value", &error) == WEED_TRUE) {
      tstr_blue += weed_get_double_value(in_params[STRENGTH(i)], "value", &error);
    }
  }

  if (palette == WEED_PALETTE_BGR24) {
    // red/blue values swapped
    is_bgr = sdata->is_bgr[0] = 1;
  } else is_bgr = sdata->is_bgr[0] = 0;

  if (sdata->tcache > 0) {
    // copy current frame -> sdata->cache[0]
    for (; src < end; src += irowstride) {
      weed_memcpy(tmpcache + x, src, width);
      x += width;
    }
    sdata->cache[0] = tmpcache;
  }

  // < 1.0 is OK
  if (tstr_red < 1.) tstr_red = 1.;
  if (tstr_green < 1.) tstr_green = 1.;
  if (tstr_blue < 1.) tstr_blue = 1.;

  if (palette == WEED_PALETTE_YUV888) {
    is_yuv = 1;
    if (weed_get_int_value(in_channel, "YUV_clamping", &error) == WEED_YUV_CLAMPING_CLAMPED) {
      // unclamp the values in the lut
      yuvmin = 16;
      uvmin = 16;
      yscale = 255. / 235.;
      uvscale = 255. / 240.;
    }
  }

  if (sdata->tcache == 0) {
    b1 = (weed_get_boolean_value(in_params[RED_ON(0)], "value", &error) == WEED_TRUE);
    b2 = (weed_get_boolean_value(in_params[GREEN_ON(0)], "value", &error) == WEED_TRUE);
    b3 = (weed_get_boolean_value(in_params[BLUE_ON(0)], "value", &error) == WEED_TRUE);

    cstr = weed_get_double_value(in_params[4], "value", &error);
    cstr_red = cstr / tstr_red;
    cstr_green = cstr / tstr_green;
    cstr_blue = cstr / tstr_blue;

    if (is_bgr) {
      // swap r / b (on-off and lut)
      bx = b1;
      b1 = b3;
      b3 = bx;
      red = 2;
      blue = 0;
    }

    make_lut(sdata->lut[red], cstr_red * yscale, yuvmin);
    make_lut(sdata->lut[1], cstr_green * uvscale, yuvmin);
    make_lut(sdata->lut[blue], cstr_blue * uvscale, yuvmin);

    height--;
    src = osrc;
    end = odst + height * orowstride;

    for (dst = odst; dst < end; dst += orowstride) {
      for (i = 0; i < width; i += 3) {
        if (b1) dst[i] = sdata->lut[0][src[i]];
        else if (inplace) dst[i] = yuvmin;
        if (b2) dst[i + 1] = sdata->lut[1][src[i + 1]];
        else if (inplace) dst[i + 1] = uvmin;
        if (b3) dst[i + 2] = sdata->lut[2][src[i + 2]];
        else if (inplace) dst[i + 2] = uvmin;
      }
      src += irowstride;
    }
  } else {
    for (j = 0; j < sdata->tcache; j++) {
      // maybe overlay something from j frames ago
      if (j <= sdata->ccache) k = j;
      else k = sdata->ccache;

      b1 = (weed_get_boolean_value(in_params[RED_ON(j)], "value", &error) == WEED_TRUE);
      b2 = (weed_get_boolean_value(in_params[GREEN_ON(j)], "value", &error) == WEED_TRUE);
      b3 = (weed_get_boolean_value(in_params[BLUE_ON(j)], "value", &error) == WEED_TRUE);

      if (!b1 && !b2 && !b3 && j > 0) continue;

      if ((!is_bgr && sdata->is_bgr[j]) || (is_bgr && !sdata->is_bgr[j]))
        cross = 2;
      else
        cross = 0;

      cstr = weed_get_double_value(in_params[STRENGTH(j)], "value", &error);
      cstr_red = cstr / tstr_red;
      cstr_green = cstr / tstr_green;
      cstr_blue = cstr / tstr_blue;

      if (sdata->is_bgr[j]) {
        // swap r / b (on-off and lut)
        bx = b1;
        b1 = b3;
        b3 = bx;
        red = 2;
        blue = 0;
      } else {
        red = 0;
        blue = 2;
      }

      make_lut(sdata->lut[red], cstr_red * yscale, yuvmin);
      make_lut(sdata->lut[1], cstr_green * uvscale, yuvmin);
      make_lut(sdata->lut[blue], cstr_blue * uvscale, yuvmin);

      x = 0;

      for (dst = odst; dst < end; dst += orowstride) {
        for (i = 0; i < width; i += 3) {
          if (j == 0) {
            weed_memset(&dst[i], 0, 3);
          }
          if (b1) dst[i] += sdata->lut[0][sdata->cache[k][x + i + cross]];
          if (b2) dst[i + 1] += sdata->lut[1][sdata->cache[k][x + i + 1]];
          if (b3) dst[i + 2] += sdata->lut[2][sdata->cache[k][x + i + 2 - cross]];
        }
        x += width;
      }
    }
  }

  if (!is_easing) {
    if (sdata->ccache < sdata->tcache) {
      sdata->ccache++;
    }
  } else {
    if (sdata->ccache > 0) sdata->ccache--;
  }

  if (is_yuv && yuvmin == 16) {
    // reclamp the values
    make_lut(sdata->lut[0], 1. / yscale, -yuvmin);
    make_lut(sdata->lut[1], 1. / uvscale, -yuvmin);
    for (dst = odst; dst < end; dst += orowstride) {
      for (i = 0; i < width; i += 3) {
        dst[i] = sdata->lut[0][dst[i]];
        dst[i + 1] = sdata->lut[1][dst[i + 1]];
        dst[i + 2] = sdata->lut[1][dst[i + 2]];
      }
    }
  }
  weed_set_int_value(inst, WEED_LEAF_PLUGIN_EASING, sdata->ccache);
  weed_free(in_params);
  return WEED_SUCCESS;
}


static weed_error_t RGBd_deinit(weed_plant_t *inst) {
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
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone;
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

  filter_class = weed_filter_class_init("RGBdelay", "salsaman", 1, WEED_FILTER_HINT_LINEAR_GAMMA, &RGBd_init, &RGBd_process, &RGBd_deinit,
                                        in_chantmpls, out_chantmpls,
                                        in_params,
                                        NULL);

  gui = weed_filter_class_get_gui(filter_class);
  rfx_strings[0] = "layout|p0|";
  rfx_strings[1] = "layout|hseparator|";
  rfx_strings[2] = "layout|\"R\"|\"G\"|\"B\"|fill|fill|\"Blend Strength\"|fill|";

  for (i = 3; i < 54; i++) {
    rfx_strings[i] = weed_malloc(1024);
    if (rfx_strings[i] == NULL) return NULL;
    snprintf(rfx_strings[i], 1024, "layout|p%d|p%d|p%d|p%d|", (i - 3) * 4 + 1, (i - 3) * 4 + 2, (i - 3) * 4 + 3, (i - 2) * 4);
  }

  weed_set_string_value(gui, "layout_scheme", "RFX");
  weed_set_string_value(gui, "rfx_delim", "|");
  weed_set_string_array(gui, "rfx_strings", 54, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  rfx_strings[2] = "layout|\"Y\"|\"U\"|\"V\"|fill|fill|\"Blend Strength\"|fill|";

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
WEED_SETUP_END
