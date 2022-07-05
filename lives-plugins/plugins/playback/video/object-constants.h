// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// if usage of BUNDLES is desired,
// exactly one file should #define NEED_OBJECT_BUNDLES before including this header
// as well as #define BUNDLE_TYPE to b


#ifndef OBJECT_CONSTANTS_H
#define OBJECT_CONSTANTS_H

#define ashift(v0, v1) ((uint64_t)(((v0) << 8) | (v1)))
#define ashift4(a, b, c, d) ((uint64_t)(((ashift((a), (b)) << 16) | (ashift((c), (d))))))
#define ashift8(a, b, c, d, e, f, g, h) ((uint64_t)(((ashift4((a), (b), (c), (d))) << 32) | \
						    ashift4((e), (f), (g), (h))))
#define IMkType(str) ((const uint64_t)(ashift8((str)[0], (str)[1], (str)[2], (str)[3], \
					       (str)[4], (str)[5], (str)[6], (str)[7])))
// defined object types
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP")  // could do with a more generic name
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG"
#define OBJECT_TYPE_WIDGET		IMkType("obj.WDGT")
#define OBJECT_TYPE_THREAD		IMkType("obj.THRD")
#define OBJECT_TYPE_DICTIONARY		IMkType("obj.DICT")
#define OBJECT_TYPE_CONTRACT		IMkType("obj.CONT")
#define OBJECT_TYPE_TRANSFORM		IMkType("obj.TRAN")

#define OBJECT_TYPE_UNDEFINED 0
#define OBJECT_TYPE_ANY 0
#define OBJECT_SUBTYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_ANY 0

#define NO_SUBTYPE 0

#define OBJ_INTENTION_NONE 0

// INTENTIONS
enum {
  // some common intentions
  // internal or (possibly) non-functional types
  OBJ_INTENTION_UNKNOWN,

  // application types
  OBJ_INTENTION_NOTHING = OBJ_INTENTION_NONE,

  // for CHANGE_STATE, CHANGE_SUBTYPE, EDIT_DATA, and MANIPULATE_STREAM, when feasable / possible
  OBJ_INTENTION_UNDO, // undo effects of previous transform
  OBJ_INTENTION_REDO, // redo effects of previous transform, after undo

  // transform which creates an instance of specified subtype, with specified state
  OBJ_INTENTION_CREATE_INSTANCE = 0x00000100, // create instance of type / subtype

  // transform which creates a copy of an instance, maybe with a different state ot subtype
  OBJ_INTENTION_COPY_INSTANCE,

  // transform which changes the state of an instance (either self or another instance)
  OBJ_INTENTION_CHANGE_STATE,

  // transform which changes the subtype of an instance (either self or another instance)
  OBJ_INTENTION_CHANGE_SUBTYPE,

  // MANDATORY (builtin) for instances (self transform)
  OBJ_INTENTION_ADDREF,

  // MANDATORY (builtin) for instances (self transform)
  OBJ_INTENTION_UNREF,

  OBJ_INTENTION_UPDATE_VALUE, // for attributes with the async flag

  // an intention which takes one or more mutable attributes and produces output
  // either as another mutable attribute
  // - PLAY is based on this, with CAP realtime
  OBJ_INTENTION_MANIPULATE_STREAM,

  // an intent which takes mutable data and produces static array output
  OBJ_INTENTION_RECORD,  // record

  // intent which takes static attribute (array)
  // and produces an object instance
  OBJ_INTENTION_RENDER,

  // an intention which takes one or more array attributes and produces altered array output
  OBJ_INTENTION_EDIT_DATA,

  // intentions above 0x200 are for specific intents which may be derived from the generic ones for
  // convenience

  // an intent which converts an object's STATE from EXTERNAL to NORMAL
  // caps define LOCAL or REMOTE source
  OBJ_INTENTION_IMPORT = 0x00000C00,

  // an intent which takes an instance in state INTERNAL and creates an object in state EXTERNAL
  // with LOCAL - export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  // with REMOTE - export raw format to online location, e.g. export audio, save frame
  OBJ_INTENTION_EXPORT,

  // specialised intentions

  // an intent which has a data in hook, and data out hooks
  OBJ_INTENTION_PLAY = 0x00000200, // value fixed for all time, order of following must not change (see videoplugin.h)

  //

  // like play, but with the REMOTE capacity (can also be an attachment to PLAY)
  OBJ_INTENTION_STREAM,  // encode / data in -> remote stream

  // alias for encode but with a weed_layer (frame ?) requirement
  // rather than a clip object (could also be an attachment to PLAY with realtime == FALSE and display == FALSE caps)
  // media_output is created with state EXTERNAL
  OBJ_INTENTION_TRANSCODE,

  // an intent which creates a new clip object with STATE EXTERNAL
  // alias for EXPORT for clip objects ?
  // actually this can just be PLAY but with icaps non-realtime and non-display (like transcode)
  // and with icap remote for streaming
  OBJ_INTENTION_ENCODE = 0x00000899,

  // these may be specialised for clip objects

  // creates an object with CAP "backup"
  OBJ_INTENTION_BACKUP, // internal clip -> restorable object

  OBJ_INTENTION_RESTORE, // restore from object -> internal clip

  // decoders
  // this is a specialized intent for clip objects, for READY objects, produces frame objects from the clip object)
  // media_src with realtime / non-realtime CAPS
  OBJ_INTENTION_DECODE = 0x00001000, // combine with caps to determine e.g. decode_audio, decode_video

  // use caps to further refine e.g REALTIME / NON_REALTIME (can be attachment to PLAY ?)
  // this is a transform of base type MANIPULATE_STREAM, which can be attached to a hook of
  // another stream manipulation transform, altering the input stream before or after
  OBJ_INTENTION_EFFECT = 0x00001400,

  // do we need so many ? maybe these can become CAPS
  OBJ_INTENTION_ANALYSE,
  OBJ_INTENTION_CONVERT,
  OBJ_INTENTION_MIX,
  OBJ_INTENTION_SPLIT,
  OBJ_INTENTION_DUPLICATE,

  OBJ_INTENTION_FIRST_CUSTOM = 0x80000000,
  OBJ_INTENTION_MAX = 0xFFFFFFFF
};

// aliases (depending on context, intentions can be used to signify a choice amongst various alternatives)
#define OBJ_INTENTION_IGNORE OBJ_INTENTION_NOTHING
#define OBJ_INTENTION_LEAVE OBJ_INTENTION_NOTHING
#define OBJ_INTENTION_SKIP OBJ_INTENTION_NOTHING

#define  OBJ_INTENTION_DESTROY_INSTANCE OBJ_INTENTION_UNREF

// in context with capacity "local" set
#define OBJ_INTENTION_MOVE OBJ_INTENTION_EXPORT

//#define OBJ_INTENTION_MOVE x(OBJ_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

// in context with capacity "remote" set
#define OBJ_INTENTION_UPLOAD OBJ_INTENTION_EXPORT

//#define OBJ_INTENTION_UPLOAD x(OBJ_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

//#define OBJ_INTENTION_DOWNLOAD OBJ_INTENTION_IMPORT_REMOTE

#define OBJ_INTENTION_DELETE OBJ_INTENTION_DESTROY_INSTANCE

#define OBJ_INTENTION_UPDATE OBJ_INTENTION_UPDATE_VALUE

#define OBJ_INTENTION_REPLACE OBJ_INTENTION_DELETE

// generic STATES which can be altered by *transforms*
#define OBJECT_STATE_UNDEFINED	0
#define OBJECT_STATE_NORMAL	1
#define OBJECT_STATE_PREPARE	2

#define OBJECT_STATE_PREVIEW	3 // ???

#define OBJECT_STATE_EXTERNAL	64

#define OBJECT_STATE_FINALISED	512

#define OBJECT_STATE_ANY        -1 // match any state

#define OBJECT_STATE_NOT_READY OBJECT_STATE_PREPARE
#define OBJECT_STATE_READY OBJECT_STATE_NORMAL

//////////////

// generic capacities, type specific ones may also exist in another header
// values can either be present or absent

#define CAP_END			NULL // marks end of caps list for vargs

// either / or
#define CAP_LOCAL		"local"  // inputs from or outputs to local hardwares
#define CAP_REMOTE		"remote" // inputs from or outputs to remote hardwares

#define CAP_REALTIME		"realtime" // operates in realtime and has a "clock" attribute / hook
#define CAP_DISPLAY		"display" // data output is to a screen

#define CAP_VIDEO		"video" // data input / output is video frames
#define CAP_AUDIO		"audio" // data input / output is audio data
#define CAP_TEXT		"text" // data input / output is text

#define CAP_DATA		"data" // data input / output is some other non specified type

#define CAP_BACKUP		"backup" // internal format which can be restored

#define CAP_LOSSY		"lossy" // some reduction of data quality may occur between input and output
#define CAP_LOSSLESS		"lossless" // no reduction of data quality

#define HAS_CAP(caps, name) (icap_has_capacity((caps), CAP_##name))
#define HAS_NOT_CAP(caps, name) (!icap_has_capacity((caps), CAP_##name))
#define CAPS_AND(a, b) ((a) && (b))
#define CAPS_OR(a, b) ((a) || (b))
#define CAPS_XOR(a, b) (CAPS_AND(CAPS_OR(a, b) && !CAPS_AND(a, b)))

enum {
  _ICAP_IDLE = 0,
  _ICAP_DOWNLOAD,
  _ICAP_LOAD,
  N_STD_ICAPS
};

////// standard ITEMS ////

// const char * versions

#define _ELEM_TYPE_NONE					"0"

// flag bits
#define _ELEM_TYPE_FLAG_OPTIONAL       			"?"
#define _ELEM_TYPE_FLAG_ARRAY				"*"
#define _ELEM_TYPE_FLAG_COMMENT				"#"

#define _ELEM_TYPE_INT					"i"
#define _ELEM_TYPE_DOUBLE				"d"
#define _ELEM_TYPE_BOOLEAN				"b"
#define _ELEM_TYPE_STRING				"s"
#define _ELEM_TYPE_INT64	       			"I"
#define _ELEM_TYPE_UINT					"u"
#define _ELEM_TYPE_UINT64				"U"

#define _ELEM_TYPE_VOIDPTR				"v"
#define _ELEM_TYPE_BUNDLEPTR	       			"B"

#define _ELEM_TYPE_SPECIAL				"$"

////////////////////////////

///////// element types ////////

#define ELEM_TYPE_NONE					(uint32_t)'0'	// invalid type

// flag bits
#define ELEM_TYPE_FLAG_OPTIONAL		      		(uint32_t)'?'	// optional element
#define ELEM_TYPE_FLAG_ARRAY				(uint32_t)'*'	// value is *array
#define ELEM_TYPE_FLAG_COMMENT		      		(uint32_t)'#'	// comment

#define ELEM_TYPE_INT					(uint32_t)'i'	// 4 byte int
#define ELEM_TYPE_DOUBLE				(uint32_t)'d'	// 8 byte float
#define ELEM_TYPE_BOOLEAN				(uint32_t)'b'	// 1 - 4 byte int
#define ELEM_TYPE_STRING				(uint32_t)'s'	// \0 terminated string
#define ELEM_TYPE_INT64	       			     	(uint32_t)'I'	// 8 byte int
#define ELEM_TYPE_UINT					(uint32_t)'u'	// 4 byte int
#define ELEM_TYPE_UINT64				(uint32_t)'U'	// 8 byte int

#define ELEM_TYPE_VOIDPTR				(uint32_t)'v'	// void *

// void * aliases
#define ELEM_TYPE_BUNDLEPTR	       			(uint32_t)'B' // void * to other bundle

#define ELEM_TYPE_SPECIAL				(uint32_t)'$' // special rule applies

#ifdef VERSION
#undef VERSION
#endif

#define ELEM_NAMEU(a, b) "ELEM_" a "_" b
#define ELEM_NAME(a, b) ELEM_NAMEU(a, b)

// domain BASE
#define ELEM_BASE_VERSION				ELEM_NAME("BASE", "VERSION")
#define ELEM_BASE_VERSION_TYPE              		INT, 1, 100

#define ELEM_FUNDAMENTAL_BUNDLE_PTR			ELEM_NAME("FUNDAMENTAL", "BUNDLE_PTR");
#define ELEM_FUNDAMENTAL_BUNDLE_PTR_TYPE	       	BUNDLEPTR, 1, NULL

// domain GENERIC

#define ELEM_GENERIC_NAME				ELEM_NAME("GENERIC", "NAME")
#define ELEM_GENERIC_NAME_TYPE              		STRING, 1, NULL

#define ELEM_GENERIC_FLAGS				ELEM_NAME("GENERIC", "FLAGS")
#define ELEM_GENERIC_FLAGS_TYPE              		UINT64, 1, 0

#define ELEM_GENERIC_UID				ELEM_NAME("GENERIC", "UID")
#define ELEM_GENERIC_UID_TYPE				SPECIAL, 1, 0

#define ELEM_GENERIC_DESCRIPTION			ELEM_NAME("GENERIC", "DESCRIPTION")
#define ELEM_GENERIC_DESCRIPTION_TYPE              	STRING, 1, NULL

///// domain ICAP
#define ELEM_ICAP_INTENTION				ELEM_NAME("ICAP", "INTENTION")
#define ELEM_ICAP_INTENTION_TYPE              		INT, 1, OBJ_INTENTION_NONE

#define ELEM_ICAP_CAPACITIES				ELEM_NAME("ICAP", "CAPACITIES")
#define ELEM_ICAP_CAPACITIES_TYPE              		BUNDLEPTR, 1, NULL // icap_bundle

////////// domain object

#define ELEM_OBJECT_ATTRIBUTES				ELEM_NAME("OBJECT", "ATTRIBUTES")
#define ELEM_OBJECT_ATTRIBUTES_TYPE	       		BUNDLEPTR, -1, NULL // attr_bundle

#define ELEM_OBJECT_TYPE				ELEM_NAME("OBJECT", "TYPE")
#define ELEM_OBJECT_TYPE_TYPE				UINT64, 1, OBJECT_TYPE_UNDEFINED

#define ELEM_OBJECT_SUBTYPE				ELEM_NAME("OBJECT", "SUBTYPE")
#define ELEM_OBJECT_SUBTYPE_TYPE       			UINT64, 1, OBJECT_SUBTYPE_UNDEFINED

#define ELEM_OBJECT_STATE				ELEM_NAME("OBJECT", "STATE")
#define ELEM_OBJECT_STATE_TYPE       			INT, 1, OBJECT_STATE_UNDEFINED

// There are two types of hooks, object hooks and transform hooks
// object hooks may be triggered when an object changes state and / or subtype
// transform hooks may be triggered when a transform status changes
#define ELEM_OBJECT_HOOKS				ELEM_NAME("OBJECT", "HOOKS")
#define ELEM_OBJECT_HOOKS_TYPE		       	       	BUNDLEPTR, -1, NULL // hook_bundle

#define ELEM_OBJECT_ACTIVE_TRANSFORMS		       	ELEM_NAME("OBJECT", "ACTIVE_TRANSFORMS")
#define ELEM_OBJECT_ACTIVE_TRANSFORMS_TYPE	       	BUNDLEPTR, -1, NULL
#define ELEM_OBJECT_ACTIVE_TRANSFORMS_OBJDEF			\
  OBJECT_TYPE_TRANSFORM, OBJECT_SUBTYPE_ANY, OBJECT_STATE_ANY

#define ELEM_OBJECT_CONTRACTS				ELEM_NAME("OBJECT", "CONTRACTS")
#define ELEM_OBJECT_CONTRACTS_TYPE       		BUNDLEPTR, -1, NULL
#define ELEM_OBJECT_CONTRACTS_OBJDEF					\
  OBJECT_TYPE_CONTRACT, OBJECT_SUBTYPE_ANY, OBJECT_STATE_PREVIEW

// max number of elements allowed in DATA
//
// - if absent or 1, indicates a scalar value
//
// a value of -1 means no limit (unbounded array)
//
// if > 1, denotes the maximum number of array elements
// if DEFAULT is present, then this is modified to MAX(1, MAX_REPEATS, n_elements(DEFAULT))
// "element size" is 1 value by default - other factors may modify this, however
//
// a MAX_REPEATS of 0 indicates "undefined"; the data should not be written to or read
// until this vallue is changed or deleted.
//
#define ELEM_VALUE_MAX_REPEATS				ELEM_NAME("VALUE", "MAX_REPEATS")
#define ELEM_VALUE_MAX_REPEATS_TYPE	       		INT, 0, 1

#define ELEM_VALUE_DATA		      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_DATA_TYPE       			SPECIAL, 1,

#define ELEM_VALUE_VALUE_TYPE	      			ELEM_NAME("VALUE", "VALUE_TYPE")
#define ELEM_VALUE_VALUE_TYPE_TYPE     			UINT, 1, ATTR_TYPE_NONE

#define ELEM_VALUE_DEFALT	      			ELEM_NAME("VALUE", "DEFAULT")
#define ELEM_VALUE_DEFAULT_TYPE       			SPECIAL, 0,

#define ELEM_VALUE_NEW_DEFALT	      			ELEM_NAME("VALUE", "NEW_DEFAULT")
#define ELEM_VALUE_NEW_DEFAULT_TYPE   			SPECIAL, 0,

// domain TRANSFORMS
// There are two types of hooks, object hooks and transform hooks
// object hooks may be triggered when an object changes state and / or subtype
// transform hooks may be triggered when a transform status changes
// When a "contract" is being negotiated, the hooks will be pinned to the contract,
// and the negotiating parties should ensure that the any input hooks with mandatory
// attribute connections are connected before the contract is agreed.
// During transform preparation, the hooks will be transferred from contract to
// the transaction instance.
#define ELEM_TRANSFORM_HOOKS				ELEM_NAME("TRANSFORM", "HOOKS")
#define ELEM_TRANSFORM_HOOKS_TYPE      	       	       	BUNDLEPTR, -1, NULL // hook_bundle

///// domain CONTRACT
// attributes for a contract (replace normal attributes)
// these are pooled attributes, with each negotiating party contributing to
// or removing attributes from the pool
// all mandatory attributes in the contract must have values set before the contract
// can be agreed. During the transform preparation step, all attributes with values will be
// copied to the the transform object, after which the pooled attributes will be returned to
// their respective owners.
#define ELEM_CONTRACT_ATTR_POOL			      	ELEM_NAME("CONTRACT", "ATTR_POOL")
#define ELEM_CONTRACT_ATTR_POOL_TYPE		       	BUNDLEPTR, -1, NULL
//#define ELEM_CONTRACT_ATTR_POOL_BUNDLE_TYPE	       	list_of_lists_bundle

///// domain HOOKS
// status for transform hooks
#define ELEM_HOOK_STATUS_FROM				ELEM_NAME("HOOK", "STATUS_FROM")
#define ELEM_HOOK_STATUS_FROM_TYPE       	       	INT, 1, TRANSFORM_STATUS_NONE

#define ELEM_HOOK_STATUS_TO				ELEM_NAME("HOOK", "STATUS_TO")
#define ELEM_HOOK_STATUS_TO_TYPE       	       		INT, 1, TRANSFORM_STATUS_NONE

#define ELEM_HOOK_ATTRIBUTES				ELEM_NAME("HOOK", "ATTRIBUTES")
#define ELEM_HOOK_ATTRIBUTES_TYPE       	       	BUNDLEPTR, -1, NULL
#define ELEM_HOOK_ATTRIBUTES_BUNDLE_TYPE       	       	hook_attr_bundle

// state for object hooks
#define ELEM_HOOK_STATE_FROM				ELEM_NAME("HOOK", "STATE_FROM")
#define ELEM_HOOK_STATE_FROM_TYPE       	       	INT, 1, OBJECT_STATE_UNDEFINED

#define ELEM_HOOK_STATE_TO				ELEM_NAME("HOOK", "STATE_TO")
#define ELEM_HOOK_STATE_TO_TYPE       	       		INT, 1, OBJECT_STATE_UNDEFINED

// domain INTROSPECTION
// this is an optional item which should be added to ever bundldef, it is designed to allow
// the bundle creator to store the pared down quarks used to construct the bundle
#define ELEM_INTROSPECTION_QUARKS    			ELEM_NAME("INTROSPECTION", "QUARKS")
#define ELEM_INTROSPECTION_QUARKS_TYPE       	       	STRING, -1, NULL

// as an alternative this can be used instead to point to a static copy of the quarks
#define ELEM_INTROSPECTION_QUARKS_PTR    	       	ELEM_NAME("INTROSPECTION", "QUARKS_PTR")
#define ELEM_INTROSPECTION_QUARKS_PTR_TYPE            	VOIDPTR, 1, NULL

#define ELEM_INTROSPECTION_COMMENT    			ELEM_NAME("INTROSPECTION", "COMMENT")
#define ELEM_INTROSPECTION_COMMENT_TYPE       	       	STRING, 1, NULL

#define ELEM_INTROSPECTION_REFCOUNT    			ELEM_NAME("INTROSPECTION", "REFCOUNT")
#define ELEM_INTROSPECTION_REFCOUNT_TYPE       	       	INT, 1, 0

#define ELEM_INTROSPECTION_PRIVATE_DATA			ELEM_NAME("INTROSPECTION", "PRIVATE_DATA")
#define ELEM_INTROSPECTION_PRIVATE_DATA_TYPE		VOIDPTR, 1, NULL

#define ELEM_INTROSPECTION_NATIVE_TYPE 			ELEM_NAME("INTROSPECTION", "NATIVE_TYPE")
#define ELEM_INTROSPECTION_NATIVE_TYPE_TYPE 	       	INT, 1, NULL

#define ELEM_INTROSPECTION_NATIVE_SIZE	 	       	ELEM_NAME("INTROSPECTION", "NATIVE_SIZE")
#define ELEM_INTROSPECTION_NATIVE_SIZE_TYPE		UINT64, -1, NULL

#define ELEM_INTROSPECTION_NATIVE_PTR 			ELEM_NAME("INTROSPECTION", "NATIVE_PTR")
#define ELEM_INTROSPECTION_NATIVE_PTR_TYPE		VOIDPTR, -1, NULL

///// domain ATTRIBUTE
#define ELEM_ATTRIBUTE_LOCAL				ELEM_NAME("ATTRIBUTE", "LOCAL")
#define ELEM_ATTRIBUTE_LOCAL_TYPE			BUNDLEPTR, 1, NULL // attr_bundle

#define ELEM_ATTRIBUTE_OWNER_OBJECT		      	ELEM_NAME("ATTRIBUTE", "OWNER_OBJECT")
#define ELEM_ATTRIBUTE_OWNER_OBJECT_TYPE      	       	BUNDLEPTR, 1, NULL // obj_bundle

#define ELEM_ATTRIBUTE_FOREIGN				ELEM_NAME("ATTRIBUTE", "FOREIGN")
#define ELEM_ATTRIBUTE_FOREIGN_TYPE			BUNDLEPTR, 1, NULL // attr_bundle

#define ELEM_ATTRIBUTE_ATTRPTR				ELEM_NAME("ATTRIBUTE", "ATTRPTR")
#define ELEM_ATTRIBUTE_ATTRPTR_TYPE			BUNDLEPTR, 1, NULL // attr_bundle

///// domain LIST
#define ELEM_LIST_NEXT					ELEM_NAME("LIST", "NEXT")
#define ELEM_LIST_NEXT_TYPE              		BUNDLEPTR, 1, NULL
#define ELEM_LIST_NEXT_BUNDLE_TYPE              	list_bundle

#define ELEM_LIST_PREV					ELEM_NAME("LIST", "PREV")
#define ELEM_LIST_PREV_TYPE              		BUNDLEPTR, 1, NULL
#define ELEM_LIST_PREV_BUNDLE_TYPE              	list_bundle

#define ELEM_LIST_DATA					ELEM_NAME("LIST", "DATA")
#define ELEM_LIST_DATA_TYPE              		VOIDPTR, 1, NULL

#define ELEM_LIST_STRING_DATA		       		ELEM_NAME("LIST", "STRING_DATA")
#define ELEM_LIST_STRING_DATA_TYPE             		STRING, 1, NULL

#define ELEM_LIST_ATTRDATA				ELEM_NAME("LIST", "ATTRDATA")
#define ELEM_LIST_ATTRDATA_TYPE              		BUNDLEPTR, 1, NULL // attr_bundle

#define ELEM_LIST_HDRDATA				ELEM_NAME("LIST", "HDRDATA")
#define ELEM_LIST_HDRDATA_TYPE              		BUNDLEPTR, 1, NULL
#define ELEM_LIST_HDRDATA_BUNDLE_TYPE              	list_header_bundle

#define ELEM_LIST_OWNED_LIST				ELEM_NAME("LIST", "OWNED_LIST")
#define ELEM_LIST_OWNED_LIST_TYPE              		BUNDLEPTR, 1, NULL
#define ELEM_LIST_OWNED_LIST_BUNDLE_TYPE              	attr_list_bundle

////////////////////////////////////////////////////////////////////////////////////

// OBJECT / HOOK / CONTRACT ATTRIBUTES
// these are created as bundle_type attr_bundle
// similar to bundle items, these also have the form DOMAIN, ITEM
// however rather than simple data elements, they should be create as attr_bundle
#define ATTR_TYPE_NONE					(uint32_t)'0'	// invalid type

#define ATTR_TYPE_INT					1	// 4 byte int
#define ATTR_TYPE_DOUBLE				2	// 8 byte float
#define ATTR_TYPE_BOOLEAN				3	// 1 - 4 byte int
#define ATTR_TYPE_STRING				4	// \0 terminated string
#define ATTR_TYPE_INT64	       			     	5	// 8 byte int
#define ATTR_TYPE_UINT					6	// 4 byte int
#define ATTR_TYPE_UINT64				7	// 8 byte int
#define ATTR_TYPE_FLOAT	       				6	// 4 or 8 byte float

#define ATTR_TYPE_VOIDPTR				64	// void *

// void * aliases
#define ATTR_TYPE_BUNDLEPTR	       			80 // void * to other bundle

/////////

#define ATTR_NAMEU(a, b) "ATTR_" a "_" b
#define ATTR_NAME(a, b) ATTR_NAMEU(a, b)

/// domain URI
#define ATTR_URI_FILENAME				ATTR_NAME("URI", "FILENAME")
#define ATTR_URI_FILENAME_TYPE 				STRING, 1, NULL

///// domain UI
#define ATTR_UI_TEMPLATE 				ATTR_NAME("UI", "TEMPLATE")
#define ATTR_UI_TEMPLATE_TYPE 				STRING, -1, NULL

// audio
#define ATTR_AUDIO_SOURCE				"audio_source"
#define ATTR_AUDIO_RATE 				"audio_rate"
#define ATTR_AUDIO_CHANNELS				"audio_channels"
#define ATTR_AUDIO_SAMPSIZE				"audio_sampsize"
#define ATTR_AUDIO_SIGNED				"audio_signed"
#define ATTR_AUDIO_ENDIAN				"audio_endian"
#define ATTR_AUDIO_FLOAT				"audio_is_float"
#define ATTR_AUDIO_STATUS 				"current_status"
#define ATTR_AUDIO_INTERLEAVED 				"audio_interleaf"
#define ATTR_AUDIO_DATA					"audio_data"
#define ATTR_AUDIO_DATA_LENGTH				"audio_data_length"

///// domain VIDEO (incomplete)
#define ELEM_VIDEO_FRAME_RATE	       			ELEM_NAME(VIDEO, FRAME_RATE)
#define ELEM_VIDEO_FRAME_RATE_TYPE    			DOUBLE, 1, 0.

#define ELEM_VIDEO_DISPLAY_WIDTH  	       		ELEM_NAME(VIDEO, DISPLAY_WIDTH)
#define ELEM_VIDEO_DISPLAY_WIDTH_TYPE    	 	UINT64, 1, 0

#define ELEM_VIDEO_DISPLAY_HEIGHT  	       		ELEM_NAME(VIDEO, DISPLAY_HEIGHT)
#define ELEM_VIDEO_DISPLAY_HEIGHT_TYPE    	 	UINT64, 1, 0

#define ELEM_VIDEO_PIXEL_WIDTH  	       		ELEM_NAME(VIDEO, PIXEL_WIDTH)
#define ELEM_VIDEO_PIXEL_WIDTH_TYPE 	   	 	UINT64, 1, 0

#define ELEM_VIDEO_PIXEL_HEIGHT  	       		ELEM_NAME(VIDEO, PIXEL_HEIGHT)
#define ELEM_VIDEO_PIXEL_HEIGHT_TYPE    	 	UINT64, 1, 0

#define ELEM_VIDEO_COLOR_SPACE  	       		ELEM_NAME(VIDEO, COLOR_SPACE)
#define ELEM_VIDEO_COLOR_SPACE_TYPE 	   	 	INT, -1, 0

#define ELEM_VIDEO_STEREO_MODE  	       		ELEM_NAME(VIDEO, STEREO_MODE)
#define ELEM_VIDEO_STEREO_MODE_TYPE 	   	 	UINT64, 1, 0

#define ELEM_VIDEO_FLAG_INTERLACED  	       		ELEM_NAME(VIDEO, FLAG_INTERLACED)
#define ELEM_VIDEO_FLAG_INTERLACED_TYPE    	 	UINT64, 1, 0

// attribute flag bits //
// these flagbits are for input attributes, for output attributes they are ignored
#define OBJ_ATTR_FLAG_READONLY 		0x00001
#define OBJ_ATTR_FLAG_MANDATORY        	0x00002
//
// output value - indicates that the value will be updated by a transform
// at the end of processing, and / or in a hook during the transform
#define OBJ_ATTR_FLAG_OUTPUT	 	0x1000

// attr value may update spontaneously (may or may not have a value changed hook)
// otherwise the value will only update as the result of a transform
#define OBJ_ATTR_FLAG_VOLATILE	 	0x2000

// each update returns the next value in a sequence, i.e it should only be updated
// once for each read, otherwise, the value returned is the "current value"
#define OBJ_ATTR_FLAG_SEQUENTIAL       	0x4000

// attr updates via UPDATE_VALUE transform; atach to hook to read value
#define TC_FLAG_ASYNC // the transform does not return immediately
#define TX_FLAG_ASYNC_COSTLY // the transform use system resources, therefore should be runsparingly
#define TX_FLAG_ASYNC_DELAY // can take time to run, therefore it is recomended to run in a thread
// n.b if a transform delays, and produces sequential data, it should be run
// as early as possible.

///////////// error codes ///////////////

#define OBJ_ERROR_NULL_OBJECT			1
#define OBJ_ERROR_NULL_ATTRIBUTE		2
#define OBJ_ERROR_NOSUCH_ATTRIBUTE		3
#define OBJ_ERROR_ATTRIBUTE_INVALID		4
#define OBJ_ERROR_ATTRIBUTE_READONLY		5
#define OBJ_ERROR_NULL_ICAP			6
#define OBJ_ERROR_NULL_CAP			7
#define OBJ_ERROR_NOT_OWNER			8

// transform status
#define TRANSFORM_ERROR_REQ -1 // not all requirements to run the transform have been satisfied
#define TRANSFORM_STATUS_NONE 0
#define TRANSFORM_STATUS_SUCCESS 1	///< normal / success
#define TRANSFORM_STATUS_RUNNING 16	///< transform is "running" and the state cannot be changed
#define TRANSFORM_STATUS_NEEDS_DATA 32	///< reqmts. need updating
#define TRANSFORM_STATUS_CANCELLED 256	///< transform was cancelled during running
#define TRANSFORM_STATUS_ERROR  512	///< transform encountered an error during running

/////////////////////////// bundles //
//
/**
   Let us consider a single 'data element' (node) consisting of a name, type and data;
   we can represent that data element as a string 's'
   where s[0] denotes the type, and the remainder of the string is the element name.
   So for example a string "sCOMMENT" could represent a data element of TYPE 's' (string)
   with NAME == "COMMENT".

   The idea here is that by constructing an array of these strings (quarks)
   we can then process this, allocate some type of container object (an empty BUNDLE)
   and then create elements inside it following the template of the quark array
   which we shall call a BUNDLE DEFINITION or simply BUNDLEDEF) and thus create within
   the container a collection of "data elements", which taken together with the container,
   form a complete bundle.

   Furthermore, we can elaborate on these quarks by adding more information; in this case
   we say that a quark starting with a '?' character represents an optional element,
   and we may follow each quark with additional quarks with more information.
   In this case we will add a second quark, following the first:
   quark2[0] defines whether the data is a scalar '0', or an array '1'
   this is followed by a space ' ' and subsequently the default value as string
   which can be cast to the type specific to the element. Let us also say that any quark
   beginning with the character '#' is a comment and may be skipped past when constructing a bundle.

   We can construct a bundledef simply by concatenating these definitions, and specify whether
   each is mandatory or optional. It is possible to include one bundledef inside another,
   to extend one by adding extra elements, and we also have a type "pointer to bundle" which can be
   used within the bundledef to create element to point to a pre-created sub bundle, or array of these.
   Thus in this manner it is possible to build up from basic building blocks
   to increasingly more sophisticated and specialised bundles.

   There are TWO TYPES of bundledef: BASE and ATTRIBUTES.

   A BASE BUNDLEDEF is constructed from elementary items, the FULL NAMES
   of all the elements are prefixed ELEM_. For example a base element (DOMAIN, ITEM)
   would have full name ELEM_DOMAIN_ITEM.

   To simplify the task of creating base bundledefs, the header provided macro
   ELEM_NAME(DOMAIN, ITEM) will produce a symbol: ELEM_DOMAIN_ITEM.

   Many such base elements are already defined.

   the values here are created as simple name / value pairs, using the SHORTENED NAME.

   The ELEM_DOMAIN_ part of the full name should be ommited when creating the
   item - thus ELEM_GENERIC_FLAGS would create an item called "FLAGS" in the bundle.

   This allows for simple polymorphism when creating base bundledefs, and also allows for
   the SPECIAL rules to apply to only particular variants. Thus there should be a bundledef
   "validator" which checks that there is no duplication of the same item from two different domains.
   In case this occurs, the bundledef should be rejected as invalid and the definition should be
   corrected to remove th duplication.
   Duplication of items from the same domain may occur du to the way bundledefs are constructed.

   Due to the way that bundledefs build upon each other, it may happen that an element
   (ELEM_DOMAIN_ITEM or ATTR_CDOMAIN_ITEM) appears multiple times in a bundledef.
   In this case only a single copy of the element should be added.

   The shortened names in the bundle MUST be unique.

   (However it is permitted for an element of the same shortened name to appear in both
   the main bundle and any sub bundle pointed to by an element in the bundle, the element
   may be optional in one bundle and non optional in the other).

   If an item appears as both optional and non-optional in the same bundle,
   then it shall be considered mandatory, and the instance marking it as optional ignored.

   If so desired, the array of quarks may be "pruned" to remove any duplicate entries,
   taking care to leave a copy marked as non-optional in the case where optional and non-optional
   variants both exist in the original.
   Comments in the original may optionally be removed for the pruned copy.

   ///

   There are a handful of SPECIAL RULES for the treatment of a few elements.
   For exmple, the type of the (VALUE, DATA) element is determined by the value of the
   (VALUE, TYPE) element, which must be present in the same bundledef.

   The second type of bundledef is an ATTRIBUTES bundledef.
   This time the full names appear as ATTR_DOMAIN_ITEM.

   Thes items are not to be constructed by a builder, instead they are created later and
   can have DEFAULT and / or VALUE items set. The attribute bundledef simply records the names
   o the the attributes, their type and maximum number of data elements,
   and whether the attribute is mandatory or optional.

   In this case, the optional '?' indicates that the entire bundle is optional.

   Using the macro ATTR_NAME(DOMAIN, ITEM) will create the symbol: ATTR_DOMAIN_ITEM.

   The goal of the implementor should be to create a "transcriber" which passes throught a
   bundledef and appends the items to an empty bundle container.

   If the full item names are ELEM_*_*, then we are dealing with a base bundle.
   and simple elemnts should be created in the bundle.

   If the full item names are ATTR_*_*, then this is an attribute bundledef.

   When setting values after intiialisation, firstly any applicable SPECIAL RULES must be
   followed. Otherwis, of a base bundle, jus tupdate the value of the data element in the bundle.

   In an attributes, bundle, the elements are created as bundles of a specific type.

   If the full item name is ATTR_*_*, then it should create a bundle of type attr_bundle,
   amd set the NAME, TYPE, and other elements inside accordingly.
   In this case the attribute name should include the DOMAIN - for example
   ATTR_AUDIO_RATE would create an attr_bundle and set the NAME item in it to
   "AUDIO_RATE". This allows for multiple ATTRIBUTE domains to include items with the same item
   name.

   To recap:
   if the FULL NAME in a quark is ELEM_DOMAIN_ITEM, then the SHORTENED name is simply ITEM,
   and this should be created as a name / value pair with the shortened name and the specified type.

   if the FULL NAME is ATTR_DOMAIN_ITEM, then th SHORTENED name is DOMAIN_ITEM.
   The creator function should create an attr_bundle and an item DOMAIN_ITEM to hold the
   attr_bundle. When setting or reading the value of an "attribute", take the value from the
   DATA (full name ELEM_VALUE_DATA) item inside the attr_bundle, the type of this is whatever is in
   TYPE (full name == ELEM_VALUE_TYPE) which is also in the bundle.
   The SPECIAL RULES describe this in more detail.

   SUGGESTED implementation:
   It is recommended to implement a variadic function which takes a copy of the bundldef
   and allows the caller to follow this with the shortened names of optional
   elements to be included in the final bundle.

   (To assist this it may be wise to implement a function to display the shortened names of items
   inside a bundledef, noting the name, type and whether they are optional or not).

   in a bundle definition, the full name is always of the format ELEM_DOMAIN_ITEM,
   or ATTR_DOMAIN_ITEM. The type can be looked up, and the element listed as optional or mandatory.

   Names in the parameters not matching the shortened name of an element in the bundledef
   should not be created, and the bundle builder function should throw an error and return NULL.

   After adding the optional items provided in the parameter list by the caller, the creator
   function should include any remaining non optional elements and set their default values.

   Some bundles provide an optional INTROSPECTION_QUARKS element which can (should) be used to
   hold the "pruned" list of quarks; in this case there will also be an optional
   INTROSPECTION_QUARKS_PTR element which can be used instead to hold a pointer to
   a static copy of the pruned quarks.

   Following the bundle creation, the values in a bundle may be updated,
   simply by passing the bundle, shortened name and data of the correct TYPE
   and numeration (scalar or array).

   For attributes, an attr_bundle should be created and the values set accordingly.

   Optional elements may be added to or removed from the bundle at any time, unless a SPECIAL RULE
   prevnts this.

   The TYPE of an element MUST always be as defined in the bundledef (accounting for
   any SPECIAL RULES) and arrays and scalars may not be switched from one to the other.

   When freeing (unreferencing) a bundle, the implementor should first unreference (recursively)
   any sub bundles pointed to before freeing the bundle itself.

   SPECIAL RULES:
   1.	For the items, ELEM_VALUE_DATA, ELEM_VALUE_DEFAULT and ELEM_VALUE_NEW_DEFAULT,
   the data type is determined by the value of the ELEM_VALUE_TYPE item
   which MUST be present in the same bundle. The number of data elements which can be set is
   defined by the ELEM_VALUE_MAX_REPEATS item, which may optionally be present in the same
   bundle. If ELEM_VALUE_DEFAULT is set and ELEM_VALUE_DATA is not set, then ELEM_VALUE_DEFAULT
   should copied to ELEM_VALUE_DATA.

   2.	For item ELEM_ICAP_CAPACITIES, the type is BUNDLEPTR, intialised as NULL.
   The bundle_type is icap_bundle - a special bundle;
   EVERY possible valid name is considered an optional element inside it.
   The data in these elements is irrelevant, the only factor is whether an item with a given
   name exists or not.
   (It is suggeseted to prefix the names internally, e.g with "CAP_"
   to ensure these items do not clash with "fundamental" items in the sub-bundle.)

   Additionally, if a bundle containg icap_bundle is copied, a copy of the sub bundle
   should be made in the new container bundle, with the exact same items as the original
   (but see Rule 3 for an exception)

   3.	For item ELEM_GENERIC_UID, a randomly generate uint64_t number should be generated
   as the default value and never changed.	If a bundle containing this element is copied,
   then a new random value shall be generated for the copy bundle.

   4.	Items ELEM_LIST_NEXT and ELEM_LIST_PREV may be used like any normal linked list pointers
   (doubly linked if LIST_PREV is present, otherwise singly linked). They are pointers to
   other instances of the same bundle type containing them. and the usual methods for
   appending, listing, freeing and removing nodes should be implemented.

   5.	The item ELEM_CONTRACT_ATTR_POOL is a doubly linked list with data type list_header_bundle.
   List header bundle  has a slot for "owner object" and a pointer to a
   sub list of attributes.

   If an item is to be added to the pool, the caller must check if the item already exists
   in any of the existing sub nodes - if so do not re create it.

   If the item does not exist in any subnode, then the attribute should be added to
   the object's own list. (If the object does not have an own list,
   then it should create a list_header_bundle, set the owner to self, and add it to the pool list.)

   In this way, it is possible to free the main list without having to free the sub lists
   containing items (attributes) belonging to other objects. When the list is freed simply
   detach all the subnodes, and allow each owner object to free its own sub list.
   The implementation will require some means by which owner objects can be notified when
   they should free their own sub lists.
**/

typedef const char *bundle_element;
typedef const char **const_bundledef_t;
typedef char **bundledef_t;

#define _GET_TYPE(a, b, c) _ELEM_TYPE_##a
#define _GET_MAX_REPEATS(a, b, c) b
#define _GET_DEFAULT(a, b, c) #c

#define _CALL(MACRO, ...) MACRO(__VA_ARGS__)

#define GET_ELEM_TYPE(xdomain, xitem) _CALL(_GET_TYPE, ELEM_##xdomain##_##xitem##_TYPE)

#define GET_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, ELEM_##xdomain##_##xitem##_TYPE)

#define GET_MAX_REPEATS(xdomain, xitem) _CALL(_GET_MAX_REPEATS, ELEM_##xdomain##_##xitem##_TYPE)

// TODO - need some method to get these, ideally without forcing all attibutes to
// 		#define 3 unused values

#define ELEM_END NULL

/// builtin bundledefs ////

#define JOIN(a, b) GET_ELEM_TYPE(a, b) ELEM_NAMEU(#a, #b)

#define _ADD_ELEM(domain, item) JOIN(domain, item)
#define _ADD_OPT_ELEM(domain, item) "?" JOIN(domain, item)
#define _ADD_ELEM2(domain, item) ((GET_MAX_REPEATS(domain, item) == -1) \
				  ? "1 " GET_DEFAULT(domain, item)	\
				  : "0 " GET_DEFAULT(domain, item))

#define ADD_ELEM(d, i) _ADD_ELEM(d, i) , _ADD_ELEM2(d, i)
#define ADD_OPT_ELEM(d, i) _ADD_OPT_ELEM(d, i), _ADD_ELEM2(d, i)

// then, we can create a linked list of these values. a_slist_bundle_p
// should be a pointer to a bundle created from the slist_bundle
// definition, and the second param should be the bundle definition to
// be created
#define LLIST_MAKE(a_list_bundle_p, ...) do {llist_make(a_list_handle_p, __VA_ARGS__);) while (0);

#define BUNDLE_COMMENT(text) "#" text
#define _BUNDLE_COMMENT(text) BUNDLE_COMMENT(#text)
#define BUNDLE_EXTENDS(what) _BUNDLE_COMMENT(extends what##_BUNDLE {),  _##what##_BUNDLE, \
    BUNDLE_COMMENT("}")
#define BUNDLE_INCLUDES(what) _BUNDLE_COMMENT(includes what##_BUNDLE {),  _##what##_BUNDLE, \
    BUNDLE_COMMENT("}")

#define _BASE_BUNDLE BUNDLE_COMMENT("BUNDLE BASE"), ADD_OPT_ELEM(BASE, VERSION), \
    ADD_OPT_ELEM(INTROSPECTION, QUARKS), ADD_OPT_ELEM(INTROSPECTION, QUARKS_PTR), \
    ADD_OPT_ELEM(GENERIC, UID), ADD_OPT_ELEM(INTROSPECTION, COMMENT),	\
    ADD_OPT_ELEM(INTROSPECTION, PRIVATE_DATA)

// a bundle that just holds a single data item
#define _VALUE_BUNDLE BUNDLE_COMMENT("VALUE BUNDLE"), BUNDLE_EXTENDS(BASE), \
    ADD_ELEM(GENERIC, NAME), ADD_ELEM(VALUE, VALUE_TYPE), ADD_OPT_ELEM(VALUE, DATA)
#define VALUE_BUNDLE _VALUE_BUNDLE, ELEM_END

// a bundle that defines object type / subtype / state
#define _OBJDEF_BUNDLE ADD_ELEM(OBJECT, TYPE), ADD_ELEM(OBJECT, SUBTYPE), ADD_ELEM(OBJECT, STATE)
#define OBJDEF_BUNDLE _OBJDEF_BUNDLE, ELEM_END

// a bundle that defines object type with optional subtype / state
// can also be used to enumerate a bundle_type
#define _OBJDEF_OPT_BUNDLE ADD_OPT_ELEM(OBJECT, TYPE), ADD_OPT_ELEM(OBJECT, SUBTYPE), \
    ADD_OPT_ELEM(OBJECT, STATE)
#define OBJDEF_OPT_BUNDLE _OBJDEF_OPT_BUNDLE, ELEM_END

#define _NATIVE_BUNDLE  ADD_OPT_ELEM(INTROSPECTION, NATIVE_TYPE), \
    ADD_OPT_ELEM(INTROSPECTION, NATIVE_SIZE), ADD_OPT_ELEM(INTROSPECTION, NATIVE_PTR)
#define NATIVE_BUNDLE _NATIVE_BUNDLE, ELEM_END

/////////////////////

// a bundle that defines a standard object attribute
#define _ATTR_BUNDLE BUNDLE_COMMENT("STANDARD OBJECT ATTRIBUTE"), BUNDLE_EXTENDS(BASE), \
    BUNDLE_INCLUDES(VALUE), ADD_ELEM(VALUE, DEFAULT),			\
    ADD_OPT_ELEM(VALUE, MAX_REPEATS), ADD_OPT_ELEM(VALUE, NEW_DEFAULT), \
    ADD_ELEM(INTROSPECTION, REFCOUNT), BUNDLE_INCLUDES(NATIVE)
#define ATTR_BUNDLE _ATTR_BUNDLE, ELEM_END

// a base bundle with pointer to attribute, and pointer to "owner" object
// generally this would indicate an attribute inside the object's own attributes
// unless the object is a proxy for another
#define _ATTR_CONNECTION_BUNDLE ADD_ELEM(ATTRIBUTE, FOREIGN), ADD_ELEM(ATTRIBUTE, OWNER_OBJECT)
#define ATTR_CONNECTION_BUNDLE _ATTR_CONNECTION_BUNDLE, ELEM_END

// a hook bundle - defines the transform status change that triggers it
// along with an array of hook attributes - each one has a slot for a conneting
// in or out
#define _HOOK_BUNDLE ADD_ELEM(HOOK, STATUS_FROM), ADD_ELEM(HOOK, STATUS_TO), \
    ADD_ELEM(HOOK, ATTRIBUTES)
#define HOOK_BUNDLE _HOOK_BUNDLE, ELEM_END

// attribute type used in input hook functions, connects an object attribute
// (within the contract or within the object providing the contract ?)
// to a pointer to an attribute owned by another object
#define _HOOK_ATTR_BUNDLE BUNDLE_EXTENDS(ATTR_CONNECTION), ADD_ELEM(ATTRIBUTE, LOCAL)
#define HOOK_ATTR_BUNDLE _HOOK_ATTR_BUNDLE, ELEM_END

// intent / capacities bundle
#define _ICAP_BUNDLE BUNDLE_COMMENT("ICAP BUNDLE"), ADD_OPT_ELEM(GENERIC, DESCRIPTION), \
    ADD_ELEM(ICAP, INTENTION), ADD_ELEM(ICAP, CAPACITIES)
#define ICAP_BUNDLE _ICAP_BUNDLE, ELEM_END

// base bundle for objects / instances
#define _OBJ_BUNDLE BUNDLE_COMMENT("BASE OBJECT BUNDLE"),		\
    BUNDLE_INCLUDES(BASE), BUNDLE_INCLUDES(OBJDEF), BUNDLE_INCLUDES(ICAP), \
    ADD_OPT_ELEM(OBJECT, ATTRIBUTES), ADD_OPT_ELEM(OBJECT, ACTIVE_TRANSFORMS), \
    ADD_OPT_ELEM(OBJECT, CONTRACTS), ADD_OPT_ELEM(OBJECT, HOOKS),	\
    ADD_ELEM(INTROSPECTION, REFCOUNT)
#define OBJ_BUNDLE _OBJ_BUNDLE, ELEM_END

#define _VIDEO_TRACK_BUNDLE BUNDLE_COMMENT("Matroska Video Track"), BUNDLE_EXTENDS(BASE), \
    ADD_ELEM(VIDEO, FRAME_RATE),					\
    ADD_ELEM(VIDEO, DISPLAY_WIDTH), ADD_ELEM(VIDEO, DISPLAY_HEIGHT),	\
    ADD_ELEM(VIDEO, PIXEL_WIDTH), ADD_ELEM(VIDEO, PIXEL_HEIGHT),	\
    ADD_ELEM(VIDEO, COLOR_SPACE), ADD_ELEM(VIDEO, STEREO_MODE),	\
    ADD_ELEM(VIDEO, FLAG_INTERLACED)
#define VIDEO_TRACK_BUNDLE _VIDEO_TRACK_BUNDLE, ELEM_END

#define _STORAGE_BUNDLE BUNDLE_COMMENT("A utility bundle for stroring things"), ADD_OPT_ELEM(GENERIC, UID), \
    BUNDLE_INCLUDES(NATIVE), ADD_OPT_ELEM(INTROSPECTION, COMMENT)
#define STORAGE_BUNDLE _STORAGE_BUNDLE, ELEM_END

// contract will have std transforms: create instance -> create in state PREVIEW
// copy instance -> create copy with state prepare
// list_append
// list_remove
// ...

// base bundle for a singly linked list
#define _SLIST_BUNDLE ADD_ELEM(LIST, NEXT)
#define SLIST_BUNDLE _SLIST_BUNDLE, ELEM_END

// base bundle for a doubly linked list
#define _DLIST_BUNDLE BUNDLE_EXTENDS(SLIST), ADD_ELEM(LIST, PREV)
#define DLIST_BUNDLE _DLIST_BUNDLE, ELEM_END

// special bundles for "contract" objects

// a list 'header' with owner uid and pointer to attr_list_bundle
#define _LIST_HEADER_BUNDLE ADD_ELEM(ATTRIBUTE, OWNER_OBJECT), ADD_ELEM(LIST, ATTRDATA)
#define LIST_HEADER_BUNDLE _LIST_HEADER_BUNDLE, ELEM_END

// a single linked list of attribute *
#define _ATTR_LIST_BUNDLE BUNDLE_EXTENDS(SLIST), ADD_ELEM(LIST, ATTRDATA)
#define ATTR_LIST_BUNDLE _ATTR_LIST_BUNDLE, ELEM_END

// a dlist with data pointer to list_header
#define _LIST_OF_LISTS_BUNDLE BUNDLE_INCLUDES(DLIST), ADD_ELEM(LIST, OWNED_LIST)
#define LIST_OF_LISTS_BUNDLE _LIST_OF_LISTS_BUNDLE, ELEM_END

#define _CONTRACT_BUNDLE BUNDLE_COMMENT("CONTRACT_BUNDLE"), BUNDLE_EXTENDS(OBJ), \
    ADD_OPT_ELEM(CONTRACT, ATTR_POOL)
#define CONTRACT_BUNDLE _CONTRACT_BUNDLE, ELEM_END

// flag bits may optionally be used to store information derived from the quarks
#define ELEM_FLAG_COMMENT 		(1ull << 0)	// denotes a comment entry
#define ELEM_FLAG_OPTIONAL 		(1ull << 1)	// denotes the entry is optional
#define ELEM_FLAG_ARRAY 		(iull << 2)	// denotes the data type is array

typedef enum {
  attr_bundle, // x
  object_bundle, // x
  objdef_bundle, // x
  icap_bundle, // x
  hook_bundle, // x
  slist_bundle, // x
  dlist_bundle, // x
  //
  // sub-bundles
  hook_attr_bundle,
  attr_ptr_bundle,
  list_of_lists_bundle,
  list_header_bundle,
  attr_list_bundle,
  value_bundle,
  vtrack_bundle,
  storage_bundle,
  n_bundles
} bundle_type;

#ifdef NEED_OBJECT_BUNDLES
#ifndef HAVE_OBJECT_BUNDLES
#undef NEED_OBJECT_BUNDLES
#define HAVE_OBJECT_BUNDLES

#define GET_BUNDLEDEF(btype) (get_bundledef((btype))

#define ADD_BUNDLE(a, b) static const bundle_element a##_bundledef[] = {b##_BUNDLE};

#define CBUN(name) (const_bundledef_t)(name##_bundledef),

ADD_BUNDLE(attr, ATTR)
ADD_BUNDLE(obj, OBJ)
ADD_BUNDLE(objdef, OBJDEF)
ADD_BUNDLE(icap, ICAP)
ADD_BUNDLE(hook, HOOK)
ADD_BUNDLE(hook_attr, HOOK_ATTR)
ADD_BUNDLE(attr_connection, ATTR_CONNECTION)
ADD_BUNDLE(slist, SLIST)
ADD_BUNDLE(dlist, DLIST)
ADD_BUNDLE(storage, STORAGE)
ADD_BUNDLE(native, NATIVE)
ADD_BUNDLE(value, VALUE)
ADD_BUNDLE(list_of_lists, LIST_OF_LISTS)
ADD_BUNDLE(list_header, LIST_HEADER)
ADD_BUNDLE(attr_list, ATTR_LIST)
ADD_BUNDLE(vtrack, VIDEO_TRACK)

static const_bundledef_t bundledefs[] = {
  CBUN(attr) CBUN(obj) CBUN(objdef) CBUN(icap) CBUN(hook) CBUN(slist) CBUN(dlist) CBUN(hook_attr)
  CBUN(storage) CBUN(native) CBUN(value) CBUN(hook_attr) CBUN(attr_connection) CBUN(list_of_lists)
  CBUN(list_header) CBUN(attr_list) CBUN(vtrack) CBUN(storage)
};

const_bundledef_t get_bundledef(bundle_type btype) {
  if (btype < 0 || btype >= n_bundles) return NULL;
  return bundledefs[btype];
}

#endif
#else
extern const_bundledef_t get_bundledef(bundle_type type);
#endif

#endif

