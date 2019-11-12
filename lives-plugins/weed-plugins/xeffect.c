// xeffect.c
// livido plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS
#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

static inline void make_white(unsigned char *pixel) {
  pixel[0] = pixel[1] = pixel[2] = (unsigned char)255;
}

static inline void nine_fill(unsigned char *new_data, int row, unsigned char *o) {
  // fill nine pixels with the centre colour
  new_data[-row - 3] = new_data[-row] = new_data[-row + 3] = new_data[-3] = new_data[0] =
                                          new_data[3] = new_data[row - 3] = new_data[row] = new_data[row + 3] = o[0];
  new_data[-row - 2] = new_data[-row + 1] = new_data[-row + 4] = new_data[-2] = new_data[1] =
                         new_data[4] = new_data[row - 2] = new_data[row + 1] = new_data[row + 4] = o[1];
  new_data[-row - 1] = new_data[-row + 2] = new_data[-row + 5] = new_data[-1] = new_data[2] =
                         new_data[5] = new_data[row - 1] = new_data[row + 2] = new_data[row + 5] = o[2];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

static weed_error_t xeffect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL) * 3;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  unsigned char *end = src + height * irowstride - irowstride;

  unsigned int myluma;
  unsigned int threshold = 10000;
  int nbr;

  register int i, j, k;

  src += irowstride;
  dst += orowstride;
  width -= 4;

  for (; src < end; src += irowstride) {
    for (i = 3; i < width; i += 3) {
      myluma = calc_luma(&src[i], palette, 0);
      nbr = 0;
      for (j = -irowstride; j <= irowstride; j += irowstride) {
        for (k = -3; k < 4; k += 3) {
          if ((j != 0 || k != 0) && ABS(calc_luma(&src[j + i + k], palette, 0) - myluma) > threshold) nbr++;
        }
      }
      if (nbr < 2 || nbr > 5) {
        nine_fill(&dst[i], orowstride, &src[i]);
      } else {
        if (myluma < 12500) {
          blank_pixel(&dst[i], palette, 0, NULL);
        } else {
          if (myluma > 20000) {
            make_white(&dst[i]);
          }
        }
      }
    }
    dst += orowstride;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("graphic novel", "salsaman", 1, 0, NULL, &xeffect_process, NULL,
                               in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

