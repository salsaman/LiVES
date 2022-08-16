// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef __cplusplus
extern "C"
{
#endif

//#define DEBUG_BUNDLE_MAKER

// the implementation should define its own NIRVA_BUNDLE_T before this header
#ifndef NIRVA_BUNDLE_T
#error NIRVA_BUNDLE_T MUST BE DEFINED
#endif

#ifdef IS_BUNDLE_MAKER
#ifndef _HAS_OBJC_MAKER
#ifdef OBJECT_CONSTANTS_H
#undef OBJECT_CONSTANTS_H
#define SKIP_MAIN
#endif
#endif
#endif

#ifndef OBJECT_CONSTANTS_H
#define OBJECT_CONSTANTS_H

#ifdef NIRVA_IMPL_STYLE_DEFAULT_CPP
#undef NIRVA_IMPL_STYLE_DEFAULT_CPP
#endif
#define NIRVA_IMPL_STYLE_DEFAULT_CPP 10001

#ifdef NIRVA_IMPL_STYLE_DEFAULT_C
#undef NIRVA_IMPL_STYLE_DEFAULT_C
#endif
#define NIRVA_IMPL_STYLE_DEFAULT_C 10002

#ifndef NIRVA_IMPL_STYLE
#ifdef __cplusplus
#define NIRVA_IMPL_STYLE NIRVA_IMPL_STYLE_DEFAULT_CPP
#else
#define NIRVA_IMPL_STYLE NIRVA_IMPL_STYLE_DEFAULT_C
#endif
#endif

#define NIRVA_IMPL_IS(style) (NIRVA_IMPL_STYLE == NIRVA_IMPL_STYLE_##style)

#if NIRVA_IMPL_IS(DEFAULT_CPP) || NIRVA_IMPL_IS(DEFAULT_C)
#define NIRVA_X_IMPL_C_CPP
#endif

#ifndef SKIP_MAIN

#define _CALL(MACRO, ...) MACRO(__VA_ARGS__)

#if NIRVA_IMPL_IS(DEFAULT_C)
#ifndef NO_STD_INCLUDES
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#endif

#define __IMPL_ENUM__ enum
#define __IMPL_EXTERN__ extern
#define __IMPL_NULL__ NULL
#define __IMPL_VARIADIC__ ...
#define __IMPL_CONST__ const
#define __IMPL_INLINE__ inline
#define __IMPL_FN_STATIC__ static
#define __IMPL_FILE_STATIC__ static
#define __IMPL_STATIC_INLINE__ __IMPL_FN_STATIC__ __IMPL_INLINE__
#define __IMPL_NO_VAL__ void
#define __IMPL_LINE_END__ ;
#define __IMPL_VA_START__(a, b) va_start(a,b)
#define __IMPL_VA_END__(a) va_end(a)
#define __IMPL_VA_ARG__(a, b) va_arg(a, b)
#define __IMPL_TYPEDEF__ typedef

#define __IMPL_GENPTR__ void *

#define __IMPL_DEF_ARRAY_OF__(a) a*
#define __IMPL_PTR_TO_TYPE__(a) a *
#define __IMPL_DECLARE_AS_TYPE_MULTI__(xtype,...) xtype __VA_ARGS__

#ifndef NIRVA_BOOLEAN
#define NIRVA_BOOLEAN int
#endif
#define NIRVA_INT int
#define NIRVA_UINT uint32_t
#define NIRVA_INT64 int64_t
#define NIRVA_UINT64 uint64_t
#define NIRVA_STRING char *
#define NIRVA_DOUBLE double
#define NIRVA_FLOAT float
#define NIRVA_VOIDPTR void *
#define NIRVA_VARIADIC ...
#define NIRVA_VA_LIST va_list
#define NIRVA_VA_START(a,b) NIRVA_CMD(__IMPL_VA_START__(a,b))
#define NIRVA_VA_END(a) NIRVA_CMD(__IMPL_VA_END__(a))
#define NIRVA_VA_ARG(a,b) __IMPL_VA_ARG__(a,b)
#define NIRVA_NULL __IMPL_NULL__
#define NIRVA_ENUM __IMPL_ENUM__
#define NIRVA_CONST __IMPL_CONST__
#define NIRVA_STATIC __IMPL_FN_STATIC__
#define NIRVA_STATIC_INLINE __IMPL_STATIC_INLINE__
#define NIRVA_EXTERN __IMPL_EXTERN__
#define NIRVA_NO_RETURN __IMPL_NO_VAL__
#define NIRVA_VOID __IMPL_NO_VAL__
#define NIRVA_GENPTR __IMPL_GENPTR__

#define NIRVA_EQUAL(a,b) ((a)==(b))
#define NIRVA_TYPED(a,...) __IMPL_TYPEDEF__ a __VA_ARGS__
#define NIRVA_TYPEDEF(a,...) NIRVA_TYPED(a,__VA_ARGS__)__IMPL_LINE_END__
#define NIRVA_TYPEDEF_MULTI(a,...) NIRVA_TYPEDEF(,__VA_ARGS__ a)
#define NIRVA_TYPEDEF_ENUM(typename,...) __IMPL_TYPEDEF__ NIRVA_ENUM {__VA_ARGS__,} \
  typename __IMPL_LINE_END__
#define NIRVA_FUNC_TYPE_DEF(ret_type,funcname,...) __IMPL_TYPEDEF__ \
  ret_type (* funcname)(__VA_ARGS__)__IMPL_LINE_END__
#define NIRVA_DEF_VARS(xtype, ...) NIRVA_CMD(__IMPL_DECLARE_AS_TYPE_MULTI__(xtype,__VA_ARGS__))

#define NIRVA_ARRAY_OF __IMPL_DEF_ARRAY_OF__
#define NIRVA_PTR_TO __IMPL_PTR_TO_TYPE__

#endif // C style

////// function return codes ////

#define NIRVA_RESULT_ERROR 			-1
#define NIRVA_RESULT_FAIL 			0
#define NIRVA_RESULT_SUCCESS 			1

// error in condition - could not evaluate
#define NIRVA_COND_ERROR		-1
// condition failed
#define NIRVA_COND_FAIL			0
// condition succeeded
#define NIRVA_COND_SUCCESS		1
// request to give more time and retry. If no other conditions fail then cond_once may
// emulate cond_retry
#define NIRVA_COND_WAIT_RETRY		2

// value that should be returned if you have no clue what else to do
#define NIRVA_COND_CLUELESS		8

// these values should only be returned from system callback ////s
// force conditions to succeed, even if some others would fail
// takes precedence over all other return codes, including NIRV_COND_ERROR
//exit and return NIRVA_COND_SUCCESS
#define NIRVA_COND_FORCE		16
// force condition fail, exit and do not retry
#define NIRVA_COND_ABANDON		17
/////

#define NIRVA_REQUEST_NO		NIRVA_COND_FAIL
#define NIRVA_REQUEST_YES		NIRVA_COND_SUCCESS
#define NIRVA_REQUEST_WAIT_RETRY	NIRVA_COND_WAIT_RETRY
#define NIRVA_REQUEST_NEEDS_PRIVILEGE	NIRVA_COND_ABANDON

// life is complicated and sometimes you just don't know what to do
#define NIRVA_REQUEST_NO_IDEA		NIRVA_COND_CLUELESS

// the request is taking too long, some other object will take over,
// go and do something else instead
#define NIRVA_REQUEST_PROXIED		NIRVA_COND_FORCE

// return values for non-conditional hook cbs
#define NIRVA_HOOK_CB_LAST		NIRVA_COND_FAIL
#define NIRVA_HOOK_CB_AGAIN		NIRVA_COND_SUCCESS

#define ashift(v0, v1) ((uint64_t)(((v0) << 8) | (v1)))
#define ashift4(a, b, c, d) ((uint64_t)(((ashift((a), (b)) << 16) | (ashift((c), (d))))))
#define ashift8(a, b, c, d, e, f, g, h) ((uint64_t)(((ashift4((a), (b), (c), (d))) << 32) | \
						    ashift4((e), (f), (g), (h))))
#define IMkType(str) ((const uint64_t)(ashift8((str)[0], (str)[1], (str)[2], (str)[3], \
					       (str)[4], (str)[5], (str)[6], (str)[7])))

// example predefined object TYPES

#define OBJECT_TYPE_STRUCTURAL		IMkType("obj.STUC")
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP")
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG")
#define OBJECT_TYPE_WIDGET		IMkType("obj.WDGT")
#define OBJECT_TYPE_THREAD		IMkType("obj.THRD")
#define OBJECT_TYPE_DICTIONARY		IMkType("obj.DICT")
#define OBJECT_TYPE_CONTRACT		IMkType("obj.CONT")
#define OBJECT_TYPE_TRANSFORM		IMkType("obj.TRAN")

#define OBJECT_TYPE_UNDEFINED 0
#define OBJECT_TYPE_ANY 0

#define OBJECT_SUBTYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_ANY 0
#define OBJECT_SUBTYPE_NONE 0

#define NO_SUBTYPE 0

#define OBJ_INTENTION_NONE 0

// INTENTIONS
NIRVA_ENUM {
  // some common intentions
  // internal or (possibly) non-functional types
  OBJ_INTENTION_UNKNOWN,

  // application types
  OBJ_INTENTION_NOTHING = OBJ_INTENTION_NONE,

  // pasive intents
  // template intents - there are 3 optional passive intents for templates
  // if an instance wants to run one of these is should do so via a tamplete for the type

  // transform which creates an instance of specified subtype, with specified state
  // will trigger init_hook on the new instance
  // if it takes an instance of same type / subtype  state as an input atribute,
  // this becomes COPY_INSTANCE
  OBJ_INTENTION_CREATE_BUNDLE = 0x00000100, // create instance of type / subtype

  // there is a single passive intent for instances - actioning this
  // no negotiate, mandatory intent will trigger the corresponding request hook
  // in the target. This may be called either on the instance itself or on an attribute

  OBJ_INTENTION_REQUEST_UPDATE,

  // active intents - there is only 1 active intent, which can be further deliniated by
  // the hoks required / provided:
  // the CAPS and atributes futher delineate these

  // a transform which takes data input and produces data output
  OBJ_INTENTION_PROCESS,

  // the following are "synthetic" intents formed by a combination of factors

  // transform which changes the state of an instance (either self or another instance)
  // will trigger object state change hook- This is simply request_update called with target "STATE"
  OBJ_INTENTION_CHANGE_STATE,

  // transform which changes the subtype of an instance (either self or another instance)
  // will trigger object config_changed hook. This is simply request_update called with target "SUBTYPE.
  OBJ_INTENTION_CHANGE_SUBTYPE,

  // an intention which takes one or more sequential attributes and produces output
  // either as another sequential attribute
  // - PLAY is based on this, with CAP realtime
  //  (this is INTENTION_PROCESS with a a data out hook and need_data hook)
  OBJ_INTENTION_MANIPULATE_SEQUENCE,

  // an intent which takes sequential data and produces static array output
  //(this is INTENTION_PROCESS with a need_data hook, which
  // produces attribute(s) and / or objects as output)
  OBJ_INTENTION_RECORD,  // record

  // intent INTENTION_PROCESS takes static attribute (array)
  // and produces an object instance of a different type
  // (c.f create instance, used to create objects of the SAME type)
  //
  OBJ_INTENTION_RENDER,

  // an INTENT_PROCESS intention which takes one or more array attributes and produces altered array output
  OBJ_INTENTION_EDIT_DATA,

  // derived intentions

  // variety of manipulate_stream
  // an intent which has a data in hook, and data out hooks
  OBJ_INTENTION_PLAY = 0x00000200, // value fixed for all time, order of following must not change (see videoplugin.h)

  // like play, but with the REMOTE capacity (can also be an attachment to PLAY)
  OBJ_INTENTION_STREAM,  // encode / data in -> remote stream

  // alias for encode but with a weed_layer (frame ?) requirement
  // rather than a clip object (could also be an attachment to PLAY
  // with realtime == FALSE and display == FALSE caps)
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

  // an intent which converts an object's STATE from EXTERNAL to NORMAL
  // caps define LOCAL or REMOTE source
  OBJ_INTENTION_IMPORT = 0x00000C00,

  // an intent which takes an instance in state INTERNAL and creates an object in state EXTERNAL
  // with LOCAL - export to local filesystem, from internal clip to ext (raw) file format -> e.g. export audio, save frame
  // with REMOTE - export raw format to online location, e.g. export audio, save frame
  OBJ_INTENTION_EXPORT,

  // decoders
  // this is a specialized intent for clip objects, for READY objects, produces frame objects from the clip object)
  // media_src with realtime / non-realtime CAPS
  OBJ_INTENTION_DECODE = 0x00001000, // combine with caps to determine e.g. decode_audio, decode_video

  // use caps to further refine e.g REALTIME / NON_REALTIME (can be attachment to PLAY ?)
  // this is a transform of base type MANIPULATE_STREAM, which can be attached to a data_prepared hook of
  // a stream manipulation transform, altering the input stream
  OBJ_INTENTION_EFFECT = 0x00001400,

  // specialized effect intents
  OBJ_INTENTION_ANALYSE,
  OBJ_INTENTION_CONVERT,
  OBJ_INTENTION_MIX,
  OBJ_INTENTION_SPLIT,
  OBJ_INTENTION_DUPLICATE,

  // - this is now OBJ_INTENTION_REQUEST_UPDATE / dest == taraget_object.refcoucnt.refcount,
  // value == -1
  //#define  OBJ_INTENTION_DESTROY_INSTANCE OBJ_INTENTION_UNREF
#define  OBJ_INTENTION_DESTROY_INSTANCE 0x00002000

  OBJ_INTENTION_FIRST_CUSTOM = 0x80000000,
  OBJ_INTENTION_MAX = 0xFFFFFFFF
};

// aliases (depending on context, intentions can be used to signify a choice amongst various alternatives)
#define OBJ_INTENTION_IGNORE OBJ_INTENTION_NOTHING
#define OBJ_INTENTION_LEAVE OBJ_INTENTION_NOTHING
#define OBJ_INTENTION_SKIP OBJ_INTENTION_NOTHING

// in context with capacity "local" set
#define OBJ_INTENTION_MOVE OBJ_INTENTION_EXPORT

//#define OBJ_INTENTION_MOVE x(OBJ_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

// in context with capacity "remote" set
#define OBJ_INTENTION_UPLOAD OBJ_INTENTION_EXPORT

//#define OBJ_INTENTION_UPLOAD x(OBJ_INTENTION_EXPORT, CAP_AND(HAS_CAP(LOCAL), HAS_NOT_CAP(RENOTE)))

//#define OBJ_INTENTION_DOWNLOAD OBJ_INTENTION_IMPORT_REMOTE

#define OBJ_INTENTION_DELETE OBJ_INTENTION_DESTROY_INSTANCE

#define OBJ_INTENTION_UPDATE OBJ_INTENTION_REQUEST_UPDATE

#define OBJ_INTENTION_REPLACE OBJ_INTENTION_DELETE

// object flags
// object is static. It is always safe to referenc via a pointer to
// can apply to templates as well as instances
#define NIRVA_OBJ_FLAG_STATIC 			(1ull < 0)

// object instance should only be created once as it has some application wide globals
// this implies either it is a template which produces no instances
// or else it is an instance created by a template and a single instance may be instantiated
// at any one time
#define NIRVA_OBJ_FLAG_SINGLETON 		(1ull < 1)

// do not create an object template, instead instances should be created directly
// this is for trivial objects that dont need tracking by the broker
// in this case the object can built directly from an object_instance bundle
// there will be an attr bundle supplied in the template which should be created in each instance
#define NIRVA_OBJ_FLAG_INSTANCE_ONLY 	       	(1ull < 2)

// indicates an object instance created from a template object
#define NIRVA_OBJ_FLAG_INSTANCE 	       	(1ull < 3)

// indicates that a (native) thread should be assigned to run the instance
// communicatuon is via request hooks and status change hooks
#define TX_FLAG_ACTIVE 	     	  		(1ull < 4)

// indicates that the object has been destroyed, (DESTRUCTION_HOOK has been triggerd),
// but the 'shell' is left due to being ref counted

// see Developer Docs for more details

// thus when examining an unknown object / instance one should first
#define NIRVA_OBJ_FLAG_ZOMBIE 	       		(1ull < 16)

/// object STATES

// this state cannot be altered, but instances can be created in state NORMAL or NOT_READY
// via the CREATE_INSTNACE intent

#define OBJECT_STATE_UNDEFINED		0
// generic STATES which can be altered by *transforms*
//
// neutral state - if the instance has no "active" transforms then it will remain in this state
// such isntances support only object life cycle hooks, and data update hooks
#define OBJECT_STATE_READY		1
#define OBJECT_STATE_NORMAL		1
#define OBJECT_STATE_DORMANT		1

// some instantces may be born in this state and will need a transform to bring them to state normal
// preprared
#define OBJECT_STATE_NOT_READY		2

// some instances may need to be in a prepared state to run a transform
#define OBJECT_STATE_PREPARED		3

//// active states
// thread is "live", ie has a thread instance assigned to it, but is not current running any transform
#define OBJECT_STATE_ACTIVE_IDLE		4

// object is updating ints internal state
#define OBJECT_STATE_UPDATING		4

// some async transformss may cause the state to change to this temporarily
// an object may spontaneously change to this if doing internal updates / reads etc.
#define OBJECT_STATE_BUSY		5

//  IMPORT intent is a CHANGE_STATE intent which can alter or copy an object's state from external to not_ready / normal
// the EXPORT intent is a CHANGE_STATE intent which can create an object in this state
#define OBJECT_STATE_EXTERNAL		64

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

// composite icaps (TBD)
NIRVA_ENUM {
  _ICAP_IDLE = 0,
  _ICAP_DOWNLOAD,
  _ICAP_LOAD,
  N_STD_ICAPS,
};

////// standard ITEMS ////

/// flags and types start with an "_" so as not to conflict with strand names

// we define types and flags once as string "a", and again as char 'a'
// when constructing the strands, we make use of automatic string concatenation, and
// the string versions have to be used.
// in all other places, the type is a uint32_t, so we use (uint32_t)'a'
// ATTR_TYPE_* uses a different set of values. This is done deliberately to highlight the
// fact that strand types represent a single data value, whereas ATTR_TYPE is linked to the data
// type of the "VALUE", "DEFAULT" and "NEW_DEFAULT" strands inside the attribute bundle
// as well as to make the strand types easier to read

// a void strand - there is no built in default for this strand. It can be set to a value of
// any permitted type; once the value is set, the type is defined and fixed
#define _STRAND_TYPE_NONE		       		"0"

// standard types for the first letter of strand0
#define _STRAND_TYPE_INT		       		"i"
#define _STRAND_TYPE_DOUBLE				"d"
#define _STRAND_TYPE_BOOLEAN				"b"
#define _STRAND_TYPE_STRING				"s"
#define _STRAND_TYPE_INT64	       			"I"
#define _STRAND_TYPE_UINT		       		"u"
#define _STRAND_TYPE_UINT64				"U"

#define _STRAND_TYPE_VOIDPTR				"V"

#define _STRAND_TYPE_FUNCPTR				"F"

// internal (sub)bundle, should be unreffed / freed with the parent container
#define _STRAND_TYPE_BUNDLEPTR	       			"B"

// this is a bunlde pointer to external bundle, should NOT be unreffed / freed
// when the bundle containing this pointer is freed
#define _STRAND_TYPE_CONST_BUNDLEPTR	       		"C"

// array of bundleptr or far_pointer, with a unique key strand
// recommended to create as an array of individual bundle *,
// so each can be indexed externally by the unique key
#define _STRAND_TYPE_KEYED_ARRAY	       		"K"

// 0 or 1 of these flag values can be placed immediately before the type letter
#define _STRAND_TYPE_FLAG_COMMENT	       		"#"
#define _STRAND_TYPE_FLAG_DIRECTIVE	       		"@"
#define _STRAND_TYPE_FLAG_OPTIONAL	       		"?"

#define _STRAND_VALUE_SPECIAL_VARIABLE	       		"!"

////////////////////////////

///////// strand types ////////

#define STRAND_TYPE_NONE	       			(uint32_t)'0'	// invalid type

// flag bits
#define STRAND_TYPE_FLAG_OPTIONAL      	      		(uint32_t)'?'	// optional strand
#define STRAND_TYPE_FLAG_COMMENT       	      		(uint32_t)'#'	// comment
#define STRAND_TYPE_FLAG_DIRECTIVE			(uint32_t)'@'	// directive

#define STRAND_TYPE_INT					(uint32_t)'i'	// 4 byte int
#define STRAND_TYPE_DOUBLE				(uint32_t)'d'	// 8 byte float
#define STRAND_TYPE_BOOLEAN				(uint32_t)'b'	// 1 - 4 byte int
#define STRAND_TYPE_STRING				(uint32_t)'s'	// \0 terminated string
#define STRAND_TYPE_INT64              		     	(uint32_t)'I'	// 8 byte int
#define STRAND_TYPE_UINT	       			(uint32_t)'u'	// 4 byte int
#define STRAND_TYPE_UINT64				(uint32_t)'U'	// 8 byte int

#define STRAND_TYPE_VOIDPTR				(uint32_t)'V'	// void *
#define STRAND_TYPE_FUNCPTR				(uint32_t)'F'	// pointer to function

// void * aliases
#define STRAND_TYPE_BUNDLEPTR	       			(uint32_t)'B' // pointer to sub bundle
#define STRAND_TYPE_CONST_BUNDLEPTR           		(uint32_t)'C' // pointer to extern bundle

// KEYED values - this is a functional extension for bundleptr arrays.
// The objective is to facilitate lookups, using the "key" as a direct reference
// rather than having to go through each array item in turn looking for a match.
// the implementation can choose to implement its own method by setting automation
// or defining the IMPL_FUNC_find_item_by_key function
#define STRAND_TYPE_KEYED      				(uint32_t)'K'

#ifdef VERSION
#undef VERSION
#endif

#define STRAND_NAME(a, b) STRAND_NAMEU(a, b)

////////////// standard strands ///
#define MACROZ(what, va) MACRO(what __##va##__)
#define MACROX(what) MACROZ(what, VA_ARGS)

/* #define FOR_ALL_DOMAINS(MACRO, ...) MACRO(BASE __VA_ARGS__) MACRO(GENERIC __VA_ARGS__) MACRO(STRAND __VA_ARGS__) MACRO(VALUE __VA_ARGS__) MACRO(ATTRIBUTE __VA_ARGS__) MACRO(FUNCTION __VA_ARGS__) \ */
/*     MACRO(THREADS __VA_ARGS__) MACRO(INTROSPECTION __VA_ARGS__) MACRO(ICAP __VA_ARGS__) MACRO(OBJECT __VA_ARGS__) MACRO(HOOK __VA_ARGS__) MACRO(CONTRACT __VA_ARGS__) MACRO(TRANSFORM __VA_ARGS__) \ */
/*     MACRO(ATTRBUNDLE __VA_ARGS__) MACRO(CONDITION __VA_ARGS__) MACRO(LOGIC __VA_ARGS__) */

/* #define FOR_ALL_DOMAINS(MACROX, ...) MACROX(BASE) MACROX(GENERIC) MACROX(STRAND) MACROX(VALUE) MACROX(ATTRIBUTE) \ */
/*     MACROX(FUNCTION) MACROX(THREADS) MACROX(INTROSPECTION) MACROX(ICAP) MACROX(OBJECT) MACROX(HOOK) MACROX(CONTRACT) \ */
/*     MACROX(TRANSFORM) MACROX(ATTRBUNDLE) MACROX(CONDITION) MACROX(LOGIC) */

#define FOR_ALL_DOMAINS(MACROX, ...) MACROX(BASE), MACROX(GENERIC), MACROX(INTROSPECTION)
// domain BASE
#define STRAND_SPEC_VERSION				STRAND_NAME("SPEC", "VERSION")
#define STRAND_SPEC_VERSION_TYPE              		INT, 100

#define ALL_STRANDS_SPEC "VERSION"
#define ALL_BUNDLES_SPEC

#define STRAND_BASE_BUNDLE_TYPE				STRAND_NAME("BASE", "BUNDLE_TYPE")
#define STRAND_BASE_BUNDLE_TYPE_TYPE			INT, 0

#define ALL_STRANDS_BASE "BUNDLE_TYPE"
#define ALL_BUNDLES_BASE

// domain BLUEPRINT

#define STRAND_BLUEPRINT_TEXT				STRAND_NAME("BLUEPRINT", "TEXT")
#define STRAND_BLUEPRINT_TEXT_TYPE		       	STRING, NULL

#define STRAND_BLUEPRINT_BUNDLE_TYPE	       		STRAND_NAME("BLUEPRINT", "BUNDLE_TYPE")
#define STRAND_BLUEPRINT_BUNDLE_TYPE_TYPE      	       	UINT64, NULL

#define BUNDLE_BLUEPRINT_STRAND_DESC		      	STRAND_NAME("BLUEPRINT", "STRAND_DESC")
#define BUNDLE_BLUEPRINT_STRAND_DESC_TYPE      	       	STRAND_DESC, NULL

// domain SPECIAL
// indicates any strand can be added to / deleted from bundle
// without a  PRIV_CHECK
#define STRAND_SPECIAL_OPT_ANY			       	STRAND_NAME("SPECIAL", "*")
#define STRAND_SPECIAL_OPT_ANY_TYPE	       	       	STRING,"*"

// domain GENERIC

// const
#define STRAND_GENERIC_NAME				STRAND_NAME("GENERIC", "NAME")
#define STRAND_GENERIC_NAME_TYPE              		STRING, NULL

#define STRAND_GENERIC_FLAGS				STRAND_NAME("GENERIC", "FLAGS")
#define STRAND_GENERIC_FLAGS_TYPE              		UINT64, 0

// const
#define STRAND_GENERIC_UID				STRAND_NAME("GENERIC", "UID")
#define STRAND_GENERIC_UID_TYPE				UINT64, 0

#define STRAND_GENERIC_DESCRIPTION			STRAND_NAME("GENERIC", "DESCRIPTION")
#define STRAND_GENERIC_DESCRIPTION_TYPE              	STRING, NULL

#define ALL_STRANDS_GENERIC "UID", "DESCRIPTION", "FLAGS", "NAME"
#define ALL_BUNDLES_GENERIC

//// domain STRAND

#define STRAND_STRAND_HIERARCHY	              	       	STRAND_NAME("STRAND", "HIERARCHY")
#define STRAND_STRAND_HIERARCHY_TYPE	        	STRING, "STRAND_"

#define STRAND_STRAND_PTR_TO	              	       	STRAND_NAME("STRAND", "PTR_TO")
#define STRAND_STRAND_PTR_TO_TYPE	      		VOIDPTR, NULL

#define ALL_STRANDS_STRAND "PTR_TO", "HIERARCHY"
#define ALL_BUNDLES_STRAND_TYPE

//// domain STANDARD - some standard bundle types

#define BUNDLE_STANDARD_ATTRIBUTE	       		STRAND_NAME("STANDARD", "ATTRIBUTE")
#define BUNDLE_STANDARD_ATTRIBUTE_TYPE			ATTRIBUTE, NULL

#define BUNDLE_STANDARD_ATTR_CON			STRAND_NAME("STANDARD", "ATTR_CONTAINER")
#define BUNDLE_STANDARD_ATTR_CON_TYPE            	ATTR_CON, NULL

#define BUNDLE_STANDARD_ATTR_VALUE			STRAND_NAME("STANDARD", "ATTR_VALUE")
#define BUNDLE_STANDARD_ATTR_VALUE_TYPE            	ATTR_VALUE, NULL

#define BUNDLE_STANDARD_ERROR				STRAND_NAME("STANDARD", "ERROR")
#define BUNDLE_STANDARD_ERROR_TYPE       		EEROR_BUNDLE, NULL

#define BUNDLE_STANDARD_ATTR_DESC	       		STRAND_NAME("STANDARD", "ATTR_DESC")
#define BUNDLE_STANDARD_ATTR_DESC_TYPE			ATTR_DESC, NULL

#define BUNDLE_STANDARD_ATTR_DESC_CON            	STRAND_NAME("STANDARD", "ATTR_DESC_CONTAINER")
#define BUNDLE_STANDARD_ATTR_DESC_CON_TYPE      	ATTR_DESC_CON, NULL

#define BUNDLE_STANDARD_FUNC_DESC	       		STRAND_NAME("STANDARD", "FUNC_DESC")
#define BUNDLE_STANDARD_FUNC_DESC_TYPE			FUNC_DESC, NULL

#define BUNDLE_STANDARD_FUNC_MAP	       		STRAND_NAME("STANDARD", "FUNC_MAP")
#define BUNDLE_STANDARD_FUNC_MAP_TYPE			FUNC_MAP, NULL

#define BUNDLE_STANDARD_FUNC_DESC_CON            	STRAND_NAME("STANDARD", "FUNC_DESC_CONTAINER")
#define BUNDLE_STANDARD_FUNC_DESC_CON_TYPE      	FUNC_DESC_CON, NULL

#define BUNDLE_STANDARD_CONTRACT	       		STRAND_NAME("STANDARD", "CONTRACT")
#define BUNDLE_STANDARD_CONTRACT_TYPE			CONTRACT, NULL

#define BUNDLE_STANDARD_FUNCTIONAL	       		STRAND_NAME("STANDARD", "FUNCTIONAL")
#define BUNDLE_STANDARD_FUNCTIONAL_TYPE			FUNCTIONAL, NULL

#define BUNDLE_STANDARD_SELECTOR             		STRAND_NAME("STANDARD", "SELECTOR")
#define BUNDLE_STANDARD_SELECTOR_TYPE			SELECTOR, NULL

#define BUNDLE_STANDARD_LOCATOR             		STRAND_NAME("STANDARD", "LOCATOR")
#define BUNDLE_STANDARD_LOCATOR_TYPE			LOCATOR, NULL

#define BUNDLE_STANDARD_CASCADE             		STRAND_NAME("STANDARD", "CASCADE")
#define BUNDLE_STANDARD_CASCADE_TYPE			CASCADE, NULL

#define BUNDLE_STANDARD_REQUEST             		STRAND_NAME("STANDARD", "REQUEST")
#define BUNDLE_STANDARD_REQUEST_TYPE			REQUEST, NULL

#define BUNDLE_STANDARD_KEY_LOOKUP             		STRAND_NAME("STANDARD", "KEY_LOOKUP")
#define BUNDLE_STANDARD_KEY_LOOKUP_TYPE			KEY_LOOKUP, NULL

#define BUNDLE_STANDARD_MATRIX_2D             		STRAND_NAME("STANDARD", "MATRIX_2D")
#define BUNDLE_STANDARD_MATRIX_2D_TYPE			MATRIX_2D, NULL

/// domain VALUE - values are common to both attributes and strands

#define STRAND_VALUE_DATA	               	       	STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_DATA_TYPE	      			NONE,

// const if !none
#define STRAND_VALUE_TYPE	      			STRAND_NAME("VALUE", "VALUE_TYPE")
#define STRAND_VALUE_TYPE_TYPE	   	   		INT, ATTR_TYPE_NONE

#define STRAND_VALUE_DEFAULT	      			STRAND_NAME("VALUE", "DEFAULT")
#define STRAND_VALUE_DEFAULT_TYPE      			NONE,

#define STRAND_VALUE_NEW_DEFAULT       			STRAND_NAME("VALUE", "NEW_DEFAULT")
#define STRAND_VALUE_NEW_DEFAULT_TYPE     	       	NONE,

/////////////

#define STRAND_VALUE_MAX_VALUES				STRAND_NAME("VALUE", "MAX_VALUES")
#define STRAND_VALUE_MAX_VALUES_TYPE     	       	INT, -1

#define STRAND_VALUE_INTEGER				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INTEGER_TYPE	       	       	INT, 0

#define STRAND_VALUE_INT			       	STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INT_TYPE			       	INT, 0

#define STRAND_VALUE_UINT		       		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_UINT_TYPE			       	UINT, 0

#define STRAND_VALUE_DOUBLE				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_DOUBLE_TYPE	      	       	DOUBLE, 0.

#define STRAND_VALUE_BOOLEAN	      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_BOOLEAN_TYPE   	  	       	BOOLEAN, FALSE

#define STRAND_VALUE_STRING	      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_STRING_TYPE   	  	       	STRING, NULL

#define STRAND_VALUE_INT64		       		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INT64_TYPE			       	INT64, 0

#define STRAND_VALUE_UINT64		       		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_UINT64_TYPE	       	       	UINT64, 0

#define STRAND_VALUE_VOIDPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_VOIDPTR_TYPE 	    	       	VOIDPTR, NULL

#define STRAND_VALUE_FUNCPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_FUNCPTR_TYPE 	    	       	FUNCPTR, NULL

#define STRAND_VALUE_BUNDLEPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_BUNDLEPTR_TYPE 	    	       	BUNDLEPTR, NULL

#define STRAND_VALUE_CONST_BUNDLEPTR   	   	       	STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_CONST_BUNDLEPTR_TYPE     	       	CONST_BUNDLEPTR, NULL

#define STRAND_VALUE_KEYED_ARRAY   	   	       	STRAND_NAME("VALUE", "KEYED_ARRAY")
#define STRAND_VALUE_KEYED_ARRAY_TYPE        	       	KEYED_ARRAY, NULL

#define BUNDLE_VALUE_MAPPED	              	       	STRAND_NAME("VALUE", "MAPPPED")
#define BUNDLE_VALUE_MAPPED_TYPE              		MAPPED_VALUE, NULL

#define BUNDLE_VALUE_CHANGE	              	       	STRAND_NAME("VALUE", "CHANGE")
#define BUNDLE_VALUE_CHANGE_TYPE              		VALUE_CHANGE, NULL

#define STRAND_VALUE_ANY_BUNDLE	              	       	STRAND_NAME("VALUE", "ANY_BUNDLE")
#define STRAND_VALUE_ANY_BUNDLE_TYPE          		BUNDLEPTR, NULL

#define STRAND_VALUE_USER_DATA      			STRAND_NAME("VALUE", "USER_DATA")
#define STRAND_VALUE_USER_DATA_TYPE 	    	       	VOIDPTR, NULL

#define BUNDLE_VALUE_OLD      				STRAND_NAME("VALUE", "OLD")
#define BUNDLE_VALUE_OLD_TYPE 	    		       	VALUE, NULL

#define BUNDLE_VALUE_NEW      				STRAND_NAME("VALUE", "NEW")
#define BUNDLE_VALUE_NEW_TYPE 	    		       	VALUE, NULL

// domain AUTOMATION

#define BUNDLE_AUTOMATION_CONDITION  	             	STRAND_NAME("AUTOMATION", "CONDITION")
#define BUNDLE_AUTOMATION_CONDITION_TYPE      		SCRIPTLET, NULL

#define BUNDLE_AUTOMATION_SCRIPT                   	STRAND_NAME("AUTOMATION", "SCRIPT")
#define BUNDLE_AUTOMATION_SCRIPT_TYPE      		SCRIPTLET, NULL

// domain SRIPTLET

#define STRAND_SCRIPTLET_CATEGORY	      		STRAND_NAME("SCRIPTLET", "CATEGORY")
#define STRAND_SCRIPTLET_CATEGORY_TYPE	      		INT, 0

#define STRAND_SCRIPTLET_STRINGS	      		STRAND_NAME("SCRIPTLET", "STRINGS")
#define STRAND_SCRIPTLET_STRINGS_TYPE	      		STRING, NULL

// domain LOCATOR

#define STRAND_LOCATOR_UNIT    		               	STRAND_NAME("LOCATOR", "UNIT")
#define STRAND_LOCATOR_UNIT_TYPE      			STRING, NULL

#define STRAND_LOCATOR_SUB_UNIT    	               	STRAND_NAME("LOCATOR", "SUB_UNIT")
#define STRAND_LOCATOR_SUB_UNIT_TYPE           		STRING, NULL

#define STRAND_LOCATOR_INDEX           	               	STRAND_NAME("LOCATOR", "INDEX")
#define STRAND_LOCATOR_INDEX_TYPE      			INT64, NULL

#define STRAND_LOCATOR_SUB_INDEX    	               	STRAND_NAME("LOCATOR", "SUB_INDEX")
#define STRAND_LOCATOR_SUB_INDEX_TYPE          		INT64, NULL

// domain datetime

#define STRAND_DATETIME_TIMESTAMP                   	STRAND_NAME("DATETIME", "TIMESTAMP")
#define STRAND_DATETIME_TIMESTAMP_TYPE      		INT64, NULL

#define STRAND_DATETIME_DELTA 	                  	STRAND_NAME("DATETIME", "DELTA")
#define STRAND_DATETIME_DELTA_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_START  	                 	STRAND_NAME("DATETIME", "START")
#define STRAND_DATETIME_START_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_END  	                 	STRAND_NAME("DATETIME", "END")
#define STRAND_DATETIME_END_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_CURENT                        	STRAND_NAME("DATETIME", "CURRENT")
#define STRAND_DATETIME_CURRENT_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_FIRST                        	STRAND_NAME("DATETIME", "FIRST")
#define STRAND_DATETIME_FIRST_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_LAST                        	STRAND_NAME("DATETIME", "LAST")
#define STRAND_DATETIME_LAST_TYPE 	     		INT64, NULL

#define STRAND_DATETIME_DEADLINE                      	STRAND_NAME("DATETIME", "DEADLINE")
#define STRAND_DATETIME_DEADLINE_TYPE 	     		INT64, NULL

// domain CASCADE

#define BUNDLE_CASCADE_DECISION_NODE			STRAND_NAME("CASCADE", "DECISION_NODE")
#define BUNDLE_CASCADE_DECISION_NODE_TYPE      		CONDVAL_NODE, NULL

#define STRAND_CASCADE_VALUE				STRAND_NAME("CASCADE", "VALUE")
#define STRAND_CASCADE_VALUE_TYPE    	      		BUNDLEPTR, NULL

#define BUNDLE_CASCADE_CONSTVAL_MAP			STRAND_NAME("CASCADE", "CONSTVAL_MAP")
#define BUNDLE_CASCADE_CONSTVAL_MAP_TYPE      		CONSTVAL_MAP, NULL

#define BUNDLE_CASCADE_MATRIX_NODE			STRAND_NAME("CASCADE", "MATRIX_NODE")
#define BUNDLE_CASCADE_MATRIX_NODE_TYPE	      		CASCMATRIX_NODE, NULL

#define BUNDLE_CASCADE_MATRIX				STRAND_NAME("CASCADE", "MATRIX")
#define BUNDLE_CASCADE_MATRIX_TYPE	      		MATRIX_2D, NULL

#define BUNDLE_CASCADE_CONDLOGIC			STRAND_NAME("CASCADE", "CONDLOGIC")
#define BUNDLE_CASCADE_CONDLOGIC_TYPE			CONDLOGIC, NULL

// domain CASCMATRIX

#define BUNDLE_CASCMATRIX_ON_SUCCESS			STRAND_NAME("CASCMATRIX", "ON_SCUCCESS")
#define BUNDLE_CASCMATRIX_ON_SUCCESS_TYPE		CASCMATRIX_NDOE, NULL

#define BUNDLE_CASCMATRIX_ON_FAIL			STRAND_NAME("CASCMATRIX", "ON_FAIL")
#define BUNDLE_CASCMATRIX_ON_FAIL_TYPE			CASCMATRIX_NDOE, NULL

#define STRAND_CASCMATRIX_OP_SUCCESS			STRAND_NAME("CASCMATRIX", "OP_SUCCESS")
#define STRAND_CASCMATRIX_OP_SUCCESS_TYPE      		INT, CASC_MATRIX_NOOP

#define STRAND_CASCMATRIX_OP_FAIL	       		STRAND_NAME("CASCMATRIX", "OP_FAIL")
#define STRAND_CASCMATRIX_OP_FAIL_TYPE			INT, CASC_MATRIX_NOOP

#define STRAND_CASCMATRIX_P_SUCESS			STRAND_NAME("CASCMATRIX", "P_SUCCESS")
#define STRAND_CASCMATRIX_P_SUCESS_TYPE       		DOUBLE, 0.

#define STRAND_CASCMATRIX_P_FAIL			STRAND_NAME("CASCMATRIX", "P_FAIL")
#define STRAND_CASCMATRIX_P_FAIL_TYPE       		DOUBLE, 0.

#define STRAND_CASCMATRIX_WEIGHT			STRAND_NAME("CASCMATRIX", "WEIGHT")
#define STRAND_CASCMATRIX_WEIGHT_TYPE       		DOUBLE, 0.

#define STRAND_CASCMATRIX_NRUNS				STRAND_NAME("CASCMATRIX", "NRUNS")
#define STRAND_CASCMATRIX_NRUNS_TYPE       		INT, 0.

#define STRAND_CASCMATRIX_OTHER_IDX			STRAND_NAME("CASCMATRIX", "OTHER_IDX")
#define STRAND_CASCMATRIX_OTHER_IDX_TYPE       		INT, -1

// domain MATRIX

#define BUNDLE_MATRIX_ROW				STRAND_NAME("MATRIX", "ROW")
#define BUNDLE_MATRIX_ROW_TYPE				MATRIX_ROW, NULL

// domain CONDVAL
#define STRAND_CONDVAL_CURRENT				STRAND_NAME("CONDVAL", "CURRENT")
#define STRAND_CONDVAL_CURRENT_TYPE	       		CONDMAP_NODE, NULL

// domain LOGIC
#define STRAND_LOGIC_OP					STRAND_NAME("LOCIC", "OP")
#define STRAND_LOGIC_OP_TYPE				INT, LOGIC_LAST

#define STRAND_LOGIC_NOT	       			STRAND_NAME("LOCIC", "NOT")
#define STRAND_LOGIC_NOT_TYPE			       	INT, LOGICAL_NOT

#define STRAND_LOGIC_AND		       		STRAND_NAME("LOCIC", "AND")
#define STRAND_LOGIC_AND_TYPE			       	INT, LOGICAL_AND

#define STRAND_LOGIC_OR					STRAND_NAME("LOCIC", "OR")
#define STRAND_LOGIC_OR_TYPE		    	   	INT, LOGICAL_OR

#define STRAND_LOGIC_XOR		       		STRAND_NAME("LOCIC", "XOR")
#define STRAND_LOGIC_XOR_TYPE		    	   	INT, LOGICAL_XOR

/// domain ATTRIBUTE

#define STRAND_ATTRIBUTE_VALUE_TYPE	       		STRAND_NAME("ATTRIBUTE", "VALUE_TYPE")
#define STRAND_ATTRIBUTE_VALUE_TYPE_TYPE  		UINT, ATTR_TYPE_NONE

#define BUNDLE_ATTRIBUTE_CONNECTION	       		STRAND_NAME("ATTRIBUTE", "CONNECTION")
#define BUNDLE_ATTRIBUTE_CONNECTION_TYPE  		ATTR_CONNECTION, NULL

#define BUNDLE_ATTRIBUTE_HOOK_STACK	       		STRAND_NAME("ATTRIBUTE", "HOOK_STACK")
#define BUNDLE_ATTRIBUTE_HOOK_STACK_TYPE  		HOOK_STACK, NULL

// emissions are from source in caller object, to dest in target object
#define BUNDLE_EMISSION_CALLER		       		STRAND_NAME("EMISSION", "CALLER")
#define BUNDLE_EMISSION_CALLER_TYPE			OBJECT, NULL

#define BUNDLE_EMISSION_SOURCE		       		STRAND_NAME("EMISSION", "SOURCE")
#define BUNDLE_EMISSION_SOURCE_TYPE			SELECTOR, NULL

#define BUNDLE_EMISSION_TARGET		       		STRAND_NAME("EMISSION", "TARGET")
#define BUNDLE_EMISSION_TARGET_TYPE			OBJECT, NULL

#define BUNDLE_EMISSION_DEST		       		STRAND_NAME("EMISSION", "DEST")
#define BUNDLE_EMISSION_DEST_TYPE			SELECTOR, NULL

// domain FUNCTION

#define STRAND_FUNCTION_CATEGORY	      		STRAND_NAME("FUNCTION", "CATEGORY")
#define STRAND_FUNCTION_CATEGORY_TYPE	      		INT, 0

#define BUNDLE_FUNCTION_INPUT		       		STRAND_NAME("FUNCTION", "INPUT_BUNDLE")
#define BUNDLE_FUNCTION_INPUT_TYPE			FN_INPUT, NULL

#define BUNDLE_FUNCTION_OUTPUT		       		STRAND_NAME("FUNCTION", "OUTPUT_BUNDLE")
#define BUNDLE_FUNCTION_OUTPUT_TYPE			FN_OUTPUT, NULL

#define BUNDLE_FUNCTION_SCRIPT		       		STRAND_NAME("FUNCTION", "SCRIPT")
#define BUNDLE_FUNCTION_SCRIPT_TYPE			SCRIPTLET, NULL

#define STRAND_FUNCTION_OBJ_FUNCTION	       	     	STRAND_NAME("FUNCTION", "OBJ_FUNCTION")
#define STRAND_FUNCTION_OBJ_FUNCTION_TYPE      	     	FUNCPTR, NULL

#define STRAND_FUNCTION_NATIVE_FUNCTION		     	STRAND_NAME("FUNCTION", "NATIVE_FUNCTION")
#define STRAND_FUNCTION_NATIVE_FUNCTION_TYPE           	FUNCPTR, NULL

#define STRAND_FUNCTION_PARAM_NUM	      		STRAND_NAME("FUNCTION", "PARAM_NUMBER")
#define STRAND_FUNCTION_PARAM_NUM_TYPE	      		INT, 0

#define STRAND_FUNCTION_RESPONSE	       		STRAND_NAME("FUNCTION", "RESPONSE")
#define STRAND_FUNCTION_RESPONSE_TYPE         		INT64, 0

#define BUNDLE_FUNCTION_MAPPING				STRAND_NAME("FUNCTION", "MAPPING")
#define BUNDLE_FUNCTION_MAPPING_TYPE			PMAP_DESC, NULL

#define BUNDLE_FUNCTION_REV_PMAP	       		STRAND_NAME("FUNCTION", "REV_PMAP")
#define BUNDLE_FUNCTION_REV_PMAP_TYPE			PMAP, NULL

#define BUNDLE_FUNCTION_PMAP_IN				STRAND_NAME("FUNCTION", "PMAP_IN")
#define BUNDLE_FUNCTION_PMAP_IN_TYPE			PMAP, NULL

#define BUNDLE_FUNCTION_PMAP_OUT	       		STRAND_NAME("FUNCTION", "PMAP_OUT")
#define BUNDLE_FUNCTION_PMAP_OUT_TYPE			PMAP, NULL

// domain THREADS
#define STRAND_THREADS_NATIVE_THREAD		       	STRAND_NAME("THREADS", "NATIVE_THREAD")
#define STRAND_THREADS_NATIVE_THREAD_TYPE     		VOIDPTR, NULL

#define STRAND_THREADS_NATIVE_STATE		       	STRAND_NAME("THREADS", "NATIVE_STATE")
#define STRAND_THREADS_NATIVE_STATE_TYPE       		UINT64, NULL

#define STRAND_THREADS_FLAGS			       	STRAND_NAME("THREADS", "FLAGS")
#define STRAND_THREADS_FLAGS_TYPE 	       		UINT64, NULL

#define STRAND_THREADS_MUTEX				STRAND_NAME("THREADS", "MUTEX")
#define STRAND_THREADS_MUTEX_TYPE	       		VOIDPTR, NULL

#define BUNDLE_THREAD_INSTANCE				STRAND_NAME("THREAD", "INSTANCE")
#define BUNDLE_THREAD_INSTANCE_TYPE    			THREAD_INSTANCE, NULL

// domain INTROSPECTION
// item which should be added to every bundldef, it is designed to allow
// the bundle creator to store the pared down strands used to construct the bundle
#define BUNDLE_INTROSPECTION_BLUEPRINT    		STRAND_NAME("INTROSPECTION", "BLUEPRINT")
#define BUNDLE_INTROSPECTION_BLUEPRINT_TYPE            	BLUEPRINT, NULL

// as an alternative this can be used instead to point to a static copy of the strands
#define STRAND_INTROSPECTION_BLUEPRINT_PTR    	       	STRAND_NAME("INTROSPECTION", "BLUEPRINT_PTR")
#define STRAND_INTROSPECTION_BLUEPRINT_PTR_TYPE        	VOIDPTR, NULL

#define BUNDLE_INTROSPECTION_REFCOUNT       	       	STRAND_NAME("INTROSPECTION", "REFCOUNT")
#define BUNDLE_INTROSPECTION_REFCOUNT_TYPE     		REFCOUNT, NULL

#define STRAND_INTROSPECTION_COMMENT    	       	STRAND_NAME("INTROSPECTION", "COMMENT")
#define STRAND_INTROSPECTION_COMMENT_TYPE      	       	STRING, NULL

#define STRAND_INTROSPECTION_PRIVATE_DATA	       	STRAND_NAME("INTROSPECTION", "PRIVATE_DATA")
#define STRAND_INTROSPECTION_PRIVATE_DATA_TYPE		VOIDPTR, NULL

#define STRAND_INTROSPECTION_NATIVE_TYPE 	       	STRAND_NAME("INTROSPECTION", "NATIVE_TYPE")
#define STRAND_INTROSPECTION_NATIVE_TYPE_TYPE 	       	INT, 0

#define STRAND_INTROSPECTION_NATIVE_SIZE      	       	STRAND_NAME("INTROSPECTION", "NATIVE_SIZE")
#define STRAND_INTROSPECTION_NATIVE_SIZE_TYPE		UINT64, 0

#define STRAND_INTROSPECTION_NATIVE_PTR        		STRAND_NAME("INTROSPECTION", "NATIVE_PTR")
#define STRAND_INTROSPECTION_NATIVE_PTR_TYPE		VOIDPTR, NULL

#define ALL_STRANDS_INTROSPECTION "BLUEPRINT", "BLUEPRINT_PTR", "COMMENT", "REFCOUNT", "PRIVATE_DATA", \
    "NATIVE_PTR", "NATIVE_SIZE", "NATIVE_TYPE"
#define ALL_BUNDLES_INTROSPECTION

//////////// BUNDLE SPECIFIC STRANDS /////

///// domain ICAP

#define STRAND_ICAP_INTENTION				STRAND_NAME("ICAP", "INTENTION")
#define STRAND_ICAP_INTENTION_TYPE             		INT, OBJ_INTENTION_NONE

#define STRAND_ICAP_CAPACITY				STRAND_NAME("ICAP", "CAPACITY")
#define STRAND_ICAP_CAPACITY_TYPE              		STRING, NULL

////////// domain OBJECT

#define STRAND_OBJECT_TYPE				STRAND_NAME("OBJECT", "TYPE")
#define STRAND_OBJECT_TYPE_TYPE				UINT64, OBJECT_TYPE_UNDEFINED

#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS	       		STRAND_NAME("OBJECT", "ACTIVE_TRANSFORMS")
#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS_TYPE	       	TRANSFORM, NULL

#define BUNDLE_OBJECT_ATTRIBUTES	       		STRAND_NAME("OBJECT", "ATTRIBUTES")
#define BUNDLE_OBJECT_ATTRIBUTES_TYPE	  	     	ATTR_CON, NULL

#define STRAND_OBJECT_SUBTYPE				STRAND_NAME("OBJECT", "SUBTYPE")
#define STRAND_OBJECT_SUBTYPE_TYPE	       		UINT64, NO_SUBTYPE

#define STRAND_OBJECT_STATE				STRAND_NAME("OBJECT", "STATE")
#define STRAND_OBJECT_STATE_TYPE	       		UINT64, OBJ_STATE_TEMPLATE

#define BUNDLE_OBJECT_CONTRACTS	 	      		STRAND_NAME("OBJECT", "CONTRACT")
#define BUNDLE_OBJECT_CONTRACTS_TYPE			CONTRACT, NULL

///// domain HOOK

// this is one of three basic patterns
#define STRAND_HOOK_TYPE		       		STRAND_NAME("HOOK", "TYPE")
#define STRAND_HOOK_TYPE_TYPE				INT, 0

// after the template callback modified by flags is called
// the value is cascaded by automation, resulting in a differentiatied value
// the second value is then triggered either during or after the first
// custom hooks can be added by adding new cascade conditions mapping to a new value
// depending on the basic pattern the new hook number must be attached to a
// specific strand or attribute value (for data hook) or else must map to an update value
// intent (for request hooks), or for spntaneous hooks, this must map to a structure transform
// in include in an existing or custom structure subtype
#define STRAND_HOOK_NUMBER	       			STRAND_NAME("HOOK", "NUMBER")
#define STRAND_HOOK_NUMBER_TYPE				INT, 0

#define STRAND_HOOK_HANDLE				STRAND_NAME("HOOK", "HANDLE")
#define STRAND_HOOK_HANDLE_TYPE				UINT, 0

// the name of the item which triggers the hook
#define STRAND_HOOK_TARGET				STRAND_NAME("HOOK", "TARGET")
#define STRAND_HOOK_TARGET_TYPE				STRING, NULL

#define STRAND_HOOK_FLAGS		       		STRAND_NAME("HOOK", "FLAGS")
#define STRAND_HOOK_FLAGS_TYPE				UINT64, 0

#define STRAND_HOOK_CB_DATA		       		STRAND_NAME("HOOK", "CB_DATA")
#define STRAND_HOOK_CB_DATA_TYPE       			VOIDPTR, NULL

#define STRAND_HOOK_COND_STATE				STRAND_NAME("HOOK", "COND_STATE")
#define STRAND_HOOK_COND_STATE_TYPE	       		BOOLEAN, TRUE

#define BUNDLE_HOOK_CALLBACK		       		STRAND_NAME("HOOK", "CALLBACK")
#define BUNDLE_HOOK_CALLBACK_TYPE			HOOK_CB_FUNC, NULL

#define BUNDLE_HOOK_DETAILS		       		STRAND_NAME("HOOK", "DETAILS")
#define BUNDLE_HOOK_DETAILS_TYPE			HOOK_DETAILS, NULL

#define BUNDLE_HOOK_STACK		       		STRAND_NAME("HOOK", "STACK")
#define BUNDLE_HOOK_STACK_TYPE				HOOK_STACK, NULL

#define BUNDLE_HOOK_TRIGGER		       		STRAND_NAME("HOOK", "TRIGGER")
#define BUNDLE_HOOK_TRIGGER_TYPE       			HOOK_TRIGGER, NULL

#define BUNDLE_HOOK_CB_ARRAY		       		STRAND_NAME("HOOK", "CB_STACK")
#define BUNDLE_HOOK_CB_ARRAY_TYPE      			COND_ARRAY, NULL

// domain CONTRACT

// this is to enable mapping static transforms to native function calls
#define BUNDLE_CONTRACT_FUNC_WRAPPER			STRAND_NAME("CONTRACT", "FUNC_WRAPPER")
#define BUNDLE_CONTRACT_FUNC_WRAPPER_TYPE	       	FUNC_DESC, NULL

#define BUNDLE_CONTRACT_REQUIREMENTS	       		STRAND_NAME("CONTRACT", "REQUIREMENTS")
#define BUNDLE_CONTRACT_REQUIREMENTS_TYPE      		CONDITION, NULL

// domain TRANSFORM

#define STRAND_TRANSFORM_STATUS 	       		STRAND_NAME("TRANSFORM", "STATUS")
#define STRAND_TRANSFORM_STATUS_TYPE			INT, TRANSFORM_STATUS_NONE

#define STRAND_TRANSFORM_RESULT 	       		STRAND_NAME("TRANSFORM", "RESULT")
#define STRAND_TRANSFORM_RESULT_TYPE			INT, TX_RESULT_NONE

#define BUNDLE_TRANSFORM_ICAP 				STRAND_NAME("TRANSFORM", "ICAP")
#define BUNDLE_TRANSFORM_ICAP_TYPE			ICAP, NULL

// domain TRAJECTORY

#define BUNDLE_TRAJECTORY_NEXT_SEGMENT	       		STRAND_NAME("NEXT", "SEGMENT")
#define BUNDLE_TRAJECTORY_NEXT_SEGMENT_TYPE	     	TSEGMENT, NULL

#define BUNDLE_TRAJECTORY_SEGMENT	       		STRAND_NAME("TRAJECTORY", "SEGMENT")
#define BUNDLE_TRAJECTORY_SEGMENT_TYPE	       		TSEGMENT, NULL

#define BUNDLE_TRAJECTORY_FUNCTIONAL	       		STRAND_NAME("TRAJECTORY", "FUNCTIONAL")
#define BUNDLE_TRAJECTORY_FUNCTIONAL_TYPE	       	FUNCTIONAL, NULL

/// domain ATTR_CON

#define STRAND_ATTRCON_MAX_REPEATS 		     	STRAND_NAME("ATTRCON", "MAX_REPEATS")
#define STRAND_ATTRCON_MAX_REPEATS_TYPE     		INT, 1

#define BUNDLE_ATTRCON_HOOK_STACK	               	STRAND_NAME("ATTR", "HOOK_STACK")
#define BUNDLE_ATTRCON_HOOK_STACK_TYPE       		HOOK_STACK, NULL

//// domain CONDITION - condiitons are simple functions which produce a TRUE / FALSE result

#define STRAND_CONDITION_RESPONSE	       		STRAND_NAME("CONDITION", "RESPONSE")
#define STRAND_CONDITION_RESPONSE_TYPE	       		UINT64, NIRVA_COND_SUCCESS

#define STRAND_CONDITION_CURRENT	       		STRAND_NAME("CONDITION", "CURRENT")
#define STRAND_CONDITION_CURRENT_TYPE	       		CONDVAL_MAP, NULL

/* #define BUNDLE_CONDITION_MAX_ITEMS	       		STRAND_NAME("CONDITION", "MAX_ITEMS") */
/* #define BUNDLE_CONDITION_MAX_ITEMS_TYPE	       		FUNCTION, MAX_ITEMS */

/* #define BUNDLE_CONDITION_HAS_VALUE	       		STRAND_NAME("CONDITION", "HAS_VALUE") */
/* #define BUNDLE_CONDITION_HAS_VALUE_TYPE   		FUNCTION, VALUE_SET */

/* #define BUNDLE_CONDITION_HAS_ITEM	       		STRAND_NAME("CONDITION", "HAS_ITEM") */
/* #define BUNDLE_CONDITION_HAS_ITEM_TYPE   		FUNCTION, HAS_ITEM */

/* #define BUNDLE_CONDITION_VALUES_EQUAL	       		STRAND_NAME("CONDITION", "VALUE_MATCH") */
/* #define BUNDLE_CONDITION_VALUE_MATCH_TYPE   		FUNCTION, VALUE_MATCH */

// domain STRUCTURAL

////////////////////////////////////////////////////////////////////////////////////

// whereas STRANDS generally pertain to the internal state of a bundle, ATTRIBUTES
// (more precisely, variants of the ATTRIBUTE bundle) are desinged for passing and sharing data between
// bundles. However since both strands can be wrapped by a strand bundle, which contains a VALUE bundle,
// and ATTRIBUTES are based around a VALUE bundle, it is possible to treat them similarly in some aspects
// by wrapping the value bundles in a vale_map bundle. Thes bundles also have a HIEARARCHY strand
// which will be either HIERARCHY_STRAND or HIERARCHY_ATTRIBUTE accordingly.

// The main differences:
//- STRANDS have type, name and data, although these are internal to the strand
// - AATTRIBUTES have strands for DATA, NAME and TYPE
// - ATTRIBUTES are bundles composed of several strands, whereas strands are not bundles - they are the building blocks
// for bundles.
// - Strands cna be scalar values or arrays of unlimited size. Attributes may have "repeats", ie. the value can be
// limited to a certain number of data values (i.e. bounded arrays).
// Although strands have a default value when first created, this is generally 0 or NULL, and is not visible
// after the strand has been created. Attributes have a default value which must be set in the "default" strand.
// - Attributes have flags and an optional description. Attributes can also be refcounted.
//

#define ATTR_TYPE_NONE					(uint32_t)'0'	// invalid type

#define ATTR_TYPE_INT					1	// 4 byte int
#define ATTR_TYPE_DOUBLE				2	// 8 byte float
#define ATTR_TYPE_BOOLEAN				3	// 1 - 4 byte int
#define ATTR_TYPE_STRING				4	// \0 terminated string
#define ATTR_TYPE_INT64	       			     	5	// 8 byte int
#define ATTR_TYPE_UINT					6	// 4 byte int
#define ATTR_TYPE_UINT64				7	// 8 byte int
#define ATTR_TYPE_FLOAT	       				8	// 4 or 8 byte float

#define ATTR_TYPE_VOIDPTR				64	// void *
#define ATTR_TYPE_FUNCPTR				65	// funcptr

// void * alias
#define ATTR_TYPE_BUNDLEPTR	       			80 // void * to other bundle

// used for variadic function  maps
// the "VALUE" will be a bundleptr -> sup mapping
#define ATTR_TYPE_VA_ARGS	       			128 // void * to other bundle

/////////

#define ATTR_NAMEU(a, b) "ATTR_" a "_" b
#define ATTR_NAME(a, b) ATTR_NAMEU(a, b)

//////////////////////////

// domain STRUCTURAL
#define ATTR_STRUCTURAL_SUBTYPES			ATTR_NAME("STRUCTURAL", "SUBTYPES")
#define ATTR_STRUCTURAL_SUBTYPES_TYPE			BUNDLEPTR, OBJECT_INSTANCE

// domain SELF - atributes for thread_instances
#define ATTR_SELF_THREAD_ID				ATTR_NAME("SELF", "THREAD_ID")
#define ATTR_SELF_THREAD_ID_TYPE	       	       	UINT64, 0

#define ATTR_SELF_STATUS				ATTR_NAME("SELF", "STATUS")
#define ATTR_SELF_STATUS_TYPE		       	       	UINT64, 0

#define ATTR_SELF_FLAGS					ATTR_NAME("SELF", "FLAGS")
#define ATTR_SELF_FLAGS_TYPE	 	      	       	UINT64, 0

#define ATTR_SELF_NATIVE_THREAD				ATTR_NAME("SELF", "NATIVE_THREAD")
#define ATTR_SELF_NATIVE_THREAD_TYPE		       	NATIVE_PTR, NULL

#define ATTR_SELF_PRIVS					ATTR_NAME("SELF", "PRIVS")
#define ATTR_SELF_PRIVS_TYPE		       		INT, NULL

#define ATTR_SELF_MANTLE	       			ATTR_NAME("SELF", "MANTLE")
#define ATTR_SELF_MANTLE_TYPE		       		BUNDLEPTR, OBJECT

#define ATTR_SELF_TRAJECTORY	       			ATTR_NAME("SELF", "TRAJECTORY")
#define ATTR_SELF_TRAJECTORY_TYPE		       	BUNDLEPTR, TRAJECTORY

#define ATTR_SELF_TSEGMENT	       			ATTR_NAME("SELF", "TSEGMENT")
#define ATTR_SELF_TSEGMENT_TYPE			       	BUNDLEPTR, TSEGMENT

///

// TODO - attributes should moeve to type specific headers

/// domain URI
#define ATTR_URI_FILENAME				ATTR_NAME("URI", "FILENAME")
#define ATTR_URI_FILENAME_TYPE 				STRING, NULL

///// domain UI
#define ATTR_UI_TEMPLATE 				ATTR_NAME("UI", "TEMPLATE")
#define ATTR_UI_TEMPLATE_TYPE 				STRING, NULL

// audio (TODO)
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
#define ATTR_VIDEO_FRAME_RATE	       			STRAND_NAME(VIDEO, FRAME_RATE)
#define ATTR_VIDEO_FRAME_RATE_TYPE    			DOUBLE, 0.

#define ATTR_VIDEO_DISPLAY_WIDTH  	       		STRAND_NAME(VIDEO, DISPLAY_WIDTH)
#define ATTR_VIDEO_DISPLAY_WIDTH_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_DISPLAY_HEIGHT  	       		STRAND_NAME(VIDEO, DISPLAY_HEIGHT)
#define ATTR_VIDEO_DISPLAY_HEIGHT_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_PIXEL_WIDTH  	       		STRAND_NAME(VIDEO, PIXEL_WIDTH)
#define ATTR_VIDEO_PIXEL_WIDTH_TYPE 	   	 	UINT64, 0

#define ATTR_VIDEO_PIXEL_HEIGHT  	       		STRAND_NAME(VIDEO, PIXEL_HEIGHT)
#define ATTR_VIDEO_PIXEL_HEIGHT_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_COLOR_SPACE  	       		STRAND_NAME(VIDEO, COLOR_SPACE)
#define ATTR_VIDEO_COLOR_SPACE_TYPE 	   	 	INT, 0

#define ATTR_VIDEO_STEREO_MODE  	       		STRAND_NAME(VIDEO, STEREO_MODE)
#define ATTR_VIDEO_STEREO_MODE_TYPE 	   	 	UINT64, 0

#define ATTR_VIDEO_FLAG_INTERLACED  	       		STRAND_NAME(VIDEO, FLAG_INTERLACED)
#define ATTR_VIDEO_FLAG_INTERLACED_TYPE    	 	UINT64, 0

// attribute flag bits //
// defaults for these flags are usually defined in an attr_desc bundle
// however the attribute "owner" may adjust these
//
// mandatory means that the attribute must be created and have a value set
// - generally this will create a condition in a conditional
// until the value is set, it is considered read_write
// if the atribute has a default then it only needs to be created
// before running the functional it is associated with
// and setting the value is optional
// if there is no default and caller is unable to set a value,
// it can ask for help via a structural transform
//
#define OBJ_ATTR_FLAG_MANDATORY       	0x00001

// the default for attributes is that in input bundles attributes are read/all, write/all
// for outputs, the default is read/all write/owner
// adding this flag changes this - inputs become read/all, write/owner
// outputs become read all/write all
//  for inputs this can be set during contract negotiations to lock in a value
// for outputs, this flag only has meaning when combined with STREAM
// and indicates that that the data can be edited during the data_prepared hook
// callbacks.
#define OBJ_ATTR_FLAG_RW_CHANGE       	0x00002

// this flag is for inputs and outputs, for inputs, if it is also mandatory
// then it creates a condition that the caller must connect it to a provider,
// who agrees to respond to the data request trigger
// for outputs it indicates that the value is updated as part of the transform
// and there will be data_ready and possibly data_prep hooks
// if the attribute

#define OBJ_ATTR_FLAG_SEQUENTIAL    	0x04

// connected to remote val
#define OBJ_ATTR_FLAG_IS_CONNECTED    	0x10

// this is for connected attributes,
// indicates that the value may return REQUEST_WAIT_RETRY from data_request hook
// in this case the data_request should be done as early as possible
// so that it will be ready when actually needed
#define OBJ_ATTR_FLAG_ASYNC_UPDATE    	0x20

// indicates that the value may update spontaneously with it being possible to trigger
// data hooks
#define OBJ_ATTR_FLAG_VOLATILE	 	0x200

// indicates the value is known to be out of date, but a a transform is needed to
// update it
#define OBJ_ATTR_FLAG_NOT_CURRENT      	0x400

// indicates the value is a "best guess", the actual value of whatever it represents may differ
#define OBJ_ATTR_FLAG_ESTIMATE       	0x800

// combinations
// SEQUENTIAL  for an input indicates an optional input data stream
// SEQUENTIAL | MANDATORY for an input creates an input data condition
// SEQUENTIAL  for an output indicates an output stream
// SEQUENTIAL | RW_CHANGE for an output indicates an editable stream

// if a transform has both input and output streams then INTENT_PROCESS becomes
// INTENTION_MANIPULATE_SEQUENCE,
// adding CAP_REALTIME produces INTENTION_PLAY
// adding CAP_REMOTE produces INTENTION_STREAM, etc.
//
// if a transform has input streams inly then INTENT_PROCESS becomes
// INTENTION_RECORD,  // record

// if a transform has output streams only then INTENT_PROCESS becomes
// INTENTION_RENDER,

// ATTR CONNECTION flags
#define CONNECTION_FLAG_COPY_ON_DISCONNECT (1ull << 0)

// for optional attributes, indicates the when the connection is broken, the local attribute
// should be removed from the bundle rather than left in place
// in this case any attributes connected to this one will be disconnected first
#define CONNECTION_FLAG_DESTROY_ON_DISCONNECT (1ull << 1)

// if write connection accepted
#define CONNECTION_FLAG_READ_WRITE (1ull << 2)

// Transform flags

// this flag bit indicates that the transform can be actioned "at will"
// as if the constract were permantly marked as "agreed"
// note there may still be static (non-negotiable) conditions to action the transform
// so caller should request a consitions check before actioning it
#define TX_FLAG_NO_NEGOTIATE

// the transform does not return immediately
// depending on threading automation level, the caller should either provide a thread instance
// to action it, or ask the automation to supply one
// if caller chooses to action it using seld thread then the instance will be flagged as busy
#define TX_FLAG_ASYNC

// the transform uses system resources, therefore should be runsparingly
#define TX_FLAG_COSTLY

// this flag bit is a hint for transforns which produce sequential data
// as early as possible.
#define TX_FLAG_ASYNC_DELAY // can take time to run, therefore it is recomended to run in a thread

// function wrappings

// auto implies the real / wrapper can be determined automatically
// in case of ambiguity, real and wrapper can be used
#define FUNC_MAPPING_AUTO		0ull
// the underlying fn type, can only be one of these
#define FUNC_REAL_STANDARD 		1ull // nirva_function_t format available
#define FUNC_REAL_NATIVE 		2ull // native format available
#define FUNC_REAL_SCRIPT 		3ull // script format available
// any mapped versions available, e.g nirva -> native
//				      native -> nirva for some structurals
//					script -> native : symbol for IMPL
#define FUNC_WRAPPER_STANDARD		(1ull << 2)
#define FUNC_WRAPPER_NATIVE		(1ull << 3)
#define FUNC_WRAPPER_SCRIPT		(1ull << 4)

// info about the type, we can have segment, structural, automation, external,
// callback, conditional, placeholder, synthetic

#define FUNC_CATEGORY_UNKNOWN		0 // marks an IMPL function
#define FUNC_CATEGORY_INTERNAL		1 // marks an IMPL function
#define FUNC_CATEGORY_OUTSIDE		2 // falls outside of nirva, eg. nirva_init)
#define FUNC_CATEGORY_SEGMENT		3 // function wrapped by on traj. segment
#define FUNC_CATEGORY_STRUCTURAL       	4 // represents transform in structural
#define FUNC_CATEGORY_AUTOMATION       	5 // some kind of auto script
#define FUNC_CATEGORY_EXTERNAL   	6 // an external "implenetion dependant" fuction
#define FUNC_CATEGORY_CALLBACK		7 // function added to a hook cb stack
// functional which is a wrapper around a CASCADE:
// each node has a condition, then depending on success or fail, a next node is selected
// this continues until we reach a "value" node
// - a node with no exits, and the value or default returned).
// in technical terms, a binary decision tree)
#define FUNC_CATEGORY_CASCADE      	8
// CONDITIONAL is special configuration of CASCADE
// every fail node is NULL, success goes to the next cond
// and eventually to the value COND_SUCCESS
#define FUNC_CATEGORY_CONDITIONAL      	9
// function which selects an output, this is like a conditional in reverse
// given a set of conditions, it will try to find something which satisfies them
// all. or comes closest
#define FUNC_CATEGORY_SELECTOR       	10
#define FUNC_CATEGORY_SYNTHETIC      	11 // "functions" like a trajectory
#define FUNC_CATEGORY_PLACEHOLDER      	12 // for reference / info only - do not call
#define FUNC_CATEGORY_NATIVE_TEXT      	13 // textual transcription of native code

///////////// error codes ///////////////

#define OBJ_ERROR_NULL_OBJECT			1
#define OBJ_ERROR_NULL_ATTRIBUTE		2
#define OBJ_ERROR_INVALID_ARGUMENTS		3
#define OBJ_ERROR_NOSUCH_ATTRIBUTE		4
#define OBJ_ERROR_ATTRIBUTE_INVALID		5
#define OBJ_ERROR_ATTRIBUTE_READONLY		6
#define OBJ_ERROR_NULL_ICAP			7
#define OBJ_ERROR_NULL_CAP			8
#define OBJ_ERROR_NOT_OWNER			9

// transform status
#define TRANSFORM_ERROR_REQ -1 // not all requirements to run the transform have been satisfied
#define TRANSFORM_STATUS_NONE 0

// inital statuses
#define TRANSFORM_STATUS_DEFERRED 	1
#define TRANSFORM_STATUS_PREPARING 	2

// runtime statuses
#define TRANSFORM_STATUS_RUNNING 	16 ///< transform is "running" and the state cannot be changed
#define TRANSFORM_STATUS_WAITING 	17	///< transform is waiting for conditions to be satisfied
#define TRANSFORM_STATUS_PAUSED	 	18///< transform has been paused, via a call to the pause_hook

// transaction is blocked, action may be needed to return it to running status
#define TRANSFORM_STATUS_BLOCKED 	18///< transform is waitin and has passed the blocked time limit

// final statuses
#define TRANSFORM_STATUS_SUCCESS 	32	///< normal / success
#define TRANSFORM_STATUS_CANCELLED 	33  ///< transform was cancelled via a call to the cancel_hook
#define TRANSFORM_STATUS_ERROR  	34	///< transform encountered an error during running
#define TRANSFORM_STATUS_TIMED_OUT 	35	///< timed out waiting for data

// results returned from actioning a transform
// in the transform RESULTS item
// negative values indicate error statuses
#define TX_RESULT_ERROR        		-1
#define TX_RESULT_CANCELLED 		-2
#define TX_RESULT_DATA_TIMED_OUT 	-3
#define TX_RESULT_SYNC_TIMED_OUT 	-4

// segment was missing some data which should have been specified in the contract
// the contract should be adjusted to avoid this
// an ADJUDICATOR object may flag the contract as invalid until updated
#define TX_RESULT_CONTRACT_WAS_WRONG 	-5

// a data connection was broken and not replaced
#define TX_RESULT_CONTRACT_BROKEN 	-6

// SEGMENT_END was not listed as a possible next segment
// and no conditions were met for any next segment
// the trajectory or contract should be adjusted to avoid this
// an ADJUDICATOR object may flag the contract as invalid until updated
#define TX_RESULT_TRAJECTORY_INVALID 	-7

#define TX_RESULT_INVALID 		-8

#define TX_RESULT_NONE 			0

#define TX_RESULT_SUCCESS 		1

// failed, but not with error
#define TX_RESULT_FAILED 		2

// transform is idling and may be continued by trigering the RESUME_REQUEST_HOOK
#define TX_RESULT_IDLING 		3

//  cascade matrix node values

#define CASC_MATRIX_NOOP		0
#define CASC_MATRIX_REMOVE		1
#define CASC_MATRIX_SET_OP		2

// HOOK types
// fundamentally there are only three types of hooks - "value_updating",
// deping on the flag bit HHOK_FLAG_BEFORE,   "updated_value" and "request_update" for HOOK_FLAG_REQUEST

// hook callbacks always receive the following paramters (bundle) - the outermost bbundle conating the item (owner),
// a value change bundle, and data
// and user data provided when adding the callback (void *)
// for conditional hooks, returning FALSE blocks the value change
// for other hook types, return TRUE leaves the hook in the stack, FALSE reomvoes it.

#define NIRVA_HOOK_FLAG_SELF			(1ull << 0) // only the object istself may add callbacks

// hook caller will continue to rety conditions until they succeed
// or the hook times out
#define NIRVA_HOOK_FLAG_COND_RETRY		(1ull << 1)

// conditions will be checked once only on fail the operation triggering the hook
// will be abandoned. For request hooks, REQUEST_NO will be returned
#define NIRVA_HOOK_FLAG_COND_ONCE		(1ull << 2)

// for DATA_HOOK_TYPE - this indicate the hook should be tirgger before the change is made
// if present then this is VALUE_UPDATING, else it is UPDATED_VALUE (the default DATA_HOOK_TYPE)
// or VALUE_INITED_VALUE
// or VALUE_FREEING
#define NIRVA_HOOK_FLAG_BEFORE		(1ull << 3)

// indicates a request type hook, rather than being triggered by a data change, request some
// other object change its data or allows some action
// caller will receive either REQUEST_NO - the request is denied, REQUEST_YES - the request
// is accepted, or REQUEST_WAIT_RETRY - the requst is denied temporarily, caller can retry
// and it may be accepted later
#define NIRVA_HOOK_FLAG_REQUEST	      	(1ull << 4)

// spontaneous

// indicates the data which is in the bundle_in is ready for previewing and or / editing
// the transform will call hooks in sequence to give each observer a chance to edit
#define NIRVA_HOOK_FLAG_DATA_PREP     	(1ull << 5)

// data in bundle_in is in its final format, all hook cbs will be called in parallel
// if possible. The transform may continue so it can start processing the next data
// however the data in bundle_in will not be altered or freed until all calbacks have returned
#define NIRVA_HOOK_FLAG_DATA_READY     	(1ull << 6)

// objects have an "idle" queue where prepared contracts can be added to be run in the background
// while in this state, the tx statuse will be "QUEUED"
// when adding a callback (dropping a contract), no callback function is specified
// the automation will set this to a structure function to action the contract transform
// condcheck will be run to make sure the conditions are still valid
// reduce the wait time, it may be possible to do one or more of the following, depending on
// ATTR_STRUCTURAL_THREAD_MODEL
// thread-per-instance:
// fins a transform which creates a duplicate instance
// thread-on-request model : try asking the structure to create more threads
#define NIRVA_HOOK_FLAG_IDLE			(1ull << 7)

// hook was triggered by some underlying condition, eg. a native signal rather than being
// generated by the application
#define NIRVA_HOOK_FLAG_NATIVE      		(1ull << 8)

NIRVA_TYPEDEF_ENUM(nirva_hook_patterns, NIRVA_DATA_HOOK, NIRVA_REQUEST_HOOK, \
                   NIRVA_SPONTANEOUS_HOOK, NIRVA_N_HOOK_PATTERNS)

NIRVA_ENUM {
  // The following are the standard hook points in the system
  // objects and attributes (any bundle with a refcount sub-bundle)
  // may add a callback function to these
  // all DATA_HOOKS muts return "immedaitely"

  // OBJECT STATE and SUBTYPE HOOKS
  // for an APPLICATION instance, these are GLOBAL HOOKS
  // these are also passive hooks, provided the system has semi or full hook automation
  // the structure will call them on behalf of a thread instance

  // when an object instance is created, this hook will be triggered
  // in the creator template or instance, and will contain a pointer to the freshly created
  // or copied instance - the new instance ,ay be of a differnet type / subtype
  // to the creator
  OBJECT_CREATED_HOOK, // object state / after

  INSTANCE_COPIED_HOOK,

  // conditions:
  // IS_EQUAL(GET_BUNDLE_VALUE_UINT64(BUNDLE_IN, "FLAGS"), 0)
  // STRING_MATCH(GET_BUNDLE_VALUE_STRING(BUNDLE_IN, "NAME"), STRAND_OBJECT_STATE)
  // IS_EQUAL(GET_BUNDLE_VALUE_INT(GET_SUB_BUNDLE(GET_SUB_BUNDLE(BUNDLE_IN, "CHANGED"),
  // "NEW_VALUE"), OBJ_STATE_NORMAL))
  // NEW_HOOK_TYPE = INIT_HHOK, PMAP: 0, GET_BUNDLE_VALUE_BUNDLEPTR(BUNDLE_IN, OBJECT), 1,
  // GET_BUNDLE_VALUE_VOIDPTR(CALLBACK, DATA), -1 GET_PTR_TO(BUNDLE_OUT, RETURN_VALUE)

  // object suffered a FATAL error or was aborted,
  FATAL_HOOK,

  // state changing from normal -> not ready, i.e. restarting
  RESETTING_HOOK,  // object state / before

  // object is about to be freed
  // THIS IS A VERY IMPORTANT HOOK POINT, anything that wants to be informed when
  // a bundle is about to be freed should add a callback here
  // this is actually the HOOK_CB_REMOVED hook for the bundle
  // all callbacks are force removed when an object is about to be recycled
  DESTRUCTION_HOOK, // hook_cb_remove, bundle_type == object_instance

  // object subtype change
  MODIFYING_SUBTYPE_HOOK,

  SUBTYPE_MODIFIED_HOOK,

  // object subtype changed
  ALTERING_STATE_HOOK,

  STATE_ALTERED_HOOK,

  N_OBJECT_HOOKS,

#define N_GLOBAL_HOOKS N_OBJECT_HOOKS
  ADDING_STRAND_HOOK,

  DELETING_STRAND_HOOK,

  STRAND_ADDED_HOOK,

  STRAND_DELETED_HOOK,

  // this hook is triggered after reading a value.
  // input.value holds a copy of the value
  // read., to prevent overloading this requires PRIV_HHOKS > 10 to add a callback
  VALUE_READ_HOOK,

  // DATA_HOOK + NIRVA_HOOK_FLAG_BEFORE
  APPENDING_ITEM_HOOK,

  REMOVING_ITEM_HOOK,

  CLEARING_ITEMS_HOOK,

  UPDATING_VALUE_HOOK,

  VALUE_UPDATED_HOOK,

  ITEMS_CLEARED_HOOK,

  ITEM_APPENDED_HOOK,

  ITEM_REMOVED_HOOK,

  // versions with ATTR are cascaded values if the bundle is an ATTR_CONTAINER
  // and attributes are being added or deleted. In this case multiple
  // calls of the same type may be combined into one.
  // to monitor individual values, use the STRAND and DATA hooks
  ATTRS_ADDED_HOOK,

  ATTRS_UPDATED_HOOK,

  ATTRS_DELETED_HOOK,

  // associated with transaction / thread_instance status change

  PREPARING_HOOK,  /// none -> prepare

  PREPARED_HOOK,  /// prepare -> running

  TX_START_HOOK, /// any -> running

  ///
  PAUSED_HOOK, ///< transform was paused via pause_hook

  ///< transform was resumed via resume hook (if it exists),
  // and / or all paused hook callbacks returning
  RESUMING_HOOK,

  TIMED_OUT_HOOK, ///< timed out in hook - need_data, sync_wait or paused

  FINISHED_HOOK,   /// running -> finished -> from = we can go to SUCCESS,
  // ERROR, DESTRUCTION, etc.,
  COMPLETED_HOOK,   /// finished with no errors, end results achieved

  ///< error occured during the transform
  // if the object has a transform to change the status
  // back to running this should be actioned
  // when the FINISHED_HOOK is called
  // otherwise if there is a transform to return the status to normal
  // this should be actioned instead
  // and TX_RESULT_ERROR returned
  // otherwise do nothing, and let the transform return TX_RESULT_ERROR
  ERROR_HOOK,

  CANCELLED_HOOK, ///< tx cancelled via cancel_hook, transform will return TX_RESULT_CANCELLED

  // SPONTANEOUS HOOKS
  //
  // spontaneous hooks are not triggered by data changes, but rather as a response to
  // events, or at fixed points in a transform.
  //
  //
  // thread instances have a special "idle" hook stack,
  // this will be called continuously when the thread is "idleing", ie. not running
  // a transform. callbacks can be for instance running a transform, and hence they may
  // block for some considerable time. It is posible to add any type of callback,
  // e.g a cascade, condition check, function, script.
  // the flags provided when adding the callback determine the callback type
  // cond_once, cond_retry, seqeuential, async, etc
  // thread_herder if running attaches callbacks to each thread, so it can know
  // which are idleing, when a contract is actioned in the bg, broker or negotiator
  // will forward it to thread_herder so that the transform it points to can be assinged
  // to either a thread of the contract instance or to a pool thread, if none are avaialble
  // then thread herder may create or ask to borrow more.
  // if none are available after this, the transform will be "deferred"
  // note also, some transforms can only be actioned by the object thread (eg. GUI's
  // transforms) so to aboid overloading the object, flags like HOOK_UNIQUE_TX
  // can be used to reduce the queue size
  IDLE_HOOK,
  //
  // this is a "self hook" meaning the object running the transform only should append to this
  // the hook response type is COND_RETRY
  SYNC_WAIT_HOOK, ///< synchronisation point, transform is waitng until

  /// tx transition from one trajectory segment to the next
  // after this hook returns. a cascade will be run to decide the next segment
  // this hook provides an opportunity to affect the choice
  // bundle out will contain an attribute for next segment choice
  // setting this will require PRIV_TRANSFORMS > 10
  SEGMENT_END_HOOK,

  // this is triggered when a new trajectory segment is about to begin
  // for the inital segment, TX_START is triggered instead
  // for segment end, FINISHED_HOOK runs instead
  SEGMENT_START_HOOK,

  // if for some reason a Transform cannot be started immediately
  // this hook should be triggered
  // for example, the Transform may be queued and waiting to be processed
  TX_DEFERRED_HOOK,

  ///< tx is blocked, may be triggered after waiting has surpassed a limit
  // but hasnt yet TIMED_OUT
  //
  // to any hooks with callbacks - callbacks shoudl return quickly
  // but especilly important for hooks with cb type COND_RETRY

  // an ARBITRATOR object can attempt to remedy the situation,
  // for SYNC_WAIT this implies finding which Conditions are delaying and attempting
  // to remedy this
  // for DATA_REQUEST this implies hceking why the data provider is delaying,
  // and possibly finding
  // a replacement
  // for SEGMENT, the arbitrator may force the Transform to resume,
  // if multiple next segments are causing the
  // delay it may select which one to follow next, preferring segment_end
  // if avaialble, to complete the transform

  TX_BLOCKED_HOOK,

  // calbacks for the following two hooks are allowed to block "briefly"
  // so that data can be copied
  // or edited. The hook callback will receive a "max_time_target" in input, the value depends
  // on the caller and for data prep this is divided depending on the number
  // of callbacks remaing
  // to run. The chronometer may help with the calculation.
  // objects which delay for too long may be penalized, if they persistently do so

  // tx hooks not associated ith status change
  // if the data is readonly then DATA_PREVIEW_HOOK is cal
  DATA_PREVIEW_HOOK,

  // data in its "final" state is ready
  // data is readonly. The Transform will call all callbacks in parallel and will not block
  // however it will wait for all callbacks to return before freeing / altering the data
  DATA_READY_HOOK,

  // this hook is triggered when an object is destructing
  // data includes the hook stack owner, item, hook number, hook handle and
  // user data. The callback is also called when the hook_detachimg callback itself
  // is detached
  // in fact, DESTRUCTING hook callbacks are HOOK_DETACHING callback for for the DESTUCTING
  // hook stack
  HOOK_DETACHING_HOOK,

  // hook is triggered in an object if a callback has been placed in its
  // idle stack
  IDLE_QUEUED_HOOK,

  // thread running the transform received a fatal signal
  THREAD_EXIT_HOOK,

  // this can be used for debugging, in a function put NIRVA_CALL(tripwire, "Reason")
  // the hook stack will be held in one or other structural subtypes
  TRIPWIRE_HOOK,

  // REQUEST HOOKS - tx will provide hook callbacks which another object can trigger
  // if the target object is not active, or is busy, the automation may respond as a proxy

  // these hooks are triggered by a structure transform:
  // nirva_request_hook(fn_input, fn_output)
  // fn_input should contain at least: caller_object, target_object, target_dest, hook_number,
  // (or hook_type, hook_flags), and any other strands / attributes specified by the hook
  // (see documentation for details), the output and reponse depend on the request type

  // this request should be triggered to read the value of any strand (ex.
  // the data strand of an attribute). if there ar no callbacks then the
  // value is simply returned
  // Triggering this hook is normally done automatically in a mcro e.g. NIRVA_GET_VALUE
  // If there are no callbacks, the reuslt will be simply to retuen the value of the strand.
  // For added (user or structure) callbacks, the type of callback here is "Oraculor",
  // the return codes are the same as thos used for conditions, with the followin menaings:
  // COND_FAIL, COND_SUCCESS - continue to next cb, on COND_FAIL the cb will be removed
  // COND_FORCE - the callback can povide an override in fn_output.valu.data to be returned
  // instead. COND_WAIT_RETRY - data not ready, caller should wait and retry.
  // This is the return code returned during a transform by a data provider when it is not yet
  // ready to supply new data.
  // COND_ERROR - if the strand does not exist, or other erro,
  // Since end caller expects a value
  // back this can be cehcked for via NIRVA_ERR_CHECK.
  // When reading a value from an array, optionally fn_in may contain a BUNDLE_MATCH_CONDITION
  // which can facilitate searching for a specific item in the array.
  // This is utilised by keyed
  // arrays to optimise locating strands (attribute by name, bundle by uid and so on).

  DATA_REQUEST_HOOK,

  // asks the structure to check if it is OK to add a strand to a bundle
  // if the strand is in the blueprint it will always return REQUEST_YES
  // otherwise a COND_CHECK on PRIV_STRANDS is done,
  // ormally requires PRIV_STRANDS > 10, to prevent this from happening
  // accidentally, requests originating from a thread in another object require priv >50
  // adding a strand to a template requires priv > 100
  // and to a structural, priv > 200
  ADD_STRAND_REQUEST_HOOK,

  // asks the structure if it is OK to delete a strand
  // if the strand is optional in the blueprint it will always return REQUEST_YES
  // otherwise a COND_CHECK on PRIV_STRANDS is done,
  // ormally requires PRIV_STRANDS > 20, to prevent this from happening
  // accidentally, requests originating from a thread in another object require priv >70
  // to stop runaway threads from deleting parts of the system
  // deleting a strand from a template requires priv > 100
  // and from a structural, priv > 1000
  DELETE_STRAND_REQUEST_HOOK,

  // this is called by autopmation in response to refcount going below zeo
  // it will set the destruct_request flagbit in the thread
  // the thread should abort the transform ASAP, put the object in the zobie state and
  // deliver the object to the recycler for recycling
  // equivalent to request_update on the bundle;s REFECOUNT.SHOULD_FREE and a target
  // value of NIRVA_TRUE
  // (in this case the "value" becomes an offset)
  // needs PRIV_LIFECYCLE > 10
  DESTRUCT_REQUEST_HOOK,

  // asks the bundle (which must have a refcount sub-bundle) to increase the refcount
  // automation will add the caller uid to a list, so that only obejcts which added a
  // ref can remove one.  The initial value of the array contains the UID of the ownere object,
  // so that it can always unref 1 extra time
  // equivalent to request_update on the bundle;s REFECOUNT.REFCOUNT and a target 'value' of +1
  // (in this case the "value" becomes an offset)
  REF_REQUEST_HOOK,

  // request the objec decrement the refcount by 1. The caller must previosly have added a ref
  // (the deafult automation will add a single value with owner object UID
  // obejct threads should remove 1 ref and if the refcount goes below zero, abort the tx
  // deliver the instance or attribute to the bundle recycler and withdraw from the instance
  // equivalent to request_update on the bundle;s REFECOUNT.REFCOUNT and a target
  // 'value' of -1
  // (in this case the "value" becomes an offset)
  UNREF_REQUEST_HOOK,


  // if the transform exposes this hook, then this request can be triggered after or
  // during a transform. If accepted, the target
  // will reverse the pervious data update either for the attribute of for
  // the previous transform
  // target / value TBD. If the transform cannot be undone further,
  // NIRVA_REPSONE_NO should be returned
  UNDO_REQUEST_HOOK,

  // if the transform exposes this hook, then this request can be triggered after or
  // during a transform. If accepted, the target
  // will reverse the pervious data undo either for the attribute of
  // for the previous transform
  // target / value TBD. If the transform cannot be redone further,
  // NIRVA_REPSONE_NO should be returned
  REDO_REQUEST_HOOK,

  // this is called BEFORE a remote attribute connects to a local attribute
  // this is a SELF hook, meaning only the object owning the attribute may add callbacks
  // returng RQUEST_NO blocks the connection
  //
  // and remote attr maps a pointer to the local attr. The reomote attribute
  // gets the same flags as the local one
  // plus the CONNECTED flagbits. The prior data is not freed,
  // however when the attribute disconnects
  // target is attribute connections_in. array, data is ptr to local attr
  ATTR_CONNECT_REQUEST,

  //
  // if the app has an arbitrator, an object bound to a contract may tigger this, and on
  // COND_SUCCESSm the attribute will be disconnected with no penalties
  // target is local attribute "value", value is NULL
  SUBSTITUTE_REQUEST_HOOK,
  //
  // target is transform "status", value is cancelled
  CANCEL_REQUEST_HOOK, // an input hook which can be called to cancel a running tx
  //
  // ask the transform to pause processing. May not happen immediately (or ever,
  // so add a callback
  // for the PAUSED hook)
  // will wait for all callbacks to return, and for unpause hook (if it exists) to be called
  // target is transform "status", value is pausedd
  PAUSE_REQUEST_HOOK,
  // if this hook exists, then to unpause a paused transform this must be called
  // and all paused
  // callbacks must have returned (may be called before the functions return)
  // after this, the unpaused callbacks will be called and processing will only continue
  // once all of those have returned. Calling this  when the tx is not paused or
  // running unpaused
  // hooks will have no effect
  // target is transform "status", value is resuming
  RESUME_REQUEST_HOOK,
  //
  // hooks reserved for internal use by instances
  INTERNAL_HOOK_0,
  INTERNAL_HOOK_1,
  INTERNAL_HOOK_2,
  INTERNAL_HOOK_3,
  ///
  N_HOOK_POINTS,
} nirva_hook_number;

#define RESTART_HOOK RESETTING_HOOK

// for some hook callbacks a value of true returned means the hook callback will stay in the
// callback stack. Setting this ensures that it is removed after the first call, even if
// true is returned
#define HOOK_CB_FLAG_ONE_SHOT			(1 << 1)


#define NIRVA_BUNDLEPTR NIRVA_PTR_TO(NIRVA_BUNDLE_T)
#define NIRVA_CONST_BUNDLEPTR NIRVA_CONST NIRVA_BUNDLEPTR
#define NIRVA_BUNDLE_TYPE bundle_type
#define NIRVA_BUNDLEPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR)
#define NIRVA_CONST_STRING NIRVA_CONST NIRVA_STRING
#define NIRVA_STRING_ARRAY NIRVA_ARRAY_OF(NIRVA_STRING)
#define NIRVA_CONST_STRING_ARRAY NIRVA_CONST NIRVA_STRING_ARRAY

NIRVA_TYPEDEF(NIRVA_BUNDLEPTR, nirva_bundleptr_t)

#endif // !SKIP_MAIN

//
#define BDEF_DEF_CONCAT(BNAME, PRE, EXTRA) PRE BNAME##EXTRA,
#define ATTR_BDEF_DEF_CONCAT(ATTR_BNAME, PRE, EXTRA) PRE ATTR_BNAME##EXTRA,

#define BUNLIST(BDEF_DEF, pre, extra) BUNLISTx(BDEF_DEF, BDEF_DEF, pre, extra)
#define ABUNLIST(ABDEF_DEF, pre, extra) ABUNLISTx(ABDEF_DEF, ABDEF_DEF, pre, extra)

#define BUNLISTx(BDEF_DEFx_, BDEF_DEF_, pre, extra)			\
    BDEF_DEF_(STRAND, pre, extra) BDEF_DEF_(DEF, pre, extra)		\
      BDEF_DEF_(STRAND_DESC, pre, extra)				\
      BDEF_DEF_(BLUEPRINT, pre, extra) BDEF_DEF_(VALUE, pre, extra)	\
      BDEF_DEF_(ATTR_VALUE, pre, extra) BDEF_DEF_(VALUE_CHANGE, pre, extra) \
      BDEF_DEF_(ATTR_DESC, pre, extra) BDEF_DEF_(PMAP, pre, extra)	\
      BDEF_DEF_(PMAP_DESC, pre, extra) BDEF_DEF_(ATTRIBUTE, pre, extra)	\
      BDEF_DEF_(ATTR_CONNECTION, pre, extra) BDEF_DEF_(EMISSION, pre, extra) \
      BDEF_DEF_(FN_INPUT, pre, extra) BDEF_DEF_(FN_OUTPUT, pre, extra)	\
      BDEF_DEF_(FUNCTIONAL, pre, extra) BDEF_DEF_(FUNC_DESC, pre, extra) \
      BDEF_DEF_(CAP, pre, extra)  BDEF_DEF_(REQUEST, pre, extra)	\
      BDEF_DEF_(INDEX, pre, extra) BDEF_DEF_(ERROR, pre, extra)		\
      BDEF_DEF_(ICAP, pre, extra) BDEF_DEF_(ATTR_POOL, pre, extra)	\
      BDEF_DEF_(HOOK_DETAILS, pre, extra) BDEF_DEF_(HOOK_STACK, pre, extra) \
      BDEF_DEF_(HOOK_CB_FUNC, pre, extra) BDEF_DEF_(COND_ARRAY, pre, extra) \
      BDEF_DEF_(OBJECT_TEMPLATE, pre, extra) BDEF_DEF_(OBJECT_INSTANCE, pre, extra) \
      BDEF_DEF_(THREAD_INSTANCE, pre, extra) BDEF_DEF_(OWNED_ATTRS, pre, extra) \
      BDEF_DEF_(TRANSFORM, pre, extra) BDEF_DEF_(OBJECT, pre, extra)	\
      BDEF_DEF_(CONTRACT, pre, extra) BDEF_DEF_(LOCATOR, pre, extra)	\
      BDEF_DEF_(MATRIX_2D, pre, extra) BDEF_DEF_(CONDVAL_NODE, pre, extra) \
      BDEF_DEF_(CONDLOGIC, pre, extra) BDEF_DEF_(CASCADE, pre, extra)	\
      BDEF_DEF_(CONSTVAL_MAP, pre, extra) BDEF_DEF_(CASCMATRIX_NODE, pre, extra) \
      BDEF_DEF_(TSEGMENT, pre, extra) BDEF_DEF_(SCRIPTLET, pre, extra) \
      BDEF_DEF_(TRAJECTORY, pre, extra) BDEF_DEF_(KEY_LOOKUP, pre, extra) \
      BDEF_DEF_(FUNC_DESC_CON, pre, extra) BDEF_DEF_(ATTR_CON, pre, extra) \
      BDEF_DEF_(ATTR_DESC_CON, pre, extra) BDEF_DEF_(OBJECT_CON, pre, extra)

#define ABUNLISTx(ABDEF_DEFx_, ABDEF_DEF_, pre, extra) ABDEF_DEF_(ASPECT_THREADS, pre, extra)

#ifdef IS_BUNDLE_MAKER

#include "nirva_auto.h"

#else

NIRVA_TYPEDEF_MULTI(bundle_type, \
                    NIRVA_ENUM {BUNLIST(BDEF_DEF_CONCAT,, _BUNDLE_TYPE) \
                                n_builtin_bundledefs});

NIRVA_TYPEDEF_MULTI(attr_bundle_type, \
                    NIRVA_ENUM {ABUNLIST(ATTR_BDEF_DEF_CONCAT,, _ATTR_BUNDLE_TYPE) \
                                n_builtin_attr_bundles});
#ifdef BDEF_DEF
#undef BDEF_DEF
#endif
#ifdef ABDEF_DEF
#undef ABDEF_DEF
#endif

NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY all_strands;
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_strands(NIRVA_CONST_STRING fmt, ...);
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_bundledef(NIRVA_CONST_STRING name,	\
    NIRVA_CONST_STRING pfx, ...);
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY maker_get_bundledef(bundle_type btype);

#define GET_BDEF(btype) maker_get_bundledef(btype)

#ifndef MB
#define MB(name, pfx, ...) make_bundledef(name, pfx, __VA_ARGS__, NULL)
#endif
#ifndef MBX
#define MBX(name) MB(#name, NULL, _##name##_BUNDLE)
#endif
#ifndef MABX
#define MABX(name) MB(#name, NULL, _##name##_ATTR_BUNDLE)
#endif
#define BDEF_DEFINE(BNAME) NIRVA_CONST_STRING_ARRAY BNAME##_BUNDLEDEF

// these values may be adjusted during inial bootstrap - see the documentation for more details
// values here define the level of automation for each "aspect"
#define NIRVA_AUTOMATION_NONE 			0
#define NIRVA_AUTOMATION_MANUAL 	       	1
#define NIRVA_AUTOMATION_SEMI 			2
#define NIRVA_AUTOMATION_FULL 			3

#define NIRVA_AUTOMATION_DEFAULT 		NIRVA_AUTOMATION_SEMI

// NIRVA_ASPECT_BUNDLE_MAKING
// NIRVA_ASPECT_NEGOTIATING
// NIRVA_ASPECT_FUNCTION_BENDING
// NIRVA_ASPECT_OPTIMISATION
// NIRVA_ASPECT_SELF_ORGANISATION

#define NIRVA_ASPECT(m) NIRVA_ASPECT_NAME_##n

// automation for these "aspects"
#define NIRVA_ASPECT_NONE			0

#define NIRVA_ASPECT_THREADS			1
#define NIRVA_ASPECT_NAME_1 "THREADS"
// attributes created intially in STRUCTURE_PRIME
// preferred subtypes: THREAD_HERDER
// the attributes are transfered to preferred subtypes in order of preference
// changing these after structure_app is in PREPARED state requires PRIV_STRUCTURE > 100

// AUTOMATION LEVELS
// for med. and occasionally for manual thread aspects,
// should be defined to something which returns a void * to a Native Thread,

#define ATTR_AUTOLEVEL_THREADS				ATTR_NAME("AUTOLEVEL", "THREADS")
#define ATTR_AUTOLEVEL_THREADS_TYPE 		       	INT, NIRVA_AUTOMATION_DEFAULT

// other atttributes //////////////////

#define ATTR_THREADS_MODEL				ATTR_NAME("THREADS", "MODEL")
#define ATTR_THREADS_MODEL_TYPE				INT,

// no model - this is the default for autmoation levels none and manual
#define NIRVA_THREAD_MODEL_NONE				0

// this is the default for semi and full, threads are in a pool and assigned as needed
// to negotiaite, run transforms and monitor timeouts
#define _NIRVA_THREAD_MODEL_ON_DEMAND		       	1

// a thread is assigned to each instance with active transforms
// the thread remains bound to the instance but will idle while the instance is "dormant"
// the thread will be destroyed when the instance is recycled
// this is best for applications with a small number of active instances which need to interact
// with each other at all times
#define NIRVA_THREAD_MODEL_ONE_PER_INSTANCE    	       	2

// implementation functions (NEED defining)
// for automation level semi, these funcitons need to be defined externally
// for thread model ON_DEMAND
// these functions should return a native thread from a pool, and then return them later
// thread must not be passed again until it returns
// the amount of time the Thread will be borrowed for cannot be detrmined
// in some cases it will never be returned. in other cases it will be returned almost imemdiately
#define ATTR_IMPLFUNC_BORROW_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "BORROW_NATIVE_THREAD")
#define ATTR_IMPLFUNC_BORROW_NATIVE_THREAD_TYPE       FUCNPTR, NULL

#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "RETURN_NATIVE_THREAD")
#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD_TYPE       FUCNPTR, NULL

// for FULL automation, the structure will handle all aspects of thread management itself

#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "CREATE_NATIVE_THREAD")
#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD_TYPE       FUCNPTR, NULL

#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "DESTROY_NATIVE_THREAD")
#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD_TYPE      FUCNPTR, NULL

// this is a condition to be checked, on NIRV_COND_SUCCESS, the atribute becomes mandatory
#define __COND_THREADS_1 NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI), NIRVA_VAL_EQUALS,		\
    _COND_ATTR_VAL(ATTR_THREADS_MODEL), _COND_INT_VAL(NIRVA_THREAD_MODEL_ON_DEMAND))

// this is a condition to be checked, on NIRV_COND_SUCCESS, the attribute becomes mandatory
#define __COND_THREADS_2 NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI), NIRVA_VAL_EQUALS,		\
    _COND_ATTR_VAL(ATTR_THREADS_MODEL),					\
    _COND_INT_VAL(NIRVA_THREAD_MODEL_INSTANCE_PER_THREAD),		\
    NIRVA_OP_OR, NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_FULL))

#define _ASPECT_THREADS_ATTRBUNDLE #THREADS_ASPECT, ADD_ATTR(AUTOLEVEL, THREADS), \
    ADD_ATTR(THREADS, MODEL), ADD_COND_ATTR(IMPLFUNC, 101, NIRVA_EQUALS, \
					    ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, \
					    NIRVA_EQUALS, ATTR_THREADS_MODEL, \
					    NIRVA_THREAD_MODEL_ON_DEMAND), \
    ADD_COND_ATTR(IMPLFUNC, 102, ADD_COND_ATTR(IMPLFUNC, 103, _COND(THREADS, 2)), \
    ADD_COND_ATTR(IMPLFUNC, 104, _COND(THREADS, 2))

#define NIRVA_OPT_FUNC_101 borrow_native_thread,			\
    "request a native thread from application thread pool",RETURN_NATIVE_THREAD,0
#define NIRVA_OPT_FUNC_102 return_native_thread,"return a native thread to  application thread pool", \
    NIRVA_NO_RETURN,1,NIRVA_NATIVE_THREAD,thread

#define _MAKE_IMPL_COND_FUNC_1_DESC(n)					\
  (n == 101 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_101) : n == 102 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_102) : \
   n == 103 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_103) : n == 104 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_104) : NULL)

////////////////////////////////////////////////

#define NIRVA_ASPECT_HOOKS			2

#define NIRVA_ASPECT_NAME_2 "HOOKS"

// attributes created intially in STRUCTURE_APP
// preferred subtypes: AUTOMATION
// the attributes are transfered to preferred subtypes in order of preference
// changing these after structure_app is in PREPARED state requires PRIV_STRUCTURE > 100

// level of automation for hooks
//  - full, the structure will manage every aspect,including calling user callbcaks
//  - semi - system hooks will run automatically, but the application will run user_callbacks
//  - manual - the automation will signal when to trigger hooks and wait for return, but nothing more

// this is a condition to be checked, on NIRV_COND_SUCCESS, the atribute becomes mandatory
#define __COND_HOOKS_1 NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_HOOKS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI)
#define __COND_HOOKS_2 NIRVA_VAL_NOT_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_FULL)

#define ATTR_AUTOLEVEL_HOOKS				ATTR_NAME("AUTOLEVEL", "HOOKS")
#define ATTR_AUTOLEVEL_HOOKS_TYPE 		       	INT, NIRVA_AUTOMATION_DEFAULT

#define _ASPECT_HOOKS_ATTRBUNDLE #HOOKS_ASPECT, ADD_ATTR(AUTOLEVEL, HOOKS), \
    ADD_COND_ATTR(IMPLFUNC, 201,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 202,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 203,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 204,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 205,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 206,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 207,_COND(HOOKS, 1), _COND_(HOOKS, 2)),	\
    ADD_COND_ATTR(IMPLFUNC, 208,_COND(HOOKS, 1), _COND_(HOOKS, 2)),	\
    ADD_COND_ATTR(IMPLFUNC, 209,_COND(HOOKS, 1), _COND(HOOKS, 2))

#define NIRVA_OPT_FUNC_201 add_hook_callback // etc
#define NIRVA_OPT_FUNC_202 remove_hook_callback // etc
#define NIRVA_OPT_FUNC_203 trigger_hook_callback // etc
#define NIRVA_OPT_FUNC_204 clear_hook_stackk // etc
#define NIRVA_OPT_FUNC_205 clear_all_stacks // etc
#define NIRVA_OPT_FUNC_206 list_hook_stacks // etc
#define NIRVA_OPT_FUNC_207 join_async_hooks // etc
#define NIRVA_OPT_FUNC_208 run_hooks_async // etc
#define NIRVA_OPT_FUNC_209 run_hooks_aync_seq


#define _MAKE_IMPL_COND_FUNC_2_DESC(n)			\
  (n == 201 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_201) :	\
   n == 202 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_202) :	\
   n == 203 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_203) :	\
   n == 204 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_204) :	\
   n == 205 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_205) :	\
   n == 206 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_206) :	\
   n == 207 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_207) :	\
   n == 208 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_208) :	\
   n == 209 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_209) : NULL)

////////////////////////////////////////////////

#define NIRVA_ASPECT_REFCOUNTING		3
#define NIRVA_ASPECT_NAME_3 "REFCOUNTING"

// level of automation for refcounting
//  - full, the structure will manage every aspect,including freeing bundles when refcount reaches -1
//  - semi - the structure will manage reffing and unreffing, but will only indicate when a bundle
//		should be free
//  - manual - the automation will signal when to ref / =unref somthing

#define ATTR_AUTOLEVEL_REFCOUNTING	       		ATTR_NAME("AUTOLEVEL", "REFCOUNTING")
#define ATTR_AUTOLEVEL_REFCOUNTING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

#define __COND_REFCOUNTING_1 NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_REFCOUNTING), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI)

#define _ASPECT_REFCOUNTING_ATTRBUNDLE #REFCOUNTING_ASPECT, ADD_ATTR(AUTOLEVEL, REFCOUNTING, \
    ADD_COND_ATTR(IMPLFUNC, REF_BUNDLE_301,_COND(REFCOUNTING, 1)),	\
    ADD_COND_ATTR(IMPLFUNC, UNREF_BUNDLE_302,_COND(REFCOUNTING, 1))

// add mutex_trylock, mutex_unlock

#define _MAKE_IMPL_COND_FUNC_3_DESC(n)			\
  (n == 301 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_301) :	\
   NULL)
////////////////////////////////

#define NIRVA_ASPECT_RECYCLING			4
#define NIRVA_ASPECT_NAME_4 "RECYCLING"

// level of automation for bundle recycling bundle memory and connectiosn
//  - full, the structure will manage every aspect, including recycling bundles as instructed
// by refcounter
//  - semi - the structure will handke automated activites like disconnecting atributes, detaching hooks
//		but will leave freeing to the applicaiton
//  - manual - the automation will signal when to recycle bundledefs

#define ATTR_AUTOLEVEL_RECYCLING	       		ATTR_NAME("AUTOLEVEL", "RECYCLING")
#define ATTR_AUTOLEVEL_RECYCLING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

#define __COND_RECYCLINGING_1 NIRVA_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_RECYCLING), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI)

#define _ASPECT_RECYCLING_ATTRBUNDLE #RECYCLING_ASPECT, ADD_ATTR(AUTOLEVEL, RECYCLING),	\
    ADD_COND_ATTR(IMPLFUNC, FREE_BUNDLE_401,_COND(RECYCLING, 1))

#define _MAKE_IMPL_COND_FUNC_4_DESC(n)			\
  (n == 401 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_401) : NULL)

////////////////////////////////

#define NIRVA_ASPECT_TRANSFORMS			5
#define NIRVA_ASPECT_NAME_5 "TRANSFORMS"

// level of automation for running monitoring transforms
//  - full, the structure will manage every aspect, including transform trajectories
// by refcounter
//  - semi - the structure will handle automated activities like running structure functions
// 		thre applications will be able ot particpate in the process
//  - manual - the automation will signal which functions to run but the application will run them

#define ATTR_AUTOLEVEL_TRANSFORMS	       		ATTR_NAME("AUTOLEVEL", "TRANSFORMS")
#define ATTR_AUTOLEVEL_TRANSFORMS_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

#define _ASPECT_TRANSFORMS_ATTRBUNDLE #TRANSFORMS_ASPECT, ADD_ATTR(AUTOLEVEL, TRANSFORMS)

////////////////////////
// list of structural subtypes
// in nirva_init(), object_template STRUCTURE_PRIME should be created
// STRUCTURE_PRIME should then create an instance of itself with subtyoe APP
// STRUCTURE_APP, which should be returned.

// STRUCTURE_APP will initially be in state NOT_READY, the first goal should be
// to get it to state running. This involves first getting it to state prepared
// the transform for this will cause Prime to instantiate any other subtypes
// as defined by APPs attributes. Following this, Prime can distribute APPs attribute bundles
// to their preferred subtypes (in order of preference). When doing so a condition is added
// to the VALUE_UPDATING hook of each one.
// The condition is NIRVA_GT(NIRVA_GET_INT_VALUE,SELF_ATTRS,PRIV_STRUCTURE,100)
// Prime can also distribute the contracts for all "structural transforms", depending on the subtypes
// available. Subtypes can later be started ot stopped, this will casue Prime to reallocate
// attributes and contracts accordingly.

// the minimal recommended set is THREADHERDER and AUTOMATION - these are on by default

// BROKER is also reommended

// for medium sized apps it is recommende to add NEGOTIATOR, ARIBTRATOR and ADJUDICATOR.

// for larger apps, ARCHIVER and UI_ARRANGER if it uses a front end

// for real time apps, it can be useful to add CHRONOMETER,  PERFORMANC_MANAGER,
// HARDWARE_CONTROLLER and possibly OS_LIASON for system optimisations

// for self-organinsing, AI driven apps , it can be good to enable STATUS_MONITOR, STRATEGIST,
// and / or USER_

///
//
/// STARTUP and enabling / disabling subsytems
// to bootstrap the NIRVA structure, call:

// NIRVA_BNUNDLE_T structure_app = nirva_init();

// after this,

// NIRVA_CALL(enable_subsystem, STRUCTURE_SUBTYPE_BROKER);

// this should return (NIRVA_REQUEST_RESPONSE)  NIRVA_REQUEST_YES

// a subsystem can be disabled with NIRVA_CALL(disable_subsystem, STRUCTURE_SUBTYPE_BROKER);

// then:
// NIRVA_CALL(satisfy_intent, NIRVA_INTENT_UPDATE_VALUE, structure_app,
//		STRAND_OBJECT_STATE, NIRVA_OBJ_STATE_PREPARED);
// or just simply:
// NIRVA_CALL(change_object_state, structure_app, NIRVA_OBJ_STATE_PREPARED);

#define _ASPECT_AFFINITY(n) ASPECT_AFFINITY_##n
#define _SUBSYS_NAME(n) SUBSYS_NAME_##n
#define _SUBSYS_DESC(n) SUBSYS_DESC_##n
#define _SUBSYS_REQUIRES(n) SUBSYS_REQUIRES_##n

#define ASPECT_AFFINITY(n) _ASPECT_AFFINITY(STRUCTURE_SUBTYPE_##n)
#define SUBSYS_NAME(n) _SUBSYS_NAME(STRUCTURE_SUBTYPE_##n)
#define SUBSYS_DESC(n) _SUBSYS_DESC(STRUCTURE_SUBTYPE_##n)
#define SUBSYS_REQUIRES(n) _SUBSYS_REQUIRES(STRUCTURE_SUBTYPE_##n)

#define	STRUCTURE_SUBTYPE_PRIME 		0
#define ASPECT_AFFINITY_0 ASPECT_NONE
#define SUBSYS_NAME_0 "PRIME"
#define SUBSYS_DESC_0 "This object represents the structure itself, and is the first object created." \
  "It performs the structure bootstrap operations and is the template used to create all the other " \
  "structure subtypes. It oversees the activites of all other subsystems, fullfilling the role of " \
  "administrator. Destroying this object requires PRIV_STRUCTURE > 200, and will result in " \
  "an immediate abort of the application."

#define	STRUCTURE_SUBTYPE_APP 			1
#define ASPECT_AFFINITY_1 ASPECT_THREADS, 1, ASPECT_HOOKS, 1, ASPECT_REFCOUNTING, 1, \
    ASPECT_RECYCLING, 1, ASPECT_TRANSFORMS, 1, ASPECT_NONE
#define SUBSYS_NAME_1 "APP"
#define SUBSYS_DESC_1 "Structure subtype which represents an application - ie process. Its state " \
  "reflects the state of the application overall, and it can be used to manage and control the " \
  "application process itself"
#define SUBSYS_REQUIRES_1 STRUCTURE_SUBTYPE_PRIME

#define STRUCTURE_SUBTYPE_GUI			2
#define ASPECT_AFFINITY_2 ASPECT_NONE
#define SUBSYS_REQUIRES_2 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_2 "GUI"
#define SUBSYS_DESC_2 "GUI is an expert system designed to manage the user interface elements, " \
  "essential for any application which has a graphical front end."

#define	STRUCTURE_SUBTYPE_THREAD_HERDER		3
#define ASPECT_AFFINITY_3 ASPECT_THREADS, 100, ASPECT_NONE
#define SUBSYS_REQUIRES_3 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_3 "THREAD_HERDER"
#define SUBSYS_DESC_3 "Thread_herder is an expert subsytem designed to, depending on its " \
  "level of automation, manage and monitor native threads and thread instances created from these," \
  "as well as interfacing thread_instances with transform status, trajectory segments, " \
  "hook triggers and callbacks, in short acting as liason between thereads and the rest of the " \
  "structure."

#define	STRUCTURE_SUBTYPE_AUTOMATION		4
#define ASPECT_AFFINITY_4 ASPECT_THREADS, 50, ASPECT_HOOKS, 100, ASPECT_REFCOUNTING, 50, \
    ASPECT_RECYCLING, 50, ASPECT_TRANSFORMS, 50, ASPECT_NONE
#define SUBSYS_REQUIRES_4 STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, \
    STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_4 "AUTOMATION"
#define SUBSYS_DESC_4 "Automation is an expert system which can be programmed to carry out tasks " \
  "according to triggers and conditions. Automations can be used for example to add hook " \
  "callbacks when a bundle or item is created, to perform actions on hook triggers, or to " \
  "check conditions at specific points. The trigger is normally a hook callback or a structure " \
  "transform, conditions are a COND_CHECK script or a function call, and actions may be to call a " \
  "structure transform or run a Scriptlet. Depending on the levels of automation selected, the " \
  "the subsystem may trigger hooks, activate hook callbacks, and mange reference counting."

#define	STRUCTURE_SUBTYPE_BROKER		5
#define ASPECT_AFFINITY_5 ASPECT_TRANSFORMS, 50, ASPECT_NONE
#define SUBSYS_REQUIRES_5 STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, \
    STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_5 "BROKER"
#define SUBSYS_DESC_5 "Broker is an expert system designed to match intentcaps and requirements with " \
  "contracts, and thus with the objects holding those contracts. Broker will thus maintain a " \
  "an index of all contracts, the intencaps they satisfy, their side effects. Iin addition it will " \
  "keep a database of the attributes of each object type / subtype. Given an intentcap to satisfy, " \
  "broker will map out sequences of transforms to be actioned, possibly providing multiple solutions" \
  "In the case where no solution can be found, broker may indicate the missing steps, as well as " \
  "providing the the nearest alternative resolutions"

#define STRUCTURE_SUBTYPE_LIFECYCLE		6
#define ASPECT_AFFINITY_6 ASPECT_RECYCLING, 100, ASPECT_REFCOUNTING, 100, ASPECT_NONE
#define SUBSYS_REQUIRES_6 STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, \
    STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_6 "LIFECYCLE"
#define SUBSYS_DESC_6 "This will be an expert system designed to track and monitor the states "	\
  "of objects from the monet they are created until they are no longer required. Objects which " \
  "idle for too long may be recycled and their threads redeployed. Objects which lock up system " \
  "resources unnecessarily may be flagged. Runaway objects which create too many instances or " \
  "use up thereads unnecesarily will be flagged and blocked if necessary"

#define	STRUCTURE_SUBTYPE_ARBITRATOR		7
#define ASPECT_AFFINITY_7 ASPECT_NONE
#define SUBSYS_REQUIRES_7 SRTUCTURE_SUBTYPE_AUTOMATION, STRUCTURE_SUBTYPE_THREAD_HERDER, \
    STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_7 "ARBITRATOR"
#define SUBSYS_DESC_7 "Arbitrator will be an expert system specifally designed fo resolving " \
  "failed condition checks. It will use a vareity of methods depending on the nature of the " \
  "check, as well as the condition itself. For example if the check is rqueirements check " \
  "for a contract, this may be passed on to NEGOTIATOR to resolve."

#define	STRUCTURE_SUBTYPE_NEGOTIATOR		8
#define ASPECT_AFFINITY_8 ASPETCT_TRANSFORMS, 100, ASPECT_NONE
#define SUBSYS_REQUIRES_8 STRUCTURE_SUBTYPE_ARBITRATOR, STRUCTURE_SUBTYPE_BROKER, \
    STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_8 "NEGOTIATOR"
#define SUBSYS_DESC_8 "Negotiator will be an expert system specifially designed to resolve " \
  "failed conditions checks realting to a contract. Depending on the particular condition it may " \
  "search for a means to provide missing attributes, to run parallel transforms to supply streamed " \
  "data, or action pre-transforms to bring objectsto the correct state / subtype"

#define	STRUCTURE_SUBTYPE_ADJUDICATOR		9
#define ASPECT_AFFINITY_9 ASPECT_NONE
#define SUBSYS_REQUIRES_9 SRTUCTURE_SUBTYPE_ARBITRATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_9 "ADJUDICATOR"
#define SUBSYS_DESC_9 "Adjudicator will be an expert system designed to flag and possibly remove " \
  "anything from the application which may cause errors or timeouts."

#define STRUCTURE_SUBTYPE_CHRONOMETRY			10
#define ASPECT_AFFINITY_10 ASPECT_NONE
#define SUBSYS_REQUIRES_10 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_10 "CHRONOMETRY"
#define SUBSYS_DESC_10 "Chronometry will be an expert system desinged to handle features such as " \
  "high precision timeing, thread synchronisation, time realted instrumentation, as well as " \
  "mainatinaing queues for actioning events at specific intervals or absolute times"

#define	STRUCTURE_SUBTYPE_ARCHIVER		11
#define ASPECT_AFFINITY_11 ASPECT_NONE
#define SUBSYS_REQUIRES_11 STRUCTURE_SUBTYPE_LIFECYCLE, STRUCTURE_SUBTYPE_BROKER, \
    STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_11 "ARCHIVER"
#define SUBSYS_DESC_11 "Archiver will be an expert subsystem designed specifially for rapid data " \
  "storage, indexing and retrieval. It will have features for dealing with arrays, lists, " \
  "hash tables, lookup tables as well as managing offline stroage, backups and restores, " \
  "crash recovery and so on"

#define	STRUCTURE_SUBTYPE_OPTIMISATION 		12
#define ASPECT_AFFINITY_12 ASPECT_NONE
#define SUBSYS_REQUIRES_12 STRUCTURE_SUBTYPE_CHRONOMETRY, STRUCTURE_SUBTYPE_THREAD_HERDER, \
    STRUCTURE_SUBTYPE_BROKER, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_12 "OPTIMISATION"
#define SUBSYS_DESC_12 "This will be an expert system designed to montor the application and " \
  "overall system state, optimising performance"

#define	STRUCTURE_SUBTYPE_SECURITY		13
#define ASPECT_AFFINITY_13 ASPECT_NONE
#define SUBSYS_REQUIRES_13 STRUCTURE_SUBTYPE_ADJUDICATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_13 "SECURITY"
#define SUBSYS_DESC_13 "This subsystem will be an expert system designed to manage credentials and " \
  "privilege levels to ensure the integrity of the application and the environment it runs in"

#define	STRUCTURE_SUBTYPE_HARMONY		14
#define ASPECT_AFFINITY_14 ASPECT_NONE
#define SUBSYS_REQUIRES_14 STRUCTURE_SUBTYPE_ADJUDICATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_14 "HARMONY"
#define SUBSYS_DESC_14 "This is an expert system desing to interface with the underlying operating " \
  "system, as well as interacting with exernal systems, applications and Oracles"

#define	N_STRUCTURE_SUBTYPES 15

// this subtype cannot be created but can be discovered. An Oracle is able
// to supply missing contract
// attribute values, provided they are of simple types (non array, non binary data).
// end users are a type of oracle, the user prefs instance may be able to help constuct a
// more accurat model

#define STRUCTURE_SUBTYPE_ORACLE 128
#define ASPECT_AFFINITY_128 ASPECT_NONE
#define SUBSYS_REQUIRES_128 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_128 "ORACLE"
#define SUBSYS_DESC_128 "If you are reading this, this is you."

#endif

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
