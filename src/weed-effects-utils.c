// weed-effects-utils.c
// - will probably become libweed-effects-utils or something
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// weed filter utility functions

// TODO: get in_paramtmpl(ptmpl, n)
/// get_colorspace
/// get_max / min

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h" // TODO

#if NEED_LOCAL_WEED
#include "../libweed/weed-host.h"
#include "../libweed/weed.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-palettes.h"
#else
#include <weed/weed-host.h>
#include <weed/weed.h>
#include <weed/weed-utils.h>
#include <weed/weed-effects.h>
#include <weed/weed-palettes.h>
#endif

#include "weed-effects-utils.h"

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
      for (register int i = 0; leaves[i]; i++) {
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
      && type != WEED_PLANT_PARAMETER && type != WEED_PLANT_FILTER_INSTANCE) return NULL;
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
  if (weed_filter_get_flags(filter) & WEED_FILTER_HINT_STATELESS) return WEED_TRUE;
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

/* int weed_filter_get_transition_param(weed_plant_t *filter, const char *skip) { */
/*   int num_params, count = 0; */
/*   weed_plant_t **in_ptmpls = weed_filter_get_in_paramtmpls(filter, &num_params); */
/*   if (num_params == 0) return -1; */
/*   for (int i = 0; i < num_params; i++) { */
/*     if (skip != NULL && weed_plant_has_leaf(in_ptmpls[i], skip)) continue; */
/*     if (weed_get_boolean_value(in_ptmpls[i], WEED_LEAF_IS_TRANSITION, NULL) == WEED_TRUE) { */
/*       free_func(in_ptmpls); */
/*       return count; */
/*     } */
/*     count++; */
/*   } */
/*   free_func(in_ptmpls); */
/*   return -1; */
/* } */

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
  if (!WEED_PLANT_IS_PARAMETER_TEMPLATE(paramtmpl)) return -1;
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
    for (register int i = 0; i < npals; i++) {
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
  return  weed_channel_get_width(channel) * weed_palette_get_pixels_per_macropixel(weed_channel_get_palette(channel));
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

WEED_GLOBAL_INLINE weed_layer_t *weed_channel_set_audio_data(weed_plant_t *channel, float **data,
    int arate, int naudchans, int nsamps) {
  if (!WEED_PLANT_IS_CHANNEL(channel)) return NULL;
  weed_set_voidptr_array(channel, WEED_LEAF_AUDIO_DATA, naudchans, (void **)data);
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

WEED_GLOBAL_INLINE int64_t weed_param_get_value_int64(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return 0;
  return weed_get_int64_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE char *weed_param_get_value_string(weed_plant_t *param) {
  if (!WEED_PLANT_IS_PARAMETER(param)) return NULL;
  if (weed_leaf_num_elements(param, WEED_LEAF_VALUE) == 0) return NULL;
  return weed_get_string_value(param, WEED_LEAF_VALUE, NULL);
}

WEED_GLOBAL_INLINE int weed_gui_get_flags(weed_plant_t *gui) {
  if (!WEED_PLANT_IS_GUI(gui)) return 0;
  return weed_get_int_value(gui, WEED_LEAF_FLAGS, NULL);
}

//////////////////////////////////////////// utilities ///////////////////////

char *weed_error_to_text(weed_error_t error) {
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

char *weed_seed_type_to_text(uint32_t seed_type) {
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

const char *weed_seed_type_to_short_text(uint32_t seed_type) {
  switch (seed_type) {
  case WEED_SEED_INT: return "int";
  case WEED_SEED_INT64: return "int64";
  case WEED_SEED_BOOLEAN: return "boolean";
  case WEED_SEED_DOUBLE: return "double";
  case WEED_SEED_STRING: return "string";
  case WEED_SEED_FUNCPTR: return "funcptr";
  case WEED_SEED_VOIDPTR: return "voidptr";
  case WEED_SEED_PLANTPTR: return "plantptr";
  default: return "void";
  }
}

const char *weed_seed_to_ctype(uint32_t st, int add_space) {
  const char *tp = NULL;
  switch (st) {
  case 0: tp = "void"; break;
  case WEED_SEED_STRING: tp = "char *"; break;
  case WEED_SEED_FUNCPTR: tp = "lives_func_t *"; break;
  case WEED_SEED_VOIDPTR: tp = "void *"; break;
  case WEED_SEED_PLANTPTR: tp = "weed_plant_t *"; break;
  case WEED_SEED_INT: case WEED_SEED_BOOLEAN: case WEED_SEED_DOUBLE: case WEED_SEED_INT64:
    if (add_space == WEED_TRUE) {
      switch (st) {
      case WEED_SEED_INT: tp = "int "; break;
      case WEED_SEED_BOOLEAN: tp = "boolean "; break;
      case WEED_SEED_DOUBLE: tp = "double "; break;
      case WEED_SEED_INT64: tp = "int64_t "; break;
      }
    } else {
      switch (st) {
      case WEED_SEED_INT: tp = "int"; break;
      case WEED_SEED_BOOLEAN: tp = "boolean"; break;
      case WEED_SEED_DOUBLE: tp = "double"; break;
      case WEED_SEED_INT64: tp = "int64_t"; break;
      }
    }
    break;
  default: tp = "void *"; break;
  }
  return tp;
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
    const char *clamp = weed_yuv_clamping_get_name(clamping);
    const char *sspace = weed_yuv_subspace_get_name(subspace);
    return lives_strdup_printf("%s:%s (%s)", pname, sspace, clamp); // TODO
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

  register int i;

  if (!weed_palette_is_valid(pal)) return 0.;
  if (weed_palette_is_alpha(pal)) return 0.; // invalid for alpha palettes
  for (i = 0; i < nplanes; i++) {
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

