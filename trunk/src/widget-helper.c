// widget-helper.c
// LiVES
// (c) G. Finch 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details



// The idea here is to replace toolkit specific functions with generic ones

// TODO - replace as much code in the other files with these functions as possible

// TODO - add for other toolkits, e.g. qt


#include "main.h"

// basic functions


LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
  return GTK_DIALOG(dialog)->vbox;
#endif

#endif
}











// compound functions




LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, 
					     const char *tooltip) {
  LiVESWidget *checkbutton;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox;
  LiVESWidget *label;
  LiVESWidget *hbox;

  checkbutton = gtk_check_button_new ();
  if (tooltip!=NULL) gtk_widget_set_tooltip_text(checkbutton, tooltip);
  eventbox=gtk_event_box_new();
  gtk_tooltips_copy(eventbox,checkbutton);
  if (use_mnemonic) {
    label=gtk_label_new_with_mnemonic (labeltext);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
  }
  else label=gtk_label_new (labeltext);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    checkbutton);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
  }
  
  gtk_box_set_homogeneous(GTK_BOX(hbox),FALSE);
  
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
#endif

  return checkbutton;
}







LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltips) {
  LiVESWidget *radiobutton;

  // pack a themed check button into box



#ifdef GUI_GTK
  LiVESWidget *eventbox;
  LiVESWidget *label;
  LiVESWidget *hbox;

  radiobutton = gtk_radio_button_new (rbgroup);

  if (tooltips!=NULL) gtk_widget_set_tooltip_text(radiobutton, tooltips);

  GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
  }

  gtk_box_set_homogeneous(GTK_BOX(hbox),FALSE);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);
      
  if (use_mnemonic) {
    label=gtk_label_new_with_mnemonic (labeltext);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);
  }
  else label=gtk_label_new (labeltext);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  eventbox=gtk_event_box_new();
  gtk_tooltips_copy(eventbox,radiobutton);
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
 
#endif

  return radiobutton;
}












LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip) {
  LiVESWidget *spinbutton;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox;
  LiVESWidget *label;
  LiVESWidget *hbox;
  LiVESObject *adj;

  char *txt;
  size_t maxlen;

  adj = gtk_adjustment_new (val, min, max, step, page, 0.);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, dp);
  if (tooltip!=NULL) gtk_widget_set_tooltip_text(spinbutton, tooltip);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton),TRUE);
  txt=g_strdup_printf ("%d",(int)max);
  maxlen=strlen (txt);
  g_free (txt);
  txt=g_strdup_printf ("%d",(int)min);
  if (strlen (txt)>maxlen) maxlen=strlen (txt);
  g_free (txt);

  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton),maxlen+dp<4?4:maxlen+dp+1);
  GTK_WIDGET_SET_FLAGS (spinbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton)->entry)), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),GTK_UPDATE_ALWAYS);
  if (use_mnemonic) {
    label=gtk_label_new_with_mnemonic (labeltext);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),spinbutton);
  }
  else label=gtk_label_new (labeltext);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  eventbox=gtk_event_box_new();
  gtk_tooltips_copy(eventbox,spinbutton);
  gtk_container_add(GTK_CONTAINER(eventbox),label);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);

  else {
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
  }

  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 10);

#endif

  return spinbutton;
}






LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip) {
  LiVESWidget *combo;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox;
  LiVESWidget *label;
  LiVESWidget *hbox;

  combo = gtk_combo_new ();

  if (tooltip!=NULL) gtk_widget_set_tooltip_text(combo, tooltip);
  
  if (use_mnemonic) {
    label = gtk_label_new_with_mnemonic (labeltext);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_COMBO(combo)->entry);
  }
  else label = gtk_label_new (labeltext);

  eventbox=gtk_event_box_new();
  gtk_tooltips_copy(eventbox,combo);
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);

  else {
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
  }

  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, TRUE, 10);
  gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (combo))->entry),FALSE);
  gtk_entry_set_activates_default(GTK_ENTRY((GTK_COMBO(combo))->entry),TRUE);

  combo_set_popdown_strings (GTK_COMBO (combo), list);
#endif

  return combo;
}
