// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// if usage of BUNDLES is desired,
// exactly one file should #define NEED_OBJECT_BUNDLES before including this header
// as well as #define BUNDLE_TYPE to b

#define DEBUG_BUNDLE_MAKER

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

#ifndef SKIP_MAIN

#ifndef NO_STD_INCLUDES
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#endif

#define NIRVA_ARRAY_OF(a) a*
#define NIRVA_PTR_TO(a) a *
#define NIRVA_EXTERN extern
#define NIRVA_TYPEDEF(a,...) typedef a __VA_ARGS__;
#define NIRVA_FUNC_TYPE_DEF(ret_type, funcname, ...) typedef ret_type(* funcname)(__VA_ARGS__);
#define NIRVA_CONST const
#define NIRVA_ENUM enum
#define NIRVA_NULL NULL
#define NIRVA_VARIADIC ...

#define NIRVA_BUNDLEPTR NIRVA_PTR_TO(NIRVA_BUNDLE_T)
#define NIRVA_CONST_BUNDLEPTR NIRVA_CONST NIRVA_BUNDLEPTR

#define NIRVA_CHAR char
#define NIRVA_BUNDLE_TYPE bundle_type
#define NIRVA_VA_LIST va_list

#define NIRVA_BUNDLEPTR_ARRAY NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR)

#define NIRVA_STRING NIRVA_ARRAY_OF(NIRVA_CHAR)
#define NIRVA_CONST_STRING NIRVA_CONST NIRVA_STRING
#define NIRVA_STRING_ARRAY NIRVA_ARRAY_OF(NIRVA_STRING)
#define NIRVA_CONST_STRING_ARRAY NIRVA_ARRAY_OF(NIRVA_CONST_STRING)

NIRVA_TYPEDEF(int, NIRVA_INT)
NIRVA_TYPEDEF(int64_t, NIRVA_INT64)

#ifndef NIRVA_FUNC_RETURN
#define NIRVA_FUNC_RETURN NIRVA_INT64
#endif

#define NIRVA_REQUEST_REPSONSE NIRVA_FUNC_RETURN
#define NIRVA_COND_RETURN NIRVA_FUNC_RETURN
#define NIRVA_COND_RESULT NIRVA_FUNC_RETURN

  NIRVA_TYPEDEF(NIRVA_CONST_STRING, NIRVA_STRAND)
  NIRVA_TYPEDEF(NIRVA_CONST_STRING_ARRAY, NIRVA_CONST_BUNDLEDEF)
  NIRVA_TYPEDEF(NIRVA_STRING_ARRAY, NIRVA_BUNDLEDEF)

  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_object_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_instance_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_contract_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_transform_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_trajactory_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_tsegment_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_desc_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_thread_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_bundle_t)
  NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_connect_t)

  NIRVA_FUNC_TYPE_DEF(NIRVA_FUNC_RETURN, nirva_function_t, NIRVA_CONST_BUNDLEPTR input,\
		      NIRVA_CONST_BUNDLEPTR outputs)

  NIRVA_TYPEDEF(nirva_function_t, nirva_condfunc_t)
  NIRVA_TYPEDEF(nirva_function_t, nirva_callback_t)

#define NIRVA_NEEDS_EXT_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type ext_func_##funcname(__VA_ARGS__);

/// list of functions which the implementation must define ////

  NIRVA_NEEDS_EXT_FUNC(get_int_value, "get integer value from 'item_name' in 'bundle'", NIRVA_INT, \
		       NIRVA_CONST_BUNDLEPTR bundle, NIRVA_CONST_STRING item_name)
  NIRVA_NEEDS_EXT_FUNC(get_bundleptr_array, "get bundleptr array from 'item_name' in 'bundle'", \
		       NIRVA_BUNDLEPTR_ARRAY, NIRVA_CONST_BUNDLEPTR bundle, \
		       NIRVA_CONST_STRING item_name)
  NIRVA_NEEDS_EXT_FUNC(create_bundle_vargs, "create and return a bundle from bundledef and "\
		       "va_list of 'item_name', 'value' in pairs", NIRVA_BUNDLEPTR, NIRVA_BUNDLEDEF, \
		       NIRVA_VA_LIST)

/////// optional overrides ///////
#define NIRVA_OPT_EXT_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type ext_func_##funcname(__VA_ARGS__);

  NIRVA_OPT_EXT_FUNC(nirva_call, "run a static transform via a function wrapper", \
		     NIRVA_FUNC_RETURN,	NIRVA_CONST_STRING transform_name, NIRVA_VARIADIC)

////// function return codes ////

// error in condition - could not evaluate
#define NIRVA_COND_ERROR		-1
// condition failed
#define NIRVA_COND_FAIL			0
// condition succeeded
#define NIRVA_COND_SUCCESS		1
// request to give more time and retry. If no other conditions fail then cond_once may
// emulate cond_retry
#define NIRVA_COND_WAIT_RETRY		2

// these values should only be returned from system callback ////s
// force conditions to succeed, even if some fail
#define NIRVA_COND_FORCE		16
// force condition fail and do not retry
#define NIRVA_COND_ABANDON		17
/////

#define NIRVA_REQUEST_NO		NIRVA_COND_FAIL
#define NIRVA_REQUEST_YES		NIRVA_COND_SUCCESS
#define NIRVA_REQUEST_WAIT_RETRY	NIRVA_COND_WAIT_RETRY
#define NIRVA_REQUEST_NEEDS_PRIVILEGE	NIRVA_COND_ABANDON

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

	// an intention which takes one or more mutable attributes and produces output
	// either as another mutable attribute
	// - PLAY is based on this, with CAP realtime
	//  (this is INTENTION_PROCESS with a a data out hook and need_data hook)
	OBJ_INTENTION_MANIPULATE_STREAM,

	// an intent which takes mutable data and produces static array output
	//(this is INTENTION_PROCESS with a need_data hook, which produces attribute(s) and / or objects as output)
	OBJ_INTENTION_RECORD,  // record

	// intent INTENTION_PROCESS takes static attribute (array)
	// and produces an object instance of a different type (c.f create instance, used to create objects of the SAME type)
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
#define TX_FLAG_ACTIVE 	       		(1ull < 4)

// indicates that the object has been destroyed, (FINAL_HOOK has been triggerd),
// but the 'shell' is left due to being ref counted
// elements other than flags / destroy_mutex, refcount and uid, are considered invalid
// reference count may not be increased but may be decreased
// if the implemntation supports semi or full thread automation then there will be a destruct_mutex
// to be completely safe
// This should be locked firs prior to reading the state
// then only if it is not a zombie,  with the mutex still locked add a hook to the object / instance
// FINAL_HOOK which sets a pointer to NULL, then unlock teh mutex.
// once the final ref is removed, all elemnts in the bundle except for uid will be removed.
// 

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

/// flags and types start with an "_" so as not to conflict with element names

// we define types and flags once as string "a", and again as char 'a'
// when constructing the strands, we make use of automatic string concatenation, and
// the string versions have to be used.
// in all other places, the type is a uint32_t, so we use (uint32_t)'a'
// ATTR_TYPE_* uses a different set of values. This is done deliberately to highlight the
// fact that element types represent a single data value, whereas ATTR_TYPE is linked to the data
// type of the "VALUE", "DEFAULT" and "NEW_DEFAULT" elements inside the attribute bundle
// the biggest difference is that those elements are POINTERS to the underlying variable, rather
// than representing the variable itself. Doing it this way allows for an attribute to map any native value
// the type is really a void *, ATTR_TYPE is set in the "TYPE" element, and native_type can be used to
// indicate the actual "true" undelrying type

// a void element - do not create until value is set, even if mandatory. When value is set, that defines its type
#define _ELEM_TYPE_NONE					"0"

// standard types for the first letter of strand0
#define _ELEM_TYPE_INT					"i"
#define _ELEM_TYPE_DOUBLE				"d"
#define _ELEM_TYPE_BOOLEAN				"b"
#define _ELEM_TYPE_STRING				"s"
#define _ELEM_TYPE_INT64	       			"I"
#define _ELEM_TYPE_UINT					"u"
#define _ELEM_TYPE_UINT64				"U"

#define _ELEM_TYPE_VOIDPTR				"V"

#define _ELEM_TYPE_FUNCPTR				"F"
#define _ELEM_TYPE_BUNDLEPTR	       			"B"
#define _ELEM_TYPE_BUNDLETYPE_PTR	       		"T" // followed by bundlety

// these flag values can appear as the first 
#define _ELEM_TYPE_FLAG_COMMENT				"#"
#define _ELEM_TYPE_FLAG_DIRECTIVE	       		"@"
#define _ELEM_TYPE_FLAG_OPTIONAL	       		"?"

// for ptrs to bundles, "!TOP_BUNDLE" means ptr to outermost bundle,
// "!MY_CONTAINER" == ptr to bundle containg this one
// "!THIS_BUNDLE" == ptr to current sub-bundle
#define _ELEM_VALUE_VARIABLE				"!"

// a value with & in front represents a pointer (void *) to another element in the same (sub)bundle
#define _ELEM_VALUE_PTR_TO_ELEM				"&"

// since there is the posisibility that more directives may be added in the future,
// if a @START directive is not recognised, the bundle maker will skip to the next @END tag and continue from
// the next strand. However the directiv will be left inside the bundledef if it is stored in a blueprint element

#define DIRECTIVE_BEGIN "@BEGIN "
#define DIRECTIVE_END "@END "

#define DIR_START(dirnm)  DIRECTIVE_BEGIN,#dirnm
#define DIR_FINISH(dirnm) DIRECTIVE_END,#dirnm

// the name of the item to be overriden follows the opening tag
// anything between start and end becomes the default for the item
// it is possible to nest sub bundle and array directives
// inside this. Invalid values will be ignored and the previosuly defined default used instead
#define DIRECTIVE_REPLACE_DEFAULT			DIR_START(repl_def) // "item_name"
// "value_0"
// "value_1"
// "value_2"
// ...
#define DIRECTIVE_REPLACE_DEFAULT_END			DIR_FINISH(repl_def)

// this is used to connect a system callback to a hook point
// the system trigger is always run first before any user added callbacks
// start will be followed by the item name, then the next strand will be flags and data separated by a space
#define DIRECTIVE_ADD_HOOK_TRIGGER     			DIR_START(add_hook_trigger)  // "item_name"
// "flags" "data"
#define DIRECTIVE_ADD_HOOK_TRIGGER_END 		       	DIR_FINISH(add_hook_trigger)

#define ADD_HOOK_TRIGGER(item, hook_type, flags, data) MSTY		\
  ("%s%s %s",DIRECTIVE_ADD_HOOK_TRIGGER, #item, "%d", (nirva_hook_type)(hook_type), "%lu", \
   (flags), "%s",  #data, "%s%s", DIRECTIVE_ADD_HOOK_TRIGGER_END)

#define DIRECTIVE_MAKE_EXCLUSIVE     			DIR_START(make_exclusive) // choice_name
// item_names
// one per line
// pick one only
#define DIRECTIVE_MAKE_EXCLUSIVR_END 		       	DIR_FINISH(make_exclusive)

// when building bundle, add automation hook for each item, which deletes other items
#define MAKE_EXCLUSIVE(group_name,...) MS(DIRECTIVE_MAKE_EXCLUSIVE #group_name, __VA_ARGS__, \
					  DIRECTIVE_MAKE_EXCLUSIVR_END)

/* // array start and array end must in pairs. the values in between represent an array of values */
/* #define DIRECTIVE_ARRAY_START 	       		"@array_start " // followed by array TYPE (e.g. "B") */
/* #define DIRECTIVE_ARRAY_END 				"@array_end" */

// instructs the bundlemaker to rename-in-place an exsitng element
// this can be use to create variants on exsitng bundles
// by renaming alternate choices, or as a measure to prevent naming clashes in cases where this is unvaoidable
// lines between the type tags will be of the form "old_name new_name"
// any item mathching old_name should be renamed to new_name, this should be done in the bujndledef
// and in the bundle. After renaming in the bundledef, the tags and lines between shall be trimmed out
/* #define DIRECTIVE_RENAME_ELEM 				DIR_START(rename_elem) */
/* #define DIRECTIVE_RENAME_ELEM_END 		       	DIR_FINSIHrename_elem) */

// it is possible to nest replace_default + pack_start / pack_end immediately following pack_time_end
// in this case the directives refer to an element inside the previous pack_item

// strand 2

// flag bits, decimal value in strand1
#define _ELEM2_FLAG_ARRAY				1
#define _ELEM2_FLAG_PTR_TO_				2

////////////////////////////

///////// element types ////////

#define ELEM_TYPE_NONE					(uint32_t)'0'	// invalid type

// flag bits
#define ELEM_TYPE_FLAG_OPTIONAL		      		(uint32_t)'?'	// optional element
#define ELEM_TYPE_FLAG_COMMENT		      		(uint32_t)'#'	// comment
#define ELEM_TYPE_FLAG_DIRECTIVE			(uint32_t)'@'	// directive

#define ELEM_TYPE_INT					(uint32_t)'i'	// 4 byte int
#define ELEM_TYPE_DOUBLE				(uint32_t)'d'	// 8 byte float
#define ELEM_TYPE_BOOLEAN				(uint32_t)'b'	// 1 - 4 byte int
#define ELEM_TYPE_STRING				(uint32_t)'s'	// \0 terminated string
#define ELEM_TYPE_INT64	       			     	(uint32_t)'I'	// 8 byte int
#define ELEM_TYPE_UINT					(uint32_t)'u'	// 4 byte int
#define ELEM_TYPE_UINT64				(uint32_t)'U'	// 8 byte int

#define ELEM_TYPE_VOIDPTR				(uint32_t)'V'	// void *
#define ELEM_TYPE_FUNCPTR				(uint32_t)'F'	// pointer to function

// void * aliases
#define ELEM_TYPE_BUNDLEPTR	       			(uint32_t)'B' // void * to other bundle

#ifdef VERSION
#undef VERSION
#endif

#define BUNDLE_NAMEU(a, b) "BUNDLE_" a "_" b
#define ELEM_NAMEU(a, b) "ELEM_" a "_" b
#define CUSTOM_NAMEU(a, b)  a "_" b

#define ELEM_NAME(a, b) ELEM_NAMEU(a, b)

////////////// standard elements ///
#define MACROZ(what, va) MACRO(what __##va##__)
#define MACROX(what) MACROZ(what, VA_ARGS)

/* #define FOR_ALL_DOMAINS(MACRO, ...) MACRO(BASE __VA_ARGS__) MACRO(GENERIC __VA_ARGS__) MACRO(ELEMENT __VA_ARGS__) MACRO(VALUE __VA_ARGS__) MACRO(ATTRIBUTE __VA_ARGS__) MACRO(FUNCTION __VA_ARGS__) \ */
/*     MACRO(THREADS __VA_ARGS__) MACRO(INTROSPECTION __VA_ARGS__) MACRO(ICAP __VA_ARGS__) MACRO(OBJECT __VA_ARGS__) MACRO(HOOK __VA_ARGS__) MACRO(CONTRACT __VA_ARGS__) MACRO(TRANSFORM __VA_ARGS__) \ */
/*     MACRO(ATTRBUNDLE __VA_ARGS__) MACRO(CONDITION __VA_ARGS__) MACRO(LOGIC __VA_ARGS__) */

/* #define FOR_ALL_DOMAINS(MACROX, ...) MACROX(BASE) MACROX(GENERIC) MACROX(ELEMENT) MACROX(VALUE) MACROX(ATTRIBUTE) \ */
/*     MACROX(FUNCTION) MACROX(THREADS) MACROX(INTROSPECTION) MACROX(ICAP) MACROX(OBJECT) MACROX(HOOK) MACROX(CONTRACT) \ */
/*     MACROX(TRANSFORM) MACROX(ATTRBUNDLE) MACROX(CONDITION) MACROX(LOGIC) */

#define FOR_ALL_DOMAINS(MACROX, ...) MACROX(BASE), MACROX(GENERIC), MACROX(INTROSPECTION)
// domain BASE
#define ELEM_SPEC_VERSION				ELEM_NAME("BASE", "VERSION")
#define ELEM_SPEC_VERSION_TYPE              		INT, 100

#define ELEM_BASE_BUNDLE_TYPE
#define ELEM_BASE_BUNDLE_TYPE_TYPE

#define ALL_ELEMS_BASE "VERSION"
#define ALL_BUNDLES_BASE

// domain GENERIC

// const
#define ELEM_GENERIC_NAME				ELEM_NAME("GENERIC", "NAME")
#define ELEM_GENERIC_NAME_TYPE              		STRING, NULL

#define ELEM_GENERIC_FLAGS				ELEM_NAME("GENERIC", "FLAGS")
#define ELEM_GENERIC_FLAGS_TYPE              		UINT64, 0

// const
#define ELEM_GENERIC_UID				ELEM_NAME("GENERIC", "UID")
#define ELEM_GENERIC_UID_TYPE				UINT64, 0

#define ELEM_GENERIC_DESCRIPTION			ELEM_NAME("GENERIC", "DESCRIPTION")
#define ELEM_GENERIC_DESCRIPTION_TYPE              	STRING, NULL

#define ALL_ELEMS_GENERIC "UID", "DESCRIPTION",  "FLAGS", "NAME"
#define ALL_BUNDLES_GENERIC

//// domain ELEMENT

#define ELEM_ELEMENT_HIERARCHY	              	       	ELEM_NAME("ELEMENT", "HIERARCHY")
#define ELEM_ELEMENT_HIERARCHY_TYPE	        	STRING, "ELEM_"

#define ELEM_ELEMENT_PTR_TO	              	       	ELEM_NAME("ELEMENT", "PTR_TO")
#define ELEM_ELEMENT_PTR_TO_TYPE	      		VOIDPTR, NULL

#define ALL_ELEMS_ELEMENT "PTR_TO"
#define ALL_BUNDLES_ELEMENT_TYPE 

//// domain STANDARD - some standard bundle types

#define BUNDLE_STANDARD_ATTR_BUNDLE			ELEM_NAME("STANDARD", "ATTRIBUTE")
#define BUNDLE_STANDARD_ATTR_BUNDLE_TYPE       		ATTR_BUNDLE, NULL

#define BUNDLE_STANDARD_ERROR				ELEM_NAME("STANDARD", "ERROR")
#define BUNDLE_STANDARD_ERROR_TYPE       		EEROR_BUNDLE, NULL

#define BUNDLE_STANDARD_ATTRIBUTE	       		ELEM_NAME("STANDARD", "ATTRIBUTE")
#define BUNDLE_STANDARD_ATTRIBUTE_TYPE			ATTRIBUTE, NULL

#define BUNDLE_STANDARD_ATTR_DESC	       		ELEM_NAME("STANDARD", "ATTR_DESC")
#define BUNDLE_STANDARD_ATTR_DESC_TYPE			ATTR_DESC, NULL

#define BUNDLE_STANDARD_ATTR_DESC_BUNDLE       		ELEM_NAME("STANDARD", "ATTR_DESC_BUNDLE")
#define BUNDLE_STANDARD_ATTR_DESC_BUNDLE_TYPE	       	ATTR_DESC, NULL

#define BUNDLE_STANDARD_CONTRACT	       		ELEM_NAME("STANDARD", "CONTRACT")
#define BUNDLE_STANDARD_CONTRACT_TYPE			CONTRACT, NULL

#define BUNDLE_STANDARD_REFCOUNT       	       		ELEM_NAME("STANDARD", "REFCOUNT")
#define BUNDLE_STANDARD_REFCOUNT_TYPE			REFCOUNT, NULL

#define BUNDLE_STANDARD_CONDITION             		ELEM_NAME("STANDARD", "CONDITION")
#define BUNDLE_STANDARD_CONDITION_TYPE			FUNCTION, NULL

/// domain VALUE - values are common to both attributes and elements
  
#define ELEM_VALUE_DATA		               	       	ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_DATA_TYPE	      			NONE,

// const if !none
#define ELEM_VALUE_TYPE	   	   			ELEM_NAME("VALUE", "VALUE_TYPE")
#define ELEM_VALUE_TYPE_TYPE	   	   		INT, ATTR_TYPE_NONE

#define ELEM_VALUE_DEFAULT	      			ELEM_NAME("VALUE", "DEFAULT")
#define ELEM_VALUE_DEFAULT_TYPE	      			NONE,

#define ELEM_VALUE_NEW_DEFAULT	      			ELEM_NAME("VALUE", "NEW_DEFAULT")
#define ELEM_VALUE_NEW_DEFAULT_TYPE     	       	NONE,

/////////////

#define ELEM_VALUE_MAX_VALUES				ELEM_NAME("VALUE", "MAX_VALUES")
#define ELEM_VALUE_MAX_VALUES_TYPE     		       	INT, -1

#define ELEM_VALUE_INTEGER				ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_INTEGER_TYPE			       	INT, 0

#define ELEM_VALUE_INT					ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_INT_TYPE			       	INT, 0

#define ELEM_VALUE_UINT					ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_UINT_TYPE			       	UINT, 0

#define ELEM_VALUE_DOUBLE				ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_DOUBLE_TYPE			       	DOUBLE, 0.
  
#define ELEM_VALUE_BOOLEAN	      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_BOOLEAN_TYPE   	  	       	BOOLEAN, FALSE

#define ELEM_VALUE_STRING	      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_STRING_TYPE   	  	       	STRING, NULL

#define ELEM_VALUE_INT64		       		ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_INT64_TYPE			       	INT64, 0

#define ELEM_VALUE_UINT64		       		ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_UINT64_TYPE			       	UINT64, 0

#define ELEM_VALUE_VOIDPTR      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_VOIDPTR_TYPE 	    	       	VOIDPTR, NULL

#define ELEM_VALUE_FUNCPTR      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_FUNCPTR_TYPE 	    	       	FUNCPTR, NULL

#define ELEM_VALUE_BUNDLEPTR      			ELEM_NAME("VALUE", "DATA")
#define ELEM_VALUE_BUNDLEPTR_TYPE 	    	       	BUNDLEPTR, NULL

#define BUNDLE_VALUE_MAPPED	              	       	ELEM_NAME("VALUE", "MAPPPED")
#define BUNDLE_VALUE_MAPPED_TYPE              		MAPPED_VALUE, NULL

#define BUNDLE_VALUE_CHANGE	              	       	ELEM_NAME("VALUE", "CHANGE")
#define BUNDLE_VALUE_CHANGE_TYPE              		VALUE_CHANGE, NULL

#define BUNDLE_VALUE_ANY_BUNDLE	              	       	ELEM_NAME("VALUE", "ANY_BUNDLE")
#define BUNDLE_VALUE_ANY_BUNDLE_TYPE          		ANY_BUNDLE, NULL

#define ELEM_VALUE_USER_DATA      			ELEM_NAME("VALUE", "USER_DATA")
#define ELEM_VALUE_USER_DATA_TYPE 	    	       	VOIDPTR, NULL
  
// domain AUTOMATION

#define BUNDLE_AUTOMATION_CONDITION                   	ELEM_NAME("AUTOMATION", "CONDITION")
#define BUNDLE_AUTOMATION_CONDITION_TYPE      		FUNCTION, NULL

#define ELEM_AUTOMATION_SCRIPT                   	ELEM_NAME("AUTOMATION", "SCRIPT")
#define ELEM_AUTOMATION_SCRIPT_TYPE      		STRING, NULL

// domain datetime
  
#define ELEM_DATETIME_TIMESTAMP                   	ELEM_NAME("DATETIME", "TIMESTAMP")
#define ELEM_DATETIME_TIMEPSTAMP_TYPE      		INT64, NULL

#define ELEM_DATETIME_DELTA 	                  	ELEM_NAME("DATETIME", "DELTA")
#define ELEM_DATETIME_DELTA_TYPE 	     		INT64, NULL

#define ELEM_DATETIME_START  	                 	ELEM_NAME("DATETIME", "STOP")
#define ELEM_DATETIME_START_TYPE 	     		INT64, NULL

// domain CASCADE
  
#define BUNDLE_CASCADE_DECISION_NODE			ELEM_NAME("CASCADE", "DECISION_NODE")
#define BUNDLE_CASCADE_DECISION_NODE_TYPE      		CONDVAL_NODE, NULL

#define BUNDLE_CASCADE_CONSTVAL_MAP			ELEM_NAME("CASCADE", "CONSTVAL_MAP")
#define BUNDLE_CASCADE_CONSTVAL_MAP_TYPE      		CONSTVAL_MAP, NULL

#define BUNDLE_CASCADE_MATRIX_NODE			ELEM_NAME("CASCADE", "MATRIX_NODE")
#define BUNDLE_CASCADE_MATRIX_NODE_TYPE	      		CASCMATRIX_NODE, NULL

#define BUNDLE_CASCADE_MATRIX				ELEM_NAME("CASCADE", "MATRIX")
#define BUNDLE_CASCADE_MATRIX_TYPE	      		MATRIX_2D, NULL

#define BUNDLE_CASCADE_CONDLOGIC			ELEM_NAME("CASCADE", "CONDLOGIC")
#define BUNDLE_CASCADE_CONDLOGIC_TYPE			CONDLOGIC, NULL

// domain CASCMATRIX

#define BUNDLE_CASCMATRIX_ON_SUCCESS			ELEM_NAME("CASCMATRIX", "ON_SCUCCESS")
#define BUNDLE_CASCMATRIX_ON_SUCCESS_TYPE		CASCMATRIX_NDOE, NULL

#define BUNDLE_CASCMATRIX_ON_FAIL			ELEM_NAME("CASCMATRIX", "ON_FAIL")
#define BUNDLE_CASCMATRIX_ON_FAIL_TYPE			CASCMATRIX_NDOE, NULL

#define ELEM_CASCMATRIX_OP_SUCESS			ELEM_NAME("CASCMATRIX", "OP_SUCCESS")
#define ELEM_CASCMATRIX_OP_SUCESS_TYPE			INT, CASC_MATRIX_NOOP

#define ELEM_CASCMATRIX_OP_FAIL				ELEM_NAME("CASCMATRIX", "OP_FAIL")
#define ELEM_CASCMATRIX_OP_FAIL_TYPE			INT, CASC_MATRIX_NOOP

#define ELEM_CASCMATRIX_OTHER_IDX			ELEM_NAME("CASCMATRIX", "OTHER_IDX")
#define ELEM_CASCMATRIX_OTHER_IDX_TYPE			INT, -1

// domain MATRIX

#define BUNDLE_MATRIX_ROW				ELEM_NAME("MATRIX", "ROW")
#define BUNDLE_MATRIX_ROW_TYPE				MATRIX_ROW, NULL

// domain LOGIC
#define ELEM_LOGIC_OP

#define ELEM_LOGIC_NOT					ELEM_NAME("LOCIC", "AND")
#define ELEM_LOGIC_NOT_TYPE			       	INT, LOGICAL_AND

#define ELEM_LOGIC_AND					ELEM_NAME("LOCIC", "AND")
#define ELEM_LOGIC_AND_TYPE			       	INT, LOGICAL_AND

#define ELEM_LOGIC_OR					ELEM_NAME("LOCIC", "OR")
#define ELEM_LOGIC_OR_TYPE		    	   	INT, LOGICAL_OR

#define ELEM_LOGIC_XOR					ELEM_NAME("LOCIC", "XOR")
#define ELEM_LOGIC_XOR_TYPE		    	   	INT, LOGICAL_XOR

/// domain ATTRIBUTE

#define BUNDLE_ATTRIBUTE_CONNECTION	       		ELEM_NAME("ATTRIBUTE", "CONNECTION")
#define BUNDLE_ATTRIBUTE_CONNECTION_TYPE  		ATTR_CONNECTION, NULL

// domain FUNCTION

#define BUNDLE_FUNCTION_INPUT		       		ELEM_NAME("HOOK", "INPUT_BUNDLE")
#define BUNDLE_FUNCTION_INPUT_TYPE			FN_INPUT, NULL

#define BUNDLE_FUNCTION_OUTPUT		       		ELEM_NAME("FUNCTION", "OUTPUT_BUNDLE")
#define BUNDLE_FUNCTION_OUTPUT_TYPE			FN_OUTPUT, NULL

// functions are called from source item in caller object
// functions are sent to dest item in target object
#define BUNDLE_FUNCTION_CALLER		       		ELEM_NAME("FUNCTION", "CALLER")
#define BUNDLE_FUNCTION_CALLER_TYPE			OBJECT, NULL

#define BUNDLE_FUNCTION_SOURCE		       		ELEM_NAME("FUNCTION", "SOURCE")
#define BUNDLE_FUNCTION_SOURCE_TYPE			SOURCE, NULL

#define BUNDLE_FUNCTION_TARGET		       		ELEM_NAME("FUNCTION", "TARGET")
#define BUNDLE_FUNCTION_TARGET_TYPE			OBJECT, NULL

#define BUNDLE_FUNCTION_DEST		       		ELEM_NAME("FUNCTION", "DEST")
#define BUNDLE_FUNCTION_DEST_TYPE			ANY_BUNDLE, NULL

#define ELEM_FUNCTION_OBJ_FUNCTION	       	     	ELEM_NAME("FUNCTION", "OBJ_FUNCTION")
#define ELEM_FUNCTION_OBJ_FUNCTION_TYPE		     	FUNCPTR, NULL

#define ELEM_FUNCTION_NATIVE_FUNCTION		     	ELEM_NAME("FUNCTION", "NATIVE_FUNCTION")
#define ELEM_FUNCTION_NATIVE_FUNCTION_TYPE     	     	FUNCPTR, NULL

#define ELEM_FUNCTION_PARAM_NUM				ELEM_NAME("FUNCTION", "PARAM_NUMBER")
#define ELEM_FUNCTION_PARAM_NUM_TYPE	      		INT, 0

#define ELEM_FUNCTION_RETURN_VALUE	       		ELEM_NAME("FUNCTION", "RETURN_VALUE")
#define ELEM_FUNCTION_RETURN_VALUE_TYPE       		INT64, 0

#define BUNDLE_FUNCTION_MAPPING				ELEM_NAME("FUNCTION", "MAPPING")
#define BUNDLE_FUNCTION_MAPPING_TYPE			PMAP_DESC, NULL

// domain THREADS
#define ELEM_THREADS_NATIVE_THREAD		       	ELEM_NAME("THREADS", "NATIVE_THREAD")
#define ELEM_THREADS_NATIVE_THREAD_TYPE        		VOIDPTR, NULL

#define ELEM_THREADS_NATIVE_STATE		       	ELEM_NAME("THREADS", "NATIVE_STATE")
#define ELEM_THREADS_NATIVE_STATE_TYPE        		UINT64, NULL

#define ELEM_THREADS_FLAGS			       	ELEM_NAME("THREADS", "FLAGS")
#define ELEM_THREADS_FLAGS_TYPE 	       		UINT64, NULL

#define ELEM_THREADS_MUTEX				ELEM_NAME("THREADS", "MUTEX")
#define ELEM_THREADS_MUTEX_TYPE				VOIDPTR, NULL

#define BUNDLE_THREAD_INSTANCE				ELEM_NAME("THREAD", "INSTANCE")
#define BUNDLE_THREAD_INSTANCE_TYPE    			THREAD_INSTANCE, NULL

// domain INTROSPECTION
// item which should be added to every bundldef, it is designed to allow
// the bundle creator to store the pared down strands used to construct the bundle
#define ELEM_INTROSPECTION_BLUEPRINT    		ELEM_NAME("INTROSPECTION", "BLUEPRINT")
#define ELEM_INTROSPECTION_BLUEPRINT_TYPE              	STRING, NULL

// as an alternative this can be used instead to point to a static copy of the strands
#define ELEM_INTROSPECTION_BLUEPRINT_PTR    	       	ELEM_NAME("INTROSPECTION", "BLUEPRINT_PTR")
#define ELEM_INTROSPECTION_BLUEPRINT_PTR_TYPE          	VOIDPTR, NULL

#define ELEM_INTROSPECTION_COMMENT    			ELEM_NAME("INTROSPECTION", "COMMENT")
#define ELEM_INTROSPECTION_COMMENT_TYPE       	       	STRING, NULL

#define ELEM_INTROSPECTION_PRIVATE_DATA			ELEM_NAME("INTROSPECTION", "PRIVATE_DATA")
#define ELEM_INTROSPECTION_PRIVATE_DATA_TYPE		VOIDPTR, NULL

#define ELEM_INTROSPECTION_NATIVE_TYPE 			ELEM_NAME("INTROSPECTION", "NATIVE_TYPE")
#define ELEM_INTROSPECTION_NATIVE_TYPE_TYPE 	       	INT, 0

#define ELEM_INTROSPECTION_NATIVE_SIZE	 	       	ELEM_NAME("INTROSPECTION", "NATIVE_SIZE")
#define ELEM_INTROSPECTION_NATIVE_SIZE_TYPE		UINT64, 0

#define ELEM_INTROSPECTION_NATIVE_PTR 			ELEM_NAME("INTROSPECTION", "NATIVE_PTR")
#define ELEM_INTROSPECTION_NATIVE_PTR_TYPE		VOIDPTR, NULL

#define ALL_ELEMS_INTROSPECTION "BLUEPRINT", "BLUEPRINT_PTR", "COMMENT", "REFCOUNT", "PRIVATE_DATA", \
    "NATIVE_PTR", "NATIVE_SIZE", "NATIVE_TYPE"
#define ALL_BUNDLES_INTROSPECTION

//////////// BUNDLE SPECIFIC ELEMENTS /////

///// domain ICAP

#define ELEM_ICAP_INTENTION				ELEM_NAME("ICAP", "INTENTION")
#define ELEM_ICAP_INTENTION_TYPE              		INT, OBJ_INTENTION_NONE

#define ELEM_ICAP_CAPACITY				ELEM_NAME("ICAP", "CAPACITY")
#define ELEM_ICAP_CAPACITY_TYPE              		STRING, NULL

////////// domain OBJECT

#define ELEM_OBJECT_TYPE				ELEM_NAME("OBJECT", "TYPE")
#define ELEM_OBJECT_TYPE_TYPE				UINT64, OBJECT_TYPE_UNDEFINED

#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS	       		ELEM_NAME("OBJECT", "ACTIVE_TRANSFORMS")
#define BUNDLE_OBJECT_ACTIVE_TRANSFORMS_TYPE	       	TRANSFORM, NULL

#define ELEM_OBJECT_SUBTYPE				ELEM_NAME("OBJECT", "SUBTYPE")
#define ELEM_OBJECT_SUBTYPE_TYPE	       		UINT64, NO_SUBTYPE

#define ELEM_OBJECT_STATE				ELEM_NAME("OBJECT", "STATE")
#define ELEM_OBJECT_STATE_TYPE		       		UINT64, OBJ_STATE_TEMPLATE

///// domain HOOK

// this is one of three basic patterns
#define ELEM_HOOK_TYPE					ELEM_NAME("HOOK", "TYPE")
#define ELEM_HOOK_TYPE_TYPE				INT, 0

// after the template callback modified by flags is called
// the value is cascaded by automation, resulting in a differentiatied value
// the second value is then triggered either during or after the first
// custom hooks can be added by adding new cascade conditions mapping to a new value
// depending on the basic pattern the new hook number must be attached to a
// specific element or attribute value (for data hook) or else must map to an update value
// intent (for request hooks), or for spntaneous hooks, this must map to a structure transform
// in include in an existing or custom structure subtype
#define ELEM_HOOK_NUMBER	       			ELEM_NAME("HOOK", "NUMBER")
#define ELEM_HOOK_NUMBER_TYPE				INT, 0

#define ELEM_HOOK_HANDLE				ELEM_NAME("HOOK", "HANDLE")
#define ELEM_HOOK_HANDLE_TYPE				UINT, 0

// the name of the item which triggers the hook
#define ELEM_HOOK_TARGET				ELEM_NAME("HOOK", "TARGET")
#define ELEM_HOOK_TARGET_TYPE				STRING, NULL

#define ELEM_HOOK_FLAGS					ELEM_NAME("HOOK", "FLAGS")
#define ELEM_HOOK_FLAGS_TYPE				UINT64, 0

#define ELEM_HOOK_CB_DATA		       		ELEM_NAME("HOOK", "CB_DATA")
#define ELEM_HOOK_CB_DATA_TYPE				VOIDPTR, NULL

#define ELEM_HOOK_COND_STATE				ELEM_NAME("HOOK", "COND_STATE")
#define ELEM_HOOK_COND_STATE_TYPE	       		BOOLEAN, TRUE

#define BUNDLE_HOOK_CALLBACK		       		ELEM_NAME("HOOK", "CALLBACK")
#define BUNDLE_HOOK_CALLBACK_TYPE			HOOK_CB_FUNC, NULL

#define BUNDLE_HOOK_DETAILS		       		ELEM_NAME("HOOK", "DETAILS")
#define BUNDLE_HOOK_DETAILS_TYPE			HOOK_DETAILS, NULL

#define BUNDLE_HOOK_STACK		       		ELEM_NAME("HOOK", "STACK")
#define BUNDLE_HOOK_STACK_TYPE				HOOK_STACK, NULL

#define BUNDLE_HOOK_TRIGGER		       		ELEM_NAME("HOOK", "TRIGGER")
#define BUNDLE_HOOK_TRIGGER_TYPE       			HOOK_TRIGGER, NULL

#define BUNDLE_HOOK_CB_ARRAY		       		ELEM_NAME("HOOK", "CB_STACK")
#define BUNDLE_HOOK_CB_ARRAY_TYPE      			COND_ARRAY, NULL

// domain CONTRACT

#define ELEM_CONTRACT_FAKE_FUNCNAME			ELEM_NAME("CONTRACT", "FAKE_FUNCNAME")
#define ELEM_CONTRACT_FAKE_FUNCNAME_TYPE		STRING, NULL

#define BUNDLE_CONTRACT_REVPMAP				ELEM_NAME("CONTRACT", "REVPMAP")
#define BUNDLE_CONTRACT_REVPMAP_TYPE	       		PMAP, NULL

#define BUNDLE_CONTRACT_REQUIREMENTS	       		ELEM_NAME("CONTRACT", "REQUIREMENTS")
#define BUNDLE_CONTRACT_REQUIREMENTS_TYPE      		FUNCTION, NULL

// domain TRANSFORM

#define ELEM_TRANSFORM_STATUS 				ELEM_NAME("TRANSFORM", "STATUS")
#define ELEM_TRANSFORM_STATUS_TYPE			INT, TRANSFORM_STATUS_NONE

#define ELEM_TRANSFORM_RESULT 				ELEM_NAME("TRANSFORM", "RESULT")
#define ELEM_TRANSFORM_RESULT_TYPE			INT, TX_RESULT_NONE

#define BUNDLE_TRANSFORM_ICAP 				ELEM_NAME("TRANSFORM", "ICAP")
#define BUNDLE_TRANSFORM_ICAP_TYPE			ICAP, NULL

// domain TRAJECTORY

#define BUNDLE_TRAJECTORY_NEXT_SEGMENT	       		ELEM_NAME("NEXT", "SEGMENT")
#define BUNDLE_TRAJECTORY_NEXT_SEGMENT_TYPE	     	TSEGMENT, NULL

#define BUNDLE_TRAJECTORY_SEGMENT	       		ELEM_NAME("TRAJECTORY", "SEGMENT")
#define BUNDLE_TRAJECTORY_SEGMENT_TYPE	       		TSEGMENT, NULL

/// domain ATTRBUNDLE

#define ELEM_ATTRBUNDLE_MAX_REPEATS 		     	ELEM_NAME("ATTRBUNDLE", "MAX_REPEATS")
#define ELEM_ATTRBUNDLE_MAX_REPEATS_TYPE       		INT, 1

#define BUNDLE_ATTRIBUTE_HOOK_STACK	               	ELEM_NAME("ATTRIBUTE", "HOOK_STACK")
#define BUNDLE_ATTRIBUTE_HOOK_STACK_TYPE       		HOOK_STACK, NULL

//// domain CONDITION - condiitons are simple functions which produce a TRUE / FALSE result

#define ELEM_CONDITION_RESPONSE		       		ELEM_NAME("CONDITION", "RESPONSE")
#define ELEM_CONDITION_RESPONSE_TYPE	       		UINT64, NIRVA_COND_SUCCESS

#define BUNDLE_CONDITION_MAX_ITEMS	       		ELEM_NAME("CONDITION", "MAX_ITEMS")
#define BUNDLE_CONDITION_MAX_ITEMS_TYPE	       		FUNCTION, MAX_ITEMS

#define BUNDLE_CONDITION_HAS_VALUE	       		ELEM_NAME("CONDITION", "HAS_VALUE")
#define BUNDLE_CONDITION_HAS_VALUE_TYPE   		FUNCTION, VALUE_SET

#define BUNDLE_CONDITION_HAS_ITEM	       		ELEM_NAME("CONDITION", "HAS_ITEM")
#define BUNDLE_CONDITION_HAS_ITEM_TYPE   		FUNCTION, HAS_ITEM

#define BUNDLE_CONDITION_VALUES_EQUAL	       		ELEM_NAME("CONDITION", "VALUE_MATCH")
#define BUNDLE_CONDITION_VALUE_MATCH_TYPE   		FUNCTION, VALUE_MATCH

// domain STRUCTURAL

////////////////////////////////////////////////////////////////////////////////////

// whereas ELEMENTS generally pertain to the internal state of a bundle, ATTRIBUTES
// (more precisely, variants of the ATTRIBUTE bundle) are desinged for passing and sharing data between
// bundles. However since both elements can be wrapped by an ELEM bundle, which contains a VALUE bundle,
// and ATTRIBUTES are based around a VALUE bundle, it is possible to treat them similarly in some aspects
// by wrapping the value bundles in a vale_map bundle. Thes bundles also have a HIEARARCHY element
// which will be either HIERARCHY_ELEM or HIERARCHY_ATTRIBUTE accordingly.

// The main differences:
//- ELEMENTS have type, name and data, although these are internal to the element
// - AATTRIBUTES have elements for DATA, NAME and TYPE
// - ATTRIBUTES are bundles composed of several elements, whereas elements are not bundles - they are the building blocks
// for bundles.
// - Elements cna be scalar values or arrays of unlimited size. Attributes may have "repeats", ie. the value can be
// limited to a certain number of data values (i.e. bounded arrays).
// Although elements have a default value when first created, this is generally 0 or NULL, and is not visible
// after the element has been created. Attributes have a default value which must be set in the "default" element.
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

// void * aliase
#define ATTR_TYPE_BUNDLEPTR	       			80 // void * to other bundle

/////////

#define ATTR_NAMEU(a, b) "ATTR_" a "_" b
#define ATTR_NAME(a, b) ATTR_NAMEU(a, b)

//////////////////////////

// domain STRUCTURAL
#define ATTR_STRUCTURAL_SUBTYPES			ATTR_NAME("SRTUCTURAL", "SUBTYPES")
#define ATTR_STRUCTURAL_SUBTYPES_TYPE			OBJECT_INSTANCE, NULL

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
#define ATTR_VIDEO_FRAME_RATE	       			ELEM_NAME(VIDEO, FRAME_RATE)
#define ATTR_VIDEO_FRAME_RATE_TYPE    			DOUBLE, 0.

#define ATTR_VIDEO_DISPLAY_WIDTH  	       		ELEM_NAME(VIDEO, DISPLAY_WIDTH)
#define ATTR_VIDEO_DISPLAY_WIDTH_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_DISPLAY_HEIGHT  	       		ELEM_NAME(VIDEO, DISPLAY_HEIGHT)
#define ATTR_VIDEO_DISPLAY_HEIGHT_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_PIXEL_WIDTH  	       		ELEM_NAME(VIDEO, PIXEL_WIDTH)
#define ATTR_VIDEO_PIXEL_WIDTH_TYPE 	   	 	UINT64, 0

#define ATTR_VIDEO_PIXEL_HEIGHT  	       		ELEM_NAME(VIDEO, PIXEL_HEIGHT)
#define ATTR_VIDEO_PIXEL_HEIGHT_TYPE    	 	UINT64, 0

#define ATTR_VIDEO_COLOR_SPACE  	       		ELEM_NAME(VIDEO, COLOR_SPACE)
#define ATTR_VIDEO_COLOR_SPACE_TYPE 	   	 	INT, 0

#define ATTR_VIDEO_STEREO_MODE  	       		ELEM_NAME(VIDEO, STEREO_MODE)
#define ATTR_VIDEO_STEREO_MODE_TYPE 	   	 	UINT64, 0

#define ATTR_VIDEO_FLAG_INTERLACED  	       		ELEM_NAME(VIDEO, FLAG_INTERLACED)
#define ATTR_VIDEO_FLAG_INTERLACED_TYPE    	 	UINT64, 0

// attribute flag bits //
// these flagbits are for input attributes, for output attributes they are ignored

// when an object contributes an attribute to the pool, gene
#define OBJ_ATTR_FLAG_READONLY 		0x00001
#define OBJ_ATTR_FLAG_OPTIONAL        	0x00002

// value is constant and will never change
#define OBJ_ATTR_FLAG_CONSTANT	 	0x10

// attribute is connected to a remote attribute. The remote attribute value should be read
// in place of this one
#define OBJ_ATTR_FLAG_REMOTE	 	0x10

// indicates that the value will only update when an update transform is called
#define OBJ_ATTR_FLAG_UPDATE    	0x20

// for attributes with an update transform, indicates that the value is correct
// and the update should not be called
#define OBJ_ATTR_FLAG_CURRENT    	0x40

// output value - indicates that the value will ONLY EVER be updated by a transform
// at the end of processing, and / or in hooks during the transform
#define OBJ_ATTR_FLAG_OUTPUT	 	0x100

// indicates that the value may update spontaneously with it being possible to trigger
// data hooks
#define OBJ_ATTR_FLAG_VOLATILE	 	0x200

// each update returns the next value in a sequence, i.e it should only be updated
// once for each read, otherwise, the value returned is the "current value"
#define OBJ_ATTR_FLAG_SEQUENTIAL       	0x400

// indicates the value is a "best guess", the actual value of whatever it represents may differ
#define OBJ_ATTR_FLAG_ESTIMATE       	0x800

// attr connection flags
  
// the value of the target attr should be copied to local when the connection is broken
// if not set, or if copying is not possible, then the attribute will be reset to its default
// value instead
#define ATTR_CONX_FLAG_COPY_ON_DISCONNECT (1ull << 0)

// for optional attributes, indicates the when the connection is broken, the local attribute
// should be removed from the bundle rather than left in place
// in this case any attributes connected to this one will be disconnected first
#define ATTR_CONX_FLAG_DESTROY_ON_DISCONNECT (1ull << 1)

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

// function categories

#define FUNC_CATEGORY_GENERAL		0ull
// the underlying fn type, can only be one of these
#define FUNC_CATEGORY_STANDARD 		1ull // nirva_function_t format available
#define FUNC_CATEGORY_NATIVE 		2ull // native format available
#define FUNC_CATEGORY_SCRIPT 		3ull // script format available
// any mapped versions available, e.g nirva -> native
//				      native -> nirva for some structurals
//					script -> native : symbol for IMPL
#define FUNC_WRAPPER_STANDARD		(1ull << 2)
#define FUNC_WRAPPER_NATIVE		(1ull << 3)
#define FUNC_WRAPPER_SCRIPT		(1ull << 4)

// info about the type, we can have segment, structural, automation, external,
// callback, conditional, placeholder, synthetic

#define FUNC_PURPOSE_OUTSIDE		(1ull << 32) // falls outside of nirva, eg. nirva_init()
#define FUNC_PURPOSE_SEGMENT		(2ull << 32) // function wrapped by on traj. segment
#define FUNC_PURPOSE_STRUCTURAL		(3ull << 32) // represents transform in structural
#define FUNC_PURPOSE_AUTOMATION		(4ull << 32) // some kind of auto script
#define FUNC_PURPOSE_EXTERNAL		(5ull << 32) // an external "implenetion dependant" fuction
#define FUNC_PURPOSE_CALLBACK		(6ull << 32) // function added to a hook cb stack
#define FUNC_PURPOSE_CONDITIONAL       	(7ull << 32) // function which checks a single condition
#define FUNC_PURPOSE_SYNTHETIC      	(8ull << 32) // "functions" like a trajectory
#define FUNC_PURPOSE_PLACEHOLDER      	(9ull << 32) // for reference only - do not call

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
#define TRANSFORM_STATUS_DEFERRED 1
#define TRANSFORM_STATUS_PREPARING 2

// runtime statuses
#define TRANSFORM_STATUS_RUNNING 16	///< transform is "running" and the state cannot be changed
#define TRANSFORM_STATUS_WAITING 17	///< transform is waiting for conditions to be satisfied
#define TRANSFORM_STATUS_PAUSED	 18	///< transform has been paused, via a call to the pause_hook

// transaction is blocked, action may be needed to return it to running status
#define TRANSFORM_STATUS_BLOCKED 18	///< transform is waitin and has passed the blocked time limit

// final statuses
#define TRANSFORM_STATUS_SUCCESS 32	///< normal / success
#define TRANSFORM_STATUS_CANCELLED 33	///< transform was cancelled via a call to the cancel_hook
#define TRANSFORM_STATUS_ERROR  34	///< transform encountered an error during running
#define TRANSFORM_STATUS_TIMED_OUT 35	///< timed out waiting for data

// results returned from actioning a transform
// in the transform RESULTS item
// negative values indicate error statuses
#define TX_RESULT_ERROR -1
#define TX_RESULT_CANCELLED -2
#define TX_RESULT_DATA_TIMED_OUT -3
#define TX_RESULT_SYNC_TIMED_OUT -4

// segment was missing some data which should have been specified in the contract
// the contract should be adjusted to avoid this
// an ADJUDICATOR object may flag the contract as invalid until updated
#define TX_RESULT_CONTRACT_WAS_WRONG -5

// a data connection was broken and not replaced
#define TX_RESULT_CONTRACT_BROKEN -6

// SEGMENT_END was not listed as a possible next segment
// and no conditions were met for any next segment
// the trajectory or contract should be adjusted to avoid this
// an ADJUDICATOR object may flag the contract as invalid until updated
#define TX_RESULT_TRAJECTORY_INVALID -7

#define TX_RESULT_INVALID -8

#define TX_RESULT_NONE 0

#define TX_RESULT_SUCCESS 1

// failed, but not with error
#define TX_RESULT_FAILED 2

// transform is idling and may be continued by trigering the RESUME_REQUEST_HOOK
#define TX_RESULT_IDLING 3

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

#define HOOK_FLAG_SELF			(1ull << 0) // only the object istself may add callbacks

// hook caller will continue to rety conditions until they succeed
// or the hook times out
#define HOOK_FLAG_COND_RETRY		(1ull << 1)

// conditions will be checked once only on fail the operation triggering the hook
// will be abandoned. For request hooks, REQUEST_NO will be returned
#define HOOK_FLAG_COND_ONCE		(1ull << 2)

// for DATA_HOOK_TYPE - this indicate the hook should be tirgger before the change is made
// if present then this is VALUE_UPDATING, else it is UPDATED_VALUE (the default DATA_HOOK_TYPE)
#define HOOK_FLAG_BEFORE		(1ull << 3)

// indicates a request type hook, rather than being triggered by a data change, request some
// other object change its data or allows some action
// caller will receive either REQUEST_NO - the request is denied, REQUEST_YES - the request
// is accepted, or REQUEST_WAIT_RETRY - the requst is denied temporarily, caller can retry
// and it may be accepted later
#define HOOK_FLAG_REQUEST	      	(1ull << 4)

// spontaneous

// indicates the data which is in the bundle_in is ready for previewing and or / editing
// the transform will call hooks in sequence to give each observer a chance to edit
#define HOOK_FLAG_DATA_PREP     	(1ull << 5)

// data in bundle_in is in its final format, all hook cbs will be called in parallel
// if possible. The transform may continue so it can start processing the next data
// however the data in bundle_in will not be altered or freed until all calbacks have returned
#define HOOK_FLAG_DATA_READY     	(1ull << 7)

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
#define HOOK_FLAG_IDLE			(1ull << 8)

// hook was triggered by some underlying condition, eg. a native signal rather than being
// generated by the application
#define HOOK_FLAG_NATIVE      		(1ull << 9)

NIRVA_TYPEDEF(NIRVA_ENUM _hook_patterns, {
				   DATA_HOOK_TYPE,
				   REQUEST_HOOK_TYPE,
				   SPONTANEOUS_HOOK_TYPE,
				   N_HOOK_PATTERNS,
				     } nirva_hook_type)

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
      INIT_HOOK, // obeject state / after

      // conditions:
      // IS_EQUAL(GET_BUNDLE_VALUE_UINT64(BUNDLE_IN, "FLAGS"), 0)
      // STRING_MATCH(GET_BUNDLE_VALUE_STRING(BUNDLE_IN, "NAME"), ELEM_OBJECT_STATE)
      // IS_EQUAL(GET_BUNDLE_VALUE_INT(GET_SUB_BUNDLE(GET_SUB_BUNDLE(BUNDLE_IN, "CHANGED"),
      // "NEW_VALUE"), OBJ_STATE_NORMAL))
      // NEW_HOOK_TYPE = INIT_HHOK, PMAP: 0, GET_BUNDLE_VALUE_BUNDLEPTR(BUNDLE_IN, OBJECT), 1,
      // GET_BUNDLE_VALUE_VOIDPTR(CALLBACK, DATA), -1 GET_PTR_TO(BUNDLE_OUT, RETURN_VALUE) 

      // object suffered a FATAL error or was aborted,
      FATAL_HOOK,

      // state changing from normal -> not ready, i.e. restarting
      RESETTING_HOOK,  // object state / before

      // object is about to be freed
      DESTRUCTION_HOOK, // object state / before

      // object subtype changed
      SUBTYPE_CHANGED_HOOK,

      N_OBJECT_HOOKS,

#define N_GLOBAL_HOOKS N_OBJECT_HOOKS

      VALUE_INITING_HOOK,
      // called when the value being changed does not exist
      // 
      //NIRVA_NOT(HAS_VALUE,object,item)
      //NIRVA_HAS_FLAGBIT(INPUT,"FLAGS",BEFORE)

      INITED_VALUE_HOOK,
      //NIRVA_NOT(HAS_VALUE,object,item)
      //NIRVA_NOT(NIRVA_HAS_FLAGBIT,INPUT,"FLAGS",BEFORE)

      VALUE_FREEING_HOOK,
      // called when the value being changed does not exist
      //NIRVA_NOT(HAS_ITEM,OUTPUT,VALUE_CHANGE,NEW_VALUE)
      //NIRVA_HAS_FLAGBIT(INPUT,"FLAGS",BEFORE)

      FREED_VALUE_HOOK,
      //NIRVA_NOT(HAS_ITEM,OUTPUT,VALUE_CHANGE,NEW_VALUE)
      //NIRVA_NOT(NIRVA_HAS_FLAGBIT(INPUT,"FLAGS",BEFORE))

      // DATA_HHOK + HOOK_FLAG_BEFORE
      VALUE_UPDATING_HOOK,

      UPDATED_VALUE_HOOK,

      // associated with transaction status change

      PREPARING_HOOK,  /// none -> prepare

      PREPARED_HOOK,  /// prepare -> running

      TX_START_HOOK, /// any -> running

      ///
      PAUSED_HOOK, ///< transform was paused via pause_hook

      ///< transform was resumed via resume hook (if it exists),
      // and / or all paused hook callbacks returning
      RESUMING_HOOK,

      TIMED_OUT_HOOK, ///< timed out in hook - need_data, sync_wait or paused

      /// tx transition from one trajectory segment to the next
      // in some cases there may be a choice for the next vector, and an 'abritrator' may be
      // required in order to decide which route to take
      // (TBD)
      SEGMENT_END_HOOK,

      // this is triggered when a new trajectory segment begins
      // for the inital segment, TX_START is triggered instead
      SEGMENT_START_HOOK,

      FINISHED_HOOK,   /// running -> finished -> from = we can go to SUCCESS, ERROR, DESTRUCTION, etc.,
      COMPLETED_HOOK,   /// finished with no errors, end results achieved

      ///< error occured during the transform
      // if the object has a transform to change the status back to running this should be actioned
      // when the FINISHED_HOOK is called
      // otherwise if there is a transform to return the status to normal this should be actioned instead
      // and TX_RESULT_ERROR returned
      // otherwise do nothing, and let the transform return TX_RESULT_ERROR
      ERROR_HOOK,

      CANCELLED_HOOK, ///< tx cancelled via cancel_hook, transform will return TX_RESULT_CANCELLED

      // SPONTANEOUS HOOKS
      // if for some reason a Transform cannot be started immediately this hook should be triggered
      // for example, the Transform may be queued and waiting to be processed
      TX_DEFERRED_HOOK,

      INSTANCE_COPIED_HOOK, // TBD

      // this is a "self hook" meaning the object running the transform only should append to this
      // the things appended are not normal hook callbacks, instead they are CONDITION bundles
      SYNC_WAIT_HOOK, ///< synchronisation point, transform is waitng until all hook functions return

      ///< tx is blocked, may be triggered after waiting has surpassed a limit
      // but hasnt yet TIMED_OUT
      //
      // applies to SYNC_WAIT, DATA_REQUEST, TSEGMENT

      // an ARBITRATOR object can attempt to remedy the situation,
      // for SYNC_WAIT this implies finding which Conditions are delaying and attempting to remedy this
      // for DATA_REQUEST this implies hceking why the data provider is delaying, and possibly finding
      // a replacement
      // for SEGMENT, the arbitrator may force the Transform to resume, if multiple next segments are causing the
      // delay it may select which one to follow next, preferring segment_end if avaialble, to complete the transform

      TX_BLOCKED_HOOK,

      // calbacks for the following two hooks are allowed to block "briefly" so that data can be copied
      // or edited. The hook callback will receive a "max_time_target" in input, the value depends
      // on the caller and for data prep this is divided depending on the number of callbacks remaing
      // to run. The chronometer may help with the calculation.
      // objects which delay for too long may be penalized, if they persistently do so

      // tx hooks not associated ith status changes
      // this hook may be triggered in a Transform data in its "raw" state is ready
      // if the data is read / write then the EDIT_PRE hook is called
      // if the data is readonly then DATA_PREVIEW_HOOK is called

      EDIT_PREP_HOOK,

      DATA_PREP_HOOK,

      // data in its "final" state is ready
      // data is readonly. The Transform will call all callbacks in parallel and will not block
      // however it will wait for all callbacks to return before freeing / altering the data

      DATA_READY_HOOK,

      // 2 hooks which are triggered when a remot obejct attaches hook
      // these are self hooks, and are not triggered by self hooks
      // this is to allow automation - attached is sent to the receiver
      // detached is sent to the caller, so it can remove the local pointers
      // when an instance is finalized it will release all of its hook stacks, after calling final hook
      HOOK_ATTACHED_HOOK,

      // this hook is triggered in the PASSIVE party, for exmaple if an object is freed
      // this hook is called for all objects with hooks attached
      // if an obejct removes a hook callback from a remote object the hook is trigger in remote
      HOOK_DETACHED_HOOK,

      // hook is triggered in an object if a message bundle has been placed in its
      // message queue. This normally occurs after successful negotiation of a contract
      // when the caller would like the target to run the transform itself
      // if thread_model is on-demand, the queued message will be retrieved by the thread herder
      // and placed in its own queue instead, where it will wait to be assinged to a pool thread
      // if chronometry subsystem support is enabeled, contracts may be placed in chronometry;s
      // message queue, together with an attribute indicate an action time,
      // whether it repeats and so on.
      MSG_QUEUED_HOOK,

      // this hook is triggered when a queued contract is processed in from an object's message queue
      // caller is the object processing and source is the contract
      // target is the object that originally placed it in the queue
      MSG_RECEIVED_HOOK,

      // this hook triggers when an attribute connection is made
      // for attributes connected TO anothr, this implies that the remote attribute is
      // if refcounting is semi or full
      ATTR_CONNECT_HOOK,

      // this hook triggers when an attribute connection is broken
      // for attributes connected TO anothr, this implies that the remote attribute is
      // about to be freed
      // for attributes connected from another this is for information, however the local
      // attribute will lose 1 added reference, possibly freeing it
      ATTR_DISCONNECT_HOOK,
      
      // this is triggered during a transform if a breach of contract terms is detected,
      // generally the arbitrator would attach automatically to this hook
      // if it fails to correct the situation, it may be passed on to the adjudicator
      // mark the contract breaker as "untrustworthy" log the incident, and set ERROR
      // status for the transform
      CONTRACT_BREACHED_HOOK,
      
      // thread running the transform received a fatal signal
      THREAD_EXIT_HOOK,

      // this can be used for debugging, in a function put NIRVA_CALL(tripwire, "Reason")
      // the hook stack will be held in one or other structural subtypes
      TRIPWIRE_HOOK,
      
      // REQUEST HOOKS - tx will provide hook callbacks which another object can trigger
      // if the target object is not active, or is busy, the automation may respond as a proxy

      // every object instance has a default, no negotiate contract with intent INTENTION_REQUEST_UPDATE
      // this should b actioned with the hook type passed in the input bundle, elemet "REQUEST_TYPE"
      // type INT. the response will be in output bundle element "RESPONSE" (UINT64)
      // if RESPONSE_WAIT_RETRY is received, then the tranform may be rerried after a short
      // pause

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
      // equivalent to request_update on the bundle;s REFECOUNT.REFCOUNT and a target 'value' of -1
      // (in this case the "value" becomes an offset)
      UNREF_REQUEST_HOOK,

      // if the transform exposes this hook, then this request can be triggered after or
      // during a transform. If accepted, the target
      // will reverse the pervious data update either for the attribute of for the previous transform
      // target / value TBD. If the transform cannot be undone further,
      // NIRVA_REPSONE_NO should be returned
      UNDO_REQUEST_HOOK,

      // if the transform exposes this hook, then this request can be triggered after or
      // during a transform. If accepted, the target
      // will reverse the pervious data undo either for the attribute of for the previous transform
      // target / value TBD. If the transform cannot be redone further,
      // NIRVA_REPSONE_NO should be returned
      REDO_REQUEST_HOOK,

      // this is called BEFORE a remote attribute connects to a local attribute
      // this is a SELF hook, meaning only the object owning the attribute may add callbacks
      // returng FALSE blocks the connection rather than removing the callback
      // so the hook callback must be removed manually
      //
      // if the connection is not blocked, the default hook adds a reference to remote_attr and
      // local_attr
      // and remote attr maps a pointer to the local attr. The reomote attribute
      // gets the same flags as the local one
      // plus the CONNECTED flagbits. The prior data is not freed,
      // however when the attribute disconnects
      // target is attribute connections_in. array, data is ptr to local attr
      ATTR_CONNECT_REQUEST,

      // requests that the attribute value be updated
      // if the TX is being run to provide data input for another Transfomr
      // calling this will request new data in the linkedd attribute
      // only the object running may call this
      // target is attribute "value", value is not created
      UPDATE_REQUEST_HOOK,
      //
      // if the app has an arbitrator, an object bound to a contract may tigger this, and on
      // COND_SUCCESSm the attribute will be disconnected with no penalties
      // target is local attribute "value", value is NULL
      SUBSTITUTE_REQUEST_HOOK,
      //
      // target is transform "status", value is cancelled
      CANCEL_REQUEST_HOOK, // an input hook which can be called to cancel a running tx
      //
      // ask the transform to pause processing. May not happen immediately (or ever, so add a callback
      // for the PAUSED hook)
      // will wait for all callbacks to return, and for unpause hook (if it exists) to be called
      // target is transform "status", value is pausedd
      PAUSE_REQUEST_HOOK,
      // if this hook exists, then to unpause a paused transform this must be called and all paused
      // callbacks must have returned (may be called before the functions return)
      // after this, the unpaused callbacks will be called and processing will only continue
      // once all of those have returned. Calling this  when the tx is not paused or running unpaused
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

#define ATTR_UPDATED_HOOK DATA_READY_HOOK
#define ATTR_DELETE_HOOK FINAL_HOOK
#define RESTART_HOOK RESETTING_HOOK

#define UPDATED_VALUE_HOOK DATA_HOOK_TYPE

/////////////////////////// bundles //
/*  SPECIAL RULES: */
/*  1.	For the items, ELEM_VALUE_DATA, ELEM_VALUE_DEFAULT and ELEM_VALUE_NEW_DEFAULT, */
/*  the data type is determined by the value of the ELEM_VALUE_TYPE item */
/*  which MUST be present in the same bundle. The number of data elements which can be set is */
/*  defined by the ELEM_VALUE_MAX_REPEATS item, which may optionally be present in the same */
/*  bundle. If ELEM_VALUE_DEFAULT is set and ELEM_VALUE_DATA is not set, then ELEM_VALUE_DEFAULT */
/*  should copied to ELEM_VALUE_DATA. */

/*  2.	For item ELEM_GENERIC_UID, a randomly generate uint64_t number should be generated */
/*  as the default value and never changed.	If a bundle containing this element is copied, */
/*  then a new random value shall be generated for the copy bundle. */

/* 3. When setting the value of any element, check if the bundle contains hook_triiger bundles */
/* if so and target matches, call data hooks before and after. */
/* if before hook returns NIRVA_COND_FAILED, do not change the value. */

/* // flag bits may optionally be used to store information derived from the strands */

NIRVA_TYPEDEF(NIRVA_ENUM _hook_patterns, nirva_hook_type)

#define ELEM_FLAG_ARRAY 		(iull << 0)	// denotes the data type is array
#define ELEM_FLAG_PTR_TO_SCALER        	(iull << 1)	// denotes element is a pointer to specified type
#define ELEM_FLAG_PTR_TO_ARRAY        	(iull << 2)	// denotes element is a pointer to specified type
#define ELEM_FLAG_OPTIONAL 		(1ull << 3)	// denotes the entry is optional
#define ELEM_FLAG_CONST 		(1ull << 4)	// value is set to default, then can only be set once more
#define ELEM_FLAG_COMMENT 		(1ull << 5)
#define ELEM_FLAG_DIRECTIVE 		(1ull << 6)

#define _GET_TYPE(a, b) _ELEM_TYPE_##a
#define _GET_ATYPE(a, b) _ATTR_TYPE_##a
#define _GET_BUNDLE_TYPE(a, b) a##_BUNDLE_TYPE
#define _GET_DEFAULT(a, b) #b
#define _CALL(MACRO, ...) MACRO(__VA_ARGS__)

#define GET_ELEM_TYPE(xdomain, xitem) _CALL(_GET_TYPE, ELEM_##xdomain##_##xitem##_TYPE)
#define GET_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, ELEM_##xdomain##_##xitem##_TYPE)

#define GET_BUNDLE_TYPE(xdomain, xitem) _CALL(_GET_BUNDLE_TYPE, BUNDLE_##xdomain##_##xitem##_TYPE)
#define GET_BUNDLE_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, BUNDLE_##xdomain##_##xitem##_TYPE)

#define GET_ATTR_TYPE(xdomain, xitem) _CALL(_AGET_TYPE, ATTR_##xdomain##_##xitem##_TYPE)
#define GET_ATTR_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, ATTR_##xdomain##_##xitem##_TYPE)

#define JOIN(a, b) GET_ELEM_TYPE(a, b) ELEM_##a##_##b  //ELEM_NAMEU(#a, #b)
#define JOIN2(a, b, c) GET_ELEM_TYPE(a, b) ELEM_NAMEU(#a, #c)
#define JOIN3(a, b, c, d, e) GET_ELEM_TYPE(a, b) ELEM_NAMEU(#d, #e) " " #c

#define AJOIN(a, b) GET_ATTR_TYPE(a, b) ATTR_##a##_##b  //ELEM_NAMEU(#a, #b)
#define AJOIN2(a, b, c) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#a, #c)
#define AJOIN3(a, b, c, d, e) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#d, #e) " " #c

#define PJOIN3(a, b, c, d, e) GET_ELEM_TYPE(a, b) BUNDLE_NAMEU(#d, #e) " " #c
#define XJOIN3(a, b, c, d, e) GET_ELEM_TYPE(a, b) CUSTOM_NAMEU(#d, #e) " " #c
#define BJOIN3(a, b, c, d, e) GET_ELEM_TYPE(a, b) BUNDLE_NAMEU(#d, #e) " " #c

#define _ADD_STRAND(domain, item) JOIN(domain, item)
#define _ADD_ASTRAND(domain, item) AJOIN(domain, item)
#define _ADD_STRANDn(domain, item, name) JOIN2(domain, item, name)
#define _ADD_ASTRANDn(domain, item, name) AJOIN2(domain, item, name)
//#define _ADD_NAMED_STRAND(xd, xi, typename, domain, item) JOIN3(xd, xi, typename, domain, item)
#define _ADD_NAMED_PSTRAND(xd, xi, typename, domain, item) PJOIN3(xd, xi, typename, domain, item)
#define _ADD_NAMED_XSTRAND(xd, xi, typename, domain, item) XJOIN3(xd, xi, typename, domain, item)
#define _ADD_NAMED_BSTRAND(xd, xi, typename, domain, item) BJOIN3(xd, xi, typename, domain, item)  
#define _ADD_OPT_STRAND(domain, item) "?" JOIN(domain, item)
#define _ADD_OPT_STRANDn(domain, item, name) "?" JOIN2(domain, item, name)
#define _ADD_OPT_ASTRAND(domain, item) "?" AJOIN(domain, item)
#define _ADD_OPT_ASTRANDn(domain, item, name) "?" AJOIN2(domain, item, name)

#define MS(...) make_strands("", __VA_ARGS__, NULL)
#define MSTY(...) make_strands(__VA_ARGS__, NULL)

//#define RENAME_ELEM(...) MS_PAIR(DIRECTIVE_RENAME_ELEM, __VA_ARGS__, DIRECTIVE_RENAME_ELEM_END);
//#define MAKE_PACK(...) MS(__VA_ARGS__)
#define ADD_ATTR(domain, name) INCLUDE_SUB_BUNDLE(domain, name, ATTRIBUTE)

// overwwrite existing default for item
#define REPL_DEF(elemname, newval, ...) MS(DIRECTIVE_REPLACE_DEFAULT" "#elemname, "" __VA_ARGS__, \
					   DIRECTIVE_REPLACE_DEFAULT_END)

#define _ADD_STRAND2(domain, item) ("0 " GET_DEFAULT(domain, item))
#define _ADD_STRAND2a(domain, item) ("1 " GET_DEFAULT(domain, item))
#define _ADD_STRAND2p(domain, item) ("2 " "((void *)0)")
#define _ADD_STRAND2pa(domain, item) ("3 " "((void *)0)")

#define ADD_BUNDLE(td, ti, bt, d, i) MS(_ADD_NAMED_PSTRAND(td, ti, bt, d, i), _ADD_STRAND2p(td, ti) )
#define ADD_BUNDLE_ARR(td, ti, bt, d, i) MS(_ADD_NAMED_XSTRAND(td, ti, bt, d, i), _ADD_STRAND2pa(td, ti) )

#define INC_BUNDLE_ARR(td, ti, d, i, n) MS(_ADD_NAMED_BSTRAND(td, ti, GET_BUNDLE_TYPE(d,i), d, i), \
					   _ADD_STRAND2a(td, ti))

#define INC_BUNDLE(td, ti, d, i, n) MS(_ADD_NAMED_BSTRAND(td, ti, GET_BUNDLE_TYPE(d,i), d, i), \
				       _ADD_STRAND2(td, ti))

#define CRE_BUNDLE(td, ti, d, i, n) MS("+" _ADD_NAMED_BSTRAND(td, ti, GET_BUNDLE_TYPE(d,i), d, i), \
				       _ADD_STRAND2(td, ti))

#define INC_NAMED_BUNDLE_ARR(td, ti, bt, d, i, n) MS(_ADD_NAMED_BSTRAND(td, ti, \
									GET_BUNDLE_TYPE(d,i), d, n), \
						     _ADD_STRAND2a(td, ti))

////////////////////////// BUNDLEDEF "DIRECTIVES" ////////////////////////////////////////
// + ATTRBUNDLE
#define ADD_ELEM(d, i)			 	    	MS(_ADD_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_ATTR(d, i)			 	    	MS(_ADD_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_NAMED_ELEM(d, i, n)		       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_ARRAY(d, i) 			    	MS(_ADD_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_ARRAY(d, i, n) 	       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2a(d,i))

#define ADD_OPT_ELEM(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_OPT_ATTR(d, i) 				MS(_ADD_OPT_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_NAMED_OPT_ELEM(d, i, n)   	       	    	MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_OPT_NAMED_ELEM(d, i, n) 			ADD_NAMED_OPT_ELEM(d, i, n)
#define ADD_OPT_ARRAY(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_OPT_ARRAY(d, i) 	       		MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2a(d,i))
#define ADD_OPT_NAMED_ARRAY(d, i)			ADD_NAMED_OPT_ARRAY(d, i)
#define ADD_COND_ATTR(d, i, ...) 			MS(DIRECTIVE_COND_ATTR, \
							   _ADD_OPT_ASTRAND(d,i), \
							   _ADD_ASTRAND2(d,i), \
							   __VA_ARGS__,DIRECTIVE_COND_ATTR_END))

#define ADD_COMMENT(text)				MS("#" text)

// include all elements from bundle directly
#define EXTEND_BUNDLE(BNAME) 				BNAME##_BUNDLEDEF
#define EXTENDS_BUNDLE(BNAME)				EXTEND_BUNDLE(BNAME)

// ptr to bundle and array of ptrs
#define INCLUDE_BUNDLES(domain, item)			INC_BUNDLE_ARR(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_BUNDLES(domain, item)			INCLUDE_BUNDLES(domain, item)
#define INCLUDE_NAMED_BUNDLES(domain, item, name)      	INC_BUNDLE_ARR(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLES(domain, item, name)    	INCLUDE_NAMED_BUNDLES(domain, item, name)

#define INCLUDE_BUNDLE(domain, item)  		    	INC_BUNDLE(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_BUNDLE(domain, item)			INCLUDE_BUNDLE(domain, item)
#define INCLUDE_NAMED_BUNDLE(domain, item, name)      	INC_BUNDLE(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLE(domain, item, name)      	INCLUDE_NAMED_BUNDLE(domain, item, name)

// ptr to bundleptr and ptr to array of bundleptrs
#define ADD_BUNDLE_PTR(domain, item, btype) 		ADD_BUNDLE(VALUE, BUNDLEPTR, btype##_BUNDLE, domain, item)
#define ADD_BUNDLE_ARRAY_PTR(domain, item, btype) 	ADD_BUNDLE_ARR(VALUE, BUNDLEPTR, btype##_BUNDLE, domain, item)

#endif // !SKIP_MAIN

//// PREDEFINED BUNDLEDEFS //////////////

#ifdef IS_BUNDLE_MAKER
// bundle_type should be set to the toplevel bundle type
// uid should be set to a random id
// optionally can hold API version and a name for the bundledef
// MUST be extended by ALL bundles either directly or indirectly
#define _DEF_BUNDLE ADD_NAMED_ELEM(VALUE, UINT64, BUNDLE_TYPE), ADD_ELEM(GENERIC, UID),	\
    ADD_OPT_ELEM(BASE, VERSION), ADD_OPT_ELEM(GENERIC, NAME)

#define _KERNEL_BUNDLE EXTEND_

// this is for bundles which want to include the bundledef, this is necessary for any bundles
// with a non standard bundledef, or a pointer to bundledef, which details optional items
// andy bundles which allow additon or deletion of opt elements need to extend this
#define _CORE_BUNDLE EXTEND_BUNDLE(DEF),				\
    ADD_OPT_ELEM(INTROSPECTION, BLUEPRINT), ADD_OPT_ELEM(INTROSPECTION, BLUEPRINT_PTR)

// a little more extensive than core, this also includes API version, comment and priv data
// useful for bundles which may have many copies created, or for long term storage
#define _BASE_BUNDLE EXTEND_BUNDLE(CORE),				\
    ADD_OPT_ELEM(INTROSPECTION, COMMENT), ADD_OPT_ELEM(INTROSPECTION, PRIVATE_DATA)

// wraps a native ptr / size / type
#define _MAPPED_VALUE_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_ELEM(INTROSPECTION, NATIVE_TYPE), \
    ADD_OPT_ELEM(INTROSPECTION, NATIVE_SIZE), ADD_NAMED_ELEM(INTROSPECTION, NATIVE_PTR, VALUE)

/* A bundle that is used for attribute VALUE. Initially (unless directed otherwise), DATA is not created
   and VALUE (from the extended MAPPED_VALUE) is set to NULL. Type may have been 

*/
#define _VALUE_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(MAPPED_VALUE), ADD_OPT_ELEM(VALUE, DATA)

// may contain "before" and "after" values for data_hook, as well as being useful for
// comparison functions
#define _VALUE_CHANGE_BUNDLE ADD_BUNDLE_ARRAY_PTR(VALUE, OLD, VALUE), ADD_BUNDLE_ARRAY_PTR(VALUE, NEW, VALUE)

// maps a single native function paramater to an attr_desc
#define _PMAP_DESC_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(FUNCTION, PARAM_NUM), \
    ADD_BUNDLE_PTR(MAPPED, ATRRTIBUTE, ATTR_DESC)

// the "live" version of the preceding, we now link the actual parameter "value"
// to an atribute
#define _PMAP_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_PTR(PMAP, TEMPLATE, PMAP_DESC) \
    INCLUDE_BUNDLE(VALUE, MAPPED), INCLUDE_BUNDLE(STANDARD, ATTRUBUTE)

// TODO
#define _ERROR_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(GENERIC, DESCRIPTION)

#define _CONDVAL_NODE_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLE(AUTOMATION, CONDITION), \
    ADD_BUNDLE_PTR(CASCADE, CONSTVAL, CONSTVAL_MAP), ADD_BUNDLE_PTR(NEXT, SUCCESS, CONDVAL_NODE) \
    ADD_BUNDLE_PTR(NEXT, FAIL, CONDVAL_NODE), ADD_NAMED_OPT_ELEM(VALUE, DOUBLE, P_SUCCESS), \
    ADD_NAMED_OPT_ELEM(VALUE, DOUBLE, P_FAIL)

#define _CONDLOGIC_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(LOGIC, OP),	\
    ADD_BUNDLE_PTR(CONDITON, CONDA, CONDVAL_NODE), ADD_BUNDLE_PTR(LOGIC, CONDB, CONDVAL_NODE)

// one of these may have NULL for condlogic, this defines a default value
#define _CONSTVAL_MAP_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLE(VALUE), \
    INCLUDE_BUNDLES(CASCADE, CONDLOGIC)

#define _CASCMATRIX_NODE_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(CASCMATRIX, OP_SUCCESS), \
    ADD_ELEM(CASCMATRIX, OP_FAIL), ADD_BUNDLE_PTR(CASCMATRIX, OTHER_IDX), \
    INCLUDE_BUNDLE(CASCMATRIX, ON_SUCCESS), INCLUDE_BUNDLE(CASCMATRIX, ON_FAIL) 

#define _BUNDLE_ARRAY EXTEND_BUNDLE(DEF) ADD_ARRAY(ELEM_VALUE_BUNDLEPTR)

#define _MATRIX_2D_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLES(MATRIX, ROW)
  
#define _CASCADE_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(GENERIC, NAME), ADD_ELEM(GENERIC, UID), \
    ADD_OPT_ELEM(GENERIC, DESCRIPTION), ADD_ELEM(GENERIC, FLAGS),	\
    INCLUDE_BUNDLES(CASCADE, DECISION_MAP), INCLUDE_BUNDLES(CASCADE, CONSTVALMAP), \
    INCLUDE_BUNDLES(STANDARD, MATRIX_2D)
  
// this bundle describes an attribute, but is "inert" - has no value or connections
// attr_desc is to attributes, what object_template is to object instance
#define _ATTR_DESC_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(GENERIC, NAME), ADD_ELEM(GENERIC, UID), \
    ADD_OPT_ELEM(GENERIC, DESCRIPTION), ADD_ELEM(GENERIC, FLAGS), ADD_ELEM(VALUE, TYPE), \
    ADD_OPT_ELEM(VALUE, MAX_VALUES), ADD_ELEM(VALUE, DEFAULT), ADD_OPT_ELEM(VALUE, NEW_DEFAULT)

/* a bundle that defines a standard object attribute */
/* we extend am attribute_base and include a VALUE sub-bundle. This wraps the actual implementation
   dependant "actual" value, and adds hooks; "default" and "new_default" are also VALUE bundles,
   the hook will check the type matches. */
// when setting item "value", actually we want to set element value in the value_mapped sub bundle of that name
// the dafault hook stack types are data hooks, attr_connect_request, attr_conect, att_disconnect
// data request, data_ready. data_prep
// if several atributes are updated together they can be added to a bundle
// and the hook stacks embeded in there
#define _ATTRIBUTE_BUNDLE EXTEND_BUNDLE(ATTR_DESC), EXTEND_BUNDLE(VALUE), \
    ADD_BUNDLE_PTR(PARENT, TEMPLATE, ATTR_DESC),			\
    INCLUDE_NAMED_BUNDLES(ATTRIBUTE, CONNECTION, CONNECTIONS_IN),	\
    INCLUDE_BUNDLE(STANDARD, REFCOUNT), INCLUDE_NAMED_BUNDLE(ATTRIBUTE, CONNECTION, CONNECTION_OUT), \
    INCLUDE_BUNDLES(ATTRIBUTE, HOOK_STACK)

// this is a sub-bundle of attribute and serves to map a remote attibute
// when the 'value' of attr is to be read, the value returned is that of the remote attribute
// the target attribute is itself connected then it is up to the application to follow the chain
// this is to avoid taking too much time or getting stuck in loops
// detached hook is a self hook, automation will add a callback which handles this
// in a manner dependant on flags
// the automation will also add a data_hook - before, if the value of rremote_attribute will
// change then detached hook will be triggered in the remote attribute first
// making a connection also refs local and remote, the ref is removed when detached
#define _ATTR_CONNECTION_BUNDLE ADD_BUNDLE_PTR(REMOTE, ATTRIBUTE, ATTRIBUTE), \
    ADD_OPT_ELEM(GENERIC, FLAGS), INCLUDE_NAMED_BUNDLE(HOOK, STACK, DETATCHED_HOOK)

// a bundle comprised of metadata and array of bundle_desc - this can be used as a replacement
// for the strand based bundledefs
#define _ATTR_DESC_BUNDLE_BUNDLE EXTEND_BUNDLE(BASE), INCLUDE_NAMED_BUNDLES(STANDARD, ATTR_DESC, ITEMS)

// a bundle comprised of metadata and array of bundles
#define _ATTR_BUNDLE_BUNDLE EXTEND_BUNDLE(BASE), INCLUDE_NAMED_BUNDLES(STANDARD, ATTRIBUTE, ATTRS)

#define _ATTR_CHANGE_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_ARRAY_PTR(ATTRS, SRC, ATTRIBUTE), ADD_BUNDLE_ARRAY_PTR(ATTRS, DEST, ATTRIBUTE)

// data in sent to hook callbacks, and to transform actions
// for hook callbacks, hook details is used, for data hooks, change, for transform hooks, contract
// for negociate_contract also conrtract
#define _FN_INPUT_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_PTR(INPUT, TARGET_OBJECT, OBJECT_INSTANCE), \
    ADD_BUNDLE_PTR(INPUT, CALLER_OBJECT, OBJECT_INSTANCE),		\
    ADD_BUNDLE_PTR(INPUT, TARGET_DEST, ANY_BUNDLE), ADD_BUNDLE_PTR(INPUT, CALLER_SOURCE, ANY_BUNDLE), \
    INCLUDES_BUNDLES(FUNCTION, PMAP_IN), INCLUDE_NAMED_BUNDLE(HOOK, DETAILS, HOOK_DETAILS), \
    INCLUDE_NAMED_BUNDLE(VALUE, CHANGE, VALUE_CHANGE), ADD_BUNDLE_PTR(LINKED, CONTRACT, CONTRACT), \
    ADD_NAMED_OPT_ELEM(VALUE, VOIDPTR, USER_DATA), ADD_BUNDLE_PTR(INPUT, ATTR_BUNDLE, ATTR_BUNDLE)

// this is the minimal output from a FUNCTION. RESPONSE is COND_FAIL, COND_SUCCESS etc for
// a condition, REQUEST_YES, REQUEST_NO, etc for a request hook
// for other hook callbacks response is LAST or AGAIN
// for Transforms the repsonse for the extend bundle is a transform result
#define _FN_OUTPUT_BUNDLE EXTEND_BUNDLE(DEF), INCLUDES_BUNDLES(FUCNTION, PMAP_OUT), \
    ADD_NAMED_ELEM(FUNCTION, RETURN_VALUE, RESPONSE)

#define _LOCATOR_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_ELEM(GENERIC, DESCRIPTION),	\
    ADD_OPT_ELEM(LOCATOR, UNIT), ADD_OPT_ELEM(LOCATOR, SUB_UNIT), ADD_OPT_ELEM(LOCATOR, INDEX), \
    ADD_OPT_ELEM(LOCATOR, SUB_INDEX), INCLUDE_BUNDLE(STANDARD, VALUE)

// holds  function native or object, mapping can be set to map values to params
// amongst the possibilities an instance can be used directly to call a function (native or
// obj. this can map to anctual real function or to a transform
// in the cas of a real fnction, we take bundle in and map it to native params if necessary
// in case of a transform "proxy" we can have nirv_call("funcname", params...)
// nirv_call will find the equivalent transform for "funcname", and use reverse pmapping to
// create bundle in - this is mainly for simple stucture_instance static transforms
// prime or some other stuctural can build a cascade for the sting -> tx match
// then nirv_call will get the tx from cascade passing in funcname and object / contract set in
// out bundle allows actioning. In case of pmap mismatch nirv_call can set caller in error, and
// the error bundle will contain the params, the pmap or similar

#define _FUNCTION_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_ELEM(FUNCTION, NAME), \
    ADD_ELEM(FUNCTION, CATEGORY), INCLUDE_NAMED_BUNDLE(STANDARD, LOCATOR, CODE_REF), \
    ADD_OPT_ELEM(FUNCTION, OBJ_FUNCTION), ADD_OPT_ELEM(FUNCTION, NATIVE_FUNCTION), \
    ADD_OPT_ELEM(VALUE, SCRIPT), INCLUDE_BUNDLES(FUNCTION, MAPPING)

// each function will be called in the manner depending on the flags in the corresponding stack_header
// for native_funcs, PMAP also comes from the hook stack header
// timestamp may be set when the added to the queue
#define _HOOK_CB_FUNC_BUNDLE EXTEND_BUNDLE(FUNCTION), ADD_ELEM(DATETIME, TIMESTAMP), \
    ADD_ELEM(HOOK, HANDLE), ADD_ELEM(HOOK, CB_DATA)

// hook stacks are created either in an instance (for self, spontaneous hooks, as well as for data
// hooks reating to elemnts. For data_hooks of attributes, the hook pair before / after are created in
// the attribute bundle.
// type denotes the  basic patterns - data_update, request, and spontaneous
// in the automations, 
// the flags combine with this to produce the more detailed "hook number". For transform
// STATUS,
// utility bundllee for describing each type of hook function, keyed by hook_number
#define _HOOK_DETAILS_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(HOOK, TYPE), ADD_ELEM(HOOK, NUMBER), \
    ADD_ELEM(HOOK, FLAGS), INCLUDE_BUNDLES(FUNCTION, MAPPING)

#define _HOOK_STACK_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(HOOK, TYPE), ADD_ELEM(HOOK, NUMBER), \
    ADD_NAMED_OPT_ELEM(HOOK, TARGET, ITEM_NAME),			\
    ADD_ELEM(HOOK, FLAGS), ADD_OPT_ARRAY(AUTOMATION, SCRIPT),		\
    INCLUDE_BUNDLES(FUNCTION, MAPPING), ADD_ELEM(HOOK, COND_STATE),	\
    INCLUDE_BUNDLE(FUNCTION, INPUT), INCLUDE_BUNDLE(FUNCTION, OUTPUT)

#define _ANY_BUNDLE_BUNDLE EXTEND_BUNDLE(DEF), ADD_NAMED_ELEM(VALUE, BUNDLEPTR, BUNDLE)

#define _OBJECT_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_PTR(OBJECT, TEMPLATE, OBJECT_TEMPLATE), \
    ADD_BUNDLE_PTR(OBJECT, INSTANCE, OBJECT_INSTANCE), MAKE_EXCLUSIVE(TEMPLATE, INSTANCE)
  
// for templates the subtype is always SUBTYPE_NONE, and the state wiil be STATE_NONE */
// hook stacks should be created when an item is added ffor data_change of any element
// (before and after) plus init and free, plus the idle_hook stack
// plus destroy_hook
#define _OBJECT_TEMPLATE_BUNDLE EXTEND_BUNDLE(BASE), ADD_ELEM(OBJECT, TYPE), \
    ADD_ELEM(OBJECT, SUBTYPE), ADD_ELEM(OBJECT, STATE),			\
    INCLUDE_BUNDLES(STANDARD, ATTRIBUTE), INCLUDE_BUNDLES(STANDARD, CONTRACT), \
    INCLUDE_NAMED_BUNDLES(OBJECT, ACTIVE_TRANSFORMS, TRANSFORMS), ADD_ELEM(GENERIC, FLAGS)

// this bundle is include in object_instance and attribute
// refcount value starts at 0, and can be increased or decreased by triggering
// ref_request and unref_request hooks. For passive (no thread_instance) object instances
// when refcount goes below 0, the object instance will be zombified (see below)
// if the app enables RECYCLER then it will respond to FINAL_HOOKs
// for full garbage collection, the object elements will be reset to default values
// if the bundle is active, and the ref goes below zero, the effects depend on the status
// if the status is running, then a destroy_request will be triggered, this is a self hook
// it is up to the thread how to handle the destroy request. either it will cancel the transform
// or finish it and then
// generic_uid is a cross check to enure only objects which added a ref can remove one
// when a ref is added, the caller uid should be added to the array, and on unref, a uid should
// be removed. If a caller tries to unref withou addind a ref first, this may be reported
// to the adjudicator who can take appropriate action, including zombifying or recycling

#define _REFCOUNT_BUNDLE EXTEND_BUNDLE(DEF), ADD_ELEM(INTROSPECTION, REFCOUNT), \
    ADD_NAMED_ELEM(THREADS, MUTEX, COUNTER_MUTEX), ADD_NAMED_ELEM(VALUE, BOOLEAN, SHOULD_FREE), \
    ADD_HOOK_TRIGGER(REFCOUNT, DATA_HOOK_TYPE, HOOK_FLAG_BEFORE, &SHOULD_FREE),	\
    INCLUDE_NAMED_ELEM(THREADS, MUTEX, DESTRUCT_MUTEX), ADD_ARRAY(GENERIC, UID), \
    INCLUDE_NAMED_BUNDLES(HOOK, CB_POINTER, HOOK_CALLBACK)

// base bundle for object instances
/* subtype and state are created as ATTRIBUTES so we can add hooks to them */
// these bundles should not be created directly, a template object should do that via a CREATE_INSTANCE intent
// or an instance may create a copy of itself vie the CREATE_INSTANCE intent.
// object instances must also have contracts for the ADD_REF and UNREF intents, which should be flagged
// no-negotiate. The transform for these should simply increase the refcount, or decreas it respectively.
// If the refcount falls below its default value, the bundle should be freed
// we add hook_triggers for subtype and state - since these are simple elemnts the triggers
// will embed in the instance bundle
// the hook stacks are for request and spontaneous hook types
// passive lifecycle
#define _OBJECT_INSTANCE_BUNDLE ADD_BUNDLE_PTR(OBJECT, TEMPLATE, OBJECT_TEMPLATE), \
    EXTEND_BUNDLE(OBJECT_TEMPLATE), INCLUDE_BUNDLE(STANDARD, REFCOUNT),	\
    INCLUDE_NAMED_BUNDLES(THREAD, INSTANCE, THREAD_INSTANCE),		\
    ADD_HOOK_TRIGGER(SUBTYPE, DATA_HOOK_TYPE, 0, NULL),	ADD_HOOK_TRIGGER(STATE, DATA_HOOK_TYPE, 0, NULL)

#define _THREAD_INSTANCE_BUNDLE EXTEND_BUNDLE(OBJECT_INSTANCE), ADD_ELEM(THREADS, NATIVE_THREAD), \
    ADD_BUNDLE_PTR(ACTIVE, TRANSFORM, TRANSFORM), ADD_ELEM(THREADS, FLAGS), \
    ADD_ELEM(THREADS, NATIVE_STATE)

#define _STORAGE_BUNDLE ADD_COMMENT("A utility bundle for storing things"), EXTEND_BUNDLE(DEF), \
    ADD_OPT_ELEM(GENERIC, UID), EXTEND_BUNDLE(MAPPED_VALUE), ADD_OPT_ELEM(INTROSPECTION, COMMENT)

/* // a list 'header' with owner uid and pointer to attr_list_bundle */
#define _OWNED_ATTRS_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_PTR(OWNER, OBJECT, OBJECT), \
    ADD_BUNDLE_ARRAY_PTR(LIST, DATA, ATTRIBUTE)

#define _ATTR_POOL_BUNDLE EXTEND_BUNDLE(DEF), ADD_BUNDLE_PTR(LIST, NEXT, ATTR_POOL), \
    ADD_BUNDLE_PTR(OWNED, LIST, OWNED_ATTRS)

/* // intent / capacities bundle */
#define _ICAP_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_ELEM(GENERIC, DESCRIPTION), ADD_ELEM(ICAP, INTENTION), ADD_ARRAY(ICAP, CAPACITY)

// for semi / full automation, the requiremnts are parsed by the automation
#define _CONTRACT_BUNDLE EXTEND_BUNDLE(OBJECT_INSTANCE), INCLUDE_BUNDLE(TRANSFORM, ICAP), \
    ADD_BUNDLE_PTR(ATTRIBUTE, POOL, ATTR_POOL), INCLUDE_BUNDLES(CONTRACT, REQUIREMENTS), \
    ADD_OPT_ELEM(CONTRACT, FAKE_FUNCNAME), INCLUDE_BUNDLE(CONTRACT, REVPMAP)

//** bundle for an array which checks conditions before allowing an item to be added
#define _COND_ARRAY_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLES(AUTOMATION, CONDITION), \
    INCLUDE_NAMED_BUNDLES(VALUE, ANY_BUNDLE, ARRAY),			\
    ADD_HOOK_TRIGGER(ARRAY, DATA_HOOK_TYPE, (HOOK_FLAG_BEFORE | HOOK_FLAG_COND_ONCE), &CONDITIONS) 

// place holder
// transform result is based on the status after all segments have been actioned
#define _TRANSFORM_BUNDLE EXTEND_BUNDLE(OBJECT_INSTANCE),		\
    ADD_BUNDLE_PTR(TRANSFORM, CONTRACT, CONTRACT),			\
    ADD_ELEM(TRANSFORM, STATUS), ADD_HOOK_TRIGGER(STATUS, DATA_HOOK_TYPE, 0, 0), \
    ADD_ELEM(TRANSFORM, RESULT)

#define _TRAJECTORY_CONDITION_BUNDLE INCLUDE_BUNDLES(STANDARD, CONDITION), \
    ADD_BUNDLE_PTR(NEXT, SEGMENT, TSEGMENT)

#define _TSEGMENT_BUNDLE INCLUDE_BUNDLES(TRAJECTORY, CONDITIONAL), ADD_ELEM(GENERIC, FLAGS)

#define _TRAJECTORY_BUNDLE ADD_ELEM(GENERIC, UID), INCLUDE_BUNDLES(TRAJECTORY, SEGMENT), \
    EXTEND_BUNDLE(FUNCTION)

/// HOOK_CB MACROS
// CHECK(CONDITIONS_ARRAY_ITEM_NAME) // for cond_once aborts the action / returns request_no
// CONDITIONS:
//

/////////////////////////// THE BUNDLE MAKER //////

// takes a sequence of char * and returns a null terminated array
// strands are copied directly, so only the array itself should be freed
#ifdef DEBUG_BUNDLE_MAKER
#define p_debug(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef HAVE_MAKE_STRANDS_FUNC
#define HAVE_MAKE_STRANDS_FUNC
#define ADD_MAKE_STRANDS_FUNC static char *make_strand(const char *fmt,va_list ap){ \
    char *st;size_t strsz;strsz=vsnprintf(0,0,fmt,ap);			\
    st=malloc(++strsz);vsnprintf(st,strsz,fmt,ap);return st;}		\
  const char **make_strands(const char*fmt,...){			\
    const char**sts=0,*xfmt;int ns=1;va_list ap;va_start(ap,fmt);	\
    while((xfmt=*fmt?fmt=va_arg(ap,char*):xfmt)){vsnprintf(0,0,xfmt,ap);ns++;};va_end(ap); \
    sts=malloc(ns*sizeof(char*));ns=0;va_start(ap,fmt);			\
    for((xfmt=*fmt?va_arg(ap,char*):xfmt);(sts[ns]=make_strand(xfmt,ap));ns++); \
    va_end(ap);sts[ns++]=0;return sts;}
#endif

// this function should be called with a bundle name, followed by a sequence of char **
// each char ** array must be NULL terminated
// the final parameter must be NULL
// the result will be a const char ** which joins all the other arrays into 1, ignoring te final NULLS
// if name is not NULL, a comment #name will be prepended
// and in every case, a terminating NULL string will be appended to the final array
// and the amalgameted array returned
// thus it is possible to call, for example:
// 	const char **bdef = make_bundledef("main bundledef", make_strands(a, b, NULL),
//	 make_bundledef("sub bundledef", make_strands(c, d, NULL), NULL), NULL);
// this would return an array {"#main bundledef", a, b, "#sub bundledef", c, d, NULL}
// in this manner it is possible to nest bundldefs (macro EXTEND_BUNDLE works like this)
// In the latest version the second param can be NULL or a prefix 
#ifndef HAVE_MAKE_BUNDLEDEF_FUNC
#define HAVE_MAKE_BUNDLEDEF_FUNC
#define ADD_MAKE_BUNDLEDEF_FUNC						\
  const char **make_bundledef(const char*n,const char*pfx,...){		\
    char**sT=0;int nsT=0;va_list ap;if(n&&*n){size_t Cln=strlen(n)+2;	\
      char*str=malloc(Cln);snprintf(str,Cln,"#%s",n);sT=(char**)make_strands("",str,0);nsT++; \
      p_debug("\nGenerating bundle definition for %s\n",n);va_start(ap,pfx); \
      while(1){int nC=0;char**newsT=va_arg(ap,char**);if(!newsT)break;	\
	while(newsT[++nC]);;sT=realloc(sT,(nsT+nC+1)*sizeof(char*));	\
	for(nC=0;newsT[nC];nC++){size_t strsz=snprintf(0,0,"%s%s",pfx?pfx:"",newsT[nC]); \
	  p_debug("Adding strand:\t%s\n",newsT[nC]);if(strsz){sT[nsT]=malloc(++strsz); \
	    snprintf(sT[nsT++],strsz,"%s%s",pfx?pfx:"",newsT[nC]);free(newsT[nC]);}}}va_end(ap); \
      p_debug("Adding,strand, bundledef complete\n\n");sT[nsT]=0;return(const char**)sT;}return 0;}
#endif

// pick EXACTLY ONE.c, .cpp file per project to be the "bundle maker"
// In that file, (re) include this header with IS_BUNDLE_MAKER defined first
// Then, near the start of the file, outside of all functions, put the lines:
// 		DEFINE_CORE_BUNDLES
//
// then in one function in the same file, (possibly a function like init_bundles()), add
// 		INIT_CORE_BUNDLES

#define NIRVA_IMPLFUNC "IMPLFUNC"
#define _IMPL(func, ...) (IMPL_FUNC_##func(__VA_ARGS__))

#ifndef IMP_FUNC_VA_START
#define IMP_FUNC_VA_START(a, b) va_start(a,b)
#endif
#ifndef IMP_FUNC_VA_END
#define IMP_FUNC_VA_END(a) va_end(a)
#endif

// find_tx_by_name
// make_bundle_attr_bundle
// nirva_recycle
// nirva_revmap_vargs
// set_value_bundleptr
// cond_check_reqs
// tx_action

//IMPL_nirva_call, IMPL_make_bundle_in, IMPL_make_bundle_out, IMPL_get_value_type
//IMPL_nirva_recycle, IMPL_va_start, IMPL_va_end, IMPL_va_arg
// IMPL_get_value_bundleptr

// NIRVA_ERRHCK
// NIRVA_RETRY_PAUSE

/// condfuncs //
#define NIRVA_EQUAL(a,b) ((a) == (b))
#define NIRVA_IF(osp, a, b, c) do {if (op(a,b)) c;} while (0);
//////////////////

#define NIRVA_DO do {
#define NIRVA_WHILE(x) } while(x);

#define NIRVA_PAIRS(n, ...) _NIRVA_PAIRS_##n(__VA_ARGS__)

#define _NIRVA_PAIRS_3(a,b, ...) _NIRVA_PAIRS_1(a,b), _NIRVA_PAIRS_2(__VA_ARGS__)
#define _NIRVA_PAIRS_2(a,b, ...) _NIRVA_PAIRS_1(a,b), _NIRVA_PAIRS_1(__VA_ARGS__)
#define _NIRVA_PAIRS_1(a,b) a b

#define NIRVA_DEF_FUNC(rtype, fname, np, ...) rtype fname(NIRVA_PAIRS(np, __VA_ARGS__)) {
#define NIRVA_FUNC_END }
#define NIRVA_DEF_VARS(type, ...) type __VA_ARGS__;
#define NIRVA_ASSIGN_EXT(var, func, ...) var = _IMPL(func, __VA_ARGS__);
#define NIRVA_ASSIGN(var, val) var = val;
#define NIRVA_ASSERT(val, func, ...) if (!val) func(__VA_ARGS__);
#define NIRVA_RETURN(res) return res;
#define NIRVA_ASSIGN_FROM_ARRAY(val, array, idx) val = array[idx];
#define NIRVA_EXT_FUNC(funcname) ext_func_##funcname
#define NIRVA_ARRAY_FREE(array) free(array);

#define __INTERNAL_make_bundle(n,...)					\
  b##n = _IMPL_create_bundle_vargs(bdef##n,_INTERNAL_split_vargs(n,___VA_ARGS__)); \
#define _INTERNAL_make_3_bundles(b1,b2,b3,bdef1,bdef2,bdef3,...)	\
  _INTERNAL_make_2_bundles(b1,b2,bdef1,bdef2,__VA_ARGS__);		\
  __INTERNAL_make_bundle_vargs(3,__VA_ARGS__);
#define _INTERNAL_make_2_bundles(b1,b2,bdef1,bdef2,...)	\
  __INTERNAL_make_bundle_vargs(1 __VA_ARGS__);		\
  __INTERNAL_make_bundle_vargs(2,__VA_ARGS__);
#define _INTERNAL_make_n_bundles(n,...) _INTERNAL_make_##n##_bundles(__VA_ARGS__)
#define _INTERNAL_make_func_bundles(b_in,b_out,...)			\
  _INTERNAL_make_n_bundles(2,b_in,b_out,FN_INPUT,FN_OUTPUT,__VA_ARGS__)

#define INTERNAL_FUNC(name, ...) _INTERNAL_##name(__VAR_ARGS__)
#define NIRVA_ASSIGN_INTERNAL(var, name, ...) INTERNAL_FUNC(name, var, __VAR_ARGS__)

#define _INTERNAL_get_attribute(attr, bundle, attr_name)		\
  NIRVA_DO NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attrs)			\
    NIRVA_ASSIGN_EXT(attrs, get_bundleptr_array, bundle, ELEM_STANDARD_ATTRIBUTE) \
    NIRVA_ASSIGN_EXT(attr, find_attr_by_name, attrs, attr) NIRVA_WHILE(0)

#define NIRVA_ASSERT_RANGE(var, min, mx, onfail, ...) if (var < min || var > mx) onfail(__VA_ARGS__)



#define _INTERNAL_get_subsystem(subsystem, subtype) NIRVA_DO		\
  NIRVA_ASSERT_RANGE(subtype, 1, N_STRUCT_SUBTYPES - 2, NIRVA_FAIL, NIRVA_NULL)	\
    NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, structurals, attrs)		\
    NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, subsystem)				\
    NIRVA_ASSIGN_INTERNAL(attr, get_attribute, STRUCTURE_PRIME, ATTR_STRUCTURAL_SUBSYSTEM) \
    NIRVA_ASSIGN_EXT(structurals, get_array_bundleptr, from_attr_value(attr)) \
    NIRVA_ASSIGN_FROM_ARRAY(subsystem, structurals, subtype)		\
    NIRVA_ARRAY_FREE(structurals) NIRVA_WHILE(0)

#define NIRVA_DET_IMPLFUNC(substruct, name)			\
  NIRVA_EXT_FUNC(set_attr_funcptr, substruct,			\
		 ATTR_NAME(NIRVA_IMPL_FUNC, #name), func)

#define NIRVA_DEF_STRUCTURE_FUNC(substruct)			\
  NIRVA_EXT_FUNC(set_attr_funcptr, substruct,			\
		 ATTR_NAME(NIRVA_STRUCT_FUNC, #name), NULL)

#define NIRVA_SET_STRUCTURE_FUNC(substruct, name)		\
  NIRVA_EXT_FUNC(set_attr_funcptr, substruct,			\
		 ATTR_NAME(NIRVA_STRUCT_FUNC, #name), name)

////////////////////////////////////////////////////////////////
///////// IMPL_FUNCS - soft version  
  
#define _INTERNAL_set_structural_app_funcs				\
  NIRVA_DO								\
  NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, sub, attr)				\
    NIRVA_ASSIGN_INTERNAL(structure_app, get_subsystem, STRUCTURE_SUBTYPE_APP) \
    NIRVA_DEF_IMPLFUNC(structure_app, get_int_value)			\
    NIRVA_DEF_IMPLFUNC(structure_app, get_bundleptr_array)		\
    NIRVA_DEF_IMPLFUNC(structure_app, create_bundle_vargs)		\
    NIRVA_DEF_IMPLFUNC(structure_app, get_value_type)			\
    NIRVA_DEF_IMPLFUNC(structure_app, find_attr_by_name)		\
    NIRVA_DEF_IMPLFUNC(structure_app, find_attr_by_name)		\
    NIRVA_DEF_IMPLFUNC(structure_app, set_attr_funcptr)			\
    NIRVA_DEF_IMPLFUNC(structure_app, split_vargs)			\
    NIRVA_DEF_IMPLFUNC(structure_app, make_bundle_attr_bundle)		\
									\
    NIRVA_SET_STRUCTURE_FUNC(structure_app, IMPL_FUNC_nirva_call)      	\
    NIRVA_SET_STRUCTURE_FUNC(structure_app, IMPL_FUNC_get_bundledef_for_type) \
    NIRVA_SET_STRUCTURE_FUNC(structure_app, IMPL_FUNC_tx_action)      	\
    NIRVA_SET_STRUCTURE_FUNC(structure_app, IMPL_FUNC_nirva_revmap_vargs) \
    NIRVA_WHILE(0)
  
#ifndef IMPL_FUNC_nirva_call
#define IMPL_FUNC_nirva_call _def_nirva_call
#define _DEPLOY_def_nirva_call						\
  NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, _def_nirva_call, 2, NIRVA_CONST_STRING, \
		 funcname, NIRVA_VARIADIC,)				\
    NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, caller, xtarget, b_in, b_out, contract, attrs, revpmap) \
    NIRVA_DEF_VARS(NIRVA_VA_LIST. vargs)				\
    NIRVA_DEF_VARS(NIRVA_COND_RESULT, condres)				\
    NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, response)				\
    /* self_instance is the thread_instance */				\
    NIRVA_ASSIGN(caller, self_instance)					\
    NIRVA_ASSERT(caller, NIRVA_FAIL, NIRVA_NULL)			\
    INTERNAL_FUNC(make_func_bundles, b_in, b_out, ELEM_FUNTION_NAME,	\
		  funcname, NIRVA_NULL, NIRVA_NULL)			\
    NIRVA_ASSERT(b_in, NIRVA_FAIL, NIRVA_NULL)				\
    NIRVA_ASSERT(b_out, NIRVA_FAIL, NIRVA_NULL)				\
    /* IMPL_FUNC_find_tx_by_name - takes input "func_name" from input and returns	*/ \
    /*    "target_object" and "target_dest" in output - in this case target_dest is the contract */ \
    NIRVA_ASSIGN_EXT(xtarget, get_value_bundleptr, b_out, ELEM_TARGET_OBJECT) \
    NIRVA_ASSERT(xtarget, NIRVA_FAIL, NIRVA_NULL)			\
    NIRVA_ASSIGN_EXT(contract, get_value_bundleptr, b_out, ELEM_TARGET_DEST) \
    NIRVA_ASSERT(contract, NIRVA_FAIL, NIRVA_NULL)			\
    NIRVA_EXT_FUNC(nirva_recycle, b_in)					\
    NIRVA_EXT_FUNC(nirva_recycle, b_out)				\
    NIRVA_ASSIGN_EXT(b_in, make_bundle_fn_input, CALLER_OBJECT, caller, CALLER_SOURCE, \
		     TARGET_OBJECT, xtarget, CONTRACT, contract, NIRVA_NULL) \
    NIRVA_ASSERT(b_in, NIRVA_FAIL, NIRVA_NULL)				\
    /* NIRVA_COMMENT use pmap in tx to map  b_in, if  b_out in parms, */ \
    /*it rets so we can pass to next */					\
    /* NIRVA_COMMENT on fail it will set ELEM_STANDARD_ERROR in attrs */ \
    /*  some contracts (static) have a reverse pmapping, so we can optionally present them */ \
    /* 	   like native functions via nirv_call */			\
    NIRVA_ASSIGN_EXT(revpmap, get_value_bundleptr, contract, ELEM_CONTRACT_REVPMAP) \
    /* we cannot assert revpmap because some tx might not have in / out and just return a value */ \
    /* make an attr_bundle to receive the params, and point b_in.attrs at it */	\
    NIRVA_ASSIGN_EXT(attrs, make_bundle_attr_bundle, 0)			\
    NIRVA_ASSERT(attrs, NIRVA_FAIL, NIRVA_NULL)				\
    NIRVA_EXT_FUNC(set_value_bundleptr, b_in, ELEM_STANDARD_ATTR_BUNDLE, attrs) \
    NIRVA_VA_START(vargs, funcname)					\
    /* if params include output, then it is returned so we can pass it to tx */	\
    NIRVA_ASSIGN_EXT(b_out, nirva_rev_map_vargs, attrs, revpmap, vargs) \
    NIRVA_VA_END(vargs)							\
    NIRVA_ERR_CHK(attrs, ELEM_STANDARD_ERROR, NIRVA_FAIL, NIRVA_NULL)	\
    /* if it is async, then we ask t_bun to run it (TODO) */		\
    NIRVA_ERR_CHK(b_in, ELEM_STANDARD_ERROR, NIRVA_FAIL, NIRVA_NULL)	\
    NIRVA_DO								\
    /* even though it is no_neg, we need to run a cond_check to see if we can run it */	\
    NIRVA_CONDCHECK(condres, NIRVA_SUB_BUNDLE(contract, ELEM_CONTRACT_REQUIREMENTS), \
		    b_in, b_out)					\
    NIRVA_IF(NIRVA_EQUAL, res, COND_WAIT_RETRY, NIRVA_RETRY_PAUSE)	\
    NIRVA_WHILE(NIRVA_EQUAL, res, COND_WAIT_RETRY)			\
    NIRVA_IF(NIRVA_EQUAL, condres, COND_FAIL, NIRVA_FAIL, NIRVA_NULL)	\
    NIRVA_ASSIGN_EXT(result, tx_action, xtarget, contract, b_in, b_out)	\
    NIRVA_EXT_FUNC(nirva_recycle, b_in)					\
    NIRVA_RETURN(result)						\
    NIRVA_FUNC_END 
#endif

#define NIRVA_CALL(fname, ...) NIRVA_EXT_FUNC(nirva_call, #fname, __VA_ARGS__)

#define ADDC(text) "#" text
#define ADDS(text) ADDC(#text)

#define XMB(name, pfx,  ...) make_bundledef(name, pfx,"" __VA_ARGS__)
#define XMBX(name) XMB(#name, NULL, _##name##_BUNDLE)

#define REG_ALL_ITEMS(DOMAIN)						\
  make_bundledef(NULL, "ELEM_" #DOMAIN "_", MS("" ALL_ELEMS_##DOMAIN), NULL), \
    make_bundledef(NULL, "BUNDLE_" #DOMAIN "_", MS("" ALL_BUNDLES_##DOMAIN), NULL)

#define CONCAT_DOMAINS(MACRO) make_bundledef(NULL, NULL, FOR_ALL_DOMAINS(MACRO), NULL)
#define REG_ALL CONCAT_DOMAINS(REG_ALL_ITEMS)

#define DEFINE_CORE_BUNDLES						\
  ADD_MAKE_STRANDS_FUNC ADD_MAKE_BUNDLEDEF_FUNC				\
  const char **all_elems, BUNLIST(BDEF_DEF_CONCAT,**,_BUNDLEDEF = NULL) **LAST_BUNDLEDEF = NULL; \
  const char ***builtin_bundledefs[] = {BUNLIST(BDEF_DEF_CONCAT, &,_BUNDLEDEF) NULL}; \
  const char **maker_get_bundledef(bundle_type btype)			\
  {return (btype >= 0 && btype < n_builtin_bundledefs ? *builtin_bundledefs[btype] : NULL);}

#define BDEF_IGN(a,b,c)

// make BNAME_BUNDLEDEF
#define BDEF_INIT(BNAME, PRE, EXTRA) BNAME##_BUNDLEDEF = MBX(BNAME);

// make ATTR_BNAME_ATTR_BUNDLE
// e.g. ASPECT_THREAD_ATTR_BUNDLE
#define ATTR_BDEF_INIT(ATTR_BNAME, PRE, EXTRA) ATTR_BNAME##_ATTR_BUNDLE = MBAX(ATTR_BNAME);

#define INIT_CORE_DATA_BUNDLES BUNLISTx(BDEF_IGN,BDEF_INIT,,) all_elems = REG_ALL;
#define INIT_CORE_ATTR_BUNDLES ABUNLIST(BDEF_IGN,ATTR_BDEF_INIT,,) all_attr_bundle = REG_ALL_ATTRS;
#define INIT_CORE_BUNDLES INIT_CORE_DATA_BUNDLES INIT_CORE_ATTR_BUNDLES 
#endif

#define BDEF_DEF_CONCAT(BNAME, PRE, EXTRA) PRE BNAME##EXTRA,
#define ATTR_BDEF_DEF_CONCAT(ATTR_BNAME, PRE, EXTRA) PRE BNAME##EXTRA,

#define BUNLIST(BDEF_DEF, pre, extra) BUNLISTx(BDEF_DEF, BDEF_DEF, pre, extra)
#define ABUNLIST(ABDEF_DEF, pre, extra) ABUNLISTx(ABDEF_DEF, ABDEF_DEF, pre, extra)
  
#define BUNLISTx(BDEF_DEFx_, BDEF_DEF_, pre, extra)			\
  BDEF_DEF_(DEF, pre, extra) BDEF_DEF_(CORE, pre, extra)		\
    BDEF_DEF_(BASE, pre, extra) BDEF_DEF_(MAPPED_VALUE, pre, extra)	\
    BDEF_DEF_(VALUE, pre, extra) BDEF_DEF_(VALUE_CHANGE, pre, extra)	\
    BDEF_DEF_(PMAP_DESC, pre, extra) BDEF_DEF_(PMAP, pre, extra)	\
    BDEF_DEF_(ATTR_DESC, pre, extra) BDEF_DEF_(ATTRIBUTE, pre, extra)	\
    BDEF_DEF_(ATTR_CONNECTION, pre, extra) BDEF_DEF_(ATTR_BUNDLE, pre, extra) \
    BDEF_DEF_(ATTR_DESC_BUNDLE, pre, extra) BDEF_DEF_(ATTR_CHANGE, pre, extra) \
    BDEF_DEF_(FN_INPUT, pre, extra) BDEF_DEF_(FN_OUTPUT, pre, extra)	\
    BDEF_DEF_(FUNCTION, pre, extra) BDEF_DEF_(ICAP, pre, extra)		\
    BDEF_DEF_(ANY_BUNDLE, pre, extra) BDEF_DEF_(STORAGE, pre, extra)	\
    BDEF_DEF_(HOOK_DETAILS, pre, extra) BDEF_DEF_(HOOK_STACK, pre, extra) \
    BDEF_DEF_(HOOK_CB_FUNC, pre, extra) BDEF_DEF_(COND_ARRAY, pre, extra) \
    BDEF_DEF_(TRANSFORM, pre, extra) BDEF_DEF_(OBJECT, pre, extra)	\
    BDEF_DEF_(OBJECT_TEMPLATE, pre, extra) BDEF_DEF_(OBJECT_INSTANCE, pre, extra) \
    BDEF_DEF_(THREAD_INSTANCE, pre, extra) BDEF_DEF_(OWNED_ATTRS, pre, extra) \
    BDEF_DEF_(CONTRACT, pre, extra) BDEF_DEF_(LOCATOR, pre, extra)	\
    BDEF_DEF_(ERROR, pre, extra) BDEF_DEF_(BUNDLE_ARRAY, pre, extra)	\
    BDEF_DEF_(MATRIX_2D, pre, extra) BDEF_DEF_(CONDVAL_NODE, pre, extra) \
    BDEF_DEF_(CONDLOGIC, pre, extra) BDEF_DEF_(CASCADE, pre, extra)	\
    BDEF_DEF_(CONSTVAL_MAP, pre, extra) BDEF_DEF_(CASCMATRIX_NODE, pre, extra) \
    BDEF_DEFx_(TSEGMENT, pre, extra) BDEF_DEFx_(TRAJECTORY, pre, extra) \
    BDEF_DEFx_(TRAJECTORY_CONDITION, pre, extra)	  

#define ABUNLISTx(ABDEF_DEFx_, ABDEF_DEF_, pre, extra)			\
  ABDEF_DEF_(THREAD_ASPECT, pre, extra) BDEF_DEF_(CORE, pre, extra)

#ifndef IS_BUNDLE_MAKER
    NIRVA_TYPEDEF(NIRVA_ENUM {BUNLIST(BDEF_DEF_CONCAT,, _BUNDLE_TYPE) \
				n_builtin_bundledefs}, bundle_type)
#undef BDEF_DEF

NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY all_elems;
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_strands(NIRVA_CONST_STRING fmt,...);
NIRVA_EXTERN NIRVA_CONST_STRING_ARRAY make_bundledef(NIRVA_CONST_STRING name, NIRVA_CONST_STRING pfx,...);
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

#define BDEF_DEFINE(BNAME) NIRVA_CONST_STING_ARRAY BNAME##_BUNDLEDEF
///#define BDEF_INIT(BNAME) BNAME##_BUNDLEDEF = MBX(BNAME);

// these values may be adjusted during inial bootstrap - see the documentation for more details

// values here define the level
#define NIRVA_AUTOMATION_NONE 			0
#define NIRVA_AUTOMATION_MANUAL 	       	1
#define NIRVA_AUTOMATION_SEMI 			2
#define NIRVA_AUTOMATION_FULL 			3

#define NIRVA_AUTOMATION_DEFAULT 		NIRVA_AUTOMATION_SEMI

// NIRVA_ASPECT_STRANDS
// NIRVA_ASPECT_FUNCTION_SHAPING

// automation for these "aspects"
#define NIRVA_ASPECT_NONE			0


#define NIRVA_ASPECT_THREADS			1
// attributes created intially in STRUCTURE_APP
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
#define _NIRVA_THREAD_MODEL_ON_DEMAND				1

// a thread is assigned to each instance with active transforms
// the thread remains bound to the instance but will idle while the instance is "dormant"
// the thread will be destroyed when the instance is recycled
// this is best for applications with a small number of active instances which need to interact
// with each other at all times
#define NIRVA_THREAD_MODEL_ONE_PER_INSTANCE		       	2

// implementation functions (NEED defining)
// for automation level semi, these funcitons need to be defined externally
// for thread model ON_DEMANS
// these functions should return a native thread from a pool, and then return them later
// thread must not be passed again until it returns
// the amount of time the Thread will be borrowed for cannot be detrmined
// in some cases it will never be returned. in other cases it will be returned almost imemdiately
#define ATTR_IMPLFUNC_BORROW_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "BORROW_NATIVE_THREAD")
#define ATTR_IMPLFUNC_BORROW_NATIVE_THREAD_TYPE       FUCNPTR, NULL

#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "RETURN_NATIVE_THREAD")
#define ATTR_IMPLFUNC_RETURN_NATIVE_THREAD_TYPE       FUCNPTR, NULL

// for FULL automation, the structure will handle all aspedcts of threead management itself

#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "CREATE_NATIVE_THREAD")
#define ATTR_IMPLFUNC_CREATE_NATIVE_THREAD_TYPE       FUCNPTR, NULL

#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD    	      ATTR_NAME("IMPLFUNC", "DESTROY_NATIVE_THREAD")
#define ATTR_IMPLFUNC_DESTROY_NATIVE_THREAD_TYPE      FUCNPTR, NULL

#define _ASPECT_THREADS_ATTRBUNDLE "THREADS_ASPECT", ADD_ATTR(AUTOLEVEL, THREADS), \
    ADD_ATTR(THREADS, MODEL), ADD_COND_ATTR(IMPLFUNC, BORROW_NATIVE_THREAD, NIRVA_EQUALS,\
    ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, NIRVA_EQUALS, ATTR_THREADS_MODEL,\
					    NIRVA_THREAD_MODEL_ON_DEMAND),\
    ADD_COND_ATTR(IMPLFUNC, RETURN_NATIVE_THREAD, NIRVA_COND_EQUALS,	\
		  ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, NIRVA_EQUALS, ATTR_THREADS_MODEL, \
		  NIRVA_THREAD_MODEL_ON_DEMAND),			\
     ADD_COND_ATTR(IMPLFUNC, CREATE_NATIVE_THREAD, NIRVA_EQUALS,\
    ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, NIRVA_EQUALS, ATTR_THREADS_MODEL,\
		   NIRVA_THREAD_MODEL_INSTANCE_PER_THREAD,\
		   NIRVA_LOGIC_OR, ATTR_AUTOLEVEL_THREADS,	\
		   NIRVA_AUTOMATION_FULL),				\
    ADD_COND_ATTR(IMPLFUNC, DESTROY_NATIVE_THREAD, NIRVA_EQUALS,	\
    ATTR_AUTOLEVEL_THREADS, NIRVA_AUTOMATION_SEMI, NIRVA_EQUALS, ATTR_THREADS_MODEL, \
    NIRVA_THREAD_MODEL_INSTANCE_PER_THREAD, NIRVA_LOGIC_OR, ATTR_AUTOLEVEL_THREADS, \
    NIRVA_AUTOMATION_FULL)

////////////////////////////////////////////////

#define NIRVA_ASPECT_HOOKS			2
// attributes created intially in STRUCTURE_APP
// preferred subtypes: AUTOMATION
// the attributes are transfered to preferred subtypes in order of preference
// changing these after structure_app is in PREPARED state requires PRIV_STRUCTURE > 100

// level of automation for hooks
//  - full, the structure will manage every aspect,including calling user callbcaks
//  - semi - system hooks will run automatically, but the application will run user_callbacks
//  - manual - the automation will signal when to trigger hooks and wait for return, but nothing more

#define ATTR_AUTOLEVEL_HOOKS				ATTR_NAME("AUTOLEVEL", "HOOKS")
#define ATTR_AUTOLEVEL_HOOKS_TYPE 		       	INT, NIRVA_AUTOMATION_DEFAULT

////////////////////////////////////////////////

#define NIRVA_ASPECT_REFCOUNTING		3

// level of automation for refcounting
//  - full, the structure will manage every aspect,including freeing bundles when refcount reaches -1
//  - semi - the structure will manage reffing and unreffing, but will only indicate when a bundle
//		should be free
//  - manual - the automation will signal when to ref / =unref somthing 

#define ATTR_AUTOLEVEL_REFCOUNTING	       		ATTR_NAME("AUTOLEVEL", "REFCOUNTING")
#define ATTR_AUTOLEVEL_REFCOUNTING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

////////////////////////////////

#define NIRVA_ASPECT_RECYCLING			4

// level of automation for bundle recycling bundle memory and connectiosn
//  - full, the structure will manage every aspect, including recycling bundles as instructed
// by refcounter
//  - semi - the structure will handke automated activites like disconnecting atributes, detaching hooks
//		but will leave freeing to the applicaiton
//  - manual - the automation will signal when to recycle bundledefs

#define ATTR_AUTOLEVEL_RECYCLING	       		ATTR_NAME("AUTOLEVEL", "RECYCLING")
#define ATTR_AUTOLEVEL_RECYCLING_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

////////////////////////////////
  
#define NIRVA_ASPECT_TRANSFORMS			5

// level of automation for running monitoring transforms
//  - full, the structure will manage every aspect, including transform trajectories
// by refcounter
//  - semi - the structure will handle automated activities like running structure functions
// 		thre applications will be able ot particpate in the process
//  - manual - the automation will signal which functions to run but the application will run them

#define ATTR_AUTOLEVEL_TRANSFORMS	       		ATTR_NAME("AUTOLEVEL", "TRANSFORMS")
#define ATTR_AUTOLEVEL_TRANSFORMS_TYPE        	       	INT, NIRVA_AUTOMATION_DEFAULT

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
//		ELEM_OBJECT_STATE, NIRVA_OBJ_STATE_PREPARED);
// or just simply:
// NIRVA_CALL(change_object_state, structure_app, NIRVA_OBJ_STATE_PREPARED);

//
  NIRVA_ENUM {
	STRUCTURE_SUBTYPE_PRIME = 0,
	// structure subtype which represents an application - ie process

	STRUCTURE_SUBTYPE_APP = 1,

	// role which handles user interface interactions - acts as proxy for
	// the end_user. Essential for applications that want to include UI
	// automation.
	// It can act as proxy to an Oracle instance, and its proxy functions include
	// Helping broker or more especially negotiator to fill in missing attribute values during
	// negotiation, can advise help arbitrator choose
	// between alternatives. 

	STRUCTURE_SUBTYPE_GUI,

	// role which involves creating and managing the the thrread pool
	// essential for any kiknd of automation

	STRUCTURE_SUBTYPE_THREAD_HERDER,

	// instance which is responsible for automatic actions
	// triggering hooks and running system hook functions
	// requires thread_herder to function 

	STRUCTURE_SUBTYPE_AUTOMATOR,

	// role which involves finding transforms for intents and trajectory navigation
	// requires threed_herder, and can benefit from archivist for larger applications
	// using a negotiator can help it find third party objects for supplying transform
	// side-data. The arbitrator may also consult it when searching for alternatives.

	STRUCTURE_SUBTYPE_BROKER,

	// subtype which specializes in tracking bundle lifescycles
	// noting whne instances are created and handling the destruction process
	// - recycling the elements - for exmaple disconnecting connetceted attributes
	// recalling hook callbacks and so on. If not enabled,
	// these tasks will be handled by automation
	// If lifecycle is enabled it will moitor objects, and those which are idle for too
	// long and not flagged as retain, will be sent for recycling, freeing up the resources.

	STRUCTURE_SUBTYPE_LIFECYCLE,

	// role which involves attempting to recover from errors and blocked states
	// as well as deciding choosing between alternatives if needed
      
	STRUCTURE_SUBTYPE_ARBITRATOR,

	// role which involves helping out in contract negotioations
	// can act as a proxy negotiator, working with the broker to help fulfill contract conditions
	// it can perform such as tasks as finding providers for data requrements,
	// and if necessary, negociate side contracst to receive side data. In addition,
	// it can map data output streams and interacting with broker can offer to make use of
	// or edit the data

	STRUCTURE_SUBTYPE_NEGOTIATOR,

	// role whcih involves taking actions for misbehaviour, flagging occurences
	// marking things as untrustworthy and so on
	// called in when the arbitrator is unable to remedy a problem
	// also takes on the role of managing privileges when there is no SECURITY sub system
	// or whne SECURITY is busy.

	STRUCTURE_SUBTYPE_ADJUDICATOR,

	// role which reloves around keeping track of time, synchronising activites
	// monitoring how long tasks take and so on. Sends reports to the OPTIMSATION.

	// if enabled it will also take on the role of timing out blocked threads,
	// triggering the hooks, as well as monitoring sync points

	// if a part of the application is blocked, it may call upon the ARNITRATOR to
	// investigate

	// it will supply application time / date information
	// if enabled also exposes a message queue for receiving timed actions
	// if a negotiator is avaialble it can also action contracts involving negotiating

	STRUCTURE_SUBTYPE_CHRONOMETRY,

	// role whcih involves storage and retrieval of object, fn, data etc
	// as well as loading and saving meta data. It is designed to provide quick lookups and
	// cross-referencing, as well as being able to restore the system to a stored state.
	// Also manages crash recovery, checksuming,  and error checking

	STRUCTURE_SUBTYPE_ARCHIVER,

	// role which invloves advising on resource usage. Receives updates from various
	// parts of the system and monitors performance Performs realtime optimisation using the
	// values it knows. Tuning may forward updates about ideal values.

	STRUCTURE_SUBTYPE_OPTIMISATION,

	// role which involves monitoring and adjsuting hardaware
	// as well as system tuning. Shares data with OPTIMISATION and HARMONY

	STRUCTURE_SUBTYPE_TUNING,

	// this role involves maintaining and checking PRIVelege levels for threads
	// and transforms. If not present, these tasks are handled by threadherder.
	// threads are created with default PRIV_ levels, usually 0.
	// some structural transform functions require higher PRIV levels in one or more
	// ASPECCTS. A thread_instance can change its PRIV level by passing a condcheck
	// this may involve receiving permision from an Oracle.
	// Failing a conditional PRIV check will result in some action - arbitrator may try to
	// ask an Oracle, adjudicator may flag an error and mark the transform as questionable
	// the oreacle can force_succeed to the condition once, always for the thread or always for
	// the transform. User interaction may be available to act as a substitute Oracle, after
	// observing another Oreacle's behaviour.

	STRUCTURE_SUBTYPE_SECURITY,

	// this role involves interacting with the underlying operating system, as well as with
	// other software components
	// may operate in conjunction with hadware contoller, performance_manage,
	// and the threader herder

	STRUCTURE_SUBTYPE_HARMONY,

	// this role involves introspecting the state of the applicationm and
	// testing and comparing alternative possibilites.
	//
	// consulting with STRATEGIST it may devise strategies to fine tune the parameters of the
	// infrastrucutre itself, adjusting or creating, bundles, making adjustments
	// to automation rules and so on. For exmaple STRATEGY may model an abstraction of some
	// portion of the application to be optimised. EVOUTION can test  these strategies
	// in the running system, and feed the results back to STRATEGY.
	// May share data with TUNING when testing alternatives and discovering optimal values.

	STRUCTURE_SUBTYPE_EVOLUTION,

	// role that involves planning and finding solutions to abstract issues,
	// given a problem it will try to find an optimal solution, may consuit with
	// any other structural compnents in order to do so
	// EVOLUTION and BEHAVIOR can wokr together to improve ewach others models
	// while strategy will employ abstract methods to optimise and improve the models

	STRUCTURE_SUBTYPE_STRATEGY,

	// role which includes analysing Oracles,
	// attempting to discern patterns in their responses, reducing the frequency at which
	// they are consulted. The subsytem will attempt to predict Oracle responses and adjust
	// internal modules to replicate their choices as closely as possible.
	// It will provide the Oracle a means to inform it when its predicted choices differ from
	// those it would have supplied and when they align with or are superior to its.
	// Such feedback will be used to adjust the internal models.

	STRUCTURE_SUBTYPE_BEHAVIOUR,

	// this subtype, if enabled, imposes an economic model on objects. This is intended for
	// advanced, self optimising code applications. In this mode, each object has an "account"
	// and can earn credit for performing transforms and producing data. These credits can
	// then be "spent" to advertise contracts, outsouce processing and to "rent" more threads.
	// an purchase more resources. Objects which run out of credits will eventually "starve"
	// and be sent for recycling. Objects can also pay to be modified, to purchase clones and so on.
	// Objects can transfer credits to another object. In addition when an object is sent for
	// recylcing, its attributes are auctioned off. Broker will pay for intents being satisified.
	// In addition, objects can pay to attach their own attributes to a remote attibute of
	// another object.

	// the banker monitors the state of the economy and may lend out at negotiated interest rates
	// it also maintains the accounts of each object, and faciliates transactions.
	// in addition, objects may specify an "heir" to receive their credits in case they are recycled.
      
	STRUCTURE_SUBTYPE_BANKER,

	// this subtype provides objects with a space to store scripts
	// an object may request that a script be run at any time,
	// the effects of the scrips are to be determined, but objects may produce copies of themselves
	// with a partner object, and the scripts will be co-mingled. Scripts which are dormant for
	// too long are sent for recycling, and their offspring may continue.

	STRUCTURE_SUBTYPE_GENETIC,
      

	N_STRUCTURE_SUBTYPES,

	// this subtype cannot be created but can be discovered. An Oracle is able
	// to supply missing contract
	// attribute values, provided they are of simple types (non array, non binary data).
	// end users are a type of oracle, the user prefs instance may be able to help constuct a
	// more accurat model

	STRUCTURE_SUBTYPE_ORACLE,
};
#endif

#endif
