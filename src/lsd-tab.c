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
      lsd_add_special_field((lsd_struct_def_t *)lsd, "priv", LSD_FIELD_FLAG_ZERO_ON_COPY |
                            LSD_FIELD_FLAG_FREE_ON_DELETE, &cdata->priv, 0, cdata, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "URI", LSD_FIELD_CHARPTR, &cdata->URI,
                            0, cdata, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "title", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->title,
                            1024, cdata, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "author", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->author,
                            1024, cdata, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "comment", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->comment,
                            1024, cdata, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "palettes", LSD_FIELD_ARRAY, &cdata->palettes,
                            4, cdata, NULL);
      lives_free(cdata);
    }
    break;
  }
  case LIVES_STRUCT_FILE_DETS_T: {
    lives_file_dets_t *fdets = (lives_file_dets_t *)lives_calloc(1, sizeof(lives_file_dets_t));
    lsd = lsd_create_p("lives_file_dets_t", fdets, sizeof(lives_file_dets_t), &fdets->lsd);
    if (lsd) {
      lsd_add_special_field((lsd_struct_def_t *)lsd, "name", LSD_FIELD_CHARPTR, &fdets->name,
                            0, fdets, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "md5sum", LSD_FIELD_CHARPTR, &fdets->md5sum,
                            0, fdets, NULL);
      lsd_add_special_field((lsd_struct_def_t *)lsd, "extra_details", LSD_FIELD_CHARPTR, &fdets->extra_details,
                            0, fdets, NULL);
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


; LIVES_GLOBAL_INLINE void *copy_struct(lsd_struct_def_t *lsd) {
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


//#define CHECK_VERBOSE 1
#if CHECK_VERBOSE
#define errprint(...) _DW0(MSGMODE_ON(DEBUG); d_print_debug(__VA_ARGS__); MSGMODE_OFF(DEBUG);)
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

  if (id != LSD_STRUCT_ID) {
    errprint("lsd_check: lsd (%p) has non-standard identifier 0X%016lX\n", lsd, id);
    err |= (1ull << 1);
  }

  eid = lsd_struct_get_end_id(lsd);

  if (eid != (LSD_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF))
    errprint("lsd_check: lsd (%p) has non-standard end_id 0X%016lX\n", lsd, eid);

  if (eid != (id ^ 0xFFFFFFFFFFFFFFFF)) {
    errprint("lsd_check: lsd (%p) has non matching identifier / end_id pair\n"
             "0X%016lX 0X%016lX should be 0X%016lX\n", lsd, id, eid, id ^ 0xFFFFFFFFFFFFFFFF);
    err |= (1ull << 2);
  }

  uid = lsd_struct_get_uid(lsd);
  if (!uid) {
    errprint("lsd_check: lsd (%p) has no unique_id\n", lsd);
    break_me("no uid");
    err |= (1ull << 3);
  } else if (uid < (1 << 20)) {
    errprint("lsd_check: lsd (%p) has unique_id 0X%016lX\n"
             "The probability of this is < 1 in 17.5 trillion\n", lsd, uid);
    err |= (1ull << 4);
  }

  if (lives_strcmp(lsd_struct_get_class_id(lsd), CREATOR_ID))
    errprint("lsd_check: lsd (%p) has alternate class_id [%s]\n"
             "Ours is [%s] (not an error)\n", lsd, (char *)lsd_struct_get_class_id(lsd), CREATOR_ID);

#endif
  return err;
}

uint64_t lsd_check_match(lsd_struct_def_t *lsd1, lsd_struct_def_t *lsd2) {
  size_t sz1, sz2;
  uint64_t err = 0, uidiff;
#if CHECK_VERBOSE
  uint64_t own1, own2;
  errprint("lsd_check: comparing lsd1 (%p) and lsd2 (%p)\n", lsd1, lsd2);
#endif

  if (lsd1 == lsd2) {
    errprint("lsd_check: lsd1  == lsd2\n");
    err |= (1ull << 0);
  }
  if (!lsd1) {
    errprint("lsd_check: lsd1 is NULL\n");
    err |= (1ull << 1);
  }
  if (!lsd2) {
    errprint("lsd_check: lsd2 is NULL\n");
    err |= (1ull << 2);
  }
  if (err) {
    errprint("lsd_check: unable to continue checks. Exiting.\n");
    return err;
  }

  if (!lsd1->self_fields || strcmp(lsd1->self_fields->name, LSD_FIELD_LSD)) {
    errprint("lsd_check: lsd1 self fields corrupted");
    err |= (1ull << 8);
  }
  if (err) return err;

  if (!lsd2->self_fields || strcmp(lsd2->self_fields->name, LSD_FIELD_LSD)) {
    errprint("lsd_check: lsd2 self fields corrupted");
    err |= (1ull << 9);
  }
  if (err) {
    errprint("lsd_check: unable to continue checks. Exiting.\n");
    return err;
  }

  if (lsd_struct_get_uid(lsd1) >= lsd_struct_get_uid(lsd2))
    uidiff = lsd_struct_get_uid(lsd1) - lsd_struct_get_uid(lsd2);
  else
    uidiff = lsd_struct_get_uid(lsd2) - lsd_struct_get_uid(lsd1);

  if (!uidiff) {
    errprint("lsd1 uid == lsd2 uid, they should be different !\n");
    err |= (1ull << 16);
  } else if (uidiff < (1 << 20)) {
    errprint("lsd_check: difference between uid1 and uid2 is < (2 ^ 20)\n"
             "The probability of this is < 1 in 17.5 trillion\n");
    err |= (1ull << 17);
  }

  if (!lsd_structs_same_type(lsd1, lsd2)) {
    errprint("lsd_check: lsd1 type is %s but lsd2 type is %s\n",
             lsd_struct_get_type(lsd1), lsd_struct_get_type(lsd2));
    err |= (1ull << 18);
  }

  sz1 = lsd_struct_get_size(lsd1);
  sz2 = lsd_struct_get_size(lsd2);
  if (sz1 != sz2) {
    errprint("lsd_check: lsd1 (%p) size is %lu but lsd2 (%p) size is %lu\n",
             lsd1, sz1, lsd2, sz2);
    if (sz1 > sz2) err |= (1ull << 19);
    else err |= (1ull << 20);
  }

  /// TODO - check special_fields and self_fields

  errprint("lsd_check: checking lsd1 (%p)\n", lsd1);
  err |= (lsd_check_struct(lsd1) << 32);
  errprint("lsd_check: checking lsd2 (%p)\n", lsd2);
  err |= (lsd_check_struct(lsd2) << 48);

#if CHECK_VERBOSE
  own1 = lsd_struct_get_owner_uid(lsd1);
  own2 = lsd_struct_get_owner_uid(lsd2);

  errprint("lsd_check: lsd (%p) has owner 0x%016lX, and lsd2 (%p) has owner 0x%016lX "
           "(not an error)\n", lsd1, own1, lsd2, own2);
#endif
  return err;
}

