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
  LIVES_STRUCT_FILE_DETS_T,
  LIVES_STRUCT_INTENTCAP_T,
  LIVES_N_STRUCTS
} lives_struct_type;

#define LIVES_STRUCT_FIRST LIVES_STRUCT_CLIP_DATA_T

const lsd_struct_def_t *lsd_from_store(lives_struct_type st_type);

const lsd_struct_def_t *get_lsd(lives_struct_type st_type);
void *struct_from_template(lives_struct_type st_type);
void *struct_from_template_inplace(lives_struct_type st_type, void *thestruct);
void *copy_struct(lsd_struct_def_t *);
void *copy_struct_inplace(lsd_struct_def_t *, void *thestruct);
const char *lsd_struct_get_creator(lsd_struct_def_t *);
void unref_struct(lsd_struct_def_t *);
void ref_struct(lsd_struct_def_t *);
boolean lsd_structs_equal(lsd_struct_def_t *, lsd_struct_def_t *);
boolean lsd_struct_is_a(lsd_struct_def_t *, const char *st_type);
boolean lsd_structs_same_type(lsd_struct_def_t *, lsd_struct_def_t *);

uint64_t lsd_check_struct(lsd_struct_def_t *);
uint64_t lsd_check_match(lsd_struct_def_t *, lsd_struct_def_t *);

char *weed_plant_to_header(weed_plant_t *, const char *tname);

#endif
