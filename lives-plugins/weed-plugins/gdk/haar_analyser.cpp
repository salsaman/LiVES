// haar_analyser.cpp
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// based partly on earlier code:

/***************************************************************************
    imgSeek ::  Haar 2d transform implemented in C/C++ to speed things up
                             -------------------
    begin                : Fri Jan 17 2003
    email                : nieder|at|mail.ru
    Time-stamp:            <05/01/30 19:58:56 rnc>
    ***************************************************************************
    *    Wavelet algorithms, metric and query ideas based on the paper        *
    *    Fast Multiresolution Image Querying                                  *
    *    by Charles E. Jacobs, Adam Finkelstein and David H. Salesin.         *
    *    <http://www.cs.washington.edu/homes/salesin/abstracts.html>          *
    ***************************************************************************

    Copyright (C) 2003 Ricardo Niederberger Cabral

    Clean-up and speed-ups by Geert Janssen <geert at ieee.org>, Jan 2006:
    - introduced names for various `magic' numbers
    - made coding style suitable for Emacs c-mode
    - expressly doing constant propagation by hand (combined scalings)
    - preferring pointer access over indexed access of arrays
    - introduced local variables to avoid expression re-evaluations
    - took out all dynamic allocations
    - completely rewrote calcHaar and eliminated truncq()
    - better scheme of introducing sqrt(0.5) factors borrowed from
      FXT package: author Joerg Arndt, email: arndt@jjj.de,
      http://www.jjj.de/
    - separate processing per array: better cache behavior
    - do away with all scaling; not needed except for DC component

    To do:
    - the whole Haar transform should be done using fixpoints

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions = 1; // number of different weed api versions supported
static int api_versions[] = {131}; // array of weed api versions supported in plugin, in order (most preferred first)

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#define NEED_PALETTE_UTILS
#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

/* C Includes */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdk.h>

/* imgSeek Includes */
#include "haar_analyser.h"


// Do the Haar tensorial 2d transform itself.
// Here input is RGB data [0..255] in Unit arrays
// Computation is (almost) in-situ.
static void haar2D(Unit a[]) {
  int i;
  Unit t[NUM_PIXELS >> 1];

  // scale by 1/sqrt(128) = 0.08838834764831843:
  /*
  for (i = 0; i < NUM_PIXELS_SQUARED; i++)
    a[i] *= 0.08838834764831843;
  */

  // Decompose rows:
  for (i = 0; i < NUM_PIXELS_SQUARED; i += NUM_PIXELS) {
    int h, h1;
    Unit C = 1;

    for (h = NUM_PIXELS; h > 1; h = h1) {
      int j1, j2, k;

      h1 = h >> 1;		// h = 2*h1
      C *= 0.7071;		// 1/sqrt(2)
      for (k = 0, j1 = j2 = i; k < h1; k++, j1++, j2 += 2) {
        int j21 = j2 + 1;

        t[k]  = (a[j2] - a[j21]) * C;
        a[j1] = (a[j2] + a[j21]);
      }
      // Write back subtraction results:
      weed_memcpy(a + i + h1, t, h1 * sizeof(a[0]));
    }
    // Fix first element of each row:
    a[i] *= C;	// C = 1/sqrt(NUM_PIXELS)
  }

  // scale by 1/sqrt(128) = 0.08838834764831843:
  /*
  for (i = 0; i < NUM_PIXELS_SQUARED; i++)
    a[i] *= 0.08838834764831843;
  */

  //q Decompose columns:
  for (i = 0; i < NUM_PIXELS; i++) {
    Unit C = 1;
    int h, h1;

    for (h = NUM_PIXELS; h > 1; h = h1) {
      int j1, j2, k;

      h1 = h >> 1;
      C *= 0.7071;		// 1/sqrt(2) = 0.7071
      for (k = 0, j1 = j2 = i; k < h1;
           k++, j1 += NUM_PIXELS, j2 += 2 * NUM_PIXELS) {
        int j21 = j2 + NUM_PIXELS;

        t[k]  = (a[j2] - a[j21]) * C;
        a[j1] = (a[j2] + a[j21]);
      }
      // Write back subtraction results:
      for (k = 0, j1 = i + h1 * NUM_PIXELS; k < h1; k++, j1 += NUM_PIXELS)
        a[j1] = t[k];
    }
    // Fix first element of each column:
    a[i] *= C;
  }
}


/* Do the Haar tensorial 2d transform itself.
   Here input is unclamped YUV data [0..255] in Unit arrays.
   Results are available in a, b, and c.
   Fully inplace calculation; order of result is interleaved though,
   but we don't care about that.
*/
static void transform(Unit *a, Unit *b, Unit *c, int pal) {
  if (pal == WEED_PALETTE_RGB24) {
    RGB_2_YIQ(a, b, c);
  } else if (pal == WEED_PALETTE_BGR24) {
    RGB_2_YIQ(c, b, a);
  }

  haar2D(a);
  haar2D(b);
  haar2D(c);

  /* Reintroduce the skipped scaling factors: */
  a[0] /= 2 * NUM_PIXELS_SQUARED;
  b[0] /= 2 * NUM_PIXELS_SQUARED;
  c[0] /= 2 * NUM_PIXELS_SQUARED;
}


// Find the NUM_COEFS largest numbers in cdata[] (in magnitude that is)
// and store their indices in sig[].
inline static void get_m_largests(Unit *cdata, Idx *sig, int num_coefs) {
  int cnt, i;
  valStruct val;
  valqueue vq;			// dynamic priority queue of valStruct's

  // Could skip i=0: goes into separate avgl

  // Fill up the bounded queue. (Assuming NUM_PIXELS_SQUARED > NUM_COEFS)
  for (i = 1; i < num_coefs + 1; i++) {
    val.i = i;
    val.d = ABS(cdata[i]);
    vq.push(val);
  }
  // Queue is full (size is NUM_COEFS)

  for (/*i = NUM_COEFS+1*/; i < NUM_PIXELS_SQUARED; i++) {
    val.d = ABS(cdata[i]);

    if (val.d > vq.top().d) {
      // Make room by dropping smallest entry:
      vq.pop();
      // Insert val as new entry:
      val.i = i;
      vq.push(val);
    }
    // else discard: do nothing
  }

  // Empty the (non-empty) queue and fill-in sig:
  cnt = 0;
  do {
    int t;

    val = vq.top();
    t = (cdata[val.i] <= 0);	/* t = 0 if pos else 1 */
    /* i - 0 ^ 0 = i; i - 1 ^ 0b111..1111 = 2-compl(i) = -i */
    sig[cnt++] = (val.i - t) ^ -t; // never 0
    vq.pop();
  } while (!vq.empty());
  // Must have cnt==NUM_COEFS here.
}


// Determines a total of NUM_COEFS positions in the image that have the
// largest magnitude (absolute value) in color value. Returns linearized
// coordinates in sig1, sig2, and sig3. avgl are the [0,0] values.
// The order of occurrence of the coordinates in sig doesn't matter.
// Complexity is 3 x NUM_PIXELS^2 x 2log(NUM_COEFS).
static int calcHaar(Unit *cdata1, Unit *cdata2, Unit *cdata3,
                    Idx *sig1, Idx *sig2, Idx *sig3, double *avgl, int num_coefs) {
  avgl[0] = cdata1[0];
  avgl[1] = cdata2[0];
  avgl[2] = cdata3[0];

  // Color channel 1:
  get_m_largests(cdata1, sig1, num_coefs);

  // Color channel 2:
  get_m_largests(cdata2, sig2, num_coefs);

  // Color channel 3:
  get_m_largests(cdata3, sig3, num_coefs);

  return 1;
}


///////////////////////////////////////

inline G_GNUC_CONST int pl_gdk_rowstride_value(int rowstride) {
  return (rowstride + 3) & ~3;
}


inline int G_GNUC_CONST pl_gdk_last_rowstride_value(int width, int nchans) {
  return width * (((nchans << 3) + 7) >> 3);
}


static void plugin_free_buffer(guchar *pixels, gpointer data) {
  return;
}


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height,
                                      guchar *buf) {
  int channels = has_alpha ? 4 : 3;
  int rowstride = pl_gdk_rowstride_value(width * channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}


static GdkPixbuf *pl_channel_to_pixbuf(weed_plant_t *channel) {
  int error;
  GdkPixbuf *pixbuf;
  int palette = weed_get_int_value(channel, "current_palette", &error);
  int width = weed_get_int_value(channel, "width", &error);
  int height = weed_get_int_value(channel, "height", &error);
  int irowstride = weed_get_int_value(channel, "rowstrides", &error);
  int rowstride, orowstride;
  guchar *pixel_data = (guchar *)weed_get_voidptr_value(channel, "pixel_data", &error), *pixels, *end;
  gboolean cheat = FALSE;
  gint n_channels;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_YUV888:
    if (irowstride == pl_gdk_rowstride_value(width * 3)) {
      pixbuf = pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat = TRUE;
    } else pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels = 3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_YUVA8888:
    if (irowstride == pl_gdk_rowstride_value(width * 4)) {
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

  if (irowstride > orowstride) rowstride = orowstride;
  else rowstride = irowstride;
  end = pixels + orowstride * height;

  if (!cheat) {
    gboolean done = FALSE;
    for (; pixels < end && !done; pixels += orowstride) {
      if (pixels + orowstride >= end) {
        orowstride = rowstride = pl_gdk_last_rowstride_value(width, n_channels);
        done = TRUE;
      }
      weed_memcpy(pixels, pixel_data, rowstride);
      if (rowstride < orowstride) weed_memset(pixels + rowstride, 0, orowstride - rowstride);
      pixel_data += irowstride;
    }
  }
  return pixbuf;
}


static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf) {
  int error;
  int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
  int width = gdk_pixbuf_get_width(pixbuf);
  int height = gdk_pixbuf_get_height(pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
  guchar *in_pixel_data = (guchar *)gdk_pixbuf_get_pixels(pixbuf);
  int out_rowstride = weed_get_int_value(channel, "rowstrides", &error);
  guchar *dst = (guchar *)weed_get_voidptr_value(channel, "pixel_data", &error);

  register int i;

  if (rowstride == pl_gdk_last_rowstride_value(width, n_channels) && rowstride == out_rowstride) {
    weed_memcpy(dst, in_pixel_data, rowstride * height);
    return FALSE;
  }

  for (i = 0; i < height; i++) {
    if (i == height - 1) rowstride = pl_gdk_last_rowstride_value(width, n_channels);
    weed_memcpy(dst, in_pixel_data, rowstride);
    in_pixel_data += rowstride;
    dst += out_rowstride;
  }

  return FALSE;
}


//////////////////////////

typedef struct {
  int coefs;
  Idx *sig1;
  Idx *sig2;
  Idx *sig3;
} _sdata;


static int make_sigs(_sdata *sdata, int num_coefs) {
  sdata->sig1 = (Idx *)weed_malloc(num_coefs * sizeof(Idx));
  if (sdata->sig1 == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->sig2 = (Idx *)weed_malloc(num_coefs * sizeof(Idx));
  if (sdata->sig2 == NULL) {
    weed_free(sdata->sig1);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->sig3 = (Idx *)weed_malloc(num_coefs * sizeof(Idx));
  if (sdata->sig3 == NULL) {
    weed_free(sdata->sig1);
    weed_free(sdata->sig2);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->coefs = num_coefs;
  return WEED_NO_ERROR;
}


////////////////////////////////////////////////////////////

static int haar_init(weed_plant_t *inst) {
  int error, retval;
  _sdata *sdata;
  weed_plant_t **in_params = (weed_plant_t **)weed_get_plantptr_array(inst, "in_parameters", &error);
  int num_coefs = weed_get_int_value(in_params[0], "value", &error);

  weed_free(in_params);

  sdata = (_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  if ((retval = make_sigs(sdata, num_coefs)) != WEED_NO_ERROR) return retval;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_NO_ERROR;
}


static int haar_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  if (sdata != NULL) {
    weed_free(sdata->sig1);
    weed_free(sdata->sig2);
    weed_free(sdata->sig3);
    weed_free(sdata);
  }

  return WEED_NO_ERROR;
}


static int haar_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *channel = weed_get_plantptr_value(inst, "in_channels", &error);
  unsigned char **orig_src, *src;
  int width = weed_get_int_value(channel, "width", &error);
  int height = weed_get_int_value(channel, "height", &error);
  int pal = weed_get_int_value(channel, "current_palette", &error);
  int irowstride = weed_get_int_value(channel, "rowstrides", &error);
  weed_plant_t **out_params = (weed_plant_t **)weed_get_plantptr_array(inst, "out_parameters", &error);
  weed_plant_t **in_params = (weed_plant_t **)weed_get_plantptr_array(inst, "in_parameters", &error);

  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  int psize = 4;
  int clamped = 0;

  int *orig_rows, orowstride, nplanes, hmax;

  int num_coefs = weed_get_int_value(in_params[0], "value", &error);

  unsigned char *end = src + height * irowstride;
  register int i, j, k;

  int cn = 0;
  Unit cdata1[NUM_PIXELS_SQUARED];
  Unit cdata2[NUM_PIXELS_SQUARED];
  Unit cdata3[NUM_PIXELS_SQUARED];

  double avgl[3];

  GError **gerror;

  GdkPixbuf *new_pixbuf;
  GdkPixbuf *out_pixbuf = NULL;

  weed_free(in_params);

  if (weed_plant_has_leaf(channel, "YUV_clamping") &&
      (weed_get_int_value(channel, "YUV_clamping", &error) == WEED_YUV_CLAMPING_CLAMPED))
    clamped = 1;

  if (pal == WEED_PALETTE_YUV888) psize = 3;

  // resize to NUM_PIXELS x NUM_PIXELS

  if (width != NUM_PIXELS || height != NUM_PIXELS) {
    GdkPixbuf *in_pixbuf = pl_channel_to_pixbuf(channel);
    GdkInterpType up_interp = GDK_INTERP_HYPER;
    GdkInterpType down_interp = GDK_INTERP_BILINEAR;

    if (width > NUM_PIXELS || height > NUM_PIXELS) {
      out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, NUM_PIXELS, NUM_PIXELS, up_interp);
    } else {
      out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, NUM_PIXELS, NUM_PIXELS, down_interp);
    }

    g_object_unref(in_pixbuf);

    irowstride = gdk_pixbuf_get_rowstride(out_pixbuf);

    src = gdk_pixbuf_get_pixels(out_pixbuf);
  } else src = orig_src[0];

  hmax = NUM_PIXELS * psize;

  for (i = 0; i < NUM_PIXELS; i++) {
    k = i * irowstride;
    for (j = 0; j < hmax; j += psize) {
      if (clamped) {
        // convert to unclamped
        cdata1[cn] = YCL_YUCL[src[k + j]];
        cdata2[cn] = UVCL_UVUCL[src[k + j + 1]];
        cdata3[cn] = UVCL_UVUCL[src[k + j]];
      } else {
        // unclamped YUV - pass through
        cdata1[cn] = src[k + j];
        cdata2[cn] = src[k + j + 1];
        cdata3[cn] = src[k + j];
      }
      cn++;
    }
  }

  if (out_pixbuf != NULL) {
    g_object_unref(out_pixbuf);
  } else {
    if (src != orig_src[0]) {
      weed_set_voidptr_array(channel, "pixel_data", nplanes, (void **)orig_src);
      weed_set_int_array(channel, "rowstrides", nplanes, orig_rows);
    }
  }

  weed_free(orig_src);
  weed_free(orig_rows);

  if (num_coefs != sdata->coefs) {
    weed_free(sdata->sig1);
    weed_free(sdata->sig2);
    weed_free(sdata->sig3);
    make_sigs(sdata, num_coefs);
  }

  transform(cdata1, cdata2, cdata3, pal);
  calcHaar(cdata1, cdata2, cdata3, sdata->sig1, sdata->sig2, sdata->sig3, avgl, num_coefs);

  weed_set_int_array(out_params[0], "value", num_coefs, sdata->sig1);
  weed_set_int_array(out_params[1], "value", num_coefs, sdata->sig2);
  weed_set_int_array(out_params[2], "value", num_coefs, sdata->sig3);

  weed_set_double_value(out_params[3], "value", avgl[0]);
  weed_set_double_value(out_params[4], "value", avgl[1]);
  weed_set_double_value(out_params[5], "value", avgl[2]);

#ifdef DEBUG
  printf("vals %f %f %f\n", avgl[0], avgl[1], avgl[2]);

  for (i = 0; i < num_coefs; i++) {
    printf("vals %d: %d %d %d\n", i, sdata->sig1[i], sdata->sig2[i], sdata->sig3[i]);
  }
#endif

  weed_free(out_params);

  return WEED_NO_ERROR;
}


//
#define VLIMIT 4096

weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, num_versions, api_versions);
  if (plugin_info != NULL) {
    int palette_list[] = {WEED_PALETTE_YUVA8888, WEED_PALETTE_YUV888,
                          WEED_PALETTE_END
                         };
    weed_plant_t *out_params[] = {weed_out_param_integer_init("Y maxima", 0, -VLIMIT, VLIMIT), weed_out_param_integer_init("U maxima", 0, -VLIMIT, VLIMIT), weed_out_param_integer_init("V maxima", 0, -VLIMIT, VLIMIT), weed_out_param_float_init("Y average", 0., 0., 1.), weed_out_param_float_init("U average", 0., 0., 1.), weed_out_param_float_init("V average", 0., 0., 1.), NULL};
    weed_plant_t *in_params[] = {weed_integer_init("nco", "Number of _Coefficients", 40, 1, NUM_PIXELS), NULL};
    weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
    weed_plant_t *filter_class = weed_filter_class_init("haar_analyser", "salsaman and others", 1, 0, &haar_init,
                                 &haar_process, &haar_deinit, in_chantmpls, NULL, in_params, out_params);

    weed_set_int_value(out_params[0], "flags", WEED_PARAMETER_VARIABLE_ELEMENTS);
    weed_set_int_value(out_params[1], "flags", WEED_PARAMETER_VARIABLE_ELEMENTS);
    weed_set_int_value(out_params[2], "flags", WEED_PARAMETER_VARIABLE_ELEMENTS);

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    weed_set_int_value(plugin_info, "version", package_version);

    init_RGB_to_YCbCr_tables();
    init_Y_to_Y_tables();
  }
  return plugin_info;
}
