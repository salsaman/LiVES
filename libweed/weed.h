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

///////////////// host applications should #include weed-host.h before this header /////////////////////////

#ifndef __WEED_H__
#define __WEED_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define __need_size_t
#define __need_NULL
#include <stddef.h>
#include <inttypes.h>

  /* API version * 200 */
#define WEED_API_VERSION 200
#define WEED_API_VERSION_200  // do not remove this, even in higher versions

#define WEED_TRUE 1
#define WEED_FALSE 0

#define WEED_VOIDPTR_SIZE 8

  /* Weed errors */
#define WEED_NO_ERROR 0
#define WEED_ERROR_MEMORY_ALLOCATION 1
#define WEED_ERROR_NOSUCH_LEAF 2
#define WEED_ERROR_NOSUCH_ELEMENT 3
#define WEED_ERROR_WRONG_SEED_TYPE 4
#define WEED_ERROR_FLAGALL 5
#define WEED_ERROR_FLAGBIT 6
#define WEED_ERROR_BADVERSION 7

  /* Seed types */
  /* Fundamental seeds */
#define WEED_SEED_INT     1
#define WEED_SEED_DOUBLE  2
#define WEED_SEED_BOOLEAN 3
#define WEED_SEED_STRING  4
#define WEED_SEED_INT64   5

  /* Pointer seeds */
#define WEED_SEED_VOIDPTR  65
#define WEED_SEED_PLANTPTR 66

#define WEED_LEAF_TYPE "type"

  typedef uint32_t weed_size_t;
  typedef int32_t weed_error_t;

#ifndef HAVE_WEED_DATA_T
#define HAVE_WEED_DATA_T
  typedef struct {
    weed_size_t size;
    void *value;
  } weed_data_t;
#endif

#ifndef HAVE_WEED_LEAF_T
#define HAVE_WEED_LEAF_T
  typedef struct weed_leaf weed_leaf_t;
  struct weed_leaf {
    const char *key;
    uint32_t key_hash;
    int32_t seed_type;
    weed_size_t num_elements;
    weed_data_t **data;
    int32_t flags;
    weed_leaf_t *next;
  };
#endif

#ifndef HAVE_WEED_PLANT_T
#define HAVE_WEED_PLANT_T
  typedef weed_leaf_t weed_plant_t;
#endif

  // these functions must be defined in the host
  typedef void *(*weed_malloc_f)(size_t size);
  typedef void *(*weed_realloc_f)(void *, size_t size);
  typedef void (*weed_free_f)(void *);
  typedef void *(*weed_memset_f)(void *, int c, size_t n);
  typedef void *(*weed_memcpy_f)(void *dest, const void *src, size_t n);

  /* host and plugin functions */
  typedef weed_plant_t *(*weed_plant_new_f)(int32_t plant_type);
  typedef char **(*weed_plant_list_leaves_f)(weed_plant_t *);
  typedef weed_error_t (*weed_leaf_set_f)(weed_plant_t *, const char *key, int32_t seed_type, weed_size_t num_elems, void *values);
  typedef weed_error_t (*weed_leaf_get_f)(weed_plant_t *, const char *key, int32_t idx, void *value);
  typedef weed_size_t (*weed_leaf_num_elements_f)(weed_plant_t *, const char *key);
  typedef weed_size_t (*weed_leaf_element_size_f)(weed_plant_t *, const char *key, int32_t idx);
  typedef int32_t (*weed_leaf_seed_type_f)(weed_plant_t *, const char *key);
  typedef int32_t (*weed_leaf_get_flags_f)(weed_plant_t *, const char *key);

  /* API 200 */
  typedef weed_error_t (*weed_leaf_flag_set_f)(weed_plant_t *, const char *key, int32_t seed_type,
					       weed_size_t num_elems, void **values, weed_size_t *sizes, int32_t flagmask);
  typedef weed_error_t (*weed_leaf_flag_get_f)(weed_plant_t *, const char *key, int32_t *seed_type,
					       weed_size_t *num_elems, void **values, weed_size_t **sizes, int32_t *flags);

#ifdef __WEED_HOST__
  // host only functions
  typedef weed_error_t (*weed_plant_free_f)(weed_plant_t *);
  typedef weed_error_t (*weed_leaf_delete_f)(weed_plant_t *, const char *key);
  typedef weed_error_t (*weed_leaf_set_flags_f)(weed_plant_t *, const char *key, int32_t flags);

  // host MUST call this to set up the lib, passing in the API version it wants to use
  weed_error_t weed_init(int32_t api);

#ifndef _SKIP_WEED_API_
  // function pointers which get set in weed_init()
  weed_leaf_get_f weed_leaf_get;
  weed_leaf_set_f weed_leaf_set;
  weed_leaf_flag_set_f weed_leaf_flag_set;
  weed_leaf_flag_get_f weed_leaf_flag_get;
  weed_plant_new_f weed_plant_new;
  weed_plant_list_leaves_f weed_plant_list_leaves;
  weed_leaf_num_elements_f weed_leaf_num_elements;
  weed_leaf_element_size_f weed_leaf_element_size;
  weed_leaf_seed_type_f weed_leaf_seed_type;
  weed_leaf_get_flags_f weed_leaf_get_flags;
  
  weed_malloc_f weed_malloc;
  weed_realloc_f weed_realloc;
  weed_free_f weed_free;
  weed_memcpy_f weed_memcpy;
  weed_memset_f weed_memset;

  /* host only functions */
  weed_plant_free_f weed_plant_free;
  weed_leaf_delete_f weed_leaf_delete;
  weed_leaf_set_flags_f weed_leaf_set_flags;

#endif

#else
  // plugin gets static versions, host sets these in plugin_info when the plugin calls weed_bootstrap() in the host
  // plugin then sets them from the plugin_info
  static weed_leaf_get_f weed_leaf_get;
  static weed_leaf_set_f weed_leaf_set;
  static weed_leaf_flag_set_f weed_leaf_flag_set;
  static weed_leaf_flag_get_f weed_leaf_flag_get;
  static weed_plant_new_f weed_plant_new;
  static weed_plant_list_leaves_f weed_plant_list_leaves;
  static weed_leaf_num_elements_f weed_leaf_num_elements;
  static weed_leaf_element_size_f weed_leaf_element_size;
  static weed_leaf_seed_type_f weed_leaf_seed_type;
  static weed_leaf_get_flags_f weed_leaf_get_flags;
  
  static weed_malloc_f weed_malloc;
  static weed_realloc_f weed_realloc;
  static weed_free_f weed_free;
  static weed_memcpy_f weed_memcpy;
  static weed_memset_f weed_memset;
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_H__
