#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef NO_STDARGS
#include <stdarg.h>
#endif

#ifdef ALLOW_UNUSED
#undef ALLOW_UNUSED
#endif

#ifdef __GNUC__
#define ALLOW_UNUSED __attribute__((unused))
#else
#define ALLOW_UNUSED
#endif

#ifndef PLUGIN_UID
#error "PLUGIN_UID must be defined before including lives_plugin.h.\n[This should be a randomly generated 64 bit (unsigned) \
integer, which must NEVER change for the plugin]"
#endif

#if !defined _LIVES_PLUGIN_H_INC_1_ && !defined _LIVES_PLUGIN_H_
#define _LIVES_PLUGIN_H_INC_1_ 1

#define _PL_ERROR_ATTRIBUTE_INVALID		WEED_ERROR_WRONG_SEED_TYPE

#define PL_ATTR_INT		WEED_SEED_INT
#define PL_ATTR_DOUBLE		WEED_SEED_DOUBLE
#define PL_ATTR_BOOLEAN		WEED_SEED_BOOLEAN
#define PL_ATTR_STRING		WEED_SEED_STRING
#define PL_ATTR_INT64		WEED_SEED_INT64
#define PL_ATTR_FUNCPTR		WEED_SEED_FUNCPTR
#define PL_ATTR_VOIDPTR		WEED_SEED_VOIDPTR
#define PL_ATTR_PLANTPTR       	WEED_SEED_PLANTPTR

#define _ADD_CONSTANTS_

////////////////////////////////////////////////

// pre-defined plugin types (normally set in a type specific header)
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

#define _make_plugin_id(a) _show_lp_warn()
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

#if !defined PLUGIN_TYPE && !defined PLUGIN_CONTRACT
#error "You must define either PLUGIN_TYPE or pass **contracts before including lives-plugin.h"
#endif
#ifndef PLUGIN_VERSION_MAJOR
#error "You must define PLUGIN_VERSION_MAJOR before including lives-plugin.h"
#endif
#ifndef PLUGIN_VERSION_MINOR
#error "You must define PLUGIN_VERSION_MINOR before including lives-plugin.h"
#endif

#ifndef _LIVES_PLUGIN_H_VERSION_
#define _LIVES_PLUGIN_H_VERSION_ 300

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

#define _WEED_SET_(stype) return weed_leaf_set(plant, key, PL_ATTR_##stype, 1, (weed_voidptr_t)&value);
#define _WEED_SET_P(stype) return weed_leaf_set(plant, key, PL_ATTR_##stype, 1, value ? (weed_voidptr_t)&value : NULL);

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

#define _WEED_GET_(ctype, stype) ctype retval;				\
  return *((ctype *)(__weed_value_get__(plant, key, PL_ATTR_##stype,(weed_voidptr_t)&retval,error)));

#undef FN_TYPE
#define FN_TYPE OFN_TYPE

#endif // undef __WEED_UTILS_H__

#ifndef TRUE
#undef FALSE
#define TRUE WEED_TRUE
#define FALSE WEED_FALSE
#endif

typedef weed_plant_t pl_capacities;
typedef weed_plant_t pl_attribute;
typedef int32_t pl_intention;
typedef weed_error_t pl_error_t;

#define ICAP_DESC_LEN 64

typedef struct {
  char desc[ICAP_DESC_LEN];  // descriptive name of icaps (e.g load, download)
  pl_intention intention;
  pl_capacities *capacities; ///< type specific capabilities
} pl_intentcap;

typedef void pl_requirements;		// TBD

typedef struct {
  pl_intentcap *icap;
  pl_attribute **attributes;
  pl_requirements *reqs; // requirements that must be met, aside from mandatory attrs (TBD)
  int32_t flags; // flags to denote what is being negotiated
} pl_contract;

#define PL_DEFAULT WEED_LEAF_DEFAULT
#define PL_FLAGS WEED_LEAF_FLAGS
#define PL_VALUE WEED_LEAF_VALUE
#define PL_NAME WEED_LEAF_NAME
#define PL_UID "uid"
#define PL_OWNER "owner_uid"

const char *weed_type_to_string(uint32_t wtype) {
  switch (wtype) {
  case 1: return "int";
  case 2: return "double";
  case 3: return "boolean";
  case 4: return "string";
  case 5: return "int64";
  case 64: return "weed_funcptr_t";
  case 65: return "void *";
  case 66: return "weed_plant_t *";
  default: return "?";
  }
}

#ifdef NO_STDARGS
#define _pl_attr_set_value(a, v) (1)
#define _pl_attr_set_array(a, n, v) (1)
#define _pl_attr_set_def_value(a, v) (1)
#define _pl_attr_set_def_array(a, n, v) (1)
#define _pl_declare_attribute(a, b, c) (0)

#else

static pl_error_t _set_con_attr_vargs(pl_attribute  *attr, const char *key,
                                      int ne, va_list args);
static pl_error_t _set_con_attr_vargs(pl_attribute  *attr, const char *key,
                                      int ne, va_list args) {
  pl_error_t err; uint32_t st = weed_leaf_seed_type(attr, PL_DEFAULT);
  if (ne == 1) {
    switch (st) {
    case PL_ATTR_INT: {
      int val = va_arg(args, int);
      err = weed_set_int_value(attr, key, val); break;
    }
    case PL_ATTR_BOOLEAN: {
      int val = va_arg(args, int);
      err = weed_set_boolean_value(attr, key, val); break;
    }
    case PL_ATTR_DOUBLE: {
      double val = va_arg(args, double);
      err = weed_set_double_value(attr, key, val); break;
    }
    case PL_ATTR_INT64: {
      int64_t val = va_arg(args, int64_t);
      err = weed_set_int64_value(attr, key, val); break;
    }
    case PL_ATTR_STRING: {
      char *val = va_arg(args, char *);
      err = weed_set_string_value(attr, key, val); break;
    }
    case PL_ATTR_VOIDPTR: {
      void *val = va_arg(args, void *);
      err = weed_set_voidptr_value(attr, key, val); break;
    }
    case PL_ATTR_FUNCPTR: {
      weed_funcptr_t val = va_arg(args, weed_funcptr_t);
      err = weed_set_funcptr_value(attr, key, val); break;
    }
    case PL_ATTR_PLANTPTR: {
      weed_plantptr_t val = va_arg(args, weed_plantptr_t);
      err = weed_set_plantptr_value(attr, key, val); break;
    }
    default: return _PL_ERROR_ATTRIBUTE_INVALID;
    }
  } else {
#ifdef __WEED_UTILS_H__
    switch (st) {
    case PL_ATTR_INT: {
      int *vals = va_arg(args, int *);
      err = weed_set_int_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_BOOLEAN: {
      int *vals = va_arg(args, int *);
      err = weed_set_boolean_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_DOUBLE: {
      double *vals = va_arg(args, double *);
      err = weed_set_double_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_INT64: {
      int64_t *vals = va_arg(args, int64_t *);
      err = weed_set_int64_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_STRING: {
      char **vals = va_arg(args, char **);
      err = weed_set_string_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_VOIDPTR: {
      void **vals = va_arg(args, void **);
      err = weed_set_voidptr_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_FUNCPTR: {
      weed_funcptr_t *vals = va_arg(args, weed_funcptr_t *);
      err = weed_set_funcptr_array(attr, key, ne, vals); break;
    }
    case PL_ATTR_PLANTPTR: {
      weed_plantptr_t *vals = va_arg(args, weed_plantptr_t *);
      err = weed_set_plantptr_array(attr, key, ne, vals); break;
    }
    default: return _PL_ERROR_ATTRIBUTE_INVALID;
    }
#endif
  } return err;
}

pl_error_t _pl_attr_set_value(pl_attribute *attr, ...) {
  pl_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs; va_start(xargs, attr);
    err = _set_con_attr_vargs(attr, PL_VALUE, 1, xargs); va_end(xargs);
  } return err;
}
pl_error_t _pl_attr_set_def_value(pl_attribute *attr, ...) {
  pl_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs; va_start(xargs, attr);
    err = _set_con_attr_vargs(attr, PL_DEFAULT, 1, xargs); va_end(xargs);
  } return err;
}
pl_error_t _pl_attr_set_array(pl_attribute *attr, int ne, ...) {
  pl_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs; va_start(xargs, ne);
    err = _set_con_attr_vargs(attr, PL_VALUE, ne, xargs); va_end(xargs);
  } return err;
}
pl_error_t _pl_attr_set_def_array(pl_attribute *attr, int ne,  ...) {
  pl_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs; va_start(xargs, ne);
    err = _set_con_attr_vargs(attr, PL_DEFAULT, ne, xargs); va_end(xargs);
  } return err;
}

#endif

#define CAP_PREFIX "cap_"
#define CAP_PREFIX_LEN 4

char *_make_capnm(const char *name) {
  char *capname = (char *)calloc(1, strlen(name) + 1 + CAP_PREFIX_LEN);
  sprintf(capname, "%s%s", CAP_PREFIX, name);	return capname;
}

#define _pl_capacity_set(icap, name) do {char *capname = _make_capnm(name); \
    weed_set_boolean_value(icap->capacities, capname, TRUE); free(capname);} while (0);

#define _pl_capacity_unset(icap, name) do {char *capname = _make_capnm(name); \
    weed_leaf_delete(icap->capacities, capname); free(capname);} while (0);

static int _pl_has_capacity(pl_intentcap *icap, const char *capname) ALLOW_UNUSED;
static int _pl_has_capacity(pl_intentcap *icap, const char *name) {
  char *capname = _make_capnm(name);
  int ret = weed_plant_has_leaf(icap->capacities, capname); free(capname); return ret;
}

#define PL_FLAG_READONLY 1
#define PL_SUBTYPE_ATTR 129

static pl_attribute *_pl_contract_get_attr(pl_contract *cn, const char *aname) ALLOW_UNUSED;
static pl_attribute *_pl_contract_get_attr(pl_contract *cn, const char *aname) {
  for (int c = 0; cn->attributes[c]; c++) {
    char *n = weed_get_string_value(cn->attributes[c], PL_NAME, 0);
    if (!strcmp(aname, n)) {free(n); return cn->attributes[c];} free(n);
  } return 0;
}



#define _pl_contract_get_attr(contract, aname) _pl_contract_get_attr(contract, aname)
#define _pl_attribute_is_mine(attr) ((weed_get_int64_value((attr), PL_UID, 0) == PLUGIN_UID) \
				     ? TRUE : FALSE)
#define _pl_attr_set_rdonly(attr, state)				\
  (((attr) && weed_leaf_num_elements((attr), PL_VALUE)			\
    && pl_attribute_is_mine((attr))) ?					\
   ((state) ? (weed_set_int_value((attr), PL_FLAGS, (weed_get_int_value((attr), PL_FLAGS, 0) \
						     | PL_FLAG_READONLY)) == WEED_SUCCESS) \
    : (weed_set_int_value((attr), PL_FLAGS, (weed_get_int_value((attr), PL_FLAGS, 0) \
					     & ~PL_FLAG_READONLY)) == WEED_SUCCESS)) : 0)
#define _pl_attr_get_value(attr, type) (weed_get_##type##_value((attr), PL_VALUE, 0))
#define _pl_attr_get_array(attr, type) (weed_get_##type##_array((attr), PL_VALUE, 0))
#define _pl_attr_get_nvalues(attr) (weed_leaf_num_elements((attr), PL_VALUE, 0))
#define _pl_attr_get_type(attr) (type_to_string(weed_leaf_seed_type((attr), PL_DEFAULT)))
#define _pl_attr_get_def_value(attr, type) (weed_get_##type##_value((attr), PL_DEFAULT, 0))
#define _pl_attr_get_def_array(attr, type) (weed_get_##type##_array((attr), PL_DEFAULT, 0))
#define _pl_attr_get_def_nvalues(attr) (weed_leaf_num_elements((attr), PL_DEFAULT, 0))
#define _pl_attr_get_rdonly(attr) ((weed_get_int_value((attr), PL_FLAGS, 0) & PL_FLAG_READONLY)	\
				   ? TRUE : FALSE)
#define _pl_attr_get_value(attr, type) (weed_get_##type##_value((attr), PL_VALUE, 0))
#define _pl_attr_get_array(attr, type) (weed_get_##type##_array((attr), PL_VALUE, 0))
#define _pl_attr_get_nvalues(attr) (weed_leaf_num_elements((attr), PL_VALUE, 0))
#define _pl_attr_get_type(attr) (type_to_string(weed_leaf_seed_type((attr), PL_DEFAULT)))

#define st_invalid(st) ((((st) > 0 && (st) < 6) || (st) >= 64) ? TRUE:FALSE)

static pl_attribute *_pl_declare_attribute(pl_contract *contract,
    const char *name, uint32_t st) ALLOW_UNUSED;
static pl_attribute *_pl_declare_attribute(pl_contract *contract, const char *name, uint32_t st) {
  pl_attribute *attr = NULL; size_t slen; int c;
  if (!contract || !name || (slen = strlen(name)) < 6 || slen > 32 || st_invalid(st)) return 0L;
  for (c = 0; contract->attributes[c]; c++) {
    char *pname = weed_get_string_value(contract->attributes[c], PL_NAME, NULL);
    if (!strcmp(name, pname)) {free(pname); return 0;} free(pname);
  }
  contract->attributes = (pl_attribute **)realloc(contract->attributes,
                         (c + 2) * sizeof(pl_attribute *));
  if (contract->attributes) {
    pl_error_t err, err2;
    attr = weed_plant_new(PL_SUBTYPE_ATTR);
    if (attr) {
      weed_set_string_value(attr, PL_NAME, name);
      weed_set_int64_value(attr, PL_OWNER, PLUGIN_UID);
      weed_leaf_set(attr, PL_VALUE, st, 0, 0);
      weed_leaf_set(attr, PL_DEFAULT, st, 0, 0);
      (void) err; contract->attributes[c] = attr;
      contract->attributes[++c] = 0;
    }
  } return attr;
}

//////////////////////////////////////////////////////////

typedef struct {
  uint64_t uid; // fixed enumeration
  char name[32];  ///< e.g. "mkv_decoder"
  int pl_version_major; ///< version of plugin
  int pl_version_minor;
  int devstate; // e.g. LIVES_DEVSTATE_UNSTABLE

  // the following would normally be filled via a type specific header:
  uint64_t type;  ///< e.g. "decoder"
  int api_version_major; ///< version of interface API
  int api_version_minor;
  uint64_t pkgtype;  ///< e.g. dynamic
  char script_lang[32];  ///< for scripted types only, the script interpreter, e.g. "perl", "python3"

  // contracts advertise the functions which the plugin can carry out
  // this is done by setting the icaps of a contract. Then if another object is interested,
  // it can pass a copy of the contract to the negotiate_contract function.

  // SEE BELOW for more details
  const pl_contract **contracts;  /// array of contracts (at a minimim, plugin must set icaps
} plugin_id_t;

//////////////////////////////////////////////////////////////////
// -- specifics for individual plugins

// intentcaps

static plugin_id_t plugin_id;

static inline plugin_id_t *_make_plugin_id(const pl_contract **contracts) {
  static int inited = 0;
  if (!inited) {
    inited = 1;
    plugin_id.uid = PLUGIN_UID;
    snprintf(plugin_id.name, 32, "%s", PLUGIN_NAME);
    plugin_id.pl_version_major = PLUGIN_VERSION_MAJOR;
    plugin_id.pl_version_minor = PLUGIN_VERSION_MINOR;
#ifndef PLUGIN_DEVSTATE
#define PLUGIN_DEVSTATE PLUGIN_DEVSTATE_CUSTOM
#endif
    plugin_id.devstate = PLUGIN_DEVSTATE;
#ifndef PLUGIN_TYPE
#define PLUGIN_TYPE_UNSPECIFIED
#endif
    plugin_id.type = PLUGIN_TYPE;
    plugin_id.api_version_major = PLUGIN_API_VERSION_MAJOR;
    plugin_id.api_version_major = PLUGIN_API_VERSION_MINOR;
#ifndef PLUGIN_PKGTYPE
#define PLUGIN_PKGTYPE PLUGIN_PKGTYPE_DYNAMIC
#endif
    plugin_id.pkgtype = PLUGIN_PKGTYPE;
#ifndef PLUGIN_SCRIPT_LANG
#define PLUGIN_SCRIPT_LANG "\0"
#endif
    snprintf(plugin_id.script_lang, 32, "%s", PLUGIN_SCRIPT_LANG);
    //#endif
    if (contracts) plugin_id.contracts = contracts;
  }
  return &plugin_id;
}

#ifndef HAVE_GET_PLUGIN_ID
#ifndef HAVE_CONTRACTS
const plugin_id_t *get_plugin_id_default(void) {
  // set plugin_id for the plugin, and return it to the host
  return _make_plugin_id((const pl_contract **)NULL);
}
#else
const plugin_id_t *get_plugin_id_default(const pl_contract **contracts) {
  // set plugin_id for the plugin, and return it to the host

  return _make_plugin_id(contracts);
}
#endif

///////////////////////////////////

// std functions (older API)

// host should call this if the plugin defines it, otherwise get_plugin_id_default may be used instead
// HAVE_GET_PLUGIN_ID may optionally be defined to avoid exporting get_plugin_id_default()
const plugin_id_t *get_plugin_id(void);

#endif
#endif

#ifdef _ADD_CONSTANTS_
#undef _ADD_CONSTANTS_

// INTENTIONS
#define PL_INTENTION_DECODE 		0x00001000

#define PL_INTENTION_PLAY		0x00000200
#define PL_INTENTION_STREAM		0x00000201
#define PL_INTENTION_TRANSCODE		0x00000202

// generic capacities, type specific ones may also exist in another header
// values can either be present or absent
#define PL_CAPACITY_LOCAL		"local"
#define PL_CAPACITY_REMOTE		"remote"

#define PL_CAPACITY_REALTIME		"realtime"
#define PL_CAPACITY_DISPLAY		"display"

#define PL_CAPACITY_VIDEO		"video"
#define PL_CAPACITY_AUDIO		"audio"
#define PL_CAPACITY_TEXT		"text"

#define PL_CAPACITY_DATA		"data"

////// standard ATTRIBUTES ////

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
#define ATTR_VIDEO_FPS WEED_LEAF_FPS

// UI
#define ATTR_UI_RFX_TEMPLATE "ui_rfx_template"

#ifdef _PL_ERROR_ATTRIBUTE_INVALID
#define PL_ERROR_ATTRIBUTE_INVALID		_PL_ERROR_ATTRIBUTE_INVALID
#endif

// API functions (for NEW API) ////////////

// intentcaps
#define pl_contract_get_icap(contract) 	((contract) ? (contract)->icap : 0)

#define pl_capacity_set(icap, cname) 		_pl_capacity_set((icap), (name))
#define pl_capacity_unset(icap, cname) 		_pl_capacity_unset((icap), (name))
#define pl_has_capacity(icap, cname) 		_pl_has_capacity((icap), (cname))

// attributes
#define pl_contract_get_attr(contract, aname)	_pl_contract_get_attr((contract), (aname))

#define pl_attr_is_mine(attr) 			_pl_attr_is_mine((attr))
#define pl_attr_get_rdonly(attr)  		_pl_attr_get_rdonly((attr))
#define pl_attr_set_rdonly(attr, state) 	_pl_attr_set_rdonly((attr), (state))
#define pl_attr_get_value(attr, type) 		_pl_attr_get_value((attr), type)
#define pl_attr_get_array(attr, type) 		_pl_attr_get_array((attr), type)
#define pl_attr_get_nvalues(attr) 		_pl_attr_get_nvalues((attr))
#define pl_attr_get_type(attr) 			_pl_attr_get_type((attr))
#define pl_attr_set_value(attr, ...) 		_pl_attr_set_value((attr), __VA_ARGS__)
#define pl_attr_set_array(attr, nvals, ...) 	_pl_attr_set_array((attr), (nvals), __VA_ARGS__)

#define pl_attr_get_def_value(attr, type) 	_pl_attr_get_def_value((attr), type)
#define pl_attr_get_def_array(attr, type) 	_pl_attr_get_def_array((attr), type)
#define pl_attr_get_def_nvalues(attr) 		_pl_attr_get_def_nvalues((attr))
#define pl_attr_set_def_value(attr, ...) 	_pl_attr_set_def_value((attr), __VA_ARGS__)
#define pl_attr_set_def_array(attr, nvals, ...) _pl_attr_set_def_array((attr), (nvals), __VA_ARGS__)

#define pl_declare_attribute(contract, name, value_type)	\
  _pl_declare_attribute((contract), (name), (value_type))

// contracts:

// The HOW
// plugin creates a set of CONTRACTS, these


// The plugin may set requirements to run the associated transform, add attributes to
// be supplied, and set readonly attributes. negotiate_contract() can return status

// the plugin has contracts in its plugin_id, the host should make a copy and then pass it
//  to the plugin. Host should not free it unless plugin sets status cancelled or invalid,
// or the transform has completed with success

// the plugin may add its own attributes, these can be identified by the "owner"
// the host should not free these, the plugin must do that.

typedef enum {
  // the default if the contract is not being negotiated or acted on
  CONTRACT_STATUS_NONE,

  //AGREED means the host can now call run_transform(contract->transform)
  // there may be optional attributes which the plugin has declared which
  // the host can update. In this case thehost can update these and
  // call negotiate_contract once more.
  CONTRACT_STATUS_AGREED,

  // NOTREADY means the caller needs to fulfill more requirements.
  // The plugin should mark the requirements in question as unfullfilled (TBD)
  CONTRACT_STATUS_NOTREADY,

  // Wait means the plugin allows to run the transform but is currently busy,
  // this can happen for example if another thread is running a transform
  // which blocks this one, or the plugin is waitng on some external condition
  CONTRACT_STATUS_WAIT,

  // this status may be set by the plugin if another contract is received,
  // in order not to keep the other party held up, the caller will have a
  // short time to respond and only one more attempt, or if agreed it should
  // action the transform
  CONTRACT_STATUS_HURRY,

  // cancelled means the plugin is negotiateing another contract and is unable
  // to continue with this one, or else the host has decided not to run the
  // transform. In this case the party who is not the owner should
  // drop all references to the contract, as it can now be freed
  CONTRACT_STATUS_CANCELLED,

  // invalid means the plugin is unable to run the
  // contract transform due to some internal fault or error
  // or for any reason does not wish to run the transform
  // In this case the party who is not the owner should
  // drop all references to the contract, as it can now be freed
  CONTRACT_STATUS_INVALID,
} contract_status;

// THE CONCEPT
// in future, every plugin will be an OBJECT. and all plugins will have a common API
// There is only one FIXED function: get_plugin_id(),
// The plugin can either implement its own or just
// #define some constants and return a call to get_plugin_id_default().
// the plugin should return a pointer to a static plugin_id struct which is created.
// in future, the host will be able to pass in a template to this call, the plugin should
// use a standard API (lives stuct definition) to instantiate the struct and fill in the fields as
// possible
//
// Now to be more precise -
// plugins are TEMPLATE objects; and every object has a TYPE and a SUBTYPE,
// (although the subtype may be "no subtype") as well as a 64 bit randomly generated "unique id"
// For template objects, this number should never change

// Some objects can produce other objects - generally a template object would create
// various subtypes of the same type, these other objects are object instances.
// (some objects can produce template objects, these are known as Dictionaries)
// Some templates are singletons and do not produce any instances Objects can also produce
// objects of a different type, for example they may create some kind of file or media object.
//
// Every object has a STATE, for example NOT READY, READY, ERROR, etc. If the object does not set
// a state purposely then its state is UNDEFINED.
//
// Now we come to the crux of the new model. Objects do not have functions which can be called
// at will (at least not legitimately), instead they provide a set of CONTRACTS in the plugin_id
//
// at a minimum, a contract must have an intentcap.

// These consist of INTENT satisfied + CAPACITIES.
// For example, the PLAY intent with AUDIO capacity.
// the play intent means that the object can process some kind of sequential data provided
// by another object, and produce some kind of output.
// depending on the caps this can have different forms, for exampe with cap video it can process
// a sequence of images, with the cap realtime it can process the data in realtime, with the
// caps local and display it means it can show the output on the screen. Now we have just defined
// an intencap for audio / video player.
//
// Each object SUBTYPE may have different intencaps,
// for example a PLAYER type may have subtype video and subtype audio, in the case that it cannot
// do both, or if it also encode to disk it may have subtype which lacks realtime and display
// but does produce a media object in the "external" state.
//
// Now, each each intentcap is linked inside a CONTRACT. The contract states the intentcap (or
// and contains all of the possible capacities - however it may not be possible to use all caps at
// once, for example the cap local and cap remote are exclusive.
// This is not a problem as the contract is somthing which is negotiated between the two (or more)
// objects. The caller may select the subset of caps it prefers. The intent may not be alter though.
// having done so, the host now calls the "negotiation" function in the plugin. This is
// (will be) a callback pointed to by a field field in the plugin id., and as an exception may be
// led directly without an agreed contract.
// The aim is for the host and plugin to agree conditions (sufficiently defined attributes
// and so on) so that the host can run the TRANSFORM which the contract refers to and which
// is designed to fulfill the intcaps agreead.
//
// Accordingly, the object (template or instance) will define how the intent is satisfied.
// a transform can require the object be in a certain state first, it can have ATTRIBUTES whose
// values are mandatory or optional as well as providing readonly attributes.
// Any other object can action the transform provided it satisfies the requirements to run it.
// If the object needs to be in a specific state, then it must provide a transform which has
// the effect of altering its state. So the caller must figure out how to navigate a sequence
// of transforms to satify the intentcap it wants.
// the negotiation may take place in sever rounds, each round either party may adust the
// capacities. add or update attributes, and arrange data sources and observers.
// each round the host tesends the updated contract and the plugin responds with a status (see below)
// Once the plugin responds with agreed, the host may the action the transform.
// rather than calling a single function, transforms nay define a sequence of function calls,
// with the outputs from one function sometimes passed to the next,
//
// Having done this, the caller can the action the
// transform. If the transform is quick (get a value, set a value) this is known as a "passive"
// transform, and the caller can simply action the transform and will get a result in an attribute.
// Other transforms are longer running and the caller should assign a threa to run it async.
// assign a background thread to run asyncronously
// This is known as an "active" transform, and it will have a STATUS, which can be for example
// "running", "need data", "finished", "error", "cancelled"
//  If the status is "finished" then the caller can read the result from an object attribute
// (or since this is possible, it may have connected the output attribute to one of its own
// attributes, so it will have the data already). Some transforms (active or passive) also
// have "hooks", ie. callbacks which are generally triggered when the transform status changes.
// any other object can attach to a hook, and will have its function called when those
// hooks are triggerd. Finally, some STATUS changes will also change the STATE of an object,
// for example, "needs data" status can change the object state to "waiting", and  a hook function
// set will be triggered. In the case that a transform needs data updates whilst running,
// it can make a mandatory hook connection be part of the transform requirements. For example
// an effect type object will usually require frame or audio data to be passed to it.
// objects themselves also can have hook points, for example when a object is created, or when
// an attribute is updated.
// If the production of an error status changes the object state to error, then the object must
// provide a transform which changes the state back to normal.
// As well as this, if the caller lacks some attribute values, it may
// wish to show a user interface and allow a user to enter missing values.
// To sumarise:
// - host calls get_plugin_id(), plugin returns information
// - host selects a contract which satisfies the goal driven intentcap.
// - host adjusts the capacities to suit, anf depending on the requirements, may set or
// add some attriibutes. Host then calls the negotiation function in the plugin.
// Plugin checks the contract stae, to see if it is possible to run a transform.
// plugin may also adjust the capacities and may add its own attirbute, some of these will
// be for the host to fill in, some will be readonly, some will have defaults but no values set,
// The host can ask the user for missing values if unable to supply them, or if there are
// choices to be made.

// It may ask other objects for data (linking their attributes to
// to the contract.) If the trnsform requires an active data source (e.g realtime audio),
// then it may search for another object able to satisfy this, and neciate with that object.
// Then it can pass the updated contract back to the plugin. This back and forth may continue
// for several rounds, until the status is agreed, or possibly cancelled, if it seems impossible.
// If the contaract is agreed, then the transform is run. The plugin will add an entry point to
// the contract and host can then call this entry or action a thread to do so.
// in case of passibe transforms this will be quick, for example reading the valu of an attribute
// may not require any negotiation. Such transforms can bemarked as "negociation free"
// Active processes on the other hand may run for a conseiderable time. The transform status
// wil allow other objects to monitor the progress. It may produce data streams in attributes,
// it may spawn other objects, it may may change the state of other objects, or even their subtypes
// Hoever, no transforms can change either the object or its uid.
// Once the transfrm completes, the status may change to finished, erro, cancelled or data ready.

// ..... this is a work in progress.


#endif // ...INC1
#endif
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

//#endif /* _LIVES_PLUGIN_H_VERSION_ */
