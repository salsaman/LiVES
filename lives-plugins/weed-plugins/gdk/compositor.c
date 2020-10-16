// simple_blend.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-utils.h>
#include <weed/weed-plugin-utils.h>
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-utils.h"
#include "../../../libweed/weed-plugin-utils.h"
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////
// gdk stuff for resizing

#include <gdk/gdk.h>

inline G_GNUC_CONST int pl_gdk_rowstride_value(int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}


inline int G_GNUC_CONST pl_gdk_last_rowstride_value(int width, int nchans) {
  // from gdk pixbuf docs
  return width * (((nchans << 3) + 7) >> 3);
}


static void plugin_free_buffer(guchar *pixels, gpointer data) {
  return;
}


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha,
                                      int bits_per_sample, int width, int height,
                                      guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels = has_alpha ? 4 : 3;
  int rowstride = pl_gdk_rowstride_value(width * channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height,
                                  rowstride, plugin_free_buffer, NULL);
}


static GdkPixbuf *pl_data_to_pixbuf(int palette, int width, int height, int irowstride,
                                    guchar *pixel_data, int xoffs, int yoffs) {
  GdkPixbuf *pixbuf;
  int orowstride;
  gboolean cheat = FALSE;
  gint n_channels;
  guchar *pixels, *end;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    if (!xoffs && !yoffs && irowstride == pl_gdk_rowstride_value(width * 3)) {
      pixbuf = pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat = TRUE;
    } else pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels = 3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    if (!xoffs && !yoffs && irowstride == pl_gdk_rowstride_value(width * 4)) {
      pixbuf = pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat = TRUE;
    } else pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels = 4;
    break;
  default:
    return NULL;
  }
  pixels = gdk_pixbuf_get_pixels(pixbuf);
  orowstride = gdk_pixbuf_get_rowstride(pixbuf);

  end = pixels + orowstride * height;

  if (!cheat) {
    gboolean done = FALSE;
    int widthx = width * n_channels;
    pixel_data += yoffs * irowstride;
    pixel_data += xoffs * n_channels;
    for (; pixels < end && !done; pixels += orowstride) {
      if (pixels + orowstride >= end) {
        orowstride = pl_gdk_last_rowstride_value(width, n_channels);
        done = TRUE;
      }
      weed_memcpy(pixels, pixel_data, widthx);
      if (widthx < orowstride) weed_memset(pixels + widthx, 0, orowstride - widthx);
      pixel_data += irowstride;
    }
  }
  return pixbuf;
}


///////////////////////////////////////////////////////

static void paint_pixel(unsigned char *dst, int dof, unsigned char *src, int sof, double alpha) {
  double invalpha;
  dst[dof] = dst[dof] * ((invalpha = 1. - alpha)) + src[sof] * alpha;
  dst[dof + 1] = dst[dof + 1] * invalpha + src[sof + 1] * alpha;
  dst[dof + 2] = dst[dof + 2] * invalpha + src[sof + 2] * alpha;
}


static weed_error_t compositor_process(weed_plant_t *inst, weed_timecode_t timecode) {
  GdkPixbuf *in_pixbuf, *out_pixbuf;
  weed_plant_t **in_channels = NULL;
  weed_plant_t **in_params;
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);

  double *offsx, *offsy, *scalex, *scaley, *alpha;

  unsigned char *src;
  unsigned char *dst = weed_channel_get_pixel_data(out_channel), *dst2;
  unsigned char *end;

  int *bgcol;

  double myoffsx, myoffsy, myscalex, myscaley, myalpha;

  gboolean revz;

  int owidth = weed_channel_get_width(out_channel), owidthx;
  int oheight = weed_channel_get_height(out_channel);
  int pal = weed_channel_get_palette(out_channel);
  int in_width, in_height, out_width, out_height;
  int num_in_channels = 0;
  int irowstride, orowstride = weed_channel_get_stride(out_channel);
  int numscalex = 0, numscaley = 0, numoffsx = 0, numoffsy = 0, numalpha = 0;
  int starti, endi, stepi;
  int up_interp = GDK_INTERP_HYPER;
  int down_interp = GDK_INTERP_BILINEAR;
  int r = 0, b = 2;
  int psize = pixel_size(pal);
  int x, y, z;
  int lbwidth, lbheight;
  int cuttop = 0, cutleft = 0;

  in_channels = weed_get_in_channels(inst, &num_in_channels);
  in_params = weed_get_in_params(inst, NULL);
  offsx = weed_param_get_array_double(in_params[0], &numoffsx);
  offsy = weed_param_get_array_double(in_params[1], &numoffsy);
  scalex = weed_param_get_array_double(in_params[2], &numscalex);
  scaley = weed_param_get_array_double(in_params[3], &numscaley);
  alpha = weed_param_get_array_double(in_params[4], &numalpha);
  bgcol = weed_param_get_array_int(in_params[5], NULL);
  revz = weed_param_get_value_boolean(in_params[6]);
  weed_free(in_params);

  // set out frame to bgcol
  if (pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_BGRA32) {
    r = 2; b = 0;
  }
  owidthx = owidth * psize;
  end = dst + oheight * orowstride;
  for (dst2 = dst; dst2 < end; dst2 += orowstride) {
    for (x = 0; x < owidthx; x += psize) {
      dst2[x] = bgcol[r];
      dst2[x + 1] = bgcol[1];
      dst2[x + 2] = bgcol[b];
      if (psize == 4) dst2[x + 3] = 0xFF;
    }
  }

  weed_free(bgcol);

  // add overlays in reverse order
  if (revz == WEED_FALSE) {
    starti = num_in_channels - 1;
    endi = -1;
    stepi = -1;
  } else {
    starti = 0;
    endi = num_in_channels;
    stepi = 1;
  }

  for (z = starti; z != endi; z += stepi) {
    // check if host disabled this channel : this is allowed as we have set WEED_LEAF_MAX_REPEATS
    if (weed_channel_is_disabled(in_channels[z])) continue;

    src = weed_channel_get_pixel_data(in_channels[z]);
    if (!src) continue;

    if (z < numoffsx) myoffsx = (int)(offsx[z] * (double)owidth);
    else myoffsx = 0;
    if (z < numoffsy) myoffsy = (int)(offsy[z] * (double)oheight);
    else myoffsy = 0;
    if (z < numscalex) myscalex = scalex[z];
    else myscalex = 1.;
    if (z < numscaley) myscaley = scaley[z];
    else myscaley = 1.;
    if (z < numalpha) myalpha = alpha[z];
    else myalpha = 1.;

    out_width = (((int)(owidth * myscalex + 1.)) >> 1) << 1;
    out_height = (((int)(oheight * myscaley + 1.)) >> 1) << 1;

    if (out_width * out_height >= 16) {
      lbwidth = in_width = weed_channel_get_width(in_channels[z]);
      lbheight = in_height = weed_channel_get_height(in_channels[z]);
      irowstride = weed_channel_get_stride(in_channels[z]);

      // scale image to new size
      if (weed_plant_has_leaf(in_channels[z], WEED_LEAF_INNER_SIZE)) {
        /// LETTERBOXING
        int *lbvals = weed_get_int_array(in_channels[z], WEED_LEAF_INNER_SIZE, NULL);
        int lbx = lbvals[0];
        int lby = lbvals[1];

        lbwidth = lbvals[2] / myscalex;
        lbheight = lbvals[3] / myscaley;

        if (lbwidth < in_width) {
          /// instead of upscaling, just cut some letterboxing from left and right
          int totlbw = in_width - lbvals[2];
          double lbscale = (double)(in_width - lbwidth) / (double)totlbw;
          int extra = (double)(in_width - lbx - lbvals[2]) * lbscale + .5;
          cutleft = (double)lbx * (1. - lbscale) + .5;
          in_width = lbx - cutleft + lbvals[2] + extra;
        } else {
          cutleft = lbx;
          in_width = lbvals[2];
        }
        if (lbheight < in_height) {
          int totlbh = in_height - lbvals[3];
          double lbscale = ((double)in_height - (double)lbheight) / (double)totlbh;
          int extra = (double)(in_height - lby - lbvals[3]) * lbscale + .5;
          cuttop = (double)lby * (1. - lbscale) + .5;
          in_height = lby - cuttop + lbvals[3] + extra;
        } else {
          cuttop = lby;
          in_height = lbvals[3];
        }
      }

      in_pixbuf = pl_data_to_pixbuf(pal, in_width, in_height, irowstride, (guchar *)src, cutleft, cuttop);

      if (out_width > in_width || out_height > in_height) {
        out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, out_width, out_height, up_interp);
      } else {
        out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, out_width, out_height, down_interp);
      }

      g_object_unref(in_pixbuf);

      src = gdk_pixbuf_get_pixels(out_pixbuf);

      out_width = gdk_pixbuf_get_width(out_pixbuf);
      out_height = gdk_pixbuf_get_height(out_pixbuf);
      irowstride = gdk_pixbuf_get_rowstride(out_pixbuf);

      for (y = myoffsy; y < oheight && y < myoffsy + out_height; y++) {
        for (x = myoffsx; x < owidth && x < myoffsx + out_width; x++) {
          paint_pixel(dst, y * orowstride + x * psize, src, (y - myoffsy) * irowstride
                      + (x - myoffsx) * psize, myalpha);
        }
      }
      g_object_unref(out_pixbuf);
    }
  }

  weed_free(offsx); weed_free(offsy);
  weed_free(scalex); weed_free(scaley);
  weed_free(alpha);

  if (num_in_channels > 0) weed_free(in_channels);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32,
                        WEED_PALETTE_RGBA32, WEED_PALETTE_END
                       };
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};

  weed_plant_t *in_params[] = {weed_float_init("xoffs", "_X offset", 0., 0., 1.), weed_float_init("yoffs", "_Y offset", 0., 0., 1.),
                               weed_float_init("scalex", "Scale _width", 1., 0., 1.), weed_float_init("scaley", "Scale _height", 1., 0., 1.),
                               weed_float_init("alpha", "_Alpha", 1.0, 0.0, 1.0), weed_colRGBi_init("bgcol", "_Background color", 0, 0, 0),
                               weed_switch_init("revz", "Invert _Z Index", WEED_FALSE), NULL
                              };

  int filter_flags = WEED_FILTER_CHANNEL_SIZES_MAY_VARY;

  // define RFX layout
  char *rfx_strings[] = {"layout|p6|",
                         "layout|p0|p1|",
                         "layout|p2|p3|",
                         "layout|p4|",
                         "layout|hseparator|",
                         "layout|p5|",
                         "special|framedraw|multirect|0|1|2|3|4|"
                        };

  //if (api_used >= 133) filter_flags |= WEED_FILTER_HINT_SRGB;

  weed_plant_t *filter_class = weed_filter_class_init("compositor", "salsaman", 1,
                               filter_flags, palette_list,
                               NULL, compositor_process, NULL,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plant_t *gui = weed_filter_get_gui(filter_class);

  // set to 0 to allow infinite repeats
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_MAX_REPEATS, 0);

  // this is necessary for the host
  weed_set_int_value(in_params[0], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_int_value(in_params[1], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_int_value(in_params[2], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_int_value(in_params[3], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_int_value(in_params[4], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);

  // set default value for elements added by the host
  weed_set_double_value(in_params[0], WEED_LEAF_NEW_DEFAULT, 0.);
  weed_set_double_value(in_params[1], WEED_LEAF_NEW_DEFAULT, 0.);
  weed_set_double_value(in_params[2], WEED_LEAF_NEW_DEFAULT, 1.);
  weed_set_double_value(in_params[3], WEED_LEAF_NEW_DEFAULT, 1.);
  weed_set_double_value(in_params[4], WEED_LEAF_NEW_DEFAULT, 1.);

  weed_set_string_value(in_params[6], WEED_LEAF_DESCRIPTION, "If checked, the rear frames overlay the front ones.");

  // set RFX layout
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 7, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;
