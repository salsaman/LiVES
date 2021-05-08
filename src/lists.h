// lists.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _LISTS_H_
#define _LISTS_H_

LiVESList *array_to_string_list(const char **y, int offset, int len);

boolean lives_list_contains_string(LiVESList *, const char *);

LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, const char *string) WARN_UNUSED;
LiVESList *lives_list_copy_strings(LiVESList *);
boolean string_lists_differ(LiVESList *, LiVESList *);
boolean lists_differ(LiVESList *, LiVESList *, boolean ordered);
LiVESList *lives_list_append_unique(LiVESList *, const char *add);
LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip);
int lives_list_strcmp_index(LiVESList *list, livesconstpointer data, boolean case_sensitive);

boolean int_array_contains_value(int *array, int num_elems, int value);

LiVESList *lives_list_sort_alpha(LiVESList *, boolean fwd);

void lives_list_free_strings(LiVESList *);
void lives_list_free_all(LiVESList **);
void lives_slist_free_all(LiVESSList **);

LiVESList *lives_list_remove_node(LiVESList *, LiVESList *node, boolean free_data);
LiVESList *lives_list_remove_data(LiVESList *, livespointer data, boolean free_data);

LiVESList *lives_list_append_unique(LiVESList *, const char *add);

LiVESList *idx_list_update(LiVESList *, int64_t idx, void *data);
LiVESList *idx_list_update(LiVESList *, int64_t idx, void *data);
LiVESList *idx_list_remove(LiVESList *, int idx, boolean free_data);
boolean idx_list_get_data(LiVESList *, int idx, void **val_locn);

#endif
