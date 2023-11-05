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

static uint32_t weed_seed_to_strand_type(uint32_t st);

static strand_def_t *get_strand_def_by_name(blueprint_t *, const char *name);
static strand_def_t *get_strand_def(bundle_t *, const char *name);
static uint32_t stdef_get_strand_type(strand_def_t *stdef, bundle_t *bundle);

/////// gen 1 functions ////
static boolean set_strand_val_varg(bundle_t *, const char *iname, uint32_t vtype,
                                   uint32_t ne, int array_op, va_list vargs);
static boolean set_strand_val(bundle_t *, const char *name, uint32_t vtype,
                              uint32_t ne, int array_op, ...);
void lives_strand_value_set(bundle_t *, const char *strand_nam, uint32_t st_type, ...);
static boolean stdef_is_comment(strand_def_t *);

/////////////////////////////////

static bundledef_t new_bdefs[N_BUNDLE_DEFS];
lives_obj_t **abundle_bdefs;

NIRVA_CORE_DEFS

static void validate_bdefs(void);

static const bundle_t *blueprints[N_BUNDLE_DEFS];
static void make_blueprint_for(bundle_type btype);

#define CALL_WRAPPER(func,...) WRAP_FN_##func(__VA_ARGS__);

#define MAKE_WRAP_FN(func, ret, nparams, ...)			\
  uint64_t WRAP_FN_##func(bundleptr in, bundleptr out, ...) {	\
    va_list va;							\
    va_start(va, out);						\
    FN_BEND(func, in, out, nparams, __VA_ARGS__, va);		\
    va_end(va);							\
    return GET_RETVAL(out);}					\


// impl funcs

bundle_t *lives_bundle_create(void) {return weed_plant_new(LIVES_PLANT_BUNDLE);}


const bundle_t *init_bundles(void) {
  // will init the system and create all standard bundle definitions
  //const char *plugname = "/usr/lib/nirva/subsystems/prime";
  bundle_t *bun;

  // BEGIN STAGE 1 BOOTSTRAP
  NIRVA_FUNC_GENERATION = 1;

  NIRVA_INIT_CORE;

  // for FULL automation this will be done automatically
  // after creating all core bundles we should validate them
  validate_bdefs();

  // BEGIN STAGE 2 BOOTSTRAP
  // we are now able to create bundles from bundledefs
  NIRVA_FUNC_GENERATION = 2;

  for (int i = 0; i < N_BUNDLE_DEFS; i++) blueprints[i] = NULL;

  // recreate all the bundle defs as "blueprints"
  // starting with the most basic, we create blueprints from bundledefs
  // for BLUEPRINT_BUNDLE_TYPE, we need DEF, INDEX, and STRAND_DEF so we will create blueprints for
  // DEF, STRAND_DEF and BLUEPRINT from bundledefs
  make_blueprint_for(DEF_BUNDLE_TYPE);
  make_blueprint_for(STRAND_DEF_BUNDLE_TYPE);
  make_blueprint_for(INDEX_BUNDLE_TYPE);
  make_blueprint_for(BLUEPRINT_BUNDLE_TYPE);

  /// now we will use the blueprints-from-bdefs to recreate the blueprints, but this time from blueprints
  // thus we will have a blueprint for blueprint, made from bundles created from blueprints
  make_blueprint_for(DEF_BUNDLE_TYPE);
  make_blueprint_for(STRAND_DEF_BUNDLE_TYPE);
  make_blueprint_for(INDEX_BUNDLE_TYPE);
  make_blueprint_for(BLUEPRINT_BUNDLE_TYPE);

  // create blueprints for all standard bundles
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    make_blueprint_for(i);
  }

  // recreate all blueprints with all blueprints defined, just in case we missed anything on pass1
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    make_blueprint_for(i);
    g_print("\n%s\n", nirvascope_bundle_to_header((bundle_t *)blueprints[i], '\0', 0));
  }

  // now test by building and freeing one copy of each bundle
  // from its blueprint
  for (int i = 0; i < N_BUNDLE_DEFS; i++) {
    bundle_t *b = create_bundle_by_type(i, NULL);
    g_print("\n\ncreated bundle type %d\n", i);
    g_print("%s\n", nirvascope_bundle_to_header(b, NULL, 0));
    g_print("bsize == %lu\n", nirvascope_get_bundle_weight(b));
    lives_bundle_free(b);
  }

  // we have reached bootstrap level 3, we can now build bundles from their blueprint bundles
  NIRVA_FUNC_GENERATION = 3;

  // the default intent is to create an object template type structural
  // in the Satisfaction Cascade, there will be default contract, for intent CREATE_BUNDLE, CAPS_SEL bndle type
  // which wraps a single standard functional, no in params, but one out param "TX_RESULT"
  //
  // we will get a transform for this and test our current CAPs, which will be accepted, and action the transform
  // the return attr will be reparented to itself and set as STRUCTURE_PRIME
  //
  /////////////////////////////////////
  // this will dlopen it, find the nirva_init function
  // and call it
  //
  // nirva_load
  // - dlopen plugin
  // - create an LSD struct
  // call nirva_init in plugin
  // - plugin fills in LSD struct
  // pl rturns
  // LSD is parsed
  // - ctx examined

  // for st. prime
  // this will return a nirva LSD containing a contract to create object_template
  // type struct
  //
  //
  // first it will create a proto blueprint object template complete with sub bundles
  // and load the blueprint object attr_pack
  // this must be done using gen 3 functions
  //
  // it will then produce the factory subtypes needed to produce obj tmpl / obj inst
  // now using gen3 funcs it will create obj tmpl of type blueprint
  // with cx for each bundl type
  // each one will require input of the factory subtype
  // and a contract to build factory subtypes
  // it will destroy all instances
  // return obj_tmpl
  //
  // Prime will find a contract for obj store and another for contract store
  // it will proxy those contracts as structure tx
  // - later this will passed to broker
  // - obj store - can be passed to lifecycle
  //
  // it will pull contracts from bp templ and store in ctx strore
  // it will add a hook to bp created inst for bp, each inst is added to obj_store ?
  // it will find ctx for obj_tmpl and action it
  //
  // obj tmplt is returned to NIRVA_LOAD
  // NIRVA_LOAD will action this
  //

  // tx in prime will call nirva_init in each object in store blueprint instance
  // and get contracts for each instance.
  // it will find the one for object store and grab one, then store all contracts in it
  //
  // object_template to create an object template of
  // type stuctural. This is structure_prime.
  // The function will load the attr_pack, which includes all config attributes.
  // this is returned to caller
  //
  //  STRUCTURE_PRIME = NIRVA_LOAD(plugname);

  // grab the default INTENTCAP, we need to satisfy this and return the output

  /* NIRVA_DECLARE_ORACLE(NIRVA_PRIORITY_DEFAULT, NIRVA_EQUALS, _TARGET_ITEM(NAME), \ */
  /* 		       ATTR_STRUCTURAL_BLUEPRINT, _COND_FIN,  ATTR_TYPE_CONST_BUNDLEPTR, \ */
  /* 		       get_blueprint_for_btype, ATTR_TYPE_UINT64); */

  /* NIRVA_DEF_ORACLE(ATTR_STRUCT_ATTR_PACK, 10, _COND_ALWAYS, _COND_FIN, get_attr_pack) */
  /* NIRVA_DEF_ORACLE(ATTR_STRUCT_BLUEPRINTS, get_all_blueprints) */

  bun = NIRVA_SATISFY(NIRVA_DEF_ICAPI, NIRVA_DEF_ICAPC);

  NIRVA_FUNC_GENERATION = 4;

  return bun;
}


LIVES_GLOBAL_INLINE char *get_short_name(const char *q) {
  if (q) {
    char *p = (char *)q;
    if (*p != '.') {
      char *xiname = lives_strdup(".");
      xiname = lives_concat(xiname, lives_strdup(lives_string_tolower(p)));
      return xiname;
    }
    return lives_strdup(lives_string_tolower(p));
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////
/// gen 1 functions

// these functions are for stage 1 bootstrap. Once bundledefs are remade as BLUEPRINTS
// then we move to gen 2

const bundledef_t get_bundledef(bundle_type btype) {
  return (const bundledef_t)maker_get_bundledef(btype);
}


/* static int skip_directive(bundledef_t bdef, int i) { */
/*   // returns number of lines to skip, starting from DIRECTIVE_BEGIN */
/*   if (bdef && i >= 0) { */
/*     bundle_strand strand = bdef[i]; */
/*     if (lives_str_starts_with(strand, DIRECTIVE_BEGIN)) { */
/*       for (int j = 1; (strand = bdef[i + j]); j++) { */
/*         if (lives_str_starts_with(strand, DIRECTIVE_END)) return ++j; */
/*       } */
/*     } */
/*   } */
/*   return 1; */
/* } */


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
  }
  /* else if (*q == STRAND_TYPE_FLAG_DIRECTIVE) { */
  /*   if (bdef && ii) { */
  /*     int skippy = skip_directive(bdef, *ii); */
  /*     //g_print("SKIPPY is %d\n", skippy); */
  /*     if (ii) *ii += skippy - 1; */
  /*     if (offx) *offx = -skippy; */
  /*   } */
  /*   vflags |= STRAND_TYPE_FLAG_DIRECTIVE; */
  /*   return vflags; */
  /* } */
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

LIVES_GLOBAL_INLINE int get_restrnum(const char *q) {return *q ? atoi(q) : 0;}

LIVES_GLOBAL_INLINE boolean get_is_array(const char *q) {return (q && *q == '1');}

static int bundledef_get_item_idx(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  // return the idx of the strand[] (strand) where "item" starts
  // -1 if not found
  bundle_strand strand;
  off_t offx;
  int tdef = -1, tflags;
  const char *vname;
  char *sname = exact ? (char *)item : get_short_name(item);
  for (int i = 0; (strand = bundledef[i]); i++) {
    //g_print("check i %d\n", i);
    offx = 0;
    get_vflags(strand, &offx, &i, bundledef);
    if (offx < 0) continue;
    get_vtype(strand, &offx);
    vname = get_vname(strand + offx);

    tflags = atoi(strand + offx + 1);
    if (tflags & STRAND2_FLAG_TEMPLATE) tdef = i - 1;

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
  if (tdef >= 0) return tdef;
  return -1;
}


bundledef_t get_bundledef_fraom_bundle(bundle_t *bundle) {
  char *sname2 = get_short_name("blueprint");
  bundledef_t bundledef = (bundledef_t)weed_get_voidptr_value(bundle, sname2, NULL);
  lives_free(sname2);
  return bundledef;
}

// gen 1

// this should only be called if all checks have been performed - restrictions, readonly, proxy
// etc
static boolean set_strand_val(bundle_t *bundle, const char *name, uint32_t vtype,
                              uint32_t ne, int array_op,  ...) {
  // if array_op > 0 and ne == 1, then idx tells us the source is vargs[idx]
  va_list vargs;
  boolean bval;
  va_start(vargs, array_op);
  bval = set_strand_val_varg(bundle, name, vtype, ne, array_op, vargs);
  va_end(vargs);
  return bval;
}


void set_strand_value(bundle_t *bundle, const char *name, uint32_t vtype, ...) {
  va_list va;
  va_start(va, vtype);
  set_strand_val_varg(bundle, name, vtype, 1, 0, va);
  va_end(va);
}


static boolean set_def_value_gen1(bundle_t *bundle, const char *overrd_name, \
                                  bundle_strand strand, bundle_strand strand2) {
  const char *vname;
  const char *sname;
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
  if (vtype == STRAND_TYPE_FLAG_DIRECTIVE) vtype = STRAND_TYPE_STRING;
  if (vtype == STRAND_TYPE_NONE) return FALSE;

  vname = get_vname(strand + offx);
  if (overrd_name) sname = overrd_name;
  else sname = vname;
  if (lives_strlen_atleast(strand2, 2)) {
    const char *defval = strand2 + 2;
    //boolean is_array = get_is_array(strand2);
    boolean is_array = FALSE; // just set to scalar and 1 value - defaults are mostly just 0 oe NULL
    switch (vtype) {
    case (STRAND_TYPE_STRING): case (STRAND_TYPE_VOIDPTR):
    case (STRAND_TYPE_BUNDLEPTR):
    case (STRAND_TYPE_CONST_BUNDLEPTR):
      if (!lives_strcmp(defval, "NULL") || !lives_strcmp(defval, "((void *)0)"))
        err = set_strand_val(bundle, sname, vtype, 1, is_array, NULL);
      else if (vtype == STRAND_TYPE_STRING)
        err = set_strand_val(bundle, sname, vtype, 1, is_array, defval);
      break;
    case (STRAND_TYPE_INT):
    case (STRAND_TYPE_UINT):
    case (STRAND_TYPE_BOOLEAN):
      err = set_strand_val(bundle, sname, vtype, 1, is_array, atoi(defval));
      break;
    case (STRAND_TYPE_INT64):
      err = set_strand_val(bundle, sname, vtype, 1, is_array, lives_strtol(defval));
      break;
    case (STRAND_TYPE_UINT64):
      err = set_strand_val(bundle, sname, vtype, 1, is_array, lives_strtoul(defval));
      break;
    case (STRAND_TYPE_DOUBLE):
      err = set_strand_val(bundle, sname, vtype, 1, is_array, lives_strtod(defval));
      break;
    default: break;
    }
#if DEBUG_BUNDLES
    if (!err) g_printerr("Setting default for %s [%s] to %s\n", sname, vname, defval);
#endif
  } else {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Missing default for %s [%s]\n", sname, vname);
#endif
    err = TRUE;
  }
  return err;
}


static bundledef_t get_obj_bundledef(uint64_t otype, uint64_t osubtype) {
  return NULL;
}


static bundledef_t validate_bdef(bundledef_t bdef) {
  // check bundledefs to make sure they contain only valid elements
  // any duplicate item names are removed. If we have an optional version and mandatory then
  // we retain the mandatory one
  bundledef_t newq = NULL;
  bundle_strand stranda;
  boolean err = FALSE;
  /* size_t dblen = lives_strlen(DIRECTIVE_BEGIN); */
  /* size_t delen = lives_strlen(DIRECTIVE_END); */
  int nq = 0;//, dirx = 0;
  for (int i = 0; (stranda = bdef[i]); i++) {
    bundle_strand stranda2, strandb, strandb2;
    off_t offx = 0;
    uint64_t vflagsa, vflagsb;
    const char *vnamea, *vnameb;
    char *snamea, *snameb;
    uint32_t vtypea, vtypeb;
    int j;

    g_print("VAL1 %s\n", stranda);

    vflagsa = get_vflags(stranda, &offx, NULL, bdef);
    if (vflagsa == STRAND_TYPE_FLAG_COMMENT) {
      nq++;
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 1] = lives_strdup(stranda);
      newq[nq] = NULL;
      //g_print("GOT comment: %s\n", stranda);
      continue;
    }
    /* if (vflagsa == STRAND_TYPE_FLAG_DIRECTIVE) { */
    /*   if (dirx) { */
    /* 	if (!lives_strncmp(stranda, DIRECTIVE_END, delen)) { */
    /* 	  //g_print("END dirx %s\n", stranda + delen); */
    /* 	  dirx--; */
    /* 	  nq++; */
    /* 	  newq = lives_realloc(newq, (nq + 1) * sizeof(char *)); */
    /* 	  newq[nq - 1] = lives_strdup(stranda); */
    /* 	  newq[nq] = NULL; */
    /* 	  continue; */
    /* 	} */
    /*   } */
    /*   if (!strncmp(stranda, DIRECTIVE_BEGIN, dblen)) { */
    /* 	//g_print("START dirx %s\n", stranda + dblen); */
    /* 	dirx++; */
    /*   } */
    /* } */
    /* if (dirx) { */
    /*   //g_print("SKIP directive data: %s\n", stranda); */
    /*   nq++; */
    /*   newq = lives_realloc(newq, (nq + 1) * sizeof(char *)); */
    /*   newq[nq - 1] = lives_strdup(stranda); */
    /*   newq[nq] = NULL; */
    /*   continue; */
    /* } */
    i++;
    if (!(stranda2 = bdef[i])) {
#if DEBUG_BUNDLES
      g_print("%s\n", stranda);
      g_print("Second strand not found !\n");
#endif
      err = TRUE;
      break;
    }

    g_print("VAL2 %s\n", stranda2);

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
    //char *name;
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


bundle_t *create_gen1_bundle_with_vargs(uint64_t bt, bundledef_t bundledef, va_list vargs) {
  bundle_t *bundle = weed_plant_new(LIVES_PLANT_BUNDLE);
  if (bundle) {
    boolean err = FALSE;
    bundle_strand strand;
    const char *vname;
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
          set_strand_val(bundle, iname, vtype, 1, 0, vargs, 0);
        }
        if (err) {
          g_printerr("ERROR adding item '%s' to bundle, invalid item name\n", err_item);
          BREAK_ME("invname 1");
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
          if (!(vflags & STRAND_TYPE_FLAG_OPTIONAL)) {
            //g_print("%s is mando\n", sname);
            if (!bundle_has_strand(bundle, vname)) {
              //g_print("%s is absent\n", sname);
              //g_print("SETTING to default\n");
              err = set_def_value_gen1(bundle, NULL, strand, bundledef[i]);
              if (err) goto endit;
            }
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


////////////////////////////////// gen 2 funcfions - stage 2+ bootstrap //

void lives_strand_delete(bundle_t *bundle, const char *stname) {
  if (bundle) {
    char *sname = get_short_name(stname);
    weed_leaf_delete(bundle, sname);
    lives_free(sname);
  }
}


int64_t lives_strand_copy(bundle_t *dest, const char *dname, bundle_t *src,
                          const char *sname) {
  char *shname = get_short_name(dname);
  char *shname2 = get_short_name(sname);
  lives_leaf_copy(dest, shname, src, shname2);
  lives_free(shname);
  lives_free(shname2);
  return 1;
}

uint64_t stdef_get_flags(strand_def_t *stdef) {
  return lives_strand_get_value_uint64(stdef, "flags");
}

static uint32_t bundle_get_strand_type(bundle_t *bundle) {
  if (bundle) return lives_strand_get_value_uint32(bundle, "strand_type");
  return STRAND_TYPE_NONE;
}

static uint32_t stdef_get_strand_type(strand_def_t *stdef, bundle_t *bundle) {
  uint32_t sttype = bundle_get_strand_type(stdef);
  if (sttype == STRAND_TYPE_PROXIED && bundle) {
    char *type_proxy = lives_strand_get_value_string(stdef, "type_proxy");
    if (type_proxy) sttype = NIRVA_GET_VALUE_UINT(bundle, type_proxy);
  }
  if (sttype == STRAND_TYPE_PROXIED) return STRAND_TYPE_UNDEFINED;
  return sttype;
}

char *stdef_get_comment(strand_def_t *stdef) {
  if (stdef_is_comment(stdef)) return lives_strand_get_value_string(stdef, "default");
  return NULL;
}

boolean stdef_is_comment(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_COMMENT);
}

scriptlet_t *stdef_get_restrictions(strand_def_t *stdef) {
  return weed_get_plantptr_value(stdef, "restrictions", NULL);
}

boolean stdef_is_value(strand_def_t *stdef) {
  return !(stdef_get_flags(stdef) & BLUEPRINT_FLAG_COMMENT);
}

boolean stdef_is_optional(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_OPTIONAL);
}

boolean stdef_is_readonly(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_READONLY);
}

boolean stdef_get_set_sub_rdonly(strand_def_t *stdef) {
  return !!(stdef_get_flags(stdef) & BLUEPRINT_FLAG_RDONLY_SUB);
}

boolean stdef_is_array(strand_def_t *stdef) {
  return (lives_strand_get_value_int(stdef, "MAX_SIZE") != 0);
}

boolean stdefs_exclude(strand_def_t *stdef, const char *oname) {
  return FALSE;
}

boolean stdef_excluded_by(strand_def_t *stdef) {
  return FALSE;
}

char *stdef_get_name(strand_def_t *stdef) {
  return lives_strand_get_value_string(stdef, "name");
}

bundle_t *get_strand_def_by_name(bundle_t *blueprint, const char *sname) {
  bundle_t *stdef = NIRVA_GET_VALUE_BY_KEY(blueprint, "strand_defs", sname);
  if (!stdef) stdef = NIRVA_GET_VALUE_BUNDLEPTR(blueprint, "multi");
  return stdef;
}

strand_def_t *get_strand_def(bundle_t *bundle, const char *name) {
  blueprint_t *blueprint = get_blueprint_from_bundle(bundle);
  if (blueprint) return get_strand_def_by_name(blueprint, name);
  return NULL;
}


const blueprint_t *get_blueprint_for_btype(bundle_type btype) {
  if (btype >= 0 && btype < n_builtin_bundledefs) return blueprints[btype];
  return NULL;
}


blueprint_t *get_blueprint_from_bundle(bundle_t *b) {
  return (blueprint_t *)
         lives_strand_get_value_const_bundleptr(b, "blueprint");
}


strand_def_t *get_mand_strand_def(blueprint_t *bp, int idx) {
  int ns, i;
  char *sname = get_short_name("strand_defs");
  bundle_t *stdef = NULL, **sdefs = weed_get_plantptr_array_counted(bp, sname, &ns);
  lives_free(sname);
  for (i = 0; i < ns; i++) {
    stdef = sdefs[i];
    if (!stdef_is_value(stdef) || stdef_is_optional(stdef)) continue;
    if (!(idx--)) break;
  }
  if (sdefs) lives_free(sdefs);
  return stdef;
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
    const char *err_item = NULL;
    uint64_t stflags;
    uint32_t vtype;

    stdef = get_strand_def_by_name(blueprint, "uid");
    if (stdef) {
      set_strand_val(bundle, "uid", STRAND_TYPE_UINT64, 1, 0, gen_unique_id());
    }

    stdef = get_strand_def_by_name(blueprint, "blueprint");
    if (stdef) {
      set_strand_val(bundle, "blueprint", STRAND_TYPE_CONST_BUNDLEPTR, 1, 0, blueprint);
    }

    if (!err) {
      // step 1, go through params
      if (vargs) {
        while (1) {
          // caller should provide vargs, list of optional items to be initialized
          stname = va_arg(vargs, char *);
          if (!stname) break; // done parsing params
          //g_print("2WILL add %s\n",stname);
          //g_print("ptr stname is %p\n", stname);
          stdef = get_strand_def_by_name(blueprint, stname);
          if (!stdef) {
            err_item = stname;
            err = TRUE;
            break;
          }
          vtype = stdef_get_strand_type(stdef, bundle);
          if (vtype == STRAND_TYPE_NONE || vtype == STRAND_TYPE_UNDEFINED) {
            err_item = stname;
            err = TRUE;
            break;
          }
          set_strand_val(bundle, stname, vtype, 1, 0, vargs);
        }
        if (err) {
          g_printerr("2ERROR adding item '%s' to bundle, item name or type invalid\n", err_item);
          err = FALSE;
        }
      }
      if (!err) {
        // add any missing mandos
        // TODO -check for EXCLUSIVE
        for (int i = 0; (stdef = get_mand_strand_def(blueprint, i)); i++) {
          uint32_t sttype = stdef_get_strand_type(stdef, blueprint);
          stflags = stdef_get_flags(stdef);
          //if (sttype == STRAND_TYPE_NONE) continue;
          stname = stdef_get_name(stdef);
          //g_print("DO WE need defs for %s ?\n", stname);
          if (!bundle_has_strand(bundle, stname)) {
            if ((stflags & BLUEPRINT_FLAG_READONLY)
                && !(stflags & BLUEPRINT_FLAG_OPTIONAL)) {
              err_item = stname;
              err = TRUE;
              break;
            }
            if (sttype != STRAND_TYPE_NONE && sttype != STRAND_TYPE_UNDEFINED) {
              //g_print("%s is mando\n", shname);
              //g_print("%s is absent\n", shname);
              if (bundle_has_strand(stdef, "default"))
                lives_strand_copy(bundle, stname, stdef, "default");
            }
          }
        }
        if (err) {
          g_printerr("3ERROR adding item '%s' to bundle, "
                     "readonly item must have value set in vargs\n", err_item);
        }
      }
    }
    if (err) {
      g_printerr("Could not create bundle\n");
#if DEBUG_BUNDLES
      g_printerr("\nERROR: adding item to bundle\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      // reverse the leaves, since they are prepended
      bundle = weed_plant_copy(bundle);
    }
  }
  return bundle;
}


/////////////// common funcs ///

bundle_t *create_bundle_by_type(bundle_type bt, ...) {
  bundle_t *bun = NULL;
  if (bt >= 0 && bt < n_builtin_bundledefs) {
    va_list vargs;
    const blueprint_t *bp = blueprints[bt];
    va_start(vargs, bt);
    if (bp) bun = create_gen2_bundle_with_vargs((uint64_t)bt, (blueprint_t *)bp, vargs);
    else {
      bundledef_t bdef = get_bundledef(bt);
      if (bdef) bun = create_gen1_bundle_with_vargs((uint64_t)bt, bdef, vargs);
    }
    va_end(vargs);
  }
  return bun;
}


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


uint32_t attr_type_to_weed_seed(uint32_t st) {
  switch (st) {
  case ATTR_TYPE_INT: return WEED_SEED_INT;
  case ATTR_TYPE_DOUBLE: return WEED_SEED_DOUBLE;
  case ATTR_TYPE_BOOLEAN: return WEED_SEED_BOOLEAN;
  case ATTR_TYPE_STRING: return WEED_SEED_STRING;
  case ATTR_TYPE_INT64: return WEED_SEED_INT64;
  case ATTR_TYPE_UINT: return WEED_SEED_UINT;
  case ATTR_TYPE_UINT64: return WEED_SEED_UINT64;
  case ATTR_TYPE_FLOAT: return WEED_SEED_FLOAT;
  case ATTR_TYPE_VOIDPTR: return WEED_SEED_VOIDPTR;
  case ATTR_TYPE_FUNCPTR: return WEED_SEED_FUNCPTR;
  case ATTR_TYPE_BUNDLEPTR: return WEED_SEED_PLANTPTR;
  //case ATTR_TYPE_CONST_BUNDLEPTR: return WEED_SEED_VOIDPTR;
  default: return WEED_SEED_INVALID;
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
  default: return STRAND_TYPE_NONE;
  }
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
  default: return WEED_SEED_INVALID;
  }
}


// should not be called directly, only through set_strand_val, set_strand_value
// or array_append
static boolean set_strand_val_varg(bundle_t *bundle, const char *iname, uint32_t vtype,
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
    if (!array_op) {
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
        } else {
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
        } else {
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
        } else {
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
        } else {
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
        if (array_op == 2)
          weed_ext_append_elements(bundle, xiname, stype, ne, val);
        else
          weed_set_string_array(bundle, xiname, ne, val);
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
        } else {
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
        } else {
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


int32_t lives_strand_array_append(bundle_t *bundle, const char *name,
                                  uint32_t vtype, uint32_t ne, ...) {
  va_list va;
  int op = 2;
  va_start(va, ne);
  set_strand_val_varg(bundle, name, vtype, ne, op, va);
  va_end(va);
  return weed_leaf_num_elements(bundle, name);
}


/* uint32_t lives_strand_array_append(bundle_t *bundle, const char *stname, */
/* 				   uint32_t vtype, uint32_t ne, va_list va) { */
/*   if (bundle) { */
/*     char *sname = get_short_name(stname); */
/*     weed_size_t ne; */
/*     set_strand_val_varg(bundle, sname, vtype, ne, 2, va); */
/*     ne = weed_leaf_num_elements(bundle, sname); */
/*     lives_free(sname); */
/*     return ne; */
/*   } */
/*   return 0; */
/* } */


void lives_strand_array_clear(bundle_t *bundle, const char *stname) {
  if (bundle && weed_plant_has_leaf(bundle, stname)) {
    char *sname = get_short_name(stname);
    uint32_t stype = weed_leaf_seed_type(bundle, sname);
    weed_leaf_set(bundle, sname, stype, 0, NULL);
    lives_free(sname);
  }
}


uint32_t lives_array_get_size(bundle_t *bundle, const char *stname) {
  char *sname = get_short_name(stname);
  weed_size_t ne = weed_leaf_num_elements(bundle, sname);
  lives_free(sname);
  return ne;
}


// set_strand_value_bundleptr should only be used to set the DATA in a value bundle
// (or bundles which extend value, such as strand_replacement, attribute)
// everything else must use include_sub_bundle(s) or add/append const bundleptr(s)

int lives_strand_append_sub_bundles(bundle_t *bundle, const char *name, int ns, bundle_t **subs) {
  if (bundle) {
    set_strand_val(bundle, name, STRAND_TYPE_BUNDLEPTR, ns, 1, subs);
    for (int i = 0; i < ns; i++) {
      bundle_t *sub = subs[i];
      set_strand_val(sub, "container", STRAND_TYPE_VOIDPTR, 1, 0, bundle);
      set_strand_val(sub, "container_strand", STRAND_TYPE_STRING, 1, 0, name);
    }
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
}


int lives_strand_include_sub_bundle(bundle_t *bundle, const char *name, bundle_t *sub) {
  if (bundle) {
    set_strand_val(bundle, name, STRAND_TYPE_BUNDLEPTR, 1, 0, sub);
    set_strand_val(sub, "container", STRAND_TYPE_VOIDPTR, 1, 0, bundle);
    set_strand_val(sub, "container_strand", STRAND_TYPE_STRING, 1, 0, name);
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
}



int lives_strand_add_const_bundles(bundle_t *bundle, const char *name, int ns, bundle_t **ptrs) {
  // if the data to be added does not have a container, we will add it via include_sub_bundles
  // this is possible since the strand_type for strand_replacement may vary from the strand_def
  // doing it like this ensures that a bundle is always included as a sub bundle somewhere so that
  // it will get unreffed with the container, and not leaked

  if (bundle) {
    set_strand_val(bundle, name, STRAND_TYPE_VOIDPTR, ns, 2, ptrs);
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
}


int lives_strand_add_const_bundle(bundle_t *bundle, const char *name, bundle_t *ptr) {
  if (bundle) {
    set_strand_val(bundle, name, STRAND_TYPE_VOIDPTR, 1, 0, (void *)ptr);
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
}


// list leaves (strands) in bun and filter out any that do not start with a '.'
char **lives_bundle_list_strands(bundle_t *bun) {
  char **leaves;
  weed_size_t nleaves;
  int j, filled = 0;
  leaves = weed_plant_list_leaves(bun, &nleaves);
  for (int i = 0; i < nleaves; i++) {
    if (leaves[i][0] != '.') {
      lives_free(leaves[i]);
      j = i + 1;
      while (leaves[j]) {
        if (leaves[j][0] == '.') {
          leaves[filled++] = leaves[j];
          i = j;
          break;
        }
        lives_free(leaves[j]);
      }
    } else filled++;
  }
  leaves[filled] = NULL;
  return leaves;
}


// TODO - macro should unref first
/* static void lives_bundle_free(bundle_t *bundle) { */
/*   if (bundle) { */
/*     weed_size_t xnvals = 0; */
/*     int nvals = 0; */
/*     char **leaves = weed_plant_list_leaves(bundle, &xnvals); */
/*     int i = 0; */
/*     for (char *leaf = leaves[0]; leaf; leaf = leaves[++i]) { */
/*       if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_PLANTPTR) { */
/* 	bundle_t **sub = weed_get_plantptr_array_counted(bundle, leaf, &nvals); */
/* 	for (int k = 0; k < nvals; k++) { */
/* 	  if (sub[k] && sub[k] != bundle) { */
/* 	    lives_bundle_free(sub[k]); */
/* 	  } */
/* 	} */
/* 	weed_leaf_delete(bundle, leaf); */
/*       } else if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_VOIDPTR) { */
/* 	if (weed_leaf_element_size(bundle, leaf, 0) > 0) */
/* 	  lives_free(weed_get_voidptr_value(bundle, leaf, NULL)); */
/*       } */
/*       lives_free(leaf); */
/*     } */
/*     if (xnvals) lives_free(leaves); */
/*     weed_plant_free(bundle); */
/*   } */
/* } */


void lives_bundle_free(bundle_t *bun) {if (bun) weed_plant_free(bun);}

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
  //scriptlet_t *scriptlet = NULL;
  boolean err = FALSE;
  if (btype >= 0 && btype < n_builtin_bundledefs) {
    bundledef_t bdef;
    bundle_strand strand, strand2;
    char *vname;
    uint64_t vflags;
    uint32_t vtype;
    uint64_t bflags;
    int tflags;
    int i;
    // we will pass through th bdef and create strand_def for each entry or directive
    // we will set name, flags and for values we will set type and default
    // comments are ignored
    // readonly sets the readonly flag bit, and we skip adding a default
    // For arrays we set ARRAY_SIZE to -1
    // automations are only added later in gen3
    g_print("\n**************************MAKE BLUEPRINT FOR %d\n", btype);
    bluep = create_bundle_by_type(BLUEPRINT_BUNDLE_TYPE, "bundle_type", btype, NULL);
    if ((bp = blueprints[btype])) {
      // if we already have a blueprint we will recreate it
      int ns, i;
      char *sname = get_short_name("strand_defs");
      strand_def_t **sds = weed_get_plantptr_array_counted((weed_plant_t *)bp, sname, &ns), *stdef = NULL;
      bundle_t *stdef_old;
      lives_free(sname);
      for (i = 0; i < ns; i++) {
        stdef_old = sds[i];
        vtype = stdef_get_strand_type(stdef_old, NULL);
        vname = (char *)stdef_get_name(stdef_old);
        bflags = stdef_get_flags(stdef_old);
        stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "name", vname,
                                      "flags", bflags, "strand_type", vtype, NULL);
        lives_free(vname);
        // copy default
        if (bundle_has_strand(stdef_old, "default"))
          lives_strand_copy(stdef, "default", stdef_old, "default");
        st_defarr = lives_realloc(st_defarr, ++nstr * sizeof(bundle_t *));
        st_defarr[nstr - 1] = stdef;
      }
      if (sds) lives_free(sds);
    } else {
      bdef = (bundledef_t)get_bundledef(btype);
      for (i = 0; bdef[i]; i++) {
        off_t offx = 0;
        bflags = 0;
        strand = bdef[i];
        //g_print("STRAND ------------ %s\n", strand);
        vflags = get_vflags(strand, &offx, NULL, bdef);
        if (vflags == STRAND_TYPE_FLAG_COMMENT) continue;
        /* if (vflags == STRAND_TYPE_FLAG_DIRECTIVE) { */
        /*   // any directives are simply copied verbatime */
        /*   // when creating bundles in gen1, directives are simply skipped over */
        /*   // for gen2 directive strands are ignored */
        /*   // only in gen3 will these be recast as hook automations etc. */
        /*   // */
        /*   // here we flag them, set type to directive, and set name to @nnnnn */
        /*   // where nnnn = 0... unique for blueprint */
        /*   // the directive itself is stored in default, with implied strand_type STRAND_TYPE_STIRNG */
        /*   bflags = BLUEPRINT_FLAG_DIRECTIVE; */
        /*   vtype = STRAND_TYPE_FLAG_DIRECTIVE; */
        /*   vname = lives_strdup_printf("%c%d", STRAND_TYPE_FLAG_DIRECTIVE); */
        /*   stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "name", vname, */
        /* 				"flags", bflags, "strand_type", vtype, NULL); */
        /*   set_def_value_gen1(stdef, shdef, strand, NULL); */
        //}
        if (1) {
          int array_size = 0;
          //int restrnum = 0;
          if (vflags == STRAND_TYPE_FLAG_OPTIONAL) bflags |= BLUEPRINT_FLAG_OPTIONAL;
          vtype = get_vtype(strand, &offx);
          vname = (char *)get_vname(strand + offx);
          strand2 = bdef[++i];
          tflags = atoi(strand2);
          // for arrays we set array_size to -1, for scalars it is not created
          if (tflags & STRAND2_FLAG_ARRAY) array_size = -1;

          // for readonly, we do not set any default, the value must be specified when the
          // bundle is created, then cannot be changed
          // gen3 will add automation for this, for now we just treat it as
          // mandatory in vargs
          if (tflags & STRAND2_FLAG_READONLY) bflags |= BLUEPRINT_FLAG_READONLY;
          if (tflags & STRAND2_FLAG_RDONLY_SUB) bflags |= BLUEPRINT_FLAG_RDONLY_SUB;
          if (tflags & STRAND2_FLAG_KEYED) bflags |= BLUEPRINT_FLAG_KEYED_ARRAY;

          if (tflags & STRAND2_FLAG_TEMPLATE) {
            bflags |= BLUEPRINT_FLAG_READONLY | BLUEPRINT_FLAG_OPTIONAL;
            stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "flags", bflags, "strand_type", vtype, !array_size ? NULL :
                                          "max_size", array_size, NULL);
            NIRVA_VALUE_SET(bluep, "multi", STRAND_TYPE_BUNDLEPTR, stdef);
            continue;

          }
          stdef = create_bundle_by_type(STRAND_DEF_BUNDLE_TYPE, "name", vname,
                                        "flags", bflags, "strand_type", vtype, !array_size ? NULL :
                                        "max_size", array_size, NULL);

          if (vtype == STRAND_TYPE_PROXIED) {
            NIRVA_VALUE_SET(stdef, "type_proxy", STRAND_TYPE_STRING, "strand_type");
            NIRVA_VALUE_SET(stdef, "restrict_proxy", STRAND_TYPE_STRING, "restrictions");
          }

          else if (vtype == STRAND_TYPE_BUNDLEPTR || vtype == STRAND_TYPE_CONST_BUNDLEPTR) {
            int restrnum = get_restrnum(strand + offx + strlen(vname) + 1);
            NIRVA_VALUE_SET(stdef, "restrictions", STRAND_TYPE_STRING, nirva_restrictions[restrnum]);
          }

          if (vtype != STRAND_TYPE_NONE && vtype != STRAND_TYPE_UNDEFINED && !(bflags & BLUEPRINT_FLAG_READONLY)) {
            err = set_def_value_gen1(stdef, "default", strand, strand2);
            if (err) goto endit2;
          }
        }
        st_defarr = lives_realloc(st_defarr, ++nstr * sizeof(bundle_t *));
        st_defarr[nstr - 1] = stdef;
      }
    }

    if (nstr > 0) {
      // because strand_defs in blueprint is a keyed_array, we need to add keys for each strand_def "name"
      if (bluep) {
        for (i = 0; i < nstr; i++) {
          char *stname = NIRVA_GET_VALUE_STRING(st_defarr[i], "name");
          NIRVA_ADD_VALUE_BY_KEY(bluep, "strand_defs", stname, STRAND_TYPE_BUNDLEPTR, st_defarr[i]);
          lives_free(stname);
        }
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
  const char *tp;

  if (tname) hdr  = lives_strdup("typedef struct {");
  else hdr = lives_strdup("struct {");

  bp = get_blueprint_from_bundle(bundle);
  if (bp) {
    int ns;
    char *comment = NULL;
    char *sname = get_short_name("strand_defs");
    strand_def_t **sds = weed_get_plantptr_array_counted(bp, "sname", &ns);
    boolean is_arr;
    lives_free(sname);
    for (int i = 0; i < ns; i++) {
      char *stname;
      uint32_t st, sttype;
      strand_def_t *stdef = sds[i];
      if (!stdef_is_value(stdef)) continue;
      sttype = stdef_get_strand_type(stdef, NULL);
      stname = stdef_get_name(stdef);

      //g_print("STRAND is %s\n", strand);

      is_arr = stdef_is_array(stdef);
      if (sttype == STRAND_TYPE_CONST_BUNDLEPTR) {
        tp = "const bundle_t **";
        if (is_arr) ar = lives_strdup("[]");
      } else {
        st = strand_type_to_weed_seed(sttype);
        tp = weed_seed_to_ctype(st, TRUE);
        if (sttype == WEED_SEED_PLANTPTR) tp = "bundle_t *";
        if (is_arr) ar = lives_strdup("[]");
      }
      if (stdef_is_optional(stdef)) comment = lives_strdup("// optional");
      line = lives_strdup_printf("\n  %s%s%s;\t%s", tp, stname, ar ? ar : "",
                                 comment);
      lives_freep((void **)&comment);
      hdr = lives_concat(hdr, line);
      if (ar) {
        lives_free(ar);
        ar = NULL;
      }
      lives_free(stname);
    }
  }
  if (!tname) line = lives_strdup("\n}");
  else line = lives_strdup_printf("\n} %s;", tname);
  hdr = lives_concat(hdr, line);
  return hdr;
}


char *nirvascope_bundle_to_header(bundle_t *bundle, const char *tname, int idx) {
  //blueprint_t *bp;
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
  } else {
    hdr = lives_strdup_printf("    [%d]", idx - 1);
    tabs = "\t";
  }
  for (int i = 0; leaves[i]; i++) {
    int offs = 0;
    if (*leaves[i] == '.') offs = 1;
    if (1) {
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
        case WEED_SEED_CONST_CHARPTR: {
          const char *vs = weed_get_custom_value(bundle, leaves[i], st, NULL);
          val = lives_strdup_printf("\t (%s)", vs);
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
      xline = lives_strdup_printf("\n  %s%s%s%s;", tabs, tp, leaves[i] + offs,
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
    if (!tname || !*tname) line = lives_strdup("\n}");
    else line = lives_strdup_printf("\n} %s;", tname);
  }
  hdr = lives_concat(hdr, line);
  return hdr;
}

#define CONT_PFX "CONT"
bundle_t *find_array_item_by_key(bundle_t *con, const char *kname) {
  // array mut be a keyed_array. So all we do is open up the sub bundle and find
  // a sub sub bundle called "index" The items will be held in index with a prefixed name.
  // The (const bundlptr) entries point to elements in the array
  if (con) {
    lives_bundle_t *index = lives_strand_get_value_bundleptr(con, "index");
    if (index) {
      char *xkname = lives_strdup_printf("%s%s", CONT_PFX, kname);
      lives_bundle_t *tagr = lives_strand_get_value_const_bundleptr(index, xkname);
      lives_free(xkname);
      return tagr;
    }
  }
  return NULL;
}


void add_array_item_by_key(bundle_t *con, const char *kname, bundle_t *b) {
  // array must be a keyed_array. So all we do is open up the container and find
  // a sub bundle called "index" The items will be held in index with a prefixed name.
  // The (const bundlptr) entries point to elements in the array
  if (con) {
    lives_bundle_t *index = lives_strand_get_value_bundleptr(con, "index");
    if (index) {
      char *xkname = lives_strdup_printf("%s%s", CONT_PFX, kname);
      set_strand_val(index, xkname, STRAND_TYPE_CONST_BUNDLEPTR, 1, 0, b);
      lives_free(xkname);
    }
  }
}


void remove_array_item_by_key(bundle_t *con, const char *kname) {
  // array mut be a keyed_array. So all we do is open up the container and find
  // a sub bundle called "index" The items will be held in index with a prefixed name.
  // The (const bundlptr) entries point to elements in the array
  if (con) {
    lives_bundle_t *index = lives_strand_get_value_bundleptr(con, "index");
    if (index) {
      char *xkname = lives_strdup_printf("%s%s", CONT_PFX, kname);
      lives_strand_delete(index, xkname);
      lives_free(xkname);
    }
  }
}


uint32_t get_attr_type(lives_attr_t *attr) {
  if (attr) return ATTR_TYPE_NONE;
  return lives_strand_get_value_uint32(attr, "attr_type");
}

uint32_t lives_get_attr_type(lives_attr_t *attr) {
  return attr_type_to_weed_seed(get_attr_type(attr));
}


int lives_attr_get_value_int(lives_attr_t *attr) {
  if (attr) return 0;
  return lives_strand_get_value_int(attr, "value");
}

int64_t lives_attr_get_value_int64(lives_attr_t *attr) {
  if (!attr) return 0;
  return lives_strand_get_value_int64(attr, "value");
}
bundle_t *lives_attr_get_value_bundleptr(lives_attr_t *attr) {
  if (!attr) return NULL;
  return lives_strand_get_value_bundleptr(attr, "value");
}

//

bundle_t *lives_strand_get_value_bundleptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  weed_error_t err;
  weed_plant_t *val = weed_get_plantptr_value(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (weed_plant_t *)weed_get_voidptr_value(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t *)val;
}

bundle_t *lives_strand_get_value_const_bundleptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  weed_error_t err;
  void *val = weed_get_voidptr_value(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (void *)weed_get_plantptr_value(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t *)val;
}

void *lives_strand_get_value_voidptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  void *val = weed_get_voidptr_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

int lives_strand_get_value_int(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  int val = weed_get_int_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

uint32_t lives_strand_get_value_uint32(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  uint32_t val;
  weed_error_t err;
  val = weed_get_uint_value(bundle, sname, &err);
  lives_free(sname);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (uint32_t)lives_strand_get_value_int(bundle, strand_name);
  return val;
}

int64_t lives_strand_get_value_int64(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  int64_t val = weed_get_int64_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

uint64_t lives_strand_get_value_uint64(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  weed_error_t err;
  uint64_t val = weed_get_uint64_value(bundle, sname, &err);
  lives_free(sname);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    val = (uint64_t)lives_strand_get_value_int64(bundle, strand_name);
  return val;
}

char *lives_strand_get_value_string(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  char *val = weed_get_string_value(bundle, sname, NULL);
  lives_free(sname);
  return val;
}

bundle_t **lives_strand_get_array_bundleptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  weed_error_t err;
  weed_plant_t **vals = weed_get_plantptr_array(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE)
    vals = (weed_plant_t **)weed_get_voidptr_array(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t **)vals;
}


bundle_t **lives_strand_get_array_const_bundleptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  weed_error_t err;
  weed_plant_t **vals = (weed_plant_t **)weed_get_voidptr_array(bundle, sname, &err);
  if (err == WEED_ERROR_WRONG_SEED_TYPE) vals = weed_get_plantptr_array(bundle, sname, NULL);
  lives_free(sname);
  return (bundle_t **)vals;
}

char **lives_strand_get_array_string(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  char **vals = weed_get_string_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

int *lives_strand_get_array_int(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  int *vals = weed_get_int_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

uint32_t *lives_strand_get_array_uint(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  uint *vals = weed_get_uint_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

int64_t *lives_strand_get_array_int64(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  int64_t *vals = weed_get_int64_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

uint64_t *lives_strand_get_array_uint64(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  uint64_t *vals = weed_get_uint64_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

boolean *lives_strand_get_array_boolean(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  boolean *vals = weed_get_boolean_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

double *lives_strand_get_array_double(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  double *vals = weed_get_double_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

/* float *lives_strand_get_array_float(bundle_t *bundle, const char *strand_name) { */
/*   char *sname = get_short_name(strand_name); */
/*   float *vals = weed_get_float_array(bundle, sname, NULL); */
/*   lives_free(sname); */
/*   return vals; */
/* } */

void **lives_strand_get_array_voidptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  void **vals = weed_get_voidptr_array(bundle, sname, NULL);
  lives_free(sname);
  return vals;
}

lives_funcptr_t *lives_strand_get_array_funcptr(bundle_t *bundle, const char *strand_name) {
  char *sname = get_short_name(strand_name);
  lives_funcptr_t *vals = weed_get_funcptr_array(bundle, sname, NULL);
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

