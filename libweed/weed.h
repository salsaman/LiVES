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

/* (C) G. Finch, 2005 - 2019 */

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

/* API / ABI version * 200 */
#define WEED_ABI_VERSION 200
#define WEED_ABI_VERSION_200  // do not remove this, even in higher versions
#define WEED_API_VERSION WEED_ABI_VERSION
#define WEED_API_VERSION_200  WEED_ABI_VERSION_200 // do not remove this, even in higher versions

#define WEED_TRUE 1
#define WEED_FALSE 0

#ifdef __LIBWEED__
#define  __WEED_FN_DEF__ extern
#else
#ifdef __WEED_HOST__
#define  __WEED_FN_DEF__
#else
#define  __WEED_FN_DEF__ static
#endif
#endif

typedef uint32_t weed_size_t;
typedef int32_t weed_error_t;
typedef void *weed_voidptr_t;

#define WEED_VOIDPTR_SIZE sizeof(weed_voidptr_t)

#ifndef HAVE_WEED_DATA_T
#define HAVE_WEED_DATA_T
typedef struct _weed_data weed_data_t;
#ifdef __LIBWEED__
struct _weed_data {
  weed_size_t size;
  weed_voidptr_t value;
};
#endif
#endif

#ifndef HAVE_WEED_LEAF_T
#define HAVE_WEED_LEAF_T
typedef struct _weed_leaf weed_leaf_t;
#ifdef __LIBWEED__
struct _weed_leaf {
  const char *key;
  uint32_t key_hash;
  int32_t seed_type;
  weed_size_t num_elements;
  weed_data_t **data;
  int32_t flags;
  weed_leaf_t *next;
};
#endif
#endif

#ifndef HAVE_WEED_PLANT_T
#define HAVE_WEED_PLANT_T
typedef weed_leaf_t weed_plant_t;
#endif

typedef void *(*weed_malloc_f)(size_t);
typedef void (*weed_free_f)(void *);
typedef void *(*weed_memset_f)(void *, int, size_t);
typedef void *(*weed_memcpy_f)(void *, const void *, size_t);

/* added in ABI 200 */
typedef void *(*weed_realloc_f)(void *, size_t);
typedef void *(*weed_calloc_f)(size_t, size_t);
typedef void *(*weed_memmove_f)(void *, const void *, size_t);

typedef weed_plant_t *(*weed_plant_new_f)(int32_t plant_type);
typedef char **(*weed_plant_list_leaves_f)(weed_plant_t *);
typedef weed_error_t (*weed_leaf_set_f)(weed_plant_t *, const char *key, int32_t seed_type, weed_size_t num_elems,
                                        weed_voidptr_t values);
typedef weed_error_t (*weed_leaf_get_f)(weed_plant_t *, const char *key, int32_t idx, weed_voidptr_t value);
typedef weed_size_t (*weed_leaf_num_elements_f)(weed_plant_t *, const char *key);
typedef weed_size_t (*weed_leaf_element_size_f)(weed_plant_t *, const char *key, int32_t idx);
typedef int32_t (*weed_leaf_seed_type_f)(weed_plant_t *, const char *key);
typedef int32_t (*weed_leaf_get_flags_f)(weed_plant_t *, const char *key);
typedef weed_error_t (*weed_plant_free_f)(weed_plant_t *);
typedef weed_error_t (*weed_leaf_delete_f)(weed_plant_t *, const char *key);

/* added in ABI 200 */
typedef weed_error_t (*weed_leaf_set_voidptr_sizes_f)(weed_plant_t *, const char *key,
    weed_size_t num_elems, weed_voidptr_t values, weed_size_t *sizes);
typedef weed_error_t (*weed_leaf_get_all_f)(weed_plant_t *, const char *key, int32_t *seed_type,
    weed_size_t *num_elems, weed_voidptr_t values, weed_size_t **sizes, int32_t *flags);

#if defined (__WEED_HOST__) || defined (__LIBWEED__)
/* host only functions */
typedef weed_error_t (*weed_leaf_set_flags_f)(weed_plant_t *, const char *key, int32_t flags);

__WEED_FN_DEF__ weed_leaf_set_flags_f weed_leaf_set_flags;

#ifndef __LIBWEED__
weed_error_t weed_init(int32_t abi);
#endif
#endif

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

/* Weed errors */
#define WEED_SUCCESS 0

#define WEED_NO_ERROR WEED_SUCCESS

#define WEED_ERROR_MEMORY_ALLOCATION 1
#define WEED_ERROR_NOSUCH_LEAF 2
#define WEED_ERROR_NOSUCH_ELEMENT 3
#define WEED_ERROR_WRONG_SEED_TYPE 4
#define WEED_ERROR_IMMUTABLE 5
#define WEED_ERROR_UNDELETABLE 6
#define WEED_ERROR_BADVERSION 8

/* Seed types */
#define WEED_SEED_INVALID 0 // the "seed_type" of a non-existent leaf

/* Fundamental seeds */
#define WEED_SEED_INT     1 // int32_t / uint_32t
#define WEED_SEED_DOUBLE  2 // 64 bit signed double
#define WEED_SEED_BOOLEAN 3 // int32_t: should only be set to values WEED_TRUE or WEED_FALSE
#define WEED_SEED_STRING  4 // NUL terminated array of char
#define WEED_SEED_INT64   5 // int64_t

/* Pointer seeds */
#define WEED_SEED_VOIDPTR  65 // weed_voidptr_t
#define WEED_SEED_PLANTPTR 66 // weed_plant_t *

/* flag bits */
#define WEED_FLAG_UNDELETABLE (1 << 0)  // leaf value may be altered but it cannot be deleted
#define WEED_FLAG_IMMUTABLE   (1 << 1)  // leaf value may not be changed, but it may be deleted
#define WEED_FLAG_RESERVED_0  (1 << 2)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_1  (1 << 3)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_2  (1 << 4)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_3  (1 << 5)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_4  (1 << 6)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_5  (1 << 7)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_6  (1 << 8)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_7  (1 << 9)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_8  (1 << 10) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_9  (1 << 11) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_10 (1 << 12) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_11 (1 << 13) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_12 (1 << 14) // reserved for future use by Weed
#define WEED_FLAG_RESERVED_13 (1 << 15) // reserved for future use by Weed

/* mandatory leaf for all WEED_PLANTs, WEED_SEED_INT */
#define WEED_LEAF_TYPE "type"

/* may be used by any plant to set the API / ABI version, WEED_SEED_INT */
#define WEED_LEAF_WEED_API_VERSION "weed_api_version"
#define WEED_LEAF_WEED_ABI_VERSION WEED_LEAF_WEED_API_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_H__
