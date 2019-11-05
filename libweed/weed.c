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

// implementation of libweed with or without glib's slice allocator

//#define USE_GSLICE

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef USE_GSLICE
#include <glib.h>
#endif

#define __LIBWEED__

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#else
#include "weed.h"
#endif

#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
#  define GNU_FLATTEN  __attribute__((flatten)) // inline all function calls
#  define GNU_CONST  __attribute__((const))
#  define GNU_HOT  __attribute__((hot))
#  define GNU_PURE  __attribute__((pure))
#else
#  define GNU_FLATTEN
#  define GNU_CONST
#  define GNU_HOT
#  define GNU_PURE
#endif

// Define EXPORTED for any platform
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
#ifdef WIN_EXPORT
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllexport))
#else
#define EXPORTED __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllimport))
#else
#define EXPORTED __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#define NOT_EXPORTED
#else
#if __GNUC__ >= 4
#define EXPORTED __attribute__ ((visibility ("default")))
#define NOT_EXPORTED  __attribute__ ((visibility ("hidden")))
#else
#define EXPORTED
#define NOT_EXPORTED
#endif
#endif

EXPORTED weed_error_t weed_init(int32_t abi);

static int abi_version = WEED_ABI_VERSION;
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

static int weed_strcmp(const char *st1, const char *st2) GNU_HOT;
static weed_size_t weed_strlen(const char *string) GNU_PURE;
static uint32_t weed_hash(const char *string) GNU_PURE;

#ifdef USE_GSLICE
#define weed_malloc g_slice_alloc
#define weed_malloc_sizeof g_slice_new
#define weed_unmalloc_sizeof g_slice_free
#define weed_unmalloc_and_copy g_slice_free1

#if GLIB_CHECK_VERSION(2, 14, 0)
#define weed_malloc_and_copy(bsize, block) g_slice_copy(bsize, block)
#endif

#else
#define weed_malloc malloc
#define weed_malloc_sizeof(t) malloc(sizeof(t))
#define weed_unmalloc_sizeof(t, ptr) free(ptr)
#define weed_unmalloc_and_copy(size, ptr) free(ptr)
#endif

#ifndef weed_malloc_and_copy
#define weed_malloc_and_copy(size, src) memcpy(malloc(size), src, size)
#endif

#define weed_strdup(oldstring) (oldstring == NULL ? (char *)NULL : \
				(char *)(weed_malloc_and_copy(weed_strlen(oldstring) + 1, oldstring)))

weed_error_t weed_init(int32_t abi) {
  // this is called by the host in order for it to set its version of the functions

  // *the plugin should never call this, instead the plugin functions are passed to the plugin
  // from the host in the "host_info" plant*

  if (abi < 0 || abi > WEED_ABI_VERSION) return WEED_ERROR_BADVERSION;

  abi_version = abi;

  if (abi < 200) {
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
  return WEED_SUCCESS;
}


static inline weed_size_t weed_strlen(const char *string) {
  weed_size_t len = 0;
  weed_size_t maxlen = (weed_size_t) - 2;
  if (string == NULL) return 0;
  while (*(string++) != 0 && (len != maxlen)) len++;
  return len;
}


static inline int weed_strcmp(const char *st1, const char *st2) {
  if (st1 == NULL && st2 == NULL) return 1;
  if (st1 == NULL || st2 == NULL) return 0;
  while (!(*st1 == 0 && *st2 == 0)) {
    if (*(st1) == 0 || *(st2) == 0 || *(st1++) != *(st2++)) return 1;
  }
  return 0;
}


static inline uint32_t weed_hash(const char *string) {
  char c;
  uint32_t hash = 5381;
  if (string == NULL) return 0;
  while ((c = *(string++)) != 0) hash += (hash << 5) + c;
  return hash;
}

#define weed_seed_is_ptr(seed_type) (seed_type >= 64 ? 1 : 0)

#define weed_seed_get_size(seed_type, value) (weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE : \
					      (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT) ? 4 : \
					      seed_type == WEED_SEED_DOUBLE ? 8 : \
					      seed_type == WEED_SEED_INT64 ? 8 : \
					      seed_type == WEED_SEED_STRING ? weed_strlen((const char *)value) : 0)

static inline void *weed_data_free(weed_data_t **data, weed_size_t num_elems, int32_t seed_type, int32_t flags) {
  register int i;
  int is_nonptr = !weed_seed_is_ptr(seed_type);
  int is_mutable = !(flags & WEED_FLAG_IMMUTABLE);
  for (i = 0; i < num_elems; i++) {
    if (is_nonptr ||
        (data[i]->value != NULL &&
         (seed_type == WEED_SEED_STRING || (seed_type == WEED_SEED_VOIDPTR && data[i]->size != 0 && is_mutable)))) {
      if (seed_type == WEED_SEED_VOIDPTR) {
        free((weed_voidptr_t)data[i]->value);
      } else weed_unmalloc_and_copy(data[i]->size, (weed_voidptr_t)data[i]->value);
    }
    weed_unmalloc_sizeof(weed_data_t, data[i]);
  }
  weed_unmalloc_and_copy(num_elems * sizeof(weed_data_t *), data);
  return NULL;
}


static inline weed_data_t **weed_data_new(int32_t seed_type, weed_size_t num_elems, weed_voidptr_t values) {
  weed_data_t **data = NULL;
  char **valuec = (char **)values;
  weed_voidptr_t *valuep = (weed_voidptr_t *)values;
  int is_ptr;
  register int i;

  if (num_elems == 0) return (weed_data_t **)NULL;
  if ((data = (weed_data_t **)weed_malloc(num_elems * sizeof(weed_data_t *))) == NULL) return (weed_data_t **)NULL;
  is_ptr = (weed_seed_is_ptr(seed_type));
  for (i = 0; i < num_elems; i++) {
    if ((data[i] = weed_malloc_sizeof(weed_data_t)) == NULL) return weed_data_free(data, --i, seed_type, 0);
    if (is_ptr) {
      data[i]->value = (weed_voidptr_t)valuep[i];
      data[i]->size = 0;
    } else {
      if (seed_type == WEED_SEED_STRING) {
        data[i]->value = (weed_voidptr_t)(((data[i]->size = weed_strlen(valuec[i])) > 0) ?
                                          (weed_voidptr_t)weed_malloc_and_copy(data[i]->size, valuec[i]) : NULL);
      } else {
        data[i]->size = weed_seed_get_size(seed_type, NULL);
        data[i]->value = (weed_voidptr_t)(weed_malloc_and_copy(data[i]->size, (char *)values + i * data[i]->size));
      }
      if (data[i]->size > 0 && data[i]->value == NULL) // memory error
        return weed_data_free(data, --i, seed_type, 0);
    }
  }
  return data;
}


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) {
  uint32_t hash;
  if (!weed_strlen(key)) return leaf;
  hash = weed_hash(key);
  while (leaf != NULL) {
    if (hash == leaf->key_hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
      return leaf;
    }
    leaf = leaf->next;
  }
  return NULL;
}


static inline void weed_leaf_free(weed_leaf_t *leaf) {
  weed_data_free(leaf->data, leaf->num_elements, leaf->seed_type, leaf->flags);
  weed_unmalloc_and_copy(weed_strlen(leaf->key) + 1, (void *)leaf->key);
  weed_unmalloc_sizeof(weed_leaf_t, leaf);
}


static inline weed_leaf_t *weed_leaf_new(const char *key, int32_t seed_type) {
  weed_leaf_t *leaf;
  if ((leaf = weed_malloc_sizeof(weed_leaf_t)) == NULL) return NULL;
  if ((leaf->key = weed_strdup(key)) == NULL) {
    weed_unmalloc_sizeof(weed_leaf_t, leaf);
    return NULL;
  }
  leaf->key_hash = weed_hash(key);
  leaf->seed_type = seed_type;
  leaf->data = NULL;
  leaf->next = NULL;
  leaf->num_elements = (weed_size_t)(leaf->flags = 0);
  return leaf;
}


static inline void weed_leaf_append(weed_plant_t *leaf, weed_leaf_t *newleaf) {
  newleaf->next = leaf->next;
  leaf->next = newleaf;
}


static weed_error_t _weed_plant_free(weed_plant_t *plant) {
  weed_leaf_t *leaf, *leafprev = plant;
  if (plant == NULL) return WEED_SUCCESS;
  if (plant->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
  while ((leaf = leafprev->next) != NULL) {
    if (leaf->flags & WEED_FLAG_UNDELETABLE) leafprev = leaf;
    else {
      leafprev->next = leaf->next;
      weed_leaf_free(leaf);
    }
  }
  if (plant->next == NULL) {
    weed_leaf_free(plant);
    return WEED_SUCCESS;
  }
  return WEED_ERROR_UNDELETABLE;
}


static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf, *leafprev;
  uint32_t hash = weed_hash(key);
  for (leafprev = leaf = plant;  leaf != NULL; leaf = leaf->next) {
    if (leaf->key_hash == hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
      if (leaf->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
      if (leaf == plant) break; // can't ever delete the "type" leaf
      leafprev->next = leaf->next;
      weed_leaf_free(leaf);
      return WEED_SUCCESS;
    }
    leafprev = leaf;
  }
  return WEED_ERROR_NOSUCH_LEAF;
}


static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->flags = flags;
  return WEED_SUCCESS;
}


static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf;
  if ((leaf = weed_leaf_new(WEED_LEAF_TYPE, WEED_SEED_INT)) == NULL) return NULL;
  if ((leaf->data = (weed_data_t **)weed_data_new(WEED_SEED_INT, 1, &plant_type)) == NULL) {
    weed_unmalloc_and_copy(weed_strlen(leaf->key) + 1, (void *)leaf->key);
    weed_unmalloc_sizeof(weed_leaf_t, leaf);
    return NULL;
  }
  leaf->num_elements = 1;
  leaf->flags = WEED_FLAG_IMMUTABLE;
  return leaf;
}


static char **_weed_plant_list_leaves(weed_plant_t *plant) {
  // must use normal malloc, strdup here since the caller will free the strings
  weed_leaf_t *leaf = plant;
  char **leaflist;
  register int i = 1, j = 0;
  for (; leaf != NULL; i++) leaf = leaf->next;
  if ((leaflist = (char **)malloc(i * sizeof(char *))) == NULL) return NULL;
  for (leaf = plant; leaf != NULL; leaf = leaf->next) {
    if ((leaflist[j++] = strdup(leaf->key)) == NULL) {
      for (--j; j > 0; free(leaflist[--j]));
      free(leaflist);
      return NULL;
    }
  }
  leaflist[j] = NULL;
  return leaflist;
}


#define IS_VALID_SEED_TYPE(seed_type) ((seed_type >=64 || (seed_type >= 1 && seed_type <= 5) ? WEED_TRUE : WEED_FALSE))

static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t values) {
  weed_data_t **data = NULL;
  weed_leaf_t *leaf;

  if (plant == NULL) return WEED_ERROR_NOSUCH_LEAF;

  if (IS_VALID_SEED_TYPE(seed_type) == WEED_FALSE) return WEED_ERROR_WRONG_SEED_TYPE;

  leaf = weed_find_leaf(plant, key);

  if (leaf == NULL) {
    if ((leaf = weed_leaf_new(key, seed_type)) == NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_leaf_append(plant, leaf);
  } else {
    if (seed_type != leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
    if (leaf->flags & WEED_FLAG_IMMUTABLE) return WEED_ERROR_IMMUTABLE;
    if (leaf == plant && num_elems != 1) return WEED_ERROR_NOSUCH_ELEMENT;
    leaf->data = (weed_data_t **)weed_data_free(leaf->data, leaf->num_elements, seed_type, leaf->flags);
  }
  leaf->num_elements = 0;
  if (num_elems > 0 && ((data = (weed_data_t **)weed_data_new(seed_type, num_elems, values)) == NULL)) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  leaf->data = data;
  leaf->num_elements = num_elems;
  return WEED_SUCCESS;
}


static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, weed_voidptr_t value) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (idx >= leaf->num_elements) return WEED_ERROR_NOSUCH_ELEMENT;
  if (value == NULL) return WEED_SUCCESS;
  if (weed_seed_is_ptr(leaf->seed_type)) memcpy((weed_voidptr_t)value, &leaf->data[idx]->value, WEED_VOIDPTR_SIZE);
  else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      size_t size = (size_t)leaf->data[idx]->size;
      char **valuecharptrptr = (char **)value;
      if (size > 0) memcpy(*valuecharptrptr, leaf->data[idx]->value, size);
      (*valuecharptrptr)[size] = '\0';
    } else memcpy(value, leaf->data[idx]->value, weed_seed_get_size(leaf->seed_type, leaf->data[idx]->value));
  }
  return WEED_SUCCESS;
}


static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key)) == NULL) return 0;
  return leaf->num_elements;
}


static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if (leaf == NULL || idx > leaf->num_elements) return (weed_size_t)0;
  if (abi_version >= 200 || !weed_seed_is_ptr(leaf->seed_type)) {
    return leaf->data[idx]->size;
  }
  return (weed_size_t)(WEED_VOIDPTR_SIZE); // only for backwards compat.
}


static int32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key)) == NULL) return (int32_t)WEED_SEED_INVALID;
  return leaf->seed_type;
}


static int32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key);
  if ((leaf = weed_find_leaf(plant, key)) == NULL) return (int32_t)0;
  return leaf->flags;
}

