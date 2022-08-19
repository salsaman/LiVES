// bundles.c
// LiVES
// (c) G. Finch 2003-2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"

#include "object-constants.h"

#include "bundles.h"

typedef char *(*nscope_hdr_f)(int i);
typedef boolean(*nscope_cond_f)(int i, int idx, void *data);
typedef char *(*nscope_text_f)(int i, int idx, void *data, char **sep, char **fmt);
typedef char *(*nscope_ftr_f)(int i);

static char *nirvascope(int i, nscope_hdr_f head_func, nscope_cond_f cond_func, void *cdata,
		       nscope_text_f text_func, void *tdata, nscope_ftr_f foot_func);
static char *nirvascope_print_str_array(char **arr);

#define N_BUNDLE_DEFS n_builtin_bundledefs
static bundle_t *create_gen2_bundle_with_vargs(uint64_t btype, blueprint_t *, va_list vargs);

static bundle_t *create_bundle_by_type(bundle_type bt, ...);

static uint32_t weed_seed_to_strand_type(uint32_t st);
static void lives_bundle_free(bundle_t *);

/////// gen 1 functions ////
static boolean set_bundle_val_varg(bundle_t *, const char *iname, uint32_t vtype,
                                   uint32_t ne, int array_op, va_list vargs);
static boolean set_bundle_val(bundle_t *, const char *name, uint32_t vtype,
                              uint32_t ne, int array_op, ...);

static boolean lives_strand_is_comment(strand_def_t *);

/////////////////////////////////

static bundledef_t new_bdefs[N_BUNDLE_DEFS];
lives_obj_t **abundle_bdefs;

DEFINE_CORE_BUNDLES

static void validate_bdefs(void);

static const bundle_t *blueprints[N_BUNDLE_DEFS];
static void make_blueprint_for(bundle_type btype);

#define CALL_WRAPPER(func,...) WRAP_FN_##func(__VA_ARGS__);

#define MAKE_WRAP_FN(func, ret, nparams, ...) \
  uint64_t WRAP_FN_##func(bundleptr in, bundleptr out, ...) {	\
  va_list va;								\
  va_start(va, out);							\
  FN_BEND(func, in, out, nparams, __VA_ARGS__, va);			\
  va_end(va);								\
  return GET_RETVAL(out);}						\


const bundle_t *init_bundles(void) {
  // will init the system and create all standard bundle definitions
  const char *plugname = "/usr/lib/nirva/subsystems/prime";

  INIT_CORE_BUNDLES;

  // for FULL automation this will be done automatically
  // after creating all core bundles we should validate them
  validate_bdefs();

  // the next step is to recteate all the bundle defs as "blueprints"
  // step 1 is to create strand_def bundles. We will use these to cteate the blueprint
  // for "blueprint"

  for (int i = 0; i < N_BUNDLE_DEFS; i++) blueprints[i] = NULL;

  // since we dont yet have a blueprint for strand_def, we will use the bundledefs to create
  // the blueprint bundle and the strand_def array for it.
  make_blueprint_for(STRAND_DEF_BUNDLE_TYPE);
  make_blueprint_for(DEF_BUNDLE_TYPE);
  make_blueprint_for(STRAND_DEF_BUNDLE_TYPE);
  make_blueprint_for(BLUEPRINT_BUNDLE_TYPE);
  make_blueprint_for(DEF_BUNDLE_TYPE);
  make_blueprint_for(STRAND_DEF_BUNDLE_TYPE);
  make_blueprint_for(BLUEPRINT_BUNDLE_TYPE);

  // now we have blueprints for strand_def and scriptlet,
  // created with strand defs created from blueprints,
  // we will now creatre all blueprints, including remaking the ones we just made

  // create blueprints for all standard bundles
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    make_blueprint_for(i);
  }

  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    make_blueprint_for(i);
    g_print("\n%s\nn", nirvascope_bundle_to_header(blueprints[i], "", 0));
  }

  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    bundle_t *b = create_bundle_by_type(i, NULL);
    g_print("\n\ncreated bundle type %d\n", i);
    g_print("%s\n", nirvascope_bundle_to_header(b, NULL, 0));
    g_print("bsize == %lu\n", nirvascope_get_bundle_weight(b));
    lives_bundle_free(b);
  }

  // now we have prepared the way,
  // what we need do now is locate structure_prime,
  // call NIRVA_LOAD(URI)
  //
  // this will dlopen it, find the nirva_init function
  // create in and out bundles, then get the pmap for nirva_init
  // NIRVA_ACTION it
  // check the output bundle for contracts
  // find a contract with data out of an object template, type structural
  // action the transform
  // grab the object from the bundle
  // and return it as output

  //  STRUCTURE_PRIME = NIRVA_LOAD(plugname);
  return STRUCTURE_PRIME;
}


LIVES_GLOBAL_INLINE char *get_short_name(const char *q) {
  if (!q) return NULL;
  else {
    char *xiname = lives_strdup(".");
    char *p = (char *)q;
    char *z;
    if (*p != '.') {
      //g_print("NOW %s\n", p);
      if ((p = (char *)lives_str_starts_with_skip(q, "STRAND_"))) {
        //g_print("parse %s\n", p);
        for (; *p && *p != '_'; p++);
      } else if ((p = (char *)lives_str_starts_with_skip(q, "CONST_"))) {
        //g_print("parse2 %s\n", p);
        for (; *p && *p != '_'; p++);
      } else if ((p = (char *)lives_str_starts_with_skip(q, "BUNDLE_"))) for (; *p && *p != '_'; p++);
      else if ((p = (char *)lives_str_starts_with_skip(q, "ATTR_"))) for (; *p && *p != '_'; p++);
      if (!p) p = (char *)q;
      if (p != q && *p) p++;
      if (*p) {
        //g_print("NOW3 %s\n", p);
        xiname = lives_concat(xiname, lives_string_tolower(p));
        for (z = xiname; *z; z++) {
          if (*z == ' ') {
            *z = 0;
            break;
          }
        }
      }
      //g_print("NOW4 %s\n", xiname);
      return xiname;
    }
    return lives_strdup(p);
  }
}

////////////////////////////////////////////////////////////////////////
/// gen 1 functions

// these functions are for stage 1 bootstrap. Once bundledefs are remade as BLUEPRINTS
// then we move to gen 2

const bundledef_t get_bundledef(bundle_type btype) {
  return (const bundledef_t)maker_get_bundledef(btype);
}


bundledef_t get_bundledef_from_bundle(bundle_t *bundle) {
  char *sname2 = get_short_name(STRAND_INTROSPECTION_BLUEPRINT_PTR);
  bundledef_t bundledef = (bundledef_t)weed_get_voidptr_value(bundle, sname2, NULL);
  lives_free(sname2);
  return bundledef;
}


static int skip_directive(bundledef_t bdef, int i) {
  // returns number of lines to skip, starting from DIRECTIVE_BEGIN
  if (bdef && i >= 0) {
    bundle_strand strand = bdef[i];
    if (lives_str_starts_with(strand, DIRECTIVE_BEGIN)) {
      for (int j = 1; (strand = bdef[i + j]); j++) {
        if (lives_str_starts_with(strand, DIRECTIVE_END)) return ++j;
      }
    }
  }
  return 1;
}


static uint64_t get_vflags(const char *q, off_t *offx, int *ii, bundledef_t bdef) {
  uint64_t vflags = 0;
  if (offx)(*offx) = 0;
  if (*q == STRAND_TYPE_FLAG_OPTIONAL) {
    vflags |= STRAND_TYPE_FLAG_OPTIONAL;
    if (offx)(*offx)++;
  } else if (*q == STRAND_TYPE_FLAG_COMMENT) {
    vflags = STRAND_TYPE_FLAG_COMMENT;
    if (offx) *offx = -1;
    return vflags;
  } else if (*q == STRAND_TYPE_FLAG_DIRECTIVE) {
    if (bdef && ii) {
      int skippy = skip_directive(bdef, *ii);
      //g_print("SKIPPY is %d\n", skippy);
      if (ii) *ii += skippy - 1;
      if (offx) *offx = -skippy;
    }
    vflags |= STRAND_TYPE_FLAG_DIRECTIVE;
    return vflags;
  }
  if (ii)(*ii)++;
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


static int bundledef_get_item_idx(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  // return the idx of the strand[] (strand) where "item" starts
  // -1 if not found
  bundle_strand strand;
  off_t offx;
  const char *vname;
  char *sname = exact ? (char *)item : get_short_name(item);
  for (int i = 0; (strand = bundledef[i]); i++) {
    //g_print("check i %d\n", i);
    offx = 0;
    get_vflags(strand, &offx, &i, bundledef);
    if (offx < 0) continue;
    get_vtype(strand, &offx);
    vname = get_vname(strand + offx);
    if (!exact) {
      char *sname2 = get_short_name(vname);
      //g_print("CF %s and %s\n", sname2, sname);
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


static uint32_t get_bundle_strand_type(bundle_t *bundle, const char *name) {
  // get strand type for existing or optional element in bundle
  bundledef_t bundledef;
  char *sname = get_short_name(name);
  uint32_t st;
  st = weed_leaf_seed_type(bundle, sname);
  if (st) {
    lives_free(sname);
    return weed_seed_to_strand_type(st);
  }
  // may be an optional param
  bundledef = get_bundledef_from_bundle(bundle);
  if (bundledef) {
    int eidx = bundledef_get_item_idx(bundledef, sname, FALSE);
    if (eidx >= 0) {
      bundle_strand strand = bundledef[eidx];
      off_t offx = 0;
      uint32_t vtype;
      get_vflags(strand, &offx, NULL, NULL);
      if (offx < 0) return STRAND_TYPE_NONE;
      vtype = get_vtype(strand, &offx);
      lives_free(sname);
      return vtype;
    }
  }
  return STRAND_TYPE_NONE;
}


static boolean set_def_value_gen1(bundle_t *bundle, const char *overrd_name, \
                             bundle_strand strand, bundle_strand strand2) {
  const char *vname;
  char *sname;
  off_t offx = 0;
  uint32_t vtype;
  boolean err = FALSE;

  //g_print("SET DEF for: %s|%s|\n", strand, strand2);

  get_vflags(strand, &offx, NULL, NULL);
  if (offx < 0) {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Trying to set default value for comment or directive %s\n", strand);
#endif
    return TRUE;
  }
  vtype = get_vtype(strand, &offx);
  if (vtype == STRAND_TYPE_NONE) return FALSE;

  vname = get_vname(strand + offx);
  if (overrd_name) sname = lives_strdup(overrd_name);
  else sname = get_short_name(vname);
  if (lives_strlen_atleast(strand2, 2)) {
    const char *defval = strand2 + 2;
    //boolean is_array = get_is_array(strand2);
    boolean is_array = FALSE; // just set to scalar and 1 value - defaults are mostly just 0 oe NULL
    switch (vtype) {
    case (STRAND_TYPE_STRING): case (STRAND_TYPE_VOIDPTR):
    case (STRAND_TYPE_BUNDLEPTR):
    case (STRAND_TYPE_CONST_BUNDLEPTR):
      if (!lives_strcmp(defval, "NULL") || !lives_strcmp(defval, "((void *)0)"))
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, NULL);
      else if (vtype == STRAND_TYPE_STRING)
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, defval);
      break;
    case (STRAND_TYPE_INT):
    case (STRAND_TYPE_UINT):
    case (STRAND_TYPE_BOOLEAN):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, atoi(defval));
      break;
    case (STRAND_TYPE_INT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtol(defval));
      break;
    case (STRAND_TYPE_UINT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtoul(defval));
      break;
    case (STRAND_TYPE_DOUBLE):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtod(defval));
      break;
    default: break;
    }
#if DEBUG_BUNDLES
    if (!err) g_printerr("Setting default for %s [%s] to %s\n", sname, vname, defval);
#endif
    if (!overrd_name) lives_free(sname);
  } else {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Missing default for %s [%s]\n", sname, vname);
#endif
    err = TRUE;
  }
  return err;
}


static bundledef_t get_obj_bundledef(uint64_t otype, uint64_t osubtype) {
  if (otype == OBJECT_TYPE_CONTRACT) {
    //return CONTRACT_BUNDLEDEF;
  }
  return NULL;
}


static bundledef_t validate_bdef(bundledef_t bdef) {
  // check bundledefs to make sure they contain only valid elements
  // any duplicate item names are removed. If we have an optional version and mandatory then
  // we retain the mandatory one
  // we are going do this TWICE. The first time we will validate all
  // then the second time we recreate the bundledefs as attr_def containers
  bundledef_t newq = NULL;
  bundle_strand stranda;
  boolean err = FALSE;
  size_t dblen = lives_strlen(DIRECTIVE_BEGIN);
  size_t delen = lives_strlen(DIRECTIVE_END);
  int nq = 0, dirx = 0;
  for (int i = 0; (stranda = bdef[i]); i++) {
    bundle_strand stranda2, strandb, strandb2;
    off_t offx = 0;
    uint64_t vflagsa, vflagsb;
    const char *vnamea, *vnameb;
    char *snamea, *snameb;
    uint32_t vtypea, vtypeb;
    int j;

    vflagsa = get_vflags(stranda, &offx, NULL, bdef);
    if (vflagsa == STRAND_TYPE_FLAG_COMMENT) {
      nq++;
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 1] = lives_strdup(stranda);
      newq[nq] = NULL;
      //g_print("GOT comment: %s\n", stranda);
      continue;
    }
    if (vflagsa == STRAND_TYPE_FLAG_DIRECTIVE) {
      if (dirx) {
        if (!lives_strncmp(stranda, DIRECTIVE_END, delen)) {
          //g_print("END dirx %s\n", stranda + delen);
          dirx--;
          nq++;
          newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
          newq[nq - 1] = lives_strdup(stranda);
          newq[nq] = NULL;
          continue;
        }
      }
      if (!strncmp(stranda, DIRECTIVE_BEGIN, dblen)) {
        //g_print("START dirx %s\n", stranda + dblen);
        dirx++;
      }
    }
    if (dirx) {
      //g_print("SKIP directive data: %s\n", stranda);
      nq++;
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 1] = lives_strdup(stranda);
      newq[nq] = NULL;
      continue;
    }
    i++;
    if (!(stranda2 = bdef[i])) {
#if DEBUG_BUNDLES
      g_print("%s\n", stranda);
      g_print("Second strand not found !\n");
#endif
      err = TRUE;
      break;
    }

    //g_print("PARSE %s\n", stranda);
    vtypea = get_vtype(stranda, &offx);
    vnamea = get_vname(stranda + offx);
    //g_print("PARSE2 %s\n", vnamea);
    snamea = get_short_name(vnamea);
    //g_print("PARSE3 %s\n", snamea);

    if (!newq) strandb = NULL;
    else {
      //check for duplicates
      for (j = 0; (strandb = newq[j]); j++) {
        offx = 0;
        vflagsb = get_vflags(strandb, &offx, &j, bdef);
        if (offx < 0) continue;
        //g_print("PARSExx %s\n", strandb);
        vtypeb = get_vtype(strandb, &offx);
        vnameb = get_vname(strandb + offx);
        //g_print("PARSE2x %s\n", vnameb);
        snameb = get_short_name(vnameb);
        //g_print("PARSE3x %s\n", snameb);
        if (!lives_strcmp(snamea, snameb)) break;
      }
    }
    if (!strandb) {
      nq += 2;
      //g_print("ADD2 %s\n", stranda);
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 2] = lives_strdup(stranda);
      newq[nq - 1] = lives_strdup(stranda2);
      newq[nq] = NULL;
#if DEBUG_BUNDLES
      if (!(vflagsa & STRAND_TYPE_FLAG_OPTIONAL)) {
        g_print("mandatory");
      } else g_print("optional");
      if (!atoi(stranda2)) g_print(" scalar");
      else g_print(" array");
      g_print(" %s [%s] found (data is %s)\n", snamea, vnamea, stranda2);
#endif
      lives_free(snamea);
      continue;
    } else {
      strandb2 = newq[j];
      //
      if (lives_strcmp(vnamea, vnameb)) {
        // dupl from2 domains
	//#if DEBUG_BUNDLES
        g_print("Duplicate items found: %s and %s cannot co-exist in same bundledef !\n",
                vnamea, vnameb);
	//#endif
        err = TRUE;
      } else {
	//#if DEBUG_BUNDLES
        g_print("Duplicate item found for %s [%s]\n", snamea, vnamea);
	//#endif
        if (vtypea != vtypeb) {
	  //#if DEBUG_BUNDLES
          g_print("ERROR: types do not match, we had %d and now we have %d\n",
                  vtypeb, vtypea);
	  //#endif
          err = TRUE;
        } else {
          if (lives_strcmp(stranda2, strandb2)) {
            g_print("Mismatch in strand2: we had %s and now we have %s\n",
                    stranda2, strandb2);
            err = TRUE;
          } else {
            g_print("This is normal due to the extendable nature of bundles\n");
            if ((vflagsb & STRAND_TYPE_FLAG_OPTIONAL)
                && !(vflagsa & STRAND_TYPE_FLAG_OPTIONAL)) {
              g_print("Replacing optional value with mandatory\n");
              lives_free(newq[j]); lives_free(newq[j + 1]);
              newq[j] = lives_strdup(stranda);
              newq[j + 1] = lives_strdup(stranda2);
            } else {
              g_print("Ignoring duplicate\n");
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*
    lives_free(snameb);
  }
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


static void validate_bdefs(void) {
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    g_print("\nCLEAN UP up bef %d\n", i);
    new_bdefs[i] = validate_bdef((bundledef_t)GET_BDEF(i));
    if (new_bdefs[i]) g_print("%s", new_bdefs[i][0]);
    g_print("\nCLEAN UP up bef %d\n", i);
  }
  g_print("\n\nCLEANED UP up befs\n\n\n");
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    char *name;
    bundledef_t bdef = new_bdefs[i];
    bundle_t *bundle;
    //g_print("\n\n\nbundledef number %d is:\n", i);
    if (bdef) {
      // check we can create and free all types
      //name = lives_strdup_printf("NIRVA_%s", bdef[0] + 1);
      bundle = create_bundle_by_type(i, NULL);
      //g_print("%s", nirvascope_bundle_to_header(bundle, name, 0));
      //lives_free(name);
      lives_bundle_free(bundle);
    }
  }
}


static bundle_t *create_gen1_bundle_with_vargs(uint64_t bt, bundledef_t bundledef, va_list vargs) {
  bundle_t *bundle = weed_plant_new(LIVES_PLANT_BUNDLE);
  if (bundle) {
    boolean err = FALSE;
    bundle_strand strand;
    const char *vname;
    char *sname;
    off_t offx;
    uint64_t vflags;
    uint32_t vtype;

    if (!err) {
      // step 1, go through params
      if (vargs) {
        char *err_item = NULL;
        while (1) {
          // caller should provide vargs, list of optional items to be initialized
          char *iname;
          int idx;
          offx = 0;
          iname = va_arg(vargs, char *);
          if (!iname) break; // done parsing params
          //g_print("WILL add %s\n",iname);

          idx = bundledef_get_item_idx(bundledef, iname, FALSE);

          if (idx < 0) {
            err_item = iname;
            err = TRUE;
            break;
          }
          strand = bundledef[idx];
          vflags = get_vflags(strand, &offx, NULL, bundledef);
          if (offx < 0) {
            err_item = iname;
            err = TRUE;
            break;
          }
          vtype = get_vtype(strand, &offx);
	  //// we need to handle a few items manually here since we have not yet reached
	  // full automation
	  if (bt == STRAND_DEF_BUNDLE_TYPE) {
	    if (!lives_strcmp(iname, "default")) {
	      vtype = lives_strand_get_value_uint32(bundle, "strand_type");
	    }
	  }
	  lives_set_strand_value_gen1(bundle, iname, vtype, vargs);
        }
        if (err) {
          g_printerr("ERROR adding item '%s' to bundle, invalid item name\n", err_item);
	  break_me("invname 1");
          err = FALSE;
        }
      }
      if (!err) {
        // add any missing mandos
        for (int i = 0; (strand = bundledef[i]); i++) {
          offx = 0;
          vflags = get_vflags(strand, &offx, &i, bundledef);
          if (offx < 0) continue;
          vtype = get_vtype(strand, &offx);
          //if (vtype == STRAND_TYPE_NONE) continue;
          vname = get_vname(strand + offx);
          //g_print("DO WE need defs for %s ?\n", vname);
          sname = get_short_name(vname);
          if (!(vflags & STRAND_TYPE_FLAG_OPTIONAL)) {
            //g_print("%s is mando\n", sname);
            if (!bundle_has_strand(bundle, sname)) {
              //g_print("%s is absent\n", sname);
	      //g_print("SETTING to default\n");
	      err = set_def_value_gen1(bundle, NULL, strand, bundledef[i]);
	      if (err) goto endit;
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
  endit:
    if (err) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: adding item to bundle\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      // reverse the leaves
      return weed_plant_copy(bundle);
    }
  }
  return NULL;
}

//////////////////////////////////


void bundle_strand_copy(bundle_t *bundle, const char *shname, bundle_t *stdef,
			const char *defname) {
  weed_leaf_copy(bundle, shname, stdef, defname);
}


uint64_t stdef_get_flags(strand_def_t *stdef) {
  return lives_strand_get_value_uint64(stdef, "flags");
}

uint32_t stdef_get_type(strand_def_t *stdef) {
  return lives_strand_get_value_uint32(stdef, "strand_type");
}

char *stdef_get_comment(strand_def_t *stdef) {
  if (lives_strand_is_comment(stdef))
    return lives_strand_get_value_string(stdef, "default");
  return NULL;
}

boolean lives_strand_is_directive(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_DIRECTIVE);
}

static boolean lives_strand_is_comment(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_COMMENT);
}

boolean lives_strand_is_value(strand_def_t *stdef) {
  return !(stdef_get_flags(stdef) & (BLUEPRINT_FLAG_COMMENT | BLUEPRINT_FLAG_DIRECTIVE));
}

boolean lives_strand_is_optional(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_OPTIONAL);
}

boolean lives_strand_is_readonly(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_READONLY);
}

boolean lives_strand_is_array(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_ARRAY);
}

boolean lives_strand_is_pointer_to(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & (BLUEPRINT_FLAG_PTR_TO_SCALAR
					| BLUEPRINT_FLAG_PTR_TO_ARRAY));
}

boolean lives_strands_exclude(strand_def_t *stdef, const char *oname) {
  return FALSE;
}

boolean lives_strand_excluded_by(strand_def_t *stdef) {
  return FALSE;
}

boolean lives_strand_is_pointer_to_array(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & (BLUEPRINT_FLAG_PTR_TO_ARRAY));
}

boolean is_value_strand(strand_def_t *stdef) {
  return (!(lives_strand_is_comment(stdef)) && !lives_strand_is_directive(stdef));
}

char *stdef_get_name(strand_def_t *stdef) {
  return lives_strand_get_value_string(stdef, "name");
}

strand_t *lives_blueprint_get_strand(blueprint_t *bp, const char *name) {
  return NULL;
}


strand_def_t *get_strand_def(blueprint_t *bp, const char *name) {
  int ns;
  strand_def_t **sds = weed_get_plantptr_array_counted(bp, ".strand_def", &ns);
  strand_def_t *stdef = NULL;
  char *sname1 = get_short_name(name), *sname2, *xname;
  for (weed_size_t i = 0; i < ns; i++) {
    if (!sds[i]) continue;
    if (!is_value_strand(sds[i])) continue;
    xname = stdef_get_name(sds[i]);
    sname2 = get_short_name(xname);
    if (!lives_strcmp(sname2, sname1)) {
      stdef = sds[i];
      break;
    }
    lives_free(xname); lives_free(sname2);
  }
  if (sds) lives_free(sds);
  lives_free(sname1);
  return stdef;
}


const blueprint_t *get_blueprint_for_btype(bundle_type btype) {
  if (btype >= 0 && btype < n_builtin_bundledefs) return blueprints[btype];
  return NULL;
}


blueprint_t *get_blueprint_from_bundle(bundle_t *b) {
  return (blueprint_t *)
    lives_strand_get_value_const_bundleptr(b, STRAND_INTROSPECTION_BLUEPRINT_PTR);
}


strand_def_t *get_mand_strand_def(blueprint_t *bp, int idx) {
  int ns, i;
  strand_def_t **sds = weed_get_plantptr_array_counted(bp, ".strand_def", &ns), *stdef = NULL;
  for (i = 0; i < ns; i++) {
    stdef = sds[i];
    if (!lives_strand_is_value(stdef) || lives_strand_is_optional(stdef)) continue;
    if (!(idx--)) break;
  }
  if (sds) lives_free(sds);
  if (i < ns) return stdef;
  return NULL;
}


void load_hook_automations(blueprint_t *bp, bundle_t *b) {


}


static boolean blueprint_has_strand(blueprint_t *bp, const char *sname, boolean exact) {
  return TRUE;
}

// create a bundle, using a blueprint instead of a bdef
// there are two ways this can be done - with a static blueprint - we add a pointer in blueprint_ptr
// or with a custom blueprinte, we store this in bluprint strand and add ptrs in strand_def to strands
static bundle_t *create_gen2_bundle_with_vargs(uint64_t btype, blueprint_t *blueprint, va_list vargs) {
  bundle_t *bundle = weed_plant_new(LIVES_PLANT_BUNDLE);
  if (bundle) {
    boolean err = FALSE;
    strand_def_t *stdef;
    const char *stname;
    char *shname;
    char *defname = get_short_name(STRAND_VALUE_DEFAULT);
    uint64_t stflags;
    uint32_t vtype;

    stdef = get_strand_def(blueprint, STRAND_GENERIC_UID);
    if (stdef) {
      vtype = stdef_get_type(stdef);
      set_bundle_val(bundle, STRAND_GENERIC_UID, vtype, 1, 0, gen_unique_id());
    }

    stdef = get_strand_def(blueprint, STRAND_BLUEPRINT_BUNDLE_TYPE);
    if (stdef) {
      vtype = stdef_get_type(stdef);
      set_bundle_val(bundle, STRAND_BLUEPRINT_BUNDLE_TYPE, vtype, 1, 0, btype);
    }

    stdef = get_strand_def(blueprint, STRAND_INTROSPECTION_BLUEPRINT_PTR);
    if (stdef) {
      vtype = stdef_get_type(stdef);
      set_bundle_val(bundle, STRAND_INTROSPECTION_BLUEPRINT_PTR, vtype, 1, 0, blueprint);
    }

    if (!err) {
      // step 1, go through params
      if (vargs) {
        const char *err_item = NULL;
        while (1) {
          // caller should provide vargs, list of optional items to be initialized
          stname = va_arg(vargs, char *);
          if (!stname) break; // done parsing params
          //g_print("2WILL add %s\n",stname);
	  //g_print("ptr stname is %p\n", stname);
	  stdef = get_strand_def(blueprint, stname);
	  if (!lives_strand_is_value(stdef)) {
	    if (!stdef) {
	      // this is possible if adding a comment or directive
	      err_item = stname;
	      err = TRUE;
	      break;
	    }
	  }
	  vtype = stdef_get_type(stdef);
	  lives_set_strand_value_gen1(bundle, stname, vtype, vargs);
	}
        if (err) {
          g_printerr("2ERROR adding item '%s' to bundle, invalid item name\n", err_item);
          err = FALSE;
        }
      }
      if (!err) {
        // add any missing mandos
	// TODO -check for EXCLUSIVE
        for (int i = 0; (stdef = get_mand_strand_def(blueprint, i)); i++) {
          uint32_t sttype = stdef_get_type(stdef);
          stflags = stdef_get_flags(stdef);
          //if (sttype == STRAND_TYPE_NONE) continue;
          stname = stdef_get_name(stdef);
          //g_print("DO WE need defs for %s ?\n", stname);
          shname = get_short_name(stname);
	  if (!bundle_has_strand(bundle, shname)) {
	    //load_automation_for(blueprint, bundle, stname);
	    if (sttype != STRAND_TYPE_NONE) {
	      //g_print("%s is mando\n", shname);
              //g_print("%s is absent\n", shname);
	      if (bundle_has_strand(stdef, defname))
		bundle_strand_copy(bundle, shname, stdef, defname);
            }
          }
          if (shname) {
            lives_free(shname);
            shname = NULL;
          }
          if (err) break;
	  // *INDENT-OFF*
	}
	//if (!err) load_hook_automations(blueprint, bundle);
      }}
    // *INDENT-ON*
    if (err) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: adding item to bundle\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      // reverse the leaves
      return weed_plant_copy(bundle);
    }
  }
  return NULL;
}


LIVES_SENTINEL bundle_t *create_bundle_by_type(bundle_type bt, ...) {
bundle_t *bun = NULL;
if (bt >= 0 && bt < n_builtin_bundledefs) {
    va_list vargs;
    const blueprint_t *bp = blueprints[bt];
    va_start(vargs, bt);
    if (bp) bun = create_gen2_bundle_with_vargs((uint64_t)bt, (blueprint_t *)bp, vargs);
    else {
      const_bundledef_t cbundledef = (const_bundledef_t)get_bundledef(bt);
      if (cbundledef) bun = create_gen1_bundle_with_vargs((uint64_t)bt, (bundledef_t)cbundledef, vargs);
    }
    va_end(vargs);
  }
  return bun;
}


////////////////////


uint32_t weed_seed_to_attr_type(uint32_t st) {
  switch (st) {
  case WEED_SEED_INT: return ATTR_TYPE_INT;
  case WEED_SEED_DOUBLE: return ATTR_TYPE_DOUBLE;
  case WEED_SEED_BOOLEAN: return ATTR_TYPE_BOOLEAN;
  case WEED_SEED_STRING: return ATTR_TYPE_STRING;
  case WEED_SEED_INT64: return ATTR_TYPE_INT64;
  case WEED_SEED_UINT: return ATTR_TYPE_UINT;
  case WEED_SEED_UINT64: return ATTR_TYPE_UINT64;
  case WEED_SEED_FLOAT: return ATTR_TYPE_FLOAT;
  case WEED_SEED_VOIDPTR: return ATTR_TYPE_VOIDPTR;
  case WEED_SEED_FUNCPTR: return ATTR_TYPE_FUNCPTR;
  case WEED_SEED_PLANTPTR: return ATTR_TYPE_BUNDLEPTR;
  default: return ATTR_TYPE_NONE;
  }
}


uint32_t weed_seed_to_strand_type(uint32_t st) {
  switch (st) {
  case WEED_SEED_INT: return STRAND_TYPE_INT;
  case WEED_SEED_UINT: return STRAND_TYPE_UINT;
  case WEED_SEED_BOOLEAN: return STRAND_TYPE_BOOLEAN;
  case WEED_SEED_INT64: return STRAND_TYPE_INT64;
  case WEED_SEED_UINT64: return STRAND_TYPE_UINT64;
  case WEED_SEED_DOUBLE: return STRAND_TYPE_DOUBLE;
  case WEED_SEED_STRING: return STRAND_TYPE_STRING;
  case WEED_SEED_VOIDPTR: return STRAND_TYPE_VOIDPTR;
  case WEED_SEED_FUNCPTR: return STRAND_TYPE_FUNCPTR;
  case WEED_SEED_PLANTPTR: return STRAND_TYPE_BUNDLEPTR;
  default: break;
  }
  return STRAND_TYPE_NONE;
}


uint32_t strand_type_to_weed_seed(uint32_t st) {
  switch (st) {
  case STRAND_TYPE_INT: return WEED_SEED_INT;
  case STRAND_TYPE_UINT: return WEED_SEED_UINT;
  case STRAND_TYPE_BOOLEAN: return WEED_SEED_BOOLEAN;
  case STRAND_TYPE_INT64: return WEED_SEED_INT64;
  case STRAND_TYPE_UINT64: return WEED_SEED_UINT64;
  case STRAND_TYPE_DOUBLE: return WEED_SEED_DOUBLE;
  case STRAND_TYPE_STRING: return WEED_SEED_STRING;
  case STRAND_TYPE_VOIDPTR: return WEED_SEED_VOIDPTR;
  case STRAND_TYPE_FUNCPTR: return WEED_SEED_FUNCPTR;
  case STRAND_TYPE_BUNDLEPTR: return WEED_SEED_PLANTPTR;
  case STRAND_TYPE_CONST_BUNDLEPTR: return WEED_SEED_VOIDPTR;
  default: break;
  }
  return WEED_SEED_INVALID;
}


boolean set_bundle_value_gen1(bundle_t *bundle, const char *name, ...) {
  uint32_t vtype = get_bundle_strand_type(bundle, name);
  va_list varg;
  char *sname = get_short_name(name);
  boolean bval = FALSE;
  if (!vtype) {
#if DEBUG_BUNDLES
    g_printerr("Item %s [%s] not found in bundle\n", sname, name);
#endif
  } else {
    va_start(varg, name);
    lives_set_strand_value_gen1(bundle, name, vtype, varg);
    va_end(varg);
  }
  lives_free(sname);
  return bval;
}


boolean set_bundle_value(bundle_t *bundle, const char *name, ...) {
  blueprint_t *bp = get_blueprint_from_bundle(bundle);
  va_list varg;
  char *sname = get_short_name(name);
  boolean bval = FALSE;
  va_start(varg, name);
  lives_set_strand_value_gen2(bundle, name, varg);
  va_end(varg);
  lives_free(sname);
  return bval;
}


static boolean set_bundle_values(bundle_t *bundle, ...) {
  va_list vargs;
  char *name, *sname;
  boolean bval;
  va_start(vargs, bundle);
  while (1) {
    uint32_t vtype;
    name = va_arg(vargs, char *);
    if (!name) break;
    sname = get_short_name(name);
    vtype = get_bundle_strand_type(bundle, name);
    bval = set_bundle_val_varg(bundle, name, vtype, 1, 0, vargs);
    lives_free(sname);
    if (bval) break;
  }
  va_end(vargs);
  return bval;
}


#if 0
/* static boolean handle_special_value(bundle_t *bundle, bundle_type btype, */
/* 				    uint64_t otype, uint64_t osubtype, */
/* 				    const char *iname, va_list vargs) { */

static boolean init_special_value(bundle_t *bundle, int btype, bundle_strand strand,
                                  bundle_strand strand2) {
  //if (btype == attr_bundle) {
  // setting value, default, or new_default
  //bundle_t *vb = weed_get_plantptr_value(bundle, ATTR_OBJECT_TYPE, NULL);
  //uint32_t atype = (uint32_t)weed_get_int_value(vb, ATTR_VALUE_DATA, NULL);
  // TODO - set attr "value", "default" or "new_default"
  //}
  //if (strand->domain ICAP && item CAPACITIES) prefix iname and det boolean in bndle
  //if (strand->domain CONTRACT && item ATTRIBUTES) check list of lists, if iname there
  // - check if owner, check ir readonly, else add to owner list
  return FALSE;
}
#endif

static boolean set_bundle_val_varg(bundle_t *bundle, const char *iname, uint32_t vtype,
                                   uint32_t ne, int array_op, va_list vargs) {
  // THIS IS FOR STRAND_* values, for ATTR_bundles this is called for the "DATA" element
  // set a value / array in bundle, element with name "name"
  // ne holds (new) number of elements, array tells us whether value is array or scalar
  // value holds the data to be set
  // vtype is the 'extended' type, which can be one of:
  // INT, UINT, DOUBLE, FLOAT, BOOLEAN, STRING, CHAR, INT64, UINT64
  // VOIDPTR, BUNDLEPTR or FUNCPTR

  //g_print("VTYPE is %d\n", vtype);
  
  char *etext = NULL, *xiname = lives_strdup(".");
  boolean err = FALSE;
  weed_error_t werr;
  xiname = get_short_name(iname);
  if (!vargs) {
    etext = lives_strdup_concat(etext, "\n", "missing value setting item %s [%s] in bundle.",
                                xiname, iname);
    err = TRUE;
  } else {
    uint32_t stype = strand_type_to_weed_seed(vtype);
    if (!array_op || (array_op == 2 && ne <= 1)) {
      switch (vtype) {
      case (STRAND_TYPE_INT): {
        int val = va_arg(vargs, int);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_int_value(bundle, xiname, val);
      }
      break;
      case (STRAND_TYPE_UINT): {
        uint val = va_arg(vargs, uint);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, stype, 1, &val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_INT, 1, &val);
	}
        else {
          werr = weed_set_uint_value(bundle, xiname, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_int_value(bundle, xiname, val);
	}
      }
      break;
      case STRAND_TYPE_BOOLEAN: {
        int val = va_arg(vargs, int);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_boolean_value(bundle, xiname, val);
      }
      break;
      case STRAND_TYPE_DOUBLE: {
        double val = va_arg(vargs, double);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_double_value(bundle, xiname, val);
      }
      break;
      case STRAND_TYPE_STRING: {
        char *val = lives_strdup(va_arg(vargs, char *));
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_string_value(bundle, xiname, val);
      }
      break;
      case STRAND_TYPE_INT64: {
        int64_t val = va_arg(vargs, int64_t);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_int64_value(bundle, xiname, val);
      }
      break;
      case (STRAND_TYPE_UINT64): {
        uint64_t val = va_arg(vargs, uint64_t);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, stype, 1, &val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_INT64, 1, &val);
	}
        else {
          werr = weed_set_uint64_value(bundle, xiname, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_int64_value(bundle, xiname, val);
	}
      }
      break;
      case STRAND_TYPE_CONST_BUNDLEPTR: {
        void *val = va_arg(vargs, void *);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, WEED_SEED_VOIDPTR, 1, &val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_PLANTPTR, 1, &val);
	}
        else {
          werr = weed_set_voidptr_value(bundle, xiname, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_plantptr_value(bundle, xiname, val);
	}
      }
	break;
      case STRAND_TYPE_BUNDLEPTR: {
        weed_plant_t *val = va_arg(vargs, weed_plant_t *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_plantptr_value(bundle, xiname, val);
      }
      break;
      case STRAND_TYPE_VOIDPTR: {
        void *val = va_arg(vargs, void *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_voidptr_value(bundle, xiname, val);
      }
      break;
      case STRAND_TYPE_FUNCPTR: {
        weed_funcptr_t val = va_arg(vargs, weed_funcptr_t);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, 1, &val);
        else
          weed_set_funcptr_value(bundle, xiname, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type %d invalid for %s in bundle.", vtype, xiname);
        err = TRUE;
        break;
      }
    } else {
      switch (vtype) {
      case STRAND_TYPE_INT: {
        int *val = va_arg(vargs, int *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_int_array(bundle, xiname, ne, val);
      }
      break;
      case STRAND_TYPE_UINT: {
        uint32_t *val = va_arg(vargs, uint32_t *);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, stype, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_INT, ne, val);
	}
        else {
          werr = weed_set_uint_array(bundle, xiname, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_int_array(bundle, xiname, ne, (int *)val);
	}
      }
      break;
      case STRAND_TYPE_BOOLEAN: {
        int *val = va_arg(vargs, int *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_boolean_array(bundle, xiname, ne, val);
      }
      break;
      //
      case STRAND_TYPE_DOUBLE: {
        double *val = va_arg(vargs, double *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_double_array(bundle, xiname, ne, val);
      }
      break;
      case STRAND_TYPE_STRING: {
        char **val = va_arg(vargs, char **);
        /* char **xval = lives_calloc(ne, sizeof(char *)); */
        /* for (int j = 0; j < ne; j++) { */
        /*   if (val[j]) xval[j] = lives_strdup(val[j]); */
        /*   else xval[j] = NULL; */
        /* } */
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_string_array(bundle, xiname, ne, val);
        /* for (int j = 0; j < ne; j++) if (xval[j]) lives_free(xval[j]); */
        /* lives_free(xval); */
      }
      break;
      case STRAND_TYPE_INT64: {
        int64_t *val = va_arg(vargs, int64_t *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_int64_array(bundle, xiname, ne, val);
      }
      break;
      case STRAND_TYPE_UINT64: {
        uint64_t *val = va_arg(vargs, uint64_t *);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, stype, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_INT64, ne, val);
	}
        else {
          werr = weed_set_uint64_array(bundle, xiname, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_int64_array(bundle, xiname, ne, (int64_t *)val);
	}
      }
      break;
      case STRAND_TYPE_CONST_BUNDLEPTR: {
        void **val = va_arg(vargs, void **);
        if (array_op == 2) {
          werr = weed_ext_append_elements(bundle, xiname, WEED_SEED_VOIDPTR, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_ext_append_elements(bundle, xiname, WEED_SEED_PLANTPTR, ne, val);
	}
        else {
          werr = weed_set_voidptr_array(bundle, xiname, ne, val);
	  if (werr == WEED_ERROR_WRONG_SEED_TYPE)
	    weed_set_plantptr_array(bundle, xiname, ne, (weed_plant_t **)val);
	}
      }
	break;
      case STRAND_TYPE_BUNDLEPTR: {
        weed_plant_t **val = va_arg(vargs, weed_plant_t **);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_plantptr_array(bundle, xiname, ne, val);
      }
      break;
      case STRAND_TYPE_VOIDPTR: {
        void **val = va_arg(vargs, void **);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_voidptr_array(bundle, xiname, ne, val);
      }
      break;
      case STRAND_TYPE_FUNCPTR: {
        weed_funcptr_t *val = va_arg(vargs, weed_funcptr_t *);
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_funcptr_array(bundle, xiname, ne, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type %d invalid for %s in bundle.", vtype, xiname);
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


static boolean set_bundle_val(bundle_t *bundle, const char *name, uint32_t vtype,
                              uint32_t ne, int array_op, ...) {
  va_list vargs;
  boolean bval;
  va_start(vargs, array_op);
  bval = set_bundle_val_varg(bundle, name, vtype, ne, array_op, vargs);
  va_end(vargs);
  return bval;
}


LIVES_GLOBAL_INLINE size_t bundle_array_append_n(bundle_t *bundle, const char *name, uint32_t vtype,
    int ne, ...) {
  va_list vargs;
  va_start(vargs, ne);
  set_bundle_val_varg(bundle, name, vtype, ne, 2, vargs);
  va_end(vargs);
  return weed_leaf_num_elements(bundle, name);
}


size_t lives_strand_array_append(bundle_t *bundle, const char *name, uint32_t vtype, va_list varg) {
  set_bundle_val_varg(bundle, name, vtype, 1, 2, varg);
  return weed_leaf_num_elements(bundle, name);
}


void lives_set_strand_value_gen1(bundle_t *bundle, const char *name, uint32_t vtype, va_list val) {
  set_bundle_val_varg(bundle, name, vtype, 1, 0, val);
}


void lives_set_strand_value_gen2(bundle_t *bundle, const char *name, va_list val) {
  blueprint_t *blueprint = get_blueprint_from_bundle(bundle);
  strand_def_t *stdef = get_strand_def(blueprint, name);
  uint32_t vtype = stdef_get_type(stdef);
  set_bundle_val_varg(bundle, name, vtype, 1, 0, val);
}


void lives_strand_include_sub_bundles(bundle_t *bundle, const char *name, int ns, bundle_t **subs) {
  set_bundle_val(bundle, name, STRAND_TYPE_BUNDLEPTR, ns, 1, subs);
  for (int i = 0; i < ns; i++)
    set_bundle_val(subs[i], "container", STRAND_TYPE_VOIDPTR, 1, 0, bundle);
  // TODO - check for automation migration. if bundle has hook_stacks,
  // - recursively parse sub bundles. If we find a strand_def, then any hook automations shall
  // be added the hook stacks at this level
}


void lives_strand_include_sub_bundle(bundle_t *bundle, const char *name, bundle_t *sub) {
  set_bundle_val(bundle, name, STRAND_TYPE_BUNDLEPTR, 1, 0, sub);
  set_bundle_val(sub, "container", STRAND_TYPE_VOIDPTR, 1, 0, bundle);
  // TODO - check for automation migration. if bundle has hook_stacks,
  // - recursively parse sub bundles. If we find a strand_def, then any hook automations shall
  // be added the hook stacks at this level
}


static void lives_bundle_free(bundle_t *bundle) {
  if (bundle) {
    weed_size_t xnvals = 0;
    int nvals = 0;
    char **leaves = weed_plant_list_leaves(bundle, &xnvals);
    int i = 0;
    for (char *leaf = leaves[0]; leaf; leaf = leaves[++i]) {
      if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_PLANTPTR) {
        bundle_t **sub = weed_get_plantptr_array_counted(bundle, leaf, &nvals);
        for (int k = 0; k < nvals; k++) {
          if (sub[k] && sub[k] != bundle) {
            lives_bundle_free(sub[k]);
          }
        }
        weed_leaf_delete(bundle, leaf);
      } else if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_VOIDPTR) {
        if (weed_leaf_element_size(bundle, leaf, 0) > 0)
          lives_free(weed_get_voidptr_value(bundle, leaf, NULL));
      }
      lives_free(leaf);
    }
    if (xnvals) lives_free(leaves);
    weed_plant_free(bundle);
  }
}


boolean bundle_has_strand(bundle_t *bundle, const char *iname) {
  char *sname = get_short_name(iname);
  boolean bval;
  bval = weed_plant_has_leaf(bundle, sname);
  lives_free(sname);
  return bval == WEED_TRUE;
}


static boolean is_std_item(const char *item) {
  const char *ename;
  for (int i = 0; (ename = all_def_strands[i++]); g_print("NOW: %s", ename));
  return TRUE;
}


bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype) {
  bundle_t *bundle;

  // we ought to create the template first then use it to create instance,
  //but for now we will just create the instance directly
  bundle = create_bundle_by_type(OBJECT_INSTANCE_BUNDLE_TYPE, "type", otype, "subtype", subtype, NULL);

  // after this, we need to create contracts for the object
  // - (1) call get_contracts on a contract template with "create instance" intent
  ///
  //  (2) create a first instance with intent "negotiate", caps none, and flagged "no negotiate"
  // and mandatory attr: object type contract, state prepare.
  //
  // -- call more times to create contracts for other object transforms

  return bundle;
}


static void make_blueprint_for(bundle_type btype) {
  // create a blueprint for whatever bundle type from a bundledef
  // first time through we will not have blueprints for blueprint of strand_def
  // so we create from bdef
  // after 1 pass we have blueprint for strand_def, so we can create those from the blueprint
  // we then create a blueprint for blueprint using strand_defs from blueprint
  // we can now create blueprints and strand_def from blueprints
  // the remining process is to create bluprints for all the other bundle types
  int nstr = 0;
  bundle_t **st_defarr = NULL, *stdef = NULL, *bluep;
  const blueprint_t *bp;
  scriptlet_t *scriptlet = NULL;
  boolean err = FALSE;
  if (btype >= 0 && btype < n_builtin_bundledefs) {
    bundledef_t bdef;
    bundle_strand strand, strand2;
    char *vname;
    size_t dblen = lives_strlen(DIRECTIVE_BEGIN);
    size_t delen = lives_strlen(DIRECTIVE_END);
    uint64_t vflags;
    char *shname = get_short_name(STRAND_GENERIC_NAME);
    char *shflags = get_short_name(STRAND_GENERIC_FLAGS);
    char *shdef = get_short_name(STRAND_VALUE_DEFAULT);
    uint32_t vtype;
    uint64_t bflags;
    int tflags;
    int i, j = 0, dirx = 0;
    // we will pass through th bdef and create strand_def for each entry or directive
    // we will set name, flags and for values we will set type and default
    // for comments and directives we set text
    // (make_mandatory will clear the optional flag, and we won't store it)
    // (make_readonly will set the readonly flag, and we won't store it)
    // for other directives we store each atom in a string array
    LiVESList *dismands = NULL, *disreads = NULL, *xl;
    g_print("\n**************************MAKE BLUEPRINT FOR %d\n", btype);
    bluep = create_bundle_by_type(BLUEPRINT_BUNDLE_TYPE, NULL);
    if ((bp = blueprints[btype])) {
      // if we already have a blueprint we will reacreate it
      char *defname = get_short_name(STRAND_VALUE_DEFAULT);
      int ns, i;
      strand_def_t **sds = weed_get_plantptr_array_counted((weed_plant_t *)bp, ".strand_def",
							   &ns), *stdef = NULL;
      bundle_t *stdef_old;
      for (i = 0; i < ns; i++) {
	stdef_old = sds[i];
	vtype = stdef_get_type(stdef_old);
	//if (vtype == STRAND_TYPE_NONE) continue;
	vname = (char *)stdef_get_name(stdef_old);
	bflags = stdef_get_flags(stdef_old);
        stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "name", vname,
				       "flags", bflags, "strand_type", vtype, NULL);
	lives_free(vname);
	// copy default
	if (bundle_has_strand(stdef_old, defname))
	  bundle_strand_copy(stdef, defname, stdef_old, defname);
        st_defarr = lives_realloc(st_defarr, ++nstr * sizeof(bundle_t *));
        st_defarr[nstr - 1] = stdef;
      }
      if (sds) lives_free(sds);
      lives_free(defname);
    }
    else {
      bdef = (bundledef_t)get_bundledef(btype);
      for (i = 0; bdef[i]; i++) {
      off_t offx = 0;
      bflags = 0;
      strand = bdef[i];
      //g_print("STRAND ------------ %s\n", strand);
      vflags = get_vflags(strand, &offx, NULL, bdef);

      if (vflags == STRAND_TYPE_FLAG_COMMENT) continue;

      if (vflags == STRAND_TYPE_FLAG_DIRECTIVE) {
        if (!lives_strncmp(strand, DIRECTIVE_END, delen)) {
          //g_print("END dirx %s\n", strand + delen);
          dirx--;
          /* if (stdef) { */
          /*   st_defarr = lives_realloc(st_defarr, ++nstr * sizeof(bundle_t *)); */
          /*   st_defarr[nstr - 1] = stdef; */
          /*   stdef = NULL; */
          /* } */
          continue;
        }
        if (!lives_strncmp(strand, DIRECTIVE_BEGIN, dblen)) {
	  boolean mm = FALSE, mr = FALSE;
	  dirx++;
          if ((mm = !strcmp(strand + dblen, "make_mandatory"))
	      || (mr = !strcmp(strand + dblen, "make_readonly"))) {
            // clear opt from flag
            strand = bdef[++i];
            if (strand) {
              char *str;
              for (j = 0; j < nstr; j++) {
                bundle_t *stddsc = st_defarr[j];
                char *xname = find_strand_by_name(stddsc, "name");
                if (xname) {
                  str = lives_strand_get_value_string(stddsc, xname);
                  if (!lives_strcmp(strand, str)) {
                    uint64_t oflags;
                    oflags = lives_strand_get_value_uint64(stddsc, "flags");
		    if (mm) oflags &= ~BLUEPRINT_FLAG_OPTIONAL;
		    else if (mr) oflags |= BLUEPRINT_FLAG_READONLY;
		    set_bundle_val(stddsc, "flags", STRAND_TYPE_UINT64, 1, 0,  oflags);
		    lives_free(str);
                    lives_free(xname);
                    break;
                  }
                  lives_free(str);
                  lives_free(xname);
                }
              }
              if (j >= nstr) {
		if (mm) dismands = lives_list_append(dismands, lives_strdup(strand));
		else if (mr) disreads = lives_list_append(disreads, lives_strdup(strand));
		// pass end directive
		i++;
	      }
	    }
	    continue;
	  }
	}
      }
      if (dirx) continue;
      //
      // remaining strands are values
      if (vflags == STRAND_TYPE_FLAG_OPTIONAL) bflags |= BLUEPRINT_FLAG_OPTIONAL;
      vtype = get_vtype(strand, &offx);
      vname = (char *)get_vname(strand + offx);
      if ((xl = lives_list_locate_string(dismands, vname))) {
        bflags &= ~BLUEPRINT_FLAG_OPTIONAL;
        dismands = lives_list_remove_node(dismands, xl, TRUE);
      }
      if ((xl = lives_list_locate_string(disreads, vname))) {
        bflags |= BLUEPRINT_FLAG_READONLY;
        disreads = lives_list_remove_node(disreads, xl, TRUE);
      }
      strand2 = bdef[++i];
      tflags = atoi(strand2);
      if (tflags & STRAND2_FLAG_PTR_TO_ARRAY) bflags |= BLUEPRINT_FLAG_PTR_TO_ARRAY;
      else {
        if (tflags & STRAND2_FLAG_ARRAY_OF) bflags |= BLUEPRINT_FLAG_ARRAY;
        if (tflags & STRAND2_FLAG_PTR_TO) bflags |= BLUEPRINT_FLAG_PTR_TO_SCALAR;

        stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "name", vname,
				       "flags", bflags, "strand_type", vtype, NULL);

	bflags = 0;
	if (vtype != STRAND_TYPE_NONE) {
	  err = set_def_value_gen1(stdef, shdef, strand, strand2);
	  if (err) goto endit2;
	}
        st_defarr = lives_realloc(st_defarr, ++nstr * sizeof(bundle_t *));
        st_defarr[nstr - 1] = stdef;
        stdef = NULL;
      }
      }
      lives_free(shname); lives_free(shflags); lives_free(shdef);
    }

  if (nstr > 1) {
    lives_strand_include_sub_bundles(bluep, "strand_def", nstr, st_defarr);
    lives_free(st_defarr);
  }

endit2:
  if (err) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR creating blueprint for type %d\n", btype);
#endif
    if (bluep) {
      lives_bundle_free(bluep);
      bluep = NULL;
    }
  }
  if (blueprints[btype]) lives_bundle_free((bundle_t *)blueprints[btype]);
  blueprints[btype] = bluep;
  }
}


/////////// nirvascope

static char *nirvascope(int i, nscope_hdr_f head_func, nscope_cond_f cond_func, void *cdata,
		       nscope_text_f text_func, void *tdata, nscope_ftr_f foot_func) {
  char *hdr;
  if (!head_func) hdr = lives_strdup("");
  else hdr = (*head_func)(i);
  for (int j = 0; (*cond_func)(i, j, cdata); j++) {
    char *sep = NULL, *fmt = "", *res;
    res = text_func(i, j, tdata, &sep, &fmt);
    if (res) {
      hdr = lives_strdup_concat(hdr, sep, fmt, res);
      lives_free(res);
    }
  }
  if (foot_func) {
    char *line = (*foot_func)(i);
    hdr = lives_strdup_concat(hdr, NULL, "%s", line);
  }
  return hdr;
}

// cond funcs
static boolean non_null_elem(int i, int idx, void *data) {
  char **arr = (char **)data;
  return arr[idx] != NULL;
}

// text fmt funcs
static char *elem_nl(int i, int idx, void *data, char **sep, char **fmt) {
  char **arr = (char **)data;
  *sep = "\n";
  *fmt = "%s";
  return lives_strdup(arr[idx]);
}

static char *nirvascope_print_str_array(char **arr) {
  return nirvascope(0, NULL, non_null_elem, arr, elem_nl, arr, NULL);
}

// show what a bundle would look like as a 'C/C++' header
// input: any bundle [target_obj]
// output: scriptlet, FUNC CATEGORY_NATIVE_TEXT
char *nirvascope_blueprint_to_header(bundle_t *bundle, const char *tname) {
  blueprint_t *bp;
  char *hdr, *ar = NULL, *line;
  uint32_t st;
  weed_size_t ne;
  const char *tp;

  if (tname) hdr  = lives_strdup("typedef struct {");
  else hdr = lives_strdup("struct {");

  bp = get_blueprint_from_bundle(bundle);
  if (bp) {
    int ns;
    char *comment = NULL;
    strand_def_t **sds = weed_get_plantptr_array_counted(bp, ".strand_def", &ns);
    boolean is_arr, is_ptr_to;
    for (int i = 0; i < ns; i++) {
      char *stname, *sname;
      uint32_t st, sttype;
      strand_def_t * stdef = sds[i];
      if (!lives_strand_is_value(stdef)) continue;
      sttype = stdef_get_type(stdef);
      stname = stdef_get_name(stdef);
      sname = get_short_name(stname);

      //g_print("STRAND is %s\n", strand);

      st = strand_type_to_weed_seed(sttype);
      is_arr = lives_strand_is_array(stdef);
      tp = weed_seed_to_ctype(st, TRUE);
      is_ptr_to = lives_strand_is_pointer_to(stdef);
      if (!is_ptr_to) {
	if (sttype == WEED_SEED_PLANTPTR) tp = "bundle_t *";
	if (is_arr) ar = lives_strdup_printf("[]");
      }
      else {
	if (sttype == STRAND_TYPE_BUNDLEPTR || sttype == STRAND_TYPE_CONST_BUNDLEPTR) {
	  if (lives_strand_is_pointer_to_array(stdef)) {
	    tp = "const bundlearray_t *";
	  }
	  else {
	    tp = "const bundle_t **";
	    if (is_arr) ar = lives_strdup_printf("[]");
	  }
	}
      }
      if (lives_strand_is_optional(stdef)) comment = lives_strdup("// optional");
      line = lives_strdup_printf("\n  %s%s%s;\t%s", tp, sname + 1, ar ? ar : "",
				 comment);
      lives_freep((void **)&comment);
      hdr = lives_concat(hdr, line);
      if (ar) {
	lives_free(ar);
	ar = NULL;
      }
      lives_free(sname);
      lives_free(stname);
    }
  }
  if (!tname) line = lives_strdup("\n}");
  else line = lives_strdup_printf("\n} %s;", tname);
  hdr = lives_concat(hdr, line);
  return hdr;
}


char *nirvascope_bundle_to_header(bundle_t *bundle, const char *tname, int idx) {
  blueprint_t *bp;
  const char *tabs;
  char *hdr, *ar = NULL, *line, *xline, *val = NULL;
  uint32_t st;
  weed_size_t ne;
  char **leaves = weed_plant_list_leaves(bundle, NULL);
  const char *tp;

  if (!idx) {
    if (tname) hdr = lives_strdup("typedef struct {");
    else hdr = lives_strdup("struct {");
    tabs = "";
  }
  else {
    hdr = lives_strdup_printf("    [%d]", idx - 1);
    tabs = "\t";
  }
  for (int i = 0; leaves[i]; i++) {
    if (*leaves[i] == '.') {
      st = weed_leaf_seed_type(bundle, leaves[i]);
      ne = weed_leaf_num_elements(bundle, leaves[i]);
      tp = weed_seed_to_ctype(st, TRUE);
      if (st == WEED_SEED_PLANTPTR) tp = "bundle_t *";
      if (ne > 1) ar = lives_strdup_printf("[%d]", ne);
      else {
	switch (st) {
	case WEED_SEED_STRING: {
	  char *vs = weed_get_string_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (\"%s\")", vs);
	  lives_free(vs);
	}
	break;
	case WEED_SEED_INT: {
	  int vs = weed_get_int_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (%d)", vs);
	}
	break;
	case WEED_SEED_UINT: {
	  uint32_t vs = weed_get_uint_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (%u)", vs);
	}
	break;
	case WEED_SEED_INT64: {
	  int64_t vs = weed_get_int64_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (0X%016lX)", vs);
	}
	break;
	case WEED_SEED_UINT64: {
	  uint64_t vs = weed_get_uint64_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (0X%016lX)", vs);
	}
	break;
	case WEED_SEED_VOIDPTR: {
	  void *vs = weed_get_voidptr_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (%p)", vs);
	}
	break;
	case WEED_SEED_PLANTPTR: {
	  weed_plant_t *vs = weed_get_plantptr_value(bundle, leaves[i], NULL);
	  val = lives_strdup_printf("\t (%p)", vs);
	}
	break;
    default: break;
    }
      }
      xline = lives_strdup_printf("\n  %s%s%s%s;", tabs, tp, leaves[i] + 1,
				  ar ? ar : "");
      line = lives_strdup_printf("%-40s%s", xline, val ? val : "");
      lives_free(xline);
      lives_freep((void **)&val);
      hdr = lives_concat(hdr, line);
      if (ar) {
	if (st == WEED_SEED_PLANTPTR) {
	  weed_plant_t **pp = weed_get_plantptr_array(bundle, leaves[i], NULL);
	  if (pp) {
	    for (int j = 0; j < ne; j++) {
	      xline = nirvascope_bundle_to_header(pp[j], NULL, j + 1);
	      line = lives_strdup_printf("\n%s", xline);
	      lives_free(xline);
	      hdr = lives_concat(hdr, line);
	    }
	    lives_free(pp);
	  }
	}
	lives_free(ar);
	ar = NULL;
      }
    }
    lives_free(leaves[i]);
  }
  lives_free(leaves);
  if (idx) line = lives_strdup("\n");
  else {
    if (!tname) line = lives_strdup("\n}");
    else line = lives_strdup_printf("\n} %s;", tname);
  }
  hdr = lives_concat(hdr, line);
  return hdr;
}


#define CONT_PFX "CONT"

bundle_t *lookup_item_in_array(bundle_t *con, const char *aname) {
  if (con) {
    char *kname = lives_strdup_printf("%s%s", CONT_PFX, aname);
    lives_bundle_t *tagr = weed_get_plantptr_value(con, kname, NULL);
    lives_free(kname);
    return tagr;
  }
  return NULL;
}


char *find_strand_by_name(bundle_t *bun, const char *name) {
  char *sname;
  if (weed_plant_has_leaf(bun, name)) return lives_strdup(name);
  sname = get_short_name(name);
  if (weed_plant_has_leaf(bun, sname)) return sname;
  return NULL;
}


uint32_t lives_get_attr_value_type(lives_attr_t *attr) {
  if (!attr) return WEED_SEED_INVALID;
  if (weed_plant_has_leaf(attr, WEED_LEAF_DEFAULT))
    return weed_leaf_seed_type(attr, WEED_LEAF_DEFAULT);
  return weed_leaf_seed_type(attr, WEED_LEAF_VALUE);
}


uint32_t get_attr_type(lives_attr_t *attr) {
  return weed_seed_to_attr_type(lives_get_attr_value_type(attr));
}


int lives_attr_get_value_int(lives_attr_t *attr) {
  if (!attr) return 0;
  return weed_get_int_value(attr, WEED_LEAF_VALUE, NULL);
}
int64_t lives_attr_get_value_int64(lives_attr_t *attr) {
  if (!attr) return 0;
  return weed_get_int64_value(attr, WEED_LEAF_VALUE, NULL);
}
bundle_t *lives_attr_get_value_bundleptr(lives_attr_t *attr) {
  if (!attr) return NULL;
  return weed_get_plantptr_value(attr, WEED_LEAF_VALUE, NULL);
}

//

bundle_t *lives_strand_get_value_bundleptr(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  weed_plant_t *val = weed_get_plantptr_value(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t *)val;
}

bundle_t *lives_strand_get_value_const_bundleptr(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  weed_error_t err;
  void *val = weed_get_voidptr_value(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (void *)weed_get_plantptr_value(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t *)val;
}

void *lives_strand_get_value_voidptr(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  void *val = weed_get_voidptr_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

int lives_strand_get_value_int(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  int val = weed_get_int_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

uint32_t lives_strand_get_value_uint32(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  uint32_t val;
  weed_error_t err;
  val = weed_get_uint_value(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (uint32_t)weed_get_int_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

int64_t lives_strand_get_value_int64(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  int64_t val = weed_get_int64_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

uint64_t lives_strand_get_value_uint64(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  weed_error_t err;
  uint64_t val = weed_get_uint64_value(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (uint64_t)weed_get_int64_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

char *lives_strand_get_value_string(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  char *val = weed_get_string_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

bundle_t **lives_strand_get_array_bundletptr(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  weed_error_t err;
  weed_plant_t **vals = weed_get_plantptr_array(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    vals = (weed_plant_t **)weed_get_voidptr_array(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t **)vals;
}

char **lives_strand_get_array_string(bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  char **vals = weed_get_string_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}


size_t nirvascope_get_bundle_weight(bundle_t *b) {
  if (!b) return 0;
  else {
    size_t tot = weed_plant_get_byte_size(b);
    char **ll = weed_plant_list_leaves(b, NULL), *l;
    for (int i = 0; (l = ll[i]); i++) {
      if (weed_leaf_seed_type(b, l) == WEED_SEED_PLANTPTR) {
	int np;
	weed_plant_t **pp = weed_get_plantptr_array_counted(b, l, &np);
	for (int j = 0; j < np; j++) {
	  if (pp[j] != b) tot += nirvascope_get_bundle_weight(pp[j]);
	}
	if (pp) lives_free(pp);
      }
      lives_free(l);
    }
    if (ll) lives_free(ll);
    return tot;
  }
}
