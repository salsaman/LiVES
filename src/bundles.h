// bundles.h
// LiVES
// (c) G. Finch 2003-2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _BUNDLES_H
#define _BUNDLES_H

typedef weed_plant_t bundle_t;
typedef bundle_t nirva_bundle_t;

typedef bundle_t lives_obj_t;
typedef bundle_t lives_instance_t;
typedef bundle_t lives_attr_t;
typedef bundle_t lives_contract_t;
typedef bundle_t lives_transform_t;
typedef bundle_t lives_trajactory_t;
typedef bundle_t lives_tsegment_t;
typedef bundle_t lives_attr_def_t;
typedef bundle_t lives_attr_container_t;
typedef bundle_t lives_attr_def_container_t;
typedef bundle_t lives_attr_connect_t;
typedef bundle_t blueprint_t;
typedef bundle_t strand_def_t;
typedef bundle_t strand_t;
typedef bundle_t scriptlet_t;

// implementation defaults and overrides
#define NIRVA_BUNDLE_T weed_plant_t

// type, ...
#define IMPL_FUNC_create_bundle_by_type create_gen2_bundle_by_type

// TODO - does not pass btype
#define IMPL_FUNC_create_bundle_from_blueprint create_gen2_bundle_with_vargs

#define IMPL_FUNC_nirva_value_set lives_strand_value_set
#define IMPL_FUNC_nirva_array_append lives_strand_array_append

// free a strand and the data it contains
#define IMPL_FUNC_nirva_strand_delete lives_strand_free

// free an empty bundle after all strands have been removed
#define IMPL_FUNC_nirva_bundle_free lives_bundle_free

#define IMPL_FUNC_nirva_bundle_list_strands lives_bundle_list_strands

#define IMPL_FUNC_nirva_get_value_int lives_strand_get_value_int
#define IMPL_FUNC_nirva_get_value_boolean lives_strand_get_value_boolean
#define IMPL_FUNC_nirva_get_value_double lives_strand_get_value_double
#define IMPL_FUNC_nirva_get_value_string lives_strand_get_value_string
#define IMPL_FUNC_nirva_get_value_int64 lives_strand_get_value_int64
#define IMPL_FUNC_nirva_get_value_uint64 lives_strand_get_value_uint64
#define IMPL_FUNC_nirva_get_value_voidptr lives_strand_get_value_voidptr
#define IMPL_FUNC_nirva_get_value_funcptr lives_strand_get_value_funcptr
#define IMPL_FUNC_nirva_get_value_bundleptr lives_strand_get_value_plantptr

#define IMPL_FUNC_nirva_get_array_int lives_strand_get_array_int
#define IMPL_FUNC_nirva_get_array_boolean lives_strand_get_array_boolean
#define IMPL_FUNC_nirva_get_array_double lives_strand_get_array_double
#define IMPL_FUNC_nirva_get_array_string lives_strand_get_array_string
#define IMPL_FUNC_nirva_get_array_int64 lives_strand_get_array_int64
#define IMPL_FUNC_nirva_get_array_voidptr lives_strand_get_array_voidptr
#define IMPL_FUNC_nirva_get_array_funcptr lives_strand_get_array_funcptr
#define IMPL_FUNC_nirva_get_array_bundleptr lives_strand_get_array_plantptr

// function which provides a short cut for finding array elements with matching key
// params bdl, strand or attr, key name, key_type, mval)
// returns bundleptr with matching data
#define IMPL_FUNC_nirva_keyed_array_find lives_strand_find_by_data

// list current strands

#define IMPL_FUNC_nirva_array_count lives_array_count

// optional optimisation; else we will check list_strands
#define IMPL_FUNC_nirva_bundle_has_strand bundle_has_strand

// recommended
#define IMPL_FUNC_nirva_bundle_request_blueprint get_blueprint_from_bundle
#define IMPL_FUNC_nirva_strand_copy bundle_strand_copy

/* //#define IMPL_FUNC_get_bundle_strand_type get_bundle_strand_type */
/* //#define IMPL_FUNC_bundle_list_opt_strands bundle_list_opt_items */
/* // */
/* /////////////////////// */
/* // if not present, the array will be cleared and recreated */
/* #define IMPL_FUNC_nirva_array_remove_item */
/* // if not present, the array will be deleted and recreated */
/* #define IMPL_FUNC_nirva_array_clear */

/* #define IMPL_FUNC_nirva_get_value_uint lives_strand_get_value_uint */
/* #define IMPL_FUNC_nirva_get_value_uint64 lives_strand_get_value_uint64 */
/* #define IMPL_FUNC_nirva_get_value_float lives_strand_get_value_float */

/* #define IMPL_FUNC_nirva_get_array_uint lives_strand_get_array_uint */
/* #define IMPL_FUNC_nirva_get_array_uint64 lives_strand_get_array_uint64 */
/* #define IMPL_FUNC_nirva_get_array_float lives_strand_get_array_float */

#define IS_BUNDLE_MAKER
#include "object-constants.h"

typedef NIRVA_BUNDLEDEF bundledef_t;
typedef NIRVA_CONST_BUNDLEDEF const_bundledef_t;
typedef NIRVA_STRAND bundle_strand;

///
const bundle_t *init_bundles(void);

char *get_short_name(const char *q);
//uint64_t get_vflags(const char *q, off_t *offx);
uint32_t get_vtype(const char *q, off_t *offx);
const char *get_vname(const char *q);
boolean get_is_array(const char *q);

bundle_t *create_gen1_bundle_by_type(bundle_type btype, ...) LIVES_SENTINEL;

bundle_t *var_bundle_from_bundledef(bundledef_t bdef, ...);
bundle_t *def_bundle_from_bundledef(bundledef_t bdef);

bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype);

boolean bundle_has_strand(bundle_t *, const char *stname);

bundledef_t get_bundledef_from_bundle(bundle_t *);
blueprint_t *get_blueprint_from_bundle(bundle_t *);

size_t bundle_array_append(bundle_t *bundle, const char *name, void *thing);
size_t bundle_array_append_n(bundle_t *bundle, const char *name, uint32_t vtype, int ne, ...);

uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened);
uint64_t get_bundle64sum(bundle_t *, char **flattened);

boolean set_bundle_value(bundle_t *, const char *name, ...);

char *nirvascope_bundle_to_header(bundle_t *, const char *tname, int idx);
char *nirvascope_blueprint_to_header(bundle_t *, const char *tname);
size_t nirvascope_get_bundle_weight(bundle_t *b);

bundle_t *lookup_item_in_array(bundle_t *con, const char *aname);

char *find_strand_by_name(bundle_t *, const char *name);

uint32_t get_attr_type(nirva_attr_t *attr);
int lives_attr_get_value_int(lives_attr_t *attr);
int64_t lives_attr_get_value_int64(lives_attr_t *attr);

bundle_t *lives_strand_get_value_bundleptr(bundle_t *, const char *item);
bundle_t *lives_strand_get_value_const_bundleptr(bundle_t *, const char *item);
void *lives_strand_get_value_voidptr(bundle_t *, const char *item);
int lives_strand_get_value_int(bundle_t *, const char *item);
uint32_t lives_strand_get_value_uint32(bundle_t *, const char *item);
char *lives_strand_get_value_string(bundle_t *, const char *item);
int64_t lives_strand_get_value_int64(bundle_t *, const char *item);
uint64_t lives_strand_get_value_uint64(bundle_t *, const char *item);

char **lives_strand_get_array_string(bundle_t *, const char *item);
char **bundle_list_items(bundle_t *);
void set_strand_value(bundle_t *, const char *, ...);

void lives_set_strand_value_gen1(bundle_t *bundle, const char *name, uint32_t vtype, va_list val);
void lives_set_strand_value_gen2(bundle_t *bundle, const char *name, va_list val);
#endif
