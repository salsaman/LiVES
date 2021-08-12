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
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
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
    if (sdata->cache[i - 1]) {
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
  if (!sdata->cache) {
    weed_free(sdata->is_bgr);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = oldsize; i < newsize; i++) {
    sdata->cache[i] = weed_malloc(width * height * 3);
    if (!sdata->cache[i]) {
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
  weed_plant_t **in_params = weed_get_in_params(inst, NULL), *gui;
  int maxcache = weed_param_get_value_int(in_params[0]);
  int i;

  _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));

  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->ccache = sdata->tcache = 0;
  sdata->cache = NULL;
  sdata->is_bgr = weed_malloc(maxcache * sizeof(int));

  if (!sdata->is_bgr) {
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

#define USE_LUT

static weed_error_t RGBd_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);

  size_t x = 0;

  double tstr_red = 0., tstr_green = 0., tstr_blue = 0., cstr_red, cstr_green, cstr_blue, cstr;
  double yscale = 1., uvscale = 1.;

  int width = weed_channel_get_width(in_channel) * 3;
  int height = weed_channel_get_height(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  int palette = weed_channel_get_palette(in_channel);

  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);
  unsigned char *tmpcache = NULL;

  int maxcache = weed_param_get_value_int(in_params[0]);
  int inplace = (src == dst);

  int cross;
#ifdef USE_LUT
  int red = 0, blue = 2;
#endif
  int b1, b2, b3, bx;
  int maxneeded = 0;
  int is_bgr = 0, is_yuv = 0, yuvmin = 0, uvmin = 0;
  int i, j, k, d, s;

  int iframesize = height * irowstride;
  int dframesize = height * orowstride;
#ifndef USE_LUT
  float rxval, gxval, bxval, rval, gval, bval;
#endif

  weed_plant_t *gui = weed_instance_get_gui(inst);
  int host_ease = weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL);
  if (host_ease > 0) {
    if (sdata->ease_every == 0) {
      // easing (experimental) part 1
      // how many cycles to ease by 1
      sdata->ease_every = (int)((float)host_ease / (float)sdata->ccache);
    }
  }
  else sdata->ease_every = sdata->ease_counter = 0;

  if (maxcache < 0) maxcache = 0;
  else if (maxcache > 50) maxcache = 50;

  if (sdata->ease_every == 0) {
    for (i = 1; i < maxcache; i++) {
      if (weed_param_get_value_boolean(in_params[RED_ON(i)]) == WEED_TRUE ||
          weed_param_get_value_boolean(in_params[GREEN_ON(i)]) == WEED_TRUE ||
          weed_param_get_value_boolean(in_params[BLUE_ON(i)]) == WEED_TRUE) {
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

    // normalise the blend strength for each colour channel, so the total doesn't exceed 1.0
    // tstr_* hold the overall totals
    if (weed_param_get_value_boolean(in_params[RED_ON(i)]) == WEED_TRUE) {
      tstr_red += weed_param_get_value_double(in_params[STRENGTH(i)]);
    }

    if (weed_param_get_value_boolean(in_params[GREEN_ON(i)]) == WEED_TRUE)
      tstr_green += weed_param_get_value_double(in_params[STRENGTH(i)]);

    if (weed_param_get_value_boolean(in_params[BLUE_ON(i)]) == WEED_TRUE) {
      tstr_blue += weed_param_get_value_double(in_params[STRENGTH(i)]);
    }
  }

  if (palette == WEED_PALETTE_BGR24) {
    // red/blue values swapped
    is_bgr = sdata->is_bgr[0] = 1;
  } else is_bgr = sdata->is_bgr[0] = 0;

  if (sdata->tcache > 0) {
    // copy current frame -> qsdata->cache[0]
    for (j = 0; j < iframesize; j += irowstride) {
      weed_memcpy(&tmpcache[x], &src[j], width);
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
    if (weed_channel_get_yuv_clamping(in_channel) == WEED_YUV_CLAMPING_CLAMPED) {
      // unclamp the values in the lut
      yuvmin = 16; uvmin = 16;
      yscale = 255. / 219.;
      uvscale = 255. / 224.;
    }
  }

  if (sdata->tcache == 0) {
    b1 = (weed_param_get_value_boolean(in_params[RED_ON(0)]) == WEED_TRUE);
    b2 = (weed_param_get_value_boolean(in_params[GREEN_ON(0)]) == WEED_TRUE);
    b3 = (weed_param_get_value_boolean(in_params[BLUE_ON(0)]) == WEED_TRUE);

    cstr = weed_param_get_value_double(in_params[4]);
    cstr_red = cstr / tstr_red;
    cstr_green = cstr / tstr_green;
    cstr_blue = cstr / tstr_blue;

    if (is_bgr) {
      // swap r / b (on-off and lut)
      bx = b1; b1 = b3; b3 = bx;
#ifdef USE_LUT
      red = 2; blue = 0;
#endif
    }

#ifdef USE_LUT
    make_lut(sdata->lut[red], cstr_red * yscale, yuvmin);
    make_lut(sdata->lut[1], cstr_green * uvscale, yuvmin);
    make_lut(sdata->lut[blue], cstr_blue * uvscale, yuvmin);
#else
    rxval = (float)(cstr_red * yscale);
    gxval = (float)(cstr_green * uvscale);
    bxval = (float)(cstr_blue * uvscale);
#endif

    s = 0;
    for (d = 0; d < dframesize; d += orowstride) {
      for (i = 0; i < width; i += 3) {
#ifdef USE_LUT
        if (b1) dst[d + i] = sdata->lut[0][src[s + i]];
        else if (inplace) dst[d + i] = yuvmin;
        if (b2) dst[d + i + 1] = sdata->lut[1][src[s + i + 1]];
        else if (inplace) dst[d + i + 1] = uvmin;
        if (b3) dst[d + i + 2] = sdata->lut[2][src[s + i + 2]];
        else if (inplace) dst[d + i + 2] = uvmin;
#else
        if (b1) {
          rval = (float)(src[s + i] - yuvmin) * rxval + .5;
          if (rval < 0.) rval = 0.;
          else if (rval > 255.) rval = 255.;
          dst[d + i] = (unsigned char)rval;
        } else if (inplace) dst[d + i] = yuvmin;
        if (b2) {
          gval = (float)(src[s + i + 1] - yuvmin) * gxval + .5;
          if (gval < 0.)gval = 0.;
          else if (gval > 255.) gval = 255.;
          dst[d + i + 1] = (unsigned char)gval;
        } else if (inplace) dst[d + i + 1] = uvmin;
        if (b3) {
          bval = (float)(src[s + i + 2] - yuvmin) * bxval + .5;
          if (bval < 0.)bval = 0.;
          else if (bval > 255.) bval = 255.;
          dst[d + i + 2] = (unsigned char)bval;
        } else if (inplace) dst[d + i + 2] = uvmin;
#endif
      }
      s += irowstride;
    }
  } else {
    weed_memset(dst, 0, dframesize);
    for (j = 0; j < sdata->tcache; j++) {
      // maybe overlay something from j frames ago
      if (j <= sdata->ccache) k = j;
      else k = sdata->ccache;

      b1 = (weed_param_get_value_boolean(in_params[RED_ON(j)]) == WEED_TRUE);
      b2 = (weed_param_get_value_boolean(in_params[GREEN_ON(j)]) == WEED_TRUE);
      b3 = (weed_param_get_value_boolean(in_params[BLUE_ON(j)]) == WEED_TRUE);

      if (!b1 && !b2 && !b3 && j > 0) continue;

      if ((!is_bgr && sdata->is_bgr[j]) || (is_bgr && !sdata->is_bgr[j]))
        cross = 2;
      else
        cross = 0;

      cstr = weed_param_get_value_double(in_params[STRENGTH(j)]);
      cstr_red = cstr / tstr_red;
      cstr_green = cstr / tstr_green;
      cstr_blue = cstr / tstr_blue;

      if (sdata->is_bgr[j]) {
        // swap r / b (on-off and lut)
        bx = b1; b1 = b3; b3 = bx;
#ifdef USE_LUT
        red = 2; blue = 0;
#endif
      } else {
#ifdef USE_LUT
        red = 0; blue = 2;
#endif
      }

#ifdef USE_LUT
      make_lut(sdata->lut[red], cstr_red * yscale, yuvmin);
      make_lut(sdata->lut[1], cstr_green * uvscale, yuvmin);
      make_lut(sdata->lut[blue], cstr_blue * uvscale, yuvmin);
#else
      rxval = (float)(cstr_red * yscale);
      gxval = (float)(cstr_green * uvscale);
      bxval = (float)(cstr_blue * uvscale);
#endif

      s = 0;
      for (d = 0; d < dframesize; d += orowstride) {
        for (i = 0; i < width; i += 3) {
#ifdef USE_LUT
          if (b1) dst[d + i] += sdata->lut[0][sdata->cache[k][s + i + cross]];
          if (b2) dst[d + i + 1] += sdata->lut[1][sdata->cache[k][s + i + 1]];
          if (b3) dst[d + i + 2] += sdata->lut[2][sdata->cache[k][s + i + 2 - cross]];
#else
          if (b1) {
            rval = (float)(sdata->cache[k][s + i + cross] - yuvmin) * rxval + .5;
            if (rval < 0.) rval = 0.;
            else if (rval > 255.) rval = 255.;
            dst[d + i] += (unsigned char)rval;
          }
          if (b2) {
            gval = (float)(sdata->cache[k][s + i + 1] - yuvmin) * gxval + .5;
            if (gval < 0.)gval = 0.;
            else if (gval > 255.) gval = 255.;
            dst[d + i + 1] += (unsigned char)gval;
          }
          if (b3) {
            bval = (float)(sdata->cache[k][s + i + 2 - cross] - yuvmin) * bxval + .5;
            if (bval < 0.)bval = 0.;
            else if (bval > 255.) bval = 255.;
            dst[d + i + 2] += (unsigned char)bval;
          }
#endif
        }
        s += width;
      }
    }
  }

  if (is_yuv && yuvmin == 16) {
    // reclamp the values
    make_lut(sdata->lut[0], 1. / yscale, -yuvmin);
    make_lut(sdata->lut[1], 1. / uvscale, -yuvmin);
    for (d = 0; d < dframesize; d += orowstride) {
      for (i = 0; i < width; i += 3) {
        dst[d + i] = sdata->lut[0][dst[d + i]];
        dst[d + i + 1] = sdata->lut[1][dst[d + i + 1]];
        dst[d + i + 2] = sdata->lut[1][dst[d + i + 2]];
      }
    }
  }

  weed_free(in_params);

  // easing part 2
  if (sdata->ease_every <= 0) {
    weed_plant_t *gui = weed_instance_get_gui(inst);
    if (sdata->ccache < sdata->tcache) sdata->ccache++;
    weed_set_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, sdata->ccache);
  } else {
    weed_plant_t *gui = weed_instance_get_gui(inst);
    if (sdata->ease_counter++ >= sdata->ease_every) {
      if (sdata->ccache > 0) sdata->ccache--;
      sdata->ease_counter = 0;
    }
    weed_set_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, sdata->ccache * sdata->ease_every - sdata->ease_counter);
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
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_END};
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
  gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_gui_set_flags(gui, WEED_GUI_REINIT_ON_VALUE_CHANGE);

  weed_paramtmpl_set_flags(in_params[0], WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

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

  filter_class = weed_filter_class_init("RGBdelay", "salsaman", 1, WEED_FILTER_PREF_LINEAR_GAMMA, palette_list,
                                        RGBd_init, RGBd_process, RGBd_deinit, in_chantmpls, out_chantmpls,
                                        in_params, NULL);

  gui = weed_filter_get_gui(filter_class);
  rfx_strings[0] = "layout|p0|";
  rfx_strings[1] = "layout|hseparator|";
  rfx_strings[2] = "layout|\"R\"|fill|\"G\"|fill|\"B\"|fill|fill|fill|\"Blend Strength\"|fill|";

  for (i = 3; i < 54; i++) {
    rfx_strings[i] = weed_malloc(1024);
    if (!rfx_strings[i]) return NULL;
    snprintf(rfx_strings[i], 1024, "layout|p%d|p%d|p%d|p%d|",
             (i - 3) * 4 + 1, (i - 3) * 4 + 2, (i - 3) * 4 + 3, (i - 2) * 4);
  }

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 54, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  rfx_strings[2] = "layout|\"Y\"|fill|\"U\"|fill|\"V\"|fill|fill|fill|\"Blend Strength\"|fill|";

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

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END
