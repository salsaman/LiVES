// intents.h
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// this is highly experimental code, and may change radically

// features (potential)
// - formalises function prototypes so their argument types and return types can be know at runtime
// - using intents allows different subtypes to call different functions for the same intent
// - within one object, same intent can be mapped to various functions depending on the arguments and return type
// - provides a convenient location for gathering function arguments and storing the return value(s)
// - objects can be "smart" and provide defaults for arguments, query other objects, or get missing values from the user
// - arguments can be mapped to things other than simple parameters, for example "self", "self.fundamental"
// - allows functions to be chained in sequence with outputs from one feeding into the next
// - provides for verifying that a set of conditions is satisfied before the function(s) are called
// - objects can list capabilities which depend on the intent and object state
// - a status can be returned after any step in the transform - new requirements and conditions can be added
// - the transform returns a status which can be updated dynamically (e,g waiting, needs_data)
// - allows for the possibility of object - object communication and data sharing
// - enables goal based activities based on a sequence of state changes from the current state to the goal state
// - permits the creation of functions which can handle various object types / subtypes differently
// - allows for "factory" type templates which can create instances of various subtypes
// - modular implementation allows for the various components to be used independantly (e.g "intention" can be used anywhere
//		like an enum)
//

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
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP")
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG"
#define OBJECT_TYPE_WIDGET		IMkType("obj.WIDG")

typedef struct _object_t lives_object_t;
typedef struct _object_t lives_object_template_t;
typedef struct _object_t lives_object_instance_t;
typedef struct _obj_status lives_object_status_t;

// lives_object_t
struct _object_t {
  uint64_t uid; // unique id for this object, for static objects (templates), should be a fixed value
  // for other objects should be randomly generated for the lifetime of the object
  uint64_t type; // object type - from IMkType
  uint64_t subtype; // object subtype, can change during a transformation
  const lives_object_t *template; // pointer to template class, or NULL
  lives_object_status_t *status; // pointer to status struct
  int n_params;  // internal params for the object - may be mapped to transform reqs. / out params
  weed_param_t **params;
  void *priv; // internal data belonging to the object
};

enum {
  LIVES_INTENTION_UNKNOWN,
  LIVES_INTENTION_IDENTITY, // do nothing

  // some default intentions
  // function like
  LIVES_INTENTION_CREATE_INSTANCE,
  LIVES_INTENTION_DESTROY,

  LIVES_INTENTION_ADDREF,
  LIVES_INTENTION_UNREF,

  LIVES_INTENTION_GET_VALUE,
  LIVES_INTENTION_SET_VALUE,

  // internal type actions
  LIVES_INTENTION_UNDO,
  LIVES_INTENTION_REDO,

  // video players
  LIVES_INTENTION_PLAY = 0x00000200,
  LIVES_INTENTION_STREAM,  // encode / data in -> remote stream
  LIVES_INTENTION_TRANSCODE,  // encode / data in -> external file format
  LIVES_INTENTION_ENCODE, // encode / file in -> external file format
  LIVES_INTENTION_RENDER, // data -> internal clip

  // clip-like objects (TBD. - part of clip object or should there be a clip_manager static object ?)
  LIVES_INTENTION_IMPORT_LOCAL, // import from local filesystem, -> internal clips  i.e "open"
  LIVES_INTENTION_IMPORT_REMOTE, // import from online source  -> internal clip "download"
  LIVES_INTENTION_EXPORT_LOCAL, // export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  LIVES_INTENTION_EXPORT_REMOTE, // export raw format to online location, e.g. export audio, save frame

  LIVES_INTENTION_BACKUP, // internal clip -> restorable object
  LIVES_INTENTION_RESTORE, // restore from object -> internal clip

  // use caps to further refine e.g REALTIMNE / NON_REALTIME
  LIVES_INTENTION_EFFECT,
  LIVES_INTENTION_ANALYSE,
  LIVES_INTENTION_CONVERT,
  LIVES_INTENTION_MIX,
  LIVES_INTENTION_SPLIT,
  LIVES_INTENTION_DUPLICATE,

  LiVES_INTENTION_FIRST_CUSTOM = 0x80000000,
  LIVES_INTENTION_MAX = 0xFFFFFFFF
};

// avoiding using an enum allows the list to be extended in other headers
typedef uint32_t lives_intention;

/// type specific caps
// vpp
//"general"
#define VPP_CAN_RESIZE    (1<<0)
#define VPP_CAN_RETURN    (1<<1)
#define VPP_LOCAL_DISPLAY (1<<2)
#define VPP_LINEAR_GAMMA  (1<<3)
#define VPP_CAN_RESIZE_WINDOW          		(1<<4)   /// can resize the image to fit the play window
#define VPP_CAN_LETTERBOX                  	(1<<5)
#define VPP_CAN_CHANGE_PALETTE			(1<<6)

// encoder
//"general"
#define HAS_RFX (1<<0)
#define CAN_ENCODE_PNG (1<<2)
#define ENCODER_NON_NATIVE (1<<3)

//"acodecs"
#define AUDIO_CODEC_MP3 0
#define AUDIO_CODEC_PCM 1
#define AUDIO_CODEC_MP2 2
#define AUDIO_CODEC_VORBIS 3
#define AUDIO_CODEC_AC3 4
#define AUDIO_CODEC_AAC 5
#define AUDIO_CODEC_AMR_NB 6
#define AUDIO_CODEC_RAW 7       // reserved
#define AUDIO_CODEC_WMA2 8
#define AUDIO_CODEC_OPUS 9

#define AUDIO_CODEC_MAX 31
//
#define AUDIO_CODEC_NONE 32
#define AUDIO_CODEC_UNKNOWN 33

// decoders
// "sync_hint"
#define SYNC_HINT_AUDIO_TRIM_START (1<<0)
#define SYNC_HINT_AUDIO_PAD_START (1<<1)
#define SYNC_HINT_AUDIO_TRIM_END (1<<2)
#define SYNC_HINT_AUDIO_PAD_END (1<<3)

#define SYNC_HINT_VIDEO_PAD_START (1<<4)
#define SYNC_HINT_VIDEO_PAD_END (1<<5)

//"seek_flag"
/// good
#define LIVES_SEEK_FAST (1<<0)
#define LIVES_SEEK_FAST_REV (1<<1)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<2)
#define LIVES_SEEK_QUALITY_LOSS (1<<3)

// rendered effects
// "general"
#define RFX_PROPS_SLOW        0x0001  ///< hint to GUI
#define RFX_PROPS_MAY_RESIZE  0x0002 ///< is a tool (can only be applied to entire clip)
#define RFX_PROPS_BATCHG      0x0004 ///< is a batch generator
#define RFX_PROPS_NO_PREVIEWS 0x0008 ///< no previews possible (e.g. effect has long prep. time)

#define RFX_PROPS_RESERVED1   0x1000
#define RFX_PROPS_RESERVED2   0x2000
#define RFX_PROPS_RESERVED3   0x4000
#define RFX_PROPS_AUTO_BUILT  0x8000

// generic STATES which can be altered by *transforms*
#define OBJECT_STATE_NULL	0
#define OBJECT_STATE_NORMAL	1
#define OBJECT_STATE_PREVIEW	2

// TODO - move into cliphandler.c

// aliases for object states
#define CLIP_STATE_NOT_LOADED 	OBJECT_STATE_NULL
#define CLIP_STATE_READY	OBJECT_STATE_NORMAL

// txparams
#define CLIP_PARAM_STAGING_DIR "staging_dir"

// when object is destroyed it should add a ref to status to be returned, and set state to DESTROYED before freeing itself
struct _obj_status {
  int state;
  int *status; /// pointer to an int (some states are dynamic)
  int refcount;
};

// pretty straightforward - object enumerates type / subtype specific capabilites for a given intention
typedef struct {
  lives_intention intent;
  int n_caps;
  int *capabilities; ///< type specific capabilities
} lives_intentcap_t;

// values which are used to perform the transformation, part of the requirement rules
typedef struct {
  lives_intention intent;
  int n_params;
  weed_param_t **params; ///< (can be converted to normal params via weed_param_from_iparams)
} lives_intentparams_t;

/// NOT YET FULLY IMPLEMENTED

// object status (these differ from states in that they are dynamic and under control of the object itself)
#define LIVES_OBJECT_ERROR_COND -3
#define LIVES_OBJECT_ERROR_PREREQ -2
#define LIVES_OBJECT_ERROR_INTENT -1
#define LIVES_OBJECT_STATUS_NORMAL 0 ///< normal / success
#define LIVES_OBJECT_STATUS_WAIT 1 ///< object is doing internal processing not in final state yet
#define LIVES_OBJECT_STATUS_PREP 2 ///< object is in a preparatory status and has new param requirements
#define LIVES_OBJECT_STATUS_READY 3 ///< object is ready but has new conditions
#define LIVES_OBJECT_STATUS_RUNNING 4 ///< object is "running" and the state cannot be changed
#define LIVES_OBJECT_STATUS_NEEDS_DATA 6 ///< object is running but has param / data requirements
#define LIVES_OBJECT_STATUS_DESTROYED 32768

typedef weed_param_t lives_req_t;

// may become functions
typedef boolean lives_cond_t;

// possibly - req type param value, req type condition, req type ...

// rules which must be satisfied before the transformation can succeed
typedef struct {
  lives_object_instance_t *oinst;
  int n_reqs;
  lives_req_t **reqs; ///< requirements. The values of non-optional reqs must be set
  int n_conditions;
  lives_cond_t **conditions; /// ptrs to conditions which must be satisfied (set to TRUE)
  int refcount;
} lives_rules_t;

// flagbits for transformations
#define TR_FLAGS_CREATES_CHILD (1 << 0) // creates child instance from template
#define TR_FLAGS_CREATES_COPY  (1 << 1) // creates a copy of an instance with a different uid
#define TR_FLAGS_CHANGES_SUBTYPE  (1 << 2) // object subtype may change during the transformation
#define TR_FLAGS_FINAL (1 << 3) // state is final after tx, no further transformations possible
#define TR_FLAGS_INSTANT (1 << 4) // must immediately be followed by another transformation

#define TR_FLAGS_DIRECT (1 << 32) // function may be called directly (this is a transitional step
// which may be deprecated

typedef weed_param_t out_params_t;

// maps in / out params to actual function parameters
typedef struct {
  // negative mapping values may have special meanings, e.g self, fundamental
  int n_in_mappings;
  int *in_mappings; // array that maps function parameters to req. params (0 == first param, etc)
  lives_func_info_t func_info; /// function(s) to perform the transformation
  int n_out_mappings;
  int *out_mapping; // index of the out_parameters which will be updated by this function
  lives_funcptr_t callback_hook; // optional callback which may be called after each step for multi step transforms
  /// type is void (*callback_hook)(lives_object_instance_t *self, void *user_data);
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
typedef struct {
  int start_state;
  lives_rules_t *prereqs; // pointer to the prerequisites for carrying out the transform (can be NULL)
  int n_mappings; // mappings
  tx_map *mappings;
  int n_out_params;
  // output parameters created / updated in the transformation; this is actually array of pointers
  // and may even point to params in other objects
  out_params_t **oparams;
  int new_state; // the state after, assuming success (can be same as start_state)
  uint64_t flags;
} lives_object_transform_t;

#if 0
// base functions

// for instances created from a template, select a transform with the flag bit CREATES_CHILD
// for instances which dont have a template, create an instance with just the type set,
// and call the function with a state of zero and pick a transform with LIVES_INTENTION_CREATE
LiVESTransformList *list_transformations(lives_object_t *obj, int state);
#endif

// ensure that all variable not marked optional are set, and all condition flags are TRUE
boolean requirements_met(lives_object_transform_t *);


// the return value belongs to the object, and should only be freed with lives_object_status_free
lives_object_status_t *transform(lives_object_t *obj, lives_object_transform_t *tx,
                                 lives_object_t **other);

//////
// derived functions

const lives_object_template_t *lives_object_template_for_type(uint64_t type, uint64_t subtype);

lives_object_transform_t *find_transform_for_intent(lives_object_t *obj, lives_intention intent);

boolean rules_lack_param(lives_rules_t *req, const char *pname);

//boolean rules_lack_condition(lives_rules_t *req, int condition number);

// convert an iparam to a regular weed_param
weed_param_t *weed_param_from_iparams(lives_intentparams_t *iparams, const char *name);

#if 0
const lives_object_status_t *get_current_status(lives_object_t *obj);
void lives_intentcaps_free(lives_intcaps_t **icap_pp);
void lives_rules_ref(lives_rules_t *rules_pp);
void lives_transform_list_free(LiVESTransformList **transform_list_pp);
void lives_tx_map_free(LiVESTransformList **transform_list_pp);
#endif

void lives_intentparams_free(lives_intentparams_t *p);
void lives_object_status_free(lives_object_status_t *st);
void lives_object_transform_free(lives_object_transform_t *tx);

lives_intentparams_t *get_txparams_for_clip(int clipno, lives_intention intent);

#endif

