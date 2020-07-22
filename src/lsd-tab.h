// lsd-tab.h
// LiVES
// (c) G. Finch 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details
// functions for handling the LiVES Struct Def Table

#ifndef HAS_LIVES_LSD_TAB_H
#define HAS_LIVES_LSD_TAB_H

typedef enum {
  LIVES_STRUCT_CLIP_DATA_T,
  LIVES_N_STRUCTS
} lives_struct_type;

#define LIVES_STRUCT_FIRST LIVES_STRUCT_CLIP_DATA_T

const lives_struct_def_t *get_lsd(lives_struct_type st_type);
void *struct_from_template(lives_struct_type st_type);
void *copy_struct(lives_struct_def_t *);
const char *lives_struct_get_creator(lives_struct_def_t *);
void unref_struct(lives_struct_def_t *);
void ref_struct(lives_struct_def_t *);
boolean lives_structs_equal(lives_struct_def_t *, lives_struct_def_t *);
boolean lives_struct_is_a(lives_struct_def_t *, const char *st_type);
boolean lives_structs_same_type(lives_struct_def_t *, lives_struct_def_t *);

uint64_t lsd_check_struct(lives_struct_def_t *);
uint64_t lsd_check_match(lives_struct_def_t *, lives_struct_def_t *);

#endif
