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

#include "weed-effects-utils.h"

///////////////////////////////////

#include "callbacks.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "ce_thumbs.h"
#include "paramwindow.h"

////////////////////////////////////////////////////////////////////////

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

struct _procvals {
  weed_process_f procfunc;
  weed_plant_t *inst;
  weed_timecode_t tc;
  weed_error_t ret;
  char padding[DEF_ALIGN - ((sizeof(weed_process_f) - sizeof(weed_plant_t *)
                             - sizeof(weed_timecode_t) - sizeof(weed_error_t)) % DEF_ALIGN)];
};

static weed_plantptr_t statsplant = NULL;

static int load_compound_fx(void);


#if 0
LIVES_LOCAL_INLINE int weed_inst_refs_count(weed_plant_t *inst) {
  int error;
  if (!inst) return -1;
  if (!weed_plant_has_leaf(inst, WEED_LEAF_HOST_REFS)) return 0;
  return weed_get_int_value(inst, WEED_LEAF_HOST_REFS, &error);
}
#endif
////////////////////////////////////////////////////////////////////////////

LIVES_GLOBAL_INLINE weed_error_t weed_leaf_copy_or_delete(weed_layer_t *dlayer, const char *key, weed_layer_t *slayer) {
  if (!weed_plant_has_leaf(slayer, key)) return weed_leaf_delete(dlayer, key);
  else return weed_leaf_copy(dlayer, key, slayer, key);
}


LIVES_GLOBAL_INLINE int filter_mutex_trylock(int key) {
#ifdef DEBUG_FILTER_MUTEXES
  int retval;
  g_print("trylock of %d\n", key);
#endif
  if (key < 0 || key >= FX_KEYS_MAX) {
    if (key != -1) {
      char *msg = lives_strdup_printf("attempted lock of bad fx key %d", key);
      LIVES_ERROR(msg);
      lives_free(msg);
    }
    return 0;
  }
#ifdef DEBUG_FILTER_MUTEXES
  retval = pthread_mutex_trylock(&mainw->fx_mutex[key]);
  g_print("trylock of %d returned %d\n", key, retval);
  return retval;
#endif
  return pthread_mutex_trylock(&mainw->fx_mutex[key]);
}

#ifndef DEBUG_FILTER_MUTEXES
LIVES_GLOBAL_INLINE int filter_mutex_lock(int key) {
  int ret = filter_mutex_trylock(key);
  /// attempted double locking is actually normal behaviour when we have both video and audio effects running
  /* if (ret != 0 && !mainw->is_exiting) { */
  /*   /\* char *msg = lives_strdup_printf("attempted double lock of fx key %d, err was %d", key, ret); *\/ */
  /*   /\* LIVES_ERROR(msg); *\/ */
  /*   /\* lives_free(msg); *\/ */
  /* } */
  return ret;
}


LIVES_GLOBAL_INLINE int filter_mutex_unlock(int key) {
  if (key >= 0 && key < FX_KEYS_MAX) {
    int ret = pthread_mutex_unlock(&mainw->fx_mutex[key]);
    if (ret != 0 && !mainw->is_exiting) {
      char *msg = lives_strdup_printf("attempted double unlock of fx key %d, err was %d", key, ret);
      LIVES_ERROR(msg);
      lives_free(msg);
    }
    return ret;
  } else {
    if (key != -1) {
      char *msg = lives_strdup_printf("attempted unlock of bad fx key %d", key);
      LIVES_ERROR(msg);
      lives_free(msg);
    }
  }
  return 0;
}
#endif


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
  register int i;

  plist = weed_chantmpl_get_palette_list(filter, ctmpl, &npals);
  if (!plist) return TRUE; ///< probably audio
  for (i = 0; i < npals; i++) {
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
  register int i;

  plist = weed_chantmpl_get_palette_list(filter, ctmpl, &npals);
  for (i = 0; i < npals; i++) {
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


static boolean all_outs_alpha(weed_plant_t *filt, boolean ign_opt) {
  // check (mandatory) output chans, see if any are non-alpha
  int nouts;
  weed_plant_t **ctmpls = weed_filter_get_out_chantmpls(filt, &nouts);
  if (!nouts) return FALSE;
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
  if (nins == 0) return FALSE;
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
  register int i;

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
  int32_t hash;
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
static void *filter_map[FX_KEYS_MAX + 2];
static int next_free_key;

////////////////////////////////////////////////////////////////////


void backup_weed_instances(void) {
  // this is called during multitrack rendering.
  // We are rendering, but we want to display the current frame in the preview window
  // thus we backup our rendering instances, apply the current frame instances, and then restore the rendering instances
  register int i;

  for (i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    key_to_instance_copy[i][0] = key_to_instance[i][0];
    key_to_instance[i][0] = NULL;
  }
}


void restore_weed_instances(void) {
  register int i;
  for (i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
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
  case AUD_PLAYER_SOX:
    weed_set_string_value(hinfo, WEED_LEAF_HOST_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
    break;
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


static weed_plant_t *get_enabled_channel_inner(weed_plant_t *inst, int which, boolean is_in, boolean audio_only) {
  // plant is a filter_instance
  // "which" starts at 0
  weed_plant_t **channels = NULL;
  weed_plant_t *retval, *ctmpl = NULL;

  int nchans = 3;

  int i = 0;

  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return NULL;

  if (is_in) {
    channels = weed_instance_get_in_channels(inst, &nchans);
  } else {
    channels = weed_instance_get_out_channels(inst, &nchans);
  }

  if (!nchans) return NULL;

  while (1) {
    if (weed_get_boolean_value(channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE) {
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


weed_plant_t *get_enabled_channel(weed_plant_t *inst, int which, boolean is_in) {
  // count audio and video channels
  return get_enabled_channel_inner(inst, which, is_in, FALSE);
}


weed_plant_t *get_enabled_audio_channel(weed_plant_t *inst, int which, boolean is_in) {
  // count only audio channels
  return get_enabled_channel_inner(inst, which, is_in, TRUE);
}


weed_plant_t *get_mandatory_channel(weed_plant_t *filter, int which, boolean is_in) {
  // plant is a filter_class
  // "which" starts at 0
  weed_plant_t **ctmpls;
  weed_plant_t *retval;
  register int i = 0;

  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;

  if (is_in) ctmpls = weed_filter_get_in_chantmpls(filter, NULL);
  else ctmpls = weed_filter_get_out_chantmpls(filter, NULL);

  if (!ctmpls) return NULL;
  while (which > -1) {
    if (!weed_chantmpl_is_optional(ctmpls[i])) which--;
    i++;
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

  for (register int i = 0; i < nchans; i++) {
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

  for (register int i = 0; i < nchans; i++) {
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
  weed_plant_t **chans, *ctmpl;
  int flags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, NULL);
  int nchans;

  if (flags & WEED_PARAMETER_VARIABLE_SIZE) return TRUE;

  if (!inst) return FALSE;

  if (!(flags & WEED_PARAMETER_VALUE_PER_CHANNEL)) return FALSE;

  if ((nchans = weed_leaf_num_elements(inst, WEED_LEAF_IN_CHANNELS)) == 0)
    return FALSE;

  chans = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL);

  for (int i = 0; i < nchans; i++) {
    if (weed_get_boolean_value(chans[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE) continue; //ignore disabled channels
    ctmpl = weed_get_plantptr_value(chans[i], WEED_LEAF_TEMPLATE, NULL);
    if (weed_plant_has_leaf(ctmpl, WEED_LEAF_MAX_REPEATS) && weed_get_int_value(ctmpl, WEED_LEAF_MAX_REPEATS, NULL) != 1) {
      lives_free(chans);
      return TRUE;
    }
  }

  lives_freep((void **)&chans);
  return FALSE;
}


/**
   @brief create an effect (filter) map

   here we create an effect map which defines the order in which effects are applied to a frame stack
   this is done during recording, the keymap is from mainw->rte which is a bitmap of effect keys
   keys are applied here from smallest (ctrl-1) to largest (virtual key ctrl-FX_KEYS_MAX_VIRTUAL)

   this is transformed into filter_map which holds init_events
   these pointers are then stored in a filter_map event

   what we actually point to are the init_events for the effects. The init_events are stored when we
   init an effect

   during rendering we read the filter_map event, and retrieve the new key, which is at that time
   held in the
   WEED_LEAF_HOST_TAG property of the init_event, and we apply our effects
   (which are then bound to virtual keys >=FX_KEYS_MAX_VIRTUAL)

   [note] that we can do cool things, like mapping the same instance multiple times (though it will always
   apply itself to the same in/out tracks

   we don't need to worry about free()ing init_events, since they will be free'd
   when the instance is deinited
*/
static void create_filter_map(uint64_t rteval) {
  int count = 0, i;
  weed_plant_t *inst;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rteval & (GU641 << i) && (inst = weed_instance_obtain(i, key_modes[i])) != NULL) {
      if (enabled_in_channels(inst, FALSE) > 0) {
        filter_map[count++] = init_events[i];
      }
      weed_instance_unref(inst);
    }
  }
  filter_map[count] = NULL; ///< marks the end of the effect map
}


/**
   @brief add effect_deinit events to an event_list

    during real time recording we use the "keys" 0 -> FX_KEYS_MAX_VIRTUAL
    here we add effect_deinit events to an event_list

    @see deinit_render_effects()
*/
weed_plant_t *add_filter_deinit_events(weed_plant_t *event_list) {
  // should be called with mainw->event_list_mutex unlocked !
  int i;
  boolean needs_filter_map = FALSE;
  weed_timecode_t last_tc = 0;

  if (event_list) last_tc = get_event_timecode(get_last_event(event_list));

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (init_events[i]) {
      event_list = append_filter_deinit_event(event_list, last_tc, init_events[i], pchains[i]);
      init_events[i] = NULL;
      // no freep !
      if (pchains[i]) lives_free(pchains[i]);
      pchains[i] = NULL;
      needs_filter_map = TRUE;
    }
  }

  /// add an empty filter_map event (in case more frames are added)
  create_filter_map(mainw->rte); ///< we create filter_map event_t * array with ordered effects
  if (needs_filter_map) {
    pthread_mutex_lock(&mainw->event_list_mutex);
    event_list = append_filter_map_event(event_list, last_tc, filter_map);
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
  int i;
  weed_plant_t *inst;
  int fx_idx, ntracks;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if ((inst = weed_instance_obtain(i, key_modes[i])) != NULL) {
      if (enabled_in_channels(inst, FALSE) > 0) {
        if (!weed_plant_has_leaf(inst, WEED_LEAF_RANDOM_SEED))
          weed_set_int64_value(inst, WEED_LEAF_RANDOM_SEED, gen_unique_id());
        event_list = append_filter_init_event(event_list, tc,
                                              (fx_idx = key_to_fx[i][key_modes[i]]), -1, i, inst);
        init_events[i] = get_last_event(event_list);
        ntracks = weed_leaf_num_elements(init_events[i], WEED_LEAF_IN_TRACKS);
        pchains[i] = filter_init_add_pchanges(event_list, inst, init_events[i], ntracks, 0);
      }
      weed_instance_unref(inst);
    }
  }
  /// add an empty filter_map event (in case more frames are added)
  create_filter_map(mainw->rte); /// we create filter_map event_t * array with ordered effects
  if (filter_map[0]) event_list = append_filter_map_event(event_list, tc, filter_map);
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

  for (int i = 0; (num_palettes > 0 && i < num_palettes) || (num_palettes <= 0 && palette_list[i] != WEED_PALETTE_END); i++) {
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
static void set_channel_size(weed_plant_t *filter, weed_plant_t *channel, int width, int height) {
  weed_plant_t *chantmpl = weed_channel_get_template(channel);
  boolean check_ctmpl = FALSE;
  int max, min;
  int filter_flags = weed_filter_get_flags(filter);

  if (width < MIN_CHAN_WIDTH) width = MIN_CHAN_WIDTH;
  if (width > MAX_CHAN_WIDTH) width = MAX_CHAN_WIDTH;
  if (height < MIN_CHAN_HEIGHT) height = MIN_CHAN_HEIGHT;
  if (height > MAX_CHAN_HEIGHT) height = MAX_CHAN_HEIGHT;

  if (filter_flags & WEED_FILTER_CHANNEL_SIZES_MAY_VARY) {
    check_ctmpl = TRUE;
  }

  if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_WIDTH)) {
    get_fixed_channel_size(chantmpl, &width, &height);
  } else {
    if (weed_plant_has_leaf(filter, WEED_LEAF_WIDTH)) {
      get_fixed_channel_size(filter, &width, &height);
    } else {
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_WIDTH)) {
        width = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
      }
      if (weed_plant_has_leaf(filter, WEED_LEAF_HSTEP)) {
        width = ALIGN_CEIL(width, weed_get_int_value(filter, WEED_LEAF_HSTEP, NULL));
      }
      if (width < MIN_CHAN_WIDTH) width = MIN_CHAN_WIDTH;
      if (width > MAX_CHAN_WIDTH) width = MAX_CHAN_WIDTH;
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MAXWIDTH)) {
        max = weed_get_int_value(chantmpl, WEED_LEAF_MAXWIDTH, NULL);
        if (max > 0 && width > max) width = max;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MAXWIDTH)) {
        max = weed_get_int_value(filter, WEED_LEAF_MAXWIDTH, NULL);
        if (width > max) width = max;
      }
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MINWIDTH)) {
        min = weed_get_int_value(chantmpl, WEED_LEAF_MINWIDTH, NULL);
        if (min > 0 && width < min) width = min;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MINWIDTH)) {
        min = weed_get_int_value(filter, WEED_LEAF_MINWIDTH, NULL);
        if (width < min) width = min;
      }
    }
  }

  if (width < MIN_CHAN_WIDTH) width = MIN_CHAN_WIDTH;
  if (width > MAX_CHAN_WIDTH) width = MAX_CHAN_WIDTH;

  width = (width >> 1) << 1;
  weed_set_int_value(channel, WEED_LEAF_WIDTH, width);

  if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_HEIGHT)) {
    get_fixed_channel_size(chantmpl, &width, &height);
  } else {
    if (weed_plant_has_leaf(filter, WEED_LEAF_HEIGHT)) {
      get_fixed_channel_size(filter, &width, &height);
    } else {
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_HEIGHT)) {
        height = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
      }
      if (weed_plant_has_leaf(filter, WEED_LEAF_VSTEP)) {
        height = ALIGN_CEIL(height, weed_get_int_value(filter, WEED_LEAF_VSTEP, NULL));
      }
      if (height < MIN_CHAN_HEIGHT) height = MIN_CHAN_HEIGHT;
      if (height > MAX_CHAN_HEIGHT) height = MAX_CHAN_HEIGHT;
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MAXHEIGHT)) {
        max = weed_get_int_value(chantmpl, WEED_LEAF_MAXHEIGHT, NULL);
        if (max > 0 && height > max) height = max;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MAXHEIGHT)) {
        max = weed_get_int_value(filter, WEED_LEAF_MAXHEIGHT, NULL);
        if (height > max) height = max;
      }
      if (check_ctmpl && weed_plant_has_leaf(chantmpl, WEED_LEAF_MINHEIGHT)) {
        min = weed_get_int_value(chantmpl, WEED_LEAF_MINHEIGHT, NULL);
        if (min > 0 && height < min) height = min;
      } else if (weed_plant_has_leaf(filter, WEED_LEAF_MINHEIGHT)) {
        min = weed_get_int_value(filter, WEED_LEAF_MINHEIGHT, NULL);
        if (height < min) height = min;
      }
    }
  }

  if (height < MIN_CHAN_HEIGHT) height = MIN_CHAN_HEIGHT;
  if (height > MAX_CHAN_HEIGHT) height = MAX_CHAN_HEIGHT;
  height = (height >> 1) << 1;
  weed_set_int_value(channel, WEED_LEAF_HEIGHT, height);
}


int weed_flagset_array_count(weed_plant_t **array, boolean set_readonly) {
  register int i;
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
  // call with filter_mutex unlocked
  weed_plant_t *filter, *orig_inst = inst;
  lives_rfx_t *rfx = NULL;
  boolean deinit_first = FALSE;
  weed_error_t retval;
  lives_filter_error_t filter_error = FILTER_SUCCESS;
  int key = -1;

  weed_instance_ref(inst);

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY))
    key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);

  if (key != -1) {
    weed_plant_t *gui;
    filter_mutex_lock(key);
    gui = weed_instance_get_gui(inst, FALSE);
    if (weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL) > 0) {
      // plugin is easing, so we need to deinit it
      uint64_t new_rte = GU641 << (key);
      if (mainw->rte & new_rte) mainw->rte ^= new_rte;
      // WEED_LEAF_EASE_OUT exists, so it'll get deinited right away
      weed_deinit_effect(key);
      weed_instance_unref(inst);
      filter_mutex_unlock(key);
      return FILTER_ERROR_COULD_NOT_REINIT;
    }
  }

  mainw->blend_palette = WEED_PALETTE_END;

  if (fx_dialog[1]) rfx = fx_dialog[1]->rfx;
  else if (mainw->multitrack) rfx = mainw->multitrack->current_rfx;

reinit:

  filter = weed_instance_get_filter(inst, FALSE);

  if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_TRUE) deinit_first = TRUE;

  if (deinit_first) {
    retval = weed_call_deinit_func(inst);
    if (retval != WEED_SUCCESS) goto re_done;
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
        //rfx_params_free(rfx);
        //lives_freep((void **)&rfx->params);
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

  if (!deinit_first) {
    if (!rfx || !(rfx->flags & (RFX_FLAGS_UPD_FROM_GUI | RFX_FLAGS_UPD_FROM_VAL)))
      retval = weed_call_deinit_func(inst);
  }

  if (retval != WEED_SUCCESS) goto re_done;

  if (filter_error != FILTER_INFO_REDRAWN) filter_error = FILTER_INFO_REINITED;

  if (reinit_compound) {
    inst = get_next_compound_inst(inst);
    if (inst) goto reinit;
  }

re_done:
  weed_instance_unref(orig_inst);
  if (key != -1) filter_mutex_unlock(key);
  mainw->blend_palette = WEED_PALETTE_END;
  if (retval == WEED_SUCCESS) return filter_error;
  if (retval == WEED_ERROR_PLUGIN_INVALID) return FILTER_ERROR_INVALID_PLUGIN;
  if (retval == WEED_ERROR_FILTER_INVALID) return FILTER_ERROR_INVALID_FILTER;
  return FILTER_ERROR_COULD_NOT_REINIT;
}


void weed_reinit_all(void) {
  // reinit all effects on playback start
  lives_filter_error_t retval;
  weed_plant_t *instance, *last_inst, *next_inst;
  register int i;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (rte_key_is_enabled(1 + i)) {
        mainw->osc_block = TRUE;
        if ((instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
          mainw->osc_block = FALSE;
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
        if (retval == FILTER_ERROR_COULD_NOT_REINIT
            || retval == FILTER_ERROR_INVALID_PLUGIN
            || retval == FILTER_ERROR_INVALID_FILTER) {
          weed_instance_unref(instance);
          continue;
        }
        weed_instance_unref(instance);
      }
    }
  }

  mainw->osc_block = FALSE;
}


static void *thread_process_func(void *arg) {
  struct _procvals *procvals = (struct _procvals *)arg;
  procvals->ret = (*procvals->procfunc)(procvals->inst, procvals->tc);
  return NULL;
}


#define SLICE_ALIGN 2

static lives_filter_error_t process_func_threaded(weed_plant_t *inst, weed_timecode_t tc) {
  // split output(s) into horizontal slices
  struct _procvals *procvals;
  lives_thread_t *dthreads = NULL;
  weed_plant_t **xinst = NULL;
  weed_plant_t **xchannels, *xchan;
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_error_t retval;
  int nchannels;
  int pal;
  double vrt;

  weed_plant_t **out_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_CHANNELS, &nchannels);
  boolean plugin_invalid = FALSE;
  boolean filter_invalid = FALSE;
  boolean filter_busy = FALSE;
  boolean needs_reinit = FALSE;

  int vstep = SLICE_ALIGN, minh;
  int slices, slices_per_thread, to_use;
  int heights[2], *xheights;
  int offset = 0;
  int dheight, height, xheight = 0, cheight;
  int nthreads = 0;

  int i, j;

  weed_process_f process_func = (weed_process_f)weed_get_funcptr_value(filter, WEED_LEAF_PROCESS_FUNC, NULL);

  filter = weed_instance_get_filter(inst, TRUE);
  if (weed_plant_has_leaf(filter, WEED_LEAF_VSTEP)) {
    minh = weed_get_int_value(filter, WEED_LEAF_VSTEP, NULL);
    if (minh > vstep) vstep = minh;
  }

  for (i = 0; i < nchannels; i++) {
    /// min height for slices (in all planes) is SLICE_ALIGN, unless an out channel has a larger vstep set
    height = weed_channel_get_height(out_channels[i]);
    pal = weed_channel_get_palette(out_channels[i]);

    for (j = 0; j < weed_palette_get_nplanes(pal); j++) {
      vrt = weed_palette_get_plane_ratio_vertical(pal, j);
      cheight = height * vrt;
      if (xheight == 0 || cheight < xheight) xheight = cheight;
    }
  }

  if (xheight % vstep != 0) return FILTER_ERROR_DONT_THREAD;

  // slices = min height / step
  slices = xheight / vstep;
  slices_per_thread = ALIGN_CEIL(slices, prefs->nfx_threads) / prefs->nfx_threads;

  to_use = ALIGN_CEIL(slices, slices_per_thread) / slices_per_thread;
  if (--to_use < 1) return FILTER_ERROR_DONT_THREAD;

  procvals = (struct _procvals *)lives_calloc(to_use, sizeof(struct _procvals));
  procvals->ret = WEED_SUCCESS;
  xinst = (weed_plant_t **)lives_calloc(to_use, sizeof(weed_plant_t *));
  dthreads = (lives_thread_t *)lives_calloc(to_use - 1, sizeof(lives_thread_t));

  for (i = 0; i < nchannels; i++) {
    heights[1] = height = weed_get_int_value(out_channels[i], WEED_LEAF_HEIGHT, NULL);
    slices = height / vstep;
    slices_per_thread = CEIL((double)slices / (double)to_use, 1.);
    dheight = slices_per_thread * vstep;
    heights[0] = dheight;
    weed_set_int_value(out_channels[i], WEED_LEAF_OFFSET, 0);
    weed_set_int_array(out_channels[i], WEED_LEAF_HEIGHT, 2, heights);
  }

  for (j = 0; j < to_use; j++) {
    // each thread needs its own copy of the output channels, so it can have its own WEED_LEAF_OFFSET and WEED_LEAF_HEIGHT
    // therefore it also needs its own copy of inst
    // but note that WEED_LEAF_PIXEL_DATA always points to the same memory buffer(s)
    // this is good also because it avoids concurrency problems with updating inst leaves
    // one of the threads (in this case the last one) will get the original inst so it can update values
    if (j < to_use - 1) {
      pthread_mutex_lock(&mainw->instance_ref_mutex);
      xinst[j] = weed_plant_copy(inst);
      pthread_mutex_unlock(&mainw->instance_ref_mutex);
      xchannels = (weed_plant_t **)lives_calloc(nchannels, sizeof(weed_plant_t *));
    } else {
      xinst[j] = inst;
      xchannels = NULL;
    }

    for (i = 0; i < nchannels; i++) {
      if (j < to_use - 1) {
        xchan = xchannels[i] = weed_plant_copy(out_channels[i]);
      } else {
        xchan = out_channels[i];
      }
      xheights = weed_get_int_array(out_channels[i], WEED_LEAF_HEIGHT, NULL);
      height = xheights[1];
      dheight = xheights[0];
      offset = dheight * j;
      if ((height - offset) < dheight) dheight = height - offset;
      xheights[0] = dheight;
      weed_set_int_value(xchan, WEED_LEAF_OFFSET, offset);
      weed_set_int_array(xchan, WEED_LEAF_HEIGHT, 2, xheights);
      lives_free(xheights);
    }

    if (j < to_use - 1) {
      weed_set_plantptr_array(xinst[j], WEED_LEAF_OUT_CHANNELS, nchannels, xchannels);
    }
    lives_freep((void **)&xchannels);

    procvals[j].procfunc = process_func;
    procvals[j].inst = xinst[j];
    procvals[j].tc = tc; // use same timecode for all slices

    if (j < to_use - 1) {
      // start a thread for processing
      lives_thread_create(&dthreads[j], LIVES_THRDATTR_NONE, thread_process_func, &procvals[j]);
      nthreads++; // actual number of threads used
    } else {
      /// do the last portion oiurselves, rather than just waiting around
      (*thread_process_func)(&procvals[j]);
      retval = procvals[j].ret;
      if (retval == WEED_ERROR_PLUGIN_INVALID) plugin_invalid = TRUE;
      if (retval == WEED_ERROR_FILTER_INVALID) filter_invalid = TRUE;
      if (retval == WEED_ERROR_NOT_READY) filter_busy = TRUE;
      if (retval == WEED_ERROR_REINIT_NEEDED) needs_reinit = TRUE;
    }
  }

  // wait for threads to finish
  for (j = 0; j < nthreads; j++) {
    lives_thread_join(dthreads[j], NULL);
    retval = procvals[j].ret;
    if (retval == WEED_ERROR_PLUGIN_INVALID) plugin_invalid = TRUE;
    if (retval == WEED_ERROR_FILTER_INVALID) filter_invalid = TRUE;
    if (retval == WEED_ERROR_NOT_READY) filter_busy = TRUE;
    if (retval == WEED_ERROR_REINIT_NEEDED) needs_reinit = TRUE;

    xchannels = weed_get_plantptr_array(xinst[j], WEED_LEAF_OUT_CHANNELS, NULL);
    for (i = 0; i < nchannels; i++) {
      weed_plant_free(xchannels[i]);
    }
    lives_freep((void **)&xchannels);
    weed_plant_free(xinst[j]);
  }

  for (i = 0; i < nchannels; i++) {
    // reset the channel heights
    xheights = weed_get_int_array(out_channels[i], WEED_LEAF_HEIGHT, NULL);
    weed_set_int_value(out_channels[i], WEED_LEAF_HEIGHT, xheights[1]);
    lives_free(xheights);
    weed_leaf_delete(out_channels[i], WEED_LEAF_OFFSET);
  }

  lives_freep((void **)&procvals);
  lives_freep((void **)&xinst);
  lives_freep((void **)&dthreads);
  lives_freep((void **)&out_channels);
  weed_leaf_delete(inst, WEED_LEAF_HOST_UNUSED);

  if (plugin_invalid) return FILTER_ERROR_INVALID_PLUGIN;
  if (filter_invalid) return FILTER_ERROR_INVALID_FILTER;
  if (filter_busy) return FILTER_ERROR_BUSY;
  if (needs_reinit) return FILTER_ERROR_NEEDS_REINIT; // TODO...
  return FILTER_SUCCESS;
}


static lives_filter_error_t check_cconx(weed_plant_t *inst, int nchans, boolean *needs_reinit) {
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

  in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL);

  for (int i = 0; i < nchans; i++) {
    if (!weed_palette_is_alpha(weed_get_int_value(in_channels[i], WEED_LEAF_CURRENT_PALETTE, NULL))) continue;

    if (weed_plant_has_leaf(in_channels[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
      if (cconx_chain_data_internal(in_channels[i])) *needs_reinit = TRUE;
    }

    if (!weed_get_voidptr_value(in_channels[i], WEED_LEAF_PIXEL_DATA, NULL)) {
      weed_plant_t *chantmpl = weed_get_plantptr_value(in_channels[i], WEED_LEAF_TEMPLATE, NULL);
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl)))
        if (weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
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


/**
   @brief process a single video filter instance

   get in_tracks and out_tracks from the init_event; these map filter_instance channels to layers

   clear WEED_LEAF_DISABLED if we have non-zero frame and there is no WEED_LEAF_DISABLED in template
   if we have a zero (NULL) frame, set WEED_LEAF_DISABLED if WEED_LEAF_OPTIONAL, otherwise we cannot apply the filter

   set channel timecodes

   set the fps (data) in the instance

   pull pixel_data or wait for threads to complete for all input layers (unless it is there already)

   set each channel width, height to match largest (or smallest depending on quality level - TODO)
   of in layers (unless the plugin allows differing sizes)

   if width and height are wrong, resize in the layer

   if palette is wrong, first we try to change the plugin channel palette,
   if not possible we convert palette in the layer

   apply the effect, put result in output layer, set layer palette, width, height, rowstrides

   if filter does not support inplace, we must create a new pixel_data; this will then replace the original layer
   (this is passed to the plugin as the output, so we just copy by reference)

   for in/out alpha channels, there is no matching layer. These channels are passed around like data
   using mainw->cconx as a guide. We will simply free any in alpha channels after processing (unless inplace was used)

   WARNING: output layer may need resizing, and its palette may need adjusting - should be checked by the caller

   opwidth and opheight limit the maximum frame size, they can either be set to 0,0 or to max display size;
   however if all sources are smaller than this
   then the output will be smaller also and need resizing by the caller

   for purely audio filters, these are handled in weed_apply_audio_instance()

   for filters with mixed video / audio inputs (currently only generators) audio is added
*/
lives_filter_error_t weed_apply_instance(weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers,
    int opwidth, int opheight, weed_timecode_t tc) {
  // filter_mutex should be unlocked

  void *pdata;

  weed_plant_t **in_channels = NULL, **out_channels = NULL, *channel, *chantmpl;
  weed_plant_t **in_ctmpls;
  weed_plant_t *def_channel = NULL;
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_layer_t *layer = NULL, *orig_layer = NULL;

  int *in_tracks, *out_tracks;
  int *rowstrides;
  int *palettes;
  int *channel_rows;
  int *mand;
  lives_filter_error_t retval = FILTER_SUCCESS;

  short pb_quality = prefs->pb_quality;

  boolean rowstrides_changed;
  boolean needs_reinit = FALSE, inplace = FALSE;
  boolean all_out_alpha = TRUE; //,all_in_alpha=FALSE;
  boolean is_converter = FALSE, pvary = FALSE, svary = FALSE;
  boolean resized = FALSE, letterboxed = FALSE;
  boolean letterbox = FALSE;
  int num_palettes, num_in_tracks = 0, num_out_tracks;
  int inwidth, inheight, inpalette, outpalette, opalette, channel_flags, filter_flags = 0;
  int palette, cpalette, def_palette = 0;
  int outwidth, outheight;
  int numplanes = 0, width, height, xwidth, xheight;
  int nchr;
  int maxinwidth = 4, maxinheight = 4;
  int area, minarea = 0, maxarea = 0, rmaxw, rmaxh, rminw, rminh;
  int iclamping, isampling, isubspace;
  int clip = -1;
  frames_t frame;
  int num_ctmpl, num_inc, num_outc;
  int osubspace = -1, osampling = -1, oclamping = -1;
  int num_in_alpha = 0, num_out_alpha = 0;
  int nmandout = 0;
  int lcount = 0;
  int key = -1;
  int i, j, k;

  if (prefs->dev_show_timing)
    g_printerr("apply inst @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

  if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE) needs_reinit = TRUE;

  if (LIVES_IS_RENDERING) pb_quality = PB_QUALITY_HIGH;

  // TODO - check if inited, if not return with error (check also for filters with no init_func)

  /// pb_quality: HIGH == use MAX(in sizes, out size); MED == use MIN( MAX in sizes, output_size), LOW == use MIN(in sizes, out size
  if (pb_quality == PB_QUALITY_HIGH) {
    if (opwidth > 0)
      maxinwidth = opwidth;
    if (opheight > 0)
      maxinheight = opheight;
  }

  out_channels = weed_get_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  if (!out_channels && (mainw->preview || mainw->is_rendering) && num_compound_fx(inst) == 1) {
    /// skip filters with no output channels (we'll deal with them elsewhere)
    lives_freep((void **)&out_channels);
    return retval;
  }

  filter_flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);

  if (is_pure_audio(filter, TRUE)) {
    /// we process audio effects elsewhere
    lives_freep((void **)&out_channels);
    return FILTER_ERROR_IS_AUDIO;
  }

  in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL);

  // here, in_tracks and out_tracks map our layers to in_channels and out_channels in the filter
  if (!in_channels || !has_video_chans_in(filter, TRUE) || all_ins_alpha(filter, TRUE)) {
    if ((!out_channels && weed_plant_has_leaf(inst, WEED_LEAF_OUT_PARAMETERS)) ||
        (all_outs_alpha(filter, TRUE))) {
      /// if alpha out(s) we need to construct the output frames
      for (i = 0; (channel = get_enabled_channel(inst, i, FALSE)) != NULL; i++) {
        pdata = weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);

        if (!pdata) {
          width = DEF_FRAME_HSIZE_43S_UNSCALED; // TODO: default size for new alpha only channels
          height = DEF_FRAME_VSIZE_43S_UNSCALED; // TODO: default size for new alpha only channels

          weed_set_int_value(channel, WEED_LEAF_WIDTH, width);
          weed_set_int_value(channel, WEED_LEAF_HEIGHT, height);

          set_channel_size(filter, channel, width, height);

          if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
            int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
            if (rowstride_alignment_hint  > ALIGN_DEF)
              THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
          }

          // this will look at width, height, current_palette, and create an empty pixel_data and set rowstrides
          // and update width and height if necessary
          if (!create_empty_pixel_data(channel, FALSE, TRUE)) {
            retval = FILTER_ERROR_MEMORY_ERROR;
            goto done_video;
          }
        }
      }

      // TODO - this is more complex, as we have to check the entire chain of fx

      /// run it only if it outputs into effects which have video chans
      /// if (!feeds_to_video_filters(key,rte_key_getmode(key+1))) return FILTER_ERROR_NO_IN_CHANNELS;
      retval = run_process_func(inst, tc, key);

      lives_freep((void **)&in_channels);
      return retval;
    }
    lives_freep((void **)&in_channels);
    lives_freep((void **)&out_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (!get_enabled_channel(inst, 0, TRUE)) {
    /// we process generators elsewhere
    lives_freep((void **)&in_channels);
    lives_freep((void **)&out_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

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
    if (weed_palette_is_alpha(weed_get_int_value(in_channels[i], WEED_LEAF_CURRENT_PALETTE, NULL)) &&
        weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
      num_in_alpha++;
  }

  num_inc -= num_in_alpha;

  retval = check_cconx(inst, num_inc + num_in_alpha, &needs_reinit);
  if (retval != FILTER_SUCCESS) {
    goto done_video;
  }

  if (num_in_tracks > num_inc) num_in_tracks = num_inc; // for example, compound fx

  /// if we have more in_channels in the effect than in_tracks, we MUST (temp) disable the extra in_channels
  if (num_inc > num_in_tracks) {
    for (i = num_in_tracks; i < num_inc + num_in_alpha; i++) {
      if (!weed_palette_is_alpha(weed_channel_get_palette(in_channels[i]))) {
        if (weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
          weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        else weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
      }
    }
  }

  // count the actual layers fed in
  while (layers[lcount]) lcount++;

  for (k = i = 0; i < num_in_tracks; i++) {
    if (in_tracks[i] < 0) {
      retval = FILTER_ERROR_INVALID_TRACK; // probably audio
      goto done_video;
    }

    while (weed_palette_is_alpha(weed_channel_get_palette(in_channels[k]))) k++;

    channel = in_channels[k];
    weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);

    if (in_tracks[i] >= lcount) {
      /// here we have more in_tracks than actual layers (this can happen if we have blank frames)
      /// disable some optional channels if we can
      for (j = k; j < num_in_tracks + num_in_alpha; j++) {
        if (weed_palette_is_alpha(weed_channel_get_palette(in_channels[j]))) continue;
        channel = in_channels[j];
        chantmpl = weed_get_plantptr_value(channel, WEED_LEAF_TEMPLATE, NULL);
        if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl)))
          if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
            weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
          else weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
        else {
          retval = FILTER_ERROR_MISSING_LAYER;
          goto done_video;
        }
      }
      break;
    }
    layer = layers[in_tracks[i]];

    if (!weed_get_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, NULL)) {
      /// wait for thread to pull layer pixel_data
      if (prefs->dev_show_timing)
        g_printerr("fx clr pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      check_layer_ready(layer);
      if (prefs->dev_show_timing)
        g_printerr("fx clr post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      frame = weed_get_int_value(layer, WEED_LEAF_FRAME, NULL);
      if (frame == 0) {
        /// temp disable channels if we can
        channel = in_channels[k];
        chantmpl = weed_get_plantptr_value(channel, WEED_LEAF_TEMPLATE, NULL);
        if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl))) {
          if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
            weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        } else {
          weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
          retval = FILTER_ERROR_BLANK_FRAME;
          goto done_video;
	  // *INDENT-OFF*
	}}}
    k++;
  }
  // *INDENT-ON*

  /// ensure all chantmpls NOT marked "optional" have at least one corresponding enabled channel
  /// e.g. we could have disabled all channels from a template with "max_repeats" that is not optional
  in_ctmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &num_ctmpl);
  mand = (int *)lives_calloc(num_ctmpl, sizint);

  for (i = 0; i < num_inc + num_in_alpha; i++) {
    /// skip disabled in channels
    if (weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
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

  /// that is it for in_channels, now we move on to out_channels

  num_outc = weed_leaf_num_elements(inst, WEED_LEAF_OUT_CHANNELS);

  for (i = 0; i < num_outc; i++) {
    if (weed_palette_is_alpha(weed_channel_get_palette(out_channels[i]))) {
      if (weed_get_boolean_value(out_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE)
        num_out_alpha++;
    } else {
      if (weed_get_boolean_value(out_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE &&
          weed_get_boolean_value(out_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_FALSE) {
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
  }

  // pull frames for tracks

  for (i = 0; i < num_out_tracks + num_out_alpha; i++) {
    if (i >= num_outc) continue; // for compound filters, num_out_tracks may not be valid
    channel = out_channels[i];
    palette = weed_channel_get_palette(channel);
    if (weed_palette_is_alpha(palette)) continue;
    if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
        weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;
    all_out_alpha = FALSE;
  }

  for (i = 0; i < num_in_tracks; i++) {
    if (weed_palette_is_alpha(weed_channel_get_palette(in_channels[i]))) continue;
    if (weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
        weed_get_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) {
      continue;
    }
    layer = layers[in_tracks[i]];
    clip = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);

    if (!weed_layer_get_pixel_data(layer)) {
      //check_layer_ready(layer);
      /* /// wait for thread to pull layer pixel_data */
      if (!is_layer_ready(layer)) {
#define FX_WAIT_LIM 10000 // microseconds * 10
        if (prefs->dev_show_timing)
          g_printerr("fx clr2 pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        for (register int tt = 0; tt < FX_WAIT_LIM && !is_layer_ready(layer); tt++) {
          lives_nanosleep(10000);
        }
        if (prefs->dev_show_timing)
          g_printerr("fx clr2 post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        if (!is_layer_ready(layer)) {
          retval = FILTER_ERROR_MISSING_FRAME;
          goto done_video;
        }
        if (!weed_layer_get_pixel_data(layer)) {
          retval = FILTER_ERROR_MISSING_FRAME;
          goto done_video;
        }
      }
    }
    // we apply only transitions and compositors to the scrap file
    if (clip == mainw->scrap_file && num_in_tracks <= 1 && num_out_tracks <= 1) {
      retval = FILTER_ERROR_IS_SCRAP_FILE;
      goto done_video;
    }
    // use comparative widths - in RGB(A) pixels
    palette = weed_layer_get_palette(layer);
    inwidth = weed_layer_get_width_pixels(layer);
    inheight = weed_layer_get_height(layer);

    if (filter_flags & WEED_FILTER_CHANNEL_SIZES_MAY_VARY) svary = TRUE;
    if (filter_flags & WEED_FILTER_IS_CONVERTER) is_converter = TRUE;

    if (maxinwidth == 4 && inwidth > 4) maxinwidth = inwidth;
    if (maxinheight == 4 && inheight > 4) maxinheight = inheight;

    if ((mainw->multitrack && prefs->letterbox_mt) || (prefs->letterbox && !mainw->multitrack))
      letterbox = TRUE;

    if (!svary) {
      area = inwidth * inheight;
      if (area > maxarea) {
        maxarea = area;
        rmaxw = inwidth;
        rmaxh = inheight;
      }
      if (area < minarea || !minarea) {
        minarea = area;
        rminw = inwidth;
        rminh = inheight;
      }
      if (letterbox) {
        // there are at most 3 aspect ratios to consider:
        // - the player / encoded clip
        // - the renderer (which all frames letterbox inside, and which is letterboxed into the output)
        // - individual frame sizes (inwidth, inheight) which get letterboxed into the renderer

        // for multitrack, the renderer and output size / aspect ratio are always the same
        // for clip editor we use an algorithm to calculate the renderer aspect ratio, then set the size after.

        // - each frame is shrunk / expanded to fit the renderer size; in letterbox mode it maintains its aspect ratio
        // - the renderer is then shrunk / expanded to fit the output; in letterbox mode the renderer keeps its a.r

        // this is further complicated with variable quality, so we use a system where the renderer aspect ratio is
        // indpendent of the quality / player size. The quality setting only affects the renderer size
        // - we must avoid a situation where the padding changes when
        // quality is adjusted, as this can look very bad if the quality is switching often, thus at each quality setting the renderer must
        // have the same aspect ratio - the only change should be its size

        // if possible we avoid the situation where frames have padding in one direction inside the renderer,
        // and then the renderer gets padding in the other dimension

        // thus:
        // 1) pick the aspect ratio of the renderer, independent of the quality setting
        //  - with only one clip this is simple, we just use a.r of the clip
        //  - with multiple clips (transition / compositor) we use a heuristic, this done is such a way
        //    that the order of clips does not matter. i.e swapping fg / bg clips doesnt cause the a.r to change
        // 2) adjust the size of the renderer depending on quality
        //   - in high, med, all frames must fit inside this, in low only 1 frame (the smallest [by area]) need fit inside
        //   - in high quality, the screen / output size must also fit inside
        // 3) the effect is then applied
        // 4) the output from the effect is normally the same as all the inputs, we just maintain this until
        //     all effects are applied, then finally the end result is resized / letterboxed into the player
        //
        // ideally:
        //  - we want at least one frame to completely fill the rendererer in one dimension
        //  - if we make sure the render always has the same a.r as one of the frames, this is ensured
        //
        // there are various possibilities - we could use a.r of largest frame, smallest frame, widest frame, tallest frame
        // - we pick a.r. of the smallest frame, since in low quality we also set the renderer size by this,
        // thus at least one of the frames will not need any resizing / letterboxing

#if 1
        // min size
        maxinwidth = rminw;
        maxinheight = rminh;
#else
        // min area
        double hscale, vscale;
        double diw  = (double)inheight;
        double dih = (double)inheight;
        double miw = (double)maxinwidth;
        double mih = (double)maxinheight;

        int lb_width, lb_height;

        hscale = diw / miw;
        vscale = dih / mih;

        if (maxinwidth == 4 || maxinheight == 4 || (hscale <= 1. && vscale <= 1.)) {
          // fits inside completely, use the new frame a.r
          maxinwidth = inwidth;
          maxinheight = inheight;
        } else {
          if (hscale > 1. && vscale > 1.) {
            // current. sz. fits inside completely, keep it
            // DO NOTHING
          } else {
            int a0, a1;
            // exactly one dimension larger
            lb_width = inwidth;
            lb_height = inheight;
            calc_maxspect(opwidth, opheight, &lb_width, &lb_height);
            a0 = opwidth * opheight - lb_width * lb_height;
            lb_width = maxinwidth;
            lb_height = maxinheight;
            calc_maxspect(opwidth, opheight, &lb_width, &lb_height);
            a1 = opwidth * opheight - lb_width * lb_height;
            if (a0 < a1) {
              maxinwidth = inwidth;
              maxinheight = inheight;
            } else if (a0 == a1) {
              if (inwidth > maxinwidth) {
                maxinwidth = inwidth;
                maxinheight = inheight;
              }
            }
          }
#endif
      }
    }
  }
  // opwidth, opheight - these are actually renderer width and height

  if (!svary || prefs->pb_quality == PB_QUALITY_LOW || !is_converter) {
    switch (pb_quality) {
    case PB_QUALITY_HIGH:
      if (letterbox) {
        // keep a.r. of maxinwidth, maxinheight, but make sure we contain rmax and op
        calc_midspect(maxinwidth, maxinheight, &rmaxw, &rmaxh);
        if (rmaxw < opwidth || rmaxh < opheight)
          calc_midspect(rmaxw, rmaxh, &opwidth, &opheight);
        else {
          opwidth = rmaxw;
          opheight = rmaxh;
        }
      } else {
        opwidth = MAX(opwidth, rmaxw);
        opheight = MAX(opheight, rmaxh);
      }
      break;
    case PB_QUALITY_MED:
      if (letterbox) {
        if (!mainw->multitrack) {
          if (maxinwidth > opwidth || maxinheight > opheight) {
            calc_maxspect(opwidth, opheight, &maxinwidth, &maxinheight);
            opwidth = maxinwidth;
            opheight = maxinheight;
          } else {
            calc_maxspect(maxinwidth, maxinheight, &opwidth, &opheight);
          }
        } else {
          calc_midspect(maxinwidth, maxinheight, &rmaxw, &rmaxh);
          opwidth = rmaxw;
          opheight = rmaxh;
        }
      } else {
        opwidth = MIN(opwidth, rmaxw);
        opheight = MIN(opheight, rmaxh);
      }
      break;
    default:
      // PB_QUALITY_LOW
      if (letterbox) {
        calc_midspect(maxinwidth, maxinheight, &rminw, &rminh);
        opwidth = rminw;
        opheight = rminh;
      } else {
        opwidth = MIN(opwidth, rminw);
        opheight = MIN(opheight, rminh);
      }
      break;
    }
  }

  // swscale (I think) likes to have multiples of 8
  opwidth = (opwidth >> 3) << 3;
  opheight = (opheight >> 1) << 1;

  if (pb_quality < PB_QUALITY_HIGH && mainw->multitrack && prefs->letterbox_mt) {
    calc_midspect(cfile->hsize, cfile->vsize, &opwidth, &opheight);
  }

  /// pass 1, we try to set channel sizes to opwidth X opheight
  /// channel may have restrictions (e.g fixed size or step values) so we will set it as near as we can
  /// we won't resize next as we will try to palette convert and resize in one go
  for (k = i = 0; k < num_inc + num_in_alpha; k++) {
    int owidth, oheight;

    channel = get_enabled_channel(inst, k, TRUE);
    if (!channel) break;

    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;

    if (!def_channel) def_channel = channel;

    if (weed_palette_is_alpha(weed_channel_get_palette(channel))) {
      /// currently, alpha channels don't come in layers, we add them like data directly to the channel
      /// plus, we ignore the display size (it doesn't make sense to resize an alpha channel)
      /// so if the filter allows varying channel sizes we are OK
      /// else if its not the first channel and the size differs and the filter doesn't allow varying sizes then we're in trouble !
      if (!svary && def_channel != channel && (weed_channel_get_width(def_channel) != weed_channel_get_width(channel)
          || weed_channel_get_width(def_channel) != weed_channel_get_width(channel))) {
        retval = FILTER_ERROR_UNABLE_TO_RESIZE;
        goto done_video;
      }
      continue;
    }

    layer = layers[in_tracks[i]];

    // values in pixels
    width = opwidth;
    height = opheight;

    if (svary) {
      /// if sizes can vary then we must set the first in channel to op size, because we'll want the first out channel at that size
      /// otherwise (or if it's a converter) then we can use layer size
      if (is_converter || def_channel != channel) {
        palette = weed_layer_get_palette(layer);
        inwidth = weed_get_int_value(layer, WEED_LEAF_WIDTH, NULL);
        inheight = weed_get_int_value(layer, WEED_LEAF_HEIGHT, NULL);
        width = inwidth * weed_palette_get_pixels_per_macropixel(palette);
        height = inheight;
      }
    }

    cpalette = weed_channel_get_palette(channel);
    width /= weed_palette_get_pixels_per_macropixel(cpalette); // convert width to channel macropixels

    /// check if the plugin needs reinit
    owidth = weed_channel_get_width(channel);
    oheight = weed_channel_get_height(channel);

    chantmpl = weed_channel_get_template(channel);
    channel_flags = weed_chantmpl_get_flags(chantmpl);

    if (owidth != width || oheight != height) {
      set_channel_size(filter, channel, width, height);
      opwidth = weed_channel_get_width(channel);
      opheight = weed_channel_get_height(channel);
      if (channel_flags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE) {
        boolean oneeds_reinit = needs_reinit;
        needs_reinit = TRUE;
        if (channel_flags & WEED_CHANNEL_NEEDS_NATURAL_SIZE) {
          int *nsizes = weed_get_int_array(channel, WEED_LEAF_NATURAL_SIZE, NULL);
          if (nsizes) {
            int *lnsizes;
            if (prefs->dev_show_timing)
              g_printerr("nsw pre2 @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
            lives_nanosleep_until_nonzero(weed_plant_has_leaf(layer, WEED_LEAF_NATURAL_SIZE));
            if (prefs->dev_show_timing)
              g_printerr("nsw post2 @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
            lnsizes = weed_get_int_array(layer, WEED_LEAF_NATURAL_SIZE, NULL);
            if (nsizes[0] == lnsizes[0] && nsizes[1] == lnsizes[1]) needs_reinit = oneeds_reinit;
            lives_free(lnsizes);
            lives_free(nsizes);
          }
        }
      }
    }
    if (prefs->dev_show_timing)
      g_printerr("nsw pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    if (channel_flags & WEED_CHANNEL_NEEDS_NATURAL_SIZE) {
      lives_nanosleep_until_nonzero(weed_plant_has_leaf(layer, WEED_LEAF_NATURAL_SIZE));
      weed_leaf_dup(channel, layer, WEED_LEAF_NATURAL_SIZE);
    }
    if (prefs->dev_show_timing)
      g_printerr("nsw post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    i++;
  }

  if (!def_channel) {
    retval = FILTER_ERROR_NO_IN_CHANNELS;
    goto done_video;
  }

  /** now we do a second pass, and we set up the palettes on the in channels
    if it's the first channel, we try to change the channel palette to the layer palette*
    if that is not possible then we set it to the nearest alternative
    if it's not the first channel, we must use the palette from the first chan,
    unless the filter has WEED_FILTER_PALETTES_MAY_VARY.

     - if the channel template flags have WEED_FILTER_REINIT_ON_PALETTE_CHANGE, then we want to minimise palette changes, else
    we may be reiniting the filter instance a lot. If the clip type is CLIP_TYPE_FILE then we actually have 2 alternatives:
    the decoder palette, which will be used for unchanged frames pulled from the clip, + RGB24 which will be the case for
    altered frames loaded from png / jpeg. We want to avoid the worst case where we are reiniting on each alternate frame.
    so we'll pick in order of preference: RGB24, BGR24 (if the layer has no alpha), then RGBA32, BGRA32, ARGB32 (if it has alpha)
    (if the layer is already RGB we'll try that first). If no RGB palettes are available then we'll pick:
    YUV444P / YUVA4444P, YUV888 / YUVA8888, 422, 420 / UYVY / YUYV, 411

    There are two points where we can alter the palette: when the instance has just been inited (we can reinit with near zero cost)
    and if we are going to reinit anyway due to some other factor (e.g size change).

    IF the template has WEED_FILTER_REINIT_ON_ROWSTRIDES_CHANGE then similar rules apply except we will look at the macropixel
    sizes.

    Once the channel palette is set then we'll do a palette conversion / resize of the layer.

    TODO: we should be smarter here and figure out the best palette when forced to change,
    given the input palettes and output palettes
    of the entire filter chain from sources to output, since each change has a cost in terms of time and possibly quality.

    once the channel palette is set, then we may need to change the size and palette of the layer feeding in to it
    we do the resize first, since it may be possible to resize and palette convert all in one go
    after resizing we convert the palette if necessary
  */
  for (i = 0; i < num_in_tracks; i++) {
    int tgamma = WEED_GAMMA_UNKNOWN;
    if (i > 0) def_palette = weed_channel_get_palette(def_channel);

    channel = get_enabled_channel(inst, i, TRUE);
    if (channel) {
      chantmpl = weed_channel_get_template(channel);
      channel_flags = weed_chantmpl_get_flags(chantmpl);
    }

    //channel = get_enabled_channel(inst, i, TRUE);
    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;

    inpalette = weed_channel_get_palette(channel);
    if (weed_palette_is_alpha(inpalette)) continue;

    if (prefs->dev_show_timing)
      g_printerr("clrfx pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    layer = layers[in_tracks[i]];
    check_layer_ready(layer);
    if (prefs->dev_show_timing)
      g_printerr("clrfx post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    cpalette = opalette = weed_layer_get_palette(layer);
    if (weed_palette_is_alpha(opalette)) continue;

    chantmpl = weed_channel_get_template(channel);
    channel_flags |= weed_chantmpl_get_flags(chantmpl);

    if (filter_flags & WEED_FILTER_PALETTES_MAY_VARY) {
      pvary = TRUE;
    } else if (i > 0) opalette = weed_channel_get_palette(def_channel);

    if ((channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
        || ((channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
            && weed_palette_get_bits_per_macropixel(inpalette) != weed_palette_get_bits_per_macropixel(opalette))) {
      if (!needs_reinit && weed_get_boolean_value(inst, WEED_LEAF_HOST_UNUSED, NULL) == WEED_FALSE)
        /// use channel palette to avoid a reinit
        opalette = inpalette;
      else {
        int clip = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);
        if (clip == 0 || IS_NORMAL_CLIP(clip)) {
          if (!weed_palette_is_rgb(opalette))
            opalette = (weed_palette_has_alpha(opalette) ? WEED_PALETTE_RGBA32 : WEED_PALETTE_RGB24);
          needs_reinit = TRUE;
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

    if (opalette != inpalette) {
      /// check which of the plugin's allowed palettes is closest to our target palette
      palettes = weed_chantmpl_get_palette_list(filter, chantmpl, &num_palettes);
      palette = best_palette_match(palettes, num_palettes, opalette);
      if (i > 0 && !pvary && palette != def_palette) {
        lives_freep((void **)&palettes);
        lives_freep((void **)&rowstrides);
        retval = FILTER_ERROR_TEMPLATE_MISMATCH; // plugin author messed up...
        goto done_video;
      }

      if (palette != inpalette) {
        if (channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) needs_reinit = TRUE;
      }
      lives_freep((void **)&palettes);
      opalette = palette;
    }

    if (weed_palette_is_yuv(opalette)) {
      /// set up clamping, sampling and subspace for YUV palettes
      weed_channel_get_palette_yuv(channel, &iclamping, &isampling, &isubspace);

      if (!needs_reinit && (channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) != 0
          && weed_get_boolean_value(inst, WEED_LEAF_HOST_UNUSED, NULL) == WEED_FALSE) {
        /// avoid a reinit if we can
        oclamping = iclamping;
        osampling = isampling;
        osubspace = isubspace;
      } else {
        if (i > 0 && !pvary) {
          weed_channel_get_palette_yuv(def_channel, &oclamping, &osampling, &osubspace);
        } else {
          // cpalette is layer palette here
          if ((i == 0 || pvary) && weed_palette_is_yuv(cpalette)) {
            oclamping = weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL);
          } else {
            if (i > 0)
              oclamping = weed_get_int_value(def_channel, WEED_LEAF_YUV_CLAMPING, NULL);
            else
              oclamping = WEED_YUV_CLAMPING_UNCLAMPED;
          }
          if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_CLAMPING)) {
            oclamping = weed_get_int_value(chantmpl, WEED_LEAF_YUV_CLAMPING, NULL);
          } else {
            if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_CLAMPING)) {
              oclamping = weed_get_int_value(filter, WEED_LEAF_YUV_CLAMPING, NULL);
            }
          }

          // cpalette is layer palette here
          if (weed_palette_is_yuv(cpalette)) {
            // cant convert YUV <-> YUV sampling yet
            osampling = weed_get_int_value(layer, WEED_LEAF_YUV_SAMPLING, NULL);
          } else {
            if (i > 0)
              osampling = weed_get_int_value(def_channel, WEED_LEAF_YUV_SAMPLING, NULL);
            else
              osampling = WEED_YUV_SAMPLING_DEFAULT;
            if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_SAMPLING)) {
              osampling = weed_get_int_value(chantmpl, WEED_LEAF_YUV_SAMPLING, NULL);
            } else if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_SAMPLING)) {
              osampling = weed_get_int_value(filter, WEED_LEAF_YUV_SAMPLING, NULL);
            }
          }

          // cpalette is layer palette here
          if (weed_palette_is_yuv(cpalette)) {
            // cant convert YUV <-> YUV subspace yet
            osubspace = weed_get_int_value(layer, WEED_LEAF_YUV_SUBSPACE, NULL);
          } else {
            if (i > 0)
              osubspace = weed_get_int_value(def_channel, WEED_LEAF_YUV_SUBSPACE, NULL);
            else
              osampling = WEED_YUV_SUBSPACE_YUV;
            if (pvary && weed_plant_has_leaf(chantmpl, WEED_LEAF_YUV_SUBSPACE)) {
              osubspace = weed_get_int_value(chantmpl, WEED_LEAF_YUV_SUBSPACE, NULL);
            } else {
              if (weed_plant_has_leaf(filter, WEED_LEAF_YUV_SUBSPACE)) {
                osubspace = weed_get_int_value(filter, WEED_LEAF_YUV_SUBSPACE, NULL);
              } else
                osubspace = WEED_YUV_SUBSPACE_YUV;
            }
          }
        }

        if (channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) {
          if (oclamping != iclamping || isampling != osampling || isubspace != osubspace) {
            // we need to reinit due to yuv changes, see if we can switch to RGB
            if ((i == 0 || pvary) && IS_NORMAL_CLIP(clip)) {
              if (!weed_palette_is_rgb(opalette)) opalette = (weed_palette_has_alpha(opalette) ? WEED_PALETTE_RGBA32
                    : WEED_PALETTE_RGB24);
              if (opalette != palette) {
                /// check which of the plugin's allowed palettes is closest to our target palette
                palettes = weed_chantmpl_get_palette_list(filter, chantmpl, &num_palettes);
                opalette = best_palette_match(palettes, num_palettes, opalette);
              }
            }
            needs_reinit = TRUE;
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    orig_layer = NULL;

    // cpalette is layer palette here, opalette is now channel palette
    if (cpalette != opalette || (weed_palette_is_yuv(opalette) && weed_palette_is_yuv(cpalette)
                                 && weed_get_int_value(layer, WEED_LEAF_YUV_CLAMPING, NULL) != oclamping)) {
      if (all_out_alpha && (weed_palette_is_lower_quality(opalette, cpalette) ||
                            (weed_palette_is_rgb(inpalette) &&
                             !weed_palette_is_rgb(cpalette)) ||
                            (weed_palette_is_rgb(cpalette) &&
                             !weed_palette_is_rgb(inpalette)))) {
        /// for an analyser (no out channels, or only alpha outs) we copy the layer if it needs lower quality
        /// since won't need to pass the output to anything else, we'll just destroy the copy after
        orig_layer = layer;
        layer = weed_layer_copy(NULL, orig_layer);
        weed_set_plantptr_value(orig_layer, WEED_LEAF_DUPLICATE, layer);
      }
    }

    /// we'll resize first, since we may be able to change the palette at the same time

    /// check if we need to resize the layer
    /// get the pixel widths to compare
    /// the channel has the sizes we set in pass1

    width = opwidth;
    height = opheight;

    xwidth = inwidth = weed_layer_get_width_pixels(layer);
    xheight = inheight = weed_layer_get_height(layer);

    resized = FALSE;
    letterboxed = FALSE;

    // we are comparing the macropixel sizes which is fine because that won't change
    // regardless of the channel / layer palette, but for resize_layer we need the pixel size,
    // which will be width * pixels_per_macropixel of the original channel palette, which is the divisor we used when
    // setting the channel width. resize_layer() will handle conversion of the macropixel sizes between layer palette
    // and opalette
    if ((!svary && (inwidth != width || inheight != height))
        || (svary && (inwidth > width || inheight > height || letterbox))) {
      short interp = get_interp_value(pb_quality, TRUE);
      if (letterbox && !orig_layer) {
        // if we are letterboxing, as well as letterboxing the final output in the player, we will also letterbox each layer into its channel
        // if we only have 1 layer this is irrelevant since the channel size == layer size, (or in high quality, channel size == player size,
        /// but the player size would have been adjusted to the letterboxed size in any case).
        /// However, when mixing channels, all will have
        // the same size, which was set by the algorithm above. [unless the filter set WEED_FILTER_SIZES_MAY_VARY]
        // another alternative would be to resize all of the layers, and ignore letterboxing for the intermediate stages,
        //// but for now we assume if the user wants letterboxing then it applies to all layers
        int lbvals[4];

        calc_maxspect(width, height, &xwidth, &xheight);

        if (xwidth != width || height != xheight) {
          if (!letterbox_layer(layer, width, height, xwidth, xheight, interp, opalette, oclamping)) {
            retval = FILTER_ERROR_UNABLE_TO_RESIZE;
            goto done_video;
          }
          resized = TRUE;
          letterboxed = TRUE;
          lbvals[0] = (width - xwidth) >> 1;
          lbvals[1] = (height - xheight) >> 1;
          lbvals[2] = xwidth;
          lbvals[3] = xheight;
          weed_set_int_array(channel, WEED_LEAF_INNER_SIZE, 4, lbvals);
          if (!mainw->multitrack && i > 0 && mainw->blend_palette == WEED_PALETTE_END) {
            mainw->blend_palette = weed_layer_get_palette_yuv(layer, &mainw->blend_clamping, &mainw->blend_sampling,
                                   &mainw->blend_subspace);
            mainw->blend_width = xwidth;
            mainw->blend_height = xheight;
            mainw->blend_gamma = weed_layer_get_gamma(layer);
          }
        }
      }
      if (!resized) {
        if (weed_plant_has_leaf(channel, WEED_LEAF_INNER_SIZE))
          weed_leaf_delete(channel, WEED_LEAF_INNER_SIZE);
        if (!resize_layer(layer, width, height, interp, opalette, oclamping)) {
          retval = FILTER_ERROR_UNABLE_TO_RESIZE;
          goto done_video;
        }

        if (!mainw->multitrack && i > 0 && mainw->blend_palette == WEED_PALETTE_END) {
          mainw->blend_palette = weed_layer_get_palette_yuv(layer, &mainw->blend_clamping, &mainw->blend_sampling,
                                 &mainw->blend_subspace);
          mainw->blend_width = width;
          mainw->blend_height = height;
          mainw->blend_gamma = weed_layer_get_gamma(layer);
        }
      }
    } else {
      if (weed_plant_has_leaf(channel, WEED_LEAF_INNER_SIZE))
        weed_leaf_delete(channel, WEED_LEAF_INNER_SIZE);
    }

    // check palette again in case it changed during resize
    cpalette = weed_layer_get_palette(layer);

    if (prefs->apply_gamma && weed_palette_is_rgb(opalette)) {
      // apply gamma conversion if plugin requested it
      if (filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA)
        tgamma = WEED_GAMMA_LINEAR;
      else
        tgamma = cfile->gamma_type;
    }

    if (cpalette != opalette) {
      if (prefs->dev_show_timing)
        g_printerr("clpal1 pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (!convert_layer_palette_full(layer, opalette, oclamping, osampling, osubspace, tgamma)) {
        retval = FILTER_ERROR_INVALID_PALETTE_CONVERSION;
        goto done_video;
      }
      if (prefs->dev_show_timing)
        g_printerr("clpal1 post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    }

    /// check if the plugin needs reinit
    // at this stage we still haven't updated values in the channel, except for width and height
    channel_rows = weed_channel_get_rowstrides(channel, &nchr);

    // check layer rowstrides against previous settings
    rowstrides = weed_layer_get_rowstrides(layer, &numplanes);
    rowstrides_changed = rowstrides_differ(numplanes, rowstrides, nchr, channel_rows);
    lives_free(channel_rows);
    lives_free(rowstrides);

    if (rowstrides_changed && (channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))
      needs_reinit = TRUE;

    if (tgamma != WEED_GAMMA_UNKNOWN) {
      if (prefs->dev_show_timing)
        g_printerr("gamma1 pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      // if we letterboxed then we can save a few cycles by not gamma converting the blank regions
      // in med, low this seems not to work
      if (letterboxed) {
        gamma_convert_sub_layer(tgamma, 1.0, layer, (width - xwidth) / 2, (height - xheight) / 2,
                                xwidth, xheight, TRUE);
      } else
        gamma_convert_layer(tgamma, layer);
      if (prefs->dev_show_timing)
        g_printerr("gamma1 post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    }

    /// since layers and channels are interchangeable, we just call weed_layer_copy(channel, layer)
    /// the cast to weed_layer_t * is unnecessary, but added for clarity

    /// weed_layer_copy() will adjust all the important leaves in the channel:
    // width, height, rowstrides, current palette, pixel_data,
    // YUV_*, etc.
    if (!weed_layer_copy((weed_layer_t *)channel, layer)) {
      retval = FILTER_ERROR_COPYING_FAILED;
      goto done_video;
    }
    if (!resized) {
      if (!mainw->multitrack && i > 0 && mainw->blend_palette == WEED_PALETTE_END) {
        mainw->blend_palette = weed_layer_get_palette_yuv(layer, &mainw->blend_clamping, &mainw->blend_sampling,
                               &mainw->blend_subspace);
        mainw->blend_width = weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(mainw->blend_palette);
        mainw->blend_height = weed_layer_get_height(layer);
        mainw->blend_gamma = weed_layer_get_gamma(layer);
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /// That's it for in channels, now we move on to out channels
  /// now we don't need to resize / convert palette because we simply create new pixel_data for the channel
  for (i = 0; i < num_out_tracks + num_out_alpha; i++) {
    /// for the time being we only have a single out channel, so it simplifes things greatly

    channel = get_enabled_channel(inst, i, FALSE);
    if (!channel) break; // compound fx

    inpalette = weed_channel_get_palette(channel);

    if (!weed_palette_is_alpha(inpalette) && out_tracks[i] < 0) {
      retval = FILTER_ERROR_INVALID_TRACK; // probably audio
      goto done_video;
    }

    chantmpl = weed_channel_get_template(channel);
    channel_flags = weed_chantmpl_get_flags(chantmpl);

    /// store values to see if they changed
    rowstrides = weed_channel_get_rowstrides(channel, &numplanes);
    outwidth = weed_channel_get_width(channel);
    outheight = weed_channel_get_height(channel);
    outpalette = weed_channel_get_palette_yuv(channel, &oclamping, &osampling, &osubspace);

    /// set the timecode for the channel (this is optional, but its good to do to timestamp the output)
    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);

    // check for INPLACE. According to the spec we can make outputs other than the first inplace, but we'll skip that for now
    if (i == 0 &&
        (((weed_palette_is_alpha(weed_channel_get_palette(channel)) &&
           weed_palette_is_alpha(weed_channel_get_palette(def_channel))))
         || (in_tracks && out_tracks && in_tracks[0] == out_tracks[0]))) {
      if (channel_flags & WEED_CHANNEL_CAN_DO_INPLACE) {
        if (!(weed_palette_is_alpha(outpalette) &&
              weed_get_boolean_value(channel, WEED_LEAF_HOST_ORIG_PDATA, NULL) == WEED_TRUE)) {
          /// INPLACE
          palettes = weed_chantmpl_get_palette_list(filter, chantmpl, &num_palettes);
          palette = weed_channel_get_palette(def_channel);
          if (best_palette_match(palettes, num_palettes, palette) == palette) {
            if (!weed_layer_copy(channel, def_channel)) {
              retval = FILTER_ERROR_COPYING_FAILED;
              goto done_video;
            }
            inplace = TRUE;
          } else {
            lives_freep((void **)&rowstrides);
            retval = FILTER_ERROR_TEMPLATE_MISMATCH; // plugin author messed up...
            goto done_video;
          }
          if (weed_palette_is_alpha(palette)) {
            // protect our in / out channel from being freed()
            weed_set_boolean_value(channel, WEED_LEAF_HOST_ORIG_PDATA, WEED_TRUE);
          }
          lives_freep((void **)&palettes);
          weed_set_boolean_value(channel, WEED_LEAF_HOST_INPLACE, WEED_TRUE);
        }
      }
    }

    if (!inplace) {
      /// try to match palettes with first enabled in channel
      /// According to the spec, if the plugin permits we can match out channel n with in channel n
      /// but we'll skip that for now
      palette = weed_channel_get_palette(def_channel);
      if (palette != outpalette) {
        // palette change needed; try to change channel palette
        palettes = weed_chantmpl_get_palette_list(filter, chantmpl, &num_palettes);
        if ((outpalette = best_palette_match(palettes, num_palettes, palette)) == palette) {
          weed_set_int_value(channel, WEED_LEAF_CURRENT_PALETTE, palette);
        } else {
          lives_freep((void **)&palettes);
          lives_freep((void **)&rowstrides);
          retval = FILTER_ERROR_TEMPLATE_MISMATCH;
          goto done_video;
        }
        lives_freep((void **)&palettes);
      }

      weed_leaf_copy_or_delete(channel, WEED_LEAF_YUV_CLAMPING, def_channel);
      weed_leaf_copy_or_delete(channel, WEED_LEAF_YUV_SAMPLING, def_channel);
      weed_leaf_copy_or_delete(channel, WEED_LEAF_YUV_SUBSPACE, def_channel);

      if (svary && is_converter) {
        /// resizing - use the value we set in channel template
        /// this allows us to, for example, resize the same in_channel to multiple out_channels at various sizes
        width = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
        width /= weed_palette_get_pixels_per_macropixel(weed_channel_get_palette(channel));
        height = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
      } else {
        // NB. in future if we add more out channels, if (svary) this should be the ith in_channel.
        width = weed_channel_get_width(def_channel);
        height = weed_channel_get_height(def_channel);
      }

      set_channel_size(filter, channel, width, height);

      if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
        int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
        if (rowstride_alignment_hint > ALIGN_DEF) {
          THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
        }
      }

      // this will look at width, height, current_palette, and create an empty pixel_data and set rowstrides
      // and update width and height if necessary
      if (!create_empty_pixel_data(channel, FALSE, TRUE)) {
        retval = FILTER_ERROR_MEMORY_ERROR;
        goto done_video;
      }
      if (filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA)
        weed_channel_set_gamma_type(channel, WEED_GAMMA_LINEAR);
      else {
        if (CURRENT_CLIP_IS_VALID)
          weed_channel_set_gamma_type(channel, cfile->gamma_type);
      }

      weed_set_boolean_value(channel, WEED_LEAF_HOST_INPLACE, WEED_FALSE);
      // end of (!inplace)
    }

    /// check whether the filter needs a reinit

    channel_rows = weed_channel_get_rowstrides(channel, &nchr);
    palette = weed_channel_get_palette_yuv(channel, &iclamping, &isampling, &isubspace);

    rowstrides_changed = rowstrides_differ(nchr, channel_rows, numplanes, rowstrides);

    lives_freep((void **)&channel_rows);
    lives_freep((void **)&rowstrides);

    width = weed_channel_get_width(channel);
    height = weed_channel_get_height(channel);

    if ((rowstrides_changed && (channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))) needs_reinit = TRUE;

    if ((outwidth != width || outheight != height) && (channel_flags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE))
      needs_reinit = TRUE;

    if (palette != inpalette || (weed_palette_is_yuv(palette)
                                 && (oclamping != iclamping || osampling != isampling || osubspace != isubspace)))
      if (channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) needs_reinit = TRUE;
  }

  /// all channels now set up

  if (needs_reinit) {
    retval = weed_reinit_effect(inst, FALSE);
    if (retval == FILTER_ERROR_COULD_NOT_REINIT
        || retval == FILTER_ERROR_INVALID_PLUGIN
        || retval == FILTER_ERROR_INVALID_FILTER) {
      goto done_video;
    }
  }

  if (prefs->apply_gamma) {
    // do gamma correction of any RGB(A) parameters
    gamma_conv_params(((filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA))
                      ? WEED_GAMMA_LINEAR : WEED_GAMMA_SRGB, inst, TRUE);
    gamma_conv_params(((filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA))
                      ? WEED_GAMMA_LINEAR : WEED_GAMMA_SRGB, inst, FALSE);
  }

  if (CURRENT_CLIP_IS_VALID)
    weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);

  //...finally we are ready to apply the filter

  // TODO - better error handling
  //...finally we are ready to apply the filter
  retval = run_process_func(inst, tc, key);

  /// do gamma correction of any integer RGB(A) parameters
  /// convert in and out parameters to SRGB
  /// TODO: store / restore old vals
  gamma_conv_params(WEED_GAMMA_SRGB, inst, TRUE);
  gamma_conv_params(WEED_GAMMA_SRGB, inst, FALSE);

  if (retval == FILTER_ERROR_INVALID_PLUGIN) {
    goto done_video;
  }

  if (retval == FILTER_ERROR_NEEDS_REINIT) {
    needs_reinit = TRUE;
  }

  for (k = 0; k < num_inc + num_in_alpha; k++) {
    channel = get_enabled_channel(inst, k, TRUE);
    if (weed_palette_is_alpha(weed_channel_get_palette(channel))) {
      // free pdata for all alpha in channels, unless orig pdata was passed from a prior fx
      layer = layers[in_tracks[k]];
      weed_layer_pixel_data_free(channel);
      if (!weed_channel_get_pixel_data(channel)) {
        weed_layer_t *dupe = weed_get_plantptr_value(layers[in_tracks[i]], WEED_LEAF_DUPLICATE, NULL);
        if (dupe) weed_layer_nullify_pixel_data(dupe);
        else weed_layer_nullify_pixel_data(layer);
      }
    }
  }

  // now we write our out channels back to layers, leaving the palettes and sizes unchanged

  for (i = k = 0; k < num_out_tracks + num_out_alpha; k++) {
    channel = get_enabled_channel(inst, k, FALSE);
    if (!channel) break; // compound fx

    layer = layers[out_tracks[i]];
    weed_set_boolean_value(layer, "letterboxed", letterbox);

    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_INPLACE, NULL) == WEED_TRUE) continue;

    if (weed_palette_is_alpha(weed_channel_get_palette(channel))) {
      // out chan data for alpha is freed after all fx proc - in case we need for in chans
      continue;
    }

    if (retval == FILTER_ERROR_BUSY && num_in_tracks == 1 && num_out_tracks == 1) {
      // filter busy, we will free out_channel data, set values from in_channel instead
      weed_plant_t *in_channel = get_enabled_channel(inst, k, TRUE);
      weed_layer_pixel_data_free(channel);
      if (!weed_layer_copy(layer, in_channel)) {
        retval = FILTER_ERROR_COPYING_FAILED;
        goto done_video;
      }
    } else {
      // free any existing pixel_data, we will replace it with the channel data
      weed_layer_pixel_data_free(layer);
      if (!weed_layer_copy(layer, channel)) {
        retval = FILTER_ERROR_COPYING_FAILED;
        goto done_video;
      }
    }
    i++;
  }

  if (needs_reinit) retval = FILTER_ERROR_NEEDS_REINIT;

  for (i = 0; i < num_inc + num_in_alpha; i++) {
    if (weed_get_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) {
      weed_set_boolean_value(in_channels[i], WEED_LEAF_DISABLED, WEED_FALSE);
      weed_set_boolean_value(in_channels[i], WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
    }
  }

done_video:

  for (i = 0; i < num_in_tracks; i++) {
    weed_plant_t *dupe;
    layer = layers[in_tracks[i]];
    check_layer_ready(layer);
    dupe = weed_get_plantptr_value(layer, WEED_LEAF_DUPLICATE, NULL);
    if (dupe) {
      weed_layer_free(dupe);
      weed_leaf_delete(layer, WEED_LEAF_DUPLICATE);
    }
  }

  lives_freep((void **)&in_tracks);
  lives_freep((void **)&out_tracks);
  lives_freep((void **)&in_channels);
  lives_freep((void **)&out_channels);

  return retval;
}


static lives_filter_error_t enable_disable_channels(weed_plant_t *inst, boolean is_in, int *tracks, int num_tracks,
    int nbtracks,
    weed_plant_t **layers) {
  // handle case where in_channels > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  weed_plant_t *filter = weed_instance_get_filter(inst, FALSE);
  weed_plant_t *channel, **channels, *chantmpl, **ctmpls = NULL, *layer;
  int maxcheck = num_tracks, i, j, num_ctmpls, num_channels;
  void **pixdata = NULL;
  boolean *mand;

  if (is_in)
    channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_CHANNELS, &num_channels);
  else
    channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_CHANNELS, &num_channels);

  if (num_tracks > num_channels) maxcheck = num_tracks = num_channels;
  if (num_channels > num_tracks) maxcheck = num_channels;

  for (i = 0; i < maxcheck; i++) {
    channel = channels[i];
    weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);

    // skip disabled channels for now
    if (weed_channel_is_disabled(channel)) continue;
    if (i < num_tracks) layer = layers[tracks[i] + nbtracks];
    else layer = NULL;

    if (!layer || ((weed_layer_is_audio(layer) && !weed_layer_get_audio_data(layer, NULL)) ||
                   (weed_layer_is_video(layer) && (pixdata = weed_layer_get_pixel_data_planar(layer, NULL)) == NULL))) {
      // if the layer data is NULL and it maps to a repeating channel, then disable the channel temporarily
      chantmpl = weed_channel_get_template(channel);
      if (weed_chantmpl_get_max_repeats(chantmpl) != 1) {
        if (!weed_channel_is_disabled(channel)) {
          weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
        } else weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_FALSE);
      } else {
        lives_free(channels);
        lives_freep((void **)&pixdata);
        lives_freep((void **)&ctmpls);
        return FILTER_ERROR_MISSING_LAYER;
      }
    }
  }

  lives_freep((void **)&pixdata);

  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not also "optional"
  if (is_in) ctmpls = weed_filter_get_in_chantmpls(filter, &num_ctmpls);
  else ctmpls = weed_filter_get_out_chantmpls(filter, &num_ctmpls);

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
    lives_freep((void **)&ctmpls);
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


lives_filter_error_t run_process_func(weed_plant_t *instance, weed_timecode_t tc, int key) {
  weed_plant_t *filter = weed_instance_get_filter(instance, FALSE);
  weed_process_f process_func;
  lives_filter_error_t retval = FILTER_SUCCESS;
  boolean did_thread = FALSE;
  int filter_flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
  //int64_t timex = lives_get_current_ticks();

  // see if we can multithread
  if ((prefs->nfx_threads = future_prefs->nfx_threads) > 1 &&
      (filter_flags & WEED_FILTER_HINT_MAY_THREAD)) {
    weed_plant_t **out_channels = weed_instance_get_out_channels(instance, NULL);
    if (key == -1 || !filter_mutex_trylock(key)) {
      retval = process_func_threaded(instance, tc);
      if (key != -1) filter_mutex_unlock(key);
    } else retval = FILTER_ERROR_INVALID_PLUGIN;
    /* totth += (lives_get_current_ticks() - timex); */
    /* thcount++; */
    lives_free(out_channels);
    if (retval != FILTER_ERROR_DONT_THREAD) did_thread = TRUE;
  }
  if (!did_thread) {
    // normal single threaded version
    process_func = (weed_process_f)weed_get_funcptr_value(filter, WEED_LEAF_PROCESS_FUNC, NULL);
    if (process_func && (key == -1 || !filter_mutex_trylock(key))) {
      weed_error_t ret = (*process_func)(instance, tc);
      if (key != -1) filter_mutex_unlock(key);
      /* totnth += (lives_get_current_ticks() - timex); */
      /* nthcount++; */
      if (ret == WEED_ERROR_PLUGIN_INVALID) retval = FILTER_ERROR_INVALID_PLUGIN;
      if (ret == WEED_ERROR_FILTER_INVALID) retval = FILTER_ERROR_INVALID_FILTER;
      if (ret == WEED_ERROR_NOT_READY) retval = FILTER_ERROR_BUSY;
    } else retval = FILTER_ERROR_INVALID_PLUGIN;
    weed_leaf_delete(instance, WEED_LEAF_HOST_UNUSED);
  }
  weed_leaf_delete(instance, WEED_LEAF_RANDOM_SEED);
  //  thrdit = !thrdit;
  return retval;
}


/// here, in_tracks and out_tracks map our layers array to in_channels and out_channels in the filter
/// each layer may only map to zero or one in channel and zero or one out channel.

/// in addition, each mandatory channel in/out must have a layer mapped to it. If an optional, repeatable  channel is unmatched
/// we disable it temporarily. We don't disable channels permanently here since other functions will handle that more complex issue.
static lives_filter_error_t weed_apply_audio_instance_inner(weed_plant_t *inst, weed_plant_t *init_event,
    weed_plant_t **layers, weed_timecode_t tc, int nbtracks) {
  // filter_mutex MUST be unlocked

  // TODO - handle the following:
  // input audio_channels are mono, but the plugin NEEDS stereo; we should duplicate the audio.

  // TODO - handle plugin return errors better, eg. NEEDS_REINIT

  int *in_tracks = NULL, *out_tracks = NULL;

  weed_layer_t *layer;
  weed_plant_t **in_channels = NULL, **out_channels = NULL, *channel, *chantmpl;

  float **adata;

  lives_filter_error_t retval = FILTER_SUCCESS;

  int channel_flags;
  int num_in_tracks, num_out_tracks;

  int num_inc;
  int key = -1;

  int nchans = 0;
  int nsamps = 0;

  register int i, j;

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);

  if (!get_enabled_channel(inst, 0, TRUE)) {
    // we process generators elsewhere
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (!init_event) {
    num_in_tracks = enabled_in_channels(inst, FALSE);
    in_tracks = (int *)lives_calloc(2, sizint);
    in_tracks[1] = 1;
    if (!get_enabled_channel(inst, 0, FALSE)) num_out_tracks = 0;
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
    channel = get_enabled_channel(inst, i, TRUE);

    if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) continue;

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA_LENGTH);
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_RATE);
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_CHANNELS);

    /* g_print("setting ad for channel %d from layer %d. val %p, eg %f \n", i, in_tracks[i] + nbtracks, weed_get_voidptr_value(layer, "audio_data", NULL), ((float *)(weed_get_voidptr_value(layer, "audio_data", NULL)))[0]); */
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA);
    /* g_print("setting afterval %p, eg %f \n", weed_get_voidptr_value(channel, "audio_data", NULL), ((float *)(weed_get_voidptr_value(channel, "audio_data", NULL)))[0]); */
  }

  // set up our out channels
  for (i = 0; i < num_out_tracks; i++) {
    weed_plant_t *inchan;

    if (out_tracks[i] != in_tracks[i]) {
      retval = FILTER_ERROR_INVALID_TRACK; // we dont do swapping around of audio tracks
      goto done_audio;
    }

    channel = get_enabled_channel(inst, i, FALSE);
    inchan = get_enabled_channel(inst, i, TRUE);
    layer = layers[out_tracks[i] + nbtracks];

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);
    chantmpl = weed_channel_get_template(channel);
    channel_flags = weed_chantmpl_get_flags(chantmpl);

    /// set up the audio data for each out channel. IF we can inplace then we use the audio_data from the corresponding in_channel
    /// otherwise we allocate it
    if ((channel_flags & WEED_CHANNEL_CAN_DO_INPLACE)
        && weed_channel_get_audio_rate(channel) == weed_channel_get_audio_rate(inchan)
        && weed_channel_get_naudchans(channel) == weed_channel_get_naudchans(inchan)) {
      weed_set_boolean_value(layer, WEED_LEAF_HOST_INPLACE, WEED_TRUE);
      weed_leaf_dup(channel, inchan, WEED_LEAF_AUDIO_DATA);
      /* g_print("setting dfff ad for channel %p from chan %p, eg %f \n", weed_get_voidptr_value(channel, "audio_data", NULL), weed_get_voidptr_value(inchan, "audio_data", NULL), ((float *)(weed_get_voidptr_value(layer, "audio_data", NULL)))[0]); */
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

    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_DATA_LENGTH);
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_RATE);
    weed_leaf_dup(channel, layer, WEED_LEAF_AUDIO_CHANNELS);
  }

  if (CURRENT_CLIP_IS_VALID) weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);

  //...finally we are ready to apply the filter
  retval = run_process_func(inst, tc, key);

  if (retval == FILTER_ERROR_INVALID_PLUGIN || retval == FILTER_ERROR_INVALID_FILTER)
    goto done_audio;

  // TODO - handle process errors (e.g. WEED_ERROR_PLUGIN_INVALID)

  // now we write our out channels back to layers
  for (i = 0; i < num_out_tracks; i++) {
    channel = get_enabled_channel(inst, i, FALSE);
    layer = layers[out_tracks[i] + nbtracks];
    if (weed_plant_has_leaf(channel, WEED_LEAF_AUDIO_DATA)) {
      /// free any old audio data in the layer, unless it's INPLACE
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
        weed_leaf_dup(layer, channel, WEED_LEAF_AUDIO_DATA);
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
  lives_freep((void **)&out_channels);

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

    in_channels = weed_get_plantptr_array(instance, WEED_LEAF_IN_CHANNELS, NULL);
    out_channels = weed_get_plantptr_array(instance, WEED_LEAF_OUT_CHANNELS, NULL);

    if (!in_channels) {
      if (!out_channels && weed_plant_has_leaf(instance, WEED_LEAF_OUT_PARAMETERS)) {
        // plugin has no in channels and no out channels, but if it has out parameters then it must be a data processing module

        // if the data feeds into audio effects then we run it now, otherwise we will run it during the video cycle
        if (!feeds_to_audio_filters(key, rte_key_getmode(key + 1))) return FILTER_ERROR_NO_IN_CHANNELS;

        // otherwise we just run the process_func() and return

        if (CURRENT_CLIP_IS_VALID)  weed_set_double_value(instance, WEED_LEAF_FPS, cfile->pb_fps);
        retval = run_process_func(instance, tc, key);
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
      if (mainw->pchains && mainw->pchains[key]) {
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

  in_channels = weed_get_plantptr_array_counted(instance, WEED_LEAF_IN_CHANNELS, &numinchans);
  out_channels = weed_get_plantptr_array_counted(instance, WEED_LEAF_OUT_CHANNELS, &numoutchans);

  retval = enable_disable_channels(instance, TRUE, in_tracks, ntracks, nbtracks, layers);
  // in theory we should check out channels also, but for now we only load audio filters with one mandatory out

  if (retval != FILTER_SUCCESS) goto audret1;

  filter = weed_instance_get_filter(instance, FALSE);
  flags = weed_filter_get_flags(filter);

  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) lvary = TRUE;

  if (vis && vis[0] < 0. && in_tracks[0] <= -nbtracks) {
    /// first layer comes from ascrap file; do not apply any effects except the final audio mixer
    if (numinchans == 1 && numoutchans == 1 && !(flags & WEED_FILTER_IS_CONVERTER)) {
      retval = FILTER_ERROR_IS_SCRAP_FILE;
      goto audret1;
    }
  }

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
    // - deinit inst
    // mutex locked
    if (key != -1) filter_mutex_lock(key);
    weed_call_deinit_func(instance);
    if (key != -1) filter_mutex_unlock(key);

    // - init inst
    if (weed_call_init_func(instance) != WEED_SUCCESS) {
      key_to_instance[key][key_modes[key]] = NULL;
      lives_freep((void **)&in_channels);
      lives_freep((void **)&out_channels);
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
      if (!filter_mutex_trylock(key)) {
        weed_set_double_array(in_params[vmaster], WEED_LEAF_VALUE, nvals, fvols);
        filter_mutex_unlock(key);
        //set_copy_to(instance, vmaster, TRUE);
      }
      lives_freep((void **)&fvols);
      lives_freep((void **)&in_params);
    }
  }

  retval2 = weed_apply_audio_instance_inner(instance, init_event, layers, tc, nbtracks);
  if (retval == FILTER_SUCCESS) retval = retval2;

  if (retval2 == FILTER_SUCCESS && was_init_event) {
    // handle compound filters
    if ((instance = get_next_compound_inst(instance)) != NULL) goto audinst1;
  }

  // TODO - handle invalid, needs_reinit etc.

  if (was_init_event) weed_instance_unref(orig_inst);

audret1:
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
static void weed_apply_filter_map(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, void ***pchains) {
  weed_plant_t *instance, *orig_inst;
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
    init_event = (weed_plant_t *)init_events[i];

    if (weed_plant_has_leaf(init_event, WEED_LEAF_HOST_TAG)) {
      keystr = weed_get_string_value(init_event, WEED_LEAF_HOST_TAG, NULL);
      key = atoi(keystr);
      lives_free(keystr);
      if (rte_key_valid(key + 1, FALSE)) {
        if ((instance = weed_instance_obtain(key, key_modes[key])) == NULL) continue;

        if (is_pure_audio(instance, FALSE)) {
          weed_instance_unref(instance);
          continue; // audio effects are applied in the audio renderer
        }

        orig_inst = instance;

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
            if (mainw->multitrack->solo_inst) {
              weed_instance_unref(instance);
              continue;
            }
          }
        } else {
          int idx;
          char *filter_hash;
          boolean is_valid = FALSE;
          if (!weed_plant_has_leaf(init_events[i], WEED_LEAF_OUT_TRACKS)
              || !weed_plant_has_leaf(init_events[i], WEED_LEAF_IN_TRACKS)) continue;
          if (mainw->multitrack && mainw->multitrack->solo_inst && mainw->multitrack->init_event
              && !LIVES_IS_PLAYING && mainw->multitrack->init_event != init_events[i]) continue;
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
          if (!is_valid) continue;

          if (mainw->multitrack && pchains && pchains[key]) {
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
            if (retval == FILTER_ERROR_COULD_NOT_REINIT
                || retval == FILTER_ERROR_INVALID_PLUGIN
                || retval == FILTER_ERROR_INVALID_FILTER) {
              weed_instance_unref(orig_inst);
              if (!LIVES_IS_PLAYING && mainw->multitrack && mainw->multitrack->solo_inst &&
                  orig_inst == mainw->multitrack->solo_inst) break;
              continue;
            }
          }
        }

        //if (LIVES_IS_PLAYING)
        filter_error = weed_apply_instance(instance, init_event, layers, mainw->pwidth, mainw->pheight, tc);

        //filter_error = weed_apply_instance(instance, init_event, layers, 0, 0, tc);

        if (filter_error == WEED_SUCCESS && (instance = get_next_compound_inst(instance)) != NULL) goto apply_inst2;
        if (filter_error == WEED_ERROR_REINIT_NEEDED) {
          retval = weed_reinit_effect(instance, FALSE);
          if (retval == FILTER_ERROR_COULD_NOT_REINIT
              || retval == FILTER_ERROR_INVALID_PLUGIN
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
          weed_instance_unref(orig_inst);
          break;
        }
        weed_instance_unref(orig_inst);
      }
    }
  }
  lives_freep((void **)&init_events);
}


weed_plant_t *weed_apply_effects(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc,
                                 int opwidth, int opheight, void ***pchains) {
  // given a stack of layers, a filter map, a timecode and possibly parameter chains
  // apply the effects in the filter map, and return a single layer as the result

  // if all goes wrong we return a blank 4x4 RGB24 layer (TODO - return a NULL ?)

  // returned layer can be of any width,height,palette
  // caller should free all input layers, WEED_LEAF_PIXEL_DATA of all non-returned layers is free()d here

  weed_plant_t *filter, *instance, *orig_inst, *instance2, *layer;
  lives_filter_error_t filter_error;

  boolean needs_reinit;

  int output = -1;
  int clip;
  int easeval = 0;

  int i;

  if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
    // rendering from multitrack
    if (filter_map && layers[0]) {
      weed_apply_filter_map(layers, filter_map, tc, pchains);
    }
  }

  // free playback: we will have here only one or two layers, and no filter_map.
  // Effects are applied in key order, in tracks are 0 and 1, out track is 0
  else {
    weed_plant_t *gui;
    for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
      if (rte_key_valid(i + 1, TRUE)) {
        if (!(rte_key_is_enabled(1 + i))) {
          // if anything is connected to ACTIVATE, the fx may be activated
          pconx_chain_data(i, key_modes[i], FALSE);
        }
        if (rte_key_is_enabled(1 + i)) {
          mainw->osc_block = TRUE;
          if ((instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
            mainw->osc_block = FALSE;
            continue;
          }
          filter = weed_instance_get_filter(instance, TRUE);

          if (is_pure_audio(filter, TRUE)) {
            weed_instance_unref(instance);
            continue;
          }
          gui = weed_instance_get_gui(instance, FALSE);
          if (weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL) > 0) {
            // if the plugin is easing out, check if it finished
            if (!weed_plant_has_leaf(instance, WEED_LEAF_AUTO_EASING)) { // if auto_Easing then we'll deinit it on the event_list
              if (!weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL)) {
                // easing finished, deinit it
                uint64_t new_rte = GU641 << (i);
                // record
                if (init_events[i]) {
                  // if we are recording, mark the number of frames to ease out
                  // we'll need to repeat the same process during preview / rendering
                  // - when we hit the init_event, we'll find the deinit, then work back x frames and mark easing start
                  weed_set_int_value(init_events[i], WEED_LEAF_EASE_OUT,
                                     weed_get_int_value(instance, WEED_LEAF_EASE_OUT, NULL));
                }
                weed_instance_unref(instance);
                filter_mutex_lock(i);
                weed_deinit_effect(i);
                if (mainw->rte & new_rte) {
                  mainw->rte ^= new_rte;
                  if (rte_window) rtew_set_keych(i, FALSE);
                  if (mainw->ce_thumbs) ce_thumbs_set_keych(i, FALSE);
                }
                filter_mutex_unlock(i);
                continue;
		// *INDENT-OFF*
              }}}
	  // *INDENT-ON*

          if (mainw->pchains && mainw->pchains[i]) {
            if (!filter_mutex_trylock(i)) {
              interpolate_params(instance, mainw->pchains[i], tc); // interpolate parameters during preview
              filter_mutex_unlock(i);
            }
          }
          //filter = weed_instance_get_filter(instance, TRUE);

          // TODO *** enable this, and apply pconx to audio gens in audio.c
          if (!is_pure_audio(filter, TRUE)) {
            if (mainw->pconx && !(mainw->preview || mainw->is_rendering)) {
              // chain any data pipelines
              needs_reinit = pconx_chain_data(i, key_modes[i], FALSE);

              // if anything is connected to ACTIVATE, the fx may be activated
              if ((instance2 = weed_instance_obtain(i, key_modes[i])) == NULL) {
                weed_instance_unref(instance);
                continue;
              }
              weed_instance_unref(instance);
              instance = instance2;
              if (needs_reinit) {
                filter_error = weed_reinit_effect(instance, FALSE);
                if (filter_error == FILTER_ERROR_COULD_NOT_REINIT
                    || filter_error == FILTER_ERROR_INVALID_PLUGIN
                    || filter_error == FILTER_ERROR_INVALID_FILTER) {
                  weed_instance_unref(instance);
                  continue;
		// *INDENT-OFF*
                }}}}
	  // *INDENT-ON*

          orig_inst = instance;

apply_inst3:

          if (weed_plant_has_leaf(instance, WEED_LEAF_HOST_NEXT_INSTANCE)) {
            // chain any internal data pipelines for compound fx
            needs_reinit = pconx_chain_data_internal(instance);
            if (needs_reinit) {
              filter_error = weed_reinit_effect(instance, FALSE);
              if (filter_error == FILTER_ERROR_COULD_NOT_REINIT
                  || filter_error == FILTER_ERROR_INVALID_PLUGIN
                  || filter_error == FILTER_ERROR_INVALID_FILTER) {
                weed_instance_unref(instance);
                continue;
		// *INDENT-OFF*
              }}}
	  // *INDENT-ON*

          filter_error = weed_apply_instance(instance, NULL, layers, opwidth, opheight, tc);

          if (easeval > 0 && !weed_plant_has_leaf(orig_inst, WEED_LEAF_AUTO_EASING)) {
            // if the plugin is supposed to be easing out, make sure it is really
            weed_plant_t *gui = weed_instance_get_gui(orig_inst, FALSE);
            if (gui) {
              int xeaseval = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL), myeaseval;
              myeaseval = weed_get_int_value(instance, WEED_LEAF_HOST_EASE_OUT_COUNT, NULL);
              if (xeaseval > myeaseval) {
                uint64_t new_rte = GU641 << (i);
                filter_mutex_lock(i);
                weed_instance_unref(orig_inst);
                if (mainw->rte & new_rte) {
                  mainw->rte ^= new_rte;
                  if (rte_window) rtew_set_keych(i, FALSE);
                  if (mainw->ce_thumbs) ce_thumbs_set_keych(i, FALSE);
                }
                weed_deinit_effect(i);
                filter_mutex_unlock(i);
                continue;
              }
              // count how many frames to ease out
              weed_set_int_value(instance, WEED_LEAF_HOST_EASE_OUT_COUNT,
                                 weed_get_int_value(instance, WEED_LEAF_HOST_EASE_OUT_COUNT, NULL) + 1);
            }
          }
          if (filter_error == FILTER_ERROR_NEEDS_REINIT) {
            // TODO...
          }
          if (filter_error == FILTER_INFO_REINITED)
            update_widget_vis(NULL, i, key_modes[i]);
          //#define DEBUG_RTE
#ifdef DEBUG_RTE
          if (filter_error != FILTER_SUCCESS) lives_printerr("Render error was %d\n", filter_error);
#endif
          if (filter_error == FILTER_SUCCESS && (instance = get_next_compound_inst(instance))) goto apply_inst3;

          if (mainw->pconx && (filter_error == FILTER_SUCCESS || filter_error == FILTER_INFO_REINITED
                               || filter_error == FILTER_INFO_REDRAWN)) {
            pconx_chain_data_omc(orig_inst, i, key_modes[i]);
          }
          weed_instance_unref(orig_inst);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  // TODO - set mainw->vpp->play_params from connected out params and out alphas

  if (!mainw->is_rendering) {
    mainw->osc_block = FALSE;
  }

  // caller should free all layers, but here we will free all other pixel_data

  for (i = 0; layers[i]; i++) {
    if (layers[i] == mainw->blend_layer) mainw->blend_layer = NULL;

    if ((mainw->multitrack && i == mainw->multitrack->preview_layer) || ((!mainw->multitrack ||
        mainw->multitrack->preview_layer < 0) &&
        ((weed_get_voidptr_value(layers[i], WEED_LEAF_PIXEL_DATA, NULL)) ||
         (weed_get_int_value(layers[i], WEED_LEAF_FRAME, NULL) != 0 &&
          (LIVES_IS_PLAYING || !mainw->multitrack || !mainw->multitrack->current_rfx ||
           (!mainw->multitrack->init_event || tc < get_event_timecode(mainw->multitrack->init_event) ||
            (mainw->multitrack->init_event == mainw->multitrack->avol_init_event) ||
            tc > get_event_timecode(weed_get_plantptr_value
                                    (mainw->multitrack->init_event, WEED_LEAF_DEINIT_EVENT, NULL)))))))) {
      if (output != -1 || weed_get_int_value(layers[i], WEED_LEAF_CLIP, NULL) == -1) {
        if (!weed_plant_has_leaf(layers[i], WEED_LEAF_PIXEL_DATA)) continue;
        check_layer_ready(layers[i]);
        weed_layer_pixel_data_free(layers[i]);
      } else output = i;
    } else {
      if (!weed_plant_has_leaf(layers[i], WEED_LEAF_PIXEL_DATA)) continue;
      check_layer_ready(layers[i]);
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
  if (!weed_get_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, NULL)) {
    check_layer_ready(layer);
    if (!pull_frame_at_size(layer, get_image_ext_for_type(mainw->files[clip]->img_type), tc, opwidth, opheight,
                            WEED_PALETTE_END)) {
      char *msg = lives_strdup_printf("weed_apply_effects created empty pixel_data at tc %ld, map was %p, clip = %d, frame = %d",
                                      tc, filter_map, clip, weed_get_int_value(layer, WEED_LEAF_FRAME, NULL));
      LIVES_WARN(msg);
      lives_free(msg);
      create_blank_layer(layer, get_image_ext_for_type(mainw->files[clip]->img_type), opwidth, opheight, WEED_PALETTE_END);
    }
  }
  return layer;
}


void weed_apply_audio_effects(weed_plant_t *filter_map, weed_layer_t **layers, int nbtracks, int nchans, int64_t nsamps,
                              double arate, weed_timecode_t tc, double * vis) {
  int i, num_inst, error;
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
  mainw->pchains = get_event_pchains();
  init_events = weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &num_inst);
  for (i = 0; i < num_inst; i++) {
    init_event = (weed_plant_t *)init_events[i];
    fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, &error);
    filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
    lives_freep((void **)&fhash);
    if (has_audio_chans_in(filter, FALSE) && !has_video_chans_in(filter, FALSE) && !has_video_chans_out(filter, FALSE) &&
        has_audio_chans_out(filter, FALSE)) {
      weed_apply_audio_instance(init_event, layers, nbtracks, nchans, nsamps, arate, tc, vis);
    }
    // TODO *** - also run any pure data processing filters which feed into audio filters
  }
  lives_freep((void **)&init_events);
  mainw->pchains = NULL;
}


void weed_apply_audio_effects_rt(weed_layer_t *alayer, weed_timecode_t tc, boolean analysers_only, boolean is_audio_thread) {
  weed_plant_t *instance, *filter, *orig_inst, *new_inst;
  lives_filter_error_t filter_error;
  weed_layer_t *layers[1];
  boolean needs_reinit;
  int nsamps, arate, nchans;

  // free playback: apply any audio filters or analysers (but not generators)
  // Effects are applied in key order

  if (!alayer) return;

  nchans = weed_layer_get_naudchans(alayer);
  arate = weed_layer_get_audio_rate(alayer);
  nsamps = weed_layer_get_audio_length(alayer);

  for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (!(rte_key_is_enabled(1 + i))) {
        // if anything is connected to ACTIVATE, the fx may be activated
        if (is_audio_thread) pconx_chain_data(i, key_modes[i], TRUE);
      }
      if (rte_key_is_enabled(1 + i)) {
        mainw->osc_block = TRUE;

        if ((orig_inst = instance = weed_instance_obtain(i, key_modes[i])) == NULL) {
          mainw->osc_block = FALSE;
          continue;
        }

        filter = weed_instance_get_filter(instance, FALSE);

        if (!has_audio_chans_in(filter, FALSE) || has_video_chans_in(filter, FALSE) || has_video_chans_out(filter, FALSE)) {
          weed_instance_unref(instance);
          mainw->osc_block = FALSE;
          continue;
        }

        if (analysers_only && has_audio_chans_out(filter, FALSE)) {
          weed_instance_unref(instance);
          continue;
        }

        if (mainw->pchains && mainw->pchains[i]) {
          if (!filter_mutex_trylock(i)) {
            interpolate_params(instance, mainw->pchains[i], tc); // interpolate parameters during preview
            filter_mutex_unlock(i);
          }
        }
        if (mainw->pconx  && is_audio_thread) {
          // chain any data pipelines

          needs_reinit = pconx_chain_data(i, key_modes[i], TRUE);

          // if anything is connected to ACTIVATE, the fx may be deactivated
          if ((new_inst = weed_instance_obtain(i, key_modes[i])) == NULL) {
            weed_instance_unref(instance);
            mainw->osc_block = FALSE;
            continue;
          }

          weed_instance_unref(instance);
          instance = new_inst;

          if (needs_reinit) {
            filter_error = weed_reinit_effect(instance, FALSE);
            if (filter_error == FILTER_ERROR_COULD_NOT_REINIT
                || filter_error == FILTER_ERROR_INVALID_PLUGIN
                || filter_error == FILTER_ERROR_INVALID_FILTER) {
              weed_instance_unref(instance);
              continue;
		// *INDENT-OFF*
            }}}
	  // *INDENT-ON*

        orig_inst = instance;

apply_audio_inst2:

        if (weed_plant_has_leaf(instance, WEED_LEAF_HOST_NEXT_INSTANCE)) {
          // chain any internal data pipelines for compound fx
          needs_reinit = pconx_chain_data_internal(instance);
          if (needs_reinit) {
            filter_error = weed_reinit_effect(instance, FALSE);
            if (filter_error == FILTER_ERROR_COULD_NOT_REINIT
                || filter_error == FILTER_ERROR_INVALID_PLUGIN
                || filter_error == FILTER_ERROR_INVALID_FILTER) {
              weed_instance_unref(instance);
              continue;
            }
          }
        }

        layers[0] = alayer;
        // will unref instance
        filter_error = weed_apply_audio_instance(instance, layers, 0, nchans, nsamps, arate, tc, NULL);

        if (filter_error == FILTER_SUCCESS && (instance = get_next_compound_inst(instance))) {
          goto apply_audio_inst2;
        }
        if (filter_error == FILTER_ERROR_NEEDS_REINIT) {
          // TODO...
        }

        weed_instance_unref(orig_inst);

        if (filter_error == FILTER_INFO_REINITED) update_widget_vis(NULL, i, key_modes[i]); // redraw our paramwindow
#ifdef DEBUG_RTE
        if (filter_error != FILTER_SUCCESS) lives_printerr("Render error was %d\n", filter_error);
#endif
        mainw->osc_block = FALSE;

        if (mainw->pconx && (filter_error == FILTER_SUCCESS || filter_error == FILTER_INFO_REINITED
                             || filter_error == FILTER_INFO_REDRAWN)) {
          pconx_chain_data_omc((instance = weed_instance_obtain(i, key_modes[i])), i, key_modes[i]);
          weed_instance_unref(instance);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
}


boolean has_audio_filters(lives_af_t af_type) {
  // do we have any active audio filters (excluding audio generators) ?
  // called from audio thread during playback
  weed_plant_t *filter;
  int idx;
  register int i;

  if (af_type == AF_TYPE_A && mainw->audio_frame_buffer) return TRUE;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (rte_key_is_enabled(1 + i)) {
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
  register int i;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i + 1, TRUE)) {
      if (rte_key_is_enabled(1 + i)) {
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

  weed_plant_t *param;

  int counted = 0;
  int num_params, i;
  boolean is_template = (WEED_PLANT_IS_FILTER_CLASS(plant));

nip1:

  if (is_template) {
    if (!(params = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params))) return 0;
  } else {
    if (!(params = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_PARAMETERS, &num_params))) goto nip1done;
  }

  if (!skip_hidden && !skip_internal) {
    counted += num_params;
    goto nip1done;
  }

  for (i = 0; i < num_params; i++) {
    if (skip_hidden && is_hidden_param(plant, i)) continue;
    param = params[i];
    if (skip_internal && weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
    counted++;
  }

nip1done:

  lives_freep((void **)&params);

  // TODO: should be skip_internal or !skip_internal ?
  if (!is_template && skip_internal && (plant = get_next_compound_inst(plant)) != NULL) goto nip1;

  return counted;
}


int num_out_params(weed_plant_t *plant) {
  int num_params, error;
  boolean is_template = (WEED_PLANT_IS_FILTER_CLASS(plant));

  if (is_template) {
    if (!weed_plant_has_leaf(plant, WEED_LEAF_OUT_PARAMETER_TEMPLATES) ||
        !weed_get_plantptr_value(plant, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error)) return 0;
    num_params = weed_leaf_num_elements(plant, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  } else {
    if (!weed_plant_has_leaf(plant, WEED_LEAF_OUT_PARAMETERS)) return 0;
    if (!weed_get_plantptr_value(plant, WEED_LEAF_OUT_PARAMETERS, &error)) return 0;
    num_params = weed_leaf_num_elements(plant, WEED_LEAF_OUT_PARAMETERS);
  }
  return num_params;
}

boolean has_usable_palette(weed_plant_t *chantmpl) {
  int palette = weed_get_int_value(chantmpl, WEED_LEAF_CURRENT_PALETTE, NULL);
  // currently only integer RGB palettes are usable
  if (palette == 5 || palette == 6) return FALSE;
  if (palette > 0 && palette <= 7) return TRUE;
  return FALSE;
}


int enabled_in_channels(weed_plant_t *plant, boolean count_repeats) {
  // count number of non-disabled in channels (video and/or audio) for a filter or instance

  // NOTE: for instances, we do not count optional audio channels, even if they are enabled

  weed_plant_t **channels = NULL;
  weed_plant_t *filter;
  boolean is_template = WEED_PLANT_IS_FILTER_CLASS(plant);
  int enabled = 0;
  int num_channels;

  register int i;

  if (is_template) {
    filter = plant;
    if (!weed_plant_has_leaf(plant, WEED_LEAF_IN_CHANNEL_TEMPLATES)) return 0;
    channels = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_CHANNEL_TEMPLATES, &num_channels);
  } else {
    filter = weed_instance_get_filter(plant, TRUE);
    if (!weed_plant_has_leaf(plant, WEED_LEAF_IN_CHANNELS)) return 0;
    channels = weed_get_plantptr_array_counted(plant, WEED_LEAF_IN_CHANNELS, &num_channels);
  }

  for (i = 0; i < num_channels; i++) {
    if (!is_template) {
      weed_plant_t *ctmpl = weed_get_plantptr_value(channels[i], WEED_LEAF_TEMPLATE, NULL);
      if (weed_chantmpl_is_audio(ctmpl) == WEED_TRUE) {
        if (weed_chantmpl_is_optional(ctmpl)) continue;
      }
      if (weed_get_boolean_value(channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE) enabled++;
    } else {
      // skip alpha channels
      if (mainw->multitrack && !has_non_alpha_palette(channels[i], filter)) continue;
      if (weed_get_boolean_value(channels[i], WEED_LEAF_HOST_DISABLED, NULL) == WEED_FALSE) enabled++;
    }

    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl = channels[i];
      else chantmpl = weed_get_plantptr_value(channels[i], WEED_LEAF_TEMPLATE, NULL);
      if (weed_plant_has_leaf(channels[i], WEED_LEAF_MAX_REPEATS)) {
        if (weed_get_boolean_value(channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE &&
            !has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
        repeats = weed_get_int_value(channels[i], WEED_LEAF_MAX_REPEATS, NULL) - 1;
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
  int enabled = 0;
  int num_channels, i;
  boolean is_template = WEED_PLANT_IS_FILTER_CLASS(plant);

  if (is_template) {
    channels = weed_get_plantptr_array_counted(plant, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_channels);
  } else {
    channels = weed_get_plantptr_array_counted(plant, WEED_LEAF_OUT_CHANNELS, &num_channels);
  }

  for (i = 0; i < num_channels; i++) {
    if (!is_template) {
      if (weed_get_boolean_value(channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE) enabled++;
    } else {
      if (weed_get_boolean_value(channels[i], WEED_LEAF_HOST_DISABLED, NULL) == WEED_FALSE) enabled++;
    }
    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl = channels[i];
      else chantmpl = weed_get_plantptr_value(channels[i], WEED_LEAF_TEMPLATE, NULL);
      if (weed_plant_has_leaf(channels[i], WEED_LEAF_MAX_REPEATS)) {
        if (weed_get_boolean_value(channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE &&
            !has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
        repeats = weed_get_int_value(channels[i], WEED_LEAF_MAX_REPEATS, NULL) - 1;
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
  int ne;
  int minas = 1, i;
  ents = weed_get_int_array_counted(plant, WEED_LEAF_AUDIO_CHANNELS, &ne);
  if (ne == 0) return 1;
  for (i = 0; i < ne; i++) {
    if (minas == 1 || (ents[i] > 0 && ents[i] < minas)) minas = ents[i];
    if (minas == 1) {
      lives_free(ents);
      return 1;
    }
  }
  lives_free(ents);
  return minas;
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
          weed_leaf_copy(array[i], WEED_LEAF_PALETTE_LIST, filter, WEED_LEAF_PALETTE_LIST);
      }
    }

    if (weed_chantmpl_is_optional(array[i])) {
      // is optional
      chans_in_opt_max++;
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
          weed_leaf_copy(array[i], WEED_LEAF_PALETTE_LIST, filter, WEED_LEAF_PALETTE_LIST);
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

weed_error_t weed_plant_free_host(weed_plant_t *plant) {
  // delete even undeletable plants
  weed_error_t err;
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  err = _weed_plant_free(plant);
  if (err == WEED_ERROR_UNDELETABLE) {
    weed_clear_plant_flags(plant, WEED_FLAG_UNDELETABLE, NULL);
    return _weed_plant_free(plant);
  }
  return err;
}


/* weed_plant_t *weed_plant_new_host(int type) { */
/*   return _weed_plant_new(type); */
/* } */


weed_error_t weed_leaf_delete_host(weed_plant_t *plant, const char *key) {
  // delete even undeletable leaves
  weed_error_t err;
  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  err = _weed_leaf_delete(plant, key);
  if (err == WEED_ERROR_UNDELETABLE) {
    weed_leaf_clear_flagbits(plant, key, WEED_FLAG_UNDELETABLE);
    err = _weed_leaf_delete(plant, key);
    if (err != WEED_SUCCESS) abort();
  }
  return err;
}


weed_error_t weed_leaf_set_host(weed_plant_t *plant, const char *key, uint32_t seed_type,
                                weed_size_t num_elems, void *values) {
  // change even immutable leaves
  weed_error_t err;

  if (!plant) return WEED_ERROR_NOSUCH_PLANT;
  do {
    err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
  } while (err == WEED_ERROR_CONCURRENCY);
  if (err == WEED_ERROR_IMMUTABLE) {
    int32_t flags = weed_leaf_get_flags(plant, key);
    flags ^= WEED_FLAG_IMMUTABLE;
    weed_leaf_set_flags(plant, key, flags);
    err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
    flags |= WEED_FLAG_IMMUTABLE;
    weed_leaf_set_flags(plant, key, flags);
  }
  return err;
}


static void upd_statsplant(const char *key) {
  int freq;
  if (mainw->is_exiting) return;
  if (!statsplant) statsplant = weed_plant_new(0);
  _weed_leaf_get(statsplant, key, WEED_SEED_INT, &freq);
  _weed_leaf_set(statsplant, key, WEED_SEED_INT, 1, &freq);
}


// memory profiling for plugins

void *lives_monitor_malloc(size_t size) {
  void *p = malloc(size);
  fprintf(stderr, "plugin mallocing %ld bytes, got ptr %p\n", size, p);
  if (size == 1024) break_me("monitor_malloc");
  return NULL;
}


void lives_monitor_free(void *p) {
  //fprintf(stderr, "plugin freeing ptr ptr %p\n", p);
}


weed_error_t weed_leaf_set_monitor(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_size_t num_elems,
                                   void *values) {
  weed_error_t err;
  err = _weed_leaf_set(plant, key, seed_type, num_elems, values);
  g_print("PL setting %s in type %d\n", key, weed_plant_get_type(plant));

  if (WEED_PLANT_IS_GUI(plant) && !strcmp(key, WEED_LEAF_FLAGS)) g_print("Err was %d\n", err);
  return err;
}


weed_error_t weed_leaf_get_monitor(weed_plant_t *plant, const char *key, int32_t idx, void *value) {
  // plugins can be monitored for example
  weed_error_t err = _weed_leaf_get(plant, key, idx, value);
  // and simulating bugs...
  /* if (!strcmp(key, WEED_LEAF_WIDTH) && value) { */
  /*   int *ww = (int *)value; */
  /*   *ww -= 100; */
  /* } */
  upd_statsplant(key);
  return err;
}


void show_weed_stats(weed_plant_t *statsplant) {
  LiVESList *freq = NULL, *sorted = NULL, *list;
  char **leaves;
  int val, i, added = 0, min, lmin = 0;
  weed_size_t nleaves;

  if (!statsplant) return;
  leaves = weed_plant_list_leaves(statsplant, &nleaves);
  /// sort in descending order
  for (i = 0; i < nleaves; i++) {
    int f = weed_get_int_value(statsplant, leaves[i], NULL);
    freq = lives_list_prepend(freq, LIVES_INT_TO_POINTER(f));
    //g_print("added %s with freq %d\n", leaves[i], f);
  }
  while (added < nleaves) {
    min = LIVES_MAXINT32;
    for (list = freq; list; list = list->next) {
      val = LIVES_POINTER_TO_INT(list->data);
      if (val < min && val > lmin) min = val;
    }
    //g_print("next min was %d\n", min);
    i = nleaves - 1;
    for (list = freq; list; list = list->next) {
      val = LIVES_POINTER_TO_INT(list->data);
      if (val == min) {
        //g_print("prep. %d %s\n", i, leaves[i]);
        sorted = lives_list_prepend(sorted, LIVES_INT_TO_POINTER(i));
        if (++added == nleaves) break;
      }
      i--;
    }
    if (min == lmin) break;
    lmin = min;
  }
  for (list = sorted; list; list = list->next) {
    val = LIVES_POINTER_TO_INT(list->data);
    g_print("STATS: %s : %d\n", leaves[val], weed_get_int_value(statsplant, leaves[val], NULL));
    free(leaves[val]);
  }
  free(leaves);
  lives_list_free(freq);
  lives_list_free(sorted);
  weed_plant_free(statsplant);
}


weed_plant_t *host_info_cb(weed_plant_t *xhost_info, void *data) {
  // if the plugin called weed_boostrap during its weed_setup() as we requested, then we will end up here
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

#ifndef USE_STD_MEMFUNCS
  // let's override some plugin functions...
  if (id == 100) {
    weed_set_funcptr_value(xhost_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)_ext_malloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)_ext_free);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)_ext_realloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)_ext_calloc);
  } else {
    weed_set_funcptr_value(xhost_info, WEED_LEAF_MALLOC_FUNC, (weed_funcptr_t)lives_malloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_FREE_FUNC, (weed_funcptr_t)lives_free);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_REALLOC_FUNC, (weed_funcptr_t)lives_realloc);
    weed_set_funcptr_value(xhost_info, WEED_LEAF_CALLOC_FUNC, (weed_funcptr_t)lives_calloc);
  }
  //weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)lives_memcpy_monitor);
  weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMCPY_FUNC, (weed_funcptr_t)lives_memcpy);
  weed_set_funcptr_value(xhost_info, WEED_LEAF_MEMSET_FUNC, (weed_funcptr_t)lives_memset);
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

  if (fxname && !strcmp(fxname, "projectM")) {
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

  int i;

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
  fxname = strip_ext(plugin_name);

  plugin_info = (*setup_fn)(weed_bootstrap);

  if (!plugin_info || (filters_in_plugin = check_weed_plugin_info(plugin_info)) < 1) {
    msg = lives_strdup_printf(_("No usable filters found in plugin:\n%s\n"), plugin_path);
    LIVES_INFO(msg);
    lives_free(msg);
    if (plugin_info) weed_plant_free(plugin_info);
    dlclose(handle);
    lives_chdir(pwd, FALSE);
    lives_freep((void **)&fxname);
    return;
  }

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
#ifdef DEBUG_WEED
      lives_printerr("Unsuitable filter \"%s\" in plugin \"%s\", reason code %d\n",
                     filter_name, plugin_name, reason);
#endif
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
  char *pkg = NULL, *pkgstring;
  boolean hidden;
  register int i;

  weed_fx_sorted_list = NULL;

  // menu entries for vj/set defs
  for (i = 0; i < num_weed_filters; i++) {
    filter = weed_filters[i];

    if (weed_filter_hints_unstable(filter) && !prefs->show_dev_opts && !prefs->unstable_fx) continue;
    filter_name = weed_filter_idx_get_name(i, FALSE, FALSE);

    // skip hidden filters
    if (weed_get_boolean_value(filter, WEED_LEAF_HOST_MENU_HIDE, NULL) == WEED_TRUE ||
        (num_in_params(filter, TRUE, TRUE) == 0 && !(!has_video_chans_in(filter, FALSE) && has_video_chans_out(filter, FALSE))) ||
        (enabled_in_channels(filter, TRUE) > 2 && enabled_out_channels(filter, TRUE) > 0))
      hidden = TRUE;
    else hidden = FALSE;

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

    lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
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


void weed_load_all(void) {
  // get list of plugins from directory and create our fx
  LiVESList *weed_plugin_list, *weed_plugin_sublist;
  char **dirs;
  char *subdir_path, *subdir_name, *plugin_path, *plugin_name;
  int max_modes = prefs->max_modes_per_key;
  int numdirs, ncompounds;
  int i, j;

  num_weed_filters = 0;

  weed_filters = NULL;
  hashnames = NULL;

  threaded_dialog_spin(0.);

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

    if (i < FX_KEYS_MAX_VIRTUAL)
      // *INDENT-OFF*
      key_defaults[i] = (weed_plant_t ***)lives_calloc(max_modes, sizeof(weed_plant_t **));
    // *INDENT-ON*

    key_modes[i] = 0; // current active mode of each key
    filter_map[i] = NULL; // maps effects in order of application for multitrack rendering
    for (j = 0; j < max_modes; j++) key_to_fx[i][j] = -1;
  }

  filter_map[FX_KEYS_MAX + 1] = NULL;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    pchains[i] = NULL;
    init_events[i] = NULL;
  }

  next_free_key = FX_KEYS_MAX_VIRTUAL;

  threaded_dialog_spin(0.);

  // first we parse the weed_plugin_path
#ifndef IS_MINGW
  numdirs = get_token_count(prefs->weed_plugin_path, ':');
  dirs = lives_strsplit(prefs->weed_plugin_path, ":", numdirs);
#else
  numdirs = get_token_count(prefs->weed_plugin_path, ';');
  dirs = lives_strsplit(prefs->weed_plugin_path, ";", numdirs);
#endif
  threaded_dialog_spin(0.);

  for (i = numdirs - 1; i >= 0; i--) {
    // get list of all files
    LiVESList *list;
    if (!lives_file_test(dirs[i], LIVES_FILE_TEST_IS_DIR)) continue;
    list = weed_plugin_list = get_plugin_list(PLUGIN_EFFECTS_WEED, TRUE, dirs[i], NULL);

    // parse twice, first we get the plugins, then 1 level of subdirs
    while (list) {
      LiVESList *listnext = list->next;
      threaded_dialog_spin(0.);
      plugin_name = (char *)list->data;
      if (!lives_strncmp(plugin_name + lives_strlen(plugin_name) - strlen(DLL_NAME) - 1, "." DLL_NAME, strlen(DLL_NAME) + 1)) {
        plugin_path = lives_build_filename(dirs[i], plugin_name, NULL);
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
      list = listnext;
      threaded_dialog_spin(0.);
    }

    // get 1 level of subdirs
    for (list = weed_plugin_list; list; list = list->next) {
      threaded_dialog_spin(0.);
      subdir_name = (char *)list->data;
      subdir_path = lives_build_filename(dirs[i], subdir_name, NULL);
      if (!lives_file_test(subdir_path, LIVES_FILE_TEST_IS_DIR) || !strcmp(subdir_name, "icons") || !strcmp(subdir_name, "data")) {
        lives_free(subdir_path);
        continue;
      }

      for (LiVESList *list2 = weed_plugin_sublist = get_plugin_list(PLUGIN_EFFECTS_WEED, TRUE, subdir_path, DLL_NAME);
           list2; list2 = list2 ->next) {
        plugin_name = (char *)list2->data;
        plugin_path = lives_build_filename(subdir_path, plugin_name, NULL);
        load_weed_plugin(plugin_name, plugin_path, subdir_path);
        lives_free(plugin_path);
      }
      lives_list_free_all(&weed_plugin_sublist);
      lives_freep((void **)&subdir_path);
      threaded_dialog_spin(0.);
    }
    lives_list_free_all(&weed_plugin_list);
  }

  lives_strfreev(dirs);

  d_print(_("Successfully loaded %d Weed filters\n"), num_weed_filters);

  threaded_dialog_spin(0.);
  ncompounds = load_compound_fx();
  threaded_dialog_spin(0.);
  make_fx_defs_menu(ncompounds);
  threaded_dialog_spin(0.);
  weed_fx_sorted_list = lives_list_reverse(weed_fx_sorted_list);
  fx_inited = TRUE;
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
    plants = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &nitems);
    if (nitems > 0) {
      for (i = 0; i < nitems; i++) weed_plant_free_if_not_in_list(plants[i], freed_ptrs);
      lives_freep((void **)&plants);
    }
  }

  // free out_param_templates
  plants = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &nitems);
  if (nitems > 0) {
    threaded_dialog_spin(0.);
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


static weed_plant_t *create_compound_filter(char *plugin_name, int nfilts, int *filts) {
  weed_plant_t *filter = weed_plant_new(WEED_PLANT_FILTER_CLASS), *xfilter, *gui;
  weed_plant_t **in_params = NULL, **out_params = NULL, **params;
  weed_plant_t **in_chans, **out_chans;

  char *tmp;

  double tgfps = -1., tfps;

  int count, xcount, error, nvals;
  int txparam = -1, tparam, txvolm = -1, tvolm;

  register int i, j, x;

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
        in_params[j] = weed_plant_copy(params[x]);
        gui = weed_get_plantptr_value(params[x], WEED_LEAF_GUI, &error);
        if (gui) weed_set_plantptr_value(in_params[j], WEED_LEAF_GUI, weed_plant_copy(gui));

        if (x == tparam) {
          weed_set_boolean_value(in_params[j], WEED_LEAF_IS_TRANSITION, WEED_TRUE);
        } else if (weed_plant_has_leaf(in_params[j], WEED_LEAF_IS_TRANSITION))
          weed_leaf_delete(in_params[j], WEED_LEAF_IS_TRANSITION);

        if (x == tvolm) {
          weed_set_boolean_value(in_params[j], WEED_LEAF_IS_VOLUME_MASTER, WEED_TRUE);
        } else if (weed_plant_has_leaf(in_params[j], WEED_LEAF_IS_VOLUME_MASTER))
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
    in_chans = weed_get_plantptr_array_counted(xfilter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &count);
    weed_set_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, count, in_chans);
    lives_free(in_chans);
  }

  // use out channels from last filter

  xfilter = weed_filters[filts[nfilts - 1]];
  if (weed_plant_has_leaf(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
    out_chans = weed_get_plantptr_array_counted(xfilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &count);
    weed_set_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, count, out_chans);
    lives_free(out_chans);
  }

  return filter;
}


static void load_compound_plugin(char *plugin_name, char *plugin_path) {
  FILE *cpdfile;

  weed_plant_t *filter = NULL, *ptmpl, *iptmpl, *xfilter;
  char buff[16384];
  char **array, **svals;
  char *author = NULL, *tmp, *key;
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
              d_print((tmp = lives_strdup_printf(_("Invalid compound effect %s - must have >1 sub filters\n"), plugin_name)));
              LIVES_ERROR(tmp);
              lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid effect %s found in compound effect %s, line %d\n"),
                                               buff, plugin_name, line)));
            LIVES_INFO(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid default found in compound effect %s, line %d\n"), plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < 0 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid filter %d for defaults found in compound effect %s, line %d\n"),
                                               xfilt, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid param %d for defaults found in compound effect %s, line %d\n"),
                                               pnum, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid number of values for defaults found in compound effect %s, line %d\n"),
                                               plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        // TODO - for INT and DOUBLE, check if default is within min/max bounds

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
                d_print((tmp = lives_strdup_printf(_("Invalid non-boolean value for defaults found in compound effect %s, "
                                                     "line %d\n"), pnum, plugin_name, line)));
                LIVES_ERROR(tmp);
                lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid param link found in compound effect %s, line %d\n"),
                                               plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < -1 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid out filter %d for link params found in compound effect %s, line %d\n"),
                                               xfilt, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
              d_print((tmp = lives_strdup_printf(_("Invalid out param %d for link params found in compound effect %s, line %d\n"),
                                                 pnum, plugin_name, line)));
              LIVES_ERROR(tmp);
              lives_free(tmp);
            }
            ok = FALSE;
            lives_strfreev(array);
            break;
          }
        }

        autoscale = atoi(array[2]);

        if (autoscale != WEED_TRUE && autoscale != WEED_FALSE) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid non-boolean value for autoscale found in compound effect %s, "
                                                 "line %d\n"), pnum, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2 = atoi(array[3]); // sub filter number
        if (xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid in filter %d for link params found in compound effect %s, line %d\n"),
                                               xfilt2, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid in param %d for link params found in compound effect %s, line %d\n"),
                                               pnum2, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid channel link found in compound effect %s, line %d\n"),
                                               plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          break;
        }

        array = lives_strsplit(buff, "|", ntok);

        xfilt = atoi(array[0]); // sub filter number
        if (xfilt < 0 || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid out filter %d for link channels found in compound effect %s, "
                                                 "line %d\n"), xfilt, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid out channel %d for link params found in compound effect %s, line %d\n"),
                                               cnum, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
          }
          ok = FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2 = atoi(array[2]); // sub filter number
        if (xfilt2 <= xfilt || xfilt >= nfilts) {
          if (!prefs->vj_mode) {
            d_print((tmp = lives_strdup_printf(_("Invalid in filter %d for link channels found in compound effect %s, line %d\n"),
                                               xfilt2, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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
            d_print((tmp = lives_strdup_printf(_("Invalid in channel %d for link params found in compound effect %s, line %d\n"),
                                               cnum2, plugin_name, line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
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


static int load_compound_fx(void) {
  LiVESList *compound_plugin_list;

  int plugin_idx, onum_filters = num_weed_filters;

  char *lives_compound_plugin_path, *plugin_name, *plugin_path;

  threaded_dialog_spin(0.);

  lives_compound_plugin_path = lives_build_path(prefs->config_datadir, PLUGIN_COMPOUND_EFFECTS_CUSTOM, NULL);
  compound_plugin_list = get_plugin_list(PLUGIN_COMPOUND_EFFECTS_CUSTOM, TRUE, lives_compound_plugin_path, NULL);

  for (plugin_idx = 0; plugin_idx < lives_list_length(compound_plugin_list); plugin_idx++) {
    plugin_name = (char *)lives_list_nth_data(compound_plugin_list, plugin_idx);
    plugin_path = lives_build_path(lives_compound_plugin_path, plugin_name, NULL);
    load_compound_plugin(plugin_name, plugin_path);
    lives_free(plugin_path);
    threaded_dialog_spin(0.);
  }

  lives_list_free_all(&compound_plugin_list);

  threaded_dialog_spin(0.);
  lives_free(lives_compound_plugin_path);

  lives_compound_plugin_path = lives_build_path(prefs->prefix_dir, PLUGIN_COMPOUND_DIR,
                               PLUGIN_COMPOUND_EFFECTS_BUILTIN, NULL);


  compound_plugin_list = get_plugin_list(PLUGIN_COMPOUND_EFFECTS_BUILTIN, TRUE, lives_compound_plugin_path, NULL);

  for (plugin_idx = 0; plugin_idx < lives_list_length(compound_plugin_list); plugin_idx++) {
    plugin_name = (char *)lives_list_nth_data(compound_plugin_list, plugin_idx);
    plugin_path = lives_build_path(lives_compound_plugin_path, plugin_name, NULL);
    load_compound_plugin(plugin_name, plugin_path);
    lives_free(plugin_path);
    threaded_dialog_spin(0.);
  }

  lives_list_free_all(&compound_plugin_list);

  if (num_weed_filters > onum_filters) {
    d_print(_("Successfully loaded %d compound filters\n"), num_weed_filters - onum_filters);
  }

  lives_free(lives_compound_plugin_path);
  threaded_dialog_spin(0.);
  return num_weed_filters - onum_filters;
}


void weed_unload_all(void) {
  weed_plant_t *filter, *plugin_info, *host_info;
  LiVESList *pinfo = NULL, *list, *freed_plants = NULL, *hostinfo_ptrs = NULL;

  int error;

  register int i, j;

  if (!fx_inited) return;

  mainw->num_tr_applied = 0;
  weed_deinit_all(TRUE);

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    for (j = 0; j < prefs->max_modes_per_key; j++) {
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
        threaded_dialog_spin(0.);
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

  threaded_dialog_spin(0.);

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
        if (weed_palette_is_alpha(weed_channel_get_palette(channels[i]))) weed_layer_free((weed_layer_t *)channels[i]);
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
        if (weed_palette_is_alpha(weed_channel_get_palette(channels[i]))) weed_layer_free((weed_layer_t *)channels[i]);
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


LIVES_GLOBAL_INLINE int _weed_instance_unref(weed_plant_t *inst) {
  // return new refcount
  // return value of -1 indicates the instance was freed
  // -2 if the inst was NULL
  int nrefs;
  if (!inst) return -2;

  pthread_mutex_lock(&mainw->instance_ref_mutex);
  nrefs = weed_get_int_value(inst, WEED_LEAF_HOST_REFS, NULL);

  if (--nrefs < -1) {
    char *msg;
    pthread_mutex_unlock(&mainw->instance_ref_mutex);
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_REFS)) {
      msg = lives_strdup_printf("unref of filter instance (%p) with nrefs == %d\n", inst, nrefs);
      LIVES_ERROR(msg);
      break_me("invalid filt inst unref");
    }
    return nrefs;
  }
  weed_set_int_value(inst, WEED_LEAF_HOST_REFS, nrefs);
  pthread_mutex_unlock(&mainw->instance_ref_mutex);

#ifdef DEBUG_REFCOUNT
  g_print("unref %p, nrefs==%d\n", inst, nrefs);
#endif
  if (nrefs == -1) {
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
  int nrefs;

  if (!inst) return 0;

  pthread_mutex_lock(&mainw->instance_ref_mutex);
  nrefs = weed_get_int_value(inst, WEED_LEAF_HOST_REFS, NULL);
  weed_set_int_value(inst, WEED_LEAF_HOST_REFS, ++nrefs);
#ifdef DEBUG_REFCOUNT
  g_print("ref %p, nrefs==%d\n", inst, nrefs);
#endif
  pthread_mutex_unlock(&mainw->instance_ref_mutex);
  return nrefs;
}


weed_plant_t *_weed_instance_obtain(int line, char *file, int key, int mode) {
  // does not create the instance, but adds a ref to an existing one
  // caller MUST call weed_instance_unref() when instance is no longer needed
  weed_plant_t *instance;

  if (mode < 0 || mode > rte_key_getmaxmode(key + 1)) return NULL;
  pthread_mutex_lock(&mainw->instance_ref_mutex);
  instance = key_to_instance[key][mode];
#ifdef DEBUG_REFCOUNT
  if (instance) g_print("wio %p at line %d in file %s\n", instance, line, file);
#endif
  if (instance) _weed_instance_ref(instance);
  pthread_mutex_unlock(&mainw->instance_ref_mutex);
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
        int rah = ALIGN_DEF, nplanes, width, n, pal;
        int rs[4];
        weed_set_voidptr_value(channels[ccount], WEED_LEAF_PIXEL_DATA, NULL);
        set_channel_size(filter, channels[ccount], DEF_GEN_WIDTH, DEF_GEN_HEIGHT);
        pal = WEED_PALETTE_END;
        if (pvary) pal = weed_get_int_value(chantmpls[i], WEED_LEAF_PALETTE_LIST, NULL);
        if (pal == WEED_PALETTE_NONE) pal = weed_get_int_value(filter, WEED_LEAF_PALETTE_LIST, NULL);
        weed_set_int_value(channels[ccount], WEED_LEAF_CURRENT_PALETTE, pal);
        if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
          rah = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
        }
        nplanes = weed_palette_get_nplanes(pal);
        width = weed_channel_get_width(channels[ccount]) * (weed_palette_get_bits_per_macropixel(pal) >> 3);
        for (n = 0; n < weed_palette_get_nplanes(pal); n++) {
          rs[n] = ALIGN_CEIL(width * weed_palette_get_plane_ratio_horizontal(pal, n), rah);
        }
        weed_set_int_array(channels[ccount], WEED_LEAF_ROWSTRIDES, nplanes, rs);
      }
      if (weed_plant_has_leaf(chantmpls[i], WEED_LEAF_HOST_DISABLED)) {
        weed_set_boolean_value(channels[ccount], WEED_LEAF_DISABLED,
                               weed_get_boolean_value(chantmpls[i], WEED_LEAF_HOST_DISABLED, NULL));
      }
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
        weed_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_HOST_DEFAULT);
      } else weed_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_DEFAULT);
      weed_add_plant_flags(params[i], WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE, "plugin_");
    } else {
      weed_leaf_copy(params[i], WEED_LEAF_VALUE, paramtmpls[i], WEED_LEAF_DEFAULT);
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

  register int i;

  // ignore filters with no in/out channels (e.g. data processors)
  if ((!in_channels || !in_channels[0]) && (!out_channels || !out_channels[0])) return;

  for (i = 0; in_channels && in_channels[i] &&
       !(weed_plant_has_leaf(in_channels[i], WEED_LEAF_DISABLED) &&
         weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE); i++) {
    channel = in_channels[i];
    chantmpl = weed_get_plantptr_value(channel, WEED_LEAF_TEMPLATE, NULL);
    if (weed_chantmpl_is_audio(chantmpl) == WEED_FALSE) {
      set_channel_size(filter, channel, 320, 240);
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
      set_channel_size(filter, channel, width, height);
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
  return inst;
}


void add_param_connections(weed_plant_t *inst) {
  weed_plant_t **in_ptmpls, **outp;
  weed_plant_t *iparam, *oparam, *first_inst = inst, *filter = weed_instance_get_filter(inst, TRUE);
  int *xvals;
  int nptmpls, error;

  register int i;

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
  // return an instance from a filter, with the (first if compound) instance refcounted
  // caller should call weed_instance_unref() when done
  weed_plant_t **inc = NULL, **outc = NULL, **inp = NULL, **outp = NULL, **xinp;

  weed_plant_t *last_inst = NULL, *first_inst = NULL, *inst, *ofilter = filter;
  weed_plant_t *ochan = NULL;

  int *filters = NULL, *xvals;

  char *key;

  int nfilters, ninpar = 0, error;

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

    if (i == 0) pthread_mutex_lock(&mainw->instance_ref_mutex);
    inst = weed_create_instance(filter, inc, outc, xinp, outp);

    if (i == 0) {
      weed_instance_ref(inst);
      pthread_mutex_unlock(&mainw->instance_ref_mutex);
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
    if (xinp != inp) lives_freep((void **)&xinp);

    /* for (j = 0; j < ninpar; j++) { */
    /*   // copy param values if that was set up */
    /*   set_copy_to(inst, j, TRUE); */
    /* } */
  }

  lives_freep((void **)&inp);

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


boolean weed_init_effect(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !
  // filter_mutex MUST be locked, on FALSE return is unlocked
  weed_plant_t *filter, *gui;
  weed_plant_t *new_instance, *inst;
  weed_plant_t *event_list;
  weed_error_t error;

  boolean fg_modeswitch = FALSE, is_trans = FALSE, gen_start = FALSE, is_modeswitch = FALSE;
  boolean all_out_alpha;
  boolean is_gen = FALSE;
  boolean is_audio_gen = FALSE;
  boolean inherited = FALSE;

  int num_tr_applied;
  int rte_keys = mainw->rte_keys;
  int inc_count, outc_count;
  int ntracks;
  int idx;
  int playing_file = mainw->playing_file;

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
      filter_mutex_unlock(hotkey);
      return FALSE;
    }

    if (is_gen) {
      if (mainw->agen_key != 0) {
        // we had an existing audio gen running - stop that one first
        int agen_key = mainw->agen_key - 1;
        boolean needs_unlock = FALSE;
        if (!filter_mutex_trylock(agen_key)) {
          needs_unlock = TRUE;
        }

        weed_deinit_effect(agen_key);

        pthread_mutex_lock(&mainw->event_list_mutex);
        if ((rte_key_is_enabled(1 + agen_key))) {
          // need to do this in case starting another audio gen caused us to come here
          mainw->rte ^= (GU641 << agen_key);
          pthread_mutex_unlock(&mainw->event_list_mutex);
          if (rte_window && !mainw->is_rendering && !mainw->multitrack) {
            rtew_set_keych(agen_key, FALSE);
          }
          if (mainw->ce_thumbs) ce_thumbs_set_keych(agen_key, FALSE);
        } else pthread_mutex_unlock(&mainw->event_list_mutex);
        if (needs_unlock) filter_mutex_unlock(agen_key);
      }
      is_audio_gen = TRUE;
    }
  }

  // if outputs are all alpha it is not a true (video/audio generating) generator
  all_out_alpha = all_outs_alpha(filter, TRUE);

  // TODO - block template channel changes
  // we must stop any old generators

  if (hotkey < FX_KEYS_MAX_VIRTUAL) {
    if (!all_out_alpha && is_gen && outc_count > 0 && hotkey != fg_generator_key && mainw->num_tr_applied > 0 &&
        mainw->blend_file != -1 && mainw->blend_file != mainw->current_file &&
        mainw->files[mainw->blend_file] &&
        mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR && !is_audio_gen) {
      /////////////////////////////////////// if blend_file is a generator we should finish it
      if (bg_gen_to_start == -1) {
        weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
      }

      bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;
      mainw->blend_file = -1;
    }

    if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR &&
        (fg_modeswitch || (is_gen && outc_count > 0 && mainw->num_tr_applied == 0)) && !is_audio_gen && !all_out_alpha) {
      if (mainw->is_processing || mainw->preview) {
        mainw->error = TRUE;
        filter_mutex_unlock(hotkey);
        return FALSE; // stopping fg gen will cause clip to switch
      }
      if (LIVES_IS_PLAYING && mainw->whentostop == STOP_ON_VID_END && !is_gen) {
        // stop on vid end, and the playing generator stopped
        mainw->cancelled = CANCEL_GENERATOR_END;
      } else {
        if (is_gen && mainw->whentostop == STOP_ON_VID_END) mainw->whentostop = NEVER_STOP;
        //////////////////////////////////// switch from one generator to another: keep playing and stop the old one
        weed_generator_end((weed_plant_t *)cfile->ext_src);
        fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
        if (CURRENT_CLIP_IS_VALID && (cfile->achans == 0 || cfile->frames > 0)) {
          // in case we switched to bg clip, and bg clip was gen
          // otherwise we will get killed in generator_start
          mainw->current_file = -1;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (hotkey < FX_KEYS_MAX_VIRTUAL) {
    if (inc_count == 2) {
      mainw->num_tr_applied++; // increase trans count
      if (mainw->active_sa_clips == SCREEN_AREA_FOREGROUND) {
        mainw->active_sa_clips = SCREEN_AREA_BACKGROUND;
      }
      if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_BACKGROUND);
      if (mainw->num_tr_applied == 1 && !is_modeswitch) {
        mainw->blend_file = mainw->current_file;
      }
    } else if (is_gen && outc_count > 0 && !is_audio_gen && !all_out_alpha) {
      // aha - a generator
      if (!LIVES_IS_PLAYING) {
        // if we are not playing, we will postpone creating the instance
        // this is a workaround for a problem in libvisual
        fg_gen_to_start = hotkey;
        fg_generator_key = hotkey;
        fg_generator_mode = key_modes[hotkey];
        gen_start = TRUE;
      } else if (!fg_modeswitch && mainw->num_tr_applied == 0 && (mainw->is_processing || mainw->preview))  {
        mainw->error = TRUE;
        filter_mutex_unlock(hotkey);
        return FALSE;
      }
    }
  }
  // TODO - unblock template channel changes

  // if the param window is already open, use instance from there
  if (fx_dialog[1] && fx_dialog[1]->key == hotkey && fx_dialog[1]->mode == key_modes[hotkey]) {
    lives_rfx_t *rfx = fx_dialog[1]->rfx;
    new_instance = (weed_plant_t *)rfx->source;
    // add a ref since we will remove one below, the param window should already have a ref
    weed_instance_ref(new_instance);
    update_widget_vis(NULL, hotkey, key_modes[hotkey]); // redraw our paramwindow
    inherited = TRUE;
  } else {
    new_instance = weed_instance_from_filter(filter); //adds a ref
    // if it is a key effect, set key defaults
    if (hotkey < FX_KEYS_MAX_VIRTUAL && key_defaults[hotkey][key_modes[hotkey]]) {
      // TODO - handle compound fx
      /* filter_mutex_unlock(hotkey); */
      /* weed_reinit_effect(new_instance, FALSE); */
      /* filter_mutex_lock(hotkey); */
      apply_key_defaults(new_instance, hotkey, key_modes[hotkey]);
      filter_mutex_unlock(hotkey);
      weed_reinit_effect(new_instance, FALSE);
      filter_mutex_lock(hotkey);
    }
  }

  // record the key so we know whose parameters to record later
  weed_set_int_value(new_instance, WEED_LEAF_HOST_KEY, hotkey);

  inst = new_instance;

  gui = weed_instance_get_gui(inst, FALSE);
  if (gui && weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL)) {
    weed_leaf_delete(gui, WEED_LEAF_EASE_OUT);
  }
  weed_leaf_delete(inst, WEED_LEAF_HOST_EASE_OUT_COUNT);
  weed_leaf_delete(inst, WEED_LEAF_AUTO_EASING);

  if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS))
    weed_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_HOST_FPS);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS))
    weed_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_PREFERRED_FPS);

  while (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
    // handle compound fx
    inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, &error);
    weed_set_int_value(inst, WEED_LEAF_HOST_KEY, hotkey);
  }
  inst = new_instance;

  if (!gen_start) {
    weed_plant_t *inst, *next_inst = NULL;
    inst = new_instance;

start1:
    if (weed_call_init_func(inst) != WEED_SUCCESS) {
      char *filter_name, *tmp;
      filter = weed_filters[idx];
      filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
      d_print(_("Failed to start instance %s, (%s)\n"), filter_name, (tmp = weed_error_to_text(error)));
      lives_free(tmp);
      lives_free(filter_name);
      key_to_instance[hotkey][key_modes[hotkey]] = NULL;

deinit2:

      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE))
        next_inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, &error);
      else next_inst = NULL;

      weed_call_deinit_func(inst);

      if (next_inst && weed_get_boolean_value(next_inst, WEED_LEAF_HOST_INITED, &error) == WEED_TRUE) {
        // handle compound fx
        inst = next_inst;
        goto deinit2;
      }

      inst = new_instance;

      if (hotkey < FX_KEYS_MAX_VIRTUAL) {
        if (is_trans) {
          // TODO: - do we need this ? is_trans is always FALSE !
          mainw->num_tr_applied--;
          if (mainw->num_tr_applied == 0) {
            if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_FOREGROUND);
            if (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) {
              mainw->active_sa_clips = SCREEN_AREA_FOREGROUND;
	      // *INDENT-OFF*
	    }}}}
      // *INDENT-ON*

      weed_instance_unref(inst);
      if (is_audio_gen) mainw->agen_needs_reinit = FALSE;
      mainw->error = TRUE;
      filter_mutex_unlock(hotkey);
      return FALSE;
    }

    if ((inst = get_next_compound_inst(inst)) != NULL) goto start1;

    inst = new_instance;
    filter = weed_filters[idx];
  }

  if (is_gen && outc_count > 0 && !is_audio_gen && !all_out_alpha) {
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
    num_tr_applied = mainw->num_tr_applied;
    if (fg_modeswitch) mainw->num_tr_applied = 0; // force to fg

    key_to_instance[hotkey][key_modes[hotkey]] = inst;
    // enable param recording, in case the instance was obtained from a param window
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)) weed_leaf_delete(inst, WEED_LEAF_HOST_NORECORD);

    error = weed_generator_start(inst, hotkey);
    if (error != 0) {
      int weed_error;
      char *filter_name;
      filter_mutex_lock(hotkey);
      weed_call_deinit_func(inst);
      weed_instance_unref(inst);
      weed_instance_unref(inst);
      if (error != 2) {
        filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, &weed_error);
        d_print(_("Unable to start generator %s (error code: %d)\n"), filter_name, error);
        lives_free(filter_name);
      } else mainw->error = TRUE;
      if (mainw->num_tr_applied && mainw->current_file > -1) {
        bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;
      } else {
        fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
      }
      if (fg_modeswitch) mainw->num_tr_applied = num_tr_applied;
      key_to_instance[hotkey][key_modes[hotkey]] = NULL;
      filter_mutex_unlock(hotkey);
      if (!mainw->multitrack) {
        if (!LIVES_IS_PLAYING) {
          switch_clip(1, mainw->current_file, TRUE);
        }
      }
      return FALSE;
    }

    if (playing_file == -1 && mainw->gen_started_play) {
      return TRUE;
    }

    // weed_generator_start can change the instance
    //inst = weed_instance_obtain(hotkey, key_modes[hotkey]);/////

    // TODO - problem if modeswitch triggers playback
    // hence we do not allow mixing of generators and non-gens on the same key
    if (fg_modeswitch) mainw->num_tr_applied = num_tr_applied;
    if (fg_generator_key != -1) {
      pthread_mutex_lock(&mainw->event_list_mutex);
      mainw->rte |= (GU641 << fg_generator_key);
      pthread_mutex_unlock(&mainw->event_list_mutex);
      mainw->clip_switched = TRUE;
    }
    if (bg_generator_key != -1 && !fg_modeswitch) {
      pthread_mutex_lock(&mainw->event_list_mutex);
      mainw->rte |= (GU641 << bg_generator_key);
      pthread_mutex_unlock(&mainw->event_list_mutex);
      if (hotkey < prefs->rte_keys_virtual) {
        if (rte_window && !mainw->is_rendering && !mainw->multitrack) {
          rtew_set_keych(bg_generator_key, TRUE);
        }
        if (mainw->ce_thumbs) ce_thumbs_set_keych(bg_generator_key, TRUE);
      }
    }
  }

  if (rte_keys == hotkey) {
    mainw->rte_keys = rte_keys;
    mainw->blend_factor = weed_get_blend_factor(rte_keys);
  }

  // need to do this *before* calling append_filter_map_event
  key_to_instance[hotkey][key_modes[hotkey]] = inst;

  // enable param recording, in case the instance was obtained from a param window
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)) weed_leaf_delete(inst, WEED_LEAF_HOST_NORECORD);

  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS) && !is_gen) {
    ticks_t actual_ticks = mainw->startticks; ///< use the "thoretical" time
    //ticks_t actual_ticks = mainw->clock_ticks; ///< use the "thoretical" time
    uint64_t rteval, new_rte;
    pthread_mutex_lock(&mainw->event_list_mutex);
    event_list = append_filter_init_event(mainw->event_list, actual_ticks, idx, -1, hotkey, inst);
    if (!mainw->event_list) mainw->event_list = event_list;
    init_events[hotkey] = get_last_event(mainw->event_list);
    ntracks = weed_leaf_num_elements(init_events[hotkey], WEED_LEAF_IN_TRACKS);
    pchains[hotkey] = filter_init_add_pchanges(mainw->event_list, inst, init_events[hotkey], ntracks, 0);
    rteval = mainw->rte;
    new_rte = GU641 << (hotkey);
    if (!(rteval & new_rte)) rteval |= new_rte;
    create_filter_map(rteval); // we create filter_map event_t * array with ordered effects
    mainw->event_list = append_filter_map_event(mainw->event_list, actual_ticks, filter_map);
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }

  if (!inherited) weed_instance_unref(inst); // release the ref we added earlier

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


weed_error_t weed_call_init_func(weed_plant_t *inst) {
  weed_plant_t *filter;
  weed_error_t error = WEED_SUCCESS;
  uint64_t rseed;

  weed_instance_ref(inst);
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
  weed_set_boolean_value(inst, WEED_LEAF_HOST_INITED, WEED_TRUE);
  weed_set_boolean_value(inst, WEED_LEAF_HOST_UNUSED, WEED_TRUE);
  return error;
}


weed_error_t weed_call_deinit_func(weed_plant_t *instance) {
  // filter_mutex MUST be locked
  weed_plant_t *filter;
  weed_error_t error = WEED_SUCCESS;

  //g_print("TMINGS: %d %ld and %d %ld nth\n", thcount, totth, nthcount, totnth);

  weed_instance_ref(instance);

  filter = weed_instance_get_filter(instance, FALSE);

  if (weed_plant_has_leaf(filter, WEED_LEAF_DEINIT_FUNC)) {
    if (weed_get_boolean_value(instance, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE) {
      weed_instance_unref(instance);
      return WEED_SUCCESS;
    } else {
      weed_deinit_f deinit_func =
        (weed_deinit_f)weed_get_funcptr_value(filter, WEED_LEAF_DEINIT_FUNC, NULL);
      if (deinit_func) {
        char *cwd = cd_to_plugin_dir(filter);
        error = (*deinit_func)(instance);
        lives_chdir(cwd, FALSE);
        lives_free(cwd);
      }
    }
  }
  weed_set_boolean_value(instance, WEED_LEAF_HOST_INITED, WEED_FALSE);
  weed_leaf_delete(instance, WEED_LEAF_HOST_UNUSED);
  weed_instance_unref(instance);
  weed_instance_unref(instance);
  return error;
}


boolean weed_deinit_effect(int hotkey) {
  // hotkey is 0 based
  // mainw->osc_block should be set before calling this function !
  // caller should also handle mainw->rte

  // filter_mutex_lock(hotkey) must be be called before entering

  weed_plant_t *instance, *inst, *last_inst, *next_inst, *filter, *gui;

  boolean is_modeswitch = FALSE;
  boolean was_transition = FALSE;
  boolean is_audio_gen = FALSE;
  boolean is_video_gen = FALSE;
  int num_in_chans, num_out_chans;

  int error;
  int easing;

  int needs_unlock = -1;

  if (hotkey < 0) {
    is_modeswitch = TRUE;
    hotkey = -hotkey - 1;
  }

  if (hotkey >= FX_KEYS_MAX) return FALSE;

  // adds a ref
  if (!(instance = weed_instance_obtain(hotkey, key_modes[hotkey]))) return TRUE;

  if (LIVES_IS_PLAYING && hotkey < FX_KEYS_MAX_VIRTUAL) {
    if (prefs->allow_easing) {
      // if it's a user key and the plugin supports easing out, we'll do that instead
      weed_plant_t *gui = weed_instance_get_gui(instance, FALSE);
      if (!weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)) {
        uint64_t new_rte = GU641 << (hotkey);
        if (mainw->rte & new_rte) {
          if ((easing = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL))) {
            int myease = cfile->pb_fps * 2.;
            if (easing < myease) {
              weed_set_int_value(gui, WEED_LEAF_EASE_OUT, myease);
              weed_set_int_value(instance, WEED_LEAF_HOST_EASE_OUT_COUNT, myease);
              weed_instance_unref(instance);
              return FALSE;
	      // *INDENT-OFF*
	    }}}}}}
    // *INDENT-ON*
  mainw->blend_palette = WEED_PALETTE_END;

  // disable param recording, in case the instance is still attached to a param window
  weed_set_boolean_value(instance, WEED_LEAF_HOST_NORECORD, WEED_TRUE);

  num_in_chans = enabled_in_channels(instance, FALSE);

  gui = weed_instance_get_gui(instance, FALSE);
  if (gui && weed_get_int_value(gui, WEED_LEAF_EASE_OUT, NULL)) {
    weed_leaf_delete(gui, WEED_LEAF_EASE_OUT);
  }
  weed_leaf_delete(instance, WEED_LEAF_HOST_EASE_OUT_COUNT);
  weed_leaf_delete(instance, WEED_LEAF_AUTO_EASING);

  // handle compound fx
  last_inst = instance;
  while (weed_plant_has_leaf(last_inst, WEED_LEAF_HOST_NEXT_INSTANCE)) last_inst = weed_get_plantptr_value(last_inst,
        WEED_LEAF_HOST_NEXT_INSTANCE, &error);
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

  if (!filter_mutex_trylock(hotkey)) {
    // should fail, but just in case
    needs_unlock = hotkey;
  }
  key_to_instance[hotkey][key_modes[hotkey]] = NULL;
  if (needs_unlock != -1) {
    filter_mutex_unlock(needs_unlock);
    needs_unlock = -1;
  }

  if (hotkey < FX_KEYS_MAX_VIRTUAL) {
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

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE))
    next_inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE,
                                        &error);
  else next_inst = NULL;

  weed_instance_unref(inst); // remove ref from weed_instance_obtain
  weed_call_deinit_func(inst);
  weed_instance_unref(inst);  // free if no other refs

  if (next_inst) {
    // handle compound fx
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
        // all transitions off, so end the bg generator
        // lock the filter mutex if it's not locked
        if (!filter_mutex_trylock(bg_generator_key)) {
          needs_unlock = bg_generator_key; // will get reset to -1
        }
        weed_deinit_effect(bg_generator_key);
        if (needs_unlock != -1) {
          filter_mutex_unlock(needs_unlock);
          needs_unlock = -1;
        }
      }
      mainw->blend_file = -1;
    }
  }
  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && init_events[hotkey] &&
      (prefs->rec_opts & REC_EFFECTS) && num_in_chans > 0) {
    uint64_t rteval, new_rte;
    //ticks_t actual_ticks = mainw->clock_ticks;//lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
    ticks_t actual_ticks = mainw->startticks; ///< use the "thoretical" time
    pthread_mutex_lock(&mainw->event_list_mutex);
    mainw->event_list = append_filter_deinit_event(mainw->event_list, actual_ticks, init_events[hotkey], pchains[hotkey]);
    init_events[hotkey] = NULL;
    if (pchains[hotkey]) lives_free(pchains[hotkey]);
    pchains[hotkey] = NULL;
    rteval = mainw->rte;
    new_rte = GU641 << (hotkey);
    if (rteval & new_rte) {
      rteval ^= new_rte;
      if (rte_window) rtew_set_keych(hotkey, FALSE);
      if (mainw->ce_thumbs) ce_thumbs_set_keych(hotkey, FALSE);
    }
    create_filter_map(rteval); // we create filter_map event_t * array with ordered effects
    mainw->event_list = append_filter_map_event(mainw->event_list, actual_ticks, filter_map);
    pthread_mutex_unlock(&mainw->event_list_mutex);
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
  register int i;

  for (i = FX_KEYS_MAX_VIRTUAL; i < FX_KEYS_MAX; i++) {
    if (key_to_instance[i][0]) {
      // no mutex needed since we are rendering
      weed_deinit_effect(i);
      if (mainw->multitrack && mainw->multitrack->is_rendering && pchains[i]) {
        lives_free(pchains[i]);
        pchains[i] = NULL;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
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
  register int i;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if ((instance = key_to_instance[i][key_modes[i]]) != NULL) {
      weed_plant_t *gui = weed_instance_get_gui(instance, FALSE);
      if (weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)) {
        // no mutex needed since we are rendering. and since we aren't playing it will get deinited now
        weed_deinit_effect(i);
        /// if recording, the deinit_event won't be recorded, since we are not playing now,
        /// this will be handled in deal_with_render_choice() so it need not concern us
	  // *INDENT-OFF*
	}}}
  // *INDENT-ON*
}


/**
   @brief deinit all effects (except generators* during playback)
   this is called on ctrl-0 or on shutdown
     background generators will be killed because their transition will be deinited
*/
void weed_deinit_all(boolean shutdown) {
  int i;
  weed_plant_t *instance;

  mainw->osc_block = TRUE;
  if (rte_window) {
    rtew_set_keygr(-1);
  }

  mainw->rte_keys = -1;
  mainw->last_grabbable_effect = -1;
  if (!LIVES_IS_PLAYING) bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
    if (!shutdown) {
      // maintain braces because of #DEBUG_FILTER_MUTEXES
      if (LIVES_IS_PLAYING && !shutdown && i >= FX_KEYS_PHYSICAL) {
        // meta-physical  keys only shutdown through the interface or via easter-egg keys...
        mainw->osc_block = FALSE;
        return;
      }
      filter_mutex_lock(i);
    } else {
      // on shutdown, another thread might be deadlocked on this
      // so we will unlock it after
      filter_mutex_trylock(i);
    }
    if ((rte_key_is_enabled(1 + i))) {
      if ((instance = weed_instance_obtain(i, key_modes[i])) != NULL) {
        if (shutdown || !LIVES_IS_PLAYING || mainw->current_file == -1 || cfile->ext_src != instance) {
          if (rte_window) rtew_set_keych(i, FALSE);
          if (mainw->ce_thumbs) ce_thumbs_set_keych(i, FALSE);
          weed_deinit_effect(i);
          pthread_mutex_lock(&mainw->event_list_mutex);
          mainw->rte ^= (GU641 << i);
          pthread_mutex_unlock(&mainw->event_list_mutex);
        } else weed_instance_unref(instance);
      }
    }
    filter_mutex_unlock(i);
  }

  mainw->osc_block = FALSE;
}


/**
   @brief registration fn. for video effects with audio input channels
   during playback there is the option of buffering audio sent to the soundcard
   if a video effect requires audio it can register itself here, and the audio buffer will be filled and
   only refreshed after all clients have read from it. Once this has happened, the read / write buffers are swapped for the
   following cycle. This ensures that no client will miss audio samples, at the cost of some minor latency.
   Purely audio filters are run directly during the audio cycle or by the audio caching thread.
*/
static int register_audio_channels(int nchannels) {
  if (!is_realtime_aplayer(prefs->audio_player)) {
    mainw->afbuffer_clients = 0;
    return -1;
  }
  if (nchannels <= 0) return mainw->afbuffer_clients;
  pthread_mutex_lock(&mainw->abuf_frame_mutex);
  if (mainw->afbuffer_clients == 0) {
    init_audio_frame_buffers(prefs->audio_player);
    mainw->afbuffer_clients_read = 0;
  }
  mainw->afbuffer_clients += nchannels;
  pthread_mutex_unlock(&mainw->abuf_frame_mutex);
  return mainw->afbuffer_clients;
}


static int unregister_audio_channels(int nchannels) {
  if (!mainw->audio_frame_buffer) {
    mainw->afbuffer_clients = 0;
    return -1;
  }

  mainw->afbuffer_clients -= nchannels;
  if (mainw->afbuffer_clients <= 0) {
    int i;
    // lock out the audio thread
    pthread_mutex_lock(&mainw->abuf_frame_mutex);
    for (i = 0; i < 2; i++) {
      if (mainw->afb[i]) {
        free_audio_frame_buffer(mainw->afb[i]);
        lives_free(mainw->afb[i]);
        mainw->afb[i] = NULL;
      }
    }
    mainw->audio_frame_buffer = NULL;
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
  }
  return mainw->afbuffer_clients;
}


static boolean fill_audio_channel(weed_plant_t *filter, weed_plant_t *achan) {
  // this is for filter instances with mixed audio / video inputs/outputs
  // uneffected audio is buffered by the audio thread; here we copy / convert it to a video effect's audio channel
  // purely audio filters run in the audio thread

  static lives_audio_buf_t *audbuf;

  if (achan) {
    weed_set_int_value(achan, WEED_LEAF_AUDIO_DATA_LENGTH, 0);
    weed_set_voidptr_value(achan, WEED_LEAF_AUDIO_DATA, NULL);
  }

  if (!mainw->audio_frame_buffer || mainw->audio_frame_buffer->samples_filled <= 0 || mainw->afbuffer_clients == 0) {
    // no audio has been buffered
    return FALSE;
  }

  // lock the buffers

  if (mainw->afbuffer_clients_read == 0) {
    /// when the first client reads, we grab the audio frame buffer and swap the other for writing
    // cast away the (volatile)
    pthread_mutex_lock(&mainw->abuf_frame_mutex);
    audbuf = (lives_audio_buf_t *)mainw->audio_frame_buffer;
    if (audbuf == mainw->afb[0]) mainw->audio_frame_buffer = mainw->afb[1];
    else mainw->audio_frame_buffer = mainw->afb[0];
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
  }

  // push read buffer to channel
  if (achan && audbuf) {
    // convert audio to format requested, and copy it to the audio channel data
    push_audio_to_channel(filter, achan, audbuf);
  }

  if (++mainw->afbuffer_clients_read >= mainw->afbuffer_clients) {
    // all clients have read the data, now we can free it
    free_audio_frame_buffer(audbuf);
    mainw->afbuffer_clients_read = 0;
  }

  return TRUE;
}


int check_filter_chain_palettes(boolean is_bg, int *palette_list, int npals) {
  register int i;
  int palette = WEED_PALETTE_END;

  if (mainw->rte) {
    for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
      if (palette != WEED_PALETTE_END) break;
      if (rte_key_is_enabled(1 + i)) {
        weed_plant_t *instance;
        if (i == fg_generator_key) continue;
        if ((instance = weed_instance_obtain(i, key_modes[i])) != NULL) {
          if (is_bg && enabled_in_channels(instance, FALSE) < 2) continue;
          weed_plant_t *filter = weed_instance_get_filter(instance, TRUE);
          weed_plant_t *channel = get_enabled_channel(instance, (is_bg ? 1 : 0), TRUE);
          if (channel) {
            int nvals;
            weed_plant_t *chantmpl = weed_channel_get_template(channel);
            int *plist = weed_chantmpl_get_palette_list(filter, chantmpl, &nvals);
            for (int j = 0; j < nvals; j++) {
              if (plist[j] == WEED_PALETTE_END) break;
              else for (int k = 0; k < npals; k++) {
                  if (palette_list[k] == WEED_PALETTE_END) break;
                  if (palette_list[k] == plist[j]) {
                    palette = plist[j];
                    break;
		    // *INDENT-OFF*
		  }}
	      if (palette != WEED_PALETTE_END) break;
	    }
            lives_free(plist);
          }
	  weed_instance_unref(instance);
	}}}
    // *INDENT-ON*

    if (palette == WEED_PALETTE_END) {
      if (mainw->ext_playback && (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)) {
        for (i = 0; i < npals; i++) {
          if (palette_list[i] == mainw->vpp->palette) {
            palette = palette_list[i];
            break;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (palette == WEED_PALETTE_END) {
    for (i = 0; i < npals; i++) {
      if (weed_palette_is_pixbuf_palette(palette_list[i])) break;
    }
  }
  if (palette == WEED_PALETTE_END) {
    for (i = 0; i < npals; i++) {
      if (weed_palette_is_rgb(palette_list[i])) break;
    }
  }
  if (palette == WEED_PALETTE_END) palette = palette_list[0];
  return palette;
}

/////////////////////
// special handling for generators (sources)

weed_plant_t *weed_layer_create_from_generator(weed_plant_t *inst, weed_timecode_t tc, int clipno) {
  weed_plant_t *channel, **out_channels;
  weed_plant_t *filter;
  weed_plant_t *chantmpl;
  weed_plant_t *achan;
  weed_process_f process_func;
  int *palette_list;
  int *rowstrides;
  lives_filter_error_t retval;
  weed_error_t ret;
  int npals;
  int num_channels;
  int npl, npl2;
  int palette, opalette;
  int filter_flags = 0, channel_flags;
  int num_in_alpha = 0;
  int width, height, xwidth, xheight;
  int reinits = 0;

  int i;

  boolean did_thread = FALSE;
  boolean needs_reinit = FALSE;
  boolean can_change = FALSE;
  boolean is_bg = TRUE;
  char *cwd;

  if (!inst) return NULL;

  weed_instance_ref(inst);

  out_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_CHANNELS, &num_channels);
  if (!num_channels) {
    weed_instance_unref(inst);
    return NULL;
  }

  if ((channel = get_enabled_channel(inst, 0, FALSE)) == NULL) {
    lives_free(out_channels);
    weed_instance_unref(inst);
    return NULL;
  }

  filter = weed_instance_get_filter(inst, FALSE);
  filter_flags = weed_filter_get_flags(filter);
  chantmpl = weed_channel_get_template(channel);
  palette_list = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);
  palette = WEED_PALETTE_END;

  if (clipno == mainw->current_file) is_bg = FALSE;
  /*
    - just as with normal filters,
    - if the channel template flags have WEED_FILTER_REINIT_ON_PALETTE_CHANGE, then we want to minimise palette changes,
    in this case because reinit may disrupt the output flow from the generator, causing unnecessary visual changes.
    thus unless forced to, we will leave the palette as is. However as with filters,
    There are two points where we can voluntariily alter the palette:
    when the instance has just been inited (since it would be reset anyway)
    and if we are going to reinit anyway due to some other factor (e.g size change).

    when setting the palette we will consider the following:
    - if there are no filters active then look at the player palette. If the player can change palettes then find the best pair.
    else
    - if the gen. is fg,  check the filters to be applied, If 1st filter has reinit on pal. change then try to match its palette
    - otherwise check the longest running palette, ie. intersection of gen. palette(s), filter_n  palettes, filter_n+1 pal. list untiil
    either the intersection of our remaining palettes with filter palettes is the empty set. or we pass all palettes,
    then take intersection with player. Any filters with reinit on pal. change, we first try the current palette, if that doesn't work then
    we check its full palette list as we will be forced to reinit it.
    - if it is bg we will use mainw->blend_palette
    TODO: we should find the transition where the gen. joins, and work backwards and forwards from there

    IF the template has WEED_FILTER_REINIT_ON_ROWSTRIDES_CHANGE or WEED_FILTER_REINIT_ON_SIZE_CHANGE
    then similar rules apply for those values
  */

  /// if we have a choice of palettes, try to match with the first filter, if not, with the player
  // unless it says reinit on palette change. Then as with normal filters we only change after an init / reinit

matchvals:

  if (needs_reinit || weed_get_boolean_value(inst, WEED_LEAF_HOST_UNUSED, NULL) == WEED_TRUE)
    can_change = TRUE;

  needs_reinit = FALSE;

  channel = get_enabled_channel(inst, 0, FALSE);
  chantmpl = weed_channel_get_template(channel);
  channel_flags = weed_chantmpl_get_flags(chantmpl);

  opalette = weed_channel_get_palette(channel);
  if (can_change || !((channel_flags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE))) {
    palette = check_filter_chain_palettes(is_bg, palette_list, npals);
    if (palette != WEED_PALETTE_END) {
      if (palette != opalette && !((channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE) && pixel_size(palette)
                                   != pixel_size(opalette))) {
        weed_channel_set_palette(channel, palette);
      }
    }
  }

  if (weed_plant_has_leaf(inst, WEED_LEAF_IN_CHANNELS)) {
    int num_inc;
    weed_plant_t **in_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_CHANNELS, &num_inc);
    for (i = 0; i < num_inc; i++) {
      if (weed_palette_is_alpha(weed_channel_get_palette(in_channels[i])) &&
          weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_FALSE) {
        num_in_alpha++;
      }
      lives_freep((void **)&in_channels);
    }
  }

  if (num_in_alpha > 0) {
    // if we have mandatory alpha ins, make sure they are filled
    retval = check_cconx(inst, num_in_alpha, &needs_reinit);
    if (retval != FILTER_SUCCESS) {
      lives_free(out_channels);
      weed_instance_unref(inst);
      return channel;
    }
  }

  xwidth = width = weed_channel_get_width(channel);
  xheight = height = weed_channel_get_height(channel);

  if (can_change || (!(channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE) && !(channel_flags
                     & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE))) {
    // if it's in the bg, and letterboxing, set size to maxspect fg clip
    // or if it's fg or bg and we are playing high quality
    if (mainw->num_tr_applied > 0 && !num_in_alpha) {
      if ((mainw->multitrack && !mainw->multitrack->is_rendering && prefs->letterbox_mt)
          || (!mainw->multitrack && prefs->letterbox && (!mainw->is_rendering || mainw->preview_rendering))) {
        int lb_width, lb_height, opwidth, opheight;
        boolean can_resize = FALSE, is_bg = FALSE;
        if (IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->playing_file
            && mainw->files[mainw->blend_file]->ext_src == inst) {
          is_bg = TRUE;
        }
        if (is_bg || prefs->no_lb_gens) {
          if (is_bg) {
            lb_width = mainw->files[mainw->playing_file]->hsize;
            lb_height = mainw->files[mainw->playing_file]->vsize;
          } else {
            lb_width = mainw->files[mainw->blend_file]->hsize;
            lb_height = mainw->files[mainw->blend_file]->vsize;
          }

          if (mainw->ext_playback && (mainw->vpp->capabilities & VPP_CAN_RESIZE) != 0) can_resize = TRUE;
          get_player_size(&opwidth, &opheight);

          // op size = fg frame in player
          get_letterbox_sizes(&opwidth, &opheight, &lb_width, &lb_height, can_resize);

          if (!is_bg || prefs->no_lb_gens) {
            xwidth = lb_width;
            xheight = lb_height;
          } else {
            opwidth = lb_width;
            opheight = lb_height;
            can_resize = FALSE;

            if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_WIDTH)) {
              lb_width = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
            } else lb_width = DEF_GEN_WIDTH;
            if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_HEIGHT)) {
              lb_height = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
            } else lb_height = DEF_GEN_HEIGHT;
          }
        }
        if (is_bg && !prefs->no_lb_gens) {
          if ((mainw->multitrack && !mainw->multitrack->is_rendering && prefs->letterbox_mt)
              || (!mainw->multitrack && prefs->letterbox && (!mainw->is_rendering || mainw->preview_rendering))) {
            get_letterbox_sizes(&opwidth, &opheight, &lb_width, &lb_height, can_resize);
            xwidth = lb_width;
            xheight = lb_height;
          }
        }
      }
    } else {
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_WIDTH)) {
        xwidth = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
      } else xwidth = DEF_GEN_WIDTH;
      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_HEIGHT)) {
        xheight = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
      } else xheight = DEF_GEN_HEIGHT;
    }
  }

  if (xwidth * xheight == 0) {
    if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_WIDTH)) {
      xwidth = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
    } else xwidth = DEF_GEN_WIDTH;
    if (weed_plant_has_leaf(chantmpl, WEED_LEAF_HOST_HEIGHT)) {
      xheight = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
    } else xheight = DEF_GEN_HEIGHT;
  }

  rowstrides = weed_channel_get_rowstrides(channel, &npl);

  if (weed_plant_has_leaf(filter, WEED_LEAF_ALIGNMENT_HINT)) {
    int rowstride_alignment_hint = weed_get_int_value(filter, WEED_LEAF_ALIGNMENT_HINT, NULL);
    if (rowstride_alignment_hint > ALIGN_DEF) {
      THREADVAR(rowstride_alignment_hint) = rowstride_alignment_hint;
    }
  }

  /// store original values (width / height)
  for (i = 0; (channel = get_enabled_channel(inst, i, FALSE)); i++) {
    if (xwidth != weed_channel_get_width(channel) ||
        xheight != weed_channel_get_height(channel)) {
      int nplanes;
      void **pd = weed_channel_get_pixel_data_planar(channel, &nplanes);
      //g_print("size is %d X %d\n", xwidth, xheight);
      for (int j = 0; j < nplanes; j++) lives_free(pd[j]);
      lives_free(pd);
      weed_set_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);
      //g_print("set channel size to %d X %d\n", xwidth, xheight);
      set_channel_size(filter, channel, xwidth, xheight);
    }
  }

  channel = get_enabled_channel(inst, 0, FALSE);

  if (!create_empty_pixel_data(channel, FALSE, TRUE)) return NULL;

  /// store original values (width / height)
  xwidth = width;
  xheight = height;

  width = weed_channel_get_width(channel);
  //cpalette = weed_channel_get_palette(channel);
  //width /= weed_palette_get_pixels_per_macropixel(cpalette); // convert width to channel macropixels
  height = weed_channel_get_height(channel);

  if (xwidth != width || xheight != height) {
    if (channel_flags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE) {
      needs_reinit = TRUE;
    }
  }

  if (!needs_reinit && (channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)) {
    int *rowstrides2 = weed_channel_get_rowstrides(channel, &npl2);
    if (rowstrides_differ(npl, rowstrides, npl2, rowstrides2))
      needs_reinit = TRUE;
    lives_free(rowstrides2);
  }
  lives_free(rowstrides);

  if (filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA)
    weed_channel_set_gamma_type(channel, WEED_GAMMA_LINEAR);
  else
    weed_channel_set_gamma_type(channel, cfile->gamma_type);

  if (needs_reinit) {
    retval = weed_reinit_effect(inst, FALSE);
    if (retval == FILTER_ERROR_COULD_NOT_REINIT
        || retval == FILTER_ERROR_INVALID_PLUGIN
        || retval == FILTER_ERROR_INVALID_FILTER) {
      lives_free(out_channels);
      weed_instance_unref(inst);
      return NULL;
    }
    goto matchvals;
  }

  if (i == 0 && mainw->current_file == mainw->playing_file) {
    cfile->hsize = width;
    cfile->vsize = height;
    set_main_title(cfile->file_name, 0);
  }

  // if we have an optional audio channel, we can push audio to it
  if ((achan = get_enabled_audio_channel(inst, 0, TRUE)) != NULL) {
    fill_audio_channel(filter, achan);
  }

  if (prefs->pb_quality == PB_QUALITY_LOW && mainw->inst_fps > 0. && mainw->inst_fps
      < weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL))
    weed_set_double_value(inst, WEED_LEAF_TARGET_FPS, mainw->inst_fps);
  else {
    if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS))
      weed_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_HOST_FPS);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS))
      weed_leaf_copy(inst, WEED_LEAF_TARGET_FPS, filter, WEED_LEAF_PREFERRED_FPS);
  }

  if (CURRENT_CLIP_IS_VALID) weed_set_double_value(inst, WEED_LEAF_FPS, mainw->inst_fps);

  cwd = cd_to_plugin_dir(filter);

procfunc1:
  // cannot lock the filter here as we may be multithreading
  if (filter_flags & WEED_FILTER_HINT_MAY_THREAD) {
    retval = process_func_threaded(inst, tc);
    if (retval != FILTER_ERROR_DONT_THREAD) did_thread = TRUE;
  }
  if (!did_thread) {
    // normal single threaded version
    process_func = (weed_process_f)weed_get_funcptr_value(filter, WEED_LEAF_PROCESS_FUNC, NULL);
    if (process_func) ret = (*process_func)(inst, tc);
  }
  lives_free(out_channels);

  //if (weed_palette_has_alpha(palette)) lives_layer_set_opaque(channel);

  if (achan) {
    int nachans;
    float **abuf = weed_channel_get_audio_data(achan, &nachans);
    for (i = 0; i < nachans; i++) lives_freep((void **)&abuf[i]);
    lives_freep((void **)&abuf);
  }

  if (ret == WEED_ERROR_REINIT_NEEDED) {
    if (reinits == 1) {
      weed_instance_unref(inst);
      return channel;
    }
    retval = weed_reinit_effect(inst, FALSE);
    if (retval == FILTER_ERROR_COULD_NOT_REINIT
        || retval == FILTER_ERROR_INVALID_PLUGIN
        || retval == FILTER_ERROR_INVALID_FILTER) {
      weed_instance_unref(inst);
      return channel;
    }
    reinits = 1;
    goto procfunc1;
  }

  weed_instance_unref(inst);

  lives_chdir(cwd, FALSE);
  lives_free(cwd);

#ifdef NEW_SCRAPFILE
  // TODO: it would be better perhaps to save the original input frame(s) to the (a) scrapfile
  // so that rendering could be done at higher quality and with more continuity,
  // however we would need one file per 'ephemeral' input, and disk throughput becomes an even bigger issue
  if (mainw->record && !mainw->record_paused) {
    if (!((prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
          (mainw->vpp->capabilities & VPP_CAN_RETURN))) {
      save_to_scrap_file(channel, clipno);
      /// need to record scrap_file offset for the input clip
    }
  }
#endif

  return channel;
}


int weed_generator_start(weed_plant_t *inst, int key) {
  // key here is zero based
  // make an "ephemeral clip"

  // cf. yuv4mpeg.c
  // start "playing" but receive frames from a plugin

  // filter_mutex MUST be locked, on FALSE return is unlocked

  weed_plant_t **out_channels, *channel, *filter, *achan;
  char *filter_name;
  int error, num_channels;
  int old_file = mainw->current_file, blend_file = mainw->blend_file;
  int palette;

  // create a virtual clip
  int new_file = 0;
  boolean is_bg = FALSE;

  //weed_instance_ref(inst);
  //filter_mutex_lock(key);

  if (LIVES_IS_PLAYING) mainw->ignore_clipswitch = TRUE;

  if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR && mainw->num_tr_applied == 0) {
    close_current_file(0);
    old_file = -1;
  }

  if (mainw->is_processing && !mainw->preview) {
    filter_mutex_unlock(key);
    return 1;
  }

  if ((mainw->preview || (CURRENT_CLIP_IS_VALID && cfile->opening)) &&
      (mainw->num_tr_applied == 0 || mainw->blend_file == -1 || mainw->blend_file == mainw->current_file)) {
    filter_mutex_unlock(key);
    return 2;
  }

  if (!LIVES_IS_PLAYING) mainw->pre_src_file = mainw->current_file;

  if (old_file != -1 && mainw->blend_file != -1 && mainw->blend_file != mainw->current_file &&
      mainw->num_tr_applied > 0 && mainw->files[mainw->blend_file] &&
      mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
    ////////////////////////// switching background generator: stop the old one first
    weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file = mainw->blend_file;
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
    mainw->play_start = mainw->play_end = 1;
    mainw->startticks = mainw->currticks;
  } else {
    if (!create_cfile(-1, filter_name, TRUE)) {
      filter_mutex_unlock(key);
      return 3;
    }

    cfile->clip_type = CLIP_TYPE_GENERATOR;

    lives_snprintf(cfile->type, 40, "generator:%s", filter_name);
    lives_snprintf(cfile->file_name, PATH_MAX, "generator: %s", filter_name);
    lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "generator: %s", filter_name);
    lives_free(filter_name);

    // open as a clip with 1 frame
    cfile->start = cfile->end = cfile->frames = 1;
  }

  new_file = mainw->current_file;
  cfile->ext_src = inst;
  cfile->ext_src_type = LIVES_EXT_SRC_FILTER;

  if (is_bg) {
    mainw->blend_file = mainw->current_file;
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

  out_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_CHANNELS, &num_channels);
  if (num_channels  == 0) {
    filter_mutex_unlock(key);
    cfile->ext_src = NULL;
    cfile->ext_src_type = LIVES_EXT_SRC_NONE;
    close_current_file(mainw->pre_src_file);
    return 4;
  }

  if (!(channel = get_enabled_channel(inst, 0, FALSE))) {
    lives_free(out_channels);
    filter_mutex_unlock(key);
    cfile->ext_src = NULL;
    cfile->ext_src_type = LIVES_EXT_SRC_NONE;
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

  // if the generator has an optional audio in channel, enable it: TODO - make this configurable
  if ((achan = get_audio_channel_in(inst, 0)) != NULL) {
    if (weed_plant_has_leaf(achan, WEED_LEAF_DISABLED)) weed_leaf_delete(achan, WEED_LEAF_DISABLED);
    register_audio_channels(1);
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
        mainw->blend_file = mainw->current_file;
        if (old_file != -1) mainw->current_file = old_file;
      }
    } else {
      mainw->blend_file = mainw->current_file;
      mainw->current_file = old_file;
      mainw->play_start = cfile->start;
      mainw->play_end = cfile->end;
      mainw->playing_sel = FALSE;
    }

    new_rte = GU641 << key;
    pthread_mutex_lock(&mainw->event_list_mutex);
    if (!(mainw->rte & new_rte)) mainw->rte |= new_rte;
    pthread_mutex_unlock(&mainw->event_list_mutex);

    mainw->last_grabbable_effect = key;
    if (rte_window) rtew_set_keych(key, TRUE);
    if (mainw->ce_thumbs) {
      ce_thumbs_set_keych(key, TRUE);

      // if effect was auto (from ACTIVATE data connection), leave all param boxes
      // otherwise, remove any which are not "pinned"
      if (!mainw->fx_is_auto) ce_thumbs_add_param_box(key, !mainw->fx_is_auto);
    }
    filter_mutex_unlock(key);

    weed_instance_unref(inst);  // release ref from weed_instance_from_filter, normally we would do this on return

    if (mainw->play_window) {
      lives_widget_queue_draw(mainw->play_window);
    }
    mainw->gen_started_play = TRUE;

    if (!mainw->osc_auto) start_playback_async(6);
    else {
      on_playall_activate(NULL, NULL);
      // need to set this after playback ends; this stops the key from being activated (again) in effects.c
      // also stops the (now defunct instance being unreffed)
      mainw->gen_started_play = TRUE;
    }
    return 0;
  } else {
    // already playing

    if (old_file != -1 && mainw->files[old_file]) {
      if (IS_NORMAL_CLIP(old_file)) mainw->pre_src_file = old_file;
      mainw->current_file = old_file;
    }

    if (!is_bg || old_file == -1 || old_file == new_file) {
      //if (mainw->current_file == -1) mainw->current_file = new_file;

      if (new_file != old_file) {
        mainw->new_clip = new_file;

        /* filter_mutex_unlock(key); // else we get stuck pulling a frame */
        /* do_quick_switch(new_file); */
        /* filter_mutex_lock(key); */
        /* mainw->force_show = TRUE; */

        // switch audio clip
        /* if (is_realtime_aplayer(prefs->audio_player) && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) */
        /*     && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || prefs->audio_src == AUDIO_SRC_EXT))) { */
        /*   switch_audio_clip(new_file, TRUE); */
        /* } */

        if (mainw->files[mainw->new_blend_file]) mainw->blend_file = mainw->new_blend_file;
        if (!is_bg && IS_VALID_CLIP(blend_file)) mainw->blend_file = blend_file;
        mainw->new_blend_file = -1;
      } else {
        lives_widget_show_all(mainw->playframe);
        resize(1);
        lives_widget_set_opacity(mainw->playframe, 1.);
      }
      //if (old_file==-1) mainw->whentostop=STOP_ON_VID_END;
    } else {
      if (mainw->current_file == -1) {
        /* mainw->current_file = new_file; */
        /* if (is_realtime_aplayer(prefs->audio_player) && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) */
        /*     && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || prefs->audio_src == AUDIO_SRC_EXT))) { */
        /*   switch_audio_clip(new_file, TRUE); */
        /* } */
      } else mainw->blend_file = new_file;
      if (mainw->ce_thumbs && (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND
                               || mainw->active_sa_clips == SCREEN_AREA_FOREGROUND))
        ce_thumbs_highlight_current_clip();
    }
    if (mainw->cancelled == CANCEL_GENERATOR_END) mainw->cancelled = CANCEL_NONE;
  }

  filter_mutex_unlock(key);

  return 0;
}


void wge_inner(weed_plant_t *inst) {
  weed_plant_t *next_inst = NULL;
  // called from weed_generator_end() below and also called directly after playback ends

  // during playback, MUST call this with filter_mutex locked

  // unrefs inst 1 times

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
    int error;
    int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, &error);
    key_to_instance[key][key_modes[key]] = NULL;
  }

  while (inst) {
    next_inst = get_next_compound_inst(inst);
    weed_call_deinit_func(inst);
    inst = next_inst;
  }
}


void weed_generator_end(weed_plant_t *inst) {
  // generator has stopped for one of the following reasons:
  // effect was de-inited; clip (bg/fg) was changed; playback stopped with fg

  // during playback, MUST be called with filter_mutex locked

  lives_whentostop_t wts = mainw->whentostop;
  boolean is_bg = FALSE;
  boolean clip_switched = mainw->clip_switched;
  int current_file = mainw->current_file, pre_src_file = mainw->pre_src_file;

  if (LIVES_IS_PLAYING) mainw->ignore_clipswitch = TRUE;

  if (!inst) {
    LIVES_WARN("inst was NULL !");
  }

  if (mainw->blend_file != -1 && mainw->blend_file != current_file && mainw->files[mainw->blend_file] &&
      mainw->files[mainw->blend_file]->ext_src == inst) is_bg = TRUE;
  else mainw->new_blend_file = mainw->blend_file;

  if (LIVES_IS_PLAYING && !is_bg && mainw->whentostop == STOP_ON_VID_END) {
    // we will close the file after playback stops
    // and also unref the instance
    mainw->cancelled = CANCEL_GENERATOR_END;
    return;
  }

  /////// update the key checkboxes //////
  /// TODO: do we want to do this if switching from one gen to another ?
  if (rte_window  && !mainw->is_rendering && !mainw->multitrack) {
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

  if (inst && get_audio_channel_in(inst, 0)) {
    unregister_audio_channels(1);
  }

  if (is_bg) {
    if (mainw->blend_layer) check_layer_ready(mainw->blend_layer);
    key_to_instance[bg_generator_key][bg_generator_mode] = NULL;
    pthread_mutex_lock(&mainw->event_list_mutex);
    if (rte_key_is_enabled(1 + bg_generator_key)) mainw->rte ^= (GU641 << bg_generator_key);
    pthread_mutex_unlock(&mainw->event_list_mutex);
    bg_gen_to_start = bg_generator_key = bg_generator_mode = -1;
    pre_src_file = mainw->pre_src_file;
    mainw->pre_src_file = mainw->current_file;
  } else {
    if (mainw->frame_layer) check_layer_ready(mainw->frame_layer);
    key_to_instance[fg_generator_key][fg_generator_mode] = NULL;
    pthread_mutex_lock(&mainw->event_list_mutex);
    if (rte_key_is_enabled(1 + fg_generator_key)) mainw->rte ^= (GU641 << fg_generator_key);
    pthread_mutex_unlock(&mainw->event_list_mutex);
    fg_gen_to_start = fg_generator_key = fg_generator_clip = fg_generator_mode = -1;
    if (mainw->blend_file == mainw->current_file) mainw->blend_file = -1;
  }

  if (inst) wge_inner(inst); // removes 1 ref

  // if the param window is already open, show any reinits now
  if (fx_dialog[1]) {
    if (is_bg) update_widget_vis(NULL, bg_generator_key, bg_generator_mode); // redraw our paramwindow
    else update_widget_vis(NULL, fg_generator_key, fg_generator_mode); // redraw our paramwindow
  }

  if (!is_bg && cfile->achans > 0 && cfile->clip_type == CLIP_TYPE_GENERATOR) {
    // we started playing from an audio clip
    cfile->frames = cfile->start = cfile->end = 0;
    cfile->ext_src = NULL;
    cfile->ext_src_type = LIVES_EXT_SRC_NONE;
    cfile->clip_type = CLIP_TYPE_DISK;
    cfile->hsize = cfile->vsize = 0;
    cfile->pb_fps = cfile->fps = prefs->default_fps;
    return;
  }

  /// here we must be very careful, because we are about to close the clip which is either playing as fg or bg
  /// in the case of background, we just switch to it very briefly to close it, then back to the fg clip
  /// in case the generator was ended by a change of bg clip, we restore the new bg clip
  ///
  /// in the case of the fg clip we close it and try to switch to another valid clip
  /// we switch back to the old (invalid) clip after this, while setting mainw->new_clip to the new one
  /// the player will detect the invalid clip and jump to the switch point to handle the changeover cleanly
  if (is_bg) {
    mainw->pre_src_file = mainw->current_file;
    mainw->current_file = mainw->blend_file;
    mainw->blend_file = mainw->new_blend_file;
    mainw->new_blend_file = -1;
    // close generator file and switch to original file if possible
    if (!cfile || cfile->clip_type != CLIP_TYPE_GENERATOR) {
      break_me("close non-gen file");
      LIVES_WARN("Close non-generator file 2");
      mainw->current_file = mainw->pre_src_file;
    } else {
      cfile->ext_src = NULL;
      cfile->ext_src_type = LIVES_EXT_SRC_NONE;
      close_current_file(mainw->pre_src_file);
    }
    if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) ce_thumbs_update_current_clip();
  } else {
    // close generator file and switch to original file if possible
    if (!cfile || cfile->clip_type != CLIP_TYPE_GENERATOR) {
      LIVES_WARN("Close non-generator file 1");
    } else {
      cfile->ext_src = NULL;
      cfile->ext_src_type = LIVES_EXT_SRC_NONE;
      if (cfile->achans == 0) {
        current_file = mainw->current_file;
        close_current_file(mainw->pre_src_file);
        if (LIVES_IS_PLAYING) {
          mainw->new_clip = mainw->pre_src_file;
          mainw->current_file = current_file;
        }
      }
    }
    if (mainw->current_file == current_file) mainw->clip_switched = clip_switched;
  }

  if (is_bg) {
    mainw->current_file = current_file;
    mainw->pre_src_file = pre_src_file;
    mainw->whentostop = wts;
  }

  if (mainw->current_file == -1) mainw->cancelled = CANCEL_GENERATOR_END;
}


void weed_bg_generator_end(weed_plant_t *inst) {
  // when we stop with a bg generator we want it to be restarted next time
  // i.e we will need a new clip for it
  int bg_gen_key = bg_generator_key;

  // filter_mutex unlocked

  /// ref the instance so it isn't deleted
  weed_instance_ref(inst);
  weed_generator_end(inst); // unrefs inst
  bg_gen_to_start = bg_gen_key;
}


boolean weed_playback_gen_start(void) {
  // init generators on pb. We have to do this after audio startup
  // filter_mutex unlocked
  weed_plant_t *inst = NULL, *filter;
  weed_plant_t *next_inst = NULL, *orig_inst;

  char *filter_name;

  weed_error_t error = WEED_SUCCESS;

  int bgs = bg_gen_to_start;
  boolean was_started = FALSE;

  if (mainw->is_rendering) return TRUE;

  if (fg_gen_to_start == bg_gen_to_start) bg_gen_to_start = -1;

  if (cfile->frames == 0 && fg_gen_to_start == -1 && bg_gen_to_start != -1) {
    fg_gen_to_start = bg_gen_to_start;
    bg_gen_to_start = -1;
  }

  mainw->osc_block = TRUE;

  if (fg_gen_to_start != -1) {
    filter_mutex_lock(fg_gen_to_start);
    // check is still gen

    if (enabled_in_channels(weed_filters[key_to_fx[fg_gen_to_start][key_modes[fg_gen_to_start]]], FALSE) == 0) {
      orig_inst = inst = weed_instance_obtain(fg_gen_to_start, key_modes[fg_gen_to_start]);

      if (inst) {
geninit1:
        error = weed_call_init_func(inst);
        if (error != WEED_SUCCESS) {
          weed_plant_t *oldinst = inst;
          orig_inst = inst = weed_instance_obtain(fg_gen_to_start, key_modes[fg_gen_to_start]);
          weed_instance_unref(oldinst);
          key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]] = NULL;
          if (inst) {
            char *tmp;
            filter = weed_instance_get_filter(inst, TRUE);
            filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
            d_print(_("Failed to start generator %s (%s)\n"), filter_name,
                    (tmp = lives_strdup(weed_error_to_text(error))));
            lives_free(tmp);
            lives_free(filter_name);

deinit4:
            next_inst = get_next_compound_inst(inst);

            weed_call_deinit_func(inst);

            if (next_inst) {
              // handle compound fx
              inst = next_inst;
              if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_TRUE) goto deinit4;
            }
          }

          // unref twice to destroy
          weed_instance_unref(orig_inst);
          weed_instance_unref(orig_inst);

          filter_mutex_unlock(fg_gen_to_start);
          fg_gen_to_start = -1;
          cfile->ext_src = NULL;
          cfile->ext_src_type = LIVES_EXT_SRC_NONE;
          mainw->osc_block = FALSE;
          return FALSE;
        }

        if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
          inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
          goto geninit1;
        }

        inst = orig_inst;
        filter = weed_instance_get_filter(inst, TRUE);

        if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS)) {
          int current_file = mainw->current_file;
          mainw->current_file = fg_generator_clip;
          cfile->fps = weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, NULL);
          set_main_title(cfile->file_name, 0);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->fps);
          mainw->current_file = current_file;
        }

        mainw->clip_switched = TRUE;
        cfile->ext_src = inst;
        cfile->ext_src_type = LIVES_EXT_SRC_FILTER;
        weed_instance_unref(inst);
      }
    }

    filter_mutex_unlock(fg_gen_to_start);
    fg_gen_to_start = -1;
  }

  inst = NULL;

  if (bg_gen_to_start != -1) {
    filter_mutex_lock(bg_gen_to_start);

    // check is still gen
    if (mainw->num_tr_applied > 0
        && enabled_in_channels(weed_filters[key_to_fx[bg_gen_to_start][key_modes[bg_gen_to_start]]],
                               FALSE) == 0) {
      if ((inst = weed_instance_obtain(bg_gen_to_start, key_modes[bg_gen_to_start])) == NULL) {
        // restart bg generator
        if (!weed_init_effect(bg_gen_to_start)) {
          mainw->osc_block = FALSE;
          filter_mutex_unlock(bg_gen_to_start);
          return TRUE;
        }
        was_started = TRUE;
      }

      if (!inst) inst = weed_instance_obtain(bgs, key_modes[bgs]);

      orig_inst = inst;

      if (!inst) {
        // 2nd playback
        int playing_file = mainw->playing_file;
        mainw->playing_file = -100; //kludge to stop playing a second time
        if (!weed_init_effect(bg_gen_to_start)) {
          filter_mutex_unlock(bg_gen_to_start);
          error++;
        }
        mainw->playing_file = playing_file;
        orig_inst = weed_instance_obtain(bg_gen_to_start, key_modes[bg_gen_to_start]);
      } else {
        if (!was_started) {
          orig_inst = inst;
genstart2:

          // TODO - error check
          weed_call_init_func(inst);

          // handle compound fx
          if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
            inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
            goto genstart2;
          }
        }
      }

      inst = orig_inst;

      if (error != WEED_SUCCESS) {
undoit:
        key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]] = NULL;
        if (inst) {
          char *tmp;
          filter = weed_instance_get_filter(inst, TRUE);
          filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
          d_print(_("Failed to start generator %s, (%s)\n"), filter_name,
                  (tmp = lives_strdup(weed_error_to_text(error))));
          lives_free(tmp);
          lives_free(filter_name);

deinit5:

          next_inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);

          weed_call_deinit_func(inst);
          weed_instance_unref(inst);
          weed_instance_unref(inst);

          if (next_inst) {
            // handle compound fx
            inst = next_inst;
            weed_instance_ref(inst);
            goto deinit5;
          }
        }

        mainw->blend_file = -1;
        pthread_mutex_lock(&mainw->event_list_mutex);
        if (rte_key_is_enabled(1 + ABS(bg_gen_to_start))) mainw->rte ^= (GU641 << ABS(bg_gen_to_start));
        pthread_mutex_unlock(&mainw->event_list_mutex);
        mainw->osc_block = FALSE;
        filter_mutex_unlock(bg_gen_to_start);
        bg_gen_to_start = -1;
        return FALSE;
      }

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
        mainw->blend_file = mainw->current_file;
        mainw->files[mainw->blend_file]->ext_src = inst;
        mainw->files[mainw->blend_file]->ext_src_type = LIVES_EXT_SRC_FILTER;
        mainw->current_file = current_file;
      }
    }
    filter_mutex_unlock(bg_gen_to_start);
  }

  orig_inst = inst;

setgui1:

  // handle compound fx
  if ((inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)) != NULL) {
    goto setgui1;
  }

  if (orig_inst) weed_instance_unref(orig_inst);
  bg_gen_to_start = -1;
  mainw->osc_block = FALSE;

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
    lives_free(wtmpls);
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
  lives_free(in_ptmpls);
  return count;
}


int set_copy_to(weed_plant_t *inst, int pnum, lives_rfx_t *rfx, boolean update) {
  // if we update a plugin in_parameter, evaluate any "copy_value_to"
  // filter_mutex MUST be unlocked
  weed_plant_t *paramtmpl;
  weed_plant_t *pgui, *in_param2;
  weed_plant_t *in_param = weed_inst_in_param(inst, pnum, FALSE, FALSE); // use this here in case of compound fx
  int copyto = -1;
  int key = -1;
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

  if (copyto >= rfx->num_params) return -1;

  if (rfx->params[copyto].change_blocked) return -1; ///< prevent loops

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

    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
      key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      filter_mutex_lock(key);
    }
    if (flags & WEED_PARAMETER_VALUE_PER_CHANNEL) {
      int *ign_array;
      int nvals = weed_leaf_num_elements(in_param2, WEED_LEAF_VALUE);
      int vsize = 1;
      if (param_type2 == WEED_PARAM_COLOR) {
        int cspace = weed_get_int_value(paramtmpl2, WEED_LEAF_COLORSPACE, NULL);
        if (cspace == WEED_COLORSPACE_RGB) vsize = 3;
        else vsize = 4;
      }
      weed_leaf_copy(paramtmpl2, "host_new_def_backup", paramtmpl2, WEED_LEAF_NEW_DEFAULT);
      weed_leaf_copy(in_param2, "host_value_backup", in_param2, WEED_LEAF_VALUE);
      weed_leaf_copy(paramtmpl2, WEED_LEAF_NEW_DEFAULT, in_param, WEED_LEAF_VALUE);
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
      weed_leaf_copy(paramtmpl2, WEED_LEAF_NEW_DEFAULT, paramtmpl2, "host_new_def_backup");
      weed_leaf_delete(paramtmpl2, "host_new_def_backup");
      lives_freep((void **)&ign_array);
    } else {
      weed_leaf_dup(in_param2, in_param, WEED_LEAF_VALUE);
      if (key != -1) filter_mutex_unlock(key);
    }
  }
  return copyto;
}


void rec_param_change(weed_plant_t *inst, int pnum) {
  // should be called with event_list_mutex unlocked !
  ticks_t actual_ticks;
  weed_plant_t *in_param;
  int key;
  int error;

  weed_instance_ref(inst);

  // do not record changes for the floating fx dialog box (rte window params)
  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NORECORD)
      && weed_get_boolean_value(inst, WEED_LEAF_HOST_NORECORD, &error)) {
    weed_instance_unref(inst);
    return;
  }

  // do not record changes for generators - those get recorded to scrap_file or ascrap_file
  if (!enabled_in_channels(inst, FALSE)) {
    weed_instance_unref(inst);
    return;
  }

  //actual_ticks = mainw->clock_ticks;//lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
  actual_ticks = mainw->startticks; ///< use the "thoretical" time

  pthread_mutex_lock(&mainw->event_list_mutex);
  key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, &error);

  in_param = weed_inst_in_param(inst, pnum, FALSE, FALSE);

  mainw->event_list = append_param_change_event(mainw->event_list, actual_ticks, pnum, in_param, init_events[key], pchains[key]);
  pthread_mutex_unlock(&mainw->event_list_mutex);

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

  if (!inst) {
    filter_mutex_unlock(hotkey);
    return;
  }

  pnum = get_nth_simple_param(inst, 0);

  if (pnum == -1)  {
    weed_instance_unref(inst);
    filter_mutex_unlock(hotkey);
    return;
  }

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  in_param = in_params[pnum];
  lives_free(in_params);

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
  filter_mutex_unlock(hotkey);
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

  mainw->osc_block = TRUE;

  if ((inst = key_to_instance[key][mode]) != NULL) {
    // return details for instance
    type = weed_instance_get_type(inst, TRUE);
  } else type = weed_filter_get_type(filter, TRUE, TRUE);

  mainw->osc_block = FALSE;
  return type;
}


lives_fx_cat_t rte_keymode_get_category(int key, int mode) {
  weed_plant_t *filter;
  int idx;
  lives_fx_cat_t cat;

  key--;
  if (!rte_keymode_valid(key + 1, mode, TRUE)) return LIVES_FX_CAT_NONE;

  if ((idx = key_to_fx[key][mode]) == -1) return LIVES_FX_CAT_NONE;
  if ((filter = weed_filters[idx]) == NULL) return LIVES_FX_CAT_NONE;

  else cat = weed_filter_categorise(filter,
                                      enabled_in_channels(filter, FALSE),
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
  if (i == FX_KEYS_MAX) next_free_key = -1;
  return free_key;
}


boolean weed_delete_effectkey(int key, int mode) {
  // delete the effect binding for key/mode and move higher numbered slots down
  // also moves the active mode if applicable
  // returns FALSE if there was no effect bound to key/mode

  char *tmp;

  boolean was_started = FALSE;

  int oldkeymode = key_modes[--key];
  int orig_mode = mode;
  int modekey = key;

  if (key_to_fx[key][mode] == -1) return FALSE;

  filter_mutex_lock(key);
  if (key < FX_KEYS_MAX_VIRTUAL) free_key_defaults(key, mode);

  for (; mode < (key < FX_KEYS_MAX_VIRTUAL ? prefs->max_modes_per_key : 1); mode++) {
    mainw->osc_block = TRUE;
    if (key >= FX_KEYS_MAX_VIRTUAL || mode == prefs->max_modes_per_key - 1 || key_to_fx[key][mode + 1] == -1) {
      if (key_to_instance[key][mode]) {
        was_started = TRUE;
        if (key_modes[key] == mode) modekey = -key - 1;
        else key_modes[key] = mode;
        weed_deinit_effect(modekey);
        key_modes[key] = oldkeymode;
      }

      key_to_fx[key][mode] = -1;

      if (mode == orig_mode && key_modes[key] == mode) {
        key_modes[key] = 0;
        if (was_started) {
          if (key_to_fx[key][0] != -1) {
            if (!weed_init_effect(modekey)) {
              // TODO
              filter_mutex_lock(key);
            }
          } else {
            pthread_mutex_lock(&mainw->event_list_mutex);
            if (rte_key_is_enabled(1 + key)) mainw->rte ^= (GU641 << key);
            pthread_mutex_unlock(&mainw->event_list_mutex);
          }
        }
      }

      break; // quit the loop
    } else if (key < FX_KEYS_MAX_VIRTUAL) {
      filter_mutex_unlock(key);
      rte_switch_keymode(key + 1, mode, (tmp = make_weed_hashname
                                         (key_to_fx[key][mode + 1], TRUE, FALSE, 0, FALSE)));
      lives_free(tmp);
      filter_mutex_lock(key);
      key_defaults[key][mode] = key_defaults[key][mode + 1];
      key_defaults[key][mode + 1] = NULL;
    }
  }

  if (key >= FX_KEYS_MAX_VIRTUAL && key < next_free_key) next_free_key = key;

  mainw->osc_block = FALSE;
  if (key_modes[key] > orig_mode) key_modes[key]--;
  filter_mutex_unlock(key);

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
      mode >= (key < FX_KEYS_MAX_VIRTUAL ? prefs->max_modes_per_key : 1)) return FALSE;
  if (key_to_fx[--key][mode] == -1) return FALSE;
  return TRUE;
}


int rte_keymode_get_filter_idx(int key, int mode) {
  // key is 1 based
  if (key < 1 || key > FX_KEYS_MAX || mode < 0 ||
      mode >= (key < FX_KEYS_MAX_VIRTUAL ? prefs->max_modes_per_key : 1)) return -1;
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
  // not to be confused with rte_get_modespk() which returns the maximum possible

  register int i;

  if (key < 1 || key > FX_KEYS_MAX) return -1;

  key--;

  for (i = 0; i < (key < FX_KEYS_MAX_VIRTUAL ? prefs->max_modes_per_key : 1); i++) {
    if (key_to_fx[key][i] == -1) return --i;
  }
  return --i;
}


weed_plant_t *rte_keymode_get_instance(int key, int mode) {
  weed_plant_t *inst;
  if (!rte_keymode_valid(key, mode, FALSE)) return NULL;
  mainw->osc_block = TRUE;
  if ((inst = weed_instance_obtain(--key, mode)) == NULL) {
    mainw->osc_block = FALSE;
    return NULL;
  }
  mainw->osc_block = FALSE;
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
  int oldmode;
  int blend_file;
  lives_whentostop_t whentostop = mainw->whentostop;
  int real_key;

  if (key == 0) {
    if ((key = mainw->rte_keys) == -1) return FALSE;
  } else key--;

  filter_mutex_lock(key);

  real_key = key;

  oldmode = key_modes[key];

  if (key_to_fx[key][0] == -1) {
    filter_mutex_unlock(key);
    return FALSE; // nothing is mapped to effect key
  }

  if (newmode == -1) {
    // cycle forwards
    if (oldmode == prefs->max_modes_per_key - 1 || key_to_fx[key][oldmode + 1] == -1) {
      newmode = 0;
    } else {
      newmode = key_modes[key] + 1;
    }
  }

  if (newmode == -2) {
    // cycle backwards
    newmode = key_modes[key] - 1;
    if (newmode < 0) {
      for (newmode = prefs->max_modes_per_key - 1; newmode >= 0; newmode--) {
        if (key_to_fx[key][newmode] != -1) break;
      }
    }
  }

  if (newmode < 0 || newmode > rte_key_getmaxmode(key + 1)) {
    filter_mutex_unlock(key);
    return FALSE;
  }

  if (key_to_fx[key][newmode] == -1) {
    filter_mutex_unlock(key);
    return FALSE;
  }

  if (rte_window) rtew_set_mode_radio(key, newmode);
  if (mainw->ce_thumbs) ce_thumbs_set_mode_combo(key, newmode);

  mainw->osc_block = TRUE;
  mainw->blend_palette = WEED_PALETTE_END;

  // TODO - block template channel changes

  if ((inst = weed_instance_obtain(key, oldmode)) != NULL) {  // adds a ref
    if (enabled_in_channels(inst, FALSE) == 2 && enabled_in_channels(weed_filters[key_to_fx[key][newmode]], FALSE) == 2) {
      // transition --> transition, allow any bg generators to survive
      key = -key - 1;
    }
  }

  if (oldmode != newmode) {
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

    mainw->blend_file = blend_file;

    if (inst) {
      if (!weed_init_effect(key)) {
        weed_instance_unref(inst);
        // TODO - unblock template channel changes
        mainw->whentostop = whentostop;
        key = real_key;
        pthread_mutex_lock(&mainw->event_list_mutex);
        if (rte_key_is_enabled(1 + key)) mainw->rte ^= (GU641 << key);
        pthread_mutex_unlock(&mainw->event_list_mutex);
        mainw->osc_block = FALSE;
        return FALSE;
      }
      weed_instance_unref(inst);
      if (mainw->ce_thumbs) ce_thumbs_add_param_box(real_key, TRUE);
    }
    // TODO - unblock template channel changes
    mainw->whentostop = whentostop;
  }

  filter_mutex_unlock(real_key);
  mainw->osc_block = FALSE;
  return TRUE;
}


/**
   @brief

  we will add a filter_class at the next free slot for key, and return the slot number
  if idx is -1 (probably meaning the filter was not found), we return -1
  if all slots are full, we return -3
  currently, generators and non-generators cannot be mixed on the same key (causes problems if the mode is switched)
  in this case a -2 is returned
*/
int weed_add_effectkey_by_idx(int key, int idx) {
  boolean has_gen = FALSE;
  boolean has_non_gen = FALSE;

  int i;

  if (idx == -1) return -1;

  key--;

  for (i = 0; i < prefs->max_modes_per_key; i++) {
    if (key_to_fx[key][i] != -1) {
      if (enabled_in_channels(weed_filters[key_to_fx[key][i]], FALSE) == 0
          && has_video_chans_out(weed_filters[key_to_fx[key][i]], TRUE))
        has_gen = TRUE;
      else has_non_gen = TRUE;
    } else {
      if ((enabled_in_channels(weed_filters[idx], FALSE) == 0 && has_non_gen &&
           !all_outs_alpha(weed_filters[idx], TRUE) && has_video_chans_out(weed_filters[idx], TRUE)) ||
          (enabled_in_channels(weed_filters[idx], FALSE) > 0 && has_gen)) return -2;
      key_to_fx[key][i] = idx;
      if (rte_window && !mainw->is_rendering && !mainw->multitrack) {
        // if rte window is visible add to combo box
        char *tmp;
        rtew_combo_set_text(key, i, (tmp = rte_keymode_get_filter_name(key + 1, i, FALSE)));
        lives_free(tmp);

        // set in ce_thumb combos
        if (mainw->ce_thumbs) ce_thumbs_reset_combo(key);
      }
      return i;
    }
  }
  return -3;
}


int weed_add_effectkey(int key, const char *hashname, boolean fullname) {
  // add a filter_class by hashname to an effect_key
  int idx = weed_get_idx_for_hashname(hashname, fullname);
  return weed_add_effectkey_by_idx(key, idx);
}


int rte_switch_keymode(int key, int mode, const char *hashname) {
  // this is called when we switch the filter_class bound to an effect_key/mode
  // filter mutex unlocked
  weed_plant_t *inst;
  int oldkeymode = key_modes[--key];
  int id = weed_get_idx_for_hashname(hashname, TRUE), tid;
  boolean osc_block;
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
    filter_mutex_unlock(key);
    return -2;
  }

  osc_block = mainw->osc_block;
  mainw->osc_block = TRUE;

  // must be done before switching the key_to_fx, as we need to know number of in_parameter_templates
  if (key_defaults[key][mode]) free_key_defaults(key, mode);

  if ((inst = weed_instance_obtain(key, mode)) != NULL) {
    key_modes[key] = mode;
    weed_deinit_effect(-key - 1); // set is_modeswitch
    key_to_fx[key][mode] = id;
    if (!weed_init_effect(-key - 1)) {
      // TODO
      filter_mutex_lock(key);
    }
    key_modes[key] = oldkeymode;
    weed_instance_unref(inst);
  } else key_to_fx[key][mode] = id;

  filter_mutex_unlock(key);
  mainw->osc_block = osc_block;

  return 0;
}


void rte_swap_fg_bg(void) {
  int key = fg_generator_key;
  int mode = fg_generator_mode;
  mainw->blend_palette = WEED_PALETTE_END;

  if (key != -1) {
    fg_generator_clip = -1;
  }
  fg_generator_key = bg_generator_key;
  fg_generator_mode = bg_generator_mode;
  if (fg_generator_key != -1) {
    fg_generator_clip = mainw->current_file;
  }
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
  // filter_mutex should be locked for the key during realtime processing

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
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          valds[j] = nvalds[j];
          lives_free(nvalds);
        } else valds[j] = get_default_element_double(param, j, 1, 0);
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
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              valis[k] = nvalis[k];
              valis[k + 1] = nvalis[k + 1];
              valis[k + 2] = nvalis[k + 2];
              lives_free(nvalis);
            } else {
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
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              valds[k] = nvalds[k];
              valds[k + 1] = nvalds[k + 1];
              valds[k + 2] = nvalds[k + 2];
              lives_free(nvalds);
            } else {
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
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              valis[k] = nvalis[k];
              valis[k + 1] = nvalis[k + 1];
              valis[k + 2] = nvalis[k + 2];
              valis[k + 3] = nvalis[k + 3];
              lives_free(nvalis);
            } else {
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
            xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
            if (xnum > k) {
              nvalds = weed_get_double_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
              valds[k] = nvalds[k];
              valds[k + 1] = nvalds[k + 1];
              valds[k + 2] = nvalds[k + 2];
              valds[k + 3] = nvalds[k + 3];
              lives_free(nvalds);
            } else {
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
          xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
          if (xnum > j) {
            nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
            valis[j] = nvalis[j];
            lives_free(nvalis);
          } else valis[j] = get_default_element_int(param, j, 1, 0);
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
          xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
          if (xnum > j) {
            nvalis = weed_get_int_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
            valis[j] = nvalis[j];
            lives_free(nvalis);
          } else valis[j] = get_default_element_int(param, j, 1, 0);
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
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalis = weed_get_boolean_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          valis[j] = nvalis[j];
          lives_free(nvalis);
        } else valis[j] = get_default_element_bool(param, j);
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
        xnum = weed_leaf_num_elements((weed_plant_t *)lpc[j], WEED_LEAF_VALUE);
        if (xnum > j) {
          nvalss = weed_get_string_array((weed_plant_t *)lpc[j], WEED_LEAF_VALUE, NULL);
          valss[j] = lives_strdup(nvalss[j]);
          for (k = 0; k < xnum; k++) lives_free(nvalss[k]);
          lives_free(nvalss);
        } else valss[j] = get_default_element_string(param, j);
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
  int32_t numhash;
  char *xhashname = fix_hashnames(hashname);
  int type = 0;
  register int i;

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

  if (pkg && *pkg) {
    char *filename;

    if (weed_plant_has_leaf(filter, WEED_LEAF_PLUGIN_INFO)) {
      plugin_info = weed_get_plantptr_value(filter, WEED_LEAF_PLUGIN_INFO, NULL);
      if (weed_plant_has_leaf(plugin_info, WEED_LEAF_PACKAGE_NAME)) {
        filename = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
      } else {
        plugin_name = weed_get_string_value(plugin_info, WEED_LEAF_HOST_PLUGIN_NAME, NULL);
        lives_snprintf(plugin_fname, PATH_MAX, "%s", plugin_name);
        lives_freep((void **)&plugin_name);
        get_filename(plugin_fname, TRUE);
        filename = F2U8(plugin_fname);
      }
    } else {
      return FALSE;
    }

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

  register int i;

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
    ival = 0x44454557;
    lives_write_le_buffered(fd, &ival, 4, TRUE);
    ival = 1; /// version
    lives_write_le_buffered(fd, &ival, 4, TRUE);
    lives_write_le_buffered(fd, &nplanes, 4, TRUE);
    lives_write_le_buffered(fd, &pal, 4, TRUE);
    lives_write_le_buffered(fd, &width, 4, TRUE);
    lives_write_le_buffered(fd, &height, 4, TRUE);

    totsize += 28;

    if (weed_get_boolean_value(layer, WEED_LEAF_HOST_PIXEL_DATA_CONTIGUOUS, NULL) == WEED_TRUE) {
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

  if (WEED_IS_LAYER(plant)) pd_needed = 1;

  if (!mem) lives_write_le_buffered(fd, &i, 4, TRUE); // write number of leaves
  else {
    lives_memcpy(*mem, &i, 4);
    *mem += 4;
  }

  totsize += 4;

  // serialise the "type" leaf first, so that we know this is a new plant when deserialising
  totsize += weed_leaf_serialise(fd, plant, WEED_LEAF_TYPE, TRUE, mem);
  lives_free(proplist[0]);

  for (i = 1; (prop = proplist[i]); i++) {
    // write each leaf and key
    if (pd_needed > 0) {
      // write pal, height, rowstrides before pixel_data.
      if (!lives_strcmp(prop, WEED_LEAF_PIXEL_DATA)) {
        if (pd_reqs > 3) pd_needed = 0;
        else {
          ++pd_needed;
          lives_free(prop);
          continue;
        }
      } else {
        if (pd_reqs < 4 && (!strcmp(prop, WEED_LEAF_WIDTH) || !strcmp(prop, WEED_LEAF_HEIGHT)
                            || !lives_strcmp(prop, WEED_LEAF_CURRENT_PALETTE)
                            || !strcmp(prop, WEED_LEAF_ROWSTRIDES))) {
          if (++pd_reqs == 4 && pd_needed > 1) {
            totsize += weed_leaf_serialise(fd, plant, prop, TRUE, mem);
            lives_free(prop);
            totsize += weed_leaf_serialise(fd, plant, WEED_LEAF_PIXEL_DATA, TRUE, mem);
            pd_needed  = 0;
            continue;
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*

    totsize += weed_leaf_serialise(fd, plant, prop, TRUE, mem);
    lives_free(prop);
  }
  lives_freep((void **)&proplist);
  return totsize;
}


static int32_t weed_plant_mutate(weed_plantptr_t plant, int32_t newtype) {
  // beware of mutant plants....
  int32_t flags = weed_leaf_get_flags(plant, WEED_LEAF_TYPE);
  // clear the default flags to allow the "type" leaf to be altered
  weed_leaf_set_flags(plant, WEED_LEAF_TYPE, flags & ~(WEED_FLAG_IMMUTABLE));
  weed_set_int_value(plant, WEED_LEAF_TYPE, newtype);
  // lock the "type" leaf again so it cannot be altered accidentally
  weed_leaf_set_flags(plant, WEED_LEAF_TYPE, WEED_FLAG_IMMUTABLE);
  return weed_plant_get_type(plant);
}


#define REALIGN_MAX (40 * 1024 * 1024)  /// 40 MiB "should be enough for anyone"
#define MAX_FRAME_SIZE MILLIONS(100)
#define MAX_FRAME_SIZE64 3019898880

static int realign_typeleaf(int fd, weed_plant_t *plant) {
  uint8_t buff[12];
  const char XMATCH[8] = {4, 0, 0, 0, 't', 'y', 'p', 'e'};
  int type, nl;
  register uint64_t count = REALIGN_MAX;
  register int len;

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
      break_me("ink. seed type in w.l. deser");
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
          rs = weed_layer_get_rowstrides(plant, &nplanes); {
            if (nplanes != ne) {
              LIVES_WARN("Invalid planes in retrieved layer");
	      // *INDENT-OFF*
	    }}}}}
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

          weed_layer_pixel_data_free(layer);
          values[0] = lives_calloc(ALIGN_CEIL(vlen64_tot + EXTRA_BYTES, 16) / 16, 16);

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
            weed_set_boolean_value(plant, WEED_LEAF_HOST_PIXEL_DATA_CONTIGUOUS, WEED_TRUE);
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

        values[j] = lives_calloc(ALIGN_CEIL(vlen + EXTRA_BYTES, 16) / 16, 16);
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


        }
        if (j >= nplanes) lives_free(values[j]);
      }
    }
    if (plant) {
      weed_set_voidptr_array(plant, WEED_LEAF_PIXEL_DATA, nplanes, values);
      while (nplanes != 0) values[--nplanes] = NULL; /// prevent "values" from being freed because we copy-by-value
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
                msg = lives_strdup_printf("Type mismatch in deserialization: expected %d, got %d\n",
                                          type, *ints);
                lives_free(ints);
                LIVES_ERROR(msg);
                lives_free(msg);
                type = -7;
                goto done;
              }
              // type already OK
            }
          } else {
            weed_leaf_set(plant, key, st, ne, (void *)ints);
          }
        } else {
          weed_leaf_set(plant, key, st, ne, (void *)ints);
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
          boolean add_leaf = TRUE;
          if (check_ptrs) {
            switch (weed_plant_get_type(plant)) {
            case WEED_PLANT_LAYER:
              if (lives_strcmp(key, WEED_LEAF_PIXEL_DATA)) {
                add_leaf = FALSE;
              }
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

  ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);
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

    if (capable->byte_order == LIVES_BIG_ENDIAN && prefs->bigendbug) {
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

  ctmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_channels);
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

    if (capable->byte_order == LIVES_BIG_ENDIAN && prefs->bigendbug) {
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
      ctmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_chans);
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


void reset_frame_and_clip_index(void) {
  if (!mainw->clip_index) {
    mainw->clip_index = (int *)lives_malloc(sizint);
    mainw->clip_index[0] = -1;
  }
  if (!mainw->frame_index) {
    mainw->frame_index = (int64_t *)lives_malloc(8);
    mainw->frame_index[0] = 0;
  }
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
  // call with filter_mutex locked
  // apply key/mode param defaults to a filter instance
  weed_plant_t **defs, **params;
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
  int nparams = num_in_params(filter, FALSE, FALSE);
  register int i, j = 0;

  if (nparams == 0) return;

  defs = key_defaults[key][mode];
  if (!defs) return;

  do {
    params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
    nparams = num_in_params(inst, FALSE, FALSE);

    for (i = 0; i < nparams; i++) {
      if (!is_hidden_param(inst, i))
        weed_leaf_dup(params[i], defs[j], WEED_LEAF_VALUE);
      j++;
    }
    lives_free(params);
  } while (weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL));
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

  if (key >= FX_KEYS_MAX_VIRTUAL || mode >= prefs->max_modes_per_key) return;

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
        weed_leaf_dup(key_defs[i], params[i - poffset], WEED_LEAF_VALUE);
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
    for (int j = 0; j < prefs->max_modes_per_key; j++) {
      if (key_defaults[i][j]) return TRUE;
    }
  }
  return FALSE;
}


