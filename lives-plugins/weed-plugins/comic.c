/////////////////////////////////////////////////////////////////////////////
// Weed comic plugin, version 1
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by salsaman
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////
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

#include "weed-plugin-utils.c"

//////////////////////////////////////////////////////////////////

#include <inttypes.h>

static int verbosity = WEED_VERBOSITY_ERROR;

static uint32_t sqrti(uint32_t n) {
  uint32_t root = 0, remainder = n, place = 0x40000000, tmp;
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


static void cp_chroma(unsigned char *dst, unsigned char *src, int irow,
      int orow, int width, int height) {
  if (irow == orow && irow == width) weed_memcpy(dst, src, width * height);
  else {
    for (int i = 0; i < height; i++) {
      weed_memcpy(&dst[orow * i], &src[irow * i], width);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////

static weed_error_t comic_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  unsigned char **srcp = (unsigned char **)weed_channel_get_pixel_data_planar(in_chan, NULL);
  unsigned char **dstp = (unsigned char **)weed_channel_get_pixel_data_planar(out_chan, NULL);
  int pal = weed_channel_get_palette(in_chan);
  int *irows = weed_channel_get_rowstrides(in_chan, NULL);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int *orows = weed_channel_get_rowstrides(out_chan, NULL);
  int psize = pixel_size(pal);

  if (1) {
    int clamping = weed_channel_get_yuv_clamping(in_chan);
    int row0, row1, sum, scale = 384;
    unsigned char *src = srcp[0];
    unsigned char *dst = dstp[0];
    
    int irow = irows[0];
    int orow = orows[0];
    int ymin, ymax, nplanes, j;
    
    unsigned char *end = src + (height - 2) * irow;
    
    if (clamping == WEED_YUV_CLAMPING_UNCLAMPED) {
      ymin = 0;
      ymax = 255;
    } else {
      ymin = 16;
      ymax = 235;
    }
    
    // skip top scanline
    weed_memcpy(dst, src, width);
    
    // skip rightmost pixel
    width--;
    
    // process each row
    for (int i = 1; i < height; i++) {
      // skip leftmost pixel
      dst[orow * i] = src[irow * i];
    
      // process all pixels except leftmost and rightmost
      for (j = 1; j < width; j++) {
        // do edge detect and convolve
        row0 = src[irow * (i + 1) - 1 + j] - src[irow * (i - 1) - 1 + j]
              + ((src[irow * (i + 1) + j] - src[irow * (i - 1) + j]) << 1)
              + src[irow * (i + 1) + 1 + j] - src[irow * (i + 1)- 1 + j];
        row1 = src[irow * (i - 1) + 1 + j] - src[irow * (i - 1) - 1 + j]
              + ((src[irow * i + 1 + j] - src[irow * i - 1 + j]) << 1)
              + src[irow * (i + 1) + 1 + j] - src[irow * (i + 1)- 1 + j];
    
        sum = ((3 * sqrti(row0 * row0 + row1 * row1) / 2) * scale) >> 8;
    
        // clamp and invert
        sum = 255 - (sum < 0 ? 0 : sum > 255 ? 255 : sum);
    
        // mix 25% effected with 75% original
        sum = (64 * sum + 192 * (*src)) >> 8;
        if (clamping == WEED_YUV_CLAMPING_CLAMPED) sum = (double)sum / 255. * 219. + 16.;
    
        dst[orow * i + j] = (uint8_t)(sum < ymin ? ymin : sum > ymax ? ymax : sum);
      }
    
      // skip rightmost pixel
      dst[orow * i + j] = src[irow * i +j];
    }
    
    width++;
    
    // copy bottom row
    weed_memcpy(dst, src, width);
    
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) height >>= 1;
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P
          || pal == WEED_PALETTE_YUV422P) width >>= 1;
    
    if (pal == WEED_PALETTE_YUVA4444P) nplanes = 4;
    else nplanes = 3;
    
    for (int i = 1; i < nplanes; i++) {
      cp_chroma(dstp[i], srcp[i], irows[i], orows[i], width, height);
    }
  }
    weed_free(srcp);
    weed_free(dstp);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  int palette_list[] = ALL_PLANAR_PALETTES;
  weed_plant_t *in_chantmpls[] = {
      weed_channel_template_init("in_channel0", 0),
      NULL};
  weed_plant_t *out_chantmpls[] = {
      weed_channel_template_init("out_channel", 0),
      NULL};
  int filter_flags = 0;

  verbosity = weed_get_host_verbosity(host_info);

  filter_class = weed_filter_class_init("comic book", "salsaman", 1, filter_flags, palette_list,
    NULL, comic_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

