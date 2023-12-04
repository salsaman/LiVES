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

/* (C) G. Finch, 2005 - 2023 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define __LIBWEED__

#define _WEED_ABI_VERSION_MAX_SUPPORTED 203
#define _WEED_ABI_VERSION_MIN_SUPPORTED 200

#ifdef _BUILD_LOCAL_
#include "weed.h"
#else
#include <weed/weed.h>
#endif

#ifdef ENABLE_PROXIES
#define _get_leaf_proxy(leaf) if (leaf && (leaf->flags & WEED_FLAG_PROXY)) { \
    weed_leaf_t *proxy = (weed_leaf_t *)leaf->data[0]->v.value.voidptr;	\
    if (proxy)data_lock_readlock(proxy);data_lock_unlock(leaf);leaf = proxy;}}
#else
#define _get_leaf_proxy(leaf)
#endif

#ifdef WRITER_PREF_AVAILABLE
#undef WRITER_PREF_AVAILABLE
#endif

#if _XOPEN_SOURCE >= 500 || _POSIX_C_SOURCE >= 200809
#define WRITER_PREF_AVAILABLE 1
#define WEED_INIT_PRIV_PREF_WRITERS (1ull << 32)
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
#else
#if __GNUC__ >= 4
#define EXPORTED __attribute__ ((visibility ("default")))
#else
#define EXPORTED
#endif
#endif

#include <pthread.h>

/* explanation of locks and mutexes
   structure_mutex - plant only, locking this prevents leaves from being added or deleted
   except by the lock owner. Used by deletion threads and writer threads running in pass2.
   Some functions which operate on the entire plant (e.g weed_plant-list-leaves) will
   also lock this.

   reader_count - plant only, readers in non-checkmode (see below) increment this before
   traversing the list, and decrement it afterwards. Used by deletion threads to
   ensure all readers are running in checkmode.

   chain_lock - plant and leaves, this rwlock acts like an advisory, readers in checkmode
   will stop before entering a leaf with chain lock writelock. This is used by deletion
   threads to stop readers / writers from entering a leaf which is to be deleted.
   Used by deletion threads, affects readers and writers running in pass1.

   data_lock - threads which want to read the value of a leaf must obtain a data_lock readlock,
   those wanting to change the data must obtain a writelock. Used by reader and writer threads.
   Functions which read or write other values from leaves (e.g. weed_leaf_seed_type) also use this
   - functions which update values, eg. weed_leaf_set_flags, must have a writelock.
*/

typedef struct {
  pthread_rwlock_t	chain_lock;
  pthread_rwlock_t	data_lock;
  pthread_rwlock_t	ref_lock;
} leaf_priv_data_t;

typedef struct {
  leaf_priv_data_t	ldata;
  pthread_rwlock_t	reader_count;
  pthread_mutex_t	structure_mutex;
  weed_leaf_t *		quickptr;
} plant_priv_data_t;


#ifndef HAVE_WEED_HASHFUNC

/* this must be the hash value for WEED_LEAF_TYPE */

#define WEED_MAGIC_HASH 0xB82E802F

#define get16bits(d) (*((const uint16_t *)(d)))

#define HASHROOT 5381
// fast hash from: http://www.azillionmonkeys.com/qed/hash.html
// (c) Paul Hsieh

static weed_hash_t def_weed_hash(const char *key) {
  if (key && *key) {
    size_t len = strlen(key), rem = len & 3;
    weed_hash_t hash = len + HASHROOT, tmp;
    for (len >>= 2; len; len--) {
      hash += get16bits (key);
      tmp = (get16bits (key + 2) << 11) ^ hash;
      hash = (hash << 16) ^ tmp; hash += hash >> 11;
      key += 4;
    }
    switch (rem) {
    case 3: hash += get16bits (key);
      hash ^= hash << 16; hash ^= ((int8_t)key[2]) << 18;
      hash += hash >> 11; break;
    case 2: hash += get16bits (key);
      hash ^= hash << 11; hash += hash >> 17; break;
    case 1: hash += (int8_t)*key;
      hash ^= hash << 10; hash += hash >> 1; break;
    default: break;
    }
    hash ^= hash << 3; hash += hash >> 5; hash ^= hash << 4;
    hash += hash >> 17; hash ^= hash << 25; hash += hash >> 6;
    return hash;
  }
  return 0;
}

#undef get16bits

#define weed_hash def_weed_hash
#endif

#define is_plant(leaf) (leaf->key_hash == WEED_MAGIC_HASH)

#define get_data_lock(leaf) (is_plant(leaf) ?				\
			     &(((plant_priv_data_t *)((leaf)->private_data))->ldata.data_lock) : \
			     &(((leaf_priv_data_t *)((leaf)->private_data))->data_lock))

#define get_chain_lock(leaf) (is_plant(leaf) ?				\
			      &(((plant_priv_data_t *)((leaf)->private_data))->ldata.chain_lock) : \
			      &(((leaf_priv_data_t *)((leaf)->private_data))->chain_lock))

#define get_ref_lock(leaf) (is_plant(leaf) ?				\
			    &(((plant_priv_data_t *)((leaf)->private_data))->ldata.ref_lock) : \
			    &(((leaf_priv_data_t *)((leaf)->private_data))->ref_lock))

#define get_structure_mutex(plant) (&(((plant_priv_data_t *)((plant)->private_data))->structure_mutex))

#define get_count_lock(plant) (&(((plant_priv_data_t *)((plant)->private_data))->reader_count))

#define X_lock_unlock(obj, locktype) do {				\
    if ((obj)) {pthread_rwlock_trywrlock(get_##locktype##_lock((obj)));	\
      pthread_rwlock_unlock(get_##locktype##_lock((obj)));}} while (0)

#define X_lock_unlock_retval(obj, locktype, val)			\
  ((obj) ? (pthread_rwlock_trywrlock(get_##locktype##_lock(obj))	\
	    ? !pthread_rwlock_unlock(get_##locktype##_lock(obj)) ? (val) : (val) \
	    : !pthread_rwlock_unlock(get_##locktype##_lock(obj)) ? (val) : (val)) : 0)

#define X_lock_writelock(obj, locktype) do {				\
    if ((obj)) pthread_rwlock_wrlock(get_##locktype##_lock((obj)));} while (0)

#define X_lock_readlock(obj, locktype) do {					\
    if ((obj)) pthread_rwlock_rdlock(get_##locktype##_lock((obj)));} while (0)

#define X_lock_try_writelock(obj, locktype) ((obj) ? pthread_rwlock_trywrlock(get_##locktype##_lock((obj))) : -1)

#define X_lock_try_readlock(obj, locktype) ((obj) ? pthread_rwlock_tryrdlock(get_##locktype##_lock((obj))) : -1)

#define data_lock_unlock(obj) X_lock_unlock(obj, data)
#define data_lock_unlock_retval(obj, val) X_lock_unlock_retval(obj, data, val)

#define data_lock_writelock(obj) X_lock_writelock(obj, data)

#define data_lock_readlock(obj) X_lock_readlock(obj, data)

#define data_lock_try_writelock(obj) X_lock_try_writelock(obj, data)

#define chain_lock_unlock(obj) X_lock_unlock(obj, chain)

#define chain_lock_writelock(obj) X_lock_writelock(obj, chain)

#define chain_lock_readlock(obj) X_lock_readlock(obj, chain)

#define chain_lock_try_writelock(obj) X_lock_try_writelock(obj, chain)

#define chain_lock_try_readlock(obj) X_lock_try_readlock(obj, chain)

#define ref_lock_unlock(obj) X_lock_unlock(obj, ref)

#define ref_lock_writelock(obj) X_lock_writelock(obj, ref)

#define ref_lock_readlock(obj) X_lock_readlock(obj, ref)

#define ref_lock_try_writelock(obj) X_lock_try_writelock(obj, ref)

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

#define _unlock(obj, val) (data_lock_unlock_retval(obj, val))

#define data_lock_unlock_retval(obj, val) X_lock_unlock_retval(obj, data, val)

static int allbugfixes = 0;
static int debugmode = 0;
static int extrafuncs = 0;
#if WRITER_PREF_AVAILABLE
static int pref_writers = 0;
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
#endif

/* weed--utils functions */
EXPORTED  weed_error_t __wbg__(size_t, weed_hash_t, int, weed_plant_t *, const char *, weed_voidptr_t);
//
#if WEED_ABI_CHECK_VERSION(203)
EXPORTED weed_leaf_t * _weed_intern_freeze(weed_plant_t *, const char *);
EXPORTED weed_error_t _weed_intern_unfreeze(weed_leaf_t *);
EXPORTED weed_seed_t _weed_intern_seed_type(weed_leaf_t *);
EXPORTED weed_size_t _weed_intern_num_elems(weed_leaf_t *);
EXPORTED weed_size_t _weed_intern_elem_size(weed_leaf_t *, weed_size_t idx, weed_error_t *);
EXPORTED weed_size_t _weed_intern_elem_sizes(weed_leaf_t *, weed_size_t *);
EXPORTED weed_error_t _weed_intern_get_all(weed_leaf_t *, weed_voidptr_t rvals);
EXPORTED weed_error_t _weed_intern_leaf_get(weed_leaf_t *, weed_size_t idx, weed_voidptr_t retval);
#endif

static weed_plant_t *_weed_plant_new(int32_t plant_type) GNU_FLATTEN;
static weed_error_t _weed_plant_free(weed_plant_t *) GNU_FLATTEN;
static char **_weed_plant_list_leaves(weed_plant_t *, weed_size_t *nleaves) GNU_FLATTEN;

static weed_error_t _weed_leaf_get(weed_plant_t *, const char *key, weed_size_t idx,
				   weed_voidptr_t value);
static weed_error_t _weed_leaf_set(weed_plant_t *, const char *key, weed_seed_t seed_type,
				   weed_size_t num_elems, weed_voidptr_t values);

static weed_size_t _weed_leaf_num_elements(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_size_t _weed_leaf_element_size(weed_plant_t *, const char *key, weed_size_t idx) GNU_FLATTEN;
static weed_seed_t _weed_leaf_seed_type(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_flags_t _weed_leaf_get_flags(weed_plant_t *, const char *key) GNU_FLATTEN;
static weed_error_t _weed_leaf_delete(weed_plant_t *, const char *key) GNU_FLATTEN;

/* host only functions */
static weed_error_t _weed_leaf_set_flags(weed_plant_t *, const char *key, weed_flags_t flags) GNU_FLATTEN;

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *, const char *key, void *data)
  GNU_FLATTEN;
static weed_error_t _weed_leaf_get_private_data(weed_plant_t *, const char *key, void **data_return)
  GNU_FLATTEN;

#if WEED_ABI_CHECK_VERSION(203)
static weed_error_t _weed_ext_append_elements(weed_plant_t *, const char *key, weed_seed_t seed_type,
					      weed_size_t nvals, void *data) GNU_FLATTEN;

static weed_error_t _weed_ext_recast_seed_type(weed_plant_t *, const char *key, weed_seed_t new_st) GNU_FLATTEN;
static weed_error_t _weed_ext_set_element_size(weed_plant_t *, const char *key, weed_size_t idx,
					       weed_size_t new_size) GNU_FLATTEN;

static weed_error_t _weed_ext_attach_leaf(weed_plant_t *src, const char *key, weed_plant_t *dst)
  GNU_FLATTEN;
static weed_error_t _weed_ext_detach_leaf(weed_plant_t *, const char *key) GNU_FLATTEN;

static weed_error_t _weed_ext_atomic_exchange(weed_plant_t *, const char *key, weed_seed_t seed_type,
					      weed_voidptr_t new_value, weed_voidptr_t old_valptr);

/* internal functions */
static void _weed_ext_detach_leaf_inner(weed_leaf_t *);
#endif

/* internal functions */
static inline weed_leaf_t *weed_find_leaf(weed_plant_t *, const char *key, weed_hash_t *hash_ret,
					  weed_leaf_t **refnode) GNU_FLATTEN GNU_HOT;

static inline weed_leaf_t *weed_leaf_new(const char *key, weed_seed_t seed_type, weed_hash_t hash) GNU_FLATTEN;

static weed_error_t _weed_leaf_set_or_append(int append, weed_plant_t *, const char *key,
					     weed_seed_t seed_type, weed_size_t num_elems,
					     weed_data_t *data, weed_size_t *old_ne,
					     weed_data_t **old_data) GNU_HOT;

static weed_size_t nullv = 1;
static weed_size_t _pptrsize = WEED_PLANTPTR_SIZE;

static inline int weed_strcmp(const char *, const char *) GNU_HOT;
static inline weed_size_t weed_strlen(const char *) GNU_PURE;

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

static inline void *weed_unmalloc_copy_retnull(size_t sz, void *ptr)
{weed_unmalloc_and_copy(sz, ptr); return NULL;}

#define weed_calloc_sizeof(t) weed_calloc(1, sizeof(t))
#define weed_uncalloc_sizeof(t, ptr) weed_unmalloc_and_copy(sizeof(t), ptr)
#define weed_uncalloc_sizeof_retnull(t, ptr) weed_unmalloc_copy_retnull(sizeof(t), ptr)

/* end memfuncs */

#define KEY_IN_SIZE (_WEED_PADBYTES_ ? _WEED_PADBYTES_ + sizeof(char *) : 0)

#define weed_leaf_set_key(leaf, key, size)				\
  ((size < KEY_IN_SIZE ? weed_memcpy((void *)leaf->padding, key, size + 1) \
    : ((leaf->key = weed_malloc_and_copy(size + 1, (void *)key))) ? leaf->key : NULL))

#define weed_leaf_get_key(leaf) ((!_WEED_PADBYTES_ || !leaf->padding[0]) \
				 ? leaf->key : (const char *)leaf->padding)

EXPORTED int libweed_set_memory_funcs(weed_malloc_f my_malloc, weed_free_f my_free, weed_calloc_f my_calloc) {
    weed_malloc = my_malloc;
    if (my_calloc) weed_calloc = my_calloc;
    else my_calloc = _malloc0_product;
    weed_free = my_free;
    weed_malloc_and_copy = _weed_malloc_copy;
    weed_unmalloc_and_copy = _weed_unmalloc_copy;
    return 0;
  }

EXPORTED int libweed_set_slab_funcs(libweed_slab_alloc_clear_f my_slab_alloc0, libweed_slab_unalloc_f my_slab_unalloc,
				    libweed_slab_alloc_and_copy_f my_slab_alloc_and_copy) {
  weed_malloc0 = my_slab_alloc0;
  weed_calloc = _malloc0_product;
  if (my_slab_alloc_and_copy) weed_malloc_and_copy = my_slab_alloc_and_copy;
  else weed_malloc_and_copy = _weed_malloc_copy;
  weed_unmalloc_and_copy = my_slab_unalloc;
  weed_free = _weed_free;
  return 0;
}

EXPORTED size_t libweed_get_leaf_t_size(void) {return sizeof(weed_leaf_t);}
EXPORTED size_t libweed_get_data_t_size(void) {return sizeof(weed_data_t);}

EXPORTED int32_t libweed_get_abi_version(void) {return _abi_;}

EXPORTED int32_t libweed_get_abi_min_supported_version(void) {return _WEED_ABI_VERSION_MAX_SUPPORTED;}
EXPORTED int32_t libweed_get_abi_max_supported_version(void) {return _WEED_ABI_VERSION_MIN_SUPPORTED;}

EXPORTED void libweed_print_init_opts(FILE *out) {
  fprintf(out, "%s\t%s\n", " BIT  0:  WEED_INIT_ALLBUGFIXES	",
	  "Backport all future non-breaking bug fixes "
	  "into the current API version");
  fprintf(out, "%s\t%s\n"," BIT  1: WEED_INIT_DEBUG		",
	  "Run libweed in debug mode.");
  fprintf(out, "%s\t%s\n"," BIT  2: WEED_INIT_EXTENDED_FUNCS	",
	  "Enable host only extended functions.");
#if WRITER_PREF_AVAILABLE
  fprintf(out, "%s\t%s\n", "BIT 32: Prefer writers		",
	  "If functionality permits, can be set to force readers to block if there is a writer waiting "
	  "to update data. This is useful in applications where there are many more writers than readers.\n"
	  "Not recommended in situations where there may also be many concurrent deletions,\n"
	  "as the deletion threads may effectively become blocked.\n");
#endif
  fprintf(out, "%s\t%s\n","BIT 33: Skip errchecks		",
	  "Optimise performance by skipping unnecessary edge case eror checks.");
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
  if (init_flags & WEED_INIT_PRIV_PREF_WRITERS) pref_writers = 1;
  else pref_writers = 0;
#endif
  if (init_flags & WEED_INIT_PRIV_SKIP_ERRCHECKS) skip_errchecks = 1;
  else skip_errchecks = 0;

  if (_abi_ < 201 && !allbugfixes) nullv = 0;
  if (_abi_ < 202 && !allbugfixes) _pptrsize = WEED_SEED_VOIDPTR;

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

#if WEED_ABI_CHECK_VERSION(203)
  // host only extended
  if (extrafuncs) {
    weed_ext_attach_leaf = _weed_ext_attach_leaf;
    weed_ext_detach_leaf = _weed_ext_detach_leaf;
    weed_ext_set_element_size = _weed_ext_set_element_size;
    weed_ext_append_elements = _weed_ext_append_elements;
    weed_ext_atomic_exchange = _weed_ext_atomic_exchange;
    weed_ext_recast_seed_type = _weed_ext_recast_seed_type;
   }
#endif
  return WEED_SUCCESS;
}

#define weed_strlen(s) ((s) ? strlen((s)) + nullv : 0)
#define weed_strcmp(s1, s2) ((!(s1) || !(s2)) ? (s1 != s2) : strcmp(s1, s2))

#define weed_seed_is_ptr(seed_type) ((seed_type) >= WEED_SEED_FIRST_PTR_TYPE ? 1 : 0)

#if WEED_ABI_CHECK_VERSION(202)
#define _vs(a) a
#else
#define _vs(a) WEED_VOIDPTR_SIZE
#endif

// internal data size
#define weed_seed_get_size(seed_type, size)				\
  (seed_type == WEED_SEED_STRING ? size					\
   : seed_type == WEED_SEED_PLANTPTR ? _pptrsize			\
   : (weed_seed_is_ptr(seed_type)) ? _vs(size)				\
   : (seed_type == WEED_SEED_INT || seed_type == WEED_SEED_UINT) ? 4	\
   : seed_type == WEED_SEED_BOOLEAN ? 1					\
   : (seed_type == WEED_SEED_DOUBLE || seed_type == WEED_SEED_FLOAT	\
      || seed_type == WEED_SEED_INT64 || seed_type == WEED_SEED_UINT64) ? 8 \
   : 0)

// external data size
#define weed_seed_get_offset(seed_type)					\
  (seed_type == WEED_SEED_BOOLEAN ? sizeof(weed_boolean_t) : weed_seed_get_size(seed_type, WEED_VOIDPTR_SIZE))

//#undef _vs

static inline void *weed_data_free(weed_data_t *data, weed_size_t num_valid_elems,
				   weed_size_t num_elems, weed_seed_t seed_type) {
  int is_nonptr = (seed_type > 0 && seed_type <= WEED_SEED_LAST_NON_PTR_TYPE);
  int xnullv = 0, minsize = WEED_VOIDPTR_SIZE;
  if (seed_type == WEED_SEED_STRING) minsize = xnullv = nullv;
  for (weed_size_t i = 0; i < num_valid_elems; i++)
    if (is_nonptr && data[i].size > minsize && data[i].v.value)
      weed_unmalloc_and_copy(data[i].size - xnullv, data[i].v.value);
  weed_free(data);
  return NULL;
}

static inline weed_data_t *weed_data_new(weed_seed_t seed_type, weed_size_t num_elems,
					  weed_voidptr_t values) {
  weed_data_t *data;
  if (!num_elems) return NULL;
  // for better performance, we allocate a block of memory to hold all data structs, ie
  // num_elements * sizeof(weed_data_t). This ensures all the values re localised when being fetched
  if (!(data = (weed_data_t *)weed_calloc(num_elems, sizeof(weed_data_t)))) return NULL;

  if (seed_type == WEED_SEED_STRING) {
    char **valuec = (char **)values;
    for (int i = 0; i < num_elems; i++)
      data[i].v.value = valuec ?
	(weed_voidptr_t)(((data[i].size = weed_strlen(valuec[i])) > nullv) ?
			 (weed_voidptr_t)weed_malloc_and_copy(data[i].size - nullv,
							      valuec[i]) : NULL) : NULL;
  }
  else {
    int is_ptr = (weed_seed_is_ptr(seed_type));
    weed_size_t esize = weed_seed_get_size(seed_type, 0);
    if (is_ptr) {
      weed_voidptr_t *valuep = (weed_voidptr_t *)values;
      for (int i = 0; i < num_elems; i++) {
	data[i].size = esize;
	data[i].v.value = valuep ? valuep[i] : NULL;
      }
    }
    else {
      int off_size = weed_seed_get_offset(seed_type);
      if (esize <= WEED_VOIDPTR_SIZE)
	for (int i = 0; i < num_elems; i++) {
	  data[i].size = esize;
	  weed_memcpy(&data[i].v.storage, (char *)values + i * off_size, data[i].size);
	}
      else for (int i = 0; i < num_elems; i++) {
	  data[i].size = esize;
	  data[i].v.value = (weed_voidptr_t)(weed_malloc_and_copy(data[i].size,
								  (char *)values + i * off_size));
	  if (!data[i].v.value && data[i].size > nullv)
	    return weed_data_free(data, --i, num_elems, seed_type);
	}
    }
  }
  return data;
}

static inline weed_data_t *weed_data_append(weed_leaf_t *leaf,
					    weed_size_t new_elems, weed_data_t *add_data) {
  if (!new_elems) return leaf->data;
  weed_data_t *new_data;
  weed_size_t old_elems = leaf->num_elements;
  weed_size_t old_size = old_elems * sizeof(weed_data_t);
  weed_size_t new_size = new_elems * sizeof(weed_data_t);
  new_elems += old_elems;
  if (!(new_data = (weed_data_t *)weed_calloc(new_elems, sizeof(weed_data_t)))) return NULL;
  weed_memcpy(new_data, leaf->data, old_size);
  weed_memcpy(new_data + old_size, add_data, new_size);
  return new_data;
}

static inline weed_leaf_t *weed_find_leaf(weed_plant_t *plant, const char *key, weed_hash_t *hash_ret,
					  weed_leaf_t **refnode) {
  weed_hash_t hash = 0;
  weed_leaf_t *leaf = plant, *chain_leaf = NULL, *refleaf = NULL;
  int is_writer = 1, checkmode = 0;

  if (!plant) return NULL;

  if (!key || !*key) {
    hash = WEED_MAGIC_HASH;
    if (hash_ret) *hash_ret = hash;
    data_lock_readlock(leaf);
    return leaf;
  }

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
      data_lock_readlock(plant);
      data_lock_unlock(plant);
    }
    else chain_lock_unlock(plant);

    // this counts the number of readers running in non-check mode
    if (!checkmode) reader_count_add(plant);
  }

  if (hash_ret) hash = *hash_ret;
  if (!hash) hash = weed_hash(key);
  if (!checkmode && !refnode) {
    leaf = ((plant_priv_data_t *)plant->private_data)->quickptr;
    if (!leaf || hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key))) {
      leaf = plant;
      while (hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key)))
	if (!(leaf = leaf->next)) break;
    }
  }
  else {
    while (hash != leaf->key_hash || (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf), (char *)key))) {
      if (!(leaf = leaf->next)) break;
      if (refnode) {
	if (*refnode) {
	  if (leaf == *refnode) return NULL;
	  continue;
	}
	refleaf = leaf;
	ref_lock_readlock(refleaf);
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
      }}
  }
  if (!is_writer) {
    // checkmode (and by extension chain_leaf) can only possibly be non-zero if ! is_writer
    if (checkmode) chain_lock_unlock(chain_leaf);
    else reader_count_sub(plant);
  }
  if (hash_ret) *hash_ret = hash;
  if (leaf) {
    if (refleaf) ref_lock_unlock(refleaf);
    data_lock_readlock(leaf);
  }
  else if (refnode && !*refnode && refleaf) *refnode = refleaf;
  return leaf;
}

static inline void *weed_leaf_free(weed_leaf_t *leaf) {
  data_lock_writelock(leaf);
  data_lock_unlock(leaf);
  if (leaf->data)
    weed_data_free((void *)leaf->data, leaf->num_elements, leaf->num_elements, leaf->seed_type);
  if (!_WEED_PADBYTES_ || !*leaf->padding)
    weed_unmalloc_and_copy(strlen(leaf->key + 1) + 2, (void *)leaf->key);
  if (is_plant(leaf)) {
    plant_priv_data_t *pdata = (plant_priv_data_t *)leaf->private_data;
    weed_uncalloc_sizeof(plant_priv_data_t, pdata);
  }
  else {
    leaf_priv_data_t *ldata = (leaf_priv_data_t *)leaf->private_data;
    weed_uncalloc_sizeof(leaf_priv_data_t, ldata);
  }
  return weed_uncalloc_sizeof_retnull(weed_leaf_t, leaf);
}

static inline weed_leaf_t *weed_leaf_new(const char *key, weed_seed_t seed_type, weed_hash_t hash) {
  pthread_rwlockattr_t *rwattrp = NULL;
#if WRITER_PREF_AVAILABLE
  pthread_rwlockattr_t rwattr;
#endif
  const char *xkey;
  weed_leaf_t *leaf = weed_calloc_sizeof(weed_leaf_t);
  if (!leaf) return NULL;
  xkey = weed_leaf_set_key(leaf, key, strlen(key));
  if (!xkey) return weed_uncalloc_sizeof_retnull(weed_leaf_t, leaf);
  leaf->key_hash = hash;
  leaf->seed_type = seed_type;

#if WRITER_PREF_AVAILABLE
  rwattrp = &rwattr;
  pthread_rwlockattr_init(rwattrp);
  if (pref_writers)
    pthread_rwlockattr_setkind_np(rwattrp, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

  if (is_plant(leaf)) {
    plant_priv_data_t *pdata = weed_calloc_sizeof(plant_priv_data_t);
    if (!pdata) {
      if (weed_leaf_get_key(leaf) != leaf->padding)
	weed_unmalloc_and_copy(strlen(leaf->key + 1) + 2, (void *)leaf->key);
      return weed_uncalloc_sizeof_retnull(weed_leaf_t, leaf);}
    pthread_rwlock_init(&pdata->ldata.data_lock, rwattrp);
    pthread_rwlock_init(&pdata->ldata.chain_lock, rwattrp);
    pthread_rwlock_init(&pdata->ldata.ref_lock, rwattrp);
    pthread_mutex_init(&pdata->structure_mutex, NULL);
    pthread_rwlock_init(&pdata->reader_count, NULL);
    leaf->private_data = (void *)pdata;
  }
  else {
    leaf_priv_data_t *ldata = weed_calloc_sizeof(leaf_priv_data_t);
    if (!ldata) {
      if (weed_leaf_get_key(leaf) != leaf->padding)
	weed_unmalloc_and_copy(strlen(leaf->key + 1) + 2, (void *)leaf->key);
      return weed_uncalloc_sizeof_retnull(weed_leaf_t, leaf);}
    pthread_rwlock_init(&ldata->chain_lock, rwattrp);
    pthread_rwlock_init(&ldata->data_lock, rwattrp);
    pthread_rwlock_init(&ldata->ref_lock, rwattrp);
    leaf->private_data = (void *)ldata;
  }
  return leaf;
}

static inline weed_error_t weed_leaf_append(weed_plant_t *plant, weed_leaf_t *newleaf) {
  // has to be done atomiccally
  newleaf->next = plant->next;
  plant->next = newleaf;
  return WEED_SUCCESS;
}

static weed_error_t _weed_plant_free(weed_plant_t *plant) {
  weed_leaf_t *leaf, *leafprev = plant, *leafnext;
  if (!plant) return WEED_SUCCESS;

  if (plant->flags & WEED_FLAG_UNDELETABLE) return WEED_ERROR_UNDELETABLE;

  structure_mutex_lock(plant);
  plant->flags |= WEED_FLAG_OP_DELETE;
  chain_lock_writelock(plant);

  reader_count_wait(plant);

  ((plant_priv_data_t *)plant->private_data)->quickptr = NULL;

  /// hold on to structure_mutex until we are done
  leafnext = plant->next;
  while ((leaf = leafnext)) {
    leafnext = leaf->next;
    chain_lock_writelock(leaf);
    if (leaf->flags & WEED_FLAG_UNDELETABLE) leafprev = leaf;
    else {
      leafprev->next = leafnext;
      data_lock_writelock(leaf);
      chain_lock_unlock(leaf);
      data_lock_unlock(leaf);
      weed_leaf_free(leaf);
    }
  }

  if (leafprev == plant) {
    // remove lock temporarily just in case other threads were trying to grab a read lock
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);
    structure_mutex_lock(plant);
    plant->flags |= WEED_FLAG_OP_DELETE;
    chain_lock_writelock(plant);
    reader_count_wait(plant);
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);
    weed_leaf_free(plant);
    return WEED_SUCCESS;
  }
  plant->flags &= ~WEED_FLAG_OP_DELETE;
  for (leaf = plant; leaf; leaf = leaf->next) chain_lock_unlock(leaf);
  structure_mutex_unlock(plant);
  return WEED_ERROR_UNDELETABLE;
}

static weed_error_t _weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = plant, *leafprev = leaf;
  weed_hash_t hash = key && *key ? weed_hash(key) : WEED_MAGIC_HASH;
  weed_error_t err = WEED_SUCCESS;

  if (!plant) return err;

  // A)
  // get structure mutex, this locks out other deleters
  structure_mutex_lock(plant);

  // B)
  // this will begin forcing readers into checkmode
  // it will alos lock out any setters
  plant->flags |= WEED_FLAG_OP_DELETE;
  chain_lock_writelock(plant);

  for (leaf = plant; leaf && (leaf->key_hash != hash ||
			      (!skip_errchecks && weed_strcmp(weed_leaf_get_key(leaf),
							      (char *)key))); leaf = leaf->next) leafprev = leaf;
  if (!leaf || leaf == plant) err = WEED_ERROR_NOSUCH_LEAF;
  else {
#ifdef ENABLE_PROXIES
    if (leaf->flags & WEED_FLAG_PROXY) err = _weed_ext_detatch_leaf_inner(leaf);
#endif
    if (leaf->flags & WEED_FLAG_UNDELETABLE) err = WEED_ERROR_UNDELETABLE;
  }

  if (err != WEED_SUCCESS) {
    chain_lock_unlock(plant);
    structure_mutex_unlock(plant);
    return err;
  }

  if (leafprev != plant) chain_lock_writelock(leafprev);
  else data_lock_writelock(plant);

  if (((plant_priv_data_t *)plant->private_data)->quickptr == leaf)
    ((plant_priv_data_t *)plant->private_data)->quickptr = NULL;

  // Wait for teaders in checkmode to finish
  // any remaining readers will be held at leafprev
  reader_count_wait(plant);

  // adjust the link
  leafprev->next = leaf->next;

  if (leafprev == plant) data_lock_unlock(plant);

  // and that is it, job done. Now we can free leaf at leisure
  plant->flags &= ~WEED_FLAG_OP_DELETE;

  if (leafprev != plant) chain_lock_unlock(leafprev);
  chain_lock_unlock(plant);
  structure_mutex_unlock(plant);

  if (ref_lock_try_writelock(leaf)) leaf->flags |= WEED_FLAG_OP_DELETE;
  else {
    ref_lock_unlock(leaf);
    weed_leaf_free(leaf);
  }
  return WEED_SUCCESS;
}

static weed_error_t _weed_leaf_set_flags(weed_plant_t *plant, const char *key, weed_flags_t flags) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  // strip any reserved bits from flags
  if (!leaf) return WEED_ERROR_NOSUCH_LEAF;
#ifdef ENABLE_PROXIES
  if (leaf->flags & WEED_FLAG_PROXY) return_unlock(leaf, WEED_EEROR_IMMUTABLE);
#endif
  if (leaf != plant) {
    ref_lock_readlock(leaf);
    data_lock_unlock(leaf);
    data_lock_writelock(leaf);
    ref_lock_unlock(leaf);
    if (leaf->flags & WEED_FLAG_OP_DELETE) {
      if (!ref_lock_try_writelock(leaf)) {
	ref_lock_unlock(leaf);
	weed_leaf_free(leaf);
      }
      return_unlock(leaf, WEED_ERROR_CONCURRENCY);
    }
  }
  //
  leaf->flags = (leaf->flags & (WEED_FLAGBITS_RESERVED | WEED_FLAG_PROXY))
    | (flags & ~(WEED_FLAGBITS_RESERVED | WEED_FLAG_PROXY));
  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_leaf_set_private_data(weed_plant_t *plant, const char *key, void *data) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  return leaf ? _unlock(leaf, WEED_ERROR_CONCURRENCY) : WEED_ERROR_NOSUCH_LEAF;
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

static weed_error_t _weed_data_get(weed_data_t *data, uint32_t type, weed_size_t idx,
				   weed_voidptr_t value) {
  if (0 && type == WEED_SEED_FUNCPTR) *((weed_funcptr_t *)value) = 0;//data[idx].funcval;
  else {
    if (weed_seed_is_ptr(type)) *((weed_voidptr_t *)value) = data[idx].v.value;
    else {
      if (type == WEED_SEED_STRING) {
	weed_size_t size = (size_t)data[idx].size;
	char **valuecharptrptr = (char **)value;
	if (nullv && data[idx].size < nullv) *valuecharptrptr = NULL;
	else {
	  size -= nullv;
	  if (size > 0) weed_memcpy(*valuecharptrptr, data[idx].v.value, size);
	  (*valuecharptrptr)[size] = 0;
	}}
      else weed_memcpy(value, &(data[idx].v.storage), data[idx].size);
    }}
  return WEED_SUCCESS;
}

#if WEED_ABI_CHECK_VERSION(203)
static weed_error_t _weed_data_get_all(weed_leaf_t *leaf, weed_voidptr_t rvals) {
  weed_size_t ne = leaf->num_elements;
  weed_seed_t type = leaf->seed_type;
  if (weed_seed_is_ptr(type)) {
    for (int i = 0; i < ne; i++)
      (((weed_voidptr_t *)rvals))[i] = leaf->data[i].v.value;}
  else {
    if (type == WEED_SEED_STRING) {
      for (int i = 0; i < ne; i++) {
	weed_size_t size = (size_t)leaf->data[i].size;
	if (nullv && leaf->data[i].size < nullv) ((char **)rvals)[i] = NULL;
	else {
	  size -= nullv;
	  if (size > 0) weed_memcpy(((char **)rvals)[i], leaf->data[i].v.value, size);
	  ((char **)rvals)[i][size] = 0;}}}
    else {
      weed_size_t esz = weed_seed_get_size(leaf->seed_type, sizeof(char *));
      int osz = weed_seed_get_offset(leaf->seed_type);
      for (int i = 0; i < ne; i++)
	weed_memcpy(rvals + i * osz, &(leaf->data[i].v.storage), esz);}}
  return WEED_SUCCESS;
}
#endif

static weed_error_t _weed_leaf_set_or_append(int append, weed_plant_t *plant, const char *key,
					     weed_seed_t seed_type, weed_size_t num_elems,
					     weed_data_t *data, weed_size_t *old_ne, weed_data_t **old_ret) {
  weed_data_t *old_data = NULL;
  weed_leaf_t *leaf = NULL, *refleaf = NULL;
  weed_hash_t hash = 0;
  weed_size_t old_num_elems = 0;
  weed_error_t err = WEED_SUCCESS;
  int isnew = 0;

  if (!plant) return WEED_ERROR_NOSUCH_LEAF;
  if (!WEED_SEED_IS_VALID(seed_type)) return WEED_ERROR_WRONG_SEED_TYPE;

  // pass 1, try to find leaf. If we fail, we note the leaf following plant (refleaf)
  // if no leaves follow plant, refleaf will be NULL and we set it to plant (so it will never be located)
  // we also get a reflock readlock on refleaf, this temporarily stops deletion threads from deleting it,
  // instead they will unlink it and flag it with OP_DELETE
  if (!(leaf = weed_find_leaf(plant, key, &hash, &refleaf))) {
    if (!refleaf) refleaf = plant;

    // pass 2, we put reflock writelock on plant. This stops other setter threads from adding leafs before refleaf
    // we check only the part of the plant up to refleaf, since if leaf had been added already it would have been
    // appended after plant, but before refleaf
    // leaves can be deleted.

    // this lock is need so that other writers don't try to dd the same leaf
    // we will add it, then the following writer will find it before its own refleaf and update the value
    // rather than adding a duplicate. Addtionally we cannot have to threads both updating the next pointer for the plant
    // else we would lose one of the two leaves
    ref_lock_writelock(plant);

    // search the start of plant for target in case it was newly added, stopping if we hit refleaf
    // if we find leaf we get a data_lock read lock on it
    // otherwise we are going to add leaf
    if (!(leaf = weed_find_leaf(plant, key, &hash, &refleaf))) {
      if (refleaf != plant) {
	ref_lock_unlock(refleaf);
	if (refleaf->flags & WEED_FLAG_OP_DELETE) {
	  if (!ref_lock_try_writelock(refleaf)) {
	    ref_lock_unlock(refleaf);
	    weed_leaf_free(refleaf);
	  }}}
      if (!(leaf = weed_leaf_new(key, seed_type, hash))) {
	ref_lock_unlock(plant);
	return WEED_ERROR_MEMORY_ALLOCATION;
      }
      leaf->data = data;
      leaf->num_elements = num_elems;
      weed_leaf_append(plant, leaf);
      // now we have appended the leaf, other setters may continue
      ref_lock_unlock(plant);
      return WEED_SUCCESS;
    }
    // if we did not find leaf, we can unlock ref_lock on plant - we don't care if other threads
    // add leaves ahead of us
    ref_lock_unlock(plant);
  }

  // we put a ref_lock read_lock on it
  // then get a data_lock writelock
  // if by any chance a deletion thread acts between dropping the data_lock readloock and writelock, with the reflock
  // on it will not delete the leaf  but flag it as op_delete
  // after getting the data_lock writelock, we check for this and maybe delete the leaf

  ref_lock_readlock(leaf);
  data_lock_unlock(leaf);
  data_lock_writelock(leaf);
  ref_lock_unlock(leaf);

  if (leaf->flags & WEED_FLAG_OP_DELETE) {
    if (ref_lock_try_writelock(leaf)) return_unlock(leaf, WEED_ERROR_CONCURRENCY);
    ref_lock_unlock(leaf);
    data_lock_unlock(leaf);
    weed_leaf_free(leaf);
    return WEED_ERROR_CONCURRENCY;
  }

  // with data lock writelock, we can update the leaf and we are done

  if (!isnew) {
#ifdef ALLOW_PROXIES
    if (leaf->flags & WEED_FLAG_PROXY)
      return_unlock(leaf, WEED_ERROR_IMMUTABLE);
#endif
    if (seed_type != leaf->seed_type)
      return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);

    if (leaf->flags & WEED_FLAG_IMMUTABLE)
      return_unlock(leaf, WEED_ERROR_IMMUTABLE);

    if (leaf == plant && (append || num_elems != 1))
      return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);

    old_num_elems = leaf->num_elements;
    old_data = leaf->data;
  }
  if (append) {
    data = weed_data_append(leaf, num_elems, data);
    if (!data) return_unlock(leaf, WEED_ERROR_MEMORY_ALLOCATION);
    if (data == old_data) old_data = NULL;
    num_elems += leaf->num_elements;
  }

  leaf->num_elements = num_elems;
  if (num_elems) leaf->data = data;
  else leaf->data = NULL;

  // only now we unlock this, can any potential deleter delete this leaf
  data_lock_unlock(leaf);
  if (old_ret) {
    *old_ret = old_data;
    *old_ne = old_num_elems;
  }
  else weed_data_free(old_data, old_num_elems, old_num_elems, seed_type);
  return err;
}

static weed_error_t _weed_leaf_set(weed_plant_t *plant, const char *key,
				   weed_seed_t seed_type, weed_size_t num_elements,
				   weed_voidptr_t values) {
  weed_data_t *data = NULL;
  if (!plant) return WEED_ERROR_NOSUCH_LEAF;
  if (!WEED_SEED_IS_VALID(seed_type)) return WEED_ERROR_WRONG_SEED_TYPE;
  if (num_elements)
    if (!(data = weed_data_new(seed_type, num_elements, values)))
      return WEED_ERROR_MEMORY_ALLOCATION;
  weed_error_t err = _weed_leaf_set_or_append(0, plant, key, seed_type, num_elements, data, NULL, NULL);
  if (err != WEED_SUCCESS && data) weed_data_free(data, num_elements, num_elements, seed_type);
  return err;
}

static weed_error_t _weed_leaf_get(weed_plant_t *plant, const char *key, weed_size_t idx,
				   weed_voidptr_t value) {
  weed_error_t err;
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return WEED_ERROR_NOSUCH_LEAF;
  _get_leaf_proxy(leaf);
  if (idx >= leaf->num_elements) return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);
  if (!value) return_unlock(leaf, WEED_SUCCESS);
  err = _weed_data_get(leaf->data, leaf->seed_type, idx, value);
  if (leaf == plant || leaf == plant->next)
    ((plant_priv_data_t *)plant->private_data)->quickptr = NULL;
  else ((plant_priv_data_t *)plant->private_data)->quickptr = leaf;
  return_unlock(leaf, err);
}

EXPORTED weed_error_t __wbg__(size_t c1, weed_hash_t c2, int c3, weed_plant_t *plant, const char *key,
			      weed_voidptr_t value) {
  if (c1 == _WEED_PADBYTES_ && c2 == WEED_MAGIC_HASH && c3 == 1) return _weed_leaf_get(plant, key, 0, value);
  return WEED_ERROR_NOSUCH_LEAF;
}

static weed_size_t _weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return 0;
  _get_leaf_proxy(leaf);
  return_unlock(leaf, leaf->num_elements);
}

static weed_size_t _weed_leaf_element_size(weed_plant_t *plant, const char *key, weed_size_t idx) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return 0;
  _get_leaf_proxy(leaf);
  if (idx > leaf->num_elements) return_unlock(leaf, 0);
  return_unlock(leaf, leaf->data[idx].size);
}

#if WEED_ABI_CHECK_VERSION(203)
static weed_error_t _weed_ext_atomic_exchange(weed_plant_t *plant, const char *key, weed_seed_t seed_type,
					      weed_voidptr_t new_value, weed_voidptr_t old_valptr) {
  weed_data_t *old_data = NULL, *new_data;
  weed_size_t old_ne;
  if (!(new_data = weed_data_new(seed_type, 1, new_value))) return WEED_ERROR_MEMORY_ALLOCATION;
  weed_error_t err = _weed_leaf_set_or_append(0, plant, key, seed_type, 1, new_data, &old_ne, &old_data);
  if (err != WEED_SUCCESS) weed_data_free(new_data, 1, 1, seed_type);
  else {
    if (old_data && old_ne) {
      err = _weed_data_get(old_data, seed_type, 0, old_valptr);
      weed_data_free(old_data, old_ne, old_ne, seed_type);
    }
  }
  return err;
}

static weed_error_t _weed_ext_append_elements(weed_plant_t *plant, const char *key, weed_seed_t seed_type,
					      weed_size_t nvals, void *data) {
  weed_data_t *new_data = NULL;
  if (nvals)
    if (!(new_data = weed_data_new(seed_type, nvals, data)))
      return WEED_ERROR_MEMORY_ALLOCATION;
  weed_error_t err = _weed_leaf_set_or_append(1, plant, key, seed_type, nvals, new_data, NULL, NULL);
  if (err != WEED_SUCCESS && new_data) weed_data_free(new_data, nvals, nvals, seed_type);
  else if (new_data) weed_free(new_data);
  return err;

}

static weed_error_t _weed_ext_set_element_size(weed_plant_t *plant, const char *key, weed_size_t idx,
					       weed_size_t new_size) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return WEED_ERROR_NOSUCH_LEAF;
  if (leaf->seed_type != WEED_SEED_VOIDPTR && leaf->seed_type < WEED_SEED_FIRST_CUSTOM)
    return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  if (idx > leaf->num_elements) return_unlock(leaf, WEED_ERROR_NOSUCH_ELEMENT);
  leaf->data[idx].size = new_size;
  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_ext_recast_seed_type(weed_plant_t *plant, const char *key, weed_seed_t new_st) {
  weed_leaf_t *leaf;
  if (!(leaf = weed_find_leaf(plant, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  if (leaf->seed_type != WEED_SEED_VOIDPTR && leaf->seed_type < WEED_SEED_FIRST_CUSTOM
      && leaf->seed_type != WEED_SEED_STRING)
    return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
 if (new_st < WEED_SEED_FIRST_CUSTOM)
    return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  leaf->seed_type = new_st;
  return_unlock(leaf, WEED_SUCCESS);
}

static weed_error_t _weed_ext_attach_leaf(weed_plant_t *src, const char *key, weed_plant_t *dst) {
  weed_leaf_t *leaf = weed_find_leaf(src, key, NULL, NULL), *xleaf;
  weed_error_t err;
  if (!leaf) return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  if (!(xleaf = weed_find_leaf(dst, key, NULL, NULL))) return WEED_ERROR_NOSUCH_LEAF;
  err = _weed_leaf_set_or_append(0, src, key, WEED_SEED_PLANTPTR, 1, xleaf->data, NULL, NULL);
  if (err == WEED_SUCCESS) {
    leaf->num_elements = xleaf->num_elements;
    leaf->seed_type = xleaf->seed_type;
    leaf->flags = WEED_FLAG_PROXY | WEED_FLAG_IMMUTABLE;
  }
  return_unlock(xleaf, err);
}

static void _weed_ext_detach_leaf_inner(weed_leaf_t *leaf) {
  leaf->data = NULL;
  leaf->num_elements = 0;
  leaf->flags = 0;
}

static weed_error_t _weed_ext_detach_leaf(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return WEED_ERROR_NOSUCH_LEAF;
  if (!(leaf->flags & WEED_FLAG_PROXY)) return_unlock(leaf, WEED_ERROR_WRONG_SEED_TYPE);
  _weed_ext_detach_leaf_inner(leaf);
  return_unlock(leaf, WEED_SUCCESS);
}
#endif

static weed_seed_t _weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return WEED_SEED_INVALID;
  _get_leaf_proxy(leaf);
  return_unlock(leaf, leaf->seed_type);
}

static weed_flags_t _weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return 0;
  return_unlock(leaf, (leaf->flags & ~WEED_FLAGBITS_RESERVED));
}

static weed_error_t _weed_leaf_get_private_data(weed_plant_t *plant, const char *key,
						void **data_return) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return WEED_ERROR_NOSUCH_LEAF;
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
    for (int i = 0; i < ne; i++) size += leaf->data[i].size;
 return size + ne * (libweed_get_data_t_size());
}

EXPORTED size_t weed_leaf_get_byte_size(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return 0;
  weed_size_t size = _get_leaf_size(plant, leaf);
  return_unlock(leaf, size);
}

EXPORTED size_t weed_plant_get_byte_size(weed_plant_t *plant) {
  // this is an expensive function call as it requires a structure_mutex lock to be held
  size_t psize = 0;
  if (plant) {
    structure_mutex_lock(plant);
    for (weed_leaf_t *leaf = plant; leaf; leaf = leaf->next) psize += _get_leaf_size(plant, leaf);
    structure_mutex_unlock(plant);
  }
  return psize;
}
#endif

#if WEED_ABI_CHECK_VERSION(203)
/* internal lib functions for libweed-utils */
EXPORTED weed_leaf_t *_weed_intern_freeze(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf = weed_find_leaf(plant, key, NULL, NULL);
  if (!leaf) return NULL;
  _get_leaf_proxy(leaf);
  return leaf;
}

EXPORTED weed_error_t _weed_intern_unfreeze(weed_leaf_t *leaf)
{return leaf ? _unlock(leaf, WEED_SUCCESS) : WEED_ERROR_NOSUCH_LEAF;}

EXPORTED weed_size_t _weed_intern_num_elems(weed_leaf_t *leaf) {return leaf ? leaf->num_elements : 0;}

EXPORTED weed_seed_t _weed_intern_seed_type(weed_leaf_t *leaf) {return leaf ? leaf->seed_type : 0;}

EXPORTED weed_error_t _weed_intern_leaf_get(weed_leaf_t *leaf, weed_size_t idx, weed_voidptr_t retval) {
  if (idx >= leaf->num_elements) return WEED_ERROR_NOSUCH_ELEMENT;
  if (!retval) return WEED_SUCCESS;
  return _weed_data_get(leaf->data, leaf->seed_type, idx, retval);
}

EXPORTED weed_size_t _weed_intern_elem_sizes(weed_leaf_t *leaf, weed_size_t *sizes) {
  weed_size_t totsize = 0;
  if (leaf) {
    weed_size_t ne = leaf->num_elements;
    if (ne) {
      if (leaf->seed_type != WEED_SEED_STRING) {
	weed_size_t esz = weed_seed_get_offset(leaf->seed_type);
	totsize = ne * esz;
	if (sizes) for (int i = 0; i < ne; i++) sizes[i] = esz;
      }
      else {
	for (int i = 0; i < ne; i++) {
	  weed_size_t xsize = leaf->data[i].size + 1;
	  totsize += xsize;
	  if (sizes) sizes[i] = xsize;
	}
      }}}
  return totsize;
}

EXPORTED weed_size_t _weed_intern_elem_size(weed_leaf_t *leaf, weed_size_t idx, weed_error_t *error) {
  weed_size_t esz = 0;
  weed_error_t err = WEED_SUCCESS;
  if (!leaf) err = WEED_ERROR_NOSUCH_LEAF;
  else {
    if (idx >= leaf->num_elements) err = WEED_ERROR_NOSUCH_ELEMENT;
    else esz = leaf->data[idx].size;
  }
  if (error) *error = err;
  return esz;
}

EXPORTED weed_error_t _weed_intern_get_all(weed_leaf_t *leaf, weed_voidptr_t rvals) {
  return leaf ? _weed_data_get_all(leaf, rvals) : WEED_ERROR_NOSUCH_LEAF;
}
#endif
