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

#define ashift(v0, v1) ((uint64_t)(((v0) << 8) | (v1)))
#define ashift4(a, b, c, d) ((uint64_t)(((ashift((a), (b)) << 16) | (ashift((c), (d))))))
#define ashift8(a, b, c, d, e, f, g, h) ((uint64_t)(((ashift4((a), (b), (c), (d))) << 32) | \
							      ashift4((e), (f), (g), (h))))
#define IMkType(str) ((uint64_t)(ashift8((str)[0], (str)[1], (str)[2], (str)[3], \
					       (str)[4], (str)[5], (str)[6], (str)[7])))

#define LIVES_OBJECT(o) ((lives_object_t *)((o)))

// for external object data (e.g g_obect)
#define INTENTION_KEY "intention_"

// example types
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP")  // could do with a more generic name
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG"
#define OBJECT_TYPE_WIDGET		IMkType("obj.WDGT")
#define OBJECT_TYPE_THREAD		IMkType("obj.THRD")

#define LIVES_SEED_HOOK WEED_SEED_FUNCPTR
#define LIVES_SEED_OBJECT 2048

typedef lives_funcptr_t object_funcptr_t;
typedef struct _object_t lives_object_t;
typedef struct _object_t lives_object_instance_t;
typedef struct _obj_status_t lives_transform_status_t;
typedef struct _obj_transform_t lives_object_transform_t;
typedef weed_param_t lives_tx_param_t;
typedef weed_param_t lives_obj_attr_t;

#define HOOK_CB_SINGLE_SHOT		(1 << 1) //< hook function should be called only once then removed
#define HOOK_CB_ASYNC			(1 << 2) //< hook function should not block
#define HOOK_CB_ASYNC_JOIN		(1 << 3) //< hook function should not block, but the thread should be joined
///							at the end of processing, or before calling the hook
///							a subsequent time
#define HOOK_CB_CHILD_INHERITS		(1 << 4) // TODO - child threads should inherit the hook callbacks

#define HOOK_BLOCKED			(1 << 16) // hook function should not be called

#define HOOK_UNIQUE_FUNC		(1 << 24) // do not add if func already in hooks

#define HOOK_UNIQUE_DATA		(1 << 25) // do not add if data already in hooks (UNIQUE_FUNC assumed)
#define HOOK_UNIQUE_REPLACE		(1 << 26) // remove other entries with same func and add this (UNIQUE_FUNC assumed)


#define HOOK_UNIQUE_CHANGE_DATA 	(HOOK_UNIQUE_DATA | HOOK_UNIQUE_REPLACE) // change data but leave func

#define HOOK_UNIQUE_REPLACE_FUNC	(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_REPLACE) // replace other entries with same func

// replace other entries having same func and data
#define HOOK_UNIQUE_REPLACE_MATCH	(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_DATA | HOOK_UNIQUE_REPLACE)

enum {
  ABORT_HOOK, ///< can be set to point to a function to be run before abort, for critical functions
  EXIT_HOOK,
  THREAD_EXIT_HOOK, // run on thread exit
  THREAD_INTERNAL_HOOK, /// reserved for internal use, should not be modified
  N_GLOBAL_HOOKS,
  ///
  TX_PRE_HOOK,
  TX_START_HOOK, /// status -> running
  TX_POST_HOOK,
  TX_DONE_HOOK,   /// status -> success
  DATA_PREP_HOOK,   // data supplied, may be altered
  DATA_READY_HOOK, // data ready for processing
  VALUE_CHANGED_HOOK, /// attribute value amended
  FINAL_HOOK, ///< about to be freed
  N_HOOK_FUNCS,
};

typedef void *(*hook_funcptr_t)(lives_object_t *, void *);
typedef void (*attr_listener_f)(lives_object_t *, lives_obj_attr_t *);

typedef struct {
  hook_funcptr_t func;
  lives_object_t *obj;
  void *attr;
  void *data;
  uint64_t flags;
  lives_proc_thread_t tinfo; // for async_join
} lives_closure_t;

typedef struct {
  int count; // if count < 0, object should be destroyed
  pthread_mutex_t mutex;
  boolean mutex_inited;
} obj_refcounter;

int refcount_inc(obj_refcounter *);
int refcount_dec(obj_refcounter *);
int refcount_query(obj_refcounter *);

// lives_object_t
struct _object_t {
  uint64_t uid; // unique id for this instnace (const)
  obj_refcounter refcount;
  uint64_t type; // object type - from IMkType (const)
  uint64_t subtype; // object subtype, can change during a transformation
  int state; // object state
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

#define OBJECT_TYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_UNDEFINED 0

enum {
  // some common intentions
  // internal or (possibly) non-functional types
  LIVES_INTENTION_UNKNOWN,

  // application types
  LIVES_INTENTION_NOTHING,
  LIVES_INTENTION_UNDO,
  LIVES_INTENTION_REDO,
  LIVES_INTENTION_RUN, // request status update to "running"
  LIVES_INTENTION_CANCEL, // requests an object with status "running" to transform to status "cancelled"

  // function like
  // MANDATORY for templates which can create instances
  LIVES_INTENTION_CREATE_INSTANCE = 0x00000100, // create instance of type / subtype

  // MANDATORY (builtin) for instances
  LIVES_INTENTION_ADDREF,

  // MANDATORY (builtin) for instances
  LIVES_INTENTION_UNREF,

  LIVES_INTENTION_GET_VALUE,
  LIVES_INTENTION_SET_VALUE,

  // an intent which converts an object's STATE from PREPARE to NORMAL
  LIVES_INTENTION_PREPARE = 0x00000200,

  // an intent which converts an object's STATE from EXTERNAL to NORMAL
  // caps define LOCAL or REMOTE source
  LIVES_INTENTION_IMPORT = 0x00000C00,

  // an intent which creates a copy object in STATE EXTERNAL
  // with LOCAL - export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  // with REMOTE - export raw format to online location, e.g. export audio, save frame
  LIVES_INTENTION_EXPORT,

  // specialised intentions

  // video players
  // an intent which creates media_output from an array of media_inputs
  // or from an event_list object
  LIVES_INTENTION_PLAY = 0x00000200, // value fixed for all time, order of following must not change (see videoplugin.h)

  // like play, but with the REMOTE capacity (can also be an attachment to PLAY)
  LIVES_INTENTION_STREAM,  // encode / data in -> remote stream

  // alias for encode but with a weed_layer (frame ?) requirement
  // rather than a clip object (could also be an attachment to PLAY with realtime == FALSE and display == FALSE caps)
  // media_output is created with state EXTERNAL
  LIVES_INTENTION_TRANSCODE,

  // an attachment (?) to a player which can create an event_list object
  LIVES_INTENTION_RECORD,  // record

  // intent which creates a new clip_object from an event_list
  // media_src with state INTERNAL
  // (can also be an attachment to PLAY, with non-realtime, non-display and output clip in state READY)
  LIVES_INTENTION_RENDER,

  // an intent which creates a new clip object with STATE EXTERNAL
  // alias for EXPORT for clip objects ?
  // actually this can just be PLAY but with non-realtime and non-display (like transcode)
  LIVES_INTENTION_ENCODE = 0x00000899,

  // these may be specialised for clip objects

  LIVES_INTENTION_BACKUP, // internal clip -> restorable object
  LIVES_INTENTION_RESTORE, // restore from object -> internal clip

  // decoders
  // this is a specialized intent for clip objects, for READY objects, produces frame objects from the clip object)
  // media_src with realtime / non-realtime CAPS
  LIVES_INTENTION_DECODE = 0x00001000, // combine with caps to determine e.g. decode_audio, decode_video

  // use caps to further refine e.g REALTIME / NON_REALTIME (can be attachment to PLAY ?)
  LIVES_INTENTION_EFFECT = 0x00001400,

  // do we need so many ? maybe these can become CAPS
  LIVES_INTENTION_ANALYSE,
  LIVES_INTENTION_CONVERT,
  LIVES_INTENTION_MIX,
  LIVES_INTENTION_SPLIT,
  LIVES_INTENTION_DUPLICATE,

  LiVES_INTENTION_FIRST_CUSTOM = 0x80000000,
  LIVES_INTENTION_MAX = 0xFFFFFFFF
};

// generic capacities, type specific ones may also exist
// key name is defined here. Values are int32_t interpreted as boolean: FALSE (0) or TRUE (1 or non-zero)
// absent values are assumed FALSE
//#define LIVES_CAPACITY_LOCAL		"local"
#define LIVES_CAPACITY_REMOTE		"remote"

#define LIVES_CAPACITY_REALTIME		"realtime"
#define LIVES_CAPACITY_DISPLAY		"display" // provides some type of (local) display output

#define LIVES_CAPACITY_VIDEO		"video"
#define LIVES_CAPACITY_AUDIO		"audio"
#define LIVES_CAPACITY_TEXT		"text"

#define LIVES_CAPACITY_DATA		"data"

/////// intentcaps //

#define LIVES_ICAPS_LOAD LIVES_INTENTION_IMPORT
#define LIVES_ICAPS_DOWNLOAD LIVES_INTENTION_IMPORT + 1

#define LIVES_INTENCAP_IS_IMPORT_LOCAL(icap) (icap->intent = LIVE_INTENTION_IMPORT && lives_get_capacity(LIVES_CAPACITY_LOCAL))
#define LIVES_INTENCAP_IS_IMPORT_REMOTE(icap) (icap->intent = LIVE_INTENTION_IMPORT && lives_get_capacity(LIVES_CAPACITY_REMOTE))

// aliases
#define LIVES_INTENTION_IGNORE LIVES_INTENTION_NOTHING
#define LIVES_INTENTION_LEAVE LIVES_INTENTION_NOTHING
#define LIVES_INTENTION_SKIP LIVES_INTENTION_NOTHING

#define  LIVES_INTENTION_DESTROY_INSTANCE LIVES_INTENTION_UNREF

// or maybe just set value with workdir param for LiVES object ?
#define LIVES_INTENTION_MOVE LIVES_INTENTION_EXPORT

#define LIVES_INTENTION_UPLOAD LIVES_INTENTION_EXPORT // with "local" set to FALSE

//#define LIVES_INTENTION_DOWNLOAD LIVES_INTENTION_IMPORT_REMOTE

#define LIVES_INTENTION_DELETE LIVES_INTENTION_DESTROY_INSTANCE

// generic STATES which can be altered by *transforms*
#define OBJECT_STATE_UNDEFINED	0
#define OBJECT_STATE_NORMAL	1
#define OBJECT_STATE_PREPARE	2

#define OBJECT_STATE_PREVIEW	3 // ???

#define OBJECT_STATE_EXTERNAL	64

#define OBJECT_STATE_FINALISED	512

#define OBJECT_STATE_NOT_READY OBJECT_STATE_PREPARE
#define OBJECT_STATE_READY OBJECT_STATE_NORMAL

struct _obj_status_t {
  int *status; /// pointer to an int (some states are dynamic)
  obj_refcounter refcount;
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

/* typedef struct { */
/*   int type; */
/*   // flags may include READONLY, OPTIONAL, GUI */
/*   int flags; */
/*   union { */
/*     weed_param_t *param; // can be free standing or point to an attribute */
/*     lives_object_t *object; // maybe hook is enough ?? */
/*     hook_func hook; */
/*   } */
/* } tx_req_t; */


typedef struct {
  //lives_intention intent;
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

// when creating the instance, we should set the intial STATE, and declare its attributes
lives_obj_attr_t *lives_object_declare_attribute(lives_object_t *, const char *name, int stype);

lives_obj_attr_t *lives_object_get_attribute(lives_object_t *, const char *name);

// TODO - add listeners for attribute value changes (also for subtype / state changes)

// values can be set later
weed_error_t lives_object_set_attribute_value(lives_object_t *, const char *name, ...);
weed_error_t lives_object_set_attribute_array(lives_object_t *, const char *name, int n_elems, ...);

//// placeholder - should operate on transform requirements instead
weed_error_t lives_attribute_set_readonly(lives_object_t *, const char *name, boolean state);
boolean lives_attribute_is_readonly(lives_object_t *, const char *name);

// cast to / from lives_param_type_t
int lives_attribute_get_param_type(lives_object_t *obj, const char *name);
weed_error_t lives_attribute_set_param_type(lives_object_t *obj, const char *name,
    const char *label, int ptype);

void lives_attribute_append_listener(lives_object_t *obj, const char *name, attr_listener_f func);
void lives_attribute_prepend_listener(lives_object_t *obj, const char *name, attr_listener_f func);

int lives_object_get_num_attributes(lives_object_t *);

int lives_object_dump_attributes(lives_object_t *);

/// NOT YET FULLY IMPLEMENTED

// transform status
#define LIVES_TRANSFORM_ERROR_REQ -1 // not all requirements met (e.g. params with no values and no way to
//  find them, objects of wrong type / subtype / state)
#define LIVES_TRANSFORM_STATUS_RUNNING 0 ///< transform is "running" and the state cannot be changed
#define LIVES_TRANSFORM_STATUS_NEEDS_DATA 1 ///< reqmts. need updating
#define LIVES_TRANSFORM_STATUS_SUCCESS 2 ///< normal / success
#define LIVES_TRANSFORM_STATUS_CANCELLED 3 ///< transform was cancelled during running
#define LIVES_TRANSFORM_STATUS_ERROR 4 ///< transform encountered an error during running

weed_plant_t *int_req_init(const char *name, int def, int min, int max);
weed_plant_t *boolean_req_init(const char *name, int def);
weed_plant_t *double_req_init(const char *name, double def, double min, double max);
weed_plant_t *string_req_init(const char *name, const char *def);
weed_plant_t *obj_req_init(const char *name, lives_obj_template allowed);
weed_plant_t *hook_req_init(const char *name, int status, lives_object_transform_t *trn, void *user_data);

typedef struct {
  int category; // category type for function (0 for none)
  lives_funcptr_t function; // pointer to a function
  char *args_fmt; // definition of the params, e.g. "idV" (int, double, void *)
  uint32_t rettype; // weed_seed type e.g. WEED_SEED_INT, a value of 0 implies a void *(fuunc)
  void *data; // category specific data, may be NULL
  uint64_t padding[3];
} lives_func_info_t;

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
  obj_refcounter refcount;
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
  int *obj_in_mapping; // maps reqts -> internal variables (can also be internal vars in other objects) (weed / weed) or obj / obj
  int *fn_in_mapping; // maps internal variables -> fn params (weed or obj / idx)
  lives_func_info_t func_info; /// function(s) to perform the transformation
  int *fn_out_mapping; // maps fn variables -> internal params (ptr to var -> weed or obj)
  int *obj_out_mapping; // maps internal params -> out params (weed / weed) or obj / obj
  // hook mappings ???
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

  // inputs
  lives_rules_t *prereqs; // pointer to the prerequisites for carrying out the transform (can be NULL)

  // outputs
  /// first element is always SELF (or NULL if self is unchanged)
  lives_tx_param_t **oparams;  // a mix of params and objects (type / subtype / state)
  ///
  tx_map *mappings; // internal mapping for the object
  uint64_t flags;
};

#define LIVES_WEED_SUBTYPE_CAPACITIES 512

lives_capacity_t *lives_capacities_new(void);
void lives_capacities_free(lives_capacity_t *);

#define LIVES_ERROR_NOSUCH_CAP WEED_ERROR_NOSUCH_LEAF
#define LIVES_ERROR_NOSUCH_ATTRIBUTE WEED_ERROR_NOSUCH_LEAF
#define LIVES_ERROR_ICAP_NULL 66000

#define LIVES_CAPACITY_TRUE(caps, key) ((caps) ? weed_get_boolean_value((caps), (key)) == WEED_TRUE : FALSE)
#define LIVES_CAPACITY_FALSE(caps, key) ((caps) ? weed_get_boolean_value((caps), (key), NULL) == WEED_FALSE \
					 ? TRUE : FALSE : FALSE)

#define LIVES_CAPACITY_NEGATED(caps, key) ((caps) ? lives_capacity_exists((caps), (key)) \
					 ? weed_get_boolean_value((caps), (key)) == WEED_TRUE \
					 ? TRUE : FALSE : FALSE : FALSE)

#define LIVES_HAS_CAPACITY(caps, key) (lives_capacity_exists((caps), (key)))

#define lives_capacity_exists(caps, key) ((caps) ? (weed_plant_has_leaf((caps), (key)) == WEED_TRUE ? TRUE : FALSE) \
					  FALSE)

#define lives_capacity_delete(caps, key) ((caps) ? weed_leaf_delete((caps), (key)) : LIVES_ERROR_ICAP_NULL)

#define lives_capacity_set(caps, key) ((caps) ? weed_set_boolean_value((caps), (key), WEED_TRUE) : LIVES_ERROR_ICAP_NULL)
#define lives_capacity_unset(caps, key) ((caps) ? weed_set_boolean_value((caps), (key), WEED_FALSE) : LIVES_ERROR_ICAP_NULL)

#define lives_capacity_set_int(caps, key, val) weed_set_int_value((caps), (key), val)
#define lives_capacity_set_string(caps, key, val) weed_set_string_value((caps), (key), val)

// TODO - add some error handling
#define lives_capacity_get(caps, key) ((caps) ? weed_get_boolean_value((caps), (key), 0) == WEED_TRUE ? TRUE : FALSE : FALSE)
#define lives_capacity_get_int(caps, key) ((caps) ? weed_get_int_value((caps), (key), 0) : 0)
#define lives_capacity_get_string(caps, key) ((caps) ? weed_get_string_value((caps), (key), 0) : 0)

#define lives_capacity_set_readonly(caps, key, state) \
  ((caps) ? weed_leaf_set_flags((caps), (key), weed_leaf_get_flags((caps), (key)) | (state ? WEED_FLAG_IMMUTABLE : 0)) \
   : LIVES_ERROR_NOSUCH_CAP)

#define lives_capacity_is_readonly(caps, key) (cap ? ((weed_leaf_get_flags((caps), (key)) & WEED_FLAG_IMMUTABLE) \
						      ? TRUE : FALSE) : FALSE)

void lives_intentcaps_free(lives_intentcap_t *);
lives_intentcap_t *lives_intentcaps_new(int icapstype);

#if 0
// base functions
lives_intentcaps_t **list_intentcaps(void);
get_transform_for(intentcap);
#endif

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

// convert an iparam to a regular weed_param
weed_param_t *weed_param_from_iparams(lives_intentparams_t *, const char *name);
weed_param_t *weed_param_from_object(lives_object_t *j, const char *name);

#if 0
const lives_transform_status_t *get_current_status(lives_object_t *);
void lives_intentcaps_free(lives_intcaps_t **);
void lives_rules_ref(lives_rules_t *);
void lives_transform_list_free(LiVESTransformList **);
void lives_tx_map_free(LiVESTransformList **);
#endif

void lives_intentparams_free(lives_intentparams_t *);
boolean lives_transform_status_free(lives_transform_status_t *);
void lives_object_transform_free(lives_object_transform_t *);

#endif

