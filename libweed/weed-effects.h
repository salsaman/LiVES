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

/* (C) G. Finch, 2005 - 2019 */

#ifndef __WEED_EFFECTS_H__
#define __WEED_EFFECTS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <inttypes.h>

/* API version * 200 */
#define WEED_FILTER_API_VERSION 200
#define WEED_FILTER_API_VERSION 200

/* plant types */
#define WEED_PLANT_PLUGIN_INFO        		1
#define WEED_PLANT_FILTER_CLASS       		2
#define WEED_PLANT_FILTER_INSTANCE     		3
#define WEED_PLANT_CHANNEL_TEMPLATE     	4
#define WEED_PLANT_PARAMETER_TEMPLATE   	5
#define WEED_PLANT_CHANNEL                     	6
#define WEED_PLANT_PARAMETER                  	7
#define WEED_PLANT_GUI                        	8
#define WEED_PLANT_HOST_INFO                   	9

/* Parameter types */
#define WEED_PARAM_UNSPECIFIED	  	0
#define WEED_PARAM_INTEGER        	1
#define WEED_PARAM_FLOAT           	2
#define WEED_PARAM_TEXT             	3
#define WEED_PARAM_SWITCH          	4
#define WEED_PARAM_COLOR           	5

typedef struct {
  int32_t red, green, blue;
} weed_rgb_int_t;

typedef struct {
  int32_t red, green, blue, alpha;
} weed_rgba_int_t;

typedef struct {
  double red, green, blue;
} weed_rgb_double_t;

typedef struct {
  double red, green, blue, alpha;
} weed_rgba_double_t;

/* Colorspaces for Color parameters */
#define WEED_COLORSPACE_RGB   	1
#define WEED_COLORSPACE_RGBA  	2

/* host_info flags */
/* API version 200 */
#define WEED_HOST_SUPPORTS_LINEAR_GAMMA    		(1 << 0)
#define WEED_HOST_SUPPORTS_PREMULTIPLIED_ALPHA    	(1 << 1)

/* Filter flags */
#define WEED_FILTER_NON_REALTIME                	(1 << 0)
#define WEED_FILTER_IS_CONVERTER                	(1 << 1)
#define WEED_FILTER_HINT_STATELESS			(1 << 2)
#define WEED_FILTER_PREF_LINEAR_GAMMA			(1 << 3)
#define WEED_FILTER_PREF_PREMULTIPLIED_ALPHA		(1 << 4)
#define WEED_FILTER_HINT_PROCESS_LAST			(1 << 5)
#define WEED_FILTER_HINT_MAY_THREAD                    	(1 << 6)
#define WEED_FILTER_HINT_MAYBE_UNSTABLE                	(1 << 7)
#define WEED_FILTER_CHANNEL_SIZES_MAY_VARY     		(1 << 8)
#define WEED_FILTER_PALETTES_MAY_VARY			(1 << 9)

/* audio */
#define WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY		(1 << 15)
#define WEED_FILTER_AUDIO_RATES_MAY_VARY	 	(1 << 16)

/* Channel template flags */
#define WEED_CHANNEL_REINIT_ON_SIZE_CHANGE		(1 << 0)
#define WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE          	(1 << 1)
#define WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE    	(1 << 2)
#define WEED_CHANNEL_OPTIONAL                          	(1 << 3)
#define WEED_CHANNEL_CAN_DO_INPLACE                    	(1 << 4)
#define WEED_CHANNEL_NEEDS_NATURAL_SIZE                	(1 << 5)

/* audio */
#define WEED_CHANNEL_REINIT_ON_RATE_CHANGE	WEED_CHANNEL_REINIT_ON_SIZE_CHANGE
#define WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE	WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE

/* Parameter template flags */
#define WEED_PARAMETER_REINIT_ON_VALUE_CHANGE     	(1 << 0)
#define WEED_PARAMETER_VARIABLE_SIZE                   	(1 << 1)
#define WEED_PARAMETER_VALUE_PER_CHANNEL              	(1 << 2)
#define WEED_PARAMETER_VALUE_IRRELEVANT			(1 << 3)

/* Parameter template GUI flags */
#define WEED_GUI_REINIT_ON_VALUE_CHANGE		(1 << 0)
#define WEED_GUI_CHOICES_SET_ON_INIT	      	(1 << 1)

/* filter instance flags */
#define WEED_INSTANCE_UPDATE_GUI_ONLY		(1 << 0)

/* error codes (in addidion to WEED_SUCCESS and WEED_ERROR_MEMORY_ALLOCATION) */
#define WEED_ERROR_PLUGIN_INVALID              	64
#define WEED_ERROR_FILTER_INVALID		65
#define WEED_ERROR_TOO_MANY_INSTANCES		66
#define WEED_ERROR_REINIT_NEEDED               	67
#define WEED_ERROR_NOT_READY	               	68

#define WEED_VERBOSITY_SILENT	 	-2 ///< no output
#define WEED_VERBOSITY_CRITICAL		-1 ///< only critical errors which prevent the plugin / filter from operating AT ALL
#define WEED_VERBOSITY_ERROR	      	 0 ///< default choice a (errors which prevent normal operation)
#define WEED_VERBOSITY_WARN	      	 1 ///< default choice b (errors which adversly affect operation)
#define WEED_VERBOSITY_INFO		 2 ///< info (any additional non-debug info)
#define WEED_VERBOSITY_DEBUG		 3 ///< output to assist with debugging the plugin / filter

typedef int64_t weed_timecode_t;

// allows the plugin to get the plugin_info before weed_leaf_get() is defined
typedef weed_error_t (*weed_default_getter_f)(weed_plant_t *plant, const char *key, void *value);

/* host bootstrap function */
typedef weed_plant_t *(*weed_bootstrap_f)(weed_default_getter_f *,
    int32_t plugin_weed_min_api_version,
    int32_t plugin_weed_max_api_version,
    int32_t plugin_filter_min_api_version,
    int32_t plugin_filter_max_api_version);

/* mandatory plugin functions */
typedef weed_plant_t *(*weed_setup_f)(weed_bootstrap_f weed_boot);

/* optional plugin functions */
typedef void (*weed_desetup_f)(void);
typedef weed_error_t (*weed_process_f)(weed_plant_t *filter_instance, weed_timecode_t timestamp);
typedef weed_error_t (*weed_init_f)(weed_plant_t *filter_instance);
typedef weed_error_t (*weed_deinit_f)(weed_plant_t *filter_instance);

/* special plugin functions */
typedef void (*weed_display_f)(weed_plant_t *parameter); // deprecated
typedef weed_error_t (*weed_interpolate_f)(weed_plant_t **in_values, weed_plant_t *out_value);

// PLUGIN_INFO
// mandatory:
#define WEED_LEAF_FILTERS "filters"
#define WEED_LEAF_HOST_INFO "host_info"
#define WEED_LEAF_VERSION "version"

// optional
#define WEED_LEAF_PACKAGE_NAME "package_name"
#define WEED_LEAF_MAINTAINER "maintainer"
#define WEED_LEAF_URL "url"
#define WEED_LEAF_DESCRIPTION "description"
#define WEED_LEAF_ERROR_NUMBER "error_number"
#define WEED_LEAF_ERROR_TEXT "error_text"

// HOST_INFO
// mandatory:
#define WEED_LEAF_FILTER_API_VERSION "filter_api_version"
#define WEED_LEAF_GET_FUNC "weed_leaf_get_func"
#define WEED_LEAF_SET_FUNC "weed_leaf_set_func"
#define WEED_LEAF_DELETE_FUNC "weed_leaf_delete_func"
#define WEED_PLANT_NEW_FUNC "weed_plant_new_func"
#define WEED_PLANT_FREE_FUNC "weed_plant_free_func"
#define WEED_PLANT_LIST_LEAVES_FUNC "weed_plant_list_leaves_func"
#define WEED_LEAF_NUM_ELEMENTS_FUNC "weed_leaf_num_elements_func"
#define WEED_LEAF_ELEMENT_SIZE_FUNC "weed_leaf_element_size_func"
#define WEED_LEAF_SEED_TYPE_FUNC "weed_leaf_seed_type_func"
#define WEED_LEAF_GET_FLAGS_FUNC "weed_leaf_get_flags_func"
#define WEED_LEAF_MALLOC_FUNC "weed_malloc_func"
#define WEED_LEAF_FREE_FUNC "weed_free_func"
#define WEED_LEAF_MEMSET_FUNC "weed_memset_func"
#define WEED_LEAF_MEMCPY_FUNC "weed_memcpy_func"
#define WEED_LEAF_MEMMOVE_FUNC "weed_memmove_func"
#define WEED_LEAF_CALLOC_FUNC "weed_calloc_func"
#define WEED_LEAF_REALLOC_FUNC "weed_realloc_func"

// optional //
#define WEED_LEAF_HOST_NAME "host_name"
#define WEED_LEAF_HOST_VERSION "host_version"
#define WEED_LEAF_FLAGS "flags"
#define WEED_LEAF_VERBOSITY "verbosity"
#define WEED_LEAF_LAYOUT_SCHEMES "layout_schemes"
#define WEED_LEAF_PLUGIN_INFO "plugin_info"

// FILTER_CLASS
// mandatory
#define WEED_LEAF_NAME "name"
#define WEED_LEAF_AUTHOR "author"
/* also WEED_LEAF_VERSION */

/* mandatory for filters with video, unless overridden in channel templates */
#define WEED_LEAF_PALETTE_LIST "palette_list"

// optional
#define WEED_LEAF_INIT_FUNC "init_func"
#define WEED_LEAF_DEINIT_FUNC "deinit_func"
#define WEED_LEAF_PROCESS_FUNC "process_func"
#define WEED_LEAF_IN_PARAMETER_TEMPLATES "in_param_tmpls"
#define WEED_LEAF_OUT_PARAMETER_TEMPLATES "out_param_tmpls"
#define WEED_LEAF_IN_CHANNEL_TEMPLATES "in_chan_tmpls"
#define WEED_LEAF_OUT_CHANNEL_TEMPLATES "out_chan_tmpls"
#define WEED_LEAF_GUI "gui"
#define WEED_LEAF_EXTRA_AUTHORS "extra_authors"
#define WEED_LEAF_MICRO_VERSION "micro_version"
#define WEED_LEAF_ICON "icon"
#define WEED_LEAF_LICENSE "license"
#define WEED_LEAF_COPYRIGHT "copyright"

/* optional for filters with video channels */
#define WEED_LEAF_PREFERRED_FPS "target_fps"
#define WEED_LEAF_HSTEP "hstep"
#define WEED_LEAF_VSTEP "vstep"
#define WEED_LEAF_ALIGNMENT_HINT "alignment_hint"

/* optional for filters with video channels (may be overriden in channel templates depending on filter_class flags) */
#define WEED_LEAF_WIDTH "width"
#define WEED_LEAF_HEIGHT "height"
#define WEED_LEAF_MAXWIDTH "maxwidth"
#define WEED_LEAF_MAXHEIGHT "maxheight"
#define WEED_LEAF_MINWIDTH "minwidth"
#define WEED_LEAF_MINHEIGHT "minheight"
#define WEED_LEAF_YUV_CLAMPING "YUV_clamping"
#define WEED_LEAF_YUV_SAMPLING "YUV_sampling"
#define WEED_LEAF_YUV_SUBSPACE "YUV_subspace"
#define WEED_LEAF_NATURAL_SIZE "natural_size"

///
/* optional for filters with audio channels (maybe overriden in channel templates depending on filter_class flags) */
#define WEED_LEAF_AUDIO_RATE "audio_rate"
#define WEED_LEAF_MAX_AUDIO_CHANNELS "max_audio_chans"
#define WEED_LEAF_MIN_AUDIO_LENGTH "min_audio_len"
#define WEED_LEAF_MAX_AUDIO_LENGTH "max_audio_len"
#define WEED_LEAF_CHANNEL_LAYOUTS "channel_layouts"  /// only if set in filter_class or channel_template

/* audio channel layouts (default settings) */
#ifndef  WEED_CHANNEL_LAYOUT_TYPE
#define WEED_CHANNEL_LAYOUT_TYPE "default"
#define WEED_CH_FRONT_LEFT 0x00000001
#define WEED_CH_FRONT_RIGHT 0x00000002
#define WEED_CH_FRONT_CENTER 0x00000004
#define WEED_CH_LAYOUT_MONO (WEED_CH_FRONT_CENTER)
#define WEED_CH_LAYOUT_STEREO (WEED_CH_FRONT_LEFT | WEED_CH_FRONT_RIGHT)
#define WEED_CH_LAYOUT_DEFAULT_1 WEED_CH_LAYOUT_MONO
#define WEED_CH_LAYOUT_DEFAULT_2 WEED_CH_LAYOUT_STEREO
#define WEED_CH_LAYOUTS_DEFAULT (WEED_CH_LAYOUT_DEFAULT_2, WEED_CH_LAYOUT_DEFAULT_1}
#define WEED_CH_LAYOUTS_DEFAULT_MIN2 (WEED_CH_LAYOUT_DEFAULT_2}
#endif

// FILTER_CLASS GUI
#define WEED_LEAF_LAYOUT_SCHEME "layout_scheme"


// FILTER_INSTANCE
// mandatory
#define WEED_LEAF_FILTER_CLASS "filter_class"
#define WEED_LEAF_IN_PARAMETERS "in_parameters"
#define WEED_LEAF_OUT_PARAMETERS "out_parameters"
#define WEED_LEAF_IN_CHANNELS "in_channels"
#define WEED_LEAF_OUT_CHANNELS "out_channels"
  // optional
#define WEED_LEAF_FPS "fps"
#define WEED_LEAF_TARGET_FPS "target_fps"

// instance GUI leaves
#define WEED_LEAF_EASE_IN_FRAMES "ease_in_frames" // set by filter in init_func()
#define WEED_LEAF_EASE_OUT_FRAMES "ease_out_frames" // set by filter in process_func()

#define WEED_LEAF_EASE_IN "ease_in" // host request, set prior to first process_func() call
#define WEED_LEAF_EASE_OUT "ease_out" // host request, set prior to andy process_func() call


// CHANNEL_TEMPLATE
// mandatory
/// WEED_LEAF_NAME

// mandatory for audio
#define WEED_LEAF_IS_AUDIO "is_audio"

//optional
/// WEED_LEAF_FLAGS
/// WEED_LEAF_DESCRIPTION
#define WEED_LEAF_MAX_REPEATS "max_repeats"
/// WEED_LEAF_PALETTE_LIST


// CHANNEL
// mandatory
#define WEED_LEAF_TEMPLATE "template"
/// WEED_LEAF_FLAGS

/* mandatory for VIDEO */
/// WEED_LEAF_WIDTH
/// WEED_LEAF_HEIGHT
#define WEED_LEAF_PIXEL_DATA "pixel_data"
#define WEED_LEAF_CURRENT_PALETTE "current_palette"
#define WEED_LEAF_ROWSTRIDES "rowstrides"

// mandatory for AUDIO
#define WEED_LEAF_AUDIO_DATA "audio_data"
#define WEED_LEAF_AUDIO_DATA_LENGTH "audio_data_len"
#define WEED_LEAF_AUDIO_RATE "audio_rate"
#define WEED_LEAF_AUDIO_CHANNELS "audio_channels"
//#define WEED_LEAF_AUDIO_CHANNEL_LAYOUT "channel_layout"  /// ONLY if set in filter_class or channel_template

// optional
#define WEED_LEAF_OFFSET "offset" ///< threading
#define WEED_LEAF_DISABLED "disabled"

/// optional for VIDEO
/// WEED_LEAF_YUV_CLAMPING
/// WEED_LEAF_YUV_SAMPLING
/// WEED_LEAF_YUV_SUBSPACE
#define WEED_LEAF_PIXEL_ASPECT_RATIO "par"
#define WEED_LEAF_GAMMA_TYPE "gamma_type"
#define WEED_LEAF_ALPHA_PREMULTIPLIED "alpha_premult"


// PARAM_TEMPLATE
// mandatory
/// WEED_LEAF_NAME
#define WEED_LEAF_DEFAULT "default"
#define WEED_LEAF_MIN "min"  /// Mand. for in chans, Opt. for out
#define WEED_LEAF_MAX "max"  /// M for in chans, O for out
#define WEED_LEAF_PARAM_TYPE "param_type"

//optional
/// WEED_LEAF_FLAGS
/// WEED_LEAF_GUI
/// WEED_LEAF_DESCRIPTION
#define WEED_LEAF_NEW_DEFAULT "new_default"
#define WEED_LEAF_GROUP "group"
#define WEED_LEAF_COLORSPACE "colorspace"
#define WEED_LEAF_IS_TRANSITION "is_transition"
#define WEED_LEAF_IS_VOLUME_MASTER "is_vol_master"

// PARAM_TEMPLATE GUI
#define WEED_LEAF_WRAP "wrap"
#define WEED_LEAF_MAXCHARS "maxchars"
#define WEED_LEAF_LABEL "label"
#define WEED_LEAF_DECIMALS "decimals"
#define WEED_LEAF_STEP_SIZE "step_size"
#define WEED_LEAF_USE_MNEMONIC "use_mnemonic"
#define WEED_LEAF_CHOICES "choices"
#define WEED_LEAF_CHOICES_LANGUAGES "choices_langs"
#define WEED_LEAF_HIDDEN "hidden"
#define WEED_LEAF_COPY_VALUE_TO "copy_value_to"

// PARAM
// mandatory
#define WEED_LEAF_VALUE "value"

// PARAM_GUI
#define WEED_LEAF_CHOICES_LANGUAGE "choices_lang"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_EFFECTS_H__
