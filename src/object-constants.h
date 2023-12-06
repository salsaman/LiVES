// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef __cplusplus
extern "C"
{
#endif

// ATTR_MAPS, CONDITIONS, CALLBACKS, CONDLOGIC

//#define DEBUG_BUNDLE_MAKER

// the implementation should define its own NIRVA_BUNDLE_T before this header
#ifndef NIRVA_BUNDLEPTR_T
#error NIRVA_BUNDLEPTR_T MUST BE DEFINED
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

#if NIRVA_IMPL_IS(DEFAULT_C)
#ifndef NO_STD_INCLUDES
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#endif

#define __IMPL_TYPEDEF__ typedef
#define __IMPL_LINE_END__ ;
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
#define __IMPL_BLOCK_START__ {
#define __IMPL_BLOCK_END__ }

#define NIRVA_XENUM(...)__IMPL_ENUM__ __IMPL_BLOCK_START__ __VA_ARGS__, __IMPL_BLOCK_END__

#define NIRVA_CMD(a)a __IMPL_LINE_END__
#define NIRVA_TYPED(a,...)__IMPL_TYPEDEF__ a __VA_ARGS__
#define NIRVA_TYPEDEF(a,...)NIRVA_CMD(NIRVA_TYPED(a,__VA_ARGS__))
#define NIRVA_TYPEDEF_ENUM(typename,...)NIRVA_CMD(__IMPL_TYPEDEF__ NIRVA_XENUM(__VA_ARGS__) typename)

#define __IMPL_GENPTR__ void *
#define __IMPL_DEF_ARRAY_OF__(a)a*

NIRVA_TYPEDEF(__IMPL_GENPTR__, NIRVA_GENPTR)

#ifndef NIRVA_BOOLEAN
#define NIRVA_BOOLEAN int
#endif
#define NIRVA_SIZE size_t
#define NIRVA_INT int
#define NIRVA_UINT uint32_t
#define NIRVA_INT64 int64_t
#define NIRVA_UINT64 uint64_t
#define NIRVA_STRING_T char *
NIRVA_TYPEDEF(NIRVA_STRING_T, NIRVA_STRING)
#define NIRVA_DOUBLE double
#define NIRVA_FLOAT float
#define NIRVA_VOIDPTR_T void *
NIRVA_TYPEDEF(NIRVA_VOIDPTR_T, NIRVA_VOIDPTR)
#define NIRVA_VARIADIC ...
#define NIRVA_VA_LIST va_list
#define NIRVA_VA_START(a,b)NIRVA_CMD(__IMPL_VA_START__(a,b))
#define NIRVA_VA_END(a)NIRVA_CMD(__IMPL_VA_END__(a))
#define NIRVA_VA_ARG(a,b)__IMPL_VA_ARG__(a,b)
#define NIRVA_NULL __IMPL_NULL__
#define NIRVA_ENUM(...)NIRVA_CMD(NIRVA_XENUM(__VA_ARGS__))
#define NIRVA_CONST __IMPL_CONST__
#define NIRVA_STATIC __IMPL_FN_STATIC__
#define NIRVA_STATIC_INLINE __IMPL_STATIC_INLINE__
#define NIRVA_EXTERN __IMPL_EXTERN__
#define NIRVA_NO_RETURN __IMPL_NO_VAL__
#define NIRVA_VOID __IMPL_NO_VAL__

#define NIRVA_EQUAL(a,b)(a==b)
#define NIRVA_FUNC_TYPE_DEF(ret_type,funcname,...)NIRVA_CMD(__IMPL_TYPEDEF__ ret_type \
							    (* funcname)(__VA_ARGS__))

#define NIRVA_ARRAY_OF __IMPL_DEF_ARRAY_OF__
#define NIRVA_PTR_TO __IMPL_PTR_TO_TYPE__

NIRVA_FUNC_TYPE_DEF(NIRVA_NO_RETURN, nirva_native_function_t,)
#define NIRVA_NATIVE_FUNC nirva_native_function_t

#endif // C style

////// function return codes ////
// one or more call parameters was invalid
#define NIRVA_RESULT_PARAM_INVALID	-3
// invalid function or macro call
#define NIRVA_RESULT_CALL_INVALID	-2
// error occurred whilst evaluating
#define NIRVA_RESULT_ERROR 		-1
#define NIRVA_RESULT_FAIL 		0
#define NIRVA_RESULT_SUCCESS 		1

// values for NIRVA_BOOLEAN
#define NIRVA_FALSE 0
#define NIRVA_TRUE 1

// condition_check results

// indicates an invalid / empty cond
#define NIRVA_COND_INVALID		-2
// error occured while evalutaing a condition
#define NIRVA_COND_ERROR		-1
// condition failed
#define NIRVA_COND_FAIL			0
// condition succeeded
#define NIRVA_COND_SUCCESS		1
// request to give more time and retry. If no other conditions fail then cond_once may
// emulate cond_retry
#define NIRVA_COND_WAIT_RETRY		2

// these values should only be returned from system callbacks
// force conditions to succeed, even if some others would fail
// takes precedence over all other return codes, including NIRV_COND_ERROR
//exit and return NIRVA_COND_SUCCESS
#define NIRVA_COND_FORCE		16
// force condition fail, exit and do not retry
#define NIRVA_COND_ABANDON		17
/////

// responses to nirva_request hooks

#define NIRVA_REQUEST_INVALID		NIRVA_COND_INVALID
#define NIRVA_REQUEST_ERROR		NIRVA_COND_ERROR

#define NIRVA_REQUEST_NO		NIRVA_COND_FAIL
#define NIRVA_REQUEST_YES		NIRVA_COND_SUCCESS
// cannot be responded to immediately, response will be received via hook callback
#define NIRVA_REQUEST_WAIT		NIRVA_COND_WAIT_RETRY
// security check failed, request is denied
#define NIRVA_REQUEST_NEEDS_PRIVELEGE  	NIRVA_COND_ABANDON

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
// instances are programmed to produce one specific bundle_type
// (except for object_templates, which are produced by plugins, and object_instances, which are produced by object_templates)
// so to produce a new bundle, find its producer, and action the contract, attributes will correspond to mand / opt strands
// and output is a shiny new bundle. Some factories can produce a stream of bundles (sequential output)
#define OBJECT_TYPE_BUNDLEMAKER		IMkType("obj.BMKR")
//
#define OBJECT_TYPE_CLIP		IMkType("obj.CLIP") // media clip
#define OBJECT_TYPE_PLUGIN		IMkType("obj.PLUG")
#define OBJECT_TYPE_WIDGET		IMkType("obj.WDGT")
#define OBJECT_TYPE_THREAD		IMkType("obj.THRD")
#define OBJECT_TYPE_DICTIONARY		IMkType("obj.DICT")

#define OBJECT_TYPE_UNDEFNED 0
#define OBJECT_TYPE_ANY 0

#define OBJECT_SUBTYPE_UNDEFINED 0
#define OBJECT_SUBTYPE_ANY 0
#define OBJECT_SUBTYPE_NONE 0

#define NO_SUBTYPE 0

#define OBJ_INTENTION_NONE 0

// INTENTIONS
NIRVA_ENUM
(
  // some common intentions
  // internal or (possibly) non-functional types
  OBJ_INTENTION_UNKNOWN,

  // application types
  OBJ_INTENTION_NOTHING = OBJ_INTENTION_NONE,

  // passive intents

  // transform which creates a new bundle.
  // - Find an object of type blueprint_factory, subtype according to bundle type
  // get the contract, then action it.
  // For object instances, will require input of an object template of same type (CREATE_INSTANCE)
  // or another instance of same type / subtype (COPY_INSTANCE)
  //
  // (will trigger init_hook on the new instance)
  OBJ_INTENTION_CREATE_BUNDLE = 0x00000100, // create instance of type / subtype

  // there is a single passive intent for instances - actioning this
  // no negotiate, mandatory intent will trigger the corresponding request hook
  // in the target. This may be called for a strand, or for an object attribute
  // (target determined by CAPS)
  OBJ_INTENTION_REQUEST_UPDATE,

  // active intents - there is only 1 active intent, which can be further deliniated by
  // the hoks required / provided:
  // the CAPS and atributes futher delineate these

  // a transform which takes data input and produces data output
  OBJ_INTENTION_PROCESS,

  //
  // the following are "synthetic" intents formed by a combination of factors

  // transform which changes the state of an instance (either self or another instance)
  // will trigger object state change hook- This is simply request_update called with target "STATE"
  OBJ_INTENTION_CHANGE_STATE,

  // transform which changes the subtype of an instance (either self or another instance)
  // will trigger object config_changed hook. This is simply request_update called with target "SUBTYPE.
  OBJ_INTENTION_CHANGE_SUBTYPE,

  // an intention which takes one or more sequential attributes and produces output
  // either as another sequential attribute or the same one
  // - PLAY is based on this, with CAP realtime
  //  (this is INTENTION_PROCESS with input and output sequential attrs)
  OBJ_INTENTION_MANIPULATE_SEQUENCE,

  // an intent which takes sequential input and produces array output
  OBJ_INTENTION_RECORD,  // record

  // intent INTENTION_PROCESS takes array input
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
  OBJ_INTENTION_DESTROY_INSTANCE = 0x00002000,

  OBJ_INTENTION_FIRST_CUSTOM = 0x80000000,
  OBJ_INTENTION_MAX = 0xFFFFFFFF)

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
// object is static. It is always safe to reference via a pointer to it
// can apply to templates as well as instances
#define NIRVA_OBJ_FLAG_STATIC 			(1ull << 0)

// object instance should only be created once as it has some application wide globals
// this implies either it is a template which produces no instances
// or else it is an instance created by a template and only a single instance may be instantiated
// at any one time
#define NIRVA_OBJ_FLAG_SINGLETON 		(1ull << 1)

// do not create an object template, instead instances should be created directly
// this is for trivial objects that dont need tracking by the broker
// in this case the object can built directly from an object_instance bundle
// there will be an attr bundle supplied in the template which should be created in each instance
#define NIRVA_OBJ_FLAG_INSTANCES_ONLY 	       	(1ull << 2)

// indicates that a (native) thread should be assigned to run the instance
// responding to hook callbacks
// communicatuon is via request hooks and status change hooks
#define NIRVA_OBJ_FLAG_ACTIVE 	     	  		(1ull << 4)

// indicates that the object has been destroyed, (DESTRUCTION_HOOK has been triggerd),
// but the 'shell' is left due to being ref counted

// see Developer Docs for more details


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

// object is updating its internal state
#define OBJECT_STATE_UPDATING		5

// some async transformss may cause the state to change to this temporarily
// an object may spontaneously change to this if doing internal updates / reads etc.
#define OBJECT_STATE_BUSY		6

#define OBJECT_STATE_ZOMBIE 	       	32

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

#define CAP_SELECTOR(cname) "%s", "CAP_SELECT", "%s", #cname, _COND_START
#define CAP_SELECTOR_END _COND_END

#define NIRVA_ROAST_INTENTCAP(intent, ...) make_bundledef(0,0,MSTY("%d",intent),MS( __VA_ARGS__))

// these are almost identical to strand value restrictions, however the start and
// end keywords differ
#define _CAP_BUNDLE_TYPE(btype) COND_VAL_EQUALS, VAR_STRAND_VAL, "BLUEPRINT/BUNDLE_TYPE", \
    _CONST_UINT64_VAL, btype##_BUNDLE_TYPE
#define CAP_BUNDLE_TYPE(btype) MSTY(CAP_SELECTOR(bundle_type), _CAP_BUNDLE_TYPE(btype), CAP_SELECTOR_END)

#define _CAP_OBJECT_TYPE(otype) _CAP_BUNDLE_TYPE(OBJECT_TEMPLATE), COND_LOGIC_AND, \
    COND_VAL_EQUALS, VAR_STRAND_VAL, "TYPE",  _CONST_UINT64_VAL, OBJECT_TYPE_##otype
#define CAP_OBJECT_TYPE(otype) MSTY(CAP_SELECTOR(object_type), _CAP_OBJECT_TYPE(otype), CAP_SELECTOR_END)

#define _CAP_INSTANCE_TYPE_SUBTYPE(itype, isubtype) _CAP_BUNDLE_TYPE(OBJECT_INSTANCE), COND_LOGIC_AND, \
    COND_VAL_EQUALS, VAR_STRAND_VAL, "TYPE", _CONST_UINT64_VAL, OBJECT_TYPE_##itype, \
    COND_LOGIC_AND, COND_VAL_EQUALS, VAR_STRAND_VAL, "SUBTYPE", \
    _CONST_UINT64_VAL, OBJECT_SUBTYPE_##isubtype
#define CAP_INSTANCE_TYPE_SUBTYPE(itype, isubtype) \
  MSTY(CAP_SELECTOR(instance_type_subtype), _CAP_INSTANCE_TYPE_SUBTYPE(itype, isubtype), CAP_SELECTOR_END)

#define CAPS_AND(a, b) ((a) && (b))
#define CAPS_OR(a, b) ((a) || (b))
#define CAPS_XOR(a, b) (CAPS_AND(CAPS_OR(a, b) && !CAPS_AND(a, b)))

// composite icaps (TBD)
NIRVA_ENUM(_ICAP_IDLE = 0, _ICAP_DOWNLOAD, _ICAP_LOAD, N_STD_ICAPS)

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

// a variable strand - there is no built in default for this strand. In the STRAND_DEF, the strand_type will be set to PROXIED
// and there will be a strand name for the proxy holding the value
#define _STRAND_TYPE_VARIABLE		       		"*"

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

// this is an implementation defined type, eg. it can be a  void * or void * array,
// or it can be euqal to a bundleptr (as defined in the implementation)
// for external bundle(s), should NOT be unreffed / freed
// when the bundle containing this pointer is freed
// other than the (virtual) type, the difference in this case is that "CONTAINER" does not point to the parent bundle
#define _STRAND_TYPE_CONST_BUNDLEPTR	       		"C"

// can be set to either "B" or "C"
// this is STRAND_TYPE_PROXY,
#define _STRAND_TYPE_BUNDLEPTR_OPT_CONST	 	"O"

// type flags

// 0 or 1 of these flag values can precede the type letter
#define _STRAND_TYPE_FLAG_COMMENT	       		"#"
#define _STRAND_TYPE_FLAG_DIRECTIVE	       		"@"
#define _STRAND_TYPE_FLAG_OPTIONAL	       		"?"

////////////////////////////

///////// strand types ////////

// non-standard types
#define STRAND_TYPE_NONE	       			(uint32_t)0
#define STRAND_TYPE_UNDEFINED	       			(uint32_t)'X' // proxied, but not defined
#define STRAND_TYPE_DEFAULT	       			(uint32_t)'-'	// use default strand_type
#define STRAND_TYPE_INVALID	       			(uint32_t)'!'	// invalid

#define STRAND_TYPE_PROXIED	       			(uint32_t)'P'	// strand_type is held in proxy strand
#define STRAND_TYPE_VARIABLE				STRAND_TYPE_PROXIED

// flag bits
#define STRAND_TYPE_FLAG_OPTIONAL      	      		(uint32_t)'?'	// optional strand
#define STRAND_TYPE_FLAG_COMMENT       	      		(uint32_t)'#'	// comment
#define STRAND_TYPE_FLAG_DIRECTIVE			(uint32_t)'@'	// directive

#define STRAND_TYPE_INT					(uint32_t)'i'	// 4 byte int
#define STRAND_TYPE_DOUBLE				(uint32_t)'d'	// 8 byte float
#define STRAND_TYPE_BOOLEAN				(uint32_t)'b'	// 1 - 4 byte int

#define STRAND_TYPE_STRING				(uint32_t)'s'	// \0 terminated string
#define STRAND_TYPE_CONST_STRING	       		(uint32_t)'S'	// this is only used as a return type from nat. fn

#define STRAND_TYPE_INT64              		     	(uint32_t)'I'	// 8 byte int
#define STRAND_TYPE_UINT	       			(uint32_t)'u'	// 4 byte int
#define STRAND_TYPE_UINT64				(uint32_t)'U'	// 8 byte int

#define STRAND_TYPE_VOIDPTR				(uint32_t)'V'	// void *
#define STRAND_TYPE_FUNCPTR				(uint32_t)'F'	// pointer to function

// void * aliases
#define STRAND_TYPE_BUNDLEPTR	       			(uint32_t)'B' // pointer to included sub bundle
#define STRAND_TYPE_CONST_BUNDLEPTR           		(uint32_t)'C' // implementation type for extern bundleptr
#define STRAND_TYPE_BUNDLEPTR_OPT_CONST        		(uint32_t)'O' // can be set to either 'C' or 'B'

#if NIRVA_IMPL_IS(DEFAULT_C)
#define STRAND_TYPE_GENPTR STRAND_TYPE_VOIDPTR
#endif

#ifndef STRAND_TYPE_FUNC_RETURN
#define STRAND_TYPE_FUNC_RETURN STRAND_TYPE_INT64
#endif

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

#define FOR_ALL_DOMAINS(MACROX, ...) MACROX(GENERIC), MACROX(INTROSPECTION)
// domain SPEC
#define STRAND_SPEC_VERSION				STRAND_NAME("SPEC", "VERSION")
#define STRAND_SPEC_VERSION_TYPE              		INT, 100

#define ALL_STRANDS_SPEC "VERSION"
#define ALL_BUNDLES_SPEC

// domain BLUEPRINT

#define BUNDLE_BLUEPRINT_AUTOMATIONS		      	STRAND_NAME("BLUEPRINT", "AUTOMATIONS")
#define BUNDLE_BLUEPRINT_AUTOMATIONS_TYPE      	       	SCRIPTLET, NULL

#define STRAND_BLUEPRINT_BUNDLE_TYPE	       		STRAND_NAME("BLUEPRINT", "BUNDLE_TYPE")
#define STRAND_BLUEPRINT_BUNDLE_TYPE_TYPE      	       	UINT64, NULL

#define STRAND_BLUEPRINT_CREATES_TYPE	       		STRAND_NAME("BLUEPRINT", "CREATES_TYPE")
#define STRAND_BLUEPRINT_CREATES_TYPE_TYPE            	UINT64, NULL

#define BUNDLE_BLUEPRINT_STRAND_DEFS		      	STRAND_NAME("BLUEPRINT", "STRAND_DEFS")
#define BUNDLE_BLUEPRINT_STRAND_DEFS_TYPE      	       	STRAND_DEF, NULL

// domain GENERIC

// readonly
#define STRAND_GENERIC_NAME				STRAND_NAME("GENERIC", "NAME")
#define STRAND_GENERIC_NAME_TYPE              		STRING, NULL

#define STRAND_GENERIC_FLAGS				STRAND_NAME("GENERIC", "FLAGS")
#define STRAND_GENERIC_FLAGS_TYPE              		UINT64, 0

// readonly
#define STRAND_GENERIC_UID				STRAND_NAME("GENERIC", "UID")
#define STRAND_GENERIC_UID_TYPE				UINT64, 0

#define STRAND_GENERIC_DESCRIPTION			STRAND_NAME("GENERIC", "DESCRIPTION")
#define STRAND_GENERIC_DESCRIPTION_TYPE              	STRING, NULL

#define ALL_STRANDS_GENERIC "UID", "DESCRIPTION", "FLAGS", "NAME"
#define ALL_BUNDLES_GENERIC

//// domain FRAMEWORK
#define STRAND_FRAMEWORK_OWNER				STRAND_NAME("FRAMEWORK", "OWNER")
#define STRAND_FRAMEWORK_OWNER_TYPE			UINT64, 0

#define STRAND_FRAMEWORK_CONTAINER			STRAND_NAME("FRAMEWORK", "CONTAINER")
#define STRAND_FRAMEWORK_CONTAINER_TYPE			CONST_BUNDLEPTR, NULL

#define STRAND_FRAMEWORK_CONTAINER_STRAND		STRAND_NAME("FRAMEWORK", "CONTAINER_STRAND")
#define STRAND_FRAMEWORK_CONTAINER_STRAND_TYPE	       	STRING, NULL

//// domain STANDARD - some standard bundle types

#define BUNDLE_STANDARD_BLUEPRINT			STRAND_NAME("STANDARD", "BLUEPRINT")
#define BUNDLE_STANDARD_BLUEPRINT_TYPE       		BLUEPRINT, NULL

#define BUNDLE_STANDARD_STRAND_DEF	       		STRAND_NAME("STANDARD", "STRAND_DEF")
#define BUNDLE_STANDARD_STRAND_DEF_TYPE			STRAND_DEF, NULL

#define BUNDLE_STANDARD_ATTRIBUTE	       		STRAND_NAME("STANDARD", "ATTRIBUTE")
#define BUNDLE_STANDARD_ATTRIBUTE_TYPE			ATTRIBUTE, NULL

#define BUNDLE_STANDARD_ATTR_GROUP			STRAND_NAME("STANDARD", "ATTR_GROUP")
#define BUNDLE_STANDARD_ATTR_GROUP_TYPE            	ATTR_GROUP, NULL

#define BUNDLE_STANDARD_ATTR_DEF	       		STRAND_NAME("STANDARD", "ATTR_DEF")
#define BUNDLE_STANDARD_ATTR_DEF_TYPE			ATTR_DEF, NULL

#define BUNDLE_STANDARD_ATTR_DEF_GROUP			STRAND_NAME("STANDARD", "ATTR_DEF_GROUP")
#define BUNDLE_STANDARD_ATTR_DEF_GROUP_TYPE            	ATTR_DEF_GROUP, NULL

#define BUNDLE_STANDARD_ERROR				STRAND_NAME("STANDARD", "ERROR")
#define BUNDLE_STANDARD_ERROR_TYPE       		ERROR, NULL

#define BUNDLE_STANDARD_EMISSION			STRAND_NAME("STANDARD", "EMISSION")
#define BUNDLE_STANDARD_EMISSION_TYPE       		EMISSION, NULL

#define BUNDLE_STANDARD_SCRIPTLET		       	STRAND_NAME("STANDARD", "SCRIPTLET")
#define BUNDLE_STANDARD_SCRIPTLET_TYPE       		SCRIPTLET, NULL

#define BUNDLE_STANDARD_CONDLOGIC_NODE	       		STRAND_NAME("STANDARD", "CONDLOGIC_NODE")
#define BUNDLE_STANDARD_CONDLOGIC_NODE_TYPE	       	CONDLOGIC_NODE, NULL

#define BUNDLE_STANDARD_SELECTOR             		STRAND_NAME("STANDARD", "SELECTOR")
#define BUNDLE_STANDARD_SELECTOR_TYPE			SELECTOR, NULL

#define BUNDLE_STANDARD_LOCATOR             		STRAND_NAME("STANDARD", "LOCATOR")
#define BUNDLE_STANDARD_LOCATOR_TYPE			LOCATOR, NULL

#define BUNDLE_STANDARD_TRAJECTORY             		STRAND_NAME("STANDARD", "TRAJECTORY")
#define BUNDLE_STANDARD_TRAJECTORY_TYPE			TRAJECTORY, NULL

#define BUNDLE_STANDARD_CASCADE             		STRAND_NAME("STANDARD", "CASCADE")
#define BUNDLE_STANDARD_CASCADE_TYPE			CASCADE, NULL

#define BUNDLE_STANDARD_REQUEST             		STRAND_NAME("STANDARD", "REQUEST")
#define BUNDLE_STANDARD_REQUEST_TYPE			REQUEST, NULL

#define BUNDLE_STANDARD_INDEX             		STRAND_NAME("STANDARD", "INDEX")
#define BUNDLE_STANDARD_INDEX_TYPE			INDEX, NULL

#define BUNDLE_STANDARD_TRANSFORM             		STRAND_NAME("STANDARD", "TRANSFORM")
#define BUNDLE_STANDARD_TRANSFORM_TYPE			TRANSFORM, NULL

#define BUNDLE_STANDARD_SEGMENT             		STRAND_NAME("STANDARD", "SEGMENT")
#define BUNDLE_STANDARD_SEGMENT_TYPE			SEGMENT, NULL

#define BUNDLE_STANDARD_ATTRIBUTE             		STRAND_NAME("STANDARD", "ATTRIBUTE")
#define BUNDLE_STANDARD_ATTRIBUTE_TYPE			ATTRIBUTE, NULL

#define BUNDLE_STANDARD_FUNCTIONAL             		STRAND_NAME("STANDARD", "FUNCTIONAL")
#define BUNDLE_STANDARD_FUNCTIONAL_TYPE			FUNCTIONAL, NULL

#define BUNDLE_STANDARD_HOOK_STACK             		STRAND_NAME("STANDARD", "HOOK_STACK")
#define BUNDLE_STANDARD_HOOK_STACK_TYPE			HOOK_STACK, NULL

#define BUNDLE_STANDARD_VALUE_CHANGE           		STRAND_NAME("STANDARD", "VALUE_CHANGE")
#define BUNDLE_STANDARD_VALUE_CHANGE_TYPE		VALUE_CHANGE, NULL

#define BUNDLE_STANDARD_MATRIX_2D             		STRAND_NAME("STANDARD", "MATRIX_2D")
#define BUNDLE_STANDARD_MATRIX_2D_TYPE			MATRIX_2D, NULL

/// domain VALUE - values are common to both attributes and strands

#define STRAND_VALUE_DATA	               	       	STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_DATA_TYPE	      			VARIABLE,

#define STRAND_VALUE_ANY				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_ANY_TYPE			       	VARIABLE,

#define BUNDLE_VALUE_ANY_BUNDLE				STRAND_NAME("VALUE", "ANY_BUNDLE")
#define BUNDLE_VALUE_ANY_BNDLTE_TYPE			ANY,

#define BUNDLE_VALUE_PREV				STRAND_NAME("VALUE", "PREV")
#define BUNDLE_VALUE_PREV_TYPE				VALUE, NULL

#define BUNDLE_VALUE_NEXT				STRAND_NAME("VALUE", "NEXT")
#define BUNDLE_VALUE_NEXT_TYPE				VALUE, NULL

// readonly
#define STRAND_VALUE_STRAND_TYPE      			STRAND_NAME("VALUE", "STRAND_TYPE")
#define STRAND_VALUE_STRAND_TYPE_TYPE          		UINT, STRAND_TYPE_NONE

#define STRAND_VALUE_DEFAULT	      			STRAND_NAME("VALUE", "DEFAULT")
#define STRAND_VALUE_DEFAULT_TYPE      			VARIABLE,

#define STRAND_VALUE_INTEGER				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INTEGER_TYPE		       	INT, 0

#define STRAND_VALUE_INT				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INT_TYPE			       	INT, 0

#define STRAND_VALUE_UINT				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_UINT_TYPE			       	UINT, 0

#define STRAND_VALUE_DOUBLE				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_DOUBLE_TYPE		       	DOUBLE, 0.

#define STRAND_VALUE_BOOLEAN	      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_BOOLEAN_TYPE   	  	       	BOOLEAN, FALSE

#define STRAND_VALUE_STRING	      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_STRING_TYPE   	  	       	STRING, NULL

#define STRAND_VALUE_INT64		       		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_INT64_TYPE			       	INT64, 0

#define STRAND_VALUE_UINT64		       		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_UINT64_TYPE		       	UINT64, 0

#define STRAND_VALUE_VOIDPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_VOIDPTR_TYPE 	    	       	VOIDPTR, NULL

#define STRAND_VALUE_GENPTR 				STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_GENPTR_TYPE 	    	       	VOIDPTR, NULL

#define STRAND_VALUE_FUNCPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_FUNCPTR_TYPE 	    	       	FUNCPTR, NULL

#define STRAND_VALUE_BUNDLEPTR      			STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_BUNDLEPTR_TYPE 	    	       	BUNDLEPTR, NULL

#define STRAND_VALUE_CONST_BUNDLEPTR     		STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_CONST_BUNDLEPTR_TYPE   	       	CONST_BUNDLEPTR, NULL

#define STRAND_VALUE_BUNDLEPTR_OPT_CONST      	       	STRAND_NAME("VALUE", "DATA")
#define STRAND_VALUE_BUNDLEPTR_OPT_CONST_TYPE 	       	OPT_CONST_BUNDLEPTR, NULL

/////////////

#define STRAND_VALUE_ARRAY_SIZE                		STRAND_NAME("VALUE", "ARRAY_SIZE")
#define STRAND_VALUE_ARRAY_SIZE_TYPE                  	INT, 0

#define STRAND_VALUE_MAX_VALUES                		STRAND_NAME("VALUE", "MAX_VALUES")
#define STRAND_VALUE_MAX_VALUES_TYPE                  	INT, 0

// domain STDEF
#define STRAND_STDEF_TYPE_PROXY                		STRAND_NAME("STDEF", "TYPE_PROXY")
#define STRAND_STDEF_TYPE_PROXY_TYPE			STRING, STRAND_TYPE

#define STRAND_STDEF_RESTRICT_PROXY       		STRAND_NAME("STDEF", "RESTRICT_PROXY")
#define STRAND_STDEF_RESTRICT_PROXY_TYPE		STRING, RESTRICTIONS

// domain AUTOMATION

#define BUNDLE_AUTOMATION_CONDITIONS  	             	STRAND_NAME("AUTOMATION", "CONDITIONS")
#define BUNDLE_AUTOMATION_CONDITIONS_TYPE      		SCRIPTLET, NULL

#define STRAND_AUTOMATION_RESTRICTIONS                 	STRAND_NAME("AUTOMATION", "RESTRICTIONS")
#define STRAND_AUTOMATION_RESTRICTIONS_TYPE   		STRING, NULL

#define BUNDLE_AUTOMATION_SCRIPT                   	STRAND_NAME("AUTOMATION", "SCRIPT")
#define BUNDLE_AUTOMATION_SCRIPT_TYPE      		SCRIPTLET, NULL

// domain SRIPTLET

#define STRAND_SCRIPTLET_CATEGORY	      		STRAND_NAME("SCRIPTLET", "CATEGORY")
#define STRAND_SCRIPTLET_CATEGORY_TYPE	      		INT, 0

#define STRAND_SCRIPTLET_STRINGS	      		STRAND_NAME("SCRIPTLET", "STRINGS")
#define STRAND_SCRIPTLET_STRINGS_TYPE	      		STRING, NULL

#define STRAND_SCRIPTLET_MATURITY	      		STRAND_NAME("SCRIPTLET", "MATURITY")
#define STRAND_SCRIPTLET_MATURITY_TYPE	      		INT, SCRIPT_MATURITY_UNCHECKED

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

#define STRAND_DATETIME_CREATION_DATE                  	STRAND_NAME("DATETIME", "CREATION_DATE")
#define STRAND_DATETIME_CREATION_DATE_TYPE     		INT64, NULL

// domain CASCADE

#define STRAND_CASCADE_VALUE				STRAND_NAME("CASCADE", "VALUE")
#define STRAND_CASCADE_VALUE_TYPE    	      		VARIABLE,

#define STRAND_CASCADE_CATEGORY				STRAND_NAME("CASCADE", "CATEGORY")
#define STRAND_CASCADE_CATEGORY_TYPE   	      		INT, FUNC_CATEGORY_SELECTOR

#define STRAND_CASCADE_PROC_FUNC	       		STRAND_NAME("CASCADE", "PROC_FUNC")
#define STRAND_CASCADE_PROC_FUNC_TYPE          		STRING, NULL

#define BUNDLE_CASCADE_MATRIX_NODE			STRAND_NAME("CASCADE", "MATRIX_NODE")
#define BUNDLE_CASCADE_MATRIX_NODE_TYPE	      		CASCMATRIX_NODE, NULL

#define BUNDLE_CASCADE_MATRIX				STRAND_NAME("CASCADE", "MATRIX")
#define BUNDLE_CASCADE_MATRIX_TYPE	      		MATRIX_2D, NULL

#define BUNDLE_CASCADE_CONDLOGIC			STRAND_NAME("CASCADE", "CONDLOGIC")
#define BUNDLE_CASCADE_CONDLOGIC_TYPE			CONDLOGIC_NODE, NULL

#define STRAND_CONDLOGIC_WEIGHTING			STRAND_NAME("CONDLOGIC", "WEIGHTING")
#define STRAND_CONDLOGIC_WEIGHTING_TYPE			DOUBLE, 1.

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

#define STRAND_LOGIC_FITNESS       			STRAND_NAME("LOCIC", "FITNESS")
#define STRAND_LOGIC_FITNESS_TYPE	       	       	DOUBLE, 0.

#define STRAND_LOGIC_CORRELATION       			STRAND_NAME("LOCIC", "CORRELATION")
#define STRAND_LOGIC_CORRELATION_TYPE	       	       	DOUBLE, 0.

#define STRAND_LOGIC_WEIGHTING       			STRAND_NAME("LOCIC", "WEIGHTING")
#define STRAND_LOGIC_WEIGHTING_TYPE	       	       	DOUBLE, 1.

#define STRAND_LOGIC_PROBABILIY       			STRAND_NAME("LOCIC", "PROBABILITY")
#define STRAND_LOGIC_PROBABILITY_TYPE	       	       	DOUBLE, 0.

#define STRAND_LOGIC_CONFIDENCE       			STRAND_NAME("LOCIC", "CONFIDENCE")
#define STRAND_LOGIC_CONFIDENCE_TYPE	       	       	DOUBLE, 0.

#define BUNDLE_LOGIC_DECISION_NODE			STRAND_NAME("LOGIC", "DECISION_NODE")
#define BUNDLE_LOGIC_DECISION_NODE_TYPE      		CONDLOGIC_NODE, NULL

/// domain ATTRIBUTE

#define STRAND_ATTRIBUTE_MAX_VALUES			STRAND_NAME("ATTRIBUTE", "MAX_VALUES")
#define STRAND_ATTRIBUTE_MAX_VALUES_TYPE     	       	INT, -1

#define BUNDLE_ATTRIBUTE_DEFAULT   			STRAND_NAME("ATTRIBUTE", "DEFAULT")
#define BUNDLE_ATTRIBUTE_DEFAULT_TYPE     	       	VALUE, NULL

#define STRAND_ATTRIBUTE_NEW_DEFAULT   			STRAND_NAME("ATTRIBUTE", "NEW_DEFAULT")
#define STRAND_ATTRIBUTE_NEW_DEFAULT_TYPE     	       	VARIABLE,

#define STRAND_ATTRIBUTE_ATTR_TYPE	       		STRAND_NAME("ATTRIBUTE", "ATTR_TYPE")
#define STRAND_ATTRIBUTE_ATTR_TYPE_TYPE  		UINT, ATTR_TYPE_NONE

#define BUNDLE_ATTRIBUTE_CONNECTION	       		STRAND_NAME("ATTRIBUTE", "CONNECTION")
#define BUNDLE_ATTRIBUTE_CONNECTION_TYPE  		ATTR_CONNECTION, NULL

#define BUNDLE_ATTRIBUTE_HOOK_STACKS	       		STRAND_NAME("ATTRIBUTE", "HOOK_STACKS")
#define BUNDLE_ATTRIBUTE_HOOK_STACKS_TYPE  		HOOK_STACK, NULL

#define BUNDLE_ATTRMAP_ATTR_DEF		       		STRAND_NAME("ATTRMAP", "ATTR_DEF")
#define BUNDLE_ATTRMAP_ATTR_DEF_TYPE  			ATTR_DEF, NULL

#define STRAND_ATTRMAP_MAPPING		       		STRAND_NAME("ATTRMAP", "MAPPING")
#define STRAND_ATTRMAP_MAPPING_TYPE  			UINT64, NIRVA_ATTR_MAP_UNDEFINED

// domain REQUEST

#define STRAND_REQUEST_DETAILS         			STRAND_NAME("REQUEST", "DETAILS")
#define STRAND_REQUEST_DETAILS_TYPE		       	CONST_BUNDLEPTR, NULL

// domain EMISSION

#define STRAND_EMISSION_CALLER_UID	      		STRAND_NAME("EMISSION", "CALLER_UID")
#define STRAND_EMISSION_CALLER_UID_TYPE	    	   	UINT64, 0

#define STRAND_EMISSION_SOURCE_ITEM           		STRAND_NAME("EMISSION", "SOURCE_ITEM")
#define STRAND_EMISSION_SOURCE_ITEM_TYPE	       	STRING, NULL

#define STRAND_EMISSION_TARGET_ITEM	       		STRAND_NAME("EMISSION", "TARGET_ITEM")
#define STRAND_EMISSION_TARGET_ITEM_TYPE	      	STRING, NULL

// domain FUNCTIONAL

#define STRAND_FUNCTIONAL_CATEGORY	      		STRAND_NAME("FUNCTIONAL", "CATEGORY")
#define STRAND_FUNCTIONAL_CATEGORY_TYPE	      		INT, 0

#define STRAND_FUNCTIONAL_RESPONSES	       		STRAND_NAME("FUNCTIONAL", "RESPONSES")
#define STRAND_FUNCTIONAL_RESPONSES_TYPE       		INT64, 0

#define STRAND_FUNCTIONAL_FUNC_TYPE	      		STRAND_NAME("FUNCTIONAL", "FUNC_TYPE")
#define STRAND_FUNCTIONAL_FUNC_TYPE_TYPE   		INT, 0

#define STRAND_FUNCTIONAL_WRAPPING_TYPE	      		STRAND_NAME("FUNCTIONAL", "WRAPPING_TYPE")
#define STRAND_FUNCTIONAL_WRAPPING_TYPE_TYPE   		INT, 0

#define BUNDLE_FUNCTIONAL_FUNC_DATA	      		STRAND_NAME("FUNCTIONAL", "FUNC_DATA")
#define BUNDLE_FUNCTIONAL_FUNC_DATA_TYPE   		FUNC_DATA, NULL

#define BUNDLE_FUNCTIONAL_ATTR_MAPS	      		STRAND_NAME("FUNCTIONAL", "ATTR_MAPS")
#define BUNDLE_FUNCTIONAL_ATTR_MAPS_TYPE   		ATTR_MAP, NULL

#define BUNDLE_SEGMENT_ATTR_MAPS	      		STRAND_NAME("SEGMENT", "ATTR_MAPS")
#define BUNDLE_SEGMENT_ATTR_MAPS_TYPE   		ATTR_MAP, NULL

// domain THREADS
#define STRAND_THREADS_NATIVE_MUTEX	       		STRAND_NAME("THREADS", "NATIVE_MUTEX")
#define STRAND_THREADS_NATIVE_MUTEX_TYPE       		VOIDPTR, NULL

#define BUNDLE_THREADS_INSTANCE		       		STRAND_NAME("THREADS", "INSTANCE")
#define BUNDLE_THREADS_INSTANCE_TYPE       		OBJECT_INSTANCE, NULL

// domain INTROSPECTION
// item which should be added to every bundldef, it is designed to allow
// the bundle creator to store the pared down strands used to construct the bundle

// as an alternative this can be used instead to point to a static copy of the strands
#define BUNDLE_INTROSPECTION_REFCOUNTER       	       	STRAND_NAME("INTROSPECTION", "REFCOUNTER")
#define BUNDLE_INTROSPECTION_REFCOUNTER_TYPE		REFCOUNTER, NULL

#define STRAND_INTROSPECTION_REFCOUNT       	       	STRAND_NAME("INTROSPECTION", "REFCOUNT")
#define STRAND_INTROSPECTION_REFCOUNT_TYPE		INT, 0

#define STRAND_INTROSPECTION_COMMENT    	       	STRAND_NAME("INTROSPECTION", "COMMENT")
#define STRAND_INTROSPECTION_COMMENT_TYPE      	       	STRING, NULL

#define STRAND_INTROSPECTION_PRIVATE_DATA	       	STRAND_NAME("INTROSPECTION", "PRIVATE_DATA")
#define STRAND_INTROSPECTION_PRIVATE_DATA_TYPE		VOIDPTR, NULL

#define STRAND_INTROSPECTION_NATIVE_TYPE 	       	STRAND_NAME("INTROSPECTION", "NATIVE_TYPE")
#define STRAND_INTROSPECTION_NATIVE_TYPE_TYPE 	       	INT, 0

#define STRAND_INTROSPECTION_NATIVE_SIZE      	       	STRAND_NAME("INTROSPECTION", "NATIVE_SIZE")
#define STRAND_INTROSPECTION_NATIVE_SIZE_TYPE		UINT64, 0

#define ALL_STRANDS_INTROSPECTION "BLUEPRINT", "BLUEPRINT_PTR", "COMMENT", "REFCOUNTER", "PRIVATE_DATA", \
	  "NATIVE_PTR", "NATIVE_SIZE", "NATIVE_TYPE"
#define ALL_BUNDLES_INTROSPECTION "REFCOUNTER", "BLUEPRINT"

//////////// BUNDLE SPECIFIC STRANDS /////

#define STRAND_CONTRACT_VALID_CAPS			STRAND_NAME("CONTRACT", "VALID_CAPS")
#define STRAND_CONTRACT_VALID_CAPS_TYPE        		STRING, NULL

///// domain ICAP

#define STRAND_ICAP_INTENTION				STRAND_NAME("ICAP", "INTENTION")
#define STRAND_ICAP_INTENTION_TYPE             		INT, OBJ_INTENTION_NONE

#define BUNDLE_ICAP_CAPS				STRAND_NAME("ICAP", "CAPS")
#define BUNDLE_ICAP_CAPS_TYPE             		CAPS, NULL

////////// domain OBJECT

#define STRAND_OBJECT_TYPE				STRAND_NAME("OBJECT", "TYPE")
#define STRAND_OBJECT_TYPE_TYPE				UINT64, OBJECT_TYPE_UNDEFINED

#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS	       		STRAND_NAME("OBJECT", "ACTIVE_TRANSFORMS")
#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS_TYPE	       	OBJECT_INSTANCE, NULL

#define BUNDLE_OBJECT_ATTRIBUTES	       		STRAND_NAME("OBJECT", "ATTRIBUTES")
#define BUNDLE_OBJECT_ATTRIBUTES_TYPE	  	     	ATTR_GROUP, NULL

#define STRAND_OBJECT_STATE				STRAND_NAME("OBJECT", "STATE")
#define STRAND_OBJECT_STATE_TYPE	       		UINT64, OBJECT_STATE_UNDEFINED

#define BUNDLE_OBJECT_CONTRACTS	 	      		STRAND_NAME("OBJECT", "CONTRACTS")
#define BUNDLE_OBJECT_CONTRACTS_TYPE			CONTRACT, NULL

#define BUNDLE_OBJECT_HOOK_STACKS	               	STRAND_NAME("OBJECT", "HOOK_STACKS")
#define BUNDLE_OBJECT_HOOK_STACKS_TYPE      		HOOK_STACK, NULL

#define STRAND_INSTANCE_SUBTYPE				STRAND_NAME("INSTANCE", "SUBTYPE")
#define STRAND_INSTANCE_SUBTYPE_TYPE	       		UINT64, NO_SUBTYPE

///// domain HOOK

// this is one of the four basic patterns
#define STRAND_HOOK_MODEL		       		STRAND_NAME("HOOK", "MODEL")
#define STRAND_HOOK_MODEL_TYPE				INT, 0

#define BUNDLE_HOOK_CALLBACKS		       		STRAND_NAME("HOOK", "CALLBACKS")
#define BUNDLE_HOOK_CALLBACKS_TYPE			HOOK_CB_FUNC, NULL

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
#define STRAND_HOOK_HANDLE_TYPE				UINT64, 0

#define STRAND_HOOK_FLAGS		       		STRAND_NAME("HOOK", "FLAGS")
#define STRAND_HOOK_FLAGS_TYPE				UINT64, 0

#define STRAND_HOOK_COND_STATE				STRAND_NAME("HOOK", "COND_STATE")
#define STRAND_HOOK_COND_STATE_TYPE	       		INT, NIRVA_COND_SUCCESS

#define BUNDLE_HOOK_DETAILS		       		STRAND_NAME("HOOK", "DETAILS")
#define BUNDLE_HOOK_DETAILS_TYPE			HOOK_DETAILS, NULL

// domain TRAJECTORY

#define BUNDLE_TRAJECTORY_SEGMENTS	       		STRAND_NAME("TRAJECTORY", "SEGMENTS")
#define BUNDLE_TRAJECTORY_SEGMENTS_TYPE	       		SEGMENT, NULL

// domain TRANSFORM

#define STRAND_TRANSFORM_STATUS		       		STRAND_NAME("TRANSFORM", "STATUS")
#define STRAND_TRANSFORM_STATUS_TYPE	       		INT64, TRANSFORM_STATUS_NONE

#define STRAND_TRANSFORM_TX_RESULT	       		STRAND_NAME("TRANSFORM", "TX_RESULT")
#define STRAND_TRANSFORM_TX_RESULT_TYPE	       		INT64, TX_RESULT_NONE

#define BUNDLE_TRANSFORM_CURRENT_SEGMENT     		STRAND_NAME("TRANSFORM", "CURRENT_SEGMENT")
#define BUNDLE_TRANSFORM_CURRENT_SEGMENT_TYPE		SEGMENT, NULL

////////////////////////////////////////////////////////////////////////////////////

// whereas STRANDS generally pertain to the internal state of a bundle, ATTRIBUTES
// (more precisely, instances of the ATTRIBUTE bundle)
// are desinged for passing and sharing data between
// bundles.
// The main differences:
//- STRANDS have type, name and data, although these are internal to the strand
// - ATTRIBUTES have strands for DATA, NAME and TYPE
// - ATTRIBUTES are bundles composed of several strands, whereas strands are not bundles -
// they are the building blocks for bundles.
// - Strands cna be scalar values or arrays of unlimited size.
// Attributes may have "max_elements", ie. the value can be
// limited to a certain number of data values (i.e. bounded arrays).
//  Attributes have a default value which must be set in the "default" strand.
// - Attributes have flags and an optional description. Attributes can also be refcounted.
// just as strands may be produced, from strand_defs (i.e. a blueprint)
// attributes can be produced using an attr_def
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
#define ATTR_TYPE_BUNDLEPTR	       			80 // strand_type may be STRAND_TYPE_BUNDLEPTR or
//							STRAND_TYPE_CONST_BUNDLEPTR

// used for variadic function  maps
// the "VALUE" will be a bundleptr -> sup mapping
#define ATTR_TYPE_VA_ARGS	       			128 // void * to other bundle

///////// n.b there is no STRAND_TYPE_FLOAT, this is stored internally as
#define _NIRVA_ATTR_TYPE_TO_STRAND_TYPE_(at) \
    (at == ATTR_TYPE_INT ? STRAND_TYPE_INT :	\
      at == ATTR_TYPE_UINT ? STRAND_TYPE_UINT :		\
      at == ATTR_TYPE_BOOLEAN ? STRAND_TYPE_BOOLEAN :	\
      at == ATTR_TYPE_DOUBLE ? STRAND_TYPE_DOUBLE :	\
      at == ATTR_TYPE_FLOAT ? STRAND_TYPE_DOUBLE :	\
      at == ATTR_TYPE_STRING ? STRAND_TYPE_STRING :	\
      at == ATTR_TYPE_INT64 ? STRAND_TYPE_INT64 :	\
      at == ATTR_TYPE_UINT64 ? STRAND_TYPE_UINT64 :	\
      at == ATTR_TYPE_VOIDPTR ? STRAND_TYPE_VOIDPTR :	\
      at == ATTR_TYPE_FUNCPTR ? STRAND_TYPE_FUNCPTR :	\
      at == ATTR_TYPE_BUNDLEPTR ? STRAND_TYPE_BUNDLEPTR_OPT_CONST :		\
      STRAND_TYPE_INVALID)

#define ATTR_NAMEU(a, b) "ATTR_" a "_" b
#define ATTR_NAME(a, b) ATTR_NAMEU(a, b)

//////////////////////////

// domain STRUCTURAL
#define ATTR_STRUCTURAL_SUBTYPES			ATTR_NAME("STRUCTURAL", "SUBTYPES")
#define ATTR_STRUCTURAL_SUBTYPES_TYPE			BUNDLEPTR, OBJECT_INSTANCE

#define ATTR_STRUCTURAL_BLUEPRINT			ATTR_NAME("STRUCTURAL", "BLUEPRINTS")
#define ATTR_STRUCTURAL_BLUEPRINT_TYPE			BUNDLEPTR, BLUEPRINT

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

// attr_mapping types
// there are three levels (layers) of mapping - trajectory level - maps in / out for transform
// segment - maps in / out for segment
// functional - maps in / out for functional

#define NIRVA_ATTR_MAP_UNDEFINED       		0x00
#define NIRVA_ATTR_MAP_IN       		0x01 // input to functional / segment -> trajectory
#define NIRVA_ATTR_MAP_OUT       		0x02 // internal output within segment

// for in or in / out, maps to param, for out, maps retval
// for trajectory, can be used in native wrapping, and indicates reverse mapping - params to attrs
#define NIRVA_ATTR_MAP_PARAM       		0x08

// defines the attribute as input / output for the entire trajectory
#define NIRVA_ATTR_MAP_EXTERN			0x10

// declares opt attr, at functional level, combined with in | param, implies va_arg,
// cannot be combined with out + param
#define NIRVA_ATTR_MAP_OPT			0x20

// this is a specific flag bit for attributes mapped from native functions where the return value is an array
// it tells us that the functional to calculate the size must be called before the main functional
// this may be due to the fact that the size has to be calculated from attribute (param) values which may be altered
// when the main function runs
#define NIRVA_ATTR_MAP_PRE_CALC_SIZE		0x100

// combinations

// these do not need mapping for functionals / segments but are used for trajectories
// without EXTERN, info values set by contract owner durign negotiations
#define NIRVA_ATTR_MAP_IN_INFO		       	NIRVA_ATTR_MAP_IN
#define NIRVA_ATTR_MAP_OPT_IN_INFO             	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OPT

// ditto, - attributes created from these mappings should intially be readonly at that level
/* #define NIRVA_ATTR_MAP_IN_OUT_INFO	       	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT */
/* #define NIRVA_ATTR_MAP_OPT_IN_OUT_INFO         	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT */

//

#define NIRVA_ATTR_MAP_EXTERN_INPUT	       	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_EXTERN
#define NIRVA_ATTR_MAP_OPT_EXTERN_INPUT        	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OPT | NIRVA_ATTR_MAP_EXTERN

#define NIRVA_ATTR_MAP_IN_PARAM		       	NIRVA_ATTR_MAP_IN |  NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_OPT_IN_PARAM            	NIRVA_ATTR_MAP_IN |  NIRVA_ATTR_MAP_OPT  | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_EXTERN_IN_PARAM	       	NIRVA_ATTR_MAP_IN |  NIRVA_ATTR_MAP_EXTERN | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_OPT_EXTERN_IN_PARAM     	NIRVA_ATTR_MAP_IN |  NIRVA_ATTR_MAP_OPT \
  | NIRVA_ATTR_MAP_EXTERN | NIRVA_ATTR_MAP_PARAM

#define NIRVA_ATTR_MAP_EXTERN_IN_OUT	       	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_EXTERN
#define NIRVA_ATTR_MAP_OPT_EXTERN_IN_OUT       	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT | \
  NIRVA_ATTR_MAP_EXTERN

#define NIRVA_ATTR_MAP_IN_OUT_PARAM	       	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_OPT_IN_OUT_PARAM        	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT \
  | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_EXTERN_IN_OUT_PARAM     	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_EXTERN \
  | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_OPT_EXTERN_IN_OUT_PARAM 	NIRVA_ATTR_MAP_IN | NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT \
  | NIRVA_ATTR_MAP_EXTERN | NIRVA_ATTR_MAP_PARAM

// only needs mapping if it connects to IN_PARAM without extern
#define NIRVA_ATTR_MAP_OUTPUT		       	NIRVA_ATTR_MAP_OUT

// only needs mapping if it connects to OPT_IN_PARAM without extern
#define NIRVA_ATTR_MAP_OPT_OUTPUT              	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT

#define NIRVA_ATTR_MAP_EXTERN_OUTPUT	       	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_EXTERN
#define NIRVA_ATTR_MAP_OPT_EXTERN_OUTPUT       	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_OPT | NIRVA_ATTR_MAP_EXTERN

#define NIRVA_ATTR_MAP_RETVAL		       	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_PARAM
#define NIRVA_ATTR_MAP_EXTERN_RETVAL	       	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_PARAM | NIRVA_ATTR_MAP_EXTERN

#define NIRVA_ATTR_MAP_ANON_RETVAL	       	NIRVA_ATTR_MAP_OUT | NIRVA_ATTR_MAP_PARAM | NIRVA_ATTR_MAP_OPT

// all levels
#define NIRVA_ATTR_MAP_INVALID_1(map)	       	(!(map & NIRVA_ATTR_MAP_OUT) && !(map & NIRVA_ATTR_MAP_IN))

// valid only for HELPER functionals
#define NIRVA_ATTR_MAP_INVALID_2(map)	       	((map & NIRVA_ATTR_MAP_OUT) && (map & NIRVA_ATTR_MAP_PARAM) \
						 && (map & NIRVA_ATTR_MAP_OPT))
// also invalid: mapping same attr as IN_OUT and OUT in same functional
// also invalid: mapping same attr as IN_OUT and IN_OUT in same functional
// functionals may not map input or in_out without external
// functionals may map output without external, this is necessary if they later
// inputs are mapped first as in / param / extern or in_out / param / extern
// outputs can be mapped as
// out - attr is available as param input and param in out
// out and extern  - attr is available as param input and param in out, and is exported
// out and opt  - attr is available as opt param input or opt param in out
// out and opt and exported  - attr is available as opt param input or opt param in out and may be exported
// out and param - return val, usable as param input and param in out
// out and param and extern - return val for export, usable as param input and param in out

#define NIRVA_ATTR_MAP_VALID(map) (!NIRVA_ATTR_MAP_INVALID_0(map) && !NIRVA_ATTR_MAP_INVALID_1(map) \
				   && !NIRVA_ATTR_MAP_INVALID_2(map)

// attribute flag bits (defined in attr_def)//

// this is only valid for attrs with EXTERN mapping
// - for inputs, if not OPT,  it creates a condition that the caller must connect it to a provider,
// who agrees to respond to the data request trigger
//
// if not flagged RO, indicates an edit stream - DATA is pulled sequentially, possibly altered
// then pushed back
//

// For outputs it indicates that the value is updated as part of the transform
// and there will be data_ready and possibly data_prep hooks
// if the attribute
// this bit must set in the attr_def template, and may not be altered
#define NIRVA_ATTR_FLAG_SEQUENTIAL 	   	0x01

// attribute flag bits - set in attribute

// the attribute owner may set this for an in / out or output attribute
// this will prevent the atribute DATA from being altered
// in addition, during negotiations, the owner can set this to "lock" the value of an attibute
#define NIRVA_ATTR_FLAG_READONLY 	   	0x04

// indicates an optional attr. i.e. it is not necessary to set the "DATA" to action a transform
// This is set by default for attrs created from optional in / out
// for a trajectory, however the attr owner may alter this during negotiations
#define NIRVA_ATTR_FLAG_OPTIONAL 	   	0x08

// this flag bit must be set (usually automatically) when an attribute is connected to another
#define NIRVA_ATTR_FLAG_CONNECTED 	   	0x10

// this is for SEQUENTIAL attributes,
// indicates that new data may take some time to process
// if this is connected
// during negotiations, if a connectionis to be made with ASYNC_UPDATES flag
// the structure may interpose and connect the producer and receiver via a BUFFER instance
// - the buffer will request data as fast as possible from the producer, and pass this to the receiver on demand
#define NIRVA_ATTR_FLAG_ASYNC_RECEIVER    	0x20

// the following are informational and maybe set by the attribute owner
// (valid for object attrs)

// indicates that the DATA in the attribute is not a normal data strand,
// but rather a pointer to a variable. NATIVE_TYPE and NATIVE_SIZE may be used
// to indicate the variable type and data size.
#define NIRVA_ATTR_FLAG_NATIVE       		0x40

// indicates that the value may update spontaneously without it being possible to trigger
// data hooks
#define NIRVA_ATTR_FLAG_VOLATILE	 	0x80

// the following are for information

// indicates the value is known to be out of date, but a a transform is needed to
// update it
#define NIRVA_ATTR_FLAG_NOT_CURRENT      	0x100

// indicates the value is a "best guess", the actual value of whatever it represents may differ
#define NIRVA_ATTR_FLAG_ESTIMATE       		0x200

// combinations
// IN | EXTERN | SEQUENTIAL | OPT | RO indicates an optional input data stream
// IN | EXTERN | SEQUENTIAL | RO creates an input data condition

// IN | EXTERN | SEQUENTIAL | OPT indicates an optional input data stream
// IN | EXTERN | SEQUENTIAL | creates an input data condition

// OUT | EXTERN | SEQUENTIAL | RO indicates an output stream with readonly values
// OUT | EXTERN | SEQUENTIAL 	indicates an editable output stream, may be connected to an EDITOR

// OUT | EXTERN | SEQUENTIAL | RO | OPT indicates a conditional output stream with readonly values
// OUT | EXTERN | SEQUENTIAL | OPT	indicates a conditional editable output stream, may be connected to an EDITOR

// if a transform has both input and output streams then INTENT_PROCESS becomes
//
// if a transform has input streams only then INTENT_PROCESS becomes
// INTENTION_RECORD,  // record

// if a transform has input streams only, and they are not mapped RO (i.e IN_OUT) then INTENT_PROCESS becomes
// INTENTION_EDIT,  // edit - some kinf of inplace data change
// adding CAP_REALTIME produces INTENTION_FILTER

// if a transform has input streams and output streams, then INTENT_PROCESS becomes
// INTENTION_CONVERT,  // convert - creates an ouput stream from an input stream
// adding CAP_REALTIME produces INTENTION_PLAY
// adding CAP_REMOTE produces INTENTION_STREAM, etc.

// if a transform has output streams only, then INTENT_PROCESS becomes
// INTENTION_RENDER,

// ATTR CONNECTION flags
#define CONNECTION_FLAG_COPY_ON_DISCONNECT (1ull << 0)

// for optional attributes, indicates the when the connection is broken, the local attribute
// should be removed from the bundle rather than left in place
// in this case any attributes connected to this one will be disconnected first
#define CONNECTION_FLAG_DESTROY_ON_DISCONNECT (1ull << 1)

// normally when a local attribute is connected to a remote one, the local attr becomes readonly,
// the local attribute returns the data value from the remote one
// when an attr is connected, the owner of the remote attr may set this, which allowes writes to teh local attr
// to be forwarded to the remote one
// the automation handles this by adding callbacks to the value_updated hook for the attr "data"
#define CONNECTION_FLAG_READ_WRITE (1ull << 2)

// this is for attributes connected as part of a transform
// indicates that data_request may return REQUEST_WAIT_RETRY from data_request hook
// if the attribute is flagged realtime,
// the data_request should be done as early as possible
// so that it will be ready when actually needed

#define CONNECTION_FLAG_ASYNC_UPDATES (1ull << 3)

// Transform flags

// this flag bit indicates that the transform can be actioned "at will"
// note that the caps still need to be valid, all mandatory attrs set
// any extra_conditions met
#define TX_FLAG_NO_NEGOTIATE				1

// the transform does not return immediately
// depending on threading automation level, the caller should either provide a thread instance
// to action it, or ask the automation to supply one
// if caller chooses to action it using seld thread then the instance will be flagged as busy
#define TX_FLAG_ASYNC

// indicates that multiple simultaneous copies of the transform can be actioned, however
// some segemnts may be single threaded - other copies will be queued temporarily
#define TX_FLAG_QUEUED					2

// indicates that multiple copies of the transform can be actioned simultaneously, with no
// additional performance costs
#define TX_FLAG_THREADSAFE				3

// indicates that the transform can benefit from having multiple threads assigned to it
#define TX_FLAG_PARALLEL				4

// indicates that actioning the transform usilises system resources to an extend that it should
// be actioned only when strictly necessary
#define TX_FLAG_COSTLY					5

// may be set if a problem is detected running the transform - e.g missing in / out attrs
#define TX_FLAG_BROKEN					16

// function wrappings

// the underlying fn type, can only be one of these
#define FUNC_TYPE_UNDEFINED		0 // placeholder
#define FUNC_TYPE_STANDARD 		1 // nirva_function_t format available
#define FUNC_TYPE_NATIVE 		2 // native format available, params mapped to / from attrs
#define FUNC_TYPE_SCRIPT 		3 // script format available - parsed at runtime

// wrapping refers to how a trajectory as a whole is presented
// real refers to the individual functionals in the trajectory
/// there maybe one type or a mixture
// for NATIVE wrapping, we have a choice, - either va_list params can be mapped to attrs
// or else the attrs can be set directly

// for standard wrapping, there is no mapping from params to attrs at teh trajectory level
//
// info about the type, we can have segment, structural, automation, external,
// callback, conditional, placeholder, synthetic
// some functionals may fit in > 1 category, in which case, the most specific
// less generic option should be used, takiing into consideration the internded purpose of the functional

NIRVA_ENUM(
  FUNC_CATEGORY_GENERAL,	// uncategorised
  FUNC_CATEGORY_CORE,	// marks an IMPL function
  FUNC_CATEGORY_TX_SEGMENT,	// function contained in a transform. segment
  FUNC_CATEGORY_HELPER,	// a helper function in a segment, whoch should not be run in the sequence (e.g calc_array_size)
  // represents "static" transform in structurals, this means:
  // - the contract is not held in an object, but in the static lookup table
  // - the contract is referenced by name rather than by icaps
  // - the default CAPs are always valid, and are not adjusted
  // - the contract is flagged "no-negotiate"
  // - there are no optional input / output attributes
  // - the trajectory corresponding to the transform is native wrapped
  // - there is a single non optional output attribute, which can be returned from the native wrapper
  FUNC_CATEGORY_STRUCTURAL,
  FUNC_CATEGORY_TRANSFORM,	// "function" is a trajectory which can action a transform
  FUNC_CATEGORY_AUTOMATION,	// any kind of auto script
  FUNC_CATEGORY_CALLBACK,	// function added to a hook cb stack
  // functional which divides a set of candidates into subsets, discrete (either or) or by affinity (-1. to + 1.)
  FUNC_CATEGORY_CLASSIFIER,
  // a subtype of classifier, which divides candidates into two discrete subsets, valid and invalid for example
  FUNC_CATEGORY_SELECTOR,
  // a subtype of selector which operates on a single candidate at a time
  FUNC_CATEGORY_CONDITIONAL,
  // a subtype of classifier which assigns candidate affinities to two subsets, positive and negative
  FUNC_CATEGORY_RANKING,
  FUNC_CATEGORY_SYNTHETIC,	// "functions" that wrap around non functions - e.g macros
  FUNC_CATEGORY_INTERNAL,	// internal utility
  FUNC_CATEGORY_WRAPPER,	// a function shell, interface to other fuctions
  FUNC_CATEGORY_EXTERNAL,	// an external "native" function which can be called directly or indirectly
  FUNC_CATEGORY_UTIL, 		// for small general fuctions . macros which do not fit in any other category
  FUNC_CATEGORY_OUTSIDE,	// falls outside of nirva, eg. library or system call
  FUNC_CATEGORY_PLACEHOLDER,// for reference / info only - do not call
  FUNC_CATEGORY_NATIVE_TEXT,// textual transcription of native code
  FUNC_CATEGORY_ORACLE,	// an oracle function
  N_STD_FUNC_CATEGORIES 	// values above this may be used for custom categories
)

#define SCRIPT_MATURITY_INVALID			0
#define SCRIPT_MATURITY_UNCHEKED		1
#define SCRIPT_MATURITY_FIRST_PASS     		2
#define SCRIPT_MATURITY_PARSED			3
#define SCRIPT_MATURITY_OPTIMISED	       	4

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

// transform status (int64)
#define TRANSFORM_ERROR_REQ -1 // not all requirements to run the transform have been satisfied
#define TRANSFORM_STATUS_NONE 0

// transform is being configured / created
#define TRANSFORM_STATUS_CONFIGURING 	1

// CAPS have been checked and subset is valid
#define TRANSFORM_STATUS_CAPS_VALID 	2

// transform is being negotiated
#define TRANSFORM_STATUS_NEGOTIATING 	3

// transform is ready to be actioned
#define TRANSFORM_STATUS_READY	 	4

// inital statuses
// transform has been actioned and is queued for trajectory_runner
#define TRANSFORM_STATUS_QUEUED 	8

// transform is preparing for execution
#define TRANSFORM_STATUS_PREPARING 	9

// runtime statuses
#define TRANSFORM_STATUS_RUNNING 	16 ///< transform is "running" and the state cannot be changed
#define TRANSFORM_STATUS_WAITING 	17	///< transform is waiting for conditions to be satisfied
#define TRANSFORM_STATUS_PAUSED	 	18///< transform has been paused, via a call to the pause_hook

// transaction is blocked, action may be needed to return it to running status
#define TRANSFORM_STATUS_BLOCKED 	19///< transform is waiting and has passed the blocked time limit

// final statuses
#define TRANSFORM_STATUS_SUCCESS 	32	///< normal / success
#define TRANSFORM_STATUS_CANCELLED 	33  ///< transform was cancelled via a call to the cancel_hook
#define TRANSFORM_STATUS_ERROR  	34	///< transform encountered an error during running
#define TRANSFORM_STATUS_TIMED_OUT 	35	///< timed out waiting for data / sync

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

// not all of the promised output attributes were created
#define TX_RESULT_UNFULFILLED 		-7

// SEGMENT_END was not listed as a possible next segment
// and no conditions were met for any next segment
// the trajectory or contract should be adjusted to avoid this
// an ADJUDICATOR object may flag the contract as invalid until updated
#define TX_RESULT_TRAJECTORY_INVALID 	-8

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
//  there are 4 hook "patterns"

NIRVA_TYPEDEF_ENUM(nirva_hook_patterns, NIRVA_HOOK_PATTERN_CONFIG,	\
                   NIRVA_HOOK_PATTERN_DATA, NIRVA_HOOK_PATTERN_REQUEST,	\
                   NIRVA_HOOK_PATTERN_SPONTANEOUS, NIRVA_N_HOOK_PATTERNS);

// the pattern combined with HOOK_FLAGS defines the hook_type
// bundles can have one HOOK_STACK per hook_type, however these are only created on demand

// this flag bit indicates that the hook should be triggered just prior to the triggering event (data change,
// config update, spontaneous event, or request handling)
#define NIRVA_HOOK_FLAG_BEFORE		(1ull << 0)

// for CONFIG mpdel
#define NIRVA_HOOK_FLAG_DELETE		(1ull << 1) // ??

// depending on the details of the triggering event, these basic hook_types can then be "cascaded"
// to produce further finely divided "hook_numbers"

// these flags - hook_details, describe properties of the cascaded values

#define NIRVA_HOOK_DTL_SELF			(1ull << 0) // only the object istself may add callbacks

// hook caller will continue to retry conditions until they succeed
// or the hook times out
#define NIRVA_HOOK_DTL_COND_RETRY		(1ull << 1)

// conditions will be checked once only, on fail the operation triggering the hook
// will be abandoned. For request hooks, REQUEST_NO will be returned
#define NIRVA_HOOK_DTL_COND_ONCE		(1ull << 2)

// spontaneous

// hook was triggered by some underlying condition, eg. a native signal rather than being
// generated by the application
#define NIRVA_HOOK_DTL_NATIVE         	(1ull << 7)

NIRVA_TYPEDEF_ENUM(nirva_hook_number,
                   NO_HOOK = 0,
                   /// CONFIG hook pattern
                   // config hooks - these are based on other patterns, but given an omportant significance

                   // transform suffered a FATAL error or was aborted, (HOOK_DTL_NATIVE)
                   // hook_stack specifc to structure_app
                   FATAL_HOOK,

                   // state changing from normal -> not ready, i.e. restarting
                   // this is specific to structure_app
                   RESETTING_HOOK,  // object state / before

                   // bundle is about to be freed
                   // THIS IS A VERY IMPORTANT HOOK POINT, anything that wants to be informed when
                   // a bundle is about to be freed should add a callback here
                   // this is actually the HOOK_CB_REMOVED hook for the stack
                   // - all callbacks are force removed when an object is about to be recycled
                   // hook_stack is created on demand when a callback is added
                   DESTRUCTION_HOOK, // hook_cb_remove, bundle_type == object_instance

                   // thread running the transform received a fatal signal
                   // DTL_NATIVE
                   THREAD_EXIT_HOOK,

                   // for an APPLICATION instance, these are GLOBAL HOOKS
                   N_GLOBAL_HOOKS,// 5

                   // The following are the standard hook points in the system
                   // all DATA_HOOKS must return "immedaitely"

                   // OBJECT STATE and SUBTYPE HOOKS
                   // these passive hooks, provided the system has semi or full hook automation
                   // the structure will call them on behalf of a thread instance

                   // these are spontaneous hooks, triggered when a transform with intent
                   // create bundle or copy bundle complete susccesfully and create a bundle
                   // of type object_template or object_instance

                   // the hook_stacks for these are in the structure, adding removing, triggering
                   // is done via a structure transform
                   OBJECT_CREATED_HOOK, // object state / after

                   INSTANCE_COPIED_HOOK,

                   // these hooks are specific to bundle_type object_template, object_instance
                   // they are data hooks for specific strands
                   // hook_stacks are contained in the bundle
                   MODIFYING_SUBTYPE_HOOK,
                   // object subtype changed
                   SUBTYPE_MODIFIED_HOOK,
                   ALTERING_STATE_HOOK,
                   STATE_ALTERED_HOOK,

                   // add strand is triggered when data is to be appended to a non existent strand, ie old value is absent
                   // delete strand is triggered from delete_strand, ie. new_value is absent

                   ADDING_STRAND_HOOK, // 12

                   DELETING_STRAND_HOOK,

                   STRAND_ADDED_HOOK,

                   STRAND_DELETED_HOOK,

                   // DATA_HOOK pattern

                   // for ARRAYs

                   APPENDING_ITEM_HOOK,

                   REMOVING_ITEM_HOOK,

                   CLEARING_ITEMS_HOOK,

                   ITEMS_CLEARED_HOOK,

                   ITEM_APPENDED_HOOK,

                   ITEM_REMOVED_HOOK,

                   // for SCALAR values
                   UPDATING_VALUE_HOOK,

                   VALUE_UPDATED_HOOK, //21,

                   // TRANSFORM lifecycle hooks
                   // these are triggered by transform state changes
                   // transforms begin in state UNQUEUED,
                   // once actioned they may go into state QUEUED
                   // or straight to PREPARING -> PREPARED -> TX_START
                   // after this we will have a sequence of one or more SEGMENT_START, SEGMENT_END
                   // DATA_READY indicates a sequential out or in . out sttr has been updated
                   // the state may also change to PAUSED -> RESUMING
                   // the state may also change to SYNC_WAIT -> TX_BLOCKED -> TIMED_OUT
                   // the state may also change to WAITING -> TX_BLOCKED -> TIMED_OUT
                   // ERROR, CANCELLED, TIMED_OUT (data),CONTRACT_BREACHED, SYNC_ANNOUNCE can be set spontaneously
                   // then we can have ATTRS_UPDATED
                   // finally, COMPLETED
                   // after this, the transform will be returned to the caller with TX_RESULT updated

                   // these hook stacks are created on demand when a callback is added to a transform bundle

                   PREPARING_HOOK,  /// none or queued -> prepare **

                   PREPARED_HOOK,  /// prepare -> running ??

                   TX_START_HOOK, /// any -> running //25

                   ///
                   PAUSED_HOOK, ///< transform was paused via pause_hook **

                   ///< transform was resumed via resume hook (if it exists),
                   // and / or all paused hook callbacks returning
                   RESUMING_HOOK,  // **

                   COMPLETED_HOOK,   /// 27 running -> finished

                   FINISHED_HOOK,

                   /// this is for IDLEFUNC transforms, indicates the transform completed one cycle
                   // and mey be actioned again
                   IDLE_HOOK,

                   TIMED_OUT_HOOK, ///< timed out in hook - need_data, sync_wait or paused

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

                   // transform is waiting for some external event. If it reamins in this state too long it
                   // may become BLOCKED and then eventually TIMED_OUT
                   TX_WAITING_HOOK,

                   ///< tx is blocked, may be triggered after waiting has surpassed a limit
                   // but hasnt yet TIMED_OUT
                   // indicates that some external action may be needed to allow the transform to continue
                   //
                   // to any hooks with callbacks - callbacks shoudl return quickly
                   // but especilly important for hooks with cb type COND_RETRY

                   // an ARBITRATOR object can attempt to remedy the situation,
                   // for SYNC_WAIT this implies finding which Conditions are delaying and attempting
                   // to remedy this
                   // for DATA_REQUEST this implies cheking why the data provider is delaying,
                   // and possibly finding
                   // a replacement
                   // for SEGMENT, the arbitrator may force the Transform to resume,
                   // if multiple next segments are causing the
                   // delay it may select which one to follow next, preferring segment_end
                   // if avaialble, to complete the transform

                   TX_BLOCKED_HOOK,

                   //

                   // SPONTANEOUS HOOK pattern
                   //
                   // spontaneous hooks are not triggered by data changes, but rather as a response to
                   // events, or at fixed points in a transform.
                   //
                   //

                   // this hook is triggered whenever some contract condition is found to be broken
                   // for example, an attr connectin being broken, a missing input attribute
                   // if ARBITRATOR is enabled, then it will handle callbacks for this hook
                   // if due to mising attrs, then the transform will end with reeult TX_RESULT_CONTRACT_WAS_WRONG
                   // this is a spntaneous hook, the stack is held in the structure, adding, removing or
                   // triggering is done via structural transform, triggering requires priveleges
                   // attributes include the transform bundle, and details of the breach
                   CONTRACT_BREACHED_HOOK,

                   //
                   // this is a "self hook" meaning the object running the transform only should append to this
                   // the calling thread will block until either: all callbacks return TRUE, or a "control variable" is set to TRUE
                   // callbacks for this hook number must return boolean
                   // hook_stack is in the thread instance, as it is a self hook, callbacks are added via functional code
                   // and triggered by functional code
                   SYNC_WAIT_HOOK, ///< synchronisation point, transform is waitng until **

                   // functionals may trigger sync announcements at key points during their processing
                   // other threads can add callbacks for this and be advised when such a point is reached
                   // hook_stack is in the thread instance
                   // hook is triggered by functional code
                   SYNC_ANNOUNCE_HOOK, ///< synchronisation point, transform is waitng until

                   /// tx transition from one trajectory segment to the next
                   // after this hook returns. a cascade will be run to decide the next segment
                   // this hook provides an opportunity to affect the choice
                   // trajectory may have a next segment which can be overwritten
                   // setting this will require PRIV_TRANSFORMS > 10
                   // hook_stack is in the transform bundle
                   SEGMENT_END_HOOK,

                   // this is triggered when a new trajectory segment is about to begin
                   // for the inital segment, TX_START is triggered instead
                   // for segment end, FINISHED_HOOK runs instead
                   // hook_stack is in the transform bundle
                   SEGMENT_START_HOOK, // 40

                   // this hook may be triggered after a transform completes
                   // it will highlight any IN_OUT attrs which have been altered
                   // and any OUTPUT attrs which may have been created
                   // i.e it collates any hook_callbacks for the array in TX_ATTRS
                   // for the FUNC_DATA bundle
                   // hook_stack is in the attr_group bundle
                   ATTRS_UPDATED_HOOK,

                   // calbacks for the following two hooks are allowed to block "briefly"
                   // so that data can be copied
                   // or edited. The hook callback will receive a "max_time_target" in input, the value depends
                   // on the caller and for data prep this is divided depending on the number
                   // of callbacks remaing
                   // to run. The chronometer may help with the calculation.
                   // objects which delay for too long may be penalized, if they persistently do so

                   // tx hooks not associated with status change
                   // if the data is readonly then DATA_PREVIEW_HOOK is cal
                   // DATA_READY + BEFORE
                   // this hook_stack is held in the transform bundle
                   DATA_PREVIEW_HOOK,

                   // data in its "final" state is ready
                   // data is readonly. The Transform will call all callbacks in parallel and will not block
                   // however it will wait for all callbacks to return before freeing / altering the data
                   // this hook_stack is held in the transform bundle
                   DATA_READY_HOOK,

                   // adding a callback here will cause it to be called when a callback is removed (detached) from
                   // a hook stack in the same bundle, the target_hook_type can be specified ot can be all
                   HOOK_CB_DETACHING_HOOK,

                   // this is called automatically when a callback is added to any hook_stack
                   // all request hooks are cascaded versions of this, depending on the stack
                   // added to, e.g add a callback (request) to the data request queue
                   // this triggers hook attached callback
                   // there is an automation callback (run last),
                   // which when triggered, cascades the trigger using the
                   // hook_stack parameter, and triggers a follow on trigger "data_request"
                   //
                   // which is a virutal hook, that in turn toggles a flag bit for the attribute
                   // thus the thread actioning the transform can add a callback to the hook_cb_attached hook
                   // a.k.a REQUEST_HOOK, specifying the request_type (e.g. data_request), target_item (attribute_name)
                   // and respond directly, otherwise, the structure will repsond on the thread's behalf
                   // the request will remain in the data_request hook_stack. At some later point, the thread
                   // can trigger data_ready, and all appropriate cllbacks in the data_request stack are actioned
                   // in this way, threads can be notified instantly of requests, or async via
                   HOOK_CB_ATTACHED_HOOK,

                   // this can be used for debugging, in a function put NIRVA_CALL(trace, "Reason")
                   // the hook stack will be held in one or other structural subtypes
                   TRACE_HOOK, //47

                   // REQUEST HOOK pattern -certain objects will provide request hook stacks, and requests
                   // can be added to these
                   // sometimes requests will be responded to immediatel with YES, NO, or NEEDS_PRIVELEGE
                   // otherwise WAIT will be returned and the request result will be provided asyn via a
                   // callback function supplie with the hook request
                   //
                   // if the target object is not active, or is busy, the automation may
                   // step in and action the transform itself
                   //
                   // the callback function added is designed to receive the request response
                   // if the reequest is responded to YES or NO immediately, the callback is not added
                   // if the response is wait or proxy, then the callback will be added and triggered on YES or NO

                   // these are similar to no negotiate contracts, however requests can be made even whilst
                   // another transform is already running, and there are no requirement to try to
                   // add a callback (although the request may be rejected, and some requests require PRIV levels)

                   // requests are cascade values of the HOOK_ATTACHED_HOOK
                   // a request is made by adding a request_bundle to the target's request hook
                   // the result is to trigger a hook_attached_hook. If this is not reponded to (ie. no callbacks exist),
                   // then wait_retry
                   // will be returned. The object thread will at some later point, check the request stacks,
                   // and respond by changing transform state, or triggering another hook (e.g. data_ready)
                   // at that point, any requests in the request stacks will be responded to (request_yes)
                   // thus when making a request, the requester can provide a callback function - if the response is
                   // wait_retry or proxied, the caller can do something else until the callback is triggered
                   // this is simailar to adding a callback except that the automation will ensure only the most recent
                   // request for a particular emmision is retained in the request stack. In addition the running thread
                   // will know to trigger something becuase there are callbacks in a request stack

                   DATA_REQUEST_HOOK,

                   // this is called by autopmation in response to refcount reaching (explicitly or implicitly) zero
                   // objects can add callbacks here, if request_yes is returned, the bundle will be recycled
                   // it is possible to return request_no (all object templates will return this, as well as structural instances)
                   // in this case a privelege is required to recycle the bundle
                   // the responder can also return request_wait_retry if the object / bundle needs to free resources for example
                   // the recycler will then wait and try at a later time
                   DESTRUCT_REQUEST_HOOK,

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
                   RESUME_REQUEST_HOOK, //61

                   // normally, sub bundles cannot be reparented from one bundle to another;
                   // UNLESS they are contained in attribute owned by the caller
                   // or in a KEYED_ARRAY owned by the caller (essentially the same thing)
                   // sending this request to an attribute owner, if allowed, will result in the attribute
                   // ownership being transferred to the caller and the attribute will be reparented
                   //
                   TRANSFER_REQUEST_HOOK,

                   // if the transform exposes this hook, then this request can be triggered after or
                   // during a transform. If accepted, the target
                   // will reverse the pervious data update either for the attribute of for
                   // the previous transform
                   // target / value TBD. If the transform cannot be undone further,
                   // NIRVA_REPSONE_NO should be returned
                   // target is a transform with completed status
                   UNDO_REQUEST_HOOK,

                   // if the transform exposes this hook, then this request can be triggered after or
                   // during a transform. If accepted, the target
                   // will reverse the pervious data undo either for the attribute of
                   // for the previous transform
                   // target / value TBD. If the transform cannot be redone further,
                   // NIRVA_REPSONE_NO should be returned
                   // target is a transform with completed status
                   REDO_REQUEST_HOOK,

                   // this request may be made before or after connect a local attribute to a remote one
                   // when an attribute is connected, it becomes readonly by default
                   // in case the object connecting wants write access, it must call this with a callback as data
                   // if the request is accepted, the callback is added to to the data_preview hook
                   // whenevr data is ready for editing, the callback will be triggered with the attribute
                   // marked rw. The target_dest is an attr_group which the attribute is part of,

                   ATTR_CONNECT_RW_REQUEST,

                   //
                   // if the app has an arbitrator, an object bound to a contract may tigger this, and on
                   // COND_SUCCESSm the attribute will be disconnected with no penalties
                   // target is local attribute "value", value is NULL
                   SUBSTITUTE_REQUEST_HOOK,
                   //
                   //
                   // hooks reserved for internal use by instances
                   INTERNAL_HOOK_0,
                   INTERNAL_HOOK_1,
                   INTERNAL_HOOK_2,
                   INTERNAL_HOOK_3,
                   ///
                   N_HOOK_POINTS
                  )

#define RESTART_HOOK RESETTING_HOOK

// for some hook callbacks a value of true returned means the hook callback will stay in the
// callback stack. Setting this ensures that it is removed after the first call, even if
// true is returned
#define HOOK_CB_FLAG_ONE_SHOT			(1 << 1)

// abstraction levels - these may be used to denote the level that some fucntion operates at

NIRVA_ENUM(
  NIRVA_ABSTRACTION_DATA_NODES, // level dealing with individual pieces of data: naem, type, value, array_size
  NIRVA_ABSTRACTION_STRANDS, // data plus and abstract definition (strand_def in blueprint)
  NIRVA_ABSTRACTION_BUNDLES, // grouping of strands, defined in a blueprint
  NIRVA_ABSTRACTION_BUNDLE_TYPES, // differentiation of bundles, assignment to specific roles
  NIRVA_ABSTRACTION_OPERATIONAL, // hook triggers and callbacks, attributes, functional threads
  NIRVA_ABSTRACTION_TRAJECTORY, // abstract thread instances, states, segments, differentiation of hook types
  NIRVA_ABSTRACTION_TRANSFORMS, // input data, output data, transform results, specific for intent / caps
  NIRVA_ABSTRACTION_CONTRACTS, // linking of intentcaps to specific transforms, requirements, negotiation
  NIRVA_ABSTRACTION_INTENTCAPS, // level dealing with intentcaps, mapping of these to contracts or sequences
  NIRVA_ABSTRACTION_APPLICATION, // a mechanism for satisfying intentcaps, includes standalone and plugin extensions
  NIRVA_ABSTRACTION_UNIVERSE // the entire set of known applications / plugins
)

//#define NIRVA_BUNDLE_TYPE bundle_type
#define NIRVA_BUNDLE_TYPE NIRVA_UINT64
#define NIRVA_BUNDLEPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR)
#define NIRVA_CONST_STRING_T NIRVA_CONST NIRVA_STRING_T
NIRVA_TYPEDEF(NIRVA_CONST_STRING_T, NIRVA_CONST_STRING)
#define NIRVA_STRING_ARRAY NIRVA_ARRAY_OF(NIRVA_STRING_T)
#define NIRVA_CONST_STRING_ARRAY NIRVA_CONST NIRVA_STRING_ARRAY
#define NIRVA_GENPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_GENPTR)

NIRVA_TYPEDEF(NIRVA_BUNDLEPTR_T, NIRVA_BUNDLEPTR)

#ifndef NIRVA_CONST_BUNDLEPTR_T
#define NIRVA_CONST_BUNDLEPTR_T NIRVA_BUNDLEPTR_T
#endif

NIRVA_TYPEDEF(NIRVA_CONST_BUNDLEPTR_T, NIRVA_CONST_BUNDLEPTR)

#endif
#ifndef NIRVA_CONST_BUNDLEPTR_ARRAY
#define NIRVA_CONST_BUNDLEPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_CONST_BUNDLEPTR)
#endif

#define NIRVA_INT_ARRAY NIRVA_ARRAY_OF(NIRVA_INT)
#define NIRVA_UINT_ARRAY NIRVA_ARRAY_OF(NIRVA_UINT)
#define NIRVA_INT64_ARRAY NIRVA_ARRAY_OF(NIRVA_INT64)
#define NIRVA_UINT64_ARRAY NIRVA_ARRAY_OF(NIRVA_UINT64)
#define NIRVA_DOUBLE_ARRAY NIRVA_ARRAY_OF(NIRVA_DOUBLE)
#define NIRVA_FUNCPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_FUNCPTR))

NIRVA_TYPEDEF(NIRVA_CONST_STRING, NIRVA_STRAND)
NIRVA_TYPEDEF(NIRVA_STRING_ARRAY, NIRVA_BUNDLEDEF)

#endif // !SKIP_MAIN

//
#define BDEF_REST_CONCAT(BNAME, PRE, EXTRA)
#define BDEF_DEF_CONCAT(BNAME, PRE, EXTRA) PRE BNAME##EXTRA,
#define ATTR_BDEF_DEF_CONCAT(ATTR_BNAME, PRE, EXTRA) PRE ATTR_BNAME##EXTRA,

#define BUNLISTx(BDEF_DEFx_, BDEF_DEF_, pre, extra)			\
  BDEF_DEF_(DEF, pre, extra)						\
    BDEF_DEF_(STRAND_DEF, pre, extra) BDEF_DEF_(VALUE, pre, extra)	\
    BDEF_DEF_(BLUEPRINT, pre, extra) BDEF_DEF_(VALUE_CHANGE, pre, extra) \
    BDEF_DEF_(ATTR_DEF, pre, extra) BDEF_DEF_(SEGMENT, pre, extra)	\
    BDEF_DEF_(REFCOUNTER, pre, extra) BDEF_DEF_(ATTRIBUTE, pre, extra)	\
    BDEF_DEF_(FUNCTIONAL, pre, extra) BDEF_DEF_(ATTR_MAP, pre, extra)	\
    BDEF_DEF_(ATTR_CONNECTION, pre, extra) BDEF_DEF_(EMISSION, pre, extra) \
    BDEF_DEF_(ATTR_GROUP, pre, extra) BDEF_DEF_(CAPS, pre, extra)	\
    BDEF_DEF_(INDEX, pre, extra) BDEF_DEF_(SCRIPTLET_HOLDER, pre, extra) \
    BDEF_DEF_(ERROR, pre, extra) BDEF_DEF_(ICAP, pre, extra)		\
    BDEF_DEF_(HOOK_DETAILS, pre, extra) BDEF_DEF_(HOOK_STACK, pre, extra) \
    BDEF_DEF_(HOOK_CB_FUNC, pre, extra) BDEF_DEF_(LOCATOR, pre, extra)	\
    BDEF_DEF_(OBJECT_TEMPLATE, pre, extra) BDEF_DEF_(OBJECT_INSTANCE, pre, extra) \
    BDEF_DEF_(MATRIX_2D, pre, extra) BDEF_DEF_(CASCMATRIX_NODE, pre, extra) \
    BDEF_DEF_(CONDLOGIC_NODE, pre, extra) BDEF_DEF_(CASCADE, pre, extra) \
    BDEF_DEF_(SELECTOR, pre, extra) BDEF_DEF_(FUNC_DATA, pre, extra)	\
    BDEF_DEF_(SCRIPTLET, pre, extra) BDEF_DEF_(TRAJECTORY, pre, extra)	\
    BDEF_DEF_(CONTRACT, pre, extra) BDEF_DEF_(TRANSFORM, pre, extra)	\
    BDEF_DEF_(ATTR_DEF_GROUP, pre, extra) BDEF_DEF_(OBJECT_HOLDER, pre, extra)

#define ABUNLISTx(ABDEF_DEFx_, ABDEF_DEF_, pre, extra) ABDEF_DEF_(ASPECT_THREADS, pre, extra)

#define BUNLIST(BDEF_DEF, pre, extra) BUNLISTx(BDEF_DEF, BDEF_DEF, pre, extra)
#define ABUNLIST(ABDEF_DEF, pre, extra) ABUNLISTx(ABDEF_DEF, ABDEF_DEF, pre, extra)

#ifdef IS_BUNDLE_MAKER

#include "nirva_auto.h"

// attribute MACROS
#define _GET_ATYPE(a, b) _ATTR_TYPE_##a

#define GET_ATTR_TYPE(xdomain, xitem) _CALL(_GET_ATYPE, ATTR_##xdomain##_##xitem)

#define AJOIN(a, b) GET_ATTR_TYPE(a, b) ATTR_##a##_##b  //STRAND_NAMEU(#a, #b)
#define AJOIN2(a, b, c) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#a, #c)

#define _ADD_ASTRAND(domain, item) AJOIN(domain, item)
#define _ADD_OPT_ASTRAND(domain, item) "?" AJOIN(domain, item)

#define ADD_ATTR(d, i) 	 	    	MS(_ADD_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_OPT_ATTR(d, i) 	      	MS(_ADD_OPT_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_COND_ATTR(d, i, ...)	MS(_ADD_ASTRAND(d,i),_ADD_ASTRAND2(d,i)), \
    NIRVA_COND_CREATE(__VA_ARGS__)

#else // ! BUNDLE_MAKER
#define NIRVA_CALL_DEF(fname,...) NIRVA_CMD(_nirva_##fname?(*_nirva_##fname)(__VA_ARGS__):_NIRVA_MACRO_##fname(__VA_ARGS__))
#define NIRVA_RESULT_DEF(fname,...) (_nirva_##fname?(*_nirva_##fname)(__VA_ARGS__):_NIRVA_MACRO_##fname(__VA_ARGS__))
#define _NIRVA_RESULT_DEF(fname,...) (_nirva_##fname?(*_nirva_##fname)(__VA_ARGS__):_NIRVA_MACRO_##fname(__VA_ARGS__))
#define NIRVA_EXPORTED(fname) NIRVA_EXPORTED_##fname

// direct calls = only used in wrappers internally
#define _NIRVA_ARRAY_APPEND(...)NIRVA_RESULT_DEF(array_append, __VA_ARGS__)
#define _NIRVA_ARRAY_APPENDx(...)NIRVA_CALL_DEF(array_append, __VA_ARGS__)
#define _NIRVA_ARRAY_CLEAR(...)NIRVA_CALL_DEF(array_clear, __VA_ARGS__)
#define _NIRVA_STRAND_DELETE(...)NIRVA_CALL_DEF(strand_delete, __VA_ARGS__)

// low level API
// wrappers for MANDATORY IMPLementation funcs - if not defined, the fallback inmplementation (libweed) will be used
// redefining any of these may require a full restart
#define NIRVA_CREATE_BUNDLE(blueprint) NIRVA_CALL(nirva_create_bundle, blueprint)
#define NIRVA_BUNDLE_LIST_STRANDS(...)NIRVA_RESULT_DEF(bundle_list_strands, __VA_ARGS__)
#define NIRVA_BUNDLE_FREE(...)NIRVA_CALL_DEF(bundle_free, __VA_ARGS__)
#define NIRVA_ARRAY_GET_SIZE(...)NIRVA_RESULT_DEF(array_get_size, __VA_ARGS__)

// indirect calls
#define NIRVA_STRAND_DELETE(...)NIRVA_CMD(NIRVA_EXPORTED(strand_delete(__VA_ARGS__)))
#define NIRVA_ARRAY_APPEND NIRVA_EXPORTED(array_append)
#define NIRVA_ARRAY_CLEAR(...)NIRVA_CMD(NIRVA_EXPORTED(array_clear(__VA_ARGS__)))

// direct calls
#define NIRVA_GET_ARRAY_INT(...)NIRVA_RESULT_DEF(get_array_int, __VA_ARGS__)
#define NIRVA_GET_ARRAY_INT64(...)NIRVA_RESULT_DEF(get_array_int64, __VA_ARGS__)
#define NIRVA_GET_ARRAY_BOOLEAN(...)NIRVA_RESULT_DEF(get_array_boolean, __VA_ARGS__)
#define NIRVA_GET_ARRAY_DOUBLE(...)NIRVA_RESULT_DEF(get_array_double, __VA_ARGS__)
#define NIRVA_GET_ARRAY_STRING(...)NIRVA_RESULT_DEF(get_array_string, __VA_ARGS__)
#define NIRVA_GET_ARRAY_VOIDPTR(...)NIRVA_RESULT_DEF(get_array_voidptr, __VA_ARGS__)
#define NIRVA_GET_ARRAY_FUNCPTR(...)NIRVA_RESULT_DEF(get_array_funcptr, __VA_ARGS__)
#define NIRVA_GET_ARRAY_BUNDLEPTR(...)NIRVA_RESULT_DEF(get_array_bundleptr, __VA_ARGS__)

// direct or indirect
#define NIRVA_BUNDLE_HAS_STRAND(...)NIRVA_RESULT_DEF(bundle_has_strand, __VA_ARGS__)
#define NIRVA_STRAND_COPY(...)NIRVA_CALL_DEF(strand_copy, __VA_ARGS__)

#define NIRVA_ADD_VALUE_BY_KEY(...)NIRVA_CALL_DEF(add_value_by_key, __VA_ARGS__)
#define NIRVA_REMOVE_VALUE_BY_KEY(...)NIRVA_CALL_DEF(remove_value_by_key, __VA_ARGS__)
#define NIRVA_GET_VALUE_BY_KEY(...)NIRVA_RESULT_DEF(get_value_by_key, __VA_ARGS__)
#define NIRVA_HAS_VALUE_FOR_KEY(...)NIRVA_RESULT_DEF(has_value_for_key, __VA_ARGS__)

#define NIRVA_GET_ARRAY_UINT(...)NIRVA_RESULT_DEF(get_array_uint, __VA_ARGS__)
#define NIRVA_GET_ARRAY_UINT64(...)NIRVA_RESULT_DEF(get_array_uint64, __VA_ARGS__)

// constructed funcs - these functions are compounds of the IMPL funcs - can optionally be overloaded
#define NIRVA_GET_VALUE_INT NIRVA_EXPORTED(get_value_int)
#define NIRVA_GET_VALUE_UINT NIRVA_EXPORTED(get_value_uint)
#define NIRVA_GET_VALUE_INT64 NIRVA_EXPORTED(get_value_int64)
#define NIRVA_GET_VALUE_UINT64 NIRVA_EXPORTED(get_value_uint64)
#define NIRVA_GET_VALUE_BOOLEAN NIRVA_EXPORTED(get_value_boolean)
#define NIRVA_GET_VALUE_DOUBLE NIRVA_EXPORTED(get_value_double)
#define NIRVA_GET_VALUE_STRING NIRVA_EXPORTED(get_value_string)
#define NIRVA_GET_VALUE_VOIDPTR NIRVA_EXPORTED(get_value_voidptr)
#define NIRVA_GET_VALUE_FUNCPTR NIRVA_EXPORTED(get_value_funcptr)
#define NIRVA_GET_VALUE_BUNDLEPTR NIRVA_EXPORTED(get_value_bundleptr)
//
#define NIRVA_GET_VALUE_int  NIRVA_GET_VALUE_INT
#define NIRVA_GET_VALUE_uint  NIRVA_GET_VALUE_UINT
#define NIRVA_GET_VALUE_int64  NIRVA_GET_VALUE_INT64
#define NIRVA_GET_VALUE_uint64  NIRVA_GET_VALUE_UINT64
#define NIRVA_GET_VALUE_boolean  NIRVA_GET_VALUE_BOOLEAN
#define NIRVA_GET_VALUE_double  NIRVA_GET_VALUE_DOUBLE
#define NIRVA_GET_VALUE_string  NIRVA_GET_VALUE_STRING
#define NIRVA_GET_VALUE_voidptr  NIRVA_GET_VALUE_VOIDPTR
#define NIRVA_GET_VALUE_funcptr  NIRVA_GET_VALUE_FUNCPTR
#define NIRVA_GET_VALUE_bundleptr  NIRVA_GET_VALUE_BUNDLEPTR

//
#define NIRVA_VALUE_SET(...)NIRVA_CMD(NIRVA_INTERNAL_set_value(__VA_ARGS__))
#define NIRVA_ARRAY_SET(...)NIRVA_CMD(NIRVA_INTERNAL_array_replace(__VA_ARGS__))

#if NIRVA_IMPL_IS(DEFAULT_C)
#define NIRVA_GET_VALUE_GENPTR NIRVA_GET_VALUE_VOIDPTR
#define NIRVA_GET_ARRAY_GENPTR NIRVA_GET_ARRAY_VOIDPTR
#endif

// mid level API - may be overridden to alter functionality, changing may require an APPlication restart
#define NIRVA_RECYCLE(...) NIRVA_CMD(NIRVA_EXPORTED_recycle(__VA_ARGS__))

#define NIRVA_ACTION(funcname,...)(void)def_nirva_action(funcname, __VA_ARGS__)
#define NIRVA_ACTION_RET_STRING(funcname,...)def_nirva_action_ret_string(funcname,__VA_ARGS__)
#define NIRVA_ACTION_RET_BUNDLEPTR(funcname,...)def_nirva_action_ret_bundleptr(funcname, STRAND___VA_ARGS__)

// high level API
// these are wrappers around NIRVA_ACTION* calls
// can be changed by subsituting contracts
#define NIRVA_CREATE_BUNDLE_BY_TYPE(btype,...) create_bundle_by_type(btype,__VA_ARGS__)

#undef NIRVA_DEF_FUNC
#endif

#ifndef HAVE_NIRVA_BTYPE
#define HAVE_NIRVA_BTYPE
NIRVA_TYPEDEF_ENUM(bundle_type, BUNLIST(BDEF_DEF_CONCAT,, _BUNDLE_TYPE) n_builtin_bundledefs)
NIRVA_TYPEDEF_ENUM(attr_bundle_type, ABUNLIST(ATTR_BDEF_DEF_CONCAT,, _ATTR_GROUP_TYPE) \
                   n_builtin_attr_bundles)
#endif
#ifdef BDEF_DEF
#undef BDEF_DEF
#endif
#ifdef ABDEF_DEF
#undef ABDEF_DEF

NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY all_def_strands;
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_strands(NIRVA_CONST_STRING fmt, ...);
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_bundledef(NIRVA_CONST_STRING name,	\
    NIRVA_CONST_STRING pfx, ...);
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY maker_get_bundledef(bundle_type btype);

#endif

#define GET_BDEF(btype) maker_get_bundledef(btype)

#ifndef MB
#define MB(name, pfx, ...) make_bundledef(name, pfx, __VA_ARGS__, NULL)
#endif
#ifndef MBX
#define MBX(name) MB(#name, NULL, _##name##_BUNDLE)
#endif
#ifndef MABX
#define MABX(name) MB(#name, NULL, _##name##_ATTR_GROUP)
#endif
#define BDEF_DEFINE(BNAME) NIRVA_CONST_STRING_ARRAY BNAME##_BUNDLEDEF

// these values may be adjusted during inial bootstrap - see the documentation for more details
// values here define the level of automation for each "aspect"
#define NIRVA_AUTOMATION_NONE 			0
#define NIRVA_AUTOMATION_MANUAL 	       	1
#define NIRVA_AUTOMATION_SEMI 			2
#define NIRVA_AUTOMATION_FULL 			3

#define NIRVA_AUTOMATION_DEFAULT 		NIRVA_AUTOMATION_SEMI

// NIRVA_ASPECT_NEGOTIATING
// NIRVA_ASPECT_FUNCTION_BENDING
// NIRVA_ASPECT_OPTIMISATION
// NIRVA_ASPECT_SELF_ORGANISATION

#define NIRVA_ASPECT(m) NIRVA_ASPECT_NAME_##n

// automation for these "aspects"
#define NIRVA_ASPECT_NONE			0

#define NIRVA_ASPECT_BUNDLES			1
#define NIRVA_ASPECT_NAME_1 "BUNDLES"
// if set to NIRVA_AUTOMATION_FULL then automation will control:
// - including sub bundles, - setting "container"
// - adding const bundleptrs
// - checking restrictions for added / included bundles
// - managing KEYED_ARRAYs and INDEXES
// - updating "ARRAY_SIZE" for arrays, getting array size


// if set to NIRVA_AUTOMATION_SEMI then automation will control:
// - including sub bundles, - setting "container"
// - adding const bundleptrs
// - checking restrictions for added / included bundles
// - managing KEYED_ARRAYs and INDEXES, but only if any of find_array_item_by_key, add_array_item_by_key,
//    remove_array_item_by_key are missing (undefined)
// - updating "ARRAY_SIZE" for arrays, getting array size, but only if

#define NIRVA_ASPECT_THREADS			2
#define NIRVA_ASPECT_NAME_2 "THREADS"
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
#define ATTR_THREADS_MODEL_TYPE				INT, NIRVA_THREAD_MODEL_NONE

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
#define ATTR_IMPLFUNC_BORROW_NATIVE_THREAD_TYPE       FUNCPTR, NULL

#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "RETURN_NATIVE_THREAD")
#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD_TYPE       FUNCPTR, NULL

// for FULL automation, the structure will handle all aspects of thread management itself

#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "CREATE_NATIVE_THREAD")
#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD_TYPE       FUNCPTR, NULL

#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "DESTROY_NATIVE_THREAD")
#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD_TYPE      FUNCPTR, NULL

// this is a condition to be checked, on NIRV_COND_SUCCESS, the atribute becomes mandatory
#define __COND_THREADS_1 _COND_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI), _COND_VAL_EQUALS,		\
    _COND_ATTR_VAL(ATTR_THREADS_MODEL), _COND_INT_VAL(NIRVA_THREAD_MODEL_ON_DEMAND))

// this is a condition to be checked, on NIRV_COND_SUCCESS, the attribute becomes mandatory
#define __COND_THREADS_2 _COND_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
      _COND_INT_VAL(NIRVA_AUTOMATION_SEMI), _COND_VAL_EQUALS,		\
      _COND_ATTR_VAL(ATTR_THREADS_MODEL),				\
      _COND_INT_VAL(NIRVA_THREAD_MODEL_INSTANCE_PER_THREAD),		\
      NIRVA_OP_OR, _COND_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
      _COND_INT_VAL(NIRVA_AUTOMATION_FULL))

#define _ASPECT_THREADS_ATTRBUNDLE #THREADS_ASPECT, ADD_ATTR(AUTOLEVEL, THREADS), \
    ADD_ATTR(THREADS, MODEL), ADD_COND_ATTR(IMPLFUNC, 201, NIRVA_EQUALS, \
					    ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, \
					    NIRVA_EQUALS, ATTR_THREADS_MODEL, \
					    NIRVA_THREAD_MODEL_ON_DEMAND), \
    ADD_COND_ATTR(IMPLFUNC, 202, ADD_COND_ATTR(IMPLFUNC, 203, _COND(THREADS, 2)), \
    ADD_COND_ATTR(IMPLFUNC, 204, _COND(THREADS, 2))

#define NIRVA_OPT_FUNC_201 borrow_native_thread,			\
      "request a native thread from application thread pool",RETURN_NATIVE_THREAD,0
#define NIRVA_OPT_FUNC_202 return_native_thread,"return a native thread to  application thread pool", \
    NIRVA_NO_RETURN,1,NIRVA_NATIVE_THREAD,thread

#define _MAKE_IMPL_COND_FUNC_2_DESC(n)					\
  (n == 201 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_201) : n == 202 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_202) : \
   n == 203 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_203) : n == 204 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_204) : NULL)

////////////////////////////////////////////////

#define NIRVA_ASPECT_HOOKS			3
#define NIRVA_ASPECT_NAME_3 "HOOKS"

// attributes created intially in STRUCTURE_APP
// preferred subtypes: AUTOMATION
// the attributes are transfered to preferred subtypes in order of preference
// changing these after structure_app is in PREPARED state requires PRIV_STRUCTURE > 100

// level of automation for hooks
//  - full, the structure will manage every aspect,including calling user callbcaks
//  - semi - system hooks will run automatically, but the application will run user_callbacks
//  - manual - the automation will signal when to trigger hooks and wait for return, but nothing more

// this is a condition to be checked, on NIRV_COND_SUCCESS, the atribute becomes mandatory
#define __COND_HOOKS_1 _COND_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_HOOKS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI)
#define __COND_HOOKS_2 COND_VAL_NOT_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_THREADS), \
    _COND_INT_VAL(NIRVA_AUTOMATION_FULL)

#define ATTR_AUTOLEVEL_HOOKS				ATTR_NAME("AUTOLEVEL", "HOOKS")
#define ATTR_AUTOLEVEL_HOOKS_TYPE 		       	INT, NIRVA_AUTOMATION_DEFAULT

#define _ASPECT_HOOKS_ATTRBUNDLE #HOOKS_ASPECT, ADD_ATTR(AUTOLEVEL, HOOKS), \
    ADD_COND_ATTR(IMPLFUNC, 301,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 302,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 303,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 304,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 305,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 306,_COND(HOOKS, 1)),			\
    ADD_COND_ATTR(IMPLFUNC, 307,_COND(HOOKS, 1), _COND_(HOOKS, 2)),	\
    ADD_COND_ATTR(IMPLFUNC, 308,_COND(HOOKS, 1), _COND_(HOOKS, 2)),	\
    ADD_COND_ATTR(IMPLFUNC, 309,_COND(HOOKS, 1), _COND(HOOKS, 2))

#define NIRVA_OPT_FUNC_301 add_hook_callback // etc
#define NIRVA_OPT_FUNC_302 remove_hook_callback // etc
#define NIRVA_OPT_FUNC_303 trigger_hook_callback // etc
#define NIRVA_OPT_FUNC_304 clear_hook_stackk // etc
#define NIRVA_OPT_FUNC_305 clear_all_stacks // etc
#define NIRVA_OPT_FUNC_306 list_hook_stacks // etc
#define NIRVA_OPT_FUNC_307 join_async_hooks // etc
#define NIRVA_OPT_FUNC_308 run_hooks_async // etc
#define NIRVA_OPT_FUNC_309 run_hooks_aync_seq


#define _MAKE_IMPL_COND_FUNC_3_DESC(n)				\
    (n == 301 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_301) :		\
     n == 302 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_302) :		\
     n == 303 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_303) :		\
     n == 304 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_304) :		\
     n == 305 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_305) :		\
     n == 306 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_306) :		\
     n == 307 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_307) :		\
     n == 308 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_308) :		\
     n == 309 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_309) : NULL)

////////////////////////////////////////////////

#define NIRVA_ASPECT_REFCOUNTING		4
#define NIRVA_ASPECT_NAME_4 "REFCOUNTING"

// level of automation for refcounting
//  - full, the structure will manage every aspect,including freeing bundles when refcount reaches -1
//  - semi - the structure will manage reffing and unreffing, but will only indicate when a bundle
//		should be free
//  - manual - the automation will signal when to ref / =unref somthing

#define ATTR_AUTOLEVEL_REFCOUNTING	       		ATTR_NAME("AUTOLEVEL", "REFCOUNTING")
#define ATTR_AUTOLEVEL_REFCOUNTING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

#define __COND_REFCOUNTING_1 _COND_VAL_EQUALS, _COND_ATTR_VAL(ATTR_AUTOLEVEL_REFCOUNTING), \
    _COND_INT_VAL(NIRVA_AUTOMATION_SEMI)

#define _ASPECT_REFCOUNTING_ATTRBUNDLE #REFCOUNTING_ASPECT, ADD_ATTR(AUTOLEVEL, REFCOUNTING, \
    ADD_COND_ATTR(IMPLFUNC, REF_BUNDLE_401,_COND(REFCOUNTING, 1)),	\
    ADD_COND_ATTR(IMPLFUNC, UNREF_BUNDLE_402,_COND(REFCOUNTING, 1))

// add mutex_trylock, mutex_unlock

#define _MAKE_IMPL_COND_FUNC_4_DESC(n)			\
    (n == 401 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_401) :	\
     NULL)
////////////////////////////////

#define NIRVA_ASPECT_RECYCLING			5
#define NIRVA_ASPECT_NAME_5 "RECYCLING"

// level of automation for bundle recycling bundle memory and connectiosn
//  - full, the structure will manage every aspect, including recycling bundles as instructed
// by refcounter
//  - semi - the structure will handke automated activites like disconnecting atributes, detaching hooks
//		but will leave freeing to the applicaiton
//  - manual - the automation will signal when to recycle bundledefs

#define ATTR_AUTOLEVEL_RECYCLING	       		ATTR_NAME("AUTOLEVEL", "RECYCLING")
#define ATTR_AUTOLEVEL_RECYCLING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

#define __COND_RECYCLINGING_1 _COND_VAL_EQUALS, _COND_ATTR_VAL ATTR_AUTOLEVEL_RECYCLING, \
    _CONST_INT_VAL NIRVA_AUTOMATION_SEMI

#define _ASPECT_RECYCLING_ATTRBUNDLE #RECYCLING_ASPECT, ADD_ATTR(AUTOLEVEL, RECYCLING),	\
    ADD_COND_ATTR(IMPLFUNC, FREE_BUNDLE_501,_COND(RECYCLING, 1))

#define _MAKE_IMPL_COND_FUNC_5_DESC(n)			\
  (n == 501 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_501) : NULL)

////////////////////////////////

#define NIRVA_ASPECT_TRANSFORMS			6
#define NIRVA_ASPECT_NAME_6 "TRANSFORMS"

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

// the minimal recommended is THREAD_HERDER this is on by default
// GUI is necessary if the APP wants to use interface automation
// BROKER is also reommended

// for medium sized apps it is recommended to add LIFECYCLE, ARIBTRATOR and ADJUDICATOR.

// for larger apps, ARCHIVER and NEGOTIATOR may be added

// for real time apps, it can be useful to add CHRONOMETRY and OPTIMISER

// for apps with a lot of thread interactions, SECURITY can be added to help
// with PRIV checks

// for highly interactive apps, HARMONY and BEHAVIOUR can be useful additions

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
#define ASPECT_AFFINITY_0 ASPECT_HOOKS, 1, ASPECT_REFCOUNTING, 11, \
      ASPECT_RECYCLING, 1, ASPECT_TRANSFORMS, 1, ASPECT_NONE
#define SUBSYS_NAME_0 "PRIME"
#define SUBSYS_DESC_0 "This object represents the structure itself, and is the first object created." \
  "It performs the structure bootstrap operations and is the template used to create all the other " \
  "structure subtypes. It oversees the activites of all other subsystems, fullfilling the role of " \
  "administrator. Destroying this object requires PRIV_STRUCTURE > 1000, and will result in " \
  "an immediate abort of the application."				\
  "Prime is also responsible for running automations - the subsystem may trigger hooks, " \
  "run hook automations, and mange reference counting."

#define	STRUCTURE_SUBTYPE_APP 			1
#define ASPECT_AFFINITY_1 ASPECT_THREADS, 2, ASPECT_HOOKS, 2, ASPECT_TRANSFORMS, 2, ASPECT_NONE
#define SUBSYS_NAME_1 "APP"
#define SUBSYS_DESC_1 "Structure subtype which represents an application - ie process. Its state " \
    "reflects the state of the application overall, and it can be used to manage and control the " \
  "application process itself. It can also assist Prime with automations, for example managing "\
  "app specific hook callbacks"
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
  "hook triggers and callbacks, in short acting as liason between threads and the rest of the " \
  "structure. It will also be responsible for assigning threads to other loaded subsystems. " \
  "It will coordinate with OPTIMISER if that subsystem is loaded."

#define	STRUCTURE_SUBTYPE_BROKER		4
#define ASPECT_AFFINITY_4 ASPECT_TRANSFORMS, 40, ASPECT_NONE
#define SUBSYS_REQUIRES_4 STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, \
    STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_4 "BROKER"
#define SUBSYS_DESC_4 "Broker is an expert system designed to match intentcaps and requirements with " \
  "contracts, and thus with the objects holding those contracts. Broker will thus maintain a " \
  "an index of all contracts, the intencaps they satisfy, their side effects. Iin addition it will " \
  "keep a database of the attributes of each object type / subtype. Given an intentcap to satisfy, " \
  "broker will map out sequences of transforms to be actioned, possibly providing multiple solutions" \
  "In the case where no solution can be found, broker may indicate the missing steps, as well as " \
  "providing the the nearest alternative resolutions"

#define STRUCTURE_SUBTYPE_LIFECYCLE		5
#define ASPECT_AFFINITY_5 ASPECT_RECYCLING, 100, ASPECT_REFCOUNTING, 100, ASPECT_NONE
#define SUBSYS_REQUIRES_5 STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, \
    STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_5 "LIFECYCLE"
#define SUBSYS_DESC_5 "This will be an expert system designed to track and monitor the states "	\
  "of objects from the moment they are created until they are no longer required. Objects which " \
  "idle for too long may be recycled and their threads redeployed. Objects which lock up system " \
  "resources unnecessarily may be flagged. Runaway objects which create too many instances or " \
  "use up thereads unnecesarily will be flagged and blocked if necessary. If arbitrator or " \
  "adjudicator are present, they will take over some of these roles, and lifecycle will " \
  "relieve Prime of the refcounting role"

#define	STRUCTURE_SUBTYPE_ARBITRATOR		6
#define ASPECT_AFFINITY_6 ASPECT_NONE
#define SUBSYS_REQUIRES_6 STRUCTURE_SUBTYPE_THREAD_HERDER, \
    STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_6 "ARBITRATOR"
#define SUBSYS_DESC_6 "Arbitrator will be an expert system specifally designed fo resolving " \
  "failed condition checks. It will use a vareity of methods depending on the nature of the " \
  "check, as well as the condition itself. For example if the check is rqueirements check " \
  "for a contract, this may be passed on to NEGOTIATOR to resolve."

#define	STRUCTURE_SUBTYPE_ADJUDICATOR		7
#define ASPECT_AFFINITY_7 ASPECT_RECYCLING, 20, ASPECT_NONE
#define SUBSYS_REQUIRES_7 STRUCTURE_SUBTYPE_ARBITRATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_7 "ADJUDICATOR"
#define SUBSYS_DESC_7 "Adjudicator will be an expert system designed to flag and possibly remove " \
  "anything from the application which may cause errors or timeouts."

#define	STRUCTURE_SUBTYPE_NEGOTIATOR		8
#define ASPECT_AFFINITY_8 ASPECT_TRANSFORMS, 100, ASPECT_NONE
#define SUBSYS_REQUIRES_8 STRUCTURE_SUBTYPE_ARBITRATOR, STRUCTURE_SUBTYPE_BROKER, \
    STRUCTURE_SUBTYPE_THREAD_HERDER, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_8 "NEGOTIATOR"
#define SUBSYS_DESC_8 "Negotiator will be an expert system specifially designed to resolve " \
  "failed conditions checks realting to a contract. Depending on the particular condition it may " \
  "search for a means to provide missing attributes, to run parallel transforms to supply streamed " \
  "data, or action pre-transforms to bring objectsto the correct state / subtype"

#define STRUCTURE_SUBTYPE_CHRONOMETRY			9
#define ASPECT_AFFINITY_9 ASPECT_HOOKS 50
#define SUBSYS_REQUIRES_9 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_9 "CHRONOMETRY"
#define SUBSYS_DESC_9 "Chronometry will be an expert system desinged to handle features such as " \
  "high precision timeing, thread synchronisation, time realted instrumentation, as well as " \
  "mainatinaing queues for actioning events at specific intervals or absolute times. In additiion " \
  "it can also assist with automations, triggering hooks when called for"

#define	STRUCTURE_SUBTYPE_ARCHIVER		10
#define ASPECT_AFFINITY_10 ASPECT_NONE
#define SUBSYS_REQUIRES_10 STRUCTURE_SUBTYPE_LIFECYCLE, STRUCTURE_SUBTYPE_BROKER, \
    STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_10 "ARCHIVER"
#define SUBSYS_DESC_10 "Archiver will be an expert subsystem designed specifially for rapid data " \
  "storage, indexing and retrieval. It will have features for dealing with arrays, lists, " \
  "hash tables, lookup tables as well as managing offline stroage, backups and restores, " \
  "crash recovery and so on"

#define	STRUCTURE_SUBTYPE_OPTIMISER 		11
#define ASPECT_AFFINITY_11 ASPECT_THREADS 20, ASPECT_NONE
#define SUBSYS_REQUIRES_11 STRUCTURE_SUBTYPE_CHRONOMETRY, STRUCTURE_SUBTYPE_THREAD_HERDER, \
    STRUCTURE_SUBTYPE_BROKER, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_11 "OPTIMISER"
#define SUBSYS_DESC_11 "This will be an expert system designed to montor the application and " \
  "overall system state, optimising performance"

#define	STRUCTURE_SUBTYPE_SECURITY		12
#define ASPECT_AFFINITY_12 ASPECT_NONE
#define SUBSYS_REQUIRES_12 STRUCTURE_SUBTYPE_ADJUDICATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_12 "SECURITY"
#define SUBSYS_DESC_12 "This subsystem will be an expert system designed to manage credentials and " \
  "privilege levels to ensure the integrity of the application and the environment it runs in"

#define	STRUCTURE_SUBTYPE_HARMONY		13
#define ASPECT_AFFINITY_13 ASPECT_NONE
#define SUBSYS_REQUIRES_13 STRUCTURE_SUBTYPE_ADJUDICATOR, STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_13 "HARMONY"
#define SUBSYS_DESC_13 "This is an expert system designed to interface harmoniously " \
  "with external objects such as operating systems, other applications and Oracles"


#define	STRUCTURE_SUBTYPE_BEHAVIOUR		14
#define ASPECT_AFFINITY_14 ASPECT_NONE
#define SUBSYS_REQUIRES_14 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_14 "BEHAVIOUR"
#define SUBSYS_DESC_14 "Behaviour is an expert system which attempts to fulfill the role of a proxy " \
  "Oracle, supplying best guess and predicitons for mising data, based on observing past responses " \
  "pattern matching, probabilities, correlations and so on"


/* #define _CONDANAL_BUNDLE  INCLUDE_BUNDLE(AUTOMATION, CONDITION),	\ */
/*       ADD_NAMED_OPT_STRAND(VALUE, COUNT, N_TRIALS),			\ */
/*       ADD_NAMED_OPT_STRAND(VALUE, COUNT, N_FAIL),			\ */
/*       ADD_NAMED_OPT_STRAND(VALUE, COUNT, N_SUCCESS),			\ */
/*       ADD_OPT_NAMED_ARRAY(CONDITION, RESPONSE, WINDOW),			\ */
/*       ADD_NAMED_OPT_STRAND(VALUE, COUNT, WINDOW_SIZE),			\ */
/*       ADD_NAMED_OPT_STRAND(VALUE, MAX_VALUES, MAX_WINDOW_SIZE),		\ */
/*       ADD_NAMED_OPT_STRAND(LOGIC, PROBABILITY, P_FAIL),			\ */
/*       ADD_NAMED_OPT_STRAND(LOGIC, PROBABILITY, P_SUCCESS),		\ */
/*       INCLUDE_NAMED_BUNDLES(STANDARD, CORRELATION, CORRELATIONS) */


#define	N_STRUCTURE_SUBTYPES 15

// this subtype cannot be created but can be discovered. An Oracle is able
// to supply the values of specific data types. Oracles have a set of data they can provide
// (selection), conditions under which they can supply it, and a priority.
// higher priority Oracles are consulted before lower priority ones.
// Sometimes there is an object type / subtype, which mediates for an Oracle,
// for example, the GUI subtype can act as a mediator for the USER subtype of Oracle

#define STRUCTURE_SUBTYPE_ORACLE 128
#define ASPECT_AFFINITY_128 ASPECT_NONE
#define SUBSYS_REQUIRES_128 STRUCTURE_SUBTYPE_APP, STRUCTURE_SUBTYPE_PRIME
#define SUBSYS_NAME_128 "ORACLE"
#define SUBSYS_DESC_128 "If you are reading this, this is you."

//#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
