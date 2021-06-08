#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef PLUGIN_UID
#error "PLUGIN_UID must be defined before including lives_plugin.h.\n[This should be a randomly generated 64 bit (unsigned) \
integer, which must NEVER change for the plugin]"
#endif

#if !defined _LIVES_PLUGIN_H_INC_1_ && !defined _LIVES_PLUGIN_H_
#define _LIVES_PLUGIN_H_INC_1_ 1
// pre-defined plugin types (normally set in a type specific header)
// some plugins may choose to omit this and set PLUGIN_INTENTCAPS instead
#define PLUGIN_TYPE_DECODER		256//		"decoder"
#define PLUGIN_TYPE_ENCODER		257//		"encoder"
#define PLUGIN_TYPE_FILTER		258//		"filter"
#define PLUGIN_TYPE_SOURCE		259//		"source"
#define PLUGIN_TYPE_PLAYER      	260//		"player"

// package types (normally set in type specific header)
#define PLUGIN_PKGTYPE_DYNAMIC 		128//	dynamic library (default)
#define PLUGIN_PKGTYPE_EXE 		129//	binary executable (plugin should be called from commandline)
#define PLUGIN_PKGTYPE_SCRIPT 		130//	interpreted script (plugin is a script that rqeuires some type of parser)

// package development state
// e.g. #define PLUGIN_DEVSTATE PLUGIN_DEVSTATE_TESTING
// default is PLUGIN_DEVSTATE_CUSTOM

#define PLUGIN_DEVSTATE_NORMAL 0 // normal development status, presumed bug free
#define PLUGIN_DEVSTATE_RECOMMENDED 1 // recommended, suitable for default use
#define PLUGIN_DEVSTATE_CUSTOM 2 // plugin is not normally shipped with base package
#define PLUGIN_DEVSTATE_TESTING 3 // waring - plugin is being tested and may not function correctly

#define PLUGIN_DEVSTATE_UNSTABLE -1 // warning - may be unstable in specific circumstances
#define PLUGIN_DEVSTATE_BROKEN -2 // WARNING - plugin is know to function incorrectly
#define PLUGIN_DEVSTATE_AVOID -3 // WARNING - plugin should be completely ignored

#include <error.h>
#define __QUOTE__ME__(x) #x
static inline void _show_lp_warn(void) {
  error(1, 0, "lives-plugin.h must be included twice, once to define constants like PLUGIN_TYPE, and then a second time\n"
        "with the constants already defined.\nI am going to exit now until you fix plugin with PLUGIN_UID %s \n",
        __QUOTE__ME__(PLUGIN_UID));
}

#define _make_plugin_id() _show_lp_warn()
#else

#ifndef _LIVES_PLUGIN_H_

#undef _make_plugin_id

#define MIN_WEED_ABI_VERSION 201

#if !defined __WEED_H__ || WEED_ABI_VERSION < MIN_WEED_ABI_VERSION
#error "You must include weed.h version " MIN_WEED_ABI_VERSION " or higher to use lives-plugin.h"
#endif

/* PLUGIN_API_VERSION_MAJOR and PLUGIN_API_VERSION_MINOR should NOT be defined by individual plugins,
   they should be defined in a header file for the plugin type (if appropriate) */

/* PLUGIN_NAME, PLUGIN_PKGTYPE, PLUGIN_DEVSTATE and PLUGIN_SCRIPT_LANG may optionally be defined */

#if !defined PLUGIN_TYPE && !defined PLUGIN_INTENTCAPS
#error "You must define either PLUGIN_TYPE or PLUGIN_INTENTCAPS before including lives-plugin.h"
#endif
#ifndef PLUGIN_VERSION_MAJOR
#error "You must define PLUGIN_VERSION_MAJOR before including lives-plugin.h"
#endif
#ifndef PLUGIN_VERSION_MINOR
#error "You must define PLUGIN_VERSION_MINOR before including lives-plugin.h"
#endif

#ifndef _LIVES_PLUGIN_H_VERSION_
#define _LIVES_PLUGIN_H_VERSION_ 201

#include <inttypes.h>

#ifndef __WEED_UTILS_H__
// internal functions
#ifdef FN_TYPE
#define OFN_TYPE FN_TYPE
#undef FN_TYPE
#endif

#define FN_TYPE static inline

#ifdef __weed_get_value__
#undef __weed_get_value__
#endif
#ifdef __weed_check_leaf__
#undef __weed_check_leaf__
#endif

/* functions need to be defined here for the plugin, else it will use the host versions, breaking function overloading */
#ifdef __weed_get_value__
#undef __weed_get_value__
#endif
#ifdef __weed_check_leaf__
#undef __weed_check_leaf__
#endif

#define __weed_get_value__(plant, key, value) weed_leaf_get(plant, key, 0, value)
#define __weed_check_leaf__(plant, key) __weed_get_value__(plant, key, NULL)

/* check for existence of a leaf; leaf must must have a value and not just a seed_type, returns WEED_TRUE or WEED_FALSE */
FN_TYPE int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  return __weed_check_leaf__(plant, key) == WEED_SUCCESS ? WEED_TRUE : WEED_FALSE;
}

#define _WEED_SET_(stype) return weed_leaf_set(plant, key, WEED_SEED_##stype, 1, (weed_voidptr_t)&value);
#define _WEED_SET_P(stype) return weed_leaf_set(plant, key, WEED_SEED_##stype, 1, value ? (weed_voidptr_t)&value : NULL);

FN_TYPE weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {_WEED_SET_(INT)}
FN_TYPE weed_error_t weed_set_double_value(weed_plant_t *plant, const char *key, double value) {_WEED_SET_(DOUBLE)}
FN_TYPE weed_error_t weed_set_boolean_value(weed_plant_t *plant, const char *key, int32_t value) {_WEED_SET_(BOOLEAN)}
FN_TYPE weed_error_t weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {_WEED_SET_(INT64)}
FN_TYPE weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {_WEED_SET_(STRING)}
FN_TYPE weed_error_t weed_set_funcptr_value(weed_plant_t *plant, const char *key, weed_funcptr_t value) {_WEED_SET_P(FUNCPTR)}
FN_TYPE weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {_WEED_SET_P(VOIDPTR)}
FN_TYPE weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {_WEED_SET_P(PLANTPTR)}

#undef _WEED_SET_

FN_TYPE weed_error_t __weed_leaf_check__(weed_plant_t *plant, const char *key, uint32_t seed_type) {
  weed_error_t err = __weed_check_leaf__(plant, key);
  return err != WEED_SUCCESS ? err
         : weed_leaf_seed_type(plant, key) != seed_type ? WEED_ERROR_WRONG_SEED_TYPE : WEED_SUCCESS;
}

FN_TYPE weed_voidptr_t __weed_value_get__(weed_plant_t *plant, const char *key, uint32_t seed_type,
    weed_voidptr_t retval, weed_error_t *error) {
  weed_error_t err, *perr = (error ? error : &err);
  if ((*perr = __weed_leaf_check__(plant, key, seed_type)) == WEED_SUCCESS) * perr = __weed_get_value__(plant, key, retval);
  return retval;
}

#define _WEED_GET_(ctype, stype) ctype retval; \
  return *((ctype *)(__weed_value_get__(plant, key, WEED_SEED_##stype, (weed_voidptr_t)&retval, error)));

#undef FN_TYPE
#define FN_TYPE OFN_TYPE

#endif // undef __WEED_UTILS_H__

#ifndef TRUE
#undef FALSE
#define TRUE WEED_TRUE
#define FALSE WEED_FALSE
#endif

typedef int32_t plugin_intention;

// capacities
typedef weed_plant_t plugin_capacity_t;

typedef struct {
  plugin_intention intent;
  plugin_capacity_t *capacities; ///< type specific capabilities
} plugin_intentcap_t;

typedef struct {
  uint64_t uid; // fixed enumeration
  uint64_t type;  ///< e.g. "decoder"
  uint64_t pkgtype;  ///< e.g. dynamic
  char script_lang[32];  ///< for scripted types only, the script interpreter, e.g. "perl", "python3"
  int api_version_major; ///< version of interface API
  int api_version_minor;
  char name[32];  ///< e.g. "mkv_decoder"
  int pl_version_major; ///< version of plugin
  int pl_version_minor;
  int devstate; // e.g. PLUGIN_DEVSTATE_UNSTABLE
  plugin_intentcap_t *intentcaps;  /// array of intentcaps (NULL terminated)
  void *unused; // padding
} plugin_id_t;

//////////////////////////////////////////////////////////////////
// -- specifics for individual plugins

// intentcaps

// intentions
#define PLUGIN_INTENTION_DECODE 	0x00001000

#define PLUGIN_INTENTION_PLAY		0x00000200
#define PLUGIN_INTENTION_STREAM		0x00000201
#define PLUGIN_INTENTION_TRANSCODE	0x00000202

static plugin_id_t plugin_id;

static inline plugin_id_t *_make_plugin_id(void) {
  static int inited = 0;
  if (!inited) {
    inited = 1;
    plugin_id.uid = PLUGIN_UID;
#ifndef PLUGIN_TYPE
#define PLUGIN_TYPE_UNSPECIFIED
#endif
    plugin_id.type = PLUGIN_TYPE;
#ifndef PLUGIN_PKGTYPE
#define PLUGIN_PKGTYPE PLUGIN_PKGTYPE_DYNAMIC
#endif
    plugin_id.pkgtype = PLUGIN_PKGTYPE;
#ifndef PLUGIN_SCRIPT_LANG
#define PLUGIN_SCRIPT_LANG "\0"
#endif
    snprintf(plugin_id.script_lang, 32, "%s", PLUGIN_SCRIPT_LANG);
#endif
#ifndef PLUGIN_DEVSTATE
#define PLUGIN_DEVSTATE PLUGIN_DEVSTATE_CUSTOM
#endif
    plugin_id.devstate = PLUGIN_DEVSTATE;
    plugin_id.api_version_major = PLUGIN_API_VERSION_MAJOR;
    plugin_id.api_version_major = PLUGIN_API_VERSION_MINOR;
    snprintf(plugin_id.name, 32, "%s", PLUGIN_NAME);
    plugin_id.pl_version_major = PLUGIN_VERSION_MAJOR;
    plugin_id.pl_version_minor = PLUGIN_VERSION_MINOR;
#ifndef PLUGIN_INTENTCAPS
#define PLUGIN_INTENTCAPS NULL
#endif
    plugin_id.intentcaps = PLUGIN_INTENTCAPS;
  }
  return &plugin_id;
}

#ifndef HAVE_GET_PLUGIN_ID
const plugin_id_t *get_plugin_id_default(void) {
  // set plugin_id for the plugin, and return it to the host
  return _make_plugin_id();
}
#endif

// generic capacities, type specific ones may also exist
// key name is defined here. Values are int32_t interprited as boolean: FALSE (0) or TRUE (1 or non-zero)
// absent values are assumed FALSE
#define PLUGIN_CAPACITY_LOCAL		"local"
#define PLUGIN_CAPACITY_REMOTE		"remote"

#define PLUGIN_CAPACITY_VIDEO		"video"
#define PLUGIN_CAPACITY_AUDIO		"audio"
#define PLUGIN_CAPACITY_TEXT		"text"

#define PLUGIN_CAPACITY_DATA		"data"

#define plugin_capcity_set(caps, key) (caps ? weed_set_boolean_value(caps, key, 0) : FALSE)
#define plugin_capacity_set_int(caps, key) (caps ? weed_set_int_value(caps, key, 0) : 0)
#define plugin_capacity_set_string(caps, key) (caps ? weed_set_string_value(caps, key, 0) : 0)

//

// requirements - for future use

#define PLUGIN_REQUIREMENT_AUDIO_CHANS	"audio_channels"
#define PLUGIN_REQUIREMENT_AUDIO_RATE	"audio_rate"

#define plugin_requirement_is_readonly(caps, key) (caps ? ((weed_leaf_get_flags(caps, key) & WEED_FLAG_IMMUTABLE) \
						      ? TRUE : FALSE) : FALSE)

#define plugin_requirement_get(caps, key) (caps ? weed_get_boolean_value(caps, key, 0) : FALSE)
#define plugin_requirement_get_int(caps, key) (caps ? weed_get_int_value(caps, key, 0) : 0)
#define plugin_requirement_get_string(caps, key) (caps ? get_string_value(caps, key, 0) : 0)

// std functions

// host should call this if the plugin defines it, otherwise get_plugin_id_default may be used instead
// HAVE_GET_PLUGIN_ID may optionally be defined to avoid exporting get_plugin_id_default()
const plugin_id_t *get_plugin_id(void);

// in future, a plugin will only have the one fixed function: get_plugin_id(), either implementing its own or
// via the default get_plugin_id_default().
// instead of functions it will define a set of intentcaps
// intent satisfied + capababilities -> each of these will be linked to a 'transform' function which
// will define how the intent is satisfied.
// The transform will define requirements (like paramater values) to be filled by the host

// (the plugin may also supply an interface to the host to fill in missing values from the user)

// - some requirements may be objects, the plugin may change the state and occasionally the subtype of an object

// the transform may also have outputs (similar to requirements, but set by the plugin), and should return a status to the caller
// some statuses may be ongoing (running, need_data); setting 'need_data' prompts the host or another object to
// update the value(s) of one or more requirements during the transform. other statuses include error or cancelled
// - some errors may be more severe and change an object state to an error state - the host may then try to find
// a transform which returns the object state to normal.

// some transforms are passive and simply set ouput values, or collect requirements (i.e getters and setters)
// process transforms convert inputs to outputs, and active transforms alter the object state or subtype
// may produce new objects, or finalise objects (freeing resources and setting state to final)

// A plugin is also a type of object: initially a template type.
// It may provide transforms to create instances of itself,
// copy them and destroy them; initialise / deinitialise them (changing their state), or altering their subtype
// many transforms in the plugin will require an instance of themselves, with a particular state / subtype, as a requirement
// it is up to the host to navigate the sequence of transfroms to satisft any particular intentcap.
//
// Transforms, as well as all the above, may have hooks, certain types of objects can act as "attachments" and provide
// co-processing during a transform, e.g. an effect plugin (object) may declare instances of itself in the "inited"
// state to be attachments to the 'playing' intent, with video capacity, the requirements being parameters +
// the frame (layer) object from the player, which the hook provides.
//
// details of all this are still in the process of being elaborated / tested

#endif // ...INC1
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

//#endif /* _LIVES_PLUGIN_H_VERSION_ */
