// omc-learn.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef HAS_LIVES_OMC_LEARN_H
#define HAS_LIVES_OMC_LEARN_H

/** max number of macros */
#define N_OMC_MACROS 32

/** floating point precision */
#define OMC_FP_FIX 4

// OMC device interfaces
#if HAVE_LINUX_JOYSTICK_H
#define OMC_JS_IMPL
#endif

#define OMC_MIDI_IMPL


#ifdef OMC_JS_IMPL
gchar *js_mangle(void);
boolean js_open(void);
void js_close(void);
const gchar *get_js_filename(void);
#endif

#ifdef OMC_MIDI_IMPL
gchar *midi_mangle(void);
const gchar *get_midi_filename(void);
boolean midi_open(void);
void midi_close(void);
#endif

/* parameter types */
#define OMC_PARAM_INT 1
#define OMC_PARAM_DOUBLE 2
#define OMC_PARAM_SPECIAL 128 ///< can be int or double, depending on effect type

typedef struct {
  gchar *msg;   ///< OSC message
  gchar *macro_text;  ///< macro text
  gchar *info_text;  ///< descriptive text
  gchar *stype_tags;   ///< setter type tags

  gint nparams;

  gchar **pname;

  gint *ptypes;

  gint *mini;
  gint *maxi;
  gint *vali;

  gdouble *mind;
  gdouble *maxd;
  gdouble *vald;


} lives_omc_macro_t;




typedef struct {
  gchar *srch; ///< string to match
  gint macro; ///< macro number this is linked to (or -1)

  gint nvars; ///< number of params
  gint *offs0; ///< offs to add to params before scale
  gdouble *scale; ///< scale for params
  gint *offs1; ///< offs to add to params after scale

  gint *min; ///< min values of params
  gint *max; ///< max values of params

  boolean *matchp; ///< do we need to match this pval ?
  gint *matchi; ///< match value

  // enumerated by number of params in target macro
  gint *map; ///< reverse mapping to params of OSC message
  gint *fvali; ///< mapping to fixed ints
  gdouble *fvald; ///< mapping to fixed doubles


  ///////////////////////// following this is not saved/loaded

  GtkWidget *treev1;
  GtkWidget *treev2;

  GtkTreeStore *gtkstore;
  GtkTreeStore *gtkstore2;

  int *tmpvals;

} lives_omc_match_node_t;



typedef struct {
  GtkWidget *dialog;
  GtkWidget *clear_button;
  GtkWidget *del_all_button;

  gint tbl_rows;
  gint tbl_currow;
  GtkWidget *table;
  GtkWidget *top_vbox;
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
void on_midi_learn_activate (GtkMenuItem *, gpointer);

/// process a string (i.e. convert to an OSC message and pass to OSC subsys)
/// only need to set omclw if learn is TRUE
boolean omc_process_string(gint supertype, const gchar *string, boolean learn, omclearn_w *omclw);



#define OMC_FILE_VSTRING "LiVES OMC map version 1.0"

void on_midi_save_activate (GtkMenuItem *, gpointer);
void on_midi_load_activate (GtkMenuItem *, gpointer);


#include "osc.h"


#define OSC_BUF_SIZE 1024
#define OSC_MAX_TYPETAGS 64 

/// decode learnt behaviours
OSCbuf *omc_learner_decode(gint type, gint index, const gchar *string);


#endif // HAS_LIVES_OMC_LEARN_H



