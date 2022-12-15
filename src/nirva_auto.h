// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// PROJECT N.I.R.V.A
// bootstrap:
//// The macros and defintions here are just sufficient to bootstrap the system
// - facilitate the creation bundle defintions for all the defualt types, and enumerate them
//
// - provide default versions of "Implementation functions"
//
// - define language specific macros for creating generic functions and macros
//
// - ensure the bootstrap process has all of the funcitonality required
//
// - provide an application function call, nirva_init() which will begin the bootstrap process
//
// - provide sufficient supporting framework to reach the point where we can construct the first
// 		object template - TYPE == STRUCTURAL, reffered to as STRUCTURE_PRIME
//
//  - provide minimal infrastructure for dynamically loading the body of the STRUCTURE_PRIME
//
//  - once this is done, call the gateway function in STRUCTURE_PRIME
//
//  - STRUCTURE_PRIME will now continue the initialisation, if we get to this point,
//      our task is complete

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef OBJECT_CONSTANTS_H
#include "object-constants.h"
#endif

// functionality is split by "generation", correspondiong to bootstrap phases
// we start in generation 0, when nirva_setup() is called, we go immediately to gen1
// then when all bundle defintions are created from macros, and validated, we move to gen2
// when we have created bluprint bundles for each bundle type, we move to gen3
// in gen3, the bundle defintions are transfered to factory instances of the blueprint
// object, once these are all available, we move to gen4

static int NIRVA_FUNC_GENERATION = 0;

#define NIRVA_DEF_ICAPI OBJ_INTENTION_CREATE_BUNDLE
#define NIRVA_DEF_ICAPC CAP_OBJECT_TYPE(STRUCTURAL)

/* #define NIRVA_MAKE_INTENTCAP(name,ICAP) NIRVA_GRIND_INTENTCAP(name,NIRVA_ROAST_INTENTCAP(ICAP)) */
/* #define NIRVA_DEF_INTENTCAP NIRVA_MAKE_INTENTCAP("default",_NIRVA_DEF_ICAP) */

/////////////////////////// bundles //

// priorities for Oracular data sources
#define NIRVA_PRIORITY_FALLBACK 1
#define NIRVA_PRIORITY_LOW 10
#define NIRVA_PRIORITY_DEFAULT 20
#define NIRVA_PRIORITY_HIGH 50
#define NIRVA_PRIORITY_TOP 100

#define SEGMENT_END NIRVA_NULL

// DIRECTIVES
// these are instructions that can be embedded in bundledef
// in gen1 and gen2 functions thes should simply be copied
// in gen3, they are converted to automations

#define DIRECTIVE_BEGIN "@BEGIN "
#define DIRECTIVE_END "@END "

#define DIR_START(dirnm)  			DIRECTIVE_BEGIN #dirnm
#define DIR_FINISH(dirnm) 			DIRECTIVE_END #dirnm

// thes directives are only added in gen3
// see notes in documentation

#define DIRECTIVE_ADD_HOOK_AUTO          	DIR_START(add_hook_auto)
// automation script
#define DIRECTIVE_ADD_HOOK_AUTO_END  	  	DIR_FINISH(add_hook_auto)

// hook automations are basically self callbacks, but in the form of
// a script

// AUTOMATIONS - can also be stored in scriptlets just as conditions can be

// commands may require priv checks to perform, except for some self
// automations + cascade_hooks
#define NIRVA_AUTO_CMD_1 "SET_STRAND_VALUE" // strand name, strand_val
#define NIRVA_AUTO_CMD_2 "CASCADE_HOOKS" // causes a follow on hook_number to be calculated and called
#define NIRVA_AUTO_CMD_3 "ADD_STRAND_TO" // holder, strand_name, type, value
#define NIRVA_AUTO_CMD_4 "REMOVE_STRAND_FROM" // container, strand name
#define NIRVA_AUTO_CMD_5 "ADD_HOOK_CB" // obj, item, hook_number, cb, data
#define NIRVA_AUTO_CMD_6 "REMOVE_HOOK_CB" // bj, item, hook_number, cb, data
#define NIRVA_AUTO_CMD_7 "APPEND_TO_ARRAY" // strand, vakue
#define NIRVA_AUTO_CMD_8 "REMOVE_FROM_ARRAY" // strand, value
// add a HOoK automation in a sub bundle. followed by the sub bundle, then the rest is
// added as if the hook automation were added at the sub bundle level
#define NIRVA_AUTO_CMD_9 "ADD_HOOK_AUTO" // sub bundle, strand_name, hook_number, cond_type, args
// this is for STRAND bundle...when STRAND_TYPE or ATTR_TYPE is set, set type for default, val, nd
#define NIRVA_AUTO_CMD_10 "SET_STRAND_TYPE" // strand_name, strand_type
#define NIRVA_AUTO_CMD_11 "REF_BUNDLE" // ptr to bundle...if target has no refcounter, does nothing
#define NIRVA_AUTO_CMD_12 "UNREF_BUNDLE" // unrefs, if target has no refcounter or rc is -1, frees

#define HOOK_AUTO_BEGIN(strand_name, hook_number, cond, ...)		\
  MSTY("%s", DIRECTIVE_ADD_HOOK_AUTOMATION, "%s", #strand_name, "%d", hook_number##_HOOK, cond, __VA_ARGS__)

#define HOOK_AUTO_ADD_HOOK_AUTO(sub, strand_name, hook_number, cond, ...) \
  MSTY("%s", "ADD_HOOK_AUTO", "%s", sub, "%s", #strand_name, "%d", hook_number##_HOOK, cond, __VA_ARGS__)

#define HOOK_AUTO_SET_STRAND_VALUE(strand_name, val_as_string)		\
  MS("SET_STRAND_VALUE", #strand_name, val_as_string, DIRECTIVE_ADD_HOOK_AUTO_FINISH)

#define HOOK_AUTO_ADD_STRAND_TO(sub, strand_name, strand_type, val_as_string) \
  MSTY("%s", "ADD_STRAND_TO", "%s", sub, "%s", #strand_name, "%u", strand_type, "%s", val_as_string, \
       "%s", DIRECTIVE_ADD_HOOK_AUTO_FINISH)

#define HOOK_AUTO_DELETE_STRAND_FROM(sub, strand_name)			\
  MSTY("%s", "DELETE_STRAND_FROM", "%s", sub, "%s", #strand_name, DIRECTIVE_ADD_HOOK_AUTO_FINISH)

// the following substitutions may be used in conditions, automations and so on
// in place of a value

// for pre-data changes, the current value to be updated, removed or deleted
// for post-data, the value of the strand before updating
// for attributes, this refers to the attribute "data"
#define NIRVA_STRAND_REF_OLD_VAL 			"@OLD_VAL"

// for post-data changes, the current value,
// for pre-data, the new value to be set, appended or added
// for attributes, this refers to the attribute "data"
#define NIRVA_STRAND_REF_NEW_VAL 			"@NEW_VAL"

// followed by a strand name, refers to the value held in the strand
#define NIRVA_STRAND_REF_ANY				"@*"

// strand 2

// gen1 flag bits
// other than "optional" which is set in strand1
// there are 3 mutually exclusive values which can be set in strand2

// flag bits, decimal value in strand1
#define STRAND2_FLAG_ARRAY				1

// gen 2 flag bits

// for the time being only one of these can be set at once
#define STRAND2_FLAG_READONLY				2

#define STRAND2_FLAG_TEMPLATE				4

#define STRAND2_FLAG_RDONLY_SUB				6

#define STRAND2_FLAG_KEYED				8

//	FLAGS FOR blueprint bundles (some of these correspond to bdef flags, but they
// are not identical)

// when recreating the bundledef as a blueprint, the following
// flag bits can be set in the strand_def. Aside from marking a strand_def
// as a comment, there are just 4 values:

// info bits:

// functional flags

// strand can be created at any time
#define BLUEPRINT_FLAG_OPTIONAL 		(1ull << 1)

// value MUST be set at creation, then becomes readonly
// used for uid, bundle_type, name, strand_type
// (this is different from readonly ATTRIBUTEs, which may be created with or without
// a default value, and have the "real" readonly value set later)
// NOTE: if the OPTIONAL bit is also set, this turns it into an INDEX strand_def
#define BLUEPRINT_FLAG_READONLY 	       	(1ull << 2)

// KEYED_ARRAY - the DATA will hold an array of bundleptr or const bundleptr
// (limited by STRAND_TYPE and RESTRICTIONS)
// Unlike normal arrays, items must be appended one by one to the array, and each time a unique string key is supplied

// items can later be read, updated or removed using the key reference
// the usual array functions (nirva_get_array_bundleptr. nirva_array_get_size,
// nirva_array_clear. nirva_array_append (single value at a time) and nirva_array_set functions work as per usual
//
// in addition, for keyed arrays, one can use nirva_get_value_by_keyy, nirva_update_value_by_key,
// nirva_remove_value_by_key, and nirva_has_value_for_key
//
// if the implementation does not define its own versions of the key_array functions, then the automation will
// add and maintain an index bundle,
// the automation will intercept calls and intervene to add / remove  a reference (copy of the data, always as const_bundleptr) in index
#define BLUEPRINT_FLAG_KEYED_ARRAY 		(1ull << 4)

// declares that when a sub bundle is set / appended in this strand,
// all strands in the sub bundle must be marked readonly
#define BLUEPRINT_FLAG_RDONLY_SUB 		(1ull << 5)

// marks a comment in blueprint. Strands should not be created from this
#define BLUEPRINT_FLAG_COMMENT 			(1ull << 6)

// ispecial value which can be used when describing a strand_name
#define $CONTAINER "$CONTAINER" // e.g @CONTAINER.STRaND_TYPE

// built in restrictions
#define _RESTRICT_BUNDLE_TYPE(btype)			\
  _COND_VAL_EQUALS, VAR_STRAND_VAL, "BLUEPRINT/BUNDLE_TYPE",	\
    _CONST_UINT64_VAL, btype##_BUNDLE_TYPE

#define RESTRICT_BUNDLE_TYPE(btype) MSTY(_COND_START, _RESTRICT_BUNDLE_TYPE(btype), _COND_END)

#define RESTRICT_TO_OBJECT MSTY(_COND_START, COND_P_OPEN, RESTRICT_BUNDLE_TYPE(OBJECT_TEMPLATE), COND_LOGIC_OR, \
				_RESTRICT_BUNDLE_TYPE(OBJECT_INSTANCE), _COND_P_CLOSE, _COND_END)

#define RESTRICT_OBJECT_TEMPLATE(type) MSTY(_RESTRICT_BUNDLE_TYPE(OBJECT_TEMPLATE), \
					    COND_LOGIC_AND,		\
					    _COND_VAL_EQUALS, VAR_STRAND_VAL, "TYPE", \
					    _CONST_UINT64_VAL, OBJECT_TYPE_##type, _COND_END)

#define RESTRICT_OBJECT_INSTANCE(type) MSTY(_RESTRICT_BUNDLE_TYPE(OBJECT_INSTANCE), \
					    COND_LOGIC_AND,		\
					    _COND_VAL_EQUALS, VAR_STRAND_VAL, "TYPE", \
					    _CONST_UINT64_VAL, OBJECT_TYPE_##type, _COND_END)

#define RESTRICT_INSTANCE_TYPE_SUBTYPE(type, subtype)		\
  MSTY(_RESTRICT_BUNDLE_TYPE(OBJECT_INSTANCE),			\
       COND_LOGIC_AND,						\
       _COND_VAL_EQUALS, VAR_STRAND_VAL, "TYPE",	\
       _CONST_UINT64_VAL OBJECT_TYPE_##type,			\
       COND_LOGIC_AND,						\
       _COND_VAL_EQUALS, VAR_STRAND_VAL, "SUBTYPE",	\
       _CONST_UINT64_VAL, OBJECT_SUBTYPE_##subtype, _COND_END	\
       )
NIRVA_ENUM(ANY_RESTRICTION,
           BLUEPRINT_RESTRICTION,
           ATTRIBUTE_RESTRICTION,
           SEGMENT_RESTRICTION,
           CONDLOGIC_NODE_RESTRICTION,
           CASCMATRIX_NODE_RESTRICTION,
           CASCADE_RESTRICTION,
           OBJECT_RESTRICTION,
           HOOK_DETAILS_RESTRICTION,
           OBJECT_INSTANCE_RESTRICTION,
           ATTR_GROUP_RESTRICTION,
           CONTRACT_RESTRICTION,
           SCRIPTLET_RESTRICTION,
           OBJECT_TEMPLATE_RESTRICTION,
           STRAND_DEF_RESTRICTION,
           HOOK_STACK_RESTRICTION,
           VALUE_RESTRICTION,
           SELECTOR_RESTRICTION,
           INDEX_RESTRICTION,
           ATTR_CONNECTION_RESTRICTION,
           REFCOUNTER_RESTRICTION,
           DEF_RESTRICTION,
           TRAJECTORY_RESTRICTION,
           VALUE_CHANGE_RESTRICTION,
           ATTR_DEF_GROUP_RESTRICTION,
           LOCATOR_RESTRICTION,
           FUNC_DATA_RESTRICTION,
	   EMISSION_RESTRICTION,
           ICAP_RESTRICTION,
           TRANSFORM_RESTRICTION,
           THREAD_INSTANCE_RESTRICTION,
           ATTR_DEF_RESTRICTION,
           CAPS_RESTRICTION,
           ATTR_MAP_RESTRICTION,
           HOOK_CB_FUNC_RESTRICTION,
           FUNCTIONAL_RESTRICTION
          )

#define NIRVA_RESTRICTION_0 MSTY(_COND_ALWAYS)
#define NIRVA_RESTRICTION_1  RESTRICT_BUNDLE_TYPE(BLUEPRINT)
#define NIRVA_RESTRICTION_2  RESTRICT_BUNDLE_TYPE(ATTRIBUTE)
#define NIRVA_RESTRICTION_3  RESTRICT_BUNDLE_TYPE(SEGMENT)
#define NIRVA_RESTRICTION_4  RESTRICT_BUNDLE_TYPE(CONDLOGIC_NODE)
#define NIRVA_RESTRICTION_5  RESTRICT_BUNDLE_TYPE(CASCMATRIX_NODE)
#define NIRVA_RESTRICTION_6  RESTRICT_BUNDLE_TYPE(CASCADE)
#define NIRVA_RESTRICTION_7  RESTRICT_TO_OBJECT
#define NIRVA_RESTRICTION_8  RESTRICT_BUNDLE_TYPE(HOOK_DETAILS)
#define NIRVA_RESTRICTION_9  RESTRICT_BUNDLE_TYPE(OBJECT_INSTANCE)
#define NIRVA_RESTRICTION_10 RESTRICT_BUNDLE_TYPE(ATTR_GROUP)
#define NIRVA_RESTRICTION_11 RESTRICT_BUNDLE_TYPE(CONTRACT)
#define NIRVA_RESTRICTION_12 RESTRICT_BUNDLE_TYPE(SCRIPTLET)
#define NIRVA_RESTRICTION_13 RESTRICT_BUNDLE_TYPE(OBJECT_TEMPLATE)
#define NIRVA_RESTRICTION_14 RESTRICT_BUNDLE_TYPE(STRAND_DEF)
#define NIRVA_RESTRICTION_15 RESTRICT_BUNDLE_TYPE(HOOK_STACK)
#define NIRVA_RESTRICTION_16 RESTRICT_BUNDLE_TYPE(VALUE)
#define NIRVA_RESTRICTION_17 RESTRICT_BUNDLE_TYPE(SELECTOR)
#define NIRVA_RESTRICTION_18 RESTRICT_BUNDLE_TYPE(INDEX)
#define NIRVA_RESTRICTION_19 RESTRICT_BUNDLE_TYPE(ATTR_CONNECTION)
#define NIRVA_RESTRICTION_20 RESTRICT_BUNDLE_TYPE(REFCOUNTER)
#define NIRVA_RESTRICTION_21 RESTRICT_BUNDLE_TYPE(DEF)
#define NIRVA_RESTRICTION_22 RESTRICT_BUNDLE_TYPE(TRAJECTORY)
#define NIRVA_RESTRICTION_23 RESTRICT_BUNDLE_TYPE(VALUE_CHANGE)
#define NIRVA_RESTRICTION_24 RESTRICT_BUNDLE_TYPE(ATTR_DEF_GROUP)
#define NIRVA_RESTRICTION_25 RESTRICT_BUNDLE_TYPE(LOCATOR)
#define NIRVA_RESTRICTION_26 RESTRICT_BUNDLE_TYPE(FUNC_DATA)
#define NIRVA_RESTRICTION_27 RESTRICT_BUNDLE_TYPE(EMISSION)
#define NIRVA_RESTRICTION_28 RESTRICT_BUNDLE_TYPE(ICAP)
#define NIRVA_RESTRICTION_29 RESTRICT_BUNDLE_TYPE(TRANSFORM)
#define NIRVA_RESTRICTION_30 RESTRICT_OBJECT_INSTANCE(THREAD)
#define NIRVA_RESTRICTION_31 RESTRICT_BUNDLE_TYPE(ATTR_DEF)
#define NIRVA_RESTRICTION_32 RESTRICT_BUNDLE_TYPE(CAPS)
#define NIRVA_RESTRICTION_33 RESTRICT_BUNDLE_TYPE(ATTR_MAP)
#define NIRVA_RESTRICTION_34 RESTRICT_BUNDLE_TYPE(HOOK_CB_FUNC)
#define NIRVA_RESTRICTION_35 RESTRICT_BUNDLE_TYPE(FUNCTIONAL)

#define N_REST_TYPES 36
  
#define GET_STRAND_TYPE(xdomain, xitem) _CALL(_GET_STYPE, STRAND_##xdomain##_##xitem##_TYPE)

#define _GET_TYPE(a, b) _STRAND_TYPE_##a
#define _GET_STYPE(a, b) STRAND_TYPE_##a
#define _GET_BUNDLE_TYPE(a, b) a##_BUNDLE_TYPE
#define _GET_DEFAULT(a, b) #b
#define _GET_REST(a, b) a##_RESTRICTION

#define GET_STRD_TYPE(xdomain, xitem) _CALL(_GET_TYPE, STRAND_##xdomain##_##xitem##_TYPE)
#define GET_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, STRAND_##xdomain##_##xitem##_TYPE)
#define GET_RESTRICTION(xdomain, xitem) _CALL(_GET_REST, BUNDLE_##xdomain##_##xitem##_TYPE)
#define GET_BUNDLE_TYPE(xdomain, xitem) _CALL(_GET_BUNDLE_TYPE, BUNDLE_##xdomain##_##xitem##_TYPE)
#define GET_BUNDLE_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, BUNDLE_##xdomain##_##xitem##_TYPE)
//mach1n3
#define JOIN(a, b) GET_STRD_TYPE(a, b) #b
#define JOIN2(a, b, c) GET_STRD_TYPE(a, b) #c
#define JOIN3(a, b, c, d, e) "%s", GET_STRD_TYPE(a, b)#e " ", "%u", c

#define PJOIN3(a, b, c, d) GET_STRD_TYPE(a, b) #d
#define BJOIN3(a, b, r, n) GET_STRD_TYPE(a, b) #n " ", "%u",  r

#define _ADD_STRAND(domain, item) JOIN(domain, item)
#define _ADD_STRANDn(domain, item, name) JOIN2(domain, item, name)

#define _ADD_NAMED_BSTRAND(xd, xi, rest, name) BJOIN3(xd, xi, rest, name)
#define _ADD_NAMED_OPT_BSTRAND(xd, xi, rest, name) "?" BJOIN3(xd, xi, rest, name)

#define _ADD_NAMED_PSTRAND(xd, xi, name) PJOIN3(xd, xi, name)
#define _ADD_NAMED_OPT_PSTRAND(xd, xi, name) PJOIN3(xd, xi, name)

#define _ADD_KSTRAND(xd, xi, btype, name) JOIN3(xd, xi, btype, domain, item)
#define _ADD_OPT_STRAND(domain, item) "?" JOIN(domain, item)
#define _ADD_OPT_ANON_STRAND(domain, item) "?" JOIN2(domain, item,)
#define _ADD_VARIABLE_STRAND(domain, item) ":" JOIN(domain, item)
#define _ADD_OPT_STRANDn(domain, item, name) "?" JOIN2(domain, item, name)

#define MS(...)make_strands("", __VA_ARGS__, NULL)
#define MSTY(...)make_strands(__VA_ARGS__, NULL)

// local ptrs, scalar and array
#ifdef DESCRIPTIVE_BDEFS
#define _ADD_STRAND2(domain, item) "DEFAULT: "GET_DEFAULT(domain, item)
#define _ADD_STRAND2a(domain, item) "FLAGS: ARRAY, DEFAULT: "GET_DEFAULT(domain, item)
#else
#define _ADD_STRAND2(domain, item) "0 " GET_DEFAULT(domain, item)
#define _ADD_STRAND2a(domain, item) "1 " GET_DEFAULT(domain, item)
#endif

// add readonly strand, default is ignored, value must be set
#ifdef DESCRIPTIVE_BDEFS
#define _ADD_STRAND2ro "FLAGS: READONLY"
#define _ADD_STRAND2ro "FLAGS: READONLY, ARRAY"
#define _ADD_STRAND2idx "FLAGS: TEMPLATE"
#define _ADD_STRAND2rob "FLAGS: READONLY_BUNDLE"
#define _ADD_STRAND2akey "FLAGS: ARRAY, KEYED"
#else
#define _ADD_STRAND2ro "2 none"
#define _ADD_STRAND2roa "3 none"
#define _ADD_STRAND2idx "4 none"
#define _ADD_STRAND2rob "6 none"
#define _ADD_STRAND2akey "9 none"
#endif

#define INC_CUN(td, ti, r, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", r,	\
				   "%s", _ADD_STRAND2(td, ti))
#define INC_CUNO(td, ti, r, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", r, \
				    "%s", _ADD_STRAND2(td, ti))
#define INC_CARR(td, ti, r, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", r,	\
				    "%s", _ADD_STRAND2a(td, ti))
#define INC_CARRO(td, ti, r, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", r, \
				     "%s", _ADD_STRAND2a(td, ti))
#define INC_CARROKEY(td, ti, r, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", r, \
					"%s", _ADD_STRAND2akey)

#define INC_BUN(td, ti, d, i, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
				      "%s", _ADD_STRAND2(td, ti))
#define INC_BUNR(td, ti, d, i, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
				       "%s", _ADD_STRAND2rob)
#define INC_BUNO(td, ti, d, i, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
				       "%s", _ADD_STRAND2(td, ti))
#define INC_BUNOR(td, ti, d, i) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #i " %u", GET_RESTRICTION(d,i), \
				     "%s", _ADD_STRAND2rob)

#define INC_BARR(td, ti, d, i, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
				       "%s", _ADD_STRAND2a(td, ti))

#define INC_BARRO(td, ti, d, i, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
					"%s", _ADD_STRAND2a(td, ti))

#define INC_BARRKEY(td, ti, d, i, n) MSTY("%s", GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
					  "%s", _ADD_STRAND2akey)
#define INC_BARROKEY(td, ti, d, i, n) MSTY("%s", "?" GET_STRD_TYPE(td, ti) #n " %u", GET_RESTRICTION(d,i), \
					   "%s", _ADD_STRAND2akey)

////////////////////////// BUNDLEDEF "DIRECTIVES" ////////////////////////////////////////
#define ADD_STRAND(d, i)	       	 	    	MS(_ADD_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_NAMED_STRAND(d, i, n)      	       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_ARRAY(d, i) 			    	MS(_ADD_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_ARRAY(d, i, n) 	       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2a(d,i))

#define ADD_READONLY_STRAND(d, i) 		       	MS(_ADD_STRAND(d,i),_ADD_STRAND2ro)

#define ADD_OPT_STRAND(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_OPT_READONLY_STRAND(d, i) 		       	MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2ro)
#define ADD_OPT_READONLY_ARRAY(d, i) 		       	MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2roa)

#define ADD_TEMPLATE_STRAND(d, i) 	 	      	MS(_ADD_OPT_ANON_STRAND(d,i),_ADD_STRAND2idx)

#define ADD_NAMED_OPT_STRAND(d, i, n)          	    	MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_NAMED_OPT_READONLY_STRAND(d, i, n) 	    	MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2ro)

#define ADD_OPT_NAMED_STRAND(d, i, n) 			ADD_NAMED_OPT_STRAND(d,i,n)
#define ADD_OPT_ARRAY(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_OPT_ARRAY(d, i, n) 	       		MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2a(d,i))
#define ADD_OPT_NAMED_ARRAY(d, i, n)			ADD_NAMED_OPT_ARRAY(d,i,n)

//#define ADD_COMMENT(text)				MS("#" text)

// include all strands from bundle directly
#define EXTEND_BUNDLE(BNAME) 				GET_BDEF(BNAME##_BUNDLE_TYPE)
#define EXTENDS_BUNDLE(BNAME)				EXTEND_BUNDLE(BNAME)

// ptr to bundle and array of ptrs
#define INCLUDE_BUNDLES(domain, item)		     	INC_BARR(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_BUNDLES(domain, item)			INCLUDE_BUNDLES(domain, item)
#define INCLUDE_NAMED_BUNDLES(domain, item, name)      	INC_BARR(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLES(domain, item, name)    	INCLUDE_NAMED_BUNDLES(domain, item, name)

#define INCLUDE_KEYED_BUNDLES(domain, item, name)      	INC_BARRKEY(VALUE, BUNDLEPTR, domain, item, name)

#define INCLUDE_OPT_BUNDLES(domain, item)	     	INC_BARRO(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_OPT_BUNDLES(domain, item)		INCLUDE_OPT_BUNDLES(domain, item)

#define INCLUDE_OPT_KEYED_BUNDLES(domain, item, name)	INC_BARROKEY(VALUE, BUNDLEPTR, domain, item, name)

#define INCLUDE_OPT_NAMED_BUNDLES(domain, item, name)  	INC_BARRO(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_OPT_NAMED_BUNDLES(domain, item, name) 	INCLUDE_OPT_NAMED_BUNDLES(domain, item, name)
#define INCLUDE_NAMED_OPT_BUNDLES(domain, item, name)  	INCLUDE_OPT_NAMED_BUNDLES(domain, item, name)
#define INCLUDES_NAMED_OPT_BUNDLES(domain, item, name) 	INCLUDE_OPT_NAMED_BUNDLES(domain, item, name)

#define INCLUDE_BUNDLE(domain, item)  		    	INC_BUN(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_BUNDLE(domain, item)			INCLUDE_BUNDLE(domain, item)

#define INCLUDE_READONLY_BUNDLE(domain, item)  	      	INC_BUNR(VALUE, BUNDLEPTR, domain, item, item)

#define INCLUDE_OPT_BUNDLE(domain, item)  	      	INC_BUNO(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_OPT_BUNDLE(domain, item)	       	INCLUDE_OPT_BUNDLE(domain, item)

#define INCLUDE_NAMED_READONLY_BUNDLE(domain,item,name) INC_BUNR(VALUE,BUNDLEPTR,domain,item,name)

#define INCLUDE_NAMED_BUNDLE(domain, item, name)      	INC_BUN(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLE(domain, item, name)      	INCLUDE_NAMED_BUNDLE(domain, item, name)

#define INCLUDE_OPT_NAMED_BUNDLE(domain, item, name)	\
  INC_BUNO(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_OPT_NAMED_BUNDLE(domain, item, name)	\
  INCLUDE_OPT_NAMED_BUNDLE(domain, item, name)
#define INCLUDE_NAMED_OPT_BUNDLE(domain, item, name)	\
    INCLUDE_OPT_NAMED_BUNDLE(domain, item, name)
#define INCLUDES_NAMED_OPT_BUNDLE(domain, item, name)	\
    INCLUDE_OPT_NAMED_BUNDLE(domain, item, name)

#define INCLUDE_OPT_READONLY_BUNDLE(domain, item)	\
    INC_BUNOR(VALUE, BUNDLEPTR, domain, item)

// IMPORTANT !!!
// INCLUDE_BUNDLE(s) and ADD_CONST_BUNDLEPTRs
// are different.
// THE DIFFERENCE BETWEEN INCLUDE_BUNDLE(s) AND ADD_CONST_BUNDLEPTR(s) is that with INCLUDE_BUNDLE,
// the sub-bundle will be "owned", that is, when the containing bundle is freed, the sub-bundle
// should also be freed (unreffed).
// This is done simply by creating a strand in the bundle with type bundleptr (array)
// creating sub bundles then appending to the array strand
//
// With ADD_CONST_BUNDLEPTR, the pointers are void *
// to external bundles. (referred to as CONST_BUNDELPTR)These MUST NOT be unreffed / freed
// thus the implementation needs to differentiate these two types.
//
// as a cross-check, included bundles MUST set a CONST_BUNDLEPTR
// to the container when being included. Thus when unreffing a sub bundle one can
// check the value of "container" to make sure it points to the correct bundle

#define ADD_CONST_BUNDLEPTR(domain, item, rest) INC_CUN(VALUE, CONST_BUNDLEPTR, \
							rest##_RESTRICTION, item)
#define ADD_OPT_CONST_BUNDLEPTR(domain, item, rest) INC_CUNO(VALUE, CONST_BUNDLEPTR, \
							     rest##_RESTRICTION, item)
// this is an array of pointers to remote bundles...
#define ADD_CONST_BUNDLEPTRS(domain, item, rest) INC_CARR(VALUE, CONST_BUNDLEPTR, \
							  rest##_RESTRICTION, item)
#define ADD_OPT_CONST_BUNDLEPTRS(domain, item, rest) INC_CARRO(VALUE, CONST_BUNDLEPTR, \
							       rest##_RESTRICTION, item)

#define ADD_OPT_KEYED_BUNDLEPTRS(domain, item, rest) INC_CARROKEY(VALUE, CONST_BUNDLEPTR, \
								  rest##_RESTRICTION, item)

//// PREDEFINED BUNDLEDEFS //////////////

// this bundle falls outside the general rules, it is solely used to store info during the
// bootstrap process, it doesnt have a bundle_type, the bundledef is created separately
// data can be created as strand bundless, a top level strand can have a bundleptr to
// a storage bundle, which can hold sub strands
//
#define _BOOTER_BUNDLE INCLUDE_NAMED_BUNDLES(STANDARD, STRAND_REPLACEMENT, STORAGE)

// ALL bundles with the exception of INTERFACE bundles (solely designed to be extended)
// MUST extend this, either directly or indirectly by extension
// MANDATORY for all bundles which extend this are
// UID - a readonly randomly generated 64 bit identifier, must be set when the bundle is created
// BLUEPRINT - a pointer to the (static) blueprint bundle used as a template for this bundle
// - the BUNDLE_TYPE cna be found in the blueprint
// CONTAINER - a pointer to the bundle containing this one
//  - the default is NULL, implying that the bundle is contained in a structure object
// if this points to the bundle itself, this implies that the bundle is a "static" bundle
// otherwise, when the container is freed, the bundle will be unreffed (and possibly also freed)
// CONTAINER_STRAND - the name of the strand in container which contains this bundle
//  - the strand must have strand_type STRAND_TYPE_BUNDLEPTR (directly or proxied), and must contain a pointer
// to this bundle
// default is NULL, if "container" is also NULL, then this is ignored, the real strand name will be something like "STATIC_BUNDLES"
// if "container" is self, "container_strand" is ignored

#define _DEF_BUNDLE							\
    ADD_READONLY_STRAND(GENERIC, UID),					\
      ADD_CONST_BUNDLEPTR(INTROSPECTION, BLUEPRINT, BLUEPRINT),		\
      ADD_STRAND(FRAMEWORK, CONTAINER),					\
      ADD_STRAND(FRAMEWORK, CONTAINER_STRAND),				\
									\
      ADD_OPT_READONLY_STRAND(SPEC, VERSION),				\
      ADD_OPT_STRAND(GENERIC, DESCRIPTION)



/* INCLUDE_NAMED_OPT_BUNDLES(STANDARD, HOOK_STACK, HOOK_STACKS),	 */
/* INCLUDE_OPT_BUNDLE(INTROSPECTION, REFCOUNTER),			 */
/* ADD_OPT_STRAND(DATETIME, CREATION_DATE) */

// BUNDLE_TYPE denotes the enumerated bundle_type which the blueprint is a template for
// MULTI is a special optional STRAND_DEF, if present, then as many copies of this as desired may be
// created, provided each has a unique (for the bundle), name
// Optionally, the strand_def contained in "MULTI" may have a name set to a Prefix, which can be prepended
// to the start of the NAMEs of all strands created from it.
// the remining strand_defs for the created bundles are contained in STRAND_DEFs
// AUTOMATIONS can contain various SCRIPTLETs, including hook_automations
#define _BLUEPRINT_BUNDLE EXTEND_BUNDLE(DEF),  ADD_READONLY_STRAND(BLUEPRINT, BUNDLE_TYPE), \
    INCLUDE_OPT_NAMED_BUNDLE(STANDARD, STRAND_DEF, MULTI),		\
    INCLUDE_KEYED_BUNDLES(STANDARD, STRAND_DEF, STRAND_DEFS),		\
    ADD_OPT_CONST_BUNDLEPTRS(BLUEPRINT, AUTOMATIONS, SCRIPTLET)

// holds a single data value, exposing the strand_type
// NEXT and PREV can be used to make singly and doubly linked lists
//
// the DATA here is a pointer to an implemetation defined data tuple, with name "DATA" and the TYPE being defined
// by the value held in STRAND_TYPE
//
// NATIVE_TYPE and NATIVE_SIZE can optionally be used to record these details
// allowing implementations to store native data types
//
// VALUE is extended by attribute
#define _VALUE_BUNDLE ADD_OPT_READONLY_STRAND(VALUE, STRAND_TYPE), ADD_OPT_STRAND(VALUE, DATA), \
    ADD_OPT_READONLY_STRAND(AUTOMATION, RESTRICTIONS),			\
    ADD_OPT_STRAND(INTROSPECTION, NATIVE_TYPE),	ADD_OPT_STRAND(INTROSPECTION, NATIVE_SIZE), \
    ADD_NAMED_OPT_STRAND(VALUE, ARRAY_SIZE, MAX_SIZE), INCLUDE_OPT_BUNDLE(VALUE, NEXT), \
    ADD_OPT_CONST_BUNDLEPTR(VALUE, PREV, VALUE)

// STRAND_DEF is a bundle designed to be included in BLUEPRINT bundles

// RESTRICTIONS is a conditional which defines the conditions to allow data to be accepted, for example
// restricting to certain values, for bundleptrs, it can define
// the bundle_type (and for objects, type , subtype, state)
// allowed, hook automations will be added to automate this if appropriate
//
// for variable strands, the strand type will be set to STRAND_TYPE_PROXIED,
// and TYPE_PROXY which then becomes mandatory, is set to the name
// of a second strand in the bundle whose UINT value defines the actual strand_type
// of this strand, normally "STRAND_TYPE",
// and an optional third strand may hold the restrictions, the name of this strand is held in  "RESTRICT_PROXY"
// NAME is mandatory for strand_defs, unless it is created as a TEMPLATE strand_def
// setting type_proxy or restrictions_proxy effectively adds these strands as optional, readonly strands to the blueprint
// for the bundle_type
#define _STRAND_DEF_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(GENERIC, FLAGS), \
      ADD_OPT_STRAND(GENERIC, NAME),					\
      ADD_STRAND(VALUE, STRAND_TYPE),  ADD_OPT_STRAND(STDEF, TYPE_PROXY), \
      ADD_OPT_READONLY_STRAND(AUTOMATION, RESTRICTIONS), ADD_OPT_STRAND(STDEF, RESTRICT_PROXY), \
      ADD_OPT_STRAND(VALUE, DEFAULT), ADD_NAMED_OPT_STRAND(VALUE, ARRAY_SIZE, MAX_SIZE)

// may point to "before" and "after" values for data_hook, as well as being useful for
// comparison functions
// CONST or NON CONST
#define _VALUE_CHANGE_BUNDLE ADD_OPT_CONST_BUNDLEPTR(VALUE, OLD, VALUE), \
    ADD_OPT_CONST_BUNDLEPTR(VALUE, NEW, VALUE)

// TODO
#define _ERROR_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(ERROR, SOURCE, SEGMENT)

// to utilize a CONDLOGIC node, the PROCESSOR scriptlet shall be actioned,
// the sctipt takes a single parameter, a bunlde which is defined elsewhere
// and returns a CONDLOGIC node, or NULL. If NULL is returned then the output is the DEFAULT
// (defined elsewhere), otherwise the next CONDLOGIC node shall be examined. If
// it contains a VALUE strand, this is the output, other the next processor sript shall be run
// again with the input bundle as parameter
// the format of the SCRIPT is next_node =  NIRVA_SCRIPT_EXECUTE(<PROCESSOR>, <input_bundle>)
#define _CONDLOGIC_NODE_BUNDLE EXTEND_BUNDLE(DEF),		\
    ADD_CONST_BUNDLEPTR(INPUT, PROCESSOR, SCRIPTLET),		\
    ADD_NAMED_OPT_STRAND(CONDLOGIC, WEIGHTING, WEIGHT),		\
    ADD_OPT_STRAND(LOGIC, OP), ADD_OPT_STRAND(CASCADE, VALUE)

//always CONST ?
#define _CASCMATRIX_NODE_BUNDLE EXTEND_BUNDLE(DEF),			\
      ADD_CONST_BUNDLEPTR(CASCMATRIX, OTHER_IDX, CASCMATRIX_NODE),	\
      ADD_CONST_BUNDLEPTR(CASCMATRIX, ON_SUCCESS, CASCMATRIX_NODE),	\
      ADD_CONST_BUNDLEPTR(CASCMATRIX, ON_FAIL, CASCMATRIX_NODE),	\
      ADD_STRAND(CASCMATRIX, OP_SUCCESS),				\
      ADD_STRAND(CASCMATRIX, OP_FAIL)

#define _MATRIX_2D_BUNDLE EXTEND_BUNDLE(DEF), ADD_NAMED_ARRAY(VALUE, BUNDLEPTR, VALUES)

// CASCADE is a kind of multi purpose logic processor
// the IN_TEMPLATE will a mutated BLUEPRINT. The cascade will take a nromal blueprint,
// and make alterations to it. Some optional strands may be marked as non optional and vice versa
// some may be marked as readonly
// the caller should use this in_template and create a bundle from it. The non optional strands
// any non optional strands will be set to default. Any strands not flagged as readonly may have their values
// altered.
// for example, the in_template could be a blueprint for an ICAP_BUNDLE with a readonly intent
// and read / write CAPS, or it cold be a TRANSFORM bundle blueprint with all strands defined except for CAPS
// Sometimes the caller may already have a bundle which matches the theoretical one whcih would be created from the blueprint
// including missing values, in this case this can be used as input
// the cascade will have a const bundleptr pointing to a contract. This is to be use as
#define _CASCADE_BUNDLE EXTEND_BUNDLE(DEF),				\
      INCLUDE_NAMED_BUNDLE(STANDARD, BLUEPRINT, IN_TEMPLATE),		\
      INCLUDE_NAMED_BUNDLE(STANDARD, STRAND_DEF, VALUE_DEF),		\
      INCLUDE_OPT_BUNDLES(CASCADE, CONDLOGIC),				\
      ADD_CONST_BUNDLEPTR(CASCADE, CURRENT_NODE, CONDLOGIC_NODE)

// selector can be used to process a set of candidates and divide them into matches and non
// matches, alternately, given 2 sets, can try to find the criteria itself
#define _SELECTOR_BUNDLE EXTEND_BUNDLE(DEF),			\
    INCLUDE_NAMED_BUNDLE(STANDARD, CASCADE, CRITERIA),		\
    ADD_NAMED_OPT_ARRAY(VALUE, CONST_BUNDLEPTR, CURRENT),	\
    ADD_NAMED_OPT_ARRAY(VALUE, CONST_BUNDLEPTR, MATCHES),	\
    ADD_NAMED_OPT_ARRAY(VALUE, CONST_BUNDLEPTR, NON-MATCHES),	\
    ADD_NAMED_OPT_ARRAY(VALUE, CONST_BUNDLEPTR, CANDIDATES)

// this bundle is the counterpart for keyed_arrays
// or can be used separately
// DEF exists only for the bundle_type + the template strand is stored
// is stored in MULTI, and has strand_type_proxy. Thus, strand_type and restrictions define the type
// of data, with a multi strand, we can add any number of strands with unique names.
#define _INDEX_BUNDLE EXTEND_BUNDLE(DEF), ADD_TEMPLATE_STRAND(VALUE, ANY), \
    ADD_STRAND(VALUE, STRAND_TYPE), ADD_OPT_READONLY_STRAND(AUTOMATION, RESTRICTIONS)

#define KEYED_ARRAY_AUTOMATIONS						\
    ADD_HOOK_AUTO_START(ARRAY, ITEM_APPENDED, _COND_ALWAYS),		\
      HOOK_AUTO_ADD_HOOK_AUTO(@NEW_VAL, DATA, ITEM_APPENDED, _COND_ALWAYS), \
      HOOK_AUTO_ADD_STRAND_TO(INDEX, @NEW_VAL.@*KEY_NAME, STRAND_TYPE_DEFAULT, @NEW_VALUE), \
									\
      ADD_HOOK_AUTO_START(ARRAY, ITEM_APPENDED, _COND_ALWAYS),		\
      HOOK_AUTO_ADD_HOOK_AUTO(@NEW_VAL, DATA, ITEM_REMOVED, _COND_ALWAYS), \
      HOOK_AUTO_DELETE_STRAND_FROM(INDEX, @OLD_VAL.@*KEY_NAME),		\
    									\
      ADD_HOOK_AUTO_START(ARRAY, ITEM_REMOVED, _COND_ALWAYS),		\
      HOOK_AUTO_DELETE_STRAND_FROM(INDEX, OLD_VALUE.OWNER_UID)

// this bundle describes an attribute, but is "inert" - has no value or connections
// some attribute types map to strand types and vice versa  but the values are not the same
// when constructed, we have up to three value "types" - "attr_type", "strand_type", and "native_type"
// DEFAULT is ignored if the attr_def is marked readonly
//
// similar to strand def, array size, max_values  is 0 (implied, default)
// for scalars or -1 for unlimited arrays
// a non zero positive  value implies a fixed array size
// RESTRICTIONS is used to restrict acceptable values for ATTR_TYPE_BUNDLEPTR
// this is copied to the ATTRIBUTE_BUNDLE, and may be adjusted there
// STRAND_TYPE must be set to the value corresponding to ATTR_TYPE
// this is intended to be type_proxy for default and new_default, and must be copied to attributes
// created from this template, since it is also the type_proxy for data
// restrictions is an optional strand that serves a similar purpose for restrict_proxy
// and must also be copied to attributes
#define _ATTR_DEF_BUNDLE EXTEND_BUNDLE(DEF), ADD_READONLY_STRAND(GENERIC, FLAGS), \
    ADD_READONLY_STRAND(GENERIC, NAME), ADD_READONLY_STRAND(ATTRIBUTE, ATTR_TYPE), \
    ADD_READONLY_STRAND(VALUE, STRAND_TYPE),				\
    ADD_OPT_STRAND(VALUE, MAX_VALUES), INCLUDE_OPT_READONLY_BUNDLE(ATTRIBUTE, DEFAULT), \
    ADD_OPT_STRAND(ATTRIBUTE, NEW_DEFAULT), ADD_OPT_READONLY_STRAND(AUTOMATION, RESTRICTIONS)

// all data which is not bundle_strands, is held in attributes. They come in a wide variety
// of types, can map underlying values, as well as be connected remotely to other attributes
// they can map to and from function parameters, can hold sacalar values or arrays,
// contain sub-bundles or point to external bundles. They can be readonly, read/write,
// volatile, const, optional, mandatory. They usually come attr_groups,
// and can be constructed from attr_defs or attr_def_groups
// when an attribute is created from a template attr_def, NAME, FLAGS and ATTR_TYPE must be copied over
//
// objects may have their own internal attributes, trajectories also have input and output attr_groups.
// These are passed to each functional along the trajectory.
// Scripts and conditions may also have a referenced attr bundle.
// CONDITIONS can be set and FLAGS adjusted as part of a contract negotiation
//
// BLOB is used when passing attributes to native functions in a transform. The DATA is read always as an array
// the pointer returned is then stored in BLOB, cast to genptr.
// then when the native function is called, this is cast to a pointer of the correct type, and if scalar, dereferenced.
// BLOB_ARRAY_SIZE holds the size whilst the DATA is in BLOB. Native functions which output arrays which are either strings of arrays,
// or extern outputs from the transform orneed to provide a means to find the
// array size after return. If this is not done, the BLOB_ARRAY_SIZE strand will not be created and the BLOB will not be copied
// to the ATTRIBUTE, trying to read the array size whilst BLOB exists and ARRAY_SIZE does not will produce an error,
// as will ouput of a string array with no blob_size.
//
// If a following functional is also a native function, we pass it the same BLOBs, which avoids having to keep writing and
// reading from attr DATA. READING directly from the attribute, causes BLOB to be copied back, and when the tx ends we also copy back.
#define _ATTRIBUTE_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(VALUE), ADD_STRAND(GENERIC, FLAGS),	\
    ADD_READONLY_STRAND(GENERIC, NAME), ADD_READONLY_STRAND(ATTRIBUTE, ATTR_TYPE), \
    INCLUDE_OPT_NAMED_BUNDLE(ATTRIBUTE, CONNECTION, CONNECTION_OUT),	\
    ADD_NAMED_OPT_ARRAY(VALUE, GENPTR, BLOB), ADD_NAMED_OPT_STRAND(VALUE, UINT, BLOB_SIZE), \
    ADD_OPT_CONST_BUNDLEPTR(ATTRIBUTE, TEMPLATE, ATTR_DEF)

// NB: for connected attrs
// when reading the data from the local attribute, first lock remote, then if non-null,
// lock remote.connection, then read "data", unlock remote.connection, then unlock remote

#define _COND_GEN_002 _COND_FUNC_RET NIRVA_MAKE_REQUEST, ATTACH, _COND_ATTR_VAL(NEW_VAL)

// read / write connections are possible but require an attach request
// the hook stack here is for the detaching hook, which connectors may use to
// detatch their end, these callbacks should be called if the attribute is about to be deleted
#define _ATTR_CONNECTION_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(GENERIC, FLAGS), \
    ADD_CONST_BUNDLEPTR(REMOTE, CONNECTION, ATTRIBUTE)

// source bundle / item is the subject of the functional (which may be derived from the segment, trajectory,
// or transform enclosing it)
//, target is the object (in the grammatical
// sense). For example when checking conditions to see if an object can be added to an array
// the target_bundle would be the bundle / attribute, target_item would be the strand, eg. "DATA"
// and "new_value" in the func_data value_change would hold the value to be added
// CALLER is the UID of the entity actioning the request / transform
#define _EMISSION_BUNDLE ADD_OPT_STRAND(EMISSION, CALLER_UID),	\
    ADD_OPT_CONST_BUNDLEPTR(EMISSION, SOURCE_BUNDLE, ANY),	\
    ADD_OPT_STRAND(EMISSION, SOURCE_ITEM),			\
    ADD_OPT_CONST_BUNDLEPTR(EMISSION, TARGET_BUNDLE, ANY),	\
    ADD_OPT_STRAND(EMISSION, TARGET_ITEM),			\
    ADD_NAMED_OPT_STRAND(DATETIME, TIMESTAMP, TRANSMIT_TIME)

// for standard_functions, this bundle is passed as the sole paramter to the function
// (for native functions, some of the attributes are mapped to / from parameters,
// and those are passed in instead, for scripts, the attrs are available as variables)
// TRAJECTORY allows for the input to be passed to several segments in sequence,
//  //
// whilst hook_details and value_change are extra components specifically used in hook callbacks
// and cascade can be used in cases where the function requires one
//
// RETURN_VALUE is for quick responses from helper functionals,
// RESPONSES is an array for return values from condtionals, this can be used to discover which conditions
// failed and which succeeded
//
// TRANSFORM is a pointer to the transform which this is attatched to
// SEG_ATTRS holds attrs which are internal to a segment, these are destroyed after the segment completes
#define _FUNC_DATA_BUNDLE EXTEND_BUNDLE(DEF),				\
    ADD_CONST_BUNDLEPTR(LINKED, TRANSFORM, TRANSFORM),			\
    INCLUDE_OPT_NAMED_BUNDLE(STANDARD, ATTR_GROUP, SEG_ATTRS),		\
    ADD_CONST_BUNDLEPTR(TRANSFORM, CURRENT_SEGMENT, SEGMENT),		\
    ADD_OPT_NAMED_STRAND(VALUE, GENPTR, RETURN_VALUE),			\
    ADD_STRAND(TRANSFORM, STATUS),					\
    ADD_OPT_CONST_BUNDLEPTR(FUNCTIONAL, CASCADE, CASCADE),		\
    ADD_OPT_ARRAY(FUNCTIONAL, RESPONSES),				\
    ADD_OPT_CONST_BUNDLEPTRS(TRANSFORM, THREADS, OBJECT_INSTANCE)

#define _LOCATOR_BUNDLE EXTEND_BUNDLE(DEF),				\
    ADD_OPT_STRAND(LOCATOR, UNIT), ADD_OPT_STRAND(LOCATOR, SUB_UNIT),	\
    ADD_OPT_STRAND(LOCATOR, INDEX), ADD_OPT_STRAND(LOCATOR, SUB_INDEX)

// i.e    MAKE_EXCLUSIVE("@SCRIPTLET", "@STANDARD_FUNCTION", "@NATIVE_FUNCTION")
// always CONST
//
// bundle which describes and wraps a function (standard, native, or script)
//  "NAME" should be the function name
// ATTR_MAPS map attr_defs in the SEGMENT to parameters for native functions
//
/// if added to a segment, then the output attributes can be fed to another functional in the segment,
// so as to avoid making these inputs to the trajectory/transform, if these attributes are not mapped
// EXTERN, then they will be destroyed when the final functon in the segment returns

#define _FUNCTIONAL_BUNDLE EXTEND_BUNDLE(DEF),			\
    ADD_STRAND(GENERIC, NAME),					\
    ADD_STRAND(FUNCTIONAL, CATEGORY),				\
    ADD_NAMED_OPT_STRAND(VALUE, UINT64, EXT_ID),		\
    ADD_READONLY_STRAND(FUNCTIONAL, FUNC_TYPE),			\
    ADD_OPT_CONST_BUNDLEPTR(STANDARD, SCRIPT_FUNC, SCRIPTLET),	\
    ADD_NAMED_OPT_STRAND(VALUE, FUNCPTR, STANDARD_FUNCTION),	\
    INCLUDE_OPT_BUNDLES(FUNCTIONAL, ATTR_MAPS),			\
    ADD_NAMED_OPT_STRAND(VALUE, FUNCPTR, NATIVE_FUNCTION),	\
    INCLUDE_OPT_NAMED_BUNDLE(STANDARD, LOCATOR, CODE_REF)

// for functionals - attr_defs are added firstly to the segment; it is necessary to define any that are
// mapped to native paramters / return values, plus any which are EXTERN (imported or exported from the segment)
/// non-extern attributes are created within the segment, and destroyed when the segment ends
// the latter only need to be declared in cases where they also map to native param inputs or return values
// when attribute values are first read within a transform, the value is copied to a memory area and passed around
// as zero copy. The memory values are copied back to attrs when the trajectory completes.
// CONDITIONS are for optional external outputs - this is for information
// for extern param in_out and return values which are arrays, we need to know the array size, so eventually this can be mapped
// back to an attribute. In this case, if the array size is a constant, it can be set in ARRAY_SIZE, or if it needs
// calculating, then we can create a special ARRAY_SIZE_FUNC functional, and insert this in the same segment, These functionals
// will be skipped over when the segmetn is executed. The functional can have mapping like any other functional
// or we can use a predefined one, NIRVA_NULL_TERMINATED, which will count up to a 0 value. The functional must return a UINT
// "number_of_elements". This can be 0 or any postive value. We can also specifiy whether the calculation should be run
// before or after using the NIRVA_ATTR_MAP_CALC_FIRST flag bit in MAPPING
#define _ATTR_MAP_BUNDLE ADD_CONST_BUNDLEPTR(ATTRMAP, ATTR_DEF, ATTR_DEF), ADD_STRAND(ATTRMAP, MAPPING), \
    ADD_NAMED_OPT_STRAND(VALUE, UINT, ARRAY_SIZE), ADD_NAMED_OPT_STRAND(VALUE, STRING, ARRAY_SIZE_FUNC), \
    ADD_NAMED_OPT_STRAND(VALUE, STRING, CONDITIONS)

// the attr maps here are only for EXPORTED attributes
// when the segment is appended to a trajectory, these mappings are collated for all segments
// into the TRAJECTORY maps, at this level we have the set of all exported ins / outs from all the segments
// if a segment is only run conditionally, its unique ins / outs become become optional ins / outs
// whenever there is a trajectory sequence which can complete without that attr being defined.
// Likewise the segment outputs beoome optional, the conditions for producing them include
// all conditons to arrive at the segment as well as having values for optional inputs to the segment.
// All the condtions for this are ANDED. There may be additional conditions for producing the output which
// functional can add in the output att_def. These are ANDED as well, if there are alternate paths through
// the trajecotry which produce the outputs, the corresponding conditions are OR ed.
#define _SEGMENT_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_NAMED_BUNDLES(STANDARD, FUNCTIONAL, FUNCTIONALS), \
    INCLUDE_NAMED_BUNDLE(STANDARD, ATTR_DEF_GROUP, ATTR_DEFS), INCLUDE_OPT_BUNDLES(FUNCTIONAL, ATTR_MAPS)

// scriptlet is an array of strings with pseudocode to be parsed at runtime
// the types here are function categories
// - condition, selector, automation, callback
// PMAPS is an optional array of STRAND_DEFs, this acts like a mini blueprint, when the script is run
// we construct ant empty bundle, then the PMAPS are used to create strands in the bundle
// the script can then access the data in these strands, using the temp bundle
#define _SCRIPTLET_BUNDLE EXTEND_BUNDLE(DEF), ADD_READONLY_STRAND(FUNCTIONAL, CATEGORY), \
    ADD_OPT_STRAND(SCRIPTLET, MATURITY), ADD_NAMED_OPT_STRAND(THREADS, NATIVE_MUTEX, REORG_MUTEX), \
    INCLUDE_OPT_NAMED_BUNDLES(STANDARD, STRAND_DEF, PMAPS), ADD_ARRAY(SCRIPTLET, STRINGS)

// each of the following has different "restrictions" which are set in STRAND_DEF for
// keyed_array. these are added in gen3 only
//
// hook callbacks nay be added here instead of for
// individual attributes
// the changes will be "bunched" together and only VALUE_UPDATED triggered
// in addition there will be stacks for appended, removed
// "OWNER" here is not the same as a bundle 'Owner' (toplevel container), but denotes which agents are allowed to
// add / remove and change flags for attributes in the group, for transforms, this is initially set to the contract owner
// but once the transform is actioned, this is changed to the uid of the caller object
#define _ATTR_GROUP_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_OPT_KEYED_BUNDLES(STANDARD, ATTRIBUTE, ATTRIBUTES), \
    ADD_OPT_STRAND(FRAMEWORK, OWNER)

// container for an array of ATTR_DEF
// can contain EITHER an array of ATTR_DEF, OR a sub group (another ATTR_DEF_GROUP)
// if a sub group, then MAX_REPEATS can define the allowed repetitions, -1 == unlimited
// >= 1 max number, 0 / not defined means optional
/// e.g we can have an attr_def_group for a video effect, this can have sub groups for in and out channels
#define _ATTR_DEF_GROUP_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_NAMED_BUNDLES(STANDARD, ATTR_DEF, ATTR_DEFS), \
    INCLUDE_NAMED_BUNDLES(STANDARD, ATTR_DEF_GROUP, SUB_GROUPS), ADD_NAMED_OPT_STRAND(VALUE, INT, MAX_REPEATS)

// restrict to OBJECT_RESTRICTION, STRAND_TYPE_CONST_BUNDLEPTR, key is derived from UID
#define _OBJECT_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_KEYED_BUNDLEPTRS(HOLDER, OBJECTS, OBJECT)

// restrict to SCRIPTLET_RESTRICTION, STRAND_TYPE_CONST_BUNDLEPTR, key is derived from UID
#define _SCRIPTLET_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), ADD_OPT_KEYED_BUNDLEPTRS(HOLDER, SCRIPTLETS, SCRIPTLET)

// each function will be called in the manner depending on the flags in the corresponding stack_header
// for native_funcs, PMAP also comes from the hook stack header
// timestamp may be set when the added to the queue
#define _HOOK_CB_FUNC_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, HANDLE), \
    INCLUDE_BUNDLE(STANDARD, EMISSION), INCLUDE_BUNDLE(FUNCTIONAL, FUNC_DATA)

// details defined at setup for each  hook number
// these are placed in a cascade so conditions translate to hook number
#define _HOOK_DETAILS_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, MODEL), ADD_STRAND(HOOK, NUMBER), \
    ADD_STRAND(GENERIC, FLAGS), INCLUDE_OPT_BUNDLES(SEGMENT, ATTR_MAPS), \
    INCLUDE_OPT_BUNDLES(AUTOMATION, CONDITIONS)

#define _HOOK_STACK_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, NUMBER), \
    ADD_STRAND(EMISSION, TARGET_ITEM), INCLUDE_OPT_BUNDLES(HOOK, CALLBACKS), \
    ADD_CONST_BUNDLEPTR(AUTOMATION, SCRIPT, SCRIPTLET)

// templates have no subtypes (SUBTYPE_NONE), but they do have a type and a state
// hook stacks should be created when an item is added for data_change of any strand
// (before and after) plus init and free,
// plus destroy_hook
// contracts actioned by templates are
// "static transforms"
#define _OBJECT_TEMPLATE_BUNDLE EXTEND_BUNDLE(DEF),			\
      ADD_OPT_STRAND(INTROSPECTION, COMMENT),				\
      ADD_OPT_STRAND(INTROSPECTION, PRIVATE_DATA),			\
      ADD_READONLY_STRAND(OBJECT, TYPE), ADD_STRAND(OBJECT, STATE),	\
      INCLUDE_OPT_NAMED_BUNDLE(STANDARD, ATTR_GROUP, ATTRIBUTES),	\
      INCLUDE_BUNDLES(OBJECT, CONTRACTS),				\
      INCLUDE_OPT_NAMED_BUNDLES(OBJECT, ACTIVE_TRANSFORMS, TRANSFORMS),	\
      ADD_STRAND(GENERIC, FLAGS),					\
      INCLUDE_OPT_NAMED_BUNDLES(THREADS, INSTANCE, THREAD_INSTANCE)

// blueprint template will add these to blueprint object instances
#define _BLUEPRINT_ATTR_PACK MAKE_ATTR_PACK(ATTR_BLUEPRINT_STRAND_DEFS, ATTR_BLUEPRINT_BUNDLE_TYPE)

// this bundle is included in object_instance and attribute
// refcount value starts at 0, and can be increased or decreased by triggering
// ref_request and unref_request hooks. For passive (no thread_instance) object instances
// when refcount goes below 0, the object instance will be zombified (see below)
// if the app enables RECYCLER then it will respond to DESTRUCTION_HOOKs
// for full garbage collection, the object strands will be reset to default values
// if the bundle is active, and the ref goes below zero, the effects depend on the status
// if the status is running, then a destroy_request will be triggered, this is a self hook
// it is up to the thread how to handle the destroy request. either it will cancel the transform
// or finish it and then
// generic_uid is a cross check to enure only objects which added a ref can remove one
// when a ref is added, the caller uid should be added to the array, and on unref, a uid should
// be removed. If a caller tries to unref withou addind a ref first, this may be reported
// to the adjudicator who can take appropriate action, including zombifying or recycling

#define _REFCOUNTER_BUNDLE ADD_STRAND(INTROSPECTION, REFCOUNT),		\
    ADD_NAMED_STRAND(THREADS, NATIVE_MUTEX, COUNTER_MUTEX),		\
    ADD_NAMED_STRAND(VALUE, BOOLEAN, SHOULD_FREE),			\
    ADD_NAMED_STRAND(THREADS, NATIVE_MUTEX, DESTRUCT_MUTEX), ADD_NAMED_ARRAY(GENERIC, UID, ADDERS)

// base bundle for object instances
// these bundles should not be created directly, a template object should do that via a
// CREATE_INSTANCE intent
// or an instance may create a copy of itself via the CREATE_INSTANCE intent.
#define _OBJECT_INSTANCE_BUNDLE EXTEND_BUNDLE(OBJECT_TEMPLATE),	\
      ADD_CONST_BUNDLEPTR(OBJECT, TEMPLATE, OBJECT_TEMPLATE),	\
      ADD_STRAND(INSTANCE, SUBTYPE)

/* // intent / capacities bundle */
// index entries are strings (some caps are selections), and names of strands are just the CAP names
#define _CAPS_BUNDLE EXTEND_BUNDLE(DEF), ADD_TEMPLATE_STRAND(VALUE, STRING)

#define _ICAP_BUNDLE ADD_READONLY_STRAND(ICAP, INTENTION), INCLUDE_BUNDLE(ICAP, CAPS)

// when a segment is added to the trajectory, all of its in and out attr_defs are checked

// ATTR_MAPS point to ATTR_DEFS in SEGMENTS, the mapping is analogous to functional ATTR_MAPS
//
// SEGMENT_SELECTOR can be used to select the next segment, FUNC_DATA is used as input for this,
// if not present, segments will be called in array order
#define _TRAJECTORY_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(GENERIC, FLAGS), \
    ADD_READONLY_STRAND(FUNCTIONAL, CATEGORY),				\
    ADD_READONLY_STRAND(FUNCTIONAL, WRAPPING_TYPE),			\
    INCLUDE_OPT_BUNDLES(FUNCTIONAL, ATTR_MAPS),				\
    INCLUDE_BUNDLES(TRAJECTORY, SEGMENTS),				\
    INCLUDE_NAMED_OPT_BUNDLE(STANDARD, SELECTOR, SEGMENT_SELECTOR)

// this is the definition part of contracts, ICAP will have intention, plus union of all possible CAPs
// VALID_CAPS is a CONDTION string which  will define the valid sets of CAPs
// from this template we can make a TRANSFORM
#define _CONTRACT_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(GENERIC, FLAGS), \
      EXTEND_BUNDLE(ICAP), ADD_OPT_READONLY_ARRAY(CONTRACT, VALID_CAPS), \
      INCLUDE_NAMED_OPT_BUNDLE(STANDARD, CASCADE, TRAJECTORY_CHOOSER),	\
      ADD_STRAND(FRAMEWORK, OWNER)

// @SET_DEFS("FLAGS", TX_FLAG_NO_NEGOTIATE, "TRAJECTORY_CHOOSER", "(", CASCADE, "DEFAULT", "(", VALUE, "STRAND_TYPE",
// STRAND_TYPE_CONST_BUNDLEPTR, "DATA", "$trajectory", ")", "OWNER", "$owner)"

// for negotiated transforms:
// EXTRA_CONDITIONS permits the defining of condtions other than "all mandatory input attrs must have a value"
// and "if they are sequential they must be connected"
//
// ATTRIBUTES - see documentation
//
// TX_RESULT will reflect the final status when the transform completes

// for the KEYED_ARRAY, data is NIRVA_STRING, key is derived from the CAP name
// Transforms can be actioned multiple times, provided each instance has its own FUNC_DATA
#define _TRANSFORM_BUNDLE EXTEND_BUNDLE(DEF),				\
    EXTEND_BUNDLE(EMISSION),						\
    ADD_CONST_BUNDLEPTR(CONTRACT, TEMPLATE, CONTRACT),			\
    ADD_STRAND(GENERIC, FLAGS), ADD_NAMED_ARRAY(VALUE, STRING, CAPS),	\
    ADD_CONST_BUNDLEPTR(TRANSFORM, TRAJECTORY, TRAJECTORY),		\
    INCLUDE_OPT_NAMED_BUNDLE(STANDARD, ATTR_GROUP, ATTRIBUTES),		\
    INCLUDE_OPT_BUNDLE(FUNCTIONAL, FUNC_DATA),				\
    INCLUDE_OPT_NAMED_BUNDLES(AUTOMATION, CONDITIONS, EXTRA_CONDITIONS), \
    ADD_OPT_STRAND(TRANSFORM, TX_RESULT)

// automations will be included in blueprint instances, depending on the bundle_type
// they produce - if the bundle has its own hook stacks, the automations will be transferred
// otherwise they will be added to the blueprint instance hook stacks
// when reaching a hook trigger point, if a bundle does not have hook stacks (for smaller bundles
// like value), then the blueprint should be used as a proxy

// automations for blueprint instances, per bundle_type not sure how these will be assigned yet
// will be transferred to instance hooks

/* #define AUTOMATION_DEFAULT_001 ADD_HOOK_AUTOMATION(ADDING_STRAND, !*, _COND_CHECK_RESULT, \ */
/* 						   BLUEPRINT_HAS_STRAND, !NEW_VAL.NAME, \ */
/* 						   COND_LOGIC_OR, _COND_PRIV_CHECK, PRIV_STRUCTURE, \ */
/* 						   100, _COND_END) */

/* #define AUTOMATION_DEFAULT_002 ADD_HOOK_AUTOMATION(DELETIING_STRAND, !*, _COND_CHECK_RESULT, \ */
/* 						   BLUEPRINT_STRAND_OPT, !OLD_VAL.NAME, \ */
/* 						   COND_LOGIC_OR, _COND_PRIV_CHECK, PRIV_STRUCTURE, \ */
/* 						   100, _COND_END) */

/* #define AUTOMATION_DEFAULT_003 ADD_HOOK_AUTOMATION(DESTRUCTION_REQUEST, !* , _COND_CHECK_RESULT, \ */
/* 						   _COND_VAL_EQUALS, !SOURCE_OBJECT, \ */
/* 						   !TARGET_OBJECT,	\ */
/* 						   COND_LOGIC_OR, _COND_PRIV_CHECK, PRIV_STRUCTURE, \ */
/* 						   500, _COND_END) */

/* #define AUTOMATION_DEFAULT_004 ADD_HOOK_AUTOMATION(DESTRUCTION, !* , _COND_SELECT, _COND_VAL_EQUALS, \ */
/* 						   STRAND_TYPE, CONST_UINT_VAL STRAND_TYPE_BUNDLEPTR, \ */
/* 						   _COND_END, UNREF_BUNDLE,\ */
/* 						   STRAND_BUNDLEPTR_VAL *!SELECTOR) */


/* #define TRANSFORM_AUTOMATIONS ADD_HOOK_AUTOMATION(STATUS, VALUE_UPDATED, _COND_ALWAYS, CASCADE_HOOKS), \ */
/*     ADD_HOOK_AUTOMATION(STATUS, UPDATING_VALUE, _COND_ALWAYS, CASCADE_HOOKS) */

/* #define STRAND_DEF_AUTOMATIONS						\ */
/*   ADD_HOOK_AUTOMATION(*, ADDING_STRAND, STRAND_VALUE_DEFAULT,		\ */
/* 		      SET_STRAND_TYPE, STRAND_VALUE_DEFAULT,		\ */
/* 		      STRAND_UINT_VAL STRAND_VALUE_STRAND_TYPE),	\ */
/*     ADD_HOOK_AUTOMATION(STRAND_STRAND_TEMPLATE, VALUE_UPDATED, _COND_ALWAYS, COPY_STRAND_VALUE, \ */
/* 			STRAND_GENERIC_NAME, *!SELF.STRAND_GENERIC_NAME), \ */
/*     ADD_HOOK_AUTOMATION(STRAND_STRAND_TEMPLATE, VALUE_UPDATED, _COND_ALWAYS, COPY_STRAND_VALUE, \ */
/* 			STRAND_VALUE_STRAND_TYPE, *!SELF.STRAND_VALUE_STRAND_TYPE), \ */
/*     ADD_HOOK_AUTOMATION(STRAND_STRAND_TEMPLATE, VALUE_UPDATED, _COND_ALWAYS, COPY_STRAND_VALUE, \ */
/* 			STRAND_VALUE_DATA, *!SELF.STRAND_VALUE_DEFAULT) */

/* #define VALUE_AUTOMATIONS */

/* #define OBJECT_INSTANCE_AUTO ADD_HOOK_AUTOMATION(SUBTYPE, VALUE_UPDATED, _COND_ALWAYS, CASCADE_HOOKS), \ */
/*     ADD_HOOK_AUTOMATION(STATE, VALUE_UPDATED, _COND_ALWAYS, CASCADE_HOOKS), \ */
/*     ADD_HOOK_AUTOMATION(SUBTYPE, UPDATING_VALUE, _COND_ALWAYS, CASCADE_HOOKS), \ */
/*     ADD_HOOK_AUTOMATION(STATE, UPDATING_VALUE, _COND_ALWAYS, CASCADE_HOOKS) */

/* #define REFCOUNTER_AUTO \ */
/*   ADD_HOOK_AUTOMATION(REFCOUNT, UPDATING_VALUE, _COND_CHECK, NIRVA_VAL_LT, \ */
/* 		      STRAND_INT_VAL STRAND_INTROSPECTION_REFCOUNT, CONST_INT_VAL 0, \ */
/* 		      NIRVA_OP_FIN, SET_STRAND_VALUE, SHOULD_FREE, _COND_INT_VAL 1) */

/* #define ATTR_CONNECTION_AUTO \ */
/*   ADD_HOOK_AUTOMATION(CONNECTION, UPDATING_VALUE, _COND_ALWAYS, CASCADE_HOOKS), \ */
/*     ADD_HOOK_AUTOMATION(CONNECTION, UPDATING_VALUE, _COND_ALWAYS, REMOVE_HOOK_CB, \ */
/* 			OLD_VALUE, DESTRUCTION),			\ */
/*     ADD_HOOK_AUTOMATION(CONNECTION, UPDATING_VALUE, _COND_CHECK_RETURN, _COND_GEN_002), \ */
/*     ADD_HOOK_AUTOMATION(CONNECTION, VALUE_UPDATED, _COND_ALWAYS,	\ */
/*     			ADD_HOOK_CB, NEW_VALUE, DESTRUCTION,		\ */
/*     			SET_STRAND_VALUE, _COND_ALWAYS, CONNECTION, NIRVA_NULL) */

/////

#if NIRVA_IMPL_IS(DEFAULT_C)

#define DEBUG_BUNDLE_MAKER
#ifdef DEBUG_BUNDLE_MAKER
#define p_debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define p_debug(...)
#endif

// incremental macros, we can define a macros like:
//#define do_macro(a) var[##a##] = CONST_##a;
//#define do_macro2(num) var == num ? CONST_##a :
//
// then: NIRVA_DO_nn_times(4,2,do_macro) // expand do_macro for all values from 0 to 42
// and: NIRVA_DO_n_times(7,do_macro2) // expands to do_macro2 for all values from 0 to 7
// furthermore, CONST_0, CONST_1, etc can themselves be macros, provided there is no recursion, this can go several levels deep
#define NIRVA_DO_n_times(n,m) NIRVA_DO_FOR_##n(m)
#define NIRVA_DO_FOR_0(m)m(0)
#define NIRVA_DO_FOR_1(m)m(0) m(1)
#define NIRVA_DO_FOR_2(m)m(0) m(1) m(2)
#define NIRVA_DO_FOR_3(m)NIRVA_DO_FOR_2(m) m(3)
#define NIRVA_DO_FOR_4(m)NIRVA_DO_FOR_3(m) m(4)
#define NIRVA_DO_FOR_5(m)NIRVA_DO_FOR_4(m) m(5)
#define NIRVA_DO_FOR_6(m)NIRVA_DO_FOR_5(m) m(6)
#define NIRVA_DO_FOR_7(m)NIRVA_DO_FOR_6(m) m(7)
#define NIRVA_DO_FOR_8(m)NIRVA_DO_FOR_7(m) m(8)
#define NIRVA_DO_FOR_9(m)NIRVA_DO_FOR_8(m) m(9)

#define NIRVA_DO_FOR_x0(x,m)m(x##0)
#define NIRVA_DO_FOR_x1(x,m)NIRVA_DO_FOR_x0(x,m) m(x##1)
#define NIRVA_DO_FOR_x2(x,m)NIRVA_DO_FOR_x1(x,m) m(x##2)
#define NIRVA_DO_FOR_x3(x,m)NIRVA_DO_FOR_x2(x,m) m(x##3)
#define NIRVA_DO_FOR_x4(x,m)NIRVA_DO_FOR_x3(x,m) m(x##4)
#define NIRVA_DO_FOR_x5(x,m)NIRVA_DO_FOR_x4(x,m) m(x##5)
#define NIRVA_DO_FOR_x6(x,m)NIRVA_DO_FOR_x5(x,m) m(x##6)
#define NIRVA_DO_FOR_x7(x,m)NIRVA_DO_FOR_x6(x,m) m(x##7)
#define NIRVA_DO_FOR_x8(x,m)NIRVA_DO_FOR_x7(x,m) m(x##8)
#define NIRVA_DO_FOR_x9(x,m)NIRVA_DO_FOR_x8(x,m) m(x##9)

#define NIRVA_DO_FOR_x(a,b,m)NIRVA_DO_FOR_x##b(a,m)
#define NIRVA_DO_FOR_xx1(b,m)NIRVA_DO_n_times(9,m) NIRVA_DO_FOR_x(1,9,m)
#define NIRVA_DO_FOR_xx2(b,m)NIRVA_DO_FOR_xx1(9,m) NIRVA_DO_FOR_x(2,b,m)
#define NIRVA_DO_FOR_xx3(b,m)NIRVA_DO_FOR_xx2(9,m) NIRVA_DO_FOR_x(3,b,m)
#define NIRVA_DO_FOR_xx4(b,m)NIRVA_DO_FOR_xx3(9,m) NIRVA_DO_FOR_x(4,b,m)
#define NIRVA_DO_FOR_xx5(b,m)NIRVA_DO_FOR_xx4(9,m) NIRVA_DO_FOR_x(5,b,m)
#define NIRVA_DO_FOR_xx6(b,m)NIRVA_DO_FOR_xx5(9,m) NIRVA_DO_FOR_x(6,b,m)
#define NIRVA_DO_FOR_xx7(b,m)NIRVA_DO_FOR_xx6(9,m) NIRVA_DO_FOR_x(7,b,m)
#define NIRVA_DO_FOR_xx8(b,m)NIRVA_DO_FOR_xx7(9,m) NIRVA_DO_FOR_x(8,b,m)
#define NIRVA_DO_FOR_xx9(b,m)NIRVA_DO_FOR_xx8(9,m) NIRVA_DO_FOR_x(9,b,m)
#define NIRVA_DO_nn_times(a,b,m) NIRVA_DO_FOR_xx##a(b,m)

//

#define __IMPL_SET__ =
#define __IMPL_EQUIVALENT__ ==

#define __IMPL_OR__ ||
#define __IMPL_AND__ &&
#define __IMPL_NOT__ !
#define __IMPL_TEST__ __IMPL_NOT__ ___IMPL_NOT__

#define __IMPL_SET_BIT__ |=
#define __IMPL_UNSET_BIT__(a,bit) &= ~
#define __IMPL_AND_BITS__ &
#define __IMPL_OR_BITS__ |
#define __IMPL_XOR_BITS__ ^
#define __IMPL_COMPL_BITS__ ~

#define __IMPL_PARENS_OPEN__ (
#define __IMPL_PARENS_CLOSE__ )
#define __IMPL_WS_INDENT__

#define __IMPL_WHILE__ while
#define __IMPL_DO__ do
#define __IMPL_IF__ if
#define __IMPL_FOR__ for
#define __IMPL_LOOP_LAST__ break
#define __IMPL_LOOP_NEXT__ continue

#define __IMPL_RETURN__(r)return r
#define __IMPL_SIZEOF__(a)sizeof(a)
#define __IMPL_TYPEOF__(a)typeof(a)

#define __IMPL_SIZE_TYPE__ size_t

#define __IMPL_BASIC_ALLOC__(size)malloc((size))
#define __IMPL_BASIC_UNALLOC__(ptr)free(ptr)

#define __IMPL_STRING_BUFF__ snprintf
#define __IMPL_STRING_DUPLICATE__(str)strdup(str)
#define __IMPL_STRING_COMPARE__(str1,str2)strcmp(str1, str2)
#define __IMPL_STRING_LENGTH__(str)strlen((str))
#define __IMPL_STRING_LENGTH_COMPARE__(str1,str2)strncmp(str1, str2, __IMPL_STRING_LENGTH__(str2))
#define __IMPL_STRING_LENGTH_TYPE__ __IMPL_SIZE_TYPE__

#define __IMPL_CHAR_NONE__ '\0'
#define __IMPL_PTR_NONE__ NULL
#define __IMPL_INT_NONE__ 0

#define __IMPL_DECL_AS_TYPE__(xtype,a)xtype a
#define __IMPL_DECL_ARRAY_VARSIZE__(a)a[]
#define __IMPL_DECL_ARRAY_NSIZE__(a,n)a[n]
#define __IMPL_POINTER_TO__(a)*a
#define __IMPL_POINTER_TO_TYPE__(xtype)xtype *
#define __IMPL_BYREF__(a)(&(a))
#define __IMPL_DEREF__(a)(*(a))
#define __IMPL_DECL__(a,...)__IMPL_DECL_AS_TYPE__(a,__VA_ARGS__)

#define __IMPL_PTR_TO_TYPE__(a) a *
#define __IMPL_DECLARE_AS_TYPE_MULTI__(xtype,...) xtype __VA_ARGS__

#define __IMPL_CAST_TO_TYPE__(atype,a)((atype) (a))
#define __IMPL_CAST_TO_TYPE_PTR__(xtype,ptr)__IMPL_CAST_TO_TYPE__(__IMPL_PTR_TO_TYPE__(xtype),ptr)
#define __IMPL_CAST_TO_GENPTR__(p)__IMPL_CAST_TO_TYPE__(__IMPL_GENPTR__,p)
#define __IMPL_VAR_FROM_GENPTR__(xtype,ptr)__IMPL_DEREF__(__IMPL_CAST_TO_TYPE_PTR__(xtype,ptr))
#define __IMPL_ARRAY_CAST__(xtype)(xtype*)
#define __IMPL_ARRAY_IDX__(a,idx)a[(idx)]

#define __IMPL_ASSIGN__(a,b)a __IMPL_SET__  b
#define __IMPL_ALLOC__(a)__IMPL_BASIC_ALLOC__(a)
#define __IMPL_ALLOC_SIZEOF__(xtype)__IMPL_ALLOC__(__IMPL_SIZEOF__(xtype))
#define __IMPL_NEW__(xtype,a)__IMPL_ASSIGN__(__IMPL_DECL__(xtype,a),	\
					     __IMPL_ALLOC_SIZEOF__(xtype))
#define __IMPL_ASSIGN_INT__(a,b)__IMPL_ASSIGN__(a,b)
#define __IMPL_ASSIGN_FLOAT__(a,b)__IMPL_ASSIGN__(a,b)
#define __IMPL_ASSIGN_PTR__(a,b)__IMPL_ASSIGN__(a,b)
#define __IMPL_ASSIGN_STRING__(a,b)__IMPL_ASSIGN_PTR__(a,b)
#define __IMPL_COPY_STRING__(dst,src)__IMPL_ASSIGN_STRING__(dst,__IMPL_STRING_DUPLICATE__(src))
#define __IMPL_HAS_BIT__(a,bit) (((a)__IMPL_AND_BITS__ (bit))	\
				 __IMPL_EQUIVALENT__ (bit))
#define __IMPL_JOIN_BIT_(b)__IMPL_OR_BITS__ (b)
#define __IMPL_JOIN_BITS_1__(a)__IMPL_PARENS_OPEN__ (a)__IMPL_PARENS_CLOSE__
#define __IMPL_JOIN_BITS_2__(a,b)__IMPL_PARENS_OPEN__ (a)	\
      __IMPL_JOIN_BIT__(b)__IMPL_PARENS_CLOSE__

#define __IMPL_TEST_EQUAL__(a,b) ((a)__IMPL_EQUIVALENT__(b))
#define __IMPL_TEST_PTR_EQUAL__(a,b)__IMPL_TEST_EQUAL__(a,b)
#define __IMPL_TEST_INT_EQUAL__(a,b)__IMPL_TEST_EQUAL__(a,b)
#define __IMPL_TEST_FLOAT_EQUAL__(a,b)__IMPL_TEST_EQUAL__(a,b)
#define __IMPL_SET_PTR_UNDEF__(a)__IMPL_ASSIGN_PTR_(a,__IMPL_PTR_NONE__)
#define __IMPL_TEST_PTR_UNDEF__(a)__IMPL_TEST_PTR_EQUAL__(a,__IMPL_PTR_NONE__)
#define __IMPL_SET_INT_UNDEF__ __IMPL_ASSIGN_INT__(a,__IMPL_INT_NONE__)
#define __IMPL_TEST_INT_UNDEF__(a)__IMPL_TEST_INT_EQUAL__(a,__IMPL_INT_NONE__)
#define __IMPL_TEST_STRING_EQUAL__(a,b)__IMPL_NOT__(__IMPL_STRING_COMPARE__(a,b))
#define __IMPL_TEST_STRLEN_EQUAL__(a,b)(__IMPL_STRING_LENGTH__(a)>=__IMPL_STRING_LENGTH__(b) \
					?__IMPL_NOT__(__IMPL_STRING_LENGTH_COMPARE__(a,b)):0)
#define __IMPL_TEST_CHAR_EQUAL__(a,b)__IMPL_TEST_EQUAL__(a,b)
#define __IMPL_NTH_CHAR__(str,n)__IMPL_ARRAY_IDX__(str,n)
#define __IMPL_IS_FINAL_CHAR__(c)(__IMPL_CHAR_EQUAL__(c,__IMPL_CHAR_NONE__))
#define __IMPL_IS_EMPTY_STRING__(str)__IMPL_PTR_EQUAL__(a,__IMPL_PTR_NONE__)

#define __IMPL_INLINE_IF_ELSE__(a,b)(a)?(b):

#define __IMPL_CALL_FUNCPTR__(func,...)(*func)(__VA_ARGS__)
#define __IMPL_POST_INC__(c)(c++)
#define __IMPL_PRE_INC__(c)(++c)
#define __IMPL_POST_DEC__(c)(c--)
#define __IMPL_PRE_DEC__(c)(--c)

#define __IMPL_VA_START__(a,b)va_start(a,b)
#define __IMPL_VA_END__(a)va_end(a)
#define __IMPL_VA_ARG__(a,b)va_arg(a,b)

#define NIRVA_MAX_RETVALS 1

#define NIRVA_TYPE_PTR(xtype)__IMPL_POINTER_TO_TYPE__(xtype)

#define NIRVA_CAST_TO(xtype,a)__IMPL_CAST_TO_TYPE__(xtype,a)
#define NIRVA_CAST_TO_PTR(xtype,a)__IMPL_CAST_TO_TYPE_PTR__(xtype,a)

#define NIRVA_GENPTR_CAST(var) NIRVA_CAST_TO(NIRVA_GENPTR,var)
#define NIRVA_VAR_FROM_GENPTR(xtype,ptr)__IMPL_VAR_FROM_GENPTR__(xtype,ptr)

#define NIRVA_NOT __IMPL_NOT__
#define NIRVA_SNPRINTF __IMPL_STRING_BUFF__

#define NIRVA_STRDUP_OFFS(str,offs)__IMPL_STRING_DUPLICATE__(str + offs)

#define NIRVA_FMT(buff,bsize,...)NIRVA_CMD((NIRVA_SNPRINTF(buff,bsize,__VA_ARGS__)))

#define NIRVA_FMT_STRING "%s"
#define NIRVA_FMT_2STRING "%s%s"
#define NIRVA_FMT_INT "%d"
#define NIRVA_FMT_INT3 "%03d"

#define NIRVA_POST_INC(c)__IMPL_POST_INC__(c)
#define NIRVA_PRE_INC(c)__IMPL_PRE_INC__(c)
#define NIRVA_POST_DEC(c)__IMPL_POST_DEC__(c)
#define NIRVA_PRE_DEC(c)__IMPL_PRE_DEC__(c)

#define _CALL_n(macro,n,...)macro_##n(__VA_ARGS__)

#define NIRVA_HAS_FLAG(flags, flagbit)__IMPL_HAS_BIT__(flags,flagbit)

#define NIRVA_PARENS(a)__IMPL_PARENS_OPEN__ a __IMPL_PARENS_CLOSE__

#define NIRVA_BLOCK_START __IMPL_BLOCK_START__
#define NIRVA_BLOCK_END __IMPL_BLOCK_END__

#define NIRVA_FUNC_END NIRVA_BLOCK_END

#define NIRVA_BREAK NIRVA_CMD(__IMPL_LOOP_LAST__)
#define NIRVA_CONTINUE NIRVA_CMD(__IMPL_LOOP_NEXT__)

#define NIRVA_MALLOC(xsize)__IMPL_BASIC_ALLOC__(xsize)
#define NIRVA_FREE(ptr)NIRVA_CMD(__IMPL_BASIC_UNALLOC__(ptr))
#define NIRVA_STRING_FREE(str)NIRVA_FREE(str)
#define NIRVA_MALLOC_SIZEOF(xtype)__IMPL_ALLOC_SIZEOF__(xtype)
#define NIRVA_MALLOC_ARRAY(xtype,nvals)NIRVA_CAST_TO(NIRVA_PTR_TO(xtype), \
						     NIRVA_MALLOC((nvals) * __IMPL_SIZEOF__(xtype)))

#define NIRVA_STRING_LENGTH(str)__IMPL_STRING_LENGTH__(str)

#define NIRVA_DEREF(a)__IMPL_DEREF__(a)

#define NIRVA_SET(var,val)__IMPL_ASSIGN__(var,val)
#define NIRVA_EQUALS(var,val)__IMPL_TEST_EQUAL__(var,val)
#define NIRVA_ASSIGN(var,val,...)NIRVA_CMD(NIRVA_SET(var,val))
#define NIRVA_ASSIGN_RET(a,b)__IMPL_INLINE_IF_ELSE__((a=b)==a,a) a

#define NIRVA_RETURN(res)NIRVA_CMD(__IMPL_RETURN__(res))

#define NIRVA_STRING_ALLOC(str,stlen)					\
  NIRVA_ASSIGN(str,NIRVA_CAST_TO(NIRVA_STRING,NIRVA_MALLOC(stlen + 1)))

#define NIRVA_RETURN_SUCCESS NIRVA_RETURN(NIRVA_RESULT_SUCCESS)
#define NIRVA_RETURN_FAIL NIRVA_RETURN(NIRVA_RESULT_FAIL)
#define NIRVA_RETURN_ERROR NIRVA_RETURN(NIRVA_RESULT_ERROR)

#define NIRVA_END_RETURN(res)NIRVA_RETURN(res) NIRVA_FUNC_END
#define NIRVA_END_SUCCESS NIRVA_END_RETURN(NIRVA_RESULT_SUCCESS)

#define NIRVA_RETURN_RESULT(func,...)NIRVA_CMD(__IMPL_RETURN__(_CALL(func,__VA_ARGS__)))
#define NIRVA_END_RESULT(func,...)NIRVA_RETURN_RESULT(func, __VA_ARGS__) NIRVA_FUNC_END

#define NIRVA_ARRAY_NTH(array,idx)__IMPL_ARRAY_IDX__(array,idx)
#define NIRVA_ASSIGN_FROM_ARRAY(val,array,idx)NIRVA_ASSIGN(val,NIRVA_ARRAY_NTH(array,idx))

#define NIRVA_DEF_VARS(xtype, ...)NIRVA_CMD(__IMPL_DECLARE_AS_TYPE_MULTI__(xtype,__VA_ARGS__))
#define NIRVA_STATIC_VARS(xtype, ...)NIRVA_STATIC NIRVA_DEF_VARS(xtype, __VA_ARGS__)
#define NIRVA_DEF_ARRAY_SIZE(xtype,var,xsize) NIRVA_CMD(xtype  __IMPL_DECL_ARRAY_NSIZE__(var,xsize))

#define NIRVA_DEF_SET(xtype,var,val)NIRVA_DEF_VARS(xtype,NIRVA_SET(var,val))

#define _CALL(MACRO,...) MACRO(__VA_ARGS__)
#define INLINE_FUNC_CALL(name,...)_INLINE_INTERNAL_##name(__VA_ARGS__)

#define NIRVA_IMPL_ASSIGN(var,name,...)NIRVA_CMD((var)__IMPL_SET__	\
						 __IMPL_CALL_FUNCPTR__(impl_func_##name, __VA_ARGS__))

#define NIRVA_RESULT(fname,...)fname(__VA_ARGS__)
#define NIRVA_CALL(fname,...)NIRVA_CMD(fname(__VA_ARGS__))
#define NIRVA_CALL_ASSIGN(var,fname,...)NIRVA_CMD(var=fname(__VA_ARGS__))
#define NIRVA_ASSIGN_CALL(var,fname,...)NIRVA_CALL_ASSIGN(var,fname,__VA_ARGS__)
#define NIRVA_DEF_ASSIGN(xtype,var,fname,...)NIRVA_DEF_VARS(xtype,var) NIRVA_CALL_ASSIGN(var,fname,__VA_ARGS__)
#define NIRVA_OP(op,a,b)_CALL(op,a,b)

#define NIRVA_ARRAY_FREE(array)NIRVA_FREE(array);
#define _NIRVA_WHILE(...)__IMPL_WHILE__(__VA_ARGS__)
#define _NIRVA_DO __IMPL_DO__
#define NIRVA_LOOP_WHILE(...)__IMPL_WHILE__(__VA_ARGS__) NIRVA_BLOCK_START
#define NIRVA_LOOP_END NIRVA_BLOCK_END
#define NIRVA_LOOP_POST_INC(var, start, end)__IMPL_FOR__ __IMPL_PARENS_OPEN__ \
  NIRVA_CMD(NIRVA_SET(var,start))NIRVA_CMD(COND_TEST_LT(var,end))NIRVA_POST_INC(var) \
    __IMPL_PARENS_CLOSE__ NIRVA_BLOCK_START
#define NIRVA_AUTO_FOR(var, start, end)__IMPL_FOR__ __IMPL_PARENS_OPEN__ \
  NIRVA_DEF_SET(NIRVA_INT, var, start)NIRVA_CMD(COND_TEST_LT(var,end))NIRVA_POST_INC(var) \
    __IMPL_PARENS_CLOSE__ NIRVA_BLOCK_START
#define NIRVA_ENDIF NIRVA_BLOCK_END
#define NIRVA_DO __IMPL_DO__ NIRVA_BLOCK_START
#define NIRVA_DO_A_THING(...)__IMPL_DO__ NIRVA_BLOCK_START __VA_ARGS__
#define NIRVA_WHILE(x)NIRVA_BLOCK_END __IMPL_WHILE__ NIRVA_PARENS(x)__IMPL_LINE_END__
#define NIRVA_ONCE NIRVA_WHILE(0)
#define NIRVA_INLINE(...)NIRVA_DO_A_THING(__VA_ARGS__)NIRVA_ONCE
#define NIRVA_IF(a)__IMPL_IF__ NIRVA_PARENS(a)
#define NIRVA_IF_NOT(a)__IMPL_IF__ NIRVA_PARENS(NIRVA_NOT(a))
#define NIRVA_IF_THEN(a,...)NIRVA_IF(a) NIRVA_BLOCK_START __VA_ARGS__ __IMPL_LINE_END__  NIRVA_BLOCK_END
#define NIRVA_IF_NOT_THEN(a,...)NIRVA_IF_NOT(a) NIRVA_BLOCK_START __VA_ARGS__ __IMPL_LINE_END__ \
    NIRVA_BLOCK_END
#define NIRVA_IF_OP(op,a,b,...)NIRVA_IF_THEN(NIRVA_OP(op,a,b),__VA_ARGS__)
#define NIRVA_OP_NOT(op,a,b)NIRVA_NOT(NIRVA_OP(op,a,b))
#define NIRVA_UNLESS_OP(op,a,b,...)NIRVA_IF_THEN(NIRVA_OP_NOT(op,a,b),__VA_ARGS__)
#define NIRVA_IF_EQUAL(a,b,...)NIRVA_IF_OP(NIRVA_EQUAL,a,b,__VA_ARGS__)
#define NIRVA_IF_NOT_EQUAL(a,b,...)NIRVA_UNLESS_OP(NIRVA_EQUAL,a,b,__VA_ARGS__)

#define NIRVA_CONTINUE_IF(op,a,b) NIRVA_IF_OP(op,a,b,NIRVA_CONTINUE)
#define NIRVA_BREAK_IF(op,a,b) NIRVA_IF_OP(op,a,b,NIRVA_BREAK)

#define NIRVA_STRING_EQUAL(a,b)__IMPL_TEST_STRING_EQUAL__(a,b)
#define NIRVA_STRLEN_EQUAL(a,b)__IMPL_TEST_STRLEN_EQUAL__(a,b)

#define NIRVA_MACRO_CALL(macro,...) _CALL(macro,__VA_ARGS__)
#define NIRVA_CALL_IF(op,a,b,macro,...)NIRVA_IF_OP(op,a,b,NIRVA_MACRO_CALL(macro,__VA_ARGS__))
#define NIRVA_CALL_UNLESS(op,a,b,macro,...)NIRVA_UNLESS_OP(op,a,b,NIRVA_MACRO_CALL(macro, \
										   __VA_ARGS__)
#define NIRVA_CALL_IF_EQUAL(a,b,macro,...)NIRVA_IF_EQUAL(a,b,NIRVA_MACRO_CALL(macro,__VA_ARGS__))

#define NIRVA_CALL_FUNC_IF_EQUAL(a,b,func,...)NIRVA_IF_EQUAL(a,b,NIRVA_CALL(func,__VA_ARGS__))
#define NIRVA_CALL_IF_NOT_EQUAL(a,b,macro,...)NIRVA_IF_NOT_EQUAL(a,b,	\
								 NIRVA_MACRO_CALL(macro,__VA_ARGS__))
#define NIRVA_ASSERT(val,func,...)NIRVA_IF_NOT_THEN(val,func(__VA_ARGS__))
#define NIRVA_ASSERT_NULL(val,func,...)NIRVA_IF_NOT_EQUAL(val,NIRVA_NULL,func(__VA_ARGS__))
#define NIRVA_STRING_BUFF(var,size) char var[size];

#endif // end cstyle

#define _SIX_PARAMS 	6
#define _FIVE_PARAMS 	5
#define _FOUR_PARAMS 	4
#define _THREE_PARAMS	3
#define _TWO_PARAMS 	2
#define _ONE_PARAM 	1
#define _NO_PARAMS 	0

#ifndef NIRVA_FUNC_RETURN
#define NIRVA_FUNC_RETURN NIRVA_INT64
#endif

#define NIRVA_REQUEST_REPSONSE NIRVA_FUNC_RETURN
#define NIRVA_COND_RESULT NIRVA_FUNC_RETURN

NIRVA_FUNC_TYPE_DEF(NIRVA_FUNC_RETURN, nirva_function_t, NIRVA_BUNDLEPTR func_data)

NIRVA_FUNC_TYPE_DEF(NIRVA_FUNC_RETURN, nirva_function_t, NIRVA_BUNDLEPTR func_data)
NIRVA_FUNC_TYPE_DEF(NIRVA_NO_RETURN, nirva_wrapper_function_t, NIRVA_GENPTR_ARRAY ivals, NIRVA_GENPTR_ARRAY ovals)

NIRVA_TYPEDEF(nirva_function_t, nirva_condfunc_t)
NIRVA_TYPEDEF(nirva_function_t, nirva_callback_t)

#define NIRVA_WRAPPER_FUNC nirva_wrapper_function_t
#define NIRVA_STANDARD_FUNC nirva_function_t

// define a function for export
#define NIRVA_DEF_FUNC(RET_TYPE, fname, ...)NIRVA_STATIC RET_TYPE fname(__VA_ARGS__) {

#define NIRVA_DECL_FUNC(RET_TYPE, fname, ...)NIRVA_CMD(NIRVA_STATIC RET_TYPE fname(__VA_ARGS__))

#define NIRVA_INTERNAL(fname) NIRVA_INTERNAL_##fname

// define an internal function
#define NIRVA_DEF_FUNC_INTERNAL(ret_type,fname,...)NIRVA_STATIC_INLINE ret_type NIRVA_INTERNAL(fname)(__VA_ARGS__) {

#define NIRVA_DECL_FUNC_INTERNAL(ret_type,fname,...)NIRVA_CMD(NIRVA_STATIC_INLINE ret_type \
							       NIRVA_INTERNAL(fname)(__VA_ARGS__))

// point func_ptr to NIRVA_IMPL_FUNC_
#define NIRVA_ADD_IMPL_FUNC(fname) NIRVA_CMD(nirva_##fname##_f _nirva_##fname = NIRVA_IMPL_FUNC_##fname)

// point func_ptr to NIRVA_EXPORTS_
#define NIRVA_EXPORTS(fname)NIRVA_CMD(nirva_##fname##_f _nirva_##fname = NIRVA_EXPORTED_##fname)

// use NIRVA_MACRO_
#define NIRVA_EXPORTS_NOFUNC(fname)NIRVA_CMD(nirva_##fname##_f _nirva_##fname = NIRVA_NULL)

// if nirva_init is called with no args (the default)
// then this is like NIRVA_AUTO_SEMI, excpet that individual aspect automations
// may be adjusted, giving rise to more or fewer impl funcs.
// with NIRVA_AUTO_FULL
//      none of these functions need to be defined
// if called with NIRVA_AUTO_SEMI,
//  then the mandatory functions + aspect functions need to be defined
// if called with NIRVA_AUTO_MANUAL,
//     then only one function is required, nirva_advise
// if called with NIRVA_AUTO_NONE,
// then no functions need be defined
// these are functions which must be defined, but can wait until after nirva_init
#define NIRVA_NEEDS(fname,rt,...)					\
  NIRVA_FUNC_TYPE_DEF(rt, nirva_##fname##_f,__VA_ARGS__); NIRVA_EXTERN nirva_##fname##_f _nirva_##fname
#define NIRVA_NEEDS_IMPL_FUNC6(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,pt4,p4,pt5,p5,...) \
  NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3,pt4,pt5)
#define NIRVA_NEEDS_IMPL_FUNC5(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,pt4,p4,...) \
  NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3,pt4)
#define NIRVA_NEEDS_IMPL_FUNC4(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,...) \
    NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3)
#define NIRVA_NEEDS_IMPL_FUNC3(fname,rt,pt0,p0,pt1,p1,pt2,p2,...)	\
  NIRVA_NEEDS(fname,rt,pt0,pt1,pt2)
#define NIRVA_NEEDS_IMPL_FUNC2(fname,rt,pt0,p0,pt1,p1,...)NIRVA_NEEDS(fname,rt,pt0,pt1)
#define NIRVA_NEEDS_IMPL_FUNC1(fname,rt,pt0,p0,...)NIRVA_NEEDS(fname,rt,pt0)
#define NIRVA_NEEDS_IMPL_FUNC0(fname,rt,x) NIRVA_NEEDS(fname,rt,)

#define NIRVA_NEEDS_IMPL_FUNC(fname, desc, ret_type, rt_desc, nparams, ...) \
    NIRVA_NEEDS_IMPL_FUNC##nparams(fname, ret_type, __VA_ARGS__)

#define NIRVA_DEF_OPT_FUNC(funcname, desc, ret_type, rt_desc, nparams, ...) \
  NIRVA_NEEDS_IMPL_FUNC##nparams(funcname, ret_type,__VA_ARGS__) = IMPL_FUNC_##funcname;

#define NIRVA_NEEDS_EXT_COND_FUNC(funcname, desc, cond, ret_type, rt_desc, nparams, ...) \
    NIRVA_NEEDS_IMPL_FUNC##nparams(funcname, ret_type,__VA_ARGS__);

#define NIRVA_MANDFUNC(nnn)NIRVA_CMD(_CALL(NIRVA_NEEDS_IMPL_FUNC,NIRVA_MAND_FUNC_##nnn))

///list of functions which the implementation must define////

// TODO - nirva_create_bundle : create an empty bundle (no strands)
#define NIRVA_MAND_FUNC_001 create_bundle_by_type,"create and return a bundle given a bundle_type and" \
    " 'item_name','value' pairs",NIRVA_BUNDLEPTR,new_bundle,2, NIRVA_UINT64, \
    bundle_type, NIRVA_VARIADIC,...

NIRVA_MANDFUNC(001)

#ifdef NIRVA_IMPL_FUNC_create_bundle_by_type
NIRVA_ADD_IMPL_FUNC(create_bundle_by_type)
#else
#define NEED_NIRVA_CREATE_BUNDLE_BY_TYPE
#endif

#define create_bundle_by_type _nirva_create_bundle_by_type

#define NIRVA_MAND_FUNC_002 create_bundle_from_bdef,"create and return a bundle given a bundledef" \
    "only needed during bootstrap",NIRVA_BUNDLEPTR,new_bundle, 1, NIRVA_STRING_ARRAY, bdef

NIRVA_MANDFUNC(002)

#ifdef NIRVA_IMPL_FUNC_create_bundle_from_bdef
NIRVA_ADD_IMPL_FUNC(create_bundle_from_bdef)
#else
#define NEED_NIRVA_CREATE_BUNDLE_FROM_BDEF
#endif

#define NIRVA_MAND_FUNC_003 array_append,"append items to an array strand. " \
    "The return value is the size of the array after the new items have been " \
    "appended, or in case of error, a negative value must be returned.",	\
    NIRVA_INT,new_size,5,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name, \
    NIRVA_UINT,strand_type,NIRVA_UINT,nitems,NIRVA_VARIADIC,...

NIRVA_MANDFUNC(003)

#ifdef NIRVA_IMPL_FUNC_array_append
NIRVA_ADD_IMPL_FUNC(array_append)
#else
#define NEED_NIRVA_ARRAY_APPEND
#endif

#define NIRVA_MAND_FUNC_004 array_clear,"remove all values from an array, resetting it to size 0, " \
    "Removing the strand itself is optional.",				\
    NIRVA_NO_RETURN,,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(004)

#ifdef NIRVA_IMPL_FUNC_array_clear
NIRVA_ADD_IMPL_FUNC(array_clear)
#else
#define NEED_NIRVA_ARRAY_CLEAR
#endif

#define NIRVA_MAND_FUNC_005 strand_delete,"remove a strand from a bundle, but do not free the data", \
    NIRVA_NO_RETURN,,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(005)

#ifdef NIRVA_IMPL_FUNC_strand_delete
NIRVA_ADD_IMPL_FUNC(strand_delete)
#else
#define NEED_NIRVA_STRAND_DELETE
#endif

#define NIRVA_MAND_FUNC_006 bundle_free,"free an empty bundle after all strands "	\
    "have been removed", NIRVA_NO_RETURN,,1,NIRVA_BUNDLEPTR,bundle

NIRVA_MANDFUNC(006)

#ifdef NIRVA_IMPL_FUNC_bundle_free
NIRVA_ADD_IMPL_FUNC(bundle_free)
#else
#define NEED_NIRVA_BUNDLE_FREE
#endif

#define NIRVA_MAND_FUNC_007 array_get_size,"return count of items in the data of a strand. " \
    "A return value of zero means either the array is empty or the strand does not exist", \
    NIRVA_UINT,array_size,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(007)

#ifdef NIRVA_IMPL_FUNC_array_get_size
NIRVA_ADD_IMPL_FUNC(array_get_size)
#else
#define NEED_NIRVA_ARRAY_GET_SIZE
#endif

#define NIRVA_MAND_FUNC_008 bundle_list_strands,"return a NULL terminated array of existing " \
    "strand names in the given bundle. The caller will free each of the strings and the array itself", \
    NIRVA_STRING_ARRAY,allocated_array_of_names,1,NIRVA_BUNDLEPTR,bundle

NIRVA_MANDFUNC(008)

#ifdef NIRVA_IMPL_FUNC_bundle_list_strands
NIRVA_ADD_IMPL_FUNC(bundle_list_strands)
#else
#define NEED_NIRVA_BUNDLE_LIST_STRANDS
#endif

#define NIRVA_MAND_FUNC_009 get_array_int,"get an int32 array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_INT),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(009)

#ifdef NIRVA_IMPL_FUNC_get_array_int
NIRVA_ADD_IMPL_FUNC(get_array_int)
#else
#define NEED_NIRVA_GET_ARRAY_INT
#endif

#define NIRVA_MAND_FUNC_010 get_array_boolean,"get a boolean array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_BOOLEAN),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(010)

#ifdef NIRVA_IMPL_FUNC_get_array_boolean
NIRVA_ADD_IMPL_FUNC(get_array_boolean)
#else
#define NEED_NIRVA_GET_ARRAY_BOOLEAN
#endif

#define NIRVA_MAND_FUNC_011 get_array_double,"get a double array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_DOUBLE),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(011)

#ifdef NIRVA_IMPL_FUNC_get_array_double
NIRVA_ADD_IMPL_FUNC(get_array_double)
#else
#define NEED_NIRVA_GET_ARRAY_DOUBLE
#endif

#define NIRVA_MAND_FUNC_012 get_array_string,"get a string array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_STRING),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(012)

#ifdef NIRVA_IMPL_FUNC_get_array_string
NIRVA_ADD_IMPL_FUNC(get_array_string)
#else
#define NEED_NIRVA_GET_ARRAY_STRING
#endif

#define NIRVA_MAND_FUNC_013 get_array_int64,"get an int64 array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_INT64),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(013)

#ifdef NIRVA_IMPL_FUNC_get_array_int64
NIRVA_ADD_IMPL_FUNC(get_array_int64)
#else
#define NEED_NIRVA_GET_ARRAY_INT64
#endif

#define NIRVA_MAND_FUNC_014 get_array_voidptr,"get a void * array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_VOIDPTR),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(014)

#ifdef NIRVA_IMPL_FUNC_get_array_voidptr
NIRVA_ADD_IMPL_FUNC(get_array_voidptr)
#else
#define NEED_NIRVA_GET_ARRAY_VOIDPTR
#endif

#define NIRVA_MAND_FUNC_015 get_array_funcptr,"get a function pointer array from data " \
    "of a strand", NIRVA_ARRAY_OF(NIRVA_NATIVE_FUNC),array,2,NIRVA_BUNDLEPTR, \
    bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(015)

#ifdef NIRVA_IMPL_FUNC_get_array_funcptr
NIRVA_ADD_IMPL_FUNC(get_array_funcptr)
#else
#define NEED_NIRVA_GET_ARRAY_FUNCPTR
#endif

#define NIRVA_MAND_FUNC_016 get_array_bundleptr,"get a bundleptr array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR),array,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_MANDFUNC(016)

#ifdef NIRVA_IMPL_FUNC_get_array_bundleptr
NIRVA_ADD_IMPL_FUNC(get_array_bundleptr)
#else
#define NEED_NIRVA_GET_ARRAY_BUNDLEPTR
#endif

#define NIRVA_PRFUNC_0		(__STD_INPS__,)	      	_CALL(MACRO,_STD_PARAMS__,__PARAMS_0__)
#define NIRVA_PRFUNC_1(__STD_INPS__,__PARAMS_IN_1__)  	_CALL(MACRO,_STD_PARAMS__,__PARAMS_1__)
#define NIRVA_PRFUNC_2(__STD_INPS__,__PARAMS_IN_2__)  	_CALL(MACRO,_STD_PARAMS__,__PARAMS_2__)
#define NIRVA_PRFUNC_3(__STD_INPS__,__PARAMS_IN_3__)   	_CALL(MACRO,_STD_PARAMS__,__PARAMS_3__)
#define NIRVA_PRFUNC_4(__STD_INPS__,__PARAMS_IN_4__)  	_CALL(MACRO,_STD_PARAMS__,__PARAMS_4__)
#define NIRVA_PRFUNC_5(__STD_INPS__,__PARAMS_IN_5__)  	_CALL(MACRO,_STD_PARAMS__,__PARAMS_5__)
#define NIRVA_PRFUNC_6(__STD_INPS__,__PARAMS_IN_6__)  	_CALL(MACRO,_STD_PARAMS__,__PARAMS_6__)
#define __PARMS_IN_4__	  	     p0,p1,p2,p3
#define __PARMS_IN_5__			p0,p1,p2,p3,p4
#define __PARMS_IN_6__			p0,p1,p2,p3,p4,p5
#define  __STD_INPS__	      	 MACRO,funcname,funcdesc,ret_type
#define __STD_PARAMS__	  	 #funcname,funcdesc,  #ret_type
#define __PARAMS_6__	       	     6,#p0,#p1,#p2,#p3,#p4,#p5
#define __PARAMS_5__			5,#p0,#p1,#p2,#p3,#p4/*~~~*/
#define __PARAMS_4__		  4,#p0,#p1,#p2,#p3
#define __PARAMS_3__		3,#p0,#p1,#p2
#define __PARAMS_2__		2,#p0,#p1
#define __PARMS_IN_3__	       	p0,p1,p2
#define __PARMS_IN_2__	       	p0,p1
#define __PARAMS_1__		1,#p0
#define __PARMS_IN_1_		p0
#define __PARAMS_0__		0
#define NIRVA_PRFUNC(fname, fdesc, rt, np, ...)			\
  NIRVA_PRFUNC_##np(funcname, funcdesc, rt, np, __VA_ARGS__)

#define _MAKE_IMPL_FUNC_DESC(n)						\
  (n == 1 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_001) :	n == 2 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_002) : \
   n == 3 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_004) : \
   n == 5 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_005) : n == 6 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_006) : \
   n == 7 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_007) : n == 8 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_008) : \
   n == 9 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_009) : n == 10 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_010) : \
   n == 11 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_011) : n == 12 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_012) : \
   n == 13 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_013) : n == 14 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_014) : \
   n == 15 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_015) : n == 16 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_016) : \
   NULL)

static void add_func_desc(nirva_native_function_t func, const char *fname, const char *fdesc,
                          const char *p0, const char *p1) {
}

#define NIRVA_FSTRING(nnn, m)_CALL(PRFUNC_##m,NIRVA_MAND_FUNC_##nnn))

#define NIRVA_OPT_IMPL_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type impl_func_##funcname(__VA_ARGS__);

#define NIRVA_REC_IMPL_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type impl_func_##funcname(__VA_ARGS__);

#define NIRVA_REC_FUNC(nnn) NIRVA_CMD(_CALL(NIRVA_NEEDS_EXT_COND_FUNC, NIRVA_REC_FUNC_##nnn))
#define NIRVA_ADD_OPT_FUNC(nnn) NIRVA_CMD(_CALL(NIRVA_NEEDS_IMPL_FUNC, NIRVA_OPT_FUNC_##nnn))

// opt / conditional core funcs - COND defines the conditions that make it mandatory

// if built according to the suggestions, each bundle will have either a copy
// of its blueprint (a strand_def_bundle defining it)
// if such can be returned, then it removes the need to implement several other functions
// listed below. The blueprint will not be altered,

// changes after nirva_prepare() require PRIV_STRUCT > 50
// adding after nirva_prepare() requires PRIV_STRUCT > 30

#define NIRVA_REC_FUNC_001 bundle_has_strand,"return TRUE (1) if strand_name currently " \
    "exists in bundle, otherewise 0.  If not defined, the strands will be listed and checked one by one",_COND_NEVER, \
    NIRVA_BOOLEAN,exists,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_REC_FUNC(001)

#ifdef NIRVA_IMPL_FUNC_bundle_has_strand
NIRVA_ADD_IMPL_FUNC(bundle_has_strand)
#else
#define NEED_NIRVA_BUNDLE_HAS_STRAND
#endif

#define NIRVA_REC_FUNC_002 strand_copy,"copy by value, data from one strand to another of " \
    "the same type, Returns a value >=1 on success",_COND_NEVER,NIRVA_FUNC_RETURN,,4, \
    NIRVA_BUNDLEPTR,dest_bundle,NIRVA_CONST_STRING,dest_strand_name,NIRVA_CONST_BUNDLEPTR, \
    src_bundle,NIRVA_CONST_STRING,src_strand_name

NIRVA_REC_FUNC(002)

#ifdef NIRVA_IMPL_FUNC_strand_copy
NIRVA_ADD_IMPL_FUNC(strand_copy)
#else
#define NEED_NIRVA_STRAND_COPY
#endif

#define NIRVA_REC_FUNC_003 get_array_uint,"get a uint32 array from data of a strand." \
      "If not defined, int32 will be used instead and cast to uint",_COND_NEVER, \
      NIRVA_ARRAY_OF(NIRVA_UINT),data,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_REC_FUNC(003)

#ifdef NIRVA_IMPL_FUNC_get_array_uint
NIRVA_ADD_IMPL_FUNC(get_array_uint)
#else
#define NEED_NIRVA_GET_ARRAY_UINT
#endif

#define NIRVA_REC_FUNC_004 get_array_uint64,"get a uint64 array from data of a strand." \
    "If not defined, int64 will be used instead and cast to uint64",_COND_NEVER, \
    NIRVA_ARRAY_OF(NIRVA_UINT64),data,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

NIRVA_REC_FUNC(004)

#ifdef NIRVA_IMPL_FUNC_get_array_uint64
NIRVA_ADD_IMPL_FUNC(get_array_uint64)
#else
#define NEED_NIRVA_GET_ARRAY_UINT64
#endif

#define _MAKE_IMPL_REC_FUNC_DESC(n)					\
  (n == 1 ? NIRVA_PRFUNC(NIRVA_REC_FUNC_001) : n == 2 ? NIRVA_PRFUNC(NIRVA_REC_FUNC_002) : \
   n == 3 ? NIRVA_PRFUNC(NIRVA_REC_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_REC_FUNC_004) : \
   NIRVA_NULL)

/////// optional overrides ///////
// optional impl_functions are those which use IMPL_FUNCs but are not fundamental
// these can be changed at any point even at runtime.
// changes after nirva_prepare() require PRIV_STRUCT > 20

#define NIRVA_OPT_FUNC_001 nirva_action, "run a static transform via a function wrapper", \
    NIRVA_FUNC_RETURN,response,2,NIRVA_CONST_STRING,transform_name, NIRVA_VARIADIC,...

#ifdef NIRVA_IMPL_FUNC_action_ret
NIRVA_ADD_IMPL_FUNC(action_ret)
#else
#define NEED_NIRVA_ACTION_RET
#endif

NIRVA_ADD_OPT_FUNC(001)

#define NIRVA_OPT_FUNC_002 atoi,"convert a string value to int",NIRVA_INT,int_value,1, \
    NIRVA_CONST_STRING,strval

#ifndef IMPL_FUNC_atoi
#if NIRVA_IMPL_IS(DEFAULT_C)
#define IMPL_FUNC_atoi atoi
#endif
#endif

NIRVA_ADD_OPT_FUNC(002)

#define NIRVA_OPT_FUNC_003 atol, "convert a string value to int64",	\
    NIRVA_INT64,int64_value,1,NIRVA_CONST_STRING,strval

#ifndef IMPL_FUNC_atol
#if NIRVA_IMPL_IS(DEFAULT_C)
#define IMPL_FUNC_atol atol
#endif
#endif

NIRVA_ADD_OPT_FUNC(003)

#define NIRVA_OPT_FUNC_004 nirva_wait_retry, "sleep for a short time, e.g 1 usec, 1 msec", \
    NIRVA_NO_RETURN,,0

#ifndef IMPL_FUNC_nirva_wait_retry
#define IMPL_FUNC_nirva_wait_retry _def_nirva_wait_retry
#define NEED_WAIT_RETRY
#endif

#define NIRVA_WAIT_RETRY(...)NIRVA_CALL(nirva_wait_retry, __VA_ARGS__)

NIRVA_ADD_OPT_FUNC(004)

#define NIRVA_OPT_FUNC_005 recycle, "recycle and free resources used by "	\
    "a no longer required bundle.", NIRVA_UINT,attr_type,1,NIRVA_BUNDLEPTR,bun

NIRVA_ADD_OPT_FUNC(005)

#ifdef NIRVA_IMPL_FUNC_recycle
NIRVA_ADD_IMPL_FUNC(recycle)
#else
#define NEED_NIRVA_RECYCLE
#endif

#define NIRVA_OPT_FUNC_006 add_value_by_key,"append an item to a keyed_array, using keyval as an index to locate it. " \
      "if defined, then the array get functions should have an extra parameter, keyval" \
      "and the fucntions nirva_remove_value_by_key, nirva_get_value_by_key, and nirva_has_value_for_key must be defined", \
      NIRVA_FUNC_RETURN,retval,5,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,stname,NIRVA_CONST_STRING, keyval, NIRVA_UINT, strand_type, \
      NIRVA_VARIADIC,...

NIRVA_ADD_OPT_FUNC(006)

#define NIRVA_OPT_FUNC_007 remove_value_by_key,"remove an item from a keyed_array, " \
    "using keyval as an index to locate it. ",				\
    NIRVA_FUNC_RETURN,retval,3,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,stname,NIRVA_CONST_STRING,keyval

NIRVA_ADD_OPT_FUNC(007)

#define NIRVA_OPT_FUNC_008 get_value_by_key,"return a bundleptr, using the keyval as reference", \
      NIRVA_BUNDLEPTR,found_value,3,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING, stname, NIRVA_CONST_STRING, keyval

NIRVA_ADD_OPT_FUNC(008)

#define NIRVA_OPT_FUNC_009 has_value_for_key,"check if a keyed_array contains an item with the specified keyval",	\
    NIRVA_BOOLEAN,found_value,3,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING, stname, NIRVA_CONST_STRING, keyval

NIRVA_ADD_OPT_FUNC(009)

#if defined(NIRVA_IMPL_FUNC_get_value_by_key) && defined(NIRVA_IMPL_FUNC_add_value_by_key)  \
  && defined(NIRVA_IMPL_FUNC_remove_value_by_key) && defined(NIRVA_IMPL_FUNC_has_value_for_key)
NIRVA_ADD_IMPL_FUNC(get_value_by_key) NIRVA_ADD_IMPL_FUNC(add_value_by_key)
NIRVA_ADD_IMPL_FUNC(remove_value_by_key) NIRVA_ADD_IMPL_FUNC(has_value_for_key)
#else

#ifdef NIRVA_IMPL_FUNC_get_value_by_key
#undef NIRVA_IMPL_FUNC_get_value_by_key
#endif
#ifdef NIRVA_IMPL_FUNC_add_value_by_key
#undef NIRVA_IMPL_FUNC_add_value_by_key
#endif
#ifdef NIRVA_IMPL_FUNC_remove_value_by_key
#undef NIRVA_IMPL_FUNC_remove_value_by_key
#endif
#ifdef NIRVA_IMPL_FUNC_has_value_for_key
#undef NIRVA_IMPL_FUNC_has_value_for_key
#endif

#define NEED_NIRVA_KEYED_ARRAYS
#endif

#define _MAKE_IMPL_OPT_FUNC_DESC(n)					\
  (n == 1 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_001) : n == 2 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_002) : \
   n == 3 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_004) : \
   n == 5 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_005) : n == 6 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_006) : \
   n == 7 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_007) : n == 8 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_008) : \
   n == 9 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_009) : NIRVA_NULL)

/////////////
NIRVA_EXTERN NIRVA_BUNDLEPTR STRUCTURE_PRIME;
NIRVA_EXTERN NIRVA_BUNDLEPTR STRUCTURE_APP;
NIRVA_CMD(NIRVA_STATIC NIRVA_BUNDLEPTR _TMP_STORAGE_BUNDLE)
/////////////

#ifdef HAVE_MAKE_STRANDS_FUNC
#define _NIRVA_DEPLOY_MAKE_STRANDS_FUNC
#else
#define HAVE_MAKE_STRANDS_FUNC
#define _NIRVA_DEPLOY_MAKE_STRANDS_FUNC static char *make_strand(const char *fmt,va_list ap){ \
      va_list vc;							\
      char *st;size_t strsz;						\
      va_copy(vc, ap);							\
      strsz=vsnprintf(0,0,fmt,vc);					\
      va_end(vc);							\
      st=malloc(++strsz);vsnprintf(st,strsz,fmt,ap);return st;}		\
    NIRVA_CONST_STRING_ARRAY make_strands(NIRVA_CONST_STRING fmt,...){			\
      p_debug("\nGenerating bundle definition\n");			\
      const char**sts=0,*xfmt,*str;int ns=0;va_list ap;va_start(ap,fmt); \
      while(1) {if (*fmt) {if (!(xfmt=va_arg(ap,char*))) break; vsnprintf(0,0,xfmt,ap);} \
	else {if (!(str = va_arg(ap, char *))) break; p_debug("got %s\n", str);}  ns++;} va_end(ap); \
      p_debug("\nCounted %d atoms\n", ns);				\
      sts=malloc((ns+1)*sizeof(char*));va_start(ap,fmt);		\
      if (*fmt) xfmt = fmt;						\
      else xfmt = "%s";							\
      p_debug("\nAllocated %d atoms\n", ns+1);				\
      for(int i = 0; i < ns; i++) {					\
	p_debug("\nproceesing %d of %d\n", i, ns);			\
	if (i > 0) xfmt=(*fmt?va_arg(ap,char*):"%s");			\
	sts[i]=make_strand(xfmt,ap);					\
	p_debug("\nproceesed %d of %d\n", i, ns);			\
      }									\
      va_end(ap);sts[ns]=0;return sts;}
#endif

// this function should be called with a bundle name, followed by a sequence of char **
// each char ** array must be NULL terminated
// the final parameter must be NULL
// the result will be a const char ** which joins all the other arrays into 1, ignoring te final NULLS
// if name is not NULL, a comment #name will be prepended
// and in every case, a terminating NULL string will be appended to the final array
// and the amalgamated array returned
// thus it is possible to call, for example:
// 	const char **bdef = make_bundledef("main bundledef", make_strands(a, b, NULL),
//	 make_bundledef("sub bundledef", make_strands(c, d, NULL), NULL), NULL);
// this would return an array {"#main bundledef", a, b, "#sub bundledef", c, d, NULL}
// in this manner it is possible to nest bundldefs (macro EXTEND_BUNDLE works like this)
// In the latest version the second param can be NULL or a prefix
#ifdef HAVE_MAKE_BUNDLEDEF_FUNC
#define _NIRVA_DEPLOY_MAKE_BUNDLEDEF_FUNC
#else
#define HAVE_MAKE_BUNDLEDEF_FUNC
#define _NIRVA_DEPLOY_MAKE_BUNDLEDEF_FUNC				\
  const char **make_bundledef(const char*n,const char*pfx,...){		\
    char**sT=0,*str;int nsT=0;va_list ap;if(n&&*n){size_t Cln=strlen(n)+1; \
      if(n){p_debug("\nGenerating bundle definition for %s\n",n);str=malloc(++Cln); \
	snprintf(str,Cln,"#%s",n);sT=(char**)MS("",str,0);nsT++;}	\
      else str=malloc(Cln);va_start(ap,pfx);while(1){int nC=0;char**newsT=va_arg(ap,char**); \
	if(!newsT)break;while(newsT[++nC]);sT=realloc(sT,(nsT+nC+1)*sizeof(char*)); \
	for(nC=0;newsT[nC];nC++){size_t strsz=snprintf(0,0,"%s%s",pfx?pfx:"",newsT[nC]); \
	  p_debug("Adding strand:\t%s\n",newsT[nC]);if(strsz){sT[nsT]=malloc(++strsz); \
	    snprintf(sT[nsT++],strsz,"%s%s",pfx?pfx:"",newsT[nC]);}}}va_end(ap); \
      p_debug("Bundledef complete\n\n");sT[nsT]=0;return(const char**)sT;}return 0;}
#endif

#define COND_NARGS_VARIABLE _1
#define COND_NARGS_INVALID -999

#define FMT_COND_KWORD(cond)NIRVA_FMT_STRING,#cond
#define FMT_COND_LOGIC(cond)NIRVA_FMT_INT,cond
#define FMT_COND_OP(cond)NIRVA_FMT_INT,cond
#define FMT_COND_VAL(cond)NIRVA_FMT_INT,cond

// all values are stored as string, e.g. int -> "%d" ->
#define COND_START		"COND_START"
#define _COND_START		FMT_COND_KWORD(COND_START)

#define COND_END		"COND_END"
#define _COND_END		FMT_COND_KWORD(COND_END)

#define COND_NOT		"COND_NOT"
#define _COND_NOT		FMT_COND_KWORD(COND_NOT)

#define COND_LIST_END		"COND_LIST_END"
#define _COND_LIST_END		FMT_COND_KWORD(COND_LIST_END)

#define COND_P_OPEN		"("
#define _COND_P_OPEN		FMT_COND_KWORD(()

#define COND_P_CLOSE		"}"
#define _COND_P_CLOSE		FMT_COND_KWORD(})

/// condfuncs //
#define COND_NOOP					0
// two args
#define COND_LOGIC_NOT					1
#define _COND_LOGIC_NOT					_CALL(FMT_COND_LOGIC,COND_LOGIC_NOT)

#define COND_LOGIC_AND					2
#define _COND_LOGIC_AND					_CALL(FMT_COND_LOGIC,COND_LOGIC_AND)

#define COND_LOGIC_OR					3
#define _COND_LOGIC_OR					_CALL(FMT_COND_LOGIC,COND_LOGIC_OR)

#define COND_LOGIC_XOR					4
#define _COND_LOGIC_XOR					_CALL(FMT_COND_LOGIC,COND_LOGIC_XOR)
//
// operations - for RANKING
#define COND_OP_ADD					8
#define _COND_OP_ADD					_CALL(FMT_COND_OP,COND_OP_ADD)
// value operations
#define COND_OP_SUBTRACT				9
#define _COND_OP_SUBTRACT				_CALL(FMT_COND_OP,COND_OP_SUBTRACT)
// value operations
#define COND_OP_MULTIPLY				10
#define _COND_OP_MULTIPLY				_CALL(FMT_COND_OP,COND_OP_MULTIPLY)
// value operations
#define COND_OP_CLAMP					11
#define _COND_OP_CLAMP					_CALL(FMT_COND_OP,COND_OP_CLAMP)
// value operations
#define COND_OP_ABS					12
#define _COND_OP_ABS					_CALL(FMT_COND_OP,COND_OP_ABS)
// value operations
// value
// if n is 0, push and pop (LIFO)
// 1 arg (n)  s[n] = next op
// with two values (-n, uid) will store as static
#define COND_OP_STORE	      				13 // s[n] = val //
#define _COND_OP_STORE					_CALL(FMT_COND_OP,COND_OP_STORE)
#define COND_OP_FETCH       				14 // val = s[n] //
#define _COND_OP_FETCH					_CALL(FMT_COND_OP,COND_OP_FETCH)

#define COND_LOGIC_LAST					14

#define COND_TRUE				    	NIRVA_TRUE
#define _COND_TRUE				    	_CALL(FMT_COND_VAL,COND_TRUE)
#define COND_FALSE				    	NIRVA_FALSE
#define _COND_FALSE				    	_CALL(FMT_COND_VAL,COND_FALSE)

// note leading _
// zero args
#define COND_VAL_FIRST					15

#define COND_VAL_TRUE					15
#define _COND_VAL_TRUE				    	_CALL(FMT_COND_VAL,COND_VAL_TRUE)
#define COND_15_NARGS					0
#define COND_15_TEST(...)			    	1
#define COND_VAL_FALSE					16
#define _COND_VAL_FALSE				    	_CALL(FMT_COND_VAL,COND_VAL_FALSE)
#define COND_16_NARGS					0
#define COND_16_TEST(...)			    	0
//
#define COND_VAL_EVAL					17
#define _COND_VAL_EVAL				    	_CALL(FMT_COND_VAL,COND_VAL_EVAL)
#define COND_17_TEST(a,...)			    	(!!(a))
#define COND_17_NARGS					1
#define COND_TEST_EVAL(a)				COND_TEST(VAL_EVAL,a)
//
#define COND_VAL_EQUALS					18
#define _COND_VAL_EQUALS			    	_CALL(FMT_COND_VAL,COND_VAL_EQUALS)
#define COND_18_NARGS					2
#define COND_18_TEST(a,b,...)			    	((a)==(b))
#define COND_TEST_EQUALS(a,b)				COND_TEST(VAL_EQUALS,a,b)
#define COND_VAL_GT					19
#define _COND_VAL_GT				    	_CALL(FMT_COND_VAL,COND_VAL_GT)
#define COND_19_NARGS					2
#define COND_19_TEST(a,b,...) 	   			((a)>(b))
#define COND_TEST_GT(a,b)	       			COND_TEST(VAL_GT,a,b)
#define COND_VAL_LT					20
#define _COND_VAL_LT				    	_CALL(FMT_COND_VAL,COND_VAL_LT)
#define COND_20_NARGS					2
#define COND_20_TEST(a,b,...)  	 			((a)<(b))

#define COND_TEST_LT(a,b)	       			COND_TEST(VAL_LT,a,b)
#define COND_VAL_GTE					21
#define _COND_VAL_GTE				    	_CALL(FMT_COND_VAL,COND_VAL_GTE)
#define COND_21_NARGS					2
#define COND_21_TEST(a,b)		    		((a)>=(b))
#define COND_TEST_GTE(a,b)	       			COND_TEST(VAL_GTE,a,b)
#define COND_VAL_LTE					22
#define _COND_VAL_LTE				    	_CALL(FMT_COND_VAL,COND_VAL_LTE)
#define COND_22_NARGS					2
#define COND_22_TEST(a,b) 	   			((a)<=(b))
#define COND_TEST_LTE(a,b)	       			COND_TEST(VAL_LTE,a,b)
//
#define COND_STR_MATCH					23
#define _COND_STR_MATCH				    	_CALL(FMT_COND_VAL,COND_STR_MATCH)
#define COND_23_NARGS					2
#define COND_23_TEST(a,b)      			    	__IMPL_TEST_STRING_EQUAL__(#a,#b)
#define COND_TEST_STR_MATCH(a,b)			COND_TEST(STR_MATCH,a,b)
#define COND_BIT_ON					24
#define _COND_VAL_BIT_ON			    	_CALL(FMT_COND_VAL,COND_VAL_BIT_ON)
#define COND_24_NARGS					2
#define COND_24_TEST(v,b)      				(((v)&(b))==(b))
#define COND_TEST_BIT_ON(v,b)				COND_TEST(BIT_ON,v,b)
#define COND_BIT_OFF					25
#define _COND_VAL_BIT_OFF			    	_CALL(FMT_COND_VAL,COND_VAL_BIT_OFF)
#define COND_25_NARGS					2
#define COND_25_TEST(v,b)      				(!((v)&(b)))
#define COND_TEST_BIT_OFF(v,b)				COND_TEST(BIT_OFF,v,b)
#define COND_PROB_PERCENT			       	26
#define _COND_VAL_PROB_PERCENT			    	_CALL(FMT_COND_VAL,COND_VAL_PROB_PERCENT)
#define COND_26_NARGS					2
#define COND_26_TEST(p,s)	       			((p)<=NIRVA_RAND_BAD(s))
#define COND_TEST_PROB_PERCENT(p,a)			COND_TEST(PROB_PERCENT,p,a)
//
// -1 args
#define COND_VAL_IN					27
#define _COND_VAL_VAL_IN			    	_CALL(FMT_COND_VAL,COND_VAL_IN)
#define COND_27_NARGS					COND_NARGS_VARIABLE
#define COND_27_TEST(v,...)	       			nirva_cond_in_set(p, __VA_ARGS__)
#define COND_TEST_VAL_IN(v,...)				COND_TEST(VAL_IN,__VA_ARGS__)

/* #define COND_HAS_STRAND					32 */
/* #define COND_32_TEST(b,n)	       			bundle_has_strand(b,n) */

/* #define COND_HAS_ATTR					33 */
/* #define COND_33_TEST(g,a)	       			attr_group_has_attr(b,n) */

#define COND_VAL_LAST					27

#define _COND_TEST(opnum,...) COND_##opnum##_TEST(__VA_ARGS__)
#define COND_TEST(op,...) _CALL(_COND_TEST,COND_##op,__VA_ARGS__)

#define NIRVA_COND_RESPONSE(r)((r)?NIRVA_COND_SUCCESS:NIRVA_COND_FAIL)

#define NIRVA_COND_SUCCEEDED(r)((r)==NIRVA_COND_SUCCESS)
#define NIRVA_COND_FAILED(r)((r)==NIRVA_COND_FAIL)

#define NIRVA_COND_CHECK_SUCCEEDED(r)(NIRVA_COND_SUCCEEDED(NIRVA_COND_RESPONSE(r)))
#define NIRVA_COND_CHECK_FAILED(r)(NIRVA_COND_FAILED(NIRVA_COND_RESPONSE(r)))

#define NIRVA_RAND_BAD1(a)(((a) + .83147) * .727953)
#define NIRVA_RAND_BAD2(a)NIRVA_RAND_BAD1(a) > 1.3846 ? NIRVA_RAND_BAD1(a) - 1.1836 : \
    NIRVA_RAND_BAD1(a)
#define NIRVA_RAND_BAD3(a)NIRVA_RAND_BAD1(a) > 1. ? NIRVA_RAND_BAD1(a) - 1. : \
    NIRVA_RAND_BAD1(a)
#define NIRVA_RAND_BAD(a)(NIRVA_RAND_BAD3(NIRVA_RAND_BAD2(NIRVA_RAND_BAD2(NIRVA_RAND_BAD2(a)))))

#define COND_ALWAYS		COND_START, COND_VAL_TRUE, COND_END
#define COND_NEVER		COND_START, COND_VAL_FALSE, COND_END

#define _COND_ALWAYS		_COND_START, "%d", COND_TRUE, _COND_END
#define _COND_NEVER		_COND_START, "%d", COND_FALSE, _COND_END

#define COND_ONCE		"COND_ONCE"
#define _COND_ONCE		FMT_CONDCHECK(COND_ONCE)
#define COND_CHECK		"COND_CHECK"
#define _COND_CHECK		FMT_CONDCHECK(COND_CHECK)
#define COND_CHECK_RETURN	"COND_CHECK_RETURN"
#define _COND_CHECK_RETURN	FMT_CONDCHECK(COND_CHECK_RETURN)

// condition automation //
#define NIRVA_COND_TEST_1(cnum,...)COND_##cnum##_TEST(__VA_ARGS__)
#define NIRVA_COND_TEST2(cnum,p0,p1,dummy)COND_##cnum##_TEST(p0,p1)
#define NIRVA_COND_TEST1(cnum,p0,dummy)COND_##cnum##_TEST(p0)
#define NIRVA_COND_TEST0(cnum,dummy)COND_##cnum##_VAL

#define TEST_CONDCHECK(cnum,nargs,...)NIRVA_COND_TEST##nargs(cnum,__VA_ARGS__)

#define N_OP_ARGS(cnum)(cnum==15?COND_15_NARGS:cnum==16?COND_16_NARGS: \
			cnum==17?COND_17_NARGS:cnum==18?COND_18_NARGS:	\
			cnum==19?COND_19_NARGS:cnum==20?COND_20_NARGS:-999)

#define NIRVA_TEST_COND(op,...) \
    NIRVA_INLINE(if (op == 15 ? COND_15_TEST(__VA_ARGS__) : op == 16 ? COND_16_TEST(__VA_ARGS__) : op == 17 ? COND_17_TEST(__VA_ARGS__) : \
		     op == 18 ? COND_18_TEST(__VA_ARGS__) : op == 19 ? COND_19_TEST(__VA_ARGS__) : op == 20 ? COND_20_TEST(__VA_ARGS__)	\
		     : 1) NIRVA_RETURN(NIRVA_COND_SUCCESS))

#define _CALLER_UID "!CALLER_UID"
#define _TARGET_ITEM "!TARGET_ITEM"
#define _SOURCE_ITEM "!SOURCE_ITEM"
#define _TARGET_BUNDLE(strand) "!TARGET_BUNDLE.%s", #strand
#define _SOURCE_BUNDLE(strand) "!SOURCE_BUNDLE.%s", #strand

#define _TARGET_BUNDLE_UID "!TARGET_BUNDLE.UID"
#define _TARGET_BUNDLE_BUNDLE_TYPE "!TARGET_BUNDLE.BUNDLE_TYPE"

// if target bundle is an object, then the following are valid
#define _TARGET_OBJECT_UID _TARGET_BUNDLE_UID
#define _TARGET_OBJECT_TYPE "!TARGET_OBJECT.TYPE"
#define _TARGET_OBJECT_STATE "!TARGET_OBJECT.STATE"
#define _TARGET_INSTANCE_SUBTYPE "!TARGET_INSTANCE.SUBTYPE"

#define _SOURCE_BUNDLE_UID "!SOURCE_BUNDLE.UID"
#define _SOURCE_BUNDLE_BUNDLE_TYPE "!SOURCE_BUNDLE.BUNDLE_TYPE"

#define _SOURCE_OBJECT_UID  _SOURCE_BUNDLE_UID
#define _SOURCE_OBJECT_TYPE "!SOURCE_OBJECT.TYPE"
#define _SOURCE_OBJECT_STATE "!SOURCE_OBJECT.STATE"
#define _SOURCE_INSTANCE_SUBTYPE "!SOURCE_INSTANCE.SUBTYPE"

#define _SOURCE_OBJECTL "SOURCE_OBJECT"
#define _TARGET_OBJECT "TARGET_OBJECT: "

#define _SOURCE_STRAND_VAL _SOURCE_VAL"%s",
#define _TARGET_STRAND_VAL _TARGET_VAL"%s",

#define _SOURCE_BUNDLE_TYPE _SOURCE_VAL"BUNDLE_TYPE",
#define _TARGET_BUNDLE_TYPE _TARGET_VAL"BUNDLE_TYPE",

#define _VAR_ARRAY_SIZE "ARRAY_SIZE: %s"

#define _VAR_SYMBOLIC_VAL "SYMBOLIC: %s"

#define _CONST_STRING_VAL "CONST_STRING: %s"
#define _CONST_BOOL_VAL "CONST_BOOL: %d"
#define _CONST_INT_VAL "CONST_INT: %d"
#define _CONST_UINT_VAL "CONST_UINT: %u"
#define _CONST_INT64_VAL "CONST_INT64: %"PRId64
#define _CONST_UINT64_VAL "CONST_UINT64: %"PRIu64
#define _CONST_DOUBLE_VAL "CONST_DOUBLE: %f"
#define _CONST_FLOAT_VAL "CONST_FLOAT: %f"
#define _CONST_VOIDPTR_VAL "CONST_VOIDPTR: %p"
#define _CONST_GENPTR_VAL CONST_VOIDPTR_VAL
#define _CONST_BUNDLEPTR_VAL "CONST_BUNDLEPTR: %p"

#define VAR_STRAND_VAL "STRAND_VAL: %s"
#define VAR_ATTR_VAL "ATTR_VAL: %s",

#define VAR_STRAND_TYPE "STRAND_TYPE: %s"
#define VAR_ATTR_TYPE "ATTR_TYPE: %s"
#define VAR_ATTR_STRAND_TYPE "ATTR_STRAND_TYPE: %s"

#define ATTR_HAS_VALUE "ATTR_HAS_VAL: %s"
#define ATTR_EXISTS "ATTR_EXISTS: %s"

// attr_has_mapping, attr_name, mappping
#define ATTR_HAS_MAPPING "ATTR_HAS_MAPPING: %s %u"
#define ATTR_HAS_NOT_MAPPING "ATTR_HAS_NOT_MAPPING: %s %u"
#define ATTR_MAPPING_IS "ATTR_MAPPING_IS: %s %u"

#define HAS_CAP "HAS_CAP: %s"
#define HAS_NOT_CAP "HAS_NOT_CAP: %s"

// eg _STRAND_ALLOWS, stname, CONS_BUNDLEPTR_VAL, some_bundleptr, ...
#define STRAND_ALLOWS "STRAND_ALLOWS: %s"

#define NIRVA_ERROR(...) abort();
#define NIRVA_FAIL(...) abort();
#define NIRVA_FATAL(...) abort();

///////////////
////////////// mini API ///


// core MACROS
/// fallbacks for optional overrides

#ifdef NEED_NIRVA_GET_ARRAY_UINT
#define NIRVA_MACRO_get_array_uint(bundle,strand)			\
  NIRVA_CAST_TO_PTR(NIRVA_UINT,_NIRVA_RESULT_DEF(get_array_int,bundle,strand))
NIRVA_EXPORTS_NOFUNC(get_array_uint)
#endif

#ifdef NIRVA_MACRO_get_array_uint
#define _NIRVA_MACRO_get_array_uint(...)NIRVA_MACRO_get_array_uint(__VA_ARGS__)
#else
#define _NIRVA_MACRO_get_array_uint(...)NIRVA_NULL
#endif

#ifdef NEED_NIRVA_GET_ARRAY_UINT64
#define NIRVA_MACRO_get_array_uint64(bundle,strand)			\
  NIRVA_CAST_TO_PTR(NIRVA_UINT64,_NIRVA_RESULT_DEF(get_array_int64,bundle,strand))
NIRVA_EXPORTS_NOFUNC(get_array_uint64)
#endif

#ifdef NIRVA_MACRO_get_array_uint64
#define _NIRVA_MACRO_get_array_uint64 NIRVA_MACRO_get_array_uint64
#else
#define _NIRVA_MACRO_get_array_uint64(...)NIRVA_NULL
#endif

// functions with no builtin replacements

#ifdef NIRVA_MACRO_get_array_int
#define _NIRVA_MACRO_get_array_int NIRVA_MACRO_get_array_int
#else
#define _NIRVA_MACRO_get_array_int(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_boolean
#define _NIRVA_MACRO_get_array_boolean NIRVA_MACRO_get_array_boolean
#else
#define _NIRVA_MACRO_get_array_boolean(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_int64
#define _NIRVA_MACRO_get_array_int64 NIRVA_MACRO_get_array_int64
#else
#define _NIRVA_MACRO_get_array_int64(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_double
#define _NIRVA_MACRO_get_array_double NIRVA_MACRO_get_array_double
#else
#define _NIRVA_MACRO_get_array_double(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_string
#define _NIRVA_MACRO_get_array_string NIRVA_MACRO_get_array_string
#else
#define _NIRVA_MACRO_get_array_string(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_voidptr
#define _NIRVA_MACRO_get_array_voidptr NIRVA_MACRO_get_array_voidptr
#else
#define _NIRVA_MACRO_get_array_voidptr(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_funcptr
#define _NIRVA_MACRO_get_array_funcptr NIRVA_MACRO_get_array_funcptr
#else
#define _NIRVA_MACRO_get_array_funcptr(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_get_array_bundleptr
#define _NIRVA_MACRO_get_array_bundleptr NIRVA_MACRO_get_array_bundleptr
#else
#define _NIRVA_MACRO_get_array_bundleptr(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_bundle_list_strands
#define _NIRVA_MACRO_bundle_list_strands NIRVA_MACRO_bundle_list_strands
#else
#define _NIRVA_MACRO_bundle_list_strands(...)NIRVA_NULL
#endif

#ifdef NIRVA_MACRO_array_get_size
#define _NIRVA_MACRO_array_get_size NIRVA_MACRO_array_get_size
#else
#define _NIRVA_MACRO_array_get_size(...)0
#endif

#ifdef NIRVA_MACRO_bundle_free
#define _NIRVA_MACRO_bundle_free NIRVA_MACRO_bundle_free
#else
#define _NIRVA_MACRO_bundle_free(...)0
#endif

#ifdef NIRVA_MACRO_array_append
#define _NIRVA_MACRO_array_append NIRVA_MACRO_array_append
#else
#define _NIRVA_MACRO_array_append(...)NIRVA_RESULT_CALL_INVALID
#endif

#ifdef NIRVA_MACRO_array_clear
#define _NIRVA_MACRO_array_clear NIRVA_MACRO_array_clear
#else
#define _NIRVA_MACRO_array_clear(...)NIRVA_RESULT_CALL_INVALID
#endif

#ifdef NIRVA_MACRO_strand_delete
#define _NIRVA_MACRO_strand_delete NIRVA_MACRO_strand_delete
#else
#define _NIRVA_MACRO_strand_delete(...)NIRVA_RESULT_CALL_INVALID
#endif

#ifdef NEED_NIRVA_BUNDLE_HAS_STRAND
NIRVA_DEF_FUNC(NIRVA_BOOLEAN, NIRVA_EXPORTED_bundle_has_strand, NIRVA_BUNDLEPTR bun, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, allnames)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_ASSERT(stname, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_ASSIGN(allnames, NIRVA_BUNDLE_LIST_STRANDS(bun))
for (i = 0; allnames[i]; i++) {
  if (NIRVA_STRING_EQUAL(stname, allnames[i])) {
    while (allnames[i]) NIRVA_STRING_FREE(allnames[i++]);
    NIRVA_ARRAY_FREE(allnames)
    NIRVA_RETURN(NIRVA_TRUE)
  }
  NIRVA_STRING_FREE(allnames[i])
}
if (allnames) NIRVA_ARRAY_FREE(allnames);
NIRVA_END_RETURN(NIRVA_FALSE)
NIRVA_EXPORTS(bundle_has_strand)
#endif

#ifdef NIRVA_MACRO_bundle_has_strand
#define _NIRVA_MACRO_bundle_has_strand NIRVA_MACRO_bundle_has_strand
#else
#define _NIRVA_MACRO_bundle_has_strand(...)NIRVA_FALSE
#endif

// make sure we have at least 1 value set in stname
#define _NIRVA_CHECK_VALUES(bun, stname)				\
    (bun?NIRVA_BUNDLE_HAS_STRAND(bun,stname)?NIRVA_ARRAY_GET_SIZE(bun,stname)>0?NIRVA_TRUE:NIRVA_FALSE:NIRVA_FALSE:NIRVA_FALSE)

NIRVA_DEF_FUNC(NIRVA_BUNDLEPTR, NIRVA_EXPORTED_get_value_bundleptr, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_BUNDLEPTR(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

#define NIRVA_MACRO_bundle_get_blueprint(bundle) NIRVA_GET_VALUE_BUNDLEPTR(bundle, "BLUEPRINT")
#define NIRVA_BUNDLE_GET_BLUEPRINT(b) NIRVA_MACRO_bundle_get_blueprint(b)

NIRVA_DEF_FUNC(NIRVA_UINT, NIRVA_EXPORTED_get_value_uint, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_UINT, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_UINT), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_UINT(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_STRING, NIRVA_EXPORTED(get_value_string), NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_STRING, var)
NIRVA_DEF_VARS(NIRVA_UINT, nsize)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_STRING), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_STRING(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ASSIGN(nsize, NIRVA_ARRAY_GET_SIZE(bundle, stname))
for (i = 0; i < nsize; i++) NIRVA_STRING_FREE(vals[i]);
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

#define _NIRVA_BUNDLE_GET_STRAND_TYPE NIRVA_MACRO_nirva_bundle_get_strand_type
#define NIRVA_MACRO_nirva_bundle_get_strand_type(bundle)		\
    (bundle?NIRVA_GET_VALUE_UINT(bundle,"STRAND_TYPE"):STRAND_TYPE_NONE)

// check blueprint to see if there is a strand_def in "MULTI"
#define _NIRVA_GET_TEMPLATE_STRAND_DEF NIRVA_INTERNAL(get_template_strand_def)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, get_template_strand_def, NIRVA_BUNDLEPTR blueprint)
/* NIRVA_ASSERT(blueprint, NIRVA_RETURN, NIRVA_NULL) */
/* NIRVA_ASSERT(NIRVA_BUNDLE_HAS_STRAND(blueprint, "MULTI"), NIRVA_RETURN, NIRVA_NULL)  */
NIRVA_END_RETURN(NIRVA_GET_VALUE_BUNDLEPTR(blueprint, "MULTI"))

NIRVA_DEF_FUNC(NIRVA_UINT64, NIRVA_EXPORTED_get_value_uint64, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_UINT64, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_UINT64), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_UINT64(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)


#define NIRVA_MACRO_stdef_get_flags(stdef) NIRVA_GET_VALUE_UINT64(stdef, "FLAGS")
#define _NIRVA_STDEF_GET_FLAGS  NIRVA_MACRO_stdef_get_flags

#define NIRVA_CALL_METHOD_nirva_stflags_is_value MACRO
#define NIRVA_MACRO_stflags_is_value(stflags)(!(stflags & BLUEPRINT_FLAG_COMMENT))

#define NIRVA_CALL_METHOD_nirva_stdef_is_template MACRO
#define NIRVA_MACRO_stdef_is_template(stdef, blueprint)	\
    (stdef?blueprint?_NIRVA_GET_TEMPLATE_STRAND_DEF(blueprint)	\
     ==stdef?NIRVA_TRUE:NIRVA_FALSE:NIRVA_FALSE:NIRVA_FALSE)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_UINT, attr_type_to_sttype, NIRVA_UINT atype)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_ASSIGN(sttype, _NIRVA_ATTR_TYPE_TO_STRAND_TYPE_(atype))
NIRVA_END_RETURN(sttype)

#define NIRVA_MACRO_attr_get_strand_type(attr)				\
  ((attr)?NIRVA_BUNDLE_HAS_STRAND(attr, "STRAND_TYPE")?NIRVA_GET_VALUE_UINT(attr,"STRAND_TYPE"): \
   NIRVA_INTERNAL(attr_type_to_sttype)(NIRVA_GET_VALUE_UINT(attr,"ATTR_TYPE")):ATTR_TYPE_NONE)
#define _NIRVA_ATTR_GET_STRAND_TYPE NIRVA_MACRO_attr_get_strand_type
#define _NIRVA_ATTR_DEF_GET_STRAND_TYPE _NIRVA_ATTR_GET_STRAND_TYPE

#define _NIRVA_STDEF_GET_TYPE NIRVA_INTERNAL(stdef_get_type)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_UINT, stdef_get_type, NIRVA_BUNDLEPTR stdef, NIRVA_BUNDLEPTR bundle)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_ASSERT(stdef, NIRVA_RETURN, STRAND_TYPE_NONE);
NIRVA_ASSIGN(sttype, _NIRVA_BUNDLE_GET_STRAND_TYPE(stdef))
if (sttype == STRAND_TYPE_PROXIED) {
  NIRVA_DEF_VARS(NIRVA_STRING, stname)
  NIRVA_ASSERT(bundle, NIRVA_RETURN, STRAND_TYPE_NONE)
  NIRVA_ASSIGN(stname, NIRVA_GET_VALUE_STRING(stdef, "TYPE_PROXY"))
  if (!NIRVA_BUNDLE_HAS_STRAND(bundle, stname)) {
    NIRVA_ASSIGN(sttype, NIRVA_GET_VALUE_UINT(bundle, stname));
  }
  NIRVA_STRING_FREE(stname)
  if (sttype == STRAND_TYPE_PROXIED) {
    NIRVA_ASSIGN(sttype, STRAND_TYPE_UNDEFINED)
  }
}
NIRVA_END_RETURN(sttype)

#define MY_ROLE_IS(role) 0

#ifndef NEED_WAIT_RETRY
#define _NIRVA_DEPLOY_def_nirva_wait_retry
#else
#if NIRVA_IMPL_IS(DEFAULT_C)
#undef NEED_WAIT_RETRY
#define _NIRVA_DEPLOY_def_nirva_wait_retry				\
  NIRVA_DEF_FUNC(NIRVA_NO_RETURN, _def_nirva_wait_retry, NIRVA_VOID)	\
    NIRVA_DEF_VARS(struct timespec, ts)					\
    ts.tv_sec = 0;							\
  ts.tv_nsec = 50000;							\
  while (nanosleep(&ts, &ts) == -1 &&  errno != ETIMEDOUT);		\
  NIRVA_FUNC_END
#endif
#endif

static void NIRVA_ERR_CHK(NIRVA_BUNDLEPTR *b, ...) {
}

NIRVA_DECL_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_strand_delete, NIRVA_BUNDLEPTR, NIRVA_CONST_STRING)

#ifdef NEED_NIRVA_RECYCLE
NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_recycle, NIRVA_BUNDLEPTR bun)
NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, stnames)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESULT_ERROR)
//NIRVA_ASSIGN(refs, _NIRVA_BUNDLE_UNREF(bun))
//if (refs > 0) {NIRVA_RETURN_FAIL}
NIRVA_ASSIGN(stnames, NIRVA_BUNDLE_LIST_STRANDS(bun))
for (i = 0; stnames[i]; i++) {
  NIRVA_STRAND_DELETE(bun, stnames[i])
  NIRVA_STRING_FREE(stnames[i])
}
NIRVA_ARRAY_FREE(stnames)
NIRVA_BUNDLE_FREE(bun)
NIRVA_END_SUCCESS
#endif

#ifndef NIRVA_IDX_PREFIX
#define NIRVA_IDX_PREFIX "IDX_"
#endif
#define _NIRVA_PREFIX_KNAME NIRVA_INTERNAL(prefix_kname)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_STRING, prefix_kname, NIRVA_CONST_STRING keyval, NIRVA_CONST_STRING prefix)
NIRVA_DEF_VARS(NIRVA_STRING, kname)
NIRVA_DEF_VARS(NIRVA_SIZE, stlen, pfxlen)
NIRVA_ASSIGN(stlen, NIRVA_STRING_LENGTH(keyval))
NIRVA_ASSIGN(pfxlen, NIRVA_STRING_LENGTH(prefix))
NIRVA_STRING_ALLOC(kname, stlen + pfxlen + 100)
NIRVA_FMT(kname, stlen + pfxlen + 1, NIRVA_FMT_2STRING, prefix, keyval)
NIRVA_END_RETURN(kname)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BOOLEAN, idx_has_key, NIRVA_BUNDLEPTR idx, NIRVA_CONST_STRING keyval, NIRVA_CONST_STRING prefix)
NIRVA_DEF_ASSIGN(NIRVA_STRING, kname, _NIRVA_PREFIX_KNAME, keyval, prefix)
if (!NIRVA_BUNDLE_HAS_STRAND(idx, kname)) {
  NIRVA_STRING_FREE(kname)
  NIRVA_RETURN(NIRVA_FALSE)
}
NIRVA_STRING_FREE(kname)
NIRVA_END_RETURN(NIRVA_TRUE)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, get_idx_value_bundleptr, NIRVA_BUNDLEPTR idx,
                        NIRVA_CONST_STRING keyval, NIRVA_CONST_STRING prefix)
NIRVA_DEF_ASSIGN(NIRVA_STRING, kname, _NIRVA_PREFIX_KNAME, keyval, prefix)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, bun)
if (!NIRVA_BUNDLE_HAS_STRAND(idx, kname)) {
  NIRVA_STRING_FREE(kname)
  NIRVA_RETURN(NIRVA_NULL)
}
NIRVA_CALL_ASSIGN(bun, NIRVA_GET_VALUE_BUNDLEPTR, idx, kname)
NIRVA_STRING_FREE(kname)
NIRVA_END_RETURN(bun)

#ifdef NEED_NIRVA_KEYED_ARRAYS
// in case of error or non-existent keyval, we return NIRVA_NULL, however beware, since this is also a valid return value
NIRVA_DEF_FUNC(NIRVA_BUNDLEPTR, NIRVA_EXPORTED_get_value_by_key, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING stname, NIRVA_CONST_STRING keyval)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, xbun, idx)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_NULL)
//if (!NIRVA_IS_KEYED(bun, stname)) NIRVA_RETURN(NIRVA_NULL);
NIRVA_ASSERT(keyval, NIRVA_RETURN, NIRVA_NULL)
NIRVA_CALL_ASSIGN(idx, NIRVA_GET_VALUE_BUNDLEPTR, bun, "INDEX")
NIRVA_ASSERT(idx, NIRVA_RETURN, NIRVA_NULL)
NIRVA_CALL_ASSIGN(xbun, NIRVA_INTERNAL(get_idx_value_bundleptr), idx, keyval, NIRVA_IDX_PREFIX)
NIRVA_END_RETURN(xbun)
NIRVA_EXPORTS(get_value_by_key)
#endif

#ifdef NIRVA_MACRO_get_value_by_key
#define _NIRVA_MACRO_get_value_by_key NIRVA_MACRO_get_value_by_key
#else
#define _NIRVA_MACRO_get_value_by_key(...)NIRVA_NULL
#endif

#define _NIRVA_GET_STRAND_DEF NIRVA_INTERNAL(get_strand_def)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, get_strand_def, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING name)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef, blueprint)
NIRVA_ASSIGN(blueprint, NIRVA_BUNDLE_GET_BLUEPRINT(bundle))
NIRVA_ASSERT(blueprint, NIRVA_FAIL, NIRVA_NULL);
NIRVA_ASSIGN(stdef, NIRVA_GET_VALUE_BY_KEY(blueprint, "STRAND_DEFS", name))
if (!stdef) {
  NIRVA_ASSIGN(stdef, _NIRVA_GET_TEMPLATE_STRAND_DEF(blueprint))
  // if we have and index stdef, return that
}
NIRVA_END_RETURN(stdef)

#define _NIRVA_IS_KEYED_ARRAY NIRVA_INTERNAL(is_keyed_array)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BOOLEAN, is_keyed_array, NIRVA_BUNDLEPTR bun, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef)
NIRVA_DEF_VARS(NIRVA_UINT64, flags)

NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_FALSE)

NIRVA_ASSERT(stname, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_ASSIGN(stdef, _NIRVA_GET_STRAND_DEF(bun, stname))
NIRVA_ASSIGN(flags, _NIRVA_STDEF_GET_FLAGS(stdef))
if (NIRVA_HAS_FLAG(flags, BLUEPRINT_FLAG_KEYED_ARRAY)) NIRVA_RETURN(NIRVA_TRUE);
NIRVA_END_RETURN(NIRVA_FALSE)

NIRVA_DECL_FUNC_INTERNAL(NIRVA_FUNC_RETURN, set_proxy_strand_type, NIRVA_BUNDLEPTR, NIRVA_BUNDLEPTR, NIRVA_UINT)

#define _NIRVA_STRAND_GET_TYPE NIRVA_INTERNAL_strand_get_type
NIRVA_DEF_FUNC_INTERNAL(NIRVA_UINT, strand_get_type, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef)
NIRVA_ASSERT(bundle, NIRVA_RETURN, STRAND_TYPE_INVALID)
NIRVA_ASSIGN(stdef, _NIRVA_GET_STRAND_DEF(bundle, stname))
NIRVA_ASSERT(stdef, NIRVA_RETURN, STRAND_TYPE_INVALID)
NIRVA_END_RETURN(_NIRVA_STDEF_GET_TYPE(stdef, bundle))

NIRVA_DECL_FUNC_INTERNAL(NIRVA_FUNC_RETURN, append_sub_bundles, NIRVA_BUNDLEPTR,
                         NIRVA_CONST_STRING, NIRVA_UINT, NIRVA_VA_LIST)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_INT, array_append_va, NIRVA_BUNDLEPTR bun,
                        NIRVA_CONST_STRING stname, NIRVA_UINT sttype, NIRVA_UINT ne, NIRVA_VA_LIST v)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef)
NIRVA_DEF_VARS(NIRVA_UINT, xtype)
NIRVA_DEF_SET(NIRVA_INT, ns, -1)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, retval)
if (sttype == STRAND_TYPE_BUNDLEPTR || sttype == STRAND_TYPE_CONST_BUNDLEPTR) {
  // for keyed_arrays, we must add_value_by_key
  if (_NIRVA_IS_KEYED_ARRAY(bun, stname)) NIRVA_RETURN_ERROR;
}
NIRVA_ASSIGN(stdef, _NIRVA_GET_STRAND_DEF(bun, stname))
NIRVA_ASSIGN(xtype, _NIRVA_STDEF_GET_TYPE(stdef, bun))
if (xtype == STRAND_TYPE_UNDEFINED) {
  NIRVA_ASSIGN(retval, NIRVA_INTERNAL(set_proxy_strand_type)(stdef, bun, sttype))
  if (retval != NIRVA_RESULT_SUCCESS) NIRVA_RETURN(retval);
  NIRVA_ASSIGN(xtype, sttype)
}

if (xtype != sttype) {
  // TODO - in gen 1,2,3 -
  // if we have a bundleptr / const_bundleptr mismatch
  // - if the array is empty, we are allowed to change strand_type from bundleptr to const_bundleptr
  // - however we need to delete the strand, delete type_proxy (if proxied), reset proxy type
  // - then append without furtehr checking

  // after gen3, either we would fail, or if the owner, we would reparent the bundles instead

  NIRVA_RETURN_FAIL
}

switch (sttype) {
case STRAND_TYPE_BUNDLEPTR: {
  NIRVA_ASSIGN(ns, NIRVA_INTERNAL(append_sub_bundles)(bun, stname, ne, v))
} break;
case STRAND_TYPE_INT: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_INT), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_INT)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_UINT: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_UINT), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_UINT)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_BOOLEAN: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_BOOLEAN), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_BOOLEAN)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_DOUBLE: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_DOUBLE), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_DOUBLE)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_INT64: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_INT64), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_INT64)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_UINT64: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_UINT64), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_UINT64)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_STRING: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_STRING), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_STRING)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_VOIDPTR: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_VOIDPTR), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_VOIDPTR)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_FUNCPTR: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_NATIVE_FUNC), ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_NATIVE_FUNC)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
case STRAND_TYPE_CONST_BUNDLEPTR: {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, xbun, NIRVA_GET_VALUE_BUNDLEPTR(bun, stname))
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_CONST_BUNDLEPTR), ival)
  if (xbun) {
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR, con, NIRVA_GET_VALUE_BUNDLEPTR(xbun, "CONTAINER"))
    if (con == bun) {
      NIRVA_DEF_SET(NIRVA_STRING, constr, NIRVA_GET_VALUE_STRING(xbun, "CONTAINER_STRAND"))
      if (NIRVA_STRING_EQUAL(constr, stname)) {
        NIRVA_STRING_FREE(constr)
        NIRVA_RETURN(-1)
      }
      NIRVA_STRING_FREE(constr)
    }
  }
  NIRVA_ASSIGN(ival, NIRVA_VA_ARG(v, NIRVA_TYPE_PTR(NIRVA_CONST_BUNDLEPTR)))
  NIRVA_ASSIGN(ns, _NIRVA_ARRAY_APPEND(bun, stname, sttype, ne, ival))
} break;
default: break;
}
NIRVA_END_RETURN(ns)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, add_idx_value, NIRVA_BUNDLEPTR idx, NIRVA_CONST_STRING keyval,
                        NIRVA_CONST_STRING prefix,
                        NIRVA_UINT sttype, NIRVA_VA_LIST var)
NIRVA_DEF_ASSIGN(NIRVA_STRING, kname, _NIRVA_PREFIX_KNAME, keyval, prefix)
NIRVA_DEF_VARS(NIRVA_INT, ne)
if (NIRVA_BUNDLE_HAS_STRAND(idx, kname)) {
  NIRVA_STRING_FREE(kname)
  NIRVA_RETURN_FAIL
}
NIRVA_CALL_ASSIGN(ne, NIRVA_INTERNAL(array_append_va), idx, kname, sttype, 1, var)
NIRVA_STRING_FREE(kname)
if (ne != 1) {NIRVA_RETURN_ERROR}
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC(NIRVA_INT, NIRVA_EXPORTED_array_append, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING item, NIRVA_UINT xtype, NIRVA_UINT ne, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_VA_LIST, v)
NIRVA_VA_START(v, ne)
NIRVA_ASSIGN(ne, NIRVA_INTERNAL(array_append_va)(bun, item, xtype, ne, v))
NIRVA_VA_END(v)
NIRVA_END_RETURN(ne)

// clear array - WARNING, this version may free included sub bundles
// it will also remove the strand

NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_array_clear, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_UINT, stype)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, subs)
NIRVA_ASSERT(bun, NIRVA_FAIL, NIRVA_NULL)
if (!NIRVA_BUNDLE_HAS_STRAND(bun, stname)) {NIRVA_RETURN_ERROR}
// if ASPECT_BUNDLES is set to NIRVA_AUtOMATION_FULL
// clear should do this
NIRVA_ASSIGN(stype, _NIRVA_STRAND_GET_TYPE(bun, stname))
if (stype == STRAND_TYPE_BUNDLEPTR) {
  NIRVA_DEF_VARS(NIRVA_INT, i)
  NIRVA_DEF_VARS(NIRVA_UINT, asize)
  if (NIRVA_FUNC_GENERATION >= 3) {
    // TODO
    // check if caller is owner of array
  }
  NIRVA_ASSIGN(subs, NIRVA_GET_ARRAY_BUNDLEPTR(bun, stname))
  NIRVA_ASSIGN(asize, NIRVA_ARRAY_GET_SIZE(bun, stname))
  for (i = 0; i < asize; i++) {
    NIRVA_RECYCLE(subs[i])
  }
  NIRVA_ARRAY_FREE(subs)
}
_NIRVA_ARRAY_CLEAR(bun, stname)

#ifdef NIRVA_NEEDS_KEYED_ARRAYS
if (_NIRVA_IS_KEYED_ARRAY(bun, stname)) {NIRVA_STRAND_DELETE(bun, "INDEX")}
#endif
/* if (NIRVA_BUNDLE_HAS_STRAND(bun, stname)) */
/*   NIRVA_STRAND_DELETE(bun, stname) */
NIRVA_END_SUCCESS;

NIRVA_DEF_FUNC_INTERNAL(NIRVA_INT, array_replace, NIRVA_BUNDLEPTR bun,
                        NIRVA_CONST_STRING stname, NIRVA_UINT sttype, NIRVA_UINT ne, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_VA_LIST, vars)
NIRVA_DEF_VARS(NIRVA_INT, ns)
NIRVA_DEF_SET(NIRVA_FUNC_RETURN, res, NIRVA_ARRAY_CLEAR(bun, stname))
if (res != NIRVA_RESULT_SUCCESS) {NIRVA_RETURN(res)}
NIRVA_VA_START(vars, ne)
NIRVA_ASSIGN(ns, NIRVA_INTERNAL(array_append_va)(bun, stname, sttype, ne, vars))
NIRVA_VA_END(vars)
NIRVA_END_RETURN(ns)

// TODO - call hooks
NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_strand_delete, NIRVA_BUNDLEPTR bun, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESULT_PARAM_INVALID)
NIRVA_ASSIGN(sttype, _NIRVA_STRAND_GET_TYPE(bun, stname))
if (sttype == STRAND_TYPE_BUNDLEPTR) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, xbun, NIRVA_GET_VALUE_BUNDLEPTR(bun, stname))
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, con, NIRVA_GET_VALUE_BUNDLEPTR(xbun, "CONTAINER"))
  if (con == bun) {
    NIRVA_DEF_SET(NIRVA_STRING, constr, NIRVA_GET_VALUE_STRING(xbun, "CONTAINER_STRAND"))
    if (NIRVA_STRING_EQUAL(constr, stname)) {
      NIRVA_RECYCLE(xbun)
    }
    NIRVA_STRING_FREE(constr)
  }
}
_NIRVA_STRAND_DELETE(bun, stname)
NIRVA_END_SUCCESS

#ifdef NEED_NIRVA_STRAND_COPY
NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_strand_copy, NIRVA_BUNDLEPTR dest, NIRVA_CONST_STRING dstrand,
               NIRVA_BUNDLEPTR src, NIRVA_CONST_STRING sstrand)
NIRVA_DEF_VARS(NIRVA_UINT, asize, sttype)
NIRVA_DEF_SET(NIRVA_FUNC_RETURN, res, NIRVA_RESULT_ERROR)
NIRVA_ASSERT(src, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSERT(dest, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSERT(NIRVA_BUNDLE_HAS_STRAND(src, sstrand), NIRVA_RETURN, NIRVA_RESULT_FAIL)
NIRVA_ASSIGN(asize, NIRVA_ARRAY_GET_SIZE(src, sstrand))
NIRVA_ASSERT(asize, NIRVA_RETURN, NIRVA_RESULT_FAIL)
NIRVA_ASSIGN(sttype, _NIRVA_STRAND_GET_TYPE(src, sstrand))
switch (sttype) {
case STRAND_TYPE_INT: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_INT), intp, NIRVA_GET_ARRAY_INT(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, intp))
  NIRVA_ARRAY_FREE(intp)
} break;
case STRAND_TYPE_UINT: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_UINT), uintp, NIRVA_GET_ARRAY_UINT(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, uintp))
  NIRVA_ARRAY_FREE(uintp)
} break;
case STRAND_TYPE_INT64: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_INT64), int64p, NIRVA_GET_ARRAY_INT64(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, int64p))
  NIRVA_ARRAY_FREE(int64p)
} break;
case STRAND_TYPE_UINT64: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_UINT64), uint64p, NIRVA_GET_ARRAY_UINT64(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, uint64p))
  NIRVA_ARRAY_FREE(uint64p)
} break;
case STRAND_TYPE_BOOLEAN: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_BOOLEAN), booleanp, NIRVA_GET_ARRAY_BOOLEAN(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, booleanp))
  NIRVA_ARRAY_FREE(booleanp)
} break;
case STRAND_TYPE_DOUBLE: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_DOUBLE), doublep, NIRVA_GET_ARRAY_DOUBLE(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, doublep))
  NIRVA_ARRAY_FREE(doublep)
} break;
case STRAND_TYPE_STRING: {
  NIRVA_DEF_VARS(NIRVA_INT, i)
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_STRING), stringp, NIRVA_GET_ARRAY_STRING(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, stringp))
  for (i = 0; i < asize; i++) {
    if (stringp[i]) NIRVA_STRING_FREE(stringp[i]);
  }
  NIRVA_ARRAY_FREE(stringp)
} break;
case STRAND_TYPE_VOIDPTR: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_VOIDPTR), voidptrp, NIRVA_GET_ARRAY_VOIDPTR(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, voidptrp))
  NIRVA_ARRAY_FREE(voidptrp)
} break;
case STRAND_TYPE_FUNCPTR: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_NATIVE_FUNC), funcptrp, NIRVA_GET_ARRAY_FUNCPTR(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, sttype, asize, funcptrp))
  NIRVA_ARRAY_FREE(funcptrp)
} break;
case STRAND_TYPE_BUNDLEPTR: {
  NIRVA_DEF_SET(NIRVA_TYPE_PTR(NIRVA_BUNDLEPTR), bundleptrp, NIRVA_GET_ARRAY_BUNDLEPTR(src, sstrand))
  NIRVA_ASSIGN(res, NIRVA_ARRAY_SET(dest, dstrand, STRAND_TYPE_CONST_BUNDLEPTR, asize, bundleptrp))
  NIRVA_ARRAY_FREE(bundleptrp)
} break;
default:
  NIRVA_FAIL("unknown strand_type")
  break;
}
NIRVA_END_RETURN(res)
NIRVA_EXPORTS(strand_copy)
#endif

#ifdef NIRVA_MACRO_strand_copy
#define _NIRVA_MACRO_strand_copy(...)NIRVA_MACRO_strand_copy(__VA_ARGS__)
#else
#define _NIRVA_MACRO_strand_copy(...)0
#endif

NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, nirva_strands_copy, NIRVA_BUNDLEPTR dst,
                        NIRVA_BUNDLEPTR src, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_VA_LIST, va)
NIRVA_VA_START(va, src)
while (1) {
  NIRVA_DEF_SET(NIRVA_STRING, stname, NIRVA_VA_ARG(va, NIRVA_STRING))
  if (!stname) break;
  NIRVA_STRAND_COPY(dst, stname, src, stname)
};
NIRVA_VA_END(va)
NIRVA_FUNC_END

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, check_can_add_va, NIRVA_BUNDLEPTR bun, NIRVA_CONST_STRING stname,
                        NIRVA_UINT sttype, NIRVA_VA_LIST va)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef)
NIRVA_DEF_VARS(NIRVA_UINT, xtype)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_NULL)
// check - strand type, restrictions, max_size
NIRVA_ASSIGN(stdef, _NIRVA_GET_STRAND_DEF(bun, stname))
NIRVA_ASSERT(stdef, NIRVA_RETURN, NIRVA_NULL)
NIRVA_ASSIGN(xtype, _NIRVA_STDEF_GET_TYPE(stdef, bun))
if (sttype != xtype && xtype != STRAND_TYPE_UNDEFINED) {
  NIRVA_RETURN(NIRVA_NULL)
}
NIRVA_END_RETURN(stdef)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, check_can_add, NIRVA_BUNDLEPTR bun, NIRVA_CONST_STRING stname,
                        NIRVA_UINT sttype, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef)
NIRVA_DEF_VARS(NIRVA_VA_LIST, va)
NIRVA_VA_START(va, sttype)
NIRVA_ASSIGN(stdef, NIRVA_INTERNAL(check_can_add_va)(bun, stname, sttype, va))
NIRVA_VA_END(va)
NIRVA_END_RETURN(stdef)

NIRVA_DECL_FUNC_INTERNAL(NIRVA_FUNC_RETURN, include_sub_bundle, NIRVA_BUNDLEPTR,
                         NIRVA_CONST_STRING, NIRVA_VA_LIST)
#define _NIRVA_INCLUDE_SUB_BUNDLE(...) NIRVA_CMD(NIRVA_INTERNAL(include_sub_bundle)(__VA_ARGS__))

// this is equivalent to nirva_array_replace, except we take the address of var, and append as a single element
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, set_value, NIRVA_BUNDLEPTR bundle,
                        NIRVA_CONST_STRING stname, NIRVA_UINT sttype, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_VA_LIST, var)
NIRVA_DEF_VARS(NIRVA_UINT, xtype)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, retval)

NIRVA_DEF_SET(NIRVA_BUNDLEPTR, stdef, NIRVA_INTERNAL(check_can_add_va)(bundle, stname, sttype, var))
// TODO - if we failed due to bundleptr / const_bundleptr mismatch, we
NIRVA_ASSERT(stdef, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(xtype, _NIRVA_STDEF_GET_TYPE(stdef, bundle))

if (xtype == STRAND_TYPE_UNDEFINED) {
  NIRVA_ASSIGN(retval, NIRVA_INTERNAL(set_proxy_strand_type)(stdef, bundle, sttype))
  if (retval != NIRVA_RESULT_SUCCESS) NIRVA_RETURN(retval);
  NIRVA_ASSIGN(xtype, sttype)
}

// TODO - check if we have data_hook(s) for bundle, stname
// if so, we need to create new_val
// NIRVA_CALL_ASSIGN(new_val, NIRVA_CREATE_BUNDLE_BY_TYPE, VALUE_BUNDLE_TYPE, "STRAND_TYPE", sttype, NIRVA_NULL)

if (!NIRVA_BUNDLE_HAS_STRAND(bundle, stname)) {
  // TODO - check if there is config_change hook for bundle
} else {
  // TODO - check if we have data_hook(s) for bundle, stname
  /* NIRVA_CALL_ASSIGN(old_val, NIRVA_CREATE_BUNDLE_BY_TYPE, VALUE_BUNDLE_TYPE, "STRAND_TYPE", xtype, NIRVA_NULL)  */
  /*   NIRVA_CALL(nirva_strand_copy, old_val, "DATA", bundle, stname) */
  /*NIRVA_HOOK_TRIGGER(bundle,stname, old_val, new_val)) {*/
  NIRVA_ASSIGN(retval, NIRVA_ARRAY_CLEAR(bundle, stname))
  if (retval != NIRVA_RESULT_SUCCESS) NIRVA_RETURN(retval);
}

NIRVA_VA_START(var, sttype)
// call array_append, but we only add a single item

switch (sttype) {
case STRAND_TYPE_BUNDLEPTR: {
  _NIRVA_INCLUDE_SUB_BUNDLE(bundle, stname, var)
} break;
case STRAND_TYPE_INT: {
  NIRVA_DEF_SET(NIRVA_INT, ival, NIRVA_VA_ARG(var, NIRVA_INT))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_UINT: {
  NIRVA_DEF_SET(NIRVA_UINT, ival, NIRVA_VA_ARG(var, NIRVA_UINT))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_BOOLEAN: {
  NIRVA_DEF_SET(NIRVA_BOOLEAN, ival, NIRVA_VA_ARG(var, NIRVA_BOOLEAN))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_DOUBLE: {
  NIRVA_DEF_SET(NIRVA_DOUBLE, ival, NIRVA_VA_ARG(var, NIRVA_DOUBLE))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_INT64: {
  NIRVA_DEF_SET(NIRVA_INT64, ival, NIRVA_VA_ARG(var, NIRVA_INT64))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_UINT64: {
  NIRVA_DEF_SET(NIRVA_UINT64, ival, NIRVA_VA_ARG(var, NIRVA_UINT64))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_STRING: {
  NIRVA_DEF_SET(NIRVA_STRING, ival, NIRVA_VA_ARG(var, NIRVA_STRING))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_VOIDPTR: {
  NIRVA_DEF_SET(NIRVA_VOIDPTR, ival, NIRVA_VA_ARG(var, NIRVA_VOIDPTR))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_FUNCPTR: {
  NIRVA_DEF_SET(NIRVA_NATIVE_FUNC, ival, NIRVA_VA_ARG(var, NIRVA_NATIVE_FUNC))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
case STRAND_TYPE_CONST_BUNDLEPTR: {
  NIRVA_DEF_SET(NIRVA_CONST_BUNDLEPTR, ival, NIRVA_VA_ARG(var, NIRVA_CONST_BUNDLEPTR))
  _NIRVA_ARRAY_APPENDx(bundle, stname, sttype, 1, &ival)
} break;
default: break;
}

// TODO - check if we have data_hook(s) for bundle, stname
// call after hook

// TODO - check if we need to call after_strand_added hook

NIRVA_VA_END(var)
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, set_proxy_strand_type, NIRVA_BUNDLEPTR stdef, NIRVA_BUNDLEPTR bundle,
                        NIRVA_UINT sttype)
NIRVA_DEF_VARS(NIRVA_UINT, xtype)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, res)
NIRVA_DEF_VARS(NIRVA_STRING, type_proxy)
NIRVA_ASSERT(stdef, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSERT(bundle, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(xtype, _NIRVA_STDEF_GET_TYPE(stdef, bundle))
if (xtype != STRAND_TYPE_UNDEFINED) {
  NIRVA_RETURN_FAIL
}
NIRVA_ASSIGN(type_proxy, NIRVA_GET_VALUE_STRING(stdef, "TYPE_PROXY"))
NIRVA_ASSERT(type_proxy, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(res, NIRVA_VALUE_SET(bundle, type_proxy, STRAND_TYPE_UINT, sttype))
NIRVA_STRING_FREE(type_proxy)
NIRVA_END_RETURN(res)

NIRVA_DEF_FUNC(NIRVA_INT, NIRVA_EXPORTED_get_value_int, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_INT, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_INT), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_INT(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_INT64, NIRVA_EXPORTED_get_value_int64, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_INT64, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_INT64), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_INT64(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_BOOLEAN, NIRVA_EXPORTED_get_value_boolean, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_BOOLEAN, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_BOOLEAN), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_BOOLEAN(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_DOUBLE, NIRVA_EXPORTED_get_value_double, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_DOUBLE, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_DOUBLE), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_DOUBLE(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_VOIDPTR, NIRVA_EXPORTED_get_value_voidptr, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_VOIDPTR, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_VOIDPTR), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_VOIDPTR(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

NIRVA_DEF_FUNC(NIRVA_NATIVE_FUNC, NIRVA_EXPORTED_get_value_funcptr, NIRVA_BUNDLEPTR bundle, NIRVA_CONST_STRING stname)
NIRVA_DEF_VARS(NIRVA_NATIVE_FUNC, var)
NIRVA_DEF_VARS(NIRVA_ARRAY_OF(NIRVA_NATIVE_FUNC), vals)
if (!_NIRVA_CHECK_VALUES(bundle, stname)) NIRVA_FAIL(NIRVA_NULL);
NIRVA_ASSIGN(vals, NIRVA_GET_ARRAY_FUNCPTR(bundle, stname))
NIRVA_ASSIGN(var, vals[0])
NIRVA_ARRAY_FREE(vals)
NIRVA_END_RETURN(var)

#define NIRVA_CALL_METHOD_assert_range INLINE
#define NIRVA_ASSERT_RANGE(var, min, mx, onfail, ...) INLINE_FUNC_CALL(assert_range, __VA_ARGS__)
#define _INLINE_INTERNAL_assert_range(var, min, mx, onfail, ...) NIRVA_INLINE \
  (if (var < min || var > mx) onfail(__VA_ARGS__);)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, append_sub_bundles, NIRVA_BUNDLEPTR bundle,
                        NIRVA_CONST_STRING sname, NIRVA_UINT ns, NIRVA_VA_LIST va)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, stdef, container)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, subs)
NIRVA_ASSERT(bundle, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(subs, NIRVA_VA_ARG(va, NIRVA_BUNDLEPTR_ARRAY))
for (i = 0; i < ns; i++) {
  NIRVA_ASSIGN(stdef, NIRVA_INTERNAL(check_can_add)(bundle, sname, STRAND_TYPE_BUNDLEPTR, subs[i]))
  NIRVA_ASSERT(stdef, NIRVA_RETURN, NIRVA_RESULT_FAIL)
  NIRVA_ASSIGN(sttype, _NIRVA_STDEF_GET_TYPE(stdef, bundle))
  if (sttype == STRAND_TYPE_CONST_BUNDLEPTR) {
    NIRVA_RETURN(NIRVA_ARRAY_APPEND(bundle, sname, STRAND_TYPE_CONST_BUNDLEPTR, ns, subs))
  }
  NIRVA_ASSIGN(container, NIRVA_GET_VALUE_BUNDLEPTR(subs[i], "CONTAINER"))
  if (container && container != bundle) {
    if (NIRVA_FUNC_GENERATION < 3) {
      // until generation 3, we are still in early bootstrap, so we can freely reparent sub bundles
      // - provided: ALL bundles have "container" == self
      if (container != subs[i]) {
        NIRVA_RETURN(NIRVA_ARRAY_APPEND(bundle, sname, STRAND_TYPE_CONST_BUNDLEPTR, ns, subs))
      }
    } else {
      //this is permitted IF the container has a strand "OWNER" and the value is equal
      // or if container UID == caller uid
      NIRVA_RETURN(NIRVA_ARRAY_APPEND(bundle, sname, STRAND_TYPE_CONST_BUNDLEPTR, ns, subs))
    }
  }
}
for (int i = 0; i < ns; i++) {
  NIRVA_VALUE_SET(subs[i], "CONTAINER", STRAND_TYPE_CONST_BUNDLEPTR, bundle)
  NIRVA_VALUE_SET(subs[i], "CONTAINER_STRAND", STRAND_TYPE_STRING, sname)
}
NIRVA_END_RETURN(_NIRVA_ARRAY_APPEND(bundle, sname, STRAND_TYPE_BUNDLEPTR, ns, va))

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, include_sub_bundle_redir, NIRVA_BUNDLEPTR bundle,
                        NIRVA_CONST_STRING sname, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_VA_LIST, var)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, res)
NIRVA_VA_START(var, sname)
NIRVA_ASSIGN(res, NIRVA_INTERNAL(append_sub_bundles)(bundle, sname, 1, var))
NIRVA_VA_END(var)
NIRVA_END_RETURN(res)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, include_sub_bundle, NIRVA_BUNDLEPTR bundle,
                        NIRVA_CONST_STRING sname, NIRVA_VA_LIST va)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, xbun, NIRVA_VA_ARG(va, NIRVA_BUNDLEPTR))
NIRVA_END_RETURN(NIRVA_INTERNAL(include_sub_bundle_redir)(bundle, sname, &xbun))

#ifdef NEED_NIRVA_KEYED_ARRAYS
NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_add_value_by_key, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING stname, NIRVA_CONST_STRING keyval, NIRVA_UINT sttype, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, idx)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, res)
NIRVA_DEF_VARS(NIRVA_VA_LIST, var)
NIRVA_DEF_VARS(NIRVA_UINT, ne)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESULT_ERROR)
if (sttype != STRAND_TYPE_BUNDLEPTR && sttype != STRAND_TYPE_CONST_BUNDLEPTR) {NIRVA_RETURN_ERROR}
NIRVA_ASSERT(stname, NIRVA_RETURN, NIRVA_RESULT_ERROR)
if (NIRVA_FUNC_GENERATION >= 3) {if (!_NIRVA_IS_KEYED_ARRAY(bun, stname)) NIRVA_RETURN_ERROR;}
NIRVA_ASSERT(keyval, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_CALL_ASSIGN(idx, NIRVA_GET_VALUE_BUNDLEPTR, bun, "INDEX")
if (idx) {
  if (NIRVA_RESULT(NIRVA_INTERNAL(idx_has_key), idx, keyval, NIRVA_IDX_PREFIX))
    NIRVA_RETURN_FAIL
  }
NIRVA_VA_START(var, sttype)
NIRVA_CALL_ASSIGN(ne, NIRVA_INTERNAL(array_append_va), bun, stname, sttype, 1, var)
NIRVA_VA_END(var)
if (ne < 0) {NIRVA_RETURN_ERROR}
if (!idx) {
  NIRVA_ASSIGN(idx, create_bundle_by_type(INDEX_BUNDLE_TYPE, "STRAND_TYPE", sttype, NIRVA_NULL))
  NIRVA_ASSERT(idx, NIRVA_RETURN, NIRVA_RESULT_ERROR)
}

NIRVA_VA_START(var, sttype)
NIRVA_CALL_ASSIGN(res, NIRVA_INTERNAL(add_idx_value), idx, keyval, NIRVA_IDX_PREFIX, sttype, var)
NIRVA_VA_END(var)
NIRVA_END_RETURN(res)

NIRVA_DEF_FUNC(NIRVA_BOOLEAN, NIRVA_EXPORTED_has_value_for_key, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING stname, NIRVA_CONST_STRING keyval)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, idx)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_ASSERT(stname, NIRVA_RETURN, NIRVA_FALSE)
//if (!_NIRVA_IS_KEYED_ARRAY(bun, stname)) {NIRVA_RETURN(NIRVA_FALSE);}
NIRVA_ASSERT(keyval, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_ASSIGN(idx, NIRVA_GET_VALUE_BUNDLEPTR(bun, "INDEX"))
NIRVA_ASSERT(idx, NIRVA_RETURN, NIRVA_FALSE)
NIRVA_END_RESULT(NIRVA_INTERNAL(idx_has_key), idx, keyval, NIRVA_IDX_PREFIX)

NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, NIRVA_EXPORTED_remove_value_by_key, NIRVA_BUNDLEPTR bun,
               NIRVA_CONST_STRING stname, NIRVA_CONST_STRING keyval)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, remval, idx)
NIRVA_DEF_VARS(NIRVA_STRING, kname)
NIRVA_DEF_VARS(NIRVA_UINT, sttype, nitems)
NIRVA_DEF_VARS(NIRVA_INT, i, j)
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSERT(stname, NIRVA_RETURN, NIRVA_RESULT_ERROR)
if (!_NIRVA_IS_KEYED_ARRAY(bun, stname)) {NIRVA_RETURN_ERROR;}
NIRVA_ASSERT(keyval, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(idx, NIRVA_GET_VALUE_BUNDLEPTR(bun, "INDEX"))
NIRVA_ASSERT(idx, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_ASSIGN(kname, _NIRVA_PREFIX_KNAME(keyval, NIRVA_IDX_PREFIX))
if (!NIRVA_BUNDLE_HAS_STRAND(idx, kname)) {
  NIRVA_STRING_FREE(kname)
  NIRVA_RETURN(NIRVA_RESULT_ERROR)
}
NIRVA_ASSIGN(remval, NIRVA_GET_VALUE_BUNDLEPTR(idx, kname))
NIRVA_STRAND_DELETE(idx, kname)
NIRVA_STRING_FREE(kname)
NIRVA_ASSIGN(nitems, NIRVA_ARRAY_GET_SIZE(bun, stname))
NIRVA_ASSIGN(sttype, _NIRVA_STRAND_GET_TYPE(bun, stname))
if (nitems > 1) {
  if (sttype == STRAND_TYPE_BUNDLEPTR) {
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR_ARRAY, array, NIRVA_GET_ARRAY_BUNDLEPTR(bun, stname))
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR_ARRAY, xarray, NIRVA_MALLOC_ARRAY(NIRVA_BUNDLEPTR, nitems - 1))
    NIRVA_ASSIGN(j, 0)
    for (i = 0; i < nitems; i++) {
      if (array[i] == remval) continue;
      NIRVA_ASSIGN(xarray[j++], array[i])
    }
    NIRVA_ARRAY_FREE(array)

    // must be careful here not to free the sub bundles, and not to remove INDEX  when clearing array
    _NIRVA_ARRAY_CLEAR(bun, stname)

    // "container" is already set to bun, so we should have no problems here
    NIRVA_CALL(NIRVA_ARRAY_APPEND, bun, stname, STRAND_TYPE_BUNDLEPTR, nitems - 1, xarray)
    NIRVA_ARRAY_FREE(xarray)
    NIRVA_RECYCLE(remval)
  } else {
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR_ARRAY, array, NIRVA_GET_ARRAY_BUNDLEPTR(bun, stname))
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR_ARRAY, xarray, NIRVA_MALLOC_ARRAY(NIRVA_BUNDLEPTR, nitems - 1))
    NIRVA_ASSIGN(j, 0)
    for (i = 0; i < nitems; i++) {
      if (array[i] == remval) continue;
      NIRVA_ASSIGN(xarray[j++], array[i])
    }
    NIRVA_ARRAY_FREE(array)
    NIRVA_ARRAY_CLEAR(bun, stname)
    // "container" is already set to bun, so we should have no problems here
    NIRVA_CALL(NIRVA_ARRAY_APPEND, bun, stname, STRAND_TYPE_CONST_BUNDLEPTR, nitems - 1, xarray)
    NIRVA_ARRAY_FREE(xarray)
  }
} else {
  NIRVA_ARRAY_CLEAR(bun, stname)
}
NIRVA_END_SUCCESS

NIRVA_EXPORTS(add_value_by_key)
NIRVA_EXPORTS(remove_value_by_key)
NIRVA_EXPORTS(has_value_for_key)
#endif
#ifdef NIRVA_MACRO_add_value_by_key
#define _NIRVA_MACRO_add_value_by_key(...)NIRVA_MACRO_add_value_by_key(__VA_ARGS__)
#else
#define _NIRVA_MACRO_add_value_by_key(...)NIRVA_RESULT_CALL_INVALID
#endif
#ifdef NIRVA_MACRO_remove_value_by_key
#define _NIRVA_MACRO_remove_value_by_key(...)NIRVA_MACRO_remove_value_by_key(__VA_ARGS__)
#else
#define _NIRVA_MACRO_remove_value_by_key(...)NIRVA_RESULT_CALL_INVALID
#endif
#ifdef NIRVA_MACRO_has_value_for_key
#define _NIRVA_MACRO_has_value_for_key(...)NIRVA_MACRO_has_value_for_key(__VA_ARGS__)
#else
#define _NIRVA_MACRO_has_value_for_key(...)NIRVA_FALSE
#endif

#define NIRVA_ATTR_DEF_IS_SCALAR(adef)					\
  (!adef || !NIRVA_BUNDLE_HAS_STRAND(adef, "MAX_VALUES") || NIRVA_GET_VALUE_INT(adef, "MAX_VALUES") == 0)

#define NIRVA_ATTR_DEF_IS_ARRAY(adef) !NIRVA_ATTR_DEF_IS_SCALAR(adef)

#define NIRVA_MACRO_get_attr_by_name(attr_group, attr_name)	\
  NIRVA_GET_VALUE_BY_KEY(attr_group, "ATTRIBUTES", attr_name)
#define NIRVA_ATTR_GET NIRVA_MACRO_get_attr_by_name

#define NIRVA_MACRO_remove_attr_by_name(attr_group, attr_name)	\
  NIRVA_REMOVE_VALUE_BY_KEY(attr_group, "ATTRIBUTES", attr_name)
#define NIRVA_ATTR_DELETE NIRVA_MACRO_remove_attr_by_name

#define NIRVA_MACRO_add_attr_by_name(attr_group, aname, attr)NIRVA_ADD_VALUE_BY_KEY(attr_group,"ATTRIBUTES", \
										    aname,STRAND_TYPE_BUNDLEPTR,attr)
#define NIRVA_ATTR_APPEND NIRVA_MACRO_add_attr_by_name

#define NIRVA_MACRO_get_attr_value_known_type(xtype, attr)NIRVA_GET_VALUE_##xtype(attr, "DATA")

#define NIRVA_MACRO_attr_get_type(attr)					\
  ((attr)?NIRVA_GET_VALUE_UINT(attr,"ATTR_TYPE"):ATTR_TYPE_NONE)
#define NIRVA_ATTR_GET_TYPE NIRVA_MACRO_attr_get_type

// check a single condition, e.g a == b

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, check_condition, NIRVA_INT64 op, NIRVA_INT n_op_args, NIRVA_STRING_ARRAY strings,
                        NIRVA_INT i, NIRVA_BUNDLEPTR fdata)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr)
NIRVA_DEF_SET(NIRVA_STRING, str0, strings[i + 1])
NIRVA_DEF_SET(NIRVA_STRING, str1, strings[i + 2])
// this gives us the from type - it could be a value as string
// or it could be attribute name,
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attrs, NIRVA_GET_VALUE_BUNDLEPTR(fdata, "SEG_ATTRS"))
char tcode0 = *str0, tcode1;
if (tcode0 == 'A') {
  uint32_t t0;
  char *attr_name = str0 + 1;
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
  NIRVA_ASSIGN(t0, NIRVA_ATTR_GET_TYPE(attr))
  if (t0 == ATTR_TYPE_BOOLEAN) {
    boolean p0 = 0;
    NIRVA_ASSIGN(p0, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
    if (op == COND_LOGIC_NOT) {
      NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
    }
    tcode1 = *str1;
    if (tcode1 == 'A') {
      uint32_t t1 = 0;
      char *attr_name = str1 + 1;
      NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
      NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr))
      if (t1 == ATTR_TYPE_BOOLEAN) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT64) {
        int64_t p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
        NIRVA_TEST_COND(op, p0, p1);
      }
      NIRVA_RETURN(NIRVA_COND_FAIL);
    }
    if (tcode1 == 'i' || tcode1 == 'b') {
      int p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    if (tcode1 == 'I') {
      int64_t p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  } else if (t0 == ATTR_TYPE_INT) {
    int p0 = 0;
    NIRVA_ASSIGN(p0, NIRVA_MACRO_get_attr_value_known_type(int, attr));
    if (op == COND_LOGIC_NOT) {
      NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
    }
    tcode1 = *str1;
    if (tcode1 == 'A') {
      uint32_t t1 = 0;
      char *attr_name = str1 + 1;
      NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
      NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr))
      if (t1 == ATTR_TYPE_BOOLEAN) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT64) {
        int64_t p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
        NIRVA_TEST_COND(op, p0, p1);
      }
      NIRVA_RETURN(NIRVA_COND_FAIL);
    }
    if (tcode1 == 'i' || tcode1 == 'b') {
      int p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    if (tcode1 == 'I') {
      int64_t p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  } else if (t0 == ATTR_TYPE_INT64) {
    int64_t p0 = 0;
    NIRVA_ASSIGN(p0, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
    if (op == COND_LOGIC_NOT) {
      NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
    }
    tcode1 = *str1;
    if (tcode1 == 'A') {
      uint32_t t1;
      char *attr_name = str1 + 1;
      NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
      NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr));
      if (t1 == ATTR_TYPE_BOOLEAN) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT) {
        int p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
        NIRVA_TEST_COND(op, p0, p1);
      } else if (t1 == ATTR_TYPE_INT64) {
        int64_t p1 = 0;
        NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
        NIRVA_TEST_COND(op, p0, p1);
      }
      NIRVA_RETURN(NIRVA_COND_FAIL);
    }
    if (tcode1 == 'i' || tcode1 == 'b') {
      int p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    if (tcode1 == 'I') {
      int64_t p1 = 0;
      NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  }
}
if (tcode0 == 'b') {
  int p0 = 0;
  NIRVA_CALL_ASSIGN(p0, atoi, str0 + 1);
  if (op == COND_LOGIC_NOT) {
    NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
  }
  tcode1 = *str1;
  if (tcode1 == 'A') {
    uint32_t t1;
    char *attr_name = str1 + 1;
    NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
    NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr));
    if (t1 == ATTR_TYPE_BOOLEAN) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT64) {
      int64_t p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  }
  if (tcode1 == 'i' || tcode1 == 'b') {
    int p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
  if (tcode1 == 'I') {
    int64_t p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
  NIRVA_RETURN(NIRVA_COND_FAIL);
} else if (tcode0 == 'i') {
  int p0 = 0;
  NIRVA_CALL_ASSIGN(p0, atoi, str0 + 1);
  if (op == COND_LOGIC_NOT) {
    NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
  }
  tcode1 = *str1;
  if (tcode1 == 'A') {
    uint32_t t1;
    char *attr_name = str1 + 1;
    NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
    NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr));
    if (t1 == ATTR_TYPE_BOOLEAN) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT64) {
      int64_t p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  }
  if (tcode1 == 'i' || tcode1 == 'b') {
    int p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
  if (tcode1 == 'I') {
    int64_t p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
  NIRVA_RETURN(NIRVA_COND_FAIL);
} else if (tcode0 == 'I') {
  int64_t p0 = 0;
  NIRVA_CALL_ASSIGN(p0, atol, str0 + 1);
  if (op == COND_LOGIC_NOT) {
    NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_NOT(p0)));
  }
  tcode1 = *str1;
  if (tcode1 == 'A') {
    uint32_t t1;
    char *attr_name = str1 + 1;
    NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, attr_name))
    NIRVA_ASSIGN(t1, NIRVA_ATTR_GET_TYPE(attr));
    if (t1 == ATTR_TYPE_BOOLEAN) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(boolean, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT) {
      int p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int, attr));
      NIRVA_TEST_COND(op, p0, p1);
    } else if (t1 == ATTR_TYPE_INT64) {
      int64_t p1 = 0;
      NIRVA_ASSIGN(p1, NIRVA_MACRO_get_attr_value_known_type(int64, attr));
      NIRVA_TEST_COND(op, p0, p1);
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  }
  if (tcode1 == 'i' || tcode1 == 'b') {
    int p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atoi, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
  if (tcode1 == 'I') {
    int64_t p1 = 0;
    NIRVA_CALL_ASSIGN(p1, atol, str1 + 1);
    NIRVA_TEST_COND(op, p0, p1);
  }
}
NIRVA_END_RETURN(NIRVA_COND_FAIL);

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, update_gcondres, NIRVA_INT logic_op,
                        NIRVA_INT64 gcondres,  NIRVA_INT64 condres)
if (condres == NIRVA_COND_FORCE || condres == NIRVA_COND_ABANDON) NIRVA_RETURN(condres);
if (gcondres != NIRVA_COND_INVALID && gcondres != NIRVA_COND_SUCCESS
    && gcondres != NIRVA_COND_FAIL) NIRVA_RETURN(gcondres);

if (condres == NIRVA_COND_FAIL) {
  // COND_FAIL
  if (logic_op == COND_LOGIC_OR || logic_op == COND_LOGIC_XOR)
    return gcondres;
} else {
  // COND_SUCCESS
  if (logic_op == COND_LOGIC_XOR && gcondres == NIRVA_COND_SUCCESS) {
    return NIRVA_COND_FAIL;
  }
}
NIRVA_END_RETURN(condres);

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, push_int, NIRVA_BUNDLEPTR stack, NIRVA_INT val)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, node, old_end)
if (!stack) NIRVA_CALL_ASSIGN(stack, create_bundle_by_type, VALUE_BUNDLE_TYPE, NIRVA_NULL);
NIRVA_CALL_ASSIGN(node, create_bundle_by_type, VALUE_BUNDLE_TYPE, "STRAND_TYPE", STRAND_TYPE_INT, NIRVA_NULL)
NIRVA_VALUE_SET(node, "DATA", STRAND_TYPE_INT, val)
NIRVA_ASSIGN(old_end, NIRVA_GET_VALUE_BUNDLEPTR(stack, "PREV"))
if (old_end) {
  NIRVA_VALUE_SET(node, "PREV", STRAND_TYPE_CONST_BUNDLEPTR, old_end)
  NIRVA_VALUE_SET(old_end, "NEXT", STRAND_TYPE_BUNDLEPTR, node)
} else {
  NIRVA_VALUE_SET(stack, "NEXT", STRAND_TYPE_BUNDLEPTR, node)
}
NIRVA_VALUE_SET(stack, "PREV", STRAND_TYPE_CONST_BUNDLEPTR, node)
NIRVA_END_RETURN(stack)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_INT, pop_int, NIRVA_BUNDLEPTR stack)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, old_end, NIRVA_GET_VALUE_BUNDLEPTR(stack, "PREV"))
NIRVA_DEF_SET(NIRVA_INT, val, NIRVA_GET_VALUE_INT(old_end, "DATA"))
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, old_prev, NIRVA_GET_VALUE_BUNDLEPTR(old_end, "PREV"))
if (old_prev) {
  NIRVA_VALUE_SET(stack, "PREV", STRAND_TYPE_CONST_BUNDLEPTR, old_prev)
  NIRVA_STRAND_DELETE(old_prev, "NEXT")
} else {
  NIRVA_STRAND_DELETE(stack, "NEXT")
  NIRVA_STRAND_DELETE(stack, "PREV")
}
NIRVA_END_RETURN(val)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_INT, pop_parens, NIRVA_BUNDLEPTR stack, NIRVA_INT64 gcondres)
NIRVA_DEF_VARS(NIRVA_INT, notval, logic_op, condres)
NIRVA_ASSIGN(notval, 0)
NIRVA_CALL_ASSIGN(logic_op, NIRVA_INTERNAL(pop_int), stack)
if (logic_op == COND_LOGIC_NOT) {
  NIRVA_ASSIGN(notval, 1)
  NIRVA_CALL_ASSIGN(logic_op, NIRVA_INTERNAL(pop_int), stack)
}
if (logic_op != COND_NOOP) NIRVA_CALL_ASSIGN(condres, NIRVA_INTERNAL(pop_int), stack)
  if (notval) {
    if (gcondres == NIRVA_COND_FAIL) NIRVA_ASSIGN(gcondres, NIRVA_COND_SUCCESS)
      else if (gcondres == NIRVA_COND_SUCCESS) NIRVA_ASSIGN(gcondres, NIRVA_COND_FAIL)
        NIRVA_ASSIGN(notval, 0)
      }
NIRVA_END_RESULT(NIRVA_INTERNAL(update_gcondres), logic_op, gcondres, condres)

// Scripts: these are an array of string. We read 0 or more NOT
// NOT (0 or more) ( op, aval, bval) logic_op

// each sting should exactly 1 symbol, for example:

// s[0] : COND_START			: "COND_START"
// s[1] : COND_VAL_EQUALS		: "18"
// s[2] : VAR_

// run through conditions in scriptlet strings (array pf const string)
// strins

#define N_SYNTAX_SYMS 5
#define NIRVA_CALL_METHOD_check_cond_script INTERN
// almost STANDARD FUNC
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, check_cond_script, NIRVA_BUNDLEPTR scriptlet, NIRVA_BUNDLEPTR func_data)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, pstack, NIRVA_NULL)
NIRVA_DEF_VARS(NIRVA_INT64, condres, gcondres)
NIRVA_DEF_VARS(NIRVA_INT, plevel, notval, j, k)
NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, strings)
NIRVA_DEF_VARS(NIRVA_INT, logic_op, value_op, syn_start, syn_end, n_op_args)
NIRVA_DEF_ARRAY_SIZE(NIRVA_STRING, syntax, N_SYNTAX_SYMS)
NIRVA_ASSIGN(syntax[0], COND_START)
NIRVA_ASSIGN(syntax[1], COND_P_CLOSE)
NIRVA_ASSIGN(syntax[2], COND_END)
NIRVA_ASSIGN(syntax[3], COND_NOT)
NIRVA_ASSIGN(syntax[4], COND_P_OPEN)
NIRVA_ASSIGN(gcondres, NIRVA_COND_INVALID)
NIRVA_ASSIGN(plevel, 0)
NIRVA_ASSIGN(notval, 0)
NIRVA_ASSIGN(strings, NIRVA_GET_ARRAY_STRING(scriptlet, "STRINGS"))
if (!strings) NIRVA_RETURN(NIRVA_COND_INVALID);

// TODO - get syntax from a cascade
NIRVA_ASSIGN(logic_op, COND_NOOP)
NIRVA_ASSIGN(syn_start, 0)
NIRVA_ASSIGN(syn_end, N_SYNTAX_SYMS)
for (int i = 0; strings[i]; i++) {
  for (j = syn_start; j < syn_end; j++) {
    if (NIRVA_STRLEN_EQUAL(strings[i], syntax[j])) break;
  }
  if (!i) {
    // MUST have exactly 1 COND_START to begin
    if (j != 0) {
      NIRVA_ERROR(NULL);
      NIRVA_RETURN(NIRVA_COND_INVALID)
    }
    i++;
  } else {
    if (j == 3) {
      // NOT
      if (++notval == 2) {
        for (k = i - 1; strings[k + 2]; k++) strings[k] = strings[k + 2];
        i -= 2;
        notval = 0;
      }
      continue;
    } else if (j != syn_end) {
      NIRVA_ERROR(NIRVA_NULL);
      NIRVA_RETURN(NIRVA_COND_INVALID)
    }

    // next we have value op
    value_op = atoi(strings[i]);
    n_op_args = N_OP_ARGS(value_op);
    if (n_op_args == COND_NARGS_INVALID) {
      NIRVA_ERROR(NIRVA_NULL);
      NIRVA_RETURN(NIRVA_COND_INVALID)
    }
    i++;

    NIRVA_CALL_ASSIGN(condres, NIRVA_INTERNAL(check_condition), value_op, n_op_args, strings, i, func_data);

    NIRVA_CALL(NIRVA_ARRAY_APPEND, func_data, "RESPONSES",
               _NIRVA_STRAND_GET_TYPE(func_data, "RESPONSES"), 1, &condres)
    NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_FORCE, NIRVA_RETURN, gcondres)
    NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_ABANDON, NIRVA_RETURN, gcondres)
    i += n_op_args;
    // after evaluating, we apply
    // not (if appropriate)
    if (notval) {
      if (condres == NIRVA_COND_FAIL) {condres = NIRVA_COND_SUCCESS;}
      else if (gcondres == NIRVA_COND_SUCCESS) gcondres = NIRVA_COND_FAIL;
      notval = 0;
    }

    // we then update gcondress depending on logic_op
    NIRVA_CALL_ASSIGN(gcondres,  NIRVA_INTERNAL(update_gcondres), logic_op, gcondres, condres)

    // next we can have - parens close (up to plevel times), and / or COND_END
    do {
      for (j = syn_start; j < syn_end; j++) {
        if (NIRVA_STRLEN_EQUAL(strings[i], syntax[j])) break;
      }
      if (j == 2) {
        // COND_END
        while (plevel--) {
          // strictly speaking this is a syntax error, but we will close parens
          NIRVA_CALL_ASSIGN(gcondres, NIRVA_INTERNAL(pop_parens), pstack, gcondres);
        }
        return gcondres;
      }
      if (j == 1) {
        // CLOSE PARENS
        if (--plevel < 0) {
          NIRVA_ERROR(NULL);
          NIRVA_RETURN(NIRVA_COND_INVALID)
        }
        i++;
        plevel--;
        NIRVA_CALL_ASSIGN(gcondres, NIRVA_INTERNAL(pop_parens), pstack, gcondres);
      } else if (j != syn_end) {
        NIRVA_ERROR(NULL);
        NIRVA_RETURN(NIRVA_COND_INVALID)
      }
    } while (1);

    // NEXT LOGIC OP
    logic_op = atoi(strings[i]);
    notval = 0;
  } while (1);

  // xi lets us peek ahead to see if we have [NOT] P_OPEN
  // otherwuse we have reached the next condition
  for (int xi = i + 1; strings[xi]; xi++) {
    for (j = syn_start; j < syn_end; j++) {
      if (NIRVA_STRLEN_EQUAL(strings[xi], syntax[j])) break;
    }
    if (j == 3) {
      // NOT
      if (++notval == 2) {
        for (k = xi - 1; strings[k + 2]; k++) strings[k] = strings[k + 2];
        xi -= 2;
        notval = 0;
      }
      continue;
    }
    if (j == 4) {
      // parens open
      plevel++;
      i = xi;
      if (logic_op != COND_NOOP) {
        NIRVA_CALL_ASSIGN(pstack, NIRVA_INTERNAL(push_int), pstack, gcondres);
        NIRVA_CALL(NIRVA_INTERNAL(push_int), pstack, logic_op);
        if (notval) NIRVA_CALL(NIRVA_INTERNAL(push_int), pstack, COND_LOGIC_NOT);
        logic_op = COND_NOOP;
        notval = 0;
      } else NIRVA_CALL(NIRVA_INTERNAL(push_int), pstack, logic_op);
      continue;
    }
    break;
  }
  notval = 0;
}
// we reached the end with no COND_END
// this is a syntax error, but we can skip it
NIRVA_END_RETURN(gcondres)

// run a cascade node, then depending on result, go to next node
// STANDARD FUNC
#define NIRVA_CALL_METHOD_nirva_condcheck INTERN
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, nirva_condcheck, NIRVA_BUNDLEPTR conds,
                        NIRVA_BUNDLEPTR  func_data)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, condval)
NIRVA_DEF_VARS(NIRVA_UINT64, gcondres)
//NIRVA_DEF_VARS(NIRVA_INT, logic_macro = COND_LOGIC_AND)
NIRVA_ASSIGN(gcondres, NIRVA_COND_SUCCESS)
do {
  NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, val)
  NIRVA_ASSIGN(val, NIRVA_GET_VALUE_BUNDLEPTR(condval, "VALUE"))
  if (val) {
    NIRVA_VALUE_SET(func_data, "VALUE", STRAND_TYPE_CONST_BUNDLEPTR, val)
    NIRVA_RETURN(gcondres)
  }
} while (0);
NIRVA_FUNC_END

// once a transform has been assigned a trajectory, this function can be called
// to create the func_data in the transform
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, create_func_data, NIRVA_BUNDLEPTR transform)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, fdata)
NIRVA_ASSERT(transform, NIRVA_FAIL, NIRVA_NULL)
NIRVA_CALL_ASSIGN(fdata, create_bundle_by_type, FUNC_DATA_BUNDLE_TYPE, "TRANSFORM", transform, NIRVA_NULL)
NIRVA_VALUE_SET(transform, "FUNC_DATA", STRAND_TYPE_BUNDLEPTR, fdata)
NIRVA_END_SUCCESS

// cascades are always accompanied by a transform. All of the strands in the transform are avaialable for perusal
// generally including - emission data (source / target), contract, flags, caps, attributes
// for an 'active' transform, we would also have a func_data bundle,
// with its seg_attrs, transform status,  and for helper functions, return_value
// and at the end of a transform action, TX_RESULT
// get first condlogic_node value
// action it, it will follow nodes and return a COND value or if
// STANDARD FUNC
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, cascade, NIRVA_BUNDLEPTR cascade, NIRVA_BUNDLEPTR transform, NIRVA_GENPTR retloc)
/*   NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr, cascade) */
/*   NIRVA_DEF_VARS(NIRVA_STRING, procfunc) */
/*   NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, retval) */

/*   NIRVA_ASSIGN(cascade, NIRVA_GET_VALUE_BUNDLEPTR(func_data, "CASCADE")) */
/*   NIRVA__ASSIGN(procfunc, NIRVA_GET_VALUE_STRING(cascade, "PROCESSOR")) */

/*   if (!procfunc) { */
/*     NIRVA_DEF_VARS(NIRVA_BUNDLEPTR func_casc) */
/*       NIRVA_DEF_VARS(NIRVA_INT, cat) */
/*       NIRVA_ASSIGN(cat, NIRVA_GET_VALUE_INT(cascade, "CATEGORY")) */
/*       NIRVA_ASSIGN(func_casc, NIRVA_GET_VALUE_INT(cascade, "FUNC_CHOOSER")) */
/*       NIRVA_ASSIGN(procfunc, NIRVA_ACTION_RET_STRING, nirva_cascade, func_casc)) */
/*       } */

/* NIRVA_CALL_ASSIGN(retval, NIRVA_ACTION_RET, procfunc, func_data)  */
/* // call the processing func in the cascade, return the value (only valid if numeric) */
/* // otherwise results are in func_data */
/*   NIRVA_STRING_FREE(procfunc) */
/* // */
//NIRVA_END_RETURN(retval)
NIRVA_END_SUCCESS

#define NIRVA_CALL_METHOD_declare_oracle INTERNAL
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, nirva_declare_oracle, NIRVA_INT prio, NIRVA_VARIADIC)
if (NIRVA_FUNC_GENERATION < 3) {
  // if generation is < 3, we will store in storage bundle
  // we will make a strand of type bundleptr array
  // this will be an array of strand_replacement - func, category, cond, prio and pmap
  // prio, categ will be int, cond string array, func, funcptr,
  // and pmap array of pmap_def
  // try to put the most relevant info first, in this case the funcptr and category
  //NIRVA_STATIC_VARS(NIRVA_INT, NIRVA_SET(count, 0))
  /* NIRVA_STRING_BUFF(buff, 512) */
  /* NIRVA_DEF_VARS(nirva_bundle_t, *st0, *pmap, *sts[5]) */
  /* NIRVA_DEF_VARS(NIRVA_INT, np, i) */
  /* NIRVA_DEF_VARS(NIRVA_STRING, str) */
  /* NIRVA_DEF_VARS(NIRVA_UINT, rt, pt) */
  /* NIRVA_DEF_VARS(nirva_native_function_t, func) */
  /* NIRVA_DEF_VARS(NIRVA_VA_LIST, va) */
  /* NIRVA_ASSERT(_TMP_STORAGE_BUNDLE,NIRVA_CALL,_make_tmp_store,NIRVA_NIRVA_NULL) */
  /* NIRVA_ASSERT(_TMP_STORAGE_BUNDLE,NIRVA_FAIL,NIRVA_NIRVA_NULL) */
  /* NIRVA_FMT(buff, 512, NIRVA_FMT_STRING  NIRVA_FMT_INT3, "ORAC_", */
  /* 		NIRVA_POST_INC(count)) */
  /* NIRVA_CALL_ASSIGN(st0, create_bundle_by_type, STRAND_REPLACEMENT_BUNDLE_TYPE, "NAME", buff, */
  /* 			"STRAND_TYPE",STRAND_TYPE_BUNDLEPTR, "ARRAY_SIZE", 5, NIRVA_NULL) */
  /* NIRVA_VA_START(va, prio) */

  /* NIRVA_CALL_ASSIGN(sts[1], create_bundle_by_type, */
  /* 			STRAND_REPLACEMENT_BUNDLE_TYPE,"NAME","category", */
  /* 			"STRAND_TYPE", STRAND_TYPE_INT, */
  /* 			"DATA", FUNC_CATEGORY_ORACLE, NIRVA_NULL) */

  /* NIRVA_CALL_ASSIGN(sts[2], create_bundle_by_type, */
  /* 			STRAND_REPLACEMENT_BUNDLE_TYPE,"NAME", "conditions" */
  /* 			"STRAND_TYPE", STRAND_TYPE_STRING, */
  /* 			"ARRAY_SIZE", -1, NIRVA_NULL) */

  /* NIRVA_LOOP_WHILE(NIRVA_ASSIGN_RET(str,NIRVA_VA_ARG(va, NIRVA_STRING))) */
  /* NIRVA_ARRAY_SET(sts[2], "DATA", STRAND_TYPE_STRING, 1, str) */
  /* NIRVA_BREAK_IF(NIRVA_STRING_EQUAL,str,COND_END) */
  /* NIRVA_LOOP_END */

  /* NIRVA_ASSIGN(rt, NIRVA_VA_ARG(va, NIRVA_UINT)) */
  /* NIRVA_ASSIGN(func, NIRVA_VA_ARG(va, nirva_native_function_t)) */

  /* NIRVA_CALL_ASSIGN(sts[0], create_bundle_by_type, */
  /* 			STRAND_REPLACEMENT_BUNDLE_TYPE,"NAME","function" */
  /* 			"STRAND_TYPE", STRAND_TYPE_FUNCPTR, "DATA", func, NIRVA_NULL) */

  /* NIRVA_CALL_ASSIGN(sts[3], create_bundle_by_type, */
  /* 			STRAND_REPLACEMENT_BUNDLE_TYPE,"NAME","priority" */
  /* 			"STRAND_TYPE", STRAND_TYPE_INT, "DATA", prio, NIRVA_NULL) */

  /* NIRVA_CALL_ASSIGN(sts[4], create_bundle_by_type, */
  /* 			STRAND_REPLACEMENT_BUNDLE_TYPE,"NAME","pmappings" */
  /* 			"STRAND_TYPE", STRAND_TYPE_BUNDLEPTR, */
  /* 			"ARRAY_SIZE", -1, NIRVA_NULL) */

  /* NIRVA_CALL_ASSIGN(pmap, create_bundle_by_type, */
  /* 			PMAP_BUNDLE_TYPE, "ATTR_TYPE", rt, */
  /* 			"PARAM_NUM", 0, NIRVA_NULL) */

  /* NIRVA_ARRAY_SET(sts[4], "DATA", STRAND_TYPE_BUNDLEPTR, 1, pmap) */
  /* NIRVA_LOOP_WHILE(NIRVA_TEST_GT(NIRVA_POST_DEC(np), 0)) */
  /* NIRVA_ASSIGN(pt, NIRVA_VA_ARG(va, NIRVA_UINT)) */
  /* NIRVA_CALL_ASSIGN(pmap, create_bundle_by_type, */
  /* 			PMAP_BUNDLE_TYPE,"ATTR_TYPE", pt, */
  /* 			"PARAM_NUM", NIRVA_PRE_INC(i), NIRVA_NULL) */
  /* NIRVA_ARRAY_SET(st5, "DATA", STRAND_TYPE_BUNDLEPTR, 1, pmap) */
  /* NIRVA_LOOP_END */
  /* NIRVA_VA_END(va) */

  /* NIRVA_ARRAY_SET(st0, "DATA", STRAND_TYPE_BUNDLEPTR, 5, sts) */
  /* NIRVA_ARRAY_APPEND(_TMP_STORAGE_BUNDLE, BUNDLE_STANDARD_STORAGE, STRAND_TYPE_BUNDLEPTR, 1, &stdef) */
  /* } */

  // so first we create the object_template itself
  // we assume that all bundledefs have already been set up, we can just create this from the
  // bundledef

}
NIRVA_END_SUCCESS;

// we can set these in attr_def (templates)
// or in attributes (runtime)
// in attr_def, the we dont have a value, we can only set the default.
// we will be passed an attr_des with defaul type bundleptr,  ptr to bundle type func_desc
// so what we actually want to do is set values inside the func_def.
// there we find name, uid, category, pmap array, and function
// name we set to th function nama, and point function at it. the uid will be set already
// category depends on fn type but for native impl funcs it would be:
// FUNC_PURPOSE_IMPL | FUNC_CATEGORY_NATIVE
// for structure funcs, these are standard funcs, wrapped as native and the category value is
// FUNC_PURPOSE_STRUCTURAL | FUNC_CATEGORY_STANDARD | FUNC_WRAPPER_NATIVE
// having set these data, finally we need to set pmap strands, for impl_funcs this is a forward
// mapping -> attributes to func params, for strucutral, it is a reverse mapping, func params
// -> attributes. So we just call make_pmap(-1, ret_type, 0, p0_type, ... NULL)
// the types are ATTR_TYPE, and the returned pmap is the set in attr_desc..
// changing the value in attribute is similar but generally we would only alter the defaut on
// attr desc, since changing a function while in use is risky.
//

/* #define NIRVA_SET_IMPLFUNC(substruct, name)			\ */
/*   NIRVA_IMPL_FUNC(set_attr_funcptr, substruct,			\ */
/* 		 ATTR_NAME(NIRVA_IMPL_FUNC, #name), func) */

/* #define NIRVA_DEF_STRUCTURE_FUNC(substruct)			\ */
/*   NIRVA_IMPL_FUNC(set_attr_funcptr, substruct,			\ */
/* 		 ATTR_NAME(NIRVA_STRUCT_FUNC, #name), NULL) */

/* #define NIRVA_SET_STRUCTURE_FUNC(substruct, name)		\ */
/*   NIRVA_IMPL_FUNC(set_attr_funcptr, substruct,			\ */
/* 		 ATTR_NAME(NIRVA_STRUCT_FUNC, #name), name) */

////////////////////////////////////////////////////////////////
// during bootstrap, init will first create sufficient infrastructure to support creating
// attr_desc and func_desc, this wil be done using the #define values
// once this is done,
// prime will search for Oracles to define the inital values. This function will present itself as
// a proxy oracle and if there are no higher priotity Oracles, the processng will come here.
// the values of any already used functions will be set as defaults, if we find any fivergence we
// need to flag an warning.
// - pass in an attr_desc_bundle and here this will be filled with all
// impl funcs, madatory and optional, as well as the structure transforms
// if any funciions which were previously used had been redefined, the old infrastructure will
// be torn down using the old functions, leaving only the functions attr_desc_bundle
// this will be used to re-bootstrap the system until it can once again build attr_desc bundles
// the process will be repeated, this time any changes will trigger a fatal error.
// finally the structure_app instance

// here we take a attr_def_group and return an attr
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, create_attr_from_def, NIRVA_BUNDLEPTR atdef)
NIRVA_DEF_SET(NIRVA_UINT64, flags, NIRVA_GET_VALUE_UINT64(atdef, "FLAGS"))
NIRVA_DEF_SET(NIRVA_STRING, aname, NIRVA_GET_VALUE_STRING(atdef, "NAME"))
NIRVA_DEF_SET(NIRVA_UINT, attr_type, NIRVA_ATTR_GET_TYPE(atdef))
NIRVA_DEF_ASSIGN(NIRVA_UINT, sttype, NIRVA_GET_VALUE_UINT, atdef, "STRAND_TYPE")
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, attr, create_bundle_by_type, ATTRIBUTE_BUNDLE_TYPE,
                 "TEMPLATE", atdef, "FLAGS", flags, "NAME", aname, "ATTR_TYPE", attr_type,
                 "STRAND_TYPE", sttype, NIRVA_NULL)
if (NIRVA_BUNDLE_HAS_STRAND(atdef, "RESTRCITIONS")) {
  NIRVA_DEF_ASSIGN(NIRVA_STRING, rests, NIRVA_GET_VALUE_STRING, atdef, "RESTRICTIONS")
  NIRVA_VALUE_SET(attr, "RESTRICTIONS", STRAND_TYPE_STRING, rests)
  NIRVA_STRING_FREE(rests)
}
// max_size ?
if (!(flags &NIRVA_ATTR_FLAG_OPTIONAL)) {
  if (NIRVA_BUNDLE_HAS_STRAND(atdef, "DEFAULT")) {
    NIRVA_STRAND_COPY(attr, "DATA", atdef, "DEFAULT")
  }
}
NIRVA_STRING_FREE(aname)
NIRVA_END_RETURN(attr)

// create a CONTRACT bundle with default values
// variables are: owner - ptr to object "owning" the contract, and trajectory
// FLAGS will be set to TX_FLAG_NO_NEGOTIATE
// INTENT set to NONE, CAPS to NULL,
// a simple CASCADE will be created and included,
// with DEFAULT set to trajectory, and category to FUNC_CATEGORY_WRAPPER
// (n.b the trajectory "wrapper" can narrow the category, e.g FUNC_CATEGORY_STRUCTURAL
// CONDLOGIC, CURRENT_NODE will be set to NULL, and other strands (e.g PROC_FUNC) not set.
// this is a temporay hack, we pass trajectory (1) directly, which is set in VALUE and added as default to cascade
// really we should make the cascade, inserting one or more trajectories, and constions to select one
// we also, for now dont set intent or caps
#define NIRVA_CALL_METHOD_nirva_create_contract INTERN
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, nirva_create_contract, NIRVA_BUNDLEPTR owner, NIRVA_UINT flags,
                        NIRVA_INT intent, NIRVA_VARIADIC)
//NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, caps, nirva_create_bundle_caps)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, caps, NIRVA_NULL)
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, ctract, create_bundle_by_type, CONTRACT_BUNDLE_TYPE, "FLAGS", flags, "INTENT", intent,
                 "CAPS", caps, "OWNER", owner, NIRVA_NULL)
NIRVA_END_RETURN(ctract)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, contract_add_trajectory, NIRVA_BUNDLEPTR ctract, NIRVA_BUNDLEPTR traj,
                        NIRVA_CONST_STRING caps_cond)
NIRVA_END_SUCCESS

// this function can be called for any bundle which extends, or is, VALUE bundle.
// if the caller is the "owner" of the bundle (attr_group owner, transform "source_bundle")
// then reparenting the bundle is permitted. this function causes the bundles to be included by newp
// if newp is NIRVA_NULL, then they will be reparented to structure_app, if that exists
// so they will not be unreffed until the app exits
// if structure_app has not yet been created, the container will be set to self (e.g substructure bundles,
// blueprint bundles)
//
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, bundle_reparent, NIRVA_BUNDLEPTR self, NIRVA_BUNDLEPTR bun,
                        NIRVA_CONST_STRING stname, NIRVA_BUNDLEPTR newp, NIRVA_CONST_STRING newname, NIRVA_BOOLEAN append)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, container)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_DEF_VARS(NIRVA_UINT, nitems)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, subs)
if (newp != NIRVA_NULL) {
  NIRVA_ASSERT(self, NIRVA_RETURN, NIRVA_RESULT_ERROR)
}
NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESULT_ERROR)
/* // TODO - check owner is caller, check container of bun - for attr this will be attr_group, */
/* // for return_values, it will be transform, for strand_repl, it will be bundle */

/* if (NIRVA_GET_OWNER(bun) != self) { */
/*   // check priveleges */

/*  } */

/* if (newp && NIRVA_GET_OWNER(newp) != self) { */
/*   // check priveleges */

/*  } */

NIRVA_ASSIGN(nitems, NIRVA_ARRAY_GET_SIZE(bun, stname))
NIRVA_ASSERT(nitems, NIRVA_RETURN, NIRVA_RESULT_FAIL)
NIRVA_ASSIGN(subs, NIRVA_GET_ARRAY_BUNDLEPTR(bun, stname))
if (newp) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, stdef, NIRVA_INTERNAL(check_can_add)(newp, newname,
                STRAND_TYPE_BUNDLEPTR, subs[0]))
  if (!stdef) {
    NIRVA_ARRAY_FREE(subs)
    NIRVA_RETURN_FAIL
  }
}
NIRVA_ASSIGN(container, NIRVA_GET_VALUE_BUNDLEPTR(subs[0], "CONTAINER"))
if (container != bun && (container != subs[0] || STRUCTURE_APP != NULL))  {
  NIRVA_ARRAY_FREE(subs)
  NIRVA_RETURN_FAIL
}

if (newp == NIRVA_NULL) {
  for (i = 0; i < nitems; i++) {
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR, sub, subs[i])
    NIRVA_STRAND_DELETE(sub, "CONTAINER")
    NIRVA_STRAND_DELETE(sub, "CONTAINER_STRAND")
    if (STRUCTURE_APP) {
      NIRVA_CALL(NIRVA_ARRAY_APPEND, STRUCTURE_APP, "STATIC_BUNDLES", STRAND_TYPE_BUNDLEPTR, nitems, subs)
    } else {
      NIRVA_VALUE_SET(sub, "CONTAINER", STRAND_TYPE_CONST_BUNDLEPTR, sub)
    }
  }
} else {
  if (append) {
    NIRVA_DEF_SET(NIRVA_UINT, ns, NIRVA_ARRAY_GET_SIZE(newp, newname))
    if (ns > 0) {
      NIRVA_DEF_VARS(NIRVA_STRING, constr)
      NIRVA_DEF_SET(NIRVA_BUNDLEPTR, xist, NIRVA_GET_VALUE_BUNDLEPTR(newp, newname))
      NIRVA_DEF_SET(NIRVA_BUNDLEPTR, container, NIRVA_GET_VALUE_BUNDLEPTR(xist, "CONTAINER"))
      if (container != newp) {
        NIRVA_ARRAY_FREE(subs)
        NIRVA_RETURN_FAIL
      }
      NIRVA_ASSIGN(constr, NIRVA_GET_VALUE_STRING(xist, "CONTAINER_STRAND"))
      if (!NIRVA_STRING_EQUAL(constr, newname)) {
        NIRVA_STRING_FREE(constr)
        NIRVA_ARRAY_FREE(subs)
        NIRVA_RETURN_FAIL
      }
      NIRVA_STRING_FREE(constr)
    }
    NIRVA_CALL(NIRVA_ARRAY_APPEND, newp, newname, STRAND_TYPE_BUNDLEPTR, nitems, subs)
  } else {
    NIRVA_ARRAY_SET(newp, newname, STRAND_TYPE_BUNDLEPTR, nitems, subs)
  }
}
_NIRVA_STRAND_DELETE(bun, stname)
NIRVA_ARRAY_SET(bun, stname, STRAND_TYPE_CONST_BUNDLEPTR, nitems, subs)
NIRVA_ARRAY_FREE(subs)
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, bundle_make_static, NIRVA_BUNDLEPTR self, NIRVA_BUNDLEPTR bun,
                        NIRVA_CONST_STRING stname)
NIRVA_END_RESULT(NIRVA_INTERNAL(bundle_reparent), self, bun, stname, NIRVA_NULL, NIRVA_NULL, NIRVA_TRUE)

// for developers building contracts, the sequence is this:
// - create the barebones trajectory N?IRVA_CREATE_TRAJECTORY
// - create one or more segments - NIRVA_CREATE_SEGMENT
// - add attr_defs for the first functional, inputs will be segment inpts, outputs may be segment outputs
// or internal
// create a functional, wrapping a native, standar func, or script
// append the functional to the segment
// set pointers for in attrs for the functional
// set pointers for out attrs for the functional
// if desired, more functionals can be added to the segment. This is necessary if one functional produces
// internal attributes which are inputs to a later functional
// once all functionals have been added, the segment can be appended to the trajectory. If the trajectory
// already holds other segments, then conditions should be created for
// traversing from a previous segment to this one. There can be paths from multiple previous segments.
// if desired, a condition can be added for traversing from this segment to a previous one
// after all segments have been added, append SEGMENT_END
// it is not necessary to add conditions to traverse to segment end, as this is the default if no other conditions
// succeed
// once all segments are added to the trajectory, the trajectory must be vaslidated
// input attrbutes to all segments will be collated and added to trajectory inputs. If there are branching paths
// and only some require the attributes, then they will be marked as optional, and conditions added has_attribute
// if the in attributes are in / out in a segment, they will be flagged in / out in the trajectory
//
// any attributes may be selected as trajectory outputs. if optional inputs are selected,
// the corresponding output will also be optional If the segment producing an output attribute
// has condtions then all conditions along the pathe will be collated (AND vertically, OR horizontally)
// and it will be marekd optional
// attributes marked as internal cannot be outputs as they are destroyed once the segment has completed
// multiple segments can produce the same outputs, provide they are on exclusive paths
// a segment attemping to output an already existing attribute will trigger an error condition.
//
// At a minimum, the trajectory MUST produce the output attributes required to satisfy the INTENTCAP it purports
// these are defined in the documentation
//
NIRVA_DEF_FUNC_INTERNAL(NIRVA_UINT, count_attr_maps, NIRVA_UINT nmaps,
                        NIRVA_BUNDLEPTR_ARRAY attr_maps, NIRVA_UINT64 xmapping)
NIRVA_DEF_SET(NIRVA_UINT, count, 0)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if ((mapping &xmapping) == xmapping) count++;
NIRVA_LOOP_END
NIRVA_END_RETURN(count)

// create a trajectory. We can set FLAGS and CATEGORY
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, create_trajectory, NIRVA_UINT64 flags, NIRVA_INT category)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, traj)
NIRVA_CALL_ASSIGN(traj, create_bundle_by_type, TRAJECTORY_BUNDLE_TYPE, "FLAGS", flags, "CATEGORY", category,
                  NIRVA_NULL)
NIRVA_END_RETURN(traj)

#define NIRVA_CALL_METHOD_nirva_add_wrapping INTERN
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, nirva_add_wrapping, NIRVA_BUNDLEPTR thing,
                        NIRVA_BUNDLEPTR func_type, NIRVA_INT num_pmap_attrs, NIRVA_BUNDLEPTR_ARRAY pmap_attrs,
                        NIRVA_INT num_pmaps_out, NIRVA_INT_ARRAY pmaps_out)
NIRVA_END_SUCCESS

// after building a "barebones" trajectory, we can then proceed to add segements to it
// segments must first be created from functionals
// once this is done, we need to call validate_trajectory, which will amalgamate all the segments
// and create attributes as inputs and internals
// follwoing this it is necessary to declare the output attributes for the trajectory,
// these MUST be already included either in inputs or in internal, otherwise they will be added as input attributes
// if they are in inputs, they become in / out attributes, if the input or internal is optional,
// they become optional outputs

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, nirva_trajectory_add_segment, NIRVA_BUNDLEPTR traj,
                        NIRVA_BUNDLEPTR seg,  NIRVA_CONST_STRING cond)
//NIRVA_BUNDLPTR_ARRAY segments, NIRVA_BUNDLEPTR seg_selector)
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, nirva_create_attr_def, NIRVA_CONST_STRING aname, NIRVA_UINT64 flags,
                        NIRVA_UINT attr_type, NIRVA_INT max_values, NIRVA_BUNDLEPTR restrictions)
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, atdef, create_bundle_by_type, ATTR_DEF_BUNDLE_TYPE, "NAME", aname, "FLAGS", flags,
                 "ATTR_TYPE", attr_type, "MAX_VALUES", max_values, "RESTRICTIONS", restrictions, NIRVA_NULL)
// "DEFAULT" and "NEW_DEFAULT" can be set separately, as they each need a va_list
NIRVA_END_RETURN(atdef)

// std_func
#define _NIRVA_CREATE_FUNCTIONAL_1(fname, cat, ext_id, ftype, dummy)	\
    NIRVA_ACTION_RET_BUNDLEPTR(nirva_create_functional, #fname, cat, ext_id, ftype, fname)

// native_func
#define _NIRVA_CREATE_FUNCTIONAL_2(fname, cat, ext_id, ftype, dummy)	\
    NIRVA_ACTION_RET_BUNDLEPTR(nirva_create_functional, #fname, cat, ext_id, ftype, nirva_fname##_wrapper)

// script
#define _NIRVA_CREATE_FUNCTIONAL_3(script_name, cat, ext_id, ftype, scriptlet) \
    NIRVA_ACTION_RET_BUNDLEPTR(nirva_create_functional, #script_name, cat, ext_id, ftype, scriptlet)

#define _NIRVA_CREATE_FUNCTIONAL(fname, cat, ext_id, ftype, param)	\
    _NIRVA_CREATE_FUNCTIONAL_##ftype(fname, cat, ext_id, ftype, param)

#define NIRVA_CREATE_FUNCTIONAL(fname, cat, ext_id, ...)		\
  _NIRVA_CREATE_FUNCTIONAL_##ftype(fname, cat, ext_id, __VA_ARGS__, dummy)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, nirva_create_functional, NIRVA_STRING funcname, NIRVA_INT cat,
                        NIRVA_UINT64 ext_id, NIRVA_INT func_type,  NIRVA_VARIADIC)
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, func, create_bundle_by_type, FUNCTIONAL_BUNDLE_TYPE, "NAME", funcname,
                 "CATEGORY", cat, "EXT_ID", ext_id, "FUNC_TYPE", func_type, NIRVA_NULL)
NIRVA_DEF_VARS(NIRVA_VA_LIST, va)
NIRVA_VA_START(va, func_type)
switch (func_type) {
case FUNC_TYPE_STANDARD:
  NIRVA_VALUE_SET(func, "STANDARD_FUNCTION", STRAND_TYPE_FUNCPTR, NIRVA_VA_ARG(va, NIRVA_STANDARD_FUNC))
  break;
case FUNC_TYPE_NATIVE:
  NIRVA_VALUE_SET(func, "NATIVE_FUNCTION", STRAND_TYPE_FUNCPTR, NIRVA_VA_ARG(va, NIRVA_WRAPPER_FUNC))
  break;
case FUNC_TYPE_SCRIPT:
  NIRVA_VALUE_SET(func, "SCRIPTLET", STRAND_TYPE_CONST_BUNDLEPTR, NIRVA_VA_ARG(va, NIRVA_CONST_BUNDLEPTR))
  break;
default: NIRVA_ASSIGN(func, NIRVA_NULL);
  break;
}
NIRVA_VA_END(va)
NIRVA_ASSERT(func, NIRVA_FAIL, NIRVA_NULL)
NIRVA_END_RETURN(func)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, nirva_segment_add_attr_def, NIRVA_BUNDLEPTR functional,
                        NIRVA_CONST_STRING aname, NIRVA_UINT64 flags,
                        NIRVA_UINT attr_type, NIRVA_INT max_values, NIRVA_BUNDLEPTR restrictions)
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR, attr_def, create_bundle_by_type, ATTR_DEF_BUNDLE_TYPE, "NAME", aname,
                 "FLAGS", flags, "ATTR_TYPE", attr_type, "MAX_VALUES", max_values, "RESTRICTIONS",
                 restrictions, NIRVA_NULL)
NIRVA_CALL(NIRVA_ARRAY_APPEND, functional, "ATTR_DEFS", STRAND_TYPE_BUNDLEPTR, 1, attr_def)
NIRVA_END_RETURN(attr_def)

#define NIRVA_CAP_PREFIX "CAP_"

#define NIRVA_GRIND_INTENTCAP NIRVA_INTERNAL(grind_intentcap)
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, grind_intentcap,
                        NIRVA_CONST_STRING name, NIRVA_CONST_STRING_ARRAY bdeficap)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, icap, caps)
NIRVA_DEF_VARS(NIRVA_INT, intent, i)
NIRVA_CALL_ASSIGN(caps, create_bundle_by_type, CAPS_BUNDLE_TYPE, NIRVA_NULL)
NIRVA_CALL_ASSIGN(intent, atoi, bdeficap[0])

NIRVA_VALUE_SET(icap, "CAPS", STRAND_TYPE_BUNDLEPTR, caps)

NIRVA_CALL_ASSIGN(icap, create_bundle_by_type, ICAP_BUNDLE_TYPE, "INTENTION",
                  intent, "NAME", name, "CAPS", caps, NIRVA_NULL)

for (i = 1; bdeficap[i]; i++) {
  NIRVA_CALL(NIRVA_ARRAY_APPEND, caps, "DATA", STRAND_TYPE_STRING, 1, &bdeficap[i])
}
NIRVA_END_RETURN(icap)

// takes a CAPS bundle and writes only the cap NAMES to a string array
// the resulting array is created in target/stname
#define _NIRVA_CAPS_DISTILL(...)NIRVA_CMD(NIRVA_INTERNAL(caps_distill)(__VA_ARGS__))
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, caps_distill, NIRVA_BUNDLEPTR caps,
                        NIRVA_BUNDLEPTR target, NIRVA_CONST_STRING stname)
NIRVA_DEF_SET(NIRVA_INT, ncaps, 0)
NIRVA_DEF_SET(NIRVA_INT, j, 0)
NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, capnames)
NIRVA_DEF_VARS(NIRVA_SIZE, pfxlen)
NIRVA_ASSERT(caps, NIRVA_FAIL, NIRVA_NULL)
NIRVA_ASSERT(target, NIRVA_FAIL, NIRVA_NULL)
// get idx prefix, this is in the NAME strand of the template (MULTI) strand_def in the blueprint
// default is "CAP_"
NIRVA_ASSIGN(pfxlen, NIRVA_STRING_LENGTH(NIRVA_CAP_PREFIX))
NIRVA_ASSIGN(capnames, NIRVA_BUNDLE_LIST_STRANDS(caps))
while (capnames[ncaps++]);
if (ncaps > 0) {
  NIRVA_DEF_SET(NIRVA_STRING_ARRAY, outnames, NIRVA_MALLOC_ARRAY(NIRVA_STRING, ncaps))
  NIRVA_ASSIGN(ncaps, 0)
  while (capnames[ncaps]) {
    if (NIRVA_STRLEN_EQUAL(capnames[ncaps], NIRVA_CAP_PREFIX)) {
      NIRVA_ASSIGN(outnames[j++], NIRVA_STRDUP_OFFS(capnames[ncaps], pfxlen));
    }
    NIRVA_STRING_FREE(capnames[ncaps])
    ncaps++;
  }
  NIRVA_ARRAY_FREE(capnames)
  NIRVA_ARRAY_SET(target, stname, STRAND_TYPE_STRING, j, outnames)
  for (ncaps = 0; ncaps < j; ncaps++) {
    NIRVA_STRING_FREE(outnames[ncaps])
  }
  NIRVA_ARRAY_FREE(outnames)
} else {
  // should we not create, or set to NULL ?
  NIRVA_ARRAY_CLEAR(target, stname)
}
NIRVA_END_SUCCESS

// takes a CONTRACT bundle template and returns a TRANSFORM bundle
// first we copy some strands over: FLAGS, CAPS - names only
// add a pointer to contract, set status to not ready or something
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, create_transform, NIRVA_BUNDLEPTR contract)
NIRVA_DEF_VARS(NIRVA_UINT64, flags)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, ccaps, transform)
NIRVA_ASSERT(contract, NIRVA_FAIL, "NULL contract")
NIRVA_ASSIGN(flags, NIRVA_GET_VALUE_UINT64(contract, "FLAGS"))
NIRVA_ASSIGN(ccaps, NIRVA_GET_VALUE_BUNDLEPTR(contract, "CAPS"))
// CAPS bundle is a template bundle, so we need to list the strands in it, and check for those which start with the prefix
// we will skip over prefix and the rest is the CAPS name, this is returned as a NIRVA_STRING_ARRAY
NIRVA_CALL_ASSIGN(transform, create_bundle_by_type, TRANSFORM_BUNDLE_TYPE,
                  "FLAGS", flags, "TEMPLATE", contract, "STATUS", TRANSFORM_STATUS_CONFIGURING, NIRVA_NULL)
NIRVA_CALL(_NIRVA_CAPS_DISTILL, ccaps, transform, "CAPS")
NIRVA_END_RETURN(transform)

// check a CAPS set is valide for a contract
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, validate_caps, NIRVA_BUNDLEPTR contract, NIRVA_UINT ncaps,
                        NIRVA_STRING_ARRAY caps)
#ifdef NIRVASCRIPT_CHECK_CAPSVALID
NIRVA_DEF_SET(NIRVA_STRING_ARRAY, vcaps, NIRVA_GET_ARRAY_STRING(contract, "VALID_CAPS"))
NIRVA_DEF_SET(NIRVA_UINT, nvcaps, NIRVA_ARRAY_GET_SIZE(contract, "VALID_CAPS"))
if (nvcaps) {
  NIRVA_DEF_VARS(NIRVA_COND_RESULT, result)
  NIRVA_DEF_VARS(NIRVA_INT, i)
  for (i = 0; i < nvcaps; i++) {
    // vcaps[i] is a cond string, caps is a NULL terminated array of caps
    NIRVA_CALL_ASSIGN(result, NIRVA_INTERNAL(check_cond_script), NIRVASCRIPT_CHECK_CAPSVALID, vcaps[i], caps)
    if (result == NIRVA_COND_SUCCESS) break;
  }
  if (i == nvcaps) NIRVA_RETURN_FAIL;
}
#endif
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, create_in_attrs, NIRVA_BUNDLEPTR attr_group, NIRVA_INT nmaps,
                        NIRVA_BUNDLEPTR_ARRAY attr_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr, attr_def)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN)) continue;
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OPT)) continue;
NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
//
NIRVA_CALL_ASSIGN(attr, NIRVA_INTERNAL(create_attr_from_def), attr_def)
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr, "NAME"))
NIRVA_ATTR_APPEND(attr_group, aname, attr)
NIRVA_STRING_FREE(aname)
}
NIRVA_END_SUCCESS

// here we get the trajectory for a valid TRANSFORM
// first we check the CAPS set passes the RESTRICTIONS in the contract pointed to
// we can then call the SELECTOR in the CONTRACT which will return a pointer to TRAJECTORY
// the TRANSFORM adds a CONST_BUNDLEPTR to this
// the TRAJECTORY will have a WRAPPER (func_def), either this will have a single MAPPING (functional)
// or else we get the MAP_SELECTOR and pass in the almost complete transform to get the MAPPING
// and finally this is referenced by the transform
// indivudual functionals
#define NIRVA_CALL_METHOD_nirva_find_trajectory INTERN
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, find_trajectory, NIRVA_BUNDLEPTR transform)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, cascade, trajectory, attr_group)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, caps)
NIRVA_DEF_VARS(NIRVA_INT, ncaps, nmaps)
NIRVA_DEF_VARS(NIRVA_UINT64, owner_uid)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, res)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, contract, NIRVA_GET_VALUE_BUNDLEPTR(transform, "TEMPLATE"))
NIRVA_ASSERT(contract, NIRVA_FAIL, "NULL contract")
NIRVA_ASSIGN(ncaps, NIRVA_ARRAY_GET_SIZE(transform, "CAPS"))
if (ncaps > 0) {
  NIRVA_ASSIGN(caps, NIRVA_GET_ARRAY_STRING(transform, "CAPS"))
} else NIRVA_ASSIGN(caps, NIRVA_NULL);
NIRVA_CALL_ASSIGN(res, NIRVA_INTERNAL(validate_caps), contract, ncaps, caps)

NIRVA_AUTO_FOR(i, 0, ncaps)
NIRVA_STRING_FREE(caps[i])
NIRVA_LOOP_END
if (caps) {NIRVA_ARRAY_FREE(caps)}
if (res != NIRVA_RESULT_SUCCESS) {
  NIRVA_FAIL("Invalid CAPs subset for transform, please adjust and retry")
}

NIRVA_ASSIGN(cascade, NIRVA_GET_VALUE_BUNDLEPTR(contract, "TRAJECTORY_CHOOSER"))
NIRVA_CALL(NIRVA_INTERNAL(cascade), cascade, transform, &trajectory)

NIRVA_ASSERT(trajectory, NIRVA_FAIL, "NULL trajectory")
NIRVA_VALUE_SET(transform, "TRAJECTORY", STRAND_TYPE_CONST_BUNDLEPTR, trajectory)

NIRVA_ASSIGN(owner_uid, NIRVA_GET_VALUE_UINT64(contract, "OWNER"))
NIRVA_CALL_ASSIGN(attr_group, create_bundle_by_type, ATTR_GROUP_BUNDLE_TYPE, "OWNER", owner_uid, NIRVA_NULL)
NIRVA_ASSERT(attr_group, NIRVA_FAIL, "NULL attr_group")

NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(trajectory, "ATTR_MAPS"))
NIRVA_CALL(NIRVA_INTERNAL(create_in_attrs), attr_group, nmaps, attr_maps)
NIRVA_VALUE_SET(transform, "ATTRIBUTES", STRAND_TYPE_BUNDLEPTR, attr_group)

if (NIRVA_RESULT(NIRVA_INTERNAL(create_func_data), transform) != NIRVA_RESULT_SUCCESS) {
  NIRVA_RECYCLE(trajectory)
  NIRVA_RECYCLE(attr_group)
  NIRVA_FAIL("func_data could not be created for transform")
}
NIRVA_END_RETURN(trajectory)

// free the BLOB data in an attribute. This done - after remapping the blob back to DATA, before deleting the attr,
// and before overwriting the blob (if it is output only)
// if sttype is STRING, then we get the array size, and free all strings before freeing BLOB itself
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, attr_blob_free, NIRVA_BUNDLEPTR attr, NIRVA_UINT sttype)
NIRVA_DEF_VARS(NIRVA_GENPTR, blob)
NIRVA_DEF_VARS(NIRVA_UINT, asize)
NIRVA_ASSERT(attr, NIRVA_FAIL, NIRVA_NULL)
if (NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB")) {
  NIRVA_CALL_ASSIGN(blob, NIRVA_GET_VALUE_GENPTR, attr, "BLOB")
  if (!NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB_SIZE")) {NIRVA_ASSIGN(asize, 0);}
  else  NIRVA_ASSIGN(asize, NIRVA_GET_VALUE_UINT(attr, "BLOB_SIZE"));
  if (asize > 0) {
    if (sttype == STRAND_TYPE_STRING) {
      NIRVA_DEF_SET(NIRVA_STRING_ARRAY, starray, NIRVA_VAR_FROM_GENPTR(NIRVA_STRING_ARRAY, blob))
      NIRVA_DEF_VARS(NIRVA_INT, i)
      for (i = 0; i < asize; i++) NIRVA_STRING_FREE(starray[i]);
    }
    NIRVA_ARRAY_FREE(blob)
  } else {
    if (sttype == STRAND_TYPE_STRING) {
      NIRVA_STRING_FREE(blob)
    } else {
      NIRVA_FREE(blob)
    }
  }
  NIRVA_STRAND_DELETE(attr, "BLOB")
}
if (NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB_SIZE")) NIRVA_STRAND_DELETE(attr, "BLOB_SIZE");
NIRVA_END_SUCCESS

// here we take an attr_group, and a ATTR_MAP array
// for each matching mapping, we locate the attribute either in tx_attrs or seg_attrs
// if the attr has no BLOB, we allocate it, then copy attr DATA into it
// the pointer to BLOB is then set in vals array
// for out attrs, we do the same except we only create the BLOB, the DATA value is not copied

// the array is passed to the native wrapper, which will cast the void * back to typed ptrs, dereferencing
// for input values, the DATA from the attribute is copied, for output (func return) values,
// we just allocate the memory; when the (native) function returns the return_values will be copied into the array
// then from the array, either to attr DATA,  or else to RETURN_VALUES
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, map_from_attrs, NIRVA_BUNDLEPTR tx_attrs, NIRVA_BUNDLEPTR seg_attrs,
                        NIRVA_INT nmaps, NIRVA_BUNDLEPTR_ARRAY attr_maps, NIRVA_BUNDLEPTR func_data,
                        NIRVA_UINT64 maptype, NIRVA_GENPTR_ARRAY vals)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr, attr_def)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_DEF_SET(NIRVA_BOOLEAN, anon_out, NIRVA_FALSE)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr_map, attr_maps[i])
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_map, "MAPPING"))
// check mapping matches
if (maptype == NIRVA_ATTR_MAP_IN) {
  if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN_PARAM)) continue;
} else if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN) || !NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_RETVAL)) continue;
NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_map, "ATTR_DEF"))
if (maptype == NIRVA_ATTR_MAP_OUT && NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OPT)) {
  // this is only allowed for specific func_categories
  // and means that the return value will be mapped anonymously to
  // "RETURN_VALUE" in func_data
  anon_out = NIRVA_TRUE;
  NIRVA_CALL_ASSIGN(sttype, _NIRVA_ATTR_DEF_GET_STRAND_TYPE, attr_def)
}
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
// for ext, map from tx_attrs / seg_attrs, else from seg_attrs
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN)) {
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(tx_attrs, aname));
} else {
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(seg_attrs, aname));
}
NIRVA_STRING_FREE(aname)
if (!attr) {
  if (maptype == NIRVA_ATTR_MAP_IN) {
    if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OPT)) continue;
    // we have a missing in attr. This is a breach of contract.
    //NIRVA_HOOK_TRIGGER(CONTRACT_BREACHED_HOOK)
    NIRVA_RETURN_ERROR
  }
  // if output, we create in tx_attrs or seg_attrs
  NIRVA_CALL_ASSIGN(attr, NIRVA_INTERNAL(create_attr_from_def), attr_def)
  if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN)) {
    // create in tx_attrs
    NIRVA_ATTR_APPEND(tx_attrs, aname, attr)
  } else {
    NIRVA_ATTR_APPEND(seg_attrs, aname, attr)
  }
  NIRVA_CALL_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE, attr)
} else {
  if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN)) {
    NIRVA_CALL_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE, attr)
    NIRVA_CALL(NIRVA_INTERNAL(attr_blob_free), attr, sttype);
  } else {
    if (NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB")) continue;
    NIRVA_CALL_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE, attr);
    if (NIRVA_ATTR_DEF_IS_ARRAY(attr)) {
      NIRVA_VALUE_SET(attr, "BLOB_SIZE", STRAND_TYPE_UINT, NIRVA_ARRAY_GET_SIZE(attr, "DATA"))
    } else {
      NIRVA_STRAND_DELETE(attr, "BLOB_SIZE")
    }
  }
}
// for return arrays, we will just copy the pointer
if (maptype == NIRVA_ATTR_MAP_OUT && NIRVA_ATTR_DEF_IS_ARRAY(attr_def)) continue;

switch (sttype) {
case STRAND_TYPE_INT: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_INT), intp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(intp, NIRVA_GET_ARRAY_INT, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(intp, NIRVA_MALLOC_SIZEOF(NIRVA_INT))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(intp))
} break;
case STRAND_TYPE_UINT: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_UINT), uintp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_ASSIGN(uintp, NIRVA_GET_ARRAY_UINT(attr, "DATA"))
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(uintp, NIRVA_MALLOC_SIZEOF(NIRVA_UINT))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(uintp))
} break;
case STRAND_TYPE_INT64: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_INT64), int64p)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(int64p, NIRVA_GET_ARRAY_INT64, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(int64p, NIRVA_MALLOC_SIZEOF(NIRVA_INT64))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(int64p))
} break;
case STRAND_TYPE_UINT64: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_UINT64), uint64p)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_ASSIGN(uint64p, NIRVA_GET_ARRAY_UINT64(attr, "DATA"))
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(uint64p, NIRVA_MALLOC_SIZEOF(NIRVA_UINT64))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(uint64p))
} break;
case STRAND_TYPE_BOOLEAN: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_BOOLEAN), booleanp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(booleanp, NIRVA_GET_ARRAY_BOOLEAN, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(booleanp, NIRVA_MALLOC_SIZEOF(NIRVA_BOOLEAN))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(booleanp))
} break;
case STRAND_TYPE_DOUBLE: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_DOUBLE), doublep)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(doublep, NIRVA_GET_ARRAY_DOUBLE, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(doublep, NIRVA_MALLOC_SIZEOF(NIRVA_DOUBLE))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(doublep))
} break;
case STRAND_TYPE_STRING: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_STRING), stringp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_ASSIGN(stringp, NIRVA_GET_ARRAY_STRING(attr, "DATA"))
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(stringp, NIRVA_MALLOC_SIZEOF(NIRVA_STRING))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(stringp))
} break;
#ifdef NIRVA_USE_FLOAT
case STRAND_TYPE_FLOAT: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_FLOAT), floatp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(floatp, NIRVA_GET_ARRAY_FLOAT, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(floatp, NIRVA_MALLOC_SIZEOF(NIRVA_FLOAT))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(floatp))
} break;
#endif
case STRAND_TYPE_VOIDPTR: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_VOIDPTR), voidptrp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(voidptrp, NIRVA_GET_ARRAY_VOIDPTR, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(voidptrp, NIRVA_MALLOC_SIZEOF(NIRVA_VOIDPTR))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(voidptrp))
} break;
case STRAND_TYPE_FUNCPTR: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_NATIVE_FUNC), funcptrp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(funcptrp, NIRVA_GET_ARRAY_FUNCPTR, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(funcptrp, NIRVA_MALLOC_SIZEOF(NIRVA_NATIVE_FUNC))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(funcptrp))
} break;
case STRAND_TYPE_BUNDLEPTR: {
  NIRVA_DEF_VARS(NIRVA_TYPE_PTR(NIRVA_BUNDLEPTR), bundleptrp)
  if (maptype == NIRVA_ATTR_MAP_IN) {
    NIRVA_CALL_ASSIGN(bundleptrp, NIRVA_GET_ARRAY_BUNDLEPTR, attr, "DATA")
  } else {
    // for scalar return value - allocate memory, the native wrapper will copy the return value here
    NIRVA_ASSIGN(bundleptrp, NIRVA_MALLOC_SIZEOF(NIRVA_BUNDLEPTR))
  }
  NIRVA_ASSIGN(vals[i], NIRVA_GENPTR_CAST(bundleptrp))
  /*   if (NIRVA_REQUEST(attr, TRANSFER) != NIRVA_REQUEST_YES) */
  /* NIRVA_RETURN(NIRVA_RESULT_FAIL) */
  /*   NIRVA_CALL(nirva_bundle_unlock, attr) */
} break;
default:
  NIRVA_FAIL("unknown attr_type")
  break;
}
if (anon_out) {
  NIRVA_VALUE_SET(func_data, "RETURN_VALUE", STRAND_TYPE_GENPTR, vals[i])
} else {
  NIRVA_VALUE_SET(attr, "BLOB", STRAND_TYPE_GENPTR, vals[i])
}
}
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, map_to_attrs, NIRVA_BUNDLEPTR attrs,  NIRVA_UINT nmaps,
                        NIRVA_BUNDLEPTR_ARRAY attr_maps, NIRVA_VA_LIST va)
NIRVA_DEF_VARS(NIRVA_INT, i)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr, attr_def)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_DEF_VARS(NIRVA_UINT64, mapping, flags)
// valid maptypes:
//  NIRVA_ATTR_MAP_IN_PARAM  (AND)
//  NIRVA_ATTR_MAP_IN_OUT_PARAM as above, but we skip over any with RO
//  NIRVA_ATTR_MAP_RETVAL (skip RO)

for (i = 0; i < nmaps; i++) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr_map, attr_maps[i])
  NIRVA_ASSIGN(mapping, NIRVA_GET_VALUE_UINT64(attr_map, "MAPPING"))
  if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN_IN_PARAM)) continue;
  NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_map, "ATTR_DEF"))
  NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, aname))
  NIRVA_STRING_FREE(aname)
  NIRVA_ASSIGN(flags, NIRVA_GET_VALUE_UINT64(attr, "FLAGS"))
  if (NIRVA_HAS_FLAG(flags, NIRVA_ATTR_FLAG_READONLY)) continue;

  NIRVA_CALL_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE, attr)
  NIRVA_VALUE_SET(attr, "DATA", sttype, va)
}
NIRVA_END_SUCCESS

// delete any attrs not mapped as EXTERN
// - first we will free any "BLOB" data
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, clear_local_attrs, NIRVA_BUNDLEPTR attr_group, NIRVA_UINT nmaps,
                        NIRVA_BUNDLEPTR_ARRAY attr_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr_def, attr)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_DEF_VARS(NIRVA_UINT, sttype)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN)) continue;
NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attr_group, aname))
NIRVA_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE(attr))
NIRVA_CALL(NIRVA_INTERNAL(attr_blob_free), attr, sttype);
NIRVA_ATTR_DELETE(attr_group, aname)
NIRVA_STRING_FREE(aname)
NIRVA_LOOP_END
NIRVA_END_SUCCESS

// data from BLOB is copied back to attr DATA
// this is for attrs which are output from trajectory, as well as any attr in a transform when we read or
// write the value directly
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, remap_attrs, NIRVA_BUNDLEPTR attr_group, NIRVA_UINT nmaps,
                        NIRVA_BUNDLEPTR_ARRAY attr_maps)
NIRVA_DEF_VARS(NIRVA_GENPTR, blob)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr, attr_def)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_DEF_VARS(NIRVA_UINT, asize, sttype)
NIRVA_DEF_VARS(NIRVA_INT, i)
for (i = 0; i < nmaps; i++) {
  // get attr, sttype, blob, blob_size
  // then simply set attr DATA as array of size blob_type, strand_type, sttype
  // blob is either an array pointer, or a pointer to alloced value
  // so thsi should work
  // the we free blob, delete "blob" and "blob_size"
  NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
  if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN_OUTPUT)) continue;
  NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
  NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
  NIRVA_CALL_ASSIGN(attr, NIRVA_ATTR_GET, attr_group, aname)
  NIRVA_STRING_FREE(aname)
  if (!attr) {
    // raise contract breached
    NIRVA_FAIL(NIRVA_NULL)
  }
  if (!NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB")) continue;
  NIRVA_CALL_ASSIGN(blob, NIRVA_GET_VALUE_GENPTR, attr, "BLOB")
  NIRVA_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE(attr))
  if (!NIRVA_BUNDLE_HAS_STRAND(attr, "BLOB_SIZE")) {NIRVA_ASSIGN(asize, 1);}
  else  NIRVA_ASSIGN(asize, NIRVA_GET_VALUE_UINT(attr, "BLOB_SIZE"));
  NIRVA_ARRAY_SET(attr, "DATA", sttype, asize, blob)
  NIRVA_CALL(NIRVA_INTERNAL(attr_blob_free), attr, sttype);
}
NIRVA_END_SUCCESS

NIRVA_DECL_FUNC_INTERNAL(NIRVA_FUNC_RETURN, native_call, NIRVA_BUNDLEPTR functional, NIRVA_BUNDLEPTR func_data)

// for native functionals, if a returned value is an array, and either it is designated for return from the segment (EXTERN)
// or if a functional in the segment will read / write to it directly,
// or if it is a non-const string array, we need to know the size. For extern values, we need this so we can remap
// the data back to attributes, the same holds for attrs for direct access.
// for non-const string, we need this information, so we know how many strings to free
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, calc_array_size, NIRVA_BUNDLEPTR attr_map, NIRVA_BUNDLEPTR attrs,
                        NIRVA_BUNDLEPTR func_data)
// maooing may be CONST, then we just set the value
// else we may have to calulate it by calling a (native wrapped) helper functional in th segment
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr_def, attr)
NIRVA_DEF_VARS(NIRVA_STRING, aname, xname)
NIRVA_DEF_SET(NIRVA_UINT, asize, 0)
if (NIRVA_BUNDLE_HAS_STRAND(attr_map, "ARRAY_SIZE")) {
  NIRVA_ASSIGN(asize, NIRVA_GET_VALUE_UINT(attr_map, "ARRAY_SIZE"))
} else if (NIRVA_BUNDLE_HAS_STRAND(attr_map, "ARRAY_SIZE_FUNC")) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, segment, NIRVA_GET_VALUE_BUNDLEPTR(func_data, "CURRENT_SEGMENT"))
  NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR_ARRAY, functionals, NIRVA_GET_ARRAY_BUNDLEPTR, segment, "FUNCTIONALS")
  NIRVA_DEF_SET(NIRVA_UINT, nfuncs, NIRVA_ARRAY_GET_SIZE(segment, "FUNCTIONALS"))
  NIRVA_DEF_VARS(NIRVA_INT, i)
  NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_map, "ARRAY_SIZE_FUNC"))
  for (i = 0; i < nfuncs; i++) {
    NIRVA_DEF_SET(NIRVA_BUNDLEPTR, func, functionals[i])
    NIRVA_DEF_SET(NIRVA_INT, categ, NIRVA_GET_VALUE_INT(func, "CATEGORY"))
    if (categ != FUNC_CATEGORY_HELPER) continue;
    NIRVA_ASSIGN(xname, NIRVA_GET_VALUE_STRING(func, "NAME"))
    if (NIRVA_STRING_EQUAL(aname, xname)) {
      NIRVA_DEF_VARS(NIRVA_GENPTR, retval)
      NIRVA_CALL(NIRVA_INTERNAL(native_call), func, func_data)
      NIRVA_ASSIGN(retval, NIRVA_GET_VALUE_GENPTR(func_data, "RETURN_VALUE"))
      NIRVA_STRAND_DELETE(func_data, "RETURN_VALUE")
      NIRVA_ASSIGN(asize, NIRVA_VAR_FROM_GENPTR(NIRVA_UINT, retval))
      NIRVA_FREE(retval)
    }
    NIRVA_STRING_FREE(xname)
  }
  NIRVA_STRING_FREE(aname)
} else NIRVA_RETURN_FAIL;
if (asize == 0) NIRVA_RETURN_SUCCESS;
NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_map, "ATTR_DEF"))
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
NIRVA_CALL_ASSIGN(attr, NIRVA_ATTR_GET, attrs, aname)
NIRVA_STRING_FREE(aname)
NIRVA_ASSERT(attr, NIRVA_RETURN, NIRVA_RESULT_ERROR)
NIRVA_VALUE_SET(attr, "BLOB_SIZE", asize)
NIRVA_END_SUCCESS

// go through attr_maps, skip any which dont have mapflags, skip any which are not outputs,
// skip any without ARRAY_SIZE or ARRAY_SIZE_FUNC
// for the remainder, we will call nirva_calc_array_size
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, calc_array_sizes, NIRVA_BUNDLEPTR tx_attrs, NIRVA_BUNDLEPTR seg_attrs,
                        NIRVA_UINT nmaps, NIRVA_BUNDLEPTR_ARRAY attr_maps, NIRVA_BUNDLEPTR func_data, NIRVA_UINT64 mapflags)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_ASSIGN(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64, attr_maps[i], "MAPPING")
if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OUT)) continue;
if (mapflags && !NIRVA_HAS_FLAG(mapping, mapflags)) continue;
if (NIRVA_BUNDLE_HAS_STRAND(attr_maps[i], "ARRAY_SIZE") || NIRVA_BUNDLE_HAS_STRAND(attr_maps[i], "ARRAY_SIZE_FUNC")) {
  if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN)) {
    NIRVA_CALL(NIRVA_INTERNAL(calc_array_size), tx_attrs, attr_maps[i], func_data)
  } else {
    NIRVA_CALL(NIRVA_INTERNAL(calc_array_size), seg_attrs, attr_maps[i], func_data)
  }
}
NIRVA_LOOP_END
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, copy_to_blobs, NIRVA_BUNDLEPTR tx_attrs, NIRVA_BUNDLEPTR seg_attrs,
                        NIRVA_UINT nmaps, NIRVA_BUNDLEPTR_ARRAY attr_maps, NIRVA_GENPTR_ARRAY ovals)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr_def, attr)
NIRVA_DEF_VARS(NIRVA_STRING, aname)
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_RETVAL)) continue;
NIRVA_ASSIGN(attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
if (NIRVA_ATTR_DEF_IS_SCALAR(attr_def)) {continue;}
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN)) {
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(tx_attrs, aname))
} else {
  NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(seg_attrs, aname))
}
NIRVA_STRING_FREE(aname)
if (!attr) {
  // TODO - complain
  continue;
}
NIRVA_VALUE_SET(attr, "BLOB", STRAND_TYPE_GENPTR, ovals[i])
}
NIRVA_END_SUCCESS


NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, native_call, NIRVA_BUNDLEPTR functional, NIRVA_BUNDLEPTR func_data)
// we will create two arrays, the first will be the attribute values to be mapped to IN/OUT parameters
// the array will contain pointers to allocated memory containing copies of the data in the referenced atttribute

// the second will be for return values, in this case we simply create an array of pointers,
// when the target function returns, its return value(s) will be assigned to these locations
//
// both arrays are passed to the wrapper function which calls the real function, casting the in array values
// to pointers of the correct types and dereferencing, whilst the return value is copied into ovals[0]
//
// NB, scripts and standard functions just read from and write to in / out attributes directly

// when the segment completes, any attibutes not mapped as extern are freed, along with their tempstores

NIRVA_DEF_VARS(NIRVA_GENPTR_ARRAY, ivals, ovals)
NIRVA_DEF_VARS(NIRVA_UINT, nmaps, n_in_maps, n_out_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, transform, tx_attrs, seg_attrs)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_WRAPPER_FUNC, func)

NIRVA_ASSIGN(transform, NIRVA_GET_VALUE_BUNDLEPTR(func_data, "TRANSFORM"))
NIRVA_ASSIGN(tx_attrs, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))
NIRVA_ASSIGN(seg_attrs, NIRVA_GET_VALUE_BUNDLEPTR(func_data, "SEG_ATTRS"))

NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, functional, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(functional, "ATTR_MAPS"))

NIRVA_CALL_ASSIGN(n_in_maps, NIRVA_INTERNAL(count_attr_maps), nmaps, attr_maps, NIRVA_ATTR_MAP_IN_PARAM)
// count return values
NIRVA_CALL_ASSIGN(n_out_maps, NIRVA_INTERNAL(count_attr_maps), nmaps, attr_maps, NIRVA_ATTR_MAP_RETVAL)

if (n_in_maps > 0) {
  NIRVA_ASSIGN(ivals, NIRVA_MALLOC_ARRAY(NIRVA_GENPTR, n_in_maps))
  NIRVA_ASSERT(ivals, NIRVA_FAIL, NIRVA_NULL)
  // we read the DATA from attrs, and put this in "blob" data (if not already)
  // then arrange the "blob"s into ivals
  NIRVA_CALL(NIRVA_INTERNAL(map_from_attrs), tx_attrs, seg_attrs, nmaps, attr_maps, func_data, NIRVA_ATTR_MAP_IN, ivals)
} else {NIRVA_ASSIGN(ivals, NIRVA_NULL)}

if (n_out_maps > 0) {
  NIRVA_ASSIGN(ovals, NIRVA_MALLOC_ARRAY(NIRVA_GENPTR, n_out_maps))
  NIRVA_ASSERT(ovals, NIRVA_FAIL, NIRVA_NULL)

  // if output is scalar, as before we create BLOB data, but now the attr data is not copied
  NIRVA_CALL(NIRVA_INTERNAL(map_from_attrs), tx_attrs, seg_attrs, nmaps, attr_maps, func_data, NIRVA_ATTR_MAP_OUT, ovals)
} else NIRVA_ASSIGN(ovals, NIRVA_NULL);

// for any attr_maps that map EXTERN out or in / out, if they are arrays, and ARE mapped PRE_CALC_SIZE,
// if the array_size is non-const, we calculate it
NIRVA_CALL(NIRVA_INTERNAL(calc_array_sizes), tx_attrs, seg_attrs, nmaps, attr_maps, func_data, NIRVA_ATTR_MAP_PRE_CALC_SIZE)

NIRVA_CALL_ASSIGN(func, NIRVA_GET_VALUE_FUNCPTR, functional, "NATIVE_FUNCTION")

(*func)(ivals, ovals);

NIRVA_ARRAY_FREE(ivals)

// for out params which are arrays, we wont have allocated blob, so we set blob equal to ovals[n]
NIRVA_CALL(NIRVA_INTERNAL(copy_to_blobs), tx_attrs, seg_attrs, nmaps, attr_maps, ovals)
NIRVA_ARRAY_FREE(ovals)

// for any attr_maps that map EXTERN out or in / out, if they are arrays, and not mapped PRE_CALC_SIZE,
// if the array_size is non-const, we calculate it
NIRVA_CALL(NIRVA_INTERNAL(calc_array_sizes), tx_attrs, seg_attrs, nmaps, attr_maps, func_data, 0)

NIRVA_END_SUCCESS

#define NIRVA_FUNC_WRAPx(rt,fn,np,...)NIRVA_FUNC_WRAP_##np(rt,fn,__VA_ARGS__)
#define NIRVA_FUNC_WRAP(ret_type,funcname,...)NIRVA_FUNC_WRAPx(ret_type,funcname,__VA_ARGS__,dummy)
#define NIRVA_FUNC_WRAP_0(rt,fn,dd)static void fn##_wrapper(rt*rv,void**vv){*(t*)vv[0]=fn();}
#define NIRVA_FUNC_WRAP_1(rt,fn,t0,dd)static void fn##_wrapper(void**vv){*(rt*)vv[0]=fn(*(t0*)vv[1]);}
#define NIRVA_FUNC_WRAP_2(rt,fn,t0,t1,dd)static void fn##_wrapper(void**vv){*(rt*)vv[0])=fn(*(t0*)vv[1],*(t1*)vv[2]);}
#define NIRVA_FUNC_WRAP_3(rt,fn,t0,t1,t2dd)static void fn##_wrapper(rt*rv,void**vv) \
  {*(rt*)vv[0]=fn(*(t0*)vv[1],*(t1*)vv[2],*(t2*)vv[3]);}
#define NIRVA_FUNC_WRAP_4(rt,fn,t0,t1,t2,t3,dd)static void fn##_wrapper(rt*rv,void**vv) \
  {*(rt*)vv[0]=fn(*(t0*)vv[1],*(t1*)vv[2],*(t2*)vv[3]),*(t3*)vv[4]);}
#define NIRVA_FUNC_WRAP_5(rt,fn,t0,t1,t2,t3,t4,dd)static void fn##_wrapper(rt*rv,void**vv) \
  {*(rt*)vv[0]=fn(*(t0*)vv[1],*(t1*)vv[2],*(t2*)vv[3]),*(t3*)vv[4]);}

#define NIRVA_NR_FUNC_WRAPx(fn,np,...)NIRVA_NR_FUNC_WRAP_##np(fn,__VA_ARGS__)
#define NIRVA_FUNC_WRAP_VOID(funcname,...)NIRVA_NR_FUNC_WRAPx(funcname,__VA_ARGS__,dummy)
#define NIRVA_NR_FUNC_WRAP_0(fn,dd)static void fn##_wrapper(void**vv){fn();}
#define NIRVA_NR_FUNC_WRAP_1(fn,t0,dd)static void fn##_wrapper(void**vv){fn(*(t0*)vv[0]);}
#define NIRVA_NR_FUNC_WRAP_2(fn,t0,t1,dd)static void fn##_wrapper(void**vv){fn(*(t0*)vv[0],*(t1*)vv[1]);}
#define NIRVA_NR_FUNC_WRAP_3(fn,t0,t1,t2dd)static void fn##_wrapper(void**vv) \
  {fn(*(t0*)vv[0],*(t1*)vv[1],*(t2*)vv[2]);}
#define NIRVA_NR_FUNC_WRAP_4(fn,t0,t1,t2,t3,dd)static void fn##_wrapper(void**vv) \
  {fn(*(t0*)vv[0],*(t1*)vv[1],*(t2*)vv[2],*(t3*)vv[3]);}
#define NIRVA_NR_FUNC_WRAP_5(fn,t0,t1,t2,t3,t4,dd)static void fn##_wrapper(void**vv) \
  {fn(*(t0*)vv[0],*(t1*)vv[1],*(t2*)vv[2],*(t3*)vv[3],*(t4*),vv[4]);}

#define NIRVA_MACRO_get_value_std_funcptr(bun, stname)			\
  (nirva_function_t)(NIRVA_RESULT(NIRVA_GET_VALUE_FUNCPTR, bun, stname))

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, run_functional, NIRVA_BUNDLEPTR functional,
                        NIRVA_BUNDLEPTR func_data)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, result)
NIRVA_DEF_SET(NIRVA_INT, func_type, NIRVA_GET_VALUE_INT(functional, "FUNC_TYPE"))
if (NIRVA_EQUALS(func_type, FUNC_TYPE_SCRIPT)) {
  //NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, scriptlet)
  //NIRVA_ASSIGN(scriptlet, NIRVA_GET_VALUE_BUNDLEPTR(functional, "SCRIPTLET"))
  // depends on cat..
  //NIRVA_CALL_ASSIGN(result, nirva_run_scriptlet, scriptlet, func_data)
} else if (NIRVA_EQUALS(func_type, FUNC_TYPE_STANDARD)) {
  NIRVA_DEF_VARS(nirva_function_t, func)
  NIRVA_CALL_ASSIGN(func, NIRVA_MACRO_get_value_std_funcptr, functional, "STANDARD_FUNCTION")
  NIRVA_ASSIGN(result, (*func)(func_data))
} else if (NIRVA_EQUALS(func_type, FUNC_TYPE_NATIVE)) {
  NIRVA_CALL_ASSIGN(result, NIRVA_INTERNAL(native_call), functional, func_data)
}
NIRVA_END_RETURN(result)

// run a SEGMENT, this is a sequence of 1 or more FUNCTIONALS. The components are called in array sequence,
// and we maintain a temporary storage bundle to hold attributes which are created that are iinternal to the segment.
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, run_segment, NIRVA_BUNDLEPTR segment,
                        NIRVA_BUNDLEPTR func_data)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, seg_attrs)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attrs)
NIRVA_DEF_VARS(NIRVA_UINT, n_attrs, sttype)
NIRVA_DEF_ASSIGN(NIRVA_BUNDLEPTR_ARRAY, functionals, NIRVA_GET_ARRAY_BUNDLEPTR, segment, "FUNCTIONALS")
NIRVA_DEF_SET(NIRVA_UINT, n_funcs, NIRVA_ARRAY_GET_SIZE(segment, "FUNCTIONALS"))
NIRVA_AUTO_FOR(i, 0, n_funcs)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, functional, functionals[i])
NIRVA_DEF_SET(NIRVA_INT, cat, NIRVA_GET_VALUE_INT(functional, "CATEGORY"))
if (cat != FUNC_CATEGORY_TX_SEGMENT) continue;
NIRVA_CALL(NIRVA_INTERNAL(run_functional), functionals[i], func_data)
NIRVA_LOOP_END
NIRVA_ARRAY_FREE(functionals)

// clear (delete) any attributes in SEG_ATTRS
NIRVA_ASSIGN(seg_attrs, NIRVA_GET_VALUE_BUNDLEPTR(func_data, "SEG_ATTRS"))
NIRVA_CALL_ASSIGN(attrs, NIRVA_GET_ARRAY_BUNDLEPTR, seg_attrs, "ATTRIBUTES")
NIRVA_ASSIGN(n_attrs, NIRVA_ARRAY_GET_SIZE(seg_attrs, "ATTRIBUTES"))
NIRVA_AUTO_FOR(i, 0, n_attrs)
NIRVA_ASSIGN(sttype, _NIRVA_ATTR_GET_STRAND_TYPE(attrs[i]))
NIRVA_CALL(NIRVA_INTERNAL(attr_blob_free), attrs[i], sttype);
NIRVA_LOOP_END
NIRVA_ARRAY_FREE(attrs)
NIRVA_ARRAY_CLEAR(seg_attrs, "ATTRIBUTES")
NIRVA_STRAND_DELETE(func_data, "SEG_ATTRS")
NIRVA_END_SUCCESS

NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, run_trajectory, NIRVA_BUNDLEPTR transform)
NIRVA_DEF_SET(NIRVA_FUNC_RETURN, result, TX_RESULT_TRAJECTORY_INVALID)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, trajectory, next_segment, func_data)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, attr_grp)
NIRVA_DEF_VARS(NIRVA_UINT, nmaps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_SET(NIRVA_BUNDLEPTR_ARRAY, segments, NIRVA_NULL)
NIRVA_DEF_SET(NIRVA_INT, i, 0)
// run a selector to get next segment
// make a bundle in for selector with selector and segments
NIRVA_ASSIGN(trajectory, NIRVA_GET_VALUE_BUNDLEPTR(transform, "TRAJECTORY"))
NIRVA_ASSERT(trajectory, NIRVA_ERROR, ERR_IS_NULL, transform, "TRAJECTORY", NIRVA_NULL)
NIRVA_ASSIGN(func_data, NIRVA_GET_VALUE_BUNDLEPTR(transform, "FUNC_DATA"))
NIRVA_ASSERT(func_data, NIRVA_ERROR, ERR_IS_NULL, transform, "FUNC_DATA", NIRVA_NULL)
//  NIRVA_ASSIGN(selector, NIRVA_GET_VALUE_BUNDLEPTR(trajectory, "SEGMENT_SELECTOR"))
// TODO: trigger tx_start hook
// we cannot always have a selector for segments, otherwise the selector would
// also have a selector, which would also have a select, ad infinitum
// so if there is none, we just run the segments in array order
//
//  if (selector) NIRVA_CALL_ASSIGN(next_segment, NIRVA_SELECT, STRAND_TYPE_BUNDLEPTR, selector, transform)
if (0);
else {
  NIRVA_CALL_ASSIGN(segments, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "SEGMENTS")
  NIRVA_ASSIGN(next_segment, segments[0])
}
NIRVA_VALUE_SET(func_data, "CURRENT_SEGMENT", STRAND_TYPE_CONST_BUNDLEPTR, next_segment)
while (next_segment != SEGMENT_END) {
  // call segment start hooks. If arbitrator is present it will check next segements
  // it may return COND_PROXY, then we should exit
  // if ajudicator is present it may mark the tx and return cond_abandon
  NIRVA_CALL_ASSIGN(result, NIRVA_INTERNAL(run_segment), next_segment, func_data)
  // TODO: check response for errors, cancel, idle, etc
  // TODO: trigger segment end hooks
  //
  //if (selector) NIRVA_CALL_ASSIGN(next_segment, NIRVA_SELECT, STRAND_TYPE_BUNDLEPTR, selector, transform)
  if (0);
  else NIRVA_ASSIGN(next_segment, segments[i++]);

  NIRVA_VALUE_SET(func_data, "CURRENT_SEGMENT", STRAND_TYPE_CONST_BUNDLEPTR, next_segment)
}
if (segments) NIRVA_ARRAY_FREE(segments);

// clear (delete) any attributes in ATTRIBUTES not mapped es EXTERN
NIRVA_ASSIGN(attr_grp, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))
NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_CALL_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE, trajectory, "ATTR_MAPS")
NIRVA_CALL(NIRVA_INTERNAL(clear_local_attrs), attr_grp, nmaps, attr_maps)
// for any remaining attrs in ATTRIBUTES, if they are mapped as in / out or out from the trajectory, we copy
// tmpstore back to attr data now
NIRVA_CALL(NIRVA_INTERNAL(remap_attrs), attr_grp, nmaps, attr_maps)
NIRVA_ARRAY_FREE(attr_maps)
NIRVA_END_RETURN(result)

// here we are supplied a transform which has been created from a contract
// first we check the CAPs are valid, using RESTRICTIONS in the contract
// if these are valid, then we check the EXTRA_CONDITIONS, using the ATTR_POOL as fodder
// if this succeeds, then we run the trajectory_chooser in the contract, passing in the transform itself
// this will return a TRAJECTORY, which we add a pointer to in TRAJECTORY
// then we call the trajectory runner, which will run each segment in turn, updating the STATUS
NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, action_transform, NIRVA_BUNDLEPTR transform)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, conds, trajectory, func_data, attr, attrs)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, result)
//NIRVA_DEF_VARS(NIRVA_INT64, status)
NIRVA_DEF_VARS(NIRVA_UINT64, flags)
NIRVA_DEF_VARS(NIRVA_INT, nmaps)
NIRVA_ASSERT(transform, NIRVA_FAIL, NIRVA_NULL)
NIRVA_ASSIGN(trajectory, NIRVA_GET_VALUE_BUNDLEPTR(transform, "TRAJECTORY"))
NIRVA_ASSERT(trajectory, NIRVA_FAIL, NIRVA_NULL)
NIRVA_ASSIGN(func_data, NIRVA_GET_VALUE_BUNDLEPTR(transform, "FUNC_DATA"))
NIRVA_ASSERT(func_data, NIRVA_FAIL, NIRVA_NULL)

NIRVA_ASSIGN(conds, NIRVA_GET_VALUE_BUNDLEPTR(transform, "EXTRA_CONDITIONS"))
if (conds) {
  NIRVA_CALL_ASSIGN(result, NIRVA_INTERNAL(check_cond_script), conds, transform)
  if (result != NIRVA_COND_SUCCESS)
    NIRVA_RETURN_FAIL;
}

// check all mand. inputs have value set
// or if sequential,, have a connection
// set all input attrs readonly, unless they are also out
NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(trajectory, "ATTR_MAPS"))
NIRVA_ASSIGN(attrs, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))
// clear OWNER for attr_group
NIRVA_STRAND_DELETE(attrs, "OWNER")
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
NIRVA_DEF_VARS(NIRVA_STRING, aname)
if (!NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN)) continue;
NIRVA_ASSIGN(aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
NIRVA_ASSIGN(attr, NIRVA_ATTR_GET(attrs, aname))
NIRVA_STRING_FREE(aname)
if (!attr || !NIRVA_BUNDLE_HAS_STRAND(attr, "DATA")) {
  if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OPT)) continue;
  // missing attr
  NIRVA_ARRAY_FREE(attr_maps)
  NIRVA_RETURN_FAIL;
}
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_OUT)) continue;
NIRVA_ASSIGN(flags, NIRVA_GET_VALUE_UINT64(attr, "FLAGS"))
NIRVA_VALUE_SET(attr, "FLAGS", STRAND_TYPE_UINT64, flags | NIRVA_ATTR_FLAG_READONLY)
NIRVA_LOOP_END
NIRVA_ARRAY_FREE(attr_maps)
//NIRVA_VALUE_SET(attrs, "OWNER", myuid)

NIRVA_CALL_ASSIGN(result, NIRVA_INTERNAL(run_trajectory), transform)
if (!NIRVA_BUNDLE_HAS_STRAND(transform, "TX_RESULT")) {
  NIRVA_DEF_SET(NIRVA_INT64, tx_result, TX_RESULT_SUCCESS)
  NIRVA_DEF_SET(NIRVA_INT64, status, NIRVA_GET_VALUE_INT64(func_data, "STATUS"))
  if (status < 32) {
    if (result != NIRVA_RESULT_SUCCESS) {
      NIRVA_ASSIGN(tx_result, TX_RESULT_ERROR)
    }
  } else {
    if (status == TRANSFORM_STATUS_CANCELLED) {
      NIRVA_ASSIGN(tx_result, TX_RESULT_CANCELLED)
    }
    if (status == TRANSFORM_STATUS_ERROR) {
      NIRVA_ASSIGN(tx_result, TX_RESULT_ERROR)
    }
    if (status == TRANSFORM_STATUS_TIMED_OUT) {
      NIRVA_ASSIGN(tx_result, TX_RESULT_SYNC_TIMED_OUT)
    }
  }
  NIRVA_VALUE_SET(transform, "TX_RESULT", STRAND_TYPE_INT64, tx_result)
}
NIRVA_END_SUCCESS

#define NIRVA_STATIC_TRANSFORMS NIRVA_NULL

// in bootstrap gens 1,2 and 3 we lookup the contracts in keyed_array NIRVA_STATIC_TRANSFORMS
// during bootstrap transition from gen 3 to gen 4, the keyed_array is handed over to the structure
// which then manages this
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, find_structure_contract, NIRVA_CONST_STRING action_name)
if (NIRVA_FUNC_GENERATION < 4) {
  NIRVA_RETURN_RESULT(NIRVA_INTERNAL(get_idx_value_bundleptr), NIRVA_STATIC_TRANSFORMS, "CONTRACTS", NIRVA_IDX_PREFIX)
}
// do something else
NIRVA_END_RETURN(NIRVA_NULL)

#ifdef NEED_NIRVA_ACTION_RET
// this is the internal API call for "structure transforms", a.k.a ACTIONS
// these are referred to by "action_name", which can be used to look up a contract owned by the structure
// these contracts all have the properties, that they need no negotiation, have fixed CAPs, have a single trajectory
// option which has a native_wrapper, all input attributes are non optional and mapped from parameters
// and there is a single non optional output attribute
//
// externally,this is not called directly, but instead depending on the return type, it will be called via a wrapper function
// which in turn can be enclosed in a macro call.
// the simplest wrapper is NIRVA_ACTION, which does not return a value, NIRVA_ACTION_RET, which returns NIRVA_RETURN_TYPE
// there is also NIRVA_ACTION_STRING and NIRVA_ACTION_BUNDLEPTR.
//
// the first step is to get a CONTRACT; this is done by looking up the "action_name"
// the contract is then passed to nirva_create_transform, which returns a transform
// since these are STRUCTURAL transforms, there is no CAPs negotiation to be done,
// so we simply call nirva_find_trajectory, passing in the transform
// after this we can create func_data for the traansform, using the trajectory attr_maps to convert the va_list to
// attribute data
//
// after return from nirva_action_transform, the finished transform is returned to the caller.
// the wrapper will then do the reverse - map an output attribute to the function return
// The wrapper will also recycle the transform bundle, to avoid leaking memory.
//
// adding new structure transforms can be done at any time, it simply requires creating a new contract
// and then appending this to the structure transforms lookup index.

// in the default setup, nirva_action_ret is a FIXED API function, and can be overriden at compile time by defining
// NIRVA_IMPL_FUNC_action_ret
// or dynamically by changing the value of function pointer _nirva_actio_ret

//#define _NIRVA_DEPLOY_def_nirva_action_ret
NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, action_ret, NIRVA_CONST_STRING action_name, NIRVA_VA_LIST vargs)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, contract, transform, trajectory, attrs)
NIRVA_DEF_VARS(NIRVA_UINT64, flags)
NIRVA_DEF_VARS(NIRVA_INT, wrapping)
NIRVA_DEF_VARS(NIRVA_UINT, nmaps)
NIRVA_CALL_ASSIGN(contract, NIRVA_INTERNAL(find_structure_contract), action_name)
NIRVA_ASSERT(contract, NIRVA_FAIL, "no contract found")
// check the contract, if it isnt flagged TX_FLAG_NO_NEGOTIATE, we cant call it from here

NIRVA_ASSIGN(flags, NIRVA_GET_VALUE_UINT64(contract, "FLAGS"))
if (!NIRVA_HAS_FLAG(flags, TX_FLAG_NO_NEGOTIATE)) NIRVA_FAIL("contract needs negotiation");

NIRVA_CALL_ASSIGN(transform, NIRVA_INTERNAL(create_transform), contract)
NIRVA_ASSERT(transform, NIRVA_FAIL, "unable to make transform")

NIRVA_CALL_ASSIGN(trajectory, NIRVA_INTERNAL(find_trajectory), transform)	;

if (trajectory == NIRVA_NULL) {
  NIRVA_RECYCLE(transform)
  NIRVA_FAIL("no trajectory found")
}

NIRVA_ASSIGN(wrapping, NIRVA_GET_VALUE_INT(trajectory, "WRAPPING"))

if (wrapping != FUNC_TYPE_NATIVE) {
  NIRVA_RECYCLE(transform)
  NIRVA_FAIL("nirva_Action_ret - trajectory wrapping must be 'native'")
}

// the input attributes should have been created in ATTRIBUTES in transform
// now since the trajectory has to be native_wrapped, this means that all the input attrs are mapped from params

NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(trajectory, "ATTR_MAPS"))

NIRVA_ASSIGN(attrs, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))

NIRVA_CALL(NIRVA_INTERNAL(map_to_attrs), attrs, nmaps, attr_maps, vargs);
NIRVA_ARRAY_FREE(attr_maps)
NIRVA_CALL(NIRVA_INTERNAL(action_transform), transform)
NIRVA_END_RETURN(transform)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, action, NIRVA_CONST_STRING funcname, NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, transform)
NIRVA_DEF_VARS(NIRVA_VA_LIST, vargs)
NIRVA_VA_START(vargs, funcname)
NIRVA_CALL_ASSIGN(transform, NIRVA_INTERNAL(action_ret), funcname, vargs)
NIRVA_VA_END(vargs)
NIRVA_ASSERT(transform, NIRVA_FAIL, "Error running transform", NIRVA_NULL)
NIRVA_RECYCLE(transform)
NIRVA_FUNC_END

NIRVA_DEF_FUNC_INTERNAL(NIRVA_STRING, action_ret_string, NIRVA_CONST_STRING funcname,
                        NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, transform, trajectory, attrs)
NIRVA_DEF_VARS(NIRVA_STRING, ret)
NIRVA_DEF_VARS(NIRVA_UINT, nmaps)
NIRVA_DEF_VARS(NIRVA_VA_LIST, vargs)
NIRVA_VA_START(vargs, funcname)
NIRVA_CALL_ASSIGN(transform, NIRVA_INTERNAL(action_ret), funcname, vargs)
NIRVA_VA_END(vargs)
NIRVA_ASSERT(transform, NIRVA_FAIL, NIRVA_NULL)
// find the attr_map in trajectory which maps with OUT | PARAM | EXTERN and not IN
// get the STRING value in DATA and return it
NIRVA_ASSIGN(attrs, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))
NIRVA_ASSIGN(trajectory, NIRVA_GET_VALUE_BUNDLEPTR(transform, "TRAJECTORY"))
NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(trajectory, "ATTR_MAPS"))
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN_RETVAL) && !NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN)) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
  NIRVA_DEF_SET(NIRVA_STRING, aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr, NIRVA_ATTR_GET(attrs, aname))
  NIRVA_STRING_FREE(aname)
  if (attr) {
    NIRVA_ASSIGN(ret, NIRVA_GET_VALUE_STRING(attr, "DATA"))
    break;
  }
}
NIRVA_LOOP_END
NIRVA_ARRAY_FREE(attr_maps)
NIRVA_RECYCLE(transform)
NIRVA_END_RETURN(ret)

NIRVA_DEF_FUNC_INTERNAL(NIRVA_BUNDLEPTR, action_ret_bundleptr, NIRVA_CONST_STRING funcname,
                        NIRVA_VARIADIC)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attr_maps)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, transform, trajectory, attrs)
NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, ret)
NIRVA_DEF_VARS(NIRVA_VA_LIST, vargs)
NIRVA_DEF_VARS(NIRVA_UINT, nmaps)
NIRVA_VA_START(vargs, funcname)
NIRVA_CALL_ASSIGN(transform, NIRVA_INTERNAL(action_ret), funcname, vargs)
NIRVA_VA_END(vargs)
NIRVA_ASSERT(transform, NIRVA_FAIL, NIRVA_NULL)
NIRVA_ASSIGN(attrs, NIRVA_GET_VALUE_BUNDLEPTR(transform, "ATTRIBUTES"))
NIRVA_ASSIGN(trajectory, NIRVA_GET_VALUE_BUNDLEPTR(transform, "TRAJECTORY"))
NIRVA_CALL_ASSIGN(attr_maps, NIRVA_GET_ARRAY_BUNDLEPTR, trajectory, "ATTR_MAPS")
NIRVA_ASSIGN(nmaps, NIRVA_ARRAY_GET_SIZE(trajectory, "ATTR_MAPS"))
NIRVA_AUTO_FOR(i, 0, nmaps)
NIRVA_DEF_SET(NIRVA_UINT64, mapping, NIRVA_GET_VALUE_UINT64(attr_maps[i], "MAPPING"))
if (NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_EXTERN_RETVAL) && !NIRVA_HAS_FLAG(mapping, NIRVA_ATTR_MAP_IN)) {
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr_def, NIRVA_GET_VALUE_BUNDLEPTR(attr_maps[i], "ATTR_DEF"))
  NIRVA_DEF_SET(NIRVA_STRING, aname, NIRVA_GET_VALUE_STRING(attr_def, "NAME"))
  NIRVA_DEF_SET(NIRVA_BUNDLEPTR, attr, NIRVA_ATTR_GET(attrs, aname))
  NIRVA_STRING_FREE(aname)
  if (attr) {
    NIRVA_ASSIGN(ret, NIRVA_GET_VALUE_BUNDLEPTR(attr, "DATA"))
    NIRVA_CALL(NIRVA_INTERNAL(bundle_reparent), 0, attr, "DATA", NIRVA_NULL, NIRVA_NULL, 1)
    break;
  }
}
NIRVA_LOOP_END
NIRVA_ARRAY_FREE(attr_maps)
NIRVA_RECYCLE(transform)
NIRVA_END_RETURN(ret)

#endif

//
#define NIRVA_COND_CHECK(conds,...)NIRVA_INTERNAL(cond_check_script)(conds, __VA_ARGS__)
#define NIRVA_SATISFY(intent, caps)NIRVA_INTERNAL(action_ret_bundleptr)("nirva_satisfy", intent, caps)
//

#define ADDC(text) "#" text
#define ADDS(text) ADDC(#text)
#define XMB(name, pfx,  ...) make_bundledef(name, pfx,"" __VA_ARGS__)
#define XMBX(name) XMB(#name, NULL, _##name##_BUNDLE)

#define REG_ALL_ITEMS(DOMAIN)						\
  make_bundledef(NULL, "STRAND_" #DOMAIN "_", MS("" ALL_STRANDS_##DOMAIN), NULL), \
    make_bundledef(NULL, "BUNDLE_" #DOMAIN "_", MS("" ALL_BUNDLES_##DOMAIN), NULL)

#define CONCAT_DOMAINS(MACRO) make_bundledef(NULL, NULL, FOR_ALL_DOMAINS(MACRO), NULL)

#define REG_ALL CONCAT_DOMAINS(REG_ALL_ITEMS)

#define _NIRVA_MAKE_BUNDLEDEFS BUNLISTx(BDEF_IGN,BDEF_INIT,,) all_def_strands = REG_ALL;
#define _NIRVA_DEF_REST(n) nirva_restrictions[n] = NIRVA_RESTRICTION_##n;
#define _NIRVA_MAKE_RESTRICTIONS(a,b) NIRVA_DO_nn_times(a,b,_NIRVA_DEF_REST)

//#define INIT_CORE_ATTRDEF_PACKS ABUNLISTx(BDEF_IGN,ATTR_BDEF_INIT,,) all_attrdef_packs = REG_ALL_ATTRS;
#define NIRVA_INIT_CORE _NIRVA_MAKE_BUNDLEDEFS _NIRVA_MAKE_RESTRICTIONS(3,5)						\
//  _NIRVA_DEPLOY_def_nirva_action;

#define NIRVA_CORE_DEFS							\
  NIRVA_BUNDLEPTR STRUCTURE_PRIME;					\
  NIRVA_BUNDLEPTR STRUCTURE_APP;					\
  NIRVA_CONST_STRING_ARRAY maker_get_bundledef(bundle_type btype);	\
  _NIRVA_DEPLOY_MAKE_STRANDS_FUNC;					\
  _NIRVA_DEPLOY_MAKE_BUNDLEDEF_FUNC;					\
  _NIRVA_DEPLOY_def_nirva_wait_retry;					\
  const char **all_def_strands, BUNLIST(BDEF_DEF_CONCAT,**,_BUNDLEDEF = NULL) \
    **LAST_BUNDLEDEF = NULL;						\
const char **all_attr_packs, ABUNLIST(ATTR_BDEF_DEF_CONCAT,**,_ATTR_GROUP = NULL) \
    **LAST_ATTR_GROUP = NULL;						\
const char **nirva_restrictions[N_REST_TYPES];						\
const char ***builtin_bundledefs[] = {BUNLIST(BDEF_DEF_CONCAT, &,_BUNDLEDEF) NULL}; \
const char ***builtin_attr_bundles[] = {ABUNLIST(ATTR_BDEF_DEF_CONCAT, &,_ATTR_GROUP) NULL}; \
NIRVA_CONST_STRING_ARRAY maker_get_bundledef(bundle_type btype)	\
{return (btype >= 0 && btype < n_builtin_bundledefs ? *builtin_bundledefs[btype] : NULL);} \
const char **maker_get_attr_bundle(attr_bundle_type abtype)		\
{return (abtype >= 0 && abtype < n_builtin_attr_bundles ? *builtin_attr_bundles[abtype] : NULL);}

#define BDEF_IGN(a,b,c)

// make BNAME_BUNDLEDEF
#define BDEF_INIT(BNAME, PRE, EXTRA) BNAME##_BUNDLEDEF = MBX(BNAME);

// make ATTR_BNAME_ATTR_GROUP
// e.g. ASPECT_THREAD_ATTR_GROUP
#define ATTR_BDEF_INIT(ATTR_BNAME, PRE, EXTRA) ATTR_BNAME##_ATTR_GROUP = MABX(ATTR_BNAME);

#define _MAKE_IMPL_COND_FUNC_DESC(m, nnn) _CALL(_MAKE_IMPL_COND_FUNC_##n##_DESC, nnn)

#define DEFINE_CORE_BUNDLES _DEFINE_CORE_BUNDLES
#define INIT_CORE_BUNDLES _INIT_CORE_BUNDLES

#ifdef _HAVE_LSD_
NIRVA_FILE_STATIC NIRVA_NO_RETURN nirva_null_cb(void *none) {};
#define NIRVA_LSD_CREATE_P(lsd,type) NIRVA_INLINE			\
  (NIRVA_DEF_VARS(const lsd_struct_def_t*,lsd) NIRVA_DEF_VARS(type*,ts) NIRVA_NEW(ts,type) \
   NIRVA_CALL_ASSIGN(lsd,lsd_create_p,#type,ts,NIRVA_SIZEOF(type),&ts->lsd) \
   NIRVA_CALL(nirva_add_flds(#type,ts)) NIRVA_FREE(ts))
#define ADD_SPCL_FIELD_NO_CBS(ts,f,fl,ds) NIRVA_CALL		\
  (nirva_add_special_field,ts->lsd,#f,fl,&ts->##f,ds,ts)
#define ADD_SPCL_FIELD_CBS(ts,f,fl,ds,icb,ccb,fcb) NIRVA_CALL		\
  (nirva_add_special_field,ts->lsd,#f,fl,&ts->##f,ds,ts,__VA_ARGS__)
#endif

// Exactly one file per application needs to be designated as "Bundle Maker"
// this file will act as a "home" for the structure definitions and functions and
// so on. It will also be the place where custom bundles can be built
//
// the designated file must #include this header
//
// Depending on what level of "automation" chosen to be the default, there may be some
// additional setup requirements before including the header. See the documentation for details.
//
//
/* MACROS: The two macros which the bundle maker may call */

//		NIRVA_CORE_DEFS
// This macro will create the default enums and so on
// as well as "deploying" helper functions required for the bootstrap

//		NIRVA_INIT_CORE
// This should be placed inside a function
// The macro will set up the runtime dependent definitions

// there are a few steps needed to bootstrap the structure. These can be done
// from the same function which includes INIT_CORE_BUNDLES or elsewhere
// See the documentation for details.
//
// The first time the application is run with NIRVA inside, nirvascope will open,
// and the procedure for migrating existing code and for building / customising
// will be available.
//////////////
////////////

#undef create_bundle_by_type

#ifdef __cplusplus
}
#endif /* __cplusplus */
