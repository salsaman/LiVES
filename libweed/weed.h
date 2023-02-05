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

///////////////// host applications should #include weed-host.h before this header /////////////////////////

#ifndef __WEED_H__
#define __WEED_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define __need_size_t // for malloc, realloc, etc
#define __need_NULL
#include <stddef.h>
#include <inttypes.h>

  /* API / ABI version * 201 */
  // changes in 200 -> 201: weed_leaf_element_size now returns (strlen + 1) for WEED_SEED_STRING values,
  // allowing NULL strings, which return size 0; prior to this, strlen was returned, and NULLS
  // were treated like empty strings.
  // 201 -> 202 :: technical updates (see spec for details)

#define WEED_ABI_VERSION 		202
#define WEED_API_VERSION 		WEED_ABI_VERSION

#define WEED_TRUE	1
#define WEED_FALSE	0

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

#ifndef HAVE_HASHFUNC
  typedef uint32_t weed_hash_t;
#endif

  typedef uint32_t weed_seed_t;

#define weed_seed_type weed_seed_t;
  
#define WEED_VOIDPTR_SIZE	sizeof(weed_voidptr_t)
#define WEED_FUNCPTR_SIZE	sizeof(weed_funcptr_t)

#ifndef HAVE_WEED_DATA_T
#define HAVE_WEED_DATA_T
  typedef struct _weed_data weed_data_t;
#ifdef __LIBWEED__
  struct _weed_data {
    weed_size_t		size;
    union {
      weed_voidptr_t	voidptr;
      weed_funcptr_t	funcptr;
      char storage[WEED_VOIDPTR_SIZE];
    } value;
  };
#endif
#endif

#ifndef  HAVE_WEED_LEAF_T
#define HAVE_WEED_LEAF_T
  typedef struct _weed_leaf weed_leaf_t;
#ifdef __LIBWEED__
#define _CACHE_SIZE_ 64 /// altering _CACHE_SIZE_ requires recompiling libweed

  struct _weed_leaf_nopadding {
    weed_hash_t	key_hash;
    weed_size_t num_elements;
    weed_leaf_t *next;
    weed_seed_t seed_type;
    uint32_t flags;
    weed_data_t **data;
    void *private_data;
    const char *key;
  };

  /* N.B. padbytes are not wasted, they may be used to store key names provided they fit */
  /* as of 202, key is moved to before padding, so the key can be stored in the char *
     + padding */
#define _WEED_PADBYTES_ ((_CACHE_SIZE_-(int)(sizeof(struct _weed_leaf_nopadding)))%_CACHE_SIZE_)

  struct _weed_leaf {
    weed_hash_t	key_hash;
    weed_size_t num_elements;
    weed_leaf_t *next;
    weed_seed_t seed_type;
    uint32_t flags;
    weed_data_t **data;
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

  typedef void *(*weed_malloc_f)(size_t);
  typedef void (*weed_free_f)(void *);
  typedef void *(*weed_memset_f)(void *, int, size_t);
  typedef void *(*weed_memcpy_f)(void *, const void *, size_t);

  /* added in ABI 200 */
  typedef void *(*weed_realloc_f)(void *, size_t);
  typedef void *(*weed_calloc_f)(size_t, size_t);
  typedef void *(*weed_memmove_f)(void *, const void *, size_t);

  typedef weed_plant_t *(*weed_plant_new_f)(int32_t plant_type);
  typedef char **(*weed_plant_list_leaves_f)(weed_plant_t *, weed_size_t *nleaves);
  typedef weed_error_t (*weed_leaf_set_f)(weed_plant_t *, const char *key, weed_seed_t seed_type,
					  weed_size_t num_elems, weed_voidptr_t values);
  typedef weed_error_t (*weed_leaf_get_f)(weed_plant_t *, const char *key, weed_size_t idx,
					  weed_voidptr_t value);
  typedef weed_size_t (*weed_leaf_num_elements_f)(weed_plant_t *, const char *key);
  typedef weed_size_t (*weed_leaf_element_size_f)(weed_plant_t *, const char *key, weed_size_t idx);

  typedef weed_seed_t (*weed_leaf_seed_type_f)(weed_plant_t *, const char *key);
  typedef uint32_t (*weed_leaf_get_flags_f)(weed_plant_t *, const char *key);
  typedef weed_error_t (*weed_plant_free_f)(weed_plant_t *);
  typedef weed_error_t (*weed_leaf_delete_f)(weed_plant_t *, const char *key);

  /* "extended" functions - only enabled if WEED_INIT_EXTENDED_FUNCS is passed to libweed_init */
  /* functions may be dangerous if not used with caution */
#if defined (__WEED_HOST__) || defined (__LIBWEED__)
  /* CAUTION - if existing value is NULL, elements will be appended after the NULL */
  typedef weed_error_t (*weed_ext_append_elements_f)(weed_plant_t *, const char *key,
						     weed_seed_t seed_type,
						     weed_size_t num_new_elems,
						     weed_voidptr_t new_values);

  /* CAUTION - no checking is done to ensure target is still valid */
  typedef weed_error_t (*weed_ext_attach_leaf_f)(weed_plant_t *src, const char *key, weed_plant_t *dst);

  typedef weed_error_t (*weed_ext_detach_leaf_f)(weed_plant_t *, const char *key);

  /* CAUTION - no checking is done to ensure size is correct or target is still valid */
  typedef weed_error_t (*weed_ext_set_element_size_f)(weed_plant_t *, const char *key, weed_size_t idx,
						      weed_size_t new_size);
#endif

  /* end extended functions */

  /////////////////////////////

#if defined (__WEED_HOST__) || defined (__LIBWEED__)
  /* host only functions */

  typedef weed_error_t (*weed_leaf_set_flags_f)(weed_plant_t *, const char *key, uint32_t flags);
  typedef weed_error_t (*weed_leaf_set_private_data_f)(weed_plant_t *, const char *key, void *data);
  typedef weed_error_t (*weed_leaf_get_private_data_f)(weed_plant_t *, const char *key,
						       void **data_return);

  __WEED_FN_DEF__ weed_leaf_set_flags_f weed_leaf_set_flags;
  __WEED_FN_DEF__ weed_leaf_set_private_data_f weed_leaf_set_private_data;
  __WEED_FN_DEF__ weed_leaf_get_private_data_f weed_leaf_get_private_data;

  /* extenended functions */
  __WEED_FN_DEF__ weed_ext_attach_leaf_f  weed_ext_attach_leaf;
  __WEED_FN_DEF__ weed_ext_detach_leaf_f  weed_ext_detach_leaf;
  __WEED_FN_DEF__ weed_ext_set_element_size_f weed_ext_set_element_size;
  __WEED_FN_DEF__ weed_ext_append_elements_f weed_ext_append_elements;
  /*------------------------------*/

#ifndef WITHOUT_LIBWEED  /// functions will be exported from libweed
  
  __WEED_FN_DEF__ size_t weed_leaf_get_byte_size(weed_plant_t *, const char *key);
  __WEED_FN_DEF__ size_t weed_plant_get_byte_size(weed_plant_t *);

  /// set this flagbit to enable potential backported bugfixes which may
  /// theoretically impact existing behaviour
#define WEED_INIT_ALLBUGFIXES			(1<<0)

  /// set this to expose extra debug functions
#define WEED_INIT_DEBUGMODE			(1<<1)

  /// set this to enable non core "extended" functions
#define WEED_INIT_EXTENDED_FUNCS	       	(1<<2)

  /* flag bits >= 32 are reserved for library specific features */

  int32_t libweed_get_abi_version(void);
  int32_t libweed_get_abi_min_supported_version(void);
  int32_t libweed_get_abi_max_supported_version(void);

  void libweed_print_init_opts(FILE *);

  weed_error_t libweed_init(int32_t abi, uint64_t init_flags);
  int libweed_set_memory_funcs(weed_malloc_f, weed_free_f, weed_calloc_f);

  typedef void *(*libweed_slab_alloc_clear_f)(size_t);
  typedef void *(*libweed_slab_alloc_and_copy_f)(size_t, void *);
  typedef void (*libweed_slab_unalloc_f)(size_t, void *);
  typedef void (*libweed_unmalloc_and_copy_f)(size_t, void *);

  int libweed_set_slab_funcs(libweed_slab_alloc_clear_f, libweed_slab_unalloc_f,
			     libweed_slab_alloc_and_copy_f);

#ifdef __LIBWEED__
  // for plugin bootstrap, only relevent for libweed
  __WEED_FN_DEF__ weed_error_t __wbg__(size_t, weed_hash_t, int, weed_plant_t *,
				       const char *,  weed_voidptr_t);
#else
#ifndef _wbg
#define _wbg(...) 2
#endif
#endif

#endif // without libweed
#endif // host only functions

  __WEED_FN_DEF__ weed_leaf_get_f weed_leaf_get;
  __WEED_FN_DEF__ weed_leaf_set_f weed_leaf_set;
  __WEED_FN_DEF__ weed_plant_new_f weed_plant_new;
  __WEED_FN_DEF__ weed_plant_list_leaves_f weed_plant_list_leaves;
  __WEED_FN_DEF__ weed_leaf_num_elements_f weed_leaf_num_elements;
  __WEED_FN_DEF__ weed_leaf_element_size_f weed_leaf_element_size;
  __WEED_FN_DEF__ weed_leaf_seed_type_f weed_leaf_seed_type;
  __WEED_FN_DEF__ weed_leaf_get_flags_f weed_leaf_get_flags;

  /* plugins only got these in API 200 */
  __WEED_FN_DEF__ weed_plant_free_f weed_plant_free;
  __WEED_FN_DEF__ weed_leaf_delete_f weed_leaf_delete;

#ifndef __LIBWEED__
  __WEED_FN_DEF__ weed_malloc_f weed_malloc;
  __WEED_FN_DEF__ weed_free_f weed_free;
  __WEED_FN_DEF__ weed_memcpy_f weed_memcpy;
  __WEED_FN_DEF__ weed_memset_f weed_memset;

  /* added in API 200 */
  __WEED_FN_DEF__ weed_realloc_f weed_realloc;
  __WEED_FN_DEF__ weed_calloc_f weed_calloc;
  __WEED_FN_DEF__ weed_memmove_f weed_memmove;
#endif

  /* plant types */
#define WEED_PLANT_UNKNOWN 0
#define WEED_PLANT_FIRST_CUSTOM 16384

  /* type 'irrelevant' */
#define WEED_PLANT_GENERIC (WEED_PLANT_FIRST_CUSTOM - 1)

  /* Weed errors */
#define WEED_SUCCESS 			0
#define WEED_ERROR_MEMORY_ALLOCATION	1
#define WEED_ERROR_NOSUCH_LEAF		2
#define WEED_ERROR_NOSUCH_ELEMENT	3
#define WEED_ERROR_WRONG_SEED_TYPE	4
#define WEED_ERROR_IMMUTABLE		5
#define WEED_ERROR_UNDELETABLE		6
#define WEED_ERROR_CONCURRENCY		7
#define WEED_ERROR_BADVERSION		8

#define WEED_ERROR_FIRST_CUSTOM 1024

  /* Seed types */
#define WEED_SEED_INVALID		0

  /* Fundamental seeds */
#define WEED_SEED_INT			1 // int32_t / uint_32t

#define WEED_SEED_INT32			WEED_SEED_INT
#define WEED_SEED_int			WEED_SEED_INT
#define WEED_SEED_int32			WEED_SEED_INT
#define WEED_SEED_int32_t      		WEED_SEED_INT

#define WEED_SEED_DOUBLE		2 // 64 bit signed double

#define WEED_SEED_double		WEED_SEED_DOUBLE

#define WEED_SEED_BOOLEAN		3 // int32_t: restrict to values WEED_TRUE or WEED_FALSE

#define WEED_SEED_boolean		WEED_SEED_BOOLEAN

#define WEED_SEED_STRING		4 // NUL terminated array of char

#define WEED_SEED_string		WEED_SEED_STRING
#define WEED_SEED_charptr		WEED_SEED_STRING

#define WEED_SEED_INT64			5 // int64_t

#define WEED_SEED_int64			WEED_SEED_INT64
#define WEED_SEED_int64_t		WEED_SEED_INT64

  // annotation types
#define WEED_SEED_VOID			0

#define WEED_SEED_UINT			6 // alias for WEED_SEED_INT

#define WEED_SEED_UINT32		WEED_SEED_UINT
#define WEED_SEED_uint			WEED_SEED_UINT
#define WEED_SEED_uint32       		WEED_SEED_UINT
#define WEED_SEED_uint32_t     		WEED_SEED_UINT

#define WEED_SEED_UINT64		7 // alias for WEED_SEED_INT64

#define WEED_SEED_uint64		WEED_SEED_UINT64
#define WEED_SEED_uint64_t		WEED_SEED_UINT64

#define WEED_SEED_FLOAT			8 // alias for WEED_SEED_DOUBLE

#define WEED_SEED_float			WEED_SEED_FLOAT

#define WEED_SEED_FIRST_NON_PTR_TYPE	WEED_SEED_INT
#define WEED_SEED_LAST_NON_PTR_TYPE	WEED_SEED_UINT64

  /* Pointer seeds */
#define WEED_SEED_FUNCPTR		64 // weed_funcptr_t

#define WEED_SEED_funcptr		WEED_SEED_FUNCPTR
#define WEED_SEED_weed_funcptr_t       	WEED_SEED_FUNCPTR

#define WEED_SEED_VOIDPTR		65 // weed_voidptr_t

#define WEED_SEED_voidptr		WEED_SEED_VOIDPTR
#define WEED_SEED_voidptr_t		WEED_SEED_VOIDPTR

#define WEED_SEED_PLANTPTR		66 // weed_plant_t *

#define WEED_SEED_plantptr		WEED_SEED_PLANTPTR
#define WEED_SEED_weed_plantptr_t      	WEED_SEED_PLANTPTR

#define WEED_SEED_CONST_CHARPTR		67 // pointer to READ-ONLY string

#define WEED_SEED_FIRST_PTR_TYPE	WEED_SEED_FUNCPTR
#define WEED_SEED_LAST_PTR_TYPE		WEED_SEED_CONST_CHARPTR

#define WEED_SEED_FIRST_CUSTOM	1024

#define WEED_SEED_IS_STANDARD(st)					\
  ((((st) >= WEED_SEED_FIRST_NON_PTR_TYPE && (st) <= WEED_SEED_LAST_NON_PTR_TYPE) \
    || ((st) >= WEED_SEED_FIRST_PTR_TYPE && (st) <= WEED_SEED_LAST_PTR_TYPE)) ? WEED_TRUE : WEED_FALSE)

#define WEED_SEED_IS_CUSTOM(st) ((st) >= WEED_SEED_FIRST_CUSTOM ? WEED_TRUE : WEED_FALSE)

#define WEED_SEED_IS_POINTER(st) (WEED_IS_TRUE(WEED_SEED_IS_CUSTOM(st)) \
				  || (st >= WEED_SEED_FIRST_PTR_TYPE	\
				      && st <= WEED_SEED_LAST_PTR_TYPE) ? WEED_TRUE : WEED_FALSE)

#define WEED_SEED_IS_VALID(st) ((WEED_IS_TRUE(WEED_SEED_IS_STANDARD(st)) \
				 || WEED_IS_TRUE(WEED_SEED_IS_CUSTOM(st))) ? WEED_TRUE : WEED_FALSE)

  /* flag bits */
#define WEED_FLAG_UNDELETABLE		(1 << 0)  // leaf value may be altered but it cannot be deleted
#define WEED_FLAG_IMMUTABLE		(1 << 1)  // leaf value may not be changed, but it may be deleted
#define WEED_FLAG_PROXY			(1 << 2)  // ussed for extended func weed_ext_attach_leaf

#define WEED_FLAG_FIRST_RESERVED       	(1 << 3)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_11		(1 << 4)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_10		(1 << 5)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_9		(1 << 6)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_8		(1 << 7)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_7		(1 << 8)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_6		(1 << 9)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_5		(1 << 10) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_4		(1 << 11) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_3	 	(1 << 12) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_2	 	(1 << 13) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_1	 	(1 << 14) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_0	 	(1 << 15) // reserved for future use by Weed
#define WEED_FLAGBITS_RESERVED ((WEED_FLAG_FIRST_CUSTOM - 1) ^ (WEED_FLAG_FIRST_RESERVED - 1))
#define WEED_FLAG_FIRST_CUSTOM	(1 << 16) // bits 16 - 31 left for custom use

  /* mandatory leaf for all WEED_PLANTs, WEED_SEED_INT */
#define WEED_LEAF_TYPE		       	"type"

  /* may be used by any plant to set the API / ABI version, WEED_SEED_INT */
#define WEED_LEAF_WEED_API_VERSION 	"weed_api_version"
#define WEED_LEAF_WEED_ABI_VERSION 	WEED_LEAF_WEED_API_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_H__
