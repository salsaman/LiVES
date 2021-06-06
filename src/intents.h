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
//    Some values can be set readonly.

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

typedef struct _object_t lives_object_t;
typedef struct _object_t lives_object_template_t;
typedef struct _object_t lives_object_instance_t;
typedef struct _obj_status_t lives_transform_status_t;
typedef struct _obj_transform_t lives_object_transform_t;
typedef weed_param_t lives_obj_param_t;

// lives_object_t
struct _object_t {
  uint64_t uid; // unique id for this object, for static objects (templates), should be a fixed value
  // for other objects should be randomly generated for the lifetime of the object
  uint64_t type; // object type - from IMkType
  uint64_t subtype; // object subtype, can change during a transformation
  int state; // object state
  lives_obj_param_t **params; // internal parameters
  const lives_object_t *template; // pointer to template class which the instance was created from, or NULL
  void *priv; // internal data belonging to the object
};

#define OBJECT_TYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_UNDEFINED 0

typedef struct {
  uint64_t *types;
  uint64_t *subtypes;
  int *states;
} lives_obj_stance;

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
  LIVES_INTENTION_CREATE_INSTANCE = 0x00000100,
  LIVES_INTENTION_DESTROY,

  LIVES_INTENTION_ADDREF,
  LIVES_INTENTION_UNREF,

  LIVES_INTENTION_GET_VALUE,
  LIVES_INTENTION_SET_VALUE,

  // specialised intentions

  // video players
  // an intent which creates weed_layers / audio (depending on CAPACITIES) from a set of clip_like objects
  LIVES_INTENTION_PLAY = 0x00000200, // value fixed for all time, order of following must not change (see videoplugin.h)

  // like play, but with the REMOTE capacity
  LIVES_INTENTION_STREAM,  // encode / data in -> remote stream

  // alias for encode but with a weed_layer requirement
  // rather than a clip object
  LIVES_INTENTION_TRANSCODE,

  // an attatchment (?) to a player which can create an event_list object
  LIVES_INTENTION_RECORD,  // record

  // an intent which creates a new clip object with STATUS NOT_LOADED
  LIVES_INTENTION_ENCODE = 0x00000899,

  // an intent which converts a clip object's STATE from NOT_LOADED to READY
  // caps define LOCAL or REMOTE source
  LIVES_INTENTION_IMPORT = 0x00000C00,

  // TODO - just EXPORT and use LOCAL / REMOTE caps
  LIVES_INTENTION_EXPORT_LOCAL, // export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  LIVES_INTENTION_EXPORT_REMOTE, // export raw format to online location, e.g. export audio, save frame

  // intent which creates a new clip_object from an event_list
  LIVES_INTENTION_RENDER,

  LIVES_INTENTION_BACKUP, // internal clip -> restorable object
  LIVES_INTENTION_RESTORE, // restore from object -> internal clip

  // decoders
  // TODO - maybe this is IMPORT with LOCAL capacity
  // or an attachment to import / local
  LIVES_INTENTION_DECODE = 0x00001000, // combine with caps to determine e.g. decode_audio, decode_video

  // use caps to further refine e.g REALTIME / NON_REALTIME
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
// key name is defined here. Values are int32_t interprited as boolean: FALSE (0) or TRUE (1 or non-zero)
// absent values are assumed FALSE
#define LIVES_CAPACITY_LOCAL		"local"
#define LIVES_CAPACITY_REMOTE		"remote"

#define LIVES_CAPACITY_REALTIME		"realtime"

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

#define LIVES_INTENTION_MOVE LIVES_INTENTION_EXPORT_LOCAL

#define LIVES_INTENTION_UPLOAD LIVES_INTENTION_EXPORT_REMOTE
//#define LIVES_INTENTION_DOWNLOAD LIVES_INTENTION_IMPORT_REMOTE

#define LIVES_INTENTION_DELETE LIVES_INTENTION_DESTROY

// generic STATES which can be altered by *transforms*
#define OBJECT_STATE_UNDEFINED	0
#define OBJECT_STATE_NORMAL	1
#define OBJECT_STATE_PREVIEW	2
#define OBJECT_STATE_READY	3

#define OBJECT_STATE_FINALISED	512

struct _obj_status_t {
  int *status; /// pointer to an int (some states are dynamic)
  int refcount;
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


typedef struct {
  //lives_intention intent;
  //int n_params; // to simplify
  lives_obj_param_t **params; ///< (can be converted to normal params via weed_param_from_iparams)
} lives_intentparams_t;

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
weed_plant_t *obj_req_init(const char *name, lives_obj_stance stances);
weed_plant_t *hook_req_init(const char *name, int status, lives_object_transform_t *trn, void *user_data);

// rules which must be satisfied before the transformation can succeed
typedef struct {
  // list of subtypes and states the owner object must be in to run the transform
  int *subtype;
  int *state;
  lives_intentparams_t *reqs; // mix of params and object types, caller needs to set these
  // --------
  /// if caller cannot fill all values it can call a fn. and UI will run
  char ui_schema; // schema for ui, eg. "RFX 1.x.x |"
  char **uistrings; ///< strings to provide hints about constructing an interface for user entry, to get missing values
  // internal book keeping
  lives_object_instance_t *oinst; // owner instance / template
  int refcount;
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
struct _obj_transform_t {
  lives_intentcap_t icaps;
  lives_rules_t *prereqs; // pointer to the prerequisites for carrying out the transform (can be NULL)
  ///
  lives_obj_param_t **oparams;  // a mix of params and objects
  //
  int new_state; // the state of some requt object after
  uint64_t *new_subtype; // subtype after, assuming success
  ///
  tx_map *mappings; // internal mapping for the object
  uint64_t flags;
};

//
//const lives_object_template_t *lives_object_template_for_type(uint64_t type, uint64_t subtype);
const lives_object_template_t *lives_object_template_for_type(uint64_t type);

#define LIVES_WEED_SUBTYPE_CAPACITIES 512

lives_capacity_t *lives_capacities_new(void);
void lives_capacities_free(lives_capacity_t *);

#define LIVES_ERROR_NOSUCH_CAP WEED_ERROR_NOSUCH_LEAF
#define LIVES_ERROR_ICAP_NULL 65536

#define lives_capacity_exists(caps, key) (caps ? (weed_plant_has_leaf(caps, key) == WEED_TRUE ? TRUE : FALSE) \
					  FALSE)

#define lives_capacity_delete(caps, key) (caps ? weed_leaf_delete(caps, key) \
					  : LIVES_ERROR_ICAP_NULL)

#define lives_capacity_set(caps, key) (caps ? weed_set_boolean_value(caps, key, WEED_TRUE) : LIVES_ERROR_ICAP_NULL)
#define lives_capacity_set_int(caps, key, val) weed_set_int_value(caps, key, val)
#define lives_capacity_set_string(caps, key, val) weed_set_string_value(caps, key, val)

// TODO - add some error handling
#define lives_capacity_get(caps, key) (caps ? weed_get_boolean_value(caps, key, 0) == WEED_TRUE ? TRUE : FALSE : FALSE)
#define lives_capacity_get_int(caps, key) (caps ? weed_get_int_value(caps, key, 0) : 0)
#define lives_capacity_get_string(caps, key) (caps ? weed_get_string_value(caps, key, 0) : 0)

#define lives_capacity_set_readonly(caps, key, state) \
  (caps ? weed_leaf_set_flags(caps, key, weed_leaf_get_flags(caps, key) | (state ? WEED_FLAG_IMMUTABLE : 0)) \
   : LIVES_ERROR_NOSUCH_CAP)

#define lives_capacity_is_readonly(caps, key) (cap ? ((weed_leaf_get_flags(caps, key) & WEED_FLAG_IMMUTABLE) \
						      ? TRUE : FALSE) : FALSE)

void lives_intentcaps_free(lives_intentcap_t *);
lives_intentcap_t *lives_intentcaps_new(int icapstype);

#if 0
// base functions

// return a LiVESList of lives_object_transform_t
LiVESTransformList **list_transformations(lives_object_t *obj);
#endif

// check if all variables not marked optional are set, and all condition flags are TRUE, type, subtype, state, status OK
boolean requirements_met(lives_object_transform_t *);

// the return value belongs to the object, and should only be freed with lives_transform_status_free
lives_transform_status_t *transform(lives_object_t *, lives_object_transform_t *, lives_object_t **other);

//////
// derived functions

// standard match funcs (match intent and caps) would be e.g at_least, nearest, exact, excludes
// - TODO how to handle case where we need to transform state or subtype
// -- should return a list, or several lists with alts.

// note : may require change of subtype, state, status
// first we should clear the status, then convert the subtype (maybe convert state first), then convert state
// then finally convert status again

//lives_object_transform_t *find_transform_for_intentcaps(lives_object_t *obj, lives_intentcaps icaps, lives_funct_t match_fn);

lives_object_transform_t *find_transform_for_intent(lives_object_t *obj, lives_intention intent);

boolean rules_lack_param(lives_rules_t *req, const char *pname);
//boolean rules_lack_condition(lives_rules_t *req, int condition number);
// type mismatch, subtype mismatch, state mismatch, status mismatch

// convert an iparam to a regular weed_param
weed_param_t *weed_param_from_iparams(lives_intentparams_t *iparams, const char *name);

#if 0
const lives_transform_status_t *get_current_status(lives_object_t *obj);
void lives_intentcaps_free(lives_intcaps_t **icap_pp);
void lives_rules_ref(lives_rules_t *rules_pp);
void lives_transform_list_free(LiVESTransformList **transform_list_pp);
void lives_tx_map_free(LiVESTransformList **transform_list_pp);
#endif

void lives_intentparams_free(lives_intentparams_t *p);
void lives_transform_status_free(lives_transform_status_t *st);
void lives_object_transform_free(lives_object_transform_t *tx);

#endif

