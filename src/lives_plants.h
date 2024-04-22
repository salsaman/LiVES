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

// blueprints

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

// given a "blueprint" for lives_plant_type bltype, we create all its leaves, setting the values from va_args
#define PLANT_FROM_BLUEPRINT(bltype, ...) plant_from_blueprint(LIVES_PLANT_##bltype, WEED_LEAF_UID, gen_unique_id(), __VA_ARGS__)

#define REGISTER_BLUEPRINT(bltype) register_blueprint(LIVES_PLANT_#bltype, LIVES_##bltype##_BLUEPRINT, NULL)

#define LIVES_STD_LEAVES WEED_LEAF_UID, WEED_SEED_UINT64, BLU_FLAG_CONST

// defines LIVES_LEAF_DEF plant - a plant which defines a leaf in a plant
#define LIVES_LEAF_DEF_BLUEPRINT					\
  WEED_PLANT_NAME, LIVES_SEED_CONST_CHARPTR, BLU_FLAG_CONST, WEED_LEAF_SEED_TYPE, WEED_SEED_INT, BLU_FLAG_CONST, \
    WEED_SEED_FLAGS, WEED_SEED_UINT64, BLU_FLAG_NONE

// defines LIVES_BLUEPRINT plant which is a blueprint for itself and other lives_plants
#define LIVES_BLUEPRINT_BLUEPRINT					\
  LIVES_STD_LEAVES, LIVES_LEAF_BLUEPRINT_IDX, WEED_SEED_UINT64, BLU_FLAG_CONST, LIVES_LEAF_LEAF_DEFS, WEED_SEED_PLANTPTR, BLU_FLAG_ARRAY

// LIVES_PLANT_INDEX

#define LIVES_INDEX_BLUEPRINT						\
  LIVES_STD_LEAVES, LIVES_LEAF_INDEX_TYPE, WEED_SEED_INT, BLU_FLAG_CONST, LIVES_LEAF_PREFIX, LIVES_SEED_CONST_STRING, BLU_FLAG_CONST, \
    LIVES_LEAF_ITEM_TYPE, WEED_SEED_INT, BLU_FLAG_CONST

typedef enum {
	      idx_type_anon,
	      idx_type_data_book,
} index_type;

#define LIVES_LEAF_INDEX_TYPE "_index_type"
#define LIVES_LEAF_PREFIX "_prefix"
#define LIVES_LEAF_ITEM_TYPE "_data_type"

typedef weed_plant_t lives_index_t;

weed_plant_t *lives_index_new(index_type idxtype, const char *prefix, weed_seed_t itemtype);
index_type lives_index_get_idxtype(lives_index_t *);
const char *lives_index_get_prefix(lives_index_t *);
weed_seed_t lives_index_get_itemtype(lives_index_t *);

weed_error_t lives_index_set_value(lives_index_t *, const char *key, ...);
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

#define DATA_BOOK_PREFIX "data_"
#define MAKE_DATA_BOOK lives_index_new(idx_type_data_book, DATA_BOOK_PREFIX, LIVES_SEED_ALLVALUES)

weed_seed_t lives_data_book_get_item_type(lives_databook_t *book, const char *item);
lives_result_t lives_data_book_set_item_type(lives_databook_t *book, const char *item, weed_seed_t itype);

#define SET_BOOK_DATATYPE(book, name, type) _DW0(lives_data_book_set_item_type((book), (item), (itype)))
#define GET_BOOK_DATATYPE(book, name) 

#define SET_BOOK_VALUE(book, type, name, val) _DW0(set_value(book, type, name, val);)
#define SET_BOOK_ARRAY(book, type, name, nvals, valsptr) _DW0(lives_index_set_array(book, type, name, nvals, valsptr);)
#define GET_BOOK_VALUE(book, type, name) weed_get_##type##_value(book, mk_data_namex(name), NULL)
#define GET_BOOK_ARRAY(book, type, name, nvals)	weed_get_##type##_array_counted(book, mk_data_namex(name), &nvals)

// must maintain this order: we register INDEX as a template, then create an index plant
// this gives us somewhere to store templates and blueprints
// then we create templates for leaf_def and blueprint. then recreate these as blueprints using the templates just created
// then finally we recreate them as blueprints using plants from blueprints
// all blueprints are now in malleable form including the blueprints for leaf_def and for blueprint itself
#define BOOTSTRAP_BLUEPRINTS REGISTER_BLUEPRINT(INDEX), REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT); \
  REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT); REGISTER_BLUEPRINT(LEAF_DEF); REGISTER_BLUEPRINT(BLUEPRINT);

#define REGISTER_ALL_BLUEPRINTS BOOTSTRAP_BLUEPRINTS REGISTER_BLUEPRINT(INDEX)

void register_blueprints(void);

#endif
