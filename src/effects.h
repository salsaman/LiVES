// effects.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2021
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_EFFECTS_H
#define HAS_LIVES_EFFECTS_H

// general effects
typedef enum {
  LIVES_FX_CAT_NONE = 0,
  LIVES_FX_CAT_VIDEO_GENERATOR,
  LIVES_FX_CAT_AV_GENERATOR,
  LIVES_FX_CAT_AUDIO_GENERATOR,
  LIVES_FX_CAT_DATA_GENERATOR,
  LIVES_FX_CAT_DATA_VISUALISER,
  LIVES_FX_CAT_DATA_PROCESSOR,
  LIVES_FX_CAT_DATA_SOURCE,
  LIVES_FX_CAT_TRANSITION,
  LIVES_FX_CAT_AV_TRANSITION,
  LIVES_FX_CAT_VIDEO_TRANSITION,
  LIVES_FX_CAT_AUDIO_TRANSITION,
  LIVES_FX_CAT_EFFECT,
  LIVES_FX_CAT_VIDEO_EFFECT,
  LIVES_FX_CAT_AUDIO_EFFECT,
  LIVES_FX_CAT_UTILITY,
  LIVES_FX_CAT_COMPOSITOR,
  LIVES_FX_CAT_AUDIO_MIXER,
  LIVES_FX_CAT_TAP,
  LIVES_FX_CAT_SPLITTER,
  LIVES_FX_CAT_CONVERTER,
  LIVES_FX_CAT_AUDIO_VOL,
  LIVES_FX_CAT_ANALYSER,
  LIVES_FX_CAT_VIDEO_ANALYSER,
  LIVES_FX_CAT_AUDIO_ANALYSER
} lives_fx_cat_t;

/// audio filter type (any, analyser only, non analyser only)
typedef enum {
  AF_TYPE_ANY,
  AF_TYPE_A,
  AF_TYPE_NONA
} lives_af_t;

char *lives_fx_cat_to_text(lives_fx_cat_t cat, boolean plural) WARN_UNUSED;

#include "effects-weed.h"

boolean do_effect(lives_rfx_t *, boolean is_preview); ///< defined as extern in paramwindow.c

///////////////// real time effects

// render
void on_realfx_activate(LiVESMenuItem *, lives_rfx_t *rfx);
boolean on_realfx_activate_inner(int type, lives_rfx_t *rfx);

lives_render_error_t realfx_progress(boolean reset);

// key callbacks


#define	FXKEY_SOFT_DEINIT			(1ull << 0)

typedef enum {
  activator_none,
  activator_ui,
  activator_pconx,
} activator_type;

#define FX_KEY_PHYSICAL			1 // 1 ... prefs->n_fxkeys;
#define FX_KET_VIRTUAL			2 // prefs->nfxkeys + 1 .... FX_KEYS_MAX_VIRTUAL

typedef struct {
  uint64_t uid;
  int type;
  int nmodes;
  int active_mode;
  activator_type last_activator;
  weed_instance_t *instances;
  weed_filter_t *filters;
  uint64_t flags;
} rte_key_desc;

extern rte_key_desc fx_key_defs[FX_KEYS_MAX_VIRTUAL];

void fx_keys_init(void);

boolean textparm_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval,
                          LiVESXModifierType mod, livespointer user_data);

boolean grabkeys_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType,
                          livespointer user_data);  ///< for accel groups
boolean grabkeys_callback_hook(LiVESToggleButton *button, livespointer user_data);  ///< for widgets

boolean rte_on_off_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType,
                            livespointer user_data);  ///< for accel groups
boolean rte_on_off_callback_fg(LiVESToggleButton *, livespointer user_data);  ///< for widgets

boolean rtemode_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType,
                         livespointer user_data);  ///< for accel groups
boolean rtemode_callback_hook(LiVESToggleButton *, livespointer user_data);  ///< for widgets

boolean swap_fg_bg_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

frames64_t get_blend_frame(weed_timecode_t tc);

weed_layer_t *on_rte_apply(weed_layer_t **, int opwidth, int opheight, ticks_t tc, boolean dry_run);

void deinterlace_frame(weed_layer_t *, ticks_t tc);

// rte keys state
void rte_keymodes_backup(int nkeys);
void rte_keymodes_restore(int nkeys);

#define rte_keys_reset() rte_key_toggle(0)

void rte_keys_update(void);

// hotkey is 1 based
lives_result_t rte_key_toggle(int key);

lives_result_t _rte_key_toggle(int key, activator_type acti);

boolean rte_key_on_off(int key, boolean on);

// key is 0 based (now)
boolean rte_key_is_enabled(int key, boolean ign_soft_deinits);

#define rte_getmodespk() prefs->rte_modes_per_key

#endif
