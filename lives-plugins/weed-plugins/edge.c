// edge.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2015
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
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <string.h>
#include <math.h>

/////////////////////////////////////////////

typedef struct {
  uint8_t *map;
} static_data;

//static int video_width_margin;


static weed_error_t edge_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int width, height;
  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_in_channel(inst, 0);
  width = weed_channel_get_width(in_channel);
  height = weed_channel_get_height(in_channel);

  sdata->map = weed_calloc(height, width * 3);
  if (!sdata->map) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t edge_deinit(weed_plant_t *inst) {
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    weed_free(sdata->map);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static inline void copywalpha(uint8_t *dest, size_t doffs, uint8_t *src,
                              size_t offs, uint8_t red, uint8_t green, uint8_t blue) {
  // copy alpha from src, and RGB from val; return val
  dest[doffs] = (((red * src[0]) >> 7) + red) >> 1;
  // dest[doffs + 1] = (((green * src[1]) >> 7) + green) >> 1;
  dest[doffs + 1] = green;
  dest[doffs + 2] = (((blue * src[2]) >> 7) + blue) >> 1;
  dest[doffs + 3] = src[offs + 3];
}


static weed_error_t  edge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);

  uint8_t *src = (uint8_t *)weed_channel_get_pixel_data(in_channel);
  uint8_t *dest = (uint8_t *)weed_channel_get_pixel_data(out_channel);
  uint8_t *map = sdata->map;
  uint8_t *p, *q;

  int width = weed_channel_get_width(in_channel), hwidth = width >> 1, w3 = width * 3;
  int height = weed_channel_get_height(in_channel);
  int pal = weed_channel_get_palette(in_channel);
  int irow = weed_channel_get_stride(in_channel);
  int orow = weed_channel_get_stride(out_channel);
  short r, g, b, a;
  int x, y;
  int psize = pixel_size(pal);
  int red = 0, green = 1, blue = 2;
  uint8_t r0, r1, r2, r3, g0, g1, g2, g3, b0, b1, b2, b3;

  src += irow;
  dest += orow;

  for (y = 1; y < height - 4; y++) {
    int x6 = 0;
    for (x = 0; x < hwidth; x++, x6 += 6) {
      p = src;
      q = src + psize;

      /* difference between the current pixel and right neighbor. */
      r = p[red] - q[red];
      g = p[green] - q[green];
      b = p[blue] - q[blue];

      r *= r; /* Multiply itself and divide it by 16, instead of */
      g *= g; /* using abs(). */
      b *= b;

      r >>= 4; /* To lack the lower bit for saturated addition,  */
      g >>= 4; /* divide the value by 32, instead of 16. It is */
      b >>= 5; /* the same as `v2 &= 0xfefeff' */

      if (r > 127) r = 127;
      if (g > 127) g = 127;
      if (b > 255) b = 255;

      r2 = map[y * w3 + x6] = r;
      g2 = map[y * w3 + x6 + 1] = g;
      b2 = map[y * w3 + x6 + 2] = b;

      /* difference between the current pixel and upper neighbor. */
      q = src - irow;
      r = p[red] - q[red];
      g = p[green] - q[green];
      b = p[blue] - q[blue];

      r *= r;
      g *= g;
      b *= b;

      r >>= 4;
      g >>= 4;
      b >>= 5;

      if (r > 127) r = 127;
      if (g > 127) g = 127;
      if (b > 255) b = 255;

      r3 = map[y * w3 + x6 + 3] = r;
      g3 = map[y * w3 + x6 + 4] = g;
      b3 = map[y * w3 + x6 + 5] = b;

      r0 = map[(y - 1) * w3 + x6];
      g0 = map[(y - 1) * w3 + x6 + 1];
      b0 = map[(y - 1) * w3 + x6 + 2];

      r1 = map[(y - 1) * w3 + x6 + 3];
      g1 = map[(y - 1) * w3 + x6 + 4];
      b1 = map[(y - 1) * w3 + x6 + 5];

      a = (b0 + b1);
      b2 = a > 255 ? 255 : a;

      g1 = g2 = (g0 + g1) & 0xFF;
      a = (g2 | (g2 - b2)) & 0xFF;
      g2 = a < 0 ? 0 : a & 0xFF;

      a = (r0 + r1);
      r2 = a > 255 ? 255 : a;

      copywalpha(dest, 0, src, 0, r2, g2, b2);

      a = (b0 + b3);
      b2 = a > 255 ? 255 : a;

      g2 = (g0 + g3) & 0xFF;
      a = (g2 | (g2 - b2)) & 0xFF;
      g2 = a < 0 ? 0 : a & 0xFF;

      a = (r0 + r3);
      r2 = a > 255 ? 255 : a;

      copywalpha(dest, psize, src, psize, r2, g2, b2);
      src += psize * 2; // jump 2 pixels
      dest += psize * 2; // jump 2 pixels
    }

    src += irow - width * psize;
    dest += orow - width * psize;
  }

  for (y = 0; y < 2; y++) {
    for (x = 0; x < width; x++) copywalpha(dest++, 0, src, 0, 0, 0, 0);
    dest += orow - width;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("edge detect", "effectTV", 1, WEED_FILTER_PREF_LINEAR_GAMMA,
                               palette_list, edge_init, edge_process, edge_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

