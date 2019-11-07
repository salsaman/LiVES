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

#include <string.h>
#include <stdio.h>

#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>

#ifndef ALLOW_UNUSED
#ifdef __GNUC__
#  define ALLOW_UNUSED  __attribute__((unused))
#else
#  define ALLOW_UNUSED
#endif
#endif

static int weed_get_api_version(weed_plant_t *plugin_info) ALLOW_UNUSED;
weed_plant_t *weed_plugin_info_init(weed_bootstrap_f weed_boot,
                                    int32_t weed_abi_min_version, int32_t weed_abi_max_version,
                                    int32_t filter_api_min_version, int32_t weed_filter_api_max_version) ALLOW_UNUSED;
static void weed_plugin_info_add_filter_class(weed_plant_t *plugin_info, weed_plant_t *filter_class) ALLOW_UNUSED;
weed_plant_t *weed_filter_class_init(const char *name, const char *author, int version, int flags, weed_init_f init_func,
                                     weed_process_f process_func, weed_deinit_f deinit_func, weed_plant_t **in_chantmpls, weed_plant_t **out_chantmpls,
                                     weed_plant_t **in_paramtmpls, weed_plant_t **out_paramtmpls) ALLOW_UNUSED;
weed_plant_t *weed_text_init(const char *name, const char *label, const char *def) ALLOW_UNUSED;
weed_plant_t *weed_float_init(const char *name, const char *label, double def, double min, double max) ALLOW_UNUSED;
weed_plant_t *weed_switch_init(const char *name, const char *label, int def) ALLOW_UNUSED;
weed_plant_t *weed_integer_init(const char *name, const char *label, int def, int min, int max) ALLOW_UNUSED;
weed_plant_t *weed_colRGBd_init(const char *name, const char *label, double red, double green, double blue) ALLOW_UNUSED;
weed_plant_t *weed_colRGBi_init(const char *name, const char *label, int red, int green, int blue) ALLOW_UNUSED;
weed_plant_t *weed_radio_init(const char *name, const char *label, int def, int group) ALLOW_UNUSED;
weed_plant_t *weed_string_list_init(const char *name, const char *label, int def, const char **const list) ALLOW_UNUSED;
weed_plant_t *weed_channel_template_init(const char *name, int flags, int *palettes) ALLOW_UNUSED;
weed_plant_t *weed_audio_channel_template_init(const char *name, int flags) ALLOW_UNUSED;
weed_plant_t *weed_filter_class_get_gui(weed_plant_t *filter) ALLOW_UNUSED;
weed_plant_t *weed_parameter_template_get_gui(weed_plant_t *paramt) ALLOW_UNUSED;
weed_plant_t *weed_parameter_get_gui(weed_plant_t *param) ALLOW_UNUSED;
weed_plant_t *weed_out_param_colRGBd_init(const char *name, double red, double green, double blue) ALLOW_UNUSED;
weed_plant_t *weed_out_param_colRGBi_init(const char *name, int red, int green, int blue) ALLOW_UNUSED;
weed_plant_t *weed_out_param_text_init(const char *name, const char *def) ALLOW_UNUSED;
weed_plant_t *weed_out_param_float_init_nominmax(const char *name, double def) ALLOW_UNUSED;
weed_plant_t *weed_out_param_float_init(const char *name, double def, double min, double max) ALLOW_UNUSED;
weed_plant_t *weed_out_param_switch_init(const char *name, int def) ALLOW_UNUSED;
weed_plant_t *weed_out_param_integer_init_nominmax(const char *name, int def) ALLOW_UNUSED;
weed_plant_t *weed_out_param_integer_init(const char *name, int def, int min, int max) ALLOW_UNUSED;
weed_plant_t **weed_clone_plants(weed_plant_t **plants) ALLOW_UNUSED;


///////////////////////////////////////////////////////////
// check if leaf exists and has a value
#define _leaf_exists(plant, key) ((weed_leaf_get(plant, key, 0, NULL) == WEED_SUCCESS) ? 1 : 0)

static int weed_get_api_version(weed_plant_t *plugin_info) {
  // return the FILTER_API version selected by host
  int api_version;
  weed_plant_t *host_info;
  weed_leaf_get(plugin_info, WEED_LEAF_HOST_INFO, 0, &host_info);
  weed_leaf_get(host_info, WEED_LEAF_FILTER_API_VERSION, 0, &api_version);
  return api_version;
}


////////////////////////////////////////////////////////////////////////////////////////////////////

weed_plant_t *weed_plugin_info_init(weed_bootstrap_f weed_boot, int32_t weed_api_min_version,
                                    int32_t weed_api_max_version,
                                    int32_t weed_filter_api_min_version,
                                    int32_t weed_filter_api_max_version
                                   ) {
  /////////////////////////////////////////////////////////
  // get our bootstrap values

  // every plugin should call this at the beginning of its weed_setup(), and return NULL if this function returns NULL
  // otherwise it may add its filter classes to the returned plugin_info, and then return the plugin_info to the host

  weed_default_getter_f weed_default_getp;

  weed_plant_t *host_info = (*weed_boot)(&weed_default_getp, weed_api_min_version, weed_api_max_version,
                                         weed_filter_api_min_version, weed_filter_api_max_version);

  weed_plant_t *plugin_info = NULL;
  int32_t weed_abi_version = WEED_ABI_VERSION;
  int32_t filter_api_version = WEED_API_VERSION;
  weed_error_t err;

  if (host_info == NULL) return NULL; // matching version was not found

  // we must use the default getter to bootstrap our actual API functions

  if ((*weed_default_getp)(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t *)&weed_leaf_get) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t *)&weed_malloc) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t *)&weed_free) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t *)&weed_memset) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t *)&weed_memcpy) != WEED_SUCCESS) return NULL;

  // now we can use the normal get function (weed_leaf_get)

  //////////// get weed api version /////////
  weed_leaf_get(host_info, WEED_LEAF_WEED_API_VERSION, 0, &weed_abi_version);

  // get any additional functions for higher API versions ////////////
  weed_realloc = NULL;
  weed_plant_free = NULL;

  // 2.0
  if (weed_abi_version >= 200) {
    // added weed_realloc
    if (weed_leaf_get(host_info, WEED_LEAF_REALLOC_FUNC, 0, &weed_realloc) != WEED_SUCCESS) return NULL;

    // added weed_calloc
    if (weed_leaf_get(host_info, WEED_LEAF_CALLOC_FUNC, 0, &weed_calloc) != WEED_SUCCESS) return NULL;

    // added weed_memmove
    if (weed_leaf_get(host_info, WEED_LEAF_MEMMOVE_FUNC, 0, &weed_memmove) != WEED_SUCCESS) return NULL;
  }

  // base functions 1.0
  if (weed_leaf_get(host_info, WEED_LEAF_SET_FUNC, 0, &weed_leaf_set) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_PLANT_NEW_FUNC, 0, &weed_plant_new) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_PLANT_LIST_LEAVES_FUNC, 0, &weed_plant_list_leaves) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, 0, &weed_leaf_num_elements) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, 0, &weed_leaf_element_size) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_LEAF_SEED_TYPE_FUNC, 0, &weed_leaf_seed_type) != WEED_SUCCESS) return NULL;
  if (weed_leaf_get(host_info, WEED_LEAF_GET_FLAGS_FUNC, 0, &weed_leaf_get_flags) != WEED_SUCCESS) return NULL;

  weed_leaf_get(host_info, WEED_LEAF_FILTER_API_VERSION, 0, &filter_api_version);

  // base functions 2.0
  if (filter_api_version >= 200) {
    // added weed_plant_free for plugins
    if (weed_leaf_get(host_info, WEED_PLANT_FREE_FUNC, 0, &weed_plant_free) != WEED_SUCCESS) return NULL;
    // added weed_leaf_delete for plugins
    if (weed_leaf_get(host_info, WEED_LEAF_DELETE_FUNC, 0, &weed_leaf_delete) != WEED_SUCCESS) return NULL;
  }

  //////////////////////////////////////////////////////////////////////

  if (_leaf_exists(host_info, WEED_LEAF_PLUGIN_INFO)) {
    err = weed_leaf_get(host_info, WEED_LEAF_PLUGIN_INFO, 0, &plugin_info);
    if (err == WEED_SUCCESS) {
      int32_t type;
      weed_leaf_get(plugin_info, WEED_LEAF_TYPE, 0, &type);
      if (err != WEED_SUCCESS) {
        return NULL;
      }
      if (type != WEED_PLANT_PLUGIN_INFO) plugin_info = NULL;
    } else {
      return NULL;
    }
  }
  if (plugin_info == NULL) {
    plugin_info = weed_plant_new(WEED_PLANT_PLUGIN_INFO);
    if (plugin_info == NULL) return NULL;
  }

  weed_leaf_set(plugin_info, WEED_LEAF_HOST_INFO, WEED_SEED_PLANTPTR, 1, &host_info);
  return plugin_info;
}


weed_plant_t *weed_channel_template_init(const char *name, int flags, int *palettes) {
  int i;
  weed_plant_t *chantmpl = weed_plant_new(WEED_PLANT_CHANNEL_TEMPLATE);

  weed_leaf_set(chantmpl, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(chantmpl, "flags", WEED_SEED_INT, 1, &flags);

  for (i = 0; palettes[i] != WEED_PALETTE_END; i++);
  if (i == 0) weed_leaf_set(chantmpl, "palette_list", WEED_SEED_INT, 0, NULL);
  else weed_leaf_set(chantmpl, "palette_list", WEED_SEED_INT, i, palettes);
  return chantmpl;
}


weed_plant_t *weed_audio_channel_template_init(const char *name, int flags) {
  int wtrue = WEED_TRUE;
  weed_plant_t *chantmpl = weed_plant_new(WEED_PLANT_CHANNEL_TEMPLATE);

  weed_leaf_set(chantmpl, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(chantmpl, "flags", WEED_SEED_INT, 1, &flags);
  weed_leaf_set(chantmpl, "is_audio", WEED_SEED_BOOLEAN, 1, &wtrue);
  return chantmpl;
}


weed_plant_t *weed_filter_class_init(const char *name, const char *author, int version, int flags, weed_init_f init_func,
                                     weed_process_f process_func, weed_deinit_f deinit_func, weed_plant_t **in_chantmpls, weed_plant_t **out_chantmpls,
                                     weed_plant_t **in_paramtmpls, weed_plant_t **out_paramtmpls) {
  int i;
  weed_plant_t *filter_class = weed_plant_new(WEED_PLANT_FILTER_CLASS);
  if (filter_class == NULL) return NULL;

  weed_leaf_set(filter_class, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(filter_class, "author", WEED_SEED_STRING, 1, &author);
  weed_leaf_set(filter_class, "version", WEED_SEED_INT, 1, &version);
  weed_leaf_set(filter_class, "flags", WEED_SEED_INT, 1, &flags);

  if (init_func != NULL) {
    weed_leaf_set(filter_class, WEED_LEAF_INIT_FUNC, WEED_SEED_FUNCPTR, 1, &init_func);
  }
  if (process_func != NULL) {
    weed_leaf_set(filter_class, WEED_LEAF_PROCESS_FUNC, WEED_SEED_FUNCPTR, 1, &process_func);
  }
  if (deinit_func != NULL) {
    weed_leaf_set(filter_class, WEED_LEAF_DEINIT_FUNC, WEED_SEED_VOIDPTR, 1, &deinit_func);
  }

  if (in_chantmpls == NULL || in_chantmpls[0] == NULL) weed_leaf_set(filter_class, "in_channel_templates", WEED_SEED_VOIDPTR, 0, NULL);
  else {
    for (i = 0; in_chantmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, "in_channel_templates", WEED_SEED_PLANTPTR, i, in_chantmpls);
  }

  if (out_chantmpls == NULL || out_chantmpls[0] == NULL) weed_leaf_set(filter_class, "out_channel_templates", WEED_SEED_VOIDPTR, 0, NULL);
  else {
    for (i = 0; out_chantmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, "out_channel_templates", WEED_SEED_PLANTPTR, i, out_chantmpls);
  }

  if (in_paramtmpls == NULL || in_paramtmpls[0] == NULL) weed_leaf_set(filter_class, "in_parameter_templates", WEED_SEED_VOIDPTR, 0, NULL);
  else {
    for (i = 0; in_paramtmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, "in_parameter_templates", WEED_SEED_PLANTPTR, i, in_paramtmpls);
  }

  if (out_paramtmpls == NULL || out_paramtmpls[0] == NULL) weed_leaf_set(filter_class, "out_parameter_templates", WEED_SEED_VOIDPTR, 0, NULL);
  else {
    for (i = 0; out_paramtmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, "out_parameter_templates", WEED_SEED_PLANTPTR, i, out_paramtmpls);
  }

  return filter_class;
}


static void weed_plugin_info_add_filter_class(weed_plant_t *plugin_info, weed_plant_t *filter_class) {
  int num_filters = 0, i;
  weed_plant_t **filters;

  if (_leaf_exists(plugin_info, "filters")) num_filters = weed_leaf_num_elements(plugin_info, "filters");
  filters = (weed_plant_t **)weed_malloc((num_filters + 1) * sizeof(weed_plant_t *));
  if (filters == NULL) return;
  for (i = 0; i < num_filters; i++) weed_leaf_get(plugin_info, "filters", i, &filters[i]);
  filters[i] = filter_class;
  weed_leaf_set(plugin_info, "filters", WEED_SEED_PLANTPTR, i + 1, filters);
  weed_leaf_set(filter_class, "plugin_info", WEED_SEED_PLANTPTR, 1, &plugin_info);
  weed_free(filters);
}


weed_plant_t *weed_parameter_template_get_gui(weed_plant_t *paramt) {
  weed_plant_t *gui;

  if (_leaf_exists(paramt, "gui")) {
    weed_leaf_get(paramt, "gui", 0, &gui);
    return gui;
  }

  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_leaf_set(paramt, "gui", WEED_SEED_PLANTPTR, 1, &gui);
  return gui;
}


weed_plant_t *weed_filter_class_get_gui(weed_plant_t *filter) {
  weed_plant_t *gui;

  if (_leaf_exists(filter, "gui")) {
    weed_leaf_get(filter, "gui", 0, &gui);
    return gui;
  }

  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_leaf_set(filter, "gui", WEED_SEED_PLANTPTR, 1, &gui);
  return gui;
}


weed_plant_t *weed_parameter_get_gui(weed_plant_t *param) {
  weed_plant_t *xtemplate;

  if (_leaf_exists(param, "template")) {
    weed_leaf_get(param, "template", 0, &xtemplate);
    return weed_parameter_template_get_gui(xtemplate);
  }
  return NULL;
}


//////////////////////////////////////////////////////////////////////////////////////////////

weed_plant_t *weed_integer_init(const char *name, const char *label, int def, int min, int max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_INTEGER;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, "min", WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_INT, 1, &max);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_string_list_init(const char *name, const char *label, int def, const char **const list) {
  int i = 0;
  weed_plant_t *paramt, *gui;
  int min = 0;

  while (list[i] != NULL) i++;
  i--;

  if (def <= -1) min = def = -1;

  paramt = weed_integer_init(name, label, def, min, i);
  gui = weed_parameter_template_get_gui(paramt);

  weed_leaf_set(gui, "choices", WEED_SEED_STRING, i + 1, list);

  return paramt;
}


weed_plant_t *weed_switch_init(const char *name, const char *label, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_SWITCH;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_BOOLEAN, 1, &def);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_radio_init(const char *name, const char *label, int def, int group) {
  weed_plant_t *paramt = weed_switch_init(name, label, def);
  weed_leaf_set(paramt, "group", WEED_SEED_INT, 1, &group);
  return paramt;
}


weed_plant_t *weed_float_init(const char *name, const char *label, double def, double min, double max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_FLOAT;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, "min", WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_DOUBLE, 1, &max);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_text_init(const char *name, const char *label, const char *def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_TEXT;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_STRING, 1, &def);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_colRGBi_init(const char *name, const char *label, int red, int green, int blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  int def[3] = {red, green, blue};
  int min = 0;
  int max = 255;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "colorspace", WEED_SEED_INT, 1, &cspace);

  weed_leaf_set(paramt, "default", WEED_SEED_INT, 3, def);
  weed_leaf_set(paramt, "min", WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_INT, 1, &max);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_colRGBd_init(const char *name, const char *label, double red, double green, double blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  double def[3] = {red, green, blue};
  double min = 0.;
  double max = 1.;
  weed_plant_t *gui;
  int wtrue = WEED_TRUE;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "colorspace", WEED_SEED_INT, 1, &cspace);

  weed_leaf_set(paramt, "default", WEED_SEED_DOUBLE, 3, def);
  weed_leaf_set(paramt, "min", WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_DOUBLE, 1, &max);

  gui = weed_parameter_template_get_gui(paramt);
  weed_leaf_set(gui, "label", WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, "use_mnemonic", WEED_SEED_BOOLEAN, 1, &wtrue);

  return paramt;
}


weed_plant_t *weed_out_param_integer_init(const char *name, int def, int min, int max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_INTEGER;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, "min", WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_INT, 1, &max);
  return paramt;
}


weed_plant_t *weed_out_param_integer_init_nominmax(const char *name, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_INTEGER;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_INT, 1, &def);
  return paramt;
}


weed_plant_t *weed_out_param_switch_init(const char *name, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_SWITCH;
  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_BOOLEAN, 1, &def);
  return paramt;
}


weed_plant_t *weed_out_param_float_init(const char *name, double def, double min, double max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_FLOAT;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, "min", WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_DOUBLE, 1, &max);

  return paramt;
}


weed_plant_t *weed_out_param_float_init_nominmax(const char *name, double def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_FLOAT;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_DOUBLE, 1, &def);

  return paramt;
}


weed_plant_t *weed_out_param_text_init(const char *name, const char *def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_TEXT;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "default", WEED_SEED_STRING, 1, &def);

  return paramt;
}


weed_plant_t *weed_out_param_colRGBi_init(const char *name, int red, int green, int blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  int def[3] = {red, green, blue};
  int min = 0;
  int max = 255;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "colorspace", WEED_SEED_INT, 1, &cspace);

  weed_leaf_set(paramt, "default", WEED_SEED_INT, 3, def);
  weed_leaf_set(paramt, "min", WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_INT, 1, &max);

  return paramt;
}


weed_plant_t *weed_out_param_colRGBd_init(const char *name, double red, double green, double blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int hint = WEED_HINT_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  double def[3] = {red, green, blue};
  double min = 0.;
  double max = 1.;

  weed_leaf_set(paramt, "name", WEED_SEED_STRING, 1, &name);
  weed_leaf_set(paramt, "hint", WEED_SEED_INT, 1, &hint);
  weed_leaf_set(paramt, "colorspace", WEED_SEED_INT, 1, &cspace);

  weed_leaf_set(paramt, "default", WEED_SEED_DOUBLE, 3, def);
  weed_leaf_set(paramt, "min", WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, "max", WEED_SEED_DOUBLE, 1, &max);

  return paramt;
}


///////////////////////////////////////////////////////////////////////

static void _weed_clone_leaf(weed_plant_t *from, const char *key, weed_plant_t *to) {
  int i, num = weed_leaf_num_elements(from, key);

  int *datai;
  double *datad;
  char **datac;
  int64_t *datai6;
  void **datav;
  weed_funcptr_t *dataf;
  weed_plant_t **datap;
  size_t stlen;

  int seed_type = weed_leaf_seed_type(from, key);

  if (num == 0) weed_leaf_set(to, key, seed_type, 0, NULL);
  else {
    switch (seed_type) {
    case WEED_SEED_INT:
      datai = (int *)weed_malloc(num * sizeof(int));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datai[i]);
      weed_leaf_set(to, key, WEED_SEED_INT, num, datai);
      weed_free(datai);
      break;
    case WEED_SEED_INT64:
      datai6 = (int64_t *)weed_malloc(num * sizeof(int64_t));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datai6[i]);
      weed_leaf_set(to, key, WEED_SEED_INT64, num, datai6);
      weed_free(datai6);
      break;
    case WEED_SEED_BOOLEAN:
      datai = (int *)weed_malloc(num * sizeof(int));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datai[i]);
      weed_leaf_set(to, key, WEED_SEED_BOOLEAN, num, datai);
      weed_free(datai);
      break;
    case WEED_SEED_DOUBLE:
      datad = (double *)weed_malloc(num * sizeof(double));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datad[i]);
      weed_leaf_set(to, key, WEED_SEED_DOUBLE, num, datad);
      weed_free(datad);
      break;
    case WEED_SEED_FUNCPTR:
      dataf = (weed_funcptr_t *)weed_malloc(num * sizeof(weed_funcptr_t));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &dataf[i]);
      weed_leaf_set(to, key, WEED_SEED_FUNCPTR, num, dataf);
      weed_free(dataf);
      break;
    case WEED_SEED_VOIDPTR:
      datav = (void **)weed_malloc(num * sizeof(void *));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datav[i]);
      weed_leaf_set(to, key, WEED_SEED_VOIDPTR, num, datav);
      weed_free(datav);
      break;
    case WEED_SEED_PLANTPTR:
      datap = (weed_plant_t **)weed_malloc(num * sizeof(weed_plant_t *));
      for (i = 0; i < num; i++) weed_leaf_get(from, key, i, &datap[i]);
      weed_leaf_set(to, key, WEED_SEED_PLANTPTR, num, datap);
      weed_free(datap);
      break;
    case WEED_SEED_STRING:
      datac = (char **)weed_malloc(num * sizeof(char *));
      for (i = 0; i < num; i++) {
        stlen = weed_leaf_element_size(from, key, i);
        datac[i] = (char *)weed_malloc(stlen + 1);
        weed_leaf_get(from, key, i, &datac[i]);
      }
      weed_leaf_set(to, key, WEED_SEED_STRING, num, datac);
      for (i = 0; i < num; i++) weed_free(datac[i]);
      weed_free(datac);
      break;
    }
  }
}


weed_plant_t **weed_clone_plants(weed_plant_t **plants) {
  //plants must be a NULL terminated array
  int i, j, k, type, num_plants;
  weed_plant_t **ret, *gui, *gui2;
  char **leaves, **leaves2;
  for (i = 0; plants[i] != NULL; i++);
  num_plants = i;
  ret = (weed_plant_t **)weed_malloc((num_plants + 1) * sizeof(weed_plant_t *));
  if (ret == NULL) return NULL;

  for (i = 0; i < num_plants; i++) {
    weed_leaf_get(plants[i], "type", 0, &type);
    ret[i] = weed_plant_new(type);
    if (ret[i] == NULL) return NULL;

    leaves = weed_plant_list_leaves(plants[i]);
    for (j = 0; leaves[j] != NULL; j++) {
      if (!strcmp(leaves[j], "gui")) {
        weed_leaf_get(plants[i], "gui", 0, &gui);
        gui2 = weed_plant_new(WEED_PLANT_GUI);
        weed_leaf_set(ret[i], "gui", WEED_SEED_PLANTPTR, 1, &gui2);
        leaves2 = weed_plant_list_leaves(gui);
        for (k = 0; leaves2[k] != NULL; k++) {
          _weed_clone_leaf(gui, leaves2[k], gui2);
          weed_free(leaves2[k]);
        }
        weed_free(leaves2);
      } else _weed_clone_leaf(plants[i], leaves[j], ret[i]);
      //weed_free(leaves[j]);
    }
    weed_free(leaves);
  }
  ret[i] = NULL;
  return ret;
}


#ifdef NEED_ALPHA_SORT // for wrappers, use this to sort files

typedef struct dlink_list dlink_list_t;
struct dlink_list {
  weed_plant_t *filter;
  const char *name;
  dlink_list_t *prev;
  dlink_list_t *next;
};


static dlink_list_t *add_to_list_sorted(dlink_list_t *list, weed_plant_t *filter, const char *name) {
  dlink_list_t *lptr = list;
  dlink_list_t *entry = (dlink_list_t *)weed_malloc(sizeof(dlink_list_t));
  if (entry == NULL) return list;
  entry->filter = filter;
  entry->name = strdup(name);
  entry->next = entry->prev = NULL;
  if (list == NULL) return entry;
  while (lptr != NULL) {
    if (strncasecmp(lptr->name, name, 256) > 0) {
      // lptr is after entry, insert entry before
      if (lptr->prev != NULL) {
        lptr->prev->next = entry;
        entry->prev = lptr->prev;
      }
      lptr->prev = entry;
      entry->next = lptr;
      if (entry->prev == NULL) list = entry;
      break;
    }
    if (lptr->next == NULL) {
      lptr->next = entry;
      entry->prev = lptr;
      break;
    }
    lptr = lptr->next;
  }
  return list;
}


static int add_filters_from_list(weed_plant_t *plugin_info, dlink_list_t *list) {
  int count = 0;
  while (list != NULL) {
    dlink_list_t *listnext = list->next;
    weed_plugin_info_add_filter_class(plugin_info, list->filter);
    weed_free(list);
    list = listnext;
    count++;
  }
  return count;
}

#endif

#ifdef NEED_RANDOM

#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>


static inline uint32_t fastrand(uint32_t oldval) {
  // pseudo-random number generator
#define rand_a 1073741789L
#define rand_c 32749L
  return (rand_a * oldval + rand_c);
}


static double drand(double max) {
  double denom = (double)(2ul << 30) / max;
  double num = (double)lrand48();
  //fprintf(stderr, "rnd %f %f\n", num, denom);
  return (double)(num / denom);
}


static void seed_rand(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand48(tv.tv_sec);
}

#endif

//utilities

#ifndef ABS
#define ABS(a)           (((a) < 0) ? -(a) : (a))
#endif

static inline int myround(double n) {
  return (n >= 0.) ? (int)(n + 0.5) : (int)(n - 0.5);
}


#ifdef NEED_PALETTE_UTILS

static inline int weed_palette_is_alpha(int pal) {
  return (pal >= 1024 && pal < 2048) ? 1 : 0;
}


static inline int weed_palette_is_rgb(int pal) {
  return (pal < 512) ? 1 : 0;
}


static inline int weed_palette_is_yuv(int pal) {
  return (pal >= 512 && pal < 1024) ? 1 : 0;
}


static inline int weed_palette_get_numplanes(int pal) {
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 ||
      pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV411 ||
      pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_AFLOAT || pal == WEED_PALETTE_A8 ||
      pal == WEED_PALETTE_A1 || pal == WEED_PALETTE_RGBFLOAT || pal == WEED_PALETTE_RGBAFLOAT) return 1;
  if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P) return 3;
  if (pal == WEED_PALETTE_YUVA4444P) return 4;
  return 0; // unknown palette
}


static inline int weed_palette_is_valid(int pal) {
  if (weed_palette_get_numplanes(pal) == 0) return 0;
  return 1;
}


static inline int weed_palette_get_bits_per_macropixel(int pal) {
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


static inline int weed_palette_get_pixels_per_macropixel(int pal) {
  if (pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888) return 2;
  if (pal == WEED_PALETTE_YUV411) return 4;
  return 1;
}


static inline int weed_palette_is_float_palette(int pal) {
  return (pal == WEED_PALETTE_RGBAFLOAT || pal == WEED_PALETTE_AFLOAT || pal == WEED_PALETTE_RGBFLOAT) ? 1 : 0;
}


static inline int weed_palette_has_alpha_channel(int pal) {
  return (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 ||
          pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_RGBAFLOAT ||
          weed_palette_is_alpha(pal)) ? 1 : 0;
}


static inline double weed_palette_get_plane_ratio_horizontal(int pal, int plane) {
  // return ratio of plane[n] width/plane[0] width;
  if (plane == 0) return 1.0;
  if (plane == 1 || plane == 2) {
    if (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) return 1.0;
    if (pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane == 3) {
    if (pal == WEED_PALETTE_YUVA4444P) return 1.0;
  }
  return 0.0;
}


static inline double weed_palette_get_plane_ratio_vertical(int pal, int plane) {
  // return ratio of plane[n] height/plane[n] height
  if (plane == 0) return 1.0;
  if (plane == 1 || plane == 2) {
    if (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUV422P) return 1.0;
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane == 3) {
    if (pal == WEED_PALETTE_YUVA4444P) return 1.0;
  }
  return 0.0;
}


/* precomputed tables */
#define FP_BITS 16

static int Y_Ru[256];
static int Y_Gu[256];
static int Y_Bu[256];
static int Cb_Ru[256];
static int Cb_Gu[256];
static int Cb_Bu[256];
static int Cr_Ru[256];
static int Cr_Gu[256];
static int Cr_Bu[256];
static int YCL_YUCL[256];
static int UVCL_UVUCL[256];
static int YUCL_YCL[256];

static int yuv_rgb_inited = 0;
static int y_y_inited = 0;


static void init_RGB_to_YCbCr_tables(void) {
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-pogint-factor)
   *
   * to one of each, add the following:
   *             + (fixed-pogint-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-pogint-factor)  --- to add the offset
   *
   */
  for (i = 0; i < 256; i++) {
    Y_Ru[i] = myround(0.299 * (double)i
                      * (1 << FP_BITS));
    Y_Gu[i] = myround(0.587 * (double)i
                      * (1 << FP_BITS));
    Y_Bu[i] = myround(0.114 * (double)i
                      * (1 << FP_BITS));


    Cb_Bu[i] = myround(-0.168736 * (double)i
                       * (1 << FP_BITS));
    Cb_Gu[i] = myround(-0.331264 * (double)i
                       * (1 << FP_BITS));
    Cb_Ru[i] = myround((0.500 * (double)i
                        + 128.) * (1 << FP_BITS));

    Cr_Bu[i] = myround(0.500 * (double)i
                       * (1 << FP_BITS));
    Cr_Gu[i] = myround(-0.418688 * (double)i
                       * (1 << FP_BITS));
    Cr_Ru[i] = myround((-0.081312 * (double)i
                        + 128.) * (1 << FP_BITS));
  }

  yuv_rgb_inited = 1;
}


// unclamped values
#define RGB_2_YIQ(a, b, c)			\
  do { \
    int i; \
    register short x; \
    \
    for (i = 0; i < NUM_PIXELS_SQUARED; i++) { \
      Unit Y, I, Q; \
      if ((x=((Y_Ru[(int)a[i]]+Y_Gu[(int)b[i]]+Y_Bu[(int)(int)c[i]])>>FP_BITS))>255) x=255; \
      Y=x<0?0:x; \
      if ((x=((Cr_Ru[(int)a[i]]+Cr_Gu[(int)b[i]]+Cr_Bu[(int)c[i]])>>FP_BITS))>255) x=255; \
      I=x<0?0:x; \
      if ((x=((Cb_Ru[(int)a[i]]+Cb_Gu[(int)b[i]]+Cb_Bu[(int)c[i]])>>FP_BITS))>255) x=255; \
      Q=x<0?0:x; \
      a[i] = Y; \
      b[i] = I; \
      c[i] = Q; \
    } \
  } while(0)


static void init_Y_to_Y_tables(void) {
  register int i;

  for (i = 0; i < 17; i++) {
    UVCL_UVUCL[i] = YCL_YUCL[i] = 0;
    YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;
  }

  for (i = 17; i < 235; i++) {
    YCL_YUCL[i] = (int)((float)(i - 16.) / 219. * 255. + .5);
    UVCL_UVUCL[i] = (int)((float)(i - 16.) / 224. * 255. + .5);
    YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;
  }

  for (i = 235; i < 256; i++) {
    UVCL_UVUCL[i] = YCL_YUCL[i] = 255;
    YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;
  }

  y_y_inited = 1;
}


static inline uint8_t calc_luma(uint8_t *pixel, int palette, int yuv_clamping) {
  if (!yuv_rgb_inited) init_RGB_to_YCbCr_tables();

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_RGBA32:
    return (Y_Ru[pixel[0]] + Y_Gu[pixel[1]] + Y_Bu[pixel[2]]) >> FP_BITS;
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_BGRA32:
    return (Y_Ru[pixel[2]] + Y_Gu[pixel[1]] + Y_Bu[pixel[0]]) >> FP_BITS;
  case WEED_PALETTE_ARGB32:
    return (Y_Ru[pixel[1]] + Y_Gu[pixel[2]] + Y_Bu[pixel[3]]) >> FP_BITS;
  default:
    // yuv
    if (yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED)
      return pixel[0];
    else {
      if (!y_y_inited) init_Y_to_Y_tables();
      return YCL_YUCL[pixel[0]];
    }
  }
  return 0;
}


static size_t blank_pixel(uint8_t *dst, int pal, int yuv_clamping, uint8_t *src) {
  // set src to non-null to preserve the alpha channel (if applicable)
  // yuv_clamping
  // only valid for non-planar (packed) palettes
  unsigned char *idst = dst;
  uint8_t y_black = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16;

  switch (pal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    dst[0] = dst[1] = dst[2] = 0;
    dst += 3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    dst[0] = dst[1] = dst[2] = 0;
    dst[3] = src == NULL ? 255 : src[3];
    dst += 4;
    break;
  case WEED_PALETTE_ARGB32:
    dst[1] = dst[2] = dst[3] = 0;
    dst[0] = src == NULL ? 255 : src[0];
    dst += 4;
    break;
  case WEED_PALETTE_UYVY8888:
    dst[1] = dst[3] = y_black;
    dst[0] = dst[2] = 128;
    dst += 4;
    break;
  case WEED_PALETTE_YUYV8888:
    dst[0] = dst[2] = y_black;
    dst[1] = dst[3] = 128;
    dst += 4;
    break;
  case WEED_PALETTE_YUV888:
    dst[0] = y_black;
    dst[1] = dst[2] = 128;
    dst += 3;
    break;
  case WEED_PALETTE_YUVA8888:
    dst[0] = y_black;
    dst[1] = dst[2] = 128;
    dst[3] = src == NULL ? 255 : src[3];
    dst += 4;
    break;
  case WEED_PALETTE_YUV411:
    dst[0] = dst[3] = 128;
    dst[1] = dst[2] = dst[4] = dst[5] = y_black;
    dst += 6;
  default:
    break;
  }

  return dst - idst;
}


static void blank_row(uint8_t **pdst, int width, int pal, int yuv_clamping, int ycopy, uint8_t **psrc) {
  // for YUV420 and YVU420, only set ycopy for even rows, and increment pdst[1], pdst[2] on the odd rows
  int nplanes, p, mpsize;
  uint8_t *dst = *pdst, *src;
  uint8_t black[3];

  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {
    weed_memset(dst, 0, width * 3);
    return;
  }

  nplanes = weed_palette_get_numplanes(pal);
  if (!ycopy && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P)) nplanes = 1;

  black[0] = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16;
  black[1] = black[2] = 128;

  for (p = 0; p < nplanes; p++) {
    dst = pdst[p];
    src = psrc[p];

    if (p == 3) {
      // copy planar alpha
      weed_memcpy(dst, src, width);
      break;
    }
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P ||
        pal == WEED_PALETTE_YUVA4444P) {
      // yuv plane, set to black
      weed_memset(dst, black[p], width);
    } else {
      // RGBA, BGRA, ARGB, YUV888, YUVA8888, UYVY, YUYV, YUV411
      int i;
      for (i = 0; i < width; i++) {
        mpsize = blank_pixel(dst, pal, yuv_clamping, src);
        dst += mpsize;
        if (src != NULL) src += mpsize;
      }
      break;
    }

    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P)) {
      width >>= 1;
    }
  }
}

#endif
