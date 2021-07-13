// omc-learn.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2018
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_OMC_LEARN_H
#define HAS_LIVES_OMC_LEARN_H

#define OMC_DEV_MIDI 		(1 << 0)
#define OMC_DEV_JS		(1 << 1)
#define OMC_DEV_FORCE_RAW_MIDI	(1 << 2)
#define OMC_DEV_MIDI_DUMMY	(1 << 3)

/** max number of macros */
#define N_OMC_MACROS 64

/** floating point precision */
#define OMC_FP_FIX 4

// OMC device interfaces
#if HAVE_LINUX_JOYSTICK_H
#define OMC_JS_IMPL
#endif

#define OMC_MIDI_IMPL

#define MIDI_OMNI -1

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
  int *mini, *maxi, *vali;

  double *mind, *maxd, *vald;
} lives_omc_macro_t;

typedef struct {
  char *srch; ///< string to match
  int macro; ///< action number this is linked to (or -1) (see mainwindow.h)

  int nvars; ///< number of input params
  int *offs0; ///< offs to add to params before scale (pre-bias)
  double *scale; ///< scale for params (unbiased scale)
  int *offs1; ///< offs to add to params after scale (post bias)

  int *min; ///< min values of input params
  int *max; ///< max values of input params

  boolean *matchp; ///< do we additionally need to match this param val in the input ?
  int *matchi; ///< match value

  // enumerated by number of params in target macro
  int *map; ///< mapping macro parameters to variables in the input (whether we use a default or a variable)
  // (reverse ordered, starting from the last macro parameter)
  int *fvali; ///< defaults, mapping to fixed ints
  double *fvald; ///< defaults, mapping to fixed doubles

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

  LiVESResponseType bresp;
  boolean ready;
} omclearn_w;

enum {
  TITLE_COLUMN,
  VALUE_COLUMN,
  FILTER_COLUMN,
  RANGE_COLUMN,
  OFFS1_COLUMN,
  SCALE_COLUMN,
  OFFS2_COLUMN,
  OMC_NUM_COLUMNS
};

enum {
  TITLE2_COLUMN,
  VALUE2_COLUMN,
  ADJUSTMENT,
  OMC_NUM2_COLUMNS
};

#define OMC_INTERNAL	0

#define OMC_JS 		1
#define OMC_JS_AXIS	2
#define OMC_JS_BUTTON	3

#define OMC_MIDI		128
#define OMC_MIDI_NOTE		129
#define OMC_MIDI_NOTE_OFF	130
#define OMC_MIDI_CONTROLLER	131
#define OMC_MIDI_PITCH_BEND	132
#define OMC_MIDI_PGM_CHANGE	133

// get a macro
const lives_omc_macro_t *get_omc_macro(int idx);

/// start learning MIDI inputs
void on_midi_learn_activate(LiVESMenuItem *, livespointer);

/// process a string (i.e. convert to an OSC message and pass to OSC subsys)
/// only need to set omclw if learn is TRUE
boolean omc_process_string(int supertype, const char *string, boolean learn, omclearn_w *omclw);

#define OMC_FILE_VSTRING "LiVES OMC map version 1.1"
#define OMC_FILE_VSTRING_1_0 "LiVES OMC map version 1.0"

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
