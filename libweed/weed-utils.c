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


weed_error_t weed_set_funcptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, 1, (weed_voidptr_t)&value);
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
  if (weed_leaf_seed_type(plant, key) != seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
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
  if ((retval = (char *)malloc(weed_leaf_element_size(plant, key, 0) + 1)) == NULL) {
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


weed_funcptr_t weed_get_funcptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_funcptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_FUNCPTR, &retval);
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

static inline weed_error_t weed_get_values(weed_plant_t *plant, const char *key, size_t dsize, char **retval, int *elems) {
  weed_error_t err;
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);
  int i;

  if (num_elems * dsize > 0) {
    if ((*retval = (char *)calloc(num_elems, dsize)) == NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }
  }

  for (i = 0; i < num_elems; i++) {
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      free(*retval);
      *retval = NULL;
      return err;
    }
  }
  if (elems) *elems = (int)num_elems;
  return WEED_SUCCESS;
}


static inline weed_voidptr_t weed_get_array(weed_plant_t *plant, const char *key,
    int32_t seed_type, weed_size_t typelen, weed_voidptr_t retvals, weed_error_t *error, int *elems) {
  weed_error_t err = weed_leaf_check(plant, key, seed_type);
  if (err != WEED_SUCCESS) {
    if (elems) *elems = 0;
    if (error != NULL) *error = err;
    return NULL;
  }
  err = weed_get_values(plant, key, typelen, (char **)&retvals, elems);
  if (error != NULL) *error = err;
  return retvals;
}


int32_t *weed_get_int_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_INT, 4, (void *)retvals, NULL, count));
}
int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_INT, 4, (void *)retvals, error, NULL));
}


double *weed_get_double_array_counted(weed_plant_t *plant, const char *key, int *count) {
  double *retvals = NULL;
  return (double *)(weed_get_array(plant, key, WEED_SEED_DOUBLE, 8, (void *)retvals, NULL, count));
}
double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double *retvals = NULL;
  return (double *)(weed_get_array(plant, key, WEED_SEED_DOUBLE, 8, (void *)retvals, error, NULL));
}

int32_t *weed_get_boolean_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_BOOLEAN, 4, (void *)retvals, NULL, count));
}
int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_BOOLEAN, 4, (void *)retvals, error, NULL));
}


int64_t *weed_get_int64_array_counted(weed_plant_t *plant, const char *key, int *count) {
  int64_t *retvals = NULL;
  return (int64_t *)(weed_get_array(plant, key, WEED_SEED_INT64, 8, (void *)retvals, NULL, count));
}
int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t *retvals = NULL;
  return (int64_t *)(weed_get_array(plant, key, WEED_SEED_INT64, 8, (void *)retvals, error, NULL));
}


char **__weed_get_string_array__(weed_plant_t *plant, const char *key, weed_error_t *error, int *count) {
  weed_size_t num_elems;
  char **retvals = NULL;

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

  for (int i = 0; i < num_elems; i++) {
    if ((retvals[i] = (char *)malloc(weed_leaf_element_size(plant, key, i) + 1)) == NULL) {
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
  }
  if (error != NULL) *error = WEED_SUCCESS;
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
  weed_funcptr_t *retvals = NULL;
  return (weed_funcptr_t *)(weed_get_array(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, (void *)retvals, NULL, count));
}
weed_funcptr_t *weed_get_funcptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_funcptr_t *retvals = NULL;
  return (weed_funcptr_t *)(weed_get_array(plant, key, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, (void *)retvals, error, NULL));
}


weed_voidptr_t *weed_get_voidptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(weed_get_array(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, (void *)retvals, NULL, count));
}
weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(weed_get_array(plant, key, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, (void *)retvals, error, NULL));
}


weed_plant_t **weed_get_plantptr_array_counted(weed_plant_t *plant, const char *key, int *count) {
  weed_plant_t **retvals = NULL;
  return (weed_plant_t **)(weed_get_array(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, (void *)retvals, NULL, count));
}
weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t **retvals = NULL;
  return (weed_plant_t **)(weed_get_array(plant, key, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, (void *)retvals, error, NULL));
}


weed_voidptr_t *weed_get_custom_array_counted(weed_plant_t *plant, const char *key, int32_t seed_type, int *count) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(weed_get_array(plant, key, seed_type, WEED_VOIDPTR_SIZE, (void *)retvals, NULL, count));
}
weed_voidptr_t *weed_get_custom_array(weed_plant_t *plant, const char *key, int32_t seed_type, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;
  return (weed_voidptr_t *)(weed_get_array(plant, key, seed_type, WEED_VOIDPTR_SIZE, (void *)retvals, error, NULL));
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

weed_error_t weed_set_funcptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_funcptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_FUNCPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, weed_plant_t **values) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, num_elems, (weed_voidptr_t)values);
}

weed_error_t weed_set_custom_array(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, seed_type, num_elems, (weed_voidptr_t)values);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t weed_get_plant_type(weed_plant_t *plant) {
  if (plant == NULL) return WEED_PLANT_UNKNOWN;
  return weed_get_int_value(plant, WEED_LEAF_TYPE, NULL);
}


weed_error_t weed_leaf_copy_nth(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf, int n) {
  // copy a leaf from src to dest
  // pointers are copied by reference only
  // strings are duplicated

  // if src or dst are NULL, nothing is copied and WEED_SUCCESS is returned

  // may return the standard errors:
  // WEED_SUCCESS, WEED_ERROR_MEMORY_ALLOCATION, WEED_ERROR_IMMUTABLE, WEED_ERROR_WRONG_SEED_TYPE
  weed_error_t err;
  int32_t seed_type;
  int num, count;
  int i;

  if (dst == NULL || src == NULL) return WEED_SUCCESS;

  if ((err = weed_check_leaf(src, keyf)) == WEED_ERROR_NOSUCH_LEAF) return WEED_ERROR_NOSUCH_LEAF;

  seed_type = weed_leaf_seed_type(src, keyf);

  if (err == WEED_ERROR_NOSUCH_ELEMENT) {
    err = weed_leaf_set(dst, keyt, seed_type, 0, NULL);
  } else {
    switch (seed_type) {
    case WEED_SEED_INT: {
      int32_t *datai = NULL;
      datai = weed_get_array(src, keyf, WEED_SEED_INT, 4, (void *)datai, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            int32_t *datai2 = NULL;
            datai2 = weed_get_array(dst, keyt, WEED_SEED_INT, 4, (void *)datai2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datai2[n] = datai[n];
                free(datai);
                datai = datai2;
                  num = count;
		// *INDENT-OFF*
   }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_int_array(dst, keyt, num, datai);
      }
      if (datai != NULL) free(datai);
    }
    break;
    case WEED_SEED_INT64: {
      int64_t *datai64 = NULL;
      datai64 = weed_get_array(src, keyf, WEED_SEED_INT64, 8, (void *)datai64, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            int64_t *datai642 = NULL;
            datai642 = weed_get_array(dst, keyt, WEED_SEED_INT64, 8, (void *)datai642, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datai642[n] = datai64[n];
                free(datai64);
                datai64 = datai642;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_int64_array(dst, keyt, num, datai64);
      }
      if (datai64 != NULL) free(datai64);
    }
    break;
    case WEED_SEED_BOOLEAN: {
      int32_t *datai = NULL;
      datai = weed_get_array(src, keyf, WEED_SEED_BOOLEAN, 4, (void *)datai, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            int32_t *datai2 = NULL;
            datai2 = weed_get_array(dst, keyt, WEED_SEED_BOOLEAN, 4, (void *)datai2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datai2[n] = datai[n];
                free(datai);
                datai = datai2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_boolean_array(dst, keyt, num, datai);
      }
      if (datai != NULL) free(datai);
    }
    break;
    case WEED_SEED_DOUBLE: {
      double *datad = NULL;
      datad = weed_get_array(src, keyf, WEED_SEED_DOUBLE, 8, (void *)datad, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            double *datad2 = NULL;
            datad2 = weed_get_array(dst, keyt, WEED_SEED_DOUBLE, 8, (void *)datad2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datad2[n] = datad[n];
                free(datad);
                datad = datad2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_double_array(dst, keyt, num, datad);
      }
      if (datad != NULL) free(datad);
    }
    break;
    case WEED_SEED_FUNCPTR: {
      weed_funcptr_t *dataf = NULL;
      dataf = weed_get_array(src, keyf, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, (void *)dataf, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            weed_funcptr_t *dataf2 = NULL;
            dataf2 = weed_get_array(dst, keyt, WEED_SEED_FUNCPTR, WEED_FUNCPTR_SIZE, (void *)dataf2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                dataf2[n] = dataf[n];
                free(dataf);
                dataf = dataf2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_funcptr_array(dst, keyt, num, dataf);
      }
      if (dataf != NULL) free(dataf);
    }
    break;
    case WEED_SEED_VOIDPTR: {
      weed_voidptr_t *datav = NULL;
      datav = weed_get_array(src, keyf, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, (void *)datav, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            weed_voidptr_t *datav2 = NULL;
            datav2 = weed_get_array(dst, keyt, WEED_SEED_VOIDPTR, WEED_VOIDPTR_SIZE, (void *)datav2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datav2[n] = datav[n];
                free(datav);
                datav = datav2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_voidptr_array(dst, keyt, num, datav);
      }
      if (datav != NULL) free(datav);
    }
    break;
    case WEED_SEED_PLANTPTR: {
      weed_plant_t **datap = NULL;
      datap = weed_get_array(src, keyf, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, (void *)datap, &err, &num);
      if (err == WEED_SUCCESS) {
        if (n >= 0) {
          if (n >= num) err = WEED_ERROR_NOSUCH_ELEMENT;
          else {
            weed_plant_t **datap2 = NULL;
            datap2 = weed_get_array(dst, keyt, WEED_SEED_PLANTPTR, WEED_VOIDPTR_SIZE, (void *)datap2, &err, &count);
            if (err == WEED_SUCCESS) {
              if (n >= count) err = WEED_ERROR_NOSUCH_ELEMENT;
              else {
                datap2[n] = datap[n];
                free(datap);
                datap = datap2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_plantptr_array(dst, keyt, num, datap);
      }
      if (datap != NULL) free(datap);
    }
    break;
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
                free(datac2[n]);
                datac2[n] = datac[n];
                for (i = 0; i < num; i++) if (i != n) free(datac[n]);
                free(datac);
                datac = datac2;
                  num = count;
		// *INDENT-OFF*
 }}}}
	// *INDENT-ON*
        if (err == WEED_SUCCESS)
          err = weed_set_string_array(dst, keyt, num, datac);
      }
      for (i = 0; i < num; i++) free(datac[i]);
      free(datac);
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
    char *m0 = NULL, *m1 = NULL;
    weed_size_t sz = weed_leaf_element_size(p0, k0, 0);
    m0 = weed_get_array(p0, k0, st, sz, (void *)m0, &err, &c0);
    if (err == WEED_SUCCESS) {
      m1 = weed_get_array(p1, k1, st, sz, (void *)m1, &err, &c1);
      if (err == WEED_SUCCESS) {
        if (elem < 0) {
          if (c0 == c1 && !memcmp(m0, m1, c0 * sz)) ret = WEED_TRUE;
        } else if (c0 > elem && c1 > elem && !memcmp(m0 + elem * sz, m1 + elem * sz, sz)) ret = WEED_TRUE;
      }
    }
    if (m1) free(m1);
    if (m0) free(m0);
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
        for (i = 0; i < c1; i++) free(s1[i]);
        free(s1);
      }
    }
    if (s0) {
      for (i = 0; i < c0; i++) free(s0[i]);
      free(s0);
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

  if (src == NULL) return NULL;

  plant = weed_plant_new(weed_get_int_value(src, WEED_LEAF_TYPE, &err));
  if (plant == NULL) return NULL;

  proplist = weed_plant_list_leaves(src, NULL);
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define weed_seed_is_ptr(seed_type) (seed_type >= 64 ? WEED_TRUE : WEED_FALSE)

#define weed_seed_get_size(seed_type, size) (weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE : \
					      (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT) ? 4 : \
					      seed_type == WEED_SEED_DOUBLE ? 8 : \
					      seed_type == WEED_SEED_INT64 ? 8 : \
					      seed_type == WEED_SEED_STRING ? size : 0)


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) {
  while (leaf != NULL) {
    if (!strcmp((char *)leaf->key, (char *)key)) return leaf;
    leaf = (weed_leaf_t *)leaf->next;
  }
  return NULL;
}


static weed_error_t _weed_default_get(weed_plant_t *plant, const char *key, void *value) {
  // we pass a pointer to this function back to the plugin so that it can bootstrap its real functions

  // here we must assume that the plugin does not yet have its (static) memory functions, so we can only
  // use the standard ones

  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (leaf->num_elements == 0) return WEED_ERROR_NOSUCH_ELEMENT;
  if (value == NULL) return WEED_SUCCESS; // value can be NULL to check if leaf exists

  if (leaf->seed_type == WEED_SEED_FUNCPTR) {
    if (leaf->data[0]->value.funcptr == NULL) {
      // because this is a special function, we return an error if the value is NULL, even though the value exists
      *((weed_funcptr_t **)value) = NULL;
      return WEED_ERROR_NOSUCH_ELEMENT;
    }
    memcpy(value, &(((weed_data_t *)(leaf->data[0]))->value.funcptr), WEED_FUNCPTR_SIZE);
    return WEED_SUCCESS;
  }
  if (weed_seed_is_ptr(leaf->seed_type)) {
    if (leaf->data[0]->value.voidptr == NULL) *((void **)value) = NULL;
    else memcpy(value, &(((weed_data_t *)(leaf->data[0]))->value.voidptr), WEED_VOIDPTR_SIZE);
    return WEED_SUCCESS;
  } else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      weed_size_t size = leaf->data[0]->size;
      char **valuecharptrptr = (char **)value;
      if (size > 0) memcpy(*valuecharptrptr, leaf->data[0]->value.voidptr, size);
      memset(*valuecharptrptr + size, 0, 1);
    } else memcpy(value, leaf->data[0]->value.voidptr,
                    weed_seed_get_size(leaf->seed_type, leaf->data[0]->size));
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
  if (lower == higher) return WEED_TRUE; // equal versions are always compatible
  if (lower > higher) {
    int32_t tmp = lower;
    lower = higher;
    higher = tmp;
  }
  if (higher > WEED_ABI_VERSION) return WEED_FALSE; // we cant possibly know about future versions
  if (lower < 200 && higher >= 200) return WEED_FALSE; // ABI 200 made breaking changes
  if (higher < 100) return WEED_FALSE;
  return WEED_TRUE;
}


int check_filter_api_compat(int32_t higher, int32_t lower) {
  if (lower == higher) return WEED_TRUE; // equal versions are always compatible
  if (lower > higher) {
    int32_t tmp = lower;
    lower = higher;
    higher = tmp;
  }
  if (higher > WEED_FILTER_API_VERSION) return WEED_FALSE; // we cant possibly know about future versions
  if (higher < 100) return WEED_FALSE;
  return WEED_TRUE;
}


static int check_version_compat(int host_weed_api_version,
                                int plugin_weed_api_min_version,
                                int plugin_weed_api_max_version,
                                int host_filter_api_version,
                                int plugin_filter_api_min_version,
                                int plugin_filter_api_max_version) {
  if (plugin_weed_api_min_version > host_weed_api_version || plugin_filter_api_min_version > host_filter_api_version)
    return WEED_FALSE;

  if (host_weed_api_version > plugin_weed_api_max_version) {
    if (check_weed_abi_compat(host_weed_api_version, plugin_weed_api_max_version) == 0) return 0;
  }

  if (host_filter_api_version > plugin_filter_api_max_version) {
    return check_filter_api_compat(host_filter_api_version, plugin_filter_api_max_version);
  }
  return WEED_TRUE;
}


weed_plant_t *weed_bootstrap(weed_default_getter_f * value,
                             int32_t plugin_min_weed_abi_version,
                             int32_t plugin_max_weed_abi_version,
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
#ifdef WEED_ABI_VERSION_200
  static weed_realloc_f weedrealloc;
  static weed_calloc_f weedcalloc;
  static weed_memmove_f weedmemmove;
#endif
  static weed_free_f weedfree;
  static weed_memcpy_f weedmemcpy;
  static weed_memset_f weedmemset;
  static weed_plant_free_f wpf;
  static weed_leaf_delete_f wld;

  int host_set_host_info = WEED_FALSE;

  /* versions here are just default values, we will set them again later, after possibly calling the host_info_callback function */
  int32_t host_weed_abi_version = WEED_ABI_VERSION;
  int32_t host_filter_api_version = WEED_FILTER_API_VERSION;

  int32_t plugin_weed_abi_version = plugin_min_weed_abi_version;
  int32_t plugin_filter_api_version = plugin_min_filter_api_version;

  weed_plant_t *host_info = NULL;
  weed_plant_t *plugin_info = NULL;

  weed_error_t err;

  *value = _weed_default_get; // value is a pointer to fn. ptr
  if (*value == NULL) return NULL;

  if (plugin_min_weed_abi_version > plugin_max_weed_abi_version) {
    // plugin author may be confused
    int32_t tmp = plugin_min_weed_abi_version;
    plugin_min_weed_abi_version = plugin_max_weed_abi_version;
    plugin_max_weed_abi_version = tmp;
  }
  if (plugin_min_filter_api_version > plugin_max_filter_api_version) {
    int32_t tmp = plugin_min_weed_abi_version;
    plugin_min_weed_abi_version = plugin_max_weed_abi_version;
    plugin_max_weed_abi_version = tmp;
  }

  // set pointers to the functions the plugin will use

  wpn = weed_plant_new;
  wpll = weed_plant_list_leaves;
  wlne = weed_leaf_num_elements;
  wles = weed_leaf_element_size;
  wlst = weed_leaf_seed_type;
  wlgf = weed_leaf_get_flags;
  wls  = weed_leaf_set;
  wlg  = weed_leaf_get;

  // added for plugins in Filter API 200
  wpf = weed_plant_free;
  wld = weed_leaf_delete;

  weedmalloc = malloc;
  weedfree = free;
  weedmemcpy = memcpy;
  weedmemset = memset;

#ifdef WEED_ABI_VERSION_200
  // added for plugins in Weed ABI 200
  weedrealloc = realloc;
  weedmemmove = memmove;
  weedcalloc = calloc;
#endif

  host_info = weed_plant_new(WEED_PLANT_HOST_INFO);
  if (host_info == NULL) return NULL;

  if (weed_set_int_value(host_info, WEED_LEAF_WEED_ABI_VERSION, host_weed_abi_version) != WEED_SUCCESS) {
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(host_info, WEED_LEAF_FILTER_API_VERSION, host_filter_api_version) != WEED_SUCCESS) {
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }

  if (weedmalloc != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)weedmalloc) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedfree != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)weedfree) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedmemset != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)weedmemset) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (weedmemcpy != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)weedmemcpy) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }

#ifdef WEED_ABI_VERSION_200
  if (plugin_max_weed_abi_version >= 200) {
    if (weedmemmove != NULL) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMMOVE_FUNC, (weed_funcptr_t)weedmemmove) != WEED_SUCCESS) {
        if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weedrealloc != NULL) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)weedrealloc) != WEED_SUCCESS) {
        if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weedcalloc != NULL) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)weedcalloc) != WEED_SUCCESS) {
        if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

#endif

  if (wpn != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_PLANT_NEW_FUNC, (weed_funcptr_t)wpn) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlg != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t)wlg) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wls != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)wls) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlst != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_SEED_TYPE_FUNC, (weed_funcptr_t)wlst) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlne != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, (weed_funcptr_t)wlne) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wles != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, (weed_funcptr_t)wles) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wpll != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_PLANT_LIST_LEAVES_FUNC, (weed_funcptr_t)wpll) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (wlgf != NULL) {
    if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FLAGS_FUNC, (weed_funcptr_t)wlgf) != WEED_SUCCESS) {
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }
  }
  if (plugin_max_filter_api_version >= 200) {
    if (wpf != NULL) {
      if (weed_set_funcptr_value(host_info, WEED_PLANT_FREE_FUNC, (weed_funcptr_t)wpf) != WEED_SUCCESS) {
        if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (wld != NULL) {
      if (weed_set_funcptr_value(host_info, WEED_LEAF_DELETE_FUNC, (weed_funcptr_t)wld) != WEED_SUCCESS) {
        if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

  plugin_info = weed_plant_new(WEED_PLANT_PLUGIN_INFO);
  if (plugin_info == NULL) {
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }

  if (weed_set_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, plugin_info) != WEED_SUCCESS) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }

  if (weed_set_int_value(plugin_info, WEED_LEAF_MIN_WEED_ABI_VERSION, plugin_min_weed_abi_version) != WEED_SUCCESS) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MAX_WEED_ABI_VERSION, plugin_max_weed_abi_version) != WEED_SUCCESS) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MIN_FILTER_API_VERSION, plugin_min_filter_api_version) != WEED_SUCCESS) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }
  if (weed_set_int_value(plugin_info, WEED_LEAF_MAX_FILTER_API_VERSION, plugin_max_filter_api_version) != WEED_SUCCESS) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }

  if (host_info_callback != NULL) {
    // if host set a callback function, we call it so it can examine and adjust the host_info plant
    // including setting memory functions and checking the weed and filter api values if it wishes
    // host can also substitute its own host_info
    // the host is also free to adjust or replace plant_info

    weed_plant_t *host_host_info = host_info_callback(host_info, host_info_callback_data);
    if (host_host_info == NULL) {
      if (plugin_info != NULL) weed_plant_free(plugin_info);
      if (host_info != NULL) weed_plant_free(host_info);
      return NULL;
    }

    if (host_host_info != host_info) {
      if (host_info != NULL) weed_plant_free(host_info);
      host_info = host_host_info;
      host_set_host_info = WEED_TRUE;
    }

    if (weed_plant_has_leaf(host_host_info, WEED_LEAF_WEED_ABI_VERSION)) {
      host_weed_abi_version = weed_get_int_value(host_host_info, WEED_LEAF_WEED_ABI_VERSION, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    if (weed_plant_has_leaf(host_host_info, WEED_LEAF_FILTER_API_VERSION)) {
      host_filter_api_version = weed_get_int_value(host_host_info, WEED_LEAF_FILTER_API_VERSION, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
  }

  if (!check_version_compat(host_weed_abi_version, plugin_min_weed_abi_version, plugin_max_weed_abi_version,
                            host_filter_api_version, plugin_min_filter_api_version, plugin_max_filter_api_version)) {
    if (plugin_info != NULL) weed_plant_free(plugin_info);
    if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
    return NULL;
  }

  plugin_weed_abi_version = host_weed_abi_version;
  plugin_filter_api_version = host_filter_api_version;

  if (host_set_host_info) {
    if (weed_plant_has_leaf(host_info, WEED_LEAF_PLUGIN_INFO)) {
      weed_plant_t *host_plugin_info = weed_get_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, &err);
      if (err != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }

      if (host_plugin_info != plugin_info) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        plugin_info = NULL;
      }
    }

    // host replaced the host_info with one of its own, check that all of the functions are present
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MALLOC_FUNC)) {
      if (weedmalloc == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)weedmalloc) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_FREE_FUNC)) {
      if (weedfree == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)weedfree) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMSET_FUNC)) {
      if (weedmemset == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)weedmemset) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMCPY_FUNC)) {
      if (weedmemcpy == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)weedmemcpy) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }

#ifdef WEED_ABI_VERSION_200
    if (plugin_weed_abi_version >= 200) {
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_MEMMOVE_FUNC)) {
        if (weedmemmove == NULL) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_MEMMOVE_FUNC, (weed_funcptr_t)weedmemmove) != WEED_SUCCESS) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
      }
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_REALLOC_FUNC)) {
        if (weedrealloc == NULL) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)weedrealloc) != WEED_SUCCESS) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
      }
      if (!weed_plant_has_leaf(host_info, WEED_LEAF_CALLOC_FUNC)) {
        if (weedcalloc == NULL) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
        if (weed_set_funcptr_value(host_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)weedcalloc) != WEED_SUCCESS) {
          if (plugin_info != NULL) weed_plant_free(plugin_info);
          return NULL;
        }
      }
    }

#endif

    if (!weed_plant_has_leaf(host_info, WEED_PLANT_NEW_FUNC)) {
      if (wpn == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_NEW_FUNC, (weed_funcptr_t)wpn) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_GET_FUNC)) {
      if (wlg == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FUNC, (weed_funcptr_t)wlg) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_SET_FUNC)) {
      if (wls == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)wls) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_SEED_TYPE_FUNC)) {
      if (wlst == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_SEED_TYPE_FUNC, (weed_funcptr_t)wlst) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC)) {
      if (wlne == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_NUM_ELEMENTS_FUNC, (weed_funcptr_t)wlne) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC)) {
      if (wles == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_ELEMENT_SIZE_FUNC, (weed_funcptr_t)wles) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_PLANT_LIST_LEAVES_FUNC)) {
      if (wpll == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_LIST_LEAVES_FUNC, (weed_funcptr_t)wpll) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_GET_FLAGS_FUNC)) {
      if (wlgf == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_GET_FLAGS_FUNC, (weed_funcptr_t)wlgf) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
  }

  if (plugin_filter_api_version >= 200) {
    if (!weed_plant_has_leaf(host_info, WEED_PLANT_FREE_FUNC)) {
      if (wpf == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_PLANT_FREE_FUNC, (weed_funcptr_t)wpf) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
    if (!weed_plant_has_leaf(host_info, WEED_LEAF_DELETE_FUNC)) {
      if (wld == NULL) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
      if (weed_set_funcptr_value(host_info, WEED_LEAF_DELETE_FUNC, (weed_funcptr_t)wld) != WEED_SUCCESS) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        return NULL;
      }
    }
  }

#ifdef WEED_ABI_VERSION_200
  // readjust the ABI depending on the weed_abi_version selected by the host

  if (plugin_weed_abi_version < 200) {
    // added in ABI 200, so remove them for lower versions
    if (weed_plant_has_leaf(host_info, WEED_LEAF_REALLOC_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_REALLOC_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedrealloc = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_CALLOC_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_CALLOC_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedcalloc = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_MEMMOVE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_MEMMOVE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    weedmemmove = NULL;
  }
#endif

  if (plugin_filter_api_version < 200) {
    if (weed_plant_has_leaf(host_info, WEED_PLANT_FREE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_PLANT_FREE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    wpf = NULL;
    if (weed_plant_has_leaf(host_info, WEED_LEAF_DELETE_FUNC)) {
      err = weed_leaf_delete(host_info, WEED_LEAF_DELETE_FUNC);
      if (err != WEED_SUCCESS && err != WEED_ERROR_UNDELETABLE) {
        if (plugin_info != NULL) weed_plant_free(plugin_info);
        if (host_set_host_info == WEED_FALSE) if (host_info != NULL) weed_plant_free(host_info);
        return NULL;
      }
    }
    wld = NULL;
  }
  return host_info;
}



