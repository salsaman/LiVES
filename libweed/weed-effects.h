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
#define WEED_PLANT_PLUGIN_INFO 1
#define WEED_PLANT_FILTER_CLASS 2
#define WEED_PLANT_FILTER_INSTANCE 3
#define WEED_PLANT_CHANNEL_TEMPLATE 4
#define WEED_PLANT_PARAMETER_TEMPLATE 5
#define WEED_PLANT_CHANNEL 6
#define WEED_PLANT_PARAMETER 7
#define WEED_PLANT_GUI 8
#define WEED_PLANT_HOST_INFO 255

/* Parameter hints */
#define WEED_HINT_UNSPECIFIED     0
#define WEED_HINT_INTEGER         1
#define WEED_HINT_FLOAT           2
#define WEED_HINT_TEXT            3
#define WEED_HINT_SWITCH          4
#define WEED_HINT_COLOR           5

/* Colorspaces for Color parameters */
#define WEED_COLORSPACE_RGB   1
#define WEED_COLORSPACE_RGBA  2

/* host_info flags */
/* API version 200 */
#define WEED_HOST_SUPPORTS_LINEAR_GAMMA    (1<<0)

/* Filter flags */
#define WEED_FILTER_NON_REALTIME    (1<<0)
#define WEED_FILTER_IS_CONVERTER    (1<<1)
#define WEED_FILTER_HINT_IS_STATELESS (1<<2)

/* API version 200 */
#define WEED_FILTER_HINT_LINEAR_GAMMA (1<<3)

/* API version 131 */
#define WEED_FILTER_PROCESS_LAST (1<<4)

/* API version 132 */
#define WEED_FILTER_HINT_MAY_THREAD (1<<5)

/* Channel template flags */
#define WEED_CHANNEL_REINIT_ON_SIZE_CHANGE    (1<<0)

/* API version 130 */
#define WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE    (1<<6)
#define WEED_CHANNEL_OUT_ALPHA_PREMULT (1<<7)

#define WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE (1<<1)
#define WEED_CHANNEL_CAN_DO_INPLACE           (1<<2)
#define WEED_CHANNEL_SIZE_CAN_VARY            (1<<3)
#define WEED_CHANNEL_PALETTE_CAN_VARY         (1<<4)

/* Channel flags */
#define WEED_CHANNEL_ALPHA_PREMULT (1<<0)

/* Parameter template flags */
#define WEED_PARAMETER_REINIT_ON_VALUE_CHANGE (1<<0)
#define WEED_PARAMETER_VARIABLE_ELEMENTS      (1<<1)

/* API version 110 */
#define WEED_PARAMETER_ELEMENT_PER_CHANNEL    (1<<2)

#define WEED_ERROR_TOO_MANY_INSTANCES 64
#define WEED_ERROR_HARDWARE 65
#define WEED_ERROR_INIT_ERROR 66
#define WEED_ERROR_PLUGIN_INVALID 67

typedef int64_t weed_timecode_t;

typedef void (*weed_function_f)();

// allows the plugin to get the plugin_info before weed_leaf_get() is defined
typedef weed_error_t (*weed_default_getter_f)(weed_plant_t *plant, const char *key, weed_function_f *value);

/* host bootstrap function */
typedef weed_plant_t *(*weed_bootstrap_f)(weed_default_getter_f *, int32_t plugin_weed_min_api_version,
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
typedef void (*weed_display_f)(weed_plant_t *parameter);
typedef weed_error_t (*weed_interpolate_f)(weed_plant_t **in_values, weed_plant_t *out_value);

// leaf names
#define WEED_LEAF_PLUGIN_INFO "plugin_info"
#define WEED_LEAF_FILTERS "filters"
#define WEED_LEAF_MAINTAINER "maintainer"
#define WEED_LEAF_HOST_INFO "host_info"
#define WEED_LEAF_HOST_PLUGIN_NAME "host_plugin_name"
#define WEED_LEAF_PACKAGE_NAME "package_name"

// host info
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
#define WEED_LEAF_CALLOC_FUNC "weed_calloc_func"
#define WEED_LEAF_REALLOC_FUNC "weed_realloc_func"
#define WEED_LEAF_FREE_FUNC "weed_free_func"
#define WEED_LEAF_MEMSET_FUNC "weed_memset_func"
#define WEED_LEAF_MEMCPY_FUNC "weed_memcpy_func"
#define WEED_LEAF_MEMMOVE_FUNC "weed_memmove_func"

// filter_class
#define WEED_LEAF_INIT_FUNC "init_func"
#define WEED_LEAF_DEINIT_FUNC "deinit_func"
#define WEED_LEAF_PROCESS_FUNC "process_func"
#define WEED_LEAF_DISPLAY_FUNC "display_func"
#define WEED_LEAF_INTERPOLATE_FUNC "interpolate_func"
#define WEED_LEAF_TARGET_FPS "target_fps"
#define WEED_LEAF_GUI "gui"
#define WEED_LEAF_DESCRIPTION "description"
#define WEED_LEAF_AUTHOR "author"
#define WEED_LEAF_EXTRA_AUTHORS "extra_authors"
#define WEED_LEAF_URL "url"
#define WEED_LEAF_ICON "icon"
#define WEED_LEAF_LICENSE "license"
#define WEED_LEAF_COPYRIGHT "copyright"
#define WEED_LEAF_VERSION "version"

// instance
#define WEED_LEAF_FILTER_CLASS "filter_class"
#define WEED_LEAF_TIMECODE "timecode"
#define WEED_LEAF_FPS "fps"

// channels / chan template
#define WEED_LEAF_PIXEL_DATA "pixel_data"
#define WEED_LEAF_WIDTH "width"
#define WEED_LEAF_HEIGHT "height"
#define WEED_LEAF_PALETTE_LIST "palette_list"
#define WEED_LEAF_CURRENT_PALETTE "current_palette"
#define WEED_LEAF_ROWSTRIDES "rowstrides"
#define WEED_LEAF_YUV_SUBSPACE "YUV_subspace"
#define WEED_LEAF_YUV_SAMPLING "YUV_sampling"
#define WEED_LEAF_YUV_CLAMPING "YUV_clamping"
#define WEED_LEAF_IN_CHANNELS "in_channels"
#define WEED_LEAF_OUT_CHANNELS "out_channels"
#define WEED_LEAF_IN_CHANNEL_TEMPLATES "in_channel_templates"
#define WEED_LEAF_OUT_CHANNEL_TEMPLATES "out_channel_templates"
#define WEED_LEAF_OFFSET "offset"
#define WEED_LEAF_HSTEP "hstep"
#define WEED_LEAF_VSTEP "vstep"
#define WEED_LEAF_MAXWIDTH "maxwidth"
#define WEED_LEAF_MAXHEIGHT "maxheight"
#define WEED_LEAF_OPTIONAL "optional"
#define WEED_LEAF_DISABLED "disabled"
#define WEED_LEAF_ALIGNMENT "alignment"
#define WEED_LEAF_TEMPLATE "template"
#define WEED_LEAF_PIXEL_ASPECT_RATIO "pixel_aspect_ratio"
#define WEED_LEAF_ROWSTRIDE_ALIGNMENT_HINT "rowstride_alignment_hint"
#define WEED_LEAF_MAX_REPEATS "max_repeats"
#define WEED_LEAF_GAMMA_TYPE "gamma_type"

// params / param tmpl
#define WEED_LEAF_IN_PARAMETERS "in_parameters"
#define WEED_LEAF_OUT_PARAMETERS "out_parameters"
#define WEED_LEAF_VALUE "value"
#define WEED_LEAF_FLAGS "flags"
#define WEED_LEAF_HINT "hint"
#define WEED_LEAF_GROUP "group"
#define WEED_LEAF_NAME "name"
#define WEED_LEAF_DEFAULT "default"
#define WEED_LEAF_MIN "min"
#define WEED_LEAF_MAX "max"
#define WEED_LEAF_IGNORE "ignore"
#define WEED_LEAF_NEW_DEFAULT "new_default"
#define WEED_LEAF_COLORSPACE "colorspace"
#define WEED_LEAF_IN_PARAMETER_TEMPLATES "in_parameter_templates"
#define WEED_LEAF_OUT_PARAMETER_TEMPLATES "out_parameter_templates"
#define WEED_LEAF_TRANSITION "transition"
#define WEED_LEAF_IS_VOLUME_MASTER "is_volume_master"

// audio
#define WEED_LEAF_IS_AUDIO "is_audio"
#define WEED_LEAF_AUDIO_DATA "audio_data"
#define WEED_LEAF_AUDIO_DATA_LENGTH "audio_data_length"
#define WEED_LEAF_AUDIO_RATE "audio_rate"
#define WEED_LEAF_AUDIO_CHANNELS "audio_channels"
#define WEED_LEAF_AUDIO_INTERLEAF "audio_interleaf"

// param gui
#define WEED_LEAF_WRAP "wrap"
#define WEED_LEAF_MAXCHARS "maxchars"
#define WEED_LEAF_LABEL "label"
#define WEED_LEAF_DECIMALS "decimals"
#define WEED_LEAF_STEP_SIZE "step_size"
#define WEED_LEAF_CHOICES "choices"
#define WEED_LEAF_USE_MNEMONIC "use_mnemonic"
#define WEED_LEAF_HIDDEN "hidden"
#define WEED_LEAF_DISPLAY_VALUE "display_value"
#define WEED_LEAF_COPY_VALUE_TO "copy_value_to"

// plugin gui: layout
#define WEED_LEAF_LAYOUT_SCHEME "layout_scheme"
#define WEED_LEAF_RFX_STRINGS "rfx_strings"
#define WEED_LEAF_RFX_DELIM "rfx_delim"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_EFFECTS_H__
