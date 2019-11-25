// weed-effects-utils.c
// - will probaly become libweed-effects-utils or something
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// weed filter utility functions

#include "main.h"
#include "support.h"

LIVES_GLOBAL_INLINE void *weed_channel_get_pixel_data(weed_plant_t *channel) {
  if (channel == NULL) return NULL;
  return weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);
}

LIVES_GLOBAL_INLINE int weed_channel_get_width(weed_plant_t *channel) {
  if (channel == NULL) return 0;
  return weed_get_int_value(channel, WEED_LEAF_WIDTH, NULL);
}

LIVES_GLOBAL_INLINE int weed_channel_get_height(weed_plant_t *channel) {
  if (channel == NULL) return 0;
  return weed_get_int_value(channel, WEED_LEAF_HEIGHT, NULL);
}

LIVES_GLOBAL_INLINE int weed_channel_get_palette(weed_plant_t *channel) {
  if (channel == NULL) return WEED_PALETTE_END;
  return weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, NULL);
}

LIVES_GLOBAL_INLINE int weed_channel_get_rowstride(weed_plant_t *channel) {
  if (channel == NULL) return 0;
  return weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, NULL);
}

LIVES_GLOBAL_INLINE int weed_filter_get_flags(weed_plant_t *filter) {
  if (filter == NULL) return 0;
  return weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
}

LIVES_GLOBAL_INLINE int weed_chantmpl_get_flags(weed_plant_t *chantmpl) {
  if (chantmpl == NULL) return 0;
  return weed_get_int_value(chantmpl, WEED_LEAF_FLAGS, NULL);
}

LIVES_GLOBAL_INLINE int weed_chantmpl_is_optional(weed_plant_t *chantmpl) {
  if (chantmpl == NULL) return WEED_TRUE;
  if (weed_chantmpl_get_flags(chantmpl)  & WEED_CHANNEL_OPTIONAL) return WEED_TRUE;
  return WEED_FALSE;
}

char *weed_error_to_text(weed_error_t error) {
  // return value should be freed after use
  switch (error) {
  case (WEED_ERROR_MEMORY_ALLOCATION):
    return strdup("Memory allocation error");
  /* case (WEED_ERROR_CONCURRENCY): */
  /*   return strdup("Thread concurrency error"); */
  case (WEED_ERROR_IMMUTABLE):
    return strdup("Read only property");
  case (WEED_ERROR_UNDELETABLE):
    return strdup("Undeletable property");
  case (WEED_ERROR_BADVERSION):
    return strdup("Bad version number");
  case (WEED_ERROR_NOSUCH_ELEMENT):
    return strdup("Invalid element");
  case (WEED_ERROR_NOSUCH_LEAF):
    return strdup("Invalid property");
  case (WEED_ERROR_WRONG_SEED_TYPE):
    return strdup("Incorrect property type");
  case (WEED_ERROR_TOO_MANY_INSTANCES):
    return strdup("Too many instances");
  case (WEED_ERROR_PLUGIN_INVALID):
    return strdup("Fatal plugin error");
  case (WEED_ERROR_FILTER_INVALID):
    return strdup("Invalid filter in plugin");
  case (WEED_ERROR_REINIT_NEEDED):
    return strdup("Filter needs reiniting");
  default:
    break;
  }
  return strdup("No error");
}

char *weed_seed_type_to_text(int32_t seed_type) {
  switch (seed_type) {
  case WEED_SEED_INT:
    return strdup("integer");
  case WEED_SEED_INT64:
    return strdup("int64");
  case WEED_SEED_BOOLEAN:
    return strdup("boolean");
  case WEED_SEED_DOUBLE:
    return strdup("double");
  case WEED_SEED_STRING:
    return strdup("string");
  case WEED_SEED_FUNCPTR:
    return strdup("function pointer");
  case WEED_SEED_VOIDPTR:
    return strdup("void *");
  case WEED_SEED_PLANTPTR:
    return strdup("weed_plant_t *");
  default:
    return strdup("custom pointer type");
  }
}

const char *weed_palette_get_name(int pal) {
  switch (pal) {
  case WEED_PALETTE_RGB24:
    return "RGB24";
  case WEED_PALETTE_RGBA32:
    return "RGBA32";
  case WEED_PALETTE_BGR24:
    return "BGR24";
  case WEED_PALETTE_BGRA32:
    return "BGRA32";
  case WEED_PALETTE_ARGB32:
    return "ARGB32";
  case WEED_PALETTE_RGBFLOAT:
    return "RGBFLOAT";
  case WEED_PALETTE_RGBAFLOAT:
    return "RGBAFLOAT";
  case WEED_PALETTE_YUV888:
    return "YUV888";
  case WEED_PALETTE_YUVA8888:
    return "YUVA8888";
  case WEED_PALETTE_YUV444P:
    return "YUV444P";
  case WEED_PALETTE_YUVA4444P:
    return "YUVA4444P";
  case WEED_PALETTE_YUV422P:
    return "YUV4422P";
  case WEED_PALETTE_YUV420P:
    return "YUV420P";
  case WEED_PALETTE_YVU420P:
    return "YVU420P";
  case WEED_PALETTE_YUV411:
    return "YUV411";
  case WEED_PALETTE_UYVY8888:
    return "UYVY";
  case WEED_PALETTE_YUYV8888:
    return "YUYV";
  case WEED_PALETTE_A8:
    return "8 BIT ALPHA";
  case WEED_PALETTE_A1:
    return "1 BIT ALPHA";
  case WEED_PALETTE_AFLOAT:
    return "FLOAT ALPHA";
  default:
    if (pal >= 2048) return "custom";
    return "unknown";
  }
}

const char *weed_yuv_clamping_get_name(int clamping) {
  if (clamping == WEED_YUV_CLAMPING_UNCLAMPED) return "unclamped";
  if (clamping == WEED_YUV_CLAMPING_CLAMPED) return "clamped";
  return NULL;
}

const char *weed_yuv_subspace_get_name(int subspace) {
  if (subspace == WEED_YUV_SUBSPACE_YUV) return "Y'UV";
  if (subspace == WEED_YUV_SUBSPACE_YCBCR) return "Y'CbCr";
  if (subspace == WEED_YUV_SUBSPACE_BT709) return "BT.709";
  return NULL;
}

char *weed_palette_get_name_full(int pal, int clamping, int subspace) {
  const char *pname = weed_palette_get_name(pal);

  if (!weed_palette_is_yuv(pal)) return lives_strdup(pname);
  else {
    const char *clamp = weed_yuv_clamping_get_name(clamping);
    const char *sspace = weed_yuv_subspace_get_name(subspace);
    return lives_strdup_printf("%s:%s (%s)", pname, sspace, clamp);
  }
}

const char *weed_gamma_get_name(int gamma) {
  if (gamma == WEED_GAMMA_LINEAR) return "linear";
  if (gamma == WEED_GAMMA_SRGB) return "sRGB";
  if (gamma == WEED_GAMMA_BT709) return "bt709";
  return "unknown";
}

LIVES_GLOBAL_INLINE int weed_palette_get_bits_per_macropixel(int pal) {
  if (pal == WEED_PALETTE_A8 || pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P ||
      pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) return 8;
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) return 24;
  if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 ||
      pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888)
    return 32;
  if (pal == WEED_PALETTE_YUV411) return 48;
  if (pal == WEED_PALETTE_AFLOAT) return sizeof(float);
  if (pal == WEED_PALETTE_A1) return 1;
  if (pal == WEED_PALETTE_RGBFLOAT) return (3 * sizeof(float));
  if (pal == WEED_PALETTE_RGBAFLOAT) return (4 * sizeof(float));
  return 0; // unknown palette
}

// just as an example: 1.0 == RGB(A), 0.5 == compressed to 50%, etc

double weed_palette_get_compression_ratio(int pal) {
  double tbits = 0.;

  int nplanes = weed_palette_get_nplanes(pal);
  int pbits;

  register int i;

  if (!weed_palette_is_valid(pal)) return 0.;
  if (weed_palette_is_alpha(pal)) return 0.; // invalid for alpha palettes
  for (i = 0; i < nplanes; i++) {
    pbits = weed_palette_get_bits_per_macropixel(pal) / weed_palette_get_pixels_per_macropixel(pal);
    tbits += pbits * weed_palette_get_plane_ratio_vertical(pal, i) * weed_palette_get_plane_ratio_horizontal(pal, i);
  }
  if (weed_palette_has_alpha_channel(pal)) return tbits / 32.;
  return tbits / 24.;
}

