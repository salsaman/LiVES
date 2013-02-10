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

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   mainly based on LiViDO, which is developed by:


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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2013 */

#ifndef __WEED_PLUGIN_UTILS_H__
#define __WEED_PLUGIN_UTILS_H__

#ifndef __WEED_H__
#include <weed/weed.h>
#endif

#ifndef __WEED_PALETTES_H__
#include <weed/weed-palettes.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef __WEED_EFFECTS_H__
  weed_plant_t *weed_plugin_info_init (weed_bootstrap_f weed_boot, int num_versions, int *api_versions);
  weed_plant_t *weed_filter_class_init (const char *name, const char *author, int version, int flags, weed_init_f init_func, weed_process_f process_func, weed_deinit_f deinit_func, weed_plant_t **in_chantmpls, weed_plant_t **out_chantmpls, weed_plant_t **in_paramtmpls, weed_plant_t **out_paramtmpls);
#endif
  int weed_get_api_version(weed_plant_t *plugin_info);
  weed_plant_t *weed_channel_template_init (const char *name, int flags, int *palettes);
  weed_plant_t *weed_audio_channel_template_init (const char *name, int flags);
  void weed_plugin_info_add_filter_class (weed_plant_t *plugin_info, weed_plant_t *filter_class);
  weed_plant_t *weed_parameter_template_get_gui (weed_plant_t *paramt);
  weed_plant_t *weed_parameter_get_gui(weed_plant_t *param);
  weed_plant_t *weed_filter_class_get_gui (weed_plant_t *filter);
  
  weed_plant_t *weed_integer_init (const char *name, const char *label, int def, int min, int max);
  weed_plant_t *weed_string_list_init (const char *name, const char *label, int def, const char ** const list);
  weed_plant_t *weed_switch_init (const char *name, const char *label, int def);
  weed_plant_t *weed_radio_init (const char *name, const char *label, int def, int group);
  weed_plant_t *weed_float_init (const char *name, const char *label, double def, double min, double max);
  weed_plant_t *weed_text_init (const char *name, const char *label, const char *def);
  weed_plant_t *weed_colRGBi_init (const char *name, const char *label, int red, int green, int blue);
  weed_plant_t *weed_colRGBd_init (const char *name, const char *label, double red, double green, double blue);

  weed_plant_t *weed_out_param_integer_init (const char *name, int def, int min, int max);
  weed_plant_t *weed_out_param_switch_init (const char *name, int def);
  weed_plant_t *weed_out_param_float_init (const char *name, double def, double min, double max);
  weed_plant_t *weed_out_param_text_init (const char *name, const char *def);
  weed_plant_t *weed_out_param_colRGBi_init (const char *name, int red, int green, int blue);
  weed_plant_t *weed_out_param_colRGBd_init (const char *name, double red, double green, double blue);

  weed_plant_t **weed_clone_plants (weed_plant_t **plants);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_PLUGIN_UTILS_H__


