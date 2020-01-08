// keyboard.h
// LiVES
// (c) G. Finch 2004 - 2016 <salsaman+lives@gmail.com>
// see file ../COPYING for licensing details

#define LIVES_XEVENT_TYPE_KEYPRESS 2
#define LIVES_XEVENT_TYPE_KEYRELEASE 3

// repeating keys
uint16_t cached_key;
uint16_t cached_mod;

// these keys should be cached on a key down and sent every time until a key up
#define key_left 100
#define key_left2 113
#define key_right 102
#define key_right2 114
#define key_up 98
#define key_up2 111
#define key_down 104
#define key_down2 116

boolean key_press_or_release(LiVESWidget *, LiVESXEventKey *, livespointer); ///< wrapper for pl_key_function

boolean ext_triggers_poll(livespointer); ///< poll for external playback start

/* #if defined HAVE_X11 */
/* LiVESFilterReturn filter_func(LiVESXXEvent *xevent, LiVESXEvent *event, livespointer data); ///< unused ? */
/* #endif */

void handle_cached_keys(void); ///< smooth the key repeat for scratching

boolean pl_key_function(boolean down, uint16_t unicode, uint16_t keymod); ///< all funky stuff with keys

//////////////////////// callbacks ////////////////////////////////////////////

boolean faster_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean slower_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean more_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean less_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean skip_back_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean skip_forward_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean stop_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean rec_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean loop_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean loop_cont_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean ping_pong_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean dblsize_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean showfct_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean showsubs_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean fullscreen_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean sepwin_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean fade_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer user_data);

#define EXT_TRIGGER_INTERVAL 4 // polling time for osc / midi / joystick etc. (milliseconds)

#define KEY_RPT_INTERVAL 40  // repeat rate for cached keys (ctrl-left, ctrl-right, ctrl-up, ctrl-down) (milliseconds)

/** default MIDI checks per keyboard cycle (i.e. normally x checks per 4 ms - raw MIDI only) */
/* can be over-ridden in prefs */
#define DEF_MIDI_CHECK_RATE 1000

/** allowed non-reads between reads (raw MIDI only) */
#define DEF_MIDI_RPT 1000
