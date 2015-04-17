// effects.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_EFFECTS_H
#define HAS_LIVES_EFFECTS_H

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#else
#include "../libweed/weed.h"
#endif

// general effects
typedef enum {
  LIVES_FX_CAT_NONE=0,
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

boolean do_effect(lives_rfx_t *rfx, boolean is_preview); ///< defined as extern in paramwindow.c

void on_render_fx_activate(LiVESMenuItem *menuitem, lives_rfx_t *rfx);

///////////////// real time effects

// render
void on_realfx_activate(LiVESMenuItem *, livespointer rfx);
boolean on_realfx_activate_inner(int type, lives_rfx_t *rfx);

lives_render_error_t realfx_progress(boolean reset);

// key callbacks

boolean textparm_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data);

boolean grabkeys_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);  ///< for accel groups
boolean grabkeys_callback_hook(LiVESToggleButton *button, livespointer user_data);  ///< for widgets

boolean rte_on_off_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);  ///< for accel groups
boolean rte_on_off_callback_hook(LiVESToggleButton *, livespointer user_data);  ///< for widgets

boolean rtemode_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);  ///< for accel groups
boolean rtemode_callback_hook(LiVESToggleButton *, livespointer user_data);  ///< for widgets

boolean swap_fg_bg_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

weed_plant_t *get_blend_layer(weed_timecode_t tc);

weed_plant_t *on_rte_apply(weed_plant_t *main_layer, int opwidth, int opheight, weed_timecode_t tc);


void deinterlace_frame(weed_plant_t *layer, weed_timecode_t tc);


#endif
