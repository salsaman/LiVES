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

   partly based on LiViDO, which was developed by:
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

/* (C) G. Finch, 2005 - 2022 */

// implementation of libweed with or without glib's slice allocator, with or without thread safety (needs pthread)

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define __LIBWEED__

#define _WEED_ABI_VERSION_MAX_SUPPORTED 202
#define _WEED_ABI_VERSION_MIN_SUPPORTED 200

#define WEED_MAGIC_HASH 0xB82E802F

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#else
#include "weed.h"
#endif

#ifdef ENABLE_PROXIES
#define _get_leaf_proxy(leaf) if (leaf && (leaf->flags & WEED_FLAG_PROXY)) {\
    weed_leaf_t *proxy = (weed_leaf_t *)leaf->data[0]->value.voidptr;	\
    if (proxy)data_lock_readlock(proxy);data_lock_unlock(leaf);leaf = proxy;}}
#endif

#ifdef WRITER_PREF_AVAILABLE
#undef WRITER_PREF_AVAILABLE
#endif

#if _XOPEN_SOURCE >= 500 || _POSIX_C_SOURCE >= 200809
#define WRITER_PREF_AVAILABLE 1
#define WEED_INIT_PRIV_NO_PREF_WRITERS (1ull << 32)
#else
#define WRITER_PREF_AVAILABLE 0
#endif

#define WEED_INIT_PRIV_SKIP_ERRCHECKS (1ull << 33)

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
  // with a data_lock readlock held, this function can be used to try to
  // grab a writelock, this is done with data_lock_mutex locked
  // if the mutex cannot be obtained immediately, drop the readlock, in case another
  // thread holding the mutex is waiting fro a writelock
  // otherwise if we get the mutex right away we drop the readlock after doing so
  // then with the data_mutex locked, we wait to get the writelock and unlock the mutex
  // lock is set then if we fail to get the mutex we just exit with nozero
  // exit of zero implies the writelock was botained
  if (leaf) {
    // try to grab the mutex
    int ret = pthread_mutex_trylock(get_data_mutex(leaf));
    if (ret) {
      // if we fail and !block, return with data readlock held
      if (!block) return ret;
      // otherwise, drop the data readlock in case a writer is blocked
      // then block until we do get data mutex
      data_lock_unlock(leaf);
      pthread_mutex_lock(get_data_mutex(leaf));
    }
    // now we can wait for the writelock and then unlock mutex
    // subsequent writers will drop their readlocks and block on the mutex
    if (!ret) data_lock_unlock(leaf);
    data_lock_writelock(leaf);
    pthread_mutex_unlock(get_data_mutex(leaf));
    return 0;
  }
  return 1;
}

static int chain_lock_upgrade(weed_leaf_t *leaf, int have_rdlock, int is_del) {
  // there are two possibilites here:
  // have_readlock == FALSE
  //  (this is ALWAYS called with leaf == plant)
  //  - obtain the structure lock
  //  - obtain a writelock on the chainlock for the leaf (plant)
  // - (if deleting, set flag bit on leaf) (plant)
  // - release structure lock
  // --- this ensures we have a chainlock writelock on plant, use of the structure lock
  //     restricts access to the chain lock.
  //     This achieves various things - only one thread may have the writelock on plant chainlock
  //				- if the writelock can be obtained, this means there are threads reading th chain at plant
  //				- if no threads are reading the chain at plant, it is safe to append a new leaf after plant
  //				- additionally, with structure_mutex held, a thread can set a flagbit for plant
  //				- to indicate a deletion is in progress.
  //   	                           - all threads traversing the linked list first try to obtain a chain lock read lock
  //					on plant. Should they fail, and the deletion bit is set in flags for plant,
  //				-- failing the readlock and reading the delete flag bit causes a reader thread to
  //					enter a special "checking mode"
  // 
  
  // have_readlock == TRUE
  // this is only called with leaf != plant, the leaf must have a chainlock readlock
  // this means if threads are checking ahead by trying to obtain a chainlock readlock
  // on the next leaf, they will block at that
  // point in list, until the wchainlock writelock is released
  // in this mode:
  // - drop chain lock readlock for leaf
  // - get chainlock write lock
  // combining these two concepts, when a thread enters "checking mode", it does not increment the "list readers" counter
  // - thus the readers counter becomes a measure of how many threads are traversing the list without being in "checking mode"
  // - when the count falls to zere as exiting threads not in checking mode decremtn it as they leave, then one can
  // be certain that any threads currently traversing the list are doing so in checking mode
  // once a deletion thread is certain that all threads traversing are in checking mode, it can create a temporay roadblock
  // by getting a chain lock write lock on a leaf
  // the comments below explain this
  //
  // return value: always 9
  if (leaf) {
    if (have_rdlock) chain_lock_unlock(leaf);
    else structure_mutex_lock(leaf);

    // for plants -
    // now we have the structure mutex lock, we can be certain that other threads waiting to write will have dropped
    // any readlocks. Thus as soon as there are no more readers we will get a write lock on chain_lock for plant

    // there are two reasons why we would want this. If setting a leaf value we lock out other setters, thus avoiding the
    // possibility that two threads might both add the same leaf. With the lock held, we check if the leaf already exists,
    // and if so we get a write lock on data lock before releasing this;
    // otherwise the lock is held until after the new leaf is added.
    // since readers only use try_readlock, they are not blocked at all, the only overhead
    // is that they will check leaf->flags

    // when deleting a leaf, as well as locking out any setters or other deleters,
    // we ensure that any readers from now on traverse the list in check-mode
    // which allows the leaf to be deleted without locking the entire plant for readers

    // check mode adds only a minute overhead, and we make the assumption the deleting leaves will occur relatively rarely

    chain_lock_writelock(leaf); /// a crash here is indicative of a double weed_plant_free()

    // if it is a SET, then we release the mutex; the next writer
    // will block on writelock; subsequent writeers will block on the mutex
    if (!have_rdlock) {
      if (is_del) leaf->flags |= WEED_FLAG_OP_DELETE;
      else structure_mutex_unlock(leaf);
    }
  }
  return 0;
}

static int allbugfixes = 0;
static int debugmode = 0;
static int extrafuncs = 0;
#if WRITER_PREF_AVAILABLE
static int pref_writers = 1;
#endif
static int skip_errchecks = 0;

static int32_t _abi_ = _WEED_ABI_VERSION_MAX_SUPPORTED;

EXPORTED size_t libweed_get_leaf_t_size(void) GNU_CONST;
EXPORTED size_t libweed_get_data_t_size(void) GNU_CONST;

EXPORTED int32_t libweed_get_abi_version(void) GNU_PURE;

EXPORTED int32_t libweed_get_abi_max_supported(void) GNU_CONST;
EXPORTED int32_t libweed_get_abi_min_supported(void) GNU_CONST;

EXPORTED void libweed_print_init_opts(FILE *);

EXPORTED weed_error_t libweed_init(int32_t abi, uint64_t init_flags);
EXPORTED int libweed_set_memory_funcs(weed_malloc_f, weed_free_f, weed_calloc_f);
EXPORTED int libweed_set_slab_funcs(libweed_slab_alloc_clear_f, libweed_slab_unalloc_f, libweed_slab_alloc_and_copy_f);

#if WEED_ABI_CHECK_VERSION(202)
EXPORTED size_t weed_leaf_get_byte_size(weed_plant_t *, const char *key);
EXPORTED size_t weed_plant_get_byte_size(weed_plant_t *);
EXPORTED  weed_error_t __wbg__(size_t, weed_hash_t, int, weed_plant_t *, const char *, weed_voidptr_t);
#endif

static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_plant_free(weed_plant_t *) GNU_FLATTEN;
static char **_weed_plant_list_leaves(weed_plant_t *, weed_size_t *nleaves) GNU_FLATTEN;
static weed_error_t _weed_leaf_get(weed_plant_t *, const char *key, weed_size_t idx,
				   weed_voidptr_t value) GNU_HOT;
static weed_error_t _weed_leaf_set(weed_plant_t *, const char *key, uint32_t seed_type,
				   weed_size_t num_elems, weed_voidptr_t values) GNU_FLATTEN;
static weed_size_t _weed_leaf_num_elements(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *, const char *key, weed_size_t idx) GNU_FLATTEN;
static uint32_t _weed_leaf_seed_type(weed_plant_t *, const char *key) GNU_FLATTEN;
static uint32_t _weed_leaf_get_flags(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_leaf_set_flags(weed_plant_t *, const char *key, uint32_t flags) GNU_FLATTEN;

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *, const char *key, void *data)
  GNU_FLATTEN;
static weed_error_t _weed_leaf_get_private_data(weed_plant_t *, const char *key, void **data_return)
  GNU_FLATTEN;
#if WEED_ABI_CHECK_VERSION(202)
static weed_error_t _weed_ext_set_element_size(weed_plant_t *, const char *key, weed_size_t idx,
						weed_size_t new_size) GNU_FLATTEN;
static weed_error_t _weed_ext_append_elements(weed_plant_t *, const char *key, uint32_t seed_type,
					      weed_size_t nvals, void *data) GNU_FLATTEN;
static weed_error_t _weed_ext_attach_leaf(weed_plant_t *src, const char *key, weed_plant_t *dst)
  GNU_FLATTEN;

static weed_error_t _weed_ext_detach_leaf(weed_plant_t *, const char *key) GNU_FLATTEN;
#endif
/* internal functions */
static inline weed_leaf_t *weed_find_leaf(weed_plant_t *, const char *key, weed_hash_t *hash_ret,
				   weed_leaf_t **refnode) GNU_FLATTEN GNU_HOT;
static inline weed_leaf_t *weed_leaf_new(const char *key, uint32_t seed_type, weed_hash_t hash) GNU_FLATTEN;

static weed_size_t nullv = 1;
static weed_size_t pptrsize = WEED_PLANTPTR_SIZE;

static inline int weed_strcmp(const char *, const char *) GNU_HOT;
static inline weed_size_t weed_strlen(const char *) GNU_PURE;
static inline weed_hash_t weed_hash(const char *) GNU_PURE;

/* memfuncs */

#ifndef _weed_malloc
#define _weed_malloc malloc
#endif

#ifndef _weed_calloc
#define _weed_calloc calloc
#endif

#ifndef _weed_free
#define _weed_free free
#endif

static weed_malloc_f weed_malloc = _weed_malloc;
static weed_calloc_f weed_calloc = _weed_calloc;
static weed_free_f weed_free = _weed_free;

#ifndef weed_memcpy
#define weed_memcpy memcpy
#endif
#ifndef weed_memset
#define weed_memset memset
#endif

#ifndef _weed_malloc0
static void *_weed_malloc0(size_t sz) {void *p = weed_malloc(sz); if (p) weed_memset(p, 0, sz); return p;}
#endif
static weed_malloc_f weed_malloc0 = _weed_malloc0;
static void *_malloc0_product(size_t ne, size_t sz) {return weed_malloc0(ne * sz);}

#ifndef _weed_malloc_copy
static void *_weed_malloc_copy(size_t sz, void *p) {return weed_memcpy(weed_malloc(sz), p, sz);}
#endif
static libweed_slab_alloc_and_copy_f weed_malloc_and_copy = _weed_malloc_copy;

#ifndef _weed_unmalloc_copy
static void _weed_unmalloc_copy(size_t sz, void *ptr) {weed_free(ptr);}
#endif
static libweed_unmalloc_and_copy_f weed_unmalloc_and_copy = _weed_unmalloc_copy;

#define weed_calloc_sizeof(t) weed_calloc(1, sizeof(t))
#define weed_uncalloc_sizeof(t, ptr) weed_unmalloc_and_copy(sizeof(t), ptr)

/* end memfuncs */

#define KEY_IN_SIZE (_WEED_PADBYTES_ ? _WEED_PADBYTES_ + sizeof(char *) : 0)

#define weed_leaf_get_key(leaf) ((!_WEED_PADBYTES_ || !leaf->padding[0]) \
				 ? leaf->key : (const char *)leaf->padding)

#define weed_leaf_set_key(leaf, key, size) \
  (size < KEY_IN_SIZE ? weed_memcpy((void *)leaf->padding, key, size + 1)	\
   : ((leaf->key = weed_malloc_and_copy(size + 1, (void *)key)))	\
   ? leaf->key : NULL)

EXPORTED int libweed_set_memory_funcs(weed_malloc_f my_malloc, weed_free_f my_free, weed_calloc_f my_calloc) {
  weed_malloc = my_malloc;
  if (my_calloc) weed_calloc = my_calloc;
  else my_calloc = _malloc0_product;
  weed_free = my_free;
  return 0;
}

EXPORTED int libweed_set_slab_funcs(libweed_slab_alloc_clear_f my_slab_alloc0, libweed_slab_unalloc_f my_slab_unalloc,
				 libweed_slab_alloc_and_copy_f my_slab_alloc_and_copy) {
  weed_malloc0 = my_slab_alloc0;
  weed_calloc = _malloc0_product;
  weed_unmalloc_and_copy = my_slab_unalloc;
  if (my_slab_alloc_and_copy) weed_malloc_and_copy = my_slab_alloc_and_copy;
  return 0;
}

EXPORTED size_t libweed_get_leaf_t_size(void) {return sizeof(weed_leaf_t);}
EXPORTED size_t libweed_get_data_t_size(void) {return sizeof(weed_data_t);}

EXPORTED int32_t libweed_get_abi_version(void) {return _abi_;}

EXPORTED int32_t libweed_get_abi_min_supported_version(void) {return _WEED_ABI_VERSION_MAX_SUPPORTED;}
EXPORTED int32_t libweed_get_abi_max_supported_version(void) {return _WEED_ABI_VERSION_MIN_SUPPORTED;}

EXPORTED void libweed_print_init_opts(FILE *out) {
  fprintf(out, "%s\t%s\n", " BIT  0:  WEED_INIT_ALLBUGFIXES	", "Backport all future non-breaking bug fixes "
	  "inro the current API version");

  fprintf(out, "%s\t%s\n"," BIT  1: WEED_INIT_DEBUG		", "Run libweed in debug mode.");

  fprintf(out, "%s\t%s\n"," BIT  2: WEED_INIT_EXTENDED_FUNCS	", "Enable host only extended functions.");

#if WRITER_PREF_AVAILABLE
  fprintf(out, "%s\t%s\n", "BIT 32: No prefer writers		",
	  "If functionality permits, the  dafault is to force readers to block if there is a writer waiting "
	  "to update data. This is useful in applications where there are many more readers than writers.\n"
	  "In applications with more writers than readers, the option can be disabled "
	  "so that readers are preferred instead.");
#endif

  fprintf(out, "%s\t%s\n","BIT 33: Skip errchecks		", "Optimise performance by skipping unnecessary edge case eror checks.");
}


EXPORTED weed_error_t libweed_init(int32_t abi, uint64_t init_flags) {
  // this is called by the host in order for it to set its version of the functions

  // *the plugin should never call this, instead the plugin functions are passed to the plugin
  // from the host in the "host_info" plant*

  if (abi < 0 || abi > WEED_ABI_VERSION) return WEED_ERROR_BADVERSION;
  _abi_ = abi;

  if (init_flags & WEED_INIT_ALLBUGFIXES) allbugfixes = 1;
  else allbugfixes = 0;
  if (init_flags & WEED_INIT_DEBUGMODE) debugmode = 1;
  else debugmode = 0;
  if (init_flags & WEED_INIT_EXTENDED_FUNCS) extrafuncs = 1;
  else extrafuncs = 0;
#if WRITER_PREF_AVAILABLE
  if (init_flags & WEED_INIT_PRIV_NO_PREF_WRITERS) pref_writers = 0;
  else pref_writers = 1;
#endif
  if (init_flags & WEED_INIT_PRIV_SKIP_ERRCHECKS) skip_errchecks = 1;
  else skip_errchecks = 0;

  if (_abi_ < 201 && !allbugfixes) nullv = 0;
  if (_abi_ < 202 && !allbugfixes) pptrsize = WEED_SEED_VOIDPTR;

  if (debugmode) {
    fprintf(stderr, "Weed abi %d selected%s\n", _abi_, allbugfixes ? ", bugfix mode enabled" : "");
    fprintf(stderr, "Library incorporates thread-safety features\n");
    fprintf(stderr, "Internal key space is %ld\n", KEY_IN_SIZE);
    fprintf(stderr, "Weed data_t size is %ld\n", libweed_get_data_t_size());
    fprintf(stderr, "Weed leaf size is %ld\n", libweed_get_leaf_t_size());
    fprintf(stderr, "NULL values in strings are %s\n", nullv ? "enabled" : "disabled");
    if (!nullv && !allbugfixes)
      fprintf(stderr, " - feature can be enabled by passing option WEED_INIT_ALLBUGFIXES to weed_init()\n");
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

  // host only
  weed_leaf_set_flags = _weed_leaf_set_flags;
  weed_leaf_get_private_data = _weed_leaf_get_private_data;
  weed_leaf_set_private_data = _weed_leaf_set_private_data;

#if WEED_ABI_CHECK_VERSION(202)
  // host only extended
  if (extrafuncs) {
    weed_ext_attach_leaf = _weed_ext_attach_leaf;
    weed_ext_detach_leaf = _weed_ext_detach_leaf;
    weed_ext_set_element_size = _weed_ext_set_element_size;
    weed_ext_append_elements = _weed_ext_append_elements;
  }
#endif
  return WEED_SUCCESS;
}

#define weed_strlen(s) ((s) ? strlen((s)) + nullv : 0)
#define weed_strcmp(s1, s2) ((!(s1) || !(s2)) ? (s1 != s2) : strcmp(s1, s2))

#define get16bits(d) (*((const uint16_t *) (d)))

#define HASHROOT 5381
// fast hash from: http://www.azillionmonkeys.com/qed/hash.html
// (c) Paul Hsieh

static weed_hash_t weed_hash(const char *key) {
  if (key && *key) {
    size_t len = strlen(key), rem = len & 3;
    uint32_t hash = len + HASHROOT, tmp;
    len >>= 2;
    for (; len > 0; len--) {
      hash  += get16bits (key);
      tmp = (get16bits (key + 2) << 11) ^ hash;
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

#define weed_seed_is_ptr(seed_type) ((seed_type) >= WEED_SEED_FIRST_PTR_TYPE ? 1 : 0)

#if WEED_ABI_CHECK_VERSION(202)
#define _vs(a) a
#else
#define _vs(a) WEED_VOIDPTR_SIZE
#endif

#define weed_seed_get_size(seed_type, size)				\
  (seed_type == WEED_SEED_STRING ? size				\
   : (seed_type == WEED_SEED_VOIDPTR					\
      || seed_type >= WEED_SEED_FIRST_CUSTOM) ? _vs(size)		\
: seed_type == WEED_SEED_FUNCPTR ? WEED_FUNCPTR_SIZE			\
   : (seed_type == WEED_SEED_BOOLEAN || seed_type == WEED_SEED_INT	\
      || seed_type == WEED_SEED_UINT)					\
   ? 4 : (seed_type == WEED_SEED_DOUBLE || seed_type == WEED_SEED_FLOAT	\
	  || seed_type == WEED_SEED_INT64 || seed_type == WEED_SEED_UINT64) \
   ? 8 : seed_type == WEED_SEED_PLANTPTR ? pptrsize : 0)

#undef _vptsize

  static inline void *weed_data_free(weed_data_t **data, weed_size_t num_valid_elems,
				   weed_size_t num_elems, uint32_t seed_type) {
  int is_nonptr = (seed_type > 0 && seed_type <= WEED_SEED_LAST_NON_PTR_TYPE);
  int xnullv = 0, minsize = WEED_VOIDPTR_SIZE;
  if (seed_type == WEED_SEED_STRING) minsize = xnullv = nullv;
  for (weed_size_t i = 0; i < num_valid_elems; i++) {
    if (is_nonptr && data[i]->size > minsize && data[i]->value.voidptr)
      weed_unmalloc_and_copy(data[i]->size - xnullv, data[i]->value.voidptr);
    weed_uncalloc_sizeof(weed_data_t, data[i]);
  }
  weed_unmalloc_and_copy(num_elems * sizeof(weed_data_t *), data);
  return NULL;
}

static inline weed_data_t **weed_data_new(uint32_t seed_type, weed_size_t num_elems,
					  weed_voidptr_t values) {
  weed_data_t **data;
  if (!num_elems) return NULL;
  if (!(data = (weed_data_t **)weed_calloc(num_elems, sizeof(weed_data_t *)))) return NULL;
  else {
    char **valuec = (char **)values;
    weed_voidptr_t *valuep = (weed_voidptr_t *)values;
    weed_funcptr_t *valuef = (weed_funcptr_t *)values;
    int is_ptr = (weed_seed_is_ptr(seed_type));
    for (int i = 0; i < num_elems; i++) {
      if (!(data[i] = weed_calloc_sizeof(weed_data_t)))
	return weed_data_free(data, --i, num_elems, seed_type);
      if (seed_type == WEED_SEED_STRING) {
	data[i]->value.voidptr = valuec ?
	  (weed_voidptr_t)(((data[i]->size = weed_strlen(valuec[i])) > nullv) ?
			   (weed_voidptr_t)weed_malloc_and_copy(data[i]->size - nullv,
								valuec[i]) : NULL) : NULL; continue;}
      data[i]->size = weed_seed_get_size(seed_type, 0);
      if (seed_type == WEED_SEED_FUNCPTR) {
	data[i]->value.funcptr = valuef ? valuef[i] : NULL; continue;}
      if (is_ptr) {data[i]->value.voidptr = valuep ? valuep[i] : NULL; continue;}
      if (data[i]->size <= WEED_VOIDPTR_SIZE) {
	weed_memcpy(&data[i]->value.voidptr, (char *)values + i * data[i]->size, data[i]->size); continue;}
      data[i]->value.voidptr =
	(weed_voidptr_t)(weed_malloc_and_copy(data[i]->size, (char *)values + i * data[i]->size));
      if (!is_ptr && !data[i]->value.voidptr && data[i]->size > nullv) //memory error
	return weed_data_free(data, --i, num_elems, seed_type);}} return data;
}

static inline weed_data_t **weed_data_append(weed_leaf_t *leaf,
					     weed_size_t new_elems, weed_voidptr_t values) {
  weed_data_t **data;
  if (!new_elems) return leaf->data;;
  new_elems += leaf->num_elements;
  if (!(data = (weed_data_t **)weed_calloc(new_elems, sizeof(weed_data_t *)))) return NULL;
  else {
    char **valuec = (char **)values;
    weed_voidptr_t *valuep = (weed_voidptr_t *)values;
    weed_funcptr_t *valuef = (weed_funcptr_t *)values;
    uint32_t seed_type = leaf->seed_type;
    int is_ptr = (weed_seed_is_ptr(seed_type));
    int j = 0;
    for (int i = 0; i < leaf->num_elements; i++) data[i] = leaf->data[i];
    for (int i = leaf->num_elements; i < new_elems; i++) {
      if (!(data[i] = weed_calloc_sizeof(weed_data_t)))
	return weed_data_free(data, --i, new_elems, seed_type);
      if (seed_type == WEED_SEED_STRING) {
	data[i]->value.voidptr = valuec ?
	  (weed_voidptr_t)(((data[i]->size = weed_strlen(valuec[j])) > nullv) ?
			   (weed_voidptr_t)weed_malloc_and_copy(data[i]->size - nullv,
								valuec[j]) : NULL) : NULL; continue;}
      data[i]->size = weed_seed_get_size(seed_type, 0);
      if (seed_type == WEED_SEED_FUNCPTR) {
	data[i]->value.funcptr = valuef ? valuef[j] : NULL; continue;}
      if (is_ptr) {data[i]->value.voidptr = valuep ? valuep[j] : NULL; continue;}
      if (data[i]->size <= WEED_VOIDPTR_SIZE) {
	weed_memcpy(&data[i]->value.voidptr, (char *)values + j * data[i]->size, data[i]->size); continue;}
      data[i]->value.voidptr =
	(weed_voidptr_t)(weed_malloc_and_copy(data[i]->size, (char *)values + j * data[i]->size));
      if (!is_ptr && !data[i]->value.voidptr && data[i]->size > nullv) //memory error
	return weed_data_free(data, --i, new_elems, seed_type);
      j++;
    }
  }
  return data;
}

static inline weed_leaf_t *weed_find_leaf(weed_plant_t *plant, const char *key, weed_hash_t *hash_ret,
					  weed_leaf_t **refnode) {
  weed_hash_t hash = 0;
  weed_leaf_t *leaf = plant, *chain_leaf = NULL;
  int is_writer = 1, checkmode = 0;

  if (!plant) return NULL;

  if (key && *key) {
    // if hash_ret is set then this is a setter looking for leaf
    // in this case it already has a chain_lock writelock and does not need to check further
    if (!hash_ret || !refnode || !*refnode) {
      /// grab chain_lock readlock
      /// if we get a readlock, then remove it
      /// otherwise check flagbits, if op. is !SET, run in checking mode
      is_writer = 0;
      if (chain_lock_try_readlock(plant)) {
	// another thread has writelock
	if (plant->flags & WEED_FLAG_OP_DELETE) checkmode = 1;
      }
      else chain_lock_unlock(plant);
      // this counts the number of readers running in non-check mode
      if (!checkmode) reader_count_add(plant);
    }
    if (hash_ret) hash = *hash_ret;
    if (!hash) hash = weed_hash(key);

    if (!checkmode && !refnode) {
      while (hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key)))
	if (!(leaf = leaf->next)) break;
    }
    else {
      while (hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key))) {
	if (refnode) {
	  if (leaf == plant) {
	    if (!(leaf = leaf->next)) break;
	    continue;
	  }
	  if (*refnode) {
	    if (leaf == *refnode) return NULL;
	    if (!(leaf = leaf->next)) break;
	    continue;
	  }
	  *refnode = leaf;
	  if (!checkmode) {
	    while (hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key)))
	      if (!(leaf = leaf->next)) break;
	    break;
	  }
	  refnode = NULL;
	}
	if (checkmode) {
	  // lock leaf so it cannot be freed till we have passed over it
	  // also we will block if the next leaf is about to be adjusted
	  chain_lock_readlock(leaf);
	  chain_lock_unlock(chain_leaf); // does nothing if chain_leaf is NULL
	  chain_leaf = leaf;
	}
	if (!(leaf = leaf->next)) break;
      }
    }

    if (leaf) data_lock_readlock(leaf);
    if (!is_writer) {
      // checkmode (and by extension chain_leaf) can only possibly be non-zero if ! is_writer
      chain_lock_unlock(chain_leaf);
      if (!checkmode) reader_count_sub(plant);
    }
  }
  else {
    hash = WEED_MAGIC_HASH;
    data_lock_readlock(leaf);
  }
  if (hash_ret) *hash_ret = hash;
  return leaf;
}

static inline void *weed_leaf_free(weed_leaf_t *leaf) {
  if (leaf) {
    if (leaf->data)
      weed_data_free((void *)leaf->data, leaf->num_elements, leaf->num_elements, leaf->seed_type);
    if (!_WEED_PADBYTES_ || !*leaf->padding)
      weed_unmalloc_and_copy(strlen(leaf->key) + 1, (void *)leaf->key);
    data_lock_unlock(leaf);
    data_lock_readlock(leaf);
    data_lock_upgrade(leaf, 1);
    data_lock_unlock(leaf);

    if (is_plant(leaf)) {
      plant_priv_data_t *pdata = (plant_priv_data_t *)leaf->private_data;
      weed_uncalloc_sizeof(plant_priv_data_t, pdata);
    }
    else {
      leaf_priv_data_t *ldata = (leaf_priv_data_t *)leaf->private_data;
      weed_uncalloc_sizeof(leaf_priv_data_t, ldata);
    }
    weed_uncalloc_sizeof(weed_leaf_t, leaf);
  }
  return NULL;
}

static inline weed_leaf_t *weed_leaf_new(const char *key, uint32_t seed_type, weed_hash_t hash) {
  pthread_rwlockattr_t *rwattrp = NULL;
#if WRITER_PREF_AVAILABLE
  pthread_rwlockattr_t rwattr;
#endif
  const char *xkey;
  weed_leaf_t *leaf = weed_calloc_sizeof(weed_leaf_t);
  if (!leaf) return NULL;
  leaf->padding[0] = 0;
  xkey = weed_leaf_set_key(leaf, key, strlen(key));
  if (!xkey) {weed_uncalloc_sizeof(weed_leaf_t, leaf); return NULL;}
  leaf->key_hash = hash;
  leaf->next = NULL;
  leaf->num_elements = 0;
  leaf->seed_type = seed_type;
  leaf->flags = 0;
  leaf->data = NULL;
#if WRITER_PREF_AVAILABLE
  rwattrp = &rwattr;
  pthread_rwlockattr_init(rwattrp);
  pthread_rwlockattr_setkind_np(rwattrp, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

  if (is_plant(leaf)) {
    plant_priv_data_t *pdata = weed_calloc_sizeof(plant_priv_data_t);
    if (!pdata) {
      if (weed_leaf_get_key(leaf) != leaf->padding)
	weed_unmalloc_and_copy(strlen(leaf->key + 1), (void *)leaf->key);
      weed_uncalloc_sizeof(weed_leaf_t, leaf); return NULL;}
    pthread_rwlock_init(&pdata->ldata.chain_lock, rwattrp);
    pthread_rwlock_init(&pdata->ldata.data_lock, rwattrp);
    pthread_mutex_init(&pdata->ldata.data_mutex, NULL);

    pthread_rwlock_init(&pdata->reader_count, NULL);
    pthread_mutex_init(&pdata->structure_mutex, NULL);
    leaf->private_data = (void *)pdata;
  }
  else {
    leaf_priv_data_t *ldata = weed_calloc_sizeof(leaf_priv_data_t);
    if (!ldata) {
      if (weed_leaf_get_key(leaf) != leaf->padding)
	weed_unmalloc_and_copy(strlen(leaf->key + 1), (void *)leaf->key);
      weed_uncalloc_sizeof(weed_leaf_t, leaf); return NULL;}

    pthread_rwlock_init(&ldata->chain_lock, rwattrp);
    pthread_rwlock_init(&ldata->data_lock, rwattrp);
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
  chain_lock_upgrade(plant, 0, 0);
  // structure mutex is locked

  reader_count_wait(plant);

  /// hold on to structure_mutex until we are done
  leafnext = plant->next;
  while ((leaf = leafnext)) {
    leafnext = leaf->next;
    if (leaf->flags & WEED_FLAG_UNDELETABLE) leafprev = leaf;
    else {
      leafprev->next = leafnext;
      chain_lock_readlock(leaf);
      chain_lock_upgrade(leaf, 1, 0);
      chain_lock_unlock(leaf);
      data_lock_writelock(leaf);
      weed_leaf_free(leaf);
    }
  }

  if (!plant->next) {
    // remove lock temporarily just in case other threads were trying to grab a read lock
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);

    chain_lock_upgrade(plant, 0, 1);
    // structure_mutex locked
    reader_count_wait(plant);
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);

    data_lock_readlock(plant);
    data_lock_upgrade(plant, 1);
    weed_leaf_free(plant);
    return WEED_SUCCESS;
  }
  plant->flags &= ~WEED_FLAG_OP_DELETE;
  chain_lock_unlock(plant);
  structure_mutex_unlock(plant);
  return WEED_ERROR_UNDELETABLE;
}

static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf, *leafprev = plant;
  weed_hash_t hash = weed_hash(key);
  weed_error_t err = WEED_SUCCESS;

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
  // and unlocking the travel rwlock as they go
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

  // grab chain lock writelock on plant
  chain_lock_upgrade(plant, 0, 1);
  // because we are deleting, we keep structure_mutex locked

  leaf = plant;
  // we want to end with a lock on matghing leaf, and leaf before (prevleaf)
  while (leaf->key_hash != hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key))) {
    // no match
    // get next leaf and lock it, then unlock the current leaf
    // normally we would do this after locking leaf, but in this case we want to keep lock on leaf
    // and on prevleaf
    if (leafprev != plant) chain_lock_unlock(leafprev);
    // we have lock on leaf, either from last loop, or because it is plant
    leafprev = leaf;
    leaf = leaf->next;
    if (!leaf) break;
    chain_lock_readlock(leaf);
  }
  // finish with chain_lock readlock on prev and leaf

  if (!leaf || leaf == plant) err = WEED_ERROR_NOSUCH_LEAF;
  else {
#ifdef ENABLE_PROXIES
    if (leaf->flags & WEED_FLAG_PROXY) err = _weed_ext_detatch_leaf_int(leaf);
#endif
    if (leaf->flags & WEED_FLAG_UNDELETABLE) err = WEED_ERROR_UNDELETABLE;
  }

  if (err != WEED_SUCCESS) {
    chain_lock_unlock(plant);
    if (leafprev != leaf && leafprev != plant) chain_lock_unlock(leafprev);
    if (leaf && leaf != plant) chain_lock_unlock(leaf);
    structure_mutex_unlock(plant);
    return err;
  }

  // we dont update link until we have write trans. lock on leafprev
  // - after that, new readers will only get the new value, so we can be sure that
  // if we get trans write lock and data write lock on leaf then it is safe to free it
  // there can still be read / write locks on leafprev but we dont care about that

  // first, wait for non-checking readers to complete
  reader_count_wait(plant);

  // block readers at leafprev - rlease chain lock readlock on leafprev and grab writelock
  if (leafprev != plant) chain_lock_upgrade(leafprev, 1, 0);

  // adjust the link
  leafprev->next = leaf->next;

  // and that is it, job done. Now we can free leaf at leisure
  plant->flags &= ~WEED_FLAG_OP_DELETE;
  chain_lock_unlock(plant);

  if (leafprev != leaf && leafprev != plant) chain_lock_unlock(leafprev);
  structure_mutex_unlock(plant);

  // get a chain_lock write link on leaf, once we have this, all readers have moved to the next
  // leaf, unless they are reading / wiritng to this leaf, and we are almost done
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
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
#ifdef ENABLE_PROXIES
  leaf->flags & WEED_FLAG_PROXY return_unlock(leaf, WEED_EEROR_IMMUTABLE);
#endif
  // block here, flags are important
  data_lock_upgrade(leaf, 1);

  leaf->flags = (leaf->flags & (WEED_FLAGBITS_RESERVED | WEED_FLAG_PROXY))
    | (flags & ~(WEED_FLAGBITS_RESERVED | WEED_FLAG_PROXY));

  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *plant, const char *key, void *data) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  return_unlock(leaf, WEED_ERROR_CONCURRENCY);
}

static weed_plant_t *_weed_plant_new(int32_t plant_type) {
  weed_leaf_t *leaf = weed_leaf_new(WEED_LEAF_TYPE, WEED_SEED_INT, WEED_MAGIC_HASH);
  if (!leaf) return NULL;
  leaf->data = weed_data_new(WEED_SEED_INT, 1, &plant_type);
  if (!leaf->data) {
    data_lock_writelock(leaf);
    return weed_leaf_free(leaf);
  }
  leaf->num_elements = 1;
  leaf->flags = WEED_FLAG_IMMUTABLE;
  return leaf;
}

static char **_weed_plant_list_leaves(weed_plant_t *plant, weed_size_t *nleaves) {
  // this is an expensive function call as it requires a structure_mutex lock to be held
  // must use normal malloc, strdup here since the caller will free the strings
  weed_leaf_t *leaf = plant;
  char **leaflist;
  int i = 1, j = 0;
  if (nleaves) *nleaves = 0;
  if (!plant) return NULL;

  structure_mutex_lock(plant);

  for (; leaf; i++) leaf = leaf->next;
  if (!(leaflist = (char **)malloc(i * sizeof(char *)))) {
    structure_mutex_unlock(plant);
    return NULL;
  }
  for (leaf = plant; leaf; leaf = leaf->next) {
    leaflist[j++] = strdup(weed_leaf_get_key(leaf));
    if (!leaflist[j - 1]) {
      structure_mutex_unlock(plant);
      for (--j; j > 0; free(leaflist[--j]));
      free(leaflist);
      return NULL;
    }}
  structure_mutex_unlock(plant);
  leaflist[j] = NULL;
  if (nleaves) *nleaves = j;
  return leaflist;
}

static weed_error_t _weed_leaf_set_or_append(int append, weed_plant_t *plant, const char *key,
					  uint32_t seed_type, weed_size_t num_elems,
					     weed_voidptr_t values) {
  weed_data_t **data = NULL, **old_data = NULL;
  weed_leaf_t *leaf = NULL, *refleaf = NULL;
  weed_hash_t hash = 0;
  weed_size_t old_num_elems = 0;
  int isnew = 0;

  if (!plant) return WEED_ERROR_NOSUCH_LEAF;

  if (!WEED_SEED_IS_VALID(seed_type)) return WEED_ERROR_WRONG_SEED_TYPE;

  if (!append) {
    // prepare the data first, so that locking is as brief as possible
    if (num_elems) {
      data = (weed_data_t **)weed_data_new(seed_type, num_elems, values);
      if (!data) return WEED_ERROR_MEMORY_ALLOCATION;
    }
  }
  // To keep locking as brief as possble, we check in 2 passes. pass 1 we scan the chain like a reader,
  // (refleaf == NULL)
  // but make a note of the leaf directly after the plant (refleasf).
  // If we find the target we will have a data readlock
  // on the leaf which we will upgrade to a writelock. Since we hold the data lock on it, it cannot be deleted
  // while we wait.
  // If the leaf is not found on the first pass, we then obtain s write lock on the plant which
  // prevents new leaves from being added. We need only scan as far as the reference node, since if the leaf
  // were adding during the first pass, it would have been inserted between the plant and the reference leaf.
  // If on pass one, there was only the plant, this is set in refleaf, and since we only check for refleaf AFTER the plant
  // we willscan all leaves (shich must hav been appended since we checked).
  //
  // If we find the leaf on the second pass we will again, have a data readlock on it
  // and we unlock the plant, then proceed as
  // normal to get a data writelock. If we do not find it on pass 2, then since we already hold a write lock on
  // the plant, we simply append the new leaf after the plant, and release the plant lock.
  //
  // Thus we only lock out readers on the leaf which we are about to update, OR we lock out other writers
  // only during the brief time it takes to do the second scan (from plant to refleaf).
  // There is a small possibility that a deletion thread is wating to delete our reference node,
  // and by the time we obtain the plant write lock it will no longer be present.
  // This means simply that we will scan the entire chain with writelock held, however this is no worse
  // than if we held the writelock from the start, and cannot be avoided without causing potential deadlocks
  // with deletion threads.
  // Since the chance of this is smaller than 1., this is still an improvmeent.
  // This could be avoided by keeping a data readlock on the reference leaf, but this could end up
  // prejudicing deletion threads in the case where the chain is long and there are many writers.
  //
  // To further optimise, the new data is prepared before we even start the first scan. We either switch it with
  // the old data, or else insert it.
  leaf = weed_find_leaf(plant, key, &hash, &refleaf);
  if (!leaf) {
    if (!refleaf) refleaf = plant;
    chain_lock_upgrade(plant, 0, 0);
    // search only until we reach refnode (if refleaf is plant, we check all)
    if (!(leaf = weed_find_leaf(plant, key, &hash, &refleaf))) {
      if (!(leaf = weed_leaf_new(key, seed_type, hash))) {
	chain_lock_unlock(plant);
	if (data) weed_data_free(data, num_elems, num_elems, seed_type);
	return WEED_ERROR_MEMORY_ALLOCATION;
      }
      isnew = 1;
      if (append) {
	append = 0;
	if (num_elems) {
	  data = (weed_data_t **)weed_data_new(seed_type, num_elems, values);
	  if (!data) {
	    chain_lock_unlock(plant);
	    return WEED_ERROR_MEMORY_ALLOCATION;
	  }
	}
      }
    }
    // we have a readlock on the leaf so we can release the plant lock
    else chain_lock_unlock(plant);
  }
  if (!isnew) {
    // we hold a readlock on the leaf, we will now grab the mutex and upgrade to a writelock
    // if we fail, too bad the other thread has priority
#ifdef ALLOW_PROXIES
    if (leaf->flags & WEED_FLAG_PROXY) {
      if (data) weed_data_free(data, num_elems, num_elems, seed_type);
      return_unlock(leaf, WEED_ERROR_IMMUTABLE);
    }
#endif
    if (seed_type != leaf->seed_type) {
      if (data) weed_data_free(data, num_elems, num_elems, seed_type);
      return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
    }
    if (leaf->flags & WEED_FLAG_IMMUTABLE) {
      if (data) weed_data_free(data, num_elems, num_elems, seed_type);
      return_unlock(leaf, WEED_ERROR_IMMUTABLE);
    }

    if (leaf == plant && (append || num_elems != 1)) {
      if (data) weed_data_free(data, num_elems, num_elems, seed_type);
      return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);
    }
    // another thread is updating the same leaf concurrently
    if (data_lock_upgrade(leaf, 0)) {
      if (data) weed_data_free(data, num_elems, num_elems, seed_type);
      return_unlock(leaf, WEED_ERROR_CONCURRENCY);
    }
    old_data = leaf->data;
    old_num_elems = leaf->num_elements;
  }
  if (append) {
    data = (weed_data_t **)weed_data_append(leaf, num_elems, values);
    if (!data) return WEED_ERROR_MEMORY_ALLOCATION;
    if (old_data != data) weed_free(old_data);
    old_data = NULL;
    num_elems += leaf->num_elements;
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
  
static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key,
				   uint32_t seed_type, weed_size_t num_elements,
                                   weed_voidptr_t values) {
  return _weed_leaf_set_or_append(0, plant, key, seed_type, num_elements, values);
}

static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, weed_size_t idx,
				   weed_voidptr_t value) {
  weed_data_t **data;
  uint32_t type;
  weed_leaf_t *leaf;

  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;

#ifdef ENABLE_PROXIES
  leaf = _get_leaf_proxy(leaf);
#endif

  if (idx >= leaf->num_elements) return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);

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
	if (nullv && data[idx]->size < nullv) *valuecharptrptr = NULL;
	else {
	  size -= nullv;
	  if (size > 0) weed_memcpy(*valuecharptrptr, data[idx]->value.voidptr, size);
	  (*valuecharptrptr)[size] = 0;
	}}
      else weed_memcpy(value, &(data[idx]->value.voidptr), leaf->data[idx]->size);
    }}
  return_unlock(leaf, WEED_SUCCESS);
}

EXPORTED weed_error_t __wbg__(size_t c1, weed_hash_t c2, int c3, weed_plant_t *plant, const char *key,
			      weed_voidptr_t value) {
  if (c1 == _WEED_PADBYTES_ && c2 == WEED_MAGIC_HASH && c3 == 1) return _weed_leaf_get(plant, key, 0, value);
  return WEED_ERROR_NOSUCH_LEAF;
}

static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return 0;
#ifdef ENABLE_PROXIES
  leaf = _get_leaf_proxy(leaf);
#endif
  return_unlock(leaf, leaf->num_elements);
}

static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, weed_size_t idx) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return 0;
#ifdef ENABLE_PROXIES
  leaf = _get_leaf_proxy(leaf);
#endif
  if (idx > leaf->num_elements) return_unlock(leaf, 0);
  return_unlock(leaf, leaf->data[idx]->size);
}

#if WEED_ABI_CHECK_VERSION(202)
static weed_error_t _weed_ext_append_elements(weed_plant_t *plant, const char *key, uint32_t seed_type,
					       weed_size_t nvals, void *data) {
  return _weed_leaf_set_or_append(1, plant, key, seed_type, nvals, data);
}

static weed_error_t _weed_ext_set_element_size(weed_plant_t *plant, const char *key, weed_size_t idx,
					       weed_size_t new_size) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  if (leaf->seed_type != WEED_SEED_VOIDPTR && leaf->seed_type < WEED_SEED_FIRST_CUSTOM)
  if (idx > leaf->num_elements) return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);
  leaf->data[idx]->size = new_size;
  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_ext_attach_leaf(weed_plant_t *src, const char *key, weed_plant_t *dst) {
  weed_leaf_t *leaf, *xleaf;
  weed_error_t err;
  if ((leaf = weed_find_leaf(src, key, NULL, NULL))) return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  if (!(xleaf = weed_find_leaf(dst, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  //
  err = _weed_leaf_set_or_append(0, src, key, WEED_SEED_PLANTPTR, 1, xleaf);
  if (err == WEED_SUCCESS) {
    leaf->num_elements = xleaf->num_elements;
    leaf->seed_type = xleaf->seed_type;
    leaf->flags = WEED_FLAG_PROXY | WEED_FLAG_IMMUTABLE;
  }
  return_unlock(xleaf, err);
}

static weed_error_t _weed_ext_detach_leaf(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  if (!(leaf->flags & WEED_FLAG_PROXY)) return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  leaf->data = NULL;
  leaf->num_elements = 0;
  leaf->flags = 0;
  return_unlock(leaf, WEED_SUCCESS);
}

#endif

static uint32_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_SEED_INVALID;
#ifdef ENABLE_PROXIES
  leaf = _get_leaf_proxy(leaf);
#endif
  return_unlock(leaf, leaf->seed_type);
}

static uint32_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return 0;
  return_unlock(leaf, (uint32_t)(leaf->flags & ~WEED_FLAGBITS_RESERVED));
}

static weed_error_t _weed_leaf_get_private_data(weed_plant_t *plant, const char *key,
						void **data_return) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  return_unlock(leaf, WEED_ERROR_CONCURRENCY);
}

#if WEED_ABI_CHECK_VERSION(202)
static inline size_t _get_leaf_size(weed_plant_t *plant, weed_leaf_t *leaf) {
  weed_size_t ne;
  size_t size = libweed_get_leaf_t_size() + sizeof(leaf_priv_data_t);
  if (leaf == plant) size += sizeof(plant_priv_data_t);
  if (weed_leaf_get_key(leaf) != leaf->padding) size += strlen(leaf->key);
  ne = leaf->num_elements;
  if (leaf->seed_type == WEED_SEED_STRING
      || (leaf->seed_type >= WEED_SEED_FIRST_NON_PTR_TYPE && leaf->seed_type
	  <= WEED_SEED_LAST_NON_PTR_TYPE
	  && weed_seed_get_size(leaf->seed_type, 0) > WEED_VOIDPTR_SIZE))
    for (int i = 0; i < ne; i++) size += leaf->data[i]->size;
  size += ne * (libweed_get_data_t_size());
  return size;
}

EXPORTED size_t weed_leaf_get_byte_size(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf;
  weed_size_t size = 0;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return 0;
  size = _get_leaf_size(plant, leaf);
  return_unlock(leaf, size);
}

EXPORTED size_t weed_plant_get_byte_size(weed_plant_t *plant) {
  // this is an expensive function call as it requires a structure_mutex lock to be held
  size_t psize = 0;
  if (plant) {
    weed_leaf_t *leaf;
    structure_mutex_lock(plant);
    for (leaf = plant; leaf; leaf = leaf->next) psize += _get_leaf_size(plant, leaf);
    structure_mutex_unlock(plant);
  }
  return psize;
}
#endif
