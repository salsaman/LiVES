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

int weed_plant_has_leaf(weed_plant_t *plant, const char *key);
int weed_set_int_value(weed_plant_t *plant, const char *key, int value);
int weed_set_double_value(weed_plant_t *plant, const char *key, double value);
int weed_set_boolean_value(weed_plant_t *plant, const char *key, int value);
int weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value);
int weed_set_string_value(weed_plant_t *plant, const char *key, const char *value);
int weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value);
int weed_set_voidptr_value(weed_plant_t *plant, const char *key, void *value);

int weed_get_int_value(weed_plant_t *plant, const char *key, int *error);
double weed_get_double_value(weed_plant_t *plant, const char *key, int *error);
int weed_get_boolean_value(weed_plant_t *plant, const char *key, int *error);
int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, int *error);
char *weed_get_string_value(weed_plant_t *plant, const char *key, int *error);
void *weed_get_voidptr_value(weed_plant_t *plant, const char *key, int *error);
weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, int *error);

int *weed_get_int_array(weed_plant_t *plant, const char *key, int *error);
double *weed_get_double_array(weed_plant_t *plant, const char *key, int *error);
int *weed_get_boolean_array(weed_plant_t *plant, const char *key, int *error);
int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, int *error);
char **weed_get_string_array(weed_plant_t *plant, const char *key, int *error);
void **weed_get_voidptr_array(weed_plant_t *plant, const char *key, int *error);
weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, int *error);

int weed_set_int_array(weed_plant_t *plant, const char *key, int num_elems, int *values);
int weed_set_double_array(weed_plant_t *plant, const char *key, int num_elems, double *values);
int weed_set_boolean_array(weed_plant_t *plant, const char *key, int num_elems, int *values);
int weed_set_int64_array(weed_plant_t *plant, const char *key, int num_elems, int64_t *values);
int weed_set_string_array(weed_plant_t *plant, const char *key, int num_elems, char **values);
int weed_set_voidptr_array(weed_plant_t *plant, const char *key, int num_elems, void **values);
int weed_set_plantptr_array(weed_plant_t *plant, const char *key, int num_elems, weed_plant_t **values);

int weed_leaf_copy(weed_plant_t *dest, const char *keyt, weed_plant_t *src, const char *keyf);
weed_plant_t *weed_plant_copy(weed_plant_t *src);
int weed_get_plant_type(weed_plant_t *plant);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_UTILS_H__
