// tvpic.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// simulate a TV display

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static void set_avg(unsigned char *dst, unsigned char *src1, unsigned char *src2, size_t col, size_t alpha, int psize) {
  unsigned char avg = (src1[col] + src2[col]) / 2;
  weed_memset(dst, 0, psize);
  dst[col] = avg;
  if (alpha != -1) dst[alpha] = src1[alpha];
}


static weed_error_t tvpic_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL), *out_channel = weed_get_plantptr_value(inst,
                             WEED_LEAF_OUT_CHANNELS,
                             NULL);

  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dest = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int pal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int psize = (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) ? 3 : 4;
  int offset = 0, dheight = height;
  int odd = 0;
  int rem = width % 6; // modulo 6
  int lbord, rbord;

  size_t red, green, blue, alpha;

  register int x, y, i;

  if (height < 2) return WEED_SUCCESS;

  width *= psize;

  lbord = (rem >> 1) * psize;
  rbord = width - lbord;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int *heights;
    offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    heights = weed_get_int_array(out_channel, WEED_LEAF_HEIGHT, NULL);
    dheight = heights[0];
    height = heights[1];
    dheight += offset;
    weed_free(heights);
    src += offset * irowstride;
    dest += offset * orowstride;
    odd = offset % 2;
  }

  switch (pal) {
  case WEED_PALETTE_RGB24:
    red = 0;
    green = 1;
    blue = 2;
    alpha = -1;
    break;
  case WEED_PALETTE_RGBA32:
    red = 0;
    green = 1;
    blue = 2;
    alpha = 3;
    break;
  case WEED_PALETTE_BGR24:
    blue = 0;
    green = 1;
    red = 2;
    alpha = -1;
    break;
  case WEED_PALETTE_BGRA32:
    blue = 0;
    green = 1;
    red = 2;
    alpha = 4;
    break;
  default:
    // ARGB32
    alpha = 0;
    red = 1;
    green = 2;
    blue = 3;
  }

  for (y = offset; y < dheight; y++) {
    x = 0;
    while (x < width) {
      if (x < lbord || x > rbord) {
        blank_pixel(&dest[x], pal, 0, &src[x]);
        x += psize;
      } else if (y == height - 1) {
        // bottom row, 2 possibilities
        // if odd, rgb from row, rgb from row/row-1
        if (odd) {
          for (i = 0; i < 3; i++) {
            blank_pixel(&dest[x], pal, 0, &src[x]);
            x += psize;
          }
          set_avg(&dest[x], &src[x], &src[x - irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], blue, alpha, psize);
          x += psize;
        } else {
          // if even, rgb from row/row-1, 3 black
          set_avg(&dest[x], &src[x], &src[x - irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], blue, alpha, psize);
          x += psize;
          for (i = 0; i < 3; i++) {
            blank_pixel(&dest[x], pal, 0, &src[x]);
            x += psize;
          }
        }
      } else if (y == 0) {
        // top row has 3 black, rgb from row/row+1, 3 black, etc
        for (i = 0; i < 3; i++) {
          blank_pixel(&dest[x], pal, 0, &src[x]);
          x += psize;
        }
        set_avg(&dest[x], &src[x], &src[x + irowstride], red, alpha, psize);
        x += psize;
        set_avg(&dest[x], &src[x], &src[x + irowstride], green, alpha, psize);
        x += psize;
        set_avg(&dest[x], &src[x], &src[x + irowstride], blue, alpha, psize);
        x += psize;
      } else {
        // normal row, 2 possibilities
        if (odd) {
          // if odd, rgb from row/row+1, rgb from row/row-1
          set_avg(&dest[x], &src[x], &src[x + irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x + irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x + irowstride], blue, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], blue, alpha, psize);
          x += psize;
        } else {
          // if even, rgb from row/row-1, rgb from row/row+1
          set_avg(&dest[x], &src[x], &src[x - irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x - irowstride], blue, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x + irowstride], red, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x + irowstride], green, alpha, psize);
          x += psize;
          set_avg(&dest[x], &src[x], &src[x + irowstride], blue, alpha, psize);
          x += psize;
        }
      }
    }
    dest += orowstride;
    src += irowstride;
    odd = !odd;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("tvpic", "salsaman", 1, 0,
                               NULL, tvpic_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

