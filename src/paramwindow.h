// paramwindow.h
// LiVES
// (c) G. Finch 2004 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PARAMWINDOW_H
#define HAS_LIVES_PARAMWINDOW_H

enum {
  LIVES_PARAM_WIDGET_UNKNOWN,
  LIVES_PARAM_WIDGET_LABEL,
  LIVES_PARAM_WIDGET_SPINBUTTON,
  LIVES_PARAM_WIDGET_SLIDER,
  LIVES_PARAM_WIDGET_KNOB
};

typedef struct {
  int usr_number;
  LiVESSList *rbgroup;
  int active_param;
} lives_widget_group_t;

#define RFX_TEXT_MAGIC 80 ///< length at which entry turns into textview
#define RFX_DEF_NUM_MAX 1000000. ///< default param max when not defined

#define MAX_FMT_STRINGS 256
#define FMT_STRING_SIZE 256

#define RFX_TEXT_SCROLL_HEIGHT ((int)(80.*widget_opts.scaleH)) ///< height of textview scrolled window

#define GIW_KNOB_WIDTH ((int)(40.*widget_opts.scaleW))
#define GIW_KNOB_HEIGHT ((int)(40.*widget_opts.scaleH))

#define DEF_SLIDER_WIDTH ((int)(200.*widget_opts.scaleW))

#define NOSLID_RANGE_LIM 1000000.
#define NOSLID_VALUE_LIM 100.

#if WEED_FILTER_API_VERSION < 201
typedef weed_error_t (*weed_display_value_f)(weed_plant_t *filter_instance, weed_plant_t *parameter,
    boolean inverse);
#define WEED_LEAF_DISPLAY_VALUE_FUNC "display_value_func"
#define WEED_LEAF_DISPLAY_VALUE "display_value"
#endif

#define LINKED_RFX_KEY "linked_rfx"
#define EXTRA_WIDGET_KEY "extra_widget"

int get_param_widget_by_type(lives_param_t *, int wtype);

void on_paramwindow_button_clicked(LiVESButton *, lives_rfx_t *);
void on_paramwindow_button_clicked2(LiVESButton *, lives_rfx_t *);

void on_render_fx_pre_activate(LiVESMenuItem *, livespointer unused);
void on_render_fx_activate(LiVESMenuItem *, livespointer do_onch);

_fx_dialog *on_fx_pre_activate(lives_rfx_t *, boolean is_realtime, LiVESWidget *pbox);

boolean make_param_box(LiVESVBox *, lives_rfx_t *);

boolean add_param_to_box(LiVESBox *, lives_rfx_t *, int param_number, boolean add_slider);
LiVESWidget *add_param_label_to_box(LiVESBox *, boolean do_trans, const char *text);

LiVESSList *add_usrgrp_to_livesgrp(LiVESSList *u2l, LiVESSList *rbgroup, int usr_number);
lives_widget_group_t *livesgrp_from_usrgrp(LiVESSList *u2l, int usrgrp);

boolean set_dispval_from_value(LiVESAdjustment *, lives_param_t *, boolean config);
boolean set_value_from_dispval(LiVESAdjustment *, lives_param_t *);

void after_boolean_param_toggled(LiVESToggleButton *, lives_rfx_t *);
void after_param_value_changed(LiVESSpinButton *, lives_rfx_t *);
void after_param_red_changed(LiVESSpinButton *, lives_rfx_t *);
void after_param_green_changed(LiVESSpinButton *, lives_rfx_t *);
void after_param_blue_changed(LiVESSpinButton *, lives_rfx_t *);
void after_param_alpha_changed(LiVESSpinButton *, lives_rfx_t *);
boolean after_param_text_focus_changed(LiVESWidget *, LiVESWidget *, lives_rfx_t *);
void after_param_text_changed(LiVESWidget *, lives_rfx_t *);
void after_string_list_changed(LiVESWidget *, lives_rfx_t *);

void on_pwcolsel(LiVESButton *, lives_rfx_t *);

boolean lives_param_update_gui_num(lives_param_t *, double val);
boolean lives_param_update_gui_double(lives_param_t *, double val);
boolean lives_param_update_gui_int(lives_param_t *, int val);

char *param_marshall(lives_rfx_t *, boolean with_min_max);
char **param_marshall_to_argv(lives_rfx_t *);
void param_demarshall(lives_rfx_t *, LiVESList *plist, boolean with_min_max, boolean update_widgets);
LiVESList *set_param_from_list(LiVESList *plist, lives_param_t *param, boolean with_min_max,
                               boolean upd);
LiVESList *argv_to_marshalled_list(lives_rfx_t *, int argc, char **argv);

boolean _make_param_box(LiVESVBox *top_vbox, lives_rfx_t *s);

/// object should have g_set_object_data "param_number" set to parameter number
///
/// (0 based, -ve for init onchanges)
LiVESList *do_onchange(LiVESWidgetObject *object, lives_rfx_t *) WARN_UNUSED;
LiVESList *do_onchange_init(lives_rfx_t *) WARN_UNUSED;

void update_weed_color_value(weed_plant_t *plant, int pnum, int c1, int c2, int c3, int c4, lives_rfx_t *);

/// apply internal value changes to interface widgets
void update_visual_params(lives_rfx_t *, boolean update_hidden);

/// show / hide widgets set by plugin in init_func()
boolean update_widget_vis(lives_rfx_t *, int key, int mode);

#define LIVES_LEAF_DISPVAL "host_disp_val"

#endif
