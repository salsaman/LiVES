// keyboard.h
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@gmail.com>
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


boolean ext_triggers_poll(gpointer); ///< poll for external playback start

GdkFilterReturn filter_func(GdkXEvent *xevent, GdkEvent *event, gpointer data);

boolean plugin_poll_keyboard (void);

boolean pl_key_function (boolean down, uint16_t unicode, uint16_t keymod);

boolean faster_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean slower_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean skip_back_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean skip_forward_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean stop_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean rec_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean loop_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean loop_cont_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean ping_pong_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean dblsize_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean showfct_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean showsubs_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean fullscreen_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean sepwin_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);

boolean fade_callback (GtkAccelGroup *, GObject *, uint32_t, GdkModifierType, gpointer user_data);


#define KEY_RPT_INTERVAL 4


/** default MIDI checks per keyboard cycle (i.e. normally x checks per 4 ms - raw MIDI only) */
/* can be over-ridden in prefs */
#define DEF_MIDI_CHECK_RATE 1000


/** allowed non-reads between reads (raw MIDI only) */
#define DEF_MIDI_RPT 1000
