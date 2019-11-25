// weed-effects-utils.c
// - will probaly become libweed-effects-utils or something
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// weed filter utility functions

int weed_filter_get_flags(weed_plant_t *filter);

int weed_chantmpl_get_flags(weed_plant_t *chantmpl);
int weed_chantmpl_is_optional(weed_plant_t *chantmpl);
int *weed_chantmpl_get_palette_list(weed_plant_t *filter, weed_plant_t *chantmpl, weed_size_t *nvals);

void *weed_channel_get_pixel_data(weed_plant_t *channel);
int weed_channel_get_width(weed_plant_t *channel);
int weed_channel_get_height(weed_plant_t *channel);
int weed_channel_get_palette(weed_plant_t *channel);
int weed_channel_get_rowstride(weed_plant_t *channel);
int *weed_channel_get_rowstrides(weed_plant_t *channel);
weed_plant_t *weed_channel_get_template(weed_plant_t *channel);

char *weed_seed_type_to_text(int32_t seed_type);
char *weed_error_to_text(weed_error_t error);
char *weed_palette_get_name_full(int pal, int clamping, int subspace);

const char *weed_palette_get_name(int pal);
const char *weed_yuv_clamping_get_name(int clamping);
const char *weed_yuv_subspace_get_name(int subspace);

int weed_palette_get_bits_per_macropixel(int pal);
double weed_palette_get_compression_ratio(int pal);

#define weed_palette_is_alpha(pal) ((((pal >= 1024) && (pal < 2048))) ? TRUE : FALSE)
#define weed_palette_is_rgb(pal) (pal < 512 ? TRUE : FALSE)
#define weed_palette_is_yuv(pal) (pal >= 512 && pal < 1024 ? TRUE : FALSE)
#define weed_palette_is_valid(pal) (weed_palette_get_nplanes(pal) == 0 ? FALSE : TRUE)

#define weed_palette_get_pixels_per_macropixel(pal) ((pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888) ? 2 : (pal == WEED_PALETTE_YUV411) ? 4 : 1)

#define weed_palette_is_float(pal) ((pal == WEED_PALETTE_RGBAFLOAT || pal == WEED_PALETTE_AFLOAT || pal == WEED_PALETTE_RGBFLOAT) ? TRUE : FALSE)

#define weed_palette_has_alpha_channel(pal) ((pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_RGBAFLOAT || weed_palette_is_alpha(pal)) ? TRUE : FALSE)

// return ratio of plane[n] width/plane[0] width
#define weed_palette_get_plane_ratio_horizontal(pal, plane) ((double)(plane == 0 ? 1.0 : (plane == 1 || plane == 2) ? (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) ? 1.0 : (pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) ? 0.5 : plane == 3 ? pal == WEED_PALETTE_YUVA4444P ? 1.0 : 0.0 : 0.0 : 0.0))

// return ratio of plane[n] height/plane[n] height
#define weed_palette_get_plane_ratio_vertical(pal, plane) ((double)(plane == 0 ? 1.0 : (plane == 1 || plane == 2) ? (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUV422P) ? 1.0 : (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) ? 0.5 : plane == 3 ? pal == WEED_PALETTE_YUVA4444P ? 1.0 : 0.0 : 0.0 : 0.0))

#define weed_palette_get_nplanes(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV411 || pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_AFLOAT || pal == WEED_PALETTE_A8 || pal == WEED_PALETTE_A1 || pal == WEED_PALETTE_RGBFLOAT || pal == WEED_PALETTE_RGBAFLOAT) ? 1 : (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P) ? 3 : pal == WEED_PALETTE_YUVA4444P ? 4 : 0)

