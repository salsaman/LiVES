// lists.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _LISTS_H_
#define _LISTS_H_

#define SYNCLIST_FLAG_LILO		(1 << 0)
#define SYNCLIST_FLAG_FREE_ON_EMPTY	(1 << 1)
#define SYNCLIST_FLAG_POP_HEAD		(1 << 2)

typedef struct {
  pthread_rwlock_t lock;
  LiVESList *list;
  LiVESList *last;
  uint32_t flags;
} lives_sync_list_t;

// versions beginning with underscore should be called if and only if synclist->mutex is locked

void lives_sync_list_set_lilo(lives_sync_list_t *, boolean lilo);
void lives_sync_list_set_pop_head(lives_sync_list_t *, boolean yes);
void lives_sync_list_free_on_empty(lives_sync_list_t *, boolean yes);
void lives_sync_list_wrlock(lives_sync_list_t *);
void lives_sync_list_unlock(lives_sync_list_t *);

lives_sync_list_t *lives_sync_list_push(lives_sync_list_t *, void *data);

LiVESList *_lives_sync_list_pop(lives_sync_list_t **);
void *lives_sync_list_pop(lives_sync_list_t **);

void *_lives_sync_list_find(lives_sync_list_t *, lives_condfunc_f cond_func);
void *lives_sync_list_find(lives_sync_list_t *, lives_condfunc_f cond_func);

void lives_sync_list_replace_data(lives_sync_list_t *, void *from, void *to);

lives_sync_list_t *_lives_sync_list_remove(lives_sync_list_t *, void *data, boolean do_free);
lives_sync_list_t *lives_sync_list_remove(lives_sync_list_t *, void *data, boolean do_free);

lives_sync_list_t *_lives_sync_list_clear(lives_sync_list_t *, boolean free_data);
lives_sync_list_t *lives_sync_list_clear(lives_sync_list_t *, boolean free_data);

lives_sync_list_t *_lives_sync_list_free(lives_sync_list_t *, boolean free_data);
lives_sync_list_t *lives_sync_list_free(lives_sync_list_t *, boolean free_data);

LiVESList *array_to_string_list(const char **y, int offset, int len);
char *lives_list_to_string(LiVESList *, const char *delim);

LiVESList *lives_list_locate_string(LiVESList *, const char *);

LiVESList *lives_list_move_to_first(LiVESList *, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, const char *string) WARN_UNUSED;

LiVESList *lives_list_copy_strings(LiVESList *);
LiVESList *lives_list_copy_reverse_strings(LiVESList *);

boolean string_lists_differ(LiVESList *, LiVESList *);
boolean lists_differ(LiVESList *, LiVESList *, boolean ordered);

LiVESList *lives_list_append_unique_str(LiVESList *, const char *add);
LiVESList *lives_list_append_unique(LiVESList *, livespointer add);

LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip);
int lives_list_strcmp_index(LiVESList *, livesconstpointer data, boolean case_sensitive);

boolean int_array_contains_value(int *array, int num_elems, int value);
boolean int64_array_contains_value(int64_t *array, int num_elems, int64_t value);

boolean cmp_rows(uint64_t *r0, uint64_t *r1, int nr);

LiVESList *lives_list_sort_alpha(LiVESList *, boolean fwd);

void lives_list_free_data(LiVESList *);

#define lives_list_free_strings lives_list_free_data

void lives_list_free_all(LiVESList **);

#define LIVES_CONST_LIST_FOREACH(const_list, list) for(LiVESList* list=(const_list);list;list=list->next)

#define LIVES_LIST_FOREACH(list) for(;list;list=list->next)

#define DATA_IS(list, x) (list->data==(void *)x)

#define FIND_BY_DATA(list, data) LIVES_LIST_FOREACH(list) if (DATA_IS(list, data)) break;

#define DATA_FIELD_IS(l,t,f,x) (((t *)(l->data))->f==x)

#define FIND_BY_DATA_FIELD(list, struct_type, field, target) LIVES_LIST_FOREACH(list)	\
    if (DATA_FIELD_IS(list,structtype,field,target)) break;

/* #define FIND_BY_DATA2_FIELD(list, struct_type, field, target) LIVES_LIST_FOREACH(list)	\ */
/*     if (DATA_FIELD_IS(qlist,struct_type,field,target)||DATA_FIELD_IS(qlist,struct_type,field2,target)) break; */

// each data element is passed to a callbackj function, if the function returns TRUE then the
//callback should be of the form: lives_result_t (*cond_func)(void *);
#define FIND_BY_CALLBACK(list, callback) LIVES_LIST_FOREACH(list)	\
    if (LIVES_RESULT_SUCCESS==(*(callback))(list->data)) break;

#define LIVES_LIST_CHECK_CONTAINS(list, node) LIVES_LIST_FOREACH(list) if (list==node) break;

// returns node if node is within list, otherwise NULL
LiVESList *lives_list_contains(LiVESList *, LiVESList *node);

// locate in list
LiVESList *lives_list_find_by_data(LiVESList *, livespointer data);

// detatch and free node
// !! no checking is done to verify that 'node' is within list
LiVESList *lives_list_remove_node(LiVESList *, LiVESList *node, boolean free_data);
LiVESList *lives_list_remove_data(LiVESList *, livespointer data, boolean free_data);

// detatch but do not free
// !! no checking is done to verify that 'node' is within list
LiVESList *lives_list_detatch_node(LiVESList *, LiVESList *node);
LiVESList *lives_list_detatch_data(LiVESList **, livespointer data);

// trim list so last node is node before 'node', freeing the sublist starting from node
// if node not in list, returns list unchanged. If node == list, returns NULL, otherwise returns list
// !! no checking is done to verify that 'node' is within list
LiVESList *lives_list_trim(LiVESList *, LiVESList *node, boolean free_data);

// returns TRUE if found and removed
boolean lives_list_check_remove_data(LiVESList **, livespointer data, boolean free_data);

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
lives_hash_store_t *remove_from_hash_store(lives_hash_store_t *, const char *key);
const char *hash_key_from_leaf_name(const char *name);
#endif
