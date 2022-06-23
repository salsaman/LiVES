// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef OBJECT_CONSTANTS_H
#define OBJECT_CONSTANTS_H

// defined object types
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP")  // could do with a more generic name
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG"
#define OBJECT_TYPE_WIDGET		IMkType("obj.WDGT")
#define OBJECT_TYPE_THREAD		IMkType("obj.THRD")
#define OBJECT_TYPE_DICTIONARY		IMkType("obj.DICT")

#define OBJECT_TYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_UNDEFINED 0

#define NO_SUBTYPE 0

// INTENTIONS
enum {
  // some common intentions
  // internal or (possibly) non-functional types
  LIVES_INTENTION_UNKNOWN,

  // application types
  LIVES_INTENTION_NOTHING = 0,

  // for CHANGE_STATE, CHANGE_SUBTYPE, EDIT_DATA, and MANIPULATE_STREAM, when feasable / possible
  LIVES_INTENTION_UNDO, // undo effects of previous transform
  LIVES_INTENTION_REDO, // redo effects of previous transform, after undo

  // transform which creates an instance of specified subtype, with specified state
  LIVES_INTENTION_CREATE_INSTANCE = 0x00000100, // create instance of type / subtype

  // transform which creates a copy of an instance, maybe with a different state ot subtype
  LIVES_INTENTION_COPY_INSTANCE,

  // transform which changes the state of an instance (either self or another instance)
  LIVES_INTENTION_CHANGE_STATE,

  // transform which changes the subtype of an instance (either self or another instance)
  LIVES_INTENTION_CHANGE_SUBTYPE,

  // MANDATORY (builtin) for instances (self transform)
  LIVES_INTENTION_ADDREF,

  // MANDATORY (builtin) for instances (self transform)
  LIVES_INTENTION_UNREF,

  LIVES_INTENTION_UPDATE_VALUE, // for attributes with the async flag

  // an intention which takes one or more mutable attributes and produces output
  // either as another mutable attribute
  // - PLAY is based on this, with CAP realtime
  LIVES_INTENTION_MANIPULATE_STREAM,

  // an intent which takes mutable data and produces static array output
  LIVES_INTENTION_RECORD,  // record

  // intent which takes static attribute (array)
  // and produces an object instance
  LIVES_INTENTION_RENDER,

  // an intention which takes one or more array attributes and produces altered array output
  LIVES_INTENTION_EDIT_DATA,

  // intentions above 0x200 are for specific intents which may be derived from the generic ones for
  // convenience

  // an intent which converts an object's STATE from EXTERNAL to NORMAL
  // caps define LOCAL or REMOTE source
  LIVES_INTENTION_IMPORT = 0x00000C00,

  // an intent which takes an instance in state INTERNAL and creates an object in state EXTERNAL
  // with LOCAL - export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  // with REMOTE - export raw format to online location, e.g. export audio, save frame
  LIVES_INTENTION_EXPORT,

  // specialised intentions

  // an intent which has a data in hook, and data out hooks
  LIVES_INTENTION_PLAY = 0x00000200, // value fixed for all time, order of following must not change (see videoplugin.h)

  //

  // like play, but with the REMOTE capacity (can also be an attachment to PLAY)
  LIVES_INTENTION_STREAM,  // encode / data in -> remote stream

  // alias for encode but with a weed_layer (frame ?) requirement
  // rather than a clip object (could also be an attachment to PLAY with realtime == FALSE and display == FALSE caps)
  // media_output is created with state EXTERNAL
  LIVES_INTENTION_TRANSCODE,

  // an intent which creates a new clip object with STATE EXTERNAL
  // alias for EXPORT for clip objects ?
  // actually this can just be PLAY but with icaps non-realtime and non-display (like transcode)
  // and with icap remote for streaming
  LIVES_INTENTION_ENCODE = 0x00000899,

  // these may be specialised for clip objects

  // creates an object with CAP "backup"
  LIVES_INTENTION_BACKUP, // internal clip -> restorable object

  LIVES_INTENTION_RESTORE, // restore from object -> internal clip

  // decoders
  // this is a specialized intent for clip objects, for READY objects, produces frame objects from the clip object)
  // media_src with realtime / non-realtime CAPS
  LIVES_INTENTION_DECODE = 0x00001000, // combine with caps to determine e.g. decode_audio, decode_video

  // use caps to further refine e.g REALTIME / NON_REALTIME (can be attachment to PLAY ?)
  // this is a transform of base type MANIPULATE_STREAM, which can be attached to a hook of
  // another stream manipulation transform, altering the input stream before or after
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

// aliases (depending on context, intentions can be used to signify a choice amongst various alternatives)
#define LIVES_INTENTION_IGNORE LIVES_INTENTION_NOTHING
#define LIVES_INTENTION_LEAVE LIVES_INTENTION_NOTHING
#define LIVES_INTENTION_SKIP LIVES_INTENTION_NOTHING

#define  LIVES_INTENTION_DESTROY_INSTANCE LIVES_INTENTION_UNREF

// in context with capacity "local" set
#define LIVES_INTENTION_MOVE LIVES_INTENTION_EXPORT

//#define LIVES_INTENTION_MOVE x(LIVES_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

// in context with capacity "remote" set
#define LIVES_INTENTION_UPLOAD LIVES_INTENTION_EXPORT

//#define LIVES_INTENTION_UPLOAD x(LIVES_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

//#define LIVES_INTENTION_DOWNLOAD LIVES_INTENTION_IMPORT_REMOTE

#define LIVES_INTENTION_DELETE LIVES_INTENTION_DESTROY_INSTANCE

#define LIVES_INTENTION_UPDATE LIVES_INTENTION_UPDATE_VALUE

#define LIVES_INTENTION_REPLACE LIVES_INTENTION_DELETE

// generic STATES which can be altered by *transforms*
#define OBJECT_STATE_UNDEFINED	0
#define OBJECT_STATE_NORMAL	1
#define OBJECT_STATE_PREPARE	2

#define OBJECT_STATE_PREVIEW	3 // ???

#define OBJECT_STATE_EXTERNAL	64

#define OBJECT_STATE_FINALISED	512

#define OBJECT_STATE_NOT_READY OBJECT_STATE_PREPARE
#define OBJECT_STATE_READY OBJECT_STATE_NORMAL

///////////////

#define PL_INTENTION_DECODE 		0x00001000

#define PL_INTENTION_PLAY		0x00000200
#define PL_INTENTION_STREAM		0x00000201
#define PL_INTENTION_TRANSCODE		0x00000202

//////////////

// generic capacities, type specific ones may also exist in another header
// values can either be present or absent
#define CAP_PREFIX "cap_"
#define CAP_PREFIX_LEN 4

// either / or
#define OBJ_CAPACITY_LOCAL		"local"  // inputs from or outputs to local hardwares
#define OBJ_CAPACITY_REMOTE		"remote" // inputs from or outputs to remote hardwares

#define OBJ_CAPACITY_REALTIME		"realtime" // operates in realtime and has a "clock" attribute / hook
#define OBJ_CAPACITY_DISPLAY		"display" // data output is to a screen

#define OBJ_CAPACITY_VIDEO		"video" // data input / output is video frames
#define OBJ_CAPACITY_AUDIO		"audio" // data input / output is audio data
#define OBJ_CAPACITY_TEXT		"text" // data input / output is text

#define OBJ_CAPACITY_DATA		"data" // data input / output is some other non specified type

#define OBJ_CAPACITY_BACKUP		"backup" // the object produced is in an internal format

#define OBJ_CAPACITY_LOSSY		"lossy" // some reduction of data quality may occur between input and output
#define OBJ_CAPACITY_LOSSLESS		"lossless" // no reduction of data quality

#define HAS_CAP(caps, name) (lives_has_capacity((lives_capacities_t *)(caps), (name)))
#define HAS_NOT_CAP(caps, name) (!lives_has_capacity((caps), (name)))
#define CAPS_AND(a, b) ((a) && (b))
#define CAPS_OR(a, b) ((a) || (b))
#define CAPS_XOR(a, b) (CAPS_AND(CAPS_OR(a, b) && !CAPS_AND(a, b)))

enum {
  _ICAP_IDLE = 0,
  _ICAP_DOWNLOAD,
  _ICAP_LOAD,
  N_STD_ICAPS
};

////// standard ATTRIBUTES ////

#define ATTR_AUDIO_SOURCE				"audio_source"
#define ATTR_AUDIO_RATE WEED_LEAF_AUDIO_RATE
#define ATTR_AUDIO_CHANNELS WEED_LEAF_AUDIO_CHANNELS
#define ATTR_AUDIO_SAMPSIZE WEED_LEAF_AUDIO_SAMPLE_SIZE
#define ATTR_AUDIO_SIGNED WEED_LEAF_AUDIO_SIGNED
#define ATTR_AUDIO_ENDIAN WEED_LEAF_AUDIO_ENDIAN
#define ATTR_AUDIO_FLOAT "is_float"
#define ATTR_AUDIO_STATUS "current_status"
#define ATTR_AUDIO_INTERLEAVED "audio_inter"
#define ATTR_AUDIO_DATA WEED_LEAF_AUDIO_DATA
#define ATTR_AUDIO_DATA_LENGTH WEED_LEAF_AUDIO_DATA_LENGTH

// video
#define ATTR_VIDEO_FPS "fps"

// UI
#define ATTR_UI_RFX_TEMPLATE "ui_rfx_template"

// attribute flag bits //

#define OBJATTR_FLAG_READONLY 		PARAM_FLAG_READONLY
#define OBJATTR_FLAG_OPTIONAL 		PARAM_FLAG_OPTIONAL
// the 'value' has been altered since this flagbit was last reset
#define OBJATTR_FLAG_VALUE_CHANGED     	PARAM_FLAG_VALUE_SET
// attr updates via UPDATE_VALUE transform; atach to the attribute' VALUE_CHANGED hook to get the update
#define OBJATTR_FLAG_UPDATES_ASYNC 	0x10000
// attr updates via UPDATE_VALUE transform; such updates can use system resources, and should be done only when
// necessary
#define OBJATTR_FLAG_UPDATES_COSTLY 	0x20000
// attr updates via UPDATE_VALUE intent, and monitor with listeners
#define OBJATTR_FLAG_VOLATILE	 	0x40000

///////////// error codes ///////////////

#define LIVES_ERROR_NULL_OBJECT WEED_ERROR_NOSUCH_PLANT

#define LIVES_ERROR_ICAP_NULL WEED_ERROR_NOSUCH_LEAF

#define LIVES_ERROR_NULL_CAP WEED_ERROR_NOSUCH_LEAF
#define LIVES_ERROR_NULL_ATTRIBUTE WEED_ERROR_NOSUCH_LEAF

#define LIVES_ERROR_NOSUCH_CAP WEED_ERROR_NOSUCH_ELEMENT
#define LIVES_ERROR_NOSUCH_ATTRIBUTE WEED_ERROR_NOSUCH_ELEMENT

#define LIVES_ERROR_CAP_INVALID WEED_ERROR_WRONG_SEED_TYPE
#define LIVES_ERROR_ATTRIBUTE_INVALID WEED_ERROR_WRONG_SEED_TYPE

#define LIVES_ERROR_NOT_OWNER WEED_ERROR_IMMUTABLE

// transform status
#define LIVES_TRANSFORM_ERROR_REQ -1 // not all requirements to run the transform have been satisfied
#define LIVES_TRANSFORM_STATUS_NONE 0
#define LIVES_TRANSFORM_STATUS_SUCCESS 1	///< normal / success
#define LIVES_TRANSFORM_STATUS_RUNNING 16	///< transform is "running" and the state cannot be changed
#define LIVES_TRANSFORM_STATUS_NEEDS_DATA 32	///< reqmts. need updating
#define LIVES_TRANSFORM_STATUS_CANCELLED 256	///< transform was cancelled during running
#define LIVES_TRANSFORM_STATUS_ERROR  512	///< transform encountered an error during running

#endif
