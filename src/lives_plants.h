// lives_plants.h
// LiVES
// (c) G. Finch 2019 - 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// specialised weed_plants for use in LiVES
//
// LIVES_PLANT_BLUEPRINT
// plant which contains construction details for other lives plants
//

// LIVES_PLANT_INDEX
// this is a specialised plant which can hold various items of data
// it has a const char *prefix and an int item_type
// data to be stored in the index must be all of type item_tyoe
// prefix is prepended to the item name to distinguish data items from internal leaves

#ifndef _HAVE_LIVES_PLANTS_H
#define _HAVE_LIVES_PLANTS_H

// weed plants with type >= 16384 are reserved for custom use, so let's take advantage of that
#define WEED_PLANT_LIVES 31337

#define IS_LIVES_PLANT_TYPE(type) ((type) == WEED_PLANT_LIVES)
#define IS_LIVES_PLANT(plant) (plant && IS_LIVES_PLANT_TYPE(weed_plant_get_type(plant)))
#define IS_PROC_THREAD(type, subtype) (IS_LIVES_PLANT_TYPE(type) && (subtype) == LIVES_PLANT_PROC_THREAD)

#define LIVES_PLANT_MESSAGE 1
#define LIVES_PLANT_WIDGET 2
#define LIVES_PLANT_TUNABLE 3
#define LIVES_PLANT_PROC_THREAD 4
#define LIVES_PLANT_PREFERENCE 5

#define LIVES_PLANT_BLUEPRINT 32
#define LIVES_PLANT_VALUE_DEF 33

#define LIVES_PLANT_TMP 64

#define LIVES_PLANT_INDEX 128
#define LIVES_PLANT_DATA_BOOK 129
#define LIVES_PLANT_LOOKUP 130

#define LIVES_PLANT_BAG_OF_HOLDING 256 // generic - cant think of a better name right now

#define LIVES_PLANT_FUNCPARAMS 512

#define LIVES_PLANT_HASH_STORE 513
#define LIVES_PLANT_CLEANER 515
#define LIVES_PLANT_STRUCT_MIRROR 516

// used for debugging purposes
#define LIVES_PLANT_AUDIT 1024

#define STRUCT_ADAPTOR 999

#if 0
// TODO: WEED_PLANT_LIVES_EXT
///  will have a special feature,
// lives_ext_set, lives_ext_get, lives_ext_delete
// will be called just before weed_leaf_*
// these functions will be wrappers around the standard funcs
//  since getting the type of a plant is almost instantanous
//  and ublockable, this should add very little overhead

#define WEED_PLANT_LIVES_EXT 31338
#endif

weed_plant_t *lives_plant_new(int64_t subtype);
weed_plant_t *lives_plant_new_with_serialno(int64_t subtype, int64_t serialno);
weed_plant_t *lives_plant_new_with_refcount(int64_t subtype);

weed_plant_t *lives_plant_new_empty(void);

int64_t lives_plant_get_subtype(weed_plant_t *);

// LIVES_PLANT_BLUEPRINT

#define BLU_FLAGS_NONE 0
#define BLU_FLAG_READWRITE	(1ull << 1)
#define BLU_FLAG_AUTODELETE	(1ull << 2)

// check if !exist, is optional ?
#define BLU_FLAG_OPTIONAL	(1ull << 3)
#define BLU_FLAG_OPTREM		(1ull << 4)

// not a "real flagbit", just sets maxelems to -1 instead of 0
#define BLU_FLAG_ARRAY		(1ull << 31)

// add undel host, simlar to  rdonlyhost; applies unless..
#define BLU_FLAGS_REMOVABLE	(BLU_FLAG_OPTIONAL | BLU_FLAG_OPTREM)

// only used for bootstrapping
typedef struct {
  const char *name;
  weed_seed_t type;
  // (optional, can defne subtype if item type is plantptr)
  uint64_t pl_subtype;
  uint64_t flags;
} bootstrap_valdef;

typedef struct {
  uint64_t pltype;
  LiVESList *valdefs;
} bootstrap_template;

///////////////////////

typedef weed_plant_t lives_blueprint_t;
typedef weed_plant_t val_def_t;

weed_plant_t *plant_from_blueprint(int pltype, ...);

#define LIVES_LEAF_BLUEPRINT_IDX "_blueprint_idx"
#define LIVES_LEAF_BLUEPRINT_PTR "_blueprint_ptr"
#define LIVES_LEAF_VALUE_DEFS "_value_defs"

void dump_blueprint(uint64_t pltype);

// given a "blueprint" for lives_plant_type bltype, we create all its leaves, setting the values from va_args
#define PLANT_FROM_BLUEPRINT(bltype, ...) plant_from_blueprint(LIVES_PLANT_##bltype, ADD_STD_LEAVES(NULL) __VA_OPT__(,) __VA_ARGS__, NULL)

#define LIVES_STD_LEAVES WEED_LEAF_UNIQUE_ID, WEED_SEED_UINT64, BLU_FLAGS_NONE, LIVES_LEAF_BLUEPRINT_PTR, WEED_SEED_VOIDPTR, BLU_FLAGS_NONE
#define ADD_STD_LEAVES(blptr) WEED_LEAF_UNIQUE_ID, gen_unique_id(), LIVES_LEAF_BLUEPRINT_PTR, blptr

// defines LIVES_VALUE_DEF plant - a plant which defines a leaf in a plant
#define LIVES_VALUE_DEF_BLUEPRINT					\
  LIVES_LEAF_BLUEPRINT_PTR, WEED_SEED_VOIDPTR, BLU_FLAGS_NONE, WEED_LEAF_NAME, LIVES_SEED_CONST_CHARPTR, \
    BLU_FLAGS_NONE, LIVES_LEAF_SEED_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE, \
    WEED_LEAF_FLAGS, WEED_SEED_UINT64, BLU_FLAGS_NONE

// defines LIVES_BLUEPRINT plant which is a blueprint for itself and other lives_plants
// leaves for target are held in an index keyed by name, so we can immediately find them from the leaf name
#define LIVES_BLUEPRINT_BLUEPRINT					\
  LIVES_STD_LEAVES, LIVES_LEAF_BLUEPRINT_IDX, WEED_SEED_UINT64, BLU_FLAGS_NONE, \
    INCLUDES_SUB(INDEX, LIVES_LEAF_VALUE_DEFS)

#define EXTENDS_PLANT(plant) "@EXTENDS", LIVES_##plant##_BLUEPRINT
#define INCLUDES_SUB(ptype, name) name, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_##ptype, BLU_FLAG_AUTODELETE
#define INCLUDES_SUB_ARRAY(ptype, name) name, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_##ptype, BLU_FLAG_ARRAY | BLU_FLAG_AUTODELETE
#define ADD_REF(ptype, name) name, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_##ptype, 0
#define ADD_REF_ARRAY(ptype, name) name, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_##ptype, BLU_FLAG_ARRAY

void _register_blueprint(uint64_t pltype, const char *regstr, ...);

void register_blueprints(void);

// LIVES_PLANT_INDEX
// if itemtype is plantptr, and keyval is defined, items will be indexed by keyval leaf, stringified
// if prefixed by #,
#define LIVES_INDEX_BLUEPRINT						\
  LIVES_STD_LEAVES, LIVES_LEAF_INDEX_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE, LIVES_LEAF_PREFIX, \
    LIVES_SEED_CONST_CHARPTR, BLU_FLAGS_NONE, LIVES_LEAF_ITEM_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE, \
    LIVES_LEAF_ADD_SCRIPT, LIVES_SEED_ALLVALUES, BLU_FLAG_OPTIONAL, \
    LIVES_LEAF_DEL_SCRIPT, LIVES_SEED_ALLVALUES, BLU_FLAG_OPTIONAL, \
    LIVES_LEAF_UPDATE_SCRIPT, LIVES_SEED_ALLVALUES, BLU_FLAG_OPTIONAL

typedef enum {
  idx_type_anon = -1,
  idx_type_values,
  idx_type_data_book,
  idx_type_prefs,
  idx_type_blueprints,
  lookup_type_funcs,
  idx_type_max,
} index_type;

typedef index_type lookup_type;

#define LIVES_LEAF_INDEX_TYPE "_index_type"
#define LIVES_LEAF_PREFIX "_prefix"
#define LIVES_LEAF_ITEM_TYPE "_data_type"
#define LIVES_LEAF_ADD_SCRIPT "_add_script"
#define LIVES_LEAF_DEL_SCRIPT "_del_script"
#define LIVES_LEAF_UPDATE_SCRIPT "_update_script"

typedef weed_plant_t lives_index_t;

#define IDX_PREFIX "data_"

#define LIVES_MAKE_INDEX(idxtype, itemtype)				\
  PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE, idxtype, LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, itemtype)

weed_plant_t *lives_index_new(index_type idxtype, const char *prefix, weed_seed_t itemtype);
index_type lives_index_get_idxtype(lives_index_t *);
char *lives_index_get_itemname(lives_index_t *, const char *key);
const char *lives_index_get_prefix(lives_index_t *);
weed_seed_t lives_index_get_itemtype(lives_index_t *);

lives_result_t lives_index_set_value(lives_index_t *, const char *key, weed_seed_t stype, ...);
weed_error_t lives_index_get_value(void *retloc, lives_index_t *, const char *key);

#define is_autofree(plant, key) (plant ? !!(weed_leaf_get_flags(plant, key) & LIVES_FLAG_FREE_ON_DELETE) : FALSE)

// TRUE if existed / erased
boolean lives_index_erase_value(lives_index_t *, const char *key);

// defined type
boolean lives_index_contains_item(lives_index_t *, const char *key);

// contained and an has value
boolean lives_index_has_value(lives_index_t *, const char *key);

weed_error_t lives_index_set_autofree(lives_index_t *, const char *key, boolean set);

// LIVES PLANT LOOKUP
// a lookup is an index wuth read only values, index vals are readonly, autofree. We define a free func for the index vals.
//
// if we pass an allvalues with name and value, if the name is already used
// the value is not stored. Otherwise the allvalues is set static (todo - refcount)
// we can also store bound vars - like for the global databook, make th bound bvalues readonly
// etc.

#define LIVES_LEAF_LOOKUP_TYPE LIVES_LEAF_INDEX_TYPE

#define LOOKUP_PREFIX "ref_"


typedef weed_plant_t lives_lookup_t;

lives_lookup_t *lives_make_lookup(lookup_type ltype);
allvalues_t *add_to_lookup(lookup_type ltype, weed_seed_t st, const char *name, ...);
allvalues_t *find_in_lookup(lookup_type ltype, const char *name);

// LIVES_PLANT_DATA_BOOK

// new style data book. A data book is an index plant with prefix "data_"
// and type LIVES_SEED_ALLVALUES (a custom seed_type)
//
// when setting data, we first check if the value exists.
// if so we get back allvalues_t *, and we check if the st type matches the data being set
//
// if the value does not exist we create the allvalues_t *, set seed_type
//
// then we set data.
// in this way we can easily include "bound" values in a data book. This means the value points to
// an actual variable, so when setting the value, we actually update the underlying variable, and
// when reading we get the value of the underlying var.
//
// - we can also set the type without setting data yet
// - Items can be made readonly (though any underlying variable can still change independently)
//
// - setting a value "static" makes in undeletable. When we clean the book, we actually call _weed_plant_free(),
// (the original version). This will delete all items not flagged as undeleteable, and only return WEED_SUCCESS if all leaves were freed.


#define LIVES_LEAF_SCOPE "dbook_scope"

#define LIVES_MAKE_DATA_BOOK \
  PLANT_FROM_BLUEPRINT(DATA_BOOK, LIVES_LEAF_INDEX_TYPE, idx_type_data_book, LIVES_LEAF_PREFIX, IDX_PREFIX, \
		       LIVES_LEAF_ITEM_TYPE, LIVES_SEED_ALLVALUES, LIVES_LEAF_SCOPE, 0)

typedef weed_plant_t lives_databook_t;

int lives_databook_get_nitems(lives_databook_t *, const char *item);

weed_seed_t lives_databook_get_datatype(lives_databook_t *, const char *item);
lives_result_t lives_databook_set_datatype(lives_databook_t *, const char *name, weed_seed_t itype);

lives_databook_t *lives_local_databook(void);

allvalues_t *get_local_book_item(const char *item);
allvalues_t *get_global_book_item(const char *item);
allvalues_t *get_databook_item(lives_databook_t *, const char *item);

//weed_seed_t get_book_datatype(lives_databook_t *book, const char *item) {;

lives_result_t lives_databook_bind_value(lives_databook_t *, const char *name, weed_seed_t itype, void *varptr);
lives_result_t lives_databook_set_value(lives_databook_t *, const char *name, weed_seed_t itype, ...);
lives_result_t lives_databook_set_value_va(lives_databook_t *, const char *name, weed_seed_t itype, va_list va);
lives_result_t lives_databook_set_array(lives_databook_t *, const char *name, weed_seed_t itype, int nvals, void *vals);
lives_result_t lives_databook_copy_value(void *retloc, lives_databook_t *book, const char *name);

lives_result_t lives_databook_get_value(void *retloc, lives_databook_t *book, const char *name);
lives_result_t lives_databook_get_array_by_ref(void *array, lives_databook_t *book, const char *name, int *ne);

lives_result_t lives_databook_erase_value(lives_databook_t *, const char *name);

lives_result_t lives_localbook_clean(void);

lives_result_t lives_databook_inject(lives_databook_t *, lives_databook_t *ctx_book);
lives_result_t lives_databook_emit(lives_databook_t *dbook, weed_plant_t *src_obj);
lives_result_t lives_databook_end_emmission(lives_databook_t *, weed_plant_t *src_obj);

// if name == NULL this refers to whole book
lives_result_t lives_databook_set_scope(lives_databook_t *, const char *name, int scope);
lives_result_t lives_databook_get_scope(lives_databook_t *, const char *name, int *pscope);

lives_result_t lives_databook_descend(lives_databook_t *);
lives_result_t lives_databook_ascend(lives_databook_t *);

lives_result_t lives_databook_pushdown_value(lives_databook_t *, const char *name, weed_seed_t itype, ...);
lives_result_t lives_databook_pushdown_array(lives_databook_t *, const char *name, weed_seed_t itype, int nvals, void *vals);

void show_databook_contents(lives_databook_t *);

#define LIVES_INDEX_FOREACH(idx, key, value, ...) _DW0			\
  (if (idx) {const char *_pfx = lives_index_get_prefix(idx);		\
    size_t _pfxlen = lives_strlen(_pfx);				\
    char **_leaves = weed_plant_list_leaves(idx, NULL);			\
    int _IDX_ = 0;							\
    for (int _i = 0; _leaves[_i]; _i++) {				\
      if (!lives_strncmp(_leaves[_i], _pfx, _pfxlen)) {		\
	key = _leaves[_i] + _pfxlen; lives_index_get_value(&value, idx, key);	\
	__VA_ARGS__} _ext_free(_leaves[_i]); _IDX_++;} _ext_free(_leaves);})

#define SET_BOOK_DATATYPE(book, name, itype) _DW0(lives_data_book_set_datatype((book), (name), (itype));)
#define GET_BOOK_DATATYPE(book, name) lives_databook_get_datatype((book), (name))

#define SET_BOOK_VALUE(book, type, name, val) _DW0(lives_databook_set_value((book), (name), (type), (val));)
#define SET_BOOK_VALUE_VA(book, type, name, valist) _DW0(lives_databook_set_value_va((book), (name), (type), (valist));)
#define SET_BOOK_ARRAY(book, type, name, nvals, valsptr) _DW0(lives_databook_set_array((book), (name), (type), (nvals), (valsptr));)

#define BIND_BOOK_VALUE(book, type, name, valptr) _DW0(lives_databook_bind_value((book), (name), (type), (valptr));)

#define COPY_BOOK_VALUE(retloc, book, name) lives_databook_copy_value(val, book, name)

#define GET_BOOK_VALUE(val, book, name) lives_databook_get_value(&val, book, name)
#define GET_BOOK_ARRAY(array, book, name, nvalsp) lives_databook_get_array_by_ref(array, book, name, nvalsp)

#define DEL_BOOK_VALUE(book, name) lives_databook_erase_value((book), (name))

// bootstrap operates like this - we have 2 types of things, templates and blueprints, and both can be used to construct lives_plants
// templates are only used during bootstrap and are fixed data structures. Blueprints are similar but these exist as mutable definitions
// -create index from template - useful to store things like blueprints in. We create a yemplate for index, and use it to create an index plant.
// We dont store that template but we are ging to use it We create a template for leaf_descripotor plant and store that in the index.
// We crete a template for blueprint plant and store it in the index. Now we user the templates to create a real blueprint for leaf_descripptor
// and a real blueprint for blueprint.
// Now we create again the blueprint for leaf_descriptor, using the bleuprints we have, then do the same for blueprint.
// now we have blueprints for leaf_descriptor and for blueprint, both made with elemnts created from proper blueprints.
// At this point we now have the possibilit of altering blueprints, so we can change the makeup of those two plants,
// the leaf descriptro and blueprint, should we so wish. Then with our elements made from bluieprints,
// we can now create soft blueprints for all other lives_plants including index.

#define REGISTER_BLUEPRINT(bltype) _register_blueprint(LIVES_PLANT_##bltype, NULL, LIVES_##bltype##_BLUEPRINT, NULL)

#define register_blueprint(bltype, ...) _register_blueprint(LIVES_PLANT_##bltype, #__VA_ARGS__, __VA_ARGS__,  NULL)

#define BOOTSTRAP_BLUEPRINTS						\
  REGISTER_BLUEPRINT(INDEX);						\
  REGISTER_BLUEPRINT(VALUE_DEF); 	REGISTER_BLUEPRINT(BLUEPRINT);	\
  REGISTER_BLUEPRINT(BLUEPRINT); 	REGISTER_BLUEPRINT(VALUE_DEF);	\
  REGISTER_BLUEPRINT(VALUE_DEF); 	REGISTER_BLUEPRINT(BLUEPRINT);	\
  REGISTER_BLUEPRINT(INDEX);

/* REGISTER_BLUEPRINT(INDEX);						\ */
/* REGISTER_BLUEPRINT(VALUE_DEF); 	REGISTER_BLUEPRINT(BLUEPRINT);  */

extern lives_index_t *indices[idx_type_max];

//// related, generic leaves

#define LIVES_LEAF_SERIAL_NUMBER "_serial_num"
#define LIVES_LEAF_SEED_TYPE "_seed_type"

#endif
