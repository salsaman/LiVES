// gdk_fast_resize.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2007
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed-utils.h" // optional
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

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


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height,
                                      guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels = has_alpha ? 4 : 3;
  int rowstride = pl_gdk_rowstride_value(width * channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer,
                                  NULL);
}


static GdkPixbuf *pl_channel_to_pixbuf(weed_plant_t *channel) {
  GdkPixbuf *pixbuf;
  int palette = weed_channel_get_palette(channel);
  int width = weed_channel_get_width(channel);
  int height = weed_channel_get_height(channel);
  int irowstride = weed_channel_get_stride(channel);
  int rowstride, orowstride, xrowstride;
  guchar *pixel_data = weed_channel_get_pixel_data(channel), *pixels;
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
  case WEED_PALETTE_ARGB32:
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
  xrowstride = rowstride;

  if (!cheat) {
    for (int i = 0; i < height; i++) {
      if (i == height - 1) {
        xrowstride = pl_gdk_last_rowstride_value(width, n_channels);
      }
      weed_memcpy(&pixels[orowstride * i], &pixel_data[rowstride * i], xrowstride);
    }
  }
  return pixbuf;
}


static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf) {
  // return TRUE if we can use the original pixbuf pixels
  int rowstride = gdk_pixbuf_get_rowstride(pixbuf), xrowstride = rowstride;
  int width = gdk_pixbuf_get_width(pixbuf);
  int height = gdk_pixbuf_get_height(pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
  guchar *in_pixel_data = (guchar *)gdk_pixbuf_get_pixels(pixbuf);
  int out_rowstride = weed_channel_get_stride(channel);
  guchar *dst = weed_channel_get_pixel_data(channel);

  if (rowstride == pl_gdk_last_rowstride_value(width, n_channels) && rowstride == out_rowstride) {
    weed_memcpy(dst, in_pixel_data, rowstride * height);
    return FALSE;
  }

  for (int i = 0; i < height; i++) {
    if (i == height - 1) xrowstride = pl_gdk_last_rowstride_value(width, n_channels);
    weed_memcpy(&dst[out_rowstride * i], &in_pixel_data[rowstride * i], xrowstride);
  }

  return FALSE;
}


static weed_error_t resize_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);

  int inwidth = weed_channel_get_width(in_channel);
  int inheight = weed_channel_get_height(in_channel);

  int outwidth = weed_channel_get_width(out_channel);
  int outheight = weed_channel_get_height(out_channel);

  GdkPixbuf *in_pixbuf = pl_channel_to_pixbuf(in_channel);
  GdkPixbuf *out_pixbuf;

  int up_interp = GDK_INTERP_HYPER;
  int down_interp = GDK_INTERP_BILINEAR;

  if (outwidth > inwidth || outheight > inheight) {
    out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, outwidth, outheight, up_interp);
  } else {
    out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, outwidth, outheight, down_interp);
  }

  g_object_unref(in_pixbuf);
  pl_pixbuf_to_channel(out_channel, out_pixbuf);
  g_object_unref(out_pixbuf);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("gdk_fast_resize", "salsaman", 1,
                               WEED_FILTER_IS_CONVERTER | WEED_FILTER_CHANNEL_SIZES_MAY_VARY, palette_list,
                               NULL, resize_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
