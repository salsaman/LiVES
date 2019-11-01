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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2019 */

// implementation of libweed using glib's slice allocator

#include <string.h>
#include <stdlib.h>

#define __LIBWEED__

#ifdef HAVE_SYSTEM_WEEDa
#include <weed/weed.h>
#else
#include "weed.h"
#endif

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
#  define GNU_VISIBLE
#endif

extern weed_error_t weed_init(int32_t api) GNU_VISIBLE;

static int api_version = WEED_API_VERSION;
static weed_error_t nse_error = WEED_ERROR_NOSUCH_ELEMENT;

static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_plant_free(weed_plant_t *plant) GNU_FLATTEN;
static char **_weed_plant_list_leaves(weed_plant_t *plant) GNU_FLATTEN;
static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, weed_voidptr_t value) GNU_HOT;
static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t values) GNU_FLATTEN;
static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) GNU_FLATTEN;
static int32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) GNU_FLATTEN;
static int32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) GNU_FLATTEN;

/* internal functions */
static weed_leaf_t *weed_find_leaf(weed_plant_t *plant, const char *key) GNU_FLATTEN GNU_HOT;
static weed_leaf_t *weed_leaf_new(const char *key, int32_t seed_type) GNU_FLATTEN;
static weed_size_t weed_seed_get_size(int32_t seed_type, void *value) GNU_FLATTEN GNU_HOT GNU_PURE;

static int weed_strcmp(const char *st1, const char *st2) GNU_HOT;
static size_t weed_strlen(const char *string) GNU_PURE;
static uint32_t weed_hash(const char *string) GNU_PURE;


static inline size_t weed_strlen(const char *string) {
  size_t len = 0;
  size_t maxlen = (size_t) - 2;
  while (*(string++) != 0 && (len != maxlen)) len++;
  return len;
}


static inline char *weed_strdup(const char *string) {
  size_t len;
  char *ret = (char *)malloc((len = weed_strlen(string)) + 1);
  memcpy(ret, string, len + 1);
  return ret;
}


static inline int weed_strcmp(const char *st1, const char *st2) {
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


#define weed_seed_is_ptr(seed_type) (seed_type >= 64 ? 1 : 0)


static inline weed_size_t weed_seed_get_size(int32_t seed_type, void *value) {
  return (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT) ? 4 :
         (seed_type == WEED_SEED_DOUBLE) ? 8 :
         (seed_type == WEED_SEED_INT64) ? 8 :
         (seed_type == WEED_SEED_STRING) ? weed_strlen((const char *)value) :
         weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE :
         0;
}


static inline void *weed_data_free(weed_data_t **data, weed_size_t num_elems, int32_t seed_type, int32_t flags) {
  register int i;
  for (i = 0; i < num_elems; i++) {
    if (!weed_seed_is_ptr(seed_type) ||
        (data[i]->value != NULL &&
         (seed_type == WEED_SEED_STRING || (seed_type == WEED_SEED_VOIDPTR && data[i]->size != 0 && !(flags & WEED_FLAG_IMMUTABLE)))))
      free((weed_voidptr_t)data[i]->value);
    free(data[i]);
  }
  free(data);
  return NULL;
}


static inline weed_data_t **weed_data_new(int32_t seed_type, weed_size_t num_elems, void *values) {
  register int i;
  weed_data_t **data = NULL;
  char **valuec = (char **)values;
  weed_voidptr_t *valuev = (weed_voidptr_t *)values;

  if (num_elems == 0) return data;
  if ((data = (weed_data_t **)malloc(num_elems * sizeof(weed_data_t *))) == NULL) return NULL;
  for (i = 0; i < num_elems; i++) {
    if ((data[i] = (weed_data_t *)malloc(sizeof(weed_data_t))) == NULL) {
      return weed_data_free(data, --i, seed_type, 0);
    }
    if (weed_seed_is_ptr(seed_type)) {
      data[i]->value = (weed_voidptr_t)valuev[i];
      data[i]->size = 0;
    } else {
      if (seed_type == WEED_SEED_STRING) {
        if ((data[i]->size = weed_strlen(valuec[i])) > 0) {
          if ((data[i]->value = (weed_voidptr_t)malloc((data[i]->size = weed_strlen(valuec[i])))) != NULL) {
            memcpy(data[i]->value, (weed_voidptr_t)valuec[i], data[i]->size);
          }
        } else data[i]->value = NULL;
      } else {
        data[i]->size = weed_seed_get_size(seed_type, NULL);
        data[i]->value = (weed_voidptr_t)malloc(data[i]->size);
        if (data[i]->value != NULL)
          memcpy(data[i]->value, (weed_voidptr_t)((char *)values + i * data[i]->size), data[i]->size);
      }
      if (data[i]->size > 0 && data[i]->value == NULL) { // memory error
        return weed_data_free(data, --i, seed_type, 0);
      }
    }
  }
  return data;
}


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *plant, const char *key) {
  weed_plant_t *leaf, *prev = NULL;
  uint32_t hash = weed_hash(key);
  for (leaf = plant; leaf != NULL; leaf = leaf->next) {
    if (hash == leaf->key_hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
#ifndef NO_OPTIMISE_ORDER
      if (leaf != plant && plant->next != leaf) {
        // optimise by moving leaf to front
        prev->next = leaf->next;
        leaf->next = plant->next;
        plant->next = leaf;
      }
#endif
      return leaf;
    }
    prev = leaf;
  }
  return NULL;
}


static inline void weed_leaf_free(weed_leaf_t *leaf) {
  weed_data_free(leaf->data, leaf->num_elements, leaf->seed_type, leaf->flags);
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
  newleaf->next = leaf->next;
  leaf->next = newleaf;
  return;
}


static weed_error_t _weed_plant_free(weed_plant_t *leaf) {
  weed_leaf_t *leafnext;
  if (leaf->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
  for (; leaf != NULL; leaf = leafnext) {
    leafnext = leaf->next;
    weed_leaf_free(leaf);
  }
  return WEED_NO_ERROR;
}


static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = plant, *leafprev = NULL;
  uint32_t hash = weed_hash(key);
  for (; leaf != NULL; leaf = leaf->next) {
    if (leaf->key_hash == hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
      if (leaf->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
      if (leafprev == NULL) return WEED_ERROR_NOSUCH_LEAF; // can't delete the "type" leaf
      leafprev->next = leaf->next;
      weed_leaf_free(leaf);
      return WEED_NO_ERROR;
    }
    leafprev = leaf;
  }
  return WEED_ERROR_NOSUCH_LEAF;
}


static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->flags = flags;
  return WEED_NO_ERROR;
}


static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf;
  if ((leaf = weed_leaf_new(WEED_LEAF_TYPE, WEED_SEED_INT)) == NULL) return NULL;
  if ((leaf->data = (weed_data_t **)weed_data_new(WEED_SEED_INT, 1, &plant_type)) == NULL) {
    free((char *)leaf->key);
    free(leaf);
    return NULL;
  }
  leaf->num_elements = 1;
  leaf->next = NULL;
  leaf->flags = WEED_FLAG_IMMUTABLE;
  return leaf;
}


static char **_weed_plant_list_leaves(weed_plant_t *plant) {
  weed_leaf_t *leaf = plant;
  char **leaflist;
  register int i = 1;
  for (; leaf != NULL; i++) {
    leaf = leaf->next;
  }
  if ((leaflist = (char **)malloc(i * sizeof(char *))) == NULL) return NULL;
  i = 0;
  for (leaf = plant; leaf != NULL; leaf = leaf->next) {
    if ((leaflist[i++] = weed_strdup(leaf->key)) == NULL) {
      for (--i; i > 0; free(leaflist[--i]));
      free(leaflist);
      return NULL;
    }
  }
  leaflist[i] = NULL;
  return leaflist;
}


static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t values) {
  weed_data_t **data = NULL;
  weed_leaf_t *leaf = weed_find_leaf(plant, key);

  if (leaf == NULL) {
    if ((leaf = weed_leaf_new(key, seed_type)) == NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_leaf_append(plant, leaf);
  } else {
    if (seed_type != leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
    if (leaf->flags & WEED_FLAG_IMMUTABLE) return WEED_ERROR_IMMUTABLE;
    leaf->data = (weed_data_t **)weed_data_free(leaf->data, leaf->num_elements, seed_type, leaf->flags);
  }
  leaf->num_elements = 0;
  if (num_elems > 0 && (data = weed_data_new(seed_type, num_elems, values)) == NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  leaf->data = data;
  leaf->num_elements = num_elems;
  return WEED_NO_ERROR;
}


static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, weed_voidptr_t value) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (idx >= leaf->num_elements) return nse_error;
  if (value == NULL) return WEED_NO_ERROR;
  if (weed_seed_is_ptr(leaf->seed_type)) memcpy((weed_voidptr_t)value, (weed_voidptr_t)&leaf->data[idx]->value, WEED_VOIDPTR_SIZE);
  else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      size_t size = leaf->data[idx]->size;
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
  if (leaf == NULL || idx > leaf->num_elements) return 0;
  if (api_version >= 200 || !weed_seed_is_ptr(leaf->seed_type))
    return leaf->data[idx]->size;
  else
    return (sizeof(void *)); // only for backwards compat.
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

  if (api < 200) {
    nse_error = WEED_ERROR_NOSUCH_LEAF;
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
  } else {
    nse_error = WEED_ERROR_NOSUCH_ELEMENT;
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
  }
  return WEED_NO_ERROR;
}

