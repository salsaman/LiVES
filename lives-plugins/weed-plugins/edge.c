// edge.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2015
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-utils.h"
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <string.h>
#include <math.h>

/////////////////////////////////////////////

typedef unsigned int RGB32;
#define PIXEL_SIZE (sizeof(RGB32))

typedef struct {
  RGB32 *map;
} static_data;

//static int video_width_margin;


static int edge_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int error, height, width;

  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_plantptr_value(inst, "in_channels", &error);
  height = weed_get_int_value(in_channel, "height", &error);
  width = weed_get_int_value(in_channel, "width", &error);

  sdata->map = weed_malloc(width * height * PIXEL_SIZE * 2);
  if (sdata->map == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  weed_memset(sdata->map, 0, width * height * PIXEL_SIZE * 2);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static int edge_deinit(weed_plant_t *inst) {
  static_data *sdata;
  int error;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  if (sdata != NULL) {
    weed_free(sdata->map);
    weed_free(sdata);
  }

  return WEED_SUCCESS;
}


static inline RGB32 copywalpha(RGB32 *dest, size_t doffs, RGB32 *src, size_t offs, RGB32 val) {
  // copy alpha from src, and RGB from val; return val
  dest[doffs] = (src[offs] & 0xff000000) | (val & 0xffffff);
  return val;
}


int edge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error), *out_channel = weed_get_plantptr_value(inst,
                             "out_channels",
                             &error);
  RGB32 *src = weed_get_voidptr_value(in_channel, "pixel_data", &error);
  RGB32 *dest = weed_get_voidptr_value(out_channel, "pixel_data", &error), *odest;
  int video_width = weed_get_int_value(in_channel, "width", &error);
  int video_height = weed_get_int_value(in_channel, "height", &error);
  int irow = weed_get_int_value(in_channel, "rowstrides", &error) / 4; // get val in pixels
  int orow = weed_get_int_value(out_channel, "rowstrides", &error) / 4;
  int r, g, b;
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", &error);
  RGB32 *map = sdata->map;
  int map_width = video_width / 2;
  int map_height = video_height;
  register int x, y;
  RGB32 p, q;
  RGB32 v0, v1, v2, v3;

  odest = dest;

  src += irow;
  dest += orow;

  for (y = 1; y < map_height - 4; y++) {
    for (x = 0; x < map_width; x++) {
      p = *src;
      q = *(src + 1);

      /* difference between the current pixel and right neighbor. */
      r = ((int)(p & 0xff0000) - (int)(q & 0xff0000)) >> 16;
      g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00)) >> 8;
      b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
      r *= r; /* Multiply itself and divide it by 16, instead of */
      g *= g; /* using abs(). */
      b *= b;
      r >>= 5; /* To lack the lower bit for saturated addition,  */
      g >>= 5; /* divide the value by 32, instead of 16. It is */
      b >>= 4; /* the same as `v2 &= 0xfefeff' */
      if (r > 127) r = 127;
      if (g > 127) g = 127;
      if (b > 255) b = 255;
      v2 = (r << 17) | (g << 9) | b;

      /* difference between the current pixel and upper neighbor. */
      q = *(src - irow * 2);
      r = ((int)(p & 0xff0000) - (int)(q & 0xff0000)) >> 16;
      g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00)) >> 8;
      b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
      r *= r;
      g *= g;
      b *= b;
      r >>= 5;
      g >>= 5;
      b >>= 4;
      if (r > 127) r = 127;
      if (g > 127) g = 127;
      if (b > 255) b = 255;
      v3 = (r << 17) | (g << 9) | b;

      map[y * video_width + x * 2 + 2] =
        v3;//copywalpha(dest,2,src,2,copywalpha(dest,3,src,3,copywalpha(dest,orow+2,src,irow+2,copywalpha(dest,orow+3,src,
      //											      irow+3,
      //														      v3))));
      map[y * video_width * 2 + x * 2] =
        v2; //copywalpha(dest,orow*2,src,irow*2,copywalpha(dest,orow*2+1,src,irow*2+1,copywalpha(dest,orow*3,src,irow*3,
      //													  copywalpha(dest,orow*3+1,src,irow*3+1,v2))));

      v0 = map[(y - 1) * video_width * 2 + x * 2];
      v1 = map[y * video_width * 2 + x * 2 + 2];

      g = (r = v0 + v1) & 0x01010100;
      copywalpha(dest, 0, src, 0, r | (g - (g >> 8)));
      g = (r = v0 + v3) & 0x01010100;
      copywalpha(dest, 0, src, 1, r | (g - (g >> 8)));
      g = (r = v2 + v1) & 0x01010100;
      copywalpha(dest, orow, src, irow, r | (g - (g >> 8)));
      g = (r = v2 + v3) & 0x01010100;
      copywalpha(dest, orow + 1, src, irow + 1, r | (g - (g >> 8)));

      src += 2; // jump 4 pixels
      dest += 2; // jump 4 pixels
    }

    src += irow - map_width * 2;
    dest += orow - map_width * 2;
  }

  for (y = 0; y < 2; y++) {
    for (x = 0; x < video_width; x++) copywalpha(odest++, 0, src, 0, 0);
    odest += orow - video_width;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGRA32, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("edge detect", "effectTV", 1, WEED_FILTER_HINT_LINEAR_GAMMA,
                               &edge_init, &edge_process, &edge_deinit, in_chantmpls,
                               out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;

