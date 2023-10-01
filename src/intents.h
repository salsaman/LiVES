// intents.h
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// this is highly experimental code, and may change radically

// features (potential)
// - formalises function prototypes so their argument types and return types can be known at runtime (introspection)
// - using intents allows different subtypes to call different functions for the same intent (inheritance, polymorphism, namespaces)
// - within one object, same intent can be mapped to various functions depending on the arguments and return type (function overloading)
// - provides a convenient location for gathering function arguments and storing the return value(s) (pseudo classes)
// - objects can be "smart" and provide defaults for arguments, query other objects, or get missing values from the user (defaults++)
// - arguments can be mapped to things other than simple parameters, for example "self", "self.fundamental" (Decorator Pattern)
// - objects can create and manage things other than object classes, for example and object class which manages the lifecyle of a C struct
//    (Prototype pattern)
// - allows functions to be chained in sequence with outputs from one feeding into the next (function sequencing)
// - TBD - provides for verifying that a set of conditions is satisfied before the function(s) are called (argument validation)
// - objects can list capacities which depend on the intent and object state (introspection, inheritance, polymorphism)
// - a status can be returned after any step in the transform - new requirements and conditions can be added (function sequencing)
// - the transform returns a status which can be updated dynamically (e,g waiting, needs_data) (dynamic returns)
// - allows for the possibility of object - object communication and data sharing (AI like behaviours, swarming, nnets)
// - enables goal based activities based on a sequence of state changes from the current state to the goal state (AI, goal oriented behaviours)
// - allows for "factory" type templates which can create instances of various subtypes (inheritance)
// - modular implementation allows for the various components to be used independently (e.g "intention" can be used anywhere
//		like an enum) (modular design)
//
// transforms are desribed in "contracts", which need to be negotiated between
// two or more objects.
// the contract has intentcaps, attributes, hooks,
// every object / instance MUST have an attribute ATTR_GENERIC_CONTRACTS
// the value can be read, and contains a NULL terminated arrayof contracts
// one of the contracts must be for a transform negotiate_contract.
// this is a special transform which takes a contract object as input attributes
// and has the effect of changing the value of the 'status' attribute for the contract.
// This contract must be marked as "no_negotiate" as obviously it would be impossible to
// negotiate to negotiate.
//     'needs_data' status corresponds to a transform waiting for a hook to return new data
//
// - some requirements can be objects with a specific type / subtype(s) / state(s)
// - reqs, can be optional, for some, caller can change the default, and let user / or an object provide the value.
//    Some values n be set readonly.

#ifndef HAS_LIVES_INTENTS_H
#define HAS_LIVES_INTENTS_H

#define NIRVA_BUNDLEPTR_T weed_plantptr_t

extern boolean bundle_has_item(NIRVA_BUNDLEPTR_T, const char *item);

#define IMPL_FUNC_HAS_ITEM bundle_has_item

#include "lists.h"
#include "object-constants.h"

#if defined (_BASE_DEFS_ONLY_) || !defined (HAS_LIVES_INTENTS_H_BASE_DEFS)
// call with _BASE_DEFS_ONLY_ defined to get early definitions
#define ADD_BASE_DEFS
#endif

#ifndef _BASE_DEFS_ONLY_
#define ashift(v0, v1) ((uint64_t)(((v0) << 8) | (v1)))
#define ashift4(a, b, c, d) ((uint64_t)(((ashift((a), (b)) << 16) | (ashift((c), (d))))))
#define ashift8(a, b, c, d, e, f, g, h) ((uint64_t)(((ashift4((a), (b), (c), (d))) << 32) | \
							      ashift4((e), (f), (g), (h))))
#define IMkType(str) ((const uint64_t)(ashift8((str)[0], (str)[1], (str)[2], (str)[3], \
					       (str)[4], (str)[5], (str)[6], (str)[7])))

#define LIVES_OBJECT(o) ((lives_obj_t *)((o)))

typedef union {
  uint8_t c[8];
  uint64_t u;
} charbytes;

// for external object data (e.g g_object)
#define INTENTION_KEY "intention_"

typedef struct _object_transform_t lives_object_transform_t;
//
typedef weed_param_t lives_tx_param_t;
typedef weed_plant_t lives_obj_attr_t;
typedef weed_plant_t lives_contract_t;

#define LIVES_PLANT_BUNDLE 21212

#define HOOKFUNCS_ONLY
#include "threading.h"
#ifdef HOOKFUNCS_ONLY
#undef HOOKFUNCS_ONLY
#endif

typedef lives_refcounter_t obj_refcounter;

// lives_object_t // DEPRECATED - new code should use lives_obj_t
struct _object_t {
  uint64_t uid; // unique id for this instnace (const)
  obj_refcounter refcounter;
  uint64_t type; // object type - from IMkType (const)
  uint64_t subtype; // object subtype, can change during a transformation
  int state; // object state
  lives_intentcap_t icap;
  lives_obj_attr_t **attributes; // internal parameters (properties)
  lives_object_transform_t *active_tx; // pointer to currently running transform (or NULL)
  lives_hook_stack_t *hook_stacks[N_HOOK_POINTS]; /// TODO - these should probably be part of active_tx
  void *priv; // internal data belonging to the object
};

// TODO - types should register themselves, and then be queried
struct _objsubdef {
  uint64_t subtype; // object subtype, can change during a transformation
  lives_obj_attr_t **common_attributes; // internal attributes common to all states
  ////
  struct _obj_state_details {
    int *states; // possible object states
    lives_obj_attr_t **state_attributes; // state specific attributes ???
    lives_object_transform_t **tx; // array of transform functions for object type / subtype / state
  } **state_dets;
};

/////////////

#endif // _BASE_DEFS_ONLY_ is defined

#ifdef ADD_BASE_DEFS
// set base defs early, as we need these at the start

#ifndef HAS_LIVES_INTENTS_H_BASE_DEFS
#define HAS_LIVES_INTENTS_H_BASE_DEFS

#ifdef _BASE_DEFS_ONLY_
#undef  HAS_LIVES_INTENTS_H
#endif

// avoiding using an enum allows the list to be extended in other headers
typedef int32_t lives_intention;
typedef weed_plant_t lives_capacities_t;
typedef weed_plant_t lives_bundle_t;

#define ICAP_DESC_LEN 64

typedef struct {
  char desc[ICAP_DESC_LEN];  // descriptive name of icaps (e.g load, download)
  lives_intention intent;
  lives_capacities_t *capacities; ///< type specific capabilities
  lsd_struct_def_t *lsd;
} lives_intentcap_t;

#endif // not def HAS_LIVES_INTENTS_H_BASE_DEFS

#undef ADD_BASE_DEFS
#endif // ADD_BASE_DEFS

#ifdef _BASE_DEFS_ONLY_

typedef lives_funcptr_t object_funcptr_t;
typedef struct _object_t lives_object_t;
typedef struct _object_t lives_object_instance_t;
typedef weed_plant_t lives_obj_instance_t;
typedef struct _obj_status_t lives_transform_status_t;

#undef _BASE_DEFS_ONLY_
#else

struct _obj_status_t {
  int *status; /// pointer to an int (some states are dynamic)
  obj_refcounter refcounter;
};

// create, destroy, set subtype, set state, set value, get value

// clock GET_VALUE :: REALTIME :: out tcode, source, etc.
// in reset
// opt att. SET_VALUE :: ticks, source

// transport controller GET_VALUE | REALTIME, video
// mand att. running, SET_VALUE timecode | <- obj will call the cb as needed

// opt att. running / SET_VALUE cmd_msg | <- other
//
// inputs timecode, cmd msgs
// outputs clip stack, frame stack

/// hook
// status which triggers the hook
// transform that the hook must call
// callback which is set :: status cb_func(obj, * hook);
// user_data for callback

// attt : tyoe / sub / state / status, intentcaps
// - check if can match reqts for secondary tx

// player : GET_VALUE / REALTIME | VIDEO
// in ->stack of clip objects, array of clipidx, frame; ouput : layer
// mand att. SET_VALUE timecode, layer

// displayer ::
/// OBJ_INTENTION_DISPLAY
// mand att: layer
//
// GET_SET_VALUES: width heigth, sepwin

/// transforms

// TODO - link these to transform requirements / inputs / outputs
typedef struct {
  lives_tx_param_t **params; ///< (can be converted to normal params via weed_param_from_iparams)
} lives_intentparams_t;

weed_plant_t *lives_object_instance_create(uint64_t type, uint64_t subtype);

// shorthand for calling OBJ_INTENTION_UNREF in the instance
boolean lives_obj_instance_destroy(lives_obj_instance_t *);

boolean lives_object_instance_destroy(lives_obj_instance_t *);

// shorthand for calling OBJ_INTENTION_UNREF in the instance
int lives_object_instance_unref(lives_obj_instance_t *);

// shorthand for calling OBJ_INTENTION_REF in the instance
int lives_object_instance_ref(lives_obj_instance_t *);

#define LIVES_LEAF_OWNER "owner_uid"

#define LIVES_LEAF_VALUE_TYPE "value_type"
#define LIVES_LEAF_NAME "name"
#define LIVES_LEAF_ATTR_GRP "attr_group"
#define LIVES_LEAF_VALUE "value"
#define LIVES_LEAF_NAME "name"
#define LIVES_LEAF_PARENT "parent"
#define LIVES_LEAF_OBJ_TYPE "obj_type"
#define LIVES_LEAF_OBJ_SUBTYPE "obj_subtype"

#define LIVES_PLANT_OBJECT 31338
// placeholder values
#define OBJ_TYPE_PROC_THREAD   	1
#define OBJ_TYPE_PLAYER		2
///

#define lives_object_include_states(o, s) lives_proc_thread_include_states(o, s)
#define lives_object_exclude_states(o, s) lives_proc_thread_exclude_states(o, s)
#define lives_object_set_state(o, s) lives_proc_thread_set_state(o, s)

#define lives_object_get_state(o) lives_proc_thread_get_state(o)

uint64_t lives_object_get_type(lives_obj_t *);
uint64_t lives_object_get_subtype(lives_obj_t *);
uint64_t lives_object_get_uid(lives_obj_t *);
weed_plant_t **lives_object_get_attrs(lives_obj_t *);

int lives_attribute_get_value_int(lives_obj_attr_t *);
int lives_attr_get_value_int(lives_obj_attr_t *);
int lives_attribute_get_value_boolean(lives_obj_attr_t *);
double lives_attribute_get_value_double(lives_obj_attr_t *);
float lives_attribute_get_value_float(lives_obj_attr_t *);
int64_t lives_attribute_get_value_int64(lives_obj_attr_t *);
uint64_t lives_attribute_get_value_uint64(lives_obj_attr_t *);
char *lives_attribute_get_value_string(lives_obj_attr_t *);
char *lives_attr_get_value_string(lives_obj_t *, lives_obj_attr_t *);

// when creating the instance, we should set the intial STATE, and declare its attributes
lives_obj_attr_t *lives_object_declare_attribute(lives_obj_t *obj, const char *name, uint32_t st);
lives_obj_attr_t *lives_object_get_attribute(lives_obj_t *, const char *name);
lives_obj_attr_t **lives_object_get_attrs(lives_obj_t *);
weed_plant_t *lives_obj_instance_get_attribute(weed_plant_t *loi, const char *name);

weed_plant_t *lives_obj_instance_create(uint64_t type, uint64_t subtype);

boolean lives_object_attribute_unref(lives_obj_t *, lives_obj_attr_t *);
void lives_object_attributes_unref_all(lives_obj_t *);

/// TODO - standardise :: lives_attribute_*(obj, name, ...  and lives_attr(attr, ...

///////////// get values
char *lives_attr_get_name(lives_obj_attr_t *);
weed_seed_t lives_attr_get_value_type(lives_obj_attr_t *);

// implementation helper funcs
weed_error_t set_plant_leaf_any_type(weed_plant_t *, const char *key, uint32_t st, weed_size_t ne, ...);
weed_error_t set_plant_leaf_any_type_funcret(weed_plant_t *pl, const char *key, uint32_t st, weed_funcptr_t func);

// set by name
weed_error_t lives_object_set_attribute_value(lives_obj_t *, const char *name, ...);
weed_error_t lives_object_set_attribute_default(lives_obj_t *obj, const char *name, ...);
weed_error_t lives_object_set_attribute_array(lives_obj_t *, const char *name, weed_size_t ne, ...);
weed_error_t lives_object_set_attribute_def_array(lives_obj_t *obj, const char *name, weed_size_t ne, ...);

// set by attr
weed_error_t lives_object_set_attr_value(lives_obj_t *, lives_obj_attr_t *, ...);
weed_error_t lives_object_set_attr_default(lives_obj_t *obj, lives_obj_attr_t *attr, ...);
weed_error_t lives_object_set_attr_array(lives_obj_t *, lives_obj_attr_t *, weed_size_t ne,  ...);
weed_error_t lives_object_set_attr_def_array(lives_obj_t *obj, lives_obj_attr_t *attr,
    weed_size_t ne,  ...);

//// attr groups

/* #define lives_obj_instance_set_attr_group(loi, attrgrp)		\ */
/*   weed_set_plantptr_value(loi, LIVES_LEAF_ATTR_GRP, attrgrp) */

// each lpt has a "data" area. Any type of data can be written here
// and later recalled
weed_plant_t *lives_obj_instance_get_attr_group(lives_obj_instance_t *);
lives_result_t lives_obj_instance_set_attr_group(lives_obj_instance_t *, weed_plant_t *attrgrp);
weed_plant_t *lives_obj_instance_ensure_attr_group(lives_obj_instance_t *);
weed_plant_t *lives_obj_instance_share_attr_group(lives_obj_instance_t *, lives_obj_instance_t *src);

void lives_obj_attribute_make_static(lives_obj_instance_t *loi, const char *name);

#define mk_attr_name(name) "data_" #name

//#define DEL__VALUE(name)weed_leaf_delete(lives_obj_instance_get_attr_grp(self),name)

//#define lives_obj_instance_get_attribute(loi) (weed_get_plantptr_value((loi), LIVES_LEAF_ATTR_GROUP, NULL))

#define SET_ATTR_VALUE(loi, type, name, val) do {			\
  weed_set_##type##_value(lives_obj_instance_get_atribute(lives_obj_instance_ensure_attr_group(loi),\
							  name), liVES_LEAFE_VALUE, val);; \
    lives_obj_instance_make_static(lives_obj_instance_get_attribute((loi), name);} while(0);
#define SET_ATTR_ARRAY(loi, type, name, nvals, valsptr) do {		\
  weed_set_##type##_array(lives_obj_instance_get_attribute(loi, name),	\
			  LIVES_LEAF_VALUE, nvals, valsptr);		\
  lives_obj_attribute_make_static(lives_obj_instance_get_attribute(loi, name);} while(0);

#define GET_ATTR_VALUE(loi, type, name)		\
  lives_obj_instance_get_##type##_value(lives_obj_instance_get_attribute(loi, name), LIVES_LEAF_VALUE)

#define GET_ATTR_ARRAY(loi, type, name, nvals)			\
  lives_obj_instance_get_##type##_array(loi, name, nvals)

#define lives_obj_instance_get_int_value(loi, name)			\
  (weed_get_int_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
		      LIVES_LEAF_VALUE, NULL))

#define lives_obj_instance_get_boolean_value(loi, name)			\
  (weed_get_boolean_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			  LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_double_value(loi, name)			\
  (weed_get_double_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			 LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_string_value(loi, name)			\
  (weed_get_string_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			 LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_int64_value(loi, name)			\
  (weed_get_int64_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_funcptr_value(loi, name)			\
  (weed_get_funcptr_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			  LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_voidptr_value(loi, name)			\
  (weed_get_voidptr_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			  LIVES_LEAF_VALUE, NULL))
#define lives_obj_instance_get_plantptr_value(loi, name)		\
  (weed_get_plantptr_value(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, NULL), \
			   LIVES_LEAF_VALUE, NULL))

#define lives_obj_instance_get_int_array(loi, name, nvals)		\
  (weed_get_int_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_boolean_array(loi, name, nvals)		\
  (weed_get_boolean_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_double_array(loi, name, nvals)		\
  (weed_get_double_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_string_array(loi, name, nvals)		\
  (weed_get_string_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_int64_array(loi, name, nvals)		\
  (weed_get_int64_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_funcptr_array(loi, name, nvals)		\
  (weed_get_funcptr_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_voidptr_array(loi, name, nvals)		\
  (weed_get_voidptr_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))
#define lives_obj_instance_get_plantptr_array(loi, name, nvals)		\
  (weed_get_plantptr_array_counted(weed_get_plantptr_value(lives_obj_instance_get_attr_group(loi), name, \
						      NULL), LIVES_LEAF_VALUE, nvals))

boolean obj_attr_is_leaf_readonly(lives_obj_attr_t *, const char *key);
boolean lives_attribute_is_leaf_readonly(lives_obj_t *, const char *name, const char *key);

weed_error_t obj_attr_set_leaf_readonly(lives_obj_attr_t *, const char *key, boolean state);
weed_error_t lives_attribute_set_leaf_readonly(lives_obj_t *, const char *name,
    const char *key, boolean state);

// cast to / from lives_param_type_t
int lives_attribute_get_param_type(lives_obj_t *obj, const char *name);
weed_error_t lives_attribute_set_param_type(lives_obj_t *obj, const char *name,
    const char *label, int ptype);

int lives_object_get_num_attributes(lives_obj_t *);

char *lives_object_dump_attributes(lives_obj_t *);

void *lookup_entry_full(uint64_t uid);

char *interpret_uid(uint64_t uid);

/// NOT YET FULLY IMPLEMENTED

// update value is a transform, no attrs in, attrs out
// can be ondemand, async, volatile

// attr type object, attr type hook in -> attrs in / attrs out trig from status to status

// HAS_CAP, HAS_NOT_CAP / AND(x, y) / OR (x, y)

#define CAP_PREFIX "cap_"
#define CAP_PREFIX_LEN 4

/* typedef struct { */
/*   weed_plant_t **inputs; // inputs to function */
/*   lives_funcdef_t func_info; /// function(s) to perform the transformation */
/*   weed_plant_t **outputs; */
/* } tx_map; */
						
// a transform(ation) is a function or sequence of functions which correspond to satistying some intention for the object
// they will do one or more of the following:
// - change the state of the obejct from start_state to new_state (active transform)
// - change the object subtype (active)
// - alter the values of one or more out_params (passive transform)
// prereqs defines the rules which must be satisfied before the transform can be performed succesfully
// mappings defines the mapping of object params to actual function params
// there may be more than 1 mapping if the transform calls several functions in sequence
// out_params may have values updated by the tx
// flags gives additional info about the transform (e.g it create a new object)
//
// after examining the transform object, caller should endure that all of the reqmt. rules are satisfied
// and may then activate the transform for the object in question

// some transforms may operate on self, some may operate on other objects (the subject of the tx)
// TODO: how to determine this (perhaps a capacity ?); useful to act on multiple objects at once ?
// can we map requirements / outputs to other objects - seems like it might be nice
// separate default / value mappings for inputs ? multiple mappings for outputs ?

struct _object_transform_t {
  lives_intentcap_t icaps; // intention and capacities satisfied

  // can be a sequence of these
  lives_funcdef_t *funcdef;

  uint64_t flags;
};

#define LIVES_PLANT_CAPACITIES 512

lives_capacities_t *lives_capacities_new(void);

void lives_capacities_free(lives_capacities_t *); // calls unref

boolean lives_capacities_unref(lives_capacities_t *); // ret TRUE if freed

// if dst is NULL returns a copy, else overwirtes caps in dst and returns dst
lives_capacities_t *lives_capacities_copy(lives_capacities_t *dst, lives_capacities_t *src);

void icap_copy(lives_intentcap_t *dst, lives_intentcap_t *src);

///////////////// capacities ////

#define MAKE_CAPNAME(name) (lives_strdup_printf("%s%s", CAP_PREFIX, (name)))
#define IS_CAPNAME(name) ((!strncmp((name), CAP_PREFIX, CAP_PREFIX_LEN)))

void lives_capacity_set(lives_capacities_t *, const char *key);
void lives_capacity_unset(lives_capacities_t *, const char *key);
boolean lives_has_capacity(lives_capacities_t *, const char *key);

/* #define __ICAP_DOWNLOAD _ICAP_DOWNLOAD */
/* #define __ICAP_LOAD _ICAP_LOAD */

// e.g ICAP(LOAD) or ICAP(DOWNLOAD)

#define ICAP(type) get_std_icap(_ICAP_##type)

void make_std_icaps(void);

void lives_icap_init(lives_intentcap_t *);

void lives_thread_set_intention(lives_intention intent, lives_capacities_t *icap);
void lives_thread_set_intentcap(const lives_intentcap_t *icap);

//lives_intentcap_t *lives_intentcaps_new(int icapstype);
lives_intentcap_t *make_icap(lives_intention intent, ...) LIVES_SENTINEL;
LiVESList *find_std_icaps_v(lives_intention intention, boolean allow_fuzzy,  ...) LIVES_SENTINEL;
LiVESList *find_std_icaps(lives_intentcap_t *icap, boolean allow_fuzzy);
void lives_icap_ref(lives_intentcap_t *);
void lives_icap_unref(lives_intentcap_t *);
void lives_icap_free(lives_intentcap_t *);
const lives_intentcap_t *get_std_icap(int ref_id);
int count_caps(lives_capacities_t *icaps);
char *list_caps(lives_capacities_t *caps);

// contracts

lives_contract_t *create_contract_instance(lives_intention intent, ...);

//contract_attr_t *contract_add_attribute(lives_contract_t *, const char *aname, uint32_t atype);
//contract_attr_t *contract_has_attribute(lives_contract_t *, const char *aname);

//contract_t *contract_get_template(lives_contract_t *);
//obj *contract_get_owner(lives_contract_t *);
//obj_state *contract_get_state(lives_contract_t *);

//contract_attr_t *contract_add_hook(lives_contract_t *, status_from, status_to, in_attrs, out_attrs);
//contract_attr_t *hookattr_connect(lives_contract_t *, hook, attr_other, attr_mine, self);
//contract_attr_t *hookattr_is_connected(lives_contract_t *, hook, name);
//contract_attr_t *hookattr_get_local(lives_contract_t *, hook, name);
//contract_attr_t *hookattr_get_remote(lives_contract_t *, hook, name);
//contract_attr_t *hookattr_get_remote_owner(lives_contract_t *, hook, name);

/* boolean contract_has_attribute(lives_contract_t *, const char *aname, uint32_t atype); */

/* boolean contract_add_cap(lives_contract_t *, const char *cap); */
/* boolean contract_has_cap(lives_contract_t *, const char *cap); */
/* boolean contract_has_not_cap(lives_contract_t *, const char *cap); */
/* boolean contract_add_caps(lives_contract_t *, ...); */
/* boolean contract_has_caps(lives_contract_t *, ...); */
/* boolean contract_can_add_cap(lives_contract_t *, const char *cap); */
/* boolean contract_remove_cap(lives_contract_t *, const char *cap); */
/* boolean contract_can_add_caps(lives_contract_t *, ...); */
/* boolean contract_remove_caps(lives_contract_t *, ...); */

/* uint64_t contract_attr_get_owner(contract_attr_t *); */
/* boolean contract_attr_is_mine(contract_attr_t *); */

/* // contract attr */
/* boolean contract_attr_is_readonly(contract_attr_t *); */
/* weed_error_t contract_attr_set_readonly(contract_attr_t *, boolean state); */

/* boolean contract_attr_is_optional(contract_attr_t *); */
/* weed_error_t contract_attr_set_optional(contract_attr_t *, boolean state); */

// todo, - should take attr param
uint64_t contract_attribute_get_owner(lives_contract_t *, const char *name);
boolean contract_attribute_is_mine(lives_contract_t *, const char *name);
boolean contract_attribute_is_readonly(lives_contract_t *, const char *name);
weed_error_t contract_attribute_set_readonly(lives_contract_t *, const char *name, boolean state);

#if 0
// base functions
lives_intentcap_t **list_intentcaps(void);
get_transform_for(intentcap);
#endif

//

// the return value belongs to the object, and should only be freed with lives_transform_status_free
// will call check_requirements() then fill_requirements() if any mandatory values are missing
lives_transform_status_t *transform(lives_object_transform_t *);

//////
// derived functions

// standard match funcs (match intent and caps) would be e.g at_least, nearest, exact, excludes
// - TODO how to handle case where we need to transform state or subtype
// -- should return a list, or several lists with alts.

// note : may require change of subtype, state, status
// first we should clear the status, then convert the subtype (maybe convert state first), then convert state
// then finally convert status again

//lives_object_transform_t *find_transform_for_intentcaps(lives_obj_t *obj, lives_intentcaps icaps, lives_funct_t match_fn);

lives_object_transform_t *find_transform_for_intentcaps(lives_obj_t *, lives_intentcap_t *);

weed_param_t *weed_param_from_attribute(lives_obj_instance_t *obj, const char *name);
weed_param_t *weed_param_from_attr(lives_obj_attr_t *);

/* void lives_intentparams_free(lives_intentparams_t *); */
/* boolean lives_transform_status_free(lives_transform_status_t *); */
/* void lives_object_transform_free(lives_object_transform_t *); */

typedef lives_hash_store_t lives_objstore_t;

extern lives_objstore_t *main_objstore;
extern lives_objstore_t *fn_objstore;
extern lives_objstore_t *bdef_store;

////////////////// object broker part ///////////

// this is now a dictionary
typedef lives_obj_t lives_dicto_t;

// create a new dictionary object
lives_dicto_t *lives_dicto_new(uint64_t subtype);

// update attributes. leave rest in place
lives_dicto_t  *update_dicto(lives_dicto_t *, lives_obj_t *, ...) LIVES_SENTINEL;

// replace attributes
lives_dicto_t  *replace_dicto(lives_dicto_t *, lives_obj_t *, ...) LIVES_SENTINEL;

lives_dicto_t *weed_plant_to_dicto(weed_plant_t *);

size_t add_weed_plant_to_objstore(weed_plant_t *);

size_t add_object_to_objstore(lives_obj_t *);

size_t weigh_object(lives_obj_instance_t *obj);

const lives_funcdef_t *add_fn_lookup(lives_funcptr_t func, const char *name, int category, const char *rtype,
                                     const char *args_fmt, char *file, int linei);

boolean add_fdef_lookup(lives_funcdef_t *);

#endif // _BASE_DEFS_ONLY_ ! defined

#endif


