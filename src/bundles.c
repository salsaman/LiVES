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
#include "intents.h"
#include "object-constants.h"
#include "bundles.h"


bundledef_t get_bundledef_from_bundle(lives_bundle_t *bundle) {
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


LIVES_GLOBAL_INLINE uint64_t get_bundle64sum(lives_bundle_t *bundle, char **flattened) {
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
      else if (!lives_strncmp(q, "ATTR_", 5)) p += 5;
      if (p != q && *p) p++;
      xiname = lives_concat(xiname, lives_string_tolower(p));
      return xiname;
    }
    return lives_strdup(p);
  }
}


uint64_t get_vflags(const char *q, off_t *offx) {
  uint64_t vflags = 0;
  if (offx)(*offx) = 0;
  if (*q == '#') {
    vflags = ELEM_FLAG_COMMENT;
    return vflags;
  }
  if (*q == '?') {
    vflags |= ELEM_FLAG_OPTIONAL;
    if (offx)(*offx)++;
  }
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



/* static boolean handle_special_value(lives_bundle_t *bundle, bundle_type btype, */
/* 				    uint64_t otype, uint64_t osubtype, */
/* 				    const char *iname, va_list vargs) { */

static boolean init_special_value(lives_bundle_t *bundle, int btype, bundle_strand elem,
                                  bundle_strand elem2) {
  //if (btype == attr_bundle) {
  // setting value, default, or new_default
  //lives_bundle_t *vb = weed_get_plantptr_value(bundle, ATTR_OBJECT_TYPE, NULL);
  //uint32_t atype = (uint32_t)weed_get_int_value(vb, ATTR_VALUE_DATA, NULL);
  // TODO - set attr "value", "default" or "new_default"
  //}
  //if (elem->domain ICAP && item CAPACITIES) prefix iname and det boolean in bndle
  //if (elem->domain CONTRACT && item ATTRIBUTES) check list of lists, if iname there
  // - check if owner, check ir readonly, else add to owner list
  return FALSE;
}


static boolean set_def_value(lives_bundle_t *bundle, int btype,
                             bundle_strand elem, bundle_strand elem2) {
  const char *vname;
  char *sname;
  off_t offx = 0;
  uint32_t vtype;
  uint64_t vflags = get_vflags(elem, &offx);
  boolean err = FALSE;
  if (vflags & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Trying to set default value for comment %s\n", elem);
#endif
    return TRUE;
  }
  vtype = get_vtype(elem, &offx);
  vname = get_vname(elem + offx);
  sname = get_short_name(vname);
  if (lives_strlen_atleast(elem2, 2)) {
    const char *defval = elem2 + 2;
    //boolean is_array = get_is_array(elem2);
    boolean is_array = FALSE; // just set to scalar and 1 value - defaults are mostly just 0 oe NULL
    switch (vtype) {
    case (ELEM_TYPE_SPECIAL):
      err = init_special_value(bundle, btype, elem, elem2);
      break;
    case (ELEM_TYPE_STRING): case (ELEM_TYPE_VOIDPTR):
    case (ELEM_TYPE_BUNDLEPTR):
      if (!lives_strcmp(defval, "NULL"))
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


bundledef_t get_bundledefsaQ_from_bundle(lives_bundle_t *bundle) {
  char *sname2 = get_short_name(ELEM_INTROSPECTION_BLUEPRINT_PTR);
  bundledef_t bundledef = (bundledef_t)weed_get_voidptr_value(bundle, sname2, NULL);
  lives_free(sname2);
  return bundledef;
}


static int bundledef_get_item_idx(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  bundle_strand elem;
  off_t offx;
  uint64_t vflags;
  const char *vname;
  char *sname = exact ? (char *)item : get_short_name(item);
  for (int i = 0; (elem = bundledef[i]); i++) {
    offx = 0;
    vflags = get_vflags(elem, &offx);
    if (vflags & ELEM_FLAG_COMMENT) continue;
    get_vtype(elem, &offx);
    vname = get_vname(elem + offx);
    if (!exact) {
      char *sname2 = exact ? (char *)item : get_short_name(vname);
      if (!lives_strcmp(sname2, sname)) {
        if (sname != item) lives_free(sname);
        if (sname2 != vname) lives_free(sname2);
        return i;
      }
      if (sname2 != vname) lives_free(sname2);
    } else if (!lives_strcmp(vname, item)) {
      if (sname != item) lives_free(sname);
      return i;
    }
  }
  if (sname != item) lives_free(sname);
  return -1;
}


static uint32_t get_bundle_elem_type(lives_bundle_t *bundle, const char *name) {
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
      get_vflags(elem, &offx);
      vtype = get_vtype(elem, &offx);
      lives_free(sname);
      return vtype;
    }
  }
  return ELEM_TYPE_NONE;
}


static boolean set_bundle_val_varg(lives_bundle_t *bundle, const char *iname, uint32_t vtype,
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


boolean set_bundle_val(lives_bundle_t *bundle, const char *name, uint32_t vtype,
                       uint32_t ne, boolean array, ...) {
  va_list vargs;
  boolean bval;
  va_start(vargs, array);
  bval = set_bundle_val_varg(bundle, name, vtype, ne, array, vargs);
  va_end(vargs);
  return bval;
}


boolean set_bundle_value(lives_bundle_t *bundle, const char *name, ...) {
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


static boolean set_bundle_values(lives_bundle_t *bundle, ...) {
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
    static bundle_strand cb[] = {CONTRACT_BUNDLE};
    return (bundledef_t)cb;
  }
  return NULL;
}

static void lives_bundle_free(lives_bundle_t *bundle) {
  if (bundle) {
    char **leaves = weed_plant_list_leaves(bundle, NULL);
    int i = 0, nvals;
    for (char *leaf = leaves[0]; leaf; leaf = leaves[++i]) {
      if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_PLANTPTR) {
        lives_bundle_t **sub = weed_get_plantptr_array_counted(bundle, leaf, &nvals);
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

    vflagsa = get_vflags(elema, &offx);
    if (vflagsa & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
      g_print("%s\n", elema);
#endif
      continue;
    }
    if (!(elema2 = bun[++i])) {
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
      for (j = 0; (elemb = newq[j]); j += 2) {
        offx = 0;
        vflagsb = get_vflags(elemb, &offx);
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


static lives_objstore_t *add_to_bdef_store(uint64_t fixed, lives_obj_t *bstore) {
  // if intent == REPLACE replace existing entry, otherwise do not add if already there
  if (!bdef_store) bdef_store = lives_hash_store_new("flat_bundledef store");
  else if (get_from_hash_store_i(bdef_store, fixed)) return bdef_store;
  return add_to_hash_store_i(bdef_store, fixed, (void *)bstore);
}


LIVES_GLOBAL_INLINE boolean bundle_has_item(lives_bundle_t *bundle, const char *item) {
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


static boolean add_item_to_bundle(bundle_type btype, bundledef_t bundledef,
                                  boolean base_only, lives_bundle_t *bundle,
                                  const char *item, boolean add_value, bundledef_t *bdef, int *nq) {
  // check if ITEM is in bundledef, if so && add_value: add it to bundle, if not already there
  // if exact then match by full name, else by short name
  // if strands and nq, then we will added to bdef and add 2 to nq
  bundle_strand elem, elem2;
  off_t offx;
  uint64_t vflags;
  const char *vname;
  char *sname = NULL, *sname2 = NULL;
  char *etext = NULL;
  boolean err = FALSE;
  int i;

  sname = get_short_name(item);
  if (base_only && sname != item) {
    // if adding to a BASE bundle, we can only add simple elements (for now)
    if (lives_strncmp(item, "ELEM_", 5)) {
      etext = lives_strdup_concat(etext, "\n", "%s is not a base element.", item);
      err = TRUE;
      goto endit;
    }
  }
  for (i = 0; (elem = bundledef[i]); i++) {
    offx = 0;
    vflags = get_vflags(elem, &offx);
    if (vflags & ELEM_FLAG_COMMENT) continue;
    get_vtype(elem, &offx); // dont care about type for now
    vname = get_vname(elem + offx);
    if (!lives_strcmp(vname, item)) break;
    sname2 = get_short_name(vname);
    if (!lives_strcmp(sname2, sname)) break;
    lives_free(sname2);
    sname2 = NULL;
  }
  if (err) goto endit;
  if (!elem) {
    etext = lives_strdup_concat(etext, "\n", "item %s not found in bundledef.", item);
    err = TRUE;
    goto endit;
  }
  if (!(elem2 = bundledef[++i])) {
    etext = lives_strdup_concat(etext, "\n",
                                "strand2 for %s not found in bundledef !", bundledef[--i]);
    err = TRUE;
    goto endit;
  }

  if (!bundle_has_item(bundle, item)) {
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

static lives_bundle_t *create_bundle_from_def_vargs(bundle_type btype, bundledef_t xbundledef,
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
  lives_bundle_t *bundle = NULL, *xbundle = NULL;
  bundledef_t bundledef;
  char *etext = NULL;

  if (!(bundledef = validate_bdef(btype, xbundledef, btype != attr_bundle))) {
    g_printerr("ERROR: INVALID BUNDLEDEF !!\n");
    return NULL;
  }
  //

  bundle = weed_plant_new(LIVES_PLANT_BUNDLE);

  if (bundle) {
    // first we check the vargs. If an item is optional, we create it and set default,
    // and append the strands to new_quakrs.
    //
    // Then we parse the original strands. Any optional ones are skipped.
    // If we find a non-optional item, we insert in new_strands
    // before the optional user added ones.
    // If it was already created as optional, we remove the optional version from new_strands,
    // otherwise we create the element and set it to its default value.
    bundledef_t new_strands = NULL;
    boolean err = FALSE;
    bundle_strand elem;
    const char *vname;
    char *sname;
    off_t offx;
    uint64_t vflags;
    boolean add_qptr = FALSE;
    int nq = 0;

    // step 0, add default optional things
    if (bundledef_has_item(bundledef, ELEM_INTROSPECTION_BLUEPRINT_PTR, TRUE)) {
      // add this first
      err = add_item_to_bundle(btype, bundledef, TRUE, bundle, ELEM_INTROSPECTION_BLUEPRINT_PTR,
                               FALSE, &new_strands, &nq);
      add_qptr = TRUE;
    }
    if (bundledef_has_item(bundledef, ELEM_GENERIC_UID, TRUE)) {
      // add this first
      add_item_to_bundle(btype, bundledef, TRUE, bundle,
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
          err = add_item_to_bundle(btype, bundledef, TRUE, bundle, iname, TRUE, NULL, NULL);
          if (err) break;
        }
      }
      if (!err) {
        // add any missing mandos
        for (int i = 0; (elem = bundledef[i]); i += 2) {
          offx = 0;
          vflags = get_vflags(elem, &offx);
          if (vflags & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
            g_print("%s\n", elem);
#endif
            continue;
          }
          get_vtype(elem, &offx);
          vname = get_vname(elem + offx);
          sname = get_short_name(vname);
          if (bundle_has_item(bundle, sname)) {
            err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
                                     FALSE, &new_strands, &nq);
          } else {
            if (!(vflags & ELEM_FLAG_OPTIONAL)) {
              err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
                                       TRUE, &new_strands, &nq);
            } else {
              err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
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
      if (etext) {
#if DEBUG_BUNDLES
        g_printerr("\nERROR: %s\n", etext);
#endif
        lives_free(etext);
      }
#if DEBUG_BUNDLES
      else g_printerr("\nUnspecified error\n");
      g_print("Unable to create bundle.\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      bundledef_t bundledef = validate_bdef(btype, new_strands, btype != attr_bundle);
      lives_bundle_t *bstore = create_bundle(storage_bundle, "comment", "native_ptr", NULL);
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
  }

  // reverse the leaves
  if (bundle) xbundle = weed_plant_copy(bundle);
  return xbundle;
}


LIVES_SENTINEL lives_bundle_t *create_bundle(bundle_type btype, ...) {
  const_bundledef_t cbundledef;
  lives_bundle_t *bundle;
  va_list xargs;
  //
  if (btype >= 0 || btype < n_bundles) {
    cbundledef = (const_bundledef_t)get_bundledef(btype);
  } else return NULL;
  va_start(xargs, btype);
  bundle = create_bundle_from_def_vargs(btype, (bundledef_t)cbundledef, xargs);
  va_end(xargs);
  return bundle;
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


LIVES_GLOBAL_INLINE char *flatten_bundle(lives_bundle_t *bundle) {
  bundledef_t bdef = get_bundledef_from_bundle(bundle);
  return flatten_bundledef(bdef);
}


lives_bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype) {
  bundledef_t bundledef;
  lives_bundle_t *bundle;

  bundledef = get_obj_bundledef(otype, subtype);
  bundle = create_bundle_from_def_vargs(object_bundle, bundledef, NULL);

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


weed_error_t set_plant_leaf_any_type(weed_plant_t *pl, const char *key, uint32_t st, weed_size_t ne, ...) {
  weed_error_t err;
  va_list args;
  va_start(args, ne);
  err = _set_plant_leaf_any_type_vargs(pl, key, st, ne, args);
  va_end(args);
  return err;
}

