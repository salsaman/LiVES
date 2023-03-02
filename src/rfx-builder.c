// rfx-builder.c
// LiVES
// (c) G. Finch 2004 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <errno.h>

#include "main.h"
#include "rfx-builder.h"
#include "interface.h"
#include "paramwindow.h"
#include "effects.h"
#include "startup.h"

static LiVESWidget *make_param_dialog(int pnum, rfx_build_window_t *rfxbuilder);
static LiVESWidget *make_param_window_dialog(int hnum, rfx_build_window_t *rfxbuilder);
static LiVESWidget *make_trigger_dialog(int tnum, rfx_build_window_t *rfxbuilder);
static void on_table_add_row(LiVESButton *, livespointer rfxbuilder);
static void on_table_edit_row(LiVESButton *, livespointer rfxbuilder);
static void on_table_swap_row(LiVESButton *, livespointer rfxbuilder);
static void on_table_delete_row(LiVESButton *, livespointer rfxbuilder);

static void param_set_from_dialog(lives_param_t *copy_param, rfx_build_window_t *rfxbuilder);

static void after_param_dp_changed(LiVESSpinButton *, livespointer rfxbuilder);
static void after_param_min_changed(LiVESSpinButton *, livespointer rfxbuilder);
static void after_param_max_changed(LiVESSpinButton *, livespointer rfxbuilder);
static void after_param_def_changed(LiVESSpinButton *, livespointer rfxbuilder);
static void after_rfxbuilder_type_toggled(LiVESToggleButton *, livespointer rfxbuilder);
static void on_param_type_changed(LiVESCombo *, livespointer rfxbuilder);
static void on_paramw_kw_changed(LiVESCombo *, livespointer rfxbuilder);
static void on_paramw_sp_changed(LiVESCombo *, livespointer rfxbuilder);
static void on_paramw_spsub_changed(LiVESCombo *, livespointer rfxbuilder);
static void populate_script_combo(LiVESCombo *script_combo, lives_rfx_status_t status);
static void on_script_status_changed(LiVESCombo *status_combo, livespointer script_combo);

// builder window widgets/callbacks
static rfx_build_window_t *make_rfx_build_window(const char *in_script_name, lives_rfx_status_t in_status);
static void on_rfxbuilder_ok(LiVESButton *, livespointer);
static void on_rfxbuilder_cancel(LiVESButton *, livespointer);
static void rfxbuilder_destroy(rfx_build_window_t *);
static void on_list_table_clicked(LiVESButton *, livespointer rfxbuilder);
static void on_requirements_ok(LiVESButton *, livespointer rfxbuilder);
static void on_requirements_cancel(LiVESButton *, livespointer);
static void on_properties_clicked(LiVESButton *, livespointer rfxbuilder);
static void on_properties_ok(LiVESButton *, livespointer rfxbuilder);
static void on_params_ok(LiVESButton *, livespointer rfxbuilder);
static void on_params_cancel(LiVESButton *, livespointer);
static void on_param_window_ok(LiVESButton *, livespointer rfxbuilder);
static void on_param_window_cancel(LiVESButton *, livespointer);
static void on_code_clicked(LiVESButton *, livespointer rfxbuilder);
static void on_code_ok(LiVESButton *, livespointer rfxbuilder);
static void on_triggers_ok(LiVESButton *, livespointer rfxbuilder);
static void on_triggers_cancel(LiVESButton *, livespointer);

static LiVESWidget *copy_script_okbutton;

static void table_select_row(rfx_build_window_t *rfxbuilder, int row);

static int rfx_slot_count = 0;

void on_new_rfx_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  rfx_build_window_t *rfxbuilder;

  if (!check_builder_programs()) return;
  rfxbuilder = make_rfx_build_window(NULL, RFX_STATUS_ANY);
  lives_widget_show(rfxbuilder->dialog);
}


void on_edit_rfx_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  rfx_build_window_t *rfxbuilder;
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);
  char *script_name;

  if (!check_builder_programs()) return;
  if (status != RFX_STATUS_TEST) return; // we only edit test effects currently

  if ((script_name = prompt_for_script_name(NULL, RFX_STATUS_TEST)) == NULL) return;
  if (!strlen(script_name)) {
    lives_free(script_name);
    return;
  }
  if ((rfxbuilder = make_rfx_build_window(script_name, RFX_STATUS_TEST)) == NULL) {
    lives_free(script_name);
    return; // script not loaded
  }
  lives_free(script_name);
  rfxbuilder->mode = RFX_BUILDER_MODE_EDIT;
  rfxbuilder->oname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->name_entry)));
  lives_widget_show(rfxbuilder->dialog);
}


void on_copy_rfx_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);

  if (!check_builder_programs()) return;
  if (status != RFX_STATUS_TEST) return; // we only copy to test effects currently

  // prompt_for_script_name will create our builder window from the input
  // script and set the name : note, we have no rfxbuilder reference

  // TODO - use RFXBUILDER_MODE-*
  prompt_for_script_name(NULL, RFX_STATUS_COPY);
}


void on_rename_rfx_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);

  if (status != RFX_STATUS_TEST) return; // we only copy to test effects currently

  prompt_for_script_name(NULL, RFX_STATUS_RENAME);
}


rfx_build_window_t *make_rfx_build_window(const char *script_name, lives_rfx_status_t status) {
  // TODO - set mnemonic widgets for entries

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *layout;
  LiVESWidget *label;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton;
  LiVESWidget *scrollw;
  LiVESWidget *top_vbox;

  LiVESSList *radiobutton_type_group = NULL;
  LiVESList *langc = NULL;

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)lives_malloc(sizeof(rfx_build_window_t));

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  int winsize_h, winsize_v;

  char *tmp, *tmp2, *title, *string;

  rfxbuilder->rfx_version = lives_strdup(RFX_VERSION);

  rfxbuilder->type = RFX_BUILD_TYPE_EFFECT1; // the default

  rfxbuilder->num_reqs = 0;
  rfxbuilder->num_params = 0;
  rfxbuilder->num_paramw_hints = 0;

  rfxbuilder->num_triggers = 0;

  rfxbuilder->has_init_trigger = FALSE;
  rfxbuilder->props = 0;

  rfxbuilder->plugin_version = 1;

  rfxbuilder->pre_code = lives_strdup("");
  rfxbuilder->loop_code = lives_strdup("");
  rfxbuilder->post_code = lives_strdup("");

  rfxbuilder->field_delim = lives_strdup("|");

  rfxbuilder->script_name = NULL; // use default name.script
  rfxbuilder->oname = NULL;

  rfxbuilder->mode = RFX_BUILDER_MODE_NEW;

  rfxbuilder->table_swap_row1 = -1;

  /////////////////////////////////////////////////////////

  if (!script_name) {
    title = (_("New Test RFX"));
  } else {
    title = (_("Edit Test RFX"));
  }

  winsize_h = (PREF_RFXDIALOG_W  < GUI_SCREEN_WIDTH - SCR_WIDTH_SAFETY / 5.) ? PREF_RFXDIALOG_W : GUI_SCREEN_WIDTH -
              SCR_WIDTH_SAFETY / 5.;

  winsize_v = (PREF_RFXDIALOG_H < GUI_SCREEN_HEIGHT - SCR_HEIGHT_SAFETY / 2.) ? PREF_RFXDIALOG_H : GUI_SCREEN_HEIGHT -
              SCR_HEIGHT_SAFETY / 2.;

  rfxbuilder->dialog = lives_standard_dialog_new(title, FALSE, winsize_h, winsize_v);
  lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(rfxbuilder->dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(rfxbuilder->dialog));
  lives_container_set_border_width(LIVES_CONTAINER(lives_widget_get_parent(dialog_vbox)), widget_opts.border_width >> 1);

  top_vbox = lives_vbox_new(FALSE, 0);

  scrollw = lives_standard_scrolled_window_new(winsize_h, winsize_v, top_vbox);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrollw, TRUE, TRUE, 0);

  // types
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(top_vbox), hbox, TRUE, TRUE, 0);

  label = lives_standard_label_new(_("Type:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  string = lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, FALSE);
  rfxbuilder->type_effect1_radiobutton = lives_standard_radio_button_new(string, &radiobutton_type_group, LIVES_BOX(hbox), NULL);
  lives_free(string);

  string = lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, FALSE);
  rfxbuilder->type_effect2_radiobutton = lives_standard_radio_button_new(string, &radiobutton_type_group, LIVES_BOX(hbox), NULL);
  lives_free(string);

  string = lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_GENERATOR, FALSE);
  rfxbuilder->type_effect0_radiobutton = lives_standard_radio_button_new(string, &radiobutton_type_group, LIVES_BOX(hbox), NULL);
  lives_free(string);

  rfxbuilder->type_tool_radiobutton = lives_standard_radio_button_new(_("tool"), &radiobutton_type_group, LIVES_BOX(hbox), NULL);
  lives_widget_show(label);

  rfxbuilder->type_utility_radiobutton = lives_standard_radio_button_new(_("utility"), &radiobutton_type_group, LIVES_BOX(hbox),
                                         NULL);

  layout = lives_layout_new(LIVES_BOX(top_vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  // name

  rfxbuilder->name_entry = lives_standard_entry_new((tmp = (_("Name:          "))), NULL, -1, -1,
                           LIVES_BOX(hbox), (tmp2 = (_("The name of the plugin. No spaces allowed."))));
  lives_free(tmp); lives_free(tmp2);

  // version
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rfxbuilder->spinbutton_version = lives_standard_spin_button_new((tmp = (_("Version:       "))),
                                   rfxbuilder->plugin_version, rfxbuilder->plugin_version, 1000000., 1., 1., 0,
                                   LIVES_BOX(hbox), (tmp2 = (_("The script version."))));
  lives_free(tmp); lives_free(tmp2);

  // author
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rfxbuilder->author_entry = lives_standard_entry_new((tmp = (_("    Author:       "))), NULL, -1, -1,
                             LIVES_BOX(hbox), (tmp2 = (_("The script author."))));
  lives_free(tmp); lives_free(tmp2);

  // URL
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rfxbuilder->url_entry = lives_standard_entry_new((tmp = (_("    URL (optional):       "))), NULL, -1, -1,
                          LIVES_BOX(hbox), (tmp2 = (_("URL for the plugin maintainer."))));
  lives_free(tmp); lives_free(tmp2);

  // menu entry
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rfxbuilder->menu_text_entry = lives_standard_entry_new((tmp = (_("Menu text:    "))), NULL, -1, -1,
                                LIVES_BOX(hbox), (tmp2 = (_("The text to show in the menu."))));
  lives_free(tmp); lives_free(tmp2);

  rfxbuilder->action_desc_hsep = add_hsep_to_box(LIVES_BOX(top_vbox));

  // action description

  rfxbuilder->action_desc_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->action_desc_hbox, FALSE, FALSE, widget_opts.packing_height);

  rfxbuilder->action_desc_entry = lives_standard_entry_new((tmp = (_("Action description:        "))), NULL, -1, -1,
                                  LIVES_BOX(rfxbuilder->action_desc_hbox),
                                  (tmp2 = (_("Describe what the plugin is doing. E.g. \"Edge detecting\""))));
  lives_free(tmp); lives_free(tmp2);

  rfxbuilder->spinbutton_min_frames = lives_standard_spin_button_new((tmp = (_("Minimum frames:"))),
                                      1., 1., 1000., 1., 1., 0, LIVES_BOX(top_vbox),
                                      (tmp2 = (_("Minimum number of frames this effect/tool can be applied to. Normally 1."))));
  lives_free(tmp); lives_free(tmp2);
  rfxbuilder->min_frames_label = widget_opts.last_label;

  // requirements

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->requirements_button = lives_standard_button_new_with_label(_("_Requirements..."),
                                    DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->requirements_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->requirements_button,
                                (_("Enter any binaries required by the plugin.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->properties_button = lives_standard_button_new_with_label(_("_Properties..."),
                                  DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->properties_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->properties_button,
                                (_("Set properties for the plugin. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->params_button = lives_standard_button_new_with_label(_("_Parameters..."),
                              DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->params_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->params_button,
                                (_("Set up parameters used in pre/loop/post/trigger code. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->param_window_button = lives_standard_button_new_with_label(_("Parameter _Window Hints..."),
                                    DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->param_window_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->param_window_button,
                                (_("Set hints about how to lay out the parameter window. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  langc = lives_list_append(langc, (livespointer)"0xF0 - LiVES-Perl");

  rfxbuilder->langc_combo = lives_standard_combo_new((tmp = (_("_Language code:"))), langc, LIVES_BOX(top_vbox),
                            (tmp2 = (_("Language for pre/loop/post/triggers. Optional."))));

  lives_free(tmp); lives_free(tmp2);
  lives_list_free(langc);

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->pre_button = lives_standard_button_new_with_label(_("_Pre loop code..."),
                           DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->pre_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->pre_button,
                                (_("Code to be executed before the loop. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->loop_button = lives_standard_button_new_with_label(_("_Loop code..."),
                            DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->loop_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->loop_button,
                                (_("Loop code to be applied to each frame.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->post_button = lives_standard_button_new_with_label(_("_Post loop code..."),
                            DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->post_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->post_button,
                                (_("Code to be executed after the loop. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->trigger_button = lives_standard_button_new_with_label(_("_Trigger code..."),
                               DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(top_vbox), rfxbuilder->trigger_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(rfxbuilder->trigger_button,
                                (_("Set trigger code for when the parameter window is shown, or when a parameter is changed. "
                                   "Optional (except for Utilities).")));

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(rfxbuilder->dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(rfxbuilder->dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_rfxbuilder_ok), (livespointer)rfxbuilder);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_rfxbuilder_cancel), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->requirements_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_list_table_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->properties_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_properties_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->params_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_list_table_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->param_window_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_list_table_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->trigger_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_list_table_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->pre_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_code_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->loop_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_code_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(rfxbuilder->post_button, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_code_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->type_effect1_radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_rfxbuilder_type_toggled), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->type_effect2_radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_rfxbuilder_type_toggled), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->type_effect0_radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_rfxbuilder_type_toggled), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->type_tool_radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_rfxbuilder_type_toggled), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->type_utility_radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_rfxbuilder_type_toggled), (livespointer)rfxbuilder);

  // edit mode
  if (script_name) {
    // we assume the name is valid
    char *script_file;

    switch (status) {
    case RFX_STATUS_CUSTOM:
      script_file = lives_build_filename(prefs->config_datadir,
                                         PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, script_name, NULL);
      break;
    case RFX_STATUS_BUILTIN:
      script_file = lives_build_filename(prefs->prefix_dir, PLUGIN_SCRIPTS_DIR,
                                         PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS, script_name, NULL);
      break;
    default:
      script_file = lives_build_filename(prefs->config_datadir,
                                         PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, script_name, NULL);
      break;
    }
    if (!script_to_rfxbuilder(rfxbuilder, script_file)) {
      char *msg = lives_strdup_printf(_("\n\nUnable to parse the script file:\n%s\n%s\n"), script_file, mainw->msg);
      // must use blocking error dialogs as the scriptname window is modal
      do_error_dialog(msg);
      lives_free(msg);
      rfxbuilder_destroy(rfxbuilder);
      lives_free(script_file);
      return NULL;
    }
    lives_free(script_file);
    switch (rfxbuilder->type) {
    case RFX_BUILD_TYPE_TOOL:
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->type_tool_radiobutton), TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT0:
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->type_effect0_radiobutton), TRUE);
      break;
    case RFX_BUILD_TYPE_UTILITY:
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->type_utility_radiobutton), TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT2:
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->type_effect2_radiobutton), TRUE);
      break;
    default:
      break;
    }
  } else {
    lives_widget_grab_focus(rfxbuilder->name_entry);
  }

  lives_widget_show_all(rfxbuilder->dialog);

  return rfxbuilder;
}


void after_rfxbuilder_type_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  lives_widget_set_sensitive(rfxbuilder->pre_button, TRUE);
  lives_widget_set_sensitive(rfxbuilder->loop_button, TRUE);
  lives_widget_set_sensitive(rfxbuilder->post_button, TRUE);
  lives_widget_set_sensitive(rfxbuilder->requirements_button, TRUE);
  lives_widget_set_sensitive(rfxbuilder->properties_button, TRUE);
  lives_widget_show(rfxbuilder->spinbutton_min_frames);
  lives_widget_show(rfxbuilder->min_frames_label);
  lives_widget_show(rfxbuilder->action_desc_hbox);
  lives_widget_show(rfxbuilder->action_desc_hsep);

  if (togglebutton == LIVES_TOGGLE_BUTTON(rfxbuilder->type_effect1_radiobutton)) {
    rfxbuilder->type = RFX_BUILD_TYPE_EFFECT1;
  } else if (togglebutton == LIVES_TOGGLE_BUTTON(rfxbuilder->type_effect2_radiobutton)) {
    rfxbuilder->type = RFX_BUILD_TYPE_EFFECT2;
    lives_widget_set_sensitive(rfxbuilder->properties_button, FALSE);
  } else if (togglebutton == LIVES_TOGGLE_BUTTON(rfxbuilder->type_effect0_radiobutton)) {
    rfxbuilder->type = RFX_BUILD_TYPE_EFFECT0;
    lives_widget_hide(rfxbuilder->spinbutton_min_frames);
    lives_widget_hide(rfxbuilder->min_frames_label);
  } else if (togglebutton == LIVES_TOGGLE_BUTTON(rfxbuilder->type_tool_radiobutton)) {
    rfxbuilder->type = RFX_BUILD_TYPE_TOOL;
    lives_widget_hide(rfxbuilder->spinbutton_min_frames);
    lives_widget_set_sensitive(rfxbuilder->properties_button, FALSE);
    lives_widget_hide(rfxbuilder->min_frames_label);
  } else if (togglebutton == LIVES_TOGGLE_BUTTON(rfxbuilder->type_utility_radiobutton)) {
    rfxbuilder->type = RFX_BUILD_TYPE_UTILITY;
    lives_widget_hide(rfxbuilder->spinbutton_min_frames);
    lives_widget_set_sensitive(rfxbuilder->properties_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->requirements_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->pre_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->loop_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->post_button, FALSE);
    lives_widget_hide(rfxbuilder->min_frames_label);
    lives_widget_hide(rfxbuilder->action_desc_hbox);
    lives_widget_hide(rfxbuilder->action_desc_hsep);
  }
}


static void on_list_table_clicked(LiVESButton *button, livespointer user_data) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *button_box;

  // buttons
  LiVESWidget *okbutton;
  LiVESWidget *scrolledwindow;
  LiVESWidget *cancelbutton;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title = NULL;

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int i;

  if (button == LIVES_BUTTON(rfxbuilder->requirements_button)) {
    rfxbuilder->table_type = RFX_TABLE_TYPE_REQUIREMENTS;
  } else if (button == LIVES_BUTTON(rfxbuilder->params_button)) {
    rfxbuilder->table_type = RFX_TABLE_TYPE_PARAMS;
  } else if (button == LIVES_BUTTON(rfxbuilder->param_window_button)) {
    rfxbuilder->table_type = RFX_TABLE_TYPE_PARAM_WINDOW;
  } else if (button == LIVES_BUTTON(rfxbuilder->trigger_button)) {
    rfxbuilder->table_type = RFX_TABLE_TYPE_TRIGGERS;
  }

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_REQUIREMENTS) {
    title = (_("RFX Requirements"));
    rfxbuilder->onum_reqs = rfxbuilder->num_reqs;
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAMS) {
    title = (_("RFX Parameters"));
    rfxbuilder->onum_params = rfxbuilder->num_params;
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAM_WINDOW) {
    title = (_("RFX Parameter Window Hints"));
    rfxbuilder->onum_paramw_hints = rfxbuilder->num_paramw_hints;
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_TRIGGERS) {
    title = (_("RFX Triggers"));
    rfxbuilder->onum_triggers = rfxbuilder->num_triggers;
  }

  dialog = lives_standard_dialog_new(title, FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  lives_signal_handlers_disconnect_by_func(dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  if (title) lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // create table and add rows
  rfxbuilder->table_rows = 0;

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_REQUIREMENTS) {
    rfxbuilder->table = lives_table_new(rfxbuilder->num_reqs, 1, FALSE);
    for (i = 0; i < rfxbuilder->num_reqs; i++) {
      on_table_add_row(NULL, (livespointer)rfxbuilder);
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry[i]), rfxbuilder->reqs[i]);
    }
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAMS) {
    rfxbuilder->copy_params = (lives_param_t *)lives_malloc(RFXBUILD_MAX_PARAMS * sizeof(lives_param_t));
    rfxbuilder->table = lives_table_new(rfxbuilder->num_params, 3, FALSE);
    for (i = 0; i < rfxbuilder->num_params; i++) {
      param_copy(&rfxbuilder->copy_params[i], &rfxbuilder->params[i], FALSE);
      on_table_add_row(NULL, (livespointer)rfxbuilder);
    }
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->table = lives_table_new(rfxbuilder->table_rows, 2, FALSE);
    for (i = 0; i < rfxbuilder->num_paramw_hints; i++) {
      on_table_add_row(NULL, (livespointer)rfxbuilder);
    }
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_TRIGGERS) {
    rfxbuilder->copy_triggers = (rfx_trigger_t *)lives_malloc((RFXBUILD_MAX_PARAMS + 1) * sizeof(rfx_trigger_t));
    rfxbuilder->table = lives_table_new(rfxbuilder->table_rows, 1, FALSE);
    for (i = 0; i < rfxbuilder->num_triggers; i++) {
      rfxbuilder->copy_triggers[i].when = rfxbuilder->triggers[i].when;
      rfxbuilder->copy_triggers[i].code = lives_strdup(rfxbuilder->triggers[i].code);
      on_table_add_row(NULL, (livespointer)rfxbuilder);
    }
  }

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V / 4, rfxbuilder->table);

  lives_box_pack_start(LIVES_BOX(hbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_width);

  add_vsep_to_box(LIVES_BOX(hbox));

  // button box on right

  button_box = lives_vbutton_box_new();
  lives_button_box_set_layout(LIVES_BUTTON_BOX(button_box), LIVES_BUTTONBOX_START);

  lives_box_pack_start(LIVES_BOX(hbox), button_box, FALSE, FALSE, 0);

  rfxbuilder->new_entry_button = lives_standard_button_new_with_label(_("_New Entry"),
                                 DEF_BUTTON_WIDTH,
                                 DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(button_box), rfxbuilder->new_entry_button, FALSE, FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(rfxbuilder->new_entry_button), widget_opts.border_width);

  rfxbuilder->edit_entry_button = lives_standard_button_new_with_label(_("_Edit Entry"),
                                  DEF_BUTTON_WIDTH,
                                  DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(button_box), rfxbuilder->edit_entry_button, FALSE, FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(rfxbuilder->edit_entry_button), widget_opts.border_width);

  rfxbuilder->remove_entry_button = lives_standard_button_new_with_label(_("_Remove Entry"),
                                    DEF_BUTTON_WIDTH,
                                    DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(button_box), rfxbuilder->remove_entry_button, FALSE, FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(rfxbuilder->remove_entry_button), widget_opts.border_width);

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->move_up_button = lives_standard_button_new_with_label(_("Move _Up"),
                                 DEF_BUTTON_WIDTH,
                                 DEF_BUTTON_HEIGHT);

    lives_box_pack_start(LIVES_BOX(button_box), rfxbuilder->move_up_button, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(rfxbuilder->move_up_button), widget_opts.border_width);

    rfxbuilder->move_down_button = lives_standard_button_new_with_label(_("Move _Down"),
                                   DEF_BUTTON_WIDTH,
                                   DEF_BUTTON_HEIGHT);

    lives_box_pack_start(LIVES_BOX(button_box), rfxbuilder->move_down_button, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(rfxbuilder->move_down_button), widget_opts.border_width);

    lives_widget_set_sensitive(rfxbuilder->move_up_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->move_down_button, FALSE);
  } else {
    rfxbuilder->move_up_button = rfxbuilder->move_down_button = NULL;
  }

  lives_widget_set_sensitive(rfxbuilder->edit_entry_button, FALSE);
  lives_widget_set_sensitive(rfxbuilder->remove_entry_button, FALSE);

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_TRIGGERS) {
    if (rfxbuilder->num_triggers > rfxbuilder->num_params) {
      lives_widget_set_sensitive(rfxbuilder->new_entry_button, FALSE);
    }
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_button_grab_default_special(okbutton);

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_REQUIREMENTS) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_requirements_ok), user_data);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_requirements_cancel), user_data);
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAMS) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_params_ok), user_data);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_params_cancel), user_data);
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAM_WINDOW) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_param_window_ok), user_data);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_param_window_cancel),
                              user_data);
  } else if (rfxbuilder->table_type == RFX_TABLE_TYPE_TRIGGERS) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_triggers_ok), user_data);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_triggers_cancel), user_data);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->new_entry_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_table_add_row), user_data);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->edit_entry_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_table_edit_row), user_data);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->remove_entry_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_table_delete_row), user_data);

  if (rfxbuilder->table_type == RFX_TABLE_TYPE_PARAM_WINDOW) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->move_up_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_table_swap_row), user_data);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->move_down_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_table_swap_row), user_data);
  }

  lives_widget_show_all(dialog);
  table_select_row(rfxbuilder, -1);
}


static void on_requirements_ok(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int i;
  for (i = 0; i < rfxbuilder->onum_reqs; i++) {
    lives_free(rfxbuilder->reqs[i]);
  }
  for (i = 0; i < rfxbuilder->num_reqs; i++) {
    rfxbuilder->reqs[i] = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[i])));
  }
  lives_general_button_clicked(button, NULL);
}


static void on_requirements_cancel(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  rfxbuilder->num_reqs = rfxbuilder->onum_reqs;
  lives_general_button_clicked(button, NULL);
}


static void on_properties_ok(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  if (rfxbuilder->type != RFX_BUILD_TYPE_EFFECT0) {
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_slow)))
      rfxbuilder->props |= RFX_PROPS_SLOW;
    else
      rfxbuilder->props &= ~RFX_PROPS_SLOW;
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_noprv)))
      rfxbuilder->props |= RFX_PROPS_NO_PREVIEWS;
    else
      rfxbuilder->props &= ~RFX_PROPS_NO_PREVIEWS;
  } else {
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_batchg)))
      rfxbuilder->props |= RFX_PROPS_BATCHG;
    else
      rfxbuilder->props &= ~RFX_PROPS_BATCHG;
  }
  lives_general_button_clicked(button, NULL);
}


static void on_params_ok(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int i;

  for (i = 0; i < rfxbuilder->onum_params; i++) {
    lives_free(rfxbuilder->params[i].name);
    lives_free(rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type == LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list) lives_list_free(rfxbuilder->params[i].list);
    }
    if (rfxbuilder->copy_params[i].def) lives_free(rfxbuilder->params[i].def);
  }
  if (rfxbuilder->onum_params) {
    lives_free(rfxbuilder->params);
  }
  rfxbuilder->params = (lives_param_t *)lives_malloc(rfxbuilder->num_params * sizeof(lives_param_t));
  for (i = 0; i < rfxbuilder->num_params; i++) {
    param_copy(&rfxbuilder->params[i], &rfxbuilder->copy_params[i], FALSE);

    // this is the only place these should be freed
    lives_free(rfxbuilder->copy_params[i].name);
    lives_free(rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type == LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list) lives_list_free(rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def) lives_free(rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    lives_free(rfxbuilder->copy_params);
  }
  lives_general_button_clicked(button, NULL);
}

static void on_params_cancel(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  for (int i = 0; i < rfxbuilder->num_params; i++) {
    // this is the only place these should be freed
    lives_free(rfxbuilder->copy_params[i].name);
    lives_free(rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type == LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list) lives_list_free(rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def) lives_free(rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    lives_free(rfxbuilder->copy_params);
  }
  rfxbuilder->num_params = rfxbuilder->onum_params;
  lives_general_button_clicked(button, NULL);
}


static void on_param_window_ok(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int i;

  for (i = 0; i < rfxbuilder->onum_paramw_hints; i++) {
    lives_free(rfxbuilder->paramw_hints[i]);
  }
  for (i = 0; i < rfxbuilder->num_paramw_hints; i++) {
    rfxbuilder->paramw_hints[i] = lives_strdup_printf("%s%s%s", lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[i])),
                                  rfxbuilder->field_delim,
                                  lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[i])));
  }
  lives_general_button_clicked(button, NULL);
}

static void on_param_window_cancel(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  rfxbuilder->num_paramw_hints = rfxbuilder->onum_paramw_hints;
  lives_general_button_clicked(button, NULL);
}


static void on_code_ok(LiVESButton *button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  switch (rfxbuilder->codetype) {
  case RFX_CODE_TYPE_PRE:
    lives_free(rfxbuilder->pre_code);
    rfxbuilder->pre_code = lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_LOOP:
    lives_free(rfxbuilder->loop_code);
    rfxbuilder->loop_code = lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_POST:
    lives_free(rfxbuilder->post_code);
    rfxbuilder->post_code = lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_STRDEF: {
    int maxlen = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    char buf[++maxlen];

    if (rfxbuilder->copy_params[rfxbuilder->edit_param].def) lives_free(
        rfxbuilder->copy_params[rfxbuilder->edit_param].def);
    lives_snprintf(buf, maxlen, "%s", lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview)));

    rfxbuilder->copy_params[rfxbuilder->edit_param].def = subst(buf, rfxbuilder->field_delim, "");
    break;
  }
  case RFX_CODE_TYPE_STRING_LIST: {
    char *values = lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview));
    int numlines = get_token_count(values, '\n');
    char **lines = lives_strsplit(values, "\n", numlines);
    int defindex = get_int_param(rfxbuilder->copy_params[rfxbuilder->edit_param].def);

    if (rfxbuilder->copy_params[rfxbuilder->edit_param].list) {
      lives_list_free(rfxbuilder->copy_params[rfxbuilder->edit_param].list);
      rfxbuilder->copy_params[rfxbuilder->edit_param].list = NULL;
    }
    for (int i = 0; i < numlines; i++) {
      if (i < numlines - 1 || strlen(lines[i])) {
        rfxbuilder->copy_params[rfxbuilder->edit_param].list = lives_list_append
            (rfxbuilder->copy_params[rfxbuilder->edit_param].list, lives_strdup(lines[i]));
      }
    }
    lives_strfreev(lines);

    // set "default" combo - TODO - try to retain old default using string matching
    lives_combo_populate(LIVES_COMBO(rfxbuilder->param_def_combo),
                         rfxbuilder->copy_params[rfxbuilder->edit_param].list);
    if (!rfxbuilder->copy_params[rfxbuilder->edit_param].list ||
        defindex > lives_list_length(rfxbuilder->copy_params[rfxbuilder->edit_param].list)) {
      set_int_param(rfxbuilder->copy_params[rfxbuilder->edit_param].def, (defindex = 0));
    }
    if (rfxbuilder->copy_params[rfxbuilder->edit_param].list) {
      lives_combo_set_active_string(LIVES_COMBO(rfxbuilder->param_def_combo),
                                    (char *)lives_list_nth_data(rfxbuilder->copy_params[rfxbuilder->edit_param].list,
                                          defindex));
      // *INDENT-OFF*
}}}
  // *INDENT-ON*

  lives_general_button_clicked(button, NULL);
}


static void on_triggers_ok(LiVESButton * button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int i;
  for (i = 0; i < rfxbuilder->onum_triggers; i++) {
    lives_free(rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->onum_triggers) {
    lives_free(rfxbuilder->triggers);
  }
  rfxbuilder->triggers = (rfx_trigger_t *)lives_malloc(rfxbuilder->num_triggers * sizeof(rfx_trigger_t));
  for (i = 0; i < rfxbuilder->num_triggers; i++) {
    rfxbuilder->triggers[i].when = rfxbuilder->copy_triggers[i].when;
    rfxbuilder->triggers[i].code = lives_strdup(rfxbuilder->copy_triggers[i].code);
    lives_free(rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    lives_free(rfxbuilder->copy_triggers);
  }
  lives_general_button_clicked(button, NULL);
}


static void on_triggers_cancel(LiVESButton * button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  uint8_t *valid_triggers = NULL;

  boolean init_trigger_valid = FALSE;

  int i;

  if (rfxbuilder->num_params > 0)
    valid_triggers = (uint8_t *)lives_calloc(rfxbuilder->num_params, 1);

  for (i = 0; i < rfxbuilder->num_triggers; i++) {
    lives_free(rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    lives_free(rfxbuilder->copy_triggers);
  }
  rfxbuilder->num_triggers = rfxbuilder->onum_triggers;

  for (i = 0; i < rfxbuilder->num_triggers; i++) {
    int when = rfxbuilder->triggers[i].when;
    if (when == 0) init_trigger_valid = 1;
    else valid_triggers[when - 1] = 1;
  }

  // reset onchange for anything not in triggers
  for (i = 0; i < rfxbuilder->num_params; i++) {
    if (!valid_triggers[i]) {
      rfxbuilder->params[i].onchange = FALSE;
    } else {
      rfxbuilder->params[i].onchange = TRUE;
    }
  }

  if (valid_triggers) lives_free(valid_triggers);

  if (!init_trigger_valid) {
    rfxbuilder->has_init_trigger = FALSE;
  } else {
    rfxbuilder->has_init_trigger = TRUE;
  }

  lives_general_button_clicked(button, NULL);
}


static void on_properties_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  widget_opts.transient = LIVES_WINDOW(rfxbuilder->dialog);
  dialog = lives_standard_dialog_new(_("RFX Properties"), FALSE, -1, -1);
  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  rfxbuilder->prop_slow = lives_standard_check_button_new(_("_Slow (hint to GUI)"),
                          FALSE, LIVES_BOX(dialog_vbox), NULL);

  rfxbuilder->prop_noprv = lives_standard_check_button_new(_("Disable _previews"),
                           FALSE, LIVES_BOX(dialog_vbox), NULL);

  if (rfxbuilder->type != RFX_BUILD_TYPE_EFFECT0) {
    lives_widget_show(rfxbuilder->prop_slow);
    lives_widget_show(rfxbuilder->prop_noprv);
  }

  if (rfxbuilder->props & RFX_PROPS_SLOW) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_slow), TRUE);
  }

  if (rfxbuilder->props & RFX_PROPS_NO_PREVIEWS) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_noprv), TRUE);
  }

  if (rfxbuilder->type == RFX_BUILD_TYPE_EFFECT0) {
    rfxbuilder->prop_batchg = lives_standard_check_button_new(_("_Batch mode generator"),
                              FALSE, LIVES_BOX(dialog_vbox), NULL);
    if (rfxbuilder->props & RFX_PROPS_BATCHG) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->prop_batchg), TRUE);
    }
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_properties_ok), user_data);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);

  lives_widget_show_all(dialog);
}


static void table_select_row(rfx_build_window_t *rfxbuilder, int row) {
  for (int i = 0; i < rfxbuilder->table_rows; i++) {
    if (i == row) {
      lives_widget_set_sensitive(rfxbuilder->edit_entry_button, TRUE);
      lives_widget_set_sensitive(rfxbuilder->remove_entry_button, TRUE);
      if (rfxbuilder->move_up_button) lives_widget_set_sensitive(rfxbuilder->move_up_button, i != 0);
      if (rfxbuilder->move_down_button) lives_widget_set_sensitive(rfxbuilder->move_down_button,
            i < rfxbuilder->table_rows - 1);

      lives_widget_set_sensitive(rfxbuilder->entry[i], TRUE);
      if (rfxbuilder->entry2[i]) {
        lives_widget_set_sensitive(rfxbuilder->entry2[i], TRUE);
      }
      if (rfxbuilder->entry3[i]) {
        lives_widget_set_sensitive(rfxbuilder->entry3[i], TRUE);
      }
    } else {
      lives_editable_select_region(LIVES_EDITABLE(rfxbuilder->entry[i]), 0, 0);
      lives_widget_set_sensitive(rfxbuilder->entry[i], FALSE);
      lives_entry_set_editable(LIVES_ENTRY(rfxbuilder->entry[i]), FALSE);
      if (rfxbuilder->entry2[i]) {
        lives_editable_select_region(LIVES_EDITABLE(rfxbuilder->entry2[i]), 0, 0);
        lives_widget_set_sensitive(rfxbuilder->entry2[i], FALSE);
      }
      if (rfxbuilder->entry3[i]) {
        lives_editable_select_region(LIVES_EDITABLE(rfxbuilder->entry3[i]), 0, 0);
        lives_widget_set_sensitive(rfxbuilder->entry3[i], FALSE);
      }
    }
  }
}


static boolean on_entry_click(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  widget = lives_bin_get_child(LIVES_BIN(widget));
  for (int i = 0; i < rfxbuilder->table_rows; i++) {
    if (widget == rfxbuilder->entry[i] || widget == rfxbuilder->entry2[i] || widget == rfxbuilder->entry3[i]) {
      table_select_row(rfxbuilder, i);

      if (event->type != LIVES_BUTTON_PRESS) {
        //double click
        on_table_edit_row(NULL, rfxbuilder);
      }
      return TRUE;
    }
  }
  return TRUE;
}


static void on_table_add_row(LiVESButton * button, livespointer user_data) {
  LiVESWidget *entry = NULL, *entry2 = NULL, *entry3 = NULL;
  LiVESWidget *param_dialog = NULL;
  LiVESWidget *param_window_dialog = NULL;
  LiVESWidget *trigger_dialog = NULL;

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  lives_param_t *param = NULL;

  int i;

  char *tmpx;
  const char *ctext;

  LiVESWidget *ebox, *ebox2 = NULL, *ebox3 = NULL;

  rfxbuilder->entry2[rfxbuilder->table_rows] = rfxbuilder->entry3[rfxbuilder->table_rows] = NULL;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    if (rfxbuilder->table_rows >= RFXBUILD_MAX_REQ) return;

    for (i = 0; i < rfxbuilder->table_rows; i++) {
      lives_entry_set_editable(LIVES_ENTRY(rfxbuilder->entry[i]), FALSE);
    }

    entry = rfxbuilder->entry[rfxbuilder->table_rows] = lives_standard_entry_new(NULL, NULL, -1, -1, NULL, NULL);

    if (button)
      lives_entry_set_editable(LIVES_ENTRY(entry), TRUE);
    else
      lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);

    ebox = lives_event_box_new();
    lives_widget_set_events(ebox, LIVES_BUTTON_PRESS_MASK);

    lives_container_add(LIVES_CONTAINER(ebox), entry);

    lives_table_resize(LIVES_TABLE(rfxbuilder->table), ++rfxbuilder->table_rows, 1);
    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    lives_widget_context_update();
    lives_widget_grab_focus(entry);
    if (button) {
      rfxbuilder->num_reqs++;
    } else goto add_row_done;
    break;

  case RFX_TABLE_TYPE_PARAMS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = lives_entry_new();
    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows] = lives_entry_new();

    if (button) {
      boolean param_ok = FALSE;
      param = &rfxbuilder->copy_params[rfxbuilder->num_params];

      param->def = NULL;
      param->list = NULL;
      param->type = LIVES_PARAM_UNKNOWN;

      param_dialog = make_param_dialog(-1, rfxbuilder);
      do {
        if (lives_dialog_run(LIVES_DIALOG(param_dialog)) == LIVES_RESPONSE_CANCEL) {
          lives_free(param->def);
          lives_widget_destroy(entry);
          lives_widget_destroy(entry2);
          return;
        }
        param_ok = perform_param_checks(rfxbuilder, rfxbuilder->num_params, rfxbuilder->num_params + 1);
      } while (!param_ok);

      rfxbuilder->num_params++;

      lives_entry_set_text(LIVES_ENTRY(entry2), lives_entry_get_text(LIVES_ENTRY(rfxbuilder->param_name_entry)));
    } else {
      lives_entry_set_text(LIVES_ENTRY(entry2), rfxbuilder->params[rfxbuilder->table_rows].name);
    }

    lives_entry_set_text(LIVES_ENTRY(entry), (tmpx = lives_strdup_printf("p%d", rfxbuilder->table_rows)));
    lives_free(tmpx);

    lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);

    ebox = lives_event_box_new();
    lives_widget_set_events(ebox, LIVES_BUTTON_PRESS_MASK);

    lives_container_add(LIVES_CONTAINER(ebox), entry);

    lives_table_resize(LIVES_TABLE(rfxbuilder->table), ++rfxbuilder->table_rows, 3);

    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(0),
                       (LiVESAttachOptions)(0), 0, 0);

    lives_entry_set_editable(LIVES_ENTRY(entry2), FALSE);

    ebox2 = lives_event_box_new();
    lives_widget_set_events(ebox2, LIVES_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox2), entry2);

    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox2, 1, 2, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    entry3 = rfxbuilder->entry3[rfxbuilder->table_rows - 1] = lives_entry_new();

    if (!button) {
      param = &rfxbuilder->params[rfxbuilder->table_rows - 1];
    } else {
      param_set_from_dialog((param = &rfxbuilder->copy_params[rfxbuilder->table_rows - 1]), rfxbuilder);
    }

    switch (param->type) {
    case LIVES_PARAM_NUM:
      lives_entry_set_text(LIVES_ENTRY(entry3), (tmpx = lives_strdup_printf("num%d", param->dp)));
      lives_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      lives_entry_set_text(LIVES_ENTRY(entry3), "bool");
      break;
    case LIVES_PARAM_COLRGB24:
      lives_entry_set_text(LIVES_ENTRY(entry3), "colRGB24");
      break;
    case LIVES_PARAM_STRING:
      lives_entry_set_text(LIVES_ENTRY(entry3), "string");
      break;
    case LIVES_PARAM_STRING_LIST:
      lives_entry_set_text(LIVES_ENTRY(entry3), "string_list");
      break;
    default:
      break;
    }

    lives_entry_set_editable(LIVES_ENTRY(entry3), FALSE);

    ebox3 = lives_event_box_new();
    lives_widget_set_events(ebox3, LIVES_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox3), entry3);

    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox3, 2, 3, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL),
                       (LiVESAttachOptions)(0), 0, 0);

    if (param_dialog) lives_widget_destroy(param_dialog);
    if (!button) goto add_row_done;

    lives_widget_queue_resize(lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button) {
      param_window_dialog = make_param_window_dialog(-1, rfxbuilder);
      if (lives_dialog_run(LIVES_DIALOG(param_window_dialog)) == LIVES_RESPONSE_CANCEL) {
        return;
      }
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = lives_entry_new();
      ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
      lives_entry_set_text(LIVES_ENTRY(entry), ctext);
      rfxbuilder->num_paramw_hints++;
    } else {
      char **array = lives_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows], rfxbuilder->field_delim, 2);
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = lives_entry_new();
      lives_entry_set_text(LIVES_ENTRY(entry), array[0]);
      lives_strfreev(array);
    }

    lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);

    ebox = lives_event_box_new();
    lives_widget_set_events(ebox, LIVES_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox), entry);

    lives_table_resize(LIVES_TABLE(rfxbuilder->table), ++rfxbuilder->table_rows, 2);
    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL),
                       (LiVESAttachOptions)(0), 0, 0);

    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows - 1] = lives_entry_new();

    if (button) {
      if (!strcmp(lives_entry_get_text(LIVES_ENTRY(entry)), "layout")) {
        lives_entry_set_text(LIVES_ENTRY(entry2), lives_entry_get_text(LIVES_ENTRY(rfxbuilder->paramw_rest_entry)));
      } else {
        // TODO - use lives_rfx_special_t->has_subtype,name,num_params
        ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
        if (!strcmp(ctext, "framedraw")) {
          const char *ctext2 = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
          lives_entry_set_text(LIVES_ENTRY(entry2), (tmpx = lives_strdup_printf("%s%s%s%s%s",
                               ctext, rfxbuilder->field_delim, ctext2,
                               rfxbuilder->field_delim,
                               lives_entry_get_text(LIVES_ENTRY(rfxbuilder->paramw_rest_entry)))));
        } else {
          lives_entry_set_text(LIVES_ENTRY(entry2), (tmpx = lives_strdup_printf("%s%s%s", ctext, rfxbuilder->field_delim,
                               lives_entry_get_text(LIVES_ENTRY(rfxbuilder->paramw_rest_entry)))));
        }
        lives_free(tmpx);
      }
    } else {
      char **array = lives_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows - 1], rfxbuilder->field_delim, 2);
      lives_entry_set_text(LIVES_ENTRY(entry2), array[1]);
      lives_strfreev(array);
    }

    lives_entry_set_editable(LIVES_ENTRY(entry2), FALSE);

    ebox2 = lives_event_box_new();
    lives_widget_set_events(ebox2, LIVES_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox2), entry2);

    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox2, 1, 2, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (param_window_dialog) lives_widget_destroy(param_window_dialog);
    if (!button) goto add_row_done;

    lives_widget_queue_resize(lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    lives_widget_queue_resize(LIVES_WIDGET(rfxbuilder->table));
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = lives_entry_new();

    if (button) {
      trigger_dialog = make_trigger_dialog(-1, rfxbuilder);
      if (lives_dialog_run(LIVES_DIALOG(trigger_dialog)) == LIVES_RESPONSE_CANCEL) {
        return;
      }
      lives_entry_set_text(LIVES_ENTRY(entry), lives_entry_get_text(LIVES_ENTRY(rfxbuilder->trigger_when_entry)));
      rfxbuilder->num_triggers++;
    } else {
      char *tmpx2 = NULL;
      lives_entry_set_text(LIVES_ENTRY(entry), rfxbuilder->triggers[rfxbuilder->table_rows].when ?
                           (tmpx2 = lives_strdup_printf("%d", rfxbuilder->triggers[rfxbuilder->table_rows].when - 1)) : "init");
      if (tmpx2) lives_free(tmpx2);
    }

    lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);

    ebox = lives_event_box_new();
    lives_widget_set_events(ebox, LIVES_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox), entry);

    lives_table_resize(LIVES_TABLE(rfxbuilder->table), ++rfxbuilder->table_rows, 1);
    lives_table_attach(LIVES_TABLE(rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows - 1,
                       rfxbuilder->table_rows,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (!button) {
      if (trigger_dialog) lives_widget_destroy(trigger_dialog);
      goto add_row_done;
    }

    if (rfxbuilder->num_triggers > rfxbuilder->num_params) {
      lives_widget_set_sensitive(rfxbuilder->new_entry_button, FALSE);
    }

    rfxbuilder->copy_triggers[rfxbuilder->table_rows - 1].code = lives_text_view_get_text(LIVES_TEXT_VIEW(
          rfxbuilder->code_textview));

    rfxbuilder->copy_triggers[rfxbuilder->table_rows - 1].when = atoi(lives_entry_get_text(LIVES_ENTRY(
          rfxbuilder->trigger_when_entry))) + 1;
    if (!strcmp(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->trigger_when_entry)), "init"))
      rfxbuilder->copy_triggers[rfxbuilder->table_rows - 1].when = 0;

    if (!rfxbuilder->copy_triggers[rfxbuilder->table_rows - 1].when) {
      rfxbuilder->has_init_trigger = TRUE;
    } else {
      rfxbuilder->params[rfxbuilder->copy_triggers[rfxbuilder->table_rows - 1].when - 1].onchange = TRUE;
    }

    if (trigger_dialog) lives_widget_destroy(trigger_dialog);
    lives_widget_queue_resize(lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    lives_widget_queue_resize(LIVES_WIDGET(rfxbuilder->table));

    break;
  default:
    return;
  }

add_row_done:

  lives_widget_show_all(rfxbuilder->table);

  lives_event_box_set_above_child(LIVES_EVENT_BOX(ebox), TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(ebox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(on_entry_click), (livespointer)rfxbuilder);

  if (palette->style & STYLE_1) {
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
      lives_widget_set_fg_color(entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
    } else {
      lives_widget_set_bg_color(entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
      lives_widget_set_fg_color(entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
    }
  }

  if (entry2) {
    lives_event_box_set_above_child(LIVES_EVENT_BOX(ebox2), TRUE);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(ebox2), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(on_entry_click), (livespointer)rfxbuilder);

    if (palette->style & STYLE_1) {
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
        lives_widget_set_fg_color(entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
      } else {
        lives_widget_set_bg_color(entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
        lives_widget_set_fg_color(entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
      }
    }
  }

  if (entry3) {
    lives_event_box_set_above_child(LIVES_EVENT_BOX(ebox3), TRUE);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(ebox3), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(on_entry_click), (livespointer)rfxbuilder);

    if (palette->style & STYLE_1) {
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
        lives_widget_set_fg_color(entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
      } else {
        lives_widget_set_bg_color(entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
        lives_widget_set_fg_color(entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
      }
    }
  }

  if (button) {
    table_select_row(rfxbuilder, rfxbuilder->table_rows - 1);

    if (rfxbuilder->table_type == RFX_TABLE_TYPE_REQUIREMENTS) {
      on_table_edit_row(NULL, user_data);
    }
  }
}


static void param_set_from_dialog(lives_param_t *copy_param, rfx_build_window_t *rfxbuilder) {
  // set parameter values from param_dialog
  // this is called after adding a new copy_param or editing an existing one
  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  copy_param->name = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->param_name_entry)));
  copy_param->label = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->param_label_entry)));

  if (!strcmp(ctext, "num")) {
    copy_param->type = LIVES_PARAM_NUM;
  } else if (!strcmp(ctext, "bool")) {
    copy_param->type = LIVES_PARAM_BOOL;
  } else if (!strcmp(ctext, "colRGB24")) {
    copy_param->type = LIVES_PARAM_COLRGB24;
  } else if (!strcmp(ctext, "string")) {
    copy_param->type = LIVES_PARAM_STRING;
  } else if (!strcmp(ctext, "string_list")) {
    copy_param->type = LIVES_PARAM_STRING_LIST;
  }

  copy_param->dp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp));
  copy_param->group = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_group));
  copy_param->onchange = FALSE; // no trigger assigned yet

  // TODO - check
  if (copy_param->type != LIVES_PARAM_STRING && copy_param->def) lives_free(copy_param->def);
  if (copy_param->type != LIVES_PARAM_STRING_LIST && copy_param->list) lives_list_free(copy_param->list);

  switch (copy_param->type) {
  case LIVES_PARAM_NUM:
    copy_param->min = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min));
    copy_param->max = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->step_size = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step));
    copy_param->wrap = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton));
    if (!copy_param->dp) {
      copy_param->def = lives_malloc(sizint);
      set_int_param(copy_param->def, lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    } else {
      copy_param->def = lives_malloc(sizdbl);
      set_double_param(copy_param->def, lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    }
    break;
  case LIVES_PARAM_BOOL:
    copy_param->def = lives_malloc(sizint);
    set_bool_param(copy_param->def, lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    copy_param->dp = 0;
    break;
  case LIVES_PARAM_STRING:
    copy_param->max = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->dp = 0;
    break;
  case LIVES_PARAM_STRING_LIST:
    copy_param->dp = 0;
    copy_param->def = lives_malloc(sizdbl);
    ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_def_combo));
    set_int_param(copy_param->def, lives_list_strcmp_index(copy_param->list, ctext, TRUE));
    break;
  case LIVES_PARAM_COLRGB24:
    copy_param->def = lives_malloc(3 * sizint);
    set_colRGB24_param(copy_param->def, lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)),
                       lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)),
                       lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    copy_param->dp = 0;
    break;
  default:
    break;
  }
}


static void on_table_edit_row(LiVESButton * button, livespointer user_data) {
  LiVESWidget *param_dialog;
  LiVESWidget *paramw_dialog;
  LiVESWidget *trigger_dialog;

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  lives_param_t *param;

  int i;
  int found = -1;
  boolean param_ok = FALSE;

  char *tmpx;
  const char *ctext;

  for (i = 0; i < rfxbuilder->table_rows && found == -1; i++) {
    if (lives_widget_is_sensitive(rfxbuilder->entry[i])) {
      found = i;
      break;
    }
  }
  if (found == -1) return;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i = 0; i < rfxbuilder->table_rows; i++) {
      if (found == i) {
        lives_entry_set_editable(LIVES_ENTRY(rfxbuilder->entry[i]), TRUE);
        lives_widget_context_update();
        lives_widget_grab_focus(rfxbuilder->entry[i]);
      } else {
        lives_entry_set_editable(LIVES_ENTRY(rfxbuilder->entry[i]), FALSE);
      }
    }
    break;

  case RFX_TABLE_TYPE_PARAMS:
    param_dialog = make_param_dialog(found, rfxbuilder);
    do {
      if (lives_dialog_run(LIVES_DIALOG(param_dialog)) == LIVES_RESPONSE_CANCEL) {
        return;
      }
      param_ok = perform_param_checks(rfxbuilder, found, rfxbuilder->num_params);
    } while (!param_ok);

    param_set_from_dialog((param = &rfxbuilder->copy_params[found]), rfxbuilder);
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[found]), param->name);
    switch (param->type) {
    case LIVES_PARAM_NUM:
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry3[found]), (tmpx = lives_strdup_printf("num%d", param->dp)));
      lives_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry3[found]), "bool");
      break;
    case LIVES_PARAM_COLRGB24:
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry3[found]), "colRGB24");
      break;
    case LIVES_PARAM_STRING:
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry3[found]), "string");
      break;
    case LIVES_PARAM_STRING_LIST:
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry3[found]), "string_list");
      break;
    default:
      break;
    }
    lives_widget_destroy(param_dialog);
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    paramw_dialog = make_param_window_dialog(found, rfxbuilder);
    if (lives_dialog_run(LIVES_DIALOG(paramw_dialog)) == LIVES_RESPONSE_CANCEL) {
      return;
    }
    ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry[found]), ctext);
    if (!strcmp(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[found])), "layout")) {
      lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[found]),
                           lives_entry_get_text(LIVES_ENTRY(rfxbuilder->paramw_rest_entry)));
    } else {
      // TODO - use lives_rfx_special_t->has_subtype,name,num_params
      ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
      if (!strcmp(ctext, "framedraw")) {
        const char *ctext2 = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
        lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[found]),
                             (tmpx = lives_strdup_printf("%s%s%s%s%s", ctext, rfxbuilder->field_delim, ctext2,
                                     rfxbuilder->field_delim,
                                     lives_entry_get_text(LIVES_ENTRY(rfxbuilder->paramw_rest_entry)))));
      } else {
        lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[found]),
                             (tmpx = lives_strdup_printf("%s%s%s", ctext, rfxbuilder->field_delim, lives_entry_get_text
                                     (LIVES_ENTRY(rfxbuilder->paramw_rest_entry)))));
      }
      lives_free(tmpx);
    }
    lives_widget_destroy(paramw_dialog);
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    trigger_dialog = make_trigger_dialog(found, rfxbuilder);
    if (lives_dialog_run(LIVES_DIALOG(trigger_dialog)) == LIVES_RESPONSE_CANCEL) {
      return;
    }

    lives_free(rfxbuilder->copy_triggers[found].code);
    rfxbuilder->copy_triggers[found].code = lives_text_view_get_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview));

    lives_widget_destroy(trigger_dialog);
    break;
  }
}


static void on_table_swap_row(LiVESButton * button, livespointer user_data) {
  char *entry_text;
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  int found = -1;

  for (int i = 0; i < rfxbuilder->table_rows && found == -1; i++) {
    if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
      found = i;
      break;
    }
  }

  if (found == -1) if ((found = rfxbuilder->table_swap_row1) == -1) return;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button == LIVES_BUTTON(rfxbuilder->move_up_button)) {
      rfxbuilder->table_swap_row2 = found - 1;
    } else if (button == LIVES_BUTTON(rfxbuilder->move_down_button)) {
      rfxbuilder->table_swap_row2 = found + 1;
    }
    if (rfxbuilder->table_swap_row2 < 0 || rfxbuilder->table_swap_row2 >= rfxbuilder->table_rows) return;

    entry_text = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[found])));
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry[found]),
                         lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[rfxbuilder->table_swap_row2])));
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry[rfxbuilder->table_swap_row2]), entry_text);
    lives_free(entry_text);

    entry_text = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[found])));
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[found]),
                         lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[rfxbuilder->table_swap_row2])));
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->entry2[rfxbuilder->table_swap_row2]), entry_text);
    lives_free(entry_text);

    break;
  default:
    break;
  }
  rfxbuilder->table_swap_row1 = rfxbuilder->table_swap_row2;
  table_select_row(rfxbuilder, rfxbuilder->table_swap_row1);
}


static void on_table_delete_row(LiVESButton * button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  LiVESWidget *ebox;

#if !LIVES_TABLE_IS_GRID
  LiVESWidget *ebox2 = NULL, *ebox3 = NULL, *ebox4 = NULL;
#else
  LiVESWidget *entry = NULL, *entry2;
#endif

  int move = 0;
  boolean triggers_adjusted = FALSE;

  int i;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i = 0; i < rfxbuilder->table_rows; i++) {
      if (move > 0) {
#if !LIVES_TABLE_IS_GRID
        if (i < rfxbuilder->table_rows - 1) ebox2 = lives_widget_get_parent(rfxbuilder->entry[i]);
        lives_widget_reparent(rfxbuilder->entry[i], ebox);
        ebox = ebox2;
#endif
        rfxbuilder->entry[i - 1] = rfxbuilder->entry[i];
      } else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
#if LIVES_TABLE_IS_GRID
        lives_grid_remove_row(LIVES_GRID(rfxbuilder->table), i);
#else
        ebox = lives_widget_get_parent(rfxbuilder->entry[i]);
        if (rfxbuilder->table_rows > 1) {
          lives_table_resize(LIVES_TABLE(rfxbuilder->table), rfxbuilder->table_rows - 1, 1);
        }
        lives_widget_destroy(rfxbuilder->entry[i]);
#endif
        move = i + 1;
      }
    }
    if (move == 0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_reqs--;
    break;

  case RFX_TABLE_TYPE_PARAMS:
    for (i = 0; i < rfxbuilder->table_rows; i++) {
      if (move > 0) {
        // note - parameters become renumbered here

#if !LIVES_TABLE_IS_GRID
        // move everything up except p%d
        if (i < rfxbuilder->table_rows - 1) {
          ebox3 = lives_widget_get_parent(rfxbuilder->entry2[i]);
          ebox4 = lives_widget_get_parent(rfxbuilder->entry3[i]);
        }
        lives_widget_reparent(rfxbuilder->entry2[i], ebox);
        lives_widget_reparent(rfxbuilder->entry3[i], ebox2);
        ebox = ebox3;
        ebox2 = ebox4;
#else
        // move p%d up so when we remove the row the numbering is ok
        entry2 = rfxbuilder->entry[i];
        ebox = lives_widget_get_parent(rfxbuilder->entry[i]);
        lives_widget_object_ref(entry2);
        lives_widget_unparent(entry2);
        lives_container_add(LIVES_CONTAINER(ebox), entry);
        lives_widget_object_unref(entry);
        entry = entry2;
#endif

        rfxbuilder->entry2[i - 1] = rfxbuilder->entry2[i];
        rfxbuilder->entry3[i - 1] = rfxbuilder->entry3[i];
        param_copy(&rfxbuilder->copy_params[i - 1], &rfxbuilder->copy_params[i], FALSE);
        lives_free(rfxbuilder->copy_params[i].name);
        lives_free(rfxbuilder->copy_params[i].label);
        lives_free(rfxbuilder->copy_params[i].def);

      } else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
        if (rfxbuilder->copy_params[i].onchange) {
          do_error_dialog(_("\n\nCannot remove this parameter as it has a trigger.\nPlease remove the trigger first.\n\n"));
          return;
        }

#if LIVES_TABLE_IS_GRID
        entry = rfxbuilder->entry[i];
        lives_widget_object_ref(entry);
        lives_widget_unparent(entry);
        lives_grid_remove_row(LIVES_GRID(rfxbuilder->table), i);
#else
        ebox = lives_widget_get_parent(rfxbuilder->entry2[i]);
        ebox2 = lives_widget_get_parent(rfxbuilder->entry3[i]);

        lives_widget_destroy(rfxbuilder->entry[rfxbuilder->table_rows - 1]);
        lives_widget_destroy(rfxbuilder->entry2[i]);
        lives_widget_destroy(rfxbuilder->entry3[i]);

        if (rfxbuilder->table_rows > 1) {
          lives_table_resize(LIVES_TABLE(rfxbuilder->table), rfxbuilder->table_rows - 1, 3);
        }
#endif
        lives_free(rfxbuilder->copy_params[i].name);
        lives_free(rfxbuilder->copy_params[i].label);
        lives_free(rfxbuilder->copy_params[i].def);

        move = i + 1;
      }
    }

#if LIVES_TABLE_IS_GRID
    if (entry) lives_widget_destroy(entry);
#endif

    if (move == 0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_params--;
    for (i = 0; i < rfxbuilder->num_triggers; i++) {
      if (rfxbuilder->triggers[i].when > move) {
        rfxbuilder->params[rfxbuilder->triggers[i].when - 1].onchange = FALSE;
        rfxbuilder->params[--rfxbuilder->triggers[i].when - 1].onchange = TRUE;
      }
      triggers_adjusted = TRUE;
    }
    if (triggers_adjusted) {
      do_error_dialog(_("\n\nSome triggers were adjusted.\nPlease check the trigger code.\n"));
    }
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    for (i = 0; i < rfxbuilder->table_rows; i++) {
      if (move > 0) {
#if !LIVES_TABLE_IS_GRID
        if (i < rfxbuilder->table_rows - 1) {
          ebox3 = lives_widget_get_parent(rfxbuilder->entry[i]);
          ebox4 = lives_widget_get_parent(rfxbuilder->entry2[i]);
        }
        lives_widget_reparent(rfxbuilder->entry[i], ebox);
        lives_widget_reparent(rfxbuilder->entry2[i], ebox2);
        ebox = ebox3;
        ebox2 = ebox4;
#endif

        rfxbuilder->entry[i - 1] = rfxbuilder->entry[i];
        rfxbuilder->entry2[i - 1] = rfxbuilder->entry2[i];
      } else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
#if LIVES_TABLE_IS_GRID
        lives_grid_remove_row(LIVES_GRID(rfxbuilder->table), i);
#else
        ebox = lives_widget_get_parent(rfxbuilder->entry[i]);
        ebox2 = lives_widget_get_parent(rfxbuilder->entry2[i]);

        lives_widget_destroy(rfxbuilder->entry[i]);
        lives_widget_destroy(rfxbuilder->entry2[i]);
        if (rfxbuilder->table_rows > 1) {
          lives_table_resize(LIVES_TABLE(rfxbuilder->table), rfxbuilder->table_rows - 1, 1);
        }
#endif
        move = i + 1;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_paramw_hints--;
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    for (i = 0; i < rfxbuilder->table_rows; i++) {
      if (move > 0) {
#if !LIVES_TABLE_IS_GRID
        if (i < rfxbuilder->table_rows - 1) ebox2 = lives_widget_get_parent(rfxbuilder->entry[i]);
        lives_widget_reparent(rfxbuilder->entry[i], ebox);
        ebox = ebox2;
#endif

        rfxbuilder->entry[i - 1] = rfxbuilder->entry[i];
        rfxbuilder->copy_triggers[i - 1].when = rfxbuilder->copy_triggers[i].when;
        rfxbuilder->copy_triggers[i - 1].code = lives_strdup(rfxbuilder->copy_triggers[i].code);
        lives_free(rfxbuilder->copy_triggers[i].code);
      } else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
        int when = atoi(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[i]))) + 1;

        if (!strcmp(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[i])), "init")) rfxbuilder->has_init_trigger = FALSE;
        else rfxbuilder->params[when - 1].onchange = FALSE;

        lives_free(rfxbuilder->copy_triggers[i].code);

#if LIVES_TABLE_IS_GRID
        lives_grid_remove_row(LIVES_GRID(rfxbuilder->table), i);
#else
        ebox = lives_widget_get_parent(rfxbuilder->entry[i]);
        if (rfxbuilder->table_rows > 1) {
          lives_table_resize(LIVES_TABLE(rfxbuilder->table), rfxbuilder->table_rows - 1, 1);
        }
        lives_widget_destroy(rfxbuilder->entry[i]);
#endif
        move = i + 1;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_triggers--;

    if (rfxbuilder->table_rows <= rfxbuilder->num_params) {
      lives_widget_set_sensitive(rfxbuilder->new_entry_button, TRUE);
    }

    break;
  default:
    return;
  }

  if (rfxbuilder->table_rows == 0) {
    lives_widget_set_sensitive(rfxbuilder->edit_entry_button, FALSE);
    lives_widget_set_sensitive(rfxbuilder->remove_entry_button, FALSE);
    if (rfxbuilder->move_up_button) lives_widget_set_sensitive(rfxbuilder->move_up_button, FALSE);
    if (rfxbuilder->move_down_button) lives_widget_set_sensitive(rfxbuilder->move_down_button, FALSE);
  } else {
    if (move >= rfxbuilder->table_rows) move = rfxbuilder->table_rows - 1;
    table_select_row(rfxbuilder, move);
  }
}


LiVESWidget *make_param_dialog(int pnum, rfx_build_window_t *rfxbuilder) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  LiVESList *typelist = NULL;

  lives_colRGB48_t rgb;

  char *tmp, *tmp2, *title;

  if (pnum < 0) {
    title = (_("New RFX Parameter"));
  } else {
    title = (_("Edit RFX Parameter"));
  }

  dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // name

  rfxbuilder->param_name_entry = lives_standard_entry_new((tmp = (_("_Name:    "))),
                                 pnum >= 0 ? rfxbuilder->copy_params[pnum].name : NULL,
                                 MEDIUM_ENTRY_WIDTH, -1, LIVES_BOX(dialog_vbox),
                                 (tmp2 = lives_strdup((_("Name of the parameter, must be unique in the plugin.")))));

  if (pnum < 0) lives_widget_grab_focus(rfxbuilder->param_name_entry);

  lives_free(tmp);
  lives_free(tmp2);

  // label

  rfxbuilder->param_label_entry = lives_standard_entry_new((tmp = (_("_Label:    "))),
                                  pnum >= 0 ? rfxbuilder->copy_params[pnum].label : NULL,
                                  MEDIUM_ENTRY_WIDTH, -1, LIVES_BOX(dialog_vbox),
                                  (tmp2 = (_("Label to be shown by the parameter. "
                                          "An underscore represents mnemonic accelerator."))));
  lives_free(tmp);
  lives_free(tmp2);

  // type

  typelist = lives_list_append(typelist, (livespointer)"num");
  typelist = lives_list_append(typelist, (livespointer)"bool");
  typelist = lives_list_append(typelist, (livespointer)"string");
  typelist = lives_list_append(typelist, (livespointer)"colRGB24");
  typelist = lives_list_append(typelist, (livespointer)"string_list");

  rfxbuilder->param_type_combo = lives_standard_combo_new((tmp = (_("_Type:         "))), typelist,
                                 LIVES_BOX(dialog_vbox), (tmp2 = (_("Parameter type (select from list)."))));
  lives_free(tmp);
  lives_free(tmp2);
  lives_list_free(typelist);

  rfxbuilder->edit_param = pnum;

  if (pnum >= 0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo), 0); break;
    case LIVES_PARAM_BOOL:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo), 1); break;
    case LIVES_PARAM_STRING:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo), 2); break;
    case LIVES_PARAM_COLRGB24:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo), 3); break;
    case LIVES_PARAM_STRING_LIST:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo), 4); break;
    default: break;
    }
  } else rfxbuilder->edit_param = rfxbuilder->num_params;

  // dp

  rfxbuilder->spinbutton_param_dp = lives_standard_spin_button_new(_("Decimal _places: "),
                                    pnum >= 0 ? rfxbuilder->copy_params[pnum].dp : 0., 0., 16., 1., 1., 0, LIVES_BOX(dialog_vbox), NULL);
  rfxbuilder->param_dp_label = widget_opts.last_label;

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  // default val

  rfxbuilder->spinbutton_param_def = lives_standard_spin_button_new(_("_Default value:    "), 0., -LIVES_MAXINT, LIVES_MAXINT, 1.,
                                     1., 0, LIVES_BOX(dialog_vbox), NULL);
  rfxbuilder->param_def_label = widget_opts.last_label;

  // extra bits for string/string_list

  rfxbuilder->param_strdef_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), rfxbuilder->param_strdef_hbox, TRUE, TRUE, widget_opts.packing_height);

  rfxbuilder->param_strdef_button = lives_standard_button_new(DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(rfxbuilder->param_strdef_hbox), rfxbuilder->param_strdef_button, TRUE, TRUE,
                       widget_opts.packing_width);

  rfxbuilder->param_strlist_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), rfxbuilder->param_strlist_hbox, FALSE, FALSE, widget_opts.packing_width);

  rfxbuilder->param_def_combo = lives_standard_combo_new(_("_Default: "), NULL, LIVES_BOX(rfxbuilder->param_strlist_hbox), NULL);

  lives_label_set_mnemonic_widget(LIVES_LABEL(rfxbuilder->param_def_label), rfxbuilder->param_def_combo);

  if (pnum >= 0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      if (!rfxbuilder->copy_params[pnum].dp) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                    (double)get_int_param(rfxbuilder->copy_params[pnum].def));
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                    get_double_param(rfxbuilder->copy_params[pnum].def));
      }
      break;
    case LIVES_PARAM_BOOL:
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                  (double)get_bool_param(rfxbuilder->copy_params[pnum].def));
      break;
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfxbuilder->copy_params[pnum].def, &rgb);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), (double)rgb.red);
      break;
    default:
      break;
    }
  }

  // group

  rfxbuilder->hbox_bg = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), rfxbuilder->hbox_bg, FALSE, FALSE, widget_opts.packing_height);

  rfxbuilder->spinbutton_param_group = lives_standard_spin_button_new((tmp = (_("Button _Group: "))), 0., 0., 16., 1.,
                                       1., 0, LIVES_BOX(rfxbuilder->hbox_bg),
                                       (tmp2 = (_("A non-zero value can be used to group radio buttons."))));

  if (pnum >= 0) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_group),
                                (double)rfxbuilder->copy_params[pnum].group);
  }

  // min

  rfxbuilder->spinbutton_param_min = lives_standard_spin_button_new(_("_Minimum value: "), 0., -LIVES_MAXINT, LIVES_MAXINT, 1.,
                                     1., 0, LIVES_BOX(dialog_vbox), NULL);
  rfxbuilder->param_min_label = widget_opts.last_label;

  // max

  rfxbuilder->spinbutton_param_max = lives_standard_spin_button_new(_("Ma_ximum value: "), RFX_DEF_NUM_MAX, -LIVES_MAXINT,
                                     LIVES_MAXINT, 1., 1., 0, LIVES_BOX(dialog_vbox), NULL);
  rfxbuilder->param_max_label = widget_opts.last_label;

  // step size

  rfxbuilder->spinbutton_param_step = lives_standard_spin_button_new((tmp = (_("     _Step size:   "))), 1., 1.,
                                      LIVES_MAXINT, 1., 1., 0, LIVES_BOX(dialog_vbox),
                                      (tmp2 = (_("How much the parameter is adjusted when the spinbutton arrows are pressed."))));
  rfxbuilder->param_step_label = widget_opts.last_label;
  lives_free(tmp);
  lives_free(tmp2);

  // wrap

  rfxbuilder->param_wrap_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), rfxbuilder->param_wrap_hbox, TRUE, TRUE, widget_opts.packing_height);

  rfxbuilder->param_wrap_checkbutton = lives_standard_check_button_new((tmp = (_("_Wrap value"))), FALSE,
                                       LIVES_BOX(rfxbuilder->param_wrap_hbox),
                                       (tmp2 = (_("Whether the value wraps max->min and min->max."))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->param_strdef_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_code_clicked), (livespointer)rfxbuilder);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->param_type_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_param_type_changed), (livespointer)rfxbuilder);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->spinbutton_param_dp), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_param_dp_changed), (livespointer)rfxbuilder);

  rfxbuilder->def_spin_f = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->spinbutton_param_def),
                           LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(after_param_def_changed), (livespointer)rfxbuilder);

  rfxbuilder->min_spin_f = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->spinbutton_param_min),
                           LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(after_param_min_changed), (livespointer)rfxbuilder);

  rfxbuilder->max_spin_f = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rfxbuilder->spinbutton_param_max),
                           LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(after_param_max_changed), (livespointer)rfxbuilder);

  if (pnum >= 0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), rfxbuilder->copy_params[pnum].min);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), rfxbuilder->copy_params[pnum].max);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
                                  rfxbuilder->copy_params[pnum].step_size);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton),
                                     rfxbuilder->copy_params[pnum].wrap);
      break;
    case LIVES_PARAM_COLRGB24:
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), (double)rgb.green);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), (double)rgb.blue);
      break;
    case LIVES_PARAM_STRING:
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), rfxbuilder->copy_params[pnum].max);
      break;
    default:
      break;
    }
  }
  lives_widget_show_all(dialog);
  on_param_type_changed(LIVES_COMBO(rfxbuilder->param_type_combo), (livespointer)rfxbuilder);
  after_param_dp_changed(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp), (livespointer)rfxbuilder);
  return dialog;
}


static void after_param_dp_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));
  int dp;

  if (strcmp(ctext, "num")) return;

  dp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));

  lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), dp);
  lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), dp);
  lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), dp);
  lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), dp);

  if (dp > 0) {
    double max = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    double min = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), -LIVES_MAXFLOAT, LIVES_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), -LIVES_MAXFLOAT, LIVES_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), -LIVES_MAXFLOAT, LIVES_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), 1. / (double)lives_10pow(dp),
                                (max - min) > 1. ? (max - min) : 1.);

    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_step), MAXFLOATLEN + dp);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_def), MAXFLOATLEN + dp);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_min), MAXFLOATLEN + dp);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_max), MAXFLOATLEN + dp);
  } else {
    int max = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    int min = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), -LIVES_MAXINT, LIVES_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), -LIVES_MAXINT, LIVES_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), -LIVES_MAXINT, LIVES_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), 1, (max - min) > 1 ? (max - min) : 1);

    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_step), MAXINTLEN);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_def), MAXINTLEN);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_min), MAXINTLEN);
    lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_max), MAXINTLEN);
  }
}


static void after_param_min_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  int dp;
  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp(ctext, "num")) return;

  lives_signal_handler_block(rfxbuilder->spinbutton_param_max, rfxbuilder->max_spin_f);
  lives_signal_handler_block(rfxbuilder->spinbutton_param_def, rfxbuilder->def_spin_f);

  if ((dp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp))) > 0) {
    if (lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)) <
        lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                  lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
                                lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)), LIVES_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
                                1. / (double)lives_10pow(dp), lives_spin_button_get_value
                                (LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)) -
                                lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));

  } else {
    if (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)) <
        lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                  lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
                                lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)),
                                LIVES_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), 1,
                                lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)) -
                                lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
  }

  lives_signal_handler_unblock(rfxbuilder->spinbutton_param_max, rfxbuilder->max_spin_f);
  lives_signal_handler_unblock(rfxbuilder->spinbutton_param_def, rfxbuilder->def_spin_f);
}


static void after_param_max_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  int dp;
  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp(ctext, "num")) return;

  lives_signal_handler_block(rfxbuilder->spinbutton_param_min, rfxbuilder->min_spin_f);
  lives_signal_handler_block(rfxbuilder->spinbutton_param_def, rfxbuilder->def_spin_f);

  if ((dp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp))) > 0) {
    if (lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)) >
        lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                  lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), -LIVES_MAXFLOAT,
                                lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
                                1. / (double)lives_10pow(dp),
                                lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)) -
                                lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
  } else {
    if (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)) >
        lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
                                  lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), -LIVES_MAXINT,
                                lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), 1,
                                lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))
                                - lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
  }

  lives_signal_handler_unblock(rfxbuilder->spinbutton_param_min, rfxbuilder->min_spin_f);
  lives_signal_handler_unblock(rfxbuilder->spinbutton_param_def, rfxbuilder->def_spin_f);
}


static void after_param_def_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp(ctext, "num")) return;

  if (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp))) {
    double dbl_def = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def));
    if (dbl_def < lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), dbl_def);
    } else if (dbl_def > lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), dbl_def);
    }
  } else {
    int int_def = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def));
    if (int_def < lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), (double)int_def);
    } else if (int_def > lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), (double)int_def);
    }
  }
}


static void on_param_type_changed(LiVESCombo * param_type_combo, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  int pnum = rfxbuilder->edit_param;

  const char *ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  lives_widget_show(rfxbuilder->param_min_label);
  lives_widget_show(rfxbuilder->param_max_label);
  lives_widget_show(rfxbuilder->spinbutton_param_min);
  lives_widget_show(rfxbuilder->spinbutton_param_max);
  lives_widget_show(rfxbuilder->spinbutton_param_dp);
  lives_widget_show(rfxbuilder->spinbutton_param_def);
  lives_widget_show(rfxbuilder->param_dp_label);
  lives_widget_show(rfxbuilder->param_def_label);

  lives_widget_hide(rfxbuilder->param_strdef_hbox);
  lives_widget_hide(rfxbuilder->param_strlist_hbox);
  lives_widget_hide(rfxbuilder->hbox_bg);
  lives_widget_hide(rfxbuilder->spinbutton_param_step);
  lives_widget_hide(rfxbuilder->param_step_label);
  lives_widget_hide(rfxbuilder->param_wrap_hbox);

  if (pnum < 0) return;

  if (!strcmp(ctext, "string_list")) {
    int defindex;

    if (rfxbuilder->copy_params[pnum].type != LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[pnum].def) {
        lives_free(rfxbuilder->copy_params[pnum].def);
      }
      rfxbuilder->copy_params[pnum].def = lives_malloc(sizint);
      set_int_param(rfxbuilder->copy_params[pnum].def, 0);
    }

    rfxbuilder->copy_params[pnum].type = LIVES_PARAM_STRING_LIST;
    defindex = get_int_param(rfxbuilder->copy_params[pnum].def);

    lives_combo_populate(LIVES_COMBO(rfxbuilder->param_def_combo), rfxbuilder->copy_params[pnum].list);

    if (!rfxbuilder->copy_params[pnum].list || defindex > lives_list_length(rfxbuilder->copy_params[pnum].list)) {
      set_int_param(rfxbuilder->copy_params[pnum].def, (defindex = 0));
    }

    if (rfxbuilder->copy_params[pnum].list) {
      lives_combo_set_active_string(LIVES_COMBO(rfxbuilder->param_def_combo),
                                    (char *)lives_list_nth_data(rfxbuilder->copy_params[pnum].list, defindex));
    }

    lives_widget_hide(rfxbuilder->spinbutton_param_dp);
    lives_widget_hide(rfxbuilder->param_dp_label);
    lives_widget_hide(rfxbuilder->spinbutton_param_min);
    lives_widget_hide(rfxbuilder->param_min_label);
    lives_widget_hide(rfxbuilder->spinbutton_param_def);
    lives_widget_hide(rfxbuilder->param_def_label);
    lives_widget_hide(rfxbuilder->spinbutton_param_max);
    lives_widget_hide(rfxbuilder->param_max_label);

    lives_button_set_label(LIVES_BUTTON(rfxbuilder->param_strdef_button), (_("Set _values")));
    lives_widget_show(rfxbuilder->param_strdef_hbox);
    lives_widget_show(rfxbuilder->param_strlist_hbox);
  } else {
    if (!strcmp(ctext, "num")) {
      rfxbuilder->copy_params[pnum].type = LIVES_PARAM_NUM;
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_def_label), (_("_Default value:    ")));
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_min_label), (_("_Minimum value: ")));
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_max_label), (_("Ma_ximum value: ")));
      lives_widget_show(rfxbuilder->spinbutton_param_step);
      lives_widget_show(rfxbuilder->param_step_label);
      lives_widget_show_all(rfxbuilder->param_wrap_hbox);

      after_param_dp_changed(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp), (livespointer)rfxbuilder);

      if (pnum < 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), 0.);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), RFX_DEF_NUM_MAX);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0.);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step), 1.);
      }
    } else if (!strcmp(ctext, "bool")) {
      rfxbuilder->copy_params[pnum].type = LIVES_PARAM_BOOL;
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_def_label), (_("_Default value:    ")));
      lives_widget_hide(rfxbuilder->param_min_label);
      lives_widget_hide(rfxbuilder->param_max_label);
      lives_widget_hide(rfxbuilder->spinbutton_param_min);
      lives_widget_hide(rfxbuilder->spinbutton_param_max);
      lives_widget_hide(rfxbuilder->spinbutton_param_dp);
      lives_widget_hide(rfxbuilder->param_dp_label);
      lives_widget_show(rfxbuilder->hbox_bg);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0, 1);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 0);
    } else if (!strcmp(ctext, "colRGB24")) {
      rfxbuilder->copy_params[pnum].type = LIVES_PARAM_COLRGB24;
      lives_widget_hide(rfxbuilder->spinbutton_param_dp);
      lives_widget_hide(rfxbuilder->param_dp_label);
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_def_label), (_("Default _Red:  ")));
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_min_label), (_("Default _Green:")));
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_max_label), (_("Default _Blue: ")));
      lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_def), 4);
      lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_min), 4);
      lives_entry_set_width_chars(LIVES_ENTRY(rfxbuilder->spinbutton_param_max), 4);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0, 255);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), 0, 255);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 0, 255);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 0);
      if (pnum < 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 0.);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min), 0.);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0.);
      }
    } else if (!strcmp(ctext, "string")) {
      rfxbuilder->copy_params[pnum].type = LIVES_PARAM_STRING;
      lives_widget_hide(rfxbuilder->spinbutton_param_dp);
      lives_widget_hide(rfxbuilder->param_dp_label);
      lives_widget_hide(rfxbuilder->spinbutton_param_min);
      lives_widget_hide(rfxbuilder->param_min_label);
      lives_widget_hide(rfxbuilder->spinbutton_param_def);

      lives_button_set_label(LIVES_BUTTON(rfxbuilder->param_strdef_button), (_("Set _default")));
      lives_widget_show(rfxbuilder->param_strdef_hbox);
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_def_label), (_("Default value:  ")));
      lives_label_set_text(LIVES_LABEL(rfxbuilder->param_max_label), (_("Maximum length (chars): ")));
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 1, RFX_MAXSTRINGLEN);
      if (pnum < 0) lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), RFX_TEXT_MAGIC);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max), 0);

      if (!rfxbuilder->copy_params[pnum].def) {
        rfxbuilder->copy_params[pnum].def = lives_strdup("");
	// *INDENT-OFF*
      }}}
  // *INDENT-ONt*
}


LiVESWidget *make_param_window_dialog(int pnum, rfx_build_window_t *rfxbuilder) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;

  LiVESList *kwlist = NULL;
  LiVESList *splist = NULL;
  LiVESList *spsublist = NULL;

  char *kw = NULL;
  char *sp = NULL;
  char *spsub = NULL;
  char *rest = NULL;
  char *title;

  if (pnum < 0) {
    title = (_("New RFX Parameter Window Hint"));
  } else {
    title = (_("Edit RFX Parameter Window Hint"));
  }

  dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  if (pnum >= 0) {
    kw = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[pnum])));
    if (!strcmp(kw, "layout")) {
      rest = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[pnum])));
    } else if (!strcmp(kw, "special")) {
      int numtok = get_token_count(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[pnum])),
                                   (int)rfxbuilder->field_delim[0]);
      char **array = lives_strsplit(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[pnum])), rfxbuilder->field_delim, 3);
      sp = lives_strdup(array[0]);
      if (!strcmp(sp, "framedraw")) {
        if (numtok > 1) {
          spsub = lives_strdup(array[1]);
        }
        if (numtok > 2) {
          rest = lives_strdup(array[2]);
        }
      } else {
        if (numtok > 1) {
          rest = lives_strdup(array[1]);
        }
      }
      lives_strfreev(array);
    }
  }

  // kw
  kwlist = lives_list_append(kwlist, (livespointer)"layout");
  kwlist = lives_list_append(kwlist, (livespointer)"special");

  rfxbuilder->paramw_kw_combo = lives_standard_combo_new(_("_Keyword:         "), kwlist, LIVES_BOX(dialog_vbox), NULL);
  lives_list_free(kwlist);

  if (pnum >= 0 && kw) {
    lives_combo_set_active_string(LIVES_COMBO(rfxbuilder->paramw_kw_combo), kw);
  }

  // type
  splist = lives_list_append(splist, (livespointer)"aspect");
  splist = lives_list_append(splist, (livespointer)"framedraw");
  splist = lives_list_append(splist, (livespointer)"fileread");
  splist = lives_list_append(splist, (livespointer)"filewrite");
  splist = lives_list_append(splist, (livespointer)"fontchooser");
  splist = lives_list_append(splist, (livespointer)"password");
  if (rfxbuilder->type == RFX_BUILD_TYPE_EFFECT2) {
    splist = lives_list_append(splist, (livespointer)"mergealign");
  }

  rfxbuilder->paramw_sp_combo = lives_standard_combo_new(_("Special _Type:         "), splist, LIVES_BOX(dialog_vbox), NULL);
  lives_list_free(splist);

  if (pnum >= 0 && sp) {
    lives_combo_set_active_string(LIVES_COMBO(rfxbuilder->paramw_sp_combo), sp);
  }

  // subtype

  spsublist = lives_list_append(spsublist, (livespointer)"rectdemask");
  spsublist = lives_list_append(spsublist, (livespointer)"multirect");
  spsublist = lives_list_append(spsublist, (livespointer)"singlepoint");
  spsublist = lives_list_append(spsublist, (livespointer)"scaledpoint");

  rfxbuilder->paramw_spsub_combo = lives_standard_combo_new(_("Special _Subtype:         "), spsublist, LIVES_BOX(dialog_vbox),
                                   NULL);
  lives_list_free(spsublist);

  if (pnum >= 0 && spsub) {
    lives_combo_set_active_string(LIVES_COMBO(rfxbuilder->paramw_spsub_combo), spsub);
  }

  // paramwindow rest

  rfxbuilder->paramw_rest_entry = lives_standard_entry_new(_("Row:    "), pnum >= 0 && rest ? rest : NULL,
                                  -1, -1, LIVES_BOX(dialog_vbox), NULL);
  rfxbuilder->paramw_rest_label = widget_opts.last_label;

  if (kw) lives_free(kw);
  if (sp) lives_free(sp);
  if (spsub) lives_free(spsub);
  if (rest) lives_free(rest);

  lives_widget_grab_focus(rfxbuilder->paramw_rest_entry);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->paramw_kw_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_paramw_kw_changed), (livespointer)rfxbuilder);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->paramw_sp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_paramw_sp_changed), (livespointer)rfxbuilder);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rfxbuilder->paramw_spsub_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_paramw_spsub_changed), (livespointer)rfxbuilder);

  lives_widget_show_all(dialog);
  on_paramw_kw_changed(LIVES_COMBO(rfxbuilder->paramw_kw_combo), (livespointer)rfxbuilder);

  return dialog;
}


static void on_paramw_kw_changed(LiVESCombo * combo, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  const char *ctext = lives_combo_get_active_text(combo);

  if (!strcmp(ctext, "special")) {
    lives_widget_show_all(lives_widget_get_parent(rfxbuilder->paramw_sp_combo));
    on_paramw_sp_changed(LIVES_COMBO(rfxbuilder->paramw_sp_combo), (livespointer)rfxbuilder);
    lives_widget_grab_focus(lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_sp_combo)));
  } else {
    lives_label_set_text(LIVES_LABEL(rfxbuilder->paramw_rest_label), (_("Row:    ")));
    lives_widget_hide(lives_widget_get_parent(rfxbuilder->paramw_sp_combo));
    lives_widget_hide(lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    lives_widget_grab_focus(rfxbuilder->paramw_rest_entry);
  }
}


static void paramw_set_splabel(LiVESLabel * label, int npars) {
  char *tmpx;

  if (npars == 1) {
    tmpx = (_("Linked parameter:    "));
  } else {
    char example[256] = "0";
    for (int i = 1; i < npars; i++) {
      char *xnew = lives_strdup_printf("|%d", i);
      lives_strappend(example, 256, xnew);
      lives_free(xnew);
    }
    tmpx = lives_strdup_printf(_("Linked parameters [%d] (example %s) :"), npars, example);
  }
  lives_label_set_text(label, tmpx);
  lives_free(tmpx);
}


static void on_paramw_sp_changed(LiVESCombo * combo, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  int npars;
  const char *ctext = lives_combo_get_active_text(combo);

  if (!strcmp(ctext, "framedraw")) {
    lives_widget_show_all(lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    on_paramw_spsub_changed(LIVES_COMBO(rfxbuilder->paramw_spsub_combo), (livespointer)rfxbuilder);
    lives_widget_grab_focus(lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_spsub_combo)));
  } else {
    if (!strcmp(ctext, "fileread") || !strcmp(ctext, "filewrite") || !strcmp(ctext, "password")) npars = 1;
    else npars = 2;
    paramw_set_splabel(LIVES_LABEL(rfxbuilder->paramw_rest_label), npars);
    lives_widget_hide(lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
  }

  lives_widget_grab_focus(rfxbuilder->paramw_rest_entry);
}


static void on_paramw_spsub_changed(LiVESCombo * combo, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;
  const char *ctext = lives_combo_get_active_text(combo);
  int npars = 0;

  if (!strcmp(ctext, "rectdemask") || !strcmp(ctext, "multirect")) {
    npars = 4;
  } else if (!strcmp(ctext, "singlepoint")) {
    npars = 2;
  } else if (!strcmp(ctext, "scaledpoint")) {
    npars = 3;
  }
  paramw_set_splabel(LIVES_LABEL(rfxbuilder->paramw_rest_label), npars);
}


LiVESWidget *make_trigger_dialog(int tnum, rfx_build_window_t *rfxbuilder) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *combo;
  LiVESWidget *scrolledwindow;

  LiVESList *whenlist = NULL;

  char *title;

  int woat;
  int i;

  if (tnum < 0) {
    title = (_("New RFX Trigger"));
  } else {
    title = (_("Edit RFX Trigger"));
  }

  dialog = lives_standard_dialog_new(title, TRUE, PREF_RFXDIALOG_W, PREF_RFXDIALOG_H);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // when

  if (tnum >= 0) whenlist = lives_list_append(whenlist, (livespointer)(rfxbuilder->copy_triggers[tnum].when ?
                              lives_strdup_printf("%d", rfxbuilder->copy_triggers[tnum].
                                  when - 1) : "init"));
  else {
    if (!rfxbuilder->has_init_trigger) {
      whenlist = lives_list_append(whenlist, (livespointer)"init");
    }
    for (i = 0; i < rfxbuilder->num_params; i++) {
      if (!rfxbuilder->params[i].onchange) {
        whenlist = lives_list_append(whenlist, lives_strdup_printf("%d", i));
      }
    }
  }

  combo = lives_standard_combo_new(_("When:         "), whenlist, LIVES_BOX(dialog_vbox), NULL);
  lives_list_free(whenlist);

  rfxbuilder->trigger_when_entry = lives_combo_get_entry(LIVES_COMBO(combo));

  // code area
  rfxbuilder->code_textview = lives_text_view_new();
  lives_text_view_set_editable(LIVES_TEXT_VIEW(rfxbuilder->code_textview), TRUE);
  //lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(rfxbuilder->code_textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(rfxbuilder->code_textview), TRUE);

  woat = widget_opts.apply_theme;
  widget_opts.apply_theme = 0;
  widget_opts.expand = LIVES_EXPAND_EXTRA; // prevent centering
  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H * 2. / 3., RFX_WINSIZE_V / 4., rfxbuilder->code_textview);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.apply_theme = woat;

  if (palette->style & STYLE_1) {
    lives_widget_set_base_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->white);
    lives_widget_set_text_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    set_css_value_direct(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL,
			 "text", "caret-color", "#000000");

  }

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  if (tnum >= 0) {
    lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview),
                             rfxbuilder->copy_triggers[tnum].code, -1);
  }

  if (tnum >= 0 || rfxbuilder->num_params == 0) lives_widget_grab_focus(rfxbuilder->code_textview);

  lives_widget_show_all(dialog);

  return dialog;
}


static void on_code_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *scrolledwindow;

  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  int woat;

  char *tmpx;

  dialog = lives_standard_dialog_new(NULL, FALSE, PREF_RFXDIALOG_W, PREF_RFXDIALOG_H);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // code area
  rfxbuilder->code_textview = lives_text_view_new();

  woat = widget_opts.apply_theme;
  widget_opts.apply_theme = 0;
  widget_opts.expand = LIVES_EXPAND_EXTRA; // prevent centering
  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, rfxbuilder->code_textview);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.apply_theme = woat;

  if (palette->style & STYLE_1) {
    lives_widget_set_base_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->white);
    lives_widget_set_text_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    set_css_value_direct(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL,
			 "text", "caret-color", "#000000");

  }

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  lives_widget_object_ref(lives_scrolled_window_get_hadjustment(LIVES_SCROLLED_WINDOW(scrolledwindow)));
  lives_widget_object_ref(lives_scrolled_window_get_vadjustment(LIVES_SCROLLED_WINDOW(scrolledwindow)));

  lives_text_view_set_editable(LIVES_TEXT_VIEW(rfxbuilder->code_textview), TRUE);
  lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(rfxbuilder->code_textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(rfxbuilder->code_textview), TRUE);

  lives_widget_grab_focus(rfxbuilder->code_textview);

  // TODO !!
  /*  if (glib_major_version>=2&&glib_minor_version>=4) {
      lives_text_view_set_accepts_tab (LIVES_TEXT_VIEW (rfxbuilder->code_textview),TRUE);
    } */

  if (button == LIVES_BUTTON(rfxbuilder->pre_button)) {
    rfxbuilder->codetype = RFX_CODE_TYPE_PRE;
    lives_window_set_title(LIVES_WINDOW(dialog), _("Pre Loop Code"));
    lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview),
                             rfxbuilder->pre_code, -1);
  }

  else if (button == LIVES_BUTTON(rfxbuilder->loop_button)) {
    rfxbuilder->codetype = RFX_CODE_TYPE_LOOP;
    lives_window_set_title(LIVES_WINDOW(dialog), _("Loop Code"));
    lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview),
                             rfxbuilder->loop_code, -1);
  }

  else if (button == LIVES_BUTTON(rfxbuilder->post_button)) {
    rfxbuilder->codetype = RFX_CODE_TYPE_POST;
    lives_window_set_title(LIVES_WINDOW(dialog), _("Post Loop Code"));
    lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview),
                             rfxbuilder->post_code, -1);
  }

  else if (button == LIVES_BUTTON(rfxbuilder->param_strdef_button)) {
    if (rfxbuilder->copy_params[rfxbuilder->edit_param].type != LIVES_PARAM_STRING_LIST) {
      int len, maxlen = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));

      if ((len = strlen((char *)rfxbuilder->copy_params[rfxbuilder->edit_param].def)) > maxlen) len = maxlen;

      rfxbuilder->codetype = RFX_CODE_TYPE_STRDEF;
      lives_window_set_title(LIVES_WINDOW(dialog), (tmpx = lives_strdup_printf
                             (_("Default text (max length %d)"), maxlen)));
      lives_free(tmpx);
      lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(rfxbuilder->code_textview), LIVES_WRAP_WORD);

      lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview),
                               (char *)rfxbuilder->copy_params[rfxbuilder->edit_param].def, len);
    } else {
      LiVESTextIter start_iter;
      char *string = lives_strdup(""), *string_new;
      int i;

      rfxbuilder->codetype = RFX_CODE_TYPE_STRING_LIST;
      lives_window_set_title(LIVES_WINDOW(dialog), (tmpx = (_("Enter values, one per line"))));
      lives_free(tmpx);
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list) {
        for (i = 0; i < lives_list_length(rfxbuilder->copy_params[rfxbuilder->edit_param].list); i++) {
          string_new = lives_strconcat(string, (char *)lives_list_nth_data(rfxbuilder->copy_params[rfxbuilder->edit_param].list, i),
                                       "\n", NULL);
          if (string != string_new) lives_free(string);
          string = string_new;
        }
        lives_text_view_set_text(LIVES_TEXT_VIEW(rfxbuilder->code_textview), string, strlen(string) - 1);
        lives_free(string);
        lives_text_buffer_get_start_iter(lives_text_view_get_buffer(LIVES_TEXT_VIEW(rfxbuilder->code_textview)), &start_iter);
        lives_text_buffer_place_cursor(lives_text_view_get_buffer(LIVES_TEXT_VIEW(rfxbuilder->code_textview)), &start_iter);
      }
    }
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,  LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL, LIVES_RESPONSE_OK);

  lives_widget_set_can_default(okbutton, TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
			    LIVES_GUI_CALLBACK(on_code_ok), user_data);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);

  lives_widget_show_all(dialog);
}


static void on_rfxbuilder_ok(LiVESButton * button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  if (!perform_rfxbuilder_checks(rfxbuilder)) return;
  if (!rfxbuilder_to_script(rfxbuilder)) return;

  lives_general_button_clicked(button, NULL);
  rfxbuilder_destroy(rfxbuilder);
}


static void on_rfxbuilder_cancel(LiVESButton * button, livespointer user_data) {
  rfx_build_window_t *rfxbuilder = (rfx_build_window_t *)user_data;

  lives_general_button_clicked(button, NULL);
  rfxbuilder_destroy(rfxbuilder);
}


static void rfxbuilder_destroy(rfx_build_window_t *rfxbuilder) {
  int i;

  for (i = 0; i < rfxbuilder->num_reqs; i++) {
    lives_free(rfxbuilder->reqs[i]);
  }
  for (i = 0; i < rfxbuilder->num_params; i++) {
    lives_free(rfxbuilder->params[i].name);
    lives_free(rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type == LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list)lives_list_free(rfxbuilder->params[i].list);
    }
    lives_free(rfxbuilder->params[i].def);
  }
  if (rfxbuilder->num_params) {
    lives_free(rfxbuilder->params);
  }
  for (i = 0; i < rfxbuilder->num_paramw_hints; i++) {
    lives_free(rfxbuilder->paramw_hints[i]);
  }
  for (i = 0; i < rfxbuilder->num_triggers; i++) {
    lives_free(rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    lives_free(rfxbuilder->triggers);
  }

  lives_free(rfxbuilder->pre_code);
  lives_free(rfxbuilder->loop_code);
  lives_free(rfxbuilder->post_code);

  lives_free(rfxbuilder->field_delim);

  if (rfxbuilder->script_name) {
    lives_free(rfxbuilder->script_name);
  }
  if (rfxbuilder->oname) {
    lives_free(rfxbuilder->oname);
  }
  lives_free(rfxbuilder->rfx_version);

  lives_free(rfxbuilder);
}


boolean perform_rfxbuilder_checks(rfx_build_window_t *rfxbuilder) {
  char *name = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->name_entry)));

  if (!strlen(name)) {
    do_error_dialog(_("\n\nName must not be blank.\n"));
    lives_widget_grab_focus(rfxbuilder->name_entry);
    lives_free(name);
    return FALSE;
  }
  if (get_token_count(name, ' ') > 1) {
    do_error_dialog(_("\n\nName must not contain spaces.\n"));
    lives_free(name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->menu_text_entry)))) {
    do_error_dialog(_("\n\nMenu text must not be blank.\n"));
    lives_widget_grab_focus(rfxbuilder->menu_text_entry);
    lives_free(name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->action_desc_entry))) &&
      rfxbuilder->type != RFX_BUILD_TYPE_UTILITY) {
    do_error_dialog(_("\n\nAction description must not be blank.\n"));
    lives_widget_grab_focus(rfxbuilder->action_desc_entry);
    lives_free(name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->author_entry)))) {
    do_error_dialog(_("\n\nAuthor must not be blank.\n"));
    lives_widget_grab_focus(rfxbuilder->author_entry);
    lives_free(name);
    return FALSE;
  }

  if (rfxbuilder->mode != RFX_BUILDER_MODE_NEW && !(rfxbuilder->mode == RFX_BUILDER_MODE_EDIT &&
      !strcmp(rfxbuilder->oname, name))) {
    if (find_rfx_plugin_by_name(name, RFX_STATUS_TEST) > -1 || find_rfx_plugin_by_name
        (name, RFX_STATUS_CUSTOM) > -1 || find_rfx_plugin_by_name(name, RFX_STATUS_BUILTIN) > -1) {
      do_error_dialog(_("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
      lives_free(name);
      return FALSE;
    }
  }

  if (!strlen(rfxbuilder->loop_code) && rfxbuilder->type != RFX_BUILD_TYPE_UTILITY) {
    do_error_dialog(_("\n\nLoop code should not be blank.\n"));
    lives_free(name);
    return FALSE;
  }

  if (rfxbuilder->num_triggers == 0 && rfxbuilder->type == RFX_BUILD_TYPE_UTILITY) {
    do_error_dialog(_("\n\nTrigger code should not be blank for a utility.\n"));
    lives_free(name);
    return FALSE;
  }

  lives_free(name);
  return TRUE;
}


boolean perform_param_checks(rfx_build_window_t *rfxbuilder, int index, int rows) {
  int i;

  if (!strlen(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->param_name_entry)))) {
    do_error_dialog(_("\n\nParameter name must not be blank.\n"));
    lives_widget_grab_focus(rfxbuilder->param_name_entry);
    return FALSE;
  }
  for (i = 0; i < rows; i++) {
    if (i != index && !(strcmp(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->param_name_entry)),
                               rfxbuilder->copy_params[i].name))) {
      do_error_dialog(_("\n\nDuplicate parameter name detected. Parameter names must be unique in a plugin.\n\n"));
      return FALSE;
    }
  }
  return TRUE;
}


boolean rfxbuilder_to_script(rfx_build_window_t *rfxbuilder) {
  FILE *sfile;
  lives_colRGB48_t rgb;
  char **array;

  double stepwrap;

  uint32_t props;

  const char *name = lives_entry_get_text(LIVES_ENTRY(rfxbuilder->name_entry));

  char *script_file, *script_file_dir;
  char *script_name = lives_strdup_printf("%s.%s", name, RFXBUILDER_SCRIPT_SUFFIX);
  char *new_name;
  char *buf, *tmp, *tmp2;
  const char *ctext;

  int retval, i, j;

  if (rfxbuilder->mode != RFX_BUILDER_MODE_EDIT) {
    if (!(new_name = prompt_for_script_name(script_name, RFX_STATUS_TEST))) {
      lives_free(script_name);
      return FALSE;
    }
    lives_free(script_name);
    script_name = new_name;
  }

  script_file_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, NULL);

  if (!lives_file_test(script_file_dir, LIVES_FILE_TEST_IS_DIR)) {
    if (lives_mkdir_with_parents(script_file_dir, capable->umask) == -1) {
      lives_free(script_name);
      return FALSE;
    }
  }

  script_file = lives_strdup_printf("%s%s", script_file_dir, script_name);
  lives_free(script_file_dir);
  lives_free(script_name);

  d_print(_("Writing script file %s..."), script_file);

  if (!check_file(script_file, TRUE)) {
    lives_free(script_file);
    d_print_failed();
    return FALSE;
  }

  do {
    retval = 0;
    if (!(sfile = fopen(script_file, "w"))) {
      widget_opts.transient = LIVES_WINDOW(rfxbuilder->dialog);
      retval = do_write_failed_error_s_with_retry(script_file, lives_strerror(errno));
      widget_opts.transient = NULL;

      if (retval == LIVES_RESPONSE_CANCEL) {
        lives_free(script_file);
        d_print_failed();
        return FALSE;
      }
    } else {
      THREADVAR(write_failed) = FALSE;

      lives_fputs("Script file generated from LiVES\n\n", sfile);
      lives_fputs("<define>\n", sfile);
      lives_fputs(rfxbuilder->field_delim, sfile);
      lives_fputs(rfxbuilder->rfx_version, sfile);
      lives_fputs("\n</define>\n\n", sfile);
      lives_fputs("<name>\n", sfile);
      lives_fputs(name, sfile);
      lives_fputs("\n</name>\n\n", sfile);
      lives_fputs("<version>\n", sfile);
      buf = lives_strdup_printf("%d", lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_version)));
      lives_fputs(buf, sfile);
      lives_free(buf);
      lives_fputs("\n</version>\n\n", sfile);
      lives_fputs("<author>\n", sfile);
      lives_fputs(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->author_entry)), sfile);
      lives_fputs(rfxbuilder->field_delim, sfile);
      lives_fputs(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->url_entry)), sfile);
      lives_fputs("\n</author>\n\n", sfile);
      lives_fputs("<description>\n", sfile);
      lives_fputs(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->menu_text_entry)), sfile);
      lives_fputs(rfxbuilder->field_delim, sfile);
      lives_fputs(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->action_desc_entry)), sfile);
      lives_fputs(rfxbuilder->field_delim, sfile);
      if (rfxbuilder->type == RFX_BUILD_TYPE_UTILITY) {
        buf = lives_strdup("-1");
      } else {
        buf = lives_strdup_printf("%d", lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_min_frames)));
      }
      lives_fputs(buf, sfile);
      lives_free(buf);
      lives_fputs(rfxbuilder->field_delim, sfile);
      switch (rfxbuilder->type) {
      case RFX_BUILD_TYPE_EFFECT2:
        buf = lives_strdup("2");
        break;
      case RFX_BUILD_TYPE_EFFECT0:
        buf = lives_strdup("0");
        break;
      default:
        buf = lives_strdup("1");
      }
      lives_fputs(buf, sfile);
      lives_free(buf);
      lives_fputs(rfxbuilder->field_delim, sfile);
      lives_fputs("\n</description>\n\n", sfile);
      lives_fputs("<requires>\n", sfile);
      for (i = 0; i < rfxbuilder->num_reqs; i++) {
        lives_fputs(rfxbuilder->reqs[i], sfile);
        if (i < rfxbuilder->num_reqs - 1)
          lives_fputs("\n", sfile);
      }
      lives_fputs("\n</requires>\n\n", sfile);
      lives_fputs("<params>\n", sfile);
      for (i = 0; i < rfxbuilder->num_params; i++) {
        lives_fputs(rfxbuilder->params[i].name, sfile);
        lives_fputs(rfxbuilder->field_delim, sfile);
        lives_fputs(rfxbuilder->params[i].label, sfile);
        lives_fputs(rfxbuilder->field_delim, sfile);
        switch (rfxbuilder->params[i].type) {
        case LIVES_PARAM_NUM:
          stepwrap = rfxbuilder->params[i].step_size;
          if (rfxbuilder->params[i].wrap) stepwrap = -stepwrap;
          lives_fputs("num", sfile);
          buf = lives_strdup_printf("%d", rfxbuilder->params[i].dp);
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          if (!rfxbuilder->params[i].dp) {
            buf = lives_strdup_printf("%d", get_int_param(rfxbuilder->params[i].def));
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            buf = lives_strdup_printf("%d", (int)rfxbuilder->params[i].min);
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            buf = lives_strdup_printf("%d", (int)rfxbuilder->params[i].max);
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            if (stepwrap != 1.) {
              buf = lives_strdup_printf("%d", (int)stepwrap);
              lives_fputs(buf, sfile);
              lives_free(buf);
              lives_fputs(rfxbuilder->field_delim, sfile);
            }
          } else {
            char *pattern = lives_strdup_printf("%%.%df", rfxbuilder->params[i].dp);
            buf = lives_strdup_printf(pattern, get_double_param(rfxbuilder->params[i].def));
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            buf = lives_strdup_printf(pattern, rfxbuilder->params[i].min);
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            buf = lives_strdup_printf(pattern, rfxbuilder->params[i].max);
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            if (stepwrap != 1.) {
              buf = lives_strdup_printf(pattern, stepwrap);
              lives_fputs(buf, sfile);
              lives_free(buf);
              lives_fputs(rfxbuilder->field_delim, sfile);
            }
            lives_free(pattern);
          }
          break;
        case LIVES_PARAM_BOOL:
          lives_fputs("bool", sfile);
          lives_fputs(rfxbuilder->field_delim, sfile);
          buf = lives_strdup_printf("%d", get_bool_param(rfxbuilder->params[i].def));
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          if (rfxbuilder->params[i].group != 0) {
            buf = lives_strdup_printf("%d", rfxbuilder->params[i].group);
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
          }
          break;
        case LIVES_PARAM_COLRGB24:
          lives_fputs("colRGB24", sfile);
          lives_fputs(rfxbuilder->field_delim, sfile);
          get_colRGB24_param(rfxbuilder->params[i].def, &rgb);
          buf = lives_strdup_printf("%d", rgb.red);
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          buf = lives_strdup_printf("%d", rgb.green);
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          buf = lives_strdup_printf("%d", rgb.blue);
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          break;
        case LIVES_PARAM_STRING:
          lives_fputs("string", sfile);
          lives_fputs(rfxbuilder->field_delim, sfile);
          lives_fputs((tmp = U82L(tmp2 = subst((char *)rfxbuilder->params[i].def, "\n", "\\n"))), sfile);
          lives_free(tmp);
          lives_free(tmp2);
          lives_fputs(rfxbuilder->field_delim, sfile);
          buf = lives_strdup_printf("%d", (int)rfxbuilder->params[i].max);
          lives_fputs(buf, sfile);
          lives_free(buf);
          lives_fputs(rfxbuilder->field_delim, sfile);
          break;
        case LIVES_PARAM_STRING_LIST:
          lives_fputs("string_list", sfile);
          lives_fputs(rfxbuilder->field_delim, sfile);
          if (rfxbuilder->params[i].def) {
            buf = lives_strdup_printf("%d", get_bool_param(rfxbuilder->params[i].def));
            lives_fputs(buf, sfile);
            lives_free(buf);
            lives_fputs(rfxbuilder->field_delim, sfile);
            for (j = 0; j < lives_list_length(rfxbuilder->params[i].list); j++) {
              lives_fputs((tmp = U82L(tmp2 = subst((char *)lives_list_nth_data
                                                   (rfxbuilder->params[i].list, j), "\n", "\\n"))), sfile);
              lives_free(tmp);
              lives_free(tmp2);
              lives_fputs(rfxbuilder->field_delim, sfile);
            }
          }
          break;
        default:
          break;
        }
        if (i < rfxbuilder->num_params - 1)
          lives_fputs("\n", sfile);
      }
      lives_fputs("\n</params>\n\n", sfile);
      lives_fputs("<param_window>\n", sfile);
      for (i = 0; i < rfxbuilder->num_paramw_hints; i++) {
        lives_fputs(rfxbuilder->paramw_hints[i], sfile);
        if (strlen(rfxbuilder->paramw_hints[i]) > strlen(rfxbuilder->field_delim) &&
            strcmp(rfxbuilder->paramw_hints[i] + strlen(rfxbuilder->paramw_hints[i])
                   - strlen(rfxbuilder->field_delim), rfxbuilder->field_delim))
          lives_fputs(rfxbuilder->field_delim, sfile);
        lives_fputs("\n", sfile);
      }
      lives_fputs("</param_window>\n\n", sfile);
      lives_fputs("<properties>\n", sfile);
      if (rfxbuilder->type == RFX_BUILD_TYPE_TOOL) props = rfxbuilder->props | RFX_PROPS_MAY_RESIZE;
      else props = rfxbuilder->props;

      if (rfxbuilder->type != RFX_BUILD_TYPE_EFFECT0 && (rfxbuilder->props & RFX_PROPS_BATCHG))
        rfxbuilder->props ^= RFX_PROPS_BATCHG;

      buf = lives_strdup_printf("0x%04X", props);
      lives_fputs(buf, sfile);
      lives_free(buf);
      lives_fputs("\n</properties>\n\n", sfile);
      lives_fputs("<language_code>\n", sfile);
      ctext = lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->langc_combo));
      array = lives_strsplit(ctext, " ", -1);
      lives_fputs(array[0], sfile);
      lives_strfreev(array);
      lives_fputs("\n</language_code>\n\n", sfile);
      lives_fputs("<pre>\n", sfile);
      lives_fputs(rfxbuilder->pre_code, sfile);
      if (strlen(rfxbuilder->pre_code) && strcmp(rfxbuilder->pre_code + strlen(rfxbuilder->pre_code) - 1, "\n"))
        lives_fputs("\n", sfile);
      lives_fputs("</pre>\n\n", sfile);
      lives_fputs("<loop>\n", sfile);
      lives_fputs(rfxbuilder->loop_code, sfile);
      if (strlen(rfxbuilder->loop_code) && strcmp(rfxbuilder->loop_code + strlen(rfxbuilder->loop_code) - 1, "\n"))
        lives_fputs("\n", sfile);
      lives_fputs("</loop>\n\n", sfile);
      lives_fputs("<post>\n", sfile);
      lives_fputs(rfxbuilder->post_code, sfile);
      if (strlen(rfxbuilder->post_code) && strcmp(rfxbuilder->post_code + strlen(rfxbuilder->post_code) - 1, "\n"))
        lives_fputs("\n", sfile);
      lives_fputs("</post>\n\n", sfile);
      lives_fputs("<onchange>\n", sfile);
      for (i = 0; i < rfxbuilder->num_triggers; i++) {
        int numtok = get_token_count(rfxbuilder->triggers[i].code, '\n');
        buf = rfxbuilder->triggers[i].when ? lives_strdup_printf("%d", rfxbuilder->triggers[i].when - 1) : lives_strdup("init");
        array = lives_strsplit(rfxbuilder->triggers[i].code, "\n", numtok);
        for (int j = 0; j < numtok; j++) {
          lives_fputs(buf, sfile);
          lives_fputs(rfxbuilder->field_delim, sfile);
          if (array[j]) lives_fputs(array[j], sfile);
          if (j < numtok - 1)
            lives_fputs("\n", sfile);
        }
        lives_fputs("\n", sfile);
        lives_free(buf);
        lives_strfreev(array);
      }
      lives_fputs("</onchange>\n\n", sfile);
      fclose(sfile);

      if (THREADVAR(write_failed)) {
        THREADVAR(write_failed) = FALSE;
        widget_opts.transient = LIVES_WINDOW(rfxbuilder->dialog);
        retval = do_write_failed_error_s_with_retry(script_file, NULL);
        widget_opts.transient = NULL;
        if (retval == LIVES_RESPONSE_CANCEL) d_print_file_error_failed();
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(script_file);

  if (retval != LIVES_RESPONSE_CANCEL) {
    d_print_done();

    lives_widget_set_sensitive(mainw->promote_test_rfx, TRUE);
    lives_widget_set_sensitive(mainw->delete_test_rfx, TRUE);
    lives_widget_set_sensitive(mainw->rename_test_rfx, TRUE);
    lives_widget_set_sensitive(mainw->edit_test_rfx, TRUE);

    return TRUE;
  }
  return FALSE;
}


boolean script_to_rfxbuilder(rfx_build_window_t *rfxbuilder, const char *script_file) {
  LiVESList *list;

  char **array;
  char *tmp;
  char *line;
  char *version;

  int listlen;
  int num_channels;
  int tnum, found;
  int filled_triggers = 0;
  int len;

  int i, j;

  rfxbuilder->type = RFX_BUILD_TYPE_EFFECT1;
  if (!(list = get_script_section("define", script_file, TRUE))) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("No <define> section found in script.\n")));
    return FALSE;
  }
  tmp = (char *)lives_list_nth_data(list, 0);
  if (strlen(tmp) < 2) {
    lives_list_free_all(&list);
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("Bad script version.\n")));
    return FALSE;
  }

  version = lives_strdup(tmp + 1);
  if (make_version_hash(version) > make_version_hash(RFX_VERSION)) {
    lives_list_free_all(&list);
    lives_free(version);
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("Bad script version.\n")));
    return FALSE;
  }
  lives_free(version);

  lives_memset(tmp + 1, 0, 1);
  lives_free(rfxbuilder->field_delim);
  rfxbuilder->field_delim = lives_strdup(tmp);
  lives_list_free_all(&list);

  if (!(list = get_script_section("name", script_file, TRUE))) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("No <name> section found in script.\n")));
    return FALSE;
  }
  lives_entry_set_text(LIVES_ENTRY(rfxbuilder->name_entry), (char *)lives_list_nth_data(list, 0));
  lives_list_free_all(&list);

  if (!(list = get_script_section("version", script_file, TRUE))) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("No <version> section found in script.\n")));
    return FALSE;
  }
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_version),
                              (double)atoi((char *)lives_list_nth_data(list, 0)));
  lives_list_free_all(&list);

  if (!(list = get_script_section("author", script_file, TRUE))) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("No <author> section found in script.\n")));
    return FALSE;
  }
  array = lives_strsplit((char *)lives_list_nth_data(list, 0), rfxbuilder->field_delim, 2);
  lives_entry_set_text(LIVES_ENTRY(rfxbuilder->author_entry), array[0]);
  if (get_token_count((char *)lives_list_nth_data(list, 0), (int)rfxbuilder->field_delim[0]) > 1) {
    lives_entry_set_text(LIVES_ENTRY(rfxbuilder->url_entry), array[1]);
  }
  lives_strfreev(array);
  lives_list_free_all(&list);

  if (!(list = get_script_section("description", script_file, TRUE))) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", (_("No <description> section found in script.\n")));
    return FALSE;
  }
  if (get_token_count((char *)lives_list_nth_data(list, 0), (int)rfxbuilder->field_delim[0]) < 4) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, (_("Bad description. (%s)\n")), (char *)lives_list_nth_data(list, 0));
    lives_list_free_all(&list);
    return FALSE;
  }
  array = lives_strsplit((char *)lives_list_nth_data(list, 0), rfxbuilder->field_delim, -1);
  lives_list_free_all(&list);
  lives_entry_set_text(LIVES_ENTRY(rfxbuilder->menu_text_entry), array[0]);
  lives_entry_set_text(LIVES_ENTRY(rfxbuilder->action_desc_entry), array[1]);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_min_frames), (double)atoi(array[2]));
  num_channels = atoi(array[3]);

  if (num_channels == 2) {
    rfxbuilder->type = RFX_BUILD_TYPE_EFFECT2;
  }
  if (num_channels == 0) {
    rfxbuilder->type = RFX_BUILD_TYPE_EFFECT0;
  }
  if (atoi(array[2]) == -1) rfxbuilder->type = RFX_BUILD_TYPE_UTILITY;
  lives_strfreev(array);

  rfxbuilder->script_name = lives_strdup(script_file);

  if ((list = get_script_section("requires", script_file, TRUE))) {
    rfxbuilder->num_reqs = lives_list_length(list);
    for (i = 0; i < rfxbuilder->num_reqs; i++) {
      rfxbuilder->reqs[i] = (char *)lives_list_nth_data(list, i);
    }
    lives_list_free(list);
  }

  rfxbuilder->props = 0;

  if ((list = get_script_section("properties", script_file, TRUE))) {
    if (!strncmp((char *)lives_list_nth_data(list, 0), "0x", 2) || !strncmp((char *)lives_list_nth_data(list, 0), "0x", 2)) {
      rfxbuilder->props = hextodec((char *)lives_list_nth_data(list, 0) + 2);
    } else rfxbuilder->props = atoi((char *)lives_list_nth_data(list, 0));
    lives_list_free_all(&list);
  }

  if (rfxbuilder->props & RFX_PROPS_MAY_RESIZE) rfxbuilder->type = RFX_BUILD_TYPE_TOOL;

  if ((list = get_script_section("params", script_file, TRUE))) {
    char *type;

    listlen = lives_list_length(list);
    rfxbuilder->num_params = listlen;
    rfxbuilder->params = (lives_param_t *)lives_malloc(rfxbuilder->num_params * sizeof(lives_param_t));
    i = 0;

    for (j = 0; j < listlen; j++) {
      line = (char *)lives_list_nth_data(list, j);
      len = get_token_count(line, (int)rfxbuilder->field_delim[0]); // will fail if field_delim contains repeated chars !
      if (len < 4) {
        rfxbuilder->num_params--;
        rfxbuilder->params = (lives_param_t *)lives_realloc(rfxbuilder->params, rfxbuilder->num_params * sizeof(lives_param_t));
        continue;
      }
      array = lives_strsplit(line, rfxbuilder->field_delim, -1);
      lives_free(line);
      rfxbuilder->params[i].name = lives_strdup(array[0]);
      rfxbuilder->params[i].label = lives_strdup(array[1]);
      rfxbuilder->params[i].onchange = FALSE;
      rfxbuilder->params[i].dp = 0;
      rfxbuilder->params[i].group = 0;

      ////////////////
      rfxbuilder->params[i].desc = NULL;
      rfxbuilder->params[i].use_mnemonic = TRUE;
      rfxbuilder->params[i].step_size = 1.;
      rfxbuilder->params[i].hidden = FALSE;
      rfxbuilder->params[i].transition = FALSE;
      rfxbuilder->params[i].wrap = FALSE;
      ////////////////

      type = lives_strdup(array[2]);
      if (!strncmp(type, "num", 3)) {
        if (len < 6) {
          lives_free(rfxbuilder->params[i].name);
          lives_free(rfxbuilder->params[i].label);
          rfxbuilder->num_params--;
          rfxbuilder->params = (lives_param_t *)lives_realloc(rfxbuilder->params, rfxbuilder->num_params * sizeof(lives_param_t));
          continue;
        }
        rfxbuilder->params[i].dp = atoi(type + 3);
        rfxbuilder->params[i].type = LIVES_PARAM_NUM;
        if (rfxbuilder->params[i].dp) {
          rfxbuilder->params[i].def = lives_malloc(sizint);
          set_double_param(rfxbuilder->params[i].def, lives_strtod(array[3]));
        } else {
          rfxbuilder->params[i].def = lives_malloc(sizint);
          set_int_param(rfxbuilder->params[i].def, atoi(array[3]));
        }
        rfxbuilder->params[i].min = lives_strtod(array[4]);
        rfxbuilder->params[i].max = lives_strtod(array[5]);
        if (len > 6) {
          rfxbuilder->params[i].step_size = lives_strtod(array[6]);
          if (rfxbuilder->params[i].step_size == 0.) rfxbuilder->params[i].step_size = 1.;
          else if (rfxbuilder->params[i].step_size < 0.) {
            rfxbuilder->params[i].step_size = -rfxbuilder->params[i].step_size;
            rfxbuilder->params[i].wrap = TRUE;
          }
        }
      } else if (!strcmp(type, "colRGB24")) {
        if (len < 6) {
          lives_free(rfxbuilder->params[i].name);
          lives_free(rfxbuilder->params[i].label);
          rfxbuilder->num_params--;
          rfxbuilder->params = (lives_param_t *)lives_realloc(rfxbuilder->params, rfxbuilder->num_params * sizeof(lives_param_t));
          continue;
        }
        rfxbuilder->params[i].type = LIVES_PARAM_COLRGB24;
        rfxbuilder->params[i].def = lives_malloc(3 * sizint);
        set_colRGB24_param(rfxbuilder->params[i].def, (short)atoi(array[3]),
                           (short)atoi(array[4]), (short)atoi(array[5]));
      } else if (!strcmp(type, "string")) {
        rfxbuilder->params[i].type = LIVES_PARAM_STRING;
        rfxbuilder->params[i].def = subst((tmp = L2U8(array[3])), "\\n", "\n");
        lives_free(tmp);
        if (len > 4) rfxbuilder->params[i].max = (double)atoi(array[4]);
        else rfxbuilder->params[i].max = 1024; // TODO
      } else if (!strcmp(type, "string_list")) {
        rfxbuilder->params[i].type = LIVES_PARAM_STRING_LIST;
        rfxbuilder->params[i].def = lives_malloc(sizint);
        set_int_param(rfxbuilder->params[i].def, atoi(array[3]));
        if (len > 4) {
          rfxbuilder->params[i].list = array_to_string_list((const char **)array, 3, len);
        } else {
          rfxbuilder->params[i].list = NULL;
          set_int_param(rfxbuilder->params[i].def, 0);
        }
      } else {
        // default is bool
        rfxbuilder->params[i].type = LIVES_PARAM_BOOL;
        rfxbuilder->params[i].def = lives_malloc(sizint);
        set_bool_param(rfxbuilder->params[i].def, atoi(array[3]));
        if (len > 4) rfxbuilder->params[i].group = atoi(array[4]);
      }
      lives_free(type);
      lives_strfreev(array);
      i++;
    }
    lives_list_free(list);
  }

  if ((list = get_script_section("param_window", script_file, TRUE))) {
    rfxbuilder->num_paramw_hints = lives_list_length(list);
    for (i = 0; i < rfxbuilder->num_paramw_hints; i++) {
      rfxbuilder->paramw_hints[i] = (char *)lives_list_nth_data(list, i);
    }
    lives_list_free(list);
  }

  if ((list = get_script_section("onchange", script_file, TRUE))) {
    listlen = lives_list_length(list);
    for (i = 0; i < listlen; i++) {
      array = lives_strsplit((char *)lives_list_nth_data(list, i), rfxbuilder->field_delim, -1);
      if (!strcmp(array[0], "init")) {
        if (!rfxbuilder->has_init_trigger) {
          rfxbuilder->has_init_trigger = TRUE;
          rfxbuilder->num_triggers++;
        }
      } else if ((tnum = atoi(array[0]) + 1) <= rfxbuilder->num_params && tnum > 0) {
        if (!rfxbuilder->params[tnum - 1].onchange) {
          rfxbuilder->params[tnum - 1].onchange = TRUE;
          rfxbuilder->num_triggers++;
        }
      } else {
        //invalid trigger
        char *msg = lives_strdup_printf(_("\n\nInvalid trigger (%s)\nfound in script.\n\n"), array[0]);
        widget_opts.non_modal = TRUE;
        do_error_dialog(msg);
        widget_opts.non_modal = FALSE;
        lives_free(msg);
      }
      lives_strfreev(array);
    }
    //end pass 1
    rfxbuilder->triggers = (rfx_trigger_t *)lives_malloc(rfxbuilder->num_triggers * sizeof(rfx_trigger_t));

    for (i = 0; i < rfxbuilder->num_triggers; i++) {
      rfxbuilder->triggers[i].when = -1;
      rfxbuilder->triggers[i].code = lives_strdup("");
    }

    filled_triggers = 0;
    listlen = lives_list_length(list);
    for (i = 0; i < listlen; i++) {
      array = lives_strsplit((char *)lives_list_nth_data(list, i), rfxbuilder->field_delim, -1);
      if (!strcmp(array[0], "init")) {
        // find init trigger and concatenate code
        found = filled_triggers;
        for (j = 0; j < filled_triggers && found == filled_triggers; j++) if (rfxbuilder->triggers[j].when == 0) found = j;
        if (found == filled_triggers) filled_triggers++;
        if (!strlen(rfxbuilder->triggers[found].code)) {
          tmp = lives_strconcat(rfxbuilder->triggers[found].code, array[1], NULL);
        } else {
          tmp = lives_strconcat(rfxbuilder->triggers[found].code, "\n", array[1], NULL);
        }
        lives_free(rfxbuilder->triggers[found].code);
        rfxbuilder->triggers[found].when = 0;
        rfxbuilder->triggers[found].code = lives_strdup(tmp);
        lives_free(tmp);
      } else if ((tnum = atoi(array[0]) + 1) <= rfxbuilder->num_params && tnum > 0) {
        // find tnum trigger and concatenate code
        found = filled_triggers;

        for (j = 0; j < filled_triggers && found == filled_triggers; j++) if (rfxbuilder->triggers[j].when == tnum) found = j;
        if (found == filled_triggers) filled_triggers++;

        if (!strlen(rfxbuilder->triggers[found].code)) {
          tmp = lives_strconcat(rfxbuilder->triggers[found].code, array[1], NULL);
        } else {
          tmp = lives_strconcat(rfxbuilder->triggers[found].code, "\n", array[1], NULL);
        }
        lives_free(rfxbuilder->triggers[found].code);
        rfxbuilder->triggers[found].when = tnum;
        rfxbuilder->triggers[found].code = lives_strdup(tmp);
        lives_free(tmp);
      }
      lives_strfreev(array);
    }
    lives_list_free_all(&list);
  }

  if ((list = get_script_section("pre", script_file, FALSE))) {
    listlen = lives_list_length(list);
    for (i = 0; i < listlen; i++) {
      tmp = lives_strconcat(rfxbuilder->pre_code, lives_list_nth_data(list, i), NULL);
      lives_free(rfxbuilder->pre_code);
      rfxbuilder->pre_code = tmp;
    }
    lives_list_free_all(&list);
  }

  if ((list = get_script_section("loop", script_file, FALSE))) {
    listlen = lives_list_length(list);
    for (i = 0; i < listlen; i++) {
      tmp = lives_strconcat(rfxbuilder->loop_code, lives_list_nth_data(list, i), NULL);
      lives_free(rfxbuilder->loop_code);
      rfxbuilder->loop_code = tmp;
    }
    lives_list_free_all(&list);
  }

  if ((list = get_script_section("post", script_file, FALSE))) {
    listlen = lives_list_length(list);
    for (i = 0; i < listlen; i++) {
      tmp = lives_strconcat(rfxbuilder->post_code, lives_list_nth_data(list, i), NULL);
      lives_free(rfxbuilder->post_code);
      rfxbuilder->post_code = tmp;
    }
    lives_list_free_all(&list);
  }

  return TRUE;
}


LiVESList *get_script_section(const char *section, const char *file, boolean strip) {
  char *com = lives_strdup_printf("\"%s\" -get \"%s\" \"%s\"", EXEC_RFX_BUILDER, section, file);
  LiVESList *list = get_plugin_result(com, "\n", FALSE, strip);
  lives_free(com);
  return list;
}


/** @brief parse text and return non empty lines between <section> and </section>

    similar to get_val_from_cached_list, except here we parse each line into a list.
    rfx + params can be built from the <params> section
*/
/* LiVESList *get_script_section_from_text(const char *section, const char *text, boolean strip) { */
/*   LiVESList *list; */
/*   // TODO */
/*   return list; */
/* } */


void on_rebuild_rfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *com, *rfx_script_dir, *rfx_exec_dir;
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);

  if (!check_builder_programs()) return;

  do_threaded_dialog(_("Rebuilding RFX scripts"), FALSE);

  switch (status) {
  case RFX_STATUS_CUSTOM:
    d_print(_("Rebuilding custom RFX scripts..."));
    rfx_exec_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_CUSTOM, NULL);
    rfx_script_dir = lives_build_path(prefs->config_datadir,
				      PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, NULL);
    com = lives_strdup_printf("%s build_rfx_plugins custom \"%s\" \"%s\"", prefs->backend_sync,
			      rfx_script_dir, rfx_exec_dir);
    lives_system(com, FALSE);
    lives_free(com);
    break;
  case RFX_STATUS_TEST:
    d_print(_("Rebuilding test RFX scripts..."));
    rfx_exec_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST, NULL);
    rfx_script_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, NULL);
    com = lives_strdup_printf("%s build_rfx_plugins test \"%s\" \"%s\"", prefs->backend_sync,
			      rfx_script_dir, rfx_exec_dir);
    lives_system(com, FALSE);
    lives_free(com);
    break;
  default:
    return;
  }

  lives_free(rfx_script_dir); lives_free(rfx_exec_dir);

  d_print(_("recreating dynamic menu entries..."));
  lives_widget_context_update();
  threaded_dialog_spin(0.);
  add_rfx_effects(status);
  threaded_dialog_spin(0.);
  d_print_done();
  end_threaded_dialog();
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_context_update();
}


boolean check_builder_programs(void) {
#ifdef IS_MINGW
  return TRUE;
#endif

  // check our plugin builder routines are executable
  if (check_for_executable(&capable->has_rfx_builder, EXEC_RFX_BUILDER) != PRESENT) {
    do_exec_missing_error(EXEC_RFX_BUILDER);
    return FALSE;
  }
  if (check_for_executable(&capable->has_rfx_builder_multi, EXEC_RFX_BUILDER_MULTI) != PRESENT) {
    do_exec_missing_error(EXEC_RFX_BUILDER_MULTI);
    return FALSE;
  }
  return TRUE;
}


void on_delete_rfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);
  int ret;
  char *rfx_script_file, *rfx_exec_dir, *rfx_script_dir;
  char *script_name = prompt_for_script_name(NULL, status);
  char *msg;

  if (!script_name) return; // user cancelled

  if (strlen(script_name)) {
    switch (status) {
    case RFX_STATUS_TEST:
      rfx_exec_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST, NULL);
      rfx_script_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, NULL);
      rfx_script_file = lives_build_filename(rfx_script_dir, script_name, NULL);
      break;
    case RFX_STATUS_CUSTOM:
      rfx_exec_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_CUSTOM, NULL);
      rfx_script_dir = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, NULL);
      rfx_script_file = lives_build_filename(rfx_script_dir, script_name, NULL);
      break;
    default:
      // we will not delete builtins
      lives_free(script_name);
      return;
    }
    lives_free(script_name);

    // double check with user
    msg = lives_strdup_printf(_("\n\nReally delete RFX script\n%s ?\n\n"), rfx_script_file);
    if (!do_warning_dialog(msg)) {
      lives_free(msg);
      lives_free(rfx_script_file);
      lives_free(rfx_script_dir);
      lives_free(rfx_exec_dir);
      return;
    }
    lives_free(msg);

    d_print(_("Deleting rfx script %s..."), rfx_script_file);

    if (!(ret = lives_rm(rfx_script_file))) {
      lives_rmdir(rfx_exec_dir, TRUE);
      d_print_done();
      on_rebuild_rfx_activate(NULL, LIVES_INT_TO_POINTER(status));
    } else {
      d_print_failed();
      msg = lives_strdup_printf(_("\n\nFailed to delete the script\n%s\nError code was %d\n"), rfx_script_file, ret);
      widget_opts.non_modal = TRUE;
      do_error_dialog(msg);
      widget_opts.non_modal = FALSE;
      lives_free(msg);
    }
    lives_free(rfx_script_file);
    lives_free(rfx_script_dir);
    lives_free(rfx_exec_dir);
  }
}


void on_promote_rfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *rfx_script_from = NULL;
  char *rfx_script_to = NULL;
  char *rfx_dir_from = NULL;
  char *rfx_dir_to = NULL;
  char *script_name = prompt_for_script_name(NULL, RFX_STATUS_TEST);
  char *msg;
  int ret = 0;
  boolean failed = TRUE;

  if (!script_name) return; // user cancelled

  if (*script_name) {
    rfx_dir_from = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, NULL);
    rfx_script_from = lives_build_filename(rfx_dir_from, script_name, NULL);

    rfx_dir_to = lives_build_path(prefs->config_datadir,  PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, NULL);

    rfx_script_to = lives_build_filename(rfx_dir_to, script_name, NULL);

    if (lives_file_test(rfx_script_to, LIVES_FILE_TEST_EXISTS)) {
      char *msg = lives_strdup_printf(
                    _("\nCustom script file:\n%s\nalready exists.\n"
		      "Please delete it first, or rename the test script.\n"),
                    script_name);
      do_error_dialog(msg);
      lives_free(msg); lives_free(rfx_dir_from); lives_free(rfx_script_from);
      lives_free(rfx_dir_to); lives_free(rfx_script_to); lives_free(script_name);
      return;
    }

    d_print(_("Promoting rfx test plugin %s to custom..."), script_name);
    lives_free(script_name);

    lives_mkdir_with_parents(rfx_dir_to, capable->umask);

    if (!(ret = rename(rfx_script_from, rfx_script_to))) {
      d_print_done();
      // TODO - use RFX_STATUS_CUSTOM_AND_TEST
      on_rebuild_rfx_activate(NULL, LIVES_INT_TO_POINTER(RFX_STATUS_CUSTOM));
      on_rebuild_rfx_activate(NULL, LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
      failed = FALSE;
    }
  }

  if (failed) {
    lives_rmdir(rfx_dir_to, FALSE);
    d_print_failed();
    msg = lives_strdup_printf(_("\n\nFailed to move the plugin script from\n%s to\n"
				"%s\nReturn code was %d (%s)\n"),
                              rfx_script_from, rfx_script_to, errno, strerror(errno));
    widget_opts.non_modal = TRUE;
    do_error_dialog(msg);
    widget_opts.non_modal = FALSE;
    lives_free(msg);
  } else lives_rmdir(rfx_dir_from, FALSE);

  if (rfx_script_from) {
    lives_free(rfx_dir_from);
    lives_free(rfx_script_from);
    lives_free(rfx_dir_to);
    lives_free(rfx_script_to);
  }
}


void on_export_rfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);

  char *script_name = prompt_for_script_name(NULL, status);
  char *rfx_script_from, *filename;
  char *tmp, *tmp2;

  if (!script_name || !*script_name) return; // user cancelled

  filename = choose_file(NULL, script_name, NULL, LIVES_FILE_CHOOSER_ACTION_SAVE,
			 _("Export Script"), NULL);

  if (!filename) return;

  rfx_script_from = lives_build_filename(prefs->config_datadir,
                                         PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, script_name, NULL);
  d_print(_("Copying %s to %s..."), rfx_script_from, filename);

  lives_cp((tmp = lives_filename_from_utf8(rfx_script_from, -1, NULL, NULL, NULL)),
           (tmp2 = lives_filename_from_utf8(filename, -1, NULL, NULL, NULL)));

  if (THREADVAR(com_failed)) d_print_failed();
  else d_print_done();
  lives_free(tmp); lives_free(tmp2);
  lives_free(rfx_script_from); lives_free(filename); lives_free(script_name);
}


void on_import_rfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_rfx_status_t status = (lives_rfx_status_t)LIVES_POINTER_TO_INT(user_data);

  char *rfx_script_to, *rfx_dir_to;
  char *tmp, *tmp2;
  char basename[PATH_MAX];
  char *filename
    = choose_file(NULL, NULL, NULL, LIVES_FILE_CHOOSER_ACTION_OPEN, _("Import Script"), NULL);

  if (!filename) return;

  lives_snprintf(basename, PATH_MAX, "%s", filename);
  get_basename(basename);
  THREADVAR(com_failed) = FALSE;
  switch (status) {
  case RFX_STATUS_TEST:
    rfx_dir_to = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, NULL);
    lives_mkdir_with_parents((tmp = lives_filename_from_utf8(rfx_dir_to, -1, NULL, NULL, NULL)), capable->umask);
    lives_free(tmp);

    rfx_script_to = lives_build_filename(rfx_dir_to, basename, NULL);
    lives_free(rfx_dir_to);
    break;
  case RFX_STATUS_CUSTOM:
    rfx_dir_to = lives_build_path(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, NULL);
    lives_mkdir_with_parents((tmp = lives_filename_from_utf8(rfx_dir_to, -1, NULL, NULL, NULL)), capable->umask);

    lives_free(tmp);
    rfx_script_to = lives_build_filename(rfx_dir_to, basename, NULL);
    lives_free(rfx_dir_to);
    break;
  default:
    rfx_script_to = lives_build_filename(prefs->prefix_dir, PLUGIN_SCRIPTS_DIR,
                                         PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS, basename, NULL);
    break;
  }

  if (THREADVAR(com_failed)) {
    lives_free(rfx_script_to);
    lives_free(filename);
    return;
  }

  if (lives_file_test(rfx_script_to, LIVES_FILE_TEST_EXISTS)) {
    if (!do_yesno_dialogf(_("\nCustom script file:\n%s\nalready exists.\n"
			    "Do you wish to overwrite it ?\n"), basename)) {
      lives_free(rfx_script_to); lives_free(filename);
      return;
    }
  }

  d_print(_("Copying %s to %s..."), filename, rfx_script_to);

  lives_cp((tmp = lives_filename_from_utf8(filename, -1, NULL, NULL, NULL)),
           (tmp2 = lives_filename_from_utf8(rfx_script_to, -1, NULL, NULL, NULL)));

  lives_free(tmp);
  lives_free(tmp2);
  if (THREADVAR(com_failed)) d_print_failed();
  else {
    d_print_done();
    on_rebuild_rfx_activate(NULL, LIVES_INT_TO_POINTER(status));
  }
  lives_free(rfx_script_to);
  lives_free(filename);
}


char *prompt_for_script_name(const char *sname, lives_rfx_status_t status) {
  // show dialog to get script name of rfx plugin dependant on type
  // set type to RFX_STATUS_ANY to let user pick type as well
  // return value should be lives_free'd after use
  // beware: if the user cancels, return value is NULL

  // sname is suggested name, set it to NULL to prompt for a name from
  // status list

  // in copy mode, there are extra entries and the selected script will be copied
  // to test

  // in rename mode a test script will be copied to another test script

  char *name = NULL;
  char *from_name;
  char *from_status;
  char *rfx_script_from;
  char *rfx_script_to;

  rfx_build_window_t *rfxbuilder;

  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *script_combo = NULL;
  LiVESWidget *name_entry = NULL;
  LiVESWidget *script_combo_entry = NULL;
  LiVESWidget *status_combo = NULL;
  LiVESWidget *status_combo_entry = NULL;
  LiVESWidget *dialog;

  LiVESList *status_list = NULL;

  boolean copy_mode = FALSE;
  boolean rename_mode = FALSE;
  boolean OK;

  if (status == RFX_STATUS_COPY) {
    copy_mode = TRUE;
    status_list = lives_list_append(status_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]));
    status_list = lives_list_append(status_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]));
    status_list = lives_list_append(status_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_TEST]));
  }

  dialog = lives_standard_dialog_new(NULL, FALSE, -1, -1);

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), vbox, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  add_fill_to_box(LIVES_BOX(vbox));

  if (copy_mode) {
    lives_window_set_title(LIVES_WINDOW(dialog), _("Copy RFX Script"));

    status_combo = lives_standard_combo_new(_("_From type:    "), status_list, LIVES_BOX(hbox), NULL);

    status_combo_entry = lives_combo_get_entry(LIVES_COMBO(status_combo));

    lives_list_free_all(&status_list);
  } else {
    if (status == RFX_STATUS_RENAME) {
      lives_window_set_title(LIVES_WINDOW(dialog), _("Rename Test RFX Script"));
      label = lives_standard_label_new(_("From script: "));
      rename_mode = TRUE;
      status = RFX_STATUS_TEST;
    } else {
      lives_window_set_title(LIVES_WINDOW(dialog), _("RFX Script Name"));
      label = lives_standard_label_new(_("Script name: "));
    }
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);
  }

  if (!sname || copy_mode || rename_mode) {
    script_combo = lives_standard_combo_new(copy_mode ? _("Script:") : NULL, NULL, LIVES_BOX(hbox), NULL);
    name_entry = script_combo_entry = lives_combo_get_entry(LIVES_COMBO(script_combo));
  }
  if (sname || copy_mode || rename_mode) {
    // name_entry becomes a normal lives_entry
    if (copy_mode || rename_mode) {
      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    name_entry = lives_standard_entry_new(copy_mode ? _("New name:") : rename_mode ? _("New script name:") : NULL,
                                          sname, -1, 128, LIVES_BOX(hbox), NULL);

    if (copy_mode) {
      lives_signal_sync_connect(LIVES_GUI_OBJECT(status_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_script_status_changed), (livespointer)script_combo);
    }
  }
  lives_widget_grab_focus(name_entry);
  lives_entry_set_activates_default(LIVES_ENTRY(name_entry), TRUE);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL, LIVES_RESPONSE_CANCEL);

  copy_script_okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL, LIVES_RESPONSE_OK);

  lives_button_grab_default_special(copy_script_okbutton);

  lives_widget_show_all(dialog);

  if (status_combo) on_script_status_changed(LIVES_COMBO(status_combo), (livespointer)script_combo);
  else if (script_combo) populate_script_combo(LIVES_COMBO(script_combo), status);

  do {
    OK = TRUE;
    if (lives_dialog_run(LIVES_DIALOG(dialog)) == LIVES_RESPONSE_OK) {
      if (name) lives_free(name);
      name = lives_strdup(lives_entry_get_text(LIVES_ENTRY(name_entry)));
      if (copy_mode) {
        if (find_rfx_plugin_by_name(name, RFX_STATUS_TEST) > -1 ||
            find_rfx_plugin_by_name(name, RFX_STATUS_CUSTOM) > -1 ||
            find_rfx_plugin_by_name(name, RFX_STATUS_BUILTIN) > -1) {
          do_error_dialog(_("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
          OK = FALSE;
        }
        //copy selected script to test
        else {
          from_name = lives_strdup(lives_entry_get_text(LIVES_ENTRY(script_combo_entry)));
          from_status = lives_strdup(lives_entry_get_text(LIVES_ENTRY(status_combo_entry)));
          if (!strcmp(from_status, mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN])) status = RFX_STATUS_BUILTIN;
          else {
            if (!strcmp(from_status, mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM])) status = RFX_STATUS_CUSTOM;
            else status = RFX_STATUS_TEST;
          }

          if ((rfxbuilder = make_rfx_build_window(from_name, status)) == NULL) {
            // invalid name
            OK = FALSE;
          }

          lives_free(from_name);
          lives_free(from_status);

          if (OK) {
            lives_entry_set_text(LIVES_ENTRY(rfxbuilder->name_entry), name);
            rfxbuilder->mode = RFX_BUILDER_MODE_COPY;
            lives_widget_show(rfxbuilder->dialog);
          }
        }
      }
      if (rename_mode) {
        LiVESList *nmlist = NULL;
        char *xname = ensure_extension(name, LIVES_FILE_EXT_RFX_SCRIPT);

        if (name && lives_list_find((nmlist = get_script_list(status)), xname)) {
          do_error_dialog(_("\n\nThere is already a test script with this name.\nScript name must be unique.\n"));
          OK = FALSE;
        } else {
          int ret;
          from_name = lives_strdup(lives_entry_get_text(LIVES_ENTRY(script_combo_entry)));
          rfx_script_from = lives_build_filename(prefs->config_datadir,
                                                 PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, from_name, NULL);
          rfx_script_to = lives_build_filename(prefs->config_datadir,
                                               PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, xname, NULL);
          d_print(_("Renaming RFX test script %s to %s..."), from_name, xname);
          lives_free(from_name);

          if ((ret = rename(rfx_script_from, rfx_script_to))) {
            d_print_failed();
            widget_opts.non_modal = TRUE;
            do_error_dialogf(_("\n\nFailed to move the plugin script from\n%s to\n%s\n"
                               "Return code was %d\n"),
                             rfx_script_from,  rfx_script_to, ret);
            widget_opts.non_modal = FALSE;
          } else {
            d_print_done();
          }
          lives_free(rfx_script_from);
          lives_free(rfx_script_to);
        }
        if (nmlist) {
          lives_list_free_all(&nmlist);
        }
        lives_free(xname);
      }
    }
  } while (!OK);

  lives_widget_destroy(dialog);
  return name;
}


static void populate_script_combo(LiVESCombo * script_combo, lives_rfx_status_t status) {
  LiVESList *list = NULL;
  lives_combo_populate(script_combo, (list = get_script_list(status)));
  if (list) {
    lives_combo_set_active_index(script_combo, 0);
    lives_widget_set_sensitive(copy_script_okbutton, TRUE);
    lives_list_free_all(&list);
  } else {
    lives_combo_set_active_string(script_combo, "");
    lives_widget_set_sensitive(copy_script_okbutton, FALSE);
  }
}


static void on_script_status_changed(LiVESCombo * status_combo, livespointer user_data) {
  const char *text = lives_combo_get_active_text(status_combo);
  LiVESWidget *script_combo = (LiVESWidget *)user_data;

  if (!script_combo || !LIVES_IS_COMBO(script_combo)) return;

  if (!(strcmp(text, mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]))) {
    populate_script_combo(LIVES_COMBO(script_combo), RFX_STATUS_BUILTIN);
  } else {
    if (!(strcmp(text, mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]))) {
      populate_script_combo(LIVES_COMBO(script_combo), RFX_STATUS_CUSTOM);
    } else {
      if (!(strcmp(text, mainw->string_constants[LIVES_STRING_CONSTANT_TEST]))) {
        populate_script_combo(LIVES_COMBO(script_combo), RFX_STATUS_TEST);
      }
    }
  }
}


LiVESList *get_script_list(lives_rfx_status_t status) {
  LiVESList *script_list = NULL;

  switch (status) {
  case RFX_STATUS_TEST:
    script_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS, TRUE, NULL, LIVES_FILE_EXT_RFX_SCRIPT);
    break;
  case RFX_STATUS_CUSTOM:
    script_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS, TRUE, NULL, LIVES_FILE_EXT_RFX_SCRIPT);
    break;
  case RFX_STATUS_BUILTIN:
  case RFX_STATUS_COPY:
    script_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS, TRUE, NULL, LIVES_FILE_EXT_RFX_SCRIPT);
    break;
  default:
    break;
  }
  return script_list;
}


static lives_rfx_t **setup_rfx0(lives_rfx_t **rendered_fx) {
  if (!rendered_fx) {
    rendered_fx = (lives_rfx_t **)lives_calloc(1, sizeof(lives_rfx_t *));
  }

  rendered_fx[0] = (lives_rfx_t *)lives_calloc(1, sizeof(lives_rfx_t));
  rendered_fx[0]->name = lives_strdup("realtime_fx");
  rendered_fx[0]->menu_text = (_("_Apply Real Time Effects to Selection"));
  rendered_fx[0]->action_desc = (_("Applying Current Real Time Effects to"));

  rendered_fx[0]->props = 0;
  rendered_fx[0]->rfx_version[0] = 0;
  rendered_fx[0]->num_params = 0;
  rendered_fx[0]->num_in_channels = 1;
  rendered_fx[0]->menuitem = NULL;
  rendered_fx[0]->params = NULL;
  rendered_fx[0]->flags = 0;
  rendered_fx[0]->gui_strings = NULL;
  rendered_fx[0]->onchange_strings = NULL;
  rendered_fx[0]->status = RFX_STATUS_WEED;
  rendered_fx[0]->is_template = FALSE;
  rendered_fx[0]->min_frames = 1;
  rendered_fx[0]->source = NULL;
  rendered_fx[0]->source_type = LIVES_RFX_SOURCE_RFX;
  return rendered_fx;
}


boolean add_rfx_effects(lives_rfx_status_t status) {
  // scan render plugin directories, create a rfx array, and add each to the appropriate menu area

  // 0) destroy menus / entries

  // 1) scan each relevant direcotry to get updated number
  // 2) copy mainw->rendered_fx -> rendered_fx (or create it)
  // 3) append any new vals.
  // 3) alpha sort rendered_fx
  // 4) copy sorted list -> mainw->rendered_fx

  // 5) recreate menus (add_rfx_effects2)
  GET_PROC_THREAD_SELF(self);
  LiVESList *rfx_builtin_list = NULL, *rfx_custom_list = NULL, *rfx_test_list = NULL;
  LiVESList *list;
  lives_rfx_t *rfx = NULL;
  lives_rfx_t **rendered_fx;

  int i, plugin_idx;

  boolean is_startup = FALSE;
  int llen;
  int rfx_builtin_list_length, rfx_custom_list_length, rfx_test_list_length;
  int rfx_list_length, old_list_length;

  if (self && self == mainw->helper_procthreads[PT_LAZY_RFX]) {
    is_startup = TRUE;
    lives_proc_thread_set_pauseable(self, TRUE);
  }

  old_list_length = mainw->num_rendered_effects_builtin
    + mainw->num_rendered_effects_custom + mainw->num_rendered_effects_test;

  // reset / reload values
  if (status == RFX_STATUS_ANY)
    mainw->num_rendered_effects_builtin = 0;
  if (status != RFX_STATUS_TEST)
    mainw->num_rendered_effects_custom = 0;
  if (status != RFX_STATUS_CUSTOM)
    mainw->num_rendered_effects_test = 0;

  rfx_builtin_list_length = mainw->num_rendered_effects_builtin;
  rfx_custom_list_length = mainw->num_rendered_effects_custom;
  rfx_test_list_length = mainw->num_rendered_effects_test;

  if (status != RFX_STATUS_TEST) {
    mainw->has_custom_effects = mainw->has_custom_tools = mainw->has_custom_gens
      = mainw->has_custom_utilities = FALSE;
  }

  if (status == RFX_STATUS_CUSTOM) {
    if (mainw->custom_effects_separator) lives_widget_destroy(mainw->custom_effects_separator);
    if (mainw->custom_effects_menu) lives_widget_destroy(mainw->custom_effects_menu);
    if (mainw->custom_effects_submenu) lives_widget_destroy(mainw->custom_effects_submenu);
    if (mainw->custom_gens_menu) lives_widget_destroy(mainw->custom_gens_menu);
    if (mainw->custom_gens_submenu) lives_widget_destroy(mainw->custom_gens_submenu);
    if (mainw->gens_menu) lives_widget_destroy(mainw->gens_menu);

    if (mainw->custom_utilities_separator) lives_widget_destroy(mainw->custom_utilities_separator);
    if (mainw->custom_utilities_menu) lives_widget_destroy(mainw->custom_utilities_menu);
    if (mainw->custom_utilities_submenu) lives_widget_destroy(mainw->custom_utilities_submenu);
    if (mainw->custom_tools_menu) lives_widget_destroy(mainw->custom_tools_menu);
    if (mainw->utilities_menu) lives_widget_destroy(mainw->utilities_menu);

    mainw->custom_effects_separator = mainw->custom_effects_menu = mainw->custom_effects_submenu
                                      = mainw->custom_gens_menu = mainw->custom_gens_submenu = mainw->gens_menu =
                                            mainw->custom_utilities_separator = mainw->custom_utilities_menu
                                                = mainw->custom_utilities_submenu
                                                    = mainw->custom_tools_menu = mainw->utilities_menu = NULL;
  }

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  if (status != RFX_STATUS_CUSTOM) {
    mainw->run_test_rfx_menu = lives_standard_menu_new();
    rfx_test_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_TEST, FALSE, NULL, NULL);
    rfx_test_list_length = lives_list_length(rfx_test_list);
    if (rfx_test_list_length > 0) mainw->has_test_effects = TRUE;
    else mainw->has_test_effects = FALSE;
  }

  if (status != RFX_STATUS_TEST) {
    rfx_custom_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_CUSTOM, FALSE, NULL, NULL);
    rfx_custom_list_length = lives_list_length(rfx_custom_list);
  }

  if (prefs->load_rfx_builtin) {
    if (status == RFX_STATUS_ANY) {
      if (!(rfx_builtin_list = get_plugin_list(PLUGIN_RENDERED_EFFECTS_BUILTIN, FALSE, NULL, NULL))) {
	// TODO - set [0]
	mainw->rendered_fx = setup_rfx0(mainw->rendered_fx);
	return FALSE;
      } else {
        rfx_builtin_list_length = lives_list_length(rfx_builtin_list);
      }
    }
  }

  rfx_list_length = rfx_builtin_list_length + rfx_custom_list_length + rfx_test_list_length;

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  llen = rfx_list_length + 1;
  rendered_fx = (lives_rfx_t **)lives_calloc(llen, sizeof(lives_rfx_t *));

  if (status != RFX_STATUS_ANY) {
    rfx_slot_count = 0;
    // backup old values, except the ones we will recreate
    for (i = 0; i <= old_list_length; i++) {
      if (mainw->rendered_fx[i]->status != status)
	rendered_fx[rfx_slot_count++] = mainw->rendered_fx[i];
      else {
        if (mainw->rendered_fx[i]->menuitem)
          lives_widget_destroy(mainw->rendered_fx[i]->menuitem);
	rfx_free(mainw->rendered_fx[i]);
      }
      threaded_dialog_spin(0.);
    }
    lives_free(mainw->rendered_fx);
    mainw->rendered_fx = NULL;
  } else {
    setup_rfx0(rendered_fx);
    rfx_slot_count = 1;
  }

  if (rfx_list_length) {
    // query and add updated segments
    LiVESList *define = NULL;
    LiVESList *description = NULL;
    LiVESList *props = NULL;
    LiVESList *rfx_list = rfx_builtin_list;

    lives_rfx_status_t xstatus = RFX_STATUS_BUILTIN;

    char *type = PLUGIN_RENDERED_EFFECTS_BUILTIN;
    char *plugin_name;
    char *def = NULL;
    char delim[2];

    if (status == RFX_STATUS_CUSTOM) {
      rfx_list = rfx_custom_list;
      type = PLUGIN_RENDERED_EFFECTS_CUSTOM;
      xstatus = RFX_STATUS_CUSTOM;
    }
    else if (status == RFX_STATUS_TEST) {
      rfx_list = rfx_test_list;
      type = PLUGIN_RENDERED_EFFECTS_TEST;
      xstatus = RFX_STATUS_TEST;
    }

    for (;; rfx_list = rfx_list->next) {
      if (is_startup && lives_proc_thread_get_pause_requested(self))
	lives_proc_thread_pause(self);

      if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);
#if !BG_LOAD_RFX
      if (!mainw->splash_window) lives_widget_context_update();
#endif
      if (!rfx_list) {
	if (status != RFX_STATUS_ANY) break;

	if (xstatus == RFX_STATUS_BUILTIN) {
	  xstatus = RFX_STATUS_CUSTOM;
	  rfx_list = rfx_custom_list;
	  type = PLUGIN_RENDERED_EFFECTS_CUSTOM;
	}
	if (xstatus == RFX_STATUS_CUSTOM && !rfx_list) {
	  rfx_list = rfx_test_list;
	  xstatus = RFX_STATUS_TEST;
	  type = PLUGIN_RENDERED_EFFECTS_TEST;
	}
	if (!rfx_list) break;
      }

      plugin_name = lives_strdup((char *)rfx_list->data);

#if !BG_LOAD_RFX
      if (mainw->splash_window) {
        splash_msg((tmp = lives_strdup_printf(_("Loading rendered effect %s..."), plugin_name)),
		   SPLASH_LEVEL_LOAD_RFX);
        lives_free(tmp);
      }
#endif

      //#define DEBUG_RENDER_FX
#ifdef DEBUG_RENDER_FX
      g_print("Checking plugin %s\n", plugin_name);
#endif

      if ((define = plugin_request_by_line(type, plugin_name, "get_define")) == NULL) {
#ifdef DEBUG_RENDER_FX
        g_print("No get_define in %s\n", plugin_name);
#endif
        lives_free(plugin_name);
	rfx_list_length--;
        continue;
      }

      def = lives_strdup((char *)define->data);
      lives_list_free_all(&define);

      if (strlen(def) < 2) {
#ifdef DEBUG_RENDER_FX
        g_print("Invalid get_define in %s\n", plugin_name);
#endif
        lives_free(plugin_name);
        lives_free(def);
	rfx_list_length--;
        continue;
      }
      if (make_version_hash(def + 1) > make_version_hash(RFX_VERSION)) {
#ifdef DEBUG_RENDER_FX
        g_print("Invalid version %s instead of %s in %s\n", def + 1, RFX_VERSION, plugin_name);
#endif
        lives_free(plugin_name);
        lives_free(def);
	rfx_list_length--;
        continue;
      }

      delim[0] = def[0];
      delim[1] = 0;

      if ((description = plugin_request_common(type, plugin_name, "get_description", delim, TRUE)) &&
	  (props = plugin_request_common(type, plugin_name, "get_capabilities", delim, FALSE)) &&
	  lives_list_length(description) > 3) {
	rfx = rendered_fx[rfx_slot_count++] = (lives_rfx_t *)lives_calloc(1, sizeof(lives_rfx_t));
	rfx->name = lives_strdup(plugin_name);
	lives_memcpy(rfx->delim, delim, 2);
	list = description;
	rfx->menu_text = lives_strdup((char *)list->data);
	list = list->next;
	rfx->action_desc = lives_strdup((char *)list->data);
	list = list->next;
	if (!(rfx->min_frames = atoi((char *)list->data)))
	  rfx->min_frames = 1;
	list = list->next;
	rfx->num_in_channels = atoi((char *)list->data);
	rfx->status = xstatus;
	lives_snprintf(rfx->rfx_version, 64, "%s", def + 1);
	rfx->props = atoi((char *)props->data);
	rfx->num_params = 0;
	rfx->is_template = FALSE;
	rfx->params = NULL;
	rfx->source = NULL;
	rfx->source_type = LIVES_RFX_SOURCE_RFX;
	rfx->flags = 0;
	rfx->gui_strings = NULL;
	rfx->onchange_strings = NULL;
	rfx->is_template = FALSE;
	if (!check_rfx_for_lives(rfx)) rfx_slot_count--;
      }

      lives_free(plugin_name);
      if (props) {
        lives_list_free_all(&props);
        props = NULL;
      }
      if (description) {
        lives_list_free_all(&description);
        description = NULL;
      }
      lives_free(def);
    } // loop end

    if (rfx_builtin_list) lives_list_free_all(&rfx_builtin_list);
    if (rfx_custom_list) lives_list_free_all(&rfx_custom_list);
    if (rfx_test_list) lives_list_free_all(&rfx_test_list);
  }

  lives_freep((void **)&mainw->rendered_fx);

  rfx_slot_count--;

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  // sort menu text by alpha order (apart from [0]) and (re) create mainw->rendered_fx
  sort_rfx_array(rendered_fx, rfx_slot_count);
  lives_free(rendered_fx);

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  // now we need to add to the effects menu and set a callback
  for (plugin_idx = 1; plugin_idx < rfx_list_length; plugin_idx++) {
    rfx = mainw->rendered_fx[plugin_idx];

    if (status != RFX_STATUS_ANY && rfx->status != status) continue;
    if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

    render_fx_get_params(rfx, rfx->name, rfx->status);
    if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);
    rfx->source = NULL;
    rfx->flags = 0;
    rfx->gui_strings = NULL;
    rfx->onchange_strings = NULL;
    rfx->menuitem = NULL;

    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      mainw->num_rendered_effects_builtin++; break;
    case RFX_STATUS_CUSTOM:
      mainw->num_rendered_effects_custom++; break;
    case RFX_STATUS_TEST:
      mainw->num_rendered_effects_test++; break;
    default: break;
    }
  }

  if (status != RFX_STATUS_ANY) add_rfx_effects2(status);
  return TRUE;
}


void add_rfx_effects2(lives_rfx_status_t status) {
  GET_PROC_THREAD_SELF(self);
  LiVESWidget *menuitem;
  lives_rfx_t *rfx;
  char txt[64]; // menu text
  boolean is_startup = FALSE;
  int plugin_idx, tool_posn = RFX_TOOL_MENU_POSN;

  if (self && self == mainw->helper_procthreads[PT_LAZY_RFX]) {
    is_startup = TRUE;
    lives_proc_thread_set_pauseable(self, TRUE);
  }

  if (status == RFX_STATUS_ANY) {
    // recreate effects menu
    LiVESWidget *menuitem = lives_standard_image_menu_item_new_with_label(mainw->rendered_fx[0]->menu_text);

    lives_widget_set_tooltip_text(menuitem, _("See: VJ - show VJ keys. Set the realtime effects, and then apply them here."));

    lives_widget_add_accelerator(menuitem, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_e, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_realfx_activate), mainw->rendered_fx[0]);

    mainw->rendered_fx[0]->menuitem = menuitem;
    mainw->rendered_fx[0]->num_in_channels = 1;

    if (!LIVES_IS_PLAYING && CURRENT_CLIP_IS_NORMAL &&
        ((has_video_filters(TRUE) && !has_video_filters(FALSE)) ||
         (cfile->achans > 0 && prefs->audio_src == AUDIO_SRC_INT && has_audio_filters(AF_TYPE_ANY)) ||
         mainw->agen_key != 0)) {
    }
  }

  if (mainw->num_rendered_effects_test) {
    lives_widget_set_sensitive(mainw->run_test_rfx_submenu, TRUE);
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->run_test_rfx_submenu), mainw->run_test_rfx_menu);
  }
  else lives_widget_set_sensitive(mainw->run_test_rfx_submenu, FALSE);

  if (status != RFX_STATUS_TEST) make_custom_submenus();

  if (status == RFX_STATUS_TEST && mainw->run_test_rfx_menu) {
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->run_test_rfx_submenu), NULL);
    if (LIVES_IS_WIDGET(mainw->run_test_rfx_menu)) lives_widget_destroy(mainw->run_test_rfx_menu);
  }

  if (status != RFX_STATUS_TEST) {
    mainw->custom_effects_menu = lives_standard_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->custom_effects_submenu), mainw->custom_effects_menu);

    mainw->custom_tools_menu = lives_standard_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->custom_tools_submenu), mainw->custom_tools_menu);
  }

  if (status == RFX_STATUS_ANY && mainw->rendered_fx[0]->menuitem) {
    lives_container_add(LIVES_CONTAINER(mainw->effects_menu), mainw->rendered_fx[0]->menuitem);
    mainw->rte_separator = lives_menu_add_separator(LIVES_MENU(mainw->effects_menu));
  }

  if (status != RFX_STATUS_TEST) {
    lives_menu_shell_insert(LIVES_MENU_SHELL(mainw->effects_menu), mainw->custom_effects_submenu, 2);
    mainw->custom_effects_separator = lives_menu_item_new();
    lives_menu_shell_insert(LIVES_MENU_SHELL(mainw->effects_menu), mainw->custom_effects_separator, 3);
    lives_widget_set_sensitive(mainw->custom_effects_separator, FALSE);
  }

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  if (status != RFX_STATUS_TEST) {
    // recreate menus for effects, utilities, generators
    mainw->utilities_menu = lives_standard_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->utilities_submenu), mainw->utilities_menu);

    mainw->custom_utilities_menu = lives_standard_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->custom_utilities_submenu), mainw->custom_utilities_menu);

    lives_container_add(LIVES_CONTAINER(mainw->utilities_menu), mainw->custom_utilities_submenu);
    mainw->custom_utilities_separator = lives_menu_add_separator(LIVES_MENU(mainw->utilities_menu));

    mainw->gens_menu = lives_standard_menu_new();
    if (!mainw->multitrack)
      lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->gens_submenu), mainw->gens_menu);
    else
      lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->multitrack->gens_submenu), mainw->gens_menu);

    mainw->custom_gens_menu = lives_standard_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->custom_gens_submenu), mainw->custom_gens_menu);
    lives_container_add(LIVES_CONTAINER(mainw->gens_menu), mainw->custom_gens_submenu);

    if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

    if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate == -1) mainw->resize_menuitem = NULL;
  }

  if (rfx_slot_count) {
    char *tmp;
    for (plugin_idx = 1; plugin_idx <= rfx_slot_count; plugin_idx++) {
      if (is_startup && lives_proc_thread_get_pause_requested(self))
	lives_proc_thread_pause(self);

      rfx = mainw->rendered_fx[plugin_idx];
      if (status == RFX_STATUS_TEST && rfx->status == RFX_STATUS_BUILTIN) continue;

      if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

      lives_snprintf(txt, 61, "_%s", (tmp = _(rfx->menu_text)));
      lives_free(tmp);
      if (rfx->num_params) lives_strappend(txt, 64, "...");

      menuitem = lives_standard_image_menu_item_new_with_label(txt);
      rfx->menuitem = menuitem;

      if ((rfx->props & RFX_PROPS_MAY_RESIZE && rfx->num_in_channels == 1) || rfx->min_frames < 0) {
        // add resizing effects to tools menu
        if (!strcmp(rfx->name, "resize")) {
          if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate == -1) {
            mainw->resize_menuitem = menuitem;
          } else continue;
        }

        switch (rfx->status) {
        case RFX_STATUS_BUILTIN:
          if (rfx->min_frames >= 0) {
            if (status == RFX_STATUS_CUSTOM) continue;
            lives_menu_shell_insert(LIVES_MENU_SHELL(mainw->tools_menu), menuitem, tool_posn++);
          } else {
            lives_container_add(LIVES_CONTAINER(mainw->utilities_menu), menuitem);
          }
          break;
        case RFX_STATUS_CUSTOM:
          if (rfx->min_frames >= 0) {
            lives_container_add(LIVES_CONTAINER(mainw->custom_tools_menu), menuitem);
            mainw->has_custom_tools = TRUE;
          } else {
            lives_container_add(LIVES_CONTAINER(mainw->custom_utilities_menu), menuitem);
            mainw->has_custom_utilities = TRUE;
          }
          break;
        case RFX_STATUS_TEST:
          lives_container_add(LIVES_CONTAINER(mainw->run_test_rfx_menu), menuitem);
          break;
        default:
          break;
        }

        if (menuitem != mainw->resize_menuitem) {
          if (!rfx->params) {
            lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_render_fx_activate), (livespointer)rfx);
          } else {
            lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_render_fx_pre_activate), NULL);
	  }
        } else {
          mainw->fx_candidates[FX_CANDIDATE_RESIZER].func
            = lives_signal_sync_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                   LIVES_GUI_CALLBACK(on_render_fx_pre_activate), NULL);
        }
	lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), LINKED_RFX_KEY, (livespointer)rfx);
        if (rfx->min_frames >= 0) lives_widget_set_sensitive(menuitem, FALSE);
      } else {
        if (rfx->num_in_channels == 0) {
          // non-realtime generator
          switch (rfx->status) {
          case RFX_STATUS_BUILTIN:
            lives_container_add(LIVES_CONTAINER(mainw->gens_menu), menuitem); break;
          case RFX_STATUS_CUSTOM:
            lives_container_add(LIVES_CONTAINER(mainw->custom_gens_menu), menuitem);
            mainw->has_custom_gens = TRUE; break;
          case RFX_STATUS_TEST:
            lives_container_add(LIVES_CONTAINER(mainw->run_test_rfx_menu), menuitem); break;
          default: break;
          }
          lives_signal_sync_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                               LIVES_GUI_CALLBACK(on_render_fx_pre_activate), NULL);
	  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem),
				       LINKED_RFX_KEY, (livespointer)rfx);
        } else {
#if LIVES_HAS_IMAGE_MENU_ITEM
          LiVESWidget *rfx_image = NULL;
          if (rfx->props & RFX_PROPS_SLOW) {
            rfx_image = lives_image_new_from_stock(LIVES_STOCK_NO, LIVES_ICON_SIZE_MENU);
          } else {
            rfx_image = lives_image_new_from_stock(LIVES_STOCK_YES, LIVES_ICON_SIZE_MENU);
          }
	  LIVES_IGNORE_DEPRECATIONS
          lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(menuitem), rfx_image);
	  LIVES_IGNORE_DEPRECATIONS_END
#endif
          if (rfx->num_in_channels == 1) {
	    if (rfx->status == RFX_STATUS_CUSTOM) {
	      lives_container_add(LIVES_CONTAINER(mainw->custom_effects_menu), menuitem);
	      mainw->has_custom_effects = TRUE;
	    }
	    else lives_container_add(LIVES_CONTAINER(mainw->effects_menu), menuitem);
          }
          if (!rfx->params) {
            lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_render_fx_activate), (livespointer)rfx);
          } else {
            lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_render_fx_pre_activate), NULL);
          }
	  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem),
				       LINKED_RFX_KEY, (livespointer)rfx);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (status != RFX_STATUS_ANY) threaded_dialog_spin(0.);

  if (is_startup && lives_proc_thread_get_pause_requested(self))
    lives_proc_thread_pause(self);

  BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
  update_rfx_menus();
  BG_THREADVAR(hook_hints) = 0;

  if (status != RFX_STATUS_ANY) {
    threaded_dialog_spin(0.);
    if (CURRENT_CLIP_IS_VALID) {
      BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
      main_thread_execute_void(sensitize_rfx, 0);
      BG_THREADVAR(hook_hints) = 0;
    }
    threaded_dialog_spin(0.);
  }
  mainw->rfx_loaded = TRUE;
}


void update_rfx_menus(void) {
  if (prefs->show_gui) {
    if (!mainw->has_custom_effects) {
      if (mainw->custom_effects_separator)
        lives_widget_hide(mainw->custom_effects_separator);
    } else {
      lives_widget_set_no_show_all(mainw->custom_effects_submenu, FALSE);
      lives_widget_show_all(mainw->custom_effects_submenu);
      lives_widget_show(mainw->custom_effects_separator);
    }

    if (mainw->has_custom_tools) {
      lives_widget_set_no_show_all(mainw->custom_tools_submenu, FALSE);
    } else {
      lives_widget_set_no_show_all(mainw->custom_tools_submenu, TRUE);
      lives_widget_hide(mainw->custom_tools_submenu);
    }

    if (mainw->has_custom_utilities) {
      lives_widget_set_no_show_all(mainw->custom_utilities_submenu, FALSE);
    }

    lives_widget_show_all(mainw->tools_menu);

    if (!mainw->has_custom_utilities) {
      if (mainw->custom_utilities_separator) lives_widget_hide(mainw->custom_utilities_separator);
    }

    if (!mainw->has_custom_tools) {
      lives_widget_hide(mainw->custom_tools_separator);
    }

    if (mainw->has_custom_gens) {
      lives_widget_set_no_show_all(mainw->custom_gens_submenu, FALSE);
    }
    lives_widget_show_all(mainw->gens_submenu);
    if (mainw->multitrack) lives_widget_show_all(mainw->multitrack->gens_submenu);
  }

  if (mainw->num_rendered_effects_custom > 0) {
    lives_widget_set_sensitive(mainw->delete_custom_rfx, TRUE);
    lives_widget_set_sensitive(mainw->export_custom_rfx, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->delete_custom_rfx, FALSE);
    lives_widget_set_sensitive(mainw->export_custom_rfx, FALSE);
  }

  if (mainw->num_rendered_effects_test > 0) {
    lives_widget_show_all(mainw->run_test_rfx_submenu);
    lives_widget_set_sensitive(mainw->run_test_rfx_submenu, TRUE);
    lives_widget_set_sensitive(mainw->promote_test_rfx, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->run_test_rfx_submenu, FALSE);
    lives_widget_set_sensitive(mainw->promote_test_rfx, FALSE);
  }
  if (mainw->has_test_effects) {
    lives_widget_set_sensitive(mainw->delete_test_rfx, TRUE);
    lives_widget_set_sensitive(mainw->rename_test_rfx, TRUE);
    lives_widget_set_sensitive(mainw->edit_test_rfx, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->delete_test_rfx, FALSE);
    lives_widget_set_sensitive(mainw->rename_test_rfx, FALSE);
    lives_widget_set_sensitive(mainw->edit_test_rfx, FALSE);
  }
  lives_widget_show_all(mainw->effects_menu);
}

