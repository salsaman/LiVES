// RGBdelay.c
// weed plugin
// (c) G. Finch (salsaman) 2011 - 2019
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

typedef struct {
  int tcache;
  int ccache;
  unsigned char **cache;
  int *is_bgr;
  unsigned char lut[3][256];
  int ease_every;
  int ease_counter;
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
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL), *gui;
  int maxcache = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
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

  for (i = 1; i < 205; i++) {
    gui = weed_param_get_gui(in_params[i]);
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, i > maxcache ? WEED_TRUE : WEED_FALSE);
  }

  weed_free(in_params);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  sdata->ease_every = 0;
  sdata->ease_counter = 0;

  return WEED_SUCCESS;
}

#define RED_ON(i) (i * 4 + 1)
#define GREEN_ON(i) (i * 4 + 2)
#define BLUE_ON(i) (i * 4 + 3)
#define STRENGTH(i) (i * 4 + 4)

static weed_error_t RGBd_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  size_t x = 0;

  double tstr_red = 0., tstr_green = 0., tstr_blue = 0., cstr_red, cstr_green, cstr_blue, cstr;
  double yscale = 1., uvscale = 1.;

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL) * 3;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL), *osrc = src;
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL), *odst = dst;
  unsigned char *end = src + height * irowstride;
  unsigned char *tmpcache = NULL;

  int maxcache = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  int inplace = (src == dst);

  int cross, red = 0, blue = 2;
  int b1, b2, b3, bx;
  int maxneeded = 0;
  int is_bgr = 0, is_yuv = 0, yuvmin = 0, uvmin = 0;

  register int i, j, k;

  if (sdata->ease_every == 0) {
    // easing (experimental) part 1
    int host_ease = weed_get_int_value(inst, WEED_LEAF_EASE_OUT, NULL);
    if (host_ease > 0) {
      // how many cycles to ease by 1
      sdata->ease_every = (int)((float)host_ease / (float)sdata->ccache);
    }
  }

  if (maxcache < 0) maxcache = 0;
  else if (maxcache > 50) maxcache = 50;

  if (sdata->ease_every == 0) {
    for (i = 1; i < maxcache; i++) {
      if (weed_get_boolean_value(in_params[RED_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE ||
          weed_get_boolean_value(in_params[GREEN_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE ||
          weed_get_boolean_value(in_params[BLUE_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE) {
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
    if (weed_get_boolean_value(in_params[RED_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE) {
      tstr_red += weed_get_double_value(in_params[STRENGTH(i)], WEED_LEAF_VALUE, NULL);
    }

    if (weed_get_boolean_value(in_params[GREEN_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE)
      tstr_green += weed_get_double_value(in_params[STRENGTH(i)], WEED_LEAF_VALUE, NULL);

    if (weed_get_boolean_value(in_params[BLUE_ON(i)], WEED_LEAF_VALUE, NULL) == WEED_TRUE) {
      tstr_blue += weed_get_double_value(in_params[STRENGTH(i)], WEED_LEAF_VALUE, NULL);
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
    if (weed_get_int_value(in_channel, WEED_LEAF_YUV_CLAMPING, NULL) == WEED_YUV_CLAMPING_CLAMPED) {
      // unclamp the values in the lut
      yuvmin = 16;
      uvmin = 16;
      yscale = 255. / 219.;
      uvscale = 255. / 224.;
    }
  }

  if (sdata->tcache == 0) {
    b1 = (weed_get_boolean_value(in_params[RED_ON(0)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);
    b2 = (weed_get_boolean_value(in_params[GREEN_ON(0)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);
    b3 = (weed_get_boolean_value(in_params[BLUE_ON(0)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);

    cstr = weed_get_double_value(in_params[4], WEED_LEAF_VALUE, NULL);
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

      b1 = (weed_get_boolean_value(in_params[RED_ON(j)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);
      b2 = (weed_get_boolean_value(in_params[GREEN_ON(j)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);
      b3 = (weed_get_boolean_value(in_params[BLUE_ON(j)], WEED_LEAF_VALUE, NULL) == WEED_TRUE);

      if (!b1 && !b2 && !b3 && j > 0) continue;

      if ((!is_bgr && sdata->is_bgr[j]) || (is_bgr && !sdata->is_bgr[j]))
        cross = 2;
      else
        cross = 0;

      cstr = weed_get_double_value(in_params[STRENGTH(j)], WEED_LEAF_VALUE, NULL);
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

  weed_free(in_params);

  // easing part 2
  if (sdata->ease_every <= 0) {
    if (sdata->ccache < sdata->tcache) sdata->ccache++;
    weed_set_int_value(inst, WEED_LEAF_PLUGIN_EASING, sdata->ccache);
  } else {
    if (sdata->ease_counter++ >= sdata->ease_every) {
      if (sdata->ccache > 0) sdata->ccache--;
      sdata->ease_counter = 0;
    }
    weed_set_int_value(inst, WEED_LEAF_PLUGIN_EASING, sdata->ccache * sdata->ease_every - sdata->ease_counter);
    if (sdata->ccache == 0) return WEED_ERROR_REINIT_NEEDED;
  }

  return WEED_SUCCESS;
}


static weed_error_t RGBd_deinit(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->cache) {
      for (int i = 0; i < sdata->tcache; i++) {
        if (sdata->cache[i]) weed_free(sdata->cache[i]);
      }
      weed_free(sdata->cache);
    }
    if (sdata->is_bgr) weed_free(sdata->is_bgr);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone;
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};
  int palette_list2[] = {WEED_PALETTE_YUV888, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_chantmpls2[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls2[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *filter_class, *gui;

  weed_plant_t *in_params[206];
  char *rfx_strings[54];
  char label[256];
  int i, j;

  in_params[0] = weed_integer_init("fcsize", "Frame _Cache Size (max)", 20, 0, 50);
  weed_set_int_value(in_params[0], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  for (i = 1; i < 205; i += 4) {
    for (j = 0; j < 3; j++) {
      if (j == 2) snprintf(label, 256, "        Frame -%-2d       ", (i - 1) / 4);
      else weed_memset(label, 0, 1);
      in_params[i + j] = weed_switch_init("", label, (i + j == 1 || i + j == 18 || i + j == 35) ? WEED_TRUE : WEED_FALSE);
    }
    in_params[i + j] = weed_float_init("", "", 1., 0., 1.);

    // TODO: set in init_func
    if (i >= 80) {
      gui = weed_paramtmpl_get_gui(in_params[i]);
      weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
      gui = weed_paramtmpl_get_gui(in_params[i + 1]);
      weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
      gui = weed_paramtmpl_get_gui(in_params[i + 2]);
      weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
      gui = weed_paramtmpl_get_gui(in_params[i + 3]);
      weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
    }
  }

  in_params[205] = NULL;

  filter_class = weed_filter_class_init("RGBdelay", "salsaman", 1, WEED_FILTER_HINT_LINEAR_GAMMA, palette_list,
                                        RGBd_init, RGBd_process, RGBd_deinit,
                                        in_chantmpls, out_chantmpls,
                                        in_params,
                                        NULL);

  gui = weed_filter_get_gui(filter_class);
  rfx_strings[0] = "layout|p0|";
  rfx_strings[1] = "layout|hseparator|";
  rfx_strings[2] = "layout|\"R\"|\"G\"|\"B\"|fill|fill|\"Blend Strength\"|fill|";

  for (i = 3; i < 54; i++) {
    rfx_strings[i] = weed_malloc(1024);
    if (rfx_strings[i] == NULL) return NULL;
    snprintf(rfx_strings[i], 1024, "layout|p%d|p%d|p%d|p%d|", (i - 3) * 4 + 1, (i - 3) * 4 + 2, (i - 3) * 4 + 3, (i - 2) * 4);
  }

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 54, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  rfx_strings[2] = "layout|\"Y\"|\"U\"|\"V\"|fill|fill|\"Blend Strength\"|fill|";

  filter_class = weed_filter_class_init("YUVdelay", "salsaman", 1, 0, palette_list2,
                                        RGBd_init, RGBd_process, RGBd_deinit, in_chantmpls2, out_chantmpls2,
                                        (clone = weed_clone_plants(in_params)), NULL);
  weed_free(clone);

  gui = weed_filter_get_gui(filter_class);
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 54, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  for (i = 3; i < 54; i++) weed_free(rfx_strings[i]);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END
