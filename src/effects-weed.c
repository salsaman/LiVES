// effects-weed.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2020 (salsaman+lives@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <dlfcn.h>

#ifdef __cplusplus
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV4)
#ifdef HAVE_OPENCV4
#include "opencv4/opencv2/core/core.hpp"
#else
#include "opencv2/core/core.hpp"
#endif
using namespace cv;
#endif
#endif

#include "main.h"
#include "effects.h"
#include "nodemodel.h"

#define calloc_bigblock(s) _calloc_bigblock(s)

///////////////////////////////////

#include "callbacks.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "ce_thumbs.h"
#include "paramwindow.h"
#include "diagnostics.h"

////////////////////////////////////////////////////////////////////////

// leaves which should be deleted when nullifying pixel_data
const char *PIXDATA_NULLIFY_LEAVES[] = {LIVES_LEAF_PIXEL_DATA_CONTIGUOUS,
                                        WEED_LEAF_HOST_ORIG_PDATA,
                                        LIVES_LEAF_PIXBUF_SRC,
                                        LIVES_LEAF_SURFACE_SRC,
                                        LIVES_LEAF_PROC_THREAD,
                                        LIVES_LEAF_PTHREAD_PTR,
                                        LIVES_LEAF_MD5SUM,
                                        LIVES_LEAF_MD5_CHKSIZE,
                                        NULL
                                       };

// additional leaves which should be removed / reset after copying
const char *NO_COPY_LEAVES[] = {LIVES_LEAF_REFCOUNTER,
                                LIVES_LEAF_NEW_ROWSTRIDES, LIVES_LEAF_COPYLIST, NULL
                               };

static int our_plugin_id = 0;
static weed_plant_t *expected_hi = NULL, *expected_pi = NULL;
static boolean suspect = FALSE;
static char *fxname;
static int ncbcalls = 0;

/* static int thcount = 0; */
/* static int nthcount = 0; */
/* static int64_t totth = 0; */
/* static int64_t totnth = 0; */
/* static boolean thrdit = TRUE; */

static boolean fx_inited = FALSE;

#ifndef VALGRIND_ON
static int load_compound_fx(void);
#endif

#if 0
LIVES_LOCAL_INLINE int weed_inst_refs_count(weed_plant_t *inst) {
  return weed_refcount_query(inst);
}
#endif
////////////////////////////////////////////////////////////////////////////

static boolean is_pixdata_nullify_leaf(const char *key) {
  for (int i = 0; PIXDATA_NULLIFY_LEAVES[i]; i++)
    if (!lives_strcmp(key, PIXDATA_NULLIFY_LEAVES[i]))
      return TRUE;
  return FALSE;
}


static boolean is_no_copy_leaf(const char *key) {
  for (int i = 0; NO_COPY_LEAVES[i]; i++)
    if (!lives_strcmp(key, NO_COPY_LEAVES[i])) return TRUE;
  return FALSE;
}

weed_error_t lives_leaf_copy(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf) {
  // force overrride of IMMUTABLE dest leaves
  if (is_pixdata_nullify_leaf(keyf) || is_no_copy_leaf(keyf)) return WEED_SUCCESS;

  weed_flags_t flags = weed_leaf_get_flags(src, keyf);
  if (flags & LIVES_FLAG_FREE_ON_DELETE) {
    g_print("copying autofree val %s\n", keyf);
    abort();
  }

  flags = weed_leaf_get_flags(dst, keyt);
  if (flags & LIVES_FLAG_FREE_ON_DELETE) {
    weed_leaf_set_flags(dst, keyt, flags & ~WEED_FLAG_UNDELETABLE);
    weed_leaf_autofree(dst, keyt);
  }

  if (flags & WEED_FLAG_IMMUTABLE) weed_leaf_set_flags(dst, keyt, flags & ~WEED_FLAG_IMMUTABLE);
  weed_error_t err = weed_leaf_copy(dst, keyt, src, keyf);
  return err;
}


weed_error_t lives_leaf_copy_nth(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf, int n) {
  // force overrride of IMMUTABLE dest leaves
  weed_flags_t flags = weed_leaf_get_flags(dst, keyt);
  if (flags & WEED_FLAG_IMMUTABLE) weed_leaf_set_flags(dst, keyt, flags & ~WEED_FLAG_IMMUTABLE);
  weed_error_t err = weed_leaf_copy_nth(dst, keyt, src, keyf, n);
  if (flags & WEED_FLAG_IMMUTABLE) weed_leaf_set_flags(dst, keyt, flags);
  return err;
}


static weed_error_t _lives_leaf_dup(weed_plant_t *dst, weed_plant_t *src, const char *key, boolean lax) {
  // force overrride of IMMUTABLE dest leaves
  if (!lax && (is_pixdata_nullify_leaf(key) || is_no_copy_leaf(key))) return WEED_SUCCESS;
  weed_flags_t flags = weed_leaf_get_flags(dst, key);

  if (flags & LIVES_FLAG_FREE_ON_DELETE) {
    weed_leaf_set_flags(dst, key, flags & ~WEED_FLAG_UNDELETABLE);
    weed_leaf_autofree(dst, key);
  }
  if (flags & WEED_FLAG_IMMUTABLE) weed_leaf_set_flags(dst, key, flags & ~WEED_FLAG_IMMUTABLE);
  weed_error_t err = weed_leaf_dup(dst, src, key);
  return err;
}

weed_error_t lives_leaf_dup(weed_plant_t *dst, weed_plant_t *src, const char *key) {
  return _lives_leaf_dup(dst, src, key, FALSE);
}

weed_error_t lives_leaf_dup_nocheck(weed_plant_t *dst, weed_plant_t *src, const char *key) {
  return _lives_leaf_dup(dst, src, key, TRUE);
}


LIVES_GLOBAL_INLINE boolean lives_leaf_copy_or_delete(weed_layer_t *dlayer, const char *key, weed_layer_t *slayer) {
  if (!weed_plant_has_leaf(slayer, key)) {
    weed_leaf_delete(dlayer, key);
    return FALSE;
  } else lives_leaf_dup_nocheck(dlayer, slayer, key); // lives_leaf_dup forces overrride of IMMUTABLE dest leaves
  return TRUE;
}


void weed_functions_init(void) {
  int winitopts = 0;
  weed_error_t werr;
#ifndef IS_LIBLIVES
  // start up the Weed system
  weed_abi_version = libweed_get_abi_version();
  if (weed_abi_version > WEED_ABI_VERSION) weed_abi_version = WEED_ABI_VERSION;
#ifdef WEED_STARTUP_TESTS
  winitopts |= WEED_INIT_DEBUGMODE;
  test_opts |= TEST_WEED;
#endif
  winitopts |= (1ull << 33); // skip un-needed error checks
  winitopts |= WEED_INIT_EXTENDED_FUNCS;
  werr = libweed_init(weed_abi_version, winitopts);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }

#if !USE_STD_MEMFUNCS
#if USE_RPMALLOC
  libweed_set_memory_funcs(_ext_malloc, _ext_free, _ext_calloc);
#else
#ifdef xENABLE_GSLICE
#if GLIB_CHECK_VERSION(2, 14, 0)
  libweed_set_slab_funcs(lives_slice_alloc0, lives_slice_unalloc, lives_slice_alloc_and_copy,
                         _lives_memcpy);
#else
  libweed_set_slab_funcs(lives_slice_alloc0, lives_slice_unalloc, NULL, _lives_memcpy);
#endif
#else
  libweed_set_memory_funcs(default_malloc, default_free, default_calloc);
#endif // DISABLE_GSLICE

#endif // USE_RPMALLOC
  weed_utils_set_custom_memfuncs(lives_malloc, lives_calloc, lives_memcpy, NULL, lives_free);
#endif // USE_STD_MEMFUNCS
#endif //IS_LIBLIVES

  // backup the core functions so we can override them
  _weed_plant_new = weed_plant_new;
  _weed_plant_free = weed_plant_free;
  _weed_leaf_set = weed_leaf_set;
  _weed_leaf_get = weed_leaf_get;
  _weed_leaf_delete = weed_leaf_delete;
  _weed_plant_list_leaves = weed_plant_list_leaves;
  _weed_leaf_num_elements = weed_leaf_num_elements;
  _weed_leaf_element_size = weed_leaf_element_size;

#if WEED_ABI_CHECK_VERSION(203)
  _weed_ext_set_element_size = weed_ext_set_element_size;
  _weed_ext_append_elements = weed_ext_append_elements;
  _weed_ext_attach_leaf = weed_ext_attach_leaf;
  _weed_ext_detach_leaf = weed_ext_detach_leaf;
  _weed_ext_atomic_exchange = weed_ext_atomic_exchange;
#endif

  _weed_leaf_seed_type = weed_leaf_seed_type;
  _weed_leaf_get_flags = weed_leaf_get_flags;
  _weed_leaf_set_flags = weed_leaf_set_flags;
}


LIVES_GLOBAL_INLINE int filter_mutex_lock(int key) {
  if (key >= 0 && key < FX_KEYS_MAX_VIRTUAL) {
    pthread_mutex_lock(&mainw->fx_key_mutex[key]);
    if (mainw->fx_mutex_tuid[key] == 0) {
      mainw->fx_mutex_tuid[key] = THREADVAR(uid);
      mainw->fx_mutex_nlocks[key] = 0;
    }
    mainw->fx_mutex_nlocks[key]++;
  } else {
    BREAK_ME("badlock");
    char *msg = lives_strdup_printf("attempted lock of bad fx key %d", key);
    LIVES_ERROR(msg);
    lives_free(msg);
  }
  return 0;
}


LIVES_GLOBAL_INLINE int filter_mutex_trylock(int key) {
  if (key >= 0 && key < FX_KEYS_MAX_VIRTUAL) {
    return pthread_mutex_trylock(&mainw->fx_key_mutex[key]);
  } else {
    char *msg = lives_strdup_printf("attempted lock of bad fx key %d", key);
    LIVES_ERROR(msg);
    lives_free(msg);
  }
  return 0;
}

LIVES_GLOBAL_INLINE int filter_mutex_unlock(int key) {
  if (key >= 0 && key < FX_KEYS_MAX_VIRTUAL) {
    if (mainw->fx_mutex_tuid[key] == THREADVAR(uid)
        && mainw->fx_mutex_nlocks[key]) {
      mainw->fx_mutex_nlocks[key]--;
      if (!mainw->fx_mutex_nlocks[key]) {
        mainw->fx_mutex_tuid[key] = 0;
      }
      pthread_mutex_unlock(&mainw->fx_key_mutex[key]);
      return 0;
    }
    return 1;
  } else {
    char *msg = lives_strdup_printf("attempted unlock of bad fx key %d", key);
    LIVES_ERROR(msg);
    lives_free(msg);
  }
  return 0;
}


//////////////////////////////////////////////////////////////////////////////
// filter library functions

int num_compound_fx(weed_plant_t *plant) {
  weed_plant_t *filter;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter = weed_instance_get_filter(plant, TRUE);
  else filter = plant;

  if (!weed_plant_has_leaf(filter, WEED_LEAF_HOST_FILTER_LIST)) return 1;
  return weed_leaf_num_elements(filter, WEED_LEAF_HOST_FILTER_LIST);
}


boolean has_non_alpha_palette(weed_plant_t *ctmpl, weed_plant_t *filter) {
  int *plist;
  int npals = 0;

  plist = weed_chantmpl_get_palette_list(filter, ctmpl, &npals);
  if (!plist) return TRUE; ///< probably audio
  for (int i = 0; i < npals; i++) {
    if (plist[i] == WEED_PALETTE_END) break;
    if (!weed_palette_is_alpha(plist[i])) {
      lives_free(plist);
      return TRUE;
    }
  }
  lives_free(plist);
  return FALSE;
}


boolean has_alpha_palette(weed_plant_t *ctmpl, weed_plant_t *filter) {
  int *plist;
  int npals = 0;
  plist = weed_chantmpl_get_palette_list(filter, ctmpl, &npals);
  for (int i = 0; i < npals; i++) {
    if (weed_palette_is_alpha(plist[i])) {
      if (palette == WEED_PALETTE_NONE) continue;
      lives_free(plist);
      return TRUE;
    }
  }
  lives_free(plist);
  return FALSE;
}


weed_plant_t *weed_instance_get_filter(weed_plant_t *inst, boolean get_compound_parent) {
  if (get_compound_parent &&
      (weed_plant_has_leaf(inst, WEED_LEAF_HOST_COMPOUND_CLASS)))
    return weed_get_plantptr_value(inst, WEED_LEAF_HOST_COMPOUND_CLASS, NULL);
  return weed_get_plantptr_value(inst, WEED_LEAF_FILTER_CLASS, NULL);
}


LIVES_GLOBAL_INLINE boolean weed_channel_is_alpha(weed_channel_t *chan) {
  return (chan && weed_palette_is_alpha(weed_channel_get_palette(chan)));
}


static boolean all_outs_alpha(weed_plant_t *filt, boolean ign_opt) {
  // check (mandatory) output chans, see if any are non-alpha
  int nouts;
  weed_plant_t **ctmpls = weed_filter_get_out_chantmpls(filt, &nouts);
  if (nouts <= 0) return FALSE;
  if (!ctmpls[0]) {
    lives_freep((void **)&ctmpls);
    return FALSE;
  }
  for (int i = 0; i < nouts; i++) {
    if (ign_opt && weed_chantmpl_is_optional(ctmpls[i])) continue;
    if (has_non_alpha_palette(ctmpls[i], filt)) {
      lives_free(ctmpls);
      return FALSE;
    }
  }
  lives_freep((void **)&ctmpls);
  return TRUE;
}


static boolean all_ins_alpha(weed_plant_t *filt, boolean ign_opt) {
  // check mandatory input chans, see if any are non-alpha
  // if there are no mandatory inputs, we check optional (even if ign_opt is TRUE)
  boolean has_mandatory_in = FALSE;
  int nins;
  weed_plant_t **ctmpls = weed_filter_get_in_chantmpls(filt, &nins);
  if (nins <= 0) return FALSE;
  if (!ctmpls[0]) {
    lives_free(ctmpls);
    return FALSE;
  }
  for (int i = 0; i < nins; i++) {
    if (ign_opt && weed_chantmpl_is_optional(ctmpls[i])) continue;
    has_mandatory_in = TRUE;
    if (has_non_alpha_palette(ctmpls[i], filt)) {
      lives_free(ctmpls);
      return FALSE;
    }
  }
  if (!has_mandatory_in) {
    for (int i = 0; i < nins; i++) {
      if (has_non_alpha_palette(ctmpls[i], filt)) {
        lives_free(ctmpls);
        return FALSE;
      }
    }
    lives_free(ctmpls);
    return TRUE;
  }
  lives_free(ctmpls);
  return FALSE;
}


lives_fx_cat_t weed_filter_categorise(weed_plant_t *pl, int in_channels, int out_channels) {
  weed_plant_t *filt = pl;

  boolean has_out_params = FALSE;
  boolean has_in_params = FALSE;
  boolean all_out_alpha = TRUE;
  boolean all_in_alpha = TRUE;
  boolean has_in_alpha = FALSE;

  int filter_flags;

  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt = weed_instance_get_filter(pl, TRUE);

  all_out_alpha = all_outs_alpha(filt, TRUE);
  all_in_alpha = all_ins_alpha(filt, TRUE);

  filter_flags = weed_get_int_value(filt, WEED_LEAF_FLAGS, NULL);
  if (weed_plant_has_leaf(filt, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) has_out_params = TRUE;
  if (weed_plant_has_leaf(filt, WEED_LEAF_IN_PARAMETER_TEMPLATES)) has_in_params = TRUE;
  if (filter_flags & WEED_FILTER_IS_CONVERTER) return LIVES_FX_CAT_CONVERTER;
  if (in_channels == 0 && out_channels > 0 && all_out_alpha) return LIVES_FX_CAT_DATA_GENERATOR;
  if (in_channels == 0 && out_channels > 0) {
    if (!has_audio_chans_out(filt, TRUE)) return LIVES_FX_CAT_VIDEO_GENERATOR;
    else if (has_video_chans_out(filt, TRUE)) return LIVES_FX_CAT_AV_GENERATOR;
    else return LIVES_FX_CAT_AUDIO_GENERATOR;
  }
  if (out_channels >= 1 && in_channels >= 1 && (all_in_alpha || has_in_alpha) && !all_out_alpha)
    return LIVES_FX_CAT_DATA_VISUALISER;
  if (out_channels >= 1 && all_out_alpha) return LIVES_FX_CAT_ANALYSER;
  if (out_channels > 1) return LIVES_FX_CAT_SPLITTER;

  if (in_channels > 2 && out_channels == 1) {
    return LIVES_FX_CAT_COMPOSITOR;
  }
  if (in_channels == 2 && out_channels == 1) return LIVES_FX_CAT_TRANSITION;
  if (in_channels == 1 && out_channels == 1 && !(has_video_chans_in(filt, TRUE)) &&
      !(has_video_chans_out(filt, TRUE))) return LIVES_FX_CAT_AUDIO_EFFECT;
  if (in_channels == 1 && out_channels == 1) return LIVES_FX_CAT_EFFECT;
  if (in_channels > 0 && out_channels == 0 && has_out_params) return LIVES_FX_CAT_ANALYSER;
  if (in_channels > 0 && out_channels == 0) return LIVES_FX_CAT_TAP;
  if (in_channels == 0 && out_channels == 0 && has_out_params && has_in_params) return LIVES_FX_CAT_DATA_PROCESSOR;
  if (in_channels == 0 && out_channels == 0 && has_out_params) return LIVES_FX_CAT_DATA_SOURCE;
  if (in_channels == 0 && out_channels == 0) return LIVES_FX_CAT_UTILITY;
  return LIVES_FX_CAT_NONE;
}


lives_fx_cat_t weed_filter_subcategorise(weed_plant_t *pl, lives_fx_cat_t category, boolean count_opt) {
  weed_plant_t *filt = pl;
  boolean has_video_chansi;

  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt = weed_instance_get_filter(pl, TRUE);

  if (category == LIVES_FX_CAT_COMPOSITOR) count_opt = TRUE;

  has_video_chansi = has_video_chans_in(filt, count_opt);

  if (category == LIVES_FX_CAT_TRANSITION) {
    if (get_transition_param(filt, FALSE) != -1) {
      if (!has_video_chansi) return LIVES_FX_CAT_AUDIO_TRANSITION;
      return LIVES_FX_CAT_AV_TRANSITION;
    }
    return LIVES_FX_CAT_VIDEO_TRANSITION;
  }

  if (category == LIVES_FX_CAT_COMPOSITOR && !has_video_chansi) return LIVES_FX_CAT_AUDIO_MIXER;
  if (category == LIVES_FX_CAT_EFFECT && !has_video_chansi) return LIVES_FX_CAT_AUDIO_EFFECT;
  if (category == LIVES_FX_CAT_CONVERTER && !has_video_chansi) return LIVES_FX_CAT_AUDIO_VOL;

  if (category == LIVES_FX_CAT_ANALYSER) {
    if (!has_video_chansi) return LIVES_FX_CAT_AUDIO_ANALYSER;
    return LIVES_FX_CAT_VIDEO_ANALYSER;
  }

  return LIVES_FX_CAT_NONE;
}


int num_alpha_channels(weed_plant_t *filter, boolean out) {
  // get number of alpha channels (in or out) for filter; including optional
  weed_plant_t **ctmpls;
  int count = 0, nchans;
  int i;

  if (out) {
    ctmpls = weed_filter_get_out_chantmpls(filter, &nchans);
    if (!ctmpls) return FALSE;
    for (i = 0; i < nchans; i++) {
      if (has_non_alpha_palette(ctmpls[i], filter)) continue;
      count++;
    }
  } else {
    ctmpls = weed_filter_get_in_chantmpls(filter, &nchans);
    if (!ctmpls) return FALSE;
    for (i = 0; i < nchans; i++) {
      if (has_non_alpha_palette(ctmpls[i], filter)) continue;
      count++;
    }
  }
  lives_freep((void **)&ctmpls);
  return count;
}


////////////////////////////////////////////////////////////////////////

#define MAX_WEED_INSTANCES 65536

// store keys so we now eg, which rte mask entry to xor when deiniting
static int fg_generator_key;
static int bg_generator_key;

// store modes too
static int fg_generator_mode;
static int bg_generator_mode;

// generators to start on playback, because of a problem in libvisual
// - we must start generators after starting audio
static int bg_gen_to_start;
static int fg_gen_to_start;

// store the clip, this can sometimes get lost
static int fg_generator_clip;

//////////////////////////////////////////////////////////////////////

static weed_plant_t **weed_filters; // array of filter_classes

static LiVESList *weed_fx_sorted_list;

// each 'hotkey' controls n instances, selectable as 'modes' or banks
static weed_plant_t **key_to_instance[FX_KEYS_MAX];
static weed_plant_t **key_to_instance_copy[FX_KEYS_MAX]; // copy for preview during rendering

static int *key_to_fx[FX_KEYS_MAX];
static int key_modes[FX_KEYS_MAX];

static int num_weed_filters; ///< count of how many filters we have loaded


typedef struct {
  char *string;
  uint32_t hash;
} lives_hashentry;

#define NHASH_TYPES 8

typedef lives_hashentry  lives_hashjoint[NHASH_TYPES];

// 0 is TF, 1 is TT, 2 is FF, 3 is FT // then possibly repeated with spaces substituted with '_'
static lives_hashjoint *hashnames;

// per key/mode parameter defaults
// *INDENT-OFF*
weed_plant_t ***key_defaults[FX_KEYS_MAX_VIRTUAL];
// *INDENT-ON*

/////////////////// LiVES event system /////////////////

static weed_plant_t *init_events[FX_KEYS_MAX_VIRTUAL];
static void **pchains[FX_KEYS_MAX]; // parameter changes, used during recording (not for rendering)
static weed_plant_t *effects_map[FX_KEYS_MAX + 2]; // ordered list of init_events
static int next_free_key;

////////////////////////////////////////////////////////////////////


void backup_weed_instances(void) {
  // this is called during multitrack rendering.
  // We are rendering, but we want to display the current frame in the preview window
  // thus we backup our rendering instances, apply the current frame instances, and then restore the rendering instances
  for (int i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    key_to_instance_copy[i][0] = key_to_instance[i][0];
    key_to_instance[i][0] = NULL;
  }
}


void restore_weed_instances(void) {
  for (int i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    key_to_instance[i][0] = key_to_instance_copy[i][0];
  }
}


static char *weed_filter_get_type(weed_plant_t *filter, boolean getsub, boolean format) {
  // return value should be lives_free'd after use
  char *tmp1, *tmp2, *ret;
  lives_fx_cat_t cat = weed_filter_categorise(filter,
                       enabled_in_channels(filter, TRUE),
                       enabled_out_channels(filter, TRUE));

  lives_fx_cat_t sub = weed_filter_subcategorise(filter, cat, FALSE);

  if (!getsub || sub == LIVES_FX_CAT_NONE)
    return lives_fx_cat_to_text(cat, FALSE);

  tmp1 = lives_fx_cat_to_text(cat, FALSE);
  tmp2 = lives_fx_cat_to_text(sub, FALSE);

  if (format) ret = lives_strdup_printf("%s (%s)", tmp1, tmp2);
  else {
    if (cat == LIVES_FX_CAT_TRANSITION) ret = lives_strdup_printf("%s - %s", tmp1, tmp2);
    else ret = lives_strdup_printf("%s", tmp2);
  }

  lives_free(tmp1);
  lives_free(tmp2);

  return ret;
}


void update_host_info(weed_plant_t *hinfo) {
  // set "host_audio_plugin" in the host_info
  switch (prefs->audio_player) {
  case AUD_PLAYER_JACK:
    weed_set_string_value(hinfo, WEED_LEAF_HOST_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
    break;
  case AUD_PLAYER_PULSE:
    weed_set_string_value(hinfo, WEED_LEAF_HOST_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
    break;
  case AUD_PLAYER_NONE:
    weed_set_string_value(hinfo, WEED_LEAF_HOST_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
    break;
  }
}


void update_all_host_info(void) {
  // update host_info for all loaded filters
  // we should call this when any of the relevant data changes
  // (for now it's just the audio player)
  weed_plant_t *filter, *hinfo, *pinfo;
  int i;
  for (i = 0; i < num_weed_filters; i++) {
    filter = weed_filters[i];
    hinfo = weed_get_plantptr_value(filter, WEED_LEAF_HOST_INFO, NULL);
    if (!hinfo) {
      pinfo = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
      if (pinfo)
        hinfo = weed_get_plantptr_value(pinfo, WEED_LEAF_HOST_INFO, NULL);
    }
    if (hinfo && WEED_PLANT_IS_HOST_INFO(hinfo)) update_host_info(hinfo);
  }
}


static weed_plant_t *get_enabled_channel_inner(weed_plant_t *inst, int which,
    int direction, boolean audio_only) {
  // plant is a filter_instance
  // "which" starts at 0
  // direction may be LIVES_INPUT or LIVES_OUTPUT
  weed_plant_t **channels = NULL;
  weed_plant_t *retval, *ctmpl = NULL;

  int nchans = 3;

  int i = 0;

  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return NULL;

  if (direction == LIVES_INPUT)
    channels = weed_instance_get_in_channels(inst, &nchans);
  else
    channels = weed_instance_get_out_channels(inst, &nchans);

  if (!nchans) return NULL;

  while (1) {
    if (!weed_channel_is_disabled(channels[i])) {
      if (audio_only) ctmpl = weed_channel_get_template(channels[i]);
      if (!audio_only || (audio_only && weed_chantmpl_is_audio(ctmpl) == WEED_TRUE)) {
        which--;
      }
    }
    if (which < 0) break;
    if (++i >= nchans) {
      lives_free(channels);
      return NULL;
    }
  }
  retval = channels[i];
  lives_freep((void **)&channels);
  return retval;
}


weed_plant_t *get_enabled_channel(weed_plant_t *inst, int which, int direction) {
  // count audio and video channels
  return get_enabled_channel_inner(inst, which, direction, FALSE);
}


weed_plant_t *get_enabled_audio_channel(weed_plant_t *inst, int which, int direction) {
  // count only audio channels
  return get_enabled_channel_inner(inst, which, direction, TRUE);
}


weed_plant_t *get_mandatory_channel(weed_plant_t *filter, int which, int direction) {
  // plant is a filter_class
  // "which" starts at 0
  weed_plant_t **ctmpls;
  weed_plant_t *retval;
  int i;

  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;

  if (direction == LIVES_INPUT) ctmpls = weed_filter_get_in_chantmpls(filter, NULL);
  else ctmpls = weed_filter_get_out_chantmpls(filter, NULL);

  if (!ctmpls) return NULL;
  for (i = 0; which > -1; i++) {
    if (!weed_chantmpl_is_optional(ctmpls[i])) which--;
  }
  retval = ctmpls[i - 1];
  lives_free(ctmpls);
  return retval;
}


LIVES_GLOBAL_INLINE boolean weed_instance_is_resizer(weed_plant_t *inst) {
  weed_plant_t *ftmpl = weed_instance_get_filter(inst, TRUE);
  return weed_filter_is_resizer(ftmpl);
}


boolean is_audio_channel_in(weed_plant_t *inst, int chnum) {
  int nchans = 0;
  weed_plant_t **in_chans;
  weed_plant_t *ctmpl;

  in_chans = weed_instance_get_in_channels(inst, &nchans);
  if (nchans <= chnum) {
    lives_freep((void **)&in_chans);
    return FALSE;
  }

  ctmpl = weed_channel_get_template(in_chans[chnum]);
  lives_free(in_chans);

  return (weed_chantmpl_is_audio(ctmpl) == WEED_TRUE);
}


weed_plant_t *get_audio_channel_in(weed_plant_t *inst, int achnum) {
  // get nth audio channel in (not counting video channels)
  int nchans = 0;
  weed_plant_t **in_chans;
  weed_plant_t *ctmpl, *achan;

  in_chans = weed_instance_get_in_channels(inst, &nchans);
  if (!in_chans) return NULL;

  for (int i = 0; i < nchans; i++) {
    ctmpl = weed_channel_get_template(in_chans[i]);
    if (weed_chantmpl_is_audio(ctmpl)) {
      if (achnum-- == 0) {
        achan = in_chans[i];
        lives_free(in_chans);
        return achan;
      }
    }
  }

  lives_freep((void **)&in_chans);
  return NULL;
}


boolean has_video_chans_in(weed_plant_t *filter, boolean count_opt) {
  // TODO: we should probably distinguish between enabled and disabled optional channels
  int nchans = 0;
  weed_plant_t **in_ctmpls;

  in_ctmpls = weed_filter_get_in_chantmpls(filter, &nchans);
  if (!in_ctmpls) return FALSE;

  for (int i = 0; i < nchans; i++) {
    if (!count_opt && weed_chantmpl_is_optional(in_ctmpls[i])) continue;
    if (weed_chantmpl_is_audio(in_ctmpls[i])) continue;
    lives_free(in_ctmpls);
    return TRUE;
  }
  lives_freep((void **)&in_ctmpls);

  return FALSE;
}


boolean has_audio_chans_in(weed_plant_t *filter, boolean count_opt) {
  int nchans = 0;
  weed_plant_t **in_ctmpls = weed_filter_get_in_chantmpls(filter, &nchans);
  if (!in_ctmpls) return FALSE;
  for (int i = 0; i < nchans; i++) {
    if (!count_opt && weed_chantmpl_is_optional(in_ctmpls[i])) continue;
    if (weed_chantmpl_is_audio(in_ctmpls[i])) {
      lives_free(in_ctmpls);
      return TRUE;
    }
  }
  lives_freep((void **)&in_ctmpls);
  return FALSE;
}


boolean is_audio_channel_out(weed_plant_t *inst, int chnum) {
  weed_plant_t **out_chans;
  weed_plant_t *ctmpl;
  int nchans = 0;

  out_chans = weed_instance_get_in_channels(inst, &nchans);
  if (nchans <= chnum) {
    lives_freep((void **)&out_chans);
    return FALSE;
  }
  ctmpl = weed_channel_get_template(out_chans[chnum]);
  lives_free(out_chans);

  if (weed_chantmpl_is_audio(ctmpl) == WEED_TRUE) {
    return TRUE;
  }
  return FALSE;
}


boolean has_video_chans_out(weed_plant_t *filter, boolean count_opt) {
  weed_plant_t **out_ctmpls;
  int nchans = 0;

  out_ctmpls = weed_filter_get_out_chantmpls(filter, &nchans);
  if (!out_ctmpls) return FALSE;

  for (int i = 0; i < nchans; i++) {
    if (!count_opt && weed_chantmpl_is_optional(out_ctmpls[i])) continue;
    if (weed_chantmpl_is_audio(out_ctmpls[i]) == WEED_TRUE) continue;
    lives_free(out_ctmpls);
    return TRUE;
  }
  lives_freep((void **)&out_ctmpls);

  return FALSE;
}


boolean has_audio_chans_out(weed_plant_t *filter, boolean count_opt) {
  weed_plant_t **out_ctmpls;
  int nchans = 0;

  out_ctmpls = weed_filter_get_out_chantmpls(filter, &nchans);
  if (!out_ctmpls) return FALSE;

  for (int i = 0; i < nchans; i++) {
    if (!count_opt && weed_chantmpl_is_optional(out_ctmpls[i])) continue;
    if (weed_chantmpl_is_audio(out_ctmpls[i]) == WEED_FALSE) continue;
    lives_free(out_ctmpls);
    return TRUE;
  }
  lives_freep((void **)&out_ctmpls);

  return FALSE;
}


boolean is_pure_audio(weed_plant_t *plant, boolean count_opt) {
  weed_plant_t *filter = plant;
  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter = weed_instance_get_filter(plant, FALSE);

  if ((has_audio_chans_in(filter, count_opt) || has_audio_chans_out(filter, count_opt)) &&
      !has_video_chans_in(filter, count_opt) && !has_video_chans_out(filter, count_opt)) return TRUE;
  return FALSE;
}


boolean weed_parameter_has_variable_elements_strict(weed_plant_t *inst, weed_plant_t *ptmpl) {
  /** see if param has variable elements, using the strictest check */
  weed_channel_t **chans;
  weed_chantmpl_t *ctmpl;
  int flags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, NULL);
  int nchans;

  if (flags & WEED_PARAMETER_VARIABLE_SIZE) return TRUE;

  if (!inst) return FALSE;

  if (!(flags & WEED_PARAMETER_VALUE_PER_CHANNEL)) return FALSE;

  chans = weed_instance_get_in_channels(inst, &nchans);

  if (!nchans) return FALSE;

  for (int i = 0; i < nchans; i++) {
    if (weed_channel_is_disabled(chans[i])) continue; //ignore disabled channels
    ctmpl = weed_channel_get_template(chans[i]);
    if (weed_chantmpl_get_max_repeats(ctmpl) > 1) {
      lives_free(chans);
      return TRUE;
    }
  }

  lives_freep((void **)&chans);
  return FALSE;
}


/**
   @brief create an effect (filter) map (during recording)

   here we create an effect map which defines the order in which effects are applied to a frame stack
   this is done during recording, the keymap is from mainw->rte which is a bitmap of effect keys
   keys are checked here from smallest (ctrl-1) to largest (FX_KEYS_MAX_VIRTUAL)

   what we actually point to are the init_events for the effects. The init_events are stored when we
   init an effect during recording

   these arrays are then used as a template to create a filter_map event which is appended_to the event list

   separate arrays are created for audio and video threads, this is because the audio thread events do not
   get quantised to video frames, hence they may be out of order. After playback we reorder these events
   and merge the two sets of filter maps in event_list_reorder_noquant()

   during preview / rendering we read the filter_map event, and retrieve the new key, which is at that time
   held in the WEED_LEAF_HOST_TAG property of the init_event, and we apply our effects
   (which are then bound to virtual keys > FX_KEYS_MAX_VIRTUAL)

   [note] that we can do cool things, like mapping the same instance multiple times (though it will always
   apply itself to the same in/out tracks

   we don't need to worry about free()ing init_events, since they will be free'd
   when the instance is deinited
*/
static void create_effects_map(uint64_t rteval) {
  int count = 0;
  weed_plant_t *inst;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rteval & (GU641 << i) && (inst = weed_instance_obtain(i, key_modes[i])) != NULL) {
      if (!mainw->record_starting) {
        if (!THREADVAR(fx_is_audio)) {
          if (weed_get_boolean_value(init_events[i], LIVES_LEAF_NOQUANT, NULL) == WEED_TRUE) {
            weed_instance_unref(inst);
            continue;
          }
        } else {
          if (weed_get_boolean_value(init_events[i], LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) {
            weed_instance_unref(inst);
            continue;
          }
        }
      }
      if (enabled_in_channels(inst, FALSE) > 0 &&
          enabled_out_channels(inst, FALSE) > 0) {
        effects_map[count++] = init_events[i];
      }
      weed_instance_unref(inst);
    }
  }
  effects_map[count] = NULL; ///< marks the end of the effect map
}


/**
   @brief add effect_deinit events to an event_list

   during real time recording we use the "keys" 0 -> FX_KEYS_MAX_VIRTUAL
   here we add effect_deinit events to an event_list

   @see deinit_render_effects()
*/
weed_plant_t *add_filter_deinit_events(weed_plant_t *event_list) {
  weed_plant_t *inst;
  weed_timecode_t last_tc = 0;
  boolean needs_filter_map = FALSE;

  if (event_list) last_tc = get_event_timecode(get_last_event(event_list));

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (init_events[i]) {
      if ((inst = weed_instance_obtain(i, key_modes[i])) != NULL) {
        if (enabled_in_channels(inst, FALSE) > 0 && enabled_out_channels(inst, FALSE)) {
          event_list = append_filter_deinit_event(event_list, last_tc, init_events[i],
                                                  mainw->multitrack ? pchains[i] : NULL);
          needs_filter_map = TRUE;
        }
      }
      init_events[i] = NULL;
      if (pchains[i]) lives_free(pchains[i]);
      pchains[i] = NULL;
    }
  }

  /// add an empty filter_map event (in case more frames are added)
  create_effects_map(0); ///< we create filter_map event_t * array with ordered effects
  if (needs_filter_map) {
    pthread_mutex_lock(&mainw->event_list_mutex);
    event_list = append_filter_map_event(event_list, last_tc, effects_map);
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }
  return event_list;
}


/**
   @brief add init events for every effect which is switched on

   during rendering we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
   here we are about to start playback, and we add init events for every effect which is switched on
   we add the init events with a timecode of 0 */
weed_plant_t *add_filter_init_events(weed_plant_t *event_list, weed_timecode_t tc) {
  weed_plant_t *inst;
  int fx_idx, ntracks;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if ((inst = weed_instance_obtain(i, key_modes[i])) != NULL) {
      if (!(fx_key_defs[i].flags & FXKEY_SOFT_DEINIT)) {
        if (enabled_in_channels(inst, FALSE) > 0 && enabled_out_channels(inst, FALSE)) {
          if (!weed_plant_has_leaf(inst, WEED_LEAF_RANDOM_SEED))
            weed_set_int64_value(inst, WEED_LEAF_RANDOM_SEED, gen_unique_id());
          event_list = append_filter_init_event(event_list, tc,
                                                (fx_idx = key_to_fx[i][key_modes[i]]), -1, i, inst);
          init_events[i] = get_last_event(event_list);
          ntracks = weed_leaf_num_elements(init_events[i], WEED_LEAF_IN_TRACKS);
          // add values from inst
          pchains[i] = filter_init_add_pchanges(event_list, inst, init_events[i], ntracks, 0);
        }
      }
      weed_instance_unref(inst);
    }
  }
  /// add an empty filter_map event (in case more frames are added)
  create_effects_map(mainw->rte); /// we create filter_map event_t * array with ordered effects
  if (effects_map[0]) event_list = append_filter_map_event(event_list, tc, effects_map);
  return event_list;
}


/**
   @brief check palette vs. palette list

   check if palette is in the palette_list
   if not, return next best palette to use, using a heuristic method
   num_palettes is the size of the palette list */
int best_palette_match(int *palette_list, int num_palettes, int palette) {
  int best_palette = palette_list[0];
  boolean has_alpha, is_rgb, is_alpha, is_yuv, mismatch;

  if (palette == best_palette) return palette;

  has_alpha = weed_palette_has_alpha(palette);
  is_rgb = weed_palette_is_rgb(palette);
  is_alpha = weed_palette_is_alpha(palette);
  is_yuv = !is_alpha && !is_rgb;

  for (int i = 0; ((num_palettes > 0 && i < num_palettes) || num_palettes <= 0) && palette_list[i] != WEED_PALETTE_END; i++) {
    if (palette_list[i] == palette) {
      /// exact match - return it
      return palette;
    }

    if ((weed_palette_is_rgb(best_palette) && !is_rgb) || (weed_palette_is_alpha(best_palette) && !is_alpha)
        || (weed_palette_is_yuv(best_palette) && !is_yuv)) mismatch = TRUE;
    else mismatch = FALSE;

    switch (palette_list[i]) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_BGR24:
      if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) return palette_list[i];

      if ((is_rgb || mismatch) && (!weed_palette_has_alpha(palette) || best_palette == WEED_PALETTE_RGBAFLOAT ||
                                   best_palette == WEED_PALETTE_RGBFLOAT)
          && !(best_palette == WEED_PALETTE_RGB24 || best_palette == WEED_PALETTE_BGR24))
        best_palette = palette_list[i];
      break;
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32) best_palette = palette_list[i];
    case WEED_PALETTE_ARGB32:
      if ((is_rgb || mismatch) && (has_alpha || best_palette == WEED_PALETTE_RGBFLOAT
                                   || best_palette == WEED_PALETTE_RGBAFLOAT)
          && best_palette != WEED_PALETTE_RGBA32
          && !(palette == WEED_PALETTE_ARGB32 && best_palette == WEED_PALETTE_BGRA32))
        best_palette = palette_list[i];
      break;
    case WEED_PALETTE_RGBAFLOAT:
      if ((is_rgb || mismatch) && best_palette == WEED_PALETTE_RGBFLOAT && has_alpha)
        best_palette = WEED_PALETTE_RGBAFLOAT;
      break;
    case WEED_PALETTE_RGBFLOAT:
      if ((is_rgb || mismatch) && best_palette == WEED_PALETTE_RGBAFLOAT && !has_alpha)
        best_palette = WEED_PALETTE_RGBFLOAT;
      break;
    // yuv
    case WEED_PALETTE_YUVA4444P:
      if (is_yuv && has_alpha) return WEED_PALETTE_YUVA4444P;
      if ((mismatch || is_yuv)
          && (has_alpha || (best_palette != WEED_PALETTE_YUV444P && best_palette != WEED_PALETTE_YUV888)))
        best_palette = WEED_PALETTE_YUVA4444P;
      break;
    case WEED_PALETTE_YUV444P:
      if (is_yuv && !has_alpha) return WEED_PALETTE_YUV444P;
      if ((mismatch || is_yuv)
          && (!has_alpha || (best_palette != WEED_PALETTE_YUVA4444P && best_palette != WEED_PALETTE_YUVA8888)))
        best_palette = WEED_PALETTE_YUV444P;
      break;
    case WEED_PALETTE_YUVA8888:
      if ((mismatch || is_yuv) && (has_alpha || (best_palette != WEED_PALETTE_YUV444P && best_palette != WEED_PALETTE_YUV888)))
        best_palette = WEED_PALETTE_YUVA8888;
      break;
    case WEED_PALETTE_YUV888:
      if ((mismatch || is_yuv)
          && (!has_alpha || (best_palette != WEED_PALETTE_YUVA4444P && best_palette != WEED_PALETTE_YUVA8888)))
        best_palette = WEED_PALETTE_YUV888;
      break;
    case WEED_PALETTE_YUV422P:
    case WEED_PALETTE_UYVY:
    case WEED_PALETTE_YUYV:
      if ((mismatch || is_yuv) && (best_palette != WEED_PALETTE_YUVA4444P && best_palette != WEED_PALETTE_YUV444P
                                   && best_palette != WEED_PALETTE_YUVA8888 && best_palette != WEED_PALETTE_YUV888))
        best_palette = palette_list[i];
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      if ((mismatch || is_yuv) && (best_palette == WEED_PALETTE_YUV411 || weed_palette_is_alpha(best_palette)))
        best_palette = palette_list[i];
      break;
    default:
      if (weed_palette_is_alpha(best_palette)) {
        if (palette_list[i] == WEED_PALETTE_YUV411 && !is_alpha) best_palette = WEED_PALETTE_A8;
        if (palette_list[i] == WEED_PALETTE_A8) best_palette = WEED_PALETTE_A8;
        if (palette_list[i] == WEED_PALETTE_A1 && (best_palette != WEED_PALETTE_A8))
          best_palette = WEED_PALETTE_A1;
      }
      break;
    }
  }

#ifdef DEBUG_PALETTES
  lives_printerr("Debug: best palette for %d is %d\n", palette, best_palette);
#endif

  return best_palette;
}


static boolean get_fixed_channel_size(weed_plant_t *template, int *width, int *height) {
  weed_size_t nwidths = weed_leaf_num_elements(template, WEED_LEAF_WIDTH);
  weed_size_t nheights = weed_leaf_num_elements(template, WEED_LEAF_HEIGHT);
  int *heights, *widths;
  int defined = 0;
  int oheight = 0, owidth = 0;
  int i;
  if (!width || nwidths == 0) defined++;
  else {
    if (nwidths == 1) {
      *width = weed_get_int_value(template, WEED_LEAF_WIDTH, NULL);
      defined++;
    }
  }

  if (!height || nheights == 0) defined++;
  else {
    if (nheights == 1) {
      *height = weed_get_int_value(template, WEED_LEAF_HEIGHT, NULL);
      defined++;
    }
  }
  if (defined == 2) return TRUE;

  /// we have dealt with n == 0 and n == 1, now we should have pairs
  if (nwidths != nheights) {
    /// spec error
    return FALSE;
  }

  widths = weed_get_int_array(template, WEED_LEAF_WIDTH, NULL);
  heights = weed_get_int_array(template, WEED_LEAF_HEIGHT, NULL);

  if (width) owidth = *width;
  if (height) oheight = *height;

  for (i = 0; i < nwidths; i++) {
    defined = 0;
    // these should be in ascending order, so we'll just quit when both values are greater
    if (!width) defined++;
    else {
      *width = widths[i];
      if (*width >= owidth) defined++;
    }
    if (!height) defined++;
    else {
      *height = heights[i];
      if (*height >= oheight) defined++;
    }
    if (defined == 2) return TRUE;
  }
  return FALSE;
}


#define MIN_CHAN_WIDTH 4
#define MIN_CHAN_HEIGHT 4

#define MAX_CHAN_WIDTH 16384
#define MAX_CHAN_HEIGHT 16384

/**
   @brief set the size of a video filter channel as near as possible to width (macropixels) * height

   We will comply with any restrictions set up for the channel, ie.
   (fixed width(s), fixed height(s), hstep, vstep, maxwidth, maxheight, minwidth, minheight)

   if there are user defined defaults for the size (e.g it's the out channel of a generator) then we use
   those, provided this doesn't confilct with the channel's fixed size requirements

   we take the restrictions from the filter class owning the channel template from which the channel was modelled,
   unless the filter flag WEED_FILTER_CHANNEL_SIZES_MAY_VARY, in which case the filter defaults may be
   overridden by values in the channel template

   function is called with dummy values when we first create the channels, and then with real values before we process a frame
*/
void validate_channel_sizes(weed_filter_t *filter, weed_chantmpl_t *chantmpl, int *width, int *height) {
  boolean check_ctmpl = FALSE;
  int max, min;
  int filter_flags = weed_filter_get_flags(filter);

  if (*width < MIN_CHAN_WIDTH) *width = MIN_CHAN_WIDTH;
  if (*width > MAX_CHAN_WIDTH) *width = MAX_CHAN_WIDTH;
  if (*height < MIN_CHAN_HEIGHT) *height = MIN_CHAN_HEIGHT;
  if (*height > MAX_CHAN_HEIGHT) *height = MAX_CHAN_HEIGHT;

  if (filter_flags & WEED_FILTER_CHANNEL_SIZES_MAY_VARY) {
    check_ctmpl = TRUE;
  }

  if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_WIDTH)) {
    get_fixed_channel_size(chantmpl, width, height);
  } else {
    if (weed_plant_has_leaf(filter, WEED_LEAF_WIDTH)) {
      get_fixed_channel_size(filter, width, height);
    } else {
      if (weed_plant_has_leaf(filter, WEED_LEAF_HSTEP)) {
        *width = ALIGN_CEIL(*width, weed_get_int_value(filter, WEED_LEAF_HSTEP, NULL));
      }

      if (*width < MIN_CHAN_WIDTH) *width = MIN_CHAN_WIDTH;
      if (*width > MAX_CHAN_WIDTH) *width = MAX_CHAN_WIDTH;
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MAXWIDTH)) {
        max = weed_get_int_value(chantmpl, WEED_LEAF_MAXWIDTH, NULL);
        if (max > 0 && *width > max) *width = max;
      }
      if (weed_plant_has_leaf(filter, WEED_LEAF_MAXWIDTH)) {
        max = weed_get_int_value(filter, WEED_LEAF_MAXWIDTH, NULL);
        if (max > 0 && *width > max) *width = max;
      }
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MINWIDTH)) {
        min = weed_get_int_value(chantmpl, WEED_LEAF_MINWIDTH, NULL);
        if (min > 0 && *width < min) *width = min;
      }
      if (weed_plant_has_leaf(filter, WEED_LEAF_MINWIDTH)) {
        min = weed_get_int_value(filter, WEED_LEAF_MINWIDTH, NULL);
        if (*width < min) *width = min;
      }
    }
  }

  if (*width < MIN_CHAN_WIDTH) *width = MIN_CHAN_WIDTH;
  if (*width > MAX_CHAN_WIDTH) *width = MAX_CHAN_WIDTH;

  *width = (*width >> 1) << 1;

  if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_HEIGHT)) {
    get_fixed_channel_size(chantmpl, width, height);
  } else {
    if (weed_plant_has_leaf(filter, WEED_LEAF_HEIGHT)) {
      get_fixed_channel_size(filter, width, height);
    } else {
      if (weed_plant_has_leaf(filter, WEED_LEAF_VSTEP)) {
        *height = ALIGN_CEIL(*height, weed_get_int_value(filter, WEED_LEAF_VSTEP, NULL));
      }
      if (*height < MIN_CHAN_HEIGHT) *height = MIN_CHAN_HEIGHT;
      if (*height > MAX_CHAN_HEIGHT) *height = MAX_CHAN_HEIGHT;
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MAXHEIGHT)) {
        max = weed_get_int_value(chantmpl, WEED_LEAF_MAXHEIGHT, NULL);
        if (max > 0 && *height > max) *height = max;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MAXHEIGHT)) {
        max = weed_get_int_value(filter, WEED_LEAF_MAXHEIGHT, NULL);
        if (max > 0 && *height > max) *height = max;
      }
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MINHEIGHT)) {
        min = weed_get_int_value(chantmpl, WEED_LEAF_MINHEIGHT, NULL);
        if (min > 0 && *height < min) *height = min;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MINHEIGHT)) {
        min = weed_get_int_value(filter, WEED_LEAF_MINHEIGHT, NULL);
        if (*height < min) *height = min;
      }
    }
  }

  if (*height < MIN_CHAN_HEIGHT) *height = MIN_CHAN_HEIGHT;
  if (*height > MAX_CHAN_HEIGHT) *height = MAX_CHAN_HEIGHT;
  *height = (*height >> 1) << 1;
}


void set_channel_size(weed_filter_t *filter, weed_channel_t *channel, int *width, int *height) {
  weed_chantmpl_t *chantmpl = weed_channel_get_template(channel);
  validate_channel_sizes(filter, chantmpl, width, height);
  weed_channel_set_width(channel, *width);
  weed_channel_set_height(channel, *height);
}


int weed_flagset_array_count(weed_plant_t **array, boolean set_readonly) {
  int i;
  for (i = 0; array[i]; i++) {
    if (set_readonly) weed_add_plant_flags(array[i], WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_");
  }
  return i;
}


/// change directory to plugin installation dir so it can find any data files
///
/// returns copy of current directory (before directory change) which should be freed after use
char *cd_to_plugin_dir(weed_plant_t *filter) {
  weed_plant_t *plugin_info;
  char *ppath;
  char *ret;
  if (WEED_PLANT_IS_PLUGIN_INFO(filter)) plugin_info = filter; // on shutdown the filters are freed.
  else plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
  ppath = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_PATH, NULL);
  ret = lives_get_current_dir();
  // allow this to fail -it's not that important - it just means any plugin data files wont be found
  // besides, we dont want to show warnings at 50 fps
  lives_chdir(ppath, TRUE);
  lives_free(ppath);
  return ret;
}


LIVES_GLOBAL_INLINE weed_plant_t *get_next_compound_inst(weed_plant_t *inst) {
  return weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
}


lives_filter_error_t weed_reinit_effect(weed_plant_t *inst, boolean reinit_compound) {
  weed_plant_t *filter, *orig_inst = inst;
  lives_rfx_t *rfx = NULL;
  boolean deinit_first = FALSE, soft_deinit = FALSE;
  weed_error_t retval;
  lives_filter_error_t filter_error = FILTER_SUCCESS;
  int key = -1;

  // we need to reinit
  // however, a size / rowstrides change may have been caused by an adjustment in adaptive quality
  // - if the reinit causes a delay then we might immediately switch back to a lower quality
  //   (since frames will be dropped) - however if we keep doing this, we will be constantly reiniting
  //   thus we need to tell the player to ignore dropped frames for this cycle when updating effort

  // player will reset this for the next cycle
  if (LIVES_IS_PLAYING && !mainw->multitrack && mainw->scratch == SCRATCH_NONE)
    mainw->scratch = SCRATCH_JUMP_NORESYNC;

  weed_instance_ref(inst);

  if (!mainw->multitrack) {
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY))
      key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
    if (key != -1) {
      soft_deinit = !!(fx_key_defs[key].flags & FXKEY_SOFT_DEINIT);
      fx_key_defs[key].flags &= ~FXKEY_SOFT_DEINIT;
      if (!weed_plant_has_leaf(inst, LIVES_LEAF_AUTO_EASING)) {
        weed_plant_t *gui;
        gui = weed_instance_get_gui(inst, FALSE);
        if (gui) {
          filter_mutex_lock(key);
          if (weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL) > 0) {
            // plugin is easing, so we need to deinit it
            uint64_t new_rte = GU641 << key;
            weed_deinit_effect(key);
            mainw->rte &= ~new_rte;
            mainw->rte_real &= ~new_rte;
            if (rte_window_visible()) rtew_set_keych(key, FALSE);
            if (mainw->ce_thumbs) ce_thumbs_set_keych(key, FALSE);
            weed_instance_unref(inst);
            filter_mutex_unlock(key);
            mainw->refresh_model = TRUE;
            return FILTER_ERROR_COULD_NOT_REINIT;
          }
          filter_mutex_unlock(key);
        }
      }
    }
  }

  if (fx_dialog[1]) rfx = fx_dialog[1]->rfx;
  else if (mainw->multitrack) rfx = mainw->multitrack->current_rfx;

reinit:

  filter = weed_instance_get_filter(inst, FALSE);

  if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_TRUE) {
    deinit_first = TRUE;
  }

  if (deinit_first) {
    retval = weed_call_deinit_func(inst);
    if (retval != WEED_SUCCESS) {
      goto re_done;
    }
  }

  retval = weed_call_init_func(inst);

  if (retval != WEED_SUCCESS) goto re_done;

  if (weed_plant_has_leaf(filter, WEED_LEAF_INIT_FUNC)) {
    if (fx_dialog[1]
        || (mainw->multitrack &&  mainw->multitrack->current_rfx
            && mainw->multitrack->poly_state == POLY_PARAMS)) {
      // redraw GUI if necessary
      if (rfx->source_type == LIVES_RFX_SOURCE_WEED && rfx->source == inst) {
        // update any text params with focus
        if (mainw->textwidget_focus && LIVES_IS_WIDGET_OBJECT(mainw->textwidget_focus)) {
          // make sure text widgets are updated if they activate the default
          LiVESWidget *textwidget =
            (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus), TEXTWIDGET_KEY);
          weed_set_boolean_value(inst, WEED_LEAF_HOST_REINITING, WEED_TRUE);
          after_param_text_changed(textwidget, rfx);
          weed_leaf_delete(inst, WEED_LEAF_HOST_REINITING);
          mainw->textwidget_focus = NULL;
        }

        // do updates from "gui"
        if (rfx->num_params > 0 && !rfx->params)
          rfx->params = weed_params_to_rfx(rfx->num_params, inst, FALSE);
        if (fx_dialog[1]) {
          int keyw = fx_dialog[1]->key;
          int modew = fx_dialog[1]->mode;
          if (!deinit_first) weed_set_boolean_value(inst, WEED_LEAF_HOST_REINITING, WEED_TRUE);
          update_widget_vis(NULL, keyw, modew);
          if (!deinit_first) weed_leaf_delete(inst, WEED_LEAF_HOST_REINITING);
        } else update_widget_vis(rfx, -1, -1);
        filter_error = FILTER_INFO_REDRAWN;
      }
    }
  }

  if (retval != WEED_SUCCESS) goto re_done;

  if (filter_error != FILTER_INFO_REDRAWN) filter_error = FILTER_INFO_REINITED;

  if (reinit_compound) {
    weed_plant_t *xinst = inst;
    inst = get_next_compound_inst(inst);
    if (inst) {
      if (xinst && xinst != orig_inst) weed_instance_unref(xinst);
      weed_instance_ref(inst);
      goto reinit;
    }
  }
re_done:
  if (soft_deinit) fx_key_defs[key].flags |= FXKEY_SOFT_DEINIT;

  if (inst && inst != orig_inst) weed_instance_unref(inst);
  weed_instance_unref(orig_inst);

  if (retval == WEED_SUCCESS) return filter_error;
  if (retval == WEED_ERROR_PLUGIN_INVALID) return FILTER_ERROR_INVALID_PLUGIN;
  if (retval == WEED_ERROR_FILTER_INVALID) return FILTER_ERROR_INVALID_FILTER;
  return FILTER_ERROR_COULD_NOT_REINIT;
}


void weed_reinit_all(void) {
  // reinit all effects on playback start
  lives_filter_error_t retval;
  weed_plant_t *instance, *last_inst, *next_inst;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      filter_mutex_lock(i);
      if (rte_key_is_enabled(i, TRUE)) {
        if ((instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
          filter_mutex_unlock(i);
          continue;
        }

        filter_mutex_unlock(i);

        if (weed_get_voidptr_value(instance, "host_param_window", NULL) != NULL) {
          weed_instance_unref(instance);
          continue;
        }

        last_inst = instance;

        // ignore video generators
        while ((next_inst = get_next_compound_inst(last_inst)) != NULL) last_inst = next_inst;
        if (enabled_in_channels(instance, FALSE) == 0 && enabled_out_channels(last_inst, FALSE) > 0 &&
            !is_pure_audio(last_inst, FALSE)) {
          weed_instance_unref(instance);
          continue;
        }
        retval = weed_reinit_effect(instance, FALSE);
        if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
            || retval == FILTER_ERROR_INVALID_FILTER) {
          weed_instance_unref(instance);
          continue;
        }
        weed_instance_unref(instance);
      } else filter_mutex_unlock(i);
    }
  }
}


static weed_error_t thread_process_func(weed_instance_t *inst, weed_timecode_t tc, boolean thrd_local) {
  int nchans;
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_process_f process_func = (weed_process_f)weed_get_funcptr_value(filter, WEED_LEAF_PROCESS_FUNC, NULL);
  weed_channel_t **out_channels = weed_instance_get_out_channels(inst, &nchans);
  weed_error_t ret = WEED_SUCCESS;
  void ***opd = NULL;

  if (thrd_local) {
    opd = LIVES_CALLOC_SIZEOF(void **, nchans);
    void *buff = THREADVAR(buffer);
    for (int i = 0; i < nchans; i++) {
      int nplanes;
      weed_channel_t *chan = out_channels[i];
      void **pd = weed_channel_get_pixel_data_planar(chan, &nplanes);
      int dheight = weed_channel_get_height(chan);
      int *rows = weed_channel_get_rowstrides(chan, &nplanes);
      int pal = weed_channel_get_palette(chan);
      size_t totsize = 0;
      opd[i] = weed_channel_get_pixel_data_planar(chan, &nplanes);
      for (int p = 0; p < nplanes; p++) {
        size_t bsize = rows[p] * dheight * weed_palette_get_plane_ratio_vertical(pal, p);
        pd[p] = buff + totsize;
        totsize += bsize;
      }
      weed_channel_set_pixel_data_planar(chan, (void **)pd, nplanes);
      lives_free(pd); lives_free(rows);
    }
  }

  ret = (*process_func)(inst, tc);

  if (thrd_local) {
    for (int i = 0; i < nchans; i++) {
      int nplanes;
      weed_channel_t *chan = out_channels[i];
      void **pd = weed_channel_get_pixel_data_planar(chan, &nplanes);
      int dheight = weed_channel_get_height(chan);
      int *rows = weed_channel_get_rowstrides(chan, &nplanes);
      int pal = weed_channel_get_palette(chan);
      for (int p = 0; p < nplanes; p++) {
        size_t bsize = rows[p] * dheight * weed_palette_get_plane_ratio_vertical(pal, p);
        lives_memcpy(opd[i][p], pd[p], bsize);
      }
      lives_free(pd); lives_free(rows); lives_free(opd[i]);
    }
    lives_free(opd);
  }

  lives_free(out_channels);
  return ret;
}


#define SLICE_ALIGN 2

static lives_filter_error_t process_func_threaded(weed_plant_t *inst, weed_timecode_t tc) {
  // split output(s) into horizontal slices
  lives_proc_thread_t *lpts = NULL;
  weed_plant_t **xinst = NULL;
  weed_plant_t **xchannels, *xchan;
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_error_t retval;
  void **pd;
  int *rows;
  size_t maxsize, totsize = 0;
  int nchannels;
  int pal;
  double vrt;

  weed_plant_t **out_channels = weed_instance_get_out_channels(inst, &nchannels);
  boolean plugin_invalid = FALSE;
  boolean filter_invalid = FALSE;
  boolean filter_busy = FALSE;
  boolean needs_reinit = FALSE;
  boolean wait_state_upd = FALSE, state_updated = FALSE;
  boolean use_thrdlocal = FALSE, can_use_thrd_local = FALSE;

  int vstep = SLICE_ALIGN, minh;
  int slices, slices_per_thread, to_use;
  int **xheights;
  int dheight, height, xheight = 0, cheight;
  int filter_flags;
  int nthreads = 0;
  int nplanes, xoffset;
  int i, j, p;

  if (weed_plant_has_leaf(filter, WEED_LEAF_VSTEP)) {
    minh = weed_get_int_value(filter, WEED_LEAF_VSTEP, NULL);
    if (minh > vstep) vstep = minh;
  }

  filter_flags = weed_filter_get_flags(filter);
  if ((filter_flags & WEED_FILTER_HINT_STATEFUL)
      && weed_get_boolean_value(filter, LIVES_LEAF_IGNORE_STATE_UPDATES, NULL) == WEED_FALSE)
    wait_state_upd = TRUE;

  xheights = LIVES_CALLOC_SIZEOF(int *, nchannels);

  for (i = 0; i < nchannels; i++) {
    /// min height for slices (in all planes) is SLICE_ALIGN, unless an out channel has a larger vstep set
    xheights[i] = LIVES_CALLOC_SIZEOF(int, 2);
    height = weed_channel_get_height(out_channels[i]);
    pal = weed_channel_get_palette(out_channels[i]);
    nplanes = weed_palette_get_nplanes(pal);
    for (p = 0; p < nplanes; p++) {
      vrt = weed_palette_get_plane_ratio_vertical(pal, p);
      cheight = height * vrt;
      if (xheight == 0 || cheight < xheight) xheight = cheight;
    }
  }
  if (xheight % vstep != 0) return FILTER_ERROR_DONT_THREAD;

  // slices = min height / step
  slices = xheight / vstep;
  slices_per_thread = ALIGN_CEIL(slices, prefs->nfx_threads) / prefs->nfx_threads;

  to_use = ALIGN_CEIL(slices, slices_per_thread) / slices_per_thread;
  if (to_use < 0) return FILTER_ERROR_DONT_THREAD;

  xinst = (weed_plant_t **)lives_calloc(to_use, sizeof(weed_plant_t *));
  lpts = (lives_proc_thread_t *)lives_calloc(to_use, sizeof(lives_proc_thread_t));

  for (i = 0; i < nchannels; i++) {
    xheights[i][1] = height = weed_channel_get_height(out_channels[i]);
    slices = height / vstep;
    slices_per_thread = CEIL((double)slices / (double)to_use, 1.);
    dheight = slices_per_thread * vstep;

    /// min height for slices (in all planes) is SLICE_ALIGN, unless an out channel has a larger vstep set
    rows = weed_channel_get_rowstrides(out_channels[i], &nplanes);
    height = weed_channel_get_height(out_channels[i]);
    pal = weed_channel_get_palette(out_channels[i]);

    /* if (weed_get_boolean_value(out_channels[i], WEED_LEAF_HOST_INPLACE, NULL)) */
    /*   can_use_thrd_local = FALSE; */
    /* else { */
    /*   for (p = 0; p < nplanes; p++) */
    /*     totsize += rows[p] * dheight * weed_palette_get_plane_ratio_vertical(pal, p); */
    /* } */
    lives_free(rows);
    xheights[i][0] = dheight;
  }

  maxsize = THREADVAR(buff_size);

  if (can_use_thrd_local && totsize  <= maxsize) use_thrdlocal = TRUE;

  for (j = 0; j < to_use; j++) {
    // each thread gets its own copy of the filter instance, with the following changes
    // - each copy has no refcount (since it is only used here)
    // - each copy has its own set of output channel(s), with a reduced height and an offset
    // but note that WEED_LEAF_PIXEL_DATA always points to the same memory buffer(s)
    //

    xinst[j] = lives_plant_copy(inst);
    xchannels = (weed_plant_t **)lives_calloc(nchannels, sizeof(weed_plant_t *));

    for (i = 0; i < nchannels; i++) {
      dheight = xheights[i][0];
      xoffset = dheight * j;
      xchan = xchannels[i] = lives_plant_copy(out_channels[i]);
      height = xheights[i][1];

      if ((height - xoffset) < dheight) dheight = height - xoffset;
      xheights[i][0] = dheight;

      if (xchan) {
        weed_set_int_value(xchan, WEED_LEAF_OFFSET, xoffset);
        weed_set_int_array(xchan, WEED_LEAF_HEIGHT, 2, xheights[i]);
      }

      rows = weed_channel_get_rowstrides(xchan, &nplanes);
      pal = weed_channel_get_palette(xchan);

      pd = weed_channel_get_pixel_data_planar(xchan, &nplanes);

      for (p = 0; p < nplanes; p++) {
        pd[p] += (int)(xoffset * weed_palette_get_plane_ratio_vertical(pal, p))
                 * rows[p];
      }
      weed_channel_set_pixel_data_planar(xchan, (void **)pd, nplanes);
      if (pd) lives_free(pd);
      if (rows) lives_free(rows);
    }

    ///

    weed_set_plantptr_array(xinst[j], WEED_LEAF_OUT_CHANNELS, nchannels, xchannels);
    lives_freep((void **)&xchannels);

    ////

    if (wait_state_upd) {
      // for stateful effects, the first thread is added with
      // with "state_updated" set to false/ The thread will check for this, do the updates, then signal
      // that the state has been updated. We can then proceed to send the remaining threads with
      // "state_updated" set to true
      // and once this has been signalled we can send the reamaining threads
      if (!state_updated) weed_set_boolean_value(xinst[j], WEED_LEAF_STATE_UPDATED, WEED_FALSE);
      else weed_set_boolean_value(xinst[j], WEED_LEAF_STATE_UPDATED, WEED_TRUE);
    }

    // start a thread for processing

    lpts[j] = lives_proc_thread_create(LIVES_THRDATTR_PRIORITY, thread_process_func, WEED_SEED_INT, "pIb",
                                       xinst[j], tc, use_thrdlocal);
    nthreads++; // actual number of threads used

    if (wait_state_upd && !state_updated) {
      //  one thread may update static data for all threads
      // in this case we wait here for the update to be performed
      lives_nanosleep_until_nonzero(lives_proc_thread_is_done(lpts[j], FALSE)
                                    || weed_get_boolean_value(xinst[j], WEED_LEAF_STATE_UPDATED, NULL));
      if (weed_get_boolean_value(xinst[j], WEED_LEAF_STATE_UPDATED, NULL) == WEED_FALSE) {
        weed_set_boolean_value(filter, LIVES_LEAF_IGNORE_STATE_UPDATES, WEED_TRUE);
        wait_state_upd = FALSE;
      } else state_updated = TRUE;
    }
  }

  // wait for threads to finish
  for (j = 0; j < nthreads; j++) {
    retval = lives_proc_thread_join_int(lpts[j]);
    lives_proc_thread_unref(lpts[j]);
    if (retval == WEED_ERROR_PLUGIN_INVALID) plugin_invalid = TRUE;
    if (retval == WEED_ERROR_FILTER_INVALID) filter_invalid = TRUE;
    if (retval == WEED_ERROR_NOT_READY) filter_busy = TRUE;
    if (retval == WEED_ERROR_REINIT_NEEDED) needs_reinit = TRUE;

    xchannels = weed_instance_get_out_channels(xinst[j], &nchannels);

    for (i = 0; i < nchannels; i++) {
      if (xchannels[i]) weed_plant_free(xchannels[i]);
      lives_freep((void **)&xchannels);
    }
    weed_plant_free(xinst[j]);
  }

  for (i = 0; i < nchannels; i++) lives_free(xheights[i]);

  lives_freep((void **)&xheights);
  lives_freep((void **)&xinst);
  lives_freep((void **)&lpts);
  lives_freep((void **)&out_channels);

  if (plugin_invalid) return FILTER_ERROR_INVALID_PLUGIN;
  if (filter_invalid) return FILTER_ERROR_INVALID_FILTER;
  if (filter_busy) return FILTER_ERROR_BUSY;
  if (needs_reinit) return FILTER_ERROR_NEEDS_REINIT; // TODO...
  return FILTER_SUCCESS;
}


static lives_filter_error_t check_cconx(weed_plant_t *inst, int nchans, boolean *needs_reinit) {
  // processing for alpha channel connectsions
  weed_plant_t **in_channels;

  // we stored original key/mode to use here
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
    // pull from alpha chain
    int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL), mode;
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_MODE)) {
      mode = weed_get_int_value(inst, WEED_LEAF_HOST_MODE, NULL);
    } else mode = key_modes[key];

    // need to do this AFTER setting in-channel size
    if (mainw->cconx) {
      // chain any alpha channels
      if (cconx_chain_data(key, mode)) *needs_reinit = TRUE;
    }
  }

  // make sure we have pixel_data for all mandatory in alpha channels (from alpha chains)
  // if not, if the ctmpl is optnl mark as host_temp_disabled; else return with error

  in_channels = weed_instance_get_in_channels(inst, NULL);

  for (int i = 0; i < nchans; i++) {
    if (!weed_channel_is_alpha(in_channels[i])) continue;

    if (weed_plant_has_leaf(in_channels[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
      if (cconx_chain_data_internal(in_channels[i])) *needs_reinit = TRUE;
    }

    if (!weed_channel_get_pixel_data(in_channels[i])) {
      weed_chantmpl_t *chantmpl = weed_channel_get_template(in_channels[i]);
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl)))
        if (!weed_channel_is_disabled(in_channels[i]))
          weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        else weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
      // WEED_LEAF_DISABLED will serve instead
      else {
        lives_freep((void **)&in_channels);
        return FILTER_ERROR_MISSING_CHANNEL;
      }
    }
  }

  lives_freep((void **)&in_channels);
  return FILTER_SUCCESS;
}

static boolean can_thread(weed_filter_t *filt) {
  return future_prefs->nfx_threads > 1
         && (weed_filter_get_flags(filt) & WEED_FILTER_HINT_MAY_THREAD);
}


// this is qutie simple now we have the nodemodel + plan to guide things

// clear WEED_LEAF_DISABLED for any channels which receive data from a non NULL layer
//  and there is no WEED_LEAF_DISABLED in template

// if we have a NULL layer, we set WEED_LEAF_DISABLED if WEED_LEAF_OPTIONAL,
// otherwise we cannot apply the filter */

// wait for all pre reqeusite steps to complete

/* set channel timecodes */

/* set the fps (data) in the instance */

// tansfer layer pixel_data to in_channels
// if an out channel is not inplcea, create empty pixel_data for it
// apply the instance
// for non inplcae inputs, free the piixel_Data
// transfrer out_chan pixdata to layers


/**
   @brief process a single video filter instance
   If the filter was not Inplace then we need to copy the output channel back to the layer.
   Otherwise, the pixel data is already shared, so we can simply nullify it in the channel.

   for in/out alpha channels, there is no matching layer. These channels are passed around like data
   using mainw->cconx as a guide. We will simply free any in alpha channels after processing (unless inplace was used)

   for purely audio filters, these are handled in weed_apply_audio_instance()

   for filters with mixed video / audio inputs (currently only generators) audio is colleccted and pushed via
   a separate audio channel
*/
lives_filter_error_t weed_apply_instance(weed_instance_t *inst, weed_event_t *init_event, lives_layer_t **layers,
    int opwidth, int opheight, weed_timecode_t tc) {
  // for dry_run, all we need do is - check if the instance needs to be reinited for reasons other
  // than a palette change
  // there may be some other reason for doing so, then we can ignore reinit costs when routing
  // - find the channel size at the time we are going to apply the instance

  /* lives_proc_thread_t lpt = NULL; */
  /* weed_plant_t *def_channel = NULL; */
  /* lives_clip_src_t dsource; */
  /* inst_node_t *inode; */
  /* double factors[N_COST_TYPES - 1]; */

  /* int *rowstrides; */
  /* int *channel_rows; */

  weed_filter_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_chantmpl_t **in_ctmpls, *chantmpl;
  weed_channel_t **in_channels = NULL, **out_channels = NULL, *channel;
  weed_layer_t *layer = NULL;
  int *in_tracks, *out_tracks;
  void *pdata;
  int *mand;
  lives_filter_error_t retval = FILTER_SUCCESS;
  boolean needs_reinit = FALSE;
  boolean busy = FALSE;
  int clip = -1;
  int width, height, lstat;
  int num_ctmpl, num_inc, num_outc;
  int num_in_alpha = 0, num_out_alpha = 0;
  int num_in_tracks = 0, num_out_tracks;
  int nmandout = 0;
  int channel_flags;
  int i, j, k;

  if (prefs->dev_show_timing)
    g_printerr("apply inst @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

  // TODO - check if inited, if not return with error (check also for filters with no init_func)

  out_channels = weed_instance_get_out_channels(inst, NULL);

  if (!out_channels && (mainw->preview || mainw->is_rendering) && num_compound_fx(inst) == 1) {
    /// skip filters with no output channels during previews or when rendering, the output values of these
    // analysers will already have been recorded
    lives_freep((void **)&out_channels);
    return retval;
  }

  if (is_pure_audio(filter, TRUE)) {
    /// we process audio effects elsewhere
    lives_freep((void **)&out_channels);
    return FILTER_ERROR_IS_AUDIO;
  }

  in_channels = weed_instance_get_in_channels(inst, NULL);

  // TODO - for now we will always convert to unclamped, most times this is the better option
  /* if (dry_run) { */
  /*   // after building nodemodel and plan, the first time we exectue plan with real layers */
  /*   // there is one theing we need to check */
  /*   // - if the layer palette is yuv, we may not know ahead of time the clamping / sampling / subspace values */
  /*   // as these may be set in the clip_src. */
  /*   // if the values do not align with the channel values, we have two options, either convert layer each time */
  /*   // or update the channel details. Obviously we would rather adjust the instance channels to avoid the conversion */
  /*   // but some instances may require a reinit if these detials change. We check for this here by doing a preliinary */
  /*   // dry run of the plan (pulling srcs, but not applying fx). If we note that a reinit is required, the reinit is */
  /*   // done, and then the plan execution continues with the loaded sources. */
  /*   // - use in_tracks to map layers to channels */
  /*   // compoare layer against channel */
  /*   // NOTES: 1) default is unclamped, YCbCr, def sampling, this is what we set node and channels to initially */
  /*   // 2) if layer differs from this, we decide whether to convert layer or change channels */
  /*   // - if we dont have pvary, then ouput will be the same, so this ripples down */
  /*   // - converting from yuv(a)888(a) -> rgb, we prefer unclamped, as we can use RGBA / RGB for YUV / YUVA */
  /*   // - if we dont have pvary, and we have other inputs they will have to be set the same way */
  /*   //   this can induce qloss into the model. We can recalc costs with and without this change. */
  /*   // with clamped - palconv may take longer if pal cannot be resized directly, as we need to do clamped ->unclamped */
  /*   // thus the time saving here is contered if we ever need to swithc to RGB. */
  /*   // - thus, viding conversion only makes sense when - the palette is resizable with no masquerading */
  /*   // or there are no resizes to the display (unlikely), or we only resize after converting to RGB */
  /*   // and there are no other inputs with unclamped YUV, and we never mix with unclamped up to sink */
  /*   // so - go through in_tracks, if we find layer withunclamped YUV, exit */
  /*   // if we find layer with clmaped. check channel */
  /*   // if not resizable directly: */
  /*   // follow along node chain. If we resize between clip_src and sink or pt where we conv to RGB */
  /*   // the we should convert, maybe we already did. If we  mix with othe unclamped, then exit */

  /*   if (weed_palette_is_yuv(opalette)) { */
  /*     for (int z = 0; z < 2; z++) { */
  /* 	if (z) { */
  /* 	  max = num_out_tracks; */
  /* 	  is_in = FALSE; */
  /* 	} */
  /* 	for (i = 0; i < num_in_tracks; i++) { */
  /* 	  channel = get_enabled_channel(inst, i, IN_CHAN); */
  /* 	  if (!channel) continue; */
  /* 	  if (weed_channel_is_disabled(channel)) continue; */

  /* 	  chantmpl = weed_channel_get_template(channel); */
  /* 	  channel_flags = weed_chantmpl_get_flags(chantmpl); */

  /* 	  if (channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) { */
  /* 	    // check clamping, sampoling subspace for layer, if it does not match channel */
  /* 	    // we flag a reinit for this instance */
  /* 	    cpalette = weed_channel_get_palette_yuv(channel, &iclamping, &isampling, &isubspace); */
  /* 	    if (!weed_palette_is_yuv(cpalette)) continue; */

  /* 	    osampling = WEED_YUV_SAMPLING_DEFAULT; */
  /* 	    if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_SAMPLING)) { */
  /* 	      osampling = weed_get_int_value(chantmpl, WEED_LEAF_YUV_SAMPLING, NULL); */
  /* 	    } else if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_SAMPLING)) { */
  /* 	      osampling = weed_get_int_value(filter, WEED_LEAF_YUV_SAMPLING, NULL); */
  /* 	    } */

  /* 	    oclamping = WEED_YUV_CLAMPING_UNCLAMPED; */
  /* 	    if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_CLAMPING)) { */
  /* 	      oclamping = weed_get_int_value(chantmpl, WEED_LEAF_YUV_CLAMPING, NULL); */
  /* 	    } else if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_CLAMPING)) { */
  /* 	      oclamping = weed_get_int_value(filter, WEED_LEAF_YUV_CLAMPING, NULL); */
  /* 	    } */

  /* 	    osubspace = WEED_YUV_SUBSPACE_YUV; */
  /* 	    if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_SUBSPACE)) { */
  /* 	      osubspace = weed_get_int_value(chantmpl, WEED_LEAF_YUV_SUBSPACE, NULL); */
  /* 	    } else if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_SUBSPACE)) { */
  /* 	      osubspace = weed_get_int_value(filter, WEED_LEAF_YUV_SUBSPACE, NULL); */
  /* 	    } */

  /* 	    if (oclamping != iclamping || isampling != osampling || isubspace != osubspace) { */
  /* 	      inode->needs_reinit = TRUE; */
  /* 	    }} */
  /* 	  if (!pvary) break; */
  /* 	}} */
  /*   } */
  /*   // *INDENT-ON* */
  /*   goto done_video; */
  /* } */

  // special handling for alpha in channels, if we have no pixel_data fed in, create it

  if (!in_channels || !has_video_chans_in(filter, TRUE) || all_ins_alpha(filter, TRUE)) {
    // no in_chans, or no no video chans _in, or all inputs alpha
    if ((!out_channels && weed_plant_has_leaf(inst, WEED_LEAF_OUT_PARAMETERS)) ||
        (all_outs_alpha(filter, TRUE))) {
      // has no out_chans, but does have out params, or else all outputs are alpha
      //
      for (i = 0; (channel = get_enabled_channel(inst, i, LIVES_OUTPUT)) != NULL; i++) {
        pdata = weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);
        if (!pdata) {
          width = DEF_FRAME_HSIZE_43S_UNSCALED; // TODO: default size for new alpha only channels
          height = DEF_FRAME_VSIZE_43S_UNSCALED; // TODO: default size for new alpha only channels

          set_channel_size(filter, channel, &width, &height);

          if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
            int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
            if (rowstride_alignment_hint  > THREADVAR(rowstride_alignment))
              THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
          }

          // this will look at width, height, current_palette, and create an empty pixel_data and set rowstrides
          // and update width and height if necessary
          if (!create_empty_pixel_data(channel, FALSE, TRUE)) {
            retval = FILTER_ERROR_MEMORY_ERROR;
            goto done_video;
          }
          retval = run_process_func(inst, tc);
        }
      }
      lives_freep((void **)&in_channels);
      return retval;
    }
    lives_freep((void **)&in_channels);
    lives_freep((void **)&out_channels);

    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (!get_enabled_channel(inst, 0, LIVES_INPUT)) {
    /// we process generators elsewhere
    lives_freep((void **)&in_channels);
    lives_freep((void **)&out_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  // SETUP: in_tracks and out_tracks

  if (!init_event) {
    num_in_tracks = enabled_in_channels(inst, FALSE);
    in_tracks = (int *)lives_calloc(2, sizint);
    in_tracks[1] = 1;
    num_out_tracks = enabled_out_channels(inst, FALSE);
    out_tracks = (int *)lives_calloc(1, sizint);
  } else {
    in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
    out_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_OUT_TRACKS, &num_out_tracks);
  }

  /// handle case where in_tracks[i] > than num layers
  /// either we temporarily disable the channel, or we can't apply the filter
  num_inc = weed_leaf_num_elements(inst, WEED_LEAF_IN_CHANNELS);

  for (i = 0; i < num_inc; i++) {
    if (weed_channel_is_alpha(in_channels[i]) && !weed_channel_is_disabled(in_channels[i]))
      num_in_alpha++;
  }

  num_inc -= num_in_alpha;

  // FOR ALPHA CHANNELS, PULL CHANNEL DATA using cconx
  retval = check_cconx(inst, num_inc + num_in_alpha, &needs_reinit);
  if (retval != FILTER_SUCCESS) {
    goto done_video;
  }

  if (num_in_tracks > num_inc) num_in_tracks = num_inc; // for example, compound fx

  // DISABLE unmatched channels, ENABLE some others

  // TODO - see if we can use enable_disable_channels
  //
  /// if we have more in_channels in the effect than in_tracks, we MUST (temp) disable the extra in_channels

  if (num_inc > num_in_tracks) {
    for (i = num_in_tracks; i < num_inc + num_in_alpha; i++) {
      if (!weed_channel_is_alpha(in_channels[i])) {
        if (!weed_channel_is_disabled(in_channels[i]))
          weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        else weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
      }
    }
  }

  // count the actual layers fed in
  if (mainw->scrap_file != -1) {
    for (i = 0; i < num_in_tracks; i++) {
      layer = layers[i];
      clip = lives_layer_get_clip(layer);
      if (clip == mainw->scrap_file && num_in_tracks <= 1 && num_out_tracks <= 1) {
        retval = FILTER_ERROR_IS_SCRAP_FILE;
        goto done_video;
      }
    }
  }

  for (k = i = 0; i < num_in_tracks; i++) {
    if (in_tracks[i] < 0) {
      retval = FILTER_ERROR_INVALID_TRACK; // probably audio
      goto done_video;
    }

    // we may have some channels with alpha, there will be a layer for this, but no in_track
    while (k < num_inc + num_in_alpha && (weed_channel_is_alpha(in_channels[k])
                                          || weed_channel_is_disabled(in_channels[k]))) k++;

    if (k >= num_inc + num_in_alpha) break;

    channel = in_channels[k];
    weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);

    if (in_tracks[i] >= mainw->num_tracks) {
      /// here we have more in_tracks than actual layers (this can happen if we have blank frames)
      /// disable some optional channels if we can
      for (j = k; j < num_inc + num_in_alpha; j++) {
        if (weed_channel_is_alpha(in_channels[j])) continue;
        channel = in_channels[j];
        chantmpl = weed_channel_get_template(channel);
        if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl)))
          if (!weed_channel_is_disabled(channel))
            weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
          else weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
        else {
          retval = FILTER_ERROR_MISSING_LAYER;
          goto done_video;
        }
      }
      break;
    }
    k++;
  }

  for (i = k = 0; i < num_in_tracks; i++) {
    // skip over alpha channels and disabled channel
    while (k < num_inc + num_in_alpha && (weed_channel_is_alpha(in_channels[k])
                                          || weed_channel_is_disabled(in_channels[k]))) k++;

    if (k >= num_inc + num_in_alpha) break;

    // connect the kth in_channel to
    channel = in_channels[k];
    layer = layers[in_tracks[i++]];

    //frame = lives_layer_get_frame(layer);

    if (!layer || !weed_layer_get_pixel_data(layer)) {
      // missing layer -
      /// temp disable channels if we can (if the channel is optional or a repeatable channel)
      chantmpl = weed_channel_get_template(channel);
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl))) {
        if (!weed_channel_is_disabled(channel))
          weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
      } else {
        weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
        retval = FILTER_ERROR_BLANK_FRAME;
        goto done_video;
	// *INDENT-OFF*
      }}
  }
  // *INDENT-ON*

  /// ensure all chantmpls NOT marked "optional" have at least one corresponding enabled channel
  /// e.g. we could have disabled all channels from a template with "max_repeats" that is not optional
  in_ctmpls = weed_filter_get_in_chantmpls(filter, &num_ctmpl);
  mand = (int *)lives_calloc(num_ctmpl, sizint);
  for (i = 0; i < num_inc + num_in_alpha; i++) {
    /// skip disabled in channels
    if (weed_channel_is_disabled(in_channels[i]) ||
        weed_get_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;
    chantmpl = weed_channel_get_template(in_channels[i]);
    for (j = 0; j < num_ctmpl; j++) {
      /// mark the non disabled in channels
      if (chantmpl == in_ctmpls[j]) {
        mand[j] = 1;
        break;
      }
    }
  }

  for (j = 0; j < num_ctmpl; j++) {
    /// quit if a mandatory channel has been disabled
    if (mand[j] == 0 && !weed_chantmpl_is_optional(in_ctmpls[j])) {
      lives_freep((void **)&in_ctmpls);
      lives_freep((void **)&mand);
      retval = FILTER_ERROR_MISSING_LAYER;
      goto done_video;
    }
  }

  lives_freep((void **)&in_ctmpls);
  lives_freep((void **)&mand);

  //////////
  /// that is it for in_channels setup, now we move on to out_channels
  ///////

  out_channels = weed_instance_get_out_channels(inst, &num_outc);

  for (i = 0; i < num_outc; i++) {
    if (weed_channel_is_alpha(out_channels[i])) {
      if (!weed_channel_is_disabled(out_channels[i]))
        num_out_alpha++;
    } else {
      if (!weed_channel_is_disabled(out_channels[i]) &&
          !weed_get_boolean_value(out_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL)) {
        nmandout++;
      }
    }
  }

  if (!init_event || num_compound_fx(inst) > 1) num_out_tracks -= num_out_alpha;

  if (num_out_tracks < 0) num_out_tracks = 0;

  if (nmandout > num_out_tracks) {
    /// occasionally during recording we get an init_event with no WEED_LEAF_OUT_TRACKS
    /// (probably when an audio effect inits/deinits a video effect)
    // - needs more investigation if it is still happening
    retval = FILTER_ERROR_MISSING_CHANNEL;
    goto done_video;
  }

  // now we link layers to channels
  // - any existing pdata for in channels is freed
  // - pdata is shared from layers to in channels
  //
  // - free any existing pdata for out channels
  // - if an out channel is inplace, share pdata from corresponding layer
  //    otherwise, create new pdata for out channel

  // after applying the instance:
  // nullify all in_chans,
  //
  // if out chan is not inplace, free layer, share pdata to layer
  // nullify out_chan
  //
  // free / unref will take care of nullify

  // if filte is busy - skip step of free layer, share pdata to layer

  // i.e for inplace, layer pdata is shared with in channel, out chanel and layer
  // for non inplace, layer pdata is shared with in channel, out chanel gets new pdata
  //    in channel / layer pdata is freed, out channel pdata is shared to layer

  // pdata for alpha channels is a little different - this is not passed around in layers,
  // instead there is an "alpha channel connection". The pdata from the previous out channel is
  // simply transferred directly to the in channel. Thus we must ensure that out channel alpha
  // pdata is not freed or nullified (this is handled elsewhere)

  for (i = 0, k = 0; k < num_inc + num_in_alpha; k++) {
    /// since layers and channels are interchangeable, we just call weed_layer_copy(channel, layer)
    /// the cast to weed_layer_t * is unnecessary, but added for clarity
    channel = get_enabled_channel(inst, k, LIVES_INPUT);
    if (weed_channel_is_alpha(channel) &&
        weed_get_boolean_value(channel, WEED_LEAF_HOST_ORIG_PDATA, NULL)) continue;

    layer = layers[in_tracks[i++]];

    lock_layer_status(layer);
    lstat = _lives_layer_get_status(layer);
    if (lstat == LAYER_STATUS_INVALID) {
      unlock_layer_status(layer);
      retval = FILTER_ERROR_INVALID_LAYER;
      goto done_video;
    }

    if (lstat == LAYER_STATUS_READY ||
        lstat == LAYER_STATUS_LOADED)
      _lives_layer_set_status(layer, LAYER_STATUS_BUSY);

    unlock_layer_status(layer);

    if (weed_pixel_data_share(channel, layer) != LIVES_RESULT_SUCCESS) {
      retval = FILTER_ERROR_COPYING_FAILED;
      goto done_video;
    }
  }

  for (i = 0; i < num_out_tracks + num_out_alpha; i++) {
    boolean inplace = FALSE;
    // check for INPLACE.
    channel = get_enabled_channel(inst, i, LIVES_OUTPUT);
    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL))
      continue;
    if (weed_channel_is_alpha(channel)) continue;

    chantmpl = weed_channel_get_template(channel);
    channel_flags = weed_chantmpl_get_flags(chantmpl);

    layer = layers[out_tracks[i]];

    lock_layer_status(layer);
    lstat = _lives_layer_get_status(layer);
    if (lstat == LAYER_STATUS_INVALID) {
      unlock_layer_status(layer);
      retval = FILTER_ERROR_INVALID_LAYER;
      goto done_video;
    }

    if (lstat == LAYER_STATUS_READY ||
        lstat == LAYER_STATUS_LOADED)
      _lives_layer_set_status(layer, LAYER_STATUS_BUSY);

    unlock_layer_status(layer);

    if (channel_flags & WEED_CHANNEL_CAN_DO_INPLACE) {
      // if the filter can thread, avoid doing inplace unless low on memory
      if (!can_thread(filter) || bigblock_occupancy() > 50.) {
        if (!weed_pixel_data_share(channel, layer)) {
          retval = FILTER_ERROR_COPYING_FAILED;
          goto done_video;
        }
        inplace = TRUE;
        weed_set_boolean_value(channel, WEED_LEAF_HOST_INPLACE, TRUE);
      }
    }

    if (!inplace) {
      weed_set_boolean_value(channel, WEED_LEAF_HOST_INPLACE, FALSE);

      if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
        int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
        if (rowstride_alignment_hint > THREADVAR(rowstride_alignment)) {
          THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
        }
      }

      lives_layer_copy_metadata(channel, layer, FALSE);

      if (!create_empty_pixel_data(channel, FALSE, TRUE)) {
        retval = FILTER_ERROR_MEMORY_ERROR;
        goto done_video;
      }
    }
  }

  if (CURRENT_CLIP_IS_VALID)
    weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);

  // RUN THE INSTANCE //////////////
  retval = run_process_func(inst, tc);
  //////////////////////

  if (retval == FILTER_ERROR_INVALID_PLUGIN) {
    goto done_video;
  }

  // now we write our out channels back to layers, leaving the palettes and sizes unchanged

  if (retval == FILTER_ERROR_BUSY) busy = TRUE;

  for (i = k = 0; k < num_out_tracks + num_out_alpha; k++) {
    weed_plant_t *channel = get_enabled_channel(inst, k, LIVES_OUTPUT);
    boolean inplace = FALSE;

    if (!channel) break; // compound fx

    /// set the timecode for the channel (this is optional, but its good to do to timestamp the output)
    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);

    if (weed_channel_is_alpha(channel)) {
      // out chan data for alpha is freed after all fx proc - in case we need for in chans
      continue;
    }

    layer = layers[out_tracks[i++]];

    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_INPLACE, NULL)) inplace = TRUE;

    // output layer
    if (!inplace && !busy) {
      if (weed_pixel_data_share(layer, channel) != LIVES_RESULT_SUCCESS) {
        retval = FILTER_ERROR_COPYING_FAILED;
        goto done_video;
      }
    }
  }

done_video:

  if (needs_reinit && retval == FILTER_SUCCESS)
    retval = FILTER_ERROR_NEEDS_REINIT;

  for (i = 0; i < num_out_tracks + num_out_alpha; i++) {
    weed_plant_t *channel = get_enabled_channel(inst, i, LIVES_OUTPUT);
    weed_layer_pixel_data_free(channel);
    layer = layers[out_tracks[i]];
    lock_layer_status(layer);
    lstat = _lives_layer_get_status(layer);
    if (lstat == LAYER_STATUS_INVALID) {
      retval = FILTER_ERROR_INVALID_LAYER;
    } else {
      if (!lives_layer_plan_controlled(layer)) {
        if (lstat == LAYER_STATUS_BUSY)
          _lives_layer_set_status(layer, LAYER_STATUS_READY);
      }
    }
    unlock_layer_status(layer);
  }

  for (i = 0; i < num_inc + num_in_alpha; i++) {
    channel = get_enabled_channel(inst, i, LIVES_INPUT);
    weed_layer_pixel_data_free(channel);
    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL)) {
      weed_set_boolean_value(channel, WEED_LEAF_DISABLED, WEED_FALSE);
      weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, FALSE);
    }
    layer = layers[in_tracks[i]];
    if (!lives_layer_plan_controlled(layer)) {
      lock_layer_status(layer);
      lstat = _lives_layer_get_status(layer);
      if (lstat == LAYER_STATUS_INVALID) {
        retval = FILTER_ERROR_INVALID_LAYER;
      } else {
        if (lstat == LAYER_STATUS_BUSY)
          _lives_layer_set_status(layer, LAYER_STATUS_INVALID);
      }
      unlock_layer_status(layer);
    }
  }
  lives_freep((void **)&in_tracks);
  lives_freep((void **)&out_tracks);
  lives_freep((void **)&in_channels);
  lives_freep((void **)&out_channels);

  return retval;
}


static lives_filter_error_t enable_disable_channels(weed_plant_t *inst, int direction, int *tracks, int num_tracks,
    int nbtracks,
    weed_plant_t **layers) {
  // handle case where in_channels > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_plant_t *channel, **channels, *chantmpl, **ctmpls = NULL, *layer;
  float **adata = NULL;
  void **pixdata = NULL;
  boolean *mand;
  int maxcheck = num_tracks, i, j, num_ctmpls, num_channels;

  if (direction == LIVES_INPUT)
    channels = weed_instance_get_in_channels(inst, &num_channels);
  else
    channels = weed_instance_get_out_channels(inst, &num_channels);

  if (num_tracks > num_channels) maxcheck = num_tracks = num_channels;
  if (num_channels > num_tracks) maxcheck = num_channels;

  for (i = 0; i < maxcheck; i++) {
    channel = channels[i];
    weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);

    // skip disabled channels for now
    if (weed_channel_is_disabled(channel)) continue;
    if (i < num_tracks) layer = layers[tracks[i] + nbtracks];
    else layer = NULL;

    if (!layer || ((weed_layer_is_audio(layer) && !(adata = weed_layer_get_audio_data(layer, NULL))) ||
                   (weed_layer_is_video(layer) && (pixdata = weed_layer_get_pixel_data_planar(layer, NULL)) == NULL))) {
      // if the layer data is NULL and it maps to a repeating channel, then disable the channel temporarily
      chantmpl = weed_channel_get_template(channel);
      if (weed_chantmpl_get_max_repeats(chantmpl) != 1) {
        if (!weed_channel_is_disabled(channel)) {
          weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        } else weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
      } else {
        if (adata) lives_free(adata);
        lives_free(channels);
        lives_freep((void **)&ctmpls);
        lives_freep((void **)&pixdata);
        return FILTER_ERROR_MISSING_LAYER;
      }
    }
    lives_freep((void **)&pixdata);
    if (adata) lives_free(adata);
  }

  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not also "optional"
  if (direction == LIVES_INPUT)
    ctmpls = weed_filter_get_in_chantmpls(filter, &num_ctmpls);
  else
    ctmpls = weed_filter_get_out_chantmpls(filter, &num_ctmpls);

  if (num_ctmpls > 0) {
    // step through all the active channels and mark the template as fulfilled
    mand = (int *)lives_calloc(num_ctmpls, sizint);
    for (i = 0; i < num_channels; i++) {
      if (weed_channel_is_disabled(channels[i]) ||
          weed_get_boolean_value(channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;
      chantmpl = weed_channel_get_template(channels[i]);
      for (j = 0; j < num_ctmpls; j++) {
        if (chantmpl == ctmpls[j]) {
          mand[j] = TRUE;
          break;
        }
      }
    }

    for (j = 0; j < num_ctmpls; j++) if (mand[j] == FALSE && (!weed_chantmpl_is_optional(ctmpls[j]))) {
        lives_freep((void **)&ctmpls);
        lives_freep((void **)&mand);
        lives_free(channels);
        lives_freep((void **)&ctmpls);
        return FILTER_ERROR_MISSING_LAYER;
      }
    lives_freep((void **)&mand);
  } else {
    lives_free(channels);
    lives_freep((void **)&ctmpls);
    return FILTER_ERROR_TEMPLATE_MISMATCH;
  }

  lives_freep((void **)&ctmpls);
  lives_free(channels);
  return FILTER_SUCCESS;
}


lives_filter_error_t run_process_func(weed_plant_t *instance, weed_timecode_t tc) {
  weed_plant_t *filter = weed_instance_get_filter(instance, FALSE);
  weed_process_f process_func;
  lives_filter_error_t retval = FILTER_SUCCESS;
  boolean did_thread = FALSE;
  //int64_t timex = lives_get_current_ticks();

  // see if we can multithread
  if (can_thread(filter)) {
    weed_plant_t **out_channels = weed_instance_get_out_channels(instance, NULL);
    prefs->nfx_threads = future_prefs->nfx_threads;
    retval = process_func_threaded(instance, tc);
    lives_free(out_channels);
    if (retval != FILTER_ERROR_DONT_THREAD) did_thread = TRUE;
  }

  if (!did_thread) {
    // normal single threaded version
    process_func = (weed_process_f)weed_get_funcptr_value(filter, WEED_LEAF_PROCESS_FUNC, NULL);
    if (process_func) {
      weed_error_t ret = (*process_func)(instance, tc);
      if (ret == WEED_ERROR_PLUGIN_INVALID) retval = FILTER_ERROR_INVALID_PLUGIN;
      if (ret == WEED_ERROR_FILTER_INVALID) retval = FILTER_ERROR_INVALID_FILTER;
      if (ret == WEED_ERROR_NOT_READY) retval = FILTER_ERROR_BUSY;
    } else retval = FILTER_ERROR_INVALID_PLUGIN;
  }
  weed_leaf_delete(instance, WEED_LEAF_RANDOM_SEED);
  //  thrdit = !thrdit;
  return retval;
}


/// here, in_tracks and out_tracks map our layers array to in_channels and out_channels in the filter
/// each layer may only map to zero or one in channel and zero or one out channel.

/// in addition, each mandatory channel in/out must have a layer mapped to it.
// If an optional, repeatable  channel is unmatched
/// we disable it temporarily. We don't disable channels permanently here
// since other functions will handle that more complex issue.
static lives_filter_error_t weed_apply_audio_instance_inner(weed_plant_t *inst, weed_plant_t *init_event,
    weed_plant_t **layers, weed_timecode_t tc, int nbtracks) {

  // TODO - handle the following:
  // input audio_channels are mono, but the plugin NEEDS stereo; we should duplicate the audio.

  // TODO - handle plugin return errors better, eg. NEEDS_REINIT

  int *in_tracks = NULL, *out_tracks = NULL;

  weed_layer_t *layer;
  weed_plant_t **in_channels = NULL, *channel, *chantmpl;

  float **adata;

  lives_filter_error_t retval = FILTER_SUCCESS;

  int channel_flags;
  int num_in_tracks, num_out_tracks;

  int num_inc;

  int nchans = 0;
  int nsamps = 0;

  int i, j;

  if (!get_enabled_channel(inst, 0, LIVES_INPUT)) {
    // we process generators elsewhere
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (!init_event) {
    num_in_tracks = enabled_in_channels(inst, FALSE);
    in_tracks = (int *)lives_calloc(2, sizint);
    in_tracks[1] = 1;
    if (!get_enabled_channel(inst, 0, LIVES_OUTPUT)) num_out_tracks = 0;
    else {
      num_out_tracks = 1;
      out_tracks = (int *)lives_calloc(1, sizint);
    }
  } else {
    in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
    out_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_OUT_TRACKS, &num_out_tracks);
  }

  in_channels = weed_instance_get_in_channels(inst, &num_inc);
  if (num_in_tracks > num_inc) num_in_tracks = num_inc;

  for (i = 0; i < num_in_tracks; i++) {
    layer = layers[in_tracks[i] + nbtracks];
    channel = get_enabled_channel(inst, i, LIVES_INPUT);

    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA_LENGTH);
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_RATE);
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_CHANNELS);

    /* g_print("setting ad for channel %d from layer %d. val %p, eg %f \n", i, in_tracks[i] + nbtracks, weed_get_voidptr_value(layer,
       "audio_data", NULL), ((float *)(weed_get_voidptr_value(layer, "audio_data", NULL)))[0]); */
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA);
    /* g_print("setting afterval %p, eg %f \n", weed_get_voidptr_value(channel, "audio_data", NULL),
       ((float *)(weed_get_voidptr_value(channel, "audio_data", NULL)))[0]); */
  }

  // set up our out channels
  for (i = 0; i < num_out_tracks; i++) {
    weed_plant_t *inchan;

    if (out_tracks[i] != in_tracks[i]) {
      retval = FILTER_ERROR_INVALID_TRACK; // we dont do swapping around of audio tracks
      goto done_audio;
    }

    channel = get_enabled_channel(inst, i, LIVES_OUTPUT);
    inchan = get_enabled_channel(inst, i, LIVES_INPUT);
    layer = layers[out_tracks[i] + nbtracks];

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);
    chantmpl = weed_channel_get_template(channel);
    channel_flags = weed_chantmpl_get_flags(chantmpl);

    /// set up the audio data for each out channel.
    // IF we can inplace then we use the audio_data from the corresponding in_channel
    /// otherwise we allocate it
    if ((channel_flags & WEED_CHANNEL_CAN_DO_INPLACE)
        && weed_channel_get_audio_rate(channel) == weed_channel_get_audio_rate(inchan)
        && weed_channel_get_naudchans(channel) == weed_channel_get_naudchans(inchan)) {
      weed_set_boolean_value(layer, WEED_LEAF_HOST_INPLACE, WEED_TRUE);
      lives_leaf_dup(channel, inchan, WEED_LEAF_AUDIO_DATA);
      /* g_print("setting dfff ad for channel %p from chan %p, eg %f \n", weed_get_voidptr_value(channel, "audio_data", NULL),
        weed_get_voidptr_value(inchan, "audio_data", NULL), ((float *)(weed_get_voidptr_value(layer, "audio_data", NULL)))[0]); */
    } else {
      nchans = weed_layer_get_naudchans(layer);
      adata = (float **)lives_calloc(nchans, sizeof(float *));
      nsamps = weed_layer_get_audio_length(layer);
      for (i = 0; i < nchans; i++) {
        adata[i] = lives_calloc(nsamps, sizeof(float));
        if (!adata[i]) {
          for (--i; i > 0; i--) lives_free(adata[i]);
          lives_free(adata);
          retval = FILTER_ERROR_MEMORY_ERROR;
          goto done_audio;
        }
      }
      weed_set_boolean_value(layer, WEED_LEAF_HOST_INPLACE, WEED_FALSE);
      weed_set_voidptr_array(channel, WEED_LEAF_AUDIO_DATA, nchans, (void **)adata);
      lives_free(adata);
    }

    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA_LENGTH);
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_RATE);
    lives_leaf_dup(channel, layer, WEED_LEAF_AUDIO_CHANNELS);
  }

  if (CURRENT_CLIP_IS_VALID) weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);

  //...finally we are ready to apply the filter
  retval = run_process_func(inst, tc);

  if (retval == FILTER_ERROR_INVALID_PLUGIN || retval == FILTER_ERROR_INVALID_FILTER)
    goto done_audio;

  // TODO - handle process errors (e.g. WEED_ERROR_PLUGIN_INVALID)

  // now we write our out channels back to layers
  for (i = 0; i < num_out_tracks; i++) {
    channel = get_enabled_channel(inst, i, LIVES_OUTPUT);
    layer = layers[out_tracks[i] + nbtracks];
    if (weed_plant_has_leaf(channel, WEED_LEAF_AUDIO_DATA)) {
      /// free any old audio data in the layer, unless it's INPLACE
      /// or KEEP_ADATA is set (i.e readonly input)
      if (weed_get_boolean_value(layer, WEED_LEAF_HOST_INPLACE, NULL) == WEED_FALSE
          && weed_get_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, NULL) == WEED_FALSE) {
        if (retval == FILTER_ERROR_BUSY) {
          adata = (float **)weed_get_voidptr_array_counted(channel, WEED_LEAF_AUDIO_DATA, &nchans);
          channel = layer;
          weed_set_boolean_value(layer, WEED_LEAF_HOST_INPLACE, WEED_TRUE);
        } else adata = (float **)weed_get_voidptr_array_counted(layer, WEED_LEAF_AUDIO_DATA, &nchans);
        for (j = 0; j < nchans; j++) lives_freep((void **)&adata[j]);
        lives_freep((void **)&adata);
        // shallow copy
        lives_leaf_dup(layer, channel, WEED_LEAF_AUDIO_DATA);
      }
    }
  }

  for (i = 0; i < num_inc; i++) {
    if (weed_get_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) {
      weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
    }
  }

done_audio:
  lives_freep((void **)&in_tracks);
  lives_freep((void **)&out_tracks);
  lives_freep((void **)&in_channels);

  return retval;
}


/**
   @brief apply an audio filter to a stack of audio tracks

   init_event may contain a pointer to the init_event in the event_list (if any)
   this is used to map tracks to the in and out channels of the filter indtance

   if NULL (e.g. during free playback) we create a fake channel map mapping layer 0 -> in, layer 0 -> out.

   abuf is a float ** array of audio, with each element being 1 audio channel (as in left/ right)
   grouped by track number
   nchans is number of audio channels (1 or 2 currently) in each track
   nbtracks is the number of backing tracks; track number will be -nbtracks <= track <= n
   track no. >= 0 is audio from a video track in multitrack, < 0 is a backing audio track (audio only, no video)
   nsamps is the sample size, arate the audio rate, tc the current timecode of the event_list or player
   and vis is a matrix of "visibility": 1 = front layer, full volume, 0 - rear layer, no volume, and a value in between
   can result from a transition.

   During rendering the audio render process feeds small chunks at a time to be processed, with the final result passing into
   the channel mixer (volume and pan)

   We set the values for the in and out channels, possibly disabling or enabling some.
   IF the instance needs reiniting (because od our changes and its filter / chantmpl flags) then we reinit it

   we runi the process_func and the output is returned to abuf, using the out_tracks to map the out channels
*/
lives_filter_error_t weed_apply_audio_instance(weed_plant_t *init_event, weed_layer_t **layers, int nbtracks, int nchans,
    int64_t nsamps, double arate, weed_timecode_t tc, double * vis) {
  lives_filter_error_t retval = FILTER_SUCCESS, retval2;

  weed_plant_t **in_channels = NULL, **out_channels = NULL;

  weed_plant_t *instance = NULL, *orig_inst, *filter;
  weed_plant_t *channel = NULL;
  weed_plant_t *ctmpl;

  int *in_tracks = NULL, *out_tracks = NULL;

  boolean needs_reinit = FALSE;
  boolean was_init_event = FALSE;

  int flags = 0, cflags;
  int key = -1;
  boolean rvary = FALSE, lvary = FALSE;

  int ntracks = 1;
  int numinchans = 0, numoutchans = 0, xnchans, xxnchans, xrate;

  int i;

  // caller passes an init_event from event list, or an instance (for realtime mode)

  if (WEED_PLANT_IS_FILTER_INSTANCE(init_event)) {
    // for realtime, we pass a single instance instead of init_event

    instance = init_event; // needs to be refcounted

    in_channels = weed_instance_get_in_channels(instance, NULL);
    out_channels = weed_instance_get_out_channels(instance, NULL);

    if (!in_channels) {
      if (!out_channels && weed_plant_has_leaf(instance, WEED_LEAF_OUT_PARAMETERS)) {
        // plugin has no in channels and no out channels,
        // but if it has out parameters then it must be a data processing module
        ;
        // if the data feeds into audio effects then we run it now, otherwise we will run it during the video cycle
        if (!feeds_to_audio_filters(key, rte_key_getmode(key + 1))) return FILTER_ERROR_NO_IN_CHANNELS;

        // otherwise we just run the process_func() and return

        if (weed_get_boolean_value(instance, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE) {
          retval = weed_reinit_effect(instance, FALSE);
          if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
              || retval == FILTER_ERROR_INVALID_FILTER) {
            weed_instance_unref(instance);
            return retval;
          }
        }

        if (CURRENT_CLIP_IS_VALID) weed_set_double_value(instance, WEED_LEAF_FPS, cfile->pb_fps);
        retval = run_process_func(instance, tc);
        return retval;
      }
      lives_free(out_channels);
      return FILTER_ERROR_NO_IN_CHANNELS;
    }

    lives_free(in_channels);
    lives_free(out_channels);
    in_channels = out_channels = NULL;

    // set init_event to NULL before passing it to the inner function
    init_event = NULL;

    in_tracks = (int *)lives_calloc(1, sizint);
    out_tracks = (int *)lives_calloc(1, sizint);
  } else {
    // when processing an event list, we pass an init_event
    was_init_event = TRUE;
    if (weed_plant_has_leaf(init_event, WEED_LEAF_HOST_TAG)) {
      char *keystr = weed_get_string_value(init_event, WEED_LEAF_HOST_TAG, NULL);
      key = atoi(keystr);
      lives_freep((void **)&keystr);
    } else return FILTER_ERROR_INVALID_INIT_EVENT;

    in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &ntracks);

    if (weed_plant_has_leaf(init_event, WEED_LEAF_OUT_TRACKS))
      out_tracks = weed_get_int_array(init_event, WEED_LEAF_OUT_TRACKS, NULL);

    // check instance exists, and interpolate parameters

    if (rte_key_valid(key + 1, FALSE)) {
      if ((instance = weed_instance_obtain(key, key_modes[key])) == NULL) {
        lives_freep((void **)&in_tracks);
        lives_freep((void **)&out_tracks);
        return FILTER_ERROR_INVALID_INSTANCE;
      }
      if (mainw->multitrack && !mainw->unordered_blocks && mainw->pchains && mainw->pchains[key]) {
        /// interpolate the params, if we can get a mutex lock on the inst
        if (!interpolate_params(instance, mainw->pchains[key], tc)) {
          weed_instance_unref(instance);
          lives_freep((void **)&in_tracks);
          lives_freep((void **)&out_tracks);
          return FILTER_ERROR_INTERPOLATION_FAILED;
        }
      }
    } else {
      lives_freep((void **)&in_tracks);
      lives_freep((void **)&out_tracks);
      return FILTER_ERROR_INVALID_INIT_EVENT;
    }
  }

  orig_inst = instance;

audinst1:
  lives_freep((void **)&in_channels);
  lives_freep((void **)&out_channels);

  in_channels = weed_instance_get_in_channels(instance, &numinchans);
  out_channels = weed_instance_get_out_channels(instance, &numoutchans);

  retval = enable_disable_channels(instance, LIVES_INPUT, in_tracks, ntracks, nbtracks, layers);
  // in theory we should check out channels also, but for now we only load audio filters with one mandatory out

  if (retval != FILTER_SUCCESS) goto audret1;

  filter = weed_instance_get_filter(instance, FALSE);
  flags = weed_filter_get_flags(filter);

  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) lvary = TRUE;

  if (vis && vis[0] < 0. && ((nbtracks > 0 && in_tracks[0] <= -nbtracks) || (!nbtracks && in_tracks[0] < 0))) {
    /// first layer comes from ascrap file; do not apply any effects except the final audio mixer
    if (numinchans == 1 && numoutchans == 1 && !(flags & WEED_FILTER_IS_CONVERTER)) {
      retval = FILTER_ERROR_IS_SCRAP_FILE;
      goto audret1;
    }
  }

  if (weed_get_boolean_value(instance, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE)
    needs_reinit = TRUE;

  for (i = 0; i < numinchans; i++) {
    if ((channel = in_channels[i]) == NULL) continue;
    if (weed_channel_is_disabled(channel) ||
        weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;

    xnchans = nchans; // preferred value
    ctmpl = weed_channel_get_template(channel);
    cflags = weed_chantmpl_get_flags(ctmpl);
    // TODO ** - can be list
    if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNELS))
      xnchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_CHANNELS, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_MAX_AUDIO_CHANNELS)) {
      xxnchans = weed_get_int_value(filter, WEED_LEAF_MAX_AUDIO_CHANNELS, NULL);
      if (xxnchans > 0 && xxnchans < nchans) xnchans = xxnchans;
    }
    if (xnchans != nchans) {
      retval = FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }

    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, NULL) != nchans
        && (cflags & WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE))
      needs_reinit = TRUE;
    else weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, nchans);

    //if ((weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNEL_LAYOUT) {
    // TODO - check layouts if present, make sure it supports mono / stereo

    xrate = arate;
    // TODO : can be list
    if (rvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_RATE, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(filter, WEED_LEAF_AUDIO_RATE, NULL);
    if (arate != xrate) {
      // TODO - resample
      retval = FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }

    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_RATE, NULL) != arate) {
      if (cflags & WEED_CHANNEL_REINIT_ON_RATE_CHANGE) {
        needs_reinit = TRUE;
      }
    }

    weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, arate);
  }

  for (i = 0; i < numoutchans; i++) {
    if ((channel = out_channels[i]) == NULL) continue;
    if (weed_channel_is_disabled(channel) ||
        weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;
    xnchans = nchans; // preferred value
    ctmpl = weed_channel_get_template(channel);
    cflags = weed_chantmpl_get_flags(ctmpl);
    // TODO ** - can be list
    if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNELS))
      xnchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_CHANNELS, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_MAX_AUDIO_CHANNELS)) {
      xxnchans = weed_get_int_value(filter, WEED_LEAF_MAX_AUDIO_CHANNELS, NULL);
      if (xxnchans > 0 && xxnchans < nchans) xnchans = xxnchans;
    }
    if (xnchans != nchans) {
      retval = FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }
    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, NULL) != nchans
        && (cflags & WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE))
      needs_reinit = TRUE;
    else weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, nchans);

    //if ((weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNEL_LAYOUT) {
    // TODO - check layouts if present, make sure it supports mono / stereo

    xrate = arate;
    // TODO : can be list
    if (rvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_RATE, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(filter, WEED_LEAF_AUDIO_RATE, NULL);
    if (arate != xrate) {
      // TODO - resample
      retval = FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }

    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_RATE, NULL) != arate) {
      if (cflags & WEED_CHANNEL_REINIT_ON_RATE_CHANGE) {
        needs_reinit = TRUE;
      }
    }

    weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, arate);
  }

  if (needs_reinit) {
    lives_filter_error_t filter_error = weed_reinit_effect(instance, FALSE);
    if (filter_error == FILTER_ERROR_COULD_NOT_REINIT || filter_error == FILTER_ERROR_INVALID_PLUGIN
        || filter_error == FILTER_ERROR_INVALID_FILTER) {
      retval = FILTER_ERROR_COULD_NOT_REINIT;
      goto audret1;
    }
    retval = FILTER_INFO_REINITED;
  }

  // apply visibility mask to volume values
  if (vis && (flags & WEED_FILTER_IS_CONVERTER)) {
    int vmaster = get_master_vol_param(filter, FALSE);
    if (vmaster != -1) {
      int nvals;
      weed_plant_t **in_params = weed_instance_get_in_params(instance, NULL);
      double *fvols = weed_get_double_array_counted(in_params[vmaster], WEED_LEAF_VALUE, &nvals);
      for (i = 0; i < nvals; i++) {
        fvols[i] = fvols[i] * vis[in_tracks[i] + nbtracks];
        if (vis[in_tracks[i] + nbtracks] < 0.) fvols[i] = -fvols[i];
      }
      weed_set_double_array(in_params[vmaster], WEED_LEAF_VALUE, nvals, fvols);
      lives_freep((void **)&fvols);
      lives_freep((void **)&in_params);
    }
  }

  retval2 = weed_apply_audio_instance_inner(instance, init_event, layers, tc, nbtracks);
  if (retval == FILTER_SUCCESS) retval = retval2;

  if (retval2 == FILTER_SUCCESS && was_init_event) {
    // handle compound filters
    weed_plant_t *xinstance;
    if ((xinstance = get_next_compound_inst(instance)) != NULL) {
      if (instance != orig_inst) weed_instance_unref(instance);
      weed_instance_ref(xinstance);
      instance = xinstance;
      goto audinst1;
    }
  }

  // TODO - handle invalid, needs_reinit etc.

audret1:
  if (instance != orig_inst) weed_instance_unref(instance);
  if (was_init_event) weed_instance_unref(orig_inst);

  lives_freep((void **)&in_channels);
  lives_freep((void **)&out_channels);

  lives_freep((void **)&in_tracks);
  lives_freep((void **)&out_tracks);

  return retval;
}


/**
   @brief - this is called during rendering - we will have previously received a filter_map event and now we apply this to layers

   layers will be a NULL terminated array of channels, each with two extra leaves: clip and frame
   clip corresponds to a LiVES file in mainw->files. WEED_LEAF_PIXEL_DATA will initially be NULL, we will pull this as necessary
   and the effect output is written back to the layers

   a frame number of 0 indicates a blank frame;
   if an effect gets one of these it will not process (except if the channel is optional,
   in which case the channel is disabled); the same is true for invalid track numbers -
   hence disabled channels which do not have disabled in the template are re-enabled when a frame is available

   after all processing, we generally display/output the first non-NULL layer.
   If all layers are NULL we generate a blank frame

   size and palettes of resulting layers may change during this

   the channel sizes are set by the filter: all channels are all set to the size of the largest input layer.
   (We attempt to do this, but some channels have fixed sizes).
**/
static void weed_apply_filter_map(weed_plant_t **layers, weed_plant_t *filter_map, int opwidth, int opheight,
                                  weed_timecode_t tc, void ***pchains) {
  weed_plant_t *instance, *orig_inst = NULL, *xinstance;
  weed_plant_t *init_event;
  lives_filter_error_t retval;

  char *keystr;

  void **init_events;

  weed_error_t filter_error;
  int key, num_inst;

  boolean needs_reinit;

  if (!filter_map || !weed_plant_has_leaf(filter_map, WEED_LEAF_INIT_EVENTS) ||
      !weed_get_voidptr_value(filter_map, WEED_LEAF_INIT_EVENTS, NULL)) return;

  init_events = weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &num_inst);

  for (int i = 0; i < num_inst; i++) {
    if (orig_inst) weed_instance_unref(orig_inst);
    orig_inst = NULL;
    init_event = (weed_plant_t *)init_events[i];

    if (weed_plant_has_leaf(init_event, WEED_LEAF_HOST_TAG)) {
      keystr = weed_get_string_value(init_event, WEED_LEAF_HOST_TAG, NULL);
      key = atoi(keystr);
      lives_free(keystr);
      if (rte_key_valid(key + 1, FALSE)) {
        if ((instance = weed_instance_obtain(key, key_modes[key])) == NULL) continue;
        orig_inst = instance;
        if (is_pure_audio(instance, FALSE)) {
          continue; // audio effects are applied in the audio renderer
        }
        if (!LIVES_IS_PLAYING && mainw->multitrack && mainw->multitrack->current_rfx
            && mainw->multitrack->current_rfx->source
            && (mainw->multitrack->solo_inst || instance == mainw->multitrack->current_rfx->source)) {
          if (instance == mainw->multitrack->current_rfx->source) {
            void **pchain = mt_get_pchain();
            // interpolation can be switched of by setting mainw->no_interp
            if (!mainw->no_interp && pchain) {
              interpolate_params(instance, pchain, tc); // interpolate parameters for preview
            }
          } else {
            if (mainw->multitrack->solo_inst) continue;
          }
        } else {
          int idx;
          char *filter_hash;
          boolean is_valid = FALSE;
          if (!weed_plant_has_leaf(init_events[i], WEED_LEAF_OUT_TRACKS)
              || !weed_plant_has_leaf(init_events[i], WEED_LEAF_IN_TRACKS)) {
            continue;
          }
          if (mainw->multitrack && mainw->multitrack->solo_inst && mainw->multitrack->init_event
              && !LIVES_IS_PLAYING && mainw->multitrack->init_event != init_events[i]) {
            continue;
          }
          filter_hash = weed_get_string_value(init_events[i], WEED_LEAF_FILTER, NULL);
          if ((idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
            weed_plant_t *filter = get_weed_filter(idx);
            if (has_video_chans_in(filter, FALSE) && has_video_chans_out(filter, FALSE)) {
              int nintracks, *in_tracks = weed_get_int_array_counted(init_events[i],
                                          WEED_LEAF_IN_TRACKS, &nintracks);
              for (int j = 0; j < nintracks; j++) {
                if (j >= mainw->num_tracks) break;
                if (mainw->active_track_list[in_tracks[j]] > 0) {
                  is_valid = TRUE;
                  break;
                }
              }
              lives_free(in_tracks);
            }
          }
          lives_free(filter_hash);
          /// avoid applying to non-active tracks
          if (!is_valid) {
            continue;
          }
          if (mainw->multitrack && !mainw->unordered_blocks && pchains && pchains[key]) {
            interpolate_params(instance, pchains[key], tc); // interpolate parameters during playback
          }
        }
        /*
          // might be needed for multitrack ???
          if (mainw->pconx!=NULL) {
          int key=i;
          int mode=key_modes[i];
          if (weed_plant_has_leaf(instance,WEED_LEAF_HOST_MODE)) {
          key=weed_get_int_value(instance,WEED_LEAF_HOST_KEY,&error);
          mode=weed_get_int_value(instance,WEED_LEAF_HOST_MODE,&error);
          }
          // chain any data pipelines
          pconx_chain_data(key,mode);
          }*/

apply_inst2:

        if (weed_plant_has_leaf(instance, WEED_LEAF_HOST_NEXT_INSTANCE)) {
          // chain any internal data pipelines for compound fx
          needs_reinit = pconx_chain_data_internal(instance);
          if (needs_reinit) {
            retval = weed_reinit_effect(instance, FALSE);
            if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
                || retval == FILTER_ERROR_INVALID_FILTER) {
              if (instance != orig_inst) weed_instance_unref(instance);
              if (!LIVES_IS_PLAYING && mainw->multitrack && mainw->multitrack->solo_inst &&
                  orig_inst == mainw->multitrack->solo_inst) break;
              continue;
            }
          }
        }

        //if (LIVES_IS_PLAYING)
        filter_error = weed_apply_instance(instance, init_event, layers, opwidth, opheight, tc);

        if (filter_error == WEED_SUCCESS && (xinstance = get_next_compound_inst(instance)) != NULL) {
          if (instance != orig_inst) weed_instance_unref(instance);
          weed_instance_ref(xinstance);
          instance = xinstance;
          goto apply_inst2;
        }
        if (filter_error == WEED_ERROR_REINIT_NEEDED) {
          retval = weed_reinit_effect(instance, FALSE);
          if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
              || retval == FILTER_ERROR_INVALID_FILTER) {
            weed_instance_unref(orig_inst);
            if (!LIVES_IS_PLAYING && mainw->multitrack && mainw->multitrack->solo_inst &&
                orig_inst == mainw->multitrack->solo_inst) break;
            continue;
          }
        }
        //if (filter_error!=FILTER_SUCCESS) lives_printerr("Render error was %d\n",filter_error);
        if (!LIVES_IS_PLAYING && mainw->multitrack && mainw->multitrack->solo_inst &&
            orig_inst == mainw->multitrack->solo_inst) {
          break;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (orig_inst) weed_instance_unref(orig_inst);
  lives_freep((void **)&init_events);
}


lives_filter_error_t act_on_instance(weed_instance_t *instance, int key, lives_layer_t **layers,
                                     int opwidth, int opheight) {
  // check and apply instance
  // checks:
  // if this is an audio instance, skip it
  // NB:
  // if the instance is "soft deinited", we simply skip the instance and continue to the next plan step
  // but we flag to rebuild the nodemodel

  // - double check to make sure this is not an audio effect
  // - if we are in multitrack mode, interpolate params for the instance / timecode

  // we first chain any data from parameter data connections
  // this can cause the instance to need reiniting
  // which we have to do now, since we cannot predict this

  weed_filter_t *filter;
  weed_instance_t *orig_inst = instance, *xinstance;
  weed_gui_t *gui;
  weed_timecode_t tc = 0;
  lives_filter_error_t filter_error = FILTER_SUCCESS;
  int myeaseval = -1;

  if (!instance) return FILTER_ERROR_INVALID_INSTANCE;

  filter = weed_instance_get_filter(instance, TRUE);
  if (is_pure_audio(filter, TRUE)) return FILTER_ERROR_IS_AUDIO;

  tc = mainw->currticks;

  if ((mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))) tc = mainw->cevent_tc;

  if (mainw->multitrack && !mainw->unordered_blocks && pchains[key]) {
    tc = mainw->cevent_tc;
    interpolate_params(instance, mainw->pchains[key], tc); // interpolate parameters during preview
  }

apply_inst3:

  if (weed_plant_has_leaf(instance, WEED_LEAF_HOST_NEXT_INSTANCE)) {
    // chain any internal data pipelines for compound fx
    boolean needs_reinit = pconx_chain_data_internal(instance);
    if (needs_reinit) return FILTER_ERROR_NEEDS_REINIT;
  }

  // if the plugin is supposed to be easing out, make sure it is really doing so
  gui = weed_instance_get_gui(instance, FALSE);
  if (gui && weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT))
    myeaseval = weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL);

  if (myeaseval) filter_error = weed_apply_instance(instance, NULL, layers, opwidth, opheight, tc);
  else filter_error = FILTER_SUCCESS;

  if (myeaseval >= 0) {
    //g_print("mev is %d\n", myeaseval);
    int xeaseval = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL);
    if (!xeaseval) {
      if (!weed_plant_has_leaf(instance, LIVES_LEAF_AUTO_EASING)) {
        rte_key_on_off(key, FALSE);
        return FILTER_ERROR_INVALID_INSTANCE;
      }
    }
    // count how many frames to ease out
    weed_set_int_value(gui, WEED_LEAF_EASE_OUT, xeaseval);
  }

  //#define DEBUG_RTE
#ifdef DEBUG_RTE
  if (filter_error != FILTER_SUCCESS) lives_printerr("Render error was %d\n", filter_error);
#endif

  if (filter_error == FILTER_SUCCESS && (xinstance = get_next_compound_inst(instance))) {
    if (instance != orig_inst) weed_instance_unref(instance);
    weed_instance_ref(xinstance);
    instance = xinstance;
    goto apply_inst3;
  }

  if (mainw->pconx && (filter_error == FILTER_SUCCESS || filter_error == FILTER_INFO_REINITED
                       || filter_error == FILTER_INFO_REDRAWN)) {
    pconx_chain_data_omc(orig_inst, key, key_modes[key]);
  }

  return filter_error;
}


weed_plant_t *weed_apply_effects(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc,
                                 int opwidth, int opheight, void ***pchains, boolean dry_run) {
  // given a stack of layers, a filter map, a timecode and possibly parameter chains
  // apply the effects in the filter map, and return a single layer as the result

  // if all goes wrong we return a blank 4x4 RGB24 layer (TODO - return a NULL ?)

  // returned layer can be of any width,height,palette
  // caller should free all input layers, WEED_LEAF_PIXEL_DATA of all non-returned layers is free()d here

  weed_plant_t *instance, *orig_inst = NULL;
  weed_layer_t *layer;

  int output = -1;
  int clip;

  if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
    // rendering from multitrack
    if (filter_map && layers[0]) {
      weed_apply_filter_map(layers, filter_map, opwidth, opheight, tc, pchains);
    }
  }

  // free playback: we will have here only one or two layers, and no filter_map.
  // Effects are applied in key order, in tracks are 0 and 1, out track is 0
  else {
    for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
      if (rte_key_valid(i + 1, TRUE)) {
        if (!(rte_key_is_enabled(i, FALSE))) {
          // if anything is connected to ACTIVATE, the fx may be activated
          pconx_chain_data(i, key_modes[i], FALSE);
        }
        if (rte_key_is_enabled(i, TRUE)) {
          // adds a ref
          if ((instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
            continue;
          }
          act_on_instance(instance, i, layers, opwidth, opheight);
          weed_instance_unref(orig_inst);
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
    if (orig_inst) {
    }
  }
  if (dry_run) return NULL;

  // TODO - set mainw->vpp->play_params from connected out params and out alphas

  // caller should free all layers, but here we will free all other pixel_data
  // (apart from return layer)
  for (int i = 0; layers[i]; i++) {
    //check if this is a candidate for output_layer, which will not be freed
    if (((mainw->multitrack && i == mainw->multitrack->preview_layer)
         || (!mainw->multitrack || mainw->multitrack->preview_layer < 0))
        && (weed_layer_get_pixel_data(layers[i])
            || (lives_layer_get_frame(layers[i]) != 0
                && (LIVES_IS_PLAYING || !mainw->multitrack || !mainw->multitrack->current_rfx
                    || !mainw->multitrack->init_event || tc < get_event_timecode(mainw->multitrack->init_event) ||
                    mainw->multitrack->init_event == mainw->multitrack->avol_init_event ||
                    tc > get_event_timecode(weed_get_plantptr_value
                                            (mainw->multitrack->init_event, WEED_LEAF_DEINIT_EVENT, NULL)))))) {
      if (output != -1 || weed_get_int_value(layers[i], WEED_LEAF_CLIP, NULL) == -1) {
        if (!weed_layer_get_pixel_data(layers[i]))
          wait_layer_ready(layers[i], FALSE);
        if (!weed_layer_get_pixel_data(layers[i])) continue;
        weed_layer_pixel_data_free(layers[i]);
      } else output = i;
    } else {
      if (!weed_layer_get_pixel_data(layers[i]))
        wait_layer_ready(layers[i], FALSE);
      if (!weed_layer_get_pixel_data(layers[i])) continue;
      weed_layer_pixel_data_free(layers[i]);
    }
  }

  if (output == -1) {
    // blank frame - e.g. for multitrack
    return create_blank_layer(NULL, NULL, opwidth, opheight, WEED_PALETTE_END);
  }

  layer = layers[output];
  clip = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);

  // frame is pulled uneffected here. TODO: Try to pull at target output palette
  if (!weed_layer_get_pixel_data(layer)) {
    wait_layer_ready(layer, FALSE);
    if (!weed_layer_get_pixel_data(layer)) {
      if (!pull_frame_at_size(layer, get_image_ext_for_type(mainw->files[clip]->img_type), tc, opwidth, opheight,
                              WEED_PALETTE_END)) {
        char *msg = lives_strdup_printf("weed_apply_effects created empty pixel_data at tc %ld, map was %p, clip = %d, frame = %d",
                                        tc, filter_map, clip, weed_get_int_value(layer, WEED_LEAF_FRAME, NULL));
        LIVES_WARN(msg);
        lives_free(msg);
        if (opwidth < 16) opwidth = 16;
        if (opheight < 16) opheight = 16;
        weed_layer_pixel_data_free(layer);
        create_blank_layer(layer, get_image_ext_for_type(mainw->files[clip]->img_type), opwidth, opheight, WEED_PALETTE_END);
      }
    }
  }
  return layer;
}


void weed_apply_audio_effects(weed_plant_t *filter_map, weed_layer_t **layers, int nbtracks, int nchans, int64_t nsamps,
                              double arate, weed_timecode_t tc, double * vis) {
  int num_inst;
  void **init_events;
  weed_plant_t *init_event, *filter;
  char *fhash;

  // this is called during rendering - we will have previously received a
  // filter_map event and now we apply this to audio (abuf)
  // abuf will be a NULL terminated array of float audio

  // the results of abuf[0] and abuf[1] (for stereo) will be written to fileno
  if (!filter_map || !weed_plant_has_leaf(filter_map, WEED_LEAF_INIT_EVENTS) ||
      !weed_get_voidptr_value(filter_map, WEED_LEAF_INIT_EVENTS, NULL)) {
    return;
  }

  if (mainw->multitrack && !mainw->unordered_blocks)
    mainw->pchains = get_event_pchains();
  else mainw->pchains = NULL;
  init_events = weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &num_inst);
  if (init_events && (num_inst > 1 || init_events[0])) {
    for (int i = 0; i < num_inst; i++) {
      init_event = (weed_plant_t *)init_events[i];
      fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, NULL);
      if (fhash) {
        filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
        lives_freep((void **)&fhash);
        if (has_audio_chans_in(filter, FALSE) && !has_video_chans_in(filter, FALSE) && !has_video_chans_out(filter, FALSE) &&
            has_audio_chans_out(filter, FALSE)) {
          weed_apply_audio_instance(init_event, layers, nbtracks, nchans, nsamps, arate, tc, vis);
        }
      }
      // TODO *** - also run any pure data processing filters which feed into audio filters
    }
  }
  lives_freep((void **)&init_events);
  mainw->pchains = NULL;
}


void weed_apply_audio_effects_rt(weed_layer_t *alayer, weed_timecode_t tc, boolean analysers_only, boolean is_audio_thread) {
  weed_plant_t *instance, *filter, *orig_inst = NULL, *new_inst;
  lives_filter_error_t filter_error;
  weed_layer_t *layers[1];
  boolean needs_reinit;
  int nsamps, arate, nchans;
  int idx;

  // free playback: apply any audio filters or analysers (but not generators)
  // Effects are applied in key order

  if (!alayer) return;

  nchans = weed_layer_get_naudchans(alayer);
  arate = weed_layer_get_audio_rate(alayer);
  nsamps = weed_layer_get_audio_length(alayer);

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (orig_inst) weed_instance_unref(orig_inst);
    orig_inst = NULL;

    // we only apply pure audio filters here
    idx = key_to_fx[i][key_modes[i]];
    if (idx == -1) continue;

    filter = weed_filters[idx];
    if (!filter || !is_pure_audio(filter, FALSE)) continue;

    // chain input data, as this may enable / disable the active state
    needs_reinit = pconx_chain_data(i, key_modes[i], TRUE);

    filter_mutex_lock(i);

    // ignore soft deinited fx
    if (!(rte_key_is_enabled(i, TRUE))) {
      filter_mutex_unlock(i);
      continue;
    }

    if ((orig_inst = instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
      filter_mutex_unlock(i);
      continue;
    }

    filter_mutex_unlock(i);

    if (mainw->multitrack && !mainw->unordered_blocks && mainw->pchains && mainw->pchains[i]) {
      interpolate_params(instance, mainw->pchains[i], tc); // interpolate parameters during preview
    }

    if (needs_reinit) {
      filter_error = weed_reinit_effect(instance, FALSE);
      if (filter_error == FILTER_ERROR_COULD_NOT_REINIT || filter_error == FILTER_ERROR_INVALID_PLUGIN
          || filter_error == FILTER_ERROR_INVALID_FILTER) {
        continue;
      }
      orig_inst = instance;
    }

apply_audio_inst2:

    if (weed_plant_has_leaf(instance, WEED_LEAF_HOST_NEXT_INSTANCE)) {
      // chain any internal data pipelines for compound fx
      needs_reinit = pconx_chain_data_internal(instance);
      if (needs_reinit) {
        filter_error = weed_reinit_effect(instance, FALSE);
        if (filter_error == FILTER_ERROR_COULD_NOT_REINIT || filter_error == FILTER_ERROR_INVALID_PLUGIN
            || filter_error == FILTER_ERROR_INVALID_FILTER) {
          if (instance != orig_inst) weed_instance_unref(instance);
          continue;
        }
      }
    }

    layers[0] = alayer;

    filter_error = weed_apply_audio_instance(instance, layers, 0, nchans, nsamps, arate, tc, NULL);

    if (filter_error == FILTER_SUCCESS && (new_inst = get_next_compound_inst(instance))) {
      if (instance != orig_inst) weed_instance_unref(instance);
      weed_instance_ref(new_inst);
      instance = new_inst;
      goto apply_audio_inst2;
    }
    if (filter_error == FILTER_ERROR_NEEDS_REINIT) {
      // TODO...
    }

    if (filter_error == FILTER_INFO_REINITED) update_widget_vis(NULL, i, key_modes[i]); // redraw our paramwindow
#ifdef DEBUG_RTE
    if (filter_error != FILTER_SUCCESS) lives_printerr("Render error was %d\n", filter_error);
#endif

    if (mainw->pconx && (filter_error == FILTER_SUCCESS || filter_error == FILTER_INFO_REINITED
                         || filter_error == FILTER_INFO_REDRAWN)) {

      new_inst = weed_instance_obtain(i, key_modes[i]);

      filter_mutex_lock(i);
      pconx_chain_data_omc(new_inst, i, key_modes[i]);
      filter_mutex_unlock(i);

      weed_instance_unref(instance);
      if (orig_inst == instance) orig_inst = new_inst;
      instance = new_inst;
    }
  }
  if (orig_inst) weed_instance_unref(orig_inst);
}


boolean has_audio_filters(lives_af_t af_type) {
  // do we have any active audio filters (excluding audio generators) ?
  // called from audio thread during playback
  weed_plant_t *filter;
  int idx;

  if (af_type == AF_TYPE_A && mainw->afbuffer) return TRUE;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (rte_key_is_enabled(i, TRUE)) {
        idx = key_to_fx[i][key_modes[i]];
        filter = weed_filters[idx];
        if (has_audio_chans_in(filter, FALSE) && !has_video_chans_in(filter, FALSE) && !has_video_chans_out(filter, FALSE)) {
          if ((af_type == AF_TYPE_A && has_audio_chans_out(filter, FALSE)) || // check for analysers only
              (af_type == AF_TYPE_NONA && !has_audio_chans_out(filter, FALSE))) // check for non-analysers only
            continue;
          return TRUE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  return FALSE;
}


boolean has_video_filters(boolean analysers_only) {
  // do we have any active video filters (excluding generators) ?
  weed_plant_t *filter;
  int idx;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (rte_key_is_enabled(i, TRUE)) {
        idx = key_to_fx[i][key_modes[i]];
        filter = weed_filters[idx];
        if (has_video_chans_in(filter, FALSE)) {
          if (analysers_only && has_video_chans_out(filter, FALSE)) continue;
          return TRUE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  return FALSE;
}


/////////////////////////////////////////////////////////////////////////

static int check_weed_plugin_info(weed_plant_t *plugin_info) {
  // verify the plugin_info returned from the plugin
  // TODO - print descriptive errors
  if (!weed_plant_has_leaf(plugin_info, WEED_LEAF_HOST_INFO)) return -1;
  if (!weed_plant_has_leaf(plugin_info, WEED_LEAF_VERSION)) return -2;
  if (!weed_plant_has_leaf(plugin_info, WEED_LEAF_FILTERS)) return -3;
  return weed_leaf_num_elements(plugin_info, WEED_LEAF_FILTERS);
}


int num_in_params(weed_plant_t *plant, boolean skip_hidden, boolean skip_internal) {
  weed_plant_t **params = NULL;
  int num_params, counted = 0;
  boolean is_template = (WEED_PLANT_IS_FILTER_CLASS(plant));

nip1:
  if (is_template) {
    if (!weed_get_plantptr_value(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL)) return 0;
    params = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
  } else {
    if (!weed_get_plantptr_value(plant, WEED_LEAF_IN_PARAMETERS, NULL)) goto nip1done;
    params = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETERS, &num_params);
  }

  if (!skip_hidden && !skip_internal) {
    counted += num_params;
    goto nip1done;
  }

  for (int i = 0; i < num_params; i++) {
    if (skip_hidden && is_hidden_param(plant, i)) continue;
    if (skip_internal && weed_plant_has_leaf(params[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
    counted++;
  }

nip1done:

  if (params) lives_free(params);

  // TODO: should be skip_internal or !skip_internal ?
  if (!is_template && skip_internal && (plant = get_next_compound_inst(plant)) != NULL) goto nip1;

  return counted;
}


int num_out_params(weed_plant_t *plant) {
  weed_plant_t **params = NULL;
  int num_params = 0;
  if (WEED_PLANT_IS_FILTER_CLASS(plant)) {
    if (!weed_get_plantptr_value(plant, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL)) return 0;
    params = weed_get_plantptr_array_counted(plant, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &num_params);
  } else {
    if (!weed_get_plantptr_value(plant, WEED_LEAF_OUT_PARAMETERS, NULL)) return 0;
    params = weed_get_plantptr_array_counted(plant, WEED_LEAF_OUT_PARAMETERS, &num_params);
  }
  if (params) lives_free(params);
  return num_params;
}


boolean has_usable_palette(weed_plant_t *filter, weed_plant_t *chantmpl) {
  int npals, palette = WEED_PALETTE_END;
  int *palettes = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);
  for (int i = 0; i < npals; i++) {
    palette = palettes[i];
    if (palette == WEED_PALETTE_END || is_usable_palette(palette)) break;
  }
  lives_free(palettes);
  return palette != WEED_PALETTE_END;
}


LIVES_GLOBAL_INLINE boolean weed_chantmpl_is_disabled(weed_chantmpl_t *ctmpl) {
  return ctmpl && weed_get_boolean_value(ctmpl, WEED_LEAF_HOST_DISABLED, NULL);
}


int enabled_in_channels(weed_plant_t *plant, boolean count_repeats) {
  // count number of non-disabled in channels (video and/or audio) for a filter or instance

  // NOTE: for instances, we do not count optional audio channels, even if they are enabled

  weed_plant_t **channels = NULL;
  weed_filter_t *filter;
  boolean is_template = WEED_PLANT_IS_FILTER_CLASS(plant);
  int enabled = 0;
  int num_channels;

  if (is_template) {
    // if a filter is passed, check chantmpls
    filter = plant;
    channels = weed_filter_get_in_chantmpls(filter, &num_channels);
    if (!channels) return 0;
  } else {
    // if an instance is passed, check channels
    filter = weed_instance_get_filter(plant, TRUE);
    channels = weed_instance_get_in_channels(plant, &num_channels);
    if (!channels) return 0;
  }

  for (int i = 0; i < num_channels; i++) {
    if (!is_template) {
      weed_plant_t *ctmpl = weed_channel_get_template(channels[i]);
      // do not count optional audio ins
      if (weed_chantmpl_is_audio(ctmpl) == WEED_TRUE) {
        if (weed_chantmpl_is_optional(ctmpl)) continue;
      }
      // we skip only perm disabled chans, and count any temp disabled
      if (!weed_channel_is_disabled(channels[i])) enabled++;
    } else {
      // skip alpha channels for multitrack
      if (mainw->multitrack && !has_non_alpha_palette(channels[i], filter)) continue;
      if (!weed_chantmpl_is_disabled(channels[i])) enabled++;
    }

    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl = channels[i];
      else chantmpl = weed_channel_get_template(channels[i]);
      if (weed_plant_has_leaf(channels[i], WEED_LEAF_MAX_REPEATS)) {
        if (weed_channel_is_disabled(channels[i]) &&
            !has_usable_palette(filter, chantmpl)) continue; // channel was disabled because palette is unusable
        repeats = weed_chantmpl_get_max_repeats(chantmpl) - 1;
        if (repeats == -1) repeats = 1000000;
        enabled += repeats;
      }
    }
  }

  if (channels) lives_freep((void **)&channels);

  return enabled;
}


int enabled_out_channels(weed_plant_t *plant, boolean count_repeats) {
  weed_plant_t **channels = NULL;
  weed_plant_t *filter;
  int enabled = 0;
  int num_channels, i;
  boolean is_template = WEED_PLANT_IS_FILTER_CLASS(plant);

  if (is_template) {
    filter = plant;
    channels = weed_filter_get_out_chantmpls(filter, &num_channels);
  } else {
    channels = weed_instance_get_out_channels(plant, &num_channels);
    filter = weed_instance_get_filter(plant, TRUE);
  }

  for (i = 0; i < num_channels; i++) {
    if (!is_template) {
      if (!weed_channel_is_disabled(channels[i])) enabled++;
    } else {
      if (!weed_chantmpl_is_disabled(channels[i])) enabled++;
    }
    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl = channels[i];
      else chantmpl = weed_channel_get_template(channels[i]);
      if (weed_plant_has_leaf(channels[i], WEED_LEAF_MAX_REPEATS)) {
        if (weed_channel_is_disabled(channels[i]) &&
            !has_usable_palette(filter, chantmpl)) continue; // channel was disabled because palette is unusable
        repeats = weed_chantmpl_get_max_repeats(chantmpl) - 1;
        if (repeats == -1) repeats = 1000000;
        enabled += repeats;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

  if (channels) lives_freep((void **)&channels);

  return enabled;
}


/////////////////////////////////////////////////////////////////////

static int min_audio_chans(weed_plant_t *plant) {
  int *ents;
  int ne, minas = 0, i;
  ents = weed_get_int_array_counted(plant, WEED_LEAF_AUDIO_CHANNELS, &ne);
  if (ne) {
    for (i = 0; i < ne; i++) {
      if (!minas || (ents[i] > 0 && ents[i] < minas)) minas = ents[i];
      if (minas == 1) break;
    }
    lives_free(ents);
  }
  return minas > 0 ? minas : 1;
}


static int check_for_lives(weed_plant_t *filter, int filter_idx) {
  // for LiVES, currently:
  // all filters must take 0, 1 or 2 mandatory/optional inputs and provide
  // 1 mandatory output (audio or video) or >1 optional (video only) outputs (for now)

  // or 1 or more mandatory alpha outputs (analysers)

  // filters can also have 1 mandatory input and no outputs and out parameters
  // (video analyzer)

  // they may have no outs provided they have out parameters (data sources or processors)

  // all channels used must support a limited range of palettes (for now)

  // filters can now have any number of mandatory in alphas, the effect will not be run unless the channels are filled
  // (by chaining to output of another fx)
  weed_plant_t **array = NULL;

  int chans_in_mand = 0; // number of mandatory channels
  int chans_in_opt_max = 0; // number of usable (by LiVES) optional channels
  int chans_out_mand = 0;
  int chans_out_opt_max = 0;
  int achans_in_mand = 0, achans_out_mand = 0;

  boolean is_audio = FALSE;
  boolean has_out_params = FALSE;
  boolean all_out_alpha = TRUE;
  boolean hidden = FALSE;
  boolean cvary = FALSE, pvary = FALSE;

  int flags = 0;
  int num_elements, i;
  int naudins = 0, naudouts = 0;
  int filter_achans = 0;
  int ctachans = 1;

  //g_print("checking %s\n", weed_filter_get_name(filter));
  // TODO - check seed types
  if (!weed_plant_has_leaf(filter, WEED_LEAF_NAME)) return 1;
  if (!weed_plant_has_leaf(filter, WEED_LEAF_AUTHOR)) return 2;
  if (!weed_plant_has_leaf(filter, WEED_LEAF_VERSION)) return 3;
  if (!weed_plant_has_leaf(filter, WEED_LEAF_PROCESS_FUNC)) return 4;

  flags = weed_filter_get_flags(filter);

  // for now we will only load realtime effects
  if (flags & WEED_FILTER_NON_REALTIME) return 5;
  if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) cvary = TRUE;
  if (flags & WEED_FILTER_PALETTES_MAY_VARY) pvary = TRUE;

  // count number of mandatory and optional in_channels
  array = weed_filter_get_in_chantmpls(filter, &num_elements);

  for (i = 0; i < num_elements; i++) {
    if (!weed_plant_has_leaf(array[i], WEED_LEAF_NAME)) {
      lives_freep((void **)&array);
      return 6;
    }

    if (weed_chantmpl_is_audio(array[i]) == WEED_TRUE) {
      /// filter has audio channels
      if (filter_achans == 0) {
        filter_achans = min_audio_chans(filter);
      }
      // filter set nothing, or set a bad n umber but channels may vary
      // we're ok if the total number of mandatory non repeaters is <= 2
      if (!weed_chantmpl_is_optional(array[i])) {
        if (!cvary && (ctachans = min_audio_chans(array[i])) > 0)
          naudins += ctachans;
        else naudins += filter_achans;
        if (naudins > 2) {
          if (prefs->show_dev_opts && !prefs->vj_mode) {
            // currently we only handle mono and stereo audio filters
            char *filtname = weed_filter_get_name(filter);
            char *pkgstring = weed_filter_get_package_name(filter);
            char *msg, *pkstr;
            if (pkgstring) pkstr = lives_strdup_printf(" from package %s ", pkgstring);
            else pkstr = lives_strdup("");
            msg = lives_strdup_printf("Cannot use filter %s%s\n"
                                      "as it requires at least %d input audio channels\n",
                                      filtname, pkstr, naudins);
            lives_free(filtname); lives_free(pkstr); lives_free(pkgstring);
            LIVES_INFO(msg);
            lives_free(msg);
          }
          lives_freep((void **)&array);
          return 70;
        }
      }
      is_audio = TRUE;
    }

    if (!is_audio) {
      if (!weed_plant_has_leaf(filter, WEED_LEAF_PALETTE_LIST)) {
        if (!pvary || !weed_plant_has_leaf(array[i], WEED_LEAF_PALETTE_LIST)) {
          lives_freep((void **)&array);
          return 16;
        }
      } else {
        if (!weed_plant_has_leaf(array[i], WEED_LEAF_PALETTE_LIST))
          lives_leaf_copy(array[i], WEED_LEAF_PALETTE_LIST, filter, WEED_LEAF_PALETTE_LIST);
      }
    }

    if (weed_chantmpl_is_optional(array[i])) {
      // is optional
      chans_in_opt_max++;

      // HOST_DISABLED - chantmpl flag
      weed_set_boolean_value(array[i], WEED_LEAF_HOST_DISABLED, WEED_TRUE);
    } else {
      if (has_non_alpha_palette(array[i], filter)) {
        if (!is_audio) chans_in_mand++;
        else achans_in_mand++;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

  if (num_elements > 0) lives_freep((void **)&array);
  if (chans_in_mand > 2) {
    // currently we only handle mono and stereo audio filters
    if (!prefs->vj_mode) {
      char *filtname = weed_filter_get_name(filter);
      char *pkgstring = weed_filter_get_package_name(filter);
      char *msg, *pkstr;
      lives_freep((void **)&array);
      if (pkgstring) pkstr = lives_strdup_printf(" from package %s ", pkgstring);
      else pkstr = lives_strdup("");
      msg = lives_strdup_printf("Cannot use filter %s%s\n"
                                "as it requires at least %d input video channels\n",
                                filtname, pkstr, chans_in_mand);
      lives_free(filtname); lives_free(pkstr); lives_free(pkgstring);
      LIVES_INFO(msg);
      lives_free(msg);
    }
    return 8; // we dont handle mixers yet...
  }
  if (achans_in_mand > 0 && chans_in_mand > 0) return 13; // can't yet handle effects that need both audio and video

  // count number of mandatory and optional out_channels
  array = weed_filter_get_out_chantmpls(filter, &num_elements);

  for (i = 0; i < num_elements; i++) {
    if (!weed_plant_has_leaf(array[i], WEED_LEAF_NAME)) {
      lives_freep((void **)&array);
      return 6;
    }

    if (weed_chantmpl_is_audio(array[i]) == WEED_TRUE) {
      if (!weed_chantmpl_is_optional(array[i])) {
        if (!cvary && (ctachans = min_audio_chans(array[i])) > 0) {
          naudouts += ctachans;
        } else {
          naudouts += filter_achans;
        }
        if (naudouts > 2) {
          // currently we only handle mono and stereo audio filters
          lives_freep((void **)&array);
          return 71;
        }
      }
      is_audio = TRUE;
      if (naudins == 1 && naudouts == 2) {
        // converting mono to stereo cannot be done like that
        lives_freep((void **)&array);
        return 72;
      }
      is_audio = TRUE;
    }

    if (!is_audio) {
      if (!weed_plant_has_leaf(filter, WEED_LEAF_PALETTE_LIST)) {
        if (!pvary || !weed_plant_has_leaf(array[i], WEED_LEAF_PALETTE_LIST)) {
          lives_freep((void **)&array);
          return 16;
        }
      } else {
        if (!weed_plant_has_leaf(array[i], WEED_LEAF_PALETTE_LIST))
          lives_leaf_copy(array[i], WEED_LEAF_PALETTE_LIST, filter, WEED_LEAF_PALETTE_LIST);
      }
    }

    if (weed_chantmpl_is_optional(array[i])) {
      // is optional
      chans_out_opt_max++;
      weed_set_boolean_value(array[i], WEED_LEAF_HOST_DISABLED, WEED_TRUE);
    } else {
      // is mandatory
      if (!is_audio) {
        chans_out_mand++;
        if (has_non_alpha_palette(array[i], filter)) all_out_alpha = FALSE;
      } else achans_out_mand++;
    }
  }

  if (num_elements > 0) lives_freep((void **)&array);
  if (weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) has_out_params = TRUE;

  if ((chans_out_mand > 1 && !all_out_alpha) || ((chans_out_mand + chans_out_opt_max + achans_out_mand < 1)
      && (!has_out_params))) {
    return 11;
  }
  if (achans_out_mand > 1 || (achans_out_mand == 1 && chans_out_mand > 0)) return 14;
  if (achans_in_mand >= 1 && achans_out_mand == 0 && !(has_out_params)) return 15;

  weed_add_plant_flags(filter, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_");
  if (weed_plant_has_leaf(filter, WEED_LEAF_GUI)) {
    weed_plant_t *gui = weed_get_plantptr_value(filter, WEED_LEAF_GUI, NULL);
    weed_add_plant_flags(gui, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_");
    if (weed_get_boolean_value(gui, WEED_LEAF_HIDDEN, NULL) == WEED_TRUE) {
      hidden = TRUE;
    }
  }

  if (hidden || (flags & WEED_FILTER_IS_CONVERTER)) {
    if (is_audio) {
      weed_set_boolean_value(filter, WEED_LEAF_HOST_MENU_HIDE, WEED_TRUE);
      if (enabled_in_channels(filter, TRUE) >= 1000000) {
        // this is a candidate for audio volume
        lives_fx_candidate_t *cand = &mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL];
        cand->list = lives_list_append(cand->list, LIVES_INT_TO_POINTER(filter_idx));
        cand->delegate = 0;
      }
    } else {
      weed_set_boolean_value(filter, WEED_LEAF_HOST_MENU_HIDE, WEED_TRUE);
      if (chans_in_mand == 1 && chans_out_mand == 1) {
        if ((flags & WEED_FILTER_IS_CONVERTER) && (flags & WEED_FILTER_CHANNEL_SIZES_MAY_VARY)) {
          // this is a candidate for resize
          lives_fx_candidate_t *cand = &mainw->fx_candidates[FX_CANDIDATE_RESIZER];
          cand->list = lives_list_append(cand->list, LIVES_INT_TO_POINTER(filter_idx));
          cand->delegate = 0;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  return 0;
}


// Weed function overrides /////////////////

weed_error_t weed_set_const_string_value(weed_plant_t *plant, const char *key, const char *string) {
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  if (!key || !*key) return WEED_ERROR_NOSUCH_LEAF;
  char *xstr = lives_strdup(string);
  weed_error_t err = weed_set_custom_value(plant, key, WEED_SEED_CONST_CHARPTR, xstr);
  if (err != WEED_SUCCESS) {
    lives_free(xstr);
    return err;
  }

  // set flags so - autodelete on free, unchangeable
  err = weed_leaf_set_flagbits(plant, key, LIVES_FLAG_FREE_ON_DELETE | WEED_FLAG_UNDELETABLE
                               | WEED_FLAG_IMMUTABLE | LIVES_FLAGS_RDONLY_HOST);
  if (err == WEED_SUCCESS)  err = weed_ext_set_element_size(plant, key, 0, lives_strlen(string));
  return err;
}


LIVES_GLOBAL_INLINE boolean weed_leaf_is_const_string(weed_plant_t *plant, const char *key) {
  if (!plant) return FALSE;
  return (weed_leaf_seed_type(plant, key) == WEED_SEED_CONST_CHARPTR);
}


LIVES_GLOBAL_INLINE const char *weed_get_const_string_value(weed_plant_t *plant,
    const char *key, weed_error_t *err) {
  weed_error_t xerr = WEED_SUCCESS;
  if (!plant) xerr = WEED_ERROR_NOSUCH_PLANT;
  else if (!key || !*key) xerr = WEED_ERROR_NOSUCH_LEAF;
  else if (!(weed_leaf_is_const_string(plant, key)))
    xerr = WEED_ERROR_WRONG_SEED_TYPE;
  if (xerr != WEED_SUCCESS) {
    if (err) *err = xerr;
    return NULL;
  }
  return weed_get_custom_value(plant, key, WEED_SEED_CONST_CHARPTR, err);
}


LIVES_GLOBAL_INLINE weed_size_t weed_leaf_const_string_len(weed_plant_t *plant, const char *key) {
  if (weed_leaf_is_const_string(plant, key))
    return weed_leaf_element_size(plant, key, 0);
  return 0;
}


boolean weed_leaf_autofree(weed_plant_t *plant, const char *key) {
  // free data pointed to by leaves flagged as autofree
  // the leaf is not actually deleted, only the data
  // the undeletable flag bit must be cleared before calling this,
  // (this is done automatically by LiVES if we get error undeletable
  // when deleting a leaf or freeing a plant)
  // and here we also clear the autodelete bit

  boolean bret = TRUE;
  if (plant) {
    int flags = weed_leaf_get_flags(plant, key);

    if (flags & LIVES_FLAG_FREE_ON_DELETE) {
      uint32_t st = weed_leaf_seed_type(plant, key);
      int nvals;
      bret = FALSE;
      if (!lives_strcmp(key, LIVES_LEAF_REFCOUNTER)) {
        lives_refcounter_t *refcount = (lives_refcounter_t *)
                                       weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
        pthread_mutex_destroy(&refcount->mutex);
      }
      switch (st) {
      case WEED_SEED_PLANTPTR: {
        weed_plantptr_t *pls = weed_get_plantptr_array_counted(plant, key, &nvals);
        for (int i = 0; i < nvals; i++) if (pls[i]) weed_plant_free(pls[i]);
        if (pls) lives_free(pls);
        lives_leaf_set_rdonly(plant, key, FALSE, FALSE);
        weed_set_plantptr_value(plant, key, NULL);
        lives_leaf_set_rdonly(plant, key, FALSE, flags & WEED_FLAG_IMMUTABLE);
        bret = TRUE;
      }
      break;
      case WEED_SEED_VOIDPTR: {
        void **data = weed_get_voidptr_array_counted(plant, key, &nvals);
        for (int i = 0; i < nvals; i++) if (data[i]) lives_free(data[i]);
        if (data) lives_free(data);
        lives_leaf_set_rdonly(plant, key, FALSE, FALSE);
        weed_set_voidptr_value(plant, key, NULL);
        lives_leaf_set_rdonly(plant, key, FALSE, flags & WEED_FLAG_IMMUTABLE);
        bret = TRUE;
      }
      break;
      case WEED_SEED_CONST_CHARPTR: {
        void **data = weed_get_custom_array_counted(plant, key, WEED_SEED_CONST_CHARPTR, &nvals);
        for (int i = 0; i < nvals; i++) if (data[i]) lives_free(data[i]);
        if (data) lives_free(data);
        lives_leaf_set_rdonly(plant, key, FALSE, FALSE);
        weed_set_custom_value(plant, key, WEED_SEED_CONST_CHARPTR, NULL);
        lives_leaf_set_rdonly(plant, key, FALSE, flags & WEED_FLAG_IMMUTABLE);
      }
      break;
      default: break;
      }
      weed_leaf_clear_flagbits(plant, key, LIVES_FLAG_FREE_ON_DELETE);
    }
  }
  return bret;
}


void  weed_plant_autofree(weed_plant_t *plant) {
  // if weed_plant_free fails becaus some leaves are undeletable
  // mark them as deletable, and autofree any flagged accordingly
  if (!plant) return;
  else {
    char **leaves = weed_plant_list_leaves(plant, NULL);
    if (leaves) {
      for (int i = 0; leaves[i]; i++) {
        weed_leaf_set_undeletable(plant, leaves[i], WEED_FALSE);
        // act on FREE_ON_DELETE, remove flag
        weed_leaf_autofree(plant, leaves[i]);
        _ext_free(leaves[i]);
      }
      _ext_free(leaves);
    }
  }
}


LIVES_GLOBAL_INLINE weed_error_t weed_leaf_set_autofree(weed_plant_t *plant, const char *key, boolean state) {
  // a leaf can be set to autofree, this can be done for a plantptr ot voidptr, array or scalar
  // the leaf is also set immutable
  // then when val is changed, we get error immutable, and will check for autofree.
  if (state)
    return weed_leaf_set_flagbits(plant, key, LIVES_FLAG_FREE_ON_DELETE | WEED_FLAG_IMMUTABLE
                                  | WEED_FLAG_UNDELETABLE);
  return weed_leaf_clear_flagbits(plant, key, LIVES_FLAG_FREE_ON_DELETE);
}

//static int nplants = 0;

weed_error_t weed_plant_free_host(weed_plant_t *plant) {
  // delete even undeletable plants
  weed_error_t err;
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  err = _weed_plant_free(plant);
  if (err == WEED_ERROR_UNDELETABLE) {
    // make remeining leaves deleteable
    // handle FREE_ON_DELETE
    weed_plant_autofree(plant);
    // try again
    return _weed_plant_free(plant);
  }
  return err;
}


weed_plant_t *weed_plant_new_host(int type) {
  weed_plant_t *plant = _weed_plant_new(type);
  return plant;
}


weed_error_t weed_leaf_delete_host(weed_plant_t *plant, const char *key) {
  // delete even undeletable leaves
  weed_error_t err = WEED_ERROR_NOSUCH_LEAF;
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  if (weed_plant_has_leaf(plant, key)) {
    err = _weed_leaf_delete(plant, key);
    if (err == WEED_ERROR_UNDELETABLE) {
      // *** check this - there may be leaves that absolutely MUST be undeletable
      weed_leaf_set_undeletable(plant, key, WEED_FALSE);
      // free any planptrs or voidptrs fallged with FREE_ON_DELETE
      weed_leaf_autofree(plant, key);
      // try again
      err = _weed_leaf_delete(plant, key);
      if (err != WEED_SUCCESS) {
        char *msg = lives_strdup_printf("Unable to delete weed leaf - internal error detected (%d)", err);
        lives_abort(msg);
      }
    }
  }
  return err;
}


weed_error_t weed_leaf_set_host(weed_plant_t *plant, const char *key, uint32_t seed_type,
                                weed_size_t num_elems, void *values) {
  // change even immutable leaves
  // WEED_FLAG_RDONLY_HOST can also be set if we want to make a leaf self readonly
  boolean autofree = FALSE;
  weed_error_t err;

  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
  if (err == WEED_ERROR_IMMUTABLE) {
    int32_t flags = weed_leaf_get_flags(plant, key);
    if ((flags & LIVES_FLAGS_RDONLY_HOST) == LIVES_FLAGS_RDONLY_HOST) return err;
    flags &= ~WEED_FLAG_IMMUTABLE;
    if (flags & LIVES_FLAG_FREE_ON_DELETE) {
      flags &= ~WEED_FLAG_UNDELETABLE;
      autofree = TRUE;
    }
    weed_leaf_set_flags(plant, key, flags);
    if (autofree) weed_leaf_autofree(plant, key);
    err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
    if (autofree) flags |= LIVES_FLAG_FREE_ON_DELETE | WEED_FLAG_UNDELETABLE;
    flags |= WEED_FLAG_IMMUTABLE;
    weed_leaf_set_flags(plant, key, flags);
  }
  return err;
}


LIVES_GLOBAL_INLINE weed_error_t lives_leaf_set_rdonly(weed_plant_t *plant, const char *key,
    boolean rdonly_host, boolean rdonly_plugin) {
  int32_t flags;
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  flags = weed_leaf_get_flags(plant, key);
  if (rdonly_host) flags |= LIVES_FLAGS_RDONLY_HOST;
  else {
    flags &= ~LIVES_FLAGS_RDONLY_HOST;
    if (rdonly_plugin) flags |= WEED_FLAG_IMMUTABLE;
  }
  return weed_leaf_set_flags(plant, key, flags);
}


// memory profiling for plugins

void *lives_monitor_malloc(size_t size) {
  void *p = malloc(size);
  fprintf(stderr, "plugin mallocing %ld bytes, got ptr %p\n", size, p);
  if (size == 1024) BREAK_ME("monitor_malloc");
  return NULL;
}


void lives_monitor_free(void *p) {
  //fprintf(stderr, "plugin freeing ptr ptr %p\n", p);
}


weed_error_t weed_leaf_set_monitor(weed_plant_t *plant, const char *key, weed_seed_t seed_type, weed_size_t num_elems,
                                   void *values) {
  weed_error_t err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
  if (values && !num_elems) BREAK_ME("bas aeeat set\n");
  g_print("PL setting %s in type %d\n", key, weed_plant_get_type(plant));
  if (WEED_PLANT_IS_GUI(plant) && !strcmp(key, WEED_LEAF_FLAGS)) g_print("Err was %d\n", err);
  return err;
}


weed_error_t weed_leaf_get_monitor(weed_plant_t *plant, const char *key, weed_size_t idx, void *value) {
  // plugins can be monitored for example
  weed_error_t err = _weed_leaf_get(plant, key, idx, value);
  return err;
}


weed_plant_t *host_info_cb(weed_plant_t *xhost_info, void *data) {
  // if the plugin called weed_boostrap during its weed_setup() as we requested,
  // then we will end up here
  // from weed_bootstrap()
  int id = LIVES_POINTER_TO_INT(data);

  int lib_weed_version = 0;
  int lib_filter_version = 0;

  if (id == 100) {
    // the openGL plugin
    our_plugin_id = 100;
    fxname = NULL;
  }

  suspect = TRUE;

  ncbcalls++;

  if (id != 100 && ncbcalls > 1) {
    return NULL;
  }

  if (!xhost_info) {
    return NULL;
  }

  expected_hi = xhost_info;

  if (id == our_plugin_id) {
    // GOOD !
    // plugin called weed_bootstrap as requested
    //g_print("plugin called weed_bootstrap, and we assigned it ID %d\n", our_plugin_id);
    weed_set_int_value(xhost_info, WEED_LEAF_HOST_IDENTIFIER, id);
    weed_set_boolean_value(xhost_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_FALSE);
    suspect = FALSE;
  } else {
    weed_set_int_value(xhost_info, WEED_LEAF_HOST_IDENTIFIER, 0);
    weed_set_boolean_value(xhost_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_TRUE);
  }

  // we can do cool stuff here !

  if (!suspect) {
    // let's check the versions the library is using...
    if (weed_plant_has_leaf(xhost_info, WEED_LEAF_WEED_ABI_VERSION)) {
      lib_weed_version = weed_get_int_value(xhost_info, WEED_LEAF_WEED_ABI_VERSION, NULL);

      // this allows us to load plugins compiled with older versions of the lib
      // we must not set it any higher than it is though
      // this is redundant really, since we already checked before calling weed_init()
      if (lib_weed_version > weed_abi_version)
        weed_set_int_value(xhost_info, WEED_LEAF_WEED_ABI_VERSION, weed_abi_version);
    }
    if (weed_plant_has_leaf(xhost_info, WEED_LEAF_FILTER_API_VERSION)) {
      lib_filter_version = weed_get_int_value(xhost_info, WEED_LEAF_FILTER_API_VERSION, NULL);
      if (lib_filter_version > WEED_FILTER_API_VERSION)
        weed_set_int_value(xhost_info, WEED_LEAF_FILTER_API_VERSION, WEED_FILTER_API_VERSION);
    }
  }

  if (weed_plant_has_leaf(xhost_info, WEED_LEAF_PLUGIN_INFO)) {
    // let's check the versions the plugin supports
    weed_plant_t *plugin_info = weed_get_plantptr_value(xhost_info, WEED_LEAF_PLUGIN_INFO, NULL);
    int pl_max_weed_abi, pl_max_filter_api;
    if (plugin_info) {
      expected_pi = plugin_info;
      // we don't need to bother with min versions, the lib will check for us
      /* if (weed_plant_has_leaf(plugin_info, WEED_LEAF_MIN_WEED_ABI_VERSION)) { */
      /* 	pl_min_weed_api = weed_get_int_value(plugin_info, WEED_LEAF_MIN_WEED_ABI_VERSION, NULL); */
      /* } */
      if (weed_plant_has_leaf(plugin_info, WEED_LEAF_MAX_WEED_ABI_VERSION)) {
        pl_max_weed_abi = weed_get_int_value(plugin_info, WEED_LEAF_MAX_WEED_ABI_VERSION, NULL);
        // we can support all versions back to 110
        if (pl_max_weed_abi < weed_abi_version && pl_max_weed_abi >= 110) {
          weed_set_int_value(xhost_info, WEED_LEAF_WEED_ABI_VERSION, pl_max_weed_abi);
        }
      }
      // we don't need to bother with min versions, the lib will check for us
      /* if (weed_plant_has_leaf(plugin_info, WEED_LEAF_MIN_WEED_API_VERSION)) { */
      /* 	pl_min_filter_api = weed_get_int_value(plugin_info, WEED_LEAF_MIN_FILTER_API_VERSION, NULL); */
      /* } */
      if (weed_plant_has_leaf(plugin_info, WEED_LEAF_MAX_WEED_API_VERSION)) {
        pl_max_filter_api = weed_get_int_value(plugin_info, WEED_LEAF_MAX_FILTER_API_VERSION, NULL);
        // we can support all versions back to 110
        if (pl_max_filter_api < WEED_FILTER_API_VERSION && pl_max_filter_api >= 110) {
          weed_set_int_value(xhost_info, WEED_LEAF_FILTER_API_VERSION, pl_max_filter_api);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  //  fprintf(stderr, "API versions %d %d / %d %d : %d %d\n", lib_weed_version, lib_filter_version,
  //pl_min_weed_api, pl_max_weed_api, pl_min_filter_api, pl_max_filter_api);

#if !USE_STD_MEMFUNCS
  // let's override some plugin functions...
  if (0 && id != 100) {
    weed_set_funcptr_value(xhost_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)_ext_malloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)_ext_free);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)_ext_realloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)_ext_calloc);
  } else {
    // id 100 is what we set for a playback plugin, openGL uses its own memory allocator in hardware
    weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)memset);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)malloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)free);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)realloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)calloc);
  }
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)lives_memcpy_monitor);
  weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)lives_memcpy);
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)lives_memset);
  weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMMOVE_FUNC, (weed_funcptr_t)lives_memmove);
#endif
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)monitor_malloc);
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)monitor_free);

  // since we redefined weed_leaf_set, weed_leaf_delete and weed_plant_free for ourselves,
  // we need to reset the plugin versions, since it will inherit ours by default when calling setup_func()

  weed_set_funcptr_value(xhost_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)_weed_leaf_set);
  weed_set_funcptr_value(xhost_info, WEED_LEAF_DELETE_FUNC, (weed_funcptr_t)_weed_leaf_delete);
  weed_set_funcptr_value(xhost_info, WEED_PLANT_FREE_FUNC, (weed_funcptr_t)_weed_plant_free);
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_NUM_ELEMENTS_FUNC, (weed_funcptr_t)_weed_leaf_num_elements);

  weed_set_string_value(xhost_info, WEED_LEAF_HOST_NAME, "LiVES");
  weed_set_string_value(xhost_info, WEED_LEAF_HOST_VERSION, LiVES_VERSION);

  weed_set_string_value(xhost_info, WEED_LEAF_LAYOUT_SCHEMES, "rfx");

  weed_set_int_value(xhost_info, WEED_LEAF_FLAGS, WEED_HOST_SUPPORTS_LINEAR_GAMMA
                     | WEED_HOST_SUPPORTS_PREMULTIPLIED_ALPHA);

  if (fxname && !lives_strcmp(fxname, "projectM")) {
    //  weed_set_funcptr_value(xhost_info, WEED_LEAF_SET_FUNC, (weed_funcptr_t)weed_leaf_set_monitor);
    //weed_set_int_value(xhost_info, WEED_LEAF_VERBOSITY, WEED_VERBOSITY_DEBUG);
  } else
    weed_set_int_value(xhost_info, WEED_LEAF_VERBOSITY, WEED_VERBOSITY_WARN);

  update_host_info(xhost_info);
  return xhost_info;
}


///////////////////////////////////////////////////////////////////////////////////////

static void gen_hashnames(int i, int j) {
  hashnames[i][0].string = 	NULL;
  hashnames[i][0].string = 	make_weed_hashname(j, TRUE, FALSE, 0, FALSE);
  hashnames[i][0].hash =  	lives_string_hash(hashnames[i][0].string);
  hashnames[i][1].string = 	NULL;
  hashnames[i][1].string = 	make_weed_hashname(j, TRUE, TRUE, 0, FALSE); // full dupes
  hashnames[i][1].hash =  	lives_string_hash(hashnames[i][1].string);
  hashnames[i][2].string = 	NULL;
  hashnames[i][2].string = 	make_weed_hashname(j, FALSE, FALSE, 0, FALSE); // partial pdupes
  hashnames[i][2].hash =  	lives_string_hash(hashnames[i][2].string);
  hashnames[i][3].string = 	NULL;
  hashnames[i][3].string =	make_weed_hashname(j, FALSE, TRUE, 0, FALSE);
  hashnames[i][3].hash =  	lives_string_hash(hashnames[i][3].string);

  hashnames[i][4].string = 	NULL;
  hashnames[i][4].string = 	make_weed_hashname(j, TRUE, FALSE, 0, TRUE);
  hashnames[i][4].hash =  	lives_string_hash(hashnames[i][4].string);
  hashnames[i][5].string = 	NULL;
  hashnames[i][5].string = 	make_weed_hashname(j, TRUE, TRUE, 0, TRUE); // full dupes
  hashnames[i][5].hash =  	lives_string_hash(hashnames[i][5].string);
  hashnames[i][6].string = 	NULL;
  hashnames[i][6].string = 	make_weed_hashname(j, FALSE, FALSE, 0, TRUE); // partial pdupes
  hashnames[i][6].hash =  	lives_string_hash(hashnames[i][6].string);
  hashnames[i][7].string = 	NULL;
  hashnames[i][7].string =	make_weed_hashname(j, FALSE, TRUE, 0, TRUE);
  hashnames[i][7].hash =  	lives_string_hash(hashnames[i][7].string);
}


static void load_weed_plugin(char *plugin_name, char *plugin_path, char *dir) {
#if defined TEST_ISOL && defined LM_ID_NEWLM
  static Lmid_t lmid = LM_ID_NEWLM;
  static boolean have_lmid = FALSE;
  Lmid_t new_lmid;
#endif
  weed_setup_f setup_fn;
  weed_plant_t *plugin_info = NULL, **filters = NULL, *filter = NULL;
  weed_plant_t *host_info;
  void *handle;
  int dlflags = RTLD_NOW | RTLD_LOCAL;
  int reason, idx = num_weed_filters;
  int filters_in_plugin, fnum, j;

  char cwd[PATH_MAX];

  // filters which can cause a segfault
  const char *frei0r_blacklist[] = {"Timeout indicator", NULL};
  const char *ladspa_blacklist[] = {"Mag's Notch Filter", "Identity (Control)", "Signal Branch (IC)",
                                    "Signal Product (ICIC)", "Signal Difference (ICMC)",
                                    "Signal Sum (ICIC)", "Signal Ratio (NCDC)",
                                    NULL
                                   };

  char *pwd, *tmp, *msg, *filtname = NULL;
  char *filter_name = NULL, *package_name = NULL;
  boolean blacklisted;
  boolean none_valid = TRUE;

  int i, nf = 0;

#ifdef RTLD_DEEPBIND
  dlflags |= RTLD_DEEPBIND;
#endif

  pwd = getcwd(cwd, PATH_MAX);
  THREADVAR(chdir_failed) = FALSE;

  // walk list and create fx structures
  //#define DEBUG_WEED
#ifdef DEBUG_WEED
  lives_printerr("Checking plugin %s\n", plugin_path);
#endif

#if defined TEST_ISOL && defined LM_ID_NEWLM
  if (!have_lmid) {
    handle = dlmopen(LM_ID_NEWLM, plugin_path, dlflags);
  } else {
    handle = dlmopen(lmid, plugin_path, dlflags);
  }
  if (handle) {
    dlerror(); // clear existing errors
    if (!have_lmid) {
      have_lmid = TRUE;
      dlinfo(handle, RTLD_DI_LMID, &new_lmid);
      dlerror(); // clear existing errors
      lmid = new_lmid;
    }
  }
#else
  if ((handle = dlopen(plugin_path, dlflags))) {
    dlerror(); // clear existing errors
  }
#endif
  else {
    msg = lives_strdup_printf(_("Unable to load plugin %s\nError was: %s\n"), plugin_path, dlerror());
    LIVES_WARN(msg);
    lives_free(msg);
    return;
  }
  if ((setup_fn = (weed_setup_f)dlsym(handle, "weed_setup")) == NULL) {
    msg = lives_strdup_printf(_("Error: plugin %s has no weed_setup() function.\n"), plugin_path);
    LIVES_ERROR(msg);
    lives_free(msg);
    return;
  }

  // here we call the plugin's setup_fn, passing in our bootstrap function
  // the plugin will call our bootstrap function to get the correct versions of the core weed functions
  // and bootstrap itself

  // if we use the plugin, we must not free the plugin_info, since the plugin has a reference to this

  // chdir to plugin dir, in case it needs to load data

  // set a callback so we can adjust plugin functions (provided the plugin calls weed_bootstrap as we request)
  host_info = NULL;
  our_plugin_id = 0;
  do {
    our_plugin_id = (int)((lives_random() * lives_random()) & 0xFFFF);
  } while (our_plugin_id == 0);

  weed_set_host_info_callback(host_info_cb, LIVES_INT_TO_POINTER(our_plugin_id));

  lives_chdir(dir, TRUE);
  suspect = TRUE;
  expected_hi = expected_pi = NULL;
  ncbcalls = 0;
  fxname = lives_get_filename(plugin_name);

  plugin_info = (*setup_fn)(weed_bootstrap);

  if (!plugin_info || ((nf = filters_in_plugin = check_weed_plugin_info(plugin_info)) < 1)) {
    msg = lives_strdup_printf(_("No usable filters found in plugin:\n%s\n"), plugin_path);
    LIVES_INFO(msg);
    lives_free(msg);
    if (plugin_info) weed_plant_free(plugin_info);
    dlclose(handle);
    lives_chdir(pwd, FALSE);
    lives_freep((void **)&fxname);
    return;
  }
  //g_print("Loaded plugin:\n%s\n", plugin_path);

  lives_freep((void **)&fxname);

  if (expected_pi && plugin_info != expected_pi) suspect = TRUE;
  if (weed_plant_has_leaf(plugin_info, WEED_LEAF_HOST_INFO)) {
    host_info = weed_get_plantptr_value(plugin_info, WEED_LEAF_HOST_INFO, NULL);
    if (!host_info || host_info != expected_hi) {
      // filter switched the host_info...very naughty
      if (host_info) weed_plant_free(host_info);
      msg = lives_strdup_printf("Badly behaved plugin (%s)\n", plugin_path);
      LIVES_WARN(msg);
      lives_free(msg);
      if (plugin_info) weed_plant_free(plugin_info);
      dlclose(handle);
      lives_chdir(pwd, FALSE);
      return;
    }
    if (weed_plant_has_leaf(host_info, WEED_LEAF_PLUGIN_INFO)) {
      weed_plant_t *pi = weed_get_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, NULL);
      if (!pi || pi != plugin_info) {
        // filter switched the plugin_info...very naughty
        msg = lives_strdup_printf("Badly behaved plugin - %s %p %p %p %p\n",
                                  plugin_path, pi, plugin_info, host_info, expected_hi);
        LIVES_WARN(msg);
        lives_free(msg);
        if (pi) weed_plant_free(pi);
        if (host_info) weed_plant_free(host_info);
        if (plugin_info) weed_plant_free(plugin_info);
        dlclose(handle);
        lives_chdir(pwd, FALSE);
        return;
      }
    } else {
      suspect = TRUE;
      weed_set_plantptr_value(host_info, WEED_LEAF_PLUGIN_INFO, plugin_info);
    }
  } else {
    if (suspect || !expected_hi) {
      msg = lives_strdup_printf("Badly behaved plugin: %s\n", plugin_path);
      LIVES_WARN(msg);
      lives_free(msg);
      if (host_info) weed_plant_free(host_info);
      if (plugin_info) weed_plant_free(plugin_info);
      dlclose(handle);
      lives_chdir(pwd, FALSE);
      return;
    }
    host_info = expected_hi;
    suspect = TRUE;
    weed_set_plantptr_value(plugin_info, WEED_LEAF_HOST_INFO, expected_hi);
  }
  if (!suspect) {
    weed_set_boolean_value(plugin_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_FALSE);
    weed_set_boolean_value(host_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_FALSE);
  } else {
    weed_set_boolean_value(plugin_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_TRUE);
    weed_set_boolean_value(host_info, WEED_LEAF_HOST_SUSPICIOUS, WEED_TRUE);
  }
  weed_set_voidptr_value(plugin_info, WEED_LEAF_HOST_HANDLE, handle);
  weed_set_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, plugin_name); // for hashname
  weed_set_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_PATH, dir);
  weed_add_plant_flags(plugin_info, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_");

  filters = weed_get_plantptr_array(plugin_info, WEED_LEAF_FILTERS, NULL);

  if (weed_plant_has_leaf(plugin_info, WEED_LEAF_PACKAGE_NAME)) {
    package_name = lives_strdup_printf("%s: ", (tmp = weed_get_string_value(plugin_info,
                                       WEED_LEAF_PACKAGE_NAME, NULL)));
    lives_free(tmp);
  } else package_name = lives_strdup("");

  for (fnum = 0; fnum < filters_in_plugin; fnum++) {
    filter = filters[fnum];
    blacklisted = FALSE;

    if (!filter) {
      msg = lives_strdup_printf(_("Plugin %s returned an empty filter !"), plugin_path);
      LIVES_WARN(msg);
      lives_free(msg);
      continue;
    }

    if (filtname) lives_free(filtname);
    filtname = weed_filter_get_name(filter);
    blacklisted = FALSE;

    if (!strcmp(package_name, "Frei0r: ")) {
      for (i = 0; frei0r_blacklist[i]; i++) {
        if (!strcmp(filtname, frei0r_blacklist[i])) {
          blacklisted = TRUE;
          break;
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

    if (!strcmp(package_name, "LADSPA: ")) {
      for (i = 0; ladspa_blacklist[i]; i++) {
        if (!lives_strcmp(filtname, ladspa_blacklist[i])) {
          blacklisted = TRUE;
          break;
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

    if (blacklisted) {
      if (!prefs->vj_mode) {
        msg = lives_strdup_printf(_("%sskipping blacklisted filter %s\n"), package_name, filtname);
        fprintf(stderr, "%s", msg);
        lives_free(msg);
      }
      continue;
    }

    filter_name = lives_strdup_printf("%s%s:", package_name, filtname);
    lives_free(filtname);
    filtname = NULL;

    // add value returned in host_info_cb
    if (host_info) weed_set_plantptr_value(filter, WEED_LEAF_HOST_INFO, host_info);

    if (!(reason = check_for_lives(filter, idx))) {
      boolean dup = FALSE;
      none_valid = FALSE;
      num_weed_filters++;
      weed_filters = (weed_plant_t **)lives_realloc(weed_filters, num_weed_filters * sizeof(weed_plant_t *));
      weed_filters[idx] = filter;

      hashnames = (lives_hashjoint *)lives_realloc(hashnames, num_weed_filters * sizeof(lives_hashjoint));
      gen_hashnames(idx, idx);

      for (i = 0; i < idx; i++) {
        if (hashnames[idx][1].hash == hashnames[i][1].hash
            && !lives_utf8_strcasecmp(hashnames[idx][1].string, hashnames[i][1].string)) {
          // skip dups
          if (!prefs->vj_mode) {
            msg = lives_strdup_printf(_("Found duplicate plugin %s"), hashnames[idx][1].string);
            LIVES_INFO(msg);
            lives_free(msg);
          }
          for (j = 0; j < NHASH_TYPES; j ++) {
            lives_freep((void **)&hashnames[idx][j].string);
          }
          num_weed_filters--;
          weed_filters = (weed_plant_t **)lives_realloc(weed_filters, num_weed_filters * sizeof(weed_plant_t *));
          hashnames = (lives_hashjoint *)lives_realloc(hashnames, num_weed_filters * sizeof(lives_hashjoint));
          lives_freep((void **)&filter_name);
          dup = TRUE;
          break;
        }

        if (hashnames[i][2].hash == hashnames[idx][2].hash
            && !lives_utf8_strcasecmp(hashnames[i][2].string, hashnames[idx][2].string)) {
          //g_print("partial dupe: %s and %s\n",phashnames[phashes-1],phashnames[i-oidx-1]);
          // found a partial match: author and/or version differ
          // hide oldder version from menus
          if (weed_get_int_value(filter, WEED_LEAF_VERSION, NULL) <
              weed_get_int_value(weed_filters[i], WEED_LEAF_VERSION, NULL)) {
            weed_set_boolean_value(filter, WEED_LEAF_HOST_MENU_HIDE, WEED_TRUE);
          } else {
            weed_set_boolean_value(weed_filters[i], WEED_LEAF_HOST_MENU_HIDE, WEED_TRUE);
          }
          break;
        }
      }

      if (dup) continue;
      idx++;
      if (prefs->show_splash) {
        msg = lives_strdup_printf((tmp = _("Loaded filter %s in plugin %s")), filter_name, plugin_name);
        lives_free(tmp);
        splash_msg(msg, SPLASH_LEVEL_LOAD_RTE);
        lives_free(msg);
      }
    } else {
      d_print_debug("Unsuitable filter \"%s\" in plugin \"%s\", reason code %d\n",
                    filter_name, plugin_name, reason);
    }
    lives_freep((void **)&filter_name);
  }

  if (none_valid && plugin_info) {
    host_info = weed_get_plantptr_value(plugin_info, WEED_LEAF_HOST_INFO, NULL);
    if (host_info) weed_plant_free(host_info);
    weed_plant_free(plugin_info);
  }

  lives_freep((void **)&filters);
  lives_free(package_name);
  if (THREADVAR(chdir_failed)) {
    char *dirs = (_("Some plugin directories"));
    do_chdir_failed_error(dirs);
    lives_free(dirs);
  }

  lives_chdir(pwd, FALSE);

  // TODO - add any rendered effects to fx submenu
}


static void make_fx_defs_menu(int num_weed_compounds) {
  weed_plant_t *filter;

  LiVESWidget *menuitem, *menu = mainw->rte_defs;
  LiVESWidget *pkg_menu;
  LiVESWidget *pkg_submenu = NULL;

  LiVESList *menu_list = NULL;
  LiVESList *pkg_menu_list = NULL;
  LiVESList *compound_list = NULL;

  char *string, *filter_type, *filter_name;
  char *pkg = NULL, *pkgstring = NULL;
  boolean hidden;

  weed_fx_sorted_list = NULL;

  // menu entries for vj/set defs
  for (int i = 0; i < num_weed_filters; i++) {
    filter = weed_filters[i];

    if (weed_filter_hints_unstable(filter) && !prefs->show_dev_opts && !prefs->unstable_fx) continue;
    filter_name = weed_filter_idx_get_name(i, FALSE, FALSE);

    // skip hidden filters
    if (weed_get_boolean_value(filter, WEED_LEAF_HOST_MENU_HIDE, NULL) == WEED_TRUE ||
        (num_in_params(filter, TRUE, TRUE) == 0 && !(!has_video_chans_in(filter, FALSE) && has_video_chans_out(filter, FALSE))) ||
        (enabled_in_channels(filter, TRUE) > 2 && enabled_out_channels(filter, TRUE) > 0))
      hidden = TRUE;
    else hidden = FALSE;

    if (pkgstring && pkgstring != pkg) lives_free(pkgstring);
    pkgstring = weed_filter_get_package_name(filter);

    if (pkgstring) {
      // new package
      if (pkg && strcmp(pkg, pkgstring)) {
        pkg_menu_list = add_sorted_list_to_menu(LIVES_MENU(menu), pkg_menu_list);
        lives_list_free(pkg_menu_list);
        pkg_menu_list = NULL;
        menu = mainw->rte_defs;
        lives_freep((void **)&pkg);
      }

      if (!pkg) {
        pkg = pkgstring;
        /* TRANSLATORS: example " - LADSPA plugins -" */
        pkgstring = lives_strdup_printf(_(" - %s plugins -"), pkg);
        // create new submenu

        widget_opts.mnemonic_label = FALSE;
        pkg_menu = lives_standard_menu_item_new_with_label(pkgstring);
        widget_opts.mnemonic_label = TRUE;
        lives_container_add(LIVES_CONTAINER(mainw->rte_defs), pkg_menu);

        pkg_submenu = lives_menu_new();
        lives_menu_item_set_submenu(LIVES_MENU_ITEM(pkg_menu), pkg_submenu);

        if (palette->style & STYLE_1) {
          lives_widget_set_bg_color(pkg_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
          lives_widget_set_fg_color(pkg_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
        }

        lives_widget_show(pkg_menu);
        lives_widget_show(pkg_submenu);
        lives_free(pkgstring);
        pkgstring = NULL;

        // add to submenu
        menu = pkg_submenu;
      }
    } else {
      pkg_menu_list = add_sorted_list_to_menu(LIVES_MENU(menu), pkg_menu_list);
      lives_list_free(pkg_menu_list);
      pkg_menu_list = NULL;
      lives_freep((void **)&pkg);
      menu = mainw->rte_defs;
    }
    filter_type = weed_filter_get_type(filter, TRUE, FALSE);
    string = lives_strdup_printf("%s (%s)", filter_name, filter_type);

    widget_opts.mnemonic_label = FALSE;
    menuitem = lives_standard_menu_item_new_with_label(string);
    widget_opts.mnemonic_label = TRUE;
    lives_free(string);
    lives_free(filter_type);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), HIDDEN_KEY, LIVES_INT_TO_POINTER((int)hidden));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), SECLIST_KEY, &weed_fx_sorted_list);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), SECLIST_VAL_KEY, LIVES_INT_TO_POINTER(i));

    if (pkg) pkg_menu_list = lives_list_prepend(pkg_menu_list, (livespointer)menuitem);
    else {
      if (i >= num_weed_filters - num_weed_compounds) compound_list = lives_list_prepend(compound_list, (livespointer)menuitem);
      else menu_list = lives_list_prepend(menu_list, (livespointer)menuitem);
    }

    lives_signal_sync_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                              LIVES_GUI_CALLBACK(rte_set_defs_activate), LIVES_INT_TO_POINTER(i));

    lives_freep((void **)&filter_name);
  }

  if (pkg) {
    pkg_menu_list = add_sorted_list_to_menu(LIVES_MENU(menu), pkg_menu_list);
    lives_list_free(pkg_menu_list);
    lives_free(pkg);
  }
  menu_list = add_sorted_list_to_menu(LIVES_MENU(mainw->rte_defs), menu_list);
  lives_list_free(menu_list);
  compound_list = add_sorted_list_to_menu(LIVES_MENU(mainw->rte_defs), compound_list);
  lives_list_free(compound_list);
}


static int ncompounds = 0;

void weed_load_all(void) {
  // get list of plugins from directory and create our fx
  LiVESList *weed_plugin_list, *weed_plugin_sublist;
  pthread_mutexattr_t mattr;
  char **dirs;
  char *subdir_path, *subdir_name, *plugin_path, *plugin_name, *plugin_ext;
  int max_modes = FX_MODES_MAX;
  int numdirs;
  int i, j;

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  num_weed_filters = 0;

  weed_filters = NULL;
  hashnames = NULL;

#ifdef DEBUG_WEED
  lives_printerr("In weed init\n");
#endif

  fg_gen_to_start = fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
  bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;

  for (i = 0; i < FX_KEYS_MAX; i++) {
    if (i == FX_KEYS_MAX_VIRTUAL) max_modes = 1;

    key_to_instance[i] = (weed_plant_t **)lives_calloc(max_modes, sizeof(weed_plant_t *));
    key_to_instance_copy[i] = (weed_plant_t **)lives_calloc(1, sizeof(weed_plant_t *));
    key_to_fx[i] = (int *)lives_calloc(max_modes, sizint);

    pchains[i] = NULL;

    if (i < FX_KEYS_MAX_VIRTUAL) {
      key_defaults[i] = (weed_plant_t ***)lives_calloc(max_modes, sizeof(weed_plant_t **));
      pthread_mutex_init(&mainw->fx_key_mutex[i], &mattr);
    }
    key_modes[i] = 0; // current active mode of each key
    effects_map[i] = NULL; // maps effects in order of application for multitrack rendering
    for (j = 0; j < max_modes; j++) key_to_fx[i][j] = -1;
  }

  effects_map[FX_KEYS_MAX + 1] = NULL;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    pchains[i] = NULL;
    init_events[i] = NULL;
  }

  next_free_key = FX_KEYS_MAX_VIRTUAL;

  // first we parse the weed_plugin_path
#ifndef IS_MINGW
  numdirs = get_token_count(prefs->weed_plugin_path, ':');
  dirs = lives_strsplit(prefs->weed_plugin_path, ":", numdirs);
#else
  numdirs = get_token_count(prefs->weed_plugin_path, ';');
  dirs = lives_strsplit(prefs->weed_plugin_path, ";", numdirs);
#endif

  /* here we want to load all plants and leaves into std memory, */
  /* otherwise we use up all allocated space for rpmalloc */
  /* this is safe povided no other threads are active, and we use same functions when unloading */
#if !USE_STD_MEMFUNCS
#if USE_RPMALLOC
  libweed_set_memory_funcs(default_malloc, default_free, default_calloc);
#endif
#endif
  for (i = numdirs - 1; i >= 0; i--) {
    // get list of all files
    LiVESList *list;
    if (!lives_file_test(dirs[i], LIVES_FILE_TEST_IS_DIR)) continue;
    list = weed_plugin_list = get_plugin_list(PLUGIN_EFFECTS_WEED, TRUE, dirs[i], NULL);

    // parse twice, first we get the plugins, then 1 level of subdirs
    while (list) {
      LiVESList *listnext = list->next;
      // threaded_dialog_spin(0.);
      plugin_name = (char *)list->data;
      plugin_ext = get_extension(plugin_name);
      if (!lives_strcmp(plugin_ext, DLL_EXT)) {
        plugin_path = lives_build_filename(dirs[i], plugin_name, NULL);
        d_print_debug("\nloading %s\n", plugin_name);
        load_weed_plugin(plugin_name, plugin_path, dirs[i]);
        lives_freep((void **)&plugin_name);
        lives_free(plugin_path);
        list->data = NULL;
        if (list->prev) list->prev->next = listnext;
        else weed_plugin_list = listnext;
        if (listnext) listnext->prev = list->prev;
        list->prev = list->next = NULL;
        lives_list_free(list);
      }
      lives_free(plugin_ext);
      list = listnext;
    }

    // get 1 level of subdirs
    for (list = weed_plugin_list; list; list = list->next) {
      subdir_name = (char *)list->data;
      subdir_path = lives_build_filename(dirs[i], subdir_name, NULL);
      if (!lives_file_test(subdir_path, LIVES_FILE_TEST_IS_DIR) || !strcmp(subdir_name, "icons") || !strcmp(subdir_name, "data")) {
        lives_free(subdir_path);
        subdir_path = NULL;
        continue;
      }

      for (LiVESList *list2 = weed_plugin_sublist = get_plugin_list(PLUGIN_EFFECTS_WEED, TRUE, subdir_path, DLL_EXT);
           list2; list2 = list2 ->next) {
        plugin_name = (char *)list2->data;
        plugin_path = lives_build_filename(subdir_path, plugin_name, NULL);
        load_weed_plugin(plugin_name, plugin_path, subdir_path);
        lives_free(plugin_path);
        plugin_path = NULL;
      }
      lives_list_free_all(&weed_plugin_sublist);
      lives_freep((void **)&subdir_path);
      //threaded_dialog_spin(0.);
    }
    lives_list_free_all(&weed_plugin_list);
  }

  lives_strfreev(dirs);

  //ncompounds = load_compound_fx();

#if !USE_STD_MEMFUNCS
#if USE_RPMALLOC
  libweed_set_memory_funcs(_ext_malloc, _ext_free, _ext_calloc);
#endif
#endif


  d_print(_("Successfully loaded %d Weed filters\n"), num_weed_filters);

  if (ncompounds)
    d_print(_("Successfully loaded %d compound filters\n"), ncompounds);
}


void create_fx_defs_menu(void) {
#ifndef VALGRIND_ON
  make_fx_defs_menu(ncompounds);
  weed_fx_sorted_list = lives_list_reverse(weed_fx_sorted_list);
  fx_inited = TRUE;
#endif
}


void load_rte_plugins(void) {
  /// load fx plugins, part of the startup process
  // MUST be run single threaded
  char *weed_plugin_path;
#ifdef HAVE_FREI0R
  char *frei0r_path;
#endif
#ifdef HAVE_LADSPA
  char *ladspa_path;
#endif
#ifdef HAVE_LIBVISUAL
  char *libvis_path;
#endif
  boolean needs_free = FALSE;

  get_string_pref(PREF_WEED_PLUGIN_PATH, prefs->weed_plugin_path, PATH_MAX);
  if (!*prefs->weed_plugin_path) {
    weed_plugin_path = getenv("WEED_PLUGIN_PATH");
    if (!weed_plugin_path) {
      char *ppath1 = lives_build_path(prefs->lib_dir, PLUGIN_EXEC_DIR,
                                      PLUGIN_WEED_FX_BUILTIN, NULL);
      char *ppath2 = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_LIB_DIR, PLUGIN_EXEC_DIR,
                                      PLUGIN_WEED_FX_BUILTIN, NULL);
      weed_plugin_path = lives_strdup_printf("%s:%s", ppath1, ppath2);
      lives_free(ppath1); lives_free(ppath2);
      needs_free = TRUE;
    }
    lives_snprintf(prefs->weed_plugin_path, PATH_MAX, "%s", weed_plugin_path);
    if (needs_free) lives_free(weed_plugin_path);
    set_string_pref(PREF_WEED_PLUGIN_PATH, prefs->weed_plugin_path);
  }
  lives_setenv("WEED_PLUGIN_PATH", prefs->weed_plugin_path);

#ifdef HAVE_FREI0R
  needs_free = FALSE;
  get_string_pref(PREF_FREI0R_PATH, prefs->frei0r_path, PATH_MAX);
  if (!*prefs->frei0r_path) {
    frei0r_path = getenv("FREI0R_PATH");
    if (!frei0r_path) {
      char *fp0 = lives_build_path(LIVES_USR_DIR, LIVES_LIB_DIR, FREI0R1_LITERAL, NULL);
      char *fp1 = lives_build_path(LIVES_USR_DIR, LIVES_LOCAL_DIR, LIVES_LIB_DIR, FREI0R1_LITERAL, NULL);
      char *fp2 = lives_build_path(capable->home_dir, FREI0R1_LITERAL, NULL);
      frei0r_path =
        lives_strdup_printf("%s:%s:%s", fp0, fp1, fp2);
      lives_free(fp0); lives_free(fp1); lives_free(fp2);
      needs_free = TRUE;
    }
    lives_snprintf(prefs->frei0r_path, PATH_MAX, "%s", frei0r_path);
    if (needs_free) lives_free(frei0r_path);
    set_string_pref(PREF_FREI0R_PATH, prefs->frei0r_path);
  }
  lives_setenv("FREI0R_PATH", prefs->frei0r_path);
#endif

#if HAVE_LADSPA
  needs_free = FALSE;
  get_string_pref(PREF_LADSPA_PATH, prefs->ladspa_path, PATH_MAX);
  if (!*prefs->ladspa_path) {
    ladspa_path = getenv("LADSPA_PATH");
    if (!ladspa_path) {
      ladspa_path = lives_build_path(LIVES_USR_DIR, LIVES_LIB_DIR, LADSPA_LITERAL, NULL);
      needs_free = TRUE;
    }
    lives_snprintf(prefs->ladspa_path, PATH_MAX, "%s", ladspa_path);
    if (needs_free) lives_free(ladspa_path);
    set_string_pref(PREF_LADSPA_PATH, prefs->ladspa_path);
  }
  lives_setenv("LADSPA_PATH", prefs->ladspa_path);
#endif

#if HAVE_LIBVISUAL
  needs_free = FALSE;
  get_string_pref(PREF_LIBVISUAL_PATH, prefs->libvis_path, PATH_MAX);
  if (!*prefs->libvis_path) {
    libvis_path = getenv("VISUAL_PLUGIN_PATH");
    if (!libvis_path) {
      libvis_path = "";
    }
    lives_snprintf(prefs->libvis_path, PATH_MAX, "%s", libvis_path);
    set_string_pref(PREF_LIBVISUAL_PATH, prefs->libvis_path);
  }
  lives_setenv("VISUAL_PLUGIN_PATH", prefs->libvis_path);
#endif

  weed_load_all();
}


static void weed_plant_free_if_not_in_list(weed_plant_t *plant, LiVESList **freed_ptrs) {
  /// avoid duplicate frees when unloading fx plugins
  if (freed_ptrs) {
    LiVESList *list = *freed_ptrs;
    while (list) {
      if (plant == (weed_plant_t *)list->data) return;
      list = list->next;
    }
    *freed_ptrs = lives_list_prepend(*freed_ptrs, (livespointer)plant);
  }
  weed_plant_free(plant);
}


static void weed_filter_free(weed_plant_t *filter, LiVESList **freed_ptrs) {
  int nitems, i;
  weed_plant_t **plants, *gui;
  boolean is_compound = FALSE;

  if (num_compound_fx(filter) > 1) is_compound = TRUE;

  // free in_param_templates
  plants = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nitems);
  if (nitems > 0) {
    for (i = 0; i < nitems; i++) {
      if (weed_plant_has_leaf(plants[i], WEED_LEAF_GUI)) {
        gui = (weed_get_plantptr_value(plants[i], WEED_LEAF_GUI, NULL));
        weed_plant_free_if_not_in_list(gui, freed_ptrs);
      }
      weed_plant_free_if_not_in_list(plants[i], freed_ptrs);
    }
    lives_freep((void **)&plants);
  }

  if (is_compound) {
    weed_plant_free(filter);
    return;
  }

  // free in_channel_templates
  if (weed_plant_has_leaf(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES)) {
    plants = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &nitems);
    if (nitems > 0) {
      for (i = 0; i < nitems; i++) weed_plant_free_if_not_in_list(plants[i], freed_ptrs);
      lives_freep((void **)&plants);
    }
  }

  // free out_channel_templates
  if (weed_plant_has_leaf(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
    plants = weed_filter_get_out_chantmpls(filter, &nitems);
    if (nitems > 0) {
      for (i = 0; i < nitems; i++) weed_plant_free_if_not_in_list(plants[i], freed_ptrs);
      lives_freep((void **)&plants);
    }
  }

  // free out_param_templates
  plants = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &nitems);
  if (nitems > 0) {
    //   threaded_dialog_spin(0.);
    for (i = 0; i < nitems; i++) {
      if (weed_plant_has_leaf(plants[i], WEED_LEAF_GUI))
        weed_plant_free(weed_get_plantptr_value(plants[i], WEED_LEAF_GUI, NULL));
      weed_plant_free_if_not_in_list(plants[i], freed_ptrs);
    }
    lives_freep((void **)&plants);
  }

  // free gui
  if (weed_plant_has_leaf(filter, WEED_LEAF_GUI))
    weed_plant_free_if_not_in_list(weed_get_plantptr_value(filter, WEED_LEAF_GUI, NULL), freed_ptrs);

  // free filter
  weed_plant_free(filter);
}


#ifndef VALGRIND_ON
static weed_plant_t *create_compound_filter(char *plugin_name, int nfilts, int *filts) {
  weed_plant_t *filter = weed_plant_new(WEED_PLANT_FILTER_CLASS), *xfilter, *gui;
  weed_plant_t **in_params = NULL, **out_params = NULL, **params;
  weed_plant_t **in_chans, **out_chans;

  char *tmp;

  double tgfps = -1., tfps;

  int count, xcount, error, nvals;
  int txparam = -1, tparam, txvolm = -1, tvolm;

  int i, j, x;

  weed_set_int_array(filter, WEED_LEAF_HOST_FILTER_LIST, nfilts, filts);

  // create parameter templates - concatenate all sub filters
  count = xcount = 0;

  for (i = 0; i < nfilts; i++) {
    xfilter = weed_filters[filts[i]];

    if (weed_plant_has_leaf(xfilter, WEED_LEAF_PREFERRED_FPS)) {
      tfps = weed_get_double_value(xfilter, WEED_LEAF_PREFERRED_FPS, &error);
      if (tgfps == -1.) tgfps = tfps;
      else if (tgfps != tfps) {
        if (!prefs->vj_mode) {
          d_print((tmp = lives_strdup_printf(_("Invalid compound effect %s - has conflicting target_fps\n"), plugin_name)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
        }
        return NULL;
      }
    }

    // in_parameter templates are copy-by-value, since we may set a different default/host_default value, and a different gui

    // everything else - filter classes, out_param templates and channels are copy by ref.

    if (weed_plant_has_leaf(xfilter, WEED_LEAF_IN_PARAMETER_TEMPLATES)) {
      tparam = get_transition_param(xfilter, FALSE);

      // TODO *** - ignore transition params if they are connected to themselves

      if (tparam != -1) {
        if (txparam != -1) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid compound effect %s - has multiple transition parameters\n"),
                                               plugin_name)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          return NULL;
        }
        txparam = tparam;
      }

      tvolm = get_master_vol_param(xfilter, FALSE);

      // TODO *** - ignore master vol params if they are connected to themselves

      if (tvolm != -1) {
        if (txvolm != -1) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid compound effect %s - has multiple master volume parameters\n"),
                                               plugin_name)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          return NULL;
        }
        txvolm = tvolm;
      }

      params = weed_get_plantptr_array_counted(xfilter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nvals);
      count += nvals;

      in_params = (weed_plant_t **)lives_realloc(in_params, count * sizeof(weed_plant_t *));
      x = 0;

      for (j = xcount; j < count; j++) {
        in_params[j] = lives_plant_copy(params[x]);
        gui = weed_get_plantptr_value(params[x], WEED_LEAF_GUI, &error);
        if (gui) weed_set_plantptr_value(in_params[j], WEED_LEAF_GUI, lives_plant_copy(gui));

        if (x == tparam) {
          weed_set_boolean_value(in_params[j], WEED_LEAF_IS_TRANSITION, WEED_TRUE);
        } else if (weed_plant_has_leaf(in_params[j], WEED_LEAF_IS_TRANSITION))
          weed_leaf_delete(in_params[j], WEED_LEAF_IS_TRANSITION);

        if (x == tvolm)
          weed_set_boolean_value(in_params[j], WEED_LEAF_IS_VOLUME_MASTER, WEED_TRUE);
        else if (weed_plant_has_leaf(in_params[j], WEED_LEAF_IS_VOLUME_MASTER))
          weed_leaf_delete(in_params[j], WEED_LEAF_IS_VOLUME_MASTER);

        x++;
      }
      lives_free(params);
      xcount = count;
    }
  }

  if (tgfps != -1.) weed_set_double_value(filter, WEED_LEAF_PREFERRED_FPS, tgfps);

  if (count > 0) {
    weed_set_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, count, in_params);
    lives_free(in_params);
  }

  count = xcount = 0;

  for (i = 0; i < nfilts; i++) {
    xfilter = weed_filters[filts[i]];
    if (weed_plant_has_leaf(xfilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) {
      params = weed_get_plantptr_array_counted(xfilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &nvals);
      count += nvals;

      out_params = (weed_plant_t **)lives_realloc(out_params, count * sizeof(weed_plant_t *));
      x = 0;

      for (j = xcount; j < count; j++) {
        out_params[j] = params[x++];
      }
      if (params) lives_free(params);
      xcount = count;
    }
  }

  if (count > 0) {
    weed_set_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, count, out_params);
    lives_free(out_params);
  }

  // use in channels from first filter

  xfilter = weed_filters[filts[0]];
  if (weed_plant_has_leaf(xfilter, WEED_LEAF_IN_CHANNEL_TEMPLATES)) {
    in_chans = weed_filter_get_in_chantmpls(xfilter, &count);
    weed_set_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, count, in_chans);
    lives_free(in_chans);
  }

  // use out channels from last filter

  xfilter = weed_filters[filts[nfilts - 1]];
  if (weed_plant_has_leaf(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
    out_chans = weed_filter_get_out_chantmpls(filter, &count);
    weed_set_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, count, out_chans);
    lives_free(out_chans);
  }

  return filter;
}
#endif

#ifndef VALGRIND_ON
static void load_compound_plugin(char *plugin_name, char *plugin_path) {
  FILE *cpdfile;

  weed_plant_t *filter = NULL, *ptmpl, *iptmpl, *xfilter;
  char buff[16384];
  char **array, **svals;
  char *author = NULL, *key;
  int *filters = NULL, *ivals;
  double *dvals;
  int xvals[4];

  boolean ok = TRUE, autoscale;
  int stage = 0, nfilts = 0, fnum, line = 0, version = 0;
  int qvals = 1, ntok, xfilt, xfilt2;
  int xconx = 0;
  int nparams, nchans, pnum, pnum2, xpnum2, cnum, cnum2, ptype, pptype, pcspace, pflags;
  int error;

  int i;

  if ((cpdfile = fopen(plugin_path, "r"))) {
    while (fgets(buff, 16384, cpdfile)) {
      line++;
      lives_strstrip(buff);
      if (!(*buff)) {
        if (++stage == 5) break;
        if (stage == 2) {
          if (nfilts < 2) {
            if (!prefs->vj_mode) {
              d_print_debug(_("Invalid compound effect %s - must have >1 sub filters\n"),
                            plugin_name);
            }
            ok = FALSE;
            break;
          }
          filter = create_compound_filter(plugin_name, nfilts, filters);
          if (!filter) break;
        }
        continue;
      }
      switch (stage) {
      case 0:
        if (line == 1) author = lives_strdup(buff);
        if (line == 2) version = atoi(buff);
        break;
      case 1:
        // add filters
        fnum = weed_get_idx_for_hashname(buff, TRUE);
        if (fnum == -1) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid effect %s found in compound effect %s, line %d\n"),
                          buff, plugin_name, line);
          }
          ok = FALSE;
          break;
        }
        filters = (int *)lives_realloc(filters, ++nfilts * sizeof(int));
        filters[nfilts - 1] = fnum;
        break;
      case 2:
        // override defaults: format is i|p|d0|d1|...|dn
        // NOTE: for obvious reasons, | may not be used inside string defaults
        ntok = get_token_count(buff, '|');

        if (ntok < 2) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid default found in compound effect %s, line %d\n"),
                          plugin_name, line);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < 0 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid filter %d for defaults found in compound effect %s, line %d\n"),
                          xfilt, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter = get_weed_filter(filters[xfilt]);

        nparams = num_in_params(xfilter, FALSE, FALSE);

        pnum = atoi(array[1]);

        if (pnum >= nparams) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid param %d for defaults found in compound effect %s, line %d\n"),
                          pnum, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        // get pnum for compound filter
        for (i = 0; i < xfilt; i++) pnum += num_in_params(get_weed_filter(filters[i]), FALSE, FALSE);

        ptmpl = weed_filter_in_paramtmpl(filter, pnum, FALSE);

        ptype = weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT);
        pflags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, &error);
        pptype = weed_paramtmpl_get_type(ptmpl);

        if (pptype == WEED_PARAM_COLOR) {
          pcspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, &error);
          qvals = 3;
          if (pcspace == WEED_COLORSPACE_RGBA) qvals = 4;
        }

        ntok -= 2;

        if ((ntok != weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT) && !(pflags & WEED_PARAMETER_VARIABLE_SIZE)) ||
            ntok % qvals != 0) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid number of values for defaults found in compound effect %s, "
                            "line %d\n"), plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        // TODO - for INT and DOUBLE, check if default is within min/max bounds
        if (!ntok) break;

        switch (ptype) {
        case WEED_SEED_INT:
          ivals = (int *)lives_malloc(ntok * sizint);
          for (i = 0; i < ntok; i++) {
            ivals[i] = atoi(array[i + 2]);
          }
          weed_set_int_array(ptmpl, WEED_LEAF_DEFAULT, ntok, ivals);
          lives_free(ivals);
          break;
        case WEED_SEED_DOUBLE:
          dvals = (double *)lives_malloc(ntok * sizdbl);
          for (i = 0; i < ntok; i++) {
            dvals[i] = lives_strtod(array[i + 2]);
          }
          weed_set_double_array(ptmpl, WEED_LEAF_DEFAULT, ntok, dvals);
          lives_free(dvals);
          break;
        case WEED_SEED_BOOLEAN:
          ivals = (int *)lives_malloc(ntok * sizint);
          for (i = 0; i < ntok; i++) {
            ivals[i] = atoi(array[i + 2]);

            if (ivals[i] != WEED_TRUE && ivals[i] != WEED_FALSE) {
              lives_free(ivals);
              if (!prefs->vj_mode) {
                d_print_debug(_("Invalid non-boolean value for defaults found in compound effect %s, "
                                "line %d\n"), pnum, plugin_name, line);
              }
              ok = FALSE;
              lives_strfreev(array);
              break;
            }
          }
          weed_set_boolean_array(ptmpl, WEED_LEAF_DEFAULT, ntok, ivals);
          lives_free(ivals);
          break;
        default: // string
          svals = (char **)lives_malloc(ntok * sizeof(char *));
          for (i = 0; i < ntok; i++) {
            svals[i] = lives_strdup(array[i + 2]);
          }
          weed_set_string_array(ptmpl, WEED_LEAF_DEFAULT, ntok, svals);
          for (i = 0; i < ntok; i++) {
            lives_free(svals[i]);
          }
          lives_free(svals);
          break;
        }
        lives_strfreev(array);
        break;
      case 3:
        // link params: format is f0|p0|autoscale|f1|p1

        ntok = get_token_count(buff, '|');

        if (ntok != 5) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid param link found in compound effect %s, line %d\n"),
                          plugin_name, line);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < -1 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid out filter %d for link params found in compound effect %s, line %d\n"),
                          xfilt, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        pnum = atoi(array[1]);

        if (xfilt > -1) {
          xfilter = get_weed_filter(filters[xfilt]);

          if (weed_plant_has_leaf(xfilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES))
            nparams = weed_leaf_num_elements(xfilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
          else nparams = 0;

          if (pnum >= nparams) {
            if (!prefs->vj_mode) {
              d_print_debug(_("Invalid out param %d for link params found in compound effect %s, line %d\n"),
                            pnum, plugin_name, line);
            }
            ok = FALSE;
            lives_strfreev(array);
            break;
          }
        }

        autoscale = atoi(array[2]);

        if (autoscale != WEED_TRUE && autoscale != WEED_FALSE) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid non-boolean value for autoscale found in compound effect %s, "
                            "line %d\n"), pnum, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2 = atoi(array[3]); // sub filter number
        if (xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid in filter %d for link params found in compound effect %s, line %d\n"),
                          xfilt2, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter = get_weed_filter(filters[xfilt2]);

        nparams = num_in_params(xfilter, FALSE, FALSE);

        pnum2 = atoi(array[4]);

        if (pnum2 >= nparams) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid in param %d for link params found in compound effect %s, line %d\n"),
                          pnum2, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        // calc xpnum2
        xpnum2 = pnum2;
        for (i = 0; i < xfilt2; i++) xpnum2 += num_in_params(get_weed_filter(filters[i]), FALSE, FALSE);

        // must get paramtmpl here from filter (not xfilter)
        iptmpl = weed_filter_in_paramtmpl(filter, xpnum2, FALSE);

        xvals[0] = xfilt;
        xvals[1] = pnum;

        weed_set_int_array(iptmpl, WEED_LEAF_HOST_INTERNAL_CONNECTION, 2, xvals);
        if (autoscale == WEED_TRUE) weed_set_boolean_value(iptmpl,
              WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE, WEED_TRUE);

        lives_strfreev(array);
        break;
      case 4:
        // link alpha channels: format is f0|c0|f1|c1
        ntok = get_token_count(buff, '|');

        if (ntok != 4) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid channel link found in compound effect %s, line %d\n"),
                          plugin_name, line);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < 0 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid out filter %d for link channels found in compound effect %s, "
                            "line %d\n"), xfilt, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter = get_weed_filter(filters[xfilt]);

        nchans = 0;
        if (weed_plant_has_leaf(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
          nchans = weed_leaf_num_elements(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES);
          if (!weed_get_plantptr_value(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &error)) nchans = 0;
        }

        cnum = atoi(array[1]);

        if (cnum >= nchans) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid out channel %d for link params found in compound effect %s, line %d\n"),
                          cnum, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2 = atoi(array[2]); // sub filter number
        if (xfilt2 <= xfilt || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid in filter %d for link channels found in compound effect %s, line %d\n"),
                          xfilt2, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter = get_weed_filter(filters[xfilt2]);

        nchans = 0;
        if (weed_plant_has_leaf(xfilter, WEED_LEAF_IN_CHANNEL_TEMPLATES)) {
          nchans = weed_leaf_num_elements(xfilter, WEED_LEAF_IN_CHANNEL_TEMPLATES);
          if (!weed_get_plantptr_value(xfilter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &error)) nchans = 0;
        }

        cnum2 = atoi(array[3]);

        if (cnum2 >= nchans) {
          if (!prefs->vj_mode) {
            d_print_debug(_("Invalid in channel %d for link params found in compound effect %s, line %d\n"),
                          cnum2, plugin_name, line);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        xvals[0] = xfilt;
        xvals[1] = cnum;
        xvals[2] = xfilt2;
        xvals[3] = cnum2;

        // unlike with in_param_templates, which are copy by value, in_channel_templates are copy by ref.
        // so we need to add some extra leaves to the compound filter to note channel connections
        // - we will link the actual channels when we create an instance from the filter
        key = lives_strdup_printf(WEED_LEAF_HOST_CHANNEL_CONNECTION "%d", xconx++);
        weed_set_int_array(filter, key, 4, xvals);
        lives_free(key);

        lives_strfreev(array);
        break;
      default:
        break;
      }
      if (!ok) {
        if (filter) weed_filter_free(filter, NULL);
        filter = NULL;
        break;
      }
    }
    fclose(cpdfile);
  }

  if (filter) {
    int idx;
    char *filter_name = lives_strdup_printf(_("Compound:%s"), plugin_name);
    weed_set_string_value(filter, WEED_LEAF_NAME, filter_name);

    weed_set_string_value(filter, WEED_LEAF_AUTHOR, author);
    weed_set_int_value(filter, WEED_LEAF_VERSION, version);
    idx = num_weed_filters++;

    weed_filters = (weed_plant_t **)lives_realloc(weed_filters, num_weed_filters * sizeof(weed_plant_t *));
    weed_filters[idx] = filter;
    hashnames = (lives_hashjoint *)lives_realloc(hashnames, num_weed_filters * sizeof(lives_hashjoint));
    gen_hashnames(idx, idx);
    lives_free(filter_name);
  }

  if (author) lives_free(author);
  if (filters) lives_free(filters);
}
#endif

#ifndef VALGRIND_ON
static int load_compound_fx(void) {
  LiVESList *compound_plugin_list;

  int onum_filters = num_weed_filters;

  char *lives_compound_plugin_path, *plugin_name, *plugin_path;

  lives_compound_plugin_path = lives_build_path(prefs->config_datadir, PLUGIN_COMPOUND_EFFECTS_CUSTOM, NULL);
  compound_plugin_list = get_plugin_list(PLUGIN_COMPOUND_EFFECTS_CUSTOM, TRUE, lives_compound_plugin_path, NULL);

  for (LiVESList *list = compound_plugin_list; list; list = list->next) {
    plugin_name = (char *)list->data;
    plugin_path = lives_build_path(lives_compound_plugin_path, plugin_name, NULL);
    load_compound_plugin(plugin_name, plugin_path);
    lives_free(plugin_path);
  }

  lives_list_free_all(&compound_plugin_list);

  lives_free(lives_compound_plugin_path);

  lives_compound_plugin_path = lives_build_path(prefs->prefix_dir, PLUGIN_COMPOUND_DIR,
                               PLUGIN_COMPOUND_EFFECTS_BUILTIN, NULL);


  compound_plugin_list = get_plugin_list(PLUGIN_COMPOUND_EFFECTS_BUILTIN, TRUE, lives_compound_plugin_path, NULL);

  for (LiVESList *list = compound_plugin_list; list; list = list->next) {
    plugin_name = (char *)list->data;
    plugin_path = lives_build_path(lives_compound_plugin_path, plugin_name, NULL);
    load_compound_plugin(plugin_name, plugin_path);
    lives_free(plugin_path);
  }

  lives_list_free_all(&compound_plugin_list);

  lives_free(lives_compound_plugin_path);
  return num_weed_filters - onum_filters;
}
#endif

void weed_unload_all(void) {
  weed_plant_t *filter, *plugin_info, *host_info;
  LiVESList *pinfo = NULL, *list, *freed_plants = NULL, *hostinfo_ptrs = NULL;

  int error;

  int i, j;

  if (!fx_inited) return;

  mainw->num_tr_applied = 0;
  weed_deinit_all(TRUE);

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    for (j = 0; j < FX_MODES_MAX; j++) {
      if (key_defaults[i][j]) free_key_defaults(i, j);
    }
    lives_free(key_defaults[i]);
  }

  THREADVAR(chdir_failed) = FALSE;

  for (i = 0; i < num_weed_filters; i++) {
    filter = weed_filters[i];
    plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, &error);
    if (!plugin_info) {
      weed_filter_free(filter, &freed_plants);
      lives_list_free(freed_plants);
      freed_plants = NULL;
      continue;
    }
    freed_plants = weed_get_voidptr_value(plugin_info, WEED_LEAF_FREED_PLANTS, NULL);
    weed_filter_free(filter, &freed_plants);
    if (!pinfo || lives_list_index(pinfo, plugin_info) == -1) {
      pinfo = lives_list_append(pinfo, plugin_info);
      weed_set_voidptr_value(plugin_info, WEED_LEAF_FREED_PLANTS, (void *)freed_plants);
    }
    freed_plants = NULL;
  }

  list = pinfo;

  while (pinfo) {
    // We need to free ALL the filters for a plugin before calling desetup and dlclose.
    // Otherwise we will end doing this on an unloaded plugin !
    plugin_info = (weed_plant_t *)pinfo->data;
    if (plugin_info) {
      void *handle = weed_get_voidptr_value(plugin_info, WEED_LEAF_HOST_HANDLE, &error);
      if (handle && prefs->startup_phase == 0) {
        weed_desetup_f desetup_fn;
        if ((desetup_fn = (weed_desetup_f)dlsym(handle, "weed_desetup")) != NULL) {
          char *cwd = cd_to_plugin_dir(plugin_info);
          // call weed_desetup()
          (*desetup_fn)();
          lives_chdir(cwd, FALSE);
          lives_free(cwd);
        }
        dlclose(handle);
        //threaded_dialog_spin(0.);
      }
      freed_plants = weed_get_voidptr_value(plugin_info, WEED_LEAF_FREED_PLANTS, NULL);
      if (freed_plants) {
        lives_list_free(freed_plants);
        freed_plants = NULL;
      }
      host_info = weed_get_plantptr_value((weed_plant_t *)pinfo->data, WEED_LEAF_HOST_INFO, &error);
      weed_plant_free_if_not_in_list(host_info, &hostinfo_ptrs);
      weed_plant_free(plugin_info);
    }
    pinfo = pinfo->next;
  }

  if (list) lives_list_free(list);
  if (hostinfo_ptrs) lives_list_free(hostinfo_ptrs);

  for (i = 0; i < num_weed_filters; i++) {
    for (j = 0; j < NHASH_TYPES; j ++) {
      if (hashnames[i][j].string) lives_free(hashnames[i][j].string);
    }
  }

  if (hashnames) lives_free(hashnames);
  if (weed_filters) lives_free(weed_filters);

  lives_list_free(weed_fx_sorted_list);

  for (i = 0; i < FX_KEYS_MAX; i++) {
    lives_free(key_to_fx[i]);
    lives_free(key_to_instance[i]);
    lives_free(key_to_instance_copy[i]);
  }

  //  threaded_dialog_spin(0.);

  if (THREADVAR(chdir_failed)) {
    char *dirs = (_("Some plugin directories"));
    do_chdir_failed_error(dirs);
    lives_free(dirs);
  }
}


static void weed_in_channels_free(weed_plant_t *inst) {
  int num_channels;
  weed_plant_t **channels = weed_instance_get_in_channels(inst, &num_channels);
  if (num_channels > 0) {
    for (int i = 0; i < num_channels; i++) {
      if (channels[i]) {
        if (weed_channel_is_alpha(channels[i]))
          weed_layer_unref((weed_layer_t *)channels[i]);
        else weed_plant_free(channels[i]);
      }
    }
  }
  lives_free(channels);
}


static void weed_out_channels_free(weed_plant_t *inst) {
  int num_channels;
  weed_plant_t **channels = weed_instance_get_out_channels(inst, &num_channels);
  if (num_channels > 0) {
    for (int i = 0; i < num_channels; i++) {
      if (channels[i]) {
        if (weed_channel_is_alpha(channels[i]))
          weed_layer_unref((weed_layer_t *)channels[i]);
        else weed_plant_free(channels[i]);
      }
    }
  }
  lives_free(channels);
}


static void weed_channels_free(weed_plant_t *inst) {
  weed_in_channels_free(inst);
  weed_out_channels_free(inst);
}


static void weed_gui_free(weed_plant_t *plant) {
  weed_plant_t *gui = weed_get_plantptr_value(plant, WEED_LEAF_GUI, NULL);
  if (gui) weed_plant_free(gui);
}


void weed_in_params_free(weed_plant_t **parameters, int num_parameters) {
  for (int i = 0; i < num_parameters; i++) {
    if (parameters[i]) {
      if (parameters[i] == mainw->rte_textparm) mainw->rte_textparm = NULL;
      weed_gui_free(parameters[i]);
      weed_plant_free(parameters[i]);
    }
  }
}


void weed_in_parameters_free(weed_plant_t *inst) {
  int num_parameters;
  weed_plant_t **parameters = weed_instance_get_in_params(inst, &num_parameters);
  if (num_parameters > 0) {
    weed_in_params_free(parameters, num_parameters);
    weed_leaf_delete(inst, WEED_LEAF_IN_PARAMETERS);
  }
  lives_free(parameters);
}


static void weed_out_parameters_free(weed_plant_t *inst) {
  int num_parameters;
  weed_plant_t **parameters = weed_instance_get_out_params(inst, &num_parameters);
  if (num_parameters > 0) {
    for (int i = 0; i < num_parameters; i++) {
      if (parameters[i]) weed_plant_free(parameters[i]);
    }
    weed_leaf_delete(inst, WEED_LEAF_OUT_PARAMETERS);
  }
  lives_free(parameters);
}


static void weed_parameters_free(weed_plant_t *inst) {
  weed_in_parameters_free(inst);
  weed_out_parameters_free(inst);
}


static void lives_free_instance(weed_plant_t *inst) {
  weed_channels_free(inst);
  weed_parameters_free(inst);
  weed_plant_free(inst);
}

static int ninstref = 0;

int check_ninstrefs(void) {
  return ninstref;
}


LIVES_GLOBAL_INLINE int _weed_instance_unref(weed_plant_t *inst) {
  // return new refcount
  // return value of 0 indicates the instance was freed
  // -1 if the inst was NULL
  int nrefs = weed_refcount_dec(inst);
  if (!nrefs) {
#ifdef DEBUG_REFCOUNT
    g_print("FREE %p\n", inst);
#endif
    lives_free_instance(inst);
  }
  return nrefs;
}


int _weed_instance_ref(weed_plant_t *inst) {
  // return refcount after the operation
  // or 0 if the inst was NULL
  return weed_refcount_inc(inst);
}


weed_plant_t *_weed_instance_obtain(int line, char *file, int key, int mode) {
  // does not create the instance, but adds an extra ref to an existing one
  // caller MUST call weed_instance_unref() when instance is no longer needed
  weed_plant_t *instance;

  if (key < 0 || key >= FX_KEYS_MAX || mode < 0 || mode > rte_key_getmaxmode(key + 1)) return NULL;
  _weed_instance_ref((instance = key_to_instance[key][mode]));
#ifdef DEBUG_REFCOUNT
  if (instance) g_print("wio %p at line %d in file %s\n", instance, line, file);
#endif
  return instance;
}


#ifndef DEBUG_REFCOUNT
LIVES_GLOBAL_INLINE int weed_instance_ref(weed_plant_t *inst) {
  return _weed_instance_ref(inst);
}

LIVES_GLOBAL_INLINE int weed_instance_unref(weed_plant_t *inst) {
  return _weed_instance_unref(inst);
}

LIVES_GLOBAL_INLINE weed_plant_t *weed_instance_obtain(int key, int mode) {
  return _weed_instance_obtain(0, NULL, key, mode);
}
#endif

/**
   @brief create channels for an instance, using the channel templates and filter class as a guide

   for repeating channels, the selected number should already have been set in WEED_LEAF_HOST_REPEATS
   for the template
*/
static weed_plant_t **weed_channels_create(weed_plant_t *filter, boolean in) {
  weed_plant_t **channels, **chantmpls;
  int num_channels;
  int i, j;
  int ccount = 0;
  int num_repeats;
  int flags;
  boolean pvary = FALSE;

  if (in) chantmpls = weed_filter_get_in_chantmpls(filter, &num_channels);
  else chantmpls = weed_filter_get_out_chantmpls(filter, &num_channels);

  for (i = 0; i < num_channels; i++) {
    if (weed_plant_has_leaf(chantmpls[i], WEED_LEAF_HOST_REPEATS))
      num_repeats = weed_get_int_value(chantmpls[i], WEED_LEAF_HOST_REPEATS, NULL);
    else num_repeats = 1;
    ccount += num_repeats;
  }

  channels = (weed_plant_t **)lives_calloc((ccount + 1), sizeof(weed_plant_t *));

  flags = weed_filter_get_flags(filter);
  if (flags & WEED_FILTER_PALETTES_MAY_VARY) {
    pvary = TRUE;
  }

  ccount = 0;

  for (i = 0; i < num_channels; i++) {
    if (weed_plant_has_leaf(chantmpls[i], WEED_LEAF_HOST_REPEATS))
      num_repeats = weed_get_int_value(chantmpls[i], WEED_LEAF_HOST_REPEATS, NULL);
    else num_repeats = 1;
    for (j = 0; j < num_repeats; j++) {
      /// set some reasonable defaults for when we call init_func()
      channels[ccount] = weed_plant_new(WEED_PLANT_CHANNEL);
      weed_set_plantptr_value(channels[ccount], WEED_LEAF_TEMPLATE, chantmpls[i]);
      if (weed_chantmpl_is_audio(chantmpls[i]) == WEED_FALSE) {
        int rah = RS_ALIGN_DEF, nplanes, n, pal;
        int rs[4];
        int width = DEF_GEN_WIDTH;
        int height = DEF_GEN_HEIGHT;
        weed_set_voidptr_value(channels[ccount], WEED_LEAF_PIXEL_DATA, NULL);

        set_channel_size(filter, channels[ccount],  &width, &height);
        pal = WEED_PALETTE_END;
        if (pvary) pal = weed_get_int_value(chantmpls[i], WEED_LEAF_PALETTE_LIST, NULL);
        if (pal == WEED_PALETTE_NONE) pal = weed_get_int_value(filter, WEED_LEAF_PALETTE_LIST, NULL);
        weed_channel_set_palette(channels[ccount], pal);
        if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
          rah = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
        }
        nplanes = weed_palette_get_nplanes(pal);
        width *= (weed_palette_get_bits_per_macropixel(pal) >> 3);
        for (n = 0; n < weed_palette_get_nplanes(pal); n++) {
          rs[n] = ALIGN_CEIL(width * weed_palette_get_plane_ratio_horizontal(pal, n), rah);
        }
        weed_set_int_array(channels[ccount], WEED_LEAF_ROWSTRIDES, nplanes, rs);
      }
      // if we flagged chantmpl as host_disabled, we set disabled in the channel
      // - this is different to HOST_TEMP_DISABLED in channels which is used to switch on and off repeated opt chans.
      // here this is more of a strucutaral thing
      weed_set_boolean_value(channels[ccount], WEED_LEAF_DISABLED, weed_chantmpl_is_disabled((channels[i])));
      weed_add_plant_flags(channels[ccount], WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE, "plugin_");
      ccount++;
    }
  }
  channels[ccount] = NULL;
  lives_free(chantmpls);
  return channels;
}


weed_plant_t **weed_params_create(weed_plant_t *filter, boolean in) {
  // return set of parameters with default/host_default values
  // in==TRUE create in parameters for filter
  // in==FALSE create out parameters for filter

  weed_plant_t **params, **paramtmpls;
  int num_params;

  if (in) paramtmpls = weed_filter_get_in_paramtmpls(filter, &num_params);
  else paramtmpls = weed_filter_get_out_paramtmpls(filter, &num_params);
  if (num_params == 0) return NULL;

  params = (weed_plant_t **)lives_malloc((num_params + 1) * sizeof(weed_plant_t *));

  for (int i = 0; i < num_params; i++) {
    params[i] = weed_plant_new(WEED_PLANT_PARAMETER);
    weed_set_plantptr_value(params[i], WEED_LEAF_TEMPLATE, paramtmpls[i]);
    if (in && !weed_paramtmpl_value_irrelevant(paramtmpls[i])) {
      if (weed_plant_has_leaf(paramtmpls[i], WEED_LEAF_HOST_DEFAULT)) {
        lives_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_HOST_DEFAULT);
      } else lives_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_DEFAULT);
      weed_add_plant_flags(params[i], WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE, "plugin_");
    } else {
      lives_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_DEFAULT);
    }
  }
  params[num_params] = NULL;
  lives_free(paramtmpls);

  return params;
}


static void set_default_channel_sizes(weed_plant_t *filter, weed_plant_t **in_channels, weed_plant_t **out_channels) {
  // set some reasonable default channel sizes when we first init the effect
  weed_plant_t *channel, *chantmpl;
  boolean has_aud_in_chans = FALSE;
  int width, height;

  int i;

  // ignore filters with no in/out channels (e.g. data processors)
  if ((!in_channels || !in_channels[0]) && (!out_channels || !out_channels[0])) return;

  for (i = 0; in_channels && in_channels[i] && !weed_channel_is_disabled(in_channels[i]); i++) {
    channel = in_channels[i];
    chantmpl = weed_channel_get_template(channel);
    if (weed_chantmpl_is_audio(chantmpl) == WEED_FALSE) {
      int width = DEF_GEN_WIDTH;
      int height = DEF_GEN_HEIGHT;
      set_channel_size(filter, channel, &width, &height);
    } else {
      if (mainw->current_file == -1) {
        weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, DEFAULT_AUDIO_CHANS);
        weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, DEFAULT_AUDIO_RATE);
      } else {
        weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, cfile->achans);
        weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, cfile->arate);
      }
      weed_set_int_value(channel, WEED_LEAF_AUDIO_DATA_LENGTH, 0);
      weed_set_voidptr_value(channel, WEED_LEAF_AUDIO_DATA, NULL);
      has_aud_in_chans = TRUE;
    }
  }

  for (i = 0; out_channels && out_channels[i]; i++) {
    channel = out_channels[i];
    chantmpl = weed_channel_get_template(channel);
    if (weed_chantmpl_is_audio(chantmpl) == WEED_FALSE) {
      width = DEF_FRAME_HSIZE_UNSCALED;
      height = DEF_FRAME_VSIZE_UNSCALED;
      set_channel_size(filter, channel, &width, &height);
    } else {
      if (mainw->current_file == -1 || !has_aud_in_chans) {
        weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, DEFAULT_AUDIO_CHANS);
        weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, DEFAULT_AUDIO_RATE);
      } else {
        weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, cfile->achans);
        weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, cfile->arate);
      }
      weed_set_int_value(channel, WEED_LEAF_AUDIO_DATA_LENGTH, 0);
      weed_set_voidptr_value(channel, WEED_LEAF_AUDIO_DATA, NULL);
    }
  }
}


static weed_plant_t *weed_create_instance(weed_plant_t *filter, weed_plant_t **inc, weed_plant_t **outc,
    weed_plant_t **inp, weed_plant_t **outp) {
  // here we create a new filter_instance from the ingredients:
  // filter_class, in_channels, out_channels, in_parameters, out_parameters
  // inst is returned with a refcount of 2
  weed_plant_t *inst = weed_plant_new(WEED_PLANT_FILTER_INSTANCE);

  int flags = WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, n;

  weed_set_plantptr_value(inst, WEED_LEAF_FILTER_CLASS, filter);
  if (inc) weed_set_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, weed_flagset_array_count(inc, TRUE), inc);
  if (outc) weed_set_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, weed_flagset_array_count(outc, TRUE), outc);
  if (inp) weed_set_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, weed_flagset_array_count(inp, TRUE), inp);
  if (outp) {
    weed_set_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, (n = weed_flagset_array_count(outp, TRUE)), outp);
    for (int i = 0; i < n; i++) {
      // allow plugins to set out_param WEED_LEAF_VALUE
      weed_leaf_set_flags(outp[i], WEED_LEAF_VALUE, (weed_leaf_get_flags(outp[i], WEED_LEAF_VALUE) | flags) ^ flags);
    }
  }

  weed_set_boolean_value(inst, WEED_LEAF_HOST_INITED, WEED_FALSE);
  weed_add_plant_flags(inst, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, "plugin_");
  weed_instance_ref(inst);
  return inst;
}


void add_param_connections(weed_plant_t *inst) {
  weed_plant_t **in_ptmpls, **outp;
  weed_plant_t *iparam, *oparam, *first_inst = inst, *filter = weed_instance_get_filter(inst, TRUE);
  int *xvals;
  int nptmpls, error;

  int i;

  in_ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nptmpls);
  if (!in_ptmpls) return;

  for (i = 0; i < nptmpls; i++) {
    if (weed_plant_has_leaf(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
      xvals = weed_get_int_array(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION, &error);

      iparam = weed_inst_in_param(first_inst, i, FALSE, FALSE);

      if (xvals[0] > -1) {
        inst = first_inst;
        while (--xvals[0] >= 0) inst = get_next_compound_inst(inst);

        outp = weed_get_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, &error);
        oparam = outp[xvals[1]];
        lives_free(outp);
      } else oparam = iparam; // just hide the parameter, but don't pull a value

      weed_set_plantptr_value(iparam, WEED_LEAF_HOST_INTERNAL_CONNECTION, oparam);

      if (weed_plant_has_leaf(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE) &&
          weed_get_boolean_value(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE, &error) == WEED_TRUE)
        weed_set_boolean_value(iparam, WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE, WEED_TRUE);
      lives_free(xvals);
    }
  }
  lives_free(in_ptmpls);
}


weed_plant_t *weed_instance_from_filter(weed_plant_t *filter) {
  // return an instance from a filter, with all compound instances refcounted 1
  weed_plant_t **inc = NULL, **outc = NULL, **inp = NULL, **outp = NULL, **xinp;

  weed_plant_t *last_inst = NULL, *first_inst = NULL, *inst, *ofilter = filter;
  weed_plant_t *ochan = NULL;

  int *filters = NULL, *xvals;

  char *key;

  int nfilters, ninpar = 0;

  weed_error_t error;

  int xcnumx = 0, x, poffset = 0;

  int i, j;

  if ((nfilters = num_compound_fx(filter)) > 1) {
    filters = weed_get_int_array(filter, WEED_LEAF_HOST_FILTER_LIST, NULL);
  }

  inp = weed_params_create(filter, TRUE);

  for (i = 0; i < nfilters; i++) {
    if (filters) {
      filter = weed_filters[filters[i]];
    }

    // create channels from channel_templates
    inc = weed_channels_create(filter, TRUE);
    outc = weed_channels_create(filter, FALSE);

    set_default_channel_sizes(filter, inc, outc); // we set the initial channel sizes to some reasonable defaults

    // create parameters from parameter_templates
    outp = weed_params_create(filter, FALSE);

    if (nfilters == 1) xinp = inp;
    else {
      // for a compound filter, assign only the params which belong to the instance
      if (ninpar == 0) xinp = NULL;
      else {
        xinp = (weed_plant_t **)lives_malloc((ninpar + 1) * sizeof(weed_plant_t *));
        x = 0;
        for (j = poffset; j < poffset + ninpar; j++) xinp[x++] = inp[j];
        xinp[x] = NULL;
        poffset += ninpar;
      }
    }

    inst = weed_create_instance(filter, inc, outc, xinp, outp);
    weed_instance_ref(inst);

    if (i == 0) {
      weed_set_boolean_value(inst, WEED_LEAF_HOST_CHKSTRG, WEED_TRUE);
    }

    if (filters) weed_set_plantptr_value(inst, WEED_LEAF_HOST_COMPOUND_CLASS, ofilter);

    if (i > 0) {
      weed_set_plantptr_value(last_inst, WEED_LEAF_HOST_NEXT_INSTANCE, inst);
    } else first_inst = inst;

    last_inst = inst;

    lives_freep((void **)&inc);
    lives_freep((void **)&outc);
    lives_freep((void **)&outp);
    if (xinp) {
      if (xinp == inp) inp = NULL;
      lives_free(xinp);
      xinp = NULL;
    }
  }

  if (inp) lives_free(inp);

  if (filters) {
    lives_free(filters);

    // for compound fx, add param and channel connections
    filter = ofilter;

    // add param connections
    add_param_connections(first_inst);

    while (1) {
      // add channel connections
      key = lives_strdup_printf(WEED_LEAF_HOST_CHANNEL_CONNECTION "%d", xcnumx++);
      if (weed_plant_has_leaf(filter, key)) {
        xvals = weed_get_int_array(filter, key, &error);

        inst = first_inst;
        while (1) {
          if (xvals[0]-- == 0) {
            // got the out instance
            outc = weed_get_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, &error);
            ochan = outc[xvals[1]];
            lives_freep((void **)&outc);
          }
          if (xvals[2]-- == 0) {
            // got the in instance
            inc = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, &error);
            weed_set_plantptr_value(inc[xvals[3]], WEED_LEAF_HOST_INTERNAL_CONNECTION, ochan);
            lives_freep((void **)&inc);
            break;
          }

          inst = get_next_compound_inst(inst);
        }

        lives_free(xvals);
        lives_free(key);
      } else {
        lives_free(key);
        break;
      }
    }
  }

  return first_inst;
}


boolean record_filter_init(int key) {
  // key is zero based
  weed_plant_t *filter = NULL;
  int idx = key_to_fx[key][key_modes[key]];
  if (idx >= 0) filter = weed_filters[idx];
  if (filter) {
    int inc_count = enabled_in_channels(filter, FALSE);
    int out_count = enabled_out_channels(filter, FALSE);
    if (all_ins_alpha(filter, TRUE)) inc_count = 0;
    if (all_outs_alpha(filter, TRUE)) out_count = 0;
    if (inc_count > 0 && out_count > 0) {
      weed_plant_t *inst = key_to_instance[key][key_modes[key]];
      if (inst) {
        weed_event_list_t *event_list;
        ticks_t actual_ticks = mainw->startticks; ///< use the "theoretical" time
        uint64_t new_rte = GU641 << (key), rteval = mainw->rte | new_rte;
        int ntracks;
        if (THREADVAR(fx_is_audio)) {
          // use real time when recording audio fx
          actual_ticks = THREADVAR(event_ticks);
        }
        pthread_mutex_lock(&mainw->event_list_mutex);
        event_list = append_filter_init_event(mainw->event_list, actual_ticks, idx, -1, key, inst);
        if (!mainw->event_list) mainw->event_list = event_list;
        init_events[key] = get_last_event(mainw->event_list);

        if (THREADVAR(fx_is_audio)) {
          weed_set_boolean_value(init_events[key], LIVES_LEAF_NOQUANT, WEED_TRUE);
        }

        pthread_mutex_unlock(&mainw->event_list_mutex);
        create_effects_map(rteval); // we create effects_map event_t * array with ordered effects
        pthread_mutex_lock(&mainw->event_list_mutex);

        if (init_events[key]) {
          ntracks = weed_leaf_num_elements(init_events[key], WEED_LEAF_IN_TRACKS);
          // add param values from inst
          pchains[key] = filter_init_add_pchanges(mainw->event_list, inst, init_events[key], ntracks, 0);
          mainw->event_list = append_filter_map_event(mainw->event_list, actual_ticks, effects_map);
          if (THREADVAR(fx_is_audio)) {
            weed_event_t *filter_map = get_last_event(mainw->event_list);
            weed_set_boolean_value(filter_map, LIVES_LEAF_NOQUANT, WEED_TRUE);
          }
        }
        pthread_mutex_unlock(&mainw->event_list_mutex);
        return TRUE;
      }
    }
  }
  return FALSE;
}


boolean record_filter_deinit(int key) {
  weed_plant_t *filter = NULL;
  int idx = key_to_fx[key][key_modes[key]];
  if (idx >= 0) filter = weed_filters[idx];
  if (filter) {
    int inc_count = enabled_in_channels(filter, FALSE);
    int out_count = enabled_out_channels(filter, FALSE);
    if (all_ins_alpha(filter, TRUE)) inc_count = 0;
    if (all_outs_alpha(filter, TRUE)) out_count = 0;
    if (inc_count > 0 && out_count > 0) {
      weed_event_t *deinit_event;
      ticks_t actual_ticks = mainw->startticks; ///< use the "theoretical" time
      uint64_t new_rte = GU641 << key, rteval = mainw->rte & ~new_rte;
      if (THREADVAR(fx_is_audio)) {
        // use real time when recording audio fx
        actual_ticks = THREADVAR(event_ticks);
      }

      pthread_mutex_lock(&mainw->event_list_mutex);
      mainw->event_list = append_filter_deinit_event(mainw->event_list, actual_ticks, init_events[key], NULL);
      deinit_event = get_last_event(mainw->event_list);
      pthread_mutex_unlock(&mainw->event_list_mutex);
      create_effects_map(rteval); // we create effects_map event_t * array with ordered effects
      pthread_mutex_lock(&mainw->event_list_mutex);
      mainw->event_list = append_filter_map_event(mainw->event_list, actual_ticks, effects_map);
      pthread_mutex_unlock(&mainw->event_list_mutex);

      if (THREADVAR(fx_is_audio)) {
        weed_event_t *filter_map = get_last_event(mainw->event_list);
        weed_set_boolean_value(filter_map, LIVES_LEAF_NOQUANT, WEED_TRUE);
        weed_set_boolean_value(deinit_event, LIVES_LEAF_NOQUANT, WEED_TRUE);
      }
      return TRUE;
    }
  }
  return FALSE;
}


boolean weed_init_effect(int hotkey) {
  weed_plant_t *filter, *channel;
  weed_plant_t *new_instance, *inst;
  weed_error_t error;

  boolean fg_modeswitch = FALSE, is_trans = FALSE, gen_start = FALSE, is_modeswitch = FALSE;
  boolean all_out_alpha;
  boolean is_gen = FALSE;
  boolean is_audio_gen = FALSE;

  int rte_keys = mainw->rte_keys;
  int inc_count, outc_count;
  int idx, ch, flags = 0;
  int retcode;

  mainw->error = FALSE;

  if (hotkey < 0) {
    is_modeswitch = TRUE;
    hotkey = -hotkey - 1;
  }

  if (hotkey == fg_generator_key) {
    fg_modeswitch = TRUE;
  }

  if (hotkey >= FX_KEYS_MAX) {
    mainw->error = TRUE;
    return FALSE;
  }

  if (!rte_key_valid(hotkey + 1, FALSE)) {
    mainw->error = TRUE;
    return FALSE;
  }

  idx = key_to_fx[hotkey][key_modes[hotkey]];
  filter = weed_filters[idx];

  inc_count = enabled_in_channels(filter, FALSE);
  outc_count = enabled_out_channels(filter, FALSE);

  if (all_ins_alpha(filter, TRUE)) inc_count = 0;

  if (inc_count == 0) is_gen = TRUE;

  // check first if it is an audio generator

  if ((is_gen || (has_audio_chans_in(filter, FALSE) && !has_video_chans_in(filter, TRUE)))
      && has_audio_chans_out(filter, FALSE) && !has_video_chans_out(filter, TRUE)) {
    if (!is_realtime_aplayer(prefs->audio_player)) {
      // audio fx only with realtime players
      char *fxname = weed_filter_idx_get_name(idx, FALSE, FALSE);
      d_print(_("Effect %s cannot be used with this audio player.\n"), fxname);
      lives_free(fxname);
      mainw->error = TRUE;
      return FALSE;
    }

    if (is_gen) {
      if (mainw->agen_key != 0) {
        // we had an existing audio gen running - stop that one first
        int agen_key = mainw->agen_key - 1;
        filter_mutex_lock(agen_key);
        weed_deinit_effect(agen_key);
        // need to do this in case starting another audio gen caused us to come here
        mainw->rte &= ~(GU641 << agen_key);
        mainw->rte_real &= ~(GU641 << agen_key);
        if (rte_window_visible() && !mainw->is_rendering && !mainw->multitrack) {
          rtew_set_keych(agen_key, FALSE);
        }
        if (mainw->ce_thumbs) ce_thumbs_set_keych(agen_key, FALSE);
        filter_mutex_unlock(agen_key);
      }
      is_audio_gen = TRUE;
    }
  }

  // if outputs are all alpha it is not a true (video/audio generating) generator
  all_out_alpha = all_outs_alpha(filter, TRUE);

  // TODO - block template channel changes
  if (hotkey < FX_KEYS_MAX_VIRTUAL) {
    // we must stop any old generators
    if (!all_out_alpha && is_gen && outc_count > 0 && hotkey != fg_generator_key && mainw->num_tr_applied > 0 &&
        mainw->blend_file != -1 && mainw->blend_file != mainw->current_file &&
        IS_VALID_CLIP(mainw->blend_file) &&
        mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR && !is_audio_gen) {
      /////////////////////////////////////// if blend_file is a generator we should finish it
      if (bg_gen_to_start == -1) {
        weed_generator_end((weed_instance_t *)get_primary_inst(mainw->files[mainw->blend_file]));
        bg_generator_key = bg_generator_mode = -1;
        //weed_layer_set_invalid(mainw->blend_layer, TRUE);
        mainw->new_blend_file = -1;
      }
    }

    if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR &&
        (fg_modeswitch || (is_gen && outc_count > 0 && mainw->num_tr_applied == 0)) && !is_audio_gen && !all_out_alpha) {
      if (mainw->is_processing || mainw->preview) {
        mainw->error = TRUE;
        return FALSE; // stopping fg gen will cause clip to switch
      }
      if (LIVES_IS_PLAYING && mainw->whentostop == STOP_ON_VID_END && !is_gen) {
        // stop on vid end, and the playing generator stopped
        mainw->cancelled = CANCEL_GENERATOR_END;
      } else {
        if (is_gen && mainw->whentostop == STOP_ON_VID_END) mainw->whentostop = NEVER_STOP;
        //////////////////////////////////// switch from one generator to another: keep playing and stop the old one
        if (CURRENT_CLIP_IS_VALID)
          weed_generator_end((weed_instance_t *)get_primary_inst(cfile));
        fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
        if (CURRENT_CLIP_IS_VALID && (cfile->achans == 0 || cfile->frames > 0)) {
          // in case we switched to bg clip, and bg clip was gen
          // otherwise we will get killed in generator_start
          // TODO - DO NOT DO THIS !
          mainw->current_file = -1;
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
    if (inc_count == 2) {
      pthread_mutex_lock(&mainw->trcount_mutex);
      mainw->num_tr_applied++; // increase trans count
      pthread_mutex_unlock(&mainw->trcount_mutex);
      if (mainw->active_sa_clips == SCREEN_AREA_FOREGROUND) {
        mainw->active_sa_clips = SCREEN_AREA_BACKGROUND;
      }
      if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_BACKGROUND);
      if (mainw->num_tr_applied == 1 && !is_modeswitch) {
        mainw->new_blend_file = mainw->current_file;
        if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR) {
          bg_generator_key = fg_generator_key;
          bg_generator_mode = fg_generator_mode;
        }
      }
      is_trans = TRUE;
    } else if (is_gen && outc_count > 0 && !is_audio_gen && !all_out_alpha) {
      // aha - a generator

      if (!LIVES_IS_PLAYING) {
        // if we are not playing, we will postpone creating the instance
        // this is a workaround for a problem in libvisual
        fg_gen_to_start = hotkey;
        fg_generator_key = hotkey;
        fg_generator_mode = key_modes[hotkey];
        gen_start = TRUE;
      } else if (!fg_modeswitch && !mainw->num_tr_applied && (mainw->is_processing || mainw->preview))  {
        mainw->error = TRUE;
        return FALSE;
      }
    }
  }

  // if the param window is already open, use instance from there
  if (fx_dialog[1] && fx_dialog[1]->key == hotkey && fx_dialog[1]->mode == key_modes[hotkey]) {
    lives_rfx_t *rfx = fx_dialog[1]->rfx;
    new_instance = (weed_plant_t *)rfx->source;
    weed_instance_ref(new_instance);
    update_widget_vis(NULL, hotkey, key_modes[hotkey]); // redraw our paramwindow
  } else {
    new_instance = key_to_instance[hotkey][key_modes[hotkey]];
    if (!new_instance) new_instance = weed_instance_from_filter(filter);
    // if it is a key effect, set key defaults
    if (hotkey < FX_KEYS_MAX_VIRTUAL && key_defaults[hotkey][key_modes[hotkey]]) {
      // TODO - handle compound fx
      apply_key_defaults(new_instance, hotkey, key_modes[hotkey]);
    }
  }

  if (!mainw->multitrack) {
    // record the key so we know whose parameters to record later
    weed_set_int_value(new_instance, WEED_LEAF_HOST_KEY, hotkey);
  }

  inst = new_instance;

  /* gui = weed_instance_get_gui(inst, FALSE); */
  /* if (gui && weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL)) { */
  /*   weed_leaf_delete(gui, WEED_LEAF_EASE_OUT); */
  /* } */
  /* weed_leaf_delete(inst, LIVES_LEAF_AUTO_EASING); */

  if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS))
    lives_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_HOST_FPS);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS))
    lives_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_PREFERRED_FPS);

  while ((inst = get_next_compound_inst(inst))) {
    weed_set_int_value(inst, WEED_LEAF_HOST_KEY, hotkey);
  }

  inst = new_instance;

  if (!gen_start) {
    boolean init_now = FALSE;
    if (LIVES_IS_PLAYING) {
      for (ch = 0; ch < inc_count; ch++) {
        channel = get_enabled_channel(inst, ch, LIVES_OUTPUT);
        if (channel) {
          weed_plant_t *chantmpl = weed_channel_get_template(channel);
          flags |= weed_chantmpl_get_flags(chantmpl);
        }
      }
      for (ch = 0; ch < outc_count; ch++) {
        channel = get_enabled_channel(inst, ch, LIVES_INPUT);
        if (channel) {
          weed_plant_t *chantmpl = weed_channel_get_template(channel);
          flags |= weed_chantmpl_get_flags(chantmpl);
        }
      }
      if ((flags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
          || (flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)) init_now = TRUE;
      else {
        int np;
        weed_plant_t **in_params = weed_instance_get_in_params(inst, &np);
        for (int i = 0; i < np; i++) {
          weed_plant_t *paramtmpl = weed_param_get_template(in_params[i]);
          weed_plant_t *gui = weed_paramtmpl_get_gui(paramtmpl, FALSE);
          if (gui && (weed_gui_get_flags(gui) & WEED_GUI_CHOICES_SET_ON_INIT)) {
            init_now = TRUE;
            break;
          }
        }
        lives_freep((void **)&in_params);
      }
    } else init_now = TRUE;

    if (init_now) {
      weed_plant_t *next_inst = NULL;
start1:
      if ((error = weed_call_init_func(inst)) != WEED_SUCCESS) {
        char *filter_name;
        filter = weed_filters[idx];
        filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
        d_print(_("Failed to start instance %s, (%s)\n"), filter_name, weed_strerror(error));
        lives_free(filter_name);
deinit2:

        next_inst = get_next_compound_inst(inst);
        //weed_call_deinit_func(inst);
        weed_instance_unref(inst);

        if (next_inst && weed_get_boolean_value(next_inst, WEED_LEAF_HOST_INITED, &error) == WEED_TRUE) {
          // handle compound fx
          inst = next_inst;
          goto deinit2;
        }

        if (is_audio_gen) mainw->agen_needs_reinit = FALSE;
        mainw->error = TRUE;
        if (hotkey < FX_KEYS_MAX_VIRTUAL) {
          if (is_trans) {
            pthread_mutex_lock(&mainw->trcount_mutex);
            mainw->num_tr_applied--;
            pthread_mutex_unlock(&mainw->trcount_mutex);
            if (!mainw->num_tr_applied) {
              if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_FOREGROUND);
              if (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) {
                mainw->active_sa_clips = SCREEN_AREA_FOREGROUND;
		// *INDENT-OFF*
	      }}}}
	// *INDENT-ON*

        return FALSE;
      }

      if ((inst = get_next_compound_inst(inst)) != NULL) {
        goto start1;
      }
      inst = new_instance;
    }

    filter = weed_filters[idx];
  }

  if (is_gen && outc_count > 0 && !is_audio_gen && !all_out_alpha) {
    int num_tr_applied;
    if (fg_modeswitch) pthread_mutex_lock(&mainw->trcount_mutex);
    num_tr_applied = mainw->num_tr_applied;
    // generator start
    if (mainw->num_tr_applied > 0 && !fg_modeswitch && mainw->current_file > -1 && LIVES_IS_PLAYING) {
      // transition is on, make into bg clip
      bg_generator_key = hotkey;
      bg_generator_mode = key_modes[hotkey];
    } else {
      // no transition, make fg (or kb was grabbed for fg generator)
      fg_generator_key = hotkey;
      fg_generator_mode = key_modes[hotkey];
    }

    // start the generator and maybe start playing
    if (fg_modeswitch) mainw->num_tr_applied = 0; // force to fg

    // enable param recording, in case the instance was obtained from a param window
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)) weed_leaf_delete(inst, WEED_LEAF_HOST_NORECORD);

    retcode = weed_generator_start(inst, hotkey);
    if (retcode == -1) {
      // playback triggered
      filter_mutex_lock(hotkey);
      key_to_instance[hotkey][key_modes[hotkey]] = inst;
      filter_mutex_unlock(hotkey);
      return TRUE;
    }

    if (retcode != 0) {
      int weed_error;
      char *filter_name;
      if (fg_modeswitch) {
        mainw->num_tr_applied = num_tr_applied;
        pthread_mutex_unlock(&mainw->trcount_mutex);
      }

      weed_call_deinit_func(inst);
      weed_instance_unref(inst);

      if (retcode != 2) {
        filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, &weed_error);
        d_print(_("Unable to start generator %s (error code: %d)\n"), filter_name, retcode);
        lives_free(filter_name);
      } else mainw->error = TRUE;
      if (mainw->num_tr_applied && mainw->current_file > -1) {
        bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;
      } else {
        fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
      }

      if (!mainw->multitrack) {
        if (!LIVES_IS_PLAYING) {
          switch_clip(1, mainw->current_file, TRUE);
        }
      }
      return FALSE;
    }

    /* if (!LIVES_IS_PLAYING && mainw->gen_started_play) { */
    /*   // TODO - problem if modeswitch triggers playback */
    /*   // hence we do not allow mixing of generators and non-gens on the same key */
    /*   if (fg_modeswitch) { */
    /*     mainw->num_tr_applied = num_tr_applied; */
    /*     pthread_mutex_unlock(&mainw->trcount_mutex); */
    /*   } */
    /*   filter_mutex_lock(hotkey); */
    /*   key_to_instance[hotkey][key_modes[hotkey]] = inst; */
    /*   filter_mutex_unlock(hotkey); */
    /*   return TRUE; */
    /* } */

    // weed_generator_start can change the instance
    //inst = weed_instance_obtain(hotkey, key_modes[hotkey]);/////

    if (fg_modeswitch) {
      mainw->num_tr_applied = num_tr_applied;
      pthread_mutex_unlock(&mainw->trcount_mutex);
    }
    if (fg_generator_key != -1) {
      mainw->rte |= (GU641 << fg_generator_key);
      //mainw->rte_real |= (GU641 << fg_generator_key);
      mainw->clip_switched = TRUE;
      mainw->playing_sel = FALSE;
    }
    if (bg_generator_key != -1 && !fg_modeswitch) {
      filter_mutex_lock(bg_generator_key);
      mainw->rte |= (GU641 << bg_generator_key);
      mainw->rte_real |= (GU641 << bg_generator_key);
      if (hotkey < prefs->rte_keys_virtual) {
        if (rte_window_visible() && !mainw->is_rendering && !mainw->multitrack) {
          rtew_set_keych(bg_generator_key, TRUE);
        }
        if (mainw->ce_thumbs) ce_thumbs_set_keych(bg_generator_key, TRUE);
      }
      filter_mutex_unlock(bg_generator_key);
    }
  }

  if (rte_keys == hotkey) {
    /* mainw->rte_real |= (~mainw->rte_keys & rte_keys); */
    /* mainw->rte_real &= ~(mainw->rte_keys & ~rte_keys); */
    //mainw->rte_keys = rte_keys;
    mainw->blend_factor = weed_get_blend_factor(rte_keys);
  }

  if (hotkey < FX_KEYS_MAX_VIRTUAL) filter_mutex_lock(hotkey);
  // need to do this *before* calling record_filter_init()
  key_to_instance[hotkey][key_modes[hotkey]] = inst;
  if (hotkey < FX_KEYS_MAX_VIRTUAL) filter_mutex_unlock(hotkey);

  // enable param recording, in case the instance was obtained from a param window
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)) weed_leaf_delete(inst, WEED_LEAF_HOST_NORECORD);

  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
    // record filter init event + parameters with key defaults applied
    record_filter_init(hotkey);
  }

  if (is_audio_gen) {
    mainw->agen_key = hotkey + 1;
    mainw->agen_needs_reinit = FALSE;

    if (mainw->playing_file > 0) {
      if (mainw->whentostop == STOP_ON_AUD_END) mainw->whentostop = STOP_ON_VID_END;
      if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
        if (mainw->jackd_read && mainw->jackd_read->in_use &&
            (mainw->jackd_read->playing_file == -1 || mainw->jackd_read->playing_file == mainw->ascrap_file)) {
          // if playing external audio, switch over to internal for an audio gen
          jack_time_reset(mainw->jackd, mainw->currticks);
          // close the reader
          jack_rec_audio_end(FALSE);
        }
        if (mainw->jackd && (!mainw->jackd_read || !mainw->jackd_read->in_use)) {
          // enable writer
          mainw->jackd->in_use = TRUE;
        }
#endif
      }
      if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
        if (mainw->pulsed_read && mainw->pulsed_read->in_use &&
            (mainw->pulsed_read->playing_file == -1 || mainw->pulsed_read->playing_file == mainw->ascrap_file)) {
          // if playing external audio, switch over to internal for an audio gen
          ticks_t audio_ticks = lives_pulse_get_time(mainw->pulsed_read);
          if (audio_ticks == -1) {
            mainw->cancelled = handle_audio_timeout();
            return mainw->cancelled;
          }
          pa_time_reset(mainw->pulsed, -audio_ticks);
          pulse_rec_audio_end(FALSE);
          pulse_driver_uncork(mainw->pulsed);
        }
        if (mainw->pulsed && (!mainw->pulsed_read || !mainw->pulsed_read->in_use)) {
          mainw->pulsed->in_use = TRUE;
        }
#endif
      }

      if (LIVES_IS_PLAYING && mainw->record && !mainw->record_paused && prefs->audio_src != AUDIO_SRC_EXT &&
          (prefs->rec_opts & REC_AUDIO)) {
        // if recording audio, open ascrap file and add audio event
        mainw->record = FALSE;
        on_record_perf_activate(NULL, NULL);
        mainw->record_starting = FALSE;
        mainw->record = TRUE;
        if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
          jack_time_reset(mainw->jackd, lives_jack_get_time(mainw->jackd_read));
          mainw->jackd->in_use = TRUE;
#endif
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  return TRUE;
}


static pthread_mutex_t inited_mutex = PTHREAD_MUTEX_INITIALIZER;

weed_error_t weed_call_init_func(weed_plant_t *inst) {
  weed_plant_t *filter;
  weed_error_t error = WEED_SUCCESS;
  uint64_t rseed;

  pthread_mutex_lock(&inited_mutex);
  if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_TRUE) {
    pthread_mutex_unlock(&inited_mutex);
    return WEED_SUCCESS;
  }
  weed_set_boolean_value(inst, WEED_LEAF_HOST_INITED, WEED_TRUE);
  pthread_mutex_unlock(&inited_mutex);

  rseed = THREADVAR(random_seed);
  if (!rseed) rseed = gen_unique_id();
  weed_set_int64_value(inst, WEED_LEAF_RANDOM_SEED, rseed);
  filter = weed_instance_get_filter(inst, FALSE);
  if (weed_plant_has_leaf(filter, WEED_LEAF_INIT_FUNC)) {
    weed_init_f init_func = (weed_init_f)weed_get_funcptr_value(filter, WEED_LEAF_INIT_FUNC, NULL);
    if (init_func) {
      char *cwd = cd_to_plugin_dir(filter);
      error = (*init_func)(inst);
      lives_chdir(cwd, FALSE);
      lives_free(cwd);
      if (weed_get_boolean_value(inst, WEED_LEAF_HOST_CHKSTRG, NULL) == WEED_TRUE) {
        check_string_choice_params(inst);
        weed_leaf_delete(inst, WEED_LEAF_HOST_CHKSTRG);
      }
    }
  }
  return error;
}


weed_error_t weed_call_deinit_func(weed_plant_t *instance) {
  weed_plant_t *filter;
  weed_error_t error = WEED_SUCCESS;

  //g_print("TMINGS: %d %ld and %d %ld nth\n", thcount, totth, nthcount, totnth);

  pthread_mutex_lock(&inited_mutex);
  if (weed_get_boolean_value(instance, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE) {
    pthread_mutex_unlock(&inited_mutex);
    return WEED_SUCCESS;
  }
  weed_set_boolean_value(instance, WEED_LEAF_HOST_INITED, WEED_FALSE);
  pthread_mutex_unlock(&inited_mutex);

  filter = weed_instance_get_filter(instance, FALSE);

  if (weed_plant_has_leaf(filter, WEED_LEAF_DEINIT_FUNC)) {
    weed_deinit_f deinit_func =
      (weed_deinit_f)weed_get_funcptr_value(filter, WEED_LEAF_DEINIT_FUNC, NULL);
    if (deinit_func) {
      char *cwd = cd_to_plugin_dir(filter);
      error = (*deinit_func)(instance);
      lives_chdir(cwd, FALSE);
      lives_free(cwd);
    }
  }

  return error;
}


boolean weed_deinit_effect(int hotkey) {
  // REMOVES 1 ref
  // hotkey is 0 based
  // caller should also handle mainw->rte

  weed_plant_t *instance, *inst, *last_inst, *next_inst, *filter, *gui;

  boolean is_modeswitch = FALSE;
  boolean was_transition = FALSE;
  boolean is_audio_gen = FALSE;
  boolean is_video_gen = FALSE;
  int num_in_chans, num_out_chans;

  int easing;

  if (hotkey < 0) {
    is_modeswitch = TRUE;
    hotkey = -hotkey - 1;
  }

  if (hotkey >= FX_KEYS_MAX) return FALSE;

  // adds a ref
  if (!(instance = weed_instance_obtain(hotkey, key_modes[hotkey]))) return TRUE;

  if (fx_key_defs[hotkey].flags & FXKEY_SOFT_DEINIT)
    fx_key_defs[hotkey].flags &= ~FXKEY_SOFT_DEINIT;

  if (LIVES_IS_PLAYING && hotkey < FX_KEYS_MAX_VIRTUAL) {
    if (prefs->allow_easing) {
      // if it's a user key and the plugin supports easing out, we'll do that instead
      if (!weed_plant_has_leaf(instance, LIVES_LEAF_AUTO_EASING)) {
        weed_plant_t *gui = weed_instance_get_gui(instance, FALSE);
        if (!weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)) {
          if (rte_key_is_enabled(hotkey, TRUE)) {
            if ((easing = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL))) {
              // try to avoid errors in plugins by ensuring we should be able to ease out quick
              int myease = cfile->pb_fps * MAX_EASE_SECS;
              if (easing > myease) easing = myease;
              weed_set_int_value(gui, WEED_LEAF_EASE_OUT, easing);
              if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
                  (prefs->rec_opts & REC_EFFECTS) && init_events[hotkey])
                weed_set_int_value(init_events[hotkey], WEED_LEAF_EASE_OUT, easing);
              weed_instance_unref(instance);
              return FALSE;
	      // *INDENT-OFF*
	    }}}}}}
  // *INDENT-ON*

  // disable param recording, in case the instance is still attached to a param window
  weed_set_boolean_value(instance, WEED_LEAF_HOST_NORECORD, WEED_TRUE);

  num_in_chans = enabled_in_channels(instance, FALSE);

  // ensure we are not still easing out if we aren't supposed to be
  gui = weed_instance_get_gui(instance, FALSE);
  if (gui && weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)) {
    weed_leaf_delete(gui, WEED_LEAF_EASE_OUT);
  }
  weed_leaf_delete(instance, LIVES_LEAF_AUTO_EASING);

  // handle compound fx
  last_inst = instance;
  while (weed_plant_has_leaf(last_inst, WEED_LEAF_HOST_NEXT_INSTANCE))
    last_inst = weed_get_plantptr_value(last_inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
  num_out_chans = enabled_out_channels(last_inst, FALSE);

  if (hotkey + 1 == mainw->agen_key) is_audio_gen = TRUE;
  else if (num_in_chans == 0) is_video_gen = TRUE;

  filter = weed_instance_get_filter(instance, TRUE);
  if ((is_video_gen || all_ins_alpha(filter, TRUE)) && num_out_chans > 0 && !all_outs_alpha(filter, TRUE)) {
    weed_instance_unref(instance); // remove ref from weed instance obtain
    /////////////////////////////////// deinit a (video) generator
    if (LIVES_IS_PLAYING && mainw->whentostop == STOP_ON_VID_END && (hotkey != bg_generator_key)) {
      mainw->cancelled = CANCEL_GENERATOR_END; // will be unreffed on pb end
    } else {
      mainw->rte &= ~(GU641 << bg_generator_key);
      mainw->rte_real &= ~(GU641 << bg_generator_key);
      weed_generator_end(instance); // removes 1 ref
    }
    return TRUE;
  }

  if (is_audio_gen) {
    // is audio generator
    // wait for current processing to finish
    mainw->agen_key = 0;
    mainw->agen_samps_count = 0;

    // for external audio, switch back to reading

    if (mainw->playing_file > 0 && prefs->audio_src == AUDIO_SRC_EXT) {
#ifdef ENABLE_JACK
      if (prefs->audio_player == AUD_PLAYER_JACK) {
        if (!mainw->jackd_read || !mainw->jackd_read->in_use) {
          mainw->jackd->in_use = FALSE; // deactivate writer
          jack_rec_audio_to_clip(-1, 0, RECA_MONITOR); //activate reader
          jack_time_reset(mainw->jackd_read, lives_jack_get_time(mainw->jackd)); // ensure time continues monotonically
          if (mainw->record) mainw->jackd_read->playing_file = mainw->ascrap_file; // if recording, continue to write to ascrap file
          mainw->jackd_read->is_paused = FALSE;
          mainw->jackd_read->in_use = TRUE;
          if (mainw->jackd) {
            mainw->jackd->playing_file = -1;
          }
        }
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player == AUD_PLAYER_PULSE) {
        if (!mainw->pulsed_read || !mainw->pulsed_read->in_use) {
          if (mainw->pulsed) mainw->pulsed->in_use = FALSE; // deactivate writer
          pulse_rec_audio_to_clip(-1, 0, RECA_MONITOR); //activate reader
          if (mainw->pulsed) {
            ticks_t audio_ticks = lives_pulse_get_time(mainw->pulsed_read);
            if (audio_ticks == -1) {
              mainw->cancelled = handle_audio_timeout();
              weed_instance_unref(instance); // remove ref from weed instance obtain
              return TRUE;
            }
            pa_time_reset(mainw->pulsed_read, -audio_ticks); // ensure time continues monotonically
            pulse_driver_uncork(mainw->pulsed_read);
            if (mainw->pulsed) {
              pulse_driver_cork(mainw->pulsed);
              mainw->pulsed->playing_file = -1;
            }
          }
          if (mainw->record) mainw->pulsed_read->playing_file = mainw->ascrap_file; // if recording, continue to write to ascrap file
          mainw->pulsed_read->is_paused = FALSE;
          mainw->pulsed_read->in_use = TRUE;
        }
      }
#endif
    } else if (mainw->playing_file > 0) {
      // for internal, continue where we should
      if (is_realtime_aplayer(prefs->audio_player)) {
        if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) switch_audio_clip(mainw->playing_file, TRUE);
        else {
          if (mainw->pre_src_audio_file == -1) {
            // audio doesn't follow clip switches and we were playing this...
            mainw->cancelled = CANCEL_AUD_END;
          } else {
#ifdef HAVE_PULSE_AUDIO
            if (prefs->audio_player == AUD_PLAYER_PULSE) {
              if (mainw->pulsed) mainw->pulsed->playing_file = mainw->pre_src_audio_file;
              if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
                pulse_get_rec_avals(mainw->pulsed);
              }
            }
#endif
#ifdef ENABLE_JACK
            if (prefs->audio_player == AUD_PLAYER_JACK) {
              if (mainw->jackd) mainw->jackd->playing_file = mainw->pre_src_audio_file;
              if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
                jack_get_rec_avals(mainw->jackd);
              }
            }
#endif
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  if (hotkey < FX_KEYS_MAX_VIRTUAL) filter_mutex_lock(hotkey);
  key_to_instance[hotkey][key_modes[hotkey]] = NULL;
  if (hotkey < FX_KEYS_MAX_VIRTUAL) filter_mutex_unlock(hotkey);

  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS) && num_in_chans > 0) {
    // must be done before removing init_event
    record_filter_deinit(hotkey);
  }

  if (hotkey < FX_KEYS_MAX_VIRTUAL) {
    init_events[hotkey] = NULL;
    if (mainw->whentostop == STOP_ON_VID_END && mainw->current_file > -1 &&
        (cfile->frames == 0 || (mainw->loop && cfile->achans > 0 && !mainw->is_rendering && (mainw->audio_end / cfile->fps)
                                < MAX(cfile->laudio_time, cfile->raudio_time)))) mainw->whentostop = STOP_ON_AUD_END;

    if (num_in_chans == 2) {
      was_transition = TRUE;
      mainw->num_tr_applied--;
      if (mainw->num_tr_applied == 0) {
        if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_FOREGROUND);
        if (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) {
          mainw->active_sa_clips = SCREEN_AREA_FOREGROUND;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
  inst = instance;

deinit3:

  next_inst = get_next_compound_inst(inst);

  weed_call_deinit_func(inst);

  // removes the ref we added in weed_instance_obtain
  weed_instance_unref(inst);

  if (next_inst) {
    // handle compound fx
    weed_instance_unref(inst);  // free if no other refs
    inst = next_inst;
    weed_instance_ref(inst); // proxy for weed_instance_obtain
    goto deinit3;
  }

  // if the param window is already open, show any reinits now
  if (fx_dialog[1] && fx_dialog[1]->key == hotkey && fx_dialog[1]->mode == key_modes[hotkey]) {
    update_widget_vis(NULL, hotkey, key_modes[hotkey]); // redraw our paramwindow
  }

  if (was_transition && !is_modeswitch) {
    if (mainw->num_tr_applied < 1) {
      if (CURRENT_CLIP_IS_VALID && mainw->effort > 0) {
        reset_effort();
      }
      if (bg_gen_to_start != -1) bg_gen_to_start = -1;
      if (mainw->blend_file != -1 && mainw->blend_file != mainw->current_file && mainw->files[mainw->blend_file] &&
          mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
        int bgk = bg_generator_key;
        // all transitions off, so end the bg generator
        filter_mutex_lock(bgk);
        weed_deinit_effect(bgk);
        mainw->rte &= ~(GU641 << bgk);
        mainw->rte_real &= ~(GU641 << bgk);
        if (rte_window_visible()) rtew_set_keych(bgk, FALSE);
        if (mainw->ce_thumbs) ce_thumbs_set_keych(bgk, FALSE);
        filter_mutex_unlock(bgk);
      }
      //weed_layer_set_invalid(mainw->blend_layer, TRUE);
      mainw->new_blend_file = -1;
    }
  }

  weed_instance_unref(inst);  // free if no other refs

  if (!THREADVAR(fx_is_audio)) {
    if (pchains[hotkey]) lives_free(pchains[hotkey]);
    pchains[hotkey] = NULL;
  }
  return TRUE;
}


/**
   @brief switch off effects after render preview
   during rendering/render preview, we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
   here we deinit all active ones, similar to weed_deinit_all, but we use higher keys

   @see weed_deinit_all()
*/
void deinit_render_effects(void) {
  for (int i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    if (key_to_instance[i][0]) {
      // no mutex needed since we are rendering
      weed_deinit_effect(i);
      if (mainw->multitrack && mainw->multitrack->is_rendering && pchains[i]) {
        if (pchains[i]) lives_free(pchains[i]);
        pchains[i] = NULL;
	// *INDENT-OFF*
      }}
    key_to_fx[i][0] = -1;
  }
  // *INDENT-ON*
}


void **get_easing_events(int *nev) {
  void **eevents = NULL;
  uint8_t xevents[FX_KEYS_MAX_VIRTUAL];
  *nev = 0;
  lives_memset(xevents, 0, FX_KEYS_MAX_VIRTUAL);
  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    filter_mutex_lock(i);
    if (init_events[i] && weed_plant_has_leaf(init_events[i], WEED_LEAF_EASE_OUT)) {
      weed_leaf_delete(init_events[i], WEED_LEAF_EASE_OUT);
      filter_mutex_unlock(i);
      xevents[i] = 1;
      (*nev)++;
    } else filter_mutex_unlock(i);
  }
  if (*nev) {
    int xnev = *nev;
    eevents = (void **)lives_calloc(xnev, sizeof(void *));
    for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
      if (xevents[i]) {
        eevents[--xnev] = init_events[i];
        if (!xnev) break;
      }
    }
  }
  return eevents;
}


/**
   @brief switch off effects in easing out state after playback ends
   during playback, some effects don't deinit right awy, instead they ease out
   if plyback ends while they are in the easing out state they won't get deinited, so we need
   to take care of that
   @see weed_deinit_all()
*/
void deinit_easing_effects(void) {
  weed_plant_t *instance;
  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    filter_mutex_lock(i);
    if ((instance = key_to_instance[i][key_modes[i]]) != NULL) {
      weed_instance_ref(instance);
      filter_mutex_unlock(i);
      weed_plant_t *gui = weed_instance_get_gui(instance, FALSE);
      if (gui) {
        if (weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)) {
          // no mutex needed since we are rendering. and since we aren't playing it will get deinited now
          weed_deinit_effect(i);
          weed_instance_unref(instance);
          /// if recording, the deinit_event won't be recorded, since we are not playing now,
          /// this will be handled in event_list_add_end_events() so it need not concern us
        }
      }
      weed_instance_unref(instance);
    } else filter_mutex_unlock(i);
  }
}


/**
   @brief deinit all effects (except generators* during playback)
   this is called on ctrl-0 or on shutdown
   background generators will be killed because their transition will be deinited
*/
void weed_deinit_all(boolean shutdown) {
  weed_plant_t *instance;

  if (rte_window_visible()) rtew_set_keygr(-1);

  mainw->rte_keys = -1;
  mainw->last_grabbable_effect = -1;
  if (!LIVES_IS_PLAYING) bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (!shutdown) {
      // maintain braces because of #DEBUG_FILTER_MUTEXES
      if (LIVES_IS_PLAYING && i >= FX_KEYS_PHYSICAL) {
        // meta-physical  keys only shutdown through the interface or via easter-egg keys...
        return;
      }
      filter_mutex_lock(i);
    }
    if (rte_key_is_enabled(i, FALSE)) {
      if ((instance = weed_instance_obtain(i, key_modes[i])) != NULL) {

        weed_instance_unref(instance);
        if (shutdown || !LIVES_IS_PLAYING || !CURRENT_CLIP_IS_VALID
            || get_primary_inst(cfile) != (void *)instance) {
          weed_deinit_effect(i);
          mainw->rte &= ~(GU641 << i);
          if (rte_window_visible()) rtew_set_keych(i, FALSE);
          if (mainw->ce_thumbs) ce_thumbs_set_keych(i, FALSE);
          weed_instance_unref(instance);
        }
      }
    }
    if (!shutdown) filter_mutex_unlock(i);
  }
}


/**
   @brief registration fn. for video effects with audio input channels
   during playback there is the option of buffering audio sent to the soundcard
   if a video effect requires audio it can register itself here, and the audio buffer will be filled and
   only refreshed after all clients have read from it. Once this has happened, the read / write buffers are swapped for the
   following cycle. This ensures that no client will miss audio samples, at the cost of some minor latency.
   Purely audio filters are run directly during the audio cycle or by the audio caching thread.
*/
int register_audio_client(boolean is_vid) {
  if (!is_realtime_aplayer(prefs->audio_player)) {
    return -1;
  }

  if (!mainw->afbuffer) {
    init_audio_frame_buffers(prefs->audio_player);
    mainw->afbuffer->aclients = mainw->afbuffer->vclients = 0;
    mainw->afbuffer->aclients_read = mainw->afbuffer->vclients_read = 0;
  }

  if (is_vid) mainw->afbuffer->vclients++;
  else mainw->afbuffer->aclients++;

  if (is_vid) return mainw->afbuffer->vclients;
  return mainw->afbuffer->aclients;
}

int unregister_audio_client(boolean is_vid) {
  if (!mainw->afbuffer) return -1;

  if (is_vid) mainw->afbuffer->vclients--;
  else mainw->afbuffer->aclients--;
  if (mainw->afbuffer->aclients <= 0 && mainw->afbuffer->vclients <= 0) {
    // lock out the audio thread
    free_audio_frame_buffer(mainw->afbuffer);
    mainw->afbuffer = NULL;
    return 0;
  }

  if (is_vid) return mainw->afbuffer->vclients;
  return mainw->afbuffer->aclients;
}


boolean fill_audio_channel(weed_plant_t *filter, weed_plant_t *achan, boolean is_vid) {
  // this is for filter instances with mixed audio / video inputs/outputs
  // uneffected audio is buffered by the audio thread; here we copy / convert it to a video effect's audio channel
  // purely audio filters run in the audio thread
  // now used also to pass loopback audio from reader to writer
  lives_audio_buf_t *audbuf;
  if (!achan) return TRUE;

  weed_set_int_value(achan, WEED_LEAF_AUDIO_DATA_LENGTH, 0);
  weed_set_voidptr_value(achan, WEED_LEAF_AUDIO_DATA, NULL);

  audbuf = mainw->afbuffer;
  if (!achan || !audbuf || audbuf->write_pos < 0) return FALSE;
  if (is_vid) {
    if (!audbuf->vclients) return FALSE;
    push_audio_to_channel(filter, achan, audbuf, TRUE);
    if (++audbuf->vclients_read >= audbuf->vclients) audbuf->vclients_read = 0;
    if (!audbuf->vclients_read) {
      audbuf->vclient_readpos = audbuf->vclient_readlevel;
      audbuf->vclient_readlevel = audbuf->write_pos;
    }
  } else {
    if (!audbuf->aclients) return FALSE;
    push_audio_to_channel(filter, achan, audbuf, FALSE);
    if (++audbuf->aclients_read >= audbuf->aclients) audbuf->aclients_read = 0;
    if (!audbuf->aclients_read) {
      audbuf->aclient_readpos = audbuf->aclient_readlevel;
      audbuf->aclient_readlevel = audbuf->write_pos;
    }
  }
  return TRUE;
}


int register_aux_audio_channels(int nchannels) {
  if (!is_realtime_aplayer(prefs->audio_player)) {
    mainw->afbuffer_aux_clients = 0;
    return -1;
  }
  if (nchannels <= 0) return mainw->afbuffer_aux_clients;
  pthread_mutex_lock(&mainw->abuf_aux_frame_mutex);
  if (mainw->afbuffer_aux_clients == 0) {
    init_aux_audio_frame_buffers(prefs->audio_player);
    mainw->afbuffer_aux_clients_read = 0;
  }
  mainw->afbuffer_aux_clients += nchannels;
  pthread_mutex_unlock(&mainw->abuf_aux_frame_mutex);
  return mainw->afbuffer_aux_clients;
}


int unregister_aux_audio_channels(int nchannels) {
  if (!mainw->audio_frame_buffer_aux) {
    mainw->afbuffer_aux_clients = 0;
    return -1;
  }

  mainw->afbuffer_aux_clients -= nchannels;
  if (mainw->afbuffer_aux_clients <= 0) {
    // lock out the audio thread
    pthread_mutex_lock(&mainw->abuf_aux_frame_mutex);
    for (int i = 0; i < 2; i++) {
      if (mainw->afb_aux[i]) {
        free_audio_frame_buffer(mainw->afb_aux[i]);
        lives_free(mainw->afb_aux[i]);
        mainw->afb_aux[i] = NULL;
      }
    }
    mainw->audio_frame_buffer_aux = NULL;
    pthread_mutex_unlock(&mainw->abuf_aux_frame_mutex);
  }
  return mainw->afbuffer_aux_clients;
}


boolean fill_audio_channel_aux(weed_plant_t *achan) {
  // this is for duplex mode, we read from aux inputs
  static lives_audio_buf_t *audbuf;

  if (achan) {
    weed_set_int_value(achan, WEED_LEAF_AUDIO_DATA_LENGTH, 0);
    weed_set_voidptr_value(achan, WEED_LEAF_AUDIO_DATA, NULL);
  }

  if (!mainw->audio_frame_buffer_aux || mainw->audio_frame_buffer_aux->samples_filled <= 0
      || mainw->afbuffer_aux_clients == 0) {
    // no audio has been buffered
    return FALSE;
  }

  // lock the buffers

  if (mainw->afbuffer_aux_clients_read == 0) {
    /// when the first client reads, we grab the audio frame buffer and swap the other for writing
    // cast away the (volatile)
    pthread_mutex_lock(&mainw->abuf_aux_frame_mutex);
    audbuf = (lives_audio_buf_t *)mainw->audio_frame_buffer_aux;
    if (audbuf == mainw->afb_aux[0]) mainw->audio_frame_buffer_aux = mainw->afb_aux[1];
    else mainw->audio_frame_buffer_aux = mainw->afb_aux[0];
    pthread_mutex_unlock(&mainw->abuf_aux_frame_mutex);
  }

  // push read buffer to channel
  if (achan && audbuf) {
    // convert audio to format requested, and copy it to the audio channel data
    push_audio_to_channel(NULL, achan, audbuf, FALSE);
  }

  if (++mainw->afbuffer_aux_clients_read >= mainw->afbuffer_aux_clients) {
    // all clients have read the data, now we can free it
    free_audio_frame_buffer(audbuf);
    mainw->afbuffer_aux_clients_read = 0;
  }

  return TRUE;
}


/////////////////////
// special handling for generators (sources)

lives_filter_error_t lives_layer_fill_from_generator(weed_layer_t *layer, weed_instance_t *inst, weed_timecode_t tc) {
  weed_filter_t *filter;
  weed_channel_t *channel, *achan;
  weed_layer_t *inter;
  lives_clip_t *sfile = NULL;
  char *cwd;
  double tfps;
  boolean is_bg = TRUE, needs_reinit = FALSE, reinited = FALSE;
  lives_filter_error_t retval;
  int num_in_alpha = 0, clipno, i;

  if (!layer) return FILTER_ERROR_MISSING_LAYER;
  if (!inst) {
    return FILTER_ERROR_INVALID_INSTANCE;
  }

  clipno = lives_layer_get_clip(layer);
  sfile = RETURN_VALID_CLIP(clipno);
  if (!sfile) return FILTER_ERROR_INVALID_INSTANCE;

  filter = weed_instance_get_filter(inst, FALSE);

  mainw->inst_fps = get_inst_fps(FALSE);
  if (!prefs->genq_mode) {
    // prefer speed
    tfps = mainw->inst_fps;
    if (abs(sfile->pb_fps) > tfps) tfps = abs(sfile->pb_fps);
    tfps += weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, NULL);
    tfps /= 2.;
  } else {
    tfps = mainw->inst_fps;
    if (abs(sfile->pb_fps) < tfps) tfps = abs(sfile->pb_fps);
    tfps += weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, NULL);
    tfps /= 2.;
  }

  weed_set_double_value(inst, WEED_LEAF_FPS, mainw->inst_fps);
  weed_set_double_value(inst, WEED_LEAF_TARGET_FPS, tfps);

matchvals:

  channel = get_enabled_channel(inst, 0, LIVES_OUTPUT);
  if (!channel) return FILTER_ERROR_MISSING_CHANNEL;

  if (!is_bg) {
    if (!get_primary_src(mainw->current_file))
      add_primary_inst(mainw->current_file, (void *)filter, (void *)inst, LIVES_SRC_TYPE_GENERATOR);
  } else {
    if (!get_primary_src(mainw->blend_file))
      add_primary_inst(mainw->blend_file, (void *)filter, (void *)inst, LIVES_SRC_TYPE_GENERATOR);
  }

  if (weed_plant_has_leaf(inst, WEED_LEAF_IN_CHANNELS)) {
    int num_inc;
    weed_plant_t **in_channels = weed_instance_get_in_channels(inst, &num_inc);
    if (!in_channels) return FILTER_ERROR_TEMPLATE_MISMATCH;

    for (i = 0; i < num_inc; i++) {
      if (weed_channel_is_alpha(in_channels[i]) && !weed_channel_is_disabled(in_channels[i]))
        num_in_alpha++;
    }
    lives_freep((void **)&in_channels);
  }

  if (num_in_alpha > 0) {
    // if we have mandatory alpha ins, make sure they are filled
    retval = check_cconx(inst, num_in_alpha, &needs_reinit);
    if (retval != FILTER_SUCCESS) return FILTER_ERROR_COPYING_FAILED;
  }

  if (prefs->apply_gamma) {
    int flags = weed_filter_get_flags(filter);
    if (flags & WEED_FILTER_PREF_LINEAR_GAMMA) weed_channel_set_gamma_type(channel, WEED_GAMMA_LINEAR);
    else weed_channel_set_gamma_type(layer, WEED_GAMMA_SRGB);
  }

  if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
    int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
    if (rowstride_alignment_hint > THREADVAR(rowstride_alignment)) {
      THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
    }
  }

  if (!weed_channel_get_pixel_data(channel)) {
    if (!create_empty_pixel_data(channel, TRUE, TRUE)) {
      g_print("NO PIXDATA\n");
      return FILTER_ERROR_MEMORY_ERROR;
    }
    if (!weed_channel_get_pixel_data(channel))
      lives_abort("Unable to allocate channel pixel_data");
  }

  if (needs_reinit) {
    g_print("RENITT\n");
    weed_layer_pixel_data_free(channel);
    needs_reinit = FALSE;
    // could recalculate palette costs here

    retval = weed_reinit_effect(inst, FALSE);
    if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
        || retval == FILTER_ERROR_INVALID_FILTER) {
      weed_layer_pixel_data_free(channel);
      return retval;
    }
    goto matchvals;
  }

  // if we have an optional audio channel, we can push audio to it
  if ((achan = get_enabled_audio_channel(inst, 0, LIVES_INPUT)) != NULL) {
    fill_audio_channel(filter, achan, TRUE);
  }

  cwd = cd_to_plugin_dir(filter);

procfunc1:

  // the timecode we get in the parameter will be the queued or "reference" time for the layer
  // for realtime playback, better to use current tc
  if (!mainw->preview && (!mainw->event_list || mainw->record || mainw->record_paused)) {
    tc = mainw->currticks;
    weed_set_int64_value(layer, WEED_LEAF_HOST_TC, tc);
  }

  // get the current video data, then we will push an audio packet for the following frame
  retval = run_process_func(inst, tc);

  if (achan) {
    int nachans;
    float **abuf = weed_channel_get_audio_data(achan, &nachans);
    for (i = 0; i < nachans; i++) lives_freep((void **)&abuf[i]);
    lives_freep((void **)&abuf);
    weed_channel_set_audio_data(achan, NULL, 0, 0, 0);
  }

  if (retval == WEED_ERROR_REINIT_NEEDED) {
    if (reinited) {
      weed_layer_pixel_data_free(channel);
      return  FILTER_ERROR_COULD_NOT_REINIT;
    }
    retval = weed_reinit_effect(inst, FALSE);
    if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN
        || retval == FILTER_ERROR_INVALID_FILTER) {
      weed_layer_pixel_data_free(channel);
      return retval;
    }
    reinited = TRUE;
    goto procfunc1;
  }

  lives_chdir(cwd, FALSE);
  lives_free(cwd);

  if (retval == FILTER_ERROR_BUSY) return retval;

  inter = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  weed_layer_copy(inter, layer);
  weed_pixel_data_share(layer, channel);
  weed_pixel_data_share(channel, inter);
  weed_layer_free(inter);
  lives_layer_set_clip(layer, clipno);

  /* g_print("get from gen done %d %d %d %p\n", weed_channel_get_width(channel), weed_channel_get_height(channel), */
  /* 	  weed_channel_get_palette(channel), weed_channel_get_pixel_data(channel)); */
  return FILTER_SUCCESS;
}


int weed_generator_start(weed_plant_t *inst, int key) {
  // key here is zero based
  // make an "ephemeral clip"

  // cf. yuv4mpeg.c
  // start "playing" but receive frames from a plugin

  // RETURNS: 0 on success, or error code. -1 if playback started.

  weed_plant_t **out_channels, *channel, *filter, *achan;
  char *filter_name;
  int error, num_channels;
  int old_file = mainw->current_file;
  int palette;

  // create a virtual clip
  int new_file = 0;
  boolean is_bg = FALSE;

  if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR && mainw->num_tr_applied == 0) {
    if (mainw->noswitch) {
      mainw->close_this_clip = mainw->current_file;
    } else close_current_file(0);
    old_file = -1;
  }

  if (mainw->is_processing && !mainw->preview) {
    return 1;
  }

  if ((mainw->preview || (CURRENT_CLIP_IS_VALID && cfile->opening)) &&
      (mainw->num_tr_applied == 0 || mainw->blend_file == -1 || mainw->blend_file == mainw->current_file)) {
    return 2;
  }

  if (!LIVES_IS_PLAYING) mainw->pre_src_file = mainw->current_file;

  if (old_file != -1 && mainw->blend_file != -1 && mainw->blend_file != mainw->current_file &&
      mainw->num_tr_applied > 0 && IS_VALID_CLIP(mainw->blend_file) &&
      mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
    if (bg_gen_to_start == -1) {
      ////////////////////////// switching background generator: stop the old one first
      weed_generator_end((weed_instance_t *)get_primary_inst(mainw->files[mainw->blend_file]));
      mainw->new_clip = mainw->blend_file;
    }
  }

  // old_file can also be -1 if we are doing a fg_modeswitch
  if (old_file > -1 && LIVES_IS_PLAYING && mainw->num_tr_applied > 0) is_bg = TRUE;

  filter = weed_instance_get_filter(inst, FALSE);
  filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, &error);

  if (CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_HAS_VIDEO) {
    // audio clip - we will init the generator as fg video in the same clip
    // otherwise known as "showoff mode"
    cfile->frames = 1;
    cfile->start = cfile->end = 1;
    cfile->clip_type = CLIP_TYPE_GENERATOR;
    cfile->frameno = 1;
    add_primary_inst(mainw->current_file, (void *)filter, (void *)inst, LIVES_SRC_TYPE_GENERATOR);
    mainw->play_start = mainw->play_end = 1;
    mainw->startticks = mainw->currticks;
    is_bg = FALSE;
  } else {
    if (!create_cfile(-1, filter_name, TRUE)) return 3;

    cfile->clip_type = CLIP_TYPE_GENERATOR;

    lives_snprintf(cfile->type, 40, "generator:%s", filter_name);
    lives_snprintf(cfile->file_name, PATH_MAX, "generator: %s", filter_name);
    lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "generator: %s", filter_name);
    lives_free(filter_name);
    add_primary_inst(mainw->current_file, (void *)filter, (void *)inst, LIVES_SRC_TYPE_GENERATOR);

    // open as a clip with 1 frame
    cfile->start = cfile->end = cfile->frames = 1;
  }

  new_file = mainw->current_file;

  if (is_bg) {
    if (mainw->blend_file != mainw->current_file) {
      mainw->new_blend_file = mainw->current_file;
    }
    if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) ce_thumbs_highlight_current_clip();
  }

  if (!is_bg || old_file == -1 || old_file == new_file) fg_generator_clip = new_file;

  if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS))
    cfile->pb_fps = cfile->fps = weed_get_double_value(filter, WEED_LEAF_HOST_FPS, &error);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS))
    cfile->pb_fps = cfile->fps = weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, &error);
  else {
    cfile->pb_fps = cfile->fps = prefs->default_fps;
  }

  out_channels = weed_instance_get_out_channels(inst, &num_channels);
  if (num_channels == 0) {
    close_current_file(mainw->pre_src_file);
    return 4;
  }

  if (!(channel = get_enabled_channel(inst, 0, LIVES_OUTPUT))) {
    lives_free(out_channels);
    close_current_file(mainw->pre_src_file);
    return 5;
  }
  lives_free(out_channels);

  cfile->hsize = weed_get_int_value(channel, WEED_LEAF_WIDTH, &error);
  cfile->vsize = weed_get_int_value(channel, WEED_LEAF_HEIGHT, &error);

  palette = weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, &error);
  if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_ARGB32 || palette == WEED_PALETTE_BGRA32) cfile->bpp = 32;
  else cfile->bpp = 24;

  cfile->opening = FALSE;
  mainw->proc_ptr = NULL;

  if (prefs->push_audio_to_gens) {
    if ((achan = get_audio_channel_in(inst, 0)) != NULL) {
      if (weed_plant_has_leaf(achan, WEED_LEAF_DISABLED)) weed_leaf_delete(achan, WEED_LEAF_DISABLED);
      register_audio_client(TRUE);
    }
  }

  // if not playing, start playing
  if (!LIVES_IS_PLAYING) {
    uint64_t new_rte;

    if (!is_bg || old_file == -1 || old_file == new_file) {
      switch_to_file((mainw->current_file = old_file), new_file);
      set_main_title(cfile->file_name, 0);
      mainw->play_start = 1;
      mainw->play_end = INT_MAX;
      if (is_bg) {
        if (mainw->blend_file != mainw->current_file) {
          //track_source_free(1, mainw->blend_file);
          mainw->new_blend_file = mainw->blend_file = mainw->current_file;
        }
        if (old_file != -1) mainw->current_file = old_file;
      }
    } else {
      if (mainw->blend_file != mainw->current_file) {
        track_source_free(1, mainw->blend_file);
        mainw->new_blend_file = mainw->blend_file = mainw->current_file;
      }
      mainw->current_file = old_file;
      mainw->play_start = cfile->start;
      mainw->play_end = cfile->end;
      mainw->playing_sel = FALSE;
    }

    new_rte = GU641 << key;
    mainw->rte |= new_rte;
    mainw->rte_real |= new_rte;

    mainw->last_grabbable_effect = key;
    if (rte_window_visible()) rtew_set_keych(key, TRUE);
    if (mainw->ce_thumbs) {
      ce_thumbs_set_keych(key, TRUE);

      // if effect was auto (from ACTIVATE data connection), leave all param boxes
      // otherwise, remove any which are not "pinned"
      if (!THREADVAR(fx_is_auto)) ce_thumbs_add_param_box(key, TRUE);
    }
    if (mainw->play_window) {
      lives_widget_queue_draw(mainw->play_window);
    }

    start_playback_async(6);
    return -1;
  } else {
    // already playing
    if (old_file != -1 && mainw->files[old_file]) {
      if (IS_NORMAL_CLIP(old_file)) mainw->pre_src_file = old_file;
      mainw->current_file = old_file;
    }

    if (!is_bg || old_file == -1 || old_file == new_file) {
      if (new_file != old_file) {
        mainw->new_clip = new_file;
        if (!is_bg && IS_VALID_CLIP(mainw->blend_file)) {
          mainw->new_blend_file = mainw->blend_file;
          if (!IS_VALID_CLIP(mainw->new_blend_file)) {
            //weed_layer_set_invalid(mainw->blend_layer, TRUE);
            mainw->new_blend_file = -1;
          }
        }
      } else {
        lives_widget_show_all(mainw->playframe);
        resize(1);
        lives_widget_set_opacity(mainw->playframe, 1.);
      }
    } else {
      if (IS_VALID_CLIP(new_file)) {
        if (mainw->blend_file != new_file) {
          //          weed_layer_set_invalid(mainw->blend_layer, TRUE);
          mainw->new_blend_file = new_file;
        }
        if (mainw->ce_thumbs && (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND
                                 || mainw->active_sa_clips == SCREEN_AREA_FOREGROUND))
          ce_thumbs_highlight_current_clip();
      }
    }
    if (mainw->cancelled == CANCEL_GENERATOR_END) mainw->cancelled = CANCEL_NONE;
  }

  return 0;
}


void wge_inner(weed_plant_t *inst) {
  weed_plant_t *next_inst = NULL;
  // called from weed_generator_end()

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
    int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
    filter_mutex_lock(key);
    key_to_instance[key][key_modes[key]] = NULL;
    filter_mutex_unlock(key);
  }

  while (inst) {
    next_inst = get_next_compound_inst(inst);
    weed_call_deinit_func(inst);
    weed_instance_unref(inst);
    inst = next_inst;
  }
}


void weed_generator_end(weed_plant_t *inst) {
  // generator has stopped for one of the following reasons:
  // effect was de-inited; clip (bg/fg) was changed; playback stopped with fg

  // when ending a generator we need to do all of the following
  //  - invalidate any layer attached to primary_src
  //  - set clip primary_src->source to NULL but NOT primary_src itself yet
  //  - unref the instance
  //  - set close_this_clip
  //
  // - closing the clip_via close_this_clip()
  RECURSE_GUARD_START;
  boolean is_bg = FALSE;
  boolean clip_switched = mainw->clip_switched;
  boolean playing_sel = mainw->playing_sel;
  int current_file = mainw->current_file;

  RETURN_IF_RECURSED;

  weed_instance_ref(inst);

  if (!inst) {
    LIVES_WARN("inst was NULL !");
  }

  if (prefs->push_audio_to_gens) {
    if (inst && get_audio_channel_in(inst, 0)) {
      unregister_audio_client(TRUE);
    }
  }

  if (IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != current_file &&
      get_primary_inst(mainw->files[mainw->blend_file]) == (void *)inst) is_bg = TRUE;
  else mainw->new_blend_file = mainw->blend_file;

  if ((!is_bg && fg_generator_key == -1) || (is_bg && bg_generator_key == -1)) return;

  if (LIVES_IS_PLAYING && !is_bg && mainw->whentostop == STOP_ON_VID_END) {
    // we will close the file after playback stops
    // and also unref the instance
    mainw->cancelled = CANCEL_GENERATOR_END;
    return;
  }

  /////// update the key checkboxes //////
  /// TODO: do we want to do this if switching from one gen to another ?
  if (rte_window_visible() && !mainw->is_rendering && !mainw->multitrack) {
    // update real time effects window if we are showing it
    if (!is_bg) {
      rtew_set_keych(fg_generator_key, FALSE);
    } else {
      rtew_set_keych(bg_generator_key, FALSE);
    }
  }

  if (mainw->ce_thumbs) {
    // update ce_thumbs window if we are showing it
    if (!is_bg) {
      ce_thumbs_set_keych(fg_generator_key, FALSE);
    } else {
      ce_thumbs_set_keych(bg_generator_key, FALSE);
    }
  }

  if (is_bg) {
    if (mainw->blend_file != mainw->current_file) {
      // do not remove if playback ended
      if (LIVES_IS_PLAYING || mainw->blend_file == -1) {
        //bg_generator_key = bg_generator_mode = -1;
        RECURSE_GUARD_ARM;
        remove_primary_src(mainw->blend_file, LIVES_SRC_TYPE_GENERATOR);
        RECURSE_GUARD_END;
      } else set_primary_inst(mainw->blend_file, NULL);
      bg_gen_to_start = -1;
    }

    filter_mutex_lock(bg_generator_key);
    key_to_instance[bg_generator_key][bg_generator_mode] = NULL;
    if (rte_key_is_enabled(bg_generator_key, FALSE)) {
      mainw->rte &= ~(GU641 << bg_generator_key);
      mainw->rte_real &= ~(GU641 << bg_generator_key);
    }
    filter_mutex_unlock(bg_generator_key);
    mainw->pre_src_file = mainw->current_file;
    bg_generator_key = bg_generator_mode = -1;
  } else {
    filter_mutex_lock(fg_generator_key);
    if (rte_key_is_enabled(fg_generator_key, FALSE)) {
      mainw->rte &= ~(GU641 << fg_generator_key);
      mainw->rte_real &= ~(GU641 << fg_generator_key);
    }
    key_to_instance[fg_generator_key][fg_generator_mode] = NULL;
    filter_mutex_unlock(fg_generator_key);
    fg_gen_to_start = fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
    if (mainw->blend_file == mainw->current_file) {
      if (mainw->noswitch) {
        //weed_layer_set_invalid(mainw->blend_layer, TRUE);
        mainw->new_blend_file = mainw->current_file;
      } else {
        //weed_layer_set_invalid(mainw->blend_layer, TRUE);
        mainw->new_blend_file = -1;
      }
    }
  }

  if (inst) wge_inner(inst); // unref inst + compound parts

  // if the param window is already open, show any reinits now
  if (fx_dialog[1]) {
    if (is_bg) {
      if (fx_dialog[1]->key == bg_generator_key && fx_dialog[1]->mode == bg_generator_mode)
        update_widget_vis(NULL, bg_generator_key, bg_generator_mode); // redraw our paramwindow
    } else {
      if (fx_dialog[1]->key == fg_generator_key && fx_dialog[1]->mode == fg_generator_mode)
        update_widget_vis(NULL, fg_generator_key, fg_generator_mode); // redraw our paramwindow
    }
  }

  if (!is_bg && cfile->achans > 0 && cfile->clip_type == CLIP_TYPE_GENERATOR) {
    // we started playing from an audio clip
    cfile->frames = cfile->start = cfile->end = 0;

    RECURSE_GUARD_ARM;
    remove_primary_src(mainw->current_file, LIVES_SRC_TYPE_GENERATOR);
    RECURSE_GUARD_END;

    cfile->clip_type = CLIP_TYPE_DISK;
    cfile->hsize = cfile->vsize = 0;
    cfile->pb_fps = cfile->fps = prefs->default_fps;
    return;
  }

  if (is_bg) {
    if (!IS_VALID_CLIP(mainw->blend_file)
        || mainw->files[mainw->blend_file]->clip_type != CLIP_TYPE_GENERATOR) {
      BREAK_ME("close non-gen file");
      LIVES_WARN("Close non-generator file 2");
    } else {
      if (LIVES_IS_PLAYING) mainw->close_this_clip = mainw->blend_file;
      else {
        mainw->current_file = mainw->blend_file;
        close_current_file(current_file);
        mainw->blend_file = -1;
      }
    }
  } else {
    // close generator file and switch to original file if possible
    if (!cfile || cfile->clip_type != CLIP_TYPE_GENERATOR) {
      BREAK_ME("close non-gen file");
      LIVES_WARN("Close non-generator file 2");
    } else {
      if (cfile->achans == 0) {
        if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND)
          ce_thumbs_update_current_clip();
        if (LIVES_IS_PLAYING) {
          mainw->close_this_clip = mainw->current_file;
          mainw->new_clip = mainw->pre_src_file;
        } else {
          close_current_file(mainw->pre_src_file);
          if (mainw->current_file == current_file) {
            mainw->clip_switched = clip_switched;
            mainw->playing_sel = playing_sel;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (!CURRENT_CLIP_IS_VALID) mainw->cancelled = CANCEL_GENERATOR_END;
}


void weed_bg_generator_end(weed_plant_t *inst) {
  // when we stop with a bg generator we want it to be restarted next time
  // but we will need a new clip for it
  int bg_gen_key = bg_generator_key;
  /// ref the instance so it isn't deleted
  weed_instance_ref(inst);
  weed_generator_end(inst); // unrefs inst
  bg_gen_to_start = bg_gen_key;
}


boolean weed_playback_gen_start(void) {
  // init generators on pb. We have to do this after audio startup
  weed_plant_t *inst = NULL, *filter;
  weed_plant_t *next_inst = NULL, *orig_inst = NULL;

  char *filter_name;

  weed_error_t error = WEED_SUCCESS;

  int bgs = bg_gen_to_start;
  boolean was_started = FALSE;

  if (mainw->is_rendering) return TRUE;

  if (fg_gen_to_start == bg_gen_to_start) bg_gen_to_start = -1;

  if (cfile->frames == 0 && fg_gen_to_start == -1 && bg_gen_to_start != -1) {
    fg_gen_to_start = bg_gen_to_start;
    bgs = -1;
  }

  if (fg_gen_to_start != -1) {
    // check is still gen
    if (enabled_in_channels(weed_filters[key_to_fx[fg_gen_to_start][key_modes[fg_gen_to_start]]], FALSE) == 0) {
      orig_inst = inst = weed_instance_obtain(fg_gen_to_start, key_modes[fg_gen_to_start]);
      // have 1 ref
      if (inst) {
geninit1:
        error = weed_call_init_func(inst);

        if (error != WEED_SUCCESS) {
          filter_mutex_lock(fg_gen_to_start);
          if (key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]] == inst)
            key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]] = NULL;
          filter_mutex_unlock(fg_gen_to_start);
          filter = weed_instance_get_filter(inst, TRUE);
          filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
          d_print(_("Failed to start generator %s (%s)\n"), filter_name, weed_strerror(error));
          lives_free(filter_name);

deinit4:
          next_inst = get_next_compound_inst(inst);
          error = weed_call_deinit_func(inst);
          weed_instance_unref(inst);

          while (next_inst) {
            inst = next_inst;
            next_inst = get_next_compound_inst(inst);
            if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_TRUE) goto deinit4;
            else weed_instance_unref(inst);
          }

          fg_gen_to_start = -1;
          remove_primary_src(mainw->current_file, LIVES_SRC_TYPE_GENERATOR);
          return FALSE;
        }

        next_inst = get_next_compound_inst(inst);
        if (inst != orig_inst) weed_instance_unref(inst);

        if (next_inst) {
          inst = next_inst;
          weed_instance_ref(inst);
          goto geninit1;
        }

        inst = orig_inst;
        filter = weed_instance_get_filter(inst, TRUE);

        if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS)) {
          lives_clip_t *sfile = RETURN_VALID_CLIP(fg_generator_clip);
          if (sfile) {
            sfile->fps = weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, NULL);
            set_main_title(sfile->file_name, 0);
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), sfile->fps);
          }
        }

        mainw->clip_switched = TRUE;
        mainw->playing_sel = FALSE;
        add_primary_inst(mainw->current_file, (void *)filter, (void *)inst, LIVES_SRC_TYPE_GENERATOR);
        weed_instance_unref(inst);
      }
    }

    fg_gen_to_start = -1;
  }

  inst = NULL;

  if (bgs != -1) {
    lives_filter_error_t filt_error = FILTER_SUCCESS;
    filter_mutex_lock(bgs);
    // check is still gen
    if (mainw->num_tr_applied > 0
        && enabled_in_channels(weed_filters[key_to_fx[bgs][key_modes[bgs]]],
                               FALSE) == 0) {
      if ((inst = weed_instance_obtain(bgs, key_modes[bgs])) == NULL) {
        // restart bg generator

        if (!weed_init_effect(bgs)) filt_error = FILTER_ERROR_INVALID_INSTANCE;
        was_started = TRUE;
        mainw->blend_file = mainw->new_blend_file;
      }

      if (!inst) inst = weed_instance_obtain(bgs, key_modes[bgs]);

      orig_inst = inst;

      /* if (!inst) { */
      /*   // 2nd playback */
      /*   int playing_file = mainw->playing_file; */
      /*   if (!weed_init_effect(bgs)) error = WEED_ERROR_NOT_READY; */
      /*   mainw->playing_file = playing_file; */
      /* 	if (error == WEED_SUCCESS) { */
      /* 	  orig_inst = weed_instance_obtain(bgs, key_modes[bgs]); */
      /* 	  mainw->blend_file = mainw->new_blend_file; */
      /* 	} */
      /* } */
      if (1) {
        if (!was_started) {
genstart2:

          // TODO - error check
          weed_call_init_func(inst);

          // handle compound fx
          next_inst = get_next_compound_inst(inst);
          if (next_inst) {
            inst = next_inst;
            weed_instance_ref(inst);
            goto genstart2;
          }
        }
      }

      inst = orig_inst;

      if (filt_error != FILTER_SUCCESS) {
undoit:
        mainw->new_blend_file = mainw->blend_file;

        if (key_to_instance[bgs][key_modes[bgs]] == inst) {
          key_to_instance[bgs][key_modes[bgs]] = NULL;
          if (rte_key_is_enabled(ABS(bgs), FALSE)) {
            mainw->rte &= ~(GU641 << ABS(bgs));
            mainw->rte_real &= ~(GU641 << ABS(bgs));
          }
        }
        filter_mutex_unlock(bgs);
        if (inst) {
          filter = weed_instance_get_filter(inst, TRUE);
          filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
          d_print(_("Failed to start generator %s, (%s)\n"), filter_name, weed_strerror(error));
          lives_free(filter_name);

deinit5:

          next_inst = get_next_compound_inst(inst);
          error = weed_call_deinit_func(inst);
          weed_instance_unref(inst);
          weed_instance_unref(inst);

          if (next_inst) {
            // handle compound fx
            inst = next_inst;
            weed_instance_ref(inst);
            goto deinit5;
          }
        }

        bg_gen_to_start = -1;
        BREAK_ME("retfalse");
        return FALSE;
      }

      filter_mutex_unlock(bgs);

      // re-reandomise
      weed_set_int64_value(inst, WEED_LEAF_RANDOM_SEED, gen_unique_id());

      if (!IS_VALID_CLIP(mainw->blend_file)
          || (mainw->files[mainw->blend_file]->frames > 0
              && mainw->files[mainw->blend_file]->clip_type != CLIP_TYPE_GENERATOR)) {
        int current_file = mainw->current_file;

        filter = weed_instance_get_filter(inst, TRUE);
        filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
        if (!create_cfile(-1, filter_name, TRUE)) {
          mainw->current_file = current_file;
          goto undoit;
        }

        cfile->clip_type = CLIP_TYPE_GENERATOR;

        lives_snprintf(cfile->type, 40, "generator:%s", filter_name);
        lives_snprintf(cfile->file_name, PATH_MAX, "generator: %s", filter_name);
        lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "generator: %s", filter_name);
        lives_free(filter_name);

        // open as a clip with 1 frame
        cfile->start = cfile->end = cfile->frames = 1;
        mainw->new_blend_file = mainw->blend_file = mainw->current_file;
        mainw->current_file = current_file;
      }

      filter = weed_instance_get_filter(orig_inst, TRUE);
      add_primary_inst(mainw->new_blend_file, (void *)filter, (void *)orig_inst, LIVES_SRC_TYPE_GENERATOR);
    } else filter_mutex_unlock(bgs);
  }

  // must do this last, else the code will try to create a new clip for the bg generator
  // intead of reusing the current one
  bg_gen_to_start = -1;

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// weed parameter functions

/// returns the permanent (structural) state
/// c.f check_hidden_gui() which sets flag values for params linked to an rfx extension
boolean is_hidden_param(weed_plant_t *plant, int i) {
  // find out if in_param i is visible or not for plant. Plant can be an instance or a filter
  weed_plant_t **wtmpls;
  weed_plant_t *filter, *pgui = NULL;
  weed_plant_t *wtmpl;
  boolean visible = TRUE;
  int num_params = 0;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) {
    weed_plant_t *param = weed_inst_in_param(plant, i, FALSE, FALSE);
    if (param) {
      if (weed_param_is_hidden(param, WEED_FALSE) == WEED_TRUE) {
        return TRUE;
      }
      pgui = weed_param_get_gui(param, FALSE);
    }
    filter = weed_instance_get_filter(plant, TRUE);
  } else filter = plant;

  wtmpls = weed_filter_get_in_paramtmpls(filter, &num_params);

  if (i >= num_params) {
    if (wtmpls) lives_free(wtmpls);
    return TRUE;
  }

  wtmpl = wtmpls[i];
  if (!wtmpl) {
    lives_free(wtmpls);
    return TRUE;
  }

  // hide internally connected parameters for compound fx
  if (weed_plant_has_leaf(wtmpl, WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
    lives_free(wtmpls);
    return TRUE;
  }

  /// if we are to copy the values to another param, make sure it's possible (type, num values)
  if ((weed_plant_has_leaf(wtmpl, WEED_LEAF_COPY_VALUE_TO))
      || (pgui && weed_plant_has_leaf(pgui, WEED_LEAF_COPY_VALUE_TO))) {
    int copyto = -1;
    int flags2 = 0, param_type, param_type2;
    weed_plant_t *wtmpl2;
    if (weed_plant_has_leaf(wtmpl, WEED_LEAF_COPY_VALUE_TO))
      copyto = weed_get_int_value(wtmpl, WEED_LEAF_COPY_VALUE_TO, NULL);
    if (pgui && weed_plant_has_leaf(pgui, WEED_LEAF_COPY_VALUE_TO))
      copyto = weed_get_int_value(pgui, WEED_LEAF_COPY_VALUE_TO, NULL);
    if (copyto == i || copyto < 0) copyto = -1;
    if (copyto > -1) {
      visible = FALSE;
      wtmpl2 = wtmpls[copyto];
      flags2 = weed_paramtmpl_get_flags(wtmpl2);
      param_type = weed_paramtmpl_get_type(wtmpl);
      param_type2 = weed_paramtmpl_get_type(wtmpl2);
      if (param_type == param_type2
          && ((flags2 & WEED_PARAMETER_VARIABLE_SIZE)
              || (flags2 & WEED_PARAMETER_VALUE_PER_CHANNEL)
              || weed_leaf_num_elements(wtmpl, WEED_LEAF_DEFAULT)
              == weed_leaf_num_elements(wtmpl2, WEED_LEAF_DEFAULT))) {
        visible = TRUE;
      }
    }
  }
  lives_free(wtmpls);
  return !visible;
}


int get_transition_param(weed_plant_t *filter, boolean skip_internal) {
  int num_params, count = 0;
  weed_plant_t **in_ptmpls = weed_filter_get_in_paramtmpls(filter, &num_params);
  if (num_params == 0) return -1;
  for (int i = 0; i < num_params; i++) {
    if (skip_internal && weed_plant_has_leaf(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
    if (weed_get_boolean_value(in_ptmpls[i], WEED_LEAF_IS_TRANSITION, NULL) == WEED_TRUE) {
      lives_free(in_ptmpls);
      return count;
    }
    count++;
  }
  lives_free(in_ptmpls);
  return -1;
}


int get_master_vol_param(weed_plant_t *filter, boolean skip_internal) {
  int error, num_params, i, count = 0;
  weed_plant_t **in_ptmpls;

  in_ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
  if (num_params == 0) return -1;
  for (i = 0; i < num_params; i++) {
    if (skip_internal && weed_plant_has_leaf(in_ptmpls[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
    if (weed_plant_has_leaf(in_ptmpls[i], WEED_LEAF_IS_VOLUME_MASTER) &&
        weed_get_boolean_value(in_ptmpls[i], WEED_LEAF_IS_VOLUME_MASTER, &error) == WEED_TRUE) {
      lives_free(in_ptmpls);
      return count;
    }
    count++;
  }
  lives_free(in_ptmpls);
  return -1;
}


boolean is_perchannel_multiw(weed_plant_t *param) {
  // updated for weed spec 1.1
  int error;
  int flags = 0;
  weed_plant_t *ptmpl;
  if (WEED_PLANT_IS_PARAMETER(param)) ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);
  else ptmpl = param;
  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_FLAGS)) flags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, &error);
  if (flags & WEED_PARAMETER_VALUE_PER_CHANNEL) return TRUE;
  return FALSE;
}


boolean has_perchannel_multiw(weed_plant_t *filter) {
  int nptmpl, i;
  weed_plant_t **ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nptmpl);

  if (nptmpl == 0) return FALSE;

  for (i = 0; i < nptmpl; i++) {
    if (is_perchannel_multiw(ptmpls[i])) {
      lives_free(ptmpls);
      return TRUE;
    }
  }

  lives_free(ptmpls);
  return FALSE;
}


weed_plant_t *weed_inst_in_param(weed_plant_t *inst, int param_num, boolean skip_hidden, boolean skip_internal) {
  weed_plant_t **in_params;
  weed_plant_t *param;
  int num_params;

  do {
    in_params = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_PARAMETERS, &num_params);
    if (!num_params) continue; // has no in_parameters
    if (!skip_hidden && !skip_internal) {
      if (num_params > param_num) {
        param = in_params[param_num];
        lives_free(in_params);
        return param;
      }
      param_num -= num_params;
    } else {
      int count = 0;
      for (int i = 0; i < num_params; i++) {
        param = in_params[i];
        if ((!skip_hidden || !is_hidden_param(inst, i)) && (!skip_internal
            || !weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION))) {
          if (count == param_num) {
            lives_free(in_params);
            return param;
          }
          count++;
        }
      }
      param_num -= count;
      lives_free(in_params);
    }
  } while ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)) != NULL);

  return NULL;
}


weed_plant_t *weed_inst_out_param(weed_plant_t *inst, int param_num) {
  weed_plant_t **out_params;
  weed_plant_t *param;
  int num_params;

  do {
    out_params = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_PARAMETERS, &num_params);
    if (!num_params) continue; // has no out_parameters
    if (num_params > param_num) {
      param = out_params[param_num];
      lives_free(out_params);
      return param;
    }
    param_num -= num_params;
  } while ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)) != NULL);

  return NULL;
}


weed_plant_t *weed_filter_in_paramtmpl(weed_plant_t *filter, int param_num, boolean skip_internal) {
  weed_plant_t **in_params;
  weed_plant_t *ptmpl;
  int num_params;
  int count = 0;

  in_params = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
  if (num_params <= param_num) {
    lives_freep((void **)&in_params);
    return NULL; // invalid parameter number
  }

  if (!skip_internal) {
    ptmpl = in_params[param_num];
    lives_free(in_params);
    return ptmpl;
  }

  for (int i = 0; i < num_params; i++) {
    ptmpl = in_params[i];
    if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
      if (count == param_num) {
        lives_free(in_params);
        return ptmpl;
      }
      count++;
    }
  }

  lives_free(in_params);
  return NULL;
}


weed_plant_t *weed_filter_out_paramtmpl(weed_plant_t *filter, int param_num) {
  weed_plant_t **out_params;
  weed_plant_t *ptmpl;
  int num_params;

  out_params = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &num_params);
  if (num_params <= param_num) {
    lives_freep((void **)&out_params);
    return NULL; // invalid parameter number
  }

  ptmpl = out_params[param_num];
  lives_free(out_params);
  return ptmpl;
}


int get_nth_simple_param(weed_plant_t *plant, int pnum) {
  // return the number of the nth "simple" parameter
  // we define "simple" as - must be single valued int or float, must not be hidden

  // -1 is returned if no such parameter is found

  weed_plant_t **in_ptmpls;
  weed_plant_t *tparamtmpl;
  weed_plant_t *gui;
  int i, ptype, flags, nparams;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) plant = weed_instance_get_filter(plant, TRUE);

  if (!weed_plant_has_leaf(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES)) return -1;

  in_ptmpls = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nparams);

  for (i = 0; i < nparams; i++) {
    tparamtmpl = in_ptmpls[i];
    gui = weed_paramtmpl_get_gui(tparamtmpl, FALSE);

    ptype = weed_paramtmpl_get_type(tparamtmpl);

    if (gui && ptype == WEED_PARAM_INTEGER && weed_plant_has_leaf(gui, WEED_LEAF_CHOICES)) continue;

    flags = weed_paramtmpl_get_flags(tparamtmpl);

    if ((ptype == WEED_PARAM_INTEGER || ptype == WEED_PARAM_FLOAT)
        && flags == 0 && weed_leaf_num_elements(tparamtmpl, WEED_LEAF_DEFAULT) == 1 &&
        !is_hidden_param(plant, i)) {
      if (pnum == 0) {
        lives_free(in_ptmpls);
        return i;
      }
      pnum--;
    }
  }
  lives_free(in_ptmpls);
  return -1;
}


int count_simple_params(weed_plant_t *plant) {
  int i, ptype, flags, nparams, count = 0;
  weed_plant_t **in_ptmpls;
  weed_plant_t *tparamtmpl;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) plant = weed_instance_get_filter(plant, TRUE);

  if (!weed_plant_has_leaf(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES)) return count;

  in_ptmpls = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES, &nparams);

  for (i = 0; i < nparams; i++) {
    tparamtmpl = in_ptmpls[i];
    ptype = weed_paramtmpl_get_type(tparamtmpl);
    flags = weed_paramtmpl_get_flags(tparamtmpl);
    if ((ptype == WEED_PARAM_INTEGER || ptype == WEED_PARAM_FLOAT)
        && flags == 0 && weed_leaf_num_elements(tparamtmpl, WEED_LEAF_DEFAULT) == 1 &&
        !is_hidden_param(plant, i)) {
      count++;
    }
  }
  lives_freep((void **)&in_ptmpls);
  return count;
}


int set_copy_to(weed_plant_t *inst, int pnum, lives_rfx_t *rfx, boolean update) {
  // if we update a plugin in_parameter, evaluate any "copy_value_to"
  weed_plant_t *paramtmpl;
  weed_plant_t *pgui, *in_param2;
  weed_plant_t *in_param = weed_inst_in_param(inst, pnum, FALSE, FALSE); // use this here in case of compound fx
  int copyto = -1;
  int param_type, param_type2;

  if (!in_param) return -1;

  pgui = weed_param_get_gui(in_param, FALSE);
  paramtmpl = weed_param_get_template(in_param);

  if (pgui && weed_plant_has_leaf(pgui, WEED_LEAF_COPY_VALUE_TO)) {
    copyto = weed_get_int_value(pgui, WEED_LEAF_COPY_VALUE_TO, NULL);
    if (copyto == pnum || copyto < 0) return -1;
  }

  if (copyto == -1 && weed_plant_has_leaf(paramtmpl, WEED_LEAF_COPY_VALUE_TO))
    copyto = weed_get_int_value(paramtmpl, WEED_LEAF_COPY_VALUE_TO, NULL);
  if (copyto == pnum || copyto < 0) return -1;

  if (rfx) {
    if (copyto >= rfx->num_params) return -1;
    if (rfx->params[copyto].change_blocked) return -1; ///< prevent loops
  }

  param_type = weed_param_get_type(in_param);
  in_param2 = weed_inst_in_param(inst, copyto, FALSE, FALSE); // use this here in case of compound fx
  if (!in_param2) return -1;
  if (weed_plant_has_leaf(in_param2, WEED_LEAF_HOST_INTERNAL_CONNECTION)) return -1;
  param_type2 = weed_param_get_type(in_param2);

  /// check for compatibility nvalues
  if (!(param_type == param_type2 && (!weed_param_has_variable_size(in_param) || weed_param_has_variable_size(in_param2))
        && (!weed_param_has_value_perchannel(in_param) || weed_param_has_variable_size(in_param2)
            || weed_param_has_value_perchannel(in_param2)))) return -1;

  if (update) {
    weed_plant_t *paramtmpl2 = weed_param_get_template(in_param2);
    int flags = weed_paramtmpl_get_flags(paramtmpl2);

    if (flags & WEED_PARAMETER_VALUE_PER_CHANNEL) {
      int *ign_array;
      int nvals = weed_leaf_num_elements(in_param2, WEED_LEAF_VALUE);
      int vsize = 1;
      if (param_type2 == WEED_PARAM_COLOR) {
        int cspace = weed_get_int_value(paramtmpl2, WEED_LEAF_COLORSPACE, NULL);
        if (cspace == WEED_COLORSPACE_RGB) vsize = 3;
        else vsize = 4;
      }
      lives_leaf_copy(paramtmpl2, "host_new_def_backup", paramtmpl2, WEED_LEAF_NEW_DEFAULT);
      lives_leaf_copy(in_param2, "host_value_backup", in_param2, WEED_LEAF_VALUE);
      lives_leaf_copy(paramtmpl2, WEED_LEAF_NEW_DEFAULT, in_param, WEED_LEAF_VALUE);
      fill_param_vals_to(in_param2, paramtmpl2, nvals / vsize);
      ign_array = weed_get_boolean_array(in_param2, WEED_LEAF_IGNORE, NULL);
      for (int i = 0; i < nvals; i += vsize) {
        if (!weed_leaf_elements_equate(in_param2, WEED_LEAF_VALUE, in_param2, "host_value_backup", i))
          ign_array[i] = WEED_FALSE;
        else
          ign_array[i] = WEED_TRUE;
      }
      weed_set_boolean_array(in_param2, WEED_LEAF_IGNORE, nvals / vsize, ign_array);
      weed_leaf_delete(in_param2, "host_value_backup");
      lives_leaf_copy(paramtmpl2, WEED_LEAF_NEW_DEFAULT, paramtmpl2, "host_new_def_backup");
      weed_leaf_delete(paramtmpl2, "host_new_def_backup");
      lives_freep((void **)&ign_array);
    } else {
      lives_leaf_dup(in_param2, in_param, WEED_LEAF_VALUE);
    }
  }
  return copyto;
}


void rec_param_change(weed_plant_t *inst, int pnum) {
  ticks_t actual_ticks;
  weed_plant_t *in_param;
  int key;

  weed_instance_ref(inst);

  // do not record changes for the floating fx dialog box (rte window params)
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)
      && weed_get_boolean_value(inst, WEED_LEAF_HOST_NORECORD, NULL)) {
    weed_instance_unref(inst);
    return;
  }

  // do not record changes for generators - those get recorded to scrap_file or ascrap_file
  // also ignore analysers
  if (!enabled_in_channels(inst, FALSE) || !enabled_out_channels(inst, FALSE)) {
    weed_instance_unref(inst);
    return;
  }

  //actual_ticks = mainw->clock_ticks;
  //lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
  actual_ticks = mainw->startticks; ///< use the "theoretical" time
  if (THREADVAR(fx_is_audio)) {
    // use real time when recording audio fx
    actual_ticks = THREADVAR(event_ticks);
  }

  key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);

  in_param = weed_inst_in_param(inst, pnum, FALSE, FALSE);

  if (!weed_param_value_irrelevant(in_param)) {
    pthread_mutex_lock(&mainw->event_list_mutex);
    mainw->event_list = append_param_change_event(mainw->event_list, actual_ticks,
                        pnum, in_param, init_events[key], NULL);//pchains[key]);
    if (THREADVAR(fx_is_audio)) {
      weed_event_t *event = get_last_event(mainw->event_list);
      weed_set_boolean_value(event, LIVES_LEAF_NOQUANT, WEED_TRUE);
    }
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }

  weed_instance_unref(inst);
}

#define KEYSCALE 255.


void weed_set_blend_factor(int hotkey) {
  weed_plant_t *inst, *in_param, *paramtmpl;

  LiVESList *list = NULL;

  weed_plant_t **in_params;

  double vald, mind, maxd;

  int vali, mini, maxi;
  int param_type, pnum, inc_count;

  if (hotkey < 0) return;

  filter_mutex_lock(hotkey);
  inst = weed_instance_obtain(hotkey, key_modes[hotkey]);
  filter_mutex_unlock(hotkey);

  if (!inst) return;

  pnum = get_nth_simple_param(inst, 0);

  if (pnum == -1)  {
    weed_instance_unref(inst);
    return;
  }

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  in_param = in_params[pnum];
  lives_freep((void **)&in_params);

  paramtmpl = weed_param_get_template(in_param);
  param_type = weed_paramtmpl_get_type(paramtmpl);

  inc_count = enabled_in_channels(inst, FALSE);

  if (weed_param_does_wrap(in_param)) {
    if (mainw->blend_factor >= 256.) mainw->blend_factor -= 256.;
    else if (mainw->blend_factor <= -1.) mainw->blend_factor += 256.;
  } else {
    if (mainw->blend_factor < 0.) mainw->blend_factor = 0.;
    else if (mainw->blend_factor > 255.) mainw->blend_factor = 255.;
  }

  switch (param_type) {
  case WEED_PARAM_INTEGER:
    vali = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);
    mini = weed_get_int_value(paramtmpl, WEED_LEAF_MIN, NULL);
    maxi = weed_get_int_value(paramtmpl, WEED_LEAF_MAX, NULL);

    weed_set_int_value(in_param, WEED_LEAF_VALUE, (int)((double)mini +
                       (mainw->blend_factor / KEYSCALE * (double)(maxi - mini)) + .5));

    vali = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);

    list = lives_list_append(list, lives_strdup_printf("%d", vali));
    list = lives_list_append(list, lives_strdup_printf("%d", mini));
    list = lives_list_append(list, lives_strdup_printf("%d", maxi));
    update_pwindow(hotkey, pnum, list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey, pnum, list);
    lives_list_free_all(&list);

    break;
  case WEED_PARAM_FLOAT:
    vald = weed_get_double_value(in_param, WEED_LEAF_VALUE, NULL);
    mind = weed_get_double_value(paramtmpl, WEED_LEAF_MIN, NULL);
    maxd = weed_get_double_value(paramtmpl, WEED_LEAF_MAX, NULL);

    weed_set_double_value(in_param, WEED_LEAF_VALUE,
                          mind + (mainw->blend_factor / KEYSCALE * (maxd - mind)));
    vald = weed_get_double_value(in_param, WEED_LEAF_VALUE, NULL);

    list = lives_list_append(list, lives_strdup_printf("%.4f", vald));
    list = lives_list_append(list, lives_strdup_printf("%.4f", mind));
    list = lives_list_append(list, lives_strdup_printf("%.4f", maxd));
    update_pwindow(hotkey, pnum, list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey, pnum, list);
    lives_list_free_all(&list);

    break;
  case WEED_PARAM_SWITCH:
    vali = !!(int)mainw->blend_factor;
    weed_set_boolean_value(in_param, WEED_LEAF_VALUE, vali);
    vali = weed_get_boolean_value(in_param, WEED_LEAF_VALUE, NULL);
    mainw->blend_factor = (double)vali;

    list = lives_list_append(list, lives_strdup_printf("%d", vali));
    update_pwindow(hotkey, pnum, list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey, pnum, list);
    lives_list_free_all(&list);

    break;
  default:
    break;
  }

  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING
      && (prefs->rec_opts & REC_EFFECTS) && inc_count > 0) {
    rec_param_change(inst, pnum);
  }
  weed_instance_unref(inst);
}


int weed_get_blend_factor(int hotkey) {
  // filter mutex MUST be locked

  weed_plant_t *inst, **in_params, *in_param, *paramtmpl;
  int vali, mini, maxi;
  double vald, mind, maxd;
  int weed_ptype;
  int i;

  if (hotkey < 0) return 0;
  inst = weed_instance_obtain(hotkey, key_modes[hotkey]);

  if (!inst) return 0;

  i = get_nth_simple_param(inst, 0);

  if (i == -1)  {
    weed_instance_unref(inst);
    return 0;
  }

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  in_param = in_params[i];

  paramtmpl = weed_param_get_template(in_param);
  weed_ptype = weed_paramtmpl_get_type(paramtmpl);

  switch (weed_ptype) {
  case WEED_PARAM_INTEGER:
    vali = weed_get_int_value(in_param, WEED_LEAF_VALUE, NULL);
    mini = weed_get_int_value(paramtmpl, WEED_LEAF_MIN, NULL);
    maxi = weed_get_int_value(paramtmpl, WEED_LEAF_MAX, NULL);
    lives_free(in_params);
    weed_instance_unref(inst);
    return (double)(vali - mini) / (double)(maxi - mini) * KEYSCALE;
  case WEED_PARAM_FLOAT:
    vald = weed_get_double_value(in_param, WEED_LEAF_VALUE, NULL);
    mind = weed_get_double_value(paramtmpl, WEED_LEAF_MIN, NULL);
    maxd = weed_get_double_value(paramtmpl, WEED_LEAF_MAX, NULL);
    lives_free(in_params);
    weed_instance_unref(inst);
    return (vald - mind) / (maxd - mind) * KEYSCALE;
  case WEED_PARAM_SWITCH:
    vali = weed_get_boolean_value(in_param, WEED_LEAF_VALUE, NULL);
    lives_free(in_params);
    weed_instance_unref(inst);
    return vali;
  default:
    lives_free(in_params);
    weed_instance_unref(inst);
  }

  return 0;
}


weed_plant_t *get_new_inst_for_keymode(int key, int mode)  {
  // returns REFCOUNTED inst
  // key is 0 based
  weed_plant_t *inst;
  if ((inst = weed_instance_obtain(key, mode)) != NULL) {
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_MODE)) {
      if (weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL) == key
          && weed_get_int_value(inst, WEED_LEAF_HOST_MODE, NULL) == mode) {
        return inst;
      }
    }
    weed_instance_unref(inst);
  }

  for (int i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    if ((inst = weed_instance_obtain(i, key_modes[i])) == NULL) {
      continue;
    }
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_MODE)) {
      if (weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL) == key
          && weed_get_int_value(inst, WEED_LEAF_HOST_MODE, NULL) == mode) {
        return inst;
      }
    }
    weed_instance_unref(inst);
  }

  return NULL;
}


////////////////////////////////////////////////////////////////////////

LIVES_INLINE char *weed_instance_get_type(weed_plant_t *inst, boolean getsub) {
  // return value should be free'd after use
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
  return weed_filter_get_type(filter, getsub, TRUE);
}


char *rte_keymode_get_type(int key, int mode) {
  // return value should be free'd after use
  char *type = lives_strdup("");
  weed_plant_t *filter, *inst;
  int idx;

  key--;
  if (!rte_keymode_valid(key + 1, mode, TRUE)) return type;

  if ((idx = key_to_fx[key][mode]) == -1) return type;
  if ((filter = weed_filters[idx]) == NULL) return type;

  lives_free(type);

  filter_mutex_lock(key);
  if ((inst = key_to_instance[key][mode]) != NULL) {
    weed_instance_ref(inst);
    filter_mutex_unlock(key);
    // return details for instance
    type = weed_instance_get_type(inst, TRUE);
    weed_instance_unref(inst);
  } else {
    filter_mutex_unlock(key);
    type = weed_filter_get_type(filter, TRUE, TRUE);
  }
  return type;
}


lives_fx_cat_t rte_keymode_get_category(int key, int mode) {
  weed_plant_t *filter;
  lives_fx_cat_t cat;
  int idx;

  if (!rte_keymode_valid(key, mode, TRUE)) return LIVES_FX_CAT_NONE;

  if ((idx = key_to_fx[--key][mode]) == -1) return LIVES_FX_CAT_NONE;
  if ((filter = weed_filters[idx]) == NULL) return LIVES_FX_CAT_NONE;

  else cat = weed_filter_categorise(filter, enabled_in_channels(filter, FALSE),
                                      enabled_out_channels(filter, FALSE));

  return cat;
}


///////////////////////////////////////////////////////////////////////////////

int get_next_free_key(void) {
  // 0 based
  int i, free_key;
  free_key = next_free_key;
  for (i = free_key + 1; i < FX_KEYS_MAX; i++) {
    if (key_to_fx[i][0] == -1) {
      next_free_key = i;
      break;
    }
  }
  if (i == FX_KEYS_MAX) {
    next_free_key = FX_KEYS_MAX_VIRTUAL;
    return -1;
  }
  return free_key;
}


boolean weed_delete_effectkey(int key, int mode) {
  // delete the effect binding for key/mode and move higher numbered slots down
  // also moves the active mode if applicable
  // returns FALSE if there was no effect bound to key/mode

  // any instance bound to key / mode should be deinited / freed first as appropriate
  int maxmode;

  if (key < FX_KEYS_MAX_VIRTUAL) filter_mutex_lock(key);

  if (key_to_fx[key][mode] == -1) {
    if (key < FX_KEYS_MAX_VIRTUAL) filter_mutex_unlock(key);
    return FALSE;
  }

  if (key < FX_KEYS_MAX_VIRTUAL) {
    if (key_modes[key] > mode) key_modes[key]--;
    free_key_defaults(key, mode);
    maxmode = rte_key_getmaxmode(key + 1);
    for (; mode < maxmode; mode++) {
      key_to_fx[key][mode] = key_to_fx[key][mode + 1];
      key_to_instance[key][mode] = key_to_instance[key][mode + 1];
      key_defaults[key][mode] = key_defaults[key][mode + 1];
    }
  } else if (key < next_free_key) next_free_key = key;

  if (key < FX_KEYS_MAX_VIRTUAL) {
    key_defaults[key][mode] = NULL;
  }
  key_to_fx[key][mode] = -1;
  key_to_instance[key][mode] = NULL;

  if (key < FX_KEYS_MAX_VIRTUAL) filter_mutex_unlock(key);

  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

boolean rte_key_valid(int key, boolean is_userkey) {
  // key is 1 based
  key--;

  if (key < 0 || (is_userkey && key >= FX_KEYS_MAX_VIRTUAL) || key >= FX_KEYS_MAX) return FALSE;
  if (key_to_fx[key][key_modes[key]] == -1) return FALSE;
  return TRUE;
}


boolean rte_keymode_valid(int key, int mode, boolean is_userkey) {
  // key is 1 based
  if (key < 1 || (is_userkey && key > FX_KEYS_MAX_VIRTUAL) || key > FX_KEYS_MAX || mode < 0 ||
      mode >= (key < FX_KEYS_MAX_VIRTUAL ? prefs->rte_modes_per_key : 1)) return FALSE;
  if (key_to_fx[--key][mode] == -1) return FALSE;
  return TRUE;
}


int rte_keymode_get_filter_idx(int key, int mode) {
  // key is 1 based
  if (key < 1 || key > FX_KEYS_MAX || mode < 0 ||
      mode >= (key < FX_KEYS_MAX_VIRTUAL ? prefs->rte_modes_per_key : 1)) return -1;
  return (key_to_fx[--key][mode]);
}


int rte_key_getmode(int key) {
  // get current active mode for an rte key
  // key is 1 based
  if (key < 1 || key > FX_KEYS_MAX) return -1;
  return key_modes[--key];
}


int rte_key_getmaxmode(int key) {
  // gets the highest mode with filter mapped for a key
  // or -1 if no modes are filled
  if (key < 1 || key > FX_KEYS_MAX) return -1;
  else {
    int maxmode = rte_key_num_modes(key);
    key--;
    for (int i = 0; i < maxmode; i++) {
      if (key_to_fx[key][i] == -1) return --i;
    }
    return --maxmode;
  }
}


int rte_key_num_modes(int key) {
  // gets the highest mode with or without filter mapped, for a key
  // (key is 1 based here)
  key--;
  if (key < 0 || key >= FX_KEYS_MAX) return 0;
  if (key < FX_KEYS_MAX_VIRTUAL) return prefs->rte_modes_per_key;
  return 1;
}


weed_plant_t *rte_keymode_get_instance(int key, int mode) {
  // returns REFCOUNTED inst
  weed_plant_t *inst;
  if (!rte_keymode_valid(key, mode, FALSE)) return NULL;
  if ((inst = weed_instance_obtain(--key, mode)) == NULL) {
    return NULL;
  }
  /* if (weed_get_boolean_value(inst, LIVES_LEAF_SOFT_DEINIT, NULL) == WEED_TRUE) { */
  /*   weed_instance_unref(inst); */
  /*   return NULL; */
  /* } */
  return inst;
}


weed_plant_t *rte_keymode_get_filter(int key, int mode) {
  // key is 1 based
  key--;
  if (!rte_keymode_valid(key + 1, mode, FALSE)) return NULL;
  return weed_filters[key_to_fx[key][mode]];
}


#define MAX_AUTHOR_LEN 10

char *weed_filter_idx_get_name(int idx, boolean add_subcats, boolean add_notes) {
  // return value should be free'd after use
  weed_plant_t *filter;
  char *filter_name, *tmp;

  if (idx == -1) return lives_strdup("");
  if ((filter = weed_filters[idx]) == NULL) return lives_strdup("");

  filter_name = weed_filter_get_name(filter);

  if (add_subcats) {
    lives_fx_cat_t cat = weed_filter_categorise(filter,
                         enabled_in_channels(filter, TRUE),
                         enabled_out_channels(filter, TRUE));
    lives_fx_cat_t sub = weed_filter_subcategorise(filter, cat, FALSE);
    if (sub != LIVES_FX_CAT_NONE) {
      tmp = lives_strdup_printf("%s (%s)", filter_name, lives_fx_cat_to_text(sub, FALSE));
      lives_free(filter_name);
      filter_name = tmp;
    }
  }

  if (add_notes) {
    // if it's unstable add that
    if (weed_filter_hints_unstable(filter)) {
      tmp = lives_strdup_printf(_("%s [unstable]"), filter_name);
      lives_free(filter_name);
      filter_name = tmp;
    }
  }

  return filter_name;
}


char *weed_filter_idx_get_package_name(int idx) {
  // return value should be free'd after use
  weed_plant_t *filter;
  if (idx == -1) return NULL;
  if (!(filter = weed_filters[idx])) return NULL;
  return weed_filter_get_package_name(filter);
}


char *weed_instance_get_filter_name(weed_plant_t *inst, boolean get_compound_parent) {
  // return value should be lives_free'd after use
  weed_plant_t *filter;
  char *filter_name;

  if (!inst) return lives_strdup("");
  filter = weed_instance_get_filter(inst, get_compound_parent);
  filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
  return filter_name;
}


char *rte_keymode_get_filter_name(int key, int mode, boolean add_notes) {
  // return value should be lives_free'd after use
  // key is 1 based
  key--;
  if (!rte_keymode_valid(key + 1, mode, TRUE)) return lives_strdup("");
  return (weed_filter_idx_get_name(key_to_fx[key][mode], FALSE, add_notes));
}


char *rte_keymode_get_plugin_name(int key, int mode) {
  // return value should be lives_free'd after use
  // key is 1 based
  weed_plant_t *filter, *plugin_info;
  char *name;

  key--;
  if (!rte_keymode_valid(key + 1, mode, TRUE)) return lives_strdup("");

  filter = weed_filters[key_to_fx[key][mode]];
  plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
  name = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
  return name;
}


LIVES_GLOBAL_INLINE int rte_bg_gen_key(void) {
  return bg_generator_key;
}

LIVES_GLOBAL_INLINE int rte_fg_gen_key(void) {
  return fg_generator_key;
}

LIVES_GLOBAL_INLINE int rte_bg_gen_mode(void) {
  return bg_generator_mode;
}

LIVES_GLOBAL_INLINE int rte_fg_gen_mode(void) {
  return fg_generator_mode;
}

/**
   @brief

   for rte textmode, get first string parameter for current key/mode instance
   we will then forward all keystrokes to this parm WEED_LEAF_VALUE until the exit key (TAB)
   is pressed
*/
weed_plant_t *get_textparm(void) {
  weed_plant_t *inst, **in_params, *ptmpl, *ret;

  int key = mainw->rte_keys, mode, i, ptype;

  if (key == -1) return NULL;

  mode = rte_key_getmode(key + 1);

  if ((inst = weed_instance_obtain(key, mode))) {
    int nparms;

    in_params = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_PARAMETERS, &nparms);
    if (nparms == 0) {
      weed_instance_unref(inst);
      return NULL;
    }

    for (i = 0; i < nparms; i++) {
      ptmpl = weed_param_get_template(in_params[i]);
      ptype = weed_paramtmpl_get_type(ptmpl);

      if (ptype == WEED_PARAM_TEXT) {
        ret = in_params[i];
        weed_set_int_value(ret, WEED_LEAF_HOST_IDX, i);
        weed_set_plantptr_value(ret, WEED_LEAF_HOST_INSTANCE, inst);
        lives_free(in_params);
        weed_instance_unref(inst);
        return ret;
      }
    }

    lives_free(in_params);
    weed_instance_unref(inst);
  }

  return NULL;
}

/**
   @brief

   newmode has two special values, -1 = cycle forwards, -2 = cycle backwards
   key is 1 based, but may be 0 to use the current mainw->rte_keys
   special handling ensures that if we switch transitions, any background generators survive the switchover
   call with filter mutex unlocked
*/
boolean rte_key_setmode(int key, int newmode) {
  weed_plant_t *inst, *last_inst;
  lives_whentostop_t whentostop = mainw->whentostop;
  int oldmode, maxmode, blend_file, real_key;

  if (key == 0) {
    if ((key = mainw->rte_keys) == -1) return FALSE;
  } else key--;

  real_key = key;

  oldmode = key_modes[key];

  if (key_to_fx[key][0] == -1) {
    return FALSE; // nothing is mapped to effect key
  }

  maxmode = rte_key_getmaxmode(key + 1);

  if (newmode == -1) {
    // cycle forwards
    if (oldmode == maxmode) newmode = 0;
    else newmode = oldmode + 1;
  }

  if (newmode == -2) {
    // cycle backwards
    if (!oldmode) newmode = maxmode;
    else newmode = oldmode - 1;
  }

  if (newmode == oldmode) return FALSE;




  if (rte_window_visible()) rtew_set_mode_radio(key, newmode);
  if (mainw->ce_thumbs) ce_thumbs_set_mode_combo(key, newmode);

  if ((inst = weed_instance_obtain(key, oldmode)) != NULL) {  // adds a ref
    if (enabled_in_channels(inst, FALSE) == 2
        && enabled_in_channels(weed_filters[key_to_fx[key][newmode]], FALSE) == 2) {
      // transition --> transition, allow any bg generators to survive
      key = -key - 1;
    }
  }

  postpone_planning();

  blend_file = mainw->blend_file;

  if (inst) {
    // handle compound fx
    last_inst = inst;
    while (get_next_compound_inst(last_inst)) last_inst = get_next_compound_inst(last_inst);
  }

  if (inst && (enabled_in_channels(inst, FALSE) > 0 || enabled_out_channels(last_inst, FALSE) == 0 ||
               is_pure_audio(inst, FALSE))) {
    // not a (video or video/audio) generator
    weed_deinit_effect(key);
  } else if (enabled_in_channels(weed_filters[key_to_fx[key][newmode]], FALSE) == 0 &&
             has_video_chans_out(weed_filters[key_to_fx[key][newmode]], TRUE))
    mainw->whentostop = NEVER_STOP; // when gen->gen, dont stop pb

  key_modes[real_key] = newmode;

  if (mainw->rte_keys != -1) {
    mainw->blend_factor = weed_get_blend_factor(mainw->rte_keys);
  }

  if (mainw->blend_file != blend_file) {
    //weed_layer_set_invalid(mainw->blend_layer, TRUE);
    mainw->new_blend_file = blend_file;
  } else {
    if (inst) {
      if (!weed_init_effect(key)) {
        weed_instance_unref(inst);
        mainw->whentostop = whentostop;
        key = real_key;
        mainw->rte &= ~(GU641 << key);
        mainw->rte_real &= ~(GU641 << key);
        continue_planning();
        return FALSE;
      }
      weed_instance_unref(inst);
    }
    if (mainw->ce_thumbs) ce_thumbs_add_param_box(real_key, TRUE);
  }
  mainw->whentostop = whentostop;

  continue_planning();

  return TRUE;
}


/**
   @brief

   we will add a filter_class at the next free slot for key, and return the slot number
   if idx or key is invalid, (probably meaning the filter was not found), we return -1
   if all slots are full, we return -3
   currently, generators and non-generators cannot be mixed on the same key (causes problems if the mode is switched)
   in this case a -2 is returned

   key is 1 based !!!

*/
int weed_add_effectkey_by_idx(int key, int idx) {
  if (idx < 0 || idx >= num_weed_filters || key <= 0 || key >= FX_KEYS_MAX) return -1;
  else {
    boolean has_gen = FALSE;
    boolean has_non_gen = FALSE;
    int maxmode;
    int i;

    if (key <= FX_KEYS_MAX_VIRTUAL) {
      filter_mutex_lock(key - 1);

      maxmode = rte_key_getmaxmode(key);
      key--;

      if (maxmode >= prefs->rte_modes_per_key - 1) {
        filter_mutex_unlock(key);
        return -3;
      }

      for (i = 0; i <= maxmode; i++) {
        if (enabled_in_channels(weed_filters[key_to_fx[key][i]], FALSE) == 0
            && has_video_chans_out(weed_filters[key_to_fx[key][i]], TRUE))
          has_gen = TRUE;
        else has_non_gen = TRUE;
      }

      if ((enabled_in_channels(weed_filters[idx], FALSE) == 0 && has_non_gen &&
           !all_outs_alpha(weed_filters[idx], TRUE) && has_video_chans_out(weed_filters[idx], TRUE)) ||
          (enabled_in_channels(weed_filters[idx], FALSE) > 0 && has_gen)) {
        filter_mutex_unlock(key);
        return -2;
      }

      key_to_fx[key][i] = idx;
      filter_mutex_unlock(key);

      if (rte_window_visible() && !mainw->is_rendering && !mainw->multitrack) {
        // if rte window is visible add to combo box
        char *tmp;
        rtew_combo_set_text(key, i, (tmp = rte_keymode_get_filter_name(key + 1, i, FALSE)));
        lives_free(tmp);

        // set in ce_thumb combos
        if (mainw->ce_thumbs) ce_thumbs_reset_combo(key);
      }
      return i;
    }
    if (key_to_instance[--key][0]) BREAK_ME("Instance leak");
    if (key_to_fx[key][0] != -1) BREAK_ME("Filter leak");
    key_to_fx[key][0] = idx;
    return 0;
  }
}


int weed_add_effectkey(int key, const char *hashname, boolean fullname) {
  // add a filter_class by hashname to an effect_key
  int idx = weed_get_idx_for_hashname(hashname, fullname);
  return weed_add_effectkey_by_idx(key, idx);
}


int rte_switch_keymode(int key, int mode, const char *hashname) {
  // this is called when we switch the filter_class bound to an effect_key/mode
  // it will deinit any old instance bound to the key and then init a new one
  // there is special handling for switching generators and transitions
  // (filter mutex unlocked)

  // key is zero based
  weed_plant_t *inst;
  int id = weed_get_idx_for_hashname(hashname, TRUE), tid;
  boolean has_gen = FALSE, has_non_gen = FALSE;

  int test = (mode == 0 ? 1 : 0);

  // effect not found
  if (id == -1) return -1;

  filter_mutex_lock(key);

  if ((tid = key_to_fx[key][test]) != -1) {
    if (enabled_in_channels(weed_filters[tid], FALSE) == 0 && has_video_chans_out(weed_filters[tid], TRUE)) has_gen = TRUE;
    else has_non_gen = TRUE;
  }

  if ((enabled_in_channels(weed_filters[id], FALSE) == 0 && has_video_chans_out(weed_filters[id], TRUE) &&
       !all_outs_alpha(weed_filters[id], TRUE) && has_non_gen) ||
      (enabled_in_channels(weed_filters[id], FALSE) > 0 && has_gen)) {
    // can't mix gens and non-gens - show an error to the user
    filter_mutex_unlock(key);
    return -2;
  }

  if ((inst = weed_instance_obtain(key, mode)) != NULL) {
    // deinit any old instance and init the new one
    int oldkeymode = key_modes[key];
    key_modes[key] = mode;
    // negative key: set is_modeswitch
    // this prevents playback from ending if we switch a generator
    // and also maintaings bg clip if we switch a transition for another
    weed_deinit_effect(-key - 1);
    key_to_fx[key][mode] = id;
    weed_init_effect(-key - 1);
    key_modes[key] = oldkeymode;
    weed_instance_unref(inst);
  } else key_to_fx[key][mode] = id;

  filter_mutex_unlock(key);

  return 0;
}


void rte_swap_fg_bg(void) {
  int key = fg_generator_key;
  int mode = fg_generator_mode;

  fg_generator_key = bg_generator_key;
  fg_generator_mode = bg_generator_mode;

  bg_generator_key = key;
  bg_generator_mode = mode;
}


LIVES_GLOBAL_INLINE int weed_get_sorted_filter(int i) {return LIVES_POINTER_TO_INT(lives_list_nth_data(weed_fx_sorted_list, i));}


LiVESList *weed_get_all_names(lives_fx_list_t list_type) {
  // remember to free list (list + data)  after use, if non-NULL
  LiVESList *list = NULL;
  char *filter_name, *hashname, *string;
  int i, error;

  for (i = 0; i < num_weed_filters; i++) {
    int sorted = weed_get_sorted_filter(i);
    weed_plant_t *filter = weed_filters[sorted];
    filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, &error);
    switch (list_type) {
    case FX_LIST_NAME:
      // just name
      string = lives_strdup(filter_name);
      list = lives_list_append(list, (livespointer)string);
      break;
    case FX_LIST_EXTENDED_NAME: {
      // name + author (if dupe) + subcat + observations
      string = weed_filter_idx_get_name(sorted, TRUE, TRUE);
      list = lives_list_append(list, (livespointer)string);
    }
    break;
    case FX_LIST_HASHNAME:
      // hashnames - authors and not extra_authors
      hashname = lives_strdup(hashnames[sorted][0].string);
      list = lives_list_append(list, (livespointer)hashname);
      break;
    }
    lives_free(filter_name);
  }
  return list;
}


int rte_get_numfilters(void) {return num_weed_filters;}


///////////////////
// parameter interpolation

/**
   @brief

   for a multi valued parameter or pchange, we will fill WEED_LEAF_VALUE up to element index with WEED_LEAF_NEW_DEFAULT
   we will also create the ignore array if there is not one. The values of this are set to WEED_TRUE if the user
   updates the values for that channel, so we know which values to set for interpolation later.
   paramtmpl must be supplied, since pchanges do not have one directly
*/
void fill_param_vals_to(weed_plant_t *param, weed_plant_t *paramtmpl, int index) {
  int i, ptype;
  int num_vals = weed_leaf_num_elements(param, WEED_LEAF_VALUE);
  int new_defi, *valis, *nvalis;
  double new_defd, *valds, *nvalds;
  char *new_defs, **valss, **nvalss;
  int cspace;
  int *colsis, *coli;
  int vcount;
  double *colsds, *cold;

  ptype = weed_paramtmpl_get_type(paramtmpl);
  vcount = num_vals;
  if (index >= vcount) vcount = ++index;

  switch (ptype) {
  case WEED_PARAM_INTEGER:
    if (vcount > num_vals) {
      new_defi = weed_get_int_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      valis = weed_get_int_array(param, WEED_LEAF_VALUE, NULL);
      nvalis = (int *)lives_malloc(vcount * sizint);
      for (i = 0; i < vcount; i++) {
        if (i < num_vals && i < index) nvalis[i] = valis[i];
        else if (i <= num_vals && i > index) nvalis[i] = valis[i - 1];
        else nvalis[i] = new_defi;
      }
      weed_set_int_array(param, WEED_LEAF_VALUE, vcount, nvalis);
      lives_freep((void **)&valis);
      lives_freep((void **)&nvalis);
    }
    break;
  case WEED_PARAM_FLOAT:
    if (vcount > num_vals) {
      new_defd = weed_get_double_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      valds = weed_get_double_array(param, WEED_LEAF_VALUE, NULL);
      nvalds = (double *)lives_malloc(vcount * sizdbl);
      for (i = 0; i < vcount; i++) {
        if (i < num_vals && i < index) nvalds[i] = valds[i];
        else if (i <= num_vals && i > index) nvalds[i] = valds[i - 1];
        else nvalds[i] = new_defd;
      }
      weed_set_double_array(param, WEED_LEAF_VALUE, vcount, nvalds);

      lives_freep((void **)&valds);
      lives_freep((void **)&nvalds);
    }
    break;
  case WEED_PARAM_SWITCH:
    if (vcount > num_vals) {
      new_defi = weed_get_boolean_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      valis = weed_get_boolean_array(param, WEED_LEAF_VALUE, NULL);
      nvalis = (int *)lives_malloc(vcount * sizint);
      for (i = 0; i < vcount; i++) {
        if (i < num_vals && i < index) nvalis[i] = valis[i];
        else if (i <= num_vals && i > index) nvalis[i] = valis[i - 1];
        else nvalis[i] = new_defi;
      }
      weed_set_boolean_array(param, WEED_LEAF_VALUE, vcount, nvalis);
      lives_freep((void **)&valis);
      lives_freep((void **)&nvalis);
    }
    break;
  case WEED_PARAM_TEXT:
    if (vcount > num_vals) {
      new_defs = weed_get_string_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
      valss = weed_get_string_array(param, WEED_LEAF_VALUE, NULL);
      nvalss = (char **)lives_malloc(vcount * sizeof(char *));
      for (i = 0; i < vcount; i++) {
        if (i < num_vals && i < index) nvalss[i] = valss[i];
        else if (i <= num_vals && i > index) nvalss[i] = valss[i - 1];
        else nvalss[i] = new_defs;
      }
      weed_set_string_array(param, WEED_LEAF_VALUE, vcount, nvalss);

      for (i = 0; i < index; i++) {
        lives_freep((void **)&nvalss[i]);
      }

      lives_freep((void **)&valss);
      lives_freep((void **)&nvalss);
    }
    break;
  case WEED_PARAM_COLOR:
    cspace = weed_get_int_value(paramtmpl, WEED_LEAF_COLORSPACE, NULL);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      num_vals /= 3;
      vcount = num_vals;
      index--;
      if (index >= vcount) vcount = ++index;
      vcount *= 3;
      if (weed_leaf_seed_type(paramtmpl, WEED_LEAF_NEW_DEFAULT) == WEED_SEED_INT) {
        colsis = weed_get_int_array(param, WEED_LEAF_VALUE, NULL);
        if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_NEW_DEFAULT) == 1) {
          coli = (int *)lives_malloc(3 * sizint);
          coli[0] = coli[1] = coli[2] = weed_get_int_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
        } else coli = weed_get_int_array(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
        valis = weed_get_int_array(param, WEED_LEAF_VALUE, NULL);
        nvalis = (int *)lives_malloc(vcount * sizint);
        for (i = 0; i < vcount; i += 3) {
          if (i < num_vals && i < index) {
            nvalis[i] = valis[i];
            nvalis[i + 1] = valis[i + 1];
            nvalis[i + 2] = valis[i + 2];
          } else if (i <= num_vals && i > index) {
            nvalis[i] = valis[i - 3];
            nvalis[i + 1] = valis[i - 2];
            nvalis[i + 2] = valis[i - 1];
          } else {
            nvalis[i] = coli[0];
            nvalis[i + 1] = coli[1];
            nvalis[i + 2] = coli[2];
          }
        }
        weed_set_int_array(param, WEED_LEAF_VALUE, vcount, nvalis);
        lives_freep((void **)&valis);
        lives_freep((void **)&colsis);
        lives_freep((void **)&nvalis);
      } else {
        colsds = weed_get_double_array(param, WEED_LEAF_VALUE, NULL);
        if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_NEW_DEFAULT) == 1) {
          cold = (double *)lives_malloc(3 * sizdbl);
          cold[0] = cold[1] = cold[2] = weed_get_double_value(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
        } else cold = weed_get_double_array(paramtmpl, WEED_LEAF_NEW_DEFAULT, NULL);
        valds = weed_get_double_array(param, WEED_LEAF_VALUE, NULL);
        nvalds = (double *)lives_malloc(vcount * sizdbl);
        for (i = 0; i < vcount; i += 3) {
          if (i < num_vals && i < index) {
            nvalds[i] = valds[i];
            nvalds[i + 1] = valds[i + 1];
            nvalds[i + 2] = valds[i + 2];
          } else if (i <= num_vals && i > index) {
            nvalds[i] = valds[i - 3];
            nvalds[i + 1] = valds[i - 2];
            nvalds[i + 2] = valds[i - 1];
          } else {
            nvalds[i] = cold[0];
            nvalds[i + 1] = cold[1];
            nvalds[i + 2] = cold[2];
          }
        }
        weed_set_double_array(param, WEED_LEAF_VALUE, vcount, nvalds);
        lives_freep((void **)&valds);
        lives_freep((void **)&colsds);
        lives_freep((void **)&nvalds);
      }
      vcount /= 3;
    }
    break;
  }

  if (is_perchannel_multiw(param)) {
    int num_vals;
    int *ign_array = weed_get_boolean_array_counted(param, WEED_LEAF_IGNORE, &num_vals);
    if (vcount > num_vals) {
      ign_array = (int *)lives_realloc(ign_array, vcount * sizint);
      for (i = num_vals; i < vcount; i++) {
        ign_array[i] = WEED_TRUE;
      }
      weed_set_boolean_array(param, WEED_LEAF_IGNORE, vcount, ign_array);
    }
    lives_freep((void **)&ign_array);
  }
}


static int get_default_element_int(weed_plant_t *param, int idx, int mpy, int add) {
  int *valsi, val;
  int error;
  weed_plant_t *ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_paramtmpl_value_irrelevant(ptmpl) && weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT) &&
      weed_leaf_num_elements(ptmpl, WEED_LEAF_HOST_DEFAULT) > idx * mpy + add) {
    valsi = weed_get_int_array(ptmpl, WEED_LEAF_HOST_DEFAULT, &error);
    val = valsi[idx * mpy + add];
    lives_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT)
      && weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT) > idx * mpy + add) {
    valsi = weed_get_int_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = valsi[idx * mpy + add];
    lives_free(valsi);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl, WEED_LEAF_NEW_DEFAULT) == mpy) {
    valsi = weed_get_int_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = valsi[add];
    lives_free(valsi);
    return val;
  }
  return weed_get_int_value(ptmpl, WEED_LEAF_NEW_DEFAULT, &error);
}


static double get_default_element_double(weed_plant_t *param, int idx, int mpy, int add) {
  double *valsd, val;
  int error;
  weed_plant_t *ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_paramtmpl_value_irrelevant(ptmpl) && weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT) &&
      weed_leaf_num_elements(ptmpl, WEED_LEAF_HOST_DEFAULT) > idx * mpy + add) {
    valsd = weed_get_double_array(ptmpl, WEED_LEAF_HOST_DEFAULT, &error);
    val = valsd[idx * mpy + add];
    lives_free(valsd);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT)
      && weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT) > idx * mpy + add) {
    valsd = weed_get_double_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = valsd[idx * mpy + add];
    lives_free(valsd);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl, WEED_LEAF_NEW_DEFAULT) == mpy) {
    valsd = weed_get_double_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = valsd[add];
    lives_free(valsd);
    return val;
  }
  return weed_get_double_value(ptmpl, WEED_LEAF_NEW_DEFAULT, &error);
}


static int get_default_element_bool(weed_plant_t *param, int idx) {
  int *valsi, val;
  int error;
  weed_plant_t *ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_paramtmpl_value_irrelevant(ptmpl) && weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT) &&
      weed_leaf_num_elements(ptmpl, WEED_LEAF_HOST_DEFAULT) > idx) {
    valsi = weed_get_boolean_array(ptmpl, WEED_LEAF_HOST_DEFAULT, &error);
    val = valsi[idx];
    lives_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT) && weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT) > idx) {
    valsi = weed_get_boolean_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = valsi[idx];
    lives_free(valsi);
    return val;
  }
  return weed_get_boolean_value(ptmpl, WEED_LEAF_NEW_DEFAULT, &error);
}


static char *get_default_element_string(weed_plant_t *param, int idx) {
  char **valss, *val, *val2;
  int error, i;
  int numvals;
  weed_plant_t *ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_paramtmpl_value_irrelevant(ptmpl) && weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT) &&
      (numvals = weed_leaf_num_elements(ptmpl, WEED_LEAF_HOST_DEFAULT)) > idx) {
    valss = weed_get_string_array(ptmpl, WEED_LEAF_HOST_DEFAULT, &error);
    val = lives_strdup(valss[idx]);
    for (i = 0; i < numvals; i++) lives_free(valss[i]);
    lives_free(valss);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT) && (numvals = weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT)) > idx) {
    valss = weed_get_string_array(ptmpl, WEED_LEAF_DEFAULT, &error);
    val = lives_strdup(valss[idx]);
    for (i = 0; i < numvals; i++) lives_free(valss[i]);
    lives_free(valss);
    return val;
  }
  val = weed_get_string_value(ptmpl, WEED_LEAF_NEW_DEFAULT, &error);
  val2 = lives_strdup(val);
  lives_free(val);
  return val2;
}


boolean interpolate_param(weed_plant_t *param, void *pchain, weed_timecode_t tc) {
  // return FALSE if param has no "value"
  // - this can happen during realtime audio processing, if the effect is inited, but no "value" has been set yet

  weed_plant_t *pchange = (weed_plant_t *)pchain, *last_pchange = NULL;
  weed_plant_t *wtmpl;
  weed_timecode_t tc_diff = 0, tc_diff2;
  void **lpc, **npc;
  char **valss, **nvalss;
  double *last_valuesd, *next_valuesd;
  double *valds = NULL, *nvalds, last_valued;
  double last_valuedr, last_valuedg, last_valuedb, last_valueda;
  int *last_valuesi, *next_valuesi;
  int *valis = NULL, *nvalis, last_valuei;
  int last_valueir, last_valueig, last_valueib, last_valueia;
  int *ign, num_ign = 0;
  int ptype, cspace = 0;
  int got_npc, num_values, xnum, num_pvals, k, j;

  if (!pchange) return TRUE;
  if ((num_values = weed_leaf_num_elements(param, WEED_LEAF_VALUE)) == 0) return FALSE;
  if (weed_param_value_irrelevant(param)) return TRUE;

  while (pchange && get_event_timecode(pchange) <= tc) {
    last_pchange = pchange;
    pchange = (weed_plant_t *)weed_get_voidptr_value(pchange, WEED_LEAF_NEXT_CHANGE, NULL);
  }

  wtmpl = weed_param_get_template(param);

  if ((num_pvals = weed_leaf_num_elements((weed_plant_t *)pchain, WEED_LEAF_VALUE)) > num_values)
    num_values = num_pvals; // init a multivalued param

  lpc = (void **)lives_calloc(num_values, sizeof(void *));
  npc = (void **)lives_calloc(num_values, sizeof(void *));

  if (num_values == 1) {
    lpc[0] = last_pchange;
    npc[0] = pchange;
  } else {
    pchange = (weed_plant_t *)pchain;

    while (pchange) {
      num_pvals = weed_leaf_num_elements(pchange, WEED_LEAF_VALUE);
      if (num_pvals > num_values) num_pvals = num_values;
      if (weed_plant_has_leaf(pchange, WEED_LEAF_IGNORE)) {
        ign = weed_get_boolean_array_counted(pchange, WEED_LEAF_IGNORE, &num_ign);
      } else ign = NULL;
      if (get_event_timecode(pchange) <= tc) {
        for (j = 0; j < num_pvals; j++) if (!ign || j >= num_ign || ign[j] == WEED_FALSE) lpc[j] = pchange;
      } else {
        for (j = 0; j < num_pvals; j++) {
          if (!npc[j] && (!ign || j >= num_ign || ign[j] == WEED_FALSE)) npc[j] = pchange;
        }
        got_npc = 0;
        for (j = 0; j < num_values; j++) {
          if (npc[j]) got_npc++;
        }
        if (got_npc == num_values) {
          lives_freep((void **)&ign);
          break;
        }
      }
      pchange = (weed_plant_t *)weed_get_voidptr_value(pchange, WEED_LEAF_NEXT_CHANGE, NULL);
      lives_freep((void **)&ign);
    }
  }

  ptype = weed_paramtmpl_get_type(wtmpl);
  switch (ptype) {
  case WEED_PARAM_FLOAT:
    valds = (double *)lives_malloc(num_values * (sizeof(double)));
    break;
  case WEED_PARAM_COLOR:
    cspace = weed_get_int_value(wtmpl, WEED_LEAF_COLORSPACE, NULL);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (!(num_values & 3)) return TRUE;
      if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
        valis = (int *)lives_malloc(num_values * sizint);
      } else {
        valds = (double *)lives_malloc(num_values * (sizeof(double)));
      }
      break;
    case WEED_COLORSPACE_RGBA:
      if (num_values & 3) return TRUE;
      if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
        valis = (int *)lives_malloc(num_values * sizint);
      } else {
        valds = (double *)lives_malloc(num_values * (sizeof(double)));
      }
      break;
    }
    break;
  case WEED_PARAM_SWITCH:
  case WEED_PARAM_INTEGER:
    valis = (int *)lives_malloc(num_values * sizint);
    break;
  }

  for (j = 0; j < num_values; j++) {
    // must interpolate - we use linear interpolation
    if (!lpc[j] && !npc[j]) continue;
    if (lpc[j] && npc[j]) tc_diff = weed_get_int64_value((weed_plant_t *)npc[j], WEED_LEAF_TIMECODE, NULL) -
                                      weed_get_int64_value((weed_plant_t *)lpc[j], WEED_LEAF_TIMECODE, NULL);
    switch (ptype) {
    case WEED_PARAM_FLOAT:
      if (!lpc[j]) {
        // before first change
        valds[j] = get_default_element_double(param, j, 1, 0);
        continue;
      }
      if (!npc[j]) {
        // after last change
        nvalds = NULL;
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          if (nvalds) {
            valds[j] = nvalds[j];
            lives_free(nvalds);
          }
        }
        if (!nvalds) valds[j] = get_default_element_double(param, j, 1, 0);
        continue;
      }

      next_valuesd = weed_get_double_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
      last_valuesd = weed_get_double_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
      if (xnum > j) last_valued = last_valuesd[j];
      else last_valued = get_default_element_double(param, j, 1, 0);

      valds[j] = last_valued + (double)(next_valuesd[j] - last_valued) / (double)(tc_diff / TICKS_PER_SECOND_DBL) *
                 (double)((tc - weed_get_int64_value((weed_plant_t *)lpc[j], WEED_LEAF_TIMECODE, NULL)) / TICKS_PER_SECOND_DBL);

      lives_free(last_valuesd);
      lives_free(next_valuesd);
      break;
    case WEED_PARAM_COLOR:
      if (num_values != weed_leaf_num_elements(last_pchange, WEED_LEAF_VALUE)) break; // no interp possible

      switch (cspace) {
      case WEED_COLORSPACE_RGB:
        k = j * 3;
        if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
          if (!lpc[j]) {
            // before first change
            valis[k] = get_default_element_int(param, j, 3, 0);
            valis[k + 1] = get_default_element_int(param, j, 3, 1);
            valis[k + 2] = get_default_element_int(param, j, 3, 2);
            j += 3;
            continue;
          }
          if (!npc[j]) {
            // after last change
            nvalis = NULL;
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              if (nvalis) {
                valis[k] = nvalis[k];
                valis[k + 1] = nvalis[k + 1];
                valis[k + 2] = nvalis[k + 2];
                lives_free(nvalis);
              }
            }
            if (!nvalis) {
              valis[k] = get_default_element_int(param, j, 3, 0);
              valis[k + 1] = get_default_element_int(param, j, 3, 1);
              valis[k + 2] = get_default_element_int(param, j, 3, 2);
            }
            j += 3;
            continue;
          }

          next_valuesi = weed_get_int_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
          last_valuesi = weed_get_int_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
          if (xnum > k) {
            last_valueir = last_valuesi[k];
            last_valueig = last_valuesi[k + 1];
            last_valueib = last_valuesi[k + 2];
          } else {
            last_valueir = get_default_element_int(param, j, 3, 0);
            last_valueig = get_default_element_int(param, j, 3, 1);
            last_valueib = get_default_element_int(param, j, 3, 2);
          }

          if (!next_valuesi) continue; // can happen if we recorded a param change

          valis[k] = last_valueir + (next_valuesi[k] - last_valueir) / (tc_diff / TICKS_PER_SECOND_DBL) *
                     ((tc_diff2 = (tc - weed_get_int64_value((weed_plant_t *)lpc[j],
                                   WEED_LEAF_TIMECODE, NULL))) / TICKS_PER_SECOND_DBL) + .5;
          valis[k + 1] = last_valueig + (next_valuesi[k + 1] - last_valueig) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valis[k + 2] = last_valueib + (next_valuesi[k + 2] - last_valueib) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;

          lives_free(last_valuesi);
          lives_free(next_valuesi);
        } else {
          if (!lpc[j]) {
            // before first change
            valds[k] = get_default_element_double(param, j, 3, 0);
            valds[k + 1] = get_default_element_double(param, j, 3, 1);
            valds[k + 2] = get_default_element_double(param, j, 3, 2);
            j += 3;
            continue;
          }
          if (!npc[j]) {
            // after last change
            nvalds = NULL;
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              if (nvalds) {
                valds[k] = nvalds[k];
                valds[k + 1] = nvalds[k + 1];
                valds[k + 2] = nvalds[k + 2];
                lives_free(nvalds);
              }
            }
            if (!nvalds) {
              valds[k] = get_default_element_double(param, j, 3, 0);
              valds[k + 1] = get_default_element_double(param, j, 3, 1);
              valds[k + 2] = get_default_element_double(param, j, 3, 2);
            }
            j += 3;
            continue;
          }

          next_valuesd = weed_get_double_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
          last_valuesd = weed_get_double_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
          if (xnum > k) {
            last_valuedr = last_valuesd[k];
            last_valuedg = last_valuesd[k + 1];
            last_valuedb = last_valuesd[k + 2];
          } else {
            last_valuedr = get_default_element_double(param, j, 3, 0);
            last_valuedg = get_default_element_double(param, j, 3, 1);
            last_valuedb = get_default_element_double(param, j, 3, 2);
          }
          valds[k] = last_valuedr + (next_valuesd[k] - last_valuedr) / (tc_diff / TICKS_PER_SECOND_DBL) *
                     ((tc_diff2 = (tc - weed_get_int64_value((weed_plant_t *)lpc[j],
                                   WEED_LEAF_TIMECODE, NULL))) / TICKS_PER_SECOND_DBL);
          valds[k + 1] = last_valuedg + (next_valuesd[k + 1] - last_valuedg) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valds[k + 2] = last_valuedb + (next_valuesd[k + 2] - last_valuedb) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;

          lives_free(last_valuesd);
          lives_free(next_valuesd);
        }
        j += 3;
        break;
      case WEED_COLORSPACE_RGBA:
        k = j * 4;
        if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
          if (!lpc[j]) {
            // before first change
            valis[k] = get_default_element_int(param, j, 4, 0);
            valis[k + 1] = get_default_element_int(param, j, 4, 1);
            valis[k + 2] = get_default_element_int(param, j, 4, 2);
            valis[k + 3] = get_default_element_int(param, j, 4, 3);
            j += 4;
            continue;
          }
          if (!npc[j]) {
            // after last change
            nvalis = NULL;
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              if (nvalis) {
                valis[k] = nvalis[k];
                valis[k + 1] = nvalis[k + 1];
                valis[k + 2] = nvalis[k + 2];
                valis[k + 3] = nvalis[k + 3];
                lives_free(nvalis);
              }
            }
            if (!nvalis) {
              valis[k] = get_default_element_int(param, j, 4, 0);
              valis[k + 1] = get_default_element_int(param, j, 4, 1);
              valis[k + 2] = get_default_element_int(param, j, 4, 2);
              valis[k + 3] = get_default_element_int(param, j, 4, 3);
            }
            j += 4;
            continue;
          }

          next_valuesi = weed_get_int_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
          last_valuesi = weed_get_int_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
          if (xnum > k) {
            last_valueir = last_valuesi[k];
            last_valueig = last_valuesi[k + 1];
            last_valueib = last_valuesi[k + 2];
            last_valueia = last_valuesi[k + 3];
          } else {
            last_valueir = get_default_element_int(param, j, 4, 0);
            last_valueig = get_default_element_int(param, j, 4, 1);
            last_valueib = get_default_element_int(param, j, 4, 2);
            last_valueia = get_default_element_int(param, j, 4, 3);
          }

          if (!next_valuesi) continue; // can happen if we recorded a param change

          valis[k] = last_valueir + (next_valuesi[k] - last_valueir) / (tc_diff / TICKS_PER_SECOND_DBL) *
                     ((tc_diff2 = (tc - weed_get_int64_value((weed_plant_t *)lpc[j],
                                   WEED_LEAF_TIMECODE, NULL))) / TICKS_PER_SECOND_DBL) + .5;
          valis[k + 1] = last_valueig + (next_valuesi[k + 1] - last_valueig) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valis[k + 2] = last_valueib + (next_valuesi[k + 2] - last_valueib) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valis[k + 3] = last_valueia + (next_valuesi[k + 3] - last_valueia) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;

          lives_free(last_valuesi);
          lives_free(next_valuesi);
        } else {
          if (!lpc[j]) {
            // before first change
            valds[k] = get_default_element_double(param, j, 4, 0);
            valds[k + 1] = get_default_element_double(param, j, 4, 1);
            valds[k + 2] = get_default_element_double(param, j, 4, 2);
            valds[k + 3] = get_default_element_double(param, j, 4, 3);
            j += 4;
            continue;
          }
          if (!npc[j]) {
            // after last change
            nvalds = NULL;
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              if (nvalds) {
                valds[k] = nvalds[k];
                valds[k + 1] = nvalds[k + 1];
                valds[k + 2] = nvalds[k + 2];
                valds[k + 3] = nvalds[k + 3];
                lives_free(nvalds);
              }
            }
            if (!nvalds) {
              valds[k] = get_default_element_double(param, j, 4, 0);
              valds[k + 1] = get_default_element_double(param, j, 4, 1);
              valds[k + 2] = get_default_element_double(param, j, 4, 2);
              valds[k + 3] = get_default_element_double(param, j, 4, 3);
            }
            j += 4;
            continue;
          }

          next_valuesd = weed_get_double_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
          last_valuesd = weed_get_double_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
          if (xnum > k) {
            last_valuedr = last_valuesd[k];
            last_valuedg = last_valuesd[k + 1];
            last_valuedb = last_valuesd[k + 2];
            last_valueda = last_valuesd[k + 3];
          } else {
            last_valuedr = get_default_element_double(param, j, 4, 0);
            last_valuedg = get_default_element_double(param, j, 4, 1);
            last_valuedb = get_default_element_double(param, j, 4, 2);
            last_valueda = get_default_element_double(param, j, 4, 3);
          }
          valds[k] = last_valuedr + (next_valuesd[k] - last_valuedr) / (tc_diff / TICKS_PER_SECOND_DBL) *
                     ((tc_diff2 = (tc - weed_get_int64_value((weed_plant_t *)lpc[j],
                                   WEED_LEAF_TIMECODE, NULL))) / TICKS_PER_SECOND_DBL);
          valds[k + 1] = last_valuedg + (next_valuesd[k + 1] - last_valuedg) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valds[k + 2] = last_valuedb + (next_valuesd[k + 2] - last_valuedb) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;
          valds[k + 3] = last_valueda + (next_valuesd[k + 3] - last_valuedb) / (tc_diff / TICKS_PER_SECOND_DBL) *
                         (tc_diff2 / TICKS_PER_SECOND_DBL) + .5;

          lives_free(last_valuesd);
          lives_free(next_valuesd);
        }
        j += 4;
        break;
      } // cspace
      break; // color
    case WEED_PARAM_INTEGER:
      if (weed_param_get_nchoices(param) > 0) {
        // no interpolation
        if (npc[j] && get_event_timecode((weed_plant_t *)npc[j]) == tc) {
          nvalis = weed_get_int_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
          valis[j] = nvalis[j];
          lives_free(nvalis);
          continue;
        } else {
          // use last_pchange value
          nvalis = NULL;
          xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
          if (xnum > j) {
            nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
            if (nvalis) {
              valis[j] = nvalis[j];
              lives_free(nvalis);
            }
          }
          if (!nvalis) valis[j] = get_default_element_int(param, j, 1, 0);
          continue;
        }
      } else {
        if (!lpc[j]) {
          // before first change
          valis[j] = get_default_element_int(param, j, 1, 0);
          continue;
        }
        if (!npc[j]) {
          // after last change
          nvalis = NULL;
          xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
          if (xnum > j) {
            nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
            if (nvalis) {
              valis[j] = nvalis[j];
              lives_free(nvalis);
            }
          }
          if (!nvalis) valis[j] = get_default_element_int(param, j, 1, 0);
          continue;
        }

        next_valuesi = weed_get_int_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
        last_valuesi = weed_get_int_array_counted((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, &xnum);
        if (xnum > j) last_valuei = last_valuesi[j];
        else last_valuei = get_default_element_int(param, j, 1, 0);

        valis[j] = last_valuei + (next_valuesi[j] - last_valuei) / (tc_diff / TICKS_PER_SECOND_DBL) *
                   ((tc - weed_get_int64_value((weed_plant_t *)lpc[j], WEED_LEAF_TIMECODE, NULL)) / TICKS_PER_SECOND_DBL) + .5;
        lives_free(last_valuesi);
        lives_free(next_valuesi);
        break;
      }
    case WEED_PARAM_SWITCH:
      // no interpolation
      if (npc[j] && get_event_timecode((weed_plant_t *)npc[j]) == tc) {
        nvalis = weed_get_boolean_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
        valis[j] = nvalis[j];
        lives_free(nvalis);
        continue;
      } else {
        // use last_pchange value
        nvalis = NULL;
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalis = weed_get_boolean_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          if (nvalis) {
            valis[j] = nvalis[j];
            lives_free(nvalis);
          }
        }
        if (!nvalis) valis[j] = get_default_element_bool(param, j);
        continue;
      }
      break;
    case WEED_PARAM_TEXT:
      // no interpolation
      valss = weed_get_string_array(param, WEED_LEAF_VALUE, NULL);
      if (npc[j] && get_event_timecode((weed_plant_t *)npc[j]) == tc) {
        nvalss = weed_get_string_array((weed_plant_t *)npc[j], WEED_LEAF_VALUE, NULL);
        valss[j] = lives_strdup(nvalss[j]);
        for (k = 0; k < num_values; k++) lives_free(nvalss[k]);
        lives_free(nvalss);
        weed_set_string_array(param, WEED_LEAF_VALUE, num_values, valss);
        for (k = 0; k < num_values; k++) lives_free(valss[k]);
        lives_free(valss);
        continue;
      } else {
        // use last_pchange value
        nvalss = NULL;
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalss = weed_get_string_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          if (nvalss) {
            valss[j] = lives_strdup(nvalss[j]);
            for (k = 0; k < xnum; k++) lives_free(nvalss[k]);
            lives_free(nvalss);
          }
        }
        if (!nvalss) valss[j] = get_default_element_string(param, j);
        weed_set_string_array(param, WEED_LEAF_VALUE, num_values, valss);
        for (k = 0; k < num_values; k++) lives_free(valss[k]);
        lives_free(valss);
        continue;
      }
      break;
    } // parameter ptype
  } // j

  switch (ptype) {
  case WEED_PARAM_FLOAT:
    weed_set_double_array(param, WEED_LEAF_VALUE, num_values, valds);
    lives_free(valds);
    break;
  case WEED_PARAM_COLOR:
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (weed_leaf_seed_type(wtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
        weed_set_int_array(param, WEED_LEAF_VALUE, num_values, valis);
        lives_free(valis);
      } else {
        weed_set_double_array(param, WEED_LEAF_VALUE, num_values, valds);
        lives_free(valds);
      }
      break;
    }
    break;
  case WEED_PARAM_INTEGER:
    weed_set_int_array(param, WEED_LEAF_VALUE, num_values, valis);
    lives_free(valis);
    break;
  case WEED_PARAM_SWITCH:
    weed_set_boolean_array(param, WEED_LEAF_VALUE, num_values, valis);
    lives_free(valis);
    break;
  }

  lives_free(npc);
  lives_free(lpc);
  return TRUE;
}

/**
   @brief

   interpolate all in_parameters for filter_instance inst, using void **pchain,
   which is an array of param_change events in temporal order
   values are calculated for timecode tc. We skip WEED_LEAF_HIDDEN parameters
*/
boolean interpolate_params(weed_plant_t *inst, void **pchains, weed_timecode_t tc) {
  weed_plant_t **in_params;
  void *pchain;
  int num_params, offset = 0;

  do {
    if (!pchains || (in_params = weed_instance_get_in_params(inst, &num_params)) == NULL) continue;
    for (int i = offset; i < offset + num_params; i++) {
      if (!is_hidden_param(inst, i - offset)) {
        pchain = pchains[i];
        if (pchain && WEED_PLANT_IS_EVENT((weed_plant_t *)pchain)
            && WEED_EVENT_IS_PARAM_CHANGE((weed_plant_t *)pchain)) {
          if (weed_param_value_irrelevant(in_params[i - offset])) continue;
          if (!interpolate_param(in_params[i - offset], pchain, tc)) {
            lives_free(in_params);
            return FALSE; // FALSE if param is not ready
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*
    offset += num_params;
    lives_free(in_params);
  } while ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)) != NULL);

  return TRUE;
}


///////////////////////////////////////////////////////////
////// hashnames

/**
   @brief

   return value should be freed after use

   make hashname from filter_idx

   if fullname is FALSE, return filename, filtername concatenated
   if fullname is TRUE, return filename, filtername, author, version concatenated

   if sep is not 0, we ignore the booleans and return filename, sep, filtername, sep, author concatenated
   (suitable to be fed into weed_filter_highest_version() later)

   use_extra_authors is only for backwards compatibility

*/
char *make_weed_hashname(int filter_idx, boolean fullname, boolean use_extra_authors, char sep, boolean subs) {
  weed_plant_t *filter, *plugin_info;

  char plugin_fname[PATH_MAX];
  char *plugin_name, *filter_name, *filter_author, *filter_version, *hashname, *filename;
  char xsep[2];
  boolean use_micro = FALSE;
  int version;
  int type = 0;

  if (filter_idx < 0 || filter_idx >= num_weed_filters) return lives_strdup("");

  if (!fullname) type += 2;
  if (use_extra_authors) type++;

  if (sep != 0) {
    fullname = TRUE;
    use_extra_authors = FALSE;
    xsep[0] = sep;
    xsep[1] = 0;
  } else {
    xsep[0] = xsep[1] = 0;
    if (hashnames[filter_idx][type].string)
      return lives_strdup(hashnames[filter_idx][type].string);
  }

  filter = weed_filters[filter_idx];

  if (sep == 0 && hashnames[filter_idx][type].string) {
    return lives_strdup(hashnames[filter_idx][type].string);
  }
  if (weed_plant_has_leaf(filter, WEED_LEAF_PLUGIN_INFO)) {
    plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
    if (weed_plant_has_leaf(plugin_info, WEED_LEAF_PACKAGE_NAME)) {
      filename = weed_get_string_value(plugin_info, WEED_LEAF_PACKAGE_NAME, NULL);
    } else {
      plugin_name = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
      lives_snprintf(plugin_fname, PATH_MAX, "%s", plugin_name);
      lives_free(plugin_name);
      get_filename(plugin_fname, TRUE);
      // should we really use utf-8 here ? (needs checking)
      filename = F2U8(plugin_fname);
    }
  } else {
    return lives_strdup("");
  }
  filter_name = weed_filter_get_name(filter);

  if (fullname) {
    if (!use_extra_authors || !weed_plant_has_leaf(filter, WEED_LEAF_EXTRA_AUTHORS))
      filter_author = weed_get_string_value(filter, WEED_LEAF_AUTHOR, NULL);
    else
      filter_author = weed_get_string_value(filter, WEED_LEAF_EXTRA_AUTHORS, NULL);

    version = weed_get_int_value(filter, WEED_LEAF_VERSION, NULL);

    if (*xsep) {
      hashname = lives_strconcat(filename, xsep, filter_name, xsep, filter_author, NULL);
    } else {
      if (use_micro) {
        int micro_version = weed_get_int_value(filter, WEED_LEAF_MICRO_VERSION, NULL);
        filter_version = lives_strdup_printf("%d.%d", version, micro_version);
      } else {
        filter_version = lives_strdup_printf("%d", version);
      }
      hashname = lives_strconcat(filename, filter_name, filter_author, filter_version, NULL);
      lives_free(filter_version);
    }
    lives_free(filter_author);
  } else {
    hashname = lives_strconcat(filename, filter_name, NULL);
  }
  lives_free(filter_name);
  lives_free(filename);

  if (!hashname) return NULL;

  if (subs) {
    char *xhashname = subst(hashname, " ", "_");
    if (lives_strcmp(xhashname, hashname)) {
      lives_free(hashname);
      return xhashname;
    }
    lives_free(hashname);
    lives_free(xhashname);
    return NULL;
  }
  //g_print("added filter %s\n",hashname);
  return hashname;
}


static char *fix_hashnames(const char *old) {
  const char *alterations[4] = {"frei0rFrei0r: ", "lapdspaLADSPA: ", "libvisuallibvisual: ", NULL};
  const char *replacements[4] = {"Frei0r", "LADSPA", "libvisual", NULL};
  char *hashname_new = NULL;
  int i = 0;
  while (alterations[i]) {
    if (strstr(old, alterations[i])) {
      hashname_new = subst(old, alterations[i], replacements[i]);
      break;
    }
    i++;
  }
  if (hashname_new) {
    return hashname_new;
  }
  return lives_strdup(old);
}


int weed_get_idx_for_hashname(const char *hashname, boolean fullname) {
  uint32_t numhash;
  char *xhashname = fix_hashnames(hashname);
  int type = 0;
  int i;

  if (!fullname) type = 2;

  numhash = lives_string_hash(xhashname);

  for (i = 0; i < num_weed_filters; i++) {
    if (numhash == hashnames[i][type].hash) {
      if (!lives_utf8_strcasecmp(xhashname, hashnames[i][type].string)) {
        lives_free(xhashname);
        return i;
      }
    }
  }

  type += 4;

  if (hashnames && hashnames[0][type].string) {
    for (i = 0; i < num_weed_filters; i++) {
      if (numhash == hashnames[i][type].hash) {
        if (!lives_utf8_strcasecmp(xhashname, hashnames[i][type].string)) {
          lives_free(xhashname);
          return i;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (fullname) {
    type = 1;
    for (i = 0; i < num_weed_filters; i++) {
      if (numhash == hashnames[i][type].hash) {
        if (!lives_utf8_strcasecmp(xhashname, hashnames[i][type].string)) {
          lives_free(xhashname);
          return i;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  lives_free(xhashname);
  return -1;
}


static boolean check_match(weed_plant_t *filter, const char *pkg, const char *fxname, const char *auth, int version) {
  // perform template matching for a filter, see weed_get_indices_from_template()
  weed_plant_t *plugin_info;

  char plugin_fname[PATH_MAX];
  char *plugin_name, *filter_name, *filter_author;

  int filter_version;

  //g_print("looking for %s / %s / %s / %d\n", pkg, fxname, auth, version);

  if (pkg && *pkg) {
    char *filename;

    if (weed_plant_has_leaf(filter, WEED_LEAF_PLUGIN_INFO)) {
      plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
      if (weed_plant_has_leaf(plugin_info, WEED_LEAF_PACKAGE_NAME)) {
        filename = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
        //g_print("xfnm = %s\n", filename);
      } else {
        plugin_name = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
        //g_print("plnm = %s\n", plugin_name);
        lives_snprintf(plugin_fname, PATH_MAX, "%s", plugin_name);
        lives_freep((void **)&plugin_name);
        get_filename(plugin_fname, TRUE);
        //g_print("plfnm = %s\n", plugin_fname);
        filename = F2U8(plugin_fname);
        //g_print("fnm = %s\n", filename);
      }
    } else return FALSE;

    //g_print("pkg = %s\n", pkg);
    if (lives_utf8_strcasecmp(pkg, filename)) {
      lives_freep((void **)&filename);
      return FALSE;
    }
    lives_freep((void **)&filename);
  }

  if (fxname && *fxname) {
    filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
    if (lives_utf8_strcasecmp(fxname, filter_name)) {
      lives_freep((void **)&filter_name);
      return FALSE;
    }
    lives_freep((void **)&filter_name);
  }

  if (auth && *auth) {
    filter_author = weed_get_string_value(filter, WEED_LEAF_AUTHOR, NULL);
    if (lives_utf8_strcasecmp(auth, filter_author)) {
      lives_freep((void **)&filter_author);
      return FALSE;
    }
    lives_freep((void **)&filter_author);
  }

  if (version > 0) {
    filter_version = weed_get_int_value(filter, WEED_LEAF_VERSION, NULL);
    if (version != filter_version) return FALSE;
  }

  return TRUE;
}


/**
   @brief

   generate a list of filter indices from a given template. Char values may be NULL or "" to signify match-any.
   version may be 0 to signify match-any
   otherwise values must match those specified

   returns: a list of int indices, with the last value == -1
   return value should be freed after use
*/
int *weed_get_indices_from_template(const char *pkg, const char *fxname, const char *auth, int version) {
  weed_plant_t *filter;
  int *rvals;

  int count = 1, count2 = 0;

  int i;

  // count number of return values
  for (i = 0; i < num_weed_filters; i++) {
    filter = weed_filters[i];

    if (check_match(filter, pkg, fxname, auth, version)) {
      count++;
    }
  }

  // allocate storage space
  rvals = (int *)lives_malloc(count * sizint);

  // get return values
  for (i = 0; count2 < count - 1; i++) {
    filter = weed_filters[i];

    if (check_match(filter, pkg, fxname, auth, version)) {
      rvals[count2++] = i;
    }
  }

  // end-of-array indicator
  rvals[count2] = -1;

  return rvals;
}


int weed_filter_highest_version(const char *pkg, const char *fxname, const char *auth, int *xversion) {
  int *allversions = weed_get_indices_from_template(pkg, fxname, auth, 0);
  int highestv = 0, version, i = 0, hidx = -1;
  char *hash;
  while ((version = allversions[i] != -1)) {
    if (version > highestv) {
      highestv = version;
    }
    i++;
  }

  lives_free(allversions);
  if (xversion) *xversion = highestv;
  hash = lives_strdup_printf("%s%s%s%d", pkg, fxname, auth, highestv);
  hidx =  weed_get_idx_for_hashname(hash, TRUE);
  lives_free(hash);
  return hidx;
}


weed_plant_t *get_weed_filter(int idx) {
  if (idx > -1 && idx < num_weed_filters) return weed_filters[idx];
  return NULL;
}


/**
   @brief serialise a leaf

   serialise a leaf with key "key" to memory or to file
   for file, set fd >= 0 and mem to NULL
   for memory, pass the address of a memory area (which must be large enough to accept the data)
   if write_all is set then we first write the key name

   serialisation format is 4 bytes "size" (little-endian for file) followed by the data
   - strings are not NULL terminated
   - pointer types are converted to uint64_t before writing

   returns bytesize of serialised leaf

   format is [key_len (4 bytes) | key (key_len bytes)] seed_type (4 bytes) n_elements (4 bytes)
   then for each element: value_size (4 bytes) value
*/
static size_t weed_leaf_serialise(int fd, weed_plant_t *plant, const char *key, boolean write_all, unsigned char **mem) {
  void *value = NULL, *valuer = NULL;

  size_t totsize = 0;

  weed_size_t vlen;
  weed_size_t keylen = (weed_size_t)lives_strlen(key);

  int st, ne;
  int j;

  // write errors will be checked for by the calling function

  if (write_all) {
    // write byte length of key, followed by key in utf-8
    if (!mem) {
      lives_write_le_buffered(fd, &keylen, 4, TRUE);
      lives_write_buffered(fd, key, (size_t)keylen, TRUE);
    } else {
      lives_memcpy(*mem, &keylen, 4);
      *mem += 4;
      lives_memcpy(*mem, key, (size_t)keylen);
      *mem += keylen;
    }
    totsize += 4 + keylen;
  }

  // write seed type and number of elements
  st = weed_leaf_seed_type(plant, key);
  if (!mem && st == WEED_SEED_PLANTPTR) st = WEED_SEED_VOIDPTR;

  if (!mem) lives_write_le_buffered(fd, &st, 4, TRUE);
  else {
    lives_memcpy(*mem, &st, 4);
    *mem += 4;
  }
  ne = weed_leaf_num_elements(plant, key);
  if (!mem) lives_write_le_buffered(fd, &ne, 4, TRUE);
  else {
    lives_memcpy(*mem, &ne, 4);
    *mem += 4;
  }

  totsize += 8;

  // for pixel_data we do special handling
  // older LiVES versions wrote the bytesize (4 bytes, then the data
  if (!mem && !lives_strcmp(key, WEED_LEAF_PIXEL_DATA)) {
    weed_layer_t *layer = (weed_layer_t *)plant;
    int nplanes;
    int *rowstrides = weed_layer_get_rowstrides(layer, &nplanes);
    int pal = weed_layer_get_palette(layer);
    int width = weed_layer_get_width(layer);
    int height = weed_layer_get_height(layer);
    int ival = 0;
    boolean contig = FALSE;
    size_t padding = 0;
    size_t pdsize = 0;

    uint8_t **pixel_data = (uint8_t **)weed_layer_get_pixel_data_planar(layer, NULL);
    /// new style: we will write 4 bytes 0, then possibly further padding bytes,
    /// following this, a 4 byte identifier: 0x57454544, and 4 bytes version *little endian. The current version is 0x01000000
    /// this is then followed by 4 bytes nplanes, 4 bytes palette, 4 bytes width, 4 bytes height.
    /// width is in macropixel size - for UYVY and YUYV each macropixel is 4 bytes and maps to 2 screen pixels
    /// then for each plane: rowstride 4 bytes, data size 8 bytes (the data size is plane height * rowstride + padding)
    /// finally the pixel data
    lives_write_le_buffered(fd, &ival, 4, TRUE);
    ival = WEED_LAYER_MARKER
           lives_write_le_buffered(fd, &ival, 4, TRUE);
    ival = 1; /// version
    lives_write_le_buffered(fd, &ival, 4, TRUE);
    lives_write_le_buffered(fd, &nplanes, 4, TRUE);
    lives_write_le_buffered(fd, &pal, 4, TRUE);
    lives_write_le_buffered(fd, &width, 4, TRUE);
    lives_write_le_buffered(fd, &height, 4, TRUE);

    totsize += 28;

    if (weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, NULL) == WEED_TRUE) {
      contig = TRUE;
    }
    padding = 0;
    for (j = 0; j < nplanes; j++) {
      vlen = (weed_size_t)((double)height * weed_palette_get_plane_ratio_vertical(pal, j) * (double)rowstrides[j]);
      if (contig && j < nplanes - 1) padding = pixel_data[j + 1] - pixel_data[j] - vlen;
      vlen += padding;
      lives_write_le_buffered(fd, &rowstrides[j], 4, TRUE);
      lives_write_le_buffered(fd, &vlen, 8, TRUE);
      pdsize += vlen;
      totsize += 12;
    }
    if (!contig) {
      for (j = 0; j < nplanes; j++) {
        vlen = (weed_size_t)((double)height * weed_palette_get_plane_ratio_vertical(pal, j) * (double)rowstrides[j]);
        lives_write_buffered(fd, (const char *)pixel_data[j], vlen, TRUE);
        totsize += vlen;
      }
    } else {
      lives_write_buffered(fd, (const char *)pixel_data[0], pdsize, TRUE);
      totsize += pdsize;
    }
    lives_free(rowstrides);
    lives_free(pixel_data);
  } else {
    // for each element, write the data size followed by the data
    for (j = 0; j < ne; j++) {
      vlen = (weed_size_t)weed_leaf_element_size(plant, key, j);
      if (!mem && vlen == 0) {
        // we need to do this because NULL pointers return a size of 0, and older versions of LiVES
        // expected to always read 8 bytes for a void *
        if (st > 64) vlen = 8;
      }
      if (st != WEED_SEED_STRING) {
        if (vlen == 0) value = NULL;
        else {
          value = lives_malloc((size_t)vlen);
          weed_leaf_get(plant, key, j, value);
        }
      } else {
        if (vlen == 0) value = NULL;
        // need to create a buffer to receive the string + terminating NULL
        value = lives_malloc((size_t)(vlen));
        // weed_leaf_get() will assume it's a pointer to a variable of the correct type, and fill in the value
        weed_leaf_get(plant, key, j, &value);
        vlen--; // for backwards compat
      }
      if (!mem && weed_leaf_seed_type(plant, key) > 64) {
        // save voidptr as 64 bit
        valuer = (uint64_t *)lives_malloc(sizeof(uint64_t));

        // 'valuer' is a void * (size 8). and 'value' is void * (of whatever size)
        // but we can cast 'value' to a (void **),
        // and then we can dereference it and cast that value to a uint64, and store the result in valuer
        *((uint64_t *)valuer) = (uint64_t)(*((void **)value));
        vlen = sizeof(uint64_t);
      } else valuer = value;

      if (!mem) {
        lives_write_le_buffered(fd, &vlen, 4, TRUE);
        if (st != WEED_SEED_STRING) {
          lives_write_le_buffered(fd, valuer, (size_t)vlen, TRUE);
        } else lives_write_buffered(fd, (const char *)valuer, (size_t)vlen, TRUE);
      } else {
        lives_memcpy(*mem, &vlen, 4);
        *mem += 4;
        if (vlen > 0) {
          lives_memcpy(*mem, value, (size_t)vlen);
          *mem += vlen;
        }
      }
      if (valuer != value) lives_freep((void **)&valuer);
      totsize += 4 + vlen;
      lives_freep((void **)&value);
    }
  }

  // write errors should be checked for by the calling function

  return totsize;
}


size_t weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem) {
  // serialise an entire plant
  //
  // write errors should be checked for by the calling function

  // returns the bytesize of the serialised plant

  size_t totsize = 0;
  weed_size_t nleaves;
  char **proplist = weed_plant_list_leaves(plant, &nleaves);
  char *prop;
  int i = (int)nleaves;
  int pd_needed = 0, pd_reqs = 0;

  if (!proplist) BREAK_ME("null proplist");

  if (WEED_IS_LAYER(plant)) pd_needed = 1;

  if (!mem) lives_write_le_buffered(fd, &i, 4, TRUE); // write number of leaves
  else {
    lives_memcpy(*mem, &i, 4);
    *mem += 4;
  }

  totsize += 4;

  // serialise the "type" leaf first, so that we know this is a new plant when deserialising
  totsize += weed_leaf_serialise(fd, plant, WEED_LEAF_TYPE, TRUE, mem);
  _ext_free(proplist[0]);

  for (i = 1; (prop = proplist[i]); i++) {
    // write each leaf and key
    if (pd_needed > 0) {
      // write pal, height, rowstrides before pixel_data.
      if (!lives_strcmp(prop, WEED_LEAF_PIXEL_DATA)) {
        if (pd_reqs > 3) pd_needed = 0;
        else {
          ++pd_needed;
          _ext_free(prop);
          continue;
        }
      } else {
        if (pd_reqs < 4 && (!strcmp(prop, WEED_LEAF_WIDTH) || !strcmp(prop, WEED_LEAF_HEIGHT)
                            || !lives_strcmp(prop, WEED_LEAF_CURRENT_PALETTE)
                            || !strcmp(prop, WEED_LEAF_ROWSTRIDES))) {
          if (++pd_reqs == 4 && pd_needed > 1) {
            totsize += weed_leaf_serialise(fd, plant, prop, TRUE, mem);
            _ext_free(prop);
            totsize += weed_leaf_serialise(fd, plant, WEED_LEAF_PIXEL_DATA, TRUE, mem);
            pd_needed  = 0;
            continue;
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*

    totsize += weed_leaf_serialise(fd, plant, prop, TRUE, mem);
    _ext_free(prop);
  }
  if (proplist) _ext_free(proplist);
  return totsize;
}


LIVES_GLOBAL_INLINE int32_t weed_plant_mutate(weed_plantptr_t plant, int32_t newtype) {
  // beware of mutant plants....
  static pthread_mutex_t mutate_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mutate_mutex);
  weed_plant_mutate_type(plant, newtype);
  pthread_mutex_unlock(&mutate_mutex);
  return newtype;
}


LIVES_GLOBAL_INLINE void weed_plant_sanitize(weed_plant_t *plant, boolean sterilize) {
  // sterilize should be used when dealing with a deserialized plant,
  // for nullifying the pixel_data of a "live" plant, 'sterilize' should not be set
  // as it may remove leaves which need specific handling (e.g refcounter)
  if (plant) {
    char **leaves = weed_plant_list_leaves(plant, NULL);
    for (int i = 0; leaves[i]; i++) {
      if (is_pixdata_nullify_leaf(leaves[i]) || (sterilize && is_no_copy_leaf(leaves[i]))) {
        //if (weed_leaf_get_flags(plant, leaves[i]) & LIVES_FLAG_FREE_ON_DELETE)
        weed_leaf_clear_flagbits(plant, leaves[i], LIVES_FLAG_FREE_ON_DELETE | WEED_FLAG_UNDELETABLE);
        weed_leaf_delete(plant, leaves[i]);
      }
      _ext_free(leaves[i]);
    }
    _ext_free(leaves);
  }
}

// duplicate, clean and reset refcount for copy
LIVES_GLOBAL_INLINE void weed_plant_duplicate_clean(weed_plant_t *dst, weed_plant_t *src) {
  weed_plant_duplicate(dst, src, FALSE);
  weed_plant_sanitize(dst, TRUE);
}

// duplicate / add, clean and reset refcountv
LIVES_GLOBAL_INLINE void weed_plant_dup_add_clean(weed_plant_t *dst, weed_plant_t *src) {
  weed_plant_duplicate(dst, src, TRUE);
  weed_plant_sanitize(dst, TRUE);
}

// TBD
LIVES_GLOBAL_INLINE void weed_plant_dup_add_sterilize(weed_plant_t *dst, weed_plant_t *src) {
  weed_plant_duplicate(dst, src, TRUE);
  weed_plant_sanitize(dst, TRUE);
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_copy(weed_plant_t *src) {
  weed_plant_t *plant;
  weed_error_t err;
  char **proplist;
  char *prop;
  int i = 0;

  if (!src) return NULL;

  plant = weed_plant_new(weed_get_int_value(src, WEED_LEAF_TYPE, &err));
  if (!plant) return NULL;

  proplist = weed_plant_list_leaves(src, NULL);
  for (prop = proplist[1]; (prop = proplist[i]) != NULL && err == WEED_SUCCESS; i++) {
    if (err == WEED_SUCCESS) {
      err = lives_leaf_dup(plant, src, prop);
    }
    _ext_free(prop);
  } _ext_free(proplist);

  if (err == WEED_ERROR_MEMORY_ALLOCATION) {
    //if (plant!=NULL) weed_plant_free(plant); // well, we should free the plant, but the plugins don't have this function...
    return NULL;
  }

  return plant;
}


#define REALIGN_MAX (40 * 1024 * 1024)  /// 40 MiB "should be enough for anyone"
#define MAX_FRAME_SIZE MILLIONS(100)
#define MAX_FRAME_SIZE64 3019898880

static int realign_typeleaf(int fd, weed_plant_t *plant) {
  uint8_t buff[12];
  const char XMATCH[8] = {4, 0, 0, 0, 't', 'y', 'p', 'e'};
  int type, nl;
  uint64_t count = REALIGN_MAX;
  int len;

  if (lives_read_buffered(fd, buff, 12, TRUE) < 12) return 0;
  for (len = 8; len > 0; len--) {
    if (!memcmp((void *)(buff + 12 - len), (void *)(XMATCH + 8 - len), len)) break;
  }
  if (len == 8) len--;

  while (--count != 0) {
    if (buff[11] == XMATCH[len]) {
      len++;
      if (len == 8) {
        nl = (uint32_t)((buff[3] << 24) + (buff[2] << 16) + (buff[1] << 8) + buff[0]);
        // skip st, ne, valsize
        if (lives_read_buffered(fd, buff, 12, TRUE) < 12) return 0;
        if (lives_read_le_buffered(fd, &type, 4, TRUE) < 4) return 0;
        weed_plant_mutate(plant, type);
        return nl;
      }
    } else {
      if (len > 0) {
        len = 0;
        continue;
      }
    }
    lives_memmove(buff + 7 - len, buff + 8 - len, len + 4);
    if (lives_read_buffered(fd, &buff[11], 1, TRUE) < 1) return 0;
  }
  return 0;
}

/**
   @brief deserialise a weed leaf
   returns "type" on success or a -ve error code on failure

   WEED_LEAF_HOST_DEFAULT and WEED_LEAF_TYPE sets key; otherwise we leave it as NULL to get the next

   if the input 'type' is 0 (WEED_PLANT_UNKNOWN) then it is set to 'type', unless an error occurs

   if check_key set to TRUE - check that we read the correct key seed_type and n_elements

   return values:
   -1 : check_key key mismatch
   -2 : "type" leaf was not an INT
   -3 : "type" leaf has invalid element count
   -4 : short read
   -5 : memory allocation error
   -6 : unknown seed_type
   -7 : plant "type" mismatch
   -8 : 'type' was < 0
   -9 : key length mismatch
   - 10 : key length too long
   - 11 : data length too long
   -12 : data length too short
*/
static int weed_leaf_deserialise(int fd, weed_plant_t *plant, const char *key, unsigned char **mem,
                                 boolean check_key) {
  void **values = NULL;

  ssize_t bytes;

  int32_t *ints;
  double *dubs;
  int64_t *int64s;

  weed_size_t len;
  weed_size_t vlen;
  //weed_size_t vlen64;
  weed_size_t vlen64_tot = 0;

  char *mykey = NULL;
  char *msg;

  boolean add_leaf = TRUE;
  boolean check_ptrs = FALSE;
  int32_t st; // seed type
  int32_t ne; // num elems
  int32_t type = 0;
  int error;

  int i, j;

  if (!key && check_key) {
    check_ptrs = TRUE;
    check_key = FALSE;
  }

  if (!key || check_key) {
    // key length
    if (!mem) {
      if (lives_read_le_buffered(fd, &len, 4, TRUE) < 4) {
        return -4;
      }
    } else {
      lives_memcpy(&len, *mem, 4);
      *mem += 4;
    }

    if (check_key && len != lives_strlen(key)) {
      if (prefs->show_dev_opts)
        g_print("len for %s was %d\n", key, len);
      return -9;
    }
    if (len > MAX_WEED_STRLEN) return -10;

    mykey = (char *)lives_malloc((size_t)len + 1);
    if (!mykey) return -5;

    // read key
    if (!mem) {
      if (lives_read_buffered(fd, mykey, (size_t)len, TRUE) < len) {
        type = -4;
        goto done;
      }
    } else {
      lives_memcpy(mykey, *mem, (size_t)len);
      *mem += len;
    }
    lives_memset(mykey + (size_t)len, 0, 1);
    //g_print("got key %s\n", mykey);
    if (check_key && lives_strcmp(mykey, key)) {
      type = -1;
      goto done;
    }

    if (!key) key = mykey;
    else {
      lives_freep((void **)&mykey);
    }
  }

  if (!mem) {
    if (lives_read_le_buffered(fd, &st, 4, TRUE) < 4) {
      type = -4;
      goto done;
    }
  } else {
    lives_memcpy(&st, *mem, 4);
    *mem += 4;
  }
  if (st < 64 && (st != WEED_SEED_INT && st != WEED_SEED_BOOLEAN && st != WEED_SEED_DOUBLE && st != WEED_SEED_INT64 &&
                  st != WEED_SEED_STRING && st != WEED_SEED_VOIDPTR && st != WEED_SEED_PLANTPTR)) {
    if (prefs->show_dev_opts) {
      g_printerr("unknown seed type %d %s\n", st, mykey);
      BREAK_ME("ink. seed type in w.l. deser");
    }
    type = -6;
    goto done;
  }

  if (check_key && !strcmp(key, WEED_LEAF_TYPE)) {
    // for the WEED_LEAF_TYPE leaf perform some extra checks
    if (st != WEED_SEED_INT) {
      type = -2;
      goto done;
    }
  }

  if (!mem) {
    if (lives_read_le_buffered(fd, &ne, 4, TRUE) < 4) {
      type = -4;
      goto done;
    }
  } else {
    lives_memcpy(&ne, *mem, 4);
    *mem += 4;
  }
  if (ne > MAX_WEED_ELEMENTS) {
    type = -11;
    goto done;
  }

  if (!lives_strcmp(key, WEED_LEAF_PIXEL_DATA)) {
    //g_print("ne was %d\n", ne);
    if (ne > 4) {
      // max planes is 4 (YUVA4444P)
      for (j = ne ; j >= 0; lives_freep((void **)&values[j--]));
      values = NULL;
      type = -11;
      goto done;
    }
  }
  if (ne > 0) {
    values = (void **)lives_malloc(ne * sizeof(void *));
    if (!values) {
      type = -5;
      goto done;
    }
  } else values = NULL;

  if (check_key && !strcmp(key, WEED_LEAF_TYPE)) {
    // for the WEED_LEAF_TYPE leaf perform some extra checks
    if (ne != 1) {
      type = -3;
      goto done;
    }
  }

  // for pixel_data we do special handling
  if (!mem && !lives_strcmp(key, WEED_LEAF_PIXEL_DATA)) {
    int width, height, pal = 0, *rs = NULL, nplanes = ne;
    height = weed_layer_get_height(plant);
    if (height > 0) {
      width = weed_layer_get_width(plant);
      if (width > 0) {
        pal = weed_layer_get_palette(plant);
        if (pal > 0) {
          rs = weed_layer_get_rowstrides(plant, &nplanes);
          if (nplanes != ne) {
            LIVES_WARN("Invalid planes in retrieved layer");
          }
          lives_free(rs);
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

    for (j = 0; j < ne; j++) {
      bytes = lives_read_le_buffered(fd, &vlen, 4, TRUE);
      if (bytes < 4) {
        for (--j; j >= 0; lives_freep((void **)&values[j--]));
        values = NULL;
        type = -4;
        goto done;
      }

      if (j == 0 && vlen == 0) {
        int id;
        bytes = lives_read_le_buffered(fd, &id, 4, TRUE);
        if (id == 0x44454557) {
          weed_layer_t *layer = (weed_layer_t *)plant;
          int ver;
          uint64_t *vlen64 = lives_calloc(nplanes, 8);
          //size_t vlen64_tot = 0;
          int *rs = lives_calloc(nplanes, sizint);

          bytes = lives_read_le_buffered(fd, &ver, 4, TRUE);
          bytes = lives_read_le_buffered(fd, &nplanes, 4, TRUE);
          bytes = lives_read_le_buffered(fd, &pal, 4, TRUE);
          weed_layer_set_palette(layer, pal);
          bytes = lives_read_le_buffered(fd, &width, 4, TRUE);
          bytes = lives_read_le_buffered(fd, &height, 4, TRUE);
          weed_layer_set_size(layer, width, height);
          vlen64 = lives_calloc(nplanes, 8);
          rs = lives_calloc(nplanes, 4);
          for (int p = 0; p < nplanes; p++) {
            bytes = lives_read_le_buffered(fd, &rs[p], 4, TRUE);
            bytes = lives_read_le_buffered(fd, &vlen64[p], 8, TRUE);
            vlen64_tot += vlen64[p];
          }

          if (vlen64_tot > MAX_FRAME_SIZE64) {
            values = NULL;
            type = -11;
            lives_free(rs);
            lives_free(vlen64);
            goto done;
          }

          weed_layer_set_rowstrides(layer, rs, nplanes);

          lives_free(rs);

          //weed_layer_pixel_data_free(layer);
#define USE_BIGBLOCKS
#ifdef USE_BIGBLOCKS
          if ((values[0] = calloc_bigblock(vlen64_tot)))
            weed_set_boolean_value(plant, LIVES_LEAF_BBLOCKALLOC, WEED_TRUE);
          else
#endif
            values[0] = lives_calloc_align(vlen64_tot + EXTRA_BYTES);
          if (!values[0]) {
            msg = lives_strdup_printf("Could not allocate %d bytes for deserialised frame", vlen);
            LIVES_ERROR(msg);
            lives_free(msg);
            lives_free(vlen64);
            weed_set_voidptr_value(plant, WEED_LEAF_PIXEL_DATA, NULL);
            type = -5;
            goto done;
          }

          if (lives_read_buffered(fd, values[0], vlen64_tot, TRUE) != vlen64_tot) {
            ///// do something
            lives_free(values[0]);
            lives_free(values);
            values = NULL;
            lives_free(vlen64);
            type = -4;
            goto done;
          }
          for (i = 1; i < nplanes; i++) {
            values[i] = values[i - 1] + vlen64[i];
          }
          if (nplanes > 1)
            weed_set_boolean_value(plant, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
          lives_free(vlen64);
        } else {
          /// size 0, bad ID
          values = NULL;
          type = -12;
          goto done;
        }
      } else {
        //g_print("vlen was %d\n", vlen);
        if (vlen > MAX_FRAME_SIZE) {
          for (--j; j >= 0; lives_freep((void **)&values[j--]));
          values = NULL;
          type = -11;
          goto done;
        }
        if (rs && vlen != rs[j] * height * weed_palette_get_plane_ratio_vertical(pal, j)) {
          int xrs, xw, xh, psize = pixel_size(pal);
          xrs = vlen / height;
          xw = xrs / psize;
          xh = vlen / xrs;

          LIVES_WARN("invalid frame size recovering layer");
          msg = lives_strdup_printf(" for plane %d, size is %d. Should be %d. Will adjust rs to %d, width to %d "
                                    "and height to %d\n", j, vlen, rs[j] * height, xrs, xw, xh);
          LIVES_WARN(msg);
          lives_free(msg);
          weed_layer_set_width(plant, (float)xw / weed_palette_get_plane_ratio_horizontal(pal, j));
          weed_layer_set_height(plant, (float)xh / weed_palette_get_plane_ratio_vertical(pal, j));
          rs[j] = xrs;
          weed_layer_set_rowstrides(plant, rs, nplanes);
        }

        values[j] = lives_calloc_align(vlen);
        if (!values[j]) {
          msg = lives_strdup_printf("Could not allocate %d bytes for deserialised frame", vlen);
          LIVES_ERROR(msg);
          lives_free(msg);
          for (--j; j >= 0; j--) lives_free(values[j]);
          weed_set_voidptr_value(plant, WEED_LEAF_PIXEL_DATA, NULL);
          type = -5;
          goto done;
        }
        if (lives_read_buffered(fd, values[j], vlen, TRUE) != vlen) {
          // TODO...

        }
        if (j >= nplanes) lives_free(values[j]);
      }
    }
    if (plant) {
      weed_set_voidptr_array(plant, WEED_LEAF_PIXEL_DATA, nplanes, values);
      while (nplanes--) values[nplanes] = NULL; /// prevent "values" from being freed because we copy-by-value
    }
    goto done;
  } else {
    for (i = 0; i < ne; i++) {
      if (!mem) {
        bytes = lives_read_le_buffered(fd, &vlen, 4, TRUE);
        if (bytes < 4) {
          for (--i; i >= 0; lives_freep((void **)&values[i--]));
          values = NULL;
          type = -4;
          goto done;
        }
      } else {
        lives_memcpy(&vlen, *mem, 4);
        *mem += 4;
      }

      if (st == WEED_SEED_STRING) {
        if (vlen > MAX_WEED_STRLEN) {
          for (--i; i >= 0; lives_freep((void **)&values[i--]));
          values = NULL;
          type = -11;
          goto done;
        }
        values[i] = lives_malloc((size_t)vlen + 1);
      } else {
        if (vlen == 0) {
          if (st >= 64) {
            values[i] = NULL;
          } else {
            values = NULL;
            type = -4;
            goto done;
          }
        } else values[i] = lives_malloc((size_t)vlen);
      }
      if (vlen > 0) {
        if (!mem) {
          if (st != WEED_SEED_STRING)
            bytes = lives_read_le_buffered(fd, values[i], vlen, TRUE);
          else
            bytes = lives_read_buffered(fd, values[i], vlen, TRUE);
          if (bytes < vlen) {
            for (--i; i >= 0; lives_freep((void **)&values[i--]));
            values = NULL;
            type = -4;
            goto done;
          }
        } else {
          lives_memcpy(values[i], *mem, vlen);
          *mem += vlen;
        }
      }
      if (st == WEED_SEED_STRING) {
        lives_memset((char *)values[i] + vlen, 0, 1);
      }
    }
  }

  if (!strcmp(key, WEED_LEAF_TYPE)) {
    type = *(int32_t *)(values[0]);
    if (type < 0) return -8;
  }
  if (plant) {
    if (!values) {
      weed_leaf_set(plant, key, st, 0, NULL);
    } else {
      switch (st) {
      case WEED_SEED_INT:
      // fallthrough
      case WEED_SEED_BOOLEAN:
        ints = (int32_t *)lives_malloc(ne * 4);
        for (j = 0; j < ne; j++) ints[j] = *(int32_t *)values[j];
        if (!strcmp(key, WEED_LEAF_TYPE)) {
          if (weed_plant_has_leaf(plant, WEED_LEAF_TYPE)) {
            type = weed_get_int_value(plant, WEED_LEAF_TYPE, &error);
            if (type == WEED_PLANT_UNKNOWN) {
              type = weed_plant_mutate(plant, *ints);
            } else {
              if (*ints != type) {
                msg = lives_strdup_printf("Type mismatch in deserialization: expected %d, got %d\n", type, *ints);
                lives_free(ints); LIVES_ERROR(msg); lives_free(msg);
                type = -7;
                goto done;
              }
              // type already OK
            }
          } else  weed_leaf_set(plant, key, st, ne, (void *)ints);
        } else {
          if (check_ptrs) {
            if (is_pixdata_nullify_leaf(key) || is_no_copy_leaf(key)) add_leaf = FALSE;
          }
          if (add_leaf) weed_leaf_set(plant, key, st, ne, (void *)ints);
        }
        lives_freep((void **)&ints);
        break;
      case WEED_SEED_DOUBLE:
        dubs = (double *)lives_malloc(ne * sizdbl);
        for (j = 0; j < ne; j++) dubs[j] = *(double *)values[j];
        weed_leaf_set(plant, key, st, ne, (void *)dubs);
        lives_freep((void **)&dubs);
        break;
      case WEED_SEED_INT64:
        int64s = (int64_t *)lives_malloc(ne * 8);
        for (j = 0; j < ne; j++) int64s[j] = *(int64_t *)values[j];
        weed_leaf_set(plant, key, st, ne, (void *)int64s);
        lives_freep((void **)&int64s);
        break;
      case WEED_SEED_STRING:
        weed_leaf_set(plant, key, st, ne, (void *)values);
        break;
      default:
        if (plant) {
          if (check_ptrs) {
            switch (weed_plant_get_type(plant)) {
            case WEED_PLANT_LAYER:
              if (lives_strcmp(key, WEED_LEAF_PIXEL_DATA))
                add_leaf = FALSE;
              break;
            default: break;
            }
          }
          if (add_leaf) {
            if (!mem && prefs->force64bit) {
              // force pointers to uint64_t
              uint64_t *voids = (uint64_t *)lives_malloc(ne * sizeof(uint64_t));
              for (j = 0; j < ne; j++) voids[j] = (uint64_t)(*(void **)values[j]);
              weed_leaf_set(plant, key, WEED_SEED_INT64, ne, (void *)voids);
              lives_freep((void **)&voids);
            } else {
              void **voids = (void **)lives_malloc(ne * sizeof(void *));
              for (j = 0; j < ne; j++) voids[j] = values[j];
              weed_leaf_set(plant, key, st, ne, (void *)voids);
              lives_freep((void **)&voids);
	      // *INDENT-OFF*
	    }}}}}}
  // *INDENT-ON*

done:

  if (values) {
    for (i = 0; i < ne; i++) lives_freep((void **)&values[i]);
    lives_freep((void **)&values);
  }
  lives_freep((void **)&mykey);

  return type;
}


weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem, weed_plant_t *plant) {
  // if plant is NULL we create a new one
  // deserialise a plant from file fd or mem
  int numleaves;
  ssize_t bytes;
  int err;
  int32_t type = WEED_PLANT_UNKNOWN;
  boolean newd = FALSE, retried = FALSE;
  boolean block_unk_ptrs = FALSE;
  int bugfd = -1;

realign:

  // caller should clear and check THREADVAR(read_failed)
  if (!mem) {
    if (fd == bugfd) {
      if (!plant) {
        // create a new plant with type unknown
        plant = weed_plant_new(WEED_PLANT_UNKNOWN);
        newd = TRUE;
      }
      numleaves = realign_typeleaf(fd, plant);
      if (numleaves == 0 || (type = weed_plant_get_type(plant)) <= 0) {
        if (newd) weed_plant_free(plant);
        return NULL;
      }
      bugfd = -1;
    } else {
      if ((bytes = lives_read_le_buffered(fd, &numleaves, 4, TRUE)) < 4) {
        bugfd = -1;
        return NULL;
      }
    }
  } else {
    lives_memcpy(&numleaves, *mem, 4);
    *mem += 4;
  }

  if (!plant) {
    // create a new plant with type unknown
    plant = weed_plant_new(type);
    newd = TRUE;
  }

  if (type == WEED_PLANT_UNKNOWN) {
    if ((type = weed_leaf_deserialise(fd, plant, WEED_LEAF_TYPE, mem, TRUE)) <= 0) {
      // check the WEED_LEAF_TYPE leaf first
      if (type != -5) {  // all except malloc error
        char *badfname = filename_from_fd(NULL, fd);
        char *msg = lives_strdup_printf("Data mismatch (%d) reading from\n%s", type, badfname ? badfname : "unknown file");
        LIVES_ERROR(msg);
        lives_free(msg);
        bugfd = fd;
      }
      if (newd) {
        weed_plant_free(plant);
        plant = NULL;
      }
      if (bugfd == fd && !retried) {
        retried = TRUE;
        goto realign;
      }
      return NULL;
    }
  }

  numleaves--;

  /// if key is NULL and check_key is TRUE, we will limit whay voidptr leaves can be added. This is necessary for layers
  /// since voidptrs which may be loaded can mess up the layer handling
  if (type == WEED_PLANT_LAYER) block_unk_ptrs = TRUE;

  while (numleaves--) {
    if ((err = weed_leaf_deserialise(fd, plant, NULL, mem, block_unk_ptrs)) != 0) {
      if (err != -5) {  // all except malloc error
        char *badfname = filename_from_fd(NULL, fd);
        char *msg = lives_strdup_printf("Data mismatch (%d) reading from\n%s", err, badfname ? badfname : "unknown file");
        LIVES_ERROR(msg);
        lives_free(msg);
        bugfd = fd;
      }
      if (newd) {
        weed_plant_free(plant);
        plant = NULL;
      }
      if (bugfd == fd && !retried) {
        retried = TRUE;
        goto realign;
      }
      return NULL;
    }
  }

  if ((type = weed_get_plant_type(plant)) == WEED_PLANT_UNKNOWN) {
    //g_print("badplant was %d\n", type);
    if (newd) weed_plant_free(plant);
    return NULL;
  }
  //g_print("goodplant %p\n", plant);
  bugfd = -1;
  return plant;
}


boolean write_filter_defaults(int fd, int idx) {
  // return FALSE on write error
  char *hashname;
  weed_plant_t *filter = weed_filters[idx], **ptmpls;
  int num_params;
  int i;
  boolean wrote_hashname = FALSE;
  size_t vlen;
  int ntowrite = 0;

  ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
  if (!ptmpls) return TRUE;

  for (i = 0; i < num_params; i++) {
    if (weed_plant_has_leaf(ptmpls[i], WEED_LEAF_HOST_DEFAULT)) {
      ntowrite++;
    }
  }

  for (i = 0; i < num_params; i++) {
    if (weed_plant_has_leaf(ptmpls[i], WEED_LEAF_HOST_DEFAULT)) {
      if (!wrote_hashname) {
        hashname = make_weed_hashname(idx, TRUE, FALSE, 0, FALSE);
        vlen = lives_strlen(hashname);

        lives_write_le_buffered(fd, &vlen, 4, TRUE);
        lives_write_buffered(fd, hashname, vlen, TRUE);
        lives_freep((void **)&hashname);
        wrote_hashname = TRUE;
        lives_write_le_buffered(fd, &ntowrite, 4, TRUE);
      }
      lives_write_le_buffered(fd, &i, 4, TRUE);
      weed_leaf_serialise(fd, ptmpls[i], WEED_LEAF_HOST_DEFAULT, FALSE, NULL);
    }
  }
  if (wrote_hashname) lives_write_buffered(fd, "\n", 1, TRUE);

  lives_freep((void **)&ptmpls);

  if (THREADVAR(write_failed) == fd + 1) {
    THREADVAR(write_failed) = 0;
    return FALSE;
  }
  return TRUE;
}


boolean read_filter_defaults(int fd) {
  weed_plant_t *filter, **ptmpls;
  void *buf = NULL;

  size_t vlen;

  int vleni, vlenz;
  int i, pnum;
  int num_params = 0;
  int ntoread;
  boolean read_failed = FALSE;
  boolean found = FALSE;

  char *tmp;

  while (1) {
    if (lives_read_le_buffered(fd, &vleni, 4, TRUE) < 4) {
      break;
    }

    // some files erroneously used a vlen of 8
    if (lives_read_le_buffered(fd, &vlenz, 4, TRUE) < 4) {
      break;
    }

    vlen = (size_t)vleni;

    if (capable->hw.byte_order == LIVES_BIG_ENDIAN && prefs->bigendbug) {
      if (vleni == 0 && vlenz != 0) vlen = (size_t)vlenz;
    } else {
      if (vlenz != 0) {
        if (lives_lseek_buffered_rdonly(fd, -4) < 0) return FALSE;
      }
    }

    if (vlen > MAX_WEED_STRLEN) return FALSE;

    buf = lives_malloc(vlen + 1);
    if (lives_read_buffered(fd, buf, vlen, TRUE) < vlen) break;

    lives_memset((char *)buf + vlen, 0, 1);
    for (i = 0; i < num_weed_filters; i++) {
      if (!lives_strcmp((char *)buf, (tmp = make_weed_hashname(i, TRUE, FALSE, 0, FALSE)))) {
        lives_free(tmp);
        found = TRUE;
        break;
      }
      lives_free(tmp);
    }
    if (!found) {
      for (i = 0; i < num_weed_filters; i++) {
        if (!lives_strcmp((char *)buf, (tmp = make_weed_hashname(i, TRUE, TRUE, 0, FALSE)))) {
          lives_free(tmp);
          break;
        }
        lives_free(tmp);
      }
    }

    lives_freep((void **)&buf);

    ptmpls = NULL;

    if (i < num_weed_filters) {
      filter = weed_filters[i];
      ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
    } else num_params = 0;

    if (lives_read_le_buffered(fd, &ntoread, 4, TRUE) < 4) {
      if (ptmpls) lives_free(ptmpls);
      break;
    }

    for (i = 0; i < ntoread; i++) {
      if (lives_read_le_buffered(fd, &pnum, 4, TRUE) < 4) {
        if (ptmpls) lives_free(ptmpls);
        break;
      }

      if (pnum < num_params) {
        if (weed_leaf_deserialise(fd, ptmpls[pnum], WEED_LEAF_HOST_DEFAULT, NULL, FALSE) < 0) {
        }
      } else {
        weed_plant_t *dummyplant = weed_plant_new(WEED_PLANT_UNKNOWN);
        if (weed_leaf_deserialise(fd, dummyplant, WEED_LEAF_HOST_DEFAULT, NULL, FALSE) < 0) {
          weed_plant_free(dummyplant);
          read_failed = TRUE;
          break;
        }
      }
      if (ptmpls && weed_paramtmpl_value_irrelevant(ptmpls[pnum]))
        weed_leaf_delete(ptmpls[pnum], WEED_LEAF_HOST_DEFAULT);
    }
    if (read_failed) {
      if (ptmpls) lives_free(ptmpls);
      break;
    }

    buf = lives_malloc(strlen("\n"));
    lives_read_buffered(fd, buf, strlen("\n"), TRUE);
    lives_free(buf);
    if (ptmpls) lives_free(ptmpls);
    if (THREADVAR(read_failed) == fd + 1) {
      break;
    }
  }

  if (THREADVAR(read_failed) == fd + 1) {
    THREADVAR(read_failed) = 0;
    return FALSE;
  }

  return TRUE;
}


boolean write_generator_sizes(int fd, int idx) {
  // TODO - handle optional channels
  // return FALSE on write error
  char *hashname;
  weed_plant_t *filter, **ctmpls;
  int num_channels;
  int i;
  size_t vlen;
  boolean wrote_hashname = FALSE;

  num_channels = enabled_in_channels(weed_filters[idx], FALSE);
  if (num_channels != 0) return TRUE;

  filter = weed_filters[idx];

  ctmpls = weed_filter_get_out_chantmpls(filter, &num_channels);
  if (num_channels == 0) return TRUE;

  for (i = 0; i < num_channels; i++) {
    if (weed_plant_has_leaf(ctmpls[i], WEED_LEAF_HOST_WIDTH) || weed_plant_has_leaf(ctmpls[i], WEED_LEAF_HOST_HEIGHT) ||
        (!wrote_hashname && weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS))) {
      if (!wrote_hashname) {
        hashname = make_weed_hashname(idx, TRUE, FALSE, 0, FALSE);
        vlen = lives_strlen(hashname);
        lives_write_le_buffered(fd, &vlen, 4, TRUE);
        lives_write_buffered(fd, hashname, vlen, TRUE);
        lives_free(hashname);
        wrote_hashname = TRUE;
        if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS)) {
          int j = -1;
          lives_write_le_buffered(fd, &j, 4, TRUE);
          weed_leaf_serialise(fd, filter, WEED_LEAF_HOST_FPS, FALSE, NULL);
        }
      }

      lives_write_le_buffered(fd, &i, 4, TRUE);
      if (weed_plant_has_leaf(ctmpls[i], WEED_LEAF_HOST_WIDTH)) weed_leaf_serialise(fd, ctmpls[i],
            WEED_LEAF_HOST_WIDTH, FALSE, NULL);
      else weed_leaf_serialise(fd, ctmpls[i], WEED_LEAF_WIDTH, FALSE, NULL);
      if (weed_plant_has_leaf(ctmpls[i], WEED_LEAF_HOST_HEIGHT)) weed_leaf_serialise(fd, ctmpls[i],
            WEED_LEAF_HOST_HEIGHT, FALSE, NULL);
      else weed_leaf_serialise(fd, ctmpls[i], WEED_LEAF_HEIGHT, FALSE, NULL);
    }
  }
  if (wrote_hashname) lives_write_buffered(fd, "\n", 1, TRUE);

  if (THREADVAR(write_failed) == fd + 1) {
    THREADVAR(write_failed) = 0;
    return FALSE;
  }
  return TRUE;
}


boolean read_generator_sizes(int fd) {
  weed_plant_t *filter, **ctmpls;
  ssize_t bytes;
  size_t vlen;

  char *buf;
  char *tmp;

  boolean found = FALSE;
  boolean ready;

  int vleni, vlenz;
  int i, num_chans = 0, cnum;

  while (1) {
    if (lives_read_le_buffered(fd, &vleni, 4, TRUE) < 4) {
      break;
    }

    // some files erroneously used a vlen of 8
    if (lives_read_le_buffered(fd, &vlenz, 4, TRUE) < 4) {
      break;
    }

    vlen = (size_t)vleni;

    if (capable->hw.byte_order == LIVES_BIG_ENDIAN && prefs->bigendbug) {
      if (vleni == 0 && vlenz != 0) vlen = (size_t)vlenz;
    } else {
      if (vlenz != 0) {
        if (lives_lseek_buffered_rdonly(fd, -4) < 0) {
          return FALSE;
        }
      }
    }

    if (vlen > MAX_WEED_STRLEN) {
      return FALSE;
    }

    buf = (char *)lives_malloc(vlen + 1);

    bytes = lives_read_buffered(fd, buf, vlen, TRUE);
    if (bytes < vlen) {
      break;
    }
    lives_memset((char *)buf + vlen, 0, 1);

    for (i = 0; i < num_weed_filters; i++) {
      if (!lives_strcmp(buf, (tmp = make_weed_hashname(i, TRUE, FALSE, 0, FALSE)))) {
        lives_free(tmp);
        found = TRUE;
        break;
      }
      lives_free(tmp);
    }
    if (!found) {
      for (i = 0; i < num_weed_filters; i++) {
        if (!lives_strcmp(buf, (tmp = make_weed_hashname(i, TRUE, TRUE, 0, FALSE)))) {
          lives_free(tmp);
          break;
        }
        lives_free(tmp);
      }
    }

    lives_free(buf);
    ctmpls = NULL;

    ready = FALSE;
    filter = NULL;
    if (i < num_weed_filters) {
      filter = weed_filters[i];
      ctmpls = weed_filter_get_out_chantmpls(filter, &num_chans);
    }
    while (!ready) {
      ready = TRUE;
      bytes = lives_read_le_buffered(fd, &cnum, 4, TRUE);
      if (bytes < 4) {
        break;
      }
      if (!filter) {
        // we still need to read past the values even if we didn't find the filter
        if (cnum == -1) {
          if (weed_leaf_deserialise(fd, NULL, WEED_LEAF_HOST_FPS, NULL, FALSE) < 0) break;
          ready = FALSE;
        } else {
          if (weed_leaf_deserialise(fd, NULL, WEED_LEAF_HOST_WIDTH, NULL, FALSE) < 0) break;
          if (weed_leaf_deserialise(fd, NULL, WEED_LEAF_HOST_HEIGHT, NULL, FALSE) < 0) break;
        }
      } else {
        if (cnum < num_chans && cnum >= 0) {
          if (weed_leaf_deserialise(fd, ctmpls[cnum], WEED_LEAF_HOST_WIDTH, NULL, FALSE) < 0) break;
          if (weed_leaf_deserialise(fd, ctmpls[cnum], WEED_LEAF_HOST_HEIGHT, NULL, FALSE) < 0) break;
          if (weed_get_int_value(ctmpls[cnum], WEED_LEAF_HOST_WIDTH, NULL) == 0)
            weed_set_int_value(ctmpls[cnum], WEED_LEAF_HOST_WIDTH, DEF_GEN_WIDTH);
          if (weed_get_int_value(ctmpls[cnum], WEED_LEAF_HOST_HEIGHT, NULL) == 0)
            weed_set_int_value(ctmpls[cnum], WEED_LEAF_HOST_HEIGHT, DEF_GEN_HEIGHT);
        } else if (cnum == -1) {
          if (weed_leaf_deserialise(fd, filter, WEED_LEAF_HOST_FPS, NULL, FALSE) < 0) break;
          ready = FALSE;
        }
      }
    }

    lives_freep((void **)&ctmpls);

    if (THREADVAR(read_failed) == fd + 1) {
      break;
    }
    buf = (char *)lives_malloc(strlen("\n"));
    lives_read_buffered(fd, buf, strlen("\n"), TRUE);
    lives_free(buf);

    if (THREADVAR(read_failed) == fd + 1) {
      break;
    }
  }

  if (THREADVAR(read_failed) == fd + 1) {
    THREADVAR(read_failed) = 0;
    return FALSE;
  }
  return TRUE;
}

// key/mode parameter defaults

boolean read_key_defaults(int fd, int nparams, int key, int mode, int ver) {
  // read default param values for key/mode from file

  // if key < 0 we just read past the bytes in the file

  // return FALSE on EOF

  int i, j, nvals, nigns;
  int idx;
  ssize_t bytes;
  weed_plant_t *filter;
  int xnparams = 0, maxparams = nparams;
  weed_plant_t **key_defs;
  weed_timecode_t tc;

  int ret = 0;

  if (key >= 0) {
    idx = key_to_fx[key][mode];
    filter = weed_filters[idx];
    maxparams = xnparams = num_in_params(filter, FALSE, FALSE);
  }

  if (xnparams > maxparams) maxparams = xnparams;
  if (!maxparams) return FALSE;

  key_defs = (weed_plant_t **)lives_calloc(maxparams, sizeof(weed_plant_t *));

  for (i = 0; i < nparams; i++) {
    if (key < 0 || i < xnparams) {
      key_defs[i] = weed_plant_new(WEED_PLANT_PARAMETER);
    }
    if (ver > 1) {
      // for future - read nvals
      bytes = lives_read_le_buffered(fd, &nvals, 4, TRUE);
      if (bytes < 4) {
        goto err123;
      }
      if (!nvals) return TRUE;
      bytes = lives_read_le_buffered(fd, &tc, 8, TRUE);
      if (bytes < sizeof(weed_timecode_t)) {
        goto err123;
      }
      // read n ints (booleans)
      bytes = lives_read_le_buffered(fd, &nigns, 4, TRUE);
      if (bytes < 4) {
        goto err123;
      }
      if (nigns > 0) {
        int *igns = (int *)lives_malloc(nigns * 4);
        for (j = 0; j < nigns; j++) {
          bytes = lives_read_le_buffered(fd, &igns[j], 4, TRUE);
          if (bytes < 4) {
            goto err123;
          }
        }
        lives_free(igns);
      }
    }

    if (weed_leaf_deserialise(fd, key_defs[i], WEED_LEAF_VALUE, NULL, FALSE) < 0) goto err123;

    if (ver > 1) {
      for (j = 0; j < nvals; j++) {
        // for future - read timecodes
        weed_plant_t *plant = weed_plant_new(WEED_PLANT_PARAMETER);
        bytes = lives_read_le_buffered(fd, &tc, 8, TRUE);
        if (bytes < 8) {
          goto err123;
        }
        // read n ints (booleans)
        bytes = lives_read_le_buffered(fd, &nigns, 4, TRUE);
        if (bytes < 4) {
          goto err123;
        }
        if (nigns > 0) {
          int *igns = (int *)lives_malloc(nigns * 4);
          for (j = 0; j < nigns; j++) {
            bytes = lives_read_le_buffered(fd, &igns[j], 4, TRUE);
            if (bytes < 4) {
              goto err123;
            }
          }
          // discard excess values
          ret = weed_leaf_deserialise(fd, plant, WEED_LEAF_VALUE, NULL, FALSE);
          weed_plant_free(plant);
          if (ret < 0) goto err123;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (key >= 0) key_defaults[key][mode] = key_defs;

err123:
  if (key < 0) {
    for (i = 0; i < nparams; i++) {
      if (key_defs[i]) weed_plant_free(key_defs[i]);
    }
    lives_free(key_defs);
  }

  if (ret < 0 || THREADVAR(read_failed) == fd + 1) {
    THREADVAR(read_failed) = 0;
    return FALSE;
  }

  return TRUE;
}


void apply_key_defaults(weed_plant_t *inst, int key, int mode) {
  // apply key/mode param defaults to a filter instance
  weed_plant_t **defs, **params;
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
  int nparams = num_in_params(filter, FALSE, FALSE);
  int i, j = 0;

  if (nparams == 0) return;

  defs = key_defaults[key][mode];
  if (!defs) return;

  do {
    params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
    nparams = num_in_params(inst, FALSE, FALSE);

    for (i = 0; i < nparams; i++) {
      if (!is_hidden_param(inst, i))
        lives_leaf_dup(params[i], defs[j], WEED_LEAF_VALUE);
      j++;
    }
    lives_free(params);
  } while ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)));
}


void write_key_defaults(int fd, int key, int mode) {
  // save key/mode param defaults to file
  // caller should check for write errors

  weed_plant_t *filter;
  weed_plant_t **key_defs;
  int nparams = 0;

  if ((key_defs = key_defaults[key][mode]) == NULL) {
    lives_write_le_buffered(fd, &nparams, 4, TRUE);
    return;
  }

  filter = weed_filters[key_to_fx[key][mode]];
  nparams = num_in_params(filter, FALSE, FALSE);
  lives_write_le_buffered(fd, &nparams, 4, TRUE);

  for (int i = 0; i < nparams; i++) {
    if (THREADVAR(write_failed) == fd + 1) {
      THREADVAR(write_failed) = 0;
      break;
    }
    weed_leaf_serialise(fd, key_defs[i], WEED_LEAF_VALUE, FALSE, NULL);
  }
}


void free_key_defaults(int key, int mode) {
  // free key/mode param defaults
  weed_plant_t *filter;
  weed_plant_t **key_defs;
  int nparams;

  if (key >= FX_KEYS_MAX_VIRTUAL || mode >= prefs->rte_modes_per_key) return;

  key_defs = key_defaults[key][mode];

  if (!key_defs) return;

  filter = weed_filters[key_to_fx[key][mode]];
  nparams = num_in_params(filter, FALSE, FALSE);

  for (int i = 0; i < nparams; i++) if (key_defs[i]) weed_plant_free(key_defs[i]);

  lives_free(key_defaults[key][mode]);
  key_defaults[key][mode] = NULL;
}


void set_key_defaults(weed_plant_t *inst, int key, int mode) {
  // copy key/mode param defaults from an instance
  weed_plant_t **key_defs, **params;
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
  int i = -1;
  int nparams, poffset = 0;

  if (key_defaults[key][mode]) free_key_defaults(key, mode);

  nparams = num_in_params(filter, FALSE, FALSE);
  if (nparams == 0) return;

  key_defs = (weed_plant_t **)lives_malloc(nparams * sizeof(weed_plant_t *));

  do {
    nparams = num_in_params(inst, FALSE, FALSE);
    if (nparams > 0) {
      int last = nparams + poffset - 1;
      params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
      while (i < last) {
        key_defs[++i] = weed_plant_new(WEED_PLANT_PARAMETER);
        lives_leaf_dup(key_defs[i], params[i - poffset], WEED_LEAF_VALUE);
      }
      lives_free(params);
      poffset = ++last;
    }
  } while ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)));

  key_defaults[key][mode] = key_defs;
}


boolean has_key_defaults(void) {
  // check if any key/mode has default parameters set
  for (int i = 0; i < prefs->rte_keys_virtual; i++) {
    for (int j = 0; j < prefs->rte_modes_per_key; j++) {
      if (key_defaults[i][j]) return TRUE;
    }
  }
  return FALSE;
}


#define AUTOTRANS_STEP .01

void set_trans_amt(int key, int mode, double * amt) {
  weed_plant_t *filter = weed_filters[key_to_fx[key][mode]];
  if (filter) {
    int trans = get_transition_param(filter, FALSE);
    if (trans != -1) {
      weed_plant_t *inst = weed_instance_obtain(key, mode);
      if (inst) {
        weed_plant_t *param = weed_inst_in_param(inst, trans, FALSE, FALSE);
        weed_plant_t *paramtmpl = weed_param_get_template(param);
        if (weed_param_get_value_type(param) == WEED_SEED_DOUBLE) {
          double mind = weed_get_double_value(paramtmpl, WEED_LEAF_MIN, NULL);
          double maxd = weed_get_double_value(paramtmpl, WEED_LEAF_MAX, NULL);
          double dval = mind + (maxd - mind) * *amt;
          weed_set_double_value(param, WEED_LEAF_VALUE, dval);
        } else {
          int mini = weed_get_int_value(paramtmpl, WEED_LEAF_MIN, NULL);
          int maxi = weed_get_int_value(paramtmpl, WEED_LEAF_MAX, NULL);
          int ival = (int)(mini + (double)(maxi - mini) * *amt + .5);
          weed_set_int_value(param, WEED_LEAF_VALUE, ival);
        }
        weed_instance_unref(inst);
        *amt += AUTOTRANS_STEP;
        if (*amt >= 1.) {
          int bfile = mainw->blend_file;
          *amt = -1.;
          rte_key_on_off(key + 1, FALSE);
          if (IS_VALID_CLIP(bfile)) switch_clip(1, bfile, FALSE);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
}


boolean set_autotrans(int clip) {
  // autotrans - switch on autotrans fx, increment trans param each frame
  // when it reaches max, then we switch
  int key = prefs->autotrans_key - 1;
  if (key >= 0 && key < prefs->rte_keys_virtual) {
    int mode = prefs->autotrans_mode >= 0 ? prefs->autotrans_mode : key_modes[key];
    weed_plant_t *filter = weed_filters[key_to_fx[key][mode]];
    if (filter) {
      if (!key_to_instance[key][mode]) {
        int trans = get_transition_param(filter, FALSE);
        if (trans != -1) {
          weed_plant_t *inst;
          key_modes[key] = mode;
          rte_key_on_off(key + 1, TRUE);
          inst = weed_instance_obtain(key, mode);
          if (inst) {
            if (mainw->blend_file != clip) {
              //weed_layer_set_invalid(mainw->blend_layer, TRUE);
              mainw->new_blend_file = clip;
            }
            prefs->autotrans_amt = 0.;
            set_trans_amt(key, mode, &prefs->autotrans_amt);
            weed_instance_unref(inst);
            pref_factory_bitmapped(PREF_AUDIO_OPTS, AUDIO_OPTS_IS_LOCKED, TRUE, FALSE);
            return TRUE;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*
  return FALSE;
}
