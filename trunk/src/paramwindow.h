// paramwindow.h
// LiVES
// (c) G. Finch 2004 - 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PARAMWINDOW_H
#define HAS_LIVES_PARAMWINDOW_H

typedef struct {
  gint usr_number;
  GSList *rbgroup;
  gint active_param;
} lives_widget_group_t;


#define RFX_TEXT_MAGIC 80
#define RFX_DEF_NUM_MAX 1000000.

#define RFX_WINSIZE_H 600
#define RFX_WINSIZE_V 400

#define DEF_BUTTON_WIDTH 80

void on_paramwindow_ok_clicked (GtkButton *, lives_rfx_t *);
void on_paramwindow_cancel_clicked (GtkButton *, lives_rfx_t *);
void on_paramwindow_cancel_clicked2 (GtkButton *, lives_rfx_t *);

void on_render_fx_pre_activate (GtkMenuItem *, lives_rfx_t *);
void on_render_fx_activate (GtkMenuItem *, lives_rfx_t *);

gboolean make_param_box(GtkVBox *, lives_rfx_t *);

gboolean add_param_to_box (GtkBox *, lives_rfx_t *, gint param_number, gboolean add_slider);
void add_hsep_to_box (GtkBox *);
void add_fill_to_box (GtkBox *);
void add_label_to_box (GtkBox *box, gboolean do_trans, const gchar *text);

GSList *add_usrgrp_to_livesgrp (GSList *u2l, GSList *rbgroup, gint usr_number);
lives_widget_group_t *livesgrp_from_usrgrp (GSList *u2l, gint usrgrp);

void after_boolean_param_toggled (GtkToggleButton *, lives_rfx_t * rfx);
void after_param_value_changed (GtkSpinButton *, lives_rfx_t * rfx);
void after_param_red_changed (GtkSpinButton *, lives_rfx_t * rfx);
void after_param_green_changed (GtkSpinButton *, lives_rfx_t * rfx);
void after_param_blue_changed (GtkSpinButton *, lives_rfx_t * rfx);
void after_param_alpha_changed (GtkSpinButton *, lives_rfx_t * rfx);
gboolean after_param_text_focus_changed (GtkWidget *, GtkWidget *, lives_rfx_t *rfx);
void after_param_text_changed (GtkWidget *, lives_rfx_t *rfx);
void after_string_list_changed (GtkComboBox *, lives_rfx_t *rfx);

void on_pwcolsel (GtkButton *button, lives_rfx_t *);

gchar *param_marshall (lives_rfx_t *rfx, gboolean with_min_max);
gchar **param_marshall_to_argv (lives_rfx_t *rfx);
void param_demarshall (lives_rfx_t *rfx, GList *plist, gboolean with_min_max, gboolean update_widgets);
gint set_param_from_list(GList *plist, lives_param_t *param, gint pnum, gboolean with_min_max, gboolean upd);
GList *argv_to_marshalled_list (lives_rfx_t *rfx, gint argc, gchar **argv);

/// object should have g_set_object_data "param_number" set to parameter number
///
/// (0 based, -ve for init onchanges)
void do_onchange (GObject *object, lives_rfx_t *);
void do_onchange_init(lives_rfx_t *rfx);



void update_weed_color_value(weed_plant_t *inst, int pnum, int c1, int c2, int c3, int c4);

void update_visual_params(lives_rfx_t *rfx, gboolean update_hidden);



#endif
