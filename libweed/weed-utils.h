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

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#else
#include "weed.h"
#endif
  
/* some nice macros, e.g
  double x = WEED_LEAF_GET(myplant, "mykey", double);
*/
#define WEED_LEAF_GET(plant, key, type) weed_get_##type##_value(plant, key, NULL)
#define WEED_LEAF_GET_ARRAY_COUNTED(plant, key, type, counter) weed_get_##type##_array_counted(plant, key, &count)
#define WEED_LEAF_SET(plant, key, type, value) weed_set_##type##_value(plant, key, value)
#define WEED_LEAF_SET_ARRAY(plant, key, type, nvals, array) weed_set_##type##_array(plant, key, nvals, array)

#if defined(__WEED_HOST__) || defined(__LIBWEED__)
typedef int (*weed_memcmp_f)(const void *, const void *, size_t);

void weed_utils_set_custom_memfuncs(weed_malloc_f malloc_func, weed_calloc_f calloc_func, weed_memcpy_f memcpy_func,
                                    weed_memcmp_f memcmp_func, weed_free_f free_func);

/* check if leaf exists and has a value, returns WEED_TRUE or WEED_FALSE */
int weed_plant_has_leaf(weed_plant_t *, const char *key);

/* check if leaf exists; may have a seed_type but no value set */
int weed_leaf_exists(weed_plant_t *, const char *key);

weed_error_t weed_set_int_value(weed_plant_t *, const char *key, int32_t);
weed_error_t weed_set_double_value(weed_plant_t *, const char *key, double);
weed_error_t weed_set_boolean_value(weed_plant_t *, const char *key, int32_t);
weed_error_t weed_set_int64_value(weed_plant_t *, const char *key, int64_t);
weed_error_t weed_set_string_value(weed_plant_t *, const char *key, const char *);
weed_error_t weed_set_funcptr_value(weed_plant_t *, const char *key, weed_funcptr_t);
weed_error_t weed_set_voidptr_value(weed_plant_t *, const char *key, void *);
weed_error_t weed_set_plantptr_value(weed_plant_t *, const char *key, weed_plant_t *);
weed_error_t weed_set_custom_value(weed_plant_t *, const char *key, uint32_t seed_type, void *);

int32_t weed_get_int_value(weed_plant_t *, const char *key, weed_error_t *);
double weed_get_double_value(weed_plant_t *, const char *key, weed_error_t *);
int32_t weed_get_boolean_value(weed_plant_t *, const char *key, weed_error_t *);
int64_t weed_get_int64_value(weed_plant_t *, const char *key, weed_error_t *);
char *weed_get_string_value(weed_plant_t *, const char *key, weed_error_t *);
weed_funcptr_t weed_get_funcptr_value(weed_plant_t *, const char *key, weed_error_t *);
void *weed_get_voidptr_value(weed_plant_t *, const char *key, weed_error_t *);
weed_plant_t *weed_get_plantptr_value(weed_plant_t *, const char *key, weed_error_t *);
void *weed_get_custom_value(weed_plant_t *, const char *key, uint32_t seed_type, weed_error_t *);

weed_error_t weed_set_int_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *);
weed_error_t weed_set_double_array(weed_plant_t *, const char *key, weed_size_t num_elems, double *);
weed_error_t weed_set_boolean_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *);
weed_error_t weed_set_int64_array(weed_plant_t *, const char *key, weed_size_t num_elems, int64_t *);
weed_error_t weed_set_string_array(weed_plant_t *, const char *key, weed_size_t num_elems, char **);
weed_error_t weed_set_funcptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_funcptr_t *);
weed_error_t weed_set_voidptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, void **);
weed_error_t weed_set_plantptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_plant_t **);
weed_error_t weed_set_custom_array(weed_plant_t *, const char *key, uint32_t seed_type, weed_size_t num_elems, void **);

int32_t *weed_get_int_array(weed_plant_t *, const char *key, weed_error_t *);
double *weed_get_double_array(weed_plant_t *, const char *key, weed_error_t *);
int32_t *weed_get_boolean_array(weed_plant_t *, const char *key, weed_error_t *);
int64_t *weed_get_int64_array(weed_plant_t *, const char *key, weed_error_t *);
char **weed_get_string_array(weed_plant_t *, const char *key, weed_error_t *);
weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *, const char *key, weed_error_t *);
void **weed_get_voidptr_array(weed_plant_t *, const char *key, weed_error_t *);
weed_plant_t **weed_get_plantptr_array(weed_plant_t *, const char *key, weed_error_t *);
void **weed_get_custom_array(weed_plant_t *, const char *key, uint32_t seed_type, weed_error_t *);

int32_t *weed_get_int_array_counted(weed_plant_t *, const char *key, int *count);
double *weed_get_double_array_counted(weed_plant_t *, const char *key, int *count);
int32_t *weed_get_boolean_array_counted(weed_plant_t *, const char *key, int *count);
int64_t *weed_get_int64_array_counted(weed_plant_t *, const char *key, int *count);
char **weed_get_string_array_counted(weed_plant_t *, const char *key, int *count);
weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_custom_array_counted(weed_plant_t *, const char *key, uint32_t seed_type, int *count);

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
int32_t weed_get_plant_type(weed_plant_t *);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_weed_abi_compat(int32_t higher, int32_t lower);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_filter_api_compat(int32_t higher, int32_t lower);

#ifdef __WEED_EFFECTS_H__
/* plugin only function; host should pass a pointer to this to the plugin as the sole parameter when calling  weed_setup()
  in the plugin */
weed_plant_t *weed_bootstrap(weed_default_getter_f *, int32_t plugin_weed_min_api_version, int32_t plugin_weed_max_api_version,
                             int32_t plugin_filter_min_api_version, int32_t plugin_filter_max_api_version);
#endif

/* typedef for host callback from weed_bootstrap; host MUST return a host_info, either the original one or a new one */
typedef weed_plant_t *(*weed_host_info_callback_f)(weed_plant_t *host_info, void *user_data);

/* set a host callback function to be called from within weed_bootstrap() */
void weed_set_host_info_callback(weed_host_info_callback_f, void *user_data);
#endif

#ifdef __WEED_PLUGIN__

#ifndef FN_TYPE
#define FN_TYPE static inline
#endif

/* functions need to be defined here for the plugin, else it will use the host versions, breaking function overloading */
#ifdef __weed_get_value__
#undef __weed_get_value__
#endif
#ifdef __weed_check_leaf__
#undef __weed_check_leaf__
#endif

#define __weed_get_value__(plant, key, value) weed_leaf_get(plant, key, 0, value)
#define __weed_check_leaf__(plant, key) __weed_get_value__(plant, key, NULL)

/* check for existence of a leaf; leaf must must have a value and not just a seed_type, returns WEED_TRUE or WEED_FALSE */
FN_TYPE int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  return __weed_check_leaf__(plant, key) == WEED_SUCCESS ? WEED_TRUE : WEED_FALSE;}

#define _WEED_SET_(stype) return weed_leaf_set(plant, key, WEED_SEED_##stype, 1, (weed_voidptr_t)&value);

  /*							--- SINGLE VALUE SETTERS ---						*/
FN_TYPE weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {_WEED_SET_(INT)}
FN_TYPE weed_error_t weed_set_double_value(weed_plant_t *plant, const char *key, double value) {_WEED_SET_(DOUBLE)}
FN_TYPE weed_error_t weed_set_boolean_value(weed_plant_t *plant, const char *key, int32_t value) {_WEED_SET_(BOOLEAN)}
FN_TYPE weed_error_t weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {_WEED_SET_(INT64)}
FN_TYPE weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {_WEED_SET_(STRING)}
FN_TYPE weed_error_t weed_set_funcptr_value(weed_plant_t *plant, const char *key, weed_funcptr_t value) {_WEED_SET_(FUNCPTR)}
FN_TYPE weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {_WEED_SET_(VOIDPTR)}
FN_TYPE weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {_WEED_SET_(PLANTPTR)}

#undef _WEED_SET_
FN_TYPE weed_error_t __weed_leaf_check__(weed_plant_t *plant, const char *key, uint32_t seed_type) {
  weed_error_t err = __weed_check_leaf__(plant, key);
  return err != WEED_SUCCESS ? err
    : weed_leaf_seed_type(plant, key) != seed_type ? WEED_ERROR_WRONG_SEED_TYPE : WEED_SUCCESS;}

FN_TYPE weed_voidptr_t __weed_value_get__(weed_plant_t *plant, const char *key, uint32_t seed_type,
    weed_voidptr_t retval, weed_error_t *error) {
  weed_error_t err, *perr = (error ? error : &err);
  if ((*perr = __weed_leaf_check__(plant, key, seed_type)) == WEED_SUCCESS) *perr = __weed_get_value__(plant, key, retval);
  return retval;}

#define _WEED_GET_(ctype, stype) ctype retval; \
  return *((ctype *)(__weed_value_get__(plant, key, WEED_SEED_##stype, (weed_voidptr_t)&retval, error)));

  /*							--- SINGLE VALUE GETTERS ---						*/
FN_TYPE int32_t weed_get_int_value(weed_plant_t *plant, const char *key, weed_error_t *error) {_WEED_GET_(int32_t, INT)}
FN_TYPE double weed_get_double_value(weed_plant_t *plant, const char *key, weed_error_t *error) {_WEED_GET_(double, DOUBLE)}
FN_TYPE int32_t weed_get_boolean_value(weed_plant_t *plant, const char *key, weed_error_t *error) {_WEED_GET_(int32_t, BOOLEAN)}
FN_TYPE int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {_WEED_GET_(int64_t, INT64)}

  FN_TYPE char *weed_get_string_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  char *retval = NULL; weed_error_t err, *perr = (error ? error : &err);
  if ((*perr =  __weed_leaf_check__(plant, key, WEED_SEED_STRING)) == WEED_SUCCESS) {
    if (!(retval = (char *)weed_malloc(weed_leaf_element_size(plant, key, 0) + 1))) *perr = WEED_ERROR_MEMORY_ALLOCATION;
    else {
      retval = *((char **)(__weed_value_get__(plant, key, WEED_SEED_STRING, &retval, perr)));
      if (*perr != WEED_SUCCESS) {if (retval) weed_free(retval); retval = NULL;}
      }} return retval;}

FN_TYPE weed_voidptr_t weed_get_voidptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _WEED_GET_(weed_voidptr_t, VOIDPTR)}

FN_TYPE weed_funcptr_t weed_get_funcptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _WEED_GET_(weed_funcptr_t, FUNCPTR)}

FN_TYPE weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _WEED_GET_(weed_plantptr_t, PLANTPTR)}

#undef _WEED_GET_
FN_TYPE weed_error_t __weed_get_values__(weed_plant_t *plant, const char *key, size_t dsize, char **retval,
    int *nvals) {
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);
  if (nvals) *nvals = 0;
  if (!(*retval = (char *)weed_calloc(num_elems, dsize))) return WEED_ERROR_MEMORY_ALLOCATION;
  for (int i = 0; (weed_size_t)i < num_elems; i++) {
    weed_error_t err;
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      weed_free(*retval); *retval = NULL;
      return err;
    }}
  if (nvals) *nvals = (int)num_elems;
  return WEED_SUCCESS;}

FN_TYPE weed_voidptr_t __weed_get_arrayx__(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_size_t typelen,
					   weed_error_t *error, int *nvals) {
  weed_error_t err, *perr = (error ? error : &err); char *retvals = NULL;
  if ((*perr = __weed_leaf_check__(plant, key, seed_type)) != WEED_SUCCESS) return NULL;
  *perr = __weed_get_values__(plant, key, typelen, (char **)&retvals, nvals); return retvals;}

#define _ARRAY_COUNT_(ctype, stype) \
  return (ctype *)(__weed_get_arrayx__(plant, key, WEED_SEED_##stype, sizeof(ctype), NULL, count));
#define _ARRAY_NORM_(ctype, stype) \
  return (ctype *)(__weed_get_arrayx__(plant, key, WEED_SEED_##stype, sizeof(ctype), error, NULL));

/*							--- ARRAY GETTERS ---						*/
FN_TYPE int32_t *weed_get_int_array_counted(weed_plant_t *plant, const char *key, int *count){_ARRAY_COUNT_(int32_t, INT)}
FN_TYPE int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error){_ARRAY_NORM_(int32_t, INT)}
FN_TYPE double *weed_get_double_array_counted(weed_plant_t *plant, const char *key, int *count){_ARRAY_COUNT_(double, DOUBLE)}
FN_TYPE double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error){_ARRAY_NORM_(double, DOUBLE)}
FN_TYPE int32_t *weed_get_boolean_array_counted(weed_plant_t *plant, const char *key, int *count){
  _ARRAY_COUNT_(int32_t, BOOLEAN)}
FN_TYPE int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error){
  _ARRAY_NORM_(int32_t, BOOLEAN)}
FN_TYPE int64_t *weed_get_int64_array_counted(weed_plant_t *plant, const char *key, int *count) {_ARRAY_COUNT_(int64_t, INT64)}
FN_TYPE int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {_ARRAY_NORM_(int64_t, INT64)}

FN_TYPE char **__weed_get_string_array__(weed_plant_t *plant, const char *key, weed_error_t *error, int *count) {
  weed_size_t num_elems; char **retvals = NULL; weed_error_t err, *perr = (error ? error : &err); uint32_t i;
  if (count) *count = 0;
  if ((*perr = __weed_leaf_check__(plant, key, WEED_SEED_STRING)) != WEED_SUCCESS
      || !(num_elems = weed_leaf_num_elements(plant, key))) return NULL;
  if (!(retvals = (char **)weed_calloc(num_elems, sizeof(char *)))) *perr = WEED_ERROR_MEMORY_ALLOCATION;
  else {
    for (i = 0; i < num_elems; i++) {
      if (!(retvals[i] = (char *)weed_malloc(weed_leaf_element_size(plant, key, i)) + 1)) {
        *perr = WEED_ERROR_MEMORY_ALLOCATION;
        goto __cleanup;
      }
      if ((*perr = weed_leaf_get(plant, key, i, &retvals[i])) != WEED_SUCCESS) goto __cleanup;
    }
    if (count) *count = num_elems;
  }
  return retvals;

__cleanup:
  while (i > 0) {weed_free(retvals[--i]);} weed_free(retvals); return NULL;
}

FN_TYPE char **weed_get_string_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return __weed_get_string_array__(plant, key, NULL, count);}
FN_TYPE char **weed_get_string_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return __weed_get_string_array__(plant, key, error, NULL);}

FN_TYPE weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  _ARRAY_COUNT_(weed_funcptr_t, FUNCPTR)}
FN_TYPE weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _ARRAY_NORM_(weed_funcptr_t, FUNCPTR)}

FN_TYPE weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  _ARRAY_COUNT_(weed_voidptr_t, VOIDPTR)}
FN_TYPE weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _ARRAY_NORM_(weed_voidptr_t, VOIDPTR)}

FN_TYPE weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  _ARRAY_COUNT_(weed_plantptr_t, PLANTPTR)}
FN_TYPE weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  _ARRAY_NORM_(weed_plantptr_t, PLANTPTR)}

#undef _ARRAY_COUNT_
#undef _ARRAY_NORM_
#define _SET_ARRAY_(stype)  return weed_leaf_set(plant, key, WEED_SEED_##stype, num_elems, (weed_voidptr_t)values);

 /*							--- ARRAY SETTERS ---						*/
FN_TYPE weed_error_t weed_set_int_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  _SET_ARRAY_(INT)}

FN_TYPE weed_error_t weed_set_double_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, double *values) {
  _SET_ARRAY_(DOUBLE)}

FN_TYPE weed_error_t weed_set_boolean_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  _SET_ARRAY_(BOOLEAN)}

FN_TYPE weed_error_t weed_set_int64_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int64_t *values) {
  _SET_ARRAY_(INT64)}

FN_TYPE weed_error_t weed_set_string_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, char **values) {
  _SET_ARRAY_(STRING)}

FN_TYPE weed_error_t weed_set_funcptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_funcptr_t *values) {
  _SET_ARRAY_(FUNCPTR)}

FN_TYPE weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_voidptr_t *values) {
  _SET_ARRAY_(VOIDPTR)}

FN_TYPE weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_plant_t **values) {
  _SET_ARRAY_(PLANTPTR)}
#undef _SET_ARRAY_

#undef __weed_get_value__
#undef __weed_check_leaf__
#endif

#define WEED_LEAF_MIN_WEED_API_VERSION   "min_weed_api_version"
#define WEED_LEAF_MAX_WEED_API_VERSION   "max_weed_api_version"
#define WEED_LEAF_MIN_WEED_ABI_VERSION WEED_LEAF_MIN_WEED_API_VERSION
#define WEED_LEAF_MAX_WEED_ABI_VERSION WEED_LEAF_MAX_WEED_API_VERSION
#define WEED_LEAF_MIN_FILTER_API_VERSION "min_weed_filter_version"
#define WEED_LEAF_MAX_FILTER_API_VERSION "max_weed_filter_version"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_UTILS_H__
