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
// - transforms also have hooks, these allow some parameters to be updated internally during processing
//    some hooks can be mandatory as part of the requts. some objects will have "attachments" which can connect to hooks
//     'needs_data' status corresponds to a transform waiting for a hook to return new data
//
// - some requirements can be objects with a specific type / subtype(s) / state(s)
// - reqs, can be optional, for some, caller can change the default, and let user / or an object provide the value.
//    Some values n be set readonly.

#ifndef HAS_LIVES_INTENTS_H
#define HAS_LIVES_INTENTS_H

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
#define IMkType(str) ((uint64_t)(ashift8((str)[0], (str)[1], (str)[2], (str)[3], \
					       (str)[4], (str)[5], (str)[6], (str)[7])))

#define LIVES_OBJECT(o) ((lives_object_t *)((o)))

typedef union {
  uint8_t c[8];
  uint64_t u;
} charbytes;

#define IMKType2(type) do {\
  charbtytes c;		   \
  snprintf(c.c, 8, "%s".#type); \
  return c.u;			\
  } while (0);

// for external object data (e.g g_object)
#define INTENTION_KEY "intention_"

#define LIVES_SEED_OBJECT 2048

typedef lives_funcptr_t object_funcptr_t;
typedef struct _object_t lives_object_t;
typedef struct _object_t lives_object_instance_t;
typedef struct _obj_status_t lives_transform_status_t;
typedef struct _obj_transform_t lives_object_transform_t;
typedef weed_param_t lives_tx_param_t;
typedef weed_plant_t lives_obj_attr_t;

#define HOOKFUNCS_ONLY
#include "threading.h"
#ifdef HOOKFUNCS_ONLY
#undef HOOKFUNCS_ONLY
#endif

typedef lives_refcounter_t obj_refcounter;

// lives_object_t
struct _object_t {
  uint64_t uid; // unique id for this instnace (const)
  obj_refcounter refcounter;
  uint64_t type; // object type - from IMkType (const)
  uint64_t subtype; // object subtype, can change during a transformation
  int state; // object state
  lives_intentcap_t icap;
  lives_obj_attr_t **attributes; // internal parameters (properties)
  lives_object_transform_t *active_tx; // pointer to currently running transform (or NULL)
  LiVESList *hook_closures[N_HOOK_FUNCS]; /// TODO - these should probably be part of active_tx
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

typedef struct {
  // for each enumnerated object type, we will define a set of subtypedefs
  uint64_t type; // object type - from IMkType
  struct objsubdef **subtypedefs; // NULL term. list
} lives_obj_template;

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
/// LIVES_INTENTION_DISPLAY
// mand att: layer
//
// GET_SET_VALUES: width heigth, sepwin

/// transforms

// TODO - link these to transform requirements / inputs / outputs
typedef struct {
  lives_tx_param_t **params; ///< (can be converted to normal params via weed_param_from_iparams)
} lives_intentparams_t;

// shorthand for calling LIVES_INTENTION_CREATE_INSTANCE in the template
lives_object_instance_t *lives_object_instance_create(uint64_t type, uint64_t subtype);

// shorthand for calling LIVES_INTENTION_UNREF in the instance
boolean lives_object_instance_destroy(lives_object_instance_t *);

// shorthand for calling LIVES_INTENTION_UNREF in the instance
int lives_object_instance_unref(lives_object_instance_t *);

// shorthand for calling LIVES_INTENTION_REF in the instance
int lives_object_instance_ref(lives_object_instance_t *);

#define LIVES_LEAF_OWNER "owner_uid"

// when creating the instance, we should set the intial STATE, and declare its attributes
lives_obj_attr_t *lives_object_declare_attribute(lives_object_t *, const char *name, uint stype);

lives_obj_attr_t *lives_object_get_attribute(lives_object_t *, const char *name);

boolean lives_object_attribute_unref(lives_object_t *, lives_obj_attr_t *);
void lives_object_attributes_unref_all(lives_object_t *);

/// TODO - standardise :: lives_attribute_*(obj, name, ...  and lives_attr(attr, ...

///////////// get values
char *lives_attr_get_name(lives_obj_attr_t *);
int lives_attr_get_value_int(lives_obj_attr_t *);
uint32_t obj_attr_get_value_type(lives_obj_attr_t *);
uint64_t obj_attr_get_owner(lives_obj_attr_t *);
boolean obj_attr_is_mine(lives_obj_attr_t *);
boolean obj_attr_is_readonly(lives_obj_attr_t *);
weed_error_t obj_attr_set_readonly(lives_obj_attr_t *, boolean state);

// todo, - should take attr param
uint64_t lives_attribute_get_owner(lives_object_t *, const char *name);
boolean lives_attribute_is_mine(lives_object_t *, const char *name);
boolean lives_attribute_is_readonly(lives_object_t *, const char *name);
weed_error_t lives_attribute_set_readonly(lives_object_t *, const char *name, boolean state);

// values can be set later
weed_error_t lives_object_set_attribute_value(lives_object_t *, const char *name, ...);
weed_error_t lives_object_set_attribute_default(lives_object_t *obj, const char *name, ...);
weed_error_t lives_object_set_attribute_array(lives_object_t *, const char *name, weed_size_t ne, ...);
weed_error_t lives_object_set_attribute_def_array(lives_object_t *obj, const char *name, weed_size_t ne, ...);

weed_error_t lives_object_set_attr_value(lives_object_t *, lives_obj_attr_t *, ...);
weed_error_t lives_object_set_attr_default(lives_object_t *obj, lives_obj_attr_t *attr, ...);
weed_error_t lives_object_set_attr_array(lives_object_t *, lives_obj_attr_t *, weed_size_t ne,  ...);
weed_error_t lives_object_set_attr_def_array(lives_object_t *obj, lives_obj_attr_t *attr,
    weed_size_t ne,  ...);

boolean obj_attr_is_leaf_readonly(lives_obj_attr_t *, const char *key);
boolean lives_attribute_is_leaf_readonly(lives_object_t *, const char *name, const char *key);

weed_error_t obj_attr_set_leaf_readonly(lives_obj_attr_t *, const char *key, boolean state);
weed_error_t lives_attribute_set_leaf_readonly(lives_object_t *, const char *name,
    const char *key, boolean state);

// cast to / from lives_param_type_t
int lives_attribute_get_param_type(lives_object_t *obj, const char *name);
weed_error_t lives_attribute_set_param_type(lives_object_t *obj, const char *name,
    const char *label, int ptype);

// listeners can attach to the pre_/ post_ value_changed hooks for the object
void lives_attribute_append_listener(lives_object_t *obj, const char *name, attr_listener_f func);
void lives_attribute_prepend_listener(lives_object_t *obj, const char *name, attr_listener_f func);

int lives_object_get_num_attributes(lives_object_t *);

char *lives_object_dump_attributes(lives_object_t *);

void *lookup_entry_full(uint64_t uid);

char *interpret_uid(uint64_t uid);

/// NOT YET FULLY IMPLEMENTED


// update value is a transform, no attrs in, attrs out
// can be ondemand, async, volatile

// attr type object, attr type hook in -> attrs in / attrs out trig from status to status

// HAS_CAP, HAS_NOT_CAP / AND(x, y) / OR (x, y)

// rules which must be satisfied before the transformation can succeed
typedef struct {
  // list of subtypes and states the owner object must be in to run the transform
  int *subtype;
  int *state;

  // TODO - use tx_req_t **

  lives_intentparams_t *reqs;
  // --------
  // internal book keeping
  lives_object_instance_t *oinst; // owner instance / template
  obj_refcounter refcounter;
} lives_rules_t;

// flagbits for transformations
// combine : child for template, copy for instance
#define TR_FLAGS_CREATES_CHILD (1 << 0) // creates child instance from template
#define TR_FLAGS_CREATES_COPY  (1 << 1) // creates a copy of an instance with a different uid

// remove: use list
#define TR_FLAGS_CHANGES_SUBTYPE  (1 << 2) // object subtype may change during the transformation

#define TR_FLAGS_FINAL (1 << 3) // state is final after tx, and can be destroyed
#define TR_FLAGS_INSTANT (1 << 4) // must immediately be followed by another transformation (should the object be able to specify it
/// 					e,g a cleanup to be run after processing, or just define it ?)

#define TR_FLAGS_DIRECT (1 << 32) // function may be called directly (this is a transitional step
// which may be deprecated

// maps in / out params to actual function parameters
typedef struct {
  weed_plant_t **inputs; // inputs to function
  lives_funcdef_t func_info; /// function(s) to perform the transformation
  weed_plant_t **outputs;
} tx_map;

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

struct _obj_transform_t {
  lives_intentcap_t icaps; // intention and capacities satisfied

  // can be a sequence of these
  lives_rules_t *prereqs; // pointer to the prerequisites for carrying out the transform (can be NULL)
  lives_funcdef_t *funcdef;

  uint64_t flags;
};

#define LIVES_WEED_SUBTYPE_CAPACITIES 512

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

/////////////// object broker ////
/* const lives_funcdef_t *get_template_for_func(funcidx_t funcidx); */
/* const lives_funcdef_t *get_template_for_func_by_uid(uint64_t uid); */
/* char *get_argstring_for_func(funcidx_t funcidx); */
char *lives_funcdef_explain(const lives_funcdef_t *funcdef);

#if 0
// base functions
lives_intentcap_t **list_intentcaps(void);
get_transform_for(intentcap);
#endif

//

// check all requmnts and mark - filled (value set) / can_fill (value unset, but has means to obtain) / missing / optional (unset)
// value readonly (constant) or variable, default readonly or variable
boolean check_requirements(lives_object_transform_t *);

// as above, but any can_fill values will become filled, unless the value is readonly
boolean fill_requirements(lives_object_transform_t *);

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

//lives_object_transform_t *find_transform_for_intentcaps(lives_object_t *obj, lives_intentcaps icaps, lives_funct_t match_fn);

lives_object_transform_t *find_transform_for_intentcaps(lives_object_t *, lives_intentcap_t *);

boolean rules_lack_param(lives_rules_t *, const char *pname);
//boolean rules_lack_condition(lives_rules_t *, int condition number);
// type mismatch, subtype mismatch, state mismatch, status mismatch

// convert an attribute to a regular weed_param

weed_param_t *weed_param_from_attribute(lives_object_instance_t *, const char *name);
weed_param_t *weed_param_from_attr(lives_obj_attr_t *);

#if 0
const lives_transform_status_t *get_current_status(lives_object_t *);
void lives_rules_ref(lives_rules_t *);
void lives_transform_list_free(LiVESTransformList **);
void lives_tx_map_free(LiVESTransformList **);
#endif

void lives_intentparams_free(lives_intentparams_t *);
boolean lives_transform_status_free(lives_transform_status_t *);
void lives_object_transform_free(lives_object_transform_t *);

////////////////// object broker part ///////////
typedef lives_hash_store_t lives_objstore_t;

// this is now a dictionary
typedef lives_object_instance_t lives_dicto_t;

// create a new dictionary object
lives_dicto_t *lives_dicto_new(uint64_t subtype);

// update attributes. leave rest in place
lives_dicto_t  *update_dicto(lives_dicto_t *, lives_object_t *, ...) LIVES_SENTINEL;

// replace attributes
lives_dicto_t  *replace_dicto(lives_dicto_t *, lives_object_t *, ...) LIVES_SENTINEL;

lives_dicto_t *weed_plant_to_dicto(weed_plant_t *);

const lives_funcdef_t *add_fn_lookup(lives_funcptr_t func, const char *name, const char *rtype,
                                     const char *args_fmt, char *file, int linei, void *txmap);

size_t add_weed_plant_to_objstore(weed_plant_t *);

size_t add_object_to_objstore(lives_object_t *);

size_t weigh_object(lives_object_instance_t *obj);

#endif // _BASE_DEFS_ONLY_ ! defined

#endif


