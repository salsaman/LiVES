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

/* (C) G. Finch, 2005 - 2019 */

#ifndef __WEED_UTILS_H__
#define __WEED_UTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(__WEED_HOST__) || defined(__LIBWEED__)
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
weed_error_t weed_set_funcptr_value(weed_plant_t *, const char *key, void *value);
weed_error_t weed_set_voidptr_value(weed_plant_t *, const char *key, void *value);
weed_error_t weed_set_plantptr_value(weed_plant_t *, const char *key, weed_plant_t *value);
weed_error_t weed_set_custom_value(weed_plant_t *, const char *key, int32_t seed_type, void *value);

int32_t weed_get_int_value(weed_plant_t *, const char *key, weed_error_t *error);
double weed_get_double_value(weed_plant_t *, const char *key, weed_error_t *error);
int32_t weed_get_boolean_value(weed_plant_t *, const char *key, weed_error_t *error);
int64_t weed_get_int64_value(weed_plant_t *, const char *key, weed_error_t *error);
char *weed_get_string_value(weed_plant_t *, const char *key, weed_error_t *error);
weed_funcptr_t weed_get_funcptr_value(weed_plant_t *, const char *key, weed_error_t *error);
void *weed_get_voidptr_value(weed_plant_t *, const char *key, weed_error_t *error);
weed_plant_t *weed_get_plantptr_value(weed_plant_t *, const char *key, weed_error_t *error);
void *weed_get_custom_value(weed_plant_t *, const char *key, int32_t seed_type, weed_error_t *error);

weed_error_t weed_set_int_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *values);
weed_error_t weed_set_double_array(weed_plant_t *, const char *key, weed_size_t num_elems, double *values);
weed_error_t weed_set_boolean_array(weed_plant_t *, const char *key, weed_size_t num_elems, int32_t *values);
weed_error_t weed_set_int64_array(weed_plant_t *, const char *key, weed_size_t num_elems, int64_t *values);
weed_error_t weed_set_string_array(weed_plant_t *, const char *key, weed_size_t num_elems, char **values);
weed_error_t weed_set_funcptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_funcptr_t *values);
weed_error_t weed_set_voidptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, void **values);
weed_error_t weed_set_plantptr_array(weed_plant_t *, const char *key, weed_size_t num_elems, weed_plant_t **values);
weed_error_t weed_set_custom_array(weed_plant_t *, const char *key, int32_t seed_type, weed_size_t num_elems, void **values);

int32_t *weed_get_int_array(weed_plant_t *, const char *key, weed_error_t *error);
double *weed_get_double_array(weed_plant_t *, const char *key, weed_error_t *error);
int32_t *weed_get_boolean_array(weed_plant_t *, const char *key, weed_error_t *error);
int64_t *weed_get_int64_array(weed_plant_t *, const char *key, weed_error_t *error);
char **weed_get_string_array(weed_plant_t *, const char *key, weed_error_t *error);
weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *, const char *key, weed_error_t *error);
void **weed_get_voidptr_array(weed_plant_t *, const char *key, weed_error_t *error);
weed_plant_t **weed_get_plantptr_array(weed_plant_t *, const char *key, weed_error_t *error);
void **weed_get_custom_array(weed_plant_t *, const char *key, int32_t seed_type, weed_error_t *error);

int32_t *weed_get_int_array_counted(weed_plant_t *, const char *key, int *count);
double *weed_get_double_array_counted(weed_plant_t *, const char *key, int *count);
int32_t *weed_get_boolean_array_counted(weed_plant_t *, const char *key, int *count);
int64_t *weed_get_int64_array_counted(weed_plant_t *, const char *key, int *count);
char **weed_get_string_array_counted(weed_plant_t *, const char *key, int *count);
weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *, const char *key, int *count);
weed_voidptr_t *weed_get_custom_array_counted(weed_plant_t *, const char *key, int32_t seed_type, int *count);

/* make a copy dest leaf from src leaf. Pointers are copied by reference only, but strings are allocated */
weed_error_t weed_leaf_copy(weed_plant_t *dest, const char *keyt, weed_plant_t *src, const char *keyf);

/* copy all leaves in src to dst using weed_leaf_copy */
weed_plant_t *weed_plant_copy(weed_plant_t *src);

/* returns the value of the "type" leaf; returns WEED_PLANT_UNKNOWN if plant is NULL */
int32_t weed_get_plant_type(weed_plant_t *);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_weed_abi_compat(int32_t higher, int32_t lower);

/* returns WEED_TRUE if higher and lower versions are compatible, WEED_FALSE if not */
int check_filter_api_compat(int32_t higher, int32_t lower);

/* plugin only function; host should pass a pointer to this to the plugin as the sole parameter when calling  weed_setup()
in the plugin */
weed_plant_t *weed_bootstrap(weed_default_getter_f *, int32_t plugin_weed_min_api_version, int32_t plugin_weed_max_api_version,
                             int32_t plugin_filter_min_api_version, int32_t plugin_filter_max_api_version);

/* host only functions */
// set flags for each leaf in a plant. If ign_prefix is not NULL, ignore leaves with keys that begin with ign_prefix
// this enables a host to do: weed_add_plant_flags(plant, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_")
void weed_add_plant_flags(weed_plant_t *, int32_t flags, const char *ign_prefix);
void weed_clear_plant_flags(weed_plant_t *t, int32_t flags, const char *ign_prefix);

/* typedef for host callback from weed_bootstrap; host MUST return a host_info, either the original one or a new one */
typedef weed_plant_t *(*weed_host_info_callback_f)(weed_plant_t *host_info, void *user_data);

/* set a host callback function to be called from within weed_bootstrap() */
void weed_set_host_info_callback(weed_host_info_callback_f, void *user_data);
#endif

#ifdef __WEED_PLUGIN__

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
static int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  if (__weed_check_leaf__(plant, key) == WEED_SUCCESS) return WEED_TRUE;
  return WEED_FALSE;
}

static weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_double_value(weed_plant_t *plant, const char *key, double value) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_boolean_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_funcptr_value(weed_plant_t *plant, const char *key, weed_funcptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, 1, (weed_voidptr_t)&value);
}

static inline weed_error_t __weed_leaf_check__(weed_plant_t *plant, const char *key, int32_t seed_type) {
  weed_error_t err;
  if ((err = __weed_check_leaf__(plant, key)) != WEED_SUCCESS) return err;
  if (weed_leaf_seed_type(plant, key) != seed_type) {
    return WEED_ERROR_WRONG_SEED_TYPE;
  }
  return WEED_SUCCESS;
}

static inline weed_error_t __weed_value_get__(weed_plant_t *plant, const char *key, int32_t seed_type, weed_voidptr_t retval) {
  weed_error_t err;
  if ((err = __weed_leaf_check__(plant, key, seed_type)) != WEED_SUCCESS) return err;
  return __weed_get_value__(plant, key, retval);
}

static int32_t weed_get_int_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = 0;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_INT, &retval);
  if (error) *error = err;
  return retval;
}

static double weed_get_double_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double retval = 0.;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_DOUBLE, &retval);
  if (error) *error = err;
  return retval;
}

static int32_t weed_get_boolean_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = WEED_FALSE;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_BOOLEAN, &retval);
  if (error) *error = err;
  return retval;
}

static int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t retval = 0;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_INT64, &retval);
  if (error) *error = err;
  return retval;
}

static char *weed_get_string_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_size_t size;
  char *retval = NULL;
  weed_error_t err = __weed_check_leaf__(plant, key);
  if (err != WEED_SUCCESS) {
    if (error) *error = err;
    return NULL;
  }
  if (weed_leaf_seed_type(plant, key) != WEED_SEED_STRING) {
    if (error) *error = WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  if (!(retval = (char *)weed_malloc(weed_leaf_element_size(plant, key, 0) + 1))) {
    if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }
  if ((err = __weed_value_get__(plant, key, WEED_SEED_STRING, &retval)) != WEED_SUCCESS) {
    if (retval) {
      weed_free(retval);
      retval = NULL;
    }
  }
  if (error) *error = err;
  return retval;
}

static weed_voidptr_t weed_get_voidptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_VOIDPTR, &retval);
  if (error) *error = err;
  return retval;
}

static weed_funcptr_t weed_get_funcptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_funcptr_t retval = NULL;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_FUNCPTR, &retval);
  if (error) *error = err;
  return retval;
}

static weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t *retval = NULL;
  weed_error_t err = __weed_value_get__(plant, key, WEED_SEED_PLANTPTR, &retval);
  if (error) *error = err;
  return retval;
}

static inline weed_error_t __weed_get_values__(weed_plant_t *plant, const char *key, size_t dsize, char **retval,
    int *nvals) {
  weed_error_t err;
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);
  if (nvals) *nvals = 0;
  if ((*retval = (char *)weed_malloc(num_elems * dsize)) == NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  for (int i = 0; (weed_size_t)i < num_elems; i++) {
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      weed_free(*retval);
      *retval = NULL;
      return err;
    }
  }
  if (nvals) *nvals = (int)num_elems;
  return WEED_SUCCESS;
}

static inline weed_voidptr_t __weed_get_array__(weed_plant_t *plant, const char *key,
    int32_t seed_type, weed_size_t typelen, weed_voidptr_t retvals, weed_error_t *error,
    int *nvals) {
  weed_error_t err = __weed_leaf_check__(plant, key, seed_type);
  if (err != WEED_SUCCESS) {
    if (error) *error = err;
    return NULL;
  }
  err = __weed_get_values__(plant, key, typelen, (char **)&retvals, nvals);
  if (error) *error = err;
  return retvals;
}

static int32_t *weed_get_int_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int32_t *retvals = NULL;
  return (int32_t *)(__weed_get_array__(plant, key, WEED_SEED_INT, 4, (char **)&retvals, NULL, count));
}

static int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(__weed_get_array__(plant, key, WEED_SEED_INT, 4, (char **)&retvals, error, NULL));
}

static double *weed_get_double_array_counted(weed_plant_t *plant, const char *key, int *count) {
  double *retvals = NULL;
  return (double *)(__weed_get_array__(plant, key, WEED_SEED_DOUBLE, 8, (char **)&retvals, NULL, count));
}

static double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double *retvals = NULL;
  return (double *)(__weed_get_array__(plant, key, WEED_SEED_DOUBLE, 8, (char **)&retvals, error, NULL));
}

static int32_t *weed_get_boolean_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int32_t *retvals = NULL;
  return (int32_t *)(__weed_get_array__(plant, key, WEED_SEED_BOOLEAN, 4, (char **)&retvals, NULL, count));
}

static int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(__weed_get_array__(plant, key, WEED_SEED_BOOLEAN, 4, (char **)&retvals, error, NULL));
}

static int64_t *weed_get_int64_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int64_t *retvals = NULL;
  return (int64_t *)(__weed_get_array__(plant, key, WEED_SEED_INT64, 8, (char **)&retvals, NULL, count));
}

static int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t *retvals = NULL;
  return (int64_t *)(__weed_get_array__(plant, key, WEED_SEED_INT64, 8, (char **)&retvals, error, NULL));
}

static char **__weed_get_string_array__(weed_plant_t *plant, const char *key, weed_error_t *error, int *count) {
  weed_size_t num_elems, size;
  char **retvals = NULL;
  weed_error_t err = __weed_leaf_check__(plant, key, WEED_SEED_STRING);
  if (err != WEED_SUCCESS) {
    if (error) *error = err;
    return NULL;
  }
  if ((num_elems = weed_leaf_num_elements(plant, key)) == 0) return NULL;
  if ((retvals = (char **)weed_malloc(num_elems * sizeof(char *))) == NULL) {
    if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }
  for (int i = 0; i < num_elems; i++) {
    if (!(retvals[i] = (char *)weed_malloc((size = weed_leaf_element_size(plant, key, i)) + 1))) {
      for (--i; i >= 0; i--) weed_free(retvals[i]);
      if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
      weed_free(retvals);
      return NULL;
    }
    if ((err = weed_leaf_get(plant, key, i, &retvals[i])) != WEED_SUCCESS) {
      for (--i; i >= 0; i--) weed_free(retvals[i]);
      if (error) *error = err;
      weed_free(retvals);
      return NULL;
    }
    retvals[i][size] = '\0';
  }
  if (error) *error = WEED_SUCCESS;
  if (count) *count = num_elems;
  return retvals;
}

static char **weed_get_string_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return __weed_get_string_array__(plant, key, NULL, count);
}

static char **weed_get_string_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return __weed_get_string_array__(plant, key, error, NULL);
}

static weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  weed_funcptr_t *retvals = NULL;
  return (weed_funcptr_t *)(__weed_get_array__(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE,
                            (char **)&retvals, NULL, count));
}

static weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_funcptr_t *retvals = NULL;
  return (weed_funcptr_t *)(__weed_get_array__(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE,
                            (char **)&retvals, error, NULL));
}

static weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(__weed_get_array__(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE,
                            (char **)&retvals, NULL, count));
}

static weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(__weed_get_array__(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE,
                            (char **)&retvals, error, NULL));
}

static weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  weed_plant_t **retvals = NULL;
  return (weed_plant_t **)(__weed_get_array__(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE,
                           (char **)&retvals, NULL, count));
}

static weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t **retvals = NULL;
  return (weed_plant_t **)(__weed_get_array__(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE,
                           (char **)&retvals, error, NULL));
}

static weed_error_t weed_set_int_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_double_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, double *values) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_boolean_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_int64_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int64_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_string_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, char **values) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_funcptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_funcptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_plant_t **values) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, num_elems, (weed_voidptr_t)values);
}

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
