// lsd-tab.c
// LiVES
// (c) G. Finch 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling the LiVES Struct Def Table

#include "main.h"

#include "lsd.h"

#define CREATOR_ID "Created in LiVES main"

static const lives_struct_def_t *lsd_tab[LIVES_N_STRUCTS];
boolean tab_inited = FALSE;

static void init_lsd_tab(void) {
  for (int i = 0; i < LIVES_N_STRUCTS; i++) lsd_tab[i] = NULL;
  tab_inited = TRUE;
}

static const lives_struct_def_t *get_lsd(lives_struct_type st_type) {
  const lives_struct_def_t *lsd;

  if (!tab_inited) init_lsd_tab();
  switch (st_type) {
  case LIVES_STRUCT_CLIP_DATA_T:
    if (!lsd_tab[st_type]) {
      lsd = lsd_create("lives_clip_data_t", sizeof(lives_clip_data_t), "strgs", 6);
      if (!lsd) return NULL;
      else {
        lives_special_field_t **specf = lsd->special_fields;
        lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
        specf[0] = make_special_field(LIVES_FIELD_CHARPTR, cdata, &cdata->URI,
                                      "URI", 0, NULL, NULL, NULL);
        specf[1] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->priv,
                                      "priv", 0, NULL, NULL, NULL);
        specf[2] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->title,
                                      "title", 1024, NULL, NULL, NULL);
        specf[3] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->author,
                                      "author", 1024, NULL, NULL, NULL);
        specf[4] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->comment,
                                      "comment", 1024, NULL, NULL, NULL);
        specf[5] = make_special_field(LIVES_FIELD_ARRAY, cdata, &cdata->palettes,
                                      "palettes", 4, NULL, NULL, NULL);
        lives_struct_init(lsd, cdata, &cdata->lsd);
        lives_struct_set_class_data((lives_struct_def_t *)lsd, CREATOR_ID);
        lives_free(cdata);
      }
      lsd_tab[st_type] = lsd;
    }
    return lsd_tab[st_type];
  default:
    break;
  }
  return NULL;
}

void *struct_from_template(lives_struct_type st_type) {
  const lives_struct_def_t *lsd = get_lsd(st_type);
  if (!lsd) return NULL;
  return lives_struct_create(lsd);
}


LIVES_GLOBAL_INLINE void *copy_struct(lives_struct_def_t *lsd) {
  if (lsd) return lives_struct_copy(lsd);
  return NULL;
}


LIVES_GLOBAL_INLINE void unref_struct(lives_struct_def_t *lsd) {
  if (lsd) lives_struct_unref(lsd);
}


LIVES_GLOBAL_INLINE void ref_struct(lives_struct_def_t *lsd) {
  if (lsd) lives_struct_ref(lsd);
}


LIVES_GLOBAL_INLINE const char *lives_struct_get_creator(lives_struct_def_t *lsd) {
  if (lsd) return lives_struct_get_class_data(lsd);
  return NULL;
}


LIVES_GLOBAL_INLINE boolean lives_structs_equal(lives_struct_def_t *lsd, lives_struct_def_t *other) {
  if (lsd && other) return (lives_struct_get_uid(lsd) == lives_struct_get_uid(other));
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_struct_is_a(lives_struct_def_t *lsd, const char *st_type) {
  if (lsd) return (!lives_strcmp(lives_struct_get_type(lsd), st_type));
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_structs_same_type(lives_struct_def_t *lsd,
    lives_struct_def_t *other) {
  if (lsd && other) return lives_struct_is_a(lsd, lives_struct_get_type(other));
  return FALSE;
}


