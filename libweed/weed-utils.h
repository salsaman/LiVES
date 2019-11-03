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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2010 */

#ifndef __WEED_UTILS_H__
#define __WEED_UTILS_H__

#ifndef __WEED_H__
#include <weed/weed.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* functions return WEED_TRUE or WEED_FALSE */

/* check if leaf exists and has a value */
int weed_plant_has_leaf(weed_plant_t *, const char *key);

/* check if leaf exists; may have a seed_type but no value set */
int weed_leaf_exists(weed_plant_t *, const char *key);

weed_error_t weed_set_int_value(weed_plant_t *, const char *key, int32_t value);
weed_error_t weed_set_double_value(weed_plant_t *, const char *key, double value);
weed_error_t weed_set_boolean_value(weed_plant_t *, const char *key, int32_t value);
weed_error_t weed_set_int64_value(weed_plant_t *, const char *key, int64_t value);
weed_error_t weed_set_string_value(weed_plant_t *, const char *key, const char *value);
weed_error_t weed_set_plantptr_value(weed_plant_t *, const char *key, weed_plant_t *value);
weed_error_t weed_set_voidptr_value(weed_plant_t *, const char *key, void *value);

int32_t weed_get_int_value(weed_plant_t *, const char *key, weed_error_t *error);
double weed_get_double_value(weed_plant_t *, const char *key, weed_error_t *error);
int32_t weed_get_boolean_value(weed_plant_t *, const char *key, weed_error_t *error);
int64_t weed_get_int64_value(weed_plant_t *, const char *key, weed_error_t *error);
char *weed_get_string_value(weed_plant_t *, const char *key, weed_error_t *error);
void *weed_get_voidptr_value(weed_plant_t *, const char *key, weed_error_t *error);
weed_plant_t *weed_get_plantptr_value(weed_plant_t *, const char *key, weed_error_t *error);

weed_error_t weed_set_int_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *values);
weed_error_t weed_set_double_array(weed_plant_t *, const char *key, weed_size_t num_elems, double *values);
weed_error_t weed_set_boolean_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *values);
weed_error_t weed_set_int64_array(weed_plant_t *, const char *key, weed_size_t num_elems, int64_t *values);
weed_error_t weed_set_string_array(weed_plant_t *, const char *key, weed_size_t num_elems, char **values);
weed_error_t weed_set_voidptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, void **values);
weed_error_t weed_set_plantptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_plant_t **values);

int32_t *weed_get_int_array(weed_plant_t *, const char *key, weed_error_t *error);
double *weed_get_double_array(weed_plant_t *, const char *key, weed_error_t *error);
int32_t *weed_get_boolean_array(weed_plant_t *, const char *key, weed_error_t *error);
int64_t *weed_get_int64_array(weed_plant_t *, const char *key, weed_error_t *error);
char **weed_get_string_array(weed_plant_t *, const char *key, weed_error_t *error);
void **weed_get_voidptr_array(weed_plant_t *, const char *key, weed_error_t *error);
weed_plant_t **weed_get_plantptr_array(weed_plant_t *, const char *key, weed_error_t *error);

weed_error_t weed_leaf_copy(weed_plant_t *dest, const char *keyt, weed_plant_t *src, const char *keyf);
weed_plant_t *weed_plant_copy(weed_plant_t *src);
int32_t weed_get_plant_type(weed_plant_t *); // returns WEED_PLANT_UNKNOWN if plant is NULL

int check_weed_abi_compat(int32_t higher, int32_t lower); // returns WEED_TRUE or WEED_FALSE
int check_filter_api_compat(int32_t higher, int32_t lower); // returns WEED_TRUE or WEED_FALSE

/* plugin only function, host should pass a pointer to this to the plugin when calling its weed_setup function */
weed_plant_t *weed_bootstrap(weed_default_getter_f *, int32_t plugin_weed_min_api_version, int32_t plugin_weed_max_api_version,
                             int32_t plugin_filter_min_api_version, int32_t plugin_filter_max_api_version);

#if defined(__WEED_HOST__) || defined(__LIBWEED__)
/* host only functions */
void weed_add_plant_flags(weed_plant_t *plant, int32_t flags);
void weed_clear_plant_flags(weed_plant_t *plant, int32_t flags);

typedef void (*weed_host_info_callback_f)(weed_plant_t *host_info, void *user_data);

void weed_set_host_info_callback(weed_host_info_callback_f, void *user_data);
#endif

#define WEED_LEAF_MIN_WEED_API_VERSION   "min_weed_version"
#define WEED_LEAF_MAX_WEED_API_VERSION   "max_weed_version"
#define WEED_LEAF_MIN_FILTER_API_VERSION "min_filter_version"
#define WEED_LEAF_MAX_FILTER_API_VERSION "max_filter_version"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_UTILS_H__
