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

// implementation of libweed with or without glib's slice allocator, with or without thread safety (needs pthread)

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

//#define WEED_MAGIC_HASH 0x7C9EBD07  // the magic number
#define WEED_MAGIC_HASH 0xB82E802F
#define WEED_FLAG_OP_DELETE WEED_FLAG_RESERVED_0

#if defined __GNUC__ && !defined WEED_IGN_GNUC_OPT
#define GNU_FLATTEN  __attribute__((flatten)) // inline all function calls
#define GNU_CONST  __attribute__((const))
#define GNU_HOT  __attribute__((hot))
#define GNU_PURE  __attribute__((pure))
#else
#define GNU_FLATTEN
#define GNU_CONST
#define GNU_HOT
#define GNU_PURE
#endif

// Define EXPORTED for any platform
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
#ifdef WIN_EXPORT
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllexport))
#else
#define EXPORTED __declspec(dllexport) // Note: actually gcc seems to also support this syntax.
#endif
#else
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllimport))
#else
#define EXPORTED __declspec(dllimport) // Note: actually gcc seems to also support this syntax.
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

#include <pthread.h>

typedef struct {
  pthread_rwlock_t	chain_lock;
  pthread_rwlock_t	data_lock;
  pthread_mutex_t	data_mutex;
} leaf_priv_data_t;

typedef struct {
  leaf_priv_data_t	ldata;
  pthread_rwlock_t	reader_count;
  pthread_mutex_t	structure_mutex;
} plant_priv_data_t;

#define is_plant(leaf) (leaf->key_hash == WEED_MAGIC_HASH)

#define get_data_lock(leaf) (is_plant(leaf) ?				\
			     &(((plant_priv_data_t *)((leaf)->private_data))->ldata.data_lock) : \
			     &(((leaf_priv_data_t *)((leaf)->private_data))->data_lock))

#define get_chain_lock(leaf) (is_plant(leaf) ?				\
			      &(((plant_priv_data_t *)((leaf)->private_data))->ldata.chain_lock) : \
			      &(((leaf_priv_data_t *)((leaf)->private_data))->chain_lock))

#define get_data_mutex(leaf) (is_plant(leaf) ?				\
			      &(((plant_priv_data_t *)((leaf)->private_data))->ldata.data_mutex) : \
			      &(((leaf_priv_data_t *)((leaf)->private_data))->data_mutex))

#define get_structure_mutex(plant) (&(((plant_priv_data_t *)((plant)->private_data))->structure_mutex))


#define get_count_lock(plant) (&(((plant_priv_data_t *)((plant)->private_data))->reader_count))

#define data_lock_unlock(obj) do {					\
    if ((obj)) pthread_rwlock_unlock(get_data_lock((obj)));} while (0)

#define data_lock_writelock(obj) do {					\
    if ((obj)) pthread_rwlock_wrlock(get_data_lock((obj)));} while (0)

#define data_lock_readlock(obj) do {					\
    if ((obj)) pthread_rwlock_rdlock(get_data_lock((obj)));} while (0)

#define chain_lock_unlock(obj) do {					\
    if ((obj)) pthread_rwlock_unlock(get_chain_lock((obj)));} while (0)

#define chain_lock_writelock(obj) do {					\
    if ((obj)) pthread_rwlock_wrlock(get_chain_lock((obj)));} while (0)

#define chain_lock_readlock(obj) do {					\
    if ((obj)) pthread_rwlock_rdlock(get_chain_lock((obj)));} while (0)

#define chain_lock_try_writelock(obj) (pthread_rwlock_trywrlock(get_chain_lock((obj))))

#define chain_lock_try_readlock(obj) (pthread_rwlock_tryrdlock(get_chain_lock((obj))))

#define structure_mutex_trylock(obj) (pthread_mutex_trylock(get_structure_mutex((obj))))

#define structure_mutex_lock(obj) do {					\
    if ((obj)) pthread_mutex_lock(get_structure_mutex((obj)));} while (0)

#define structure_mutex_unlock(obj) do {				\
    if ((obj)) pthread_mutex_unlock(get_structure_mutex((obj)));} while (0)

#define reader_count_add(obj) do {					\
    if ((obj)) pthread_rwlock_rdlock(get_count_lock((obj)));} while (0)

#define reader_count_sub(obj) do {					\
    if ((obj)) pthread_rwlock_unlock(get_count_lock((obj)));} while (0)

#define reader_count_wait(obj) do {				\
    if ((obj)) pthread_rwlock_wrlock(get_count_lock((obj)));	\
    pthread_rwlock_unlock(get_count_lock((obj)));} while (0)

#define return_unlock(obj, val) do {					\
    typeof(val) myval = (val); data_lock_unlock((obj)); return myval;} while (0)

static int data_lock_upgrade(weed_leaf_t *leaf, int block) {
  if (leaf) {
    // try to grab the mutex
    int ret = pthread_mutex_trylock(get_data_mutex(leaf));
    if (ret) {
      // if we fail and !block, return with data readlock held
      if (!block) return ret;
      else {
	// otherwise, drop the data readlock in case a writer is blocked
	// then block until we do get data mutex
	data_lock_unlock(leaf);
	pthread_mutex_lock(get_data_mutex(leaf));
      }
    }
    else data_lock_unlock(leaf);
    // now we can wait for the writelock and then unlock mutex
    // subsequent writers will drop their readlocks and block on the mutex
    data_lock_writelock(leaf);
    pthread_mutex_unlock(get_data_mutex(leaf));
  }
  return 0;
}

static int chain_lock_upgrade(weed_leaf_t *leaf, int have_rdlock, int is_del) {
  // grab the mutex, release the readlock held, grab a write lock, release the mutex,
  // release the writelock
  // if blocking is 0, then we return if we cannot get the mutex
  // return 0 if we got the write lock, otherwise
  if (leaf) {
    if (!have_rdlock) {
      // lock the leaf (plant)
      structure_mutex_lock(leaf);
    }
    else chain_lock_unlock(leaf);
    // now we have the mutex lock, we grab the writelock
    // with the mutex held, other threads will be blocked
    // and released their readlocks, allowing us to proceed

    chain_lock_writelock(leaf);

    // if it is a SET, then we release the mutex; the next writer
    // will block on writelock; subsequent writeers will block on the mutex
    if (is_del) leaf->flags |= WEED_FLAG_OP_DELETE;
    else if (!have_rdlock) structure_mutex_unlock(leaf);

  }
  return 0;
}

static int allbugfixes = 0;
static int debugmode = 0;

static int32_t _abi_ = WEED_ABI_VERSION;

EXPORTED int32_t weed_get_abi_version(void);
EXPORTED weed_error_t weed_init(int32_t abi, uint64_t init_flags);

static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_plant_free(weed_plant_t *) GNU_FLATTEN;
static char **_weed_plant_list_leaves(weed_plant_t *, weed_size_t *nleaves) GNU_FLATTEN;
static weed_error_t _weed_leaf_get(weed_plant_t *, const char *key, int32_t idx,
				   weed_voidptr_t value) GNU_HOT;
static weed_error_t _weed_leaf_set(weed_plant_t *, const char *key, uint32_t seed_type,
				   weed_size_t num_elems, weed_voidptr_t values) GNU_FLATTEN;
static weed_size_t _weed_leaf_num_elements(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *, const char *key, int32_t idx) GNU_FLATTEN;
static uint32_t _weed_leaf_seed_type(weed_plant_t *, const char *key) GNU_FLATTEN;
static uint32_t _weed_leaf_get_flags(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_leaf_set_flags(weed_plant_t *, const char *key, uint32_t flags) GNU_FLATTEN;

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *, const char *key, void *data)
  GNU_FLATTEN;
static weed_error_t _weed_leaf_get_private_data(weed_plant_t *, const char *key, void **data_return)
  GNU_FLATTEN;

/* internal functions */
static weed_leaf_t *weed_find_leaf(weed_plant_t *, const char *key, uint32_t *hash_ret)
  GNU_FLATTEN GNU_HOT;
static weed_leaf_t *weed_leaf_new(const char *key, uint32_t seed_type, uint32_t hash) GNU_FLATTEN;

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

#define weed_strdup(oldstring, size) (!oldstring ? (char *)NULL :	\
				      size < _WEED_PADBYTES_ ? memcpy(leaf->padding, key, size + 1) : \
				      (char *)(weed_malloc_and_copy(weed_strlen(oldstring) + 1, \
								    oldstring)))

#define IS_VALID_SEED_TYPE(seed_type) ((seed_type >=64 || (seed_type >= 1 && seed_type <= 5) ? 1 : 0))

EXPORTED int32_t weed_get_abi_version(void) {return _abi_;}

EXPORTED weed_error_t weed_init(int32_t abi, uint64_t init_flags) {
  // this is called by the host in order for it to set its version of the functions

  // *the plugin should never call this, instead the plugin functions are passed to the plugin
  // from the host in the "host_info" plant*

  if (abi < 0 || abi > WEED_ABI_VERSION) return WEED_ERROR_BADVERSION;
  _abi_ = abi;

  if (init_flags & WEED_INIT_ALLBUGFIXES) allbugfixes = 1;
  if (init_flags & WEED_INIT_DEBUGMODE) debugmode = 1;

  if (debugmode) {
    fprintf(stderr, "Weed padding size is %d\n", _WEED_PADBYTES_);
  }

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
  if (_abi_ >= 200) {
    weed_leaf_get_private_data = _weed_leaf_get_private_data;
    weed_leaf_set_private_data = _weed_leaf_set_private_data;
  }
  return WEED_SUCCESS;
}

#define weed_strlen(s) ((s) ? strlen((s)) : 0)
#define weed_strcmp(s1, s2) ((!(s1) || !(s2)) ? (s1 != s2) : strcmp(s1, s2))

#define get16bits(d) (*((const uint16_t *) (d)))

#define HASHROOT 5381
// fast hash from: http://www.azillionmonkeys.com/qed/hash.html
// (c) Paul Hsieh
static uint32_t weed_hash(const char *key) {
  if (key && *key) {
    int len = weed_strlen(key), rem = len & 3;
    uint32_t hash = len + HASHROOT, tmp;
    len >>= 2;
    for (; len > 0; len--) {
      hash  += get16bits (key);
      tmp = (get16bits (key+2) << 11) ^ hash;
      hash = (hash << 16) ^ tmp;
      key += 4;
      hash += hash >> 11;
    }
    switch (rem) {
    case 3: hash += get16bits (key);
      hash ^= hash << 16;
      hash ^= ((int8_t)key[2]) << 18;
      hash += hash >> 11;
      break;
    case 2: hash += get16bits (key);
      hash ^= hash << 11; hash += hash >> 17;
      break;
    case 1: hash += (int8_t)*key;
      hash ^= hash << 10; hash += hash >> 1;
      break;
    default: break;
    }
    hash ^= hash << 3; hash += hash >> 5; hash ^= hash << 4;
    hash += hash >> 17; hash ^= hash << 25; hash += hash >> 6;
    return hash;
  }
  return 0;
}

#define weed_seed_is_ptr(seed_type) (seed_type >= 64 ? 1 : 0)

#define weed_seed_get_size(seed_type, size) (seed_type == WEED_SEED_FUNCPTR ? WEED_FUNCPTR_SIZE : \
					     weed_seed_is_ptr(seed_type) ? WEED_VOIDPTR_SIZE : \
					     (seed_type == WEED_SEED_BOOLEAN || \
					      seed_type == WEED_SEED_INT) ? 4 : \
					     seed_type == WEED_SEED_DOUBLE ? 8 : \
					     seed_type == WEED_SEED_INT64 ? 8 : \
					     seed_type == WEED_SEED_STRING ? size : 0)

static inline void *weed_data_free(weed_data_t **data, weed_size_t num_valid_elems,
				   weed_size_t num_elems, uint32_t seed_type) {
  int is_nonptr = !weed_seed_is_ptr(seed_type);
  for (weed_size_t i = 0; i < num_valid_elems; i++) {
    if (is_nonptr && data[i]->value.voidptr)
      weed_unmalloc_and_copy(data[i]->size, data[i]->value.voidptr);
    weed_unmalloc_sizeof(weed_data_t, data[i]);
  }
  weed_unmalloc_and_copy(num_elems * sizeof(weed_data_t *), data);
  return NULL;
}

static inline weed_data_t **weed_data_new(uint32_t seed_type, weed_size_t num_elems,
					  weed_voidptr_t values) {
  weed_data_t **data;
  if (!num_elems) return NULL;
  if (!(data = (weed_data_t **)weed_malloc(num_elems * sizeof(weed_data_t *)))) return NULL;
  else {
    char **valuec = (char **)values;
    weed_voidptr_t *valuep = (weed_voidptr_t *)values;
    weed_funcptr_t *valuef = (weed_funcptr_t *)values;
    int is_ptr = (weed_seed_is_ptr(seed_type));
    for (int i = 0; i < num_elems; i++) {
      if (!(data[i] = weed_malloc_sizeof(weed_data_t)))
	return weed_data_free(data, --i, num_elems, seed_type);
      if (seed_type == WEED_SEED_STRING) {
        data[i]->value.voidptr = (weed_voidptr_t)(((data[i]->size = weed_strlen(valuec[i])) > 0) ?
						  (weed_voidptr_t)weed_malloc_and_copy(data[i]->size, valuec[i]) : NULL);
      } else {
        data[i]->size = weed_seed_get_size(seed_type, 0);
        if (seed_type == WEED_SEED_FUNCPTR)
	  data[i]->value.funcptr = valuef[i];
        else {
          if (is_ptr) data[i]->value.voidptr = valuep[i];
          else data[i]->value.voidptr =
		 (weed_voidptr_t)(weed_malloc_and_copy(data[i]->size,
						       (char *)values + i * data[i]->size));
        }}
      if (!is_ptr && !data[i]->value.voidptr && data[i]->size > 0) //ymemory error
        return weed_data_free(data, --i, num_elems, seed_type);
    }}
  return data;
}

static inline weed_leaf_t *weed_find_leaf(weed_plant_t *plant, const char *key, uint32_t *hash_ret) {
  uint32_t hash = WEED_MAGIC_HASH;
  weed_leaf_t *leaf = plant, *chain_leaf = NULL;
  int checkmode = 0;

  if (!plant) return NULL;

  if (key && *key) {
    // if hash_ret is set then this is a setter looking for leaf
    // in this case it already has a rwt_writelock and does not need to check further
    if (!hash_ret) {
      /// grab rwt mutex
      /// if we get a readlock, then remove it at end
      /// otherwise check flagbits, if op. is !SET, run in checking mode
      if (chain_lock_try_readlock(plant)) {
	// another thread has writelock
	if (plant->flags & WEED_FLAG_OP_DELETE) checkmode = 1;
      }
      else chain_lock_unlock(plant);
      // this counts the number of readers running in non-check mode
      if (!checkmode) {
	reader_count_add(plant);
      }
    }

    hash = weed_hash(key);

    while (leaf && (hash != leaf->key_hash || weed_strcmp((char *)leaf->key, (char *)key))) {
      leaf = leaf->next;
      if (checkmode && leaf) {
	// lock leaf so it cannot be freed till we have passed over it
	// also we will block if the next leaf is about to be adjusted
	chain_lock_readlock(leaf);
	chain_lock_unlock(chain_leaf); // does nothing if chain_leaf is NULL
	chain_leaf = leaf;
      }
    }
    if (leaf) data_lock_readlock(leaf);
    if (!hash_ret) {
      chain_lock_unlock(chain_leaf);
      if (!checkmode) reader_count_sub(plant);
    }
  }
  else data_lock_readlock(leaf);
  if (hash_ret) *hash_ret = hash;
  return leaf;
}

static inline void *weed_leaf_free(weed_leaf_t *leaf) {
  if (leaf->data)
    weed_data_free((void *)leaf->data, leaf->num_elements, leaf->num_elements, leaf->seed_type);
  if (leaf->key != leaf->padding) weed_unmalloc_and_copy(weed_strlen(leaf->key) + 1,
							 (void *)leaf->key);
  data_lock_unlock(leaf);
  data_lock_readlock(leaf);
  data_lock_upgrade(leaf, 1);
  data_lock_unlock(leaf);

  if (is_plant(leaf)) {
    plant_priv_data_t *pdata = (plant_priv_data_t *)leaf->private_data;
    weed_unmalloc_sizeof(plant_priv_data_t, pdata);
  }
  else {
    leaf_priv_data_t *ldata = (leaf_priv_data_t *)leaf->private_data;
    weed_unmalloc_sizeof(leaf_priv_data_t, ldata);
  }
  weed_unmalloc_sizeof(weed_leaf_t, leaf);
  return NULL;
}

static inline weed_leaf_t *weed_leaf_new(const char *key, uint32_t seed_type, uint32_t hash) {
  weed_leaf_t *leaf = weed_malloc_sizeof(weed_leaf_t);
  if (!leaf) return NULL;
  leaf->key_hash = hash;
  leaf->next = NULL;
  leaf->key = weed_strdup(key, weed_strlen(key));
  if (!leaf->key) {weed_unmalloc_sizeof(weed_leaf_t, leaf); return NULL;}
  leaf->num_elements = 0;
  leaf->seed_type = seed_type;
  leaf->flags = 0;
  leaf->data = NULL;
  if (is_plant(leaf)) {
    plant_priv_data_t *pdata = weed_malloc_sizeof(plant_priv_data_t);
    if (!pdata) {
      if (leaf->key != leaf->padding)
	weed_unmalloc_and_copy(weed_strlen(leaf->key + 1) + 2,
				 (void *)leaf->key);
      weed_unmalloc_sizeof(weed_leaf_t, leaf); return NULL;}
    pthread_rwlock_init(&pdata->ldata.chain_lock, NULL);
    pthread_rwlock_init(&pdata->ldata.data_lock, NULL);
    pthread_mutex_init(&pdata->ldata.data_mutex, NULL);

    pthread_rwlock_init(&pdata->reader_count, NULL);
    pthread_mutex_init(&pdata->structure_mutex, NULL);
    leaf->private_data = (void *)pdata;
  }
  else {
    leaf_priv_data_t *ldata = weed_malloc_sizeof(leaf_priv_data_t);
    if (!ldata) {
      if (leaf->key != leaf->padding)
	weed_unmalloc_and_copy(weed_strlen(leaf->key + 1) + 2,
			       (void *)leaf->key);
      weed_unmalloc_sizeof(weed_leaf_t, leaf); return NULL;}

    pthread_rwlock_init(&ldata->chain_lock, NULL);
    pthread_rwlock_init(&ldata->data_lock, NULL);
    pthread_mutex_init(&ldata->data_mutex, NULL);
    leaf->private_data = (void *)ldata;
  }
  return leaf;
}

static inline weed_error_t weed_leaf_append(weed_plant_t *plant, weed_leaf_t *newleaf) {
  newleaf->next = plant->next;
  plant->next = newleaf;
  return WEED_SUCCESS;
}

static weed_error_t _weed_plant_free(weed_plant_t *plant) {
  weed_leaf_t *leaf, *leafprev = plant, *leafnext;
  if (!plant) return WEED_SUCCESS;

  if (plant->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;

  // see: weed_leaf_delete
  chain_lock_upgrade(plant, 0, 1);

  reader_count_wait(plant);

  /// hold on to mutex until we are done
  //rwt_mutex_unlock(plant);
  leafnext = plant->next;
  while ((leaf = leafnext)) {
    leafnext = leaf->next;
    if (leaf->flags & WEED_FLAG_UNDELETABLE) {
      leafprev = leaf;
    }
    else {
      leafprev->next = leafnext;
      data_lock_readlock(leaf);
      weed_leaf_free(leaf);
    }}

  if (!plant->next) {
    // remove lock temporarily just in case other threads were trying to grab a read lock
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);

    chain_lock_upgrade(plant, 0, 1);
    reader_count_wait(plant);
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);

    data_lock_readlock(plant);
    data_lock_upgrade(plant, 1);
    weed_leaf_free(plant);
    return WEED_SUCCESS;
  }
  plant->flags ^= WEED_FLAG_OP_DELETE;
  chain_lock_unlock(plant);
  structure_mutex_unlock(plant);
  return WEED_ERROR_UNDELETABLE;
}

static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf, *leafprev = plant;
  uint32_t hash = weed_hash(key);

  // lock out all other deleters, setters
  ///
  /// why do we do this ?
  /// - we lock out other deleters because otherwise it becomes too complex
  /// another deleter may delete our prev. node or our target
  /// - we lock out setters to avoid the possibility that we are deleting the leaf following
  /// TODO:
  /// once we have passed the first leaf we can then allow setters again. There is the chance
  /// that a setter will set the value of a leaf, and then we delete it.
  /// It is thus up to the calling application to handle this possibility.

  // we will grab the mutex, forcing other rivals to drop their readlocks
  // (which actually none of them will have in the present implementation)
  // and then block.
  // and temporarily stop the flow of new readers
  // then we get writelock, and release the mutex. This allows new readers in
  // prior to this, we set
  // WEED_FLAG_OP_DELETE, forcing new readers to run in checkmode, locking
  // and unlocking the the travel rwlock as they go
  // readers which are running in non-check mode will be counted in count lock
  // so before doing any updates we will first wait for count to reach zero

  // Before deleting the target, we will witelock the prev node travel mutex.
  // Since readers are now in checkmode, they will be blocked there.
  // once we get that writelock, we know that there can be no readers on prev node
  // this is to guard against edge cases where a reader has read next link from the previous
  /// node but has not yet locked the the travel rwlock on the target node/
  // we can now repoint the prev node and unlock the prev node travel writelock
  // knowing that further readers will now be routed past the leaf to be deleted
  // We can also unlock the plant, since new readers no longer need to run in check mode.
  // finally, before deleting leaf, we must ensure there are no reamaining readers on it
  // we do this by first obtaining a travel rwlock write lock on it, then obtaining a normal
  // data writelock (there sholdnt be any writers anyway, since we locked out new writers at the
  /// start, and waited for count to reach zero)
  // we then know it is safe to delete leaf !

  chain_lock_upgrade(plant, 0, 1);

  leaf = plant;

  while (leaf && (leaf->key_hash != hash || weed_strcmp((char *)leaf->key, (char *)key))) {
    // no match

    // still have rwtlock on leafprev
    // still have rwtlock on leaf
    // we will grab rwtlock on next leaf
    if (leaf != plant) {
      if (leafprev != plant) chain_lock_unlock(leafprev);
      leafprev = leaf; // leafprev is still locked
    }
    leaf = leaf->next;
    chain_lock_readlock(leaf); // does nothing if leaf is NULL
  }
  // finish with rwtlock on prevprev. prev amd leaf
  if (!leaf || leaf == plant) {
    chain_lock_unlock(plant);
    if (leafprev != plant) chain_lock_unlock(leafprev);
    structure_mutex_unlock(plant);
    return WEED_ERROR_NOSUCH_LEAF;
  }

  if (leaf->flags & WEED_FLAG_UNDELETABLE) {
    chain_lock_unlock(plant);
    if (leafprev != leaf && leafprev != plant) chain_lock_unlock(leafprev);
    if (leaf != plant) chain_lock_unlock(leaf);
    structure_mutex_unlock(plant);
    return WEED_ERROR_UNDELETABLE;
  }

  // we dont update link until we have write trans. lock on leafprev
  // - after that, new readers will only get the new value, so we can be sure that
  // if we get trans write lock and data write lock on leaf then it is safe to free it
  // there can still be read / write locks on leafprev but we dont care about that

  // first, wait for non-checking readers to complete
  reader_count_wait(plant);

  // block readers at leafprev
  if (leafprev != plant) chain_lock_upgrade(leafprev, 1, 0);

  // adjust the link
  leafprev->next = leaf->next;

  // and that is it, job done. Now we can free leaf at leisure
  plant->flags ^= WEED_FLAG_OP_DELETE;
  chain_lock_unlock(plant);
  if (leafprev != leaf && leafprev != plant) chain_lock_unlock(leafprev);
  structure_mutex_unlock(plant);

  // get a trans write link on leaf, once we have this, all readers have moved to the next
  // leaf, and we are almosy done
  chain_lock_upgrade(leaf, 1, 0);
  chain_lock_unlock(leaf);

  // wait for all readers / writers on leaf to complete
  data_lock_writelock(leaf);
  weed_leaf_free(leaf);

  return WEED_SUCCESS;
}

static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, uint32_t flags) {
  weed_leaf_t *leaf;
  // strip any reserved bits from flags
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  // block here, flags are important
  data_lock_upgrade(leaf, 1);
  leaf->flags = (leaf->flags & WEED_FLAGBITS_RESERVED) | (flags & ~WEED_FLAGBITS_RESERVED);
  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *plant, const char *key, void *data) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  return_unlock(leaf, WEED_ERROR_CONCURRENCY);
}

static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf = weed_leaf_new(WEED_LEAF_TYPE, WEED_SEED_INT, WEED_MAGIC_HASH);
  if (!leaf) return NULL;
  leaf->data = weed_data_new(WEED_SEED_INT, 1, &plant_type);
  if (!leaf->data) return weed_leaf_free(leaf);
  leaf->num_elements = 1;
  leaf->flags = WEED_FLAG_IMMUTABLE;
  return leaf;
}

static char **_weed_plant_list_leaves(weed_plant_t *plant, weed_size_t *nleaves) {
  // must use normal malloc, strdup here since the caller will free the strings
  weed_leaf_t *leaf = plant;
  char **leaflist;
  int i = 1, j = 0;
  if (nleaves) *nleaves = 0;
  chain_lock_upgrade(plant, 0, 0);

  for (; leaf; i++) leaf = leaf->next;
  if (!(leaflist = (char **)malloc(i * sizeof(char *)))) {
    chain_lock_unlock(plant);
    return NULL;
  }
  for (leaf = plant; leaf; leaf = leaf->next) {
    leaflist[j++] = strdup(leaf->key);
    if (!leaflist[j - 1]) {
      chain_lock_unlock(plant);
      for (--j; j > 0; free(leaflist[--j]));
      free(leaflist);
      return NULL;
    }}
  chain_lock_unlock(plant);
  leaflist[j] = NULL;
  if (nleaves) *nleaves = j;
  return leaflist;
}

static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key,
				   uint32_t seed_type, weed_size_t num_elems,
                                   weed_voidptr_t values) {
  weed_data_t **data = NULL;
  weed_leaf_t *leaf;
  uint32_t hash;
  int isnew = 0;
  weed_size_t old_num_elems = 0;
  weed_data_t **old_data = NULL;

  if (!plant) return WEED_ERROR_NOSUCH_LEAF;
  if (!IS_VALID_SEED_TYPE(seed_type)) return WEED_ERROR_WRONG_SEED_TYPE;

  // lock out other setters
  chain_lock_upgrade(plant, 0, 0);

  if (!(leaf = weed_find_leaf(plant, key, &hash))) {
    if (!(leaf = weed_leaf_new(key, seed_type, hash))) {
      chain_lock_unlock(plant);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }
    isnew = 1;
  } else {
    // we hold a readlock on the leaf, we will to grab the mutex and upgrade to a writelock
    // if we fail, too bad the other thread has priority

    if (seed_type != leaf->seed_type) {
      chain_lock_unlock(plant);
      return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
    }
    if (leaf->flags & WEED_FLAG_IMMUTABLE) {
      chain_lock_unlock(plant);
      return_unlock(leaf, WEED_ERROR_IMMUTABLE);
    }
    if (leaf == plant && num_elems != 1) {
      chain_lock_unlock(plant);
      return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);  ///< type leaf must always have exactly 1 value
    }
    if (data_lock_upgrade(leaf, 0)) {
      data_lock_unlock(leaf);
      chain_lock_unlock(plant);
      return_unlock(leaf, WEED_ERROR_CONCURRENCY);
    }

    chain_lock_unlock(plant);
    old_num_elems = leaf->num_elements;
    old_data = leaf->data;
  }

  if (num_elems) {
    data = (weed_data_t **)weed_data_new(seed_type, num_elems, values);
    if (!data) {
      // memory failure...
      if (isnew) {
	chain_lock_unlock(plant);
	weed_leaf_free(leaf);
	leaf = NULL;
      }
      return_unlock(leaf, WEED_ERROR_MEMORY_ALLOCATION);
    }
  }

  leaf->data = data;
  leaf->num_elements = num_elems;

  if (isnew) {
    weed_leaf_append(plant, leaf);
    chain_lock_unlock(plant);
  }
  else data_lock_unlock(leaf);

  if (old_data) weed_data_free(old_data, old_num_elems, old_num_elems, seed_type);

  return WEED_SUCCESS;
}

static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, int32_t idx,
				   weed_voidptr_t value) {
  weed_data_t **data;
  uint32_t type;
  weed_leaf_t *leaf;

  if (!(leaf = weed_find_leaf(plant, key, NULL))) {
    return WEED_ERROR_NOSUCH_LEAF;
  }

  if (idx >= leaf->num_elements) {
    return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);
  }
  if (!value) return_unlock(leaf, WEED_SUCCESS);

  type = leaf->seed_type;
  data = leaf->data;

  if (type == WEED_SEED_FUNCPTR) *((weed_funcptr_t *)value) = data[idx]->value.funcptr;
  else {
    if (weed_seed_is_ptr(type)) *((weed_voidptr_t *)value) = data[idx]->value.voidptr;
    else {
      if (type == WEED_SEED_STRING) {
	size_t size = (size_t)data[idx]->size;
	char **valuecharptrptr = (char **)value;
	if (size > 0) memcpy(*valuecharptrptr, data[idx]->value.voidptr, size);
	(*valuecharptrptr)[size] = 0;
      } else memcpy(value, data[idx]->value.voidptr, leaf->data[idx]->size);
    }}

  return_unlock(leaf, WEED_SUCCESS);
}

static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return 0;
  return_unlock(leaf, leaf->num_elements);
}


static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, int32_t idx) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return 0;
  if (idx > leaf->num_elements) return_unlock(leaf, 0);
  return_unlock(leaf, leaf->data[idx]->size);
}

static uint32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return WEED_SEED_INVALID;
  return_unlock(leaf, leaf->seed_type);
}

static uint32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return 0;
  return_unlock(leaf, (uint32_t)(leaf->flags & ~WEED_FLAGBITS_RESERVED));
}

static weed_error_t _weed_leaf_get_private_data(weed_plant_t *plant, const char *key,
						void **data_return) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  return_unlock(leaf, WEED_ERROR_CONCURRENCY);
}
