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

/* (C) G. Finch, 2005 - 2020 */

#ifndef __WEED_UTILS_H__
#define __WEED_UTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef _BUILD_LOCAL_
#include "weed.h"
#else
#include <weed/weed.h>
#endif

#ifdef __LIBWEED__
extern weed_error_t __wbg__(size_t, weed_hash_t, int, weed_plant_t *,
			    const char *,  weed_voidptr_t);
  
#if WEED_ABI_CHECK_VERSION(203)
// library only functions to lock / unlock leaves for atomic operations
extern weed_leaf_t *_weed_intern_freeze(weed_plant_t *, const char *);
extern weed_error_t _weed_intern_unfreeze(weed_leaf_t *);
// library only functions to avoid finding a leaf multiple times
extern weed_seed_t _weed_intern_seed_type(weed_leaf_t *);
extern weed_size_t _weed_intern_num_elems(weed_leaf_t *);
extern weed_size_t _weed_intern_elem_sizes(weed_leaf_t *, weed_size_t *);
extern weed_size_t _weed_intern_elem_size(weed_leaf_t *, weed_size_t idx, weed_error_t *);
extern weed_error_t _weed_intern_get_all(weed_leaf_t *, weed_voidptr_t retvals);
extern weed_error_t _weed_intern_get(weed_leaf_t *, weed_size_t idx, weed_voidptr_t retval);
// free using the designated weed_free function
extern void _weed_intern_leaves_list_free(char **leaveslist);
#endif

#endif

/* some nice macros, e.g
  double x = WEED_LEAF_GET(myplant, "mykey", double);
*/
#define WEED_LEAF_GET(plant, key, type) weed_get_##type##_value(plant, key, NULL)
#define WEED_LEAF_GET_ARRAY_COUNTED(plant, key, type, counter) weed_get_##type##_array_counted(plant, key, &count)
#define WEED_LEAF_SET(plant, key, type, value) weed_set_##type##_value(plant, key, value)
#define WEED_LEAF_SET_ARRAY(plant, key, type, nvals, array) weed_set_##type##_array(plant, key, nvals, array)

typedef int (*weed_memcmp_f)(const void *, const void *, size_t);

void weed_utils_set_custom_memfuncs(weed_malloc_f malloc_func, weed_calloc_f calloc_func, weed_memcpy_f memcpy_func,
                                    weed_memcmp_f memcmp_func, weed_free_f free_func);

/* check if leaf exists and has a value, returns WEED_TRUE or WEED_FALSE */
int weed_plant_has_leaf(weed_plant_t *, const char *key);

/* check if leaf exists; may have a seed_type but no value set */
int weed_leaf_exists(weed_plant_t *, const char *key);

weed_error_t weed_set_int_value(weed_plant_t *, const char *key, int32_t);
weed_error_t weed_set_uint_value(weed_plant_t *, const char *key, uint32_t);
weed_error_t weed_set_double_value(weed_plant_t *, const char *key, double);
weed_error_t weed_set_boolean_value(weed_plant_t *, const char *key, weed_boolean_t);
weed_error_t weed_set_int64_value(weed_plant_t *, const char *key, int64_t);
weed_error_t weed_set_uint64_value(weed_plant_t *, const char *key, uint64_t);
weed_error_t weed_set_string_value(weed_plant_t *, const char *key, const char *);
weed_error_t weed_set_funcptr_value(weed_plant_t *, const char *key, weed_funcptr_t);
weed_error_t weed_set_voidptr_value(weed_plant_t *, const char *key, void *);
weed_error_t weed_set_plantptr_value(weed_plant_t *, const char *key, weed_plant_t *);
weed_error_t weed_set_custom_value(weed_plant_t *, const char *key, uint32_t seed_type, void *);

int32_t weed_get_int_value(weed_plant_t *, const char *key, weed_error_t *);
uint32_t weed_get_uint_value(weed_plant_t *, const char *key, weed_error_t *);
double weed_get_double_value(weed_plant_t *, const char *key, weed_error_t *);
weed_boolean_t weed_get_boolean_value(weed_plant_t *, const char *key, weed_error_t *);
int64_t weed_get_int64_value(weed_plant_t *, const char *key, weed_error_t *);
uint64_t weed_get_uint64_value(weed_plant_t *, const char *key, weed_error_t *);
char *weed_get_string_value(weed_plant_t *, const char *key, weed_error_t *);
weed_funcptr_t weed_get_funcptr_value(weed_plant_t *, const char *key, weed_error_t *);
void *weed_get_voidptr_value(weed_plant_t *, const char *key, weed_error_t *);
weed_plant_t *weed_get_plantptr_value(weed_plant_t *, const char *key, weed_error_t *);
void *weed_get_custom_value(weed_plant_t *, const char *key, uint32_t seed_type, weed_error_t *);

weed_error_t weed_set_int_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *);
weed_error_t weed_set_uint_array(weed_plant_t *, const char *key, weed_size_t num_elems, uint32_t *);
weed_error_t weed_set_double_array(weed_plant_t *, const char *key, weed_size_t num_elems, double *);
weed_error_t weed_set_boolean_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_boolean_t *);
weed_error_t weed_set_int64_array(weed_plant_t *, const char *key, weed_size_t num_elems, int64_t *);
weed_error_t weed_set_uint64_array(weed_plant_t *, const char *key, weed_size_t num_elems, uint64_t *);
weed_error_t weed_set_string_array(weed_plant_t *, const char *key, weed_size_t num_elems, char **);
weed_error_t weed_set_funcptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_funcptr_t *);
weed_error_t weed_set_voidptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, void **);
weed_error_t weed_set_plantptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_plant_t **);
weed_error_t weed_set_custom_array(weed_plant_t *, const char *key, uint32_t seed_type, weed_size_t num_elems, void **);

int32_t *weed_get_int_array(weed_plant_t *, const char *key, weed_error_t *);
uint32_t *weed_get_uint_array(weed_plant_t *, const char *key, weed_error_t *);
double *weed_get_double_array(weed_plant_t *, const char *key, weed_error_t *);
weed_boolean_t *weed_get_boolean_array(weed_plant_t *, const char *key, weed_error_t *);
int64_t *weed_get_int64_array(weed_plant_t *, const char *key, weed_error_t *);
uint64_t *weed_get_uint64_array(weed_plant_t *, const char *key, weed_error_t *);
char **weed_get_string_array(weed_plant_t *, const char *key, weed_error_t *);
weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *, const char *key, weed_error_t *);
void **weed_get_voidptr_array(weed_plant_t *, const char *key, weed_error_t *);
weed_plant_t **weed_get_plantptr_array(weed_plant_t *, const char *key, weed_error_t *);
void **weed_get_custom_array(weed_plant_t *, const char *key, uint32_t seed_type, weed_error_t *);

int32_t *weed_get_int_array_counted(weed_plant_t *, const char *key, int *count);
uint32_t *weed_get_uint_array_counted(weed_plant_t *, const char *key, int *count);
double *weed_get_double_array_counted(weed_plant_t *, const char *key, int *count);
weed_boolean_t *weed_get_boolean_array_counted(weed_plant_t *, const char *key, int *count);
int64_t *weed_get_int64_array_counted(weed_plant_t *, const char *key, int *count);
uint64_t *weed_get_uint64_array_counted(weed_plant_t *, const char *key, int *count);
char **weed_get_string_array_counted(weed_plant_t *, const char *key, int *count);
weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_custom_array_counted(weed_plant_t *, const char *key, uint32_t seed_type, int *count);

  // check if leaf value can be changed: returns WEED_ERROR_IMMUTABLE, WEED_SUCCESS, or WEED_ERROR_NOSUCH_LEAF
weed_error_t weed_leaf_is_immutable(weed_plant_t *, const char *key);

  // check if leaf value can be deleted: returns WEED_ERROR_UNDELETABLE, WEED_SUCCESS, or WEED_ERROR_NOSUCH_LEAF
weed_error_t weed_leaf_is_undeletable(weed_plant_t *, const char *key);

/* make a copy dest leaf from src leaf. Pointers are copied by reference only, but strings are allocated */
weed_error_t weed_leaf_copy(weed_plant_t *dest, const char *keyt, weed_plant_t *src, const char *keyf);

/* copy only the nth element; if either leaf has n or fewer elements then WEED_ERROR_NOSUCH_ELEMENT is returned */
weed_error_t weed_leaf_copy_nth(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf, int n);

/* convenience functions for the more common case where both keys match */
weed_error_t weed_leaf_dup(weed_plant_t *dst, weed_plant_t *src, const char *key);
weed_error_t weed_leaf_dup_nth(weed_plant_t *dst, weed_plant_t *src, const char *key, int n);

/* copy all leaves in src to dst using weed_leaf_copy */
weed_plant_t *weed_plant_copy(weed_plant_t *src);

/* returns WEED_TRUE if nth elements of 2 leaves are identical (both exist, both seed_types equal, both values equal)
     if elem < 0, then all elements must match */
int weed_leaf_elements_equate(weed_plant_t *p0, const char *k0, weed_plant_t *p1, const char *k1, int elem);

/* returns the value of the "type" leaf; returns WEED_PLANT_UNKNOWN if plant is NULL */
int32_t weed_plant_get_type(weed_plant_t *);

#define weed_get_plant_type(p) weed_plant_get_type((p))
  
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_UTILS_H__
