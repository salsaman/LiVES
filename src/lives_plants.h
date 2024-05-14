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

//

#define LIVES_PLANT_BLUEPRINT 32
#define LIVES_PLANT_LEAF_DEF 33

#define LIVES_PLANT_TMP 64

#define LIVES_PLANT_INDEX 128
#define LIVES_PLANT_DATA_BOOK 129

#define LIVES_PLANT_BAG_OF_HOLDING 256 // generic - cant think of a better name right now

#define LIVES_PLANT_FUNCPARAMS 512

#define LIVES_PLANT_HASH_STORE 513
#define LIVES_PLANT_CLEANER 515
#define LIVES_PLANT_STRUCT_MIRROR 516

// used for debugging purposes
#define LIVES_PLANT_AUDIT 1024

#define STRUCT_ADAPTOR 999

weed_plant_t *lives_plant_new(int64_t subtype);
weed_plant_t *lives_plant_new_with_serialno(int64_t subtype, int64_t serialno);
weed_plant_t *lives_plant_new_with_refcount(int64_t subtype);

int64_t lives_plant_get_subtype(weed_plant_t *);

// LIVES_PLANT_BLUEPRINT

#define BLU_FLAG_NONE 0
#define BLU_FLAG_ARRAY (1ull << 0)
#define BLU_FLAG_CONST (1ull << 1)

// only used for bootstrapping
typedef struct {
  const char *name;
  weed_seed_t type;
  uint64_t flags;
} bootstrap_leafdesc;

typedef struct {
  uint64_t pltype;
  LiVESList *leaves;
} bootstrap_template;

///////////////////////

typedef weed_plant_t lives_blueprint_t;
typedef weed_plant_t leaf_desc_t;

weed_plant_t *plant_from_blueprint(int pltype, ...);

#define LIVES_LEAF_BLUEPRINT_IDX "_blueprint_idx"
#define LIVES_LEAF_LEAF_DEFS "_leaf_defs"

// given a "blueprint" for lives_plant_type bltype, we create all its leaves, setting the values from va_args
#define PLANT_FROM_BLUEPRINT(bltype, ...) plant_from_blueprint(LIVES_PLANT_##bltype, ADD_STD_LEAVES __VA_OPT__(,) __VA_ARGS__)

#define REGISTER_BLUEPRINT(bltype) register_blueprint(LIVES_PLANT_##bltype, LIVES_##bltype##_BLUEPRINT, NULL)

#define LIVES_STD_LEAVES WEED_LEAF_UNIQUE_ID, WEED_SEED_UINT64, BLU_FLAG_CONST
#define ADD_STD_LEAVES WEED_LEAF_UNIQUE_ID, gen_unique_id()

// defines LIVES_LEAF_DEF plant - a plant which defines a leaf in a plant
#define LIVES_LEAF_DEF_BLUEPRINT					\
  WEED_LEAF_NAME, LIVES_SEED_CONST_CHARPTR, BLU_FLAG_CONST, LIVES_LEAF_SEED_TYPE, WEED_SEED_INT, BLU_FLAG_CONST, \
    WEED_LEAF_FLAGS, WEED_SEED_UINT64, BLU_FLAG_NONE

// defines LIVES_BLUEPRINT plant which is a blueprint for itself and other lives_plants
#define LIVES_BLUEPRINT_BLUEPRINT					\
  LIVES_STD_LEAVES, LIVES_LEAF_BLUEPRINT_IDX, WEED_SEED_UINT64, BLU_FLAG_CONST, LIVES_LEAF_LEAF_DEFS, WEED_SEED_PLANTPTR, BLU_FLAG_ARRAY

void register_blueprints(void);

// LIVES_PLANT_INDEX

//const char *def_accept_cond = "COND_SYM_SRC_ITEM_TYPE", "COND_EQUALS", "COND_ATTRIBUTE", "$TARGET_OBJECT", LIVES_LEAF_ITEM_TYPE

#define LIVES_INDEX_BLUEPRINT						\
  LIVES_STD_LEAVES, LIVES_LEAF_INDEX_TYPE, WEED_SEED_INT, BLU_FLAG_CONST, LIVES_LEAF_PREFIX, LIVES_SEED_CONST_CHARPTR, BLU_FLAG_CONST, \
    LIVES_LEAF_ITEM_TYPE, WEED_SEED_INT, BLU_FLAG_CONST

typedef enum {
  idx_type_anon = -1,
  idx_type_data_book = 0,
  idx_type_prefs,
  idx_type_blueprints,
  idx_type_max,
} index_type;

#define LIVES_LEAF_INDEX_TYPE "_index_type"
#define LIVES_LEAF_PREFIX "_prefix"
#define LIVES_LEAF_ITEM_TYPE "_data_type"

typedef weed_plant_t lives_index_t;

#define IDX_PREFIX "data_"

#define LIVES_MAKE_INDEX(idxtype, itemtype)				\
  PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE, idxtype, LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, itemtype)

weed_plant_t *lives_index_new(index_type idxtype, const char *prefix, weed_seed_t itemtype);
index_type lives_index_get_idxtype(lives_index_t *);
const char *lives_index_get_prefix(lives_index_t *);
weed_seed_t lives_index_get_itemtype(lives_index_t *);

lives_result_t lives_index_set_value(lives_index_t *, const char *key, weed_seed_t stype, ...);
weed_error_t lives_index_get_value(void *retloc, lives_index_t *, const char *key);

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

#define LIVES_MAKE_DATA_BOOK lives_index_new(idx_type_data_book, LIVES_SEED_ALLVALUES)

typedef weed_plant_t lives_databook_t;

int lives_databook_get_nitems(lives_databook_t *book, const char *item);

weed_seed_t lives_databook_get_datatype(lives_databook_t *book, const char *item);
lives_result_t lives_databook_set_datatype(lives_databook_t *book, const char *name, weed_seed_t itype);

allvalues_t *get_local_book_item(const char *item);
allvalues_t *get_global_book_item(const char *item);
allvalues_t *get_databook_item(lives_databook_t *book, const char *item);

//weed_seed_t get_book_datatype(lives_databook_t *book, const char *item) {;

lives_result_t lives_databook_bind_value(lives_databook_t *book, const char *name, weed_seed_t itype, void *varptr);
lives_result_t lives_databook_set_value(lives_databook_t *book, const char *name, weed_seed_t itype, ...);
lives_result_t lives_databook_set_array(lives_databook_t *book, const char *name, weed_seed_t itype, int nvals, void *vals);
lives_result_t lives_databook_get_value(void *retloc, lives_databook_t *book, const char *name);

#define SET_BOOK_DATATYPE(book, name, itype) _DW0(lives_data_book_set_datatype((book), (item), (itype));)
#define GET_BOOK_DATATYPE(book, name) lives_databook_get_datatype((book), (name))

#define SET_BOOK_VALUE(book, type, name, val) _DW0(lives_databook_set_value((book), (type), (name), (val));)
#define SET_BOOK_ARRAY(book, type, name, nvals, valsptr) _DW0(lives_databook_set_array((book), (type), (name), (nvals), (valsptr));)
#define BIND_BOOK_VALUE(book, type, name, valptr) _DW0(lives_databook_bind_value((book), (type), (name), (valptr));)

#define GET_BOOK_VALUE(retval, book, itype, name) lives_databook_get_value(&(retval), (book), (itype), (name));)
#define GET_BOOK_ARRAY(retval, book, name, itype, nvalsp) lives_databook_get_array(&(retval), (book), (name), (itype), (nvalsp))

// bootstrap operates like this - we haev 2 types of things, templates and blueprints, wnd both cn be used  to construct lives_plants
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


#define BOOTSTRAP_BLUEPRINTS						\
  REGISTER_BLUEPRINT(INDEX), REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT); \
  REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT);		\
  REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT);		\
  REGISTER_BLUEPRINT(INDEX);

#define REGISTER_ALL_BLUEPRINTS BOOTSTRAP_BLUEPRINTS REGISTER_BLUEPRINT(INDEX)

extern lives_index_t *indices[idx_type_max];

//// related, generic leaves

#define LIVES_LEAF_SERIAL_NUMBER "_serial_num"
#define LIVES_LEAF_SEED_TYPE "_seed_type"

#endif
