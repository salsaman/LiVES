// deinterlace.c
// weed plugin
// (c) G. Finch (salsaman) 2006 - 2008
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
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>


static inline unsigned char *mix(unsigned char *a, unsigned char *b, int pcpy) {
  unsigned char *mixed = (unsigned char *)weed_malloc(pcpy);
  register int i;
  for (i = 0; i < pcpy; i++) {
    mixed[i] = (*(a + i) + * (b + i)) >> 1;
  }
  return mixed;
}


static weed_error_t  deinterlace_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);

  unsigned char **src_array = (unsigned char **)weed_channel_get_pixel_data_planar(in_channel, NULL);
  unsigned char **dst_array = (unsigned char **)weed_channel_get_pixel_data_planar(out_channel, NULL);

  int inplace = (src == dst);

  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int palette = weed_channel_get_palette(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel), orowstride2 = orowstride * 2;
  int *irowstrides = weed_channel_get_rowstrides(in_channel, NULL);
  int *orowstrides = weed_channel_get_rowstrides(out_channel, NULL);

  int x;

  unsigned char *val1a, *val2a, *val3a, *val4a;
  unsigned char *val2b, *val3b, *val4b;
  unsigned char *val1c, *val2c, *val3c, *val4c;
  unsigned char *res1, *res2, *res3, *res4, *res5, *res6;

  unsigned char *val2a_u = NULL, *val3a_u = NULL, *val4a_u = NULL;
  unsigned char *val2b_u = NULL, *val3b_u = NULL, *val4b_u = NULL;
  unsigned char *val2c_u = NULL, *val3c_u = NULL, *val4c_u = NULL;
  unsigned char *res1_u = NULL, *res2_u = NULL, *res3_u = NULL, *res4_u = NULL, *res5_u = NULL, *res6_u = NULL;

  unsigned char *val2a_v = NULL, *val3a_v = NULL, *val4a_v = NULL;
  unsigned char *val2b_v = NULL, *val3b_v = NULL, *val4b_v = NULL;
  unsigned char *val2c_v = NULL, *val3c_v = NULL, *val4c_v = NULL;
  unsigned char *res1_v = NULL, *res2_v = NULL, *res3_v = NULL, *res4_v = NULL, *res5_v = NULL, *res6_v = NULL;

  int d1, d2;
  unsigned char m1, m2, m3, m4;

  int irowstride2 = irowstride * 2;
  unsigned char *end = src + height * irowstride - irowstride2;

  int psize = 3, psize2, psize3, pcpy;
  int widthx;

  int green_offs = 0;

  psize = pixel_size(palette);

  if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32 ||
      palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) green_offs = 1;
  if (palette == WEED_PALETTE_ARGB32) green_offs = 2;

  psize2 = psize * 2;
  psize3 = psize * 3;

  widthx = width * psize;

  src += irowstride;
  dst += orowstride;

  if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
    for (int i = 1; i < 3; i++) {
      dst_array[i] += orowstrides[i];
      src_array[i] += irowstrides[i];
    }
  }

  pcpy = psize;

  if (palette == WEED_PALETTE_ARGB32 || palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32 ||
      palette == WEED_PALETTE_YUVA8888) pcpy = 3;

  for (; src < end ; src += irowstride2) {
    for (x = 0; x < widthx; x += psize3) {

      if (!inplace && palette == WEED_PALETTE_ARGB32) {
        // copy alpha packed
        dst[x] = src[x];
        dst[x + 4] = src[x + 4];
        dst[x + 8] = src[x + 8];
        x++;
      }

      val1a = (src - irowstride + x);
      val2a = (src + x);
      val3a = (src + irowstride + x);
      val4a = (src + irowstride2 + x);

      val2b = (src + x + psize);

      val1c = (src - irowstride + x + psize2);
      val2c = (src + x + psize2);
      val3c = (src + irowstride + x + psize2);
      val4c = (src + irowstride2 + x + psize2);

      res1 = val2a;
      res3 = val2b;
      res5 = val2c;

      if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
        // u and v planes
        val2a_u = (src_array[1] + x);
        val3a_u = (src_array[1] + irowstrides[1] + x);
        val4a_u = (src_array[1] + irowstrides[1] * 2 + x);

        val2b_u = (src_array[1] + x + psize);

        val2c_u = (src_array[1] + x + psize2);
        val3c_u = (src_array[1] + irowstrides[1] + x + psize2);
        val4c_u = (src_array[1] + irowstrides[1] * 2 + x + psize2);

        res1_u = val2a_u;
        res3_u = val2b_u;
        res5_u = val2c_u;

        val3a_v = (src_array[2] + irowstrides[2] + x);
        val4a_v = (src_array[2] + irowstrides[2] * 2 + x);

        val2b_v = (src_array[2] + x + psize);

        val2c_v = (src_array[2] + x + psize2);
        val3c_v = (src_array[2] + irowstrides[2] + x + psize2);
        val4c_v = (src_array[2] + irowstrides[2] * 2 + x + psize2);

        res1_v = val2a_v;
        res3_v = val2b_v;
        res5_v = val2c_v;
      }

      if (palette == WEED_PALETTE_UYVY8888) {
        //average 4 y values
        m1 = (*(val1a + 1) + * (val1a + 3) + * (val1c + 1) + * (val1c + 3)) >> 2;
        m2 = (*(val3a + 1) + * (val3a + 3) + * (val3c + 1) + * (val3c + 3)) >> 2;
        m3 = (*(val2a + 1) + * (val2a + 3) + * (val2c + 1) + * (val2c + 3)) >> 2;
        m4 = (*(val4a + 1) + * (val4a + 3) + * (val4c + 1) + * (val4c + 3)) >> 2;
      } else if (palette == WEED_PALETTE_YUYV8888) {
        // average 4 y values
        m1 = (*(val1a) + * (val1a + 2) + * (val1c) + * (val1c + 2)) >> 2;
        m2 = (*(val3a) + * (val3a + 2) + * (val3c) + * (val3c + 2)) >> 2;
        m3 = (*(val2a) + * (val2a + 2) + * (val2c) + * (val2c + 2)) >> 2;
        m4 = (*(val4a) + * (val4a + 2) + * (val4c) + * (val4c + 2)) >> 2;
      } else {
        // average 2 green values
        m1 = (*(val1a + green_offs) + * (val1c + green_offs)) >> 1; // -1 row
        m2 = (*(val3a + green_offs) + * (val3c + green_offs)) >> 1; // +1 row
        m3 = (*(val2a + green_offs) + * (val2c + green_offs)) >> 1; // +0 row
        m4 = (*(val4a + green_offs) + * (val4c + green_offs)) >> 1; // +2 row
      }

      d1 = abs(m1 - m2) + abs(m3 - m4); // diff (-1,+1) + diff (0,+2)
      d2 = abs(m1 - m4) + abs(m3 - m2); // diff (-1,+2) + diff (0,+1)

      if ((d1) < (d2)) {// alternate rows differ more than consecutive rows
        val4b = (src + irowstride2 + x + psize);
        val2b = (src + x + psize);

        res2 = mix(val2a, val4a, pcpy);
        res4 = mix(val2b, val4b, pcpy);
        res6 = mix(val2c, val4c, pcpy);

        if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
          // apply to u and v planes
          val4b_u = (src_array[1] + irowstrides[1] * 2 + x + psize);
          val2b_u = (src_array[1] + x + psize);

          res2_u = mix(val2a_u, val4a_u, pcpy);
          res4_u = mix(val2b_u, val4b_u, pcpy);
          res6_u = mix(val2c_u, val4c_u, pcpy);

          val4b_v = (src_array[2] + irowstrides[2] * 2 + x + psize);
          val2b_v = (src_array[2] + x + psize);

          res2_v = mix(val2a_v, val4a_v, pcpy);
          res4_v = mix(val2b_v, val4b_v, pcpy);
          res6_v = mix(val2c_v, val4c_v, pcpy);
        }
      } else {
        val3b = (src + irowstride + x + psize);

        res2 = val3a;
        res4 = val3b;
        res6 = val3c;

        if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
          val3b_u = (src_array[1] + irowstrides[1] + x + psize);

          res2_u = val3a_u;
          res4_u = val3b_u;
          res6_u = val3c_u;

          val3b_v = (src_array[2] + irowstrides[2] + x + psize);

          res2_v = val3a_v;
          res4_v = val3b_v;
          res6_v = val3c_v;
        }
      }

      weed_memcpy(dst + x - orowstride, res1, pcpy);
      weed_memcpy(dst + x, res2, pcpy);
      weed_memcpy(dst + x + psize - orowstride, res3, pcpy);
      weed_memcpy(dst + x + psize, res4, pcpy);
      weed_memcpy(dst + x + psize2 - orowstride, res5, pcpy);
      weed_memcpy(dst + x + psize2, res6, pcpy);

      if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
        // u and v planes

        weed_memcpy(dst_array[1] + x - orowstrides[1], res1_u, pcpy);
        weed_memcpy(dst_array[1] + x, res2_u, pcpy);
        weed_memcpy(dst_array[1] + x + psize - orowstrides[1], res3_u, pcpy);
        weed_memcpy(dst_array[1] + x + psize, res4_u, pcpy);
        weed_memcpy(dst_array[1] + x + psize2 - orowstrides[1], res5_u, pcpy);
        weed_memcpy(dst_array[1] + x + psize2, res6_u, pcpy);

        weed_memcpy(dst_array[2] + x - orowstrides[2], res1_v, pcpy);
        weed_memcpy(dst_array[2] + x, res2_v, pcpy);
        weed_memcpy(dst_array[2] + x + psize - orowstrides[2], res3_v, pcpy);
        weed_memcpy(dst_array[2] + x + psize, res4_v, pcpy);
        weed_memcpy(dst_array[2] + x + psize2 - orowstrides[2], res5_v, pcpy);
        weed_memcpy(dst_array[2] + x + psize2, res6_v, pcpy);
      }

      if (!inplace && (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32
                       || palette == WEED_PALETTE_YUVA8888)) {
        // copy alpha packed
        dst[x + 3] = src[x + 3];
        dst[x + 7] = src[x + 7];
        dst[x + 11] = src[x + 11];
      }

      if ((d1) < (d2)) {
        weed_free(res2);
        weed_free(res4);
        weed_free(res6);
        if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
          weed_free(res2_u);
          weed_free(res4_u);
          weed_free(res6_u);
          weed_free(res2_v);
          weed_free(res4_v);
          weed_free(res6_v);
        }
      }
    }

    if (palette == WEED_PALETTE_YUV444P || palette == WEED_PALETTE_YUVA4444P) {
      dst_array[1] += orowstrides[1] * 2;
      dst_array[2] += orowstrides[2] * 2;
    }
    dst += orowstride2;
  }

  if (!inplace) {
    if (palette == WEED_PALETTE_YUV422P || palette == WEED_PALETTE_YUV420P || palette == WEED_PALETTE_YVU420P) {
      // copy chroma planes
      weed_memcpy(dst_array[1], src_array[1], orowstrides[1] * height >> (palette == WEED_PALETTE_YUV422P ? 0 : 1));
      weed_memcpy(dst_array[2], src_array[2], orowstrides[2] * height >> (palette == WEED_PALETTE_YUV422P ? 0 : 1));

    } else if (palette == WEED_PALETTE_YUVA4444P) {
      // copy alpha plane
      weed_memcpy(dst_array[3], src_array[3], orowstrides[3] * height);
    }
  }

  weed_free(src_array);
  weed_free(dst_array);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_YUV888, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_YUVA8888, WEED_PALETTE_UYVY, WEED_PALETTE_YUYV, WEED_PALETTE_YUV444P, WEED_PALETTE_YUVA4444P, WEED_PALETTE_YUV420P, WEED_PALETTE_YVU420P, WEED_PALETTE_YUV422P, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("deinterlace", "salsaman", 1, 0, palette_list,
                               NULL, deinterlace_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

