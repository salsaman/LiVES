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

///////////////// host applications should #include weed-host.h before this header /////////////////////////
///////////////// libweed.h  shoule be #included/////////////////////////

#ifndef __WEED_H__
#define __WEED_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

  // 203 - added weed_set_custom_element_size
  // weed_error_t (*weed_set_custom_element_size_f)(weed_plant_t *,
  // 	const char *key, weed_size_t idx, weed_size_t new_size);
  // For an existing element of a leaf, sets the value returned by weed_leaf_element_size
  // The leaf must have a custom seed type else WEED_ERROR_WRONG_SEED_TYPE will be returned.

#ifndef IGN_MISSING_LIBWEED_H
#ifndef _HAVE_LIBWEED_H_
  /* needed for weed_plant_t, weed_seed_t, weed_flags_t, weed_size_t, weed_voidptr_t */
  //#warn libweed.h should be #included before weed.h

#ifdef _BUILD_LOCAL_
#include "libweed.h"
#include "weed.h"
#else
#include <weed/libweed.h>
#include <weed/weed.h>
#endif

#endif
#endif

#define __need_size_t // for malloc, realloc, etc
#define __need_NULL
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>

#define WEED_API_VERSION 		203

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
  typedef weed_error_t (*weed_leaf_get_f)(weed_plant_t *, const char *key, weed_size_t idx, weed_voidptr_t value);
  typedef weed_size_t (*weed_leaf_num_elements_f)(weed_plant_t *, const char *key);
  typedef weed_size_t (*weed_leaf_element_size_f)(weed_plant_t *, const char *key, weed_size_t idx);

  typedef weed_seed_t (*weed_leaf_seed_type_f)(weed_plant_t *, const char *key);
  typedef weed_flags_t (*weed_leaf_get_flags_f)(weed_plant_t *, const char *key);
  typedef weed_error_t (*weed_plant_free_f)(weed_plant_t *);
  typedef weed_error_t (*weed_leaf_delete_f)(weed_plant_t *, const char *key);

#if defined (__WEED_HOST__) || defined (__LIBWEED__)
  /* host only functions */

  typedef weed_error_t (*weed_leaf_set_flags_f)(weed_plant_t *, const char *key, weed_flags_t flags);

  typedef weed_error_t (*weed_set_custom_element_size_f)(weed_plant_t *, const char *key, weed_size_t idx,
							 weed_size_t new_size);
  typedef weed_error_t (*weed_leaf_set_private_data_f)(weed_plant_t *, const char *key, void *data);
  typedef weed_error_t (*weed_leaf_get_private_data_f)(weed_plant_t *, const char *key, void **ret_loc);

  __WEED_FN_DEF__ weed_leaf_set_flags_f weed_leaf_set_flags;
  __WEED_FN_DEF__ weed_set_custom_element_size_f weed_set_custom_element_size;
  __WEED_FN_DEF__ weed_leaf_set_private_data_f weed_leaf_set_private_data;
  __WEED_FN_DEF__ weed_leaf_get_private_data_f weed_leaf_get_private_data;

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
#define WEED_PLANT_UNKNOWN	0

  // type reserved for "temporary" "on-the-fly" plants
#define WEED_PLANT_TMP		123

#define WEED_PLANT_FIRST_CUSTOM 16384

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

  /* utility errors */
#define WEED_ERROR_NOSUCH_PLANT		16
#define WEED_ERROR_WRONG_PLANT_TYPE	17

#define WEED_ERROR_UNSPECIFIED		512

#define WEED_ERROR_FIRST_CUSTOM 	1024

  /* Seed types */
#define WEED_SEED_INVALID		-1

#define WEED_SEED_VOID			0
#define WEED_SEED_NONE			0
#define WEED_SEED_ANY			0

  /* Fundamental seeds - default storage type*/
#define WEED_SEED_INT			1 // 32 bit signed inteher
  /* aliases */
#define WEED_SEED_INT32			WEED_SEED_INT
#define WEED_SEED_int			WEED_SEED_INT
#define WEED_SEED_int32			WEED_SEED_INT
#define WEED_SEED_int32_t      		WEED_SEED_INT

#define WEED_SEED_DOUBLE		2 // 64 bit signed double
  /* aliases */
#define WEED_SEED_double		WEED_SEED_DOUBLE

#define WEED_SEED_BOOLEAN		3 // weed_boolean_t -32 bit signed integer  (& 0xFF)
  /* aliases */
#define WEED_SEED_boolean		WEED_SEED_BOOLEAN

#define WEED_SEED_STRING		4 // NUL terminated array of 8 bit char
  /* aliases */
#define WEED_SEED_string		WEED_SEED_STRING
#define WEED_SEED_charptr		WEED_SEED_STRING

#define WEED_SEED_INT64			5 // 64 bit signed integer
  /* aliases */
#define WEED_SEED_int64			WEED_SEED_INT64
#define WEED_SEED_int64_t		WEED_SEED_INT64

  /* annotation types - these types may be treated internally as the aliased type,
     and cast to/from the anootation type externally*/
#define HAVE_WEED_SEED_UINT		1
#define WEED_SEED_UINT			6 // alias for WEED_SEED_INT

#define WEED_SEED_UINT32		WEED_SEED_UINT
#define WEED_SEED_uint			WEED_SEED_UINT
#define WEED_SEED_uint32       		WEED_SEED_UINT
#define WEED_SEED_uint32_t     		WEED_SEED_UINT

#define HAVE_WEED_SEED_UINT64		1
#define WEED_SEED_UINT64		7 // alias for WEED_SEED_INT64

#define WEED_SEED_uint64		WEED_SEED_UINT64
#define WEED_SEED_uint64_t		WEED_SEED_UINT64

#define HAVE_WEED_SEED_FLOAT		1
#define WEED_SEED_FLOAT			8 // alias for WEED_SEED_DOUBLE

#define WEED_SEED_float			WEED_SEED_FLOAT
  /* end annotation types */

  /* informational types - convenience values */
  /* for internal use by applications - not valid for libweed functions */
#define WEED_SEED_VARIADIC		32
#define WEED_SEED_VA_LIST		33
  /* end informational types */
  
#define WEED_SEED_FIRST_NON_PTR_TYPE	WEED_SEED_INT
#define WEED_SEED_LAST_NON_PTR_TYPE	WEED_SEED_UINT64

  /* Pointer types */
#define WEED_SEED_FUNCPTR		64 // weed_funcptr_t

#define WEED_SEED_funcptr		WEED_SEED_FUNCPTR
#define WEED_SEED_weed_funcptr_t       	WEED_SEED_FUNCPTR

#define WEED_SEED_VOIDPTR		65 // weed_voidptr_t

#define WEED_SEED_voidptr		WEED_SEED_VOIDPTR
#define WEED_SEED_voidptr_t		WEED_SEED_VOIDPTR

#define WEED_SEED_PLANTPTR		66 // weed_plant_t *

#define WEED_SEED_plantptr		WEED_SEED_PLANTPTR
#define WEED_SEED_weed_plantptr_t      	WEED_SEED_PLANTPTR

#define WEED_SEED_FIRST_PTR_TYPE	WEED_SEED_FUNCPTR
#define WEED_SEED_LAST_PTR_TYPE		WEED_SEED_PLANTPTR

#define WEED_SEED_FIRST_CUSTOM		1024

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

#define WEED_FLAG_FIRST_RESERVED       	(1 << 2)  // reserved for future use by Weed
#define WEED_FLAG_RESERVED_12		(1 << 4)  // reserved for future use by Weed
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
#define WEED_FLAG_FIRST_CUSTOM	(1 << 16) // bits 16 - 31 left for custom use
#define WEED_FLAGBITS_RESERVED ((WEED_FLAG_FIRST_CUSTOM - 1) ^ (WEED_FLAG_FIRST_RESERVED - 1))

  /* mandatory leaf for all WEED_PLANTs, WEED_SEED_INT */
#define WEED_LEAF_TYPE		       	"type"

  /* may be used by any plant to set the API / ABI version, WEED_SEED_INT */
#define WEED_LEAF_WEED_API_VERSION 	"weed_api_version"
#define WEED_LEAF_WEED_ABI_VERSION 	"weed_abi_version"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_H__
