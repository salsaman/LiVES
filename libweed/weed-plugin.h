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
