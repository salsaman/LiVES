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
   Gabriel "Salsaman" Finch - http://lives.sourceforge.net
   Denis "Jaromil" Rojo - http://freej.dyne.org
   Tom Schouten - http://zwizwa.fartit.com
   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:
   Silvano "Kysucix" Galliani - http://freej.dyne.org
   Kentaro Fukuchi - http://megaui.net/fukuchi
   Jun Iio - http://www.malib.net
   Carlo Prelz - http://www2.fluido.as:8080/
*/

/* (C) G. Finch, 2005 - 2019 */

#ifdef __WEED_HOST__
#error This header is intended only for Weed plugins
#endif

#ifndef __LIBWEED_PLUGIN_UTILS__
#ifndef __WEED_PLUGIN__
#error weed-plugin.h should be included first
#endif
#endif

#ifndef __WEED_PLUGIN_UTILS_H__
#define __WEED_PLUGIN_UTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef __WEED_PLUGIN__
#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "weed.h"
#include "weed-palettes.h"
#include "weed-effects.h"
#include "weed-utils.h"
#endif
#endif

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

#ifndef __LIBWEED_PLUGIN_UTILS__

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#else
#include "weed.h"
#endif

#define ALLOW_UNUSED
#define FN_DECL static

#else // __LIBWEED_PLUGIN_UTILS__

#ifndef ALLOW_UNUSED
#ifdef __GNUC__
#  define ALLOW_UNUSED  __attribute__((unused))
#else
#  define ALLOW_UNUSED
#endif
#endif

#define FN_DECL EXPORTED

#endif


// functions for weed_setup()

FN_DECL weed_plant_t *weed_plugin_info_init(weed_bootstrap_f weed_boot,
    int32_t weed_abi_min_version, int32_t weed_abi_max_version,
    int32_t filter_api_min_version, int32_t weed_filter_api_max_version) ALLOW_UNUSED;

FN_DECL int weed_get_api_version(weed_plant_t *plugin_info) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_channel_template_init(const char *name, int flags, int *palettes) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_audio_channel_template_init(const char *name, int flags) ALLOW_UNUSED;

FN_DECL weed_plant_t **weed_clone_plants(weed_plant_t **plants) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_filter_class_init(const char *name, const char *author, int version, int flags, weed_init_f init_func,
    weed_process_f process_func, weed_deinit_f deinit_func,
    weed_plant_t **in_chantmpls, weed_plant_t **out_chantmpls,
    weed_plant_t **in_paramtmpls, weed_plant_t **out_paramtmpls) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_filter_class_get_gui(weed_plant_t *filter) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_parameter_template_get_gui(weed_plant_t *paramt) ALLOW_UNUSED;

FN_DECL void weed_plugin_info_add_filter_class(weed_plant_t *plugin_info, weed_plant_t *filter_class) ALLOW_UNUSED;


// in params

FN_DECL weed_plant_t *weed_text_init(const char *name, const char *label, const char *def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_float_init(const char *name, const char *label, double def, double min, double max) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_switch_init(const char *name, const char *label, int def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_integer_init(const char *name, const char *label, int def, int min, int max) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_colRGBd_init(const char *name, const char *label, double red, double green, double blue) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_colRGBi_init(const char *name, const char *label, int red, int green, int blue) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_radio_init(const char *name, const char *label, int def, int group) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_string_list_init(const char *name, const char *label, int def, const char **const list) ALLOW_UNUSED;


FN_DECL weed_plant_t *weed_parameter_get_gui(weed_plant_t *param) ALLOW_UNUSED;


// out params

FN_DECL weed_plant_t *weed_out_param_colRGBd_init(const char *name, double red, double green, double blue) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_colRGBi_init(const char *name, int red, int green, int blue) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_text_init(const char *name, const char *def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_float_init_nominmax(const char *name, double def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_float_init(const char *name, double def, double min, double max) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_switch_init(const char *name, int def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_integer_init_nominmax(const char *name, int def) ALLOW_UNUSED;

FN_DECL weed_plant_t *weed_out_param_integer_init(const char *name, int def, int min, int max) ALLOW_UNUSED;

// general utils
FN_DECL int is_big_endian(void);

#ifndef ABS
#define ABS(a)           (((a) < 0) ? -(a) : (a))
#endif

// functions for process_func()

#if defined(NEED_RANDOM) || defined(__LIBWEED_PLUGIN_UTILS__)
FN_DECL uint32_t fastrand(uint32_t oldval);
#endif

#if defined (NEED_ALPHA_SORT) || defined(__LIBWEED_PLUGIN_UTILS__) // for wrappers, use this to sort filters alphabetically
typedef struct dlink_list dlink_list_t;
FN_DECL dlink_list_t *add_to_list_sorted(dlink_list_t *list, weed_plant_t *filter, const char *name);
FN_DECL int add_filters_from_list(weed_plant_t *plugin_info, dlink_list_t *list);
#endif

#if defined(NEED_PALETTE_UTILS) || defined(__LIBWEED_PLUGIN_UTILS__)
FN_DECL int weed_palette_is_alpha(int pal);
FN_DECL int weed_palette_is_rgb(int pal);
FN_DECL int weed_palette_is_yuv(int pal);
FN_DECL int weed_palette_get_numplanes(int pal);
FN_DECL int weed_palette_is_valid(int pal);
FN_DECL int weed_palette_get_bits_per_macropixel(int pal);
FN_DECL int weed_palette_get_pixels_per_macropixel(int pal);
FN_DECL int weed_palette_is_float_palette(int pal);
FN_DECL int weed_palette_has_alpha_channel(int pal);
FN_DECL double weed_palette_get_plane_ratio_horizontal(int pal, int plane);
FN_DECL double weed_palette_get_plane_ratio_vertical(int pal, int plane);

// set src to non-null to preserve the alpha channel (if applicable)
// othwerwise alpha will be set to 255
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
#endif

#if defined(NEED_PALETTE_CONVERSIONS) || defined(__LIBWEED_PLUGIN_UTILS__)
/* palette conversions use lookup tables for efficiency */
// calculate a (rough) luma valu for a pixel; works for any palette (for WEED_PALETTE_UYVY add 1 to *pixel)
FN_DECL uint8_t calc_luma(uint8_t *pixel, int palette, int yuv_clamping);

FN_DECL uint8_t y_unclamped_to_clamped(uint8_t y);
FN_DECL uint8_t y_clamped_to_unclamped(uint8_t y);
FN_DECL uint8_t uv_clamped_to_unclamped(uint8_t uv);

/* pre multiply or un-pre-multiply alpha for a frame: if un is set to WEED_TRUE we un-pre-multiply, othewise pre-multiply */
FN_DECL void alpha_premult(unsigned char *ptr, int width, int height, int rowstride, int pal, int un);
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

#ifdef __cplusplus
}
#endif

#endif
