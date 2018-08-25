// omc-learn.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2018
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_OMC_LEARN_H
#define HAS_LIVES_OMC_LEARN_H

/** max number of macros */
#define N_OMC_MACROS 64

/** floating point precision */
#define OMC_FP_FIX 4

// OMC device interfaces
#if HAVE_LINUX_JOYSTICK_H
#define OMC_JS_IMPL
#endif

#define OMC_MIDI_IMPL

#ifdef OMC_JS_IMPL
char *js_mangle(void);
boolean js_open(void);
void js_close(void);
const char *get_js_filename(void);
#endif

#ifdef OMC_MIDI_IMPL
char *midi_mangle(void);
const char *get_midi_filename(void);
boolean midi_open(void);
void midi_close(void);
#endif

/* macros */
#define UNMATCHED -1
#define START_PLAYBACK 0
#define STOP_PLAYBACK 1
#define CLIP_SELECT 2
#define PLAY_FORWARDS 3
#define PLAY_BACKWARDS 4
#define REVERSE_PLAYBACK 5
#define PLAY_FASTER 6
#define PLAY_SLOWER 7
#define TOGGLE_FREEZE 8
#define SET_FRAMERATE 9
#define START_RECORDING 10
#define STOP_RECORDING 11
#define TOGGLE_RECORDING 12
#define SWAP_FOREGROUND_BACKGROUND 13
#define RESET_EFFECT_KEYS 14
#define ENABLE_EFFECT_KEY 15
#define DISABLE_EFFECT_KEY 16
#define TOGGLE_EFFECT_KEY 17
#define SET_PARAMETER_VALUE 18
#define NEXT_CLIP_SELECT 19
#define PREV_CLIP_SELECT 20
#define SET_FPS_RATIO 21
#define RETRIGGER_CLIP 22
#define NEXT_MODE_CYCLE 23
#define PREV_MODE_CYCLE 24
#define SET_VPP_PARAMETER_VALUE 25
#define OSC_NOTIFY 26

/* parameter types */
#define OMC_PARAM_INT 1
#define OMC_PARAM_DOUBLE 2
#define OMC_PARAM_SPECIAL 128 ///< can be int or double, depending on effect type

typedef struct {
  char *msg;   ///< OSC message
  char *macro_text;  ///< macro text
  char *info_text;  ///< descriptive text
  char *stype_tags;   ///< setter type tags

  int nparams;

  char **pname;

  int *ptypes;

  int *mini;
  int *maxi;
  int *vali;

  double *mind;
  double *maxd;
  double *vald;
} lives_omc_macro_t;

typedef struct {
  char *srch; ///< string to match
  int macro; ///< macro number this is linked to (or -1)

  int nvars; ///< number of params
  int *offs0; ///< offs to add to params before scale
  double *scale; ///< scale for params
  int *offs1; ///< offs to add to params after scale

  int *min; ///< min values of params
  int *max; ///< max values of params

  boolean *matchp; ///< do we need to match this pval ?
  int *matchi; ///< match value

  // enumerated by number of params in target macro
  int *map; ///< reverse mapping to params of OSC message
  int *fvali; ///< mapping to fixed ints
  double *fvald; ///< mapping to fixed doubles

  ///////////////////////// following this is not saved/loaded

  LiVESWidget *treev1;
  LiVESWidget *treev2;

  LiVESTreeStore *gtkstore;
  LiVESTreeStore *gtkstore2;

  int *tmpvals;
} lives_omc_match_node_t;

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *clear_button;
  LiVESWidget *del_all_button;

  int tbl_rows;
  int tbl_currow;
  LiVESWidget *table;
  LiVESWidget *top_vbox;
} omclearn_w;

enum {
  TITLE_COLUMN,
  VALUE_COLUMN,
  FILTER_COLUMN,
  RANGE_COLUMN,
  OFFS1_COLUMN,
  SCALE_COLUMN,
  OFFS2_COLUMN,
  NUM_COLUMNS
};

enum {
  TITLE2_COLUMN,
  VALUE2_COLUMN,
  ADJUSTMENT,
  NUM2_COLUMNS
};

#define OMC_JS 1
#define OMC_JS_AXIS 2
#define OMC_JS_BUTTON 3

#define OMC_MIDI 128
#define OMC_MIDI_NOTE 129
#define OMC_MIDI_NOTE_OFF 130
#define OMC_MIDI_CONTROLLER 131
#define OMC_MIDI_PITCH_BEND 132
#define OMC_MIDI_PGM_CHANGE 133

/// start learning MIDI inputs
void on_midi_learn_activate(LiVESMenuItem *, livespointer);

/// process a string (i.e. convert to an OSC message and pass to OSC subsys)
/// only need to set omclw if learn is TRUE
boolean omc_process_string(int supertype, const char *string, boolean learn, omclearn_w *omclw);

#define OMC_FILE_VSTRING "LiVES OMC map version 1.1"
#define OMC_FILE_VSTRING_1_0 "LiVES OMC map version 1.0"

#define LIVES_DEVICEMAPS_DIR "devicemaps"

boolean has_devicemap(int has_this_macro);
void create_devicemap_directory(void);
void on_devicemap_save_activate(LiVESMenuItem *, livespointer);
void on_devicemap_load_activate(LiVESMenuItem *, livespointer);

#include "osc.h"

#define OSC_BUF_SIZE 1024
#define OSC_MAX_TYPETAGS 64

/// decode learnt behaviours
OSCbuf *omc_learner_decode(int type, int index, const char *string);

#endif // HAS_LIVES_OMC_LEARN_H
