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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2007 */

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

#define WEED_TRUE 1
#define WEED_FALSE 0

/* Leaf flags */
#define WEED_LEAF_READONLY_PLUGIN (1<<0)
#define WEED_LEAF_READONLY_HOST (1<<1)

/* Weed errors */
/* Core errors */
#define WEED_NO_ERROR 0
#define WEED_ERROR_MEMORY_ALLOCATION 1
#define WEED_ERROR_LEAF_READONLY 2
#define WEED_ERROR_NOSUCH_ELEMENT 3
#define WEED_ERROR_NOSUCH_LEAF 4
#define WEED_ERROR_WRONG_SEED_TYPE 5

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

// these function types are fixed, and must be defined in the host
typedef void *(*weed_malloc_f)(size_t size);
typedef void (*weed_free_f)(void *ptr);
typedef void *(*weed_memset_f)(void *s, int c, size_t n);
typedef void *(*weed_memcpy_f)(void *dest, const void *src, size_t n);

#ifndef HAVE_WEED_PLANT_T
#define HAVE_WEED_PLANT_T

typedef struct weed_leaf weed_leaf_t;
typedef struct weed_data weed_data_t;
typedef weed_leaf_t weed_plant_t;
typedef size_t weed_size_t; // may be set to uint32_t or uint64_t

/* private data - these fields must NOT be accessed directly ! */
struct weed_leaf {
  const char *key;
  int seed_type;
  int num_elements;
  weed_data_t **data;
  int flags;
  weed_leaf_t *next;
};

struct weed_data {
  weed_size_t size;
  void *value;
};

#endif

typedef int64_t weed_timecode_t;

/** this is fixed for ever, set in bootstrap_func */
typedef int (*weed_default_getter_f)(weed_plant_t *plant, const char *key, int idx, void *value);

/* host and plugin functions, may be changed depending on API level */
typedef weed_plant_t *(*weed_plant_new_f)(int plant_type);
typedef char **(*weed_plant_list_leaves_f)(weed_plant_t *plant);
typedef int (*weed_leaf_set_f)(weed_plant_t *plant, const char *key, int seed_type, int num_elems, void *value);
typedef int (*weed_leaf_get_f)(weed_plant_t *plant, const char *key, int idx, void *value);
typedef int (*weed_leaf_num_elements_f)(weed_plant_t *plant, const char *key);
typedef size_t (*weed_leaf_element_size_f)(weed_plant_t *plant, const char *key, int idx);
typedef int (*weed_leaf_seed_type_f)(weed_plant_t *plant, const char *key);
typedef int (*weed_leaf_get_flags_f)(weed_plant_t *plant, const char *key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_H__
