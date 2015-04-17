// keyboard.h
// LiVES
// (c) G. Finch 2004 - 2015 <salsaman@gmail.com>
// see file ../COPYING for licensing details

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


boolean ext_triggers_poll(livespointer); ///< poll for external playback start

#if defined HAVE_X11 || defined IS_MINGW
LiVESFilterReturn filter_func(LiVESXXEvent *xevent, LiVESXEvent *event, livespointer data);
#endif

boolean plugin_poll_keyboard(void);

boolean pl_key_function(boolean down, uint16_t unicode, uint16_t keymod);

boolean faster_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean slower_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean skip_back_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean skip_forward_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean stop_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean rec_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean loop_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean loop_cont_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean ping_pong_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean dblsize_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean showfct_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean showsubs_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean fullscreen_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean sepwin_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);

boolean fade_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);


#define KEY_RPT_INTERVAL 4


/** default MIDI checks per keyboard cycle (i.e. normally x checks per 4 ms - raw MIDI only) */
/* can be over-ridden in prefs */
#define DEF_MIDI_CHECK_RATE 1000


/** allowed non-reads between reads (raw MIDI only) */
#define DEF_MIDI_RPT 1000
