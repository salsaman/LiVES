// rfx-builder.c
// LiVES
// (c) G. Finch 2004 - 2014 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details



#include <errno.h>

#include "main.h"
#include "rfx-builder.h"
#include "support.h"
#include "interface.h"
#include "paramwindow.h"
#include "effects.h"

static GtkWidget *copy_script_okbutton;

static void table_select_row(rfx_build_window_t *rfxbuilder, int row);

void on_new_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  rfx_build_window_t *rfxbuilder;

  if (!check_builder_programs()) return;
  rfxbuilder=make_rfx_build_window(NULL,RFX_STATUS_ANY);
  lives_widget_show (rfxbuilder->dialog);
}

void on_edit_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  rfx_build_window_t *rfxbuilder;
  gchar *script_name;
  short status=GPOINTER_TO_INT (user_data);

  if (!check_builder_programs()) return;
  if (status!=RFX_STATUS_TEST) return; // we only edit test effects currently

  if ((script_name=prompt_for_script_name (NULL,RFX_STATUS_TEST))==NULL) return;
  if (!strlen (script_name)) {
    g_free (script_name);
    return;
  }
  if ((rfxbuilder=make_rfx_build_window(script_name,RFX_STATUS_TEST))==NULL) {
    g_free (script_name);
    return; // script not loaded
  }
  g_free (script_name);
  rfxbuilder->mode=RFX_BUILDER_MODE_EDIT;
  rfxbuilder->oname=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->name_entry)));
  lives_widget_show (rfxbuilder->dialog);
}


void on_copy_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  short status=GPOINTER_TO_INT (user_data);

  if (!check_builder_programs()) return;
  if (status!=RFX_STATUS_TEST) return; // we only copy to test effects currently

  // prompt_for_script_name will create our builder window from the input
  // script and set the name : note, we have no rfxbuilder reference

  // TODO - use RFXBUILDER_MODE-*
  prompt_for_script_name (NULL,RFX_STATUS_COPY);
}


void on_rename_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  short status=GPOINTER_TO_INT (user_data);

  if (status!=RFX_STATUS_TEST) return; // we only copy to test effects currently

  prompt_for_script_name (NULL,RFX_STATUS_RENAME);
}





rfx_build_window_t *make_rfx_build_window (const gchar *script_name, lives_rfx_status_t status) {
  // TODO - set mnemonic widgets for entries

  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GtkWidget *scrollw;
  GtkWidget *top_vbox;

  GSList *radiobutton_type_group = NULL;
  GList *langc=NULL;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)g_malloc (sizeof(rfx_build_window_t));

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(lives_accel_group_new ());

  gchar *tmp,*tmp2,*title,*string;

  rfxbuilder->rfx_version=g_strdup(RFX_VERSION);

  rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1; // the default

  rfxbuilder->num_reqs=0;
  rfxbuilder->num_params=0;
  rfxbuilder->num_paramw_hints=0;

  rfxbuilder->num_triggers=0;

  rfxbuilder->has_init_trigger=FALSE;
  rfxbuilder->props=0;

  rfxbuilder->plugin_version=1;

  rfxbuilder->pre_code=g_strdup ("");
  rfxbuilder->loop_code=g_strdup ("");
  rfxbuilder->post_code=g_strdup ("");

  rfxbuilder->field_delim=g_strdup ("|");

  rfxbuilder->script_name=NULL;  // use default name.script
  rfxbuilder->oname=NULL;

  rfxbuilder->mode=RFX_BUILDER_MODE_NEW;

  rfxbuilder->table_swap_row1=-1;

  /////////////////////////////////////////////////////////


  if (script_name==NULL) {
    title=g_strdup(_("LiVES: - New Test RFX"));
  }
  else {
    title=g_strdup(_("LiVES: - Edit Test RFX"));
  }

  rfxbuilder->dialog = lives_standard_dialog_new (title,FALSE);
  g_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(rfxbuilder->dialog),GTK_WINDOW(mainw->LiVES));
  }


  lives_window_add_accel_group (LIVES_WINDOW (rfxbuilder->dialog), accel_group);

  lives_container_set_border_width (LIVES_CONTAINER (rfxbuilder->dialog), widget_opts.border_width>>1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(rfxbuilder->dialog));

  top_vbox = lives_vbox_new (FALSE, 0);

  scrollw = lives_standard_scrolled_window_new ((PREF_RFXDIALOG_W<mainw->scr_width-20.*widget_opts.scale)?
						PREF_RFXDIALOG_W:mainw->scr_width-20.*widget_opts.scale, 
						(PREF_RFXDIALOG_H<mainw->scr_height-60.*widget_opts.scale)?
						PREF_RFXDIALOG_H:mainw->scr_height-60.*widget_opts.scale,top_vbox);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), scrollw, TRUE, TRUE, 0);

  // types
  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (top_vbox), hbox, TRUE, TRUE, 0);


  label = lives_standard_label_new (_("Type:"));
  lives_box_pack_start (LIVES_BOX (hbox), label, TRUE, FALSE, widget_opts.packing_width);

  string=lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT,FALSE);
  rfxbuilder->type_effect1_radiobutton = lives_standard_radio_button_new (string,FALSE,radiobutton_type_group,LIVES_BOX(hbox),NULL);
  radiobutton_type_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (rfxbuilder->type_effect1_radiobutton));
  g_free(string);

  string=lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION,FALSE);
  rfxbuilder->type_effect2_radiobutton = lives_standard_radio_button_new (string,FALSE,radiobutton_type_group,LIVES_BOX(hbox),NULL);
  radiobutton_type_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (rfxbuilder->type_effect2_radiobutton));
  g_free(string);

  string=lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_GENERATOR,FALSE);
  rfxbuilder->type_effect0_radiobutton = lives_standard_radio_button_new (string,FALSE,radiobutton_type_group,LIVES_BOX(hbox),NULL);
  radiobutton_type_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (rfxbuilder->type_effect0_radiobutton));
  g_free(string);

  rfxbuilder->type_tool_radiobutton = lives_standard_radio_button_new (_("tool"),FALSE,radiobutton_type_group,LIVES_BOX(hbox),NULL);
  radiobutton_type_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (rfxbuilder->type_tool_radiobutton));  lives_widget_show (label);

  rfxbuilder->type_utility_radiobutton = lives_standard_radio_button_new (_("utility"),FALSE,radiobutton_type_group,LIVES_BOX(hbox),NULL);


  // name

  rfxbuilder->name_entry = lives_standard_entry_new ((tmp=g_strdup(_("Name:          "))),FALSE,NULL,-1,-1,
						     LIVES_BOX(top_vbox),(tmp2=g_strdup(_("The name of the plugin. No spaces allowed.")))
						     );
  g_free(tmp); g_free(tmp2);

  // version
  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  rfxbuilder->spinbutton_version = lives_standard_spin_button_new ((tmp=g_strdup(_("Version:       "))),FALSE,
								   rfxbuilder->plugin_version, rfxbuilder->plugin_version, 1000000., 1., 1., 0,
								   LIVES_BOX(hbox),(tmp2=g_strdup(_ ("The script version.")))
								   );
  g_free(tmp); g_free(tmp2);


  // author

  rfxbuilder->author_entry = lives_standard_entry_new ((tmp=g_strdup(_("    Author:       "))),FALSE,NULL,-1,-1,
						       LIVES_BOX(hbox),(tmp2=g_strdup(_("The script author.")))
						       );
  g_free(tmp); g_free(tmp2);

  // URL

  rfxbuilder->url_entry = lives_standard_entry_new ((tmp=g_strdup(_("    URL (optional):       "))),FALSE,NULL,-1,-1,
						    LIVES_BOX(top_vbox),(tmp2=g_strdup(_("URL for the plugin maintainer.")))
						    );
  g_free(tmp); g_free(tmp2);


  // menu entry


  rfxbuilder->menu_text_entry = lives_standard_entry_new ((tmp=g_strdup(_("Menu text:    "))),FALSE,NULL,-1,-1,
							  LIVES_BOX(top_vbox),(tmp2=g_strdup(_("The text to show in the menu.")))
							  );
  g_free(tmp); g_free(tmp2);


  rfxbuilder->action_desc_hsep = add_hsep_to_box(LIVES_BOX(top_vbox));

  // action description

  rfxbuilder->action_desc_hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->action_desc_hbox, FALSE, FALSE, widget_opts.packing_height);

  rfxbuilder->action_desc_entry = lives_standard_entry_new ((tmp=g_strdup(_("Action description:        "))),FALSE,NULL,-1,-1,
							    LIVES_BOX(rfxbuilder->action_desc_hbox),
							    (tmp2=g_strdup(_("Describe what the plugin is doing. E.g. \"Edge detecting\"")))
							    );



  rfxbuilder->spinbutton_min_frames = lives_standard_spin_button_new ((tmp=g_strdup(_("Minimum frames:"))),FALSE,
								      1., 1., 1000., 1., 1., 0,
								      LIVES_BOX(top_vbox),
								      (tmp2=g_strdup(_("Minimum number of frames this effect/tool can be applied to. Normally 1.")))
								      );


  rfxbuilder->min_frames_label=widget_opts.last_label;

  // requirements


  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->requirements_button=lives_button_new_with_mnemonic (_ ("_Requirements..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->requirements_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->requirements_button,
			(_ ("Enter any binaries required by the plugin.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->properties_button=lives_button_new_with_mnemonic (_ ("_Properties..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->properties_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->properties_button,
			(_ ("Set properties for the plugin. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->params_button=lives_button_new_with_mnemonic (_ ("_Parameters..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->params_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->params_button,
			(_ ("Set up parameters used in pre/loop/post/trigger code. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->param_window_button=lives_button_new_with_mnemonic (_ ("Parameter _Window Hints..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->param_window_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->param_window_button,
			(_ ("Set hints about how to lay out the parameter window. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  langc = g_list_append (langc, (gpointer)"0xF0 - LiVES-Perl");

  rfxbuilder->langc_combo = lives_standard_combo_new ((tmp=g_strdup(_("_Language code:"))),TRUE,langc,LIVES_BOX(top_vbox),
						      (tmp2=g_strdup(_ ("Language for pre/loop/post/triggers. Optional."))));

  g_free(tmp);
  g_free(tmp2);
  g_list_free(langc);


  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->pre_button=lives_button_new_with_mnemonic (_ ("_Pre loop code..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->pre_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->pre_button,
			(_ ("Code to be executed before the loop. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->loop_button=lives_button_new_with_mnemonic (_ ("_Loop code..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->loop_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->loop_button,
			(_ ("Loop code to be applied to each frame.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->post_button=lives_button_new_with_mnemonic (_ ("_Post loop code..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->post_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->post_button,
			(_ ("Code to be executed after the loop. Optional.")));

  add_hsep_to_box(LIVES_BOX(top_vbox));

  rfxbuilder->trigger_button=lives_button_new_with_mnemonic (_ ("_Trigger code..."));
  lives_box_pack_start (LIVES_BOX (top_vbox), rfxbuilder->trigger_button, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text( rfxbuilder->trigger_button,
			(_ ("Set trigger code for when the parameter window is shown, or when a parameter is changed. Optional (except for Utilities).")));


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (rfxbuilder->dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_dialog_add_action_widget (LIVES_DIALOG (rfxbuilder->dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_dialog_add_action_widget (LIVES_DIALOG (rfxbuilder->dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);

  lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  lives_widget_add_accelerator (okbutton, "activate", accel_group,
                              LIVES_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0);


  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_rfxbuilder_ok),
		    (gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_rfxbuilder_cancel),
		    (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->requirements_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->properties_button, "clicked", 
		   G_CALLBACK (on_properties_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->params_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->param_window_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->trigger_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->pre_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->loop_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->post_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect1_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect2_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect0_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_tool_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_utility_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);


  // edit mode
  if (script_name!=NULL) {
    // we assume the name is valid
    gchar *script_file;
    
    switch (status) {
    case RFX_STATUS_CUSTOM:
      script_file=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,script_name,NULL);
    break;
    case RFX_STATUS_BUILTIN:
      script_file=g_build_filename (prefs->prefix_dir,PLUGIN_SCRIPTS_DIR,
				    PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,script_name,NULL);
    break;
    default:
      script_file=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,script_name,NULL);
    break;
    }
    if (!script_to_rfxbuilder (rfxbuilder,script_file)) {
      gchar *msg=g_strdup_printf (_ ("\n\nUnable to parse the script file:\n%s\n%s\n"),script_file,mainw->msg);
      // must use blocking error dialogs as the scriptname window is modal
      do_blocking_error_dialog (msg);
      g_free (msg);
      rfxbuilder_destroy (rfxbuilder);
      g_free (script_file);
      return NULL;
    }
    g_free (script_file);
    switch (rfxbuilder->type) {
    case RFX_BUILD_TYPE_TOOL:
      lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->type_tool_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT0:
      lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->type_effect0_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_UTILITY:
      lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->type_utility_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT2:
      lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->type_effect2_radiobutton),TRUE);
      break;
    default:
      break;
    }
  }
  else {
    lives_widget_grab_focus (rfxbuilder->name_entry);
  }

  lives_widget_show_all(rfxbuilder->dialog);

  return rfxbuilder;
}


void after_rfxbuilder_type_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  lives_widget_set_sensitive (rfxbuilder->pre_button,TRUE);
  lives_widget_set_sensitive (rfxbuilder->loop_button,TRUE);
  lives_widget_set_sensitive (rfxbuilder->post_button,TRUE);
  lives_widget_set_sensitive (rfxbuilder->requirements_button,TRUE);
  lives_widget_set_sensitive (rfxbuilder->properties_button,TRUE);
  lives_widget_show (rfxbuilder->spinbutton_min_frames);
  lives_widget_show (rfxbuilder->min_frames_label);
  lives_widget_show (rfxbuilder->action_desc_hbox);
  lives_widget_show (rfxbuilder->action_desc_hsep);

  if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect1_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1;
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect2_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT2;
    lives_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect0_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT0;
    lives_widget_hide (rfxbuilder->spinbutton_min_frames);
    lives_widget_hide (rfxbuilder->min_frames_label);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_tool_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_TOOL;
    lives_widget_hide (rfxbuilder->spinbutton_min_frames);
    lives_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
    lives_widget_hide (rfxbuilder->min_frames_label);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_utility_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_UTILITY;
    lives_widget_hide (rfxbuilder->spinbutton_min_frames);
    lives_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
    lives_widget_set_sensitive (rfxbuilder->requirements_button,FALSE);
    lives_widget_set_sensitive (rfxbuilder->pre_button,FALSE);
    lives_widget_set_sensitive (rfxbuilder->loop_button,FALSE);
    lives_widget_set_sensitive (rfxbuilder->post_button,FALSE);
    lives_widget_hide (rfxbuilder->min_frames_label);
    lives_widget_hide (rfxbuilder->action_desc_hbox);
    lives_widget_hide (rfxbuilder->action_desc_hsep);
  }

}






void on_list_table_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *vseparator;
  GtkWidget *button_box;

  // buttons
  GtkWidget *new_entry_button;
  GtkWidget *okbutton;
  GtkWidget *scrolledwindow;
  GtkWidget *cancelbutton;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(lives_accel_group_new ());

  register int i;

  char *title=NULL;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (button==GTK_BUTTON (rfxbuilder->requirements_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_REQUIREMENTS;
  }
  else if (button==GTK_BUTTON (rfxbuilder->params_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_PARAMS;
  }
  else if (button==GTK_BUTTON (rfxbuilder->param_window_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_PARAM_WINDOW;
  }
  else if (button==GTK_BUTTON (rfxbuilder->trigger_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_TRIGGERS;
  }


  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    title=g_strdup(_("LiVES: - RFX Requirements"));
    rfxbuilder->onum_reqs=rfxbuilder->num_reqs;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    title=g_strdup(_("LiVES: - RFX Parameters"));
    rfxbuilder->onum_params=rfxbuilder->num_params;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    title=g_strdup(_("LiVES: - RFX Parameter Window Hints"));
    rfxbuilder->onum_paramw_hints=rfxbuilder->num_paramw_hints;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    title=g_strdup(_("LiVES: - RFX Triggers"));
    rfxbuilder->onum_triggers=rfxbuilder->num_triggers;
  }

  dialog = lives_standard_dialog_new (title,FALSE);
  if (title!=NULL) g_free(title);

  lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));


  // create table and add rows
  rfxbuilder->table_rows=0;

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    rfxbuilder->table=lives_table_new (rfxbuilder->num_reqs,1,FALSE);
    for (i=0;i<rfxbuilder->num_reqs;i++) {
      on_table_add_row (NULL,(gpointer)rfxbuilder);
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry[i]),rfxbuilder->reqs[i]);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    rfxbuilder->copy_params=(lives_param_t *)g_malloc (RFXBUILD_MAX_PARAMS*sizeof(lives_param_t));
    rfxbuilder->table=lives_table_new (rfxbuilder->num_params,3,FALSE);
    for (i=0;i<rfxbuilder->num_params;i++) {
      param_copy (&rfxbuilder->params[i],&rfxbuilder->copy_params[i],FALSE);
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->table=lives_table_new (rfxbuilder->table_rows,2,FALSE);
    for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    rfxbuilder->copy_triggers=(rfx_trigger_t *)g_malloc ((RFXBUILD_MAX_PARAMS+1)*sizeof(rfx_trigger_t));
    rfxbuilder->table=lives_table_new (rfxbuilder->table_rows,1,FALSE);
    for (i=0;i<rfxbuilder->num_triggers;i++) {
      rfxbuilder->copy_triggers[i].when=rfxbuilder->triggers[i].when;
      rfxbuilder->copy_triggers[i].code=g_strdup (rfxbuilder->triggers[i].code);
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }

  hbox = lives_hbox_new (FALSE,0);

  lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  scrolledwindow = lives_standard_scrolled_window_new (RFX_WINSIZE_H*5/6,RFX_WINSIZE_V/4,rfxbuilder->table);

  lives_box_pack_start(LIVES_BOX(hbox),scrolledwindow,FALSE,FALSE,widget_opts.packing_width);


  // button box on right
  vseparator = lives_vseparator_new ();
  lives_box_pack_start (LIVES_BOX (hbox), vseparator, TRUE, TRUE, widget_opts.packing_width);

  button_box=lives_vbutton_box_new();
  lives_box_pack_start (LIVES_BOX (hbox), button_box, FALSE, FALSE, 0);

  new_entry_button=lives_button_new_with_mnemonic (_ ("_New Entry"));
  lives_box_pack_start (LIVES_BOX (button_box), new_entry_button, FALSE, FALSE, 0);

  rfxbuilder->edit_entry_button=lives_button_new_with_mnemonic (_ ("_Edit Entry"));
  lives_box_pack_start (LIVES_BOX (button_box), rfxbuilder->edit_entry_button, FALSE, FALSE, 0);

  rfxbuilder->remove_entry_button=lives_button_new_with_mnemonic (_ ("_Remove Entry"));
  lives_box_pack_start (LIVES_BOX (button_box), rfxbuilder->remove_entry_button, FALSE, FALSE, 0);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->move_up_button=lives_button_new_with_mnemonic (_ ("Move _Up"));
    lives_box_pack_start (LIVES_BOX (button_box), rfxbuilder->move_up_button, FALSE, FALSE, 0);
    
    rfxbuilder->move_down_button=lives_button_new_with_mnemonic (_ ("Move _Down"));
    lives_box_pack_start (LIVES_BOX (button_box), rfxbuilder->move_down_button, FALSE, FALSE, 0);


    lives_widget_set_sensitive(rfxbuilder->move_up_button,FALSE);
    lives_widget_set_sensitive(rfxbuilder->move_down_button,FALSE);
  }
  else {
    rfxbuilder->move_up_button=rfxbuilder->move_down_button=NULL;
  }

  lives_widget_set_sensitive(rfxbuilder->edit_entry_button,FALSE);
  lives_widget_set_sensitive(rfxbuilder->remove_entry_button,FALSE);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);


  okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_requirements_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_requirements_cancel),
		      user_data);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_params_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_params_cancel),
		      user_data);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_param_window_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_param_window_cancel),
		      user_data);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_triggers_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_triggers_cancel),
		      user_data);
  }



  g_signal_connect (GTK_OBJECT (new_entry_button), "clicked",
		    G_CALLBACK (on_table_add_row),
		    user_data);

  g_signal_connect (GTK_OBJECT (rfxbuilder->edit_entry_button), "clicked",
		    G_CALLBACK (on_table_edit_row),
		    user_data);

  g_signal_connect (GTK_OBJECT (rfxbuilder->remove_entry_button), "clicked",
		    G_CALLBACK (on_table_delete_row),
		    user_data);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    g_signal_connect (GTK_OBJECT (rfxbuilder->move_up_button), "clicked",
		      G_CALLBACK (on_table_swap_row),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (rfxbuilder->move_down_button), "clicked",
		      G_CALLBACK (on_table_swap_row),
		      user_data);
  }

  lives_widget_show_all (dialog);
  table_select_row(rfxbuilder,-1);
}

void on_requirements_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_reqs;i++) {
    g_free (rfxbuilder->reqs[i]);
  }
  for (i=0;i<rfxbuilder->num_reqs;i++) {
    rfxbuilder->reqs[i]=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[i])));
  }
  lives_general_button_clicked(button,NULL);
}

void on_requirements_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  rfxbuilder->num_reqs=rfxbuilder->onum_reqs;
  lives_general_button_clicked(button,NULL);
}

void on_properties_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0) {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_slow))) {
      rfxbuilder->props|=RFX_PROPS_SLOW;
    }
    else if (rfxbuilder->props&RFX_PROPS_SLOW) {
      rfxbuilder->props^=RFX_PROPS_SLOW;
    }
  }
  else {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_batchg))) {
      rfxbuilder->props|=RFX_PROPS_BATCHG;
    }
    else if (rfxbuilder->props&RFX_PROPS_BATCHG) {
      rfxbuilder->props^=RFX_PROPS_BATCHG;
    }
  }
  lives_general_button_clicked(button,NULL);
}


void on_params_ok (GtkButton *button, gpointer user_data) { 
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_params;i++) {
    g_free (rfxbuilder->params[i].name);
    g_free (rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list!=NULL) g_list_free (rfxbuilder->params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->params[i].def);
  }
  if (rfxbuilder->onum_params) {
    g_free (rfxbuilder->params);
  }
  rfxbuilder->params=(lives_param_t *)g_malloc (rfxbuilder->num_params*sizeof(lives_param_t));
  for (i=0;i<rfxbuilder->num_params;i++) {
    param_copy (&rfxbuilder->copy_params[i],&rfxbuilder->params[i],FALSE);

    // this is the only place these should be freed
    g_free (rfxbuilder->copy_params[i].name);
    g_free (rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list!=NULL) g_list_free (rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->copy_params);
  }
  lives_general_button_clicked(button,NULL);
}


void on_params_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->num_params;i++) {
    // this is the only place these should be freed
    g_free (rfxbuilder->copy_params[i].name);
    g_free (rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list!=NULL) g_list_free (rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->copy_params);
  }
  rfxbuilder->num_params=rfxbuilder->onum_params;
  lives_general_button_clicked(button,NULL);
}

void on_param_window_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;

  for (i=0;i<rfxbuilder->onum_paramw_hints;i++) {
    g_free (rfxbuilder->paramw_hints[i]);
  }
  for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
    rfxbuilder->paramw_hints[i]=g_strdup_printf ("%s%s%s",lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[i])),rfxbuilder->field_delim,
						 lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry2[i])));
  }
  lives_general_button_clicked(button,NULL);
}

void on_param_window_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  rfxbuilder->num_paramw_hints=rfxbuilder->onum_paramw_hints;
  lives_general_button_clicked(button,NULL);
}

void on_code_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  switch (rfxbuilder->codetype) {
  case RFX_CODE_TYPE_PRE:
    g_free (rfxbuilder->pre_code);
    rfxbuilder->pre_code=lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_LOOP:
    g_free (rfxbuilder->loop_code);
    rfxbuilder->loop_code=lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_POST:
    g_free (rfxbuilder->post_code);
    rfxbuilder->post_code=lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview));
    break;

  case RFX_CODE_TYPE_STRDEF:
    {
      int maxlen=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));
      gchar buf[++maxlen];
      
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].def!=NULL) g_free (rfxbuilder->copy_params[rfxbuilder->edit_param].def);
      g_snprintf (buf,maxlen,"%s",lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview)));

      rfxbuilder->copy_params[rfxbuilder->edit_param].def=subst (buf,rfxbuilder->field_delim,"");
      break;
    }
  case RFX_CODE_TYPE_STRING_LIST:
    {
      gchar *values=lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview));
      gchar **lines=g_strsplit (values,"\n",-1);
      int numlines=get_token_count (values,'\n');
      int i;
      int defindex=get_int_param (rfxbuilder->copy_params[rfxbuilder->edit_param].def);

      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	g_list_free (rfxbuilder->copy_params[rfxbuilder->edit_param].list);
	rfxbuilder->copy_params[rfxbuilder->edit_param].list=NULL;
      }
      for (i=0;i<numlines;i++) {
	if (i<numlines-1||strlen (lines[i])) {
	  rfxbuilder->copy_params[rfxbuilder->edit_param].list=g_list_append 
	    (rfxbuilder->copy_params[rfxbuilder->edit_param].list,g_strdup (lines[i]));
	}
      }
      g_strfreev (lines);

      // set "default" combo - TODO - try to retain old default using string matching
      lives_combo_populate (LIVES_COMBO (rfxbuilder->param_def_combo), 
			     rfxbuilder->copy_params[rfxbuilder->edit_param].list);
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list==NULL||
	  defindex>g_list_length (rfxbuilder->copy_params[rfxbuilder->edit_param].list)) {
	set_int_param (rfxbuilder->copy_params[rfxbuilder->edit_param].def,(defindex=0));
      }
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	lives_combo_set_active_string (LIVES_COMBO(rfxbuilder->param_def_combo), 
				       (gchar *)g_list_nth_data (rfxbuilder->copy_params[rfxbuilder->edit_param].list,defindex));
      }

    }
  }
  lives_general_button_clicked(button,NULL);
}


void on_triggers_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_triggers;i++) {
    g_free (rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->onum_triggers) {
    g_free (rfxbuilder->triggers);
  }
  rfxbuilder->triggers=(rfx_trigger_t *)g_malloc (rfxbuilder->num_triggers*sizeof(rfx_trigger_t));
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    rfxbuilder->triggers[i].when=rfxbuilder->copy_triggers[i].when;
    rfxbuilder->triggers[i].code=g_strdup (rfxbuilder->copy_triggers[i].code);
    g_free (rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->copy_triggers);
  }
  lives_general_button_clicked(button,NULL);
}

void on_triggers_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    g_free (rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->copy_triggers);
  }
  rfxbuilder->num_triggers=rfxbuilder->onum_triggers;
  lives_general_button_clicked(button,NULL);
}


void on_properties_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(lives_accel_group_new ());

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  dialog = lives_standard_dialog_new (_("LiVES: - RFX Properties"),FALSE);
  lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(rfxbuilder->dialog));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  rfxbuilder->prop_slow= lives_standard_check_button_new (_("_Slow (hint to GUI)"),TRUE,LIVES_BOX(dialog_vbox),NULL);

  if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0) {
    lives_widget_show (rfxbuilder->prop_slow);
  }

  if (rfxbuilder->props&RFX_PROPS_SLOW) {
    lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->prop_slow),TRUE);
  }

  if (rfxbuilder->type==RFX_BUILD_TYPE_EFFECT0) {
    rfxbuilder->prop_batchg=lives_standard_check_button_new (_("_Batch mode generator"),TRUE,LIVES_BOX(dialog_vbox),NULL);
    if (rfxbuilder->props&RFX_PROPS_BATCHG) {
      lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (rfxbuilder->prop_batchg),TRUE);
    }
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);

  lives_widget_grab_default (okbutton);

  lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  
  lives_widget_add_accelerator (okbutton, "activate", accel_group,
                              LIVES_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0);


  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_properties_ok),
		    user_data);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    NULL);
  
  lives_widget_show_all (dialog);

}


static void table_select_row(rfx_build_window_t *rfxbuilder, int row) {
  register int i;
  for (i=0;i<rfxbuilder->table_rows;i++) {
    if (i==row) {
      lives_widget_set_sensitive(rfxbuilder->edit_entry_button,TRUE);
      lives_widget_set_sensitive(rfxbuilder->remove_entry_button,TRUE);
      if (rfxbuilder->move_up_button!=NULL) lives_widget_set_sensitive(rfxbuilder->move_up_button,i!=0);
      if (rfxbuilder->move_down_button!=NULL) lives_widget_set_sensitive(rfxbuilder->move_down_button,i<rfxbuilder->table_rows-1);
    
      lives_widget_set_sensitive(rfxbuilder->entry[i],TRUE);
      if (rfxbuilder->entry2[i]!=NULL) {
	lives_widget_set_sensitive(rfxbuilder->entry2[i],TRUE);
      }
      if (rfxbuilder->entry3[i]!=NULL) {
	lives_widget_set_sensitive(rfxbuilder->entry3[i],TRUE);
      }
    }
    else {
      gtk_editable_select_region(GTK_EDITABLE(rfxbuilder->entry[i]),0,0);
      lives_widget_set_sensitive(rfxbuilder->entry[i],FALSE);
      lives_entry_set_editable(LIVES_ENTRY(rfxbuilder->entry[i]),FALSE);
      if (rfxbuilder->entry2[i]!=NULL) {
	gtk_editable_select_region(GTK_EDITABLE(rfxbuilder->entry2[i]),0,0);
	lives_widget_set_sensitive(rfxbuilder->entry2[i],FALSE);
      }
      if (rfxbuilder->entry3[i]!=NULL) {
	gtk_editable_select_region(GTK_EDITABLE(rfxbuilder->entry3[i]),0,0);
	lives_widget_set_sensitive(rfxbuilder->entry3[i],FALSE);
      }
    }
  }
}





static boolean on_entry_click (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  register int i;

  widget=lives_bin_get_child(LIVES_BIN(widget));
  for (i=0;i<rfxbuilder->table_rows;i++) {
    if (widget==rfxbuilder->entry[i]||widget==rfxbuilder->entry2[i]||widget==rfxbuilder->entry3[i]) {
      table_select_row(rfxbuilder,i);
      
      if (event->type!=GDK_BUTTON_PRESS) {
	//double click
	on_table_edit_row(NULL,rfxbuilder);
      }
      return TRUE;
    }
  }
  return TRUE;
}



void on_table_add_row (GtkButton *button, gpointer user_data) {
  GtkWidget *entry=NULL,*entry2=NULL,*entry3=NULL;
  GtkWidget *param_dialog=NULL;
  GtkWidget *param_window_dialog=NULL;
  GtkWidget *trigger_dialog=NULL;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  lives_param_t *param=NULL;

  register int i;

  gchar *tmpx;
  gchar *ctext;

  GtkWidget *ebox,*ebox2=NULL,*ebox3=NULL;

  rfxbuilder->entry2[rfxbuilder->table_rows]=rfxbuilder->entry3[rfxbuilder->table_rows]=NULL;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    if (rfxbuilder->table_rows>=RFXBUILD_MAX_REQ) return;

    for (i=0;i<rfxbuilder->table_rows;i++) {
      lives_entry_set_editable (LIVES_ENTRY (rfxbuilder->entry[i]), FALSE);
    }

    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();

    if (button!=NULL)
      lives_entry_set_editable (LIVES_ENTRY (entry), TRUE);
    else 
      lives_entry_set_editable (LIVES_ENTRY (entry), FALSE);

    ebox=lives_event_box_new();
    gtk_widget_set_events (ebox, GDK_BUTTON_PRESS_MASK);

    lives_container_add(LIVES_CONTAINER(ebox),entry);

    lives_table_resize (LIVES_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,1);
    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    lives_widget_context_update();
    lives_widget_grab_focus (entry);
    if (button!=NULL) {
      rfxbuilder->num_reqs++;
    }
    else goto add_row_done;
    break;

  case RFX_TABLE_TYPE_PARAMS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows] = gtk_entry_new ();

    if (button!=NULL) {
      boolean param_ok=FALSE;
      param=&rfxbuilder->copy_params[rfxbuilder->num_params];

      param->def=NULL;
      param->list=NULL;
      param->type=LIVES_PARAM_UNKNOWN;

      param_dialog=make_param_dialog(-1,rfxbuilder);
      do {
	if (lives_dialog_run (LIVES_DIALOG (param_dialog))==GTK_RESPONSE_CANCEL) {
	  g_free (param->def);
	  lives_widget_destroy (entry);
	  lives_widget_destroy (entry2);
	  lives_widget_destroy (param_dialog);
	  return;
	}
	param_ok=perform_param_checks (rfxbuilder,rfxbuilder->num_params,rfxbuilder->num_params+1);
      } while (!param_ok);
      
      rfxbuilder->num_params++;

      lives_entry_set_text (LIVES_ENTRY (entry2),lives_entry_get_text (GTK_ENTRY (rfxbuilder->param_name_entry)));
    }
    else {
      lives_entry_set_text (LIVES_ENTRY (entry2),rfxbuilder->params[rfxbuilder->table_rows].name);
    }

    lives_entry_set_text (LIVES_ENTRY (entry),(tmpx=g_strdup_printf ("p%d",rfxbuilder->table_rows)));
    g_free(tmpx);

    lives_entry_set_editable (LIVES_ENTRY (entry), FALSE);
     
    ebox=lives_event_box_new();
    gtk_widget_set_events (ebox, GDK_BUTTON_PRESS_MASK);

    lives_container_add(LIVES_CONTAINER(ebox),entry);

    lives_table_resize (LIVES_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,3);

    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (0),
		      (GtkAttachOptions) (0), 0, 0);



    lives_entry_set_editable (LIVES_ENTRY (entry2), FALSE);


    ebox2=lives_event_box_new();
    gtk_widget_set_events (ebox2, GDK_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox2),entry2);

    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox2, 1, 2, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    entry3 = rfxbuilder->entry3[rfxbuilder->table_rows-1] = gtk_entry_new ();

    if (button==NULL) {
      param=&rfxbuilder->params[rfxbuilder->table_rows-1];
    }
    else {
      param_set_from_dialog ((param=&rfxbuilder->copy_params[rfxbuilder->table_rows-1]),rfxbuilder);
    }
    
    switch (param->type) {
    case LIVES_PARAM_NUM:
      lives_entry_set_text (LIVES_ENTRY (entry3),(tmpx=g_strdup_printf ("num%d",param->dp)));
      g_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      lives_entry_set_text (LIVES_ENTRY (entry3),"bool");
      break;
    case LIVES_PARAM_COLRGB24:
      lives_entry_set_text (LIVES_ENTRY (entry3),"colRGB24");
      break;
    case LIVES_PARAM_STRING:
      lives_entry_set_text (LIVES_ENTRY (entry3),"string");
      break;
    case LIVES_PARAM_STRING_LIST:
      lives_entry_set_text (LIVES_ENTRY (entry3),"string_list");
      break;
    default:
      break;
    }

    lives_entry_set_editable (LIVES_ENTRY (entry3), FALSE);

    ebox3=lives_event_box_new();
    gtk_widget_set_events (ebox3, GDK_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox3),entry3);

    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox3, 2, 3, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    if (button==NULL) goto add_row_done;
    
    lives_widget_queue_resize(lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    lives_widget_destroy (param_dialog);
    break;


  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button!=NULL) {
      param_window_dialog=make_param_window_dialog(-1,rfxbuilder);
      if (lives_dialog_run (LIVES_DIALOG (param_window_dialog))==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy (param_window_dialog);
	return;
      }
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
      ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
      lives_entry_set_text (LIVES_ENTRY (entry),ctext);
      g_free(ctext);
      rfxbuilder->num_paramw_hints++;
    }
    else {
      gchar **array=g_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows],rfxbuilder->field_delim,2);
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
      lives_entry_set_text (LIVES_ENTRY (entry),array[0]);
      g_strfreev (array);
    }

    lives_entry_set_editable (LIVES_ENTRY (entry), FALSE);

    ebox=lives_event_box_new();
    gtk_widget_set_events (ebox, GDK_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox),entry);

    lives_table_resize (LIVES_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,2);
    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows-1] = gtk_entry_new ();

    if (button!=NULL) {
      if (!strcmp (lives_entry_get_text (LIVES_ENTRY (entry)),"layout")) {
	lives_entry_set_text (LIVES_ENTRY (entry2),lives_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)));
      }
      else {
	// TODO - use lives_rfx_special_t->has_subtype,name,num_params
	ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
	if (!strcmp (ctext,"framedraw")) {
	  gchar *ctext2=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
	  lives_entry_set_text (LIVES_ENTRY (entry2),(tmpx=g_strdup_printf ("%s%s%s%s%s",ctext,rfxbuilder->field_delim,ctext2,
									rfxbuilder->field_delim, 
									lives_entry_get_text (LIVES_ENTRY (rfxbuilder->paramw_rest_entry)))));
	  g_free(ctext2);
	}
	else {
	  lives_entry_set_text (LIVES_ENTRY (entry2),(tmpx=g_strdup_printf ("%s%s%s",ctext,rfxbuilder->field_delim,
									lives_entry_get_text (LIVES_ENTRY (rfxbuilder->paramw_rest_entry)))));
	}
	g_free(tmpx);
	g_free(ctext);
      }
    }
    else {
      gchar **array=g_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows-1],rfxbuilder->field_delim,2);
      lives_entry_set_text (LIVES_ENTRY (entry2),array[1]);
      g_strfreev (array);
    }

    lives_entry_set_editable (LIVES_ENTRY (entry2), FALSE);

    ebox2=lives_event_box_new();
    gtk_widget_set_events (ebox2, GDK_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox2),entry2);

    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox2, 1, 2, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);


    if (button==NULL) goto add_row_done;

    lives_widget_destroy (param_window_dialog);
    lives_widget_queue_resize (lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    break;


  case RFX_TABLE_TYPE_TRIGGERS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();

    if (button!=NULL) {
      trigger_dialog=make_trigger_dialog(-1,rfxbuilder);
      if (lives_dialog_run (LIVES_DIALOG (trigger_dialog))==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy (trigger_dialog);
	return;
      }
      lives_entry_set_text (LIVES_ENTRY (entry),lives_entry_get_text (GTK_ENTRY (rfxbuilder->trigger_when_entry)));
      rfxbuilder->num_triggers++;
    }
    else {
      gchar *tmpx2=NULL;
      lives_entry_set_text (LIVES_ENTRY (entry),rfxbuilder->triggers[rfxbuilder->table_rows].when?
			  (tmpx2=g_strdup_printf ("%d",rfxbuilder->triggers[rfxbuilder->table_rows].when-1)):"init");
      if (tmpx2!=NULL) g_free(tmpx2);
    }
    
    lives_entry_set_editable (LIVES_ENTRY (entry), FALSE);

    ebox=lives_event_box_new();
    gtk_widget_set_events (ebox, GDK_BUTTON_PRESS_MASK);
    lives_container_add(LIVES_CONTAINER(ebox),entry);

    lives_table_resize (LIVES_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,1);
    lives_table_attach (LIVES_TABLE (rfxbuilder->table), ebox, 0, 1, rfxbuilder->table_rows-1, 
		      rfxbuilder->table_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    if (button==NULL) goto add_row_done;

    rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].code=lives_text_view_get_text(LIVES_TEXT_VIEW (rfxbuilder->code_textview));
    
    rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when=atoi (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->trigger_when_entry)))+1;
    if (!strcmp(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->trigger_when_entry)),"init")) rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when=0;

    if (!rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when) {
      rfxbuilder->has_init_trigger=TRUE;
    }
    else {
      rfxbuilder->params[rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when-1].onchange=TRUE;
    }

    lives_widget_destroy (trigger_dialog);
    lives_widget_queue_resize (lives_widget_get_parent(LIVES_WIDGET(rfxbuilder->table)));
    break;
  default:
    return;
  }


 add_row_done:

  lives_widget_show_all(rfxbuilder->table);

  gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox),TRUE);

  g_signal_connect (GTK_OBJECT (ebox), "button_press_event",
		    G_CALLBACK (on_entry_click),
		    (gpointer)rfxbuilder);

  if (palette->style&STYLE_1) {
    if (palette->style&STYLE_3) {
      lives_widget_set_bg_color (entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
      lives_widget_set_fg_color (entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
    }
    else {
    lives_widget_set_bg_color (entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
    lives_widget_set_fg_color (entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
    }
  }

  if (entry2!=NULL) {
    gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox2),TRUE);

    g_signal_connect (GTK_OBJECT (ebox2), "button_press_event",
		      G_CALLBACK (on_entry_click),
		      (gpointer)rfxbuilder);

    if (palette->style&STYLE_1) {
      if (palette->style&STYLE_3) {
	lives_widget_set_bg_color (entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
	lives_widget_set_fg_color (entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
    }
      else {
	lives_widget_set_bg_color (entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
	lives_widget_set_fg_color (entry2, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
      }
    }
  }

  if (entry3!=NULL) {
    gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox3),TRUE);

    g_signal_connect (GTK_OBJECT (ebox3), "button_press_event",
		      G_CALLBACK (on_entry_click),
		      (gpointer)rfxbuilder);

    if (palette->style&STYLE_1) {
      if (palette->style&STYLE_3) {
	lives_widget_set_bg_color (entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
	lives_widget_set_fg_color (entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
    }
      else {
	lives_widget_set_bg_color (entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
	lives_widget_set_fg_color (entry3, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
      }
    }
  }

  if (button!=NULL) {
    table_select_row(rfxbuilder, rfxbuilder->table_rows-1);
    
    if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
      on_table_edit_row(NULL,user_data);
    }
  }

}



void param_set_from_dialog (lives_param_t *copy_param, rfx_build_window_t *rfxbuilder) {
  // set parameter values from param_dialog
  // this is called after adding a new copy_param or editing an existing one
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  copy_param->name=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->param_name_entry)));
  copy_param->label=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->param_label_entry)));
  
  if (!strcmp (ctext,"num")) {
    copy_param->type=LIVES_PARAM_NUM;
  }
  else if (!strcmp (ctext,"bool")) {
    copy_param->type=LIVES_PARAM_BOOL;
  }
  else if (!strcmp (ctext,"colRGB24")) {
    copy_param->type=LIVES_PARAM_COLRGB24;
  }
  else if (!strcmp (ctext,"string")) {
    copy_param->type=LIVES_PARAM_STRING;
  }
  else if (!strcmp (ctext,"string_list")) {
    copy_param->type=LIVES_PARAM_STRING_LIST;
  }

  g_free(ctext);

  copy_param->dp=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp));
  copy_param->group=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_group));
  copy_param->onchange=FALSE;  // no trigger assigned yet
  
  // TODO - check
  if (copy_param->type!=LIVES_PARAM_STRING&&copy_param->def!=NULL) g_free (copy_param->def);
  if (copy_param->type!=LIVES_PARAM_STRING_LIST&&copy_param->list!=NULL) g_list_free (copy_param->list);
  
  switch (copy_param->type) {
  case LIVES_PARAM_NUM:
    copy_param->min=lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min));
    copy_param->max=lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->step_size=lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step));
    copy_param->wrap=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton));
    if (!copy_param->dp) {
      copy_param->def=g_malloc (sizint);
      set_int_param (copy_param->def,lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    }
    else {
      copy_param->def=g_malloc (sizdbl);
      set_double_param (copy_param->def,lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    }
    break;
  case LIVES_PARAM_BOOL:
    copy_param->def=g_malloc (sizint);
    set_bool_param (copy_param->def,lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    copy_param->dp=0;
    break;
  case LIVES_PARAM_STRING:
    copy_param->max=lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->dp=0;
    break;
  case LIVES_PARAM_STRING_LIST:
    copy_param->dp=0;
    copy_param->def=g_malloc (sizdbl);
    set_int_param (copy_param->def,lives_list_index (copy_param->list,lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_def_combo))));
    break;
  case LIVES_PARAM_COLRGB24:
    copy_param->def=g_malloc (3*sizint);
    set_colRGB24_param (copy_param->def,lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)),
			lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)),
			lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    copy_param->dp=0;
    break;
  default:
    break;
  }
}





void on_table_edit_row (GtkButton *button, gpointer user_data) {
  GtkWidget *param_dialog;
  GtkWidget *paramw_dialog;
  GtkWidget *trigger_dialog;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  lives_param_t *param;

  int i;
  int found=-1;
  boolean param_ok=FALSE;

  gchar *tmpx;
  gchar *ctext;

  for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
    if (gtk_widget_is_sensitive(rfxbuilder->entry[i])) {
      found=i;
      break;
    }
  }
  if (found==-1) return;

  switch (rfxbuilder->table_type) {
    case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (found==i) { 
	lives_entry_set_editable (LIVES_ENTRY (rfxbuilder->entry[i]), TRUE);
	lives_widget_context_update();
	lives_widget_grab_focus (rfxbuilder->entry[i]);
      }
      else {
	lives_entry_set_editable (LIVES_ENTRY (rfxbuilder->entry[i]), FALSE);
      }
    }
    break;

  case RFX_TABLE_TYPE_PARAMS:
    param_dialog=make_param_dialog(found,rfxbuilder);
    do {
      if (lives_dialog_run (LIVES_DIALOG (param_dialog))==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy (param_dialog);
	return;
      }
      param_ok=perform_param_checks (rfxbuilder,found,rfxbuilder->num_params);
    } while (!param_ok);
    
    param_set_from_dialog ((param=&rfxbuilder->copy_params[found]),rfxbuilder);
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[found]),param->name);
    switch (param->type) {
    case LIVES_PARAM_NUM:
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry3[found]),(tmpx=g_strdup_printf ("num%d",param->dp)));
      g_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry3[found]),"bool");
      break;
    case LIVES_PARAM_COLRGB24:
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry3[found]),"colRGB24");
      break;
    case LIVES_PARAM_STRING:
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry3[found]),"string");
      break;
    case LIVES_PARAM_STRING_LIST:
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry3[found]),"string_list");
      break;
    default:
      break;
    }
    lives_widget_destroy (param_dialog);
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    paramw_dialog=make_param_window_dialog(found,rfxbuilder);
    if (lives_dialog_run (LIVES_DIALOG (paramw_dialog))==GTK_RESPONSE_CANCEL) {
      lives_widget_destroy (paramw_dialog);
      return;
    }
    ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry[found]),ctext);
    g_free(ctext);
    if (!strcmp (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[found])),"layout")) {
      lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[found]),lives_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)));
    }
    else {
      // TODO - use lives_rfx_special_t->has_subtype,name,num_params
      ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
      if (!strcmp (ctext,"framedraw")) {
	gchar *ctext2=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
	lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[found]),
			    (tmpx=g_strdup_printf ("%s%s%s%s%s",ctext,rfxbuilder->field_delim,ctext2,rfxbuilder->field_delim,
						   lives_entry_get_text (LIVES_ENTRY (rfxbuilder->paramw_rest_entry)))));
	g_free(ctext2);
      }
      else {
	lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[found]),
			    (tmpx=g_strdup_printf ("%s%s%s",ctext,rfxbuilder->field_delim,lives_entry_get_text 
						   (LIVES_ENTRY (rfxbuilder->paramw_rest_entry)))));
      }
      g_free(ctext);
      g_free(tmpx);
    }
    lives_widget_destroy (paramw_dialog);
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    trigger_dialog=make_trigger_dialog(found,rfxbuilder);
    if (lives_dialog_run (LIVES_DIALOG (trigger_dialog))==GTK_RESPONSE_CANCEL) {
      lives_widget_destroy (trigger_dialog);
      return;
    }

    g_free (rfxbuilder->copy_triggers[found].code);
    rfxbuilder->copy_triggers[found].code=lives_text_view_get_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview));

    lives_widget_destroy (trigger_dialog);
    break;
  }
}



void on_table_swap_row (GtkButton *button, gpointer user_data) {
  gchar *entry_text;
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int found=-1;

  register int i;

  for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
    if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
      found=i;
      break;
    }
  }

  if (found==-1) if ((found=rfxbuilder->table_swap_row1)==-1) return;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button==GTK_BUTTON (rfxbuilder->move_up_button)) {
      rfxbuilder->table_swap_row2=found-1;
    }
    else if (button==GTK_BUTTON (rfxbuilder->move_down_button)) {
      rfxbuilder->table_swap_row2=found+1;
    }
    if (rfxbuilder->table_swap_row2<0||rfxbuilder->table_swap_row2>=rfxbuilder->table_rows) return;

    entry_text=g_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry[found])));
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry[found]),lives_entry_get_text (GTK_ENTRY (rfxbuilder->entry[rfxbuilder->table_swap_row2])));
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry[rfxbuilder->table_swap_row2]),entry_text);
    g_free (entry_text);

    entry_text=g_strdup(lives_entry_get_text(LIVES_ENTRY(rfxbuilder->entry2[found])));
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[found]),lives_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[rfxbuilder->table_swap_row2])));
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->entry2[rfxbuilder->table_swap_row2]),entry_text);
    g_free (entry_text);

    break;
  default:
    break;
  }
  rfxbuilder->table_swap_row1=rfxbuilder->table_swap_row2;
  table_select_row(rfxbuilder,rfxbuilder->table_swap_row1);

}


void on_table_delete_row (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

#if !LIVES_TABLE_IS_GRID
  GtkWidget *ebox,*ebox2=NULL,*ebox3=NULL,*ebox4=NULL;
#endif

  int move=0;
  boolean triggers_adjusted=FALSE;

  register int i;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move>0) {

#if !LIVES_TABLE_IS_GRID
	if (i<rfxbuilder->table_rows-1) ebox2=lives_widget_get_parent(rfxbuilder->entry[i]);
	lives_widget_reparent(rfxbuilder->entry[i],ebox);
	ebox=ebox2;
#endif

	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];

      }
      else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {

#if LIVES_TABLE_IS_GRID
	if (rfxbuilder->table_rows>1) {
	  lives_grid_remove_row(LIVES_GRID(rfxbuilder->table),i);
	  // does this unref child widget ?
	}
#else
	ebox=lives_widget_get_parent(rfxbuilder->entry[i]);
	if (rfxbuilder->table_rows>1) {
	  lives_table_resize (LIVES_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
#endif
	lives_widget_destroy (rfxbuilder->entry[i]);

	move=i+1;
      }


    }
    if (move==0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_reqs--;
    break;

  case RFX_TABLE_TYPE_PARAMS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move>0) {
	// note - parameters become renumbered here

#if !LIVES_TABLE_IS_GRID
	if (i<rfxbuilder->table_rows-1) {
	  ebox3=lives_widget_get_parent(rfxbuilder->entry2[i]);
	  ebox4=lives_widget_get_parent(rfxbuilder->entry3[i]);
	}
	lives_widget_reparent(rfxbuilder->entry2[i],ebox);
	lives_widget_reparent(rfxbuilder->entry3[i],ebox2);
	ebox=ebox3;
	ebox2=ebox4;
#endif

	rfxbuilder->entry2[i-1]=rfxbuilder->entry2[i];
	rfxbuilder->entry3[i-1]=rfxbuilder->entry3[i];
	param_copy (&rfxbuilder->copy_params[i],&rfxbuilder->copy_params[i-1],FALSE);
	g_free (rfxbuilder->copy_params[i].name);
	g_free (rfxbuilder->copy_params[i].label);
	g_free (rfxbuilder->copy_params[i].def);

      }
      else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
	if (rfxbuilder->copy_params[i].onchange) {
	  do_blocking_error_dialog(_ ("\n\nCannot remove this parameter as it has a trigger.\nPlease remove the trigger first.\n\n"));
	  return;
	}

#if LIVES_TABLE_IS_GRID
	if (rfxbuilder->table_rows>1) {
	  lives_grid_remove_row(LIVES_GRID(rfxbuilder->table),i);
	}
#else
	ebox=lives_widget_get_parent(rfxbuilder->entry2[i]);
	ebox2=lives_widget_get_parent(rfxbuilder->entry3[i]);

	lives_widget_destroy (rfxbuilder->entry[rfxbuilder->table_rows-1]);
	lives_widget_destroy (rfxbuilder->entry2[i]);
	lives_widget_destroy (rfxbuilder->entry3[i]);

	if (rfxbuilder->table_rows>1) {
	  lives_table_resize (LIVES_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,3);
      }
#endif
	g_free (rfxbuilder->copy_params[i].name);
	g_free (rfxbuilder->copy_params[i].label);
	g_free (rfxbuilder->copy_params[i].def);

	move=i+1;
      }
    }
    if (move==0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_params--;
    for (i=0;i<rfxbuilder->num_triggers;i++) {
      if (rfxbuilder->triggers[i].when>move) {
	rfxbuilder->params[rfxbuilder->triggers[i].when-1].onchange=FALSE;
	rfxbuilder->params[--rfxbuilder->triggers[i].when-1].onchange=TRUE;
      }
      triggers_adjusted=TRUE;
    }
    if (triggers_adjusted) {
      do_blocking_error_dialog (_ ("\n\nSome triggers were adjusted.\nPlease check the trigger code.\n"));
    }
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move>0) {

#if !LIVES_TABLE_IS_GRID
	if (i<rfxbuilder->table_rows-1) {
	  ebox3=lives_widget_get_parent(rfxbuilder->entry[i]);
	  ebox4=lives_widget_get_parent(rfxbuilder->entry2[i]);
	}
	lives_widget_reparent(rfxbuilder->entry[i],ebox);
	lives_widget_reparent(rfxbuilder->entry2[i],ebox2);
	ebox=ebox3;
	ebox2=ebox4;
#endif

	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
	rfxbuilder->entry2[i-1]=rfxbuilder->entry2[i];
      }
      else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {

#if LIVES_TABLE_IS_GRID
	if (rfxbuilder->table_rows>1) {
	  lives_grid_remove_row(LIVES_GRID(rfxbuilder->table),i);
	}
#else
	ebox=lives_widget_get_parent(rfxbuilder->entry[i]);
	ebox2=lives_widget_get_parent(rfxbuilder->entry2[i]);

	lives_widget_destroy (rfxbuilder->entry[i]);
	lives_widget_destroy (rfxbuilder->entry2[i]);
	if (rfxbuilder->table_rows>1) {
	  lives_table_resize (LIVES_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
#endif
	move=i+1;
      }
    }
    if (move==0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_paramw_hints--;
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move>0) {

#if !LIVES_TABLE_IS_GRID
	if (i<rfxbuilder->table_rows-1) ebox2=lives_widget_get_parent(rfxbuilder->entry[i]);
	lives_widget_reparent(rfxbuilder->entry[i],ebox);
	ebox=ebox2;
#endif

	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
	rfxbuilder->copy_triggers[i-1].when=rfxbuilder->copy_triggers[i].when;
	rfxbuilder->copy_triggers[i-1].code=g_strdup (rfxbuilder->copy_triggers[i].code);
	g_free (rfxbuilder->copy_triggers[i].code);
      }
      else if (!(lives_widget_get_state(rfxbuilder->entry[i])&LIVES_WIDGET_STATE_INSENSITIVE)) {
	int when=atoi (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[i])))+1;

	if (!strcmp(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[i])),"init")) rfxbuilder->has_init_trigger=FALSE;
	else rfxbuilder->params[when-1].onchange=FALSE;

	g_free (rfxbuilder->copy_triggers[i].code);

#if LIVES_TABLE_IS_GRID
	if (rfxbuilder->table_rows>1) {
	  lives_grid_remove_row(LIVES_GRID(rfxbuilder->table),i);
	}
#else
	ebox=lives_widget_get_parent(rfxbuilder->entry[i]);
	if (rfxbuilder->table_rows>1) {
	  lives_table_resize (LIVES_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
#endif
	lives_widget_destroy (rfxbuilder->entry[i]);
	move=i+1;
      }
    }
    if (move==0) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_triggers--;
    break;
  default:
    return;
  }

  if (rfxbuilder->table_rows==0) {
    lives_widget_set_sensitive(rfxbuilder->edit_entry_button,FALSE);
    lives_widget_set_sensitive(rfxbuilder->remove_entry_button,FALSE);
    if (rfxbuilder->move_up_button!=NULL) lives_widget_set_sensitive(rfxbuilder->move_up_button,FALSE);
    if (rfxbuilder->move_down_button!=NULL) lives_widget_set_sensitive(rfxbuilder->move_down_button,FALSE);
  }
  else {
    if (move>=rfxbuilder->table_rows) move=rfxbuilder->table_rows-1;
    table_select_row(rfxbuilder,move);
  }

}


GtkWidget * make_param_dialog (int pnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(lives_accel_group_new ());

  GList *typelist=NULL;

  lives_colRGB24_t rgb;

  gchar *tmp,*tmp2,*title;

  if (pnum<0) {
    title=g_strdup(_("LiVES: - New RFX Parameter"));
  }
  else {
    title=g_strdup(_("LiVES: - Edit RFX Parameter"));
  }

  dialog = lives_standard_dialog_new (title,TRUE);
  g_free(title);

  lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // name

  rfxbuilder->param_name_entry = lives_standard_entry_new ((tmp=g_strdup(_("_Name:    "))),TRUE,
							   pnum>=0?rfxbuilder->copy_params[pnum].name:NULL,
							   60.*widget_opts.scale,-1,LIVES_BOX(dialog_vbox),
							   (tmp2=g_strdup((_("Name of the parameter, must be unique in the plugin.")))));

  if (pnum<0) lives_widget_grab_focus(rfxbuilder->param_name_entry);


  g_free(tmp); g_free(tmp2);

  // label

  rfxbuilder->param_label_entry = lives_standard_entry_new ((tmp=g_strdup(_("_Label:    "))),TRUE,
							   pnum>=0?rfxbuilder->copy_params[pnum].label:NULL,
							   60.*widget_opts.scale,-1,LIVES_BOX(dialog_vbox),
							    (tmp2=g_strdup(_("Label to be shown by the parameter. An underscore represents mnemonic accelerator."))));
  g_free(tmp); g_free(tmp2);

  // type

  typelist = g_list_append (typelist, (gpointer)"num");
  typelist = g_list_append (typelist, (gpointer)"bool");
  typelist = g_list_append (typelist, (gpointer)"string");
  typelist = g_list_append (typelist, (gpointer)"colRGB24");
  typelist = g_list_append (typelist, (gpointer)"string_list");

  rfxbuilder->param_type_combo = lives_standard_combo_new ((tmp=g_strdup(_("_Type:         "))),TRUE,typelist,
							   LIVES_BOX(dialog_vbox),(tmp2=g_strdup(_ ("Parameter type (select from list)."))));
  g_free(tmp);
  g_free(tmp2);
  g_list_free(typelist);

  rfxbuilder->edit_param=pnum;

  if (pnum>=0) {
    switch(rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo),0);
      break;
    case LIVES_PARAM_BOOL:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo),1);
      break;
    case LIVES_PARAM_STRING:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo),2);
      break;
    case LIVES_PARAM_COLRGB24:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo),3);
      break;
    case LIVES_PARAM_STRING_LIST:
      lives_combo_set_active_index(LIVES_COMBO(rfxbuilder->param_type_combo),4);
      break;
    default:
      break;
    }
  }
  else rfxbuilder->edit_param=rfxbuilder->num_params;


  // dp

  rfxbuilder->spinbutton_param_dp = lives_standard_spin_button_new (_("Decimal _places: "),TRUE,pnum>=0?rfxbuilder->copy_params[pnum].dp:0.,
								    0., 16., 1., 1., 0,
								    LIVES_BOX(dialog_vbox),NULL);
  rfxbuilder->param_dp_label = widget_opts.last_label;

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  // default val

  rfxbuilder->spinbutton_param_def = lives_standard_spin_button_new (_("_Default value:    "),TRUE, 0., -G_MAXINT, G_MAXINT, 1., 1., 0,
								     LIVES_BOX(dialog_vbox),NULL);
  rfxbuilder->param_def_label = widget_opts.last_label;

  // extra bits for string/string_list

  rfxbuilder->param_strdef_hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), rfxbuilder->param_strdef_hbox, TRUE, TRUE, widget_opts.packing_height);

  rfxbuilder->param_strdef_button = gtk_button_new();
  gtk_button_set_use_underline (GTK_BUTTON (rfxbuilder->param_strdef_button),TRUE);
  lives_box_pack_start (LIVES_BOX (rfxbuilder->param_strdef_hbox), rfxbuilder->param_strdef_button, TRUE, TRUE, widget_opts.packing_width);

  rfxbuilder->param_strlist_hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), rfxbuilder->param_strlist_hbox, FALSE, FALSE, widget_opts.packing_width);

  rfxbuilder->param_def_combo = lives_standard_combo_new (_("_Default: "),TRUE,NULL,LIVES_BOX(rfxbuilder->param_strlist_hbox),NULL);

  gtk_label_set_mnemonic_widget (LIVES_LABEL (rfxbuilder->param_def_label),rfxbuilder->param_def_combo);

  if (pnum>=0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      if (!rfxbuilder->copy_params[pnum].dp) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				   (double)get_int_param (rfxbuilder->copy_params[pnum].def));
      }
      else {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				   get_double_param (rfxbuilder->copy_params[pnum].def));
      }
      break;
    case LIVES_PARAM_BOOL:
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				 (double)get_bool_param (rfxbuilder->copy_params[pnum].def));
      break;
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param (rfxbuilder->copy_params[pnum].def,&rgb);
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),(double)rgb.red);
      break;
    default:
      break;
    }
  }

  // group

  rfxbuilder->hbox_bg = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), rfxbuilder->hbox_bg, FALSE, FALSE, widget_opts.packing_height);

  rfxbuilder->spinbutton_param_group = lives_standard_spin_button_new ((tmp=g_strdup(_("Button _Group: "))),TRUE,0., 0., 16., 1., 1., 0,
								       LIVES_BOX(rfxbuilder->hbox_bg),
								       (tmp2=g_strdup(_("A non-zero value can be used to group radio buttons."))));

  if (pnum>=0) {
    lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_group),(double)rfxbuilder->copy_params[pnum].group);
  }

  // min

  rfxbuilder->spinbutton_param_min = lives_standard_spin_button_new (_("_Minimum value: "),TRUE,0., -G_MAXINT, G_MAXINT, 1., 1., 0,
								     LIVES_BOX(dialog_vbox),NULL);
  rfxbuilder->param_min_label = widget_opts.last_label;

  // max

  rfxbuilder->spinbutton_param_max = lives_standard_spin_button_new (_("Ma_ximum value: "),TRUE, RFX_DEF_NUM_MAX, -G_MAXINT, G_MAXINT, 1., 1., 0,
								     LIVES_BOX(dialog_vbox),NULL);
  rfxbuilder->param_max_label = widget_opts.last_label;


  // step size



  rfxbuilder->spinbutton_param_step = lives_standard_spin_button_new ((tmp=g_strdup(_("     _Step size:   "))),TRUE,1., 1., G_MAXINT, 1., 1., 0,
								      LIVES_BOX(dialog_vbox),
								      (tmp2=g_strdup(_(
										       "How much the parameter is adjusted when the spinbutton arrows are pressed."
										       ))));
  rfxbuilder->param_step_label = widget_opts.last_label;
  g_free(tmp); g_free(tmp2);


  // wrap

  rfxbuilder->param_wrap_hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), rfxbuilder->param_wrap_hbox, TRUE, TRUE, widget_opts.packing_height);

  rfxbuilder->param_wrap_checkbutton = lives_standard_check_button_new ((tmp=g_strdup(_("_Wrap value"))),TRUE,LIVES_BOX(rfxbuilder->param_wrap_hbox),
									(tmp2=g_strdup(_ ("Whether the value wraps max->min and min->max."))));

  g_free(tmp); g_free(tmp2);



  g_signal_connect (GTK_OBJECT (rfxbuilder->param_strdef_button), "clicked",
		    G_CALLBACK (on_code_clicked),
		    (gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->param_type_combo),"changed",G_CALLBACK (on_param_type_changed),
		    (gpointer)rfxbuilder);

  g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_dp), "value_changed",
			  G_CALLBACK (after_param_dp_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->def_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_def), "value_changed",
			  G_CALLBACK (after_param_def_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->min_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_min), "value_changed",
			  G_CALLBACK (after_param_min_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->max_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_max), "value_changed",
			  G_CALLBACK (after_param_max_changed),
			  (gpointer)rfxbuilder);


  if (pnum>=0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),rfxbuilder->copy_params[pnum].min);
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),rfxbuilder->copy_params[pnum].max);
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_step),
				 rfxbuilder->copy_params[pnum].step_size);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton),
				     rfxbuilder->copy_params[pnum].wrap);
      break;
    case LIVES_PARAM_COLRGB24:
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),(double)rgb.green);
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),(double)rgb.blue);
      break;
    case LIVES_PARAM_STRING:
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),rfxbuilder->copy_params[pnum].max);
      break;
    default:
      break;
    }
  }
  lives_widget_show_all (dialog);
  on_param_type_changed (LIVES_COMBO (rfxbuilder->param_type_combo),(gpointer)rfxbuilder);
  after_param_dp_changed(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp),(gpointer)rfxbuilder);
  return dialog;
}


void after_param_dp_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));
  int dp;

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  dp=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (spinbutton));

  lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),dp);
  lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),dp);
  lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),dp);
  lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_step),dp);

  if (dp>0) {
    double max=lives_spin_button_get_value(LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));
    double min=lives_spin_button_get_value(LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),-G_MAXFLOAT,G_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXFLOAT,G_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),-G_MAXFLOAT,G_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1./(double)lives_10pow(dp),(max-min)>1.?(max-min):1.);

    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_step),MAXFLOATLEN+dp);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_def),MAXFLOATLEN+dp);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_min),MAXFLOATLEN+dp);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_max),MAXFLOATLEN+dp);
  }
  else {
    int max=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));
    int min=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),-G_MAXINT,G_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXINT,G_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),-G_MAXINT,G_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,(max-min)>1?(max-min):1);
    
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_step),MAXINTLEN);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_def),MAXINTLEN);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_min),MAXINTLEN);
    lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_max),MAXINTLEN);
  }
}


void after_param_min_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  int dp;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  g_signal_handler_block (rfxbuilder->spinbutton_param_max,rfxbuilder->max_spin_f);
  g_signal_handler_block (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

  if ((dp=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)))>0) {
    if (lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def))<
	lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
				lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
			      lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),G_MAXFLOAT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
			      1./(double)lives_10pow(dp),lives_spin_button_get_value 
			      (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));

  }
  else {
    if (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def))<
	lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
				lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
			      lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      G_MAXINT);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,
			      lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));

  }

  g_signal_handler_unblock (rfxbuilder->spinbutton_param_max,rfxbuilder->max_spin_f);
  g_signal_handler_unblock (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

}

void after_param_max_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  int dp;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  g_signal_handler_block (rfxbuilder->spinbutton_param_min,rfxbuilder->min_spin_f);
  g_signal_handler_block (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

  if ((dp=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)))>0) {
    if (lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def))>
	lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
				lives_spin_button_get_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXFLOAT,
			      lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
			      1./(double)lives_10pow(dp),
			      lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));

  }
  else {
    if (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def))>
	lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
				lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    }

    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXINT,
			      lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,
			      lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))
			      -lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));

  }

  g_signal_handler_unblock (rfxbuilder->spinbutton_param_min,rfxbuilder->min_spin_f);
  g_signal_handler_unblock (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

}

void after_param_def_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  if (lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp))) {
    double dbl_def=lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
    if (dbl_def<lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),dbl_def);
    }
    else if (dbl_def>lives_spin_button_get_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),dbl_def);
    }
  }
  else {
    int int_def=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
    if (int_def<lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min))) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),(double)int_def);
    }
    else if (int_def>lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),(double)int_def);
    }
  }
}


void on_param_type_changed (GtkComboBox *param_type_combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  int pnum=rfxbuilder->edit_param;

  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  lives_widget_show (rfxbuilder->param_min_label);
  lives_widget_show (rfxbuilder->param_max_label);
  lives_widget_show (rfxbuilder->spinbutton_param_min);
  lives_widget_show (rfxbuilder->spinbutton_param_max);
  lives_widget_show (rfxbuilder->spinbutton_param_dp);
  lives_widget_show (rfxbuilder->spinbutton_param_def);
  lives_widget_show (rfxbuilder->param_dp_label);
  lives_widget_show (rfxbuilder->param_def_label);

  lives_widget_hide (rfxbuilder->param_strdef_hbox);
  lives_widget_hide (rfxbuilder->param_strlist_hbox);
  lives_widget_hide (rfxbuilder->hbox_bg);
  lives_widget_hide (rfxbuilder->spinbutton_param_step);
  lives_widget_hide (rfxbuilder->param_step_label);
  lives_widget_hide (rfxbuilder->param_wrap_hbox);

  if (pnum<0) return;

  if (!strcmp (ctext,"string_list")) {
    int defindex;

    if (rfxbuilder->copy_params[pnum].type!=LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[pnum].def!=NULL) {
	g_free (rfxbuilder->copy_params[pnum].def);
      }
      rfxbuilder->copy_params[pnum].def=g_malloc (sizint);
      set_int_param (rfxbuilder->copy_params[pnum].def,0);
    }

    rfxbuilder->copy_params[pnum].type=LIVES_PARAM_STRING_LIST;
    defindex=get_int_param (rfxbuilder->copy_params[pnum].def);

    lives_combo_populate (LIVES_COMBO (rfxbuilder->param_def_combo), rfxbuilder->copy_params[pnum].list);

    if (rfxbuilder->copy_params[pnum].list==NULL||defindex>g_list_length (rfxbuilder->copy_params[pnum].list)) {
      set_int_param (rfxbuilder->copy_params[pnum].def,(defindex=0));
    }

    if (rfxbuilder->copy_params[pnum].list!=NULL) {
      lives_combo_set_active_string (LIVES_COMBO(rfxbuilder->param_def_combo), 
				     (gchar *)g_list_nth_data (rfxbuilder->copy_params[pnum].list,defindex));
    }

    lives_widget_hide (rfxbuilder->spinbutton_param_dp);
    lives_widget_hide (rfxbuilder->param_dp_label);
    lives_widget_hide (rfxbuilder->spinbutton_param_min);
    lives_widget_hide (rfxbuilder->param_min_label);
    lives_widget_hide (rfxbuilder->spinbutton_param_def);
    lives_widget_hide (rfxbuilder->param_def_label);
    lives_widget_hide (rfxbuilder->spinbutton_param_max);
    lives_widget_hide (rfxbuilder->param_max_label);

    lives_button_set_label (GTK_BUTTON (rfxbuilder->param_strdef_button),(_("Set _values")));
    lives_widget_show (rfxbuilder->param_strdef_hbox);
    lives_widget_show (rfxbuilder->param_strlist_hbox);
  }
  else {
    if (!strcmp (ctext,"num")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_NUM;
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_def_label),(_("_Default value:    ")));
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_min_label),(_("_Minimum value: ")));
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_max_label),(_("Ma_ximum value: ")));
      lives_widget_show (rfxbuilder->spinbutton_param_step);
      lives_widget_show (rfxbuilder->param_step_label);
      lives_widget_show_all (rfxbuilder->param_wrap_hbox);

      after_param_dp_changed(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp),(gpointer)rfxbuilder);

      if (pnum<0) {
	lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),0.);
	lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),RFX_DEF_NUM_MAX);
	lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),0.);
	lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1.);
      }
    }
    else if (!strcmp (ctext,"bool")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_BOOL;
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_def_label),(_("_Default value:    ")));
      lives_widget_hide (rfxbuilder->param_min_label);
      lives_widget_hide (rfxbuilder->param_max_label);
      lives_widget_hide (rfxbuilder->spinbutton_param_min);
      lives_widget_hide (rfxbuilder->spinbutton_param_max);
      lives_widget_hide (rfxbuilder->spinbutton_param_dp);
      lives_widget_hide (rfxbuilder->param_dp_label);
      lives_widget_show (rfxbuilder->hbox_bg);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),0,1);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),0);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
    }
    else if (!strcmp (ctext,"colRGB24")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_COLRGB24;
      lives_widget_hide (rfxbuilder->spinbutton_param_dp);
      lives_widget_hide (rfxbuilder->param_dp_label);
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_def_label),(_("Default _Red:  ")));
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_min_label),(_("Default _Green:")));
      lives_label_set_text_with_mnemonic (LIVES_LABEL (rfxbuilder->param_max_label),(_("Default _Blue: ")));
      lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_def),4);
      lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_min),4);
      lives_entry_set_width_chars (LIVES_ENTRY (rfxbuilder->spinbutton_param_max),4);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),0,255);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),0,255);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),0,255);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),0);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
      if (pnum<0) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0.);
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),0.);
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0.);
      }
    }
    else if (!strcmp (ctext,"string")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_STRING;
      lives_widget_hide (rfxbuilder->spinbutton_param_dp);
      lives_widget_hide (rfxbuilder->param_dp_label);
      lives_widget_hide (rfxbuilder->spinbutton_param_min);
      lives_widget_hide (rfxbuilder->param_min_label);
      lives_widget_hide (rfxbuilder->spinbutton_param_def);
      
      lives_button_set_label (GTK_BUTTON (rfxbuilder->param_strdef_button),(_("Set _default")));
      lives_widget_show (rfxbuilder->param_strdef_hbox);
      lives_label_set_text (LIVES_LABEL (rfxbuilder->param_def_label),(_ ("Default value:  ")));
      lives_label_set_text (LIVES_LABEL (rfxbuilder->param_max_label),(_ ("Maximum length (chars): ")));
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),1,RFX_MAXSTRINGLEN);
      if (pnum<0) lives_spin_button_set_value(LIVES_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),RFX_TEXT_MAGIC);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      lives_spin_button_set_digits (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
      
      if (rfxbuilder->copy_params[pnum].def==NULL) {
	rfxbuilder->copy_params[pnum].def=g_strdup ("");
      }
    }
  }

  g_free(ctext);
}






GtkWidget * make_param_window_dialog (int pnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;

  GList *kwlist=NULL;
  GList *splist=NULL;
  GList *spsublist=NULL;

  gchar *kw=NULL;
  gchar *sp=NULL;
  gchar *spsub=NULL;
  gchar *rest=NULL;
  gchar *title;

  if (pnum<0) {
    title=g_strdup(_("LiVES: - New RFX Parameter Window Hint"));
  }
  else {
    title=g_strdup(_("LiVES: - Edit RFX Parameter Window Hint"));
  }

  dialog = lives_standard_dialog_new (title,TRUE);
  g_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  if (pnum>=0) {
    kw=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry[pnum])));
    if (!strcmp (kw,"layout")) {
      rest=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry2[pnum])));
    }
    else if (!strcmp (kw,"special")) {
      int numtok=get_token_count (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry2[pnum])),
				   (int)rfxbuilder->field_delim[0]);
      gchar **array=g_strsplit(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->entry2[pnum])),rfxbuilder->field_delim,3);
      sp=g_strdup (array[0]);
      if (!strcmp (sp,"framedraw")) {
	if (numtok>1) {
	  spsub=g_strdup (array[1]);
	}
	if (numtok>2) {
	  rest=g_strdup (array[2]);
	}
      }
      else {
	if (numtok>1) {
	  rest=g_strdup (array[1]);
	}
      }
      g_strfreev (array);
    }
  }

  // kw
  kwlist = g_list_append (kwlist, (gpointer)"layout");
  kwlist = g_list_append (kwlist, (gpointer)"special");

  rfxbuilder->paramw_kw_combo = lives_standard_combo_new (_("_Keyword:         "),TRUE,kwlist,LIVES_BOX(dialog_vbox),NULL);
  g_list_free(kwlist);

  if (pnum>=0&&kw!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_kw_combo),kw);
  }


  // type
  splist = g_list_append (splist, (gpointer)"aspect");
  splist = g_list_append (splist, (gpointer)"framedraw");
  splist = g_list_append (splist, (gpointer)"fileread");
  splist = g_list_append (splist, (gpointer)"password");
  if (rfxbuilder->type==RFX_BUILD_TYPE_EFFECT2) {
    splist = g_list_append (splist, (gpointer)"mergealign");
  }

  rfxbuilder->paramw_sp_combo = lives_standard_combo_new (_("Special _Type:         "),TRUE,splist,LIVES_BOX(dialog_vbox),NULL);
  g_list_free(splist);

  if (pnum>=0&&sp!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_sp_combo),sp);
  }

  // subtype

  spsublist = g_list_append (spsublist, (gpointer)"rectdemask");
  if (pnum>=0&&spsub!=NULL) {
    // deprecated value
    if (!strcmp(spsub,"multrect")) spsublist = g_list_append (spsublist, (gpointer)"multrect");
  }
  else spsublist = g_list_append (spsublist, (gpointer)"multirect");
  spsublist = g_list_append (spsublist, (gpointer)"singlepoint");

  rfxbuilder->paramw_spsub_combo = lives_standard_combo_new (_("Special _Subtype:         "),TRUE,spsublist,LIVES_BOX(dialog_vbox),NULL);
  g_list_free(spsublist);

  if (pnum>=0&&spsub!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_spsub_combo),spsub);
  }


  // paramwindow rest


  rfxbuilder->paramw_rest_entry = lives_standard_entry_new (_("Row:    "),TRUE,pnum>=0&&rest!=NULL?rest:NULL,
							    -1,-1,LIVES_BOX(dialog_vbox),NULL);
  rfxbuilder->paramw_rest_label=widget_opts.last_label;
  
  if (kw!=NULL) g_free (kw);
  if (sp!=NULL) g_free (sp);
  if (spsub!=NULL) g_free (spsub);
  if (rest!=NULL) g_free (rest);

  lives_widget_grab_focus (rfxbuilder->paramw_rest_entry);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_kw_combo),"changed",
		    G_CALLBACK (on_paramw_kw_changed),(gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_sp_combo),"changed",
		    G_CALLBACK (on_paramw_sp_changed),(gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_spsub_combo),"changed",
		    G_CALLBACK (on_paramw_spsub_changed),(gpointer)rfxbuilder);

  lives_widget_show_all(dialog);
  on_paramw_kw_changed (LIVES_COMBO (rfxbuilder->paramw_kw_combo),(gpointer)rfxbuilder);

  return dialog;
}



void on_paramw_kw_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"special")) {
    lives_widget_show_all (lives_widget_get_parent(rfxbuilder->paramw_sp_combo));
    on_paramw_sp_changed (LIVES_COMBO(rfxbuilder->paramw_sp_combo),(gpointer)rfxbuilder);
    lives_widget_grab_focus (lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_sp_combo)));
  }
  else {
    lives_label_set_text (LIVES_LABEL (rfxbuilder->paramw_rest_label),(_("Row:    ")));
    lives_widget_hide (lives_widget_get_parent(rfxbuilder->paramw_sp_combo));
    lives_widget_hide (lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    lives_widget_grab_focus (rfxbuilder->paramw_rest_entry);
  }

  g_free(ctext);
}


void on_paramw_sp_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  int npars;
  gchar *tmpx;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"framedraw")) {
    lives_widget_show_all (lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    on_paramw_spsub_changed (LIVES_COMBO(rfxbuilder->paramw_spsub_combo),(gpointer)rfxbuilder);
    lives_widget_grab_focus (lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_spsub_combo)));
  }
  else {
    if (!strcmp(ctext,"fileread")||!strcmp(ctext,"password")) npars=1;
    else npars=2;
    lives_label_set_text (LIVES_LABEL (rfxbuilder->paramw_rest_label),
			(tmpx=g_strdup_printf(_("Linked parameters (%d):    "),npars)));
    g_free(tmpx);
    lives_widget_hide (lives_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    lives_widget_grab_focus (rfxbuilder->paramw_rest_entry);
  }

  g_free(ctext);
}


void on_paramw_spsub_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"rectdemask")||
      !strcmp (ctext,"multrect")||!strcmp(ctext,"multirect")) {
    lives_label_set_text (LIVES_LABEL (rfxbuilder->paramw_rest_label),(_("Linked parameters (4):    ")));
  }
  else if (!strcmp (ctext,"singlepoint")) {
    lives_label_set_text (LIVES_LABEL (rfxbuilder->paramw_rest_label),(_("Linked parameters (2):    ")));
  }

  g_free(ctext);
}





GtkWidget * make_trigger_dialog (int tnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *combo;
  GtkWidget *scrolledwindow;

  GList *whenlist=NULL;

  gchar *title;

  boolean woat;

  register int i;

  if (tnum<0) {
    title=g_strdup(_("LiVES: - New RFX Trigger"));
  }
  else {
    title=g_strdup(_("LiVES: - Edit RFX Trigger"));
  }

  dialog = lives_standard_dialog_new (title,TRUE);
  g_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // when

  if (tnum>=0) whenlist = g_list_append (whenlist, (gpointer)(rfxbuilder->copy_triggers[tnum].when?
							      g_strdup_printf ("%d",rfxbuilder->copy_triggers[tnum].
									       when-1):"init"));
  else {
    if (!rfxbuilder->has_init_trigger) {
      whenlist = g_list_append (whenlist, (gpointer)"init");
    }
    for (i=0;i<rfxbuilder->num_params;i++) {
      if (!rfxbuilder->params[i].onchange) {
	whenlist = g_list_append (whenlist, g_strdup_printf ("%d",i));
      }
    }
  }

  combo = lives_standard_combo_new (_("When:         "),FALSE,whenlist,LIVES_BOX(dialog_vbox),FALSE);
  g_list_free(whenlist);

  rfxbuilder->trigger_when_entry=lives_combo_get_entry(LIVES_COMBO(combo));


  // code area
  rfxbuilder->code_textview = lives_text_view_new ();
  lives_text_view_set_editable (LIVES_TEXT_VIEW (rfxbuilder->code_textview), TRUE);
  lives_text_view_set_wrap_mode (LIVES_TEXT_VIEW (rfxbuilder->code_textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible (LIVES_TEXT_VIEW (rfxbuilder->code_textview), TRUE);

  woat=widget_opts.apply_theme;
  widget_opts.apply_theme=FALSE;
  widget_opts.expand=LIVES_EXPAND_NONE; // prevent centering
  scrolledwindow = lives_standard_scrolled_window_new (RFX_WINSIZE_H*2./3.,RFX_WINSIZE_V/4.,rfxbuilder->code_textview);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;
  widget_opts.apply_theme=woat;

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->white);
    lives_widget_set_text_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->black);
  }

  lives_box_pack_start (LIVES_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  if (tnum>=0) {
    lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), 
			rfxbuilder->copy_triggers[tnum].code,-1);
  }

  if (tnum>=0||rfxbuilder->num_params==0) lives_widget_grab_focus(rfxbuilder->code_textview);

  lives_widget_show_all(dialog);
    
  return dialog;
}




void on_code_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *scrolledwindow;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  boolean woat;

  gchar *tmpx;

  dialog = lives_standard_dialog_new (NULL,FALSE);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // code area
  rfxbuilder->code_textview = lives_text_view_new ();

  woat=widget_opts.apply_theme;
  widget_opts.apply_theme=FALSE;
  widget_opts.expand=LIVES_EXPAND_NONE; // prevent centering
  scrolledwindow = lives_standard_scrolled_window_new (RFX_WINSIZE_H,RFX_WINSIZE_V,rfxbuilder->code_textview);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;
  widget_opts.apply_theme=woat;

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->white);
    lives_widget_set_text_color(rfxbuilder->code_textview, LIVES_WIDGET_STATE_NORMAL, &palette->black);
  }

  lives_box_pack_start (LIVES_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  g_object_ref (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolledwindow)));
  g_object_ref (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolledwindow)));

  lives_text_view_set_editable (LIVES_TEXT_VIEW (rfxbuilder->code_textview), TRUE);
  lives_text_view_set_wrap_mode (LIVES_TEXT_VIEW (rfxbuilder->code_textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible (LIVES_TEXT_VIEW (rfxbuilder->code_textview), TRUE);

  lives_widget_grab_focus (rfxbuilder->code_textview);

  // TODO !!
  /*  if (glib_major_version>=2&&glib_minor_version>=4) {
      lives_text_view_set_accepts_tab (GTK_TEXT_VIEW (rfxbuilder->code_textview),TRUE);
    } */

  if (button==GTK_BUTTON (rfxbuilder->pre_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_PRE;
    lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - Pre Loop Code"));
    lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), 
			rfxbuilder->pre_code,-1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->loop_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_LOOP;
    lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - Loop Code"));
    lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), 
			rfxbuilder->loop_code,-1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->post_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_POST;
    lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - Post Loop Code"));
    lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), 
			rfxbuilder->post_code,-1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->param_strdef_button)) {
    if (rfxbuilder->copy_params[rfxbuilder->edit_param].type!=LIVES_PARAM_STRING_LIST) {
      int len,maxlen=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));

      if ((len=strlen ((gchar *)rfxbuilder->copy_params[rfxbuilder->edit_param].def))>maxlen) len=maxlen;
      
      rfxbuilder->codetype=RFX_CODE_TYPE_STRDEF;
      lives_window_set_title (LIVES_WINDOW (dialog), (tmpx=g_strdup_printf 
						  (_("LiVES: - Default text (max length %d)"),maxlen)));
      g_free(tmpx);
      lives_text_view_set_wrap_mode (LIVES_TEXT_VIEW (rfxbuilder->code_textview),LIVES_WRAP_WORD);

      lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), 
			  (gchar *)rfxbuilder->copy_params[rfxbuilder->edit_param].def,len);
    }
    else {
      LiVESTextIter start_iter;
      gchar *string=g_strdup (""),*string_new;
      register int i;

      rfxbuilder->codetype=RFX_CODE_TYPE_STRING_LIST;
      lives_window_set_title (LIVES_WINDOW (dialog), (tmpx=g_strdup (_("LiVES: - Enter values, one per line"))));
      g_free(tmpx);
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	for (i=0;i<g_list_length (rfxbuilder->copy_params[rfxbuilder->edit_param].list);i++) {
	  string_new=g_strconcat (string, (gchar *)g_list_nth_data (rfxbuilder->copy_params[rfxbuilder->edit_param].list,i),
				  "\n",NULL);
	  if (string!=string_new) g_free (string);
	  string=string_new;
	}
	lives_text_view_set_text (LIVES_TEXT_VIEW (rfxbuilder->code_textview), string, strlen(string)-1);
	g_free (string);
	lives_text_buffer_get_start_iter (lives_text_view_get_buffer(LIVES_TEXT_VIEW(rfxbuilder->code_textview)),&start_iter);
	lives_text_buffer_place_cursor (lives_text_view_get_buffer (LIVES_TEXT_VIEW (rfxbuilder->code_textview)), &start_iter);
      }
    }
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (cancelbutton);

  okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_code_ok),
		    user_data);
  
  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    NULL);


  lives_widget_show_all (dialog);


}


void on_rfxbuilder_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (!perform_rfxbuilder_checks (rfxbuilder)) return;
  if (!rfxbuilder_to_script (rfxbuilder)) return;

  lives_general_button_clicked(button,NULL);
  rfxbuilder_destroy (rfxbuilder);
}

void on_rfxbuilder_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  lives_general_button_clicked(button,NULL);
  rfxbuilder_destroy (rfxbuilder);
}



void rfxbuilder_destroy (rfx_build_window_t *rfxbuilder) {
  int i;

  for (i=0;i<rfxbuilder->num_reqs;i++) {
    g_free (rfxbuilder->reqs[i]);
  }
  for (i=0;i<rfxbuilder->num_params;i++) {
    g_free (rfxbuilder->params[i].name);
    g_free (rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list!=NULL)g_list_free (rfxbuilder->params[i].list);
    }
    g_free (rfxbuilder->params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->params);
  }
  for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
    g_free (rfxbuilder->paramw_hints[i]);
  }
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    g_free (rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->triggers);
  }

  g_free (rfxbuilder->pre_code);
  g_free (rfxbuilder->loop_code);
  g_free (rfxbuilder->post_code);

  g_free (rfxbuilder->field_delim);

  if (rfxbuilder->script_name!=NULL) {
    g_free (rfxbuilder->script_name);
  }
  if (rfxbuilder->oname!=NULL) {
    g_free (rfxbuilder->oname);
  }
  g_free(rfxbuilder->rfx_version);

  g_free (rfxbuilder);
}


boolean perform_rfxbuilder_checks (rfx_build_window_t *rfxbuilder) {
  gchar *name=g_strdup (lives_entry_get_text (LIVES_ENTRY (rfxbuilder->name_entry)));

  if (!strlen (name)) {
    do_blocking_error_dialog (_ ("\n\nName must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (get_token_count (name,' ')>1) {
    do_blocking_error_dialog (_ ("\n\nName must not contain spaces.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->menu_text_entry)))) {
    do_blocking_error_dialog (_ ("\n\nMenu text must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->action_desc_entry)))&&
      rfxbuilder->type!=RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_ ("\n\nAction description must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->author_entry)))) {
    do_blocking_error_dialog (_ ("\n\nAuthor must not be blank.\n"));
    g_free (name);
    return FALSE;
  }

  if (rfxbuilder->mode!=RFX_BUILDER_MODE_NEW&&!(rfxbuilder->mode==RFX_BUILDER_MODE_EDIT&&
						!strcmp (rfxbuilder->oname,name))) {
    if (find_rfx_plugin_by_name (name,RFX_STATUS_TEST)>-1||find_rfx_plugin_by_name 
	(name,RFX_STATUS_CUSTOM)>-1||find_rfx_plugin_by_name (name,RFX_STATUS_BUILTIN)>-1) {
      do_blocking_error_dialog (_ ("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
      g_free (name);
      return FALSE;
    }
  }

  if (!strlen(rfxbuilder->loop_code)&&rfxbuilder->type!=RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_("\n\nLoop code should not be blank.\n"));
    g_free (name);
    return FALSE;
  }

  if (rfxbuilder->num_triggers==0&&rfxbuilder->type==RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_ ("\n\nTrigger code should not be blank for a utility.\n"));
    g_free (name);
    return FALSE;
  }

  g_free (name);
  return TRUE;
}

boolean perform_param_checks (rfx_build_window_t *rfxbuilder, int index, int rows) {
  register int i;

  if (!strlen(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->param_name_entry)))) {
    do_blocking_error_dialog (_ ("\n\nParameter name must not be blank.\n"));
    return FALSE;
  }
  for (i=0;i<rows;i++) {
    if (i!=index&&!(strcmp(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->param_name_entry)),
			   rfxbuilder->copy_params[i].name))) {
      do_blocking_error_dialog(_("\n\nDuplicate parameter name detected. Parameter names must be unique in a plugin.\n\n"));
      return FALSE;
    }
  }
  return TRUE;
}


boolean rfxbuilder_to_script (rfx_build_window_t *rfxbuilder) {
  FILE *sfile;
  lives_colRGB24_t rgb;
  gchar **array;

  double stepwrap;

  uint32_t props;

  const gchar *name=lives_entry_get_text (LIVES_ENTRY (rfxbuilder->name_entry));

  gchar *script_file,*script_file_dir;
  gchar *script_name=g_strdup_printf ("%s.%s",name,RFXBUILDER_SCRIPT_SUFFIX);
  gchar *new_name;
  gchar *buf;
  gchar *msg;
  gchar *tmp,*tmp2;

  int retval;

  register int i,j;


  if (rfxbuilder->mode!=RFX_BUILDER_MODE_EDIT) {
    if (!(new_name=prompt_for_script_name (script_name,RFX_STATUS_TEST))) {
      g_free(script_name);
      return FALSE;
    }
    g_free (script_name);
    script_name=new_name;
  }

  mainw->com_failed=FALSE;

  script_file_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);

  if (!g_file_test(script_file_dir,G_FILE_TEST_IS_DIR)) {
    if (g_mkdir_with_parents(script_file_dir,S_IRWXU)==-1) {
      g_free (script_name);
      return FALSE;
    }
  }

  script_file=g_strdup_printf ("%s%s",script_file_dir,script_name);
  g_free (script_file_dir);
  g_free (script_name);
  msg=g_strdup_printf (_ ("Writing script file %s..."),script_file);
  d_print (msg);
  g_free (msg);

  if (!check_file (script_file,TRUE)) {
    g_free (script_file);
    d_print_failed();
    return FALSE;
  }
  
  do {
    retval=0;
    if (!(sfile=fopen(script_file,"w"))) {
      retval=do_write_failed_error_s_with_retry(script_file,g_strerror(errno),LIVES_WINDOW(rfxbuilder->dialog));
      if (retval==LIVES_CANCEL) {
	g_free (msg);
	g_free (script_file);
	d_print_failed();
	return FALSE;
      }
    }
    else {
  
      mainw->write_failed=FALSE;

      lives_fputs("Script file generated from LiVES\n\n",sfile);
      lives_fputs("<define>\n",sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(rfxbuilder->rfx_version,sfile);
      lives_fputs("\n</define>\n\n",sfile);
      lives_fputs("<name>\n",sfile);
      lives_fputs(name,sfile);
      lives_fputs("\n</name>\n\n",sfile);
      lives_fputs("<version>\n",sfile);
      buf=g_strdup_printf ("%d",lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_version)));
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs("\n</version>\n\n",sfile);
      lives_fputs("<author>\n",sfile);
      lives_fputs(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->author_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->url_entry)),sfile);
      lives_fputs("\n</author>\n\n",sfile);
      lives_fputs("<description>\n",sfile);
      lives_fputs(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->menu_text_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(lives_entry_get_text (LIVES_ENTRY (rfxbuilder->action_desc_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      if (rfxbuilder->type==RFX_BUILD_TYPE_UTILITY) {
	buf=g_strdup ("-1");
      }
      else {
	buf=g_strdup_printf ("%d",lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_min_frames)));
      }
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs(rfxbuilder->field_delim,sfile);
      switch (rfxbuilder->type) {
      case RFX_BUILD_TYPE_EFFECT2:
	buf=g_strdup ("2");
	break;
      case RFX_BUILD_TYPE_EFFECT0:
	buf=g_strdup ("0");
	break;
      default:
	buf=g_strdup ("1");
      }
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs("\n</description>\n\n",sfile);
      lives_fputs("<requires>\n",sfile);
      for (i=0;i<rfxbuilder->num_reqs;i++) {
	lives_fputs(rfxbuilder->reqs[i],sfile);
	if (i<rfxbuilder->num_reqs-1)
	  lives_fputs("\n",sfile);
      }
      lives_fputs("\n</requires>\n\n",sfile);
      lives_fputs("<params>\n",sfile);
      for (i=0;i<rfxbuilder->num_params;i++) {
	lives_fputs(rfxbuilder->params[i].name,sfile);
	lives_fputs(rfxbuilder->field_delim,sfile);
	lives_fputs(rfxbuilder->params[i].label,sfile);
	lives_fputs(rfxbuilder->field_delim,sfile);
	switch (rfxbuilder->params[i].type) {
	case LIVES_PARAM_NUM:
	  stepwrap=rfxbuilder->params[i].step_size;
	  if (rfxbuilder->params[i].wrap) stepwrap=-stepwrap;
	  lives_fputs ("num",sfile);
	  buf=g_strdup_printf ("%d",rfxbuilder->params[i].dp);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (!rfxbuilder->params[i].dp) {
	    buf=g_strdup_printf ("%d",get_int_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf ("%d",(int)rfxbuilder->params[i].min);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf ("%d",(int)rfxbuilder->params[i].max);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    if (stepwrap!=1.) {
	      buf=g_strdup_printf ("%d",(int)stepwrap);
	      lives_fputs (buf,sfile);
	      g_free (buf);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	  }
	  else {
	    gchar *pattern=g_strdup_printf ("%%.%df",rfxbuilder->params[i].dp);
	    buf=g_strdup_printf (pattern,get_double_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf (pattern,rfxbuilder->params[i].min);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf (pattern,rfxbuilder->params[i].max);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    if (stepwrap!=1.) {
	      buf=g_strdup_printf (pattern,stepwrap);
	      lives_fputs (buf,sfile);
	      g_free (buf);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	    g_free (pattern);
	  }
	  break;
	case LIVES_PARAM_BOOL:
	  lives_fputs ("bool",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",get_bool_param (rfxbuilder->params[i].def));
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (rfxbuilder->params[i].group!=0) {
	    buf=g_strdup_printf ("%d",rfxbuilder->params[i].group);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	  }
	  break;
	case LIVES_PARAM_COLRGB24:
	  lives_fputs ("colRGB24",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  get_colRGB24_param (rfxbuilder->params[i].def,&rgb);
	  buf=g_strdup_printf ("%d",rgb.red);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",rgb.green);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",rgb.blue);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  break;
	case LIVES_PARAM_STRING:
	  lives_fputs ("string",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  lives_fputs ((tmp=U82L (tmp2=subst ((gchar *)rfxbuilder->params[i].def,"\n","\\n"))),sfile);
	  g_free(tmp);
	  g_free(tmp2);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",(int)rfxbuilder->params[i].max);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  break;
	case LIVES_PARAM_STRING_LIST:
	  lives_fputs ("string_list",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (rfxbuilder->params[i].def!=NULL) {
	    buf=g_strdup_printf ("%d",get_bool_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    for (j=0;j<g_list_length (rfxbuilder->params[i].list);j++) {
	      lives_fputs ((tmp=U82L (tmp2=subst ((gchar *)g_list_nth_data 
						  (rfxbuilder->params[i].list,j),"\n","\\n"))),sfile);
	      g_free(tmp);
	      g_free(tmp2);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	  }
	  break;
	default:
	  break;
	}
	if (i<rfxbuilder->num_params-1)
	  lives_fputs("\n",sfile);
      }
      lives_fputs("\n</params>\n\n",sfile);
      lives_fputs("<param_window>\n",sfile);
      for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
	lives_fputs(rfxbuilder->paramw_hints[i],sfile);
	if (strlen (rfxbuilder->paramw_hints[i])>strlen (rfxbuilder->field_delim)&&
	    strcmp (rfxbuilder->paramw_hints[i]+strlen (rfxbuilder->paramw_hints[i])
		    -strlen (rfxbuilder->field_delim),rfxbuilder->field_delim)) 
	  lives_fputs(rfxbuilder->field_delim,sfile);
	lives_fputs("\n",sfile);
      }
      lives_fputs("</param_window>\n\n",sfile);
      lives_fputs("<properties>\n",sfile);
      if (rfxbuilder->type==RFX_BUILD_TYPE_TOOL) props=rfxbuilder->props|RFX_PROPS_MAY_RESIZE;
      else props=rfxbuilder->props;
  
      if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0&&(rfxbuilder->props&RFX_PROPS_BATCHG)) rfxbuilder->props^=RFX_PROPS_BATCHG;

      buf=g_strdup_printf ("0x%04X",props);
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs("\n</properties>\n\n",sfile);
      lives_fputs("<language_code>\n",sfile);
      array=g_strsplit(lives_combo_get_active_text (LIVES_COMBO (rfxbuilder->langc_combo))," ",-1);
      lives_fputs (array[0],sfile);
      g_strfreev (array);
      lives_fputs("\n</language_code>\n\n",sfile);
      lives_fputs("<pre>\n",sfile);
      lives_fputs (rfxbuilder->pre_code,sfile);
      if (strlen (rfxbuilder->pre_code)&&strcmp (rfxbuilder->pre_code+strlen (rfxbuilder->pre_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</pre>\n\n",sfile);
      lives_fputs("<loop>\n",sfile);
      lives_fputs (rfxbuilder->loop_code,sfile);
      if (strlen (rfxbuilder->loop_code)&&strcmp (rfxbuilder->loop_code+strlen (rfxbuilder->loop_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</loop>\n\n",sfile);
      lives_fputs("<post>\n",sfile);
      lives_fputs (rfxbuilder->post_code,sfile);
      if (strlen (rfxbuilder->post_code)&&strcmp (rfxbuilder->post_code+strlen (rfxbuilder->post_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</post>\n\n",sfile);
      lives_fputs("<onchange>\n",sfile);
      for (i=0;i<rfxbuilder->num_triggers;i++) {
	int j;
	int numtok=get_token_count (rfxbuilder->triggers[i].code,'\n');

	buf=rfxbuilder->triggers[i].when?g_strdup_printf ("%d",rfxbuilder->triggers[i].when-1):g_strdup ("init");
	array=g_strsplit(rfxbuilder->triggers[i].code,"\n",-1);
	for (j=0;j<numtok;j++) {
	  lives_fputs (buf,sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (array[j]!=NULL) lives_fputs(array[j],sfile);
	  if (j<numtok-1)
	    lives_fputs("\n",sfile);
	}
	lives_fputs("\n",sfile);
	g_free (buf);
	g_strfreev (array);
      }
      lives_fputs("</onchange>\n\n",sfile);
      fclose (sfile);

      if (mainw->write_failed) {
	mainw->write_failed=FALSE;
	retval=do_write_failed_error_s_with_retry(script_file,NULL,LIVES_WINDOW(rfxbuilder->dialog));
	if (retval==LIVES_CANCEL) d_print_file_error_failed();
      }

    }
  } while (retval==LIVES_RETRY);

  g_free(script_file);

  if (retval!=LIVES_CANCEL) {
    d_print_done();

    lives_widget_set_sensitive(mainw->promote_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->delete_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->rename_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->edit_test_rfx,TRUE);

    return TRUE;
  }
  return FALSE;
}


boolean script_to_rfxbuilder (rfx_build_window_t *rfxbuilder, const gchar *script_file) {
  GList *list;

  gchar **array;
  gchar *tmp;
  gchar *line;
  gchar *version;

  int num_channels;
  int tnum,found;
  int filled_triggers=0;
  int len;

  register int i,j;

  rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1;
  if (!(list=get_script_section ("define",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <define> section found in script.\n")));
    return FALSE;
  }
  tmp=(gchar *)g_list_nth_data(list,0);
  if (strlen(tmp)<2) {
    g_list_free_strings(list);
    g_list_free(list);
    g_snprintf(mainw->msg,512,"%s",(_("Bad script version.\n")));
    return FALSE;
  }

  version=g_strdup(tmp+1);
  if (make_version_hash(version)>make_version_hash(RFX_VERSION)) {
    g_list_free_strings(list);
    g_list_free(list);
    g_free(version);
    g_snprintf(mainw->msg,512,"%s",(_("Bad script version.\n")));
    return FALSE;
  }
  g_free(version);

  memset(tmp+1,0,1);
  g_free(rfxbuilder->field_delim);
  rfxbuilder->field_delim=g_strdup(tmp);
  g_list_free_strings(list);
  g_list_free(list);

  if (!(list=get_script_section ("name",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <name> section found in script.\n")));
    return FALSE;
  }
  lives_entry_set_text (LIVES_ENTRY (rfxbuilder->name_entry),(gchar *)g_list_nth_data (list,0));
  g_list_free_strings (list);
  g_list_free (list);

  if (!(list=get_script_section ("version",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <version> section found in script.\n")));
    return FALSE;
  }
  lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_version),
			     (double)atoi ((gchar *)g_list_nth_data (list,0)));
  g_list_free_strings (list);
  g_list_free (list);

  if (!(list=get_script_section ("author",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <author> section found in script.\n")));
    return FALSE;
  }
  array=g_strsplit ((gchar *)g_list_nth_data (list,0),rfxbuilder->field_delim,2);
  lives_entry_set_text (LIVES_ENTRY (rfxbuilder->author_entry),array[0]);
  if (get_token_count ((gchar *)g_list_nth_data (list,0),(int)rfxbuilder->field_delim[0])>1) {
    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->url_entry),array[1]);
  }
  g_strfreev (array);
  g_list_free_strings (list);
  g_list_free (list);


  if (!(list=get_script_section ("description",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <description> section found in script.\n")));
    return FALSE;
  }
  if (get_token_count ((gchar *)g_list_nth_data (list,0),(int)rfxbuilder->field_delim[0])<4) {
    g_snprintf(mainw->msg,512,(_("Bad description. (%s)\n")),(gchar *)g_list_nth_data(list,0));
    g_list_free_strings (list);
    g_list_free (list);
    return FALSE;
  }
  array=g_strsplit ((gchar *)g_list_nth_data (list,0),rfxbuilder->field_delim,-1);
  g_list_free_strings (list);
  g_list_free (list);
  lives_entry_set_text (LIVES_ENTRY (rfxbuilder->menu_text_entry),array[0]);
  lives_entry_set_text (LIVES_ENTRY (rfxbuilder->action_desc_entry),array[1]);
  lives_spin_button_set_value (LIVES_SPIN_BUTTON (rfxbuilder->spinbutton_min_frames),(double)atoi (array[2]));
  num_channels=atoi (array[3]);

  if (num_channels==2) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT2;
  }
  if (num_channels==0) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT0;
  }
  if (atoi (array[2])==-1) rfxbuilder->type=RFX_BUILD_TYPE_UTILITY;
  g_strfreev (array);

  rfxbuilder->script_name=g_strdup (script_file);

  if ((list=get_script_section ("requires",script_file,TRUE))) {
    rfxbuilder->num_reqs=g_list_length (list);
    for (i=0;i<g_list_length (list);i++) {
      rfxbuilder->reqs[i]=(gchar *)g_list_nth_data (list,i);
    }
    g_list_free (list);
  }

  rfxbuilder->props=0;

  if ((list=get_script_section ("properties",script_file,TRUE))) {
    if (!strncmp ((gchar *)g_list_nth_data (list,0),"0x",2)||!strncmp ((gchar *)g_list_nth_data (list,0),"0x",2)) {
      rfxbuilder->props=hextodec ((const gchar *)g_list_nth_data (list,0)+2);
    }
    else rfxbuilder->props=atoi ((gchar *)g_list_nth_data (list,0));
    g_list_free_strings (list);
    g_list_free (list);
  }
  if (rfxbuilder->props<0) rfxbuilder->props=0;

  if (rfxbuilder->props&RFX_PROPS_MAY_RESIZE) rfxbuilder->type=RFX_BUILD_TYPE_TOOL;

  if ((list=get_script_section ("params",script_file,TRUE))) {
    gchar *type;

    rfxbuilder->num_params=g_list_length (list);
    rfxbuilder->params=(lives_param_t *)g_malloc (rfxbuilder->num_params*sizeof(lives_param_t));

    for (i=0;i<g_list_length (list);i++) {

      // TODO - error check
      line=(gchar *)g_list_nth_data(list,i);

      len=get_token_count (line,(int)rfxbuilder->field_delim[0]);
      array=g_strsplit(line,rfxbuilder->field_delim,-1);
      g_free (line);
      rfxbuilder->params[i].name=g_strdup (array[0]);
      rfxbuilder->params[i].label=g_strdup (array[1]);
      rfxbuilder->params[i].onchange=FALSE;
      rfxbuilder->params[i].dp=0;
      rfxbuilder->params[i].group=0;

      ////////////////
      rfxbuilder->params[i].desc=NULL;
      rfxbuilder->params[i].use_mnemonic=TRUE;
      rfxbuilder->params[i].step_size=1.;
      rfxbuilder->params[i].interp_func=rfxbuilder->params[i].display_func=NULL;
      rfxbuilder->params[i].hidden=FALSE;
      rfxbuilder->params[i].transition=FALSE;
      rfxbuilder->params[i].wrap=FALSE;
      ////////////////

      type=g_strdup (array[2]);
      if (!strncmp (type,"num",3)) {
	rfxbuilder->params[i].dp=atoi (type+3);
	rfxbuilder->params[i].type=LIVES_PARAM_NUM;
	if (rfxbuilder->params[i].dp) {
	  rfxbuilder->params[i].def=g_malloc (sizint);
	  set_double_param (rfxbuilder->params[i].def,g_strtod (array[3],NULL));
	}
	else {
	  rfxbuilder->params[i].def=g_malloc (sizint);
	  set_int_param (rfxbuilder->params[i].def,atoi (array[3]));
	}
	rfxbuilder->params[i].min=g_strtod (array[4],NULL);
	rfxbuilder->params[i].max=g_strtod (array[5],NULL);
	if (len>6) {
	  rfxbuilder->params[i].step_size=g_strtod(array[6],NULL);
	  if (rfxbuilder->params[i].step_size==0.) rfxbuilder->params[i].step_size=1.;
	  else if (rfxbuilder->params[i].step_size<0.) {
	    rfxbuilder->params[i].step_size=-rfxbuilder->params[i].step_size;
	    rfxbuilder->params[i].wrap=TRUE;
	  }
	}
      }
      else if (!strcmp (type,"colRGB24")) {
	rfxbuilder->params[i].type=LIVES_PARAM_COLRGB24;
	rfxbuilder->params[i].def=g_malloc (3*sizint);
	set_colRGB24_param (rfxbuilder->params[i].def,(short)atoi (array[3]),
			    (short)atoi (array[4]),(short)atoi (array[5]));
      }
      else if (!strcmp (type,"string")) {
	rfxbuilder->params[i].type=LIVES_PARAM_STRING;
	rfxbuilder->params[i].def=subst ((tmp=L2U8(array[3])),"\\n","\n");
	g_free(tmp);
	if (len>4) rfxbuilder->params[i].max=(double)atoi(array[4]);
	else rfxbuilder->params[i].max=1024; // TODO
      }
      else if (!strcmp (type,"string_list")) {
	rfxbuilder->params[i].type=LIVES_PARAM_STRING_LIST;
	rfxbuilder->params[i].def=g_malloc (sizint);
	set_int_param (rfxbuilder->params[i].def,atoi (array[3]));
	if (len>3) {
	  rfxbuilder->params[i].list=array_to_string_list (array,3,len);
	}
	else {
	  rfxbuilder->params[i].list=NULL;
	  set_int_param (rfxbuilder->params[i].def,0);
	}
      }
      else {
	// default is bool
	rfxbuilder->params[i].type=LIVES_PARAM_BOOL;
	rfxbuilder->params[i].def=g_malloc (sizint);
	set_bool_param (rfxbuilder->params[i].def,atoi (array[3]));
	if (len>4) rfxbuilder->params[i].group=atoi(array[4]);
      }
      g_free (type);
      g_strfreev (array);
    }
    g_list_free (list);
  }

  if ((list=get_script_section ("param_window",script_file,TRUE))) {
    rfxbuilder->num_paramw_hints=g_list_length (list);
    for (i=0;i<g_list_length (list);i++) {
      rfxbuilder->paramw_hints[i]=(gchar *)g_list_nth_data (list,i);
    }
    g_list_free (list);
  }

  if ((list=get_script_section ("onchange",script_file,TRUE))) {
    for (i=0;i<g_list_length (list);i++) {
      array=g_strsplit ((gchar *)g_list_nth_data (list,i),rfxbuilder->field_delim,-1);
      if (!strcmp (array[0],"init")) {
	if (!rfxbuilder->has_init_trigger) {
	  rfxbuilder->has_init_trigger=TRUE;
	  rfxbuilder->num_triggers++;
	}
      }
      else if ((tnum=atoi (array[0])+1)<=rfxbuilder->num_params&&tnum>0) {
	if (!rfxbuilder->params[tnum-1].onchange) {
	  rfxbuilder->params[tnum-1].onchange=TRUE;
	  rfxbuilder->num_triggers++;
	}
      }
      else {
	//invalid trigger
	gchar *msg=g_strdup_printf (_("\n\nInvalid trigger (%s)\nfound in script.\n\n"),array[0]);
	do_error_dialog (msg);
	g_free (msg);
      }
      g_strfreev (array);
    }
    //end pass 1
    rfxbuilder->triggers=(rfx_trigger_t *)g_malloc (rfxbuilder->num_triggers*sizeof(rfx_trigger_t));

    for (i=0;i<rfxbuilder->num_triggers;i++) {
      rfxbuilder->triggers[i].when=-1;
      rfxbuilder->triggers[i].code=g_strdup ("");
    }

    filled_triggers=0;
    for (i=0;i<g_list_length (list);i++) {
      array=g_strsplit ((gchar *)g_list_nth_data (list,i),rfxbuilder->field_delim,-1);
      if (!strcmp (array[0],"init")) {
	// find init trigger and concatenate code
	found=filled_triggers;
	for (j=0;j<filled_triggers&&found==filled_triggers;j++) if (rfxbuilder->triggers[j].when==0) found=j;
	if (found==filled_triggers) filled_triggers++;
	if (!strlen (rfxbuilder->triggers[found].code)) {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,array[1],NULL);
	}
	else {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,"\n",array[1],NULL);
	}
	g_free (rfxbuilder->triggers[found].code);
	rfxbuilder->triggers[found].when=0;
	rfxbuilder->triggers[found].code=g_strdup (tmp);
	g_free (tmp);
      }
      else if ((tnum=atoi (array[0])+1)<=rfxbuilder->num_params&&tnum>0) {
	// find tnum trigger and concatenate code
	found=filled_triggers;

	for (j=0;j<filled_triggers&&found==filled_triggers;j++) if (rfxbuilder->triggers[j].when==tnum) found=j;
	if (found==filled_triggers) filled_triggers++;

	if (!strlen (rfxbuilder->triggers[found].code)) {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,array[1],NULL);
	}
	else {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,"\n",array[1],NULL);
	}
	g_free (rfxbuilder->triggers[found].code);
	rfxbuilder->triggers[found].when=tnum;
	rfxbuilder->triggers[found].code=g_strdup (tmp);
	g_free (tmp);
      }
      g_strfreev (array);
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("pre",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->pre_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->pre_code);
      rfxbuilder->pre_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("loop",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->loop_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->loop_code);
      rfxbuilder->loop_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("post",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->post_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->post_code);
      rfxbuilder->post_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  return TRUE;
}




GList *get_script_section (const gchar *section, const gchar *file, boolean strip) {
  FILE *script_file;
  GList *list=NULL;

  size_t linelen;

  gchar buff[65536];

  gchar *line;
  gchar *whole=g_strdup (""),*whole2;

#ifndef IS_MINGW
  gchar *outfile=g_strdup_printf ("%s/rfxsec.%d",g_get_tmp_dir(),capable->mainpid);
  gchar *com=g_strdup_printf ("\"%s\" -get \"%s\" \"%s\" > \"%s\"",RFX_BUILDER,section,file,outfile);
#else
  gchar *outfile=g_strdup_printf ("%s\\rfxsec.%d",g_get_tmp_dir(),capable->mainpid);
  gchar *com=g_strdup_printf ("perl \"%s\\%s\" -get \"%s\" \"%s\" > \"%s\"",prefs->prefix_dir,RFX_BUILDER,
			      section,file,outfile);
#endif

  mainw->com_failed=FALSE;

  lives_system (com,FALSE);
  g_free (com);

  if (mainw->com_failed) return FALSE;

  if ((script_file=fopen (outfile,"r"))) {
    while (fgets(buff,65536,script_file)) {
      if (buff!=NULL) {
	if (strip) line=(g_strstrip(buff));
	else line=buff;
	if ((linelen=strlen (line))) {
	  whole2=g_strconcat (whole,line,NULL);
	  if (whole2!=whole) g_free (whole);
	  whole=whole2;
	  if (linelen<(size_t)65535) {
	    list=g_list_append (list, g_strdup (whole));
	    g_free (whole);
	    whole=g_strdup ("");
	  }
	}
      }
    }
    g_free (whole);
  }
  else {
    g_free (outfile);
    return NULL;
  }
  fclose (script_file);
  unlink (outfile);
  g_free (outfile);
  return list;
}





void on_rebuild_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *com;
  if (!check_builder_programs()) return;

  d_print (_("Rebuilding all RFX scripts...builtin...")); 
  do_threaded_dialog(_("Rebuilding scripts"),FALSE);

  com=g_strdup_printf("%s build_rfx_plugins builtinx \"%s%s%s\" \"%s%s%s\" \"%s/bin\"",prefs->backend_sync,
		      prefs->prefix_dir,
		      PLUGIN_SCRIPTS_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,
		      prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN,prefs->prefix_dir);
  lives_system(com,TRUE);
  g_free(com);
  d_print (_ ("custom...")); 
  com=g_strdup_printf("%s build_rfx_plugins custom",prefs->backend_sync);
  lives_system(com,FALSE);
  g_free(com);
  d_print (_("test...")); 
  com=g_strdup_printf("%s build_rfx_plugins test",prefs->backend_sync);
  lives_system(com,FALSE);
  g_free(com);


  d_print(_("rebuilding dynamic menu entries..."));
  lives_widget_context_update();
  threaded_dialog_spin();
  add_rfx_effects();
  threaded_dialog_spin();
  d_print_done(); 
  threaded_dialog_spin();
  end_threaded_dialog();

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();
}




boolean check_builder_programs (void) {
#ifdef IS_MINGW
  return TRUE;
#endif

  // check our plugin builder routines are executable
  gchar loc[32];
  gchar *msg;

  get_location(RFX_BUILDER,loc,32);
  if (!strlen(loc)) {
    msg=g_strdup_printf(_("\n\nLiVES was unable to find the program %s.\nPlease check this program is in your path and executable.\n"),RFX_BUILDER);
    do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }
  get_location(RFX_BUILDER_MULTI,loc,32);
  if (!strlen(loc)) {
    msg=g_strdup_printf(_("\n\nLiVES was unable to find the program %s.\nPlease check this program is in your path and executable.\n"),RFX_BUILDER_MULTI);
    do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }
  return TRUE;
}



void on_delete_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_rfx_status_t status=(lives_rfx_status_t)GPOINTER_TO_INT (user_data);
  int ret;
  gchar *rfx_script_file,*rfx_script_dir;
  gchar *script_name=prompt_for_script_name (NULL,status);
  gchar *msg;

  if (script_name==NULL) return;  // user cancelled

  if (strlen (script_name)) {
    switch (status) {
    case RFX_STATUS_TEST:
      rfx_script_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);
      rfx_script_file=g_build_filename (rfx_script_dir,script_name,NULL);
      break;
    case RFX_STATUS_CUSTOM:
      rfx_script_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,NULL);
      rfx_script_file=g_build_filename (rfx_script_dir,script_name,NULL);
      break;
    default:
      // we will not delete builtins
      g_free (script_name);
      return;
    }
    g_free (script_name);

    // double check with user
    msg=g_strdup_printf (_ ("\n\nReally delete RFX script\n%s ?\n\n"),rfx_script_file);
    if (!do_warning_dialog (msg)) {
      g_free (msg);
      g_free (rfx_script_file);
      g_free (rfx_script_dir);
      return;
    }
    g_free (msg);

    msg=g_strdup_printf (_ ("Deleting rfx script %s..."),rfx_script_file);
    d_print (msg);
    g_free (msg);
    if (!(ret=unlink (rfx_script_file))) {
      gchar *com;
#ifndef IS_MINGW
      com=g_strdup_printf("/bin/rm -rf \"%s\"",rfx_script_dir);
#else
      com=g_strdup_printf("DEL /q \"%s\"",rfx_script_dir);
      lives_system(com,TRUE);
      g_free(com);
      com=g_strdup_printf("RMDIR \"%s\"",rfx_script_dir);
#endif
      lives_system(com,TRUE);
      g_free(com);
      d_print_done();
      on_rebuild_rfx_activate (NULL,NULL);
    }
    else {
      d_print_failed();
       msg=g_strdup_printf(_ ("\n\nFailed to delete the script\n%s\nError code was %d\n"),rfx_script_file,ret);
      do_error_dialog (msg);
      g_free (msg);
    }
    g_free (rfx_script_file);
    g_free (rfx_script_dir);
  }
}


void on_promote_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *rfx_script_from=NULL;
  gchar *rfx_script_to=NULL;
  gchar *rfx_dir_from=NULL;
  gchar *rfx_dir_to=NULL;
  gchar *script_name=prompt_for_script_name (NULL,RFX_STATUS_TEST);
  gchar *msg;
  int ret=0;
  boolean failed=TRUE;

  if (script_name==NULL) return;  // user cancelled

  if (strlen (script_name)) {
    rfx_dir_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				   PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);

    rfx_script_from=g_build_filename (rfx_dir_from,script_name,NULL);


    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				 PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,NULL);


    rfx_script_to=g_build_filename (rfx_dir_to,script_name,NULL);

    if (g_file_test (rfx_script_to, G_FILE_TEST_EXISTS)) {
      gchar *msg=g_strdup_printf (_ ("\nCustom script file:\n%s\nalready exists.\nPlease delete it first, or rename the test script.\n"),script_name);
      do_blocking_error_dialog (msg);
      g_free (msg);
      g_free (rfx_dir_from);
      g_free (rfx_script_from);
      g_free (rfx_dir_to);
      g_free (rfx_script_to);
      g_free (script_name);
      return;
    }

    msg=g_strdup_printf (_ ("Promoting rfx test plugin %s to custom..."),script_name);
    d_print (msg);
    g_free (script_name);
    g_free (msg);

    g_mkdir_with_parents(rfx_dir_to,S_IRWXU);

    if (!(ret=rename (rfx_script_from,rfx_script_to))) {
      d_print_done();
      on_rebuild_rfx_activate (NULL,NULL);
      failed=FALSE;
    }
  }

  if (failed) {
    rmdir(rfx_dir_to);
    d_print_failed();
    msg=g_strdup_printf(_ ("\n\nFailed to move the plugin script from\n%s to\n%s\nReturn code was %d (%s)\n"),
			rfx_script_from,rfx_script_to,errno,strerror(errno));
    do_error_dialog (msg);
    g_free (msg);
  }
  else rmdir(rfx_dir_from);

  if (rfx_script_from!=NULL) {
    g_free (rfx_dir_from);
    g_free (rfx_script_from);
    g_free (rfx_dir_to);
    g_free (rfx_script_to);
  }
}



void on_export_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_rfx_status_t status=(lives_rfx_status_t)GPOINTER_TO_INT (user_data);

  gchar *script_name=prompt_for_script_name (NULL,status);
  gchar *rfx_script_from,*filename;
  gchar *com,*msg,*tmp,*tmp2;
  
  if (script_name==NULL||!strlen(script_name)) return;  // user cancelled

  filename = choose_file (NULL,script_name,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,_ ("LiVES: - Export Script to..."),NULL);

  if (filename==NULL) return;
  
  rfx_script_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,script_name,NULL);

  msg=g_strdup_printf(_ ("Copying %s to %s..."),rfx_script_from,filename);
  d_print(msg);
  g_free(msg);
#ifndef IS_MINGW
  com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",(tmp=g_filename_from_utf8 (rfx_script_from,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8 (filename,-1,NULL,NULL,NULL)));
#else
  com=g_strdup_printf("cp.exe \"%s\" \"%s\"",(tmp=g_filename_from_utf8 (rfx_script_from,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8 (filename,-1,NULL,NULL,NULL)));
#endif
  if (system(com)) d_print_failed();
  else d_print_done();
  g_free(tmp);
  g_free(tmp2);
  g_free(com);
  g_free(rfx_script_from);
  g_free(filename);
  g_free (script_name);
}


void on_import_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {

  short status=(short)GPOINTER_TO_INT (user_data);
  gchar *rfx_script_to,*rfx_dir_to;
  gchar *com,*msg,*tmp,*tmp2,*tmpx;
  gchar basename[PATH_MAX];

  gchar *filename=choose_file(NULL,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_OPEN,_ ("LiVES: Import Script from..."),NULL);

  if (filename==NULL) return;

  g_snprintf (basename,PATH_MAX,"%s",filename);
  get_basename (basename);

  mainw->com_failed=FALSE;

  switch (status) {
  case RFX_STATUS_TEST :
    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);

    g_mkdir_with_parents((tmp=g_filename_from_utf8(rfx_dir_to,-1,NULL,NULL,NULL)),S_IRWXU);

    g_free(tmp);
    rfx_script_to=g_build_filename(rfx_dir_to,basename,NULL);
    g_free (rfx_dir_to);
    break;
  case RFX_STATUS_CUSTOM :
    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,NULL);

    g_mkdir_with_parents((tmp=g_filename_from_utf8(rfx_dir_to,-1,NULL,NULL,NULL)),S_IRWXU);

    g_free(tmp);
    rfx_script_to=g_build_filename (rfx_dir_to,basename,NULL);
    g_free (rfx_dir_to);
    break;
  default :
    rfx_script_to=g_build_filename (prefs->prefix_dir,PLUGIN_SCRIPTS_DIR,
				    PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,basename,NULL);
    break;
  }

  if (mainw->com_failed) {
    g_free (rfx_script_to);
    g_free (filename);
    return;
  }

  if (g_file_test (rfx_script_to, G_FILE_TEST_EXISTS)) {
    // needs switch...eventually
    do_blocking_error_dialog ((tmpx=g_strdup_printf 
			       (_("\nCustom script file:\n%s\nalready exists.\nPlease delete it first, or rename the import script.\n"),basename)));
    g_free(tmpx);
    g_free (rfx_script_to);
    g_free (filename);
    return;
  }


  msg=g_strdup_printf(_ ("Copying %s to %s..."),filename,rfx_script_to);
  d_print(msg);
  g_free(msg);
#ifndef IS_MINGW
  com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",(tmp=g_filename_from_utf8(filename,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8(rfx_script_to,-1,NULL,NULL,NULL)));
#else
  com=g_strdup_printf("cp.exe \"%s\" \"%s\"",(tmp=g_filename_from_utf8(filename,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8(rfx_script_to,-1,NULL,NULL,NULL)));
#endif
  g_free(tmp);
  g_free(tmp2);
  if (system(com)) d_print_failed();
  else {
    d_print_done();
    on_rebuild_rfx_activate (NULL,NULL);
  }
  g_free(com);
  g_free(rfx_script_to);
  g_free(filename);
}



gchar *prompt_for_script_name(const gchar *sname, lives_rfx_status_t status) {
  // show dialog to get script name of rfx plugin dependant on type
  // set type to RFX_STATUS_ANY to let user pick type as well
  // return value should be g_free'd after use
  // beware: if the user cancels, return value is NULL

  // sname is suggested name, set it to NULL to prompt for a name from
  // status list

  // in copy mode, there are extra entries and the selected script will be copied
  // to test

  // in rename mode a test script will be copied to another test script

  gchar *name=NULL;
  gchar *from_name;
  gchar *from_status;
  gchar *rfx_script_from;
  gchar *rfx_script_to;

  rfx_build_window_t *rfxbuilder;

  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *script_combo=NULL;
  GtkWidget *name_entry=NULL;
  GtkWidget *script_combo_entry=NULL;
  GtkWidget *status_combo=NULL;
  GtkWidget *status_combo_entry=NULL;
  GtkWidget *dialog;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;

  GList *status_list=NULL;

  boolean copy_mode=FALSE;
  boolean rename_mode=FALSE;
  boolean OK;

  if (status==RFX_STATUS_COPY) {
    copy_mode=TRUE;
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]));
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]));
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_TEST]));
  }

  dialog = lives_standard_dialog_new (NULL,FALSE);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (hbox), vbox, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  add_fill_to_box(LIVES_BOX(vbox));
  
  if (copy_mode) {
    lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - Copy RFX Script"));

    status_combo = lives_standard_combo_new (_ ("_From type:    "),TRUE,status_list,LIVES_BOX(hbox),NULL);

    status_combo_entry = lives_combo_get_entry(LIVES_COMBO(status_combo));

    g_list_free_strings (status_list);
    g_list_free (status_list);

    label = lives_standard_label_new (_ ("   Script:    "));
    lives_widget_show (label);
    lives_box_pack_start (LIVES_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
    if (palette->style&STYLE_1) {
      lives_widget_set_fg_color (label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    }
  }
  else {
    if (status==RFX_STATUS_RENAME) {
      lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - Rename test RFX Script"));
      label = lives_standard_label_new (_ ("From script: "));
      rename_mode=TRUE;
      status=RFX_STATUS_TEST;
    }
    else {
      lives_window_set_title (LIVES_WINDOW (dialog), _("LiVES: - RFX Script name"));
      label = lives_standard_label_new (_ ("Script name: "));
    }
    lives_box_pack_start (LIVES_BOX (hbox), label, FALSE, FALSE, 0);
  }
  
  
  if (sname==NULL||copy_mode||rename_mode) {
    script_combo = lives_combo_new ();

    name_entry = script_combo_entry = lives_combo_get_entry(LIVES_COMBO(script_combo));
    lives_entry_set_editable (LIVES_ENTRY(name_entry),FALSE);
    lives_box_pack_start (LIVES_BOX (hbox), script_combo, TRUE, TRUE, widget_opts.packing_width);
  }
  if (sname!=NULL||copy_mode||rename_mode) {
    // name_entry becomes a normal gtk_entry
    name_entry=gtk_entry_new();

    if (copy_mode) {
      g_signal_connect (GTK_OBJECT(status_combo),"changed",G_CALLBACK (on_script_status_changed),
			(gpointer)script_combo);
      label = lives_standard_label_new (_ ("New name: "));
    }
    if (rename_mode) {
      label = lives_standard_label_new (_ ("New script name: "));
    }
    if (copy_mode||rename_mode) {
      hbox = lives_hbox_new (FALSE, 0);
      lives_box_pack_start (LIVES_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
      lives_box_pack_start (LIVES_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
    }
    else {
      lives_entry_set_text (LIVES_ENTRY (name_entry),sname);
    }
    lives_box_pack_start (LIVES_BOX (hbox), name_entry, TRUE, TRUE, widget_opts.packing_width);
  }
  lives_widget_grab_focus (name_entry);
  gtk_entry_set_activates_default (LIVES_ENTRY (name_entry), TRUE);


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  lives_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  copy_script_okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), copy_script_okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (copy_script_okbutton);
  lives_widget_grab_default (copy_script_okbutton); 

  lives_widget_show_all(dialog);

  if (status_combo!=NULL) on_script_status_changed(LIVES_COMBO(status_combo),(gpointer)script_combo);
  else if (script_combo!=NULL) populate_script_combo(LIVES_COMBO(script_combo), status);

  do {
    OK=TRUE;
    if (lives_dialog_run(LIVES_DIALOG (dialog))==GTK_RESPONSE_OK) {
      if (name!=NULL) g_free (name);
      name=g_strdup(lives_entry_get_text (LIVES_ENTRY (name_entry)));
      if (copy_mode) {
	if (find_rfx_plugin_by_name (name,RFX_STATUS_TEST)>-1||
	    find_rfx_plugin_by_name (name,RFX_STATUS_CUSTOM)>-1||
	    find_rfx_plugin_by_name (name,RFX_STATUS_BUILTIN)>-1) {
	  do_blocking_error_dialog (_ ("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
	  OK=FALSE;
	}
	//copy selected script to test
	else {
	  from_name=g_strdup(lives_entry_get_text (LIVES_ENTRY (script_combo_entry)));
	  from_status=g_strdup(lives_entry_get_text (LIVES_ENTRY (status_combo_entry)));
	  if (!strcmp (from_status,mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN])) status=RFX_STATUS_BUILTIN;
	  else {
	    if (!strcmp (from_status,mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM])) status=RFX_STATUS_CUSTOM;
	    else status=RFX_STATUS_TEST;
	  }

	  if ((rfxbuilder=make_rfx_build_window (from_name,status))==NULL) {
	    // invalid name
	    OK=FALSE;
	  }

	  g_free (from_name);
	  g_free (from_status);

	  if (OK) {
	    lives_entry_set_text (LIVES_ENTRY (rfxbuilder->name_entry),name);
	    rfxbuilder->mode=RFX_BUILDER_MODE_COPY;
	    lives_widget_show (rfxbuilder->dialog);
	  }
	}
      }
      if (rename_mode) {
	GList *nmlist=NULL;
	gchar *xname=ensure_extension(name,".script");

	if (name !=NULL&&g_list_find ((nmlist=get_script_list (status)),xname)!=NULL) {
	  do_blocking_error_dialog (_ ("\n\nThere is already a test script with this name.\nScript name must be unique.\n"));
	  OK=FALSE;
	}
	else {
	  int ret;
	  gchar *tmp;

	  from_name=g_strdup(lives_entry_get_text (LIVES_ENTRY (script_combo_entry)));
	  rfx_script_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					    PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,from_name,NULL);
	  rfx_script_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					  PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,xname,NULL);
	  d_print ((tmp=g_strdup_printf (_ ("Renaming RFX test script %s to %s..."),from_name,xname)));
	  g_free(tmp);
	  g_free (from_name);

	  if ((ret=rename (rfx_script_from,rfx_script_to))) {
	    d_print_failed();
	    do_error_dialog ((tmp=g_strdup_printf(_ ("\n\nFailed to move the plugin script from\n%s to\n%s\nReturn code was %d\n"),rfx_script_from,rfx_script_to,ret)));
	    g_free(tmp);
	  }
	  else {
	    d_print_done();
	  }
	  g_free (rfx_script_from);
	  g_free (rfx_script_to);
	}
	if (nmlist!=NULL) {
	  g_list_free_strings(nmlist);
	  g_list_free(nmlist);
	}
	g_free(xname);
      }
    }
  } while (!OK);

  lives_widget_destroy (dialog);
  return name;
}


void populate_script_combo(GtkComboBox *script_combo, lives_rfx_status_t status) {
  GList *list=NULL;
  lives_combo_populate (script_combo, (list=get_script_list (status)));
  if (list!=NULL) {
    lives_combo_set_active_index(script_combo,0);
    lives_widget_set_sensitive(copy_script_okbutton,TRUE);
    g_list_free_strings(list);
    g_list_free(list);
  }
  else {
    lives_combo_set_active_string(script_combo,"");
    lives_widget_set_sensitive(copy_script_okbutton,FALSE);
  }
}



void on_script_status_changed (GtkComboBox *status_combo, gpointer user_data) {
  gchar *text=lives_combo_get_active_text(status_combo);
  LiVESWidget *script_combo=(LiVESWidget *)user_data;

  if (script_combo==NULL||!LIVES_IS_COMBO (script_combo)) return;

  if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]))) {
    populate_script_combo(LIVES_COMBO(script_combo),RFX_STATUS_BUILTIN);
  }
  else {
    if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]))) {
      populate_script_combo(LIVES_COMBO(script_combo),RFX_STATUS_CUSTOM);
    }
    else {
      if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_TEST]))) {
	populate_script_combo(LIVES_COMBO(script_combo),RFX_STATUS_TEST);
      }
    }
  }
  g_free (text);
}


GList *get_script_list (short status) {
  GList *script_list=NULL;

  switch (status) {
  case RFX_STATUS_TEST :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,TRUE,NULL,"script");
    break;
  case RFX_STATUS_CUSTOM :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,TRUE,NULL,"script");
    break;
  case RFX_STATUS_BUILTIN :
  case RFX_STATUS_COPY :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,TRUE,NULL,"script");
    break;
  }
  return script_list;
}




void add_rfx_effects(void) {
  // scan render plugin directories, create a rfx array, and add each to the appropriate menu area
  GList *rfx_builtin_list=NULL;
  GList *rfx_custom_list=NULL;
  GList *rfx_test_list=NULL;


  lives_rfx_t *rfx=NULL;
  lives_rfx_t *rendered_fx;

  gchar txt[64]; // menu text

#if !GTK_CHECK_VERSION(3,10,0)
  GtkWidget *rfx_image;
#endif
  GtkWidget *menuitem;

  int i,plugin_idx,rfx_slot_count=1;

  int rc_child=0;

  int tool_posn=RFX_TOOL_MENU_POSN;
  int rfx_builtin_list_length=0,rfx_custom_list_length=0,rfx_test_list_length=0,rfx_list_length=0;

#ifndef NO_RFX
  boolean allow_nonex;
#endif

  mainw->has_custom_tools=FALSE;
  mainw->has_custom_gens=FALSE;
  mainw->has_custom_utilities=FALSE;

  // exterminate...all...menuentries....
  // TODO - account for case where we only have apply_realtime (i.e add 1 to builtin count)
  if (mainw->num_rendered_effects_builtin) {
    for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom
	   +mainw->num_rendered_effects_test;i++) {
      if (mainw->rendered_fx!=NULL) {
	if (mainw->rendered_fx[i].menuitem!=NULL) {
	  lives_widget_destroy(mainw->rendered_fx[i].menuitem);
	  threaded_dialog_spin();
	}
      }
    }
    
    threaded_dialog_spin();

    if (mainw->rte_separator!=NULL) {
      if (mainw->custom_effects_separator!=NULL) lives_widget_destroy (mainw->custom_effects_separator);
      if (mainw->custom_effects_menu!=NULL) lives_widget_destroy (mainw->custom_effects_menu);
      if (mainw->custom_effects_submenu!=NULL) lives_widget_destroy (mainw->custom_effects_submenu);
      if (mainw->custom_gens_menu!=NULL) lives_widget_destroy (mainw->custom_gens_menu);
      if (mainw->custom_gens_submenu!=NULL) lives_widget_destroy (mainw->custom_gens_submenu);
      if (mainw->gens_menu!=NULL) lives_widget_destroy (mainw->gens_menu);

      if (mainw->custom_utilities_separator!=NULL) lives_widget_destroy (mainw->custom_utilities_separator);
      if (mainw->custom_utilities_menu!=NULL) lives_widget_destroy (mainw->custom_utilities_menu);
      if (mainw->custom_utilities_submenu!=NULL) lives_widget_destroy (mainw->custom_utilities_submenu);
      if (mainw->custom_tools_menu!=NULL) lives_widget_destroy (mainw->custom_tools_menu);
      if (mainw->utilities_menu!=NULL) lives_widget_destroy (mainw->utilities_menu);
      if (mainw->run_test_rfx_menu!=NULL) lives_widget_destroy (mainw->run_test_rfx_menu);
    }

    lives_widget_queue_draw(mainw->effects_menu);
    lives_widget_context_update();
    threaded_dialog_spin();

    if (mainw->rendered_fx!=NULL) rfx_free_all();
  }

  mainw->num_rendered_effects_builtin=mainw->num_rendered_effects_custom=mainw->num_rendered_effects_test=0;


  threaded_dialog_spin();

  make_custom_submenus();

  mainw->run_test_rfx_menu=lives_menu_new();
  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->run_test_rfx_submenu), mainw->run_test_rfx_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->run_test_rfx_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->run_test_rfx_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(mainw->run_test_rfx_menu);

  mainw->custom_effects_menu=lives_menu_new();
  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_effects_submenu), mainw->custom_effects_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->custom_effects_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->custom_effects_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->custom_tools_menu=lives_menu_new();

  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_tools_submenu), mainw->custom_tools_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->custom_tools_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->custom_tools_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

#ifdef IS_MINGW
  //#define NO_RFX
#endif


#ifndef NO_RFX

  // scan rendered effect directories
#ifndef IS_MINGW
  allow_nonex=FALSE;
#else
  allow_nonex=TRUE;
#endif

  rfx_custom_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_CUSTOM,allow_nonex,NULL,NULL);
  rfx_custom_list_length=g_list_length(rfx_custom_list);

  rfx_test_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_TEST,allow_nonex,NULL,NULL);
  rfx_test_list_length=g_list_length(rfx_test_list);
    
  if ((rfx_builtin_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_BUILTIN,allow_nonex,NULL,NULL))==NULL) {
    do_rendered_fx_dialog();
  }
  else {
    rfx_builtin_list_length=g_list_length(rfx_builtin_list);
  }
#endif

  rfx_list_length=rfx_builtin_list_length+rfx_custom_list_length+rfx_test_list_length;

  threaded_dialog_spin();
    
  rendered_fx=(lives_rfx_t *)g_malloc ((rfx_list_length+1)*sizeof(lives_rfx_t));
  
  // use rfx[0] as "Apply realtime fx"
  rendered_fx[0].name=g_strdup("realtime_fx");
  rendered_fx[0].menu_text=g_strdup (_("_Apply Real Time Effects to Selection"));
  rendered_fx[0].action_desc=g_strdup ("Applying Current Real Time Effects to");

  rendered_fx[0].props=0;
  rendered_fx[0].num_params=0;
  rendered_fx[0].num_in_channels=1;
  rendered_fx[0].menuitem=NULL;
  rendered_fx[0].params=NULL;
  rendered_fx[0].extra=NULL;
  rendered_fx[0].status=RFX_STATUS_WEED;
  rendered_fx[0].is_template=FALSE;
  rendered_fx[0].min_frames=1;
  rendered_fx[0].source=NULL;
  rendered_fx[0].source_type=LIVES_RFX_SOURCE_RFX;

  if (rfx_list_length) {
    GList *define=NULL;
    GList *description=NULL;
    GList *props=NULL;
    GList *rfx_list=rfx_builtin_list;

    lives_rfx_status_t status=RFX_STATUS_BUILTIN;

    gchar *type=g_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    gchar *plugin_name;
    gchar *def=NULL;
    gchar *tmp;

    int offset=0;


    for (plugin_idx=0;plugin_idx<rfx_list_length;plugin_idx++) {
      threaded_dialog_spin();
      if (mainw->splash_window==NULL) {
	lives_widget_context_update();
      }
      if (plugin_idx==rfx_builtin_list_length) {
	g_free(type);
	type=g_strdup_printf(PLUGIN_RENDERED_EFFECTS_CUSTOM);
	status=RFX_STATUS_CUSTOM;
	rfx_list=rfx_custom_list;
	offset=rfx_builtin_list_length;
      }
      if (plugin_idx==rfx_builtin_list_length+rfx_custom_list_length) {
	g_free(type);
	type=g_strdup(PLUGIN_RENDERED_EFFECTS_TEST);
	status=RFX_STATUS_TEST;
	rfx_list=rfx_test_list;
	offset+=rfx_custom_list_length;
      }
      
      plugin_name=g_strdup((gchar *)g_list_nth_data(rfx_list,plugin_idx-offset));

      if (mainw->splash_window!=NULL) {
	splash_msg((tmp=g_strdup_printf(_("Loading rendered effect %s..."),plugin_name)),.2);
	g_free(tmp);
      }

#ifdef DEBUG_RENDER_FX
      g_print("Checking plugin %s\n",plugin_name);
#endif

      if ((define=plugin_request_by_line(type,plugin_name,"get_define"))==NULL) {
#ifdef DEBUG_RENDER_FX
      g_print("No get_define in %s\n",plugin_name);
#endif

      continue;
      }
      def=g_strdup((gchar *)g_list_nth_data(define,0));
      g_list_free_strings(define);
      g_list_free(define);

      if (strlen(def)<2) {
#ifdef DEBUG_RENDER_FX
	g_print("Invalid get_define in %s\n",plugin_name);
#endif
	g_free(def);
	continue;
      }
      if (make_version_hash(def+1)>make_version_hash(RFX_VERSION)) {
#ifdef DEBUG_RENDER_FX
	g_print("Invalid version %s instead of %s in %s\n",def+1,RFX_VERSION,plugin_name);
#endif
	g_free(def);
	continue;
      }
      memset(def+1,0,1);
      
      if ((description=plugin_request_common (type,plugin_name,"get_description",def,TRUE))!=NULL&&
	  (props=plugin_request_common (type,plugin_name,"get_capabilities",def,FALSE))!=NULL&&
	  g_list_length(description)>3) {
	rfx=&rendered_fx[rfx_slot_count++];
	rfx->name=g_strdup(plugin_name);
	memcpy(rfx->delim,def,2);
	rfx->menu_text=g_strdup ((gchar *)g_list_nth_data (description,0));
	rfx->action_desc=g_strdup ((gchar *)g_list_nth_data (description,1));
	if (!(rfx->min_frames=atoi ((gchar *)g_list_nth_data (description,2)))) rfx->min_frames=1;
	rfx->num_in_channels=atoi((gchar *)g_list_nth_data (description,3));
	rfx->status=status;
	rfx->props=atoi ((gchar *)g_list_nth_data(props,0));
	rfx->num_params=0;
	rfx->is_template=FALSE;
	rfx->params=NULL;
	rfx->source=NULL;
	rfx->source_type=LIVES_RFX_SOURCE_RFX;
	rfx->extra=NULL;
	rfx->is_template=FALSE;
	if (!check_rfx_for_lives (rfx)) rfx_slot_count--;
      }
      g_free(plugin_name);
      if (props!=NULL) {
	g_list_free_strings (props);
	g_list_free (props);
	props=NULL;
      }
      if (description!=NULL) {
	g_list_free_strings (description);
	g_list_free (description);
	description=NULL;
      }
      g_free(def);
    }

    if (rfx_builtin_list!=NULL) {
      g_list_free_strings (rfx_builtin_list);
      g_list_free (rfx_builtin_list);
    }
    if (rfx_custom_list!=NULL) {
      g_list_free_strings (rfx_custom_list);
      g_list_free (rfx_custom_list);
    }
    if (rfx_test_list!=NULL) {
      g_list_free_strings (rfx_test_list);
      g_list_free (rfx_test_list);
    }
    g_free(type);
  }

  rfx_slot_count--;
    
  threaded_dialog_spin();
  // sort menu text by alpha order (apart from [0])
  sort_rfx_array (rendered_fx,rfx_slot_count);
  g_free (rendered_fx);

  if (mainw->rte_separator==NULL) {
    mainw->rte_separator=lives_menu_add_separator(LIVES_MENU(mainw->effects_menu));
    lives_widget_show (mainw->rte_separator);
  }

  menuitem = lives_menu_item_new_with_mnemonic (mainw->rendered_fx[0].menu_text);
  lives_widget_show (menuitem);
  // prepend before mainw->rte_separator
  gtk_menu_shell_prepend (GTK_MENU_SHELL (mainw->effects_menu), menuitem);
  lives_widget_set_sensitive (menuitem, FALSE);
  lives_widget_set_tooltip_text( menuitem,_("See: VJ - show VJ keys. Set the realtime effects, and then apply them here."));
  
  lives_widget_add_accelerator (menuitem, "activate", mainw->accel_group,
			      LIVES_KEY_e, LIVES_CONTROL_MASK,
			      LIVES_ACCEL_VISIBLE);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_realfx_activate),
		    &mainw->rendered_fx[0]);

  mainw->rendered_fx[0].menuitem=menuitem;
  mainw->rendered_fx[0].num_in_channels=1;
  
  if (mainw->is_ready&&mainw->playing_file==-1&&mainw->current_file>0&&
      ((has_video_filters(TRUE)&&!has_video_filters(FALSE))||
       (cfile->achans>0&&prefs->audio_src==AUDIO_SRC_INT&&has_audio_filters(AF_TYPE_ANY))||
       mainw->agen_key!=0)) {
    
    lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem,TRUE);
  }
  else lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem,FALSE);

  lives_container_add (LIVES_CONTAINER (mainw->effects_menu), mainw->custom_effects_submenu);
  
  mainw->custom_effects_separator=lives_menu_add_separator(LIVES_MENU(mainw->effects_menu));

  threaded_dialog_spin();

  // now we need to add to the effects menu and set a callback
  for (rfx=&mainw->rendered_fx[(plugin_idx=1)];plugin_idx<=rfx_slot_count;rfx=&mainw->rendered_fx[++plugin_idx]) {
    threaded_dialog_spin();
    if (mainw->splash_window==NULL) {
      lives_widget_context_update();
    }
    render_fx_get_params (rfx,rfx->name,rfx->status);
    threaded_dialog_spin();
    rfx->source=NULL;
    rfx->extra=NULL;
    rfx->menuitem=NULL;

    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      mainw->num_rendered_effects_builtin++;
      break;
    case RFX_STATUS_CUSTOM:
      mainw->num_rendered_effects_custom++;
      break;
    case RFX_STATUS_TEST:
      mainw->num_rendered_effects_test++;
      break;
    default:
      break;
    }

    if (!(rfx->props&RFX_PROPS_MAY_RESIZE)&&rfx->min_frames>=0&&rfx->num_in_channels==1) {
      // add resizing effects to tools menu later
      g_snprintf (txt,61,"_%s",_(rfx->menu_text));
      if (rfx->num_params) g_strappend (txt,64,"...");
      menuitem = lives_image_menu_item_new_with_mnemonic(txt);
      lives_widget_show(menuitem);

      switch (rfx->status) {
      case RFX_STATUS_BUILTIN:
	lives_container_add (LIVES_CONTAINER (mainw->effects_menu), menuitem);
	break;
      case RFX_STATUS_CUSTOM:
	lives_container_add (LIVES_CONTAINER (mainw->custom_effects_menu), menuitem);
	rc_child++;
	break;
      case RFX_STATUS_TEST:
	lives_container_add (LIVES_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	break;
      default:
	break;
      }


#if !GTK_CHECK_VERSION(3,10,0)
      rfx_image=NULL;
      if (rfx->props&RFX_PROPS_SLOW) {
	rfx_image = lives_image_new_from_stock (LIVES_STOCK_NO, LIVES_ICON_SIZE_MENU);
      }
      else {
	rfx_image = lives_image_new_from_stock (LIVES_STOCK_YES, LIVES_ICON_SIZE_MENU);
      }
      lives_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), rfx_image);
      
      lives_widget_show(rfx_image);
#endif

      if (rfx->params==NULL) {
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (on_render_fx_activate),
			  (gpointer)rfx);
      }
      else {
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (on_render_fx_pre_activate),
			  (gpointer)rfx);
      }
      if (rfx->min_frames>=0) lives_widget_set_sensitive(menuitem,FALSE);
      rfx->menuitem=menuitem;
    }
  }

  threaded_dialog_spin();

  // custom effects
  if (rc_child>0) {
    lives_widget_show (mainw->custom_effects_separator);
    lives_widget_show (mainw->custom_effects_menu);
    lives_widget_show (mainw->custom_effects_submenu);
  }
  else {
    lives_widget_hide (mainw->custom_effects_separator);
    lives_widget_hide (mainw->custom_effects_menu);
    lives_widget_hide (mainw->custom_effects_submenu);
  }

  mainw->utilities_menu=lives_menu_new();
  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->utilities_submenu), mainw->utilities_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->utilities_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->utilities_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->gens_menu=lives_menu_new();
  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->gens_submenu), mainw->gens_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->gens_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->gens_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }


  mainw->custom_gens_menu=lives_menu_new();
  lives_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_gens_submenu), mainw->custom_gens_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->custom_gens_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->custom_gens_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_container_add (LIVES_CONTAINER (mainw->gens_menu), mainw->custom_gens_submenu);

  mainw->custom_utilities_menu=lives_menu_new();

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->custom_utilities_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->custom_utilities_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->custom_utilities_separator=lives_menu_add_separator(LIVES_MENU(mainw->custom_utilities_menu));

  lives_container_add (LIVES_CONTAINER (mainw->custom_tools_menu), mainw->custom_utilities_submenu);

  threaded_dialog_spin();

  mainw->resize_menuitem=NULL;

  if (rfx_slot_count) {
    for (rfx=&mainw->rendered_fx[(plugin_idx=1)];plugin_idx<=rfx_slot_count;rfx=&mainw->rendered_fx[++plugin_idx]) {
      threaded_dialog_spin();
      if (mainw->splash_window==NULL) {
	lives_widget_context_update();
      }
      if ((rfx->props&RFX_PROPS_MAY_RESIZE&&rfx->num_in_channels==1)||rfx->min_frames<0) {
	// add resizing effects to tools menu

	g_snprintf (txt,61,"_%s",_(rfx->menu_text));
	if (rfx->num_params) g_strappend (txt,64,"...");
	menuitem = lives_menu_item_new_with_mnemonic(txt);
	lives_widget_show(menuitem);

	if (!strcmp(rfx->name,"resize")) {
	  mainw->resize_menuitem=menuitem;
	}

	switch (rfx->status) {
	case RFX_STATUS_BUILTIN:
	  if (rfx->min_frames>=0) {
	    gtk_menu_shell_insert (GTK_MENU_SHELL (mainw->tools_menu), menuitem,tool_posn++);
	  }
	  else {
	    lives_container_add (LIVES_CONTAINER (mainw->utilities_menu), menuitem);
	    lives_widget_show (mainw->utilities_menu);
	  }
	  break;
	case RFX_STATUS_CUSTOM:
	  if (rfx->min_frames>=0) {
	    lives_container_add (LIVES_CONTAINER (mainw->custom_tools_menu), menuitem);
	    mainw->has_custom_tools=TRUE;
	  }
	  else {
	    lives_container_add (LIVES_CONTAINER (mainw->custom_utilities_menu), menuitem);
	    mainw->has_custom_utilities=TRUE;
	  }
	  break;
	case RFX_STATUS_TEST:
	  lives_container_add (LIVES_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	  break;
	default:
	  break;
	}

	if (menuitem!=mainw->resize_menuitem) {
	  if (rfx->params==NULL) {
	    g_signal_connect (GTK_OBJECT (menuitem), "activate",
			      G_CALLBACK (on_render_fx_activate),
			      (gpointer)rfx);
	  }
	  else {
	    g_signal_connect (GTK_OBJECT (menuitem), "activate",
			      G_CALLBACK (on_render_fx_pre_activate),
			      (gpointer)rfx);
	  }
	}
	else {
	  mainw->fx_candidates[FX_CANDIDATE_RESIZER].func=g_signal_connect (GTK_OBJECT (menuitem), "activate",
									    G_CALLBACK (on_render_fx_pre_activate),
									    (gpointer)rfx);
	}

	if (rfx->min_frames>=0) lives_widget_set_sensitive(menuitem,FALSE);
	rfx->menuitem=menuitem;
      }

      else if (rfx->num_in_channels==0) {
	// non-realtime generator

	g_snprintf (txt,61,"_%s",_(rfx->menu_text));
	if (rfx->num_params) g_strappend (txt,64,"...");
	menuitem = lives_menu_item_new_with_mnemonic(txt);
	lives_widget_show(menuitem);

	switch (rfx->status) {
	case RFX_STATUS_BUILTIN:
	  lives_container_add (LIVES_CONTAINER (mainw->gens_menu), menuitem);
	  lives_widget_show (mainw->gens_menu);
	  break;
	case RFX_STATUS_CUSTOM:
	  lives_container_add (LIVES_CONTAINER (mainw->custom_gens_menu), menuitem);
	  mainw->has_custom_gens=TRUE;
	  break;
	case RFX_STATUS_TEST:
	  lives_container_add (LIVES_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	  break;
	default:
	  break;
	}

	if (rfx->params==NULL) {
	  g_signal_connect (GTK_OBJECT (menuitem), "activate",
			    G_CALLBACK (on_render_fx_activate),
			    (gpointer)rfx);
	}
	else {
	  g_signal_connect (GTK_OBJECT (menuitem), "activate",
			    G_CALLBACK (on_render_fx_pre_activate),
			    (gpointer)rfx);
	}
      }
    }
  }


  threaded_dialog_spin();

  if (mainw->has_custom_tools||mainw->has_custom_utilities) {
    lives_widget_show(mainw->custom_tools_separator);
    lives_widget_show(mainw->custom_tools_menu);
    lives_widget_show(mainw->custom_tools_submenu);
  }
  else {
    lives_widget_hide(mainw->custom_tools_separator);
    lives_widget_hide(mainw->custom_tools_menu);
    lives_widget_hide(mainw->custom_tools_submenu);
  }
  if (mainw->has_custom_utilities) {
    if (mainw->has_custom_tools) {
      lives_widget_show(mainw->custom_utilities_separator);
    }
    else {
      lives_widget_hide(mainw->custom_utilities_separator);
    }
    lives_widget_show(mainw->custom_utilities_menu);
    lives_widget_show(mainw->custom_utilities_submenu);
  }
  else {
    lives_widget_hide(mainw->custom_utilities_separator);
    lives_widget_hide(mainw->custom_utilities_menu);
    lives_widget_hide(mainw->custom_utilities_submenu);
  }
  if (mainw->has_custom_gens) {
    lives_widget_show(mainw->custom_gens_menu);
    lives_widget_show(mainw->custom_gens_submenu);
  }
  else {
    lives_widget_hide(mainw->custom_gens_menu);
    lives_widget_hide(mainw->custom_gens_submenu);
  }

  if (mainw->is_ready) {
    if (mainw->num_rendered_effects_custom>0) {
      lives_widget_set_sensitive(mainw->delete_custom_rfx,TRUE);
      lives_widget_set_sensitive(mainw->export_custom_rfx,TRUE);
    }
    else {
      lives_widget_set_sensitive(mainw->delete_custom_rfx,FALSE);
      lives_widget_set_sensitive(mainw->export_custom_rfx,FALSE);
    }
    
    if (mainw->num_rendered_effects_test>0) {
      lives_widget_set_sensitive(mainw->run_test_rfx_submenu,TRUE);
      lives_widget_set_sensitive(mainw->promote_test_rfx,TRUE);
      lives_widget_set_sensitive(mainw->delete_test_rfx,TRUE);
      lives_widget_set_sensitive(mainw->rename_test_rfx,TRUE);
      lives_widget_set_sensitive(mainw->edit_test_rfx,TRUE);
    }
    else {
      lives_widget_set_sensitive(mainw->run_test_rfx_submenu,FALSE);
      lives_widget_set_sensitive(mainw->promote_test_rfx,FALSE);
      lives_widget_set_sensitive(mainw->delete_test_rfx,FALSE);
      lives_widget_set_sensitive(mainw->rename_test_rfx,FALSE);
      lives_widget_set_sensitive(mainw->edit_test_rfx,FALSE);
    }
  }

  if (mainw->current_file>0&&mainw->playing_file==-1) sensitize();

}

