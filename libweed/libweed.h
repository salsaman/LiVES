
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

/* (C) G. Finch, 2005 - 2024 */

#ifndef _HAVE_LIBWEED_H_
#define _HAVE_LIBWEED_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define __need_size_t // for malloc, realloc, etc
#define __need_NULL
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

  /* ABI version * 203 */
  // changes in 200 -> 201: weed_leaf_element_size now returns (strlen + 1) for WEED_SEED_STRING values,
  // allowing NULL strings, which return size 0; prior to this, strlen was returned, and NULLS
  // were treated like empty strings.
  // 201 -> 202 :: technical updates (see spec for details)
  // 201 - 202 :: weed_memory_funcs -> libweed_memory_funcs, added optional ext_funct
  //		added (7) mandatory functions for libweed implmentations, made more things #define-able
  // 203 - new baseline version - added weed_ext_* functions, extra symbols

#define WEED_ABI_VERSION 		203

/* Optional #defines:
#define HAVE_WEED_PLANT_T : allows alternate definition for weed_plant_t (default: weed_leaf_t)
#define HAVE_WEED_LEAF_T : allows alternate definition for weed_leaf_t
#define HAVE_WEED_DATA_T : allows alternate definition for weed_data_t (componet of default  weed_leaf_t)
#define HAVE_WEED_HASH_T : allows use of alterntate types (default uint32_t)
#define _CACHELINE_SIZE_ : hardware cacheline size in bytes  (default 64)
*/

#ifndef HAVE_WEED_BOOLEAN_T
#define HAVE_WEED_BOOLEAN_T
typedef bool weed_boolean_t;
#endif
  
#if !defined(WEED_TRUE) || !defined(WEED_FALSE)

#ifdef WEED_TRUE
#undef WEED_TRUE
#endif
#ifdef WEED_FALSE
#undef WEED_FALSE
#endif

#ifdef _WEED_TRUE
#undef _WEED_TRUE
#endif
#ifdef _WEED_FALSE
#undef _WEED_FALSE
#endif

#ifdef __cplusplus
#define WEED_TRUE ((weed_boolean_t)true)
#define WEED_FALSE ((weed_boolean_t)false)
#else

#define _WEED_FALSE	false
#define _WEED_TRUE	true

#define WEED_TRUE (weed_boolean_t)_WEED_TRUE
#define WEED_FALSE (weed_boolean_t)_WEED_FALSE

#endif
#endif

#define WEED_IS_TRUE(expression) ((expression) == WEED_TRUE)
#define WEED_IS_FALSE(expression) ((expression) == WEED_FALSE)

#define WEED_ABI_CHECK_VERSION(version) (WEED_ABI_VERSION  >= version)
#define WEED_API_CHECK_VERSION(version) WEED_ABI_CHECK_VERSION(version)

#ifdef __LIBWEED__
#define  __WEED_FN_DEF__ extern
#define _wbg(a,b,c,d,e) __wbg__(a,b,1,c,d,e)
#else
#ifdef __WEED_HOST__
#define  __WEED_FN_DEF__
#else
#define  __WEED_FN_DEF__ static
#endif
#endif

typedef uint32_t weed_size_t;
typedef int32_t weed_error_t;
typedef void * weed_voidptr_t;
typedef void (*weed_funcptr_t)();
typedef int32_t weed_seed_t;
typedef uint32_t weed_flags_t;

#define weed_seed_type weed_seed_t;
  
#define WEED_VOIDPTR_SIZE	sizeof(weed_voidptr_t)
#define WEED_FUNCPTR_SIZE	sizeof(weed_funcptr_t)
  
#ifndef HAVE_WEED_STORAGE_U
#define HAVE_WEED_STORAGE_U
typedef union _weed_storage_u weed_storage_u;
#ifdef __LIBWEED__
union _weed_storage_u {
  weed_voidptr_t	value;
  weed_funcptr_t	fvalue;
  char storage[WEED_VOIDPTR_SIZE];
};
#endif
#endif

#ifndef HAVE_WEED_DATA_T
#define HAVE_WEED_DATA_T
typedef struct _weed_data weed_data_t;
#ifdef __LIBWEED__
struct _weed_data {
  weed_size_t		size;
  weed_storage_u	v;
};
#endif
#endif

#ifndef  HAVE_WEED_LEAF_T
#define HAVE_WEED_LEAF_T
typedef struct _weed_leaf weed_leaf_t;
#ifndef HAVE_WEED_HASH_T
typedef uint32_t weed_hash_t;
#endif

typedef weed_hash_t (*weed_hash_f)(const char *key, ...);

#ifdef __LIBWEED__
#ifndef _CACHELINE_SIZE_
#define _CACHELINE_SIZE_ 64 /// altering _CACHELINE_SIZE_ requires recompiling libweed
#endif

struct _weed_leaf_nopadding {
  weed_hash_t	key_hash;
  weed_size_t num_elements;
  weed_leaf_t *next, *prev, *quick;
  weed_seed_t seed_type;
  weed_flags_t flags;
  weed_data_t **data;
  void *private_data;
  const char *key;
};

#define _WEED_PADBYTES_ (_CACHELINE_SIZE_-((((int)(sizeof(struct _weed_leaf_nopadding)))%_CACHELINE_SIZE_)))

struct _weed_leaf {
  weed_hash_t	key_hash;
  weed_size_t num_elements;
  weed_leaf_t *next, *prev, *quick;
  weed_seed_t seed_type;
  weed_flags_t flags;
  weed_data_t *data;
  void *private_data;
  char padding[_WEED_PADBYTES_];
  const char *key;
};

#endif
#endif

#ifndef  HAVE_WEED_PLANT_T
#define HAVE_WEED_PLANT_T
typedef weed_leaf_t weed_plant_t;
#endif

typedef weed_plant_t * weed_plantptr_t;

#define WEED_PLANTPTR_SIZE sizeof(weed_plantptr_t)

#define weed_get_leaf_t_size() ((size_t)(libweed_get_leaf_t_size()))
#define weed_get_data_t_size() ((size_t)(libweed_get_data_t_size()))

/// set this flagbit to enable potential backported bugfixes which may
/// theoretically impact existing behaviour
#define WEED_INIT_ALLBUGFIXES			(1<<0)

/// set this to expose extra debug functions
#define WEED_INIT_DEBUGMODE			(1<<1)

/// set this to enable non core "extended" functions
#define WEED_INIT_EXTENDED_FUNCS	       	(1<<2)
  
/* API 203 */
/* "extended" functions - only enabled if WEED_INIT_EXTENDED_FUNCS is passed to libweed_init */
/* functions may be dangerous if not used with caution */
#if defined (__WEED_HOST__) || defined (__LIBWEED__)

/* CAUTION - if existing value is NULL, elements will be appended after the NULL */
typedef weed_error_t (*weed_ext_append_elements_f)(weed_plant_t *, const char *key,
						   weed_seed_t seed_type,
						   weed_size_t num_new_elems,
						   weed_voidptr_t new_values);

/* CAUTION - only works with scalar values */
typedef weed_error_t (*weed_ext_atomic_exchange_f)(weed_plant_t *, const char *key, weed_seed_t seed_type,
						   weed_voidptr_t new_value, weed_voidptr_t old_valptr);
/* extended functions */
__WEED_FN_DEF__ weed_ext_append_elements_f weed_ext_append_elements;
__WEED_FN_DEF__ weed_ext_atomic_exchange_f weed_ext_atomic_exchange;
/*------------------------------*/

#endif

size_t weed_leaf_get_byte_size(weed_plant_t *, const char *key);
size_t weed_plant_get_byte_size(weed_plant_t *);

/* flag bits >= 32 are reserved for library specific features */

int32_t libweed_get_abi_version(void);
int32_t libweed_get_abi_min_supported_version(void);
int32_t libweed_get_abi_max_supported_version(void);

void libweed_print_init_opts(FILE *);

weed_error_t libweed_init(int32_t abi, uint64_t init_flags);

typedef void *(*libweed_malloc_f)(size_t);
typedef void (*libweed_free_f)(void *);
typedef void *(*libweed_memcpy_f)(void *dest, const void *src, size_t);
typedef void *(*libweed_calloc_f)(size_t, size_t);

  int libweed_set_memory_funcs(libweed_malloc_f, libweed_free_f, libweed_memcpy_f, libweed_calloc_f);

typedef void *(*libweed_slab_alloc_clear_f)(size_t);
typedef void *(*libweed_slab_alloc_and_copy_f)(size_t, void *);
typedef void (*libweed_slab_unalloc_f)(size_t, void *);
typedef void (*libweed_unmalloc_and_copy_f)(size_t, void *);

int libweed_set_slab_funcs(libweed_slab_alloc_clear_f, libweed_slab_unalloc_f, libweed_slab_alloc_and_copy_f);

/* deprecated versions - do not use in newly written code */
int weed_set_memory_funcs(libweed_malloc_f, libweed_free_f);

typedef void *(*libweed_slab_alloc_f)(size_t);
int weed_set_slab_funcs(libweed_slab_alloc_f, libweed_slab_unalloc_f,
			libweed_slab_alloc_and_copy_f);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
