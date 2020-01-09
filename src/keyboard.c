// keyboard.c
// LiVES
// (c) G. Finch 2004 - 2017 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "effects.h"
#include "callbacks.h"

#define NEEDS_TRANSLATION 1<<15

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef ENABLE_OSC

static void handle_omc_events(void) {
  // check for external controller events

#ifdef OMC_MIDI_IMPL
  int midi_check_rate;
  boolean gotone;
#endif

  int i;

#ifdef OMC_JS_IMPL
  if (mainw->ext_cntl[EXT_CNTL_JS]) {
    char *string = js_mangle();
    if (string != NULL) {
      omc_process_string(OMC_JS, string, FALSE, NULL);
      lives_free(string);
    }
  }
#endif // OMC_JS_IMPL

#ifdef OMC_MIDI_IMPL
  midi_check_rate = prefs->midi_check_rate;
#ifdef ALSA_MIDI
  if (prefs->use_alsa_midi) midi_check_rate = 1; // because we loop for events in midi_mangle()
#endif
  if (mainw->ext_cntl[EXT_CNTL_MIDI]) {
    do {
      gotone = FALSE;
      for (i = 0; i < midi_check_rate; i++) {
        char *string = midi_mangle();
        if (string != NULL) {
          omc_process_string(OMC_MIDI, string, FALSE, NULL);
          lives_free(string);
#ifdef ALSA_MIDI
          if (prefs->use_alsa_midi) gotone = TRUE;
#endif
        }
      }
    } while (gotone);
  }
#endif // OMC_MIDI_IMPL
}

#endif


boolean ext_triggers_poll(livespointer data) {
#ifdef USE_GLIB
  static int priority = G_PRIORITY_DEFAULT;
#endif
  boolean needs_check = FALSE;
  if (mainw->is_exiting) return FALSE;

  /* if (!LIVES_IS_PLAYING && !mainw->is_processing && CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_VIDEODEV) { */
  /*   int pwidth = lives_widget_get_allocation_width(mainw->start_image) - H_RESIZE_ADJUST; */
  /*   int pheight = lives_widget_get_allocation_height(mainw->start_image) - V_RESIZE_ADJUST; */
  /*   mainw->camframe = pull_lives_pixbuf_at_size(mainw->current_file, 1, get_image_ext_for_type(cfile->img_type), */
  /* 						cfile->fps, pwidth, pheight, LIVES_INTERP_BEST); */
  /*   load_start_image(1); */
  /* } */

  // check for external controller events
#ifdef ENABLE_JACK
#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jack_trans_poll) {
    needs_check = TRUE;
    lives_jack_poll(); ///<   check for jack transport start/stop
  }
#endif
#endif

#ifdef ENABLE_OSC
  if (mainw->ext_cntl[EXT_CNTL_MIDI] || mainw->ext_cntl[EXT_CNTL_JS]) {
    needs_check = TRUE;
    handle_omc_events(); ///< check for playback start triggered by js, MIDI, etc.
  }
#endif

  /// if we have OSC we will poll it here,
#ifdef ENABLE_OSC
  if (prefs->osc_udp_started) {
    needs_check = TRUE;
    lives_osc_poll(NULL);
  }
#endif

  if (mainw->kb_timer_end) {
    mainw->kb_timer_end = FALSE;
#if GTK_CHECK_VERSION(3, 0, 0)
    // below 3, 0, 0 the timer is removed by a function
    return FALSE;
#endif
  }

#ifdef USE_GLIB
  if (needs_check) {
    if (priority != G_PRIORITY_DEFAULT) {
      g_source_set_priority(g_main_context_find_source_by_id(NULL, mainw->kb_timer), (priority = G_PRIORITY_DEFAULT));
    }
    return TRUE;
  } else {
    if (priority != G_PRIORITY_DEFAULT_IDLE) {
      g_source_set_priority(g_main_context_find_source_by_id(NULL, mainw->kb_timer), (priority = G_PRIORITY_DEFAULT_IDLE));
    }
  }
#endif

  return TRUE;
}


// unused, but left for future reference in case i becomes useful
#if defined HAVE_X11
LiVESFilterReturn filter_func(LiVESXXEvent *xevent, LiVESXEvent *event, livespointer data) {
  // filter events at X11 level and act on key press/release
  uint32_t modifiers = 0;
  return LIVES_FILTER_CONTINUE; // this is most likely handled in key_press_or_release() now
}
#endif


boolean key_press_or_release(LiVESWidget *widget, LiVESXEventKey *event, livespointer user_data) {
  boolean ret = pl_key_function(event->type == LIVES_KEY_PRESS, event->keyval, event->state);
  //if (ckey !=0 && cached_key == 0) g_print("unaching key\n");
  return ret;
}


void handle_cached_keys(void) {
  // smooth out auto repeat for VJ scratch keys
  if (cfile->pb_fps == 0.) return;
  if (cached_key != 0) {
    lives_accel_groups_activate(LIVES_WIDGET_OBJECT(LIVES_MAIN_WINDOW_WIDGET),
                                (uint32_t)cached_key, (LiVESXModifierType)(cached_mod | LIVES_SPECIAL_MASK));
  }
}


boolean pl_key_function(boolean down, uint16_t unicode, uint16_t keymod) {
  // translate key events
  // plugins can also call this with a unicode key to pass key events to LiVES
  // (via a polling mechanism)

  // mask for ctrl and alt
  //LiVESXModifierType state=(LiVESXModifierType)(keymod&(LIVES_CONTROL_MASK|LIVES_ALT_MASK));

  // down is a press, up is a release

  if (!down) {
    // up...
    if (keymod & NEEDS_TRANSLATION) {
      switch (unicode) {
      // some keys need translating when a modifier is held down
      case (key_left) :
      case (key_left2):
        if (cached_key == LIVES_KEY_Left) cached_key = 0;
        return FALSE;
      case (key_right) :
      case (key_right2):
        if (cached_key == LIVES_KEY_Right) cached_key = 0;
        return FALSE;
      case (key_up)  :
      case (key_up2):
        if (cached_key == LIVES_KEY_Up) cached_key = 0;
        return FALSE;
      case (key_down) :
      case (key_down2):
        if (cached_key == LIVES_KEY_Down) cached_key = 0;
        return FALSE;
      }
    } else {
      if (cached_key == unicode) cached_key = 0;
    }
  }

  // translate hardware code into gdk keyval, and call any accelerators
  if (down && (keymod & NEEDS_TRANSLATION)) {
    switch (unicode) {
    // some keys need translating when a modifier is held down
    case (65) :
      unicode = LIVES_KEY_Space;
      break;
    case (22) :
      unicode = LIVES_KEY_BackSpace;
      break;
    case (36) :
      unicode = LIVES_KEY_Return;
      break;
    case (24) :
      unicode = LIVES_KEY_q;
      break;
    case (10) :
      unicode = LIVES_KEY_1;
      break;
    case (11) :
      unicode = LIVES_KEY_2;
      break;
    case (12) :
      unicode = LIVES_KEY_3;
      break;
    case (13) :
      unicode = LIVES_KEY_4;
      break;
    case (14) :
      unicode = LIVES_KEY_5;
      break;
    case (15) :
      unicode = LIVES_KEY_6;
      break;
    case (16) :
      unicode = LIVES_KEY_7;
      break;
    case (17) :
      unicode = LIVES_KEY_8;
      break;
    case (18) :
      unicode = LIVES_KEY_9;
      break;
    case (19) :
      unicode = LIVES_KEY_0;
      break;
    case (67) :
      unicode = LIVES_KEY_F1;
      break;
    case (68) :
      unicode = LIVES_KEY_F2;
      break;
    case (69) :
      unicode = LIVES_KEY_F3;
      break;
    case (70) :
      unicode = LIVES_KEY_F4;
      break;
    case (71) :
      unicode = LIVES_KEY_F5;
      break;
    case (72) :
      unicode = LIVES_KEY_F6;
      break;
    case (73) :
      unicode = LIVES_KEY_F7;
      break;
    case (74) :
      unicode = LIVES_KEY_F8;
      break;
    case (75) :
      unicode = LIVES_KEY_F9;
      break;
    case (76) :
      unicode = LIVES_KEY_F10;
      break;
    case (95) :
      unicode = LIVES_KEY_F11;
      break;
    case (96) :
      unicode = LIVES_KEY_F12;
      break;
    case (99) :
    case (112) :
      unicode = LIVES_KEY_Page_Up;
      break;
    case (105) :
    case (117) :
      unicode = LIVES_KEY_Page_Down;
      break;

    // auto repeat keys
    case (key_left) :
    case (key_left2):
      unicode = LIVES_KEY_Left;
      break;
    case (key_right) :
    case (key_right2):
      unicode = LIVES_KEY_Right;
      break;
    case (key_up)  :
    case (key_up2):
      unicode = LIVES_KEY_Up;
      break;
    case (key_down) :
    case (key_down2):
      unicode = LIVES_KEY_Down;
      break;
    }
  }

  if (down && (unicode == LIVES_KEY_Left || unicode == LIVES_KEY_Right
               || unicode == LIVES_KEY_Up || unicode == LIVES_KEY_Down) &&
      (keymod & LIVES_CONTROL_MASK)) {
    cached_key = unicode;
    cached_mod = LIVES_CONTROL_MASK;
    if (keymod & LIVES_ALT_MASK) {
      cached_mod |= LIVES_ALT_MASK;
    }
    if (keymod & LIVES_SHIFT_MASK) {
      cached_mod |= LIVES_SHIFT_MASK;
    }
  }

  if (mainw->rte_textparm != NULL && (keymod == 0 || keymod == LIVES_SHIFT_MASK
                                      || keymod == LIVES_LOCK_MASK || keymod == 16)) {
    if (unicode == LIVES_KEY_Return || unicode == 13) unicode = '\n'; // CR
    if (unicode == LIVES_KEY_BackSpace) unicode = 8; // bs
    if (unicode == LIVES_KEY_Tab || unicode == 9) mainw->rte_textparm = NULL;
    else if (unicode > 0 && unicode < 256) {
      if (down) {
        weed_plant_t *inst;
        int param_number;
        int error;
        char *nval;
        char *cval = weed_get_string_value(mainw->rte_textparm, WEED_LEAF_VALUE, &error);
        if (unicode == 8 && strlen(cval) > 0) {
          cval[strlen(cval) - 1] =  0; // delete 1 char
          nval = lives_strdup(cval);
        } else nval = lives_strdup_printf("%s%c", cval, (unsigned char)unicode); // append 1 char
        lives_free(cval);
        weed_set_string_value(mainw->rte_textparm, WEED_LEAF_VALUE, nval);
        inst = weed_get_plantptr_value(mainw->rte_textparm, WEED_LEAF_HOST_INSTANCE, &error);
        param_number = weed_get_int_value(mainw->rte_textparm, WEED_LEAF_HOST_IDX, &error);
        //copyto = set_copy_to(inst, param_number, TRUE);
        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, param_number);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        lives_free(nval);
      }
      return TRUE;
    }
  }

  return FALSE;
}


// key callback functions - ones which have keys and need wrappers

boolean slower_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                        livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  if (mod & LIVES_SHIFT_MASK) on_slower_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND));
  else on_slower_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  return TRUE;
}


boolean less_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                      livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  on_less_pressed(NULL, user_data);
  return TRUE;
}


boolean faster_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                        livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  if (mod & LIVES_SHIFT_MASK) on_faster_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND));
  else on_faster_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  return TRUE;
}


boolean more_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                      livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  on_more_pressed(NULL, user_data);
  return TRUE;
}


boolean skip_back_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  on_back_pressed(NULL, user_data);
  return TRUE;
}


boolean skip_forward_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                              livespointer user_data) {
  // special flagbit we add, we want to generate these events from the player not from a real key
  if (!(mod & LIVES_SPECIAL_MASK)) return TRUE;
  on_forward_pressed(NULL, user_data);
  return TRUE;
}


boolean stop_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                      livespointer user_data) {
  on_stop_activate(NULL, NULL);
  return TRUE;
}


boolean fullscreen_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                            livespointer user_data) {
  on_full_screen_pressed(NULL, NULL);
  return TRUE;
}


boolean sepwin_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                        livespointer user_data) {
  on_sepwin_pressed(NULL, NULL);
  return TRUE;
}


boolean loop_cont_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer user_data) {
  on_loop_button_activate(NULL, NULL);
  return TRUE;
}


boolean ping_pong_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer user_data) {
  on_ping_pong_activate(NULL, NULL);
  return TRUE;
}


boolean fade_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                      livespointer user_data) {
  on_fade_pressed(NULL, NULL);
  return TRUE;
}


boolean showfct_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                         livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->showfct), prefs->hide_framebar);
  return TRUE;
}


boolean showsubs_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                          livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->showsubs), !prefs->show_subtitles);
  return TRUE;
}


boolean loop_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                      livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video), !mainw->loop);
  return TRUE;
}


boolean dblsize_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                         livespointer user_data) {
  on_double_size_pressed(NULL, NULL);
  return TRUE;
}


boolean rec_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                     livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->record_perf),
                                   !lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->record_perf)));
  return TRUE;
}

