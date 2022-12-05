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
typedef bundle_t lives_segment_t;
typedef bundle_t lives_attr_def_t;
typedef bundle_t lives_attr_container_t;
typedef bundle_t lives_attr_def_container_t;
typedef bundle_t lives_attr_connect_t;
typedef bundle_t blueprint_t;
typedef bundle_t strand_def_t;
typedef bundle_t scriptlet_t;

#ifndef NIRVA_BUNDLEPTR_T
#define NIRVA_BUNDLEPTR_T weed_plant_t *
#endif
#ifndef NIRVA_CONST_BUNDLEPTR_T
#define NIRVA_CONST_BUNDLEPTR_T void *
#endif

typedef NIRVA_BUNDLEDEF bundledef_t;
typedef NIRVA_STRAND bundle_strand;

bundle_t *create_bundle_by_type(bundle_type bt, ...) LIVES_SENTINEL;

// implfuncs
bundle_t *lives_bundle_create(void);
int32_t lives_strand_array_append(bundle_t *, const char *stname, uint32_t sttype, uint32_t nvals, ...);
void lives_strand_array_clear(bundle_t *, const char *stname);
void lives_strand_delete(bundle_t *, const char *stname);
void lives_bundle_free(bundle_t *);
uint32_t lives_array_get_size(bundle_t *, const char *stname);
char **lives_bundle_list_strands(bundle_t *);

int *lives_strand_get_array_int(bundle_t *, const char *strand_name);
uint32_t *lives_strand_get_array_uint(bundle_t *, const char *strand_name);
boolean *lives_strand_get_array_boolean(bundle_t *, const char *strand_name);
double *lives_strand_get_array_double(bundle_t *, const char *strand_name);
int64_t *lives_strand_get_array_int64(bundle_t *, const char *strand_name);
uint64_t *lives_strand_get_array_uint64(bundle_t *, const char *strand_name);
char **lives_strand_get_array_string(bundle_t *, const char *strand_name);
void **lives_strand_get_array_voidptr(bundle_t *, const char *strand_name);
lives_funcptr_t *lives_strand_get_array_funcptr(bundle_t *, const char *strand_name);
bundle_t **lives_strand_get_array_bundleptr(bundle_t *, const char *strand_name);
//bundle_t **lives_strand_get_array_const_bundletptr(bundle_t *, const char *strand_name);

// 0X
#define NIRVA_IMPL_FUNC_create_bundle_by_type create_bundle_by_type

//#define IMPL_FUNC_create_bundle_from_blueprint create_gen2_bundle_with_vargs
// 01
#define NIRVA_IMPL_FUNC_bundle_create lives_bundle_create
// 02
#define NIRVA_IMPL_FUNC_array_append lives_strand_array_append
// 03
#define NIRVA_IMPL_FUNC_array_clear lives_strand_array_clear
// 04
#define NIRVA_IMPL_FUNC_strand_delete lives_strand_delete
// 05
#define NIRVA_IMPL_FUNC_bundle_free lives_bundle_free
// 06
#define NIRVA_IMPL_FUNC_array_get_size lives_array_get_size
// 07
#define NIRVA_IMPL_FUNC_bundle_list_strands lives_bundle_list_strands
// 08 - 015
#define NIRVA_IMPL_FUNC_get_array_int lives_strand_get_array_int
#define NIRVA_IMPL_FUNC_get_array_boolean lives_strand_get_array_boolean
#define NIRVA_IMPL_FUNC_get_array_double lives_strand_get_array_double
#define NIRVA_IMPL_FUNC_get_array_string lives_strand_get_array_string
#define NIRVA_IMPL_FUNC_get_array_int64 lives_strand_get_array_int64
#define NIRVA_IMPL_FUNC_get_array_voidptr lives_strand_get_array_voidptr
#define NIRVA_IMPL_FUNC_get_array_funcptr lives_strand_get_array_funcptr
#define NIRVA_IMPL_FUNC_get_array_bundleptr lives_strand_get_array_bundleptr

/////////////////////////////
// recommended
boolean bundle_has_strand(bundle_t *bundle, const char *iname);

int64_t lives_strand_copy(bundle_t *dest, const char *dname, bundle_t *src,
                          const char *sname);

#define NIRVA_IMPL_FUNC_strand_copy lives_strand_copy

// optional optimisation; else we will check list_strands
#define NIRVA_IMPL_FUNC_bundle_has_strand bundle_has_strand

///// others ///////////

// IMPL_FUNC_validate_bdefs
// or
// realloc or malloc,
// strdup or memcpy
// free
//
// opt
// strlen, - else look for 0
// strcmp // else compare bytes
// strncmp // else strlen + cmp
//

#define IS_BUNDLE_MAKER
#include "object-constants.h"
///
const bundle_t *init_bundles(void);

char *get_short_name(const char *q);
//uint64_t get_vflags(const char *q, off_t *offx);
uint32_t get_vtype(const char *q, off_t *offx);
const char *get_vname(const char *q);
boolean get_is_array(const char *q);

bundle_t *var_bundle_from_bundledef(bundledef_t bdef, ...);
bundle_t *def_bundle_from_bundledef(bundledef_t bdef);

bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype);

boolean bundle_has_strand(bundle_t *, const char *stname);

blueprint_t *get_blueprint_from_bundle(bundle_t *);
const blueprint_t *get_blueprint_for_btype(bundle_type btype);

uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened);
uint64_t get_bundle64sum(bundle_t *, char **flattened);

void set_strand_value(bundle_t *, const char *name, uint32_t vtype, ...);

char *nirvascope_bundle_to_header(bundle_t *, const char *tname, int idx);
char *nirvascope_blueprint_to_header(bundle_t *, const char *tname);
size_t nirvascope_get_bundle_weight(bundle_t *b);

int lives_strand_append_sub_bundles(bundle_t *bundle, const char *name, int ns, bundle_t **subs);
//void lives_strand_append_sub_bundles(bundle_t *, const char *name, int ns, bundle_t **subs);
//void lives_strand_include_sub_bundle(bundle_t *, const char *name, bundle_t *sub);
int lives_strand_include_sub_bundle(bundle_t *bundle, const char *name, bundle_t *sub);
/* void lives_strand_add_const_bundles(bundle_t *, const char *name, int ns, bundle_t **ptrs); */
/* void lives_strand_add_const_bundle(bundle_t *, const char *name, bundle_t *ptr); */
int lives_strand_add_const_bundles(bundle_t *bundle, const char *name, int ns, bundle_t **ptrs);
int lives_strand_add_const_bundle(bundle_t *bundle, const char *name, bundle_t *ptr);

bundle_t *find_array_item_by_key(bundle_t *, const char *kname);

uint32_t lives_attr_get_type(lives_attr_t *);
int lives_attr_get_value_int(lives_attr_t *);
int64_t lives_attr_get_value_int64(lives_attr_t *);

bundle_t *lives_strand_get_value_bundleptr(bundle_t *, const char *strand_name);
bundle_t *lives_strand_get_value_const_bundleptr(bundle_t *, const char *strand_name);
void *lives_strand_get_value_voidptr(bundle_t *, const char *strand_name);
int lives_strand_get_value_int(bundle_t *, const char *strand_name);
uint32_t lives_strand_get_value_uint32(bundle_t *, const char *strand_name);
char *lives_strand_get_value_string(bundle_t *, const char *strand_name);
int64_t lives_strand_get_value_int64(bundle_t *, const char *strand_name);
uint64_t lives_strand_get_value_uint64(bundle_t *, const char *strand_name);

uint64_t stdef_get_flags(strand_def_t *);
//uint32_t stdef_get_strand_type(strand_def_t *);
scriptlet_t *stdef_get_restrictions(strand_def_t *);

///
/* void lives_set_strand_value_gen1(bundle_t *bundle, const char *name, uint32_t vtype, va_list val); */
/* void lives_set_strand_value_gen2(bundle_t *bundle, const char *name, va_list val); */
#endif
