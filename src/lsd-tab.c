// lsd-tab.c
// LiVES
// (c) G. Finch 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling the LiVES Struct Def Table

#include "main.h"
#include "lsd.h"

static lives_hash_store_t *lsd_store = NULL;

#define CREATOR_ID "Created in LiVES main"

static void lfd_setdef(void *strct, const char *stype, const char *fname, int64_t *ptr) {*ptr = -1;}
static void adv_timing_init(void *strct, const char *stype, const char *fname, adv_timing_t *adv) {adv->ctiming_ratio = 1.;}

const lives_struct_def_t *lsd_from_store(lives_struct_type st_type) {
  const lives_struct_def_t *lsd = NULL;
  if (st_type >= LIVES_STRUCT_FIRST && st_type < LIVES_N_STRUCTS) {
    lsd = (const lives_struct_def_t *)get_from_hash_store_i(lsd_store, (uint64_t)st_type);
  }
  return lsd;
}

const lives_struct_def_t *get_lsd(lives_struct_type st_type) {
  const lives_struct_def_t *lsd;
  if (st_type < LIVES_STRUCT_FIRST || st_type >= LIVES_N_STRUCTS) return NULL;
  lsd = get_from_hash_store_i(lsd_store, (uint64_t)st_type);
  if (lsd) return lsd;

  switch (st_type) {
  case LIVES_STRUCT_CLIP_DATA_T:
    lsd = lsd_create("lives_clip_data_t", sizeof(lives_clip_data_t), "debug", 8);
    if (lsd) {
      lsd_special_field_t **specf = lsd->special_fields;
      lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
      specf[0] = make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY |
                                    LSD_FIELD_FLAG_FREE_ON_DELETE, cdata, &cdata->priv,
                                    "priv", 0, NULL, NULL, NULL);
      specf[1] = make_special_field(LSD_FIELD_CHARPTR, cdata, &cdata->URI,
                                    "URI", 0, NULL, NULL, NULL);
      specf[2] = make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->title,
                                    "title", 1024, NULL, NULL, NULL);
      specf[3] = make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->author,
                                    "author", 1024, NULL, NULL, NULL);
      specf[4] = make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->comment,
                                    "comment", 1024, NULL, NULL, NULL);
      specf[5] = make_special_field(LSD_FIELD_ARRAY, cdata, &cdata->palettes,
                                    "palettes", 4, NULL, NULL, NULL);
      specf[6] = make_special_field(LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY, cdata, &cdata->last_frame_decoded,
                                    "last_frame_decoded", 8, (lsd_field_init_cb)lfd_setdef, NULL, NULL);
      specf[7] = make_special_field(LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY, cdata, &cdata->adv_timing,
                                    "adv_timing", sizeof(adv_timing_t), (lsd_field_init_cb)adv_timing_init, NULL, NULL);
      lives_struct_init_p(lsd, cdata, &cdata->lsd);
      lives_free(cdata);
    }
    break;
  case LIVES_STRUCT_FILE_DETS_T:
    lsd = lsd_create("lives_file_dets_t", sizeof(lives_file_dets_t), "widgets", 3);
    if (lsd) {
      lsd_special_field_t **specf = lsd->special_fields;
      lives_file_dets_t *fdets = (lives_file_dets_t *)lives_calloc(1, sizeof(lives_file_dets_t));
      specf[0] = make_special_field(LSD_FIELD_CHARPTR, fdets, &fdets->name,
                                    "name", 0, NULL, NULL, NULL);
      specf[1] = make_special_field(LSD_FIELD_CHARPTR, fdets, &fdets->md5sum,
                                    "md5sum", 0, NULL, NULL, NULL);
      specf[2] = make_special_field(LSD_FIELD_CHARPTR, fdets, &fdets->extra_details,
                                    "extra_details", 0, NULL, NULL, NULL);
      lives_struct_init_p(lsd, fdets, &fdets->lsd);
      lives_free(fdets);
    }
    break;
  case LIVES_STRUCT_INTENTCAP_T: {
    lsd = lsd_create("lives_intentcap_t", sizeof(lives_intentcap_t), "lsd", 1);
    if (lsd) {
      lives_intentcap_t *icap = (lives_intentcap_t *)lives_calloc(1, sizeof(lives_intentcap_t));
      // caller will define special field 0, because ptrs to callbacks are needed
      lives_struct_init_p(lsd, icap, &icap->lsd);
      lives_free(icap);
    }
    break;
    default:
      return NULL;
    }
    if (lsd) {
      lives_struct_set_class_id((lives_struct_def_t *)lsd, CREATOR_ID);
      lsd_store = add_to_hash_store_i(lsd_store, (uint64_t)st_type, (void *)lsd);
    }
  }
  return lsd;
}

void *struct_from_template(lives_struct_type st_type) {
  const lives_struct_def_t *lsd = get_lsd(st_type);
  if (!lsd) return NULL;
  if (strcmp(lsd->self_fields[0]->name, "LSD")) abort();
  return lives_struct_create(lsd);
}

void *struct_from_template_inplace(lives_struct_type st_type, void *thestruct) {
  const lives_struct_def_t *lsd = get_lsd(st_type);
  if (!lsd) return NULL;
  return lives_struct_create_static(lsd, thestruct);
}


LIVES_GLOBAL_INLINE void *copy_struct(lives_struct_def_t *lsd) {
  if (lsd) {
    lives_struct_def_t *xlsd = lives_struct_copy(lsd);
    if (strcmp(xlsd->self_fields[0]->name, "LSD")) abort();
    return xlsd;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void unref_struct(lives_struct_def_t *lsd) {
  if (lsd) lives_struct_unref(lsd);
}


LIVES_GLOBAL_INLINE void ref_struct(lives_struct_def_t *lsd) {
  if (lsd) lives_struct_ref(lsd);
}


LIVES_GLOBAL_INLINE const char *lives_struct_get_creator(lives_struct_def_t *lsd) {
  if (lsd) return lives_struct_get_class_id(lsd);
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


//#define CHECK_VERBOSE 1
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

  if (id != LSD_STRUCT_ID)
    errprint("lsd_check: lsd (%p) has non-standard identifier 0X%016lX\n", lsd, id);

  eid = lives_struct_get_end_id(lsd);

  if (eid != (LSD_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non-standard end_id 0X%016lX\n", lsd, eid);

  if (eid != (id ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non matching identifier / end_id pair\n"
             "0X%016lX 0X%016lX should be 0X%016lX\n", lsd, id, eid, id ^ 0xFFFFFFFFFFFFFFFF);

  uid = lives_struct_get_uid(lsd);
  if (!uid) {
    errprint("lsd_check: lsd (%p) has no unique_id\n", lsd);
    break_me("no uid");
  } else if (uid < (1 << 20))
    errprint("lsd_check: lsd (%p) has unique_id 0X%016lX\n"
             "The probability of this is < 1 in 17.5 trillion\n", lsd, uid);

  if (lives_strcmp(lives_struct_get_class_id(lsd), CREATOR_ID))
    errprint("lsd_check: lsd (%p) has alternate class_id [%s]\n"
             "Ours is [%s]\n", lsd, (char *)lives_struct_get_class_id(lsd), CREATOR_ID);
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
