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

/* (C) G. Finch, 2005 - 2022 */

// weed-host-utils.h
// libweed
// (c) G. Finch 2003 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// weed filter utility functions designed for Weed Filter hosts

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef _WEED_HOST_UTILS_H_
#define _WEEED_HOST_UTILS_H_

#ifndef __WEED_HOST__
#error This header is designed for Weed hosts. Plugins have their own utilities in weed-plugin-utils.h
#endif

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-utils.h>
#include <weed/weed-effects.h>
#else
#include "weed.h"
#include "weed-utils.h"
#include "weed-effects.h"
#endif

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#else
#  define WARN_UNUSED
#endif
  
/* general Weed functions and definitions */
int32_t weed_plant_get_type(weed_plant_t *);

#define WEED_PLANT_IS_PLUGIN_INFO(plant) (weed_plant_get_type(plant) == WEED_PLANT_PLUGIN_INFO ? WEED_TRUE : \
					  WEED_FALSE)

#define WEED_PLANT_IS_HOST_INFO(plant) (weed_plant_get_type(plant) == WEED_PLANT_HOST_INFO ? WEED_TRUE : WEED_FALSE)

#define WEED_PLANT_IS_FILTER_CLASS(plant) (weed_plant_get_type(plant) == WEED_PLANT_FILTER_CLASS ? WEED_TRUE : \
					   WEED_FALSE)
#define WEED_PLANT_IS_FILTER_INSTANCE(plant) (weed_plant_get_type(plant) == WEED_PLANT_FILTER_INSTANCE ? WEED_TRUE : \
					      WEED_FALSE)

#define WEED_PLANT_IS_CHANNEL(plant) (weed_plant_get_type(plant) == WEED_PLANT_CHANNEL ? WEED_TRUE : WEED_FALSE)

#define WEED_PLANT_IS_CHANNEL_TEMPLATE(plant) (weed_plant_get_type(plant) == WEED_PLANT_CHANNEL_TEMPLATE ? \
					       WEED_TRUE : WEED_FALSE)

#define WEED_PLANT_IS_PARAMETER(plant) (weed_plant_get_type(plant) == WEED_PLANT_PARAMETER ? WEED_TRUE : WEED_FALSE)

#define WEED_PLANT_IS_PARAMETER_TEMPLATE(plant) (weed_plant_get_type(plant) == WEED_PLANT_PARAMETER_TEMPLATE ? \
						 WEED_TRUE : WEED_FALSE)

#define WEED_PLANT_IS_GUI(plant) (weed_plant_get_type(plant) == WEED_PLANT_GUI ? WEED_TRUE : WEED_FALSE)

#define WEED_ERROR_WRONG_PLANT_TYPE	256
#define WEED_ERROR_NOSUCH_PLANT		257

  // duplicate leaves of src plant to dst. If add == WEED_TRUE, then the original dst leaves will be left or
  // overwritten, otherwisee all original leaves will be romeved, with the exception od any flagged as UNDELETABEL
  // leaves comon to both plants will be ovewritten in dst, with the exception of any flagged as IMMUTABLE
weed_error_t weed_plant_duplicate(weed_plant_t *dst, weed_plant_t *src, int add);

  // permits the 'type' of a plant to be altered, i.e. temporarily removes the IMMUTABLE flag
  // should be used with caution, as the 'type' defines the optional and mandatory leaves
weed_error_t weed_plant_mutate_type(weed_plantptr_t, int32_t newtype);

size_t weed_plant_weigh(weed_plant_t *); // get total size in bytes

// set flags for each leaf in a plant. If ign_prefix is not NULL, ignore leaves with keys that begin with ign_prefix
// this enables a host to do: weed_add_plant_flags(plant, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_")
weed_error_t weed_add_plant_flags(weed_plant_t *, uint32_t flags, const char *ign_prefix);
weed_error_t weed_clear_plant_flags(weed_plant_t *t, uint32_t flags, const char *ign_prefix);

// add / clear bits for leaf flags, returns value of flags before alteration
weed_error_t weed_leaf_set_flagbits(weed_plant_t *, const char *leaf, uint32_t flagbits); ///< value ORed with flags
weed_error_t weed_leaf_clear_flagbits(weed_plant_t *, const char *leaf, uint32_t flagbits); ///< ~value ANDed with flags

// make a whole plant deletable / undeletable
weed_error_t weed_plant_set_undeletable(weed_plant_t *, int undeletable);
int weed_plant_is_undeletable(weed_plant_t *);

/* HOST_INFO functions */
uint32_t weed_host_info_get_flags(weed_plant_t *host_info);
weed_error_t weed_host_info_set_flags(weed_plant_t *host_info, uint32_t flags);
weed_error_t weed_host_set_supports_linear_gamma(weed_plant_t *host_info);
weed_error_t weed_host_set_supports_premult_alpha(weed_plant_t *host_info);
weed_error_t weed_host_set_verbosity(weed_plant_t *host_info, int verbosityy);

/* PLUGIN_INFO functions */
char *weed_plugin_info_get_package_name(weed_plant_t *pinfo);

/* FILTER functions */
uint32_t weed_filter_get_flags(weed_filter_t *);
int weed_filter_is_resizer(weed_filter_t *);
char *weed_filter_get_name(weed_filter_t *);
weed_chantmpl_t **weed_filter_get_in_chantmpls(weed_filter_t *, int *ntmpls);
weed_chantmpl_t **weed_filter_get_out_chantmpls(weed_filter_t *, int *ntmpls);
weed_paramtmpl_t **weed_filter_get_in_paramtmpls(weed_filter_t *, int *ntmpls);
weed_paramtmpl_t **weed_filter_get_out_paramtmpls(weed_filter_t *, int *ntmpls);
weed_gui_t *weed_filter_get_gui(weed_filter_t *, int create_if_not_exists);
weed_plugin_info_t *weed_filter_get_plugin_info(weed_filter_t *);
char *weed_filter_get_package_name(weed_filter_t *);
int weed_filter_hints_unstable(weed_filter_t *);
int weed_filter_hints_hidden(weed_filter_t *);
int weed_filter_hints_stateless(weed_filter_t *);
int weed_filter_is_converter(weed_filter_t *);
int weed_filter_is_process_last(weed_filter_t *);
int weed_filter_prefers_linear_gamma(weed_filter_t *);
int weed_filter_prefers_premult_alpha(weed_filter_t *);
int weed_filter_non_realtime(weed_filter_t *);
int weed_filter_may_thread(weed_filter_t *);
int weed_filter_channel_sizes_vary(weed_filter_t *);
int weed_filter_palettes_vary(weed_filter_t *);

/* FILTER_INSTANCE functions */
weed_channel_t **weed_instance_get_out_channels(weed_instance_t *, int *nchans);
weed_channel_t **weed_instance_get_in_channels(weed_instance_t *, int *nchans);
weed_param_t **weed_instance_get_in_params(weed_instance_t *, int *nparams);
weed_param_t **weed_instance_get_out_params(weed_instance_t *, int *nparams);
weed_gui_t *weed_instance_get_gui(weed_plant_t *inst, int create_if_not_exists);
uint32_t weed_instance_get_flags(weed_instance_t *);
weed_error_t weed_instance_set_flags(weed_instance_t *, uint32_t flags);

/* CHANNEL_TEMPLATE functions */
char *weed_chantmpl_get_name(weed_chantmpl_t *);
uint32_t weed_chantmpl_get_flags(weed_chantmpl_t *);
int weed_chantmpl_is_optional(weed_chantmpl_t *);
int weed_chantmpl_is_audio(weed_chantmpl_t *);
int *weed_chantmpl_get_palette_list(weed_filter_t *, weed_chantmpl_t *, int *nvals);

// audio
int weed_chantmpl_get_max_audio_length(weed_chantmpl_t *);

/* a return value of zero means unlimited repeats */
int weed_chantmpl_get_max_repeats(weed_chantmpl_t *);

/* CHANNEL functions */
void *weed_channel_get_pixel_data(weed_channel_t *);
void **weed_channel_get_pixel_data_planar(weed_channel_t *, int *nplanes);
weed_gui_t *weed_channel_get_gui(weed_channel_t *, int create_if_not_exists);

// returns WEED_TRUE iff width is a multiple of palette macropixel size
int weed_check_width_for_palette(int width, int palette);

/// width in macropixels
int weed_channel_get_width(weed_channel_t *);

/// width in pixels
int weed_channel_get_pixel_width(weed_channel_t *);

int weed_channel_get_height(weed_channel_t *);
int weed_channel_get_palette(weed_channel_t *);
int weed_channel_get_gamma_type(weed_channel_t *);
int weed_channel_get_palette_yuv(weed_channel_t *, int *clamping, int *sampling, int *subspace);
int weed_channel_get_rowstride(weed_channel_t *);
int *weed_channel_get_rowstrides(weed_channel_t *, int *nplanes);
int weed_channel_is_disabled(weed_channel_t *);
weed_chantmpl_t *weed_channel_get_template(weed_channel_t *);

/// width in macropixels
weed_error_t weed_channel_set_width(weed_channel_t *, int width);
weed_error_t weed_channel_set_height(weed_channel_t *, int height);
weed_error_t weed_channel_set_palette(weed_channel_t *, int palette);
weed_error_t weed_channel_set_palette_yuv(weed_channel_t *, int palette, int clamping,
				  int sampling, int subspace);

// for the following 2 functions, if width is not a multiple of palette macropixel size
// WEED_FALSE is returned and no changes are made
// otherwise the channel width is set to size converted to maropixels, and WEED_TRUE is returned

int weed_channel_set_pixel_width(weed_channel_t *, int width);
int weed_channel_set_pixel_size(weed_channel_t *, int width, int height);

weed_error_t weed_channel_set_pixel_data(weed_channel_t *, void *pixel_data);

// for the following 2 functions, if width is not a multiple of new palette macropixel size
// WEED_FALSE is returned and no changes are made
// otherwise the palette is updated, channel width is updated to new palette macropixel size,
// and WEED_TRUE is returned

int weed_channel_update_palette(weed_channel_t *, int palette);
int weed_channel_update_palette_yuv(weed_channel_t *, int palette, int clamping,
				     int sampling, int subspace);

/// only sets value; no conversion of pixel_data done
weed_channel_t *weed_channel_set_gamma_type(weed_channel_t *, int gamma_type);

/// width in pixels: only relevant when comparing widths of different palettes
int weed_channel_get_width_pixels(weed_channel_t *);

// audio channels
int weed_channel_get_audio_rate(weed_channel_t *);
int weed_channel_get_naudchans(weed_channel_t *);
int weed_channel_get_audio_length(weed_channel_t *); // in sampls
float **weed_channel_get_audio_data(weed_channel_t *, int *naudchans);

weed_channel_t *weed_channel_set_audio_data(weed_channel_t *, float **data, int arate, int naudchans, int nsamps);

// paramtmpls
weed_gui_t *weed_paramtmpl_get_gui(weed_paramtmpl_t *, int create_if_not_exists);
uint32_t weed_paramtmpl_get_flags(weed_paramtmpl_t *);
char *weed_paramtmpl_get_name(weed_paramtmpl_t *);
int weed_paramtmpl_get_type(weed_paramtmpl_t *);
int weed_paramtmpl_has_variable_size(weed_paramtmpl_t *);
int weed_paramtmpl_has_value_perchannel(weed_paramtmpl_t *);
weed_seed_t weed_paramtmpl_value_type(weed_paramtmpl_t *);
int weed_paramtmpl_does_wrap(weed_paramtmpl_t *);
int weed_paramtmpl_hints_string_choice(weed_paramtmpl_t *);
int weed_paramtmpl_hints_hidden(weed_paramtmpl_t *);
int weed_paramtmpl_value_irrelevant(weed_paramtmpl_t *);

// params

// if temporary is WEED_TRUE, then we return the current state,
// otherwise we return the permanent (structural) state
int weed_param_is_hidden(weed_param_t *, int temporary);
weed_gui_t *weed_param_get_gui(weed_param_t *, int create_if_not_exists);
weed_paramtmpl_t *weed_param_get_template(weed_param_t *);
int weed_param_get_type(weed_param_t *);
int weed_param_has_variable_size(weed_param_t *);
int weed_param_has_value_perchannel(weed_param_t *);
int weed_param_does_wrap(weed_param_t *);
weed_seed_t weed_param_get_value_type(weed_param_t *);
int weed_param_value_irrelevant(weed_param_t *);

int weed_param_get_value_int(weed_param_t *);
int weed_param_get_value_boolean(weed_param_t *);
double weed_param_get_value_double(weed_param_t *);
float weed_param_get_value_float(weed_param_t *);
int64_t weed_param_get_value_int64(weed_param_t *);
char *weed_param_get_value_string(weed_param_t *);

weed_error_t weed_param_set_value_int(weed_param_t *, int val);
weed_error_t weed_param_set_value_boolean(weed_param_t *, int val);
weed_error_t weed_param_set_value_double(weed_param_t *, double val);
weed_error_t weed_param_set_value_float(weed_param_t *, float val);
weed_error_t weed_param_set_value_int64(weed_param_t *, int64_t val);
weed_error_t weed_param_set_value_string(weed_param_t *, const char *val);

/* if param is WEED_SEED_STRING and WEED_LEAF_CHOICES is set in param or template, returns the length, else returns 0 */
int weed_param_get_nchoices(weed_param_t *);

/// gui plants
uint32_t weed_gui_get_flags(weed_gui_t *gui);

// utils
const char *weed_seed_to_ctype(weed_seed_t st, int add_space);
const char *weed_seed_to_short_text(weed_seed_t seed_type);
const char *weed_seed_to_text(weed_seed_t seed_type);
const char *weed_strerror(weed_error_t error);
const char *weed_error_to_literal(weed_error_t error);
char *weed_palette_get_name_full(int pal, int clamping, int subspace) WARN_UNUSED;
weed_seed_t ctype_to_weed_seed(const char *ctype);

const char *weed_palette_get_name(int pal);
const char *weed_yuv_clamping_get_name(int clamping);
const char *weed_yuv_subspace_get_name(int subspace);
const char *weed_gamma_get_name(int gamma);

double weed_palette_get_compression_ratio(int pal);

#ifndef WEED_ADVANCED_PALETTES

int weed_palette_get_bits_per_macropixel(int pal);

#define weed_palette_is_alpha(pal) ((((pal >= 1024) && (pal < 2048))) ? WEED_TRUE : WEED_FALSE)
#define weed_palette_is_rgb(pal) (pal >0 && pal < 512 ? WEED_TRUE : WEED_FALSE)
#define weed_palette_is_yuv(pal) (pal >= 512 && pal < 1024 ? WEED_TRUE : WEED_FALSE)

#define weed_palette_is_float(pal) ((pal == WEED_PALETTE_RGBAFLOAT || pal == WEED_PALETTE_AFLOAT || \
				     pal == WEED_PALETTE_RGBFLOAT) ? WEED_TRUE : WEED_FALSE)

/// This is actually the MACRO pixel size in bytes, to get the real pixel size, divide by weed_palette_pixels_per_macropixel()
#define pixel_size(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_YUV888) ? 3 : \
			 (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || \
			  pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV) ? 4 : 1)

#define weed_palette_has_alpha(pal) ((pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || \
				      pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUVA8888 || \
				      pal == WEED_PALETTE_RGBAFLOAT || weed_palette_is_alpha(pal)) ? WEED_TRUE : WEED_FALSE)

// return ratio of plane[n] width/plane[0] width
#define weed_palette_get_plane_ratio_horizontal(pal, plane) ((double)(plane == 0 ? 1.0 : (plane == 1 || plane == 2) ? \
								      (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) ? 1.0 : \
								      (pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV420P || \
								       pal == WEED_PALETTE_YVU420P) ? 0.5 : plane == 3 ? \
								      pal == WEED_PALETTE_YUVA4444P ? 1.0 : 0.0 : 0.0 : 0.0))

// return ratio of plane[n] height/plane[n] height
#define weed_palette_get_plane_ratio_vertical(pal, plane) ((double)(plane == 0 ? 1.0 : (plane == 1 || plane == 2) ? \
								    (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P || \
								     pal == WEED_PALETTE_YUV422P) ? 1.0 : (pal == WEED_PALETTE_YUV420P || \
													   pal == WEED_PALETTE_YVU420P) ? 0.5 : plane == 3 ? \
								    pal == WEED_PALETTE_YUVA4444P ? 1.0 : 0.0 : 0.0 : 0.0))

#define weed_palette_get_nplanes(pal) ((pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || \
					pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || \
					pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV || pal == WEED_PALETTE_YUV411 || \
					pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_AFLOAT || \
					pal == WEED_PALETTE_A8 || pal == WEED_PALETTE_A1 || pal == WEED_PALETTE_RGBFLOAT || \
					pal == WEED_PALETTE_RGBAFLOAT) ? 1 : (pal == WEED_PALETTE_YUV420P || \
									      pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || \
									      pal == WEED_PALETTE_YUV444P) ? 3 : pal == WEED_PALETTE_YUVA4444P ? 4 : 0)

#define weed_palette_get_pixels_per_macropixel(pal) ((pal == WEED_PALETTE_UYVY || pal == WEED_PALETTE_YUYV) ? 2 : \
						     (pal == WEED_PALETTE_YUV411) ? 4 : (weed_palette_is_valid(pal) ? 1 : 0))

#define weed_palette_is_valid(pal) (weed_palette_get_nplanes(pal) == 0 ? WEED_FALSE : WEED_TRUE)

#endif

#define weed_palette_is_planar(pal) (weed_palette_get_nplanes(pal) > 1)

  //////////////// plugin support functions ////////////

weed_plant_t *weed_bootstrap(weed_default_getter_f *,
			     int32_t plugin_weed_min_api_version,
			     int32_t plugin_weed_max_api_version,
                             int32_t plugin_filter_min_api_version,
			     int32_t plugin_filter_max_api_version);

/* typedef for host callback from weed_bootstrap; host MUST return a host_info, either the original one or a new one */
typedef weed_plant_t *(*weed_host_info_callback_f)(weed_plant_t *host_info, void *user_data);

/* set a host callback function to be called from within weed_bootstrap() */
weed_error_t weed_set_host_info_callback(weed_host_info_callback_f, void *user_data);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_weed_abi_compat(int32_t higher, int32_t lower);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_filter_api_compat(int32_t higher, int32_t lower);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
