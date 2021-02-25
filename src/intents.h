// intents.h
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_INTENTS_H
#define HAS_LIVES_INTENTS_H

#define OBJECT_TYPE_CLIP		64
#define OBJECT_TYPE_PLUGIN		128
#define OBJECT_TYPE_WIDGET		512

// if object is an instance, then template is a pointer to the template
// the template "data" also relates to this instance
// the difference is that templates are static, and the same uid always refers to the same object
// instances have limited scope. there may be multiple instances created for a single template,
// and the uid will vary each time the instance is created
// there is a generic lives_template_none which may be used as the template for instances
// which have no real template. For static objects, tenplate should be left NULL

// the fields should not be accessed directly, only via accessor functions below

typedef struct _object_t lives_object_t;
typedef struct _obj_status lives_object_status_t;

struct _object_t {
  uint64_t uid; // object ID
  uint64_t type; // object type
  uint64_t subtype; // object subtype, can change during a transformation
  lives_object_t *template; // pointer
  lives_object_status_t *status; // pointer to static
  void *priv;
};

// for object obj, in state state, set *intents to an array of intents offered
// the return value is the size of *intents, which may be zero

// a state of 0 is the "uncreated" state.
// get_intents may be called with a staic object, and a state of 0
// the object handler may return an intent
// LIVES_INTENTION_CREATE if the object can be instantiated
// or the static object may have only static intents

typedef enum {
  LIVES_INTENTION_UNKNOWN,
  LIVES_INTENTION_CREATE,
  LIVES_INTENTION_DESTROY,

  // video players
  LIVES_INTENTION_PLAY,
  LIVES_INTENTION_STREAM,
  LIVES_INTENTION_TRANSCODE,  // encode / data in
  LIVES_INTENTION_RENDER,

  //LIVES_INTENTION_ENCODE, // encode / file in
  LIVES_INTENTION_IMPORT_LOCAL, // import from local filesystem, i.e "open"
  LIVES_INTENTION_IMPORT_REMOTE, // import from online source "download"

  LIVES_INTENTION_BACKUP,
  LIVES_INTENTION_RESTORE,
  LIVES_INTENTION_DOWNLOAD,
  LIVES_INTENTION_UPLOAD,
  LIVES_INTENTION_EFFECT,
  LIVES_INTENTION_EFFECT_REALTIME, // or make cap ?
  LIVES_INTENTION_ANALYSE,
  LIVES_INTENTION_CONVERT,
  LIVES_INTENTION_MIX,
  LIVES_INTENTION_SPLIT,
  LIVES_INTENTION_DUPLICATE,
  LIVES_INTENTION_OTHER = 65536
} lives_intention_t;

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

// object states
#define CLIP_STATE_NOT_LOADED 0
#define CLIP_STATE_READY 1

// txparams
#define CLIP_PARAM_STAGING_DIR "staging_dir"

// all instances should create a separate static object status and return a pointer to it when
// treansforming via an intent, when freeing itself it should point to status to a static int with
// value 32768
struct _obj_status {
  int state;
  int *status; /// pointer to an int (some states are dynamic)
};

typedef struct {
  lives_intention_t intent;
  int n_caps;
  int *capabilities; ///< type specific capabilities
} lives_intentcap_t;

// intentparams along with the requirements can be used to guide the transformation
typedef struct {
  lives_intention_t intent;
  int n_params;
  lives_param_t *params; ///< params for the transform
} lives_intentparams_t;

/// NOT YET IMPLEMENTED
#if 0

// object status
#define LIVES_OBJECT_ERROR_COND -3
#define LIVES_OBJECT_ERROR_PREREQ -2
#define LIVES_OBJECT_ERROR_INTENT -1
#define LIVES_OBJECT_STATUS_NONE 0 ///< status for uninitialised instances uncreated instances
#define LIVES_OBJECT_STATUS_NORMAL 1 ///< status for passive instances
#define LIVES_OBJECT_STATUS_WAIT 2 ///< object is doing internal processing not in final state yet
#define LIVES_OBJECT_STATUS_PREP 3 ///< object is in a preparatory status and has new requirements
#define LIVES_OBJECT_STATUS_READY 4
#define LIVES_OBJECT_STATUS_RUNNING 5
#define LIVES_OBJECT_STATUS_NEEDS_DATA 6
#define LIVES_OBJECT_STATUS_DESTROYED 32768

typedef lives_param_t lives_req_t;

// bitmask for req flags
#define LIVES_REQS_FLAG_OPTIONAL 1
#define LIVES_REQS_FLAG_SET 2

typedef boolean lives_cond_t;

typedef struct {
  int n_reqs;
  lives_req_t *reqs; ///< requirements. The values of non-optional reqs must be set
  int n_conditions;
  lives_cond_t *conditions; /// checklist of conditions which must be satisfied (set to TRUE)
  int refcount;
} lives_rules_t;

// flagbits for list_transformations
#define TR_FLAGS_CREATES_CHILD (1 << 0) // creates child instance from template
#define TR_FLAGS_CREATES_COPY  (1 << 1) // creates a copy of an instance with a different uid
#define TR_FLAGS_FINAL (1 << 2) // state is final after tx, no further transformations possible
#define TR_FLAGS_INSTANT (1 << 3) // must immediately be followed by another transformation

typedef struct {
  int start_state;
  lives_intentcap_t icaps; // the matching intention and its capabilities
  lives_rules_t *prereqs; // pointer to the prerequisites for carrying out the transform (can be NULL)
  int new_state; // the state after, assuming success (can be same as start_state)
  uint64_t flags;
} lives_transform_t;

// for instances created from a template, select a transform with the flag bit CREATES_CHILD
// for instances which dont have a template, create an instance with just the type set,
// and call the function with a state of zero and pick a transform with LIVES_INTENTION_CREATE
LiVESTransformList *list_transformations(lives_object_t *obj, int state);

// increase refcount for rules, and return them
lives_rules_t *lives_rules_accept(lives_transform_t *);

// the return value belongs to the object, and should only be freed when the status is DESTROYED
// other should be the address of an object * to receive another object in the case of CREATE / COPY
const lives_object_status_t *transform(lives_object_t *obj, lives_rules_t *rules,
                                       lives_object_t **other);

//////

const lives_object_status_t *get_current_status(lives_object_t *obj);

void lives_intentcaps_free(lives_intcaps_t **icap_pp);
void lives_rules_ref(lives_rules_t *rules_pp);
void lives_rules_unref(lives_rules_t *rules_pp);
void lives_transform_free(lives_transform_t **transform_pp);
void lives_transform_list_free(LiVESTransformList **transform_list_pp);
void lives_object_status_free(lives_object_status_t **status_pp);

#endif

lives_intentparams_t *get_txparams_for_clip(int clipno, lives_intention_t intent);

#endif

