// lists.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _LISTS_H_
#define _LISTS_H_

typedef struct {
  pthread_mutex_t mutex;
  LiVESList *list;
} lives_sync_list_t;

lives_sync_list_t *lives_sync_list_new(void);
lives_sync_list_t *lives_sync_list_add(lives_sync_list_t *, void *data);
lives_sync_list_t *lives_sync_list_remove(lives_sync_list_t *, void *data, boolean do_free);
void lives_sync_list_free(lives_sync_list_t *);

LiVESList *array_to_string_list(const char **y, int offset, int len);
char *lives_list_to_string(LiVESList *list, const char *delim);

LiVESList *lives_list_locate_string(LiVESList *, const char *);

LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, const char *string) WARN_UNUSED;

LiVESList *lives_list_copy_strings(LiVESList *);

boolean string_lists_differ(LiVESList *, LiVESList *);
boolean lists_differ(LiVESList *, LiVESList *, boolean ordered);

LiVESList *lives_list_append_unique_str(LiVESList *, const char *add);
LiVESList *lives_list_append_unique(LiVESList *, livespointer add);

LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip);
int lives_list_strcmp_index(LiVESList *list, livesconstpointer data, boolean case_sensitive);

boolean int_array_contains_value(int *array, int num_elems, int value);
boolean int64_array_contains_value(int64_t *array, int num_elems, int64_t value);

boolean cmp_rows(uint64_t *r0, uint64_t *r1, int nr);

LiVESList *lives_list_sort_alpha(LiVESList *, boolean fwd);

void lives_list_free_data(LiVESList *);

#define lives_list_free_strings lives_list_free_data

void lives_list_free_all(LiVESList **);
void lives_slist_free_all(LiVESSList **);

#define DATAx(list, x) (list->data == (void *)(x))

#define FIND_BY_DATA(list, xdata) {for (LiVESList *qlist = (list); qlist; qlist = qlist->next) \
      if (DATAx(qlist, xdata)) return qlist;} return NULL;

#define DATA_FIELD(l, t, f, x) ((void *)(((t *)(l##->data))->##f) == (x))

#define FIND_BY_DATA_FIELD(list, struct_type, field, target) \
  do {for (LiVESList *qlist = list; qlist; qlist = qlist->next) 	\
	if (DATA_FIELD(qlist, struct_type, field, target)) \
	  {list = qlist; break;}; list = NULL;} while(0);

#define FIND_BY_DATA_2FIELD(list, struct_type, field1, field2, target)	\
  do {for (LiVESList *qlist = list; qlist; qlist = qlist->next) 	\
	if (DATA_FIELD(qlist, struct_type, field1, target) \
	    || DATA_FIELD(qlist, struct_type, field2, target))	\
	  {list = qlist; break;}; list = NULL;} while(0);

#define FIND_BY_DATA_FIELD(list, struct_type, field, target) \
  do {for (LiVESList *qlist = list; qlist; qlist = qlist->next) 	\
	if (DATA_FIELD(qlist, struct_type, field, target)) \
	  {list = qlist; break;}; list = NULL;} while(0);


// locate in list
LiVESList *lives_list_find_by_data(LiVESList *, livespointer data);

// detatch and free node
LiVESList *lives_list_remove_node(LiVESList *, LiVESList *node, boolean free_data);
LiVESList *lives_list_remove_data(LiVESList *, livespointer data, boolean free_data);

// detatch but do not free
LiVESList *lives_list_detatch_node(LiVESList *, LiVESList *node);
LiVESList *lives_list_detatch_data(LiVESList **, livespointer data);

// returns TRUE if found and removed
boolean lives_list_check_remove_data(LiVESList **list, livespointer data, boolean free_data);

LiVESList *idx_list_update(LiVESList *, int64_t idx, void *data);
LiVESList *idx_list_update(LiVESList *, int64_t idx, void *data);
LiVESList *idx_list_remove(LiVESList *, int idx, boolean free_data);
boolean idx_list_get_data(LiVESList *, int idx, void **val_locn);

///// hash stores

#ifndef LIVES_LEAF_ID
#define LIVES_LEAF_ID "identifier"
#endif

typedef weed_plant_t lives_hash_store_t;

typedef boolean(*lives_hash_match_f)(void *data, void *udata);
void *get_from_hash_store_cbfunc(lives_hash_store_t *, lives_hash_match_f matchfn, void *udata);

lives_hash_store_t *lives_hash_store_new(const char *id);
void *get_from_hash_store_i(lives_hash_store_t *, uint64_t ikey);
void *get_from_hash_store(lives_hash_store_t *, const char *key);
lives_hash_store_t *add_to_hash_store_i(lives_hash_store_t *, uint64_t key, void *data);
lives_hash_store_t *add_to_hash_store(lives_hash_store_t *, const char *key, void *data);
lives_hash_store_t *remove_from_hash_store_i(lives_hash_store_t *, uint64_t key);
lives_hash_store_t *remove_from_hash_store(lives_hash_store_t *store, const char *key);
const char *hash_key_from_leaf_name(const char *name);
#endif
