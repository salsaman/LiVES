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

#ifndef __WEED_PLUGIN_H__
#define __WEED_PLUGIN_H__

#ifndef __WEED_H__
#include <weed/weed.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef __WEED_INTERNAL__
 /// functions defined as static so as not to overwrite the host versions

static weed_leaf_set_f weed_leaf_set;
static weed_leaf_get_f weed_leaf_get;
static weed_plant_new_f weed_plant_new;
static weed_plant_list_leaves_f weed_plant_list_leaves;
static weed_leaf_num_elements_f weed_leaf_num_elements;
static weed_leaf_element_size_f weed_leaf_element_size;
static weed_leaf_seed_type_f weed_leaf_seed_type;
static weed_leaf_get_flags_f weed_leaf_get_flags;

static weed_malloc_f weed_malloc;
static weed_free_f weed_free;
static weed_memcpy_f weed_memcpy;
static weed_memset_f weed_memset;


weed_plant_t *weed_plugin_info_init (weed_bootstrap_f weed_boot, int num_versions, int *api_versions) {
  /////////////////////////////////////////////////////////
  // get our bootstrap values
  // every plugin should call this in its weed_setup()
  int api_version;

  weed_default_getter_f weed_default_get;
  weed_leaf_get_f *wlg;
  weed_leaf_set_f *wls;
  weed_plant_new_f *wpn;
  weed_plant_list_leaves_f *wpll;
  weed_leaf_num_elements_f *wlne;
  weed_leaf_element_size_f *wles;
  weed_leaf_seed_type_f *wlst;
  weed_leaf_get_flags_f *wlgf;
  weed_malloc_f *weedmalloc;
  weed_free_f *weedfree;
  weed_memset_f *weedmemset;
  weed_memcpy_f *weedmemcpy;

  weed_plant_t *host_info=weed_boot((weed_default_getter_f *)&weed_default_get,num_versions,api_versions),*plugin_info;
  if (host_info==NULL) return NULL; // matching version was not found


  //////////// get api version /////////
  weed_default_get(host_info,"api_version",0,&api_version);


  // depending on the api version we could have different functions

  // we must use the default getter to get our API functions

  weed_default_get(host_info,"weed_malloc_func",0,(void *)&weedmalloc);
  weed_malloc=weedmalloc[0];

  weed_default_get(host_info,"weed_free_func",0,(void *)&weedfree);
  weed_free=weedfree[0];

  weed_default_get(host_info,"weed_memset_func",0,(void *)&weedmemset);
  weed_memset=weedmemset[0];

  weed_default_get(host_info,"weed_memcpy_func",0,(void *)&weedmemcpy);
  weed_memcpy=weedmemcpy[0];

  weed_default_get(host_info,"weed_leaf_get_func",0,(void *)&wlg);
  weed_leaf_get=wlg[0];

  weed_default_get(host_info,"weed_leaf_set_func",0,(void *)&wls);
  weed_leaf_set=wls[0];

  weed_default_get(host_info,"weed_plant_new_func",0,(void *)&wpn);
  weed_plant_new=wpn[0];

  weed_default_get(host_info,"weed_plant_list_leaves_func",0,(void *)&wpll);
  weed_plant_list_leaves=wpll[0];

  weed_default_get(host_info,"weed_leaf_num_elements_func",0,(void *)&wlne);
  weed_leaf_num_elements=wlne[0];

  weed_default_get(host_info,"weed_leaf_element_size_func",0,(void *)&wles);
  weed_leaf_element_size=wles[0];

  weed_default_get(host_info,"weed_leaf_seed_type_func",0,(void *)&wlst);
  weed_leaf_seed_type=wlst[0];

  weed_default_get(host_info,"weed_leaf_get_flags_func",0,(void *)&wlgf);
  weed_leaf_get_flags=wlgf[0];


  // get any additional functions for higher API versions ////////////



  //////////////////////////////////////////////////////////////////////

  // we can now use the normal API functions


  plugin_info=weed_plant_new(WEED_PLANT_PLUGIN_INFO);

  weed_leaf_set(plugin_info,"host_info",WEED_SEED_PLANTPTR,1,&host_info);

  return plugin_info;
}

#else
/// weed libs which #include this should use the plugin's version

extern weed_leaf_set_f weed_leaf_set;
extern weed_leaf_get_f weed_leaf_get;
extern weed_plant_new_f weed_plant_new;
extern weed_plant_list_leaves_f weed_plant_list_leaves;
extern weed_leaf_num_elements_f weed_leaf_num_elements;
extern weed_leaf_element_size_f weed_leaf_element_size;
extern weed_leaf_seed_type_f weed_leaf_seed_type;
extern weed_leaf_get_flags_f weed_leaf_get_flags;

extern weed_malloc_f weed_malloc;
extern weed_free_f weed_free;
extern weed_memcpy_f weed_memcpy;
extern weed_memset_f weed_memset;
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_PLUGIN_H__
