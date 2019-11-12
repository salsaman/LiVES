// comic.c
// weed plugin
// (c) G. Finch (salsaman) 2010
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// thanks to Chris Yates for the idea

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <inttypes.h>

static uint32_t sqrti(uint32_t n) {
  register uint32_t root = 0, remainder = n, place = 0x40000000, tmp;

  while (place > remainder) place >>= 2;
  while (place) {
    if (remainder >= (tmp = (root + place))) {
      remainder -= tmp;
      root += (place << 1);
    }
    root >>= 1;
    place >>= 2;
  }
  return root;
}


static void cp_chroma(unsigned char *dst, unsigned char *src, int irowstride, int orowstride, int width, int height) {
  if (irowstride == orowstride && irowstride == width) weed_memcpy(dst, src, width * height);
  else {
    register int i;
    for (i = 0; i < height; i++) {
      weed_memcpy(dst, src, width);
      src += irowstride;
      dst += orowstride;
    }
  }
}


static weed_error_t comic_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  uint8_t **srcp = (uint8_t **)weed_get_voidptr_array(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  uint8_t **dstp = (uint8_t **)weed_get_voidptr_array(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int *irowstrides = weed_get_int_array(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int *orowstrides = weed_get_int_array(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int clamping = weed_get_int_value(in_channel, WEED_LEAF_YUV_CLAMPING, NULL);
  int irowstride, orowstride;

  uint8_t *src, *dst, *end;
  int row0, row1, sum, scale = 384, mix = 192;
  int yinv, ymin, ymax, nplanes;
  register int i;

  // get the Y planes
  src = srcp[0];
  dst = dstp[0];

  irowstride = irowstrides[0];
  orowstride = orowstrides[0];

  // skip top scanline
  weed_memcpy(dst, src, width);

  src += irowstride;
  dst += orowstride;

  // calc end
  end = src + (height - 2) * irowstride;

  // dst remainder after copying width
  orowstride -= width;

  if (clamping == WEED_YUV_CLAMPING_UNCLAMPED) {
    yinv = ymax = 255;
    ymin = 0;
    ymax = 255;
  } else {
    yinv = 251;
    ymin = 16;
    ymax = 235;
  }

  // skip rightmost pixel
  width--;

  // process each row
  for (; src < end; src += irowstride - width) {
    // skip leftmost pixel
    *(dst++) = *src;
    src++;

    // process all pixels except leftmost and rightmost
    for (i = 1; i < width; i++) {
      // do edge detect and convolve
      row0 = (*(src + irowstride - 1) - * (src - irowstride - 1)) + ((*(src + irowstride) - * (src - irowstride)) << 1) + (*
             (src + irowstride + 1) - *
             (src + irowstride - 1));
      row1 = (*(src - irowstride + 1) - * (src - irowstride - 1)) + ((*(src + 1) - * (src - 1)) << 1) + (*(src + irowstride + 1) + *
             (src + irowstride - 1));

      sum = ((3 * sqrti(row0 * row0 + row1 * row1) / 2) * scale) >> 8;

      // clamp and invert
      sum = yinv - (sum < ymin ? ymin : sum > ymax ? ymax : sum);

      // mix 25% effected with 75% original
      sum = ((256 - mix) * sum + mix * (*src)) >> 8;

      *(dst++) = (uint8_t)(sum = (sum < ymin ? ymin : sum > ymax ? ymax : sum));
      src++;
    }

    // skip rightmost pixel
    *(dst++) = *src;
    src++;

    // dst to next row
    dst += orowstride;
  }

  width++;

  // copy bottom row
  weed_memcpy(dst, src, width);

  if (palette == WEED_PALETTE_YUV420P || palette == WEED_PALETTE_YVU420P) height >>= 1;
  if (palette == WEED_PALETTE_YUV420P || palette == WEED_PALETTE_YVU420P || palette == WEED_PALETTE_YUV422P) width >>= 1;

  if (palette == WEED_PALETTE_YUVA4444P) nplanes = 4;
  else nplanes = 3;

  for (i = 1; i < nplanes; i++) {
    cp_chroma(dstp[i], srcp[i], irowstrides[i], orowstrides[i], width, height);
  }

  weed_free(srcp);
  weed_free(dstp);
  weed_free(irowstrides);
  weed_free(orowstrides);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_YUV444P, WEED_PALETTE_YUVA4444P, WEED_PALETTE_YUV422P,
                        WEED_PALETTE_YUV420P, WEED_PALETTE_YVU420P, WEED_PALETTE_END
                       };

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0, palette_list), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("comicbook", "salsaman", 1, 0, NULL, &comic_process,
                               NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  // set preference of unclamped
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_YUV_CLAMPING, WEED_YUV_CLAMPING_UNCLAMPED);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;
