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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define __LIBWEED__
#define __WEED__HOST__

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

static weed_malloc_f _malloc_func = malloc;
static weed_calloc_f _calloc_func = calloc;
static weed_memcpy_f _memcpy_func = memcpy;
static weed_memcmp_f _memcmp_func = memcmp;
static weed_free_f _free_func = free;


void weed_utils_set_custom_memfuncs(weed_malloc_f malloc_func, weed_calloc_f calloc_func, weed_memcpy_f memcpy_func,
                                    weed_memcmp_f memcmp_func, weed_free_f free_func) {
  if (malloc_func) _malloc_func = malloc_func;
  if (calloc_func) _calloc_func = calloc_func;
  if (memcpy_func) _memcpy_func = memcpy_func;
  if (memcmp_func) _memcmp_func = memcmp_func;
  if (free_func) _free_func = free_func;
}


int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, must have a value and not just a seed_type
  if (weed_check_leaf(plant, key) == WEED_SUCCESS) return WEED_TRUE;
  return WEED_FALSE;
}


int weed_leaf_exists(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, may have only a seed_type but no value set
  if (weed_leaf_seed_type(plant, key) == WEED_SEED_INVALID) return WEED_FALSE;
  return WEED_TRUE;
}


/////////////////////////////////////////////////////////////////
// leaf setters

weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_uint_value(weed_plant_t *plant, const char *key, uint32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_UINT, 1, (weed_voidptr_t)&value);
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


weed_error_t weed_set_uint64_value(weed_plant_t *plant, const char *key, uint64_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_UINT64, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, 1, (weed_voidptr_t)&value);
}


weed_error_t weed_set_funcptr_value(weed_plant_t *plant, const char *key, weed_funcptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, 1, value ? (weed_voidptr_t)&value : NULL);
}


weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, 1, value ? (weed_voidptr_t)&value : NULL);
}


weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, 1, value ? (weed_voidptr_t)&value : NULL);
}


weed_error_t weed_set_custom_value(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, seed_type, 1, value ? (weed_voidptr_t)&value : NULL);
}


//////////////////////////////////////////////////////////////////////////////////////////////////

static inline weed_error_t weed_leaf_check(weed_plant_t *plant, const char *key, uint32_t seed_type) {
  weed_error_t err;
  if ((err = weed_check_leaf(plant, key)) != WEED_SUCCESS) return err;
  if (weed_leaf_seed_type(plant, key) != seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
  return WEED_SUCCESS;
}


static inline weed_error_t weed_value_get(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_voidptr_t retval) {
  weed_error_t err;
  if ((err = weed_leaf_check(plant, key, seed_type)) != WEED_SUCCESS) return err;
  return weed_get_value(plant, key, retval);
}


////////////////////////////////////////////////////////////

int32_t weed_get_int_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT, &retval);
  if (error) *error = err;
  return retval;
}


uint32_t weed_get_uint_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  uint32_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_UINT, &retval);
  if (error) *error = err;
  return retval;
}


double weed_get_double_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double retval = 0.;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_DOUBLE, &retval);
  if (error) *error = err;
  return retval;
}


int32_t weed_get_boolean_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = WEED_FALSE;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_BOOLEAN, &retval);
  if (error) *error = err;
  return retval;
}


int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT64, &retval);
  if (error) *error = err;
  return retval;
}


uint64_t weed_get_uint64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  uint64_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_UINT64, &retval);
  if (error) *error = err;
  return retval;
}


char *weed_get_string_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
#if WEED_ABI_VERSION < 201
  fprintf(stderr, "This version of libweed-utils requires compilation with libweed WEED_ABI_VERSION 201 or higher\n");
  abort();
#endif
  char *retval = NULL;
  weed_size_t es;
  weed_error_t err = weed_check_leaf(plant, key);
  if (err != WEED_SUCCESS) {
    if (error) *error = err;
    return NULL;
  }
  if (weed_leaf_seed_type(plant, key) != WEED_SEED_STRING) {
    if (error) *error = WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  es = weed_leaf_element_size(plant, key, 0);
  if (es > 0) {
    if ((retval = (char *)(*_malloc_func)(es)) == NULL) {
      if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
      return NULL;
    }
  }
  if ((err = weed_value_get(plant, key, WEED_SEED_STRING, &retval)) != WEED_SUCCESS) {
    if (retval) {
      (*_free_func)(retval);
      retval = NULL;
    }
  }
  if (error) *error = err;
  return retval;
}


weed_voidptr_t weed_get_voidptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_VOIDPTR, &retval);
  if (error) *error = err;
  return retval;
}


weed_funcptr_t weed_get_funcptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_funcptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_FUNCPTR, &retval);
  if (error) *error = err;
  return retval;
}


weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t *retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_PLANTPTR, &retval);
  if (error) *error = err;
  return retval;
}


weed_voidptr_t weed_get_custom_value(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, seed_type, &retval);
  if (error) *error = err;
  return retval;
}

////////////////////////////////////////////////////////////

static inline weed_error_t weed_get_values(weed_plant_t *plant, const char *key, size_t dsize, char **retval, int *elems) {
  weed_error_t err;
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);

  if (num_elems * dsize > 0) {
    if ((*retval = (char *)(*_calloc_func)(num_elems, dsize)) == NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }
  }

  for (int i = 0; i < num_elems; i++) {
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      (*_free_func)(*retval);
      *retval = NULL;
      return err;
    }
  }
  if (elems) *elems = (int)num_elems;
  return WEED_SUCCESS;
}


static inline weed_voidptr_t weed_get_arrayx(weed_plant_t *plant, const char *key,
    uint32_t seed_type, weed_size_t typelen, weed_error_t *error, int *elems) {
  char *retvals = NULL;
  weed_error_t err = weed_leaf_check(plant, key, seed_type);
  if (err != WEED_SUCCESS) {
    if (elems) *elems = 0;
    if (error) *error = err;
    return NULL;
  }
  err = weed_get_values(plant, key, typelen, &retvals, elems);
  if (error) *error = err;
  return retvals;
}


int32_t *weed_get_int_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (int32_t *)(weed_get_arrayx(plant, key, WEED_SEED_INT, 4, NULL, count));
}
int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (int32_t *)(weed_get_arrayx(plant, key, WEED_SEED_INT, 4, error, NULL));
}

uint32_t *weed_get_uint_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (uint32_t *)(weed_get_arrayx(plant, key, WEED_SEED_UINT, 4, NULL, count));
}
uint32_t *weed_get_uint_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (uint32_t *)(weed_get_arrayx(plant, key, WEED_SEED_UINT, 4, error, NULL));
}

double *weed_get_double_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (double *)(weed_get_arrayx(plant, key, WEED_SEED_DOUBLE, 8, NULL, count));
}
double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (double *)(weed_get_arrayx(plant, key, WEED_SEED_DOUBLE, 8, error, NULL));
}

int32_t *weed_get_boolean_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (int32_t *)(weed_get_arrayx(plant, key, WEED_SEED_BOOLEAN, 4, NULL, count));
}
int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (int32_t *)(weed_get_arrayx(plant, key, WEED_SEED_BOOLEAN, 4, error, NULL));
}

int64_t *weed_get_int64_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (int64_t *)(weed_get_arrayx(plant, key, WEED_SEED_INT64, 8, NULL, count));
}
int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (int64_t *)(weed_get_arrayx(plant, key, WEED_SEED_INT64, 8, error, NULL));
}

uint64_t *weed_get_uint64_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (uint64_t *)(weed_get_arrayx(plant, key, WEED_SEED_UINT64, 8, NULL, count));
}
uint64_t *weed_get_uint64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (uint64_t *)(weed_get_arrayx(plant, key, WEED_SEED_UINT64, 8, error, NULL));
}


char **__weed_get_string_array__(weed_plant_t *plant, const char *key, weed_error_t *error, int *count) {
#if WEED_ABI_VERSION < 201
  fprintf(stderr, "This version of libweed-utils requires compilation with libweed WEED_ABI_VERSION 201 or higher\n");
  abort();
#endif
  weed_size_t num_elems, es;
  char **retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_STRING);

  if (err != WEED_SUCCESS) {
    if (error) *error = err;
    return NULL;
  }

  if ((num_elems = weed_leaf_num_elements(plant, key)) == 0) return NULL;

  if ((retvals = (char **)(*_malloc_func)(num_elems * sizeof(char *))) == NULL) {
    if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (int i = 0; i < num_elems; i++) {
    es = weed_leaf_element_size(plant, key, 0);
    if (es > 0) {
      if ((retvals[i] = (char *)(*_malloc_func)(weed_leaf_element_size(plant, key, i) + 1)) == NULL) {
	for (--i; i >= 0; i--)(*_free_func)(retvals[i]);
	if (error) *error = WEED_ERROR_MEMORY_ALLOCATION;
	(*_free_func)(retvals);
	return NULL;
      }
    }
    if ((err = weed_leaf_get(plant, key, i, &retvals[i])) != WEED_SUCCESS) {
      for (--i; i >= 0; i--) if (retvals[i]) (*_free_func)(retvals[i]);
      if (error) *error = err;
      (*_free_func)(retvals);
      return NULL;
    }
  }
  if (error) *error = WEED_SUCCESS;
  if (count) *count = num_elems;
  return retvals;
}


char **weed_get_string_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return __weed_get_string_array__(plant, key, NULL, count);
}
char **weed_get_string_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return __weed_get_string_array__(plant, key, error, NULL);
}

weed_funcptr_t *weed_get_funcptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (weed_funcptr_t *)(weed_get_arrayx(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, NULL, count));
}
weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (weed_funcptr_t *)(weed_get_arrayx(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, error, NULL));
}

weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (weed_voidptr_t *)(weed_get_arrayx(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, NULL, count));
}
weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (weed_voidptr_t *)(weed_get_arrayx(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, error, NULL));
}

weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  return (weed_plant_t **)(weed_get_arrayx(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, NULL, count));
}
weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  return (weed_plant_t **)(weed_get_arrayx(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, error, NULL));
}

weed_voidptr_t *weed_get_custom_array_counted(weed_plant_t *plant, const char *key, uint32_t seed_type, int *count) {
  return (weed_voidptr_t *)(weed_get_arrayx(plant, key, seed_type, WEED_VOIDPTR_SIZE, NULL, count));
}
weed_voidptr_t *weed_get_custom_array(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_error_t *error) {
  return (weed_voidptr_t *)(weed_get_arrayx(plant, key, seed_type, WEED_VOIDPTR_SIZE, error, NULL));
}

/////////////////////////////////////////////////////

weed_error_t weed_set_int_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_uint_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, uint32_t *values) {
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


weed_error_t weed_set_uint64_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, uint64_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_UINT64, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_string_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, char **values) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_funcptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_funcptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_plant_t **values) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_custom_array(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, seed_type, num_elems, (weed_voidptr_t)values);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

weed_error_t weed_leaf_set_immutable(weed_plant_t *plant, const char *key, int state) {
  return state == WEED_TRUE ? weed_leaf_set_flags(plant, key, weed_leaf_get_flags(plant, key)
						  | WEED_FLAG_IMMUTABLE)
    : weed_leaf_set_flags(plant, key, weed_leaf_get_flags(plant, key)
			  & ~WEED_FLAG_IMMUTABLE);
}

weed_error_t weed_leaf_set_undeletable(weed_plant_t *plant, const char *key, int state) {
  return state == WEED_TRUE ? weed_leaf_set_flags(plant, key, weed_leaf_get_flags(plant, key)
						  | WEED_FLAG_UNDELETABLE)
    : weed_leaf_set_flags(plant, key, weed_leaf_get_flags(plant, key)
			  & ~WEED_FLAG_UNDELETABLE);
}

weed_error_t weed_leaf_is_immutable(weed_plant_t *plant, const char *key) {
  if (!weed_plant_has_leaf(plant, key)) return WEED_ERROR_NOSUCH_LEAF;
  if (weed_leaf_get_flags(plant, key) & WEED_FLAG_IMMUTABLE) return WEED_ERROR_IMMUTABLE;
  return WEED_SUCCESS;
}

weed_error_t weed_leaf_is_undeletable(weed_plant_t *plant, const char *key) {
  if (!weed_plant_has_leaf(plant, key)) return WEED_ERROR_NOSUCH_LEAF;
  if (weed_leaf_get_flags(plant, key) & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
  return WEED_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////


int32_t weed_get_plant_type(weed_plant_t *plant) {
  if (!plant) return WEED_PLANT_UNKNOWN;
  return weed_get_int_value(plant, WEED_LEAF_TYPE, NULL);
}

#define _COPY_DATA(ctype, wtype) {				\
    ctype *data = weed_get_arrayx(src, keyf, WEED_SEED_##wtype, sizeof(ctype), &err, &num); \
    if (err == WEED_SUCCESS) {if (n >= 0) {							\
	if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;			\
	else {ctype *data2 = weed_get_arrayx(dst, keyt, WEED_SEED_##wtype, sizeof(ctype), &err, &count); \
	  if (err == WEED_SUCCESS) {if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;		\
	    else {data2[n] = data[n]; (*_free_func)(data); data = data2; num = count; \
	    }}}} if (err == WEED_SUCCESS) err = weed_set_##wtype##_array(dst, keyt, num, data);} \
												if (data) (*_free_func)(data);}

weed_error_t weed_leaf_copy_nth(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf, int n) {
  // copy a leaf from src to dest
  // pointers are copied by reference only
  // strings are duplicated

  // if src or dst are NULL, nothing is copied and WEED_SUCCESS is returned

  // may return the standard errors:
  // WEED_SUCCESS, WEED_ERROR_MEMORY_ALLOCATION, WEED_ERROR_IMMUTABLE, WEED_ERROR_WRONG_SEED_TYPE
  weed_error_t err;
  uint32_t seed_type;
  int num, count;

  if (!dst || !src) return WEED_SUCCESS;

  if ((err = weed_check_leaf(src, keyf)) == WEED_ERROR_NOSUCH_LEAF) return WEED_ERROR_NOSUCH_LEAF;

  seed_type = weed_leaf_seed_type(src, keyf);

  if (err == WEED_ERROR_NOSUCH_ELEMENT) {
    err = weed_leaf_set(dst, keyt, seed_type, 0, NULL);
  } else {
    switch (seed_type) {
    case WEED_SEED_INT: _COPY_DATA(int32_t, int); break;
    case WEED_SEED_INT64: _COPY_DATA(int64_t, int64); break;
    case WEED_SEED_UINT: _COPY_DATA(uint32_t, uint); break;
    case WEED_SEED_UINT64: _COPY_DATA(uint64_t, uint64); break;
    case WEED_SEED_BOOLEAN: _COPY_DATA(int32_t, boolean); break;
    case WEED_SEED_DOUBLE: _COPY_DATA(double, double); break;
    case WEED_SEED_FUNCPTR: _COPY_DATA(weed_funcptr_t, funcptr); break;
    case WEED_SEED_VOIDPTR: _COPY_DATA(void *, voidptr); break;
    case WEED_SEED_PLANTPTR: _COPY_DATA(weed_plantptr_t, plantptr); break;
    case WEED_SEED_STRING: {
      char **datac = __weed_get_string_array__(src, keyf, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            char **datac2 = __weed_get_string_array__(dst, keyt, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                (*_free_func)(datac2[n]);
                datac2[n] = datac[n];
                for (int i = 0; i < num; i++) if (i != n)(*_free_func)(datac[n]);
                (*_free_func)(datac);
                datac = datac2;
                  num = count;
	      }}}}
        if (err == WEED_SUCCESS)
          err = weed_set_string_array(dst, keyt, num, datac);
      }
      for (int i = 0; i < num; i++)(*_free_func)(datac[i]);
      (*_free_func)(datac);
    }
    break;
    }
  }
  return err;
}

weed_error_t weed_leaf_copy(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf) {
  return weed_leaf_copy_nth(dst, keyt, src, keyf, -1);
}

weed_error_t weed_leaf_dup(weed_plant_t *dst, weed_plant_t *src, const char *key) {
  return weed_leaf_copy_nth(dst, key, src, key, -1);
}

weed_error_t weed_leaf_dup_nth(weed_plant_t *dst, weed_plant_t *src, const char *key, int n) {
  return weed_leaf_copy_nth(dst, key, src, key, n);
}

int weed_leaf_elements_equate(weed_plant_t *p0, const char *k0, weed_plant_t *p1, const char *k1, int elem) {
  int c0, c1;
  int32_t st;
  weed_error_t err;
  int ret = WEED_FALSE, i;
  if (!p0 || !p1) return WEED_FALSE;
  st = weed_leaf_seed_type(p0, k0);
  if (st == WEED_SEED_INVALID || st != weed_leaf_seed_type(p1, k1)) return WEED_FALSE;
  if (st != WEED_SEED_STRING) {
    weed_size_t sz = weed_leaf_element_size(p0, k0, 0);
    char *m0 = weed_get_arrayx(p0, k0, st, sz, &err, &c0);
    if (err == WEED_SUCCESS) {
      char *m1 = weed_get_arrayx(p1, k1, st, sz, &err, &c1);
      if (err == WEED_SUCCESS) {
        if (elem < 0) {
          if (c0 == c1 && !(*_memcmp_func)(m0, m1, c0 * sz)) ret = WEED_TRUE;
        } else if (c0 > elem && c1 > elem && !(*_memcmp_func)(m0 + elem * sz, m1 + elem * sz, sz)) ret = WEED_TRUE;
      }
      if (m1)(*_free_func)(m1);
    }
    if (m0)(*_free_func)(m0);
  } else {
    char **s0 = __weed_get_string_array__(p0, k0, &err, &c0);
    if (err == WEED_SUCCESS) {
      char **s1 = __weed_get_string_array__(p1, k1, &err, &c1);
      if (err == WEED_SUCCESS) {
        if (elem < 0) {
          if (c0 == c1) {
            for (i = 0; i < c0; i++) if (strcmp(s0[i], s1[i])) break;
            if (i == c0) ret = WEED_TRUE;
          }
        } else if (c0 > elem && c1 > elem && !strcmp(s0[elem], s1[elem])) ret = WEED_TRUE;
      }
      if (s1) {
        for (i = 0; i < c1; i++)(*_free_func)(s1[i]);
        (*_free_func)(s1);
      }
    }
    if (s0) {
      for (i = 0; i < c0; i++)(*_free_func)(s0[i]);
      (*_free_func)(s0);
    }
  }
  return ret;
}


weed_plant_t *weed_plant_copy(weed_plant_t *src) {
  weed_plant_t *plant;
  weed_error_t err;
  char **proplist;
  char *prop;
  int i = 0;

  if (!src) return NULL;

  plant = weed_plant_new(weed_get_int_value(src, WEED_LEAF_TYPE, &err));
  if (!plant) return NULL;

  proplist = weed_plant_list_leaves(src, NULL);
  for (prop = proplist[0]; (prop = proplist[i]) != NULL && err == WEED_SUCCESS; i++) {
    if (err == WEED_SUCCESS) {
      if (strcmp(prop, WEED_LEAF_TYPE)) {
        err = weed_leaf_copy(plant, prop, src, prop);
        if (err == WEED_ERROR_IMMUTABLE || err == WEED_ERROR_WRONG_SEED_TYPE) err = WEED_SUCCESS; // ignore these errors
	else {
	  if (weed_leaf_is_immutable(src, prop) == WEED_ERROR_IMMUTABLE)
	    weed_leaf_set_immutable(plant, prop, WEED_TRUE);
	  if (weed_leaf_is_undeletable(src, prop) == WEED_ERROR_UNDELETABLE)
	    weed_leaf_set_undeletable(plant, prop, WEED_TRUE);
	}
      }
    }(*_free_func)(prop);
  }(*_free_func)(proplist);

  if (err == WEED_ERROR_MEMORY_ALLOCATION) {
    //if (plant!=NULL) weed_plant_free(plant); // well, we should free the plant, but the plugins don't have this function...
    return NULL;
  }

  return plant;
}
