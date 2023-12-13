// lists.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "maths.h"


LIVES_GLOBAL_INLINE void lives_sync_list_readlock_priv(lives_sync_list_t *synclist) {
  if (synclist) pthread_rwlock_rdlock(&synclist->priv_lock);
}
  

LIVES_GLOBAL_INLINE void lives_sync_list_writelock_priv(lives_sync_list_t *synclist) {
  if (synclist) pthread_rwlock_wrlock(&synclist->priv_lock);
}


LIVES_GLOBAL_INLINE void lives_sync_list_unlock_priv(lives_sync_list_t *synclist) {
  if (synclist) pthread_rwlock_unlock(&synclist->priv_lock);
}


LIVES_GLOBAL_INLINE void lives_sync_list_set_priv(lives_sync_list_t *synclist, void *priv) {
  if (synclist) synclist->priv = priv;
}


LIVES_GLOBAL_INLINE void *lives_sync_list_get_priv(lives_sync_list_t *synclist) {
  return synclist ? synclist->priv : NULL;
}


LIVES_GLOBAL_INLINE int lives_sync_list_get_nvals(lives_sync_list_t *synclist) {
  return synclist ? synclist->nvals : 0;
}


LIVES_GLOBAL_INLINE void lives_sync_list_set_lilo(lives_sync_list_t *synclist, boolean lilo) {
  if (synclist) synclist->flags = lilo ? synclist->flags | SYNCLIST_FLAG_LILO
                                    : synclist->flags & ~SYNCLIST_FLAG_LILO;
}


LIVES_GLOBAL_INLINE void lives_sync_list_set_pop_head(lives_sync_list_t *synclist, boolean yes) {
  if (synclist) synclist->flags = yes ? synclist->flags | SYNCLIST_FLAG_POP_HEAD
                                    : synclist->flags & ~SYNCLIST_FLAG_POP_HEAD;
}


LIVES_GLOBAL_INLINE void lives_sync_list_set_free_priv(lives_sync_list_t *synclist, boolean yes) {
  if (synclist) synclist->flags = yes ? synclist->flags | SYNCLIST_FLAG_FREE_PRIV
                                    : synclist->flags & ~SYNCLIST_FLAG_FREE_PRIV;
}


LIVES_GLOBAL_INLINE void lives_sync_list_free_on_empty(lives_sync_list_t *synclist, boolean yes) {
  if (synclist) synclist->flags = yes ? synclist->flags | SYNCLIST_FLAG_FREE_ON_EMPTY
                                    : synclist->flags & ~SYNCLIST_FLAG_FREE_ON_EMPTY;
}


LIVES_LOCAL_INLINE void lives_sync_list_rdlock(lives_sync_list_t *synclist) {
  if (synclist) pthread_rwlock_rdlock(&synclist->lock);
}

// what we pass in is a ptr to sync_list *. Initially this should be set to synclist
// itself. The sync_list ** will be stored in the synclist. If the synclist is
// to be freed the value is derefferenced and set to NULL
// since it is safe to call sync_list_unlock with a ptr to NULLptr
// this i then passed in there
LIVES_GLOBAL_INLINE void lives_sync_list_wrlock(lives_sync_list_t *synclist)
{if (synclist) pthread_rwlock_wrlock(&synclist->lock);}


LIVES_GLOBAL_INLINE void lives_sync_list_unlock(lives_sync_list_t *synclist)
{if (synclist) pthread_rwlock_unlock(&synclist->lock);}


LIVES_GLOBAL_INLINE lives_sync_list_t *lives_sync_list_new(void) {
  LIVES_CALLOC_TYPE(lives_sync_list_t, synclist, 1);
  pthread_rwlock_init(&synclist->lock, NULL);
  pthread_rwlock_init(&synclist->priv_lock, NULL);
  synclist->flags = SYNCLIST_FLAG_FREE_ON_EMPTY | SYNCLIST_FLAG_LILO
                    | SYNCLIST_FLAG_POP_HEAD;
  return synclist;
}


static void _lives_sync_list_append(lives_sync_list_t *synclist, LiVESList *node) {
  if (synclist->list) {
    synclist->last->next = node;
    node->prev = synclist->last;
  } else synclist->list = node;;
  synclist->last = node;
  synclist->nvals++;
}

static lives_sync_list_t *lives_sync_list_append(lives_sync_list_t *synclist, void *data) {
  LiVESList *list = lives_list_append(NULL, data);
  if (!synclist) synclist = lives_sync_list_new();
  lives_sync_list_wrlock(synclist);
  _lives_sync_list_append(synclist, list);
  lives_sync_list_unlock(synclist);
  return synclist;
}


static void _lives_sync_list_prepend(lives_sync_list_t *synclist, LiVESList *node) {
  if (synclist->list) {
    synclist->list->prev = node;
    node->next = synclist->list;
  } else synclist->last = node;
  synclist->list = node;
  synclist->nvals++;
}

static lives_sync_list_t *lives_sync_list_prepend(lives_sync_list_t *synclist, void *data) {
  if (!synclist) synclist = lives_sync_list_new();
  LiVESList *list = lives_list_append(NULL, data);
  lives_sync_list_wrlock(synclist);
  _lives_sync_list_prepend(synclist, list);
  lives_sync_list_unlock(synclist);
  return synclist;
}


lives_sync_list_t *lives_sync_list_push(lives_sync_list_t *synclist, void *data) {
  if (!synclist) synclist = lives_sync_list_new();
  // by default we always pop first, we append for LILO, prepend for LIFO
  if (synclist->flags & SYNCLIST_FLAG_LILO)
    synclist = lives_sync_list_append(synclist, data);
  else
    synclist = lives_sync_list_prepend(synclist, data);
  return synclist;
}


lives_sync_list_t *lives_sync_list_push_priority(lives_sync_list_t *synclist, void *data) {
  if (synclist) {
    // swap order, we prepend for LILO, append for LIFO
    if (synclist->flags & SYNCLIST_FLAG_LILO)
      synclist = lives_sync_list_prepend(synclist, data);
    else
      synclist = lives_sync_list_append(synclist, data);
  }
  return synclist;
}


LIVES_GLOBAL_INLINE void lives_sync_list_replace_data(lives_sync_list_t *synclist, void *from, void *to) {
  if (synclist && synclist->list && from) {
    lives_sync_list_wrlock(synclist);
    LiVESList *list = lives_list_find_by_data(synclist->list, from);
    if (list) list->data = to;
    lives_sync_list_unlock(synclist);
  }
}


LIVES_GLOBAL_INLINE void *_lives_sync_list_find(lives_sync_list_t *synclist, lives_condfunc_f cb_func) {
  LiVESList *list = synclist->list;
  FIND_BY_CALLBACK(list, cb_func);
  return list ? list->data : NULL;
}

LIVES_GLOBAL_INLINE void *lives_sync_list_find(lives_sync_list_t *synclist, lives_condfunc_f cb_func) {
  void *data = NULL;
  if (synclist) {
    lives_sync_list_rdlock(synclist);
    data = _lives_sync_list_find(synclist, cb_func);
    lives_sync_list_unlock(synclist);
  }
  return data;
}


LIVES_GLOBAL_INLINE LiVESList *_lives_sync_list_pop(lives_sync_list_t **synclistp) {
  lives_sync_list_t *synclist = *synclistp;
  LiVESList *list;
  if (synclist->flags & SYNCLIST_FLAG_POP_HEAD) {
    list = synclist->list;
    synclist->list = list->next;
    if (synclist->list) synclist->list->prev = NULL;
    else synclist->last = NULL;
  } else {
    list = synclist->last;
    synclist->last = list->prev;
    if (synclist->last) synclist->last->next = NULL;
    else synclist->list = NULL;
  }

  lives_list_free_1(list);
  synclist->nvals--;

  if (!synclist->list && (synclist->flags & SYNCLIST_FLAG_FREE_ON_EMPTY))
    *synclistp = _lives_sync_list_free(synclist, FALSE);

  return list;
}

LIVES_GLOBAL_INLINE void *lives_sync_list_pop(lives_sync_list_t **synclistp) {
  LiVESList *list = NULL;
  void *data = NULL;
  if (synclistp && *synclistp) {
    lives_sync_list_wrlock(*synclistp);
    list = _lives_sync_list_pop(synclistp);
    data = list->data;
    if (*synclistp) lives_sync_list_unlock(*synclistp);
  }
  return data;
}


LIVES_GLOBAL_INLINE lives_sync_list_t *_lives_sync_list_remove(lives_sync_list_t *synclist,
    void *data, boolean do_free) {
  if (synclist) {
    LiVESList *list = synclist->list;
    for (; list && list->data != data; list = list->next);
    if (!list) return synclist;
    if (list->prev) list->prev->next = list->next;
    else synclist->list = list->next;
    if (list->next) list->next->prev = list->prev;
    else synclist->last = list->prev;
    list->prev = list->next = NULL;
    lives_list_free_1(list);
    if (do_free) lives_free(data);
    synclist->nvals--;

    if (!synclist->list && (synclist->flags & SYNCLIST_FLAG_FREE_ON_EMPTY))
      synclist = _lives_sync_list_free(synclist, FALSE);
  }
  return synclist;
}

LIVES_GLOBAL_INLINE lives_sync_list_t *lives_sync_list_remove(lives_sync_list_t *synclist,
    void *data, boolean do_free) {
  if (synclist) {
    lives_sync_list_wrlock(synclist);
    synclist = _lives_sync_list_remove(synclist, data, do_free);
    if (synclist) lives_sync_list_unlock(synclist);
  }
  return synclist;
}


LIVES_GLOBAL_INLINE lives_sync_list_t *_lives_sync_list_clear(lives_sync_list_t *synclist,
    boolean free_data) {
  if (free_data) lives_list_free_data(synclist->list);
  lives_list_free(synclist->list);
  synclist->list = NULL;
  synclist->nvals = 0;
  if (synclist->flags & SYNCLIST_FLAG_FREE_ON_EMPTY)
    synclist = _lives_sync_list_free(synclist, FALSE);
  return synclist;
}


LIVES_GLOBAL_INLINE lives_sync_list_t *lives_sync_list_clear(lives_sync_list_t *synclist,
    boolean free_data) {
  if (synclist && synclist->list) {
    lives_sync_list_wrlock(synclist);
    synclist = _lives_sync_list_clear(synclist, free_data);
    if (synclist) lives_sync_list_unlock(synclist);
  }
  return synclist;
}


LIVES_GLOBAL_INLINE lives_sync_list_t *_lives_sync_list_free(lives_sync_list_t *synclist,
    boolean free_data) {
  pthread_rwlock_trywrlock(&synclist->lock);
  if (synclist->list) {
    if (free_data) lives_list_free_data(synclist->list);
    lives_list_free(synclist->list);
    synclist->list = NULL;
  }
  pthread_rwlock_unlock(&synclist->lock);

  pthread_rwlock_destroy(&synclist->lock);
  pthread_rwlock_destroy(&synclist->priv_lock);
  lives_free(synclist);
  return NULL;
}

LIVES_GLOBAL_INLINE lives_sync_list_t *lives_sync_list_free(lives_sync_list_t *synclist,
    boolean free_data) {
  if (synclist) synclist = _lives_sync_list_free(synclist, free_data);
  return NULL;
}

/////////////////////////////

LIVES_GLOBAL_INLINE LiVESList *lives_list_locate_string(LiVESList *list, const char *strng) {
  for (; list; list = list->next) if (!lives_strcmp(strng, (const char *)list->data)) return list;
  return NULL;
}


LIVES_GLOBAL_INLINE boolean cmp_rows(uint64_t *r0, uint64_t *r1, int nr) {
  for (int i = 0; i < nr; i++) if (r0[i] != r1[i]) return TRUE;
  return FALSE;
}


LiVESList *array_to_string_list(const char **array, int offset, int len) {
  // build a LiVESList from an array, starting from element offset + 1, len is len of list
  // if len is 0, then stop when we hit NULL element
  // final empty string is omitted (unless len == 0)
  // charset is converted from local to utf8, also newlines are unescaped
  LiVESList *slist = NULL;
  char *string, *tmp;

  for (int i = offset + 1; !len || i < len; i++) {
    if (!array[i]) break;
    string = subst((tmp = L2U8(array[i])), "\\n", "\n");
    lives_free(tmp);

    // omit a last empty string
    if (!len || i < len - 1 || *string) {
      slist = lives_list_append(slist, string);
    } else lives_free(string);
  }

  return slist;
}


typedef struct {
  int64_t idx;
  void *data;
} idx_list_data_t;

LIVES_LOCAL_INLINE
LiVESList *create_idx_list_element(int idx, void *data) {
  idx_list_data_t *newdata = (idx_list_data_t *)lives_malloc(sizeof(idx_list_data_t));
  LiVESList *newlist = lives_list_append(NULL, newdata);
  newdata->idx = idx;
  newdata->data = data;
  return newlist;
}


LiVESList *idx_list_update(LiVESList *idxlist, int64_t idx, void *data) {
  LiVESList *lptr = idxlist, *lptrnext, *newlist;
  idx_list_data_t *ldata;

  for (; lptr; lptr = lptrnext) {
    lptrnext = lptr->next;
    ldata = (idx_list_data_t *)lptr->data;
    if (ldata->idx < idx) {
      if (!lptrnext) break;
      continue;
    }
    if (ldata->idx == idx) {
      ldata->data = data;
      return idxlist;
    } else {
      newlist = create_idx_list_element(idx, data);
      if (lptr->prev) {
        lptr->prev->next = newlist;
        newlist->prev = lptr->prev;
      } else idxlist = newlist;
      newlist->next = lptr;
      lptr->prev = newlist;
      return idxlist;
    }
  }
  newlist = create_idx_list_element(idx, data);
  if (lptr) {
    lptr->next = newlist;
    newlist->prev = lptr;
  } else idxlist = newlist;
  return idxlist;
}


LiVESList *idx_list_remove(LiVESList *idxlist, int idx, boolean free_data) {
  LiVESList *lptr = idxlist;
  idx_list_data_t *ldata;
  for (; lptr; lptr = lptr->next) {
    ldata = (idx_list_data_t *)lptr->data;
    if (ldata->idx < idx) continue;
    if (ldata->idx > idx) break;
    if (free_data && ldata->data) lives_free(ldata->data);
    if (lptr->prev) lptr->prev->next = lptr->next;
    else idxlist = lptr->next;
    if (lptr->next) lptr->next->prev = lptr->prev;
    lives_free(lptr->data);
    lives_list_free_1(lptr);
  }
  return idxlist;
}


boolean idx_list_get_data(LiVESList *idxlist, int idx, void **val_locn) {
  LiVESList *lptr = idxlist;
  idx_list_data_t *ldata;
  for (; lptr; lptr = lptr->next) {
    ldata = (idx_list_data_t *)lptr->data;
    if (ldata->idx < idx) continue;
    if (ldata->idx > idx) break;
    if (val_locn) *val_locn = ldata->data;
    return TRUE;
  }
  return FALSE;
}


#define BL_LIM 128
LIVES_GLOBAL_INLINE LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip) {
  LiVESList *list = NULL;
  int pieces = get_token_count(buffer, delim[0]);
  char *buf, **array = lives_strsplit(buffer, delim, pieces);
  boolean biglist = pieces >= BL_LIM;
  for (int i = 0; i < pieces; i++) {
    if (array[i]) {
      if (strip || i == pieces - 1) buf = lives_strdup(lives_strstrip(array[i]));
      else buf = lives_strdup_printf("%s%s", array[i], delim);
      if (*buf || allow_blanks) {
        if (biglist) list = lives_list_prepend(list, buf);
        else list = lives_list_append(list, buf);
      } else lives_free(buf);
    }
  }
  lives_strfreev(array);
  if (biglist && list) return lives_list_reverse(list);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_append_unique_str(LiVESList *xlist, const char *add) {
  LiVESList *listlast = NULL;
  for (LiVESList *list = xlist; list; list = list->next) {
    if (!lives_utf8_strcasecmp((const char *)list->data, add)) return xlist;
    listlast = list;
  }
  return lives_list_append(listlast, (void *)add);
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_append_unique(LiVESList *xlist, livespointer add) {
  LiVESList *listlast = NULL;
  for (LiVESList *list = xlist; list; list = list->next) {
    if (list->data == add) return xlist;
    listlast = list;
  }
  listlast = lives_list_append(listlast, add);
  if (!xlist) xlist = listlast;
  return xlist;

}


LIVES_GLOBAL_INLINE LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) {
  // move item to first in list
  LiVESList *xlist = item;
  if (xlist == list || !xlist) return list;
  if (xlist->prev) xlist->prev->next = xlist->next;
  if (xlist->next) xlist->next->prev = xlist->prev;
  xlist->prev = NULL;
  if ((xlist->next = list) != NULL) list->prev = xlist;
  return xlist;
}


LiVESList *lives_list_delete_string(LiVESList *list, const char *string) {
  // remove string from list, using strcmp

  LiVESList *xlist = list;
  for (; xlist; xlist = xlist->next) {
    if (!lives_utf8_strcasecmp((char *)xlist->data, string)) {
      lives_free((livespointer)xlist->data);
      if (xlist->prev) xlist->prev->next = xlist->next;
      else list = xlist->next;
      if (xlist->next) xlist->next->prev = xlist->prev;
      xlist->next = xlist->prev = NULL;
      lives_list_free_1(xlist);
      return list;
    }
  }
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_copy_strings(LiVESList *list) {
  // copy a list, copying the strings too
  LiVESList *xlist = NULL;
  for (LiVESList *olist = list; olist; olist = olist->next)
    xlist = lives_list_prepend(xlist, lives_strdup((char *)olist->data));
  return lives_list_reverse(xlist);
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_copy_reverse_strings(LiVESList *list) {
  // copy a list, copying the strings too
  LiVESList *xlist = NULL;
  for (LiVESList *olist = list; olist; olist = olist->next)
    xlist = lives_list_prepend(xlist, lives_strdup((char *)olist->data));
  return xlist;
}


boolean string_lists_differ(LiVESList *alist, LiVESList *blist) {
  // compare 2 lists of strings and see if they are different (ignoring ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function
  // is designed to deal with short lists only

  LiVESList *rlist = blist;

  if (lives_list_length(alist) != lives_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatclish

  for (; alist; alist = alist->next) {
    LiVESList *qlist = rlist;
    boolean matched = TRUE;
    while (qlist) {
      if (!(lives_utf8_strcasecmp((char *)alist->data, (char *)qlist->data))) {
        if (matched) rlist = qlist->next;
        break;
      }
      qlist = qlist->next;
      if (!qlist) return TRUE;
      matched = FALSE;
    }
  }

  // since both lists were of the same length, there is no need to further check blist

  return FALSE;
}


boolean lists_differ(LiVESList *alist, LiVESList *blist, boolean order) {
  // compare 2 and see if they are different (with or without ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function
  // is designed to deal with short lists only

  LiVESList *rlist = blist;

  if (lives_list_length(alist) != lives_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatch

  for (; alist; alist = alist->next) {
    if (order) {
      if (alist->data != blist->data) return TRUE;
      blist = blist->next;
      continue;
    } else {
      LiVESList *qlist = rlist;
      boolean matched = TRUE;
      while (qlist) {
        if (alist->data == qlist->data) {
          if (matched) rlist = qlist->next;
          break;
        }
        qlist = qlist->next;
        if (!qlist) return TRUE;
        matched = FALSE;
      }
    }
  }
  // since both lists were of the same length, there is no need to further check blist

  return FALSE;
}


LIVES_GLOBAL_INLINE boolean int_array_contains_value(int *array, int num_elems, int value) {
  for (int i = 0; i < num_elems; i++) if (array[i] == value) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean int64_array_contains_value(int64_t *array, int num_elems, int64_t value) {
  for (int i = 0; i < num_elems; i++) if (array[i] == value) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_sort_alpha(LiVESList *list, boolean fwd) {
  /// stable sort, so input list should NOT be freed
  /// handles utf-8 strings
  return lives_list_sort_with_data(list, lives_utf8_strcmpfunc, LIVES_INT_TO_POINTER(fwd));
}


LIVES_GLOBAL_INLINE void lives_list_free_data(LiVESList *list)
{LIVES_LIST_FOREACH(list) lives_freep((void **)&list->data);}


LIVES_GLOBAL_INLINE void lives_list_free_all(LiVESList **list) {
  if (!list || !*list) return;
  lives_list_free_data(*list);
  lives_list_free(lives_steal_pointer((void **)list));
}

// if node isin list, returns node, else returns NULL
LIVES_GLOBAL_INLINE LiVESList *lives_list_contains(LiVESList *list, LiVESList *node) {
  if (!node || !list) return NULL;
  LIVES_LIST_CHECK_CONTAINS(list, node);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_find_by_data(LiVESList *list, livespointer data) {
  LiVESList *xlist = list;
  FIND_BY_DATA(xlist, data);
  return xlist;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_remove_node(LiVESList *list, LiVESList *node, boolean free_data) {
  list = lives_list_detatch_node(list, node);
  if (node->data && free_data) lives_free(node->data);
  lives_list_free_1(node);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_detatch_node(LiVESList *list, LiVESList *node) {
  if (!node || !list) return list;
  if (node->prev) node->prev->next = node->next;
  if (node->next) node->next->prev = node->prev;
  if (node == list) list = node->next;
  node->prev = node->next = NULL;
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_remove_data(LiVESList *list, livespointer data, boolean free_data) {
  LiVESList *xlist = lives_list_find_by_data(list, data);
  if (xlist) return lives_list_remove_node(list, xlist, free_data);
  return NULL;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_detatch_data(LiVESList **list, livespointer data) {
  LiVESList *xlist = lives_list_find_by_data(*list, data);
  if (xlist) {
    *list = lives_list_detatch_node(*list, xlist);
    return xlist;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE boolean lives_list_check_remove_data(LiVESList **list, livespointer data, boolean free_data) {
  // check if data is in list, id so remove it and return TRUE
  LiVESList *xlist = lives_list_find_by_data(*list, data);
  if (xlist) {
    *list = lives_list_remove_node(*list, xlist, free_data);
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE int lives_list_strcmp_index(LiVESList *list,
    livesconstpointer data, boolean case_sensitive) {
  // find data in list, using strcmp
  if (case_sensitive) {
    for (int i = 0; list; list = list->next, i++) {
      if (!lives_strcmp((const char *)list->data, (const char *)data)) return i;
    }
  } else {
    for (int i = 0; list; list = list->next, i++) {
      if (!lives_utf8_strcasecmp((const char *)list->data, (const char *)data)) return i;
    }
  }
  return -1;
}


LIVES_GLOBAL_INLINE char *lives_list_to_string(LiVESList *list, const char *delim) {
  char *res = NULL;
  for (; list; list = list->next) res = lives_strcollate(&res, delim, list->data);
  return res;
}

/////////////////// hash stores  //////
#define HASH_STORE_PFX "k_"
#define HS_PFX_LEN 2

#define IS_HASHSTORE_KEY(key) ((key) && lives_strlen_atleast((key), HS_PFX_LEN) \
			       && !lives_strncmp((key), HASH_STORE_PFX, HS_PFX_LEN))
#define GET_HASHSTORE_KEY(hkey) (IS_HASHSTORE_KEY((hkey)) ? ((hkey) + HS_PFX_LEN) : NULL)
#define MAKE_HASHSTORE_KEY(key) ((key) ? lives_strdup_printf("%s%" PRIu64, HASH_STORE_PFX, (key)) : NULL)

LIVES_GLOBAL_INLINE lives_hash_store_t *lives_hash_store_new(const char *id) {
  lives_hash_store_t *store = lives_plant_new(LIVES_PLANT_HASH_STORE);
  if (id) weed_set_string_value(store, LIVES_LEAF_ID, id);
  return store;
}

LIVES_GLOBAL_INLINE void *get_from_hash_store_i(lives_hash_store_t *store, uint64_t key) {
  if (!store) return NULL;
  else {
    char *xkey = MAKE_HASHSTORE_KEY(key);
    void *vret = weed_get_voidptr_value(store, xkey, NULL);
    lives_free(xkey);
    return vret;
  }
}

LIVES_GLOBAL_INLINE void *get_from_hash_store(lives_hash_store_t *store, const char *key) {
  return get_from_hash_store_i(store, fast_hash64((void *)key));
}


LIVES_GLOBAL_INLINE const char *hash_key_from_leaf_name(const char *name) {
  return GET_HASHSTORE_KEY(name);
}


LIVES_GLOBAL_INLINE lives_hash_store_t *add_to_hash_store_i(lives_hash_store_t *store, uint64_t key, void *data) {
  if (!store) store = lives_hash_store_new(NULL);
  if (store) {
    char *xkey = MAKE_HASHSTORE_KEY(key);
    weed_set_voidptr_value(store, xkey, data);
    lives_free(xkey);
  }
  return store;
}

LIVES_GLOBAL_INLINE lives_hash_store_t *add_to_hash_store(lives_hash_store_t *store, const char *key, void *data) {
  return add_to_hash_store_i(store, fast_hash64((void *)key), data);
}


LIVES_GLOBAL_INLINE lives_hash_store_t *remove_from_hash_store_i(lives_hash_store_t *store, uint64_t key) {
  if (store) {
    char *xkey = lives_strdup_printf("k_%lu", key);
    weed_leaf_delete(store, xkey);
    lives_free(xkey);
  }
  return store;
}

LIVES_GLOBAL_INLINE lives_hash_store_t *remove_from_hash_store(lives_hash_store_t *store, const char *key) {
  uint64_t ikey = fast_hash64(key);
  return remove_from_hash_store_i(store, ikey);
}

typedef boolean(*lives_hash_match_f)(void *data, void *udata);

LIVES_GLOBAL_INLINE void *get_from_hash_store_cbfunc(lives_hash_store_t *store, lives_hash_match_f matchfn, void *udata) {
  void *ret = NULL;
  if (store) {
    boolean found = FALSE;
    char **ll = weed_plant_list_leaves(store, NULL);
    for (int i = 0; ll[i]; i++) {
      if (!found) {
        if (IS_HASHSTORE_KEY(ll[i])) {
          if ((*matchfn)(weed_get_voidptr_value(store, ll[i], NULL), udata)) {
            ret = weed_get_voidptr_value(store, ll[i], NULL);
            found = TRUE;
          }
        }
      }
      lives_free(ll[i]);
    }
    lives_free(ll);
  }
  return ret;
}
