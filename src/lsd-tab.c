// lsd-tab.c
// LiVES
// (c) G. Finch 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling the LiVES Struct Def Table

#include "main.h"

static lives_hash_store_t *lsd_store = NULL;

#define CREATOR_ID "Created in LiVES main"

const lsd_struct_def_t *lsd_from_store(lives_struct_type st_type) {
  const lsd_struct_def_t *lsd = NULL;
  if (st_type >= LIVES_STRUCT_FIRST && st_type < LIVES_N_STRUCTS) {
    lsd = (const lsd_struct_def_t *)get_from_hash_store_i(lsd_store, (uint64_t)st_type);
  }
  return lsd;
}

const lsd_struct_def_t *get_lsd(lives_struct_type st_type) {
  const lsd_struct_def_t *lsd;
  if (st_type < LIVES_STRUCT_FIRST || st_type >= LIVES_N_STRUCTS) return NULL;
  lsd = get_from_hash_store_i(lsd_store, (uint64_t)st_type);
  if (lsd) return lsd;

  switch (st_type) {
  case LIVES_STRUCT_CLIP_DATA_T: {
    lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
    //LSD_CREATE_P(lsd, lives_clip_data_t);
    lsd = lsd_create_p("lives_clip_data_t", cdata, sizeof(lives_clip_data_t), &cdata->lsd);
    if (lsd) {
      lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
      add_special_field((lsd_struct_def_t *)lsd, "priv", LSD_FIELD_FLAG_ZERO_ON_COPY |
			LSD_FIELD_FLAG_FREE_ON_DELETE, &cdata->priv, 0, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "URI", LSD_FIELD_CHARPTR, &cdata->URI,
			0, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "title", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->title,
			1024, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "author", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->author,
			1024, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "comment", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->comment,
			1024, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "palettes", LSD_FIELD_ARRAY, &cdata->palettes,
			4, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "last_frame_decoded", LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY,
			&cdata->last_frame_decoded, 0, cdata, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "adv_timing", LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY, &cdata->adv_timing,
			0, cdata, NULL, NULL, NULL);
      lives_free(cdata);
    }
    break;
  }
  case LIVES_STRUCT_FILE_DETS_T: {
    lives_file_dets_t *fdets = (lives_file_dets_t *)lives_calloc(1, sizeof(lives_file_dets_t));
    lsd = lsd_create_p("lives_file_dets_t", fdets, sizeof(lives_file_dets_t), &fdets->lsd);
    if (lsd) {
      add_special_field((lsd_struct_def_t *)lsd, "name", LSD_FIELD_CHARPTR, &fdets->name,
			0, fdets, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "md5sum", LSD_FIELD_CHARPTR, &fdets->md5sum,
			0, fdets, NULL, NULL, NULL);
      add_special_field((lsd_struct_def_t *)lsd, "extra_details", LSD_FIELD_CHARPTR, &fdets->extra_details,
			0, fdets, NULL, NULL, NULL);
      lives_free(fdets);
    }
    break;
  }
  case LIVES_STRUCT_INTENTCAP_T: {
    lives_intentcap_t *icap = (lives_intentcap_t *)lives_calloc(1, sizeof(lives_intentcap_t));
    lsd = lsd_create_p("lives_intentcap_t", icap, sizeof(lives_intentcap_t), &icap->lsd);
    if (lsd) {
      // caller will define special field 0, because ptrs to callbacks are needed
      lives_free(icap);
    }
    break;
  }
  default:
    return NULL;
  }
  if (lsd) {
    lsd_struct_set_class_id((lsd_struct_def_t *)lsd, CREATOR_ID);
    lsd_store = add_to_hash_store_i(lsd_store, (uint64_t)st_type, (void *)lsd);
  }
  return lsd;
}

void *struct_from_template(lives_struct_type st_type) {
  const lsd_struct_def_t *lsd = get_lsd(st_type);
  //if (!lsd) return NULL;
  assert(lsd);
  return lsd_struct_create(lsd);
}

void *struct_from_template_inplace(lives_struct_type st_type, void *thestruct) {
  const lsd_struct_def_t *lsd = get_lsd(st_type);
  if (!lsd) return NULL;
  return lsd_struct_initialise(lsd, thestruct);
}


LIVES_GLOBAL_INLINE void *copy_struct(lsd_struct_def_t *lsd) {
  if (lsd) {
    lsd_struct_def_t *xlsd = lsd_struct_copy(lsd);
    return xlsd;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void unref_struct(lsd_struct_def_t *lsd) {
  if (lsd) lsd_struct_unref(lsd);
}


LIVES_GLOBAL_INLINE void ref_struct(lsd_struct_def_t *lsd) {
  if (lsd) lsd_struct_ref(lsd);
}


LIVES_GLOBAL_INLINE const char *lsd_struct_get_creator(lsd_struct_def_t *lsd) {
  if (lsd) return lsd_struct_get_class_id(lsd);
  return NULL;
}


LIVES_GLOBAL_INLINE boolean lsd_structs_equal(lsd_struct_def_t *lsd, lsd_struct_def_t *other) {
  if (lsd && other) return (lsd_struct_get_uid(lsd) == lsd_struct_get_uid(other));
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lsd_struct_is_a(lsd_struct_def_t *lsd, const char *st_type) {
  if (lsd) return (!lives_strcmp(lsd_struct_get_type(lsd), st_type));
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lsd_structs_same_type(lsd_struct_def_t *lsd,
    lsd_struct_def_t *other) {
  if (lsd && other) return lsd_struct_is_a(lsd, lsd_struct_get_type(other));
  return FALSE;
}


#define CHECK_VERBOSE 1
#if CHECK_VERBOSE
#define errprint(...) fprintf(stderr, __VA_ARGS__)
#else
#define errprint(...)
#endif

uint64_t lsd_check_struct(lsd_struct_def_t *lsd) {
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
  id = lsd_struct_get_identifier(lsd);

  if (id != LSD_STRUCT_ID)
    errprint("lsd_check: lsd (%p) has non-standard identifier 0X%016lX\n", lsd, id);

  eid = lsd_struct_get_end_id(lsd);

  if (eid != (LSD_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non-standard end_id 0X%016lX\n", lsd, eid);

  if (eid != (id ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non matching identifier / end_id pair\n"
             "0X%016lX 0X%016lX should be 0X%016lX\n", lsd, id, eid, id ^ 0xFFFFFFFFFFFFFFFF);

  uid = lsd_struct_get_uid(lsd);
  if (!uid) {
    errprint("lsd_check: lsd (%p) has no unique_id\n", lsd);
    break_me("no uid");
  } else if (uid < (1 << 20))
    errprint("lsd_check: lsd (%p) has unique_id 0X%016lX\n"
             "The probability of this is < 1 in 17.5 trillion\n", lsd, uid);

  if (lives_strcmp(lsd_struct_get_class_id(lsd), CREATOR_ID))
    errprint("lsd_check: lsd (%p) has alternate class_id [%s]\n"
             "Ours is [%s]\n", lsd, (char *)lsd_struct_get_class_id(lsd), CREATOR_ID);
#endif
  return err;
}

uint64_t lsd_check_match(lsd_struct_def_t *lsd1, lsd_struct_def_t *lsd2) {
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

  if (!lsd1->self_fields || strcmp(lsd1->self_fields->name, "LSD")) {
    errprint("lsd_check: lsd1 self fields corrupted");
    err |= (1ull << 25);
  }
  if (err) return err;

  if (!lsd2->self_fields || strcmp(lsd2->self_fields->name, "LSD")) {
    errprint("lsd_check: lsd2 self fields corrupted");
    err |= (1ull << 26);
  }
  if (err) return err;

  if (!lsd_structs_same_type(lsd1, lsd2)) {
    errprint("lsd_check: lsd1 type is %s but lsd2 type is %s\n",
             lsd_struct_get_type(lsd1), lsd_struct_get_type(lsd2));
    err |= (1ull << 48);
  }

  sz1 = lsd_struct_get_size(lsd1);
  sz2 = lsd_struct_get_size(lsd2);
  if (sz1 != sz2) {
    errprint("lsd_check: lsd1 (%p) size is %lu but lsd2 (%p) size is %lu\n",
             lsd1, sz1, lsd2, sz2);
    if (sz1 > sz2) err |= (1ull << 49);
    else err |= (1ull << 50);
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


#if 0
weed_plant_t *header_to_weed_plant(const char *fname, const char *sruct_type) {
  // we are looking for something like "} structtype;"
  // the work back from there to something like "typedef struct {"

  int bfd = lives_open_rdonly_buffered(fname);
  if (bfd >= 0) {
    char line[512];
    while (lives_buffered_readline(bfd, line, '\n', 512) > 0) {
      g_print("line is %s\n", line);
    }
  }
  lives_close_buffered(bfd);
}
#endif
