// bundles.c
// LiVES
// (c) G. Finch 2003-2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

/* Bundles are collections of STRANDS. There are various types of strand. The most basic elementary strands, */
/*   which consit of the data items: name, value, and type */

/*   elementary  types include int, double, string, etc. as well as pointer types, voidptr, funcptr and bundleptr */
/*   And may be arrays or single values. */
// bundleptrs are pointers to other bundles or to the bunlde iteslf
// this may be considered a second type of strand,
//
// stran names consist of FAMILY_DOMAIN_SHORTNAME. Family ca be ELEM - elementary, or some other type,
// such as ATTR which pertains to special purpose strands which pertain to the bundle type in question.
///
// From thes building blocks (strands and bundle (definitions), it is possible to construct
// almost any type of data structure,

// the case of LiVES this taking form in the shape of objects (bundles which can manipulate, ie. "transform"
// themselves and othe bundle types

// these "object bundles" also have arrays of other bundles, for example attributes, hook functions

#include "main.h"

#ifdef IS_BUNDLE_MAKER
#undef IS_BUNDLE_MAKER
#endif

#include "object-constants.h"

#define IS_BUNDLE_MAKER
#include "object-constants.h"
#ifdef IS_BUNDLE_MAKER
#undef IS_BUNDLE_MAKER
#endif

#include "bundles.h"

#define N_BUNDLE_DEFS n_builtin_bundledefs

static bundledef_t *new_bdefs;
lives_obj_t **abundle_bdefs;

DEFINE_CORE_BUNDLES

static lives_obj_t *STRUCTURAL_GENITOR;

static lives_obj_instance_t *STRUCTURAL_APP;

static bundledef_t *validate_bdefs(void);

static lives_obj_t **make_as_attr_desc(int level, ...);

static lives_obj_t **make_as_attr_desc(int level, ...) {


  return NULL;
}


lives_obj_instance_t *init_bundles(void) {
  INIT_CORE_BUNDLES;

  // after creating all core bundles we will validate them and recrteate as attr_bundles
  new_bdefs = validate_bdefs();

  // now lets recreate the validated bdefs, but this time as attr_desc_bundles
  // we can use the validated attr_desc_bundle bdef for this. We create an attr_desc_bundle
  // for attr_desc_bundle
  //
  // now we run  third pass. we have a chance here to adjust attr_desc_bundle itself before using to
  // create the final attr_desc_bundles which we will use henceforth
  abundle_bdefs = make_as_attr_desc(0, new_bdefs);

  // now for the final pass, we recreate the just created attr_desc_bundles using the
  // attr_desc_Bundle version of attr_desc_bundle
  // this can be iterated ove more times, but this should be enough to boostrap from
  make_as_attr_desc(1, &abundle_bdefs);

  // next step is to make the structural template and the from that we can create the structural
  // subtypes, here we will just create the APPLICATION subtype and return it
  STRUCTURAL_GENITOR = make_obj_tmpl(NULL, OBJECT_TYPE_STRUCTURAL, NULL);

  // since the broker isnt running yet we cant use intentcaps, so lets just create the subtype
  // 
  STRUCTURAL_APP = make_obj_inst(STRUCTURAL_GENITOR, STRUCTURE_SUBTYPE_APP, NULL);

  // return a singleton object instance of type STRUCTURAL, subtype APP, state NOT_READY
  // after some setup, it should init the thread model, then create the broker
  // and other structuer subtypes as desired. When STUCTURRE_APP changes state to NORMAL
  // the init is finshed, and normal operations can commence by pushing intentcaps to the broker
  // queue
  return STRUCTURAL_APP;
}


const_bundledef_t get_bundledef(bundle_type btype) {
  return maker_get_bundledef(btype);
}


bundledef_t get_bundledef_from_bundle(bundle_t *bundle) {
  char *sname2 = get_short_name(ELEM_INTROSPECTION_BLUEPRINT_PTR);
  bundledef_t bundledef = (bundledef_t)weed_get_voidptr_value(bundle, sname2, NULL);
  lives_free(sname2);
  return bundledef;
}


uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened) {
  char *xfla;
  uint64_t uval;
  if (!flattened || !*flattened) {
    xfla = flatten_bundledef(bdef);
    if (flattened) *flattened = xfla;
  } else xfla = *flattened;
  uval = minimd5(xfla, lives_strlen(xfla));
  if (!flattened) lives_free(xfla);
  return uval;
}


LIVES_GLOBAL_INLINE uint64_t get_bundle64sum(bundle_t *bundle, char **flattened) {
  bundledef_t bdef = get_bundledef_from_bundle(bundle);
  return get_bundledef64sum(bdef, flattened);
}



#define DEBUG_BUNDLES 1
LIVES_GLOBAL_INLINE char *get_short_name(const char *q) {
  if (!q) return NULL;
  else {
    char *xiname = lives_strdup(".");
    char *p = (char *)q;
    if (*p != '.') {
      if (!lives_strncmp(q, "ELEM_", 5)) for (p += 5; *p && *p != '_'; p++);
      else if (!lives_strncmp(q, "BUNDLE_", 6)) for (p += 6; *p && *p != '_'; p++);
      else if (!lives_strncmp(q, "ATTR_", 5)) p += 5;
      if (p != q && *p) p++;
      xiname = lives_concat(xiname, lives_string_tolower(p));
      return xiname;
    }
    return lives_strdup(p);
  }
}


static int skip_directive(bundledef_t bdef, int i) {
  // returns number of lines to skip, starting from DIRECTIVE_BEGIN
  if (bdef && i >= 0) {
    bundle_strand elem = bdef[i];
    if (lives_str_starts_with(elem, DIRECTIVE_BEGIN)) {
      for (int j = 1; bdef[i + j]; j++) if (lives_str_starts_with(elem, DIRECTIVE_END)) return ++j;
    }}
  return 1;
}


static uint64_t get_vflags(const char *q, off_t *offx, int *ii, bundledef_t bdef) {
  uint64_t vflags = 0;
  if (offx)(*offx) = 0;
  if (*q == ELEM_TYPE_FLAG_OPTIONAL) {
    vflags |= ELEM_FLAG_OPTIONAL;
    if (offx)(*offx)++;
  }
  else if (*q == ELEM_TYPE_FLAG_COMMENT) {
    vflags = ELEM_FLAG_COMMENT;
    if (offx) *offx = -1;
    return vflags;
  }
  else if (*q == ELEM_TYPE_FLAG_DIRECTIVE) {
    if (bdef && ii) {
      int skippy = skip_directive(bdef, *ii);
      if (ii) *ii += skippy - 1;
      if (offx) *offx = -skippy;
    }
    vflags |= ELEM_FLAG_DIRECTIVE;
    return vflags;
  }
  if (ii) (*ii)++;
  return vflags;
}

uint32_t get_vtype(const char *q, off_t *offx) {
  off_t op = 0;
  if (offx) {
    op = *offx;
    (*offx)++;
  }
  return q[op];
}

LIVES_GLOBAL_INLINE const char *get_vname(const char *q) {return q;}

LIVES_GLOBAL_INLINE boolean get_is_array(const char *q) {return (q && *q == '1');}

uint32_t weed_seed_to_elem_type(uint32_t st) {
  switch (st) {
  case WEED_SEED_INT: return ELEM_TYPE_INT;
  case WEED_SEED_UINT: return ELEM_TYPE_UINT;
  case WEED_SEED_BOOLEAN: return ELEM_TYPE_BOOLEAN;
  case WEED_SEED_INT64: return ELEM_TYPE_INT64;
  case WEED_SEED_UINT64: return ELEM_TYPE_UINT64;
  case WEED_SEED_DOUBLE: return ELEM_TYPE_DOUBLE;
  case WEED_SEED_STRING: return ELEM_TYPE_STRING;
  case WEED_SEED_VOIDPTR: return ELEM_TYPE_VOIDPTR;
  case WEED_SEED_PLANTPTR: return ELEM_TYPE_BUNDLEPTR;
  default: break;
  }
  return ELEM_TYPE_NONE;
}



#if 0
/* static boolean handle_special_value(bundle_t *bundle, bundle_type btype, */
/* 				    uint64_t otype, uint64_t osubtype, */
/* 				    const char *iname, va_list vargs) { */

static boolean init_special_value(bundle_t *bundle, int btype, bundle_strand elem,
                                  bundle_strand elem2) {
  //if (btype == attr_bundle) {
  // setting value, default, or new_default
  //bundle_t *vb = weed_get_plantptr_value(bundle, ATTR_OBJECT_TYPE, NULL);
  //uint32_t atype = (uint32_t)weed_get_int_value(vb, ATTR_VALUE_DATA, NULL);
  // TODO - set attr "value", "default" or "new_default"
  //}
  //if (elem->domain ICAP && item CAPACITIES) prefix iname and det boolean in bndle
  //if (elem->domain CONTRACT && item ATTRIBUTES) check list of lists, if iname there
  // - check if owner, check ir readonly, else add to owner list
  return FALSE;
}
#endif

static boolean set_def_value(bundle_t *bundle, int btype,
                             bundle_strand elem, bundle_strand elem2) {
  const char *vname;
  char *sname;
  off_t offx = 0;
  uint32_t vtype;
  boolean err = FALSE;
  get_vflags(elem, &offx, NULL, NULL);
  if (offx < 0) {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Trying to set default value for comment or directive %s\n", elem);
#endif
    return TRUE;
  }
  vtype = get_vtype(elem, &offx);
  if (vtype == ELEM_TYPE_NONE) return FALSE;
  vname = get_vname(elem + offx);
  sname = get_short_name(vname);
  if (lives_strlen_atleast(elem2, 2)) {
    const char *defval = elem2 + 2;
    //boolean is_array = get_is_array(elem2);
    boolean is_array = FALSE; // just set to scalar and 1 value - defaults are mostly just 0 oe NULL
    switch (vtype) {
    case (ELEM_TYPE_STRING): case (ELEM_TYPE_VOIDPTR):
    case (ELEM_TYPE_BUNDLEPTR):
      if (!lives_strcmp(defval, "NULL") || !lives_strcmp(defval, "((void *)0)"))
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, NULL);
      else if (vtype == ELEM_TYPE_STRING)
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, defval);
      break;
    case (ELEM_TYPE_INT):
    case (ELEM_TYPE_UINT):
    case (ELEM_TYPE_BOOLEAN):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, atoi(defval));
      break;
    case (ELEM_TYPE_INT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtol(defval));
      break;
    case (ELEM_TYPE_UINT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtoul(defval));
      break;
    case (ELEM_TYPE_DOUBLE):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtod(defval));
      break;
    default: break;
    }
#if DEBUG_BUNDLES
    if (!err) g_printerr("Setting default for %s [%s] to %s\n", sname, vname, defval);
#endif
    lives_free(sname);
  } else {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Missing default for %s [%s]\n", sname, vname);
#endif
    err = TRUE;
  }
  return err;
}


LIVES_GLOBAL_INLINE size_t bundle_array_append(bundle_t *bundle, const char *name, int ne,
					       int *tot_elems, void *thing) {
  // TODO - elemnt is bundleptr, check that the conditions match
  size_t tot = 0;
  char *sname = get_short_name(name);
  weed_error_t err = weed_leaf_append_elements(bundle, sname, ne, thing);
  if (err == WEED_SUCCESS) tot = weed_leaf_num_elements(bundle, sname);
  lives_free(sname);
  if (tot_elems) *tot_elems = tot;
  return tot;
}


static int bundledef_get_item_idx(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  // return the idx of the elem[] (strand) where "item" starts
  bundle_strand elem;
  off_t offx;
  const char *vname;
  char *sname = exact ? (char *)item : get_short_name(item);
  for (int i = 0; (elem = bundledef[i]); i++) {
    g_print("check i %d\n", i);
    offx = 0;
    get_vflags(elem, &offx, &i, bundledef);
    if (offx < 0) continue;
    get_vtype(elem, &offx);
    vname = get_vname(elem + offx);
    if (!exact) {
      char *sname2 = get_short_name(vname);
      g_print("CF %s and %s\n", sname2, sname);
      if (!lives_strcmp(sname2, sname) || (*sname != '.' && !lives_strcmp(sname2 + 1, sname))) {
        if (sname != item) lives_free(sname);
        if (sname2 != vname) lives_free(sname2);
        return --i;
      }
      if (sname2 != vname) lives_free(sname2);
    } else if (!lives_strcmp(vname, item)) {
      if (sname != item) lives_free(sname);
      return --i;
    }
  }
  if (sname != item) lives_free(sname);
  return -1;
}


static uint32_t get_bundle_elem_type(bundle_t *bundle, const char *name) {
  // get elem type for existing or optional element in bundle
  bundledef_t bundledef;
  char *sname = get_short_name(name);
  uint32_t st;
  st = weed_leaf_seed_type(bundle, sname);
  if (st) {
    lives_free(sname);
    return weed_seed_to_elem_type(st);
  }
  // may be an optional param
  bundledef = get_bundledef_from_bundle(bundle);
  if (bundledef) {
    int eidx = bundledef_get_item_idx(bundledef, sname, FALSE);
    if (eidx >= 0) {
      bundle_strand elem = bundledef[eidx];
      off_t offx = 0;
      uint32_t vtype;
      get_vflags(elem, &offx, NULL, NULL);
      if (offx < 0) return ELEM_TYPE_NONE;
      vtype = get_vtype(elem, &offx);
      lives_free(sname);
      return vtype;
    }
  }
  return ELEM_TYPE_NONE;
}


static boolean set_bundle_val_varg(bundle_t *bundle, const char *iname, uint32_t vtype,
                                   uint32_t ne, boolean array, va_list vargs) {
  // THIS IS FOR ELEM_* values, we handle ATTR_* elsewhere
  // set a value / array in bundle, element with name "name"
  // ne holds number of elements, array tells us whether value is array or scalar
  // value holds the data to be set
  // vtype is the 'extended' type, which can be one of:
  // INT, UINT, DOUBLE, FLOAT, BOOLEAN, STRING, CHAR, INT64, UINT64
  // VOIDPTR, BUNDLEPTR or SPECIAL
  // - SPECIAL types are handled elsewhere
  char *etext = NULL, *xiname = lives_strdup(".");
  boolean err = FALSE;
  xiname = get_short_name(iname);
  if (!vargs) {
    etext = lives_strdup_concat(etext, "\n", "missing value setting item %s [%s] in bundle.",
                                xiname, iname);
    err = TRUE;
  } else {
    if (!array) {
      switch (vtype) {
      case (ELEM_TYPE_INT): {
        int val = va_arg(vargs, int);
        weed_set_int_value(bundle, xiname, val);
      }
      break;
      case (ELEM_TYPE_UINT): {
        uint val = va_arg(vargs, uint);
        weed_set_uint_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_BOOLEAN: {
        int val = va_arg(vargs, int);
        weed_set_boolean_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_DOUBLE: {
        double val = va_arg(vargs, double);
        weed_set_double_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_STRING: {
        char *val = lives_strdup(va_arg(vargs, char *));
        weed_set_string_value(bundle, xiname, val);
        lives_free(val);
      }
      break;
      case ELEM_TYPE_INT64: {
        int64_t val = va_arg(vargs, int64_t);
        weed_set_int64_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_UINT64: {
        uint64_t val = va_arg(vargs, uint64_t);
        weed_set_uint64_value(bundle, xiname, (uint64_t)val);
      }
      break;
      case ELEM_TYPE_BUNDLEPTR: {
        weed_plant_t *val = va_arg(vargs, weed_plant_t *);
        weed_set_plantptr_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_VOIDPTR: {
        void *val = va_arg(vargs, void *);
        weed_set_voidptr_value(bundle, xiname, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type invalid for %s in bundle.", xiname);
        err = TRUE;
        break;
      }
    } else {
      switch (vtype) {
      case ELEM_TYPE_INT: {
        int *val = va_arg(vargs, int *);
        weed_set_int_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_UINT: {
        uint32_t *val = va_arg(vargs, uint32_t *);
        weed_set_uint_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_BOOLEAN: {
        int *val = va_arg(vargs, int *);
        weed_set_boolean_array(bundle, xiname, ne, val);
      }
      break;
      //
      case ELEM_TYPE_DOUBLE: {
        double *val = va_arg(vargs, double *);
        weed_set_double_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_STRING: {
        char **val = va_arg(vargs, char **);
        char **xval = lives_calloc(ne, sizeof(char *));
        for (int j = 0; j < ne; j++) {
          if (val[j]) xval[j] = lives_strdup(val[j]);
          else xval[j] = NULL;
        }
        weed_set_string_array(bundle, xiname, ne, val);
        for (int j = 0; j < ne; j++) if (xval[j]) lives_free(xval[j]);
        lives_free(xval);
      }
      break;
      case ELEM_TYPE_INT64: {
        int64_t *val = va_arg(vargs, int64_t *);
        weed_set_int64_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_UINT64: {
        uint64_t *val = va_arg(vargs, uint64_t *);
        weed_set_uint64_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_BUNDLEPTR: {
        weed_plant_t **val = va_arg(vargs, weed_plant_t **);
        weed_set_plantptr_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_VOIDPTR: {
        void **val = va_arg(vargs, void **);
        weed_set_voidptr_array(bundle, xiname, ne, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type invalid for %s in bundle.", xiname);
        err = TRUE;
        break;
      }
    }
  }
  if (err) {
    if (etext) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: %s\n", etext);
#endif
      lives_free(etext);
    }
#if DEBUG_BUNDLES
    else g_printerr("\nUnspecified error\n");
#endif
  }
  lives_free(xiname);
  return err;
}


boolean set_bundle_val(bundle_t *bundle, const char *name, uint32_t vtype,
                       uint32_t ne, boolean array, ...) {
  va_list vargs;
  boolean bval;
  va_start(vargs, array);
  bval = set_bundle_val_varg(bundle, name, vtype, ne, array, vargs);
  va_end(vargs);
  return bval;
}


boolean set_bundle_value(bundle_t *bundle, const char *name, ...) {
  va_list varg;
  char *sname = get_short_name(name);
  uint32_t vtype = get_bundle_elem_type(bundle, name);
  boolean bval = FALSE;
  if (!vtype) {
#if DEBUG_BUNDLES
    g_printerr("Item %s [%s] not found in bundle\n", sname, name);
#endif
  } else {
    va_start(varg, name);
    bval = set_bundle_val_varg(bundle, name, vtype, 1, FALSE, varg);
    va_end(varg);
  }
  lives_free(sname);
  return bval;
}


static boolean set_bundle_values(bundle_t *bundle, ...) {
  va_list vargs;
  char *name, *sname;
  boolean bval;
  va_start(vargs, bundle);
  while (1) {
    va_list xargs;
    uint32_t vtype;
    name = va_arg(vargs, char *);
    if (!name) break;
    sname = get_short_name(name);
    va_copy(xargs, vargs);
    vtype = get_bundle_elem_type(bundle, name);
    bval = set_bundle_val_varg(bundle, name, vtype, 1, FALSE, xargs);
    va_end(vargs);
    va_copy(vargs, xargs);
    va_end(xargs);
    lives_free(sname);
    if (bval) break;
  }
  va_end(vargs);
  return bval;
}


static bundledef_t get_obj_bundledef(uint64_t otype, uint64_t osubtype) {
  if (otype == OBJECT_TYPE_CONTRACT) {
    //return CONTRACT_BUNDLEDEF;
  }
  return NULL;
}


static void lives_bundle_free(bundle_t *bundle) {
  if (bundle) {
    char **leaves = weed_plant_list_leaves(bundle, NULL);
    int i = 0, nvals;
    for (char *leaf = leaves[0]; leaf; leaf = leaves[++i]) {
      if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_PLANTPTR) {
        bundle_t **sub = weed_get_plantptr_array_counted(bundle, leaf, &nvals);
        for (int k = 0; k < nvals; k++) if (sub[k]) lives_bundle_free(sub[k]);
        weed_leaf_delete(bundle, leaf);
      } else if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_VOIDPTR) {
        if (weed_leaf_element_size(bundle, leaf, 0) > 0)
          lives_free(weed_get_voidptr_value(bundle, leaf, NULL));
      }
      lives_free(leaf);
    }
    if (nvals) lives_free(leaves);
    weed_plant_free(bundle);
  }
}


static bundledef_t validate_bdef(int btype, bundledef_t bun, boolean base_only) {
  // check bundledefs to make sure they contain only valid elements
  // any duplicate item names are removed. If we have an optional version and mandatory then
  // we retain the mandatory one
  // we are going do this TWICE. The first time we will validate all
  // then the second time we recreate the bundledefs as attr_bundles
  // since attribute itself has elements this only goes on
  //
#if DEBUG_BUNDLES
  g_printerr("\nParsing bundle definition for bundle type %d\n", btype);
#endif
  bundledef_t newq = NULL;
  bundle_strand elema;
  boolean err = FALSE;
  int nq = 0;
  for (int i = 0; (elema = bun[i]); i++) {
    bundle_strand elema2, elemb, elemb2;
    off_t offx = 0;
    uint64_t vflagsa, vflagsb;
    const char *vnamea, *vnameb;
    char *snamea, *snameb;
    uint32_t vtypea, vtypeb;
    int j;

    vflagsa = get_vflags(elema, &offx, &i, bun);
    if (offx < 0) continue;

    if (!(elema2 = bun[i])) {
#if DEBUG_BUNDLES
      g_print("%s\n", elema);
      g_print("Second strand not found !\n");
#endif
      err = TRUE;
      break;
    }
    vtypea = get_vtype(elema, &offx);
    vnamea = get_vname(elema + offx);
    snamea = get_short_name(vnamea);
    if (!newq) elemb = NULL;
    else {
      for (j = 0; (elemb = newq[j]); j++) {
        offx = 0;
        vflagsb = get_vflags(elemb, &offx, &j, bun);
	if (offx < 0) continue;
        vtypeb = get_vtype(elemb, &offx);
        vnameb = get_vname(elemb + offx);
        snameb = get_short_name(vnameb);
        if (!lives_strcmp(snamea, snameb)) break;
      }
    }
    if (!elemb) {
      nq += 2;
      g_print("ADD2 %s\n", elema);
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 2] = lives_strdup(elema);
      newq[nq - 1] = lives_strdup(elema2);
      newq[nq] = NULL;
#if DEBUG_BUNDLES
      if (!(vflagsa & ELEM_FLAG_OPTIONAL)) {
        g_print("mandatory");
      } else g_print("optional");
      if (!atoi(elema2)) g_print(" scalar");
      else g_print(" array");
      g_print(" %s [%s] found (data is %s)\n", snamea, vnamea, elema2);
#endif
      lives_free(snamea);
      continue;
    } else {
      elemb2 = newq[++j];
      //
      if (lives_strcmp(vnamea, vnameb)) {
        // dupl from2 domains
#if DEBUG_BUNDLES
        g_print("Duplicate items found: %s and %s cannot co-exist in same bundledef !\n",
                vnamea, vnameb);
#endif
        err = TRUE;
      } else {
#if DEBUG_BUNDLES
        g_print("Duplicate item found for %s [%s]\n", snamea, vnamea);
#endif
        if (vtypea != vtypeb) {
#if DEBUG_BUNDLES
          g_print("ERROR: types do not match, we had %d and now we have %d\n",
                  vtypeb, vtypea);
#endif
          err = TRUE;
        } else {
          if (lives_strcmp(elema2, elemb2)) {
            g_print("Mismatch in strand2: we had %s and now we have %s\n",
                    elemb2, elema2);
            err = TRUE;
          } else {
            g_print("This is normal due to the extendable nature of bundles\n");
            if ((vflagsb & ELEM_FLAG_OPTIONAL)
                && !(vflagsa & ELEM_FLAG_OPTIONAL)) {
              g_print("Replacing optional value with mandatory\n");
              lives_free(newq[j]); lives_free(newq[j + 1]);
              newq[j] = lives_strdup(elema);
              newq[j + 1] = lives_strdup(elema2);
            } else {
              g_print("Ignoring duplicate\n");
	      // *INDENT-OFF*
	    }}}}}
    lives_free(snameb);
  }
  // *INDENT-ON*
  if (err) {
    g_print("ERRORS found in bundledef - INVALID - please correct its definition\n");
    if (newq) {
      for (int i = 0; i < nq; i++) lives_free(newq[i]);
      lives_free(newq);
      newq = NULL;
    }
  }
  return newq;
}


static bundledef_t *validate_bdefs(void) {
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    new_bdefs[i] = validate_bdef(i, GET_BDEF(i), TRUE);
  }
  return new_bdefs;
}


static lives_objstore_t *add_to_bdef_store(uint64_t fixed, lives_obj_t *bstore) {
  // if intent == REPLACE replace existing entry, otherwise do not add if already there
  if (!bdef_store) bdef_store = lives_hash_store_new("flat_bundledef store");
  else if (get_from_hash_store_i(bdef_store, fixed)) return bdef_store;
  return add_to_hash_store_i(bdef_store, fixed, (void *)bstore);
}


LIVES_GLOBAL_INLINE boolean bundle_has_item(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  boolean bval;
  bval = weed_plant_has_leaf(bundle, sname);
  lives_free(sname);
  return bval == WEED_TRUE;
}


static boolean bundledef_has_item(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  if (bundledef_get_item_idx(bundledef, item, exact) >= 0) return TRUE;
  return FALSE;
}


void add_to_bundle_array(bundle_t *bundle, const char *elem, bundle_t *abun) {
  if (!bundle || !elem || !abun) return ;
  weed_plant_t **pl_array;  
}


static boolean is_std_item(const char *item) {
  const char *ename;
  for (int i = 0; (ename = all_elems[i++]); g_print("NOW: %s", ename));
  return TRUE;
}


static boolean add_item_to_bundle(bundle_type btype, bundledef_t bundledef,
                                  boolean base_only, bundle_t *bundle,
                                  const char *item, boolean add_value, bundledef_t *bdef, int *nq) {
  // check if ITEM is in bundledef, if so && add_value: add it to bundle, if not already there
  // if exact then match by full name, else by short name
  // if strands and nq, then we will added to bdef and add 2 to nq
  bundle_strand elem, elem2;
  off_t offx;
  const char *vname;
  char *sname = NULL, *sname2 = NULL, *etext = NULL;
  boolean err = FALSE;
  int i, idx = bundledef_get_item_idx(bundledef, item, FALSE);

  is_std_item(item);
  
  if (idx < 0) {
      etext = lives_strdup_concat(etext, "\n", "%s is not a base element.", item);
      err = TRUE;
      goto endit;
  }

  if (!(elem2 = bundledef[++idx])) {
    etext = lives_strdup_concat(etext, "\n",
                                "strand2 for %s not found in bundledef !", bundledef[--idx]);
    err = TRUE;
    goto endit;
  }

  if (!bundle_has_item(bundle, item)) {
    elem = bundledef[idx];
    if (add_value) {
      err = set_def_value(bundle, btype, elem, elem2);
      if (err) goto endit;
    }
    if (bdef) {
      *nq += 2;
      *bdef = lives_realloc(*bdef, (*nq + 1) * sizeof(char *));
      (*bdef)[*nq - 2] = lives_strdup(elem);
      (*bdef)[*nq - 1] = lives_strdup(elem2);
      (*bdef)[*nq] = NULL;
      g_print("ADD %s\n", elem);
    }
  }
endit:
  if (err) {
    if (etext) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: %s\n", etext);
#endif
      lives_free(etext);
    }
  }
  if (sname) lives_free(sname);
  if (sname2) lives_free(sname2);
  return err;
}


// THIS IS THE MAIN FUNCTION FOR CREATING BUNDLES
static bundle_t *create_bundle_anon_vargs(bundledef_t bundledef, va_list vargs) {
  bundle_t *bundle = weed_plant_new(LIVES_PLANT_BUNDLE);
  if (bundle) {
    // TODO - put in validate_bundledef.
    // build the bundle, at the same time we will REBUILD the orginal bundledef as NEW_STRANDS
    // stripping out comments and anything which is not 'valid'
    // ---> do this in init_bundles
    //
    // TODO - parse bundledef and create def bundle
    // parse vargs and set defs, add optional elemnts if not created
    //
    // first we check the vargs. If an item is optional, we create it and set its default,
    // Any user added items NOT in the bundledef are errors
    //
    // Then we parse the original strands, skipping over any optional items.
    // If we find a non-optional item, we insert in new_strands, unless the user added it already.
    // We also check for and remove duplicate names. If a name exists as both optional and mandatory
    // we keep the mandatory version, otherwise we ignore the duplicate.
    // Having duplicate item names with different types is an error. If directed to replace default we
    // do this once only, any more times and it is an errof.
    //
    // elements in the original bundle which are optional are added to new_strands, but we do not
    // create an item in the bundle. The user can add this later, as well as removing optional ones
    // so we need to store new_strands so we know which are optional
    bundledef_t new_strands = NULL;
    boolean err = FALSE;
    bundle_strand elem;
    const char *vname;
    char *sname;
    off_t offx;
    uint64_t vflags;
    uint32_t vtype;
    boolean add_qptr = FALSE;
    int nq = 0;

    // step 0, add default optional things
    if (bundledef_has_item(bundledef, ELEM_INTROSPECTION_BLUEPRINT_PTR, TRUE)) {
      // if BLUEPRINT_PTR is optional we will create and store new_strands in it at the end
      err = add_item_to_bundle(0, bundledef, TRUE, bundle, ELEM_INTROSPECTION_BLUEPRINT_PTR,
                               FALSE, &new_strands, &nq);
      add_qptr = TRUE;
    }
    if (bundledef_has_item(bundledef, ELEM_GENERIC_UID, TRUE)) {
      // ** Special rule: if bundledef has generic, uid, set it to a random uint64_t
      // in each bundle created. So we will do that now.
      add_item_to_bundle(0, bundledef, TRUE, bundle,
                         ELEM_GENERIC_UID, FALSE, &new_strands, &nq);
      sname = get_short_name(ELEM_GENERIC_UID);
      weed_set_uint64_value(bundle, sname, gen_unique_id());
      lives_free(sname);
      sname = NULL;
    }

    if (!err) {
      // step 1, go through params
      if (vargs) {
        while (1) {
          // caller should provide vargs, list of optional items to be initialized
          char *iname;
          iname = va_arg(vargs, char *);
          if (!iname) break; // done parsing params
	  g_print("WILL add %s\n",iname);
          err = add_item_to_bundle(0, bundledef, TRUE, bundle, iname, TRUE, NULL, NULL);
          if (err) break;
        }
      }
      if (!err) {
        // add any missing mandos
        for (int i = 0; (elem = bundledef[i]); i++) {
          offx = 0;
	  vflags = get_vflags(elem, &offx, &i, bundledef);
	  if (offx < 0) continue;
          vtype = get_vtype(elem, &offx);
	  if (vtype == ELEM_TYPE_NONE) continue;
          vname = get_vname(elem + offx);
          sname = get_short_name(vname);
          if (bundle_has_item(bundle, sname)) {
            err = add_item_to_bundle(0, bundledef, TRUE, bundle, vname,
                                     FALSE, &new_strands, &nq);
          } else {
            if (!(vflags & ELEM_FLAG_OPTIONAL)) {
              err = add_item_to_bundle(0, bundledef, TRUE, bundle, vname,
                                       TRUE, &new_strands, &nq);
            } else {
              err = add_item_to_bundle(0, bundledef, TRUE, bundle, vname,
                                       FALSE, &new_strands, &nq);
            }
          }
          if (sname) {
            lives_free(sname);
            sname = NULL;
          }
          if (err) break;
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*
    if (err) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: adding item to bundle\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      // hmmm..
      bundledef_t bundledef = validate_bdef(0, new_strands, 0 != ATTRIBUTE_BUNDLE_TYPE);
      bundle_t *bstore = create_bundle_by_type(STORAGE_BUNDLE_TYPE, "comment", "native_ptr", NULL);
      if (bstore) {
	char *flat;
	uint64_t fixed_id = get_bundledef64sum(bundledef, &flat);
	set_bundle_value(bstore, "native_ptr", ELEM_TYPE_STRING, 1, FALSE, flat);
	set_bundle_value(bstore, "comment", ELEM_TYPE_STRING, 1, FALSE, "anon bundledef");
	bdef_store = add_to_bdef_store(fixed_id, bstore);
      }
      if (add_qptr) {
	set_bundle_val(bundle, ELEM_INTROSPECTION_BLUEPRINT_PTR,
		       ELEM_TYPE_VOIDPTR, 1, FALSE, (void *)bundledef);
      }
    }
    // reverse the leaves
    return weed_plant_copy(bundle);
  }
  return NULL;
}



static bundle_t *create_bundle_from_def_vargs(bundle_type btype, bundledef_t xbundledef,
    va_list vargs) {
  // what we do in this function is parse vargs, a list of minimum item names to be included
  // in the bundle. If an item is not found in the bundldef, show an error and return NULL.
  // if a name is given multiple times, we ingore the duplicates after the first.
  // After doing this, we then add defaults for any non-optional items not specified in the
  // parameter list.
  // if an item appears more than once in the  bundldef, then we should eliminate the duplicates.
  // However if it appears as both "optional" and "non-optional" then the copy marked as optional
  // should be the one to be removed.
  // Fianlly we will end up with a bundle with all non-optional items set to defaults,
  // and any optional items in the parameter list included and set to defaults.
  // For convenience, if the bundle includes optional "INTROSPECTION_BLUEPRINT", we place a copy
  // of the pared dwon strand array in it. This will be used later when adding or removing items
  // from the bundle. New optional items may be added, and optional items may also be removed
  // (deleted).
  bundledef_t bundledef;
  if (!(bundledef = validate_bdef(btype, xbundledef, btype != ATTRIBUTE_BUNDLE_TYPE))) {
    g_printerr("ERROR: INVALID BUNDLEDEF !!\n");
    return NULL;
  }
  return create_bundle_anon_vargs(bundledef, vargs);
}


LIVES_SENTINEL bundle_t *create_bundle_by_type(bundle_type btype, ...) {
  const_bundledef_t cbundledef;
  bundle_t *bundle;
  va_list xargs;
  //
  if (btype >= 0 || btype < n_builtin_bundledefs) {
    cbundledef = (const_bundledef_t)get_bundledef(btype);
  } else return NULL;
  va_start(xargs, btype);
  bundle = create_bundle_from_def_vargs(btype, (bundledef_t)cbundledef, xargs);
  va_end(xargs);
  return bundle;
}


LIVES_SENTINEL bundle_t *var_bundle_from_bundledef(bundledef_t bdef, ...) {
  bundle_t *bundle;
  va_list xargs;
  //
  va_start(xargs, bdef);
  bundle = create_bundle_anon_vargs(bdef, xargs);
  va_end(xargs);
  return bundle;
}


LIVES_GLOBAL_INLINE bundle_t *def_bundle_from_bundledef(bundledef_t bdef) {
  return create_bundle_anon_vargs(bdef, NULL);
}


char *flatten_bundledef(bundledef_t bdef) {
  char *buf;
  off_t offs = 0;
  size_t ts = 0;
  for (int i = 0; bdef[i]; i++) ts += strlen(bdef[i]) + 1;
  buf = (char *)lives_calloc(1, ts);
  for (int i = 0; bdef[i]; i++) {
    size_t s = strlen(bdef[i]);
    buf[offs++] = (uint8_t)(s & 255);
    lives_memcpy(buf + offs, bdef[i], s);
    offs += s;
  }
  return buf;
}


LIVES_GLOBAL_INLINE char *flatten_bundle(bundle_t *bundle) {
  bundledef_t bdef = get_bundledef_from_bundle(bundle);
  return flatten_bundledef(bdef);
}


bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype) {
  bundledef_t bundledef;
  bundle_t *bundle;

  bundledef = get_obj_bundledef(otype, subtype);

  // we ought to create the template first then use it to create instance,
  //but for now we will just create the instance directly
  
  bundle = create_bundle_from_def_vargs(OBJECT_INSTANCE_BUNDLE_TYPE, bundledef, NULL);

  // after this, we need to create contracts for the object
  // - (1) call get_contracts on a contract template with "create instance" intent
  ///
  //  (2) create a first instance with intent "negotiate", caps none, and flagged "no negotiate"
  // and mandatory attr: object type contract, state prepare.
  //
  // -- call more times to create contracts for other object transforms

  set_bundle_values(bundle, "type", otype, "subtype", subtype, NULL);
  return bundle;
}


char *bundle_to_header(bundle_t *bundle, const char *tname) {
  bundledef_t bdef;
  char *hdr, *ar = NULL, *line;
  uint32_t st;
  weed_size_t ne;
  const char *tp;

  if (tname)
    hdr  = lives_strdup("typedef struct {");
  else
    hdr = lives_strdup("struct {");

  bdef = get_bundledef_from_bundle(bundle);
  if (bdef) {
    bundle_strand elem, elem2;
    for (int i = 0; (elem = bdef[i]); i++) {
      off_t offx = 0;
      const char *vname;
      char *sname;
      boolean addaster = FALSE;

      get_vflags(elem, &offx, &i, bdef);
      if (offx < 0) continue;

      if (!(elem2 = bdef[i])) break;

      get_vtype(elem, &offx);
      vname = get_vname(elem + offx);
      sname = get_short_name(vname);

      g_print("ELEM is %s\n", elem);

      if (bundle_has_item(bundle, sname)) {
        st = weed_leaf_seed_type(bundle, sname);
        ne = weed_leaf_num_elements(bundle, sname);
        tp = weed_seed_to_ctype(st, TRUE);

        if (st == WEED_SEED_PLANTPTR) tp = "bundle_t *";

        if (ne > 1) ar = lives_strdup_printf("[%d]", ne);
        else if (st < 64 && get_is_array(elem2)) addaster = TRUE;
        line = lives_strdup_printf("\n  %s%s%s%s;", tp, addaster ? "*" : "", sname + 1, ar ? ar : "");
        hdr = lives_concat(hdr, line);
        if (ar) {
          lives_free(ar);
          ar = NULL;
        }
        lives_free(sname);
      }
    }
  } else {
    char **leaves = weed_plant_list_leaves(bundle, NULL);
    for (int i = 0; leaves[i]; i++) {
      if (*leaves[i] == '.') {
        st = weed_leaf_seed_type(bundle, leaves[i]);
        ne = weed_leaf_num_elements(bundle, leaves[i]);
        tp = weed_seed_to_ctype(st, TRUE);
        if (st == WEED_SEED_PLANTPTR) tp = "bundle_t *";
        if (ne > 1) ar = lives_strdup_printf("[%d]", ne);
        line = lives_strdup_printf("\n  %s%s%s;", tp, leaves[i] + 1, ar ? ar : "");
        hdr = lives_concat(hdr, line);
        if (ar) {
          lives_free(ar);
          ar = NULL;
        }
      }
      lives_free(leaves[i]);
    }
    lives_free(leaves);
  }
  if (!tname)
    line = lives_strdup("\n}");
  else
    line = lives_strdup_printf("\n} %s;", tname);
  lives_concat(hdr, line);
  return hdr;
}


LIVES_GLOBAL_INLINE bundle_t *get_bundleptr_value(bundle_t *bundle, const char *item) {
  return weed_get_plantptr_value(bundle, item, NULL);
}
