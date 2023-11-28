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

/* (C) G. Finch, 2005 - 2023*/

#ifdef __WEED_HOST__
#error This file is intended only for Weed plugins
#endif

#ifdef __WEED_PLUGIN__

#ifndef __HAVE_WEED_PLUGIN_UTILS__
#define __HAVE_WEED_PLUGIN_UTILS__

#ifndef __WEED_PLUGIN_UTILS_H__
#define NO_PLUGIN_UTILS_CODE
#include <weed/weed-plugin-utils.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////
static int wtrue = WEED_TRUE;
#define DEF_FN static inline
#define wlne(p,k) weed_leaf_num_elements((p),(k))
#define wlg(p,w,i,v) weed_leaf_get((p),(w),(i),(v))
#define wls(p,w,t,n,v) weed_leaf_set((p),(w),(t),(n),(v))
#define _leaf_has_value(p,k) ((wlne(p,k)>0)?1:0)
#define gg(p,w,i,v) (p?((wlg((p),(w),(i),(v))==WEED_SUCCESS)?v:0):0)
#define __WPFC__ WEED_PLANT_FILTER_CLASS
#define __WPPT__ WEED_PLANT_PARAMETER_TEMPLATE
#define __WPCT__ WEED_PLANT_CHANNEL_TEMPLATE
#define __WPP__ WEED_PLANT_PARAMETER
#define __WPG__ WEED_PLANT_GUI
#define __WPC__ WEED_PLANT_CHANNEL
#define __WPFI__ WEED_PLANT_FILTER_INSTANCE
DEF_FN int gg_i(weed_plant_t *p, const char *w)
{int v, *vp = (int *)gg(p, w, 0, &v); return vp ? v : 0;}
DEF_FN weed_plant_t *gg_p(weed_plant_t *p, const char *w, int i)
{weed_plant_t *v, **vp = (weed_plant_t **)gg(p, w, i, &v); return vp ? v : 0;}
DEF_FN int64_t gg_i64(weed_plant_t *p, const char *w)
{int64_t v, *vp = (int64_t *)gg(p, w, 0, &v); return vp ? v : 0;}
DEF_FN int weed_plant_get_type(weed_plant_t *p) {return gg_i(p, WEED_LEAF_TYPE);}
DEF_FN weed_plant_t *_weed_get_gui(weed_plant_t *p) {
  weed_plant_t *g = NULL; int t = weed_plant_get_type(p);
  if (t != __WPFC__ && t != __WPPT__ && t != __WPP__ && t != __WPC__ && t != __WPFI__) return NULL;
  gg(p, WEED_LEAF_GUI, 0, (void *)&g);
  if (!g) {
    g = weed_plant_new(__WPG__);
    wls(p, WEED_LEAF_GUI, WEED_SEED_PLANTPTR, 1, &g);
  } return g;
}
DEF_FN double gg_dbl(weed_plant_t *p, const char *w)
{double v, *vp = (double *)gg(p, w, 0, &v); return vp ? v : 0;}
DEF_FN int weed_get_api_version(weed_plant_t *pi)
{weed_plant_t *h; wlg(pi, WEED_LEAF_HOST_INFO, 0, &h); return gg_i(h, WEED_LEAF_FILTER_API_VERSION);}
DEF_FN void _weed_plant_set_flags(weed_plant_t *p, int f) {
  int t = weed_plant_get_type(p);
  if (t == __WPFC__ || t == __WPPT__ || t == __WPCT__ || t == WEED_PLANT_GUI)
    wls(p, WEED_LEAF_FLAGS, WEED_SEED_INT, 1, &f);
}
DEF_FN void weed_filter_set_flags(weed_plant_t *f, int fl) {_weed_plant_set_flags(f, fl);}
DEF_FN void weed_chantmpl_set_flags(weed_plant_t *c, int f) {_weed_plant_set_flags(c, f);}
DEF_FN void weed_paramtmpl_set_flags(weed_plant_t *p, int f) {_weed_plant_set_flags(p, f);}
DEF_FN void weed_gui_set_flags(weed_plant_t *g, int f) {_weed_plant_set_flags(g, f);}
DEF_FN void _weed_plant_set_name(weed_plant_t *p, const char *n) {
  int t = weed_plant_get_type(p);
  if (t == __WPFC__ || t == __WPPT__ || t == __WPCT__) wls(p, WEED_LEAF_NAME, WEED_SEED_STRING, 1, &n);
}
DEF_FN void weed_filter_set_name(weed_plant_t *f, const char *n) {_weed_plant_set_name(f, n);}
DEF_FN void weed_filter_set_description(weed_plant_t *f, const char *d) {
  wls(f, WEED_LEAF_DESCRIPTION, WEED_SEED_STRING, 1, &d);
}
DEF_FN void weed_chantmpl_set_name(weed_plant_t *c, const char *n) {_weed_plant_set_name(c, n);}
DEF_FN void weed_paramtmpl_set_name(weed_plant_t *p, const char *n) {_weed_plant_set_name(p, n);}
DEF_FN weed_error_t weed_paramtmpl_set_hidden(weed_plant_t *pt, int h) {
  return wls(_weed_get_gui(pt), WEED_LEAF_HIDDEN, WEED_SEED_BOOLEAN, 1, &h);
}
DEF_FN weed_error_t weed_param_set_hidden(weed_plant_t *p, int h) {
  return wls(_weed_get_gui(p), WEED_LEAF_HIDDEN, WEED_SEED_BOOLEAN, 1, &h);
}
DEF_FN weed_error_t weed_paramtmpl_declare_transition(weed_plant_t *pt) {
  return wls(pt, WEED_LEAF_IS_TRANSITION, WEED_SEED_BOOLEAN, 1, &wtrue);
}
DEF_FN weed_error_t weed_plugin_set_package_version(weed_plant_t *pi, int v) {
  return  wls(pi, WEED_LEAF_VERSION, WEED_SEED_INT, 1, &v);
}
DEF_FN weed_plant_t *weed_filter_get_gui(weed_plant_t *f) {return _weed_get_gui(f);}
DEF_FN weed_plant_t *weed_param_get_gui(weed_plant_t *p) {return _weed_get_gui(p);}
DEF_FN weed_plant_t *weed_channel_get_gui(weed_plant_t *c) {return _weed_get_gui(c);}
DEF_FN weed_plant_t *weed_paramtmpl_get_gui(weed_plant_t *pt) {return _weed_get_gui(pt);}
DEF_FN weed_plant_t *weed_instance_get_gui(weed_plant_t *i) {return _weed_get_gui(i);}
DEF_FN weed_plant_t *weed_get_host_info(weed_plant_t *pi) {
  weed_plant_t *hi; return *((weed_plant_t **)(gg(pi, WEED_LEAF_HOST_INFO, 0, (void *)&hi)));
}
DEF_FN int weed_get_host_verbosity(weed_plant_t *hi) {return gg_i(hi, WEED_LEAF_VERBOSITY);}
DEF_FN int _weed_plant_get_flags(weed_plant_t *p) {return gg_i(p, WEED_LEAF_FLAGS);}
DEF_FN int weed_host_get_flags(weed_plant_t *h) {return _weed_plant_get_flags(h);}
DEF_FN int weed_filter_get_flags(weed_plant_t *f) {return _weed_plant_get_flags(f);}
DEF_FN int weed_filter_get_version(weed_plant_t *f) {return gg_i(f, WEED_LEAF_VERSION);}
DEF_FN int weed_gui_get_flags(weed_plant_t *g) {return _weed_plant_get_flags(g);}
DEF_FN int weed_chantmpl_get_flags(weed_plant_t *c) {return _weed_plant_get_flags(c);}
DEF_FN int weed_paramtmpl_get_type(weed_plant_t *p) {return gg_i(p, WEED_LEAF_PARAM_TYPE);}
DEF_FN int weed_paramtmpl_get_flags(weed_plant_t *p) {return _weed_plant_get_flags(p);}
DEF_FN int weed_instance_get_flags(weed_plant_t *i) {return _weed_plant_get_flags(i);}
DEF_FN int weed_host_supports_linear_gamma(weed_plant_t *h)
{return (weed_host_get_flags(h) & WEED_HOST_SUPPORTS_LINEAR_GAMMA) ? 1 : 0;}
DEF_FN int weed_host_supports_premultiplied_alpha(weed_plant_t *h)
{return (weed_host_get_flags(h) & WEED_HOST_SUPPORTS_PREMULTIPLIED_ALPHA) ? 1 : 0;}
DEF_FN weed_plant_t *weed_instance_get_filter(weed_plant_t *i)
{weed_plant_t *f; return *((weed_plant_t **)gg(i, WEED_LEAF_FILTER_CLASS, 0, (void *)&f));}
DEF_FN weed_plant_t *weed_get_in_channel(weed_plant_t *i, int x)
{return gg_p(i, WEED_LEAF_IN_CHANNELS, x);}
DEF_FN weed_plant_t *weed_get_out_channel(weed_plant_t *i, int x)
{return gg_p(i, WEED_LEAF_OUT_CHANNELS, x);}
DEF_FN weed_plant_t *weed_get_in_param(weed_plant_t *i, int x)
{return gg_p(i, WEED_LEAF_IN_PARAMETERS, x);}
DEF_FN weed_plant_t *weed_get_out_param(weed_plant_t *i, int x)
{return gg_p(i, WEED_LEAF_OUT_PARAMETERS, x);}
DEF_FN void *weed_channel_get_pixel_data(weed_plant_t *c)
{void *pd; return *((void **)(gg(c, WEED_LEAF_PIXEL_DATA, 0, (void *)&pd)));}
DEF_FN int weed_channel_get_width(weed_plant_t *c) {return gg_i(c, WEED_LEAF_WIDTH);}
DEF_FN int weed_channel_get_height(weed_plant_t *c) {return gg_i(c, WEED_LEAF_HEIGHT);}
DEF_FN int weed_channel_get_palette(weed_plant_t *c) {return gg_i(c, WEED_LEAF_CURRENT_PALETTE);}
DEF_FN int weed_channel_get_yuv_clamping(weed_plant_t *c) {return gg_i(c, WEED_LEAF_YUV_CLAMPING);}
DEF_FN int weed_channel_get_stride(weed_plant_t *c) {return gg_i(c, WEED_LEAF_ROWSTRIDES);}
DEF_FN int weed_channel_get_offset(weed_plant_t *c) {return gg_i(c, WEED_LEAF_OFFSET);}
DEF_FN int weed_channel_get_real_height(weed_plant_t *c)
{int h; return *((int *)(gg(c, WEED_LEAF_HEIGHT, wlne(c, WEED_LEAF_HEIGHT) - 1, &h)));}
DEF_FN int weed_channel_is_disabled(weed_plant_t *c) {return gg_i(c, WEED_LEAF_DISABLED);}
DEF_FN weed_plant_t *weed_param_get_template(weed_plant_t *p)
{weed_plant_t *pt; return *((weed_plant_t **)(gg(p, WEED_LEAF_TEMPLATE, 0, (void *)&pt)));}
DEF_FN int weed_param_get_type(weed_plant_t *p) {return weed_paramtmpl_get_type(weed_param_get_template(p));}
////////////////////////////////////////////////////////////////////////////////////////////////////

static weed_plant_t *weed_plugin_info_init(weed_bootstrap_f weed_boot, int32_t weed_api_min_version,
    int32_t weed_api_max_version,
    int32_t weed_filter_api_min_version,
    int32_t weed_filter_api_max_version) {
  /////////////////////////////////////////////////////////
  // get our bootstrap values

  // every plugin should call this at the beginning of its weed_setup(), and return NULL if this function returns NULL
  // otherwise it may add its filter classes to the returned plugin_info, and then return the plugin_info to the host

  // if using the standard headers, then the plugin can use the macro WEED_SETUP_START(int weed_api_version, int filter_api_version)
  // and this function will be called and the return if NULL will be handled

  weed_default_getter_f weed_default_getp;

  weed_plant_t *host_info = (*weed_boot)(&weed_default_getp, weed_api_min_version, weed_api_max_version,
                                         weed_filter_api_min_version, weed_filter_api_max_version);

  weed_plant_t *plugin_info = NULL;
  int32_t weed_abi_version = WEED_ABI_VERSION;
  int32_t filter_api_version = WEED_API_VERSION;
  weed_error_t err;

  if (!host_info) {
    fprintf(stderr, "NO MATCHING FILTER API FOUND\n");
    return NULL; // matching version was not found
  }
  // we must use the default getter to bootstrap our actual API functions

  //////////// get weed api version /////////
  if ((*weed_default_getp)(host_info, WEED_LEAF_WEED_API_VERSION,
                           (weed_funcptr_t *)&weed_abi_version) != WEED_SUCCESS) return NULL;

if ((*weed_default_getp)(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t *)&weed_leaf_get) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t *)&weed_malloc) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t *)&weed_free) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t *)&weed_memset) != WEED_SUCCESS) return NULL;
  if ((*weed_default_getp)(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t *)&weed_memcpy) != WEED_SUCCESS) return NULL;

  // now we can use the normal get function (weed_leaf_get)

  // get any additional functions for higher API versions ////////////
  weed_realloc = NULL;
  weed_plant_free = NULL;

  // 2.0
  if (weed_abi_version >= 200) {
    if (weed_leaf_get(host_info, WEED_LEAF_REALLOC_FUNC, 0, &weed_realloc) != WEED_SUCCESS) return NULL;
    if (weed_leaf_get(host_info, WEED_LEAF_CALLOC_FUNC, 0, &weed_calloc) != WEED_SUCCESS) return NULL;
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

  if (_leaf_has_value(host_info, WEED_LEAF_PLUGIN_INFO)) {
    if ((err = weed_leaf_get(host_info, WEED_LEAF_PLUGIN_INFO, 0, &plugin_info)) == WEED_SUCCESS) {
      int32_t type;
      weed_leaf_get(plugin_info, WEED_LEAF_TYPE, 0, &type);
      if (err != WEED_SUCCESS) return NULL;
      if (type != WEED_PLANT_PLUGIN_INFO) plugin_info = NULL;
    } else return NULL;
  }

  if (!plugin_info) if (!(plugin_info = weed_plant_new(WEED_PLANT_PLUGIN_INFO))) return NULL;
  weed_leaf_set(plugin_info, WEED_LEAF_HOST_INFO, WEED_SEED_PLANTPTR, 1, &host_info);
  return plugin_info;
}

static weed_plant_t *weed_channel_template_init(const char *name, int flags) {
  weed_plant_t *chantmpl = weed_plant_new(WEED_PLANT_CHANNEL_TEMPLATE);
  if (!chantmpl || !name) return NULL;
  weed_chantmpl_set_name(chantmpl, name); weed_chantmpl_set_flags(chantmpl, flags);
  return chantmpl;
}

static weed_plant_t *weed_filter_class_init(const char *name, const char *author, int version, int flags, int *palettes,
    weed_init_f init_func, weed_process_f process_func, weed_deinit_f deinit_func,
    weed_plant_t **in_chantmpls, weed_plant_t **out_chantmpls,
    weed_plant_t **in_paramtmpls, weed_plant_t **out_paramtmpls) {
  int i;
  weed_plant_t *filter_class = NULL;
  if (name) filter_class = weed_plant_new(WEED_PLANT_FILTER_CLASS);
  if (!filter_class) return NULL;
  weed_filter_set_name(filter_class, name);
  weed_leaf_set(filter_class, WEED_LEAF_AUTHOR, WEED_SEED_STRING, 1, &author);
  weed_leaf_set(filter_class, WEED_LEAF_VERSION, WEED_SEED_INT, 1, &version);
  weed_filter_set_flags(filter_class, flags);

  if (init_func) weed_leaf_set(filter_class, WEED_LEAF_INIT_FUNC, WEED_SEED_FUNCPTR, 1, &init_func);
  if (process_func) weed_leaf_set(filter_class, WEED_LEAF_PROCESS_FUNC, WEED_SEED_FUNCPTR, 1, &process_func);
  if (deinit_func) weed_leaf_set(filter_class, WEED_LEAF_DEINIT_FUNC, WEED_SEED_FUNCPTR, 1, &deinit_func);

  if (!in_chantmpls || !in_chantmpls[0])
    weed_leaf_set(filter_class, WEED_LEAF_IN_CHANNEL_TEMPLATES, WEED_SEED_PLANTPTR, 0, NULL);
  else {
    for (i = 0; in_chantmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, WEED_LEAF_IN_CHANNEL_TEMPLATES, WEED_SEED_PLANTPTR, i, in_chantmpls);
  }
  if (!out_chantmpls || !out_chantmpls[0])
    weed_leaf_set(filter_class, WEED_LEAF_OUT_CHANNEL_TEMPLATES, WEED_SEED_PLANTPTR, 0, NULL);
  else {
    for (i = 0; out_chantmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, WEED_LEAF_OUT_CHANNEL_TEMPLATES, WEED_SEED_PLANTPTR, i, out_chantmpls);
  }
  if (!in_paramtmpls || !in_paramtmpls[0])
    weed_leaf_set(filter_class, WEED_LEAF_IN_PARAMETER_TEMPLATES, WEED_SEED_PLANTPTR, 0, NULL);
  else {
    for (i = 0; in_paramtmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, WEED_LEAF_IN_PARAMETER_TEMPLATES, WEED_SEED_PLANTPTR, i, in_paramtmpls);
  }
  if (!out_paramtmpls || !out_paramtmpls[0])
    weed_leaf_set(filter_class, WEED_LEAF_OUT_PARAMETER_TEMPLATES, WEED_SEED_PLANTPTR, 0, NULL);
  else {
    for (i = 0; out_paramtmpls[i] != NULL; i++);
    weed_leaf_set(filter_class, WEED_LEAF_OUT_PARAMETER_TEMPLATES, WEED_SEED_PLANTPTR, i, out_paramtmpls);
  }
  if (palettes) {
    for (i = 0; palettes[i] != WEED_PALETTE_END; i++);
    if (i == 0) weed_leaf_set(filter_class, WEED_LEAF_PALETTE_LIST, WEED_SEED_INT, 0, NULL);
    weed_leaf_set(filter_class, WEED_LEAF_PALETTE_LIST, WEED_SEED_INT, i, palettes);
  }
  return filter_class;
}

static void weed_plugin_info_add_filter_class(weed_plant_t *plugin_info, weed_plant_t *filter_class) {
  weed_plant_t **filters;
  weed_size_t num_filters = 0;
  weed_size_t  i;
  if (_leaf_has_value(plugin_info, WEED_LEAF_FILTERS)) num_filters = weed_leaf_num_elements(plugin_info, WEED_LEAF_FILTERS);
  filters = (weed_plant_t **)weed_malloc((num_filters + 1) * sizeof(weed_plant_t *));
  if (!filters) return;
  for (i = 0; i < num_filters; i++) weed_leaf_get(plugin_info, WEED_LEAF_FILTERS, i, &filters[i]);
  filters[i] = filter_class;
  weed_leaf_set(plugin_info, WEED_LEAF_FILTERS, WEED_SEED_PLANTPTR, i + 1, filters);
  weed_leaf_set(filter_class, WEED_LEAF_PLUGIN_INFO, WEED_SEED_PLANTPTR, 1, &plugin_info);
  weed_free(filters);
}

//////////////////////////////////////////////////////////////////////////////////////////////

static weed_plant_t *weed_integer_init(const char *name, const char *label, int def, int min, int max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_INTEGER;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_string_list_init(const char *name, const char *label, int def, const char **const list) {
  int i = 0;
  weed_plant_t *paramt, *gui;
  int min = 0;
  while (list[i] != NULL) i++;
  i--;
  if (def <= -1) min = def = -1;
  paramt = weed_integer_init(name, label, def, min, i);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_CHOICES, WEED_SEED_STRING, i + 1, list);
  return paramt;
}

static weed_plant_t *weed_switch_init(const char *name, const char *label, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_SWITCH;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_radio_init(const char *name, const char *label, int def, int group) {
  weed_plant_t *paramt = weed_switch_init(name, label, def);
  weed_leaf_set(paramt, WEED_LEAF_GROUP, WEED_SEED_INT, 1, &group);
  return paramt;
}

static weed_plant_t *weed_float_init(const char *name, const char *label, double def, double min, double max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_FLOAT;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_text_init(const char *name, const char *label, const char *def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_TEXT;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_colRGBi_init(const char *name, const char *label, int red, int green, int blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  int def[3] = {red, green, blue}, min = 0, max = 255;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_COLORSPACE, WEED_SEED_INT, 1, &cspace);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 3, def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_colRGBd_init(const char *name, const char *label, double red, double green, double blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  double def[3] = {red, green, blue}, min = 0., max = 1.;
  weed_plant_t *gui;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_COLORSPACE, WEED_SEED_INT, 1, &cspace);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 3, def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  gui = weed_paramtmpl_get_gui(paramt);
  weed_leaf_set(gui, WEED_LEAF_LABEL, WEED_SEED_STRING, 1, &label);
  weed_leaf_set(gui, WEED_LEAF_USE_MNEMONIC, WEED_SEED_BOOLEAN, 1, &wtrue);
  return paramt;
}

static weed_plant_t *weed_out_param_integer_init(const char *name, int def, int min, int max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_INTEGER;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  return paramt;
}

static weed_plant_t *weed_out_param_integer_init_nominmax(const char *name, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_INTEGER;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  return paramt;
}

static weed_plant_t *weed_out_param_switch_init(const char *name, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_SWITCH;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def);
  return paramt;
}

static weed_plant_t *weed_out_param_float_init(const char *name, double def, double min, double max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_FLOAT;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  return paramt;
}

static weed_plant_t *weed_out_param_float_init_nominmax(const char *name, double def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_FLOAT;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  return paramt;
}

static weed_plant_t *weed_out_param_text_init(const char *name, const char *def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_TEXT;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def);
  return paramt;
}

static weed_plant_t *weed_out_param_colRGBi_init(const char *name, int red, int green, int blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  int def[3] = {red, green, blue}, min = 0, max = 255;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_COLORSPACE, WEED_SEED_INT, 1, &cspace);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 3, def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  return paramt;
}

static weed_plant_t *weed_out_param_colRGBd_init(const char *name, double red, double green, double blue) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_COLOR;
  int cspace = WEED_COLORSPACE_RGB;
  double def[3] = {red, green, blue}, min = 0, max = 1.;
  weed_paramtmpl_set_name(paramt, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_COLORSPACE, WEED_SEED_INT, 1, &cspace);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 3, def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  return paramt;
}

///////////////////////////////////////////////////////////////////////

#ifdef NEED_AUDIO
DEF_FN int weed_channel_get_audio_rate(weed_plant_t *channel) {return gg_i(channel, WEED_LEAF_AUDIO_RATE);}
DEF_FN int weed_channel_get_naudchans(weed_plant_t *channel) {return gg_i(channel, WEED_LEAF_AUDIO_CHANNELS);}
DEF_FN int weed_channel_get_audio_length(weed_plant_t *channel) {return gg_i(channel, WEED_LEAF_AUDIO_DATA_LENGTH);}
DEF_FN void weed_paramtmpl_declare_volume_mastern(weed_plant_t *pt) {
  weed_leaf_set(pt, WEED_LEAF_IS_VOLUME_MASTER, WEED_SEED_BOOLEAN, 1, &wtrue);
}
static weed_plant_t *weed_audio_channel_template_init(const char *name, int flags);
#ifdef __WEED_UTILS_H__
DEF_FN  float **weed_channel_get_audio_data(weed_plant_t *channel, int *naudchans) {
  if (naudchans) *naudchans = 0;
  return (float **)weed_get_voidptr_array_counted(channel, WEED_LEAF_AUDIO_DATA, naudchans);
}
#endif
static weed_plant_t *weed_audio_channel_template_init(const char *name, int flags) {
  weed_plant_t *chantmpl = weed_channel_template_init(name, flags);
  if (chantmpl) weed_leaf_set(chantmpl, WEED_LEAF_IS_AUDIO, WEED_SEED_BOOLEAN, 1, &wtrue);
  return chantmpl;}

#endif // NEED_AUDIO

DEF_FN int weed_is_threading(weed_plant_t *inst) {
  if (inst) {
    weed_plant_t *ochan = weed_get_out_channel(inst, 0);
    return (ochan && _leaf_has_value(ochan, WEED_LEAF_OFFSET)) ? WEED_TRUE : WEED_FALSE;
  } return WEED_FALSE;}

#ifdef __WEED_UTILS_H__
DEF_FN int *weed_param_get_array_int(weed_plant_t *param, int *nvalues)
{return (int *)(weed_get_int_array_counted(param, WEED_LEAF_VALUE, nvalues));}
DEF_FN int *weed_param_get_array_boolean(weed_plant_t *param, int *nvalues)
{return weed_get_boolean_array_counted(param, WEED_LEAF_VALUE, nvalues);}
DEF_FN double *weed_param_get_array_double(weed_plant_t *param, int *nvalues)
{return weed_get_double_array_counted(param, WEED_LEAF_VALUE, nvalues);}
DEF_FN int64_t *weed_param_get_array_int64(weed_plant_t *param, int *nvalues)
{return weed_get_int64_array_counted(param, WEED_LEAF_VALUE, nvalues);}
DEF_FN char **weed_param_get_array_string(weed_plant_t *param, int *nvalues)
{return weed_get_string_array_counted(param, WEED_LEAF_VALUE, nvalues);}
DEF_FN weed_plant_t **weed_get_in_channels(weed_plant_t *instance, int *nchans)
{return weed_get_plantptr_array_counted(instance, WEED_LEAF_IN_CHANNELS, nchans);}
DEF_FN weed_plant_t **weed_get_out_channels(weed_plant_t *instance, int *nchans)
{return weed_get_plantptr_array_counted(instance, WEED_LEAF_OUT_CHANNELS, nchans);}
DEF_FN weed_plant_t **weed_get_in_params(weed_plant_t *instance, int *nparams)
{return weed_get_plantptr_array_counted(instance, WEED_LEAF_IN_PARAMETERS, nparams);}
DEF_FN weed_plant_t **weed_get_out_params(weed_plant_t *instance, int *nparams)
{return weed_get_plantptr_array_counted(instance, WEED_LEAF_OUT_PARAMETERS, nparams);}
DEF_FN int *weed_channel_get_rowstrides(weed_plant_t *channel, int *nplanes)
{return weed_get_int_array_counted(channel, WEED_LEAF_ROWSTRIDES, nplanes);}
DEF_FN void **weed_channel_get_pixel_data_planar(weed_plant_t *channel, int *nplanes)
{return weed_get_voidptr_array_counted(channel, WEED_LEAF_PIXEL_DATA, nplanes);}
#endif // __WEED_UTILS_H__

DEF_FN int weed_param_has_value(weed_plant_t *param)
{return _leaf_has_value(param, WEED_LEAF_VALUE) ? WEED_TRUE : WEED_FALSE;}
DEF_FN int weed_param_get_value_int(weed_plant_t *param) {return gg_i(param, WEED_LEAF_VALUE);}
DEF_FN int weed_param_get_value_boolean(weed_plant_t *param) {return gg_i(param, WEED_LEAF_VALUE);}
DEF_FN double weed_param_get_value_double(weed_plant_t *param) {return gg_dbl(param, WEED_LEAF_VALUE);}
DEF_FN int64_t weed_param_get_value_int64(weed_plant_t *param) {return gg_i64(param, WEED_LEAF_VALUE);}
DEF_FN char *weed_param_get_value_string(weed_plant_t *param) {
  char *value;
  return (!weed_leaf_num_elements(param, WEED_LEAF_VALUE)
          || !(value = (char *)weed_malloc(weed_leaf_element_size(param, WEED_LEAF_VALUE, 0) + 1))) ? NULL
         : *((char **)gg(param, WEED_LEAF_VALUE, 0, (void *)&value));
}

///////////////////////////////////////////////////////////////////////////////////////////////


static weed_plant_t **weed_clone_plants(weed_plant_t **plants) {
  //plants must be a NULL terminated array
  int i, j, k, type, num_plants;
  weed_plant_t **ret, *gui, *gui2;
  char **leaves, **leaves2;
  for (i = 0; plants[i]; i++);
  num_plants = i;
  ret = (weed_plant_t **)weed_malloc((num_plants + 1) * sizeof(weed_plant_t *));
  if (!ret) return NULL;
  for (i = 0; i < num_plants; i++) ret[i] = weed_plant_copy(plants[i]);
  ret[i] = NULL;
  return ret;
}

#ifdef NEED_ALPHA_SORT // for wrappers, use this to sort filters alphabetically
typedef struct dlink_list dlink_list_t;
struct dlink_list {
  weed_plant_t *filter;
  const char *name;
  dlink_list_t  *next;
};

static dlink_list_t *add_to_list_sorted(dlink_list_t *list, weed_plant_t *filter, const char *name) {
  dlink_list_t *entry = (dlink_list_t *)weed_malloc(sizeof(dlink_list_t));
  if (!entry) return list;
  entry->filter = filter;
  entry->name = strdup(name);
  entry->next = list;
  return entry;
}

static int dlink_list_cmp(const void *p1, const void *p2) {
  dlink_list_t *l1 = *((dlink_list_t **)p1);
  dlink_list_t *l2 = *((dlink_list_t **)p2);
  return strcmp(l1->name, l2->name);
}

static int add_filters_from_list(weed_plant_t *plugin_info, dlink_list_t *list) {
  int count = 0, ocount;
  dlink_list_t **dll, *xlist;
  // map list to flat array so we can qsort it. The original list was prepended, so adding in reverse restores original order.
  for (xlist = list; xlist; xlist = xlist->next) count++;
  dll = (dlink_list_t **)weed_malloc((ocount = count) * sizeof(dlink_list_t *));
  for (xlist = list; xlist; xlist = xlist->next) {
    dll[--count] = xlist;
  }
  qsort(dll, ocount, sizeof(dlink_list_t *), dlink_list_cmp);
  for (count = 0; count < ocount; count++) {
    weed_plugin_info_add_filter_class(plugin_info, dll[count]->filter);
    free((void *)dll[count]->name); weed_free(dll[count]);
  }
  weed_free(dll);
  return count;
}
#endif // NEED_ALPHA_SORT

#ifdef NEED_RANDOM

// recommended key to use
#define WEED_LEAF_PLUGIN_RANDOM_SEED "plugin_random_seed"

#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
#ifndef _CRT_RAND_S
#define _CRT_RAND_S
#endif
#else
#include <sys/time.h>
#endif
#include <stdlib.h>

DEF_FN double drand(double max)
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
{uint32_t rval; rand_s(&rval); return ((double)rval / (double)UINT_MAX * max);}
#else
{return (double)(drand48() * max);}
#endif

DEF_FN uint64_t xorshift(uint64_t x) {x ^= x << 13; x ^= x >> 7; return x ^ (x << 17);}

static uint64_t _rndval = 0;

DEF_FN uint64_t fastrand(uint64_t oldval) {
  // pseudo-random number generators
  if (!oldval) {
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
    uint32_t rval, rval2; val++; rand_s(&rval); rand_s(&rval2); oldval = fastrand(((uint64_t)rval << 32) | (uint64_t)rval2) + 1;
#else
    struct timeval t; gettimeofday(&t, NULL); srand48(t.tv_sec & 0xFFFFFFFFFFFF);
    oldval = ((uint64_t)(lrand48() << 16) ^ (uint64_t)(lrand48())) + 1;
#endif
  }
  return (_rndval = xorshift(oldval));
}

DEF_FN uint64_t fastrnd_int64(void)
{return fastrand(_rndval);}

DEF_FN uint64_t fastrand_re(weed_plant_t *inst, const char *leaf) {
  // re-entrant version
  // leaf should be prefixed with "plugin_", e.g "plugin_random_seed"
  uint64_t val = xorshift(weed_get_int64_value(inst, leaf, NULL));
  weed_set_int64_value(inst, leaf, val);
  return val;
}

DEF_FN double fastrnd_dbl(double range) {
  static const double divd = (double)(0xFFFFFFFF);
  double val = (double)fastrnd_int64() / divd;
  return val / divd * range;
}

DEF_FN double fastrand_dbl_re(double range, weed_plant_t *inst, const char *leaf) {
  static const double divd = (double)(0xFFFFFFFF);
  double val = (double)fastrand_re(inst, leaf) / divd;
  return val / divd * range;
}

DEF_FN uint32_t fastrnd_int(uint32_t range) {return (uint32_t)fastrnd_dbl((double)(++range));}

DEF_FN uint32_t fastrand_int_re(uint32_t range, weed_plant_t *inst, const char *leaf)
{return (uint32_t)fastrand_dbl_re((double)(++range), inst, leaf);}

#endif // NEED_RANDOM

//utilities
DEF_FN int is_big_endian(void) {
  union memtest {int32_t num; char chr[4];} mm;
  mm.num = 0x12345678; return (mm.chr[0] == 0x78) ? WEED_FALSE : WEED_TRUE;
}

#ifdef NEED_PALETTE_UTILS
DEF_FN int weed_palette_is_alpha(int pal) {return (pal >= 1024 && pal < 2048) ? WEED_TRUE : WEED_FALSE;}
DEF_FN int weed_palette_is_rgb(int pal) {return (pal < 512) ? WEED_TRUE : WEED_FALSE;}
DEF_FN int weed_palette_is_yuv(int pal) {return (pal >= 512 && pal < 1024) ? WEED_TRUE : WEED_FALSE;}
DEF_FN int weed_palette_get_nplanes(int pal) {
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_RGBA32
      || pal == WEED_PALETTE_BGRA32 ||
      pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_UYVY8888 || pal == WEED_PALETTE_YUYV8888
      || pal == WEED_PALETTE_YUV411 ||
      pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_AFLOAT
      || pal == WEED_PALETTE_A8 ||
      pal == WEED_PALETTE_A1 || pal == WEED_PALETTE_RGBFLOAT || pal == WEED_PALETTE_RGBAFLOAT) return 1;
  if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P ||
      pal == WEED_PALETTE_YUV444P) return 3;
  if (pal == WEED_PALETTE_YUVA4444P) return 4;
  return 0;
}

DEF_FN int weed_palette_is_valid(int pal) {return weed_palette_get_nplanes(pal) ? WEED_TRUE : WEED_FALSE;}

DEF_FN int weed_palette_is_float_palette(int pal) {
  return (pal == WEED_PALETTE_RGBAFLOAT || pal == WEED_PALETTE_AFLOAT
          || pal == WEED_PALETTE_RGBFLOAT) ? WEED_TRUE : WEED_FALSE;
}

DEF_FN int weed_palette_has_alpha_channel(int pal) {
  return (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 ||
          pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUVA8888 || pal == WEED_PALETTE_RGBAFLOAT ||
          weed_palette_is_alpha(pal)) ? WEED_TRUE : WEED_FALSE;
}

DEF_FN double weed_palette_get_plane_ratio_horizontal(int pal, int plane) {
  // return ratio of plane[n] width/plane[0] width;
  if (plane == 0) return 1.0;
  if (plane == 1 || plane == 2) {
    if (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) return 1.0;
    if (pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane == 3) if (pal == WEED_PALETTE_YUVA4444P) return 1.0;
  return 0.0;
}

DEF_FN double weed_palette_get_plane_ratio_vertical(int pal, int plane) {
  // return ratio of plane[n] height/plane[n] height
  if (plane == 0) return 1.0;
  if (plane == 1 || plane == 2) {
    if (pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_YUV422P) return 1.0;
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane == 3) if (pal == WEED_PALETTE_YUVA4444P) return 1.0;
  return 0.0;
}

static size_t blank_pixel(uint8_t *dst, int pal, int yuv_clamping, uint8_t *src) {
  // set src to non-null to preserve the alpha channel (if applicable)
  // yuv_clamping
  // only valid for non-planar (packed) palettes
  unsigned char *idst = dst;
  uint8_t y_black = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16;
  switch (pal) {
  case WEED_PALETTE_RGB24: case WEED_PALETTE_BGR24: dst[0] = dst[1] = dst[2] = 0; dst += 3; break;
  case WEED_PALETTE_RGBA32: case WEED_PALETTE_BGRA32:
    dst[0] = dst[1] = dst[2] = 0; dst[3] = src == NULL ? 255 : src[3]; dst += 4; break;
  case WEED_PALETTE_ARGB32: dst[1] = dst[2] = dst[3] = 0; dst[0] = src == NULL ? 255 : src[0]; dst += 4; break;
  case WEED_PALETTE_UYVY8888: dst[1] = dst[3] = y_black; dst[0] = dst[2] = 128; dst += 4; break;
  case WEED_PALETTE_YUYV8888: dst[0] = dst[2] = y_black; dst[1] = dst[3] = 128; dst += 4; break;
  case WEED_PALETTE_YUV888: dst[0] = y_black; dst[1] = dst[2] = 128; dst += 3; break;
  case WEED_PALETTE_YUVA8888: dst[0] = y_black; dst[1] = dst[2] = 128; dst[3] = src == NULL ? 255 : src[3]; dst += 4; break;
  case WEED_PALETTE_YUV411: dst[0] = dst[3] = 128; dst[1] = dst[2] = dst[4] = dst[5] = y_black; dst += 6; break;
  default: break;
  } return dst - idst;
}

static void blank_row(uint8_t **pdst, int width, int pal, int yuv_clamping, int uvcopy, uint8_t **psrc) {
  // for YUV420 and YVU420, only set uvcopy for even rows, and increment pdst[1], pdst[2] on the odd rows
  int nplanes, p, mpsize;
  uint8_t *dst = *pdst, *src = NULL, black[3];
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {weed_memset(dst, 0, width * 3); return;}
  if (!uvcopy && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P)) nplanes = 1;
  else nplanes = weed_palette_get_nplanes(pal);
  black[0] = yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED ? 0 : 16; black[1] = black[2] = 128;
  for (p = 0; p < nplanes; p++) {
    dst = pdst[p];
    if (psrc != NULL) src = psrc[p];
    if (p == 3) {if (!src) weed_memset(dst, 255, width); else weed_memcpy(dst, src, width); break;}
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P
        || pal == WEED_PALETTE_YUV444P || pal == WEED_PALETTE_YUVA4444P) weed_memset(dst, black[p], width);
    else {
      // RGBA, BGRA, ARGB, YUV888, YUVA8888, UYVY, YUYV, YUV411
      for (int i = 0; i < width; i++) {mpsize = blank_pixel(dst, pal, yuv_clamping, src); dst += mpsize; if (src != NULL) src += mpsize;}
      break;
    } if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P))
      width >>= 1;
  }
}

static void blank_frame(void **pdata, int width, int height, int *rs, int pal, int yuv_clamping) {
  uint8_t *pd2[4];
  int nplanes = weed_palette_get_nplanes(pal), odd, is_420 = (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P);
  int i, j;
  for (j = 0; j < nplanes; j++) pd2[j] = (uint8_t *)pdata[j];
  for (i = 0; i < height; i++) {
    blank_row(pd2, width, pal, yuv_clamping, !(odd = i ^ 1), NULL);
    for (j = 0; j < nplanes; j++) {pd2[j] += rs[j]; if (is_420 && !odd) break;}
  }
}
#endif // NEED_PALETTE_UTILS

#ifdef NEED_PALETTE_CONVERSIONS

#ifndef myround
#define myround(n) ((n >= 0.) ? (int)(n + 0.5) : (int)(n - 0.5))
#endif

static int alpha_inited = 0;
static int unal[256][256], al[256][256];

DEF_FN void init_unal(void) {
  // premult to postmult and vice-versa
  for (int i = 0; i < 256; i++)
    for (int j = 0; j < 256; j++) {
      unal[i][j] = (float)j * 255. / (float)i; al[i][j] = (float)j * (float)i / 255.;
    }
}

static void alpha_premult(unsigned char *ptr, int width, int height, int rowstride, int pal, int un) {
  int aoffs = 3, coffs = 0, psizel = 3, alpha, psize = 4;
  int i, j, p;
  switch (pal) {
  case WEED_PALETTE_BGRA32: break;
  case WEED_PALETTE_ARGB32: psizel = 4; coffs = 1; aoffs = 0; break;
  default: return;
  }
  if (!alpha_inited) init_unal();
  if (un == WEED_TRUE) {
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += psize)
      {alpha = ptr[j + aoffs]; for (p = coffs; p < psizel; p++) ptr[j + p] = unal[alpha][ptr[j + p]];}
      ptr += rowstride;
    }
  } else {
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += psize) {
        alpha = ptr[j + aoffs];
        for (p = coffs; p < psizel; p++) ptr[j + p] = al[alpha][ptr[j + p]];
      }
      ptr += rowstride;
    }}}

/* precomputed tables */
#define FP_BITS 16

static int Y_Ru[256], Y_Gu[256], Y_Bu[256], Cb_Ru[256], Cb_Gu[256], Cb_Bu[256];
static int Cr_Ru[256], Cr_Gu[256], Cr_Bu[256], YCL_YUCL[256], UVCL_UVUCL[256], YUCL_YCL[256];
static int yuv_rgb_inited = 0, y_y_inited = 0;

#define KR_YCBCR 0.299
#define KB_YCBCR 0.114
#define UV_BIAS 128.

#define SCALE_FACTOR 65536.

static void init_RGB_to_YCbCr_tables(void) {
  double fac;
  for (int i = 0; i < 256; i++) {
    Y_Ru[i] = myround(KR_YCBCR * (double)i * SCALE_FACTOR);   // Kr
    Y_Gu[i] = myround((1. - KR_YCBCR - KB_YCBCR) * (double)i * SCALE_FACTOR);   // Kb
    Y_Bu[i] = myround(KB_YCBCR * (double)i * SCALE_FACTOR);
    fac = .5 / (1. - KB_YCBCR); // .564
    Cb_Ru[i] = myround(-fac * KR_YCBCR * (double)i * SCALE_FACTOR); // -.16736
    Cb_Gu[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR)  * (double)i * SCALE_FACTOR); // -.331264
    Cb_Bu[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);
    fac = .5 / (1. - KR_YCBCR); // .713
    Cr_Ru[i] = myround((0.5 * (double)i + UV_BIAS) * SCALE_FACTOR);
    Cr_Gu[i] = myround(-fac * (1. - KB_YCBCR - KR_YCBCR) * (double)i * SCALE_FACTOR);
    Cr_Bu[i] = myround(-fac * KB_YCBCR * (double)i * SCALE_FACTOR);
  } yuv_rgb_inited = 1;}

// unclamped values
#define RGB_2_YIQ(a, b, c) do {short x; for (int i = 0; i < NUM_PIXELS_SQUARED; i++) {	\
      Unit Y, I, Q;							\
      if ((x = ((Y_Ru[(int)a[i]] + Y_Gu[(int)b[i]] + Y_Bu[(int)(int)c[i]]) >> FP_BITS)) > 255) x = 255; \
      Y = x < 0 ? 0 : x;							\
      if ((x = ((Cr_Ru[(int)a[i]] + Cr_Gu[(int)b[i]] + Cr_Bu[(int)c[i]]) >> FP_BITS)) > 255) x = 255; \
      I = x < 0 ? 0 : x;							\
      if ((x = ((Cb_Ru[(int)a[i]] + Cb_Gu[(int)b[i]] + Cb_Bu[(int)c[i]] ) >> FP_BITS)) > 255) x = 255; \
      Q = x < 0 ? 0 : x; a[i] = Y; b[i] = I; c[i] = Q;					\
    }} while(0)

static void init_Y_to_Y_tables(void) {
  int i;
  for (i = 0; i < 17; i++) {UVCL_UVUCL[i] = YCL_YUCL[i]
      = 0; YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;}
  for (; i < 235; i++) {
    YCL_YUCL[i] = (int)((float)(i - 16.) / 219. * 255. + .5);
    UVCL_UVUCL[i] = (int)((float)(i - 16.) / 224. * 255. + .5);
    YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;}
  for (; i < 256; i++) {UVCL_UVUCL[i] = YCL_YUCL[i] = 255;
    YUCL_YCL[i] = (uint8_t)((float)i / 255. * 219. + .5)  + 16;}
  y_y_inited = 1;}

DEF_FN uint8_t y_unclamped_to_clamped(uint8_t y) {if (!y_y_inited) init_Y_to_Y_tables(); return YUCL_YCL[y];}
DEF_FN uint8_t y_clamped_to_unclamped(uint8_t y) {if (!y_y_inited) init_Y_to_Y_tables(); return YCL_YUCL[y];}
DEF_FN uint8_t uv_clamped_to_unclamped(uint8_t u) {if (!y_y_inited) init_Y_to_Y_tables(); return UVCL_UVUCL[u];}

static uint8_t calc_luma(uint8_t *pixel, int palette, int yuv_clamping) {
  if (!yuv_rgb_inited) init_RGB_to_YCbCr_tables();
  switch (palette) {
  case WEED_PALETTE_RGB24: case WEED_PALETTE_RGBA32: return (Y_Ru[pixel[0]] + Y_Gu[pixel[1]] + Y_Bu[pixel[2]]) >> FP_BITS;
  case WEED_PALETTE_BGR24: case WEED_PALETTE_BGRA32: return (Y_Ru[pixel[2]] + Y_Gu[pixel[1]] + Y_Bu[pixel[0]]) >> FP_BITS;
  case WEED_PALETTE_ARGB32: return (Y_Ru[pixel[1]] + Y_Gu[pixel[2]] + Y_Bu[pixel[3]]) >> FP_BITS;
  default:
    if (yuv_clamping == WEED_YUV_CLAMPING_UNCLAMPED) return pixel[0];
    else {if (!y_y_inited) init_Y_to_Y_tables(); return YCL_YUCL[pixel[0]];}
  } return 0;
}

#define CLAMP0_255(N) (N&0x80000000?0x00:N&0x7FFFFF00?0xFF:N)
#define CLAMP0_255i(f) (uint8_t)CLAMP0_255((int)(f))

DEF_FN int get_gamma_idx(int gamma_type) {
  for (int i = 0; i < N_GAMMA_TYPES; i++) if (gamma_idx[i] == gamma_type) return i;
  return 0;}

FN_DECL void init_gamma_tx(void) {INIT_GAMMA(WEED_GAMMA_SRGB);
  INIT_GAMMA(WEED_GAMMA_BT709); gamma_inited = 1;}

static void convert_gamma(uint8_t *dst_rgb, uint8_t *src_rgb, int gamma_from, int gamma_to) {
  if (!gamma_inited) init_gamma_tx();
  for (int i = 0; i < 3; i++) {
    float x = (float)src_rgb[i] / 255.;
    if (gamma_from != WEED_GAMMA_LINEAR) {
      int idx = get_gamma_idx(gamma_from);
      x = (x < gamma_tx[idx].thresh) ? x / gamma_tx[idx].lin
	: powf((x + gamma_tx[idx].offs) / (1. + gamma_tx[idx].offs),
	       gamma_tx[idx].pf);}
    if (gamma_to != WEED_GAMMA_LINEAR) {
      int idx = get_gamma_idx(gamma_to);
      x = (x < (gamma_tx[idx].thresh) / gamma_tx[idx].lin)
	? x * gamma_tx[idx].lin
	: powf((1. + gamma_tx[idx].offs) * x,
	       1. / gamma_tx[idx].pf) - gamma_tx[idx].offs;}
    dst_rgb[i] = CLAMP0_255i(x * 255.);}}

#endif // NEED_PALETTE_CONVERSIONS

#ifdef NEED_FONT_UTILS
#include <wchar.h>
static const wchar_t *fstretches[] = {L"UltraCondensed", L"ExtraCondensed", L"Condensed", L"SemiCondensed",
                                      L"SemiExpanded", L"Expanded", L"ExtraExpanded", L"UltraExpanded", NULL};
static const wchar_t *fweights[] = {L"Thin", L"UltraLight", L"ExtraLight", L"Light", L"SemiLight", L"DemiLight",
                                    L"Book", L"Regular", L"Medium",
                                    L"DemiBold", L"SemiBold", L"Bold", L"ExtraBold", L"UltraBold", L"Black", L"Heavy",
                                    L"UltraHeavy", L"UltraBlack", L"ExtraBlack", NULL};
static const wchar_t *fstyles[] = {L"Roman", L"Italic", L"Oblique", NULL};

void weed_parse_font_string(const char *fontstr, char **family, char **fstretch, char **fweight,
                            char **fstyle, int *size) {
  wchar_t *token, *state, *next_token, *xfamily = L"\0";
  wchar_t *xfstretch = L"\0", *xfweight = L"\0";
  wchar_t *xfstyle = L"\0", *xfontstr = NULL;;
  size_t wcs_len;

  if (size) *size = 0;
  if (!fontstr) goto fontdone;

  wcs_len = mbstowcs(NULL, fontstr, 0) + 1;
  xfontstr = weed_calloc(wcs_len, sizeof(wchar_t));
  mbstowcs(xfontstr, fontstr, wcs_len);

  for (token = wcstok(xfontstr, L" ", &state); token; token = next_token) {
    if (*token == L'\0') {
      next_token = wcstok(NULL, L" ", &state);
      continue;
    }
    do {
      next_token = wcstok(NULL, L" ", &state);
    } while (next_token && *next_token == L'\0');
    if (!next_token) {
      wchar_t *endptr;
      uintmax_t umax = wcstoumax(token, &endptr, 10);
      if (umax > 0 && *endptr == L'\0') {
        if (size) *size = (int)(umax & 0x80FFFFFF);
        goto fontdone;
      }
    }
    if (*xfstretch == L'\0') {
      for (int i = 0; fstretches[i]; i++) {
        if (!wcscasecmp(token, fstretches[i])) {
          xfstretch = (wchar_t *)fstretches[i];
          break;
        }
      }
      if (*xfstyle != L'\0') continue;
    }
    if (*xfweight == L'\0') {
      for (int i = 0; fweights[i]; i++) {
        if (!wcscasecmp(token, fweights[i])) {
          xfweight = (wchar_t *)fweights[i];
          break;
        }
      }
      if (*xfweight != L'\0') continue;
    }
    if (*xfstyle == L'\0') {
      for (int i = 0; fstyles[i]; i++) {
        if (!wcscasecmp(token, fstyles[i])) {
          xfstyle = (wchar_t *)fstyles[i];
          break;
        }
      }
      if (*xfstyle != L'\0') continue;
    }
    if (*xfstretch == L'\0' && *xfweight == L'\0' && *xfstyle == L'\0') {
      if (*xfamily != L'\0') {
        wchar_t tmp[256];
        swprintf(tmp, 256, L"%ls %ls", xfamily, token);
        free(xfamily);
        xfamily = wcsdup(tmp);
      } else xfamily = wcsdup(token);
    }
  }
fontdone:
  if (family) {
    wcs_len = wcstombs(NULL, xfamily, 0) + 1;
    *family = weed_calloc(wcs_len, 1);
    wcstombs(*family, xfamily, wcs_len);
  }
  if (fstretch) {
    wcs_len = wcstombs(NULL, xfstretch, 0) + 1;
    *fstretch = weed_calloc(wcs_len, 1);
    wcstombs(*fstretch, xfstretch, wcs_len);
  }
  if (fweight) {
    wcs_len = wcstombs(NULL, xfweight, 0) + 1;
    *fweight = weed_calloc(wcs_len, 1);
    wcstombs(*fweight, xfweight, wcs_len);
  }
  if (fstyle) {
    wcs_len = wcstombs(NULL, xfstyle, 0) + 1;
    *fstyle = weed_calloc(wcs_len, 1);
    wcstombs(*fstyle, xfstyle, wcs_len);
  }
  if (*xfamily != L'\0') weed_free(xfamily);
  if (xfontstr) weed_free(xfontstr);
}
#endif

#ifndef WEED_TICKS_PER_SECOND
#define WEED_TICKS_PER_SECOND 100000000
#endif

#endif // __WEED_PLUGIN__
#endif // __HAVE_WEED_PLUGIN_UTILS__
