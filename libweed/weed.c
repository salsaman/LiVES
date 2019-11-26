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

// implementation of libweed with or without glib's slice allocator

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef USE_GSLICE
#include <glib.h>
#endif

#define __LIBWEED__

#ifndef NEED_LOCAL_WEED
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

static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_plant_free(weed_plant_t *) GNU_FLATTEN;
static char **_weed_plant_list_leaves(weed_plant_t *) GNU_FLATTEN;
static weed_error_t _weed_leaf_get(weed_plant_t *, const char *key, int32_t idx, weed_voidptr_t value) GNU_HOT;
static weed_error_t _weed_leaf_set(weed_plant_t *, const char *key, int32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t values) GNU_FLATTEN;
static weed_size_t _weed_leaf_num_elements(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *, const char *key, int32_t idx) GNU_FLATTEN;
static int32_t _weed_leaf_seed_type(weed_plant_t *, const char *key) GNU_FLATTEN;
static int32_t _weed_leaf_get_flags(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_leaf_set_flags(weed_plant_t *, const char *key, int32_t flags) GNU_FLATTEN;

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *, const char *key, void *data) GNU_FLATTEN;
static weed_error_t _weed_leaf_get_private_data(weed_plant_t *, const char *key, void **data_return) GNU_FLATTEN;

/* internal functions */
static weed_leaf_t *weed_find_leaf(weed_plant_t *, const char *key, uint32_t *hash_ret) GNU_FLATTEN GNU_HOT;
static weed_leaf_t *weed_leaf_new(const char *key, int32_t seed_type, uint32_t hash) GNU_FLATTEN;

static int weed_strcmp(const char *, const char *) GNU_HOT;
static weed_size_t weed_strlen(const char *) GNU_PURE;
static uint32_t weed_hash(const char *) GNU_PURE;

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

  if (abi < 200) {
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
    weed_leaf_get_private_data = _weed_leaf_get_private_data;
    weed_leaf_set_private_data = _weed_leaf_set_private_data;
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


#define weed_seed_get_size(seed_type, size) (seed_type == WEED_SEED_FUNCPTR ? WEED_FUNCPTR_SIZE : \
					      weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE : \
					      (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT) ? 4 : \
					      seed_type == WEED_SEED_DOUBLE ? 8 : \
					      seed_type == WEED_SEED_INT64 ? 8 : \
					      seed_type == WEED_SEED_STRING ? size : 0)


static inline void *weed_data_free(weed_data_t **data, weed_size_t num_elems, int32_t seed_type) {
  register int i;
  int is_nonptr = !weed_seed_is_ptr(seed_type);
  for (i = 0; i < num_elems; i++) {
    if (is_nonptr || (data[i]->value.voidptr != NULL && seed_type == WEED_SEED_STRING)) {
      weed_unmalloc_and_copy(data[i]->size, (weed_voidptr_t)data[i]->value.voidptr);
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
  weed_funcptr_t *valuef = (weed_funcptr_t *)values;
  int is_ptr;
  register int i;

  if (num_elems == 0) return (weed_data_t **)NULL;
  if ((data = (weed_data_t **)weed_malloc(num_elems * sizeof(weed_data_t *))) == NULL) return (weed_data_t **)NULL;
  is_ptr = (weed_seed_is_ptr(seed_type));
  for (i = 0; i < num_elems; i++) {
    if ((data[i] = weed_malloc_sizeof(weed_data_t)) == NULL) return weed_data_free(data, --i, seed_type);
    if (seed_type == WEED_SEED_FUNCPTR) {
      if ((data[i]->value.funcptr = (weed_funcptr_t)valuef[i]) == NULL) data[i]->size = 0;
      else data[i]->size = WEED_FUNCPTR_SIZE;
    } else {
      if (is_ptr) {
        if ((data[i]->value.voidptr = (weed_voidptr_t)valuep[i]) == NULL) data[i]->size = 0;
        else data[i]->size = WEED_VOIDPTR_SIZE;
      } else {
        if (seed_type == WEED_SEED_STRING) {
          data[i]->value.voidptr = (weed_voidptr_t)(((data[i]->size = weed_strlen(valuec[i])) > 0) ?
                                   (weed_voidptr_t)weed_malloc_and_copy(data[i]->size, valuec[i]) : NULL);
        } else {
          data[i]->size = weed_seed_get_size(seed_type, 0);
          data[i]->value.voidptr = (weed_voidptr_t)(weed_malloc_and_copy(data[i]->size, (char *)values + i * data[i]->size));
        }
        if (data[i]->size > 0 && data[i]->value.voidptr == NULL) // memory error
          return weed_data_free(data, --i, seed_type);
      }
    }
  }
  return data;
}


static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key, uint32_t *hash_ret) {
  uint32_t hash;
  if (!weed_strlen(key)) {
    if (hash_ret) *hash_ret = leaf->key_hash;
    return leaf;
  }
  hash = weed_hash(key);
  if (hash_ret) *hash_ret = hash;
  while (leaf != NULL) {
    if (hash == leaf->key_hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
      return leaf;
    }
    leaf = (weed_leaf_t *)leaf->next;
  }
  return NULL;
}


static inline void weed_leaf_free(weed_leaf_t *leaf) {
  weed_data_free((void *)leaf->data, leaf->num_elements, leaf->seed_type);
  weed_unmalloc_and_copy(weed_strlen(leaf->key) + 1, (void *)leaf->key);
  weed_unmalloc_sizeof(weed_leaf_t, leaf);
}


static inline weed_leaf_t *weed_leaf_new(const char *key, int32_t seed_type, uint32_t hash) {
  weed_leaf_t *leaf;
  if ((leaf = weed_malloc_sizeof(weed_leaf_t)) == NULL) return NULL;
  if ((leaf->key = weed_strdup(key)) == NULL) {
    weed_unmalloc_sizeof(weed_leaf_t, leaf);
    return NULL;
  }
  leaf->key_hash = hash;
  leaf->seed_type = seed_type;
  leaf->data = NULL;
  leaf->next = NULL;
  leaf->num_elements = (weed_size_t)(leaf->flags = 0);
  return leaf;
}


static inline weed_error_t weed_leaf_append(weed_plant_t *plant, weed_leaf_t *newleaf) {
  newleaf->next = plant->next;
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
  /// use gcc atomic function to update leafprev->next only if leafprev->next == leaf
  if (__sync_val_compare_and_swap((weed_leaf_t **) & (plant->next), newleaf->next, newleaf) != newleaf->next)
    return WEED_ERROR_CONCURRENCY;
#endif
  // check to try to make sure another thread hasn't already added this leaf
  plant->next = newleaf;
  return WEED_SUCCESS;
}


static weed_error_t _weed_plant_free(weed_plant_t *plant) {
  weed_leaf_t *leaf, *leafprev = plant;
  if (plant == NULL) return WEED_SUCCESS;
  if (plant->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
  while ((leaf = (weed_leaf_t *) leafprev->next) != NULL) {
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
  for (leafprev = leaf = plant;  leaf != NULL; leaf = (weed_leaf_t *)leaf->next) {
    if (leaf->key_hash == hash && !weed_strcmp((char *)leaf->key, (char *)key)) {
      if (leaf->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;
      if (leaf == plant) break; // can't ever delete the "type" leaf
      /// there is a tiny risk of a  race condition here, if another thread updates leafprev->next between us
      // reading it and writing to it. IF that happens then the other leaf will be lost.
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
      /// use an atomic function to update leafprev->next only if leafprev->next == leaf
      /// this eliminates the race condition risk.
      if (__sync_val_compare_and_swap((weed_leaf_t **) & (leafprev->next), leaf, leaf->next) != leaf)
        return WEED_ERROR_CONCURRENCY;
#endif
      if (leafprev->next == leaf)
        leafprev->next = leaf->next;
      else return WEED_ERROR_CONCURRENCY;
      weed_leaf_free(leaf);
      return WEED_SUCCESS;
    }
    leafprev = leaf;
  }
  return WEED_ERROR_NOSUCH_LEAF;
}


static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, int32_t flags) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->flags = flags;
  return WEED_SUCCESS;
}


static weed_error_t _weed_leaf_set_private_data(weed_plant_t *plant, const char *key, void *data) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->private_data = data;
  return WEED_SUCCESS;
}


#define WEED_MAGIC_HASH 0x7C9EBD07  // the magic number

static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf;
  if ((leaf = weed_leaf_new(WEED_LEAF_TYPE, WEED_SEED_INT, WEED_MAGIC_HASH)) == NULL) return NULL;
  if ((leaf->data = (volatile weed_data_t **)weed_data_new(WEED_SEED_INT, 1, &plant_type)) == NULL) {
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
  for (; leaf != NULL; i++) leaf = (weed_leaf_t *)leaf->next;
  if ((leaflist = (char **)malloc(i * sizeof(char *))) == NULL) return NULL;
  for (leaf = plant; leaf != NULL; leaf = (weed_leaf_t *)leaf->next) {
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
  uint32_t hash;
  int isnew = WEED_FALSE;
  weed_size_t old_num_elems = 0;
  weed_data_t **old_data = NULL;

  if (plant == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (IS_VALID_SEED_TYPE(seed_type) == WEED_FALSE) return WEED_ERROR_WRONG_SEED_TYPE;

  leaf = weed_find_leaf(plant, key, &hash);

  if (leaf == NULL) {
    if ((leaf = weed_leaf_new(key, seed_type, hash)) == NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    isnew = WEED_TRUE;
  } else {
    old_num_elems = leaf->num_elements;
    old_data = (weed_data_t **)leaf->data;
    if (seed_type != leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
    if (leaf->flags & WEED_FLAG_IMMUTABLE) return WEED_ERROR_IMMUTABLE;
    if ((old_num_elems = leaf->num_elements) == 0 && (old_data = (weed_data_t **)leaf->data) != NULL) {
      /// the data SHOULD be NULL if num_elements is 0. In this case either the memory is corrupted or some other thread
      /// set num_elements to zero and is about to nullify data, so we'll exit
      return WEED_ERROR_CONCURRENCY;
    }

    if (leaf == plant && num_elems != 1) return WEED_ERROR_NOSUCH_ELEMENT; ///< type leaf must always have 1 value

    /// possible race condition (a), try to minimise it by setting num_elements to zero
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
    /// if possible we use gcc buiiltin __sync_val_compare_and_swap:
    /// "if the current value of *ptr is oldval, then write newval into *ptr...and return the value of *ptr before the operation"
    /// here we read the value and swap it with 0 iff the current value is still the value we read earlier
    /// otherwise we'll get some other value back, in which case we exit with error
    if (__sync_val_compare_and_swap((weed_size_t *volatile)&leaf->num_elements, old_num_elems, 0) != old_num_elems) {
      return WEED_ERROR_CONCURRENCY;
    }
#else
    /// otherwise we'll do it the rnormal way
    if (old_num_elems != leaf->num_elements) {
      return WEED_ERROR_CONCURRENCY;
    }
    leaf->num_elements = 0;
#endif

    if (old_num_elems > 0) {
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
      /// we should be the only thread with non-zero old_num_elems, but just in case we'll nullify leaf->data whilst fetching the current
      /// value; if the current value was not what we expected that means another thread changed it, so we'll exit with error
      if (__sync_val_compare_and_swap((weed_data_t *volatile **) & (leaf->data), old_data, NULL) != old_data) {
        return WEED_ERROR_CONCURRENCY;
      }
#else
      if (leaf->data != old_data) {
        return WEED_ERROR_CONCURRENCY;
      }
      leaf->data = NULL;
#endif
    }
  }

  if (num_elems > 0 && ((data = (weed_data_t **)weed_data_new(seed_type, num_elems, values)) == NULL)) {
    // memory failure...
    if (old_data != NULL) {
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
      /// restore the old value of data iff current val is NULL, and if it was NULL, set old_num_elems
      if (__sync_val_compare_and_swap((weed_data_t *volatile **) & (leaf->data), (weed_data_t **)NULL, old_data) == NULL) {
        leaf->num_elements = old_num_elems;
      } else {
        return WEED_ERROR_CONCURRENCY;
      }
#else
      if (leaf->data == NULL) {
        leaf->data = old_data;
        leaf->num_elements = old_num_elements;
      } else {
        return WEED_ERROR_CONCURRENCY;
      }
#endif
    }
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  /// possible race condition  here: if another thread updates num_elements between reading and updating it
  if (leaf->num_elements == 0) {
#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
    /// restore the old value of data if it's NULL and if it was NULL, set old_num_elems
    if (__sync_val_compare_and_swap((weed_data_t *volatile **) & (leaf->data), (weed_data_t **)NULL, data) == NULL) {
#else
    if (leaf->data == NULL) {
      leaf->data = data;
#endif
      leaf->num_elements = num_elems;
      if (isnew == WEED_TRUE) {
        if (weed_leaf_append(plant, leaf) == WEED_ERROR_CONCURRENCY) {
          weed_leaf_free(leaf);
          return WEED_ERROR_CONCURRENCY;
        }
      }
      if (old_data != NULL) weed_data_free(old_data, old_num_elems, seed_type);
    } else {
      weed_leaf_free(leaf);
      return WEED_ERROR_CONCURRENCY;
    }
#if 0
  }
#endif
} else {
  weed_leaf_free(leaf);
  return WEED_ERROR_CONCURRENCY;
}
return WEED_SUCCESS;
}


static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx, weed_voidptr_t value) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (idx >= leaf->num_elements) return WEED_ERROR_NOSUCH_ELEMENT;
  if (value == NULL) return WEED_SUCCESS;
  if (leaf->data == NULL) return WEED_ERROR_CONCURRENCY;
  if (leaf->seed_type == WEED_SEED_FUNCPTR) {
    memcpy(value, &((weed_data_t *)(leaf->data)[idx])->value.funcptr, WEED_FUNCPTR_SIZE);
  } else if (weed_seed_is_ptr(leaf->seed_type)) {
    memcpy(value, &((weed_data_t *)(leaf->data[idx]))->value.voidptr, WEED_VOIDPTR_SIZE);
  } else {
    if (leaf->seed_type == WEED_SEED_STRING) {
      size_t size = (size_t)leaf->data[idx]->size;
      char **valuecharptrptr = (char **)value;
      if (size > 0) memcpy(*valuecharptrptr, leaf->data[idx]->value.voidptr, size);
      (*valuecharptrptr)[size] = '\0';
    } else memcpy(value, leaf->data[idx]->value.voidptr, weed_seed_get_size(leaf->seed_type, leaf->data[idx]->size));
  }
  return WEED_SUCCESS;
}


static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return 0;
  return leaf->num_elements;
}


static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL);
  if (leaf == NULL || idx > leaf->num_elements) return (weed_size_t)0;
  return leaf->data[idx]->size;
}


static int32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return (int32_t)WEED_SEED_INVALID;
  return leaf->seed_type;
}


static int32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL);
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return (int32_t)0;
  return leaf->flags;
}


static weed_error_t _weed_leaf_get_private_data(weed_plant_t *plant, const char *key, void **data_return) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL);
  if ((leaf = weed_find_leaf(plant, key, NULL)) == NULL) return WEED_ERROR_NOSUCH_LEAF;
  if (data_return) data_return = leaf->private_data;
  return WEED_SUCCESS;
}


