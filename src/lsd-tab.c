// lsd-tab.c
// LiVES
// (c) G. Finch 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling the LiVES Struct Def Table

#include "main.h"

#include "lsd.h"

#define CREATOR_ID "Created in LiVES main"

static const lives_struct_def_t *lsd_table[LIVES_N_STRUCTS];
boolean tab_inited = FALSE;

static void init_lsd_tab(void) {
  for (int i = 0; i < LIVES_N_STRUCTS; i++) lsd_table[i] = NULL;
  tab_inited = TRUE;
}

const lives_struct_def_t *get_lsd(lives_struct_type st_type) {
  const lives_struct_def_t *lsd;
  if (st_type < LIVES_STRUCT_FIRST || st_type >= LIVES_N_STRUCTS) return NULL;
  if (!tab_inited) init_lsd_tab();
  else if (lsd_table[st_type]) return lsd_table[st_type];
  switch (st_type) {
  case LIVES_STRUCT_CLIP_DATA_T:
    lsd = lsd_create("lives_clip_data_t", sizeof(lives_clip_data_t), "adv_timing", 6);
    if (lsd) {
      lives_special_field_t **specf = lsd->special_fields;
      lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
      specf[0] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY |
                                    LIVES_FIELD_FLAG_FREE_ON_DELETE, cdata, &cdata->priv,
                                    "priv", 0, NULL, NULL, NULL);
      specf[1] = make_special_field(LIVES_FIELD_CHARPTR, cdata, &cdata->URI,
                                    "URI", 0, NULL, NULL, NULL);
      specf[2] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->title,
                                    "title", 1024, NULL, NULL, NULL);
      specf[3] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->author,
                                    "author", 1024, NULL, NULL, NULL);
      specf[4] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->comment,
                                    "comment", 1024, NULL, NULL, NULL);
      specf[5] = make_special_field(LIVES_FIELD_ARRAY, cdata, &cdata->palettes,
                                    "palettes", 4, NULL, NULL, NULL);
      lives_struct_init(lsd, cdata, &cdata->lsd);
      lives_free(cdata);
    }
    break;
  case LIVES_STRUCT_FILE_DETS_T:
    lsd = lsd_create("lives_file_dets_t", sizeof(lives_file_dets_t), "widgets", 3);
    if (lsd) {
      lives_special_field_t **specf = lsd->special_fields;
      lives_file_dets_t *fdets = (lives_file_dets_t *)lives_calloc(1, sizeof(lives_file_dets_t));
      specf[0] = make_special_field(LIVES_FIELD_CHARPTR, fdets, &fdets->name,
                                    "name", 0, NULL, NULL, NULL);
      specf[1] = make_special_field(LIVES_FIELD_CHARPTR, fdets, &fdets->md5sum,
                                    "md5sum", 0, NULL, NULL, NULL);
      specf[2] = make_special_field(LIVES_FIELD_CHARPTR, fdets, &fdets->extra_details,
                                    "extra_details", 0, NULL, NULL, NULL);
      lives_struct_init_p(lsd, fdets, &fdets->lsd);
      lives_free(fdets);
    }
    break;
  default:
    return NULL;
  }
  if (lsd) {
    lives_struct_set_class_data((lives_struct_def_t *)lsd, CREATOR_ID);
    lsd_table[st_type] = lsd;
  }
  return lsd;
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


#define CHECK_VERBOSE 1
#if CHECK_VERBOSE
#define errprint(...) fprintf(stderr, __VA_ARGS__)
#else
#define errprint(...)
#endif

uint64_t lsd_check_struct(lives_struct_def_t *lsd) {
#if CHECK_VERBOSE
  uint64_t id, eid, uid;
#endif
  uint64_t err = 0;
  if (!lsd) {
    errprint("lsd_check: lsd1 is NULL\n");
    err |= (1ull << 0);
    return err;
  }

#if CHECK_VERBOSE

  /// non-error warnings
  id = lives_struct_get_identifier(lsd);

  if (id != LIVES_STRUCT_ID)
    errprint("lsd_check: lsd (%p) has non-standard identifier 0X%016lX\n", lsd, id);

  eid = lives_struct_get_end_id(lsd);

  if (eid != (LIVES_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non-standard end_id 0X%016lX\n", lsd, eid);

  if (eid != (id ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non matching identifier / end_id pair\n"
             "0X%016lX 0X%016lX should be 0X%016lX\n", lsd, id, eid, id ^ 0xFFFFFFFFFFFFFFFF);

  uid = lives_struct_get_uid(lsd);
  if (!uid)
    errprint("lsd_check: lsd (%p) has no unique_id\n", lsd);

  else if (uid < (1 << 20))
    errprint("lsd_check: lsd (%p) has unique_id 0X%016lX\n"
             "The probability of this is < 1 in 17.5 trillion\n", lsd, uid);

  if (lives_strcmp(lives_struct_get_class_data(lsd), CREATOR_ID))
    errprint("lsd_check: lsd (%p) has alternate class_data [%s]\n"
             "Ours is [%s]\n", lsd, (char *)lives_struct_get_class_data(lsd), CREATOR_ID);
#endif
  return err;
}

uint64_t lsd_check_match(lives_struct_def_t *lsd1, lives_struct_def_t *lsd2) {
  size_t sz1, sz2;
  uint64_t err = 0;
  if (!lsd1) {
    errprint("lsd_check: lsd1 is NULL\n");
    err |= (1ull << 0);
  }
  if (!lsd1) {
    errprint("lsd_check: lsd1 is NULL\n");
    err |= (1ull << 24);
  }
  if (err) return err;

  if (!lives_structs_same_type(lsd1, lsd2)) {
    errprint("lsd_check: lsd1 type is %s but lsd2 type is %s\n",
             lives_struct_get_type(lsd1), lives_struct_get_type(lsd2));
    err |= (1ull << 48);
  }

  sz1 = lives_struct_get_size(lsd1);
  sz2 = lives_struct_get_size(lsd2);
  if (sz1 != sz2) {
    errprint("lsd_check: lsd1 (%p) size is %lu but lsd2 (%p) size is %lu\n",
             lsd1, sz1, lsd2, sz2);
    if (sz1 > sz2) err |= (1ull << 49);
    else err |= (1ull << 50);
  }
  if (lives_strcmp(lives_struct_get_last_field(lsd1), lives_struct_get_last_field(lsd2))) {
    errprint("lsd_check: lsd1 (%p) last field [%s]\n"
             "is not the same as lsd2 (%p) last field [%s]\n",
             lsd1, lives_struct_get_last_field(lsd1),
             lsd2, lives_struct_get_last_field(lsd2));
    err |= (1ull << 51);
  }

  /// TODO - check special_fields and self_fields

  errprint("lsd_check: checking lsd1 (%p)\n", lsd1);
  err |= lsd_check_struct(lsd1);
  errprint("lsd_check: checking lsd2 (%p)\n", lsd2);
  err |= (lsd_check_struct(lsd2) << 24);

  return err;
}


/// bonus functions

char *weed_plant_to_header(weed_plant_t *plant, const char *tname) {
  char **leaves = weed_plant_list_leaves(plant, NULL);
  char *hdr, *ar = NULL, *line;

  if (tname)
    hdr  = lives_strdup("typedef struct {");
  else
    hdr = lives_strdup("struct {");

  for (int i = 0; leaves[i]; i++) {
    uint32_t st = weed_leaf_seed_type(plant, leaves[i]);
    weed_size_t ne = weed_leaf_num_elements(plant, leaves[i]);
    const char *tp = weed_seed_to_ctype(st, WEED_TRUE);
    if (ne > 1) ar = lives_strdup_printf("[%d]", ne);
    line = lives_strdup_printf("\n  %s%s%s;", tp, leaves[i], ar ? ar : "");
    hdr = lives_concat(hdr, line);
    if (ar) {
      lives_free(ar);
      ar = NULL;
    }
    lives_free(leaves[i]);
  }
  lives_free(leaves);

  if (!tname)
    line = lives_strdup("\n}");
  else
    line = lives_strdup_printf("\n} %s;", tname);
  lives_concat(hdr, line);
  return hdr;
}

