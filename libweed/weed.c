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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2019*/

#define _SKIP_WEED_API_

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-host.h>
#include <weed/weed.h>
#else
#include "weed-host.h"
#include "weed.h"
#endif

static int api_version = WEED_API_VERSION;

#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
#  define GNU_FLATTEN  __attribute__((flatten)) // inline all function calls
#  define GNU_CONST  __attribute__((const))
#  define GNU_HOT  __attribute__((hot))
#  define GNU_PURE  __attribute__((pure))
#  define GNU_VISIBLE  __attribute__((visibility("default")))
#else
#  define GNU_FLATTEN
#  define GNU_CONST
#  define GNU_HOT
#  define GNU_PURE
#endif

#ifndef WEED_NO_FAST_APPEND
# define WEED_FAST_APPEND
#endif

#include <string.h> // for malloc, memset, memcpy
#include <stdlib.h> // for free

// host fn pointers which get set in weed_init()
extern weed_plant_free_f weed_plant_free;
extern weed_leaf_delete_f weed_leaf_delete;
extern weed_leaf_set_flags_f weed_leaf_set_flags;

extern weed_leaf_get_f weed_leaf_get;
extern weed_leaf_set_f weed_leaf_set;
extern weed_leaf_flag_set_f weed_leaf_flag_set;
extern weed_plant_new_f weed_plant_new;
extern weed_plant_list_leaves_f weed_plant_list_leaves;
extern weed_leaf_num_elements_f weed_leaf_num_elements;
extern weed_leaf_element_size_f weed_leaf_element_size;
extern weed_leaf_seed_type_f weed_leaf_seed_type;
extern weed_leaf_get_flags_f weed_leaf_get_flags;

// exports for libweed

extern weed_error_t weed_init(int32_t api) GNU_VISIBLE;

// function defs //
static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, void *value) GNU_HOT;
static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   void *values) GNU_FLATTEN;
static weed_error_t _weed_leaf_flag_set(weed_plant_t *, const char *key, int32_t seed_type,
                                        weed_size_t num_elems, void **values, weed_size_t *sizes, int32_t flagmask);
static weed_error_t _weed_leaf_flag_get(weed_plant_t *, const char *key, int32_t *seed_type,
                                        weed_size_t *num_elems, void **values, weed_size_t **sizes, int32_t *flags);
static char **_weed_plant_list_leaves(weed_plant_t *plant) GNU_FLATTEN;
static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) GNU_FLATTEN;
static int32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) GNU_FLATTEN;
static int32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_plant_free(weed_plant_t *plant) GNU_FLATTEN;
static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) GNU_FLATTEN;

/* internal functions */
static uint64_t weed_seed_get_size(int32_t seed, void *value) GNU_FLATTEN GNU_HOT GNU_PURE;
static weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) GNU_FLATTEN GNU_HOT;
static weed_leaf_t *weed_leaf_new(const char *key, int32_t seed) GNU_FLATTEN;
static int32_t weed_seed_is_ptr(int32_t seed) GNU_CONST;
static int32_t weed_strcmp(const char *st1, const char *st2) GNU_HOT;
static uint64_t weed_strlen(const char *string) GNU_PURE;
static uint32_t weed_hash(const char *string) GNU_PURE;


static inline uint64_t weed_strlen(const char *string) {
  uint64_t len = 0;
  uint64_t maxlen = (uint64_t) - 2;
  while (*(string++) != 0 && (len != maxlen)) len++;
  return len;
}


static inline char *weed_strdup(const char *string) {
  uint64_t len;
  char *ret = malloc((len = weed_strlen(string)) + 1);
  memcpy(ret, string, len + 1);
  return ret;
}


static inline int32_t weed_strcmp(const char *st1, const char *st2) {
  while (!(*st1 == 0 && *st2 == 0)) {
    if (*(st1) == 0 || *(st2) == 0 || *(st1++) != *(st2++)) return 1;
  }
  return 0;
}


static inline uint32_t weed_hash(const char *string) {
  char c;
  uint32_t hash = 5381;
  while ((c = *(string++)) != 0) hash += (hash << 5) + c;
  return hash;
}


static inline int32_t weed_seed_is_ptr(int32_t seed) {
  return (seed != WEED_SEED_BOOLEAN && seed != WEED_SEED_INT && seed != WEED_SEED_DOUBLE && seed != WEED_SEED_STRING &&
          seed != WEED_SEED_INT64) ? 1 : 0;
}


static inline uint64_t weed_seed_get_size(int32_t seed, void *value) {
  return weed_seed_is_ptr(seed) ? (sizeof(void *)) : \
         (seed == WEED_SEED_BOOLEAN || seed == WEED_SEED_INT) ? 4 : \
         (seed == WEED_SEED_DOUBLE) ? 8 : \
         (seed == WEED_SEED_INT64) ? 8 : \
         (seed == WEED_SEED_STRING) ? weed_strlen((const char *)value) : 0;
}


static inline void weed_data_free(weed_data_t **data, int32_t num_elems, int32_t seed_type) {
  register int32_t i;
  if (data == NULL) return;
  for (i = 0; i < num_elems; i++) {
    if (!weed_seed_is_ptr(seed_type) || (seed_type == WEED_SEED_STRING && data[i]->value != NULL)) free(data[i]->value);
    else if (seed_type == WEED_SEED_VOIDPTR) {
      if (data[i]->value != NULL && data[i]->size != 0) free(data[i]->value);
    }
    free(data[i]);
  }
  free(data);
}


static inline weed_data_t **weed_data_new(int32_t seed_type, int32_t num_elems, void *values, uint64_t *sizes) {
  register int32_t i;
  weed_data_t **data = NULL;
  uint64_t size;
  char **valuec = (char **)values;
  void **valuev = (void **)values;

  if (num_elems == 0) return data;
  if ((data = (weed_data_t **)malloc(num_elems * sizeof(weed_data_t *))) == NULL) return (NULL);
  for (i = 0; i < num_elems; i++) {
    if ((data[i] = (weed_data_t *)malloc(sizeof(weed_data_t))) == NULL) {
      weed_data_free(data, --i, seed_type);
      return NULL;
    }
    if (weed_seed_is_ptr(seed_type)) {
      data[i]->value = valuev[i];
      if (seed_type == WEED_SEED_VOIDPTR && sizes != NULL) {
        data[i]->size = sizes[i];
      } else data[i]->size = 0;
    } else {
      if (seed_type == WEED_SEED_STRING) {
        if ((size = weed_strlen(valuec[i])) > 0) {
          if ((data[i]->value = malloc((size = weed_strlen(valuec[i])))) != NULL) {
            memcpy(data[i]->value, valuec[i], size);
          }
        } else data[i]->value = NULL;
        data[i]->size = size;
      } else if ((data[i]->value = malloc((size = weed_seed_get_size(seed_type, NULL)))) != NULL)
        memcpy(data[i]->value, (char *)values + i * size, size);
      if (size > 0 && data[i]->value == NULL) { // memory error
        weed_data_free(data, --i, seed_type);
        return NULL;
      } else data[i]->size = 0;
    }
  }
  return data;
}


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) {
  uint32_t hash = weed_hash(key);
  while (leaf != NULL) {
    if (hash == leaf->key_hash)
      if (!weed_strcmp((char *)leaf->key, (char *)key)) return leaf;
    leaf = leaf->next;
  }
  return NULL;
}


static inline void weed_leaf_free(weed_leaf_t *leaf) {
  weed_data_free(leaf->data, leaf->num_elements, leaf->seed_type);
  free((char *)leaf->key);
  free(leaf);
}


static inline weed_leaf_t *weed_leaf_new(const char *key, int32_t seed) {
  weed_leaf_t *leaf = (weed_leaf_t *)malloc(sizeof(weed_leaf_t));
  if (leaf == NULL) return NULL;
  if ((leaf->key = weed_strdup(key)) == NULL) {
    free(leaf);
    return NULL;
  }
  leaf->key_hash = weed_hash(key);
  leaf->seed_type = seed;
  leaf->data = NULL;
  leaf->next = NULL;
  leaf->num_elements = leaf->flags = 0;
  return leaf;
}


static inline void weed_leaf_append(weed_plant_t *leaf, weed_leaf_t *newleaf) {
#ifdef WEED_FAST_APPEND
  newleaf->next = leaf->next;
  leaf->next = newleaf;
  return;
#endif

  weed_leaf_t *leafnext;
  while (leaf != NULL) {
    if ((leafnext = leaf->next) == NULL) {
      leaf->next = newleaf;
      return;
    }
    leaf = leafnext;
  }
}


weed_error_t _weed_plant_free(weed_plant_t *leaf) {
  weed_leaf_t *leafnext;
  if (leaf->flags != 0) return WEED_ERROR_FLAGALL;
  while (leaf != NULL) {
    leafnext = leaf->next;
    weed_leaf_free(leaf);
    leaf = leafnext;
  }
  return WEED_NO_ERROR;
}


static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  // don't delete the first ("type") leaf
  if (!weed_strcmp(key, plant->key)) return WEED_ERROR_FLAGALL;
  else {
    weed_leaf_t *leaf = plant->next, *leafprev = plant;
    uint32_t hash = weed_hash(key);
    while (leaf != NULL) {
      if (leaf->key_hash == hash) {
        if (!weed_strcmp((char *)leaf->key, (char *)key)) {
          leafprev->next = leaf->next;
          weed_leaf_free(leaf);
          return WEED_NO_ERROR;
        }
      }
      leafprev = leaf;
      leaf = leaf->next;
    }
    return WEED_ERROR_NOSUCH_LEAF;
  }
}


static int32_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->flags = flags;
  return WEED_NO_ERROR;
}


static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf;
  if ((leaf = weed_leaf_new("type", WEED_SEED_INT)) == NULL) return NULL;
  if ((leaf->data = (weed_data_t **)weed_data_new(WEED_SEED_INT, 1, &plant_type, NULL)) == NULL) {
    free((char *)leaf->key);
    free(leaf);
    return NULL;
  }
  leaf->num_elements = 1;
  leaf->next = NULL;
  return leaf;
}


static char **_weed_plant_list_leaves(weed_plant_t *plant) {
  weed_leaf_t *leaf = plant;
  char **leaflist;
  register int32_t i = 1;
  for (; leaf != NULL; i++) {
    leaf = leaf->next;
  }
  if ((leaflist = (char **)malloc(i * sizeof(char *))) == NULL) return NULL;
  i = 0;
  for (leaf = plant; leaf != NULL; leaf = leaf->next) {
    if ((leaflist[i] = weed_strdup(leaf->key)) == NULL) {
      for (--i; i >= 0; i--) free(leaflist[i]);
      free(leaflist);
      return NULL;
    }
    i++;
  }
  leaflist[i] = NULL;
  return leaflist;
}


/* static weed_error_t _weed_leaf_setf(weed_plant_t *plant, const char *key, int32_t seed_type, int32_t num_elems, */
/*                                         void *values, int32_t flags) { */
/*   weed_data_t **data = NULL; */
/*   weed_leaf_t *leaf = weed_find_leaf(plant, key); */
/*   if (leaf == NULL) { */
/*     if ((leaf = weed_leaf_new(key, seed_type)) == NULL) return WEED_ERROR_MEMORY_ALLOCATION; */
/*     weed_leaf_append(plant, leaf); */
/*   } else { */
/*     if (seed_type != leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE; */
/*     weed_data_free(leaf->data, leaf->num_elements, seed_type); */
/*     leaf->data = NULL; */
/*   } */
/*   leaf->num_elements = 0; */
/*   if (num_elems > 0 && (data = weed_data_new(seed_type, num_elems, values, sizes)) == NULL) { */
/*     return WEED_ERROR_MEMORY_ALLOCATION; */
/*   } */
/*   leaf->data = data; */
/*   leaf->num_elements = num_elems; */
/*   return WEED_NO_ERROR; */
/* } */


static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems, void *values) {
  // host version
  weed_data_t **data = NULL;
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) {
    if ((leaf = weed_leaf_new(key, seed_type)) == NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_leaf_append(plant, leaf);
  } else {
    if (seed_type != leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
    weed_data_free(leaf->data, leaf->num_elements, seed_type);
    leaf->data = NULL;
  }
  leaf->num_elements = 0;
  if (num_elems > 0 && (data = weed_data_new(seed_type, num_elems, values, NULL)) == NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  leaf->data = data;
  leaf->num_elements = num_elems;
  return WEED_NO_ERROR;

}


static int32_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, void *value) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (idx >= leaf->num_elements) return WEED_ERROR_NOSUCH_ELEMENT;
  if (value == NULL) return WEED_NO_ERROR;
  if (weed_seed_is_ptr(leaf->seed_type)) memcpy(value, &leaf->data[idx]->value, sizeof(void *));
  else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      weed_size_t size = leaf->data[idx]->size;
      char **valuecharptrptr = (char **)value;
      if (size > 0) memcpy(*valuecharptrptr, leaf->data[idx]->value, size);
      memset(*valuecharptrptr + size, 0, 1);
    } else memcpy(value, leaf->data[idx]->value, weed_seed_get_size(leaf->seed_type, leaf->data[idx]->value));
  }
  return WEED_NO_ERROR;
}


static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return 0;
  return leaf->num_elements;
}


static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL || idx >= leaf->num_elements) return 0;
  return leaf->data[idx]->size;
}


static int32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return 0;
  return leaf->seed_type;
}


static int32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return 0;
  return leaf->flags;
}


weed_error_t weed_init(int32_t api) {
  // this is called by the host in order for it to set its version of the functions

  // *the plugin should never call this, instead the plugin functions are passed to the plugin
  // from the host in the "host_info" plant*

  if (api < 0 || api > WEED_API_VERSION) return WEED_ERROR_BADVERSION;

  api_version = api;

  weed_leaf_get = _weed_leaf_get;
  weed_leaf_delete = _weed_leaf_delete;
  weed_plant_free = _weed_plant_free;
  weed_plant_new = _weed_plant_new;
  weed_leaf_set = _weed_leaf_set;
  weed_plant_list_leaves = _weed_plant_list_leaves;
  weed_leaf_num_elements = _weed_leaf_num_elements;
  weed_leaf_element_size = _weed_leaf_element_size;
  weed_leaf_seed_type = _weed_leaf_seed_type;
  weed_leaf_get_flags = _weed_leaf_get_flags;
  weed_leaf_set_flags = _weed_leaf_set_flags;

  if (api >= 200) {


  }
  return WEED_NO_ERROR;
}

#ifndef WEED_NO_FAST_APPEND
# undef WEED_FAST_APPEND
#endif
