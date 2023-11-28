/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA

   Weed is developed by:
   Gabriel "Salsaman" Finch - http://lives-video.com

   partly based on LiViDO, which is developed by:
   Niels Elburg - http://veejay.sf.net
   Denis "Jaromil" Rojo - http://freej.dyne.org
   Tom Schouten - http://zwizwa.fartit.com
   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:
   Silvano "Kysucix" Galliani - http://freej.dyne.org
   Kentaro Fukuchi - http://megaui.net/fukuchi
   Jun Iio - http://www.malib.net
   Carlo Prelz - http://www2.fluido.as:8080/
*/

/* (C) G. Finch, 2005 - 2023 */

#ifdef __WEED_HOST__
#error This header is intended only for Weed plugins
#endif

#ifndef __WEED_PLUGIN_UTILS_H__
#define __WEED_PLUGIN_UTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-utils.h>
#include <weed/weed-effects.h>

/* Define EXPORTED for any platform */
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
#ifdef WIN_EXPORT
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllexport))
#else
#define EXPORTED __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else // WIN_WEX
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllimport))
#else
#define EXPORTED __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif // WINEX
#define NOT_EXPORTED
#else // MING

#if __GNUC__ >= 4
#define EXPORTED __attribute__ ((visibility ("default")))
#define NOT_EXPORTED  __attribute__ ((visibility ("hidden")))
#else
#define EXPORTED
#define NOT_EXPORTED
#endif
#endif

#ifdef ALLOW_UNUSED
#undef ALLOW_UNUSED
#endif

#ifdef __GNUC__
#define ALLOW_UNUSED __attribute__ ((unused))
#else
#define ALLOW_UNUSED
#endif
#define FN_DECL static

// functions for weed_setup()

FN_DECL weed_plugin_info_t *weed_plugin_info_init(weed_bootstrap_f weed_boot,
    int32_t weed_abi_min_version, int32_t weed_abi_max_version,
    int32_t filter_api_min_version, int32_t weed_filter_api_max_version) ALLOW_UNUSED;

FN_DECL weed_chantmpl_t *weed_channel_template_init(const char *name, int flags) ALLOW_UNUSED;

FN_DECL weed_plant_t **weed_clone_plants(weed_plant_t **plants) ALLOW_UNUSED;

FN_DECL weed_filter_t *weed_filter_class_init(const char *name, const char *author, int version, int flags,
    int *palette_list, weed_init_f init_func,
    weed_process_f process_func, weed_deinit_f deinit_func,
    weed_chantmpl_t **in_chantmpls, weed_chantmpl_t **out_chantmpls,
    weed_paramtmpl_t **in_paramtmpls, weed_paramtmpl_t **out_paramtmpls) ALLOW_UNUSED;

FN_DECL void weed_plugin_info_add_filter_class(weed_plugin_info_t *plugin_info, weed_filter_t *filter_class) ALLOW_UNUSED;

// in params
FN_DECL weed_paramtmpl_t *weed_text_init(const char *name, const char *label, const char *def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_float_init(const char *name, const char *label, double def, double min, double max) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_switch_init(const char *name, const char *label, int def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_integer_init(const char *name, const char *label, int def, int min, int max) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_colRGBd_init(const char *name, const char *label, double red, double green,
                                        double blue) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_colRGBi_init(const char *name, const char *label, int red, int green, int blue) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_radio_init(const char *name, const char *label, int def, int group) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_string_list_init(const char *name, const char *label, int def, const char **const list) ALLOW_UNUSED;

// out params
FN_DECL weed_paramtmpl_t *weed_out_param_colRGBd_init(const char *name, double red, double green, double blue) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_colRGBi_init(const char *name, int red, int green, int blue) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_text_init(const char *name, const char *def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_float_init_nominmax(const char *name, double def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_float_init(const char *name, double def, double min, double max) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_switch_init(const char *name, int def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_integer_init_nominmax(const char *name, int def) ALLOW_UNUSED;
FN_DECL weed_paramtmpl_t *weed_out_param_integer_init(const char *name, int def, int min, int max) ALLOW_UNUSED;

// value setters
FN_DECL weed_error_t weed_plugin_set_package_version(weed_plugin_info_t *plugin_info, int version);
FN_DECL void weed_filter_set_flags(weed_filter_t *filter, int flags);
FN_DECL void weed_chantmpl_set_flags(weed_chantmpl_t *chantmpl, int flags);
FN_DECL void weed_paramtmpl_set_flags(weed_paramtmpl_t *paramtmpl, int flags);
FN_DECL void weed_gui_set_flags(weed_gui_t *gui, int flags);
FN_DECL void weed_filter_set_name(weed_filter_t *filter, const char *name);
FN_DECL void weed_filter_set_description(weed_filter_t *filter, const char *name);
FN_DECL void weed_chantmpl_set_name(weed_chantmpl_t *chantmpl, const char *name);
FN_DECL void weed_paramtmpl_set_name(weed_paramtmpl_t *paramtmpl, const char *name);
FN_DECL weed_error_t weed_paramtmpl_set_hidden(weed_paramtmpl_t *paramtmpl, int hidden);
FN_DECL weed_error_t weed_param_set_hidden(weed_param_t *param, int hidden);
FN_DECL weed_error_t weed_paramtmpl_declare_transition(weed_paramtmpl_t *paramtmpl);
//FN_DECL weed_error_t weed_chantmpl_set_palette_list()

// value getters

// plugin_info
FN_DECL weed_host_info_t *weed_get_host_info(weed_plugin_info_t *plugin_info);
FN_DECL int weed_get_api_version(weed_plugin_info_t *plugin_info) ALLOW_UNUSED;

// host info
FN_DECL int weed_get_host_verbosity(weed_host_info_t *host_info);
//FN_DECL char *weed_get_host_name(weed_plant_t *host_info);
//FN_DECL char *weed_get_host_version(weed_plant_t *host_info);
FN_DECL int weed_host_get_flags(weed_host_info_t *host_info);
FN_DECL int weed_host_supports_linear_gamma(weed_host_info_t *host_info);
FN_DECL int weed_host_supports_premultiplied_alpha(weed_host_info_t *host_info);
//FN_DECL char **weed_get_host_layout_schemes(weed_plant_t *host_info);

// filter_class
FN_DECL int weed_filter_get_flags(weed_filter_t *filter);
FN_DECL int weed_filter_get_version(weed_filter_t *filter);
FN_DECL weed_gui_t *weed_filter_get_gui(weed_filter_t *filter) ALLOW_UNUSED;

// param_tmpl
FN_DECL weed_gui_t *weed_paramtmpl_get_gui(weed_paramtmpl_t *paramtmpl) ALLOW_UNUSED;
FN_DECL int weed_paramtmpl_get_flags(weed_paramtmpl_t *paramtmpl);

// chan tmpl
FN_DECL int weed_chantmpl_get_flags(weed_chantmpl_t *chantmpl);

// inst static data
#define weed_get_instance_data(inst, a) ((typeof((a)))weed_get_voidptr_value((inst), WEED_LEAF_PLUGIN_INFO, NULL))
#define weed_set_instance_data(inst, v) weed_set_voidptr_value((inst), WEED_LEAF_PLUGIN_INFO, (v))

// inst
FN_DECL weed_channel_t *weed_get_in_channel(weed_instance_t *inst, int idx);
FN_DECL weed_channel_t *weed_get_out_channel(weed_instance_t *inst, int idx);
FN_DECL weed_param_t *weed_get_in_param(weed_instance_t *inst, int idx);
FN_DECL weed_param_t *weed_get_out_param(weed_instance_t *inst, int idx);
FN_DECL int weed_instance_get_flags(weed_instance_t *inst);
FN_DECL weed_filter_t *weed_instance_get_filter(weed_instance_t *inst);
FN_DECL weed_gui_t *weed_instance_get_gui(weed_instance_t *inst);

// channel
FN_DECL void *weed_channel_get_pixel_data(weed_channel_t *channel);
FN_DECL int weed_channel_get_width(weed_channel_t *channel);
FN_DECL int weed_channel_get_height(weed_channel_t *channel);
FN_DECL int weed_channel_get_palette(weed_channel_t *channel);
FN_DECL int weed_channel_get_yuv_clamping(weed_channel_t *channel);
FN_DECL int weed_channel_get_stride(weed_channel_t *channel);

#ifdef NEED_AUDIO
FN_DECL weed_chantmpl_t *weed_audio_channel_template_init(const char *name, int flags);
FN_DECL int weed_channel_get_audio_rate(weed_channel_t *channel);
FN_DECL int weed_channel_get_naudchans(weed_channel_t *channel);
FN_DECL int weed_channel_get_audio_length(weed_channel_t *channel);
FN_DECL weed_error_t weed_paramtmpl_declare_volume_master(weed_paramtmpl_t *paramtmpl);
#ifdef __WEED_UTILS_H__
FN_DECL float **weed_channel_get_audio_data(weed_channel_t *channel, int *naudchans);
#endif
#endif

FN_DECL int weed_channel_is_disabled(weed_channel_t *channel);

// params
FN_DECL weed_paramtmpl_t  *weed_param_get_template(weed_param_t *param);
FN_DECL weed_gui_t *weed_param_get_gui(weed_param_t *param) ALLOW_UNUSED;

FN_DECL int weed_gui_get_flags(weed_gui_t *gui);

// param values
FN_DECL int weed_param_has_value(weed_param_t *param);

FN_DECL int weed_param_get_value_int(weed_param_t *param);
FN_DECL int weed_param_get_value_boolean(weed_param_t *param);
FN_DECL double weed_param_get_value_double(weed_param_t *param);
FN_DECL int64_t weed_param_get_value_int64(weed_param_t *param);
FN_DECL char *weed_param_get_value_string(weed_param_t *param);

#ifdef __WEED_UTILS_H__
FN_DECL int *weed_param_get_array_int(weed_param_t *param, int *nvalues);
FN_DECL int *weed_param_get_array_boolean(weed_param_t *param, int *nvalues);
FN_DECL double *weed_param_get_array_double(weed_param_t *param, int *nvalues);
FN_DECL int64_t *weed_param_get_array_int64(weed_param_t *param, int *nvalues);
FN_DECL char **weed_param_get_array_string(weed_param_t *param, int *mvalues);
FN_DECL weed_channel_t **weed_get_in_channels(weed_instance_t *inst, int *nchans);
FN_DECL weed_channel_t **weed_get_out_channels(weed_instance_t *inst, int *nchans);
FN_DECL weed_param_t **weed_get_in_params(weed_instance_t *inst, int *nparams);
FN_DECL weed_param_t **weed_get_out_params(weed_instance_t *inst, int *nparams);
FN_DECL int *weed_channel_get_rowstrides(weed_channel_t *channel, int *nplanes);
FN_DECL void **weed_channel_get_pixel_data_planar(weed_channel_t *channel, int *nplanes);
#endif

/* Threading */
FN_DECL int weed_is_threading(weed_instance_t *inst);
FN_DECL int weed_channel_get_offset(weed_channel_t *channel);
FN_DECL int weed_channel_get_real_height(weed_channel_t *channel);

// general utils
FN_DECL int is_big_endian(void);

#ifndef ABS
#define ABS(a) (((a) < 0) ? -(a) : (a))
#endif

// functions for process_func()

#ifdef NEED_RANDOM
FN_DECL uint64_t fastrnd_int64(void);
FN_DECL double fastrnd_dbl(double range);
FN_DECL uint32_t fastrnd_int(uint32_t range);
FN_DECL uint64_t fastrand(uint64_t seed);
FN_DECL uint64_t fastrand_re(weed_plant_t *inst, const char *leaf);
FN_DECL double fastrand_dbl_re(double range, weed_plant_t *inst, const char *leaf);
FN_DECL uint32_t fastrand_int_re(uint32_t range, weed_plant_t *inst, const char *leaf);
#endif

#ifdef NEED_ALPHA_SORT // for wrappers, use this to sort filters alphabetically
typedef struct dlink_list dlink_list_t;
FN_DECL dlink_list_t *add_to_list_sorted(dlink_list_t *list, weed_plant_t *filter, const char *name);
FN_DECL int add_filters_from_list(weed_plant_t *plugin_info, dlink_list_t *list);
#endif

#ifdef NEED_PALETTE_UTILS
/* only for packed palettes (for uyvy, yuyv, it's actually the macropixel size, but we'll skip that subtlety here)*/
#define pixel_size(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) ? 3 : \
			 (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || \
			  pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV) ? 4 : 0)

#define rgb_offset(pal) (pal == WEED_PALETTE_ARGB32 ? 1 : 0)

FN_DECL int weed_palette_is_alpha(int pal);
FN_DECL int weed_palette_is_rgb(int pal);
FN_DECL int weed_palette_is_yuv(int pal);
FN_DECL int weed_palette_get_nplanes(int pal);
FN_DECL int weed_palette_is_valid(int pal);
FN_DECL int weed_palette_is_float(int pal);
FN_DECL int weed_palette_has_alpha_channel(int pal);
FN_DECL double weed_palette_get_plane_ratio_horizontal(int pal, int plane);
FN_DECL double weed_palette_get_plane_ratio_vertical(int pal, int plane);

// set src to non-null to preserve the alpha channel (if applicable)
// otherwise alpha will be set to 255
// yuv_clamping is ignored fo non-yuv palettes
// only valid for non-planar (packed) palettes: RGB24, BGR24, RGBA32, BGRA32, ARGB32, UYVY8888, YUYV8888, YUV888, YUVA8888, and YUV411
FN_DECL size_t blank_pixel(uint8_t *dst, int pal, int yuv_clamping, uint8_t *src);

// If psrc is non-NULL then the alpha values from psrc will be copied to pdst.
// otherwise alpha will be set to 255
//
// valid for: RGB24, BGR24, RGBA32, BGRA32, ARGB32, UYVY8888, YUYV8888, YUV888, YUVA8888, YUV411
//            YUVA4444p, YUV444p, YUV422p, YUV420p, YVU420p
// if scanning vertically:
// pdst[n], psrc[n] should be increased by rowstrides[n] after each call
//
// for YUV420 and YVU420: set uvcopy to WEED_TRUE only on even rows,
// and increase pdst[1], pdst[2] only on the odd rows (pscr is ignored)
//
FN_DECL void blank_row(uint8_t **pdst, int width, int pal, int yuv_clamping, int uvcopy, uint8_t **psrc);
FN_DECL void blank_frame(void **pdata, int width, int height, int *rowstrides, int pal, int yuv_clamping);
#endif

#ifdef NEED_PALETTE_CONVERSIONS

#include <math.h>

#define L1_CONST .00885645168f
#define L2_CONST .03296296296f
#define L3_CONST .33333333333f
#define L4_CONST .16f

#define LIGHTNESS(y)((y)<L1_CONST?(y)*L2_CONST:pow((y),L3_CONST)*(1.+L4_CONST)-L4_CONST)

typedef struct {float offs, lin, thresh, pf;} gamma_const_t;

#define INIT_GAMMA(gtype) gamma_tx[gtype##_IDX] = (gamma_const_t) {0., GAMMA_CONSTS_##gtype}; \
  gamma_tx[gtype##_IDX].offs = (powf((gamma_tx[gtype##_IDX].thresh / gamma_tx[gtype##_IDX].lin), \
				     (1. / gamma_tx[gtype##_IDX].pf)) - gamma_tx[gtype##_IDX].thresh) \
    / (1. - (powf((gamma_tx[gtype##_IDX].thresh / gamma_tx[gtype##_IDX].lin), \
		  (1. / gamma_tx[gtype##_IDX].pf)))); gamma_idx[gtype##_IDX] = gtype;

#define GAMMA_CONSTS_WEED_GAMMA_SRGB 12.92,0.04045,2.4
#define GAMMA_CONSTS_WEED_GAMMA_BT709 4.5,0.018,1./.45

enum {WEED_GAMMA_SRGB_IDX, WEED_GAMMA_BT709_IDX, N_GAMMA_TYPES};

static gamma_const_t gamma_tx[N_GAMMA_TYPES];
static int gamma_idx[N_GAMMA_TYPES], gamma_inited = 0;

/* palette conversions use lookup tables for efficiency */
// calculate a (rough) luma valu for a pixel; works for any palette (for WEED_PALETTE_UYVY add 1 to *pixel)
FN_DECL uint8_t calc_luma(uint8_t *pixel, int palette, int yuv_clamping);

FN_DECL void convert_gamma(uint8_t *dst_rgb, uint8_t *src_rgb, int gamma_from, int gamma_to);

#define light_percent(pixel, palette, yuv_clamping) (LIGHTNESS(calc_luma((pixel), (palette), (yuv_clamping))))

FN_DECL uint8_t y_unclamped_to_clamped(uint8_t y);
FN_DECL uint8_t y_clamped_to_unclamped(uint8_t y);
FN_DECL uint8_t uv_clamped_to_unclamped(uint8_t uv);

/* pre multiply or un-pre-multiply alpha for a frame: if un is set to WEED_TRUE we un-pre-multiply, otherwise pre-multiply */
FN_DECL void alpha_premult(unsigned char *ptr, int width, int height, int rowstride, int pal, int un);
#endif

#ifdef NEED_FONT_UTILS
#include <wchar.h>
FN_DECL void weed_parse_font_string(const char *fontstr, char **family, char **fstretch,
				    char **fweight, char **fstyle, int *size);
#endif

#ifdef __cplusplus

#define WEED_SETUP_START(weed_api_version, filter_api_version) extern "C" { EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_version, weed_api_version, filter_api_version, filter_api_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_START_MINMAX(weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version) extern "C" { EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_min_version, weed_api_max_version, filter_api_min_Version, filter_api_max_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_END } return plugin_info;}}

#define WEED_DESETUP_START extern "C" { EXPORTED void weed_desetup(void) {
#define WEED_DESETUP_END }}

#else

#define WEED_SETUP_START(weed_api_version, filter_api_version) EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_version, weed_api_version, filter_api_version, filter_api_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_START_MINMAX(weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version) EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_END } return plugin_info;}

#define WEED_DESETUP_START EXPORTED void weed_desetup(void) {
#define WEED_DESETUP_END }

#endif

#ifndef NO_PLUGIN_UTILS_CODE
#include <weed/weed-plugin-utils/weed-plugin-utils.c>
#endif

#ifdef __cplusplus
}
#endif

#endif
