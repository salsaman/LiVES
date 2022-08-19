// object-constants.h
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// PROJECT N.I.R.V.A
// bootstrap:
//
// The macros and defintions here are just sufficient to bootstrap the system
// - facilitate the creation bundle defintions for all the defualt types, and enumerate them
//
// - provide default versions of "Implementation functions"
//
// - define language specific macros for creating generic functions and macros
//
// - ensure the bootstrap process has all of the funcitonality required
//
// - provide an application function call, nirva_inti() which will begin the bootstrap process
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

  static int in_bootstrap = 1;

  /////////////////////////// bundles //

#define _COND_ATTR_VAL(attr_name) "ATTR_VALUE %s", attr_name
#define _COND_STRAND_VAL(attr_name) "STRAND_VALUE %s", attr_name
#define _COND_INT_VAL(val) "%d", val
#define _COND_STRING_VAL(val) "%s", val
#define _COND_UINT_VAL(val) "%d", val
#define _COND_FLOAT_VAL(val) "%f", val
#define _COND_INT64_VAL(val) PrI64d, val
#define _COND_UINT64_VAL(val) PrI64u, val

  // the following substitutions may be used in consitions, automations and so on
  // in place of a value

  // the value held in the strand which is the subject of the condition, trigger or automation
#define NIRVA_STRAND_REF_SElF_VAL 			"!SELF_VAL"

  // the bundle immediately containing the strand
#define NIRVA_STRAND_REF_CONTAINER	 		"!CONTAINER"

  // for pre-data changes, the current value to be updated, removed or deleted
  // for post-data, the value of the strand before updating
  // for attributes, this refers to the attribute "data"
#define NIRVA_STRAND_REF_OLD_VAL 			"!OLD_VAL"

  // for post-data changes, the current value,
  // for pre-data, the new value to be set, appended or added
  // for attributes, this refers to the attribute "data"
#define NIRVA_STRAND_REF_NEW_VAL 			"!NEW_VAL"

  // DIRECTIVES
  // these are instructions that can be embedded in bundledef
  // since there is the posisibility that more directives may be added in the future,
  // if building a bundle from a blueprint, any directives not recocngised should be
  // skipped over (until after the matching @END directive)

#define DIRECTIVE_BEGIN "@BEGIN "
#define DIRECTIVE_END "@END "

#define DIR_START(dirnm)  			DIRECTIVE_BEGIN #dirnm
#define DIR_FINISH(dirnm) 			DIRECTIVE_END #dirnm

#define DIRECTIVE_COND		       		DIR_START(condition)
#define DIRECTIVE_COND_END     	   	    	DIR_FINISH(condition)
#define NIRVA_COND_CREATE(...) MSTY("%s", DIRECTIVE_COND, __VA_ARGS__, "%s", DIRECTIVE_COND_END)

#define DIRECTIVE_MAKE_EXCLUSIVE     	       	DIR_START(make_exclusive)
  // item_names
  // one per line
  // pick one only
#define DIRECTIVE_MAKE_EXCLUSIVE_END 	       	DIR_FINISH(make_exclusive)

  // when building bundle, add automation hook for each strand, which prevents it from
  // being created if any of the other strands exist
#define MAKE_EXCLUSIVE(...) MS(DIRECTIVE_MAKE_EXCLUSIVE, __VA_ARGS__,	\
			       DIRECTIVE_MAKE_EXCLUSIVE_END)

  // clear opt flag bit from blueprint
#define DIRECTIVE_MAKE_MANDATORY     	       	DIR_START(make_mandatory)
  // strand_name
#define DIRECTIVE_MAKE_MANDATORY_END 	       	DIR_FINISH(make_mandatory)

#define MAKE_MANDATORY(domain,item) MS(DIRECTIVE_MAKE_MANDATORY, STRAND_NAMEU(#domain,#item), \
				       DIRECTIVE_MAKE_MANDATORY_END)

  // make strand value readonly, eg. strand_type, name, uid, bundle type
  // value can be set once (after default)
#define DIRECTIVE_MAKE_READONLY     	       	DIR_START(make_readonly)
  // strand_name
#define DIRECTIVE_MAKE_READONLY_END 	       	DIR_FINISH(make_readonly)

#define MAKE_READONLY(domain,item) MS(DIRECTIVE_MAKE_READONLY, STRAND_NAMEU(#domain,#item), \
				      DIRECTIVE_MAKE_READONLY_END)

  // see notes in documentation
  // until the strand is added to a bundle with hook stacks, the automation is PENDING
  // after adding a sub bundle to a bundle the method to check for pending automations must be
  // actioned. This will "migrate" the automations upwards to the stack
#define DIRECTIVE_ADD_HOOK_AUTOMATION          	DIR_START(add_hook_auto)
  // automation script
#define DIRECTIVE_ADD_HOOK_AUTOMATION_END    	DIR_FINISH(add_hook_auto)

  // hook automations are basically self callbacks, but in the form of
  // a script
#define ADD_HOOK_AUTOMATION(strand_name, hook_number, condtype, ...)	\
  MSTY("%s",DIRECTIVE_ADD_HOOK_AUTOMATION, "%s", #strand_name, "%d", hook_number##_HOOK, \
       "%d", condtype, "%s", DIRECTIVE_ADD_HOOK_AUTOMATION_END)

#define NIRVA_AUTO_CMD_1 "SET_STRAND_VALUE" // strand name, strand_val
#define NIRVA_AUTO_CMD_2 "CASCADE_HOOKS" //
#define NIRVA_AUTO_CMD_3 "ADD_STRAND_TO" // holder, strand_name, type, value
#define NIRVA_AUTO_CMD_4 "REMOVE_STRAND_FROM" //holder, nae
#define NIRVA_AUTO_CMD_5 "ADD_HOOK_CB" // obj, item, hook_number, cb, data
#define NIRVA_AUTO_CMD_6 "REMOVE_HOOK_CB" // bj, item, hook_number, cb, data
#define NIRVA_AUTO_CMD_7 "APPEND_TO_ARRAY" // strand, vakue
#define NIRVA_AUTO_CMD_8 "REMOVE_FROM_ARRAY" // strand, value
#define NIRVA_AUTO_CMD_9 "ADD_HOOK_AUTOMATION" // args
#define NIRVA_AUTO_CMD_10 "SET_STRAND_TYPE" // strand_name, strand_type

  // strand 2

  // flag bits, decimal value in strand1
#define STRAND2_FLAG_ARRAY_OF				1
  // for pointers_to, CONNECTION_BUNDLE should be used
#define STRAND2_FLAG_PTR_TO				2
#define STRAND2_FLAG_PTR_TO_ARRAY	       		4

  //	FLAGS FOR blueprint bundles

  // denotes the data type is an array
#define BLUEPRINT_FLAG_ARRAY 			(1ull << 0)
  // denotes strand is a pointer to scalar of specified type
#define BLUEPRINT_FLAG_PTR_TO_SCALAR        	(1ull << 1)
  // denotes strand is a pointer to specified type
#define BLUEPRINT_FLAG_PTR_TO_ARRAY        	(1ull << 2)
  // denotes strand is optional
#define BLUEPRINT_FLAG_OPTIONAL 		(1ull << 3)
  // value may be set once (after defualt), then becomes constant
  // e.g uid, bundle_type, name, strand_type
#define BLUEPRINT_FLAG_READONLY 	       	(1ull << 4)

#define BLUEPRINT_FLAG_COMMENT 			(1ull << 5)

#define BLUEPRINT_FLAG_DIRECTIVE 		(1ull << 6)

  // item naming convntions
  // for STANDARD STRANDS, the default name is STRAND_DOMAIN_ITEM, e.g STRAND_GNERIC_FLAGS

  // for NAMED STRANDS, the ITEM name be be overriden, so we could change this to e.g
  // STRAND_GENERIC_BITMAP

  // for included bundles (sub-bundles), the default is still STRAND_DOMAIN_ITEM
  // however changeing the ITEM name changes STRAND to BUNDLE
  // e.g we have STRAND_OBJECT_ATTRS, then changing name to SEETINGS would make this
  // BUNDLE_OBJECT_SETTINGS

  // finally when ADDing, ie pointing to an a remote bundle or array, one always defines a custom
  // domain / item. In this case it would be somthing like CONST_TARGET_FLAGS, for DOMAIN TARGET,
  // ITEM FLAGS

#define GET_STRAND_TYPE(xdomain, xitem) _CALL(_GET_ETYPE, STRAND_##xdomain##_##xitem##_TYPE)

  // for pointed to things
#define EXTERN_NAMEU(a, b) "CONST_" a "_" b
#define BUNDLE_NAMEU(a, b) "BUNDLE_" a "_" b
#define STRAND_NAMEU(a, b) "STRAND_" a "_" b

#define _GET_TYPE(a, b) _STRAND_TYPE_##a
#define _GET_ETYPE(a, b) STRAND_TYPE_##a
#define _GET_ATYPE(a, b) _ATTR_TYPE_##a
#define _GET_BUNDLE_TYPE(a, b) a##_BUNDLE_TYPE
#define _GET_DEFAULT(a, b) #b

#define GET_STRD_TYPE(xdomain, xitem) _CALL(_GET_TYPE, STRAND_##xdomain##_##xitem##_TYPE)
#define GET_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, STRAND_##xdomain##_##xitem##_TYPE)

#define GET_BUNDLE_TYPE(xdomain, xitem) _CALL(_GET_BUNDLE_TYPE, BUNDLE_##xdomain##_##xitem##_TYPE)
#define GET_BUNDLE_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, BUNDLE_##xdomain##_##xitem##_TYPE)

#define GET_ATTR_TYPE(xdomain, xitem) _CALL(_AGET_TYPE, ATTR_##xdomain##_##xitem##_TYPE)
#define GET_ATTR_DEFAULT(xdomain, xitem) _CALL(_GET_DEFAULT, ATTR_##xdomain##_##xitem##_TYPE)

#define JOIN(a, b) GET_STRD_TYPE(a, b) STRAND_##a##_##b  //STRAND_NAMEU(#a, #b)
#define JOIN2(a, b, c) GET_STRD_TYPE(a, b) STRAND_NAMEU(#a, #c)
#define JOIN3(a, b, c, d, e) GET_STRD_TYPE(a, b) STRAND_NAMEU(#d, #e) " " #c

#define AJOIN(a, b) GET_ATTR_TYPE(a, b) ATTR_##a##_##b  //STRAND_NAMEU(#a, #b)
#define AJOIN2(a, b, c) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#a, #c)
#define AJOIN3(a, b, c, d, e) GET_ATTR_TYPE(a, b) ATTR_NAMEU(#d, #e) " " #c

#define PJOIN3(a, b, c, d, e) GET_STRD_TYPE(a, b) EXTERN_NAMEU(#d, #e) " " #c
#define BJOIN3(a, b, c, d, e) GET_STRD_TYPE(a, b) BUNDLE_NAMEU(#d, #e) " " #c

#define _ADD_STRAND(domain, item) JOIN(domain, item)
#define _ADD_ASTRAND(domain, item) AJOIN(domain, item)
#define _ADD_STRANDn(domain, item, name) JOIN2(domain, item, name)
#define _ADD_ASTRANDn(domain, item, name) AJOIN2(domain, item, name)

#define _ADD_NAMED_PSTRAND(xd, xi, btype, domain, item) PJOIN3(xd, xi, btype, domain, item)
#define _ADD_NAMED_BSTRAND(xd, xi, btype, domain, item) BJOIN3(xd, xi, btype, domain, item)

#define _ADD_KSTRAND(xd, xi, btype, domain, item) JOIN3(xd, xi, btype, domain, item)
#define _ADD_OPT_STRAND(domain, item) "?" JOIN(domain, item)
#define _ADD_OPT_STRANDn(domain, item, name) "?" JOIN2(domain, item, name)
#define _ADD_OPT_ASTRAND(domain, item) "?" AJOIN(domain, item)
#define _ADD_OPT_ASTRANDn(domain, item, name) "?" AJOIN2(domain, item, name)

#define MS(...) make_strands("", __VA_ARGS__, NULL)
#define MSTY(...) make_strands(__VA_ARGS__, NULL)

  // local ptrs, scalar and array
#define _ADD_STRAND2(domain, item) "0 " GET_DEFAULT(domain, item)

#define _ADD_STRAND2a(domain, item) "1 " GET_DEFAULT(domain, item)

#define _ADD_STRAND2ak(kname) "1 " #kname

  // remote ptrs, scalar and array
#define _ADD_STRAND2p(domain, item) "2 " "((void *)0)"
#define _ADD_STRAND2ap(domain, item) "3 " "((void *)0)"
  // keyed array to remote ptrs, default specifies the key strand

#define _ADD_STRAND2ka(kname) "4 " #kname

  // ptr to remote array
#define _ADD_STRAND2pa(domain, item) ("4 " "((void *)0)")

#define ADD_CONSTP(td, ti, bt, d, i) MS(_ADD_NAMED_PSTRAND(td, ti, bt, d, i), _ADD_STRAND2p(td, ti))
#define ADD_CONSTPS(td, ti, bt, d, i) MS(_ADD_NAMED_PSTRAND(td, ti, bt, d, i), _ADD_STRAND2ap(td, ti))
#define ADD_CONST_ARR(td, ti, bt, d, i) MS(_ADD_NAMED_PSTRAND(td, ti, bt, d, i), _ADD_STRAND2pa(td, ti))

#define ADD_CONST_KARR(td, ti, bt, d, i, k) MS(_ADD_NAMED_PSTRAND(td, ti, bt, d, i), \
					       _ADD_STRAND2ka(k))

#define INC_KARR(td, ti, bt, d, i, k) MS(_ADD_KSTRAND(td, ti, bt, d, i), \
					 _ADD_STRAND2ak(k))
#define INC_BUN(td, ti, d, i, n) MS(_ADD_NAMED_BSTRAND(td, ti, GET_BUNDLE_TYPE(d,i), d, n), \
				    _ADD_STRAND2(td, ti))
#define INC_BARR(td, ti, d, i, n) MS(_ADD_NAMED_BSTRAND(td, ti,		\
							GET_BUNDLE_TYPE(d,i), d, n), \
				     _ADD_STRAND2a(td, ti))
  ////////////////////////// BUNDLEDEF "DIRECTIVES" ////////////////////////////////////////
  // + ATTRBUNDLE
#define ADD_STRAND(d, i)	       	 	    	MS(_ADD_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_ATTR(d, i)			 	    	MS(_ADD_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_NAMED_STRAND(d, i, n)      	       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_ARRAY(d, i) 			    	MS(_ADD_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_ARRAY(d, i, n) 	       	    	MS(_ADD_STRANDn(d,i,n),_ADD_STRAND2a(d,i))

#define ADD_OPT_STRAND(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2(d,i))
#define ADD_OPT_ATTR(d, i) 				MS(_ADD_OPT_ASTRAND(d,i),_ADD_ASTRAND2(d,i))
#define ADD_NAMED_OPT_STRAND(d, i, n)          	    	MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2(d,i))
#define ADD_OPT_NAMED_STRAND(d, i, n) 			ADD_NAMED_OPT_STRAND(d, i, n)
#define ADD_OPT_ARRAY(d, i) 				MS(_ADD_OPT_STRAND(d,i),_ADD_STRAND2a(d,i))
#define ADD_NAMED_OPT_ARRAY(d, i) 	       		MS(_ADD_OPT_STRANDn(d,i,n),_ADD_STRAND2a(d,i))
#define ADD_OPT_NAMED_ARRAY(d, i)			ADD_NAMED_OPT_ARRAY(d, i)

#define ADD_COND_ATTR(d, i, ...)     		      	MS(_ADD_ASTRAND(d,i),_ADD_ASTRAND2(d,i)), \
    NIRVA_COND_CREATE(__VA_ARGS__)

#define ADD_COMMENT(text)				MS("#" text)

  // include all strands from bundle directly
#define EXTEND_BUNDLE(BNAME) 				GET_BDEF(BNAME##_BUNDLE_TYPE)
#define EXTENDS_BUNDLE(BNAME)				EXTEND_BUNDLE(BNAME)

  // ptr to bundle and array of ptrs
#define INCLUDE_BUNDLES(domain, item)		     	INC_BARR(VALUE, BUNDLEPTR, \
								 domain, item, item)
#define INCLUDES_BUNDLES(domain, item)			INCLUDE_BUNDLES(domain, item)
#define INCLUDE_NAMED_BUNDLES(domain, item, name)      	INC_BARR(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLES(domain, item, name)    	INCLUDE_NAMED_BUNDLES(domain, item, name)

  // array of ptrs to sub bundles, each may be referred to by the unique strand defined
  // as the "default" for bundle_domain_item_type
  // together this creates a storage type container
#define INCLUDE_KEYED_ARRAY(btype, kname, name)      	INC_KARR(VALUE, KEYED_ARRAY, \
								 btype, ARRAY, name, kname)
#define INCLUDE_BUNDLE(domain, item)  		    	INC_BUN(VALUE, BUNDLEPTR, domain, item, item)
#define INCLUDES_BUNDLE(domain, item)			INCLUDE_BUNDLE(domain, item)
#define INCLUDE_NAMED_BUNDLE(domain, item, name)      	INC_BUN(VALUE, BUNDLEPTR, domain, item, name)
#define INCLUDES_NAMED_BUNDLE(domain, item, name)      	INCLUDE_NAMED_BUNDLE(domain, item, name)

  // IMPORTANT !!!
  // INCLUDE_BUNDLE(s) and ADD_CONST_BUNDLEPTRs both create pointers to bundles or arrays of these
  // THE DIFFERENCE BETWEEN INCLUDE_BUNDLE AND ADD_CONST_BUNDLEPTR is that with INCLUDE_BUNDLE,
  // the sub-bundle will be "owned", that is, when the containing bundle is freed, the sub-bundle
  // should also be freed (unreffed).
  // This is done simply by creating a strand in the bundle with type bundleptr (array)
  //
  // With ADD_CONST_BUNDLEPTR, the pointers are to external bundles. These MUST NOT be unreffed / freed
  // thus the implementation needs some way to differentiate these two types. A good way is
  // to set the pointers to NULL before freeing the bundle. In addition, each bundle has an optional
  // container (CONST !) pointer which can be checked, by followin the chain back to the root
  // to ensure it is part of the top level being freed. Finally, when creating the blueprint for
  // a bundle, there are flag bits which can be set to indicate Far Pointer, the blueprint should
  // be deleted last, and should be consulted when freeing the bundle.

#define ADD_CONST_BUNDLEPTR(domain, item, btype) ADD_CONSTP(VALUE, CONST_BUNDLEPTR, btype##_BUNDLE, domain, item)

  // this is an array of pointers to remote bundles...
#define ADD_CONST_BUNDLEPTRS(domain, item, btype) ADD_CONSTPS(VALUE, CONST_BUNDLEPTR, btype##_BUNDLE, domain, item)

  // ...while this is a single pointer to a remoter array
#define ADD_CONST_ARRAY_PTR(domain, item, btype) 	ADD_CONST_ARR(VALUE, CONST_BUNDLEPTR, \
								      btype##_BUNDLE, domain, item)

  // KEYED arrays are explained in the documentation. The implementation may support this
  // by defining IMPL_FUNC_lookup_item_in_array
  // otherwise an index sub bundle may be created and inserted in the bundle
  //

  // array of far_ptrs to internal or external bundles, each may be referred to by the
  // unique strand defined
  // as the "default" for bundle_domain_item_type
  // together this creates an "index" type container
#define ADD_KEYED_ARRAY(btype, kname, name) ADD_CONST_KARR(VALUE, KEYED_ARRAY, \
							   btype##_BUNDLE, ARRAY, name, kname)

  // hook automations - some bundles define automations, when creating the bundle
  // blueprint this implies adding directive "@HOOK_AUTO" ... "@HOOK_AUTO_END"
  // these automations will be "pending" until the strand is added to a bundle with
  // hook stacks, as there is nowhere to store the "triggers"
  // once a bundle is complete these automations will be "migrated" upwards,
  // it is essential that when a strand is added to a bundle

  //// PREDEFINED BUNDLEDEFS //////////////

  // ALL bundles with the exception of INTERFACE bundles (solely designed to be extended)
  // MUST extend this, either directly or indirectly by extension
  // (MUST include at least a bundle_type derived from the blueprint, and a unique ID
  // generated randomly when the bundle is created)
  // when added to a container bundle it is ESSENTIAL to set the container pointer
  // to point to the parent bundle. Many automations rely on this.
  //
  // it is recommended that all bundles store the "blueprint" or a pointer to it
  // (for static blueprints. The implementation can ask for this for example to
  // find out strand types and flags.
  // (IMPL_FUNC_nirva_bundle_request_blueprint)
  // other bundles may make optional elements mandatory (e.g. flags) with the directive
  // @MAKE_MANDATORY <strand_name> in the blueprint
#define _DEF_BUNDLE							\
    ADD_STRAND(BLUEPRINT, BUNDLE_TYPE),			\
      ADD_STRAND(GENERIC, UID),						\
      ADD_CONST_BUNDLEPTR(BUNDLE, CONTAINER, ANY),			\
      ADD_OPT_STRAND(SPEC, VERSION),					\
      ADD_OPT_STRAND(GENERIC, NAME),					\
      ADD_OPT_STRAND(GENERIC, DESCRIPTION),				\
      ADD_OPT_STRAND(GENERIC, FLAGS),					\
      INCLUDE_BUNDLE(INTROSPECTION, BLUEPRINT),				\
      ADD_CONST_BUNDLEPTR(INTROSPECTION, BLUEPRINT_PTR, BLUEPRINT),	\
      MAKE_EXCLUSIVE("BLUEPRINT", "BLUEPRINT_PTR")

  
  // bootstrap: - manually create blueprint for strand_def
  // - the string array based version created here will do
  //
  // create strand_defs from this and create blueprint for blueprint bundle
  // - This becomes the template for creating all other blueprints
  // the blueprint_ptr strand can be pointed at self

  // use the bprint template to create a "soft" version of strand_def blueprint
  // brpint and strand_def can now be used to construct blueprints for all remaining
  // bundle types

#define _BLUEPRINT_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLES(BLUEPRINT, STRAND_DEF)

#define _VALUE_BUNDLE ADD_STRAND(VALUE, STRAND_TYPE), ADD_OPT_STRAND(VALUE, DATA)
  
#define _STRAND_DEF_BUNDLE EXTEND_BUNDLE(DEF), MAKE_MANDATORY(GENERIC, FLAGS), \
    MAKE_MANDATORY(GENERIC, NAME), ADD_STRAND(VALUE, STRAND_TYPE),	\
    ADD_OPT_STRAND(VALUE, DEFAULT), MAKE_READONLY(VALUE, STRAND_TYPE)

  // when creating strands from a blueprint, they should be created like this.
  // template" should point to the strand_def template in the bundle blueprint
  // the "strand_type" extended from VALUE MUST be set to match the "strand_type"
  // of the template, "name" must be set from the template, and value data set from the default
  // (unless overriden). Most importantly, the strand_def has a "restrictions" bundle,
  // a set of conditions which specifies the precise data type. For example for bundleptr
  // it may have a condition like NIRVA_EQUALS, ATTR_UINT64_VAL STRAND_BLUEPRINT_BUNDLE_TYPE,
  //     ATTRIBUTE_BUNDLE_TYPE
  // the condition must be checked before adding data (actually it is a hook_automantion.
  // strictly speaking this would be a hook callback for the data_hook | FLAG_BEFORE.
  // however strands do not have hook_stacks 
  //
  // additionally this is a component of attribute value (attr_value)
  // mandatory nodes are name, type, and optional value.
  // alternately, this can also point to some native value / type / size
  // so act as an adaptor
#define _STRAND_BUNDLE ADD_STRAND(GENERIC, NAME), EXTEND_BUNDLE(VALUE),	\
    ADD_CONST_BUNDLEPTR(STRAND, TEMPLATE, STRAND_DEF),			\
    ADD_OPT_STRAND(INTROSPECTION, NATIVE_TYPE), ADD_OPT_STRAND(INTROSPECTION, NATIVE_SIZE)

  // (using strand / attr value mapping)
  // volatile attrubtes can also map onto native varables
  // NB. "name" will appear twice - this cannot be avoided, however, the validation step
  // should handle this and remove the optional duplicate
#define _ATTR_VALUE_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(STRAND)

  // may contain "before" and "after" values for data_hook, as well as being useful for
  // comparison functions
#define _VALUE_CHANGE_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLE(VALUE, OLD), INCLUDE_BUNDLE(VALUE, NEW)

  // maps a single native function paramater to an attr_desc
  // the attr_desc can be for an actual attribute to be created, eg when
  // calling a native function with data, or else just the type and flags
#define _PMAP_DEF_BUNDLE EXTEND_BUNDLE(ATTR_DEF), ADD_STRAND(FUNCTION, PARAM_NUM)

  // the "live" version of the preceding, we now map the paramter to an attribute,
  // usually this would be part of an attr_container accompanyiong the function / transform
#define _PMAP_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(PMAP, TEMPLATE, PMAP_DEF), \
    ADD_CONST_BUNDLEPTR(LINKED, ATTRIBUTE, ATTTRIBUTE)

  // TODO
#define _ERROR_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(ERROR, SOURCE, FUNCTIONAL)

#define _CONDVAL_NODE_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLE(AUTOMATION, CONDITION), \
    ADD_CONST_BUNDLEPTR(CASCADE, CONSTVAL, CONSTVAL_MAP),		\
    ADD_CONST_BUNDLEPTR(NEXT, SUCCESS, CONDVAL_NODE),			\
    ADD_CONST_BUNDLEPTR(NEXT, FAIL, CONDVAL_NODE), ADD_NAMED_OPT_STRAND(VALUE, DOUBLE, P_SUCCESS), \
    ADD_NAMED_OPT_STRAND(VALUE, DOUBLE, P_FAIL), ADD_NAMED_OPT_STRAND(VALUE, DOUBLE, WEIGHT)

#define _CONDLOGIC_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(LOGIC, OP),	\
    ADD_CONST_BUNDLEPTR(CONDITON, CONDA, CONDVAL_NODE), ADD_CONST_BUNDLEPTR(LOGIC, CONDB, CONDVAL_NODE)

  // one of these may have NULL for condlogic, this defines a default value
#define _CONSTVAL_MAP_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(CASCADE, VALUE), \
    INCLUDE_BUNDLES(CASCADE, CONDLOGIC), ADD_STRAND(CASCMATRIX, OP_SUCCESS), \
    ADD_STRAND(CASCMATRIX, OP_FAIL)

#define _CASCMATRIX_NODE_BUNDLE EXTEND_BUNDLE(DEF),			\
    ADD_CONST_BUNDLEPTR(CASCMATRIX, OTHER_IDX, CASCMATRIX_NODE),	\
    INCLUDE_BUNDLE(CASCMATRIX, ON_SUCCESS), INCLUDE_BUNDLE(CASCMATRIX, ON_FAIL)

  // in this case bundleptrs to bundles of type connection
#define _INDEX_BUNDLE ADD_STRAND(SPECIAL, OPT_ANY), ADD_CONST_BUNDLEPTRS(INDEX, POINTERS, ANY)

  // the automation (if enabled) will add callbacks to the inited_value hook for the
  // array which will create a connection_bundle pointing to the item and add it to the
  // key_lookup bundle. It will also intercept data_request and freeing hooks
  // the selector "indexer" can be used to return a set of values matching conditions
  // e.g. if the key is "name", then the selector can find element matching
  // NIRVA_VAL_EQUALS, _NIRVA_STRAND_VAL "name", NIRVA _STRING_VAL name 
#define _LOOKUP_BUNDLE INCLUDE_BUNDLE(STANDARD, INDEX),	\
    INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, INDEXER)

  
  // a bundle used to return array values. contains a (const) bundleptr array
  // a count (total) a "current" ptr,
#define _SUPER_ARRAY_BUNDLE EXTEND_BUNDLE(DEF), ADD_NAMED_ARRAY(VALUE, BUNDLEPTR, ARRAY), \
    ADD_STRAND(VALUE, COUNT), INCLUDE_NAMED_BUNDLE(AUTOMATION, CONDITION, ACCEPT_CONDITIONS)

#define _KEYED_NAME_BUNDLE EXTEND_BUNDLE(INDEX), ADD_KEYED_ARRAY(ANY_BUNDLE, NAME, ENTRIES)

#define _KEYED_UID_BUNDLE EXTEND_BUNDLE(INDEX), ADD_KEYED_ARRAY(ANY_BUNDLE, UID, ENTRIES)

#define _MATRIX_2D_BUNDLE EXTEND_BUNDLE(DEF), ADD_NAMED_ARRAY(VALUE, BUNDLEPTR, VALUES)

#define _CASCADE_BUNDLE EXTEND_BUNDLE(DEF), MAKE_MANDATORY(GENERIC, NAME), \
      MAKE_MANDATORY(GENERIC, FLAGS),					\
      INCLUDE_BUNDLES(CASCADE, DECISION_NODE),				\
      INCLUDE_BUNDLES(CASCADE, CONSTVAL_MAP),				\
      INCLUDE_BUNDLES(STANDARD, MATRIX_2D),				\
      INCLUDE_NAMED_BUNDLE(STANDARD, MATRIX_2D, CORRELATIONS)

  // this bundle describes an attribute, but is "inert" - has no value or connections
  // some attribute types map to strand types and vice versa  but the values are not the same
  // when constructed, we have three "value types" - "attr_type", "strand_type", and "native_type"
  
#define _ATTR_DEF_BUNDLE EXTEND_BUNDLE(DEF), MAKE_MANDATORY(GENERIC, FLAGS), \
    ADD_STRAND(ATTRIBUTE, ATTR_TYPE), ADD_OPT_STRAND(VALUE, MAX_VALUES), \
    ADD_OPT_STRAND(VALUE, DEFAULT), ADD_OPT_STRAND(VALUE, NEW_DEFAULT)

  // all data which is not bundle_strands, is held in attributes. They come in a wide variety
  // of types, can map underlying values, as well as be connected remotely to other attributes
  // they can map to and from function paramters, can hold sacalar values or arrays,
  // contain sub-bundles or point to external bundles. They can be readonly, readwrite,
  // volatile, const, optional, mandatory. They usually come in bundles - attr_packs,
  // and can be constructed from attr_desc, or attr_deesc container.
  // objects have their own attr bundles, and they are also carried around as functional
  // inputs and outputs. Scripts and conditions may also have a referenced attr bundle
#define _ATTRIBUTE_BUNDLE EXTEND_BUNDLE(ATTR_DEF), INCLUDE_BUNDLE(STANDARD, ATTR_VALUE), \
    ADD_CONST_BUNDLEPTR(PARENT, TEMPLATE, ATTR_DEF),			\
    INCLUDE_NAMED_BUNDLE(ATTRIBUTE, CONNECTION, CONNECTION_OUT),	\
    INCLUDE_BUNDLE(INTROSPECTION, REFCOUNT), INCLUDE_BUNDLES(ATTRIBUTE, HOOK_STACKS)

  // NB: for connected attrs
  // when reading the data from the local attribute, first lock remote, then if non-null,
  // lock remote.connection, then read "data", unlock remote.connection, then unlock remote

#define _COND_GEN_002 _COND_FUNC_RET NIRVA_MAKE_REQUEST, ATTACH, _COND_ATTR_VAL(NEW_VAL)

  // read / write connections are possible but require an attach request
  // the hook atack here is for the detaching hook, which connectors may use to
  // detatch their end, these callbacks should be called if the attribute is about to be deleted
#define _ATTR_CONNECTION_BUNDLE EXTEND_BUNDLE(DEF), MAKE_MANDATORY(GENERIC, FLAGS), \
    ADD_CONST_BUNDLEPTR(REMOTE, CONNECTION, ATTRIBUTE)

  // when calling a functional, adding a transform to a queue etc.
  // we must set the caller object and source item (e.g. an attribute or strand whose value
  // changed)
#define _EMISSION_BUNDLE ADD_CONST_BUNDLEPTR(INPUT, TARGET_OBJECT, OBJECT), \
    ADD_CONST_BUNDLEPTR(INPUT, CALLER_OBJECT, OBJECT),			\
    INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, TARGET_DEST),		\
    INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, CALLER_SOURCE),		\
    ADD_NAMED_OPT_STRAND(DATETIME, TIMESTAMP, TRANSMIT_TIME)

  // callback type added to a REQUEST queue
#define _REQUEST_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(EMISSION), ADD_OPT_STRAND(HOOK, NUMBER), \
      ADD_OPT_STRAND(HOOK, HANDLE), MAKE_MANDATORY(GENERIC, FLAGS),	\
      ADD_NAMED_OPT_STRAND(DATETIME, DELTA, REPEAT),			\
      ADD_NAMED_OPT_STRAND(DATETIME, TIMESTAMP, EXEC_TARGET),		\
      ADD_NAMED_OPT_STRAND(VALUE, CONST_BUNDLEPTR, DETAILS)

#define _FN_INPUT_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(EMISSION),	\
    ADD_CONST_BUNDLEPTR(HOOK, DETAILS, HOOK_DETAILS),			\
    INCLUDE_BUNDLES(FUNCTION, PMAP_IN),					\
    ADD_CONST_BUNDLEPTR(AUTOMATION, SCRIPT, SCRIPTLET),			\
    INCLUDE_BUNDLE(VALUE, CHANGE), ADD_CONST_BUNDLEPTR(LINKED, CONTRACT, CONTRACT), \
    ADD_NAMED_OPT_STRAND(VALUE, VOIDPTR, USER_DATA),			\
    ADD_CONST_BUNDLEPTR(INPUT, ATTRIBUTES, ATTR_GROUP)

  // this is the minimal output from a FUNCTION. RESPONSE is COND_FAIL, COND_SUCCESS etc for
  // a condition, REQUEST_YES, REQUEST_NO, etc for a request hook
  // for other hook callbacks response is LAST or AGAIN
  // for Transforms the repsonse for the extend bundle is a transform result
#define _FN_OUTPUT_BUNDLE EXTEND_BUNDLE(DEF),				\
      ADD_NAMED_OPT_STRAND(VALUE, BUNDLEPTR, CASCADE_VALUE),		\
      ADD_ARRAY(FUNCTION, RESPONSE), ADD_CONST_BUNDLEPTR(OUTPUT, ATTRIBUTES, ATTR_GROUP), \
      INCLUDE_BUNDLES(FUNCTION, PMAP_OUT)

#define _LOCATOR_BUNDLE EXTEND_BUNDLE(DEF),				\
    ADD_OPT_STRAND(LOCATOR, UNIT), ADD_OPT_STRAND(LOCATOR, SUB_UNIT),	\
    ADD_OPT_STRAND(LOCATOR, INDEX), ADD_OPT_STRAND(LOCATOR, SUB_INDEX)

  // this describes the non-data part of a "functional"
  // ext_uid can be used as an external reference to allow for selection,
  // indexing or runtime definitions
  // category denotes the purpose,
  // alternate mappings can be provided with a variety of mappings, wrappers and outputs
  // the selector MAP_CHOOSER can be set up to find the best mapping, for example
  // it can select by deired wrapping, avaialbility or not of inputs etc.
#define _FUNC_DEF_BUNDLE EXTEND_BUNDLE(DEF),			\
      ADD_NAMED_OPT_STRAND(VALUE, UINT64, EXT_ID),		\
      ADD_STRAND(FUNCTION, CATEGORY),				\
      INCLUDE_NAMED_BUNDLE(STANDARD, ATTR_DEF_HOLDER, INPUT),	\
      INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, MAP_CHOOSER),	\
      INCLUDE_NAMED_BUNDLES(STANDARD, FUNC_MAP, MAPPINGS)

  // each func desc can have various mappings
#define _FUNC_MAP_BUNDLE					\
  EXTEND_BUNDLE(DEF),						\
    ADD_OPT_STRAND(FUNCTIONAL, WRAPPING),			\
    INCLUDE_NAMED_BUNDLE(STANDARD, ATTR_DEF_HOLDER, OUTPUTS),	\
    INCLUDE_NAMED_BUNDLES(STANDARD, PMAP_DEF, PMAPPING),	\
    INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, SCRIPT),		\
    ADD_NAMED_OPT_STRAND(STANDARD, SELECTOR, FUNCTIONAL),	\
    ADD_NAMED_OPT_STRAND(VALUE, FUNCPTR, NATIVE_FUNCTION),	\
    INCLUDE_NAMED_BUNDLE(STANDARD, LOCATOR, CODE_REF)

#define _FUNCTIONAL_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(FUNCTION, CATEGORY), \
      ADD_CONST_BUNDLEPTR(FUNCTIONAL, TEMPLATE, FUNC_MAP),		\
      INCLUDE_BUNDLE(FUNCTION, INPUT), INCLUDE_BUNDLE(FUNCTION, OUTPUT)

  // scriptlet is an array of strings with pseudocode to be parsed at runtime
  // the types here are function categories
  // - condition, selector, automation, callback
#define _SCRIPTLET_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(FUNCTION, CATEGORY), \
    ADD_ARRAY(SCRIPTLET, STRINGS)

#define _FUNC_DEF_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX), \
    ADD_KEYED_ARRAY(FUNC_DEF, NAME, ENTRIES)
  //
  // hook callbacks nay be added here instead of for
  // individual attributes
  // the changes will be "bunched" together and only VALUE_UPDATED triggered
  // in addition there will be stacks for appended, removed
#define _ATTR_GROUP_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX),	\
      ADD_KEYED_ARRAY(ATTR_DEF, NAME, ATTRIBUTES), INCLUDE_BUNDLES(ATTRIBUTE, HOOK_STACKS)

#define _ATTR_DEF_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX), \
    ADD_KEYED_ARRAY(ATTR_DEF, NAME, ATTRS)

#define _OBJECT_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX),	\
    ADD_KEYED_ARRAY(OBJECT, UID, OBJECTS)

#define _OFFSPRING_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX), \
    ADD_KEYED_ARRAY(OBJECT, UID, OFFSPRING)

#define _SCRIPTLET_HOLDER_BUNDLE EXTEND_BUNDLE(DEF), EXTEND_BUNDLE(INDEX), \
    ADD_KEYED_ARRAY(SCRIPTLET, UID, SCRIPTS)

  // each function will be called in the manner depending on the flags in the corresponding stack_header
  // for native_funcs, PMAP also comes from the hook stack header
  // timestamp may be set when the added to the queue
#define _HOOK_CB_FUNC_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, HANDLE), \
    ADD_NAMED_OPT_STRAND(DATETIME, TIMESTAMP, TIME_ADDED),		\
    ADD_STRAND(HOOK, CB_DATA), INCLUDE_BUNDLE(STANDARD, REQUEST),	\
    INCLUDE_BUNDLE(STANDARD, FUNCTIONAL)

  // details defined at setup for each  hook number
  // these are placed in a cascade so conditions translate to hook number
#define _HOOK_DETAILS_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, TYPE), ADD_STRAND(HOOK, NUMBER), \
    MAKE_MANDATORY(GENERIC, FLAGS), INCLUDE_BUNDLES(FUNCTION, MAPPING), \
    INCLUDE_BUNDLES(AUTOMATION, CONDITION)

#define _HOOK_STACK_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(HOOK, NUMBER), \
    INCLUDE_NAMED_BUNDLE(STANDARD, SELECTOR, TARGET), INCLUDE_BUNDLES(HOOK, CALLBACK), \
    ADD_CONST_BUNDLEPTR(AUTOMATION, SCRIPT, SCRIPTLET)

#define _OBJECT_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(OBJECT, TEMPLATE, OBJECT_TEMPLATE), \
      ADD_CONST_BUNDLEPTR(OBJECT, INSTANCE, OBJECT_INSTANCE), MAKE_EXCLUSIVE("TEMPLATE", "INSTANCE")

  // for templates the subtype is always SUBTYPE_NONE, and the state wiil be STATE_NONE */
  // hook stacks should be created when an item is added ffor data_change of any strand
  // (before and after) plus init and free, plus the idle_hook stack
  // plus destroy_hook
#define _OBJECT_TEMPLATE_BUNDLE EXTEND_BUNDLE(DEF),			\
    ADD_OPT_STRAND(INTROSPECTION, COMMENT),				\
    ADD_OPT_STRAND(INTROSPECTION, PRIVATE_DATA),			\
    ADD_STRAND(OBJECT, TYPE),						\
    ADD_STRAND(OBJECT, SUBTYPE), ADD_STRAND(OBJECT, STATE),		\
    INCLUDE_NAMED_BUNDLE(STANDARD, ATTR_GROUP, ATTRIBUTES),		\
    INCLUDE_BUNDLES(OBJECT, CONTRACTS),					\
    INCLUDE_NAMED_BUNDLES(OBJECT, ACTIVE_TRANSFORMS, TRANSFORMS),	\
    INCLUDE_BUNDLES(OBJECT, HOOK_STACKS), MAKE_MANDATORY(GENERIC, FLAGS)

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

#define _REFCOUNT_BUNDLE ADD_STRAND(INTROSPECTION, REFCOUNT),		\
    ADD_NAMED_STRAND(THREADS, MUTEX, COUNTER_MUTEX), ADD_NAMED_STRAND(VALUE, BOOLEAN, SHOULD_FREE), \
    ADD_NAMED_STRAND(THREADS, MUTEX, DESTRUCT_MUTEX), ADD_NAMED_ARRAY(UINT64, ADDERS)

  // base bundle for object instances
  /* subtype and state are created as ATTRIBUTES so we can add hooks to them */
  // these bundles should not be created directly, a template object should do that via a CREATE_INSTANCE intent
  // or an instance may create a copy of itself vie the CREATE_INSTANCE intent.
  // object instances must also have contracts for the ADD_REF and UNREF intents, which should be flagged
  // no-negotiate. The transform for these should simply increase the refcount, or decreas it respectively.
  // If the refcount falls below its default value, the bundle should be freed
  // we add hook_triggers for subtype and state - since these are simple strandnts the triggers
  // will embed in the instance bundle
  // the hook stacks are for request and spontaneous hook types
  // passive lifecycle

#define _OBJECT_INSTANCE_BUNDLE ADD_CONST_BUNDLEPTR(OBJECT, TEMPLATE, OBJECT_TEMPLATE), \
    EXTEND_BUNDLE(OBJECT_TEMPLATE), INCLUDE_BUNDLE(INTROSPECTION, REFCOUNT), \
    INCLUDE_NAMED_BUNDLES(THREAD, INSTANCE, THREAD_INSTANCE)

  /* // a list 'header' with owner uid and pointer to attr_list_bundle */
#define _OWNED_ATTRS_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(OWNER, OBJECT, OBJECT), \
    ADD_CONST_BUNDLEPTR(LIST, DATA, ATTR_GROUP)

#define _ATTR_POOL_BUNDLE EXTEND_BUNDLE(DEF), ADD_CONST_BUNDLEPTR(LIST, NEXT, ATTR_POOL), \
    ADD_CONST_BUNDLEPTR(OWNED, LIST, OWNED_ATTRS)

#define _CAP_BUNDLE ADD_STRAND(GENERIC, NAME)

  /* // intent / capacities bundle */
#define _ICAP_BUNDLE EXTEND_BUNDLE(DEF), ADD_STRAND(ICAP, INTENTION),	\
    MAKE_MANDATORY(GENERIC, NAME), EXTEND_BUNDLE(INDEX), ADD_KEYED_ARRAY(CAP, NAME, CAPS)
  
#define _TSEGMENT_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLES(TRAJECTORY, FUNCTIONAL), \
    INCLUDE_NAMED_BUNDLE(STANDARD, CASCADE, NEXT_SEGMENT), INCLUDE_BUNDLES(TRAJECTORY, SEGMENT)

#define _TRAJECTORY_BUNDLE EXTEND_BUNDLE(DEF), INCLUDE_BUNDLES(TRAJECTORY, SEGMENT), \
    ADD_CONST_BUNDLEPTRS(OBJECT, CONTRACTS, CONTRACT)


  // automations will be included in blueprint instances, depending on the bundle_type
  // they produce - if the bundle has its own hook stacks, the automations will be transferred
  // otherwise they will be added to the blueprint instance hook stacks
  // when reaching a hook trigger point, if a bundle does not have hook stacks (for smaller bundles
  // like value), then the blueprint should be used as a proxy
  
  // automations for blueprint instances, per bundle_type not sure how these will be assigned yet
  // will be transferred to instance hooks

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

  /* #define LOOKUP_AUTOMATIONS\ */
  /*   ADD_HOOK_AUTOMATION(INDEX, DELETING_STRAND,				\ */
  /* 		      _COND_ALWAYS, REMOVE_FROM_ARRAY, *ARRAY, *OLD_VALUE), \ */
  /*     ADD_HOOK_AUTOMATION(INDEX, STRAND_ADDED, _COND_ALWAYS, ADD_HOOK_AUTOMATION, \ */
  /* 			NEW_VALUE, UPDATING_VALUE, _COND_ALWAYS, PROXY_TO, *!SELF, DATA) */
  /*   /\* ADD_HOOK_AUTOMATION(BUNDLE_KEYED_ENTRIES, ITEM_APPENDED, _COND_ALWAYS, \ *\/ */
  /*   /\* 			ADD_STRAND_TO, KEY_LOOKUP, NEW_VALUE.NAME, CONST_BUNDLEPTR, NEW_VALUE), \ *\/ */
  /*   /\*   ADD_HOOK_AUTOMATION(BUNDLE_KEYED_ENTRIES, ITEM_REMOVED, _COND_ALWAYS, \ *\/ */
  /*   /\* 			DELETE_STRAND_FROM, KEY_LOOKUP, OLD_VALUE.NAME) *\/ */

  /* #define REFCOUNT_AUTO \ */
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

#ifdef DEBUG_BUNDLE_MAKER
#define p_debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define p_debug(...)
#endif

#define __IMPL_EQUIVALENCE_TEST_NUMERIC__ ==
#define __IMPL_OR__ ||
#define __IMPL_AND__ &&
#define __IMPL_NOT__ !
#define __IMPL_ASSIGN__ =
#define __IMPL_SET_BIT__ |=
#define __IMPL_UNSET_BIT__(a,bit) &= ~
#define __IMPL_AND_BITS__ &
#define __IMPL_OR_BITS__ |
#define __IMPL_XOR_BITS__ ^
#define __IMPL_COMPL_BITS__ ~

#define __IMPL_HAS_BIT__(a,bit) (((a) __IMPL_AND_BITs__ (b))		\
				 __IMPL_EQUIVALENCE_TEST_NUMERIC__ (b))
#define __IMPL_JOIN_BIT_(b) __IMPL_OR_BITS__ (b)
#define __IMPL_PAREN_START__ (
#define __IMPL_PAREN_END__ )
#define __IMPL_BLOCK_START__ {
#define __IMPL_BLOCK_END__ }
#define __IMPL_WS_INDENT__
#define __IMPL_JOIN_BITS_1__(a) __IMPL_PARENS_START__ (a) __IMPL_PARENS_END__
#define __IMPL_JOIN_BITS_2__(a,b) __IMPL_PARENSS_START__ (a)	\
    __IMPL_JOIN_BIT__(b) __IMPL_PARENS_END__
#define __IMPL_DECLARE_AS_TYPE__(xtype,a) xtype a
#define __IMPL_STAT_ARRAY_VARSIZE__(a) a[]
#define __IMPL_STAT_ARRAY_NSIZE__(xtype,a,n) xtype a[n]
#define __IMPL_POINTER_TO__(a) *a
#define __IMPL_BYREF__(a) (&(a))
#define __IMPL_DEREF__(a) (*(a))
#define __IMPL_ARRAY_CAST__(xtype) (xtype*)
#define __IMPL_ARRAY_NTH__(a,idx) ((a)[(idx)])
#define __IMPL_CAST_TO_TYPE__(a, atype) ((atype) (a))
#define __IMPL_CAST_TO_GEN_PTR__(p)_ __IMPL_CAST_TO_TYPE__(p,__IMPL_GEN_PTR__)
#define __IMPL_SIZEOF__(a) (sizeof((a)))
#define __IMPL_TYPEOF__(a) (typeof((a)))
#define __IMPL_SET_UNDEF_PTR__(a) (a) = NULL
#define __IMPL_UNDEF_CHECK_PTR__(a) ((a) == NULL)
#define __IMPL_SET_UNDEF_INT__ (a) = 0
#define __IMPL_UNDEF_CHECK_INT__(a) (!((a)))
#define __IMPL_STRING_LENGTH__(str) (strlen((str)))
#define __IMPL_STRING_LENGTH_TYPE__ size_t
#define __IMPL_STRINGS_EQUAL__(str1,str2) (!strcmp((str1), (str2)))
#define __IMPL_CHAR_EQUAL__(a,c) ((a) == (c))
#define __IMPL_NTH_CHAR__(str, n) ((str)[(n)])
#define __IMPL_FINAL_CHAR__ '\0'
#define __IMPL_IS_FINAL_CHAR__(c) (__IMPL_CHAR_EQUAL__((c), __IMPL_FINAL_CHAR__))
#define __IMPL_IS_EMPTY_STRING__(str) (!(*(str))
#define __IMPL_BASIC_ALLOC__(size) malloc((size))
#define __IMPL_BASIC_UNALLOC__(ptr) free(ptr)
#define __IMPL_CALL_FUNCPTR__(func, ...) (*func)(__VA_ARGS__)

#define _CALL_n(macro, n, ...) macro_##n(__VA_ARGS__)

#define NIRVA_CMD(a) a;

#define NIRVA_FUNC_END }

#define NIRVA_ASSIGN(var, val, ...) NIRVA_CMD(var = val)
#define NIRVA_ASSERT(val, func, ...) NIRVA_UNLESS(val,func(__VA_ARGS__))
#define NIRVA_ASSERT_NULL(val, func, ...) NIRVA_UNLESS(NIRVA_VAL_EQUALS((val),NIRVA_NULL),func(__VA_ARGS__))
#define NIRVA_RETURN(res, ...) return res;
#define NIRVA_ARRAY_NTH(array,idx) array[idx]
#define NIRVA_ASSIGN_FROM_ARRAY(val, array, idx) NIRVA_ASSIGN(val,NIRVA_ARRAY_NTH(array,idx))

#define IMPL_FUNC_CALL(name,...)__IMPL_CALL_FUNCPTR__(impl_func_##name,__VA_ARGS__)
#define INTERN_FUNC_CALL(name,...)_INTERNAL_FUNC_##name(__VA_ARGS__)
#define INLINE_FUNC_CALL(name,...)NIRVA_INLINE(__INLINE_INTERNAL_##name,__VA_ARGS__)
#define MACRO_FUNC_CALL(name,...)_CALL(_INLINE_MACRO,_##name,__VA_ARGS__)

  // we have several function types to choose from:
  // IMPLementation funcs which may be external or internal
  // INTernal funcs
  // INLINE macros
  // - more coming..funcforms, scriptlets, and of course transforms
#define NIRVA_IMPL_ASSIGN(var, name,...) NIRVA_CMD((var) = __IMPL_CALL_FUNCPTR__(impl_func_##name, \
										 __VA_ARGS__))
#define NIRVA_INTERN_ASSIGN(var, name, ...) NIRVA_CMD((var) = INTERN_FUNC_CALL(name, __VA_ARGS__))
#define NIRVA_INLINE_ASSIGN(var, name, ...) NIRVA_CMD(INLINE_FUNC_CALL(name, var, __VA_ARGS__))
#define NIRVA_MACRO_ASSIGN(var, name, ...) NIRVA_CMD(_INLINE_MACRO_##name(var, __VA_ARGS__))

#define MK_CALL_METHOD(meth,var,name,...) NIRVA_##meth##_ASSIGN(var,name,__VA_ARGS__)
#define MK_CALL_METHODX(meth,name,...) meth##_FUNC_CALL(name,__VA_ARGS__)
#define NIRVA_CALL_METHOD(var,name,...) _CALL(MK_CALL_METHOD,NIRVA_CALL_METHOD_##name,var, \
					      name,__VA_ARGS__)
#define NIRVA_CALL_METHODX(name,...) _CALL(MK_CALL_METHODX,NIRVA_CALL_METHOD_##name, \
					   name,__VA_ARGS__)
#define NIRVA_RESULT(name,...) NIRVA_CALL_METHODX(name,__VA_ARGS__)
#define NIRVA_CALL(name,...) NIRVA_CMD(NIRVA_CALL_METHODX(name,__VA_ARGS__))
#define NIRVA_CALL_ASSIGN(var,name,...) NIRVA_CALL_METHOD(var,name,__VA_ARGS__)
#define NIRVA_ASSIGN_CALL(var,name,...) NIRVA_CALL_ASSIGN(var,name,__VA_ARGS__)

#define NIRVA_ARRAY_FREE(array) NIRVA_FREE(array);
#define NIRVA_COND_RESPONSE(r) (r ? NIRVA_COND_SUCCESS : NIRVA_COND_FAIL)

#define NIRVA_DO do {
#define NIRVA_DO_A_THING(thing) do {thing
#define NIRVA_WHILE(x)} while(x)__IMPL_LINE_END__
#define NIRVA_ONCE NIRVA_WHILE(0)
#define NIRVA_INLINE(_this_) NIRVA_DO_A_THING(_this_)NIRVA_ONCE
#define NIRVA_IF(a) if (a)
#define NIRVA_OP_NOT(op,a,b) (!(op(a,b)))

#define NIRVA_IF_NOT(a, ...) NIRVA_IF(NIRVA_VAL_NOT(a)) NIRVA_INLINE(__VA_ARGS__)
#define NIRVA_UNLESS(a,...) NIRVA_IF_NOT(a,__VA_ARGS__)
#define NIRVA_IF_OP(op, a, b, ...) NIRVA_IF(_CALL(op,a,b)) NIRVA_INLINE(__VA_ARGS__)
#define NIRVA_UNLESS_OP(op, a, b, ...) NIRVA_IF(NIRVA_OP_NOT(_CALL(op,a,b)) \
  NIRVA_INLINE(__VA_ARGS__)
#define NIRVA_IF_EQUAL(a,b,...) NIRVA_IF_OP(NIRVA_VAL_EQUALS,a,b,__VA_ARGS__)
#define NIRVA_IF_NOT_EQUAL(a,b,...) NIRVA_IF(NIRVA_OP_NOT(NIRVA_VAL_EQUALS,a,b)) NIRVA_INLINE(__VA_ARGS__)

#define NIRVA_MACRO_CALL(macro,...) _CALL(macro, __VA_ARGS__)
#define NIRVA_CALL_IF(op, a, b, macro, ...) NIRVA_IF_OP(op,a,b,NIRVA_MACRO_CALL(macro,__VA_ARGS__)
#define NIRVA_CALL_UNLESS(op, a, b, macro, ...) NIRVA_UNLESS_OP(op,a,b,NIRVA_MACRO_CALL(macro, \
											__VA_ARGS__)
#define NIRVA_CALL_IF_EQUAL(a,b,macro, ...) NIRVA_IF_EQUAL(a,b,NIRVA_MACRO_CALL(macro,__VA_ARGS__))
#define NIRVA_CALL_FUNC_IF_EQUAL(a,b,func, ...) NIRVA_IF_EQUAL(a,b) NIRVA_CALL(func,__VA_ARGS__)
#define NIRVA_CALL_IF_NOT_EQUAL(a,b,macro, ...) NIRVA_UNLESS_OP(NIRVA_VAL_EQUALS,a,b, \
								NIRVA_MACRO_CALL(macro,__VA_ARGS__))

#define NIRVA_FUNCPTR nirva_native_function_t

#endif // end cstyle

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
    NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_thread_instance_t)
    NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_bundle_t)
    NIRVA_TYPEDEF(NIRVA_BUNDLE_T, nirva_attr_connect_t)

    NIRVA_FUNC_TYPE_DEF(NIRVA_FUNC_RETURN, nirva_function_t, NIRVA_CONST_BUNDLEPTR input, \
			NIRVA_CONST_BUNDLEPTR output)
    NIRVA_FUNC_TYPE_DEF(NIRVA_NO_RETURN, nirva_native_function_t,)

    NIRVA_TYPEDEF(nirva_function_t, nirva_condfunc_t)
    NIRVA_TYPEDEF(nirva_function_t, nirva_callback_t)

  // flexi call, calling _INTERNAL_FUNC(fname, ...) will call _INTENAL_FUNC_fname()
  // which can be either a macro or a func

#define NIRVA_DEF_FUNC(RET_TYPE, funcname, ...) RET_TYPE funcname(__VA_ARGS__) {
#define NIRVA_DEF_FUNC_INTERNAL(ret_type, funcname, ...) NIRVA_STATIC_INLINE ret_type \
    _INTERNAL_FUNC_##funcname(__VA_ARGS__) {

  // the core funcs are the minimal set needed for bootstrap
#define NIRVA_CORE_IMPL_FUNC(funcname, desc, ret_type, ...)		\
    NIRVA_EXTERN ret_type impl_func_##funcname(__VA_ARGS__)__IMPL_LINE_END__

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
    NIRVA_FUNC_TYPE_DEF(rt, ximpl_func_##fname##_t,__VA_ARGS__) ximpl_func_##fname##_t \
      impl_func_##fname
#define NIRVA_NEEDS_IMPL_FUNC6(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,pt4,p4,pt5,p5) \
  NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3,pt4,pt5)
#define NIRVA_NEEDS_IMPL_FUNC5(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,pt4,p4) \
  NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3,pt4)
#define NIRVA_NEEDS_IMPL_FUNC4(fname,rt,pt0,p0,pt1,p1,pt2,p2,pt3,p3,...) \
    NIRVA_NEEDS(fname,rt,pt0,pt1,pt2,pt3)
#define NIRVA_NEEDS_IMPL_FUNC3(fname,rt,pt0,p0,pt1,p1,pt2,p2,...)	\
    NIRVA_NEEDS(fname,rt,pt0,pt1,pt2)
#define NIRVA_NEEDS_IMPL_FUNC2(fname,rt,pt0,p0,pt1,p1,...) NIRVA_NEEDS(fname,rt,pt0,pt1)
#define NIRVA_NEEDS_IMPL_FUNC1(fname,rt,pt0,p0,...) NIRVA_NEEDS(fname,rt,pt0)
#define NIRVA_NEEDS_IMPL_FUNC0(fname,rt,x) NIRVA_NEEDS(fname,rt,)

#define NIRVA_NEEDS_IMPL_FUNC(funcname, desc, ret_type, rt_desc, nparams, ...) \
    NIRVA_NEEDS_IMPL_FUNC##nparams(funcname, ret_type,__VA_ARGS__);

#define NIRVA_DEF_OPT_FUNC(funcname, desc, ret_type, rt_desc, nparams, ...) \
  NIRVA_NEEDS_IMPL_FUNC##nparams(funcname, ret_type,__VA_ARGS__) = IMPL_FUNC_##funcname;

#define NIRVA_NEEDS_EXT_COND_FUNC(funcname, desc, cond, ret_type, rt_desc, nparams, ...) \
    NIRVA_NEEDS_IMPL_FUNC##nparams(funcname, ret_type,__VA_ARGS__);

  ///list of functions which the implementation must define////
  // these can be set between nirva_init and nirva_prep.
  // changing any of these after nirva_prep requires PRIV_STRUCT > 100

#define NIRVA_CALL_METHOD_create_bundle_by_type IMPL
#define NIRVA_MAND_FUNC_001 create_bundle_by_type,"create and return a bundle given a bundle_type and" \
    " 'item_name','value' pairs",NIRVA_BUNDLEPTR,new_bundle,2, NIRVA_UINT64, \
    bundle_type, NIRVA_VARIADIC,

#define NIRVA_CALL_METHOD_create_bundle_from_bdef IMPL
#define NIRVA_MAND_FUNC_001a create_bundle_from_bdef,"create and return a bundle " \
    "given a bundle_def and 'item_name','value' pairs",NIRVA_BUNDLEPTR,new_bundle,2, \
    NIRVA_BUNDLEDEF, bdef, NIRVA_VARIADIC,

#define NIRVA_CALL_METHOD_nirva_value_set IMPL
#define NIRVA_MAND_FUNC_002 nirva_value_set,"set the value of a strand", \
    NIRVA_NO_RETURN,,4,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name,NIRVA_UINT, \
    strand_type,NIRVA_VA_LIST,data

#define NIRVA_CALL_METHOD_nirva_array_append IMPL
#define NIRVA_MAND_FUNC_003 nirva_array_append,"append an item to an array strand", \
    NIRVA_UINT64,new_length,4,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name, \
    NIRVA_UINT,strand_type,NIRVA_VA_LIST,new_data

#define NIRVA_CALL_METHOD_nirva_strand_delete IMPL
#define NIRVA_MAND_FUNC_004 nirva_strand_delete,"remove a strand from a bundle and free the data", \
    NIRVA_NO_RETURN,,2,NIRVA_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_bundle_free IMPL
#define NIRVA_MAND_FUNC_005 nirva_bundle_free,"free an empty bundle after all strands "	\
    "have been deleted", NIRVA_NO_RETURN,,1,NIRVA_BUNDLEPTR,bundle

#define NIRVA_CALL_METHOD_nirva_array_get_size IMPL
#define NIRVA_MAND_FUNC_006 nirva_array_get_size,"return count of items in the data of a strand", \
    NIRVA_UINT64,data_length,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_bundle_list_strands IMPL
#define NIRVA_MAND_FUNC_007 nirva_bundle_list_strands,"return a NULL terminated array of existing " \
    "strand names in a bundle", NIRVA_STRING_ARRAY,allocated_array_of_names,1,NIRVA_BUNDLEPTR,bundle

#define NIRVA_CALL_METHOD_nirva_get_value_int IMPL
#define NIRVA_MAND_FUNC_008 nirva_get_value_int,"get an int32 value from data of a strand", \
    NIRVA_INT,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_boolean IMPL
#define NIRVA_MAND_FUNC_009 nirva_get_value_boolean,"get a boolean value from data of a strand", \
    NIRVA_BOOLEAN,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_double IMPL
#define NIRVA_MAND_FUNC_010 nirva_get_value_double,"get a double value from data of a strand", \
    NIRVA_DOUBLE,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_string IMPL
#define NIRVA_MAND_FUNC_011 nirva_get_value_string,"get a string value from data of a strand", \
    NIRVA_STRING,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_int64 IMPL
#define NIRVA_MAND_FUNC_012 nirva_get_value_int64,"get an int64 value from data of a strand", \
    NIRVA_INT64,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_voidptr IMPL
#define NIRVA_MAND_FUNC_013 nirva_get_value_voidptr,"get a void * value from data of a strand", \
    NIRVA_VOIDPTR,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_funcptr IMPL
#define NIRVA_MAND_FUNC_014 nirva_get_value_funcptr,"get a function poiner value from data of "	\
    "a strand",NIRVA_FUNCPTR,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_value_bundleptr IMPL
#define NIRVA_MAND_FUNC_015 nirva_get_value_bundleptr,"get a bundleptr value from data of a strand", \
    NIRVA_BUNDLEPTR,value,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_int IMPL
#define NIRVA_MAND_FUNC_016 nirva_get_array_int,"get an int32 array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_INT),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_boolean IMPL
#define NIRVA_MAND_FUNC_017 nirva_get_array_boolean,"get a boolean array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_BOOLEAN),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_double IMPL
#define NIRVA_MAND_FUNC_018 nirva_get_array_double,"get a double array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_DOUBLE),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_string IMPL
#define NIRVA_MAND_FUNC_019 nirva_get_array_string,"get a string array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_STRING),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_int64 IMPL
#define NIRVA_MAND_FUNC_020 nirva_get_array_int64,"get an int64 array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_INT64),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_voidptr IMPL
#define NIRVA_MAND_FUNC_021 nirva_get_array_voidptr,"get a void * array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_VOIDPTR),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_funcptr IMPL
#define NIRVA_MAND_FUNC_022 nirva_get_array_funcptr,"get a function poiner array from data " \
    "of a strand", NIRVA_ARRAY_OF(NIRVA_FUNCPTR),array,2,NIRVA_CONST_BUNDLEPTR, \
    attr,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_get_array_bundleptr IMPL
#define NIRVA_MAND_FUNC_023 nirva_get_array_bundleptr,"get a bundleptr array from data of a strand", \
    NIRVA_ARRAY_OF(NIRVA_BUNDLEPTR),array,2,NIRVA_CONST_BUNDLEPTR,attr,NIRVA_CONST_STRING,strand_name

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
  (n == 1 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_001) : n == 2 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_002) : \
   n == 3 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_004) : \
   n == 5 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_005) : n == 6 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_006) : \
   n == 7 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_007) : n == 8 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_008) : \
   n == 9 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_009) : n == 10 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_010) : \
   n == 11 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_011) : n == 12 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_012) : \
   n == 13 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_013) : n == 14 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_014) : \
   n == 15 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_015) : n == 16 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_016) : \
   n == 17 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_017) : n == 18 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_018) : \
   n == 19 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_019) : n == 20 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_020) : \
   n == 21 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_021) : n == 22 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_022) : \
   n == 23 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_023) : n == 101 ? NIRVA_PRFUNC(NIRVA_MAND_FUNC_001a) : NULL)

  static void add_func_desc(nirva_native_function_t func, const char *fname, const char *fdesc,
			    const char *p0, const char *p1) {

  }

#define NIRVA_FSTRING(nnn, m) _CALL(PRFUNC_##m, NIRVA_MAND_FUNC_##nnn))
#define NIRVA_MANDFUNC(nnn) _CALL(NIRVA_NEEDS_IMPL_FUNC, NIRVA_MAND_FUNC_##nnn)

  NIRVA_MANDFUNC(001) NIRVA_MANDFUNC(002) NIRVA_MANDFUNC(003) NIRVA_MANDFUNC(004) \
  NIRVA_MANDFUNC(005) NIRVA_MANDFUNC(006) NIRVA_MANDFUNC(007) NIRVA_MANDFUNC(008) \
  NIRVA_MANDFUNC(009) NIRVA_MANDFUNC(010) NIRVA_MANDFUNC(011) NIRVA_MANDFUNC(012) \
  NIRVA_MANDFUNC(013) NIRVA_MANDFUNC(014) NIRVA_MANDFUNC(015) NIRVA_MANDFUNC(016) \
  NIRVA_MANDFUNC(017) NIRVA_MANDFUNC(018) NIRVA_MANDFUNC(019) NIRVA_MANDFUNC(020) \
    NIRVA_MANDFUNC(021) NIRVA_MANDFUNC(022) NIRVA_MANDFUNC(023) NIRVA_MANDFUNC(001a)

#define NIRVA_OPT_IMPL_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type impl_func_##funcname(__VA_ARGS__);

#define NIRVA_REC_IMPL_FUNC(funcname, desc, ret_type, ...)	\
  NIRVA_EXTERN ret_type impl_func_##funcname(__VA_ARGS__);

#define NIRVA_REC_FUNC(nnn) _CALL(NIRVA_NEEDS_EXT_COND_FUNC, NIRVA_REC_FUNC_##nnn)

#define NIRVA_ADD_OPT_FUNC(nnn) _CALL(NIRVA_NEEDS_IMPL_FUNC, NIRVA_OPT_FUNC_##nnn)

  // opt / conditional core funcs - optional or conditional on prescence / lack of other funcs

  // if built according to the suggestions, each bundle will have either a copy
  // of its blueprint (a strand_def_bundle defining it)
  // if such can be returned, then it removes the need to implement several other functions
  // listed below. The blueprint will not be altered,

  // changes after nirva_prepare() require PRIV_STRUCT > 50
  // adding after nirva_prepare() requires PRIV_STRUCT > 30

#define _COND_REC_FUNC_001 "NIRVA_LACKS_ATTR,IMPL_FUNC_nirva_strand_get_type,NIRVA_OP_OR," \
  "NIRVA_LACKS_ATTR,IMPL_FUNC_nirva_list_opt_strands,NIRVA_OP_FIN"

#define _COND_REC_FUNC_002 "NIRVA_LACKS_ATTR,IMPL_FUNC_nirva_bundle_request_blueprint," \
  "NIRVA_OP_FIN"

#define NIRVA_REC_FUNC_001 nirva_bundle_request_blueprint,"return a pointer to a cconst attr_desc " \
    "bundle with a definition of the mandatory and optional strands of a bundle.\nNormally this " \
    "would be" "stored in STRAND_INTROSPECTION_BLUEPRINT or STRAND_INTROSPECTION_BLUEPRINT_PTR " \
    "when the bundle is first created",_COND_REC_FUNC_001,NIRVA_CONST_BUNDLEPTR, \
    pointer_to_const_blueprint,1,NIRVA_CONST_BUNDLEPTR,bundle

#define NIRVA_REC_FUNC_002 nirva_strand_get_type,"return the STRAND_TYPE for a strand. Not needed if " \
    "nirva_bundle_request_blueprint is defined",_COND_REC_FUNC_002,NIRVA_UINT,strand_type,2, \
    NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_003 nirva_list_opt_strands,"list the names of optional strands in bundle, " \
      "whether they currently exist in the bundle or not. "		\
      "Not needed if nirva_bundle_request_blueprint is defined",	\
      _COND_REC_FUNC_002,NIRVA_STRING_ARRAY,opt_strand_names,1,NIRVA_CONST_BUNDLEPTR,bundle

#define NIRVA_REC_FUNC_004 nirva_array_remove_item,"remove an item from array data, freeing the data " \
    "held in it", _COND_NONE,NIRVA_NO_RETURN,,3,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING, \
    strand_name,NIRVA_UINT,offset_from_zero

#define NIRVA_REC_FUNC_005 nirva_array_clear,"reset an array to empty (NULL), freeing the data " \
    "held in it",_COND_NONE,NIRVA_NO_RETURN,,2,NIRVA_CONST_BUNDLEPTR,bundle, NIRVA_CONST_STRING, \
    strand_name

#define NIRVA_REC_FUNC_006 nirva_strand_copy,"copy by value, data from one strand to another of " \
    "the same type, first freeing any prior data held in it",_COND_NONE,NIRVA_NO_RETURN,,4, \
    NIRVA_BUNDLEPTR,dest_bundle,NIRVA_CONST_STRING,dest_strand_name,NIRVA_CONST_BUNDLEPTR, \
    src_bundle,NIRVA_CONST_STRING,src_strand_name

#define NIRVA_REC_FUNC_007 nirva_get_value_uint,"get a uint32 value from data of a strand." \
    "If not defined, int32 will be used instead and cast to uint",_COND_NONE,NIRVA_UINT,data,2, \
    NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_008 nirva_get_value_uint64,"get a uint64 value from data of a strand." \
    "If not defined, int64 will be used instead and cast to uint64",_COND_NONE,NIRVA_UINT64, \
    data,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_009 nirva_get_value_float,"get a float value from data of a strand." \
    "If not defined, int64 will be used instead and cast to uint64",_COND_NONE,NIRVA_FLOAT, \
    data,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_010 nirva_get_array_uint,"get a uint32 array from data of a strand." \
    "If not defined, int32 will be used instead and cast to uint",_COND_NONE, \
    NIRVA_ARRAY_OF(NIRVA_UINT),data,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_011 nirva_get_array_uint64,"get a uint64 array from data of a strand." \
    "If not defined, int64 will be used instead and cast to uint64",_COND_NONE, \
    NIRVA_ARRAY_OF(NIRVA_UINT64),data,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_REC_FUNC_012 nirva_get_array_float,"get a float array from data of a strand." \
    "If not defined, int64 will be used instead and cast to uint64",_COND_NONE, \
    NIRVA_ARRAY_OF(NIRVA_FLOAT),data,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_nirva_bundle_has_strand IMPL
#define NIRVA_REC_FUNC_013 nirva_bundle_has_strand,"return TRUE (1) if strand_name currently " \
    "exists in bundle. If not defined, the strandnts will be listed and checked",_COND_NONE, \
    NIRVA_BOOLEAN,exists,2,NIRVA_CONST_BUNDLEPTR,bundle,NIRVA_CONST_STRING,strand_name

#define NIRVA_CALL_METHOD_lookup_item_in_array IMPL
#define NIRVA_REC_FUNC_014 lookup_item_in_array,"locate an item within a container, " \
    "using a key item lookup. The key strand is defined in the blueprint. " \
    "Items are always referenced by key value converted to string. "	\
    "If not defined then the automation may add its own lookup table in the bundle." \
    "If the implementation has a more optimised way, it can be defined here ",_COND_NONE, \
    NIRVA_BUNDLEPTR,found_item,2,NIRVA_CONST_BUNDLEPTR,container,NIRVA_CONST_STRING,str_key

  NIRVA_REC_FUNC(001) NIRVA_REC_FUNC(002) NIRVA_REC_FUNC(003) NIRVA_REC_FUNC(004) \
    NIRVA_REC_FUNC(005) NIRVA_REC_FUNC(006) NIRVA_REC_FUNC(007) NIRVA_REC_FUNC(008) \
  NIRVA_REC_FUNC(009) NIRVA_REC_FUNC(010) NIRVA_REC_FUNC(011) NIRVA_REC_FUNC(012) \
    NIRVA_REC_FUNC(013) NIRVA_REC_FUNC(014)

#define _MAKE_IMPL_REC_FUNC_DESC(n)					\
    (n == 1 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_001) : n == 2 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_002) : \
     n == 3 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_004) : \
     n == 5 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_005) : n == 6 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_006) : \
     n == 7 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_007) : n == 8 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_008) : \
     n == 9 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_009) : n == 10 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_010) : \
     n == 11 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_011) : n == 12 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_012) : \
     n == 13 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_013) : n == 14 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_014) : \
     NULL)

#ifndef IMPL_FUNC_nirva_get_value_uint
#define IMPL_FUNC_nirva_get_value_uint _def_nirva_get_value_uint
#define NEED_NIRVA_GET_VALUE_UINT
#endif

#ifndef IMPL_FUNC_lookup_item_in_array
#define IMPL_FUNC_lookup_item_in_array _def_nirva_lookup_item_in_array
#define NEED_NIRVA_LOOKUP_ITEM_IN_ARRAY
#endif

#ifndef IMPL_FUNC_nirva_bundle_has_strand
#define IMPL_FUNC_nirva_bundle_has_strand _def_nirva_bundle_has_strand
#define NEED_NIRVA_BUNDLE_HAS_STRAND
#endif

  /////// optional overrides ///////
  // optional impl_functions are those which use IMPL_FUNCs but are not fundamental
  // these can be changed at any point even at runtime.
  // changes after nirva_prepare() require PRIV_STRUCT > 20

#define NIRVA_OPT_FUNC_001 nirva_action, "run a static transform via a function wrapper", \
    NIRVA_FUNC_RETURN,response,2,NIRVA_CONST_STRING,transform_name, NIRVA_VARIADIC,...

#ifndef IMPL_FUNC_nirva_action
#define IMPL_FUNC_nirva_action _def_nirva_action
#define NEED_NIRVA_ACTION
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

  NIRVA_ADD_OPT_FUNC(004)

#define NIRVA_OPT_FUNC_005 nirva_attr_get_type, "return the attr_type of an attribute", \
      NIRVA_UINT,attr_type,1,NIRVA_CONST_BUNDLEPTR,attr

#ifndef IMPL_FUNC_nirva_attr_get_type
#define IMPL_FUNC_nirva_attr_get_type _def_nirva_attr_get_type
#define NEED_NIRVA_ATTR_GET_TYPE
#endif

  NIRVA_ADD_OPT_FUNC(005)

#define NIRVA_OPT_FUNC_006 nirva_recycle, "recycle and free resources used by "	\
    "a no longer required bundle.", NIRVA_UINT,attr_type,1,NIRVA_BUNDLEPTR,bun

#ifndef IMPL_FUNC_nirva_recycle
#define IMPL_FUNC_nirva_recycle _def_nirva_recycle
#define NEED_NIRVA_RECYCLE
#endif

  NIRVA_ADD_OPT_FUNC(006)

#define _MAKE_IMPL_OPT_FUNC_DESC(n)					\
  (n == 1 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_001) : n == 2 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_002) : \
   n == 3 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_003) : n == 4 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_004) : \
   n == 5 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_005) : n == 6 ? NIRVA_PRFUNC(NIRVA_OPT_FUNC_006) : \
   NULL)
  // simplyfing macros
#define NIRVA_ACTION(fname, ...) NIRVA_IMPL_FUNC(nirva_action, #fname, __VA_ARGS__)
#define NIRVA_WAIT_RETRY() NIRVA_IMPL_FUNC(nirva_wait_retry,)

  // can be used if we can specify attr type, else if we have IMPL_nirva_strand_copy
  // we can copy direct to a value bundle
  // otherwise this becomes more complicated and we have to it by individual types
#define NIRVA_CALL_METHOD_get_attr_value_known_type MACRO
#define _INLINE_MACRO_get_attr_value_known_type(ret, xtype, attr)	if (!in_bootstrap) \
    NIRVA_IMPL_ASSIGN(ret, nirva_get_value_##xtype, attr, STRAND_VALUE_DATA)

#define NIRVA_CALL_METHOD_get_value_obj_funcptr MACRO
#define _INLINE_MACRO_get_value_obj_funcptr(func, funcbundle, name)	if (!in_bootstrap) { \
    NIRVA_DEF_VARS(nirva_native_function_t, nfunc)			\
      NIRVA_IMPL_ASSIGN(nfunc, nirva_get_value_funcptr, funcbundle, name) \
      func = __IMPL_CAST_TO_TYPE__(nfunc, nirva_function_t);		\
  }

#define NIRVA_CALL_METHOD_nirva_attr_get_type IMPL
#ifndef NEED_NIRVA_ATTR_GET_TYPE
#define _NIRVA_DEPLOY_def_nirva_attr_get_type
#else
#undef NEED_NIRVA_ATTR_GET_TYPE
#define _NIRVA_DEPLOY_def_nirva_attr_get_type				\
  NIRVA_DEF_FUNC(NIRVA_UINT, _def_nirva_attr_get_type, NIRVA_CONST_BUNDLEPTR attr) \
    NIRVA_DEF_VARS(NIRVA_UINT, atype)					\
    NIRVA_DEF_VARS(NIRVA_BOOLEAN, has_value)				\
    NIRVA_ASSERT(attr, NIRVA_RETURN, ATTR_TYPE_NONE)			\
    NIRVA_IMPL_ASSIGN(has_value, nirva_bundle_has_strand, attr, STRAND_ATTRIBUTE_VALUE_TYPE) \
    NIRVA_ASSERT(has_value, NIRVA_RETURN, ATTR_TYPE_NONE)		\
    NIRVA_IMPL_ASSIGN(atype, nirva_get_value_uint, attr, STRAND_ATTRIBUTE_VALUE_TYPE) \
    NIRVA_RETURN(atype)							\
    NIRVA_FUNC_END
#endif

  /////////////
    NIRVA_EXTERN NIRVA_CONST_BUNDLEPTR STRUCTURE_PRIME;
  NIRVA_EXTERN NIRVA_CONST_BUNDLEPTR STRUCTURE_APP;
  /////////////

  // condition automation //
#define __COND(domain, id) _NIRVA_COND_##domain##_##id

#define _NIRVA_COND_TEST(n, ...) (NIRVA_COND_##n##_TEST(__VA_ARGS__))

#define _CALL2(macro,n,...) macro(n, __VA_ARGS__)
#define NIRVA_COND_TEST(n, ...) _CALL2(_NIRVA_COND_TEST,n,__VA_ARGS__)
#define NIRVA_COND_TEST0(n) (NIRVA_COND_##n##_TEST)

#ifdef HAVE_MAKE_STRANDS_FUNC
#define _NIRVA_DEPLOY_MAKE_STRANDS_FUNC
#else
#define HAVE_MAKE_STRANDS_FUNC
#define _NIRVA_DEPLOY_MAKE_STRANDS_FUNC static char *make_strand(const char *fmt,va_list ap){ \
    va_list vc;								\
    char *st;size_t strsz;						\
    va_copy(vc, ap);							\
    strsz=vsnprintf(0,0,fmt,vc);					\
    va_end(vc);								\
    st=malloc(++strsz);vsnprintf(st,strsz,fmt,ap);return st;}		\
  const char **make_strands(const char*fmt,...){			\
    p_debug("\nGenerating bundle definition\n");			\
    const char**sts=0,*xfmt,*str;int ns=0;va_list ap;va_start(ap,fmt);	\
    while(1) {if (*fmt) {if (!(xfmt=va_arg(ap,char*))) break; vsnprintf(0,0,xfmt,ap);} \
      else {if (!(str = va_arg(ap, char *))) break; p_debug("got %s\n", str);}  ns++;} va_end(ap); \
    p_debug("\nCounted %d atoms\n", ns);				\
    sts=malloc((ns+1)*sizeof(char*));va_start(ap,fmt);			\
    if (*fmt) xfmt = fmt;						\
    else xfmt = "%s";							\
    p_debug("\nAllocated %d atoms\n", ns+1);				\
    for(int i = 0; i < ns; i++) {					\
      p_debug("\nproceesing %d of %d\n", i, ns);			\
      if (i > 0) xfmt=(*fmt?va_arg(ap,char*):"%s");			\
      sts[i]=make_strand(xfmt,ap);					\
      p_debug("\nproceesed %d of %d\n", i, ns);				\
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
    char**sT=0;int nsT=0;va_list ap;if(n&&*n){size_t Cln=strlen(n)+2;	\
      p_debug("\nGenerating bundle definition for %s\n",n);		\
      char*str=malloc(Cln);snprintf(str,Cln,"#%s",n);sT=(char**)make_strands("",str,0);nsT++; \
      p_debug("\nGenerating bundle definition for %s\n",n);va_start(ap,pfx); \
      while(1){int nC=0;char**newsT=va_arg(ap,char**);if(!newsT)break;	\
	while(newsT[++nC]);;sT=realloc(sT,(nsT+nC+1)*sizeof(char*));	\
	for(nC=0;newsT[nC];nC++){size_t strsz=snprintf(0,0,"%s%s",pfx?pfx:"",newsT[nC]); \
	  p_debug("Adding strand:\t%s\n",newsT[nC]);if(strsz){sT[nsT]=malloc(++strsz); \
	    snprintf(sT[nsT++],strsz,"%s%s",pfx?pfx:"",newsT[nC]);}}}va_end(ap); \
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

#define _FULL_IMPL(pre, func, ...) (pre##_##func(__VA_ARGS__))
#define _IMPL(func, ...)  _FULL_IMPL(impl_func, func, __VA_ARGS__)

  /// condfuncs //

  //////////////////

  // conditions, selections 

  // TODO - organise into
  //  OPS - paren open / close / end
  //  LOGIC - operations on truth values
  // NUMERIC - operations on int / float vals
  //
  // vars - ATTR_INT_VAL, STRAND_INT_VAL, VAR_INT_VAL, 1
  // STORED_INT_VAL n, ARG_INT_VAL

  // logical "operations"
  // operation on TRUE / SUCCESS and FALSE / FAIL
  // one arg
#define NIRVA_OP_FIN 				-1
  // 1 arg
#define NIRVA_OP_EVAL		       		0 //(!!(a)) // OP_EVAL, var, 
#define NIRVA_LOGIC_NOT	 	      		1 // (!(a))
#define NIRVA_OP_TRUE	 	      		2 // (EVAL(a) == 1)
#define NIRVA_OP_FALSE		       		3 // (EVAL(a) == 0) 
  // 0 args
#define NIRVA_OP_PAREN_OPEN	      		4 // ) //
#define NIRVA_OP_PAREN_CLOSE	      		5 // ( //

  // 1 arg (n)  s[n] = next op
#define NIRVA_OP_STORE	      			6 // ) //
#define NIRVA_OP_FETCH       			7 // ) //

  // two args
#define NIRVA_OP_OR				8
  // current val XOR next op			
#define NIRVA_OP_XOR				9
  // value operations

  // value
#define NIRVA_LOGIC_FIRST				16

  // note leading _
#define _NIRVA_VAL_NOT					16
#define NIRVA_COND_16_TEST(a)			    	(!(a))
#define NIRVA_VAL_NOT(a)				NIRVA_COND_TEST(_NIRVA_VAL_NOT,a)
#define _NIRVA_VAL_EQUALS				17
#define NIRVA_COND_17_TEST(a,b)			    	((a)==(b))
#define NIRVA_VAL_EQUALS(a,b)				NIRVA_COND_TEST(_NIRVA_VAL_EQUALS,a,b)
#define _NIRVA_VAL_GT					18
#define NIRVA_COND_18_TEST(a,b) 	   		((a)>(b))
#define NIRVA_VAL_GT(a,b)	       			NIRVA_COND_TEST(_NIRVA_VAL_GT,a,b)
#define _NIRVA_VAL_LT					19
#define NIRVA_COND_19_TEST(a,b)   	 		((a)<(b))
#define NIRVA_VAL_LT(a,b)	       			NIRVA_COND_TEST(_NIRVA_VAL_LT,a,b)
#define _NIRVA_VAL_GTE					20
#define NIRVA_COND_20_TEST(a,b)		    		((a)>=(b))
#define NIRVA_VAL_GTE(a,b)	       			NIRVA_COND_TEST(_NIRVA_VAL_GTE,a,b)
#define _NIRVA_VAL_LTE					21
#define NIRVA_COND_21_TEST(a,b) 	   		((a)<=(b))
#define NIRVA_VAL_LTE(a,b)	       			NIRVA_COND_TEST(_NIRVA_VAL_LTE,a,b)
#define NIRVA_LOGIC_OR					22
#define NIRVA_COND_22_TEST(a,b)			    	((a)|(b))
#define NIRVA_LOGIC_AND					23
#define NIRVA_COND_23_TEST(a,b)			    	((a)&(b))
#define NIRVA_LOGIC_XOR					24
#define NIRVA_COND_24_TEST(a,b)			    	(((a)^(b)))
#define NIRVA_COND_ALWAYS		       		25
#define NIRVA_COND_25_TEST			    	(1)
#define NIRVA_COND_NEVER		       		26
#define NIRVA_COND_26_TEST			    	(0)
#define NIRVA_COND_STR_MATCH				27
#define NIRVA_COND_27_TEST(a,b)			    	__IMPL_STRINGS_EQUAL__(#a,#b)
#define NIRVA_COND_BIT_ON				28
#define NIRVA_COND_28_TEST(v,b) 			(((v)&(b))==(b))
#define NIRVA_COND_BIT_OFF				29
#define NIRVA_COND_29_TEST(v,b) 			(!((v)|(b)))
#define NIRVA_COND_PROB_0_1				30
#define NIRVA_COND_30_TEST(p,s)				((p)<=NIRVA_RAND_BAD(s))

#define NIRVA_VALUE
  
#define NIRVA_RAND_BAD1(a) (((a) + .83147) * .727953)
#define NIRVA_RAND_BAD2(a) NIRVA_RAND_BAD1(a) > 1.3846 ? NIRVA_RAND_BAD1(a) - 1.1836 : \
    NIRVA_RAND_BAD1(a)
#define NIRVA_RAND_BAD3(a) NIRVA_RAND_BAD1(a) > 1. ? NIRVA_RAND_BAD1(a) - 1. : \
    NIRVA_RAND_BAD1(a)
#define NIRVA_RAND_BAD(a) (NIRVA_RAND_BAD3(NIRVA_RAND_BAD2(NIRVA_RAND_BAD2(NIRVA_RAND_BAD2(a)))))

#define _COND_END		0
#define _COND_ONCE		1
#define _COND_ALWAYS		2
#define _COND_CHECK		3
#define _COND_CHECK_RETURN     	4

#define _CHK_LOGIC_N(n,op,p0,p1)					\
  if (op == n) NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_COND_TEST(n,p0,p1)));
#define _CHK_LOGIC1_N(n,op,p0)						\
  if (op == n) NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_COND_TEST(n,p0)));
#define _CHK_LOGIC0_N(n,op)						\
  if (op == n) NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_COND_TEST0(n)));

#define _CHK_LOGIC(op,p0,p1) _CHK_LOGIC_N(17,op,p0,p1) _CHK_LOGIC_N(18,op,p0,p1) \
  _CHK_LOGIC_N(19,op,p0,p1) _CHK_LOGIC_N(20,op,p0,p1) _CHK_LOGIC_N(21,op,p0,p1) \
  _CHK_LOGIC_N(22,op,p0,p1) _CHK_LOGIC_N(23,op,p0,p1) _CHK_LOGIC_N(24,op,p0,p1)	\
  _CHK_LOGIC0_N(25,op) _CHK_LOGIC0_N(26,op) _CHK_LOGIC_N(27,op,p0,p1)	\
  _CHK_LOGIC_N(28,op,p0,p1) _CHK_LOGIC_N(29,op,p0,p1) _CHK_LOGIC_N(30,op,p0,p1)

#define NIRVA_FAIL(...) abort();
#define NIRVA_FATAL(...) abort();

  static  int64_t check_cond(int64_t op, char **strands, int i, NIRVA_BUNDLEPTR attrs) {
    NIRVA_DEF_VARS(nirva_bundleptr_t, attr)
      char *str0 = strands[i + 1];
    char *str1 = strands[i + 2];
    // this gives us the from type - it could be a value as string
    // or it could be attribute name,
    char tcode0 = *str0, tcode1;
    if (tcode0 == 'A') {
      uint32_t t0;
      char *attr_name = str0 + 1;
      NIRVA_CALL_ASSIGN(attr, lookup_item_in_array, attrs, attr_name)
	NIRVA_CALL_ASSIGN(t0, nirva_attr_get_type, attr)
	if (t0 == ATTR_TYPE_INT) {
	  int p0 = 0;
	  NIRVA_CALL_ASSIGN(p0, get_attr_value_known_type, int, attr);
	  if (op == NIRVA_LOGIC_NOT) {
	    NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_VAL_NOT(p0)));
	  }
	  tcode1 = *str1;
	  if (tcode1 == 'A') {
	    uint32_t t1 = 0;
	    char *attr_name = str1 + 1;
	    NIRVA_CALL_ASSIGN(attr, lookup_item_in_array, attrs, attr_name)
	      NIRVA_CALL_ASSIGN(t1, nirva_attr_get_type, attr)
	      if (t1 == ATTR_TYPE_INT) {
		int p1 = 0;
		NIRVA_CALL_ASSIGN(p1, get_attr_value_known_type, int, attr);
		_CHK_LOGIC(op, p0, p1);
	      }
	    if (t1 == ATTR_TYPE_INT64) {
	      int64_t p1 = 0;
	      NIRVA_CALL_ASSIGN(p1, get_attr_value_known_type, int64, attr);
	      _CHK_LOGIC(op, p0, p1);
	    }
	    NIRVA_RETURN(NIRVA_COND_FAIL);
	  }
	  if (tcode1 == 'i') {
	    int p1 = 0;
	    NIRVA_IMPL_ASSIGN(p1, atoi, str1 + 1);
	    _CHK_LOGIC(op, p0, p1);
	  }
	  if (tcode1 == 'I') {
	    int64_t p1 = 0;
	    NIRVA_IMPL_ASSIGN(p1, atol, str1 + 1);
	    _CHK_LOGIC(op, p0, p1);
	  }
	  NIRVA_RETURN(NIRVA_COND_FAIL);
	}
      if (t0 == ATTR_TYPE_INT64) {
	int64_t p0 = 0;
	NIRVA_CALL_ASSIGN(p0, get_attr_value_known_type, int64, attr);
	if (op == NIRVA_LOGIC_NOT) {
	  NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_VAL_NOT(p0)));
	}
	tcode1 = *str1;
	if (tcode1 == 'A') {
	  uint32_t t1;
	  char *attr_name = str1 + 1;
	  NIRVA_CALL_ASSIGN(attr, lookup_item_in_array, attrs, attr_name);
	  NIRVA_CALL_ASSIGN(t1, nirva_attr_get_type, attr);
	  if (t1 == ATTR_TYPE_INT) {
	    int p1 = 0;
	    NIRVA_CALL_ASSIGN(p1, get_attr_value_known_type, int, attr);
	    _CHK_LOGIC(op, p0, p1);
	  }
	  if (t1 == ATTR_TYPE_INT64) {
	    int64_t p1 = 0;
	    NIRVA_CALL_ASSIGN(p1, get_attr_value_known_type, int64, attr);
	    _CHK_LOGIC(op, p0, p1);
	  }
	  NIRVA_RETURN(NIRVA_COND_FAIL);
	}
	if (tcode1 == 'i') {
	  int p1 = 0;
	  NIRVA_IMPL_ASSIGN(p1, atoi, str1 + 1);
	  _CHK_LOGIC(op, p0, p1);
	}
	if (tcode1 == 'I') {
	  int64_t p1 = 0;
	  NIRVA_IMPL_ASSIGN(p1, atol, str1 + 1);
	  _CHK_LOGIC(op, p0, p1);
	}
	NIRVA_RETURN(NIRVA_COND_FAIL);
      }
    }
    if (tcode0 == 'i') {
      int p0 = 0;
      NIRVA_IMPL_ASSIGN(p0, atoi, str0 + 1);
      if (op == NIRVA_LOGIC_NOT) {
	NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_VAL_NOT(p0)));
      }
      tcode1 = *str1;
      if (tcode1 == 'A') {
	uint32_t t1;
	char *attr_name = str1 + 1;
	NIRVA_CALL_ASSIGN(attr, lookup_item_in_array, attrs, attr_name);
	NIRVA_CALL_ASSIGN(t1, nirva_attr_get_type, attr);
	if (t1 == ATTR_TYPE_INT) {
	  int p1 = 0;
	  NIRVA_ASSIGN_CALL(p1, get_attr_value_known_type, int, attr);
	  _CHK_LOGIC(op, p0, p1);
	}
	if (t1 == ATTR_TYPE_INT64) {
	  int64_t p1 = 0;
	  NIRVA_ASSIGN_CALL(p1, get_attr_value_known_type, int64, attr);
	  _CHK_LOGIC(op, p0, p1);
	}
	NIRVA_RETURN(NIRVA_COND_FAIL);
      }
      if (tcode1 == 'i') {
	int p1 = 0;
	NIRVA_IMPL_ASSIGN(p1, atoi, str1 + 1);
	_CHK_LOGIC(op, p0, p1);
      }
      if (tcode1 == 'I') {
	int64_t p1 = 0;
	NIRVA_IMPL_ASSIGN(p1, atol, str1 + 1);
	_CHK_LOGIC(op, p0, p1);
      }
      NIRVA_RETURN(NIRVA_COND_FAIL);
    }
    if (tcode0 == 'I') {
      int64_t p0 = 0;
      NIRVA_IMPL_ASSIGN(p0, atol, str0 + 1);
      if (op == NIRVA_LOGIC_NOT) {
	NIRVA_RETURN(NIRVA_COND_RESPONSE(NIRVA_VAL_NOT(p0)));
      }
      tcode1 = *str1;
      if (tcode1 == 'A') {
	uint32_t t1;
	char *attr_name = str1 + 1;
	NIRVA_CALL_ASSIGN(attr, lookup_item_in_array, attrs, attr_name);
	NIRVA_CALL_ASSIGN(t1, nirva_attr_get_type, attr);
	if (t1 == ATTR_TYPE_INT) {
	  int p1 = 0;
	  NIRVA_ASSIGN_CALL(p1, get_attr_value_known_type, int, attr);
	  _CHK_LOGIC(op, p0, p1);
	}
	if (t1 == ATTR_TYPE_INT64) {
	  int64_t p1 = 0;
	  NIRVA_ASSIGN_CALL(p1, get_attr_value_known_type, int64, attr);
	  _CHK_LOGIC(op, p0, p1);
	}
	NIRVA_RETURN(NIRVA_COND_FAIL);
      }
      if (tcode1 == 'i') {
	int p1 = 0;
	NIRVA_IMPL_ASSIGN(p1, atoi, str1 + 1);
	_CHK_LOGIC(op, p0, p1);
      }
      if (tcode1 == 'I') {
	int64_t p1 = 0;
	NIRVA_IMPL_ASSIGN(p1, atol, str1 + 1);
	_CHK_LOGIC(op, p0, p1);
      }
    }
    NIRVA_RETURN(NIRVA_COND_FAIL);
  }

  NIRVA_STATIC int64_t update_gcondres(int op_macro, int64_t gcondres, int64_t condres) {
    if (condres != NIRVA_COND_SUCCESS) {
      if (condres == NIRVA_COND_FORCE) NIRVA_RETURN(NIRVA_COND_SUCCESS);
      if (condres == NIRVA_COND_ABANDON) NIRVA_RETURN(condres);
      if (condres == NIRVA_COND_ERROR) gcondres = condres;
      if (gcondres != NIRVA_COND_ERROR) {
	if (condres == NIRVA_COND_WAIT_RETRY) gcondres = condres;
	if (gcondres != NIRVA_COND_WAIT_RETRY) {
	  if (op_macro && op_macro != NIRVA_LOGIC_AND && condres == NIRVA_COND_FAIL) {
	    if ((op_macro == _NIRVA_VAL_EQUALS && gcondres == condres)
		|| (op_macro == NIRVA_LOGIC_XOR && gcondres == condres))
	      gcondres = NIRVA_COND_SUCCESS;
	    else if (gcondres == NIRVA_COND_SUCCESS) gcondres = condres;
	  } else gcondres = condres;
	}
      }
    } else {
      if (op_macro == NIRVA_LOGIC_OR && gcondres == NIRVA_COND_FAIL)
	gcondres = condres;
      else if (op_macro == NIRVA_LOGIC_XOR && gcondres == NIRVA_COND_SUCCESS)
	gcondres = NIRVA_COND_FAIL;
    }
    NIRVA_RETURN(gcondres)
      }

#define NIRVA_CALL_METHOD_call_native_func INTERN
  NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, call_native_func, NIRVA_BUNDLEPTR funcbundle,
			  NIRVA_BUNDLEPTR bin, NIRVA_BUNDLEPTR bout)
  //
    NIRVA_DEF_VARS(nirva_native_function_t, nfunc)
    NIRVA_CALL_ASSIGN(nfunc, nirva_get_value_funcptr, funcbundle, STRAND_FUNCTION_NATIVE_FUNCTION)
  // what we need to do here is simply get the pmap_desc array from the funcbundle
  // and the use it to map attribute values in bin to nfunc params,
  // the funtion return is mapped back to an attibute (-1), and if the return type is
  // NIRVE_FUNC_RETURN (int64_t by default), then we return that; otherwise we
  // may try to "interpret" the result. If in doubt, return COND_SUCCESS.
    (void)nfunc;
  //condres = (*func)(b_in, b_out);
  NIRVA_RETURN(NIRVA_COND_CLUELESS)
    NIRVA_FUNC_END


#define NIRVA_CALL_METHOD_nirva_array_append_wrapper INTERN
    NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, nirva_array_append_wrapper, NIRVA_BUNDLEPTR bun, \
			    NIRVA_CONST_STRING item, NIRVA_UINT xtype, NIRVA_VARIADIC) \
    NIRVA_DEF_VARS(NIRVA_VA_LIST, v)					\
    NIRVA_VA_START(v, xtype)						\
    NIRVA_CALL(nirva_array_append, bun, item, xtype, v)			\
    NIRVA_VA_END(v)							\
    NIRVA_FUNC_END

#define NIRVA_CALL_METHOD_set_strand_val INTERN
    NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, set_strand_val, NIRVA_BUNDLEPTR bundle, \
			    NIRVA_CONST_STRING key, NIRVA_UINT xtype, NIRVA_VARIADIC) \
    NIRVA_DEF_VARS(NIRVA_VA_LIST, var)					\
  /*     NIRVA_BUNDLEPTR old_value = create_bundle_by_type(VALUE);		\ */
  /* if (nirva_bundle_has_strand(bundle, key)) {				\ */
  /*   ctype old = get_strand_value_##etype(bundle, name);			\ */
    NIRVA_VA_START(var, xtype)						\
    /*if (call_data_hook(bundle, key, xtype, NIRVA_HOOK_FLAG_BEFORE, 0,  old, var)) {*/ \
    NIRVA_CALL(nirva_value_set, bundle, key, xtype, var)			\
    /*	call_data_hook(bundle, key, xtype, 0, 0, old, new);*/		\
  /* } else {								\ */
    /* this is adding strand hook */					\
    /*     if (call_data_hook(bundle, name, vtype, NIRVA_HOOK_FLAG_BEFORE, +1, new)) {*/ \
    NIRVA_CALL(nirva_value_set, bundle, key, xtype, var)			\
    /*	call_data_hook(bundle, name, vtype, 0, +1, new);*/		\
    NIRVA_VA_END(var)						\
    NIRVA_FUNC_END

#define NIRVA_CALL_METHOD_check_cond_script INTERN
    NIRVA_DEF_FUNC_INTERNAL(NIRVA_INT64, check_cond_script, NIRVA_BUNDLEPTR b_in, NIRVA_BUNDLEPTR b_out)
    NIRVA_DEF_VARS(nirva_bundleptr_t, func, attrs);
  int64_t condres, gcondres = NIRVA_COND_SUCCESS;
  NIRVA_IMPL_ASSIGN(func, nirva_get_value_bundleptr, b_in, STRAND_CONDVAL_CURRENT);
  NIRVA_IMPL_ASSIGN(attrs, nirva_get_value_bundleptr, b_in, BUNDLE_STANDARD_ATTR_GROUP);
  NIRVA_DEF_VARS(NIRVA_STRING_ARRAY, strands);
  NIRVA_IMPL_ASSIGN(strands, nirva_get_array_string, func, BUNDLE_AUTOMATION_SCRIPT);
  for (int i = 0; strands[i]; i++) {
    int op_macro = NIRVA_LOGIC_AND, logic_macro;
    NIRVA_IMPL_ASSIGN(logic_macro,  atoi, strands[i]);
    if (logic_macro == NIRVA_OP_FIN) break;
    if (logic_macro < NIRVA_LOGIC_FIRST) {
      op_macro = logic_macro;
      NIRVA_IMPL_ASSIGN(logic_macro,  atoi, strands[++i]);
      if (logic_macro == NIRVA_OP_FIN) break;
    }
    condres = check_cond(logic_macro, strands, i, attrs);
    NIRVA_CALL(nirva_array_append_wrapper, b_out, STRAND_FUNCTION_RESPONSE,
	       GET_STRAND_TYPE(FUNCTION, RESPONSE), condres)
      update_gcondres(op_macro, gcondres, condres);
    NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_FORCE, NIRVA_RETURN, gcondres)
      NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_ABANDON, NIRVA_RETURN, gcondres)
      i++;
  }
  NIRVA_RETURN(gcondres);
  NIRVA_FUNC_END


#define NIRVA_COND_CHECK(ret, conds, in, out) (ret) = _def_nirva_condcheck(conds, in, out)

#define NIRVA_SUB_BUNDLE(b, s) NIRVA_RESULT(nirva_get_value_bundleptr, b, s)

  NIRVA_STATIC int64_t _def_nirva_condcheck(NIRVA_BUNDLEPTR conds, NIRVA_BUNDLEPTR  b_in, NIRVA_BUNDLEPTR b_out) {
    // check conditions.
    // the first step is to locate a strand with name "CONST_AUTOMATION_CONDITION"
    // or simply, "condition"
    // in bundle b_in. This will be a pointer to a CONDVAL_NODE.
    // once located then we look inside and find another bundle, "STRAND_AUTOMATION_CONDITION"
    // this time the bundle is a function bundle. there are two uses for the condval node.
    // after returning from function, depending on whether we received COND_FAIL or COND_SUCCESS
    // there are two follow on nodes. In condcheck mode, both nodes point to the same nest node
    // and we simply continue unitl both are null, then we have the final result, COND_SUCCESS,
    // FAIL, etc. In Cascade mode, some end nodes may have a "constval" item. In that case,
    // we simply create an item STRAND_CASCADE_VALUE, type bundleptr, in b_out,
    // and copy the value from constval.
    // The value we return is the global response.
    //
    // The "function" in condval_node can be either an object function, which we will simply
    // call as res = func(b_in, b_out)
    // otherwise it can be a native function, in this case we need call a function which will map
    // other attributes in b_in to function parameters and call it
    // the third option is script. In the case of conditions, each condition is a string of the
    // which we handle similar to a va_list. We read an int op. which can be an op_macro
    // or a logic_macro
    // so we convert this with atoi. If the result is < 16 it is an op_macro, and we note this,
    // then read a logic_macro, Otherwise we read a logic_macro. Then depending on this we know
    // how many arguments and their type. The following 1 or 2 strings are the arguments.
    // The values will start with a typeletter similar to bundledefs, however we now also have
    // type "a" attribute, "e", strand, "A" ptr to attribute and "E" ptr to strand
    // the operation now is to read in the input value. We know the input type, unless it is an
    // strand or attribute. So for strands we call get item_type, for attributes get_attr_type.
    // then we get a value of that type, do likewise for the second arg (if the op takes 2 args)
    // and apply the logic opt to the 1 or 2 values. This gives a return value. Then we apply the
    // op macro and get the status. This continues until we have called all functions, or
    // we get NIRVA_COND_FORCE or NIRVA_COND_ABANDON.

    NIRVA_DEF_VARS(nirva_bundleptr_t, condval, if_succ, if_fail)
      NIRVA_IMPL_ASSIGN(condval, nirva_get_value_bundleptr, b_in, "CONST_AUTOMATION_CONDITION")
      int64_t condres, gcondres = NIRVA_COND_SUCCESS;
    NIRVA_DEF_VARS(NIRVA_INT,  op_macro = NIRVA_LOGIC_AND)
      do {
	NIRVA_DEF_VARS(nirva_bundleptr_t, funcbundle, val)
	  NIRVA_IMPL_ASSIGN(val, nirva_get_value_bundleptr, condval, STRAND_CASCADE_VALUE)
	  if (val) {
	    NIRVA_CALL(set_strand_val, b_out, STRAND_CASCADE_VALUE, STRAND_TYPE_CONST_BUNDLEPTR, val);
	    NIRVA_RETURN(gcondres);
	  }
	NIRVA_IMPL_ASSIGN(funcbundle, nirva_get_value_bundleptr, condval, BUNDLE_AUTOMATION_CONDITION)
	  if (NIRVA_RESULT(nirva_bundle_has_strand, funcbundle, BUNDLE_FUNCTION_SCRIPT)) {
	    /*indicate the fn in question and parse its script */
	    NIRVA_CALL(set_strand_val, b_in, STRAND_CONDVAL_CURRENT, STRAND_TYPE_CONST_BUNDLEPTR, \
		       funcbundle);
	    NIRVA_CALL_ASSIGN(condres, check_cond_script, b_in, b_out)
	      NIRVA_CALL(set_strand_val, b_out, STRAND_FUNCTION_RESPONSE, condres)
	      } else if (NIRVA_RESULT(nirva_bundle_has_strand, funcbundle, STRAND_FUNCTION_OBJ_FUNCTION)) {
	    NIRVA_DEF_VARS(nirva_function_t, func)
	      NIRVA_CALL_ASSIGN(func, get_value_obj_funcptr, funcbundle,
				STRAND_FUNCTION_OBJ_FUNCTION)
	      condres = (*func)(b_in, b_out);
	    NIRVA_CALL(set_strand_val, b_out, STRAND_FUNCTION_RESPONSE, condres)
	      } else if (NIRVA_RESULT(nirva_bundle_has_strand, funcbundle, STRAND_FUNCTION_NATIVE_FUNCTION)) {
	    NIRVA_CALL_ASSIGN(condres, call_native_func, funcbundle, b_in, b_out)
	      } else continue;

	update_gcondres(op_macro, gcondres, condres);
	NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_FORCE, NIRVA_RETURN, gcondres)
	  NIRVA_CALL_IF_EQUAL(gcondres, NIRVA_COND_ABANDON, NIRVA_RETURN, gcondres)
	  //now we have 2 possible paths depending on condres
	  NIRVA_CALL_ASSIGN(if_succ, nirva_get_value_bundleptr, condval, "CONST_NEXT_SUCCESS")
	  NIRVA_IF_NOT_EQUAL(if_succ, if_fail,
			     if (condres != NIRVA_COND_FAIL &&
				 condres != NIRVA_COND_SUCCESS)
			       NIRVA_RETURN(gcondres);)
	  if (if_succ || !if_fail) {
	    NIRVA_CALL_IF_EQUAL(condres, NIRVA_COND_SUCCESS, NIRVA_ASSIGN, condval, if_succ);
	    NIRVA_CALL_IF_EQUAL(condres, NIRVA_COND_FAIL, NIRVA_ASSIGN, condval, if_fail);
	  }
	NIRVA_RETURN(gcondres);
      } while (1);
  }

#define _INTERNAL_FUNC_make_n_bundles _INTERNAL_FUNC_def_make_n_bundles
#define NIRVA_CALL_METHOD_make_n_bundles INTERN
  // we use two va_list pointers - one which gets initial bundle / bdef pairs
  // and a second which points to va_args for that bundle
  NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, def_make_n_bundles, NIRVA_INT nb, NIRVA_VARIADIC)
    NIRVA_DEF_VARS(NIRVA_VA_LIST, va, vc)
    NIRVA_DEF_VARS(nirva_bundleptr_t *, b)
    NIRVA_DEF_VARS(NIRVA_INT, c)
    NIRVA_DEF_VARS(NIRVA_BUNDLEDEF, bdef)
    NIRVA_VA_START(va, nb)
  // first we will get past the intial bundle/bdef pairs until we get to the va_args
    c = nb;
  NIRVA_DO
  b = va_arg(va, nirva_bundleptr_t *);
  bdef = va_arg(va, NIRVA_BUNDLEDEF);
  NIRVA_WHILE(--c > 0)
  // now we use va_copy to set the second va_list pointer here
  va_copy(vc, va);
  NIRVA_VA_END(va)
  // now we return to the start, but this time we have the second va_list pointer
  // so each bundle / bdef pair gets its own NULL terminated section of combined va_args
    NIRVA_VA_START(va, nb)
    c = nb;
  NIRVA_DO
  b = va_arg(va, nirva_bundleptr_t *);
  bdef = va_arg(va, NIRVA_BUNDLEDEF);
  NIRVA_CALL_ASSIGN(*b, create_bundle_from_bdef, bdef, vc)
  NIRVA_WHILE(--c > 0)
  NIRVA_VA_END(vc)
  NIRVA_VA_END(va)
  NIRVA_RETURN(NIRVA_RESULT_SUCCESS)
  NIRVA_FUNC_END

  /* NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, nirva_array_append_wrapper, NIRVA_BUNDLEPTR bun, \ */
  /* 			  NIRVA_CONST_STRING item, NIRVA_UINT xtype, NIRVA_VARIADIC) \ */

  //NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, make_func_bundles, NIRVA_INT nb, NIRVA_VARIADIC)

#define NIRVA_CALL_METHOD_make_func_bundles INTERN
  NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, make_func_bundles,	\
			  NIRVA_BUNDLEPTR *pb_in, NIRVA_BUNDLEPTR *pb_out, ...) \
  NIRVA_DEF_VARS(NIRVA_VA_LIST, va)					\
  NIRVA_VA_START(va, pb_out)						\
  _INTERNAL_FUNC_make_n_bundles(2, pb_in, FN_INPUT_BUNDLE_TYPE, pb_out,	\
				FN_OUTPUT_BUNDLE_TYPE, va);		\
  NIRVA_VA_END(va)							\
    NIRVA_FUNC_END

#define NIRVA_CALL_METHOD_get_attr_bundle INLINE
#define _INLINE_INTERNAL_get_attr_bundle(attrs, bundle)	NIRVA_INLINE	\
    (NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attrs)			\
     NIRVA_IMPL_ASSIGN(attrs, nirva_get_value_bundleptr, bundle, BUNDLE_STANDARD_ATTR_CON))

#define NIRVA_CALL_METHOD_get_attribute INLINE
#define _INLINE_INTERNAL_get_attribute(attr, bundle, attr_name)	NIRVA_INLINE \
    (NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attrs)			\
     NIRVA_INLINE_ASSIGN(attrs, get_attr_bundle, bundle)		\
     NIRVA_IMPL_ASSIGN(attr, lookup_item_in_array, attrs, attr))

#define NIRVA_CALL_METHOD_assert_range INLINE
#define NIRVA_ASSERT_RANGE(var, min, mx, onfail, ...) INLINE_INTERNAL(assert_range, __VA_ARGS__)
#define _INLINE_INTERNAL_assert_range(var, min, mx, onfail, ...) NIRVA_INLINE \
  (if (var < min || var > mx) onfail(__VA_ARGS__);)

#define NIRVA_CALL_METHOD_get_subsystem INLINE
#define _INLINE_INTERNAL_get_subsystem(subsystem, subtype)		\
  NIRVA_ASSERT_RANGE(subtype, 1, N_STRUCT_SUBTYPES - 2, NIRVA_FAIL, NIRVA_NULL); \
  NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, structurals, attrs);		\
  NIRVA_DEF_VARS(NIRVA_BUNDLEPTR, subsystem);				\
  NIRVA_INLINE_ASSIGN(attr, get_attribute, STRUCTURE_PRIME, ATTR_STRUCTURAL_SUBSYSTEM); \
  NIRVA_IMPL_ASSIGN(structurals, get_array_bundleptr, from_attr_value(attr)); \
  NIRVA_ASSIGN_FROM_ARRAY(subsystem, structurals, subtype);		\
  NIRVA_ARRAY_FREE(structurals) NIRVA_WHILE(0)

#ifdef IMPL_FUNC_build_prime_mover
#define _NIRVA_DEPLOY_def_build_prime_mover
#else
#define IMPL_FUNC_build_prime_mover _def_build_prime_mover
#define _NIRVA_DEPLOY_def_build_prime_mover				\
    NIRVA_DEF_FUNC(NIRVA_NO_RETURN, _def_build_prime_mover)		\
      NIRVA_ASSERT_NULL(STRUCTURE_PRIME, NIRVA_FATAL, "build_prime_mover called with non-NULL" \
			"STRUCTURE_PRIME. This is not allowed.");	\
    NIRVA_CALL_ASSIGN(STRUCTURE_PRIME, create_bundle_by_type, OBJECT_TEMPLATE_BUNDLE_TYPE, \
		      STRAND_OBJECT_TYPE, OBJECT_TYPE_STRUCTURAL, NIRVA_NULL) \
      NIRVA_FUNC_END
#endif

  // so first we create the object_template itself
  // we assume that all bndledefs have already been set up, we can just create this from the
  // bundledef

  // we can set these in attr_desc (templates)
  // or in attributes (runtime)
  // in attr_desc, the we dont have a value, we can only set the default.
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
  // during bootstrap init willfirst create sufficient infrastructure to support creating
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

  /* #define _INLINE_INTERNAL_nirva_get_counted_array(xtypw, bundle, name) NIRVA_INLINE \ */
  /*     NIRVA_DEF_VARS(xtype, array)					\ */
  /*       NIRVA_DEF_VARS(NIRVA_INT, nitems)					\ */
  /*       NIRVA_IMPL_ASSIGN(nvals, nirva_get__array, bundleptr, attrs, attr_bun, \ */
  /* 			   BUNDLE_STANDARD_ATTRIBUTE)			\ */

  /*   // can be pverridden */
  /* #define _INLINE_INTERNAL_lookup_item_in_array(attr_bun, name) NIRVA_INLINE	\ */
  /*     (NIRVA_DEF_VARS(NIRVA_BUNDLEPTR_ARRAY, attrs)			\ */
  /*      NIRVA_ASSIGN_INTERNAL(nvals, nirva_get_counted_array, bundleptr, attrs, attr_bun, \ */
  /* 			   BUNDLE_STANDARD_ATTRIBUTE)			\ */
  /*      NIRVA_ASSIGN(attr, NIRVA_FOREACH_UNTIL(attrs, nvals,NIRVA_STRING_MATCH, \ */
  /* 					    COND_STRAND_VAL,STRAND_GENERIC_NAME, COND_STRING_VAL,name)) \ */
  /*      NIRVA_ARRAY_FREE(attrs)) */

#ifndef NEED_NIRVA_GET_VALUE_UINT
#define NIRVA_CALL_METHOD_nirva_get_value_uint IMPL
#else
#undef NEED_NIRVA_GET_VALUR_UINT
#define NIRVA_CALL_METHOD_nirva_get_value_uint MACRO
#define def_nirva_get_value_uint(bundle, strand) NIRVA_CAST_TO(NIRVA_UINT,\							       nirva_get_value_int(bundle,strand))
#endif

#ifndef NEED_NIRVA_GET_VALUE_UINT64
#define NIRVA_CALL_METHOD_nirva_get_value_uint64 IMPL
#else
#undef NEED_NIRVA_GET_VALUR_UINT64
#define NIRVA_CALL_METHOD_nirva_get_value_uint64 MACRO
#define def_nirva_get_value_uint64(bundle, strand) NIRVA_CAST_TO(NIRVA_UINT64,\							       nirva_get_value_int64(bundle,strand))
#endif

#define MY_ROLE_IS(role) 0

#define NIRVA_CALL_METHOD_nirva_wait_retry IMPL

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

#define NIRVA_CALL_METHOD_nirva_rev_map_vargs INTERN
  static NIRVA_BUNDLEPTR  _INTERNAL_FUNC_nirva_rev_map_vargs(NIRVA_BUNDLEPTR attrs, \
							     NIRVA_BUNDLEPTR revmap, \
							     NIRVA_VA_LIST vargs) {
    return NULL;

  }


#define NIRVA_CALL_METHOD_nirva_tx_action INTERN
  NIRVA_DEF_FUNC_INTERNAL(NIRVA_FUNC_RETURN, nirva_tx_action, NIRVA_BUNDLEPTR bin, \
			  NIRVA_BUNDLEPTR bout)
    NIRVA_RETURN(NIRVA_RESULT_SUCCESS)
    NIRVA_FUNC_END


    static void NIRVA_ERR_CHK(NIRVA_BUNDLEPTR *b, ...) {
  }

#ifndef NEED_NIRVA_RECYCLE
#define _NIRVA_DEPLOY_def_nirva_recycle
#else
#undef NEED_NIRVA_RECYCLE
#define NIRVA_CALL_METHOD_nirva_recycle IMPL
#define _NIRVA_DEPLOY_def_nirva_recycle					\
  NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, def_nirva_recycle, NIRVA_BUNDLEPTR bun) \
    NIRVA_ASSERT(bun, NIRVA_RETURN, NIRVA_RESPONSE_ERROR)		\
    NIRVA_CALL(nirva_bundle_free)					\
    NIRVA_FUNC_END
#endif

#define NIRVA_CALL_METHOD_make_attr_container INTERN
  NIRVA_DEF_FUNC_INTERNAL(NIRVA_NO_RETURN, make_attr_container,		\
			  NIRVA_BUNDLEPTR *bun, ...)			\
    NIRVA_DEF_VARS(NIRVA_VA_LIST, va)					\
    NIRVA_VA_START(va, bun)						\
    _INTERNAL_FUNC_make_n_bundles(1, bun, ATTR_GROUP_BUNDLE_TYPE, va);	\
  NIRVA_VA_END(va)							\
    NIRVA_FUNC_END

#ifndef NEED_NIRVA_ACTION
#define _NIRVA_DEPLOY_def_nirva_action
#else
#undef NEED_NIRVA_ACTION
#define NIRVA_CALL_METHOD_nirva_action IMPL
#define _NIRVA_DEPLOY_def_nirva_action					\
    NIRVA_DEF_FUNC(NIRVA_FUNC_RETURN, _def_nirva_action,		\
		   NIRVA_CONST_STRING funcname, NIRVA_VARIADIC)		\
      NIRVA_DEF_VARS(nirva_bundleptr_t, xtarget, contract, revpmap)	\
      NIRVA_DEF_VARS(nirva_bundleptr_t, *attrs = NULL, *pb_in, *pb_out)	\
      NIRVA_DEF_VARS(nirva_bundleptr_t, b_in, b_out)			\
      b_in = b_out = NULL;						\
    pb_in = &b_in;							\
    pb_out = &b_out;							\
    NIRVA_DEF_VARS(NIRVA_VA_LIST, vargs)				\
      NIRVA_DEF_VARS(NIRVA_FUNC_RETURN, result)				\
      NIRVA_DEF_VARS(NIRVA_COND_RESULT, condres)			\
      NIRVA_CALL(make_func_bundles, pb_in, pb_out, STRAND_GENERIC_NAME, \
		 funcname, NULL, NULL)					\
      NIRVA_ASSERT(*pb_in, NIRVA_FAIL, NIRVA_NULL)			\
      NIRVA_ASSERT(*pb_out, NIRVA_FAIL, NIRVA_NULL)			\
      /* IMPL_FUNC_find_tx_by_name - takes input "func_name" from input and returns	*/ \
      /*    "target_object" and "target_dest" in output - in this case target_dest is the contract */ \
      NIRVA_CALL_ASSIGN(xtarget, nirva_get_value_bundleptr, *pb_out, BUNDLE_EMISSION_TARGET) \
      NIRVA_ASSERT(xtarget, NIRVA_FAIL, NIRVA_NULL)			\
      NIRVA_IMPL_ASSIGN(contract, nirva_get_value_bundleptr, *pb_out, BUNDLE_EMISSION_DEST) \
      NIRVA_ASSERT(contract, NIRVA_FAIL, NIRVA_NULL)			\
    NIRVA_CALL(nirva_recycle, *pb_in)					\
    NIRVA_CALL(nirva_recycle, *pb_out)					\
    b_in = b_out = NULL;						\
    pb_in = &b_in;							\
    pb_out = &b_out;							\
    NIRVA_CALL(make_func_bundles, pb_in, pb_out, STRAND_GENERIC_NAME,	\
	       funcname, NULL, NULL)					\
      NIRVA_ASSERT(*pb_in, NIRVA_FAIL, NIRVA_NULL)			\
      NIRVA_ASSERT(*pb_out, NIRVA_FAIL, NIRVA_NULL)			\
      NIRVA_CALL_ASSIGN(revpmap, nirva_get_value_bundleptr, contract, BUNDLE_FUNCTION_REV_PMAP) \
      NIRVA_CALL(make_attr_container, attrs, NIRVA_NULL)		\
      NIRVA_ASSERT(attrs, NIRVA_FAIL, NIRVA_NULL)			\
      NIRVA_CALL(set_strand_val,*pb_in, BUNDLE_STANDARD_ATTR_GROUP, STRAND_TYPE_BUNDLEPTR,attrs) \
    NIRVA_VA_START(vargs, funcname)					\
    /* if params include output, then it is returned so we can pass it to tx */ \
    NIRVA_CALL_ASSIGN(*pb_out, nirva_rev_map_vargs, *attrs, revpmap, vargs); \
    NIRVA_VA_END(vargs)							\
      NIRVA_ERR_CHK(attrs, BUNDLE_STANDARD_ERROR);			\
    /* if it is async, then we ask t_bun to run it (TODO) */		\
    NIRVA_ERR_CHK(pb_in, BUNDLE_STANDARD_ERROR);			\
    NIRVA_DO								\
    /* even though it is no_neg, we need to run a cond_check to see if we can run it */ \
    NIRVA_COND_CHECK(condres, NIRVA_SUB_BUNDLE(contract, BUNDLE_CONTRACT_REQUIREMENTS), \
		     *pb_in, *pb_out);					\
    if (condres ==  NIRVA_COND_WAIT_RETRY) NIRVA_CALL(nirva_wait_retry); \
    NIRVA_WHILE(condres ==  NIRVA_COND_WAIT_RETRY)			\
      NIRVA_CALL_IF_EQUAL(condres, NIRVA_COND_FAIL, NIRVA_FAIL, NIRVA_NULL) \
      NIRVA_CALL_ASSIGN(result, nirva_tx_action, *pb_in, *pb_out);	\
    NIRVA_CALL(nirva_recycle, *pb_in);					\
    NIRVA_RETURN(result)						\
      NIRVA_FUNC_END
#endif

#define ADDC(text) "#" text
#define ADDS(text) ADDC(#text)
#define XMB(name, pfx,  ...) make_bundledef(name, pfx,"" __VA_ARGS__)
#define XMBX(name) XMB(#name, NULL, _##name##_BUNDLE)

#define REG_ALL_ITEMS(DOMAIN)						\
    make_bundledef(NULL, "STRAND_" #DOMAIN "_", MS("" ALL_STRANDS_##DOMAIN), NULL), \
      make_bundledef(NULL, "BUNDLE_" #DOMAIN "_", MS("" ALL_BUNDLES_##DOMAIN), NULL)

#define CONCAT_DOMAINS(MACRO) make_bundledef(NULL, NULL, FOR_ALL_DOMAINS(MACRO), NULL)

#define REG_ALL CONCAT_DOMAINS(REG_ALL_ITEMS)

#define INIT_CORE_DATA_BUNDLES BUNLISTx(BDEF_IGN,BDEF_INIT,,) all_def_strands = REG_ALL;
#define INIT_CORE_ATTRDEF_PACKS ABUNLISTx(BDEF_IGN,ATTR_BDEF_INIT,,) all_attrdef_packs = REG_ALL_ATTRS;

#define _INIT_IMPL_FUNC(fname,...) impl_func_##fname = IMPL_FUNC_##fname;
#define _INIT_IMFN(ftype,nnn) _CALL(_INIT_IMPL_FUNC, NIRVA_##ftype##_FUNC_##nnn)
#define INIT_IMPL_FUNCS _INIT_IMFN(OPT,001) _INIT_IMFN(OPT,002) _INIT_IMFN(OPT,003) \
    _INIT_IMFN(OPT,004)
#define _INIT_CORE_BUNDLES INIT_IMPL_FUNCS INIT_CORE_DATA_BUNDLES //INIT_CORE_ATTR_DEF_PACKS

#define _DEFINE_CORE_BUNDLES						\
  NIRVA_CONST_BUNDLEPTR STRUCTURE_PRIME;				\
  NIRVA_CONST_BUNDLEPTR STRUCTURE_APP;					\
  NIRVA_CONST_STRING_ARRAY maker_get_bundledef(bundle_type btype);	\
  _NIRVA_DEPLOY_MAKE_STRANDS_FUNC					\
  _NIRVA_DEPLOY_MAKE_BUNDLEDEF_FUNC					\
  _NIRVA_DEPLOY_def_nirva_wait_retry					\
  _NIRVA_DEPLOY_def_nirva_action					\
  _NIRVA_DEPLOY_def_build_prime_mover					\
  const char **all_def_strands, BUNLIST(BDEF_DEF_CONCAT,**,_BUNDLEDEF = NULL) \
    **LAST_BUNDLEDEF = NULL;						\
  const char **all_attr_packs, ABUNLIST(ATTR_BDEF_DEF_CONCAT,**,_ATTR_GROUP = NULL) \
    **LAST_ATTR_GROUP = NULL;						\
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

  ///////////////
  //////////////

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

  //		DEFINE_CORE_BUNDLES
  // This macro will create the default enums and so on
  // as well as "deploying" helper functions required for the bootstrap

  //		INIT_CORE_BUNDLES
  // This should be placed inside a function
  // The macro will set up the runtime dependent defintions

  // there are a few steps needed to bootstrap the structure. These can be done
  // from the same function which includes INIT_CORE_BUNDLES or elsewhere
  // See the documentation for details.
  //
  // The first time the application is run with NIRVA inside, nirvascope will open,
  // and the procedure for migrating existing code and for building / customising
  // will be available.
  //////////////
  ////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */
