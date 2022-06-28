// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef OBJECT_CONSTANTS_H
#define OBJECT_CONSTANTS_H

typedef enum {
  atom_bundle, // x
  attr_bundle, // x
  object_bundle, // x
  objdef_bundle, // x
  icap_bundle, // x
  hook_bundle, // x
  // sub-bundles
  hook_attr_bundle,
  attr_ptr_bundle,
  //
  list_bundle, // x
  list_of_lists_bundle,
  list_header_bundle,
  attr_list_bundle,
  n_bundles
} bundle_type;

typedef const char **bundledef_t;

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

// INTENTIONS
enum {
  // some common intentions
  // internal or (possibly) non-functional types
  OBJ_INTENTION_UNKNOWN,

  // application types
  OBJ_INTENTION_NOTHING = 0,

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

////// standard ATTRIBUTES ////

///////// attribute types ////////

#define ATTR_TYPE_NONE					"0"

// flag bits
#define ATTR_TYPE_FLAG_OPTIONAL				"?"	// "?"
#define ATTR_TYPE_FLAG_ARRAY				"*"	// "*"

#define ATTR_TYPE_INT					"i"	// "i"
#define ATTR_TYPE_DOUBLE				"d"	// "d"
#define ATTR_TYPE_BOOLEAN				"b"	// "b"
#define ATTR_TYPE_STRING				"s"	// "s"
#define ATTR_TYPE_INT64					"I"	// "I"
#define ATTR_TYPE_UINT					"u"	// "u"
#define ATTR_TYPE_FLOAT					"f"	// "f"
#define ATTR_TYPE_UINT64				"U"	// "U"
#define ATTR_TYPE_CHAR					"c"	// "c"

#define ATTR_TYPE_VOIDPTR				"v"	// "v"

// void * aliases
#define ATTR_TYPE_BUNDLEPTR	       			"B" // ptr to bundle "B"
#define ATTR_TYPE_OBJPTR				"O" // == ptr to obj_bundle "O"
#define ATTR_TYPE_ATTRPTR      				"A" // == ptr to attr_bundle "A"
#define ATTR_TYPE_HOOKPTR      				"H" // == ptr to hook_bundle "H"

#define ATTR_TYPE_SPECIAL				"$" // type rqeuires special handling "$"

#define UATTR_TYPE_NONE					(uint32_t)'0'

// flag bits
#define UATTR_TYPE_FLAG_OPTIONAL	      		(uint32_t)'?'	// '?'
#define UATTR_TYPE_FLAG_ARRAY				(uint32_t)'*'	// '*'

#define UATTR_TYPE_INT					(uint32_t)'i'	// 'i'
#define UATTR_TYPE_DOUBLE				(uint32_t)'d'	// 'd'
#define UATTR_TYPE_BOOLEAN				(uint32_t)'b'	// 'b'
#define UATTR_TYPE_STRING				(uint32_t)'s'	// 's'
#define UATTR_TYPE_INT64	       		     	(uint32_t)'I'	// 'I'
#define UATTR_TYPE_UINT					(uint32_t)'u'	// 'u'
#define UATTR_TYPE_FLOAT	       			(uint32_t)'f'	// 'f'
#define UATTR_TYPE_UINT64				(uint32_t)'U'	// 'U'
#define UATTR_TYPE_CHAR					(uint32_t)'c'	// 'c'

#define UATTR_TYPE_VOIDPTR				(uint32_t)'v'	// 'v'

// void * aliases
#define UATTR_TYPE_BUNDLEPTR	       			(uint32_t)'B' // ptr to bundle 'B'
#define UATTR_TYPE_OBJPTR				(uint32_t)'O' // == ptr to obj_bundle 'O'
#define UATTR_TYPE_ATTRPTR     				(uint32_t)'A' // == ptr to attr_bundle 'A'
#define UATTR_TYPE_HOOKPTR     				(uint32_t)'H' // == ptr to hook_bundle 'H'

#define UATTR_TYPE_SPECIAL				(uint32_t)'$' // type rqeuires special handling "$"

////////////////////////////

#define ATTR_NAMEU(a, b) a "_" b
#define _ATTR_NAMER(c) #c
#define ATTR_NAME(a, b) ATTR_NAMEU(a, b)

// bundle domain


#define ATTR_BUNDLE_FLAGS				ATTR_NAME("DOMAIN", "FLAGS")
#define ATTR_BUNDLE_FLAGS_TYPE              		SPECIAL, 1

// domain generic

#define ATTR_GENERIC_NAME				ATTR_NAME("GENERIC", "NAME")
#define ATTR_GENERIC_NAME_TYPE              		STRING, 1

#define ATTR_GENERIC_TYPE				ATTR_NAME("GENERIC", "TYPE")
#define ATTR_GENERIC_TYPE_TYPE              		UINT, 1

#define ATTR_GENERIC_FLAGS				ATTR_NAME("GENERIC", "FLAGS")
#define ATTR_GENERIC_FLAGS_TYPE              		UINT64, 1

#define ATTR_GENERIC_UID				ATTR_NAME("GENERIC", "UID")
#define ATTR_GENERIC_UID_TYPE				UINT64, 1

// currently active transforms ?
#define ATTR_GENERIC_TRANSFORMS				"transforms"
#define ATTR_GENERIC_TRANSFORMS_TYPE			OBJPTR, -1
#define ATTR_GENERIC_TRANSFORMS_OBJDEF				\
  OBJECT_TYPE_TRANSFORM, OBJECT_SUBTYPE_ANY, OBJECT_STATE_ANY

// each hook has attributes - state from / state to, attributes. Each sub attribute
// has a flags attribute, and a provider attribute, which must be set to the corresponding
// attribute of some object which will supply data, the connected attribute
// may also have flags like 'needs update'
#define ATTR_GENERIC_HOOKS				"hooks"
#define ATTR_GENERIC_HOOKS_TYPE		       	       	HOOKPTR, -1

#define ATTR_HOOK_STATUS_FROM				"hook_status_from"
#define ATTR_HOOK_STATUS_FROM_TYPE       	       	INT, 1

#define ATTR_HOOK_STATUS_TO				"hook_status_to"
#define ATTR_HOOK_STATUS_TO_TYPE       	       		INT, 1

#define ATTR_HOOK_ATTRIBUTES				"hook_attributes"
#define ATTR_HOOK_ATTRIBUTES_TYPE       	       	BUNDLEPTR, -1
#define ATTR_HOOK_ATTRIBUTES_BUNDLE_TYPE       	       	hook_attr_bundle

////////// domain object

#define ATTR_OBJECT_ATTRIBUTES				"attributes"
#define ATTR_OBJECT_ATTRIBUTES_TYPE	       		ATTRPTR, -1

#define ATTR_OBJECT_TYPE				ATTR_NAME("OBJECT", "TYPE")
#define ATTR_OBJECT_TYPE_TYPE				UINT64, 1

#define ATTR_OBJECT_SUBTYPE				ATTR_NAME("OBJECT", "SUBTYPE")
#define ATTR_OBJECT_SUBTYPE_TYPE       			UINT64, 1

#define ATTR_OBJECT_STATE				ATTR_NAME("OBJECT", "STATE")
#define ATTR_OBJECT_STATE_TYPE       			INT, 1

#define ATTR_OBJECT_CONTRACTS				"contracts"
#define ATTR_OBJECT_CONTRACTS_TYPE       		OBJPTR, -1

#define ATTR_OBJECT_CONTRACTS_OBJDEF					\
  OBJECT_TYPE_CONTRACT, OBJECT_SUBTYPE_ANY, OBJECT_STATE_PREVIEW

// max number of elements allowed in data, a value of -1 means no limit
// for scalar values, 0 or 1 indicates a single value, -1 indicates an unbounded array
// for attributes, this is resolved as.-1 (unbounded), or MAX(1, max_repeats, nvalues(default))
#define ATTR_VALUE_MAX_REPEATS				"max_repeats"
#define ATTR_VALUE_MAX_REPEATS_TYPE	       		INT, 1

// this stores the actual data of the scalar value.
// the type and number of elements must accord with value_type and max_repeats
// (0 or 1 for a single value, -1 for an array)
#define ATTR_VALUE_VALUE	      			ATTR_NAME("VALUE", "VALUE")
#define ATTR_VALUE_VALUE_TYPE       			SPECIAL, 0

#define ATTR_VALUE_DEFALT	      			"value_default"
#define ATTR_VALUE_DEFAULT_TYPE       			SPECIAL, 0

#define ATTR_VALUE_NEW_DEFALT	      			"value_new_default"
#define ATTR_VALUE_NEW_DEFAULT_TYPE   			SPECIAL, 0

// domain introspection

#define ATTR_INTROSPECTION_DOMAIN    			ATTR_NAME("INTROSPECTION", "DOMAIN")
#define ATTR_INTROSPECTION_DOMAIN_TYPE       	       	STRING, 1

#define ATTR_INTROSPECTION_COMMENT    			ATTR_NAME("INTROSPECTION", "COMMENT")
#define ATTR_INTROSPECTION_COMMENT_TYPE       	       	STRING, 1

#define ATTR_INTROSPECTION_REFCOUNT    			"refcount"
#define ATTR_INTROSPECTION_REFCOUNT_TYPE       	       	INT, 1

#define ATTR_INTROSPECTION_PRIVATE_DATA			ATTR_NAME("INTROSPECTION", "PRIVATE_DATA")
#define ATTR_INTROSPECTION_PRIVATE_DATA_TYPE		VOIDPTR, -1

#define ATTR_INTROSPECTION_NATIVE_PTR 			ATTR_NAME("INTROSPECTION", "NATIVE_PTR")
#define ATTR_INTROSPECTION_NATIVE_PTR_TYPE		VOIDPTR, -1

#define ATTR_INTROSPECTION_NATIVE_TYPE 			ATTR_NAME("INTROSPECTION", "NATIVE_TYPE")
#define ATTR_INTROSPECTION_NATIVE_TYPE_TYPE 	       	INT, -1

#define ATTR_INTROSPECTION_NATIVE_SIZE 			ATTR_NAME("INTROSPECTION", "NATIVE_SIZE")
#define ATTR_INTROSPECTION_NATIVE_SIZE_TYPE		UINT64, -1

// domain attribute

#define ATTR_ATTRIBUTE_OWNER				"owner_object"
#define ATTR_ATTRIBUTE_OWNER_TYPE      			OBJPTR, 1
#define ATTR_ATTRIBUTE_OWNER_OBJDEF			\
  OBJECT_TYPE_ANY, OBJECT_SUBTYPE_ANY, OBJECT_STATE_ANY

#define ATTR_ATTRIBUTE_TO				"attr_to"
#define ATTR_ATTRIBUTE_TO_TYPE				ATTRPTR, 1

#define ATTR_ATTRIBUTE_ATTRPTR				"attr_pointer"
#define ATTR_ATTRIBUTE_ATTRPTR_TYPE			ATTRPTR, 1

////////// domain icap /////////

#define ATTR_ICAP_DESCRIPTION				"description"
#define ATTR_ICAP_DESCRIPTION_TYPE              	STRING, 1

#define ATTR_ICAP_INTENTION				"intention"
#define ATTR_ICAP_INTENTION_TYPE              		INT, 1

#define ATTR_ICAP_CAPACITIES				"CAP_*"
#define ATTR_ICAP_CAPACITIES_TYPE              		SPECIAL, -1 // name is prefixed, val is bool

/////

#define ATTR_LIST_NEXT					"next"
#define ATTR_LIST_NEXT_TYPE              		BUNDLEPTR, 1
#define ATTR_LIST_NEXT_BUNDLE_TYPE              	list_bundle

#define ATTR_LIST_PREV					"prev"
#define ATTR_LIST_PREV_TYPE              		BUNDLEPTR, 1
#define ATTR_LIST_PREV_BUNDLE_TYPE              	list_bundle

#define ATTR_LIST_ATTRDATA				"data"
#define ATTR_LIST_ATTRDATA_TYPE              		ATTRPTR, 1

#define ATTR_LIST_LISTDATA				"data"
#define ATTR_LIST_LISTDATA_TYPE              		BUNDLEPTR, 1
#define ATTR_LIST_LISTDATA_BUNDLE_TYPE              	list_header_bundle

#define ATTR_LIST_ATTRLIST				"data"
#define ATTR_LIST_ATTRLIST_TYPE              		BUNDLEPTR, 1
#define ATTR_LIST_ATTRLIST_BUNDLE_TYPE              	attr_list_bundle

/// domain URI
#define ATTR_URI_FILENAME				ATTR_NAME("URI", "FILENAME")

/// domain contract

// attributes for a contract (replaces normal attributes)
#define ATTR_CONTRACT_ATTRIBUTES		      	"attributes"
#define ATTR_CONTRACT_ATTRIBUTES_TYPE		       	SPECIAL, -1
//#define ATTR_CONTRACT_ATTRIBUTES_BUNDLE_TYPE	       	list_of_lists_bundle

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

// video
#define ATTR_VIDEO_FPS					"fps"

// UI (can be general - ie. standalod, or a sub attribute of a main attribute)
#define ATTR_UI_TEMPLATE "ui_template"

// attribute flag bits //
// these flagbits are for input attributes, for output attributes they are ignored
#define OBJBUNDLE_FLAG_READONLY 		0x00001
#define OBJBUNDLE_FLAG_MANDATORY 		0x00002
//
// output value - indicates that the value will be updated by a transform
// at the end of processing, and / or in a hook during the transform
#define OBJBUNDLE_FLAG_OUTPUT	 	0x1000

// attr value may update spontaneously (may or may not have a value changed hook)
// otherwise the value will only update as the result of a transform
#define OBJBUNDLE_FLAG_VOLATILE	 	0x2000

// each update returns the next value in a sequence, i.e it should only be updated
// once for each read, otherwise, the value returned is the "current value"
#define OBJBUNDLE_FLAG_SEQUENTIAL	 	0x4000

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
#define OBJ_TRANSFORM_ERROR_REQ -1 // not all requirements to run the transform have been satisfied
#define OBJ_TRANSFORM_STATUS_NONE 0
#define OBJ_TRANSFORM_STATUS_SUCCESS 1	///< normal / success
#define OBJ_TRANSFORM_STATUS_RUNNING 16	///< transform is "running" and the state cannot be changed
#define OBJ_TRANSFORM_STATUS_NEEDS_DATA 32	///< reqmts. need updating
#define OBJ_TRANSFORM_STATUS_CANCELLED 256	///< transform was cancelled during running
#define OBJ_TRANSFORM_STATUS_ERROR  512	///< transform encountered an error during running

/////////////////////////// bundles //

#ifdef NEED_OBJECT_BUNDLES
#ifndef HAVE_OBJECT_BUNDLES
#undef NEED_OBJECT_BUNDLES
#define HAVE_OBJECT_BUNDLES

#define _GET_QUARK_TYPE(a) (char)((uint8_t)ATTR_TYPE_##a)
#define _GET_TYPE(a, b) ATTR_TYPE_##a
#define _GET_MAX_REPEATS(a, b) b
#define _GET_OBJ_TYPE(a, b, c) a
#define _GET_OBJ_SUBTYPE(a, b, c) b
#define _GET_OBJ_STATE(a, b, c) c
#define _CALL(MACRO, ...) MACRO(__VA_ARGS__)

#define GET_QUARK_TYPE(item) _CALL(_GET_QUARK_TYPE, ATTR_##xitem##_TYPE)
#define GET_ATTR_TYPE(xdomain, xitem) _CALL(_GET_TYPE, ATTR_##xdomain##_##xitem##_TYPE)

#define GET_MAX_REPEATS(xdomain, xitem) _CALL(_GET_MAX_REPEATS, ATTR_##xdomain##_##xitem##_TYPE)
#define GET_BUNDLE_TYPE(xdomain, xitem) ATTR_##xdomain##_##xitem##_BUNDLE_TYPE
#define GET_OBJ_TYPE(xdomain, xitem) _CALL(_GET_OBJ_TYPE, ATTR_##xdomain##_##xitem##_OBJDEF)
#define GET_OBJ_SUBTYPE(xdomain, xitem) _CALL(_GET_OBJ_SUBTYPE, ATTR_##xdomain##_##xitem##_OBJDEF)
#define GET_OBJ_STATE(xdomain, xitem) _CALL(_GET_OBJ_STATE, ATTR_##xdomain##_##xitem##_OBJDEF)

#define ATTR_END NULL

/// builtin bundledefs ////
static char *quark;

#define VSNPRINTF(c, len, fmt, args) (vsnprintf(c, len, fmy, args) ? c : NULL)

#define STRNDUP_VPRINTF(fmt, len, vargs) (quark = (*calloc_f)(1, len)) ? (VSNPRINTF(quark, len, fmt, vargs) : NULL)

#define STRNDUP_PRINTF(fmt, len, ...) STRNDUP_VPRINTF(fmt, len, __VA_ARGS__)

// at runtime, _ATTR_QUARK(item) will return an allocated string "char(item_type) item"
#define MAKE_QUARK(item) (STRNDUP_PRINTF("%c%s", strlen((item)) + 2, GET_QUARK_TYPE((item)), #item))

#define MAKE__OPT_QUARK(item) (STRNDUP_PRINTF("?%c%s", strlen((item)) + 3, \
					     GET_QUARK_TYPE((item)), #item))
#define QUOTE(c) #c
#define _JOIN(a, b) a##b
#define JOIN(a, b) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#a, #b)
#define JOIN3(a, b, c) "/**/a/**/b/**/c/**/"

#define _ATTR_QUARK(domain, item) JOIN(domain, item)


#define _ATTR_OPT_QUARK(domain, item) "?" JOIN(domain, item)

//#define _ATTR_QUARK2(domain, item) ((GET_MAX_REPEATS(domain, item) == -1) ? "1" : "0")

#define ATTR_QUARK(d, i) _ATTR_QUARK(d, i) //, _ATTR_QUARK2(d, i)
#define ATTR_OPT_QUARK(d, i) _ATTR_OPT_QUARK(d, i)//, _ATTR_QUARK2(d, i)

// then, we can create a linked list of these values. a_slist_bundle_p
// should be a pointer to a bundle created from the slist_bundle
// definition, and the second param should be the bundle definition to
// be created
#define LLIST_MAKE(a_list_bundle_p, ...) do {llist_make(a_list_handle_p, __VA_ARGS__);) while (0);

#define _VALUE_BUNDLE ATTR_QUARK(GENERIC, NAME), ATTR_QUARK(GENERIC, TYPE), \
    ATTR_OPT_QUARK(VALUE, VALUE), ATTR_OPT_QUARK(BUNDLE, FLAGS), \
    ATTR_OPT_QUARK(INTROSPECTION, COMMENT), ATTR_OPT_QUARK(INTROSPECTION, PRIVATE_DATA)

#define VALUE_BUNDLE _VALUE_BUNDLE, ATTR_END

#define _OBJDEF_BUNDLE ATTR_QUARK(OBJECT, TYPE), ATTR_QUARK(OBJECT, SUBTYPE), ATTR_QUARK(OBJECT, STATE)
#define OBJDEF_BUNDLE _OBJDEF_BUNDLE, ATTR_END

#define _OBJDEF_OPT_BUNDLE ATTR_OPT_QUARK(OBJECT, TYPE), ATTR_OPT_QUARK(OBJECT, SUBTYPE), \
    ATTR_OPT_QUARK(OBJECT, STATE)
#define OBJDEF_OPT_BUNDLE _OBJDEF_OPT_BUNDLE, ATTR_END

/////////////////////

#define _ATOM_BUNDLE _VALUE_BUNDLE, _OBJDEF_OPT_BUNDLE
#define ATOM_BUNDLE _ATOM_BUNDLE, ATTR_END

// must maintain this order - type and flags msut come ahead of default and value
#define _ATTR_BUNDLE ATTR_QUARK(GENERIC, UID), _VALUE_BUNDLE, ATTR_QUARK(VALUE, DEFAULT),	\
      ATTR_OPT_QUARK(VALUE, MAX_REPEATS), ATTR_QUARK(INTROSPECTION, REFCOUNT)
#define ATTR_BUNDLE _ATTR_BUNDLE, ATTR_END

#define _ATTR_PTR_BUNDLE ATTR_QUARK(ATTRIBUTE, ATTRPTR), ATTR_QUARK(ATTRIBUTE, OWNER)

#define ATTR_PTR_BUNDLE _ATTR_PTR_BUNDLE, ATTR_END

#define _HOOK_ATTR_BUNDLE ATTR_QUARK(ATTRIBUTE, TO), _ATTR_PTR_BUNDLE
#define HOOK_ATTR_BUNDLE _HOOK_ATTR_BUNDLE, ATTR_END

#define _ICAP_BUNDLE ATTR_QUARK(ICAP, DESCRIPTION), ATTR_QUARK(ICAP, INTENTION), ATTR_QUARK(ICAP, CAPACITIES)
#define ICAP_BUNDLE _ICAP_BUNDLE, ATTR_END

#define _OBJ_BUNDLE ATTR_QUARK(GENERIC, UID), ATTR_QUARK(INTROSPECTION, REFCOUNT), _OBJDEF_BUNDLE, \
    _ICAP_BUNDLE, ATTR_QUARK(OBJECT, ATTRIBUTES), ATTR_QUARK(GENERIC, TRANSFORMS), \
      ATTR_QUARK(OBJECT, CONTRACTS),					\
      ATTR_QUARK(GENERIC, HOOKS), ATTR_QUARK(INTROSPECTION, PRIVATE_DATA)
#define OBJ_BUNDLE _OBJ_BUNDLE, ATTR_END

// contract will have std transforms: create instance -> create in state PREVIEW
// copy instance -> create copy with state prepare
// list_append
// list_remove
// ...

#define _HOOK_BUNDLE ATTR_QUARK(HOOK, STATUS_FROM), ATTR_QUARK(HOOK, STATUS_TO), \
    ATTR_QUARK(HOOK, ATTRIBUTES)
#define HOOK_BUNDLE _HOOK_BUNDLE, ATTR_END

#define _LIST_BUNDLE ATTR_QUARK(LIST, NEXT), ATTR_QUARK(LIST, PREV)
#define LIST_BUNDLE _LIST_BUNDLE, ATTR_END

#define _LIST_OF_LISTS_BUNDLE LIST_BUNDLE, ATTR_QUARK(LIST, LISTDATA)
#define LIST_OF_LISTS_BUNDLE _LIST_OF_LISTS_BUNDLE, ATTR_END

#define _LIST_HEADER_BUNDLE ATTR_QUARK(ATTRIBUTE, OWNER), ATTR_QUARK(LIST, ATTRLIST)
#define LIST_HEADER_BUNDLE _LIST_HEADER_BUNDLE, ATTR_END

#define _ATTR_LIST_BUNDLE LIST_BUNDLE, ATTR_QUARK(LIST, ATTRDATA)
#define ATTR_LIST_BUNDLE _ATTR_LIST_BUNDLE, ATTR_END

#define BUNDLE_FLAG_OPTIONAL 		1 // denotes the bundle is a linked list
#define BUNDLE_FLAG_ARRAY 		2 // denotes the bundle is a linked list

typedef const char *bundle_element;
typedef const char **bundledef_t;

static const bundle_element attr_bundledef[] = {ATTR_BUNDLE};

static const bundle_element obj_bundledef[] = {OBJ_BUNDLE};

static const bundle_element objdef_bundledef[] = {OBJDEF_BUNDLE,};

static const bundle_element icap_bundledef[] = {ICAP_BUNDLE};

static const bundle_element hook_bundledef[] =  {HOOK_BUNDLE};

static const bundle_element hook_attr_bundledef[] =  {HOOK_ATTR_BUNDLE};

static const bundle_element attr_ptr_bundledef[] =  {ATTR_PTR_BUNDLE};

static const bundle_element list_bundledef[] =  {LIST_BUNDLE};

static const bundle_element list_of_lists_bundledef[] = {LIST_OF_LISTS_BUNDLE};

static const bundle_element list_header_bundledef[] =  {LIST_HEADER_BUNDLE};

static const bundle_element attr_list_bundledef[] =  {ATTR_LIST_BUNDLE};

static const bundle_element value_bundledef[] =  {VALUE_BUNDLE};

static const bundledef_t bundledefs[] = {(const bundledef_t)attr_bundledef,
                                         (const bundledef_t)obj_bundledef,
                                         (const bundledef_t)objdef_bundledef,
                                         (const bundledef_t)icap_bundledef,
                                         (const bundledef_t)hook_bundledef,
                                         (const bundledef_t)hook_attr_bundledef,
                                         (const bundledef_t)attr_ptr_bundledef,
                                         (const bundledef_t)list_bundledef,
                                         (const bundledef_t)list_of_lists_bundledef,
                                         (const bundledef_t)list_header_bundledef,
                                         (const bundledef_t)attr_list_bundledef,
                                         (const bundledef_t) value_bundledef
                                        };

#define GET_BUNDLE_TYPE_FOR(whatever) (atom_bundle)
#define GET_BUNDLEDEF(btype) (get_bundledef((btype))


const bundledef_t *get_bundledef(bundle_type btype) {
  if (btype < object_bundle || btype >= n_bundles) return NULL;
  return &bundledefs[btype];
}

#endif
#else
extern const bundledef_t *get_bundledef(bundle_type type);
#endif

#endif

/* To make a custom bundle: */
/* #define ATTR_BUNDLE ATTR_QUARK(GENERIC, UID), ATTR_QUARK(ATTRIBUTE, NAME),	\ */
/*     ATTR_QUARK(ATTRIBUTE, TYPE), ATTR_QUARK(GENERIC, FLAGS),		\ */
/* ATTR_QUARK(ATTRIBUTE, DEFAULT), ATTR_QUARK(ATTRIBUTE, VALUE), ATTR_QUARK(INTROSPECTION, REFCOUNT)  */

/* int flags = 0; */
/* bundle_element elements[] = {ATTR_BUNDLE}; */
/* bundledef_t my_bundle = {flags, elements}; */
