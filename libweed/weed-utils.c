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

   partlyly based on LiViDO, which is developed by:

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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2019 */

#include <string.h>
#include <stdlib.h>

#define __LIBWEED__

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "weed.h"
#include "weed-effects.h"
#include "weed-utils.h"
#endif

/////////////////////////////////////////////////////////////////

#define weed_get_value(plant, key, value) weed_leaf_get(plant, key, 0, value)
#define weed_check_leaf(plant, key) weed_get_value(plant, key, NULL)


int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, must have a value and not just a seed_type
  if (plant != NULL) {
    weed_error_t err = weed_check_leaf(plant, key);
    if (err == WEED_SUCCESS) return WEED_TRUE;
  }
  return WEED_FALSE;
}


int weed_leaf_exists(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, may have only a seed_type but no value set
  if (plant != NULL) {
    weed_error_t err = weed_check_leaf(plant, key);
    if (err != WEED_ERROR_NOSUCH_LEAF) return WEED_TRUE;
  }
  return WEED_FALSE;
}


/////////////////////////////////////////////////////////////////
// leaf setters

weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_double_value(weed_plant_t *plant, const char *key, double value) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_boolean_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_custom_value(weed_plant_t *plant, const char *key, int32_t seed_type, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, seed_type, 1, (weed_voidptr_t)&value);
}


//////////////////////////////////////////////////////////////////////////////////////////////////

static inline weed_error_t weed_leaf_check(weed_plant_t *plant, const char *key, int32_t seed_type) {
  weed_error_t err;
  if ((err = weed_check_leaf(plant, key)) != WEED_SUCCESS) return err;
  if (weed_leaf_seed_type(plant, key) != seed_type) {
    return WEED_ERROR_WRONG_SEED_TYPE;
  }
  return WEED_SUCCESS;
}


static inline weed_error_t weed_value_get(weed_plant_t *plant, const char *key, int32_t seed_type, weed_voidptr_t retval) {
  weed_error_t err;
  if ((err = weed_leaf_check(plant, key, seed_type)) != WEED_SUCCESS) return err;
  return weed_get_value(plant, key, retval);
}


////////////////////////////////////////////////////////////

int32_t weed_get_int_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT, &retval);
  if (error != NULL) *error = err;
  return retval;
}


double weed_get_double_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double retval = 0.;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_DOUBLE, &retval);
  if (error != NULL) *error = err;
  return retval;
}


int32_t weed_get_boolean_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = WEED_FALSE;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_BOOLEAN, &retval);
  if (error != NULL) *error = err;
  return retval;
}


int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT64, &retval);
  if (error != NULL) *error = err;
  return retval;
}


char *weed_get_string_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_size_t size;
  char *retval = NULL;
  weed_error_t err = weed_check_leaf(plant, key);
  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }
  if (weed_leaf_seed_type(plant, key) != WEED_SEED_STRING) {
    if (error != NULL) *error = WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  if ((retval = (char *)malloc((size = weed_leaf_element_size(plant, key, 0)) + 1)) == NULL) {
    if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }
  if ((err = weed_value_get(plant, key, WEED_SEED_STRING, &retval)) != WEED_SUCCESS) {
    if (retval != NULL) {
      free(retval);
      retval = NULL;
    }
  }
  if (error != NULL) *error = err;
  return retval;
}


weed_voidptr_t weed_get_voidptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_VOIDPTR, &retval);
  if (error != NULL) *error = err;
  return retval;
}


weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t *retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_PLANTPTR, &retval);
  if (error != NULL) *error = err;
  return retval;
}


weed_voidptr_t weed_get_custom_value(weed_plant_t *plant, const char *key, int32_t seed_type, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, seed_type, &retval);
  if (error != NULL) *error = err;
  return retval;
}


////////////////////////////////////////////////////////////

static inline weed_error_t weed_get_values(weed_plant_t *plant, const char *key, size_t dsize, char **retval) {
  weed_error_t err;
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);
  int i;

  if ((*retval = (char *)malloc(num_elems * dsize)) == NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = 0; i < num_elems; i++) {
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      free(*retval);
      *retval = NULL;
      return err;
    }
  }

  return WEED_SUCCESS;
}


static inline weed_voidptr_t weed_get_array(weed_plant_t *plant, const char *key,
    int32_t seed_type, weed_size_t typelen, weed_voidptr_t retvals, weed_error_t *error) {
  weed_error_t err = weed_leaf_check(plant, key, seed_type);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, typelen, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_INT, 4, (uint8_t **)&retvals, error));
}


double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double *retvals = NULL;
  return (double *)(weed_get_array(plant, key, WEED_SEED_DOUBLE, 8, (uint8_t **)&retvals, error));
}


int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_BOOLEAN);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, 4, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_INT64);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, 8, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


char **weed_get_string_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_size_t num_elems, size;
  char **retvals = NULL;
  int i;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_STRING);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  if ((num_elems = weed_leaf_num_elements(plant, key)) == 0) return NULL;

  if ((retvals = (char **)malloc(num_elems * sizeof(char *))) == NULL) {
    if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i = 0; i < num_elems; i++) {
    if ((retvals[i] = (char *)malloc((size = weed_leaf_element_size(plant, key, i)) + 1)) == NULL) {
      for (--i; i >= 0; i--) free(retvals[i]);
      if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
      free(retvals);
      return NULL;
    }
    if ((err = weed_leaf_get(plant, key, i, &retvals[i])) != WEED_SUCCESS) {
      for (--i; i >= 0; i--) free(retvals[i]);
      if (error != NULL) *error = err;
      free(retvals);
      return NULL;
    }
    memset(retvals[i] + size, 0, 1);
  }
  if (error != NULL) *error = WEED_SUCCESS;
  return retvals;
}


weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_VOIDPTR);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, WEED_VOIDPTR_SIZE, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t **retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_PLANTPTR);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, WEED_VOIDPTR_SIZE, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


weed_voidptr_t *weed_get_custom_array(weed_plant_t *plant, const char *key, int32_t seed_type, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, seed_type);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, WEED_VOIDPTR_SIZE, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}


/////////////////////////////////////////////////////

weed_error_t weed_set_int_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_double_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, double *values) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_boolean_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_int64_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int64_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_string_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, char **values) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_plant_t **values) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, num_elems, (weed_voidptr_t)values);
}


weed_error_t weed_set_custom_array(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems, weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, seed_type, num_elems, (weed_voidptr_t)values);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t weed_get_plant_type(weed_plant_t *plant) {
  if (plant == NULL) return WEED_PLANT_UNKNOWN;
  return weed_get_int_value(plant, WEED_LEAF_TYPE, NULL);
}


weed_error_t weed_leaf_copy(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf) {
  // copy a leaf from src to dest
  // pointers are copied by reference only
  // strings are duplicated

  // if src or dst are NULL, nothing is copied and WEED_SUCCESS is returned

  // may return the standard errors:
  // WEED_SUCCESS, WEED_ERROR_MEMORY_ALLOCATION, WEED_ERROR_IMMUTABLE, WEED_ERROR_WRONG_SEED_TYPE
  weed_size_t num;
  weed_error_t err;
  int32_t seed_type;
  int i;

  if (dst == NULL || src == NULL) return WEED_SUCCESS;

  if ((err = weed_check_leaf(src, keyf)) == WEED_ERROR_NOSUCH_LEAF) return WEED_ERROR_NOSUCH_LEAF;

  seed_type = weed_leaf_seed_type(src, keyf);

  if (err == WEED_ERROR_NOSUCH_ELEMENT) {
    err = weed_leaf_set(dst, keyt, seed_type, 0, NULL);
  } else {
    num = weed_leaf_num_elements(src, keyf);
    switch (seed_type) {
    case WEED_SEED_INT: {
      int32_t *datai = weed_get_int_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_int_array(dst, keyt, num, datai);
      if (datai != NULL) free(datai);
    }
    break;
    case WEED_SEED_INT64: {
      int64_t *datai64 = weed_get_int64_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_int64_array(dst, keyt, num, datai64);
      if (datai64 != NULL) free(datai64);
    }
    break;
    case WEED_SEED_BOOLEAN: {
      int32_t *datai = weed_get_boolean_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_boolean_array(dst, keyt, num, datai);
      if (datai != NULL) free(datai);
    }
    break;
    case WEED_SEED_DOUBLE: {
      double *datad = weed_get_double_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_double_array(dst, keyt, num, datad);
      if (datad != NULL) free(datad);
    }
    break;
    case WEED_SEED_VOIDPTR: {
      weed_voidptr_t *datav = weed_get_voidptr_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_voidptr_array(dst, keyt, num, datav);
      if (datav != NULL) free(datav);
    }
    break;
    case WEED_SEED_PLANTPTR: {
      weed_plant_t **datap = weed_get_plantptr_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_plantptr_array(dst, keyt, num, datap);
      if (datap != NULL) free(datap);
    }
    break;
    case WEED_SEED_STRING: {
      char **datac = weed_get_string_array(src, keyf, &err);
      if (err == WEED_SUCCESS)
        err = weed_set_string_array(dst, keyt, num, datac);
      for (i = 0; i < num; i++) {
        free(datac[i]);
      }
      free(datac);
    }
    break;
    }
  }
  return err;
}


weed_plant_t *weed_plant_copy(weed_plant_t *src) {
  weed_plant_t *plant;
  weed_error_t err;
  char **proplist;
  char *prop;
  int i = 0;

  if (src == NULL) return NULL;

  plant = weed_plant_new(weed_get_int_value(src, WEED_LEAF_TYPE, &err));
  if (plant == NULL) return NULL;

  proplist = weed_plant_list_leaves(src);
  for (prop = proplist[0]; (prop = proplist[i]) != NULL && err == WEED_SUCCESS; i++) {
    if (err == WEED_SUCCESS) {
      if (strcmp(prop, WEED_LEAF_TYPE)) {
        err = weed_leaf_copy(plant, prop, src, prop);
        if (err == WEED_ERROR_IMMUTABLE || err == WEED_ERROR_WRONG_SEED_TYPE) err = WEED_SUCCESS; // ignore these errors
      }
    }
    free(prop);
  }
  free(proplist);

  if (err == WEED_ERROR_MEMORY_ALLOCATION) {
    //if (plant!=NULL) weed_plant_free(plant); // well, we should free the plant, but the plugins don't have this function...
    return NULL;
  }

  return plant;
}


void weed_add_plant_flags(weed_plant_t *plant, int32_t flags) {
  // TODO
}


void weed_clear_plant_flags(weed_plant_t *plant, int32_t flags) {
  char **leaves = weed_plant_list_leaves(plant);
  int i;

  for (i = 0; leaves[i] != NULL; i++) {
    weed_leaf_set_flags(plant, leaves[i], (weed_leaf_get_flags(plant, leaves[i]) | flags) ^ flags);
    free(leaves[i]);
  }
  if (leaves != NULL) free(leaves);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////


#define weed_seed_is_ptr(seed_type) (seed_type >= 64 ? 1 : 0)

#define weed_seed_get_size(seed_type, value) (weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE : \
					      (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT) ? 4 : \
					      seed_type == WEED_SEED_DOUBLE ? 8 : \
					      seed_type == WEED_SEED_INT64 ? 8 : \
					      seed_type == WEED_SEED_STRING ? strlen((const char *)value) : 0)


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) {
  while (leaf != NULL) {
    if (!strcmp((char *)leaf->key, (char *)key)) return leaf;
    leaf = leaf->next;
  }
  return NULL;
}


static weed_error_t _weed_default_get(weed_plant_t *plant, const char *key, weed_function_f *value) {
  // we pass a pointer to this function back to the plugin so that it can bootstrap its real functions

  // here we must assume that the plugin does not yet have its (static) memory functions, so we can only
  // use the standard ones

  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (value == NULL) return WEED_SUCCESS; // value can be NULL to check if leaf exists

  if (weed_seed_is_ptr(leaf->seed_type)) memcpy(value, (weed_voidptr_t)&leaf->data[0]->value, WEED_VOIDPTR_SIZE);
  else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      weed_size_t size = ((weed_data_t *)leaf->data)->size;
      char **valuecharptrptr = (char **)value;
      if (size > 0) memcpy(*valuecharptrptr, ((weed_data_t *)leaf->data)->value, size);
      memset(*valuecharptrptr + size, 0, 1);
    } else memcpy(value, ((weed_data_t *)leaf->data)->value, weed_seed_get_size(leaf->seed_type, ((weed_data_t *)leaf->data)->value));
  }
  return WEED_SUCCESS;
}


static weed_host_info_callback_f host_info_callback = NULL;
static void *host_info_callback_data = NULL;


void weed_set_host_info_callback(weed_host_info_callback_f cb, void *user_data) {
  host_info_callback = cb;
  host_info_callback_data = user_data;
}


int check_weed_abi_compat(int32_t higher, int32_t lower) {
  if (lower < 200 && higher >= 200) return WEED_FALSE; // ABI 200 made breaking changes
  return WEED_TRUE;
}


int check_filter_api_compat(int32_t higher, int32_t lower) {
  return WEED_TRUE;
}


static int check_version_compat(int host_weed_api_version,
                                int plugin_weed_api_min_version,
                                int plugin_weed_api_max_version,
                                int host_filter_api_version,
                                int plugin_filter_api_min_version,
                                int plugin_filter_api_max_version) {
  if (plugin_weed_api_min_version > host_weed_api_version || plugin_filter_api_min_version > host_filter_api_version)
    return 0;

  if (host_weed_api_version > plugin_weed_api_max_version) {
    if (check_weed_abi_compat(host_weed_api_version, plugin_weed_api_max_version) == 0) return 0;
  }

  if (host_filter_api_version > plugin_filter_api_max_version) {
    return check_filter_api_compat(host_filter_api_version, plugin_filter_api_max_version);
  }
  return 1;
}


weed_plant_t *weed_bootstrap(weed_default_getter_f *value,
                             int32_t plugin_min_weed_api_version,
                             int32_t plugin_max_weed_api_version,
                             int32_t plugin_min_filter_api_version,
                             int32_t plugin_max_filter_api_version) {
  // function is called from weed_setup() in the plugin, using the fn ptr passed by the host

  // here is where we define the functions for the plugin to use
  // the host is free to implement its own version and then pass a pointer to that function in weed_setup() for the plugin

  static weed_leaf_get_f wlg;
  static weed_plant_new_f wpn;
  static weed_plant_list_leaves_f wpll;
  static weed_leaf_num_elements_f wlne;
  static weed_leaf_element_size_f wles;
  static weed_leaf_seed_type_f wlst;
  static weed_leaf_get_flags_f wlgf;
  static weed_leaf_set_f wls;
  static weed_malloc_f weedmalloc;
  static weed_realloc_f weedrealloc;
  static weed_calloc_f weedcalloc;
  static weed_free_f weedfree;
  static weed_memcpy_f weedmemcpy;
  static weed_memset_f weedmemset;
  static weed_memmove_f weedmemmove;
  static weed_plant_free_f wpf;
  int32_t tmp;

  int32_t host_weed_api_version = WEED_API_VERSION;
  int32_t host_filter_api_version = WEED_FILTER_API_VERSION;

  weed_plant_t *host_info = weed_plant_new(WEED_PLANT_HOST_INFO);

  if (plugin_min_weed_api_version > plugin_max_weed_api_version) {
    // plugin author may be confused
    tmp = plugin_min_weed_api_version;
    plugin_min_weed_api_version = plugin_max_weed_api_version;
    plugin_max_weed_api_version = tmp;
  }
  if (plugin_min_filter_api_version > plugin_max_filter_api_version) {
    tmp = plugin_min_weed_api_version;
    plugin_min_weed_api_version = plugin_max_weed_api_version;
    plugin_max_weed_api_version = tmp;
  }

  // set pointers to the functions the plugin will use

  wpn = weed_plant_new;
  wpll = weed_plant_list_leaves;
  wlne = weed_leaf_num_elements;
  wles = weed_leaf_element_size;
  wlst = weed_leaf_seed_type;
  wlgf = weed_leaf_get_flags;
  wls = weed_leaf_set;
  wlg = weed_leaf_get;
  wpf = weed_plant_free;

  weedmalloc = malloc;
  weedrealloc = realloc;
  weedfree = free;
  weedmemcpy = memcpy;
  weedmemset = memset;

  *value = _weed_default_get; // value is a pointer to fn. ptr

  weed_set_voidptr_value(host_info, WEED_LEAF_MALLOC_FUNC, weedmalloc);
  weed_set_voidptr_value(host_info, WEED_LEAF_FREE_FUNC, weedfree);
  weed_set_voidptr_value(host_info, WEED_LEAF_MEMSET_FUNC, weedmemset);
  weed_set_voidptr_value(host_info, WEED_LEAF_MEMCPY_FUNC, weedmemcpy);
  weed_set_voidptr_value(host_info, WEED_LEAF_MEMMOVE_FUNC, weedmemmove);
  weed_set_voidptr_value(host_info, WEED_LEAF_REALLOC_FUNC, weedrealloc);
  weed_set_voidptr_value(host_info, WEED_LEAF_CALLOC_FUNC, weedcalloc);

  weed_set_voidptr_value(host_info, WEED_LEAF_GET_FUNC, wlg);

  weed_set_voidptr_value(host_info, WEED_PLANT_NEW_FUNC, wpn);
  weed_set_voidptr_value(host_info, WEED_LEAF_SET_FUNC, wls);
  weed_set_voidptr_value(host_info, WEED_LEAF_SEED_TYPE_FUNC, wlst);
  weed_set_voidptr_value(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, wlne);
  weed_set_voidptr_value(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, wles);
  weed_set_voidptr_value(host_info, WEED_PLANT_LIST_LEAVES_FUNC, wpll);
  weed_set_voidptr_value(host_info, WEED_LEAF_GET_FLAGS_FUNC, wlgf);
  weed_set_voidptr_value(host_info, WEED_PLANT_FREE_FUNC, wpf);

  weed_set_int_value(host_info, WEED_LEAF_WEED_API_VERSION, host_weed_api_version);
  weed_set_int_value(host_info, WEED_LEAF_FILTER_API_VERSION, host_filter_api_version);

  weed_set_int_value(host_info, WEED_LEAF_MIN_WEED_API_VERSION, plugin_min_weed_api_version);
  weed_set_int_value(host_info, WEED_LEAF_MAX_WEED_API_VERSION, plugin_max_weed_api_version);
  weed_set_int_value(host_info, WEED_LEAF_MIN_FILTER_API_VERSION, plugin_min_filter_api_version);
  weed_set_int_value(host_info, WEED_LEAF_MAX_FILTER_API_VERSION, plugin_max_filter_api_version);

  if (host_info_callback != NULL) {
    // if host set a callback function, we call it so it can examine and adjust the host_info plant
    // including setting the weed and filter api values if it wishes

    host_info_callback(host_info, host_info_callback_data);

    if (weed_plant_has_leaf(host_info, WEED_LEAF_WEED_API_VERSION))
      host_weed_api_version = weed_get_int_value(host_info, WEED_LEAF_WEED_API_VERSION, NULL);
    if (weed_plant_has_leaf(host_info, WEED_LEAF_FILTER_API_VERSION))
      host_filter_api_version = weed_get_int_value(host_info, WEED_LEAF_FILTER_API_VERSION, NULL);
  }

  if (!check_version_compat(host_weed_api_version, plugin_min_weed_api_version, plugin_max_weed_api_version, host_filter_api_version,
                            plugin_min_filter_api_version, plugin_max_filter_api_version)) {
    weed_plant_free(host_info);
    return NULL;
  }

  // readjust the ABI depending on the weed_api_version selected by the host

  if (host_weed_api_version < 200) {
    // added in ABI 200
    weed_leaf_delete(host_info, WEED_LEAF_REALLOC_FUNC);
    weedrealloc = NULL;
    weed_leaf_delete(host_info, WEED_LEAF_CALLOC_FUNC);
    weedrealloc = NULL;
    weed_leaf_delete(host_info, WEED_LEAF_MEMMOVE_FUNC);
    weedrealloc = NULL;
  }

  // TODO
  //weed_add_plant_flags(host_info, WEED_LEAF_READONLY_PLUGIN);

  return host_info;
}

