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

// weed-host-utils.c
// libweed
// (c) G. Finch 2003 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// weed filter utility functions designed for Weed Filter hosts

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define __LIBWEED__
#define __WEED__HOST__

#if NEED_LOCAL_WEED
#include "../libweed/weed-host.h"
#include "../libweed/weed.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-host-utils.h"
#else
#include <weed/weed-host.h>
#include <weed/weed.h>
#if NEED_LOCAL_WEED_UTILS
#include "../libweed/weed-utils.h"
#else
#include <weed/weed-utils.h>
#endif
#if NEED_LOCAL_WEED_HOST_UTILS
#include "../libweed/weed-host-utils.h"
#else
#include <weed/weed-host-utils.h>
#endif
#include <weed/weed-effects.h>
#include <weed/weed-palettes.h>
#endif

#ifndef WEED_GLOBAL_INLINE
#define WEED_GLOBAL_INLINE inline
#endif

#ifndef WEED_LOCAL_INLINE
#define WEED_LOCAL_INLINE static inline
#endif

WEED_GLOBAL_INLINE int32_t weed_plant_get_type(weed_plant_t *plant) {
  if (!plant) return WEED_PLANT_UNKNOWN;
  return weed_get_int_value(plant, WEED_LEAF_TYPE, NULL);
}

WEED_GLOBAL_INLINE uint32_t weed_leaf_set_flagbits(weed_plant_t *plant, const char *leaf, uint32_t flagbits) {
  uint32_t flags = 0;
  if (plant) {
    flags = weed_leaf_get_flags(plant, leaf);
    weed_leaf_set_flags(plant, leaf, flags | flagbits);
  }
  return flags;
}

WEED_GLOBAL_INLINE uint32_t weed_leaf_clear_flagbits(weed_plant_t *plant, const char *leaf, uint32_t flagbits) {
  uint32_t flags = 0;
  if (plant) {
    flags = weed_leaf_get_flags(plant, leaf);
    weed_leaf_set_flags(plant, leaf, flags & ~flagbits);
  }
  return flags;
}

void weed_add_plant_flags(weed_plant_t *plant, uint32_t flags, const char *ign_prefix) {
  if (!plant) return;
  else {
    size_t ign_prefix_len = 0;
    char **leaves = weed_plant_list_leaves(plant, NULL);
    if (leaves) {
      if (ign_prefix) ign_prefix_len = strlen(ign_prefix);
      for (int i = 0; leaves[i]; i++) {
        if (!ign_prefix || strncmp(leaves[i], ign_prefix, ign_prefix_len)) {
          weed_leaf_set_flagbits(plant, leaves[i], flags);
        }
        free(leaves[i]);
      }
      free(leaves);
    }
  }
}

void weed_clear_plant_flags(weed_plant_t *plant, uint32_t flags, const char *ign_prefix) {
  if (!plant) return;
  else {
    size_t ign_prefix_len = 0;
    char **leaves = weed_plant_list_leaves(plant, NULL);
    if (leaves) {
      if (ign_prefix) ign_prefix_len = strlen(ign_prefix);
      for (int i = 0; leaves[i]; i++) {
        if (!ign_prefix || strncmp(leaves[i], ign_prefix, ign_prefix_len)) {
          weed_leaf_clear_flagbits(plant, leaves[i], flags);
        }
        free(leaves[i]);
      }
      free(leaves);
    }
  }
}

WEED_LOCAL_INLINE weed_plant_t *_weed_get_gui(weed_plant_t *plant,  int create_if_not_exists) {
  weed_plant_t *gui = NULL;
  int type = weed_plant_get_type(plant);
  if (type != WEED_PLANT_FILTER_CLASS && type != WEED_PLANT_PARAMETER_TEMPLATE
      && type != WEED_PLANT_PARAMETER && type != WEED_PLANT_FILTER_INSTANCE
      && type != WEED_PLANT_CHANNEL) return NULL;
  gui = weed_get_plantptr_value(plant, WEED_LEAF_GUI, NULL);
  if (!gui && create_if_not_exists == WEED_TRUE) {
    gui = weed_plant_new(WEED_PLANT_GUI);
    weed_leaf_set(plant, WEED_LEAF_GUI, WEED_SEED_PLANTPTR, 1, &gui);
  }
  return gui;
}

WEED_GLOBAL_INLINE int weed_host_info_get_flags(weed_plant_t *hinfo) {
  if (!WEED_PLANT_IS_HOST_INFO(hinfo)) return 0;
  return weed_get_int_value(hinfo, WEED_LEAF_FLAGS, NULL);
}

WEED_GLOBAL_INLINE void weed_host_info_set_flags(weed_plant_t *hinfo, int flags) {
  if (!WEED_PLANT_IS_HOST_INFO(hinfo)) return;
  weed_set_int_value(hinfo, WEED_LEAF_FLAGS, flags);
}

WEED_GLOBAL_INLINE void weed_host_set_verbosity(weed_plant_t *hinfo, int verbosity) {
  if (WEED_PLANT_IS_HOST_INFO(hinfo))
    weed_set_int_value(hinfo, WEED_LEAF_VERBOSITY, verbosity);
}

WEED_GLOBAL_INLINE void weed_host_set_supports_linear_gamma(weed_plant_t *hinfo) {
  if (WEED_PLANT_IS_HOST_INFO(hinfo))
    weed_host_info_set_flags(hinfo, weed_host_info_get_flags(hinfo) | WEED_HOST_SUPPORTS_LINEAR_GAMMA);
}

WEED_GLOBAL_INLINE void weed_host_set_supports_premult_alpha(weed_plant_t *hinfo) {
  if (WEED_PLANT_IS_HOST_INFO(hinfo))
    weed_host_info_set_flags(hinfo, weed_host_info_get_flags(hinfo) | WEED_HOST_SUPPORTS_PREMULTIPLIED_ALPHA);
}

WEED_GLOBAL_INLINE char *weed_plugin_info_get_package_name(weed_plant_t *pinfo) {
  if (!WEED_PLANT_IS_PLUGIN_INFO(pinfo)) return NULL;
  return weed_get_string_value(pinfo, WEED_LEAF_PACKAGE_NAME, NULL);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_filter_get_gui(weed_plant_t *filter, int create_if_not_exists) {
  return _weed_get_gui(filter, create_if_not_exists);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_filter_get_plugin_info(weed_plant_t *filter) {
  return weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
}

WEED_GLOBAL_INLINE char *weed_filter_get_package_name(weed_plant_t *filter) {
  return weed_plugin_info_get_package_name(weed_filter_get_plugin_info(filter));
}

WEED_GLOBAL_INLINE weed_plant_t *weed_instance_get_gui(weed_plant_t *inst, int create_if_not_exists) {
  return _weed_get_gui(inst, create_if_not_exists);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_paramtmpl_get_gui(weed_plant_t *paramt, int create_if_not_exists) {
  return _weed_get_gui(paramt, create_if_not_exists);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_param_get_gui(weed_plant_t *param, int create_if_not_exists) {
  return _weed_get_gui(param, create_if_not_exists);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_channel_get_gui(weed_plant_t *channel, int create_if_not_exists) {
  return _weed_get_gui(channel, create_if_not_exists);
}

WEED_GLOBAL_INLINE int weed_param_is_hidden(weed_plant_t *param, int temporary) {
  weed_plant_t *gui = weed_param_get_gui(param, WEED_FALSE);
  if (temporary == WEED_FALSE || !gui)
    return weed_paramtmpl_hints_hidden(weed_param_get_template(param));
  return weed_get_boolean_value(gui, WEED_LEAF_HIDDEN, NULL);
}

WEED_GLOBAL_INLINE int weed_filter_get_flags(weed_plant_t *filter) {
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return 0;
  return weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
}

WEED_GLOBAL_INLINE int weed_filter_hints_unstable(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_HINT_MAYBE_UNSTABLE) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_hints_stateless(weed_plant_t *filter) {
  if (!(weed_filter_get_flags(filter) & WEED_FILTER_HINT_STATEFUL)) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_non_realtime(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_NON_REALTIME) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_may_thread(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_HINT_MAY_THREAD) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_channel_sizes_vary(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_CHANNEL_SIZES_MAY_VARY) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_palettes_vary(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_PALETTES_MAY_VARY) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_prefers_linear_gamma(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_PREF_LINEAR_GAMMA) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_prefers_premult_alpha(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_PREF_PREMULTIPLIED_ALPHA) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_is_converter(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_IS_CONVERTER) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_is_process_last(weed_plant_t *filter) {
  if (weed_filter_get_flags(filter) & WEED_FILTER_HINT_PROCESS_LAST) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_hints_hidden(weed_plant_t *filter) {
  weed_plant_t *gui;
  if (WEED_PLANT_IS_FILTER_CLASS(filter)
      && (gui = weed_filter_get_gui(filter, WEED_FALSE))
      && weed_get_boolean_value(gui, WEED_LEAF_HIDDEN, NULL) == WEED_TRUE)
    return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE char *weed_filter_get_name(weed_plant_t *filter) {
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  return weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_filter_get_in_chantmpls(weed_plant_t *filter, int *ntmpls) {
  if (ntmpls) *ntmpls = 0;
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  return weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, ntmpls);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_filter_get_out_chantmpls(weed_plant_t *filter, int *ntmpls) {
  if (ntmpls) *ntmpls = 0;
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  return weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, ntmpls);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_filter_get_in_paramtmpls(weed_plant_t *filter, int *ntmpls) {
  if (ntmpls) *ntmpls = 0;
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  return weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, ntmpls);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_filter_get_out_paramtmpls(weed_plant_t *filter, int *ntmpls) {
  if (ntmpls) *ntmpls = 0;
  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  return weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, ntmpls);
}
WEED_GLOBAL_INLINE char *weed_chantmpl_get_name(weed_plant_t *chantmpl) {
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return NULL;
  return weed_get_string_value(chantmpl, WEED_LEAF_NAME, NULL);
}

WEED_GLOBAL_INLINE int weed_chantmpl_get_flags(weed_plant_t *chantmpl) {
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return 0;
  return weed_get_int_value(chantmpl, WEED_LEAF_FLAGS, NULL);
}

WEED_GLOBAL_INLINE int weed_chantmpl_get_max_audio_length(weed_plant_t *chantmpl) {
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return 0;
  return weed_get_int_value(chantmpl, WEED_LEAF_MAX_AUDIO_LENGTH, NULL);
}

WEED_GLOBAL_INLINE int weed_paramtmpl_get_flags(weed_plant_t *paramtmpl) {
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return 0;
  return weed_get_int_value(paramtmpl, WEED_LEAF_FLAGS, NULL);
}

WEED_GLOBAL_INLINE uint32_t weed_paramtmpl_value_type(weed_plant_t *paramtmpl) {
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return WEED_SEED_INVALID;
  if (weed_paramtmpl_has_variable_size(paramtmpl) && weed_plant_has_leaf(paramtmpl, WEED_LEAF_NEW_DEFAULT))
    return weed_leaf_seed_type(paramtmpl, WEED_LEAF_NEW_DEFAULT);
  return weed_leaf_seed_type(paramtmpl, WEED_LEAF_DEFAULT);
}

WEED_GLOBAL_INLINE int weed_paramtmpl_get_type(weed_plant_t *paramtmpl) {
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return 0;
  return weed_get_int_value(paramtmpl, WEED_LEAF_PARAM_TYPE, NULL);
}

WEED_GLOBAL_INLINE char *weed_paramtmpl_get_name(weed_plant_t *paramtmpl) {
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return NULL;
  return weed_get_string_value(paramtmpl, WEED_LEAF_NAME, NULL);
}

WEED_GLOBAL_INLINE int weed_paramtmpl_has_variable_size(weed_plant_t *paramtmpl) {
  if (WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)
      && (weed_paramtmpl_get_flags(paramtmpl) & WEED_PARAMETER_VARIABLE_SIZE)) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_paramtmpl_has_value_perchannel(weed_plant_t *paramtmpl) {
  if (WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)
      && (weed_paramtmpl_get_flags(paramtmpl) & WEED_PARAMETER_VALUE_PER_CHANNEL)) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_paramtmpl_does_wrap(weed_plant_t *paramtmpl) {
  weed_plant_t *gui;
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return WEED_FALSE;
  if ((gui = weed_paramtmpl_get_gui(paramtmpl, WEED_FALSE))
      && weed_get_boolean_value(gui, WEED_LEAF_WRAP, NULL) == WEED_TRUE) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_paramtmpl_hints_string_choice(weed_plant_t *paramtmpl) {
  weed_plant_t *gui;
  if (WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)
      && (gui = weed_paramtmpl_get_gui(weed_param_get_template(paramtmpl), WEED_FALSE))
      && weed_plant_has_leaf(gui, WEED_LEAF_CHOICES))
    return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_paramtmpl_hints_hidden(weed_plant_t *paramtmpl) {
  weed_plant_t *gui;
  if (WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)
      && (gui = weed_paramtmpl_get_gui(paramtmpl, WEED_FALSE)) != NULL
      && weed_get_boolean_value(gui, WEED_LEAF_HIDDEN, NULL) == WEED_TRUE)
    return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_paramtmpl_value_irrelevant(weed_plant_t *paramtmpl) {
  if (WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) {
    if (weed_paramtmpl_get_flags(paramtmpl) & WEED_PARAMETER_VALUE_IRRELEVANT) return WEED_TRUE;
  }
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_chantmpl_is_optional(weed_plant_t *chantmpl) {
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return WEED_TRUE;
  if (weed_chantmpl_get_flags(chantmpl) & WEED_CHANNEL_OPTIONAL) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_chantmpl_get_max_repeats(weed_plant_t *chantmpl) {
  /// a return value of zero means unlimited repeats
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return -1;
  if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS))
    return weed_get_int_value(chantmpl, WEED_LEAF_MAX_REPEATS, NULL);
  return 1;
}

WEED_GLOBAL_INLINE int weed_chantmpl_is_audio(weed_plant_t *chantmpl) {
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl)) return WEED_TRUE;
  return weed_get_boolean_value(chantmpl, WEED_LEAF_IS_AUDIO, NULL);
}

WEED_GLOBAL_INLINE int *weed_chantmpl_get_palette_list(weed_plant_t *filter, weed_plant_t *chantmpl, int *nvals) {
  int *pals, npals;
  if (nvals) *nvals = 0;
  if (!WEED_PLANT_IS_CHANNEL_TEMPLATE(chantmpl) || !WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;
  if ((weed_filter_get_flags(filter) & WEED_FILTER_PALETTES_MAY_VARY)
      && weed_plant_has_leaf(chantmpl, WEED_LEAF_PALETTE_LIST)) {
    pals = weed_get_int_array_counted(chantmpl, WEED_LEAF_PALETTE_LIST, &npals);
    for (int i = 0; i < npals; i++) {
    }
  } else {
    if (!weed_plant_has_leaf(filter, WEED_LEAF_PALETTE_LIST)) return NULL;
    pals = weed_get_int_array_counted(filter, WEED_LEAF_PALETTE_LIST, &npals);
  }
  if (npals > 0 && pals[npals - 1] == WEED_PALETTE_END) npals--;
  if (nvals) *nvals = npals;
  return pals;
}

WEED_GLOBAL_INLINE void *weed_channel_get_pixel_data(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  return weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);
}

WEED_GLOBAL_INLINE void **weed_channel_get_pixel_data_planar(weed_plant_t *channel, int *nplanes) {
  if (nplanes) *nplanes = 0;
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  return weed_get_voidptr_array_counted(channel, WEED_LEAF_PIXEL_DATA, NULL);
}

WEED_GLOBAL_INLINE int weed_channel_get_width(weed_plant_t *channel) {
  /// width in macropixels
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_WIDTH, NULL);
}

WEED_GLOBAL_INLINE void weed_channel_set_width(weed_plant_t *channel, int width) {
  /// width in macropixels
  if (!WEED_PLANT_IS_CHANNEL(channel)) return;
  weed_set_int_value(channel, WEED_LEAF_WIDTH, width);
}

WEED_GLOBAL_INLINE int weed_channel_get_width_pixels(weed_plant_t *channel) {
  /// width in pixels: only relevant when comparing widths of different palettes
  int pal = weed_channel_get_palette(channel);
  return weed_channel_get_width(channel) * weed_palette_get_pixels_per_macropixel(pal);
}

WEED_GLOBAL_INLINE int weed_channel_get_height(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_HEIGHT, NULL);
}

WEED_GLOBAL_INLINE void weed_channel_set_height(weed_plant_t *channel, int height) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return;
  weed_set_int_value(channel, WEED_LEAF_HEIGHT, height);
}

WEED_GLOBAL_INLINE void weed_channel_set_size(weed_plant_t *channel, int width, int height) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return;
  weed_set_int_value(channel, WEED_LEAF_WIDTH, width);
  weed_set_int_value(channel, WEED_LEAF_HEIGHT, height);
}

WEED_GLOBAL_INLINE void weed_channel_set_palette(weed_plant_t *channel, int palette) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return;
  weed_set_int_value(channel, WEED_LEAF_CURRENT_PALETTE, palette);
}

WEED_GLOBAL_INLINE int weed_channel_get_palette(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return WEED_PALETTE_NONE;
  return weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, NULL);
}

WEED_GLOBAL_INLINE int weed_channel_get_gamma_type(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return WEED_PALETTE_NONE;
  return weed_get_int_value(channel, WEED_LEAF_GAMMA_TYPE, NULL);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_channel_set_gamma_type(weed_plant_t *channel, int gamma_type) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  weed_set_int_value(channel, WEED_LEAF_GAMMA_TYPE, gamma_type);
  return channel;
}

WEED_GLOBAL_INLINE int weed_channel_get_palette_yuv(weed_plant_t *channel, int *clamping, int *sampling, int *subspace) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return WEED_PALETTE_NONE;
  else {
    int palette = weed_channel_get_palette(channel);
    if (weed_palette_is_yuv(palette)) {
      if (clamping) *clamping = weed_get_int_value(channel, WEED_LEAF_YUV_CLAMPING, NULL);
      if (sampling) *sampling = weed_get_int_value(channel, WEED_LEAF_YUV_SAMPLING, NULL);
      if (subspace) *subspace = weed_get_int_value(channel, WEED_LEAF_YUV_SUBSPACE, NULL);
    }
    return palette;
  }
}

WEED_GLOBAL_INLINE int weed_channel_get_rowstride(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, NULL);
}

WEED_GLOBAL_INLINE int *weed_channel_get_rowstrides(weed_plant_t *channel, int *nplanes) {
  if (nplanes) *nplanes = 0;
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_array_counted(channel, WEED_LEAF_ROWSTRIDES, nplanes);
}

WEED_GLOBAL_INLINE int weed_channel_get_audio_rate(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_AUDIO_RATE, NULL);
}

WEED_GLOBAL_INLINE int weed_channel_get_naudchans(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, NULL);
}

WEED_GLOBAL_INLINE int weed_channel_get_audio_length(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return 0;
  return weed_get_int_value(channel, WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
}

WEED_GLOBAL_INLINE int weed_channel_is_disabled(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return WEED_TRUE;
  return weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_channel_get_template(weed_plant_t *channel) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  return weed_get_plantptr_value(channel, WEED_LEAF_TEMPLATE, NULL);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_param_get_template(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return NULL;
  return weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, NULL);
}

WEED_GLOBAL_INLINE int weed_param_get_type(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_PARAM_UNSPECIFIED;
  return weed_paramtmpl_get_type(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_get_value_type(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_SEED_INVALID;
  return weed_paramtmpl_value_type(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_has_variable_size(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_FALSE;
  return weed_paramtmpl_has_variable_size(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_has_value_perchannel(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_FALSE;
  return weed_paramtmpl_has_value_perchannel(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_value_irrelevant(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_FALSE;
  return weed_paramtmpl_value_irrelevant(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_does_wrap(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_FALSE;
  return weed_paramtmpl_does_wrap(weed_param_get_template(param));
}

WEED_GLOBAL_INLINE int weed_param_get_nchoices(weed_plant_t *param) {
  weed_plant_t *gui;
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0;
  if ((gui = weed_param_get_gui(param, WEED_FALSE)) != NULL && weed_plant_has_leaf(gui, WEED_LEAF_CHOICES))
    return weed_leaf_num_elements(gui, WEED_LEAF_CHOICES);
  if ((gui = weed_paramtmpl_get_gui(weed_param_get_template(param), WEED_FALSE))
      && weed_plant_has_leaf(gui, WEED_LEAF_CHOICES))
    return weed_leaf_num_elements(gui, WEED_LEAF_CHOICES);
  return 0;
}

WEED_GLOBAL_INLINE float **weed_channel_get_audio_data(weed_plant_t *channel, int *naudchans) {
  if (naudchans) *naudchans = 0;
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  return (float **)weed_get_voidptr_array_counted(channel, WEED_LEAF_AUDIO_DATA, naudchans);
}

WEED_GLOBAL_INLINE weed_plant_t *weed_channel_set_audio_data(weed_plant_t *channel, float **data,
    int arate, int naudchans, int nsamps) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  if (data) weed_set_voidptr_array(channel, WEED_LEAF_AUDIO_DATA, naudchans, (void **)data);
  weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, arate);
  weed_set_int_value(channel, WEED_LEAF_AUDIO_DATA_LENGTH, nsamps);
  weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, naudchans);
  return channel;
}

WEED_GLOBAL_INLINE int weed_instance_get_flags(weed_plant_t *inst) {
  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return 0;
  return weed_get_int_value(inst, WEED_LEAF_FLAGS, NULL);
}

WEED_GLOBAL_INLINE void weed_instance_set_flags(weed_plant_t *inst, int flags) {
  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return;
  weed_set_int_value(inst, WEED_LEAF_FLAGS, flags);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_instance_get_in_channels(weed_plant_t *instance, int *nchans) {
  if (nchans) *nchans = 0;
  if (!WEED_PLANT_IS_FILTER_INSTANCE(instance)) return NULL;
  return weed_get_plantptr_array_counted(instance, WEED_LEAF_IN_CHANNELS, nchans);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_instance_get_out_channels(weed_plant_t *instance, int *nchans) {
  if (nchans) *nchans = 0;
  if (!WEED_PLANT_IS_FILTER_INSTANCE(instance)) return NULL;
  return weed_get_plantptr_array_counted(instance, WEED_LEAF_OUT_CHANNELS, nchans);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_instance_get_in_params(weed_plant_t *instance, int *nparams) {
  if (nparams) *nparams = 0;
  if (!WEED_PLANT_IS_FILTER_INSTANCE(instance)) return NULL;
  return weed_get_plantptr_array_counted(instance, WEED_LEAF_IN_PARAMETERS, nparams);
}

WEED_GLOBAL_INLINE weed_plant_t **weed_instance_get_out_params(weed_plant_t *instance, int *nparams) {
  if (nparams) *nparams = 0;
  if (!WEED_PLANT_IS_FILTER_INSTANCE(instance)) return NULL;
  return weed_get_plantptr_array_counted(instance, WEED_LEAF_OUT_PARAMETERS, nparams);
}

WEED_GLOBAL_INLINE int weed_param_get_value_int(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0;
  return weed_get_int_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE int weed_param_get_value_boolean(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_FALSE;
  return weed_get_boolean_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE double weed_param_get_value_double(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0.;
  return weed_get_double_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE float weed_param_get_value_float(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0.;
  return weed_get_double_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE int64_t weed_param_get_value_int64(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0;
  return weed_get_int64_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE uint64_t weed_param_get_value_uint64(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0;
  return weed_get_uint64_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE char *weed_param_get_value_string(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return NULL;
  if (weed_leaf_num_elements(param, WEED_LEAF_VALUE) == 0) return NULL;
  return weed_get_string_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_int(weed_plant_t *param, int val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_int_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_boolean(weed_plant_t *param, int val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_boolean_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_double(weed_plant_t *param, double val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_double_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_float(weed_plant_t *param, float val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_double_value(param, WEED_LEAF_VALUE, (double)val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_int64(weed_plant_t *param, int64_t val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_int64_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_uint64(weed_plant_t *param, uint64_t val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_uint64_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE weed_error_t weed_param_set_value_string(weed_plant_t *param, const char *val) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return WEED_ERROR_WRONG_PLANT_TYPE;
  return weed_set_string_value(param, WEED_LEAF_VALUE, val);
}

WEED_GLOBAL_INLINE int weed_gui_get_flags(weed_plant_t *gui) {
  if (!WEED_PLANT_IS_GUI(gui)) return 0;
  return weed_get_int_value(gui, WEED_LEAF_FLAGS, NULL);
}


//////////////////////////////////////////// utilities ///////////////////////

char *weed_strerror(weed_error_t error) {
  // return value should be freed after use
  switch (error) {
  case (WEED_ERROR_MEMORY_ALLOCATION):
    return strdup("Memory allocation error");
    /* case (WEED_ERROR_CONCURRENCY): */
    return strdup("Thread concurrency failure");
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

#ifndef __QUOTEME
#define __QUOTEME(a) #a
#endif
#define RET_FOR(errmsg) case (WEED_ERROR_##errmsg): return strdup(__QUOTEME(WEED_ERROR_##errmsg))

char *weed_error_to_literal(weed_error_t error) {
  // return value should be freed after use
  switch (error) {
  case WEED_SUCCESS: return strdup("WEED_SUCCESS");
    RET_FOR(MEMORY_ALLOCATION);
    RET_FOR(IMMUTABLE);
    RET_FOR(UNDELETABLE);
    RET_FOR(BADVERSION);
    RET_FOR(NOSUCH_ELEMENT);
    RET_FOR(NOSUCH_LEAF);
    RET_FOR(WRONG_SEED_TYPE);
    RET_FOR(TOO_MANY_INSTANCES);
    RET_FOR(PLUGIN_INVALID);
    RET_FOR(FILTER_INVALID);
    RET_FOR(REINIT_NEEDED);
  default: return strdup("UNKNOWN ERROR");
  }
}
#undef __QUOTEME

char *weed_seed_type_to_text(uint32_t seed_type) {
  switch (seed_type) {
  case WEED_SEED_INT:
    return strdup("integer");
  case WEED_SEED_UINT:
    return strdup("unsigned integer");
  case WEED_SEED_INT64:
    return strdup("int64");
  case WEED_SEED_UINT64:
    return strdup("uint64");
  case WEED_SEED_BOOLEAN:
    return strdup("boolean");
  case WEED_SEED_DOUBLE:
    return strdup("double");
  case WEED_SEED_FLOAT:
    return strdup("float");
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

const char *weed_seed_type_to_short_text(uint32_t seed_type) {
  switch (seed_type) {
  case WEED_SEED_INT: return "int";
  case WEED_SEED_INT64: return "int64";
  case WEED_SEED_UINT: return "uint";
  case WEED_SEED_UINT64: return "uint64";
  case WEED_SEED_BOOLEAN: return "boolean";
  case WEED_SEED_DOUBLE: return "double";
  case WEED_SEED_FLOAT: return "float";
  case WEED_SEED_STRING: return "string";
  case WEED_SEED_FUNCPTR: return "funcptr";
  case WEED_SEED_VOIDPTR: return "voidptr";
  case WEED_SEED_PLANTPTR: return "plantptr";
  default: return "void";
  }
}

const char *weed_seed_to_ctype(uint32_t st, int add_space) {
  switch (st) {
  case WEED_SEED_VOID: return "void";
  case WEED_SEED_STRING: return "char *";
  case WEED_SEED_FUNCPTR: return "lives_func_t *";
  case WEED_SEED_VOIDPTR: return "void *";
  case WEED_SEED_PLANTPTR: return "weed_plant_t *";
  case WEED_SEED_INT: case WEED_SEED_UINT: case WEED_SEED_BOOLEAN: case WEED_SEED_DOUBLE:
  case WEED_SEED_INT64: case WEED_SEED_FLOAT: case WEED_SEED_UINT64:
    if (add_space) {
      switch (st) {
      case WEED_SEED_INT: return "int ";
      case WEED_SEED_UINT: return "uint32_t ";
      case WEED_SEED_BOOLEAN: return "boolean ";
      case WEED_SEED_DOUBLE: return "double ";
      case WEED_SEED_FLOAT: return "float ";
      case WEED_SEED_INT64: return "int64_t ";
      case WEED_SEED_UINT64: return "uint64_t ";
      }
    } else {
      switch (st) {
      case WEED_SEED_INT: return "int";
      case WEED_SEED_UINT: return "uint32_t";
      case WEED_SEED_BOOLEAN: return "boolean";
      case WEED_SEED_DOUBLE: return "double";
      case WEED_SEED_FLOAT: return "float";
      case WEED_SEED_INT64: return "int64_t";
      case WEED_SEED_UINT64: return "uint64_t";
      }
    }
  default:
    if (st >= WEED_SEED_FIRST_PTR_TYPE) return "void *";
    return "<unknown type>";
  }
}



uint32_t ctypes_to_weed_seed(const char *ctype) {
  if (!ctype || !*ctype || !strcmp(ctype, "void")) return 0;
  if (!strcmp(ctype, "int")
      || !strcmp(ctype, "int32_2"))
    return WEED_SEED_INT;
  if (!strcmp(ctype, "uint")
      || !strcmp(ctype, "uint32_2"))
    return WEED_SEED_INT;
  if (!strcmp(ctype, "boolean")
      || !strcmp(ctype, "bool"))
    return WEED_SEED_BOOLEAN;
  if (!strcmp(ctype, "double")) return WEED_SEED_DOUBLE;
  if (!strcmp(ctype, "float")) return WEED_SEED_FLOAT;
  if (!strcmp(ctype, "char *")
      || !strcmp(ctype, "const char *"))
    return WEED_SEED_STRING;
  if (!strcmp(ctype, "int64_t"))
    return WEED_SEED_INT64;
  if (!strcmp(ctype, "uint64_t"))
    return WEED_SEED_UINT64;
  if (!strcmp(ctype, "void *")
      || !strcmp(ctype, "weed_voidptr_t")
      || !strcmp(ctype, "livespointer"))
    return WEED_SEED_VOIDPTR;
  if (!strcmp(ctype, "lives_funcptr_t")
      || !strcmp(ctype, "weed_funcptr_t"))
    return WEED_SEED_FUNCPTR;
  if (!strcmp(ctype, "weed_plant_t *")
      || !strcmp(ctype, "weed_plantptr_t"))
    return WEED_SEED_PLANTPTR;
  return WEED_SEED_INVALID;
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
  if (weed_palette_is_yuv(pal)) {
    char pnameb[512];
    const char *clamp = weed_yuv_clamping_get_name(clamping);
    const char *sspace = weed_yuv_subspace_get_name(subspace);
    snprintf(pnameb, 512, "%s:%s (%s)", pname, sspace, clamp);
    return strdup(pnameb);
  }
  return strdup(pname);
}

const char *weed_gamma_get_name(int gamma) {
  if (gamma == WEED_GAMMA_LINEAR) return "linear";
  if (gamma == WEED_GAMMA_SRGB) return "sRGB";
  if (gamma == WEED_GAMMA_BT709) return "bt709";
  return "unknown";
}

#ifndef WEED_ADVANCED_PALETTES

WEED_GLOBAL_INLINE int weed_palette_get_bits_per_macropixel(int pal) {
  if (pal == WEED_PALETTE_END) return 1;
  if (pal == WEED_PALETTE_A8 || pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P ||
      pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) return 8;
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) return 24;
  if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 ||
      pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV888
      || pal == WEED_PALETTE_YUVA8888)
    return 32;
  if (pal == WEED_PALETTE_YUV411) return 48;
  if (pal == WEED_PALETTE_AFLOAT) return sizeof(float);
  if (pal == WEED_PALETTE_A1) return 1;
  if (pal == WEED_PALETTE_RGBFLOAT) return (3 * sizeof(float));
  if (pal == WEED_PALETTE_RGBAFLOAT) return (4 * sizeof(float));
  return 0; // unknown palette
}

#endif

// just as an example: 1.0 == RGB(A), 0.5 == compressed to 50%, etc

double weed_palette_get_compression_ratio(int pal) {
  double tbits = 0.;

  int nplanes = weed_palette_get_nplanes(pal);
  int pbits;

  if (!weed_palette_is_valid(pal)) return 0.;
  if (weed_palette_is_alpha(pal)) return 0.; // invalid for alpha palettes
  for (int i = 0; i < nplanes; i++) {
    pbits = weed_palette_get_bits_per_macropixel(pal) / weed_palette_get_pixels_per_macropixel(pal);
    tbits += pbits * weed_palette_get_plane_ratio_vertical(pal, i) * weed_palette_get_plane_ratio_horizontal(pal, i);
  }
  if (weed_palette_has_alpha(pal)) return tbits / 32.;
  return tbits / 24.;
}

WEED_GLOBAL_INLINE int weed_filter_is_resizer(weed_plant_t *filter) {
  if (weed_filter_is_converter(filter) && weed_filter_channel_sizes_vary(filter)) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_filter_is_palette_converter(weed_plant_t *filter) {
  if (weed_filter_is_converter(filter) && weed_filter_palettes_vary(filter)) return WEED_TRUE;
  return WEED_FALSE;
}

WEED_GLOBAL_INLINE int weed_audio_filter_is_resampler(weed_plant_t *filter) {
  int flags = weed_filter_get_flags(filter);
  if (weed_filter_is_converter(filter)
      && (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY)) return WEED_TRUE;
  return WEED_FALSE;
}

weed_error_t weed_plant_duplicate(weed_plant_t *dst, weed_plant_t *src, int add) {
  char **leaves;
  char *key;
  int i = 0;
  weed_error_t err = WEED_SUCCESS;

  if (!src || !dst) return WEED_ERROR_NOSUCH_PLANT;
  if (weed_plant_get_type(dst) != weed_plant_get_type(src)) return WEED_ERROR_WRONG_PLANT_TYPE;

  if (!add) {
    int gottype = 0;
    leaves = weed_plant_list_leaves(dst, NULL);
    for (key = leaves[0]; (key = leaves[i]) != NULL && err == WEED_SUCCESS; i++) {
      if (err == WEED_SUCCESS) {
        if (!gottype && !strcmp(key, WEED_LEAF_TYPE)) gottype = 1;
        else {
          err = weed_leaf_delete(dst, key);
          if (err == WEED_ERROR_UNDELETABLE) err = WEED_SUCCESS; // ignore these errors
        }
      }
    }
  }
  if (err == WEED_SUCCESS) {
    int gottype = 0;
    leaves = weed_plant_list_leaves(src, NULL);
    for (key = leaves[0]; (key = leaves[i]) != NULL && err == WEED_SUCCESS; i++) {
      if (err == WEED_SUCCESS) {
        if (!gottype && !strcmp(key, WEED_LEAF_TYPE)) gottype = 1;
        else {
          err = weed_leaf_dup(dst, src, key);
          if (err == WEED_ERROR_IMMUTABLE || err == WEED_ERROR_WRONG_SEED_TYPE) err = WEED_SUCCESS; // ignore these errors
        }
      } free(key);
    } free(leaves);
  }
  return err;
}

size_t weed_plant_weigh(weed_plant_t *plant) {
  return weed_plant_get_byte_size(plant);
  return 0;
}

WEED_GLOBAL_INLINE weed_error_t weed_plant_mutate_type(weed_plantptr_t plant, int32_t newtype) {
  weed_error_t err = WEED_ERROR_NOSUCH_PLANT;
  if (plant) {
    int32_t flags = weed_leaf_get_flags(plant, WEED_LEAF_TYPE);
    // clear the default flags to allow the "type" leaf to be altered
    err = weed_leaf_set_flags(plant, WEED_LEAF_TYPE, flags & ~(WEED_FLAG_IMMUTABLE));
    if (err == WEED_SUCCESS) {
      weed_error_t err2;
      err = weed_set_int_value(plant, WEED_LEAF_TYPE, newtype);
      //weed_set_int_value(plant, WEED_LEAF_TYPE, newtype);
      // lock the "type" leaf again so it cannot be altered accidentally
      err2 = weed_leaf_set_flags(plant, WEED_LEAF_TYPE, WEED_FLAG_IMMUTABLE);
      if (err == WEED_SUCCESS) err = err2;
    }
  }
  return err;
}

/////////////////////////// plugin / host callbacks //////////////////

static weed_host_info_callback_f host_info_callback = NULL;
static void *host_info_callback_data = NULL;

void weed_set_host_info_callback(weed_host_info_callback_f cb, void *user_data) {
  host_info_callback = cb;
  host_info_callback_data = user_data;
}


int check_weed_abi_compat(int32_t higher, int32_t lower) {
  if (lower == higher) return WEED_TRUE; // equal versions are always compatible
  if (lower > higher) {
    int32_t tmp = lower;
    lower = higher;
    higher = tmp;
  }
  if (higher > WEED_ABI_VERSION) return WEED_FALSE; // we cant possibly know about future versions
  if (lower < 200 && higher >= 200) return WEED_FALSE; // ABI 200 made breaking changes
  if (higher < 100) return WEED_FALSE;
  return WEED_TRUE;
}


int check_filter_api_compat(int32_t higher, int32_t lower) {
  if (lower == higher) return WEED_TRUE; // equal versions are always compatible
  if (lower > higher) {
    int32_t tmp = lower;
    lower = higher;
    higher = tmp;
  }
  if (higher > WEED_FILTER_API_VERSION) return WEED_FALSE; // we cant possibly know about future versions
  if (higher < 100) return WEED_FALSE;
  return WEED_TRUE;
}


static int check_version_compat(int host_weed_api_version,
                                int plugin_weed_api_min_version,
                                int plugin_weed_api_max_version,
                                int host_filter_api_version,
                                int plugin_filter_api_min_version,
                                int plugin_filter_api_max_version) {
  if (plugin_weed_api_min_version > host_weed_api_version || plugin_filter_api_min_version > host_filter_api_version)
    return WEED_FALSE;

  if (host_weed_api_version > plugin_weed_api_max_version) {
    if (check_weed_abi_compat(host_weed_api_version, plugin_weed_api_max_version) == 0) return 0;
  }

  if (host_filter_api_version > plugin_filter_api_max_version) {
    return check_filter_api_compat(host_filter_api_version, plugin_filter_api_max_version);
  }
  return WEED_TRUE;
}



  //////////// get weed api version /////////

static weed_error_t _weed_def_get(weed_plant_t *plant, const char *key, void *value,
				 weed_leaf_get_f *wlg_def) {
  static weed_leaf_get_f *wlg = NULL;
  if (wlg_def) {
    wlg = wlg_def;
    return WEED_SUCCESS;
  }
  return (*wlg)(plant, key, 1, value);
}


static weed_error_t _weed_default_get(weed_plant_t *plant, const char *key, void *value) {
  // we pass a pointer to this function back to the plugin so that it can bootstrap its real functions
  return _wbg(_WEED_PADBYTES_, 0xB82E802F, plant, key, value);
}


weed_plant_t *weed_bootstrap(weed_default_getter_f *value,
                             int32_t plugin_min_weed_abi_version,
                             int32_t plugin_max_weed_abi_version,
                             int32_t plugin_min_filter_api_version,
                             int32_t plugin_max_filter_api_version) {
  // function is called from weed_setup() in the plugin, using the fn ptr passed by the host

  // here is where we define the functions for the plugin to use
  // the host is free to implement its own version and then pass a pointer to that function in weed_setup() for the plugin

  static weed_leaf_get_f wlg;
  static weed_plant_new_f wpn;
  static weed_plant_list_leaves_f wpll;
  static weed_leaf_num_elements_f wlne;
  static weed_leaf_element_size_f wles;
  static weed_leaf_seed_type_f wlst;
  static weed_leaf_get_flags_f wlgf;
  static weed_leaf_set_f wls;
  static weed_malloc_f weedmalloc;
#if WEED_ABI_CHECK_VERSION(200)
  static weed_realloc_f weedrealloc;
  static weed_calloc_f weedcalloc;
  static weed_memmove_f weedmemmove;
#endif
  static weed_free_f weedfree;
  static weed_memcpy_f weedmemcpy;
  static weed_memset_f weedmemset;
  static weed_plant_free_f wpf;
  static weed_leaf_delete_f wld;

  int host_set_host_info = WEED_FALSE;

#if WEED_ABI_CHECK_VERSION(200)
  int32_t host_weed_abi_version = libweed_get_abi_version();
#else
  /* versions here are just default values, we will set them again later, after possibly calling the host_info_callback function */
  int32_t host_weed_abi_version = WEED_ABI_VERSION;
#endif
  int32_t host_filter_api_version = WEED_FILTER_API_VERSION;

  int32_t plugin_weed_abi_version = plugin_min_weed_abi_version;
  int32_t plugin_filter_api_version = plugin_min_filter_api_version;

  weed_plant_t *host_info = NULL;
  weed_plant_t *plugin_info = NULL;

  weed_error_t err;

  if (host_weed_abi_version > WEED_ABI_VERSION) return NULL;
  
  *value = _weed_default_get; // value is a pointer to fn. ptr
  if (!*value) return NULL;

  if (plugin_min_weed_abi_version > plugin_max_weed_abi_version) {
    // plugin author may be confused
    int32_t tmp = plugin_min_weed_abi_version;
    plugin_min_weed_abi_version = plugin_max_weed_abi_version;
    plugin_max_weed_abi_version = tmp;
  }
  if (plugin_min_filter_api_version > plugin_max_filter_api_version) {
    int32_t tmp = plugin_min_weed_abi_version;
    plugin_min_weed_abi_version = plugin_max_weed_abi_version;
    plugin_max_weed_abi_version = tmp;
  }

  // set pointers to the functions the plugin will use

  wpn = weed_plant_new;
  wpll = weed_plant_list_leaves;
  wlne = weed_leaf_num_elements;
  wles = weed_leaf_element_size;
  wlst = weed_leaf_seed_type;
  wlgf = weed_leaf_get_flags;
  wls  = weed_leaf_set;
  wlg  = weed_leaf_get;

  // added for plugins in Filter API 200
  wpf = weed_plant_free;
  wld = weed_leaf_delete;

  weedmalloc = malloc;
  weedfree = free;
  weedmemcpy = memcpy;
  weedmemset = memset;

#if WEED_ABI_CHECK_VERSION(200)
  // added for plugins in Weed ABI 200
  weedrealloc = realloc;
  weedmemmove = memmove;
  weedcalloc = calloc;
#endif

  host_info = weed_plant_new(WEED_PLANT_HOST_INFO);
  if (!host_info) return NULL;

  if (weed_set_int_value(host_info, WEED_LEAF_WEED_ABI_VERSION, host_weed_abi_version) != WEED_SUCCESS) {
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(host_info, WEED_LEAF_FILTER_API_VERSION, host_filter_api_version) != WEED_SUCCESS) {
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }

  if (weedmalloc) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)weedmalloc) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedfree) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)weedfree) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedmemset) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)weedmemset) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedmemcpy) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)weedmemcpy) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }

#if WEED_ABI_CHECK_VERSION(200)
  if (plugin_max_weed_abi_version >= 200) {
    if (weedmemmove) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMMOVE_FUNC, (weed_funcptr_t)weedmemmove) != WEED_SUCCESS) {
        if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weedrealloc) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)weedrealloc) != WEED_SUCCESS) {
        if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weedcalloc) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)weedcalloc) != WEED_SUCCESS) {
        if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

#endif

  if (wpn) {
    if (weed_set_funcptr_value(host_info, WEED_PLANT_NEW_FUNC, (weed_funcptr_t)wpn) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlg) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t)wlg) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wls) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)wls) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlst) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_SEED_TYPE_FUNC, (weed_funcptr_t)wlst) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlne) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, (weed_funcptr_t)wlne) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wles) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, (weed_funcptr_t)wles) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wpll) {
    if (weed_set_funcptr_value(host_info, WEED_PLANT_LIST_LEAVES_FUNC, (weed_funcptr_t)wpll) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlgf) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FLAGS_FUNC, (weed_funcptr_t)wlgf) != WEED_SUCCESS) {
      if (host_info) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (plugin_max_filter_api_version >= 200) {
    if (wpf) {
      if (weed_set_funcptr_value(host_info, WEED_PLANT_FREE_FUNC, (weed_funcptr_t)wpf) != WEED_SUCCESS) {
        if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (wld) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_DELETE_FUNC, (weed_funcptr_t)wld) != WEED_SUCCESS) {
        if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

  plugin_info = weed_plant_new(WEED_PLANT_PLUGIN_INFO);
  if (!plugin_info) {
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }

  if (weed_set_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, plugin_info) != WEED_SUCCESS) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }

  if (weed_set_int_value(plugin_info, WEED_LEAF_MIN_WEED_ABI_VERSION, plugin_min_weed_abi_version) != WEED_SUCCESS) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MAX_WEED_ABI_VERSION, plugin_max_weed_abi_version) != WEED_SUCCESS) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MIN_FILTER_API_VERSION, plugin_min_filter_api_version) != WEED_SUCCESS) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MAX_FILTER_API_VERSION, plugin_max_filter_api_version) != WEED_SUCCESS) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_info) weed_plant_free(host_info);
    return NULL;
  }

  if (host_info_callback) {
    // if host set a callback function, we call it so it can examine and adjust the host_info plant
    // including setting memory functions and checking the weed and filter api values if it wishes
    // host can also substitute its own host_info
    // the host is also free to adjust or replace plant_info

    weed_plant_t *host_host_info = host_info_callback(host_info, host_info_callback_data);
    if (!host_host_info) {
      if (plugin_info) weed_plant_free(plugin_info);
      return NULL;
    }

    if (host_host_info != host_info) {
      if (host_info) weed_plant_free(host_info);
      host_info = host_host_info;
      host_set_host_info = WEED_TRUE;
    }

    if (weed_plant_has_leaf(host_info, WEED_LEAF_WEED_ABI_VERSION)) {
      host_weed_abi_version = weed_get_int_value(host_host_info, WEED_LEAF_WEED_ABI_VERSION, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weed_plant_has_leaf(host_info, WEED_LEAF_FILTER_API_VERSION)) {
      host_filter_api_version = weed_get_int_value(host_host_info, WEED_LEAF_FILTER_API_VERSION, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

  if (!check_version_compat(host_weed_abi_version, plugin_min_weed_abi_version, plugin_max_weed_abi_version,
                            host_filter_api_version, plugin_min_filter_api_version, plugin_max_filter_api_version)) {
    if (plugin_info) weed_plant_free(plugin_info);
    if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
    return NULL;
  }

  plugin_weed_abi_version = host_weed_abi_version;
  plugin_filter_api_version = host_filter_api_version;

  if (host_set_host_info) {
    if (weed_plant_has_leaf(host_info, WEED_LEAF_PLUGIN_INFO)) {
      weed_plant_t *host_plugin_info = weed_get_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }

      if (host_plugin_info != plugin_info) {
        if (plugin_info) weed_plant_free(plugin_info);
        plugin_info = NULL;
      }
    }

    // host replaced the host_info with one of its own, check that all of the functions are present
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MALLOC_FUNC)) {
      if (weedmalloc == NULL) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)weedmalloc) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_FREE_FUNC)) {
      if (!weedfree) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)weedfree) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMSET_FUNC)) {
      if (!weedmemset) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)weedmemset) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMCPY_FUNC)) {
      if (!weedmemcpy) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)weedmemcpy) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }

#if WEED_ABI_CHECK_VERSION(200)
    if (plugin_weed_abi_version >= 200) {
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMMOVE_FUNC)) {
        if (!weedmemmove) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMMOVE_FUNC, (weed_funcptr_t)weedmemmove) != WEED_SUCCESS) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
      }
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_REALLOC_FUNC)) {
        if (!weedrealloc) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)weedrealloc) != WEED_SUCCESS) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
      }
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_CALLOC_FUNC)) {
        if (!weedcalloc) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)weedcalloc) != WEED_SUCCESS) {
          if (plugin_info) weed_plant_free(plugin_info);
          return NULL;
        }
      }
    }

#endif

    if (!weed_plant_has_leaf(host_info, WEED_PLANT_NEW_FUNC)) {
      if (!wpn) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_NEW_FUNC, (weed_funcptr_t)wpn) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_GET_FUNC)) {
      if (!wlg) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t)wlg) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_SET_FUNC)) {
      if (!wls) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)wls) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_SEED_TYPE_FUNC)) {
      if (!wlst) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_SEED_TYPE_FUNC, (weed_funcptr_t)wlst) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC)) {
      if (!wlne) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, (weed_funcptr_t)wlne) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC)) {
      if (!wles) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, (weed_funcptr_t)wles) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_PLANT_LIST_LEAVES_FUNC)) {
      if (!wpll) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_LIST_LEAVES_FUNC, (weed_funcptr_t)wpll) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_GET_FLAGS_FUNC)) {
      if (!wlgf) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FLAGS_FUNC, (weed_funcptr_t)wlgf) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
  }

  if (plugin_filter_api_version >= 200) {
    if (!weed_plant_has_leaf(host_info, WEED_PLANT_FREE_FUNC)) {
      if (!wpf) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_FREE_FUNC, (weed_funcptr_t)wpf) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_DELETE_FUNC)) {
      if (!wld) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_DELETE_FUNC, (weed_funcptr_t)wld) != WEED_SUCCESS) {
        if (plugin_info) weed_plant_free(plugin_info);
        return NULL;
      }
    }
  }

#if WEED_ABI_CHECK_VERSION(200)
  // readjust the ABI depending on the weed_abi_version selected by the host

  if (plugin_weed_abi_version < 200) {
    // added in ABI 200, so remove them for lower versions
    if (weed_plant_has_leaf(host_info, WEED_LEAF_REALLOC_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_REALLOC_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedrealloc = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_CALLOC_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_CALLOC_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedcalloc = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_MEMMOVE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_MEMMOVE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedmemmove = NULL;
  }
#endif

  if (plugin_filter_api_version < 200) {
    if (weed_plant_has_leaf(host_info, WEED_PLANT_FREE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_PLANT_FREE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    wpf = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_DELETE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_DELETE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info) weed_plant_free(host_info);
        return NULL;
      }
    }
    wld = NULL;
  }
  return host_info;
}



