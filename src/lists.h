// lists.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _LISTS_H_
#define _LISTS_H_

LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, const char *string) WARN_UNUSED;
LiVESList *lives_list_copy_strings(LiVESList *list);
boolean string_lists_differ(LiVESList *, LiVESList *);
LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add);
LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip);
int lives_list_strcmp_index(LiVESList *list, livesconstpointer data, boolean case_sensitive);

boolean int_array_contains_value(int *array, int num_elems, int value);

LiVESList *lives_list_sort_alpha(LiVESList *list, boolean fwd);

void lives_list_free_strings(LiVESList *);
void lives_list_free_all(LiVESList **);
void lives_slist_free_all(LiVESSList **);

LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add);

LiVESList *idx_list_update(LiVESList *idxlist, int64_t idx, void *data);
LiVESList *idx_list_update(LiVESList *idxlist, int64_t idx, void *data);
LiVESList *idx_list_remove(LiVESList *idxlist, int idx, boolean free_data);
boolean idx_list_get_data(LiVESList *idxlist, int idx, void **val_locn);

#endif
