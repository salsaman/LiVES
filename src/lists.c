// lists.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"


LIVES_GLOBAL_INLINE boolean lives_list_contains_string(LiVESList *list, const char *strng) {
  for (; list; list = list->next) if (!lives_strcmp(strng, (const char *)list->data)) return TRUE;
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
    lives_free(lptr);
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


LIVES_GLOBAL_INLINE LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add) {
  LiVESList *list = xlist, *listlast = NULL;
  while (list) {
    listlast = list;
    if (!lives_utf8_strcasecmp((const char *)list->data, add)) return xlist;
    list = list->next;
  }
  list = lives_list_append(listlast, lives_strdup(add));
  if (!xlist) return list;
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
      lives_list_free(xlist);
      return list;
    }
  }
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_copy_strings(LiVESList *list) {
  // copy a list, copying the strings too
  LiVESList *xlist = NULL, *olist = list;
  while (olist) {
    xlist = lives_list_prepend(xlist, lives_strdup((char *)olist->data));
    olist = olist->next;
  }
  return lives_list_reverse(xlist);
}


boolean string_lists_differ(LiVESList *alist, LiVESList *blist) {
  // compare 2 lists of strings and see if they are different (ignoring ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function
  // is designed to deal with short lists only

  LiVESList *rlist = blist;

  if (lives_list_length(alist) != lives_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatch

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


LIVES_GLOBAL_INLINE void lives_list_free_strings(LiVESList *list) {
  for (; list; list = list->next) lives_freep((void **)&list->data);
}


LIVES_GLOBAL_INLINE void lives_slist_free_all(LiVESSList **list) {
  if (!list || !*list) return;
  lives_list_free_strings((LiVESList *)*list);
  lives_slist_free(*list);
  *list = NULL;
}


LIVES_GLOBAL_INLINE void lives_list_free_all(LiVESList **list) {
  if (!list || !*list) return;
  lives_list_free_strings(*list);
  lives_list_free(*list);
  *list = NULL;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_remove_node(LiVESList *list, LiVESList *node, boolean free_data) {
  if (!node) return NULL;
  if (node->prev) node->prev->next = node->next;
  if (node->next) node->next->prev = node->prev;
  if (node == list) list = list->next;
  node->next = node->prev = NULL;
  if (node->data && free_data) lives_free(node->data);
  lives_list_free(node);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_remove_data(LiVESList *list, livespointer data, boolean free_data) {
  LiVESList *xlist = list;
  for (; xlist; xlist = xlist->next) if (xlist->data == data) break;
  if (xlist) return lives_list_remove_node(list, xlist, free_data);
  return NULL;
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
